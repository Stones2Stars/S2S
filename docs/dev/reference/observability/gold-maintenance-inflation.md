# Observability map: Gold, Maintenance & Inflation — the expense side of the economy

> **Status:** reference · **Verified against:** live `Sources/Engine/CvPlayer.cpp` / `CvCity.cpp`,
> `Sources/Tools/CvHttpServer.cpp` — 2026-06-20.
> **Grounding:** function citations confirmed against the named functions in the live source; the old
> draft's line numbers had already drifted (e.g. `calculatePreInflatedCosts` 7947→7949), so every
> citation is "the function named here, near this line" — confirm the function, not the integer.
>
> **BLUF:** the whole expense side of a player's economy — city maintenance, civic/unit upkeep, supply,
> corporate maintenance, treasury tax, and inflation — folds into one opaque net number, `goldRate`, on
> `/players`. That is **Tier 1** ([Telescreen](README.md)): you see the result, never a component. This
> map gives the full per-turn mechanics, what is on the wire today, the gap, and the concrete hooks to
> climb to Tier 3/4.

The observability scale (0–5), the Orwell reconstruction bar, and the three canonical hook shapes are
defined once in [`README.md`](README.md) ([DEC-obs-scale], [DEC-obs-hook-shapes]) — not restated here.
The rules for reading the live surface (logs held open; use `/events` + `/diagnostic`) live in
[`http-server.md`](http-server.md).

---

## 1. How it works — the per-turn mechanics

### 1-A. Per-turn gold application — `doGold` (`Sources/Engine/CvPlayer.cpp`, ~15472)
Runs once per player per turn from `CvPlayer::doTurn` (before city turns):
```
gold += calculateGoldRate()              // net gold this turn
if gold < 0:
    gold = 0; setStrike(true); changeStrikeTurns(+1)
    if strikeTurns > 1: disband floor(strikeTurns/2) units   // forced disbanding
else:
    setStrike(false)
```

### 1-B. Gold rate — `calculateGoldRate` (`CvPlayer.cpp`, ~8224)
```
if isCommerceFlexible(COMMERCE_RESEARCH): goldRate = calculateBaseNetGold()
else:                                     goldRate = min(0, calculateBaseNetResearch() + calculateBaseNetGold())
```
`calculateBaseNetGold` (~8079) = `getCommerceRate(COMMERCE_GOLD)` (city gold at current slider)
`+ getGoldPerTurn()` (trade/deal income, `m_iGoldPerTurn`) `− getFinalExpense()`.

`getFinalExpense` (~8014): `isAnarchy() ? 0 : calculatePreInflatedCosts() * getInflationMod10000() / 10000`.

### 1-C. Pre-inflated costs — `calculatePreInflatedCosts` (`CvPlayer.cpp`, ~7949)
Six additive components (all suppressed during anarchy, which short-circuits before the sum):
```
getTreasuryUpkeep()        // anti-hoarding tax on the treasury balance
+ getTotalMaintenance()    // sum of city maintenance (city-level dirty-cache)
+ getCivicUpkeep()         // upkeep for current civics
+ getFinalUnitUpkeep()     // net unit upkeep (military + civilian)
+ calculateUnitSupply()    // outside-territory unit supply
+ getCorporateMaintenance()// corporation presence costs
```

### 1-D. City maintenance — `CvCity::updateMaintenance` (`Sources/Engine/CvCity.cpp`, ~7599)
Per-city lazy cache (`m_bMaintenanceDirty`). Player total = sum over cities, dirty-cached in
`m_iTotalMaintenance` and returned `/100` (`CvPlayer::updateMaintenance` ~10617, getter ~10728).

City raw value:
```
cityMaintenance = EraInfo.getInitialCityMaintenancePercent()       // era floor
if !isDisorder() && !isWeLoveTheKingDay() && population > 0:
    cityMaintenance += getModifiedIntValue(calculateBaseMaintenanceTimes100(),
                                           getEffectiveMaintenanceModifier())
```

`calculateBaseMaintenanceTimes100` (`CvCity.cpp`, ~7882) — five additive components:

| Component | Function (`CvCity.cpp`, ~line) | Key factors |
|---|---|---|
| **Distance** | `calculateDistanceMaintenanceTimes100` (~7622) | Distance to nearest government center × pop; world-size/handicap/coastal mods; halved for rebels; `isGovernmentCenter()` → 0 |
| **Num-cities** | `calculateNumCitiesMaintenanceTimes100` (~7685) | `(numCities−1) × 72 × (pop+13)/13`; vassal fraction `/(3+vassals)`; world-size/handicap mods; halved for rebels |
| **Colony** | `calculateColonyMaintenanceTimes100` (~7748) | Cities on foreign landmass; `GAMEOPTION_NO_VASSAL_STATES` → 0; capped at `maxColonyMaintenance × distanceMaint` |
| **Corporation** | `calculateCorporationMaintenanceTimes100` (~7791) | HQ commerce + bonus-count × corp maint × pop-factor × handicap; `GAMEOPTION_ADVANCED_REALISTIC_CORPORATIONS` doubles the handicap |
| **Building** | `calculateBuildingMaintenanceTimes100` (~7860) | Only when `GC.getTREAT_NEGATIVE_GOLD_AS_MAINTENANCE()`; sum of negative-gold active buildings × 100 (×50 for rebels) |

`getEffectiveMaintenanceModifier` (`CvCity.cpp`, ~7578) — global modifier stack:
```
iMod = city.maintenanceModifier + player.maintenanceModifier
     + area.totalAreaMaintenanceModifier(owner)
     + (connected && !capital ? player.connectedCityMaintenanceModifier : 0)
```

### 1-E. Civic upkeep — `getCivicUpkeep` (`CvPlayer.cpp`, ~14260)
Sum over civic-option slots of `getSingleCivicUpkeep(currentCivic)`:
```
upkeep = max(0, (population + UPKEEP_POPULATION_OFFSET) × popPercent/100)
       + max(0, (numCities  + UPKEEP_CITY_OFFSET)       × cityPercent/100)
upkeep = getModifiedIntValue(upkeep, upkeepModifier) × handicap.civicUpkeepPercent / 100
if isNormalAI(): apply AI handicap + per-era scaling
```
Halved for rebels; returns ≥1 if a non-zero-upkeep civic is active.

### 1-F. Unit upkeep — `calcFinalUnitUpkeep` (`CvPlayer.cpp`, ~10332)
Net civilian + military upkeep, then handicap-scaled. **Full per-unit mechanics
(accumulators, modifiers, free allowances) are in [`unit-upkeep-supply.md`](unit-upkeep-supply.md).**
```
iCalc = getUnitUpkeepCivilianNet() + getUnitUpkeepMilitaryNet()
if iCalc > 0:
    iCalc × handicap.unitUpkeepPercent / 100
    if !human: × AI handicap × (1 + AIPerEraModifier × era)
```
Returns 0 for NPCs.

### 1-G. Unit supply — `calculateUnitSupply` (`CvPlayer.cpp`, ~7909)
Outside-territory unit cost; see [`unit-upkeep-supply.md`](unit-upkeep-supply.md) §1-E for the full path.
```
paidUnits = max(0, getNumOutsideUnits() − INITIAL_FREE_OUTSIDE_UNITS)
baseCost  = paidUnits × INITIAL_OUTSIDE_UNIT_GOLD_PERCENT / 100 × (era + 1)
iMod      = distantUnitSupportCostModifier + (AI: AIUnitSupplyPercent − 100 + AIPerEraModifier × era)
supply    = getModifiedIntValue(baseCost, iMod)
```
Returns 0 during anarchy or for NPCs.

### 1-H. Treasury upkeep — `getTreasuryUpkeep` (`CvPlayer.cpp`, ~14276)
Anti-hoarding tax on the current treasury balance, scaled by game speed:
```
treasuryUpkeep = (gold + 250 × sqrt(gold)) / (25 × gameSpeed.speedPercent)
```

### 1-I. Inflation — `getInflationMod10000` (`CvPlayer.cpp`, ~7965)
Returns `10000 + inflationPerTurnTimes10000`. The per-turn component:
```
iInflationPerTurnTimes10000 = 100 × hurriedCount × handicap.inflationPercent / 100
iMod = inflationModifier (events) + getCivicInflation() + getProjectInflation()
     + getTechInflation() + getBuildingInflation() − 100 × isRebel()
if iMod != 0: apply to iInflationPerTurnTimes10000
if isNormalAI():
    iMod2 = handicap.AIInflationPercent − 100 + handicap.AIPerEraModifier × era
    if iMod2 != 0: apply
```
`getInflationCost()` (~8010) = `preInflatedCosts × (inflationMod10000 − 10000) / 10000` (the extra above
pre-inflation; zero during anarchy).

Hurry-inflation decay — `doAdvancedEconomy` (`CvPlayer.cpp`, ~27833, called from `doTurn`): when
`hurriedCount > 0`, decays it on a game-speed-scaled cadence (`HURRY_INFLATION_DECAY_RATE`, modified by
`hurryInflationModifier%`).

### 1-J. Slider auto-correction — `verifyGoldCommercePercent` (`CvPlayer.cpp`, ~17974)
Called before `doGold`. Silently raises the gold slider while the player would go into deficit:
```
while gold + calculateGoldRate() < 0:
    commercePercent(GOLD) += COMMERCE_PERCENT_CHANGE_INCREMENTS
    if percent == 100: break
```

### 1-K. Per-turn call order in `CvPlayer::doTurn` (`CvPlayer.cpp`, ~3683)
`verifyGoldCommercePercent()` → `doGold()` → `updateCorporateMaintenance()` (when
`GAMEOPTION_ADVANCED_REALISTIC_CORPORATIONS`) → `doAdvancedEconomy()` (decay `hurriedCount`).

---

## 2. What's on the wire today — Tier 1 (Telescreen)

`GET /players` (`Sources/Tools/CvHttpServer.cpp`, snapshot publish + `renderPlayers`):

| JSON field | C++ source | Notes |
|---|---|---|
| `gold` | `kPlayer.getGold()` | Treasury balance, ≤5s stale |
| `goldRate` | `kPlayer.calculateGoldRate()` | Net gold/turn — the single aggregated number |

`GET /cities` per-city `commerce` = `pLoopCity->getYieldRate(YIELD_COMMERCE)` — raw commerce yield, **not**
gold output (slider/division not applied).

Everything on the expense side is invisible: `getTotalMaintenance()`, per-city
maintenance + its components, `getCivicUpkeep()`, `getFinalUnitUpkeep()`, `calculateUnitSupply()`,
`getCorporateMaintenance()`, `getTreasuryUpkeep()`, `getFinalExpense()`, `calculatePreInflatedCosts()`,
`getInflationMod10000()`, `getInflationCost()`, `getHurriedCount()`, the inflation source breakdown
(`getBuildingInflation()`/`getCivicInflation()`/…), `isStrike()`/`getStrikeTurns()`, `isAnarchy()`,
`getCommercePercent(COMMERCE_GOLD)` (tax slider), `getGoldPerTurn()`, and the AI financial gates
(`AI_isFinancialTrouble()`, `AI_fundingHealth()`, `AI_goldTarget()`, `getProfitMargin()`).

---

## 3. The gap — what cannot be reconstructed from outside

Given only `/players` + `/cities` + `/events` + gated logs, an agent **cannot** determine, for any player:

1. **Why `goldRate` is what it is** — the net is exposed; not a single cost component. An AI shedding gold
   cannot be attributed to maintenance vs upkeep vs civic vs inflation vs corporate drag.
2. **The per-city maintenance bill** — no per-city cost in `/cities`; only raw `commerce` yield.
3. **The maintenance modifier stack** — distance/num-cities/coastal/corporation/connected/area modifiers.
4. **The inflation state** — `hurriedCount`, the multiplier, and every contributing factor.
5. **The treasury-upkeep tax** — a non-obvious progressive cost on the balance itself; reconciling
   `goldRate` from visible components would misattribute the gap.
6. **The AI financial-trouble state** — `AI_isFinancialTrouble()` ↔ `AI_fundingHealth() < AI_safeFunding()`
   is the primary financial gate (slider, unit production, trade declines) and is unreadable.
7. **The strike / bankrupt state** — `isStrike()`/`strikeTurns` triggers disbanding; a `/units` count drop
   can't be distinguished from normal attrition.
8. **Corporate maintenance split** — only the aggregate exists, and it isn't exposed; per-corp is invisible.
9. **The gold-slider auto-correction** — `verifyGoldCommercePercent` lifts the AI tax rate silently and the
   slider position isn't exposed, so the income side can't be verified either.

---

## 4. Proposed hooks — climb to Tier 3/4

All hooks follow the three canonical shapes ([DEC-obs-hook-shapes]).

### 4-A. New `/players` snapshot fields (`CvHttpServer.cpp` — snapshot publish + `renderPlayers`)
`finalExpense`, `preInflatedCosts`, `inflationMod` (`getInflationMod10000()`), `hurriedCount`,
`civicUpkeep`, `unitUpkeep` (`getFinalUnitUpkeep()`), `unitSupply`, `treasuryUpkeep`, `totalMaintenance`,
`corpMaintenance`, `goldPerTurn`, `isStrike`, `strikeTurns`, `goldSlider`
(`getCommercePercent(COMMERCE_GOLD)`), `isFinancialTrouble`
(`isNormalAI() ? AI_isFinancialTrouble() : false`), `fundingHealth` (AI only), `isAnarchy`.
All are `const` calls already invoked elsewhere — one call per player at publish.

### 4-B. New `/cities` snapshot fields
`maintenance` (`getMaintenance()`), `distanceMaint`, `numCitiesMaint`, `colonyMaint`, `corpMaint`,
`buildingMaint` (each `calculate*MaintenanceTimes100() / 100`), `maintenanceMod`
(`getEffectiveMaintenanceModifier()`), `goldOutput` (`getCommerceRateTimes100(COMMERCE_GOLD) / 100`).

### 4-C. Gated `[FIN]` log — `FinanceAI.log`, `gPlayerLogLevel`
Add `logFinanceAI(level, fmt, …)` to `Sources/AI/BetterBTSAI.{h,cpp}`, tag `[FIN]`:
- `[FIN/turn]` (lvl 1, per player per turn from `doGold`): `gold/goldRate/expense/preInflated/inflationMod/
  inflationCost/maint/civicUpkeep/unitUpkeep/supply/corpMaint/treasuryUpkeep/goldPT/goldSlider/isStrike/strikeTurns`
  — a complete per-turn snapshot, `/events`-streamable at level 1.
- `[FIN/strike]` (lvl 1, on `setStrike(true)` in `doGold`): `gold/goldRate/strikeTurns/disbanding`.
- `[FIN/inflation]` (lvl 2, when inflation > 0): `hurriedCount/mod` + source breakdown
  (`building/civic/project/tech/event/rebel`).
- `[FIN/aitroubled]` (lvl 1, **state-change only** — maintain `m_bLastFinancialTrouble`; the gate is called
  many times/turn): `state/fundingHealth/safeFunding/goldTarget/gold/goldRate`.
- `[FIN/slider]` (lvl 2, when `verifyGoldCommercePercent` raises the slider): `oldSlider/newSlider/gold`.

### 4-D. `/diagnostic/financeBreakdown?player=N` (optional, Tier 4)
Full breakdown on demand (mailbox / game-thread, the `placementSweep` pattern): `gold`, `goldRate`,
`finalExpense`, `preInflated`, `inflationMod`/`inflationCost`, a `components` block
(treasuryUpkeep/totalMaintenance/civicUpkeep/unitUpkeep/unitSupply/corpMaintenance), an `inflationSources`
block (building/civic/project/tech/event/rebel/hurriedCount), `isStrike`/`strikeTurns`, `goldSlider`,
`isAnarchy`, `aiFinancialTrouble`.

---

## 5. Tier self-assessment

| Tier | Status |
|---|---|
| **1** Telescreen | **MET** — `/players` `gold` + `goldRate`, but `goldRate` is an opaque net |
| **2** Informant | NOT met for finance (buildability diagnostics are orthogonal) |
| **3** Big Brother | NOT met — would be reached by §4-A `/players` + §4-B `/cities` + `[FIN/turn]`/`[FIN/aitroubled]` |
| **4** Thought Police | NOT met — `/diagnostic/financeBreakdown` (§4-D) closes it |

**Current tier: 1.** The gap is larger than it looks: finance is the primary lever the AI uses to decide
whether to build units, research, adopt civics, and make diplomatic offers — all invisible from `/events`
when financially motivated.

---

## See also
- [`README.md`](README.md) — the observability scale + the three canonical hook shapes ([DEC-obs-scale], [DEC-obs-hook-shapes]) this map assesses against.
- [`http-server.md`](http-server.md) — the live surface these snapshot fields/endpoints attach to, and the live-read rules.
- [`unit-upkeep-supply.md`](unit-upkeep-supply.md) — the per-unit upkeep + supply mechanics that feed §1-F/§1-G's `getFinalUnitUpkeep`/`calculateUnitSupply` aggregates here.
- [`../../explanation/cascade-architecture.md`](../../explanation/cascade-architecture.md) — why total observability is load-bearing ([DEC-map-before-delete](../../architecture/decisions.md#dec-map-before-delete)).
- [`../../README.md`](../../README.md) — the comprehension map / overview-of-overviews.

[DEC-obs-scale]: ../../architecture/decisions.md#dec-obs-scale
[DEC-obs-hook-shapes]: ../../architecture/decisions.md#dec-obs-hook-shapes
