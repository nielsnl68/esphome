import esphome.config_validation as cv
from esphome.components.display import display_ns
from esphome.components.display_driver import (
    DISPLAY_DRIVER_SCHEMA,
    register_display_driver,
    DisplayDriver,
    register_driver,
)

# from esphome import core, pins
# from esphome.components import display, spi, font
# from esphome.core import CORE, HexInt

CODEOWNERS = ["@nielsnl68", "@clydebarrow"]


@register_driver(
    "M5STACK", display_ns.class_("Display_M5Stack", DisplayDriver), cv.Schema({})
)
async def M5STACK_to_code(config, var):
    return [config, var]


@register_driver(
    "M5CORE", display_ns.class_("Display_M5CORE", DisplayDriver), cv.Schema({})
)
async def M5CORE_to_code(config, var):
    return [config, var]


@register_driver(
    "TFT_2.4", display_ns.class_("Display_ILI9341", DisplayDriver), cv.Schema({})
)
async def TFT_2_4_to_code(config, var):
    return [config, var]


@register_driver(
    "TFT_2.4R", display_ns.class_("Display_ILI9342", DisplayDriver), cv.Schema({})
)
async def TFT_2_4R_to_code(config, var):
    return [config, var]


@register_driver(
    "ILI9341", display_ns.class_("Display_ILI9341", DisplayDriver), cv.Schema({})
)
async def ILI9341_to_code(config, var):
    return [config, var]


@register_driver(
    "ILI9342", display_ns.class_("Display_M5CORE", DisplayDriver), cv.Schema({})
)
async def ILI9342_to_code(config, var):
    return [config, var]


@register_driver(
    "ILI9481-18", display_ns.class_("Display_ILI948118", DisplayDriver), cv.Schema({})
)
async def ILI948118_to_code(config, var):
    return [config, var]


@register_driver(
    "ILI9486", display_ns.class_("Display_ILI9486", DisplayDriver), cv.Schema({})
)
async def ILI9486_to_code(config, var):
    return [config, var]


@register_driver(
    "ILI9488", display_ns.class_("Display_ILI9488", DisplayDriver), cv.Schema({})
)
async def ILI9488_to_code(config, var):
    return [config, var]


@register_driver(
    "ILI9488_A", display_ns.class_("Display_ILI9488A", DisplayDriver), cv.Schema({})
)
async def ILI9488A_to_code(config, var):
    return [config, var]


@register_driver(
    "ST7796", display_ns.class_("Display_ST7796", DisplayDriver), cv.Schema({})
)
async def ST7796_to_code(config, var):
    return [config, var]


@register_driver(
    "S3BOX", display_ns.class_("Display_S3Box", DisplayDriver), cv.Schema({})
)
async def S3Box_to_code(config, var):
    return [config, var]


@register_driver(
    "S3BOX_LITE", display_ns.class_("Display_S3BoxLite", DisplayDriver), cv.Schema({})
)
async def S3BoxLite_to_code(config, var):
    return [config, var]


@register_driver(
    "RPI_DPI_RGB",
    display_ns.class_("Display_RPI_DPI_RGB", DisplayDriver),
    cv.Schema({}),
)
async def RPI_RGB_to_code(config, var):
    return [config, var]


CONFIG_SCHEMA = DISPLAY_DRIVER_SCHEMA


async def to_code(config):
    await register_display_driver(config)
