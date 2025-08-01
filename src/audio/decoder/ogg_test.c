#include <ovtest.h>

#ifndef TESTDATADIR
#  define TESTDATADIR NSTR(".")
#endif

#include <inttypes.h>

#include <ovarray.h>

#include "../../test_util.h"
#include <ovl/audio/decoder.h>
#include <ovl/audio/decoder/ogg.h>
#include <ovl/audio/info.h>
#include <ovl/source.h>
#include <ovl/source/file.h>

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

static size_t cb_read(void *const ptr, size_t const size, size_t const nmemb, void *const datasource) {
  struct ovl_file *f = (struct ovl_file *)datasource;
  size_t read = 0;
  error err = ovl_file_read(f, ptr, size * nmemb, &read);
  if (efailed(err)) {
    return 0;
  }
  return read / size;
}

static int cb_seek(void *const datasource, ogg_int64_t const offset, int const whence) {
  struct ovl_file *f = (struct ovl_file *)datasource;
  error err = ovl_file_seek(f, offset, test_util_convert_seek_method(whence));
  if (efailed(err)) {
    return -1;
  }
  return 0;
}

static long cb_tell(void *const datasource) {
  struct ovl_file *f = (struct ovl_file *)datasource;
  int64_t pos = 0;
  error err = ovl_file_tell(f, &pos);
  if (efailed(err)) {
    return -1;
  }
  return (long)pos;
}

static struct test_util_decoded_audio decoder_all(wchar_t const *const filepath) {
  struct test_util_decoded_audio result = {0};
  struct ovl_file *f = NULL;
  error err = ovl_file_open(filepath, &f);
  if (efailed(err)) {
    goto cleanup;
  }
  OggVorbis_File vf;
  if (ov_open_callbacks(f,
                        &vf,
                        NULL,
                        0,
                        (ov_callbacks){
                            .read_func = cb_read,
                            .seek_func = cb_seek,
                            .tell_func = cb_tell,
                        }) < 0) {
    err = errg(err_fail);
    goto cleanup;
  }
  vorbis_info *vi = ov_info(&vf, -1);
  if (!vi) {
    err = errg(err_fail);
    goto cleanup;
  }
  ogg_int64_t samples = ov_pcm_total(&vf, -1);
  if (samples < 0) {
    err = errg(err_fail);
    goto cleanup;
  }
  err = OV_ARRAY_GROW(&result.buffer, (size_t)vi->channels);
  if (efailed(err)) {
    goto cleanup;
  }
  result.buffer[0] = NULL;
  err = OV_ARRAY_GROW(&result.buffer[0], (size_t)samples * (size_t)vi->channels);
  if (efailed(err)) {
    goto cleanup;
  }
  for (size_t c = 1; c < (size_t)vi->channels; c++) {
    result.buffer[c] = result.buffer[0] + c * (size_t)samples;
  }
  ogg_int64_t read = 0;
  int bitstream;
  float **temp_ptr;
  while (read < samples) {
    long ret = ov_read_float(&vf, &temp_ptr, 4096, &bitstream);
    if (ret <= 0) {
      break;
    }
    ogg_int64_t const r = ret;
    for (ogg_int64_t i = 0; i < r; i++) {
      for (size_t c = 0; c < (size_t)vi->channels; c++) {
        result.buffer[c][read + i] = temp_ptr[c][i];
      }
    }
    read += r;
  }
  result.samples = (size_t)samples;
  result.channels = (size_t)vi->channels;
  result.sample_rate = (size_t)vi->rate;

cleanup:
  ov_clear(&vf);
  if (f) {
    ovl_file_close(f);
    f = NULL;
  }
  if (efailed(err)) {
    OV_ARRAY_DESTROY(&result.buffer);
    result = (struct test_util_decoded_audio){0};
  }
  return result;
}

static void all(void) {
  struct test_util_decoded_audio audio = {0};
  struct ovl_source *source = NULL;
  struct ovl_audio_decoder *d = NULL;
  struct ovl_file *file_golden = NULL;
  struct ovl_file *file_all = NULL;
  error err = eok();

  audio = decoder_all(TESTDATADIR NSTR("/test.ogg"));
  if (!TEST_CHECK(audio.buffer)) {
    goto cleanup;
  }

  file_golden =
      test_util_create_wave_file(NSTR("ogg-all-golden.wav"), audio.samples, audio.channels, audio.sample_rate);
  if (!TEST_CHECK(file_golden)) {
    goto cleanup;
  }
  if (!TEST_CHECK(test_util_write_wave_body_float(
          file_golden, (float const *const *)audio.buffer, audio.samples, audio.channels))) {
    goto cleanup;
  }
  err = ovl_source_file_create(TESTDATADIR NSTR("/test.ogg"), &source);
  if (efailed(err)) {
    goto cleanup;
  }
  err = ovl_audio_decoder_ogg_create(source, &d);
  if (!TEST_SUCCEEDED_F(err)) {
    goto cleanup;
  }
  struct ovl_audio_info const *info = ovl_audio_decoder_get_info(d);
  TEST_CHECK(info->sample_rate == audio.sample_rate);
  TEST_CHECK(info->channels == audio.channels);
  TEST_CHECK(info->samples == audio.samples);
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

  file_all = test_util_create_wave_file(NSTR("ogg-all.wav"), (size_t)info->samples, info->channels, info->sample_rate);
  if (!TEST_CHECK(file_all)) {
    goto cleanup;
  }

  float const *const *pcm = NULL;
  size_t pos = 0;
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
    if (!TEST_CHECK(test_util_write_wave_body_float(file_all, pcm, read, info->channels))) {
      goto cleanup;
    }
    test_util_wave_diff_counter(
        &count, pcm, (float const *[]){audio.buffer[0] + pos, audio.buffer[1] + pos}, read, info->channels);
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

  TEST_CHECK(pos == info->samples);
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

  audio = decoder_all(TESTDATADIR NSTR("/test.ogg"));
  if (!TEST_CHECK(audio.buffer)) {
    goto cleanup;
  }
  err = ovl_source_file_create(TESTDATADIR NSTR("/test.ogg"), &source);
  if (efailed(err)) {
    goto cleanup;
  }
  err = ovl_audio_decoder_ogg_create(source, &d);
  if (!TEST_SUCCEEDED_F(err)) {
    goto cleanup;
  }
  struct ovl_audio_info const *info = ovl_audio_decoder_get_info(d);
  size_t const one_beat_samples = (size_t)(60 * audio.sample_rate / 89);
  file_golden = test_util_create_wave_file(
      NSTR("ogg-seek-golden.wav"), (size_t)info->samples - one_beat_samples, info->channels, info->sample_rate);
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
      NSTR("ogg-seek.wav"), (size_t)info->samples - one_beat_samples, info->channels, info->sample_rate);
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
