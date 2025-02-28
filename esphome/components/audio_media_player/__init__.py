from esphome import pins
import os
import esphome.codegen as cg
from esphome.components.esp32 import add_idf_component
from esphome.components import esp32, media_player
from esphome.components.esp32 import add_idf_component
import esphome.config_validation as cv

from esphome import pins

from esphome.const import (
    CONF_FORMAT,
    CONF_ID,
    CONF_NAME,
    CONF_SAMPLE_RATE,
)
CODEOWNERS = ["@rwrozelle"]
DEPENDENCIES = ["media_player"]
AUTO_LOAD = ["media_player"]

esp_adf_ns = cg.esphome_ns.namespace("esp_adf")

AudioMediaPlayer = esp_adf_ns.class_(
    "AudioMediaPlayer", cg.Component, media_player.MediaPlayer
)

CONF_I2S_DOUT_PIN = "i2s_dout_pin"
CONF_I2S_MCLK_PIN = "i2s_mclk_pin"
CONF_I2S_BCLK_PIN = "i2s_bclk_pin"
CONF_I2S_LRCLK_PIN = "i2s_lrclk_pin"
CONF_ACCESS_TOKEN = "access_token"
CONF_FFMPEG_SERVER = "ffmpeg_server"

AUDIO_FORMAT = {
    "MP3": "mp3",
    "FLAC": "flac",
    "WAV": "wav",
    "NONE": "none",
}
        
CONFIG_SCHEMA = cv.All(
    media_player.MEDIA_PLAYER_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(AudioMediaPlayer),
            cv.Required(CONF_I2S_DOUT_PIN): pins.internal_gpio_output_pin_number,
            cv.Required(CONF_I2S_LRCLK_PIN): pins.internal_gpio_output_pin_number,
            cv.Optional(CONF_I2S_BCLK_PIN): pins.internal_gpio_output_pin_number,
            cv.Optional(CONF_I2S_MCLK_PIN): pins.internal_gpio_output_pin_number,
            cv.Optional(CONF_ACCESS_TOKEN, ""): cv.string,
            cv.Optional(CONF_FFMPEG_SERVER, default="http://homeassistant.local:8123"): cv.string,
            cv.Optional(CONF_FORMAT, default="FLAC"): cv.enum(AUDIO_FORMAT),
            cv.Optional(CONF_SAMPLE_RATE): cv.int_range(min=8000),
        }
    ),
    cv.only_with_esp_idf,
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await media_player.register_media_player(var, config)
    
    cg.add(var.set_dout_pin(config[CONF_I2S_DOUT_PIN]))
    cg.add(var.set_lrclk_pin(config[CONF_I2S_LRCLK_PIN]))
    if CONF_I2S_BCLK_PIN in config:
        cg.add(var.set_bclk_pin(config[CONF_I2S_BCLK_PIN]))
    if CONF_I2S_MCLK_PIN in config:
        cg.add(var.set_mclk_pin(config[CONF_I2S_MCLK_PIN]))
    if CONF_ACCESS_TOKEN in config:
        cg.add(var.set_access_token(config[CONF_ACCESS_TOKEN]))
    if CONF_FFMPEG_SERVER in config:
        cg.add(var.set_ffmpeg_server(config[CONF_FFMPEG_SERVER]))
    if CONF_FORMAT in config:
        cg.add(var.set_format(config[CONF_FORMAT]))
    if CONF_SAMPLE_RATE in config:
        cg.add(var.set_rate(config[CONF_SAMPLE_RATE]))

    cg.add_define("USE_ESP_ADF_VAD")
    
    cg.add_platformio_option("build_unflags", "-Wl,--end-group")
    
    cg.add_platformio_option(
        "board_build.embed_txtfiles", "components/dueros_service/duer_profile"
    )

    esp32.add_idf_sdkconfig_option("CONFIG_ESP_TLS_INSECURE", True)
    esp32.add_idf_sdkconfig_option("CONFIG_ESP_TLS_SKIP_SERVER_CERT_VERIFY", True)

    esp32.add_extra_build_file(
        "esp_adf_patches/idf_v5.1_freertos.patch",
        "https://github.com/espressif/esp-adf/raw/v2.7/idf_patches/idf_v5.1_freertos.patch",
    )

    esp32.add_extra_script(
        "pre",
        "apply_adf_patches.py",
        os.path.join(os.path.dirname(__file__), "apply_adf_patches.py.script"),
    )

    esp32.add_idf_component(
        name="esp-adf",
        repo="https://github.com/espressif/esp-adf.git",
        ref="v2.7",
        path="components",
        submodules=["components/esp-adf-libs", "components/esp-sr"],
        components=[
            "audio_pipeline",
            "audio_sal",
            "esp-adf-libs",
            "esp-sr",
            "dueros_service",
            "clouds",
            "audio_stream",
            "audio_board",
            "esp_peripherals",
            "audio_hal",
            "display_service",
            "esp_dispatcher",
            "esp_actions",
            "wifi_service",
            "audio_recorder",
            "tone_partition",
        ],
    )
   