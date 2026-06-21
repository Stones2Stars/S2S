# War Weariness — Observability Map

> DRAFT observability map (2026-06-18 by parent agent) — claims cited from live code; verify before relying.

Companion docs: [`../../../plans/cascade-mapping-inventory.md`](../../../plans/cascade-mapping-inventory.md) (the Orwell bar + Observability Scale),
[`../http-server.md`](../http-server.md) (live endpoints), [`../ai-logging-reference.md`](../ai-logging-reference.md) (log tag taxonomy).

---

## 1. How war weariness actually works

### 1a. Storage — team-scoped matrix, ×100 fixed-point

War weariness is stored as `m_aiWarWearinessTimes100[MAX_TEAMS]` on `CvTeam`
(`CvTeam.h:568`). Each entry is the WW this team has accumulated against one specific
enemy team, in units of 1/100 of a WW "point". Accessors:

- `getWarWeariness(TeamTypes)` — returns `m_aiWarWearinessTimes100 / 100` (integer WW
  points). `CvTeam.cpp:3516`.
- `getWarWearinessTimes100(TeamTypes)` — raw fixed-point value. `CvTeam.cpp:3521`.

The value is floored at zero on every write (`setWarWearinessTimes100`: `CvTeam.cpp:3535`).

### 1b. Accrual — six trigger sites in CvUnit.cpp, one in CvPlayer.cpp (espionage)

All accruals go through `changeWarWearinessTimes100(eOtherTeam, kPlot, iFactor)`, which
applies a **culture-ratio scale** before adding to the matrix (`CvTeam.cpp:3554-3575`):

```
iRatio = 100 * theirCulture / (ourCulture + theirCulture)   [0..100]
// Rebel vs its parent: iRatio capped at 40; raw rebel: capped at 60
changeWarWearinessTimes100(eOtherTeam, iRatio * iFactor)
```

So WW accrual is highest when fighting on territory dominated by the enemy's culture
(their home turf) and approaches zero on your own well-cultured land.

Factors from `Assets/XML/GlobalDefines.xml:701-742`:

| Event | Factor constant | Value | Whose WW rises | Notes |
|---|---|---|---|---|
| Attacker unit KILLED in melee | `WW_UNIT_KILLED_ATTACKING` | 3 | Attacker's team vs defender's team | Scaled by `(maxHP - preCombatDamage)/maxHP` (`CvUnit.cpp:2834`) |
| Defender loses HP when attacker killed | `WW_KILLED_UNIT_DEFENDING` | 2 | Defender's team vs attacker's team | Scaled by `(damage - preCombatDamage)/maxHP` (`CvUnit.cpp:2837`) |
| Defending unit KILLED in melee | `WW_UNIT_KILLED_DEFENDING` | 5 | Defender's team vs attacker's team | Scaled by `(maxHP - preCombatDamage)/maxHP` (`CvUnit.cpp:3321-3327`) |
| Attacker loses HP when defender killed | `WW_KILLED_UNIT_ATTACKING` | 1 | Attacker's team vs defender's team | Scaled by `(damage - preCombatDamage)/maxHP` (`CvUnit.cpp:3329-3336`) |
| City captured | `WW_CAPTURED_CITY` | 6 | Captor's team vs city's team | No culture-ratio scale call (flat, `CvUnit.cpp:3995`) |
| Unit captured (hidden-nationality only) | `WW_UNIT_CAPTURED` / `WW_CAPTURED_UNIT` | 5 / 1 | Captured's / captor's team | Hidden-nationality branch only (`CvUnit.cpp:13845-13846`) |
| Plot seized by cultured expansion | `WW_PLOT_CAPTURED` / `WW_CAPTURED_PLOT` | 2 / 1 | Loser's / seizer's team | `CvUnit.cpp:24320-24321` |
| Nuke hit | `WW_HIT_BY_NUKE` | 20 | Nuked team vs nuker | Flat, `CvUnit.cpp:6606` |
| Nuke used | `WW_ATTACKED_WITH_NUKE` | 10 | Nuker's team vs nuked | Flat, `CvUnit.cpp:6607` |
| Espionage "cause war weariness" mission | `kMission.getWarWearinessCounter()` | XML-defined per mission | Target city's owner team | Adds to **city-level timer**, not team WW matrix (`CvPlayer.cpp:16477-16482`) — a separate mechanism |

**Important: the city-captured trigger does NOT go through the culture-ratio overload.**
`CvUnit.cpp:13995` calls `changeWarWeariness(pNewCity->getTeam(), *pNewPlot, GC.getDefineINT("WW_CAPTURED_CITY"))`.
Tracing: `CvTeam::changeWarWeariness(TeamTypes,int)` (`CvTeam.cpp:3538`) →
`changeWarWearinessTimes100(eIndex, 100 * iChange)` — the direct (non-plot) overload, so
no culture-ratio adjustment. The other combat triggers use the plot overload.

**Note on `WW_UNIT_CAPTURED`:** the non-hidden-nationality path for unit capture does NOT
accrue WW (the `if (isHiddenNationality() || unitX->isHiddenNationality())` guard at
`CvUnit.cpp:13843`). Only covert captures trigger it.

### 1c. Accrual triggers NOT yet noted in WW literature

`makeAlliance` / `vassalAccepted`: when a team joins another via alliance or vassalage,
WW is averaged/propagated between teams (`CvTeam.cpp:796` average, `CvTeam.cpp:892`
max-propagation).

### 1d. Per-turn decay — CvTeam::doWarWeariness(), called from CvTeam::doTurn()

`CvTeam::doWarWeariness()` (`CvTeam.cpp:5770`), called unconditionally from
`CvTeam::doTurn()` (`CvTeam.cpp:1072`):

```
for each enemy team slot:
    if WW > 0:
        changeWarWeariness(eI, 100 * WW_DECAY_RATE)    // WW_DECAY_RATE = -1 → subtracts 1/turn
        if enemy team dead/no-military/peace:
            setWarWeariness(eI, WW * WW_DECAY_PEACE_PERCENT / 100)  // 99% → extra 1% drain
```

With `WW_DECAY_RATE = -1`, each war-relation decays by 1 WW point per turn
(100/100 in fixed-point), then an additional multiplicative 1% drain kicks in when the
enemy has no military, is dead, or when peace has been made. In practice: active-war WW
decays slowly (−1/turn), peace-or-dead WW melts quickly (−1 flat + 1% of remainder).

### 1e. Player-level anger conversion — CvPlayer::updateWarWearinessPercentAnger()

`CvPlayer.cpp:10910`, called from `CvPlayer::doTurn()` (`CvPlayer.cpp:3869`) and also on
war-declaration and peace-made (`CvTeam.cpp:1419`, `CvTeam.cpp:1641`):

```
for each living non-minor enemy team:
    sum += team.getWarWeariness(enemyTeam) * max(0, 100 + enemyTeam.getEnemyWarWearinessModifier()) / 1_000_000
```

`1_000_000` = `100 * PERCENT_ANGER_DIVISOR(1000)` — the result is in "percent-anger" units
(the same units used by other anger sources in `unhappyLevel`). The raw sum is then passed
through `getModifiedWarWearinessPercentAnger` (`CvPlayer.cpp:10939`):

1. `× BASE_WAR_WEARINESS_MULTIPLIER` (= 5, XML:926)
2. `× (100 + MULTIPLAYER_WAR_WEARINESS_MODIFIER) / 100` if multiplayer (= 1, essentially no change)
3. `× (100 + worldSizeModifier) / 100` — `CvWorldInfo::getWarWearinessModifier()`, scales WW
   up or down by world size
4. For AI-controlled players only: `× (100 + AIWarWearinessPercent + AIPerEraModifier × era) / 100`
   — handicap scaling (`CvHandicapInfo`); reduces AI WW at harder difficulties

Result is stored in `m_iWarWearinessPercentAnger` (per-player scalar, `CvPlayer.h:1876`).

### 1f. City-level WW angle — CvCity::getWarWearinessPercentAnger()

`CvCity.cpp:5467`:

```
iAnger = player.getWarWearinessPercentAnger()
iAnger *= max(0, city.warWearinessModifier + player.warWearinessModifier + 100) / 100
iAnger *= max(0, city.warWearinessTimer + 100) / 100
```

Three multiplicative layers:
1. **Player-level anger** (the computed scalar from §1e)
2. **Per-city + per-player WW modifier** — summed building modifiers on this city
   (`m_iWarWearinessModifier`, `CvCity.cpp:495`) and player-wide building/civic modifiers
   (`m_iWarWearinessModifier`, `CvPlayer.h:1877`)
3. **Per-city WW timer** — a decaying boost set by espionage missions
   (`m_iWarWearinessTimer`, `CvCity.h`, decays −20/turn in `CvCity::doWarWeariness`,
   `CvCity.cpp:21747-21751`, called from `CvCity::doTurn` at `CvCity.cpp:1322`)

This per-city anger value (`getWarWearinessPercentAnger()`) feeds directly into
`CvCity::unhappyLevel()` (`CvCity.cpp:5614`) via `iAngerPercent` and then:

```
iUnhappiness += (iAngerPercent * population) / PERCENT_ANGER_DIVISOR(1000)
```

### 1g. Modifier sources

| Source | Modifier affected | Applied via |
|---|---|---|
| Building `getWarWearinessModifier` | Per-city WW modifier (`m_iWarWearinessModifier`) | `CvCity::processBuilding` (`CvCity.cpp:4628`) |
| Building `getGlobalWarWearinessModifier` | Per-player WW modifier (`m_iWarWearinessModifier`) | `CvPlayer::processBuilding` (`CvPlayer.cpp:7391`) |
| Building `getEnemyWarWearinessModifier` | Enemy team's modifier (`m_iEnemyWarWearinessModifier`) | `CvTeam::processBuilding` (`CvTeam.cpp:1011`) |
| Civic `getWarWearinessModifier` | Per-player WW modifier | `CvPlayer::applyCivics` (`CvPlayer.cpp:18227`) |
| Trait `getEnemyWarWearinessModifier` | National enemy-WW modifier (`m_iNationalEnemyWarWearinessModifier`) | `CvPlayer::processTrait` (`CvPlayer.cpp:28532`) |
| Handicap `getAIWarWearinessPercent` | AI-player multiplier (only for `isNormalAI()`) | `getModifiedWarWearinessPercentAnger` (`CvPlayer.cpp:10953`) |
| World size `getWarWearinessModifier` | All-player global multiplier | `getModifiedWarWearinessPercentAnger` (`CvPlayer.cpp:10947`) |

### 1h. Espionage city-timer — separate channel

`kMission.getWarWearinessCounter()` (`CvEspionageMissionInfo`) adds to `m_iWarWearinessTimer` on
the **target city** (`CvPlayer.cpp:16482`), scaled by game speed. This is NOT added to the team
WW matrix — it operates purely through the city's per-city multiplier layer (§1f). The AI values
this espionage mission by comparing the delta in city `getWarWearinessPercentAnger` before/after
(`CvPlayerAI.cpp:15903-15906`).

---

## 2. Current observability — tier and surface

**Current tier: 1 (Telescreen).** The system has zero dedicated observability surface.
The HTTP endpoints expose no WW field at any level. No gated log tag exists for WW. There
are no WW events in the `/events` stream. The only indirect signal visible externally is
city happiness/unhappiness, which is not included in any snapshot field either.

### 2a. What IS already exposed (by endpoint)

| Endpoint / log | Field | What it tells you | Limitation |
|---|---|---|---|
| `/players` | `score` | Gross player performance | No WW component isolated |
| `/cities` | `population`, `food`, `production`, `commerce` | City economic state | No unhappiness, no WW anger |
| `/events` `log` frame | `[CIT/proplevel]` | Per-city crime/disease/education property values | Only properties, not WW anger |
| `/events` `playerTurnStart/End` | phase signal | Confirms the player's turn ran | No WW content |

None of these expose WW state, WW anger, WW modifiers, or unhappiness.

### 2b. What is NOT exposed (the gap)

The following WW-related values cannot be reconstructed from outside the game today:

1. **Team-level raw WW accumulation** — `getWarWeariness(enemyTeam)` / `getWarWearinessTimes100(enemyTeam)` for every (team, enemy-team) pair. The accrual-and-decay heartbeat is invisible.
2. **Per-player WW percent anger** — `getWarWearinessPercentAnger()`: the converted scalar feeding city unhappiness. No field anywhere.
3. **Per-city WW modifier** — `getWarWearinessModifier()` on a city: building contributions that amplify or dampen WW for that city.
4. **Per-player WW modifier** — `getWarWearinessModifier()` on a player: civic + global-building contributions.
5. **Enemy team WW modifier** — `getEnemyWarWearinessModifier()` on each team: trait + building contributions that scale how much WW your victories create for the enemy.
6. **Per-city WW timer** — `getWarWearinessTimer()`: espionage-planted transient anger boost, decaying 20/turn. No field, no event.
7. **City unhappiness breakdown** — `unhappyLevel()` total and each contributing source (WW, overcrowding, religion, etc.) are not exposed. Only the final city state (production, food) leaks out; not the morale picture.
8. **WW accrual event** — when a combat event triggers `changeWarWearinessTimes100`, no `/events` frame fires. Per-turn decay is similarly silent.
9. **Culture-ratio weighting on accrual** — `iRatio * iFactor` computed per-plot; never logged.

---

## 3. The gap — what cannot be reconstructed from outside

An agent monitoring the HTTP + `/events` stream today **cannot**:

- Know what the WW level is for any team against any other (not in `/players`).
- Know what the converted anger scalar is for any player (`getWarWearinessPercentAnger()`).
- Know what city-level WW modifiers exist (so even if player-level anger were known, the city-level anger still could not be reconstructed without city and player modifier fields).
- Know when a combat, capture, or nuke event caused WW to spike.
- Know whether espionage placed a WW timer boost on a city.
- Reconstruct the unhappiness picture for any city (needed to understand city morale, production choices, revolt risk).

The consequence for the Orwell bar: **WW is an "inner planet" of the anger/unhappiness subsystem**. Any cascade-replacement of unhappiness-based `requires.operate` conditions (e.g. property-effect buildings that respond to city morale state) is unverifiable without this surface.

---

## 4. Proposed hooks — concrete additions to reach Tier 4

All hooks follow the three canonical observability hook shapes — see [DEC-obs-hook-shapes](../../decisions.md#dec-obs-hook-shapes).

### 4a. Snapshot fields — `/players` and `/cities` additions

Add to `PlayerSnap` (`CvHttpServer.cpp:61`):
```
int iWarWearinessPercentAnger;   // CvPlayer::getWarWearinessPercentAnger()
int iWarWearinessModifier;       // CvPlayer::getWarWearinessModifier() — civic+global-building mod
```

Add to `CitySnap` (`CvHttpServer.cpp:83`):
```
int iWarWearinessPercentAnger;   // CvCity::getWarWearinessPercentAnger() — final per-city WW anger
int iWarWearinessModifier;       // CvCity::getWarWearinessModifier() — building mod for this city
int iWarWearinessTimer;          // CvCity::getWarWearinessTimer() — espionage transient boost
int iUnhappyLevel;               // CvCity::unhappyLevel(0) — total unhappiness (WW is one component)
int iHappyLevel;                 // CvCity::happyLevel(0) — total happiness (for net happy/unhappy)
```

Populate in `publishIfDue` during the existing per-player/per-city snapshot loop
(`CvHttpServer.cpp:1542-1578`). These are all `O(1)` reads on the game thread; no search.

Add a new `/diagnostic/warWeariness?player=N` endpoint (or include in the
`/players?playerNumber=N` response) that returns the full per-enemy-team WW matrix:

```json
{
  "player": 1,
  "warWearinessPercentAnger": 42,
  "warWearinessModifier": -20,
  "enemyWarWearinessModifier": 15,
  "perTeam": [
    {"enemyTeam": 2, "ww": 1200, "wwTimes100": 120000},
    ...
  ]
}
```

Source: `GET_TEAM(getTeam()).getWarWeariness(eI)` over all `MAX_PC_TEAMS`; filter
zero-entries. This should be a diagnostic-endpoint rather than part of the snapshot
(the full matrix is rarely needed; the player-level scalar belongs in the snapshot).

### 4b. Event spine — WW accrual and decay events

Register a new `[WWA]` (war-weariness-accrual) tag, gated by `gTeamLogLevel`, file
`WarWeariness.log` (new log file following existing pattern in `BetterBTSAI.{h,cpp}`):

**Combat accrual** — at each `changeWarWearinessTimes100` call in `CvUnit.cpp`:
```
[WWA/combat] turn=N team=T vs=V event=unitKilled|unitKilledDef|captured|cityCaptured|nuke
             delta=DDDD total=TTTT ratio=RR
```
Level 1 for city-capture and nuke events; level 2 for unit-kill events.

**Per-turn decay** — in `CvTeam::doWarWeariness()` (`CvTeam.cpp:5770`), once per team
per turn:
```
[WWA/decay] turn=N team=T vs=V before=BBB after=AAA mode=war|peace|dead
```
Level 2 (verbose, fires every turn per war-relation; gate at level 2 to avoid spam).

**Anger update** — in `CvPlayer::updateWarWearinessPercentAnger()`, when the value
changes (`CvPlayer.cpp:10932`):
```
[WWA/anger] turn=N player=P old=OO new=NN
```
Level 1 (only fires on change; not every turn).

**Espionage city timer** — in `CvPlayer::doEspionageMission` at the WW-timer branch
(`CvPlayer.cpp:16482`):
```
[WWA/espio] turn=N player=P city=C amount=AA
```
Level 1.

**City timer decay** — in `CvCity::doWarWeariness()` (`CvCity.cpp:21747`):
```
[WWA/timer] turn=N city=C timer=TT
```
Level 3 (fires every turn per city with a timer; very verbose).

The `[WWA/anger]` and city-timer events tee onto `/events` via `streamLogTee` (they are
state-change events, so level 1 is appropriate for the stream). The per-turn decay and
per-unit-kill lines are level 2 and only go to the file.

### 4c. No new endpoints for team-level decay

The per-team WW matrix belongs in the on-demand `/diagnostic/warWeariness?player=N`
endpoint (§4a), not in the 5-second snapshot — the matrix is a full `MAX_TEAMS × MAX_TEAMS`
table and would bloat every snapshot. The log events (§4b) cover the turn-by-turn stream.

---

## 5. Tier assessment

| Tier | Description | Met after proposed hooks? |
|---|---|---|
| 1 | Coarse snapshots | Yes (current) — but no WW in them |
| 2 | + buildability shadows | Orthogonal to WW |
| 3 | + live event stream + maintainer shadows | With §4b events added: WW accrual / anger changes are live in `/events` |
| 4 | + every maintainer shadowed for all players | With §4a fields + §4b events: WW state fully reconstructible per player/city + per-team WW via diagnostic endpoint |
| 5 | Total — §A opaque systems included | WW would be **complete at Tier 5 level** once §4a + §4b land |

**Current tier: 1.** With snapshot fields (§4a) and at minimum `[WWA/anger]` events (§4b),
the system reaches **Tier 3** (anger changes visible in stream) instantly. Full §4a + §4b +
the diagnostic matrix endpoint reaches **Tier 4/5** for war weariness specifically.

---

## 6. Priority — HIGH

War weariness is a **§A opaque system** by the cascade-mapping-inventory definition: it
affects city unhappiness (city morale → production choice → AI behaviour → war decisions),
but its internal state is currently invisible. Before any cascade replacement of
`requires.operate` conditions that gate on city morale or anger, the WW anger value must be
verifiable. **This is the verification substrate concern, not cosmetics.**

Minimum viable for cascade verification: add `iWarWearinessPercentAnger` to `CitySnap`
(§4a city field) and add `[WWA/anger]` events (§4b). Those two additions alone let an agent
track whether city WW anger agrees with the cascade's modelled expectation, city by city,
turn by turn, for all players.
