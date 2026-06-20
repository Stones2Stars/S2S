# Observability — diplomacy & attitude

> **Status:** reference (per-system observability map) · **Verified against:** live source, 2026-06-20
> **Grounding:** `Sources/AI/CvPlayerAI.cpp`, `Sources/AI/CvTeamAI.cpp`, `Sources/AI/BetterBTSAI.cpp`,
> `Sources/Engine/CvGame.cpp`, `Sources/Engine/CvDeal.h`, `Sources/Tools/CvHttpServer.cpp`. Line numbers
> drift — confirm the named function, not the integer.
> One-paragraph orientation: this maps the per-player AI attitude value (the master variable routing every AI
> diplomatic decision), the per-turn tenure counters, the 44-type memory ledger, the war-plan state, and the
> deal/deal-evaluation surface — and why the attitude value and its decomposition are completely invisible
> from outside the running game. Read the scaffold [`README.md`](README.md) (the 0–5 scale, the Orwell bar,
> the three hook shapes) and [`http-server.md`](http-server.md) (the live surface + live-read rules) first;
> this doc does not restate them.

**System:** per-player AI attitude toward every other player; the per-turn tenure counters (at-war, at-peace,
open-borders, defensive-pact, shared-war, same-religion, favorite-civic, bonus-trade); the diplo-memory
ledger; deal evaluation; war-planning posture.
**Tier today: 1 (Telescreen)** + partial Tier 2 via the existing `[WAR/*]` and `[DIP/*]` event logs. The
attitude value and its decomposition — the number all AI decisions hinge on — are entirely opaque per-turn.

---

## 1. How it works

### 1a. The attitude value — `AI_getAttitudeVal` (`Sources/AI/CvPlayerAI.cpp:6518`)

`AI_getAttitudeVal(ePlayer, bForced)` produces an integer in [−100, 100], thresholded to an `AttitudeTypes`
enum (0=Furious … 4=Friendly) via `AI_getAttitudeFromValue`. It is **lazily computed and cached** in
`m_aiAttitudeCache[ePlayer]` (MAX_INT sentinel = stale, `CvPlayerAI.cpp:6534`), invalidated on relevant
state-changes (`AI_invalidateAttitudeCache` / `AI_invalidateCloseBordersAttitudeCache`, `CvPlayerAI.cpp:25528`).

The value is the **sum of these components**, all called inline:

| Component | Function | What it measures |
|---|---|---|
| Base | `LeaderHeadInfo::getBaseAttitude()` | Personality constant |
| Handicap | `HandicapInfo::getAttitudeChange()` | Human handicap modifier |
| AI modifier | `getAIAttitudeModifier()` | Per-player mod (XML/events) |
| Peace-weight match | `4 - abs(myPeaceWeight - theirPeaceWeight)` | Warmonger vs dove alignment |
| Warmonger respect | `min(myWarmongerRespect, theirWarmongerRespect)` | Shared militarism |
| Team-size penalty | `-max(0, theirTeamSize - myTeamSize)` | Penalty for large coalitions |
| Rank difference | scaled by `worse`/`betterRankDifferenceAttitudeChange` | Score-rank gap |
| Both low-rank bonus | +1 if both rank ≥ half the field | Sympathy among weak civs |
| Lost-war penalty | `getLostWarAttitudeChange()` if their warSuccess > mine | Military-loss grudge (`CvPlayerAI.cpp:6570`) |
| Trait attitude | `AI_getTraitAttitude(ePlayer)` | Leader-trait modifiers |
| Close borders | `AI_getCloseBordersAttitude` | Stolen city-radius plots × `getCloseBordersAttitudeChange` (`:6850`) |
| War tenure | `AI_getWarAttitude` | flat −3 + `atWarCounter / divisor` (`:6889`) |
| Peace tenure | `AI_getPeaceAttitude` | `atPeaceCounter / divisor` (`:6908`) |
| Same religion | `AI_getSameReligionAttitude` | flat + `sameReligionCounter / divisor` (`:6920`) |
| Different religion | `AI_getDifferentReligionAttitude` | flat + `diffReligionCounter / divisor` (`:6944`) |
| Bonus trade | `AI_getBonusTradeAttitude` | `bonusTradeCounter / divisor` (`:6968`) |
| Open borders | `AI_getOpenBordersAttitude` | `openBordersCounter / divisor` (`:6985`) |
| Defensive pact | `AI_getDefensivePactAttitude` | `defensivePactCounter / divisor` (`:7000`) |
| Rival DP penalty | `AI_getRivalDefensivePactAttitude` | their DP count (`:7020`) |
| Rival vassal penalty | `AI_getRivalVassalAttitude` | their vassal power (`:7038`) |
| Shared war | `AI_getShareWarAttitude` | `shareWarCounter / divisor` (`:7056`) |
| Favorite civic | `AI_getFavoriteCivicAttitude` | both run leader's fav civic (`:7078`) |
| Trade value | `AI_getTradeAttitude` | peacetime grant/trade received (`:7100`) |
| Rival trade penalty | `AI_getRivalTradeAttitude` | their peacetime grants to others (`:7107`) |
| Civic share | `AI_getCivicShareAttitude` | shared civic-category alignment (`:26486`) |
| Embassy | `AI_getEmbassyAttitude` | embassy exists |
| Civic change | `AI_getCivicAttitudeChange` | recent civic adoption |
| Memory attitude | `AI_getMemoryAttitude` (loop over `NUM_MEMORY_TYPES`) | per-event memory × per-leader weight (`:7115`) |
| Colony bonus | `AI_getColonyAttitude` | freedom appreciation for former colonies (`:7120`) |
| Attitude extra | `AI_getAttitudeExtra` | manual override (events/scripting) (`:6600`) |
| Rebel penalty | −5 (they rebel) / −3 (I rebel) | rebellion flag (`:6602`) |
| Ruthless shared-enemy | +2 if same worst-enemy under `GAMEOPTION_AI_RUTHLESS` | (`:6611`) |

Final value is `range(iAttitude, -100, 100)`. **NPC short-circuit:** either player `isNPC()` returns −100
(`:6524`). **Team/vassal short-circuit:** `bForced` + same team or uncapitulated vassal returns 100 (`:6528`).

### 1b. Per-turn counter updates

**Team-level** (`CvTeamAI::AI_doCounter`, `Sources/AI/CvTeamAI.cpp:3789`, every team turn):
`atWarCounter[T]` +1 while at war (`:3802`); `atPeaceCounter[T]` +1 while not (`:3806`); `hasMetCounter[T]`
+1 after meeting (`:3811`); `openBordersCounter[T]` +1 while OB active, reset 0 when lapsed (`:3814-3817`);
`defensivePactCounter[T]` +1 while DP active, −1/turn when lapsed (`:3819-3827`); `shareWarCounter[T]` +1
while sharing a war (`:3831-3836`); `warPlanStateCounter[T]` +1 every turn (`:3798`); `warSuccess[T]`
cumulative (changed by combat-outcome code).

**Player-level** (`CvPlayerAI::AI_doDiploCounters`, in `AI_doTurnPre`): `sameReligionCounter[P]` ±1/turn
(`CvPlayerAI.cpp:16360-16365`); `differentReligionCounter[P]` ±1/turn (`:16367-16376`);
`favoriteCivicCounter[P]` ±1/turn (`:16378-16388`); `bonusTradeCounter[P]` +`numTradeBonusImports(P)`,
fractional decay at 0 (`:16391-16400`); contact cooldown timers `AI_contactTimer[P][ContactType]` −1/turn
(`:16404-16416`).

**Memory decay** (also in `AI_doDiploCounters`, `:16418-16449`): each `memoryCount[P][MemoryType] > 0` has a
per-type per-leader `getMemoryDecayRand` chance to decrement via `SorenRandNum`. Modified by
`MODDERGAMEOPTION_REALISTIC_DIPLOMACY` (`iRand /= 1 + memoryCount`, faster for high counts) and
`GAMEOPTION_AI_RUTHLESS` (`iRand /= 3`, AI forgets fast).

### 1c. Diplo-memory ledger — `AI_changeMemoryCount` (`CvPlayerAI.cpp:16418` area)

`memoryCount[P][MemoryType]` is an integer per player-pair per event type. `NUM_MEMORY_TYPES` = 44
(`CvEnums.h:2310-2353`): war events (`MEMORY_DECLARED_WAR`, `…_ON_FRIEND`, `…_HIRED_WAR_ALLY`); violence
(`MEMORY_NUKED_US/FRIEND`, `RAZED_CITY/HOLY_CITY`, `SACKED_CITY/HOLY_CITY`); espionage (`SPY_CAUGHT`);
aid/demand (`GIVE_HELP`, `REFUSED_HELP`, `ACCEPT_DEMAND`, `REJECTED_DEMAND`, `MADE_DEMAND`, `…_RECENT`);
diplomatic stances (`ACCEPTED/DENIED_RELIGION/CIVIC`, `ACCEPTED/DENIED_JOIN_WAR`,
`ACCEPTED/DENIED_STOP_TRADING`, `STOPPED_TRADING`, `CANCELLED_OPEN_BORDERS`); trades (`TRADED_TECH_TO_US`,
`RECEIVED_TECH_FROM_ANY`); votes (`VOTED_AGAINST/FOR_US`); events (`EVENT_GOOD/BAD_TO_US`); plus
`LIBERATED_CITIES`, `INQUISITION`, `RECALLED_AMBASSADOR`, `WARMONGER`, `MADE_PEACE`, `ENSLAVED_CITIZENS`,
`BACKSTAB`, `BACKSTAB_FRIEND`, `USED_NUKE`, `HIRED_TRADE_EMBARGO`. Each count contributes
`count × leaderPercent(MemoryType) / 100` to attitude via `AI_getMemoryAttitude` (`:7115`).

### 1d. War planning state — `AI_getWarPlan` / `AI_setWarPlan` (`CvTeamAI.cpp:3252-3307`)

Each team×team pair holds a `WarPlanTypes` in `m_aeWarPlan[]`: `NO_WARPLAN`, `WARPLAN_ATTACKED_RECENT`,
`WARPLAN_ATTACKED`, `WARPLAN_PREPARING_LIMITED/TOTAL` (sneak-attack prep, AI only), `WARPLAN_LIMITED`,
`WARPLAN_TOTAL`, `WARPLAN_DOGPILE`. Transitions log `[WAR/warplan]` at level 1 (`:3302`). `AI_doWar` (in
`AI_doTurnPost`) escalates/de-escalates on enemy power, funding, and `atWarCounter`. The baseline
`[WAR/begin]` fires once per team per turn with `enemyPowerPct`, `fundedPct`, `atWar` count, `warPlans` count
(`:3946`).

### 1e. Deal evaluation — `AI_dealVal` / `AI_considerOffer`

`AI_dealVal(ePlayer, pList, bIgnoreAnnual, iChange)` (`CvPlayerAI.cpp:7733`) values a trade list as an integer
(gold-equivalent) — the core scoring function, used by `AI_considerOffer` / `AI_counterPropose`. Every item
type is scored (tech, resources, cities, gold, gold-per-turn, maps, vassalage, OB, DP, peace, war, embargo,
civic, religion, embassy, contacts, corporation, votes, workers, units). `AI_considerOffer(ePlayer,
pTheirList, pOurList, iChange)` (`:7944`) applies the comparison and returns accept/reject, logging
`[DIP/begin]` (lvl 1), `[DIP/decision] verdict=reject reason=denial` (lvl 1, untradeable item),
`[DIP/score]` (lvl 2, ourValue vs theirValue), `[DIP/decision] verdict=ACCEPT|reject` (lvl 1, final).
`[DIP/cand]` (lvl 3) traces per-item contributions inside `AI_dealVal`.

### 1f. Live deal ledger — `CvGame::getNumDeals()` / `getDeal(iID)`

Active deals live in `CvGame`'s deal list. Each `CvDeal` (`Sources/Engine/CvDeal.h`) holds `getFirstPlayer()`/
`getSecondPlayer()`, `m_firstTrades`/`m_secondTrades` (per-side item lists), `getInitialGameTurn()`, and
`doTurn()` (per-turn renewal/expiry). Annual deals (`TRADE_GOLD_PER_TURN`, `TRADE_RESOURCES`) fire effects
from `doTurn()`. There is no bulk endpoint for the ledger.

---

## 2. What's on the wire today

**Tier 1 + partial Tier 2.** Exposed:

| Source | What you get |
|---|---|
| `/players` → `id`, `team` | Team / coalition membership |
| `/players` → `score`, `era` | Rank difference is an attitude input |
| `/players` → `human`, `npc` | Whether attitude computation runs at all |
| `/players` → `gold`, `goldRate` | Partly explains peacetime grant/trade inputs |
| `[WAR/begin]` (≥1) | Per-team `enemyPowerPct`, `fundedPct`, `safePct`, `atWar` count, `warPlans` count (`CvTeamAI.cpp:3946`) |
| `[WAR/warplan]` (≥1) | Every war-plan transition (old→new enum) |
| `[WAR/area]` (≥1) | Area-level posture changes |
| `[DIP/begin]` `[DIP/decision]` (≥1) | Trade-offer accept/reject + ourValue/theirValue |
| `[DIP/score]` `[DIP/dealval]` (≥2) | ourValue vs theirValue; total deal value |
| `[DIP/cand]` (≥3) | Per-item value in `AI_dealVal` |
| `[PERF/phase]` `AI_doDiplo` | Wall-clock ms for the diplomacy phase |

### What is NOT exposed (the gap)

The core gap is the **attitude total and its decomposition**: `AI_getAttitudeVal(A,B)` (the integer all AI
decisions hinge on), `AI_getAttitude(A,B)` (the enum), and the per-component breakdown — none logged
per-turn, none on `/players`. Beyond it, all opaque: every team-level counter (`atWarCounter`,
`atPeaceCounter`, `hasMetCounter`, `openBordersCounter`, `defensivePactCounter`, `shareWarCounter`,
`warSuccess`, `warPlanStateCounter`); every player-level counter (`sameReligionCounter`,
`differentReligionCounter`, `favoriteCivicCounter`, `bonusTradeCounter`); the full 44-type × MAX_PLAYERS
memory ledger and its probabilistic decay outcomes; the **steady-state** warplan enum (only transitions are
logged via `[WAR/warplan]`), `warPlanStateCounter`, and `AI_isChosenWar`/`AI_isSneakAttackPreparing`; the
deal ledger (parties/items/turn-struck) plus `peacetimeTradeValue`/`peacetimeGrantValue` and the enemy
equivalents driving `AI_getTradeAttitude`/`AI_getRivalTradeAttitude`; contact cooldown timers; and the static
personality inputs (`getBaseAttitude`, `AI_getPeaceWeight` with its random component, the per-leader divisors).

---

## 3. The gap

At Tier 1 + partial Tier 2 an agent can answer (via the `[WAR/*]`/`[DIP/*]` logs): did war-plan transitions
happen this turn? What was the trade-offer verdict and values? What is the baseline war-posture pressure? It
**cannot** answer without the screen or a debugger: what is A's current attitude toward B (the value driving
every overture, war/trade/vote decision)? Why is A friendly/hostile (the counters and memory counts
decomposing it)? How long have A and B been at war / at peace / open-borders? What is the memory ledger for
`DECLARED_WAR`, `RAZED_CITY`, etc.? Is A preparing a sneak attack? What is the current deal between A and B,
and what trade value has A accumulated from B?

**For AI players specifically, this is the worst gap in the whole observability surface:** attitude is the
*master variable* routing every AI diplomatic decision — trades, war declarations, contact overtures, votes
are all gated on `AI_getAttitudeVal`. With no attitude export, an AI player is a black box despite the
`/units` / `/players` / `/cities` snapshots.

---

## 4. Proposed hooks

All hooks are one of the three canonical shapes — see
[DEC-obs-hook-shapes](../../architecture/decisions.md#dec-obs-hook-shapes).

### Hook A — `[DIP/attitude]` per-turn attitude snapshot (cheapest, highest leverage)

In `CvPlayerAI::AI_doDiploCounters` (`CvPlayerAI.cpp:16350` area), after the counter updates, for each met
living non-NPC rival; level 1:
```
[DIP/attitude] turn=N player=A vs=B val=iAttitude tier=PLEASED closeBorders=X warTenure=X peaceTenure=X religion=X memory=X extra=X
```
Calls `AI_getAttitudeVal` (already cached after the counter-update invalidation) + key sub-components. Makes
attitude **visible per-turn in `/events`** for every pair — the foundational hook.

| Field | Source |
|---|---|
| `val` | `AI_getAttitudeVal(ePlayer)` |
| `tier` | `AI_getAttitude(ePlayer)` enum name |
| `closeBorders` | `AI_getCloseBordersAttitude(ePlayer)` |
| `warTenure` | `AI_getWarAttitude(ePlayer)` |
| `peaceTenure` | `AI_getPeaceAttitude(ePlayer)` |
| `religion` | `AI_getSameReligionAttitude + AI_getDifferentReligionAttitude` |
| `memory` | sum over i of `AI_getMemoryAttitude(ePlayer, i)` |
| `extra` | `AI_getAttitudeExtra(ePlayer)` |

Level 2 adds `tradeVal=`, `shareWar=`, `openBorders=`, `defensivePact=`, `favCivic=`. Level 3 adds the
per-`MEMORY_*` breakdown (44 fields).

### Hook B — `[DIP/counter]` once-per-team per-turn counter snapshot (level 2)

In `CvTeamAI::AI_doCounter` (`CvTeamAI.cpp:3789`), at the end of the per-pair iteration, for each met living
rival team:
```
[DIP/counter] turn=N team=A vs=B atWar=X atPeace=X hasMet=X openBorders=X defPact=X shareWar=X warSuccess=X warPlanState=X
```
The raw integer counters feeding the attitude components — reconstruct accumulating state rather than
inferring it from `[WAR/warplan]` transitions.

### Hook C — `[DIP/memory]` on memory write (event-driven, level 1)

In `AI_changeMemoryCount` (`CvPlayerAI.cpp:16415` area) when `iChange != 0`:
```
[DIP/memory] player=A vs=B memory=MEMORY_DECLARED_WAR delta=+1 newCount=3
```
The causal record of "what happened to the ledger." Combined with the per-turn decay, a reader reconstructs
the full memory history.

### Hook D — `[DIP/decay]` on probabilistic memory decay (level 2)

In the memory-decay loop in `AI_doDiploCounters` (`CvPlayerAI.cpp:16418` area) when a count decrements:
```
[DIP/decay] player=A vs=B memory=MEMORY_RAZED_CITY from=3 to=2 rand=42 threshold=20
```
The probabilistic decay otherwise makes memory evolution invisible — two games with identical events diverge silently.

### Hook E — `/players` snapshot fields for attitude/war state

Add to `PlayerSnap` and the per-player walk (`Sources/Tools/CvHttpServer.cpp`):

| JSON key | Source | Notes |
|---|---|---|
| `"stateReligion"` | `kPlayer.getStateReligion()` key or `"NONE"` | Reconstructs same/different-religion components |
| `"isAtWar"` | `GET_TEAM(eTeam).isAtWar()` | At war with anyone? |
| `"warTeams"` | array of at-war team IDs (`for each T: isAtWar(T)`) | Which teams — minimum to infer which counters tick |
| `"peaceWeight"` | `kPlayer.AI_getPeaceWeight()` | Peace-weight match input (random component baked at game-start) |

### Hook F — `[DIP/warplan]` steady-state snapshot (level 1)

Supplement to the transition-only `[WAR/warplan]`: in `[WAR/begin]` or a new per-pair block, for each rival
with a non-`NO_WARPLAN` warplan:
```
[DIP/warplan] turn=N team=A vs=B plan=WARPLAN_PREPARING_LIMITED state=12 chosen=1 sneakPrep=1
```
Makes the current warplan readable every turn even when the agent missed the transition event.

### Hook G — `/diagnostic/attitude?player=A&vs=B` endpoint

Mailbox-evaluated (the `canConstruct` pattern) — the on-demand spot-check for diplomacy state. Evaluates
`AI_getAttitudeVal` and all sub-components on the game thread (no logic duplication — the same calls as the
live game), and exposes all 44 memory types for the pair:
```json
{
  "player": 0, "vs": 1, "val": 3, "tier": "ATTITUDE_PLEASED",
  "components": { "base":1, "handicap":0, "peaceWeight":2, "warmongerRespect":1, "teamSize":0,
    "rank":-1, "lostWar":0, "closeBorders":-2, "warTenure":0, "peaceTenure":2, "sameReligion":3,
    "diffReligion":0, "bonusTrade":1, "openBorders":1, "defensivePact":0, "rivalDP":0, "rivalVassal":0,
    "shareWar":0, "favCivic":2, "trade":1, "rivalTrade":0, "civicShare":0, "embassy":1, "civicChange":0,
    "memory":-3, "colony":0, "extra":0 },
  "counters": { "atWar":0, "atPeace":45, "hasMet":120, "openBorders":30, "defensivePact":0, "shareWar":12,
    "warSuccess":0, "sameReligion":5, "favCivic":3, "bonusTrade":8 },
  "memoryLedger": { "MEMORY_DECLARED_WAR":0, "MEMORY_RAZED_CITY":1, "...":"..." }
}
```

**Priority:** Hook **A** (`[DIP/attitude]` — attitude on the wire, the foundational unblock) + Hook **C**
(`[DIP/memory]`) + Hook **E** (`stateReligion`/`isAtWar`) for Tier 3 → Hooks **B**/**D**/**F**/**G** for full
Tier-4 reconstruction (counters, decay, steady-state warplan, on-demand decomposition).

---

## 5. Cascade relevance (#428/#430)

The attitude system is a prime cascade candidate and one of the most demanding
([DEC-map-before-delete](../../architecture/decisions.md#dec-map-before-delete)):

- **Counters** (`atWarCounter`, `atPeaceCounter`, `openBordersCounter`, …) are additive domain tallies — the
  tally's natural domain; accumulating per-turn from boolean predicates, they can drive `requires` atoms directly.
- **Memory ledger** is harder: event-driven increments + probabilistic decay. The additive counts fit the
  tally, but probabilistic decay is a maintainer, not a simple `requires` — it needs a `requires.operate`
  equivalent or an event-driven modifier.
- **Attitude value itself** is a COMPUTED reduction over 25+ tallied sub-counts × per-leader weights — not a
  tally count. Threshold atoms over components are expressible; the full linear-sum is not directly a
  `requires.build` condition without a derived-field layer.
- **War plan** (`WarPlanTypes`) is an imperatively-managed state machine (`AI_doWar`); expressing it as
  cascade `requires` means encoding the war-decision logic (power ratios, counter thresholds, funding) — a
  substantial migration.
- **Deals** are transient mutual agreements, not building-like facts; they accumulate into
  `peacetimeTradeValue` (a persistent effect the tally fits) — the active deal items are harder.

**Minimum observability before any cascade replacement:** Hooks A + B + C + D (attitude + counters + memory
events visible) so the cascade shadow can be validated turn-by-turn against the legacy `AI_getAttitudeVal`.
The memory ledger's probabilistic decay makes it an opaque-system case until Hooks C + D land.

---

## See also
- [`README.md`](README.md) — the observability scale, the Orwell bar, and the three hook shapes this map applies.
- [`http-server.md`](http-server.md) — the live surface (`/players`, `/diagnostic/*`, `/events`) these hooks extend, and the live-read rules.
- [`../../architecture/decisions.md`](../../architecture/decisions.md) — [DEC-obs-scale], [DEC-obs-hook-shapes], [DEC-map-before-delete].
- [`../../explanation/cascade-architecture.md`](../../explanation/cascade-architecture.md) — why total observability is load-bearing, and the cascade/tally model the counters and memory ledger feed.
- [`../../README.md`](../../README.md) — the comprehension map (overview-of-overviews).
