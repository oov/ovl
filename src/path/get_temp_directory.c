#include <ovl/path.h>

#include <ovarray.h>

#ifdef _WIN32

#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>

NODISCARD error ovl_path_get_temp_directory(NATIVE_CHAR **const path) {
  error err = eok();
  DWORD sz = GetTempPathW(0, NULL); // sz includes the null terminator
  if (sz == 0) {
    err = errhr(HRESULT_FROM_WIN32(GetLastError()));
    goto cleanup;
  }
  --sz;
  err = OV_ARRAY_GROW(path, sz + 2);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  if (GetTempPathW(sz + 2, *path) == 0) {
    err = errhr(HRESULT_FROM_WIN32(GetLastError()));
    goto cleanup;
  }
  // According to the specification, a '\\' is always appended to the end, but
  // check for fail-safe.
  wchar_t *p = *path;
  if (p[sz - 1] != L'\\' && p[sz - 1] != L'/') {
    p[sz++] = L'\\';
    p[sz] = L'\0';
  }
  OV_ARRAY_SET_LENGTH(*path, sz);
cleanup:
  return err;
}
#endif
