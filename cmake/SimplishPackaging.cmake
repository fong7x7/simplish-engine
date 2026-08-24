# SimplishPackaging.cmake — CPack configuration per platform
#
# Configures platform-specific packaging:
#   Windows:  NSIS installer (.exe)
#   macOS:    DragNDrop disk image (.dmg)
#   Linux:    DEB package + external AppImage (via linuxdeploy)
#   PS5/Xbox: handled by platform-specific tooling (not CPack)
#
# See docs/development/ci-cd.md §6.3 for packaging requirements.

set(CPACK_PACKAGE_NAME "Simplish")
set(CPACK_PACKAGE_VENDOR "Simplish")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Simplish — cross-platform 2D isometric game engine")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Simplish")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")

# ---------------------------------------------------------------------------
# Platform-specific generator selection
# ---------------------------------------------------------------------------
if(ENGINE_PLATFORM_WINDOWS)
    set(CPACK_GENERATOR "NSIS")

    set(CPACK_NSIS_DISPLAY_NAME "Simplish")
    set(CPACK_NSIS_PACKAGE_NAME "Simplish")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)

elseif(ENGINE_PLATFORM_MACOS)
    set(CPACK_GENERATOR "DragNDrop")

    set(CPACK_DMG_VOLUME_NAME "Simplish")
    set(CPACK_BUNDLE_NAME "Simplish")

elseif(ENGINE_PLATFORM_LINUX)
    set(CPACK_GENERATOR "DEB")

    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Simplish Team")
    set(CPACK_DEBIAN_PACKAGE_SECTION "games")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.35), libstdc++6 (>= 12)")

elseif(ENGINE_PLATFORM_CONSOLE)
    # Console packaging is handled by platform-specific tools (orbis-pub-cmd, makepkg)
    # and is built on private self-hosted runners. CPack is not used for console targets.
    message(STATUS "Console platform — CPack not configured (use platform SDK packaging)")
endif()

# ---------------------------------------------------------------------------
# Install rules and CPack (desktop only — console uses platform SDK packaging)
# ---------------------------------------------------------------------------
if(NOT ENGINE_PLATFORM_CONSOLE)
    # Game client
    if(TARGET simplish)
        install(TARGETS simplish RUNTIME DESTINATION bin)
    endif()

    # Dedicated server
    if(TARGET simplish-server)
        install(TARGETS simplish-server RUNTIME DESTINATION bin)
    endif()

    # Editor (desktop only)
    if(TARGET simplish-editor)
        install(TARGETS simplish-editor RUNTIME DESTINATION bin)
    endif()

    # Audit viewer
    if(TARGET simplish-audit-viewer)
        install(TARGETS simplish-audit-viewer RUNTIME DESTINATION bin)
    endif()

    # Data directory
    if(EXISTS "${CMAKE_SOURCE_DIR}/data")
        install(DIRECTORY "${CMAKE_SOURCE_DIR}/data/" DESTINATION data)
    endif()

    # Config directory
    if(EXISTS "${CMAKE_SOURCE_DIR}/config")
        install(DIRECTORY "${CMAKE_SOURCE_DIR}/config/" DESTINATION config)
    endif()

    # Include CPack (must be last)
    include(CPack)
endif()
