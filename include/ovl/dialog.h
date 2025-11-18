#pragma once

#include <ovbase.h>

// file/folder select dialog

NODISCARD bool ovl_dialog_select_file(void *const hwnd,
                                      NATIVE_CHAR const *const title,
                                      NATIVE_CHAR const *const filter,
                                      void const *const guid_client_id,
                                      NATIVE_CHAR const *const default_path,
                                      NATIVE_CHAR **const path,
                                      struct ov_error *const err);

NODISCARD bool ovl_dialog_save_file(void *const hwnd,
                                    NATIVE_CHAR const *const title,
                                    NATIVE_CHAR const *const filter,
                                    void const *const client_id,
                                    NATIVE_CHAR const *const default_path,
                                    NATIVE_CHAR **const path,
                                    struct ov_error *const err);

NODISCARD bool ovl_dialog_select_folder(void *const hwnd,
                                        NATIVE_CHAR const *const title,
                                        void const *const guid_client_id,
                                        NATIVE_CHAR const *const default_path,
                                        NATIVE_CHAR **const path,
                                        struct ov_error *const err);
