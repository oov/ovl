#include <ovl/file.h>

#ifdef _WIN32

#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>

NODISCARD error ovl_file_tell(struct ovl_file *const file, int64_t *const pos) {
  if (!file || !pos) {
    return errg(err_invalid_arugment);
  }
  LARGE_INTEGER li;
  if (!SetFilePointerEx((HANDLE)file, (LARGE_INTEGER){0}, &li, FILE_CURRENT)) {
    return errhr(HRESULT_FROM_WIN32(GetLastError()));
  }
  *pos = li.QuadPart;
  return eok();
}

#endif
