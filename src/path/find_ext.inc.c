#ifndef FIND_EXT_INC_C
#define FIND_EXT_INC_C
#include <ovl/path.h>

#ifndef T
#  include "find_ext_char.c"
#endif

T *FUNCNAME(T const *const path) {
  if (!path) {
    return NULL;
  }
  // First, find the last path separator to avoid false positives
  // like "/home/user/repo.git/LICENSE"
  T const *const filename = EXTRACT_FILE_NAME(path);
  if (!filename) {
    return NULL;
  }
  return STRRCHR(filename, CHR('.'));
}
#endif
