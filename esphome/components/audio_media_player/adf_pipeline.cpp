#include "adf_pipeline.h"

#ifdef USE_ESP_IDF

#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esp_http_client.h"

namespace esphome {
namespace esp_adf {

static const char *const TAG = "adf_media_pipeline";

const char *pipeline_state_to_string(AdfPipelineState state) {
  switch (state) {
    case AdfPipelineState::STARTING:
      return "STARTING";
    case AdfPipelineState::RUNNING:
      return "RUNNING";
    case AdfPipelineState::STOPPING:
      return "STOPPING";
    case AdfPipelineState::STOPPED:
      return "STOPPED";
    case AdfPipelineState::PAUSING:
      return "PAUSING";
    case AdfPipelineState::PAUSED:
      return "PAUSED";
    case AdfPipelineState::RESUMING:
      return "RESUMING";
    case AdfPipelineState::START_ANNOUNCING:
      return "START_ANNOUNCING";
    case AdfPipelineState::ANNOUNCING:
      return "ANNOUNCING";
    case AdfPipelineState::STOP_ANNOUNCING:
      return "STOP_ANNOUNCING";
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

esp_err_t AdfMediaPipeline::http_stream_event_handle_(http_stream_event_msg_t *msg)
{
  esp_http_client_handle_t http = (esp_http_client_handle_t)msg->http_client;
  
  if (msg->event_id == HTTP_STREAM_PRE_REQUEST) {
    // set header
    esph_log_d("adf_media_pipeline", "HTTP client HTTP_STREAM_PRE_REQUEST, set bearer token");
	
	if (AdfMediaPipeline::access_token.length() > 0) {
		std::string value = "Bearer " + AdfMediaPipeline::access_token;
		esp_http_client_set_header(http, "Authorization", value.c_str());
	}
	
    return ESP_OK;
  }
  return ESP_OK;
}

std::string AdfMediaPipeline::access_token = "";

void AdfMediaPipeline::dump_config() {
  esph_log_config(TAG, "dout: %d",this->dout_pin_);
  esph_log_config(TAG, "mclk: %d",this->mclk_pin_);
  esph_log_config(TAG, "bclk: %d",this->bclk_pin_);
  esph_log_config(TAG, "lrclk: %d",this->lrclk_pin_);
  if (this->isServerTranscoding_()) {
    esph_log_config(TAG, "transcode server: %s",this->ffmpeg_server_.c_str());
    esph_log_config(TAG, "transcode target format: %s",this->format_.c_str());
    esph_log_config(TAG, "transcode target rate: %d",this->rate_);
  }
  esph_log_config(TAG, "http stream rb size: %d",this->http_stream_rb_size_); 
  esph_log_config(TAG, "esp decoder rb size: %d",this->esp_decoder_rb_size_); 
  esph_log_config(TAG, "i2s stream rb size: %d",this->i2s_stream_rb_size_);    
}

void AdfMediaPipeline::set_url(const std::string& url, bool is_announcement) {
}

void AdfMediaPipeline::play(bool resume) {
}

void AdfMediaPipeline::stop(bool pause) {
}

void AdfMediaPipeline::clean_up() {
  if (this->is_initialized_) {
    this->pipeline_deinit_();
  }
}

void AdfMediaPipeline::set_volume(int volume) {

  //input volume is 0 to 100
  if (volume > 100)
    volume = 100;
  else if (volume < 0)
    volume = 0;

  this->volume_ = volume;
  if (this->state_ == AdfPipelineState::RUNNING || this->state_ == AdfPipelineState::STARTING) {
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

void AdfMediaPipeline::mute() {
  if (this->state_ == AdfPipelineState::RUNNING) {
    if (this->use_adf_alc_) {
        esph_log_v(TAG, "attempt seting mute");
      if (i2s_alc_volume_set(this->i2s_stream_writer_, -64) != ESP_OK) {
        esph_log_e(TAG, "error seting mute");
      }
    }
  }
}

void AdfMediaPipeline::unmute() {
  if (this->state_ == AdfPipelineState::RUNNING) {
    if (this->use_adf_alc_) {
      int target_volume = volume_ - 64;
        esph_log_v(TAG, "attempt unmute to %d", target_volume);
      if (i2s_alc_volume_set(this->i2s_stream_writer_, target_volume) != ESP_OK) {
        esph_log_e(TAG, "error setting unmute to volume to %d", target_volume);
      }
    }
  }
}

AdfPipelineState AdfMediaPipeline::loop() {
  return this->state_;
}

void AdfMediaPipeline::pipeline_init_() {
  this->is_initialized_ = true;
}

void AdfMediaPipeline::pipeline_deinit_() {
  this->is_initialized_ = false;
}

void AdfMediaPipeline::pipeline_play_(bool resume) {
}

void AdfMediaPipeline::pipeline_stop_(bool pause, bool cleanup) {
}

void AdfMediaPipeline::play_announcement_(const std::string& url) {
}

void AdfMediaPipeline::set_state_(AdfPipelineState state) {
  esph_log_d(TAG, "Updated pipeline state: %s",pipeline_state_to_string(state));
  this->state_ = state;
}

std::string AdfMediaPipeline::url_encode_(const std::string& input) {
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
bool AdfMediaPipeline::isServerTranscoding_() {
  if (AdfMediaPipeline::access_token.length() > 0 && this->format_ != "none") {
    return true;
  }
  return false;
}
std::string AdfMediaPipeline::get_transcode_url_(const std::string& url) {
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

}  // namespace esp_adf
}  // namespace esphome

#endif  // USE_ESP_IDF
