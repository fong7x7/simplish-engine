# simplish-platform-audio — the speakers this platform has
#
# Public headers:  include/engine/audio/
# Backends:        src/<name>/, one compiled in
#
# The engine mixes (src/engine/audio/: clips, voices, buses, the listener)
# into float frames; this package decides which output exists and pulls
# those frames into it from the backend's own thread. Exactly one backend
# is compiled in:
#
#   SDL        desktop — SDL3's audio stream on the default playback device
#   TEMPEST    PS5 — from the private console overlay
#   XAUDIO2    Xbox — from the private console overlay
#   NONE       no sound; also where a console lands without its overlay
#
# Override with -DENGINE_AUDIO_BACKEND=<name>.

if(ENGINE_PLATFORM_DESKTOP)
    set(_audio_default SDL)
elseif(ENGINE_PLATFORM_PS5)
    set(_audio_default TEMPEST)
elseif(ENGINE_PLATFORM_XBOX)
    set(_audio_default XAUDIO2)
else()
    set(_audio_default NONE)
endif()
set(ENGINE_AUDIO_BACKEND "${_audio_default}" CACHE STRING
    "Audio backend: SDL, TEMPEST, XAUDIO2 or NONE")

string(TOLOWER "${ENGINE_AUDIO_BACKEND}" _audio_dir)
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/${_audio_dir}")
    message(STATUS "Audio backend ${ENGINE_AUDIO_BACKEND} is not in this "
                   "tree (console backends come from the private overlay); "
                   "building with no sound")
    set(_audio_dir none)
endif()

set(_audio_sources_sdl
    "${CMAKE_CURRENT_SOURCE_DIR}/src/sdl/sdl-audio-device.cpp")
set(_audio_sources_none
    "${CMAKE_CURRENT_SOURCE_DIR}/src/none/no-audio-device.cpp")

simplish_add_module(simplish-platform-audio
    INCLUDE_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/include
    SOURCES      ${_audio_sources_${_audio_dir}}
    DEPENDENCIES simplish-engine-audio
)
if(_audio_dir STREQUAL "sdl")
    target_link_libraries(simplish-platform-audio PUBLIC SDL3::SDL3)
endif()

if(ENGINE_BUILD_TESTS)
    simplish_add_test(simplish-platform-audio-tests
        SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/test/test_audio_device.cpp
        DEPENDENCIES simplish-platform-audio
    )
    if(_audio_dir STREQUAL "none")
        target_compile_definitions(simplish-platform-audio-tests
            PRIVATE ENGINE_AUDIO_BACKEND_NONE=1)
    endif()
endif()
