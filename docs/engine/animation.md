# Animation — Skeletons, Clips, glTF Rigs, GPU Skinning

**Parent document:** [Engine REQUIREMENTS](REQUIREMENTS.md) §5
**Packages:** `src/engine/animation/` (`eng::animation`), `src/engine/gltf/` (`eng::gltf`), and the skinned half of `src/engine/render-mesh/` (`eng`)
**Governed by:** [ADR-003's 2026-09-10 amendment](../decisions/ADR-003-hybrid-iso-render-model.md#amendment-2026-09-10-skinned-meshes-for-a-handful-of-characters), [ADR-002](../decisions/ADR-002-fixed-timestep-determinism.md)
**Status:** Built and tested. The editor imports rigged `.gltf` and `.glb` models, places them, and plays a clip on each ([Editor §1](../editor/REQUIREMENTS.md#current-state)). Nothing in the game uses them yet.

Rigged models posed by animation clips and drawn skinned on the GPU, for the handful of characters ADR-003 admits: players, bosses, set pieces. The horde stays sprites.

---

## 1. The Shape

```
 .gltf / .glb ──loadGltfModel──► SkinnedModel ──► mesh ──SkinnedMeshRenderer::upload──► MeshGpuId
                                      │
                                      └────► rig ──► RigPose::evaluate(clip, seconds)
                                                          │
                                  samplePose → computeJointWorlds → computeSkinMatrices
                                                          │
                                        span<const Mat4> skin ──► SkinnedMeshInstance
                                                                        │
                                             SkinnedMeshRenderer::draw — one call, one SkinPalette each
```

| Piece | Header | What it owns |
|---|---|---|
| `Skeleton` | `engine/animation/skeleton.h` | Joint parents, rest poses, and names as parallel arrays, **parents before children** |
| `JointPose` | `engine/animation/joint-pose.h` | One joint's translation, rotation (a quaternion), and scale |
| `Skin` | `engine/animation/skin.h` | Which skeleton joints the mesh's vertices name, their inverse bind matrices, and a root transform |
| `AnimationChannel`, `AnimationClip` | `engine/animation/animation-*.h` | Keyframes: one pose part of one joint per channel; a named set of channels per clip |
| `Rig` | `engine/animation/rig.h` | Skeleton, skin, and clips together |
| `samplePose` and friends | `engine/animation/pose-sampling.h` | The three steps from clip and time to skin matrices, each testable alone |
| `RigPose` | `engine/animation/rig-pose.h` | The storage for posing one rig, kept between frames so posing does not allocate |
| `SkinnedModel`, `loadGltfModel` | `engine/gltf/*.h` | glTF 2.0 in, mesh plus rig out, or a `GltfLoadError` that says what to fix |
| `orientSkinnedYUpToZUp` | `engine/gltf/skinned-model-orientation.h` | glTF's Y-up turned into the engine's Z-up, bones included |
| `SkinnedMeshVertex`, `SkinnedMeshData` | `engine/render-mesh/skinned-mesh-*.h` | A static vertex plus four joint bytes and four weights; 52 bytes |
| `SkinPalette` | `engine/render-mesh/skin-palette.h` | The skin matrices as the vertex stage reads them: three rows each, 80 joints, 3,840 bytes |
| `SkinnedMeshRenderer` | `engine/render-mesh/skinned-mesh-renderer.h` | Upload and draw, inside the static mesh pass and against its depth |
| `poseSkinnedMesh` | `engine/render-mesh/skinned-mesh-posing.h` | The vertex shader's arithmetic on the CPU, for thumbnails and tests |

`animation` depends on `math` alone, so every piece of posing is testable headless with no mesh and no GPU. `render-mesh` does not depend on `animation`: the renderer takes a span of matrices and has no idea where they came from. `gltf` is the one package that knows both.

---

## 2. Posing

`RigPose::evaluate(rig, clip, seconds)` runs three pure functions over its own storage:

1. **`samplePose`** copies the rest pose, then writes each of the clip's channels over the part it drives. A joint no channel touches keeps its rest pose, so a clip that animates the arms leaves the legs standing. Before a channel's first key it holds that key; after the last, the last.
2. **`computeJointWorlds`** composes each joint's local pose with its parent's world matrix in one forward pass — correct only because a `Skeleton` orders every parent before its children. `skeletonIsOrdered` checks it; the glTF loader establishes it.
3. **`computeSkinMatrices`** gives each skin joint `root × world × inverse_bind`. In the bind pose every one of them is the identity, which is the test that a rig loaded correctly.

Interpolation is glTF's: `STEP`, `LINEAR` (rotations by slerp along the shorter arc), and `CUBICSPLINE` (Hermite, with the per-second tangents scaled by each segment's length). `loopClipTime` wraps elapsed time into a clip for looping playback. A clip index past the rig's clips — `RIG_REST_POSE` — poses the rest pose.

**None of this is simulation.** Clip time may come from the render frame's delta, and slerp may call `acos`, because the tick never reads a joint. Gameplay that needs something from an animation takes it from simulation data instead. See the ADR-003 amendment before changing that.

---

## 3. Drawing

`SkinnedMeshRenderer` is `MeshRenderer`'s twin, with two differences: its vertices carry joints and weights, and each instance sends a `SkinPalette` as vertex stage bytes at **slot 2**, beside the matrices at slot 1. The fragment stage is the static mesh's own shader, so a skinned character is lit, banded, and outlined exactly as the props around it are. Both renderers share the fragment light block (`mesh-fragment-lights.h`), the untextured stand-in (`mesh-stand-in-texture.h`), and the slot numbers (`src/mesh-draw-bindings.h`).

The vertex stage does linear blend skinning: the weighted sum of four joints' rows, dotted with the position and normal. `poseSkinnedMesh` is the same arithmetic on the CPU.

It draws inside the pass `MeshRenderer::depthTarget` belongs to, after the static meshes, so the two occlude each other and the outline pass lines both. The caller keeps each instance's matrices alive until the draw is recorded, which is what `RigPose` is for.

| Backend | Skinned pipeline | Where |
|---|---|---|
| Metal | `skinned_vs_main` in the same MSL library as `mesh_fs_main` | `metal-device-impl.mm` |
| DX12 | `skinned_vs_main` in `MESH_HLSL_SOURCE`; the palette is the root signature's vertex CBV at `b2` | `dx12-builtin-pipelines.cpp`, `dx12-root-signature.*` |
| OpenGL | `SKINNED_VERTEX_SHADER_GLSL` with `MESH_FRAGMENT_SHADER_GLSL`; the palette is `u_skin_rows[240]`, carried by `GlCmdSetVertexStageBlock` since it is too big for the inline command | `opengl-device.cpp` |
| Vulkan, stub | None — `tryCreateSkinnedMeshPipeline` returns false and the renderer stays inert | |

Three numbers are restated in every shader because none of them can include a C++ header: the 52-byte vertex (`skinned-mesh-vertex.h` asserts its offsets), `MESH_MAX_SKIN_JOINTS = 80`, and slot 2. They move in all places or none.

**Limits.** 80 joints per skin: Metal takes at most 4 KB of inline bytes per slot, and 80 joints of three rows is 3,840. Four influences per vertex. One draw per instance, no instancing — the budget is 16 skinned instances per frame (Engine §7), because ADR-003 admits a handful and a crowd should be sprites.

---

## 4. Loading glTF

`loadGltfModel` reads `.gltf`, with buffers beside it or in `data:` URIs, and self-contained `.glb`. It tells the two apart by their first four bytes, not their names. It reads:

- **The first node with both a mesh and a skin.** Every triangle primitive of that mesh is merged into one mesh, drawn with the first primitive's base colour map when that is an image file beside the model.
- **The skeleton: every skin joint and every node above one.** glTF computes a joint's transform from the scene root down, so an exporter's armature node above the root bone — often a scale of 0.01 and a turn — moves every bone even though no vertex hangs from it. Nodes may be listed in any order; the skeleton is reordered depth-first. A node's `matrix`, where it gives one, is taken apart into translation, rotation, and scale.
- **Every animation's translation, rotation, and scale channels** on skeleton nodes. Channels on other nodes, and morph-target weights, are dropped. An unnamed animation is named for its position.

Accessors of any component type are read, with `normalized` integers mapped as glTF defines, and every read is bounds-checked against the buffer it names. Weights are rescaled to sum to one, since exporters round them. Normals a primitive lacks are computed, smooth.

What is refused, each with its own `GltfLoadError`: glTF 1.0, required extensions other than `KHR_mesh_quantization` (Draco compression, notably), sparse accessors, a file with no skinned mesh (a static model is an OBJ's job today), a skin over 80 joints, a vertex weighted to a joint its skin lacks, and a node hierarchy that loops or gives a node two parents. `gltfLoadErrorMessage` turns each into a sentence the editor puts in its status bar.

Not read yet: images embedded in a buffer or a data URI (the model draws untextured), morph targets, a second skinned mesh in one file, and static glTF.

**Orientation.** glTF is Y-up; the engine is Z-up. Turning only the vertices would not be enough, because every clip still moves the joints in the old space and the first frame would swing the mesh back onto its side. `orientSkinnedYUpToZUp` puts the turn `C` in three places — the vertices, the skin's root, and `C⁻¹` under each inverse bind matrix — so each skin matrix becomes `C · M · C⁻¹` and clips are untouched. `test_skinned_model_orientation.cpp` checks that a turned model plays its clip exactly turned.

---

## 5. Testing

Everything above the backends runs headless:

- `animation` tests each interpolation mode, shortest-arc slerp, the parent-first world pass, and that a bind pose gives identity skin matrices.
- `gltf` tests build documents in memory (`test/support/test-gltf.h`), as `.gltf` with an inline buffer or as `.glb`. A two-joint arm is loaded, posed by its clip, and measured with `poseSkinnedMesh`, alongside every refusal above.
- `render-mesh` tests the palette packing, CPU posing, and what `SkinnedMeshRenderer` records, against a fake device.

The MSL was compiled and its pipeline state built against the real vertex layout with Metal's runtime compiler, and the editor has drawn skinned models under Metal API and GPU validation. The DX12 backend compiles through `windows-cross`, and OpenGL through the `opengl` preset. Neither has run a skinned draw, just as neither has been run for static meshes.
