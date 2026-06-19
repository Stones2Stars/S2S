# Observability map: Gold, Maintenance & Inflation

> DRAFT observability map (2026-06-18, parent cascade-mapping-inventory sweep) — all
> mechanics claims cited from live source; verify before relying.  Tier assessment: **1**
> (Telescreen, bordering 2 in a narrow band for gold/goldRate).

---

## 1. How it actually works — the full per-turn mechanics

### 1-A. Per-turn gold application — `doGold` (CvPlayer.cpp:15472)

Executed once per player per turn at `CvPlayer::doTurn` line 3809, before city turns run.

```
gold += calculateGoldRate()          // net gold this turn
if gold < 0:
    gold = 0
    setStrike(true); changeStrikeTurns(+1)
    if strikeTurns > 1:
        disband floor(strikeTurns/2) units   // forced disbanding to recover
else:
    setStrike(false)
```

### 1-B. Gold rate calculation — `calculateGoldRate` (CvPlayer.cpp:8224)

```
if isCommerceFlexible(COMMERCE_RESEARCH):       // player can set science slider
    goldRate = calculateBaseNetGold()
else:
    goldRate = min(0, calculateBaseNetResearch() + calculateBaseNetGold())
```

`calculateBaseNetGold` (CvPlayer.cpp:8079):
```
baseNetGold = getCommerceRate(COMMERCE_GOLD)    // city gold output at current slider
            + getGoldPerTurn()                   // per-turn from trade routes / deals (m_iGoldPerTurn)
            - getFinalExpense()                  // total spending post-inflation
```

`getFinalExpense` (CvPlayer.cpp:8012):
```
finalExpense = isAnarchy() ? 0
             : calculatePreInflatedCosts() * getInflationMod10000() / 10000
```

### 1-C. Pre-inflated costs — `calculatePreInflatedCosts` (CvPlayer.cpp:7947)

```
preInflatedCosts = getTreasuryUpkeep()           // anti-hoarding tax
                 + getTotalMaintenance()          // sum of city maintenance (city-level dirty-cache)
                 + getCivicUpkeep()              // upkeep for current civics
                 + getFinalUnitUpkeep()          // net unit upkeep (military + civilian)
                 + calculateUnitSupply()         // outside-territory unit supply
                 + getCorporateMaintenance()     // corporation presence costs
```
All zero during anarchy (`isAnarchy()` short-circuits before the sum).

### 1-D. City maintenance — `CvCity::updateMaintenance` (CvCity.cpp:7599)

Per-city lazy-cached value (`m_bMaintenanceDirty`). Player total = sum over all cities via
`updateMaintenance` at CvPlayer.cpp:10617 (dirty-cached in `m_iTotalMaintenance`; returned
`/100` at line 10728).

**City raw value (CvCity.cpp:7599):**
```
cityMaintenance = EraInfo.getInitialCityMaintenancePercent()   // era-era floor
if !isDisorder() && !isWeLoveTheKingDay() && population > 0:
    cityMaintenance += getModifiedIntValue(
        calculateBaseMaintenanceTimes100(),
        getEffectiveMaintenanceModifier()
    )
```

**`calculateBaseMaintenanceTimes100` (CvCity.cpp:7882) — five additive components:**

| Component | Function | Key factors |
|---|---|---|
| **Distance** | `calculateDistanceMaintenanceTimes100` (CvCity.cpp:7622) | Distance to nearest government-center × pop; world-size/handicap/coastal modifiers; halved for rebels; `isGovernmentCenter()` → 0 |
| **Num-cities** | `calculateNumCitiesMaintenanceTimes100` (CvCity.cpp:7685) | `(numCities-1) × 72 × (pop+13)/13`; vassal city fraction `/ (3+vassals)`; world-size/handicap modifiers; halved for rebels |
| **Colony** | `calculateColonyMaintenanceTimes100` (CvCity.cpp:7748) | Cities on foreign landmass; `GAMEOPTION_NO_VASSAL_STATES` → 0; capped at `maxColonyMaintenance × distanceMaint` |
| **Corporation** | `calculateCorporationMaintenanceTimes100` (CvCity.cpp:7791) | HQ commerce + bonus-count × corp maintenance × pop-factor × handicap; `GAMEOPTION_ADVANCED_REALISTIC_CORPORATIONS` doubles the handicap |
| **Building** | `calculateBuildingMaintenanceTimes100` (CvCity.cpp:7860) | Only when `GC.getTREAT_NEGATIVE_GOLD_AS_MAINTENANCE()`; sum of negative-gold active buildings × 100 (or 50 for rebels) |

**`getEffectiveMaintenanceModifier` (CvCity.cpp:7578) — global modifier stack:**
```
iMod = city.maintenanceModifier
     + player.maintenanceModifier
     + area.totalAreaMaintenanceModifier(owner)
     + (connected && !capital ? player.connectedCityMaintenanceModifier : 0)
```

### 1-E. Civic upkeep — `getCivicUpkeep` (CvPlayer.cpp:14260)

Sum across all civic-option slots of `getSingleCivicUpkeep(currentCivic)`. Each:
```
upkeep = max(0, (population + UPKEEP_POPULATION_OFFSET) × popPercent/100)
       + max(0, (numCities  + UPKEEP_CITY_OFFSET)       × cityPercent/100)
upkeep = getModifiedIntValue(upkeep, upkeepModifier)
upkeep × handicap.civicUpkeepPercent / 100
if isNormalAI(): apply AI handicap + per-era scaling
```
Halved for rebels. Returns at least 1 if a non-zero upkeep civic is active.

### 1-F. Unit upkeep — `calcFinalUnitUpkeep` (CvPlayer.cpp:10332)

```
iCalc = getUnitUpkeepCivilianNet()    // max(0, civilian_100 × civUpkeepMod / 100) − freeCivilian
      + getUnitUpkeepMilitaryNet()    // max(0, military_100 × milUpkeepMod / 100) − freeMilitary
if iCalc > 0:
    iCalc × handicap.unitUpkeepPercent / 100
    if !human: × AI handicap × (1 + AIPerEraModifier × era)
```
Returns 0 for NPCs.  `m_iUnitUpkeepMilitary100` / `m_iUnitUpkeepCivilian100` are the raw sums
accumulated at unit creation/deletion.

### 1-G. Unit supply — `calculateUnitSupply` (CvPlayer.cpp:7909)

```
paidUnits = max(0, getNumOutsideUnits() − INITIAL_FREE_OUTSIDE_UNITS)
baseCost  = paidUnits × INITIAL_OUTSIDE_UNIT_GOLD_PERCENT / 100 × (era + 1)
iMod      = distantUnitSupportCostModifier
            + (AI: AIUnitSupplyPercent − 100 + AIPerEraModifier × era)
supply    = getModifiedIntValue(baseCost, iMod)
```
Returns 0 during anarchy or for NPCs.

### 1-H. Treasury upkeep — `getTreasuryUpkeep` (CvPlayer.cpp:14276)

Anti-hoarding tax on the current treasury balance:
```
treasuryUpkeep = (gold + 250 × sqrt(gold))
               / (25 × gameSpeed.speedPercent)
```
Scales with game speed so that larger expected treasuries on slower speeds pay proportionally.

### 1-I. Inflation — `getInflationMod10000` (CvPlayer.cpp:7963)

Returns `10000 + inflationPerTurnTimes10000`.  The per-turn component:
```
iInflationPerTurnTimes10000 = 100 × hurriedCount
iInflationPerTurnTimes10000 × handicap.inflationPercent / 100

iMod = inflationModifier              // from events (CvPlayer.cpp:22121)
     + getCivicInflation()            // from civics (processsCivics line 18121)
     + getProjectInflation()          // from projects
     + getTechInflation()             // from techs
     + getBuildingInflation()         // from buildings (line 7399)
     − 100 × isRebel()
if iMod != 0: apply to iInflationPerTurnTimes10000

if isNormalAI():
    iMod2 = handicap.AIInflationPercent − 100
           + handicap.AIPerEraModifier × era
    if iMod2 != 0: apply to iInflationPerTurnTimes10000
```
`getInflationCost()` (CvPlayer.cpp:8006) = `preInflatedCosts × (inflationMod10000 − 10000) / 10000`
(the "extra" cost above pre-inflation).  Zero during anarchy.

**Hurry-inflation decay — `doAdvancedEconomy` (CvPlayer.cpp:27833, called at doTurn:3832):**
```
if hurriedCount > 0:
    iTurnIncrement1000 = HURRY_INFLATION_DECAY_RATE × gameSpeed.speedPercent × (1 + hurryInflationModifier%)
    if (elapsedTurns % max(1, iTurnIncrement1000/1000)) == 0:
        hurriedCount -= clipped decay step
```

### 1-J. Commerce slider auto-correction — `verifyGoldCommercePercent` (CvPlayer.cpp:17974)

Called at doTurn line 3807, before `doGold`:
```
while gold + calculateGoldRate() < 0:
    commercePercent(GOLD) += COMMERCE_PERCENT_CHANGE_INCREMENTS
    if percent == 100: break
```
Silently raises the gold slider when the player would go into deficit.

### 1-K. Per-turn call order in `CvPlayer::doTurn` (CvPlayer.cpp:3683)

```
3807  verifyGoldCommercePercent()   // auto-raise gold slider if needed
3809  doGold()                      // apply calculateGoldRate(), handle strike
3828  updateCorporateMaintenance()  // (when GAMEOPTION_ADVANCED_REALISTIC_CORPORATIONS)
3832  doAdvancedEconomy()           // decay hurriedCount
```

---

## 2. Current observability — tier and what is exposed vs not

**Tier: 1 — Telescreen (partial edge into 2)**

### 2-A. What is exposed today

`GET /players` (or `?playerNumber=N`) snapshot — CvHttpServer.cpp:285, 1535:

| JSON field | C++ source | Notes |
|---|---|---|
| `gold` | `kPlayer.getGold()` | Treasury balance, snapshot-stale (≤5s) |
| `goldRate` | `kPlayer.calculateGoldRate()` | Net gold per turn — the single aggregated number |

`GET /cities` snapshot — per-city `commerce` field:

| JSON field | C++ source | Notes |
|---|---|---|
| `commerce` | `pLoopCity->getYieldRate(YIELD_COMMERCE)` | Raw commerce yield (not gold output; slider/division not applied) |

### 2-B. What is NOT exposed today

The entire expense side is invisible. No endpoint or log emits any of:

| State | C++ accessor | Why it matters |
|---|---|---|
| `totalMaintenance` | `getTotalMaintenance()` | The aggregated city-maintenance bill |
| Per-city maintenance | `getMaintenanceTimes100()` | Can't attribute bill to a city; can't watch per-city pressure |
| Per-city maintenance components | `calculateDistanceMaintenanceTimes100()`, etc. | Can't explain *why* a city is expensive |
| `civicUpkeep` | `getCivicUpkeep()` | Civic cost contribution |
| `unitUpkeep` | `getFinalUnitUpkeep()` (= civilian + military nets) | Unit cost contribution |
| `unitSupply` | `calculateUnitSupply()` | Outside-territory supply cost |
| `corporateMaintenance` | `getCorporateMaintenance()` | Corporate presence cost |
| `treasuryUpkeep` | `getTreasuryUpkeep()` | Anti-hoarding tax on gold balance |
| `finalExpense` | `getFinalExpense()` | Total post-inflation spend |
| `preInflatedCosts` | `calculatePreInflatedCosts()` | Pre-inflation subtotal |
| `inflationMod10000` | `getInflationMod10000()` | Multiplier (10000 = 1×) |
| `inflationCost` | `getInflationCost()` | The delta added by inflation |
| `hurriedCount` | `getHurriedCount()` | Accumulated hurry-inflation |
| `hurryInflationModifier` | `getHurryInflationModifier()` | Civic modifier to hurry-decay rate |
| Inflation component breakdown | `getBuildingInflation()`, `getCivicInflation()`, etc. | Which factor is driving inflation |
| `isStrike` / `strikeTurns` | `isStrike()`, `getStrikeTurns()` | Bankrupt / unit-disbanding state |
| `isAnarchy` | `isAnarchy()` | Zero-expense suppression flag |
| `commercePercent(GOLD)` | `getCommercePercent(COMMERCE_GOLD)` | Tax-slider position (auto-raised by verifyGoldCommercePercent) |
| `profitMargin` | `getProfitMargin()` | AI financial health metric |
| `fundingHealth` | `AI_fundingHealth()` | AI financial-trouble test input |
| `isFinancialTrouble` | `AI_isFinancialTrouble()` | AI decision gate — invisible to agents |
| `goldTarget` | `AI_goldTarget()` | AI's current gold savings target |
| `getGoldPerTurn` | `getGoldPerTurn()` (m_iGoldPerTurn) | Trade-route / deal gold; not in snapshot |
| `minTaxIncome` / `maxTaxIncome` | `getMinTaxIncome()`, `getMaxTaxIncome()` | Slack range for slider |

**AI-specific opaque facts:** `AI_isFinancialTrouble()` drives research-slider, production,
unit-build, and diplomatic gold decisions but is completely invisible from outside.  The
AI's effective commerce slider (`getCommercePercent(COMMERCE_GOLD)`) is also not exposed,
so we can't tell what tax rate an AI player is running at.

---

## 3. The gap — what cannot be reconstructed from outside today

Given only the HTTP endpoints + `/events` + gated logs, an agent watching a running game
**cannot** determine any of the following for any player (human or AI):

1. **Why `goldRate` is what it is.** The net number is exposed; not a single cost component.
   For an AI player shedding gold, the agent cannot attribute it to maintenance, unit upkeep,
   civic cost, inflation, or corporate drag.

2. **The per-city maintenance bill.** No per-city cost is in the `/cities` endpoint.  The only
   per-city field touching finance is `commerce` (raw yield, not gold output).

3. **The maintenance modifier stack.** Player-level modifiers (distance, num-cities, coastal,
   corporation, connected-city, home/other area) are not observable.

4. **The inflation state.** `hurriedCount`, the inflation multiplier, and all of its
   contributing factors (building/civic/project/tech/event) are invisible.  The agent
   cannot determine whether a high expense is driven by inflation vs. raw costs.

5. **The treasury-upkeep anti-hoarding tax.** This is a non-obvious progressive cost on the
   gold balance itself; an agent would misattribute the discrepancy if it tried to reconcile
   `goldRate` from visible components.

6. **The AI financial-trouble state.** `AI_isFinancialTrouble()` ↔ `AI_fundingHealth()` <
   `AI_safeFunding()` determines whether an AI raises its gold slider, holds off on unit
   production, declines trades, etc.  This is the primary financial-state gate and nothing
   about it is readable from outside.

7. **The strike / bankrupt state.** `isStrike()` / `strikeTurns` triggers unit disbanding but
   is not in any endpoint.  An agent watching `/units` count drop cannot tell forced-disbanding
   from normal attrition.

8. **Corporate maintenance split.** Only the aggregate `getCorporateMaintenance()` is exposed
   nowhere; per-corporation costs are completely invisible.

9. **The gold-slider auto-correction.** `verifyGoldCommercePercent` silently lifts the tax
   rate before the gold calculation runs; an agent cannot see the slider position for AI
   players and therefore cannot verify the income side either.

---

## 4. Proposed hooks — concrete additions to climb a tier

All hooks follow existing patterns: `key=value` log lines via a new `[FIN]` domain tag,
plus new fields on `/players` and new per-city fields on `/cities`, all gated by
`gPlayerLogLevel` / `Autolog__HttpServer`.

### 4-A. New `/players` snapshot fields (CvHttpServer.cpp — add to `publishIfDue` snapshot
and `renderPlayers`)

| New JSON key | C++ source | What it enables |
|---|---|---|
| `finalExpense` | `kPlayer.getFinalExpense()` | Total cost after inflation — the denominator for the whole finance picture |
| `preInflatedCosts` | `kPlayer.calculatePreInflatedCosts()` | Pre-inflation subtotal for deriving inflation overhead |
| `inflationMod` | `kPlayer.getInflationMod10000()` | Inflation multiplier (10000 = no inflation) |
| `hurriedCount` | `kPlayer.getHurriedCount()` | Accumulated hurry pressure |
| `civicUpkeep` | `kPlayer.getCivicUpkeep()` | Civic cost component |
| `unitUpkeep` | `kPlayer.getFinalUnitUpkeep()` | Unit cost component |
| `unitSupply` | `kPlayer.calculateUnitSupply()` | Supply cost for outside-territory units |
| `treasuryUpkeep` | `kPlayer.getTreasuryUpkeep()` | Anti-hoarding tax |
| `totalMaintenance` | `kPlayer.getTotalMaintenance()` | Sum of city maintenance |
| `corpMaintenance` | `kPlayer.getCorporateMaintenance()` | Corporate presence cost |
| `goldPerTurn` | `kPlayer.getGoldPerTurn()` | Trade/deal income (separate from commerce) |
| `isStrike` | `kPlayer.isStrike() ? 1 : 0` | Bankrupt flag |
| `strikeTurns` | `kPlayer.getStrikeTurns()` | Consecutive bankrupt turns (drives disbanding) |
| `goldSlider` | `kPlayer.getCommercePercent(COMMERCE_GOLD)` | Tax slider position (critical for AI players) |
| `isFinancialTrouble` | `(kPlayer.isNormalAI() ? kPlayer.AI_isFinancialTrouble() : false) ? 1 : 0` | AI financial-trouble gate |
| `fundingHealth` | AI players only: `AI_fundingHealth()` | AI's funding health score |
| `isAnarchy` | `kPlayer.isAnarchy() ? 1 : 0` | Zero-expense flag |

Implementation note: `calculatePreInflatedCosts()`, `calculateUnitSupply()`, and
`getInflationMod10000()` are `const` pure calculations; the expense sub-fields
(`getTreasuryUpkeep()`, `getCivicUpkeep()`, etc.) are also `const`.  Snapshot cost at
publish is one call per player through these already-called functions.

### 4-B. New `/cities` snapshot fields

| New JSON key | C++ source | What it enables |
|---|---|---|
| `maintenance` | `pCity->getMaintenance()` | Per-city maintenance total |
| `distanceMaint` | `pCity->calculateDistanceMaintenanceTimes100() / 100` | Distance component |
| `numCitiesMaint` | `pCity->calculateNumCitiesMaintenanceTimes100() / 100` | City-count component |
| `colonyMaint` | `pCity->calculateColonyMaintenanceTimes100() / 100` | Colony component |
| `corpMaint` | `pCity->calculateCorporationMaintenanceTimes100() / 100` | Corporate component |
| `buildingMaint` | `pCity->calculateBuildingMaintenanceTimes100() / 100` | Negative-gold building component |
| `maintenanceMod` | `pCity->getEffectiveMaintenanceModifier()` | The modifier that scales the above |
| `goldOutput` | `pCity->getCommerceRateTimes100(COMMERCE_GOLD) / 100` (or similar) | City's actual gold output after slider |

### 4-C. Per-turn gated log tag: `[FIN]` in a new `FinanceAI.log`, `gPlayerLogLevel`

Add `logFinanceAI(int level, const char* fmt, ...)` to `BetterBTSAI.{h,cpp}`, writing
`FinanceAI.log`, gated by `gPlayerLogLevel`.  Tag prefix `[FIN]`.

**`[FIN/turn]` (level 1) — one line per player per turn from `doGold` (CvPlayer.cpp:15472):**
```
[FIN/turn] turn=N player=P gold=G goldRate=R
  expense=E preInflated=PI inflationMod=IM inflationCost=IC
  maint=M civicUpkeep=CU unitUpkeep=UU supply=US corpMaint=CM treasuryUpkeep=TU
  goldPT=GPT goldSlider=GS isStrike=0 strikeTurns=0
```
This one line per player per turn gives a complete per-turn financial snapshot that survives
log rotation and is `/events`-streamable at level 1.

**`[FIN/strike]` (level 1) — on `setStrike(true)` (CvPlayer.cpp:15484):**
```
[FIN/strike] turn=N player=P gold=G goldRate=R strikeTurns=T disbanding=D
```
Flags the moment a player goes bankrupt and how many units are disbanded.

**`[FIN/inflation]` (level 2) — on `getInflationMod10000` when inflation > 0:**
```
[FIN/inflation] player=P hurriedCount=HC mod=IM
  building=BI civic=CI project=PI tech=TI event=EI rebel=0
```
Breaks down which inflation source is dominant.

**`[FIN/aitroubled]` (level 1) — when `AI_isFinancialTrouble()` changes state (CvPlayerAI.cpp:3923):**
```
[FIN/aitroubled] turn=N player=P state=0/1
  fundingHealth=FH safeFunding=SF goldTarget=GT gold=G goldRate=R
```
This makes the AI financial-trouble gate — currently invisible — visible in the event stream.
Since `AI_isFinancialTrouble` is called frequently (many production/research decisions per turn),
this should be emitted on **state change only** (maintain `m_bLastFinancialTrouble` bool), not
every call.

**`[FIN/slider]` (level 2) — when `verifyGoldCommercePercent` raises the slider (CvPlayer.cpp:17978):**
```
[FIN/slider] turn=N player=P oldSlider=X newSlider=Y gold=G
```
Makes the silent auto-correction visible.

### 4-D. `/diagnostic/financeBreakdown?player=N` endpoint (optional Tier 4 addition)

A new diagnostic endpoint that returns the full finance breakdown on demand:
```json
{
  "player": N, "turn": T,
  "gold": G, "goldRate": R,
  "finalExpense": E, "preInflated": PI,
  "inflationMod": IM, "inflationCost": IC,
  "components": {
    "treasuryUpkeep": TU, "totalMaintenance": M,
    "civicUpkeep": CU, "unitUpkeep": UU,
    "unitSupply": US, "corpMaintenance": CM
  },
  "inflationSources": {
    "building": BI, "civic": CI, "project": PI, "tech": TI,
    "event": EI, "rebel": RB, "hurriedCount": HC
  },
  "isStrike": false, "strikeTurns": 0,
  "goldSlider": GS, "isAnarchy": false,
  "aiFinancialTrouble": null
}
```
This is evaluated on the game thread via the mailbox pattern (same as `placementSweep`), never
the server thread.

---

## 5. Tier self-assessment

| Tier | Name | Status |
|---|---|---|
| **0** Oblivious | Nothing visible | Met — HTTP server exists |
| **1** Telescreen | `/players` `gold` + `goldRate` | **MET** — but goldRate is an opaque net |
| **2** Informant | + buildability diagnostics | Met for buildings; NOT met for finance |
| **3** Big Brother | + per-turn stream + maintainer shadows | NOT met for finance |
| **4** Thought Police | + every maintainer shadowed | NOT met for finance |
| **5** Thought Police | + every decision input readable | NOT met for finance |

**Gold/maintenance/inflation is at Tier 1 today.**  The proposed hooks in §4 (new `/players`
fields + `/cities` maintenance fields + `[FIN/turn]` log line) would bring it to **Tier 3**
(the Big Brother: per-turn stream exposes every component and `[FIN/aitroubled]` makes the
AI decision gate visible).  The `/diagnostic/financeBreakdown` endpoint would bring it
to **Tier 4**.

The gap is larger than it first appears because the finance system is the primary lever the
AI uses to decide whether to build units, research techs, adopt civics, and make diplomatic
offers — all of which are currently invisible from the event stream when financially
motivated.

---

*Cross-references:*
- `../plans/cascade-mapping-inventory.md` — §A opaque systems; §D the Observability Scale
- `../http-server.md` — the live surface
- `../ai-logging-reference.md` — the tag taxonomy; add `[FIN]` to §2 when hooks land
- `CvPlayer.cpp` — finance functions; see line references above
- `CvCity.cpp` — city maintenance; see line references above
