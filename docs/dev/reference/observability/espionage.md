> DRAFT observability map (2026-06-18 by parent agent) — all claims cited from code;
> verify against the named file:line before relying on any specific detail.

# Observability map — Espionage economy & missions

**System scope:** EP accrual, per-team allocation, mission cost/execution, counterespionage
timers, city-level espionage effects, and the AI's mission-selection logic.

**Observability tier: 0 — Oblivious.** No espionage-specific state appears in any
HTTP endpoint (`/players`, `/cities`, `/units`, `/diagnostic`). The one AI log tag
(`[ESP/best]`) exists but only covers the mission-selection commit; it does not cover
EP income, weight allocation, counterespionage, or city-level effect timers. The full
espionage economy is invisible from outside the screen.

---

## 1. How it actually works

### 1-A. EP income: per-turn accrual

`CvPlayer::doTurn` calls `doEspionagePoints()` (CvPlayer.cpp:3813), which
calls `doEspionageOneOffPoints(getCommerceRate(COMMERCE_ESPIONAGE))`
(CvPlayer.cpp:15580-15583). `getCommerceRate(COMMERCE_ESPIONAGE)` is the
player's total EP rate from the commerce slider and city outputs — it is the
same rate that appears as the espionage slider value in the UI.

`doEspionageOneOffPoints(iChange)` (CvPlayer.cpp:15555-15578):
1. Increments `CvTeam::m_iEspionagePointsEver` by `iChange`
   (CvPlayer.cpp:15561 → CvTeam::changeEspionagePointsEver).
2. Divides the EP into per-target-team buckets using `getEspionageSpending`
   (CvPlayer.cpp:15569) and calls
   `CvTeam::changeEspionagePointsAgainstTeam` (CvPlayer.cpp:15573).
   Points only go against teams the player has already met.

The espionage commerce rate is gated: if no teams have been met, the
COMMERCE_ESPIONAGE slider is forced off (CvPlayer.cpp:13200 —
`COMMERCE_ESPIONAGE && 0 == GET_TEAM(getTeam()).getHasMetCivCount(true)`).

### 1-B. EP allocation: per-target-team weights

The weight split is controlled by `m_aiEspionageSpendingWeightAgainstTeam[]`
(CvPlayer.h:1985), one integer per team, range 0-99 (clamped in
`setEspionageSpendingWeightAgainstTeam` at CvPlayer.cpp:16772).

`getEspionageSpending(TeamTypes eAgainstTeam, int iTotal)` (CvPlayer.cpp:15585):
- Sums all weights for met, alive, non-self teams → `iTotalWeight`.
- Proportionally assigns `(iTotalPoints * weight[i]) / iTotalWeight` to each.
- Remainder points go to the team(s) with the highest weight, round-robin.
- If all weights are 0, all remainder points go to all tied-best teams.

**AI weight-setting** (`AI_updateCommercePercent`, CvPlayerAI.cpp ~16534-16823):
- Runs in `AI_doTurnPost` each turn.
- Resets all weights to 0 (CvPlayerAI.cpp:16537-16540).
- For each met, alive team: computes `piWeight[team]` and `piTarget[team]`
  from attitude, war status, their EP-ever vs our EP-ever, desired mission costs,
  and spy-memory (`MEMORY_SPY_CAUGHT`) (CvPlayerAI.cpp:16680-16773).
- Sets `COMMERCE_ESPIONAGE` percent by incrementing research percent down and
  espionage percent up until `getCommerceRate(COMMERCE_ESPIONAGE) >= iEspionageTargetRate`
  or the percent cap (`iMaxEspionage`, typically 5-15%) is hit
  (CvPlayerAI.cpp:16807-16822).
- Not logged.

### 1-C. Accumulated EP per team-pair

Stored in `CvTeam::m_aiEspionagePointsAgainstTeam[]` (CvTeam.h:583), one
per target team. Incremented by `doEspionageOneOffPoints`, decremented by
`doEspionageMission` on a successful mission (CvPlayer.cpp:16707).

A second global accumulator `m_iEspionagePointsEver` (CvTeam.h:545) records
the total EP a team has ever produced; this feeds the mission cost modifier
"my EP-ever vs their EP-ever" formula
(CvPlayer.cpp:16256-16265). It is also recorded in the player history map
`m_mapEspionageHistory` keyed by turn (CvPlayer.cpp:3970), but this map is
not exposed anywhere.

### 1-D. Mission cost calculation

`getEspionageMissionCost` (CvPlayer.cpp:15758) is the full cost:
```
baseCost × costModifier / 100 × numTeamMembers
```
`getEspionageMissionBaseCost` (CvPlayer.cpp:15780) selects one of ~17
mission-type branches to compute the base (treasury fraction, tech cost,
production progress, city pop, etc.).

`getEspionageMissionCostModifier` (CvPlayer.cpp:16167) is a multiplicative
chain of factors (all cite CvPlayer.cpp:16167-16296):
- City population: `+ESPIONAGE_CITY_POP_EACH_MOD per pop` above 1
- Trade route: `+ESPIONAGE_CITY_TRADE_ROUTE_MOD`
- Shared religion: `+ESPIONAGE_CITY_RELIGION_STATE_MOD` / `+ESPIONAGE_CITY_HOLY_CITY_MOD`
- City culture ratio: reduces cost the more of your culture is in the city
- Target city's `getEspionageDefenseModifier` (building-driven; CvCity.cpp:14238)
- Distance from your capital to the target plot
- Spy fortify bonus: reduces cost by `ESPIONAGE_EACH_TURN_UNIT_COST_DECREASE` per fortify turn (up to 5)
- **EP-ever ratio:** `ESPIONAGE_SPENDING_MULTIPLIER × (2×their_ever + our_ever) / (their_ever + 2×our_ever)` — if they have more EP-ever, missions against them are cheaper (CvPlayer.cpp:16256-16265)
- Target team's counterespionage mod against our team (CvPlayer.cpp:16269-16273)
- Embassy discount (CvPlayer.cpp:16277-16280)
- Free Trade Agreement discount (CvPlayer.cpp:16282-16286)

### 1-E. Mission execution

`doEspionageMission` (CvPlayer.cpp:16299-16759) executes in one pass per call.
On success (`bSomethingHappened`): deducts `iMissionCost` from
`CvTeam::m_aiEspionagePointsAgainstTeam[targetTeam]` (CvPlayer.cpp:16707).
Effects include: building demolition, project demolition, culture insertion,
city poison/unhappy/revolt counters, civic/religion switch, anarchy counter,
research sabotage, counterespionage mission, nuclear bomb. None of these emit
any log line or event to the observable surface.

### 1-F. Counterespionage state

On a counterespionage mission (`kMission.getCounterespionageMod() > 0`):
- `CvTeam::changeCounterespionageTurnsLeftAgainstTeam(targetTeam, iTurns)` (CvTeam.h:441)
- `CvTeam::changeCounterespionageModAgainstTeam(targetTeam, mod)` (CvTeam.h:445)
  where mod = `kMission.getCounterespionageMod() + 5 × spy.currInterceptionProbability()` (CvPlayer.cpp:16689)
- The timer counts down by 1 per turn in `CvTeam::doTurn` (CvTeam.cpp:1061-1068);
  when it hits 0, `setCounterespionageModAgainstTeam(team, 0)` clears it.
- The active counterespionage mod is applied as a multiplicative cost increase
  to missions against the team that set it (CvPlayer.cpp:16269-16273).

### 1-G. City-level espionage effect timers

Four per-city effect timers, all counting-down each `CvCity::doTurn`:

| Timer | Getter (CvCity.h) | Effect | Decremented at |
|---|---|---|---|
| `m_iEspionageHealthCounter` | `getEspionageHealthCounter()` (CvCity.cpp:7994) | unhealthy by counter (CvCity.cpp:5855) | CvCity.cpp:1409-1412 |
| `m_iEspionageHappinessCounter` | `getEspionageHappinessCounter()` (CvCity.cpp:8007) | unhappy by counter (CvCity.cpp:5639) | CvCity.cpp:1414-1417 |
| `m_iDisabledPowerTimer` | `getDisabledPowerTimer()` (CvCity.h:1299) | power disabled | (doTurn) |
| `m_iWarWearinessTimer` | `getWarWearinessTimer()` (CvCity.h:1303) | war weariness | (doTurn) |

None of these are in the HTTP snapshot.

### 1-H. City espionage defense modifier

`getEspionageDefenseModifier()` (CvCity.cpp:14238) is a building-driven
per-city modifier that increases mission costs against that city (fed into
`getEspionageMissionCostModifier` at CvPlayer.cpp:16218). Not exposed.

### 1-I. AI mission selection

`CvPlayerAI::AI_bestPlotEspionage` (CvPlayerAI.cpp:15147) is the AI's spy
decision function. It iterates spy plots, enumerates all mission types, scores
each via `AI_espionageVal` (CvPlayerAI.cpp:15504), and returns the
highest-value mission. The decision is driven by: attitude weight, war plan,
EP balance, city property values (buildings, production progress, culture,
treasury), and mission cost.

The **one** log line in the entire espionage system:
```
[ESP/best] player=%d spyAt=(%d,%d) mission=%d target=%d value=%d
```
emitted at level 1 after AI_bestPlotEspionage concludes (CvPlayerAI.cpp:15495).
This log is gated by `gPlayerLogLevel` (not a separate gate; shares the
single BUG "Player BBAI log level" knob). The winning mission is logged
but: the integer enum for `mission` requires lookup against
`EspionageMissionInfo` XML to decode; `target` is a raw `PlayerTypes` int;
`value` is the raw heuristic score.

---

## 2. Current observability — tier 0 (Oblivious)

**HTTP endpoints — what exists:**

| Endpoint | Espionage-relevant fields |
|---|---|
| `/players` | `gold`, `goldRate`, `scienceRate` — none of the espionage-specific fields |
| `/cities` | `crime`, `education`, `disease` — **none** of the espionage effect timers |
| `/units` | spy unit position, type (`UNIT_SPY` etc.), `missionAI` — adequate for spy location |
| `/diagnostic` | no espionage-specific diagnostic endpoints |

**Confirmed absent from `/players` snapshot** (CvHttpServer.cpp:61-81,
rendered at CvHttpServer.cpp:285-304):
- `espionageRate` (the COMMERCE_ESPIONAGE rate, `getCommerceRate(COMMERCE_ESPIONAGE)`)
- `espionagePercent` (the slider %, `getCommercePercent(COMMERCE_ESPIONAGE)`)
- per-team EP balance (`getEspionagePointsAgainstTeam`, one per known team)
- per-team EP spending weight (`getEspionageSpendingWeightAgainstTeam`)
- `espionagePointsEver` (the lifetime EP accumulator `m_iEspionagePointsEver`)
- per-team counterespionage turns left + mod

**Confirmed absent from `/cities` snapshot** (CvHttpServer.cpp:83-104,
rendered at CvHttpServer.cpp:339-357):
- `espionageHealthCounter` — the poison-water turns remaining
- `espionageHappinessCounter` — the unhappy turns remaining
- `espionageDefenseModifier` — the building-driven defense mod
- `disabledPowerTimer`
- `warWearinessTimer`

**Log coverage:**

| Log tag | Gate | What it covers |
|---|---|---|
| `[ESP/best]` level 1 | `gPlayerLogLevel >= 1` (the single BUG knob) | Winning mission + raw score per spy eval. Raw int enum for mission type — requires XML table to decode. Streamed to `/events` via `streamLogTee`. |
| (nothing) | — | EP income per turn, EP allocation to teams, AI weight decisions, counterespionage mission effects, city espionage effect countdowns, mission execution, EP deduction |

**Summary:** The espionage economy is essentially invisible from outside. You
can see spy unit positions and infer they exist at a location. You cannot see:
- How much EP any player is accumulating per turn
- How much EP is allocated against each team
- Whether a city is under a poison/unhappy/revolt/power-disable effect
- How effective counterespionage is (the modifier and turns-remaining)
- Why the AI chose (or skipped) an espionage mission beyond the bare winning enum

---

## 3. The gap — what cannot be reconstructed from outside today

### Critical gaps (required for render-from-API / Thought Police bar)

1. **Per-player EP rate** — `getCommerceRate(COMMERCE_ESPIONAGE)` per player, the per-turn EP income. Not in `/players`. Without it you cannot verify or predict EP accumulation.

2. **Per-player, per-team EP balance** — `getEspionagePointsAgainstTeam(team)` for every team pair. The direct currency for whether any mission can fire. Not exposed.

3. **Per-player espionage commerce percent** — the slider value `getCommercePercent(COMMERCE_ESPIONAGE)`. Needed to explain the EP rate and the AI's trade-off against research.

4. **Per-team EP spending weights** — `getEspionageSpendingWeightAgainstTeam(team)` per met team. Required to understand how EP is being directed (especially for AI players where this is the primary observable of intent).

5. **EP-ever (team lifetime)** — `getEspionagePointsEver()`. This is one input to mission cost scaling; without it the cost formula cannot be reconstructed.

6. **Counterespionage state** — per-team pair: `getCounterespionageTurnsLeftAgainstTeam` + `getCounterespionageModAgainstTeam`. Active counterespionage silently inflates all enemy mission costs; if you cannot see it you cannot explain why missions are cheaper or more expensive.

7. **City espionage effect timers** — `espionageHealthCounter`, `espionageHappinessCounter`, `disabledPowerTimer`, `warWearinessTimer` per city. These are the visible outcomes of successful missions; without them you cannot tell whether a city's poor health/happiness has an espionage cause.

8. **City espionage defense modifier** — `getEspionageDefenseModifier()` per city. Needed to reconstruct mission cost against a specific city.

9. **Mission execution events** — `doEspionageMission` fires with zero log/event output. A successful mission (gold stolen, building destroyed, tech stolen, etc.) leaves no trace in the observable surface except for its side-effects on other endpoints (gold level in `/players`, building count in `/cities`, research in `/players`). The causal link is invisible.

10. **AI weight-setting decisions** — The entire `AI_updateCommercePercent` espionage block (how weights are chosen, why the target rate was set, attitude inputs) has no logging.

---

## 4. Proposed hooks — concrete additions to reach Tier 4 (Thought Police)

All follow the existing pattern: gated by `gPlayerLogLevel` (player-scope log
domain), added to the `[ESP]` tag family in `EspionageAI.log` + streamed via
`streamLogTee`, plus snapshot fields in the `/players` and `/cities` endpoints.

### 4-A. Endpoint additions (snapshot-level; Tier 1→4)

**`/players` additions** (add to `PlayerSnap` struct, CvHttpServer.cpp:61, populated
in the snapshot builder at CvHttpServer.cpp:1524):

```jsonc
// per player — existing fields omitted
"espionageRate":  <int>,   // getCommerceRate(COMMERCE_ESPIONAGE)
"espionagePercent": <int>, // getCommercePercent(COMMERCE_ESPIONAGE)
"espionageEver":  <int>,   // GET_TEAM(t).getEspionagePointsEver()
// per-team arrays (one entry per MAX_PC_TEAMS index):
"espionageAgainst": [<int>, ...],      // getEspionagePointsAgainstTeam(i) for i=0..MAX_PC_TEAMS-1
"espionageWeights": [<int>, ...],      // getEspionageSpendingWeightAgainstTeam(i)
"counterespTurns":  [<int>, ...],      // GET_TEAM(t).getCounterespionageTurnsLeftAgainstTeam(i)
"counterespMod":    [<int>, ...]       // GET_TEAM(t).getCounterespionageModAgainstTeam(i)
```

Note on the per-team arrays: the arrays are indexed by raw `TeamTypes` enum
values (0..MAX_PC_TEAMS-1), matching the team `id` field in the `/players`
response — consumers join by `team` id. Sparse encoding (object keyed by
non-zero team ids) is an alternative if array size is a concern.

**`/cities` additions** (add to `CitySnap` struct, CvHttpServer.cpp:83):

```jsonc
"espHealthCounter":    <int>,  // getEspionageHealthCounter()
"espHappyCounter":     <int>,  // getEspionageHappinessCounter()
"espDefenseModifier":  <int>,  // getEspionageDefenseModifier()
"disabledPowerTimer":  <int>,  // getDisabledPowerTimer()
"warWearinessTimer":   <int>   // getWarWearinessTimer()
```

### 4-B. Log additions (per-turn stream; Tier 3)

**`[ESP/turn]` — per-player EP income summary (level 1)**

Emit once per player per turn from `doEspionagePoints()` (CvPlayer.cpp:15580):

```
[ESP/turn] player=%d rate=%d epEver=%d
```

Fields: player id, this turn's EP income, team's EP-ever after accrual.

**`[ESP/alloc]` — per-target-team allocation (level 2)**

Emit from `doEspionageOneOffPoints` (CvPlayer.cpp:15555) for each team that
receives points:

```
[ESP/alloc] player=%d targetTeam=%d allocated=%d total=%d weight=%d
```

Fields: player id, target team id, points allocated this turn, new accumulated
balance, spending weight.

**`[ESP/weight]` — AI weight decision summary (level 1)**

Emit at the end of the espionage block in `AI_updateCommercePercent`
(CvPlayerAI.cpp ~16773), after all weights are set:

```
[ESP/weight] player=%d espPct=%d epRate=%d targetRate=%d <w0=N w1=N w2=N ...> (one w per met team)
```

Fields: player id, new COMMERCE_ESPIONAGE percent, resulting EP rate, computed
target rate, and weight per met-alive-non-self team id.

**`[ESP/mission]` — mission execution outcome (level 1)**

Emit from `doEspionageMission` (CvPlayer.cpp:16299) on return:

```
[ESP/mission] player=%d target=%d mission=%s cost=%d happened=%d epBefore=%d epAfter=%d
```

Fields: player id, target player id, mission XML key (string), cost deducted,
whether `bSomethingHappened`, EP balance before and after deduction. The mission
key (not the raw enum int) makes log lines self-describing without XML lookup.

**`[ESP/counterspy]` — counterespionage timer tick (level 2)**

Emit from `CvTeam::doTurn` when a counterespionage timer decrements to 0
(CvTeam.cpp:1066-1068):

```
[ESP/counterspy] team=%d vs=%d modCleared=%d
```

Fields: owning team id, target team id, the mod value that was cleared.

### 4-C. Tag registration

Add `[ESP]` subsystem detail to `ai-logging-reference.md` §2 registry and §3
tag-detail section, covering `[ESP/best]` (already exists), plus the new
`[ESP/turn]`, `[ESP/alloc]`, `[ESP/weight]`, `[ESP/mission]`, `[ESP/counterspy]`.
Update the doc-comment in `BetterBTSAI.cpp` beside `logEspionageAI`.

### 4-D. Diagnostic endpoint (optional; Tier 4)

`GET /diagnostic/espionage?player=N&targetTeam=T` — on-demand point-in-time
snapshot of the full espionage state for player N against team T:

```json
{
  "player": N, "targetTeam": T,
  "epAgainst": <int>, "epRate": <int>, "espPct": <int>,
  "weight": <int>, "epEver_ours": <int>, "epEver_theirs": <int>,
  "counterespTurns": <int>, "counterespMod": <int>
}
```

This is the espionage-economy analogue of `/diagnostic/canConstruct`: a
single-query "what is the full state" answer, useful when the turn-by-turn
stream is too noisy or wasn't running.

---

## 5. Cost and risk notes

- The snapshot additions (§4-A) are trivial: struct additions + snapshot-builder
  reads. No game-logic mutation. The per-team arrays are bounded by `MAX_PC_TEAMS`
  (a compile-time constant, typically 18); at worst a few hundred bytes per player
  in the snapshot. Safe.
- The log additions (§4-B) are equally cheap: all gated by `gPlayerLogLevel`;
  zero cost when off. The existing `doEspionagePoints` + `AI_updateCommercePercent`
  call sites are the right insertion points.
- The diagnostic endpoint (§4-D) requires a mailbox slot extension (the current
  `g_evalAction` is 40 bytes — fine, "espionage" fits). Pattern already established
  by `canConstruct` / `sweep` / `placementSweep`.

---

## 6. Scope not covered here (out-of-band espionage effects)

- Passive missions (`isPassive()`, `isSeeDemographics()`, `isSeeResearch()`) reveal
  info to the spy's owner but don't change game state — no observability gap.
- The `m_mapEspionageHistory` per-turn record (CvPlayer.cpp:3970) is a Python/SDK
  history hook, not read by anything in the C++ surface — not a gap, just dormant.
- `hasStolenVisibilityTimer` / `StolenVisibilityTimer` (CvTeam) — a pre-existing
  passive visibility steal mechanic; decremented alongside counterespionage in
  `CvTeam::doTurn`. Out of scope here but similarly unobservable.
