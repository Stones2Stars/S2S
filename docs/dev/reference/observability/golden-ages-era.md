# Observability map: Golden Ages & Era Advance

> DRAFT observability map (2026-06-18 by parent) — claims cited from code; verify before relying.

**Scope:** per-player golden age state (active/inactive, turns remaining, escalating cost),
golden age effects on yields/commerce/growth/GP rate/anarchy, and per-player era advance
(tech-triggered; the `m_eCurrentEra` state machine).

**Observability tier assigned: 1 — Telescreen.** The `/players` snapshot exposes
`era` (an integer) as a coarse snapshot — that is all. Golden age active/inactive, turns
remaining, accumulated count, and all per-turn yield/growth effects are completely invisible
from outside. Era transitions are not streamed as events; they are only detectable by polling
`/players` and noticing the `era` integer changed between two snapshots up to 5 s apart.

---

## 1. How it actually works

### 1a. Golden age — state variables

Each `CvPlayer` maintains two persisted scalars relevant here:

- `m_iGoldenAgeTurns` (CvPlayer.cpp:775, CvPlayer.h:1788) — turns remaining in the current
  golden age. 0 = not in a golden age. Decremented once per turn in `CvPlayer::doTurn`
  (CvPlayer.cpp:3856: `changeGoldenAgeTurns(-1)`), but only if the golden age was already
  active at the START of the turn (`bWasGoldenAgeLastTurn` guard at CvPlayer.cpp:3689).
- `m_iNumUnitGoldenAges` (CvPlayer.h, incremented at CvUnit.cpp:9925) — cumulative count
  of golden ages triggered by sacrificing great-person units. Used to scale the required
  number of great-person units for the NEXT such golden age (formula below). Does NOT count
  building-triggered or event-triggered golden ages.

### 1b. Golden age — length formula

`CvPlayer::getGoldenAgeLength()` (CvPlayer.cpp:9460):

```
max(1, goldenAgeLength100() * (1 + goldenAgeModifier/100) / 100)
```

where `goldenAgeLength100()` (CvGame.cpp:3263) = `GC.getGOLDEN_AGE_LENGTH() *
GC.getGameSpeedInfo(getGameSpeedType()).getSpeedPercent()`. Currently `GOLDEN_AGE_LENGTH = 4`
(Assets/XML/GlobalDefines.xml:637). So default length at normal speed = 4 turns before
modifiers.

`getGoldenAgeModifier()` accumulates from:
- Building `GoldenAgeModifier` XML tags → `processBuilding` → `changeGoldenAgeModifier`
  (CvPlayer.cpp:7388).
- Trait `GoldenAgeDurationModifier` → `processTrait` → `changeGoldenAgeModifier`
  (CvPlayer.cpp:28528).

### 1c. Golden age — trigger sites (5 distinct paths)

**Path 1 — Great-person unit sacrifice** (CvUnit.cpp:9915, `CvUnit::goldenAge()`):

1. Unit must have `CvUnitInfo::isGoldenAge() == true` (the great-person flag,
   CvUnit.cpp:11082).
2. `CvPlayer::unitsGoldenAgeReady()` (CvPlayer.cpp:9054) counts distinct great-person unit
   TYPES on the map (one type per slot, deduped by type). This must be `>=` the required count.
3. Required count = `unitsRequiredForGoldenAge()` (CvPlayer.cpp:9042):
   `BASE_GOLDEN_AGE_UNITS + numUnitGoldenAges * GOLDEN_AGE_UNITS_MULTIPLIER`.
   Currently `BASE_GOLDEN_AGE_UNITS = 1`, `GOLDEN_AGE_UNITS_MULTIPLIER = 0` → always 1 unit
   required regardless of how many unit golden ages have been triggered.
4. On activation: `killGoldenAgeUnits` sacrifices the required count (CvPlayer.cpp:9089);
   `changeGoldenAgeTurns(getGoldenAgeLength())` fires; `changeNumUnitGoldenAges(1)`.

**Path 2 — Building completion** (CvCity.cpp:14587, inside `CvCity::processBuilding`):

```cpp
if (kBuilding.isGoldenAge())
    GET_PLAYER(getOwner()).changeGoldenAgeTurns(1 + getGoldenAgeLength());
```

Note: `1 + getGoldenAgeLength()` — one extra turn compared to the unit path. This fires
during `setHasBuilding` (for applicable buildings marked `<bGoldenAge>1</bGoldenAge>` in XML).
Only fires when `GC.getGame().isFinalInitialized()` (CvCity.cpp:14585).

**Path 3 — Great-person birth with `GoldenAgeOnBirthOfGreatPerson`**
(CvPlayer.cpp:20467-20469, inside `doGreatPersonBorn`):

```cpp
if (getGoldenAgeOnBirthOfGreatPersonCount(eGreatPersonUnit) > 0)
    changeGoldenAgeTurns(getGoldenAgeLength());
```

`m_goldenAgeOnBirthOfGreatPersonCount` (CvPlayer.h, a `std::map<short,char>`) is keyed by
unit type. The count is loaded from save and set by traits/buildings/civics that grant
auto-golden-ages on GP birth.

**Path 4 — Random event** (CvPlayer.cpp:21714-21716, inside `applyEvent`):

```cpp
if (kEvent.isGoldenAge())
    changeGoldenAgeTurns(getGoldenAgeLength());
```

Triggered by an event XML entry with `<bGoldenAge>1</bGoldenAge>`.

**Path 5 — Python / WorldBuilder** via `CyPlayer::setCurrentEra` or direct
`changeGoldenAgeTurns` Python exposure (CvPythonPlayerLoader.cpp).

### 1d. Golden age — effects during active turns

When `isGoldenAge()` is true the following engine gates fire differently each turn:

- **Base yield bonus per city** — `CvCity::getBaseYieldRate` (CvCity.cpp:22818) adds
  `getGoldenAgeYield(eIndex)` per yield type. `m_aiGoldenAgeYield[NUM_YIELD_TYPES]` is
  accumulated from XML traits/buildings.
- **Base commerce bonus per city** — `CvCity::getBaseCommerceRateExtra` (CvCity.cpp:11936)
  adds `getGoldenAgeCommerce(eIndex)` per commerce type.
- **No anarchy** — `getCivicAnarchyLength()` (CvPlayer.cpp:8942) and
  `getReligionAnarchyLength()` (CvPlayer.cpp:9003) both short-circuit to 0 when
  `isGoldenAge()`.
- **GP rate modifier** — `CvCity::getBaseGreatPeopleRateModifier()` (CvCity.cpp:7164) adds
  `GC.getGOLDEN_AGE_GREAT_PEOPLE_MODIFIER()` to the city's GP rate.
- **Reduced food-for-growth threshold** — `CvPlayer::growthThreshold()` (CvPlayer.cpp:24462)
  applies `GOlDEN_AGE_PERCENT_LESS_FOOD_FOR_GROWTH` when golden age is active.
- **AI civic re-evaluation** — `CvPlayerAI::AI_startGoldenAge()` (CvPlayerAI.cpp:6253)
  resets the civic timer to 0 so the AI immediately reconsiders civics on golden age start.

### 1e. Golden age — `changeGoldenAgeTurns` side-effects on transition

`CvPlayer::changeGoldenAgeTurns` (CvPlayer.cpp:9395) when the golden-age flag FLIPS:
- On START: `changeAnarchyTurns(-getAnarchyTurns())` (clears anarchy); calls
  `AI_startGoldenAge()`; calls `updateYield()`.
- On END: `CvEventReporter::getInstance().endGoldenAge(getID())` (Python callback, no HTTP
  event); `updateYield()`.
- Both: `AddDLLMessage` to all met players (in-game notification text only, not HTTP events).
- Start also: `CvEventReporter::getInstance().goldenAge(getID())` (Python, no HTTP event);
  `GC.getGame().addReplayMessage(...)`.

### 1f. Era advance — state variable and trigger

`m_eCurrentEra` (CvPlayer.h:873, `DllExport EraTypes getCurrentEra() const`) is a
per-player integer era index.

**Only trigger site in the live game:** `CvTeam::setHasTech` (CvTeam.cpp:5306-5308):

```cpp
if (player.getCurrentEra() < kTech.getEra())
    player.setCurrentEra((EraTypes)(kTech.getEra()));
```

This runs for every player on the team that researched the tech. Era only increases — it
never decreases via normal gameplay. `kTech.getEra()` is the XML `<Era>` field on the
TechInfo; a tech that has a higher era than the player's current era triggers `setCurrentEra`.

There is no separate "era advance event" in the HTTP event stream. The Python event reporter
fires `techAcquired` (CvTeam.cpp:5323, CvEventReporter.cpp) but nothing specific to era
advance, and there is no SSE `publishEvent` call anywhere in `CvTeam.cpp`.

**`setCurrentEra` side-effects** (CvPlayer.cpp:12414):
- Heritage commerce era-change deltas applied.
- Per-city free specialist grant (`getEraAdvanceFreeSpecialistCount`).
- Graphics/flag dirty-bits for UI refresh.
- For human players not in network MP: `BUTTONPOPUP_PYTHON_SCREEN` era-movie popup.

### 1g. Era — indirect effects

Many modifiers are scaled by era integer directly:
- Handicap AI per-era bonuses: `getAIPerEraModifier() * getCurrentEra()` appears in ~6
  places (CvPlayer.cpp:6988, 7934, 7993, 10354, etc.).
- Anarchy length: `GC.getEraInfo(getCurrentEra()).getAnarchyPercent()` (CvPlayer.cpp:8988,
  9027).
- Growth threshold era scaling: `GC.getEraInfo(getCurrentEra()).getGrowthPercent()`
  (CvPlayer.cpp:24446).
- Event probability: `GC.getEraInfo(getCurrentEra()).getEventChancePerTurn()`
  (CvPlayer.cpp:22492).

---

## 2. Current observability

### What is already exposed

| Surface | Field / tag | What it gives you |
|---|---|---|
| `GET /players` | `era` (integer) | Player's current era index — snapshotted every 5 s. Lets you detect era changes by polling (polling only; no event). |
| `[DAI/begin]` (level 1) | `era=N` | The current era integer at the start of each AI decision turn, in `DecisionAI.log` and on the `/events` stream. |
| Python callback | `goldenAge(player)` / `endGoldenAge(player)` | Python event reporter fires on golden age start/end — but this is Python-only; it does NOT reach the HTTP `/events` SSE stream. |
| Python callback | `techAcquired(tech, team, player, bAnnounce)` | Tech acquisition is Python-observable — but again Python-only; no SSE event. |
| Replay message | `REPLAY_MESSAGE_MAJOR_EVENT` | Written to replay on golden age start, visible in the replay file — not live-accessible. |

### What is NOT exposed

- `getGoldenAgeTurns()` — turns remaining in current golden age. Not in any snapshot or
  log. You cannot tell from outside whether a player is in a golden age or how long remains.
- `isGoldenAge()` — the active/inactive flag. Derivable only if `goldenAgeTurns` is exposed
  (which it isn't).
- `getNumUnitGoldenAges()` — cumulative count (for the required-units formula). Not
  published.
- `unitsRequiredForGoldenAge()` / `unitsGoldenAgeReady()` — the "can trigger golden age"
  readiness state. Not published.
- `getGoldenAgeModifier()` — the per-player duration modifier accumulated from traits and
  buildings.
- `getGoldenAgeLength()` — computed effective length. Not published.
- Per-player `getGoldenAgeYield[]` / `getGoldenAgeCommerce[]` — the bonus magnitudes
  active during a golden age. Not published.
- Era advance as an event — era change is NOT streamed; it must be inferred by comparing
  successive `/players` snapshots up to 5 s apart.
- `getEraAdvanceFreeSpecialistCount` grants — free specialists silently added to all cities
  on era advance. Not logged or published.

---

## 3. The gap

An external reader cannot:

1. Know that any player is in a golden age at all (no field in `/players` snapshot).
2. Know when a golden age started or will end (no event; no turns-remaining field).
3. Distinguish which golden age trigger path fired (unit sacrifice vs. building vs. GP-birth
   vs. event vs. Python).
4. Know how many unit golden ages a player has accumulated (escalation counter).
5. Know the effective golden age length for a player (trait + building modifier is hidden).
6. Detect an era advance as an event — only by polling and noticing `era` incremented. The
   5 s snapshot cadence means an era advance can be missed entirely if two advances happen
   within one 5 s window (unlikely but not impossible with tech gifts/rapid conquest).
7. Know what free-specialist grants were applied on era advance.
8. Compute golden-age yield/commerce/growth bonuses without knowing the
   `getGoldenAgeYield[]` / `getGoldenAgeCommerce[]` arrays.

Under the Orwell bar: an agent watching the wire cannot narrate "player X entered a golden
age this turn" or "player Y advanced to era Z" in real time. Both facts require either looking
at the game screen or polling the era field and inferring from stale snapshots.

---

## 4. Proposed hooks

All hooks are gated (off by default) and follow the existing patterns in
`BetterBTSAI.{h,cpp}` + `CvHttpServer.cpp`. None of these modify any AI behaviour.

### 4a. `/players` snapshot — add golden age fields (cheapest win)

In `PlayerSnap` (CvHttpServer.cpp:61), add:

```cpp
int iGoldenAgeTurns;   // = kPlayer.getGoldenAgeTurns()
int iNumUnitGoldenAges; // = kPlayer.getNumUnitGoldenAges()
```

In `publishIfDue` (CvHttpServer.cpp:~1530), add:

```cpp
snap.iGoldenAgeTurns   = kPlayer.getGoldenAgeTurns();
snap.iNumUnitGoldenAges = kPlayer.getNumUnitGoldenAges();
```

In `renderPlayers` (CvHttpServer.cpp:~293), add:

```cpp
o["goldenAgeTurns"]    = picojson::value((double)p.iGoldenAgeTurns);
o["numUnitGoldenAges"] = picojson::value((double)p.iNumUnitGoldenAges);
```

This alone makes the golden age active/inactive flag and turns remaining readable from
outside with zero log cost. `isGoldenAge()` = `goldenAgeTurns > 0`.

### 4b. SSE events for golden age start/end and era advance

These are the highest-value hooks because they eliminate polling and make the system
event-driven. Add `publishEvent` calls in `CvPlayer::changeGoldenAgeTurns` and
`CvPlayer::setCurrentEra`. These calls are already gated by `CvHttpServer::isEnabled()`
(they are no-ops when the server is off).

**Golden age start** (CvPlayer.cpp:9419, beside the existing `CvEventReporter::goldenAge`
call):

```cpp
if (CvHttpServer::isEnabled())
{
    CvHttpServer::publishEvent("goldenAgeStart",
        CvString::format("{\"player\":%d,\"turns\":%d}",
            getID(), getGoldenAgeTurns()).c_str());
}
```

**Golden age end** (CvPlayer.cpp:9416, beside `CvEventReporter::endGoldenAge`):

```cpp
if (CvHttpServer::isEnabled())
{
    CvHttpServer::publishEvent("goldenAgeEnd",
        CvString::format("{\"player\":%d}", getID()).c_str());
}
```

**Era advance** (CvPlayer.cpp:12418, inside `setCurrentEra` after `m_eCurrentEra = eNewValue`):

```cpp
if (CvHttpServer::isEnabled())
{
    CvHttpServer::publishEvent("eraAdvance",
        CvString::format("{\"player\":%d,\"era\":%d}",
            getID(), (int)eNewValue).c_str());
}
```

### 4c. Gated log lines — `[ERA]` tag, `gPlayerLogLevel`

Add a new `[ERA]` subsystem log using the existing `logPlayerAI` pattern (same
`gPlayerLogLevel` gate, `PlayerAI.log` or a new `EraAI.log`). Emit at level 1:

- **Golden age start:**
  `[ERA/goldenAge] player=N turn=T turns=N trigger=<unit|building|birth|event>`
  The trigger type requires a small enum or const-char* argument passed into
  `changeGoldenAgeTurns` from each call site; alternatively, log at each call site.
- **Golden age end:**
  `[ERA/goldenAgeEnd] player=N turn=T numUnitGAs=N`
- **Era advance:**
  `[ERA/advance] player=N turn=T oldEra=N newEra=N tech=TECH_KEY`
  The `tech` argument is available in `setHasTech` context but not inside `setCurrentEra`
  itself; the cleanest hook is a one-liner log in `CvTeam::setHasTech` (CvTeam.cpp:5306)
  just before the `setCurrentEra` call.

### 4d. `/diagnostic` or per-player computed fields

For the cascade verification use case, add these to `/players` (or as a new diagnostic
endpoint):

```
goldenAgeLength       -- getGoldenAgeLength() = the effective duration for this player
goldenAgeModifier     -- getGoldenAgeModifier() (additive %)
goldenAgeYield[]      -- getGoldenAgeYield(eYield) for each yield type
goldenAgeCommerce[]   -- getGoldenAgeCommerce(eCommerce) for each commerce type
unitsGoldenAgeReady   -- unitsGoldenAgeReady() (distinct GP types on the map)
unitsRequiredForGA    -- unitsRequiredForGoldenAge()
```

These can live in `/players?verbose=1` or as a new
`/diagnostic/goldenAgeState?player=N` endpoint (the mailbox pattern used for canConstruct).

### Summary table

| Hook | Cost | What it adds |
|---|---|---|
| `goldenAgeTurns` + `numUnitGoldenAges` in `/players` snapshot | Minimal (two int reads per publish) | Golden age active/inactive + turns remaining + escalation counter readable at all times |
| SSE `goldenAgeStart` / `goldenAgeEnd` events | Minimal (conditional string format) | Real-time start/end detection; no polling required |
| SSE `eraAdvance` event | Minimal | Era transitions become stream-observable instead of poll-inferred |
| `[ERA/*]` log lines | Minimal (gated) | Per-turn narrative of era/GA state in `PlayerAI.log` or dedicated `EraAI.log`; feed to `/events` via `streamLogTee` |
| `/players` verbose / `/diagnostic/goldenAgeState` | On-demand only | Full modifier/yield picture for cascade verification |

Implementing hooks 4a + 4b brings the system from **Tier 1 (Telescreen)** to **Tier 3
(Big Brother)** for this subsystem: the snapshot gives the active state, and the events give
real-time transitions. Tier 4 (Thought Police) additionally needs the yield/modifier scalars (4d)
to fully reconstruct what a golden age is DOING to a player's economy turn by turn.
