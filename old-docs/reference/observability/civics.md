# Civics — observability map

> DRAFT observability map (2026-06-18, parent: Sonnet fan-out sweep) — all claims cited from
> live source; verify before relying. Cross-reference: `cascade-mapping-inventory.md` §A/§D,
> `http-server.md`, `ai-logging-reference.md`.

---

## 1. How it actually works

### 1a. What a civic is

A civic is an empire-wide persistent policy choice. Players hold one civic per `CivicOptionTypes`
slot (the "category": Government, Legal, Labour, Economy, Religion, …). The active set lives in
`CvPlayer::m_paeCivics[]` (one `CivicTypes` per option). NPCs hold civics but `processCivics`
is a no-op for them (`CvPlayer.cpp:17995`).

### 1b. Eligibility gate: `canDoCivics`

`CvPlayer::canDoCivics(CivicTypes)` (`CvPlayer.cpp:8448`) — the single per-civic eligibility gate:

1. `NO_CIVIC` always passes.
2. If the game has force-set this civic option (`GC.getGame().isForceCivicOption`), only the
   forced civic passes.
3. For non-NPC players, returns `false` if EITHER:
   - The civic has a `CityLimit > 0`, no `CityOverLimitUnhappy`, and the player already has more
     cities than the limit (city-count gate), OR
   - The player has not yet unlocked the civic's `CivicOptionType` via `isHasCivicOption`
     **AND** the team hasn't researched the civic's `TechPrereq` (`CvPlayer.cpp:8462-8474`).

So the "can I even pick this civic" check is: **tech prereq fulfilled OR civic option already
unlocked (from a prior civic), and city count within limit if applicable.**

### 1c. Revolution: triggering the switch

`CvPlayer::canRevolution(CivicTypes*)` (`CvPlayer.cpp:8482`): blocked if (a) already in anarchy,
(b) NPC, (c) `getRevolutionTimer() > 0`, or (d) no eligible civic differs from the current set.

`CvPlayer::revolution(CivicTypes*, bool bForce)` (`CvPlayer.cpp:8527`):

1. Saves old civics array.
2. Computes `iAnarchyLength = getCivicAnarchyLength(paeNewCivics)` — may be 0.
3. Calls `changeAnarchyTurns(iAnarchyLength)` if > 0.
4. For each changed slot: calls `setCivics(option, newCivic)`, records a `civcSwitchInstance`
   into `m_civicSwitchHistory`.
5. `NoteCivicsSwitched(iCivicChanges)`.
6. Sets `revoutionTimer = max(1, ((100 + anarchyModifier) * MIN_REVOLUTION_TURNS) / 100) +
   iAnarchyLength` (`CvPlayer.cpp:8568`).

**Revolution timer = the cooldown between revolutions.** A player cannot start another revolution
while this timer > 0 (checked in `canRevolution`). It ticks down 1/turn in `CvPlayer::doTurn`
(`CvPlayer.cpp:3783-3786`).

### 1d. Anarchy: formula and tick-down

**Civic anarchy length (`getCivicAnarchyLength`, `CvPlayer.cpp:8939`):**

- 0 if in a Golden Age.
- 0 if `getMaxAnarchyTurns() < 1` (e.g. Spiritual trait).
- For each changed civic whose `getAnarchyLength() > 0`: `iTotalAnarchyLength += anarchyLength * 100`.
- Quantity discount when N > 1 changes: `total -= total * N * CIVIC_ANARCHY_QTY_DISCOUNT / 100`.
- Scale by game speed: `* speedPercent / 100`.
- Add city count: `+= numCities * worldNumCitiesAnarchyPercent`.
- Apply `getAnarchyModifier()` and `getCivicAnarchyModifier()`: `* (mod + 100) / 100` each.
- Apply era factor: `* eraAnarchyPercent / 100`.
- Rebel discount: `/= 2` if `isRebel()`.
- Final `/= 100` (the centipercent scale).
- Clamp: `max(1, range(result, getMinAnarchyTurns(), getMaxAnarchyTurns()))`.

**Religion anarchy length (`getReligionAnarchyLength`, `CvPlayer.cpp:9001`):** similar but based
on `BASE_RELIGION_ANARCHY_LENGTH` global define + city-count + `getAnarchyModifier()` +
`getReligiousAnarchyModifier()` from traits.

**Anarchy tick-down:** `CvPlayer::doTurn` (`CvPlayer.cpp:3859-3862`):
- If `getAnarchyTurns() > 0`: increment `m_iNumAnarchyTurns` stat counter, call
  `changeAnarchyTurns(-1)`.
- Golden age and anarchy are mutually exclusive; starting a golden age clears anarchy
  (`changeAnarchyTurns(-getAnarchyTurns())` at `CvPlayer.cpp:9409`).

**Max/min anarchy:** `getMaxAnarchyTurns()` (`CvPlayer.cpp:9548`) — minimum of `MAX_ANARCHY_TURNS`
global and any trait `getMaxAnarchy()` overrides (Spiritual sets 0). `getMinAnarchyTurns()`
(`CvPlayer.cpp:9580`) — max trait `getMinAnarchy()`, capped by `getMaxAnarchyTurns()`.

**Anarchy modifier:** `m_iAnarchyModifier` (`CvPlayer.cpp:9615`) — accumulates from buildings
(`changeAnarchyModifier`) and traits (`changeCivicAnarchyModifier`). Applied to both civic and
religion anarchy lengths. Also adjusts revolution/conversion timers on change.

**Policy civics:** `isPolicy()` civics (`CvPlayer.cpp:8956`) are excluded from anarchy computation
even when switched — they are zero-cost changes.

### 1e. Civic effects: `processCivics` and `setCivics`

`CvPlayer::setCivics(CivicOptionTypes, CivicTypes)` (`CvPlayer.cpp:14288`):
- Updates `m_paeCivics[eIndex]`.
- NPCs return early after the raw array write.
- For non-NPCs: calls `processCivics(oldCivic, -1)` then `processCivics(newCivic, +1)`.
- Fires `CvEventReporter::civicChanged(player, old, new)` → Python `onCivicChanged` callback
  (`CvPlayer.cpp:14384`). This is Python only — NOT the SSE event spine.
- Dirties UI caches.

`CvPlayer::processCivics(eCivic, iChange, bLimited)` (`CvPlayer.cpp:17990`) applies/removes the
full civic effect bundle:

**Full mode (`!bLimited`) — the complete empire-wide effect list:**
- Great People / Great General rate modifiers.
- Maintenance modifiers (distance, num-cities, home-area, other-area).
- Free experience, worker speed, improvement upgrade rate, military production modifier.
- Free unit upkeep (civilian/military, flat and pop-scaled), upkeep modifiers.
- Max conscript, free specialists, trade routes.
- State-religion production/unit/building/free-XP modifiers.
- Commerce rate modifiers per type (gold/research/culture/espionage), capital modifiers, specialist
  extra commerce, landmark yields.
- Building happiness/health changes (sparse), building production modifiers, building commerce
  modifiers.
- Unit production modifiers (per unit type), unit combat production modifiers.
- Feature happiness changes.
- Specialist validity, free specialist counts, specialist yield/commerce per-type.
- Improvement yield changes (per improvement × yield type).
- Terrain yield changes (per terrain × yield type).
- Foreign trade route modifier, religion spread rate, corporation spread modifier, distant unit
  support cost modifier, extra city defense.
- Freedom fighters, enslavement chance, civic inflation, hurry cost/inflation modifiers.
- Landmark happiness, no-landmark-anger, fixed borders, freedom-fighter flag.
- Revolution index modifiers (local, national, distance, holy city, nationality, bad/good religion).
- City limit, city-over-limit unhappy, foreign unhappy percent.
- War weariness modifier.
- All-religions-active count (religious tolerance flag), bans-non-state-religions count.
- Hurry type flags.
- `SpecialBuildingNotRequired` counts (the §14 B-iii group gate).
- `StateReligionCount`, non-state-religion-spread ban, state/non-state religion happiness.
- Inquisition counts.
- Vote source secretary-general clearing (on remove).
- Bonus minted percent, bonus commerce modifier.
- Civic happiness, largest-city happiness, no-capital-unhappiness, tax-rate unhappiness,
  happy-per-military-unit.
- Civic health, no-unhealthy-population, building-only-healthy.
- Population growth rate percentage, corporation maintenance modifier, military food production.
- No-foreign-trade, no-corporations, no-foreign-corporations counts.

**Limited mode (`bLimited` = true):** only building happiness/health changes, feature happiness
changes, and specialist validity — the subset needed for the AI civic valuation's temporary
test-swap without a full recalculation.

### 1f. AI civic decision: `AI_doCivics`

`CvPlayerAI::AI_doCivics()` (`CvPlayerAI.cpp:16848`) runs on AI (non-human, non-NPC) players
from `AI_doTurnPre` (`CvPlayerAI.cpp:497`):

1. Decrements `m_turnsSinceLastRevolution`, decays `m_iCivicSwitchMinDeltaThreshold`.
2. Checks `AI_getCivicTimer()` — if > 0, ticks down and returns (the cooldown between evaluations).
3. Checks `canRevolution(NULL)` — if false, returns.
4. For each civic option: finds the best available civic via `AI_bestCivic` (using a temporary
   `processCivics` bLimited test-swap to evaluate interaction effects).
5. Iterates to convergence (max `getNumCivicOptionInfos()` passes).
6. If the best set differs from current AND the value delta exceeds `m_iCivicSwitchMinDeltaThreshold`:
   - Checks near-future civics (techs within `20 * anarchyLength` turns) — may defer the switch.
   - Costs the anarchy against a benefit estimate (`perTurnDelta * min(50, turnsSinceRevolution) *
     speedPercent`).
   - Drops the least-efficient civic change from the set if anarchy cost > benefit.
7. Calls `revolution(paeBestCivic)` if `bDoRevolution` and `canRevolution` still holds.
8. Sets `AI_getCivicTimer()` to `CIVIC_CHANGE_DELAY` (25 turns) or `MIN_REVOLUTION_TURNS` (for
   no-anarchy/golden-age cases).

Logging emitted today:
- `[DAI/civic/cand]` (level 3) — per-candidate flavor contribution during `AI_civicValue`.
- `[DAI/civic/best]` (level 2) — the REVOLUTION commit decision (player, curValue, bestValue) and
  each changed option.

### 1g. AI civic evaluation timer

`m_iAICivicTimer` decrements 1/turn; when 0, `AI_doCivics` runs. Set to `CIVIC_CHANGE_DELAY=25`
after any evaluation (revolutionary or not) and `MIN_REVOLUTION_TURNS` if no-anarchy-applicable.
Serialized in `CvPlayerAI` save stream (`CvPlayerAI.cpp:19939` reads `m_turnsSinceLastRevolution`).

### 1h. Cooldowns and re-evaluation triggers

- **Revolution timer** (`m_iRevolutionTimer`, `CvPlayer.cpp:11115`): the "can't start another
  revolution for N turns" cooldown. Ticks down 1/turn. Blocks `canRevolution`.
- **Conversion timer** (`m_iConversionTimer`): same mechanic for religion changes.
- **AI civic timer** (`m_iAICivicTimer`): 25-turn re-evaluation throttle for the AI.
- **`verifyCivics`** (`CvPlayer.cpp:4080-4103`): called each doTurn. If a player's current civic
  became ineligible (tech-requisite revoked via conquest?) and they are not in anarchy, silently
  switches to the first eligible civic in the same option slot.

### 1i. Anarchy effects on gameplay

During anarchy (`isAnarchy()` true):
- Civic upkeep is zero (`getSingleCivicUpkeep` returns 0, `CvPlayer.cpp:14232-14234`).
- Commerce is recalculated on anarchy start/end (`setCommerceDirty`, `CvPlayer.cpp:9514`).
- Trade routes are updated (`updateTradeRoutes`).
- Corporation state is updated (`updateCorporation`).
- Work-assignment dirtied (`AI_makeAssignWorkDirty`).
- Religion conversion is blocked (`canConvert` checks `isAnarchy()`).
- Further revolutions are blocked (`canRevolution` checks `isAnarchy()`).
- UI anarchy message shown (start: "Revolution has begun"; end: "Revolution is over").

---

## 2. Current observability

**Tier: 1 (Telescreen).**

What is exposed today:

| Surface | What it shows | Where |
|---|---|---|
| `/players` | `era`, `score`, `gold`, `goldRate`, `scienceRate`, `techs`, `production` — broad empire stats | `CvHttpServer.cpp:286-303` |
| `/diagnostic/canDoCivics?type=CIVIC_X&player=N` | Legacy gate result (`canDoCivics`) + partial cascade verdict (wired, no `legacyReason`) | `CvHttpServer.cpp:844-853` |
| `[DAI/civic/best]` (level 2) | AI revolution commit: `curValue`, `bestValue`, the options switched | `CvPlayerAI.cpp:17290-17297` |
| `[DAI/civic/cand]` (level 3) | Per-candidate flavor contribution during civic valuation | `CvPlayerAI.cpp:13799` |
| Python `onCivicChanged` | Fires via `CvEventReporter::civicChanged` → Python only, NOT the SSE event spine | `CvPlayer.cpp:14384` |

What is NOT exposed (the full gap list is in §3 below).

The `/players` snapshot has **zero civic fields**. There is no endpoint that returns:
- Which civics a player is currently running.
- Whether the player is in anarchy, and if so for how many more turns.
- The revolution timer cooldown remaining.
- Civic upkeep cost.
- Any of the per-civic effect scalars (happiness, health, maintenance mods, etc.).

The `[DAI/civic/best]` log line fires only when an AI actually commits to a revolution —
it is silent on every turn the AI evaluates and decides NOT to switch, and the AI civic
evaluation timer (25 turns between evaluations) means even the decision event is coarse-grained.
There is no log for the current civic set, anarchy state, or the revolution timer.

---

## 3. The gap

**State that cannot be reconstructed from the HTTP layer + logs today:**

| Piece of state | Why it matters for the "Orwell" bar |
|---|---|
| **Active civic set** — which `CivicTypes` per `CivicOptionTypes` the player is running | Foundation: every downstream civic effect is keyed to this. Cannot reconstruct any effect without knowing the active civics. |
| **Anarchy state** — `isAnarchy()`, `getAnarchyTurns()` | Changes buildability (`canConstruct` deferred?), upkeep, and the `canRevolution` gate. AI players in anarchy are invisible. |
| **Revolution timer** — `getRevolutionTimer()` | The cooldown that blocks the next revolution. Needed to know when an AI COULD switch civics. |
| **AI civic evaluation timer** — `AI_getCivicTimer()` | When the AI will next re-evaluate civics. 25-turn blind window between evaluations. |
| **Civic upkeep cost** — `getCivicUpkeep()` | Major gold drain; part of the economy model that affects AI decisions. `/players` carries `goldRate` but its breakdown is opaque. |
| **Anarchy modifier** — `getAnarchyModifier()` | Scales both civic and religion anarchy lengths; invisible. |
| **Max/min anarchy turns** — `getMaxAnarchyTurns()` / `getMinAnarchyTurns()` | Determines whether any anarchy is possible at all (Spiritual = 0 = free switches). |
| **Civic switch decision inputs** — `iCurCivicsValue`, `iBestCivicsValue`, the near-future horizon calculation, the per-option breakdown | AI_doCivics' [DAI/civic/best] logs the commit but NOT the turns where it evaluated and stayed, or the value inputs when it chose not to switch. The near-future cost-benefit is fully silent. |
| **`verifyCivics` silent switches** — when a civic became ineligible and was auto-replaced | No log or event; the switch just happens. Could corrupt cascade baseline if undetected. |
| **Civic effects on the player** — the ~50+ scalars that `processCivics` applies | None are exposed. You can see the aggregate `goldRate` / `scienceRate` but not which fraction came from civics. |
| **`SpecialBuildingNotRequired` counts** — set/cleared by civics, drives the §14 B-iii group gate | Needed for cascade observability of building prerequisites. |
| **Religion anarchy** — `getConversionTimer()`, `getReligionAnarchyLength()` inputs | Conversion is a sibling mechanism to civic revolution; same blind-spot. |
| **Revolution-enabled flag** — `canRevolution()` truth per AI player | Cannot tell whether an AI can currently switch civics. |

---

## 4. Proposed hooks

All hooks follow the three canonical observability hook shapes — see [DEC-obs-hook-shapes](../../decisions.md#dec-obs-hook-shapes).

### Hook A — `/players` snapshot: add civic fields

In `CvHttpServer::publishIfDue` player-snap loop (`CvHttpServer.cpp:1524`), add to `PlayerSnap`
and render in `renderPlayers`:

```
"anarchy":        getAnarchyTurns()            // 0 = not in anarchy; >0 = turns remaining
"maxAnarchy":     getMaxAnarchyTurns()          // 0 = Spiritual / no-anarchy trait
"revTimer":       getRevolutionTimer()          // turns until next revolution allowed
"civicUpkeep":    getCivicUpkeep()              // total gold/turn drain from active civics
"civics": {                                     // per-option map of active civic type strings
  "CIVICOPTION_GOVERNMENT": "CIVIC_REPUBLIC",
  "CIVICOPTION_LEGAL":      "CIVIC_CODEOFLAWS",
  ... (one entry per option slot, using GC.getCivicInfo(getCivics(opt)).getType())
}
```

This alone climbs to **Tier 3** on the civic axis — snapshot of the full civic state every 5s.

### Hook B — `[CIV/turn]` per-player per-turn log line

A new `[CIV]` domain tag (or extend `[DAI]` with a sub-tag) emitted once per player turn at
level 1 from `AI_doTurnPre` (or from `revolution()` / `verifyCivics`), gated by
`gPlayerLogLevel >= 1`. Minimum payload:

```
[CIV/state] player=N anarchy=K revTimer=T aiCivicTimer=U
```

This gives the event spine a per-turn heartbeat on every AI player's civic state — no more
25-turn blind window.

### Hook C — `[CIV/switch]` on every civic change (including `verifyCivics` silent switches)

In `CvPlayer::setCivics` (`CvPlayer.cpp:14288`), after the old/new swap, emit at level 1:

```
[CIV/switch] player=N option=CIVICOPTION_X old=CIVIC_OLD new=CIVIC_NEW anarchy=K bForced=0|1
```

`bForced=1` for `verifyCivics` switches (pass a flag through). This covers ALL civic changes
including the currently-silent `verifyCivics` auto-replacement.

### Hook D — `[CIV/anarchy]` on anarchy start and end

In `CvPlayer::changeAnarchyTurns` (`CvPlayer.cpp:9503`), when `bOldAnarchy != isAnarchy()`:

```
[CIV/anarchy] player=N start=1|0 turns=K revTimer=T
```

Level 1. Makes anarchy transitions instantly visible in the event stream for all players.

### Hook E — `[DAI/civic/eval]` when AI evaluates and does NOT switch

In `AI_doCivics` (`CvPlayerAI.cpp:17303-17309`), in the `else` (no revolution) branch, emit at
level 2:

```
[DAI/civic/eval] player=N NOSTAY curValue=C bestValue=B threshold=T aiCivicTimer=U
```

Without this, every 25-turn evaluation that concludes "not worth it" is completely invisible.

### Hook F — `[DAI/civic/defer]` when AI defers due to near-future civic

In `AI_doCivics` near-future horizon block (`CvPlayerAI.cpp:~17136-17244`), emit at level 2 when
the near-future score exceeds the current best:

```
[DAI/civic/defer] player=N waiting=CIVIC_X turns=T nearFutureValue=NF bestValue=B
```

### Hook G — `/diagnostic/civics?player=N` endpoint

A new diagnostic endpoint (like `canDoCivics` but a full snapshot):

```json
{
  "player": N,
  "anarchy": K,
  "maxAnarchy": M,
  "revTimer": T,
  "conversionTimer": U,
  "aiCivicTimer": V,
  "civicUpkeep": W,
  "civics": [
    { "option": "CIVICOPTION_GOVERNMENT", "civic": "CIVIC_REPUBLIC",
      "canDo": true, "anarchyLength": 2 },
    ...
  ]
}
```

This is the on-demand point-in-time snapshot — the civic parallel of
`/diagnostic/placementSweep`. Wired to the game-thread mailbox (same pattern as the existing
diagnostics). Enables reconstruct-without-screen for every AI player's civic state.

---

## 5. Summary

**Current tier: 1 (Telescreen).** The `/players` snapshot carries no civic data at all. The
only civic observability today is a diagnostic `canDoCivics` check per-civic and a level-2 AI log
line when a revolution is committed. The active civic set, anarchy state, revolution/AI-eval
cooldowns, civic upkeep, and all per-civic effect scalars are fully opaque from outside.

**To reach Tier 3 (Big Brother) on the civic axis:** implement Hooks A + C + D (snapshot civic
set + emit switch events + emit anarchy transitions). This covers the human player and all AI
players within the 5s snapshot window, and puts all state changes in the event stream.

**To reach Tier 4 (Thought Police) on the civic axis:** additionally implement Hooks B + E + F +
G (per-turn heartbeat + AI-eval-no-switch log + near-future-defer log + diagnostic endpoint).
At that point every AI player's civic decision loop is fully narrated from the wire.

**Tier 5 items** (the full `processCivics` effect breakdown — the ~50 scalars per civic) would
require per-civic modifier snapshots on `/players` or a dedicated `/civic-effects` endpoint.
These are expensive to enumerate and likely not needed for the #428/#430 cascade shadow; they
are noted here for completeness but not prioritized.

---

*All `file:line` citations are against the working tree as of 2026-06-18. Confirm before acting:
`CvPlayer.cpp` line numbers drift with every edit to the file.*
