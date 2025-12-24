#include <ovl/path.h>
#include <wchar.h>

wchar_t const *ovl_path_find_last_path_sep_wchar_const(wchar_t const *const path) {
  wchar_t const *sep1 = wcsrchr(path, L'\\');
  wchar_t const *sep2 = wcsrchr(path, L'/');
  if (!sep1) {
    return sep2;
  }
  if (!sep2) {
    return sep1;
  }
  return (sep1 > sep2) ? sep1 : sep2;
}

wchar_t *ovl_path_find_last_path_sep_wchar_mut(wchar_t *const path) {
  return (wchar_t *)ov_deconster_(ovl_path_find_last_path_sep_wchar_const(path));
}
