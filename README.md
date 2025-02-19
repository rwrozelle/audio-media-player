# ESPHome - Audio Media Player
## Install Version: ESPHome-2024.12.4, Core: 2025.2.0
* https://github.com/rwrozelle/core
* * /homeassistant/components/esphome - required 
* * /homeassistant/components/dlna_dms - optional to be able to play albums from a dlna server
* * /homeassistant/components/jellyfin - optional to be able to play albums from a jellyfin server
* * /homeassistant/components/media_source - use this if you are using dlna_dms and/ jellyfin

The jellyfin integration works better than the dlna_dms integration.

* https://github.com/rwrozelle/esphome, the components api and media_player.
* https://github.com/rwrozelle/aioesphomeapi

This external component provides an audio media-player with the following HA Available services:
* turn_on - sends action on_turn_on
* turn_off - sends action on_turn_off
* toggle, 
* volume_up
* volume_down
* volume_set
* volume_mute
* media_play
* media_pause
* media_stop
* media_next_track
* media_previous_track
* clear_playlist
* shuffle_set
* repeat_set
* play_media - can turn an m3u file into a playlist
* * enqueue - add, next, play, replace
* * announce - after announcement is played, the current track is restarted
* join - members of group are will turn off, turn on, set volume, and play the same media as leader. Synchronization is attempted by telling leader and group members to start media at the same time. Uses the Time component in the sntp platform to assume that chips have the same time.
* unjoin

![image info](./images/media-player.PNG)

Audio Media Component uses [Espressif Audio Development Framework (ADF)](https://github.com/espressif/esp-adf) version 2.6 and only works using the esp-idf version: 4.4.8

## External Component - audio-media-player
The esp-adf code is based on https://github.com/gnumpi/esphome_audio. Code is simplified to concentrate on above capabilities with the following components, note:  Only tested using this hardware:

1. ESP32-S3-DevKit1
2. I2S PCM5102 DAC Decoder

This component is built to solve the following use case:  Be able to play an extensive library of flac and mp3 files available in a local web server using ESPHome and standard HA functionality. I have python code that will generate m3u files from my library and I've made these files visible in Media Sources.

![image info](./images/media-source.PNG)

The code uses esp_decoder and is configured for the following:

* DEFAULT_ESP_AMRNB_DECODER_CONFIG(),
* DEFAULT_ESP_AMRWB_DECODER_CONFIG(),
* DEFAULT_ESP_FLAC_DECODER_CONFIG(),
* DEFAULT_ESP_OGG_DECODER_CONFIG(),
* DEFAULT_ESP_OPUS_DECODER_CONFIG(),
* DEFAULT_ESP_MP3_DECODER_CONFIG(),
* DEFAULT_ESP_WAV_DECODER_CONFIG(),
* DEFAULT_ESP_AAC_DECODER_CONFIG(),
* DEFAULT_ESP_M4A_DECODER_CONFIG(),
* DEFAULT_ESP_TS_DECODER_CONFIG(),

I've only used it with flac and mp3 files. Flac files play correct "most" of the time, a little chattering every now and then. I've written a Python script to use ffmpeg to convert flac files to mp3(320Kb) and found the sound comparible with less to no chattering.  I'm not an audio-file, I just built this to be able to reuse old stereo equipment that I own and learn about esphome.

## Example Yaml
```
external_components:
  - source: components

esphome:
  name: media-player-1
  friendly_name: Media Player 1
  platformio_options:
    board_build.flash_mode: qio
    board_upload.maximum_size: 16777216
  libraries:
    - aioesphomeapi=file:///config/aioesphomeapi

esp32:
  board: esp32-s3-devkitc-1
  flash_size: 16MB
  framework:
    type: esp-idf
    version: 4.4.8
    platform_version: 5.4.0
    sdkconfig_options:
      CONFIG_ESP32_S3_BOX_BOARD: "y"

      # below sdkconfig options are scavenged from internet, appears to help with running some internet radio stations.
      #https://github.com/espressif/esp-adf/issues/297
      
      CONFIG_ESP32_DEFAULT_CPU_FREQ_240: "y"
      CONFIG_ESP32_DEFAULT_CPU_FREQ_MHZ: "240"

      #Wi-Fi
      CONFIG_ESP32_WIFI_STATIC_RX_BUFFER_NUM: "16"
      CONFIG_ESP32_WIFI_DYNAMIC_RX_BUFFER_NUM: "512"
      CONFIG_ESP32_WIFI_STATIC_TX_BUFFER: "y"
      CONFIG_ESP32_WIFI_TX_BUFFER_TYPE: "0"
      CONFIG_ESP32_WIFI_STATIC_TX_BUFFER_NUM: "8"
      CONFIG_ESP32_WIFI_CACHE_TX_BUFFER_NUM: "32"
      CONFIG_ESP32_WIFI_CSI_ENABLED: ""
      CONFIG_ESP32_WIFI_AMPDU_TX_ENABLED: "y"
      CONFIG_ESP32_WIFI_TX_BA_WIN: "16"
      CONFIG_ESP32_WIFI_AMPDU_RX_ENABLED: "y"
      CONFIG_ESP32_WIFI_RX_BA_WIN: "32"

      #LWIP/TCP
      CONFIG_TCPIP_RECVMBOX_SIZE: "512"
      CONFIG_LWIP_MAX_ACTIVE_TCP: "16"
      CONFIG_LWIP_MAX_LISTENING_TCP: "16"
      CONFIG_TCP_MAXRTX: "12"
      CONFIG_TCP_SYNMAXRTX: "6"
      CONFIG_TCP_MSS: "1436"
      CONFIG_TCP_MSL: "60000"
      CONFIG_TCP_SND_BUF_DEFAULT: "65535"
      CONFIG_TCP_WND_DEFAULT: "512000"
      CONFIG_TCP_RECVMBOX_SIZE: "512"
      CONFIG_TCP_QUEUE_OOSEQ: "y"
      CONFIG_ESP_TCP_KEEP_CONNECTION_WHEN_IP_CHANGES: ""
      CONFIG_TCP_OVERSIZE_MSS: "y"
      CONFIG_TCP_OVERSIZE_QUARTER_MSS: ""
      CONFIG_TCP_OVERSIZE_DISABLE: ""
      CONFIG_LWIP_WND_SCALE: "y"
      CONFIG_TCP_RCV_SCALE: "3"

psram:
 mode: octal
 speed: 80MHz

# Enable logging
logger:
  hardware_uart : UART0
  level: DEBUG
  logs:
    simple_adf_pipeline: WARN
    esp-idf: ERROR
    HTTPStreamReader: WARN
    i2s_audio: WARN

# Enable Home Assistant API
api:
  encryption:
    key: ""

ota:
  - platform: esphome
    password: ""

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

# time component is not needed if not attempting to use join/unjoin
time:
  - platform: sntp
    id: sntp_time
    timezone: America/New_York
    servers:
     - 0.pool.ntp.org
     - 1.pool.ntp.org
     - 2.pool.ntp.org
  
# define the i2s controller and their pins as before
i2s_audio:
  - id: i2s_out
    # Modify pins based on physical wiring
    i2s_lrclk_pin: GPIO4
    i2s_bclk_pin: GPIO6

audio_media_player:
    name: "Media Player 1"
    dac_type: external
    # Modify pin based on physical wiring
    i2s_dout_pin: GPIO5
    mode: stereo
    #below turns on and off a switch configured in HA, remove if not using.
    on_turn_on:
      then:
        - logger.log: "Turn On Media Player 1"
        - homeassistant.service:
            service: switch.turn_on
            data:
              entity_id: switch.media_player_1_switch
    on_turn_off:
      then:
        - logger.log: "Turn Off Media Player 1"
        - homeassistant.service:
            service: switch.turn_off
            data:
              entity_id: switch.media_player_1_switch
```

## Config Folder Structure (Same folder that contains configuration.yaml for HA)
```
config
  aioesphomeapi
  custom_components
    esphome
  esphome
    components
      api
      audio-media-player
      media-player
    media-player-1.yaml
    media-player-2.yaml
```

## Example m3u file
```
#EXTM3U
#EXTART:ABBA
#EXTALB:ABBA-20th Century Masters The Millennium Collection The Best of ABBA
#EXTINF:165,01.Waterloo
http://192.168.1.47:8000/music/mp3/ABBA/ABBA-20th_Century_Masters_The_Millennium_Collection_The_Best_of_ABBA/01.Waterloo.mp3
#EXTINF:203,02.S.O.S.
http://192.168.1.47:8000/music/mp3/ABBA/ABBA-20th_Century_Masters_The_Millennium_Collection_The_Best_of_ABBA/02.S.O.S..mp3
#EXTINF:197,03.I Do, I Do, I Do, I Do, I Do
http://192.168.1.47:8000/music/mp3/ABBA/ABBA-20th_Century_Masters_The_Millennium_Collection_The_Best_of_ABBA/03.I_Do,_I_Do,_I_Do,_I_Do,_I_Do.mp3
#EXTINF:214,04.Mamma Mia
http://192.168.1.47:8000/music/mp3/ABBA/ABBA-20th_Century_Masters_The_Millennium_Collection_The_Best_of_ABBA/04.Mamma_Mia.mp3
#EXTINF:255,05.Fernando
http://192.168.1.47:8000/music/mp3/ABBA/ABBA-20th_Century_Masters_The_Millennium_Collection_The_Best_of_ABBA/05.Fernando.mp3
#EXTINF:233,06.Dancing Queen
http://192.168.1.47:8000/music/mp3/ABBA/ABBA-20th_Century_Masters_The_Millennium_Collection_The_Best_of_ABBA/06.Dancing_Queen.mp3
#EXTINF:244,07.Knowing Me, Knowing You
http://192.168.1.47:8000/music/mp3/ABBA/ABBA-20th_Century_Masters_The_Millennium_Collection_The_Best_of_ABBA/07.Knowing_Me,_Knowing_You.mp3
#EXTINF:241,08.The Name of the Game
http://192.168.1.47:8000/music/mp3/ABBA/ABBA-20th_Century_Masters_The_Millennium_Collection_The_Best_of_ABBA/08.The_Name_of_the_Game.mp3
#EXTINF:246,09.Take a Chance on Me
http://192.168.1.47:8000/music/mp3/ABBA/ABBA-20th_Century_Masters_The_Millennium_Collection_The_Best_of_ABBA/09.Take_a_Chance_on_Me.mp3
#EXTINF:329,10.Chiquitita
http://192.168.1.47:8000/music/mp3/ABBA/ABBA-20th_Century_Masters_The_Millennium_Collection_The_Best_of_ABBA/10.Chiquitita.mp3
#EXTINF:294,11.The Winner Takes It All
http://192.168.1.47:8000/music/mp3/ABBA/ABBA-20th_Century_Masters_The_Millennium_Collection_The_Best_of_ABBA/11.The_Winner_Takes_It_All.mp3
```

## Internet Radio m3u file
Only a small portion of internet radio stations play correctly, here is an example of a working station:
```
#EXTM3U
#EXTINF:0,Jazz Groove - East
http://east-mp3-128.streamthejazzgroove.com/stream
```

## Play Media Enqueue
* replace - stops current track, clears playlist and plays provided file. If file is not an m3u, player creates a single track playlist.
* add - adds file to end of running playlist, if file is an m3u, then adds each track to the playlist.
* next - adds file to front of running playlist
* play - adds file to end of playlist and restarts.

## Announcements
Announcements are added to a separate "announcements" playlist and current playlist is stopped and all added announcements are played, then current track is restarted.

## Join / Unjoin
There is commented code which is an attempt to synchronize sound accross the group members leveraging multicast. I never could get it to work right. Instead, code attempts to get group members to start each track at the same time as the leader.

Best used when speakers are not in earshot of each other since small variation in play timing is inevitable with this approach.
I've noticed some chatter that I think is caused by my Media Server being overwhelmed by multiple requests of same file.  This may be an incorrect thought.

The knowledge of the group members does not survive a reboot. Here is an example script to set the group members:
```
alias: Media Player 1 Join
sequence:
  - service: media_player.join
    metadata: {}
    data:
      group_members:
        - media_player.media_player_2
    target:
      entity_id: media_player.media_player_1
mode: single
description: ""
```

# Installation
This is how I install, there are other approaches:

1. Clone the following repositories.  For example, I've cloned them to C:\github
```
* C:\github\aioesphomeapi is a clone of https://github.com/rwrozelle/aioesphomeapi
* C:\github\audio-media-player is a clone of https://github.com/rwrozelle/audio-media-player
* C:\github\core is a clone of https://github.com/rwrozelle/core
* C:\github\esphome is a clone of https://github.com/rwrozelle/esphome
```

2. Use Samba share (https://github.com/home-assistant/addons/tree/master/samba) to create a mapped drive (Z:) to the Home Assistant __config__ folder

3. Copy C:\github\aioesphomeapi\aioesphomeapi to Z:\
![image info](./images/aioesphomeapi.PNG)

4. If needed, create Z:\custom_components
5. Copy C:\github\core\homeassistant\components\esphome to Z:\custom_components
![image info](./images/external_components_esphome.PNG)

6. Modify Z:\custom_components\esphome\manifest.json and add:
  ,"version": "1.0.0"
![image info](./images/esphome_manifest.PNG)

7. If needed, create Z:\esphome\components
8. Copy C:\github\audio-media-player\esphome\components\audio_media_player to Z:\esphome\components
9. Copy C:\github\esphome\esphome\components\api to Z:\esphome\components
10. Copy C:\github\esphome\esphome\components\media_player to Z:\esphome\components
![image info](./images/esphome_components.PNG)
11. Restart HA, In the raw log file will be the entry:
```
WARNING (SyncWorker_0) [homeassistant.loader] We found a custom integration esphome which has not been tested by Home Assistant. This component might cause stability problems, be sure to disable it if you experience issues with Home Assistant
```
This means that HA is using code in Z:\custom_components\esphome, not the code that comes with HA Release.

12. Build your ESPHome device using the Example Yaml as a guide.

13. If you want to use a dlna server to play albums or even entire artists, you can:
* Copy C:\github\core\homeassistant\components\dlna_dms to Z:\custom_components
* Copy C:\github\core\homeassistant\components\jellyfin to Z:\custom_components
* Copy C:\github\core\homeassistant\components\media_source to Z:\custom_components
* Modify Z:\custom_components\dlna_dms\manifest.json and add:
  ,"version": "1.0.0"
* Modify Z:\custom_components\jellyfin\manifest.json and add:
  ,"version": "1.0.0"
* Modify Z:\custom_components\media_source\manifest.json and add:
  ,"version": "1.0.0"

This functionality creates a playlist file on the fly, so be patient. 