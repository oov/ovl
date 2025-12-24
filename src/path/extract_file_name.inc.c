#ifndef EXTRACT_FILE_NAME_INC_C
#define EXTRACT_FILE_NAME_INC_C
#include <ovl/path.h>

#ifndef T
#  include "extract_file_name_char.c"
#endif

T *FUNCNAME(T const *const path) {
  if (!path) {
    return NULL;
  }
  T *sep = FIND_LAST_PATH_SEP(path);
  if (sep) {
    return sep + 1;
  }
  return (T *)(uintptr_t)path;
}
#endif
