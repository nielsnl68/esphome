import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import core, pins
from esphome.components import display, spi, font
from esphome.components.display import validate_rotation, display_ns
from esphome.core import CORE, HexInt

from esphome.const import (
    CONF_COLOR_PALETTE,
    CONF_DC_PIN,
    CONF_ID,
    CONF_LAMBDA,
    CONF_MODEL,
    CONF_PAGES,
    CONF_RESET_PIN,
    CONF_ROTATION,
    CONF_MIRROR_X,
    CONF_MIRROR_Y,
    CONF_SWAP_XY,
    CONF_COLOR_ORDER,
    CONF_TRANSFORM,
    CONF_INVERT_COLORS,
    CONF_TYPE,
)
from .drivers import validate_model_registry, load_display_driver, DisplayDriver

DEPENDENCIES = ["spi"]


def AUTO_LOAD():
    if CORE.is_esp32:
        return ["psram"]
    return []


CODEOWNERS = ["@nielsnl68", "@clydebarrow"]

CONF_COLOR_PALETTE_ENUM = cv.one_of("NONE", "GRAYSCALE", "IMAGE_ADAPTIVE")
CONF_COLOR_PALETTE_ID = "color_palette_id"
CONF_COLOR_PALETTE_IMAGES = "color_palette_images"
CONF_INTERFACE = "interface"
CONF_18BIT_MODE = "18bit_mode"

displayInterface = display_ns.class_("displayInterface")
SPI_Interface = display_ns.class_("SPI_Interface", displayInterface)
SPI16D_Interface = display_ns.class_("SPI16D_Interface", displayInterface)

ColorMode = display_ns.enum("ColorMode")

ColorOrder = display.display_ns.enum("ColorMode")
COLOR_ORDERS = {
    "RGB": ColorOrder.COLOR_ORDER_RGB,
    "BGR": ColorOrder.COLOR_ORDER_BGR,
}


def _validate(config):
    if config.get(CONF_COLOR_PALETTE) == "IMAGE_ADAPTIVE" and not config.get(
        CONF_COLOR_PALETTE_IMAGES
    ):
        raise cv.Invalid(
            "Color palette in IMAGE_ADAPTIVE mode requires at least one 'color_palette_images' entry to generate palette"
        )
    if (
        config.get(CONF_COLOR_PALETTE_IMAGES)
        and config.get(CONF_COLOR_PALETTE) != "IMAGE_ADAPTIVE"
    ):
        raise cv.Invalid(
            "Providing color palette images requires palette mode to be 'IMAGE_ADAPTIVE'"
        )
    if CORE.is_esp8266 and config.get(CONF_MODEL) not in [
        "M5STACK",
        "TFT_2.4",
        "TFT_2.4R",
        "ILI9341",
        "ILI9342",
        "ST7789V",
    ]:
        raise cv.Invalid(
            "Provided model can't run on ESP8266. Use an ESP32 with PSRAM onboard"
        )
    return config


CONF_BUS_ID = "bus_id"


INTERFACE_SCHEMA = cv.typed_schema(
    {
        "SPI": spi.spi_device_schema(False, "40MHz").extend(
            {
                cv.GenerateID(CONF_BUS_ID): cv.declare_id(SPI_Interface),
                cv.Required(CONF_DC_PIN): pins.gpio_output_pin_schema,
            }
        ),
        "SPI16D": spi.spi_device_schema(False, "40MHz").extend(
            {
                cv.GenerateID(CONF_BUS_ID): cv.declare_id(SPI16D_Interface),
                cv.Required(CONF_DC_PIN): pins.gpio_output_pin_schema,
            }
        ),
    },
    default_type="SPI",
    upper=True,
)


CONFIG_SCHEMA = cv.All(
    font.validate_pillow_installed,
    validate_model_registry(
        display.FULL_DISPLAY_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(DisplayDriver),
                cv.Required(CONF_INTERFACE): INTERFACE_SCHEMA,
                cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
                cv.Optional(
                    CONF_COLOR_PALETTE, default="NONE"
                ): CONF_COLOR_PALETTE_ENUM,
                cv.GenerateID(CONF_COLOR_PALETTE_ID): cv.declare_id(cg.uint8),
                cv.Optional(CONF_COLOR_PALETTE_IMAGES, default=[]): cv.ensure_list(
                    cv.file_
                ),
                cv.Optional(CONF_INVERT_COLORS): cv.boolean,
                cv.Optional(CONF_18BIT_MODE): cv.boolean,
                cv.Optional(CONF_COLOR_ORDER): cv.one_of(
                    *COLOR_ORDERS.keys(), upper=True
                ),
                cv.Exclusive(CONF_ROTATION, CONF_ROTATION): validate_rotation,
                cv.Exclusive(CONF_TRANSFORM, CONF_ROTATION): cv.Schema(
                    {
                        cv.Optional(CONF_SWAP_XY, default=False): cv.boolean,
                        cv.Optional(CONF_MIRROR_X, default=False): cv.boolean,
                        cv.Optional(CONF_MIRROR_Y, default=False): cv.boolean,
                    }
                ),
            }
        ).extend(cv.polling_component_schema("1s")),
        upper=True,
        space="_",
    ),
    cv.has_at_most_one_key(CONF_PAGES, CONF_LAMBDA),
    _validate,
)


async def to_code(config):
    (type_id, function_) = await load_display_driver(config[CONF_MODEL])

    rhs = type_id.new()

    var = cg.Pvariable(config[CONF_ID], rhs)
    await display.register_display(var, config)
    bus_config = config[CONF_INTERFACE]

    (config, var) = await function_(config, var)

    bus = cg.new_Pvariable(bus_config[CONF_BUS_ID])
    cg.add(var.set_interface(bus))

    if bus_config[CONF_TYPE] == "SPI":
        await spi.register_spi_device(bus, bus_config)
        dc = await cg.gpio_pin_expression(bus_config[CONF_DC_PIN])
        cg.add(bus.set_dc_pin(dc))

    if bus_config[CONF_TYPE] == "SPI16D":
        await spi.register_spi_device(bus, bus_config)
        dc = await cg.gpio_pin_expression(bus_config[CONF_DC_PIN])
        cg.add(bus.set_dc_pin(dc))

    if CONF_COLOR_ORDER in config:
        cg.add(var.set_color_order(COLOR_ORDERS[config[CONF_COLOR_ORDER]]))
    if CONF_TRANSFORM in config:
        transform = config[CONF_TRANSFORM]
        cg.add(var.set_swap_xy(transform[CONF_SWAP_XY]))
        cg.add(var.set_mirror_x(transform[CONF_MIRROR_X]))
        cg.add(var.set_mirror_y(transform[CONF_MIRROR_Y]))

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(display.DisplayRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))

    if CONF_RESET_PIN in config:
        reset = await cg.gpio_pin_expression(config[CONF_RESET_PIN])
        cg.add(var.set_reset_pin(reset))

    rhs = None
    if config[CONF_COLOR_PALETTE] == "GRAYSCALE":
        rhs = []
        for x in range(256):
            rhs.extend([HexInt(x), HexInt(x), HexInt(x)])
    elif config[CONF_COLOR_PALETTE] == "IMAGE_ADAPTIVE":
        from PIL import Image

        def load_image(filename):
            path = CORE.relative_config_path(filename)
            try:
                return Image.open(path)
            except Exception as e:
                raise core.EsphomeError(f"Could not load image file {path}: {e}")

        # make a wide horizontal combined image.
        images = [load_image(x) for x in config[CONF_COLOR_PALETTE_IMAGES]]
        total_width = sum(i.width for i in images)
        max_height = max(i.height for i in images)

        ref_image = Image.new("RGB", (total_width, max_height))
        x = 0
        for i in images:
            ref_image.paste(i, (x, 0))
            x = x + i.width

        # reduce the colors on combined image to 256.
        converted = ref_image.convert("P", palette=Image.Palette.ADAPTIVE, colors=256)
        # if you want to verify how the images look use
        # ref_image.save("ref_in.png")
        # converted.save("ref_out.png")
        palette = converted.getpalette()
        assert len(palette) == 256 * 3
        rhs = palette

    if rhs is not None:
        prog_arr = cg.progmem_array(config[CONF_COLOR_PALETTE_ID], rhs)
        cg.add(var.set_palette(prog_arr))

    if CONF_INVERT_COLORS in config:
        cg.add(var.invert_colors(config[CONF_INVERT_COLORS]))

    if CONF_18BIT_MODE in config:
        cg.add(var.set_18bit_mode(config[CONF_18BIT_MODE]))
