#include "audio_media_player.h"

#ifdef USE_ESP_IDF

#include <esp_http_client.h>
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

  state = media_player::MEDIA_PLAYER_STATE_OFF;
  pipeline_.set_parent(this->parent_);
  volume = .25;
}

void AudioMediaPlayer::publish_state() {

  esph_log_d(TAG, "MP State = %s, MP Prior State = %s", media_player_state_to_string(state), media_player_state_to_string(prior_state));
  if (state != prior_state || force_publish_) {
    switch (state) {
      case media_player::MEDIA_PLAYER_STATE_PLAYING:
        if (duration_ > 0 && timestamp_sec_ > 0) {
          set_position_(offset_sec_ + (get_timestamp_sec_() - timestamp_sec_));
        }
        else {
          set_position_(0);
        }
        break;
      case media_player::MEDIA_PLAYER_STATE_PAUSED:
        if (duration_ > 0 && timestamp_sec_ > 0) {
          set_position_(offset_sec_);
        }
        else {
          set_position_(0);
        }
        break;
      default:
        //set_duration_(0);
        set_position_(0);
        break;
    }
    esph_log_d(TAG, "Publish State, position: %d, duration: %d",position(),duration());
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
  
    SimpleAdfPipelineState pipeline_state = pipeline_.loop();
    if (pipeline_state != prior_pipeline_state_) {
      on_pipeline_state_change(pipeline_state);
      prior_pipeline_state_ = pipeline_state;
    }
    
    if (pipeline_state == SimpleAdfPipelineState::PAUSED
        && ((get_timestamp_sec_() - pause_timestamp_sec_) > pause_interval_sec)) {
      this->play_intent_ = false;
      stop_();
    }
    
    //multiRoomAudio_.loop();
    //mrm_process_recv_actions_();
    //mrm_process_send_actions_();
}

void AudioMediaPlayer::control(const media_player::MediaPlayerCall &call) {
  esph_log_d(TAG, "control call while state: %s", media_player_state_to_string(state));

  if (multiRoomAudio_.get_group_members().length() > 0) {
    multiRoomAudio_.set_mrm(media_player::MEDIA_PLAYER_MRM_LEADER);
  }

  //Media File is sent (no command)
  if (call.get_media_url().has_value())
  {
    std::string media_url = call.get_media_url().value();
    //special cases for setting mrm commands
    if (media_url == "mrmlisten") {
      multiRoomAudio_.set_mrm(media_player::MEDIA_PLAYER_MRM_FOLLOWER);
    }
    else if (media_url == "mrmunlisten") {
      multiRoomAudio_.set_mrm(media_player::MEDIA_PLAYER_MRM_FOLLOWER);
      multiRoomAudio_.unlisten();
      multiRoomAudio_.set_mrm(media_player::MEDIA_PLAYER_MRM_OFF);
    }
    else if (media_url.rfind("{\"mrmstart\"", 0) == 0) {
      multiRoomAudio_.set_mrm(media_player::MEDIA_PLAYER_MRM_FOLLOWER);
      cJSON *root = cJSON_Parse(media_url.c_str());
      std::string timestamp_str = cJSON_GetObjectItem(root,"timestamp")->valuestring;
      int64_t timestamp = strtoll(timestamp_str.c_str(), NULL, 10);
      cJSON_Delete(root);
      pipeline_start_(timestamp);
    }
    else if (media_url == "mrmstop") {
      multiRoomAudio_.set_mrm(media_player::MEDIA_PLAYER_MRM_FOLLOWER);
      pipeline_stop_();
    }
    else if (media_url == "mrmpause") {
      multiRoomAudio_.set_mrm(media_player::MEDIA_PLAYER_MRM_FOLLOWER);
      pipeline_pause_();
    }
    else if (media_url.rfind("{\"mrmresume\"", 0) == 0) {
      multiRoomAudio_.set_mrm(media_player::MEDIA_PLAYER_MRM_FOLLOWER);
      cJSON *root = cJSON_Parse(media_url.c_str());
      std::string timestamp_str = cJSON_GetObjectItem(root,"timestamp")->valuestring;
      int64_t timestamp = strtoll(timestamp_str.c_str(), NULL, 10);
      cJSON_Delete(root);
      pipeline_resume_(timestamp);
    }
    else if (media_url.rfind("{\"mrmurl\"", 0) == 0) {
      multiRoomAudio_.set_mrm(media_player::MEDIA_PLAYER_MRM_FOLLOWER);
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
      announcing_ = false;
      if (call.get_announcement().has_value()) {
        announcing_ = call.get_announcement().value();
      }
      if (announcing_) {
        this->play_track_id_ = audioPlaylists_.next_playlist_track_id();
        // place announcement in the announcements_ queue
        ADFUrlTrack track;
        track.url = call.get_media_url().value();
        audioPlaylists_.get_announcements()->push_back(track);
        //stop what is currently playing.
        //would need a separate pipeline sharing the i2s to not have to stop the track.
        pipeline_state_before_announcement_ = state;
        if (state == media_player::MEDIA_PLAYER_STATE_PLAYING || state == media_player::MEDIA_PLAYER_STATE_PAUSED) {
          this->play_intent_ = true;
          stop_();
          return;
        } 
        else if (state != media_player::MEDIA_PLAYER_STATE_ANNOUNCING) {
          start_();
        }
      }
      //normal media, use enqueue value to determine what to do
      else {
        if (enqueue == media_player::MEDIA_PLAYER_ENQUEUE_REPLACE || enqueue == media_player::MEDIA_PLAYER_ENQUEUE_PLAY) {
          this->play_track_id_ = -1;
          if (enqueue == media_player::MEDIA_PLAYER_ENQUEUE_REPLACE) {
            audioPlaylists_.clean_playlist();
          }
          audioPlaylists_.playlist_add(call.get_media_url().value(), true,shuffle_);
          set_playlist_track_(audioPlaylists_.get_playlist()->front());
          this->play_intent_ = true;
          if (state == media_player::MEDIA_PLAYER_STATE_PLAYING || state == media_player::MEDIA_PLAYER_STATE_PAUSED) {
            this->play_track_id_ = 0;
            stop_();
            return;
          } else {
            start_();
          }
        }
        else if (enqueue == media_player::MEDIA_PLAYER_ENQUEUE_ADD) {
          audioPlaylists_.playlist_add(call.get_media_url().value(), true, shuffle_);
        }
        else if (enqueue == media_player::MEDIA_PLAYER_ENQUEUE_NEXT) {
          audioPlaylists_.playlist_add(call.get_media_url().value(), false,shuffle_);
        }
      }
    } 
  }
  // Volume value is sent (no command)
  if (call.get_volume().has_value()) {
    set_volume_(call.get_volume().value());
  }
  //Command
  if (call.get_command().has_value()) {
    switch (call.get_command().value()) {
      case media_player::MEDIA_PLAYER_COMMAND_PLAY:
        this->play_intent_ = true;
        this->play_track_id_ = -1;
        if (state == media_player::MEDIA_PLAYER_STATE_PLAYING) {
          stop_();
        }
        if (state == media_player::MEDIA_PLAYER_STATE_PAUSED) {
          resume_();
        }
        if (state == media_player::MEDIA_PLAYER_STATE_OFF 
        || state == media_player::MEDIA_PLAYER_STATE_ON 
        || state == media_player::MEDIA_PLAYER_STATE_NONE
        || state == media_player::MEDIA_PLAYER_STATE_IDLE) {
        
          int id = audioPlaylists_.next_playlist_track_id();
          if (id > -1) {
            set_playlist_track_((*audioPlaylists_.get_playlist())[id]);
          }
          start_();
        }
        break;
      case media_player::MEDIA_PLAYER_COMMAND_PAUSE:
        if (state == media_player::MEDIA_PLAYER_STATE_PLAYING) {
          this->play_track_id_ = audioPlaylists_.next_playlist_track_id();
          play_intent_ = false;
        }
        pause_();
        break;
      case media_player::MEDIA_PLAYER_COMMAND_STOP:
        play_intent_ = false;
        audioPlaylists_.clean_playlist();
        pause_();
        break;
      case media_player::MEDIA_PLAYER_COMMAND_MUTE:
        this->mute_();
        break;
      case media_player::MEDIA_PLAYER_COMMAND_UNMUTE:
        this->unmute_();
        break;
      case media_player::MEDIA_PLAYER_COMMAND_VOLUME_UP: {
        float new_volume = this->volume + 0.05f;
        if (new_volume > 1.0f)
          new_volume = 1.0f;
        set_volume_(new_volume);
        break;
      }
      case media_player::MEDIA_PLAYER_COMMAND_VOLUME_DOWN: {
        float new_volume = this->volume - 0.05f;
        if (new_volume < 0.0f)
          new_volume = 0.0f;
        set_volume_(new_volume);
        break;
      }
      case media_player::MEDIA_PLAYER_COMMAND_NEXT_TRACK: {
        if ( this->audioPlaylists_.get_playlist()->size() > 0 ) {
          this->play_intent_ = true;
          this->play_track_id_ = -1;
          stop_();
        }
        break;
      }
      case media_player::MEDIA_PLAYER_COMMAND_PREVIOUS_TRACK: {
        if ( this->audioPlaylists_.get_playlist()->size() > 0 ) {
          this->play_intent_ = true;
          this->play_track_id_ = audioPlaylists_.previous_playlist_track_id();
          stop_();
        }
        break;
      }
      case media_player::MEDIA_PLAYER_COMMAND_TOGGLE: {
        if (state == media_player::MEDIA_PLAYER_STATE_OFF) {
          state = media_player::MEDIA_PLAYER_STATE_ON;
          publish_state();
          multiRoomAudio_.turn_on();
        }
        else {
          if (state == media_player::MEDIA_PLAYER_STATE_PLAYING 
          || state == media_player::MEDIA_PLAYER_STATE_PAUSED 
          || state == media_player::MEDIA_PLAYER_STATE_ANNOUNCING ) {
            turning_off_ = true;
            this->play_intent_ = false;
            stop_();
          }
          else {
            if (HighFrequencyLoopRequester::is_high_frequency()) {
              esph_log_d(TAG,"Set Loop to run normal cycle");
              this->high_freq_.stop();
            }
            pipeline_.clean_up();
            state = media_player::MEDIA_PLAYER_STATE_OFF;
            publish_state();
            multiRoomAudio_.turn_on();
          }
        }
        break;
      case media_player::MEDIA_PLAYER_COMMAND_TURN_ON: {
        if (state == media_player::MEDIA_PLAYER_STATE_OFF) {
            state = media_player::MEDIA_PLAYER_STATE_ON;
            publish_state();
            multiRoomAudio_.turn_on();
        }
        break;
      }
      case media_player::MEDIA_PLAYER_COMMAND_TURN_OFF: {
        if (state != media_player::MEDIA_PLAYER_STATE_OFF) {
          if (state == media_player::MEDIA_PLAYER_STATE_PLAYING 
          || state == media_player::MEDIA_PLAYER_STATE_PAUSED 
          || state == media_player::MEDIA_PLAYER_STATE_ANNOUNCING ) {
            turning_off_ = true;
            this->play_intent_ = false;
            stop_();
          }
          else {
            if (HighFrequencyLoopRequester::is_high_frequency()) {
              esph_log_d(TAG,"Set Loop to run normal cycle");
              this->high_freq_.stop();
            }
            pipeline_.clean_up();
            state = media_player::MEDIA_PLAYER_STATE_OFF;
            publish_state();
            multiRoomAudio_.turn_off();
          }
        }
        break;
      }
      case media_player::MEDIA_PLAYER_COMMAND_CLEAR_PLAYLIST: {
        audioPlaylists_.clean_playlist();
        break;
      }
      case media_player::MEDIA_PLAYER_COMMAND_SHUFFLE: {
        set_shuffle_(true);
        break;
      }
      case media_player::MEDIA_PLAYER_COMMAND_UNSHUFFLE: {
        set_shuffle_(false);
        break;
      }
      case media_player::MEDIA_PLAYER_COMMAND_REPEAT_OFF: {
        set_repeat_(media_player::MEDIA_PLAYER_REPEAT_OFF);
        break;
      }
      case media_player::MEDIA_PLAYER_COMMAND_REPEAT_ONE: {
        set_repeat_(media_player::MEDIA_PLAYER_REPEAT_ONE);
        break;
      }
      case media_player::MEDIA_PLAYER_COMMAND_REPEAT_ALL: {
        set_repeat_(media_player::MEDIA_PLAYER_REPEAT_ALL);
        break;
      }
      case media_player::MEDIA_PLAYER_COMMAND_JOIN: {
        if (call.get_group_members().has_value()) {
          multiRoomAudio_.set_group_members(call.get_group_members().value());
          multiRoomAudio_.set_mrm(media_player::MEDIA_PLAYER_MRM_LEADER);
          multiRoomAudio_.listen();
        }
        break;
      }
      case media_player::MEDIA_PLAYER_COMMAND_UNJOIN: {
        multiRoomAudio_.set_mrm(media_player::MEDIA_PLAYER_MRM_LEADER);
        multiRoomAudio_.unlisten();
        multiRoomAudio_.get_group_members() = "";
        multiRoomAudio_.set_mrm(media_player::MEDIA_PLAYER_MRM_OFF);
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
      publish_state();
      break;
    case SimpleAdfPipelineState::STOPPING:
      break;
    case SimpleAdfPipelineState::STOPPED:
      set_artist_("");
      set_album_("");
      set_title_("");
      //set_duration_(0);
      //set_position_(0);
      this->state = media_player::MEDIA_PLAYER_STATE_IDLE;
      publish_state();
      multiRoomAudio_.stop();

      if (this->turning_off_) {
        if (HighFrequencyLoopRequester::is_high_frequency()) {
          esph_log_d(TAG,"Set Loop to run normal cycle");
          this->high_freq_.stop();
        }
        pipeline_.clean_up();
        this->state = media_player::MEDIA_PLAYER_STATE_OFF;
        publish_state();
        multiRoomAudio_.turn_off();
        turning_off_ = false;
      }
      else {
        if (this->play_intent_) {
          if (!play_next_track_on_announcements_()) {
            if (!announcing_ || pipeline_state_before_announcement_ == media_player::MEDIA_PLAYER_STATE_PLAYING) {
              play_next_track_on_playlist_(this->play_track_id_);
              this->play_track_id_ = -1;
            }
            else {
              play_intent_ = false;
            }
            announcing_ = false;
            pipeline_state_before_announcement_ = media_player::MEDIA_PLAYER_STATE_NONE;
          }
        }
        if (this->play_intent_) {
          start_();
        }
        else {
          if (HighFrequencyLoopRequester::is_high_frequency()) {
            esph_log_d(TAG,"Set Loop to run normal cycle");
            this->high_freq_.stop();
          }
          pipeline_.clean_up();
        }
      }
      break;
    case SimpleAdfPipelineState::PAUSING:
      break;
    case SimpleAdfPipelineState::PAUSED:
      offset_sec_ = offset_sec_ + (get_timestamp_sec_() - timestamp_sec_);
      pause_timestamp_sec_ = get_timestamp_sec_();
      this->state = media_player::MEDIA_PLAYER_STATE_PAUSED;
      publish_state();
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
  if (multiRoomAudio_.get_mrm() == media_player::MEDIA_PLAYER_MRM_LEADER) {
    timestamp = multiRoomAudio_.get_timestamp() + mrm_run_interval;
  }
  multiRoomAudio_.start(timestamp);
  pipeline_start_(timestamp);
}

void AudioMediaPlayer::stop_() {
  esph_log_d(TAG,"stop_()");
  pipeline_stop_();
  if (turning_off_) {
    multiRoomAudio_.turn_off();
  }
  else {
    multiRoomAudio_.stop();
  }
}

void AudioMediaPlayer::pause_() {
  esph_log_d(TAG,"pause_()");
  pipeline_pause_();
  multiRoomAudio_.pause();
}

void AudioMediaPlayer::resume_()
{
  esph_log_d(TAG,"resume_()");
  
  int64_t timestamp = 0;
  if (multiRoomAudio_.get_mrm() == media_player::MEDIA_PLAYER_MRM_LEADER) {
    timestamp = multiRoomAudio_.get_timestamp() + mrm_run_interval;
  }
  multiRoomAudio_.resume(timestamp);
  pipeline_resume_(timestamp);
}

bool AudioMediaPlayer::is_announcement_() {
  return this->pipeline_.is_announcement();
}

void AudioMediaPlayer::set_volume_(float volume, bool publish) {
  pipeline_.set_volume(round(100 * volume));
  this->volume = volume;
  if (publish) {
    force_publish_ = true;
    publish_state();
    multiRoomAudio_.volume(volume);
  }
}

void AudioMediaPlayer::mute_() {
  pipeline_.mute();
  muted_ = true;
  force_publish_ = true;
  publish_state();
  multiRoomAudio_.mute();
}

void AudioMediaPlayer::unmute_() {
  pipeline_.unmute();
  muted_ = false;
  force_publish_ = true;
  publish_state();
  multiRoomAudio_.unmute();
}


void AudioMediaPlayer::pipeline_start_(int64_t launch_timestamp) {
  
  if (state == media_player::MEDIA_PLAYER_STATE_OFF 
  || state == media_player::MEDIA_PLAYER_STATE_ON 
  || state == media_player::MEDIA_PLAYER_STATE_NONE
  || state == media_player::MEDIA_PLAYER_STATE_IDLE) {
    esph_log_d(TAG,"pipeline_start_()");
    if (state == media_player::MEDIA_PLAYER_STATE_OFF) {
      state = media_player::MEDIA_PLAYER_STATE_ON;
      publish_state();
    }
    if (!HighFrequencyLoopRequester::is_high_frequency()) {
      esph_log_d(TAG,"Set Loop to run at high frequency cycle");
      this->high_freq_.start();
    }
    pipeline_.set_launch_timestamp(launch_timestamp);
    pipeline_.play();
  }
}

void AudioMediaPlayer::pipeline_stop_() {
  if (state == media_player::MEDIA_PLAYER_STATE_PLAYING || state == media_player::MEDIA_PLAYER_STATE_PAUSED) {
    esph_log_d(TAG,"pipeline_stop_()");
    pipeline_.stop();
  }
}

void AudioMediaPlayer::pipeline_pause_() {
  if (state == media_player::MEDIA_PLAYER_STATE_PLAYING) {
    esph_log_d(TAG,"pipeline_pause_()");
    pipeline_.pause();
  }
}

void AudioMediaPlayer::pipeline_resume_(int64_t launch_timestamp)
{
  if (state == media_player::MEDIA_PLAYER_STATE_PAUSED) {
    if (!HighFrequencyLoopRequester::is_high_frequency()) {
      esph_log_d(TAG,"Set Loop to run at high frequency cycle");
      this->high_freq_.start();
    }
    esph_log_d(TAG,"pipeline_resume_()");
    pipeline_.set_launch_timestamp(launch_timestamp);
    pipeline_.resume();
  }
}


void AudioMediaPlayer::set_repeat_(media_player::MediaPlayerRepeatMode repeat) {
  this->repeat_ = repeat;
  force_publish_ = true;
  publish_state();
}

void AudioMediaPlayer::set_shuffle_(bool shuffle) {
  unsigned int vid = this->audioPlaylists_.get_playlist()->size();
  if (vid > 0) {
    audioPlaylists_.shuffle_playlist(shuffle);
    this->shuffle_ = shuffle;
    force_publish_ = true;
    publish_state();
    this->play_intent_ = true;
    this->play_track_id_ = 0;
    stop_();
  }
}

void AudioMediaPlayer::set_playlist_track_(ADFPlaylistTrack track) {
  esph_log_v(TAG, "uri: %s", track.url);
  set_artist_(track.artist);
  set_album_(track.album);
  if (track.title == "") {
    set_title_(track.url);
  }
  else {
    set_title_(track.title);
  }
  set_duration_(track.duration);
  offset_sec_ = 0;
  set_position_(0);

  esph_log_d(TAG, "set_playlist_track: %s: %s: %s duration: %d %s",
     artist_.c_str(), album_.c_str(), title_.c_str(), duration_, track.url.c_str());
  pipeline_.set_url(track.url);
  multiRoomAudio_.set_url(track.url);
}

void AudioMediaPlayer::play_next_track_on_playlist_(int track_id) {

  unsigned int vid = this->audioPlaylists_.get_playlist()->size();
  if (audioPlaylists_.get_playlist()->size() > 0) {
    if (repeat_ != media_player::MEDIA_PLAYER_REPEAT_ONE) {
      audioPlaylists_.set_playlist_track_as_played(track_id);
    }
       int id = audioPlaylists_.next_playlist_track_id();
    if (id > -1) {
      set_playlist_track_((*audioPlaylists_.get_playlist())[id]);
    }
    else {
      if (repeat_ == media_player::MEDIA_PLAYER_REPEAT_ALL) {
        for(unsigned int i = 0; i < vid; i++)
        {
          (*this->audioPlaylists_.get_playlist())[i].is_played = false;
        }
        set_playlist_track_((*audioPlaylists_.get_playlist())[0]);
      }
      else {
        audioPlaylists_.clean_playlist();
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
        pipeline_.set_url((*audioPlaylists_.get_announcements())[i].url, true);
        (*audioPlaylists_.get_announcements())[i].is_played = true;
        retBool = true;
        multiRoomAudio_.set_url((*audioPlaylists_.get_announcements())[i].url);
      }
    }
    if (!retBool) {
      audioPlaylists_.clean_announcements();
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
  if (multiRoomAudio_.get_mrm() == media_player::MEDIA_PLAYER_MRM_LEADER
        && pipeline_.get_state() == SimpleAdfPipelineState::RUNNING
        && duration() > 0
        && ((multiRoomAudio_.get_timestamp() - mrm_position_timestamp_) > (mrm_position_interval_sec_ * 1000000L)))
  {
    audio_element_info_t info{};
    audio_element_getinfo(pipeline_.get_esp_decoder(), &info);
    mrm_position_timestamp_ = multiRoomAudio_.get_timestamp();
    int64_t position_byte = info.byte_pos;
    if (position_byte > 100 * 1024) {
      esph_log_v(TAG, "multiRoomAudio_ send position");
      this->multiRoomAudio_.send_position(mrm_position_timestamp_, position_byte);
    }
  }
}

void AudioMediaPlayer::mrm_process_recv_actions_() {  
  if (this->multiRoomAudio_.recv_actions.size() > 0) {
    std::string action = this->multiRoomAudio_.recv_actions.front().type;

    if (action == "sync_position" 
      && multiRoomAudio_.get_mrm() == media_player::MEDIA_PLAYER_MRM_FOLLOWER
      && ((multiRoomAudio_.get_timestamp() - mrm_position_timestamp_) > (mrm_position_interval_sec_ * 1000000L))
      )
    {
      int64_t timestamp = this->multiRoomAudio_.recv_actions.front().timestamp;
      std::string position_str = this->multiRoomAudio_.recv_actions.front().data;
      int64_t position = strtoll(position_str.c_str(), NULL, 10);
      this->mrm_sync_position_(timestamp, position);
    }
    this->multiRoomAudio_.recv_actions.pop();
  }
}

void AudioMediaPlayer::mrm_sync_position_(int64_t timestamp, int64_t position) {
  if (multiRoomAudio_.get_mrm() == media_player::MEDIA_PLAYER_MRM_FOLLOWER
    && pipeline_.get_state() == SimpleAdfPipelineState::RUNNING
    && state == media_player::MEDIA_PLAYER_STATE_PLAYING
    )
  {
    audio_element_info_t info{};
    audio_element_getinfo(pipeline_.get_esp_decoder(), &info);
    mrm_position_timestamp_ = multiRoomAudio_.get_timestamp();
    int64_t local_position = info.byte_pos;
    
    if (local_position > 100 * 1024) {
      int32_t bps = (int32_t)(info.sample_rates * info.channels * info.bits / 8);
      float adj_sec = ((mrm_position_timestamp_ - timestamp) / 1000000.0);
      int64_t adjusted_position = round((adj_sec * bps) + position);
      int32_t delay_size = (int32_t)(adjusted_position - local_position);
      esph_log_d(TAG,"sync_position: leader: %lld, follower: %lld, diff: %d, adj_sec: %f", adjusted_position, local_position, delay_size, adj_sec);
      if (delay_size < -.1 * bps) {
        if (delay_size < -.2 * bps) {
          delay_size = -.2 * bps;
        }
        i2s_stream_sync_delay_(pipeline_.get_i2s_stream_writer(), delay_size);
         esph_log_d(TAG,"sync_position done, delay_size: %d", delay_size);
         mrm_position_interval_sec_ = 1;
      }
      else if (delay_size > .1 * bps) {
        if (delay_size > .2 * bps) {
          delay_size = .2 * bps;
        }
        i2s_stream_sync_delay_(pipeline_.get_i2s_stream_writer(), delay_size);
        esph_log_d(TAG,"sync_position done, delay_size: %d", delay_size);
        mrm_position_interval_sec_ = 1;
      }
      else {
        mrm_position_interval_sec_ = 30;
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