#include <ovl/file.h>

#ifdef _WIN32

#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>

NODISCARD bool ovl_file_write(struct ovl_file *const file,
                              void const *const buffer,
                              size_t const buffer_bytes,
                              size_t *const written,
                              struct ov_error *const err) {
  if (!file || !buffer || !written) {
    OV_ERROR_SET_GENERIC(err, ov_error_generic_invalid_argument);
    return false;
  }

  DWORD bytesWritten = 0;
  BOOL result = WriteFile((HANDLE)file, buffer, (DWORD)buffer_bytes, &bytesWritten, NULL);
  if (!result) {
    OV_ERROR_SET_HRESULT(err, HRESULT_FROM_WIN32(GetLastError()));
    return false;
  }

  *written = (size_t)bytesWritten;
  return true;
}

#endif
