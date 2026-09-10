# windows-x64-clang-cl.cmake — Cross-compile for Windows x86_64 from a
# non-Windows host (macOS or Linux).
#
# clang-cl compiles and lld-link links against the MSVC CRT and Windows SDK
# laid out by xwin (https://github.com/Jake-Shadle/xwin) in /winsysroot form.
# This builds the DX12 backend without a Windows machine; nothing it produces
# runs on the host. See docs/development/REQUIREMENTS.md §3.1.
#
# One-time setup on macOS:
#   brew install llvm lld xwin
#   xwin --accept-license --cache-dir ~/.xwin/cache --arch x86_64 splat \
#        --include-debug-libs --use-winsysroot-style \
#        --preserve-ms-arch-notation --disable-symlinks \
#        --output ~/.xwin/sysroot
#
# The sysroot defaults to ~/.xwin/sysroot; set SIMPLISH_WINDOWS_SYSROOT in the
# environment to use another one.

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_VERSION 10.0)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

# ---------------------------------------------------------------------------
# Sysroot
# ---------------------------------------------------------------------------
# Read from the environment rather than the cache so the try_compile projects
# CMake spawns during compiler checks resolve the same path.
if(DEFINED ENV{SIMPLISH_WINDOWS_SYSROOT})
    set(_simplish_winsysroot "$ENV{SIMPLISH_WINDOWS_SYSROOT}")
else()
    set(_simplish_winsysroot "$ENV{HOME}/.xwin/sysroot")
endif()

if(NOT IS_DIRECTORY "${_simplish_winsysroot}/Windows Kits/10/Include")
    message(FATAL_ERROR
        "No Windows SDK at '${_simplish_winsysroot}'. Lay one out with xwin "
        "(see the header of ${CMAKE_CURRENT_LIST_FILE}) or point "
        "SIMPLISH_WINDOWS_SYSROOT at an existing /winsysroot tree.")
endif()

# ---------------------------------------------------------------------------
# Tools
# ---------------------------------------------------------------------------
# Homebrew keeps llvm and lld keg-only, so look in their prefixes before PATH.
set(_simplish_llvm_hints
    /opt/homebrew/opt/llvm/bin
    /opt/homebrew/opt/lld/bin
    /usr/local/opt/llvm/bin
    /usr/local/opt/lld/bin
)

find_program(SIMPLISH_CLANG_CL clang-cl HINTS ${_simplish_llvm_hints} REQUIRED)
find_program(SIMPLISH_LLD_LINK lld-link HINTS ${_simplish_llvm_hints} REQUIRED)
find_program(SIMPLISH_LLVM_LIB llvm-lib HINTS ${_simplish_llvm_hints} REQUIRED)
find_program(SIMPLISH_LLVM_RC llvm-rc HINTS ${_simplish_llvm_hints} REQUIRED)
find_program(SIMPLISH_LLVM_MT llvm-mt HINTS ${_simplish_llvm_hints} REQUIRED)

set(CMAKE_C_COMPILER   "${SIMPLISH_CLANG_CL}")
set(CMAKE_CXX_COMPILER "${SIMPLISH_CLANG_CL}")
set(CMAKE_LINKER       "${SIMPLISH_LLD_LINK}")
set(CMAKE_AR           "${SIMPLISH_LLVM_LIB}")
set(CMAKE_RC_COMPILER  "${SIMPLISH_LLVM_RC}")
set(CMAKE_MT           "${SIMPLISH_LLVM_MT}")

set(CMAKE_C_COMPILER_TARGET   x86_64-pc-windows-msvc)
set(CMAKE_CXX_COMPILER_TARGET x86_64-pc-windows-msvc)

# ---------------------------------------------------------------------------
# Flags
# ---------------------------------------------------------------------------
# /winsysroot finds the newest MSVC and SDK versions under the tree, so no
# version numbers are pinned here.
set(CMAKE_C_FLAGS_INIT   "-winsysroot \"${_simplish_winsysroot}\"")
set(CMAKE_CXX_FLAGS_INIT "-winsysroot \"${_simplish_winsysroot}\"")

foreach(_kind EXE SHARED MODULE)
    set(CMAKE_${_kind}_LINKER_FLAGS_INIT
        "/winsysroot:\"${_simplish_winsysroot}\"")
endforeach()

# llvm-rc has no /winsysroot, so the .rc files dependencies ship (SDL3,
# FreeType) need the newest SDK and MSVC include directories spelled out.
file(GLOB _simplish_sdk_dirs LIST_DIRECTORIES true
    "${_simplish_winsysroot}/Windows Kits/10/Include/*")
file(GLOB _simplish_msvc_dirs LIST_DIRECTORIES true
    "${_simplish_winsysroot}/VC/Tools/MSVC/*")
list(SORT _simplish_sdk_dirs COMPARE NATURAL ORDER DESCENDING)
list(SORT _simplish_msvc_dirs COMPARE NATURAL ORDER DESCENDING)
list(GET _simplish_sdk_dirs 0 _simplish_sdk_inc)
list(GET _simplish_msvc_dirs 0 _simplish_msvc_dir)

set(CMAKE_RC_FLAGS_INIT "")
foreach(_dir "${_simplish_sdk_inc}/um" "${_simplish_sdk_inc}/shared"
             "${_simplish_sdk_inc}/ucrt" "${_simplish_msvc_dir}/include")
    string(APPEND CMAKE_RC_FLAGS_INIT " /I \"${_dir}\"")
endforeach()

# Look for libraries and headers in the sysroot, programs on the host.
set(CMAKE_FIND_ROOT_PATH "${_simplish_winsysroot}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
