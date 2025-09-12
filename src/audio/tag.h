#pragma once

#include <ovl/audio/tag.h>

NODISCARD bool ovl_audio_tag_set_title(struct ovl_audio_tag *const tag,
                                       char const *const v,
                                       size_t const vlen,
                                       struct ov_error *const err);
NODISCARD bool ovl_audio_tag_set_artist(struct ovl_audio_tag *const tag,
                                        char const *const v,
                                        size_t const vlen,
                                        struct ov_error *const err);
void ovl_audio_tag_destroy(struct ovl_audio_tag *const tag);
