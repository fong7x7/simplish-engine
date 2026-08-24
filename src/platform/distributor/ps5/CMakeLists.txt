# simplish-engine-platform-ps5 — OBJECT library module
#
# PS5-specific backends: GNM rendering, Tempest audio, DualSense, PSN.
# NDA — excluded from public repo.

simplish_add_module(simplish-engine-platform-ps5
    INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include
    SRC_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/src
    DEPENDENCIES simplish-engine-core simplish-engine-render
)
