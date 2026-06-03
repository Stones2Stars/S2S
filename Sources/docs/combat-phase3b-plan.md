# Phase 3b — Route the AI's win-% through the binomial engine

Status: **planned / not started.** Phase 3a (the shared `buildRoundModel` Layer 1)
is done — resolution, the odds functions, the preview, and the AI's *damage inputs*
already share one formula. 3b is the deliberate, behaviour-changing follow-up:
replace the AI's *win-probability* heuristic with the engine's binomial
`getCombatOdds`, so the number the AI decides on is the same number the UI shows
and the resolver actually rolls.

This is deferred on purpose. It is not a refactor — it shifts AI aggression, and
the safety is in the rollout (flag + instrument + calibrate), not in the diff.

## What changes, precisely

Today `CvUnitAI::AI_attackOddsAtPlotInternal` (CvUnitAI.cpp ~1167–1353) does NOT
call `getCombatOdds`. It:
1. Gets attacker `currCombatStr`/`currFirepower` (air uses `airCurrCombatStr`).
2. Calls `getDefenderCombatValues` (now a `buildRoundModel` adapter) for per-round
   damage + odds.
3. Estimates first-strike HP loss, sets `AI_setPredictedHitPoints`, re-reads HP.
4. Computes `iNeededRoundsUs`/`iNeededRoundsThem`, **boosts the loser's effective
   strength by the round-count difference**, then returns
   `iOdds = iOurStrength*100 / (iOurStrength + iTheirStrength)` (a strength ratio,
   1–99), and sets predicted HP on both units.
5. Applies the per-leader personality bias:
   `iOdds += (iOdds * 2 * AI_getAttackOddsChange()) / 100`.

The strength-ratio + round-count-boost in step 4 is a *cheap approximation* of the
win probability. 3b replaces ONLY that win-% number with the engine's binomial:

```
iOdds = getCombatOdds(this, pDefender) / 10;   // permille -> percent
// keep the personality bias line unchanged
```

Everything else in the function — the predicted-HP machinery — stays. See below.

## Sub-problems (each must be handled, none are optional)

### 1. Predicted-HP side effects must survive
`AI_setPredictedHitPoints` / `getHP()` reading it (CvUnit.cpp:11326) is load-bearing:
`CvSelectionGroupAI::AI_attackOdds` chains it across the stack sim, and `CvPlot`
checks `== 0` for "going to die". The engine (`getCombatOdds`) is pure — no HP
side effects.

**Approach (minimal, recommended):** keep the existing round-count expected-HP
estimate (`HP - neededRounds*damagePerHit`) purely for the predicted-HP writes,
and use `getCombatOdds` only for the returned odds. The strength-boost-for-odds
becomes dead and is removed; the needed-rounds / expected-remaining-HP code stays.
This isolates the behaviour change to the odds number and leaves the stack sim's
HP bookkeeping byte-identical.

**Alternative (heavier):** expose a lean engine call returning win% *and* expected
attacker/defender remaining HP (the data already exists in `computeCombatPreview`:
`fExpHPAttackerWin/PullOut/Retreat`, `fExpHPDefenderWin`). Only worth it if we
later want the predicted HP to match the engine's distribution too. Not needed
for v1 of 3b.

### 2. Personality bias stays
`AI_getAttackOddsChange()` (set per turn from the leader, CvPlayerAI.cpp ~16902)
multiplicatively biases the odds before every threshold compare. Keep the bias line
exactly, applied to the engine odds. Without it all leaders' aggression collapses.

### 3. Air combat has no engine path
`getCombatOdds` uses `currCombatStr(NULL,NULL)`; it does not branch on air or use
`airCurrCombatStr`/interception. For DOMAIN_AIR attackers, **keep the existing
heuristic** (gate: `if (air) <old path> else iOdds = getCombatOdds(...)/10`).
Unifying air odds is a separate task (see combat-model-sketch GAP #1).

### 4. Threshold recalibration — THE risk, and the real work
~40 AI thresholds (AI_anyAttack/AI_cityAttack/… 60/70/80/etc.) are tuned to the
*current* strength-ratio distribution and then inflated by `AI_finalOddsThreshold`
(CvUnitAI.cpp ~25406: ×6/5 base, ×6/7 city, ×6/9 attack, ×2/3 single-defender,
×3/2 cargo). The binomial win% has a **different shape** than the strength ratio —
near 50/50 they roughly agree, but for lopsided matchups they diverge sharply
(binomial pushes toward 0/100 faster). Swapping the source without recalibration
will silently change how aggressive the AI is at every threshold.

**Methodology (do NOT skip):**
- a. **Instrument first.** Before changing the return value, log BOTH numbers
  (`iOdds_heuristic`, `getCombatOdds/10`) for every AI attack evaluation, with the
  matchup, to a dedicated log. Run several AI-vs-AI autoplay games. This gives the
  empirical heuristic→binomial mapping across the real distribution of matchups the
  AI actually faces (not synthetic).
- b. **Decide the calibration target.** Either (i) remap each threshold through the
  measured mapping so decisions are unchanged at first, then loosen toward the more
  accurate binomial deliberately; or (ii) adjust the `AI_finalOddsThreshold`
  multipliers globally so the *aggregate* aggression matches a baseline autoplay.
  (i) is safer (per-threshold), (ii) is less code but blunter.
- c. **Hunter raw-odds path** (`CvHunterAI` + `AI_huntRange` `bRawOdds`,
  CvUnitAI.cpp ~27141) bypasses `AI_finalOddsThreshold` — recheck its thresholds
  separately; the binomial fixed the very-weak-defender underflow that originally
  motivated the raw-odds path, so it may simplify.

### 5. Stack goodness inherits the change
`CvSelectionGroupAI::AI_attackOdds` (the loss-ratio "goodness" score, NOT a win%)
consumes per-unit odds + predicted HP. It needs no structural change, but its
output shifts because its inputs shift. Include it in the autoplay comparison.

### 6. Performance
`getCombatOdds` (nested binomial + `pow`) is heavier than a strength ratio, and the
AI evaluates many candidate attacks per turn. The per-player `AI_attackOddsAtPlot`
cache (keyed on plot+attacker+defender) already absorbs repeats, but
`modifyPredictedResults=true` calls bypass the cache. Profile an autoplay turn
before/after; if hot, memoise `getCombatOdds` per (attacker,defender,HP) within a
turn, or fall back to the heuristic for clearly-decided matchups (odds the
strength ratio already puts >95% or <5%).

## Step sequence

1. **Instrument** (no behaviour change): log heuristic vs binomial odds side by side
   in `AI_attackOddsAtPlotInternal`. Run autoplay, collect the mapping. *(buildable,
   shippable, reversible.)*
2. **Swap behind a flag**: replace step-4 odds with `getCombatOdds/10` (air gated to
   the old path), keep predicted-HP code, keep the bias. Put it behind a temporary
   `MODDERGAMEOPTION_*` / define so it can be A/B'd against the heuristic.
3. **Calibrate thresholds** per §4b using the §1 data; verify hunter + stack paths.
4. **Profile** (§6); add memoisation only if the autoplay turn time regresses.
5. **Bake in**: once autoplay aggression + win-rate are acceptable, remove the flag
   and the dead strength-ratio-for-odds code. Update combat-model-sketch status.

## Acceptance criteria
- AI-vs-AI autoplay over N turns shows aggression and unit-loss rates within an
  agreed band of the pre-3b baseline (or deliberately, justifiably different).
- No measurable per-turn time regression (or mitigated by §6).
- The number the AI decided on == `getCombatOdds` == the UI preview == what the
  resolver rolls (the whole point: one combat number, end to end).

## Open questions to settle before starting
- Calibration target: preserve current aggression exactly (remap), or accept the
  more-accurate binomial and re-tune by feel? (Affects §4b choice.)
- Is a permanent A/B option worth keeping, or flag-then-remove?
- Air: keep on the heuristic indefinitely, or schedule the air-odds engine path?

## Status (implemented)
Hybrid chosen (swap + recenter). Step 1 (instrument, `fe7ca1ff`) and Step 2
(swap + recenter, `b9e40e3b`) done on branch `combat-ai-binomial-odds`. Non-air AI
attacks now decide on `getCombatOdds()/10`; `AI_finalOddsThreshold` raises the bar
+15% (`23/20`, air-exempt) to recenter. Measured on 5.7k autoplay evals: heur 60% ==
binom 80%, heur 70% == binom 96%; post-swap odds are healthily bimodal (~56% clear
loss, ~31% clear win, ~7% judgment calls — the recenter only bites on that 7%).
Validated by feel ("good enough for now"). The `23/20` constant + the `[COC]`
calibration log are scaffolding to be removed at bake-in once the per-player work below
settles the final shape.

## Follow-up: per-player / per-trait configurable aggression (FUTURE TASK)
Make the attack-odds bar configurable and per-player so aggressiveness varies by
civ/leader — barbarians (NPC) more reckless (lower bar), pacifist/defensive civs more
cautious (higher bar) — instead of one global `23/20` constant.

Where it plugs in: `CvUnitAI::AI_finalOddsThreshold` (CvUnitAI.cpp ~25432) is the single
chokepoint; the recenter line lives at its tail. The only existing per-leader lever is
`AI_getAttackOddsChange()` (CvPlayerAI.cpp ~16902), a multiplicative bias applied to the
*odds* in `AI_attackOddsAtPlotInternal` — distinct from the *threshold* bar, but the
prior art to follow.

Sketch (smallest first):
1. **Data-drive the global constant.** Move `23/20` out of code into a GlobalDefine
   (`Assets/XML/GlobalDefines*`, e.g. `AI_BINOMIAL_ODDS_THRESHOLD_PERCENT = 115`), read
   via `GC.getDefineINT(...)`. Tunable without recompiling; cache it like other hot
   defines if `AI_finalOddsThreshold` (PROFILE_FUNC) shows up.
2. **Per-player modifier.** Add a player/leader aggression term combined multiplicatively
   in `AI_finalOddsThreshold`: `bar = bar * GLOBAL% / 100 * playerAggr% / 100`. Source it
   from a trait/leader field (new `iAttackOddsThresholdModifier` on `CvTraitInfo` /
   `CvLeaderHeadInfo`, or a `FLAVOR_MILITARY`→modifier map). Barbarians: gate on
   `GET_PLAYER(getOwner()).isNPC()` (already used for combat free-wins) for a built-in
   recklessness bump even without a trait.
3. **Keep air exempt** (still heuristic) and the hunter raw-odds path bypassing this
   (CvUnitAI.cpp ~27167) until those get engine odds too.
4. Re-run the `[COC]` autoplay comparison per archetype (aggressive vs pacifist leader,
   barb) to sanity-check the spread before baking in.

This subsumes the "is a permanent A/B option worth keeping" open question: the global
define + per-player modifier *is* the permanent, data-driven knob, replacing the
temporary `23/20` literal.
