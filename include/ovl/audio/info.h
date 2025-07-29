#pragma once

#include <ovl/audio/tag.h>

struct ovl_audio_info {
  size_t sample_rate;
  size_t channels;
  uint64_t samples;
  struct ovl_audio_tag tag;
};
