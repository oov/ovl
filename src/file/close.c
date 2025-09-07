#include <ovl/file.h>

#ifdef _WIN32

#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>

void ovl_file_close(struct ovl_file *const file) {
  if (!file) {
    return;
  }
  CloseHandle((HANDLE)file);
}

#endif
