#include <ovl/path.h>

#include <ovarray.h>

#ifdef _WIN32

#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>

NODISCARD bool ovl_path_get_temp_directory(NATIVE_CHAR **const path, struct ov_error *const err) {
  if (!path) {
    OV_ERROR_SET_GENERIC(err, ov_error_generic_invalid_argument);
    return false;
  }

  bool result = false;
  DWORD sz = GetTempPathW(0, NULL); // sz includes the null terminator
  if (sz == 0) {
    OV_ERROR_SET_HRESULT(err, HRESULT_FROM_WIN32(GetLastError()));
    goto cleanup;
  }
  --sz;
  if (!OV_ARRAY_GROW(path, sz + 2)) {
    OV_ERROR_SET_GENERIC(err, ov_error_generic_out_of_memory);
    goto cleanup;
  }
  if (GetTempPathW(sz + 2, *path) == 0) {
    OV_ERROR_SET_HRESULT(err, HRESULT_FROM_WIN32(GetLastError()));
    goto cleanup;
  }
  // According to the specification, a '\\' is always appended to the end, but
  // check for fail-safe.
  {
    wchar_t *p = *path;
    if (p[sz - 1] != L'\\' && p[sz - 1] != L'/') {
      p[sz++] = L'\\';
      p[sz] = L'\0';
    }
  }
  OV_ARRAY_SET_LENGTH(*path, sz);
  result = true;

cleanup:
  return result;
}
#endif
