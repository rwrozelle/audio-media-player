#pragma once

#include "audio_media_player.h"

#ifdef USE_ESP_IDF

#include "audio_file.h"
#include "esphome/core/automation.h"

namespace esphome {
namespace esp_audio {

template<typename... Ts> class PlayOnDeviceMediaAction : public Action<Ts...>, public Parented<AudioMediaPlayer> {
  TEMPLATABLE_VALUE(audio::AudioFile *, audio_file)
  TEMPLATABLE_VALUE(bool, announcement)
  TEMPLATABLE_VALUE(bool, enqueue)
  void play(Ts... x) override {
    this->parent_->play_file(this->audio_file_.value(x...), this->announcement_.value(x...),
                             this->enqueue_.value(x...));
  }
};

}  // namespace speaker
}  // namespace esphome

#endif
