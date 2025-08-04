#include <ovl/source/memory.h>

#include <ovl/source.h>

#include <string.h>

struct source_memory {
  struct ovl_source_vtable const *vtable;
  const void *data;
  uint64_t pos;
  uint64_t size;
};

static void destroy(struct ovl_source **const sp) {
  struct source_memory **const sfp = (void *)sp;
  if (!sfp || !*sfp) {
    return;
  }
  ereport(mem_free(sp));
}

static size_t read(struct ovl_source *const s, void *const p, uint64_t const offset, size_t const len) {
  struct source_memory *const sf = (void *)s;
  if (!sf || !sf->data || offset > sf->size || len == SIZE_MAX) {
    return SIZE_MAX;
  }
  size_t const real_len = offset + len > sf->size ? (size_t)(sf->size - offset) : len;
  if (real_len == 0) {
    return 0;
  }
  memcpy(p, (const char *)sf->data + offset, real_len);
  sf->pos = offset + real_len;
  return real_len;
}

static uint64_t size(struct ovl_source *const s) {
  struct source_memory *const sf = (void *)s;
  if (!sf || !sf->data) {
    return UINT64_MAX;
  }
  return sf->size;
}

NODISCARD error ovl_source_memory_create(void const *const ptr, size_t const sz, struct ovl_source **const sp) {
  if ((!ptr && sz) || !sp) {
    return errg(err_invalid_arugment);
  }
  struct source_memory *sf = NULL;
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
  *sf = (struct source_memory){
      .vtable = &vtable,
      .data = ptr,
      .pos = 0,
      .size = sz,
  };
  *sp = (void *)sf;
  sf = NULL;
cleanup:
  if (sf) {
    destroy((void *)&sf);
  }
  return err;
}
