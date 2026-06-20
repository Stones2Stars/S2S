# Observability — Culture — what's on the wire for accrual, borders, and flips

> **Status:** reference   ·   **Verified against:** 2026-06-20 (function sites confirmed in `Sources/Engine/CvCity.cpp` + `Sources/Engine/CvPlot.cpp`; line numbers drift)
> **Grounding:** live `Sources/Engine/CvCity.cpp` (`doCulture`, `doPlotCulture`, `updateCultureLevel`, `setCultureLevel`, `netRevoltRisk100`), `Sources/Engine/CvPlot.cpp` (`doCulture`, decay, `calculateCulturalOwner`, `checkCityRevolt`, `checkFortRevolt`), `Sources/Tools/CvHttpServer.cpp` (`/cities`, `/players`). Carried over from the old draft map; function sites re-confirmed, paths re-grounded to the reorganized `Sources/` tree.
> Culture is **Tier 1 (Telescreen)** today: `/cities` exposes a city's coarse `cultureLevel` tier-enum (advances only on the infrequent border-expansion event) plus `[PERF]` phase timings — and nothing else. The culture accumulator, per-turn rate, threshold-to-next-expansion, per-plot culture values, plot ownership changes, revolt count, revolt risk, occupation timer, improvement culture and the decay mode are all computed live and emitted nowhere. There are **no `[CUL]`-tagged log lines at all**. This map walks accrual → borders → ownership → revolts, names what's dark, and proposes the snapshot fields / log tags / diagnostic endpoint to climb to Tier 3/4.

The observability scale (0–5) and the three canonical hook shapes are defined once in
[`README.md`](README.md) ([DEC-obs-scale], [DEC-obs-hook-shapes]); the live surface and the rules for
reading it (logs held open mid-session, use `/events` + `/diagnostic`) live in
[`http-server.md`](http-server.md). This doc does not restate them.

> Line numbers below are anchors at time of writing and **drift** — confirm the named function, not the
> integer.

---

## 1. How it actually works

### 1a. Per-turn city culture accrual — `doCulture`

`CvCity::doTurn` runs `doCulture()` every turn inside a `PERF_SCOPE("city.doCulture")` block
(CvCity.cpp:1331). `CvCity::doCulture` (CvCity.cpp:16301) calls:

```cpp
changeCultureTimes100(getOwner(), getCommerceRateTimes100(COMMERCE_CULTURE), false, true);
```

This adds the city's commerce-rate culture (×100 precision) to `m_aiCulture[owner]`. Chain:

- `changeCultureTimes100` → `setCultureTimes100` (CvCity.cpp:12989). The per-turn path bumps the player's
  total culture inline (`changeCulture(iChange/100)`) when `iChange > 99` (CvCity.cpp:13036).
- `updateCultureLevel` is called (CvCity.cpp:13003) to recompute the city's `CultureLevelTypes` (§1e).

Immediately after, `doPlotCulture(getOwner(), getCommerceRate(COMMERCE_CULTURE))` runs inside its own
`PERF_SCOPE("city.doPlotCulture")` block (CvCity.cpp:1335).

### 1b. Plot culture spread from cities — `doPlotCulture`

`CvCity::doPlotCulture` (CvCity.cpp:16308) takes the city's integer commerce-culture rate and iterates a
square of radius `iCultureLevel = GC.getCultureLevelInfo(getCultureLevel()).getLevel()` centred on the
city. For each plot within `iCultureDistance <= iCultureLevel` that passes `isPotentialCityWorkForArea`:

- `plotX->changeCulture(ePlayer, cultureDistanceDropoff(...), plotX->getOwner() != ePlayer)`.
- `plotX->setInCultureRangeOfCityByPlayer(ePlayer)` — marks the plot as receiving city influence this turn.

`cultureDistanceDropoff` (CvCity.cpp:16341): linear dropoff controlled by the `CITY_CULTURE_DENSITY_FACTOR`
XML define. Full base rate at distance 0; floor at max distance is `baseCultureGain * (100 - iDensityFactor)
/ 100`. Minimum return is always 1.

### 1c. Improvement culture — `CvPlot::doCulture` → `doImprovementCulture`

`CvPlot::doCulture` (CvPlot.cpp:10826) runs once per plot per game turn from `CvMap::doTurn`
(CvPlot.cpp:689). It:

1. If the plot has an improvement and an owner, calls `doImprovementCulture(owner, improvementInfo)`
   (CvPlot.cpp:10838).
2. Applies per-player culture decay (§1d).
3. Calls `checkCityRevolt`, `checkFortRevolt`, or `setOwner(calculateCulturalOwner(...))` depending on
   whether the plot has a city, acts-as-city, or is open terrain (CvPlot.cpp:10875).

`CvPlot::doImprovementCulture` (CvPlot.cpp:4064): if `imp.getCulture() >= 1`, radiates `iCulture` points
flat (no dropoff) to all plots within `imp.getCultureRange()` (Chebyshev) via `changeCulture`.

Known ordering quirk (comment at CvPlot.cpp:10830): `doImprovementCulture` runs during map iteration, so
plots processed earlier have already done their decay while later plots have not — improvement-culture
injection is not uniformly timed relative to decay.

### 1d. Culture decay

Inside `CvPlot::doCulture` (CvPlot.cpp:10841), every player with nonzero culture on a plot is considered:

**Standard mode** (not `GAMEOPTION_CULTURE_EQUILIBRIUM`):
- Decay applies only if the player gets `getCultureRateThisTurn(ePlayer) < 1` AND the plot has no city / the
  city is not owned by this player.
- Formula: `max(0, culture * (1000 - decayPermille) / 1000)`, where
  `decayPermille = TILE_CULTURE_DECAY_PERCENT * 1000 / speedPercent`.

**Equilibrium mode** (`GAMEOPTION_CULTURE_EQUILIBRIUM`):
- Decay applies to ALL players regardless of whether they add culture this turn.
- In range of a city by player: standard `decayPermille`. NOT in range: `15 * decayPermille` (≈45% faster
  at default speed).
- Floor: culture that was `> 1` cannot decay below 1. On `setCulture` a new value of 1 is bumped to 2
  (CvPlot.cpp:8541), so a newly claimed plot survives at least one decay turn.

### 1e. Border expansion — `updateCultureLevel` / `setCultureLevel`

`updateCultureLevel` (CvCity.cpp:10619): blocked during `isOccupation()`; iterates `CultureLevelInfo`
descending and sets `m_eCultureLevel` to the highest tier whose `getSpeedThreshold(gameSpeed)` ≤ city
culture total; calls `setCultureLevel(...)` on change.

`setCultureLevel` (CvCity.cpp:10536):
- Emits a DLL message ("borders expanded") when `eNewValue > eOldValue && eNewValue > 1`.
- Fires the `CvEventReporter::cultureExpansion` Python event (CvCity.cpp:10606) — **the only Python event
  for culture expansion**.
- If max culture level reached (`getCultureThreshold() == -1`), emits a second "max culture" DLL message.

The city's working radius is driven by `CultureLevelInfo.getCityRadius()` — border expansion = enlarged
working radius.

### 1f. Plot ownership — `calculateCulturalOwner`

`CvPlot::calculateCulturalOwner` (CvPlot.cpp:4837):
- `eHighestCulturePlayer = findHighestCulturePlayer(false, bCountLastTurn)`.
- No owner → return `eHighestCulturePlayer`.
- Current owner has `hasFixedBorders()`: keep owner if
  `culture(owner) * FIXED_BORDERS_CULTURE_RATIO_PERCENT / 100 >= culture(highest)` OR a unit on the plot can
  claim territory (CvPlot.cpp:4872).
- `GAMEOPTION_CULTURE_MIN_CITY_BORDER` active and plot adjacent to a city: that city's owner wins
  unconditionally (CvPlot.cpp:4858).
- Otherwise return `eHighestCulturePlayer`.

Open-terrain plots run `setOwner(calculateCulturalOwner(false))` directly each turn (CvPlot.cpp:10883).

### 1g. City revolt — `checkCityRevolt`

`CvPlot::checkCityRevolt` (CvPlot.cpp:1022):
- Skips occupied cities. Aborts if no cultural owner, if the cultural owner's **this-turn culture rate** ≤
  the owner's this-turn rate (the attacker must be actively winning the per-turn flow), or same team.
- Roll 1: `SorenRandNum(100) < iRevoltTestProb` (speed-adjusted `REVOLT_TEST_PROB`).
- `iCityStrength100 = pCity->netRevoltRisk100(eCulturalOwner)` — a 0–10000 value (0–100% × 100).
- Roll 2: `SorenRandNum(10000) < iCityStrength100`.
- On success: if `GAMEOPTION_CULTURE_NO_CITY_FLIPPING` is off AND (`CULTURE_FLIPPING_AFTER_CONQUEST` is on
  OR the attacker never owned it) AND `numRevolts(attacker) >= NUM_WARNING_REVOLTS` → `setOwner(eCulturalOwner)`
  (permanent flip); otherwise increment `numRevolts(attacker)` and set the occupation timer
  (`BASE_REVOLT_OCCUPATION_TURNS + iCityStrength100 * REVOLT_OCCUPATION_TURNS_PERCENT / 10000`,
  speed-adjusted). "Near miss" and "discontent" produce only UI messages.

`netRevoltRisk100` (CvCity.cpp:6432): `min(10000, max(0, baseRevoltRisk100 * unitRevoltRiskModifier / 100))`.
`baseRevoltRisk100` (CvCity.cpp:6440): starts `highestPopulation * 2`; adds `(era+1)` per adjacent
attacker tile; multiplies by the nonlinear culture-ratio modifier
`10000 * attackerPercent / max(1, defenderPercent)`; applies state-religion modifiers.
`unitRevoltRiskModifier` (CvCity.cpp:6501): summed `revoltProtectionTotal()` across the garrison (positive
protection → `10000/(100+garrison)` diminishing returns; negative → `100 - garrison`).

### 1h. Fort revolt — `checkFortRevolt`

`CvPlot::checkFortRevolt` (CvPlot.cpp:1133): if improvement `isActsAsCity()` and owned longer than
`SUPER_FORTS_DURATION_BEFORE_REVOLT`, and the cultural owner is a different team with no defending units →
immediate flip (`setOwner`), no probability roll.

---

## 2. What's on the wire today — **Tier 1 (Telescreen)**

### What is exposed

| Surface | Field | Granularity |
|---|---|---|
| `GET /cities` → `cultureLevel` | City's current `CultureLevelTypes` (enum int) | Per city, snapshotted |
| `GET /cities` → `commerce` | `YIELD_COMMERCE` rate (NOT `COMMERCE_CULTURE`) | Per city |
| `GET /players` → `score` | Score (includes a culture component) | Per player |
| `[PERF/phase]` `city.doCulture` / `city.doPlotCulture` | Wall-clock ms for the culture phase | Per player, per turn (gated `gPerfLogLevel >= 1`) — cost only, not content |

### What is NOT exposed (the gap)

| State | Accessor | Why it matters |
|---|---|---|
| City culture total | `getCultureTimes100(owner)` | The running accumulator — turns-to-next-expansion need it |
| Per-city culture rate | `getCommerceRate(COMMERCE_CULTURE)` | The `commerce` field is `YIELD_COMMERCE` (pre-slider), not this |
| Culture threshold (next border) | `getCultureThreshold()` | Culture needed for the next level (−1 = at max) |
| Per-plot culture values | per-player array on each `CvPlot` | The actual border/ownership competition + decay dynamics |
| Per-plot cultural owner | `calculateCulturalOwner` result | Which player holds each tile post-decay |
| Culture-rate-this-turn | `getCultureRateThisTurn(ePlayer)` per plot | The revolt-trigger gate (attacker rate > owner rate) |
| Revolt count per attacker | `getNumRevolts(attacker)` | The warning-revolt threshold for permanent flip |
| Occupation timer | `getOccupationTimer()` | Duration of an ongoing revolt-induced occupation |
| Net revolt risk | `netRevoltRisk100(attacker)` | The output of the complex pop/culture/religion/garrison formula |
| Improvement culture | `imp.getCulture()`, `getCultureRange()` | Fort/improvement tile culture — entirely invisible |
| In-culture-range flag | `isInCultureRangeOfCityByPlayer` per plot | Equilibrium fast/slow decay gating |
| Fixed-borders status | `hasFixedBorders()` per player | Modifies the ownership threshold |
| Game-option flags | `GAMEOPTION_CULTURE_EQUILIBRIUM`, `_NO_CITY_FLIPPING`, `_FLIPPING_AFTER_CONQUEST`, `_MIN_CITY_BORDER` | Change core decay/ownership/revolt logic |

The culture system has **no `[CUL]` log domain or equivalent**, and no per-event publication to `/events`.
The only culture signal is the `cultureLevel` tier enum, which advances only on border expansion (coarse,
infrequent).

---

## 3. The gap

At Tier 1 we can answer "what culture level is each city?" and "what is each city's (yield-)commerce rate?".
We **cannot** answer without looking at the screen:

- How many turns until a city's borders expand?
- Which tiles does player X control? Has that changed this turn?
- Is city Y at risk of revolt — from whom, with how many warning revolts accumulated?
- Standard or equilibrium decay? Is a given tile in fast or slow decay?
- What culture is flowing from improvements?
- For AI players: any of the above — entirely opaque.

The owner-designed equilibrium model is itself entirely unobservable from the endpoint layer. Any
cascade replacement of the border/ownership/revolt machinery cannot be shadowed
([DEC-map-before-delete]) until the revolt ledger (`numRevolts`, `occupationTimer`) and per-plot culture
values are surfaced.

---

## 4. Proposed hooks — climbing from Tier 1 to Tier 3/4

All hooks follow the three canonical hook shapes — see [DEC-obs-hook-shapes].

### 4a. `/cities` snapshot fields (cheapest — immediate Tier 2 lift)

Add to `CitySnap` and the per-city render in `CvHttpServer.cpp`:

| JSON key | Source | Notes |
|---|---|---|
| `cultureTimes100` | `getCultureTimes100(getOwner())` | The running accumulator (÷100 for display) |
| `cultureRate` | `getCommerceRate(COMMERCE_CULTURE)` | Culture per turn (slider output, NOT raw YIELD_COMMERCE) |
| `cultureThreshold` | `getCultureThreshold()` | Culture for the next border; −1 = at max |
| `occupationTimer` | `getOccupationTimer()` | 0 = not in revolt |

Read-only scalars already computed per turn; they fully expose the city-side culture state and let a
reader compute turns-to-expansion and detect revolt occupation.

### 4b. `/players` snapshot fields

| JSON key | Source |
|---|---|
| `culture` | `kPlayer.getCulture()` — aggregate culture total (all cities) |
| `cultureRate` | `kPlayer.getCommerceRate(COMMERCE_CULTURE)` — per-turn culture output |

### 4c. `[CUL]` log domain — `CultureAI.log`, city-scope (`gCityLogLevel`)

New `logCultureAI(int level, ...)` → `CultureAI.log`, tag prefix `[CUL]`:

| Tag | Level | Where | Payload |
|---|---|---|---|
| `[CUL/expand]` | 1 | `CvCity::setCultureLevel` (`eNewValue > eOldValue`) | `turn= owner= city= oldLevel= newLevel=` |
| `[CUL/revolt]` | 1 | `CvPlot::checkCityRevolt` (roll1 success) | `turn= city= owner= attacker= risk100= roll= result=occupying\|flipping\|quelled` |
| `[CUL/flip]` | 1 | `CvPlot::setOwner` from a revolt or cultural-owner path | `turn= plot= oldOwner= newOwner= reason=revolt\|culture` |
| `[CUL/accrual]` | 2 | `CvCity::doCulture` | `turn= owner= city= rate= total= nextAt= turnsLeft=` |
| `[CUL/revolt/accumulate]` | 2 | `CvCity::changeNumRevolts` | `turn= city= owner= attacker= revolts= threshold=` |
| `[CUL/decay]` | 3 | `CvPlot::doCulture` per-player decay | `turn= plot= player= from= to= mode=standard\|equilibrium\|fastDecay` |

Level-1 lines (expand, revolt outcomes, flips) make every border and ownership change visible on `/events`.
Level 2 adds the per-city accrual summary; level 3 is verbose plot-level decay for targeted investigation.

### 4d. `[CUL/plotflip]` event on open-terrain ownership change

In `CvPlot::doCulture`'s `setOwner(calculateCulturalOwner(false))` branch (CvPlot.cpp:10883), before the
call, if the owner is about to change emit a level-1 line:
`[CUL/plotflip] turn= x= y= oldOwner= newOwner= reason=culture`. Without it, map border shifts are completely
silent from outside.

### 4e. `/diagnostic/revoltRisk?city=N&player=N&attacker=M` (Tier 4)

A mailbox endpoint (same game-thread pattern as `canConstruct`) calling `netRevoltRisk100` /
`baseRevoltRisk100` / `unitRevoltRiskModifier` read-only — the only way to see the revolt-risk formula
output without a debugger:

```json
{
  "city": "CITY_NAME", "cityId": N, "owner": K, "attacker": M,
  "cultureTotal": { "owner": 12345, "attacker": 9876 },
  "cultureRate":  { "owner": 45, "attacker": 62 },
  "baseRisk100": 2340, "unitModifier": 75, "netRisk100": 1755,
  "numRevolts": 2, "numWarningRevolts": 3, "occupationTimer": 0,
  "gameoptions": { "noFlipping": false, "flippingAfterConquest": false, "equilibrium": true }
}
```

Game-option flags are surfaced here as a one-time convenience rather than permanent snapshot fields.

A full **per-plot culture snapshot** mechanism (every player's culture on every tile) is the remaining
gap for Tier 5 coverage; it is not proposed here but is needed before the plot-ownership maintainer can be
fully shadowed.

---

## 5. Priority order

| Hook | Tier gain | Effort | Rationale |
|---|---|---|---|
| `/cities` fields (§4a) | 1→2 | ~10 lines | City-side culture state + revolt occupation visible for ALL players |
| `[CUL/expand]`+`[CUL/revolt]`+`[CUL/flip]` (§4c L1) | 2→3 | ~15 lines | Every border + ownership change live on `/events` |
| `[CUL/plotflip]` (§4d) | 3 | ~5 lines | Open-terrain border shifts visible |
| `/players` culture (§4b) | 2 | ~5 lines | Player aggregate for score/victory tracking |
| `/diagnostic/revoltRisk` (§4e) | 4 | mailbox slot | On-demand revolt-risk formula output for cascade shadow |

All log hooks are `gCityLogLevel`-gated and off by default — zero cost when off.

---

## 6. Code cross-reference

> Paths re-grounded to the reorganized `Sources/` tree (`Cv*` engine → `Sources/Engine/`, `CvHttpServer`
> → `Sources/Tools/`). Line numbers drift — confirm the function.

| Claim | Source |
|---|---|
| `doCulture` (city accrual) | `Sources/Engine/CvCity.cpp:16301` |
| `doPlotCulture` (city → plot spread) | `Sources/Engine/CvCity.cpp:16308` |
| `cultureDistanceDropoff` | `Sources/Engine/CvCity.cpp:16341` |
| `updateCultureLevel` | `Sources/Engine/CvCity.cpp:10619` |
| `setCultureLevel` (+ `cultureExpansion` event) | `Sources/Engine/CvCity.cpp:10536` |
| `netRevoltRisk100` / `baseRevoltRisk100` / `unitRevoltRiskModifier` | `Sources/Engine/CvCity.cpp:6432, 6440, 6501` |
| `CvPlot::doCulture` (decay + dispatch) | `Sources/Engine/CvPlot.cpp:10826` |
| `doImprovementCulture` | `Sources/Engine/CvPlot.cpp:4064` |
| `calculateCulturalOwner` | `Sources/Engine/CvPlot.cpp:4837` |
| `checkCityRevolt` | `Sources/Engine/CvPlot.cpp:1022` |
| `checkFortRevolt` | `Sources/Engine/CvPlot.cpp:1133` |
| `CitySnap` / city publish walk (add fields here) | `Sources/Tools/CvHttpServer.cpp` |

---

## See also
- [`README.md`](README.md) — the observability scaffold: the 0–5 scale ([DEC-obs-scale]), the Orwell bar,
  and the three canonical hook shapes ([DEC-obs-hook-shapes]) this map's hooks instantiate.
- [`http-server.md`](http-server.md) — the live surface (`/cities`, `/players`, `/diagnostic`) these hooks
  extend, and the live-read rules (logs held open mid-session).
- [`health-happiness.md`](health-happiness.md) — `getCulturePercentAnger()` (foreign culture on plot) is a
  happiness source; the same per-plot culture this map proposes to expose feeds that anger.
- [`../../explanation/cascade-architecture.md`](../../explanation/cascade-architecture.md) — why total
  observability is load-bearing: the border/ownership/revolt maintainers cannot be cut until shadowed
  ([DEC-map-before-delete]).
- [`../../README.md`](../../README.md) — the comprehension map / overview-of-overviews.

[DEC-obs-scale]: ../../architecture/decisions.md#dec-obs-scale
[DEC-obs-hook-shapes]: ../../architecture/decisions.md#dec-obs-hook-shapes
[DEC-map-before-delete]: ../../architecture/decisions.md#dec-map-before-delete
