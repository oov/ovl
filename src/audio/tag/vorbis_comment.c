#include "vorbis_comment.h"

#include <string.h>

#include "../tag.h"

static bool atou(char const *const s, size_t const slen, uint64_t *const dest) {
  if (!s) {
    return false;
  }
  uint64_t v = 0;
  for (size_t i = 0; i < slen; ++i) {
    if (s[i] < '0' || s[i] > '9') {
      return false;
    }
    uint64_t const ov = v;
    v = ov * 10 + (uint64_t)(s[i] - '0');
    if (v < ov) {
      return false;
    }
  }
  *dest = v;
  return true;
}

NODISCARD bool
ovl_audio_tag_vorbis_comment_read(struct ovl_audio_tag *const tag,
                                  size_t const n,
                                  void *userdata,
                                  size_t (*get_entry)(void *userdata, size_t const n, char const **const entry),
                                  struct ov_error *const err) {
  if (!tag || !userdata || !get_entry) {
    OV_ERROR_SET_GENERIC(err, ov_error_generic_invalid_argument);
    return false;
  }

  for (size_t i = 0; i < n; ++i) {
    char const *entry = NULL;
    size_t const entry_len = get_entry(userdata, i, &entry);
    if (!entry) {
      continue;
    }
    char const *const eq = (char const *)memchr(entry, '=', entry_len);
    if (!eq) {
      continue;
    }
    size_t const key_len = (size_t)(eq - entry);
    size_t const value_len = entry_len - key_len - 1;
    char const *const value = eq + 1;
    uint64_t v;
    struct ov_error err2 = {0};
    if (key_len == 5 && strncmp(entry, "TITLE", 5) == 0) {
      if (!ovl_audio_tag_set_title(tag, value, value_len, &err2)) {
        OV_ERROR_REPORT(&err2, NULL);
      }
    } else if (key_len == 6 && strncmp(entry, "ARTIST", 6) == 0) {
      if (!ovl_audio_tag_set_artist(tag, value, value_len, &err2)) {
        OV_ERROR_REPORT(&err2, NULL);
      }
    } else if (key_len == 9 && strncmp(entry, "LOOPSTART", 9) == 0) {
      if (atou(value, value_len, &v)) {
        tag->loop_start = v;
      }
    } else if (key_len == 7 && strncmp(entry, "LOOPEND", 7) == 0) {
      if (atou(value, value_len, &v)) {
        tag->loop_end = v;
      }
    } else if (key_len == 10 && strncmp(entry, "LOOPLENGTH", 10) == 0) {
      if (atou(value, value_len, &v)) {
        tag->loop_length = v;
      }
    }
  }
  return true;
}
