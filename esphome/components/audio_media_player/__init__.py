import hashlib
import logging
from pathlib import Path

from esphome import automation, external_files, pins
import os
import esphome.codegen as cg
from esphome.components.esp32 import add_idf_component
from esphome.components import audio, esp32, media_player
import esphome.config_validation as cv
from esphome.const import (
    CONF_FILE,
    CONF_FILES,
    CONF_ID,
    CONF_NAME,
    CONF_PATH,
    CONF_RAW_DATA_ID,
    CONF_TYPE,
    CONF_URL,
)
from esphome.core import CORE, HexInt
from esphome.external_files import download_content

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@rwrozelle"]
# portions of this file based on or copied from: esphome\esphome\components\speaker\media_player
# Embedded Files Sections are direct copy

DEPENDENCIES = ["media_player"]
AUTO_LOAD = ["media_player"]

DOMAIN = "audio_media_player"

TYPE_LOCAL = "local"
TYPE_WEB = "web"

esp_audio_ns = cg.esphome_ns.namespace("esp_audio")

AudioMediaPlayer = esp_audio_ns.class_(
    "AudioMediaPlayer", cg.Component, media_player.MediaPlayer
)

PlayOnDeviceMediaAction = esp_audio_ns.class_(
    "PlayOnDeviceMediaAction",
    automation.Action,
    cg.Parented.template(AudioMediaPlayer),
)

CONF_ANNOUNCEMENT = "announcement"
CONF_ENQUEUE = "enqueue"
CONF_MEDIA_FILE = "media_file"

CONF_I2S_DOUT_PIN = "i2s_dout_pin"
CONF_I2S_MCLK_PIN = "i2s_mclk_pin"
CONF_I2S_BCLK_PIN = "i2s_bclk_pin"
CONF_I2S_LRCLK_PIN = "i2s_lrclk_pin"
CONF_TRANSCODE_ACCESS_TOKEN = "transcode_access_token"
CONF_TRANSCODE_SERVER = "transcode_server"
CONF_TRANSCODE_FORMAT = "transcode_format"
CONF_TRANSCODE_SAMPLE_RATE = "transcode_sample_rate"

CONF_HTTP_STREAM_RB_SIZE = "http_stream_rb_size"
CONF_ESP_DECODER_RB_SIZE = "esp_decoder_rb_size"
CONF_I2S_STREAM_RB_SIZE = "i2s_stream_rb_size"
CONF_PIPELINE_TYPE = "pipeline_type"
CONF_ENABLED_CODECS = "enabled_codecs"

CONF_VOLUME_INCREMENT = "volume_increment"
CONF_VOLUME_MIN = "volume_min"
CONF_VOLUME_MAX = "volume_max"

PIPELINE_TYPE = {
    "SIMPLE": "SIMPLE",
    "COMPLEX": "COMPLEX",
}
AUDIO_FORMAT = {
    "MP3": "mp3",
    "FLAC": "flac",
    "WAV": "wav",
    "NONE": "none",
}
CODECS = {
    "AMRNB",
    "AMRWB",
    "FLAC",
    "OGG",
    "OPUS",
    "MP3",
    "WAV",
    "AAC",
    "M4A",
    "TS",
}

# Embedded Files Section Start
def _compute_local_file_path(value: dict) -> Path:
    url = value[CONF_URL]
    h = hashlib.new("sha256")
    h.update(url.encode())
    key = h.hexdigest()[:8]
    base_dir = external_files.compute_local_file_dir(DOMAIN)
    _LOGGER.debug("_compute_local_file_path: base_dir=%s", base_dir / key)
    return base_dir / key

def _download_web_file(value):
    url = value[CONF_URL]
    path = _compute_local_file_path(value)

    download_content(url, path)
    _LOGGER.debug("download_web_file: path=%s", path)
    return value

def _file_schema(value):
    if isinstance(value, str):
        return _validate_file_shorthand(value)
    return TYPED_FILE_SCHEMA(value)

def _read_audio_file_and_type(file_config):
    conf_file = file_config[CONF_FILE]
    file_source = conf_file[CONF_TYPE]
    if file_source == TYPE_LOCAL:
        path = CORE.relative_config_path(conf_file[CONF_PATH])
    elif file_source == TYPE_WEB:
        path = _compute_local_file_path(conf_file)
    else:
        raise cv.Invalid("Unsupported file source.")

    with open(path, "rb") as f:
        data = f.read()

    import puremagic

    file_type: str = puremagic.from_string(data)
    if file_type.startswith("."):
        file_type = file_type[1:]

    media_file_type = audio.AUDIO_FILE_TYPE_ENUM["NONE"]
    if file_type in ("wav"):
        media_file_type = audio.AUDIO_FILE_TYPE_ENUM["WAV"]
    elif file_type in ("mp3", "mpeg", "mpga"):
        media_file_type = audio.AUDIO_FILE_TYPE_ENUM["MP3"]
    elif file_type in ("flac"):
        media_file_type = audio.AUDIO_FILE_TYPE_ENUM["FLAC"]

    return data, media_file_type

def _validate_file_shorthand(value):
    value = cv.string_strict(value)
    if value.startswith("http://") or value.startswith("https://"):
        return _file_schema(
            {
                CONF_TYPE: TYPE_WEB,
                CONF_URL: value,
            }
        )
    return _file_schema(
        {
            CONF_TYPE: TYPE_LOCAL,
            CONF_PATH: value,
        }
    )
def _validate_supported_local_file(config):
    for file_config in config.get(CONF_FILES, []):
        _, media_file_type = _read_audio_file_and_type(file_config)
        if str(media_file_type) == str(audio.AUDIO_FILE_TYPE_ENUM["NONE"]):
            raise cv.Invalid("Unsupported local media file.")
        if not config[CONF_CODEC_SUPPORT_ENABLED] and str(media_file_type) != str(
            audio.AUDIO_FILE_TYPE_ENUM["WAV"]
        ):
            # Only wav files are supported
            raise cv.Invalid(
                f"Unsupported local media file type, set {CONF_CODEC_SUPPORT_ENABLED} to true or convert the media file to wav"
            )

    return config

LOCAL_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_PATH): cv.file_,
    }
)

WEB_SCHEMA = cv.All(
    {
        cv.Required(CONF_URL): cv.url,
    },
    _download_web_file,
)

TYPED_FILE_SCHEMA = cv.typed_schema(
    {
        TYPE_LOCAL: LOCAL_SCHEMA,
        TYPE_WEB: WEB_SCHEMA,
    },
)

MEDIA_FILE_TYPE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.declare_id(audio.AudioFile),
        cv.Required(CONF_FILE): _file_schema,
        cv.GenerateID(CONF_RAW_DATA_ID): cv.declare_id(cg.uint8),
    }
)
# Embedded Files Section End

def validate_enabled_codecs(value):
    lst = value.split(",")
    for codec in lst:
        strp = cv.string(codec).strip()
        if strp not in CODECS:
            raise cv.Invalid(f"{strp} is not a supported codec: {CODECS}")
    return value

CONFIG_SCHEMA = cv.All(
    media_player.MEDIA_PLAYER_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(AudioMediaPlayer),
            cv.Optional(CONF_PIPELINE_TYPE, default="SIMPLE"): cv.enum(PIPELINE_TYPE),
            cv.Required(CONF_I2S_DOUT_PIN): pins.internal_gpio_output_pin_number,
            cv.Required(CONF_I2S_LRCLK_PIN): pins.internal_gpio_output_pin_number,
            cv.Optional(CONF_I2S_BCLK_PIN): pins.internal_gpio_output_pin_number,
            cv.Optional(CONF_I2S_MCLK_PIN): pins.internal_gpio_output_pin_number,
            cv.Optional(CONF_TRANSCODE_ACCESS_TOKEN, ""): cv.string,
            cv.Optional(CONF_TRANSCODE_SERVER, default="http://homeassistant.local:8123"): cv.string,
            cv.Optional(CONF_TRANSCODE_FORMAT, default="FLAC"): cv.enum(AUDIO_FORMAT),
            #use the CD standard
            cv.Optional(CONF_TRANSCODE_SAMPLE_RATE, default=44100): cv.int_range(min=8000),
            #50 * 20 * 1024
            cv.Optional(CONF_HTTP_STREAM_RB_SIZE, default=1024000): cv.int_range(
                min=4000, max=4000000
            ),
            #10 * 1024
            cv.Optional(CONF_ESP_DECODER_RB_SIZE, default=10240): cv.int_range(
                min=1000, max=100000
            ),
            #8 * 1024
            cv.Optional(CONF_I2S_STREAM_RB_SIZE, default=8192): cv.int_range(
                min=1000, max=100000
            ),
            cv.Optional(CONF_VOLUME_INCREMENT, default=0.05): cv.percentage,
            cv.Optional(CONF_VOLUME_MAX, default=1.0): cv.percentage,
            cv.Optional(CONF_VOLUME_MIN, default=0.0): cv.percentage,
            cv.Optional(CONF_ENABLED_CODECS, default="FLAC"): validate_enabled_codecs,
            cv.Optional(CONF_FILES): cv.ensure_list(MEDIA_FILE_TYPE_SCHEMA),
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
    cg.add(var.set_access_token(config[CONF_TRANSCODE_ACCESS_TOKEN]))
    cg.add(var.set_ffmpeg_server(config[CONF_TRANSCODE_SERVER]))
    cg.add(var.set_format(config[CONF_TRANSCODE_FORMAT]))
    cg.add(var.set_rate(config[CONF_TRANSCODE_SAMPLE_RATE]))
    cg.add(var.set_http_stream_rb_size(config[CONF_HTTP_STREAM_RB_SIZE]))
    cg.add(var.set_esp_decoder_rb_size(config[CONF_ESP_DECODER_RB_SIZE]))
    cg.add(var.set_i2s_stream_rb_size(config[CONF_I2S_STREAM_RB_SIZE]))
    cg.add(var.set_volume_increment(config[CONF_VOLUME_INCREMENT]))
    cg.add(var.set_volume_max(config[CONF_VOLUME_MAX]))
    cg.add(var.set_volume_min(config[CONF_VOLUME_MIN]))
    
    if config[CONF_PIPELINE_TYPE] == "COMPLEX":
        cg.add_define("USE_ADF_COMPLEX_PIPELINE")
    elif config[CONF_PIPELINE_TYPE] == "SIMPLE":
        cg.add_define("USE_ADF_SIMPLE_PIPELINE")
    
    codec_list = config[CONF_ENABLED_CODECS].split(",")
    for codec in codec_list:
        strp = cv.string(codec).strip()
        cg.add_define("USE_" + strp + "_DECODER")
        #USE_AUDIO_FLAC_SUPPORT

# Embedded Files Section Start
    for file_config in config.get(CONF_FILES, []):
        data, media_file_type = _read_audio_file_and_type(file_config)

        rhs = [HexInt(x) for x in data]
        prog_arr = cg.progmem_array(file_config[CONF_RAW_DATA_ID], rhs)

        media_files_struct = cg.StructInitializer(
            audio.AudioFile,
            (
                "data",
                prog_arr,
            ),
            (
                "length",
                len(rhs),
            ),
            (
                "file_type",
                media_file_type,
            ),
        )

        cg.new_Pvariable(
            file_config[CONF_ID],
            media_files_struct,
        )
# Embedded Files Section End

    cg.add_platformio_option("build_unflags", "-Wl, --end-group")
    
    cg.add_platformio_option(
        "board_build.embed_txtfiles", "components/dueros_service/duer_profile"
    )

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
    
@automation.register_action(
    "audio_media_player.play_on_device_media_file",
    PlayOnDeviceMediaAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(AudioMediaPlayer),
            cv.Required(CONF_MEDIA_FILE): cv.use_id(audio.AudioFile),
            cv.Optional(CONF_ANNOUNCEMENT, default=False): cv.templatable(cv.boolean),
            cv.Optional(CONF_ENQUEUE, default=False): cv.templatable(cv.boolean),
        },
        key=CONF_MEDIA_FILE,
    ),
)
async def play_on_device_media_media_action(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    media_file = await cg.get_variable(config[CONF_MEDIA_FILE])
    announcement = await cg.templatable(config[CONF_ANNOUNCEMENT], args, cg.bool_)
    enqueue = await cg.templatable(config[CONF_ENQUEUE], args, cg.bool_)

    cg.add(var.set_audio_file(media_file))
    cg.add(var.set_announcement(announcement))
    cg.add(var.set_enqueue(enqueue))
    return var
