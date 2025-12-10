#include <ovtest.h>

#ifndef TESTDATADIR
#  define TESTDATADIR NSTR(".")
#endif

#include "../../test_util.h"
#include <ovl/audio/decoder.h>
#include <ovl/audio/decoder/bidi.h>
#include <ovl/audio/decoder/ogg.h>
#include <ovl/audio/info.h>
#include <ovl/source.h>
#include <ovl/source/file.h>

static inline size_t adjust_align8(size_t const size) { return (size + UINT64_C(7)) & ~UINT64_C(7); }

static void all(void) {
  struct ovl_source *source = NULL;
  struct ovl_audio_decoder *ogg = NULL;
  struct ovl_audio_decoder *bidi = NULL;
  float **golden = NULL;
  float **buffer = NULL;
  struct ovl_file *golden_file = NULL;
  struct ovl_file *file = NULL;
  struct ov_error err = {0};

  if (!TEST_SUCCEEDED(ovl_source_file_create(TESTDATADIR NSTR("/test.ogg"), &source, &err), &err)) {
    goto cleanup;
  }

  if (!TEST_SUCCEEDED(ovl_audio_decoder_ogg_create(source, &ogg, &err), &err)) {
    goto cleanup;
  }

  {
    struct ovl_audio_info const *info = ovl_audio_decoder_get_info(ogg);
    size_t const channels = info->channels;
    size_t const total_samples = (size_t)info->samples;
    size_t const total_samples_aligned = adjust_align8(total_samples);

    if (!TEST_SUCCEEDED(ovl_audio_decoder_bidi_create(ogg, &bidi, &err), &err)) {
      goto cleanup;
    }
    ovl_audio_decoder_bidi_set_direction(bidi, true);

    if (!TEST_CHECK(OV_REALLOC(&golden, channels, sizeof(float *)))) {
      OV_ERROR_SET_GENERIC(&err, ov_error_generic_out_of_memory);
      goto cleanup;
    }
    golden[0] = NULL;
    if (!TEST_CHECK(OV_ALIGNED_ALLOC(&golden[0], channels * total_samples_aligned, sizeof(float), 16))) {
      OV_ERROR_SET_GENERIC(&err, ov_error_generic_out_of_memory);
      goto cleanup;
    }
    for (size_t ch = 1; ch < channels; ch++) {
      golden[ch] = golden[ch - 1] + total_samples_aligned;
    }
    if (!TEST_SUCCEEDED(ovl_audio_decoder_seek(ogg, 0, &err), &err)) {
      goto cleanup;
    }
    {
      size_t offset = 0;
      while (offset < total_samples) {
        size_t read;
        float const *const *pcm = NULL;
        if (!TEST_SUCCEEDED(ovl_audio_decoder_read(ogg, &pcm, &read, &err), &err)) {
          break;
        }
        if (read == 0) {
          break;
        }
        for (size_t ch = 0; ch < channels; ch++) {
          for (size_t i = 0; i < read; i++) {
            golden[ch][total_samples - 1 - offset - i] = pcm[ch][i];
          }
        }
        offset += read;
      }
      TEST_CHECK(offset == total_samples);
    }

    if (!TEST_CHECK(OV_REALLOC(&buffer, channels, sizeof(float *)))) {
      OV_ERROR_SET_GENERIC(&err, ov_error_generic_out_of_memory);
      goto cleanup;
    }
    buffer[0] = NULL;
    if (!TEST_CHECK(OV_ALIGNED_ALLOC(&buffer[0], channels * total_samples_aligned, sizeof(float), 16))) {
      OV_ERROR_SET_GENERIC(&err, ov_error_generic_out_of_memory);
      goto cleanup;
    }
    for (size_t ch = 1; ch < channels; ch++) {
      buffer[ch] = buffer[ch - 1] + total_samples_aligned;
    }

    if (!TEST_SUCCEEDED(ovl_audio_decoder_seek(bidi, total_samples, &err), &err)) {
      goto cleanup;
    }

    size_t offset = 0;
    while (offset < total_samples) {
      size_t read;
      float const *const *pcm = NULL;
      if (!TEST_SUCCEEDED(ovl_audio_decoder_read(bidi, &pcm, &read, &err), &err)) {
        break;
      }
      if (read == 0) {
        break;
      }
      size_t const sz = offset + read > total_samples ? total_samples - offset : read;
      for (size_t ch = 0; ch < channels; ch++) {
        memcpy(buffer[ch] + offset, pcm[ch], sz * sizeof(float));
      }
      offset += read;
    }
    TEST_CHECK(offset == total_samples);

    struct test_util_wave_diff_count diff = {0};
    test_util_wave_diff_counter(&diff, (float const *const *)golden, (float const *const *)buffer, total_samples, 1);
    TEST_CHECK(diff.mismatches == 0 && diff.large_diff_count == 0);
    TEST_MSG("Waveform differences: mismatches=%zu, large_diff_count=%zu, "
             "total_samples=%zu",
             diff.mismatches,
             diff.large_diff_count,
             diff.total_samples);

    golden_file = test_util_create_wave_file(NSTR("bidi-all-golden.wav"), total_samples, channels, info->sample_rate);
    if (!TEST_CHECK(golden_file)) {
      goto cleanup;
    }
    if (!TEST_CHECK(
            test_util_write_wave_body_float(golden_file, (float const *const *)golden, total_samples, channels))) {
      goto cleanup;
    }
    file = test_util_create_wave_file(NSTR("bidi-all.wav"), total_samples, channels, info->sample_rate);
    if (!TEST_CHECK(file)) {
      goto cleanup;
    }
    if (!TEST_CHECK(test_util_write_wave_body_float(file, (float const *const *)buffer, total_samples, channels))) {
      goto cleanup;
    }
  }
cleanup:
  if (file) {
    test_util_close_wave(file);
  }
  if (golden_file) {
    test_util_close_wave(golden_file);
  }
  if (buffer) {
    OV_ALIGNED_FREE(&buffer[0]);
    OV_FREE(&buffer);
  }
  if (golden) {
    OV_ALIGNED_FREE(&golden[0]);
    OV_FREE(&golden);
  }
  if (bidi) {
    ovl_audio_decoder_destroy(&bidi);
  }
  if (ogg) {
    ovl_audio_decoder_destroy(&ogg);
  }
  if (source) {
    ovl_source_destroy(&source);
  }
}

static void seek(void) {
  struct ovl_source *source = NULL;
  struct ovl_audio_decoder *ogg = NULL;
  struct ovl_audio_decoder *bidi = NULL;
  float **golden = NULL;
  float **buffer = NULL;
  struct ov_error err = {0};

  {
    if (!TEST_SUCCEEDED(ovl_source_file_create(TESTDATADIR NSTR("/test.ogg"), &source, &err), &err)) {
      goto cleanup;
    }
    if (!TEST_SUCCEEDED(ovl_audio_decoder_ogg_create(source, &ogg, &err), &err)) {
      goto cleanup;
    }
    if (!TEST_SUCCEEDED(ovl_audio_decoder_bidi_create(ogg, &bidi, &err), &err)) {
      goto cleanup;
    }

    struct ovl_audio_info const *info = ovl_audio_decoder_get_info(ogg);
    if (TEST_CHECK(info != NULL)) {
      TEST_CHECK(info->sample_rate == 48000);
      TEST_CHECK(info->channels == 2);
    }

    size_t const channels = info->channels;
    size_t const total_samples = (size_t)info->samples;

    size_t const total_aligned = adjust_align8(total_samples);
    if (!TEST_CHECK(OV_REALLOC(&golden, channels, sizeof(float *)))) {
      OV_ERROR_SET_GENERIC(&err, ov_error_generic_out_of_memory);
      goto cleanup;
    }
    golden[0] = NULL;
    if (!TEST_CHECK(OV_ALIGNED_ALLOC(&golden[0], channels * total_aligned, sizeof(float), 16))) {
      OV_ERROR_SET_GENERIC(&err, ov_error_generic_out_of_memory);
      goto cleanup;
    }
    for (size_t ch = 1; ch < channels; ch++) {
      golden[ch] = golden[ch - 1] + total_aligned;
    }
    if (!TEST_SUCCEEDED(ovl_audio_decoder_seek(ogg, 0, &err), &err)) {
      goto cleanup;
    }
    {
      size_t offset = 0;
      while (offset < total_samples) {
        size_t read;
        float const *const *pcm = NULL;
        if (!TEST_SUCCEEDED(ovl_audio_decoder_read(ogg, &pcm, &read, &err), &err)) {
          break;
        }
        if (read == 0) {
          break;
        }
        for (size_t ch = 0; ch < channels; ch++) {
          memcpy(golden[ch] + offset, pcm[ch], read * sizeof(float));
        }
        offset += read;
      }
      TEST_CHECK(offset == total_samples);
    }

    size_t const seg_count = info->sample_rate / 10;
    size_t const seg_aligned = adjust_align8(seg_count);
    if (!TEST_CHECK(OV_REALLOC(&buffer, channels, sizeof(float *)))) {
      OV_ERROR_SET_GENERIC(&err, ov_error_generic_out_of_memory);
      goto cleanup;
    }
    buffer[0] = NULL;
    if (!TEST_CHECK(OV_ALIGNED_ALLOC(&buffer[0], channels * seg_aligned, sizeof(float), 16))) {
      OV_ERROR_SET_GENERIC(&err, ov_error_generic_out_of_memory);
      goto cleanup;
    }
    for (size_t ch = 1; ch < channels; ch++) {
      buffer[ch] = buffer[ch - 1] + seg_aligned;
    }

    struct test_case {
      char const *msg;
      size_t pos;
      bool reverse;
    } const tests[] = {
        {"Forward_13p", total_samples * 13 / 100, false},
        {"Reverse_27p", total_samples * 27 / 100, true},
        {"Forward_33p", total_samples * 33 / 100, false},
        {"Reverse_47p", total_samples * 47 / 100, true},
        {"Forward_55p", total_samples * 55 / 100, false},
        {"Reverse_63p", total_samples * 63 / 100, true},
        {"Forward_77p", total_samples * 77 / 100, false},
        {"Reverse_89p", total_samples * 89 / 100, true},
    };
    for (size_t tc = 0; tc < sizeof(tests) / sizeof(tests[0]); tc++) {
      struct test_case const *const test = tests + tc;
      TEST_CASE(test->msg);
      ovl_audio_decoder_bidi_set_direction(bidi, test->reverse);
      if (!TEST_SUCCEEDED(ovl_audio_decoder_seek(bidi, test->pos, &err), &err)) {
        goto cleanup;
      }

      {
        size_t offset = 0;
        while (offset < seg_count) {
          size_t read;
          float const *const *pcm = NULL;
          if (!TEST_SUCCEEDED(ovl_audio_decoder_read(bidi, &pcm, &read, &err), &err)) {
            break;
          }
          if (read == 0) {
            break;
          }
          size_t sz = offset + read > seg_count ? seg_count - offset : read;
          for (size_t ch = 0; ch < channels; ch++) {
            memcpy(buffer[ch] + offset, pcm[ch], sz * sizeof(float));
          }
          offset += sz;
        }
        TEST_CHECK(offset == seg_count);
      }

      if (test->reverse) {
        for (size_t ch = 0; ch < channels; ch++) {
          for (size_t i = 0; i < seg_count / 2; i++) {
            float tmp = buffer[ch][i];
            buffer[ch][i] = buffer[ch][seg_count - 1 - i];
            buffer[ch][seg_count - 1 - i] = tmp;
          }
        }
      }

      struct test_util_wave_diff_count diff = {0};
      size_t const offset = test->reverse ? test->pos - seg_count : test->pos;
      test_util_wave_diff_counter(&diff,
                                  (float const *[2]){golden[0] + offset, golden[1] + offset},
                                  (float const *const *)buffer,
                                  seg_count,
                                  2);
      TEST_CHECK(diff.mismatches == 0 && diff.large_diff_count == 0);
      TEST_MSG("%s: mismatches=%zu, large_diff_count=%zu, samples=%zu",
               test->msg,
               diff.mismatches,
               diff.large_diff_count,
               diff.total_samples);
    }
  }
cleanup:
  if (buffer) {
    OV_ALIGNED_FREE(&buffer[0]);
    OV_FREE(&buffer);
  }
  if (golden) {
    OV_ALIGNED_FREE(&golden[0]);
    OV_FREE(&golden);
  }
  if (bidi) {
    ovl_audio_decoder_destroy(&bidi);
  }
  if (ogg) {
    ovl_audio_decoder_destroy(&ogg);
  }
  if (source) {
    ovl_source_destroy(&source);
  }
}

static void direction_change(void) {
  struct ovl_source *source = NULL;
  struct ovl_audio_decoder *ogg = NULL;
  struct ovl_audio_decoder *bidi = NULL;
  float **temp = NULL;
  float **golden = NULL;
  float **buffer = NULL;
  struct ovl_file *golden_file = NULL;
  struct ovl_file *file = NULL;
  struct ov_error err = {0};

  {
    if (!TEST_SUCCEEDED(ovl_source_file_create(TESTDATADIR NSTR("/test.ogg"), &source, &err), &err)) {
      goto cleanup;
    }

    if (!TEST_SUCCEEDED(ovl_audio_decoder_ogg_create(source, &ogg, &err), &err)) {
      goto cleanup;
    }

    struct ovl_audio_info const *info = ovl_audio_decoder_get_info(ogg);
    TEST_CHECK(info != NULL);

    size_t const channels = info->channels;
    size_t const sample_rate = info->sample_rate;

    if (!TEST_SUCCEEDED(ovl_audio_decoder_bidi_create(ogg, &bidi, &err), &err)) {
      goto cleanup;
    }

    size_t const forward_duration1 = sample_rate * 3;
    size_t const backward_duration = sample_rate * 2;
    size_t const forward_duration2 = sample_rate * 3;
    size_t const total_duration = forward_duration1 + backward_duration + forward_duration2;
    size_t const total_duration_aligned = adjust_align8(total_duration);
    size_t const input_length = forward_duration1 + forward_duration2;
    size_t const input_length_aligned = adjust_align8(input_length);

    if (!TEST_CHECK(OV_REALLOC(&temp, channels, sizeof(float *)))) {
      OV_ERROR_SET_GENERIC(&err, ov_error_generic_out_of_memory);
      goto cleanup;
    }
    temp[0] = NULL;
    if (!TEST_CHECK(OV_ALIGNED_ALLOC(&temp[0], channels * input_length_aligned, sizeof(float), 16))) {
      OV_ERROR_SET_GENERIC(&err, ov_error_generic_out_of_memory);
      goto cleanup;
    }
    for (size_t ch = 1; ch < channels; ch++) {
      temp[ch] = temp[ch - 1] + input_length_aligned;
    }

    if (!TEST_CHECK(OV_REALLOC(&golden, channels, sizeof(float *)))) {
      OV_ERROR_SET_GENERIC(&err, ov_error_generic_out_of_memory);
      goto cleanup;
    }
    golden[0] = NULL;
    if (!TEST_CHECK(OV_ALIGNED_ALLOC(&golden[0], channels * total_duration_aligned, sizeof(float), 16))) {
      OV_ERROR_SET_GENERIC(&err, ov_error_generic_out_of_memory);
      goto cleanup;
    }
    for (size_t ch = 1; ch < channels; ch++) {
      golden[ch] = golden[ch - 1] + total_duration_aligned;
    }

    if (!TEST_SUCCEEDED(ovl_audio_decoder_seek(ogg, 0, &err), &err)) {
      goto cleanup;
    }
    {
      size_t copied = 0;
      while (copied < input_length) {
        size_t read;
        float const *const *pcm = NULL;
        if (!TEST_SUCCEEDED(ovl_audio_decoder_read(ogg, &pcm, &read, &err), &err)) {
          goto cleanup;
        }
        if (read == 0) {
          break;
        }
        size_t const sz = (copied + read > input_length) ? input_length - copied : read;
        for (size_t ch = 0; ch < channels; ch++) {
          memcpy(temp[ch] + copied, pcm[ch], sz * sizeof(float));
        }
        copied += sz;
      }
      TEST_CHECK(copied == input_length);
      TEST_MSG("Copied sample count expected %zu, got %zu", input_length, copied);

      for (size_t ch = 0; ch < channels; ch++) {
        memcpy(golden[ch], temp[ch], forward_duration1 * sizeof(float));
        for (size_t i = 0; i < backward_duration; i++) {
          golden[ch][forward_duration1 + i] = temp[ch][forward_duration1 - 1 - i];
        }
        memcpy(golden[ch] + forward_duration1 + backward_duration,
               temp[ch] + (forward_duration1 - backward_duration),
               forward_duration2 * sizeof(float));
      }
    }

    if (!TEST_CHECK(OV_REALLOC(&buffer, channels, sizeof(float *)))) {
      OV_ERROR_SET_GENERIC(&err, ov_error_generic_out_of_memory);
      goto cleanup;
    }
    buffer[0] = NULL;
    if (!TEST_CHECK(OV_ALIGNED_ALLOC(&buffer[0], channels * total_duration_aligned, sizeof(float), 16))) {
      OV_ERROR_SET_GENERIC(&err, ov_error_generic_out_of_memory);
      goto cleanup;
    }
    for (size_t ch = 1; ch < channels; ch++) {
      buffer[ch] = buffer[ch - 1] + total_duration_aligned;
    }

    ovl_audio_decoder_bidi_set_direction(bidi, false);
    if (!TEST_SUCCEEDED(ovl_audio_decoder_seek(ogg, 0, &err), &err)) {
      goto cleanup;
    }

    {
      size_t total = 0;
      size_t copied = 0;
      size_t read;
      float const *const *pcm = NULL;
      while (copied < forward_duration1) {
        if (!TEST_SUCCEEDED(ovl_audio_decoder_read(bidi, &pcm, &read, &err), &err)) {
          goto cleanup;
        }
        if (read == 0) {
          break;
        }
        size_t const sz = copied + read > forward_duration1 ? forward_duration1 - copied : read;
        for (size_t ch = 0; ch < channels; ch++) {
          memcpy(buffer[ch] + total + copied, pcm[ch], sz * sizeof(float));
        }
        copied += sz;
      }
      total += copied;

      ovl_audio_decoder_bidi_set_direction(bidi, true);
      if (!TEST_SUCCEEDED(ovl_audio_decoder_seek(bidi, forward_duration1, &err), &err)) {
        goto cleanup;
      }
      copied = 0;
      while (copied < backward_duration) {
        if (!TEST_SUCCEEDED(ovl_audio_decoder_read(bidi, &pcm, &read, &err), &err)) {
          goto cleanup;
        }
        if (read == 0) {
          break;
        }
        size_t const sz = copied + read > backward_duration ? backward_duration - copied : read;
        for (size_t ch = 0; ch < channels; ch++) {
          memcpy(buffer[ch] + total + copied, pcm[ch], sz * sizeof(float));
        }
        copied += sz;
      }
      total += copied;

      ovl_audio_decoder_bidi_set_direction(bidi, false);
      if (!TEST_SUCCEEDED(ovl_audio_decoder_seek(bidi, forward_duration1 - backward_duration, &err), &err)) {
        goto cleanup;
      }
      copied = 0;
      while (copied < forward_duration2) {
        if (!TEST_SUCCEEDED(ovl_audio_decoder_read(bidi, &pcm, &read, &err), &err)) {
          goto cleanup;
        }
        if (read == 0) {
          break;
        }
        size_t const sz = copied + read > forward_duration2 ? forward_duration2 - copied : read;
        for (size_t ch = 0; ch < channels; ch++) {
          memcpy(buffer[ch] + total + copied, pcm[ch], sz * sizeof(float));
        }
        copied += sz;
      }
      total += copied;
      TEST_CHECK(total == total_duration);
      TEST_MSG("Final copied sample count expected %zu, got %zu", total_duration, total);
    }

    golden_file = test_util_create_wave_file(
        NSTR("bidi-direction-change-golden.wav"), total_duration, channels, info->sample_rate);
    if (!TEST_CHECK(golden_file)) {
      goto cleanup;
    }
    if (!TEST_CHECK(
            test_util_write_wave_body_float(golden_file, (float const *const *)golden, total_duration, channels))) {
      goto cleanup;
    }
    file = test_util_create_wave_file(NSTR("bidi-direction-change.wav"), total_duration, channels, info->sample_rate);
    if (!TEST_CHECK(file)) {
      goto cleanup;
    }
    if (!TEST_CHECK(test_util_write_wave_body_float(file, (float const *const *)buffer, total_duration, channels))) {
      goto cleanup;
    }

    struct test_util_wave_diff_count diff = {0};
    test_util_wave_diff_counter(&diff, (float const *const *)golden, (float const *const *)buffer, total_duration, 1);
    TEST_CHECK(diff.mismatches == 0 && diff.large_diff_count == 0);
    TEST_MSG("Direction Change Waveform differences: mismatches=%zu, "
             "large_diff_count=%zu, total_samples=%zu",
             diff.mismatches,
             diff.large_diff_count,
             diff.total_samples);
  }
cleanup:
  if (file) {
    test_util_close_wave(file);
  }
  if (golden_file) {
    test_util_close_wave(golden_file);
  }
  if (buffer) {
    OV_ALIGNED_FREE(&buffer[0]);
    OV_FREE(&buffer);
  }
  if (golden) {
    OV_ALIGNED_FREE(&golden[0]);
    OV_FREE(&golden);
  }
  if (temp) {
    OV_ALIGNED_FREE(&temp[0]);
    OV_FREE(&temp);
  }
  if (bidi) {
    ovl_audio_decoder_destroy(&bidi);
  }
  if (ogg) {
    ovl_audio_decoder_destroy(&ogg);
  }
  if (source) {
    ovl_source_destroy(&source);
  }
}

TEST_LIST = {
    {"all", all},
    {"seek", seek},
    {"direction_change", direction_change},
    {NULL, NULL},
};
