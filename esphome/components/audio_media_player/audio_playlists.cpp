#include "audio_playlists.h"

#ifdef USE_ESP_IDF

#include <algorithm>
#include <random>

#include "esphome/core/log.h"
#include <esp_http_client.h>

namespace esphome {
namespace esp_adf {

static const char *const TAG = "audio_playlists";

void AudioPlaylists::playlist_add(const std::string& uri, bool toBack, bool shuffle) {
  
  esph_log_v(TAG, "playlist_add_: %s", this->uri.c_str());

  unsigned int vid = this->playlist_.size();

  if (uri.find("m3u") != std::string::npos) {
    int pl = this->parse_m3u_into_playlist_(uri.c_str(), toBack, shuffle);
  }
  else {
    if (!toBack) {
      this->update_playlist_order_(1);
      vid = 0;
    }
    ADFPlaylistTrack track;
    track.url = uri;
    track.order = 0;
    this->playlist_.push_back(track);
  }
  
  esph_log_v(TAG, "Number of playlist tracks = %d", this->playlist_.size());
}

void AudioPlaylists::update_playlist_order_(unsigned int start_order) {
  unsigned int vid = this->playlist_.size();
  if (vid > 0) {
    sort(this->playlist_.begin(), this->playlist_.end());
    for(unsigned int i = 0; i < vid; i++)
    {
      this->playlist_[i].order = i + start_order;
    }
  }
}

void AudioPlaylists::shuffle_playlist(bool shuffle) {
  unsigned int vid = this->playlist_.size();
  if (vid > 0) {
    if (shuffle) {
      auto rng = std::default_random_engine {};
      std::shuffle(std::begin(this->playlist_), std::end(this->playlist_), rng);
    }
    else {
      sort(this->playlist_.begin(), this->playlist_.end());
    }
    for(unsigned int i = 0; i < vid; i++)
    {
      this->playlist_[i].is_played = false;
    }
  }
}

void AudioPlaylists::clean_playlist()
{
  unsigned int vid = this->playlist_.size();
  if ( this->playlist_.size() > 0 ) {
    this->playlist_.clear();
  }
}

int AudioPlaylists::next_playlist_track_id()
{
  unsigned int vid = this->playlist_.size();
  if ( vid > 0 ) {
    for(unsigned int i = 0; i < vid; i++)
    {
      bool ip = this->playlist_[i].is_played;
      if (!ip) {
        return i;
      }
    }
  }
  return -1;
}

int AudioPlaylists::previous_playlist_track_id()
{
  unsigned int vid = this->playlist_.size();
  if ( vid > 0 ) {
    for(unsigned int i = 0; i < vid; i++)
    {
      bool ip = this->playlist_[i].is_played;
      if (!ip) {
        int j = i-1;
        if (j < 0) j = 0;
        this->playlist_[j].is_played = false;
        return j;
      }
    }
  }
  return -1;
}

void AudioPlaylists::set_playlist_track_as_played(int track_id)
{
  if ( this->playlist_.size() > 0 ) {
      unsigned int vid = this->playlist_.size();
    if (track_id < 0) {
      for(unsigned int i = 0; i < vid; i++)
      {
        bool ip = this->playlist_[i].is_played;
        if (!ip) {
          this->playlist_[i].is_played = true;
          break;
        }
      }
    }
    else {
      for(unsigned int i = 0; i < vid; i++)
      {
        if (i < track_id) {
          this->playlist_[i].is_played = true;
        }
        else {
          this->playlist_[i].is_played = false;
        }
      }
    }
  }
}

int AudioPlaylists::parse_m3u_into_playlist_(const char *url, bool toBack, bool shuffle)
{
  
    unsigned int vid = this->playlist_.size();
    esp_http_client_config_t config = {
        .url = url
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    char *response;
    esp_err_t err = esp_http_client_open(client,0);
    int rc = 0;
    if (err == ESP_OK) {
      //int cl = esp_http_client_get_content_length(client);
      int cl =  esp_http_client_fetch_headers(client);
      esph_log_v(TAG, "HTTP Status = %d, content_length = %d", esp_http_client_get_status_code(client), cl);
      response = (char *)malloc(cl + 1);
      if (response == NULL) {
        esph_log_e(TAG, "Cannot malloc http receive buffer");
        rc = -1;
      }
      else {
        int rl = esp_http_client_read(client, response, cl); 
        if (rl < 0) {
          esph_log_e(TAG, "HTTP request failed: %s, ", esp_err_to_name(err));
          free(response);
          rc = -1;
        }
        else {
          response[cl] = '\0';
          esph_log_v(TAG, "Response: %s", response);
          size_t start = 0;
          size_t end;
          char *ptr;
          char *buffer = response;
          bool keeplooping = true;
          std::string artist = "";
          std::string album = "";
          std::string title = "";
          int duration = 0;
          if (toBack) {
            this->update_playlist_order_(1000);
            vid = 0;
          }
          while (keeplooping) {
            ptr = strchr(buffer , '\n' );
            if (ptr != NULL)
            {
              end = ptr - response;
            }
            else {
              end = cl;
              keeplooping = false;
            }
            size_t lngth = (end - start);
            //Finding a carriage return, assume its a Windows file and this line ends with "\r\n"
            if (strchr(buffer,'\r') != NULL) {
              lngth--;
            }
            if (lngth > 0) {
              char *cLine;
              cLine = (char *)malloc(lngth + 1);
              if (cLine == NULL) {
                esph_log_e(TAG, "Cannot malloc cLine");
                free(response);
                rc = -1;
                break;
              }
              sprintf (cLine,"%.*s", lngth, buffer);
              cLine[lngth] = '\0';
              esph_log_v(TAG, "%s", cLine);
              if (strstr(cLine,"#EXTART:") != NULL) {
                artist = cLine + 8;
              }
              else if (strstr(cLine,"#EXTALB:") != NULL) {
                if (strchr(cLine,'-') != NULL) {
                  album = strchr(cLine,'-') + 1;
                }
                else {
                  album = cLine + 8;
                }
              }
              else if (strstr(cLine,"#EXTINF:") != NULL) {
                char *comma_ptr;
                comma_ptr = strchr(cLine,',');
                if (comma_ptr != NULL) {
                  title = strchr(cLine,',') + 1;
                  std::string inf_line = cLine + 8;
                  size_t comma_sz = comma_ptr - (cLine + 8);
                  std::string duration_str = inf_line.substr(0, comma_sz);
                  float duration_f = strtof(duration_str.c_str(), NULL);
                  duration = round(duration_f);
                }
              }
              else if (strchr(cLine,'#') == NULL) {
                esph_log_v(TAG, "Create Playlist Track %s: %s: %s duration: %d",
                  artist.c_str(),album.c_str(),title.c_str(),duration);
                ADFPlaylistTrack track;
                track.url = cLine;
                track.order = vid;
                track.artist = artist;
                track.album = album;
                track.title = title;
                track.duration = duration;
                this->playlist_.push_back(track);
                vid++;
              }
              free(cLine);
            }
            start = end + 1;
            buffer = ptr + 1;
          }
          free(response);
        }
      }
    } else {
        esph_log_e(TAG, "HTTP request failed: %s", esp_err_to_name(err));
        rc = -1;
    }
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    
    if (toBack) {
        this->update_playlist_order_(0);
    }
    if (shuffle) {
      auto rng = std::default_random_engine {};
      std::shuffle(std::begin(this->playlist_), std::end(this->playlist_), rng);
      unsigned int vid = this->playlist_.size();
      for(unsigned int i = 0; i < vid; i++)
      {
        esph_log_v(TAG, "Playlist: %s", this->playlist_[i].uri.c_str());
        this->playlist_[i].is_played = false;
      }
    }
    return rc;
}

void AudioPlaylists::clean_announcements()
{
  unsigned int vid = this->announcements_.size();
  if ( vid > 0 ) {
    /*
    for(unsigned int i = 0; i < vid; i++)
    {
      delete this->announcements_[i];
    }
    */
    this->announcements_.clear();
  }
}

}  // namespace esp_adf
}  // namespace esphome

#endif  // USE_ESP_IDF