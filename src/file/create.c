#include <ovl/file.h>

#ifdef _WIN32

#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>

NODISCARD error ovl_file_create(NATIVE_CHAR const *const path, struct ovl_file **const file) {
  if (!path || !file || *file) {
    return errg(err_invalid_arugment);
  }
  HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (h == INVALID_HANDLE_VALUE) {
    return errhr(HRESULT_FROM_WIN32(GetLastError()));
  }
  *file = (struct ovl_file *)h;
  return eok();
}

#endif
