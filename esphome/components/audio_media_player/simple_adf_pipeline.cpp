#include "simple_adf_pipeline.h"

#ifdef USE_ADF_SIMPLE_PIPELINE

#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace esp_audio {

static const char *const TAG = "simple_adf_media_pipeline";

void SimpleADFPipeline::dump_config() {
  AudioMediaPipeline::dump_config();
}

void SimpleADFPipeline::play(bool resume) {
  esph_log_d(TAG, "Called play with pipeline state: %s, '%s'",pipeline_state_to_string(this->state_), this->url_.c_str());
  if (this->url_.length() > 0 
    && ((!resume && this->state_ == AudioMediaPipelineState::STOPPED)
      || (resume && this->state_ == AudioMediaPipelineState::PAUSED)
      || !resume && this->state_ == AudioMediaPipelineState::STOPPED_ANNOUNCING)) {
    if (resume) {
      this->set_state_(AudioMediaPipelineState::RESUMING);
    }
    else {
      this->set_state_(AudioMediaPipelineState::STARTING);
    }

    if (!resume && this->input_type_ == AudioMediaInputType::FLASH) {
      this->pipeline_stop_(false, false);
      this->switch_pipeline_input_(AudioMediaInputType::URL);
    }

    if (!this->is_initialized_) {
      this->pipeline_init_();
    }

    if (!resume) {
      audio_element_set_uri(this->http_stream_reader_, this->get_transcode_url_(this->url_).c_str());
      esph_log_d(TAG, "Play: %s", this->url_.c_str());
      this->is_music_info_set_ = false;
    }
    this->pipeline_play_(resume);
  }
}

void SimpleADFPipeline::play_announcement_(const std::string& url) {
  esph_log_d(TAG, "Called play_announcement with pipeline state: %s",pipeline_state_to_string(this->state_));
  this->prior_state_ = this->state_;
  if (this->state_ != AudioMediaPipelineState::PAUSED) {
    this->pipeline_stop_(false, false);
  }
  this->set_state_(AudioMediaPipelineState::STARTING_ANNOUNCING);
  if (this->input_type_ == AudioMediaInputType::FLASH) {
    this->pipeline_stop_(false, false);
    this->switch_pipeline_input_(AudioMediaInputType::URL);
  }
  if (!this->is_initialized_) {
    this->pipeline_init_();
  }
  audio_element_set_uri(this->http_stream_reader_, this->get_transcode_url_(url).c_str());
  esph_log_d(TAG, "Play: %s", url.c_str());
  this->is_music_info_set_ = false;
  this->pipeline_play_(false);
}

void SimpleADFPipeline::play_announcement_(audio::AudioFile *media_file) {
  esph_log_d(TAG, "play_file while input_type_ %s", audio_media_input_type_to_string(this->input_type_));
  this->prior_state_ = this->state_;
  if (this->state_ == AudioMediaPipelineState::PAUSED) {
    this->pipeline_stop_(false, false);
  }

  embed_item_info_t embed_item_info[] = {
    [0] = {
        .address = media_file->data,
        .size    = (int)media_file->length,
        },
  };
  std::string url = "embed://tone/0_dummy";
  std::string file_type = audio::audio_file_type_to_string(media_file->file_type);
  url = url + file_type;

  if (this->input_type_ == AudioMediaInputType::URL) {
    this->pipeline_stop_(false, false);
    this->switch_pipeline_input_(AudioMediaInputType::FLASH);
  }
  if (!this->is_initialized_) {
    this->pipeline_init_();
  }

  this->set_state_(AudioMediaPipelineState::STARTING_ANNOUNCING);
  embed_flash_stream_set_context(this->flash_stream_reader_, (embed_item_info_t *)&embed_item_info[0], 1);
  audio_element_set_uri(this->flash_stream_reader_, url.c_str());
  this->is_music_info_set_ = false;
  this->pipeline_play_(false);

}

void SimpleADFPipeline::stop(bool pause) {
  esph_log_d(TAG, "stop");

  if (pause) {
    esph_log_d(TAG, "Called pause with pipeline state: %s",pipeline_state_to_string(this->state_));
  }
  else {
    esph_log_d(TAG, "Called stop with pipeline state: %s",pipeline_state_to_string(this->state_));
  }
  bool cleanup = false;
  if (!pause && this->state_ == AudioMediaPipelineState::PAUSED) {
    cleanup = true;
  }

  if (this->state_ == AudioMediaPipelineState::STARTING
  || this->state_ == AudioMediaPipelineState::RUNNING
  || this->state_ == AudioMediaPipelineState::STOPPING
  || this->state_ == AudioMediaPipelineState::PAUSING
  || this->state_ == AudioMediaPipelineState::PAUSED
  || this->state_ == AudioMediaPipelineState::STARTING_ANNOUNCING
  || this->state_ == AudioMediaPipelineState::ANNOUNCING
  || this->state_ == AudioMediaPipelineState::STOPPING_ANNOUNCING) {
    if (pause) {
      this->set_state_(AudioMediaPipelineState::PAUSING);
    }
    else if (this->is_announcement_) {
      this->set_state_(AudioMediaPipelineState::STOPPING_ANNOUNCING);
    }
    else {
      this->set_state_(AudioMediaPipelineState::STOPPING);
    }
    this->pipeline_stop_(pause, cleanup);
  }
  if (pause) {
    this->set_state_(AudioMediaPipelineState::PAUSED);
  }
  else if (this->is_announcement_) {
    this->set_state_(AudioMediaPipelineState::STOPPED_ANNOUNCING);
  }
  else {
    this->set_state_(AudioMediaPipelineState::STOPPED);
  }
}

AudioMediaPipelineState SimpleADFPipeline::loop() {

  if (!this->announcements_.empty()
    && this->state_ != AudioMediaPipelineState::STARTING_ANNOUNCING 
    && this->state_ != AudioMediaPipelineState::ANNOUNCING 
    && this->state_ != AudioMediaPipelineState::STOPPING_ANNOUNCING) {
    this->pipeline_stop_(false, false);
    AudioAnouncement announcement = this->announcements_.front();
    if (announcement.url.has_value()) {
      this->play_announcement_(announcement.url.value());
    }
    else if (announcement.file.has_value()) {
      this->play_announcement_(announcement.file.value());
    }
    this->announcements_.pop_front();
  }
  else if (this->announcements_.empty()
    && this->url_.length() > 0
    && this->state_ == AudioMediaPipelineState::STOPPED_ANNOUNCING
    && this->prior_state_ == AudioMediaPipelineState::RUNNING) {
    play();
  }
  else if (this->state_ == AudioMediaPipelineState::STOPPED_ANNOUNCING) {
    this->set_state_(AudioMediaPipelineState::STOPPED);
  }
  else if (this->is_launched_) {
    audio_event_iface_msg_t msg;
    esp_err_t ret = audio_event_iface_listen(this->evt_, &msg, 0);
    if (ret == ESP_OK) {
        // Set Music Info
        if (this->is_music_info_set_ == false
        && msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT
        && msg.source == (void *) this->esp_decoder_
        && msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO) {
          audio_element_info_t music_info = {0};
          audio_element_getinfo(this->esp_decoder_, &music_info);

          esph_log_d(TAG, "[ decoder ] Receive music info, sample_rates=%d, bits=%d, ch=%d",
            music_info.sample_rates, music_info.bits, music_info.channels);
          i2s_stream_set_clk(this->i2s_stream_writer_, music_info.sample_rates, music_info.bits, music_info.channels);
          this->is_music_info_set_ = true;
        }

      if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.cmd == AEL_MSG_CMD_REPORT_STATUS) {
        // Review Status Reports
        audio_element_status_t status;
        std::memcpy(&status, &msg.data, sizeof(audio_element_status_t));
        audio_element_handle_t el = (audio_element_handle_t) msg.source;
        esph_log_d(TAG, "[ %s ] status: %s, pipeline state: %s", audio_element_get_tag(el), audio_element_status_to_string(status), pipeline_state_to_string(this->state_));
        int message_status = (int)msg.data;

        // Starting or Resuming
        if (this->state_ == AudioMediaPipelineState::STARTING
          || this->state_ == AudioMediaPipelineState::RESUMING
          || this->state_ == AudioMediaPipelineState::STARTING_ANNOUNCING) {
          //Error, stop and cleanup
          if (message_status == AEL_STATUS_ERROR_OPEN
            || message_status == AEL_STATUS_ERROR_INPUT
            || message_status == AEL_STATUS_ERROR_PROCESS
            || message_status == AEL_STATUS_ERROR_OUTPUT
            || message_status == AEL_STATUS_ERROR_CLOSE
            || message_status == AEL_STATUS_ERROR_TIMEOUT
            || message_status == AEL_STATUS_ERROR_UNKNOWN) {
            esph_log_e(TAG, "Error received, stopping");
            this->stop();
            this->clean_up();
          }
          // Http or Flash is Running
          else if (((msg.source == (void *) this->http_stream_reader_ && this->input_type_ == AudioMediaInputType::URL)
                 || (msg.source == (void *) this->flash_stream_reader_ && this->input_type_ == AudioMediaInputType::FLASH))
            && message_status == AEL_STATUS_STATE_RUNNING) {
            esph_log_d(TAG, "Running event received");
            if (this->state_ == AudioMediaPipelineState::STARTING_ANNOUNCING) {
              this->set_state_(AudioMediaPipelineState::ANNOUNCING);
            }
            else {
              this->set_state_(AudioMediaPipelineState::RUNNING);
            }
          }
        }

        // Http or Flash is Finished
        if (this->state_ == AudioMediaPipelineState::RUNNING
          || this->state_ == AudioMediaPipelineState::ANNOUNCING) {
          if (((msg.source == (void *) this->http_stream_reader_ && this->input_type_ == AudioMediaInputType::URL)
            || (msg.source == (void *) this->flash_stream_reader_ && this->input_type_ == AudioMediaInputType::FLASH))
            && message_status == AEL_STATUS_STATE_FINISHED) {
            esph_log_d(TAG, "Finished event received");
            if (this->state_ == AudioMediaPipelineState::ANNOUNCING) {
              this->set_state_(AudioMediaPipelineState::STOPPING_ANNOUNCING);
            }
            else {
              this->set_state_(AudioMediaPipelineState::STOPPING);
              this->url_ = "";
            }
          }
        }

        // i2S is Stopped or Finished
        if (this->state_ == AudioMediaPipelineState::RUNNING
          || this->state_ == AudioMediaPipelineState::STOPPING
          || this->state_ == AudioMediaPipelineState::ANNOUNCING
          || this->state_ == AudioMediaPipelineState::STOPPING_ANNOUNCING) {
          if (msg.source == (void *) this->i2s_stream_writer_
            && (message_status == AEL_STATUS_STATE_STOPPED
            || message_status == AEL_STATUS_STATE_FINISHED)) {
            esph_log_d(TAG, "Stop event received");            
            if (this->state_ == AudioMediaPipelineState::ANNOUNCING
              || this->state_ == AudioMediaPipelineState::STOPPING_ANNOUNCING) {
              pipeline_stop_(false,false);
              this->is_announcement_ = false;
              this->set_state_(AudioMediaPipelineState::STOPPED_ANNOUNCING);
            }
            else {
              this->stop();
            }
          }
        }
      }
    }
  }
  return AudioMediaPipeline::loop();
}

void SimpleADFPipeline::pipeline_init_() {
  esph_log_d(TAG, "pipeline_init_ %s", audio_media_input_type_to_string(this->input_type_));

  this->pipeline_ = this->adf_audio_pipeline_init();

  if (this->input_type_ == AudioMediaInputType::URL) {
    this->http_stream_reader_ = this->adf_http_stream_init(this->http_stream_rb_size_);
  }
  else if (this->input_type_ == AudioMediaInputType::FLASH) {
    this->flash_stream_reader_ = this->adf_embed_flash_stream_init();
  }

  this->esp_decoder_ = this->adf_esp_decoder_init();
  this->i2s_stream_writer_ = this->adf_i2s_stream_init();

  if (this->input_type_ == AudioMediaInputType::URL) {
    audio_pipeline_register(this->pipeline_, this->http_stream_reader_, "http");
    audio_pipeline_register(this->pipeline_, this->esp_decoder_,        "decoder");
    audio_pipeline_register(this->pipeline_, this->i2s_stream_writer_,  "i2s");
    esph_log_d(TAG, "Link http_stream-->esp_decoder-->i2s_stream-->[codec_chip]");
    const char *link_tag[3] = {"http", "decoder", "i2s"};
    audio_pipeline_link(this->pipeline_, &link_tag[0], 3);
  }
  else if (this->input_type_ == AudioMediaInputType::FLASH) {
    audio_pipeline_register(this->pipeline_, this->flash_stream_reader_, "flash");
    audio_pipeline_register(this->pipeline_, this->esp_decoder_,         "decoder");
    audio_pipeline_register(this->pipeline_, this->i2s_stream_writer_,   "i2s");
    esph_log_d(TAG, "Link flash_stream-->esp_decoder-->i2s_stream-->[codec_chip]");
    const char *link_tag[3] = {"flash", "decoder", "i2s"};
    audio_pipeline_link(this->pipeline_, &link_tag[0], 3);
  }

  audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
  this->evt_ = audio_event_iface_init(&evt_cfg);
  audio_pipeline_set_listener(this->pipeline_, this->evt_);
  
  AudioMediaPipeline::pipeline_init_();
}

void SimpleADFPipeline::pipeline_deinit_() {
  esph_log_d(TAG, "pipeline_deinit_ %s", audio_media_input_type_to_string(this->input_type_));

  audio_pipeline_terminate(this->pipeline_);

  if (this->input_type_ == AudioMediaInputType::URL) {
    audio_pipeline_unregister(this->pipeline_, this->http_stream_reader_);
  }
  else if (this->input_type_ == AudioMediaInputType::FLASH) {
    audio_pipeline_unregister(this->pipeline_, this->flash_stream_reader_);
  }
  audio_pipeline_unregister(this->pipeline_, this->i2s_stream_writer_);
  audio_pipeline_unregister(this->pipeline_, this->esp_decoder_);

  audio_pipeline_remove_listener(this->pipeline_);

  /* Make sure audio_pipeline_remove_listener & audio_event_iface_remove_listener are called before destroying event_iface */
  audio_event_iface_destroy(this->evt_);
  this->evt_ = nullptr;

  audio_pipeline_deinit(this->pipeline_);
  this->pipeline_ = nullptr;
  if (this->input_type_ == AudioMediaInputType::URL) {
    audio_element_deinit(this->http_stream_reader_);
    this->http_stream_reader_ = nullptr;
  }
  else if (this->input_type_ == AudioMediaInputType::FLASH) {
    audio_element_deinit(this->flash_stream_reader_);
    this->flash_stream_reader_ = nullptr;
  }
  this->i2s_stream_writer_ = nullptr;
  audio_element_deinit(this->esp_decoder_);
  this->esp_decoder_ = nullptr;

  AudioMediaPipeline::pipeline_deinit_();
}

void SimpleADFPipeline::switch_pipeline_input_(AudioMediaInputType input_type) {
  if (input_type != this->input_type_) {
    esph_log_d(TAG, "update_pipeline_input_ %s to %s"
      , audio_media_input_type_to_string(this->input_type_)
      , audio_media_input_type_to_string(input_type));
    if (this->is_initialized_) {
      audio_pipeline_terminate(this->pipeline_);
      audio_pipeline_unlink(this->pipeline_);

      if (this->input_type_ == AudioMediaInputType::URL) {
        audio_pipeline_unregister(this->pipeline_, this->http_stream_reader_);
        audio_element_deinit(this->http_stream_reader_);
        this->http_stream_reader_ = nullptr;
        this->flash_stream_reader_ = this->adf_embed_flash_stream_init();
        audio_pipeline_register(this->pipeline_, this->flash_stream_reader_, "flash");
        esph_log_d(TAG, "Link it together flash_stream-->esp_decoder-->i2s_stream-->[codec_chip]");
        const char *link_tag[3] = {"flash", "decoder", "i2s"};
        audio_pipeline_link(this->pipeline_, &link_tag[0], 3);
      }
      else if (this->input_type_ == AudioMediaInputType::FLASH) {
        audio_pipeline_unregister(this->pipeline_, this->flash_stream_reader_);
        audio_element_deinit(this->flash_stream_reader_);
        this->flash_stream_reader_ = nullptr;
        this->http_stream_reader_ = this->adf_http_stream_init(this->http_stream_rb_size_);
        audio_pipeline_register(this->pipeline_, this->http_stream_reader_, "http");
        esph_log_d(TAG, "Link it together http_stream-->esp_decoder-->i2s_stream-->[codec_chip]");
        const char *link_tag[3] = {"http", "decoder", "i2s"};
        audio_pipeline_link(this->pipeline_, &link_tag[0], 3);
      }

      audio_pipeline_set_listener(this->pipeline_, this->evt_);
    }
    this->input_type_ = input_type;
  }
}

void SimpleADFPipeline::pipeline_play_(bool resume) {
  esph_log_d(TAG, "pipeline_play_ %d", resume);
  if (this->is_initialized_) {
    this->set_volume(this->volume_);
    this->is_launched_ = false;
    if (resume) {
      esph_log_d(TAG, "resume pipeline");
      audio_element_resume(this->i2s_stream_writer_, 0, 2000 / portTICK_PERIOD_MS);
      if (this->is_announcement_) {
        this->set_state_(AudioMediaPipelineState::ANNOUNCING);
      }
      else {
        this->set_state_(AudioMediaPipelineState::RUNNING);
      }
    }
    else {
      esph_log_d(TAG, "run pipeline");
      audio_pipeline_run(this->pipeline_);
    }
    this->is_launched_ = true;
  }
}

void SimpleADFPipeline::pipeline_stop_(bool pause, bool cleanup) {
  esph_log_d(TAG, "pipeline_stop_ %d %d", pause, cleanup);
  if (this->is_initialized_) {
    if (pause && this->is_launched_) {
      esph_log_d(TAG, "pause pipeline");
      audio_element_pause(this->i2s_stream_writer_);
      this->is_launched_ = false;
    }
    else if (!pause) {
      esph_log_d(TAG, "stop pipeline");
      audio_pipeline_stop(this->pipeline_);
      audio_pipeline_wait_for_stop(this->pipeline_);
      this->is_launched_ = false;
    }
  }

  if (this->is_initialized_ && !pause) {
    esph_log_d(TAG, "reset pipeline");
    audio_pipeline_reset_ringbuffer(this->pipeline_);
    audio_pipeline_reset_elements(this->pipeline_);
    audio_pipeline_reset_items_state(this->pipeline_);
  }

  if (cleanup) {
    this->clean_up();
  }
}

}  // namespace esp_audio
}  // namespace esphome
#endif // USE_ADF_SIMPLE_PIPELINE
