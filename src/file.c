#include <ovl/file.h>

#ifdef _WIN32

#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>

NODISCARD error ovl_file_create(NATIVE_CHAR const *const path, struct ovl_file **const file) {
  if (!path || !file || *file) {
    return errg(err_invalid_arugment);
  }
  HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (h == INVALID_HANDLE_VALUE) {
    return errhr(HRESULT_FROM_WIN32(GetLastError()));
  }
  *file = (struct ovl_file *)h;
  return eok();
}

NODISCARD error ovl_file_open(NATIVE_CHAR const *const path, struct ovl_file **const file) {
  if (!path || !file || *file) {
    return errg(err_invalid_arugment);
  }
  HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (h == INVALID_HANDLE_VALUE) {
    return errhr(HRESULT_FROM_WIN32(GetLastError()));
  }
  *file = (struct ovl_file *)h;
  return eok();
}

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

static inline DWORD convert_seek_method(enum ovl_file_seek_method const method) {
  switch (method) {
  case ovl_file_seek_method_set:
    return FILE_BEGIN;
  case ovl_file_seek_method_cur:
    return FILE_CURRENT;
  case ovl_file_seek_method_end:
    return FILE_END;
  }
  return FILE_BEGIN;
}

NODISCARD error ovl_file_seek(struct ovl_file *const file, int64_t pos, enum ovl_file_seek_method const method) {
  if (!file || (method != ovl_file_seek_method_set && method != ovl_file_seek_method_cur &&
                method != ovl_file_seek_method_end)) {
    return errg(err_invalid_arugment);
  }
  BOOL result = SetFilePointerEx((HANDLE)file, (LARGE_INTEGER){.QuadPart = pos}, NULL, convert_seek_method(method));
  if (!result) {
    return errhr(HRESULT_FROM_WIN32(GetLastError()));
  }
  return eok();
}

NODISCARD error ovl_file_tell(struct ovl_file *const file, int64_t *const pos) {
  if (!file || !pos) {
    return errg(err_invalid_arugment);
  }
  LARGE_INTEGER li;
  if (!SetFilePointerEx((HANDLE)file, (LARGE_INTEGER){0}, &li, FILE_CURRENT)) {
    return errhr(HRESULT_FROM_WIN32(GetLastError()));
  }
  *pos = li.QuadPart;
  return eok();
}

NODISCARD error ovl_file_size(struct ovl_file *const file, uint64_t *const size) {
  if (!file || !size) {
    return errg(err_invalid_arugment);
  }
  LARGE_INTEGER li;
  if (!GetFileSizeEx((HANDLE)file, &li)) {
    return errhr(HRESULT_FROM_WIN32(GetLastError()));
  }
  *size = (uint64_t)li.QuadPart;
  return eok();
}

void ovl_file_close(struct ovl_file *const file) {
  if (!file) {
    return;
  }
  CloseHandle((HANDLE)file);
}

#endif
