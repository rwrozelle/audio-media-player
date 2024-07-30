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
#if SOC_I2S_SUPPORTS_DAC
  if (this->internal_dac_mode_ != I2S_DAC_CHANNEL_DISABLE) {
    switch (this->internal_dac_mode_) {
      case I2S_DAC_CHANNEL_LEFT_EN:
        esph_log_config(TAG, "  Internal DAC mode: Left");
        break;
      case I2S_DAC_CHANNEL_RIGHT_EN:
        esph_log_config(TAG, "  Internal DAC mode: Right");
        break;
      case I2S_DAC_CHANNEL_BOTH_EN:
        esph_log_config(TAG, "  Internal DAC mode: Left & Right");
        break;
      default:
        break;
    }
  } 
  else {
#endif
    esph_log_config(TAG, "  External DAC channels: %d", this->external_dac_channels_);
    esph_log_config(TAG, "  I2S DOUT Pin: %d", this->dout_pin_);
    i2s_pin_config_t pin_config = parent_->get_pin_config();
    esph_log_config(TAG, "  I2S MCLK Pin: %d", pin_config.mck_io_num);
    esph_log_config(TAG, "  I2S BCLK Pin: %d", pin_config.bck_io_num);
    esph_log_config(TAG, "  I2S LRCLK Pin: %d", pin_config.ws_io_num);
    esph_log_config(TAG, "  I2S Port: %d", parent_->get_port());
#if SOC_I2S_SUPPORTS_TDM
    esph_log_config(TAG, "  SOC I2S Supports TDM");
#endif
#if SOC_I2S_SUPPORTS_DAC
  }
#endif
}

void SimpleAdfMediaPipeline::set_url(const std::string& url, bool is_announcement) {
  this->url_ = url;
}

void SimpleAdfMediaPipeline::play(bool resume) {
  esph_log_d(TAG, "Called play with pipeline state: %s",pipeline_state_to_string(state_));
  if (url_.length() > 0 && ((!resume && state_ == SimpleAdfPipelineState::STOPPED) || (resume && state_ == SimpleAdfPipelineState::PAUSED))) {
    if (resume) {
      set_state_(SimpleAdfPipelineState::RESUMING);
    }
    else {
      set_state_(SimpleAdfPipelineState::STARTING);
    }
    if (!is_initialized_) {
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
      }
      pipeline_init_();
    }
    
    if (!resume) {
      audio_element_set_uri(http_stream_reader_, url_.c_str());
      esph_log_d(TAG, "Play: %s", url_.c_str());
      is_music_info_set_ = false;
    }
    esph_log_d(TAG, "Launch pipeline");
    set_volume(volume_);
    is_launched_ = false;
    trying_to_launch_ = true;
  }
}

void SimpleAdfMediaPipeline::stop(bool pause) {

  if (pause) {
    esph_log_d(TAG, "Called pause with pipeline state: %s",pipeline_state_to_string(state_));
  }
  else {
    esph_log_d(TAG, "Called stop with pipeline state: %s",pipeline_state_to_string(state_));
  }
  if (!pause && state_ == SimpleAdfPipelineState::PAUSED) {
    clean_up();
  }

  if (state_ == SimpleAdfPipelineState::STARTING
  || state_ == SimpleAdfPipelineState::RUNNING
  || state_ == SimpleAdfPipelineState::STOPPING
  || state_ == SimpleAdfPipelineState::PAUSING
  || state_ == SimpleAdfPipelineState::PAUSED) {
    if (pause) {
      set_state_(SimpleAdfPipelineState::PAUSING);
    }
    else {
      set_state_(SimpleAdfPipelineState::STOPPING);
    }
    if (is_initialized_) {
      if (pause && is_launched_) {
        esph_log_d(TAG, "pause pipeline");
        //audio_pipeline_pause(pipeline_);
        //audio_pipeline_wait_for_stop(pipeline_);
        audio_element_pause(i2s_stream_writer_);
        is_launched_ = false;
      }
      else if (!pause) {
        esph_log_d(TAG, "stop pipeline");
        audio_pipeline_stop(pipeline_);
        audio_pipeline_wait_for_stop(pipeline_);
        is_launched_ = false;
      }
    }
    if (is_initialized_ && !pause) {
      esph_log_d(TAG, "reset pipeline");
      audio_pipeline_reset_ringbuffer(pipeline_);
      audio_pipeline_reset_elements(pipeline_);
      audio_pipeline_reset_items_state(pipeline_);
      //audio_pipeline_change_state(pipeline_, AEL_STATE_INIT);
    }
    else if (is_initialized_ && pause) {
      //esph_log_d(TAG, "reset ring buffers");
      //audio_pipeline_reset_ringbuffer(pipeline_);
    }
  }
  if (pause) {
    set_state_(SimpleAdfPipelineState::PAUSED);
  }
  else {
    set_state_(SimpleAdfPipelineState::STOPPED);
  }
}

void SimpleAdfMediaPipeline::clean_up() {
  if (is_initialized_) {
    pipeline_deinit_();
  }
}

void SimpleAdfMediaPipeline::set_volume(int volume) {

    //input volume is 0 to 100
    if (volume > 100)
      volume = 100;
    else if (volume < 0)
      volume = 0;

    volume_ = volume;
  if (state_ == SimpleAdfPipelineState::RUNNING || state_ == SimpleAdfPipelineState::STARTING) {
    if (this->use_adf_alc_) {
      //use -50 to 50
      int target_volume = volume - 50;
      if (i2s_alc_volume_set(this->i2s_stream_writer_, target_volume) != ESP_OK) {
        esph_log_e(TAG, "error setting volume to %d", target_volume);
      }
    }
  }
}

void SimpleAdfMediaPipeline::mute() {
  if (state_ == SimpleAdfPipelineState::RUNNING) {
    if (this->use_adf_alc_) {
      int target_volume = -64;
      if (i2s_alc_volume_set(this->i2s_stream_writer_, -64) != ESP_OK) {
        esph_log_e(TAG, "error seting mute");
      }
    }
  }
}

void SimpleAdfMediaPipeline::unmute() {
  if (state_ == SimpleAdfPipelineState::RUNNING) {
    if (this->use_adf_alc_) {
      //transform to range: -64 (muted) to 64 (super noisy)
      int target_volume = round((volume_ * 1.28) - 64);
      if (i2s_alc_volume_set(this->i2s_stream_writer_, target_volume) != ESP_OK) {
        esph_log_e(TAG, "error setting unmute to volume to %d", target_volume);
      }
    }
  }
}

SimpleAdfPipelineState SimpleAdfMediaPipeline::loop() {

  if (trying_to_launch_ && !is_launched_) {
    pipeline_run_();
  }
  else if (is_launched_) {
    audio_event_iface_msg_t msg;
    esp_err_t ret = audio_event_iface_listen(evt_, &msg, 0);
    if (ret == ESP_OK) {
      
      if (is_music_info_set_ == false
        && msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT
        && msg.source == (void *) esp_decoder_
        && msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO) {
        audio_element_info_t music_info = {0};
        audio_element_getinfo(esp_decoder_, &music_info);

        esph_log_d(TAG, "[ decoder ] Receive music info, sample_rates=%d, bits=%d, ch=%d",
                music_info.sample_rates, music_info.bits, music_info.channels);

        i2s_stream_set_clk(i2s_stream_writer_, music_info.sample_rates, music_info.bits, music_info.channels);
        esph_log_d(TAG,"updated i2s_stream with music_info");
        is_music_info_set_ = true;
        }
            
      if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.cmd == AEL_MSG_CMD_REPORT_STATUS) {
        audio_element_status_t status;
        std::memcpy(&status, &msg.data, sizeof(audio_element_status_t));
        audio_element_handle_t el = (audio_element_handle_t) msg.source;
        esph_log_i(TAG, "[ %s ] status: %s, pipeline state: %s", audio_element_get_tag(el), audio_element_status_to_string(status), pipeline_state_to_string(state_));
        int message_status = (int)msg.data;
      
        if (state_ == SimpleAdfPipelineState::STARTING || state_ == SimpleAdfPipelineState::RESUMING) {
          if (message_status == AEL_STATUS_ERROR_OPEN
            || message_status == AEL_STATUS_ERROR_INPUT
            || message_status == AEL_STATUS_ERROR_PROCESS
            || message_status == AEL_STATUS_ERROR_OUTPUT
            || message_status == AEL_STATUS_ERROR_CLOSE
            || message_status == AEL_STATUS_ERROR_TIMEOUT
            || message_status == AEL_STATUS_ERROR_UNKNOWN) {
            esph_log_e(TAG, "Error received, stopping");
            stop();
            clean_up();
          }
          else if (msg.source == (void *) http_stream_reader_
            && message_status == AEL_STATUS_STATE_RUNNING) {
            esph_log_d(TAG, "Running event received");
            set_state_(SimpleAdfPipelineState::RUNNING);
          }
        }
        
        if (state_ == SimpleAdfPipelineState::RUNNING) {
          if (msg.source == (void *) http_stream_reader_
            && message_status == AEL_STATUS_STATE_FINISHED) {
            esph_log_d(TAG, "Finished event received");
            set_state_(SimpleAdfPipelineState::STOPPING);
            url_ = "";
          }
        }
        
        if (state_ == SimpleAdfPipelineState::RUNNING || state_ == SimpleAdfPipelineState::STOPPING) {
          if (msg.source == (void *) i2s_stream_writer_
            && (message_status == AEL_STATUS_STATE_STOPPED
            || message_status == AEL_STATUS_STATE_FINISHED)) {
            esph_log_d(TAG, "Stop event received");
            stop();
          }
        }
      }
    }
  }
  return state_;
}

void SimpleAdfMediaPipeline::pipeline_init_() {

  audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
  esph_log_d(TAG, "init pipeline");
  pipeline_ = audio_pipeline_init(&pipeline_cfg);
  
  http_stream_cfg_t http_cfg = HTTP_STREAM_CFG_DEFAULT();
  http_cfg.out_rb_size = http_stream_rb_size;
  http_cfg.task_core = http_stream_task_core;
  http_cfg.task_prio = http_stream_task_prio;
  esph_log_d(TAG, "init http_stream_reader");
  http_stream_reader_ = http_stream_init(&http_cfg);

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
    auto_dec_cfg.out_rb_size = esp_decoder_rb_size;
    auto_dec_cfg.task_core = esp_decoder_task_core;
    auto_dec_cfg.task_prio = esp_decoder_task_prio;
    
  esph_log_d(TAG, "init esp_decoder");
    esp_decoder_ = esp_decoder_init(&auto_dec_cfg, auto_decode, 10);
    
  i2s_comm_format_t comm_fmt = (i2s_comm_fmt_lsb_ ? I2S_COMM_FORMAT_STAND_I2S : I2S_COMM_FORMAT_STAND_MSB );  
  i2s_driver_config_t i2s_driver_config = {
      .mode = (i2s_mode_t) (I2S_MODE_MASTER | I2S_MODE_TX),
      .sample_rate = 44100,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
      .communication_format = comm_fmt,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
       //.intr_alloc_flags = ESP_INTR_FLAG_LEVEL2 | ESP_INTR_FLAG_IRAM,
      .dma_buf_count = 4,
      .dma_buf_len = 256,
      .use_apll = false,
      .tx_desc_auto_clear = true,
      .fixed_mclk = I2S_PIN_NO_CHANGE,
      .mclk_multiple = I2S_MCLK_MULTIPLE_DEFAULT,
      .bits_per_chan = I2S_BITS_PER_CHAN_DEFAULT,
#if SOC_I2S_SUPPORTS_TDM
      .chan_mask = I2S_CHANNEL_MONO,
      .total_chan = 0,
      .left_align = false,
      .big_edin = false,
      .bit_order_msb = false,
      .skip_msk = false,
#endif
  };
#if SOC_I2S_SUPPORTS_DAC
  if (internal_dac_mode_ != I2S_DAC_CHANNEL_DISABLE) {
    config.mode = (i2s_mode_t) (config.mode | I2S_MODE_DAC_BUILT_IN);
  }
#endif

  i2s_stream_cfg_t i2s_cfg = {
      .type = AUDIO_STREAM_WRITER,
      .i2s_config = i2s_driver_config,
      .i2s_port = parent_->get_port(),
      .use_alc = use_adf_alc_,
      .volume = 0,
      .out_rb_size = i2s_stream_rb_size,
      .task_stack = I2S_STREAM_TASK_STACK,
      .task_core = i2s_stream_task_core,
      .task_prio = i2s_stream_task_prio,
      .stack_in_ext = false,
      .multi_out_num = 0,
      .uninstall_drv = false,
      .need_expand = false,
      .expand_src_bits = I2S_BITS_PER_SAMPLE_16BIT,
      .buffer_len = I2S_STREAM_BUF_SIZE,
  };

  esph_log_d(TAG, "init i2s_stream_writer");
  i2s_stream_writer_ = i2s_stream_init(&i2s_cfg);
  
    esph_log_d(TAG, "install i2s pins");
#if SOC_I2S_SUPPORTS_DAC
  if (internal_dac_mode_ == I2S_DAC_CHANNEL_DISABLE) {
#endif
    i2s_pin_config_t pin_config = parent_->get_pin_config();
    pin_config.data_out_num = dout_pin_;
    i2s_set_pin(parent_->get_port(), &pin_config);
#if SOC_I2S_SUPPORTS_DAC
  } else {
    i2s_set_dac_mode(internal_dac_mode_);
  }
#endif

  esph_log_d(TAG, "register audio elements with pipeline: http_stream_reader and esp_decoder");
  audio_pipeline_register(pipeline_, http_stream_reader_, "http");
  audio_pipeline_register(pipeline_, esp_decoder_,        "decoder");
  audio_pipeline_register(pipeline_, i2s_stream_writer_,  "i2s");

  esph_log_d(TAG, "Link it together http_stream-->esp_decoder-->i2s_stream-->[codec_chip]");
  const char *link_tag[3] = {"http", "decoder", "i2s"};
  audio_pipeline_link(pipeline_, &link_tag[0], 3);
  
  audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
  evt_ = audio_event_iface_init(&evt_cfg);
  esph_log_d(TAG, "set listener");
  audio_pipeline_set_listener(pipeline_, evt_);
  is_initialized_ = true;
}

void SimpleAdfMediaPipeline::pipeline_deinit_() {

  esph_log_d(TAG, "terminate pipeline");
  audio_pipeline_terminate(pipeline_);

  esph_log_d(TAG, "unregister elements from pipeline");
  audio_pipeline_unregister(pipeline_, http_stream_reader_);
  audio_pipeline_unregister(pipeline_, i2s_stream_writer_);
  audio_pipeline_unregister(pipeline_, esp_decoder_);

  esph_log_d(TAG, "remove listener from pipeline");
  audio_pipeline_remove_listener(pipeline_);

  /* Make sure audio_pipeline_remove_listener & audio_event_iface_remove_listener are called before destroying event_iface */
  audio_event_iface_destroy(evt_);
  evt_ = nullptr;

  esph_log_d(TAG, "release all resources");
  audio_pipeline_deinit(pipeline_);
  pipeline_ = nullptr;
  audio_element_deinit(http_stream_reader_);
  http_stream_reader_ = nullptr;
  uninstall_i2s_driver_();
  audio_element_deinit(i2s_stream_writer_);
  i2s_stream_writer_ = nullptr;
  audio_element_deinit(esp_decoder_);
  esp_decoder_ = nullptr;
  parent_->unlock();
  is_initialized_ = false;
}

int64_t SimpleAdfMediaPipeline::get_timestamp_() {
  struct timeval tv_now;
  gettimeofday(&tv_now, NULL);
  return (int64_t)tv_now.tv_sec * 1000000L + (int64_t)tv_now.tv_usec;
}

void SimpleAdfMediaPipeline::pipeline_run_() {
  int64_t timestamp = get_timestamp_();
  if (trying_to_launch_ && timestamp > launch_timestamp_) {
    trying_to_launch_ = false;
    if (launch_timestamp_ > 0) {
      esph_log_d(TAG,"Pipeline Run, requested launch time: %lld, launch time: %lld",launch_timestamp_, timestamp);
    }
    if (state_ == SimpleAdfPipelineState::RESUMING) {
      //audio_pipeline_resume(pipeline_);
      audio_element_resume(i2s_stream_writer_, 0, 2000 / portTICK_RATE_MS);
      set_state_(SimpleAdfPipelineState::RUNNING);
    }
    else {
      audio_pipeline_run(pipeline_);
    }
      
    is_launched_ = true;
    launch_timestamp_ = 0;
  }
}

void SimpleAdfMediaPipeline::set_state_(SimpleAdfPipelineState state) {
    esph_log_d(TAG, "Updated pipeline state: %s",pipeline_state_to_string(state));
  state_ = state;
}

bool SimpleAdfMediaPipeline::uninstall_i2s_driver_() {

  bool success = false;
  i2s_zero_dma_buffer(parent_->get_port());
  esp_err_t err = i2s_driver_uninstall(parent_->get_port());
  if (err == ESP_OK) {
    success = true;
  } else {
      esph_log_e(TAG, "Couldn't unload i2s_driver");
  }
  return success;
}

}  // namespace esp_adf
}  // namespace esphome

#endif  // USE_ESP_IDF
