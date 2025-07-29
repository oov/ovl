#include <ovtest.h>

#ifndef TESTDATADIR
#  define TESTDATADIR NSTR(".")
#endif

#include <inttypes.h>

#include <ovarray.h>

#include "../../test_util.h"
#include <ovl/audio/decoder.h>
#include <ovl/audio/decoder/mp3.h>
#include <ovl/audio/info.h>
#include <ovl/source.h>
#include <ovl/source/file.h>

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
#define MINIMP3_FLOAT_OUTPUT
#include <minimp3_ex.h>
#ifdef __GNUC__
#  pragma GCC diagnostic pop
#endif // __GNUC__

static size_t cb_read(void *buf, size_t size, void *user_data) {
  struct ovl_file *f = user_data;
  size_t read;
  error err = ovl_file_read(f, buf, size, &read);
  if (efailed(err)) {
    ereport(err);
    return SIZE_MAX;
  }
  return read;
}

static int cb_seek(uint64_t position, void *user_data) {
  struct ovl_file *f = user_data;
  if (position > INT64_MAX) {
    return 1;
  }
  error err = ovl_file_seek(f, (int64_t)position, ovl_file_seek_method_set);
  if (efailed(err)) {
    ereport(err);
    return 1;
  }
  return 0;
}

static struct test_util_decoded_audio decoder_all(wchar_t const *const filepath) {
  struct ovl_file *f = NULL;
  struct test_util_decoded_audio result = {0};
  mp3dec_ex_t dec = {0};
  mp3dec_io_t io = {0};
  error err = ovl_file_open(filepath, &f);
  if (efailed(err)) {
    goto cleanup;
  }

  io.read = cb_read;
  io.read_data = f;
  io.seek = cb_seek;
  io.seek_data = f;
  if (mp3dec_ex_open_cb(&dec, &io, MP3D_SEEK_TO_SAMPLE)) {
    err = errg(err_fail);
    goto cleanup;
  }

  size_t const channels = (size_t)dec.info.channels;
  size_t const samples = (size_t)(dec.samples / channels);
  err = OV_ARRAY_GROW(&result.buffer, channels);
  if (efailed(err)) {
    goto cleanup;
  }
  result.buffer[0] = NULL;
  err = OV_ARRAY_GROW(&result.buffer[0], samples * channels);
  if (efailed(err)) {
    goto cleanup;
  }
  for (size_t c = 1; c < channels; c++) {
    result.buffer[c] = result.buffer[c - 1] + samples;
  }

  mp3d_sample_t *read_buf = NULL;
  mp3dec_frame_info_t frame_info;
  size_t pos = 0;
  while (pos < samples) {
    size_t const read = mp3dec_ex_read_frame(&dec, &read_buf, &frame_info, samples - pos);
    if (read == 0 || !read_buf) {
      break;
    }
    for (size_t i = 0; i < read / channels; i++) {
      for (size_t ch = 0; ch < channels; ch++) {
        result.buffer[ch][pos] = read_buf[i * channels + ch];
      }
      pos++;
    }
  }

  result.samples = samples;
  result.channels = channels;
  result.sample_rate = (size_t)dec.info.hz;

cleanup:
  mp3dec_ex_close(&dec);
  if (f) {
    ovl_file_close(f);
  }
  if (efailed(err)) {
    if (result.buffer) {
      OV_ARRAY_DESTROY(&result.buffer[0]);
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
  error err = eok();

  audio = decoder_all(TESTDATADIR NSTR("/test.mp3"));
  if (!TEST_CHECK(audio.buffer)) {
    goto cleanup;
  }

  file_golden =
      test_util_create_wave_file(NSTR("mp3-all-golden.wav"), audio.samples, audio.channels, audio.sample_rate);
  if (!TEST_CHECK(file_golden)) {
    goto cleanup;
  }
  if (!TEST_CHECK(test_util_write_wave_body_float(
          file_golden, (float const *const *)audio.buffer, audio.samples, audio.channels))) {
    goto cleanup;
  }
  err = ovl_source_file_create(TESTDATADIR NSTR("/test.mp3"), &source);
  if (efailed(err)) {
    goto cleanup;
  }
  err = ovl_audio_decoder_mp3_create(source, &d);
  if (!TEST_SUCCEEDED_F(err)) {
    goto cleanup;
  }
  struct ovl_audio_info const *info = ovl_audio_decoder_get_info(d);
  TEST_CHECK(info->channels == audio.channels);
  TEST_CHECK(info->sample_rate == audio.sample_rate);
  TEST_CHECK(info->samples == audio.samples);
  static uint64_t const start_expected = 32360;
  static uint64_t const end_expected = UINT64_MAX;
  static uint64_t const length_expected = 517753;
  TEST_CHECK(info->tag.loop_start == start_expected);
  TEST_MSG("tag.loop_start want %" PRIu64 " got %" PRIu64, start_expected, info->tag.loop_start);
  TEST_CHECK(info->tag.loop_end == end_expected);
  TEST_MSG("tag.loop_end want %" PRIu64 " got %" PRIu64, end_expected, info->tag.loop_end);
  TEST_CHECK(info->tag.loop_length == length_expected);
  TEST_MSG("tag.loop_length want %" PRIu64 " got %" PRIu64, length_expected, info->tag.loop_length);

  file_all = test_util_create_wave_file(NSTR("mp3-all.wav"), (size_t)info->samples, info->channels, info->sample_rate);
  if (!TEST_CHECK(file_all)) {
    goto cleanup;
  }

  float const *const *pcm = NULL;
  size_t pos = 0;
  struct test_util_wave_diff_count count = {0};
  while (pos < audio.samples) {
    size_t read = 0;
    err = ovl_audio_decoder_read(d, &pcm, &read);
    if (!TEST_SUCCEEDED_F(err)) {
      goto cleanup;
    }
    if (read == 0) {
      break;
    }
    if (!TEST_CHECK(test_util_write_wave_body_float(file_all, pcm, read, audio.channels))) {
      goto cleanup;
    }
    test_util_wave_diff_counter(
        &count, pcm, (float const *[]){audio.buffer[0] + pos, audio.buffer[1] + pos}, read, audio.channels);
    pos += read;
  }

  {
    size_t final_read = 0;
    err = ovl_audio_decoder_read(d, &pcm, &final_read);
    if (!TEST_SUCCEEDED_F(err)) {
      goto cleanup;
    }
    TEST_CHECK(final_read == 0);
    TEST_MSG("Extra read after EOF: expected 0, got %zu", final_read);
  }

  TEST_CHECK(pos == audio.samples);
  TEST_CHECK(count.large_diff_count == 0);
  TEST_MSG("Total samples: %zu, Mismatches: %zu (%.2f%%), Large differences: "
           "%zu (%.2f%%)",
           count.total_samples,
           count.mismatches,
           (double)count.mismatches / (double)count.total_samples * 100.0,
           count.large_diff_count,
           (double)count.large_diff_count / (double)count.total_samples * 100.0);

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
  struct ovl_source *source = NULL;
  struct ovl_audio_decoder *d = NULL;
  struct ovl_file *file_golden = NULL;
  struct ovl_file *file_seek = NULL;
  error err = eok();

  audio = decoder_all(TESTDATADIR NSTR("/test.mp3"));
  if (!TEST_CHECK(audio.buffer)) {
    goto cleanup;
  }
  err = ovl_source_file_create(TESTDATADIR NSTR("/test.mp3"), &source);
  if (efailed(err)) {
    goto cleanup;
  }
  err = ovl_audio_decoder_mp3_create(source, &d);
  if (!TEST_SUCCEEDED_F(err)) {
    goto cleanup;
  }
  struct ovl_audio_info const *info = ovl_audio_decoder_get_info(d);
  size_t const one_beat_samples = (size_t)(60 * audio.sample_rate / 89);
  file_golden = test_util_create_wave_file(
      NSTR("mp3-seek-golden.wav"), (size_t)info->samples - one_beat_samples, info->channels, info->sample_rate);
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

  err = ovl_audio_decoder_seek(d, one_beat_samples);
  if (!TEST_SUCCEEDED_F(err)) {
    goto cleanup;
  }

  file_seek = test_util_create_wave_file(
      NSTR("mp3-seek.wav"), (size_t)info->samples - one_beat_samples, info->channels, info->sample_rate);
  if (!TEST_CHECK(file_seek)) {
    goto cleanup;
  }

  float const *const *pcm = NULL;
  size_t pos = one_beat_samples;
  struct test_util_wave_diff_count count = {0};
  while (pos < info->samples) {
    size_t read = 0;
    err = ovl_audio_decoder_read(d, &pcm, &read);
    if (!TEST_SUCCEEDED_F(err)) {
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
  TEST_MSG("pos = %zu, samples = %llu", pos, info->samples);
  TEST_CHECK(count.large_diff_count == 0);
  TEST_MSG("Total samples: %zu, Mismatches: %zu (%.2f%%), Large differences: "
           "%zu (%.2f%%)",
           count.total_samples,
           count.mismatches,
           (double)count.mismatches / (double)count.total_samples * 100.0,
           count.large_diff_count,
           (double)count.large_diff_count / (double)count.total_samples * 100.0);

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
