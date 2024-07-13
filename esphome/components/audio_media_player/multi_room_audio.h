#pragma once

#ifdef USE_ESP_IDF

#include <string>
#include <queue>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esphome/components/media_player/media_player.h"

namespace esphome {
namespace esp_adf {

class MultiRoomAction {
 public:
  std::string type{""};
  std::string data{""};
  int64_t timestamp{};
  std::string to_string();
};

class MultiRoomAudio {
 public:
  // could be configurable through yaml
  std::string multicast_ipv4_addr{"239.255.255.252"};
  //port 1900 is in use, avoid
  int udp_port{1901};
  uint8_t multicast_ttl{5};

  // public methods used by audio_media_player
  void set_group_members(const std::string group_members) { this->group_members_ = group_members; }
  std::string get_group_members() { return this->group_members_; }
  void set_mrm(media_player::MediaPlayerMRM mrm) { this->mrm_ = mrm; }
  media_player::MediaPlayerMRM get_mrm() { return this->mrm_; }

  void loop();
  void listen();
  void unlisten();
  void set_url(const std::string url, int64_t timestamp);
  void start();
  void stop();
  void pause();
  void resume();
  void turn_on();
  void turn_off();
  void volume(float volume);
  void mute();
  void unmute();
  //void send_position(int64_t timestamp, int64_t position);
  int64_t get_timestamp();

  std::queue<MultiRoomAction> recv_actions;

 protected:
  void mrm_command_(const std::string command);
  /*
  void listen_();
  void unlisten_();
  int create_multicast_ipv4_socket_(void);
  int socket_add_ipv4_multicast_group_(int sock, bool assign_source_if);
  void send_broadcast_(MultiRoomAction *action);
  void recv_broadcast_();
  void process_message_(std::string &message, std::string &sender);
  */
  std::string group_members_{""};
  media_player::MediaPlayerMRM mrm_{media_player::MEDIA_PLAYER_MRM_OFF};
  //int udp_socket_{-1};
  //size_t multicast_buffer_size_{512};
  //std::string message_prior_{""};
  bool player_on_{false};
};

}  // namespace esp_adf
}  // namespace esphome

#endif
