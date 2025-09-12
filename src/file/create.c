#include <ovl/file.h>

#ifdef _WIN32

#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>

NODISCARD bool
ovl_file_create(NATIVE_CHAR const *const path, struct ovl_file **const file, struct ov_error *const err) {
  if (!path || !file || *file) {
    OV_ERROR_SET_GENERIC(err, ov_error_generic_invalid_argument);
    return false;
  }

  HANDLE h = INVALID_HANDLE_VALUE;
  bool result = false;

  h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (h == INVALID_HANDLE_VALUE) {
    OV_ERROR_SET_HRESULT(err, HRESULT_FROM_WIN32(GetLastError()));
    goto cleanup;
  }

  *file = (struct ovl_file *)h;
  h = INVALID_HANDLE_VALUE;
  result = true;

cleanup:
  if (h != INVALID_HANDLE_VALUE) {
    CloseHandle(h);
    h = INVALID_HANDLE_VALUE;
  }
  return result;
}

#endif
