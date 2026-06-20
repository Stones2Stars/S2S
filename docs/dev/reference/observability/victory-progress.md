# Observability — victory-conditions progress

> **Status:** reference (per-system observability map) · **Verified against:** live source, 2026-06-20
> **Grounding:** `Sources/Engine/CvGame.cpp`, `Sources/Engine/CvTeam.cpp`, `Sources/Tools/CvHttpServer.cpp`.
> Line numbers drift — confirm the named function, not the integer.
> One-paragraph orientation: this maps how the per-turn victory check evaluates every victory type, runs
> countdowns, and concludes the game — and why all of it (countdowns, spaceship-launched, thresholds, mastery
> score, mercy-rule, the winner itself) is invisible from outside the running game. Read the scaffold
> [`README.md`](README.md) (the 0–5 scale, the Orwell bar, the three hook shapes) and
> [`http-server.md`](http-server.md) (the live surface + live-read rules) first; this doc does not restate them.

**Scope:** all victory types — Time/Score, Conquest, Domination, Cultural, Religious, Scientific/Building,
Space Race, Diplomatic (UN vote), Mastery (total-victory) — and the Mercy Rule countdown.
**Tier today: 1 (Telescreen).** A coarse per-player `score` snapshot exists; the victory conditions,
countdowns, and per-condition progress are entirely invisible.

---

## 1. How it works

### 1a. The per-turn victory check loop

`CvGame::testVictory()` (`Sources/Engine/CvGame.cpp:7696`) fires every turn from `CvGame::doTurn` via
`game.testVictory` (`CvGame.cpp:6106`). It bails early if a winner is decided, if the game is EXTENDED, or if
fewer than `speedPercent/10` turns have elapsed (early-game grace, `CvGame.cpp:7700-7703`). It calls
`updateScore()`, then iterates every alive non-minor `CvTeam × VictoryTypes`, calling the single-condition
overload `testVictory(VictoryTypes, TeamTypes, bool* pbEndScore)` (`CvGame.cpp:7504`).

### 1b. Per-victory-type conditions (all in the `testVictory` overload, `CvGame.cpp:7504-7693`)

| Victory type | Condition (abbreviated) | Key fields read |
|---|---|---|
| **Time / End Score** | `isEndScore()`: `getElapsedGameTurns() >= getMaxTurns()` AND uniquely highest `getTeamScore` | `getElapsedGameTurns`, `getMaxTurns`, `getTeamScore` |
| **Score** | `isTargetScore()`: `getTeamScore >= getTargetScore()` AND uniquely highest | `getTeamScore`, `getTargetScore` |
| **Conquest** | `isConquest()`: no other alive non-vassal team has any cities | `getNumCities`, `isVassal` |
| **Diplomatic** | `isDiploVote()`: a victory `VoteInfo`'s `getVoteOutcome` equals this team | `m_paiVoteOutcome[VoteTypes]` |
| **Domination** | `getAdjustedPopulationPercent(eVictory) > 0` (hold X% world pop) AND `getAdjustedLandPercent(eVictory) > 0` (hold Y% land) | `getTotalPopulation`, `getTotalLand`, `getLandPlots` |
| **Religious** | `getReligionPercent() > 0`: holy city for a religion ≥N% world pop AND `getNumCivCities() > countCivPlayersAlive()*2` | `hasHolyCity`, `calculateReligionPercent`, `getNumCivCities` |
| **Cultural** | `getCityCulture() != NO_CULTURELEVEL && getNumCultureCities() > 0`: count team cities at/above the `CultureLevelTypes` | `getCultureLevel`, `getNumCultureCities` |
| **Scientific/Building** | `getVictoryThreshold(eVictory) > 0` per building: team has `≥ threshold` of it | `getBuildingCount` |
| **Space Race** | every `CvProjectInfo` with `getVictoryMinThreshold > 0` met by `getProjectCount`; sets `starshipLaunched[eTeam]=true` on first pass | `getProjectCount`, `starshipLaunched[]` |

`starshipLaunched[eTeam]` is a `bool` array on `CvGame` (`CvGame.h:286` — `getStarshipLaunched(int ID)`). It
is set the first time Space conditions pass and latches: the `!starshipLaunched[eTeam]` gate
(`CvGame.cpp:7678-7691`) means the second pass skips the project check and still returns `bValid = true`.

### 1c. Victory countdown and delay (`CvGame.cpp:7718-7744`, `CvTeam.cpp:4898-4947`)

When `testVictory(…,eTeam)` returns `true`:

1. If `getVictoryCountdown(eVictory) < 0` AND `getVictoryDelay(eVictory) == 0`, the countdown initializes to `0` immediately (`CvGame.cpp:7725`).
2. If `getVictoryDelay > 0`: `setVictoryCountdown(eVictory, delay)` and the countdown ticks −1/turn while the condition holds.
3. At `getVictoryCountdown == 0`: an RNG check `SorenRandNum(100) < getLaunchSuccessRate` gates the win (`CvGame.cpp:7736`).
4. On failure: `resetVictoryProgress()` clears all countdowns and erases space-project counts (`CvTeam.cpp:4981-4997`).

`getVictoryDelay(VictoryTypes)` (`CvTeam.cpp:4922-4947`): base = `VictoryInfo.getVictoryDelayTurns() × speedPercent / 100`
(`CvGame.cpp:3272`); partially-complete space projects extend it by `getVictoryDelayPercent()`.
`getLaunchSuccessRate(VictoryTypes)` (`CvTeam.cpp:4960-4978`): starts at 100, reduced by
`successRate × (threshold − count)` per under-count project; returns 0 if any project is below `minThreshold`.

### 1d. Mastery / Total Victory (`CvGame.cpp:7750-7768`)

If a `VictoryTypes` has `isTotalVictory()` and is valid, **all** other winner candidates are cleared; the
winner is chosen by `getTotalVictoryScore()` (`CvTeam.cpp:7332-7487`) when `getMaxTurns > 0 &&
getElapsedGameTurns >= getMaxTurns`. Mastery score components: wonders-owned/total, team pop/global,
team land/global, team culture/global, monumental-culture cities (the `culturalVictoryValid` cache from
`doUpdateCacheOnTurn`), power-history integral (sum of `getPowerHistory(turn)`), religion (highest
`calculateReligionPercent` among held holy cities), and a +100 starship bonus for `getStarshipLaunched`.

### 1e. Mercy Rule countdown (`CvGame.cpp:7771-7871`)

With `MODDERGAMEOPTION_MERCY_RULE` on and mastery not reached: if one team's `getTotalVictoryScore` exceeds
half the global total, a countdown (`m_iMercyRuleCounter`) starts at `speedPercent/10` turns
(`CvGame.cpp:7798`) and ticks down each turn the leader stays dominant. At 0, that team wins by mastery
(`CvGame.cpp:7853-7854`); losing dominance resets the counter to 0 (`CvGame.cpp:7857-7859`).

### 1f. Game conclusion (`CvGame.cpp:4828-4860`, `4863-4887`)

`setWinner(eTeam, eVictory)` sets `m_eWinner` / `m_eVictory`, fires `CvEventReporter::victory` (→ Python
`onVictory`), logs a replay message, and sets `GAMESTATE_OVER` (or `GAMESTATE_EXTENDED` in autoplay). A
no-winner time-out (not end-score) takes the same path (`CvGame.cpp:7931-7938`).

### 1g. What is logged at session start

`CvGame::onFinalInitialized` logs one `[GAME/victory] VICTORY_TYPE_NAME` line per enabled victory into
`GameInfo.log` (`CvGame.cpp:606-611`, gated by `gPlayerLogLevel > 0`). This is the only structured victory
log — and it is static (what is enabled), never dynamic (progress).

---

## 2. What's on the wire today

**Tier 1 (Telescreen).** Exposed:

| Source | Field | What it gives you | Granularity |
|---|---|---|---|
| `/players` → `score` | `getPlayerScore()` | Derived Civ4 score — partial proxy (culture, pop, tech, wonders) | Per player, ≤5s |
| `/players` → `population` | `getTotalPopulation()` | One domination input; global total inferable by summing all players | Per player |
| `/players` → `cities` | `getNumCities()` | Conquest input (≥1 city = still in the race) | Per player |
| `/cities` → `cultureLevel` | `getCultureLevel()` | Cultural-victory progress (count cities ≥ threshold) | Per city |
| `[GAME/victory]` | name per enabled victory | Which victories are in play (static) | Once per session load |
| `/players` → `techs` | tech count | Scientific victory: indirect tech progress, NOT building-count thresholds | Per team |

### What is NOT exposed (the gap)

Per team per victory, none of the running state is exposed: `getVictoryCountdown(eVictory)` (the core timer),
`getVictoryDelay`, `getLaunchSuccessRate`, `starshipLaunched[team]` (the single most important space signal),
the countdown-0 RNG outcome, `getTeamScore` (the team aggregate, distinct from per-player `score`),
`getMaxTurns`/`getElapsedGameTurns` (time-limit fraction), `getTargetScore`, the domination thresholds
(`getAdjustedPopulationPercent`/`getAdjustedLandPercent`) and `getTotalLand`, `calculateReligionPercent` and
`hasHolyCity`, `getProjectCount` per project, `getBuildingCount` per victory building, victory `getVoteOutcome`,
`getTotalVictoryScore` (mastery/mercy composite), `diplomaticVictoryAchieved[team]`, the cultural counting-loop
output and `getCultureThreshold`, `m_eWinner`/`m_eVictory` (has the game been decided, by what type),
`m_iMercyRuleCounter`, and the victory-relevant game-option flags. **No `[VIC]` log domain exists**, no
per-turn progress lines, no `/events` publication — the winner is set silently with only a Python `onVictory`
callback that writes no structured log.

---

## 3. The gap

At Tier 1 an agent can answer "what is player X's score / city count / per-city culture level?" — noisy
proxies. It **cannot** answer without the screen or the code: is any team in a victory countdown, with how
many turns left? Has any spaceship launched, at what success-probability? How close is each team to each
victory? Is the Mercy Rule countdown active? What are this game's domination thresholds, and is team X close?
Which team holds which holy city, and what religion-pop fraction? How many space projects has each team built?
Has the game ended, and who won?

**For AI players specifically:** the victory-advisor popup and the Mercy-Rule `AddDLLMessage`
(`CvGame.cpp:7800-7821`) reach human players only, so an agent watching an AI-only autoplay game gets none of
it. The sole observable signal that a game ended is the `/events` stream ceasing `turnStart`/`turnEnd` frames
— a fragile inference, not a structured signal.

---

## 4. Proposed hooks

All hooks are one of the three canonical shapes — see
[DEC-obs-hook-shapes](../../architecture/decisions.md#dec-obs-hook-shapes).

### Hook A — `[VIC]` log domain (`VictoryAI.log`, `gPlayerLogLevel`)

New helper `logVictoryAI(level, fmt, …)` → `VictoryAI.log`, gated by `gPlayerLogLevel` (the `[DAI]`/`[WAI]`
knob), tag prefix `[VIC]`:

| Tag | Level | Where | Payload |
|---|---|---|---|
| `[VIC/check]` | 2 | per (team, victory) that passes in the outer loop | `turn= team= victory= countdown= successRate=` |
| `[VIC/countdown]` | 1 | countdown starts (−1 → delay) | `turn= team= victory= delay=` |
| `[VIC/tick]` | 1 | each turn countdown decrements | `turn= team= victory= remaining=` |
| `[VIC/launch]` | 1 | the RNG roll at countdown==0 | `turn= team= victory= successRate= roll= result=success\|fail` |
| `[VIC/reset]` | 1 | `CvTeam::resetVictoryProgress()` | `turn= team= victory= reason=launchFail\|conditionLost` |
| `[VIC/winner]` | 1 | `CvGame::setWinner()` | `turn= winner= victory= gameState=` |
| `[VIC/mercy]` | 1 | Mercy-Rule state change | `turn= leader= counter= action=start\|tick\|abort\|fire` |
| `[VIC/progress]` | 2 | per-team per-condition, only within ~20% of triggering | `turn= team= victory= metric= have= need= pct=` |

Level-1 lines cover the observable game events (countdown start/tick, win/fail, mercy); level 2 adds a
per-turn proximity trace.

### Hook B — `/players` snapshot fields (cheapest lift)

Add to `PlayerSnap` / `renderPlayers` (`Sources/Tools/CvHttpServer.cpp`):

| JSON key | Source | Notes |
|---|---|---|
| `"teamScore"` | `GC.getGame().getTeamScore(kPlayer.getTeam())` | The Time/Score victory aggregate; differs from per-player `score` for multi-player teams |
| `"teamLand"` | `GET_TEAM(eTeam).getTotalLand()` | Domination land input |

### Hook C — `/diagnostic/victoryProgress?player=N` endpoint

Mailbox-pattern diagnostic (the `placementSweep` contract — game-thread-side, never server-thread). The
single-call total-observability snapshot for victory state:

```json
{
  "turn": 315, "elapsed": 315, "maxTurns": 500, "targetScore": 0, "mercyRuleCounter": 0,
  "teams": [ {
    "team": 0, "totalVictoryScore": 423, "totalLand": 182, "totalPopulation": 74,
    "victories": [ {
      "type": "VICTORY_SPACE_RACE", "valid": true, "countdown": -1, "delay": 10,
      "successRate": 87, "launched": false, "passing": false,
      "domPopPct": 0, "domLandPct": 0, "religionPct": 0,
      "cultureThresholdCities": 0, "cultureNeeded": 5,
      "projectCounts": { "PROJECT_APOLLO": 1, "PROJECT_SHUTTLE": 0 }, "buildingCounts": {}
    } ]
  } ],
  "globalPopulation": 310, "globalLandPlots": 1540,
  "dominationPopThreshold": 42, "dominationLandThreshold": 35
}
```

Calls `getElapsedGameTurns`/`getMaxTurns`/`getTargetScore`/`getMercyRuleCounter`; per team
`getTotalVictoryScore`/`getTotalPopulation`/`getTotalLand`; per team×victory `testVictory` (read-only via the
`bValid` chain), `getVictoryCountdown`/`getVictoryDelay`/`getLaunchSuccessRate`/`getStarshipLaunched`/
`calculateReligionPercent`, per-city culture count, `getProjectCount`/`getBuildingCount`; global
`getAdjustedPopulationPercent`/`getAdjustedLandPercent`. Expensive in full — gate via the same `g_evalPending`
mailbox as `placementSweep`, throttled (2s or per-5-turn), 503 on pending.

### Hook D — game-state fields on the snapshot

Add a `GameStateSnap` to the snapshot (published once per 5s by `publishIfDue`), surfaced on a new `GET /game`
endpoint or injected into the `/players` root:

| JSON key | Source |
|---|---|
| `"gameState"` | `getGameState()` enum int |
| `"winner"` | `getWinner()` team int (−1 = none) |
| `"victoryType"` | `getVictory()` victory int (−1 = none) |
| `"elapsedTurns"` | `getElapsedGameTurns()` |
| `"maxTurns"` | `getMaxTurns()` |
| `"mercyRuleCounter"` | `getMercyRuleCounter()` |

Cheap scalar reads. `winner != -1` is the single most critical observable the agent lacks — without it an
agent watching autoplay cannot tell the game concluded.

### Hook E — `/events` SSE frames for winner + countdown

Emit `event: victoryCountdown` from `testVictory()` (start/tick/fire) and `event: winner` from `setWinner()`:
```
event: winner
data: {"turn":315,"winner":2,"victory":"VICTORY_SPACE_RACE","gameState":2}
event: victoryCountdown
data: {"turn":310,"team":2,"victory":"VICTORY_SPACE_RACE","action":"start","remaining":10}
```
`publishEvent("winner", …)` in `setWinner()` is one call at a rare moment — zero performance concern.

**Priority:** Hook **D** (game-state — detect conclusion without the screen) + Hook **B** (cheapest Tier-2
lift) → Hook **A** level 1 + Hook **E** (countdown/winner on the wire, Tier 3) → Hook **C**
(`/diagnostic/victoryProgress`, the full per-condition Tier-4 snapshot).

---

## 5. Cascade relevance (#428/#430)

Victory conditions are **end-state tests**, not running-state maintainers — they read accumulated state
(buildings, projects, culture levels, religion spread, land) rather than writing the building set. The cascade
concern is one step removed: it must replicate the inputs correctly (correct `hasBuilding`, project counts,
city culture levels) and the per-turn test then reads them. Two pieces ARE maintainer-adjacent:

- **`m_iVictoryCountdown[eVictory]` per team** — mutated by `testVictory()` itself; state that must survive
  turn boundaries. If the cascade rebuilds `hasBuilding` differently (e.g. dormancy changing what counts as
  active), the building-count threshold check changes, which can prematurely start or abort a countdown. Shadow needed.
- **`starshipLaunched[team]`** — a one-way latch set inside `testVictory`; once set, the space condition
  always returns true. Any cascade change re-deriving project counts must preserve the latch or the launch is
  incorrectly rescinded.

**Minimum observability before touching anything the victory checks read**
([DEC-map-before-delete](../../architecture/decisions.md#dec-map-before-delete)): Hook C to snapshot the
pre-change state; Hook A level 1 (`[VIC/countdown]`/`[VIC/tick]`/`[VIC/launch]`/`[VIC/winner]`) so countdown
changes are visible during shadow testing; Hook D so the agent can detect conclusion. Without these, a cascade
change affecting buildings or project counts cannot be verified to not alter victory reachability — the
before/after states are both opaque.

---

## See also
- [`README.md`](README.md) — the observability scale, the Orwell bar, and the three hook shapes this map applies.
- [`http-server.md`](http-server.md) — the live surface (`/players`, `/cities`, `/diagnostic/*`) these hooks extend, and the live-read rules.
- [`../../architecture/decisions.md`](../../architecture/decisions.md) — [DEC-obs-scale], [DEC-obs-hook-shapes], [DEC-map-before-delete].
- [`../../explanation/cascade-architecture.md`](../../explanation/cascade-architecture.md) — why total observability is load-bearing, and why the victory checks consume cascade-derived inputs.
- [`../../README.md`](../../README.md) — the comprehension map (overview-of-overviews).
