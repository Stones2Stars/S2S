# Observability — golden ages & era advance

> **Status:** reference (per-system observability map) · **Verified against:** old map (2026-06-18) re-grounded to the reorganized source tree, 2026-06-20
> **Grounding:** `Sources/Engine/CvPlayer.cpp`, `Sources/Engine/CvCity.cpp`, `Sources/Engine/CvUnit.cpp`, `Sources/Engine/CvGame.cpp`,
> `Sources/Engine/CvTeam.cpp`, `Sources/AI/CvPlayerAI.cpp`, `Sources/Tools/CvHttpServer.cpp`. Citations carried from the old map; the `Sources/` prefixes
> are re-grounded (`Cv*` → `Sources/Engine/`, `Cv*AI` → `Sources/AI/`, `CvHttpServer` → `Sources/Tools/`) but the **line numbers were NOT re-verified**
> against the reorganized tree — treat each as "the named function, around this line" and confirm the function, not the integer.
> One-paragraph orientation: this maps per-player golden-age state (active flag, turns remaining, escalating cost, the five trigger paths, and the
> yield/commerce/growth/GP/anarchy effects) and per-player era advance (the tech-triggered `m_eCurrentEra` state machine) — and why both are nearly invisible
> from outside. Read the scaffold [`README.md`](README.md) (the 0–5 scale, the Orwell bar, the three hook shapes) and [`http-server.md`](http-server.md) (the
> live surface + live-read rules) first; this doc does not restate them.

**Tier today: 1 (Telescreen).** The `/players` snapshot exposes `era` (an integer) — and that is all. Golden-age active/inactive, turns remaining, accumulated
count, and every per-turn yield/growth effect are completely invisible. Era transitions are not streamed; they are only detectable by polling `/players` and
noticing the `era` integer changed between two snapshots up to 5 s apart.

---

## 1. How it works

### 1a. Golden age — state variables
Each `CvPlayer` persists two relevant scalars:
- `m_iGoldenAgeTurns` (`Sources/Engine/CvPlayer.cpp:775` / `CvPlayer.h:1788`, verify) — turns remaining in the current golden age. 0 = not in one.
  Decremented once per turn in `CvPlayer::doTurn` (`Sources/Engine/CvPlayer.cpp:3856`: `changeGoldenAgeTurns(-1)`, verify), but only if the golden age was
  already active at the START of the turn (`bWasGoldenAgeLastTurn` guard at `Sources/Engine/CvPlayer.cpp:3689`, verify).
- `m_iNumUnitGoldenAges` (`CvPlayer.h`, incremented at `Sources/Engine/CvUnit.cpp:9925`, verify) — cumulative count of golden ages triggered by sacrificing
  great-person units; scales the required count for the NEXT such golden age (formula below). Does NOT count building- or event-triggered golden ages.

### 1b. Golden age — length formula
`CvPlayer::getGoldenAgeLength()` (`Sources/Engine/CvPlayer.cpp:9460`, verify):
```
max(1, goldenAgeLength100() * (1 + goldenAgeModifier/100) / 100)
```
where `goldenAgeLength100()` (`Sources/Engine/CvGame.cpp:3263`, verify) = `GC.getGOLDEN_AGE_LENGTH() * GC.getGameSpeedInfo(getGameSpeedType()).getSpeedPercent()`.
`GOLDEN_AGE_LENGTH = 4` (`Assets/XML/GlobalDefines.xml:637`, verify) → default length at normal speed = 4 turns before modifiers.

`getGoldenAgeModifier()` accumulates from:
- Building `GoldenAgeModifier` XML → `processBuilding` → `changeGoldenAgeModifier` (`Sources/Engine/CvPlayer.cpp:7388`, verify).
- Trait `GoldenAgeDurationModifier` → `processTrait` → `changeGoldenAgeModifier` (`Sources/Engine/CvPlayer.cpp:28528`, verify).

### 1c. Golden age — the five trigger paths
**Path 1 — Great-person unit sacrifice** (`Sources/Engine/CvUnit.cpp:9915`, `CvUnit::goldenAge()`, verify):
1. Unit must have `CvUnitInfo::isGoldenAge() == true` (`Sources/Engine/CvUnit.cpp:11082`, verify).
2. `CvPlayer::unitsGoldenAgeReady()` (`Sources/Engine/CvPlayer.cpp:9054`, verify) counts distinct great-person unit TYPES on the map; must be `>=` the required count.
3. Required count = `unitsRequiredForGoldenAge()` (`Sources/Engine/CvPlayer.cpp:9042`, verify): `BASE_GOLDEN_AGE_UNITS + numUnitGoldenAges * GOLDEN_AGE_UNITS_MULTIPLIER`.
   Currently `BASE_GOLDEN_AGE_UNITS = 1`, `GOLDEN_AGE_UNITS_MULTIPLIER = 0` → always 1 unit, regardless of prior unit golden ages.
4. On activation: `killGoldenAgeUnits` sacrifices the required count (`Sources/Engine/CvPlayer.cpp:9089`, verify); `changeGoldenAgeTurns(getGoldenAgeLength())`; `changeNumUnitGoldenAges(1)`.

**Path 2 — Building completion** (`Sources/Engine/CvCity.cpp:14587`, inside `CvCity::processBuilding`, verify):
```cpp
if (kBuilding.isGoldenAge())
    GET_PLAYER(getOwner()).changeGoldenAgeTurns(1 + getGoldenAgeLength());
```
Note `1 + getGoldenAgeLength()` — one extra turn vs. the unit path. Fires during `setHasBuilding` for `<bGoldenAge>1</bGoldenAge>` buildings, only when
`GC.getGame().isFinalInitialized()` (`Sources/Engine/CvCity.cpp:14585`, verify).

**Path 3 — Great-person birth with `GoldenAgeOnBirthOfGreatPerson`** (`Sources/Engine/CvPlayer.cpp:20467-20469`, inside `doGreatPersonBorn`, verify):
```cpp
if (getGoldenAgeOnBirthOfGreatPersonCount(eGreatPersonUnit) > 0)
    changeGoldenAgeTurns(getGoldenAgeLength());
```
`m_goldenAgeOnBirthOfGreatPersonCount` (`CvPlayer.h`, a `std::map<short,char>`) keyed by unit type; set by traits/buildings/civics granting auto-golden-ages on GP birth.

**Path 4 — Random event** (`Sources/Engine/CvPlayer.cpp:21714-21716`, inside `applyEvent`, verify):
```cpp
if (kEvent.isGoldenAge())
    changeGoldenAgeTurns(getGoldenAgeLength());
```
Triggered by an event XML entry with `<bGoldenAge>1</bGoldenAge>`.

**Path 5 — Python / WorldBuilder** via `CyPlayer::setCurrentEra` or direct `changeGoldenAgeTurns` Python exposure (`CvPythonPlayerLoader.cpp`, verify).

### 1d. Golden age — effects during active turns
When `isGoldenAge()` is true, each turn:
- **Base yield bonus per city** — `CvCity::getBaseYieldRate` (`Sources/Engine/CvCity.cpp:22818`, verify) adds `getGoldenAgeYield(eIndex)` per yield type
  (`m_aiGoldenAgeYield[]`, accumulated from XML traits/buildings).
- **Base commerce bonus per city** — `CvCity::getBaseCommerceRateExtra` (`Sources/Engine/CvCity.cpp:11936`, verify) adds `getGoldenAgeCommerce(eIndex)` per commerce type.
- **No anarchy** — `getCivicAnarchyLength()` (`Sources/Engine/CvPlayer.cpp:8942`, verify) and `getReligionAnarchyLength()` (`Sources/Engine/CvPlayer.cpp:9003`, verify) short-circuit to 0.
- **GP rate modifier** — `CvCity::getBaseGreatPeopleRateModifier()` (`Sources/Engine/CvCity.cpp:7164`, verify) adds `GC.getGOLDEN_AGE_GREAT_PEOPLE_MODIFIER()`.
- **Reduced food-for-growth** — `CvPlayer::growthThreshold()` (`Sources/Engine/CvPlayer.cpp:24462`, verify) applies `GOLDEN_AGE_PERCENT_LESS_FOOD_FOR_GROWTH`.
- **AI civic re-evaluation** — `CvPlayerAI::AI_startGoldenAge()` (`Sources/AI/CvPlayerAI.cpp:6253`, verify) resets the civic timer to 0 so the AI immediately reconsiders civics.

### 1e. Golden age — `changeGoldenAgeTurns` transition side-effects
`CvPlayer::changeGoldenAgeTurns` (`Sources/Engine/CvPlayer.cpp:9395`, verify), when the golden-age flag FLIPS:
- On START: `changeAnarchyTurns(-getAnarchyTurns())` (clears anarchy); `AI_startGoldenAge()`; `updateYield()`; `CvEventReporter::goldenAge(getID())` (Python, no HTTP event); `addReplayMessage(...)`.
- On END: `CvEventReporter::endGoldenAge(getID())` (Python, no HTTP event); `updateYield()`.
- Both: `AddDLLMessage` to all met players (in-game notification text only, not HTTP events).

### 1f. Era advance — state variable and trigger
`m_eCurrentEra` (`CvPlayer.h:873`, `DllExport EraTypes getCurrentEra() const`, verify) is a per-player era index.

**Only live trigger site:** `CvTeam::setHasTech` (`Sources/Engine/CvTeam.cpp:5306-5308`, verify):
```cpp
if (player.getCurrentEra() < kTech.getEra())
    player.setCurrentEra((EraTypes)(kTech.getEra()));
```
Runs for every player on the team that researched the tech. Era only increases. There is no era-advance event in the HTTP stream: the Python reporter fires
`techAcquired` (`Sources/Engine/CvTeam.cpp:5323`, verify) but nothing era-specific, and there is no SSE `publishEvent` anywhere in `CvTeam.cpp`.

**`setCurrentEra` side-effects** (`Sources/Engine/CvPlayer.cpp:12414`, verify): heritage commerce era-change deltas applied; per-city free-specialist grant
(`getEraAdvanceFreeSpecialistCount`); graphics/flag dirty-bits; for human players not in network MP, a `BUTTONPOPUP_PYTHON_SCREEN` era-movie popup.

### 1g. Era — indirect effects
Many modifiers scale by the era integer directly: handicap AI per-era bonuses `getAIPerEraModifier() * getCurrentEra()` (~6 sites, e.g.
`Sources/Engine/CvPlayer.cpp:6988, 7934, 7993, 10354`, verify); anarchy length `GC.getEraInfo(getCurrentEra()).getAnarchyPercent()`
(`Sources/Engine/CvPlayer.cpp:8988, 9027`, verify); growth threshold `GC.getEraInfo(getCurrentEra()).getGrowthPercent()` (`Sources/Engine/CvPlayer.cpp:24446`,
verify); event probability `GC.getEraInfo(getCurrentEra()).getEventChancePerTurn()` (`Sources/Engine/CvPlayer.cpp:22492`, verify).

---

## 2. What's on the wire today

**Tier 1 (Telescreen).** Observable:

| Surface | Field / tag | What it gives you |
|---|---|---|
| `GET /players` | `era` (integer) | Current era index, snapshotted every 5 s. Era changes detectable by polling only — no event. |
| `[DAI/begin]` (level 1) | `era=N` | Current era at the start of each AI decision turn, in `DecisionAI.log` and on `/events`. |
| Python callback | `goldenAge(player)` / `endGoldenAge(player)` | Fires on GA start/end — Python-only; does NOT reach the HTTP `/events` SSE stream. |
| Python callback | `techAcquired(tech, team, player, bAnnounce)` | Tech acquisition is Python-observable — again Python-only; no SSE event. |
| Replay message | `REPLAY_MESSAGE_MAJOR_EVENT` | Written on GA start, visible in the replay file — not live-accessible. |

Not exposed: `getGoldenAgeTurns()` (turns remaining), `isGoldenAge()` (the flag, derivable only if turns were exposed), `getNumUnitGoldenAges()` (the
escalation counter), `unitsRequiredForGoldenAge()`/`unitsGoldenAgeReady()` (the "can trigger" readiness), `getGoldenAgeModifier()`/`getGoldenAgeLength()` (the
effective duration), per-player `getGoldenAgeYield[]`/`getGoldenAgeCommerce[]` (the bonus magnitudes), era advance as an event (poll-inferred only), and the
`getEraAdvanceFreeSpecialistCount` grants (silently added on era advance).

---

## 3. The gap

An external reader cannot:
1. Know that any player is in a golden age at all (no field in `/players`).
2. Know when a golden age started or will end (no event; no turns-remaining field).
3. Distinguish which trigger path fired (unit sacrifice vs. building vs. GP-birth vs. event vs. Python).
4. Know how many unit golden ages a player has accumulated (escalation counter).
5. Know the effective golden-age length (the trait + building modifier is hidden).
6. Detect an era advance as an event — only by polling and noticing `era` incremented; the 5 s cadence means two advances within one window can be missed.
7. Know the free-specialist grants applied on era advance.
8. Compute golden-age yield/commerce/growth bonuses without the `getGoldenAgeYield[]`/`getGoldenAgeCommerce[]` arrays.

Under the Orwell bar: an agent watching the wire cannot narrate "player X entered a golden age this turn" or "player Y advanced to era Z" in real time — both
require the screen or stale-snapshot inference.

---

## 4. Proposed hooks

All hooks are one of the three canonical shapes — see [DEC-obs-hook-shapes](../../architecture/decisions.md#dec-obs-hook-shapes). None modify any AI behaviour.

### Hook A — `/players` snapshot: golden-age fields (cheapest win)
In `PlayerSnap` (`Sources/Tools/CvHttpServer.cpp:61`, verify) add `iGoldenAgeTurns`, `iNumUnitGoldenAges`; populate in `publishIfDue`
(`Sources/Tools/CvHttpServer.cpp:~1530`, verify) from `getGoldenAgeTurns()` / `getNumUnitGoldenAges()`; render in `renderPlayers` (`~293`, verify):
```json
"goldenAgeTurns":    <int>,   // getGoldenAgeTurns(); isGoldenAge() = >0
"numUnitGoldenAges": <int>    // getNumUnitGoldenAges()
```
Makes the active/inactive flag + turns remaining readable with zero log cost.

### Hook B — SSE events for GA start/end and era advance (highest-value)
Eliminates polling. Add `publishEvent` calls (already no-ops when the server is off, gated by `CvHttpServer::isEnabled()`):
- **GA start** — beside the existing `CvEventReporter::goldenAge` call (`Sources/Engine/CvPlayer.cpp:9419`, verify): `publishEvent("goldenAgeStart", {"player":N,"turns":N})`.
- **GA end** — beside `CvEventReporter::endGoldenAge` (`Sources/Engine/CvPlayer.cpp:9416`, verify): `publishEvent("goldenAgeEnd", {"player":N})`.
- **Era advance** — in `setCurrentEra` after `m_eCurrentEra = eNewValue` (`Sources/Engine/CvPlayer.cpp:12418`, verify): `publishEvent("eraAdvance", {"player":N,"era":N})`.

### Hook C — `[ERA/*]` gated log lines (`gPlayerLogLevel`)
A new `[ERA]` log via the `logPlayerAI` pattern, level 1:
```
[ERA/goldenAge]    player=N turn=T turns=N trigger=<unit|building|birth|event>
[ERA/goldenAgeEnd] player=N turn=T numUnitGAs=N
[ERA/advance]      player=N turn=T oldEra=N newEra=N tech=TECH_KEY
```
`trigger` needs a small enum/arg passed into `changeGoldenAgeTurns` from each call site (or log at each call site). `tech` is in `setHasTech` context, not
inside `setCurrentEra` — cleanest as a one-liner in `CvTeam::setHasTech` (`Sources/Engine/CvTeam.cpp:5306`, verify) just before the `setCurrentEra` call.

### Hook D — full modifier/yield picture (on-demand)
Add to `/players?verbose=1` or a new `/diagnostic/goldenAgeState?player=N` (mailbox pattern):
```
goldenAgeLength      -- getGoldenAgeLength() (effective duration)
goldenAgeModifier    -- getGoldenAgeModifier() (additive %)
goldenAgeYield[]     -- getGoldenAgeYield(eYield) per yield
goldenAgeCommerce[]  -- getGoldenAgeCommerce(eCommerce) per commerce
unitsGoldenAgeReady  -- unitsGoldenAgeReady() (distinct GP types on the map)
unitsRequiredForGA   -- unitsRequiredForGoldenAge()
```

**Priority:** Hooks A + B bring the system from **Tier 1** to **Tier 3 (Big Brother)** — the snapshot gives the active state, the events give real-time
transitions. **Tier 4 (Thought Police)** additionally needs the yield/modifier scalars (Hook D) to fully reconstruct what a golden age is DOING to a player's
economy turn by turn.

---

## 5. Cascade relevance (#428/#430)

- **Golden-age effects are modifier-cascade channels:** the per-city `getGoldenAgeYield[]`/`getGoldenAgeCommerce[]` bonuses and the GA GP-rate / growth /
  no-anarchy gates are exactly the conditional modifiers the cascade must reproduce when a golden age is active; today none are attributable on the wire.
- **The era integer is a pervasive cascade input:** era gates `requires` atoms and scales anarchy/growth/event modifiers (§1g). An era advance is a player
  state-machine transition the cascade's tally must observe — currently only poll-inferable. Hook B's `eraAdvance` event is the signal.
- **Heritage commerce era-deltas ride the same `setCurrentEra` path** (see [`heritage-score.md`](heritage-score.md) §1c) — both want the era-advance event to
  re-trigger their shadows.
- Hooks A + B are the minimum substrate for a golden-age / era shadow before any legacy maintainer is cut ([DEC-map-before-delete](../../architecture/decisions.md#dec-map-before-delete)).

---

## See also
- [`README.md`](README.md) — the observability scale, the Orwell bar, and the three hook shapes this map applies.
- [`http-server.md`](http-server.md) — the live surface (`/players`, `/events`, `/diagnostic/*`) these hooks extend, and the live-read rules.
- [`../../architecture/decisions.md`](../../architecture/decisions.md) — [DEC-obs-scale], [DEC-obs-hook-shapes], [DEC-map-before-delete].
- [`../../explanation/cascade-architecture.md`](../../explanation/cascade-architecture.md) — why total observability is load-bearing, and the modifier channels golden ages feed.
- [`heritage-score.md`](heritage-score.md) — heritage commerce rides the same `setCurrentEra` era-advance path.
- [`../../README.md`](../../README.md) — the comprehension map (overview-of-overviews).
