"""On-device advertisement filtering for ESPHome's bluetooth_proxy.

Installs a predicate into the proxy's AdvertisementFilter slot (added in
esphome/esphome#19220), so advertisements are dropped on the device instead of
being forwarded for Home Assistant to discard.
"""

from esphome import automation
import esphome.codegen as cg
from esphome.components import bluetooth_proxy
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import ID, MACAddress
from esphome.cpp_generator import MockObj, TemplateArgsType
from esphome.types import ConfigType

# Read by the `component_version` text_sensor platform, if the user adds one.
# A plain constant rather than a registration call, so this component needs no
# dependency on it and there is no codegen ordering to get wrong.
COMPONENT_VERSION = "2.0.2"

DEPENDENCIES = ["bluetooth_proxy"]
CODEOWNERS = ["@davidcoulson"]

ble_advert_filter_ns = cg.esphome_ns.namespace("ble_advert_filter")
BLEAdvertFilter = ble_advert_filter_ns.class_("BLEAdvertFilter", cg.Component)
SetIrksAction = ble_advert_filter_ns.class_(
    "SetIrksAction", automation.Action, cg.Parented.template(BLEAdvertFilter)
)
ClearIrksAction = ble_advert_filter_ns.class_(
    "ClearIrksAction", automation.Action, cg.Parented.template(BLEAdvertFilter)
)

# How the sensor and number platforms name the filter they read.
CONF_BLE_ADVERT_FILTER_ID = "ble_advert_filter_id"

CONF_BLUETOOTH_PROXY_ID = "bluetooth_proxy_id"
CONF_RSSI_THRESHOLD = "rssi_threshold"
CONF_RSSI_FLOOR = "rssi_floor"
CONF_RSSI_MAC_ALLOWLIST = "rssi_mac_allowlist"
CONF_RSSI_IRK = "rssi_irk"
CONF_RSSI_SERVICE_UUID = "rssi_service_uuid"
CONF_IRKS = "irks"
CONF_ALLOW_ESPRESSIF = "allow_espressif"
CONF_ALLOW_HOMEKIT = "allow_homekit"
CONF_ALLOW_IBEACON = "allow_ibeacon"
CONF_ALLOW_FINDMY = "allow_findmy"
CONF_UUID = "uuid"
CONF_MAJOR = "major"
CONF_MINOR = "minor"
CONF_RSSI = "rssi"
CONF_DROP_NON_RESOLVABLE = "drop_non_resolvable"
CONF_NAME_BLOCKLIST = "name_blocklist"
CONF_MAC_ALLOWLIST = "mac_allowlist"
CONF_MAC_BLOCKLIST = "mac_blocklist"
CONF_ALLOWLIST_EXCLUSIVE = "allowlist_exclusive"
CONF_MANUFACTURER_BLOCKLIST = "manufacturer_blocklist"
CONF_SERVICE_UUID_ALLOWLIST = "service_uuid_allowlist"


def _mac_address(value: str | MACAddress) -> MACAddress:
    """`cv.mac_address`, but safe to run twice (str -> MACAddress is not idempotent)."""
    if isinstance(value, MACAddress):
        return value
    return cv.mac_address(value)


def _validate_irk(value: str) -> str:
    """One 16-byte Identity Resolving Key, as 32 hex chars.

    Accepts the separator styles people paste and normalises to bare lowercase
    hex; the C++ side parses the concatenated blob once at boot.
    """
    value = cv.string_strict(value)
    stripped = value.translate(str.maketrans("", "", ":- ")).lower()
    if len(stripped) != 32:
        raise cv.Invalid(
            f"IRK must be 16 bytes (32 hex characters), got {len(stripped)}"
        )
    if any(c not in "0123456789abcdef" for c in stripped):
        raise cv.Invalid("IRK must be hexadecimal")
    return stripped


def _validate_service_uuid(value: int | str) -> int | str:
    """A 16-bit short (0xFFF6) or a full 128-bit UUID."""
    if isinstance(value, int):
        return cv.hex_uint16_t(value)
    text = cv.string_strict(value).strip()
    stripped = text.translate(str.maketrans("", "", ":- ")).lower()
    if len(stripped) == 32:
        if any(c not in "0123456789abcdef" for c in stripped):
            raise cv.Invalid("128-bit service UUID must be hexadecimal")
        return stripped
    return cv.hex_uint16_t(value)


# One entry of the allow_ibeacon list. `minor` accepts a single value or a list;
# omitting it exempts the whole major. Nested rather than flat
# ibeacon_major/ibeacon_minor keys because a minor is only meaningful inside a
# major - flat keys cannot express "major 1 minor 7 AND major 10 minor 3".
# Mirrors BLEAdvertFilter::IBEACON_RSSI_INHERIT.
_IBEACON_RSSI_INHERIT = -128


def _validate_ibeacon_uuid(value: str) -> str:
    """An iBeacon proximity UUID, normalised to 32 lowercase hex characters."""
    return cv.uuid(value).hex


def _validate_ibeacon_rule(rule: ConfigType) -> ConfigType:
    if CONF_UUID not in rule and CONF_MAJOR not in rule:
        raise cv.Invalid(
            f"An allow_ibeacon rule needs a '{CONF_UUID}', a '{CONF_MAJOR}', or "
            f"both. To exempt every iBeacon, use 'allow_ibeacon: true'."
        )
    if CONF_MINOR in rule and CONF_MAJOR not in rule:
        raise cv.Invalid(
            f"'{CONF_MINOR}' is only meaningful inside a '{CONF_MAJOR}'",
            path=[CONF_MINOR],
        )
    return rule


_IBEACON_FILTER_SCHEMA = cv.All(
    cv.Schema(
        {
            # Whose beacon it is. major/minor are small integers every vendor
            # starts counting from 1, so a rule without a uuid admits anybody's
            # beacon that was left on its defaults - at this rule's RSSI limit.
            cv.Optional(CONF_UUID): _validate_ibeacon_uuid,
            cv.Optional(CONF_MAJOR): cv.uint16_t,
            # At least one: an empty list would silently mean "any minor".
            cv.Optional(CONF_MINOR): cv.All(
                cv.ensure_list(cv.uint16_t), cv.Length(min=1)
            ),
            # This filter's own RSSI limit, overriding BOTH rssi_threshold and
            # rssi_floor for adverts it matches. Omitted = inherit: the distance
            # rules still apply. Either way a matched iBeacon is protected like
            # an allowlisted device: exempt from manufacturer_blocklist,
            # name_blocklist, drop_non_resolvable and the unresolved-RPA drop.
            # -127 forwards at any strength.
            cv.Optional(CONF_RSSI, default=_IBEACON_RSSI_INHERIT): cv.Any(
                cv.int_range(min=-127, max=0),
                cv.int_range(min=_IBEACON_RSSI_INHERIT, max=_IBEACON_RSSI_INHERIT),
            ),
        }
    ),
    _validate_ibeacon_rule,
)


# allow_findmy: `true` protects Apple FindMy adverts (Offline Finding subtype
# 0x12, and AirPods proximity-pairing subtype 0x07, which AirPods near their
# owner send from the same rotated address) like an allowlisted device: exempt
# from manufacturer_blocklist, name_blocklist, drop_non_resolvable and the
# unresolved-RPA drop. A mapping with `rssi` gives the rule its own limit, resolved
# exactly like an iBeacon rule's (overrides rssi_threshold and rssi_floor for
# the adverts it matches).
_FINDMY_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_RSSI, default=_IBEACON_RSSI_INHERIT): cv.Any(
            cv.int_range(min=_IBEACON_RSSI_INHERIT, max=_IBEACON_RSSI_INHERIT),
            cv.int_range(min=-127, max=0),
        ),
    }
)


def _validate_allow_findmy(value: bool | ConfigType) -> bool | ConfigType:
    if isinstance(value, bool):
        return value
    return _FINDMY_SCHEMA(value)


def _findmy_to_code(var: cg.MockObj, config: ConfigType) -> None:
    """Emit the allow_findmy config."""
    value = config[CONF_ALLOW_FINDMY]
    if value is False:
        return
    cg.add(var.set_allow_findmy(True))
    rssi = _IBEACON_RSSI_INHERIT if value is True else value[CONF_RSSI]
    cg.add(var.set_findmy_rssi(rssi))


def _validate_allow_ibeacon(value: bool | list[ConfigType]) -> bool | list[ConfigType]:
    """Accept `true`/`false`, or a list of major/minor filters."""
    if isinstance(value, bool):
        return value
    return cv.ensure_list(_IBEACON_FILTER_SCHEMA)(value)


def _ibeacon_to_code(var: cg.MockObj, config: ConfigType) -> None:
    """Emit the allow_ibeacon config."""
    value = config[CONF_ALLOW_IBEACON]
    if value is False:
        return
    if value is True:
        # A bare `true` exempts every iBeacon from the blocklist and nothing
        # more; the distance rules still apply, same as an inheriting filter.
        cg.add(var.set_allow_ibeacon(True))
        cg.add(var.set_ibeacon_any_rssi(_IBEACON_RSSI_INHERIT))
        return
    # A list scopes the exemption, so the unscoped flag stays off.
    cg.add(var.set_allow_ibeacon(False))
    for entry in value:
        # nullptr / -1 mean "any". The C++ side orders rules most-specific-first.
        uuid = entry.get(CONF_UUID, cg.nullptr)
        major = entry.get(CONF_MAJOR, -1)
        rssi = entry[CONF_RSSI]
        for minor in entry.get(CONF_MINOR) or [-1]:
            cg.add(var.add_ibeacon_rule(uuid, major, minor, rssi))


def _validate_rssi_floor(config: ConfigType) -> ConfigType:
    """Reject a floor stricter than the limits it is meant to backstop.

    rssi_floor runs before categorisation and applies to every advertisement;
    every other limit is applied afterwards to one category. The floor is
    therefore only meaningful while it is the loosest of them. A floor above a
    category's limit would silently take over as that category's effective
    filter, so fail loudly rather than quietly changing behaviour.
    """
    floor = config[CONF_RSSI_FLOOR]
    threshold = config[CONF_RSSI_THRESHOLD]
    for key in (CONF_RSSI_MAC_ALLOWLIST, CONF_RSSI_IRK, CONF_RSSI_SERVICE_UUID):
        limit = config[key]
        if limit != -127 and floor != -127 and limit < floor:
            raise cv.Invalid(
                f"{key} ({limit}) is below {CONF_RSSI_FLOOR} ({floor}), so it "
                f"can never fire: the floor has already dropped anything that "
                f"weak. Raise it above the floor, or remove it.",
                path=[key],
            )
    # -127 is "no threshold", not a very loose one: a floor on its own ("drop
    # only what is too weak to mean anything") is a legitimate config, and used
    # to be rejected here with a message about a threshold nobody had set.
    if floor != -127 and threshold != -127 and floor > threshold:
        raise cv.Invalid(
            f"{CONF_RSSI_FLOOR} ({floor}) must be at or below "
            f"{CONF_RSSI_THRESHOLD} ({threshold}): the floor is an absolute "
            f"backstop applied ahead of the allowlists, so a floor stricter "
            f"than the threshold would override it for every device.",
            path=[CONF_RSSI_FLOOR],
        )
    return config


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(BLEAdvertFilter),
        cv.GenerateID(CONF_BLUETOOTH_PROXY_ID): cv.use_id(
            bluetooth_proxy.BluetoothProxy
        ),
        cv.Optional(CONF_RSSI_THRESHOLD, default=-127): cv.int_range(min=-127, max=0),
        # Absolute floor, applied before categorisation rather than after it, so
        # it bounds mac_allowlist / service_uuid_allowlist too. -127 disables it
        # and is the default, so an unconfigured build is unchanged.
        cv.Optional(CONF_RSSI_FLOOR, default=-127): cv.int_range(min=-127, max=0),
        # Per-category limits. -127 (default) means inherit; see the setters in
        # ble_advert_filter.h for what each inherits and why.
        cv.Optional(CONF_RSSI_MAC_ALLOWLIST, default=-127): cv.int_range(
            min=-127, max=0
        ),
        cv.Optional(CONF_RSSI_IRK, default=-127): cv.int_range(min=-127, max=0),
        cv.Optional(CONF_RSSI_SERVICE_UUID, default=-127): cv.int_range(
            min=-127, max=0
        ),
        cv.Optional(CONF_IRKS, default=[]): cv.ensure_list(_validate_irk),
        cv.Optional(CONF_ALLOW_ESPRESSIF, default=True): cv.boolean,
        cv.Optional(CONF_ALLOW_HOMEKIT, default=True): cv.boolean,
        # iBeacon is Apple manufacturer data (subtype 0x02), so blocklisting
        # 0x004C takes every iBeacon with it. Off by default - unlike HomeKit,
        # an iBeacon is usually exactly the noise the blocklist is there to
        # kill. Turn it on when something you own beacons.
        cv.Optional(CONF_ALLOW_IBEACON, default=False): _validate_allow_ibeacon,
        # FindMy accessories (AirTags, AirPods, licensed tags) are Apple
        # manufacturer data too, from a random static address no IRK resolves.
        cv.Optional(CONF_ALLOW_FINDMY, default=False): _validate_allow_findmy,
        cv.Optional(CONF_DROP_NON_RESOLVABLE, default=False): cv.boolean,
        cv.Optional(CONF_NAME_BLOCKLIST, default=[]): cv.ensure_list(
            cv.All(cv.string_strict, cv.Length(min=1, max=29))
        ),
        cv.Optional(CONF_MAC_ALLOWLIST, default=[]): cv.ensure_list(_mac_address),
        cv.Optional(CONF_MAC_BLOCKLIST, default=[]): cv.ensure_list(_mac_address),
        cv.Optional(CONF_ALLOWLIST_EXCLUSIVE, default=False): cv.boolean,
        cv.Optional(CONF_MANUFACTURER_BLOCKLIST, default=[]): cv.ensure_list(
            cv.hex_uint16_t
        ),
        cv.Optional(CONF_SERVICE_UUID_ALLOWLIST, default=[]): cv.ensure_list(
            _validate_service_uuid
        ),
    }
).extend(cv.COMPONENT_SCHEMA)

CONFIG_SCHEMA = cv.All(CONFIG_SCHEMA, _validate_rssi_floor)


async def to_code(config: ConfigType) -> None:
    # Supported way to compile the hook into bluetooth_proxy. Do not emit the
    # define directly - it is an implementation detail of that component and may
    # be renamed (esphome/esphome#19220).
    bluetooth_proxy.enable_advertisement_filter()

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[CONF_BLUETOOTH_PROXY_ID])
    cg.add(var.set_parent(parent))

    cg.add(var.set_rssi_threshold(config[CONF_RSSI_THRESHOLD]))
    cg.add(var.set_rssi_floor(config[CONF_RSSI_FLOOR]))
    cg.add(var.set_rssi_mac_allowlist(config[CONF_RSSI_MAC_ALLOWLIST]))
    # The pre-gate is derived from all of the above in C++ (recompute_gate_),
    # so that runtime setters keep it correct; nothing to emit for it here.
    _ibeacon_to_code(var, config)
    _findmy_to_code(var, config)
    cg.add(var.set_rssi_irk(config[CONF_RSSI_IRK]))
    cg.add(var.set_rssi_service_uuid(config[CONF_RSSI_SERVICE_UUID]))
    cg.add(var.set_allow_espressif(config[CONF_ALLOW_ESPRESSIF]))
    cg.add(var.set_allow_homekit(config[CONF_ALLOW_HOMEKIT]))
    cg.add(var.set_drop_non_resolvable(config[CONF_DROP_NON_RESOLVABLE]))
    cg.add(var.set_allowlist_exclusive(config[CONF_ALLOWLIST_EXCLUSIVE]))

    if irks := config[CONF_IRKS]:
        cg.add(var.set_irks_hex("".join(irks)))
    for needle in config[CONF_NAME_BLOCKLIST]:
        cg.add(var.add_blocked_name(needle.lower()))
    for mac in config[CONF_MAC_ALLOWLIST]:
        cg.add(var.add_allowed_mac(mac.as_hex))
    for mac in config[CONF_MAC_BLOCKLIST]:
        cg.add(var.add_blocked_mac(mac.as_hex))
    for company in config[CONF_MANUFACTURER_BLOCKLIST]:
        cg.add(var.add_blocked_manufacturer(company))
    for uuid in config[CONF_SERVICE_UUID_ALLOWLIST]:
        if isinstance(uuid, str):
            cg.add(var.add_allowed_service_uuid128(uuid))
        else:
            cg.add(var.add_allowed_service_uuid(uuid))


@automation.register_action(
    "ble_advert_filter.set_irks",
    SetIrksAction,
    # Also takes the value directly: `ble_advert_filter.set_irks: !lambda ...`.
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(BLEAdvertFilter),
            # Free text: every run of exactly 32 hex characters is a key, so a
            # Home Assistant entity can keep a label next to each one.
            cv.Required(CONF_IRKS): cv.templatable(cv.string),
        },
        key=CONF_IRKS,
    ),
    synchronous=True,
)
async def set_irks_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    template_ = await cg.templatable(config[CONF_IRKS], args, cg.std_string)
    cg.add(var.set_irks(template_))
    return var


@automation.register_action(
    "ble_advert_filter.clear_irks",
    ClearIrksAction,
    cv.Schema({cv.GenerateID(): cv.use_id(BLEAdvertFilter)}),
    synchronous=True,
)
async def clear_irks_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var
