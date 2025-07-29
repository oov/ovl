#pragma once

#include <ovbase.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

NODISCARD error ovl_path_get_temp_file(wchar_t **const path, wchar_t const *const filename);
NODISCARD error ovl_path_get_module_name(wchar_t **const module_path, HINSTANCE const hinst);
wchar_t const *ovl_path_extract_file_name_const(wchar_t const *const path);
wchar_t *ovl_path_extract_file_name_mut(wchar_t *const path);
#define ovl_path_extract_file_name(path)                                                                               \
  _Generic((path), wchar_t const *: ovl_path_extract_file_name_const, wchar_t *: ovl_path_extract_file_name_mut)(path)

NODISCARD error ovl_path_select_file(HWND const window,
                                     wchar_t const *const title,
                                     wchar_t const *const filter,
                                     GUID const *const client_id,
                                     wchar_t **const path);
NODISCARD error ovl_path_select_folder(HWND const window,
                                       wchar_t const *const title,
                                       GUID const *const client_id,
                                       wchar_t **const path);
