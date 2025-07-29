#pragma once

#include <ovbase.h>

struct ovl_audio_tag;
struct ovl_source;

/**
 * Reads an ID3v2 tag from the source.
 *
 * @param tag        Pointer to the loop information structure.
 * @param source     Pointer to the source.
 * @param offset     Offset in the source from which to start reading.
 * @param chunk_size Maximum number of bytes to read (can be up to SIZE_MAX - 1).
 * @note If offset does not contain a valid ID3 tag, the read succeeds without loop info.
 *       An error is returned only when the read fails despite available data or when the tag exceeds chunk_size.
 * @return eok() on success, otherwise an error code.
 */
NODISCARD error ovl_audio_tag_id3v2_read(struct ovl_audio_tag *const tag,
                           struct ovl_source *const source,
                           uint64_t const offset,
                           size_t const chunk_size);
