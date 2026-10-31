# Changelog

## 0.1.1 - 2026-10-30
- Fixed: the value no longer draws over the entity's name in small cells.
- Limits are now per entity: one field of `warn,crit` pairs separated by `;`, in the order of the entities (`80,90;;50,70`). An entity with no pair is not coloured, so a temperature in °F no longer turns red by another entity's limits. If you had one pair before (`80,90`), it now applies to entity 1 only: set the others.

## 0.1.0 - 2026-10-30
- First version. A grid of up to eight entities. Numbers turn amber and red at your limits; on, problem and unavailable turn red, everything else green. Tap a cell for its state and age.
