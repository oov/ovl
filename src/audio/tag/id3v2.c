#include "id3v2.h"

#include "../../i18n.h"
#include "../tag.h"
#include <ovl/source.h>

#include <ovnum.h>
#include <ovutf.h>

enum id3v2_encoding {
  id3v2_encoding_iso_8859_1 = 0,           // ID3v2.3
  id3v2_encoding_utf_16_with_bom = 1,      // ID3v2.3
  id3v2_encoding_utf_16be_without_bom = 2, // ID3v2.4
  id3v2_encoding_utf_8 = 3,                // ID3v2.4
};

static size_t iso8859_1_to_utf8(char *buf, size_t buflen, char *out, size_t outlen) {
  size_t required = 0;
  for (size_t i = 0; i < buflen; i++) {
    if (buf[i] & 0x80) {
      required += 2;
    } else {
      required++;
    }
  }
  if (required == 0 || outlen < required + 1) {
    return 0;
  }
  size_t j = 0;
  for (size_t i = 0; i < buflen; i++) {
    if (buf[i] & 0x80) {
      out[j++] = 0xc0 | ((uint8_t)(buf[i]) >> 6);
      out[j++] = 0x80 | (buf[i] & 0x3f);
    } else {
      out[j++] = buf[i];
    }
  }
  out[required] = '\0';
  return required;
}

static size_t utf16_to_utf8(
    char *buf, size_t const buflen, enum id3v2_encoding const encoding, char *const out, size_t const outlen) {
  size_t offset;
  bool isle;
  if (encoding == id3v2_encoding_utf_16_with_bom) {
    if (buflen < 2) {
      return 0;
    }
    if (buf[0] == '\xff' && buf[1] == '\xfe') {
      isle = true;
    } else if (buf[0] == '\xfe' && buf[1] == '\xff') {
      isle = false;
    } else {
      return 0;
    }
    offset = 2;
  } else if (encoding == id3v2_encoding_utf_16be_without_bom) {
    isle = false;
    offset = 0;
  } else {
    return 0;
  }
  size_t const count = (buflen - offset) / 2;
  char16_t *const work = (void *)(buf + offset);
  uint8_t const *const p = (void *)work;
  if (isle) {
    for (size_t i = 0; i < count; i++) {
      work[i] = (char16_t)((p[i * 2 + 1] << 8) | p[i * 2]);
    }
  } else {
    for (size_t i = 0; i < count; i++) {
      work[i] = (char16_t)((p[i * 2] << 8) | p[i * 2 + 1]);
    }
  }
  size_t const required = ov_char16_to_utf8_len(work, count);
  if (required == 0 || outlen < required + 1) {
    return 0;
  }
  size_t conv_read;
  ov_char16_to_utf8(work, count, out, outlen, &conv_read);
  out[required] = '\0';
  return required;
}

static size_t to_utf8(char *buf, size_t buflen, enum id3v2_encoding encoding, char *out, size_t outlen) {
  if (encoding == id3v2_encoding_iso_8859_1) {
    return iso8859_1_to_utf8(buf, buflen, out, outlen);
  } else if (encoding == id3v2_encoding_utf_16_with_bom || encoding == id3v2_encoding_utf_16be_without_bom) {
    return utf16_to_utf8(buf, buflen, encoding, out, outlen);
  } else if (encoding == id3v2_encoding_utf_8) {
    if (buflen >= outlen) {
      return 0;
    }
    memcpy(out, buf, buflen);
    out[buflen] = '\0';
    return buflen;
  }
  return 0;
}

static NODISCARD error read_txxx_frame(struct ovl_audio_tag *const loop_info,
                                       struct ovl_source *const source,
                                       uint64_t offset,
                                       uint32_t const frame_size) {
  // This function designed to read loop information from TXXX frame in ID3v2
  // tag. Not for general use. If the desired data is not stored, treat it as a
  // success.
  error err = eok();
  if (frame_size < 2) { // encoding + null terminator
    err = emsg_i18n(err_type_generic, err_fail, gettext("TXXX frame too small"));
    goto cleanup;
  }

  uint8_t encoding;
  if (ovl_source_read(source, &encoding, offset, 1) != 1) {
    err = emsg_i18n(err_type_generic, err_fail, gettext("Failed to read TXXX encoding"));
    goto cleanup;
  }
  ++offset;

  if (encoding != id3v2_encoding_iso_8859_1 && encoding != id3v2_encoding_utf_16_with_bom &&
      encoding != id3v2_encoding_utf_16be_without_bom && encoding != id3v2_encoding_utf_8) {
    err = emsg_i18nf(err_type_generic, err_fail, NULL, gettext("Unsupported TXXX encoding: %d"), encoding);
    goto cleanup;
  }

  enum {
    desc_max_len = sizeof("LOOPLENGTH") - 1,
    value_max_len = sizeof("18446744073709551615") - 1,
    // UTF-16 can expand to 2 bytes per character
    content_max_bytes = (desc_max_len + 1 + value_max_len) * 2,
    // UTF-8 can expand to 4 bytes per character,
    // but only ASCII characters are handled here, so it is calculated as 1
    // byte.
    u8_buffer_size = (desc_max_len + 1 + value_max_len + 1) * 1,
  };

  size_t const content_size = frame_size - 1;
  if (content_size > content_max_bytes) {
    goto cleanup; // TXXX frame too large but not an error
  }

  char buf[content_max_bytes];
  if (ovl_source_read(source, buf, offset, content_size) < content_size) {
    err = emsg_i18n(err_type_generic, err_fail, gettext("Failed to read TXXX content"));
    goto cleanup;
  }

  char u8buf[u8_buffer_size];
  char *desc = NULL;
  char *value = NULL;
  size_t desc_len = SIZE_MAX;
  size_t value_len = SIZE_MAX;
  if (encoding == id3v2_encoding_utf_16_with_bom || encoding == id3v2_encoding_utf_16be_without_bom) {
    for (size_t i = 0; i < content_size; i += 2) {
      if (buf[i] == 0 && buf[i + 1] == 0) {
        desc = u8buf;
        desc_len = to_utf8(buf, i, encoding, desc, u8_buffer_size);
        value = u8buf + desc_len + 1;
        value_len = to_utf8(buf + i + 2, content_size - i - 2, encoding, value, u8_buffer_size - desc_len - 1);
        if (!desc_len || !value_len) {
          goto cleanup; // Invalid sequence or buffer too small but not an error
        }
        break;
      }
    }
  } else {
    for (size_t i = 0; i < content_size; ++i) {
      if (buf[i] == 0) {
        desc = buf;
        desc_len = i;
        desc[desc_len] = '\0';
        value = buf + i + 1;
        value_len = content_size - i - 1;
        value[value_len] = '\0';
        break;
      }
    }
  }
  if (desc_len == SIZE_MAX || value_len == SIZE_MAX) {
    err = emsg_i18n(err_type_generic, err_fail, gettext("The description/value separator not found in TXXX frame"));
    goto cleanup;
  }

  enum desc_type {
    desc_type_loop_start = 0,
    desc_type_loop_end = 1,
    desc_type_loop_length = 2,
  } desc_type;
  if (strcmp(desc, "LOOPSTART") == 0) {
    desc_type = desc_type_loop_start;
  } else if (strcmp(desc, "LOOPEND") == 0) {
    desc_type = desc_type_loop_end;
  } else if (strcmp(desc, "LOOPLENGTH") == 0) {
    desc_type = desc_type_loop_length;
  } else {
    goto cleanup; // Unknown TXXX description but not an error
  }
  uint64_t v;
  if (!ov_atou_char(value, &v, true)) {
    goto cleanup; // Invalid TXXX value but not an error
  }

  if (desc_type == desc_type_loop_start) {
    loop_info->loop_start = v;
  } else if (desc_type == desc_type_loop_end) {
    loop_info->loop_end = v;
  } else if (desc_type == desc_type_loop_length) {
    loop_info->loop_length = v;
  }
cleanup:
  return err;
}

static NODISCARD error read_text_frame(struct ovl_audio_tag *const tag,
                                       struct ovl_source *const source,
                                       uint64_t offset,
                                       uint32_t frame_size,
                                       const char *frame_id) {
  enum {
    buffer_max_len = 1024,
  };
  if (frame_size < 1) {
    return emsg_i18n(err_type_generic, err_fail, gettext("Text frame too small"));
  }
  if (frame_size - 1 > buffer_max_len - 1) {
    return eok(); // Text frame too large but not an error
  }

  error err = eok();

  uint8_t encoding;
  size_t read = ovl_source_read(source, &encoding, offset, 1);
  if (read < 1) {
    err = emsg_i18nf(err_type_generic, err_fail, NSTR("%s"), gettext("Failed to read %s encoding"), frame_id);
    goto cleanup;
  }
  offset += read;

  if (encoding != id3v2_encoding_iso_8859_1 && encoding != id3v2_encoding_utf_16_with_bom &&
      encoding != id3v2_encoding_utf_16be_without_bom && encoding != id3v2_encoding_utf_8) {
    err = emsg_i18nf(
        err_type_generic, err_fail, NSTR("%s%d"), gettext("Unsupported %s encoding: %d"), frame_id, encoding);
    goto cleanup;
  }

  char buf[buffer_max_len];
  size_t content_size = frame_size - 1;
  read = ovl_source_read(source, buf, offset, content_size);
  if (read < content_size) {
    err = emsg_i18nf(err_type_generic, err_fail, NSTR("%s"), gettext("Failed to read %s content"), frame_id);
    goto cleanup;
  }
  offset += read;
  if (encoding == id3v2_encoding_utf_16_with_bom || encoding == id3v2_encoding_utf_16be_without_bom) {
    char out[buffer_max_len];
    size_t const conv = to_utf8(buf, content_size, encoding, out, buffer_max_len);
    if (conv == 0) {
      goto cleanup; // Invalid sequence or buffer too small but not an error
    }
    content_size = conv;
    memcpy(buf, out, content_size + 1);
  }
  if (strcmp(frame_id, "TIT2") == 0) {
    err = ovl_audio_tag_set_title(tag, buf, content_size);
    if (efailed(err)) {
      err = ethru(err);
      goto cleanup;
    }
  } else if (strcmp(frame_id, "TPE1") == 0) {
    err = ovl_audio_tag_set_artist(tag, buf, content_size);
    if (efailed(err)) {
      err = ethru(err);
      goto cleanup;
    }
  }
cleanup:
  return err;
}

NODISCARD error ovl_audio_tag_id3v2_read(struct ovl_audio_tag *const tag,
                           struct ovl_source *const source,
                           uint64_t offset,
                           size_t const chunk_size) {
  if (!source || !chunk_size || !tag) {
    return errg(err_invalid_arugment);
  }
  error err = eok();
  struct ovl_audio_tag t = {
      .loop_start = UINT64_MAX,
      .loop_end = UINT64_MAX,
      .loop_length = UINT64_MAX,
  };
  struct ID3v2_Header {
    char id[3];
    uint8_t version_major;
    uint8_t version_minor;
    uint8_t flags;
    uint8_t size_bytes[4];
  } header;
  uint64_t chunk_end = offset + chunk_size;
  if (offset + sizeof(header) > chunk_end) {
    goto cleanup;
  }
  size_t read = ovl_source_read(source, &header, offset, sizeof(header));
  if (read < sizeof(header)) {
    err = emsg_i18n(err_type_generic, err_fail, gettext("Failed to read ID3 header"));
    goto cleanup;
  }
  offset += read;
  if (memcmp(header.id, "ID3", 3) != 0) {
    goto cleanup;
  }
  if (header.version_major != 3 && header.version_major != 4) {
    err = emsg_i18nf(err_type_generic,
                     err_fail,
                     NULL,
                     gettext("Unsupported ID3 version: %d.%d"),
                     header.version_major,
                     header.version_minor);
    goto cleanup;
  }
  uint32_t const tag_size = ((uint32_t)(header.size_bytes[0] & 0x7f) << 21) |
                            ((uint32_t)(header.size_bytes[1] & 0x7f) << 14) |
                            ((uint32_t)(header.size_bytes[2] & 0x7f) << 7) | (header.size_bytes[3] & 0x7f);
  if (offset + tag_size > chunk_end) {
    err = emsg_i18nf(err_type_generic,
                     err_fail,
                     NULL,
                     gettext("Invalid ID3 tag size: got %u, maximum allowed %zu"),
                     tag_size,
                     chunk_size - sizeof(header));
    goto cleanup;
  }
  chunk_end = offset + tag_size;
  if (header.flags & 0x40) { // has extended header
    uint8_t ext_size_bytes[4];
    read = ovl_source_read(source, ext_size_bytes, offset, sizeof(ext_size_bytes));
    if (read < sizeof(ext_size_bytes)) {
      err = emsg_i18n(err_type_generic, err_fail, gettext("Failed to read extended header"));
      goto cleanup;
    }
    offset += read;

    uint32_t ext_size;
    // clang-format off
    if (header.version_major == 4) {
      ext_size = ((uint32_t)(ext_size_bytes[0] & 0x7f) << 21) | // ID3v2.4
                 ((uint32_t)(ext_size_bytes[1] & 0x7f) << 14) |
                 ((uint32_t)(ext_size_bytes[2] & 0x7f) <<  7) |
                 ((uint32_t)(ext_size_bytes[3] & 0x7f) <<  0);
    } else {
      ext_size = ((uint32_t)ext_size_bytes[0] << 24) |          // ID3v2.3
                 ((uint32_t)ext_size_bytes[1] << 16) |
                 ((uint32_t)ext_size_bytes[2] <<  8) |
                 ((uint32_t)ext_size_bytes[3] <<  0);
    }
    // clang-format on
    if (ext_size < 6 || offset + ext_size > chunk_end) {
      err = emsg_i18n(err_type_generic, err_fail, gettext("Invalid extended header size"));
      goto cleanup;
    }
    offset += ext_size;
  }

  struct __attribute__((packed)) ID3v2_FrameHeader {
    char id[4];
    uint8_t size[4];
    uint16_t flags;
  } frame;
  while (offset + sizeof(frame) <= chunk_end) {
    read = ovl_source_read(source, &frame, offset, sizeof(frame));
    if (read < sizeof(frame)) {
      err = errg(err_fail);
      goto cleanup;
    }
    offset += read;
    // clang-format off
    uint32_t const frame_size = ((uint32_t)frame.size[0] << 24) |
                                ((uint32_t)frame.size[1] << 16) |
                                ((uint32_t)frame.size[2] <<  8) |
                                ((uint32_t)frame.size[3] <<  0);
    // clang-format on
    if (offset + frame_size > chunk_end) {
      break;
    }
    char const frame_id[5] = {frame.id[0], frame.id[1], frame.id[2], frame.id[3]};
    if (memcmp(frame_id, "TIT2", 4) == 0 || memcmp(frame_id, "TPE1", 4) == 0) {
      err = read_text_frame(&t, source, offset, frame_size, frame_id);
      if (efailed(err)) {
        err = ethru(err);
        goto cleanup;
      }
    } else if (memcmp(frame_id, "TXXX", 4) == 0) {
      err = read_txxx_frame(&t, source, offset, frame_size);
      if (efailed(err)) {
        err = ethru(err);
        goto cleanup;
      }
    }
    offset += frame_size;
  }
cleanup:
  if (esucceeded(err)) {
    *tag = t;
  }
  return err;
}
