Audio Media Player
==========================
Install Version: ESPHome-2025.3.3, Core: 2025.4.2
------------------------

I have moved on to using Music Assistant.  The only customization I continue to need is the OFf/On capability to turn on an off my external amplifier.
Only use the following GitHub projects if you need Off/On capability.

Github Projects
------------------------
- https://github.com/rwrozelle/core
- - /homeassistant/components/esphome - required, extended for Off/On
- https://github.com/rwrozelle/esphome
- - /esphome/components/api - required, extended media_player for Off/On
- - /esphome/components/media_player - required, extended for Off/On
- - /esphome/components/speaker - required, extended for Off/On
- https://github.com/rwrozelle/aioesphomeapi - required, extended for Off/On

Speaker Audio Media Player (Extended)
==========================
The ESPPhome speaker/media_player with extended Off On capability

Additional Services to speaker/media_player
------------------------
- turn_on - sends action on_turn_on
- turn_off - sends action on_turn_off

Additional Configuration Variables to speaker/media_player
------------------------
- **off_on_enabled** (*Optional* boolean): Enable Turn on and Turn off services including automation. Defaults to ``false``.

Discusion
------------------------
Enabling Off/On is useful for a media player that needs to turn on/turn off an amplifier

Example Configuration
------------------------
Below is stub showing the changes from speaker\media_player documentation.
```
external_components:
  - source: components
...
media_player:
  - platform: speaker
    name: "Speaker Media Player"
    id: speaker_media_player_id
    off_on_enabled: True
...
```

Discusion
------------------------
The extensions are built to solve the following use case:  Be able to turn off and on an external amplifier

Example Configuration
------------------------
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
    version: recommended
    sdkconfig_options:
      CONFIG_ESP32S3_DEFAULT_CPU_FREQ_240: "y"
      CONFIG_ESP32S3_DATA_CACHE_64KB: "y"
      CONFIG_ESP32S3_DATA_CACHE_LINE_64B: "y"
      CONFIG_ESP32S3_INSTRUCTION_CACHE_32KB: "y"

      CONFIG_BT_ALLOCATION_FROM_SPIRAM_FIRST: "y"
      CONFIG_BT_BLE_DYNAMIC_ENV_MEMORY: "y"

      CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC: "y"
      CONFIG_MBEDTLS_SSL_PROTO_TLS1_3: "y"  # TLS1.3 support isn't enabled by default in IDF 5.1.5

psram:
 mode: octal
 speed: 80MHz

# Enable logging
logger:
  level: DEBUG
  logs:
    sensor: WARN  # avoids logging debug sensor updates

# Enable Home Assistant API
api:
  encryption:
    key: "xxxx"

ota:
  - platform: esphome
    password: "yyyy"

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

  # Enable fallback hotspot (captive portal) in case wifi connection fails
  ap:
    ssid: "Media-Player-1 Fallback Hotspot"
    password: "zzzz"

captive_portal:

i2s_audio:
    i2s_lrclk_pin: GPIO4
    i2s_bclk_pin: GPIO6

speaker:
  - platform: i2s_audio
    id: speaker_id
    dac_type: external
    i2s_dout_pin: GPIO5
    sample_rate: 44100
    channel: stereo
    timeout: never
    
  - platform: mixer
    id: mixer_speaker_id
    output_speaker: speaker_id
    source_speakers:
      - id: announcement_spk_mixer_input
        timeout: never
      - id: media_spk_mixer_input
        timeout: never
    
media_player:
  - platform: speaker
    name: "Media Player 1"
    id: media_player_1
    media_pipeline:
        speaker: media_spk_mixer_input
        num_channels: 2
        #format: NONE
    announcement_pipeline:
        speaker: announcement_spk_mixer_input
        num_channels: 2
        
    off_on_enabled: True

    on_announcement:
      - mixer_speaker.apply_ducking:
          id: media_spk_mixer_input
          decibel_reduction: 20
          duration: 0.0s

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
access token was manually placed in secrets.yaml

Installation
------------------------
This is how I install, there are other approaches:

1. Clone the following repositories.  For example, I've cloned them to C:\github
```
C:\github\aioesphomeapi is a clone of https://github.com/rwrozelle/aioesphomeapi
C:\github\core is a clone of https://github.com/rwrozelle/core
C:\github\esphome is a clone of https://github.com/rwrozelle/esphome
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
8. Copy C:\github\esphome\esphome\components\api to Z:\esphome\components
9. Copy C:\github\esphome\esphome\components\media_player to Z:\esphome\components
10. Copy C:\github\esphome\esphome\components\speaker to Z:\esphome\components
11. Restart HA, In the raw log file will be the entry:
```
WARNING (SyncWorker_0) [homeassistant.loader] We found a custom integration esphome which has not been tested by Home Assistant. This component might cause stability problems, be sure to disable it if you experience issues with Home Assistant
```
This means that HA is using code in Z:\custom_components\esphome, not the code that comes with HA Release.

13. Build your ESPHome device using the Example Yaml as a guide.

You should end up with a folder structure that looks similar to this:
```
config
  aioesphomeapi
  custom_components
    esphome
  esphome
    components
      api
      media-player
      speaker
    media-player-1.yaml
    media-player-2.yaml
```
