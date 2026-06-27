# Golden Age — how it influences everything

A **golden age** is a temporary, player-wide boost period. Everything hangs off one counter:
`CvPlayer::getGoldenAgeTurns()` (turns remaining); **`isGoldenAge()` is just `goldenAgeTurns > 0`**
(`CvPlayer.cpp:9390`). Every effect below is gated on `isGoldenAge()` and stops the turn the counter hits 0.
It is **mutually exclusive with anarchy** and decrements 1/turn at end of the player's turn (`CvPlayer.cpp:3852`).

This doc exists so we don't re-derive golden-age behaviour from the engine each time. Locations are `file:line`
into `Sources/`.

---

## How a golden age STARTS (triggers)

| Trigger | Where | Mechanic |
|---|---|---|
| **Great-people units** | `CvUnit::goldenAge()` `9897`; `CvPlayer.cpp:9042/9089` | Units flagged `UnitInfo.isGoldenAge()` are consumed. Cost **RISES each time**: `unitsRequiredForGoldenAge() = BASE_GOLDEN_AGE_UNITS + numUnitGoldenAges × GOLDEN_AGE_UNITS_MULTIPLIER`. Activating kills the required units, then `changeGoldenAgeTurns(getGoldenAgeLength())` + `changeNumUnitGoldenAges(1)`. |
| **A building** | `CvCity.cpp:14589` | A building flagged `BuildingInfo.isGoldenAge()` fires `changeGoldenAgeTurns(1 + getGoldenAgeLength())` on completion. |
| **An event** | `CvPlayer.cpp:21714` | An event flagged `EventInfo.isGoldenAge()` triggers one length. |
| **Great-person birth** | `CvPlayer.cpp:20467` | A trait's `getGoldenAgeOnBirthofGreatPeopleType()` auto-triggers a golden age when that GP type is born. |

## How LONG it lasts (duration)

`getGoldenAgeLength() = max(1, getModifiedIntValue(game.goldenAgeLength100, getGoldenAgeModifier()) / 100)`
(`CvPlayer.cpp:9460`).
- **Base × gamespeed:** `goldenAgeLength100 = GOLDEN_AGE_LENGTH × gamespeed.speedPercent` (`CvGame.cpp:3257`).
- **Modifier:** `getGoldenAgeModifier()` accumulates trait `getGoldenAgeDurationModifier()` + building `getGoldenAgeModifier()`.

---

## What it DOES — the effects

### 1. Yields & commerce — THREE additions, all in `base` (the cascade-relevant part)

All three are gated on `isGoldenAge()` and land in the city's **`base`**, so they ride the `× modifier`
(per [modifier.md](../specs/modifier.md) §2, golden age is a **base supplier**, not a flat add-on):

1. **Per-plot yield bonus** — `CvPlot::calculateYield:8403`. **It is NOT a flat "+1 on every worked plot."** It is
   **threshold-gated**: a worked plot gets `+CvYieldInfo.getGoldenAgeYield(y)` **only if** its yield clears
   `CvYieldInfo.getGoldenAgeYieldThreshold(y)`; a plot below the threshold gets nothing. This addend is part of
   `basePlotYield` ([calc-map](../plans/structural-cleanup/legacy-value-calc-map.md) §10.1).
   - **⚠ The threshold IGNORES the plot's improvement & route (counter-intuitive, and load-bearing for parity):**
     the test runs on the **PRE-improvement, PRE-route** running yield — `nature + extra + [centre] + playerTerrain +
     seaPlot + getYieldChangeAt + landmark + extra/less-threshold`. The improvement (`:8430`) and route (`:8435`) are
     added **AFTER** the golden-age check, so a rich improvement/route on a tile does **not** help it qualify — only
     the tile's intrinsic + city/player yields decide. (Thresholds are low for some yields — e.g. commerce — so the
     bonus fires almost everywhere and *looks* like "+1 on everything", but it is genuinely gated, and a
     low-pre-improvement tile can miss it.) A cascade reproducing this must test the same pre-improvement base; the
     engine dump exposes it per tile as `preImpBase` on `/computed/cities/yields`.
2. **Player base golden-age yield** — `CvCity.cpp:22902`. `getBaseYieldRate` adds `player.getGoldenAgeYield(y)`,
   a flat player-wide per-yield bonus, fed by traits' `getGoldenAgeYieldChanges[]` (`CvTraitInfo.cpp:2263`).
   Part of `base` (calc-map §1.1) — distinct from the per-plot bonus above.
3. **Golden-age commerce** — `CvCity.cpp:11937`. Base commerce adds `100 × player.getGoldenAgeCommerce(c)`, fed by
   traits' `getGoldenAgeCommerceChanges[]` (`CvTraitInfo.cpp:2317`).

> PURE_TRAITS option filters the trait-fed golden-age yield/commerce (a positive bonus on a negative trait drops).

### 2. Growth — faster

City food-for-growth threshold is reduced by `GOLDEN_AGE_PERCENT_LESS_FOOD_FOR_GROWTH%` (`CvPlayer.cpp:24462`).

### 3. Great people — faster

`+GOLDEN_AGE_GREAT_PEOPLE_MODIFIER%` to a city's great-people rate (`CvCity.cpp:7177`).

### 4. Governance — NO anarchy

Civic changes (`getCivicAnarchyLength:8940`) and religion changes (`getReligionAnarchyLength:9001`) cost **0
anarchy** while in a golden age — the canonical "switch civics for free" window. The AI deliberately exploits it
(`CvPlayerAI.cpp:6401`, `17314`).

---

## How to OBSERVE it (no fishing)

- `/diagnostic` — `isGoldenAge` per player.
- `/diagnostic/config` — per-yield `yieldGoldenAgeYield` + `yieldGoldenAgeThreshold` arrays.
- `/computed/cities/yields` — the per-plot golden-age addend (worked-plot decomposition); city-level
  `baseGoldenAgeYield` + `goldenAgeCommerce`.
- growth / great-people endpoints — the golden-age flag + the modifiers it gates.

## Where the NUMBERS live (data fields)

| Source | Fields |
|---|---|
| `CvYieldInfo` | `getGoldenAgeYield`, `getGoldenAgeYieldThreshold` (the per-plot bonus + threshold) |
| `CvTraitInfo` | `getGoldenAgeYieldChanges[]`, `getGoldenAgeCommerceChanges[]`, `getGoldenAgeDurationModifier`, `getGoldenAgeOnBirthofGreatPeopleType` |
| `CvBuildingInfo` | `isGoldenAge` (trigger on completion), `getGoldenAgeModifier` (duration) |
| `CvUnitInfo` | `isGoldenAge` (consumable trigger unit) |
| `CvEventInfo` | `isGoldenAge` (event trigger) |
| Defines | `BASE_GOLDEN_AGE_UNITS`, `GOLDEN_AGE_UNITS_MULTIPLIER`, `GOLDEN_AGE_LENGTH`, `GOLDEN_AGE_PERCENT_LESS_FOOD_FOR_GROWTH`, `GOLDEN_AGE_GREAT_PEOPLE_MODIFIER` |

---

## TL;DR for the cascade

Golden age touches the yield path in **three** places, all inside `base` (so all `× modifier`): the
**per-plot** threshold bonus (in `basePlotYield`), the **player** golden-age yield, and the **golden-age
commerce** — plus faster growth, faster great people, and zero-anarchy civic swaps elsewhere. The one parity
gotcha is the per-plot bonus's **pre-improvement/pre-route** threshold test (`CvPlot.cpp:8403`).
