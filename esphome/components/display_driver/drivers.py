# import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components.display import display_ns
from esphome.components import display
from esphome.util import Registry
from esphome.schema_extractors import schema_extractor_registry

from esphome.const import CONF_MODEL

# from esphome import core, pins
# from esphome.components import display, spi, font
# from esphome.core import CORE, HexInt


DisplayDriver = display_ns.class_("DisplayDriver", display.Display)

DISPLAY_REGISTRY = Registry()
DISPLAY_SCHEMA = cv.Schema({})


def validate_model_registry(base_schema, **kwargs):
    registry_key = kwargs.pop("key", CONF_MODEL)
    default_schema_option = kwargs.pop("default_type", None)

    models = cv.extract_keys(DISPLAY_REGISTRY)
    key_validator = cv.one_of(*models, **kwargs)

    base_schema = cv.ensure_schema(base_schema).extend(
        {
            cv.Optional(registry_key): cv.valid,
        },
        extra=cv.ALLOW_EXTRA,
    )

    @schema_extractor_registry(DISPLAY_REGISTRY)
    def validator(value):
        if not isinstance(value, dict):
            raise cv.Invalid("Value must be dict !!")
        value = value.copy()
        key = value.pop(registry_key, default_schema_option)
        if key is None:
            raise cv.Invalid(f"{registry_key} not specified!")

        key_v = key_validator(key)
        if not isinstance(base_schema, cv.Schema):
            raise cv.Invalid("base_schema must be Schema !!")

        new_schema = base_schema.extend(DISPLAY_REGISTRY[key_v].raw_schema)

        value = new_schema(value)
        value[registry_key] = key_v
        return value

    return validator


async def load_display_driver(model_key):
    registry_item = DISPLAY_REGISTRY[model_key]
    return [registry_item.type_id, registry_item.fun]


@DISPLAY_REGISTRY.register(
    "M5STACK", display_ns.class_("Display_M5Stack", DisplayDriver), cv.Schema({})
)
async def M5STACK_to_code(config, var):
    return [config, var]


@DISPLAY_REGISTRY.register(
    "M5CORE", display_ns.class_("Display_M5CORE", DisplayDriver), cv.Schema({})
)
async def M5CORE_to_code(config, var):
    return [config, var]


@DISPLAY_REGISTRY.register(
    "TFT_2.4", display_ns.class_("Display_ILI9341", DisplayDriver), cv.Schema({})
)
async def TFT_2_4_to_code(config, var):
    return [config, var]


@DISPLAY_REGISTRY.register(
    "TFT_2.4R", display_ns.class_("Display_ILI9342", DisplayDriver), cv.Schema({})
)
async def TFT_2_4R_to_code(config, var):
    return [config, var]


@DISPLAY_REGISTRY.register(
    "ILI9341", display_ns.class_("Display_ILI9341", DisplayDriver), cv.Schema({})
)
async def ILI9341_to_code(config, var):
    return [config, var]


@DISPLAY_REGISTRY.register(
    "ILI9342", display_ns.class_("Display_M5CORE", DisplayDriver), cv.Schema({})
)
async def ILI9342_to_code(config, var):
    return [config, var]


@DISPLAY_REGISTRY.register(
    "ILI9481-18", display_ns.class_("Display_ILI948118", DisplayDriver), cv.Schema({})
)
async def ILI948118_to_code(config, var):
    return [config, var]


@DISPLAY_REGISTRY.register(
    "ILI9486", display_ns.class_("Display_ILI9486", DisplayDriver), cv.Schema({})
)
async def ILI9486_to_code(config, var):
    return [config, var]


@DISPLAY_REGISTRY.register(
    "ILI9488", display_ns.class_("Display_ILI9488", DisplayDriver), cv.Schema({})
)
async def ILI9488_to_code(config, var):
    return [config, var]


@DISPLAY_REGISTRY.register(
    "ILI9488_A", display_ns.class_("Display_ILI9488A", DisplayDriver), cv.Schema({})
)
async def ILI9488A_to_code(config, var):
    return [config, var]


@DISPLAY_REGISTRY.register(
    "ST7796", display_ns.class_("Display_ST7796", DisplayDriver), cv.Schema({})
)
async def ST7796_to_code(config, var):
    return [config, var]


@DISPLAY_REGISTRY.register(
    "S3BOX", display_ns.class_("Display_S3Box", DisplayDriver), cv.Schema({})
)
async def S3Box_to_code(config, var):
    return [config, var]


@DISPLAY_REGISTRY.register(
    "S3BOX_LITE", display_ns.class_("Display_S3BoxLite", DisplayDriver), cv.Schema({})
)
async def S3BoxLite_to_code(config, var):
    return [config, var]


@DISPLAY_REGISTRY.register(
    "RPI_DPI_RGB",
    display_ns.class_("Display_RPI_DPI_RGB", DisplayDriver),
    cv.Schema({}),
)
async def RPI_RGB_to_code(config, var):
    return [config, var]
