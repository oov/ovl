#include <ovl/file.h>

#ifdef _WIN32

#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>

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

#endif
