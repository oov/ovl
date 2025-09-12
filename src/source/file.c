#include <ovl/source/file.h>

#include <ovl/file.h>
#include <ovl/source.h>

struct source_file {
  struct ovl_source_vtable const *vtable;
  struct ovl_file *file;
  uint64_t pos, size;
};

static void destroy(struct ovl_source **const sp) {
  struct source_file **const sfp = (struct source_file **)sp;
  if (!sfp || !*sfp) {
    return;
  }
  struct source_file *const sf = *sfp;
  if (sf->file) {
    ovl_file_close(sf->file);
    sf->file = NULL;
  }
  OV_FREE(sp);
}

static size_t read(struct ovl_source *const s, void *const p, uint64_t const offset, size_t const len) {
  struct source_file *const sf = (struct source_file *)s;
  if (!sf || !sf->file || offset > INT64_MAX || len == SIZE_MAX) {
    return SIZE_MAX;
  }
  struct ovl_file *const file = sf->file;
  size_t read_size = 0;
  struct ov_error err = {0};
  bool success = false;
  {
    if (sf->pos != offset) {
      if (!ovl_file_seek(file, (int64_t)offset, ovl_file_seek_method_set, &err)) {
        OV_ERROR_ADD_TRACE(&err);
        goto cleanup;
      }
      sf->pos = offset;
    }
    size_t const real_len = offset + len > sf->size ? (size_t)(sf->size - offset) : len;
    if (real_len == 0) {
      success = true;
      goto cleanup;
    }
    if (!ovl_file_read(file, p, real_len, &read_size, &err)) {
      OV_ERROR_ADD_TRACE(&err);
      goto cleanup;
    }
    sf->pos += read_size;
  }
  success = true;

cleanup:
  if (!success) {
    OV_ERROR_REPORT(&err, NULL);
    return SIZE_MAX;
  }
  return read_size;
}

static uint64_t size(struct ovl_source *const s) {
  struct source_file *const sf = (struct source_file *)s;
  if (!sf || !sf->file) {
    return UINT64_MAX;
  }
  return sf->size;
}

NODISCARD bool
ovl_source_file_create(NATIVE_CHAR const *const path, struct ovl_source **const sp, struct ov_error *const err) {
  if (!path || !sp) {
    OV_ERROR_SET_GENERIC(err, ov_error_generic_invalid_argument);
    return false;
  }
  struct source_file *sf = NULL;
  bool result = false;

  if (!OV_REALLOC(&sf, 1, sizeof(*sf))) {
    OV_ERROR_SET_GENERIC(err, ov_error_generic_out_of_memory);
    goto cleanup;
  }
  static struct ovl_source_vtable const vtable = {
      .destroy = destroy,
      .read = read,
      .size = size,
  };
  *sf = (struct source_file){
      .vtable = &vtable,
  };
  if (!ovl_file_open(path, &sf->file, err)) {
    OV_ERROR_ADD_TRACE(err);
    goto cleanup;
  }
  if (!ovl_file_size(sf->file, &sf->size, err)) {
    OV_ERROR_ADD_TRACE(err);
    goto cleanup;
  }
  *sp = (struct ovl_source *)sf;
  sf = NULL;
  result = true;
cleanup:
  if (sf) {
    destroy((struct ovl_source **)&sf);
  }
  return result;
}
