#pragma once

#include <ovbase.h>

struct ovl_audio_decoder;
struct ovl_source;

/**
 * @brief Creates a new decoder context for an Opus file.
 * @param source The source to read from.
 * @param dp Pointer to a location where the new context will be stored.
 * @return Error code.
 */
NODISCARD error ovl_audio_decoder_opus_create(struct ovl_source *const source, struct ovl_audio_decoder **const dp);
static inline char const *ovl_audio_decoder_opus_get_file_filter(void) { return "*.opus"; }
