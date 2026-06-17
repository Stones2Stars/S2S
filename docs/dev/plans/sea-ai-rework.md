# Sea / naval AI — rework notes

Naval AI is a known weak area in this codebase and is slated for a broader rework.
This file captures the starting point, the bug that kicked it off, and the logging
now in place to drive a data-driven improvement (see
[`../reference/ai-logging-reference.md`](../reference/ai-logging-reference.md)).

## The trigger bug

Attack-sea **automation never left the player's borders** — automated attack ships
stayed in friendly waters instead of going out to engage the enemy.

**Two distinct dispatch paths — get this right before touching naval movement.**
A unit's per-turn move is chosen *differently* for AI vs. human automation
(`CvUnitAI::doUnitAIMove`, ~`263`):

- **AI players (and un-automated units):** dispatched by **UNITAI role** →
  `AI_attackSeaMove` for `UNITAI_ATTACK_SEA`. Its only proactive "engage" routine,
  `AI_seaAreaAttack`, used to filter candidate plots to
  `pLoopPlot->getOwner() == getOwner()` (only enemies that entered *our own* waters —
  the comment calls it an "incursion"). **Fix (PR #182):** dropped that filter so it
  pursues any *visible* enemy ship in the sea area. Peace-safe — the `generatePath()`
  below still refuses to path into would-be-war territory, so no auto-DoW.
- **Human automation (the actual bug report):** dispatched by **`getAutomateType()`**
  (`switch` at `doUnitAIMove` ~`265`), *not* by UNITAI. "Attack" naval automation is
  `AUTOMATE_HUNT` → `CvUnitAI::AI_SearchAndDestroyMove` → **`CvHunterAI::autoHuntMove`**
  (generic combat units a player toggled to Hunt; `UNITAI_HUNTER` units go to
  `hunterMove` instead). **So PR #182 did not touch this path at all** — which is why
  ships were still stuck after it.

Why `autoHuntMove` kept ships home: it engages only within `AI_huntRange(1/3/6)` (~6
tiles), and with no target in range it fell straight to `AI_moveToBorders()`, which
*by design* only moves the unit **within its own territory** (bails if not on an owned
plot; only steps to plots `getOwner() == getOwner()`), then `AI_patrol()`.

**Fix (issue #187):** in `autoHuntMove`, for `DOMAIN_SEA` units, *before* the
`AI_moveToBorders` fallback, take the fight out:
`AI_seaAreaAttack()` (chase visible enemy ships, relaxed by #182) →
`AI_blockade()` (sail to an enemy coast) →
**`CvHunterAI::seaExplore()`** (a naval explorer, below) → `AI_explore()` as a last
fallback. The `AI_*` helpers are reached from `CvHunterAI` via `friend class
CvHunterAI`. Peace-safe — pathing still refuses would-be-war territory, so automation
never auto-declares war.

### `seaExplore` — naval exploration that drives into the dark
`AI_explore` is a *land* explorer: it adds large `+adjacentToLand` / `+owned` score, so
ships hug the coast and home waters instead of open ocean (the "doesn't explore dark
areas / all take the same path" symptom). `CvHunterAI::seaExplore` instead:
- scores frontier water plots (adjacent to unrevealed map) and **prefers open water**
  (a bonus for *not* being adjacent to land), so ships head into the dark;
- **spreads the fleet** via the per-player claim ledger (`tryClaim`/`isClaimedByOther`)
  so ships don't trail one another down the same coastline;
- uses a **movement-derived range** (`baseMoves()` tiles) and a cheap `rect()` scan —
  deliberately *not* a `CvReachablePlotSet` (a movement flood-fill), which made turn
  times explode when every ship re-ran it on every move step (same-water-area ⇒
  reachable by sea, so the flood-fill was unnecessary);
- has **hysteresis**: it commits to its current `MISSIONAI_EXPLORE` target and keeps
  heading there while it is still a frontier, only re-scanning (with random jitter) for
  a new heading once the target is reached or revealed. Without this, the per-call
  jitter made ships dither — one ship logged 5,382 invocations across only 63 plots.

  > **Hysteresis**, in plain terms, means the system's output depends on its recent
  > *history*, not just the current input — it resists flip-flopping. Here it means a
  > ship **sticks with the heading it already chose** instead of re-deciding from
  > scratch every step. (Everyday example: a thermostat set to 20°C doesn't switch the
  > heating on and off the instant the temperature wobbles by 0.1° — it waits for a
  > margin before changing, so it doesn't rapidly cycle. Same idea: commit, then only
  > change when there's a real reason.)

  Log reasons: `seaExplore` (picked a new target) / `seaExploreKeep` (committed heading).

### `detectSpin` — turn-hang safety
A hunter/explore routine that pushes a mission which never advances the unit leaves it
`readyToMove`, so the AI re-invokes it at the same plot until the engine's `iTempHack>50`
backstop fires — ~50 expensive re-decides per stuck unit per turn (observed on AI
`UNITAI_HUNTER` units in `hunterMove` → `AI_refreshExploreRange`; `detectSpin` fired
~739× over 50 turns). `CvHunterAI::detectSpin` (called at the top of `hunterMove` and
`autoHuntMove`) counts consecutive same-plot re-decides — reset the moment the unit
moves — and after 8 ends the unit's turn (`finishMoves` + `MISSION_SKIP`, logged
`[HAI/spin]`). This **bounds** the spin to 8; the root cause in `AI_refreshExploreRange`
(why it pushes a no-progress move) is **tracked as issue #189** — a future fix.

## The cascade to understand for the rework

`AI_attackSeaMove` (CvUnitAI.cpp, ~`6983`) is **defensive / escort-first**. Rough
priority order:

```
anyAttack(1/2) → seaBombardRange → heal → break-blockade (only if our city blockaded)
→ group(carrier/attack) → [in own/neutral water] shadow assault/carrier + group assault
→ blockade (ONLY if already on an enemy plot) → pillageRange(4) → protect(3,8)
→ travelToUpgradeCity → seaAreaAttack → patrol → retreatToCity → safety → SKIP
```

The offensive options (`seaAreaAttack`, `blockade`, `pillageRange`) sit **late** and
several are range- or territory-bounded, while escort/protect/group sit early — so
even after the `AI_seaAreaAttack` relaxation, a ship with nearby friendly assets may
still prefer staying home over sailing out. Re-ordering / re-weighting that cascade
is the substance of the rework.

## Logging in place (use this before changing behaviour)

Per [`codebase-bug-hunt.md`](codebase-bug-hunt.md) and the project's
"log-then-audit" rule, the attack-sea cascade is now fully instrumented. Each turn a
unit emits one `[UNT/act]` line naming the helper that won, via
`CvUnitAI::AI_logAct(decision, reason, target)` → `logUnitAI(2, "[UNT/act] …")`
(`BetterBTSAI.cpp`). The sea-specific helpers added in PR #182:

| helper | `decision` | `reason`s |
|---|---|---|
| `AI_seaAreaAttack` | `seaAreaAttack` | `pursueEnemyInArea`, `ambush` |
| `AI_blockade` | `blockade` | `blockadeHere`, `moveToBlockade` |
| `AI_seaBombardRange` | `seaBombard` | `bombardOrPlunder`, `moveToBombard` |
| `AI_shadow` | `shadow` | `shadowHere`, `moveToShadow` |

The generic helpers in the cascade (`anyAttack`, `patrol`, `safety`, `pillageRange`,
`protect`, `group`, `heal`) were already instrumented. To study naval behaviour,
filter `UnitAI.log` for `[UNT/act]` lines on `type=` of the sea UNITAI roles and read
them alongside `[UNT/move]`.

## Plan

1. Gather real `[UNT/act]` traces from games (especially at war, large navies).
2. Identify where automated/AI attack ships stall (escort/protect/group winning over
   offence; range caps; pathing refusals).
3. Re-order / re-weight `AI_attackSeaMove` (and siblings: `AI_reserveSeaMove`,
   assault/escort) with data behind each change. Keep each step Assert-built and
   playtested — naval AI changes affect every player.
