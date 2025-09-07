#include <ovl/path.h>

#include <ovarray.h>

#ifdef _WIN32

#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>

NODISCARD error ovl_path_get_module_name(NATIVE_CHAR **const module_path, void *const hinst) {
  DWORD n = 0, r;
  error err = eok();
  for (;;) {
    err = OV_ARRAY_GROW(module_path, n += MAX_PATH);
    if (efailed(err)) {
      err = ethru(err);
      goto cleanup;
    }
    r = GetModuleFileNameW((HINSTANCE)hinst, *module_path, n);
    if (r == 0) {
      err = errhr(HRESULT_FROM_WIN32(GetLastError()));
      goto cleanup;
    }
    if (r < n) {
      OV_ARRAY_SET_LENGTH(*module_path, r);
      break;
    }
  }
cleanup:
  return err;
}

#endif
