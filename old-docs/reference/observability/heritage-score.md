# Observability map — Heritage acquisition & Score

> DRAFT observability map (2026-06-18, parent: cascade-mapping-inventory.md) — all claims cited
> from live code; verify before relying. Line numbers are anchors at time of writing; they drift.

---

## 1. How it actually works

### 1a. Heritage system — data model

Heritage (`HeritageTypes`) is a **player-level, empire-scoped, permanent flag** — once acquired it
never leaves (there is no `setHeritage(eType, false)` call outside save-load). The live set is
`CvPlayer::m_myHeritage` (`std::vector<HeritageTypes>`), persisted as a name-keyed tagged block
(`WRAPPER_WRITE_DECORATED … "numHeritage"`) at `CvPlayer.cpp:20424-20428`.

`CvHeritageInfo` (`Sources/CvHeritageInfo.h`) defines each heritage type:
- `needLanguage()` — boolean prereq: the player must have researched a tech with `isLanguage()==true`
  before any language-gated heritage can be acquired (`CvPlayer.cpp:30923`).
- `getPrereqTech()` — the tech the **team** must already have (`CvPlayer.cpp:30928`).
- `getPrereqOrHeritage()` — one of these heritages must already be held (OR-list; empty = none
  required) (`CvPlayer.cpp:30937-30950`).
- `getEraCommerceChanges100()` — a map of `EraTypes → CommerceArray (×100)` applied by
  `processHeritage` as flat commerce boosts. Each era entry stacks: if the current era ≥ the key,
  that entry's values are applied. A single heritage can therefore give different or diminishing
  commerce as eras advance (`CvPlayer.cpp:30982-31001`). The actual effect: flat `extraCommerce100`
  bumps on the **player** (not per-city).
- `getPropertyManipulators()` — optional property sources (constant per-turn additions to a
  property); evaluated by `AI_heritageValue` for the AI but also processed on the capital in
  `heritagePropertiesValue` (`CvPlayerAI.cpp:33203`). Note: these manipulators are VALUED by the
  AI but the actual property EFFECT is owned by what calls `processHeritage` — verify the live
  property-source wiring if this matters for cascade.

There are 113 heritage JSON files in `Assets/Data/heritages/` (folklore animals, primarily) plus an
unrelated "UNESCO Heritage Site" building; the JSON heritage set and the XML Heritage tags are
different data planes.

### 1b. Heritage acquisition — the trigger path

**Only one code path adds a heritage in normal play:** a unit with the heritage capability
executes `MISSION_HERITAGE`.

1. **AI path** — `CvUnitAI::AI_heritage()` (`CvUnitAI.cpp:14929`): evaluates all heritable types
   the unit knows (`m_pUnitInfo->getHeritage(iI)`), calls `player.canAddHeritage(eTypeX)`, scores
   each via `player.AI_heritageValue(eTypeX)` (`CvPlayerAI.cpp:33256`), picks the best city to
   target (weighted by travel time), pushes `MISSION_HERITAGE` or `MISSION_MOVE_TO … MISSIONAI_CONSTRUCT`
   en-route. `AI_heritage()` is called from at least 14 different UNITAI role handlers
   (`CvUnitAI.cpp:2106, 5824, 5902, 5990, 6282, 6391, 6471, 6508, 6653, 6709, 6733, 10753, 14498`).

2. **Mission execution** — `CvSelectionGroup::startMission` dispatches `MISSION_HERITAGE` to
   `pLoopUnit->addHeritage(eType)` (`CvSelectionGroup.cpp:1747-1754`).

3. **`CvUnit::addHeritage`** (`CvUnit.cpp:8909`):
   - Guards: `canAddHeritage(plot(), eType)` (unit must be in a friendly city plot,
     `CvUnit.cpp:8877-8906`).
   - Calls `GET_PLAYER(getOwner()).setHeritage(eType, true)`.
   - Fires `NotifyEntity(MISSION_HERITAGE)` if the plot is actively visible.
   - Then `kill(true, NO_PLAYER, true)` — the unit is **consumed** on acquisition.

4. **`CvPlayer::setHeritage`** (`CvPlayer.cpp:30956`):
   - Appends to `m_myHeritage`, then calls `processHeritage(eType, 1)`.
   - Calls `clearCanConstructCache(NO_BUILDING, true)` — heritage gates buildings/units so the
     build-cache must be invalidated.

5. **`processHeritage`** (`CvPlayer.cpp:30982`): applies the commerce boosts for all era entries
   whose era index ≤ current era via `changeExtraCommerce100`.

**Heritage as a building/unit prerequisite**: `canConstruct` checks `getPrereqOrHeritage()` on the
building (`CvPlayer.cpp:6610-6624`); `canTrain` checks both `getPrereqAndHeritage()` and
`getPrereqOrHeritage()` on the unit (`CvPlayer.cpp:6452-6474`). Both checks are INSIDE the
`!bTestVisible` block — they are invisible prereqs (UI hides them when not met, per `bTestVisible`
= false for the real gate, true for the greying-only pass).

**Heritage as a cascade atom**: `ATOMDOMAIN_HERITAGE` is a defined atom in the cascade condition
evaluator (`CvCascadeCondition.h:32`, `CvCascadeCondition.cpp:125`). `evaluateAtom` calls
`GET_PLAYER(p).hasHeritage((HeritageTypes)a.iType)` — so cascade `requires` can gate on heritage
already.

### 1c. Heritage effects on commerce (era-tiered)

The `getHeritageCommerceEraChange` method (`CvPlayer.cpp:31003`) is called from `CvPlayer::setCurrentEra`
(`CvPlayer.cpp:12426`) when the player advances an era. At that point the delta between the new
era's commerce total and the old era's is applied, so heritages that have declining research commerce
(like `HERITAGE_FOLKLORE_AARDVARK`: +40 research in PREHISTORIC, then −10 each subsequent era)
automatically adjust as eras tick. This adjustment is triggered by era change, not by per-turn
accumulation.

### 1d. Heritage effects on score

Heritage does NOT directly contribute to `calculateScore`. Score components are:
- Population (`getPopScore`) — incremented by `changeTotalPopulation` (`CvPlayer.cpp:9282`)
- Land (`getLandScore`) — incremented by `changeTotalLandScored` (`CvPlayer.cpp:9318`)
- Tech (`getTechScore`) — incremented by `changeTechScore` at tech research (`CvPlayer.cpp:30892`),
  where `getScoreValueOfTech(eTech) = 1 + GC.getTechInfo(eTech).getEra()` (`CvGameCoreUtils.cpp:225`)
- Wonders (`getWondersScore`) — incremented by `changeWondersScore` at building construction
  (`CvCity.cpp:5091`), where `getWonderScore(eBuilding)` = 6 for limited wonders, 1 otherwise
  (`CvGameCoreUtils.cpp:230`)

Heritage indirectly affects score through commerce (more research = faster techs = higher tech score;
more culture = faster culture levels).

### 1e. Score computation — per-frame, not per-turn

`CvGame::updateScore()` (`CvGame.cpp:2425`) is called inside `CvGame::update()` (`CvGame.cpp:2369`)
— the **frame loop**, not `doTurn`. It re-computes all player scores whenever `m_bScoreDirty` is
true. Score is dirty whenever any component changes (population, land, tech, wonders — each
calls `GC.getGame().setScoreDirty(true)`).

`updateScore` calls `calculateScore(false)` per player. `calculateScore` delegates to Python
(`CvGameUtils.py:87`): it computes a weighted sum of four components, each normalized to the
game's theoretical maximum:

```python
score = (SCORE_POPULATION_FACTOR * (popScore + free) / (free + maxPop))
      + (SCORE_LAND_FACTOR     * (landScore + free) / (free + maxLand))
      + (SCORE_TECH_FACTOR     * (techScore + free) / (free + maxTech))
      + (SCORE_WONDER_FACTOR   * (wondersScore + free) / (free + maxWonders))
```

All factors are XML GlobalDefines. The vassal-population and vassal-land adjustments in `getPopScore`
/ `getLandScore` are baked into the raw scores before Python sees them (`CvPlayer.cpp:11508-11530`,
`11548-11570`).

`updateScore` also calls `GET_PLAYER(eBestPlayer).updateScoreHistory(getGameTurn(), iBestScore)`
(`CvGame.cpp:2473`) — which writes to `m_mapScoreHistory`, the per-turn score ledger.

The `/players` endpoint reads `GC.getGame().getPlayerScore((PlayerTypes)iI)` (`CvHttpServer.cpp:1529`),
which is the last-computed score stored by `setPlayerScore` in `updateScore`.

---

## 2. Current observability

**Overall tier: 1 (Telescreen)** for heritage. **Tier 2 (Informant)** for score (the total is exposed
but no component breakdown).

### What IS observable today

| Observable | Endpoint / log | Notes |
|---|---|---|
| Player total score | `GET /players` → `score` field | Recomputed each frame; ≤5s stale via snapshot |
| Player era | `GET /players` → `era` | Context for score normalization (tech factor) |
| Player tech count | `GET /players` → `techs` | Team-shared, not player-specific; component of tech score |
| Player current research | `GET /players` → `research` | The tech being researched; not when it will complete |
| Player pop, cities, units | `GET /players` | Raw inputs to pop-score; no score breakdown |
| Heritage as build-gate reason | `GET /diagnostic/canConstruct?type=X&player=N` → `legacyReason: "heritage"` | Tells you heritage is why a building is blocked; does NOT enumerate which heritages the player has |
| Unit with heritage mission in progress | `GET /units?playerNumber=N` → `missionAI=MISSIONAI_CONSTRUCT, activity=…` | The unit en route; missionAI is MISSIONAI_CONSTRUCT not a heritage-specific tag (see §1b step 1) |

### What is NOT observable today

**Heritage system:**

| Gap | What is missing |
|---|---|
| The current heritage set | No endpoint lists `m_myHeritage`; you cannot know which heritages a player (esp. AI) holds |
| Heritage acquisition event | No log line or event fires in `setHeritage` or `addHeritage`; acquisition is invisible until the next snapshot (and even then you can only infer it indirectly) |
| Heritage prereq check detail | `legacyReason:"heritage"` tells you heritage gated the build; it does NOT say which heritage type is missing |
| Commerce effect of heritages | The `extraCommerce100` bump is applied silently; the total is reflected in city commerce rates but the heritage contribution is unattributed |
| Era-transition commerce adjustment | When a player advances an era, `getHeritageCommerceEraChange` adjusts commerce; no log or event marks this |
| `m_bHasLanguage` flag | Whether the player has unlocked the language prerequisite for folklore heritages is unexposed |
| AI heritage valuation | `AI_heritageValue` runs silently — no `[UNT]` or `[DAI]` line; you cannot see why the AI chose or skipped a heritage target |

**Score system:**

| Gap | What is missing |
|---|---|
| Score component breakdown | `/players` returns only `score` (the composite); `popScore`, `landScore`, `techScore`, `wondersScore` are all invisible |
| Score history | `m_mapScoreHistory` (per-turn score ledger) is not exposed; you cannot reconstruct score trends without re-polling every turn |
| Score dirty flag | You cannot tell from outside whether the score is stale or freshly recomputed |
| Score normalization inputs | `getMaxPopulation`, `getMaxLand`, `getMaxTech`, `getMaxWonders`, `getInitPopulation`, etc. are game-global constants but unexposed; you cannot recompute the score components without them |
| Wonder score per building | Which buildings contributed to `wondersScore` and when is not logged |

---

## 3. The gap

**Heritage gap (critical for cascade):** Heritage is a `requires`-family gate in the cascade
(`ATOMDOMAIN_HERITAGE` already wired), meaning the cascade evaluates whether a building/unit is
unlocked by checking `hasHeritage`. If a heritage is acquired, the cascade must be notified so it
can re-evaluate all buildings/units it gates. Currently:
- No event fires on acquisition — the cascade has no signal to re-evaluate.
- No endpoint exposes the current heritage set — a shadow (`/diagnostic/canConstruct`) can detect
  a mismatch but cannot explain which heritage caused it without the set.
- The unit consumption (`kill` on `addHeritage`) removes the unit from `/units` silently — an
  agent watching the wire sees a unit disappear without knowing a heritage was gained.

**Score gap (moderate):** Score is exposed as a single number. For the cascade's purpose the
component breakdown matters: if we replace building-placement logic, we need to verify that the
cascade's building set produces the same `wondersScore` and tech-acquisition rate as the legacy
system. Without the component breakdown, a change in one component can mask a regression in
another.

---

## 4. Proposed hooks

All hooks follow the three canonical observability hook shapes — see [DEC-obs-hook-shapes](../../decisions.md#dec-obs-hook-shapes).

### Hook A — `[PLR/heritage]` acquisition event (level 1 headline)

**Where:** `CvPlayer::setHeritage()` at `CvPlayer.cpp:30967` (inside the `if (bNewValue)` /
`itr == m_myHeritage.end()` block, just after `m_myHeritage.push_back(eType)`), gated by
`gPlayerLogLevel >= 1`.

**What to emit:**
```
[PLR/heritage] turn=N player=N heritage=HERITAGE_FOLKLORE_AARDVARK total=N
```
Fields:
- `heritage` — `GC.getHeritageInfo(eType).getType()` (the XML key).
- `total` — `m_myHeritage.size()` after the push (cumulative count for the player).

This is the primary event hook. It makes every heritage acquisition visible on `/events` the turn
it happens, for every player including AI.

### Hook B — `/players` endpoint: `heritages` array field

**Where:** `PlayerSnap` struct (`CvHttpServer.cpp:61`) — add `std::vector<CvString> heritageKeys`.
Snapshot builder (`CvHttpServer.cpp:1524`): iterate `kPlayer.getHeritage()`, push
`GC.getHeritageInfo(eType).getType()` for each.

**What to add to `/players` JSON output:**
```json
"heritages": ["HERITAGE_FOLKLORE_AARDVARK", "HERITAGE_FOLKLORE_BEAVER", ...]
```

This is the snapshot twin of Hook A. It lets an agent reconstruct the full heritage set for any
player (including AI) with a single GET, without depending on the log stream. It also lets the
cascade shadow compare its derived "what buildings should this player be able to build" against
the actual `canConstruct` result.

### Hook C — `[PLR/heritage/value]` AI valuation trace (level 2)

**Where:** `CvUnitAI::getBestHeritageValue()` (`CvUnitAI.cpp:14904`), at the point where
`iValue = player.AI_heritageValue(eTypeX)` is computed, gated by `gUnitLogLevel >= 2`.

**What to emit:**
```
[PLR/heritage/value] turn=N player=N unit=N heritage=HERITAGE_X value=N weighted=N city=<name> pathTurns=N
```
Fields match the local variables: `iValue` (raw heritage value), `iWeightedValue` (distance-adjusted),
`cityX->getName()` (target city), `iPathTurns`.

This makes the AI's heritage targeting decision visible — currently the entire reasoning is silent.

### Hook D — `/players` score component breakdown

**Where:** `PlayerSnap` struct (`CvHttpServer.cpp:61`) — add four int fields: `iPopScore`,
`iLandScore`, `iTechScore`, `iWondersScore`. Snapshot builder: `kPlayer.getPopScore()`,
`kPlayer.getLandScore()`, `kPlayer.getTechScore()`, `kPlayer.getWondersScore()`.

**What to add to `/players` JSON output:**
```json
"popScore":     <int>,
"landScore":    <int>,
"techScore":    <int>,
"wondersScore": <int>
```

These are the raw components before Python normalization. Combined with the existing `score` field
they let an agent verify the formula and detect component-level regressions. Four small int reads
in the snapshot loop.

### Hook E — `[PLR/score]` per-turn score snapshot (level 1)

**Where:** `CvGame::updateScore()` at `CvGame.cpp:2472-2473`, after `setPlayerScore`, gated by
`gPlayerLogLevel >= 1`.

**What to emit:**
```
[PLR/score] turn=N player=N score=N popScore=N landScore=N techScore=N wondersScore=N
```

One line per alive player per score-update cycle. Because `updateScore` runs only when dirty (not
every frame), this is lower volume than per-turn city logs. It makes the score evolution visible on
the `/events` stream without polling `/players`.

### Hook F — `/players` `hasLanguage` field and score normalization constants

**Smaller additions:**

- Add `"hasLanguage": <bool>` to `/players` — exposes `kPlayer.isHasLanguage()` (needed to
  understand heritage prereq eligibility for AI players).
- Expose the game-global score normalization constants (`maxPopulation`, `maxLand`, `maxTech`,
  `maxWonders`) on a `GET /diagnostic/scoreConstants` endpoint or as additional fields on a
  `GET /players` root object. These are static after game init so they need not be in the per-player
  snapshot; a one-shot query is enough.

---

## 5. Priority ranking

| Priority | Hook | Why |
|---|---|---|
| **Highest** | B — `/players` `heritages` array | Snapshot-queryable; exposes the full heritage set for any player; unblocks cascade shadow for heritage-gated buildings; critical gap for AI-player observability |
| **Highest** | A — `[PLR/heritage]` acquisition event | Makes the acquisition visible on `/events`; needed for cascade re-evaluation signal; low volume (one line per rare acquisition) |
| **High** | D — `/players` score component breakdown | Enables per-component regression detection; small cost; needed to verify cascade's effect on wonders/tech score |
| **Medium** | E — `[PLR/score]` per-turn snapshot | Tracks score evolution on the log stream without polling; one line per dirty-update per player |
| **Medium** | F — `hasLanguage` + score normalization constants | Completes the heritage prereq picture; score constants are small one-time data |
| **Low** | C — `[PLR/heritage/value]` AI valuation trace | Forensic; level-2; useful for diagnosing AI heritage acquisition bugs but not needed for routine coverage |

---

## 6. Cascade relevance (#428/#430)

**Heritage as a cascade prerequisite (`ATOMDOMAIN_HERITAGE`):** The cascade condition evaluator
already has `ATOMDOMAIN_HERITAGE` wired (`CvCascadeCondition.cpp:125`). When a building or unit
JSON declares a heritage prerequisite in `requires`, the cascade correctly gates on
`hasHeritage(eType)`. The gap is observability: without Hook B (the `/players` `heritages` array),
a shadow cannot explain WHY a `canConstruct` call disagrees with the cascade without reverse-
engineering the heritage set from `legacyReason:"heritage"` alone.

**Heritage acquisition event:** The cascade's shadow testing framework (the `placementSweep`
pattern) depends on knowing when player state changes so it can re-evaluate. Heritage acquisition
(`setHeritage`) invalidates `clearCanConstructCache` — the cascade shadow should likewise be re-
triggered. Hook A provides the event signal.

**Score as a cascade verification metric:** The score system is not itself a cascade target
(no §14 H state maintainer removes it), but `wondersScore` and `techScore` are downstream effects
of what buildings and techs the cascade places. A clean cascade run should produce the same score
trajectory as the legacy system; the component breakdown (Hook D) is the minimal substrate for
that comparison.

**Heritage → commerce → score path:** Heritage boosts empire-wide `extraCommerce100`. Commerce
feeds research rate → tech acquisition → `techScore`. This indirect path is currently invisible;
Hook D exposes `techScore` directly, making the end-to-end effect measurable without tracing
every commerce modifier.
