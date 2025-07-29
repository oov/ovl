#include <ovl/audio/decoder/ogg.h>

#include <ovl/audio/decoder.h>
#include <ovl/audio/info.h>
#include <ovl/source.h>

#include "../tag.h"
#include "../tag/vorbis_comment.h"
#include "../../i18n.h"

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
  struct ogg *ctx = datasource;
  size_t const read = ovl_source_read(ctx->source, ptr, ctx->source_pos, size * nmemb);
  if (read == SIZE_MAX) {
    return 0;
  }
  ctx->source_pos += read;
  return read / size;
}

static int cb_seek(void *const datasource, ogg_int64_t const offset, int const whence) {
  struct ogg *ctx = datasource;
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
  struct ogg *ctx = datasource;
  if (ctx->source_pos > LONG_MAX || ctx->source_pos > ctx->source_len) {
    return -1;
  }
  return (long)ctx->source_pos;
}

static size_t get_entry(void *const userdata, size_t const n, char const **const kv) {
  vorbis_comment *const vc = userdata;
  if (n >= (size_t)vc->comments) {
    return 0;
  }
  *kv = vc->user_comments[n];
  return (size_t)vc->comment_lengths[n];
}

static void destroy(struct ovl_audio_decoder **const dp) {
  struct ogg **const ctxp = (void *)dp;
  if (!ctxp || !*ctxp) {
    return;
  }
  struct ogg *const ctx = *ctxp;
  ov_clear(&ctx->of);
  ovl_audio_tag_destroy(&ctx->info.tag);
  ereport(mem_free(ctxp));
}

static struct ovl_audio_info const *get_info(struct ovl_audio_decoder const *const d) {
  struct ogg const *const ctx = (void const *)d;
  if (!ctx) {
    return NULL;
  }
  return &ctx->info;
}

static NODISCARD error read(struct ovl_audio_decoder *const d, float const *const **const pcm, size_t *const samples) {
  struct ogg *const ctx = (void *)d;
  if (!ctx || !pcm || !samples) {
    return errg(err_invalid_arugment);
  }
  // ov_read_float only returns data for one packet at most,
  // so if you request about 1 second of data, you will receive data of a good
  // length.
  long const r = ov_read_float(&ctx->of, (float ***)ov_deconster_(pcm), (int)ctx->info.sample_rate, NULL);
  if (r < 0) {
    return emsg_i18nf(err_type_generic, err_fail, NULL, gettext("Failed to read samples.(code:%d)"), r);
  }

  *samples = (size_t)r;
  return eok();
}

static NODISCARD error seek(struct ovl_audio_decoder *const d, uint64_t const position) {
  struct ogg *const ctx = (void *)d;
  if (!ctx) {
    return errg(err_invalid_arugment);
  }
  if (position > INT64_MAX) {
    return emsg_i18n(err_type_generic, err_fail, gettext("Invalid position"));
  }
  if (ov_pcm_seek(&ctx->of, (ogg_int64_t)position) != 0) {
    return emsg_i18n(err_type_generic, err_fail, gettext("Failed to seek"));
  }
  return eok();
}

NODISCARD error ovl_audio_decoder_ogg_create(struct ovl_source *const source, struct ovl_audio_decoder **const dp) {
  if (!dp || *dp || !source) {
    return errg(err_invalid_arugment);
  }
  struct ogg *ctx = NULL;
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
    err = emsg_i18n(err_type_generic, err_fail, gettext("Failed to get source size"));
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
    err = emsg_i18n(err_type_generic, err_fail, gettext("Failed to open OggVorbis file"));
    goto cleanup;
  }

  ogg_int64_t const total = ov_pcm_total(ov_deconster_(&ctx->of), -1);
  if (total < 0) {
    return errg(err_fail);
  }
  ctx->info.samples = (uint64_t)total;

  vorbis_info const *const info = ov_info(&ctx->of, -1);
  if (!info) {
    err = emsg_i18n(err_type_generic, err_fail, gettext("Failed to get vorbis info"));
    goto cleanup;
  }
  ctx->info.channels = (size_t)info->channels;
  ctx->info.sample_rate = (size_t)info->rate;

  vorbis_comment *const vc = ov_comment(&ctx->of, -1);
  if (vc) {
    err = ovl_audio_tag_vorbis_comment_read(&ctx->info.tag, (size_t)vc->comments, vc, get_entry);
    if (efailed(err)) {
      err = ethru(err);
      goto cleanup;
    }
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
