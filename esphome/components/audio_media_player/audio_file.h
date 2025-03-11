#pragma once

#include <cstddef>
#include <cstdint>

namespace esphome {
namespace audio {

enum class AudioFileType : uint8_t {
  NONE = 0,
#ifdef USE_FLAC_DECODER
  FLAC,
#endif
#ifdef USE_MP3_DECODER
  MP3,
#endif
  WAV,
};

struct AudioFile {
  const uint8_t *data;
  size_t length;
  AudioFileType file_type;
};

const char *audio_file_type_to_string(AudioFileType file_type); 

}  // namespace audio
}  // namespace esphome
