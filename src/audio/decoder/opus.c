#include <ovl/audio/decoder/opus.h>

#include <ovl/audio/decoder.h>
#include <ovl/audio/info.h>
#include <ovl/source.h>

#include "../../i18n.h"
#include "../tag.h"
#include "../tag/vorbis_comment.h"

#ifdef __GNUC__
#  ifndef __has_warning
#    define __has_warning(x) 0
#  endif
#  pragma GCC diagnostic push
#  if __has_warning("-Wreserved-macro-identifier")
#    pragma GCC diagnostic ignored "-Wreserved-macro-identifier"
#  endif
#endif // __GNUC__
#include <opusfile.h>
#ifdef __GNUC__
#  pragma GCC diagnostic pop
#endif // __GNUC__

enum {
  // Recommended number of samples in op_read function comment
  chunk_samples = 48000 * 120 / 1000,
};

struct opus {
  struct ovl_audio_decoder_vtable const *vtable;
  struct ovl_source *source;
  uint64_t source_pos;
  uint64_t source_len;

  OggOpusFile *of;
  struct ovl_audio_info info;

  float **pcm;
  int16_t *buf;
};

static int cb_read(void *const stream, unsigned char *const ptr, int const nbytes) {
  struct opus *ctx = stream;
  size_t read = ovl_source_read(ctx->source, ptr, ctx->source_pos, (size_t)nbytes);
  if (read == SIZE_MAX) {
    return -1;
  }
  ctx->source_pos += read;
  return (int)read;
}

static int cb_seek(void *const stream, opus_int64 const offset, int const whence) {
  struct opus *ctx = stream;
  if (whence == SEEK_SET) {
    if (offset < 0 || (uint64_t)offset > ctx->source_len) {
      return -1;
    }
    ctx->source_pos = (uint64_t)offset;
    return 0;
  }
  if (whence != SEEK_CUR && whence != SEEK_END) {
    return -1;
  }
  uint64_t const base = whence == SEEK_CUR ? ctx->source_pos : ctx->source_len;
  if (offset >= 0) {
    uint64_t const pos = base + (uint64_t)offset;
    if (pos > ctx->source_len || pos < base) {
      return -1;
    }
    ctx->source_pos = pos;
    return 0;
  }
  uint64_t const abs_offset = (uint64_t)(~offset) + UINT64_C(1);
  if (abs_offset > base) {
    return -1;
  }
  ctx->source_pos = base - abs_offset;
  return 0;
}

static opus_int64 cb_tell(void *const stream) {
  struct opus *ctx = stream;
  if (ctx->source_len > (uint64_t)INT64_MAX || ctx->source_pos > ctx->source_len) {
    return -1;
  }
  return (opus_int64)ctx->source_pos;
}

struct get_entry_ctx {
  OpusTags const *vc;
};

static size_t get_entry(void *const userdata, size_t const n, char const **const entry) {
  struct get_entry_ctx *const ctx = userdata;
  if (n >= (size_t)ctx->vc->comments) {
    return 0;
  }
  *entry = ctx->vc->user_comments[n];
  return (size_t)ctx->vc->comment_lengths[n];
}

static void destroy(struct ovl_audio_decoder **const dp) {
  struct opus **const ctxp = (void *)dp;
  if (!ctxp || !*ctxp) {
    return;
  }
  struct opus *const ctx = *ctxp;
  if (ctx->of) {
    op_free(ctx->of);
  }
  ovl_audio_tag_destroy(&ctx->info.tag);
  if (ctx->buf) {
    ereport(mem_aligned_free(&ctx->buf));
  }
  if (ctx->pcm) {
    if (ctx->pcm[0]) {
      ereport(mem_aligned_free(&ctx->pcm[0]));
    }
    ereport(mem_free(&ctx->pcm));
  }
  ereport(mem_free(dp));
}

static struct ovl_audio_info const *get_info(struct ovl_audio_decoder const *const d) {
  struct opus const *const ctx = (void const *)d;
  if (!ctx) {
    return NULL;
  }
  return &ctx->info;
}

#if defined(__GNUC__) || defined(__clang__)
#  define ASSUME_ALIGNED_16(ptr) __builtin_assume_aligned((ptr), 16)
#else
#  define ASSUME_ALIGNED_16(ptr) (ptr)
#endif

static inline float conv_int16_to_float(int16_t const v) {
  static float const inv_scale = 1.f / 32768.f;
  return (float)v * inv_scale;
}

static inline void int16_to_float_1ch(int16_t const *const src, float *const *const dst, size_t const samples) {
  int16_t const *const s = ASSUME_ALIGNED_16(src);
  float *const d = ASSUME_ALIGNED_16(dst[0]);
  for (size_t i = 0; i < samples; ++i) {
    d[i] = conv_int16_to_float(s[i]);
  }
}

static inline void int16_to_float_2ch(int16_t const *const src, float *const *const dst, size_t const samples) {
  int16_t const *const s = ASSUME_ALIGNED_16(src);
  float *const dl = ASSUME_ALIGNED_16(dst[0]);
  float *const dr = ASSUME_ALIGNED_16(dst[1]);
  for (size_t i = 0; i < samples; ++i) {
    dl[i] = conv_int16_to_float(s[i * 2]);
    dr[i] = conv_int16_to_float(s[i * 2 + 1]);
  }
}

static inline void
int16_to_float_multi(int16_t const *const src, float *const *const dst, size_t const channels, size_t const samples) {
  int16_t const *const s = ASSUME_ALIGNED_16(src);
  for (size_t i = 0; i < samples; i++) {
    for (size_t c = 0; c < channels; c++) {
      dst[c][i] = conv_int16_to_float(*(s + i * channels + c));
    }
  }
}

static inline void
int16_to_float(int16_t const *const src, float *const *const dst, size_t const channels, size_t const samples) {
  if (channels == 1) {
    int16_to_float_1ch(src, dst, samples);
  } else if (channels == 2) {
    int16_to_float_2ch(src, dst, samples);
  } else {
    int16_to_float_multi(src, dst, channels, samples);
  }
}

static NODISCARD error read(struct ovl_audio_decoder *const d, float const *const **const pcm, size_t *const samples) {
  struct opus *const ctx = (void *)d;
  if (!ctx || !pcm || !samples) {
    return errg(err_invalid_arugment);
  }
  // OpusFile has op_read_float that can get data in float format,
  // but this function internally converts data obtained in int16 to float.
  // In this program, conversion from interleaved format is also required,
  // so it is more efficient to convert to float at that timing.
  int const r = op_read(ctx->of, ctx->buf, (int)(chunk_samples * ctx->info.channels), NULL);
  if (r < 0) {
    return emsg_i18nf(err_type_generic, err_fail, NULL, gettext("Failed to read samples.(code:%d)"), r);
  }
  int16_to_float(ctx->buf, ctx->pcm, ctx->info.channels, (size_t)r);
  *pcm = ov_deconster_(ctx->pcm);
  *samples = (size_t)r;
  return eok();
}

static NODISCARD error seek(struct ovl_audio_decoder *const d, uint64_t const position) {
  struct opus *const ctx = (void *)d;
  if (!ctx) {
    return errg(err_invalid_arugment);
  }
  int const ret = op_pcm_seek(ctx->of, (ogg_int64_t)position);
  if (ret != 0) {
    return emsg_i18nf(err_type_generic, err_fail, NULL, gettext("Failed to seek.(code:%d)"), ret);
  }
  return eok();
}

NODISCARD error ovl_audio_decoder_opus_create(struct ovl_source *const source, struct ovl_audio_decoder **const dp) {
  if (!dp || !source) {
    return errg(err_invalid_arugment);
  }
  struct opus *ctx = NULL;
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
  *ctx = (struct opus){
      .vtable = &vtable,
      .source = source,
      .source_len = ovl_source_size(source),
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
  int err_code = 0;
  ctx->of = op_open_callbacks(ctx,
                              (&(OpusFileCallbacks){
                                  .read = cb_read,
                                  .seek = cb_seek,
                                  .tell = cb_tell,
                                  .close = NULL,
                              }),
                              NULL,
                              0,
                              &err_code);
  if (!ctx->of) {
    err = emsg_i18nf(err_type_generic, err_fail, NULL, gettext("Failed to Opus file.(code:%d)"), err_code);
    goto cleanup;
  }
  int const channels = op_channel_count(ctx->of, -1);
  if (channels < 0) {
    err = emsg_i18n(err_type_generic, err_fail, gettext("Failed to get channel count."));
    goto cleanup;
  }
  ctx->info.channels = (size_t)channels;
  ctx->info.sample_rate = 48000; // Opus is always 48kHz

  ogg_int64_t const total = op_pcm_total(ctx->of, -1);
  if (total < 0) {
    err = emsg_i18n(err_type_generic, err_fail, gettext("Failed to get total samples."));
    goto cleanup;
  }
  ctx->info.samples = (uint64_t)total;

  OpusTags const *const vc = op_tags(ctx->of, -1);
  if (vc) {
    err =
        ovl_audio_tag_vorbis_comment_read(&ctx->info.tag, (size_t)vc->comments, &(struct get_entry_ctx){vc}, get_entry);
    if (efailed(err)) {
      err = ethru(err);
      goto cleanup;
    }
  }

  err = mem(&ctx->pcm, ctx->info.channels, sizeof(float *));
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  ctx->pcm[0] = NULL;
  err = mem_aligned_alloc(&ctx->pcm[0], chunk_samples * ctx->info.channels, sizeof(float), 16);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  for (size_t ch = 1; ch < ctx->info.channels; ++ch) {
    ctx->pcm[ch] = ctx->pcm[0] + chunk_samples * ch;
  }
  err = mem_aligned_alloc(&ctx->buf, chunk_samples * ctx->info.channels, sizeof(int16_t), 16);
  *dp = (void *)ctx;
  ctx = NULL;
cleanup:
  if (efailed(err)) {
    if (ctx) {
      destroy((void *)&ctx);
    }
  }
  return err;
}
