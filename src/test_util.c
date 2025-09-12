#include "test_util.h"

#define CREATE_WAVE_FILE 1

#include <math.h>

void test_util_wave_diff_counter(struct test_util_wave_diff_count *const count,
                                 float const *const *const a,
                                 float const *const *const b,
                                 size_t const samples,
                                 size_t const channels) {
  for (size_t i = 0; i < samples; ++i) {
    for (size_t c = 0; c < channels; ++c) {
      float diff = fabsf(a[c][i] - b[c][i]);
      if (diff > 0.001f) {
        ++count->mismatches;
        if (diff > 0.0075f) {
          ++count->large_diff_count;
        }
      }
    }
  }
  count->total_samples += samples * channels;
}

#if CREATE_WAVE_FILE
struct ovl_file *test_util_create_wave_file(NATIVE_CHAR const *const filepath,
                                            size_t const samples,
                                            size_t const channels,
                                            size_t const sample_rate) {
  struct ovl_file *file = NULL;
  if (!ovl_file_create(filepath, &file, NULL)) {
    return NULL;
  }
  size_t const bytes = samples * channels * sizeof(int16_t);
  size_t written = 0;
  struct __attribute__((packed)) wave_header {
    char riff_id[4];
    uint32_t riff_bytes;
    char wave_id[4];
    char fmt_id[4];
    uint32_t fmt_bytes;
    struct fmt {
      uint16_t audio_format;
      uint16_t num_channels;
      uint32_t sample_rate;
      uint32_t byte_rate;
      uint16_t block_align;
      uint16_t bits_per_sample;
    } fmt;
    char data_id[4];
    uint32_t data_bytes;
  } hdr = {
      .riff_id = {'R', 'I', 'F', 'F'},
      .riff_bytes = (uint32_t)(sizeof(struct wave_header) - 4 - 4 + bytes),
      .wave_id = {'W', 'A', 'V', 'E'},
      .fmt_id = {'f', 'm', 't', ' '},
      .fmt_bytes = sizeof(struct fmt),
      .fmt =
          {
              .audio_format = 1,
              .num_channels = (uint16_t)channels,
              .sample_rate = (uint32_t)sample_rate,
              .byte_rate = (uint32_t)(sample_rate * channels * 16 / 8),
              .block_align = (uint16_t)(channels * 16 / 8),
              .bits_per_sample = 16,
          },
      .data_id = {'d', 'a', 't', 'a'},
      .data_bytes = (uint32_t)bytes,
  };

  if (!ovl_file_write(file, &hdr, sizeof(struct wave_header), &written, NULL)) {
    ovl_file_close(file);
    return NULL;
  }
  if (written != sizeof(struct wave_header)) {
    ovl_file_close(file);
    return NULL;
  }
  return file;
}

void test_util_close_wave(struct ovl_file *file) {
  if (file) {
    ovl_file_close(file);
  }
}

bool test_util_write_wave_body_i16(struct ovl_file *file,
                                   int16_t const *const data,
                                   size_t const samples,
                                   size_t const channels) {
  size_t written = 0;
  size_t to_write = samples * channels * sizeof(int16_t);
  if (!ovl_file_write(file, data, to_write, &written, NULL)) {
    return false;
  }
  if (written != to_write) {
    return false;
  }
  return true;
}

bool test_util_write_wave_body_float(struct ovl_file *file,
                                     float const *const *const data,
                                     size_t const samples,
                                     size_t const channels) {
  enum { buffer_size = 4096 };
  int16_t buf[buffer_size];
  size_t done = 0;
  size_t written = 0;
  while (done < samples) {
    size_t const block = (samples - done) < (buffer_size / channels) ? (samples - done) : (buffer_size / channels);
    for (size_t i = 0; i < block; ++i) {
      for (size_t c = 0; c < channels; ++c) {
        float val = data[c][done + i];
        if (val > 1.0f) {
          val = 1.0f;
        }
        if (val < -1.0f) {
          val = -1.0f;
        }
        buf[i * channels + c] = (int16_t)(val * 32767.f);
      }
    }
    size_t block_bytes = block * channels * sizeof(int16_t);
    if (!ovl_file_write(file, buf, block_bytes, &written, NULL)) {
      return false;
    }
    if (written != block_bytes) {
      return false;
    }
    done += block;
  }
  return true;
}

bool test_util_write_wave_body_double(struct ovl_file *file,
                                      double const *const *const data,
                                      size_t const samples,
                                      size_t const channels) {
  enum { buffer_size = 4096 };
  int16_t buf[buffer_size];
  size_t done = 0;
  size_t written = 0;
  while (done < samples) {
    size_t const block = (samples - done) < (buffer_size / channels) ? (samples - done) : (buffer_size / channels);
    for (size_t i = 0; i < block; ++i) {
      for (size_t c = 0; c < channels; ++c) {
        double val = data[c][done + i];
        if (val > 1.0) {
          val = 1.0;
        }
        if (val < -1.0) {
          val = -1.0;
        }
        buf[i * channels + c] = (int16_t)(val * 32767.);
      }
    }
    size_t block_bytes = block * channels * sizeof(int16_t);
    struct ov_error err = {0};
    if (!ovl_file_write(file, buf, block_bytes, &written, &err)) {
      OV_ERROR_REPORT(&err, NULL);
      return false;
    }
    if (written != block_bytes) {
      return false;
    }
    done += block;
  }
  return true;
}
#else
struct file *test_util_create_wave_file(NATIVE_CHAR const *const filepath,
                                        size_t const samples,
                                        size_t const channels,
                                        size_t const sample_rate) {
  (void)filepath;
  (void)samples;
  (void)channels;
  (void)sample_rate;
  return (struct file *)1;
}
void test_util_close_wave(struct file *file) { (void)file; }
bool test_util_write_wave_body_i16(struct file *file,
                                   int16_t const *data,
                                   size_t const samples,
                                   size_t const channels) {
  (void)file;
  (void)data;
  (void)samples;
  (void)channels;
  return true;
}
bool test_util_write_wave_body_float(struct file *file,
                                     float const *const *const data,
                                     size_t const samples,
                                     size_t const channels) {
  (void)file;
  (void)data;
  (void)samples;
  (void)channels;
  return true;
}
bool test_util_write_wave_body_double(struct file *file,
                                      double const *const *const data,
                                      size_t const samples,
                                      size_t const channels) {
  (void)file;
  (void)data;
  (void)samples;
  (void)channels;
  return true;
}
#endif
