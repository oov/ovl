#pragma once

#include <assert.h>
#include <memory.h>
#include <stdbool.h>
#include <stdint.h>

static inline uint64_t adjust_align2(uint64_t const size) { return (size + UINT64_C(1)) & ~UINT64_C(1); }
static inline uint64_t adjust_align8(uint64_t const size) { return (size + UINT64_C(7)) & ~UINT64_C(7); }

static inline bool is_little_endian(void) {
#if (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) ||                                          \
    (defined(_M_ENDIAN) && _M_ENDIAN == 0) || defined(__LITTLE_ENDIAN__)
#  ifndef __LITTLE_ENDIAN__
#    define __LITTLE_ENDIAN__
#  endif
  return true;
#elif (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__) || (defined(_M_ENDIAN) && _M_ENDIAN == 1) || \
    defined(__BIG_ENDIAN__)
#  ifndef __BIG_ENDIAN__
#    define __BIG_ENDIAN__
#  endif
  return false;
#else
  return (union {
           uint32_t u;
           uint8_t c[sizeof(uint32_t)];
         }){.u = 1}
             .c[0] == 1;
#endif
}

static inline uint16_t swap16(uint16_t v) {
#if defined(__clang__) || defined(__GNUC__)
  return __builtin_bswap16(v);
#elif defined(_MSC_VER)
  return _byteswap_ushort(v);
#else
  // clang-format off
  return (
    ((v >> 8) & UINT16_C(0x00ff)) |
    ((v << 8) & UINT16_C(0xff00))
  );
  // clang-format on
#endif
}

static inline uint32_t swap32(uint32_t v) {
#if defined(__clang__) || defined(__GNUC__)
  return __builtin_bswap32(v);
#elif defined(_MSC_VER)
  return _byteswap_ulong(v);
#else
  // clang-format off
  return (
    ((v >> 24) & UINT32_C(0x000000ff)) |
    ((v >>  8) & UINT32_C(0x0000ff00)) |
    ((v <<  8) & UINT32_C(0x00ff0000)) |
    ((v << 24) & UINT32_C(0xff000000))
  );
  // clang-format on
#endif
}

static inline uint64_t swap64(uint64_t v) {
#if defined(__clang__) || defined(__GNUC__)
  return __builtin_bswap64(v);
#elif defined(_MSC_VER)
  return _byteswap_uint64(v);
#else
  // clang-format off
  return (
    ((v >> 56) & UINT64_C(0x00000000000000ff)) | ((v >> 40) & UINT64_C(0x000000000000ff00)) |
    ((v >> 24) & UINT64_C(0x0000000000ff0000)) | ((v >>  8) & UINT64_C(0x00000000ff000000)) |
    ((v <<  8) & UINT64_C(0x000000ff00000000)) | ((v << 24) & UINT64_C(0x0000ff0000000000)) |
    ((v << 40) & UINT64_C(0x00ff000000000000)) | ((v << 56) & UINT64_C(0xff00000000000000))
  );
  // clang-format on
#endif
}

#ifdef __LITTLE_ENDIAN__
#  define SWAP_LE(x, swap_fn) (x)
#  define SWAP_BE(x, swap_fn) (swap_fn(x))
#elif defined(__BIG_ENDIAN__)
#  define SWAP_LE(x, swap_fn) (swap_fn(x))
#  define SWAP_BE(x, swap_fn) (x)
#else
#  define SWAP_LE(x, swap_fn) (is_little_endian() ? (x) : (swap_fn(x)))
#  define SWAP_BE(x, swap_fn) (is_little_endian() ? (swap_fn(x)) : (x))
#endif
#define DEFINE_SWAP_FUNCTION(size)                                                                                     \
  static inline uint##size##_t swap##size##le(uint##size##_t v) { return SWAP_LE(v, swap##size); }                     \
  static inline uint##size##_t swap##size##be(uint##size##_t v) { return SWAP_BE(v, swap##size); }

DEFINE_SWAP_FUNCTION(16)
DEFINE_SWAP_FUNCTION(32)
DEFINE_SWAP_FUNCTION(64)

static inline float inv_scale8(void) { return 1.f / (UINT8_C(1) << (sizeof(uint8_t) * 8 - 1)); }
static inline float inv_scale16(void) { return 1.f / (UINT16_C(1) << (sizeof(uint16_t) * 8 - 1)); }
static inline float inv_scale32(void) { return 1.f / (UINT32_C(1) << (sizeof(uint32_t) * 8 - 1)); }
static inline double inv_scale64(void) { return 1. / (UINT64_C(1) << (sizeof(uint64_t) * 8 - 1)); }

static inline float i8_to_float(int8_t const v) { return (float)v * inv_scale8(); }
static inline float u8_to_float(uint8_t const v) { return (float)(v - 128) * inv_scale8(); }
static inline float i16_to_float(int16_t const v) { return (float)v * inv_scale16(); }
static inline float i32_to_float(int32_t const v) { return (float)v * inv_scale32(); }
static inline float i64_to_float(int64_t const v) { return (float)((double)v * inv_scale64()); }

static_assert(sizeof(float) == sizeof(uint32_t), "float and uint32_t must have the same size");
static inline float u32_as_float(uint32_t const v) {
  float r;
  memcpy(&r, &v, sizeof(r));
  return r;
}
static_assert(sizeof(double) == sizeof(uint64_t), "double and uint64_t must have the same size");
static inline double u64_as_double(uint64_t const v) {
  double r;
  memcpy(&r, &v, sizeof(r));
  return r;
}
static inline float u64_as_float(uint64_t const v) { return (float)u64_as_double(v); }

static inline float read_as_u8(uint8_t const *const p) { return u8_to_float(*p); }
static inline float read_as_i8(int8_t const *const p) { return i8_to_float(*p); }
static inline float read_as_i16(int16_t const *const p) { return i16_to_float(*p); }
static inline float read_as_i32(int32_t const *const p) { return i32_to_float(*p); }
static inline float read_as_i64(int64_t const *const p) { return i64_to_float(*p); }
static inline float read_as_f32(uint32_t const *const p) { return u32_as_float(*p); }
static inline float read_as_f64(uint64_t const *const p) { return u64_as_float(*p); }

static inline float read_as_i16le(int16_t const *const p) { return i16_to_float((int16_t)swap16le((uint16_t)*p)); }
static inline float read_as_i16be(int16_t const *const p) { return i16_to_float((int16_t)swap16be((uint16_t)*p)); }
static inline float read_as_i32le(int32_t const *const p) { return i32_to_float((int32_t)swap32le((uint32_t)*p)); }
static inline float read_as_i32be(int32_t const *const p) { return i32_to_float((int32_t)swap32be((uint32_t)*p)); }
static inline float read_as_i64le(int64_t const *const p) { return i64_to_float((int64_t)swap64le((uint64_t)*p)); }
static inline float read_as_i64be(int64_t const *const p) { return i64_to_float((int64_t)swap64be((uint64_t)*p)); }

static inline float read_as_f32le(uint32_t const *const p) { return u32_as_float(swap32le(*p)); }
static inline float read_as_f32be(uint32_t const *const p) { return u32_as_float(swap32be(*p)); }
static inline float read_as_f64le(uint64_t const *const p) { return u64_as_float(swap64le(*p)); }
static inline float read_as_f64be(uint64_t const *const p) { return u64_as_float(swap64be(*p)); }

static inline float read_as_i24le(uint8_t const *const p) {
  static float const inv = 1.f / 2147483648.f;
  int32_t v = (int32_t)(uint32_t)((p[0] << 8) | (p[1] << 16) | (p[2] << 24));
  return (float)v * inv;
}
static inline float read_as_i24be(uint8_t const *const p) {
  static float const inv = 1.f / 2147483648.f;
  int32_t v = (int32_t)(uint32_t)((p[2] << 8) | (p[1] << 16) | (p[0] << 24));
  return (float)v * inv;
}

static inline double ieee754_extended_be_to_double(uint8_t const *const data10) {
  // Extract sign, exponent and mantissa
  uint16_t const sign_exp = (uint16_t)((data10[0] << 8) | data10[1]);
  int const sign = sign_exp >> 15;
  int exponent = sign_exp & 0x7fff;
  // Combine 64-bit mantissa
  uint64_t mantissa = ((uint64_t)data10[2] << 56) | ((uint64_t)data10[3] << 48) | ((uint64_t)data10[4] << 40) |
                      ((uint64_t)data10[5] << 32) | ((uint64_t)data10[6] << 24) | ((uint64_t)data10[7] << 16) |
                      ((uint64_t)data10[8] << 8) | ((uint64_t)data10[9]);
  if (exponent == 0 && mantissa == 0) {
    return 0.;
  }
  exponent = exponent - 16383; // Adjust exponent
  int const double_exp_bias = 1023;
  uint64_t double_bits;
  if (exponent < -1022) {
    return 0.; // Too small for double
  }
  if (exponent > double_exp_bias) {
    double_bits = ((uint64_t)sign << 63) | (UINT64_C(0x7ff) << 52); // Too large for double, return infinity
  } else {
    // Normal number
    // Drop the explicit integer bit and shift mantissa to fit double's 52-bit
    // format
    mantissa = (mantissa >> 11) & ((UINT64_C(1) << 52) - 1);
    double_bits = ((uint64_t)sign << 63) | ((uint64_t)(exponent + double_exp_bias) << 52) | mantissa;
  }
  return u64_as_double(double_bits);
}

#define DEFINE_PROCESS_FUNCTION(NAME, TYPE)                                                                            \
  static inline void process_##NAME##_1ch(TYPE const *const src, float *const *const dst, size_t const samples) {      \
    TYPE const *s = src;                                                                                               \
    float *d = dst[0];                                                                                                 \
    float const *const d_end = d + samples;                                                                            \
    while (d < d_end) {                                                                                                \
      *d++ = read_as_##NAME(s++);                                                                                      \
    }                                                                                                                  \
  }                                                                                                                    \
  static inline void process_##NAME##_2ch(TYPE const *const src, float *const *const dst, size_t const samples) {      \
    TYPE const *s = src;                                                                                               \
    float *l = dst[0];                                                                                                 \
    float *r = dst[1];                                                                                                 \
    float const *const r_end = r + samples;                                                                            \
    while (r < r_end) {                                                                                                \
      *l++ = read_as_##NAME(s++);                                                                                      \
      *r++ = read_as_##NAME(s++);                                                                                      \
    }                                                                                                                  \
  }                                                                                                                    \
  static inline void process_##NAME##_multi(                                                                           \
      TYPE const *const src, float *const *const dst, size_t const channels, size_t const samples) {                   \
    for (size_t i = 0; i < samples; i++) {                                                                             \
      for (size_t c = 0; c < channels; c++) {                                                                          \
        dst[c][i] = read_as_##NAME(src + i * channels + c);                                                            \
      }                                                                                                                \
    }                                                                                                                  \
  }                                                                                                                    \
  static inline void process_##NAME(                                                                                   \
      TYPE const *const src, float *const *const dst, size_t const channels, size_t const samples) {                   \
    if (channels == 1) {                                                                                               \
      process_##NAME##_1ch(src, dst, samples);                                                                         \
    } else if (channels == 2) {                                                                                        \
      process_##NAME##_2ch(src, dst, samples);                                                                         \
    } else {                                                                                                           \
      process_##NAME##_multi(src, dst, channels, samples);                                                             \
    }                                                                                                                  \
  }

#define DEFINE_PROCESS_FUNCTION_24(NAME)                                                                               \
  static inline void process_##NAME##_1ch(uint8_t const *const src, float *const *const dst, size_t const samples) {   \
    uint8_t const *s = src;                                                                                            \
    float *d = dst[0];                                                                                                 \
    float const *const d_end = d + samples;                                                                            \
    while (d < d_end) {                                                                                                \
      *d++ = read_as_##NAME(s);                                                                                        \
      s += 3;                                                                                                          \
    }                                                                                                                  \
  }                                                                                                                    \
  static inline void process_##NAME##_2ch(uint8_t const *const src, float *const *const dst, size_t const samples) {   \
    uint8_t const *s = src;                                                                                            \
    float *l = dst[0];                                                                                                 \
    float *r = dst[1];                                                                                                 \
    float const *const r_end = r + samples;                                                                            \
    while (r < r_end) {                                                                                                \
      *l++ = read_as_##NAME(s);                                                                                        \
      s += 3;                                                                                                          \
      *r++ = read_as_##NAME(s);                                                                                        \
      s += 3;                                                                                                          \
    }                                                                                                                  \
  }                                                                                                                    \
  static inline void process_##NAME##_multi(                                                                           \
      uint8_t const *const src, float *const *const dst, size_t const channels, size_t const samples) {                \
    uint8_t const *s = src;                                                                                            \
    for (size_t i = 0; i < samples; i++) {                                                                             \
      for (size_t c = 0; c < channels; c++) {                                                                          \
        dst[c][i] = read_as_##NAME(s);                                                                                 \
        s += 3;                                                                                                        \
      }                                                                                                                \
    }                                                                                                                  \
  }                                                                                                                    \
  static inline void process_##NAME(                                                                                   \
      uint8_t const *const src, float *const *const dst, size_t const channels, size_t const samples) {                \
    if (channels == 1) {                                                                                               \
      process_##NAME##_1ch(src, dst, samples);                                                                         \
    } else if (channels == 2) {                                                                                        \
      process_##NAME##_2ch(src, dst, samples);                                                                         \
    } else {                                                                                                           \
      process_##NAME##_multi(src, dst, channels, samples);                                                             \
    }                                                                                                                  \
  }

DEFINE_PROCESS_FUNCTION(u8, uint8_t)
DEFINE_PROCESS_FUNCTION(i8, int8_t)
DEFINE_PROCESS_FUNCTION(i16le, int16_t)
DEFINE_PROCESS_FUNCTION(i16be, int16_t)
DEFINE_PROCESS_FUNCTION(i32le, int32_t)
DEFINE_PROCESS_FUNCTION(i32be, int32_t)
DEFINE_PROCESS_FUNCTION(i64le, int64_t)
DEFINE_PROCESS_FUNCTION(i64be, int64_t)
DEFINE_PROCESS_FUNCTION(f32le, uint32_t)
DEFINE_PROCESS_FUNCTION(f32be, uint32_t)
DEFINE_PROCESS_FUNCTION(f64le, uint64_t)
DEFINE_PROCESS_FUNCTION(f64be, uint64_t)

DEFINE_PROCESS_FUNCTION_24(i24le)
DEFINE_PROCESS_FUNCTION_24(i24be)

static inline void
process_i16(int16_t const *const src, float *const *const dst, size_t const channels, size_t const samples) {
#if defined(__LITTLE_ENDIAN__)
  process_i16le(src, dst, channels, samples);
#elif defined(__BIG_ENDIAN__)
  process_i16be(src, dst, channels, samples);
#else
  if (is_little_endian()) {
    process_i16le(src, dst, channels, samples);
  } else {
    process_i16be(src, dst, channels, samples);
  }
#endif
}
static inline void
process_i24(uint8_t const *const src, float *const *const dst, size_t const channels, size_t const samples) {
#if defined(__LITTLE_ENDIAN__)
  process_i24le(src, dst, channels, samples);
#elif defined(__BIG_ENDIAN__)
  process_i24be(src, dst, channels, samples);
#else
  if (is_little_endian()) {
    process_i24le(src, dst, channels, samples);
  } else {
    process_i24be(src, dst, channels, samples);
  }
#endif
}

static inline void
process_i32(int32_t const *const src, float *const *const dst, size_t const channels, size_t const samples) {
#if defined(__LITTLE_ENDIAN__)
  process_i32le(src, dst, channels, samples);
#elif defined(__BIG_ENDIAN__)
  process_i32be(src, dst, channels, samples);
#else
  if (is_little_endian()) {
    process_i32le(src, dst, channels, samples);
  } else {
    process_i32be(src, dst, channels, samples);
  }
#endif
}

static inline void
process_i64(int64_t const *const src, float *const *const dst, size_t const channels, size_t const samples) {
#if defined(__LITTLE_ENDIAN__)
  process_i64le(src, dst, channels, samples);
#elif defined(__BIG_ENDIAN__)
  process_i64be(src, dst, channels, samples);
#else
  if (is_little_endian()) {
    process_i64le(src, dst, channels, samples);
  } else {
    process_i64be(src, dst, channels, samples);
  }
#endif
}

static inline void
process_f32(uint32_t const *const src, float *const *const dst, size_t const channels, size_t const samples) {
#if defined(__LITTLE_ENDIAN__)
  process_f32le(src, dst, channels, samples);
#elif defined(__BIG_ENDIAN__)
  process_f32be(src, dst, channels, samples);
#else
  if (is_little_endian()) {
    process_f32le(src, dst, channels, samples);
  } else {
    process_f32be(src, dst, channels, samples);
  }
#endif
}

static inline void
process_f64(uint64_t const *const src, float *const *const dst, size_t const channels, size_t const samples) {
#if defined(__LITTLE_ENDIAN__)
  process_f64le(src, dst, channels, samples);
#elif defined(__BIG_ENDIAN__)
  process_f64be(src, dst, channels, samples);
#else
  if (is_little_endian()) {
    process_f64le(src, dst, channels, samples);
  } else {
    process_f64be(src, dst, channels, samples);
  }
#endif
}
