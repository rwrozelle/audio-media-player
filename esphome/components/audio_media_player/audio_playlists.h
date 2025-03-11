#pragma once

#ifdef USE_ESP_IDF

#include <string>
#include <vector>
#include "esp_random.h"
#include <random>
#include "esphome/components/media_player/media_player.h"

namespace esphome {
namespace esp_audio {
  
class ESP32RandomEngine {
public:
    using result_type = uint32_t;

    static constexpr uint32_t min() { return 0; }
    static constexpr uint32_t max() { return UINT32_MAX; }

    result_type operator()() {
        return esp_random();
    }
};

class AudioUrlTrack {
  public:
    std::string url{""};
    bool is_played{false};
};

class AudioPlaylistTrack : public AudioUrlTrack {
  public:
    unsigned int order{0};
    std::string playlist{""};
    std::string artist{""};
    std::string album{""};
    std::string title{""};
    std::string thumbnail_url{""};
    int duration{0};
    
    // Overloading < operator 
    bool operator<(const AudioPlaylistTrack& obj) const
    { 
        return order < obj.order; 
    } 
};

class AudioPlaylists {

 public:
  std::vector<AudioPlaylistTrack> * get_playlist() {  return &playlist_; }

  void playlist_add(const std::string& new_uri, bool toBack, bool shuffle);
  void shuffle_playlist(bool shuffle);
  void clean_playlist();
  int next_playlist_track_id();
  int previous_playlist_track_id();
  void set_playlist_track_as_played(int track_id);

 protected:
  int parse_m3u_into_playlist_(const char *url, bool toBack, bool shuffle);
  void update_playlist_order_(unsigned int start_order);
  std::vector<AudioPlaylistTrack> playlist_;
  ESP32RandomEngine random_engine_;
};

}  // namespace esp_audio
}  // namespace esphome

#endif  // USE_ESP_IDF
