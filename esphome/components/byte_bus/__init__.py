import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.util import Registry
from esphome.schema_extractors import schema_extractor_registry

from esphome.const import (
    CONF_CLIENT_ID,
)

CODEOWNERS = ["@clydebarrow", "nielsnl68"]

CONF_BUS_TYPE = "bus_type"
CONF_BUS_ID = "bus_id"

byte_bus_ns = cg.esphome_ns.namespace("byte_bus")
ByteBus = byte_bus_ns.class_("ByteBus")

DATABUS_REGISTRY = Registry()


def include_databus(name, bus_class, bus_component, schema, *extra_validators):
    schema = cv.Schema(schema).extend(
        {
            cv.GenerateID(CONF_BUS_ID): cv.use_id(bus_component),
            cv.GenerateID(CONF_CLIENT_ID): cv.declare_id(bus_class),
        }
    )
    validator = cv.All(schema, *extra_validators)

    return DATABUS_REGISTRY.register(name, bus_class, validator)


def validate_databus_registry(base_schema, **kwargs):
    default_schema_option = kwargs.pop("default", None)

    base_schema = cv.ensure_schema(base_schema).extend(
        {
            cv.Optional(CONF_BUS_TYPE): cv.valid,
        },
        extra=cv.ALLOW_EXTRA,
    )

    @schema_extractor_registry(DATABUS_REGISTRY)
    def validator(value):
        if not isinstance(value, dict):
            raise cv.Invalid("This value must be dict !!")
        value = value.copy()
        key = value.pop(CONF_BUS_TYPE, default_schema_option)
        if key is None:
            raise cv.Invalid(f"{CONF_BUS_TYPE} not specified!")

        models = cv.extract_keys(DATABUS_REGISTRY)
        key_validator = cv.one_of(*models, **kwargs)
        key_v = key_validator(key)

        if not isinstance(base_schema, cv.Schema):
            raise cv.Invalid("base_schema must be a schema !!")

        new_schema = base_schema.extend(DATABUS_REGISTRY[key_v].raw_schema)

        value = new_schema(value)
        value[CONF_BUS_TYPE] = key_v
        return value

    return validator


async def register_databus(config):
    databus = DATABUS_REGISTRY[config[CONF_BUS_TYPE]]
    rhs = databus.type_id.new()
    var = cg.Pvariable(config[CONF_CLIENT_ID], rhs)
    cg.add(var.set_parent(await cg.get_variable(config[CONF_BUS_ID])))

    return await databus.fun(config, var)
