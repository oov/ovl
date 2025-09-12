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

NODISCARD bool ovl_file_seek(struct ovl_file *const file,
                             int64_t pos,
                             enum ovl_file_seek_method const method,
                             struct ov_error *const err) {
  if (!file || (method != ovl_file_seek_method_set && method != ovl_file_seek_method_cur &&
                method != ovl_file_seek_method_end)) {
    OV_ERROR_SET_GENERIC(err, ov_error_generic_invalid_argument);
    return false;
  }

  BOOL result = SetFilePointerEx((HANDLE)file, (LARGE_INTEGER){.QuadPart = pos}, NULL, convert_seek_method(method));
  if (!result) {
    OV_ERROR_SET_HRESULT(err, HRESULT_FROM_WIN32(GetLastError()));
    return false;
  }

  return true;
}

#endif
