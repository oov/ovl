#pragma once

#include <ovbase.h>

/**
 * Represents an loop information from tag.
 * The fields 'start', 'end' and 'length' are set to UINT64_MAX when their
 * values are invalid.
 */
struct ovl_audio_tag {
  char *title;
  char *artist;
  uint64_t loop_start;
  uint64_t loop_end;
  uint64_t loop_length;
};
