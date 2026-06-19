> DRAFT observability map (2026-06-18, parent agent) — claims cited from code; verify before relying.

# Observability map: Diplomacy & Attitude

**System:** Per-player AI attitude toward every other player; the per-turn counters
(at-war tenure, at-peace tenure, open-borders tenure, defensive-pact tenure,
shared-war tenure, same-religion tenure, favorite-civic tenure, bonus-trade
tenure); the diplo-memory ledger; deal evaluation state; war planning posture.

**Anchor files:**
- `Sources/CvPlayerAI.cpp` — `AI_getAttitudeVal` (CvPlayerAI.cpp:6518), `AI_doDiplo` (:17424), `AI_doDiploCounters` (:16350), `AI_dealVal` (:7733), `AI_considerOffer` (:7944)
- `Sources/CvTeamAI.cpp` — `AI_doCounter` (:3789), `AI_setWarPlan` (:3293), `AI_doWar` (:3946)
- `Sources/CvHttpServer.cpp` — `publishIfDue` (:1450 ff.), `PlayerSnap` struct (:61)
- `Sources/BetterBTSAI.cpp` — `logDiploAI` (:166), `logWarAI` (:186)

**Context:** #428/#430 "render from API without looking at the screen" bar
(cascade-mapping-inventory.md §A/§D).

---

## 1. How it actually works

### 1a. The attitude value — `AI_getAttitudeVal` (CvPlayerAI.cpp:6518)

`AI_getAttitudeVal(ePlayer, bForced)` produces an integer in [-100, 100] that is
immediately thresholded to an `AttitudeTypes` enum (0=Furious … 4=Friendly) via
`AI_getAttitudeFromValue`. It is **lazily computed and cached** in
`m_aiAttitudeCache[ePlayer]` (MAX_INT sentinel = stale; CvPlayerAI.cpp:6534).
The cache is invalidated on state-changes that matter:
`AI_invalidateAttitudeCache` / `AI_invalidateCloseBordersAttitudeCache`
(CvPlayerAI.cpp:25528 ff.).

The formula is the **sum of these components**, all called inline:

| Component | Function | What it measures |
|---|---|---|
| Base | `LeaderHeadInfo::getBaseAttitude()` | Personality constant |
| Handicap | `HandicapInfo::getAttitudeChange()` | Human handicap modifier |
| AI modifier | `getAIAttitudeModifier()` | Per-player mod (XML/events) |
| Peace-weight match | `4 - abs(myPeaceWeight - theirPeaceWeight)` | Warmonger vs dove alignment |
| Warmonger respect | `min(myWarmongerRespect, theirWarmongerRespect)` | Shared militarism appreciation |
| Team-size penalty | `-max(0, theirTeamSize - myTeamSize)` | Penalty for large coalitions |
| Rank difference | Scaled by `worseRankDifferenceAttitudeChange` / `betterRankDifferenceAttitudeChange` | Score rank gap |
| Both low-rank bonus | +1 if both players rank ≥ half the field | Sympathy among weak civs |
| Lost-war penalty | `getLostWarAttitudeChange()` if `ePlayer.warSuccess > me.warSuccess` | Grudge for military loss (CvPlayerAI.cpp:6570) |
| Trait attitude | `AI_getTraitAttitude(ePlayer)` | Leader-trait modifiers |
| Close borders | `AI_getCloseBordersAttitude(ePlayer)` | Stolen city-radius plots × `getCloseBordersAttitudeChange()` (CvPlayerAI.cpp:6850) |
| War tenure | `AI_getWarAttitude(ePlayer)` | Current-war flat -3 + `atWarCounter / divisor` (CvPlayerAI.cpp:6889) |
| Peace tenure | `AI_getPeaceAttitude(ePlayer)` | `atPeaceCounter / divisor` (CvPlayerAI.cpp:6908) |
| Same religion | `AI_getSameReligionAttitude(ePlayer)` | Flat + `sameReligionCounter / divisor` (CvPlayerAI.cpp:6920) |
| Different religion | `AI_getDifferentReligionAttitude(ePlayer)` | Flat + `diffReligionCounter / divisor` (CvPlayerAI.cpp:6944) |
| Bonus trade | `AI_getBonusTradeAttitude(ePlayer)` | `bonusTradeCounter / divisor` (CvPlayerAI.cpp:6968) |
| Open borders | `AI_getOpenBordersAttitude(ePlayer)` | `openBordersCounter / divisor` (CvPlayerAI.cpp:6985) |
| Defensive pact | `AI_getDefensivePactAttitude(ePlayer)` | `defensivePactCounter / divisor` (CvPlayerAI.cpp:7000) |
| Rival DP penalty | `AI_getRivalDefensivePactAttitude(ePlayer)` | Scaled by their DP count (CvPlayerAI.cpp:7020) |
| Rival vassal penalty | `AI_getRivalVassalAttitude(ePlayer)` | Scaled by their vassal power (CvPlayerAI.cpp:7038) |
| Shared war | `AI_getShareWarAttitude(ePlayer)` | `shareWarCounter / divisor` (CvPlayerAI.cpp:7056) |
| Favorite civic | `AI_getFavoriteCivicAttitude(ePlayer)` | Both running leader's fav civic (CvPlayerAI.cpp:7078) |
| Trade value | `AI_getTradeAttitude(ePlayer)` | Peacetime grant/trade value received (CvPlayerAI.cpp:7100) |
| Rival trade penalty | `AI_getRivalTradeAttitude(ePlayer)` | Enemy peacetime grants to others (CvPlayerAI.cpp:7107) |
| Civic share | `AI_getCivicShareAttitude(ePlayer)` | Shared civic category alignment (CvPlayerAI.cpp:26486) |
| Embassy | `AI_getEmbassyAttitude(ePlayer)` | Embassy exists |
| Civic change | `AI_getCivicAttitudeChange(ePlayer)` | Recent civic adoption affecting this leader |
| Memory attitude | `AI_getMemoryAttitude(ePlayer, MEMORY_*)` (loop over `NUM_MEMORY_TYPES`) | Per-event memory × per-leader weight percent (CvPlayerAI.cpp:7115) |
| Colony bonus | `AI_getColonyAttitude(ePlayer)` | Freedom appreciation for former colonies (CvPlayerAI.cpp:7120) |
| Attitude extra | `AI_getAttitudeExtra(ePlayer)` | Manually-set override (events, scripting) (CvPlayerAI.cpp:6600) |
| Rebel penalty | -5 (ePlayer is rebel) or -3 (I am rebel) | Rebellion flag (CvPlayerAI.cpp:6602) |
| Ruthless shared-enemy | +2 if same worst-enemy under GAMEOPTION_AI_RUTHLESS | (CvPlayerAI.cpp:6611) |

Final value is `range(iAttitude, -100, 100)` stored in `m_aiAttitudeCache[ePlayer]`.

**NPC short-circuit:** if either player `isNPC()`, returns -100 immediately (no computation). (CvPlayerAI.cpp:6524)
**Team/vassal short-circuit:** if `bForced` and same team or uncapitulated vassal, returns 100 (CvPlayerAI.cpp:6528).

### 1b. Per-turn counter updates — `AI_doCounter` (CvTeamAI.cpp:3789) and `AI_doDiploCounters` (CvPlayerAI.cpp:16350)

**Team-level counters** (CvTeamAI::AI_doCounter, runs every team turn):
- `atWarCounter[T]` — incremented each turn while at war with team T (CvTeamAI.cpp:3802).
- `atPeaceCounter[T]` — incremented each turn while NOT at war with T (CvTeamAI.cpp:3806).
- `hasMetCounter[T]` — incremented each turn after meeting team T (CvTeamAI.cpp:3811).
- `openBordersCounter[T]` — incremented while open borders active; reset to 0 when lapsed (CvTeamAI.cpp:3814-3817).
- `defensivePactCounter[T]` — incremented while DP active; decremented by 1/turn when lapsed (CvTeamAI.cpp:3819-3827).
- `shareWarCounter[T]` — incremented while sharing a war with T (CvTeamAI.cpp:3831-3836).
- `warPlanStateCounter[T]` — incremented every turn (time-in-warplan counter) (CvTeamAI.cpp:3798).
- `warSuccess[T]` — cumulative war-success points (battles won/lost weighted; changed by combat outcome code).

**Player-level counters** (CvPlayerAI::AI_doDiploCounters, runs in `AI_doTurnPre`):
- `sameReligionCounter[P]` — ±1/turn based on matching state religions (CvPlayerAI.cpp:16360-16365).
- `differentReligionCounter[P]` — ±1/turn based on mismatched state religions (CvPlayerAI.cpp:16367-16376).
- `favoriteCivicCounter[P]` — ±1/turn if both run leader's favorite civic (CvPlayerAI.cpp:16378-16388).
- `bonusTradeCounter[P]` — +`numTradeBonusImports(P)` while importing from P; decays fractionally when 0 imports (CvPlayerAI.cpp:16391-16400).
- Contact cooldown timers `AI_contactTimer[P][ContactType]` — all active cooldowns decrement by 1/turn (CvPlayerAI.cpp:16404-16416).

**Memory decay** (also in `AI_doDiploCounters`, CvPlayerAI.cpp:16418-16449):
Each `memoryCount[P][MemoryType]` > 0 has a per-`MemoryType` per-leader `getMemoryDecayRand` chance to decrement by 1 each turn via `SorenRandNum`. The decay rate is modified by:
- `MODDERGAMEOPTION_REALISTIC_DIPLOMACY`: `iRand /= 1 + memoryCount` (faster decay for high counts).
- `GAMEOPTION_AI_RUTHLESS`: `iRand /= 3` (3× faster decay; AI forgets transgressions quickly).

### 1c. Diplo-memory ledger — `AI_changeMemoryCount` (CvPlayerAI.cpp:16418 area)

`memoryCount[P][MemoryType]` is an integer count (0..N) per player pair per event type. There are `NUM_MEMORY_TYPES` = 44 types (CvEnums.h:2310-2353) covering:
- War events: `MEMORY_DECLARED_WAR`, `MEMORY_DECLARED_WAR_ON_FRIEND`, `MEMORY_HIRED_WAR_ALLY`
- Violence: `MEMORY_NUKED_US/FRIEND`, `MEMORY_RAZED_CITY/HOLY_CITY`, `MEMORY_SACKED_CITY/HOLY_CITY`
- Espionage: `MEMORY_SPY_CAUGHT`
- Aid/demand events: `MEMORY_GIVE_HELP`, `MEMORY_REFUSED_HELP`, `MEMORY_ACCEPT_DEMAND`, `MEMORY_REJECTED_DEMAND`, `MEMORY_MADE_DEMAND`, `MEMORY_MADE_DEMAND_RECENT`
- Diplomatic stances: `MEMORY_ACCEPTED_RELIGION/CIVIC`, `MEMORY_DENIED_RELIGION/CIVIC`, `MEMORY_ACCEPTED/DENIED_JOIN_WAR`, `MEMORY_ACCEPTED/DENIED_STOP_TRADING`, `MEMORY_STOPPED_TRADING`, `MEMORY_CANCELLED_OPEN_BORDERS`
- Trades: `MEMORY_TRADED_TECH_TO_US`, `MEMORY_RECEIVED_TECH_FROM_ANY`
- Votes: `MEMORY_VOTED_AGAINST/FOR_US`
- Events: `MEMORY_EVENT_GOOD/BAD_TO_US`
- Others: `MEMORY_LIBERATED_CITIES`, `MEMORY_INQUISITION`, `MEMORY_RECALLED_AMBASSADOR`, `MEMORY_WARMONGER`, `MEMORY_MADE_PEACE`, `MEMORY_ENSLAVED_CITIZENS`, `MEMORY_BACKSTAB`, `MEMORY_BACKSTAB_FRIEND`, `MEMORY_USED_NUKE`, `MEMORY_HIRED_TRADE_EMBARGO`

Each count contributes `count × leaderPercent(MemoryType) / 100` to attitude via `AI_getMemoryAttitude` (CvPlayerAI.cpp:7115).

### 1d. War planning state — `AI_getWarPlan` / `AI_setWarPlan` (CvTeamAI.cpp:3252-3307)

Each team × team pair holds a `WarPlanTypes` enum in `m_aeWarPlan[]`:
- `NO_WARPLAN` — no active plan
- `WARPLAN_ATTACKED_RECENT` — attacked this turn (very recent)
- `WARPLAN_ATTACKED` — being attacked, responding
- `WARPLAN_PREPARING_LIMITED` / `WARPLAN_PREPARING_TOTAL` — sneak-attack prep phase (AI only)
- `WARPLAN_LIMITED` — limited war
- `WARPLAN_TOTAL` — all-out war
- `WARPLAN_DOGPILE` — pile-on an already-fighting enemy

Transitions are logged via `[WAR/warplan]` at level 1 (CvTeamAI.cpp:3302). `AI_doWar` (called in `AI_doTurnPost`) is where the AI decides to escalate/de-escalate warplans based on enemy power, funding, and the `atWarCounter` time-in-war. The baseline log `[WAR/begin]` fires once per team per turn with `enemyPowerPct`, `fundedPct`, `atWar` count, and `warPlans` count (CvTeamAI.cpp:3946).

### 1e. Deal evaluation — `AI_dealVal` / `AI_considerOffer`

`AI_dealVal(ePlayer, pList, bIgnoreAnnual, iChange)` (CvPlayerAI.cpp:7733) values a trade list as an integer (gold-equivalent). It is the core scoring function — used by `AI_considerOffer` and `AI_counterPropose`. Every trade item type is scored: tech, resources, cities, gold, gold-per-turn, maps, surrender/vassal, open borders, defensive pact, peace, war, embargo, civic, religion, embassy, contacts, corporation, votes, workers, military units.

`AI_considerOffer(ePlayer, pTheirList, pOurList, iChange)` (CvPlayerAI.cpp:7944) applies the deal-value comparison and returns accept/reject. The logged outputs are:
- `[DIP/begin]` (lvl 1) — entry point, list sizes, iChange.
- `[DIP/decision] verdict=reject reason=denial` (lvl 1) — any item we cannot trade.
- `[DIP/score]` (lvl 2) — ourValue vs theirValue comparison.
- `[DIP/decision] verdict=ACCEPT|reject` (lvl 1) — final verdict + values.

The `[DIP/cand]` (lvl 3) lines trace per-item contribution inside `AI_dealVal`.

### 1f. Live deal ledger — `CvGame::getNumDeals()` / `getDeal(iID)`

Active deals are stored in `CvGame`'s deal list. Each `CvDeal` (CvDeal.h) holds:
- `getFirstPlayer()` / `getSecondPlayer()` — the two parties.
- `m_firstTrades` / `m_secondTrades` — the trade-item lists for each side.
- `getInitialGameTurn()` — when the deal was struck.
- `doTurn()` — per-turn renewal/expiry processing.

Annual deals (`TRADE_GOLD_PER_TURN`, `TRADE_RESOURCES`) are active ongoing contracts; their per-turn effects fire from `doTurn()`. There is no bulk endpoint for the deal ledger.

---

## 2. Current observability

### What is exposed today

| Source | What you get |
|---|---|
| `GET /players` → `id`, `team` | Who belongs to which team (coalition membership) |
| `GET /players` → `score` | Score rank is visible — rank difference is one attitude input |
| `GET /players` → `era` | Needed for rank-difference math |
| `GET /players` → `human`, `npc` | Whether the player runs attitude computation at all |
| `GET /players` → `gold`, `goldRate` | Partly explains peacetime-grant/trade inputs |
| `[WAR/begin]` (gPlayerLogLevel≥1) | Per-team: `enemyPowerPct`, `fundedPct`, `safePct`, `atWar` count, `warPlans` count (CvTeamAI.cpp:3946) |
| `[WAR/warplan]` (gPlayerLogLevel≥1) | Every war-plan transition with old→new plan enum |
| `[WAR/area]` (gPlayerLogLevel≥1) | Area-level posture changes |
| `[DIP/begin]` `[DIP/decision]` (gPlayerLogLevel≥1) | Trade-offer accept/reject verdict + ourValue/theirValue |
| `[DIP/score]` (gPlayerLogLevel≥2) | ourValue vs theirValue comparison |
| `[DIP/dealval]` (gPlayerLogLevel≥2) | Total deal value computed in `AI_dealVal` |
| `[DIP/cand]` (gPlayerLogLevel≥3) | Per-item value contribution in `AI_dealVal` |
| `[PERF/phase]` `CvPlayerAI::AI_doDiplo` | Wall-clock ms for the diplomacy turn phase |

### What is NOT exposed (the gap)

**Attitude total and decomposition** — the core gap:

| State | Why it matters | Missing surface |
|---|---|---|
| `AI_getAttitudeVal(A, B)` | The computed integer attitude — the number all AI decisions hinge on | Not in `/players`, not logged per-turn |
| `AI_getAttitude(A, B)` | The AttitudeTypes enum (FURIOUS/CAUTIOUS/PLEASED/FRIENDLY) | Not exported |
| Per-component attitude breakdown | Which component is driving a hostile/friendly stance | No per-component log |

**Counters** (all team-level and player-level, none exported):

| Counter | Attitude component it drives |
|---|---|
| `atWarCounter[team]` | `AI_getWarAttitude` |
| `atPeaceCounter[team]` | `AI_getPeaceAttitude` |
| `hasMetCounter[team]` | `AI_getTradeAttitude` denominator |
| `openBordersCounter[team]` | `AI_getOpenBordersAttitude` |
| `defensivePactCounter[team]` | `AI_getDefensivePactAttitude` |
| `shareWarCounter[team]` | `AI_getShareWarAttitude` |
| `warSuccess[team]` | `AI_getLostWarAttitude` |
| `warPlanStateCounter[team]` | Time-in-warplan (affects peace-evaluation thresholds) |
| `sameReligionCounter[player]` | `AI_getSameReligionAttitude` |
| `differentReligionCounter[player]` | `AI_getDifferentReligionAttitude` |
| `favoriteCivicCounter[player]` | `AI_getFavoriteCivicAttitude` |
| `bonusTradeCounter[player]` | `AI_getBonusTradeAttitude` |

**Memory ledger** (44 types × MAX_PLAYERS, all opaque):

| State | Impact |
|---|---|
| `memoryCount[P][MEMORY_*]` for all 44 types | Contributes to attitude via `AI_getMemoryAttitude`; decays probabilistically — the entire historical record of who did what to whom |
| Memory decay RNG outcomes | Random decay means two games with identical events diverge silently |

**War planning state** (partially visible via `[WAR/warplan]` events but no snapshot):

| State | Missing surface |
|---|---|
| Current `warPlan[team]` enum for every pair | Not in `/players` snapshot; only transitions are logged, not the steady state |
| `warPlanStateCounter[team]` | Time spent in warplan — affects AI war-continuation logic |
| Whether war was "chosen" vs "attacked" | `AI_isChosenWar` and `AI_isSneakAttackPreparing` not exported |

**Deal ledger** (completely opaque from outside):

| State | Impact |
|---|---|
| Active deals (parties, items, turn struck) | `peacetimeTradeValue` / `peacetimeGrantValue` accumulate from deals → `AI_getTradeAttitude`. No deal list endpoint. |
| `peacetimeTradeValue[P]`, `peacetimeGrantValue[P]` | Drive `AI_getTradeAttitude` (the historical trade-generosity tracker) |
| `enemyPeacetimeGrantValue[T]`, `enemyPeacetimeTradeValue[T]` | Drive `AI_getRivalTradeAttitude` — penalises a player for trading with our enemies |
| Contact cooldown timers `contactTimer[P][ContactType]` | Gate which AI-initiated contacts can fire this turn — determines whether the AI even attempts an overture |

**Personality inputs** (static, but currently not in snapshot):

| State | Impact |
|---|---|
| Leader personality type | The divisors, limits, and flat changes for every attitude component are personality-specific |
| `AI_getPeaceWeight()` | Peace-weight match is a meaningful attitude factor (4 - abs(A - B)); the random component is invisible |
| `getBaseAttitude()` per leader | Starting floor of attitude — varies widely |

---

## 3. The gap

At current **Tier 1 (Telescreen)** plus partial Tier 2 (via the `[WAR/*]` and `[DIP/*]` logs), we can answer:

- "Did war-plan transitions happen this turn?" — yes, via `[WAR/warplan]`.
- "What was the trade-deal accept/reject verdict and values?" — yes, via `[DIP/decision]`.
- "What is the baseline war-posture pressure this turn?" — yes, via `[WAR/begin]`.

We **cannot** answer without screen or debugger:

- "What is Player A's current attitude toward Player B?" — the computed value that drives all AI overtures, war decisions, trade decisions, and vote choices. Completely opaque per-turn.
- "Why is Player A friendly/hostile to Player B?" — the counter values and memory counts that decompose the attitude are invisible.
- "How many turns have A and B been at war / at peace?" — `atWarCounter` and `atPeaceCounter` not exported.
- "Does A have open borders with B? For how long?" — `openBordersCounter` not exported.
- "What is the memory ledger for DECLARED_WAR, RAZED_CITY, etc.?" — all 44 memory types × player-pairs invisible.
- "Is A preparing a sneak attack on B?" — `warPlan` enum not in snapshot; only transitions are logged.
- "What is the current deal between A and B?" — no deal-list endpoint.
- "What trade values has A accumulated from B?" — `peacetimeTradeValue` / `peacetimeGrantValue` invisible.

**For AI players specifically**, this is the worst gap in the whole system: attitude is the *master variable* that routes every AI diplomatic decision. An AI player's trades, war declarations, contact overtures, and votes are all gated on `AI_getAttitudeVal`. With no attitude export, an AI player is a black box despite the `/units` / `/players` / `/cities` snapshots.

---

## 4. Proposed hooks (concrete additions to climb toward Tier 3/4)

All additions follow the existing `[DIP/*]` / `[WAR/*]` pattern: gated by `gPlayerLogLevel`,
`key=value` space-separated, emitted via `logDiploAI` / `logWarAI` to the respective file
(also teed to `/events` via `streamLogTee`). No AI logic changes — observation only.

### Hook A — `[DIP/attitude]` per-turn attitude snapshot (cheapest, highest leverage)

Add to `CvPlayerAI::AI_doDiploCounters` (the per-turn diplo maintenance function, CvPlayerAI.cpp:16350 area), after the counter updates, for each met living non-NPC rival:

```
[DIP/attitude] turn=N player=A vs=B val=iAttitude tier=PLEASED closeBorders=X warTenure=X peaceTenure=X religion=X memory=X extra=X
```

Level 1. Calls `AI_getAttitudeVal(ePlayer)` (already cached after the counter updates forced a recompute via invalidation) and the key sub-components for decomposition. This single tag makes attitude **visible per-turn in the `/events` stream** for every player pair — the foundational hook for the whole system.

| Field | Source call |
|---|---|
| `val` | `AI_getAttitudeVal(ePlayer)` |
| `tier` | `AI_getAttitude(ePlayer).getType()` (enum name) |
| `closeBorders` | `AI_getCloseBordersAttitude(ePlayer)` |
| `warTenure` | `AI_getWarAttitude(ePlayer)` |
| `peaceTenure` | `AI_getPeaceAttitude(ePlayer)` |
| `religion` | `AI_getSameReligionAttitude(ePlayer) + AI_getDifferentReligionAttitude(ePlayer)` |
| `memory` | `sum over i: AI_getMemoryAttitude(ePlayer, (MemoryTypes)i)` |
| `extra` | `AI_getAttitudeExtra(ePlayer)` |

Level 2: add `tradeVal=`, `shareWar=`, `openBorders=`, `defensivePact=`, `favCivic=` for the remaining components.
Level 3: add per-`MEMORY_*` breakdown: `mem_DECLARED_WAR=N mem_RAZED_CITY=N …` (44 fields — level 3 only).

### Hook B — `[DIP/counter]` once-per-team per-turn counter snapshot

Add to `CvTeamAI::AI_doCounter` (CvTeamAI.cpp:3789), at the end of the per-team-pair iteration, for each met living rival team:

```
[DIP/counter] turn=N team=A vs=B atWar=X atPeace=X hasMet=X openBorders=X defPact=X shareWar=X warSuccess=X warPlanState=X
```

Level 2. These are the raw integer counters that feed the attitude components — makes the accumulating state visible and lets a reader reconstruct the `atWarCounter` history rather than inferring it from `[WAR/warplan]` transitions.

### Hook C — `[DIP/memory]` on memory write (event-driven)

Add to `AI_changeMemoryCount` (CvPlayerAI.cpp:16415 area) when `iChange != 0`:

```
[DIP/memory] player=A vs=B memory=MEMORY_DECLARED_WAR delta=+1 newCount=3
```

Level 1. Logs the event that caused the memory change — the causal record of "what happened to the ledger." Combined with the per-turn decay from `AI_doDiploCounters`, a reader can reconstruct the full memory history.

### Hook D — `[DIP/decay]` on probabilistic memory decay

Add to the memory-decay loop in `AI_doDiploCounters` (CvPlayerAI.cpp:16418 area) when a count actually decrements:

```
[DIP/decay] player=A vs=B memory=MEMORY_RAZED_CITY from=3 to=2 rand=42 threshold=20
```

Level 2. The probabilistic nature of decay currently makes memory evolution invisible — you cannot tell from attitude alone whether memory has decayed. This tag lets a reader track decay turn by turn.

### Hook E — `/players` snapshot fields for attitude and war state

Add to `PlayerSnap` (CvHttpServer.cpp:61) and the per-player snapshot walk (CvHttpServer.cpp:1524 ff.):

| JSON key | Source call | Notes |
|---|---|---|
| `"stateReligion"` | `kPlayer.getStateReligion()` XML key or `"NONE"` | Needed to reconstruct same/different-religion attitude components |
| `"isAtWar"` | `GET_TEAM(eTeam).isAtWar()` (boolean) | Is this player currently at war with anyone? |
| `"warTeams"` | array of team IDs this team is at war with | Which teams — `for each T: isAtWar(T)` |
| `"peaceWeight"` | `kPlayer.AI_getPeaceWeight()` | Peace-weight match is an attitude component; the random component baked in at game-start |

These are snapshot fields (per publish interval) rather than event-driven. `warTeams` is the minimum needed to infer whether `atWarCounter` or `atPeaceCounter` is ticking for any given pair.

### Hook F — `[DIP/warplan]` snapshot (supplement to event-driven `[WAR/warplan]`)

Add to `[WAR/begin]` or a new per-pair block: for each rival team with a non-`NO_WARPLAN` warplan, emit:

```
[DIP/warplan] turn=N team=A vs=B plan=WARPLAN_PREPARING_LIMITED state=12 chosen=1 sneakPrep=1
```

Level 1. The current `[WAR/warplan]` is transition-only — you can miss the steady state if the agent wasn't listening at the transition. This snapshot-style line makes the current warplan readable every turn even without a transition event.

### Hook G — `/diagnostic/attitude?player=A&vs=B` endpoint

Add a mailbox-evaluated diagnostic endpoint (same pattern as `canConstruct`):

```json
{
  "player": 0, "vs": 1,
  "val": 3, "tier": "ATTITUDE_PLEASED",
  "components": {
    "base": 1, "handicap": 0, "peaceWeight": 2, "warmongerRespect": 1,
    "teamSize": 0, "rank": -1, "lostWar": 0, "closeBorders": -2,
    "warTenure": 0, "peaceTenure": 2, "sameReligion": 3, "diffReligion": 0,
    "bonusTrade": 1, "openBorders": 1, "defensivePact": 0, "rivalDP": 0,
    "rivalVassal": 0, "shareWar": 0, "favCivic": 2, "trade": 1,
    "rivalTrade": 0, "civicShare": 0, "embassy": 1, "civicChange": 0,
    "memory": -3, "colony": 0, "extra": 0
  },
  "counters": {
    "atWar": 0, "atPeace": 45, "hasMet": 120, "openBorders": 30,
    "defensivePact": 0, "shareWar": 12, "warSuccess": 0,
    "sameReligion": 5, "favCivic": 3, "bonusTrade": 8
  },
  "memoryLedger": {
    "MEMORY_DECLARED_WAR": 0,
    "MEMORY_RAZED_CITY": 1,
    "...": "..."
  }
}
```

This endpoint evaluates `AI_getAttitudeVal` and all sub-components on the game thread (no logic duplication — same calls as the live game). It is the on-demand spot-check equivalent of `canConstruct` for diplomacy state. The `memoryLedger` object exposes all 44 types for the given pair.

---

## 5. Cascade/tally implications

The attitude system is a prime candidate for cascade representation, and also one of the most demanding:

- **Counters** (`atWarCounter`, `atPeaceCounter`, `openBordersCounter`, etc.) are additive domain tallies — the tally's natural domain. They accumulate per-turn from boolean predicates (am I at war? do I have open borders?). As tally DOMAIN counts they can drive `requires` atoms directly.
- **Memory ledger** is harder: it is event-driven (incremented on specific game events, decayed probabilistically). The tally handles additive counts well, but probabilistic decay is a maintainer, not a simple `requires` condition. This needs a `requires.operate` equivalent or an event-driven modifier.
- **Attitude value itself** is a COMPUTED derived quantity (25+ components summed). It is not itself a tally count — it is a reduction over many tallied sub-counts × per-leader weights. The cascade spec's `requires` model can express threshold atoms over attitude components, but the full linear-sum formula is not directly representable as a `requires.build` condition without additional machinery (a derived-field layer above the raw tally).
- **War plan** (`WarPlanTypes`) is a state-machine value managed imperatively by `AI_doWar`. Converting it to cascade `requires` would require expressing the war-decision logic (power ratios, counter thresholds, funding checks) as cascade conditions — a substantial migration scope.
- **Deals** (the live contract ledger) are transient mutual agreements, not building-like persistent facts. They accumulate into `peacetimeTradeValue` (a persistent effect). That accumulated value fits the tally; the active deal items are harder.

**Minimum observability needed before any cascade replacement:** Hooks A + B + C + D (attitude + counters + memory events visible) so the cascade shadow can be validated turn-by-turn against the legacy `AI_getAttitudeVal` result.

---

## Summary

| Dimension | Current state |
|---|---|
| **Tier** | 1 — Telescreen (+ partial Tier 2 from `[WAR/*]`/`[DIP/*]` event logs) |
| **Exposed** | Score/era/team per player (snapshot); war-plan transitions + war-posture baseline (`[WAR/*]`); trade-deal accept/reject verdicts (`[DIP/*]`); wall-clock timing for `AI_doDiplo` phase (`[PERF]`) |
| **Opaque** | Attitude value (total and per-component); all 12 team-level counters; all player-level counters; 44-type × MAX_PLAYERS memory ledger; current warplan enum (snapshot); deal ledger; peacetimeTradeValue / peacetimeGrantValue |
| **Minimum hooks for Tier 3** | Hook A (`[DIP/attitude]` per-turn val + key components) + Hook C (`[DIP/memory]` event-driven) + Hook E (`/players` stateReligion + isAtWar) |
| **Hooks for Tier 4 (full attitude reconstruction)** | All of A+B+C+D+E+F+G above |
| **Cascade-blocking gap** | Attitude value is the master variable for all AI diplomatic decisions; it is unobservable. No cascade shadow for this system can be validated until Hook A lands. The memory ledger's probabilistic decay makes it an §A opaque system until Hooks C+D are added. |
