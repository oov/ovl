#include <ovl/file.h>

#ifdef _WIN32

#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>

NODISCARD bool ovl_file_read(struct ovl_file *const file,
                             void *const buffer,
                             size_t const buffer_bytes,
                             size_t *const read,
                             struct ov_error *const err) {
  if (!file || !buffer || !read) {
    OV_ERROR_SET_GENERIC(err, ov_error_generic_invalid_argument);
    return false;
  }

  DWORD bytesRead = 0;
  BOOL result = ReadFile((HANDLE)file, buffer, (DWORD)buffer_bytes, &bytesRead, NULL);
  if (!result) {
    OV_ERROR_SET_HRESULT(err, HRESULT_FROM_WIN32(GetLastError()));
    return false;
  }

  *read = (size_t)bytesRead;
  return true;
}

#endif
