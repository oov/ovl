#include <wchar.h>

#define T wchar_t
#define CHR(x) L##x
#define STRRCHR wcsrchr
#define EXTRACT_FILE_NAME ovl_path_extract_file_name_wchar
#define FUNCNAME ovl_path_find_ext_wchar
#include "find_ext.inc.c"
