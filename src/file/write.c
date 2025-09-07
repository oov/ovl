#include <ovl/file.h>

#ifdef _WIN32

#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>

NODISCARD error ovl_file_write(struct ovl_file *const file,
                               void const *const buffer,
                               size_t const buffer_bytes,
                               size_t *const written) {
  if (!file || !buffer || !written) {
    return errg(err_invalid_arugment);
  }
  DWORD bytesWritten = 0;
  BOOL result = WriteFile((HANDLE)file, buffer, (DWORD)buffer_bytes, &bytesWritten, NULL);
  if (!result) {
    return errhr(HRESULT_FROM_WIN32(GetLastError()));
  }
  *written = (size_t)bytesWritten;
  return eok();
}

#endif
