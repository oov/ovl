#include <ovl/audio/decoder/wav.h>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

#include <ovmo.h>

#include <ovl/audio/decoder.h>
#include <ovl/audio/info.h>
#include <ovl/source.h>

#include "../tag.h"
#include "../tag/id3v2.h"
#include "wav_inline.h"

#define WAVE_FORMAT_PCM 1
#define WAVE_FORMAT_IEEE_FLOAT 3

enum sample_format {
  sample_format_unknown,
  sample_format_u8,
  sample_format_i8,
  sample_format_i16le,
  sample_format_i16be,
  sample_format_i24le,
  sample_format_i24be,
  sample_format_i32le,
  sample_format_i32be,
  sample_format_i64le,
  sample_format_i64be,
  sample_format_f32le,
  sample_format_f32be,
  sample_format_f64le,
  sample_format_f64be,
};

static inline size_t sample_format_to_bytes(enum sample_format const f) {
  switch (f) {
  case sample_format_unknown:
    return 0;
  case sample_format_u8:
  case sample_format_i8:
    return 1;
  case sample_format_i16le:
  case sample_format_i16be:
    return 2;
  case sample_format_i24le:
  case sample_format_i24be:
    return 3;
  case sample_format_i32le:
  case sample_format_i32be:
  case sample_format_f32le:
  case sample_format_f32be:
    return 4;
  case sample_format_i64le:
  case sample_format_i64be:
  case sample_format_f64le:
  case sample_format_f64be:
    return 8;
  }
}

struct wav {
  struct ovl_audio_decoder_vtable const *vtable;
  struct ovl_source *source;
  struct ovl_audio_info info;

  enum sample_format sample_format;
  uint64_t data_offset;
  uint64_t position;

  void *raw_buffer;
  float **float_buffer;
  size_t buffer_samples;
};

static NODISCARD bool
parse_fmt_chunk(struct wav *ctx, size_t const chunk_size, uint64_t const offset, struct ov_error *const err) {
  bool result = false;
  {
    struct __attribute__((packed)) FmtChunk {
      WORD wFormatTag;
      WORD channels;
      DWORD sample_rate;
      DWORD byte_rate;
      WORD block_align;
      WORD bits_per_sample;
    } fmt;
    if (chunk_size < sizeof(fmt)) {
      OV_ERROR_SETF(err,
                    ov_error_type_generic,
                    ov_error_generic_fail,
                    "%1$s%2$zu%3$zu",
                    gettext("Invalid %1$s chunk size: got %2$zu, minimum required %3$zu"),
                    "fmt",
                    chunk_size,
                    sizeof(fmt));
      goto cleanup;
    }
    size_t read = ovl_source_read(ctx->source, &fmt, offset, sizeof(fmt));
    if (read < sizeof(fmt)) {
      OV_ERROR_SETF(
          err, ov_error_type_generic, ov_error_generic_fail, "%1$s", gettext("Failed to read %1$s chunk"), "fmt");
      goto cleanup;
    }

    ctx->info.channels = fmt.channels;
    ctx->info.sample_rate = fmt.sample_rate;
    switch (fmt.wFormatTag) {
    case WAVE_FORMAT_PCM:
      switch (fmt.bits_per_sample) {
      case 8:
        ctx->sample_format = sample_format_u8;
        break;
      case 16:
        ctx->sample_format = sample_format_i16le;
        break;
      case 24:
        ctx->sample_format = sample_format_i24le;
        break;
      case 32:
        ctx->sample_format = sample_format_i32le;
        break;
      case 64:
        // 64bit PCM is not supported by WAVE format but acceptable.
        ctx->sample_format = sample_format_i64le;
        break;
      default:
        OV_ERROR_SETF(err,
                      ov_error_type_generic,
                      ov_error_generic_fail,
                      "%s%d",
                      gettext("Unsupported sample format: %s(%dbits)"),
                      "WAVE_FORMAT_PCM",
                      fmt.bits_per_sample);
        goto cleanup;
      }
      break;
    case WAVE_FORMAT_IEEE_FLOAT:
      switch (fmt.bits_per_sample) {
      case 32:
        ctx->sample_format = sample_format_f32le;
        break;
      case 64:
        ctx->sample_format = sample_format_f64le;
        break;
      default:
        OV_ERROR_SETF(err,
                      ov_error_type_generic,
                      ov_error_generic_fail,
                      "%s%d",
                      gettext("Unsupported sample format: %s(%dbits)"),
                      "WAVE_FORMAT_IEEE_FLOAT",
                      fmt.bits_per_sample);
        goto cleanup;
      }
      break;
    default:
      OV_ERROR_SETF(err,
                    ov_error_type_generic,
                    ov_error_generic_fail,
                    "%d",
                    gettext("Unsupported format tag: %d"),
                    fmt.wFormatTag);
      goto cleanup;
    }
  }
  result = true;
cleanup:
  return result;
}

static NODISCARD bool parse_smpl_chunk(struct wav *const ctx,
                                       size_t const chunk_size,
                                       struct ovl_audio_tag *const loop_smpl,
                                       uint64_t const offset,
                                       struct ov_error *const err) {
  bool result = false;
  struct __attribute__((packed)) SmplChunk {
    uint32_t manufacturer;
    uint32_t product;
    uint32_t sample_period;
    uint32_t midi_unity_note;
    uint32_t midi_pitch_fraction;
    uint32_t smpte_format;
    uint32_t smpte_offset;
    uint32_t num_sample_loops;
    uint32_t sampler_data;
  } smpl;
  struct __attribute__((packed)) SmplLoop {
    uint32_t cue_point_id;
    uint32_t type;
    uint32_t start;
    uint32_t end;
    uint32_t fraction;
    uint32_t play_count;
  } loop;

  {
    if (chunk_size < sizeof(smpl)) {
      result = true;
      goto cleanup;
    }
    size_t read = ovl_source_read(ctx->source, &smpl, offset, sizeof(smpl));
    if (read < sizeof(smpl)) {
      OV_ERROR_SETF(
          err, ov_error_type_generic, ov_error_generic_fail, "%1$s", gettext("Failed to read %1$s chunk"), "smpl");
      goto cleanup;
    }

    if (smpl.num_sample_loops > 0) {
      if (sizeof(smpl) + sizeof(loop) > chunk_size) {
        result = true;
        goto cleanup;
      }
      read = ovl_source_read(ctx->source, &loop, offset + sizeof(smpl), sizeof(loop));
      if (read < sizeof(loop)) {
        OV_ERROR_SETF(
            err, ov_error_type_generic, ov_error_generic_fail, "%1$s", gettext("Failed to read %1$s chunk"), "loop");
        goto cleanup;
      }

      loop_smpl->loop_start = loop.start;
      loop_smpl->loop_end = loop.end;
    }
  }
  result = true;
cleanup:
  return result;
}

static void
apply_loop_info(struct ovl_audio_tag *const dest, struct ovl_audio_tag *const smpl, struct ovl_audio_tag *const id3v2) {
  if (smpl->title) {
    dest->title = smpl->title;
    smpl->title = NULL;
  } else if (id3v2->title) {
    dest->title = id3v2->title;
    id3v2->title = NULL;
  }
  if (smpl->artist) {
    dest->artist = smpl->artist;
    smpl->artist = NULL;
  } else if (id3v2->artist) {
    dest->artist = id3v2->artist;
    id3v2->artist = NULL;
  }
  if (smpl->loop_start != UINT64_MAX) {
    dest->loop_start = smpl->loop_start;
  } else if (id3v2->loop_start != UINT64_MAX) {
    dest->loop_start = id3v2->loop_start;
  }
  if (smpl->loop_end != UINT64_MAX) {
    dest->loop_end = smpl->loop_end;
  } else if (id3v2->loop_end != UINT64_MAX) {
    dest->loop_end = id3v2->loop_end;
  }
  if (smpl->loop_length != UINT64_MAX) {
    dest->loop_length = smpl->loop_length;
  } else if (id3v2->loop_length != UINT64_MAX) {
    dest->loop_length = id3v2->loop_length;
  }
  ovl_audio_tag_destroy(smpl);
  ovl_audio_tag_destroy(id3v2);
}

static NODISCARD bool
parse_wave_header(struct wav *ctx, uint64_t const file_size, uint64_t offset, struct ov_error *const err) {
  bool result = false;
  struct ovl_audio_tag loop_smpl = {
      .loop_start = UINT64_MAX,
      .loop_end = UINT64_MAX,
      .loop_length = UINT64_MAX,
  };
  struct ovl_audio_tag loop_id3v2 = {
      .loop_start = UINT64_MAX,
      .loop_end = UINT64_MAX,
      .loop_length = UINT64_MAX,
  };
  bool has_fmt = false;
  bool has_data = false;

  {
    struct __attribute__((packed)) {
      uint32_t size;
      uint8_t wave_signature[4];
    } header;

    if (offset + sizeof(header) > file_size) {
      OV_ERROR_SET(err, ov_error_type_generic, ov_error_generic_fail, gettext("File is too small"));
      goto cleanup;
    }
    size_t read = ovl_source_read(ctx->source, &header, offset, sizeof(header));
    if (read < sizeof(header)) {
      OV_ERROR_SET(err, ov_error_type_generic, ov_error_generic_fail, gettext("Failed to read file header"));
      goto cleanup;
    }
    offset += read;

    if (memcmp(header.wave_signature, "WAVE", 4) != 0) {
      OV_ERROR_SETF(
          err, ov_error_type_generic, ov_error_generic_fail, "%1$s", gettext("Invalid %1$s signature"), "WAVE");
      goto cleanup;
    }
  }

  while (offset < file_size) {
    struct __attribute__((packed)) {
      uint8_t id[4];
      uint32_t size;
    } chunk_header;
    if (offset + sizeof(chunk_header) > file_size) {
      OV_ERROR_SETF(err,
                    ov_error_type_generic,
                    ov_error_generic_fail,
                    "%1$zu%2$zu",
                    gettext("Not enough bytes for chunk header: missing "
                            "%1$zu bytes of %2$zu bytes"),
                    file_size - offset,
                    sizeof(chunk_header));
      goto cleanup;
    }
    size_t read = ovl_source_read(ctx->source, &chunk_header, offset, sizeof(chunk_header));
    if (read < sizeof(chunk_header)) {
      OV_ERROR_SET(err, ov_error_type_generic, ov_error_generic_fail, gettext("Failed to read chunk header"));
      goto cleanup;
    }
    offset += read;
    if (offset + chunk_header.size > file_size) {
      OV_ERROR_SETF(err,
                    ov_error_type_generic,
                    ov_error_generic_fail,
                    "%1$zu%2$zu",
                    gettext("Invalid chunk size: required %1$zu bytes but "
                            "only %2$zu bytes available"),
                    chunk_header.size,
                    file_size - offset);
      goto cleanup;
    }
    if (memcmp(chunk_header.id, "fmt ", 4) == 0) {
      if (!parse_fmt_chunk(ctx, (size_t)chunk_header.size, offset, err)) {
        OV_ERROR_ADD_TRACE(err);
        goto cleanup;
      }
      offset = adjust_align2(offset + chunk_header.size);
      has_fmt = true;
      continue;
    }
    if (memcmp(chunk_header.id, "smpl", 4) == 0) {
      if (!parse_smpl_chunk(ctx, (size_t)chunk_header.size, &loop_smpl, offset, err)) {
        OV_ERROR_ADD_TRACE(err);
        goto cleanup;
      }
      offset = adjust_align2(offset + chunk_header.size);
      continue;
    }
    if (memcmp(chunk_header.id, "id3 ", 4) == 0) {
      if (chunk_header.size == 0) {
        OV_ERROR_SETF(err, ov_error_type_generic, ov_error_generic_fail, "%1$s", gettext("Empty %1$s chunk"), "id3");
        goto cleanup;
      }
      if (!ovl_audio_tag_id3v2_read(&loop_id3v2, ctx->source, offset, chunk_header.size, err)) {
        OV_ERROR_ADD_TRACE(err);
        goto cleanup;
      }
      offset = adjust_align2(offset + chunk_header.size);
      continue;
    }
    if (memcmp(chunk_header.id, "data", 4) == 0) {
      if (chunk_header.size == 0) {
        OV_ERROR_SETF(err, ov_error_type_generic, ov_error_generic_fail, "%1$s", gettext("Empty %1$s chunk"), "data");
        goto cleanup;
      }
      uint64_t const bytes_per_sample = ctx->info.channels * sample_format_to_bytes(ctx->sample_format);
      if (bytes_per_sample == 0) {
        OV_ERROR_SET(err, ov_error_type_generic, ov_error_generic_fail, gettext("Invalid bits per sample"));
        goto cleanup;
      }
      ctx->data_offset = offset;
      ctx->info.samples = chunk_header.size / bytes_per_sample;
      offset = adjust_align2(offset + chunk_header.size);
      has_data = true;
      continue;
    }
    offset = adjust_align2(offset + chunk_header.size);
  }
  if (!has_fmt || !has_data) {
    OV_ERROR_SET(err, ov_error_type_generic, ov_error_generic_fail, gettext("Required chunks not found"));
    goto cleanup;
  }
  result = true;
cleanup:
  if (!result && has_fmt && has_data) {
    result = true;
  }
  if (result) {
    apply_loop_info(&ctx->info.tag, &loop_smpl, &loop_id3v2);
  } else {
    ovl_audio_tag_destroy(&loop_smpl);
    ovl_audio_tag_destroy(&loop_id3v2);
  }
  return result;
}

static NODISCARD bool
parse_rf64_header(struct wav *ctx, uint64_t const file_size, uint64_t offset, struct ov_error *const err) {
  bool result = false;
  struct ovl_audio_tag loop_smpl = {
      .loop_start = UINT64_MAX,
      .loop_end = UINT64_MAX,
      .loop_length = UINT64_MAX,
  };
  struct ovl_audio_tag loop_id3v2 = {
      .loop_start = UINT64_MAX,
      .loop_end = UINT64_MAX,
      .loop_length = UINT64_MAX,
  };
  bool has_fmt = false;
  bool has_data = false;
  bool has_ds64 = false;
  uint64_t real_data_size = 0;

  {
    struct __attribute__((packed)) {
      uint32_t size;
      uint8_t wave_signature[4];
    } header;
    if (offset + sizeof(header) > file_size) {
      OV_ERROR_SET(err, ov_error_type_generic, ov_error_generic_fail, gettext("File is too small"));
      goto cleanup;
    }
    size_t read = ovl_source_read(ctx->source, &header, offset, sizeof(header));
    if (read < sizeof(header)) {
      OV_ERROR_SET(err, ov_error_type_generic, ov_error_generic_fail, gettext("Failed to read file header"));
      goto cleanup;
    }
    offset += read;
    if (memcmp(header.wave_signature, "WAVE", 4) != 0) {
      OV_ERROR_SET(err, ov_error_type_generic, ov_error_generic_fail, gettext("Invalid WAVE signature"));
      goto cleanup;
    }
  }

  while (offset < file_size) {
    struct __attribute__((packed)) {
      uint8_t id[4];
      uint32_t size;
    } chunk_header;

    if (offset + sizeof(chunk_header) > file_size) {
      OV_ERROR_SETF(err,
                    ov_error_type_generic,
                    ov_error_generic_fail,
                    "%1$zu%2$zu",
                    gettext("Not enough bytes for chunk header: missing "
                            "%1$zu bytes of %2$zu bytes"),
                    file_size - offset,
                    sizeof(chunk_header));
      goto cleanup;
    }
    size_t read = ovl_source_read(ctx->source, &chunk_header, offset, sizeof(chunk_header));
    if (read < sizeof(chunk_header)) {
      OV_ERROR_SET(err, ov_error_type_generic, ov_error_generic_fail, gettext("Failed to read chunk header"));
      goto cleanup;
    }
    offset += read;

    if (memcmp(chunk_header.id, "ds64", 4) == 0) {
      struct __attribute__((packed)) {
        uint64_t riff_size;
        uint64_t data_size;
        uint64_t sample_count;
        uint32_t table_length;
      } ds64;
      if (chunk_header.size < sizeof(ds64)) {
        OV_ERROR_SETF(err,
                      ov_error_type_generic,
                      ov_error_generic_fail,
                      "%1$s%2$zu%3$zu",
                      gettext("Invalid %1$s chunk size: got %2$zu, minimum required %3$zu"),
                      "ds64",
                      chunk_header.size,
                      sizeof(ds64));
        goto cleanup;
      }
      read = ovl_source_read(ctx->source, &ds64, offset, sizeof(ds64));
      if (read < sizeof(ds64)) {
        OV_ERROR_SETF(
            err, ov_error_type_generic, ov_error_generic_fail, "%1$s", gettext("Failed to read %1$s chunk"), "ds64");
        goto cleanup;
      }
      offset += read;
      real_data_size = ds64.data_size;
      offset = adjust_align2(offset + chunk_header.size - read);
      has_ds64 = true;
      continue;
    }

    if (chunk_header.size == UINT32_MAX && !has_ds64) {
      OV_ERROR_SET(err,
                   ov_error_type_generic,
                   ov_error_generic_fail,
                   gettext("ds64 chunk not found but chunk size is 0xffffffff"));
      goto cleanup;
    }
    if (offset + (chunk_header.size == UINT32_MAX ? real_data_size : chunk_header.size) > file_size) {
      OV_ERROR_SETF(err,
                    ov_error_type_generic,
                    ov_error_generic_fail,
                    "%1$zu%2$zu",
                    gettext("Invalid chunk size: required %1$zu bytes but "
                            "only %2$zu bytes available"),
                    chunk_header.size == UINT32_MAX ? real_data_size : chunk_header.size,
                    file_size - offset);
      goto cleanup;
    }
    if (memcmp(chunk_header.id, "fmt ", 4) == 0) {
      if (!parse_fmt_chunk(ctx, (size_t)chunk_header.size, offset, err)) {
        OV_ERROR_ADD_TRACE(err);
        goto cleanup;
      }
      offset = adjust_align2(offset + chunk_header.size);
      has_fmt = true;
      continue;
    }
    if (memcmp(chunk_header.id, "data", 4) == 0) {
      if (chunk_header.size == 0) {
        OV_ERROR_SETF(err, ov_error_type_generic, ov_error_generic_fail, "%1$s", gettext("Empty %1$s chunk"), "data");
        goto cleanup;
      }
      ctx->data_offset = offset;
      uint64_t data_size = chunk_header.size;
      if (chunk_header.size == UINT32_MAX && has_ds64) {
        data_size = real_data_size;
      }
      uint64_t const bytes_per_sample = ctx->info.channels * sample_format_to_bytes(ctx->sample_format);
      if (bytes_per_sample == 0) {
        OV_ERROR_SET(err, ov_error_type_generic, ov_error_generic_fail, gettext("Invalid bits per sample"));
        goto cleanup;
      }
      ctx->info.samples = data_size / bytes_per_sample;
      offset = adjust_align2(offset + data_size);
      has_data = true;
      continue;
    }
    if (memcmp(chunk_header.id, "smpl", 4) == 0) {
      if (!parse_smpl_chunk(ctx, (size_t)chunk_header.size, &loop_smpl, offset, err)) {
        OV_ERROR_ADD_TRACE(err);
        goto cleanup;
      }
      offset = adjust_align2(offset + chunk_header.size);
      continue;
    }
    if (memcmp(chunk_header.id, "id3 ", 4) == 0) {
      if (chunk_header.size == 0) {
        OV_ERROR_SETF(err, ov_error_type_generic, ov_error_generic_fail, "%1$s", gettext("Empty %1$s chunk"), "id3");
        goto cleanup;
      }
      if (!ovl_audio_tag_id3v2_read(&loop_id3v2, ctx->source, offset, chunk_header.size, err)) {
        OV_ERROR_ADD_TRACE(err);
        goto cleanup;
      }
      offset = adjust_align2(offset + chunk_header.size);
      continue;
    }
    offset = adjust_align2(offset + chunk_header.size);
  }
  if (!has_fmt || !has_data) {
    OV_ERROR_SET(err, ov_error_type_generic, ov_error_generic_fail, gettext("Required chunks not found"));
    goto cleanup;
  }
  result = true;
cleanup:
  if (!result && has_fmt && has_data) {
    result = true;
  }
  if (result) {
    apply_loop_info(&ctx->info.tag, &loop_smpl, &loop_id3v2);
  } else {
    ovl_audio_tag_destroy(&loop_smpl);
    ovl_audio_tag_destroy(&loop_id3v2);
  }
  return result;
}

static NODISCARD bool
parse_wave64_header(struct wav *ctx, uint64_t const file_size, uint64_t offset, struct ov_error *const err) {
  static uint8_t const guid_riff[] = {
      0x72, 0x69, 0x66, 0x66, 0x2E, 0x91, 0xCF, 0x11, 0xA5, 0xD6, 0x28, 0xDB, 0x04, 0xC1, 0x00, 0x00};
  static uint8_t const guid_wave[] = {
      0x77, 0x61, 0x76, 0x65, 0xF3, 0xAC, 0xD3, 0x11, 0x8C, 0xD1, 0x00, 0xC0, 0x4F, 0x8E, 0xDB, 0x8A};
  static uint8_t const guid_fmt[] = {
      0x66, 0x6D, 0x74, 0x20, 0xF3, 0xAC, 0xD3, 0x11, 0x8C, 0xD1, 0x00, 0xC0, 0x4F, 0x8E, 0xDB, 0x8A};
  static uint8_t const guid_data[] = {
      0x64, 0x61, 0x74, 0x61, 0xF3, 0xAC, 0xD3, 0x11, 0x8C, 0xD1, 0x00, 0xC0, 0x4F, 0x8E, 0xDB, 0x8A};
  static uint8_t const guid_id3[] = {
      0x69, 0x64, 0x33, 0x20, 0xF3, 0xAC, 0xD3, 0x11, 0x8C, 0xD1, 0x00, 0xC0, 0x4F, 0x8E, 0xDB, 0x8A};

  bool result = false;
  bool has_fmt = false;
  bool has_data = false;
  struct ovl_audio_tag loop_id3v2 = {
      .loop_start = UINT64_MAX,
      .loop_end = UINT64_MAX,
      .loop_length = UINT64_MAX,
  };

  {
    struct __attribute__((packed)) {
      uint8_t riff_guid[16];
      uint64_t size;
      uint8_t wave_guid[16];
    } header = {
        .riff_guid = {0x72, 0x69, 0x66, 0x66}, // The first 4 bytes have already been read
    };

    if (offset + sizeof(header) - 4 > file_size) {
      OV_ERROR_SET(err, ov_error_type_generic, ov_error_generic_fail, gettext("File is too small"));
      goto cleanup;
    }
    size_t read = ovl_source_read(ctx->source, header.riff_guid + 4, offset, sizeof(header) - 4);
    if (read < (sizeof(header) - 4)) {
      OV_ERROR_SET(err, ov_error_type_generic, ov_error_generic_fail, gettext("Failed to read file header"));
      goto cleanup;
    }
    offset += read;
    if (memcmp(header.riff_guid, guid_riff, sizeof(guid_riff)) != 0) {
      OV_ERROR_SETF(
          err, ov_error_type_generic, ov_error_generic_fail, "%1$s", gettext("Invalid %1$s signature"), "Wave64");
      goto cleanup;
    }
    if (memcmp(header.wave_guid, guid_wave, sizeof(guid_wave)) != 0) {
      OV_ERROR_SETF(err, ov_error_type_generic, ov_error_generic_fail, "%1$s", gettext("Invalid %1$s GUID"), "wave");
      goto cleanup;
    }
  }

  while (offset < file_size) {
    struct __attribute__((packed)) {
      uint8_t guid[16];
      uint64_t size;
    } chunk_header;

    if (offset + sizeof(chunk_header) > file_size) {
      OV_ERROR_SETF(err,
                    ov_error_type_generic,
                    ov_error_generic_fail,
                    "%1$zu%2$zu",
                    gettext("Not enough bytes for chunk header: missing "
                            "%1$zu bytes of %2$zu bytes"),
                    file_size - offset,
                    sizeof(chunk_header));
      goto cleanup;
    }
    size_t read = ovl_source_read(ctx->source, &chunk_header, offset, sizeof(chunk_header));
    if (read < sizeof(chunk_header)) {
      OV_ERROR_SET(err, ov_error_type_generic, ov_error_generic_fail, gettext("Failed to read chunk header"));
      goto cleanup;
    }
    offset += read;

    if (offset - sizeof(chunk_header) + chunk_header.size > file_size) {
      OV_ERROR_SETF(err,
                    ov_error_type_generic,
                    ov_error_generic_fail,
                    "%1$zu%2$zu",
                    gettext("Invalid chunk size: required %1$zu bytes but "
                            "only %2$zu bytes available"),
                    chunk_header.size,
                    file_size - (offset - sizeof(chunk_header)));
      goto cleanup;
    }

    if (memcmp(chunk_header.guid, guid_fmt, sizeof(guid_fmt)) == 0) {
      uint64_t const data_size = chunk_header.size - sizeof(chunk_header);
      if (!parse_fmt_chunk(ctx, (size_t)data_size, offset, err)) {
        OV_ERROR_ADD_TRACE(err);
        goto cleanup;
      }
      offset = adjust_align8(offset + data_size);
      has_fmt = true;
      continue;
    }
    if (memcmp(chunk_header.guid, guid_data, sizeof(guid_data)) == 0) {
      if (chunk_header.size <= sizeof(chunk_header)) {
        OV_ERROR_SETF(err, ov_error_type_generic, ov_error_generic_fail, "%1$s", gettext("Empty %1$s chunk"), "data");
        goto cleanup;
      }
      ctx->data_offset = offset;
      uint64_t const data_size = chunk_header.size - sizeof(chunk_header);
      uint64_t const bytes_per_sample = ctx->info.channels * sample_format_to_bytes(ctx->sample_format);
      if (bytes_per_sample == 0) {
        OV_ERROR_SET(err, ov_error_type_generic, ov_error_generic_fail, gettext("Invalid bits per sample"));
        goto cleanup;
      }
      ctx->info.samples = data_size / bytes_per_sample;
      offset = adjust_align8(offset + data_size);
      has_data = true;
      continue;
    }
    if (memcmp(chunk_header.guid, guid_id3, sizeof(guid_id3)) == 0) {
      if (chunk_header.size <= sizeof(chunk_header)) {
        OV_ERROR_SETF(err, ov_error_type_generic, ov_error_generic_fail, "%1$s", gettext("Empty %1$s chunk"), "id3");
        goto cleanup;
      }
      uint64_t const id3_size = chunk_header.size - sizeof(chunk_header);
      if (!ovl_audio_tag_id3v2_read(&loop_id3v2, ctx->source, offset, (uint32_t)id3_size, err)) {
        OV_ERROR_ADD_TRACE(err);
        goto cleanup;
      }
      offset = adjust_align8(offset + id3_size);
      continue;
    }
    offset = adjust_align8(offset + (chunk_header.size - sizeof(chunk_header)));
  }
  if (!has_fmt || !has_data) {
    OV_ERROR_SET(err, ov_error_type_generic, ov_error_generic_fail, gettext("Required chunks not found"));
    goto cleanup;
  }
  result = true;
cleanup:
  if (!result && has_fmt && has_data) {
    result = true;
  }
  if (result) {
    ctx->info.tag = loop_id3v2;
  } else {
    ovl_audio_tag_destroy(&loop_id3v2);
  }
  return result;
}

static NODISCARD bool
parse_aiff_header(struct wav *ctx, uint64_t const file_size, uint64_t offset, struct ov_error *const err) {
  bool result = false;
  bool has_comm = false;
  bool has_ssnd = false;
  struct ovl_audio_tag loop_id3v2 = {
      .loop_start = UINT64_MAX,
      .loop_end = UINT64_MAX,
      .loop_length = UINT64_MAX,
  };
  {
    struct __attribute__((packed)) {
      uint32_t size;
      uint8_t aiff_signature[4];
    } header;
    if (offset + sizeof(header) > file_size) {
      OV_ERROR_SET(err, ov_error_type_generic, ov_error_generic_fail, gettext("File is too small"));
      goto cleanup;
    }
    size_t read = ovl_source_read(ctx->source, &header, offset, sizeof(header));
    if (read < sizeof(header)) {
      OV_ERROR_SET(err, ov_error_type_generic, ov_error_generic_fail, gettext("Failed to read file header"));
      goto cleanup;
    }
    offset += read;
    if (memcmp(header.aiff_signature, "AIFF", 4) != 0) {
      OV_ERROR_SETF(err,
                    ov_error_type_generic,
                    ov_error_generic_fail,
                    "%1$s%2$s",
                    gettext("Unsupported %1$s signature: %2$s"),
                    "AIFF",
                    header.aiff_signature);
      goto cleanup;
    }
  }
  while (offset < file_size) {
    struct __attribute__((packed)) {
      char chunk_id[4];
      uint32_t chunk_size;
    } chunk_header;
    if (offset + sizeof(chunk_header) > file_size) {
      OV_ERROR_SETF(err,
                    ov_error_type_generic,
                    ov_error_generic_fail,
                    "%1$zu%2$zu",
                    gettext("Not enough bytes for chunk header: missing "
                            "%1$zu bytes of %2$zu bytes"),
                    file_size - offset,
                    sizeof(chunk_header));
      goto cleanup;
    }
    size_t read = ovl_source_read(ctx->source, &chunk_header, offset, sizeof(chunk_header));
    if (read < sizeof(chunk_header)) {
      OV_ERROR_SET(err, ov_error_type_generic, ov_error_generic_fail, gettext("Failed to read chunk header"));
      goto cleanup;
    }
    offset += read;
    chunk_header.chunk_size = swap32be(chunk_header.chunk_size);
    if (memcmp(chunk_header.chunk_id, "COMM", 4) == 0) {
      struct __attribute__((packed)) {
        int16_t channels;
        uint32_t num_sample_frames;
        int16_t bits_per_sample;
        uint8_t sample_rate[10]; // 80-bit IEEE Standard 754 floating point number
      } comm;
      if (chunk_header.chunk_size < sizeof(comm)) {
        OV_ERROR_SETF(err,
                      ov_error_type_generic,
                      ov_error_generic_fail,
                      "%1$s%2$zu%3$zu",
                      gettext("Invalid %1$s chunk size: got %2$zu, minimum required %3$zu"),
                      "COMM",
                      chunk_header.chunk_size,
                      sizeof(comm));
        goto cleanup;
      }
      read = ovl_source_read(ctx->source, &comm, offset, sizeof(comm));
      if (read < sizeof(comm)) {
        OV_ERROR_SETF(
            err, ov_error_type_generic, ov_error_generic_fail, "%1$s", gettext("Failed to read %1$s chunk"), "COMM");
        goto cleanup;
      }
      offset += read;
      ctx->info.channels = (size_t)(int16_t)swap16be((uint16_t)comm.channels);
      ctx->info.sample_rate = (size_t)(ieee754_extended_be_to_double(comm.sample_rate));
      size_t const bits_per_sample = (size_t)(int16_t)swap16be((uint16_t)comm.bits_per_sample);
      switch (bits_per_sample) {
      case 8:
        ctx->sample_format = sample_format_i8;
        break;
      case 16:
        ctx->sample_format = sample_format_i16be;
        break;
      case 24:
        ctx->sample_format = sample_format_i24be;
        break;
      case 32:
        ctx->sample_format = sample_format_i32be;
        break;
      default:
        OV_ERROR_SETF(err,
                      ov_error_type_generic,
                      ov_error_generic_fail,
                      "%1$d",
                      gettext("Unsupported sample format: AIFF(%1$dbits)"),
                      bits_per_sample);
        goto cleanup;
      }
      has_comm = true;
      continue;
    }
    if (memcmp(chunk_header.chunk_id, "SSND", 4) == 0) {
      struct __attribute__((packed)) {
        uint32_t offset;
        uint32_t block_size;
      } ssnd_header;
      if (chunk_header.chunk_size < sizeof(ssnd_header)) {
        OV_ERROR_SETF(err,
                      ov_error_type_generic,
                      ov_error_generic_fail,
                      "%1$s%2$zu%3$zu",
                      gettext("Invalid %1$s chunk size: got %2$zu, minimum required %3$zu"),
                      "SSND",
                      chunk_header.chunk_size,
                      sizeof(ssnd_header));
        goto cleanup;
      }
      read = ovl_source_read(ctx->source, &ssnd_header, offset, sizeof(ssnd_header));
      if (read < sizeof(ssnd_header)) {
        OV_ERROR_SETF(
            err, ov_error_type_generic, ov_error_generic_fail, "%1$s", gettext("Failed to read %1$s header"), "SSND");
        goto cleanup;
      }
      offset += read;
      ssnd_header.offset = swap32be(ssnd_header.offset);
      ctx->data_offset = offset + ssnd_header.offset;
      uint64_t const data_size = chunk_header.chunk_size - sizeof(ssnd_header) - ssnd_header.offset;
      uint64_t const bytes_per_sample = ctx->info.channels * sample_format_to_bytes(ctx->sample_format);
      if (bytes_per_sample == 0) {
        OV_ERROR_SET(err, ov_error_type_generic, ov_error_generic_fail, gettext("Invalid bits per sample"));
        goto cleanup;
      }
      ctx->info.samples = data_size / bytes_per_sample;
      has_ssnd = true;
      offset += chunk_header.chunk_size - sizeof(ssnd_header);
      continue;
    }
    if (memcmp(chunk_header.chunk_id, "ID3 ", 4) == 0) {
      if (chunk_header.chunk_size == 0) {
        OV_ERROR_SETF(err, ov_error_type_generic, ov_error_generic_fail, "%1$s", gettext("Empty %1$s chunk"), "ID3");
        goto cleanup;
      }
      if (!ovl_audio_tag_id3v2_read(&loop_id3v2, ctx->source, offset, chunk_header.chunk_size, err)) {
        OV_ERROR_ADD_TRACE(err);
        goto cleanup;
      }
      offset += chunk_header.chunk_size;
      continue;
    }
    offset += chunk_header.chunk_size;
  }
  if (!has_comm || !has_ssnd) {
    OV_ERROR_SET(err, ov_error_type_generic, ov_error_generic_fail, gettext("Required chunks not found"));
    goto cleanup;
  }
  result = true;

cleanup:
  if (!result && has_comm && has_ssnd) {
    result = true;
  }
  if (result) {
    ctx->info.tag = loop_id3v2;
  } else {
    ovl_audio_tag_destroy(&loop_id3v2);
  }
  return result;
}

static NODISCARD bool parse_header(struct wav *ctx, uint64_t const filesize, struct ov_error *const err) {
  bool result = false;
  char signature[sizeof(uint32_t)];
  uint64_t offset = 0;
  {
    if (filesize < sizeof(signature)) {
      OV_ERROR_SET(err, ov_error_type_generic, ov_error_generic_fail, gettext("File is too small"));
      goto cleanup;
    }
    size_t read = ovl_source_read(ctx->source, signature, offset, sizeof(signature));
    if (read < sizeof(signature)) {
      OV_ERROR_SET(err, ov_error_type_generic, ov_error_generic_fail, gettext("Failed to read file signature"));
      goto cleanup;
    }
    offset += read;
    if (memcmp(signature, "RIFF", 4) == 0) {
      if (!parse_wave_header(ctx, filesize, offset, err)) {
        OV_ERROR_ADD_TRACE(err);
        goto cleanup;
      }
    } else if (memcmp(signature, "RF64", 4) == 0) {
      if (!parse_rf64_header(ctx, filesize, offset, err)) {
        OV_ERROR_ADD_TRACE(err);
        goto cleanup;
      }
    } else if (memcmp(signature, "riff", 4) == 0) {
      if (!parse_wave64_header(ctx, filesize, offset, err)) {
        OV_ERROR_ADD_TRACE(err);
        goto cleanup;
      }
    } else if (memcmp(signature, "FORM", 4) == 0) {
      if (!parse_aiff_header(ctx, filesize, offset, err)) {
        OV_ERROR_ADD_TRACE(err);
        goto cleanup;
      }
    } else {
      OV_ERROR_SET(err, ov_error_type_generic, ov_error_generic_fail, gettext("Unknown format"));
      goto cleanup;
    }
  }
  result = true;
cleanup:
  return result;
}

static void destroy(struct ovl_audio_decoder **const dp) {
  struct wav **const ctxp = (struct wav **)(void *)dp;
  if (!ctxp || !*ctxp) {
    return;
  }
  struct wav *ctx = *ctxp;
  if (ctx->float_buffer) {
    if (ctx->float_buffer[0]) {
      OV_ALIGNED_FREE(&ctx->float_buffer[0]);
    }
    OV_FREE(&ctx->float_buffer);
  }
  if (ctx->raw_buffer) {
    OV_ALIGNED_FREE(&ctx->raw_buffer);
  }
  ovl_audio_tag_destroy(&ctx->info.tag);
  OV_FREE(ctxp);
}

static struct ovl_audio_info const *get_info(struct ovl_audio_decoder const *const d) {
  struct wav const *const ctx = (struct wav const *)(void const *)d;
  if (!ctx) {
    return NULL;
  }
  return &ctx->info;
}

static NODISCARD bool read(struct ovl_audio_decoder *const d,
                           float const *const **const pcm,
                           size_t *const samples,
                           struct ov_error *const err) {
  struct wav *const ctx = (struct wav *)(void *)d;
  if (!ctx || !pcm || !samples) {
    OV_ERROR_SET_GENERIC(err, ov_error_generic_invalid_argument);
    return false;
  }
  size_t const bytes_per_sample = sample_format_to_bytes(ctx->sample_format);
  size_t to_read = ctx->buffer_samples;
  uint64_t const remaining = ctx->info.samples - ctx->position;
  if (to_read > remaining) {
    to_read = (size_t)remaining;
  }
  if (to_read > ctx->buffer_samples) {
    to_read = ctx->buffer_samples;
  }
  size_t const channels = ctx->info.channels;
  uint64_t const read_offset = ctx->data_offset + ctx->position * channels * bytes_per_sample;
  size_t const read_size = to_read * channels * bytes_per_sample;
  size_t const read_bytes = ovl_source_read(ctx->source, ctx->raw_buffer, read_offset, read_size);
  if (read_bytes == SIZE_MAX) {
    OV_ERROR_SET_GENERIC(err, ov_error_generic_fail);
    return false;
  }
  size_t const got_samples = read_bytes / (channels * bytes_per_sample);
  if (got_samples == 0) {
    *samples = 0;
    return true;
  }
  switch (ctx->sample_format) {
  case sample_format_unknown:
    OV_ERROR_SET(err, ov_error_type_generic, ov_error_generic_fail, gettext("Unknown sample format"));
    return false;
  case sample_format_u8:
    process_u8((uint8_t const *)ctx->raw_buffer, ctx->float_buffer, channels, got_samples);
    break;
  case sample_format_i8:
    process_i8((int8_t const *)ctx->raw_buffer, ctx->float_buffer, channels, got_samples);
    break;
  case sample_format_i16le:
    process_i16le((int16_t const *)ctx->raw_buffer, ctx->float_buffer, channels, got_samples);
    break;
  case sample_format_i16be:
    process_i16be((int16_t const *)ctx->raw_buffer, ctx->float_buffer, channels, got_samples);
    break;
  case sample_format_i24le:
    process_i24le((uint8_t const *)ctx->raw_buffer, ctx->float_buffer, channels, got_samples);
    break;
  case sample_format_i24be:
    process_i24be((uint8_t const *)ctx->raw_buffer, ctx->float_buffer, channels, got_samples);
    break;
  case sample_format_i32le:
    process_i32le((int32_t const *)ctx->raw_buffer, ctx->float_buffer, channels, got_samples);
    break;
  case sample_format_i32be:
    process_i32be((int32_t const *)ctx->raw_buffer, ctx->float_buffer, channels, got_samples);
    break;
  case sample_format_i64le:
    process_i64le((int64_t const *)ctx->raw_buffer, ctx->float_buffer, channels, got_samples);
    break;
  case sample_format_i64be:
    process_i64be((int64_t const *)ctx->raw_buffer, ctx->float_buffer, channels, got_samples);
    break;
  case sample_format_f32le:
    process_f32le((uint32_t const *)ctx->raw_buffer, ctx->float_buffer, channels, got_samples);
    break;
  case sample_format_f32be:
    process_f32le((uint32_t const *)ctx->raw_buffer, ctx->float_buffer, channels, got_samples);
    break;
  case sample_format_f64le:
    process_f64le((uint64_t const *)ctx->raw_buffer, ctx->float_buffer, channels, got_samples);
    break;
  case sample_format_f64be:
    process_f64be((uint64_t const *)ctx->raw_buffer, ctx->float_buffer, channels, got_samples);
    break;
  }
  ctx->position += got_samples;
  *pcm = (float const *const *)ov_deconster_(ctx->float_buffer);
  *samples = got_samples;
  return true;
}

static NODISCARD bool seek(struct ovl_audio_decoder *const d, uint64_t const position, struct ov_error *const err) {
  struct wav *const ctx = (struct wav *)(void *)d;
  if (!ctx) {
    OV_ERROR_SET_GENERIC(err, ov_error_generic_invalid_argument);
    return false;
  }
  ctx->position = position;
  return true;
}

NODISCARD bool ovl_audio_decoder_wav_create(struct ovl_source *const source,
                                            struct ovl_audio_decoder **const dp,
                                            struct ov_error *const err) {
  if (!source || !dp) {
    OV_ERROR_SET_GENERIC(err, ov_error_generic_invalid_argument);
    return false;
  }
  struct wav *ctx = NULL;
  bool result = false;

  {
    if (!OV_REALLOC(&ctx, 1, sizeof(*ctx))) {
      OV_ERROR_SET_GENERIC(err, ov_error_generic_out_of_memory);
      goto cleanup;
    }
    static struct ovl_audio_decoder_vtable const vtable = {
        .destroy = destroy,
        .get_info = get_info,
        .read = read,
        .seek = seek,
    };
    *ctx = (struct wav){
        .vtable = &vtable,
        .source = source,
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
    uint64_t const size = ovl_source_size(source);
    if (size == UINT64_MAX) {
      OV_ERROR_SET(err, ov_error_type_generic, ov_error_generic_fail, gettext("Failed to get source size"));
      goto cleanup;
    }
    if (!parse_header(ctx, size, err)) {
      OV_ERROR_ADD_TRACE(err);
      goto cleanup;
    }
    size_t const read = ovl_source_read(source, NULL, ctx->data_offset, 0);
    if (read == SIZE_MAX) {
      OV_ERROR_SET(err, ov_error_type_generic, ov_error_generic_fail, gettext("Failed to seek to data offset"));
      goto cleanup;
    }
    ctx->buffer_samples = ctx->info.sample_rate / 10; // 100msec
    size_t const channels = ctx->info.channels;
    size_t const raw_buffer_size = ctx->buffer_samples * channels * sample_format_to_bytes(ctx->sample_format);
    if (!OV_ALIGNED_ALLOC(&ctx->raw_buffer, raw_buffer_size, 1, 16)) {
      OV_ERROR_SET_GENERIC(err, ov_error_generic_out_of_memory);
      goto cleanup;
    }
    ctx->float_buffer = NULL;
    if (!OV_REALLOC(&ctx->float_buffer, channels, sizeof(float *))) {
      OV_ERROR_SET_GENERIC(err, ov_error_generic_out_of_memory);
      goto cleanup;
    }
    ctx->float_buffer[0] = NULL;
    if (!OV_ALIGNED_ALLOC(&ctx->float_buffer[0], ctx->buffer_samples * channels, sizeof(float), 16)) {
      OV_ERROR_SET_GENERIC(err, ov_error_generic_out_of_memory);
      goto cleanup;
    }
    for (size_t i = 1; i < channels; ++i) {
      ctx->float_buffer[i] = ctx->float_buffer[i - 1] + ctx->buffer_samples;
    }
    ctx->position = 0;
    *dp = (struct ovl_audio_decoder *)(void *)ctx;
  }
  result = true;
cleanup:
  if (!result) {
    if (ctx) {
      destroy((struct ovl_audio_decoder **)(void *)&ctx);
    }
  }
  return result;
}
