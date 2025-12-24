#include <ovbase.h>

#define T wchar_t
#define CHR(x) L##x
#define FUNCNAME ovl_path_is_same_ext_wchar
#include "is_same_ext.inc.c"
