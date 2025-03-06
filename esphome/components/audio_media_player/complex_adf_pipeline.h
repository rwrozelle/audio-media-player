#pragma once

#ifdef USE_ESP_IDF

#include "adf_pipeline.h"

namespace esphome {
namespace esp_adf {

class ComplexAdfMediaPipeline : public AdfMediaPipeline {

 public:
  void dump_config() override;
  void set_url(const std::string& url, bool is_announcement = false) override;
  void play(bool resume = false) override;
  void stop(bool pause = false) override;
  virtual void pause() override { this->stop(true); }
  virtual void resume() override { this->play(true); }
  AdfPipelineState loop() override;

 protected:
  void pipeline_init_() override;
  void pipeline_deinit_() override;
  void pipeline_play_(bool resume) override;
  void pipeline_stop_(bool pause, bool cleanup) override;
  void play_announcement_(const std::string& url) override;
  void pipeline_announce_();

  audio_pipeline_handle_t pipeline_1_{nullptr};
  audio_pipeline_handle_t pipeline_2_{nullptr};
  audio_pipeline_handle_t pipeline_3_{nullptr};
  audio_element_handle_t http_stream_reader_1_{nullptr};
  audio_element_handle_t http_stream_reader_2_{nullptr};
  audio_element_handle_t esp_decoder_1_{nullptr};
  audio_element_handle_t esp_decoder_2_{nullptr};
  audio_element_handle_t rsp_filter_1_{nullptr};
  audio_element_handle_t rsp_filter_2_{nullptr};
  audio_element_handle_t raw_writer_1_{nullptr};
  audio_element_handle_t raw_writer_2_{nullptr};
  audio_element_handle_t downmixer_{nullptr};
  
  bool is_music_info_set_1_{false};
  bool is_music_info_set_2_{false};
};

}  // namespace esp_adf
}  // namespace esphome

#endif  // USE_ESP_IDF
