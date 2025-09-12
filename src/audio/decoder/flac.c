#include <ovl/audio/decoder/flac.h>

#include "../tag.h"
#include "../tag/vorbis_comment.h"

#include <ovl/audio/decoder.h>
#include <ovl/audio/info.h>
#include <ovl/source.h>

#include <ovmo.h>

#ifdef __GNUC__
#  ifndef __has_warning
#    define __has_warning(x) 0
#  endif
#  pragma GCC diagnostic push
#  if __has_warning("-Wdocumentation-unknown-command")
#    pragma GCC diagnostic ignored "-Wdocumentation-unknown-command"
#  endif
#  if __has_warning("-Wdocumentation")
#    pragma GCC diagnostic ignored "-Wdocumentation"
#  endif
#endif // __GNUC__
#include <FLAC/stream_decoder.h>
#ifdef __GNUC__
#  pragma GCC diagnostic pop
#endif // __GNUC__

#if defined(__GNUC__) || defined(__clang__)
#  define ASSUME_ALIGNED_16(ptr) __builtin_assume_aligned((ptr), 16)
#else
#  define ASSUME_ALIGNED_16(ptr) (ptr)
#endif

static void convert_samples_1ch(int32_t const *const *const src,
                                size_t const src_offset,
                                float *const *const dst,
                                size_t const samples,
                                float const scale) {
  int32_t const *const s = src[0] + src_offset;
  float *const d = (float *)ASSUME_ALIGNED_16(dst[0]);
  for (size_t i = 0; i < samples; ++i) {
    d[i] = (float)s[i] * scale;
  }
}
static void convert_samples_2ch(int32_t const *const *const src,
                                size_t const src_offset,
                                float *const *const dst,
                                size_t const samples,
                                float const scale) {
  int32_t const *const sl = src[0] + src_offset;
  int32_t const *const sr = src[1] + src_offset;
  float *const l = (float *)ASSUME_ALIGNED_16(dst[0]);
  float *const r = (float *)ASSUME_ALIGNED_16(dst[1]);
  for (size_t i = 0; i < samples; ++i) {
    l[i] = (float)sl[i] * scale;
    r[i] = (float)sr[i] * scale;
  }
}
static void convert_samples_multi(int32_t const *const *const src,
                                  size_t const src_offset,
                                  float *const *const dst,
                                  size_t const channels,
                                  size_t const samples,
                                  float const scale) {
  for (size_t c = 0; c < channels; ++c) {
    float *const aligned_d = (float *)ASSUME_ALIGNED_16(dst[c]);
    for (size_t i = 0; i < samples; ++i) {
      aligned_d[i] = (float)src[c][src_offset + i] * scale;
    }
  }
}
static void convert_samples(int32_t const *const *const src,
                            size_t const src_offset,
                            float *const *const dst,
                            size_t const channels,
                            size_t const samples,
                            uint32_t bits_per_sample) {
  float const scale = 1.f / (float)(1u << (bits_per_sample - 1));
  if (channels == 1) {
    convert_samples_1ch(src, src_offset, dst, samples, scale);
  } else if (channels == 2) {
    convert_samples_2ch(src, src_offset, dst, samples, scale);
  } else {
    convert_samples_multi(src, src_offset, dst, channels, samples, scale);
  }
}

struct flac {
  struct ovl_audio_decoder_vtable const *vtable;
  struct ovl_source *source;
  uint64_t source_pos;
  uint64_t source_len;

  FLAC__StreamDecoder *decoder;
  struct ovl_audio_info info;

  float **buffer;
  size_t buffer_len;
  size_t buffer_cap;
};

static FLAC__StreamDecoderReadStatus
read_callback(FLAC__StreamDecoder const *const decoder, FLAC__byte buffer[], size_t *bytes, void *client_data) {
  (void)decoder;
  struct flac *const ctx = (struct flac *)client_data;
  if (ctx->source_pos >= ctx->source_len) {
    return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
  }
  size_t const read = ovl_source_read(ctx->source, buffer, ctx->source_pos, *bytes);
  if (read == SIZE_MAX) {
    return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
  }
  ctx->source_pos += read;
  *bytes = read;
  return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

static FLAC__StreamDecoderSeekStatus
seek_callback(FLAC__StreamDecoder const *const decoder, FLAC__uint64 absolute_byte_offset, void *client_data) {
  (void)decoder;
  struct flac *const ctx = (struct flac *)client_data;
  if (absolute_byte_offset > ctx->source_len) {
    return FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
  }
  ctx->source_pos = absolute_byte_offset;
  return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
}

static FLAC__StreamDecoderTellStatus
tell_callback(FLAC__StreamDecoder const *const decoder, FLAC__uint64 *absolute_byte_offset, void *client_data) {
  (void)decoder;
  struct flac *const ctx = (struct flac *)client_data;
  *absolute_byte_offset = ctx->source_pos;
  return FLAC__STREAM_DECODER_TELL_STATUS_OK;
}

static FLAC__StreamDecoderLengthStatus
length_callback(FLAC__StreamDecoder const *const decoder, FLAC__uint64 *stream_length, void *client_data) {
  (void)decoder;
  struct flac *const ctx = (struct flac *)client_data;
  *stream_length = ctx->source_len;
  return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
}

static FLAC__bool eof_callback(FLAC__StreamDecoder const *const decoder, void *client_data) {
  (void)decoder;
  struct flac *const ctx = (struct flac *)client_data;
  return ctx->source_pos >= ctx->source_len;
}

static void
error_callback(FLAC__StreamDecoder const *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data) {
  (void)decoder;
  (void)status;
  (void)client_data;
}

struct get_entry_ctx {
  FLAC__StreamMetadata_VorbisComment const *vc;
};

static size_t get_entry(void *const userdata, size_t const n, char const **const entry) {
  struct get_entry_ctx *ctx = (struct get_entry_ctx *)userdata;
  if (n >= (size_t)ctx->vc->comments) {
    return 0;
  }
  *entry = (char const *)(void *)ctx->vc->comments[n].entry;
  return (size_t)ctx->vc->comments[n].length;
}

static void metadata_callback(FLAC__StreamDecoder const *const decoder,
                              FLAC__StreamMetadata const *const metadata,
                              void *const client_data) {
  (void)decoder;
  struct flac *const ctx = (struct flac *)client_data;
  if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
    ctx->info.channels = metadata->data.stream_info.channels;
    ctx->info.sample_rate = metadata->data.stream_info.sample_rate;
    ctx->info.samples = metadata->data.stream_info.total_samples;
    return;
  }
  if (metadata->type == FLAC__METADATA_TYPE_VORBIS_COMMENT) {
    FLAC__StreamMetadata_VorbisComment const *vc = &metadata->data.vorbis_comment;
    struct ov_error err = {0};
    if (!ovl_audio_tag_vorbis_comment_read(
            &ctx->info.tag, (size_t)vc->num_comments, &(struct get_entry_ctx){vc}, get_entry, &err)) {
      OV_ERROR_REPORT(&err, NULL);
    }
    return;
  }
}

static FLAC__StreamDecoderWriteStatus write_callback(FLAC__StreamDecoder const *const decoder,
                                                     FLAC__Frame const *const frame,
                                                     FLAC__int32 const *const buffer[],
                                                     void *const client_data) {
  (void)decoder;
  struct flac *const ctx = (struct flac *)client_data;
  if (!ctx || !ctx->buffer) {
    return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
  }
  if (frame->header.channels != ctx->info.channels) {
    return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
  }
  if (frame->header.blocksize > ctx->buffer_cap) {
    size_t const align = 16 / sizeof(float);
    size_t const aligned_samples = (frame->header.blocksize + (align - 1)) & ~(align - 1);
    float *new_buffer = NULL;
    if (!OV_ALIGNED_ALLOC(&new_buffer, aligned_samples * ctx->info.channels, sizeof(float), 16)) {
      struct ov_error err = {0};
      OV_ERROR_SET_GENERIC(&err, ov_error_generic_out_of_memory);
      OV_ERROR_REPORT(&err, NULL);
      return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
    }
    if (ctx->buffer[0]) {
      OV_ALIGNED_FREE(&ctx->buffer[0]);
    }
    for (size_t i = 0; i < ctx->info.channels; ++i) {
      ctx->buffer[i] = new_buffer + aligned_samples * i;
    }
    ctx->buffer_cap = aligned_samples;
  }
  size_t const frame_size = frame->header.blocksize;
  convert_samples(buffer, 0, ctx->buffer, frame->header.channels, frame_size, frame->header.bits_per_sample);
  ctx->buffer_len = frame_size;
  return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

static void destroy(struct ovl_audio_decoder **const dp) {
  struct flac **const ctxp = (struct flac **)(void *)dp;
  if (!ctxp || !*ctxp) {
    return;
  }
  struct flac *const ctx = *ctxp;
  if (ctx->decoder) {
    FLAC__stream_decoder_delete(ctx->decoder);
  }
  ovl_audio_tag_destroy(&ctx->info.tag);
  if (ctx->buffer) {
    if (ctx->buffer[0]) {
      OV_ALIGNED_FREE(&ctx->buffer[0]);
    }
    OV_FREE(&ctx->buffer);
  }
  OV_FREE(ctxp);
}

static struct ovl_audio_info const *get_info(struct ovl_audio_decoder const *const d) {
  struct flac const *const ctx = (struct flac const *)(void const *)d;
  if (!ctx) {
    return NULL;
  }
  return &ctx->info;
}

static NODISCARD bool read(struct ovl_audio_decoder *const d,
                           float const *const **const pcm,
                           size_t *const samples,
                           struct ov_error *const err) {
  struct flac *const ctx = (struct flac *)(void *)d;
  if (!ctx || !pcm || !samples || !ctx->decoder || !ctx->buffer) {
    OV_ERROR_SET_GENERIC(err, ov_error_generic_invalid_argument);
    return false;
  }
  *pcm = (float const **)ov_deconster_(ctx->buffer);
  if (ctx->buffer_len > 0) {
    *samples = ctx->buffer_len;
    ctx->buffer_len = 0;
    return true;
  }
  FLAC__StreamDecoderState const state = FLAC__stream_decoder_get_state(ctx->decoder);
  if (state == FLAC__STREAM_DECODER_END_OF_STREAM) {
    *samples = 0;
    return true;
  }
  if (!FLAC__stream_decoder_process_single(ctx->decoder)) {
    OV_ERROR_SET(err, ov_error_type_generic, ov_error_generic_fail, gettext("Failed to decode FLAC frame"));
    return false;
  }
  *samples = ctx->buffer_len;
  ctx->buffer_len = 0;
  return true;
}

static NODISCARD bool seek(struct ovl_audio_decoder *const d, uint64_t const position, struct ov_error *const err) {
  struct flac *const ctx = (struct flac *)(void *)d;
  if (!ctx) {
    OV_ERROR_SET_GENERIC(err, ov_error_generic_invalid_argument);
    return false;
  }
  if (!FLAC__stream_decoder_seek_absolute(ctx->decoder, position)) {
    OV_ERROR_SET(err, ov_error_type_generic, ov_error_generic_fail, gettext("Failed to seek"));
    return false;
  }
  return true;
}

NODISCARD bool ovl_audio_decoder_flac_create(struct ovl_source *const source,
                                             struct ovl_audio_decoder **const dp,
                                             struct ov_error *const err) {
  if (!dp || *dp || !source) {
    OV_ERROR_SET_GENERIC(err, ov_error_generic_invalid_argument);
    return false;
  }

  struct flac *ctx = NULL;
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
    *ctx = (struct flac){
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
      OV_ERROR_SET(err, ov_error_type_generic, ov_error_generic_fail, gettext("Failed to get source size"));
      goto cleanup;
    }
    ctx->decoder = FLAC__stream_decoder_new();
    if (!ctx->decoder) {
      OV_ERROR_SET(err, ov_error_type_generic, ov_error_generic_fail, gettext("Failed to allocate flac decoder"));
      goto cleanup;
    }
    FLAC__stream_decoder_set_metadata_respond(ctx->decoder, FLAC__METADATA_TYPE_VORBIS_COMMENT);
    FLAC__StreamDecoderInitStatus const init_status = FLAC__stream_decoder_init_stream(ctx->decoder,
                                                                                       read_callback,
                                                                                       seek_callback,
                                                                                       tell_callback,
                                                                                       length_callback,
                                                                                       eof_callback,
                                                                                       write_callback,
                                                                                       metadata_callback,
                                                                                       error_callback,
                                                                                       ctx);
    if (init_status != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
      OV_ERROR_SET(err, ov_error_type_generic, ov_error_generic_fail, gettext("Failed to initialize FLAC decoder"));
      goto cleanup;
    }
    if (!FLAC__stream_decoder_process_until_end_of_metadata(ctx->decoder)) {
      OV_ERROR_SET(err, ov_error_type_generic, ov_error_generic_fail, gettext("Failed to process FLAC metadata"));
      goto cleanup;
    }
    if (!OV_REALLOC(&ctx->buffer, ctx->info.channels, sizeof(float *))) {
      OV_ERROR_SET_GENERIC(err, ov_error_generic_out_of_memory);
      goto cleanup;
    }
    ctx->buffer[0] = NULL;
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
