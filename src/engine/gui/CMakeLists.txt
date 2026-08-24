# simplish-engine-gui — retained-mode UI framework
#
# Public headers:  include/engine/gui/
# Implementation:  src/

simplish_add_module(simplish-engine-gui
    INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include
    SRC_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/src
    DEPENDENCIES freetype nlohmann_json::nlohmann_json
        simplish-engine-core
        simplish-engine-image
        simplish-engine-math
        simplish-engine-render
)

# SDL3 for UI draw ops and the text renderer.
target_link_libraries(simplish-engine-gui PRIVATE SDL3::SDL3)

# stb_image for the image loader (PNG, JPG, BMP, TGA, GIF decoding).
target_link_libraries(simplish-engine-gui PRIVATE stb::stb)

# FreeType headers for glyph rasterisation in the text pipeline.
target_include_directories(simplish-engine-gui PRIVATE
    ${freetype_SOURCE_DIR}/include
)

file(GLOB _gui_tests "${CMAKE_CURRENT_SOURCE_DIR}/test/*.cpp")
if(ENGINE_BUILD_TESTS AND _gui_tests)
    simplish_add_test(simplish-engine-gui-tests
        SOURCES      ${_gui_tests}
        DEPENDENCIES simplish-engine-gui
    )
endif()
