#include <ovl/source/file.h>

#include <ovl/file.h>
#include <ovl/source.h>

struct source_file {
  struct ovl_source_vtable const *vtable;
  struct ovl_file *file;
  uint64_t pos, size;
};

static void destroy(struct ovl_source **const sp) {
  struct source_file **const sfp = (void *)sp;
  if (!sfp || !*sfp) {
    return;
  }
  struct source_file *const sf = *sfp;
  if (sf->file) {
    ovl_file_close(sf->file);
    sf->file = NULL;
  }
  ereport(mem_free(sp));
}

static size_t read(struct ovl_source *const s, void *const p, uint64_t const offset, size_t const len) {
  struct source_file *const sf = (void *)s;
  if (!sf || !sf->file || offset > INT64_MAX || len == SIZE_MAX) {
    return SIZE_MAX;
  }
  struct ovl_file *const file = sf->file;
  if (sf->pos != offset) {
    error err = ovl_file_seek(file, (int64_t)offset, ovl_file_seek_method_set);
    if (efailed(err)) {
      ereport(err);
      return SIZE_MAX;
    }
    sf->pos = offset;
  }
  size_t const real_len = offset + len > sf->size ? (size_t)(sf->size - offset) : len;
  if (real_len == 0) {
    return 0;
  }
  size_t read_size = 0;
  error err = ovl_file_read(file, p, real_len, &read_size);
  if (efailed(err)) {
    ereport(err);
    return SIZE_MAX;
  }
  sf->pos += read_size;
  return read_size;
}

static uint64_t size(struct ovl_source *const s) {
  struct source_file *const sf = (void *)s;
  if (!sf || !sf->file) {
    return UINT64_MAX;
  }
  return sf->size;
}

NODISCARD error ovl_source_file_create(NATIVE_CHAR const *const path, struct ovl_source **const sp) {
  if (!path || !sp) {
    return errg(err_invalid_arugment);
  }
  struct source_file *sf = NULL;
  error err = mem(&sf, 1, sizeof(*sf));
  if (efailed(err)) {
    err = ethru(err);
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
  err = ovl_file_open(path, &sf->file);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  uint64_t sz;
  err = ovl_file_size(sf->file, &sz);
  sf->size = sz;
  *sp = (void *)sf;
cleanup:
  if (efailed(err)) {
    if (sf) {
      destroy((void *)&sf);
    }
  }
  return err;
}
