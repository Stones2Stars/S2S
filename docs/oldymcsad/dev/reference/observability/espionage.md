# Observability — Espionage — what's on the wire for the EP economy & missions

> **Status:** reference   ·   **Verified against:** 2026-06-20 (function sites confirmed in `Sources/Engine/CvPlayer.cpp`, `Sources/Engine/CvTeam.cpp`, `Sources/AI/CvPlayerAI.cpp`; line numbers drift)
> **Grounding:** live `Sources/Engine/CvPlayer.cpp` (`doEspionagePoints`, `doEspionageOneOffPoints`, `getEspionageSpending`, `getEspionageMissionCost`/`Modifier`, `doEspionageMission`), `Sources/Engine/CvTeam.cpp` (counterespionage timers), `Sources/Engine/CvCity.cpp` (effect counters, `getEspionageDefenseModifier`), `Sources/AI/CvPlayerAI.cpp` (`AI_updateCommercePercent`, `AI_bestPlotEspionage`, `[ESP/best]`), `Sources/Tools/CvHttpServer.cpp` (`/players`, `/cities`). Carried over from the old draft map; function sites re-confirmed, paths re-grounded to the reorganized `Sources/` tree.
> Espionage is **Tier 0 (Oblivious)** today: no espionage-specific state appears in any HTTP endpoint. The one AI log tag (`[ESP/best]`) covers only the mission-selection commit — not EP income, weight allocation, counterespionage, or city-level effect timers. The full EP economy is invisible from outside the screen. This map walks EP income → per-team allocation → mission cost/execution → counterespionage → city effects → AI selection, names what's dark, and proposes the snapshot fields / log tags / diagnostic endpoint to climb to Tier 4.

The observability scale (0–5) and the three canonical hook shapes are defined once in
[`README.md`](README.md) ([DEC-obs-scale], [DEC-obs-hook-shapes]); the live surface and the rules for
reading it (logs held open mid-session, use `/events` + `/diagnostic`) live in
[`http-server.md`](http-server.md). This doc does not restate them.

> Line numbers below are anchors at time of writing and **drift** — confirm the named function, not the
> integer.

---

## 1. How it actually works

### 1a. EP income — per-turn accrual

`CvPlayer::doTurn` calls `doEspionagePoints()` (CvPlayer.cpp:3813), which calls
`doEspionageOneOffPoints(getCommerceRate(COMMERCE_ESPIONAGE))` (CvPlayer.cpp:15580).
`getCommerceRate(COMMERCE_ESPIONAGE)` is the player's total EP rate from the commerce slider and city
outputs — the same value as the espionage slider in the UI.

`doEspionageOneOffPoints(iChange)` (CvPlayer.cpp:15555):
1. Increments `CvTeam::m_iEspionagePointsEver` by `iChange` (`changeEspionagePointsEver`).
2. Splits the EP into per-target-team buckets via `getEspionageSpending` (CvPlayer.cpp:15569) and calls
   `CvTeam::changeEspionagePointsAgainstTeam` — points only go against already-met teams.

The espionage commerce rate is gated off if no teams have been met
(`COMMERCE_ESPIONAGE && 0 == GET_TEAM().getHasMetCivCount(true)`, CvPlayer.cpp:13200).

### 1b. EP allocation — per-target-team weights

The split is controlled by `m_aiEspionageSpendingWeightAgainstTeam[]` (CvPlayer.h), one int per team, range
0–99 (clamped in `setEspionageSpendingWeightAgainstTeam`).

`getEspionageSpending(eAgainstTeam, iTotal)` (CvPlayer.cpp:15585): sums weights for met/alive/non-self teams
→ `iTotalWeight`; assigns `(iTotalPoints * weight[i]) / iTotalWeight` proportionally; remainder goes to the
highest-weight team(s) round-robin (all tied-best if all weights are 0).

**AI weight-setting** — the espionage block of `AI_updateCommercePercent` (CvPlayerAI.cpp, ~16534–16823),
run in `AI_doTurnPost`: resets all weights to 0; for each met/alive team computes a weight + target from
attitude, war status, their-EP-ever vs our-EP-ever, desired mission costs, and `MEMORY_SPY_CAUGHT`; then
raises the `COMMERCE_ESPIONAGE` percent (lowering research) until
`getCommerceRate(COMMERCE_ESPIONAGE) >= iEspionageTargetRate` or the `iMaxEspionage` cap. **Not logged.**

### 1c. Accumulated EP per team-pair

`CvTeam::m_aiEspionagePointsAgainstTeam[]` (CvTeam.h), one per target team — incremented by
`doEspionageOneOffPoints`, decremented by `doEspionageMission` on success (CvPlayer.cpp:16707). The global
`m_iEspionagePointsEver` (CvTeam.h) records the team's lifetime EP, feeding the "my EP-ever vs their EP-ever"
cost factor (§1d). It is also recorded in `m_mapEspionageHistory` keyed by turn (CvPlayer.cpp:3970) — a
dormant Python/SDK hook, not exposed.

### 1d. Mission cost

`getEspionageMissionCost` (CvPlayer.cpp:15758) = `baseCost × costModifier / 100 × numTeamMembers`.
`getEspionageMissionBaseCost` (CvPlayer.cpp:15780) picks one of ~17 mission-type branches.
`getEspionageMissionCostModifier` (CvPlayer.cpp:16167) is a multiplicative chain: city pop, trade route,
shared religion / holy city, city culture ratio, target's `getEspionageDefenseModifier`, distance from
capital, spy fortify bonus, the **EP-ever ratio**
(`ESPIONAGE_SPENDING_MULTIPLIER × (2×theirEver + ourEver) / (theirEver + 2×ourEver)`, CvPlayer.cpp:16256),
the target team's counterespionage mod against us (CvPlayer.cpp:16269), embassy discount, and Free-Trade
discount.

### 1e. Mission execution

`doEspionageMission` (CvPlayer.cpp:16299) executes one pass. On success (`bSomethingHappened`): deducts
`iMissionCost` from `m_aiEspionagePointsAgainstTeam[targetTeam]` (CvPlayer.cpp:16707). Effects: building /
project demolition, culture insertion, city poison/unhappy/revolt counters, civic/religion switch, anarchy,
research sabotage, counterespionage, nuclear bomb. **None emit any log line or event** — only side-effects
(gold in `/players`, building count in `/cities`) leak, with the causal link invisible.

### 1f. Counterespionage state

On a counterespionage mission (`kMission.getCounterespionageMod() > 0`):
`CvTeam::changeCounterespionageTurnsLeftAgainstTeam(targetTeam, iTurns)` and
`changeCounterespionageModAgainstTeam(targetTeam, mod)`, where
`mod = kMission.getCounterespionageMod() + 5 × spy.currInterceptionProbability()` (CvPlayer.cpp:16689). The
timer counts down 1/turn in `CvTeam::doTurn` (CvTeam.cpp:1061); at 0 it clears
(`setCounterespionageModAgainstTeam(team, 0)`). The active mod multiplies mission cost against the team that
set it.

### 1g. City-level effect timers

Four per-city timers, all counting down each `CvCity::doTurn`:

| Timer | Getter | Effect | Decremented at |
|---|---|---|---|
| `m_iEspionageHealthCounter` | `getEspionageHealthCounter()` | unhealthy by counter | CvCity.cpp:1409 |
| `m_iEspionageHappinessCounter` | `getEspionageHappinessCounter()` | unhappy by counter | CvCity.cpp:1414 |
| `m_iDisabledPowerTimer` | `getDisabledPowerTimer()` | power disabled | (doTurn) |
| `m_iWarWearinessTimer` | `getWarWearinessTimer()` | war weariness | (doTurn) |

None are in the HTTP snapshot. (The happiness/health counters also feed the happiness/health ledgers —
see [`health-happiness.md`](health-happiness.md).)

### 1h. City espionage defense modifier

`getEspionageDefenseModifier()` (CvCity.cpp:14238) — building-driven per-city modifier that raises mission
cost against that city (fed into `getEspionageMissionCostModifier`). Not exposed.

### 1i. AI mission selection

`CvPlayerAI::AI_bestPlotEspionage` (CvPlayerAI.cpp:15147) iterates spy plots, enumerates all mission types,
scores each via `AI_espionageVal` (CvPlayerAI.cpp:15504), and returns the highest. Driven by attitude
weight, war plan, EP balance, city property values, and mission cost.

The **one** log line in the entire espionage system, emitted at level 1 after `AI_bestPlotEspionage`
(CvPlayerAI.cpp:15495), gated by `gPlayerLogLevel`, teed to `/events`:

```
[ESP/best] player=%d spyAt=(%d,%d) mission=%d target=%d value=%d
```

`mission` is a raw `EspionageMissionInfo` enum int (needs XML lookup to decode); `target` is a raw
`PlayerTypes`; `value` is the raw heuristic score.

---

## 2. What's on the wire today — **Tier 0 (Oblivious)**

### What exists

| Endpoint | Espionage-relevant fields |
|---|---|
| `/players` | `gold`, `goldRate`, `scienceRate` — none of the espionage-specific fields |
| `/cities` | `crime`, `education`, `disease` — **none** of the espionage effect timers |
| `/units` | spy position, `type` (`UNIT_SPY` etc.), `missionAI` — adequate for spy location only |
| `/diagnostic` | no espionage-specific endpoints |

**Confirmed absent from `/players`** (player snapshot walk in `CvHttpServer.cpp`): `espionageRate`
(`getCommerceRate(COMMERCE_ESPIONAGE)`), `espionagePercent` (`getCommercePercent(COMMERCE_ESPIONAGE)`),
per-team EP balance (`getEspionagePointsAgainstTeam`), per-team weight
(`getEspionageSpendingWeightAgainstTeam`), `espionagePointsEver`, per-team counterespionage turns + mod.

**Confirmed absent from `/cities`** (city snapshot walk): `espionageHealthCounter`,
`espionageHappinessCounter`, `espionageDefenseModifier`, `disabledPowerTimer`, `warWearinessTimer`.

**Log coverage:** only `[ESP/best]` (level 1, `gPlayerLogLevel`) — the winning mission + raw score per spy
eval. EP income per turn, EP allocation to teams, AI weight decisions, counterespionage effects, city
effect countdowns, and mission execution/EP deduction are all untagged.

---

## 3. The gap

An agent watching the endpoints + logs today can see spy unit positions and infer they exist somewhere. It
**cannot** see: how much EP any player accrues per turn; how much EP is allocated against each team; whether
a city is under a poison/unhappy/revolt/power-disable effect; how effective counterespionage is (mod +
turns); or why the AI chose/skipped a mission beyond the bare winning enum. A successful mission (gold
stolen, building destroyed, tech stolen) leaves no trace except its side-effects on other endpoints, with
the causal link invisible. This blocks reconstructing the EP economy and shadowing any cascade that would
represent espionage state ([DEC-map-before-delete]).

---

## 4. Proposed hooks — climbing from Tier 0 to Tier 4

All hooks follow the three canonical hook shapes — see [DEC-obs-hook-shapes].

### 4a. Endpoint additions (snapshot-level; Tier 0 → 4)

**`/players`** (add to `PlayerSnap` + the player snapshot walk):

```jsonc
"espionageRate":    <int>,        // getCommerceRate(COMMERCE_ESPIONAGE)
"espionagePercent": <int>,        // getCommercePercent(COMMERCE_ESPIONAGE)
"espionageEver":    <int>,        // GET_TEAM(t).getEspionagePointsEver()
"espionageAgainst": [<int>, ...], // getEspionagePointsAgainstTeam(i), i=0..MAX_PC_TEAMS-1
"espionageWeights": [<int>, ...], // getEspionageSpendingWeightAgainstTeam(i)
"counterespTurns":  [<int>, ...], // getCounterespionageTurnsLeftAgainstTeam(i)
"counterespMod":    [<int>, ...]  // getCounterespionageModAgainstTeam(i)
```

Per-team arrays are indexed by raw `TeamTypes` (0..MAX_PC_TEAMS-1), matching the `team` id in `/players` —
consumers join by team id. (Sparse object keyed by non-zero team ids is an alternative if size matters.)

**`/cities`** (add to `CitySnap`):

```jsonc
"espHealthCounter":   <int>,  // getEspionageHealthCounter()
"espHappyCounter":    <int>,  // getEspionageHappinessCounter()
"espDefenseModifier": <int>,  // getEspionageDefenseModifier()
"disabledPowerTimer": <int>,  // getDisabledPowerTimer()
"warWearinessTimer":  <int>   // getWarWearinessTimer()
```

### 4b. Log additions (per-turn stream; Tier 3)

New `logEspionageAI` helper, `[ESP]` tags, gated `gPlayerLogLevel` (zero cost when off):

| Tag | Level | Site | Line |
|---|---|---|---|
| `[ESP/turn]` | 1 | `doEspionagePoints` (CvPlayer.cpp:15580) | `player= rate= epEver=` |
| `[ESP/alloc]` | 2 | `doEspionageOneOffPoints` (CvPlayer.cpp:15555) per team | `player= targetTeam= allocated= total= weight=` |
| `[ESP/weight]` | 1 | end of the espionage block in `AI_updateCommercePercent` | `player= espPct= epRate= targetRate= w0=N w1=N ... (one per met team)` |
| `[ESP/mission]` | 1 | `doEspionageMission` return (CvPlayer.cpp:16299) | `player= target= mission=<XMLkey> cost= happened= epBefore= epAfter=` |
| `[ESP/counterspy]` | 2 | `CvTeam::doTurn` when a timer hits 0 (CvTeam.cpp:1066) | `team= vs= modCleared=` |

The `mission` key is the XML string (not the raw enum int) so `[ESP/mission]` lines are self-describing.

### 4c. Tag registration

Register `[ESP]` in the AI-logging tag registry (covering existing `[ESP/best]` plus the new tags) and
update the `logEspionageAI` doc-comment.

### 4d. `/diagnostic/espionage?player=N&targetTeam=T` (Tier 4)

The espionage analogue of `/diagnostic/canConstruct` — a mailbox-serviced point-in-time snapshot of the
full state for player N against team T (a single "what is the full state" answer when the turn stream is too
noisy or wasn't running):

```json
{
  "player": N, "targetTeam": T,
  "epAgainst": <int>, "epRate": <int>, "espPct": <int>,
  "weight": <int>, "epEver_ours": <int>, "epEver_theirs": <int>,
  "counterespTurns": <int>, "counterespMod": <int>
}
```

---

## 5. Cost & priority

| Hook | Tier gain | Effort | Rationale |
|---|---|---|---|
| `/players` + `/cities` fields (§4a) | 0→4 (snapshot) | struct + walk reads; per-team arrays bounded by `MAX_PC_TEAMS` (~18), a few hundred bytes/player | The whole EP economy becomes readable for ALL players |
| `[ESP/turn]`/`[ESP/mission]`/`[ESP/weight]` (§4b L1) | →3 | gated, zero cost off | Live EP income + mission outcomes + AI intent on `/events` |
| `[ESP/alloc]`/`[ESP/counterspy]` (§4b L2) | 3 | gated | Per-team allocation + counterespionage expiry |
| `/diagnostic/espionage` (§4d) | 4 | mailbox slot (`canConstruct`/`sweep` pattern) | On-demand full state for cascade shadow |

---

## 6. Scope not covered here

- Passive missions (`isPassive()`, `isSeeDemographics()`, `isSeeResearch()`) reveal info to the spy's owner
  but don't change game state — no observability gap.
- `m_mapEspionageHistory` (CvPlayer.cpp:3970) — a dormant Python/SDK history hook, not read by the C++
  surface; not a gap.
- `hasStolenVisibilityTimer` / `StolenVisibilityTimer` (CvTeam) — a passive visibility-steal mechanic
  decremented alongside counterespionage in `CvTeam::doTurn`; out of scope here but similarly unobservable.

---

## 7. Code cross-reference

> Paths re-grounded to the reorganized `Sources/` tree (`Cv*` engine → `Sources/Engine/`, `Cv*AI` →
> `Sources/AI/`, `CvHttpServer` → `Sources/Tools/`). Line numbers drift — confirm the function.

| Claim | Source |
|---|---|
| `doEspionagePoints` / `doEspionageOneOffPoints` | `Sources/Engine/CvPlayer.cpp:3813 / 15555` |
| `getEspionageSpending` (allocation) | `Sources/Engine/CvPlayer.cpp:15585` |
| `getEspionageMissionCost` / `...BaseCost` / `...CostModifier` | `Sources/Engine/CvPlayer.cpp:15758 / 15780 / 16167` |
| `doEspionageMission` (execution + EP deduct) | `Sources/Engine/CvPlayer.cpp:16299, 16707` |
| EP-ever cost ratio | `Sources/Engine/CvPlayer.cpp:16256` |
| counterespionage timer countdown | `Sources/Engine/CvTeam.cpp:1061` |
| city effect-timer countdown | `Sources/Engine/CvCity.cpp:1409–1417` |
| `getEspionageDefenseModifier` | `Sources/Engine/CvCity.cpp:14238` |
| `AI_updateCommercePercent` (espionage weight block) | `Sources/AI/CvPlayerAI.cpp:~16534–16823` |
| `AI_bestPlotEspionage` / `AI_espionageVal` / `[ESP/best]` | `Sources/AI/CvPlayerAI.cpp:15147 / 15504 / 15495` |
| `PlayerSnap` / `CitySnap` (add fields here) | `Sources/Tools/CvHttpServer.cpp` |

---

## See also
- [`README.md`](README.md) — the observability scaffold: the 0–5 scale ([DEC-obs-scale]), the Orwell bar,
  and the three canonical hook shapes ([DEC-obs-hook-shapes]) this map's hooks instantiate.
- [`http-server.md`](http-server.md) — the live surface (`/players`, `/cities`, `/diagnostic`) these hooks
  extend, and the live-read rules (logs held open mid-session).
- [`health-happiness.md`](health-happiness.md) — the espionage happiness/health counters and
  `warWearinessTimer` feed the happiness/health ledgers mapped there.
- [`../../explanation/cascade-architecture.md`](../../explanation/cascade-architecture.md) — why total
  observability is load-bearing: the EP economy cannot be shadowed/cut until it is on the wire
  ([DEC-map-before-delete]).
- [`../../README.md`](../../README.md) — the comprehension map / overview-of-overviews.

[DEC-obs-scale]: ../../architecture/decisions.md#dec-obs-scale
[DEC-obs-hook-shapes]: ../../architecture/decisions.md#dec-obs-hook-shapes
[DEC-map-before-delete]: ../../architecture/decisions.md#dec-map-before-delete
