#pragma once

#ifdef USE_ESP_IDF

#include <string>
#include <vector>
#include "esphome/components/media_player/media_player.h"

namespace esphome {
namespace esp_adf {

class ADFUrlTrack {
  public:
    std::string url{""};
    bool is_played{false};
};

class ADFPlaylistTrack : public ADFUrlTrack {
  public:
    unsigned int order{0};
    std::string artist{""};
    std::string album{""};
    std::string title{""};
    int duration{0};
    
    // Overloading < operator 
    bool operator<(const ADFPlaylistTrack& obj) const
    { 
        return order < obj.order; 
    } 
};

class AudioPlaylists {

 public:
  std::vector<ADFUrlTrack> * get_announcements() {  return &announcements_; }
  std::vector<ADFPlaylistTrack> * get_playlist() {  return &playlist_; }

  void playlist_add(const std::string& new_uri, bool toBack, bool shuffle);
  void shuffle_playlist(bool shuffle);
  void clean_playlist();
  int next_playlist_track_id();
  int previous_playlist_track_id();
  void set_playlist_track_as_played(int track_id);
  void clean_announcements();

 protected:
  int parse_m3u_into_playlist_(const char *url, bool toBack, bool shuffle);
  void update_playlist_order_(unsigned int start_order);
  std::vector<ADFUrlTrack> announcements_;
  std::vector<ADFPlaylistTrack> playlist_;
};

}  // namespace esp_adf
}  // namespace esphome

#endif  // USE_ESP_IDF
