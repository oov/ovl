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
  struct source_memory **const smp = (struct source_memory **)sp;
  if (!smp || !*smp) {
    return;
  }
  OV_FREE(sp);
}

static inline struct source_memory *get_sm(struct ovl_source *const s) {
#ifdef __GNUC__
#  ifndef __has_warning
#    define __has_warning(x) 0
#  endif
#  pragma GCC diagnostic push
#  if __has_warning("-Wcast-align")
#    pragma GCC diagnostic ignored "-Wcast-align"
#  endif
#endif                              // __GNUC__
  return (struct source_memory *)s; // safe
#ifdef __GNUC__
#  pragma GCC diagnostic pop
#endif // __GNUC__
}

static size_t read(struct ovl_source *const s, void *const p, uint64_t const offset, size_t const len) {
  struct source_memory *const sm = get_sm(s);
  if (!sm || !sm->data || offset > sm->size || len == SIZE_MAX) {
    return SIZE_MAX;
  }
  size_t const real_len = offset + len > sm->size ? (size_t)(sm->size - offset) : len;
  if (real_len == 0) {
    return 0;
  }
  memcpy(p, (const char *)sm->data + offset, real_len);
  sm->pos = offset + real_len;
  return real_len;
}

static uint64_t size(struct ovl_source *const s) {
  struct source_memory *const sm = get_sm(s);
  if (!sm || !sm->data) {
    return UINT64_MAX;
  }
  return sm->size;
}

NODISCARD bool ovl_source_memory_create(void const *const ptr,
                                        size_t const sz,
                                        struct ovl_source **const sp,
                                        struct ov_error *const err) {
  if ((!ptr && sz) || !sp) {
    OV_ERROR_SET_GENERIC(err, ov_error_generic_invalid_argument);
    return false;
  }
  struct source_memory *sm = NULL;
  bool result = false;

  if (!OV_REALLOC(&sm, 1, sizeof(*sm))) {
    OV_ERROR_SET_GENERIC(err, ov_error_generic_out_of_memory);
    goto cleanup;
  }
  static struct ovl_source_vtable const vtable = {
      .destroy = destroy,
      .read = read,
      .size = size,
  };
  *sm = (struct source_memory){
      .vtable = &vtable,
      .data = ptr,
      .pos = 0,
      .size = sz,
  };
  *sp = (struct ovl_source *)sm;
  sm = NULL;
  result = true;
cleanup:
  if (sm) {
    destroy((struct ovl_source **)&sm);
  }
  return result;
}
