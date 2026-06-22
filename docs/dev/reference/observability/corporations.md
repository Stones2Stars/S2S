# Observability — Corporations — what's on the wire for HQs, spread, and the gold drain

> **Status:** reference   ·   **Verified against:** 2026-06-21 (corp emit surface re-confirmed in `Sources/Tools/CvHttpServer.cpp`; mechanics function sites in `Sources/Engine/CvCity.cpp`, `CvGame.cpp`, `CvPlayer.cpp`, `CvUnit.cpp`, `Sources/AI/`; line numbers drift)
> **Grounding:** live `Sources/Engine/CvCity.cpp` (`doCorporation`, `getCorporationInfluence`, `setHasCorporation`, `applyCorporationModifiers`, `updateCorporationBonus`, `calculateCorporationMaintenanceTimes100`, `calcCorporateMaintenance`, `isActiveCorporation`, `isHasCorporation`), `Sources/Engine/CvGame.cpp` (`doFoundCorporation`, `setHeadquarters`), `Sources/Engine/CvPlayer.cpp` (`foundCorporation`, `isActiveCorporation`), `Sources/Engine/CvUnit.cpp` (`canSpreadCorporation`, `spreadCorporation`, `spreadCorporationCost`), `Sources/AI/CvUnitAI.cpp` (`AI_spreadCorporation`), `Sources/AI/CvPlayerAI.cpp` (`AI_corporationValue`), and the corp emits in `Sources/Tools/CvHttpServer.cpp` (per-city snapshot + the per-city diagnostic).
> Corporations are now **partially observable (~Tier 2–3)**, not the Tier 0 of an earlier draft. The per-city snapshot emits the **presence map** — `corporations` (`isActiveCorporation`) and `presentCorporations` (`isHasCorporation`) — and the per-city diagnostic emits the corp **commerce / maintenance / health / happiness** breakdowns plus the world/team/handicap maintenance scalers. So presence, the dormancy gate (§1e), and the maintenance/yield state are reconstructible from the wire. **Still DARK:** HQ location per corp, founded turn, per-city influence (`getCorporationInfluence`), the spread/decay **event spine**, the player-level civic gates (`noCorporations`/`noForeignCorporations`/`hasCorporationCount`), AI value, and the resource-transformation chain — so spread *dynamics* and HQ-dependent influence cannot yet be reconstructed. This map walks the mechanics, names what's still dark, and proposes the remaining hooks to reach Tier 4.

The observability scale (0–5) and the three canonical hook shapes are defined once in
[`README.md`](README.md) ([DEC-obs-scale], [DEC-obs-hook-shapes]); the live surface and the rules for
reading it (logs held open mid-session, use `/events` + `/diagnostic`) live in
[`http-server.md`](http-server.md). This doc does not restate them.

> Line numbers below are anchors at time of writing and **drift** — confirm the named function, not the
> integer.

---

## 1. How it actually works

### 1a. Two modes — Classic vs Advanced Realistic

Two distinct operating modes, gated by `GAMEOPTION_ADVANCED_REALISTIC_CORPORATIONS`
(`Sources/Defines/CvEnums.h:868`):

- **Classic** (flag absent): corporations spread **manually** via "executive" units running
  `MISSION_SPREAD_CORPORATION`. `CvCity::doCorporation()` returns immediately. No autonomous per-turn spread.
- **Advanced** (flag present): `doCorporation()` runs per city per turn and autonomously propagates or
  decays each corporation. Manual unit-spread is **blocked** unless
  `MODDERGAMEOPTION_NO_AUTO_CORPORATION_FOUNDING` is also set (`Sources/Engine/CvUnit.cpp`, in
  `canSpreadCorporation`).

In both modes the gold drain is always present once a corporation is active:
`calculateCorporationMaintenanceTimes100()` (`Sources/Engine/CvCity.cpp:7801`) and the Advanced-mode
pop-weighted `calcCorporateMaintenance()` (`Sources/Engine/CvCity.cpp:21754`).

### 1b. Founding / headquarters placement

- **Advanced**: `CvGame::doFoundCorporation()` (`Sources/Engine/CvGame.cpp:11131`) is the auto-founder. It
  runs periodically (every `CORPORATION_FOUND_CHECK_TURNS` × speedPercent turns) and selects a best city by
  `getCorporationInfluence()` × `getSpread()` + RNG (`CvGame.cpp:11195`). If
  `MODDERGAMEOPTION_NO_AUTO_CORPORATION_FOUNDING` is set it defers to the unit-spread path.
- **Classic**: `CvPlayer::foundCorporation()` (`Sources/Engine/CvPlayer.cpp:8893`) is called from event
  script / Python. It picks the best city by population + bonus resources + RNG, then calls
  `CvCity::setHeadquarters()` → `CvGame::setHeadquarters()` (`Sources/Engine/CvGame.cpp:5619`), which
  installs the HQ building and calls `setHasCorporation(corp, true)` on the HQ city.

`isCorporationFounded` = `m_paiCorporationGameTurnFounded[eIndex] != -1`; the turn is recorded in
`CvGame::makeCorporationFounded()`.

### 1c. Per-turn autonomous spread and decay (Advanced only)

`CvCity::doCorporation()` (`Sources/Engine/CvCity.cpp:21549`), called inside `CvCity::doTurn()`, for each
corporation not yet present in this city:

1. Checks HQ exists + `canEverSpread()` (game-option gate) + not blocked by civic
   (`isNoCorporations()` / `isNoForeignCorporations()`).
2. Sums `iRandThreshold` = max over all CONNECTED cities of
   `pLoopCity->getCorporationInfluence(corp)` × `getSpread()` / 100 ÷ distance-factor (`CvCity.cpp:21582`),
   then multiplied by player spread modifier and owner influence (`CvCity.cpp:21606`), divided by
   `1 + getCorporationCount()/2`.
3. Rolls `getSorenRandNum(CORPORATION_SPREAD_RAND × speedPercent / 100)`. If roll < threshold → removes
   competing corps first, then `setHasCorporation(corp, true)`.

**Decay** (also in `doCorporation()`): for cities that already have the corp but are not the HQ, compares
`getAverageCorporationInfluence()` against the city's own adjusted influence. If the average exceeds local
influence (`iDiff > 0`), rolls the same RNG; if roll < `iDiff` → `setHasCorporation(corp, false)`.

### 1d. Manual unit spread (Classic, or Advanced+NO_AUTO_FOUNDING)

- `CvUnit::canSpreadCorporation()` (`Sources/Engine/CvUnit.cpp:8552`): checks `getCorporationSpreads > 0`,
  city present, not already has corp, obsolete-tech check, building prereqs, can-enter-area, owner
  `isActiveCorporation`, no competing HQ in city, at least one prereq bonus present, gold ≥ cost.
- `CvUnit::spreadCorporation()` (`Sources/Engine/CvUnit.cpp:8695`): deducts gold (cost modified by foreign
  territory and competing corps), rolls `getCorporationSpreads(corp)` (halved for foreign territory) against
  `getSorenRandNum(100)`, calls `setHasCorporation` on success, then kills the unit either way.
- `CvUnit::spreadCorporationCost()` (`Sources/Engine/CvUnit.cpp:8669`): base `getSpreadCost()` ×
  foreign-percent modifier × per-active-competing-corp spread factor.

### 1e. `isActiveCorporation` — the dormancy gate

- **City level** (`CvCity::isActiveCorporation()`, `Sources/Engine/CvCity.cpp:13430`): the city
  `isHasCorporation` AND `CvPlayer::isActiveCorporation(corp)` AND corp not obsoleted by team tech AND the
  city has ≥1 prereq bonus (if the corp requires any).
- **Player level** (`CvPlayer::isActiveCorporation()`, `Sources/Engine/CvPlayer.cpp:14031`): not
  `isNoCorporations()`, not (`isNoForeignCorporations()` and doesn't own HQ), not obsoleted by team tech.

**Effect of going inactive**: all yields/commerce from the corp drop to 0
(`getCorporationYieldByCorporation` / `getCorporationCommerceByCorporation` return 0 when
`!isActiveCorporation`); maintenance still accrues but at 0 rate; buildings whose `getPrereqCorporation()`
matches are disabled via `applyCorporationModifiers()` → `setDisabledBuilding()`
(`Sources/Engine/CvCity.cpp:15193`).

### 1f. Resource consumption, bonus production, maintenance, yields

- **Influence** (`getCorporationInfluence()`, `Sources/Engine/CvCity.cpp:21678`): city-level influence =
  100 base, +1 per available instance of each prereq bonus, + `CORPORATION_RESOURCE_BASE_INFLUENCE /
  iBonusesConsumed` per *present* prereq bonus, ÷10 per active competing corp, × population / averagePop.
- **Resource transformation** (`updateCorporationBonus()`, `Sources/Engine/CvCity.cpp:12691`): for each
  active corp with `getBonusProduced() != -1`, adds produced-bonus units to a `m_corpBonusProduction`
  vector (iterated to handle chained production), applied via `processBonus()` when the set changes — e.g.
  corp A consumes Oil → produces Steel.
- **Maintenance** (`calculateCorporationMaintenanceTimes100(corp)`, `Sources/Engine/CvCity.cpp:7817`):
  base = Σ `HeadquarterCommerce` over commerce types × 100; + `getMaintenance() × numBonuses ×
  worldSizeCorporationMaintenancePercent / 100`; × `(17 + pop) / 18`; Advanced × handicap²/8000, Classic ×
  handicap/100; × player/team maintenance modifier; rebels pay 50%.
- **Yields/commerce** (`getCorporationYieldByCorporation` / `...Commerce...`): when active and not in
  disorder, contribute `getYieldChange × 100 + getYieldProduced × Σnumbonuses × worldSizePercent / 100`
  (commerce also × team `getCorporationRevenueModifier`). Aggregated into `m_aiCorporationYield` /
  `m_aiCorporationCommerce`, folded into `getBaseCommerceRate()`.

### 1g. AI decision-making

- `CvUnitAI::AI_spreadCorporation()` (`Sources/AI/CvUnitAI.cpp:13737`): if a unit can spread a corp and a
  good target city exists, pushes `MISSION_SPREAD_CORPORATION`. The push logs via `[UNT/act]` (level 2);
  the spread **result** at the target is not logged.
- `CvPlayerAI::AI_corporationValue()` (`Sources/AI/CvPlayerAI.cpp:12469`): approximate gpt value of
  spreading a corp to a city (drives executive training + spread-target choice). Internal; not observable.
- `CvCityAI` trains executive units off a random-gated `AI_executiveValue` check; the train decision only
  surfaces as `[CIT/produced]` when the unit completes.

---

## 2. What's on the wire today — **~Tier 2–3** for corporations

### What exists (verified `CvHttpServer.cpp`, 2026-06-21)

| Field / stream | Corporation state |
|---|---|
| per-city `corporations` (snapshot, :855) | the **active** corps in the city (`isActiveCorporation`) — array of corp types |
| per-city `presentCorporations` (snapshot, :864) | the **present** corps (`isHasCorporation`) — a branch can be present-but-inactive; this is the set the building corp-PREREQ dormancy reads |
| world `corporationLevels` / `corporationMaintenancePercent` (:954/957) | game-wide corp-level counts (HQ-commerce scaler) + the world corp-output/maintenance scaler |
| team `corporationRevenueModifier` (:1140) | corp-output revenue modifier |
| diagnostic `corporationCommerceDetail` (:1994) | per-ACTIVE-corp commerce breakdown (`getCorporationCommerceByCorporation`) |
| diagnostic `corporationCommerce` / `corporationMaint100` (:2164/2268) | per-city aggregate corp commerce + corp maintenance ×100 |
| diagnostic maint mods (:2283–2295) | `playerCorpMaintMod` / `teamCorpMaintMod` / `worldCorpMaintPct` / `handicapCorpMaintPct` + `optAdvancedRealisticCorporations` |
| diagnostic per-corp maintenance internals (:2348) | per-active-corp HQ-commerce + prereq-bonus × city-count breakdown |
| diagnostic `corporationHealth` / `corporationHappiness` (:2423/2482) | per-city corp health & happiness contributions |
| constructibility reasons (:540/556) | `prereqCorp` / `foundsCorp` greying reasons reference corp state |
| `[UNT/act]` (L2) · `[CIT/produced]` (L1) · `[PERF/phase]` (L1) | executive mission push · executive completing production · `doCorporation` phase time |

### Still absent

| State | Where it lives | On the wire? |
|---|---|---|
| HQ location per corp | `CvGame.m_paHeadquarters[]` | **NO** |
| Corp founded turn | `CvGame.m_paiCorporationGameTurnFounded[]` | **NO** (used internally for the `foundsCorp` reason, not emitted) |
| Per-city influence score | `getCorporationInfluence()` | **NO** |
| Spread/decay rolls + threshold + events | inside `doCorporation()` | **NO** — no `[CORP]` tags; `setHasCorporation` flips are silent (human-UI message only) |
| Corp bonus production (resource transform) | `m_corpBonusProduction` | **NO** |
| Player-level civic gates / spread mod / per-corp count | `isNoCorporations()` / `isNoForeignCorporations()` / `getCorporationSpreadModifier()` / `getHasCorporationCount()` | **NO** |
| AI corp value (`AI_corporationValue`) | internal to `CvPlayerAI` | **NO** |
| `/diagnostic/corpSweep` | — | **NO** (not implemented) |

---

## 3. The gap

The static corporation state is now largely on the wire. Against the reconstruct-from-API bar
([DEC-map-before-delete]) an agent today **CAN**: reconstruct the **presence map** (`corporations` /
`presentCorporations` per city), evaluate the **dormancy gate** (§1e) and the building-disable cascade
(`applyCorporationModifiers` reads present-corp + prereq bonuses, both observable), and isolate per-city
**corp maintenance and commerce** (`corporationMaint100`, `corporationCommerce[Detail]`, the maint-mod
scalers, `corporationHealth/Happiness`).

What remains DARK — the **dynamics and the HQ-anchored influence**, against the same bar an agent **cannot**:

1. **Observe spread/decay events** — `doCorporation()` flips `setHasCorporation` silently (no `[CORP]` tag;
   human-UI message only). An autoplay session where corp boundaries shift cannot be reconstructed turn-by-turn.
2. **Know the HQ location** — load-bearing for the influence formula and the "connected to HQ" condition;
   absent from every snapshot.
3. **Compute per-city influence** — `getCorporationInfluence()` (the spread/decay driver) is not emitted.
4. **See the player-level civic gates** — `noCorporations` / `noForeignCorporations` / `hasCorporationCount`
   are absent, so the player-level active gate (§1e) cannot be fully reconstructed.
5. **Observe the AI corporation decision** — `[UNT/act]` surfaces the mission push, but the value
   calculation (`AI_corporationValue`) is silent; the train-an-executive decision is only visible *after the
   fact* via `[CIT/produced]`.
6. **Observe the resource-transformation chain** — `updateCorporationBonus()` mutates the effective bonus
   set silently; the produced bonus shows in the count but the chain is invisible.

---

## 4. Proposed hooks — climbing from Tier 0 to Tier 4

All hooks follow the three canonical hook shapes — see [DEC-obs-hook-shapes].

### 4a. Snapshot fields

**`/cities` presence map — DONE.** The per-city snapshot emits two arrays (`CvHttpServer.cpp` :855/:864)
rather than the single annotated array first proposed: `corporations` (active, `isActiveCorporation`) and
`presentCorporations` (present, `isHasCorporation`). The present/active split is exactly what the building
corp-prereq dormancy needs (it gates on PRESENT). **Still TODO on `/cities`:** the **HQ flag** per corp
(`GC.getGame().getHeadquarters(corp) == pCity`) — load-bearing for the influence formula and the
"connected to HQ" condition, and not currently emitted.

**`/players` — TODO** (HIGH; reconstructs the player-level active gate):

```jsonc
"hqCorps":             ["CORPORATION_MINING_INC", ...],  // hasHeadquarters(corp)
"hasCorporationCount": {"CORPORATION_MINING_INC": 7},    // getHasCorporationCount(), corps with count>0
"noCorporations":        <bool>,                          // isNoCorporations()  (civic gate)
"noForeignCorporations": <bool>                           // isNoForeignCorporations()  (civic gate)
```

### 4b. Log additions (per-turn stream; Tier 3)

New `logCorpAI` helper (or reuse `logCityAI`), `[CORP]` tags, gated `gCityLogLevel` (zero cost when off):

| Tag | Level | Site | Line |
|---|---|---|---|
| `[CORP/spread]` | 1 | the two `setHasCorporation` calls in `doCorporation()` (`CvCity.cpp:21549`) | `turn= city= owner= corp= action=spread\|decay threshold= roll= influence=` (decay: `avgInfluence= cityInfluence=`) |
| `[CORP/spread]` (skip) | 2 | `doCorporation()` per skipped city | `turn= city= owner= corp= action=skip reason=noHQ\|noCivic\|noBonusReach\|competitorPresent` |
| `[CORP/found]` | 1 | `CvGame::setHeadquarters()` (`CvGame.cpp:5619`) when `pNewValue != NULL`; also `doFoundCorporation()` best-city pick | `turn= corp= city= owner= x= y= foundTurn=` (auto: `mode=auto spread=`) |
| `[CORP/maint]` | 2 | `calcCorporateMaintenance()` / `calculateCorporationMaintenanceTimes100(corp)` per active corp per city | `turn= city= owner= corp= maintenance= bonuses= pop= handicapMod=` |
| `[CORP/bonus]` | 2 | `updateCorporationBonus()` (`CvCity.cpp:12691`) when the produced set mutates | `turn= city= owner= corp= produced=BONUS_X count= consumes=BONUS_Y` |
| `[CORP/ai]` | 3 | `AI_corporationValue()` (`CvPlayerAI.cpp:12469`) + `AI_spreadCorporation()` commit (`CvUnitAI.cpp:13737`) | `turn= player= corp= city= value= reason=<label>` (mission: `unit= action=spreadMission targetCity=`) |

The `[CORP/spread]` and `[CORP/found]` tags are the event spine — without them, autoplay reconstruction of
the shifting presence map is impossible.

### 4c. `/diagnostic/corpSweep?player=N` (Tier 4)

The mailbox-serviced point-in-time analogue of §4a (the `placementSweep` pattern) — a full corp-state dump
for verification. For each corporation: HQ city+owner, founding turn, player-level `isActiveCorporation`;
and per the player's cities: present / active / city-level influence / per-corp maintenance.

```json
{
  "player": 1,
  "noCorporations": false, "noForeignCorporations": false,
  "corps": [
    {"type": "CORPORATION_MINING_INC", "founded": true, "hqCity": "Rome", "hqOwner": 2,
     "foundTurn": 145, "playerActive": true, "cityCount": 7,
     "cities": [
       {"cityId": 3, "name": "London", "has": true, "active": true, "influence": 132, "maintenance": 44}
     ]}
  ]
}
```

---

## 5. Cost & priority

| Hook | Tier gain | Effort | Rationale |
|---|---|---|---|
| ~~`/cities` presence arrays (§4a)~~ | **DONE** | — | presence map (`corporations` + `presentCorporations`) — everything else builds on it |
| `/cities` HQ flag (§4a) | →partial | snapshot-walk read | **HIGH** — HQ anchors the influence formula + "connected to HQ" |
| `/players` gate fields (§4a) | →partial | struct + walk reads | **HIGH** — player-level active gate (`noCorporations`/`noForeignCorporations`) + per-corp counts |
| `[CORP/spread]` + `[CORP/found]` (§4b L1) | →3 | gated, zero cost off | **HIGH** — the event spine; live shift of the presence map + HQ placement on `/events` |
| `/diagnostic/corpSweep` (§4c) | 4 | mailbox slot (`placementSweep` pattern) | **MEDIUM** — on-demand full state for cascade shadow (much of it is now in the per-city diagnostic) |
| `[CORP/ai]` (§4b L3) | 3 | gated | **LOW–MEDIUM** — fills the value reasoning behind the `[UNT/act]` commit |

The per-city presence arrays + the per-city corp commerce/maintenance/health/happiness diagnostic put
corporations at **~Tier 2–3**. To reach **Tier 4 (Thought Police)** — full per-turn state, incl. spread
*dynamics*, reconstructible for AI players the agent cannot watch — the remaining hooks are the **HQ flag**,
the **player-level gates**, and the **`[CORP/spread]`/`[CORP/found]` event spine**.

---

## 6. Code cross-reference

> Paths re-grounded to the reorganized `Sources/` tree (`Cv*` engine → `Sources/Engine/`, `Cv*AI` →
> `Sources/AI/`, `CvHttpServer` → `Sources/Tools/`, enums → `Sources/Defines/`, info classes →
> `Sources/Infos/`). Line numbers drift — confirm the function.

| Claim | Source |
|---|---|
| `CvCity::doCorporation()` (spread/decay, Advanced) | `Sources/Engine/CvCity.cpp:21549` |
| `CvCity::getCorporationInfluence()` | `Sources/Engine/CvCity.cpp:21678` |
| `CvCity::isActiveCorporation()` (city dormancy gate) | `Sources/Engine/CvCity.cpp:13430` |
| `CvCity::setHasCorporation()` (install/remove + modifiers) | `Sources/Engine/CvCity.cpp:15226` |
| `CvCity::applyCorporationModifiers()` (building disable/enable) | `Sources/Engine/CvCity.cpp:15193` |
| `CvCity::updateCorporationBonus()` (resource transform) | `Sources/Engine/CvCity.cpp:12691` |
| `CvCity::calculateCorporationMaintenanceTimes100(corp)` | `Sources/Engine/CvCity.cpp:7817` |
| `CvCity::calcCorporateMaintenance()` (Advanced, pop-weighted) | `Sources/Engine/CvCity.cpp:21754` |
| `CvGame::doFoundCorporation()` (auto-founder) | `Sources/Engine/CvGame.cpp:11131` |
| `CvGame::setHeadquarters()` (HQ assign + building install) | `Sources/Engine/CvGame.cpp:5619` |
| `CvPlayer::foundCorporation()` (manual HQ placement) | `Sources/Engine/CvPlayer.cpp:8893` |
| `CvPlayer::isActiveCorporation()` (player dormancy gate) | `Sources/Engine/CvPlayer.cpp:14031` |
| `CvUnit::canSpreadCorporation()` / `spreadCorporation()` / `spreadCorporationCost()` | `Sources/Engine/CvUnit.cpp:8552 / 8695 / 8669` |
| `CvUnitAI::AI_spreadCorporation()` (AI mission dispatch) | `Sources/AI/CvUnitAI.cpp:13737` |
| `CvPlayerAI::AI_corporationValue()` (AI value score) | `Sources/AI/CvPlayerAI.cpp:12469` |
| `GAMEOPTION_ADVANCED_REALISTIC_CORPORATIONS` | `Sources/Defines/CvEnums.h:868` |
| `CvCorporationInfo` (`getPrereqBonuses`/`getBonusProduced`/`getSpread`/`getMaintenance`) | `Sources/Infos/CvCorporationInfo.h` |
| `/cities`, `/players`, `/units` snapshot walks (add fields here) | `Sources/Tools/CvHttpServer.cpp` |

---

## See also
- [`README.md`](README.md) — the observability scaffold: the 0–5 scale ([DEC-obs-scale]), the Orwell bar,
  and the three canonical hook shapes ([DEC-obs-hook-shapes]) this map's hooks instantiate.
- [`http-server.md`](http-server.md) — the live surface (`/cities`, `/players`, `/units`, `/diagnostic`)
  these hooks extend, and the live-read rules (logs held open mid-session).
- [`gold-maintenance-inflation.md`](gold-maintenance-inflation.md) — corporation maintenance is a component
  of the gold/maintenance ledger mapped there; isolating it (§4b `[CORP/maint]`) sharpens that map.
- [`../../explanation/cascade-architecture.md`](../../explanation/cascade-architecture.md) — why total
  observability is load-bearing: the corporation state cannot be shadowed/cut until it is on the wire
  ([DEC-map-before-delete]).
- [`../../README.md`](../../README.md) — the comprehension map / overview-of-overviews.

[DEC-obs-scale]: ../../architecture/decisions.md#dec-obs-scale
[DEC-obs-hook-shapes]: ../../architecture/decisions.md#dec-obs-hook-shapes
[DEC-map-before-delete]: ../../architecture/decisions.md#dec-map-before-delete
