import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import core, pins
from esphome.components import display, spi, font
from esphome.components.display import validate_rotation, display_ns
from esphome.core import CORE, HexInt
from esphome.util import Registry
from esphome.schema_extractors import schema_extractor_registry
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
    CONF_OUTPUT,
    CONF_DATA_PINS,
)

DEPENDENCIES = ["spi"]

CODEOWNERS = ["@nielsnl68", "@clydebarrow"]

IS_PLATFORM_COMPONENT = True


CONF_COLOR_PALETTE_ENUM = cv.one_of("NONE", "GRAYSCALE", "IMAGE_ADAPTIVE")
CONF_COLOR_PALETTE_ID = "color_palette_id"
CONF_COLOR_PALETTE_IMAGES = "color_palette_images"
CONF_INTERFACE = "interface"
CONF_18BIT_MODE = "18bit_mode"

CONF_BUS_ID = "bus_id"
CONF_DE_PIN = "de_pin"

CONF_PCLK_PIN = "pclk_pin"
CONF_PCLK_FREQUENCY = "pclk_frequency"
CONF_PCLK_INVERTED = "pclk_inverted"

CONF_HSYNC_PIN = "hsync_pin"
CONF_HSYNC_PULSE_WIDTH = "hsync_pulse_width"
CONF_HSYNC_FRONT_PORCH = "hsync_front_porch"
CONF_HSYNC_BACK_PORCH = "hsync_back_porch"

CONF_VSYNC_PIN = "vsync_pin"
CONF_VSYNC_PULSE_WIDTH = "vsync_pulse_width"
CONF_VSYNC_FRONT_PORCH = "vsync_front_porch"
CONF_VSYNC_BACK_PORCH = "vsync_back_porch"


def AUTO_LOAD():
    if CORE.is_esp32:
        return ["psram", "display"]
    return ["display"]


displayInterface = display_ns.class_("displayInterface")
SPI_Interface = display_ns.class_("SPIBus", displayInterface)
SPI16D_Interface = display_ns.class_("SPI16DBus", displayInterface)
RGB_Interface = display_ns.class_("RGBBus", SPI_Interface)


ColorMode = display_ns.enum("ColorMode")

ColorOrder = display.display_ns.enum("ColorMode")
COLOR_ORDERS = {
    "RGB": ColorOrder.COLOR_ORDER_RGB,
    "BGR": ColorOrder.COLOR_ORDER_BGR,
}


DisplayDriver = display_ns.class_("DisplayDriver", display.Display)

DISPLAY_REGISTRY = Registry()
DISPLAY_SCHEMA = cv.Schema({})


def validate_model_registry(base_schema, **kwargs):
    registry_key = kwargs.pop("key", CONF_MODEL)
    default_schema_option = kwargs.pop("default_type", None)

    base_schema = cv.ensure_schema(base_schema).extend(
        {
            cv.Optional(registry_key): cv.valid,
        },
        extra=cv.ALLOW_EXTRA,
    )

    @schema_extractor_registry(DISPLAY_REGISTRY)
    def validator(value):
        if not isinstance(value, dict):
            raise cv.Invalid("This value must be dict !!")
        value = value.copy()
        key = value.pop(registry_key, default_schema_option)
        if key is None:
            raise cv.Invalid(f"{registry_key} not specified!")

        models = cv.extract_keys(DISPLAY_REGISTRY)
        key_validator = cv.one_of(*models, **kwargs)
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


def register_driver(name, condition_type, schema):
    return DISPLAY_REGISTRY.register(name, condition_type, schema)


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


DATA_PIN_SCHEMA = pins.gpio_pin_schema(
    {
        CONF_OUTPUT: True,
    },
    internal=True,
)

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
        "DPI_RGB": cv.All(
            cv.Schema(
                {
                    cv.Required(CONF_DATA_PINS): cv.All(
                        [DATA_PIN_SCHEMA],
                        cv.Length(min=16, max=16, msg="Exactly 16 data pins required"),
                    ),
                    cv.Required(CONF_DE_PIN): pins.internal_gpio_output_pin_schema,
                    cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
                    cv.Required(CONF_PCLK_PIN): pins.internal_gpio_output_pin_schema,
                    cv.Optional(CONF_PCLK_FREQUENCY, default="16MHz"): cv.All(
                        cv.frequency, cv.Range(min=4e6, max=30e6)
                    ),
                    cv.Optional(CONF_PCLK_INVERTED, default=True): cv.boolean,
                    cv.Required(CONF_HSYNC_PIN): pins.internal_gpio_output_pin_schema,
                    cv.Optional(CONF_HSYNC_PULSE_WIDTH, default=10): cv.int_,
                    cv.Optional(CONF_HSYNC_BACK_PORCH, default=10): cv.int_,
                    cv.Optional(CONF_HSYNC_FRONT_PORCH, default=20): cv.int_,
                    cv.Required(CONF_VSYNC_PIN): pins.internal_gpio_output_pin_schema,
                    cv.Optional(CONF_VSYNC_PULSE_WIDTH, default=10): cv.int_,
                    cv.Optional(CONF_VSYNC_BACK_PORCH, default=10): cv.int_,
                    cv.Optional(CONF_VSYNC_FRONT_PORCH, default=10): cv.int_,
                },
            ),
            cv.only_with_esp_idf,
        ),
    },
    default_type="SPI",
    upper=True,
)


DISPLAY_DRIVER_SCHEMA = cv.All(
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


async def register_display_iobus(config, var):
    bus_config = config[CONF_INTERFACE]
    bus = cg.new_Pvariable(bus_config[CONF_BUS_ID])
    cg.add(var.set_interface(bus))

    if bus_config[CONF_TYPE] == "SPI":
        await spi.register_spi_device(bus, bus_config)
        dc = await cg.gpio_pin_expression(bus_config[CONF_DC_PIN])
        cg.add(bus.set_dc_pin(dc))

    elif bus_config[CONF_TYPE] == "SPI16D":
        await spi.register_spi_device(bus, bus_config)
        dc = await cg.gpio_pin_expression(bus_config[CONF_DC_PIN])
        cg.add(bus.set_dc_pin(dc))

    elif bus_config[CONF_TYPE] == "DPI_RGB":
        pin = await cg.gpio_pin_expression(config[CONF_HSYNC_PIN])
        cg.add(bus.set_hsync_pin(pin))
        cg.add(bus.set_hsync_pulse_width(config[CONF_HSYNC_PULSE_WIDTH]))
        cg.add(bus.set_hsync_back_porch(config[CONF_HSYNC_BACK_PORCH]))
        cg.add(bus.set_hsync_front_porch(config[CONF_HSYNC_FRONT_PORCH]))

        pin = await cg.gpio_pin_expression(config[CONF_VSYNC_PIN])
        cg.add(bus.set_vsync_pin(pin))
        cg.add(bus.set_vsync_pulse_width(config[CONF_VSYNC_PULSE_WIDTH]))
        cg.add(bus.set_vsync_back_porch(config[CONF_VSYNC_BACK_PORCH]))
        cg.add(bus.set_vsync_front_porch(config[CONF_VSYNC_FRONT_PORCH]))

        pin = await cg.gpio_pin_expression(config[CONF_PCLK_PIN])
        cg.add(bus.set_pclk_pin(pin))
        cg.add(bus.set_pclk_inverted(config[CONF_PCLK_INVERTED]))
        cg.add(bus.set_pclk_frequency(config[CONF_PCLK_FREQUENCY]))

        index = 0
        for pin in config[CONF_DATA_PINS]:
            data_pin = await cg.gpio_pin_expression(pin)
            cg.add(bus.add_data_pin(data_pin, index))
            index += 1

        pin = await cg.gpio_pin_expression(config[CONF_DE_PIN])
        cg.add(bus.set_de_pin(pin))


async def register_display_driver(config):
    (type_id, function_) = await load_display_driver(config[CONF_MODEL])

    rhs = type_id.new()

    var = cg.Pvariable(config[CONF_ID], rhs)
    await display.register_display(var, config)

    (config, var) = await function_(config, var)

    await register_display_iobus(config, var)

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
