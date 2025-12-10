#include <ovtest.h>

#ifndef TESTDATADIR
#  define TESTDATADIR NSTR(".")
#endif

#include <inttypes.h>

#include <ovarray.h>

#include "../../test_util.h"
#include <ovl/audio/decoder.h>
#include <ovl/audio/decoder/flac.h>
#include <ovl/audio/info.h>
#include <ovl/source.h>
#include <ovl/source/file.h>

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

struct decoder_context {
  struct ovl_file *file;
  float **buffer;
  size_t samples;
  size_t channels;
  size_t sample_rate;
  int error;
};

static FLAC__StreamDecoderReadStatus
read_callback(FLAC__StreamDecoder const *const decoder, FLAC__byte buffer[], size_t *bytes, void *client_data) {
  (void)decoder;
  struct decoder_context *const ctx = (struct decoder_context *)client_data;
  size_t read = 0;
  struct ov_error err = {0};
  if (!ovl_file_read(ctx->file, buffer, *bytes, &read, &err)) {
    return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
  }
  if (read == 0) {
    return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
  }
  *bytes = read;
  return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

static FLAC__StreamDecoderSeekStatus
seek_callback(FLAC__StreamDecoder const *const decoder, FLAC__uint64 absolute_byte_offset, void *client_data) {
  (void)decoder;
  struct decoder_context *const ctx = (struct decoder_context *)client_data;
  struct ov_error err = {0};
  if (!ovl_file_seek(ctx->file, (int64_t)absolute_byte_offset, ovl_file_seek_method_set, &err)) {
    return FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
  }
  return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
}

static FLAC__StreamDecoderTellStatus
tell_callback(FLAC__StreamDecoder const *const decoder, FLAC__uint64 *absolute_byte_offset, void *client_data) {
  (void)decoder;
  struct decoder_context *const ctx = (struct decoder_context *)client_data;
  int64_t pos = 0;
  struct ov_error err = {0};
  if (!ovl_file_tell(ctx->file, &pos, &err)) {
    return FLAC__STREAM_DECODER_TELL_STATUS_ERROR;
  }
  *absolute_byte_offset = (FLAC__uint64)pos;
  return FLAC__STREAM_DECODER_TELL_STATUS_OK;
}

static FLAC__StreamDecoderLengthStatus
length_callback(FLAC__StreamDecoder const *const decoder, FLAC__uint64 *stream_length, void *client_data) {
  (void)decoder;
  struct decoder_context *const ctx = (struct decoder_context *)client_data;
  uint64_t size = 0;
  struct ov_error err = {0};
  if (!ovl_file_size(ctx->file, &size, &err)) {
    return FLAC__STREAM_DECODER_LENGTH_STATUS_ERROR;
  }
  *stream_length = size;
  return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
}

static FLAC__bool eof_callback(FLAC__StreamDecoder const *const decoder, void *client_data) {
  (void)decoder;
  struct decoder_context *const ctx = (struct decoder_context *)client_data;
  int64_t pos = 0;
  uint64_t size = 0;
  struct ov_error err1 = {0};
  struct ov_error err2 = {0};
  if (!ovl_file_tell(ctx->file, &pos, &err1) || !ovl_file_size(ctx->file, &size, &err2)) {
    return true;
  }
  return pos >= (int64_t)size;
}

static FLAC__StreamDecoderWriteStatus write_callback(FLAC__StreamDecoder const *const decoder,
                                                     FLAC__Frame const *const frame,
                                                     FLAC__int32 const *const buffer[],
                                                     void *const client_data) {
  (void)decoder;
  struct decoder_context *const ctx = (struct decoder_context *)client_data;

  for (size_t i = 0; i < frame->header.channels; ++i) {
    float *dst = ctx->buffer[i] + ctx->samples;
    FLAC__int32 const *src = buffer[i];
    for (size_t j = 0; j < frame->header.blocksize; ++j) {
      dst[j] = (float)src[j] / (float)(1u << (frame->header.bits_per_sample - 1));
    }
  }
  ctx->samples += frame->header.blocksize;
  return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

static void metadata_callback(FLAC__StreamDecoder const *const decoder,
                              FLAC__StreamMetadata const *const metadata,
                              void *const client_data) {
  (void)decoder;
  struct decoder_context *const ctx = (struct decoder_context *)client_data;
  if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
    ctx->channels = metadata->data.stream_info.channels;
    ctx->sample_rate = metadata->data.stream_info.sample_rate;
    if (!OV_ARRAY_GROW(&ctx->buffer, ctx->channels)) {
      ctx->error = 1;
      return;
    }
    ctx->buffer[0] = NULL;
    if (!OV_ARRAY_GROW(&ctx->buffer[0], metadata->data.stream_info.total_samples * ctx->channels)) {
      ctx->error = 1;
      return;
    }
    for (size_t c = 1; c < ctx->channels; c++) {
      ctx->buffer[c] = ctx->buffer[0] + c * metadata->data.stream_info.total_samples;
    }
  }
}

static void
error_callback(FLAC__StreamDecoder const *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data) {
  (void)decoder;
  (void)status;
  struct decoder_context *const ctx = (struct decoder_context *)client_data;
  ctx->error = 1;
}

static struct test_util_decoded_audio decoder_all(wchar_t const *const filepath) {
  struct test_util_decoded_audio result = {0};
  struct decoder_context ctx = {0};
  FLAC__StreamDecoder *decoder = NULL;
  struct ov_error err = {0};
  if (!ovl_file_open(filepath, &ctx.file, &err)) {
    goto cleanup;
  }
  decoder = FLAC__stream_decoder_new();
  if (!decoder) {
    goto cleanup;
  }
  if (FLAC__stream_decoder_init_stream(decoder,
                                       read_callback,
                                       seek_callback,
                                       tell_callback,
                                       length_callback,
                                       eof_callback,
                                       write_callback,
                                       metadata_callback,
                                       error_callback,
                                       &ctx) != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
    goto cleanup;
  }
  if (!FLAC__stream_decoder_process_until_end_of_metadata(decoder)) {
    goto cleanup;
  }
  if (ctx.error || !FLAC__stream_decoder_process_until_end_of_stream(decoder)) {
    goto cleanup;
  }
  result.buffer = ctx.buffer;
  result.samples = ctx.samples;
  result.channels = ctx.channels;
  result.sample_rate = ctx.sample_rate;
  ctx.buffer = NULL;
cleanup:
  if (decoder) {
    FLAC__stream_decoder_finish(decoder);
    FLAC__stream_decoder_delete(decoder);
  }
  if (ctx.file) {
    ovl_file_close(ctx.file);
  }
  if (ctx.buffer) {
    OV_ARRAY_DESTROY(&ctx.buffer[0]);
    OV_ARRAY_DESTROY(&ctx.buffer);
  }
  return result;
}

static void all(void) {
  struct test_util_decoded_audio audio = {0};
  struct ovl_audio_decoder *d = NULL;
  struct ovl_source *source = NULL;
  struct ovl_file *file_golden = NULL;
  struct ovl_file *file_all = NULL;
  struct ov_error err = {0};

  {
    audio = decoder_all(TESTDATADIR NSTR("/test.flac"));
    if (!TEST_CHECK(audio.buffer)) {
      goto cleanup;
    }
    file_golden =
        test_util_create_wave_file(NSTR("flac-all-golden.wav"), audio.samples, audio.channels, audio.sample_rate);
    if (!TEST_CHECK(file_golden)) {
      goto cleanup;
    }
    if (!TEST_CHECK(test_util_write_wave_body_float(
            file_golden, (float const *const *)audio.buffer, audio.samples, audio.channels))) {
      goto cleanup;
    }
    if (!TEST_SUCCEEDED(ovl_source_file_create(TESTDATADIR NSTR("/test.flac"), &source, &err), &err)) {
      goto cleanup;
    }
    if (!TEST_SUCCEEDED(ovl_audio_decoder_flac_create(source, &d, &err), &err)) {
      goto cleanup;
    }

    struct ovl_audio_info const *info = ovl_audio_decoder_get_info(d);
    TEST_CHECK(info->sample_rate == audio.sample_rate);
    TEST_CHECK(info->channels == audio.channels);
    TEST_CHECK(info->samples == audio.samples);
    static char const title_expected[] = "タイトル";
    static char const artist_expected[] = "アーティスト";
    static uint64_t const start_expected = 34944;
    static uint64_t const end_expected = 50424;
    static uint64_t const length_expected = UINT64_MAX;
    TEST_CHECK(strcmp(info->tag.title, title_expected) == 0);
    TEST_MSG("tag.title want \"%s\" got \"%s\"", title_expected, info->tag.title);
    TEST_CHECK(strcmp(info->tag.artist, artist_expected) == 0);
    TEST_MSG("tag.artist want \"%s\" got \"%s\"", artist_expected, info->tag.artist);
    TEST_CHECK(info->tag.loop_start == start_expected);
    TEST_MSG("tag.loop_start want %" PRIu64 " got %" PRIu64, start_expected, info->tag.loop_start);
    TEST_CHECK(info->tag.loop_end == end_expected);
    TEST_MSG("tag.loop_end want %" PRIu64 " got %" PRIu64, end_expected, info->tag.loop_end);
    TEST_CHECK(info->tag.loop_length == length_expected);
    TEST_MSG("tag.loop_length want %" PRIu64 " got %" PRIu64, length_expected, info->tag.loop_length);

    file_all =
        test_util_create_wave_file(NSTR("flac-all.wav"), (size_t)info->samples, info->channels, info->sample_rate);
    if (!TEST_CHECK(file_all)) {
      goto cleanup;
    }

    float const *const *pcm = NULL;
    size_t pos = 0;
    struct test_util_wave_diff_count count = {0};
    while (pos < info->samples) {
      size_t read = 0;
      if (!TEST_SUCCEEDED(ovl_audio_decoder_read(d, &pcm, &read, &err), &err)) {
        goto cleanup;
      }
      if (read == 0) {
        break;
      }
      if (!TEST_CHECK(test_util_write_wave_body_float(file_all, pcm, read, info->channels))) {
        goto cleanup;
      }
      test_util_wave_diff_counter(
          &count, pcm, (float const *[]){audio.buffer[0] + pos, audio.buffer[1] + pos}, read, info->channels);
      pos += read;
    }

    {
      size_t final_read = 0;
      if (!TEST_SUCCEEDED(ovl_audio_decoder_read(d, &pcm, &final_read, &err), &err)) {
        goto cleanup;
      }
      TEST_CHECK(final_read == 0);
      TEST_MSG("Extra read after EOF: expected 0, got %zu", final_read);
    }

    TEST_CHECK(pos == info->samples);
    TEST_CHECK(count.large_diff_count == 0);
    TEST_MSG("Total samples: %zu, Mismatches: %zu (%.2f%%), Large differences: "
             "%zu (%.2f%%)",
             count.total_samples,
             count.mismatches,
             (double)count.mismatches / (double)count.total_samples * 100.0,
             count.large_diff_count,
             (double)count.large_diff_count / (double)count.total_samples * 100.0);
  }
cleanup:
  if (file_golden) {
    test_util_close_wave(file_golden);
  }
  if (file_all) {
    test_util_close_wave(file_all);
  }
  if (d) {
    ovl_audio_decoder_destroy(&d);
  }
  if (source) {
    ovl_source_destroy(&source);
  }
  if (audio.buffer) {
    OV_ARRAY_DESTROY(&audio.buffer[0]);
    OV_ARRAY_DESTROY(&audio.buffer);
  }
}

static void seek(void) {
  struct test_util_decoded_audio audio = {0};
  struct ovl_audio_decoder *d = NULL;
  struct ovl_source *source = NULL;
  struct ovl_file *file_golden = NULL;
  struct ovl_file *file_seek = NULL;
  struct ov_error err = {0};

  audio = decoder_all(TESTDATADIR NSTR("/test.flac"));
  if (!TEST_CHECK(audio.buffer)) {
    goto cleanup;
  }
  if (!TEST_SUCCEEDED(ovl_source_file_create(TESTDATADIR NSTR("/test.flac"), &source, &err), &err)) {
    goto cleanup;
  }
  if (!TEST_SUCCEEDED(ovl_audio_decoder_flac_create(source, &d, &err), &err)) {
    goto cleanup;
  }
  {
    struct ovl_audio_info const *info = ovl_audio_decoder_get_info(d);
    size_t const one_beat_samples = (size_t)(60 * audio.sample_rate / 89);
    file_golden = test_util_create_wave_file(
        NSTR("flac-seek-golden.wav"), (size_t)info->samples - one_beat_samples, info->channels, info->sample_rate);
    if (!TEST_CHECK(file_golden)) {
      goto cleanup;
    }
    if (!TEST_CHECK(test_util_write_wave_body_float(
            file_golden,
            (float const *[]){audio.buffer[0] + one_beat_samples, audio.buffer[1] + one_beat_samples},
            (size_t)info->samples - one_beat_samples,
            info->channels))) {
      goto cleanup;
    }

    if (!TEST_SUCCEEDED(ovl_audio_decoder_seek(d, one_beat_samples, &err), &err)) {
      goto cleanup;
    }

    file_seek = test_util_create_wave_file(
        NSTR("flac-seek.wav"), (size_t)info->samples - one_beat_samples, info->channels, info->sample_rate);
    if (!TEST_CHECK(file_seek)) {
      goto cleanup;
    }

    float const *const *pcm = NULL;
    size_t pos = one_beat_samples;
    struct test_util_wave_diff_count count = {0};
    while (pos < info->samples) {
      size_t read = 0;
      if (!TEST_SUCCEEDED(ovl_audio_decoder_read(d, &pcm, &read, &err), &err)) {
        goto cleanup;
      }
      if (read == 0) {
        break;
      }
      if (!TEST_CHECK(test_util_write_wave_body_float(file_seek, pcm, read, info->channels))) {
        goto cleanup;
      }
      test_util_wave_diff_counter(
          &count, pcm, (float const *[]){audio.buffer[0] + pos, audio.buffer[1] + pos}, read, info->channels);
      pos += read;
    }
    TEST_CHECK(pos == info->samples);
    TEST_MSG("want %llu got %zu", info->samples, pos);
    TEST_CHECK(count.large_diff_count == 0);
    TEST_MSG("Total samples: %zu, Mismatches: %zu (%.2f%%), Large differences: "
             "%zu (%.2f%%)",
             count.total_samples,
             count.mismatches,
             (double)count.mismatches / (double)count.total_samples * 100.0,
             count.large_diff_count,
             (double)count.large_diff_count / (double)count.total_samples * 100.0);
  }

cleanup:
  if (file_golden) {
    test_util_close_wave(file_golden);
  }
  if (file_seek) {
    test_util_close_wave(file_seek);
  }
  if (d) {
    ovl_audio_decoder_destroy(&d);
  }
  if (source) {
    ovl_source_destroy(&source);
  }
  if (audio.buffer) {
    OV_ARRAY_DESTROY(&audio.buffer[0]);
    OV_ARRAY_DESTROY(&audio.buffer);
  }
}

TEST_LIST = {
    {"all", all},
    {"seek", seek},
    {NULL, NULL},
};
