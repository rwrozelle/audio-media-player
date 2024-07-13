#pragma once

#ifdef USE_ESP_IDF

#include "esphome/components/media_player/media_player.h"
#include "esphome/components/i2s_audio/i2s_audio.h"
#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/core/helpers.h"
#include "simple_adf_pipeline.h"
#include "multi_room_audio.h"
#include "audio_playlists.h"

namespace esphome {
namespace esp_adf {

class AudioMediaPlayer : public Component, public media_player::MediaPlayer, public i2s_audio::I2SAudioOut {

 public:
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
  int duration() const { return this->duration_; }
  int position() const { return this->position_; }
  
  //this
  void set_dout_pin(uint8_t pin) { this->pipeline_.set_dout_pin(pin); }
#if SOC_I2S_SUPPORTS_DAC
  void set_internal_dac_mode(i2s_dac_mode_t mode) { this->pipeline_.set_internal_dac_mode(mode); }
#endif
  void set_external_dac_channels(uint8_t channels) { this->pipeline_.set_external_dac_channels(channels); }
  void set_i2s_comm_fmt_lsb(bool lsb) { this->pipeline_.set_i2s_comm_fmt_lsb(lsb); }
  void set_use_adf_alc(bool use_alc){ this->pipeline_.set_use_adf_alc(use_alc); }

  media_player::MediaPlayerState prior_state{media_player::MEDIA_PLAYER_STATE_NONE};

  int64_t mrm_run_interval = 1500000L;

 protected:
  HighFrequencyLoopRequester high_freq_;

  //Media Player
  void control(const media_player::MediaPlayerCall &call) override;

  //this
  void on_pipeline_state_change(SimpleAdfPipelineState state);
    
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
  void set_duration_(int duration) {duration_ = duration;}
  void set_position_(int position) {position_ = position;}
  
  void set_playlist_track_(ADFPlaylistTrack track);
  void play_next_track_on_playlist_(int track_id);
  bool play_next_track_on_announcements_();

  int32_t get_timestamp_sec_();

/*
  void mrm_process_recv_actions_();
  void mrm_process_send_actions_();
  void mrm_sync_position_(int64_t timestamp, int64_t position);
  esp_err_t i2s_stream_sync_delay_(audio_element_handle_t i2s_stream, int32_t delay_size);
*/

  SimpleAdfMediaPipeline pipeline_;
  MultiRoomAudio multiRoomAudio_;
  AudioPlaylists audioPlaylists_;

  int force_publish_{false};

  bool muted_{false};
  media_player::MediaPlayerRepeatMode repeat_{media_player::MEDIA_PLAYER_REPEAT_OFF};
  bool shuffle_{false};
  std::string artist_{""};
  std::string album_{""};
  std::string title_{""};
  int duration_{0}; // in seconds
  int position_{0}; // in seconds
  bool play_intent_{false};
  bool turning_off_{false};
  int32_t timestamp_sec_{0};

  int play_track_id_{-1};
  //int64_t mrm_position_timestamp_{0};
  //int mrm_position_interval_sec_{10};
  SimpleAdfPipelineState prior_pipeline_state_{SimpleAdfPipelineState::STOPPED};
};

}  // namespace esp_adf
}  // namespace esphome

#endif  // USE_ESP_IDF
