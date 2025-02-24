#include "simple_adf_pipeline.h"

#ifdef USE_ESP_IDF

#include "esphome/core/log.h"

namespace esphome {
namespace esp_adf {

static const char *const TAG = "simple_adf_pipeline";

const char *pipeline_state_to_string(SimpleAdfPipelineState state) {
  switch (state) {
    case SimpleAdfPipelineState::STARTING:
      return "STARTING";
    case SimpleAdfPipelineState::RUNNING:
      return "RUNNING";
    case SimpleAdfPipelineState::STOPPING:
      return "STOPPING";
    case SimpleAdfPipelineState::STOPPED:
      return "STOPPED";
    case SimpleAdfPipelineState::PAUSING:
      return "PAUSING";
    case SimpleAdfPipelineState::PAUSED:
      return "PAUSED";
    case SimpleAdfPipelineState::RESUMING:
      return "RESUMING";
    default:
      return "UNKNOWN";
  }
}

const char *audio_element_status_to_string(audio_element_status_t status) {
  switch (status) {
    case AEL_STATUS_NONE:
      return "NONE";
    case AEL_STATUS_ERROR_OPEN:
      return "ERROR_OPEN";
    case AEL_STATUS_ERROR_INPUT:
      return "ERROR_INPUT";
    case AEL_STATUS_ERROR_PROCESS:
      return "ERROR_PROCESS";
    case AEL_STATUS_ERROR_OUTPUT:
      return "ERROR_OUTPUT";
    case AEL_STATUS_ERROR_CLOSE:
      return "ERROR_CLOSE";
    case AEL_STATUS_ERROR_TIMEOUT:
      return "ERROR_TIMEOUT";
    case AEL_STATUS_ERROR_UNKNOWN:
      return "ERROR_UNKNOWN";
    case AEL_STATUS_INPUT_DONE:
      return "INPUT_DONE";
    case AEL_STATUS_INPUT_BUFFERING:
      return "INPUT_BUFFERING";
    case AEL_STATUS_STATE_RUNNING:
      return "STATE_RUNNING";
    case AEL_STATUS_STATE_PAUSED:
      return "STATE_PAUSED";
    case AEL_STATUS_STATE_STOPPED:
      return "STATE_STOPPED";
    case AEL_STATUS_STATE_FINISHED:
      return "STATE_FINISHED";
    case AEL_STATUS_MOUNTED:
      return "MOUNTED";
    case AEL_STATUS_UNMOUNTED:
      return "UNMOUNTED";
    default:
      return "UNKNOWN";
  }
}

void SimpleAdfMediaPipeline::dump_config() {
	esph_log_config(TAG, "SimpleAdfMediaPipeline");
	esph_log_config(TAG, "dout: %d",this->dout_pin_);
	esph_log_config(TAG, "mclk: %d",this->mclk_pin_);
	esph_log_config(TAG, "bclk: %d",this->bclk_pin_);
	esph_log_config(TAG, "lrclk: %d",this->lrclk_pin_);  
}

void SimpleAdfMediaPipeline::set_url(const std::string& url, bool is_announcement) {
  this->url_ = url;
}

void SimpleAdfMediaPipeline::play(bool resume) {
  esph_log_d(TAG, "Called play with pipeline state: %s",pipeline_state_to_string(this->state_));
  if (this->url_.length() > 0 && ((!resume && this->state_ == SimpleAdfPipelineState::STOPPED) || (resume && this->state_ == SimpleAdfPipelineState::PAUSED))) {
    if (resume) {
      this->set_state_(SimpleAdfPipelineState::RESUMING);
    }
    else {
      this->set_state_(SimpleAdfPipelineState::STARTING);
    }
    if (!this->is_initialized_) {
/*
      bool isLocked = true;
      for (int i = 0; i < 1000; i++) {
        if (this->parent_->try_lock()) {
          isLocked = false;
          break;
        }
      }
      if (isLocked) {
        esph_log_e(TAG, "Unable to obtain lock on i2s");
        return;
*/
      this->pipeline_init_();
    }
    
    if (!resume) {
      audio_element_set_uri(this->http_stream_reader_, this->url_.c_str());
      esph_log_d(TAG, "Play: %s", this->url_.c_str());
      this->is_music_info_set_ = false;
    }
    esph_log_d(TAG, "Launch pipeline");
    set_volume(this->volume_);
    this->is_launched_ = false;
    this->trying_to_launch_ = true;
  }
}

void SimpleAdfMediaPipeline::stop(bool pause) {

  if (pause) {
    esph_log_d(TAG, "Called pause with pipeline state: %s",pipeline_state_to_string(this->state_));
  }
  else {
    esph_log_d(TAG, "Called stop with pipeline state: %s",pipeline_state_to_string(this->state_));
  }
  bool cleanup = false;
  if (!pause && this->state_ == SimpleAdfPipelineState::PAUSED) {
    cleanup = true;
  }

  if (this->state_ == SimpleAdfPipelineState::STARTING
  || this->state_ == SimpleAdfPipelineState::RUNNING
  || this->state_ == SimpleAdfPipelineState::STOPPING
  || this->state_ == SimpleAdfPipelineState::PAUSING
  || this->state_ == SimpleAdfPipelineState::PAUSED) {
    if (pause) {
      this->set_state_(SimpleAdfPipelineState::PAUSING);
    }
    else {
      this->set_state_(SimpleAdfPipelineState::STOPPING);
    }
    if (this->is_initialized_) {
      if (pause && this->is_launched_) {
        esph_log_d(TAG, "pause pipeline");
        //audio_pipeline_pause(this->pipeline_);
        //audio_pipeline_wait_for_stop(pipeline_);
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
      //audio_pipeline_change_state(this->pipeline_, AEL_STATE_INIT);
    }
    else if (this->is_initialized_ && pause) {
      //esph_log_d(TAG, "reset ring buffers");
      //audio_pipeline_reset_ringbuffer(this->pipeline_);
    }
    
    if (cleanup) {
      this->clean_up();
    }
  }
  if (pause) {
    this->set_state_(SimpleAdfPipelineState::PAUSED);
  }
  else {
    this->set_state_(SimpleAdfPipelineState::STOPPED);
  }
}

void SimpleAdfMediaPipeline::clean_up() {
  if (this->is_initialized_) {
    this->pipeline_deinit_();
  }
}

void SimpleAdfMediaPipeline::set_volume(int volume) {

    //input volume is 0 to 100
    if (volume > 100)
      volume = 100;
    else if (volume < 0)
      volume = 0;

    this->volume_ = volume;
  if (this->state_ == SimpleAdfPipelineState::RUNNING || this->state_ == SimpleAdfPipelineState::STARTING) {
    if (this->use_adf_alc_) {
      //use -64 to 36
      int target_volume = volume - 64;
        esph_log_v(TAG, "attempt setting volume to %d", target_volume);
      if (i2s_alc_volume_set(this->i2s_stream_writer_, target_volume) != ESP_OK) {
        esph_log_e(TAG, "error setting volume to %d", target_volume);
      }
    }
  }
}

void SimpleAdfMediaPipeline::mute() {
  if (this->state_ == SimpleAdfPipelineState::RUNNING) {
    if (this->use_adf_alc_) {
        esph_log_v(TAG, "attempt seting mute");
      if (i2s_alc_volume_set(this->i2s_stream_writer_, -64) != ESP_OK) {
        esph_log_e(TAG, "error seting mute");
      }
    }
  }
}

void SimpleAdfMediaPipeline::unmute() {
  if (this->state_ == SimpleAdfPipelineState::RUNNING) {
    if (this->use_adf_alc_) {
      int target_volume = volume_ - 64;
        esph_log_v(TAG, "attempt unmute to %d", target_volume);
      if (i2s_alc_volume_set(this->i2s_stream_writer_, target_volume) != ESP_OK) {
        esph_log_e(TAG, "error setting unmute to volume to %d", target_volume);
      }
    }
  }
}

SimpleAdfPipelineState SimpleAdfMediaPipeline::loop() {

  if (this->trying_to_launch_ && !this->is_launched_) {
    this->pipeline_run_();
  }
  else if (this->is_launched_) {
    audio_event_iface_msg_t msg;
    esp_err_t ret = audio_event_iface_listen(this->evt_, &msg, 0);
    if (ret == ESP_OK) {
      
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
        audio_element_status_t status;
        std::memcpy(&status, &msg.data, sizeof(audio_element_status_t));
        audio_element_handle_t el = (audio_element_handle_t) msg.source;
        esph_log_d(TAG, "[ %s ] status: %s, pipeline state: %s", audio_element_get_tag(el), audio_element_status_to_string(status), pipeline_state_to_string(this->state_));
        int message_status = (int)msg.data;
      
        if (this->state_ == SimpleAdfPipelineState::STARTING || this->state_ == SimpleAdfPipelineState::RESUMING) {
          if (message_status == AEL_STATUS_ERROR_OPEN
            || message_status == AEL_STATUS_ERROR_INPUT
            || message_status == AEL_STATUS_ERROR_PROCESS
            || message_status == AEL_STATUS_ERROR_OUTPUT
            || message_status == AEL_STATUS_ERROR_CLOSE
            || message_status == AEL_STATUS_ERROR_TIMEOUT
            || message_status == AEL_STATUS_ERROR_UNKNOWN) {
            esph_log_e(TAG, "Error received, stopping");
            stop();
            this->clean_up();
          }
          else if (msg.source == (void *) this->http_stream_reader_
            && message_status == AEL_STATUS_STATE_RUNNING) {
            esph_log_d(TAG, "Running event received");
            this->set_state_(SimpleAdfPipelineState::RUNNING);
          }
        }
        
        if (this->state_ == SimpleAdfPipelineState::RUNNING) {
          if (msg.source == (void *) this->http_stream_reader_
            && message_status == AEL_STATUS_STATE_FINISHED) {
            esph_log_d(TAG, "Finished event received");
            this->set_state_(SimpleAdfPipelineState::STOPPING);
            this->url_ = "";
          }
        }
        
        if (this->state_ == SimpleAdfPipelineState::RUNNING || this->state_ == SimpleAdfPipelineState::STOPPING) {
          if (msg.source == (void *) this->i2s_stream_writer_
            && (message_status == AEL_STATUS_STATE_STOPPED
            || message_status == AEL_STATUS_STATE_FINISHED)) {
            esph_log_d(TAG, "Stop event received");
            this->stop();
          }
        }
      }
    }
  }
  return state_;
}

void SimpleAdfMediaPipeline::pipeline_init_() {
/******************************************************************************/
  audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
  esph_log_d(TAG, "init pipeline");
  this->pipeline_ = audio_pipeline_init(&pipeline_cfg);

/******************************************************************************/
  http_stream_cfg_t http_cfg = HTTP_STREAM_CFG_DEFAULT();
  http_cfg.out_rb_size = this->http_stream_rb_size;
  http_cfg.task_core = this->http_stream_task_core;
  http_cfg.task_prio = this->http_stream_task_prio;
  esph_log_d(TAG, "init http_stream_reader");
  this->http_stream_reader_ = http_stream_init(&http_cfg);

/******************************************************************************/
  audio_decoder_t auto_decode[] = {
        DEFAULT_ESP_AMRNB_DECODER_CONFIG(),
        DEFAULT_ESP_AMRWB_DECODER_CONFIG(),
        DEFAULT_ESP_FLAC_DECODER_CONFIG(),
        DEFAULT_ESP_OGG_DECODER_CONFIG(),
        DEFAULT_ESP_OPUS_DECODER_CONFIG(),
        DEFAULT_ESP_MP3_DECODER_CONFIG(),
        DEFAULT_ESP_WAV_DECODER_CONFIG(),
        DEFAULT_ESP_AAC_DECODER_CONFIG(),
        DEFAULT_ESP_M4A_DECODER_CONFIG(),
        DEFAULT_ESP_TS_DECODER_CONFIG(),
  };
  esp_decoder_cfg_t auto_dec_cfg = DEFAULT_ESP_DECODER_CONFIG();
  auto_dec_cfg.out_rb_size = this->esp_decoder_rb_size;
  auto_dec_cfg.task_core = this->esp_decoder_task_core;
  auto_dec_cfg.task_prio = this->esp_decoder_task_prio;
    
  esph_log_d(TAG, "init esp_decoder");
  esp_decoder_ = esp_decoder_init(&auto_dec_cfg, auto_decode, 10);

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
    .out_rb_size = this->i2s_stream_rb_size,
    .task_stack = I2S_STREAM_TASK_STACK,
    .task_core = this->i2s_stream_task_core,
    .task_prio = this->i2s_stream_task_prio,
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
  is_initialized_ = true;
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
  evt_ = nullptr;

  esph_log_d(TAG, "release all resources");
  audio_pipeline_deinit(this->pipeline_);
  this->pipeline_ = nullptr;
  audio_element_deinit(this->http_stream_reader_);
  this->http_stream_reader_ = nullptr;
  audio_element_deinit(this->i2s_stream_writer_);
  this->i2s_stream_writer_ = nullptr;
  audio_element_deinit(this->esp_decoder_);
  this->esp_decoder_ = nullptr;
  //parent_->unlock();
  this->is_initialized_ = false;
}

int64_t SimpleAdfMediaPipeline::get_timestamp_() {
  struct timeval tv_now;
  gettimeofday(&tv_now, NULL);
  return (int64_t)tv_now.tv_sec * 1000000L + (int64_t)tv_now.tv_usec;
}

void SimpleAdfMediaPipeline::pipeline_run_() {
  int64_t timestamp = this->get_timestamp_();
  if (this->trying_to_launch_ && timestamp > this->launch_timestamp_) {
    this->trying_to_launch_ = false;
    if (this->launch_timestamp_ > 0) {
      esph_log_d(TAG,"Pipeline Run, requested launch time: %lld, launch time: %lld",this->launch_timestamp_, timestamp);
    }
    if (this->state_ == SimpleAdfPipelineState::RESUMING) {
      //audio_pipeline_resume(pipeline_);
      audio_element_resume(this->i2s_stream_writer_, 0, 2000 / portTICK_PERIOD_MS);
      this->set_state_(SimpleAdfPipelineState::RUNNING);
    }
    else {
      audio_pipeline_run(this->pipeline_);
    }
      
    this->is_launched_ = true;
    this->launch_timestamp_ = 0;
  }
}

void SimpleAdfMediaPipeline::set_state_(SimpleAdfPipelineState state) {
    esph_log_d(TAG, "Updated pipeline state: %s",pipeline_state_to_string(state));
  this->state_ = state;
}

}  // namespace esp_adf
}  // namespace esphome

#endif  // USE_ESP_IDF
