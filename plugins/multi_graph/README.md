# Multi graph

Up to four sensors drawn as coloured lines on one tile, over the last 6, 12, 24 or 48 hours. Under the head a legend
shows each sensor's name and its live value; at the left are the lowest and highest value of the lines.

## Set up

1. Add the plugin to a screen in Tessera. The screen builds once.
2. In Layout, place the **Multi graph** tile from the library's Plugins group (2x1 or larger; 4x2 has room for names).
3. In the inspector, choose one to four sensors, how many hours to show, and whether each line is the average of the
   hour or its highest value.

## Good to know

- The history is Home Assistant's long-term statistics, so a sensor must have a `state_class` (`measurement`, `total`
  or `total_increasing`). A sensor without one has no line, only its live value.
- Sensors with the same unit share one scale, so lines can be compared. When units differ (watts and degrees) each
  line uses its own range and the numbers at the left belong to the first sensor, in its colour.
- The screen asks Home Assistant for the history when the page shows, one sensor per second, and again every 10 minutes
  while it stays on the glass. The last point is the live value.
- Nothing leaves your home: the plugin only asks the Home Assistant the screen already talks to.
