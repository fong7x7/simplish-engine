#!/usr/bin/env python3
"""Generate a textured 3D model from one image, locally, with Hunyuan3D 2.1.

    python3 tools/image-to-mesh.py --setup                  # install, once
    python3 tools/image-to-mesh.py photo.png -o ~/projects/level/assets/props
    python3 tools/image-to-mesh.py --setup photo.png        # both in one go

The result is what the editor's asset scan already reads: `<name>.obj`,
`<name>.mtl`, and `<name>.png` for the base colour, Y-up like any exported OBJ.
The editor turns it into the Z-up world, scales it to one tile and stands it
on the ground when it is placed (docs/editor/REQUIREMENTS.md, "A dropped model
is scaled to one tile"), so nothing here normalises position or size. Beside
them go `<name>.source.<ext>`, the image it came from, and
`<name>.generation.json`, the settings that made it: a generated model is not
reproducible across machines or library versions, so the files are the
source of truth and the record says where they came from.

The model runs through a community port of Tencent's Hunyuan3D 2.1 to Apple
Silicon (VladimirTalyzin/hunyuan3d-2.1-mac-rocm), pinned to one commit below.
`--setup` fetches that port and runs its own installer, which creates a
Python 3.10-3.12 virtualenv (installing python@3.11 through Homebrew when
none is on PATH), builds one native extension, and downloads about 30 GB of
weights. Everything lands under the tool home, not in this checkout, so every
worktree shares one install. Generation re-runs this file inside that
virtualenv; the copy you invoke needs only the standard library.

A run takes minutes, not seconds: on an M4 Max with the defaults, about three
and a half for the shape and one more for the texture, plus a first-run
model load. `--no-texture` stops after the shape and writes an untextured
model. The model faces +Z, glTF's front, which the editor's Y-up turn puts
toward the camera.

Hunyuan3D 2.1 is under the Tencent Hunyuan Community License, which excludes
use in the EU, the UK and South Korea. Read it before shipping what it makes.

Environment:
    SIMPLISH_IMAGE_TO_MESH_HOME   where --setup installs (default
                                  ~/.cache/simplish/image-to-mesh)
"""

import argparse
import json
import os
import platform
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

FORK_URL = "https://github.com/VladimirTalyzin/hunyuan3d-2.1-mac-rocm.git"
FORK_COMMIT = "6f4b63bd6419406f40291bca23a7c849bcaae2a3"
FORK_DIR_NAME = "hunyuan3d-2.1-mac-rocm"
HOME_ENV = "SIMPLISH_IMAGE_TO_MESH_HOME"
DEFAULT_HOME = Path.home() / ".cache" / "simplish" / "image-to-mesh"

# Set on the child this file starts inside the virtualenv, so that copy runs
# the generator instead of starting another child.
WORKER_ENV = "SIMPLISH_IMAGE_TO_MESH_WORKER"

# Weights are ~30 GB and pip's build of torch and friends is several more.
SETUP_FREE_GIB = 40

# Background removal model rembg fetches on first use. --setup fetches it so
# a generation never waits on the network halfway through.
REMBG_MODEL = "u2net"

OCTREE_CHOICES = (128, 192, 256, 320, 384)

LOG_PREFIX = "[image-to-mesh]"


def say(message):
    """One line of this tool's own progress, set apart from the model's."""
    print(f"{LOG_PREFIX} {message}", flush=True)


def fail(message):
    """Stop with a message and a non-zero exit."""
    print(f"{LOG_PREFIX} error: {message}", file=sys.stderr, flush=True)
    sys.exit(1)


def parse_args(argv):
    """The command line, for both the launcher and the worker."""
    parser = argparse.ArgumentParser(
        description="Generate a textured 3D model from an image, locally.",
        epilog="Setup downloads ~30 GB and takes a while; do it once.",
    )
    parser.add_argument("image", nargs="?", type=Path,
                        help="the picture to model; one object, ideally on a "
                             "plain or transparent background")
    parser.add_argument("--setup", action="store_true",
                        help="install the model and its dependencies first")
    parser.add_argument("-o", "--out", type=Path, default=Path("."),
                        help="directory to write the model into (default: .)")
    parser.add_argument("--name",
                        help="file and asset name (default: the image's name, "
                             "snake_cased)")
    parser.add_argument("--faces", type=int, default=10000,
                        help="triangle budget the mesh is reduced to before "
                             "texturing (default: 10000)")
    parser.add_argument("--no-texture", action="store_true",
                        help="stop after the shape; much faster, untextured")
    parser.add_argument("--keep-background", action="store_true",
                        help="do not remove the background (it is kept "
                             "anyway when the image already has transparency)")
    parser.add_argument("--steps", type=int, default=30,
                        help="shape diffusion steps (default: 30)")
    parser.add_argument("--guidance", type=float, default=5.0,
                        help="shape guidance scale (default: 5.0)")
    parser.add_argument("--octree", type=int, default=256,
                        choices=OCTREE_CHOICES,
                        help="surface extraction resolution (default: 256)")
    parser.add_argument("--seed", type=int, default=42,
                        help="random seed (default: 42)")
    parser.add_argument("--home", type=Path,
                        default=Path(os.environ.get(HOME_ENV) or DEFAULT_HOME),
                        help=f"install location (default: ${HOME_ENV} or "
                             f"{DEFAULT_HOME})")
    args = parser.parse_args(argv)
    if args.image is None and not args.setup:
        parser.error("give an image to model, or --setup to install")
    if args.faces < 100:
        parser.error("--faces below 100 leaves nothing recognisable")
    return args


def fork_dir(args):
    """Where the Hunyuan3D port is checked out."""
    return args.home.expanduser().resolve() / FORK_DIR_NAME


def venv_python(fork):
    """The interpreter the port's installer creates."""
    return fork / "venv" / "bin" / "python"


# ---------------------------------------------------------------------------
# Setup
# ---------------------------------------------------------------------------

def run(command, cwd):
    """Run a command in the foreground, stopping on failure."""
    result = subprocess.run(command, cwd=cwd)
    if result.returncode != 0:
        fail(f"`{' '.join(map(str, command))}` exited {result.returncode}")


def check_host(home):
    """Refuse early on a machine the port's installer would refuse later."""
    if sys.platform != "darwin" or platform.machine() != "arm64":
        fail("setup supports macOS on Apple Silicon only; the port has its "
             "own ROCm installers for Linux and Windows under scripts/")
    for tool in ("git", "brew"):
        if shutil.which(tool) is None:
            fail(f"{tool} is not on PATH (brew: https://brew.sh)")
    home.mkdir(parents=True, exist_ok=True)
    free_gib = shutil.disk_usage(home).free / 1024 ** 3
    if free_gib < SETUP_FREE_GIB:
        fail(f"{home} has {free_gib:.0f} GiB free and setup needs about "
             f"{SETUP_FREE_GIB}; set --home to a bigger disk")


def head_commit(fork):
    """The commit the checkout is at, or None when it has none."""
    result = subprocess.run(["git", "rev-parse", "HEAD"], cwd=fork,
                            capture_output=True, text=True)
    return result.stdout.strip() if result.returncode == 0 else None


def checkout_fork(fork):
    """Put the port at the pinned commit, fetching only that commit."""
    if not (fork / ".git").is_dir():
        fork.mkdir(parents=True, exist_ok=True)
        run(["git", "init", "-q"], fork)
        run(["git", "remote", "add", "origin", FORK_URL], fork)
    if head_commit(fork) == FORK_COMMIT:
        say(f"port already at {FORK_COMMIT[:12]}")
        return
    say(f"fetching {FORK_URL} at {FORK_COMMIT[:12]}")
    run(["git", "fetch", "-q", "--depth", "1", "origin", FORK_COMMIT], fork)
    run(["git", "checkout", "-q", "--detach", FORK_COMMIT], fork)


def prefetch_rembg(fork):
    """Download the background removal model now rather than mid-run."""
    code = f"from rembg import new_session; new_session({REMBG_MODEL!r})"
    run([str(venv_python(fork)), "-c", code], fork)


def setup(args):
    """Install the port, its virtualenv, its extension and its weights.

    Safe to repeat: the port's installer skips what is already in place and
    resumes interrupted downloads, so a second run is how a failed one is
    finished.
    """
    fork = fork_dir(args)
    check_host(fork.parent)
    checkout_fork(fork)
    say("running the port's installer (its own log is partly in Russian); "
        "the weights are ~30 GB")
    run(["bash", "install.sh"], fork)
    say(f"fetching the {REMBG_MODEL} background removal model")
    prefetch_rembg(fork)
    say(f"setup complete: {fork}")


# ---------------------------------------------------------------------------
# Launcher
# ---------------------------------------------------------------------------

def launch_worker(args, argv):
    """Run this file again inside the port's virtualenv to generate."""
    fork = fork_dir(args)
    python = venv_python(fork)
    if not python.exists():
        fail(f"no install at {fork}; run with --setup first")
    worker_argv = [a for a in argv if a != "--setup"]
    env = dict(os.environ, **{WORKER_ENV: "1"})
    return subprocess.run([str(python), str(Path(__file__).resolve()),
                           *worker_argv], env=env).returncode


def main():
    """Install, generate, or both, from the system interpreter."""
    argv = sys.argv[1:]
    args = parse_args(argv)
    if os.environ.get(WORKER_ENV) == "1":
        return generate(args)
    if args.setup:
        setup(args)
    if args.image is None:
        return 0
    if not args.image.is_file():
        fail(f"no image at {args.image}")
    return launch_worker(args, argv)


# ---------------------------------------------------------------------------
# Worker: everything below runs inside the port's virtualenv.
# ---------------------------------------------------------------------------

def asset_name(raw):
    """A snake_case file stem, which the editor turns into the asset's id."""
    name = re.sub(r"[^a-z0-9]+", "_", raw.lower()).strip("_")
    return name or "generated"


def has_transparency(image):
    """True when an RGBA image already cuts its subject out."""
    return image.mode == "RGBA" and image.getchannel("A").getextrema()[0] < 255


def smooth_normals(vertices, faces):
    """Area-weighted vertex normals, shared across UV seams.

    A textured mesh splits a vertex wherever its UVs do, so normals computed
    on it directly would crease along every seam. Welding by position first
    keeps the surface smooth where only the texture layout is cut.
    """
    import numpy as np

    _, weld = np.unique(vertices, axis=0, return_inverse=True)
    weld = weld.reshape(-1)
    corners = vertices[faces]
    face_normals = np.cross(corners[:, 1] - corners[:, 0],
                            corners[:, 2] - corners[:, 0])
    sums = np.zeros((weld.max() + 1, 3))
    for k in range(3):
        np.add.at(sums, weld[faces[:, k]], face_normals)
    normals = sums[weld]
    lengths = np.linalg.norm(normals, axis=1, keepdims=True)
    return normals / np.where(lengths > 0.0, lengths, 1.0)


def material_image(mesh):
    """The base colour image a loaded mesh carries, or None."""
    material = getattr(mesh.visual, "material", None)
    if material is None:
        return None
    image = getattr(material, "image", None)
    return image if image is not None else getattr(material, "baseColorTexture",
                                                   None)


def write_obj(mesh, out_dir, name):
    """Write the OBJ, MTL and PNG the engine's loader reads, and nothing else.

    Every face corner names the same index for position, UV and normal,
    which the loader accepts in any of its `a/b/c` forms. Only `map_Kd` goes
    in the material: it is the one map the engine's shader samples.
    """
    import numpy as np

    vertices = np.asarray(mesh.vertices, dtype=np.float64)
    faces = np.asarray(mesh.faces, dtype=np.int64)
    uv = getattr(mesh.visual, "uv", None)
    image = material_image(mesh)
    textured = uv is not None and image is not None
    normals = smooth_normals(vertices, faces)

    lines = []
    if textured:
        lines += [f"mtllib {name}.mtl", f"usemtl {name}"]
        image.convert("RGB").save(out_dir / f"{name}.png")
        (out_dir / f"{name}.mtl").write_text(
            f"newmtl {name}\nKd 1 1 1\nmap_Kd {name}.png\n")
    lines += [f"v {x:.6f} {y:.6f} {z:.6f}" for x, y, z in vertices]
    if textured:
        lines += [f"vt {u:.6f} {v:.6f}" for u, v in np.asarray(uv)]
    lines += [f"vn {x:.5f} {y:.5f} {z:.5f}" for x, y, z in normals]
    corner = "{0}/{0}/{0}" if textured else "{0}//{0}"
    lines += ["f " + " ".join(corner.format(i + 1) for i in face)
              for face in faces]
    (out_dir / f"{name}.obj").write_text("\n".join(lines) + "\n")
    return textured


def write_record(args, out_dir, name, record):
    """Keep the input image and the settings beside the model."""
    source = out_dir / f"{name}.source{args.image.suffix.lower()}"
    if args.image.resolve() != source.resolve():
        shutil.copyfile(args.image, source)
    record = dict(record,
                  source=source.name,
                  generator="Hunyuan3D 2.1",
                  port=FORK_URL,
                  port_commit=FORK_COMMIT,
                  seed=args.seed,
                  steps=args.steps,
                  guidance=args.guidance,
                  octree=args.octree,
                  face_budget=args.faces,
                  generated=time.strftime("%Y-%m-%dT%H:%M:%S%z"))
    (out_dir / f"{name}.generation.json").write_text(
        json.dumps(record, indent=2) + "\n")


def load_port(fork):
    """Import the port's pipeline wrappers, from inside its own directory.

    Importing `gradio_app` detects the backend, applies the port's
    compatibility patches, and builds (but does not launch) its web UI; its
    handlers are the exact code the UI and the port's own end-to-end test
    run, which is why this calls them rather than the upstream pipelines.
    """
    sys.path.insert(0, str(fork))
    os.chdir(fork)
    import gradio_app

    return gradio_app


def make_shape(app, image, args):
    """Image to a mesh state holding the raw and the reduced mesh."""
    remove_bg = not args.keep_background and not has_transparency(image)
    say(f"shape: {args.steps} steps, octree {args.octree}, seed {args.seed}, "
        f"background removal {'on' if remove_bg else 'off'}")
    _, state, log, _ = app.generate_3d(image, remove_bg, args.steps,
                                       args.guidance, args.octree, args.seed,
                                       False, "en")
    if state is None:
        print(log, file=sys.stderr)
        fail("shape generation failed; the log is above")
    raw = state["original"]
    say(f"shape: {len(raw.vertices)} vertices, {len(raw.faces)} triangles")
    if len(raw.faces) > args.faces:
        reduced = app.decimate_mesh(raw, args.faces)
        say(f"reduced to {len(reduced.faces)} triangles")
        state = dict(state, mesh=reduced)
    return state


def paint_texture(app, state):
    """Texture the reduced mesh and load the result back as one mesh."""
    import backend
    import trimesh

    preset = backend.suggest_paint_preset(app.BACKEND)
    say(f"texture: preset {preset!r} on {app.BACKEND.kind}")
    # 0 views and 0 resolution mean "the preset's own", in resolve_params.
    _, _, saved, log = app.texture_3d_handler(state, None, preset, 0, 0,
                                              "auto", True, "en")
    if not saved:
        print(log, file=sys.stderr)
        fail("texturing failed; the log is above (--no-texture skips it)")
    return trimesh.load(saved["obj"], force="mesh", process=False)


def generate(args):
    """The worker: image in, OBJ + MTL + PNG out."""
    from PIL import Image

    out_dir = args.out.expanduser().resolve()
    out_dir.mkdir(parents=True, exist_ok=True)
    args.image = args.image.expanduser().resolve()
    name = asset_name(args.name or args.image.stem)
    image = Image.open(args.image).convert("RGBA")

    started = time.time()
    app = load_port(fork_dir(args))
    state = make_shape(app, image, args)
    shape_seconds = time.time() - started
    mesh = state["mesh"] if args.no_texture else paint_texture(app, state)

    textured = write_obj(mesh, out_dir, name)
    total_seconds = time.time() - started
    write_record(args, out_dir, name, {
        "textured": textured,
        "triangles": int(len(mesh.faces)),
        "shape_seconds": round(shape_seconds, 1),
        "total_seconds": round(total_seconds, 1),
    })
    say(f"wrote {out_dir / (name + '.obj')} ({len(mesh.faces)} triangles, "
        f"{'textured' if textured else 'untextured'}) in "
        f"{total_seconds / 60:.1f} min")
    return 0


if __name__ == "__main__":
    sys.exit(main())
