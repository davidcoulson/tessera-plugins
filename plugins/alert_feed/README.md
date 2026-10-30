# Alert feed

> **Wake on new alert lights the screen.** It is off to start with; turn it on only for a screen where that is wanted
> (a wall panel, not a bedside one).

Home Assistant's own notifications (the ones in the bell of its sidebar) on a tile, newest first: the title and how long
ago, up to 8 rows, and a badge with how many there are. Anything that calls `persistent_notification.create` lands in
it: Unraid, UniFi, your solar automations, Home Assistant itself. There is nothing to set up in Home Assistant.

Tap a row to read the message in a popup. **Dismiss** in the popup removes the notification in Home Assistant too.

With **Wake on new alert** on, a notification the screen had not seen yet lights it and shows the newest one in a popup
marked ALERT, also when the screen is in standby. The first list after a start is never announced, only what comes after
it.

## Set up

1. In Tessera, open **Plugins**, choose **Alert feed** and tick your screen. Add it; the screen builds once.
2. In Layout, place the **Alerts** tile from the library's Plugins group, 2x2 or larger.
3. To get the popup: in Home Assistant open the screen's device and turn on **Wake on new alert** (or hold the top bar,
   **Settings**, **Alert feed**). No build needed.

## Good to know

- The screen asks every 30 seconds while the tile is on the glass and every 2 minutes otherwise, so a new alert can take
  up to 2 minutes to show. For an instant full-screen card from an automation, Tessera's `show_alert` stays the way.
- Home Assistant's answer is cut to about 3 KB on its way to the screen, and every text in it to 48 characters. So a
  message is read to its first 48 characters, and with a dozen or more notifications at once the newest ones may be
  missing from the feed. Dismiss the old ones.
- The list is a copy kept on the screen: after a restart it is empty until the next answer.
