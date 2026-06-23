# Observability — Food, yields & wastage — what's on the wire for a city's food balance

> **Status:** reference   ·   **Verified against:** 2026-06-20 (function sites confirmed in `Sources/Engine/CvCity.cpp`; line numbers drift)
> **Grounding:** live `Sources/Engine/CvCity.cpp` (food tick / yield / wastage / granary), `Sources/Engine/CvPlot.cpp` (plot yield), `Sources/Tools/CvHttpServer.cpp` (`/cities`); global-define values from `Assets/XML/`. Carried over from the old draft map; function sites re-confirmed, paths re-grounded to the reorganized `Sources/` tree.
> A city's food balance is **Tier 1 (Telescreen)** today: only gross food output (`/cities.food`) and population are on the wire. Net food, consumption, wastage, stored food, threshold and granary are all computed live but emitted nowhere — an external observer cannot tell a city 1 turn from starvation apart from one growing at +8/turn. This map walks the per-turn food mechanics, names exactly what is and isn't observable, and proposes the snapshot fields / log tags / endpoint that climb it to Tier 4.

The observability scale (0–5) and the three canonical hook shapes are defined once in
[`README.md`](README.md) ([DEC-obs-scale], [DEC-obs-hook-shapes]); the live surface and the rules for
reading it (logs held open mid-session, use `/events` + `/diagnostic`) live in
[`http-server.md`](http-server.md). This doc does not restate them.

> Line numbers below are anchors at time of writing and **drift** — confirm the named function, not the
> integer. (The old draft's numbers were already ~10 lines stale when re-confirmed; the functions are stable.)

---

## 1. How it actually works — per-turn mechanics

### 1.1 The per-turn food tick (`CvCity::doTurn`, CvCity.cpp:1329)

Every `CvCity::doTurn` calls:

```cpp
changeFood(foodDifference(), true);   // Sources/Engine/CvCity.cpp:1329
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

### 1.3 Food consumption: `foodConsumption()` (CvCity.cpp:5922)

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
- `FOOD_CONSUMPTION_PER_POPULATION` = 4 (GlobalDefines.xml).
- `FOOD_CONSUMPTION_PER_POPULATION_PERCENT` = 20 (GlobalDefines.xml): the per-fractional-pop scale factor.

**`healthRate()`** (CvCity.cpp:5886): `min(0, goodHealth - badHealth)` — health *reduces* consumption
(healthy cities eat less effectively; unhealthy cities eat more). Aggregates many sources: espionage,
features, bonuses, buildings, civics, player effects, corporations, etc. Decomposition of the
health ledger is mapped in [`health-happiness.md`](health-happiness.md).

**`angryPopulation()`**: reduces consumption further if `bNoAngry=true` (only used in `isFoodProduction`
context — angry pop does not consume food in the AI's food-production mode).

**The wastage term** (`bIncludeWastage=true`): only included when computing `foodDifference` with
wastage. The *default* `foodConsumption()` call in `changeFood` (via `foodDifference()` at CvCity.cpp:1329)
does NOT include wastage — see §1.5.

### 1.4 Net food per turn: `foodDifference()` (CvCity.cpp:5980)

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

### 1.5 Wastage: `foodWastage()` (CvCity.cpp:5933)

C2C-added mechanic (Thunderbrd/Sorcdk, 2019). Activated when:
- `WASTAGE_START_CONSUMPTION_PERCENT >= 0` (currently 50, A_New_Dawn_GlobalDefines.xml).
- The city's food surplus (after subtracting `consumption_percent * consumption / 100`) is positive.

Algorithm (recursive, memoised with a static `calculatedWaste[200]` array):
```
surplass = foodDifference(true, false) - getFoodConsumedByPopulation() * 50 / 100

if surplass <= 0: wastage = 0
else:
  waste[N] = waste[N-1] + 1.0 - (0.05 + 0.95 / (1 + 0.05 * N))
```
`WASTAGE_GROWTH_FACTOR` = 0.05 (A_New_Dawn_GlobalDefines.xml).

Wastage is an *integer* (truncated float) added to `foodConsumption()` when `bIncludeWastage=true`.
The practical effect: high-surplus cities have a fraction of their surplus silently consumed
by "waste", suppressing growth. The curve is asymptotic — wastage grows ~logarithmically with
surplus. The static memo cache is global (not per-city); all calls are game-thread serial today, so
the shared cache is currently safe.

### 1.6 Food storage and granary: `changeFood` / `m_iFoodKept` (CvCity.cpp:9723)

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
  (CvCity.cpp:9788).

**`getFoodKeptPercent()`** (CvCity.cpp:9797): the fraction of the food threshold stored in the
granary on growth. Driven by building `kBuilding.getFoodKept()` accumulated into
`m_iFoodKeptPercent` (CvCity.cpp:4621). Clamped 0–99.

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

## 2. What's on the wire today — **Tier 1 (Telescreen)**

The only food data available from outside today is the gross yield rate from the `/cities` snapshot.

### 2.1 What IS observable

| Source | Field | What it is | How to get it |
|---|---|---|---|
| `GET /cities` | `food` | `getYieldRate(YIELD_FOOD)` — gross food output (rendered at CvCity.cpp:11231) | snapshot field, ≤5s stale |
| `GET /cities` | `population` | current population (needed to reconstruct consumption) | snapshot field |
| `GET /players` | `production` | `getYieldRate(YIELD_PRODUCTION)` — useful to cross-check food-to-hammers redirect | snapshot field |
| `[CIT/proplevel]` (level 1) | `val`, `change` | per-city property (crime/disease/education) values per turn — **not food**, but disease is a bad-health source that increases `healthRate` and therefore `foodConsumption` | CityAI.log + `/events` |
| `[PERF/cabv]` (level 1) | `food=…ms` | **wall-clock ms** of the food-scoring dimension in `CalculateAllBuildingValues`, not a food quantity | Performance.log + `/events` |
| `[PERF/phase]` (level 1) | `city.doTurn` phase timing | total city-turn time — not food-specific | Performance.log |

### 2.2 What is NOT observable

Every field below is computed live but not emitted to any endpoint or log line:

| State | Function | Significance |
|---|---|---|
| Net food surplus/deficit | `foodDifference()` (CvCity.cpp:5980) | The signed delta actually applied to the food bar each turn; the primary "is this city growing or starving?" signal |
| Food consumption total | `foodConsumption()` (CvCity.cpp:5922) | How much the city eats; needed to decompose gross yield into net |
| Wastage amount | `foodWastage()` (CvCity.cpp:5933) | The fraction of surplus silently destroyed; opaque by design |
| Food stored | `getFood()` / `m_iFood` (CvCity.cpp:9693) | Current position in the food bar (0 to threshold); turns until growth/starvation |
| Growth threshold | `growthThreshold()` (CvCity.cpp:5993) | The food bar ceiling; needed to interpret stored food as a fraction |
| Granary fill | `getFoodKept()` / `m_iFoodKept` (CvCity.h:672) | Guaranteed carry-forward on the next growth; not exposed |
| Granary percent | `getFoodKeptPercent()` (CvCity.cpp:9797) | The fraction stored by buildings with `getFoodKept()`; needed to explain `m_iFoodKept` |
| Health rate | `healthRate()` (CvCity.cpp:5886) | The health term that modifies consumption; positive health reduces consumption |
| Good / bad health subtotals | `goodHealth()` / `badHealth()` | Decomposition of healthRate; multiple building/civic/bonus sources |
| isFoodProduction flag | `isFoodProduction()` (CvCity.cpp:3479) | Whether food surplus is being redirected to hammers this turn |
| isDisorder flag | `isDisorder()` | Whether the city is food-locked to 0 net |
| Turns to growth/starvation | `getFoodTurnsLeft()` (CvCity.cpp:3035) | Derived from net food and stored food; the "ETA" displayed in the UI |

---

## 3. The gap

An external observer seeing only the API today can determine gross food output (`/cities.food`) and
population (`/cities.population`). It **cannot** determine:

1. **Net food flow**: `foodDifference()` is not emitted. Without it the observer cannot tell if the city
   is growing, stagnating, or starving — the key yield question.
2. **Wastage**: `foodWastage()` is computed but never logged or emitted. A high-surplus city silently
   eats part of its surplus; the observer sees only gross yield and cannot reconstruct the actual food
   accumulation rate.
3. **Consumption breakdown**: `foodConsumption()` is not emitted. The health term, angry-pop adjustment
   and gradual-growth fractional consumption are all invisible.
4. **Food bar position and threshold**: without `m_iFood` and `growthThreshold()` the observer cannot
   know how many turns remain until the next growth or starvation event.
5. **Granary state**: `m_iFoodKept` and `m_iFoodKeptPercent` are invisible — no way to predict the
   post-growth carry-forward that affects the next growth ETA.
6. **Disorder / food-production modes**: no endpoint signals these active overrides that change what
   `foodDifference()` actually returns.
7. **Plot-level food breakdown**: which tiles contribute which food amounts (drives citizen-assignment AI).

**Orwell-bar consequence:** an AI city's food balance is completely opaque today — a city 1 turn from
starvation and one growing at +8/turn give identical snapshots (gross yield + population). Tier 1
(Telescreen): coarse snapshot only, no *why*.

---

## 4. Proposed hooks (concrete additions)

All hooks follow the three canonical hook shapes — see [DEC-obs-hook-shapes].

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

These are cheap reads (no allocation, no iteration) — negligible at the 5s publish rate, and
game-thread-safe under the snapshot contract.

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

Level-1 (headline) — one line per city per turn; the full food balance sheet. A level-2
`[CIT/food/waste]` breakout can log the wastage inputs (surplass computation) to explain `waste > 0`.

### 4.3 Growth and starvation events (level-1 landmarks)

In `changeFood` (CvCity.cpp:9723), instrument the growth and starvation branches:

```cpp
// growth event (level 1):
logCityAI(1, "[CIT/food/grow] turn=%d city=%S owner=%d pop=%d -> %d stored=%d kept=%d",
    turn, name, owner, oldPop, newPop, m_iFood, m_iFoodKept);

// starvation event (level 1):
logCityAI(1, "[CIT/food/starve] turn=%d city=%S owner=%d pop=%d -> %d stored=%d",
    turn, name, owner, oldPop, newPop, m_iFood);
```

Low-frequency, high-signal. Without them an observer watching `/cities` can only notice a population
change *after* it happened (the 5s snapshot lag may miss it entirely if growth completes between snapshots).

### 4.4 `/events` SSE events for growth/starvation

In `changeFood`, emit turn events (via `publishEvent`) for growth and starvation:

```
event: cityGrow   data: {"city":N,"owner":N,"pop":N,"x":N,"y":N}
event: cityStarve data: {"city":N,"owner":N,"pop":N,"x":N,"y":N}
```

Immediate (not snapshot-delayed) — lets a watcher detect population changes in the turn they happen,
closing the 5s window.

### 4.5 `/diagnostic/food?player=N` endpoint

A mailbox endpoint (same model as `canConstruct`) evaluating the full food balance for a player's cities
on demand:

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

A complete per-city food balance sheet for all AI players in one call — the screen-free food read for
the Orwell bar.

---

## 5. Tier assessment

| Tier | Name | Food system status |
|---|---|---|
| 0 | Oblivious | (below current) |
| **1** | **Telescreen** | **Current state.** Only gross yield (`/cities.food`) + population. Surplus, consumption, wastage, stored food, threshold, granary: all invisible. |
| 2 | Informant | + `/diagnostic/food` snapshot per player (§4.5): full balance sheet on demand. |
| 3 | Big Brother | + `[CIT/food]` per-turn log line + `[CIT/food/grow]` / `[CIT/food/starve]` events in `/events` (§4.2–4.4). |
| 4 | Thought Police | + `/cities` snapshot extended with `foodNet`/`foodStored`/`foodThreshold`/`foodWaste` etc. (§4.1) — full balance reconstructible from the polling snapshot for all players, all cities. |
| 5 | Meta | + level-2 `[CIT/food/waste]` wastage-curve inputs; + plot-level food contribution logging at `gCityLogLevel >= 3`. |

**To climb 1 → 4 (the Orwell bar):** implement §4.1 (extend `/cities`) and §4.3–4.4
(`[CIT/food/grow]` / `[CIT/food/starve]` log/event pair). §4.2 `[CIT/food]` is the per-turn audit trail.
All cheap, gated, off by default — no game-logic side effects.

---

## 6. Code cross-reference

> Paths re-grounded to the reorganized `Sources/` tree (`Cv*` engine classes → `Sources/Engine/`,
> `Cv*AI` → `Sources/AI/`, `CvHttpServer` → `Sources/Tools/`). Line numbers drift — confirm the function.

| Claim | Source |
|---|---|
| Per-turn food tick site | `CvCity::doTurn` — `Sources/Engine/CvCity.cpp:1329` |
| `foodDifference()` | `Sources/Engine/CvCity.cpp:5980` |
| `foodConsumption()` | `Sources/Engine/CvCity.cpp:5922` |
| `foodWastage()` (memoised, static cache) | `Sources/Engine/CvCity.cpp:5933` |
| `getYieldRate100(YIELD_FOOD)` formula | `Sources/Engine/CvCity.cpp:11236` |
| `getBaseYieldRate(YIELD_FOOD)` | `Sources/Engine/CvCity.cpp:22814` |
| `getBaseYieldRateModifier(YIELD_FOOD)` | `Sources/Engine/CvCity.cpp:11207` |
| `getExtraYield100(YIELD_FOOD)` | `Sources/Engine/CvCity.cpp:11313` |
| `changeFood` — growth/starvation + granary | `Sources/Engine/CvCity.cpp:9723` |
| `changeFoodKept` — granary clamping | `Sources/Engine/CvCity.cpp:9788` |
| `getFoodKeptPercent` / building `getFoodKept` accumulation | `Sources/Engine/CvCity.cpp:9797`, `:4621` |
| `growthThreshold` | `Sources/Engine/CvCity.cpp:5993` |
| `getPopulationPlusProgress100` (gradual consumption) | `Sources/Engine/CvCity.cpp:5885` |
| `getFoodConsumedByPopulation` | `Sources/Engine/CvCity.cpp:5907` |
| `healthRate` | `Sources/Engine/CvCity.cpp:5886` |
| `FOOD_CONSUMPTION_PER_POPULATION` = 4 | `Assets/XML/GlobalDefines.xml` |
| `FOOD_CONSUMPTION_PER_POPULATION_PERCENT` = 20 | `Assets/XML/GlobalDefines.xml` |
| `WASTAGE_START_CONSUMPTION_PERCENT` = 50 | `Assets/XML/A_New_Dawn_GlobalDefines.xml` |
| `WASTAGE_GROWTH_FACTOR` = 0.05 | `Assets/XML/A_New_Dawn_GlobalDefines.xml` |
| `/cities` snapshot publish site | `Sources/Tools/CvHttpServer.cpp` (`publishIfDue` city walk) |
| `/cities.food` field render | `Sources/Tools/CvHttpServer.cpp` (`renderCities`) |
| `isFoodProduction` — surplus-to-hammers redirect | `Sources/Engine/CvCity.cpp:3479` |
| `CvPlot::calculateYield(YIELD_FOOD)` | `Sources/Engine/CvPlot.cpp:8285` |
| `[CIT/proplevel]` (only existing city-level log) | `Sources/Engine/CvCity.cpp:1244` |
| `[PERF/cabv]` food dimension (timing only) | `Sources/AI/CvCityAI.cpp:14127`, `:14185` |

---

## See also
- [`README.md`](README.md) — the observability scaffold: the 0–5 scale ([DEC-obs-scale]), the Orwell bar,
  and the three canonical hook shapes ([DEC-obs-hook-shapes]) this map's hooks instantiate.
- [`http-server.md`](http-server.md) — the live surface (`/cities`, `/events`, `/diagnostic`) these hooks
  extend, and the live-read rules (logs held open mid-session).
- [`health-happiness.md`](health-happiness.md) — the health ledger (`goodHealth`/`badHealth`/`healthRate`)
  that feeds `foodConsumption()` here; sick cities eat more food and starve faster.
- [`../../explanation/cascade-architecture.md`](../../explanation/cascade-architecture.md) — why total
  observability is load-bearing for map-before-delete.
- [`../../README.md`](../../README.md) — the comprehension map / overview-of-overviews.

[DEC-obs-scale]: ../../architecture/decisions.md#dec-obs-scale
[DEC-obs-hook-shapes]: ../../architecture/decisions.md#dec-obs-hook-shapes
