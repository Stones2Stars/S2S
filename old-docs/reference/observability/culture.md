# Observability map: Culture (accrual, borders, flips)

> DRAFT observability map (2026-06-18 by parent) — claims cited from code; verify before relying.

**Scope:** culture accrual per city/plot, border expansion thresholds, city/fort flips and revolts.
**Related inventory entry:** `docs/dev/plans/cascade-mapping-inventory.md` §A (Culture: `◐ partial`).
**Observability tier assigned:** **1 — Telescreen** (coarse city-level snapshot only; no accrual stream, no plot-level state, no revolt state).

---

## 1. How it actually works

### 1a. Per-turn city culture accrual

`CvCity::doTurn` runs `doCulture` every turn inside a `PERF_SCOPE`:

```
{ PERF_SCOPE("city.doCulture", getOwner()); doCulture(); }   // CvCity.cpp:1331
```

`CvCity::doCulture` (CvCity.cpp:16301-16305) calls:

```cpp
changeCultureTimes100(getOwner(), getCommerceRateTimes100(COMMERCE_CULTURE), false, true);
```

This adds the city's commerce-rate culture (×100 precision) to `m_aiCulture[owner]`. The call chain is:

- `changeCultureTimes100` → `setCultureTimes100` (CvCity.cpp:12989)
- `setCultureTimes100` also directly calls `GET_PLAYER(getOwner()).changeCulture(iChange/100)` when `bNationalSet=false` is passed (i.e., accumulating into the player's total culture). The `changeCultureTimes100` path sets `bNationalSet=true`, so in the normal per-turn path the player total is bumped inline inside `changeCultureTimes100` by `iChange/100` when `iChange > 99` (CvCity.cpp:13036-13038).
- `updateCultureLevel` is called (CvCity.cpp:13003) to recalculate the city's `CultureLevelTypes`.

Immediately after `doCulture`, `doPlotCulture` runs (CvCity.cpp:1335):

```
{ PERF_SCOPE("city.doPlotCulture", getOwner()); doPlotCulture(getOwner(), getCommerceRate(COMMERCE_CULTURE)); }
```

### 1b. Plot culture spread from cities (`doPlotCulture`)

`CvCity::doPlotCulture` (CvCity.cpp:16308-16334):
- Takes the city's integer commerce-culture rate (not ×100).
- Iterates over a square of radius `iCultureLevel = GC.getCultureLevelInfo(getCultureLevel()).getLevel()` centered on the city plot.
- For each plot within `iCultureDistance <= iCultureLevel` that passes `isPotentialCityWorkForArea`:
  - Calls `plotX->changeCulture(ePlayer, cultureDistanceDropoff(...), plotX->getOwner() != ePlayer)`.
  - Calls `plotX->setInCultureRangeOfCityByPlayer(ePlayer)` — marks this plot as receiving city influence this turn.

`cultureDistanceDropoff` (CvCity.cpp:16341-16362): linear dropoff formula controlled by `CITY_CULTURE_DENSITY_FACTOR` XML define. At distance 0 (city tile) the full base rate is applied; at max distance the dropoff floor is `baseCultureGain * (100 - iDensityFactor) / 100`. Minimum return value is always 1.

### 1c. Improvement culture (`CvPlot::doCulture` → `doImprovementCulture`)

`CvPlot::doCulture` (CvPlot.cpp:10826-10897) runs during `CvMap::doTurn` (`CvPlot.cpp:689`), once per plot per game turn. It:
1. If the plot has an improvement and an owner, calls `doImprovementCulture(owner, improvementInfo)` (CvPlot.cpp:10838).
2. Applies per-player culture decay (see §1d).
3. Calls `checkCityRevolt`, `checkFortRevolt`, or `setOwner(calculateCulturalOwner(...))` depending on whether the plot has a city, acts-as-city, or is open terrain (CvPlot.cpp:10875-10883).

`CvPlot::doImprovementCulture` (CvPlot.cpp:4064-4081): if `imp.getCulture() >= 1`, radiates `iCulture` points flat (no dropoff) to all plots within `imp.getCultureRange()` (Chebyshev) via `changeCulture`.

Known ordering issue (comment at CvPlot.cpp:10830-10835): `doImprovementCulture` runs during map iteration, so plots processed earlier have already done their decay while later plots have not — improvement culture injection is not uniformly timed relative to decay.

### 1d. Culture decay

Inside `CvPlot::doCulture` (CvPlot.cpp:10841-10873), every player with nonzero culture on a plot is considered for decay:

**Standard mode** (not `GAMEOPTION_CULTURE_EQUILIBRIUM`):
- Decay only applies if the player is getting `getCultureRateThisTurn(ePlayer) < 1` AND the plot has no city or city is not owned by this player.
- Decay formula: `max(0, culture * (1000 - decayPermille) / 1000)`.
- `decayPermille = TILE_CULTURE_DECAY_PERCENT * 1000 / speedPercent`.

**Equilibrium mode** (`GAMEOPTION_CULTURE_EQUILIBRIUM`):
- Decay applies to ALL players regardless of whether they are adding culture this turn.
- If in range of a city by player: decay at standard `decayPermille`.
- If NOT in range: decay at `15 * decayPermille` (≈ 45% faster at default speed).
- Floor: `max(1 if culture > 1, ...)` — culture cannot decay below 1 if it was > 1 (prevents instant loss).
- On `setCulture` when equilibrium is active: a new value of 1 is bumped to 2 (CvPlot.cpp:8541), ensuring a newly claimed plot can survive at least one full decay turn.

### 1e. Border expansion (`updateCultureLevel`)

`updateCultureLevel` (CvCity.cpp:10619-10646):
- Blocked during occupation (`isOccupation()`).
- Iterates `CultureLevelInfo` in descending order; sets the city's `m_eCultureLevel` to the highest tier whose `getSpeedThreshold(gameSpeed)` ≤ city culture total.
- Calls `setCultureLevel(...)` on change (CvCity.cpp:10640).

`setCultureLevel` (CvCity.cpp:10536-10616):
- Emits a DLL message ("borders expanded") when `eNewValue > eOldValue` and `eNewValue > 1` (CvCity.cpp:10552-10613).
- Fires the `CvEventReporter::cultureExpansion` Python event (CvCity.cpp:10606) — **only Python event for culture expansion**.
- Updates feature happiness/health for the city.
- If max culture level reached (`getCultureThreshold() == -1`), emits a second "max culture" DLL message.

The city's working radius is driven by `CultureLevelInfo.getCityRadius()` — border expansion = enlarged working radius.

### 1f. Plot ownership (`calculateCulturalOwner`)

`CvPlot::calculateCulturalOwner` (CvPlot.cpp:4837-4931):
- Finds `eHighestCulturePlayer = findHighestCulturePlayer(false, bCountLastTurn)`.
- If plot has no owner → return `eHighestCulturePlayer`.
- If current owner has `hasFixedBorders()`: keep owner if `culture(owner) * FIXED_BORDERS_CULTURE_RATIO_PERCENT / 100 >= culture(highestPlayer)` OR a unit on the plot can claim territory (CvPlot.cpp:4872-4883).
- If `GAMEOPTION_CULTURE_MIN_CITY_BORDER` is active and plot is adjacent to a city: that city's owner wins unconditionally (CvPlot.cpp:4858-4862).
- Otherwise return `eHighestCulturePlayer`.

For open-terrain plots, `setOwner(calculateCulturalOwner(false))` is called directly each turn.

### 1g. City revolt (`checkCityRevolt`)

`CvPlot::checkCityRevolt` (CvPlot.cpp:1022-1131):
- Skips occupied cities (`isOccupation()`).
- Abort conditions (CvPlot.cpp:1034-1041):
  - No cultural owner.
  - Cultural owner's **this-turn culture rate** ≤ owner's this-turn culture rate — the city plot must be actively receiving MORE culture per turn from the attacker.
  - Same team as current owner.
- Roll 1: `SorenRandNum(100) < iRevoltTestProb` (speed-adjusted `REVOLT_TEST_PROB`).
- `iCityStrength100 = pCity->netRevoltRisk100(eCulturalOwner)` — a 0-10000 value (0-100% × 100).
- Roll 2: `SorenRandNum(10000) < iCityStrength100`.
- On successful revolt:
  - If `GAMEOPTION_CULTURE_NO_CITY_FLIPPING` is off AND (`CULTURE_FLIPPING_AFTER_CONQUEST` is on OR city was never owned by attacker) AND `numRevolts(attacker) >= NUM_WARNING_REVOLTS`: calls `setOwner(eCulturalOwner)` — permanent flip.
  - Otherwise: increments `numRevolts(attacker)`, sets `occupationTimer` (speed-adjusted `BASE_REVOLT_OCCUPATION_TURNS + iCityStrength100 * REVOLT_OCCUPATION_TURNS_PERCENT / 10000`).
- On "near miss" (roll2 ≥ strength but roll2 - strength < strength): shows "failed revolt" message.
- On "discontent" (roll1 failed but `< 2*iRevoltTestProb`): shows "discontent" message.

`netRevoltRisk100` (CvCity.cpp:6432-6437): `min(10000, max(0, baseRevoltRisk100 * unitRevoltRiskModifier / 100))`.

`baseRevoltRisk100` (CvCity.cpp:6440-6498):
- Starts with `highestPopulation * 2`.
- Adds `(era+1)` per adjacent attacker-owned tile (or just `era+1` if `CULTURE_MIN_CITY_BORDER`).
- Multiplies by nonlinear culture-ratio modifier: `10000 * attackerPercent / max(1, defenderPercent)`.
- Religion modifiers: attacker state religion in city → `REVOLT_OFFENSE_STATE_RELIGION_MODIFIER+100`; defender state religion in city → `REVOLT_DEFENSE_STATE_RELIGION_MODIFIER+100`.

`unitRevoltRiskModifier` (CvCity.cpp:6501-6519): summed `revoltProtectionTotal()` across all garrison units. Positive protection → `10000/(100+garrison)` (diminishing returns); negative → `100 - garrison`.

### 1h. Fort revolt (`checkFortRevolt`)

`CvPlot::checkFortRevolt` (CvPlot.cpp:1133-1162): if improvement `isActsAsCity()` and owned long enough (`ownershipDuration > SUPER_FORTS_DURATION_BEFORE_REVOLT`), and cultural owner is different team with no defending units → immediate flip (`setOwner`), no probability roll.

---

## 2. Current observability

### What is exposed today

| Source | Field | Granularity |
|---|---|---|
| `GET /cities` → `cultureLevel` | City's current `CultureLevelTypes` (enum int) | Per city, snapshotted |
| `GET /cities` → `commerce` | `YIELD_COMMERCE` rate (not just culture) | Per city |
| `GET /players` → `score` | Score (includes culture component) | Per player |
| `[PERF/phase]` `city.doCulture` | Wall-clock time (ms) for doCulture per city owner | Per player, per turn (gated `gPerfLogLevel>=1`) |
| `[PERF/phase]` `city.doPlotCulture` | Wall-clock time for doPlotCulture | Per player, per turn |

### What is NOT exposed (the gap)

The following state is entirely invisible from outside:

| State | Why it matters | Missing surface |
|---|---|---|
| **City culture total** (`getCultureTimes100(owner)`) | The running accumulator — how far to the next border threshold. Cannot compute turns-to-next-expansion without it. | No endpoint field |
| **Per-city culture rate** (`getCommerceRate(COMMERCE_CULTURE)`) | How much culture is added per turn. The `commerce` field on `/cities` is `YIELD_COMMERCE`, not COMMERCE_CULTURE (different: YIELD_COMMERCE feeds all commerce types; COMMERCE_CULTURE is after sliders). | No endpoint field. Confusable with `iCommerce`. |
| **Per-plot culture values** (all players' culture on each tile) | The actual border/ownership competition state; the decay dynamics. | No endpoint. Entire plot-culture array is invisible. |
| **Per-plot cultural owner** | Which player owns each tile post-decay. | No endpoint per-plot. |
| **Revolt count per attacker** (`getNumRevolts(attacker)`) | The warning-revolt threshold for permanent flip (`NUM_WARNING_REVOLTS`). Must track per (city, attacker) pair. | No endpoint |
| **Occupation timer** (`getOccupationTimer()`) | Duration of ongoing revolt-induced occupation. | Not in `/cities` snapshot. |
| **Net revolt risk** (`netRevoltRisk100(attacker)`) | The computed probability of flip — the output of the complex formula combining population, culture ratio, religion, garrison. | No endpoint. Entirely opaque from outside. |
| **Culture-rate-this-turn per player per plot** (`getCultureRateThisTurn(ePlayer)`) | The revolt-trigger gate: `eCulturalOwner.rateThisTurn > owner.rateThisTurn` must hold. Without this, you cannot predict whether a revolt check will fire. | No endpoint |
| **Fixed-borders status per player** | Modifies the ownership threshold. | Not on `/players`. |
| **Game option flags** (`GAMEOPTION_CULTURE_EQUILIBRIUM`, `_NO_CITY_FLIPPING`, `_FLIPPING_AFTER_CONQUEST`, etc.) | These change core decay/ownership logic. | Not on `/diagnostic` or any endpoint. |
| **Improvement culture contribution** (`imp.getCulture()`, `getCultureRange()`) | Plot-tile culture from forts/improvements. Entirely invisible. | No endpoint |
| **Culture threshold (next border)** (`getCultureThreshold()`) | The culture total needed to reach the next level. | No endpoint |
| **`isInCultureRangeOfCityByPlayer`** per plot | Equilibrium decay gating — whether a plot gets slow or fast decay. | No endpoint |

---

## 3. The gap

At **Tier 1 (Telescreen)**, we can answer:
- "What culture level is each city?" (from `cultureLevel` on `/cities`).
- "What is each city's commerce rate?" (from `commerce`, but this is yield-commerce, not the culture-slider output).

We **cannot** answer without looking at the screen:
- How many turns until a city's borders expand?
- Which tiles does player X control? Has that changed this turn?
- Is city Y at risk of revolt? From whom? How many warning revolts have accumulated?
- Is the decay rate standard or equilibrium mode? Is a given tile in fast-decay or slow-decay?
- What culture is flowing from improvements?
- For AI players: any of the above — entirely opaque.

The gap is severe. The culture system has **no log tags whatsoever** (no `[CUL]` domain or equivalent). It has no per-event publication to `/events`. The only culture-related signal is the `cultureLevel` tier enum, which advances only on border expansion (a coarse, infrequent event).

**This is an §A opaque system in the inventory — and it is more opaque than the listing (`◐ partial`) currently implies.** The "owner helped design" the equilibrium model — but the model is entirely unobservable; what the owner designed is invisible to the endpoint layer.

---

## 4. Proposed hooks (concrete additions to climb toward Tier 3/4)

All hooks follow the three canonical observability hook shapes — see [DEC-obs-hook-shapes](../../decisions.md#dec-obs-hook-shapes).

### Hook A — `/cities` snapshot fields (cheapest, immediate Tier 2 lift)

Add to `CitySnap` and the per-city JSON render in `CvHttpServer.cpp`:

| JSON key | Source call | Notes |
|---|---|---|
| `"cultureTimes100"` | `pLoopCity->getCultureTimes100(pLoopCity->getOwner())` | The running accumulator. Divide by 100 for display. |
| `"cultureRate"` | `pLoopCity->getCommerceRate(COMMERCE_CULTURE)` | Culture per turn (slider output, NOT raw YIELD_COMMERCE). |
| `"cultureThreshold"` | `pLoopCity->getCultureThreshold()` | Culture needed for next border expand; -1 = at max. |
| `"occupationTimer"` | `pLoopCity->getOccupationTimer()` | 0 = not in revolt. Non-zero = mid-revolt occupation. |

These four fields fully expose the city-side culture state and let a reader compute turns-to-expansion and detect revolt occupation. They are read-only scalar fields already computed per-turn.

### Hook B — `[CUL]` log domain, `CultureAI.log`, city-scope (`gCityLogLevel`)

New log helper `logCultureAI(int level, const char* fmt, ...)` → `CultureAI.log`, gated by `gCityLogLevel`. Tag prefix `[CUL]`.

Emit at key decision points:

| Tag | Level | Where | Payload |
|---|---|---|---|
| `[CUL/accrual]` | 2 | `CvCity::doCulture` | `turn= owner= city= rate= total= nextAt= turnsLeft=` |
| `[CUL/expand]` | 1 | `CvCity::setCultureLevel` on `eNewValue > eOldValue` | `turn= owner= city= oldLevel= newLevel=` |
| `[CUL/revolt]` | 1 | `CvPlot::checkCityRevolt` on roll1 success | `turn= city= owner= attacker= risk100= roll= result=occupying|flipping|quelled` |
| `[CUL/revolt/accumulate]` | 2 | `CvCity::changeNumRevolts` | `turn= city= owner= attacker= revolts= threshold=` |
| `[CUL/flip]` | 1 | `CvPlot::setOwner` called from revolt or cultural-owner path | `turn= plot= oldOwner= newOwner= reason=revolt|culture` |
| `[CUL/decay]` | 3 | `CvPlot::doCulture` per-player decay applied | `turn= plot= player= from= to= mode=standard|equilibrium|fastDecay` |

Level 1 lines (expand, revolt outcomes, flips) are the observability essentials — they make every border and ownership change visible in the `/events` stream. Level 2 adds the per-city per-turn accrual summary. Level 3 is verbose plot-level decay (use only for targeted investigation).

### Hook C — revolt-risk diagnostic endpoint

Add `GET /diagnostic/revoltRisk?city=N&player=N&attacker=M` to `CvHttpServer.cpp`:

```json
{
  "city": "CITY_NAME",
  "cityId": N,
  "owner": K,
  "attacker": M,
  "cultureTotal": { "owner": 12345, "attacker": 9876 },
  "cultureRate": { "owner": 45, "attacker": 62 },
  "baseRisk100": 2340,
  "unitModifier": 75,
  "netRisk100": 1755,
  "numRevolts": 2,
  "numWarningRevolts": 3,
  "occupationTimer": 0,
  "gameoptions": { "noFlipping": false, "flippingAfterConquest": false, "equilibrium": true }
}
```

This endpoint calls `netRevoltRisk100` / `baseRevoltRisk100` / `unitRevoltRiskModifier` read-only, on the game thread (same mailbox pattern as `canConstruct`). It is the only way to see the revolt risk formula output without a debugger. Game options would be surfaced here as a one-time convenience rather than a permanent snapshot field.

### Hook D — `/players` field: total culture accumulated

Add to `PlayerSnap`:

| JSON key | Source call | Notes |
|---|---|---|
| `"culture"` | `kPlayer.getCulture()` | Player's aggregate culture total (all cities). Needed for score/victory-condition tracking. |
| `"cultureRate"` | `kPlayer.getCommerceRate(COMMERCE_CULTURE)` | Player's per-turn culture output. |

### Hook E — `[CUL/plotflip]` event on open-terrain ownership change

In `CvPlot::doCulture`'s `else setOwner(calculateCulturalOwner(false))` branch (CvPlot.cpp:10883), before the `setOwner` call, check if owner is about to change and log at level 1:

```
[CUL/plotflip] turn= x= y= oldOwner= newOwner= reason=culture
```

This makes every map border shift visible in the event stream. Without it, territory changes are completely silent from outside.

---

## 5. Cascade/tally implications

When the #430 tally replaces per-turn state maintainers, the culture system surfaces as:

- **`cultureTimes100` per city per owner** — a **tally domain** (`DOMAIN_CULTURE`), additive per turn from `getCommerceRate(COMMERCE_CULTURE)`. Currently maintained by `CvCity::doCulture`'s direct accumulation.
- **`cultureLevel`** — an **`autoBuild`-style derived state** (threshold check on `cultureTimes100`; the "border tier"). Currently maintained by `updateCultureLevel`.
- **Plot ownership** — a per-plot **`requires`-style evaluation** (`calculateCulturalOwner`) run each turn to decide who holds the tile. The tally would need a plot-culture domain (`DOMAIN_PLOT_CULTURE`, per-player-per-plot) to replicate it.
- **City revolts** — a probabilistic state machine (`numRevolts`, `occupationTimer`) that is NOT purely derivable from the tally's additive counts. It requires a per-city revolt ledger that is an **opaque §A system** until the `[CUL/revolt*]` hooks are added.

The cascade replacement of the culture border/ownership machinery (§14 H territory) cannot be safely done without:
1. Hooks A + B at minimum (city-side state visible).
2. Hook C for the revolt-risk formula.
3. Hook E for open-terrain flip events.
4. A plot-culture snapshot mechanism (not proposed here, but needed for full Tier 5 coverage).

---

## Summary

| Dimension | Current state |
|---|---|
| **Tier** | 1 — Telescreen |
| **Exposed** | `cultureLevel` (tier int) per city; `[PERF]` timing lines for the doCulture phase |
| **Opaque** | Culture accumulator, per-turn rate, threshold-to-next-expansion, revolt count, revolt risk, occupation timer, per-plot culture values, plot ownership changes, improvement culture, decay mode |
| **Minimum hooks for Tier 3** | Hook A (4 `/cities` fields) + Hook B level 1 (`[CUL/expand]`, `[CUL/revolt]`, `[CUL/flip]`) + Hook D (player culture total) |
| **Hooks for full Tier 4** | All of A+B+C+D+E above |
| **Cascade-blocking gap** | Revolt ledger (`numRevolts`, `occupationTimer`) and per-plot culture values are not observable; any replacement of culture-flip or border-expand logic cannot be shadowed until these are surfaced |
