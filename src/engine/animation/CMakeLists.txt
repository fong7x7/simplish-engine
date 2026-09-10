# simplish-engine-animation — skeletons, skins, and the clips that pose them
#
# Public headers:  include/engine/animation/
# Implementation:  src/
#
# Pure CPU math with no GPU and no file format in it: a loader fills a Rig,
# and a renderer draws with the skin matrices RigPose produces. Presentation
# only — the simulation never reads a joint (ADR-003 amendment 2026-09-10).

simplish_add_module(simplish-engine-animation
    INCLUDE_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/include
    SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/src/joint-pose.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/skeleton.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/pose-sampling.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/rig-pose.cpp
    DEPENDENCIES simplish-engine-math
)

if(ENGINE_BUILD_TESTS)
    simplish_add_test(simplish-engine-animation-tests
        SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/test/test_joint_pose.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_skeleton.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_pose_sampling.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_rig_pose.cpp
        DEPENDENCIES simplish-engine-animation
    )
endif()
