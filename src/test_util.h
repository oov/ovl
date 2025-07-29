#pragma once

#include <ovbase.h>

#include <stdio.h>

#include <ovl/file.h>

static inline enum ovl_file_seek_method test_util_convert_seek_method(int const whence) {
  switch (whence) {
  case SEEK_SET:
    return ovl_file_seek_method_set;
  case SEEK_CUR:
    return ovl_file_seek_method_cur;
  case SEEK_END:
    return ovl_file_seek_method_end;
  }
  return ovl_file_seek_method_set;
}

struct test_util_decoded_audio {
  float **buffer;
  size_t samples;
  size_t channels;
  size_t sample_rate;
};

struct test_util_wave_diff_count {
  size_t mismatches;
  size_t large_diff_count;
  size_t total_samples;
};

void test_util_wave_diff_counter(struct test_util_wave_diff_count *const count,
                                 float const *const *const a,
                                 float const *const *const b,
                                 size_t const samples,
                                 size_t const channels);

struct ovl_file *test_util_create_wave_file(NATIVE_CHAR const *const filepath,
                                        size_t const samples,
                                        size_t const channels,
                                        size_t const sample_rate);
void test_util_close_wave(struct ovl_file *file);

bool test_util_write_wave_body_i16(struct ovl_file *file,
                                   int16_t const *const data,
                                   size_t const samples,
                                   size_t const channels);
bool test_util_write_wave_body_float(struct ovl_file *file,
                                     float const *const *const data,
                                     size_t const samples,
                                     size_t const channels);
bool test_util_write_wave_body_double(struct ovl_file *file,
                                      double const *const *const data,
                                      size_t const samples,
                                      size_t const channels);
