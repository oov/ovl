#include <wchar.h>

#define T wchar_t
#define CHR(x) L##x
#define STRRCHR wcsrchr
#define FUNCNAME ovl_path_find_last_path_sep_wchar
#include "find_last_path_sep.inc.c"
