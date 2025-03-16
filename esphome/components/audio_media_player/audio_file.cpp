#include "audio_file.h"

namespace esphome {
namespace audio {

const char *audio_file_type_to_string(AudioFileType file_type) {
  switch (file_type) {
#ifdef USE_FLAC_DECODER
    case AudioFileType::FLAC:
      return "FLAC";
#endif
#ifdef USE_MP3_DECODER
    case AudioFileType::MP3:
      return "MP3";
#endif
    case AudioFileType::WAV:
      return "WAV";
    default:
      return "unknown";
  }
}

}  // namespace audio
}  // namespace esphome
