#pragma once

#include <ovbase.h>

enum ovl_file_seek_method {
  ovl_file_seek_method_set,
  ovl_file_seek_method_cur,
  ovl_file_seek_method_end,
};

struct ovl_file;
NODISCARD bool ovl_file_create(NATIVE_CHAR const *const path, struct ovl_file **const file, struct ov_error *const err);
NODISCARD bool ovl_file_open(NATIVE_CHAR const *const path, struct ovl_file **const file, struct ov_error *const err);
void ovl_file_close(struct ovl_file *const file);
NODISCARD bool ovl_file_read(struct ovl_file *const file,
                             void *const buffer,
                             size_t const buffer_bytes,
                             size_t *const read,
                             struct ov_error *const err);
NODISCARD bool ovl_file_write(struct ovl_file *const file,
                              void const *const buffer,
                              size_t const buffer_bytes,
                              size_t *const written,
                              struct ov_error *const err);
NODISCARD bool ovl_file_seek(struct ovl_file *const file,
                             int64_t pos,
                             enum ovl_file_seek_method const method,
                             struct ov_error *const err);
NODISCARD bool ovl_file_tell(struct ovl_file *const file, int64_t *const pos, struct ov_error *const err);
NODISCARD bool ovl_file_size(struct ovl_file *const file, uint64_t *const size, struct ov_error *const err);

NODISCARD bool ovl_file_create_unique(NATIVE_CHAR const *const dir,
                                      NATIVE_CHAR const *const base_name,
                                      struct ovl_file **const file,
                                      NATIVE_CHAR **const created_path,
                                      struct ov_error *const err);
NODISCARD bool ovl_file_create_temp(NATIVE_CHAR const *const base_name,
                                    struct ovl_file **const file,
                                    NATIVE_CHAR **const created_path,
                                    struct ov_error *const err);
