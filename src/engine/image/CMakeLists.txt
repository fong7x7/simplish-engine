# simplish-engine-image — stb_image and stb_image_write implementation units
#
# One definition of each stb implementation for the whole project. Linked by
# the GUI (image loading, rasterizer PNG capture) and by the RHI backends in
# src/platform/ (the capture API). See stb-image-codecs.h for why this is its
# own module rather than living in either consumer.

simplish_add_module(simplish-engine-image
    INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include
    SRC_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/src
    DEPENDENCIES stb::stb
)
