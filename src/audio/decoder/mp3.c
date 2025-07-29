#include <ovl/audio/decoder/mp3.h>

#include <ovl/audio/decoder.h>
#include <ovl/audio/info.h>
#include <ovl/source.h>

#include "../../i18n.h"
#include "../tag/id3v2.h"
#include "../tag.h"

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
  struct mp3 *ctx = user_data;
  size_t read = ovl_source_read(ctx->source, buf, ctx->source_pos, size);
  if (read == SIZE_MAX) {
    return SIZE_MAX;
  }
  ctx->source_pos += read;
  return read;
}

static int cb_seek(uint64_t position, void *user_data) {
  struct mp3 *ctx = user_data;
  if (position > ctx->source_len) {
    return -1;
  }
  ctx->source_pos = position;
  return 0;
}

static void destroy(struct ovl_audio_decoder **const dp) {
  struct mp3 **const ctxp = (void *)dp;
  if (!ctxp || !*ctxp) {
    return;
  }
  struct mp3 *const ctx = *ctxp;
  mp3dec_ex_close(&ctx->dec);
  if (ctx->pcm) {
    if (ctx->pcm[0]) {
      ereport(mem_aligned_free(&ctx->pcm[0]));
    }
    ereport(mem_free(&ctx->pcm));
  }
  ovl_audio_tag_destroy(&ctx->info.tag);
  ereport(mem_free(ctxp));
}

static struct ovl_audio_info const *get_info(struct ovl_audio_decoder const *const d) {
  struct mp3 const *const ctx = (void const *)d;
  if (!ctx) {
    return NULL;
  }
  return &ctx->info;
}

static NODISCARD error read(struct ovl_audio_decoder *const d, float const *const **const pcm, size_t *const samples) {
  struct mp3 *const ctx = (void *)d;
  if (!ctx || !pcm || !samples) {
    return errg(err_invalid_arugment);
  }
  mp3d_sample_t *buf = NULL;
  mp3dec_frame_info_t frame_info;
  size_t const r = mp3dec_ex_read_frame(&ctx->dec, &buf, &frame_info, chunk_samples);
  if (r == 0 || !buf) {
    *samples = 0;
    return eok();
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
  return eok();
}

static NODISCARD error seek(struct ovl_audio_decoder *const d, uint64_t const position) {
  struct mp3 *const ctx = (void *)d;
  if (!ctx) {
    return errg(err_invalid_arugment);
  }
  if (mp3dec_ex_seek(&ctx->dec, position * (uint64_t)ctx->dec.info.channels)) {
    return emsg_i18n(err_type_generic, err_fail, gettext("Failed to seek"));
  }
  return eok();
}

NODISCARD error ovl_audio_decoder_mp3_create(struct ovl_source *const source, struct ovl_audio_decoder **const dp) {
  if (!dp || *dp || !source) {
    return errg(err_invalid_arugment);
  }
  struct mp3 *ctx = NULL;
  uint8_t *detect_buf = NULL;
  error err = mem(&ctx, 1, sizeof(*ctx));
  if (efailed(err)) {
    err = ethru(err);
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
    err = emsg_i18n(err_type_generic, err_fail, gettext("Failed to get source size"));
    goto cleanup;
  }
  err = mem(&detect_buf, MINIMP3_BUF_SIZE, sizeof(uint8_t));
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  int ret = mp3dec_ex_open_cb(&ctx->dec, &ctx->io, MP3D_SEEK_TO_SAMPLE);
  if (ret) {
    err = emsg_i18nf(err_type_generic, err_fail, NULL, gettext("Failed to open MP3 file.(code:%d)"), ret);
    goto cleanup;
  }
  // If you open an invalid file, it seems that it will return without an error
  // and the initialization will not be correct.
  if (!ctx->dec.info.channels) {
    err = emsg_i18n(err_type_generic, err_fail, gettext("Failed to open MP3 file."));
    goto cleanup;
  }
  ctx->info.samples = ctx->dec.samples / (uint64_t)ctx->dec.info.channels;
  ctx->info.channels = (size_t)ctx->dec.info.channels;
  ctx->info.sample_rate = (size_t)ctx->dec.info.hz;
  size_t const channels = ctx->info.channels;
  err = mem(&ctx->pcm, channels, sizeof(float *));
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  ctx->pcm[0] = NULL;
  err = mem_aligned_alloc(&ctx->pcm[0], chunk_samples * channels, sizeof(float), 16);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  for (size_t ch = 1; ch < channels; ++ch) {
    ctx->pcm[ch] = ctx->pcm[ch - 1] + chunk_samples;
  }
  err = ovl_audio_tag_id3v2_read(
      &ctx->info.tag, source, 0, ctx->source_len > SIZE_MAX ? SIZE_MAX : (size_t)ctx->source_len);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  *dp = (struct ovl_audio_decoder *)ctx;
cleanup:
  if (detect_buf) {
    ereport(mem_free(&detect_buf));
  }
  if (efailed(err)) {
    if (ctx) {
      ovl_audio_decoder_destroy((void *)&ctx);
    }
  }
  return err;
}
