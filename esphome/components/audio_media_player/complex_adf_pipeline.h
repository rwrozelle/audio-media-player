#pragma once

#include "audio_media_pipeline.h"

#ifdef USE_ADF_COMPLEX_PIPELINE

namespace esphome {
namespace esp_audio {

class ComplexADFPipeline : public AudioMediaPipeline {

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
  void pipeline_1_init_();
  void pipeline_2_init_();
  void pipeline_3_init_();
  void pipeline_deinit_() override;
  void pipeline_1_deinit_();
  void pipeline_2_deinit_();
  void pipeline_3_deinit_();
  void pipeline_play_(bool resume) override;
  void pipeline_stop_(bool pause, bool cleanup) override;
  void pipeline_announce_();
  void switch_pipeline_1_input_(AudioMediaInputType input_type);
  void switch_pipeline_2_input_(AudioMediaInputType input_type);

  audio_pipeline_handle_t pipeline_1_{nullptr};
  audio_pipeline_handle_t pipeline_2_{nullptr};
  audio_pipeline_handle_t pipeline_3_{nullptr};
  audio_element_handle_t http_stream_reader_1_{nullptr};
  audio_element_handle_t http_stream_reader_2_{nullptr};
  audio_element_handle_t flash_stream_reader_1_{nullptr};
  audio_element_handle_t flash_stream_reader_2_{nullptr};
  audio_element_handle_t esp_decoder_1_{nullptr};
  audio_element_handle_t esp_decoder_2_{nullptr};
  audio_element_handle_t rsp_filter_1_{nullptr};
  audio_element_handle_t rsp_filter_2_{nullptr};
  audio_element_handle_t raw_writer_1_{nullptr};
  audio_element_handle_t raw_writer_2_{nullptr};
  audio_element_handle_t downmixer_{nullptr};
  
  AudioMediaInputType input_type_1_{AudioMediaInputType::URL};
  AudioMediaInputType input_type_2_{AudioMediaInputType::URL};
  
  bool is_music_info_set_1_{false};
  bool is_music_info_set_2_{false};
  bool is_play_announcement_{false};
};

}  // namespace esp_audio
}  // namespace esphome

#endif  // USE_ADF_COMPLEX_PIPELINE
