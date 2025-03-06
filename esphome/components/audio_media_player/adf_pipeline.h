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

enum AdfPipelineState : uint8_t { STARTING=0, RUNNING, STOPPING, STOPPED, PAUSING, PAUSED, RESUMING, START_ANNOUNCING, ANNOUNCING, STOP_ANNOUNCING };

const char *pipeline_state_to_string(AdfPipelineState state);
const char *audio_element_status_to_string(audio_element_status_t status);

class AdfMediaPipeline {

 public:
  static esp_err_t http_stream_event_handle_(http_stream_event_msg_t *msg);
  
  void set_dout_pin(int pin) { this->dout_pin_ = pin; }
  void set_mclk_pin(int pin) { this->mclk_pin_ = pin; }
  void set_bclk_pin(int pin) { this->bclk_pin_ = pin; }
  void set_lrclk_pin(int pin) { this->lrclk_pin_ = pin; }
  static void set_access_token(const std::string& token) { AdfMediaPipeline::access_token = token; }
  void set_ffmpeg_server(const std::string& ffmpeg_server) { this->ffmpeg_server_ = ffmpeg_server; }
  void set_format(const std::string& format) { this->format_ = format; }
  void set_rate(int rate) { this->rate_ = rate; }
  void set_http_stream_rb_size(int rb_size) {this ->http_stream_rb_size_ = rb_size; }
  void set_esp_decoder_rb_size(int rb_size) {this ->esp_decoder_rb_size_ = rb_size; }
  void set_i2s_stream_rb_size(int rb_size) {this ->i2s_stream_rb_size_ = rb_size; }

  virtual void dump_config();
  virtual AdfPipelineState loop();
  virtual void set_url(const std::string& url, bool is_announcement = false);
  virtual void play(bool resume = false);
  virtual void stop(bool pause = false);
  virtual void pause() { this->stop(true); }
  virtual void resume() { this->play(true); }
  void clean_up();
  void set_volume(int volume);
  void mute();
  void unmute();
  bool is_announcement() { return this->is_announcement_; }
  
  static std::string access_token;

 protected:
  virtual void pipeline_init_();
  virtual void pipeline_deinit_();
  virtual void pipeline_play_(bool resume);
  virtual void pipeline_stop_(bool pause, bool cleanup);
  virtual void play_announcement_(const std::string& url);
  void set_state_(AdfPipelineState state);
  bool isServerTranscoding_();
  std::string url_encode_(const std::string& input);
  std::string get_transcode_url_(const std::string& url);
    
  int dout_pin_{I2S_GPIO_UNUSED};
  int mclk_pin_{I2S_GPIO_UNUSED};
  int bclk_pin_{I2S_GPIO_UNUSED};
  int lrclk_pin_{I2S_GPIO_UNUSED};
  
  std::string ffmpeg_server_{"http://homeassistant.local:8123"};
  std::string format_{"flac"};
  uint32_t rate_{44100};
  uint32_t bits_{16};
  uint32_t ch_{2};
  
  int http_stream_rb_size_{50 * HTTP_STREAM_RINGBUFFER_SIZE};
  int http_stream_task_core_{HTTP_STREAM_TASK_CORE};
  int http_stream_task_prio_{HTTP_STREAM_TASK_PRIO};
  
  int esp_decoder_rb_size_{ESP_DECODER_RINGBUFFER_SIZE};
  int esp_decoder_task_core_{ESP_DECODER_TASK_CORE};
  int esp_decoder_task_prio_{ESP_DECODER_TASK_PRIO};
  
  int i2s_stream_rb_size_{I2S_STREAM_RINGBUFFER_SIZE};
  int i2s_stream_task_core_{I2S_STREAM_TASK_CORE};
  int i2s_stream_task_prio_{I2S_STREAM_TASK_PRIO};
  
  bool is_announcement_{false};
  bool use_adf_alc_{true};
  int volume_{25}; //between 0 and 100
  
  audio_element_handle_t i2s_stream_writer_{nullptr};
  audio_event_iface_handle_t evt_{nullptr};
  AdfPipelineState state_{AdfPipelineState::STOPPED};
  std::string url_{""};
  bool is_launched_{false};
  bool is_initialized_{false};
};

}  // namespace esp_adf
}  // namespace esphome

#endif  // USE_ESP_IDF
