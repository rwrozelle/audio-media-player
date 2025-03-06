#pragma once

#ifdef USE_ESP_IDF

#include "esphome/components/media_player/media_player.h"
#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/core/helpers.h"
#include "simple_adf_pipeline.h"
#include "complex_adf_pipeline.h"
  
#include "audio_playlists.h"

namespace esphome {
namespace esp_adf {

class AudioMediaPlayer : public Component, public media_player::MediaPlayer {

 public:
   
  AudioMediaPlayer() : Component(), media_player::MediaPlayer()  {
    this->pipeline_ = new AdfMediaPipeline();
  }
  // ESPHome-Component implementations
  float get_setup_priority() const override { return esphome::setup_priority::LATE; }
  void dump_config() override;
  void setup() override;
  void publish_state();
  void loop() override;
  
  //Media Player
  media_player::MediaPlayerTraits get_traits() override;
  bool is_muted() const override { return this->muted_; }
  std::string repeat() const { return media_player_repeat_mode_to_string(this->repeat_); }
  bool is_shuffle() const override { return this->shuffle_; }
  std::string artist() const { return this->artist_; }
  std::string album() const { return this->album_; }
  std::string title() const { return this->title_; }
  std::string thumbnail_url() const { return this->thumbnail_url_; }
  int duration() const { return this->duration_; }
  int position() const { return this->position_; }
  
  //this
  void set_dout_pin(int pin) { this->dout_pin_ = pin; }
  void set_mclk_pin(int pin) { this->mclk_pin_ = pin; }
  void set_bclk_pin(int pin) { this->bclk_pin_ = pin; }
  void set_lrclk_pin(int pin) { this->lrclk_pin_ = pin; }
  void set_access_token(const std::string& token) { this->access_token_ = token; }
  void set_ffmpeg_server(const std::string& ffmpeg_server) { this->ffmpeg_server_ = ffmpeg_server; }
  void set_format(const std::string& format) { this->format_ = format; }
  void set_rate(int rate) { this->rate_ = rate; }
  void set_http_stream_rb_size(int rb_size) {this ->http_stream_rb_size_ = rb_size; }
  void set_esp_decoder_rb_size(int rb_size) {this ->esp_decoder_rb_size_ = rb_size; }
  void set_i2s_stream_rb_size(int rb_size) {this ->i2s_stream_rb_size_ = rb_size; }
  void set_pipeline_type(const std::string& pipeline_type) {
    if (pipeline_type == "COMPLEX") {
      this->pipeline_type_ = 1;
    }
    else {
      this->pipeline_type_ = 0;
    }
  }
  
  media_player::MediaPlayerState prior_state{media_player::MEDIA_PLAYER_STATE_NONE};

  //microseconds
  int64_t mrm_run_interval = 1500000L;
  //seconds
  int64_t pause_interval_sec = 600;

 protected:
  HighFrequencyLoopRequester high_freq_;

  //Media Player
  void control(const media_player::MediaPlayerCall &call) override;

  //this 
  void on_pipeline_state_change(AdfPipelineState state);
    
  void start_();
  void stop_();
  void pause_();
  void resume_();
  bool is_announcement_();
  void set_volume_(float volume, bool publish = true);
  void mute_();
  void unmute_();
  
  void pipeline_start_();
  void pipeline_stop_();
  void pipeline_pause_();
  void pipeline_resume_();

  void set_repeat_(media_player::MediaPlayerRepeatMode repeat);
  void set_shuffle_(bool shuffle);
  void set_artist_(const std::string& artist) {artist_ = artist;}
  void set_album_(const std::string& album) {album_ = album;}
  void set_title_(const std::string& title) {title_ = title;}
  void set_thumbnail_url_(const std::string& thumbnail_url) {thumbnail_url_ = thumbnail_url;}
  void set_duration_(int duration) {duration_ = duration;}
  void set_position_(int position) {position_ = position;}
    
  void set_playlist_track_(ADFPlaylistTrack track);
  void play_next_track_on_playlist_(int track_id);

  int32_t get_timestamp_sec_();
  
  int dout_pin_{I2S_GPIO_UNUSED};
  int mclk_pin_{I2S_GPIO_UNUSED};
  int bclk_pin_{I2S_GPIO_UNUSED};
  int lrclk_pin_{I2S_GPIO_UNUSED};
  std::string access_token_{""};
  std::string ffmpeg_server_{"http://homeassistant.local:8123"};
  std::string format_{"flac"};
  uint32_t rate_{44100};
  int http_stream_rb_size_{50 * HTTP_STREAM_RINGBUFFER_SIZE};  
  int esp_decoder_rb_size_{ESP_DECODER_RINGBUFFER_SIZE};
  int i2s_stream_rb_size_{I2S_STREAM_RINGBUFFER_SIZE};
  int32_t pipeline_type_{0};
  AdfMediaPipeline *pipeline_{nullptr};

  AudioPlaylists audioPlaylists_;

  int force_publish_{false};

  bool muted_{false};
  media_player::MediaPlayerRepeatMode repeat_{media_player::MEDIA_PLAYER_REPEAT_OFF};
  bool shuffle_{false};
  std::string artist_{""};
  std::string album_{""};
  std::string title_{""};
  std::string thumbnail_url_{""};
  int duration_{0}; // in seconds
  int position_{0}; // in seconds
  bool play_intent_{false};
  bool turning_off_{false};
  int32_t timestamp_sec_{0};
  int32_t pause_timestamp_sec_{0};
  int32_t offset_sec_{0};

  int play_track_id_{-1};
  ADFPlaylistTrack current_track_;
  ADFPlaylistTrack empty_track_;
  AdfPipelineState prior_pipeline_state_{AdfPipelineState::STOPPED};
};

}  // namespace esp_adf
}  // namespace esphome

#endif  // USE_ESP_IDF
