#include <ovl/path.h>

#include <ovarray.h>

#ifdef _WIN32

#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>

NODISCARD bool
ovl_path_get_module_name(NATIVE_CHAR **const module_path, void *const hinst, struct ov_error *const err) {
  if (!module_path) {
    OV_ERROR_SET_GENERIC(err, ov_error_generic_invalid_argument);
    return false;
  }

  DWORD n = 0, r;
  bool result = false;

  for (;;) {
    if (!OV_ARRAY_GROW(module_path, n += MAX_PATH)) {
      OV_ERROR_SET_GENERIC(err, ov_error_generic_out_of_memory);
      goto cleanup;
    }
    r = GetModuleFileNameW((HINSTANCE)hinst, *module_path, n);
    if (r == 0) {
      OV_ERROR_SET_HRESULT(err, HRESULT_FROM_WIN32(GetLastError()));
      goto cleanup;
    }
    if (r < n) {
      OV_ARRAY_SET_LENGTH(*module_path, r);
      break;
    }
  }
  result = true;

cleanup:
  return result;
}

#endif
