#include <ovl/file.h>

#ifdef _WIN32

#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>

NODISCARD bool ovl_file_tell(struct ovl_file *const file, int64_t *const pos, struct ov_error *const err) {
  if (!file || !pos) {
    OV_ERROR_SET_GENERIC(err, ov_error_generic_invalid_argument);
    return false;
  }

  LARGE_INTEGER li;
  if (!SetFilePointerEx((HANDLE)file, (LARGE_INTEGER){0}, &li, FILE_CURRENT)) {
    OV_ERROR_SET_HRESULT(err, HRESULT_FROM_WIN32(GetLastError()));
    return false;
  }

  *pos = li.QuadPart;
  return true;
}

#endif
