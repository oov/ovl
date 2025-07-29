#pragma once

#include <ovbase.h>

struct ovl_audio_decoder;
struct ovl_source;

/**
 * @brief Creates a new decoder context for a Wave file.
 * This function supports uncompressed Wave, RF64, Wave64, and AIFF.
 * @param source The source to read from.
 * @param dp Pointer to a location where the new context will be stored.
 * @return Error code.
 */
NODISCARD error ovl_audio_decoder_wav_create(struct ovl_source *const source, struct ovl_audio_decoder **const dp);
static inline char const *ovl_audio_decoder_wav_get_file_filter(void) { return "*.wav;*.w64;*.aif;*.aiff"; }
