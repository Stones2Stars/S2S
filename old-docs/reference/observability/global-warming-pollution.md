> DRAFT observability map (2026-06-18 by parent agent) — claims cited from code; verify before relying.

# Observability map: Global Warming & Pollution

**System scope:** two semi-distinct systems share this map.
- **Pollution** (`PROPERTY_AIR_POLLUTION`, `PROPERTY_WATER_POLLUTION`) — live, property-system-driven accrual, per-city and per-plot, driving ~24 effect buildings.
- **Global Warming / Nuclear Winter** (`CvGame::doGlobalWarming`) — compiled out; the mechanic does not run.

---

## 1. How it actually works

### 1a. Global Warming / Nuclear Winter — permanently compiled out

The entire mechanic is gated on `#define GLOBAL_WARMING` at `Sources/CvGameCoreDLL.h:232`, which is commented out:

```cpp
// #define GLOBAL_WARMING          // CvGameCoreDLL.h:232
```

Consequences:
- `CvGame::doGlobalWarming()` (`CvGame.cpp:6581–6844`) is **never compiled** and never runs.
- The per-turn call site (`CvGame.cpp:5984–5986`) is `#ifdef`-guarded and likewise dead.
- The mechanic DID read `CvFeatureInfo::getWarmingDefense()` (tree-hugger defence) and `CvGame::getNukesExploded()` (nuke-weight spike); both accessors still compile but drive nothing.

**Vestiges that are live (compiled but do nothing):**
- `CvFeatureInfo::m_iWarmingDefense` / `getWarmingDefense()` (`CvFeatureInfo.h/.cpp`) — readable, but no consumer.
- Python binding `getWarmingDefense` (`CyInfoInterface2.cpp:249`) — exposes the inert getter.
- Pedia display (`Assets/Python/Screens/Pedia/PediaFeature.py:156`) — renders a zero-effect stat to players.
- `GlobalDefines.xml`: `GLOBAL_WARMING_UNHEALTH_WEIGHT`, `_BONUS_WEIGHT`, `_FOREST`, `_PROB`, `_NUKE_WEIGHT`, `GW_MOD_ENABLED`.
- `CvGame::getNukesExploded()` (`CvGame.cpp:3858`) — serialized (`CvGame.cpp:8504/8808`), incremented on nuke use, Python-exposed (`CyGame.cpp:420`, `CyGameInterface.cpp:115`), but only consumed inside the dead `#ifdef GLOBAL_WARMING` block.

See `docs/dev/plans/global-warming-mod.md` for the full vestige inventory and owner-sanctioned removal plan (#436).

### 1b. Pollution — per-turn property system (LIVE)

Both `PROPERTY_AIR_POLLUTION` and `PROPERTY_WATER_POLLUTION` are **live** and follow the standard `CvPropertySolver` pipeline.

**Per-turn solve cycle** (`CvGame.cpp:5944` → `m_PropertySolver.doTurn()`):

1. `CvPropertySolver::resetPropertyChanges()` — clears all `change` rates on every game object (`CvPropertySolver.cpp:402–409`).
2. `gatherGlobalManipulators()` — loads every `CvPropertyInfo`'s manipulator set once from `GC.getPropertyInfo(i)` (`CvPropertySolver.cpp:411–418`). Both pollution types register:
   - City scope: `PROPERTYSOURCE_DECAY` at 6%/turn + `PROPERTYSOURCE_ATTRIBUTE_CONSTANT` of +1/population/turn.
   - City→adjacent-plots: `PROPERTYPROPAGATOR_DIFFUSE` at 5% of city value to nearby non-peak plots.
   - Plot→same-city: `PROPERTYPROPAGATOR_DIFFUSE` at 12% back to the city.
   - Plot→adjacent-plots: `PROPERTYPROPAGATOR_DIFFUSE` at 4% to neighbouring non-ocean plots.
   - Plot scope: `PROPERTYSOURCE_DECAY` at 6%/turn.
   - (All from `Assets/XML/GameInfo/CIV4PropertyInfos.xml:532–760`.)
3. `gatherAndSolve()` — propagators first, then interactions, then sources; each phase is predict→applyChanges→correct→applyChanges (`CvPropertySolver.cpp:421–444`).
4. The result: every city and surrounding plot has its air/water pollution value updated in `CvProperties` storage.

**Building sources:** many regular and special buildings contribute `PROPERTYSOURCE_CONSTANT` deltas to pollution on their host city. Examples: industrial/factory buildings add; environmental policy buildings (Carpool Ordinance adds -25 air pollution, `SpecialBuildings_CIV4BuildingInfos.xml:706–710`) reduce.

**Effect buildings (property-band auto-placement):** `CvCity::checkPropertyBuildings()` (`CvCity.cpp:1490–1518`) runs every turn (inside the city doTurn autobuild block, ~`CvCity.cpp:1455–1490`). For each pollution value band defined in `CIV4PropertyInfos.xml`'s `<PropertyBuildings>`, it adds or removes the corresponding effect building:

- Air pollution bands (12 buildings, `CIV4PropertyInfos.xml:761–822`):
  `BUILDING_POLLUTION_LIGHT_SMOG` (≥400), `BUILDING_POLLUTION_MODERATE_SMOG` (≥850), `BUILDING_POLLUTION_HEAVY_SMOG` (≥1300), `BUILDING_POLLUTION_MINOR_GLOBAL_WARMING` (≥700), `BUILDING_POLLUTION_MODERATE_GLOBAL_WARMING` (≥1200), `BUILDING_POLLUTION_MAJOR_GLOBAL_WARMING` (≥1750), `BUILDING_POLLUTION_MINOR_OZONE_DEPLETION` (≥550), ..., `BUILDING_POLLUTION_BLACKENED_SKIES` (≥1950).
- Water pollution bands (12 buildings, `CIV4PropertyInfos.xml:607–668`):
  `BUILDING_POLLUTION_MINOR_GROUNDWATER_POLLUTION` (≥450), ..., `BUILDING_POLLUTION_TOXIC_HYDROSPHERE` (≥1800).

These effect buildings are a subset of `checkPropertyBuildings`' `kind=2` targets — the property-band auto-placement maintainer (cascade-mapping-inventory §B-i).

**Targets:** `targetLevel` = 0 for both pollution types (`CIV4PropertyInfos.xml:527, 679`); the solver drives toward 0 via the 6%/turn decay source. Buildings adding pollution fight this decay; cities only accumulate if sources outpace it.

**Nuclear Winter secondary:** even with `GLOBAL_WARMING` compiled out, `getNukesExploded()` is still updated when nukes fire; it is stored in saves. The nuke counter does NOT affect pollution properties directly — the pollution system is entirely building-driven. The only connection was inside the dead `doGlobalWarming` function.

---

## 2. Current observability

**Overall tier: Tier 1 (Telescreen) with partial Tier 3 for pollution via file logs.**

### 2a. What IS observable today

| What | Where | Notes |
|---|---|---|
| City air/water pollution values + per-turn change | `[CIT/proplevel]` in `CityAI.log` (gated `gCityLogLevel >= 1`) at `CvCity.cpp:1244` | ALL active properties logged every turn-start per city, including both pollution types — once they are non-zero. Teed to `/events` at `gStreamLogLevel`. |
| City crime/education/disease (the live-gameplay triad) | `/cities` endpoint: `crime`, `education`, `disease` JSON fields | Explicitly in `CvHttpServer.cpp:99–101,354–356,1558–1563`. |
| Pollution effect buildings presence (indirect read) | `/diagnostic/placementSweep?player=N` + `[PLACEMENT]` per-turn | These 24 buildings are property-band maintainer targets; `kind=2` in the sweep. Presence of `BUILDING_POLLUTION_LIGHT_SMOG` etc. tells you the pollution level crossed a band. Currently `reason=noMarker` (JSON not yet autoBuild-flagged) — shadow flags the gap. |
| `nukesExploded` game counter | Python via `CyGame.getNukesExploded()` | Serialized; no HTTP endpoint. |

### 2b. What is NOT observable from outside

| Gap | Why |
|---|---|
| **City air/water pollution values** — not in `/cities` snapshot | `CvHttpServer.cpp:97–101` explicitly names crime/education/disease only; comment says "flammability and the pollutions are dormant." |
| **Plot-level pollution values** | Plots have `CvProperties` containers (the propagators write to `GAMEOBJECT_PLOT`) but no HTTP endpoint exposes plot properties. `PlotSnapshot` CSV (`docs/dev/reference/PlotSnapshot.md`) does not include any property columns. |
| **Per-turn net source/decay breakdown** | The solver runs silently — no logging from `CvPropertySolver.cpp` at any level. You can see the delta from two consecutive `[CIT/proplevel]` lines but not the component breakdown (building source vs population source vs decay vs plot propagation). |
| **Which buildings are contributing to pollution** | No log or endpoint lists a city's active `PROPERTYSOURCE_CONSTANT` contributors. You can only infer from the total. |
| **nukesExploded game counter** | Not in `/players` or any HTTP endpoint. Inside the compiled-out `doGlobalWarming` it drove nuclear winter; that path is dead. The value is effectively invisible from outside. |
| **Global warming trigger state** | There is no trigger state to expose — the entire mechanic is compiled out. |
| **Property solver doTurn timing** | `PERF_SCOPE("CvPropertySolver::doTurn", -1)` is wired (`CvPropertySolver.cpp:451`), so turn cost appears in `[PERF/phase]` as `phase=CvPropertySolver::doTurn` — this IS observable via Performance.log / `/events`. |

---

## 3. The gap

To meet the "render-from-API without looking at the screen" bar:

1. **City pollution values are invisible in the HTTP snapshot.** The `/cities` endpoint does not include `airPollution` or `waterPollution` fields. An agent cannot know the city's pollution level, its direction of change, or which effect-building bands are about to trigger.
2. **Plot-level pollution spread is entirely unobservable.** The property solver diffuses pollution to adjacent plots every turn. No endpoint or log surfaces per-plot property values. A significant pollution plume (e.g. around a dense industrial core) is completely dark.
3. **Property solver internals are silent.** The six-phase predict-correct cycle (`CvPropertySolver.cpp`) has no logging. There is no way to see which manipulators fired, which sources were active, or what the intermediate predict values were.
4. **Effect-building placement is partially observable** (via `placementSweep`, tier 3) but currently incomplete: pollution buildings report `reason=noMarker` in the shadow — their JSON lacks `autoBuild` flag and `requires.operate` bands — so the cascade cannot yet reproduce their auto-placement logic.
5. **nukesExploded is invisible.** Serialized game state with no HTTP surface; entirely irrelevant while `GLOBAL_WARMING` is compiled out, but if it is ever re-enabled the counter is the primary driver of nuclear winter intensity.

---

## 4. Proposed hooks (to climb from Tier 1 → Tier 3/4)

All hooks follow the three canonical observability hook shapes — see [DEC-obs-hook-shapes](../../decisions.md#dec-obs-hook-shapes).

### Hook A — Add `airPollution` / `waterPollution` to `/cities` snapshot

**File:** `CvHttpServer.cpp`

**Change:** in `CitySnap` struct (near line 99), add:

```cpp
int iAirPollution;
int iWaterPollution;
```

In `publishSnapshot` (near line 1561), populate them alongside crime/education/disease:

```cpp
const PropertyTypes eAirPol = GC.getPROPERTY_AIR_POLLUTION();
const PropertyTypes eWatPol = GC.getPROPERTY_WATER_POLLUTION();
city.iAirPollution  = eAirPol  > NO_PROPERTY ? pProps->getValueByProperty(eAirPol)  : 0;
city.iWaterPollution = eWatPol > NO_PROPERTY ? pProps->getValueByProperty(eWatPol) : 0;
```

In the JSON render (near line 354), emit:

```cpp
o["airPollution"]   = picojson::value((double)c.iAirPollution);
o["waterPollution"] = picojson::value((double)c.iWaterPollution);
```

**Rationale:** closes the single most important gap. Pollution is the only live gameplay property missing from `/cities`. Cost: two `getValueByProperty` lookups per city per 5s publish — negligible.

### Hook B — Add `nukesExploded` to `/players` or `/diagnostic`

**File:** `CvHttpServer.cpp`

**Change:** in `PlayerSnap` struct (line ~63), add `int iNukesExploded` on the game-level snap (or add a `/game` endpoint). Since this is a per-game counter (not per-player), the cleanest option is a `/diagnostic/gameState` endpoint or a field on the existing game-level snapshot. Minimal: add it to one player's entry or emit it as a `[GAME]` line on session start.

**Rationale:** needed if `GLOBAL_WARMING` is ever re-enabled. Currently cosmetic, but should be wired before the mechanic is revived.

### Hook C — `[PROP]` log tag: per-turn per-city pollution source breakdown

**File:** `CvCity.cpp` (piggyback on the `[CIT/proplevel]` block, or add a separate `[PROP]` domain)

**Pattern:** extend the existing `[CIT/proplevel]` at `CvCity.cpp:1244` to include a per-source breakdown at level 2. Or add a new `[PROP]` log tag (new domain: `logPropertyAI`, file `PropertyAI.log`, scope `gCityLogLevel`) that emits one line per active manipulator on the city.

Example line:

```
[PROP/source] turn=N city=X owner=P prop=PROPERTY_AIR_POLLUTION kind=DECAY val=-38
[PROP/source] turn=N city=X owner=P prop=PROPERTY_AIR_POLLUTION kind=CONSTANT_ATTR pop=12 val=+12
[PROP/propagate] turn=N city=X owner=P prop=PROPERTY_AIR_POLLUTION from=plot(3,5) pct=12 val=+47
```

**Rationale:** closes gap 3 (solver internals silent). With this, an agent can see exactly why pollution is rising or falling in a city — which buildings are contributing, how fast decay is working, what the plot propagation is adding. Essential for cascade shadow verification.

### Hook D — Add pollution columns to `PlotSnapshot`

**File:** `Sources/Utils/PlotSnapshot.cpp`

**Change:** extend the CSV schema (bump schema version) to include:

```
airPollution,waterPollution
```

Per-plot, emitting `pPlot->getPropertiesConst()->getValueByProperty(eAirPol)` and `eWatPol` (0 if property not present).

**Rationale:** closes gap 2 (plot pollution invisible). The propagated pollution plume is a spatial phenomenon; you cannot reason about it without per-plot values. This is the lowest-cost way to get a snapshot of the spatial distribution.

### Hook E — Curate pollution effect buildings as `autoBuild` + `requires.operate` bands in JSON

**Files:** `Assets/Data/` (one JSON file per effect building, or extend `property_air_pollution.json` / `property_water_pollution.json`)

**Change:** for each of the 24 effect buildings, add `"identity": {"autoBuild": true}` and a `requires.operate` atom `{type: "PROPERTY_AIR_POLLUTION", scope: "city", min: N, max: 100000}`. This converts them from `reason=noMarker` to `reason=place`/`requiresOperate` in the `placementSweep` shadow, closing the property-band auto-placement gap in cascade-mapping-inventory §B-i.

**Rationale:** not a new hook per se, but a data-curation step that makes the existing `placementSweep` shadow track all 24 pollution-driven buildings. Without it the shadow permanently reports noMarker and the maintainer (§14 H) cannot be safely deleted.

---

## 5. Summary table

| Item | Tier today | After proposed hooks |
|---|---|---|
| City air/water pollution value | 0 (invisible to HTTP) | 1 — in `/cities` snapshot (Hook A) |
| City pollution trend | 1 (via `[CIT/proplevel]` log) | 3 — `/events` stream + snapshot (Hook A + existing log) |
| Pollution source breakdown | 0 | 3 — `[PROP/source]` per-turn stream (Hook C) |
| Plot pollution spatial map | 0 | 2 — `PlotSnapshot` CSV (Hook D) |
| Effect-building placement (pollution) | 1 (building presence via `/cities` building count) | 3 — `placementSweep` kind=2 (Hook E data curation) |
| nukesExploded | 0 (HTTP) | 1 — `/players` or `/game` field (Hook B) |
| Global Warming trigger | N/A — compiled out | N/A until re-enabled |

**Current tier for this system: 1 (Telescreen) — city-level pollution values are in the gated file log but absent from the HTTP snapshot; the spatial/plot dimension is completely dark.**

---

## 6. Cross-references

- `docs/dev/plans/global-warming-mod.md` — vestige inventory + removal plan (#436).
- `docs/dev/plans/cascade-mapping-inventory.md` §B-i — the property-band auto-placement shadow (`checkPropertyBuildings`, kind=2 in `placementSweep`).
- `Assets/XML/GameInfo/CIV4PropertyInfos.xml` — authoritative manipulation rules for both pollution types.
- `Assets/Data/properties/property_air_pollution.json` / `property_water_pollution.json` — the curated JSON; currently missing `autoBuild` markers and band atoms.
- `Sources/CvPropertySolver.cpp` — the per-turn solve cycle (no logging today).
- `Sources/CvCity.cpp:1244` — `[CIT/proplevel]` emit site (all properties, all non-zero values).
- `Sources/CvHttpServer.cpp:97–101` — explicit comment "flammability and the pollutions are dormant" explaining why they are absent from `/cities`.
