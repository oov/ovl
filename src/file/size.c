#include <ovl/file.h>

#ifdef _WIN32

#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>

NODISCARD bool ovl_file_size(struct ovl_file *const file, uint64_t *const size, struct ov_error *const err) {
  if (!file || !size) {
    OV_ERROR_SET_GENERIC(err, ov_error_generic_invalid_argument);
    return false;
  }

  LARGE_INTEGER li;
  if (!GetFileSizeEx((HANDLE)file, &li)) {
    OV_ERROR_SET_HRESULT(err, HRESULT_FROM_WIN32(GetLastError()));
    return false;
  }

  *size = (uint64_t)li.QuadPart;
  return true;
}

#endif
