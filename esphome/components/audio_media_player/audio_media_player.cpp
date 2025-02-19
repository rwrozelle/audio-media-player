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
  pipeline_.dump_config();
}

void AudioMediaPlayer::setup() {

  this->state = media_player::MEDIA_PLAYER_STATE_OFF;
  this->pipeline_.set_parent(this->parent_);
  this->volume = .25;
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
  traits.set_supports_grouping( true );
  return traits;
}

// from Component
void AudioMediaPlayer::loop() {
  
    SimpleAdfPipelineState pipeline_state = this->pipeline_.loop();
    if (pipeline_state != this->prior_pipeline_state_) {
      this->on_pipeline_state_change(pipeline_state);
      this->prior_pipeline_state_ = pipeline_state;
    }
    
    if (pipeline_state == SimpleAdfPipelineState::PAUSED
        && ((this->get_timestamp_sec_() - this->pause_timestamp_sec_) > this->pause_interval_sec)) {
      this->play_intent_ = false;
      this->stop_();
    }
    //if (this->multiRoomAudio_ != nullptr) {
    //  this->multiRoomAudio_->loop();
    //  this->mrm_process_recv_actions_();
    //  this->mrm_process_send_actions_();
    //}
}

void AudioMediaPlayer::control(const media_player::MediaPlayerCall &call) {
  esph_log_d(TAG, "control call while state: %s", media_player_state_to_string(this->state));

  if (this->multiRoomAudio_ != nullptr && this->multiRoomAudio_->get_group_members().length() > 0) {
    this->multiRoomAudio_->set_mrm(media_player::MEDIA_PLAYER_MRM_LEADER);
  }

  //Media File is sent (no command)
  if (call.get_media_url().has_value())
  {
    std::string media_url = call.get_media_url().value();
    //special cases for setting mrm commands
    if (media_url == "mrmlisten") {
      if (this->multiRoomAudio_ != nullptr) {
        this->init_mrm_();
      }
      this->multiRoomAudio_->set_mrm(media_player::MEDIA_PLAYER_MRM_FOLLOWER);
    }
    else if (this->multiRoomAudio_ != nullptr && media_url == "mrmunlisten") {
      this->multiRoomAudio_->set_mrm(media_player::MEDIA_PLAYER_MRM_FOLLOWER);
      this->multiRoomAudio_->unlisten();
      this->multiRoomAudio_->set_mrm(media_player::MEDIA_PLAYER_MRM_OFF);
      this->multiRoomAudio_ = nullptr;
    }
    else if (this->multiRoomAudio_ != nullptr && media_url.rfind("{\"mrmstart\"", 0) == 0) {
      this->multiRoomAudio_->set_mrm(media_player::MEDIA_PLAYER_MRM_FOLLOWER);
      cJSON *root = cJSON_Parse(media_url.c_str());
      std::string timestamp_str = cJSON_GetObjectItem(root,"timestamp")->valuestring;
      int64_t timestamp = strtoll(timestamp_str.c_str(), NULL, 10);
      cJSON_Delete(root);
      this->pipeline_start_(timestamp);
    }
    else if (this->multiRoomAudio_ != nullptr && media_url == "mrmstop") {
      this->multiRoomAudio_->set_mrm(media_player::MEDIA_PLAYER_MRM_FOLLOWER);
      this->pipeline_stop_();
    }
    else if (this->multiRoomAudio_ != nullptr && media_url == "mrmpause") {
      this->multiRoomAudio_->set_mrm(media_player::MEDIA_PLAYER_MRM_FOLLOWER);
      this->pipeline_pause_();
    }
    else if (this->multiRoomAudio_ != nullptr && media_url.rfind("{\"mrmresume\"", 0) == 0) {
      this->multiRoomAudio_->set_mrm(media_player::MEDIA_PLAYER_MRM_FOLLOWER);
      cJSON *root = cJSON_Parse(media_url.c_str());
      std::string timestamp_str = cJSON_GetObjectItem(root,"timestamp")->valuestring;
      int64_t timestamp = strtoll(timestamp_str.c_str(), NULL, 10);
      cJSON_Delete(root);
      this->pipeline_resume_(timestamp);
    }
    else if (this->multiRoomAudio_ != nullptr && media_url.rfind("{\"mrmurl\"", 0) == 0) {
      this->multiRoomAudio_->set_mrm(media_player::MEDIA_PLAYER_MRM_FOLLOWER);
      cJSON *root = cJSON_Parse(media_url.c_str());
      std::string mrmurl = cJSON_GetObjectItem(root,"mrmurl")->valuestring;
      cJSON_Delete(root);
      this->pipeline_.set_url(mrmurl);
    }
    else {
      //enqueue
      media_player::MediaPlayerEnqueue enqueue = media_player::MEDIA_PLAYER_ENQUEUE_PLAY;
      if (call.get_enqueue().has_value()) {
        enqueue = call.get_enqueue().value();
      }
      // announcing
      this->announcing_ = false;
      if (call.get_announcement().has_value()) {
        this->announcing_ = call.get_announcement().value();
      }
      if (this->announcing_) {
        this->play_track_id_ = this->audioPlaylists_.next_playlist_track_id();
        // place announcement in the announcements_ queue
        ADFUrlTrack track;
        track.url = call.get_media_url().value();
        this->audioPlaylists_.get_announcements()->push_back(track);
        //stop what is currently playing.
        //would need a separate pipeline sharing the i2s to not have to stop the track.
        pipeline_state_before_announcement_ = this->state;
        if (this->state == media_player::MEDIA_PLAYER_STATE_PLAYING || this->state == media_player::MEDIA_PLAYER_STATE_PAUSED) {
          this->play_intent_ = true;
          this->stop_();
          return;
        } 
        else if (this->state != media_player::MEDIA_PLAYER_STATE_ANNOUNCING) {
          this->start_();
        }
      }
      //normal media, use enqueue value to determine what to do
      else {
        if (enqueue == media_player::MEDIA_PLAYER_ENQUEUE_REPLACE || enqueue == media_player::MEDIA_PLAYER_ENQUEUE_PLAY) {
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
        float new_volume = this->volume + 0.05f;
        if (new_volume > 1.0f) {
          new_volume = 1.0f;
        }
        this->set_volume_(new_volume);
        break;
      }
      case media_player::MEDIA_PLAYER_COMMAND_VOLUME_DOWN: {
        float new_volume = this->volume - 0.05f;
        if (new_volume < 0.0f) {
          new_volume = 0.0f;
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
          if (this->multiRoomAudio_ != nullptr) {
            this->multiRoomAudio_->turn_on();
          }
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
            this->pipeline_.clean_up();
            this->state = media_player::MEDIA_PLAYER_STATE_OFF;
            this->publish_state();
            if (this->multiRoomAudio_ != nullptr) {
              this->multiRoomAudio_->turn_off();
            }
          }
        }
        break;
      case media_player::MEDIA_PLAYER_COMMAND_TURN_ON: {
        if (this->state == media_player::MEDIA_PLAYER_STATE_OFF) {
            this->state = media_player::MEDIA_PLAYER_STATE_ON;
            this->publish_state();
            if (this->multiRoomAudio_ != nullptr) {
              this->multiRoomAudio_->turn_on();
            }
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
            this->pipeline_.clean_up();
            this->state = media_player::MEDIA_PLAYER_STATE_OFF;
            this->publish_state();
            if (this->multiRoomAudio_ != nullptr) {
              this->multiRoomAudio_->turn_off();
            }
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
      case media_player::MEDIA_PLAYER_COMMAND_JOIN: {
        if (call.get_group_members().has_value()) {
          
          if (this->multiRoomAudio_ != nullptr) {
            this->init_mrm_();
          } 
          this->multiRoomAudio_->set_group_members(call.get_group_members().value());
          this->multiRoomAudio_->set_mrm(media_player::MEDIA_PLAYER_MRM_LEADER);
          this->multiRoomAudio_->listen();
        }
        break;
      }
      case media_player::MEDIA_PLAYER_COMMAND_UNJOIN: {
        this->multiRoomAudio_->set_mrm(media_player::MEDIA_PLAYER_MRM_LEADER);
        this->multiRoomAudio_->unlisten();
        this->multiRoomAudio_->get_group_members() = "";
        this->multiRoomAudio_->set_mrm(media_player::MEDIA_PLAYER_MRM_OFF);
        this->multiRoomAudio_ = nullptr;
        break;
      }
      default:
        break;
      }
    }
  }
}

void AudioMediaPlayer::on_pipeline_state_change(SimpleAdfPipelineState state) {
  esph_log_i(TAG, "got new pipeline state: %s", pipeline_state_to_string(state));
  switch (state) {
    case SimpleAdfPipelineState::STARTING:
     break;
    case SimpleAdfPipelineState::RESUMING:
     break;
    case SimpleAdfPipelineState::RUNNING:
      this->set_volume_( this->volume, false);
      if (is_announcement_()) {
        this->state = media_player::MEDIA_PLAYER_STATE_ANNOUNCING;
        timestamp_sec_ = 0;
      }
      else {
        this->state = media_player::MEDIA_PLAYER_STATE_PLAYING;
        timestamp_sec_ = get_timestamp_sec_();
      }
      this->publish_state();
      break;
    case SimpleAdfPipelineState::STOPPING:
      break;
    case SimpleAdfPipelineState::STOPPED:
      this->set_artist_("");
      this->set_album_("");
      this->set_title_("");
	  this->set_thumbnail_url_("");
      //this->set_duration_(0);
      //this->set_position_(0);
      this->state = media_player::MEDIA_PLAYER_STATE_IDLE;
      this->publish_state();
      if (this->multiRoomAudio_ != nullptr) {
        this->multiRoomAudio_->stop();
      }
      if (this->turning_off_) {
        if (HighFrequencyLoopRequester::is_high_frequency()) {
          esph_log_d(TAG,"Set Loop to run normal cycle");
          this->high_freq_.stop();
        }
        pipeline_.clean_up();
        this->state = media_player::MEDIA_PLAYER_STATE_OFF;
        this->publish_state();
        if (this->multiRoomAudio_ != nullptr) {
          this->multiRoomAudio_->turn_off();
        }
        this->turning_off_ = false;
      }
      else {
        if (this->play_intent_) {
          if (!this->play_next_track_on_announcements_()) {
            if (!this->announcing_ || this->pipeline_state_before_announcement_ == media_player::MEDIA_PLAYER_STATE_PLAYING) {
              this->play_next_track_on_playlist_(this->play_track_id_);
              this->play_track_id_ = -1;
            }
            else {
              this->play_intent_ = false;
            }
            this->announcing_ = false;
            this->pipeline_state_before_announcement_ = media_player::MEDIA_PLAYER_STATE_NONE;
          }
        }
        if (this->play_intent_) {
          this->start_();
        }
        else {
          if (HighFrequencyLoopRequester::is_high_frequency()) {
            esph_log_d(TAG,"Set Loop to run normal cycle");
            this->high_freq_.stop();
          }
          this->pipeline_.clean_up();
        }
      }
      break;
    case SimpleAdfPipelineState::PAUSING:
      break;
    case SimpleAdfPipelineState::PAUSED:
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
  
  int64_t timestamp = 0;
  if (this->multiRoomAudio_ != nullptr) {
    if (this->multiRoomAudio_->get_mrm() == media_player::MEDIA_PLAYER_MRM_LEADER) {
      timestamp = this->multiRoomAudio_->get_timestamp() + mrm_run_interval;
    }
    this->multiRoomAudio_->start(timestamp);
  }
  this->pipeline_start_(timestamp);
}

void AudioMediaPlayer::stop_() {
  esph_log_d(TAG,"stop_()");
  this->pipeline_stop_();
  if (this->multiRoomAudio_ != nullptr) {
    if (turning_off_) {
      this->multiRoomAudio_->turn_off();
    }
    else {
      this->multiRoomAudio_->stop();
    }
  }
}

void AudioMediaPlayer::pause_() {
  esph_log_d(TAG,"pause_()");
  this->pipeline_pause_();
  if (this->multiRoomAudio_ != nullptr) {
    this->multiRoomAudio_->pause();
  }
}

void AudioMediaPlayer::resume_()
{
  esph_log_d(TAG,"resume_()");
  
  int64_t timestamp = 0;
  if (this->multiRoomAudio_ != nullptr) {
    if (this->multiRoomAudio_->get_mrm() == media_player::MEDIA_PLAYER_MRM_LEADER) {
      timestamp = this->multiRoomAudio_->get_timestamp() + mrm_run_interval;
    }
    this->multiRoomAudio_->resume(timestamp);
  }
  this->pipeline_resume_(timestamp);
}

bool AudioMediaPlayer::is_announcement_() {
  return this->pipeline_.is_announcement();
}

void AudioMediaPlayer::set_volume_(float volume, bool publish) {
  pipeline_.set_volume(round(100 * volume));
  this->volume = volume;
  if (publish) {
    this->force_publish_ = true;
    this->publish_state();
    if (this->multiRoomAudio_ != nullptr) {
      this->multiRoomAudio_->volume(volume);
    }
  }
}

void AudioMediaPlayer::mute_() {
  this->pipeline_.mute();
  this->muted_ = true;
  this->force_publish_ = true;
  this->publish_state();
  if (this->multiRoomAudio_ != nullptr) {
    this->multiRoomAudio_->mute();
  }
}

void AudioMediaPlayer::unmute_() {
  this->pipeline_.unmute();
  this->muted_ = false;
  this->force_publish_ = true;
  this->publish_state();
  if (this->multiRoomAudio_ != nullptr) {
    this->multiRoomAudio_->unmute();
  }
}


void AudioMediaPlayer::pipeline_start_(int64_t launch_timestamp) {
  
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
    this->pipeline_.set_launch_timestamp(launch_timestamp);
    this->pipeline_.play();
  }
}

void AudioMediaPlayer::pipeline_stop_() {
  if (this->state == media_player::MEDIA_PLAYER_STATE_PLAYING || this->state == media_player::MEDIA_PLAYER_STATE_PAUSED) {
    esph_log_d(TAG,"pipeline_stop_()");
    this->pipeline_.stop();
  }
}

void AudioMediaPlayer::pipeline_pause_() {
  if (this->state == media_player::MEDIA_PLAYER_STATE_PLAYING) {
    esph_log_d(TAG,"pipeline_pause_()");
    this->pipeline_.pause();
  }
}

void AudioMediaPlayer::pipeline_resume_(int64_t launch_timestamp)
{
  if (this->state == media_player::MEDIA_PLAYER_STATE_PAUSED) {
    if (!HighFrequencyLoopRequester::is_high_frequency()) {
      esph_log_d(TAG,"Set Loop to run at high frequency cycle");
      this->high_freq_.start();
    }
    esph_log_d(TAG,"pipeline_resume_()");
    this->pipeline_.set_launch_timestamp(launch_timestamp);
    this->pipeline_.resume();
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
  this->pipeline_.set_url(track.url);
  if (this->multiRoomAudio_ != nullptr) {
    this->multiRoomAudio_->set_url(track.url);
  }
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

bool AudioMediaPlayer::play_next_track_on_announcements_() {
  bool retBool = false;
  unsigned int vid = this->audioPlaylists_.get_announcements()->size();
  if (vid > 0) {
    for(unsigned int i = 0; i < vid; i++) {
      bool ip = (*this->audioPlaylists_.get_announcements())[i].is_played;
      if (!ip) {
        this->pipeline_.set_url((*this->audioPlaylists_.get_announcements())[i].url, true);
        (*this->audioPlaylists_.get_announcements())[i].is_played = true;
        retBool = true;
        if (this->multiRoomAudio_ != nullptr) {
          this->multiRoomAudio_->set_url((*this->audioPlaylists_.get_announcements())[i].url);
        }
      }
    }
    if (!retBool) {
      this->audioPlaylists_.clean_announcements();
    }
  }
  return retBool;
}

int32_t AudioMediaPlayer::get_timestamp_sec_() {
  struct timeval tv_now;
  gettimeofday(&tv_now, NULL);
  return (int32_t)tv_now.tv_sec;
}

/*
void AudioMediaPlayer::mrm_process_send_actions_() {
  if (this->multiRoomAudio_ != nullptr
        && this->multiRoomAudio_->get_mrm() == media_player::MEDIA_PLAYER_MRM_LEADER
        && this->pipeline_.get_state() == SimpleAdfPipelineState::RUNNING
        && this->duration() > 0
        && ((this->multiRoomAudio_->get_timestamp() - this->mrm_position_timestamp_) > (this->mrm_position_interval_sec_ * 1000000L)))
  {
    audio_element_info_t info{};
    audio_element_getinfo(pipeline_.get_esp_decoder(), &info);
    this->mrm_position_timestamp_ = this->multiRoomAudio_->get_timestamp();
    int64_t position_byte = info.byte_pos;
    if (position_byte > 100 * 1024) {
      esph_log_v(TAG, "this->multiRoomAudio_ send position");
      this->multiRoomAudio_->send_position(this->mrm_position_timestamp_, position_byte);
    }
  }
}

void AudioMediaPlayer::mrm_process_recv_actions_() {  
  if (this->multiRoomAudio_ != nullptr && this->multiRoomAudio_->recv_actions.size() > 0) {
    std::string action = this->multiRoomAudio_->recv_actions.front().type;

    if (action == "sync_position" 
      && this->multiRoomAudio_->get_mrm() == media_player::MEDIA_PLAYER_MRM_FOLLOWER
      && ((this->multiRoomAudio_->get_timestamp() - this->mrm_position_timestamp_) > (this->mrm_position_interval_sec_ * 1000000L))
      )
    {
      int64_t timestamp = this->multiRoomAudio_->recv_actions.front().timestamp;
      std::string position_str = this->multiRoomAudio_->recv_actions.front().data;
      int64_t position = strtoll(position_str.c_str(), NULL, 10);
      this->mrm_sync_position_(timestamp, position);
    }
    this->multiRoomAudio_->recv_actions.pop();
  }
}

void AudioMediaPlayer::mrm_sync_position_(int64_t timestamp, int64_t position) {
  if (this->multiRoomAudio_ != nullptr) {
    if (this->multiRoomAudio_->get_mrm() == media_player::MEDIA_PLAYER_MRM_FOLLOWER
      && this->pipeline_.get_state() == SimpleAdfPipelineState::RUNNING
      && this->state == media_player::MEDIA_PLAYER_STATE_PLAYING
      )
    {
      audio_element_info_t info{};
      audio_element_getinfo(pipeline_.get_esp_decoder(), &info);
      this->mrm_position_timestamp_ = this->multiRoomAudio_->get_timestamp();
      int64_t local_position = info.byte_pos;
      
      if (local_position > 100 * 1024) {
        int32_t bps = (int32_t)(info.sample_rates * info.channels * info.bits / 8);
        float adj_sec = ((this->mrm_position_timestamp_ - timestamp) / 1000000.0);
        int64_t adjusted_position = round((adj_sec * bps) + position);
        int32_t delay_size = (int32_t)(adjusted_position - local_position);
        esph_log_d(TAG,"sync_position: leader: %lld, follower: %lld, diff: %d, adj_sec: %f", adjusted_position, local_position, delay_size, adj_sec);
        if (delay_size < -.1 * bps) {
          if (delay_size < -.2 * bps) {
            delay_size = -.2 * bps;
          }
          i2s_stream_sync_delay_(this->pipeline_.get_i2s_stream_writer(), delay_size);
           esph_log_d(TAG,"sync_position done, delay_size: %d", delay_size);
           this->mrm_position_interval_sec_ = 1;
        }
        else if (delay_size > .1 * bps) {
          if (delay_size > .2 * bps) {
            delay_size = .2 * bps;
          }
          i2s_stream_sync_delay_(this->pipeline_.get_i2s_stream_writer(), delay_size);
          esph_log_d(TAG,"sync_position done, delay_size: %d", delay_size);
          this->mrm_position_interval_sec_ = 1;
        }
        else {
          this->mrm_position_interval_sec_ = 30;
        }
      }
    }
  }
}

esp_err_t AudioMediaPlayer::i2s_stream_sync_delay_(audio_element_handle_t i2s_stream, int32_t delay_size)
{
    char *in_buffer = NULL;

    if (delay_size < 0) {
        int32_t abs_delay_size = abs(delay_size);
        in_buffer = (char *)audio_malloc(abs_delay_size);
        AUDIO_MEM_CHECK(TAG, in_buffer, return ESP_FAIL);
#if SOC_I2S_SUPPORTS_ADC_DAC
        i2s_stream_t *i2s = (i2s_stream_t *)audio_element_getdata(i2s_stream);
        if ((i2s->config.i2s_config.mode & I2S_MODE_DAC_BUILT_IN) != 0) {
            memset(in_buffer, 0x80, abs_delay_size);
        } else
#endif
        {
            memset(in_buffer, 0x00, abs_delay_size);
        }
        ringbuf_handle_t input_rb = audio_element_get_input_ringbuf(i2s_stream);
        if (input_rb) {
            rb_write(input_rb, in_buffer, abs_delay_size, 0);
        }
        audio_free(in_buffer);
    } else if (delay_size > 0) {
        uint32_t drop_size = delay_size;
        in_buffer = (char *)audio_malloc(drop_size);
        AUDIO_MEM_CHECK(TAG, in_buffer, return ESP_FAIL);
        uint32_t r_size = audio_element_input(i2s_stream, in_buffer, drop_size);
        audio_free(in_buffer);

        if (r_size > 0) {
            audio_element_update_byte_pos(i2s_stream, r_size);
        } else {
            ESP_LOGW(TAG, "Can't get enough data to drop.");
            return ESP_FAIL;
        }
    }

    return ESP_OK;
}
*/

}  // namespace esp_adf
}  // namespace esphome

#endif  // USE_ESP_IDF