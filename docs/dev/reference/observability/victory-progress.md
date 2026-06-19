# Observability map: Victory-conditions progress

> DRAFT observability map (2026-06-18 by parent) — claims cited from code; verify before relying.

**Scope:** all victory types — Time/Score, Conquest, Domination, Cultural, Religious, Scientific/Building,
Space Race (project countdown), Diplomatic (UN vote), Mastery (total-victory) — and the Mercy Rule countdown.
**No existing `docs/dev/reference/` page existed for this system.**
**Observability tier assigned:** **1 — Telescreen** (coarse score snapshot per player; the victory conditions
themselves, countdowns, and per-condition progress are entirely invisible from outside).

---

## 1. How it actually works

### 1a. The per-turn victory check loop

`CvGame::testVictory()` (CvGame.cpp:7696) fires every turn from
`CvGame::doTurn` via `game.testVictory` (`CvGame.cpp:6106`). It bails early if a winner
is already decided, if the game is in EXTENDED state, or if fewer than
`speedPercent/10` turns have elapsed (the early-game grace period, CvGame.cpp:7700-7703).

It calls `updateScore()` to ensure scores are fresh, then iterates every alive non-minor
`CvTeam × VictoryTypes`, calling the single-condition overload:

```
bool CvGame::testVictory(VictoryTypes eVictory, TeamTypes eTeam, bool* pbEndScore)
```
(CvGame.cpp:7504)

### 1b. Per-victory-type conditions (all in `testVictory` overload, CvGame.cpp:7504-7693)

| Victory type | Condition (abbreviated) | Key fields read |
|---|---|---|
| **Time / End Score** | `isEndScore()`: `getElapsedGameTurns() >= getMaxTurns()` AND this team has the highest `getTeamScore` | `getElapsedGameTurns`, `getMaxTurns`, `getTeamScore` |
| **Score** | `isTargetScore()`: `getTeamScore(eTeam) >= getTargetScore()` AND uniquely highest | `getTeamScore`, `getTargetScore` |
| **Conquest** | `isConquest()`: no other alive non-vassal team has any cities | `GET_TEAM(iK).getNumCities()`, `isVassal` |
| **Diplomatic** | `isDiploVote()`: a `VoteInfo` with `isVictory()` has its `getVoteOutcome` equal to this team | `m_paiVoteOutcome[VoteTypes]` |
| **Domination** | `getAdjustedPopulationPercent(eVictory) > 0` — team must hold `X%` of world pop; `getAdjustedLandPercent(eVictory) > 0` — team must hold `Y%` of world land | `GET_TEAM.getTotalPopulation`, `GET_TEAM.getTotalLand`, `GC.getMap().getLandPlots`, `getTotalPopulation()` |
| **Religious** | `getReligionPercent() > 0`: team has a holy city for a religion covering ≥N% of world pop (`calculateReligionPercent`) AND `getNumCivCities() > countCivPlayersAlive()*2` | `hasHolyCity`, `calculateReligionPercent`, `getNumCivCities` |
| **Cultural** | `getCityCulture() != NO_CULTURELEVEL && getNumCultureCities() > 0`: count cities team-wide at or above the specified `CultureLevelTypes` | `pLoopCity->getCultureLevel()`, `getNumCultureCities()` |
| **Scientific/Building** | `getVictoryThreshold(eVictory) > 0` per building: team must have `≥ threshold` count of that building | `GET_TEAM.getBuildingCount(BuildingTypes)` |
| **Space Race** | Projects: every `CvProjectInfo` with `getVictoryMinThreshold(eVictory) > 0` must have `team.getProjectCount ≥ minThreshold`; sets `starshipLaunched[eTeam]=true` on first pass | `GET_TEAM.getProjectCount`, `starshipLaunched[]` (bool array on CvGame) |

`starshipLaunched[eTeam]` is a `bool` array on `CvGame` (CvGame.h:286 — `getStarshipLaunched(int ID)`).
It is set the first time Space conditions pass, and the team remains launched even if the conditions
are re-tested (CvGame.cpp:7678-7691 — the `!starshipLaunched[eTeam]` gate means the second pass
skips the project check and still returns `bValid = true`).

### 1c. Victory countdown and delay (CvGame.cpp:7718-7744, CvTeam.cpp:4898-4947)

When `testVictory(…,eTeam)` returns `true`:

1. If `getVictoryCountdown(eVictory) < 0` AND `getVictoryDelay(eVictory) == 0`, the countdown is
   initialized to `0` immediately (CvGame.cpp:7725).
2. If `getVictoryDelay > 0`, the delay fires: `setVictoryCountdown(eVictory, delay)` and the
   countdown ticks down by 1 per passing turn while the condition still holds.
3. When `getVictoryCountdown(eVictory) == 0`: an RNG check `SorenRandNum(100) < getLaunchSuccessRate`
   gates the actual win (CvGame.cpp:7736). Success rate can be < 100 for incomplete space projects.
4. On failure: `resetVictoryProgress()` clears all countdowns and erases project counts for space
   projects (CvTeam.cpp:4981-4997).

`getVictoryDelay(VictoryTypes)` (CvTeam.cpp:4922-4947): base delay = `VictoryInfo.getVictoryDelayTurns() × speedPercent / 100` (CvGame.cpp:3272). If space projects are partially complete (count in `[minThreshold, threshold)`) the delay is extended proportionally by `getVictoryDelayPercent()`.

`getLaunchSuccessRate(VictoryTypes)` (CvTeam.cpp:4960-4978): starts at 100, reduced by `successRate × (threshold − count)` per under-count project; returns 0 if any project is below `minThreshold`.

### 1d. Mastery / Total Victory (CvGame.cpp:7750-7768)

If a `VictoryTypes` has `isTotalVictory() == true` and that victory is valid, **all** other winner
candidates are cleared and instead the winner is chosen by `getTotalVictoryScore()` (CvTeam.cpp:7332-7487)
when `getMaxTurns > 0 && getElapsedGameTurns >= getMaxTurns`. Mastery score components (CvTeam.cpp):
- Wonders owned / total (globalWonderScore)
- Team population / global population
- Team land / global land
- Team culture / global culture
- Monumental-culture-level cities (the `culturalVictoryValid` cache from `doUpdateCacheOnTurn`)
- Power history integral (sum of `getPowerHistory(turn)` across all turns)
- Religion (highest `calculateReligionPercent` among holy cities held)
- Starship bonus (+100 for `getStarshipLaunched`)

### 1e. Mercy Rule countdown (CvGame.cpp:7771-7871)

When `MODDERGAMEOPTION_MERCY_RULE` is on and the game has not reached mastery: if one team's
`getTotalVictoryScore` exceeds half the global total, a countdown (`m_iMercyRuleCounter`) starts
at `speedPercent/10` turns (CvGame.cpp:7798). It counts down each turn the leader remains dominant.
When it reaches 0, that team wins by mastery score (CvGame.cpp:7853-7854). If dominance is lost,
the counter resets to 0 (CvGame.cpp:7857-7859).

### 1f. Game conclusion (CvGame.cpp:4828-4860, 4863-4887)

`setWinner(eTeam, eVictory)` sets `m_eWinner` / `m_eVictory`, fires `CvEventReporter::victory`
(→ Python `onVictory`), logs a replay message, and sets `GAMESTATE_OVER` (or `GAMESTATE_EXTENDED`
in autoplay). If no winner and time is up and it wasn't an end-score victory: same GAMESTATE_OVER
/ EXTENDED path (CvGame.cpp:7931-7938).

### 1g. What is logged at session start

`CvGame::onFinalInitialized` logs one `[GAME/victory] VICTORY_TYPE_NAME` line per enabled victory
into `GameInfo.log` (CvGame.cpp:606-611, gated by any `gPlayerLogLevel > 0`). This is the only
structured log about victory conditions — and it is static (what is enabled), not dynamic (progress).

---

## 2. Current observability

### What is exposed today

| Source | Field | What it gives you | Granularity |
|---|---|---|---|
| `GET /players` → `score` | `getPlayerScore()` (ranking score, updated by `updateScore()`) | The derived Civ4 score — partially reflects cultural progress, population, tech, wonders | Per player, snapshotted every ≤5s |
| `GET /players` → `population` | `getTotalPopulation()` | One of the two domination inputs (population share). Global total can be inferred if all players' populations are summed. | Per player |
| `GET /players` → `cities` | `getNumCities()` | Conquest check input (≥1 city = still in the race) | Per player |
| `GET /cities` → `cultureLevel` | `getCultureLevel()` (CultureLevelTypes int) | Cultural victory progress: count cities at or above the threshold | Per city |
| `[GAME/victory]` | Name string per enabled victory | Which victories are in play (static, one-time at session start) | Once per session load |
| `GET /players` → `techs` | Tech count (per team) | Scientific victory: indirectly shows tech progress, but NOT building-count thresholds | Per team |

### What is NOT exposed (the gap — exhaustive)

| State | Why it matters for victory tracking | Missing surface |
|---|---|---|
| **`getVictoryCountdown(eVictory)` per team** | The core timer — how many turns until the win fires (or -1 = not in progress). Without this, you cannot see that a team is in a countdown at all. | No endpoint, no log tag |
| **`getVictoryDelay(eVictory)` per team** | How long the countdown will be when it starts — depends on project completeness. | No endpoint |
| **`getLaunchSuccessRate(eVictory)` per team** | Space race success probability; < 100 means launch can fail. | No endpoint |
| **`starshipLaunched[team]`** | Whether a team's spaceship has launched. The single most important space-race signal. | No endpoint field, no log tag |
| **`getVictoryCountdown` == 0 → RNG roll** | Whether a turn-0 countdown succeeded or was reset. The only signal today is `resetVictoryProgress()` quietly wiping everything. | No log, invisible |
| **`m_eWinner` / `m_eVictory`** | Whether the game has been decided — and by which type. Not surfaced in any endpoint. | No endpoint |
| **`m_iMercyRuleCounter`** | The mercy-rule countdown. Fires only for human players (DLL popup), invisible to an agent watching AI games. | No endpoint, no log tag |
| **`getTeamScore(eTeam)`** | The team-aggregated score used in the Time/Score victory check. `/players` gives individual player score, not the team aggregate — for multi-player teams these differ. | No endpoint (only individual `getPlayerScore`) |
| **`getMaxTurns()` / `getElapsedGameTurns()`** | The time-limit progress fraction. Without these, the agent cannot compute when the end-score / mastery check fires. | No endpoint |
| **`getTargetScore()`** | The score target for a score-based victory, or 0 if none. | No endpoint |
| **Domination thresholds** (`getAdjustedPopulationPercent`, `getAdjustedLandPercent`) | The actual bar each team must exceed; depends on the number of ever-alive civs. | No endpoint |
| **`GET_TEAM.getTotalLand()`** | Land plots controlled per team — the second domination input. | No endpoint |
| **`calculateReligionPercent(eReligion)`** | Religious victory: what fraction of world population follows a religion. | No endpoint |
| **`hasHolyCity(eReligion)` per team** | Which team holds the holy city for each religion. | No endpoint |
| **`GET_TEAM.getProjectCount(eProject)` per team** | Space race: how many of each project a team has built. The raw progress counter. | No endpoint |
| **`GET_TEAM.getBuildingCount(eBuilding)` per team for victory buildings** | Scientific/building victory threshold check input. | No endpoint |
| **`getVoteOutcome(eVote)` for victory votes** | Diplomatic victory: which team won the UN vote (if any). | No endpoint |
| **`getTotalVictoryScore()` per team** | The composite mastery score (pop + land + culture + wonders + power + religion + starship). Used for Mastery and Mercy Rule. | No endpoint |
| **`diplomaticVictoryAchieved[team]`** | Whether a team has ever achieved a diplomatic victory this game. | No endpoint |
| **Cultural victory counting loop output** | How many cities across a team currently meet the culture-level threshold (vs. how many are needed). | No endpoint — must be recomputed from `/cities?playerNumber=N` but that requires knowing which CultureLevel the victory needs, which is also not exposed |
| **`getCultureThreshold()` / `culturalVictoryNumCultureCities()`** | The actual number of high-culture cities required (cached in `m_iNumCultureVictoryCities`). | No endpoint |
| **Game-option flags relevant to victory** | `MODDERGAMEOPTION_MERCY_RULE`, `GAMEOPTION_ENABLE_UNITED_NATIONS`, `isTotalVictory` active | Not surfaced on any endpoint |

---

## 3. The gap

At **Tier 1 (Telescreen)**, we can answer:

- "What is player X's score?" — a noisy proxy, not a direct victory metric.
- "How many cities does player X have?" — sufficient to detect Conquest *candidate* status but not
  to know whether the conquest condition actually passes (must check all teams).
- "What culture level are each city at?" — sufficient to manually count cultural-victory-threshold
  cities if you know the threshold, but the threshold is not exposed.

We **cannot** answer without looking at the screen (or the code):

- Is any team in a victory countdown right now? How many turns left?
- Has any team's spaceship launched? What is the success-probability?
- How close is each team to each victory type?
- Is the Mercy Rule countdown active? How many turns remain?
- What is the actual domination threshold for this game (pop %, land %)? Is team X close?
- Which team holds which religion's holy city? What fraction of world pop follows each religion?
- How many space-race projects has each team built? What still remains?
- Has the game ended, and who won?

**For AI players specifically:** the game screen shows the victory advisor popup. An agent watching an
AI-only autoplay game gets none of that — the DLL messages (`AddDLLMessage`) go to human players only
(CvGame.cpp:7800-7821 for Mercy Rule), so even the "countdown has begun" UI notice is invisible via the
endpoints. The only observable signal that a game ended is that the `/events` stream would stop
receiving `turnEnd` / `turnStart` events — a fragile inference, not a structured signal.

**No log tags exist for any victory system.** There is no `[VIC]` domain, no per-turn progress lines,
and no per-event publication to `/events` for any victory condition — the winner is set silently, with
only a Python `onVictory` callback that writes no structured log.

---

## 4. Proposed hooks (concrete additions to climb toward Tier 3/4)

All additions are gated (zero off-state cost — one int compare per gated call, or `isEnabled()` for
HTTP hooks). No AI/game-logic changes.

### Hook A — `[VIC]` log domain, `VictoryAI.log`, player-scope (`gPlayerLogLevel`)

New log helper `logVictoryAI(int level, const char* fmt, ...)` → `VictoryAI.log`, gated by
`gPlayerLogLevel` (the same knob as `[DAI]`/`[WAI]`). Tag prefix `[VIC]`.

| Tag | Level | Where | Payload |
|---|---|---|---|
| `[VIC/check]` | 2 | `CvGame::testVictory()` outer loop entry, per (team, victoryType) that passes | `turn= team= victory= countdown= successRate=` |
| `[VIC/countdown]` | 1 | `CvGame::testVictory()` when countdown starts (transitions from -1 to delay value) | `turn= team= victory= delay=` |
| `[VIC/tick]` | 1 | `CvGame::testVictory()` each turn countdown decrements | `turn= team= victory= remaining=` |
| `[VIC/launch]` | 1 | `CvGame::testVictory()` at the RNG roll on countdown==0 | `turn= team= victory= successRate= roll= result=success|fail` |
| `[VIC/reset]` | 1 | `CvTeam::resetVictoryProgress()` | `turn= team= victory= reason=launchFail|conditionLost` |
| `[VIC/winner]` | 1 | `CvGame::setWinner()` | `turn= winner= victory= gameState=` |
| `[VIC/mercy]` | 1 | Mercy Rule state change (start / tick / abort / fire) | `turn= leader= counter= action=start|tick|abort|fire` |
| `[VIC/progress]` | 2 | `CvGame::testVictory()` per-team per-condition proximity lines (only when condition is within ~20% of triggering) | `turn= team= victory= metric= have= need= pct=` |

Level 1 lines cover the observable game events (countdown start, ticks, win/fail, mercy events).
Level 2 adds a per-turn proximity trace so the agent can track how close each team is.

### Hook B — `/players` snapshot fields (cheapest Tier 2 lift)

Add to `PlayerSnap` and rendered in `CvHttpServer.cpp::renderPlayers`:

| JSON key | Source call | Notes |
|---|---|---|
| `"teamScore"` | `GC.getGame().getTeamScore(kPlayer.getTeam())` | Team aggregate score — what the Time/Score victory checks use; different from per-player score for multi-player teams |
| `"teamLand"` | `GET_TEAM(eTeam).getTotalLand()` | Domination land input per team |

### Hook C — `/diagnostic/victoryProgress?player=N` endpoint

New mailbox-pattern diagnostic (same contract as `placementSweep` — evaluated game-thread-side, never server-thread):

```json
{
  "turn": 315,
  "gameId": "2026061418",
  "elapsed": 315,
  "maxTurns": 500,
  "targetScore": 0,
  "mercyRuleCounter": 0,
  "teams": [
    {
      "team": 0,
      "totalVictoryScore": 423,
      "totalLand": 182,
      "totalPopulation": 74,
      "victories": [
        {
          "type": "VICTORY_SPACE_RACE",
          "valid": true,
          "countdown": -1,
          "delay": 10,
          "successRate": 87,
          "launched": false,
          "passing": false,
          "domPopPct": 0,
          "domLandPct": 0,
          "religionPct": 0,
          "cultureThresholdCities": 0,
          "cultureNeeded": 5,
          "projectCounts": { "PROJECT_APOLLO": 1, "PROJECT_SHUTTLE": 0 },
          "buildingCounts": {}
        }
      ]
    }
  ],
  "globalPopulation": 310,
  "globalLandPlots": 1540,
  "dominationPopThreshold": 42,
  "dominationLandThreshold": 35
}
```

This is the **single-call total-observability snapshot for victory state**. It calls:
- `getElapsedGameTurns()`, `getMaxTurns()`, `getTargetScore()`
- `getMercyRuleCounter()`
- Per team: `getTotalVictoryScore()`, `getTotalPopulation()`, `getTotalLand()`
- Per team × victory: `testVictory(…)` (read-only, no side effects via the `bValid` chain), `getVictoryCountdown()`,
  `getVictoryDelay()`, `getLaunchSuccessRate()`, `getStarshipLaunched()`, `calculateReligionPercent()`,
  per-city culture count, `getProjectCount()` per project, `getBuildingCount()` per victory building
- Global: `getAdjustedPopulationPercent()`, `getAdjustedLandPercent()`

Expensive to compute in full — gate it via the same `g_evalPending` mailbox used by `placementSweep`,
with a `2s` or per-5-turn throttle. Return 503 if a previous eval is pending.

### Hook D — game-state fields on the snapshot (game-level; add to `GameSnapshot`)

Add a `GameStateSnap` to the snapshot (published once per 5s by `publishIfDue`):

| JSON key | Source | Where to expose |
|---|---|---|
| `"gameState"` | `GC.getGame().getGameState()` enum int | Top-level in `GameSnapshot`, surfaced as a field on a new `GET /game` endpoint or injected into `/players` root |
| `"winner"` | `GC.getGame().getWinner()` team int (-1 = none) | Same |
| `"victoryType"` | `GC.getGame().getVictory()` victory int (-1 = none) | Same |
| `"elapsedTurns"` | `GC.getGame().getElapsedGameTurns()` | Same |
| `"maxTurns"` | `GC.getGame().getMaxTurns()` | Same |
| `"mercyRuleCounter"` | `GC.getGame().getMercyRuleCounter()` | Same |

These are cheap scalar reads. A game-ended state (`winner != -1`) is the most critical observable the agent lacks: without it, an agent watching an autoplay game cannot tell when the game has concluded.

### Hook E — `/events` SSE event for winner declared and countdown events

Emit a structured `/events` SSE `event: victoryCountdown` frame from `testVictory()` when a countdown
starts, ticks, or fires, and an `event: winner` frame from `setWinner()`:

```
event: winner
data: {"turn":315,"winner":2,"victory":"VICTORY_SPACE_RACE","gameState":2}

event: victoryCountdown
data: {"turn":310,"team":2,"victory":"VICTORY_SPACE_RACE","action":"start","remaining":10}
```

`publishEvent("winner", ...)` in `CvGame::setWinner()` — a single call at a rare moment, zero
performance concern.

---

## 5. Cascade/tally implications

Victory conditions are **end-state tests** rather than running state maintainers in the §14 H sense —
they read accumulated state (buildings, projects, culture levels, religion spread, land ownership) rather
than writing to the building set. The cascade replacement concern is therefore **one step removed**:
the cascade must replicate the inputs correctly (correct `hasBuilding` for building-threshold victories,
correct project counts for space race, correct city culture levels for cultural victory) and then the
per-turn victory test reads those values. The tests themselves are not §14 H maintainers.

However two pieces ARE maintainer-adjacent:

- **`m_iVictoryCountdown[eVictory]` per team** — mutated by `testVictory()` itself; the countdown
  IS state that must survive turn boundaries. If the cascade rebuilds `hasBuilding` differently
  (e.g., dormancy changes what buildings are "active"), the victory building-count threshold
  check changes, which can prematurely start or abort a countdown. Shadow needed.
- **`starshipLaunched[team]`** — a one-way latch set inside `testVictory`. Once set, the space-race
  condition always returns true (the project check is gated by `!starshipLaunched`). Any cascade change
  that re-derives project counts must preserve this latch or the launch can be incorrectly rescinded.

The **minimum observability needed** before touching anything the victory checks read:
- Hook C (`/diagnostic/victoryProgress`) to snapshot the pre-change state.
- Hook A level 1 (`[VIC/countdown]`, `[VIC/tick]`, `[VIC/launch]`, `[VIC/winner]`) so the turn-by-turn
  change in countdown state is visible in the event stream during shadow testing.
- Hook D game-state fields so the agent can detect game conclusion without watching the screen.

Without these, any cascade change that affects buildings or project counts cannot be safely verified
to not have changed victory reachability for any team — the "before" and "after" states are both opaque.

---

## Summary

| Dimension | Current state |
|---|---|
| **Tier** | 1 — Telescreen |
| **Exposed** | Per-player Civ4 ranking `score`; `population`; `cities` count; per-city `cultureLevel`; one-time `[GAME/victory]` enabled-set log |
| **Opaque** | Countdown state per team per victory; spaceship-launched flag; domination thresholds; land per team; project counts; building-threshold counts; religion spread; vote outcomes; mastery score; mercy-rule counter; game winner / victory type / game state; whether game has ended |
| **Minimum hooks for Tier 2** | Hook B (`teamScore`, `teamLand` on `/players`) + Hook D (game-state on snapshot/new `/game` endpoint) |
| **Minimum hooks for Tier 3** | Hook A level 1 (`[VIC/countdown]`, `[VIC/tick]`, `[VIC/launch]`, `[VIC/winner]`, `[VIC/mercy]`) + Hook E (SSE `winner` frame) |
| **Hooks for full Tier 4** | All of A+B+C+D+E above — especially Hook C (`/diagnostic/victoryProgress`) for per-team per-condition snapshot |
| **Cascade-blocking gap** | `starshipLaunched` latch and `m_iVictoryCountdown[]` array are invisible; any cascade change to building/project counts cannot be verified to not alter victory reachability without Hook C + Hook A |
