#pragma once

#include <ovl/audio/tag.h>

NODISCARD error ovl_audio_tag_set_title(struct ovl_audio_tag *const tag, char const *const v, size_t const vlen);
NODISCARD error ovl_audio_tag_set_artist(struct ovl_audio_tag *const tag, char const *const v, size_t const vlen);
void ovl_audio_tag_destroy(struct ovl_audio_tag *const tag);
