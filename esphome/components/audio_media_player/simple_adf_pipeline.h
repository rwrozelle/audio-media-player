#pragma once

#ifdef USE_ESP_IDF

#include "driver/i2s_std.h"
#include <http_stream.h>
#include <esp_decoder.h>
#include "i2s_stream_mod.h"
#include <audio_pipeline.h>
#include <string>
#include "esphome/components/media_player/media_player.h"

namespace esphome {
namespace esp_adf {

enum SimpleAdfPipelineState : uint8_t { STARTING=0, RUNNING, STOPPING, STOPPED, PAUSING, PAUSED, RESUMING };

const char *pipeline_state_to_string(SimpleAdfPipelineState state);
const char *audio_element_status_to_string(audio_element_status_t status);

class SimpleAdfMediaPipeline {

 public:

  void set_dout_pin(int pin) { this->dout_pin_ = pin; }
  void set_mclk_pin(int pin) { this->mclk_pin_ = pin; }
  void set_bclk_pin(int pin) { this->bclk_pin_ = pin; }
  void set_lrclk_pin(int pin) { this->lrclk_pin_ = pin; }
  static void set_access_token(const std::string& token) { SimpleAdfMediaPipeline::access_token = token; }
  void set_ffmpeg_server(const std::string& ffmpeg_server) { this->ffmpeg_server_ = ffmpeg_server; }
  void set_format(const std::string& format) { this->format_ = format; }
  void set_rate(int rate) { this->rate_ = rate; }
  void set_http_stream_rb_size(int rb_size) {this ->http_stream_rb_size_ = rb_size; }
  void set_esp_decoder_rb_size(int rb_size) {this ->esp_decoder_rb_size_ = rb_size; }
  void set_i2s_stream_rb_size(int rb_size) {this ->i2s_stream_rb_size_ = rb_size; }

  void dump_config();
  void set_url(const std::string& url, bool is_announcement = false);
  void set_launch_timestamp(int64_t launch_timestamp) {this->launch_timestamp_ = launch_timestamp; }
  void play(bool resume = false);
  void stop(bool pause = false);
  void pause() { stop(true); }
  void resume() { this->play(true); }
  void set_volume(int volume);
  void mute();
  void unmute();
  void clean_up();
  SimpleAdfPipelineState loop();
  bool is_announcement() { return this->is_announcement_; }
  //audio_element_handle_t get_esp_decoder() { return this->esp_decoder_; }
  //audio_element_handle_t get_i2s_stream_writer() { return this->i2s_stream_writer_; }
  //SimpleAdfPipelineState get_state() { return this->state_; }
  
  static std::string access_token;

 protected:  
  void pipeline_init_();
  void pipeline_deinit_();
  int64_t get_timestamp_();
  void pipeline_run_();
  void set_state_(SimpleAdfPipelineState state);
  bool uninstall_i2s_driver_();
  std::string url_encode_(const std::string& input);

  int http_stream_rb_size_{50 * HTTP_STREAM_RINGBUFFER_SIZE};
  int http_stream_task_core_{HTTP_STREAM_TASK_CORE};
  int http_stream_task_prio_{HTTP_STREAM_TASK_PRIO};
  
  int esp_decoder_rb_size_{ESP_DECODER_RINGBUFFER_SIZE};
  int esp_decoder_task_core_{ESP_DECODER_TASK_CORE};
  int esp_decoder_task_prio_{ESP_DECODER_TASK_PRIO};
  
  int i2s_stream_rb_size_{I2S_STREAM_RINGBUFFER_SIZE};
  int i2s_stream_task_core_{I2S_STREAM_TASK_CORE};
  int i2s_stream_task_prio_{I2S_STREAM_TASK_PRIO};
  
  int dout_pin_{I2S_GPIO_UNUSED};
  int mclk_pin_{I2S_GPIO_UNUSED};
  int bclk_pin_{I2S_GPIO_UNUSED};
  int lrclk_pin_{I2S_GPIO_UNUSED};
  
  bool use_adf_alc_{true};
  int volume_{25}; //between 0 and 100

  audio_pipeline_handle_t pipeline_{nullptr};
  audio_element_handle_t http_stream_reader_{nullptr};
  audio_element_handle_t esp_decoder_{nullptr};
  audio_element_handle_t i2s_stream_writer_{nullptr};
  audio_event_iface_handle_t evt_{nullptr};
  SimpleAdfPipelineState state_{SimpleAdfPipelineState::STOPPED};
  std::string url_{""};
  bool is_announcement_{false};
  bool trying_to_launch_{false};
  bool is_launched_{false};
  int64_t launch_timestamp_{0};
  bool is_initialized_{false};
  bool is_music_info_set_{false};
  uint32_t rate_{44100};
  uint32_t bits_{16};
  uint32_t ch_{2};
  std::string format_{"flac"};
  std::string ffmpeg_server_{"http://homeassistant.local:8123"};
};

}  // namespace esp_adf
}  // namespace esphome

#endif  // USE_ESP_IDF
