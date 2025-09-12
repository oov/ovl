#include <ovl/audio/decoder/bidi.h>

#include <ovl/audio/decoder.h>
#include <ovl/audio/info.h>

#include <ovmo.h>

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
  struct bidi **const ctxp = (struct bidi **)(void *)dp;
  if (!ctxp || !*ctxp) {
    return;
  }
  struct bidi *ctx = *ctxp;
  if (ctx->buffer) {
    OV_ALIGNED_FREE(&ctx->buffer[0]);
    OV_FREE(&ctx->buffer);
  }
  if (ctx->pcm) {
    OV_FREE(&ctx->pcm);
  }
  OV_FREE(ctxp);
}

static struct ovl_audio_info const *get_info(struct ovl_audio_decoder const *const d) {
  struct bidi const *const ctx = (struct bidi const *)(void const *)d;
  return ctx->info;
}

static NODISCARD bool
read_forward(struct bidi *ctx, float const *const **pcm, size_t *samples, struct ov_error *const err) {
  bool result = false;

  if (ctx->decoder_cursor != ctx->bidi_cursor) {
    if (!ovl_audio_decoder_seek(ctx->decoder, ctx->bidi_cursor, err)) {
      OV_ERROR_ADD_TRACE(err);
      goto cleanup;
    }
    ctx->decoder_cursor = ctx->bidi_cursor;
  }
  size_t read;
  if (!ovl_audio_decoder_read(ctx->decoder, pcm, &read, err)) {
    OV_ERROR_ADD_TRACE(err);
    goto cleanup;
  }
  ctx->decoder_cursor += read;
  ctx->bidi_cursor += read;
  *samples = read;
  result = true;

cleanup:
  return result;
}

static NODISCARD bool fill_reverse_buffer(struct bidi *ctx, struct ov_error *const err) {
  size_t const block_size = ctx->bidi_cursor < ctx->buffer_cap ? (size_t)ctx->bidi_cursor : ctx->buffer_cap;
  uint64_t const start = ctx->bidi_cursor - block_size;
  bool result = false;
  {
    if (!ovl_audio_decoder_seek(ctx->decoder, start, err)) {
      OV_ERROR_ADD_TRACE(err);
      goto cleanup;
    }
    ctx->decoder_cursor = start;
    float const *const *pcm;
    size_t read;
    size_t offset = 0;
    while (offset < block_size) {
      if (!ovl_audio_decoder_read(ctx->decoder, &pcm, &read, err)) {
        OV_ERROR_ADD_TRACE(err);
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
  }
  result = true;

cleanup:
  return result;
}

static NODISCARD bool
read_reverse(struct bidi *ctx, float const *const **pcm, size_t *samples, struct ov_error *const err) {
  bool result = false;
  {
    if (ctx->buffer_pos == ctx->buffer_len) {
      if (ctx->bidi_cursor == 0) {
        *samples = 0;
        result = true;
        goto cleanup;
      }
      if (!fill_reverse_buffer(ctx, err)) {
        OV_ERROR_ADD_TRACE(err);
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
  }
  result = true;

cleanup:
  return result;
}

static NODISCARD bool read(struct ovl_audio_decoder *const d,
                           float const *const **const pcm,
                           size_t *const samples,
                           struct ov_error *const err) {
  struct bidi *const ctx = (struct bidi *)(void *)d;
  if (!ctx || !pcm || !samples) {
    OV_ERROR_SET_GENERIC(err, ov_error_generic_invalid_argument);
    return false;
  }
  if (ctx->reverse) {
    return read_reverse(ctx, pcm, samples, err);
  } else {
    return read_forward(ctx, pcm, samples, err);
  }
}

static NODISCARD bool seek(struct ovl_audio_decoder *const d, uint64_t const position, struct ov_error *const err) {
  struct bidi *const ctx = (struct bidi *)(void *)d;
  if (!ctx || position > ctx->info->samples) {
    OV_ERROR_SET_GENERIC(err, ov_error_generic_invalid_argument);
    return false;
  }
  ctx->bidi_cursor = position;
  ctx->buffer_len = 0;
  ctx->buffer_pos = 0;
  return true;
}

NODISCARD bool ovl_audio_decoder_bidi_create(struct ovl_audio_decoder *const source,
                                             struct ovl_audio_decoder **const dp,
                                             struct ov_error *const err) {
  if (!dp || *dp || !source) {
    OV_ERROR_SET_GENERIC(err, ov_error_generic_invalid_argument);
    return false;
  }

  struct bidi *ctx = NULL;
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
    *ctx = (struct bidi){
        .vtable = &vtable,
        .decoder = source,
        .info = ovl_audio_decoder_get_info(source),
    };
    ctx->buffer_cap = ctx->info->sample_rate; // 1sec

    size_t const channels = ctx->info->channels;
    size_t const buffer_size_aligned = adjust_align8(ctx->info->sample_rate);

    if (!OV_REALLOC(&ctx->pcm, channels, sizeof(float *))) {
      OV_ERROR_SET_GENERIC(err, ov_error_generic_out_of_memory);
      goto cleanup;
    }
    if (!OV_REALLOC(&ctx->buffer, channels, sizeof(float *))) {
      OV_ERROR_SET_GENERIC(err, ov_error_generic_out_of_memory);
      goto cleanup;
    }
    ctx->buffer[0] = NULL;
    if (!OV_ALIGNED_ALLOC(&ctx->buffer[0], buffer_size_aligned * channels, sizeof(float), 16)) {
      OV_ERROR_SET_GENERIC(err, ov_error_generic_out_of_memory);
      goto cleanup;
    }
    for (size_t ch = 1; ch < channels; ++ch) {
      ctx->buffer[ch] = ctx->buffer[ch - 1] + buffer_size_aligned;
    }
    *dp = (struct ovl_audio_decoder *)(void *)ctx;
  }
  result = true;

cleanup:
  if (!result) {
    if (ctx) {
      destroy((struct ovl_audio_decoder **)(void *)&ctx);
    }
  }
  return result;
}

void ovl_audio_decoder_bidi_set_direction(struct ovl_audio_decoder *const d, bool const reverse) {
  struct bidi *const ctx = (struct bidi *)(void *)d;
  if (!ctx || !ctx->vtable || ctx->vtable->destroy != destroy || ctx->reverse == reverse) {
    return;
  }
  ctx->buffer_len = 0;
  ctx->buffer_pos = 0;
  ctx->reverse = reverse;
}
