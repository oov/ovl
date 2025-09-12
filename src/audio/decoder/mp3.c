#include <ovl/audio/decoder/mp3.h>

#include "../tag.h"
#include "../tag/id3v2.h"

#include <ovl/audio/decoder.h>
#include <ovl/audio/info.h>
#include <ovl/source.h>

#include <ovmo.h>

#ifdef __GNUC__
#  ifndef __has_warning
#    define __has_warning(x) 0
#  endif
#  pragma GCC diagnostic push
#  if __has_warning("-Wimplicit-int-float-conversion")
#    pragma GCC diagnostic ignored "-Wimplicit-int-float-conversion"
#  endif
#  if __has_warning("-Wextra-semi-stmt")
#    pragma GCC diagnostic ignored "-Wextra-semi-stmt"
#  endif
#  if __has_warning("-Wsign-conversion")
#    pragma GCC diagnostic ignored "-Wsign-conversion"
#  endif
#  if __has_warning("-Wreserved-macro-identifier")
#    pragma GCC diagnostic ignored "-Wreserved-macro-identifier"
#  endif
#  if __has_warning("-Wcast-qual")
#    pragma GCC diagnostic ignored "-Wcast-qual"
#  endif
#  if __has_warning("-Wshorten-64-to-32")
#    pragma GCC diagnostic ignored "-Wshorten-64-to-32"
#  endif
#  if __has_warning("-Wshadow")
#    pragma GCC diagnostic ignored "-Wshadow"
#  endif
#  if __has_warning("-Wimplicit-int-conversion")
#    pragma GCC diagnostic ignored "-Wimplicit-int-conversion"
#  endif
#  if __has_warning("-Wdouble-promotion")
#    pragma GCC diagnostic ignored "-Wdouble-promotion"
#  endif
#  if __has_warning("-Wfloat-equal")
#    pragma GCC diagnostic ignored "-Wfloat-equal"
#  endif
#endif // __GNUC__
#define MINIMP3_NO_STDIO
#define MINIMP3_IMPLEMENTATION
#define MINIMP3_FLOAT_OUTPUT
#include <minimp3_ex.h>
#ifdef __GNUC__
#  pragma GCC diagnostic pop
#endif // __GNUC__

enum {
  chunk_samples = MINIMP3_MAX_SAMPLES_PER_FRAME / 2,
};

struct mp3 {
  struct ovl_audio_decoder_vtable const *vtable;
  struct ovl_source *source;
  uint64_t source_pos;
  uint64_t source_len;

  mp3dec_ex_t dec;
  mp3dec_io_t io;

  float **pcm;
  struct ovl_audio_info info;
};

static size_t cb_read(void *buf, size_t size, void *user_data) {
  struct mp3 *ctx = (struct mp3 *)user_data;
  size_t read = ovl_source_read(ctx->source, buf, ctx->source_pos, size);
  if (read == SIZE_MAX) {
    return SIZE_MAX;
  }
  ctx->source_pos += read;
  return read;
}

static int cb_seek(uint64_t position, void *user_data) {
  struct mp3 *ctx = (struct mp3 *)user_data;
  if (position > ctx->source_len) {
    return -1;
  }
  ctx->source_pos = position;
  return 0;
}

static void destroy(struct ovl_audio_decoder **const dp) {
  struct mp3 **const ctxp = (struct mp3 **)(void *)dp;
  if (!ctxp || !*ctxp) {
    return;
  }
  struct mp3 *const ctx = *ctxp;
  mp3dec_ex_close(&ctx->dec);
  if (ctx->pcm) {
    if (ctx->pcm[0]) {
      OV_ALIGNED_FREE(&ctx->pcm[0]);
    }
    OV_FREE(&ctx->pcm);
  }
  ovl_audio_tag_destroy(&ctx->info.tag);
  OV_FREE(ctxp);
}

static struct ovl_audio_info const *get_info(struct ovl_audio_decoder const *const d) {
  struct mp3 const *const ctx = (struct mp3 const *)(void const *)d;
  if (!ctx) {
    return NULL;
  }
  return &ctx->info;
}

static NODISCARD bool read(struct ovl_audio_decoder *const d,
                           float const *const **const pcm,
                           size_t *const samples,
                           struct ov_error *const err) {
  struct mp3 *const ctx = (struct mp3 *)(void *)d;
  if (!ctx || !pcm || !samples) {
    OV_ERROR_SET_GENERIC(err, ov_error_generic_invalid_argument);
    return false;
  }
  mp3d_sample_t *buf = NULL;
  mp3dec_frame_info_t frame_info;
  size_t const r = mp3dec_ex_read_frame(&ctx->dec, &buf, &frame_info, chunk_samples);
  if (r == 0 || !buf) {
    *samples = 0;
    return true;
  }
  size_t const channels = ctx->info.channels;
  size_t const n = r / channels;
  for (size_t i = 0; i < n; i++) {
    for (size_t ch = 0; ch < channels; ch++) {
      ctx->pcm[ch][i] = buf[i * channels + ch];
    }
  }
  *pcm = (float const *const *)ctx->pcm;
  *samples = n;
  return true;
}

static NODISCARD bool seek(struct ovl_audio_decoder *const d, uint64_t const position, struct ov_error *const err) {
  struct mp3 *const ctx = (struct mp3 *)(void *)d;
  if (!ctx) {
    OV_ERROR_SET_GENERIC(err, ov_error_generic_invalid_argument);
    return false;
  }
  if (mp3dec_ex_seek(&ctx->dec, position * (uint64_t)ctx->dec.info.channels)) {
    OV_ERROR_SET(err, ov_error_type_generic, ov_error_generic_fail, gettext("Failed to seek"));
    return false;
  }
  return true;
}

NODISCARD bool ovl_audio_decoder_mp3_create(struct ovl_source *const source,
                                            struct ovl_audio_decoder **const dp,
                                            struct ov_error *const err) {
  if (!dp || *dp || !source) {
    OV_ERROR_SET_GENERIC(err, ov_error_generic_invalid_argument);
    return false;
  }

  struct mp3 *ctx = NULL;
  uint8_t *detect_buf = NULL;
  bool result = false;
  {
    if (!OV_REALLOC(&ctx, 1, sizeof(*ctx))) {
      OV_ERROR_SET_GENERIC(err, ov_error_generic_out_of_memory);
      goto cleanup;
    }
    static struct ovl_audio_decoder_vtable const vtable = {
        .destroy = destroy,
        .get_info = get_info,
        .read = read,
        .seek = seek,
    };
    *ctx = (struct mp3){
        .vtable = &vtable,
        .source = source,
        .source_len = ovl_source_size(source),
        .io =
            {
                .read = cb_read,
                .read_data = ctx,
                .seek = cb_seek,
                .seek_data = ctx,
            },
        .info =
            {
                .tag =
                    {
                        .loop_start = UINT64_MAX,
                        .loop_end = UINT64_MAX,
                        .loop_length = UINT64_MAX,
                    },
            },
    };
    if (ctx->source_len == UINT64_MAX) {
      OV_ERROR_SET(err, ov_error_type_generic, ov_error_generic_fail, gettext("Failed to get source size"));
      goto cleanup;
    }
    if (!OV_REALLOC(&detect_buf, MINIMP3_BUF_SIZE, sizeof(uint8_t))) {
      OV_ERROR_SET_GENERIC(err, ov_error_generic_out_of_memory);
      goto cleanup;
    }
    int ret = mp3dec_ex_open_cb(&ctx->dec, &ctx->io, MP3D_SEEK_TO_SAMPLE);
    if (ret) {
      OV_ERROR_SETF(err,
                    ov_error_type_generic,
                    ov_error_generic_fail,
                    "%1$d",
                    gettext("Failed to open MP3 file.(code:%1$d)"),
                    ret);
      goto cleanup;
    }
    // If you open an invalid file, it seems that it will return without an error
    // and the initialization will not be correct.
    if (!ctx->dec.info.channels) {
      OV_ERROR_SET(err, ov_error_type_generic, ov_error_generic_fail, gettext("Failed to open MP3 file."));
      goto cleanup;
    }
    ctx->info.samples = ctx->dec.samples / (uint64_t)ctx->dec.info.channels;
    ctx->info.channels = (size_t)ctx->dec.info.channels;
    ctx->info.sample_rate = (size_t)ctx->dec.info.hz;
    size_t const channels = ctx->info.channels;
    if (!OV_REALLOC(&ctx->pcm, channels, sizeof(float *))) {
      OV_ERROR_SET_GENERIC(err, ov_error_generic_out_of_memory);
      goto cleanup;
    }
    ctx->pcm[0] = NULL;
    if (!OV_ALIGNED_ALLOC(&ctx->pcm[0], chunk_samples * channels, sizeof(float), 16)) {
      OV_ERROR_SET_GENERIC(err, ov_error_generic_out_of_memory);
      goto cleanup;
    }
    for (size_t ch = 1; ch < channels; ++ch) {
      ctx->pcm[ch] = ctx->pcm[ch - 1] + chunk_samples;
    }
    if (!ovl_audio_tag_id3v2_read(
            &ctx->info.tag, source, 0, ctx->source_len > SIZE_MAX ? SIZE_MAX : (size_t)ctx->source_len, err)) {
      OV_ERROR_ADD_TRACE(err);
      goto cleanup;
    }
    *dp = (struct ovl_audio_decoder *)ctx;
  }
  result = true;

cleanup:
  if (detect_buf) {
    OV_FREE(&detect_buf);
  }
  if (!result) {
    if (ctx) {
      ovl_audio_decoder_destroy((struct ovl_audio_decoder **)(void *)&ctx);
    }
  }
  return result;
}
