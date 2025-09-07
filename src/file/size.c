#include <ovl/file.h>

#ifdef _WIN32

#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>

NODISCARD error ovl_file_size(struct ovl_file *const file, uint64_t *const size) {
  if (!file || !size) {
    return errg(err_invalid_arugment);
  }
  LARGE_INTEGER li;
  if (!GetFileSizeEx((HANDLE)file, &li)) {
    return errhr(HRESULT_FROM_WIN32(GetLastError()));
  }
  *size = (uint64_t)li.QuadPart;
  return eok();
}

#endif
