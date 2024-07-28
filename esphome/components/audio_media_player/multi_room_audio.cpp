#include "multi_room_audio.h"

#ifdef USE_ESP_IDF

#include <string>
#include <sstream>
#include <sys/param.h>
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include <cJSON.h>
#include <algorithm>

#include "esphome/core/log.h"
#include "esphome/components/network/ip_address.h"
#include "esphome/components/network/util.h"
#include "esphome/components/api/homeassistant_service.h"

// Refer to "esp-idf/examples/protocols/sockets/udp_multicast/main/udp_multicast_example_main.c"
// as basis for the multicast coding, only coded for IPV4

namespace esphome {
namespace esp_adf {

static const char *const TAG = "multi_room_audio";


std::string MultiRoomAction::to_string() {

  std::string message = "{\"action\":\"" + this->type + "\"";
  if (this->type == "sync_position") {
    std::ostringstream otimestampt;
    otimestampt << this->timestamp;
    // send time as a string so that cJSON can parse
    message += ",\"position_timestamp\":\"" + otimestampt.str()+"\"";
    message += ",\"position\":\"" + this->data + "\"";
  }
  message += "}";
  esph_log_d(TAG, "Send: %s", message.c_str());
  return message;
}

void MultiRoomAudio::loop() {
  if (get_mrm() == media_player::MEDIA_PLAYER_MRM_FOLLOWER) {
    //recv_broadcast_();
  }
}

void MultiRoomAudio::listen() {
  if (mrm_ == media_player::MEDIA_PLAYER_MRM_OFF) {
    mrm_command_("mrmlisten");
    esph_log_d(TAG, "multiRoomAudio_ listen");
    //this->listen_();
  }
}

void MultiRoomAudio::unlisten() {
  if (mrm_ != media_player::MEDIA_PLAYER_MRM_OFF) {
    mrm_command_("mrmunlisten");
    esph_log_d(TAG, "multiRoomAudio_ unlisten");
    //this->unlisten_();
  }
}

void MultiRoomAudio::set_url(const std::string url) {
  std::string message = "{\"mrmurl\":\"" + url + "\"}";
  mrm_command_(message);
}

void MultiRoomAudio::start(int64_t timestamp) {
  
  std::string message = "{\"mrmstart\":\"\"";
  std::ostringstream otimestampt;
  otimestampt << timestamp;
  // send time as a string so that cJSON can parse
  message += ",\"timestamp\":\"" + otimestampt.str()+"\"}";
  mrm_command_(message);
}

void MultiRoomAudio::stop() {
  mrm_command_("mrmstop");
}

void MultiRoomAudio::pause() {
  mrm_command_("mrmpause");
}

void MultiRoomAudio::resume(int64_t timestamp) {
  
  std::string message = "{\"mrmresume\":\"\"";
  std::ostringstream otimestampt;
  otimestampt << timestamp;
  // send time as a string so that cJSON can parse
  message += ",\"timestamp\":\"" + otimestampt.str()+"\"}";
  mrm_command_(message);
}

void MultiRoomAudio::turn_on() {
  player_on_ = true;
  if (group_members_.length() > 0 && mrm_ == media_player::MEDIA_PLAYER_MRM_LEADER) {
    std::string group_members = group_members_ + ",";
    char *token = strtok(const_cast<char*>(group_members.c_str()), ",");
    esphome::api::HomeassistantServiceResponse resp;
    resp.service = "media_player.turn_on";
    while (token != nullptr)
    {
      esph_log_d(TAG, "%s on %s turn_on", resp.service.c_str(), token);
      esphome::api::HomeassistantServiceMap kv1;
      kv1.key = "entity_id";
      kv1.value = token;
      resp.data.push_back(kv1);
      esphome::api::global_api_server->send_homeassistant_service_call(resp);
      token = strtok(nullptr, ",");
    }
  }
}

void MultiRoomAudio::turn_off() {
  player_on_ = false;
  if (group_members_.length() > 0 && mrm_ == media_player::MEDIA_PLAYER_MRM_LEADER) {
    std::string group_members = group_members_ + ",";
    char *token = strtok(const_cast<char*>(group_members.c_str()), ",");
    esphome::api::HomeassistantServiceResponse resp;
    resp.service = "media_player.turn_off";
    while (token != nullptr)
    {
      esph_log_d(TAG, "%s on %s turn_off", resp.service.c_str(), token);
      esphome::api::HomeassistantServiceMap kv1;
      kv1.key = "entity_id";
      kv1.value = token;
      resp.data.push_back(kv1);
      esphome::api::global_api_server->send_homeassistant_service_call(resp);
      token = strtok(nullptr, ",");
    }
  }
}

void MultiRoomAudio::volume(float volume) {
  if (group_members_.length() > 0 && mrm_ == media_player::MEDIA_PLAYER_MRM_LEADER) {
    std::string group_members = group_members_ + ",";
    char *token = strtok(const_cast<char*>(group_members.c_str()), ",");
    esphome::api::HomeassistantServiceResponse resp;
    resp.service = "media_player.volume_set";
    esphome::api::HomeassistantServiceMap kv2;
    kv2.key = "volume_level";
    kv2.value = to_string(volume);

    while (token != nullptr)
    {
      esph_log_d(TAG, "%s on %s volume", resp.service.c_str(), token);
      esphome::api::HomeassistantServiceMap kv1;
      kv1.key = "entity_id";
      kv1.value = token;
      resp.data.push_back(kv1);
      resp.data.push_back(kv2);

      esphome::api::global_api_server->send_homeassistant_service_call(resp);
      token = strtok(nullptr, ",");
    }
  }
}

void MultiRoomAudio::mute() {
  if (group_members_.length() > 0 && mrm_ == media_player::MEDIA_PLAYER_MRM_LEADER) {
    std::string group_members = group_members_ + ",";
    char *token = strtok(const_cast<char*>(group_members.c_str()), ",");
    esphome::api::HomeassistantServiceResponse resp;
    resp.service = "media_player.volume_mute";
    esphome::api::HomeassistantServiceMap kv2;
    kv2.key = "is_volume_muted";
    kv2.value = "true";

    while (token != nullptr)
    {
      esph_log_d(TAG, "%s on %s mute", resp.service.c_str(), token);
      esphome::api::HomeassistantServiceMap kv1;
      kv1.key = "entity_id";
      kv1.value = token;
      resp.data.push_back(kv1);
      resp.data.push_back(kv2);

      esphome::api::global_api_server->send_homeassistant_service_call(resp);
      token = strtok(nullptr, ",");
    }
  }
}

void MultiRoomAudio::unmute() {
  if (group_members_.length() > 0 && mrm_ == media_player::MEDIA_PLAYER_MRM_LEADER) {
    std::string group_members = group_members_ + ",";
    char *token = strtok(const_cast<char*>(group_members.c_str()), ",");
    esphome::api::HomeassistantServiceResponse resp;
    resp.service = "media_player.volume_mute";
    esphome::api::HomeassistantServiceMap kv2;
    kv2.key = "is_volume_muted";
    kv2.value = "false";

    while (token != nullptr)
    {
      esph_log_d(TAG, "%s on %s unmute", resp.service.c_str(), token);
      esphome::api::HomeassistantServiceMap kv1;
      kv1.key = "entity_id";
      kv1.value = token;
      resp.data.push_back(kv1);
      resp.data.push_back(kv2);

      esphome::api::global_api_server->send_homeassistant_service_call(resp);
      token = strtok(nullptr, ",");
    }
  }
}

/*
void MultiRoomAudio::send_position(int64_t timestamp, int64_t position) {
  std::ostringstream oss;
  oss << position;
  MultiRoomAction action;
  action.type = "sync_position";
  action.timestamp = timestamp;
  action.data = oss.str();
  send_broadcast_(&action);
}
*/
int64_t MultiRoomAudio::get_timestamp() {
  struct timeval tv_now;
  gettimeofday(&tv_now, NULL);
  return (int64_t)tv_now.tv_sec * 1000000L + (int64_t)tv_now.tv_usec;
}

void MultiRoomAudio::mrm_command_(const std::string command) {
  if (group_members_.length() > 0 && get_mrm() == media_player::MEDIA_PLAYER_MRM_LEADER) {

    std::string group_members = group_members_ + ",";
    char *token = strtok(const_cast<char*>(group_members.c_str()), ",");
    esphome::api::HomeassistantServiceResponse resp;
    resp.service = "media_player.play_media";
    
    //TBD - this requires a change in core esphome media-player
    esphome::api::HomeassistantServiceMap kv2;
    kv2.key = "media_content_id";
    kv2.value = command;
    esphome::api::HomeassistantServiceMap kv3;
    kv3.key = "media_content_type";
    kv3.value = "music";

    while (token != nullptr)
    {
      esph_log_d(TAG, "%s on %s %s", resp.service.c_str(), token, command.c_str());
      esphome::api::HomeassistantServiceMap kv1;
      kv1.key = "entity_id";
      kv1.value = token;
      resp.data.push_back(kv1);
      resp.data.push_back(kv2);
      resp.data.push_back(kv3);
      esphome::api::global_api_server->send_homeassistant_service_call(resp);
      token = strtok(nullptr, ",");
    }
  }
}

/*
void MultiRoomAudio::listen_() {
  if (udp_socket_ < 0 && player_on_) {
    for(int i = 1; i <= 100; ++i) {
      udp_socket_ = create_multicast_ipv4_socket_();
      if (udp_socket_ >= 0) {
        esph_log_d(TAG, "Connection open for multicast on %s:%d", multicast_ipv4_addr.c_str(),udp_port);
        break;
      }
    }
    if (udp_socket_ < 0) {
      esph_log_e(TAG, "Unable to open Connection");
    }
  }
  if (!player_on_) {
    unlisten_();
  }
}

void MultiRoomAudio::unlisten_() {
  if (udp_socket_ >=0 || !player_on_) {
    if (udp_socket_ >=0) {
      shutdown(udp_socket_, 0);
      close(udp_socket_);
      udp_socket_ = -1;
      esph_log_d(TAG, "Connection closed for multicast on %s:%d", multicast_ipv4_addr.c_str(),udp_port);
    }
  }
}

int MultiRoomAudio::create_multicast_ipv4_socket_(void)
{
  struct sockaddr_in saddr = { 0 };
  int sock = -1;
  int retval = 0;

  // use function lwip_socket instead of socket because of 
  // conflict with esphome/components/socket/socket.h
  sock = lwip_socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
  if (sock < 0) {
    esph_log_e(TAG, "Failed to create socket. Error %d", errno);
    return -1;
  }

  // Bind the socket to any address
  saddr.sin_family = PF_INET;
  saddr.sin_port = htons(this->udp_port);
  saddr.sin_addr.s_addr = htonl(INADDR_ANY);
  retval = bind(sock, (struct sockaddr *)&saddr, sizeof(struct sockaddr_in));
  if (retval < 0) {
    esph_log_e(TAG, "Failed to bind socket. Error %d", errno);
    close(sock);
    return retval;
  }

  // Assign multicast TTL (set separately from normal interface TTL)
  uint8_t ttl = this->multicast_ttl;
  retval = setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(uint8_t));
  if (retval < 0) {
    esph_log_e(TAG, "Failed to set IP_MULTICAST_TTL. Error %d", errno);
    close(sock);
    return retval;
  }

  // no loopback
  uint8_t loopback_val = 0;
  retval = setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP, &loopback_val, sizeof(uint8_t));
  if (retval < 0) {
    esph_log_e(TAG, "Failed to set IP_MULTICAST_LOOP. Error %d", errno);
    close(sock);
    return retval;
  }

  // this is also a listening socket, so add it to the multicast
  // group for listening...
  retval = socket_add_ipv4_multicast_group_(sock, true);
  if (retval < 0) {
    close(sock);
    return retval;
  }

  // All set, socket is configured for sending and receiving
  return sock;
}

// Add a socket to the IPV4 multicast group
int MultiRoomAudio::socket_add_ipv4_multicast_group_(int sock, bool assign_source_if)
{
  struct ip_mreq imreq = { 0 };
  struct in_addr iaddr = { 0 };
  int retval = 0;
  // Configure source interface
  for (auto ip : esphome::network::get_ip_addresses()) {
    if (ip.is_ip4()) {
      esph_log_d(TAG,"Multicast Source: %s",ip.str().c_str());
      inet_aton(ip.str().c_str(), &iaddr);
      inet_aton(ip.str().c_str(), &(imreq.imr_interface.s_addr));
      break;
    }
  }

  // Configure multicast address to listen to
  retval = inet_aton(this->multicast_ipv4_addr.c_str(), &imreq.imr_multiaddr.s_addr);
  if (retval != 1) {
    esph_log_e(TAG, "Configured IPV4 multicast address '%s' is invalid.", this->multicast_ipv4_addr.c_str());
    // Errors in the return value have to be negative
    retval = -1;
    return retval;
  }
  esph_log_i(TAG, "Configured IPV4 Multicast address %s", inet_ntoa(imreq.imr_multiaddr.s_addr));
  if (!IP_MULTICAST(ntohl(imreq.imr_multiaddr.s_addr))) {
    esph_log_w(TAG, "Configured IPV4 multicast address '%s' is not a valid multicast address. This will probably not work.", this->multicast_ipv4_addr.c_str());
  }

  if (assign_source_if) {
    // Assign the IPv4 multicast source interface, via its IP
    // (only necessary if this socket is IPV4 only)
    retval = setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF, &iaddr,
             sizeof(struct in_addr));
    if (retval < 0) {
      esph_log_e(TAG, "Failed to set IP_MULTICAST_IF. Error %d", errno);
      return retval;
    }
  }

  retval = setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
             &imreq, sizeof(struct ip_mreq));
  if (retval < 0) {
    esph_log_e(TAG, "Failed to set IP_ADD_MEMBERSHIP. Error %d", errno);
    return retval;
  }

  return ESP_OK;
}

void MultiRoomAudio::send_broadcast_(MultiRoomAction *action) {
  listen_();
  char addr_name[32] = { 0 };
  int retval = 0;
  if (udp_socket_ >= 0) {
    // send actions to multicast
    std::string message = action->to_string();

    int len = message.length();
    if (len > multicast_buffer_size_) {
        esph_log_e(TAG, "%d is larger than will be able to be received: %d", len, multicast_buffer_size_);
        return;
    }

    struct addrinfo hints = {
     .ai_flags = AI_PASSIVE,
     .ai_socktype = SOCK_DGRAM,
    };
    struct addrinfo *res;
    hints.ai_family = AF_INET; // For an IPv4 socket

    retval = getaddrinfo(multicast_ipv4_addr.c_str(), NULL, &hints, &res);
    if (retval < 0) {
      esph_log_e(TAG, "getaddrinfo() failed for IPV4 destination address. error: %d", retval);
      udp_socket_ = retval;
      return;
    }
    if (res == 0) {
      esph_log_e(TAG, "getaddrinfo() did not return any addresses");
      udp_socket_ = -1;
      return;
    }
    ((struct sockaddr_in *)res->ai_addr)->sin_port = htons(udp_port);
    inet_ntoa_r(((struct sockaddr_in *)res->ai_addr)->sin_addr, addr_name, sizeof(addr_name)-1);
    retval = sendto(udp_socket_, message.c_str(), len, 0, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);
    if (retval < 0) {
      esph_log_e(TAG, "IPV4 sendto failed. errno: %d", errno);
      udp_socket_ = retval;
    }
  }
}

void MultiRoomAudio::recv_broadcast_() {
  listen_();
  char recvbuf[multicast_buffer_size_] = { 0 };
  char addr_name[32] = { 0 };
  struct timeval tv = {
    .tv_sec = 0,
    .tv_usec = 0,
  };
  fd_set rfds;
  FD_ZERO(&rfds);
  FD_SET(udp_socket_, &rfds);

  int s = select(udp_socket_ + 1, &rfds, NULL, NULL, &tv);

  // socket failure
  if (s < 0) {
    esph_log_e(TAG, "Select failed: errno %d", errno);
    udp_socket_ = s;
    return;
  }

  // get actions from multicast
  if (s > 0) {
    if (FD_ISSET(udp_socket_, &rfds)) {
      // Incoming datagram received
      struct sockaddr_storage raddr; // Large enough for both IPv4 or IPv6
      socklen_t socklen = sizeof(raddr);
      int len = recvfrom(udp_socket_, recvbuf, sizeof(recvbuf)-1, 0, (struct sockaddr *)&raddr, &socklen);
      if (len < 0) {
        esph_log_e(TAG, "multicast recvfrom failed: errno %d", errno);
        udp_socket_ = s;
        return;
      }

      // Get the sender's address as a string
      if (raddr.ss_family == PF_INET) {
        inet_ntoa_r(((struct sockaddr_in *)&raddr)->sin_addr, addr_name, sizeof(addr_name)-1);
      }
      recvbuf[len] = 0; // Null-terminate whatever we received and treat like a string...
      std::string message = recvbuf;
      std::string address = addr_name;
      if (message != message_prior_) {
        process_message_(message, address);
        message_prior_ = message;
      }
    }
  }
}

void MultiRoomAudio::process_message_(std::string &message, std::string &sender) {
  if (message.length() > 0) {
    esph_log_d(TAG, "Received: %s from %s", message.c_str(), sender.c_str());
    
    cJSON *root = cJSON_Parse(message.c_str());
    std::string recv_action = cJSON_GetObjectItem(root,"action")->valuestring;
    
    if (recv_action == "sync_position") {
      if (get_mrm() == media_player::MEDIA_PLAYER_MRM_FOLLOWER) {
        // process this action to speed up or slow down followers output to sync with leader
        std::string timestamp_str = cJSON_GetObjectItem(root,"position_timestamp")->valuestring;
        int64_t timestamp = strtoll(timestamp_str.c_str(), NULL, 10);
        std::string position_str = cJSON_GetObjectItem(root,"position")->valuestring;
        int64_t position = strtoll(position_str.c_str(), NULL, 10);
        MultiRoomAction action;
        action.type = recv_action;
        action.data = position_str;
        action.timestamp = timestamp;
        recv_actions.push(action);
      }
    }
    cJSON_Delete(root);
  }
}
*/

}  // namespace esp_adf
}  // namespace esphome
#endif
