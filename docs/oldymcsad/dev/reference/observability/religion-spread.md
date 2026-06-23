# Observability — Religion spread — what's on the wire for propagation, decay, and missionaries

> **Status:** reference   ·   **Verified against:** 2026-06-20 (function sites confirmed in `Sources/Engine/CvCity.cpp`, `Sources/Engine/CvUnit.cpp`, `Sources/Engine/CvGame.cpp`; line numbers drift)
> **Grounding:** live `Sources/Engine/CvCity.cpp` (`doReligion`, `setHasReligion`), `Sources/Engine/CvGame.cpp` (`setHolyCity`, `calculateReligionPercent`), `Sources/Engine/CvUnit.cpp` (`spread`/`canSpread`), `Sources/AI/CvUnitAI.cpp` (`AI_spreadReligion`), `Sources/Tools/CvHttpServer.cpp` (`/cities`, `/players`), `Assets/Python/CvEventManager.py`. Carried over from the old draft map; function sites re-confirmed, paths re-grounded to the reorganized `Sources/` tree.
> Religion spread is **Tier 1 (Telescreen)** today, and effectively dark: the coarse `/players` / `/cities` / `/units` snapshots carry **zero** religion state. Which religion a city has, the holy-city identity, per-city influence weights, player state religion, the passive spread/decay odds, missionary attempts and AI targeting are all computed inside `doTurn` and emitted nowhere. There are no `[REL]`-tagged log lines, no SSE events (the Python `religionSpread`/`religionRemove` events are commented out), and no diagnostic endpoints. This map walks passive spread → decay → holy-city influence → missionary spread → AI targeting, names what's dark, and proposes the snapshot fields / log tags / diagnostic endpoint to climb to Tier 3/4.

The observability scale (0–5) and the three canonical hook shapes are defined once in
[`README.md`](README.md) ([DEC-obs-scale], [DEC-obs-hook-shapes]); the live surface and the rules for
reading it (logs held open mid-session, use `/events` + `/diagnostic`) live in
[`http-server.md`](http-server.md). This doc does not restate them.

> Line numbers below are anchors at time of writing and **drift** — confirm the named function, not the
> integer.

---

## 1. How it actually works

### 1a. Per-turn passive spread — `CvCity::doReligion`

`CvCity::doReligion` (CvCity.cpp:16708) runs once per city per turn from `CvCity::doTurn`
(CvCity.cpp:1349) inside a `PERF_SCOPE` block. Two game-options gate its two branches:

| Option | Enum | Effect when on |
|---|---|---|
| Religion Decay | `MODDERGAMEOPTION_RELIGION_DECAY` | religions can leave cities |
| Multiple Religion Spread | `MODDERGAMEOPTION_MULTIPLE_RELIGION_SPREAD` | a city with religions can still receive more |

**Iteration order:** the function walks religion types from a *random* offset —
`iReligionX = GAME.getSorenRandNum(iNumReligions, "Random start index")` (CvCity.cpp:16719) — and `break`s
once a spread or decay fires, so only **one** religion is considered per call, in a per-turn-random order.

**Spread branch** (CvCity.cpp:16792) — fires when the city does NOT have that religion AND
(`bMultRelSpread` OR `iReligionCount == 0`):

1. **State-religion gate:** if the owner has `isNoNonStateReligionSpread()` (CvCity.cpp:16794, civic-set),
   only the state religion may spread in.
2. **Influence accumulation:** walks all living players' cities (CvCity.cpp:16799). Each source `cityX` must
   equal `this` OR be trade-connected (`isConnectedTo`). `iSpread = cityX->getReligionInfluence(eReligionX)`
   (starts 0; the holy city gets `GC.getHOLY_CITY_INFLUENCE()` via `setHolyCity`; buildings contribute via
   `processBuilding` `changeReligionInfluence`). Multiplied by `iSpreadFactor`
   (`CvReligionInfo::getSpreadFactor()`, ≥1). Foreign-city distance penalty:
   `iSpread /= (iReligionCount+1) * max(1, RELIGION_SPREAD_DISTANCE_DIVISOR * dist/maxDist - 5)`; the local
   path (`cityX == this`) is instead `2*iSpread/(iReligionCount+1)`. `iRandThreshold = max(iRandThreshold,
   iSpread)` — best single source, not a sum.
3. **Player modifier:** `iRandThreshold *= max(1, getModifiedIntValue(100, owner.getReligionSpreadRate())) / 100`
   (civic-sourced).
4. **Roll:** `getSorenRandNum(RELIGION_SPREAD_RAND * gameSpeedPercent / 100, "Religion Spread")`. If
   `roll < threshold`: `setHasReligion(eReligionX, true, true, true)`.

**Decay branch** (CvCity.cpp:16729) — fires when the city HAS the religion AND `bReligionDecay`:
- Exempt: state religion, the holy city itself, or only one religion present.
- `iDecay = getSpreadFactor() + (iReligionCount - 2)^2`; mitigated by connected own-empire cities with the
  religion, by this city's own influence, and modified by holy-city ownership (own owner → half; at war with
  holy city → ×4/3). Same `RELIGION_SPREAD_RAND * gameSpeedPercent` roll. On fire:
  `setHasReligion(eReligionX, false, true, false)` and all `getPrereqReligion() == eReligionX` buildings are
  forcibly removed.

### 1b. `setHasReligion` side-effects — `CvCity::setHasReligion` (CvCity.cpp:15008)

Every gain/loss triggers: `GET_PLAYER(getOwner()).changeHasReligionCount(eIndex, ±1)`;
`FlushCanConstructCache()`; the Python event `religionSpread`/`religionRemove`;
`applyReligionModifiers`; `checkReligiousDisablingAllBuildings()` (dormancy check). A human-UI-only
`AddDLLMessage` is sent to the city owner / state-religion sharers / holy-city owner who can see the city
(CvCity.cpp:15044) — AI players get no announce, and it is not an event/log line.

### 1c. Holy-city influence — `CvGame::setHolyCity` (CvGame.cpp:5510)

Moves `GC.getHOLY_CITY_INFLUENCE()` off the old holy city and onto the new one via
`changeReligionInfluence`. A city's `m_paiReligionInfluence[eIndex]` is the spread-source weight for all
passive rolls; saved via `WRAPPER_READ/WRITE_CLASS_ARRAY ... REMAPPED_CLASS_TYPE_RELIGIONS`.
`GC.getHOLY_CITY_INFLUENCE()` is an XML define — confirm the value if it matters.

### 1d. Missionary spread — `CvUnit::spread` (CvUnit.cpp:8481)

Triggered by a missionary executing `MISSION_SPREAD` on a city plot. `canSpread` (CvUnit.cpp:8426): religion
founded, city lacks it, unit has `getReligionSpreads(eReligion) > 0`, entry allowed; non-state religions
blocked by the target owner's `isNoNonStateReligionSpread()` (CvUnit.cpp:8448, same gate as passive); a
"Divine prophet" mode (`GAMEOPTION_RELIGION_DIVINE_PROPHETS`) adds tech-timing constraints.

- Unfounded religion (divine-prophet path): the unit founds it (`setHolyCity`, always succeeds,
  CvUnit.cpp:8532).
- Founded (normal path): `iSpreadProb = getReligionSpreads(eReligion)` (CvUnit.cpp:8493); +state/non-state
  spread modifiers; ÷2 into a foreign-team city; `+= (numReligions - cityReligionCount) * (100 - iSpreadProb)
  / numReligions` (empty-slot bonus). Roll: `getSorenRandNum(100) < iSpreadProb`. The Python event
  `unitSpreadReligionAttempt(unit, religion, bSuccess)` fires regardless (CvUnit.cpp:8510). Failure → human-UI
  `AddDLLMessage` only; success → `pCity->setHasReligion(eReligion, true, true, false)`. The missionary is
  always killed afterward.

### 1e. AI missionary decision — `CvUnitAI::AI_missionaryMove` / `AI_spreadReligion`

`AI_missionaryMove` (CvUnitAI.cpp:5584) calls `AI_spreadReligion()` (CvUnitAI.cpp:13269): pick the religion
(state preferred), build a `CvReachablePlotSet`, score candidate cities by existing mission targeting,
culture-victory strategy, holy-city ownership, target relationship, and number of cities without the
religion, then push `MISSION_SPREAD` or a move-toward mission. No logging — none of this AI evaluation is
tagged or streamed.

### 1f. Religiously-limited buildings (downstream)

`setReligiouslyLimitedBuilding` / `m_pabReligiouslyDisabledBuilding`: a building is disabled when the owner's
state religion does not match the building's required religion (unless a `hasAllReligionsActive` waiver
applies). This is a dormancy trigger driven by religion state — downstream of spread, not spread itself; its
shadow is tracked with the building-dormancy work, not here.

---

## 2. What's on the wire today — **Tier 1 (Telescreen)**

### What IS reachable

| Surface | What you can see |
|---|---|
| `/players` snapshot | score, era, techs, gold, cities, units, production, research. **No religion fields.** |
| `/cities` snapshot | position, population, yields, building count, culture level, capital, crime/education/disease. **No religion fields.** |
| `/units` snapshot | A missionary is identifiable by `type=UNIT_*MISSIONARY*` + `unitAI=UNITAI_MISSIONARY`; its target is in `missionAI` (`MISSIONAI_SPREAD`). |
| `/diagnostic/canConstruct` `legacyReason` | `"prereqReligion"` / `"prereqStateReligion"` / `"stateReligionInCity"` / `"holyCity"` when a building is religion-blocked. **The block reason is exposed; the city's actual religion set is not.** |
| Python `religionFounded` | Fires + logs to the autolog (`autologEventManager.py`). |
| Python `unitSpreadReligionAttempt` | Fires for every missionary attempt (success+fail) but carries no AI-log output. |
| `[PERF]` `city.doReligion` | PERF_SCOPE timing (gated `gPerfLogLevel >= 1`) — cost only, not content. |

### What is NOT reachable (the gap)

- **Which religions a city has** — `m_pabHasReligion[eIndex]` not in `/cities`.
- **Holy-city identity** per religion — exposed nowhere.
- **Per-city influence values** — `m_paiReligionInfluence[eIndex]`, the key state the spread formula reads.
- **Player state religion** — `getStateReligion()` not in `/players`.
- **Player religion city counts** — `getHasReligionCount(eIndex)` not exposed.
- **Spread/decay events** — `setHasReligion` fires Python events, but `onReligionSpread`/`onReligionRemove`
  are **commented out** in `CvEventManager.py`; neither roll emits a `[REL]` log line or SSE event.
- **Missionary attempts** — `unitSpreadReligionAttempt` fires but produces no log/SSE output; the
  `iSpreadProb` calc is silent.
- **AI missionary targeting** — `AI_spreadReligion` scoring is untagged (only the resulting
  `missionAI=MISSIONAI_SPREAD` target plot is visible in `/units`).
- **Per-turn spread threshold** — `iRandThreshold` + roll never logged; cannot audit a spread/no-spread.
- **`calculateReligionPercent`** — game-wide religion percentage (used for religious-victory + AI strategy)
  in no endpoint.
- **`noNonStateReligionSpread` status** — not surfaced.
- **Spread game-options** — `MODDERGAMEOPTION_RELIGION_DECAY`, `_MULTIPLE_RELIGION_SPREAD` not in any
  snapshot.

---

## 3. The gap

An observer reading only the HTTP endpoints + event stream today sees: missionary units on the map; and
buildings becoming blocked with `prereqReligion` / `stateReligionInCity` reasons (but **not** the city's
actual religion set). Nothing of: which city has which religion, who the holy city is, how influence
accrues, the passive-spread odds, when a spread or decay fired, or why the AI sent missionaries where it
did. To render religion from the wire you would need the screen for essentially everything — Tier 1 only:
count units + see build-block reasons. This blocks shadowing the religion-dormancy `requires.operate
STATE_RELIGION_IN_CITY` cascade gate against ground truth ([DEC-map-before-delete]).

---

## 4. Proposed hooks — climbing from Tier 1 to Tier 3/4

All hooks follow the three canonical hook shapes — see [DEC-obs-hook-shapes].

### 4a. `/cities` religion fields (Tier 1 → 2)

Add to `CitySnap` and the city render. One pass over `GC.getNumReligionInfos()` per city (negligible):

| JSON key | Source |
|---|---|
| `religions` | array of type-strings where `isHasReligion()` |
| `influence` | map type-string → `getReligionInfluence()` |
| `holyFor` | array of type-strings where `isHolyCity()` |

```json
"religions": ["RELIGION_CHRISTIANITY", "RELIGION_ISLAM"],
"influence": {"RELIGION_CHRISTIANITY": 14, "RELIGION_ISLAM": 3},
"holyFor": ["RELIGION_CHRISTIANITY"]
```

### 4b. `/players` religion fields (Tier 1 → 2)

| JSON key | Source |
|---|---|
| `stateReligion` | `getStateReligion()` type-string (or `"NONE"`) |
| `religionCityCounts` | map type-string → `getHasReligionCount()` |

### 4c. `[REL]` log domain (Tier 2 → 3)

New `logReligionAI(int level, ...)` → `ReligionAI.log`, tag `[REL]`, gated `gPlayerLogLevel`:

| Site | Level | Line |
|---|---|---|
| `CvCity::doReligion` spread fires | 1 | `[REL/spread] turn= city= owner= religion= threshold= roll=` |
| `CvCity::doReligion` decay fires | 1 | `[REL/decay] turn= city= owner= religion= decay= roll=` |
| `setHasReligion` gain / loss | 1 | `[REL/gain\|loss] turn= city= owner= religion=` |
| `CvGame::setHolyCity` change | 1 | `[REL/holycity] turn= religion= city= owner= (was oldCity=)` |
| `CvUnit::spread` attempt | 1 | `[REL/missionary] turn= unit= owner= religion= city= prob= result=success\|fail` |
| `CvCity::doReligion` threshold, no spread | 2 | `[REL/odds] turn= city= owner= religion= threshold= (no spread)` |
| `AI_spreadReligion` target chosen | 2 | `[REL/target] turn= unit= owner= religion= targetCity= score=` |

`[REL/gain]`/`[REL/loss]` are the minimal state-change stream — they let an observer reconstruct which
cities hold which religions turn-by-turn from log start, without the snapshot fields. `[REL/odds]` is the
audit surface to verify the cascade's spread-probability model.

### 4d. Re-enable the Python religion events (Tier 2 → 3, near-free)

Un-comment `'religionSpread': self.onReligionSpread` / `'religionRemove': self.onReligionRemove` in
`Assets/Python/CvEventManager.py`. The handlers are already defined (currently empty) — add log calls or
hook the autolog. Redundant with the C++ `[REL/gain|loss]` lines but zero-cost since the handler exists.

### 4e. `/diagnostic/religionState?player=N` (Tier 3 → 4)

A mailbox endpoint (same game-thread pattern as `canConstruct`) returning the full religion state for a
player's cities, the holy cities, and `calculateReligionPercent` (already on `CvGame`, callable on the game
thread):

```json
{
  "player": 1,
  "stateReligion": "RELIGION_CHRISTIANITY",
  "holyCities": {"RELIGION_CHRISTIANITY": {"city": 42, "name": "Jerusalem"}},
  "cities": [{"id": 42, "name": "Jerusalem", "religions": ["RELIGION_CHRISTIANITY"], "influence": {"RELIGION_CHRISTIANITY": 14}, "holyFor": ["RELIGION_CHRISTIANITY"]}],
  "religionPercent": {"RELIGION_CHRISTIANITY": 37}
}
```

This is the "render religion from API" snapshot, meeting the total-observability bar without a per-frame
snapshot update.

---

## 5. Priority order

| Hook | Tier gain | What becomes visible |
|---|---|---|
| 4a + 4b (snapshot fields) | 1→2 (Informant) | Per-city religion set, influence, holy cities; player state religion; per-religion city counts. Screen no longer needed for the religion census. |
| 4c (`[REL]` domain) + 4d (Python events) | 2→3 (Big Brother) | Live spread/decay/missionary events on `/events`; an autoplay run is fully narrated religion-wise. |
| 4e (`/diagnostic/religionState`) | 3→4 (Thought Police) | On-demand full religion state for cascade shadow comparison. |

All `[REL]` hooks are `gPlayerLogLevel`-gated and off by default.

---

## 6. Code cross-reference

> Paths re-grounded to the reorganized `Sources/` tree (`Cv*` engine → `Sources/Engine/`, `Cv*AI` →
> `Sources/AI/`, `CvHttpServer` → `Sources/Tools/`). Line numbers drift — confirm the function.

| Claim | Source |
|---|---|
| `doReligion` (passive spread + decay) | `Sources/Engine/CvCity.cpp:16708` |
| spread branch / decay branch | `Sources/Engine/CvCity.cpp:16792 / 16729` |
| `setHasReligion` (+ Python events, dormancy check) | `Sources/Engine/CvCity.cpp:15008` |
| `setHolyCity` (influence transfer) | `Sources/Engine/CvGame.cpp:5510` |
| `calculateReligionPercent` | `Sources/Engine/CvGame.cpp:3237` |
| `CvUnit::spread` / `canSpread` | `Sources/Engine/CvUnit.cpp:8481 / 8426` |
| `unitSpreadReligionAttempt` event fire | `Sources/Engine/CvUnit.cpp:8510` |
| `AI_missionaryMove` / `AI_spreadReligion` | `Sources/AI/CvUnitAI.cpp:5584 / 13269` |
| `religionSpread`/`religionRemove` registrations (commented out) | `Assets/Python/CvEventManager.py:153–154` |
| `CitySnap` / `PlayerSnap` (add fields here) | `Sources/Tools/CvHttpServer.cpp` |

---

## See also
- [`README.md`](README.md) — the observability scaffold: the 0–5 scale ([DEC-obs-scale]), the Orwell bar,
  and the three canonical hook shapes ([DEC-obs-hook-shapes]) this map's hooks instantiate.
- [`http-server.md`](http-server.md) — the live surface (`/cities`, `/players`, `/diagnostic`) these hooks
  extend, and the live-read rules (logs held open mid-session).
- [`../../explanation/cascade-architecture.md`](../../explanation/cascade-architecture.md) — why total
  observability is load-bearing: the religion-dormancy `requires.operate STATE_RELIGION_IN_CITY` gate cannot
  be shadowed until religion state is on the wire ([DEC-map-before-delete]).
- [`../../README.md`](../../README.md) — the comprehension map / overview-of-overviews.

[DEC-obs-scale]: ../../architecture/decisions.md#dec-obs-scale
[DEC-obs-hook-shapes]: ../../architecture/decisions.md#dec-obs-hook-shapes
[DEC-map-before-delete]: ../../architecture/decisions.md#dec-map-before-delete
