#pragma once

#include <ovbase.h>

// file/folder select dialog

NODISCARD error ovl_dialog_select_file(void *const hwnd,
                                       NATIVE_CHAR const *const title,
                                       NATIVE_CHAR const *const filter,
                                       void const *const guid_client_id,
                                       NATIVE_CHAR **const path);

NODISCARD error ovl_dialog_select_folder(void *const hwnd,
                                         NATIVE_CHAR const *const title,
                                         void const *const guid_client_id,
                                         NATIVE_CHAR **const path);
