# BLE advert filter

> **Needs ESPHome 2026.10 and a screen with an ESP32-P4.** It adds the **Bluetooth proxy** plugin, and that one costs
> the screen Wi-Fi: read its notice first. Not tried on a real screen yet.

A Bluetooth proxy forwards everything it hears, including your neighbours' phones and a constant stream of Apple
background adverts that no integration uses. This plugin puts [esphome-ble-advert-filter](https://github.com/davidcoulson/esphome-ble-advert-filter)
(v2.0.4, by David Coulson, MIT) in front of the proxy, so those are dropped on the screen and never use its Wi-Fi.

It drops adverts weaker than the threshold (-80 dBm to start with) and Apple's background traffic. HomeKit accessories
still pass. It adds no tile: its numbers are on the screen's settings page (hold the top bar, **Settings**,
**Bluetooth filter**: threshold, forwarded, dropped).

## Which screens

Every screen with an ESP32-P4 (Guition P4, Waveshare P4, Tab5, reTerminal D1001, CrowPanel 7 and 10.1): the plugin
needs the **Bluetooth proxy** plugin, which only those boards fit. Bluetooth runs through the ESP32-C6 beside the P4.
The ESP32-S3 boards with an RGB panel are out for the reason that plugin gives (memory), and the 4 MB boards have no room.

## Set up

1. In Tessera, open **Plugins**, choose **BLE advert filter** and tick your screen. **Bluetooth proxy** comes along.
2. Add it. The screen builds once.
3. In Home Assistant, open the screen's device. **BLE RSSI Threshold** sets how weak an advert may be: raise it (toward
   -60) to hear only what is near the screen, lower it (toward -90) to hear more.
4. Let an iBeacon through? Add to the screen's Override YAML:
   `ble_advert_filter: {allow_ibeacon: [{uuid: "your-beacon-uuid"}]}`. More options (your phones' IRKs, name blocks) are
   in the component's [docs](https://github.com/davidcoulson/esphome-ble-advert-filter/blob/v2.0.4/docs/options.md).

## Good to know

- The filter is a copy of the component at v2.0.4 in `components/ble_advert_filter/` (a plugin cannot add external
  components), with one change: two `str.replace` calls in `__init__.py` are `str.translate`, because the plugin check
  refuses any `.replace(`. To update it, copy the newer release over it, redo that, and raise this plugin's version.
- The hook it uses (`bluetooth_proxy`'s advertisement filter) is new in ESPHome 2026.10, so the plugin asks for it.
- Checked with `tools/check.py` and ESPHome 2026.10.1 (`config` and `compile`), see below. Not flashed, so nothing here
  has been heard on a real screen.
