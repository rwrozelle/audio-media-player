#pragma once

#ifdef USE_ESP_IDF

#include "driver/i2s.h"
#include <audio_pipeline.h>
#include <string>
#include "esphome/components/media_player/media_player.h"
#include "esphome/components/i2s_audio/i2s_audio.h"

namespace esphome {
namespace esp_adf {

enum SimpleAdfPipelineState : uint8_t { STARTING=0, RUNNING, STOPPING, STOPPED, PAUSING, PAUSED, RESUMING };

const char *pipeline_state_to_string(SimpleAdfPipelineState state);
const char *audio_element_status_to_string(audio_element_status_t status);

class SimpleAdfMediaPipeline : public i2s_audio::I2SAudioOut {
//class SimpleAdfMediaPipeline {

 public:
  int http_stream_rb_size{(10 * 1024)};
  int esp_decoder_rb_size{(20 * 1024)};
  int i2s_stream_rb_size{(20 * 1024)};

  void set_dout_pin(uint8_t pin) { this->dout_pin_ = pin; }
#if SOC_I2S_SUPPORTS_DAC
  void set_internal_dac_mode(i2s_dac_mode_t mode) { this->internal_dac_mode_ = mode; }
#endif
  void set_external_dac_channels(uint8_t channels) { this->external_dac_channels_ = channels; }
  void set_i2s_comm_fmt_lsb(bool lsb) { this->i2s_comm_fmt_lsb_ = lsb; }
  void set_use_adf_alc(bool use_alc){ this->use_adf_alc_ = use_alc; }

  media_player::MediaPlayerState prior_state{media_player::MEDIA_PLAYER_STATE_NONE};

  void dump_config();
  void set_url(const std::string& url, bool is_announcement = false);
  void set_launch_timestamp(int64_t launch_timestamp) {launch_timestamp_ = launch_timestamp; }
  void play(bool resume = false);
  void stop(bool pause = false);
  void pause() { stop(true); }
  void resume() { play(true); }
  void set_volume(int volume);
  void mute();
  void unmute();
  SimpleAdfPipelineState loop();
  bool is_announcement() { return is_announcement_; }
  audio_element_handle_t get_esp_decoder() { return esp_decoder_; }
  audio_element_handle_t get_i2s_stream_writer() { return i2s_stream_writer_; }
  SimpleAdfPipelineState get_state() { return state_; }

 protected:
  void pipeline_init_();
  void pipeline_deinit_();
  int64_t get_timestamp_();
  void pipeline_run_();
  void set_state_(SimpleAdfPipelineState state);
  bool uninstall_i2s_driver_();

  uint8_t dout_pin_{0};
#if SOC_I2S_SUPPORTS_DAC
  i2s_dac_mode_t internal_dac_mode_{I2S_DAC_CHANNEL_DISABLE};
#endif
  uint8_t external_dac_channels_;
  bool i2s_comm_fmt_lsb_;
  bool use_adf_alc_{false};
  int volume_{25}; //between 0 and 100

  audio_pipeline_handle_t pipeline_{};
  audio_element_handle_t http_stream_reader_{};
  audio_element_handle_t esp_decoder_{};
  audio_element_handle_t i2s_stream_writer_{};
  audio_event_iface_handle_t evt_{};
  audio_event_iface_msg_t msg_;
  SimpleAdfPipelineState state_{SimpleAdfPipelineState::STOPPED};
  std::string url_{"https://dl.espressif.com/dl/audio/ff-16b-2c-44100hz.mp3"};
  bool is_announcement_{false};
  bool trying_to_launch_{false};
  bool is_launched_{false};
  int64_t launch_timestamp_{0};

  uint32_t sample_rate_{44100};
  i2s_bits_per_sample_t bits_per_sample_{I2S_BITS_PER_SAMPLE_16BIT};
  i2s_channel_fmt_t channel_fmt_{I2S_CHANNEL_FMT_RIGHT_LEFT};
};

}  // namespace esp_adf
}  // namespace esphome

#endif  // USE_ESP_IDF
