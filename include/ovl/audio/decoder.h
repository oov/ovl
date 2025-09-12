#pragma once

#include <ovbase.h>

struct ovl_audio_decoder;
struct ovl_audio_info;

/**
 * @brief Vtable for the decoder.
 */
struct ovl_audio_decoder_vtable {
  /**
   * @brief Destroys the codec context.
   * @param dp Pointer to the context pointer to be destroyed.
   */
  void (*destroy)(struct ovl_audio_decoder **const dp);
  /**
   * @brief Retrieves audio information.
   * @param d Pointer to a constant context.
   * @return Pointer to constant decoder_audio_info.
   */
  struct ovl_audio_info const *(*get_info)(struct ovl_audio_decoder const *const d);
  /**
   * @brief Reads audio data.
   * @param d Pointer to the context.
   * @param pcm returns the pointer to the PCM, valid until next read.
   * @param samples returns the number of samples stored in pcm.
   * @param err Error information.
   * @return true on success, false on failure.
   */
  NODISCARD bool (*read)(struct ovl_audio_decoder *const d,
                         float const *const **const pcm,
                         size_t *const samples,
                         struct ov_error *const err);
  /**
   * @brief Seeks to a specific sample position.
   * @param d Pointer to the context.
   * @param position Desired position.
   * @param err Error information.
   * @return true on success, false on failure.
   */
  NODISCARD bool (*seek)(struct ovl_audio_decoder *const d, uint64_t const position, struct ov_error *const err);
};

struct ovl_audio_decoder {
  /**
   * @brief Vtable for the decoder.
   *
   * It is recommended to use the helper functions (e.g., decoder_get_info)
   * instead of directly calling the functions in this vtable.
   */
  struct ovl_audio_decoder_vtable const *vtable;
};

static inline void ovl_audio_decoder_destroy(struct ovl_audio_decoder **const dp) { (*dp)->vtable->destroy(dp); }
static inline struct ovl_audio_info const *ovl_audio_decoder_get_info(struct ovl_audio_decoder const *const d) {
  return d->vtable->get_info(d);
}
static inline NODISCARD bool ovl_audio_decoder_read(struct ovl_audio_decoder *const d,
                                                    float const *const **const pcm,
                                                    size_t *const samples,
                                                    struct ov_error *const err) {
  return d->vtable->read(d, pcm, samples, err);
}
static inline NODISCARD bool
ovl_audio_decoder_seek(struct ovl_audio_decoder *const d, uint64_t const position, struct ov_error *const err) {
  return d->vtable->seek(d, position, err);
}
