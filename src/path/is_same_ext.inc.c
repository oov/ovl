#ifndef IS_SAME_EXT_INC_C
#define IS_SAME_EXT_INC_C
#include <ovl/path.h>

#ifndef T
#  include "is_same_ext_char.c"
#endif

static inline T TO_LOWER(T c) {
  if (c >= CHR('A') && c <= CHR('Z')) {
    return c | 0x20;
  }
  return c;
}

bool FUNCNAME(T const *const ext1, T const *const ext2) {
  if (!ext1 || !ext2) {
    return false;
  }
  T const *p1 = ext1;
  T const *p2 = ext2;
  for (; *p1 != CHR('\0') && *p2 != CHR('\0'); ++p1, ++p2) {
    if (TO_LOWER(*p1) != TO_LOWER(*p2)) {
      return false;
    }
  }
  return *p1 == CHR('\0') && *p2 == CHR('\0');
}
#endif
