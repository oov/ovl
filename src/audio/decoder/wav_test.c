#include <ovtest.h>

#include "../../test_util.h"
#include <ovl/audio/decoder.h>
#include <ovl/audio/decoder/wav.h>
#include <ovl/audio/info.h>
#include <ovl/source.h>
#include <ovl/source/file.h>

#include <inttypes.h>
#include <ovarray.h>
#include <ovprintf.h>

#ifndef TESTDATADIR
#  define TESTDATADIR NSTR(".")
#endif

static void various_formats(void) {
  static struct test_case {
    char const *name;
    NATIVE_CHAR const *input;
  } const tests[] = {
      {"wav-mono-8", TESTDATADIR NSTR("/test-8khz-mono-8.wav")},
      {"wav-mono-16", TESTDATADIR NSTR("/test-8khz-mono-16.wav")},
      {"wav-mono-24", TESTDATADIR NSTR("/test-8khz-mono-24.wav")},
      {"wav-mono-32", TESTDATADIR NSTR("/test-8khz-mono-32.wav")},
      {"wav-mono-32f", TESTDATADIR NSTR("/test-8khz-mono-32f.wav")},
      {"wav-mono-64f", TESTDATADIR NSTR("/test-8khz-mono-64f.wav")},
      {"wav-stereo-8", TESTDATADIR NSTR("/test-8khz-stereo-8.wav")},
      {"rf64-mono-8", TESTDATADIR NSTR("/test-8khz-mono-8-rf64.wav")},
      {"w64-mono-8", TESTDATADIR NSTR("/test-8khz-mono-8.w64")},
      {"aiff-mono-8", TESTDATADIR NSTR("/test-8khz-mono-8.aiff")},
      {"aiff-mono-16", TESTDATADIR NSTR("/test-8khz-mono-16.aiff")},
      {"aiff-mono-24", TESTDATADIR NSTR("/test-8khz-mono-24.aiff")},
      {"aiff-mono-32", TESTDATADIR NSTR("/test-8khz-mono-32.aiff")},
      {"aiff-stereo-8", TESTDATADIR NSTR("/test-8khz-stereo-8.aiff")},
  };
  for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
    struct test_case const *const test = &tests[i];
    TEST_CASE(test->name);
    struct ovl_audio_decoder *d = NULL;
    struct ovl_file *file_out = NULL;
    struct ovl_source *source = NULL;

    error err = ovl_source_file_create(test->input, &source);
    if (!TEST_SUCCEEDED_F(err)) {
      TEST_MSG("source_file_open failed for %ls", test->input);
      goto cleanup;
    }

    err = ovl_audio_decoder_wav_create(source, &d);
    if (!TEST_SUCCEEDED_F(err)) {
      TEST_MSG("decoder_create failed for %ls", test->input);
      goto cleanup;
    }

    struct ovl_audio_info const *info = ovl_audio_decoder_get_info(d);
    if (!TEST_CHECK(info->sample_rate == 8000)) {
      TEST_MSG("want %d, got %zu", 8000, info->sample_rate);
      goto cleanup;
    }
    if (!TEST_CHECK(info->channels > 0)) {
      goto cleanup;
    }
    NATIVE_CHAR filename[512];
    ov_snprintf(filename, 512, NULL, NSTR("%s.wav"), test->name);
    file_out = test_util_create_wave_file(filename, (size_t)info->samples, info->channels, info->sample_rate);
    if (!TEST_CHECK(file_out)) {
      goto cleanup;
    }

    float const *const *pcm = NULL;
    size_t total_read = 0;
    while (total_read < info->samples) {
      size_t read = 0;
      err = ovl_audio_decoder_read(d, &pcm, &read);
      if (!TEST_SUCCEEDED_F(err)) {
        goto cleanup;
      }
      if (read == 0) {
        break;
      }
      if (!TEST_CHECK(test_util_write_wave_body_float(file_out, pcm, read, info->channels))) {
        goto cleanup;
      }
      total_read += read;
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

    TEST_CHECK(total_read == info->samples);
    TEST_MSG("total_read = %zu / samples = %" PRIu64, total_read, info->samples);

  cleanup:
    if (file_out) {
      test_util_close_wave(file_out);
    }
    if (d) {
      ovl_audio_decoder_destroy(&d);
    }
    if (source) {
      ovl_source_destroy(&source);
    }
  }
}

static void seek(void) {
  struct ovl_source *source = NULL;
  struct ovl_audio_decoder *d = NULL;
  struct ovl_file *file_out = NULL;
  error err = ovl_source_file_create(TESTDATADIR NSTR("/test-8khz-stereo-8.wav"), &source);
  if (!TEST_SUCCEEDED_F(err)) {
    goto cleanup;
  }
  err = ovl_audio_decoder_wav_create(source, &d);
  if (!TEST_SUCCEEDED_F(err)) {
    goto cleanup;
  }
  struct ovl_audio_info const *info = ovl_audio_decoder_get_info(d);
  uint64_t half = info->samples / 2;
  err = ovl_audio_decoder_seek(d, half);
  if (!TEST_SUCCEEDED_F(err)) {
    goto cleanup;
  }
  file_out = test_util_create_wave_file(
      NSTR("wav-seek.wav"), (size_t)(info->samples - half), info->channels, info->sample_rate);
  if (!TEST_CHECK(file_out)) {
    goto cleanup;
  }

  float const *const *pcm = NULL;
  uint64_t total_read = 0;
  while (total_read < (info->samples - half)) {
    size_t got = 0;
    err = ovl_audio_decoder_read(d, &pcm, &got);
    if (!TEST_SUCCEEDED_F(err)) {
      goto cleanup;
    }
    if (got == 0) {
      break;
    }
    TEST_CHECK(test_util_write_wave_body_float(file_out, pcm, got, info->channels));
    total_read += got;
  }
  TEST_CHECK(total_read == (info->samples - half));

cleanup:
  if (file_out) {
    test_util_close_wave(file_out);
  }
  if (d) {
    ovl_audio_decoder_destroy(&d);
  }
  if (source) {
    ovl_source_destroy(&source);
  }
}

static void get_info(void) {
  static struct test_case {
    char const *name;
    NATIVE_CHAR const *input;
  } const tests[] = {
      {"wav-smpl", TESTDATADIR NSTR("/test-8khz-mono-8-loop-smpl.wav")},
      {"wav-id3", TESTDATADIR NSTR("/test-8khz-mono-8-loop-id3.wav")},
      {"rf64-id3", TESTDATADIR NSTR("/test-8khz-mono-8-rf64-loop-id3.wav")},
      {"w64-id3", TESTDATADIR NSTR("/test-8khz-mono-8-loop-id3.w64")},
      {"aiff-id3", TESTDATADIR NSTR("/test-8khz-mono-8-loop-id3.aiff")},
  };
  for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
    struct test_case const *const test = &tests[i];
    TEST_CASE(test->name);
    struct ovl_audio_decoder *d = NULL;
    struct ovl_source *source = NULL;

    error err = ovl_source_file_create(test->input, &source);
    if (!TEST_SUCCEEDED_F(err)) {
      goto cleanup;
    }

    err = ovl_audio_decoder_wav_create(source, &d);
    if (!TEST_SUCCEEDED_F(err)) {
      goto cleanup;
    }

    struct ovl_audio_info const *info = ovl_audio_decoder_get_info(d);
    static char const title_expected[] = "タイトル";
    static char const artist_expected[] = "アーティスト";
    static uint64_t const start_expected = 5803;
    static uint64_t const end_expected = 11057;
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

  cleanup:
    if (d) {
      ovl_audio_decoder_destroy(&d);
    }
    if (source) {
      ovl_source_destroy(&source);
    }
  }
}

TEST_LIST = {
    {"various_formats", various_formats},
    {"seek", seek},
    {"get_info", get_info},
    {NULL, NULL},
};
