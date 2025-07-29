#include <ovl/audio/decoder/bidi.h>

#include <ovl/audio/decoder.h>
#include <ovl/audio/info.h>

#include "../../i18n.h"

struct bidi {
  struct ovl_audio_decoder_vtable const *vtable;
  struct ovl_audio_decoder *decoder;
  struct ovl_audio_info const *info;
  uint64_t decoder_cursor;
  uint64_t bidi_cursor;

  float const **pcm;

  float **buffer;
  size_t buffer_cap;
  size_t buffer_len;
  size_t buffer_pos;

  bool reverse;
};

static inline size_t adjust_align8(size_t const size) { return (size + (size_t)(7)) & ~(size_t)(7); }
static inline size_t min2(size_t const a, size_t const b) { return a < b ? a : b; }

static void destroy(struct ovl_audio_decoder **const dp) {
  struct bidi **const ctxp = (void *)dp;
  if (!ctxp || !*ctxp) {
    return;
  }
  struct bidi *ctx = *ctxp;
  if (ctx->buffer) {
    ereport(mem_aligned_free(&ctx->buffer[0]));
    ereport(mem_free(&ctx->buffer));
  }
  if (ctx->pcm) {
    ereport(mem_free(&ctx->pcm));
  }
  ereport(mem_free(ctxp));
}

static struct ovl_audio_info const *get_info(struct ovl_audio_decoder const *const d) {
  struct bidi const *const ctx = (void const *)d;
  return ctx->info;
}

static NODISCARD error read_forward(struct bidi *ctx, float const *const **pcm, size_t *samples) {
  error err = eok();
  if (ctx->decoder_cursor != ctx->bidi_cursor) {
    err = ovl_audio_decoder_seek(ctx->decoder, ctx->bidi_cursor);
    if (efailed(err)) {
      err = ethru(err);
      goto cleanup;
    }
    ctx->decoder_cursor = ctx->bidi_cursor;
  }
  size_t read;
  err = ovl_audio_decoder_read(ctx->decoder, pcm, &read);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  ctx->decoder_cursor += read;
  ctx->bidi_cursor += read;
  *samples = read;
cleanup:
  return err;
}

static NODISCARD error fill_reverse_buffer(struct bidi *ctx) {
  size_t const block_size = ctx->bidi_cursor < ctx->buffer_cap ? (size_t)ctx->bidi_cursor : ctx->buffer_cap;
  uint64_t const start = ctx->bidi_cursor - block_size;
  error err = ovl_audio_decoder_seek(ctx->decoder, start);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  ctx->decoder_cursor = start;
  float const *const *pcm;
  size_t read;
  size_t offset = 0;
  while (offset < block_size) {
    err = ovl_audio_decoder_read(ctx->decoder, &pcm, &read);
    if (efailed(err)) {
      err = ethru(err);
      goto cleanup;
    }
    if (read == 0) {
      break;
    }
    ctx->decoder_cursor += read;
    if (read > block_size - offset) {
      read = block_size - offset;
    }
    for (size_t ch = 0; ch < ctx->info->channels; ++ch) {
      for (size_t i = 0; i < read; ++i) {
        ctx->buffer[ch][block_size - 1 - offset - i] = pcm[ch][i];
      }
    }
    offset += read;
  }
  ctx->buffer_len = offset;
  ctx->buffer_pos = 0;
cleanup:
  return err;
}

static NODISCARD error read_reverse(struct bidi *ctx, float const *const **pcm, size_t *samples) {
  error err = eok();
  if (ctx->buffer_pos == ctx->buffer_len) {
    if (ctx->bidi_cursor == 0) {
      *samples = 0;
      goto cleanup;
    }
    err = fill_reverse_buffer(ctx);
    if (efailed(err)) {
      err = ethru(err);
      goto cleanup;
    }
  }
  size_t const r = min2(ctx->info->sample_rate / 10, ctx->buffer_len - ctx->buffer_pos);
  for (size_t ch = 0; ch < ctx->info->channels; ++ch) {
    ctx->pcm[ch] = ctx->buffer[ch] + ctx->buffer_pos;
  }
  ctx->buffer_pos += r;
  ctx->bidi_cursor -= r;
  *pcm = ctx->pcm;
  *samples = r;
cleanup:
  return err;
}

static NODISCARD error read(struct ovl_audio_decoder *const d, float const *const **const pcm, size_t *const samples) {
  struct bidi *const ctx = (void *)d;
  if (!ctx || !pcm || !samples) {
    return errg(err_invalid_arugment);
  }
  if (ctx->reverse) {
    return read_reverse(ctx, pcm, samples);
  } else {
    return read_forward(ctx, pcm, samples);
  }
}

static NODISCARD error seek(struct ovl_audio_decoder *const d, uint64_t const position) {
  struct bidi *const ctx = (void *)d;
  if (!ctx || position > ctx->info->samples) {
    return errg(err_invalid_arugment);
  }
  ctx->bidi_cursor = position;
  ctx->buffer_len = 0;
  ctx->buffer_pos = 0;
  return eok();
}

NODISCARD error ovl_audio_decoder_bidi_create(struct ovl_audio_decoder *const source,
                                              struct ovl_audio_decoder **const dp) {
  if (!dp || *dp || !source) {
    return errg(err_invalid_arugment);
  }
  struct bidi *ctx = NULL;
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
  *ctx = (struct bidi){
      .vtable = &vtable,
      .decoder = source,
      .info = ovl_audio_decoder_get_info(source),
  };
  ctx->buffer_cap = ctx->info->sample_rate; // 1sec

  size_t const channels = ctx->info->channels;
  size_t const buffer_size_aligned = adjust_align8(ctx->info->sample_rate);

  err = mem(&ctx->pcm, channels, sizeof(float *));
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  err = mem(&ctx->buffer, channels, sizeof(float *));
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  ctx->buffer[0] = NULL;
  err = mem_aligned_alloc(&ctx->buffer[0], buffer_size_aligned * channels, sizeof(float), 16);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  for (size_t ch = 1; ch < channels; ++ch) {
    ctx->buffer[ch] = ctx->buffer[ch - 1] + buffer_size_aligned;
  }
  *dp = (void *)ctx;
cleanup:
  if (efailed(err)) {
    if (ctx) {
      destroy((void *)&ctx);
    }
  }
  return err;
}

void ovl_audio_decoder_bidi_set_direction(struct ovl_audio_decoder *const d, bool const reverse) {
  struct bidi *const ctx = (void *)d;
  if (!ctx || !ctx->vtable || ctx->vtable->destroy != destroy || ctx->reverse == reverse) {
    return;
  }
  ctx->buffer_len = 0;
  ctx->buffer_pos = 0;
  ctx->reverse = reverse;
}
