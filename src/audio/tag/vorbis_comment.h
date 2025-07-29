#pragma once

#include <ovbase.h>

struct ovl_audio_tag;

NODISCARD error ovl_audio_tag_vorbis_comment_read(struct ovl_audio_tag *const tag,
                                    size_t const n,
                                    void *userdata,
                                    size_t (*get_entry)(void *userdata, size_t const n, char const **const kv));
