# Observability map: Food, yields & wastage

> DRAFT observability map (parent task 2026-06-18) — all mechanics claims cited from live
> code; verify before relying. Line numbers are anchors at time of writing and will drift.

---

## 1. How it actually works — per-turn mechanics

### 1.1 The per-turn food tick (CvCity.cpp:1329)

Every `CvCity::doTurn` calls:

```cpp
changeFood(foodDifference(), true);   // CvCity.cpp:1329
```

`foodDifference()` is the *signed net food* this turn: positive = growth, negative = starvation.
`bHandleGrowth=true` means `changeFood` itself handles population changes and granary bookkeeping.

### 1.2 Gross food output: `getYieldRate(YIELD_FOOD)` (CvCity.cpp:11231)

```
getYieldRate(YIELD_FOOD) = getYieldRate100(YIELD_FOOD) / 100
```

`getYieldRate100` (CvCity.cpp:11236):
```
min(CITY_MAX_YIELD_RATE,
    max(100,
        (getBaseYieldRate(YIELD_FOOD) + getSpecialistYieldTotal(YIELD_FOOD))
          * getBaseYieldRateModifier(YIELD_FOOD)
        + 100 * getExtraYield(YIELD_FOOD)
    )
)
```

Broken down:
- **`getBaseYieldRate(YIELD_FOOD)`** (CvCity.cpp:22814): plot tiles worked by the city
  (`m_aiBaseYieldRate[YIELD_FOOD]` = sum of worked tile yields) + trade yield + free city yield
  (golden-age bonus included here if active).
- **`getSpecialistYieldTotal(YIELD_FOOD)`** (CvCity.cpp:11341): sum of food from specialists
  (free specialists + assigned specialists × their food yield).
- **`getBaseYieldRateModifier(YIELD_FOOD)`** (CvCity.cpp:11207): 100 + bonus-yield modifier +
  building yield modifier + event modifier + player event modifier + power yield modifier
  (if powered) + area yield modifier + capital yield modifier. Returns a percentage; the final
  rate is the base × this / 100.
- **`getExtraYield(YIELD_FOOD)`** (CvCity.cpp:11313): flat extra food = `m_aiExtraYield[YIELD_FOOD]`
  (event/misc flat adder) + `getBuildingExtraYield100(YIELD_FOOD) / 100` (per-building flat yields)
  + `getBaseYieldPerPopRate(YIELD_FOOD) * getPopulation()` (per-pop food yield).

### 1.3 Food consumption: `foodConsumption()` (CvCity.cpp:5912)

```cpp
return getFoodConsumedByPopulation(iExtra)
     - (bNoAngry ? angryPopulation(iExtra) : 0)
     - healthRate(bNoAngry, iExtra)
     + (bIncludeWastage ? (int)foodWastage() : 0);
```

**`getFoodConsumedByPopulation()`** (CvCity.cpp:5907):
- Uses `getPopulationPlusProgress100()` (CvCity.cpp:5885): `100 * pop + 100 * food / growthThreshold`
  — a fractional population that scales consumption *smoothly* as the food bar fills, so a pop-1 city
  eating toward size-2 consumes a fractionally larger amount each turn (the Toffer gradual-growth mod).
- `FOOD_CONSUMPTION_PER_POPULATION` = 4 (GlobalDefines.xml:426).
- `FOOD_CONSUMPTION_PER_POPULATION_PERCENT` = 20 (GlobalDefines.xml:430): the per-fractional-pop
  scale factor.

**`healthRate()`** (CvCity.cpp:5876): `min(0, goodHealth - badHealth)` — health *reduces* consumption
(healthy cities eat less effectively; unhealthy cities eat more). Aggregates many sources: espionage,
features, bonuses, buildings, civics, player effects, corporations, etc.

**`angryPopulation()`**: reduces consumption further if `bNoAngry=true` (only used in `isFoodProduction`
context — angry pop does not consume food in the AI's food-production mode).

**The wastage term** (`bIncludeWastage=true`): only included when computing `foodDifference` with
wastage. The *default* `foodConsumption()` call in `changeFood` (via `foodDifference()` at CvCity.cpp:1329)
does NOT include wastage — see §1.5 below.

### 1.4 Net food per turn: `foodDifference()` (CvCity.cpp:5970)

```cpp
int CvCity::foodDifference(const bool bBottom, const bool bIncludeWastage,
                            const bool bIgnoreFoodBuildOrRev) const
{
    if (!bIgnoreFoodBuildOrRev && isDisorder())  return 0;  // disorder = no net change
    if (!bIgnoreFoodBuildOrRev && isFoodProduction())
        iDifference = min(0, getYieldRate(YIELD_FOOD) - foodConsumption(false, 0, bIncludeWastage));
    else
        iDifference = getYieldRate(YIELD_FOOD) - foodConsumption(false, 0, bIncludeWastage);
    if (bBottom && pop == 1 && food == 0)
        iDifference = max(0, iDifference);  // pop-1 floor
    return iDifference;
}
```

Important variants:
- `foodDifference()` — canonical per-turn tick, NO wastage, no floor special-cases except pop-1 edge.
- `foodDifference(false, true)` — includes wastage (used in wastage computation itself).
- `foodDifference(true, true)` — bBottom + wastage (used by AI growth checks at CvCityAI.cpp:4240).
- `foodDifference(false, false, true)` — ignores food-production and disorder status (AI baseline
  at CvCityAI.cpp:1324).
- `isDisorder()` → food-locked cities produce `0` net food regardless of yield/consumption.
- `isFoodProduction()` → when training a unit with `isFoodProduction()==true`, the upward food
  path is capped at 0 (surplus is redirected to production hammers); only starvation bleeds through.

### 1.5 Wastage: `foodWastage()` (CvCity.cpp:5923)

C2C-added mechanic (Thunderbrd/Sorcdk, 2019). Activated when:
- `WASTAGE_START_CONSUMPTION_PERCENT >= 0` (currently 50, A_New_Dawn_GlobalDefines.xml:260–261).
- The city's food surplus (after subtracting `consumption_percent * consumption / 100`) is positive.

Algorithm (recursive, memoised with a static `calculatedWaste[200]` array):
```
surplass = foodDifference(true, false) - getFoodConsumedByPopulation() * 50 / 100

if surplass <= 0: wastage = 0
else:
  waste[N] = waste[N-1] + 1.0 - (0.05 + 0.95 / (1 + 0.05 * N))
```
`WASTAGE_GROWTH_FACTOR` = 0.05 (A_New_Dawn_GlobalDefines.xml:264–265).

Wastage is an *integer* (truncated float) added to `foodConsumption()` when `bIncludeWastage=true`.
The practical effect: high-surplus cities have a fraction of their surplus silently consumed
by "waste", suppressing growth. The curve is asymptotic — wastage grows ~logarithmically with
surplus. The static memo cache is global (not per-city), which means at large surplus values
the cache is reused across all cities computing wastage on the same call-stack — this is a
subtle implementation detail that could produce incorrect results if called from multiple
threads (currently not an issue; all calls are game-thread serial).

### 1.6 Food storage and granary: `changeFood` / `m_iFoodKept` (CvCity.cpp:9713)

`m_iFood`: raw food-bar counter (0 to `growthThreshold()`). Serialized as a named tag
(`WRAPPER_READ/WRITE "CvCity" m_iFood`).

On `changeFood(delta, true)`:
- **Granary update**: if `delta > 0`, adds `max(1, delta * getFoodKeptPercent() / 100)` to
  `m_iFoodKept` (granary fill); if `delta < 0`, removes `min(-1, delta / 2)` (granary bleeds
  at half the starvation rate — hardcoded comment: "hardcoded rate for now").
- **Starvation**: if `m_iFood < 0` → remove population until `m_iFood >= 0` or pop is 1,
  adding `growthThreshold()` per pop lost.
- **Growth**: if `m_iFood >= growthThreshold()` → loop: subtract `growthThreshold()`, if the
  post-growth food is below `m_iFoodKept` pull food from the granary to fill the gap
  (`m_iFood += diff; m_iFoodKept -= diff`), then `changePopulation(+1)`, recalculate threshold.
  Respect `AI_avoidGrowth()` / `AI_isEmphasizeAvoidGrowth()` (caps `m_iFood` at threshold without
  growing).
- Granary cap: `m_iFoodKept` is clamped to `growthThreshold() * getFoodKeptPercent() / 100`
  (CvCity.cpp:9782).

**`getFoodKeptPercent()`** (CvCity.cpp:9787): the fraction of the food threshold stored in the
granary on growth. Driven by building `kBuilding.getFoodKept()` accumulated into
`m_iFoodKeptPercent` (CvCity.cpp:4621). Clamped 0–99 (CvCity.cpp:9789).

**`growthThreshold()`** (CvCity.cpp:5993): `getModifiedIntValue(player.getGrowthThreshold(pop),
cityGrowthRatePercent + playerGrowthRatePercent)`. Halved for hominid/barbarian cities (`isHominid()`).

### 1.7 Plot-level yield: `CvPlot::calculateYield` (CvPlot.cpp:8285)

Each worked tile contributes its `calculateYield(YIELD_FOOD)` to `m_aiBaseYieldRate[YIELD_FOOD]`
(CvCity's plot-yield sum, updated via `changePlotYield`). Per-plot yield adds:
- `calculateNatureYield` (terrain/feature/bonus base).
- `m_aExtraYield[YIELD_FOOD]` (plot extra yield, e.g. from events).
- City tile bonus: `GC.getYieldInfo(YIELD_FOOD).getCityChange()` + pop divisor.
- Player terrain yield change, sea plot yield if water.
- Working city `getYieldChangeAt` (building/improvement-yield changes at that plot).
- Landmark yield (if `GAMEOPTION_MAP_PERSONALIZED`).
- Extra-yield threshold bonuses (CvPlayer).
- Golden-age threshold bonus.
- Improvement yield change (`calculateImprovementYieldChange`) if not the city tile itself.
- Route yield change if routed.
- `getExtraYieldThreshold` / `getLessYieldThreshold` civic-driven thresholds.

---

## 2. Current observability

### 2.1 Observability tier: **1 — Telescreen**

The only food data available from outside today is the gross yield rate from the `/cities`
snapshot.

### 2.2 What IS observable today

| Source | Field | What it is | How to get it |
|---|---|---|---|
| `GET /cities` | `food` | `getYieldRate(YIELD_FOOD)` — gross food output (CvHttpServer.cpp:1550, rendered at CvCity.cpp:11231) | snapshot field, ≤5s stale |
| `GET /cities` | `population` | current population (not food-related directly, but needed to reconstruct consumption) | snapshot field |
| `GET /players` | `production` | `getYieldRate(YIELD_PRODUCTION)` — useful to cross-check food-to-hammers redirect | snapshot field |
| `[CIT/proplevel]` (level 1) | `val`, `change` | per-city property (crime/disease/education) values per turn — **not food**, but disease is a bad-health source that increases `healthRate` and therefore `foodConsumption` | CityAI.log + `/events` |
| `[PERF/cabv]` (level 1) | `food=…ms` | **wall-clock ms** of the food-scoring dimension in `CalculateAllBuildingValues`, not a food quantity | Performance.log + `/events` |
| `[PERF/phase]` (level 1) | `city.doTurn` phase timing | total city-turn time — not food-specific | Performance.log |

### 2.3 What is NOT observable today

Every field below is computed live but not emitted to any endpoint or log line:

| State | Function | Significance |
|---|---|---|
| Net food surplus/deficit | `foodDifference()` (CvCity.cpp:5970) | The signed delta actually applied to the food bar each turn; the primary "is this city growing or starving?" signal |
| Food consumption total | `foodConsumption()` (CvCity.cpp:5912) | How much the city eats; needed to decompose gross yield into net |
| Wastage amount | `foodWastage()` (CvCity.cpp:5923) | The fraction of surplus silently destroyed; opaque by design — not exposed anywhere |
| Food stored | `getFood()` / `m_iFood` (CvCity.cpp:9693) | Current position in the food bar (0 to threshold); how many turns until growth/starvation |
| Growth threshold | `growthThreshold()` (CvCity.cpp:5993) | The food bar ceiling; needed to interpret stored food as a fraction |
| Granary fill | `getFoodKept()` / `m_iFoodKept` (CvCity.h:672) | How much food is guaranteed-carry-forward on the next growth; not exposed |
| Granary percent | `getFoodKeptPercent()` (CvCity.cpp:9787) | The fraction stored by buildings with `getFoodKept()`; needed to explain `m_iFoodKept` |
| Health rate | `healthRate()` (CvCity.cpp:5876) | The health term that modifies consumption; positive health reduces consumption |
| Good / bad health subtotals | `goodHealth()` / `badHealth()` | Decomposition of healthRate; multiple building/civic/bonus sources |
| isFoodProduction flag | `isFoodProduction()` (CvCity.cpp:3479) | Whether food surplus is being redirected to hammers this turn |
| isDisorder flag | `isDisorder()` | Whether the city is food-locked to 0 net |
| Turns to growth/starvation | `getFoodTurnsLeft()` (CvCity.cpp:3035) | Derived from net food and stored food; the "ETA" displayed in the UI |

---

## 3. The gap

An external observer seeing only the API today can determine:
- How much gross food a city produces (`/cities.food`).
- How many people live there (`/cities.population`).

But cannot determine:
1. **Net food flow**: `foodDifference()` is not emitted. Without it the observer cannot tell if the city
   is growing, stagnating, or starving — the key yield question.
2. **Wastage**: `foodWastage()` is computed but never logged or emitted. A city with a large food surplus
   and active wastage silently eats part of that surplus; the observer sees only the gross yield and cannot
   reconstruct the actual food accumulation rate.
3. **Consumption breakdown**: `foodConsumption()` is not emitted. The health term, angry-pop adjustment,
   and gradual-growth fractional consumption are all invisible. An observer cannot decompose gross yield
   into (base consumption + health adjustment + wastage = effective consumption).
4. **Food bar position and threshold**: without `m_iFood` and `growthThreshold()` the observer cannot
   know how many turns remain until the next growth or starvation event.
5. **Granary state**: `m_iFoodKept` and `m_iFoodKeptPercent` are invisible — the observer cannot predict
   the post-growth food carry-forward that affects the next growth ETA.
6. **Disorder / food-production modes**: no endpoint signals these active overrides that change what
   `foodDifference()` actually returns.
7. **Plot-level food breakdown**: which tiles contribute which food amounts. The city aggregate is exposed
   but not the per-tile contribution that drives citizen assignment AI decisions.

**For the Orwell bar**: an AI city's food balance is completely opaque today. A city could be 1 turn from
starvation or growing at +8/turn — the endpoint gives identical snapshots (just the gross yield and
population). Per the §D metric, **the food/yields/wastage system is Tier 1 (Telescreen)** — coarse
snapshot only, no *why*.

---

## 4. Proposed hooks (concrete additions)

Each proposed hook follows the existing patterns: gated by `gCityLogLevel`, single-line `key=value`,
feeds `/events` via `streamLogTee`, publishable via a `/cities` snapshot field extension.

### 4.1 Extend `/cities` snapshot with food internals (CvHttpServer.cpp)

Add to `CitySnap` struct and `renderCities`:

| JSON field | C++ source | Notes |
|---|---|---|
| `foodNet` | `foodDifference()` | Signed net food per turn; the primary food-state signal |
| `foodConsume` | `foodConsumption()` | Total effective consumption (includes health adjustment; excludes wastage) |
| `foodWaste` | `(int)foodWastage()` | Wastage amount this turn (0 when below threshold) |
| `foodStored` | `getFood()` | Current food-bar position |
| `foodThreshold` | `growthThreshold()` | Growth threshold (food bar ceiling) |
| `foodKept` | `getFoodKept()` | Granary fill (carry-forward on growth) |
| `foodKeptPct` | `getFoodKeptPercent()` | Granary percent (0–99) |
| `healthRate` | `healthRate()` | Health modifier on consumption (negative = unhealthy = more food eaten) |
| `isFoodProd` | `isFoodProduction() ? 1 : 0` | 1 when surplus is redirected to hammers |
| `isDisorder` | `isDisorder() ? 1 : 0` | 1 when food is locked to 0 net |

These are cheap reads (no allocation, no iteration) — cost is negligible at the 5s publish rate.
All game-thread-safe under the snapshot contract.

### 4.2 Per-turn city food log tag: `[CIT/food]`

Add to `CvCity::doTurn` after `changeFood(foodDifference(), true)` at CvCity.cpp:1329:

```cpp
if (gCityLogLevel >= 1)
{
    logCityAI(1, "[CIT/food] turn=%d city=%S owner=%d pop=%d gross=%d consume=%d net=%d waste=%d stored=%d threshold=%d kept=%d health=%d foodProd=%d disorder=%d",
        GC.getGame().getGameTurn(), getName().GetCString(), (int)getOwner(),
        getPopulation(),
        getYieldRate(YIELD_FOOD),    // gross output
        foodConsumption(),           // effective consumption (no wastage)
        foodDifference(),            // net (what was actually applied)
        (int)foodWastage(),          // wastage burned this turn
        getFood(),                   // food bar after the tick
        growthThreshold(),           // ceiling
        getFoodKept(),               // granary
        healthRate(),                // health term
        isFoodProduction() ? 1 : 0, // food-to-hammers redirect
        isDisorder() ? 1 : 0        // disorder lock
    );
}
```

Level-1 (headline) — one line per city per turn. Gives an external reader the full food balance sheet.

For level-2 detail, a `[CIT/food/waste]` breakout can log the wastage inputs (surplass computation) to
explain why `waste > 0`.

### 4.3 Growth and starvation events (level-1 landmarks)

In `changeFood` (CvCity.cpp:9713), instrument the growth and starvation branches:

```cpp
// growth event (level 1):
logCityAI(1, "[CIT/food/grow] turn=%d city=%S owner=%d pop=%d -> %d stored=%d kept=%d",
    turn, name, owner, oldPop, newPop, m_iFood, m_iFoodKept);

// starvation event (level 1):
logCityAI(1, "[CIT/food/starve] turn=%d city=%S owner=%d pop=%d -> %d stored=%d",
    turn, name, owner, oldPop, newPop, m_iFood);
```

These are low-frequency, high-signal events. Without them an observer watching the `/cities` snapshot
can only notice a population change *after* it happened (the 5s snapshot lag may miss it entirely if
growth completes between snapshots).

### 4.4 `/events` SSE events for growth/starvation

In `changeFood`, emit turn events (via `publishEvent`) for growth and starvation:

```
event: cityGrow   data: {"city":N,"owner":N,"pop":N,"x":N,"y":N}
event: cityStarve data: {"city":N,"owner":N,"pop":N,"x":N,"y":N}
```

These are immediate (not snapshot-delayed) and let a watcher detect population changes in the
turn they happen — closing the 5s snapshot window for population event timing.

### 4.5 `/cities` wastage-details endpoint (or `/diagnostic/food?city=N&player=N`)

A `/diagnostic/food?player=N` endpoint evaluating the full food balance for a player's cities
on demand (same mailbox model as `canConstruct`):

```json
{
  "player": 1,
  "cities": [
    { "id": 42, "name": "London", "gross": 18, "consume": 12, "net": 4, "waste": 2,
      "stored": 45, "threshold": 60, "kept": 18, "keptPct": 33,
      "healthRate": -1, "isFoodProd": 0, "isDisorder": 0,
      "turnsToGrowth": 4 }
  ]
}
```

This provides a complete per-city food balance sheet for all AI players in one call — the
"screen-free" food state read for the Orwell bar.

---

## 5. Tier assessment

| Tier | Name | Food system status |
|---|---|---|
| 0 | Oblivious | (below current) |
| **1** | **Telescreen** | **Current state.** Only gross yield (`/cities.food`) and population. Surplus, consumption, wastage, stored food, threshold, granary: all invisible. |
| 2 | Informant | + `/diagnostic/food` snapshot per player: full balance sheet on demand. |
| 3 | Big Brother | + `[CIT/food]` per-turn log line + `[CIT/food/grow]` / `[CIT/food/starve]` events in `/events` stream. |
| 4 | Thought Police | + `/cities` snapshot extended with `foodNet`/`foodStored`/`foodThreshold`/`foodWaste` etc. (§4.1) — the full balance is reconstructible from the polling snapshot for all players, all cities. |
| 5 | Thought Police | + level-2 `[CIT/food/waste]` breakout logging the wastage-curve inputs; + plot-level food contribution logging at `gCityLogLevel >= 3`. |

**To climb from 1 → 4 (what the Orwell bar requires):** implement §4.1 (extend `/cities` snapshot)
and §4.3–4.4 (`[CIT/food/grow]` / `[CIT/food/starve]` log/event pair). The §4.2 `[CIT/food]`
headline log is then the per-turn audit trail. These are all cheap, gated additions with no
game-logic side effects.

---

## 6. Code cross-reference

| Claim | Source |
|---|---|
| Per-turn food tick site | `CvCity::doTurn` CvCity.cpp:1329 |
| `foodDifference()` | CvCity.cpp:5970–5989 |
| `foodConsumption()` | CvCity.cpp:5912–5918 |
| `foodWastage()` with memoization + static cache | CvCity.cpp:5923–5967 |
| `getYieldRate100(YIELD_FOOD)` formula | CvCity.cpp:11236–11244 |
| `getBaseYieldRate(YIELD_FOOD)` | CvCity.cpp:22814–22823 |
| `getBaseYieldRateModifier(YIELD_FOOD)` | CvCity.cpp:11207–11229 |
| `getExtraYield100(YIELD_FOOD)` | CvCity.cpp:11313–11323 |
| `changeFood` — growth/starvation + granary logic | CvCity.cpp:9713–9774 |
| `changeFoodKept` — granary clamping | CvCity.cpp:9778–9784 |
| `getFoodKeptPercent` / building `getFoodKept` accumulation | CvCity.cpp:9787–9795, 4621 |
| `growthThreshold` | CvCity.cpp:5993–6006 |
| `getPopulationPlusProgress100` (gradual food consumption) | CvCity.cpp:5885–5891 |
| `getFoodConsumedByPopulation` | CvCity.cpp:5907–5909 |
| `FOOD_CONSUMPTION_PER_POPULATION` = 4 | Assets/XML/GlobalDefines.xml:425–426 |
| `FOOD_CONSUMPTION_PER_POPULATION_PERCENT` = 20 | Assets/XML/GlobalDefines.xml:429–430 |
| `WASTAGE_START_CONSUMPTION_PERCENT` = 50 | Assets/XML/A_New_Dawn_GlobalDefines.xml:260–261 |
| `WASTAGE_GROWTH_FACTOR` = 0.05 | Assets/XML/A_New_Dawn_GlobalDefines.xml:264–265 |
| `/cities` snapshot publish site | CvHttpServer.cpp:1550–1552 |
| `/cities.food` field render | CvHttpServer.cpp:346 |
| `isFoodProduction` — surplus-to-hammers redirect | CvCity.cpp:3479–3490, 5978–5980 |
| `CvPlot::calculateYield(YIELD_FOOD)` | CvPlot.cpp:8285–8403 |
| `[CIT/proplevel]` (only existing city-level log) | CvCity.cpp:1244 |
| `[PERF/cabv]` food dimension (timing only) | CvCityAI.cpp:14127–14133, 14185–14186 |
