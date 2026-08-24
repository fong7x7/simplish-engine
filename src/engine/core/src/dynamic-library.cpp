#include <engine/core/dynamic-library.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace eng {

void* DynamicLibrary::open(std::string_view path) {
  std::string path_str(path);
#ifdef _WIN32
  return static_cast<void*>(LoadLibraryA(path_str.c_str()));
#else
  return dlopen(path_str.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
}

void DynamicLibrary::close(void* handle) {
  if (handle == nullptr) {
    return;
  }
#ifdef _WIN32
  FreeLibrary(static_cast<HMODULE>(handle));
#else
  dlclose(handle);
#endif
}

void* DynamicLibrary::symbol(void* handle, std::string_view name) {
  if (handle == nullptr) {
    return nullptr;
  }
  std::string name_str(name);
#ifdef _WIN32
  // NOLINTNEXTLINE(google-readability-casting) — Win32 API requires cast
  return reinterpret_cast<void*>(
      GetProcAddress(static_cast<HMODULE>(handle), name_str.c_str()));
#else
  return dlsym(handle, name_str.c_str());
#endif
}

std::string DynamicLibrary::lastError() {
#ifdef _WIN32
  DWORD error_code = GetLastError();
  if (error_code == 0) {
    return {};
  }
  LPSTR buffer = nullptr;
  FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                 nullptr, error_code, 0, reinterpret_cast<LPSTR>(&buffer), 0,
                 nullptr);
  std::string message(buffer != nullptr ? buffer : "unknown error");
  LocalFree(buffer);
  return message;
#else
  const char* msg = dlerror();  // NOLINT(concurrency-mt-unsafe) — no
                                // thread-safe alternative for dlerror
  return msg != nullptr ? std::string(msg) : std::string{};
#endif
}

}  // namespace eng
