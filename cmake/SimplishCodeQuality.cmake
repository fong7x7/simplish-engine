# SimplishCodeQuality.cmake — clang-format and clang-tidy custom targets
#
# Provides:
#   format-check  — dry-run clang-format (CI gate)
#   format-fix    — in-place clang-format
#   tidy          — run clang-tidy on all sources
#
# Uses the project's .clang-format and .clang-tidy configuration files.

# ---------------------------------------------------------------------------
# Collect source files
# ---------------------------------------------------------------------------
file(GLOB_RECURSE ENGINE_ALL_SOURCES
    "${CMAKE_SOURCE_DIR}/engine/*.cpp"
    "${CMAKE_SOURCE_DIR}/engine/*.h"
    "${CMAKE_SOURCE_DIR}/game/*.cpp"
    "${CMAKE_SOURCE_DIR}/game/*.h"
    "${CMAKE_SOURCE_DIR}/editor/*.cpp"
    "${CMAKE_SOURCE_DIR}/editor/*.h"
    "${CMAKE_SOURCE_DIR}/server/*.cpp"
    "${CMAKE_SOURCE_DIR}/server/*.h"
    "${CMAKE_SOURCE_DIR}/bin/*.cpp"
    "${CMAKE_SOURCE_DIR}/bin/*.h"
    "${CMAKE_SOURCE_DIR}/sandbox/*.cpp"
    "${CMAKE_SOURCE_DIR}/sandbox/*.h"
    "${CMAKE_SOURCE_DIR}/tests/*.cpp"
    "${CMAKE_SOURCE_DIR}/tests/*.h"
)

# ---------------------------------------------------------------------------
# clang-format targets
# ---------------------------------------------------------------------------
find_program(CLANG_FORMAT_EXE NAMES clang-format)

if(CLANG_FORMAT_EXE AND ENGINE_ALL_SOURCES)
    message(STATUS "clang-format found: ${CLANG_FORMAT_EXE}")

    # Dry-run check (used by CI — fails on formatting violations)
    add_custom_target(format-check
        COMMAND ${CLANG_FORMAT_EXE} --dry-run --Werror ${ENGINE_ALL_SOURCES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Checking code formatting (clang-format --dry-run --Werror)"
        VERBATIM
    )

    # In-place fix
    add_custom_target(format-fix
        COMMAND ${CLANG_FORMAT_EXE} -i ${ENGINE_ALL_SOURCES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Fixing code formatting (clang-format -i)"
        VERBATIM
    )
elseif(NOT CLANG_FORMAT_EXE)
    message(STATUS "clang-format not found — format-check and format-fix targets disabled")
else()
    message(STATUS "No source files found — format-check and format-fix targets disabled")
endif()

# ---------------------------------------------------------------------------
# clang-tidy target
# ---------------------------------------------------------------------------
find_program(CLANG_TIDY_EXE NAMES clang-tidy)

if(CLANG_TIDY_EXE AND ENGINE_ALL_SOURCES)
    message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE}")

    # compile_commands.json may not exist at configure time (it's generated
    # during the build). The tidy target is always created; it will fail
    # at build time if the file is missing, which is acceptable.
    add_custom_target(tidy
        COMMAND ${CLANG_TIDY_EXE}
            -p ${CMAKE_BINARY_DIR}
            ${ENGINE_ALL_SOURCES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running clang-tidy"
        VERBATIM
    )
elseif(NOT CLANG_TIDY_EXE)
    message(STATUS "clang-tidy not found — tidy target disabled")
else()
    message(STATUS "No source files found — tidy target disabled")
endif()

# ---------------------------------------------------------------------------
# Optional: set clang-tidy as a property on all targets for IDE integration
# ---------------------------------------------------------------------------
option(ENGINE_ENABLE_CLANG_TIDY "Enable clang-tidy during build (slower)" OFF)
if(ENGINE_ENABLE_CLANG_TIDY AND CLANG_TIDY_EXE)
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}" CACHE STRING "" FORCE)
    message(STATUS "clang-tidy enabled during build")
endif()
