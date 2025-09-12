#include <ovtest.h>

#ifndef TESTDATADIR
#  define TESTDATADIR NSTR(".")
#endif

#include <inttypes.h>

#include <ovarray.h>

#include "../../test_util.h"
#include <ovl/audio/decoder.h>
#include <ovl/audio/decoder/opus.h>
#include <ovl/audio/info.h>
#include <ovl/source.h>
#include <ovl/source/file.h>

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

static int cb_read(void *const stream, unsigned char *const ptr, int const nbytes) {
  struct ovl_file *f = (struct ovl_file *)stream;
  size_t read = 0;
  struct ov_error err = {0};
  if (!ovl_file_read(f, ptr, (size_t)nbytes, &read, &err)) {
    return -1;
  }
  return (int)read;
}

static int cb_seek(void *const stream, opus_int64 const offset, int const whence) {
  struct ovl_file *f = (struct ovl_file *)stream;
  struct ov_error err = {0};
  if (!ovl_file_seek(f, offset, test_util_convert_seek_method(whence), &err)) {
    return -1;
  }
  return 0;
}

static opus_int64 cb_tell(void *const stream) {
  struct ovl_file *f = (struct ovl_file *)stream;
  int64_t pos = 0;
  struct ov_error err = {0};
  if (!ovl_file_tell(f, &pos, &err)) {
    return -1;
  }
  return pos;
}

static struct test_util_decoded_audio decoder_all(wchar_t const *const filepath) {
  struct test_util_decoded_audio result = {0};
  struct ovl_file *f = NULL;
  OggOpusFile *of = NULL;
  float *temp = NULL;
  bool success = false;
  struct ov_error err = {0};
  {
    if (!ovl_file_open(filepath, &f, &err)) {
      goto cleanup;
    }
    int error = 0;
    of = op_open_callbacks(f,
                           &(OpusFileCallbacks){
                               .read = cb_read,
                               .seek = cb_seek,
                               .tell = cb_tell,
                               .close = NULL,
                           },
                           NULL,
                           0,
                           &error);
    if (!of) {
      OV_ERROR_SET_GENERIC(&err, ov_error_generic_fail);
      goto cleanup;
    }

    const OpusHead *head = op_head(of, 0);
    if (!head) {
      OV_ERROR_SET_GENERIC(&err, ov_error_generic_fail);
      goto cleanup;
    }
    size_t const channels = (size_t)head->channel_count;

    ogg_int64_t const samples_i64 = op_pcm_total(of, -1);
    if (samples_i64 < 0) {
      OV_ERROR_SET_GENERIC(&err, ov_error_generic_fail);
      goto cleanup;
    }
    size_t const samples = (size_t)samples_i64;

    if (!OV_ARRAY_GROW(&result.buffer, head->channel_count)) {
      OV_ERROR_SET_GENERIC(&err, ov_error_generic_out_of_memory);
      goto cleanup;
    }
    result.buffer[0] = NULL;
    if (!OV_ARRAY_GROW(&result.buffer[0], samples * channels)) {
      OV_ERROR_SET_GENERIC(&err, ov_error_generic_out_of_memory);
      goto cleanup;
    }
    for (size_t i = 1; i < channels; i++) {
      result.buffer[i] = result.buffer[i - 1] + samples;
    }

    enum { decoder_block = 4800 };
    if (!OV_ARRAY_GROW(&temp, channels * decoder_block)) {
      OV_ERROR_SET_GENERIC(&err, ov_error_generic_out_of_memory);
      goto cleanup;
    }

    size_t read = 0;
    while (read < samples) {
      int ret = op_read_float(of, temp, (int)decoder_block, NULL);
      if (ret <= 0) {
        break;
      }
      size_t const r = (size_t)ret;
      for (size_t i = 0; i < r; i++) {
        for (size_t c = 0; c < channels; c++) {
          result.buffer[c][read + i] = temp[i * channels + c];
        }
      }
      read += r;
    }

    result.samples = samples;
    result.channels = channels;
    result.sample_rate = 48000; // Opus is always 48kHz
  }
  success = true;

cleanup:
  if (temp) {
    OV_ARRAY_DESTROY(&temp);
  }
  if (of) {
    op_free(of);
    of = NULL;
  }
  if (f) {
    ovl_file_close(f);
    f = NULL;
  }
  if (!success) {
    if (result.buffer[0]) {
      OV_ARRAY_DESTROY(&result.buffer[0]);
    }
    if (result.buffer) {
      OV_ARRAY_DESTROY(&result.buffer);
    }
    result = (struct test_util_decoded_audio){0};
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
  bool success = false;

  audio = decoder_all(TESTDATADIR NSTR("/test.opus"));
  if (!TEST_CHECK(audio.buffer)) {
    goto cleanup;
  }

  file_golden =
      test_util_create_wave_file(NSTR("opus-all-golden.wav"), audio.samples, audio.channels, audio.sample_rate);
  if (!TEST_CHECK(file_golden)) {
    goto cleanup;
  }
  if (!TEST_CHECK(test_util_write_wave_body_float(
          file_golden, (float const *const *)audio.buffer, audio.samples, audio.channels))) {
    goto cleanup;
  }

  if (!TEST_CHECK(ovl_source_file_create(TESTDATADIR NSTR("/test.opus"), &source, &err))) {
    OV_ERROR_ADD_TRACE(&err);
    goto cleanup;
  }
  if (!TEST_CHECK(ovl_audio_decoder_opus_create(source, &d, &err))) {
    OV_ERROR_ADD_TRACE(&err);
    goto cleanup;
  }
  {
    struct ovl_audio_info const *info = ovl_audio_decoder_get_info(d);
    TEST_CHECK(info->samples == audio.samples);
    TEST_CHECK(info->channels == audio.channels);
    TEST_CHECK(info->sample_rate == audio.sample_rate);
    static char const title_expected[] = "タイトル";
    static char const artist_expected[] = "アーティスト";
    static uint64_t const start_expected = 32360;
    static uint64_t const end_expected = UINT64_MAX;
    static uint64_t const length_expected = 517753;
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
        test_util_create_wave_file(NSTR("opus-all.wav"), (size_t)info->samples, info->channels, info->sample_rate);
    if (!TEST_CHECK(file_all)) {
      goto cleanup;
    }

    float const *const *pcm = NULL;
    size_t pos = 0;
    struct test_util_wave_diff_count count = {0};
    while (pos < info->samples) {
      size_t read = 0;
      if (!TEST_CHECK(ovl_audio_decoder_read(d, &pcm, &read, &err))) {
        OV_ERROR_ADD_TRACE(&err);
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
      if (!TEST_CHECK(ovl_audio_decoder_read(d, &pcm, &final_read, &err))) {
        OV_ERROR_ADD_TRACE(&err);
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

  success = true;
cleanup:
  if (!success) {
    OV_ERROR_REPORT(&err, NULL);
  }
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
  struct ovl_source *source = NULL;
  struct ovl_audio_decoder *d = NULL;
  struct ovl_file *file_golden = NULL;
  struct ovl_file *file_seek = NULL;
  struct ov_error err = {0};
  bool success = false;

  audio = decoder_all(TESTDATADIR NSTR("/test.opus"));
  if (!TEST_CHECK(audio.buffer)) {
    goto cleanup;
  }
  if (!TEST_CHECK(ovl_source_file_create(TESTDATADIR NSTR("/test.opus"), &source, &err))) {
    OV_ERROR_ADD_TRACE(&err);
    goto cleanup;
  }
  if (!TEST_CHECK(ovl_audio_decoder_opus_create(source, &d, &err))) {
    OV_ERROR_ADD_TRACE(&err);
    goto cleanup;
  }
  {
    struct ovl_audio_info const *info = ovl_audio_decoder_get_info(d);
    size_t const one_beat_samples = (size_t)(60 * audio.sample_rate / 89);

    file_golden = test_util_create_wave_file(
        NSTR("opus-seek-golden.wav"), (size_t)info->samples - one_beat_samples, info->channels, audio.sample_rate);
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

    if (!TEST_CHECK(ovl_audio_decoder_seek(d, one_beat_samples, &err))) {
      OV_ERROR_ADD_TRACE(&err);
      goto cleanup;
    }
    file_seek = test_util_create_wave_file(
        NSTR("opus-seek.wav"), (size_t)info->samples - one_beat_samples, info->channels, audio.sample_rate);
    if (!TEST_CHECK(file_seek)) {
      goto cleanup;
    }

    float const *const *pcm = NULL;
    size_t pos = one_beat_samples;
    struct test_util_wave_diff_count count = {0};
    while (pos < info->samples) {
      size_t read = 0;
      if (!TEST_CHECK(ovl_audio_decoder_read(d, &pcm, &read, &err))) {
        OV_ERROR_ADD_TRACE(&err);
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

    TEST_CHECK(count.large_diff_count == 0);
    TEST_MSG("Total samples: %zu, Mismatches: %zu (%.2f%%), Large differences: "
             "%zu (%.2f%%)",
             count.total_samples,
             count.mismatches,
             (double)count.mismatches / (double)count.total_samples * 100.0,
             count.large_diff_count,
             (double)count.large_diff_count / (double)count.total_samples * 100.0);
    TEST_CHECK(pos == info->samples);
  }

  success = true;
cleanup:
  if (!success) {
    OV_ERROR_REPORT(&err, NULL);
  }
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
