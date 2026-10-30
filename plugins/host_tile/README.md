# Host summary

One machine on one tile: its name with an online dot, three bars for CPU, memory and disk, and the temperature and
uptime on the bottom line. Tap the tile to see every value in full.

## Set up

1. Add the plugin to a screen in Tessera. The screen builds once.
2. In Layout, place the **Host summary** tile (2x1 or 2x2) from the library's Plugins group.
3. In the inspector, type a name and choose the entities: CPU, memory and disk as percentages, a temperature, an uptime
   sensor (seconds, or a timestamp of the last boot) and a binary sensor or ping for online. All of them are optional.

## Good to know

- Bars turn accent above 80% and red above 90%. The limits are fixed.
- Changing an entity needs no build.
