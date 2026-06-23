# Glossary — the value-calc / parity terms (engine field ⇄ plain meaning)

> Plain-language translations of the terms used in the calc-parity work. Engine field names are terse; this maps
> each to what it actually *is*. Grouped by: the yield formula, the per-plot components, the extra bucket, the
> dry/cascade calc, and the methodology.

## The yield formula (the spine of it all)

`getYieldRate100(y)` — a city's final rate of one yield (food/production/commerce), ×100 (2 decimals). The whole
calc is this one line (CvCity.cpp:11246):

```
yield = (base + specialist) × modifier  +  extra
```

- **base** (`getBaseYieldRate`) — worked-tile yield + trade + free-city + golden-age. The "raw" yield before %s.
- **specialist** (`getSpecialistYieldTotal`) — yield from assigned specialists (engineers/etc.). Added to base, so
  it **is** multiplied by the modifier (specialists get the building bonuses too).
- **modifier** (`getBaseYieldRateModifier`) — the **percent** stack (e.g. +50% production from a Factory). A
  number like 150 = +50%. Applied to (base+specialist).
- **extra** (`getExtraYield`) — flat yield added **outside** the multiply (so buildings' %s do NOT amplify it).

**modified vs unmodified** — "in the base" = gets the % bonus; "in the extra/flat" = does not. Where a mechanic
lands changes its value a lot when modifiers are big.

## The per-plot yield components (what each worked tile contributes)

The dump (`/diagnostic/cityInput`, `plots[]`) breaks each plot's yield into named pieces. Suffix = yield letter
(**F**=food, **P**=production, **C**=commerce). So `natP` = the *nature* component of *production*.

- **natX** (`calculateNatureYield`) — the tile's intrinsic yield from its **terrain + feature + resource** (e.g.
  grassland 2 food).
- **impX** (`calculateImprovementYieldChange`) — the **improvement's own** yield (a farm/mine/treefarm), including
  its tech/river/route upgrades.
- **wcX** (`getYieldChangeAt`, "working-city") — yield this **city's buildings** add **per worked tile of a type**
  (e.g. a building giving +1 production to every hills tile, or every farm). Sums the city's per-plot-type /
  per-terrain / per-river / per-improvement yield changes.
- **terX** — the **player's** terrain yield change (civics/traits granting +yield to a terrain everywhere).
- **seaX** (`getSeaPlotYield`) — the player's flat bonus to **water tiles** (Lighthouse/Harbor-style).
- **ccX** — the **city-center tile**'s special yield (`getCityChange` + `population / divisor`). The center tile
  scales with city size.

## The "extra" bucket (the unmodified flat term), broken out

`getExtraYield = m_buildingExtraYield100 + m_aiExtraYield×100` — two sub-buckets:

- **`extraBuildingYield100`** (`m_buildingExtraYield100`) — flat yields a building grants the city directly
  (e.g. Granary +N food), ×100. The "building flat."
- **`m_aiExtraYield`** — everything else in the flat bucket, fed by only two things (verified):
  - **`corporationYield`** — a corporation's flat per-city yield output.
  - **`buildingYieldChange`** (`m_aBuildingYieldChange`) — per-building yield-changes set by: `BonusYieldChanges`
    (building +yield per a city resource), `VicinityBonusYieldChanges` (per resource in the city's working radius),
    vote-source religion (Apostolic-Palace-style), and game events.
- **`BonusYieldChanges` / `VicinityBonusYieldChanges`** — a building yields extra **per resource** (in-city /
  in-vicinity). The corp version of "per resource" is the only **volumetric** mechanic (scales with *how many* of
  a resource you have); everything else is presence-based.

## The dry calc (`Tools/ModifierCalc/dry_calc.py`) terms

- **dry / dry calc** — the offline Python re-implementation of the cascade value calc. It reads raw game **state**
  + the curated **JSON** and computes each value, to validate against the live engine.
- **deposit** — one curated JSON entry that contributes a value, e.g. `production.city.flat` on a building. The
  cascade is "things deposit value down onto a city."
- **`requires.build` vs `requires.operate`** — *build* = a one-time gate to **construct** it (greying, not
  re-checked after). *operate* = a continuous gate; if it fails the built thing goes **dormant** (stops working).
- **`active_source_jsons`** — the set of sources currently depositing onto a city (active buildings + civics +
  traits + techs + bonuses + religions + projects + heritages).
- **`gather_slot`** — sums all active sources' flat/percent/multiplier for one (family, scope) into a "slot."
- **`slot.flat` / `slot.percent`** — the accumulated flat (extra) and percent (modifier) for a channel.
- **`_intrinsic` / `_delivered`** (plot) — `_intrinsic` = the tile's own yield (nat+imp); `_delivered` = yields a
  building/civic delivers *to* this tile (keyed by its terrain/improvement/plot-type). "deliveryguy rule."
- **`_corp_yield100` / `_corp_output100`** — dry's corp yield / corp commerce functions.
- **scope** — where a deposit applies: `city` / `area` / `empire` / `team` / `world`.

## Fixed-point & curation

- **×100 / "2 decimals"** — all engine math is integer, scaled ×100 (so 0.75 food is stored 75). No floats.
- **`readJson` (`CvCascadeReadJson`)** — the one place human-readable JSON → ×100 integer, at load.
- **descale / `descale100`** — the curator turning a legacy ×100 XML value back into a human number for the JSON
  (75 → 0.75), since JSON is uniformly human and readJson re-applies ×100.
- **curator** — the migration scripts (`Tools/Migration/curate_*.py`) that convert legacy XML → curated JSON.

## Methodology

- **per-mechanic parity** — verify each *individual mechanic* matches the engine **per city, exactly** — never an
  averaged or whole-output gap (averages hide offsetting errors). See `shadow.md §5b`.
- **shadow** — running the cascade alongside the live engine and diffing, until clean, before deleting the legacy.
- **the kraken** — this codebase's standardless tangle; shorthand for "an assumption here gets your ship eaten."
