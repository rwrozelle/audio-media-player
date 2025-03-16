#include "complex_adf_pipeline.h"

#ifdef USE_ADF_COMPLEX_PIPELINE

#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace esp_audio {

static const char *const TAG = "complex_adf_media_pipeline";

void ComplexADFPipeline::dump_config() {
  AudioMediaPipeline::dump_config();
}


void ComplexADFPipeline::play(bool resume) {
  esph_log_d(TAG, "Called play with pipeline state: %s, '%s'",pipeline_state_to_string(this->state_), this->url_.c_str());
  if (this->url_.length() > 0 
    && ((!resume && this->state_ == AudioMediaPipelineState::STOPPED)
      || (resume && this->state_ == AudioMediaPipelineState::PAUSED))) {
    if (resume) {
      this->set_state_(AudioMediaPipelineState::RESUMING);
    }
    else {
      this->set_state_(AudioMediaPipelineState::STARTING);
    }

    if (!resume && this->input_type_1_ == AudioMediaInputType::FLASH) {
      this->pipeline_stop_(false, false);
      this->switch_pipeline_1_input_(AudioMediaInputType::URL);
    }

    if (!this->is_initialized_) {
      this->pipeline_init_();
    }

    if (!resume) {
      audio_element_set_uri(this->http_stream_reader_1_, this->get_transcode_url_(this->url_).c_str());
      esph_log_d(TAG, "Play: %s", this->url_.c_str());
      this->is_music_info_set_1_ = false;
    }
    this->pipeline_play_(resume);
  }
}

void ComplexADFPipeline::play_announcement_(const std::string& url) {
  esph_log_d(TAG, "Called play_announcement with pipeline state: %s",pipeline_state_to_string(this->state_));
  if (url.length() > 0) { 
    if (this->state_ == AudioMediaPipelineState::STOPPED 
      || this->state_ == AudioMediaPipelineState::STOPPING)  {
        this->url_ = url;
        this->play();
    }
    else if (this->state_ == AudioMediaPipelineState::STARTING
    || this->state_ == AudioMediaPipelineState::RUNNING) {
      if (this->input_type_2_ == AudioMediaInputType::FLASH) {
        this->switch_pipeline_2_input_(AudioMediaInputType::URL);
      }
      audio_element_set_uri(this->http_stream_reader_2_, this->get_transcode_url_(url).c_str());
      esph_log_d(TAG, "Play Announcement: %s", url.c_str());
      this->is_music_info_set_2_ = false;
      this->pipeline_announce_(); 
    }
    else if (this->state_ == AudioMediaPipelineState::PAUSING
    || this->state_ == AudioMediaPipelineState::PAUSED) {
      this->pipeline_stop_(false, false);
      if (this->input_type_2_ == AudioMediaInputType::FLASH) {
        this->switch_pipeline_2_input_(AudioMediaInputType::URL);
      }
      this->url_ = url;
      this->set_state_(AudioMediaPipelineState::STARTING);
      if (!this->is_initialized_) {
        this->pipeline_init_();
      }
      audio_element_set_uri(this->http_stream_reader_1_, this->get_transcode_url_(this->url_).c_str());
      esph_log_d(TAG, "Play: %s", this->url_.c_str());
      this->is_music_info_set_1_ = false;
      this->pipeline_play_(false);
    }
  }
}

void ComplexADFPipeline::play_announcement_(audio::AudioFile *media_file) {
  esph_log_d(TAG, "play_file");
  embed_item_info_t embed_item_info[] = {
    [0] = {
        .address = media_file->data,
        .size    = (int)media_file->length,
        },
  };
  std::string url = "embed://tone/0_dummy";
  std::string file_type = audio::audio_file_type_to_string(media_file->file_type);
  url = url + file_type;
  if (this->state_ == AudioMediaPipelineState::STOPPED 
    || this->state_ == AudioMediaPipelineState::STOPPING
    || this->state_ == AudioMediaPipelineState::PAUSING
    || this->state_ == AudioMediaPipelineState::PAUSED) {
    this->pipeline_stop_(false, false);
    if (this->input_type_1_ == AudioMediaInputType::URL) {
      this->switch_pipeline_1_input_(AudioMediaInputType::FLASH);
    }
      this->set_state_(AudioMediaPipelineState::STARTING);
    if (!this->is_initialized_) {
      this->pipeline_init_();
    }
    embed_flash_stream_set_context(this->flash_stream_reader_1_, (embed_item_info_t *)&embed_item_info[0], 1);
    audio_element_set_uri(this->flash_stream_reader_1_, url.c_str());
    this->is_music_info_set_1_ = false;
    this->pipeline_play_(false);    
  }
  else if (this->state_ == AudioMediaPipelineState::STARTING
  || this->state_ == AudioMediaPipelineState::RUNNING) {
    if (this->input_type_2_ == AudioMediaInputType::URL) {
      this->switch_pipeline_2_input_(AudioMediaInputType::FLASH);
    }
    if (!this->is_initialized_) {
      this->pipeline_init_();
    }
    embed_flash_stream_set_context(this->flash_stream_reader_2_, (embed_item_info_t *)&embed_item_info[0], 1);
    audio_element_set_uri(this->flash_stream_reader_2_, url.c_str());
    this->is_music_info_set_2_ = false;
    this->pipeline_announce_();    
  }
}

void ComplexADFPipeline::stop(bool pause) {
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
  || this->state_ == AudioMediaPipelineState::PAUSED) {
    if (pause) {
      this->set_state_(AudioMediaPipelineState::PAUSING);
    }
    else {
      this->set_state_(AudioMediaPipelineState::STOPPING);
    }
    this->pipeline_stop_(pause, cleanup);
  }
  if (pause) {
    this->set_state_(AudioMediaPipelineState::PAUSED);
  }
  else {
    this->set_state_(AudioMediaPipelineState::STOPPED);
  }
}

AudioMediaPipelineState ComplexADFPipeline::loop() {

  if (!this->announcements_.empty() && !this->is_play_announcement_) {
    this->is_play_announcement_ = true;
    AudioAnouncement announcement = this->announcements_.front();
    if (announcement.url.has_value()) {
      this->play_announcement_(announcement.url.value());
    }
    else if (announcement.file.has_value()) {
      this->play_announcement_(announcement.file.value());
    }
    this->announcements_.pop_front();
  }
  else if (this->is_launched_) {
    audio_event_iface_msg_t msg;
    esp_err_t ret = audio_event_iface_listen(this->evt_, &msg, 0);
    if (ret == ESP_OK) {
      // Set Music Info pipeline 1
      if ((!this->isServerTranscoding_() || this->input_type_1_ == AudioMediaInputType::FLASH)
      && this->is_music_info_set_1_ == false
      && msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT
      && msg.source == (void *) this->esp_decoder_1_
      && msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO) {
        audio_element_info_t music_info = {0};
        audio_element_getinfo(this->esp_decoder_1_, &music_info);

        esph_log_d(TAG, "[ decoder1 ] Receive music info, sample_rates=%d, bits=%d, ch=%d",
          music_info.sample_rates, music_info.bits, music_info.channels);
        rsp_filter_change_src_info(this->rsp_filter_1_, (int)music_info.sample_rates, (int)music_info.channels, (int)music_info.bits);         
        this->is_music_info_set_1_ = true;
      }
      // Set Music Info pipeline 2
      if ((!this->isServerTranscoding_() || this->input_type_2_ == AudioMediaInputType::FLASH)
      && this->is_music_info_set_2_ == false
      && msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT
      && msg.source == (void *) this->esp_decoder_2_
      && msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO) {
        audio_element_info_t music_info = {0};
        audio_element_getinfo(this->esp_decoder_2_, &music_info);

        esph_log_d(TAG, "[ decoder2 ] Receive music info, sample_rates=%d, bits=%d, ch=%d",
          music_info.sample_rates, music_info.bits, music_info.channels);

        rsp_filter_change_src_info(this->rsp_filter_2_, (int)music_info.sample_rates, (int)music_info.channels, (int)music_info.bits);
        this->is_music_info_set_2_ = true;
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
          || this->state_ == AudioMediaPipelineState::RESUMING) {
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
          else if ((((this->isServerTranscoding_() && this->input_type_1_ != AudioMediaInputType::FLASH) && msg.source == (void *)this->http_stream_reader_1_)
           || ((!this->isServerTranscoding_() || this->input_type_1_ == AudioMediaInputType::FLASH) && msg.source == (void *)this->flash_stream_reader_1_))
            && message_status == AEL_STATUS_STATE_RUNNING) {
            esph_log_d(TAG, "Running event received");
            this->set_state_(AudioMediaPipelineState::RUNNING);
          }
        }

        // Http or Flash is Finished
        if (this->state_ == AudioMediaPipelineState::RUNNING) {
          if ((((this->isServerTranscoding_() && this->input_type_1_ != AudioMediaInputType::FLASH) && msg.source == (void *)this->http_stream_reader_1_)
           || ((!this->isServerTranscoding_() || this->input_type_1_ == AudioMediaInputType::FLASH) && msg.source == (void *)this->flash_stream_reader_1_))
            && message_status == AEL_STATUS_STATE_FINISHED) {
            esph_log_d(TAG, "Finished event received");
            this->set_state_(AudioMediaPipelineState::STOPPING);
            this->url_ = "";
          }
        }

        // i2S is Stopped or Finished
        if (this->state_ == AudioMediaPipelineState::RUNNING
          || this->state_ == AudioMediaPipelineState::STOPPING) {
          if (msg.source == (void *) this->i2s_stream_writer_
            && (message_status == AEL_STATUS_STATE_STOPPED
              || message_status == AEL_STATUS_STATE_FINISHED)) {
            esph_log_d(TAG, "Stop event received");            
            this->stop();
          }
        }

        //announcement pipeline
        if (this->state_ == AudioMediaPipelineState::RUNNING) {
          if (!this->is_announcement_ 
          && (((this->isServerTranscoding_() && this->input_type_2_ != AudioMediaInputType::FLASH) && msg.source == (void *)this->esp_decoder_2_)
           || ((!this->isServerTranscoding_() || this->input_type_2_ == AudioMediaInputType::FLASH) && msg.source == (void *)this->rsp_filter_2_))
            && (message_status == AEL_STATUS_STATE_RUNNING)) {
            esph_log_d(TAG, "Announcement start event received");            
            downmix_set_work_mode(this->downmixer_, ESP_DOWNMIX_WORK_MODE_SWITCH_ON);
            downmix_set_input_rb_timeout(this->downmixer_, 50, 1);
            this->is_announcement_ = true;
          }
          if (this->is_announcement_ 
          && (((this->isServerTranscoding_() && this->input_type_2_ != AudioMediaInputType::FLASH) && msg.source == (void *)this->esp_decoder_2_)
           || ((!this->isServerTranscoding_() || this->input_type_2_ == AudioMediaInputType::FLASH) && msg.source == (void *)this->rsp_filter_2_))
            && (message_status == AEL_STATUS_STATE_STOPPED || message_status == AEL_STATUS_STATE_FINISHED)) {
            esph_log_d(TAG, "Announcement Stop event received");            
            downmix_set_work_mode(this->downmixer_, ESP_DOWNMIX_WORK_MODE_SWITCH_OFF);
            downmix_set_input_rb_timeout(this->downmixer_, 0, 1);
            audio_pipeline_stop(this->pipeline_2_);
            audio_pipeline_wait_for_stop(this->pipeline_2_);
            audio_pipeline_reset_ringbuffer(this->pipeline_2_);
            audio_pipeline_reset_elements(this->pipeline_2_);
            audio_pipeline_reset_items_state(this->pipeline_2_);
            this->is_announcement_ = false;
            this->is_play_announcement_ = false;
            downmix_set_work_mode(this->downmixer_, ESP_DOWNMIX_WORK_MODE_BYPASS);
          }
        }
      }
    }
  }
  return AudioMediaPipeline::loop();
}

void ComplexADFPipeline::pipeline_init_() {
  esph_log_d(TAG, "pipeline_init_");

  audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
  this->evt_ = audio_event_iface_init(&evt_cfg);

  this->pipeline_1_init_();
  this->pipeline_2_init_();
  this->pipeline_3_init_();

  ringbuf_handle_t rb_1 = audio_element_get_input_ringbuf(this->raw_writer_1_);
  downmix_set_input_rb(this->downmixer_, rb_1, 0);
  ringbuf_handle_t rb_2 = audio_element_get_input_ringbuf(this->raw_writer_2_);
  downmix_set_input_rb(this->downmixer_, rb_2, 1);

  this->is_announcement_ = false;
  AudioMediaPipeline::pipeline_init_();
}

void ComplexADFPipeline::pipeline_1_init_() {
  esph_log_d(TAG, "pipeline_1_init_ %s", audio_media_input_type_to_string(this->input_type_1_));

  this->pipeline_1_ = this->adf_audio_pipeline_init();

  if (this->input_type_1_ == AudioMediaInputType::URL) {
    this->http_stream_reader_1_ = this->adf_http_stream_init(this->http_stream_rb_size_);
  }
  else if (this->input_type_1_ == AudioMediaInputType::FLASH) {
    this->flash_stream_reader_1_ = this->adf_embed_flash_stream_init();
  }

  this->esp_decoder_1_ = this->adf_esp_decoder_init();

  if (!this->isServerTranscoding_() || this->input_type_1_ == AudioMediaInputType::FLASH) {
    this->rsp_filter_1_ = this->adf_rsp_filter_init();
  }

  this->raw_writer_1_ = adf_raw_stream_init();

  audio_pipeline_register(this->pipeline_1_, this->esp_decoder_1_,"decoder1"); 
  audio_pipeline_register(this->pipeline_1_, this->raw_writer_1_, "raw1"); 
  if (this->input_type_1_ == AudioMediaInputType::URL) {
    audio_pipeline_register(this->pipeline_1_, this->http_stream_reader_1_, "http1");
    if (this->isServerTranscoding_()) {
      esph_log_d(TAG, "Link http1-->decoder1-->raw1");
      const char *link_tag[3] = {"http1", "decoder1", "raw1"};
      audio_pipeline_link(this->pipeline_1_, &link_tag[0], 3);
    }
    else {
      audio_pipeline_register(this->pipeline_1_, this->rsp_filter_1_, "resampler1");
      esph_log_d(TAG, "Link http1-->decoder1-->resampler1-->raw1");
      const char *link_tag[4] = {"http1", "decoder1", "resampler1", "raw1"};
      audio_pipeline_link(this->pipeline_1_, &link_tag[0], 4);
    }
  }
  else if (this->input_type_1_ == AudioMediaInputType::FLASH) {
    audio_pipeline_register(this->pipeline_1_, this->flash_stream_reader_1_, "flash1");
    audio_pipeline_register(this->pipeline_1_, this->rsp_filter_1_, "resampler1");
    esph_log_d(TAG, "Link flash-->decoder1-->resampler1-->raw1");
    const char *link_tag[4] = {"flash1", "decoder1", "resampler1", "raw1"};
    audio_pipeline_link(this->pipeline_1_, &link_tag[0], 4);
  }

  audio_pipeline_set_listener(this->pipeline_1_, this->evt_);
}

void ComplexADFPipeline::pipeline_2_init_() {
  esph_log_d(TAG, "pipeline_2_init_ %s", audio_media_input_type_to_string(this->input_type_2_));

  this->pipeline_2_ = this->adf_audio_pipeline_init();

  if (this->input_type_2_ == AudioMediaInputType::URL) {
    this->http_stream_reader_2_ = this->adf_http_stream_init(this->http_stream_rb_size_);
  }
  else if (this->input_type_2_ == AudioMediaInputType::FLASH) {
    this->flash_stream_reader_2_ = this->adf_embed_flash_stream_init();
  }

  this->esp_decoder_2_ = this->adf_esp_decoder_init();

  if (!this->isServerTranscoding_() || this->input_type_2_ == AudioMediaInputType::FLASH) {
    this->rsp_filter_2_ = this->adf_rsp_filter_init();
  }

  this->raw_writer_2_ = adf_raw_stream_init();

  audio_pipeline_register(this->pipeline_2_, this->esp_decoder_2_,"decoder2"); 
  audio_pipeline_register(this->pipeline_2_, this->raw_writer_2_, "raw2"); 
  if (this->input_type_2_ == AudioMediaInputType::URL) {
    audio_pipeline_register(this->pipeline_2_, this->http_stream_reader_2_, "http2");
    if (this->isServerTranscoding_()) {
      esph_log_d(TAG, "Link http2-->decoder2-->raw2");
      const char *link_tag[3] = {"http2", "decoder2", "raw2"};
      audio_pipeline_link(this->pipeline_2_, &link_tag[0], 3);
    }
    else {
      audio_pipeline_register(this->pipeline_2_, this->rsp_filter_2_, "resampler2");
      esph_log_d(TAG, "Link http2-->decoder2-->resampler2-->raw2");
      const char *link_tag[4] = {"http2", "decoder2", "resampler2", "raw2"};
      audio_pipeline_link(this->pipeline_2_, &link_tag[0], 4);
    }
  }
  else if (this->input_type_2_ == AudioMediaInputType::FLASH) {
    audio_pipeline_register(this->pipeline_2_, this->flash_stream_reader_2_, "flash2");
    audio_pipeline_register(this->pipeline_2_, this->rsp_filter_2_, "resampler2");
    esph_log_d(TAG, "Link flash-->decoder2-->resampler2-->raw2");
    const char *link_tag[4] = {"flash2", "decoder2", "resampler2", "raw2"};
    audio_pipeline_link(this->pipeline_2_, &link_tag[0], 4);
  }

  audio_pipeline_set_listener(this->pipeline_2_, this->evt_);
}

void ComplexADFPipeline::pipeline_3_init_() {
  esph_log_d(TAG, "pipeline_3_init_");

  this->pipeline_3_ = this->adf_audio_pipeline_init();
  this->downmixer_ = this->adf_downmix_init();
  this->i2s_stream_writer_ = this->adf_i2s_stream_init();

  audio_pipeline_register(this->pipeline_3_, this->downmixer_,         "downmixer");
  audio_pipeline_register(this->pipeline_3_, this->i2s_stream_writer_, "i2s");
  esph_log_d(TAG, "Link downmixer-->i2s_stream-->[codec_chip]");
  const char *link_tag[2] = {"downmixer", "i2s"};
  audio_pipeline_link(this->pipeline_3_, &link_tag[0], 2);

  audio_pipeline_set_listener(this->pipeline_3_, this->evt_);
}

void ComplexADFPipeline::pipeline_deinit_() {

  esph_log_d(TAG, "terminate pipelines");
  this->pipeline_1_deinit_();
  this->pipeline_2_deinit_();
  this->pipeline_3_deinit_();

  /* Make sure audio_pipeline_remove_listener & audio_event_iface_remove_listener are called before destroying event_iface */
  audio_event_iface_destroy(this->evt_);
  this->evt_ = nullptr;

  this->is_announcement_ = false;
  this->is_play_announcement_ = false;
  AudioMediaPipeline::pipeline_deinit_();
}

void ComplexADFPipeline::pipeline_1_deinit_() {
  esph_log_d(TAG, "pipeline_1_deinit_ %s", audio_media_input_type_to_string(this->input_type_1_));

  audio_pipeline_terminate(this->pipeline_1_);

  if (this->input_type_1_ == AudioMediaInputType::URL) {
    audio_pipeline_unregister(this->pipeline_1_, this->http_stream_reader_1_);
  }
  else if (this->input_type_1_ == AudioMediaInputType::FLASH) {
    audio_pipeline_unregister(this->pipeline_1_, this->flash_stream_reader_1_);
  }
  audio_pipeline_unregister(this->pipeline_1_, this->esp_decoder_1_);

  if (!this->isServerTranscoding_() || this->input_type_1_ == AudioMediaInputType::FLASH) {
    audio_pipeline_unregister(this->pipeline_1_, this->rsp_filter_1_);
  }
  audio_pipeline_unregister(this->pipeline_1_, this->raw_writer_1_);

  audio_pipeline_remove_listener(this->pipeline_1_);

  audio_pipeline_deinit(this->pipeline_1_);
  this->pipeline_1_ = nullptr;

  if (this->input_type_1_ == AudioMediaInputType::URL) {
    audio_element_deinit(this->http_stream_reader_1_);
    this->http_stream_reader_1_ = nullptr;
  }
  else if (this->input_type_1_ == AudioMediaInputType::FLASH) {
    audio_element_deinit(this->flash_stream_reader_1_);
    this->flash_stream_reader_1_ = nullptr;
  }
  audio_element_deinit(this->esp_decoder_1_);
  this->esp_decoder_1_ = nullptr;

  if (!this->isServerTranscoding_() || this->input_type_1_ == AudioMediaInputType::FLASH) {
    audio_element_deinit(this->rsp_filter_1_);
    this->rsp_filter_1_ = nullptr;
  }
  audio_element_deinit(this->raw_writer_1_);
  this->raw_writer_1_ = nullptr;
}

void ComplexADFPipeline::pipeline_2_deinit_() {
  esph_log_d(TAG, "pipeline_2_deinit_ %s", audio_media_input_type_to_string(this->input_type_2_));

  audio_pipeline_terminate(this->pipeline_2_);

  if (this->input_type_2_ == AudioMediaInputType::URL) {
    audio_pipeline_unregister(this->pipeline_2_, this->http_stream_reader_2_);
  }
  else if (this->input_type_2_ == AudioMediaInputType::FLASH) {
    audio_pipeline_unregister(this->pipeline_2_, this->flash_stream_reader_2_);
  }
  audio_pipeline_unregister(this->pipeline_2_, this->esp_decoder_2_);

  if (!this->isServerTranscoding_() || this->input_type_2_ == AudioMediaInputType::FLASH) {
    audio_pipeline_unregister(this->pipeline_2_, this->rsp_filter_2_);
  }
  audio_pipeline_unregister(this->pipeline_2_, this->raw_writer_2_);

  audio_pipeline_remove_listener(this->pipeline_2_);

  audio_pipeline_deinit(this->pipeline_2_);
  this->pipeline_2_ = nullptr;

  if (this->input_type_2_ == AudioMediaInputType::URL) {
    audio_element_deinit(this->http_stream_reader_2_);
    this->http_stream_reader_2_ = nullptr;
  }
  else if (this->input_type_2_ == AudioMediaInputType::FLASH) {
    audio_element_deinit(this->flash_stream_reader_2_);
    this->flash_stream_reader_2_ = nullptr;
  }
  audio_element_deinit(this->esp_decoder_2_);
  this->esp_decoder_2_ = nullptr;

  if (!this->isServerTranscoding_() || this->input_type_2_ == AudioMediaInputType::FLASH) {
    audio_element_deinit(this->rsp_filter_2_);
    this->rsp_filter_2_ = nullptr;
  }
  audio_element_deinit(this->raw_writer_2_);
  this->raw_writer_2_ = nullptr;
}

void ComplexADFPipeline::pipeline_3_deinit_() {
  esph_log_d(TAG, "pipeline_3_deinit_");

  audio_pipeline_terminate(this->pipeline_3_);

  audio_pipeline_unregister(this->pipeline_3_, this->downmixer_);
  audio_pipeline_unregister(this->pipeline_3_, this->i2s_stream_writer_);

  audio_pipeline_remove_listener(this->pipeline_3_);

  audio_pipeline_deinit(this->pipeline_3_);
  this->pipeline_3_ = nullptr;

  audio_element_deinit(this->i2s_stream_writer_);
  this->i2s_stream_writer_ = nullptr;
  audio_element_deinit(this->downmixer_);
  this->downmixer_ = nullptr;
}

void ComplexADFPipeline::switch_pipeline_1_input_(AudioMediaInputType input_type) {
  if (input_type != this->input_type_1_) {    
    esph_log_d(TAG, "update_pipeline_1_input_ %s to %s"
      , audio_media_input_type_to_string(this->input_type_1_)
      , audio_media_input_type_to_string(input_type));
    if (this->is_initialized_) {
      audio_pipeline_terminate(this->pipeline_1_);
      audio_pipeline_unlink(this->pipeline_1_);

      if (this->input_type_1_ == AudioMediaInputType::URL) {
        audio_pipeline_unregister(this->pipeline_1_, this->http_stream_reader_1_);
        audio_element_deinit(this->http_stream_reader_1_);
        this->http_stream_reader_1_ = nullptr;
        this->flash_stream_reader_1_ = this->adf_embed_flash_stream_init();
        audio_pipeline_register(this->pipeline_1_, this->flash_stream_reader_1_, "flash1");
        if (this->isServerTranscoding_()) {
          this->rsp_filter_1_ = this->adf_rsp_filter_init();
          audio_pipeline_register(this->pipeline_1_, this->rsp_filter_1_, "resampler1");
        }
        esph_log_d(TAG, "Link flash1-->decoder1-->resampler1-->raw1");
        const char *link_tag[4] = {"flash1", "decoder1", "resampler1", "raw1"};
        audio_pipeline_link(this->pipeline_1_, &link_tag[0], 4);
      }
      else if (this->input_type_1_ == AudioMediaInputType::FLASH) {
        audio_pipeline_unregister(this->pipeline_1_, this->flash_stream_reader_1_);
        audio_element_deinit(this->flash_stream_reader_1_);
        this->flash_stream_reader_1_ = nullptr;
        this->http_stream_reader_1_ = this->adf_http_stream_init(this->http_stream_rb_size_);
        audio_pipeline_register(this->pipeline_1_, this->http_stream_reader_1_, "http1");
        if (this->isServerTranscoding_()) {
          audio_pipeline_unregister(this->pipeline_1_, this->rsp_filter_1_);
          audio_element_deinit(this->rsp_filter_1_);
          this->rsp_filter_1_ = nullptr;
          esph_log_d(TAG, "Link http1-->decoder1-->raw1");
          const char *link_tag[3] = {"http1", "decoder1", "raw1"};
          audio_pipeline_link(this->pipeline_1_, &link_tag[0], 3);
        }
        else {
          audio_pipeline_register(this->pipeline_1_, this->rsp_filter_1_, "resampler1");
          esph_log_d(TAG, "Link http1-->decoder1-->resampler1-->raw1");
          const char *link_tag[4] = {"http1", "decoder1", "resampler1", "raw1"};
          audio_pipeline_link(this->pipeline_1_, &link_tag[0], 4);
        }
      }

      audio_pipeline_set_listener(this->pipeline_1_, this->evt_);
      ringbuf_handle_t rb_1 = audio_element_get_input_ringbuf(this->raw_writer_1_);
      downmix_set_input_rb(this->downmixer_, rb_1, 0);
    }
    this->input_type_1_ = input_type;
  }
}

void ComplexADFPipeline::switch_pipeline_2_input_(AudioMediaInputType input_type) {
  if (input_type != this->input_type_2_) {    
    esph_log_d(TAG, "update_pipeline_2_input_ %s to %s"
      , audio_media_input_type_to_string(this->input_type_2_)
      , audio_media_input_type_to_string(input_type));
    if (this->is_initialized_) {
      audio_pipeline_terminate(this->pipeline_2_);
      audio_pipeline_unlink(this->pipeline_2_);

      if (this->input_type_2_ == AudioMediaInputType::URL) {
        audio_pipeline_unregister(this->pipeline_2_, this->http_stream_reader_2_);
        audio_element_deinit(this->http_stream_reader_2_);
        this->http_stream_reader_2_ = nullptr;
        this->flash_stream_reader_2_ = this->adf_embed_flash_stream_init();
        audio_pipeline_register(this->pipeline_2_, this->flash_stream_reader_2_, "flash2");
        if (this->isServerTranscoding_()) {
          this->rsp_filter_2_ = this->adf_rsp_filter_init();
          audio_pipeline_register(this->pipeline_2_, this->rsp_filter_2_, "resampler2");
        }
        esph_log_d(TAG, "Link flash2-->decoder2-->resampler2-->raw2");
        const char *link_tag[4] = {"flash2", "decoder2", "resampler2", "raw2"};
        audio_pipeline_link(this->pipeline_2_, &link_tag[0], 4);
      }
      else if (this->input_type_2_ == AudioMediaInputType::FLASH) {
        audio_pipeline_unregister(this->pipeline_2_, this->flash_stream_reader_2_);
        audio_element_deinit(this->flash_stream_reader_2_);
        this->flash_stream_reader_2_ = nullptr;
        this->http_stream_reader_2_ = this->adf_http_stream_init(HTTP_STREAM_RINGBUFFER_SIZE);
        audio_pipeline_register(this->pipeline_2_, this->http_stream_reader_2_, "http2");
        if (this->isServerTranscoding_()) {
          audio_pipeline_unregister(this->pipeline_2_, this->rsp_filter_2_);
          audio_element_deinit(this->rsp_filter_2_);
          this->rsp_filter_2_ = nullptr;
          esph_log_d(TAG, "Link http2-->decoder2-->raw2");
          const char *link_tag[3] = {"http2", "decoder2", "raw2"};
          audio_pipeline_link(this->pipeline_2_, &link_tag[0], 3);
        }
        else {
          audio_pipeline_register(this->pipeline_2_, this->rsp_filter_2_, "resampler2");
          esph_log_d(TAG, "Link http2-->decoder2-->resampler2-->raw2");
          const char *link_tag[4] = {"http2", "decoder2", "resampler2", "raw2"};
          audio_pipeline_link(this->pipeline_2_, &link_tag[0], 4);
        }
      }

      audio_pipeline_set_listener(this->pipeline_2_, this->evt_);
      ringbuf_handle_t rb_2 = audio_element_get_input_ringbuf(this->raw_writer_2_);
      downmix_set_input_rb(this->downmixer_, rb_2, 1);
    }
    this->input_type_2_ = input_type;
  }
}

void ComplexADFPipeline::pipeline_play_(bool resume) {
  esph_log_d(TAG, "pipeline_play_ %d", resume);
  if (this->is_initialized_) {
    this->set_volume(this->volume_);
    this->is_launched_ = false;
    if (resume) {
      esph_log_d(TAG, "resume pipeline");
      audio_element_resume(this->i2s_stream_writer_, 0, 2000 / portTICK_PERIOD_MS);
      this->set_state_(AudioMediaPipelineState::RUNNING);
    }
    else {
      esph_log_d(TAG, "run pipeline");
      audio_pipeline_run(this->pipeline_1_);
      audio_pipeline_run(this->pipeline_3_);
      downmix_set_work_mode(this->downmixer_, ESP_DOWNMIX_WORK_MODE_BYPASS);
      /* A mixer is used to mix multiple channels of data. It reads data through polling.
       Each channel has a timeout. If the mixer fails to read data within the timeout period,
       the data on this path will be set to mute and mixing processing will continue.
       
       It is not recommended to set the timeout of each channel too large,
       as excessive timeout can affect the reading of data from other channels.
       Similarly, it is not recommended to set the timeout time too small,
       as it can make it difficult to read data in a timely manner,
       and may result in intermittent situations */
    downmix_set_input_rb_timeout(this->downmixer_, 50, 0);
    }
    this->is_launched_ = true;
  }
}

void ComplexADFPipeline::pipeline_announce_() {
  esph_log_d(TAG, "pipeline_announce_");
  if (this->is_initialized_) {
    esph_log_d(TAG, "run announcement pipeline");
    audio_pipeline_run(this->pipeline_2_);
    this->is_announcement_ = false;
  }
}

void ComplexADFPipeline::pipeline_stop_(bool pause, bool cleanup) {
  esph_log_d(TAG, "pipeline_stop_ %d %d", pause, cleanup);
  if (this->is_initialized_) {
    if (pause && this->is_launched_) {
      esph_log_d(TAG, "pause pipeline");
      audio_element_pause(this->i2s_stream_writer_);
      this->is_launched_ = false;
    }
    else if (!pause) {
      esph_log_d(TAG, "stop pipeline");
      audio_pipeline_stop(this->pipeline_3_);
      audio_pipeline_wait_for_stop(this->pipeline_3_);
      audio_pipeline_stop(this->pipeline_1_);
      audio_pipeline_wait_for_stop(this->pipeline_1_);
      audio_pipeline_stop(this->pipeline_2_);
      audio_pipeline_wait_for_stop(this->pipeline_2_);
      this->is_launched_ = false;
    }
  }

  if (this->is_initialized_ && !pause) {
    esph_log_d(TAG, "reset pipeline");
    audio_pipeline_reset_ringbuffer(this->pipeline_3_);
    audio_pipeline_reset_elements(this->pipeline_3_);
    audio_pipeline_reset_items_state(this->pipeline_3_);
    audio_pipeline_reset_ringbuffer(this->pipeline_1_);
    audio_pipeline_reset_elements(this->pipeline_1_);
    audio_pipeline_reset_items_state(this->pipeline_1_);
    audio_pipeline_reset_ringbuffer(this->pipeline_2_);
    audio_pipeline_reset_elements(this->pipeline_2_);
    audio_pipeline_reset_items_state(this->pipeline_2_);
  }

  if (cleanup) {
    this->clean_up();
  }
}

}  // namespace esp_audio
}  // namespace esphome
#endif // USE_ADF_COMPLEX_PIPELINE
