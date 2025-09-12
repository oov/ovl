#include "tag.h"

#include <ovarray.h>
#include <string.h>

static NODISCARD bool set_string(char **dest, char const *const v, size_t const vlen, struct ov_error *const err) {
  if (!OV_ARRAY_GROW(dest, vlen + 1)) {
    OV_ERROR_SET_GENERIC(err, ov_error_generic_out_of_memory);
    return false;
  }
  memcpy(*dest, v, vlen);
  (*dest)[vlen] = '\0';
  return true;
}

NODISCARD bool ovl_audio_tag_set_title(struct ovl_audio_tag *const tag,
                                       char const *const v,
                                       size_t const vlen,
                                       struct ov_error *const err) {
  if (!tag || !v) {
    OV_ERROR_SET_GENERIC(err, ov_error_generic_invalid_argument);
    return false;
  }

  bool result = false;

  if (!set_string(&tag->title, v, vlen, err)) {
    OV_ERROR_ADD_TRACE(err);
    goto cleanup;
  }

  result = true;

cleanup:
  return result;
}

NODISCARD bool ovl_audio_tag_set_artist(struct ovl_audio_tag *const tag,
                                        char const *const v,
                                        size_t const vlen,
                                        struct ov_error *const err) {
  if (!tag || !v) {
    OV_ERROR_SET_GENERIC(err, ov_error_generic_invalid_argument);
    return false;
  }

  bool result = false;

  if (!set_string(&tag->artist, v, vlen, err)) {
    OV_ERROR_ADD_TRACE(err);
    goto cleanup;
  }

  result = true;

cleanup:
  return result;
}

void ovl_audio_tag_destroy(struct ovl_audio_tag *const tag) {
  if (!tag) {
    return;
  }
  if (tag->title) {
    OV_ARRAY_DESTROY(&tag->title);
  }
  if (tag->artist) {
    OV_ARRAY_DESTROY(&tag->artist);
  }
}
