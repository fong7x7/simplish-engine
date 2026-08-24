# simplish-engine-platform-xbox — OBJECT library module
#
# Xbox Series X backends: DX12 extensions, XAudio2, GameInput, Xbox Live.
# NDA — excluded from public repo.

simplish_add_module(simplish-engine-platform-xbox
    INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include
    SRC_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/src
    DEPENDENCIES simplish-engine-core simplish-engine-render
)
