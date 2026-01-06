#pragma once

#include <ovbase.h>

/**
 * @brief Opens a file selection dialog
 *
 * Displays a system file open dialog to allow the user to select a file.
 * The selected file path is returned via the path output parameter.
 *
 * @param hwnd Parent window handle (HWND on Windows), can be NULL
 * @param title Dialog title text
 * @param filter File type filter in null-separated pairs format (e.g., L"Text Files\0*.txt\0All Files\0*.*\0\0")
 * @param guid_client_id GUID to persist dialog state, can be NULL
 * @param default_path Default file path or filename to display
 * @param path Output parameter for the selected file path (allocated via OV_ARRAY_GROW, caller must free with
 * OV_ARRAY_DESTROY)
 * @param err Error information output
 * @return bool true on success, false on failure or cancellation
 */
NODISCARD bool ovl_dialog_select_file(void *const hwnd,
                                      NATIVE_CHAR const *const title,
                                      NATIVE_CHAR const *const filter,
                                      void const *const guid_client_id,
                                      NATIVE_CHAR const *const default_path,
                                      NATIVE_CHAR **const path,
                                      struct ov_error *const err);

/**
 * @brief Opens a file save dialog
 *
 * Displays a system file save dialog to allow the user to specify a file path for saving.
 * The specified file path is returned via the path output parameter.
 *
 * @param hwnd Parent window handle (HWND on Windows), can be NULL
 * @param title Dialog title text
 * @param filter File type filter in null-separated pairs format (e.g., L"Text Files\0*.txt\0All Files\0*.*\0\0")
 * @param client_id GUID to persist dialog state, can be NULL
 * @param default_path Default file path or filename to display
 * @param default_extension Default file extension to append if user doesn't specify one (without leading dot, e.g.,
 * L"txt"), can be NULL
 * @param path Output parameter for the specified file path (allocated via OV_ARRAY_GROW, caller must free with
 * OV_ARRAY_DESTROY)
 * @param err Error information output
 * @return bool true on success, false on failure or cancellation
 */
NODISCARD bool ovl_dialog_save_file(void *const hwnd,
                                    NATIVE_CHAR const *const title,
                                    NATIVE_CHAR const *const filter,
                                    void const *const client_id,
                                    NATIVE_CHAR const *const default_path,
                                    NATIVE_CHAR const *const default_extension,
                                    NATIVE_CHAR **const path,
                                    struct ov_error *const err);

/**
 * @brief Opens a folder selection dialog
 *
 * Displays a system folder selection dialog to allow the user to select a folder.
 * The selected folder path is returned via the path output parameter.
 *
 * @param hwnd Parent window handle (HWND on Windows), can be NULL
 * @param title Dialog title text
 * @param guid_client_id GUID to persist dialog state, can be NULL
 * @param default_path Default folder path to display
 * @param path Output parameter for the selected folder path (allocated via OV_ARRAY_GROW, caller must free with
 * OV_ARRAY_DESTROY)
 * @param err Error information output
 * @return bool true on success, false on failure or cancellation
 */
NODISCARD bool ovl_dialog_select_folder(void *const hwnd,
                                        NATIVE_CHAR const *const title,
                                        void const *const guid_client_id,
                                        NATIVE_CHAR const *const default_path,
                                        NATIVE_CHAR **const path,
                                        struct ov_error *const err);
