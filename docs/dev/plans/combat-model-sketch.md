# CvCombatModel — Engine API Sketch (for review)

## STATUS — Phase 3a landed (Layer 1 RoundModel)
`buildRoundModel()` (CvCombatModel.h/.cpp) is now the single per-round formula:
strength-ratio odds (with barb free-wins via `isNPC`), firepower-ratio damage,
combat limit, first-strike inputs. Consumers routed through it:
`CvUnit::getDefenderCombatValues` (a thin adapter — so resolveCombat, flanking,
and the AI's `AI_attackOddsAtPlotInternal` damage inputs all share it),
`getCombatOddsImpl`, `getCombatOddsSpecific`, and `computeCombatPreview`. The
divergences were reconciled to one behaviour (prediction == resolution): barb
free-wins now apply uniformly (incl. `getCombatOdds`, dropping the old
`isHominid`/`ACO_IgnoreBarbFreeWins` split), and the `resolveCombat` round-1
attacker hit-chance bug (~0.5% via a one-round lag) is fixed.
Phase 3b LANDED (PR #318; FinalRelease-playtested foundation, balance/calibration are
separate followups): the AI's *win-%* now comes
from the binomial engine (`getCombatOdds()/10`) for non-air attacks, replacing the
strength-ratio heuristic. The heuristic still runs for the predicted-HP contract the
stack sim depends on; the personality bias is preserved; air keeps the heuristic. The
~40 thresholds are recentred globally by `AI_finalOddsThreshold *23/20` (air-exempt).
See `combat-phase3b-plan.md` for the implemented detail + the GlobalDefine follow-up.

---

Draft of the unified combat engine so we can spot gaps before building Phase 1b.
Priorities (per user): **maintainability > outcome parity**; **game options pluggable
as rules**; **deterministic/OOS-safe** (synced `getSorenRandNum`, integer applied
damage). Not a final API — a sketch to poke holes in.

## Layered design

```
Layer 0  Strength primitives (stay on CvUnit; engine READS them)
         currCombatStr / currFirepower / maxCombatStr / combatLimit / first-strikes /
         armor/puncture/precision/dodge / withdraw/repel/knockback "VSOpponent" totals.
         NOTE: COMBAT_SIZE_MATTERS is ALREADY baked into currCombatStr (getSMStrength),
         so it is not a model-level rule — it enters via the strength inputs.

Layer 1  RoundModel  = buildRoundModel(att, def, plot, RuleSet)
         The single per-round formula. Pure, no mutation. Consolidates
         getDefenderCombatValues + the inline blocks in getCombatOdds/Specific.

Layer 2a predict(att, def, plot, RuleSet) -> OutcomeDistribution    [FLOAT ok]
         Statistical integration over rounds + first-strike distribution.
         Full HP/withdrawal/repel/knockback distribution (subsumes
         getCombatOddsSpecific). Used by UI (ACO), basic odds, and AI decisions.
         winProbPermille()/winProbPercent() = sum over this. NO state mutation.

Layer 2b resolveCombat (CvUnit) — restructured to consume RoundModel  [INTEGER + RNG]
         Per round: read RoundModel params, roll getSorenRandNum, apply integer
         damage + effects, mutate HP/XP/state. Deterministic. May change outcomes
         vs legacy (allowed).

Layer 3  Stack/decision layer (CvSelectionGroupAI::AI_attackOdds "goodness",
         AI_getBestGroupAttacker, collateral valuation) — consumes per-duel win%
         from Layer 2a. Explicitly NOT win-odds; never threshold-compared as such.
```

## Data structures (sketch; C++03)

```cpp
namespace combat {

struct StrengthInputs {            // Layer 0 snapshot, after rules
    int iAttStrength, iAttFirepower;
    int iDefStrength, iDefFirepower;
};

struct RoundModel {                // Layer 1 — everything one round needs
    int iAttackerHitChance;        // out of COMBAT_DIE_SIDES (precision/dodge, min 5)
    int iDefenderHitChance;
    int iDamageToAttacker;         // per defender hit (armor/modifiers applied), HP
    int iDamageToDefender;         // per attacker hit
    int iAttackerCombatLimit;      // max HP attacker can remove from defender
    int iAttFirstStrikes, iAttChanceFirstStrikes;
    int iDefFirstStrikes, iDefChanceFirstStrikes;
    bool bAttImmuneFS, bDefImmuneFS;
    int iAttWithdrawPct, iDefWithdrawPct;   // per-round, already vs-opponent adjusted
    int iAttKnockbackPct, iDefRepelPct;
    int iAttKnockbackRetries, iDefRepelRetries, iAttEarlyWithdraw, iDefEarlyWithdraw;
    // CombatDetails passthrough for combat log/animation (today via getDefenderCombatValues)
};

struct OutcomeDistribution {       // Layer 2a
    int iAttackerWinPermille;      // defeats defender or reaches combat limit
    int iDefenderWinPermille;
    int iAttackerWithdrawPermille, iDefenderWithdrawPermille, iRepelPermille, iKnockbackPermille;
    // optional HP-distribution arrays for ACO display
};

// Pluggable game-option rules. Assembled ONCE per game (options are fixed per game)
// and cached on CvGame; cheap to apply. First cut: each rule reads its option/#ifdef
// inside its own registration, not smeared across the math.
struct CombatRules {
    void adjustStrengthInputs(const CvUnit& a, const CvUnit& d, const CvPlot& p, StrengthInputs& io) const; // STRENGTH_IN_NUMBERS
    void adjustRoundModel(const CvUnit& a, const CvUnit& d, const CvPlot& p, RoundModel& io) const;          // future per-round tweaks
    void adjustOutcomeOdds(const CvUnit& a, const CvUnit& d, int& iAttOdds, int& iDefOdds) const;            // barb free-wins
    // resolution hooks (Layer 2b), applied during a real round:
    // onAttackerHit / onDefenderHit / onRoundEnd  -> afflictions, cold, stun, powershot,
    //   critical, distance-attack, collateral. (Phase 3; structured as hooks.)
};

RoundModel          buildRoundModel(const CvUnit& a, const CvUnit& d, const CvPlot& p);
OutcomeDistribution predict        (const CvUnit& a, const CvUnit& d, const CvPlot& p);
int                 winProbPermille(const CvUnit& a, const CvUnit& d, const CvPlot& p); // /1000
int                 winProbPercent (const CvUnit& a, const CvUnit& d, const CvPlot& p); // /100
}
```

## Consumers after unification
- `getCombatOdds` -> `combat::winProbPermille` (preview + Python `cyGetCombatOdds`).
- `getCombatOddsSpecific` -> reader over `combat::predict` (ACO).
- `CvUnitAI::AI_attackOddsAtPlotInternal` -> `combat::winProbPercent` (+ keep its
  predicted-HP side effects used by the stack sim; or move that into the engine).
- `CvUnit::getDefenderCombatValues` -> thin adapter filling out-params from RoundModel
  (keeps its resolution + flank callers).
- `CvUnit::resolveCombat` -> per-round params from RoundModel; rolls + applies.
- Stack `CvSelectionGroupAI::AI_attackOdds` -> per-duel win% from engine; goodness layer
  stays separate.

## Game-option -> where it plugs (the pluggable map)
| Option | Plug point |
|--------|-----------|
| `COMBAT_SIZE_MATTERS` | already inside `currCombatStr` (Layer 0) — enters via StrengthInputs; not a separate rule today |
| `COMBAT_STRENGTH_IN_NUMBERS` (+`STRENGTH_IN_NUMBERS` define) | `adjustStrengthInputs` (adds support strength/firepower) |
| barb free-wins (handicap) | `adjustOutcomeOdds` (clamp) — **standardize** (see gaps) |
| `COMBAT_HIDE_SEEK` | upstream of the duel (visibility / valid-defender selection), mostly outside this engine |
| ACO | display consumer of `predict`, not a rule |

## GAPS / things we may have missed (decide before building)
1. **Air combat is a separate path** (`resolveAirCombat` CvUnit.cpp:2030,
   `airCombatDamage` :14043, `airCombatLimit` :14000; `getCombatOdds` already branches
   on air via `airCurrCombatStr`). Decide: engine covers air too, or v1 = land/sea
   melee duel and air is a tracked follow-up.
2. **Resolution-only per-round effects** not in prediction: afflictions/poison, cold
   damage, stuns, power-shots, critical hits, distance attacks, and **collateral**
   (`collateralCombat` runs inside `resolveCombat`). Decide: model as pluggable
   resolution hooks now, or leave in `resolveCombat` for v1 and unify only the
   hit/damage/odds core.
3. **Barb free-wins inconsistency (real bug-ish divergence).** Applied by
   `getDefenderCombatValues` (CvUnit.cpp:25761/25766) and `getCombatOddsSpecific`
   (CvCombatModel.cpp:396/408) but NOT by the simple `getCombatOdds`. Unifying via one
   `adjustOutcomeOdds` rule will make the basic preview odds change (acceptable, but
   note it).
4. **Predicted-HP side effects.** `AI_attackOddsAtPlotInternal(modifyPredictedResults)`
   sets `AI_setPredictedHitPoints` that the stack sim chains on. The engine is pure;
   we must keep this side-effect path (either a separate stack-sim helper or an
   engine variant that returns expected HP).
5. **Determinism boundary.** Prediction may use float (pow/binomial). Resolution must
   stay integer in applied damage. The API splits these (2a float vs 2b integer) —
   confirm we never let a float-derived value drive applied HP.
6. **RuleSet lifetime.** Options are fixed per game → build the RuleSet once (cache on
   CvGame), not per combat. Confirm.
7. **CombatDetails / combat log** out-params (animation, flanking, defender breakdown)
   must still be produced — RoundModel carries them through.
8. **Bombard** (`bombard()` :8904) is its own thing (city/collateral damage, not a
   unit duel) — out of scope unless we say otherwise.
9. **`m_sorenRand.reseed(timeGetTime())`** (CvGame.cpp:8553) must stay off MP paths —
   don't let any new resolution code touch it.

## Findings from footprint exploration (resolves several gaps)

### Save format is tolerant — removals are low-risk for saves
Serialization uses name-tagged `WRAPPER_READ/WRITE` ("Class::field"). The loader reads
by name and is "tolerant provided their value was 0/-1/MIN_INT (likely defaults)"
(CvTaggedSaveFormatWrapper.cpp ~3448). So removing a serialized CvUnit field generally
does NOT break old saves (the tag is just absent → default). The earlier "save-breaking"
worry is largely moot. (Still: leave unrelated adjacent fields alone; the wrapper is
name-keyed not positional, so even that is safe.)

### The mechanics to remove are (mostly) game-option clusters
There are 15 `GAMEOPTION_COMBAT_*` options — effectively the natural "rules":
AMNESTY, BATTLEWORN, EQUIPMENT, FIGHT_OR_FLIGHT, HEART_OF_WAR, HIDE_SEEK,
NEW_RANDOM_SEED, OUTBREAKS_AND_AFFLICTIONS, REALISTIC_SIEGE, SIZE_MATTERS,
STRENGTH_IN_NUMBERS, SURROUND_DESTROY, UPRANGE, VANILLA_ENGINE, WITHOUT_WARNING.
- **Afflictions + critical hits** are gated by `COMBAT_OUTBREAKS_AND_AFFLICTIONS`
  (resolveCombat ~2769/2971, checkForCritical ~2708). Clean removal boundary: drop the
  option + its mechanics. `assignCritical()` feeds the affliction system — same cluster.
- **Cold damage, stuns, power-shots** are NOT behind a dedicated option (appear
  always-on / dead) → remove outright. Footprints: cold = surgical (3 fields);
  stuns = moderate (checkForStun + promotion/unitcombat arrays); power-shots = largest
  (5 modifier types). All are name-tagged in save (safe to drop).
- **KEEP `endurance`** (m_iExtraEndurance) — it's a damage-mitigation/heal mechanic
  serialized next to critical but is NOT in the removal set; must survive.
- `VANILLA_ENGINE` already switches resolveCombat to a vanilla path (bVanillaCombat
  ~2572) — the engine design should treat "vanilla vs TB-combat" as a rule too.

### Bombard / air / collateral share a damage primitive (per exploration)
- The damage core `iStrengthFactor=(fpA+fpD+1)/2` and `DAMAGE*(fp+factor)/(fp+factor)`
  is shared by **melee, air-v-air, and collateral**. → the engine should own ONE
  `computeRoundDamage` primitive these all call.
- **Melee + air-v-air** also share the odds roll → both consume the engine's odds.
- **City bombard** (`CvUnit::bombard` :8904) only damages city defense, no unit duel →
  stays a separate consumer (may reuse the primitive but no odds).
- **Air-v-ground interception** uses interception probability → separate.
- User's "bombard ties into air" = air-strike/bombing missions (air→unit damage) use the
  air path; ground bombard (city defense) is the separate one.

## Revised sequencing (supersedes the phase order in the plan)
Removing the dead/option mechanics FIRST strips per-round complexity out of
`resolveCombat`, making the resolution-unification far smaller. Proposed order:
- **R1** Remove cold damage (surgical) — warm-up, proves the save-tolerance assumption.
- **R2** Remove stuns; **R3** remove power-shots; **R4** remove afflictions + critical
  (the `COMBAT_OUTBREAKS_AND_AFFLICTIONS` cluster) — keep endurance.
- Then the unify phases (1b/2/3) over a much-simplified `resolveCombat`, with options as
  pluggable rules and one shared `computeRoundDamage` primitive (melee + air-v-air +
  collateral; city-bombard + interception as separate consumers).
```
