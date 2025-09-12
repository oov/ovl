#include <ovl/audio/decoder/ogg.h>

#include "../tag.h"
#include "../tag/vorbis_comment.h"

#include <ovl/audio/decoder.h>
#include <ovl/audio/info.h>
#include <ovl/source.h>

#include <ovmo.h>

#include <limits.h>

#ifdef __GNUC__
#  ifndef __has_warning
#    define __has_warning(x) 0
#  endif
#  pragma GCC diagnostic push
#  if __has_warning("-Wreserved-identifier")
#    pragma GCC diagnostic ignored "-Wreserved-identifier"
#  endif
#  if __has_warning("-Wreserved-macro-identifier")
#    pragma GCC diagnostic ignored "-Wreserved-macro-identifier"
#  endif
#  if __has_warning("-Wused-but-marked-unused")
#    pragma GCC diagnostic ignored "-Wused-but-marked-unused"
#  endif
#  if __has_warning("-Wcast-function-type-strict")
#    pragma GCC diagnostic ignored "-Wcast-function-type-strict"
#  endif
#endif // __GNUC__
#include <vorbis/vorbisfile.h>
#ifdef __GNUC__
#  pragma GCC diagnostic pop
#endif // __GNUC__

struct ogg {
  struct ovl_audio_decoder_vtable const *vtable;
  struct ovl_source *source;
  uint64_t source_pos;
  uint64_t source_len;

  OggVorbis_File of;
  struct ovl_audio_info info;
};

static size_t cb_read(void *const ptr, size_t const size, size_t const nmemb, void *const datasource) {
  struct ogg *ctx = (struct ogg *)datasource;
  size_t const read = ovl_source_read(ctx->source, ptr, ctx->source_pos, size * nmemb);
  if (read == SIZE_MAX) {
    return 0;
  }
  ctx->source_pos += read;
  return read / size;
}

static int cb_seek(void *const datasource, ogg_int64_t const offset, int const whence) {
  struct ogg *ctx = (struct ogg *)datasource;
  if (whence == SEEK_SET) {
    if (offset < 0 || (uint64_t)offset > ctx->source_len) {
      return 1;
    }
    ctx->source_pos = (uint64_t)offset;
    return 0;
  }
  if (whence != SEEK_CUR && whence != SEEK_END) {
    return 1;
  }
  uint64_t const base = whence == SEEK_CUR ? ctx->source_pos : ctx->source_len;
  if (offset >= 0) {
    uint64_t const pos = base + (uint64_t)offset;
    if (pos > ctx->source_len || pos < base) {
      return 1;
    }
    ctx->source_pos = pos;
    return 0;
  }
  uint64_t const abs_offset = (uint64_t)(~offset) + UINT64_C(1);
  if (abs_offset > base) {
    return 1;
  }
  ctx->source_pos = base - abs_offset;
  return 0;
}

static long cb_tell(void *const datasource) {
  struct ogg *ctx = (struct ogg *)datasource;
  if (ctx->source_pos > LONG_MAX || ctx->source_pos > ctx->source_len) {
    return -1;
  }
  return (long)ctx->source_pos;
}

static size_t get_entry(void *const userdata, size_t const n, char const **const kv) {
  vorbis_comment *const vc = (vorbis_comment *)userdata;
  if (n >= (size_t)vc->comments) {
    return 0;
  }
  *kv = vc->user_comments[n];
  return (size_t)vc->comment_lengths[n];
}

static void destroy(struct ovl_audio_decoder **const dp) {
  struct ogg **const ctxp = (struct ogg **)(void *)dp;
  if (!ctxp || !*ctxp) {
    return;
  }
  struct ogg *const ctx = *ctxp;
  ov_clear(&ctx->of);
  ovl_audio_tag_destroy(&ctx->info.tag);
  OV_FREE(ctxp);
}

static struct ovl_audio_info const *get_info(struct ovl_audio_decoder const *const d) {
  struct ogg const *const ctx = (struct ogg const *)(void const *)d;
  if (!ctx) {
    return NULL;
  }
  return &ctx->info;
}

static NODISCARD bool read(struct ovl_audio_decoder *const d,
                           float const *const **const pcm,
                           size_t *const samples,
                           struct ov_error *const err) {
  struct ogg *const ctx = (struct ogg *)(void *)d;
  if (!ctx || !pcm || !samples) {
    OV_ERROR_SET_GENERIC(err, ov_error_generic_invalid_argument);
    return false;
  }
  // ov_read_float only returns data for one packet at most,
  // so if you request about 1 second of data, you will receive data of a good
  // length.
  long const r = ov_read_float(&ctx->of, (float ***)ov_deconster_(pcm), (int)ctx->info.sample_rate, NULL);
  if (r < 0) {
    OV_ERROR_SETF(
        err, ov_error_type_generic, ov_error_generic_fail, "%1$d", gettext("Failed to read samples.(code:%1$d)"), r);
    return false;
  }

  *samples = (size_t)r;
  return true;
}

static NODISCARD bool seek(struct ovl_audio_decoder *const d, uint64_t const position, struct ov_error *const err) {
  struct ogg *const ctx = (struct ogg *)(void *)d;
  if (!ctx) {
    OV_ERROR_SET_GENERIC(err, ov_error_generic_invalid_argument);
    return false;
  }
  if (position > INT64_MAX) {
    OV_ERROR_SET(err, ov_error_type_generic, ov_error_generic_fail, gettext("Invalid position"));
    return false;
  }
  if (ov_pcm_seek(&ctx->of, (ogg_int64_t)position) != 0) {
    OV_ERROR_SET(err, ov_error_type_generic, ov_error_generic_fail, gettext("Failed to seek"));
    return false;
  }
  return true;
}

NODISCARD bool ovl_audio_decoder_ogg_create(struct ovl_source *const source,
                                            struct ovl_audio_decoder **const dp,
                                            struct ov_error *const err) {
  if (!dp || *dp || !source) {
    OV_ERROR_SET_GENERIC(err, ov_error_generic_invalid_argument);
    return false;
  }
  struct ogg *ctx = NULL;
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
    *ctx = (struct ogg){
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
    if (ov_open_callbacks(ctx,
                          &ctx->of,
                          NULL,
                          0,
                          (ov_callbacks){
                              .read_func = cb_read,
                              .seek_func = cb_seek,
                              .tell_func = cb_tell,
                          }) != 0) {
      OV_ERROR_SET(err, ov_error_type_generic, ov_error_generic_fail, gettext("Failed to open OggVorbis file"));
      goto cleanup;
    }

    ogg_int64_t const total = ov_pcm_total((OggVorbis_File *)ov_deconster_(&ctx->of), -1);
    if (total < 0) {
      OV_ERROR_SET_GENERIC(err, ov_error_generic_fail);
      goto cleanup;
    }
    ctx->info.samples = (uint64_t)total;

    vorbis_info const *const info = ov_info(&ctx->of, -1);
    if (!info) {
      OV_ERROR_SET(err, ov_error_type_generic, ov_error_generic_fail, gettext("Failed to get vorbis info"));
      goto cleanup;
    }
    ctx->info.channels = (size_t)info->channels;
    ctx->info.sample_rate = (size_t)info->rate;

    vorbis_comment *const vc = ov_comment(&ctx->of, -1);
    if (vc) {
      if (!ovl_audio_tag_vorbis_comment_read(&ctx->info.tag, (size_t)vc->comments, vc, get_entry, err)) {
        OV_ERROR_ADD_TRACE(err);
        goto cleanup;
      }
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
