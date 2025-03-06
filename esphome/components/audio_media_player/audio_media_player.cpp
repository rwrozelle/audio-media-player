#include "audio_media_player.h"

#ifdef USE_ESP_IDF

#include <audio_error.h>
#include <audio_mem.h>
#include <cJSON.h>

#include "esphome/core/log.h"

namespace esphome {
namespace esp_adf {

static const char *const TAG = "audio_media_player";


void AudioMediaPlayer::dump_config() {

  esph_log_config(TAG, "AudioMediaPlayer");
  esph_log_config(TAG, "volume increment %d%%", (int)(this->volume_increment_ * 100));
  esph_log_config(TAG, "volume max %d%%", (int)(this->volume_max_ * 100));
  esph_log_config(TAG, "volume min %d%%", (int)(this->volume_min_ * 100));
  this->pipeline_->dump_config();
}

void AudioMediaPlayer::setup() {

  this->state = media_player::MEDIA_PLAYER_STATE_OFF;
  
  this->volume = .25;
  if (this->volume > this->volume_max_) {
    this->volume = this->volume_max_;
  }
  else if (this->volume < this->volume_min_) {
    this->volume = this->volume_min_;
  }
  
  if (this->pipeline_type_ == 1) {
    this->pipeline_ = new ComplexAdfMediaPipeline();
  }
  else {
    this->pipeline_ = new SimpleAdfMediaPipeline();
  }
  pipeline_->set_dout_pin(this->dout_pin_);
  pipeline_->set_mclk_pin(this->mclk_pin_);
  pipeline_->set_bclk_pin(this->bclk_pin_);
  pipeline_->set_lrclk_pin(this->lrclk_pin_);
  pipeline_->set_access_token(this->access_token_);
  pipeline_->set_ffmpeg_server(this->ffmpeg_server_);
  pipeline_->set_format(this->format_);
  pipeline_->set_rate(this->rate_);
  pipeline_->set_http_stream_rb_size(this->http_stream_rb_size_);
  pipeline_->set_esp_decoder_rb_size(this->esp_decoder_rb_size_);
  pipeline_->set_i2s_stream_rb_size(this->i2s_stream_rb_size_);
}

void AudioMediaPlayer::publish_state() {

  esph_log_d(TAG, "MP State = %s, MP Prior State = %s", media_player_state_to_string(this->state), media_player_state_to_string(this->prior_state));
  if (this->state != this->prior_state || this->force_publish_) {
    switch (this->state) {
      case media_player::MEDIA_PLAYER_STATE_PLAYING:
        if (this->duration_ > 0 && this->timestamp_sec_ > 0) {
          this->set_position_(this->offset_sec_ + (this->get_timestamp_sec_() - this->timestamp_sec_));
        }
        else {
          this->set_position_(0);
        }
        break;
      case media_player::MEDIA_PLAYER_STATE_PAUSED:
        if (this->duration_ > 0 && this->timestamp_sec_ > 0) {
          this->set_position_(this->offset_sec_);
        }
        else {
          this->set_position_(0);
        }
        break;
      default:
        //set_duration_(0);
        this->set_position_(0);
        this->offset_sec_ = 0;
        break;
    }
    esph_log_d(TAG, "Publish State, position: %d, duration: %d",this->position(),this->duration());
    this->state_callback_.call();
    this->prior_state = this->state;
    this->force_publish_ = false;
  }
}

media_player::MediaPlayerTraits AudioMediaPlayer::get_traits() {

  auto traits = media_player::MediaPlayerTraits();
  traits.set_supports_pause( true );
  traits.set_supports_next_previous_track( true );
  traits.set_supports_turn_off_on( true );
  return traits;
}

// from Component
void AudioMediaPlayer::loop() {
  
    AdfPipelineState pipeline_state = this->pipeline_->loop();
    if (pipeline_state != this->prior_pipeline_state_) {
      this->on_pipeline_state_change(pipeline_state);
      this->prior_pipeline_state_ = pipeline_state;
    }
    
    if (pipeline_state == AdfPipelineState::PAUSED
        && ((this->get_timestamp_sec_() - this->pause_timestamp_sec_) > this->pause_interval_sec)) {
      this->play_intent_ = false;
      this->stop_();
    }
}

void AudioMediaPlayer::control(const media_player::MediaPlayerCall &call) {
  esph_log_d(TAG, "control call while state: %s", media_player_state_to_string(this->state));

  //Media File is sent (no command)
  if (call.get_media_url().has_value())
  {
    std::string media_url = call.get_media_url().value();

    //enqueue
    media_player::MediaPlayerEnqueue enqueue = media_player::MEDIA_PLAYER_ENQUEUE_PLAY;
    if (call.get_enqueue().has_value()) {
      enqueue = call.get_enqueue().value();
    }
    // announcing
    bool announcing = false;
    if (call.get_announcement().has_value()) {
      announcing = call.get_announcement().value();
    }

    // announcement media
    if (announcing) {
      if (this->state == media_player::MEDIA_PLAYER_STATE_OFF) {
        this->state = media_player::MEDIA_PLAYER_STATE_ON;
        publish_state();
      }
      this->pipeline_->set_url(call.get_media_url().value(), announcing);
    }
    //normal media, use enqueue value to determine what to do
    else {
      if (enqueue == media_player::MEDIA_PLAYER_ENQUEUE_REPLACE || enqueue == media_player::MEDIA_PLAYER_ENQUEUE_PLAY) {
        if (this->state == media_player::MEDIA_PLAYER_STATE_OFF) {
          this->state = media_player::MEDIA_PLAYER_STATE_ON;
          publish_state();
        }
        this->play_track_id_ = -1;
        if (enqueue == media_player::MEDIA_PLAYER_ENQUEUE_REPLACE) {
          this->audioPlaylists_.clean_playlist();
        }
        this->audioPlaylists_.playlist_add(call.get_media_url().value(), true,shuffle_);
        this->set_playlist_track_(this->audioPlaylists_.get_playlist()->front());
        this->play_intent_ = true;
        if (this->state == media_player::MEDIA_PLAYER_STATE_PLAYING || this->state == media_player::MEDIA_PLAYER_STATE_PAUSED) {
          this->play_track_id_ = 0;
          this->stop_();
          return;
        } else {
          this->start_();
        }
      }
      else if (enqueue == media_player::MEDIA_PLAYER_ENQUEUE_ADD) {
        this->audioPlaylists_.playlist_add(call.get_media_url().value(), true, shuffle_);
      }
      else if (enqueue == media_player::MEDIA_PLAYER_ENQUEUE_NEXT) {
        this->audioPlaylists_.playlist_add(call.get_media_url().value(), false,shuffle_);
      }
    }
  }
  // Volume value is sent (no command)
  if (call.get_volume().has_value()) {
    this->set_volume_(call.get_volume().value());
  }
  //Command
  if (call.get_command().has_value()) {
    switch (call.get_command().value()) {
      case media_player::MEDIA_PLAYER_COMMAND_PLAY:
        this->play_intent_ = true;
        this->play_track_id_ = -1;
        if (this->state == media_player::MEDIA_PLAYER_STATE_PLAYING) {
          this->stop_();
        }
        if (this->state == media_player::MEDIA_PLAYER_STATE_PAUSED) {
          this->resume_();
        }
        if (this->state == media_player::MEDIA_PLAYER_STATE_OFF 
        || this->state == media_player::MEDIA_PLAYER_STATE_ON 
        || this->state == media_player::MEDIA_PLAYER_STATE_NONE
        || this->state == media_player::MEDIA_PLAYER_STATE_IDLE) {
        
          int id = this->audioPlaylists_.next_playlist_track_id();
          if (id > -1) {
            this->set_playlist_track_((*this->audioPlaylists_.get_playlist())[id]);
          }
          start_();
        }
        break;
      case media_player::MEDIA_PLAYER_COMMAND_PAUSE:
        if (this->state == media_player::MEDIA_PLAYER_STATE_PLAYING) {
          this->play_track_id_ = this->audioPlaylists_.next_playlist_track_id();
          this->play_intent_ = false;
        }
        this->pause_();
        break;
      case media_player::MEDIA_PLAYER_COMMAND_STOP:
        this->play_intent_ = false;
        this->audioPlaylists_.clean_playlist();
        this->pause_();
        break;
      case media_player::MEDIA_PLAYER_COMMAND_MUTE:
        this->mute_();
        break;
      case media_player::MEDIA_PLAYER_COMMAND_UNMUTE:
        this->unmute_();
        break;
      case media_player::MEDIA_PLAYER_COMMAND_VOLUME_UP: {
        float new_volume = this->volume + this->volume_increment_;
        if (new_volume > this->volume_max_) {
          new_volume = this->volume_max_;
        }
        this->set_volume_(new_volume);
        break;
      }
      case media_player::MEDIA_PLAYER_COMMAND_VOLUME_DOWN: {
        float new_volume = this->volume - this->volume_increment_;
        if (new_volume < this->volume_min_) {
          new_volume = this->volume_min_;
        }
        this->set_volume_(new_volume);
        break;
      }
      case media_player::MEDIA_PLAYER_COMMAND_NEXT_TRACK: {
        if ( this->audioPlaylists_.get_playlist()->size() > 0 ) {
          this->play_intent_ = true;
          this->play_track_id_ = -1;
          this->stop_();
        }
        break;
      }
      case media_player::MEDIA_PLAYER_COMMAND_PREVIOUS_TRACK: {
        if ( this->audioPlaylists_.get_playlist()->size() > 0 ) {
          this->play_intent_ = true;
          this->play_track_id_ = this->audioPlaylists_.previous_playlist_track_id();
          this->stop_();
        }
        break;
      }
      case media_player::MEDIA_PLAYER_COMMAND_TOGGLE: {
        if (this->state == media_player::MEDIA_PLAYER_STATE_OFF) {
          this->state = media_player::MEDIA_PLAYER_STATE_ON;
          publish_state();
        }
        else {
          if (this->state == media_player::MEDIA_PLAYER_STATE_PLAYING 
          || this->state == media_player::MEDIA_PLAYER_STATE_PAUSED 
          || this->state == media_player::MEDIA_PLAYER_STATE_ANNOUNCING ) {
            this->turning_off_ = true;
            this->play_intent_ = false;
            this->stop_();
          }
          else {
            if (HighFrequencyLoopRequester::is_high_frequency()) {
              esph_log_d(TAG,"Set Loop to run normal cycle");
              this->high_freq_.stop();
            }
            this->pipeline_->clean_up();
            this->state = media_player::MEDIA_PLAYER_STATE_OFF;
            this->publish_state();
          }
        }
        break;
      case media_player::MEDIA_PLAYER_COMMAND_TURN_ON: {
        if (this->state == media_player::MEDIA_PLAYER_STATE_OFF) {
            this->state = media_player::MEDIA_PLAYER_STATE_ON;
            this->publish_state();
        }
        break;
      }
      case media_player::MEDIA_PLAYER_COMMAND_TURN_OFF: {
        if (this->state != media_player::MEDIA_PLAYER_STATE_OFF) {
          if (this->state == media_player::MEDIA_PLAYER_STATE_PLAYING 
          || this->state == media_player::MEDIA_PLAYER_STATE_PAUSED 
          || this->state == media_player::MEDIA_PLAYER_STATE_ANNOUNCING ) {
            this->turning_off_ = true;
            this->play_intent_ = false;
            this->stop_();
          }
          else {
            if (HighFrequencyLoopRequester::is_high_frequency()) {
              esph_log_d(TAG,"Set Loop to run normal cycle");
              this->high_freq_.stop();
            }
            this->pipeline_->clean_up();
            this->state = media_player::MEDIA_PLAYER_STATE_OFF;
            this->publish_state();
          }
        }
        break;
      }
      case media_player::MEDIA_PLAYER_COMMAND_CLEAR_PLAYLIST: {
        this->audioPlaylists_.clean_playlist();
        break;
      }
      case media_player::MEDIA_PLAYER_COMMAND_SHUFFLE: {
        this->set_shuffle_(true);
        break;
      }
      case media_player::MEDIA_PLAYER_COMMAND_UNSHUFFLE: {
        this->set_shuffle_(false);
        break;
      }
      case media_player::MEDIA_PLAYER_COMMAND_REPEAT_OFF: {
        this->set_repeat_(media_player::MEDIA_PLAYER_REPEAT_OFF);
        break;
      }
      case media_player::MEDIA_PLAYER_COMMAND_REPEAT_ONE: {
        this->set_repeat_(media_player::MEDIA_PLAYER_REPEAT_ONE);
        break;
      }
      case media_player::MEDIA_PLAYER_COMMAND_REPEAT_ALL: {
        this->set_repeat_(media_player::MEDIA_PLAYER_REPEAT_ALL);
        break;
      }
      default:
        break;
      }
    }
  }
}

void AudioMediaPlayer::on_pipeline_state_change(AdfPipelineState state) {
  esph_log_i(TAG, "got new pipeline state: %s", pipeline_state_to_string(state));
  switch (state) {
    case AdfPipelineState::STARTING:
     break;
    case AdfPipelineState::RESUMING:
     break;
    case AdfPipelineState::START_ANNOUNCING:
     break;
    case AdfPipelineState::ANNOUNCING:
      this->set_volume_( this->volume, false);
      this->state = media_player::MEDIA_PLAYER_STATE_ANNOUNCING;
      timestamp_sec_ = 0;
      this->set_artist_("");
      this->set_album_("");
      this->set_title_("Announcing");
      this->set_thumbnail_url_("");
      set_duration_(0);
      set_position_(0);
      this->publish_state();
      break;
    case AdfPipelineState::RUNNING:
      this->set_volume_( this->volume, false);
      this->state = media_player::MEDIA_PLAYER_STATE_PLAYING;
      timestamp_sec_ = get_timestamp_sec_();
      this->publish_state();
      break;
    case AdfPipelineState::STOPPING:
      break;
    case AdfPipelineState::STOP_ANNOUNCING:
      set_playlist_track_(this->current_track_);
      break;
    case AdfPipelineState::STOPPED:
      this->current_track_ = empty_track_;
      this->set_artist_("");
      this->set_album_("");
      this->set_title_("");
      this->set_thumbnail_url_("");
      //this->set_duration_(0);
      //this->set_position_(0);
      this->state = media_player::MEDIA_PLAYER_STATE_IDLE;
      this->publish_state();
      if (this->turning_off_) {
        if (HighFrequencyLoopRequester::is_high_frequency()) {
          esph_log_d(TAG,"Set Loop to run normal cycle");
          this->high_freq_.stop();
        }
        this->pipeline_->clean_up();
        this->state = media_player::MEDIA_PLAYER_STATE_OFF;
        this->publish_state();
        this->turning_off_ = false;
      }
      else {
        if (this->play_intent_) {
          this->play_next_track_on_playlist_(this->play_track_id_);
          this->play_track_id_ = -1;
        }
        if (this->play_intent_) {
          this->start_();
        }
        else {
          if (HighFrequencyLoopRequester::is_high_frequency()) {
            esph_log_d(TAG,"Set Loop to run normal cycle");
            this->high_freq_.stop();
          }
          this->pipeline_->clean_up();
        }
      }
      break;
    case AdfPipelineState::PAUSING:
      break;
    case AdfPipelineState::PAUSED:
      this->offset_sec_ = offset_sec_ + (get_timestamp_sec_() - timestamp_sec_);
      this->pause_timestamp_sec_ = get_timestamp_sec_();
      this->state = media_player::MEDIA_PLAYER_STATE_PAUSED;
      this->publish_state();
      this->high_freq_.stop();
      break;
    default:
      break;
  }
}

void AudioMediaPlayer::start_() 
{
  esph_log_d(TAG,"start_()");
  this->pipeline_start_();
}

void AudioMediaPlayer::stop_() {
  esph_log_d(TAG,"stop_()");
  this->pipeline_stop_();
}

void AudioMediaPlayer::pause_() {
  esph_log_d(TAG,"pause_()");
  this->pipeline_pause_();
}

void AudioMediaPlayer::resume_()
{
  esph_log_d(TAG,"resume_()");
  this->pipeline_resume_();
}

void AudioMediaPlayer::set_volume_(float volume, bool publish) {
  
  float new_volume = volume;
  if (new_volume > this->volume_max_) {
    new_volume = this->volume_max_;
  }
  else if (new_volume < this->volume_min_) {
    new_volume = this->volume_min_;
  }
  this->pipeline_->set_volume(round(100 * new_volume));
  this->volume = new_volume;
  if (publish) {
    this->force_publish_ = true;
    this->publish_state();
  }
}

void AudioMediaPlayer::mute_() {
  this->pipeline_->mute();
  this->muted_ = true;
  this->force_publish_ = true;
  this->publish_state();
}

void AudioMediaPlayer::unmute_() {
  this->pipeline_->unmute();
  this->muted_ = false;
  this->force_publish_ = true;
  this->publish_state();
}

void AudioMediaPlayer::pipeline_start_() {
  
  if (this->state == media_player::MEDIA_PLAYER_STATE_OFF 
  || this->state == media_player::MEDIA_PLAYER_STATE_ON 
  || this->state == media_player::MEDIA_PLAYER_STATE_NONE
  || this->state == media_player::MEDIA_PLAYER_STATE_IDLE) {
    esph_log_d(TAG,"pipeline_start_()");
    if (this->state == media_player::MEDIA_PLAYER_STATE_OFF) {
      this->state = media_player::MEDIA_PLAYER_STATE_ON;
      this->publish_state();
    }
    if (!HighFrequencyLoopRequester::is_high_frequency()) {
      esph_log_d(TAG,"Set Loop to run at high frequency cycle");
      this->high_freq_.start();
    }
    this->pipeline_->play();
  }
}

void AudioMediaPlayer::pipeline_stop_() {
  if (this->state == media_player::MEDIA_PLAYER_STATE_PLAYING || this->state == media_player::MEDIA_PLAYER_STATE_PAUSED) {
    esph_log_d(TAG,"pipeline_stop_()");
    this->pipeline_->stop();
  }
}

void AudioMediaPlayer::pipeline_pause_() {
  if (this->state == media_player::MEDIA_PLAYER_STATE_PLAYING) {
    esph_log_d(TAG,"pipeline_pause_()");
    this->pipeline_->pause();
  }
}

void AudioMediaPlayer::pipeline_resume_()
{
  if (this->state == media_player::MEDIA_PLAYER_STATE_PAUSED) {
    if (!HighFrequencyLoopRequester::is_high_frequency()) {
      esph_log_d(TAG,"Set Loop to run at high frequency cycle");
      this->high_freq_.start();
    }
    esph_log_d(TAG,"pipeline_resume_()");
    this->pipeline_->resume();
  }
}

void AudioMediaPlayer::set_repeat_(media_player::MediaPlayerRepeatMode repeat) {
  this->repeat_ = repeat;
  this->force_publish_ = true;
  this->publish_state();
}

void AudioMediaPlayer::set_shuffle_(bool shuffle) {
  unsigned int vid = this->audioPlaylists_.get_playlist()->size();
  if (vid > 0) {
    this->audioPlaylists_.shuffle_playlist(shuffle);
    this->shuffle_ = shuffle;
    this->force_publish_ = true;
    this->publish_state();
    this->play_intent_ = true;
    this->play_track_id_ = 0;
    this->stop_();
  }
}

void AudioMediaPlayer::set_playlist_track_(ADFPlaylistTrack track) {
  esph_log_v(TAG, "uri: %s", track.url.c_str());
  if (track.artist == "") {
	this->set_artist_(track.playlist);
  } else {
	this->set_artist_(track.artist);
  }
  this->set_album_(track.album);
  if (track.title == "") {
    this->set_title_(track.url);
  }
  else {
    this->set_title_(track.title);
  }
  this->set_thumbnail_url_(track.thumbnail_url);
  this->set_duration_(track.duration);
  this->offset_sec_ = 0;
  this->set_position_(0);

  esph_log_d(TAG, "set_playlist_track: %s: %s: %s duration: %d %s",
     this->artist_.c_str(), this->album_.c_str(), this->title_.c_str(), this->duration_, track.url.c_str());
  this->pipeline_->set_url(track.url);
  this->current_track_ = track;
}

void AudioMediaPlayer::play_next_track_on_playlist_(int track_id) {

  unsigned int vid = this->audioPlaylists_.get_playlist()->size();
  if (this->audioPlaylists_.get_playlist()->size() > 0) {
    if (this->repeat_ != media_player::MEDIA_PLAYER_REPEAT_ONE) {
      this->audioPlaylists_.set_playlist_track_as_played(track_id);
    }
       int id = this->audioPlaylists_.next_playlist_track_id();
    if (id > -1) {
      this->set_playlist_track_((*this->audioPlaylists_.get_playlist())[id]);
    }
    else {
      if (this->repeat_ == media_player::MEDIA_PLAYER_REPEAT_ALL) {
        for(unsigned int i = 0; i < vid; i++)
        {
          (*this->audioPlaylists_.get_playlist())[i].is_played = false;
        }
        this->set_playlist_track_((*this->audioPlaylists_.get_playlist())[0]);
      }
      else {
        this->audioPlaylists_.clean_playlist();
        this->play_intent_ = false;
      }
    }
  }
}

int32_t AudioMediaPlayer::get_timestamp_sec_() {
  struct timeval tv_now;
  gettimeofday(&tv_now, NULL);
  return (int32_t)tv_now.tv_sec;
}

}  // namespace esp_adf
}  // namespace esphome

#endif  // USE_ESP_IDF