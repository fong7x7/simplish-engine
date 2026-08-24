# simplish-engine-render — abstract RHI interface
#
# Headers only: RhiDevice, RhiCommandList, resource descriptors, and the
# capture API. No concrete backend and no graphics SDK dependency — those
# live in src/platform/render/ so that a per-distribution or per-target
# rebuild never touches the engine (docs/platform/REQUIREMENTS.md §1).
#
# simplish_add_module creates an INTERFACE library when a module has no
# compiled sources, which is exactly this case.

simplish_add_module(simplish-engine-render
    INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include
    SOURCES     ""
)
