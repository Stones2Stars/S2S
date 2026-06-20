# Observability — heritage acquisition & player score

> **Status:** reference (per-system observability map) · **Verified against:** old map (2026-06-18) re-grounded to the reorganized source tree, 2026-06-20
> **Grounding:** `Sources/Engine/CvPlayer.cpp`, `Sources/Engine/CvCity.cpp`, `Sources/Engine/CvUnit.cpp`, `Sources/Engine/CvGame.cpp`,
> `Sources/Engine/CvSelectionGroup.cpp`, `Sources/AI/CvPlayerAI.cpp`, `Sources/AI/CvUnitAI.cpp`, `Sources/Infos/CvHeritageInfo.h`,
> `Sources/Cascade/CvCascadeCondition.{h,cpp}`, `Sources/Tools/CvHttpServer.cpp`, `Assets/Python/.../CvGameUtils.py`. Citations carried from the old map; the
> `Sources/` prefixes are re-grounded (`Cv*` → `Sources/Engine/`, `Cv*AI` → `Sources/AI/`, `CvHeritageInfo`/Info headers → `Sources/Infos/`, `CvCascade*` →
> `Sources/Cascade/`, `CvHttpServer` → `Sources/Tools/`) but the **line numbers were NOT re-verified** against the reorganized tree — treat each as "the named
> function, around this line" and confirm the function, not the integer.
> One-paragraph orientation: this maps heritage (a permanent player-level flag that gates buildings/units and boosts era-tiered commerce) and the player score
> system (a Python-computed weighted sum) — and why the heritage set, its acquisition event, and the score component breakdown are all opaque from outside. Read
> the scaffold [`README.md`](README.md) (the 0–5 scale, the Orwell bar, the three hook shapes) and [`http-server.md`](http-server.md) (the live surface +
> live-read rules) first; this doc does not restate them.

**Tier today: 1 (Telescreen) for heritage; 2 (Informant) for score.** Heritage acquisition is invisible — no endpoint lists a player's heritage set, no event
fires on acquisition, and the unit is silently consumed. Score is exposed as a single composite number with no component breakdown.

---

## 1. How it works

### 1a. Heritage — data model
Heritage (`HeritageTypes`) is a **player-level, empire-scoped, permanent flag** — once acquired it never leaves (no `setHeritage(eType, false)` outside
save-load). The live set is `CvPlayer::m_myHeritage` (`std::vector<HeritageTypes>`), persisted as a name-keyed tagged block (`WRAPPER_WRITE_DECORATED …
"numHeritage"`) at `Sources/Engine/CvPlayer.cpp:20424-20428` (verify).

`CvHeritageInfo` (`Sources/Infos/CvHeritageInfo.h`) defines each type:
- `needLanguage()` — the player must have researched a tech with `isLanguage()==true` before any language-gated heritage (`Sources/Engine/CvPlayer.cpp:30923`, verify).
- `getPrereqTech()` — the tech the **team** must already have (`Sources/Engine/CvPlayer.cpp:30928`, verify).
- `getPrereqOrHeritage()` — one of these heritages must already be held (OR-list; empty = none) (`Sources/Engine/CvPlayer.cpp:30937-30950`, verify).
- `getEraCommerceChanges100()` — a map `EraTypes → CommerceArray (×100)` applied by `processHeritage` as flat commerce boosts. Each era entry stacks; if the
  current era ≥ the key, that entry's values apply. One heritage can give different/diminishing commerce as eras advance (`Sources/Engine/CvPlayer.cpp:30982-31001`,
  verify). The effect is a flat `extraCommerce100` bump on the **player** (not per-city).
- `getPropertyManipulators()` — optional property sources; VALUED by `AI_heritageValue` and processed on the capital in `heritagePropertiesValue`
  (`Sources/AI/CvPlayerAI.cpp:33203`, verify). The valuation and the actual property effect have separate owners — verify the live property-source wiring if it matters for cascade.

There are 113 heritage JSON files in `Assets/Data/heritages/` (folklore animals, primarily) plus an unrelated "UNESCO Heritage Site" building; the JSON
heritage set and the XML Heritage tags are different data planes.

### 1b. Heritage — the acquisition path
**Only one code path adds a heritage in normal play:** a unit with the heritage capability executes `MISSION_HERITAGE`.
1. **AI path** — `CvUnitAI::AI_heritage()` (`Sources/AI/CvUnitAI.cpp:14929`, verify): evaluates heritable types the unit knows (`m_pUnitInfo->getHeritage(iI)`),
   calls `player.canAddHeritage(eTypeX)`, scores each via `player.AI_heritageValue(eTypeX)` (`Sources/AI/CvPlayerAI.cpp:33256`, verify), picks the best target
   city (weighted by travel time), pushes `MISSION_HERITAGE` or `MISSION_MOVE_TO … MISSIONAI_CONSTRUCT` en route. Called from ~14 UNITAI role handlers
   (`Sources/AI/CvUnitAI.cpp:2106, 5824, 5902, 5990, 6282, 6391, 6471, 6508, 6653, 6709, 6733, 10753, 14498`, verify).
2. **Mission execution** — `CvSelectionGroup::startMission` dispatches `MISSION_HERITAGE` to `pLoopUnit->addHeritage(eType)` (`Sources/Engine/CvSelectionGroup.cpp:1747-1754`, verify).
3. **`CvUnit::addHeritage`** (`Sources/Engine/CvUnit.cpp:8909`, verify): guards `canAddHeritage(plot(), eType)` (friendly city plot, `Sources/Engine/CvUnit.cpp:8877-8906`,
   verify); calls `GET_PLAYER(getOwner()).setHeritage(eType, true)`; fires `NotifyEntity(MISSION_HERITAGE)` if visible; then `kill(true, NO_PLAYER, true)` — the unit is **consumed**.
4. **`CvPlayer::setHeritage`** (`Sources/Engine/CvPlayer.cpp:30956`, verify): appends to `m_myHeritage`, calls `processHeritage(eType, 1)`, then
   `clearCanConstructCache(NO_BUILDING, true)` (heritage gates buildings/units, so the build-cache is invalidated).
5. **`processHeritage`** (`Sources/Engine/CvPlayer.cpp:30982`, verify): applies the commerce boosts for all era entries whose era ≤ current era via `changeExtraCommerce100`.

**Heritage as a build/train prereq:** `canConstruct` checks `getPrereqOrHeritage()` on the building (`Sources/Engine/CvPlayer.cpp:6610-6624`, verify);
`canTrain` checks `getPrereqAndHeritage()` and `getPrereqOrHeritage()` on the unit (`Sources/Engine/CvPlayer.cpp:6452-6474`, verify). Both are INSIDE the
`!bTestVisible` block — invisible prereqs (the UI greys when not met).

**Heritage as a cascade atom:** `ATOMDOMAIN_HERITAGE` is a defined atom in the cascade condition evaluator (`Sources/Cascade/CvCascadeCondition.h:32`,
`Sources/Cascade/CvCascadeCondition.cpp:125`, verify); `evaluateAtom` calls `GET_PLAYER(p).hasHeritage((HeritageTypes)a.iType)`, so cascade `requires` can gate
on heritage. For how the atom/enabler model works, see [`../../explanation/cascade-architecture.md`](../../explanation/cascade-architecture.md) — not re-explained here.

### 1c. Heritage — era-tiered commerce
`getHeritageCommerceEraChange` (`Sources/Engine/CvPlayer.cpp:31003`, verify) is called from `CvPlayer::setCurrentEra` (`Sources/Engine/CvPlayer.cpp:12426`,
verify) on era advance: it applies the delta between the new era's commerce total and the old era's, so heritages with declining commerce (e.g.
`HERITAGE_FOLKLORE_AARDVARK`: +40 research in PREHISTORIC, then −10 each subsequent era) auto-adjust as eras tick. Triggered by era change, not per-turn accumulation.

### 1d. Heritage — effect on score (indirect only)
Heritage does NOT directly contribute to `calculateScore`. Score components:
- Population (`getPopScore`) — via `changeTotalPopulation` (`Sources/Engine/CvPlayer.cpp:9282`, verify).
- Land (`getLandScore`) — via `changeTotalLandScored` (`Sources/Engine/CvPlayer.cpp:9318`, verify).
- Tech (`getTechScore`) — via `changeTechScore` at research (`Sources/Engine/CvPlayer.cpp:30892`, verify), where `getScoreValueOfTech(eTech) = 1 + GC.getTechInfo(eTech).getEra()` (`Sources/Engine/CvGameCoreUtils.cpp:225`, verify).
- Wonders (`getWondersScore`) — via `changeWondersScore` at construction (`Sources/Engine/CvCity.cpp:5091`, verify), where `getWonderScore(eBuilding)` = 6 for limited wonders, 1 otherwise (`Sources/Engine/CvGameCoreUtils.cpp:230`, verify).

Heritage affects score only **indirectly** through commerce (more research → faster techs → higher tech score; more culture → faster culture levels).

### 1e. Score — per-frame, not per-turn
`CvGame::updateScore()` (`Sources/Engine/CvGame.cpp:2425`, verify) is called inside `CvGame::update()` (`Sources/Engine/CvGame.cpp:2369`, verify) — the
**frame loop**, not `doTurn` — and re-computes all player scores when `m_bScoreDirty` is true (set by any component change). It calls `calculateScore(false)` per
player, which delegates to Python (`CvGameUtils.py:87`, verify):
```python
score = (SCORE_POPULATION_FACTOR * (popScore + free) / (free + maxPop))
      + (SCORE_LAND_FACTOR       * (landScore + free) / (free + maxLand))
      + (SCORE_TECH_FACTOR       * (techScore + free) / (free + maxTech))
      + (SCORE_WONDER_FACTOR     * (wondersScore + free) / (free + maxWonders))
```
All factors are XML GlobalDefines. Vassal-population/land adjustments are baked into the raw scores before Python sees them (`Sources/Engine/CvPlayer.cpp:11508-11530,
11548-11570`, verify). `updateScore` also calls `updateScoreHistory(getGameTurn(), iBestScore)` (`Sources/Engine/CvGame.cpp:2473`, verify) → `m_mapScoreHistory`,
the per-turn ledger. The `/players` endpoint reads `GC.getGame().getPlayerScore(...)` (`Sources/Tools/CvHttpServer.cpp:1529`, verify) — the last-computed value stored by `setPlayerScore`.

---

## 2. What's on the wire today

**Tier 1 (Telescreen) for heritage; Tier 2 (Informant) for score** (the total is exposed, no component breakdown).

| Observable | Endpoint / log | Notes |
|---|---|---|
| Player total score | `/players` → `score` | Recomputed each frame; ≤5 s stale via snapshot |
| Player era | `/players` → `era` | Context for score normalization (tech factor) |
| Player tech count | `/players` → `techs` | Team-shared, not player-specific |
| Player current research | `/players` → `research` | The tech being researched; not when it completes |
| Player pop / cities / units | `/players` | Raw pop-score inputs; no score breakdown |
| Heritage as build-gate reason | `/diagnostic/canConstruct?type=X&player=N` → `legacyReason:"heritage"` | Says heritage blocked the build; does NOT enumerate which heritages the player has |
| Unit with heritage mission in progress | `/units?playerNumber=N` → `missionAI=MISSIONAI_CONSTRUCT` | The unit en route; `missionAI` is generic CONSTRUCT, not heritage-specific |

**Not observable — heritage:** the current set (`m_myHeritage` — no endpoint, so you can't know which heritages an AI holds); the acquisition event (no log/event
in `setHeritage`/`addHeritage`); which heritage type is missing for a blocked build; the silent `extraCommerce100` bump and its era-transition adjustment;
`m_bHasLanguage`; the `AI_heritageValue` valuation reasoning.

**Not observable — score:** the component breakdown (`popScore`/`landScore`/`techScore`/`wondersScore` — only the composite ships); `m_mapScoreHistory`; the
dirty flag; the normalization inputs (`getMaxPopulation`/`getMaxLand`/`getMaxTech`/`getMaxWonders`, etc. — needed to recompute components); which buildings contributed to `wondersScore`.

---

## 3. The gap

**Heritage (critical for cascade):** Heritage is a `requires`-family gate (`ATOMDOMAIN_HERITAGE` wired). On acquisition the cascade must re-evaluate every
building/unit it gates, yet: no event fires on acquisition (the cascade has no re-eval signal); no endpoint exposes the set (a `/diagnostic/canConstruct` shadow
can detect a mismatch but cannot say which heritage caused it); and the unit-consumption `kill` removes the unit from `/units` silently (an agent sees a unit
vanish without knowing a heritage was gained).

**Score (moderate):** Score is a single number. For cascade verification the component breakdown matters — if building-placement logic is replaced, we must
verify the cascade's building set yields the same `wondersScore` and tech-acquisition rate as the legacy system. Without the breakdown, a change in one
component can mask a regression in another.

---

## 4. Proposed hooks

All hooks are one of the three canonical shapes — see [DEC-obs-hook-shapes](../../architecture/decisions.md#dec-obs-hook-shapes).

### Hook A — `[PLR/heritage]` acquisition event (level 1)
In `CvPlayer::setHeritage()` (`Sources/Engine/CvPlayer.cpp:30967`, verify, just after `m_myHeritage.push_back(eType)`), gated `gPlayerLogLevel >= 1`:
```
[PLR/heritage] turn=N player=N heritage=HERITAGE_FOLKLORE_AARDVARK total=N
```
`heritage = GC.getHeritageInfo(eType).getType()`; `total = m_myHeritage.size()` after the push. The primary event hook — every acquisition visible on `/events` the turn it happens, for every player including AI.

### Hook B — `/players` `heritages` array (highest priority)
In `PlayerSnap` (`Sources/Tools/CvHttpServer.cpp:61`, verify) add `std::vector<CvString> heritageKeys`; in the snapshot builder
(`Sources/Tools/CvHttpServer.cpp:1524`, verify) iterate `kPlayer.getHeritage()` pushing `GC.getHeritageInfo(eType).getType()`:
```json
"heritages": ["HERITAGE_FOLKLORE_AARDVARK", "HERITAGE_FOLKLORE_BEAVER", ...]
```
The snapshot twin of Hook A — reconstruct the full set for any player from one GET, and let the cascade shadow compare its derived buildable set against `canConstruct`.

### Hook C — `[PLR/heritage/value]` AI valuation trace (level 2, forensic)
In `CvUnitAI::getBestHeritageValue()` (`Sources/AI/CvUnitAI.cpp:14904`, verify) where `iValue = player.AI_heritageValue(eTypeX)`, gated `gUnitLogLevel >= 2`:
```
[PLR/heritage/value] turn=N player=N unit=N heritage=HERITAGE_X value=N weighted=N city=<name> pathTurns=N
```
Makes the AI's heritage targeting visible — currently entirely silent.

### Hook D — `/players` score component breakdown
In `PlayerSnap` (`Sources/Tools/CvHttpServer.cpp:61`, verify) add `iPopScore`, `iLandScore`, `iTechScore`, `iWondersScore` from
`getPopScore()`/`getLandScore()`/`getTechScore()`/`getWondersScore()`:
```json
"popScore": <int>, "landScore": <int>, "techScore": <int>, "wondersScore": <int>
```
The raw components before normalization; combined with `score`, they let an agent verify the formula and detect component-level regressions. Four small int reads.

### Hook E — `[PLR/score]` per-turn score snapshot (level 1)
In `CvGame::updateScore()` after `setPlayerScore` (`Sources/Engine/CvGame.cpp:2472-2473`, verify), gated `gPlayerLogLevel >= 1`:
```
[PLR/score] turn=N player=N score=N popScore=N landScore=N techScore=N wondersScore=N
```
One line per alive player per score-update cycle. `updateScore` runs only when dirty, so volume is lower than per-turn city logs.

### Hook F — `hasLanguage` + score normalization constants (smaller)
Add `"hasLanguage": <bool>` (`kPlayer.isHasLanguage()`) to `/players` (needed to understand heritage prereq eligibility for AI players); expose the game-global
normalization constants (`maxPopulation`/`maxLand`/`maxTech`/`maxWonders`) on a `/diagnostic/scoreConstants` endpoint (static after init — a one-shot query, not per-player).

**Priority:** **Highest** — B (`heritages` array; unblocks the cascade shadow for heritage-gated buildings) and A (acquisition event; the cascade re-eval
signal, low volume). **High** — D (score component breakdown). **Medium** — E (per-turn score snapshot) and F (`hasLanguage` + constants). **Low** — C (forensic AI valuation trace).

---

## 5. Cascade relevance (#428/#430)

- **Heritage as a cascade prerequisite (`ATOMDOMAIN_HERITAGE`):** the cascade already gates `requires` on `hasHeritage(eType)`
  (`Sources/Cascade/CvCascadeCondition.cpp:125`, verify). The gap is purely observability — without Hook B (the `heritages` array), a shadow can't explain why
  a `canConstruct` call disagrees without reverse-engineering the set from `legacyReason:"heritage"`. The atom/enabler model itself is in
  [`../../explanation/cascade-architecture.md`](../../explanation/cascade-architecture.md), not restated here.
- **Acquisition as a re-eval signal:** `setHeritage` invalidates `clearCanConstructCache`; the cascade shadow (the `placementSweep` pattern) should re-trigger
  on the same event. Hook A provides it.
- **Score as a verification metric (not a cascade target):** no §14 H state maintainer removes score, but `wondersScore`/`techScore` are downstream effects of
  what the cascade places. A clean cascade run should reproduce the legacy score trajectory; the component breakdown (Hook D) is the minimal substrate for that comparison.
- **Heritage → commerce → score path:** heritage boosts empire-wide `extraCommerce100` → research → tech acquisition → `techScore`. This indirect path is
  invisible today; Hook D exposes `techScore` directly, making the end-to-end effect measurable without tracing every modifier.
- All hooks honour [DEC-map-before-delete](../../architecture/decisions.md#dec-map-before-delete): observe before any legacy maintainer is cut.

---

## See also
- [`README.md`](README.md) — the observability scale, the Orwell bar, and the three hook shapes this map applies.
- [`http-server.md`](http-server.md) — the live surface (`/players`, `/units`, `/diagnostic/*`) these hooks extend, and the live-read rules.
- [`../../architecture/decisions.md`](../../architecture/decisions.md) — [DEC-obs-scale], [DEC-obs-hook-shapes], [DEC-map-before-delete].
- [`../../explanation/cascade-architecture.md`](../../explanation/cascade-architecture.md) — the `ATOMDOMAIN_HERITAGE` atom / enabler model heritage gates plug into (the cascade design, not re-explained in this map).
- [`golden-ages-era.md`](golden-ages-era.md) — heritage commerce era-deltas ride the same `setCurrentEra` era-advance path.
- [`../../README.md`](../../README.md) — the comprehension map (overview-of-overviews).
