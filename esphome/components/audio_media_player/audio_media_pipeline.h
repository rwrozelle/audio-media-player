#pragma once

#ifdef USE_ESP_IDF

#include "driver/i2s_std.h"
#include <string>
#include <deque>
#include "esphome/components/media_player/media_player.h"
#include "audio_file.h"

#include <http_stream.h>
#include "embed_flash_stream_mstr.h"
#include <esp_http_client.h>
#include <esp_decoder.h>
#include "i2s_stream_mod.h"
#include <downmix.h>
#include <filter_resample.h>
#include <raw_stream.h>
#include <audio_pipeline.h>
#include <audio_element.h>

namespace esphome {
namespace esp_audio {


enum AudioMediaPipelineState : uint8_t { STARTING=0, RUNNING, STOPPING, STOPPED, PAUSING, PAUSED, RESUMING, STARTING_ANNOUNCING, ANNOUNCING, STOPPING_ANNOUNCING, STOPPED_ANNOUNCING };
const char *pipeline_state_to_string(AudioMediaPipelineState state);
enum AudioMediaInputType : uint8_t { URL=0, FLASH };
const char *audio_media_input_type_to_string(AudioMediaInputType type);

const char *audio_element_status_to_string(audio_element_status_t status);

class AudioAnouncement {
  public:
    optional<std::string> url;
    optional<audio::AudioFile *> file;
};

class AudioMediaPipeline {

 public:  
  
  void set_dout_pin(int pin) { this->dout_pin_ = pin; }
  void set_mclk_pin(int pin) { this->mclk_pin_ = pin; }
  void set_bclk_pin(int pin) { this->bclk_pin_ = pin; }
  void set_lrclk_pin(int pin) { this->lrclk_pin_ = pin; }
  static void set_access_token(const std::string& token) { AudioMediaPipeline::access_token = token; }
  void set_ffmpeg_server(const std::string& ffmpeg_server) { this->ffmpeg_server_ = ffmpeg_server; }
  void set_format(const std::string& format) { this->format_ = format; }
  void set_rate(int rate) { this->rate_ = rate; }
  void set_http_stream_rb_size(int rb_size) {this ->http_stream_rb_size_ = rb_size; }
  void set_esp_decoder_rb_size(int rb_size) {this ->esp_decoder_rb_size_ = rb_size; }
  void set_i2s_stream_rb_size(int rb_size) {this ->i2s_stream_rb_size_ = rb_size; }

  virtual void dump_config();
  virtual AudioMediaPipelineState loop();
  void set_url(const std::string& url, bool is_announcement = false, media_player::MediaPlayerEnqueue enqueue = media_player::MEDIA_PLAYER_ENQUEUE_PLAY);
  void play_file(audio::AudioFile *media_file, media_player::MediaPlayerEnqueue enqueue = media_player::MEDIA_PLAYER_ENQUEUE_PLAY);
  virtual void play(bool resume = false);
  virtual void stop(bool pause = false);
  virtual void pause() { this->stop(true); }
  virtual void resume() { this->play(true); }
  void clean_up();
  void set_volume(int volume);
  void mute();
  void unmute();
  bool is_announcement() { return this->is_announcement_; }
  
  static esp_err_t http_stream_event_handle_(http_stream_event_msg_t *msg);
  
  static std::string access_token;

 protected:
  virtual void play_announcement_(const std::string& url);
  virtual void play_announcement_(audio::AudioFile *media_file);
  virtual void pipeline_init_();
  virtual void pipeline_deinit_();
  virtual void pipeline_play_(bool resume);
  virtual void pipeline_stop_(bool pause, bool cleanup);
  void set_state_(AudioMediaPipelineState state);
  bool isServerTranscoding_();
  std::string url_encode_(const std::string& input);
  std::string get_transcode_url_(const std::string& url);
  audio_pipeline_handle_t adf_audio_pipeline_init();
  audio_element_handle_t adf_http_stream_init(int http_stream_rb_size);
  audio_element_handle_t adf_embed_flash_stream_init();
  audio_element_handle_t adf_esp_decoder_init();
  audio_element_handle_t adf_rsp_filter_init();
  audio_element_handle_t adf_raw_stream_init();
  audio_element_handle_t adf_downmix_init();
  audio_element_handle_t adf_i2s_stream_init();
    
  int dout_pin_{I2S_GPIO_UNUSED};
  int mclk_pin_{I2S_GPIO_UNUSED};
  int bclk_pin_{I2S_GPIO_UNUSED};
  int lrclk_pin_{I2S_GPIO_UNUSED};
  
  std::string ffmpeg_server_{"http://homeassistant.local:8123"};
  std::string format_{"flac"};
  uint32_t rate_{44100};
  uint32_t bits_{16};
  uint32_t ch_{2};
  
  int http_stream_rb_size_{50 * 20 * 1024};
  int http_stream_task_core_{0};
  int http_stream_task_prio_{4};
  
  int esp_decoder_rb_size_{10 * 1024};
  int esp_decoder_task_core_{0};
  int esp_decoder_task_prio_{5};
  
  int i2s_stream_rb_size_{8 * 1024};
  int i2s_stream_task_core_{0};
  int i2s_stream_task_prio_{23};
  
  audio_element_handle_t i2s_stream_writer_{nullptr};
  audio_event_iface_handle_t evt_{nullptr};
  
  bool is_announcement_{false};
  
  std::deque<AudioAnouncement> announcements_;
  
  bool use_adf_alc_{true};
  int volume_{25}; //between 0 and 100
  
  std::string url_{""};
  bool is_launched_{false};
  bool is_initialized_{false};
  
  AudioMediaPipelineState prior_state_{AudioMediaPipelineState::STOPPED};
  AudioMediaPipelineState state_{AudioMediaPipelineState::STOPPED};
};

}  // namespace esp_audio
}  // namespace esphome

#endif  // USE_ESP_IDF
