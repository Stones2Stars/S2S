# Observability — war weariness

> **Status:** reference (per-system observability map) · **Verified against:** live source, 2026-06-20
> **Grounding:** `Sources/Engine/CvTeam.cpp`, `Sources/Engine/CvUnit.cpp`, `Sources/Engine/CvPlayer.cpp`,
> `Sources/Engine/CvCity.cpp`, `Sources/AI/CvPlayerAI.cpp`, `Sources/Tools/CvHttpServer.cpp`,
> `Assets/XML/GlobalDefines.xml`. Line numbers drift — confirm the named function, not the integer.
> One-paragraph orientation: this maps how war weariness (WW) accrues from combat, decays per turn, and
> converts into per-player then per-city unhappiness anger — and why the whole pipeline is invisible from
> outside the running game. Read the scaffold [`README.md`](README.md) (the 0–5 scale, the Orwell bar, the
> three hook shapes) and [`http-server.md`](http-server.md) (the live surface + live-read rules) first; this
> doc does not restate them.

**Tier today: 1 (Telescreen).** The system has zero dedicated observability surface: no WW field on any
endpoint, no gated WW log tag, no WW `/events` frame. The only indirect external signal is city
happiness/unhappiness — and that is not on any snapshot either. WW is an "inner planet" of the
anger/unhappiness subsystem: any cascade replacement of unhappiness-gated `requires.operate` conditions is
unverifiable without this surface.

---

## 1. How it works

### 1a. Storage — team-scoped matrix, ×100 fixed-point

WW is stored as `m_aiWarWearinessTimes100[MAX_TEAMS]` on `CvTeam` (`Sources/Engine/CvTeam.h:568`). Each entry
is the WW this team has accumulated against one specific enemy team, in 1/100 of a WW "point". Accessors:

- `getWarWeariness(TeamTypes)` — `m_aiWarWearinessTimes100 / 100` (integer points), `Sources/Engine/CvTeam.cpp:3516`.
- `getWarWearinessTimes100(TeamTypes)` — raw fixed-point, `CvTeam.cpp:3521`.

The value is floored at zero on every write (`setWarWearinessTimes100`, `CvTeam.cpp:3535`).

### 1b. Accrual — combat triggers in CvUnit.cpp, espionage in CvPlayer.cpp

Combat accruals go through `changeWarWearinessTimes100(eOtherTeam, kPlot, iFactor)`, which applies a
**culture-ratio scale** before adding to the matrix (`CvTeam.cpp:3554-3575`):

```
iRatio = 100 * theirCulture / (ourCulture + theirCulture)   [0..100]
// Rebel vs its parent: iRatio capped at 40; raw rebel: capped at 60
changeWarWearinessTimes100(eOtherTeam, iRatio * iFactor)
```

So accrual is highest fighting on enemy-cultured turf and ~zero on your own well-cultured land. Factors from
`Assets/XML/GlobalDefines.xml:701-742`:

| Event | Factor constant | Value | Whose WW rises | Notes |
|---|---|---|---|---|
| Attacker unit KILLED in melee | `WW_UNIT_KILLED_ATTACKING` | 3 | Attacker vs defender | Scaled by `(maxHP - preCombatDamage)/maxHP` (`CvUnit.cpp:2834`) |
| Defender loses HP when attacker killed | `WW_KILLED_UNIT_DEFENDING` | 2 | Defender vs attacker | Scaled by `(damage - preCombatDamage)/maxHP` (`CvUnit.cpp:2837`) |
| Defending unit KILLED in melee | `WW_UNIT_KILLED_DEFENDING` | 5 | Defender vs attacker | Scaled by `(maxHP - preCombatDamage)/maxHP` (`CvUnit.cpp:3321-3327`) |
| Attacker loses HP when defender killed | `WW_KILLED_UNIT_ATTACKING` | 1 | Attacker vs defender | Scaled by `(damage - preCombatDamage)/maxHP` (`CvUnit.cpp:3329-3336`) |
| City captured | `WW_CAPTURED_CITY` | 6 | Captor vs city's team | No culture-ratio scale (flat, `CvUnit.cpp:3995`) |
| Unit captured (hidden-nationality only) | `WW_UNIT_CAPTURED` / `WW_CAPTURED_UNIT` | 5 / 1 | Captured's / captor's team | Hidden-nationality branch only (`CvUnit.cpp:13845-13846`) |
| Plot seized by cultured expansion | `WW_PLOT_CAPTURED` / `WW_CAPTURED_PLOT` | 2 / 1 | Loser's / seizer's team | `CvUnit.cpp:24320-24321` |
| Nuke hit | `WW_HIT_BY_NUKE` | 20 | Nuked vs nuker | Flat, `CvUnit.cpp:6606` |
| Nuke used | `WW_ATTACKED_WITH_NUKE` | 10 | Nuker vs nuked | Flat, `CvUnit.cpp:6607` |
| Espionage "cause war weariness" mission | `kMission.getWarWearinessCounter()` | XML per mission | Target city's owner team | Adds to **city-level timer**, not the team matrix (`CvPlayer.cpp:16477-16482`) — separate channel (§1f) |

**City-capture skips the culture-ratio overload.** `CvUnit.cpp:13995` calls `changeWarWeariness(pNewCity->getTeam(), *pNewPlot, WW_CAPTURED_CITY)`,
which routes through `CvTeam::changeWarWeariness(TeamTypes,int)` (`CvTeam.cpp:3538`) →
`changeWarWearinessTimes100(eIndex, 100 * iChange)`, the direct (non-plot) overload — no culture adjustment.
The other combat triggers use the plot overload.

**`WW_UNIT_CAPTURED` only fires for covert captures** — the non-hidden-nationality path is guarded out by
`if (isHiddenNationality() || unitX->isHiddenNationality())` at `CvUnit.cpp:13843`.

`makeAlliance` / `vassalAccepted` also propagate WW: when a team joins another, WW is averaged (`CvTeam.cpp:796`)
or max-propagated (`CvTeam.cpp:892`) between teams.

### 1c. Per-turn decay — CvTeam::doWarWeariness()

`CvTeam::doWarWeariness()` (`CvTeam.cpp:5770`), called unconditionally from `CvTeam::doTurn()` (`CvTeam.cpp:1072`):

```
for each enemy team slot:
    if WW > 0:
        changeWarWeariness(eI, 100 * WW_DECAY_RATE)    // WW_DECAY_RATE = -1 → -1/turn
        if enemy team dead/no-military/peace:
            setWarWeariness(eI, WW * WW_DECAY_PEACE_PERCENT / 100)  // 99% → extra 1% drain
```

Active-war WW decays slowly (−1/turn); peace-or-dead WW melts fast (−1 flat + 1% of remainder).

### 1d. Player-level anger conversion — CvPlayer::updateWarWearinessPercentAnger()

`Sources/Engine/CvPlayer.cpp:10910`, called from `CvPlayer::doTurn()` (`CvPlayer.cpp:3869`) and on
war-declaration / peace-made (`CvTeam.cpp:1419`, `CvTeam.cpp:1641`):

```
for each living non-minor enemy team:
    sum += team.getWarWeariness(enemyTeam) * max(0, 100 + enemyTeam.getEnemyWarWearinessModifier()) / 1_000_000
```

`1_000_000 = 100 * PERCENT_ANGER_DIVISOR(1000)` — result in percent-anger units. The raw sum then passes
through `getModifiedWarWearinessPercentAnger` (`CvPlayer.cpp:10939`):

1. `× BASE_WAR_WEARINESS_MULTIPLIER` (= 5, `GlobalDefines.xml:926`)
2. `× (100 + MULTIPLAYER_WAR_WEARINESS_MODIFIER) / 100` if multiplayer (= 1, ~no change)
3. `× (100 + worldSizeModifier) / 100` — `CvWorldInfo::getWarWearinessModifier()`, scales by world size
4. AI players only: `× (100 + AIWarWearinessPercent + AIPerEraModifier × era) / 100` — handicap scaling
   (`CvHandicapInfo`), reduces AI WW at harder difficulties (`CvPlayer.cpp:10953`)

Stored in `m_iWarWearinessPercentAnger` (per-player scalar, `CvPlayer.h:1876`).

### 1e. City-level WW anger — CvCity::getWarWearinessPercentAnger()

`Sources/Engine/CvCity.cpp:5467` — three multiplicative layers:

```
iAnger  = player.getWarWearinessPercentAnger()                                      // §1d scalar
iAnger *= max(0, city.warWearinessModifier + player.warWearinessModifier + 100)/100 // building+civic mods
iAnger *= max(0, city.warWearinessTimer + 100) / 100                                // espionage timer
```

This feeds `CvCity::unhappyLevel()` (`CvCity.cpp:5614`) via `iAngerPercent`, then
`iUnhappiness += (iAngerPercent * population) / PERCENT_ANGER_DIVISOR(1000)`.

### 1f. Espionage city-timer — separate channel

`kMission.getWarWearinessCounter()` adds to `m_iWarWearinessTimer` on the **target city** (`CvPlayer.cpp:16482`),
scaled by game speed. This is NOT added to the team WW matrix — it operates purely through the city's
multiplier layer (§1e) and decays −20/turn in `CvCity::doWarWeariness` (`CvCity.cpp:21747-21751`, called from
`CvCity::doTurn` at `CvCity.cpp:1322`). The AI values the mission by the delta in city
`getWarWearinessPercentAnger` before/after (`Sources/AI/CvPlayerAI.cpp:15903-15906`).

### 1g. Modifier sources

| Source | Modifier affected | Applied via |
|---|---|---|
| Building `getWarWearinessModifier` | Per-city WW modifier (`m_iWarWearinessModifier`) | `CvCity::processBuilding` (`CvCity.cpp:4628`) |
| Building `getGlobalWarWearinessModifier` | Per-player WW modifier | `CvPlayer::processBuilding` (`CvPlayer.cpp:7391`) |
| Building `getEnemyWarWearinessModifier` | Enemy team's modifier (`m_iEnemyWarWearinessModifier`) | `CvTeam::processBuilding` (`CvTeam.cpp:1011`) |
| Civic `getWarWearinessModifier` | Per-player WW modifier | `CvPlayer::applyCivics` (`CvPlayer.cpp:18227`) |
| Trait `getEnemyWarWearinessModifier` | National enemy-WW modifier | `CvPlayer::processTrait` (`CvPlayer.cpp:28532`) |
| Handicap `getAIWarWearinessPercent` | AI-player multiplier (`isNormalAI()` only) | `getModifiedWarWearinessPercentAnger` (`CvPlayer.cpp:10953`) |
| World size `getWarWearinessModifier` | All-player global multiplier | `getModifiedWarWearinessPercentAnger` (`CvPlayer.cpp:10947`) |

---

## 2. What's on the wire today

**Tier 1 (Telescreen).** No endpoint, log tag, or event exposes any WW state. The closest indirect signals:

| Endpoint / log | Field | Limitation |
|---|---|---|
| `/players` | `score` | No WW component isolated |
| `/cities` | `population`, `food`, `production`, `commerce` | No unhappiness, no WW anger |
| `/events` `log` frame | `[CIT/proplevel]` | Crime/disease/education properties only, not WW |
| `/events` `playerTurnStart/End` | phase signal | No WW content |

None of these expose WW state, WW anger, WW modifiers, or unhappiness.

---

## 3. The gap

An agent on the HTTP + `/events` surface today **cannot** reconstruct any of:

1. **Team-level raw WW** — `getWarWeariness(enemyTeam)` for every (team, enemy-team) pair; the accrual/decay heartbeat is invisible.
2. **Per-player WW percent anger** — `getWarWearinessPercentAnger()`, the scalar feeding city unhappiness.
3. **Per-city WW modifier** — building contributions amplifying/dampening WW for that city.
4. **Per-player WW modifier** — civic + global-building contributions.
5. **Enemy team WW modifier** — `getEnemyWarWearinessModifier()`: trait/building scaling of how much WW your victories create.
6. **Per-city WW timer** — the espionage-planted transient boost (−20/turn).
7. **City unhappiness breakdown** — `unhappyLevel()` total and per-source split (WW, overcrowding, religion…).
8. **WW accrual / decay events** — no `/events` frame fires on a combat accrual; per-turn decay is silent.
9. **Culture-ratio weighting** — `iRatio * iFactor` per-plot, never logged.

The consequence for the Orwell bar: any cascade replacement of unhappiness-gated `requires.operate`
conditions (e.g. property-effect buildings responding to city morale) is unverifiable without this surface.

---

## 4. Proposed hooks

All hooks are one of the three canonical shapes — see
[DEC-obs-hook-shapes](../../architecture/decisions.md#dec-obs-hook-shapes).

### Hook A — `/players` + `/cities` snapshot fields (highest leverage)

Add to `PlayerSnap` (`Sources/Tools/CvHttpServer.cpp`):
```json
"warWearinessPercentAnger": <int>,   // CvPlayer::getWarWearinessPercentAnger()
"warWearinessModifier":     <int>    // CvPlayer::getWarWearinessModifier() — civic+global-building mod
```
Add to `CitySnap`:
```json
"warWearinessPercentAnger": <int>,   // CvCity::getWarWearinessPercentAnger() — final per-city WW anger
"warWearinessModifier":     <int>,   // CvCity::getWarWearinessModifier() — building mod
"warWearinessTimer":        <int>,   // CvCity::getWarWearinessTimer() — espionage transient boost
"unhappyLevel":             <int>,   // CvCity::unhappyLevel(0) — total (WW is one component)
"happyLevel":               <int>    // CvCity::happyLevel(0) — for net happy/unhappy
```
Populate in the existing per-player/per-city snapshot loop. All `O(1)` reads on the game thread; no search.
The single highest-value lift — `warWearinessPercentAnger` on `CitySnap` is the minimum for cascade
verification (see §5).

### Hook B — `/diagnostic/warWeariness?player=N` endpoint

Mailbox-evaluated (the `canConstruct`/`placementSweep` contract) — returns the full per-enemy-team matrix,
too large for the 5-second snapshot:
```json
{
  "player": 1,
  "warWearinessPercentAnger": 42,
  "warWearinessModifier": -20,
  "enemyWarWearinessModifier": 15,
  "perTeam": [ {"enemyTeam": 2, "ww": 1200, "wwTimes100": 120000} ]
}
```
Source: `GET_TEAM(getTeam()).getWarWeariness(eI)` over all `MAX_PC_TEAMS`, filtering zero entries.

### Hook C — `[WWA]` event spine (war-weariness accrual)

New `WarWeariness.log` (following the `BetterBTSAI.{h,cpp}` log-helper pattern under `Sources/AI/`), tag
`[WWA]`, gated by `gTeamLogLevel`; level-1 lines tee to `/events` via `streamLogTee`.

| Tag | Level | Where | Payload |
|---|---|---|---|
| `[WWA/combat]` | 1 (city-capture, nuke), 2 (unit-kill) | each `changeWarWearinessTimes100` call in `CvUnit.cpp` | `turn= team= vs= event=unitKilled\|cityCaptured\|nuke\|… delta= total= ratio=` |
| `[WWA/decay]` | 2 | `CvTeam::doWarWeariness()` (`CvTeam.cpp:5770`), per team-pair | `turn= team= vs= before= after= mode=war\|peace\|dead` |
| `[WWA/anger]` | 1 | `CvPlayer::updateWarWearinessPercentAnger()` on change (`CvPlayer.cpp:10932`) | `turn= player= old= new=` |
| `[WWA/espio]` | 1 | espionage WW-timer branch (`CvPlayer.cpp:16482`) | `turn= player= city= amount=` |
| `[WWA/timer]` | 3 | `CvCity::doWarWeariness()` (`CvCity.cpp:21747`), per city with timer | `turn= city= timer=` |

`[WWA/anger]` and `[WWA/espio]` are the state-change events worth teeing to `/events` (level 1); the per-turn
decay and per-unit-kill lines are level 2 and file-only. The per-team matrix is left to Hook B, not the
snapshot (a `MAX_TEAMS × MAX_TEAMS` table would bloat every publish).

**Priority:** Hook **A** city `warWearinessPercentAnger` + Hook **C** `[WWA/anger]` together unblock the bar
(turn-by-turn city WW anger vs the cascade's expectation, all players) → rest of Hook A → Hook B (on-demand
matrix) → remaining Hook C tags.

---

## 5. Cascade relevance (#428/#430)

WW is an opaque system feeding city unhappiness (city morale → production choice → AI war decisions) whose
internal state is invisible; it needs observability before any cascade hard-switch
([DEC-map-before-delete](../../architecture/decisions.md#dec-map-before-delete)). The WW modifier sources
(§1g) are `modifier-cascade` targets (per-city / per-player / enemy-team channels), and the WW anger value
must be verifiable before any cascade replacement of `requires.operate` conditions that gate on city morale.
Minimum viable substrate: `warWearinessPercentAnger` on `CitySnap` (Hook A) + `[WWA/anger]` events (Hook C) —
enough to shadow city WW anger against the cascade's modelled expectation, city by city, turn by turn.

---

## See also
- [`README.md`](README.md) — the observability scale, the Orwell bar, and the three hook shapes this map applies.
- [`http-server.md`](http-server.md) — the live surface (`/players`, `/cities`, `/diagnostic/*`) these hooks extend, and the live-read rules.
- [`../../architecture/decisions.md`](../../architecture/decisions.md) — [DEC-obs-scale], [DEC-obs-hook-shapes], [DEC-map-before-delete].
- [`../../explanation/cascade-architecture.md`](../../explanation/cascade-architecture.md) — why total observability is load-bearing, and the cascade model the WW modifiers feed.
- [`../../README.md`](../../README.md) — the comprehension map (overview-of-overviews).
