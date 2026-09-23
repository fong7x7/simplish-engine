# simplish-engine-audio — clips, a mixer, and playing sounds into it
#
# Public headers:  include/engine/audio/
# Implementation:  src/
#
# Platform-free: decoding WAV and Ogg Vorbis into clips, synthesising
# stand-in sounds, and a software mixer that places sounds around a
# listener, steals voices by priority, and ducks the music — all into a
# buffer of float frames. Which device those frames go to, and on what
# thread, is the platform's (src/platform/audio/); nothing here knows one.
#
# stb_vorbis's implementation is compiled once, here: nothing else in the
# tree decodes Vorbis (see src/engine/image/ for the same rule on stb_image).

simplish_add_module(simplish-engine-audio
    INCLUDE_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/include
    SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/src/audio-clip.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/audio-clip-bank.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/audio-command-queue.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/audio-engine.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/audio-mixer.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/audio-spatial.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/audio-synth.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/audio-volumes.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/audio-volumes-json.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/ogg-decode.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/stb-vorbis-impl.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/wav-decode.cpp
    DEPENDENCIES simplish-engine-math
                 nlohmann_json::nlohmann_json
                 stb::stb
)

if(ENGINE_BUILD_TESTS)
    simplish_add_test(simplish-engine-audio-tests
        SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/test/test_audio_clip.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_audio_clip_bank.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_audio_command_queue.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_audio_decode.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_audio_engine.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_audio_mixer.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_audio_spatial.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_audio_synth.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_audio_volumes.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_audio_volumes_json.cpp
        DEPENDENCIES simplish-engine-audio
    )
endif()
