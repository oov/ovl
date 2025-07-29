#include "tag.h"

#include <ovarray.h>

static NODISCARD error set_string(char **dest, char const *const v, size_t const vlen) {
  error err = OV_ARRAY_GROW(dest, vlen + 1);
  if (efailed(err)) {
    err = ethru(err);
    return err;
  }
  memcpy(*dest, v, vlen);
  (*dest)[vlen] = '\0';
  return eok();
}

NODISCARD error ovl_audio_tag_set_title(struct ovl_audio_tag *const tag, char const *const v, size_t const vlen) {
  if (!tag || !v) {
    return errg(err_invalid_arugment);
  }
  error err = set_string(&tag->title, v, vlen);
  if (efailed(err)) {
    return ethru(err);
  }
  return eok();
}

NODISCARD error ovl_audio_tag_set_artist(struct ovl_audio_tag *const tag, char const *const v, size_t const vlen) {
  if (!tag || !v) {
    return errg(err_invalid_arugment);
  }
  error err = set_string(&tag->artist, v, vlen);
  if (efailed(err)) {
    return ethru(err);
  }
  return eok();
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
