
import os
import esphome.codegen as cg
from esphome.components.esp32 import add_idf_component
from esphome.components import media_player, esp32
from esphome.components.esp32 import add_idf_component
import esphome.config_validation as cv

from esphome import pins

from esphome.const import CONF_ID, CONF_MODE

from esphome.components.i2s_audio import (
    I2SAudioComponent,
    I2SAudioOut,
    CONF_I2S_AUDIO_ID,
    CONF_I2S_DOUT_PIN,
    CONF_LEFT,
    CONF_RIGHT,
    CONF_MONO,
    CONF_STEREO,
)
CONF_USE_ADF_ALC = "adf_alc"

CODEOWNERS = ["@rwrozelle","@gnumpi","@jesserockz"]
DEPENDENCIES = ["i2s_audio", "media_player"]
AUTO_LOAD = ["media_player"]

esp_adf_ns = cg.esphome_ns.namespace("esp_adf")

AudioMediaPlayer = esp_adf_ns.class_(
    "AudioMediaPlayer", cg.Component, media_player.MediaPlayer, I2SAudioOut
)

i2s_dac_mode_t = cg.global_ns.enum("i2s_dac_mode_t")

CONF_AUDIO_ID = "audio_id"
CONF_DAC_TYPE = "dac_type"
CONF_I2S_COMM_FMT = "i2s_comm_fmt"

INTERNAL_DAC_OPTIONS = {
    CONF_LEFT: i2s_dac_mode_t.I2S_DAC_CHANNEL_LEFT_EN,
    CONF_RIGHT: i2s_dac_mode_t.I2S_DAC_CHANNEL_RIGHT_EN,
    CONF_STEREO: i2s_dac_mode_t.I2S_DAC_CHANNEL_BOTH_EN,
}

EXTERNAL_DAC_OPTIONS = [CONF_MONO, CONF_STEREO]

NO_INTERNAL_DAC_VARIANTS = [esp32.const.VARIANT_ESP32S2]

I2C_COMM_FMT_OPTIONS = ["lsb", "msb"]


def validate_esp32_variant(config):
    if config[CONF_DAC_TYPE] != "internal":
        return config
    variant = esp32.get_esp32_variant()
    if variant in NO_INTERNAL_DAC_VARIANTS:
        raise cv.Invalid(f"{variant} does not have an internal DAC")
    return config


CONFIG_SCHEMA = cv.All(
    cv.typed_schema(
        {
            "internal": media_player.MEDIA_PLAYER_SCHEMA.extend(
                {
                    cv.GenerateID(): cv.declare_id(AudioMediaPlayer),
                    cv.GenerateID(CONF_I2S_AUDIO_ID): cv.use_id(I2SAudioComponent),
                    cv.Required(CONF_MODE): cv.enum(INTERNAL_DAC_OPTIONS, lower=True),
                }
            ).extend(cv.COMPONENT_SCHEMA),
            "external": media_player.MEDIA_PLAYER_SCHEMA.extend(
                {
                    cv.GenerateID(): cv.declare_id(AudioMediaPlayer),
                    cv.GenerateID(CONF_I2S_AUDIO_ID): cv.use_id(I2SAudioComponent),
                    cv.Required(
                        CONF_I2S_DOUT_PIN
                    ): pins.internal_gpio_output_pin_number,
                    cv.Optional(CONF_MODE, default="mono"): cv.one_of(
                        *EXTERNAL_DAC_OPTIONS, lower=True
                    ),
                    cv.Optional(CONF_I2S_COMM_FMT, default="msb"): cv.one_of(
                        *I2C_COMM_FMT_OPTIONS, lower=True
                    ),
                    cv.Optional(CONF_USE_ADF_ALC, default=True): cv.boolean,
                }
            ).extend(cv.COMPONENT_SCHEMA),
        },
        key=CONF_DAC_TYPE,
    ),
    validate_esp32_variant,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await media_player.register_media_player(var, config)

    await cg.register_parented(var, config[CONF_I2S_AUDIO_ID])

    if config[CONF_DAC_TYPE] == "internal":
        cg.add(var.set_internal_dac_mode(config[CONF_MODE]))
    else:
        cg.add(var.set_dout_pin(config[CONF_I2S_DOUT_PIN]))
        cg.add(var.set_external_dac_channels(2 if config[CONF_MODE] == "stereo" else 1))
        cg.add(var.set_i2s_comm_fmt_lsb(config[CONF_I2S_COMM_FMT] == "lsb"))
        cg.add(var.set_use_adf_alc(config[CONF_USE_ADF_ALC]))

    cg.add_define("USE_ESP_ADF_VAD")
    
    cg.add_platformio_option("build_unflags", "-Wl,--end-group")
    
    cg.add_platformio_option(
        "board_build.embed_txtfiles", "components/dueros_service/duer_profile"
    )

    esp32.add_idf_sdkconfig_option("CONFIG_ESP_TLS_INSECURE", True)
    esp32.add_idf_sdkconfig_option("CONFIG_ESP_TLS_SKIP_SERVER_CERT_VERIFY", True)

    esp32.add_extra_script(
        "pre",
        "apply_adf_patches.py",
        os.path.join(os.path.dirname(__file__), "apply_adf_patches.py.script"),
    )

    esp32.add_extra_build_file(
        "esp_adf_patches/idf_v4.4_freertos.patch",
        "https://github.com/espressif/esp-adf/raw/v2.6/idf_patches/idf_v5.1_freertos.patch",
    )

    esp32.add_idf_component(
        name="esp-adf",
        repo="https://github.com/espressif/esp-adf.git",
        ref="v2.6",
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
   