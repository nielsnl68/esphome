import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.util import Registry
from esphome.schema_extractors import schema_extractor_registry

CODEOWNERS = ["@clydebarrow", "nielsnl68"]

CONF_BUS_TYPE = "bus_type"

byte_bus_ns = cg.esphome_ns.namespace("byte_bus")
ByteBus = byte_bus_ns.class_("ByteBus")


DATABUS_REGISTRY = Registry()


def validate_bytebus_registry(base_schema, **kwargs):
    registry_key = kwargs.pop("key", CONF_BUS_TYPE)
    default_schema_option = kwargs.pop("default_type", None)

    base_schema = cv.ensure_schema(base_schema).extend(
        {
            cv.Optional(registry_key): cv.valid,
        },
        extra=cv.ALLOW_EXTRA,
    )

    @schema_extractor_registry(DATABUS_REGISTRY)
    def validator(value):
        if not isinstance(value, dict):
            raise cv.Invalid("This value must be dict !!")
        value = value.copy()
        key = value.pop(registry_key, default_schema_option)
        if key is None:
            raise cv.Invalid(f"{registry_key} not specified!")

        models = cv.extract_keys(DATABUS_REGISTRY)
        key_validator = cv.one_of(*models, **kwargs)
        key_v = key_validator(key)

        if not isinstance(base_schema, cv.Schema):
            raise cv.Invalid("base_schema must be a schema !!")

        new_schema = base_schema.extend(DATABUS_REGISTRY[key_v].raw_schema)

        value = new_schema(value)
        value[registry_key] = key_v
        return value

    return validator


async def load_display_driver(key):
    registry_item = DATABUS_REGISTRY[key]
    return [registry_item.type_id, registry_item.fun]


def register_databus(name, condition_type, schema):
    return DATABUS_REGISTRY.register(name, condition_type, schema)
