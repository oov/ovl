#include <ovl/os.h>

#ifdef _WIN32

#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>

bool ovl_os_get_hinstance_from_fnptr(void *fn, void **hinstance, struct ov_error *const err) {
  if (!fn || !hinstance) {
    OV_ERROR_SET_GENERIC(err, ov_error_generic_invalid_argument);
    return false;
  }

  bool result = false;

  MEMORY_BASIC_INFORMATION mbi;
  if (!VirtualQuery((LPCVOID)fn, &mbi, sizeof(mbi))) {
    OV_ERROR_SET_HRESULT(err, HRESULT_FROM_WIN32(GetLastError()));
    goto cleanup;
  }

  *hinstance = (HINSTANCE)mbi.AllocationBase;
  result = true;

cleanup:
  return result;
}

#endif
