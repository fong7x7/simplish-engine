# simplish-engine — the engine static library
#
# Platform-agnostic. Dependency order:
#
#   math ──► core ──► gui ──► client
#          image ──┘  ▲        │
#         render ─────┴────────┘
#            └──► render-mesh ──┘
#   math ──► core ──► sim ──► input
#   math ──► physics ──► spatial
#   math ──► animation ──► gltf ◄── render-mesh
#
# Concrete RHI backends, the desktop client, and distributor services are NOT
# here — they are src/platform/, which links this library. That split is what
# lets a per-target or per-distribution build recompile only the platform
# layer while these object files stay valid.

add_subdirectory(math)
add_subdirectory(core)
add_subdirectory(sim)
add_subdirectory(input)
add_subdirectory(physics)
add_subdirectory(spatial)
add_subdirectory(animation)
add_subdirectory(image)
add_subdirectory(render)
add_subdirectory(render-mesh)
add_subdirectory(gltf)
add_subdirectory(gui)
add_subdirectory(client)

simplish_add_library(simplish-engine
    TYPE    STATIC
    MODULES simplish-engine-math
            simplish-engine-core
            simplish-engine-sim
            simplish-engine-input
            simplish-engine-physics
            simplish-engine-spatial
            simplish-engine-animation
            simplish-engine-image
            simplish-engine-render
            simplish-engine-render-mesh
            simplish-engine-gltf
            simplish-engine-gui
            simplish-engine-client
)
