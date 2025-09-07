#include <ovl/file.h>

#ifdef _WIN32

#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>

NODISCARD error ovl_file_read(struct ovl_file *const file,
                              void *const buffer,
                              size_t const buffer_bytes,
                              size_t *const read) {
  if (!file || !buffer || !read) {
    return errg(err_invalid_arugment);
  }
  DWORD bytesRead = 0;
  BOOL result = ReadFile((HANDLE)file, buffer, (DWORD)buffer_bytes, &bytesRead, NULL);
  if (!result) {
    return errhr(HRESULT_FROM_WIN32(GetLastError()));
  }
  *read = (size_t)bytesRead;
  return eok();
}

#endif
