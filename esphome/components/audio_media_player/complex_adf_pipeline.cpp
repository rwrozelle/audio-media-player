#include "complex_adf_pipeline.h"

#ifdef USE_ESP_IDF

#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esp_http_client.h"
#include "downmix.h"
#include "filter_resample.h"
#include "raw_stream.h"

namespace esphome {
namespace esp_adf {

static const char *const TAG = "complex_adf_media_pipeline";

void ComplexAdfMediaPipeline::dump_config() {
  esph_log_config(TAG, "ComplexAdfMediaPipeline");
  AdfMediaPipeline::dump_config();
}

void ComplexAdfMediaPipeline::set_url(const std::string& url, bool is_announcement) {
  if (is_announcement) {
    this->play_announcement_(this->get_transcode_url_(url).c_str()); 
  }
  else {
    this->url_ = this->get_transcode_url_(url);
  }
}

void ComplexAdfMediaPipeline::play(bool resume) {
  esph_log_d(TAG, "Called play with pipeline state: %s",pipeline_state_to_string(this->state_));
  if (this->url_.length() > 0 
    && ((!resume && this->state_ == AdfPipelineState::STOPPED)
      || (resume && this->state_ == AdfPipelineState::PAUSED))) {
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
      audio_element_set_uri(this->http_stream_reader_1_, this->url_.c_str());
      esph_log_d(TAG, "Play: %s", this->url_.c_str());
      this->is_music_info_set_1_ = false;
    }
    pipeline_play_(resume);
  }
}

void ComplexAdfMediaPipeline::play_announcement_(const std::string& url) {
  esph_log_d(TAG, "Called play_announcement with pipeline state: %s",pipeline_state_to_string(this->state_));
  if (url.length() > 0) { 
    if (this->state_ == AdfPipelineState::STOPPED 
      || this->state_ == AdfPipelineState::STOPPING)  {
        this->url_ = url;
        this->play();
    }
    else if (this->state_ == AdfPipelineState::STARTING
    || this->state_ == AdfPipelineState::RUNNING) {
      audio_element_set_uri(this->http_stream_reader_2_, url.c_str());
      esph_log_d(TAG, "Play Announcement: %s", url.c_str());
      this->is_music_info_set_2_ = false;
      this->pipeline_announce_(); 
    }
    else if (this->state_ == AdfPipelineState::PAUSING
    || this->state_ == AdfPipelineState::PAUSED) {
      this->pipeline_stop_(false, false);
      this->url_ = url;
      this->set_state_(AdfPipelineState::STARTING);
      if (!this->is_initialized_) {
        this->pipeline_init_();
      }
      audio_element_set_uri(this->http_stream_reader_1_, this->url_.c_str());
      esph_log_d(TAG, "Play: %s", this->url_.c_str());
      this->is_music_info_set_1_ = false;
      pipeline_play_(false);
    }
  }
}

void ComplexAdfMediaPipeline::stop(bool pause) {

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
  || this->state_ == AdfPipelineState::PAUSED) {
    if (pause) {
      this->set_state_(AdfPipelineState::PAUSING);
    }
    else {
      this->set_state_(AdfPipelineState::STOPPING);
    }
    this->pipeline_stop_(pause, cleanup);
  }
  if (pause) {
    this->set_state_(AdfPipelineState::PAUSED);
  }
  else {
    this->set_state_(AdfPipelineState::STOPPED);
  }
}


AdfPipelineState ComplexAdfMediaPipeline::loop() {

  if (this->is_launched_) {
    audio_event_iface_msg_t msg;
    esp_err_t ret = audio_event_iface_listen(this->evt_, &msg, 0);
    if (ret == ESP_OK) {
      if (!this->isServerTranscoding_()) {
        // Set Music Info pipeline 1
        if (this->is_music_info_set_1_ == false
        && msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT
        && msg.source == (void *) this->esp_decoder_1_
        && msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO) {
          audio_element_info_t music_info = {0};
          audio_element_getinfo(this->esp_decoder_1_, &music_info);

          esph_log_d(TAG, "[ decoder1 ] Receive music info, sample_rates=%d, bits=%d, ch=%d",
            music_info.sample_rates, music_info.bits, music_info.channels);

          if (this->rate_ != music_info.sample_rates || this->bits_ != music_info.bits || this->ch_ != music_info.channels) {
            rsp_filter_change_src_info(rsp_filter_1_, (int)music_info.sample_rates, (int)music_info.channels, (int)music_info.bits);         
          }
          this->is_music_info_set_1_ = true;
        }
        // Set Music Info pipeline 2
        if (this->is_music_info_set_2_ == false
        && msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT
        && msg.source == (void *) this->esp_decoder_2_
        && msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO) {
          audio_element_info_t music_info = {0};
          audio_element_getinfo(this->esp_decoder_2_, &music_info);

          esph_log_d(TAG, "[ decoder2 ] Receive music info, sample_rates=%d, bits=%d, ch=%d",
            music_info.sample_rates, music_info.bits, music_info.channels);

          if (this->rate_ != music_info.sample_rates || this->bits_ != music_info.bits || this->ch_ != music_info.channels) {
            rsp_filter_change_src_info(rsp_filter_2_, (int)music_info.sample_rates, (int)music_info.channels, (int)music_info.bits);
          }
          this->is_music_info_set_2_ = true;
        }
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
          || this->state_ == AdfPipelineState::RESUMING) {
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
          else if (msg.source == (void *) this->http_stream_reader_1_
            && message_status == AEL_STATUS_STATE_RUNNING) {
            esph_log_d(TAG, "Running event received");
            this->set_state_(AdfPipelineState::RUNNING);
          }
        }

        // Http is Finished
        if (this->state_ == AdfPipelineState::RUNNING) {
          if (msg.source == (void *) this->http_stream_reader_1_
            && message_status == AEL_STATUS_STATE_FINISHED) {
            esph_log_d(TAG, "Finished event received");
            this->set_state_(AdfPipelineState::STOPPING);
            this->url_ = "";
          }
        }

        // i2S is Stopped or Finished
        if (this->state_ == AdfPipelineState::RUNNING
          || this->state_ == AdfPipelineState::STOPPING) {
          if (msg.source == (void *) this->i2s_stream_writer_
            && (message_status == AEL_STATUS_STATE_STOPPED
              || message_status == AEL_STATUS_STATE_FINISHED)) {
            esph_log_d(TAG, "Stop event received");            
            this->stop();
          }
        }
        
        //announcement pipeline
        if (this->state_ == AdfPipelineState::RUNNING) {
          if (!this->is_announcement_ && ((this->isServerTranscoding_() && msg.source == (void *)this->esp_decoder_2_)
              || (!this->isServerTranscoding_() && msg.source == (void *)this->rsp_filter_2_))
            && (message_status == AEL_STATUS_STATE_RUNNING)) {
            esph_log_d(TAG, "Announcement start event received");            
            downmix_set_work_mode(this->downmixer_, ESP_DOWNMIX_WORK_MODE_SWITCH_ON);
            downmix_set_input_rb_timeout(this->downmixer_, 50, 1);
            this->is_announcement_ = true;
          }
          if (this->is_announcement_ && ((this->isServerTranscoding_() && msg.source == (void *)this->esp_decoder_2_)
              || (!this->isServerTranscoding_() && msg.source == (void *)this->rsp_filter_2_))
            && (message_status == AEL_STATUS_STATE_STOPPED
              || message_status == AEL_STATUS_STATE_FINISHED)) {
            esph_log_d(TAG, "Announcement Stop event received");            
            downmix_set_work_mode(this->downmixer_, ESP_DOWNMIX_WORK_MODE_SWITCH_OFF);
            downmix_set_input_rb_timeout(this->downmixer_, 0, 1);
            audio_pipeline_stop(this->pipeline_2_);
            audio_pipeline_wait_for_stop(this->pipeline_2_);
            audio_pipeline_reset_ringbuffer(this->pipeline_2_);
            audio_pipeline_reset_elements(this->pipeline_2_);
            audio_pipeline_reset_items_state(this->pipeline_2_);
            this->is_announcement_ = false;
            downmix_set_work_mode(this->downmixer_, ESP_DOWNMIX_WORK_MODE_BYPASS);
          }
        }
      }
    }
  }
  return AdfMediaPipeline::loop();
}

void ComplexAdfMediaPipeline::pipeline_init_() {
/******************************************************************************/
  audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
  esph_log_d(TAG, "init pipelines");
  if (this->isServerTranscoding_()) {
    esph_log_d(TAG, "Server Transcoding");
  }
  this->pipeline_1_ = audio_pipeline_init(&pipeline_cfg);
  this->pipeline_2_ = audio_pipeline_init(&pipeline_cfg);
  this->pipeline_3_ = audio_pipeline_init(&pipeline_cfg);

/******************************************************************************/
  esph_log_d(TAG, "init http_stream_readers");
  http_stream_cfg_t http_cfg_1 = HTTP_STREAM_CFG_DEFAULT();
  http_cfg_1.out_rb_size = this->http_stream_rb_size_;
  http_cfg_1.task_core = this->http_stream_task_core_;
  http_cfg_1.task_prio = this->http_stream_task_prio_;
  http_cfg_1.event_handle = AdfMediaPipeline::http_stream_event_handle_;
  this->http_stream_reader_1_ = http_stream_init(&http_cfg_1);
  
  http_stream_cfg_t http_cfg_2 = HTTP_STREAM_CFG_DEFAULT();
  http_cfg_2.out_rb_size = HTTP_STREAM_RINGBUFFER_SIZE;
  http_cfg_2.task_core = this->http_stream_task_core_;
  http_cfg_2.task_prio = this->http_stream_task_prio_;
  http_cfg_2.event_handle = AdfMediaPipeline::http_stream_event_handle_;
  this->http_stream_reader_2_ = http_stream_init(&http_cfg_2);

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
    
  esph_log_d(TAG, "init esp_decoders");
  esp_decoder_1_ = esp_decoder_init(&auto_dec_cfg, auto_decode, 3);
  esp_decoder_2_ = esp_decoder_init(&auto_dec_cfg, auto_decode, 3);

/******************************************************************************/
  if (!this->isServerTranscoding_()) {
    rsp_filter_cfg_t rsp_sdcard_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
    rsp_sdcard_cfg.src_rate = this->rate_;
    rsp_sdcard_cfg.src_ch = this->ch_;
    rsp_sdcard_cfg.dest_rate = this->rate_;
    rsp_sdcard_cfg.dest_ch = this->ch_;
    esph_log_d(TAG, "init resamplers");
    this->rsp_filter_1_ = rsp_filter_init(&rsp_sdcard_cfg);
    this->rsp_filter_2_ = rsp_filter_init(&rsp_sdcard_cfg);
  }
    
/******************************************************************************/
  raw_stream_cfg_t raw_cfg = RAW_STREAM_CFG_DEFAULT();
  raw_cfg.type = AUDIO_STREAM_WRITER;
  esph_log_d(TAG, "init raw writers");
  this->raw_writer_1_ = raw_stream_init(&raw_cfg);
  this->raw_writer_2_ = raw_stream_init(&raw_cfg);

/******************************************************************************/
  downmix_cfg_t downmix_cfg = {
    .downmix_info = {
        .source_num  = 2,
        .out_ctx     = ESP_DOWNMIX_OUT_CTX_LEFT_RIGHT,
        .mode        = ESP_DOWNMIX_WORK_MODE_BYPASS,
        .output_type = (esp_downmix_output_type_t)this->ch_,
    },
    .max_sample      = DM_BUF_SIZE,
    .out_rb_size     = DOWNMIX_RINGBUFFER_SIZE,
    .task_stack      = DOWNMIX_TASK_STACK,
    .task_core       = DOWNMIX_TASK_CORE,
    .task_prio       = DOWNMIX_TASK_PRIO,
    .stack_in_ext    = true,
  };
  esph_log_d(TAG, "init downmixer");
  downmixer_ = downmix_init(&downmix_cfg);
  downmix_set_input_rb_timeout(downmixer_, 0, 0);
  downmix_set_input_rb_timeout(downmixer_, 0, 1);
  esp_downmix_input_info_t source_information[2] = {0};
  esp_downmix_input_info_t source_info_1 = {
    .samplerate = (int)this->rate_,
    .channel = (int)this->ch_,
    .bits_num = (int)this->bits_,
    /* 1 music depress form 0dB to -10dB */
    .gain = {0, -15},
    .transit_time = 100,
  };
  source_information[0] = source_info_1;
  esp_downmix_input_info_t source_info_2= {
    .samplerate = (int)this->rate_,
    .channel = (int)this->ch_,
    .bits_num = (int)this->bits_,
    /* 2 music rise form -10dB to 0dB */
    .gain = {-15, 1},
    .transit_time = 100,
  };
  source_information[1] = source_info_2;
  source_info_init(downmixer_, source_information);
    
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
  esph_log_d(TAG, "register audio elements with pipeline 1");
  audio_pipeline_register(pipeline_1_, this->http_stream_reader_1_, "http1");
  audio_pipeline_register(pipeline_1_, this->esp_decoder_1_,        "decoder1"); 
  audio_pipeline_register(pipeline_1_, this->raw_writer_1_,         "raw1"); 
  if (this->isServerTranscoding_()) {
    esph_log_d(TAG, "Link it together http1-->decoder1-->raw1");
    const char *link_tag_1[3] = {"http1", "decoder1", "raw1"};
    audio_pipeline_link(this->pipeline_1_, &link_tag_1[0], 3);
  }
  else {
    audio_pipeline_register(pipeline_1_, this->rsp_filter_1_, "resampler1");
    esph_log_d(TAG, "Link it together http1-->decoder1-->resampler1-->raw1");
    const char *link_tag_1[4] = {"http1", "decoder1", "resampler1", "raw1"};
    audio_pipeline_link(this->pipeline_1_, &link_tag_1[0], 4);
  }

  esph_log_d(TAG, "register audio elements with pipeline 2");
  audio_pipeline_register(pipeline_2_, this->http_stream_reader_2_, "http2");
  audio_pipeline_register(pipeline_2_, this->esp_decoder_2_,        "decoder2");
  audio_pipeline_register(pipeline_2_, this->raw_writer_2_,         "raw2");
  if (this->isServerTranscoding_()) {
    esph_log_d(TAG, "Link it together http2-->decoder2-->raw2");
    const char *link_tag_2[3] = {"http2", "decoder2", "raw2"};
    audio_pipeline_link(this->pipeline_2_, &link_tag_2[0], 3);
  }
  else {
    audio_pipeline_register(pipeline_2_, this->rsp_filter_2_, "resampler2");
    esph_log_d(TAG, "Link it together http2-->decoder2-->resampler2-->raw2");
    const char *link_tag_2[4] = {"http2", "decoder2", "resampler2", "raw2"};
    audio_pipeline_link(this->pipeline_2_, &link_tag_2[0], 4);
  }

  esph_log_d(TAG, "register audio elements with pipeline 3:");
  audio_pipeline_register(pipeline_3_, this->downmixer_,         "downmixer");
  audio_pipeline_register(pipeline_3_, this->i2s_stream_writer_, "i2s");
  esph_log_d(TAG, "Link it together downmixer-->i2s_stream-->[codec_chip]");
  const char *link_tag_3[2] = {"downmixer", "i2s"};
  audio_pipeline_link(this->pipeline_3_, &link_tag_3[0], 2);
  
  ringbuf_handle_t rb_1 = audio_element_get_input_ringbuf(this->raw_writer_1_);
  downmix_set_input_rb(this->downmixer_, rb_1, 0);
  ringbuf_handle_t rb_2 = audio_element_get_input_ringbuf(this->raw_writer_2_);
  downmix_set_input_rb(this->downmixer_, rb_2, 1);

/******************************************************************************/
  audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
  this->evt_ = audio_event_iface_init(&evt_cfg);
  esph_log_d(TAG, "set listeners");
  audio_pipeline_set_listener(this->pipeline_1_, this->evt_);
  audio_pipeline_set_listener(this->pipeline_2_, this->evt_);
  audio_pipeline_set_listener(this->pipeline_3_, this->evt_);
  
  AdfMediaPipeline::pipeline_init_();
}

void ComplexAdfMediaPipeline::pipeline_deinit_() {

  esph_log_d(TAG, "terminate pipelines");
  audio_pipeline_terminate(this->pipeline_3_);
  audio_pipeline_terminate(this->pipeline_2_);
  audio_pipeline_terminate(this->pipeline_1_);

  esph_log_d(TAG, "unregister elements from pipeline");
  audio_pipeline_unregister(this->pipeline_3_, this->downmixer_);
  audio_pipeline_unregister(this->pipeline_3_, this->i2s_stream_writer_);

  audio_pipeline_unregister(this->pipeline_1_, this->http_stream_reader_1_);
  audio_pipeline_unregister(this->pipeline_2_, this->http_stream_reader_2_);
  audio_pipeline_unregister(this->pipeline_1_, this->esp_decoder_1_);
  audio_pipeline_unregister(this->pipeline_2_, this->esp_decoder_2_);
  
  if (!this->isServerTranscoding_()) {
    audio_pipeline_unregister(this->pipeline_1_, this->rsp_filter_1_);
    audio_pipeline_unregister(this->pipeline_2_, this->rsp_filter_2_);
  }
  audio_pipeline_unregister(this->pipeline_1_, this->raw_writer_1_);
  audio_pipeline_unregister(this->pipeline_2_, this->raw_writer_2_);

  esph_log_d(TAG, "remove listeners from pipeline");
  audio_pipeline_remove_listener(this->pipeline_3_);
  audio_pipeline_remove_listener(this->pipeline_2_);
  audio_pipeline_remove_listener(this->pipeline_1_);

  /* Make sure audio_pipeline_remove_listener & audio_event_iface_remove_listener are called before destroying event_iface */
  audio_event_iface_destroy(this->evt_);
  this->evt_ = nullptr;

  esph_log_d(TAG, "release all resources");
  audio_pipeline_deinit(this->pipeline_3_);
  this->pipeline_3_ = nullptr;
  audio_pipeline_deinit(this->pipeline_2_);
  this->pipeline_2_ = nullptr;
  audio_pipeline_deinit(this->pipeline_1_);
  this->pipeline_1_ = nullptr;
  
  audio_element_deinit(this->i2s_stream_writer_);
  this->i2s_stream_writer_ = nullptr;
  audio_element_deinit(this->downmixer_);
  this->downmixer_ = nullptr;
  
  audio_element_deinit(this->http_stream_reader_1_);
  this->http_stream_reader_1_ = nullptr;
  audio_element_deinit(this->http_stream_reader_2_);
  this->http_stream_reader_2_ = nullptr;
  audio_element_deinit(this->esp_decoder_1_);
  this->esp_decoder_1_ = nullptr;
  audio_element_deinit(this->esp_decoder_2_);
  this->esp_decoder_2_ = nullptr;
  
  if (!this->isServerTranscoding_()) {
    audio_element_deinit(this->rsp_filter_1_);
    this->rsp_filter_1_ = nullptr;
    audio_element_deinit(this->rsp_filter_2_);
    this->rsp_filter_2_ = nullptr;
  }
  audio_element_deinit(this->raw_writer_1_);
  this->raw_writer_1_ = nullptr;
  audio_element_deinit(this->raw_writer_2_);
  this->raw_writer_2_ = nullptr;

  AdfMediaPipeline::pipeline_deinit_();
}

void ComplexAdfMediaPipeline::pipeline_play_(bool resume) {
  if (this->is_initialized_) {
    set_volume(this->volume_);
    this->is_launched_ = false;
    if (resume) {
      esph_log_d(TAG, "resume pipeline");
      audio_element_resume(this->i2s_stream_writer_, 0, 2000 / portTICK_PERIOD_MS);
      this->set_state_(AdfPipelineState::RUNNING);
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

void ComplexAdfMediaPipeline::pipeline_announce_() {
  if (this->is_initialized_) {
    esph_log_d(TAG, "run announcement pipeline");
    audio_pipeline_run(this->pipeline_2_);
    this->is_announcement_ = false;
  }
}


void ComplexAdfMediaPipeline::pipeline_stop_(bool pause, bool cleanup) {
  if (this->is_initialized_) {
    if (pause && this->is_launched_) {
      esph_log_d(TAG, "pause pipeline");
      audio_element_pause(this->i2s_stream_writer_);
      is_launched_ = false;
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

}  // namespace esp_adf
}  // namespace esphome
#endif  // USE_ESP_IDF
