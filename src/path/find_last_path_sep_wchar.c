#include <ovl/path.h>
#include <wchar.h>

wchar_t const *ovl_path_find_last_path_sep_wchar_const(wchar_t const *const path) {
  wchar_t const *sep = wcsrchr(path, L'\\');
  if (!sep) {
    sep = wcsrchr(path, L'/');
  }
  return sep;
}

wchar_t *ovl_path_find_last_path_sep_wchar_mut(wchar_t *const path) {
  return (wchar_t *)ov_deconster_(ovl_path_find_last_path_sep_wchar_const(path));
}
