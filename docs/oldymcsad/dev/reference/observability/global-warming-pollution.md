# Observability — Global Warming & Pollution — what's on the wire (the live half is pollution)

> **Status:** reference   ·   **Verified against:** 2026-06-20 (function sites + the `GLOBAL_WARMING` define re-confirmed in `Sources/CvGameCoreDLL.h`, `Sources/Engine/CvGame.cpp`, `Sources/Engine/CvCity.cpp`, `Sources/Engine/CvPropertySolver.cpp`; line numbers drift)
> **Grounding:** live `Sources/CvGameCoreDLL.h` (`#define GLOBAL_WARMING` commented out), `Sources/Engine/CvGame.cpp` (`doGlobalWarming` + its `#ifdef`-guarded call site, `doTurn` solver call), `Sources/Engine/CvPropertySolver.cpp` (`doTurn`, `gatherGlobalManipulators`, `PERF_SCOPE`), `Sources/Engine/CvCity.cpp` (`checkPropertyBuildings`, the `[CIT/proplevel]` emit), `Sources/Engine/CvProperties.cpp` (`getValueByProperty`), `Sources/Tools/CvHttpServer.cpp` (the "pollutions are dormant" comment), `Assets/XML/GameInfo/CIV4PropertyInfos.xml` (the manipulation rules + band tables). Carried from the old draft map; function sites re-confirmed, paths re-grounded to the reorganized `Sources/` tree.
> **The key fact:** this map spans two systems, and only one is live. **Global Warming / Nuclear Winter is compiled out — the mechanic does not run.** Only the **pollution PROPERTY system** (`PROPERTY_AIR_POLLUTION` / `PROPERTY_WATER_POLLUTION`) is live: per-city + per-plot accrual through the standard `CvPropertySolver` pipeline, driving ~24 effect buildings via property-band auto-placement. Pollution sits at **Tier 1 (Telescreen)** — city values are in a gated file log but absent from the HTTP snapshot, and the plot/spatial dimension is completely dark. This map separates the dead mechanic from the live one, walks the pollution solve cycle + effect-building bands, names what's dark, and proposes the snapshot fields / log tags / data-curation to climb to Tier 3/4.

The observability scale (0–5) and the three canonical hook shapes are defined once in
[`README.md`](README.md) ([DEC-obs-scale], [DEC-obs-hook-shapes]); the live surface and the rules for
reading it (logs held open mid-session, use `/events` + `/diagnostic`) live in
[`http-server.md`](http-server.md). This doc does not restate them.

> Line numbers below are anchors at time of writing and **drift** — confirm the named function, not the
> integer.

---

## 1. How it actually works

### 1a. Global Warming / Nuclear Winter — permanently compiled out

The entire mechanic is gated on a commented-out `#define`:

```cpp
// #define GLOBAL_WARMING          // Sources/CvGameCoreDLL.h:232
```

Consequences:

- `CvGame::doGlobalWarming()` (`Sources/Engine/CvGame.cpp:6588`) is **never compiled** and never runs.
- The per-turn call site (`Sources/Engine/CvGame.cpp:5995`, `PERF_SCOPE("game.doGlobalWarming")`) is
  `#ifdef`-guarded and likewise dead.
- The mechanic DID read `CvFeatureInfo::getWarmingDefense()` (tree-hugger defence) and
  `CvGame::getNukesExploded()` (nuke-weight spike); both accessors still compile but drive nothing.

**Vestiges that compile but do nothing:** `CvFeatureInfo::m_iWarmingDefense` / `getWarmingDefense()`
(`Sources/Infos/CvFeatureInfo.cpp`, readable, no consumer); the `getWarmingDefense` Python binding; the
Pedia feature display (renders a zero-effect stat to players); the `GLOBAL_WARMING_*` defines in
`GlobalDefines.xml`; and `CvGame::getNukesExploded()` — serialized, incremented on nuke use, Python-exposed,
but only consumed inside the dead `#ifdef GLOBAL_WARMING` block. The full vestige inventory and the
owner-sanctioned removal plan (#436) live in the old `docs/dev/plans/global-warming-mod.md`
(UNVERIFIED in docs2 — not yet migrated into this set).

### 1b. Pollution — per-turn property system (LIVE)

Both `PROPERTY_AIR_POLLUTION` and `PROPERTY_WATER_POLLUTION` are live and follow the standard
`CvPropertySolver` pipeline, invoked once per turn from `CvGame::doTurn` (`m_PropertySolver.doTurn()`):

`CvPropertySolver::doTurn()` (`Sources/Engine/CvPropertySolver.cpp:448`, `PERF_SCOPE` at `:451`):

1. `resetPropertyChanges()` — clears all `change` rates on every game object.
2. `gatherGlobalManipulators()` (`Sources/Engine/CvPropertySolver.cpp:411`) — loads every
   `CvPropertyInfo`'s manipulator set once. Both pollution types register (from
   `Assets/XML/GameInfo/CIV4PropertyInfos.xml`):
   - City scope: `PROPERTYSOURCE_DECAY` ~6%/turn + `PROPERTYSOURCE_ATTRIBUTE_CONSTANT` of +1/population/turn.
   - City→adjacent-plots: `PROPERTYPROPAGATOR_DIFFUSE` ~5% of city value to nearby non-peak plots.
   - Plot→same-city: `PROPERTYPROPAGATOR_DIFFUSE` ~12% back to the city.
   - Plot→adjacent-plots: `PROPERTYPROPAGATOR_DIFFUSE` ~4% to neighbouring non-ocean plots.
   - Plot scope: `PROPERTYSOURCE_DECAY` ~6%/turn.
3. `gatherAndSolve()` — propagators first, then interactions, then sources; each phase is
   predict→applyChanges→correct→applyChanges.
4. Result: every city and surrounding plot has its air/water pollution value updated in `CvProperties`
   storage (`Sources/Engine/CvProperties.cpp`, read via `getValueByProperty`).

**Building sources:** many regular/special buildings contribute `PROPERTYSOURCE_CONSTANT` deltas to
pollution on their host city (industrial buildings add; environmental-policy buildings — e.g. Carpool
Ordinance, −25 air pollution — reduce).

**Effect buildings (property-band auto-placement):** `CvCity::checkPropertyBuildings()`
(`Sources/Engine/CvCity.cpp:1500`, called from the city-doTurn autobuild block at `:1467`) runs every turn.
For each pollution band defined in `CIV4PropertyInfos.xml`'s `<PropertyBuildings>`, it adds or removes the
corresponding effect building — 12 air-pollution bands (`BUILDING_POLLUTION_LIGHT_SMOG` ≥400 …
`BUILDING_POLLUTION_BLACKENED_SKIES` ≥1950) and 12 water-pollution bands
(`BUILDING_POLLUTION_MINOR_GROUNDWATER_POLLUTION` ≥450 … `BUILDING_POLLUTION_TOXIC_HYDROSPHERE` ≥1800).
These ~24 buildings are the property-band auto-placement maintainer's targets (`kind=2` in `placementSweep`),
which the cascade models via `identity.autoBuild` + a `requires.operate` `PROPERTY_X` band atom — see the
cascade design below.

**Targets:** `targetLevel` = 0 for both pollution types; the solver drives toward 0 via the ~6%/turn decay.
Buildings adding pollution fight this decay; a city only accumulates if sources outpace it.

**Nuclear Winter secondary:** even compiled out, `getNukesExploded()` is still updated on nuke fire and
saved, but it does **not** affect the pollution properties — pollution is entirely building-driven. The only
connection was inside the dead `doGlobalWarming`.

---

## 2. What's on the wire today — **Tier 1 (Telescreen)** for pollution

### What exists

| What | Where | Notes |
|---|---|---|
| City air/water pollution value + per-turn change | `[CIT/proplevel]` in `CityAI.log` (gated `gCityLogLevel ≥ 1`), `Sources/Engine/CvCity.cpp:1249` | ALL active properties logged each turn-start per city — including both pollution types **once non-zero**. Teed to `/events` at `gStreamLogLevel`. |
| City crime / education / disease (the live triad) | `/cities`: `crime`, `education`, `disease` JSON fields | The three properties explicitly exposed in `Sources/Tools/CvHttpServer.cpp`. |
| Pollution effect-building presence (indirect) | `/diagnostic/placementSweep?player=N` + `[PLACEMENT]` per-turn | The 24 buildings are property-band maintainer targets (`kind=2`); presence of `BUILDING_POLLUTION_LIGHT_SMOG` etc. tells you the level crossed a band. Currently `reason=noMarker` (JSON not yet autoBuild-flagged) — the shadow flags the gap. |
| `nukesExploded` game counter | Python via `CyGame.getNukesExploded()` | Serialized; no HTTP endpoint. |
| Property-solver turn cost | `[PERF/phase]` `phase=CvPropertySolver::doTurn` | `PERF_SCOPE` at `Sources/Engine/CvPropertySolver.cpp:451`; visible via Performance.log / `/events`. |

### Confirmed absent

| Gap | Why |
|---|---|
| **City air/water pollution values in `/cities`** | `Sources/Tools/CvHttpServer.cpp:100` names crime/education/disease only; the comment states "flammability and the pollutions are dormant." |
| **Plot-level pollution values** | Plots have `CvProperties` containers (propagators write to `GAMEOBJECT_PLOT`) but no HTTP endpoint exposes plot properties, and the `PlotSnapshot` CSV has no property columns. |
| **Per-turn net source/decay breakdown** | The solver runs silently — no `CvPropertySolver` logging at any level. Two consecutive `[CIT/proplevel]` lines give the delta but not the components (building source vs population source vs decay vs plot propagation). |
| **Which buildings contribute to pollution** | No log or endpoint lists a city's active `PROPERTYSOURCE_CONSTANT` contributors; only the total is observable. |
| **`nukesExploded`** | Not in `/players` or any endpoint. Drove nuclear winter inside the dead `doGlobalWarming`; effectively invisible from outside. |
| **Global-warming trigger state** | No trigger state to expose — the entire mechanic is compiled out. |

---

## 3. The gap

To meet the reconstruct-from-API bar ([DEC-map-before-delete]):

1. **City pollution values are invisible in the HTTP snapshot.** `/cities` has no `airPollution` /
   `waterPollution`; an agent cannot know a city's pollution level, its direction of change, or which
   effect-building band is about to trigger.
2. **Plot-level pollution spread is entirely unobservable.** The solver diffuses pollution to adjacent
   plots every turn; no endpoint or log surfaces per-plot property values, so a pollution plume around a
   dense industrial core is dark.
3. **Property-solver internals are silent.** The predict-correct cycle has no logging — no way to see which
   manipulators fired, which sources were active, or the intermediate predict values.
4. **Effect-building placement is only partially observable.** Via `placementSweep` (Tier 3) but currently
   incomplete: pollution buildings report `reason=noMarker` (their JSON lacks the `autoBuild` flag and the
   `requires.operate` band atom), so the cascade cannot yet reproduce their auto-placement.
5. **`nukesExploded` is invisible.** Serialized game state with no HTTP surface — irrelevant while
   `GLOBAL_WARMING` is compiled out, but the primary driver of nuclear-winter intensity if ever re-enabled.

---

## 4. Proposed hooks — climbing from Tier 1 to Tier 3/4

All hooks follow the three canonical hook shapes — see [DEC-obs-hook-shapes].

### 4a. `airPollution` / `waterPollution` on `/cities` (snapshot; Tier 0→1, the top gap)

In `CitySnap` add `int iAirPollution; int iWaterPollution;`; populate alongside crime/education/disease in
the city snapshot walk from `pProps->getValueByProperty(GC.getPROPERTY_AIR_POLLUTION())` (and water), and
emit `airPollution` / `waterPollution` in the JSON render. Two `getValueByProperty` lookups per city per 5 s
publish — negligible. Closes the single most important gap: pollution is the only live gameplay property
missing from `/cities`.

### 4b. `nukesExploded` on `/players` or a `/diagnostic` field (snapshot)

It is a per-**game** counter, so the cleanest home is a game-level snapshot field (a `/diagnostic/gameState`
entry, or one `[GAME]` line on session start) rather than a per-player field. Cosmetic today; should be
wired before `GLOBAL_WARMING` is ever revived.

### 4c. `[PROP]` log tag — per-turn per-city pollution source breakdown (Tier 3)

Either extend the existing `[CIT/proplevel]` block (`Sources/Engine/CvCity.cpp:1249`) at level 2, or add a
new `logPropertyAI` domain (`PropertyAI.log`, scope `gCityLogLevel`) that emits one line per active
manipulator:

```
[PROP/source]    turn=N city=X owner=P prop=PROPERTY_AIR_POLLUTION kind=DECAY         val=-38
[PROP/source]    turn=N city=X owner=P prop=PROPERTY_AIR_POLLUTION kind=CONSTANT_ATTR pop=12 val=+12
[PROP/propagate] turn=N city=X owner=P prop=PROPERTY_AIR_POLLUTION from=plot(3,5) pct=12 val=+47
```

Closes gap 3 (solver internals silent): an agent can see exactly why pollution is rising/falling — which
buildings contribute, how fast decay works, what plot propagation adds. Essential for cascade shadow
verification.

### 4d. Pollution columns on `PlotSnapshot` (Tier 2)

Extend the `Sources/Utils/PlotSnapshot.cpp` CSV schema (bump the schema version) with
`airPollution,waterPollution`, per-plot via `pPlot->getPropertiesConst()->getValueByProperty(...)` (0 if
absent). Closes gap 2 (plot pollution invisible) — the propagated plume is a spatial phenomenon, and this is
the lowest-cost snapshot of its distribution.

### 4e. Curate the 24 effect buildings as `autoBuild` + `requires.operate` bands (data, not a hook)

For each of the 24 effect buildings, add `identity.autoBuild` and a `requires.operate` `PROPERTY_X` band
atom (`{type: PROPERTY_AIR_POLLUTION, scope: city, min: N, max: …}`) to its `Assets/Data/` JSON. This flips
the `placementSweep` shadow for these buildings from `reason=noMarker` to a real placement disposition,
closing the property-band auto-placement gap so the maintainer can be safely deleted. (See the cascade
design below — this is the data side of the same maintainer.)

---

## 5. Cost & priority

| Item | Tier today | Hook | After |
|---|---|---|---|
| City air/water pollution value | 0 (invisible to HTTP) | §4a `/cities` fields | 1 — in snapshot |
| City pollution trend | 1 (via `[CIT/proplevel]`) | §4a + existing log | 3 — `/events` + snapshot |
| Pollution source breakdown | 0 | §4c `[PROP/source]` | 3 — per-turn stream |
| Plot pollution spatial map | 0 | §4d `PlotSnapshot` CSV | 2 — per-plot CSV |
| Effect-building placement (pollution) | 1 (presence via building count) | §4e data curation | 3 — `placementSweep` `kind=2` |
| `nukesExploded` | 0 (HTTP) | §4b `/players` or `/game` | 1 — snapshot field |
| Global-warming trigger | N/A — compiled out | — | N/A until re-enabled |

**Current tier for this system: 1 (Telescreen)** — city-level pollution values are in the gated file log but
absent from the HTTP snapshot; the spatial/plot dimension is completely dark.

---

## 6. Code cross-reference

> Paths re-grounded to the reorganized `Sources/` tree (`Cv*` engine + `CvPropertySolver`/`CvProperties` →
> `Sources/Engine/`; `CvFeatureInfo` → `Sources/Infos/`; `CvHttpServer` → `Sources/Tools/`; `PlotSnapshot`
> → `Sources/Utils/`; `CvGameCoreDLL.h` stays at `Sources/`). Line numbers drift — confirm the function.

| Claim | Source |
|---|---|
| `#define GLOBAL_WARMING` (commented out) | `Sources/CvGameCoreDLL.h:232` |
| `CvGame::doGlobalWarming()` (never compiled) | `Sources/Engine/CvGame.cpp:6588` |
| dead call site (`#ifdef`-guarded `PERF_SCOPE`) | `Sources/Engine/CvGame.cpp:5995` |
| `CvPropertySolver::doTurn()` + `PERF_SCOPE` | `Sources/Engine/CvPropertySolver.cpp:448 / 451` |
| `gatherGlobalManipulators()` | `Sources/Engine/CvPropertySolver.cpp:411` |
| `CvCity::checkPropertyBuildings()` (band auto-placement) | `Sources/Engine/CvCity.cpp:1500` (called `:1467`) |
| `[CIT/proplevel]` emit (all properties, non-zero) | `Sources/Engine/CvCity.cpp:1249` |
| `CvProperties::getValueByProperty` | `Sources/Engine/CvProperties.cpp` |
| `"flammability and the pollutions are dormant"` comment | `Sources/Tools/CvHttpServer.cpp:100` |
| `CvFeatureInfo::getWarmingDefense()` (inert vestige) | `Sources/Infos/CvFeatureInfo.cpp` |
| `PlotSnapshot` CSV (add pollution columns) | `Sources/Utils/PlotSnapshot.cpp` |
| pollution manipulation rules + band tables | `Assets/XML/GameInfo/CIV4PropertyInfos.xml` |
| curated pollution JSON (missing `autoBuild` + band atoms) | `Assets/Data/properties/property_air_pollution.json` / `property_water_pollution.json` |

---

## See also
- [`README.md`](README.md) — the observability scaffold: the 0–5 scale ([DEC-obs-scale]), the Orwell bar,
  and the three canonical hook shapes ([DEC-obs-hook-shapes]) this map's hooks instantiate.
- [`http-server.md`](http-server.md) — the live surface (`/cities`, `/diagnostic/placementSweep`) these
  hooks extend, and the live-read rules (logs held open mid-session).
- [`../../reference/cascade/data-model.md`](../../reference/cascade/data-model.md) — defines the
  `identity.autoBuild` placement marker and the `PROPERTY_X` `requires.operate` band atom (§4e) that the 24
  pollution effect-buildings must carry.
- [`../../reference/cascade/legacy-value-calc-map.md`](../../reference/cascade/legacy-value-calc-map.md) —
  the legacy effect-band gating (`checkPropertyBuildings`) the cascade replaces; the maintainer this map's
  §4e data-curation lets the shadow close.
- [`../../explanation/cascade-architecture.md`](../../explanation/cascade-architecture.md) — why total
  observability is load-bearing: the property-band auto-placement maintainer cannot be cut until its
  placement is shadowed ([DEC-map-before-delete]).
- [`../../README.md`](../../README.md) — the comprehension map / overview-of-overviews.

[DEC-obs-scale]: ../../architecture/decisions.md#dec-obs-scale
[DEC-obs-hook-shapes]: ../../architecture/decisions.md#dec-obs-hook-shapes
[DEC-map-before-delete]: ../../architecture/decisions.md#dec-map-before-delete
