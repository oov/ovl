#pragma once

#include <ovbase.h>

struct ovl_audio_decoder;
struct ovl_source;

/**
 * @brief Creates a new decoder context for a MP3 file.
 * @param source The source to read from.
 * @param dp Pointer to a location where the new context will be stored.
 * @return Error code.
 */
NODISCARD bool ovl_audio_decoder_mp3_create(struct ovl_source *const source,
                                            struct ovl_audio_decoder **const dp,
                                            struct ov_error *const err);
static inline char const *ovl_audio_decoder_mp3_get_file_filter(void) { return "*.mp3"; }
