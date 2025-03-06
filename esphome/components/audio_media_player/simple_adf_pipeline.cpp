#include "simple_adf_pipeline.h"

#ifdef USE_ESP_IDF

#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esp_http_client.h"

namespace esphome {
namespace esp_adf {

static const char *const TAG = "simple_adf_media_pipeline";

void SimpleAdfMediaPipeline::dump_config() {
  esph_log_config(TAG, "SimpleAdfMediaPipeline");
  AdfMediaPipeline::dump_config();
}

void SimpleAdfMediaPipeline::set_url(const std::string& url, bool is_announcement) {
  this->is_announcement_ = is_announcement;
  if (is_announcement) {
    this->pipeline_stop_(false, false);
    this->state_ = AdfPipelineState::STOP_ANNOUNCING;
    this->play_announcement_(this->get_transcode_url_(url).c_str()); 
  }
  else {
    this->url_ = this->get_transcode_url_(url);
  }
}

void SimpleAdfMediaPipeline::play(bool resume) {
  esph_log_d(TAG, "Called play with pipeline state: %s",pipeline_state_to_string(this->state_));
  if (this->url_.length() > 0 
    && ((!resume && this->state_ == AdfPipelineState::STOPPED)
      || (resume && this->state_ == AdfPipelineState::PAUSED)
      || !resume && this->state_ == AdfPipelineState::STOP_ANNOUNCING)) {
    if (resume) {
      this->set_state_(AdfPipelineState::RESUMING);
    }
    else {
      this->set_state_(AdfPipelineState::STARTING);
    }

    if (!this->is_initialized_) {
      this->pipeline_init_();
    }

    if (!resume) {
      audio_element_set_uri(this->http_stream_reader_, this->url_.c_str());
      esph_log_d(TAG, "Play: %s", this->url_.c_str());
      this->is_music_info_set_ = false;
    }
    pipeline_play_(resume);
  }
}

void SimpleAdfMediaPipeline::play_announcement_(const std::string& url) {
  esph_log_d(TAG, "Called play announcement");
  this->set_state_(AdfPipelineState::START_ANNOUNCING);
  if (!this->is_initialized_) {
    this->pipeline_init_();
  }
  audio_element_set_uri(this->http_stream_reader_, url.c_str());
  esph_log_d(TAG, "Play: %s", url.c_str());
  this->is_music_info_set_ = false;
  pipeline_play_(false);
}

void SimpleAdfMediaPipeline::stop(bool pause) {

  if (pause) {
    esph_log_d(TAG, "Called pause with pipeline state: %s",pipeline_state_to_string(this->state_));
  }
  else {
    esph_log_d(TAG, "Called stop with pipeline state: %s",pipeline_state_to_string(this->state_));
  }
  bool cleanup = false;
  if (!pause && this->state_ == AdfPipelineState::PAUSED) {
    cleanup = true;
  }

  if (this->state_ == AdfPipelineState::STARTING
  || this->state_ == AdfPipelineState::RUNNING
  || this->state_ == AdfPipelineState::STOPPING
  || this->state_ == AdfPipelineState::PAUSING
  || this->state_ == AdfPipelineState::PAUSED
  || this->state_ == AdfPipelineState::START_ANNOUNCING
  || this->state_ == AdfPipelineState::ANNOUNCING
  || this->state_ == AdfPipelineState::STOP_ANNOUNCING) {
    if (pause) {
      this->set_state_(AdfPipelineState::PAUSING);
    }
    else if (this->is_announcement_) {
      this->set_state_(AdfPipelineState::STOP_ANNOUNCING);
    }
    else {
      this->set_state_(AdfPipelineState::STOPPING);
    }
    this->pipeline_stop_(pause, cleanup);
  }
  if (pause) {
    this->set_state_(AdfPipelineState::PAUSED);
  }
  else if (!this->is_announcement_) {
    this->set_state_(AdfPipelineState::STOPPED);
  }
}

AdfPipelineState SimpleAdfMediaPipeline::loop() {

  if (this->is_launched_) {
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

          if (this->rate_ != music_info.sample_rates || this->bits_ != music_info.bits || this->ch_ != music_info.channels) {
            i2s_stream_set_clk(i2s_stream_writer_, music_info.sample_rates, music_info.bits, music_info.channels);
            esph_log_d(TAG,"updated i2s_stream with music_info");
            this->rate_ = music_info.sample_rates;
            this->bits_ = music_info.bits;
            this->ch_ = music_info.channels;
          }
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
        if (this->state_ == AdfPipelineState::STARTING
          || this->state_ == AdfPipelineState::RESUMING
          || this->state_ == AdfPipelineState::START_ANNOUNCING) {
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
          // Http is Running
          else if (msg.source == (void *) this->http_stream_reader_
            && message_status == AEL_STATUS_STATE_RUNNING) {
            esph_log_d(TAG, "Running event received");
            if (this->state_ == AdfPipelineState::START_ANNOUNCING) {
              this->set_state_(AdfPipelineState::ANNOUNCING);
            }
            else {
              this->set_state_(AdfPipelineState::RUNNING);
            }
          }
        }

        // Http is Finished
        if (this->state_ == AdfPipelineState::RUNNING
          || this->state_ == AdfPipelineState::ANNOUNCING) {
          if (msg.source == (void *) this->http_stream_reader_
            && message_status == AEL_STATUS_STATE_FINISHED) {
            esph_log_d(TAG, "Finished event received");
            if (this->state_ == AdfPipelineState::ANNOUNCING) {
              this->set_state_(AdfPipelineState::STOP_ANNOUNCING);
            }
            else {
              this->set_state_(AdfPipelineState::STOPPING);
              this->url_ = "";
            }
          }
        }

        // i2S is Stopped or Finished
        if (this->state_ == AdfPipelineState::RUNNING
          || this->state_ == AdfPipelineState::STOPPING
          || this->state_ == AdfPipelineState::ANNOUNCING
          || this->state_ == AdfPipelineState::STOP_ANNOUNCING) {
          if (msg.source == (void *) this->i2s_stream_writer_
            && (message_status == AEL_STATUS_STATE_STOPPED
            || message_status == AEL_STATUS_STATE_FINISHED)) {
            esph_log_d(TAG, "Stop event received");            
            if (this->state_ == AdfPipelineState::ANNOUNCING
              || this->state_ == AdfPipelineState::STOP_ANNOUNCING) {
              pipeline_stop_(false,false);
              this->is_announcement_ = false;
              play();
            }
            else {
              this->stop();
            }
          }
        }
      }
    }
  }
  return AdfMediaPipeline::loop();
}

void SimpleAdfMediaPipeline::pipeline_init_() {
/******************************************************************************/
  audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
  esph_log_d(TAG, "init pipeline");
  this->pipeline_ = audio_pipeline_init(&pipeline_cfg);

/******************************************************************************/
  http_stream_cfg_t http_cfg = HTTP_STREAM_CFG_DEFAULT();
  http_cfg.out_rb_size = this->http_stream_rb_size_;
  http_cfg.task_core = this->http_stream_task_core_;
  http_cfg.task_prio = this->http_stream_task_prio_;
  http_cfg.event_handle = AdfMediaPipeline::http_stream_event_handle_;
  esph_log_d(TAG, "init http_stream_reader");
  this->http_stream_reader_ = http_stream_init(&http_cfg);

/******************************************************************************/
  audio_decoder_t auto_decode[] = {
        //DEFAULT_ESP_AMRNB_DECODER_CONFIG(),
        //DEFAULT_ESP_AMRWB_DECODER_CONFIG(),
        DEFAULT_ESP_FLAC_DECODER_CONFIG(),
        //DEFAULT_ESP_OGG_DECODER_CONFIG(),
        //DEFAULT_ESP_OPUS_DECODER_CONFIG(),
        DEFAULT_ESP_MP3_DECODER_CONFIG(),
        DEFAULT_ESP_WAV_DECODER_CONFIG(),
        //DEFAULT_ESP_AAC_DECODER_CONFIG(),
        //DEFAULT_ESP_M4A_DECODER_CONFIG(),
        //DEFAULT_ESP_TS_DECODER_CONFIG(),
  };
  esp_decoder_cfg_t auto_dec_cfg = DEFAULT_ESP_DECODER_CONFIG();
  auto_dec_cfg.out_rb_size = this->esp_decoder_rb_size_;
  auto_dec_cfg.task_core = this->esp_decoder_task_core_;
  auto_dec_cfg.task_prio = this->esp_decoder_task_prio_;
    
  esph_log_d(TAG, "init esp_decoder");
  esp_decoder_ = esp_decoder_init(&auto_dec_cfg, auto_decode, 3);

/******************************************************************************/ 
  i2s_stream_cfg_t i2s_cfg = {
    .type = AUDIO_STREAM_WRITER,
    .transmit_mode = I2S_COMM_MODE_STD,
    .chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER),
    .std_cfg = {
      .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(this->rate_),
      .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_ADF_CONFIG((i2s_data_bit_width_t)this->bits_, (i2s_slot_mode_t)this->ch_),
      .gpio_cfg = {
        .mclk = (gpio_num_t)this->mclk_pin_,
        .bclk = (gpio_num_t)this->bclk_pin_,
        .ws = (gpio_num_t)this->lrclk_pin_,
        .dout = (gpio_num_t)this->dout_pin_,
        .din = I2S_GPIO_UNUSED,
      },
    },                      
    .expand_src_bits = I2S_DATA_BIT_WIDTH_16BIT,
    .use_alc = use_adf_alc_,
    .volume = 0,
    .out_rb_size = this->i2s_stream_rb_size_,
    .task_stack = I2S_STREAM_TASK_STACK,
    .task_core = this->i2s_stream_task_core_,
    .task_prio = this->i2s_stream_task_prio_,
    .stack_in_ext = false,
    .multi_out_num = 0,
    .uninstall_drv = true,
    .need_expand = false,
    .buffer_len = I2S_STREAM_BUF_SIZE,
  };
  
  esph_log_d(TAG, "init i2s_stream");
  i2s_stream_writer_ = i2s_stream_init(&i2s_cfg);

/******************************************************************************/
  esph_log_d(TAG, "register audio elements with pipeline: http_stream_reader and esp_decoder");
  audio_pipeline_register(pipeline_, this->http_stream_reader_, "http");
  audio_pipeline_register(pipeline_, this->esp_decoder_,        "decoder");
  audio_pipeline_register(pipeline_, this->i2s_stream_writer_,  "i2s");

  esph_log_d(TAG, "Link it together http_stream-->esp_decoder-->i2s_stream-->[codec_chip]");
  const char *link_tag[3] = {"http", "decoder", "i2s"};
  audio_pipeline_link(this->pipeline_, &link_tag[0], 3);

/******************************************************************************/
  audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
  this->evt_ = audio_event_iface_init(&evt_cfg);
  esph_log_d(TAG, "set listener");
  audio_pipeline_set_listener(this->pipeline_, this->evt_);
  
  AdfMediaPipeline::pipeline_init_();
}

void SimpleAdfMediaPipeline::pipeline_deinit_() {

  esph_log_d(TAG, "terminate pipeline");
  audio_pipeline_terminate(this->pipeline_);

  esph_log_d(TAG, "unregister elements from pipeline");
  audio_pipeline_unregister(this->pipeline_, this->http_stream_reader_);
  audio_pipeline_unregister(this->pipeline_, this->i2s_stream_writer_);
  audio_pipeline_unregister(this->pipeline_, this->esp_decoder_);

  esph_log_d(TAG, "remove listener from pipeline");
  audio_pipeline_remove_listener(this->pipeline_);

  /* Make sure audio_pipeline_remove_listener & audio_event_iface_remove_listener are called before destroying event_iface */
  audio_event_iface_destroy(this->evt_);
  this->evt_ = nullptr;

  esph_log_d(TAG, "release all resources");
  audio_pipeline_deinit(this->pipeline_);
  this->pipeline_ = nullptr;
  audio_element_deinit(this->http_stream_reader_);
  this->http_stream_reader_ = nullptr;
  audio_element_deinit(this->i2s_stream_writer_);
  this->i2s_stream_writer_ = nullptr;
  audio_element_deinit(this->esp_decoder_);
  this->esp_decoder_ = nullptr;

  AdfMediaPipeline::pipeline_deinit_();
}

void SimpleAdfMediaPipeline::pipeline_play_(bool resume) {
  if (this->is_initialized_) {
    set_volume(this->volume_);
    this->is_launched_ = false;
    if (resume) {
      esph_log_d(TAG, "resume pipeline");
      audio_element_resume(this->i2s_stream_writer_, 0, 2000 / portTICK_PERIOD_MS);
      if (this->is_announcement_) {
        this->set_state_(AdfPipelineState::ANNOUNCING);
      }
      else {
        this->set_state_(AdfPipelineState::RUNNING);
      }
    }
    else {
      esph_log_d(TAG, "run pipeline");
      audio_pipeline_run(this->pipeline_);
    }
    this->is_launched_ = true;
  }
}

void SimpleAdfMediaPipeline::pipeline_stop_(bool pause, bool cleanup) {
  if (this->is_initialized_) {
    if (pause && this->is_launched_) {
      esph_log_d(TAG, "pause pipeline");
      audio_element_pause(this->i2s_stream_writer_);
      is_launched_ = false;
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

}  // namespace esp_adf
}  // namespace esphome
#endif  // USE_ESP_IDF
