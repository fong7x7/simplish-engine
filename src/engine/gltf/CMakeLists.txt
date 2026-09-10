# simplish-engine-gltf — rigged, animated models from glTF 2.0
#
# Public headers:  include/engine/gltf/
# Implementation:  src/
#
# Reads .gltf and .glb into a SkinnedModel: triangles for render-mesh's
# SkinnedMeshRenderer, and a Rig for engine/animation to pose. Written
# in-tree over nlohmann_json rather than pulling in a glTF library: the
# subset a skinned character needs is small, and every read is bounds-checked
# here under the no-exceptions rule (ADR-001).

simplish_add_module(simplish-engine-gltf
    INCLUDE_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/include
    SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/src/glb-container.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/gltf-accessor.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/gltf-buffers.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/gltf-clip-builder.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/gltf-json.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/gltf-load-error.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/gltf-loader.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/gltf-mesh-builder.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/gltf-node-pose.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/gltf-rig-builder.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/gltf-uri.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/skinned-model-orientation.cpp
    DEPENDENCIES simplish-engine-animation
                 simplish-engine-core
                 simplish-engine-render-mesh
                 nlohmann_json::nlohmann_json
)

if(ENGINE_BUILD_TESTS)
    simplish_add_test(simplish-engine-gltf-tests
        SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/test/support/test-gltf.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_glb_container.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_gltf_accessor.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_gltf_load_error.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_gltf_loader.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_gltf_node_pose.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_gltf_uri.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_skinned_model_orientation.cpp
        DEPENDENCIES simplish-engine-gltf
    )
    target_include_directories(simplish-engine-gltf-tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/test)
endif()
