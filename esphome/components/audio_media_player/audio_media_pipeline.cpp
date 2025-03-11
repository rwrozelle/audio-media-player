#include "audio_media_pipeline.h"

#ifdef USE_ESP_IDF

#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace esp_audio {

static const char *const TAG = "audio_media_pipeline";

const char *pipeline_state_to_string(AudioMediaPipelineState state) {
  switch (state) {
    case AudioMediaPipelineState::STARTING:
      return "STARTING";
    case AudioMediaPipelineState::RUNNING:
      return "RUNNING";
    case AudioMediaPipelineState::STOPPING:
      return "STOPPING";
    case AudioMediaPipelineState::STOPPED:
      return "STOPPED";
    case AudioMediaPipelineState::PAUSING:
      return "PAUSING";
    case AudioMediaPipelineState::PAUSED:
      return "PAUSED";
    case AudioMediaPipelineState::RESUMING:
      return "RESUMING";
    case AudioMediaPipelineState::STARTING_ANNOUNCING:
      return "STARTING_ANNOUNCING";
    case AudioMediaPipelineState::ANNOUNCING:
      return "ANNOUNCING";
    case AudioMediaPipelineState::STOPPING_ANNOUNCING:
      return "STOPPING_ANNOUNCING";
    case AudioMediaPipelineState::STOPPED_ANNOUNCING:
      return "STOPPED_ANNOUNCING";
    default:
      return "UNKNOWN";
  }
}
const char *audio_media_input_type_to_string(AudioMediaInputType type) {
  switch (type) {
    case AudioMediaInputType::URL:
      return "URL";
    case AudioMediaInputType::FLASH:
      return "FLASH";
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

std::string AudioMediaPipeline::access_token = "";

void AudioMediaPipeline::dump_config() {
  esph_log_config(TAG, "dout: %d",this->dout_pin_);
  esph_log_config(TAG, "mclk: %d",this->mclk_pin_);
  esph_log_config(TAG, "bclk: %d",this->bclk_pin_);
  esph_log_config(TAG, "lrclk: %d",this->lrclk_pin_);
  if (this->isServerTranscoding_()) {
    esph_log_config(TAG, "transcode server: %s",this->ffmpeg_server_.c_str());
    esph_log_config(TAG, "transcode target format: %s",this->format_.c_str());
  }
  esph_log_config(TAG, "target rate: %d",this->rate_);
  esph_log_config(TAG, "target bits: %d",this->bits_);
  esph_log_config(TAG, "target channels: %d",this->ch_);
  
  esph_log_config(TAG, "http stream rb size: %d",this->http_stream_rb_size_);
  esph_log_config(TAG, "http stream task core: %d",this->http_stream_task_core_);
  esph_log_config(TAG, "http stream task prio: %d",this->http_stream_task_prio_);
  esph_log_config(TAG, "esp decoder rb size: %d",this->esp_decoder_rb_size_);
  esph_log_config(TAG, "esp decoder task core: %d",this->esp_decoder_task_core_);
  esph_log_config(TAG, "esp decoder task prio: %d",this->esp_decoder_task_prio_);
  esph_log_config(TAG, "i2s stream rb size: %d",this->i2s_stream_rb_size_);
  esph_log_config(TAG, "esp decoder task core: %d",this->i2s_stream_task_core_);
  esph_log_config(TAG, "esp decoder task prio: %d",this->i2s_stream_task_prio_);
  esph_log_config(TAG, "use adf alc: %d",this->use_adf_alc_);

#ifdef USE_AMRNB_DECODER
  esph_log_config(TAG, "Use AMRNB Decoder");
#endif
#ifdef USE_AMRWB_DECODER
  esph_log_config(TAG, "Use AMRWB Decoder");
#endif
#ifdef USE_FLAC_DECODER
  esph_log_config(TAG, "Use FLAC Decoder");
#endif
#ifdef USE_OGG_DECODER
  esph_log_config(TAG, "Use OGG Decoder");
#endif
#ifdef USE_OPUS_DECODER
  esph_log_config(TAG, "Use OPUS Decoder");
#endif
#ifdef USE_MP3_DECODER
  esph_log_config(TAG, "Use MP3 Decoder");
#endif
#ifdef USE_WAV_DECODER
  esph_log_config(TAG, "Use WAV Decoder");
#endif
#ifdef USE_AAC_DECODER
  esph_log_config(TAG, "Use AAC Decoder");
#endif
#ifdef USE_M4A_DECODER
  esph_log_config(TAG, "Use M4A Decoder");
#endif
#ifdef USE_TS_DECODER
  esph_log_config(TAG, "Use TS Decoder");
#endif  
}

void AudioMediaPipeline::set_url(const std::string& url, bool is_announcement, media_player::MediaPlayerEnqueue enqueue) {
  esph_log_d(TAG, "set_url %d %d %s", url.length(), is_announcement, media_player::media_player_enqueue_to_string(enqueue));
  if (url.length() > 0) {
    this->is_announcement_ = is_announcement;
    if (is_announcement) {
      AudioAnouncement announcement;
      announcement.url = url;
      if (enqueue == media_player::MEDIA_PLAYER_ENQUEUE_PLAY || enqueue == media_player::MEDIA_PLAYER_ENQUEUE_ADD) {
        this->announcements_.push_back(announcement);
      }
      else if (enqueue == media_player::MEDIA_PLAYER_ENQUEUE_NEXT) {
        this->announcements_.push_front(announcement);
      }
      else if (enqueue == media_player::MEDIA_PLAYER_ENQUEUE_REPLACE) {
        this->announcements_.clear();
        this->announcements_.push_front(announcement);
      }
    }
    else {
      this->url_ = url;
    }
  }
}

void AudioMediaPipeline::play_file(audio::AudioFile *media_file, media_player::MediaPlayerEnqueue enqueue) {
  esph_log_d(TAG, "play_file %s", media_player::media_player_enqueue_to_string(enqueue));
  this->is_announcement_ = true;
  AudioAnouncement announcement;
  announcement.file = media_file;
  if (enqueue == media_player::MEDIA_PLAYER_ENQUEUE_PLAY || enqueue == media_player::MEDIA_PLAYER_ENQUEUE_ADD) {
    this->announcements_.push_back(announcement);
  }
  else if (enqueue == media_player::MEDIA_PLAYER_ENQUEUE_NEXT) {
    this->announcements_.push_front(announcement);
  }
  else if (enqueue == media_player::MEDIA_PLAYER_ENQUEUE_REPLACE) {
    this->announcements_.clear();
    this->announcements_.push_front(announcement);
  }
}

void AudioMediaPipeline::play(bool resume) {
}

void AudioMediaPipeline::stop(bool pause) {
}

void AudioMediaPipeline::clean_up() {
  esph_log_d(TAG, "cleanup %d", is_initialized_);
  if (this->is_initialized_) {
    this->pipeline_deinit_();
  }
}

void AudioMediaPipeline::set_volume(int volume) {

  //input volume is 0 to 100
  if (volume > 100)
    volume = 100;
  else if (volume < 0)
    volume = 0;

  this->volume_ = volume;
  if (this->state_ == AudioMediaPipelineState::RUNNING
  || this->state_ == AudioMediaPipelineState::STARTING
  || this->state_ == AudioMediaPipelineState::STARTING_ANNOUNCING
  || this->state_ == AudioMediaPipelineState::ANNOUNCING) {
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

void AudioMediaPipeline::mute() {
  if (this->state_ == AudioMediaPipelineState::RUNNING) {
    if (this->use_adf_alc_) {
        esph_log_v(TAG, "attempt seting mute");
      if (i2s_alc_volume_set(this->i2s_stream_writer_, -64) != ESP_OK) {
        esph_log_e(TAG, "error seting mute");
      }
    }
  }
}

void AudioMediaPipeline::unmute() {
  if (this->state_ == AudioMediaPipelineState::RUNNING) {
    if (this->use_adf_alc_) {
      int target_volume = volume_ - 64;
        esph_log_v(TAG, "attempt unmute to %d", target_volume);
      if (i2s_alc_volume_set(this->i2s_stream_writer_, target_volume) != ESP_OK) {
        esph_log_e(TAG, "error setting unmute to volume to %d", target_volume);
      }
    }
  }
}

AudioMediaPipelineState AudioMediaPipeline::loop() {
  return this->state_;
}

void AudioMediaPipeline::pipeline_init_() {
  this->is_initialized_ = true;
}

void AudioMediaPipeline::pipeline_deinit_() {
  this->is_initialized_ = false;
}

void AudioMediaPipeline::pipeline_play_(bool resume) {
}

void AudioMediaPipeline::pipeline_stop_(bool pause, bool cleanup) {
}

void AudioMediaPipeline::play_announcement_(const std::string& url) {
}

void AudioMediaPipeline::play_announcement_(audio::AudioFile *media_file) {
}

void AudioMediaPipeline::set_state_(AudioMediaPipelineState state) {
  esph_log_d(TAG, "Updated pipeline state: %s",pipeline_state_to_string(state));
  this->state_ = state;
}

std::string AudioMediaPipeline::url_encode_(const std::string& input) {
  std::string result;
  for (char c : input) {
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      result += c;
    } else if (c == ' ') {
      result += "%20";
    } else {
      char hex[4];
      sprintf(hex, "%%%02X", (int)c);
      result += hex;
    }
  }
  return result;
}
bool AudioMediaPipeline::isServerTranscoding_() {
  if (AudioMediaPipeline::access_token.length() > 0 && this->format_ != "none") {
    return true;
  }
  return false;
}
std::string AudioMediaPipeline::get_transcode_url_(const std::string& url) {
  std::string result;
  if (this->isServerTranscoding_()) {
      std::hash<std::string> hasher;
      size_t hashValue = hasher(url);
      
      std::string encoded_url = this->url_encode_(url);
	  std::string wd2 = "";
	  if (this->format_ != "mp3") {
		  wd2 = "&width=2";
	  }
      std::string proxy_url = this->ffmpeg_server_ + "/api/esphome/ffmpeg_proxy_with_conversion_info/" \
	  + App.get_name() + "/" + std::to_string(hashValue) + "." + this->format_ \
      + "?rate=" + std::to_string(this->rate_) + "&channels=" + std::to_string(this->ch_) + wd2 + "&media=" + encoded_url;
      esph_log_d(TAG,"using ffmpeg proxy on: %s", url.c_str());
      result = proxy_url;
  }
  else {
    result = url;
  }
  return result;
}

esp_err_t AudioMediaPipeline::http_stream_event_handle_(http_stream_event_msg_t *msg)
{
  esp_http_client_handle_t http = (esp_http_client_handle_t)msg->http_client;
  
  if (msg->event_id == HTTP_STREAM_PRE_REQUEST) {
    // set header
    esph_log_d("adf_media_pipeline", "HTTP client HTTP_STREAM_PRE_REQUEST, set bearer token");
	
	if (AudioMediaPipeline::access_token.length() > 0) {
		std::string value = "Bearer " + AudioMediaPipeline::access_token;
		esp_http_client_set_header(http, "Authorization", value.c_str());
	}
	
    return ESP_OK;
  }
  return ESP_OK;
}

audio_pipeline_handle_t AudioMediaPipeline::adf_audio_pipeline_init() {
  audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
  return audio_pipeline_init(&pipeline_cfg);
}
  
audio_element_handle_t AudioMediaPipeline::adf_http_stream_init(int http_stream_rb_size) {
  http_stream_cfg_t http_cfg = HTTP_STREAM_CFG_DEFAULT();
  http_cfg.out_rb_size = http_stream_rb_size;
  http_cfg.task_core = this->http_stream_task_core_;
  http_cfg.task_prio = this->http_stream_task_prio_;
  http_cfg.event_handle = AudioMediaPipeline::http_stream_event_handle_;
  return http_stream_init(&http_cfg);
}

audio_element_handle_t AudioMediaPipeline::adf_embed_flash_stream_init() {
  embed_flash_stream_cfg_t embed_cfg = EMBED_FLASH_STREAM_CFG_DEFAULT();
  return embed_flash_stream_init(&embed_cfg); 
}

audio_element_handle_t AudioMediaPipeline::adf_esp_decoder_init() {
  audio_decoder_t auto_decode[] = {
#ifdef USE_AMRNB_DECODER
        DEFAULT_ESP_AMRNB_DECODER_CONFIG(),
#endif
#ifdef USE_AMRWB_DECODER
        DEFAULT_ESP_AMRWB_DECODER_CONFIG(),
#endif
#ifdef USE_FLAC_DECODER
        DEFAULT_ESP_FLAC_DECODER_CONFIG(),
#endif
#ifdef USE_OGG_DECODER
        DEFAULT_ESP_OGG_DECODER_CONFIG(),
#endif
#ifdef USE_OPUS_DECODER
        DEFAULT_ESP_OPUS_DECODER_CONFIG(),
#endif
#ifdef USE_MP3_DECODER
        DEFAULT_ESP_MP3_DECODER_CONFIG(),
#endif
#ifdef USE_WAV_DECODER
        DEFAULT_ESP_WAV_DECODER_CONFIG(),
#endif
#ifdef USE_AAC_DECODER
        DEFAULT_ESP_AAC_DECODER_CONFIG(),
#endif
#ifdef USE_M4A_DECODER
        DEFAULT_ESP_M4A_DECODER_CONFIG(),
#endif
#ifdef USE_TS_DECODER
        DEFAULT_ESP_TS_DECODER_CONFIG(),
#endif
  };
  esp_decoder_cfg_t auto_dec_cfg = DEFAULT_ESP_DECODER_CONFIG();
  auto_dec_cfg.out_rb_size = this->esp_decoder_rb_size_;
  auto_dec_cfg.task_core = this->esp_decoder_task_core_;
  auto_dec_cfg.task_prio = this->esp_decoder_task_prio_;
  int auto_decode_len = 0;
  for (audio_decoder_t ad : auto_decode) {
    auto_decode_len++;
  }
  return esp_decoder_init(&auto_dec_cfg, auto_decode, auto_decode_len);
}

audio_element_handle_t AudioMediaPipeline::adf_rsp_filter_init() {
  rsp_filter_cfg_t rsp_sdcard_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
  rsp_sdcard_cfg.src_rate = this->rate_;
  rsp_sdcard_cfg.src_ch = this->ch_;
  rsp_sdcard_cfg.dest_rate = this->rate_;
  rsp_sdcard_cfg.dest_ch = this->ch_;
  return rsp_filter_init(&rsp_sdcard_cfg);
}

audio_element_handle_t AudioMediaPipeline::adf_downmix_init() {
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
  audio_element_handle_t downmixer = downmix_init(&downmix_cfg);
  downmix_set_input_rb_timeout(downmixer, 0, 0);
  downmix_set_input_rb_timeout(downmixer, 0, 1);
  esp_downmix_input_info_t source_information[2] = {0};
  esp_downmix_input_info_t source_info_1 = {
    .samplerate = (int)this->rate_,
    .channel = (int)this->ch_,
    .bits_num = (int)this->bits_,
    .gain = {0, -15},
    .transit_time = 100,
  };
  source_information[0] = source_info_1;
  esp_downmix_input_info_t source_info_2= {
    .samplerate = (int)this->rate_,
    .channel = (int)this->ch_,
    .bits_num = (int)this->bits_,
    .gain = {-15, 1},
    .transit_time = 100,
  };
  source_information[1] = source_info_2;
  source_info_init(downmixer, source_information);
  return downmixer;
}

audio_element_handle_t AudioMediaPipeline::adf_raw_stream_init() {
  raw_stream_cfg_t raw_cfg = RAW_STREAM_CFG_DEFAULT();
  raw_cfg.type = AUDIO_STREAM_WRITER;
  return raw_stream_init(&raw_cfg);
}

audio_element_handle_t AudioMediaPipeline::adf_i2s_stream_init() {
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
    .use_alc = this->use_adf_alc_,
    .volume = this->volume_ - 64,
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
  return i2s_stream_init(&i2s_cfg);
}

}  // namespace esp_audio
}  // namespace esphome

#endif  // USE_ESP_IDF
