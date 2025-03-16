#pragma once

#include "audio_media_pipeline.h"

#ifdef USE_ADF_SIMPLE_PIPELINE

namespace esphome {
namespace esp_audio {

class SimpleADFPipeline : public AudioMediaPipeline {

 public:
  void dump_config() override;
  void play(bool resume = false) override;
  void stop(bool pause = false) override;
  virtual void pause() override { this->stop(true); }
  virtual void resume() override { this->play(true); }
  AudioMediaPipelineState loop() override;

 protected:
  void play_announcement_(const std::string& url) override;
  void play_announcement_(audio::AudioFile *media_file) override;
  void pipeline_init_() override;
  void pipeline_deinit_() override;
  void pipeline_play_(bool resume) override;
  void pipeline_stop_(bool pause, bool cleanup) override;
  void switch_pipeline_input_(AudioMediaInputType input_type);
  
  audio_pipeline_handle_t pipeline_{nullptr};
  audio_element_handle_t http_stream_reader_{nullptr};
  audio_element_handle_t esp_decoder_{nullptr};
  audio_element_handle_t flash_stream_reader_{nullptr};
  bool is_music_info_set_{false};
  AudioMediaInputType input_type_{AudioMediaInputType::URL};
};

}  // namespace esp_audio
}  // namespace esphome
#endif // USE_ADF_SIMPLE_PIPELINE
