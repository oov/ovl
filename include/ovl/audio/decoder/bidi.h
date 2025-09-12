#pragma once

#include <ovbase.h>

struct ovl_audio_decoder;

/**
 * @brief Creates a new decoder_bidi context.
 * The decoder_bidi is an implementation that enables reverse playback.
 * @param source The decoder used for playback. The decoder must support
 * accurate seeking. Subsequent operations (e.g., calling read or seek)
 * on the original decoder may result in undefined behavior.
 * Furthermore, since decoder_bidi does not manage the decoder's resources,
 * you should destroy the decoder after the decoder_bidi is destroyed.
 * @param dp Pointer to a location where the new context will be stored.
 * @return Error code.
 */
NODISCARD bool ovl_audio_decoder_bidi_create(struct ovl_audio_decoder *const source,
                                             struct ovl_audio_decoder **const dp,
                                             struct ov_error *const err);

/**
 * @brief Sets the direction of the decoder_bidi.
 * @param d The decoder_bidi context.
 * @param reverse The direction of the decoder.
 * @note This method should only be used on a decoder_bidi instance; calling it
 * on any other instance will have no effect.
 */
void ovl_audio_decoder_bidi_set_direction(struct ovl_audio_decoder *const d, bool const reverse);
