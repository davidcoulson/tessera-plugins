# Status grid

Up to eight Home Assistant entities in one tile, as cells that change colour by what they say: a CPU at 95%, a UPS on
battery, a service that is down. Tap a cell to see its name, state and how long ago it changed.

## Set up

1. Add the plugin to a screen in Tessera. The screen builds once.
2. In Layout, place the **Status grid** tile (2x2 or 4x2) from the library's Plugins group.
3. In the inspector, choose up to eight entities, then the limits.
4. Choose whether a high or a low number is bad.

## Colours

- A number: normal below the warning limit, accent from it, red from the critical limit (reversed when low is bad).
  Each entity has its own limits, set in the **Limits per entity** field as `warn,crit` pairs in the order of the
  entities, separated by `;`: `80,90;;50,70` sets entities 1 and 3 and leaves entity 2 uncoloured. An entity without a
  pair is never coloured by its number.
- Anything that is not a number: red when it is `on`, `problem` or `unavailable`, green otherwise. A binary sensor
  that is `on` means a problem, as does an update that is `on`.

## Good to know

- Changing an entity or a limit needs no build.
- Tap a cell to see the entity's name, state and when it last changed.
