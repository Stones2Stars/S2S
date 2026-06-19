> DRAFT observability map (2026-06-18) — claims cited from code; verify before relying.

# Observability map: Religion spread

**System:** Religion spread — how religions propagate between cities, spread odds, state-religion
effects, decay, missionary dispatch, and holy-city influence.

**Tier today: 1 (Telescreen)** — coarse snapshots exist (`/players`/`/cities`/`/units`) but zero
religion state is in any of them. Spread and decay events fire silently inside `doTurn`. There are
no `[REL]`-tagged log lines, no SSE events, and no diagnostic endpoints for religion state.

---

## 1. How it actually works

### 1a. Per-turn passive spread — `CvCity::doReligion` (CvCity.cpp:16708)

Called once per city per turn from `CvCity::doTurn` (CvCity.cpp:1349) inside a `PERF_SCOPE`
block. Two game-options gate its two branches:

| Option | Enum | Effect when on |
|---|---|---|
| Religion Decay | `MODDERGAMEOPTION_RELIGION_DECAY` | religions can leave cities |
| Multiple Religion Spread | `MODDERGAMEOPTION_MULTIPLE_RELIGION_SPREAD` | a city with religions can still receive more |

**Iteration order:** the function walks all religion types starting at a *random* offset —
`iReligionX = GAME.getSorenRandNum(iNumReligions, "Random start index")` (CvCity.cpp:16719) —
so only **one** religion is considered per call (the `break` on line 16853/16786 exits the loop
once a spread or decay occurs). The order is random each turn and the starting offset is the seed.

**Spread branch** (CvCity.cpp:16792-16856) — fires when the city does NOT already have that
religion AND (`bMultRelSpread` OR `iReligionCount == 0`):

1. **State-religion gate:** if the city owner has `isNoNonStateReligionSpread()` (CvCity.cpp:16794)
   — set by civic — only the state religion can spread in. Non-state religions are blocked entirely.

2. **Influence accumulation:** walks all living players and all their cities (CvCity.cpp:16799-16834).
   For each source city `cityX`:
   - `cityX` must equal `this` OR be **trade-connected** to it (`isConnectedTo`, CvCity.cpp:16805).
   - `iSpread = cityX->getReligionInfluence(eReligionX)` — the city's accumulated influence for
     that religion (CvCity.cpp:16807). Starts at 0; the holy city receives `GC.getHOLY_CITY_INFLUENCE()`
     (CvGame.cpp:5540) via `setHolyCity`. Buildings also contribute via `CvCity::processBuilding`
     (CvCity.cpp:4694-4695: `changeReligionInfluence` per `ReligionModifier`).
   - Multiplied by `iSpreadFactor` (from `CvReligionInfo::getSpreadFactor()`; clamped to 1 minimum).
   - **Distance penalty for foreign cities:**
     `iSpread /= (iReligionCount + 1) * max(1, RELIGION_SPREAD_DISTANCE_DIVISOR * dist / maxDist - 5)`
     (CvCity.cpp:16817-16829). For the city itself (`cityX == this`), the formula is instead
     `2 * iSpread / (iReligionCount + 1)` (CvCity.cpp:16813) — a local-city "baseline" path.
   - `iRandThreshold = max(iRandThreshold, iSpread)` — takes the best single source, not a sum.

3. **Player spread-rate modifier** (CvCity.cpp:16838):
   `iRandThreshold *= max(1, getModifiedIntValue(100, owner.getReligionSpreadRate())) / 100`
   — a per-player percentage modifier (sourced from civics via `CvCivicInfo::getReligionSpreadRate`).

4. **Roll:** `getSorenRandNum(RELIGION_SPREAD_RAND * gameSpeedPercent / 100, "Religion Spread")`
   (CvCity.cpp:16841-16850). If roll < threshold: `setHasReligion(eReligionX, true, true, true)`
   — religion is added, Python event fires, buildings are potentially enabled/disabled (see below).

**Decay branch** (CvCity.cpp:16729-16789) — fires when the city HAS that religion AND
`bReligionDecay` is on:

- Exempt: `eReligionX == eStateReligion`, or this city IS the holy city, or only one religion present.
- `iDecay = getSpreadFactor() + (iReligionCount - 2)^2` — base decay scaled by religion crowding.
- Mitigated by connected own-empire cities that also have the religion
  (`iDecay *= 9/(10 + cityX->getReligionInfluence(eReligionX))`).
- Mitigated by this city's own influence (`iDecay /= 1 + getReligionInfluence(eReligionX)`).
- Modified by holy-city ownership: own-empire owner → half decay; at-war with holy city → ×4/3.
- Same `RELIGION_SPREAD_RAND * gameSpeedPercent` roll. If decay fires:
  - `setHasReligion(eReligionX, false, true, false)` (CvCity.cpp:16772).
  - All buildings with `getPrereqReligion() == eReligionX` are forcibly removed
    (`changeHasBuilding(eTypeX, false)`, CvCity.cpp:16782-16784).

### 1b. `setHasReligion` side-effects (CvCity.cpp:15008)

Every religion gain/loss triggers:
- `GET_PLAYER(getOwner()).changeHasReligionCount(eIndex, ±1)` (CvCity.cpp:15027)
- `FlushCanConstructCache()` — buildability cache invalidated (religion-gated buildings may change)
- Python event: `CvEventReporter::getInstance().religionSpread/Remove(...)` (CvCity.cpp:15097/15099)
- `applyReligionModifiers(eIndex, bNewValue)` — modifiers applied or removed
- `checkReligiousDisablingAllBuildings()` — dormancy check for religiously-limited buildings

**UI announce:** a `AddDLLMessage` is sent to human players who are: the city owner, share the
state religion, or own the holy city, AND can see the city (CvCity.cpp:15044-15059). AI players
get no announce. The announce is human-UI only, not an event/log line.

### 1c. Holy city influence (CvGame::setHolyCity — CvGame.cpp:5510)

- `pOldValue->changeReligionInfluence(eIndex, -GC.getHOLY_CITY_INFLUENCE())` on the old holy city
- `pHolyCity->changeReligionInfluence(eIndex, +GC.getHOLY_CITY_INFLUENCE())` on the new one
- `GC.getHOLY_CITY_INFLUENCE()` is a global constant (XML define), not documented here — verify.
- A city's `m_paiReligionInfluence[eIndex]` is the spread-source weight for all passive rolls.
  It is saved/loaded via `WRAPPER_READ/WRITE_CLASS_ARRAY ... REMAPPED_CLASS_TYPE_RELIGIONS`
  (CvCity.cpp:17122/17844).

### 1d. Missionary spread (CvUnit::spread — CvUnit.cpp:8481)

Triggered when a missionary unit executes `MISSION_SPREAD` on a city plot.

- `canSpread` check (CvUnit.cpp:8426): religion must be founded, city must not already have it,
  unit must have `getReligionSpreads(eReligion) > 0` (from `CvUnitInfo`), entry must be allowed.
  - Non-state religions blocked by `isNoNonStateReligionSpread()` on the target city's owner
    (CvUnit.cpp:8448) — same gate as passive spread.
  - "Divine prophet" mode: if `GAMEOPTION_RELIGION_DIVINE_PROPHETS` and unit is not
    `UNITAI_MISSIONARY`, additional tech-timing constraints apply (CvUnit.cpp:8455-8474).

- If the religion is **not yet founded**, the unit founds it (divine-prophet path): `setHolyCity`,
  `setReligionSlotTaken`, always succeeds (CvUnit.cpp:8532-8536).

- If already founded (normal missionary path):
  - `iSpreadProb = getReligionSpreads(eReligion)` base probability (CvUnit.cpp:8493).
  - +`getExtraStateReligionSpreadModifier()` if spreading state religion, else
    +`getExtraNonStateReligionSpreadModifier()` (CvUnit.cpp:8497-8499).
  - ÷2 if spreading into a foreign team city (CvUnit.cpp:8502-8503).
  - `iSpreadProb += (numReligions - cityReligionCount) * (100 - iSpreadProb) / numReligions`
    — bonus for empty slots (CvUnit.cpp:8506).
  - Roll: `getSorenRandNum(100, "Unit Spread Religion") < iSpreadProb` (CvUnit.cpp:8507).
  - Python event `unitSpreadReligionAttempt(unit, religion, bSuccess)` fires regardless of outcome
    (CvUnit.cpp:8510).
  - On failure: `AddDLLMessage` to the owner (human-UI only, no log line).
  - On success: `pCity->setHasReligion(eReligion, true, true, false)` (CvUnit.cpp:8530).
  - Missionary is always killed after the attempt (CvUnit.cpp:8545).

### 1e. AI missionary decision (CvUnitAI::AI_missionaryMove / AI_spreadReligion)

`AI_missionaryMove` (CvUnitAI.cpp:5584) is the main AI missionary turn routine.
It calls `AI_spreadReligion()` (CvUnitAI.cpp:13269) which:
- Determines the religion to spread (state religion preferred; falls back to first with `getReligionSpreads > 0`).
- Builds a `CvReachablePlotSet` from current position (MOVE_NO_ENEMY_TERRITORY, cached).
- Scores candidate cities by: existing mission targeting, culture-victory strategy, holy-city ownership,
  target-player relationship, number of cities without the religion.
- Pushes `MISSION_SPREAD` or a move-toward mission.
- No logging — none of this AI evaluation is tagged or streamed.

### 1f. Religiously-limited buildings (CvCity.cpp:21279-21319 area)

Referenced in `cascade-mapping-inventory.md` §B-ii: `setReligiouslyLimitedBuilding` /
`m_pabReligiouslyDisabledBuilding`. A building is disabled when the city owner's state religion
does not match the building's required religion (unless `hasAllReligionsActive` waiver applies).
This is a dormancy trigger driven by religion state — not spread itself, but directly downstream.

---

## 2. Current observability

**Tier: 1 (Telescreen).**

### What IS reachable from outside today

| Surface | What you can see |
|---|---|
| `/players` snapshot (CvHttpServer.cpp:286-304) | Player score, era, techs, gold, cities count, units count, production, research. **No religion fields.** |
| `/cities` snapshot (CvHttpServer.cpp:339-357) | City position, population, yields, buildings count, culture level, capital flag, crime/education/disease properties. **No religion fields.** |
| `/units` snapshot | Unit type, AI, position, damage, level. A missionary can be identified by `type=UNIT_MISSIONARY_*` and `unitAI=UNITAI_MISSIONARY` — its target is in `missionAI` (MISSIONAI_SPREAD). |
| `/diagnostic/canConstruct` legacyReason | Returns `"prereqReligion"` / `"prereqStateReligion"` / `"stateReligionInCity"` / `"holyCity"` if a building is blocked by religion. **The blocking reason is exposed; the underlying city religion set is not.** |
| Python event `religionFounded` | Registered and fires (autologEventManager.py:160); logs the founding event to the autolog. |
| Python event `religionSpread` | Handler exists (CvEventManager.py:2369) but **commented out** (CvEventManager.py:153). autologEventManager.py:161 registers `onReligionSpread` — but it only fires if the engine event registration is live. |
| Python event `religionRemove` | Same: handler defined but **commented out** in main event manager (CvEventManager.py:154). |
| Python event `unitSpreadReligionAttempt` | Fires from CvUnit.cpp:8510 for every missionary attempt (success+fail). Handler in CvEventManager.py — **not commented out** but carries no logging to the AI log. |
| `[PERF]` city phase timing | `city.doReligion` is measured as a PERF_SCOPE (CvCity.cpp:1349) — visible in Performance.log if `gPerfLogLevel >= 1`. Cost only, not content. |

### What is NOT reachable today (the gap)

1. **Which religions a city has** — `m_pabHasReligion[eIndex]` is not in the `/cities` snapshot.
2. **Holy city identity** — which city holds the holy seat for each religion is not exposed anywhere.
3. **Per-city influence values** — `m_paiReligionInfluence[eIndex]` (the spread-source weights) are
   not in the snapshot. This is the key per-city state the passive spread formula reads.
4. **Player state religion** — `getStateReligion()` is not in the `/players` snapshot.
5. **Player religion city counts** — `getHasReligionCount(eIndex)` (cities-with-religion count per player)
   is not exposed.
6. **Spread/decay events as a live stream** — `setHasReligion` fires a Python event, but
   `onReligionSpread`/`onReligionRemove` are commented out of `CvEventManager.py` (line 153-154).
   Neither the passive-spread roll (CvCity.cpp:16850) nor the decay roll (CvCity.cpp:16770) emit
   any `[REL]`-tagged log line or SSE event.
7. **Missionary spread attempts** — `unitSpreadReligionAttempt` fires but produces no observable
   output to logs/SSE. The probability calculation (iSpreadProb, CvUnit.cpp:8493-8507) is silent.
8. **AI missionary targeting logic** — `AI_spreadReligion` scores and commits with no log tags.
   The chosen target appears in `/units` as `missionAI=MISSIONAI_SPREAD` with a target plot, but
   the scoring breakdown is invisible.
9. **Religion decay events** — when `MODDERGAMEOPTION_RELIGION_DECAY` removes a religion from a
   city, the only signal is the Python `religionRemove` event (commented out).
10. **Per-turn spread threshold** — the computed `iRandThreshold` and the roll are never logged.
    There is no way to audit why a religion spread or failed to spread in a given turn.
11. **`calculateReligionPercent`** — the game-wide religion percentage (used for religious victory
    evaluation and AI strategy) is not in any snapshot endpoint.
12. **`noNonStateReligionSpread` status** — whether a city/player is blocking non-state religions is
    not surfaced in any endpoint.
13. **Game options affecting spread** — `MODDERGAMEOPTION_RELIGION_DECAY` and
    `MODDERGAMEOPTION_MULTIPLE_RELIGION_SPREAD` are not in the `/players` or `/diagnostic` snapshot.

---

## 3. The gap (summary)

The religion spread system is completely dark from outside. An observer reading only the HTTP
endpoints + event stream today would see:

- Missionary units on the map (type + MISSIONAI_SPREAD on their group) — **visible**.
- Buildings becoming blocked with `prereqReligion` / `stateReligionInCity` reasons — **visible from
  /diagnostic/canConstruct but you don't see the city's actual religion set**.
- Nothing of: which city has which religion, who the holy city is, how influence accrues, what the
  passive-spread odds are, when a spread or decay fired, why the AI sent missionaries where it did.

To "render the game from the wire" for religion, you would need to query the screen for essentially
everything. Tier 1 only: you can count units + see build-block reasons.

---

## 4. Proposed hooks to climb tiers

All hooks are **gated** (off by default; behind existing `gPlayerLogLevel` / `Autolog__HttpServer`)
and follow the patterns in `ai-logging-reference.md`. No engine behaviour is changed.

### 4a. Add religion state to the `/cities` snapshot (Tier 1 → 2)

In `CvHttpServer.cpp`'s city snapshot loop (around line 1557), add per-city religion fields to
`CitySnap` and the `renderCities` picojson render:

```
// in CitySnap struct: add
std::vector<std::string> religions; // PREFIX_NAMEs of religions present in city
bool isHolyCity[MAX_RELIGIONS];     // compact: or just a string set
int religionInfluence[MAX_RELIGIONS];
```

Or, since `MAX_RELIGIONS` is unknown at snapshot time, use a `std::vector<std::pair<std::string,int>>`
(type name + influence value). Serializes as:

```json
"religions": ["RELIGION_CHRISTIANITY", "RELIGION_ISLAM"],
"influence": {"RELIGION_CHRISTIANITY": 14, "RELIGION_ISLAM": 3},
"holyCities": ["RELIGION_CHRISTIANITY"]
```

Concrete field additions to `CitySnap`:
- `religions` — array of religion type-strings where `isHasReligion()` is true.
- `influence` — map of religion type-string → `getReligionInfluence()` value.
- `holyFor` — array of religion type-strings where `isHolyCity()` is true.

**Impact on snapshot cost:** one pass over `GC.getNumReligionInfos()` per city — negligible (typically
<20 religions; the loop is already per-city).

### 4b. Add religion state to the `/players` snapshot (Tier 1 → 2)

In `PlayerSnap` / `renderPlayers`, add:
- `stateReligion` — type-string of `getStateReligion()` (or `"NONE"`).
- `religionCityCounts` — map of religion type-string → `getHasReligionCount()`, i.e. how many of
  this player's cities have each religion.

### 4c. Add a `[REL]` log domain (Tier 2 → 3)

New `logReligionAI(int level, const char* fmt, ...)` helper in `BetterBTSAI.{h,cpp}`:
- Tag: `[REL]`, log file: `ReligionAI.log`, scope global: `gPlayerLogLevel`.
- Reserve tag in the registry in `ai-logging-reference.md`.

**Hook sites:**

| Site | Level | Proposed line | File:line |
|---|---|---|---|
| `CvCity::doReligion` spread fires | 1 | `[REL/spread] turn=N city=ID owner=P religion=TYPE threshold=T roll=R` | CvCity.cpp:16850 |
| `CvCity::doReligion` decay fires | 1 | `[REL/decay] turn=N city=ID owner=P religion=TYPE decay=D roll=R` | CvCity.cpp:16770 |
| `CvCity::doReligion` spread computed threshold (no spread this turn) | 2 | `[REL/odds] turn=N city=ID owner=P religion=TYPE threshold=T (no spread)` | CvCity.cpp:16836 |
| `CvUnit::spread` missionary attempt | 1 | `[REL/missionary] turn=N unit=ID owner=P religion=TYPE city=ID prob=P result=success|fail` | CvUnit.cpp:8507 |
| `CvUnitAI::AI_spreadReligion` target chosen | 2 | `[REL/target] turn=N unit=ID owner=P religion=TYPE targetCity=ID score=S` | CvUnitAI.cpp:~13450 |
| `setHasReligion` gain | 1 | `[REL/gain] turn=N city=ID owner=P religion=TYPE` | CvCity.cpp:15020 |
| `setHasReligion` loss | 1 | `[REL/loss] turn=N city=ID owner=P religion=TYPE` | CvCity.cpp:15020 |
| `CvGame::setHolyCity` change | 1 | `[REL/holycity] turn=N religion=TYPE city=ID owner=P (was oldCity=ID)` | CvGame.cpp:5530 |

The `[REL/gain]` and `[REL/loss]` lines are the minimal "state change stream" — they let an observer
reconstruct which cities have which religions turn-by-turn from log start, without needing the snapshot
fields. The spread-odds lines (`[REL/odds]`) are the audit surface needed to verify the cascade
correctly models the spread probability.

### 4d. Re-enable Python religion events in CvEventManager.py (Tier 2 → 3)

Un-comment lines 153-154 in `Assets/Python/CvEventManager.py`:
```python
'religionSpread': self.onReligionSpread,
'religionRemove': self.onReligionRemove,
```
The handlers (CvEventManager.py:2369-2374) are already defined but empty — add log calls or
hook the autolog path. The Python events fire from the same `setHasReligion` call that
the C++ `[REL/gain|loss]` lines would cover, so these are redundant with 4c — but the Python
path is zero-cost to enable since the handler exists.

### 4e. Add `/diagnostic/religionState?player=N` endpoint (Tier 3 → 4)

A new on-demand diagnostic that returns the full religion state for a player's cities. Like
`/diagnostic/canConstruct` it runs on the game thread via the mailbox:

```json
{
  "player": 1,
  "stateReligion": "RELIGION_CHRISTIANITY",
  "holyCities": {"RELIGION_CHRISTIANITY": {"city": 42, "name": "Jerusalem"}},
  "cities": [
    {"id": 42, "name": "Jerusalem", "religions": ["RELIGION_CHRISTIANITY"], "influence": {"RELIGION_CHRISTIANITY": 14}, "holyFor": ["RELIGION_CHRISTIANITY"]},
    ...
  ],
  "religionPercent": {"RELIGION_CHRISTIANITY": 37, ...}
}
```

`calculateReligionPercent` is already on `CvGame` (CvGame.cpp:3237) and can be called from the
game thread. This endpoint would be the "render religion from API" snapshot, satisfying the
total-observability bar for this system without needing a per-frame snapshot update.

### Summary: tier targets

| After hook | Tier | What becomes visible |
|---|---|---|
| 4a + 4b (snapshot fields) | 2 (Informant) | Per-city religion set, influence values, holy cities; player state religion; city counts per religion. Screen no longer needed for religion census. |
| 4c ([REL] log domain) + 4d (Python events re-enabled) | 3 (Big Brother) | Real-time spread/decay/missionary events on `/events` stream. An autoplay run is fully narrated religion-wise from the wire. |
| 4e (/diagnostic endpoint) | 4 (Thought Police) | On-demand full religion state for any player, for cascade shadow comparison. The §A "religion spread" opaque-system bar is met. |

---

## 5. Cross-references

- `cascade-mapping-inventory.md` §A — religion spread is listed as an opaque system.
- `cascade-mapping-inventory.md` §D — the Observability Scale; this system is currently Tier 1.
- `cascade-mapping-inventory.md` §B-ii — religious-building dormancy (`isReligiouslyLimitedBuilding`)
  is a downstream effect of religion spread; its shadow is separately tracked there.
- `http-server.md` — the live surface; the snapshot structure (`CitySnap`, `PlayerSnap`) is the
  place to add fields 4a/4b. The mailbox pattern is the place to add 4e.
- `ai-logging-reference.md` — tag registry; `[REL]` must be added when 4c is implemented.
- `enabler-cascade-spec.md` §B-ii — religion dormancy (`requires.operate STATE_RELIGION_IN_CITY`)
  is the cascade representation; spread is the state that feeds that gate.
