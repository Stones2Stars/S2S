# Size Matters AI literacy (#395)

**Problem (owner, 2026-06-12).** Under `GAMEOPTION_COMBAT_SIZE_MATTERS` the AI "does a lot
of calculations unaware that it exists": force-sufficiency gates count raw bodies, unit-mix
valuation applies a blanket bonus to mergeable types, and merge/split behaviour is
opportunistic rather than purposeful. Census evidence (t1340): 9,176 units world-wide, 2,734
`UNITAI_CITY_DEFENSE` — the garrison cluster is ~65% of all units, and surplus defenders are
also the measured CITY_DEFENSE turn-time plateau (`turn-time-optimization.md`).

## Mechanics ground truth (verified 2026-06-12)

- Merge: 3 identical units (same UnitType, groupRank, qualityRank — and same UNITAI on the
  AI auto path) on one plot → 1 unit at groupRank+1. Strength scales **×1.5 per rank**
  (`SIZE_MATTERS_MOST_MULTIPLIER=150`, `applySMRank`), volume ×3
  (`SIZE_MATTERS_MOST_VOLUMETRIC_MULTIPLIER=300`), upkeep multiplier ×1.5 per rank above
  base (`calcUpkeepMultiplierSM`). **A 3→1 merge halves aggregate strength** while
  concentrating per-battle punch and halving upkeep; split is the exact inverse. Merging is
  a tactical tradeoff, not free consolidation.
- The AI **does** merge: `doUnitAIMove` runs `CvSelectionGroup::doMergeCheck()` before every
  UNITAI routine (`CvUnitAI.cpp:493`). But `canMerge(bAutocheck=true)` requires **four**
  identical co-plotted same-UNITAI units — the old C2C "gradient" design (TBSPLIT comment,
  `CvUnit.cpp:26825`): merge 3, keep a 4th so counts stay flat. (The 4th-unit split order
  the comment promises does not exist in code; in practice it is just a conservative gate.)
- The AI also **splits**: city defenders split when `iEnemyOffense > iOurDefense / 4`
  (`CvUnitAI.cpp:4990` — a hair-trigger; any enemy at 25% of garrison strength shatters
  merged defenders), and groups >20 units split. `setInhibitMerge(false)` is cleared in the
  same site's safe branch.
- Bookkeeping that already exists: `CvPlayer::changeUnitCountSM` tracks per-unit-TYPE
  volumetric counts (`3^(groupRank-1)`, maintained by `changeExtraGroup`); per-UNITAI
  body counts live in `CvPlayerAI::m_aiNumAIUnits` + `CvArea` per-player counters
  (serialized; full recount in `AI_recalculateUnitCounts`).
- Count-blind surfaces (the gap): production/demand gates (`AI_totalUnitAIs`,
  `AI_totalAreaUnitAIs`, `AI_getNumAIUnits` consumers in `AI_chooseProduction`),
  `AI_getTotalFloatingDefenders` (six raw count sums), `AI_neededDefenders` /
  `AI_minDefenders` floors, `AI_neededHunters` / `AI_neededExplorers`,
  `AI_plotTargetMissionAIs` (cargo-volume variant exists but the count variant dominates),
  healer demand, and `EVAL_MERGE_FACTOR=2` blanket-doubling mergeable types' training value
  in ~5 UNITAI cases of `AI_unitValue`.

## Owner rulings (2026-06-12) — design law for this work

1. **Force accounting is strength-weighted: a rank+1 unit counts as ×1.5 bodies, not ×3.**
   Rationale: "the AI could be tricked to not have sufficient force if we tell it that a
   merged unit is the same power as 3 normal units, because in many cases it is not."
   Never over-credit merged force. (Effective bodies = `applySMRank(100, extraGroupRank,
   SIZE_MATTERS_MOST_MULTIPLIER)`, i.e. ×1.5 per rank above the unit type's base group
   rank, ÷1.5 below. Group-change promotions count — they scale strength identically.
   Quality/size ranks are out of scope: they exist without merging, and the pre-SM gate
   constants never weighted promotions either.)
2. **Garrisons consolidate — the count-neutral TBSPLIT "gradient" design is overridden for
   city garrisons.** A deliberate city-level pass merges surplus same-type primary-defender
   triples (3 needed, no 4th-unit rule) while the city keeps a strength surplus beyond the
   release margin. Field/attack stacks keep the conservative 4-unit auto-merge for now.
   The ¼-threshold auto-split is retuned to a real "overwhelmed" test.
3. **Hunters keep merging; `AI_neededHunters` becomes capacity-based** (strength-weighted
   per ruling 1, not raw bodies). Merged hunters punching up vs strong animals IS the
   intended counter to high-strength wildlife; the coverage loss is accepted.
4. **Attack stacks must recognize "merge to beat the defender".** When a city-attack
   stack's odds vs the city's best defender fail the attack gate, evaluate the hypothetical
   rank+1 merged attacker (strength ×1.5); if that clears the gate, merge the triple and
   proceed — instead of stalling/feeding singles into a defender that grinds them 1v1.
5. **Healer demand is SM-aware.** Merged units have more HP and therefore heal slower in
   absolute terms, so healer support for (attack) stacks scales with merged HP-mass, not
   body count — fewer-bigger armies need MORE healer capacity per body, not less.

## Plan

**Status (2026-06-12): Phases A–E implemented on `feature/395-size-matters-ai`
(Assert-green), awaiting playtest verification against the benchmark census.**

- **Phase A — effective-count infrastructure.** `CvUnit::SMeffectiveCountTimes100()`
  (100 at type base rank; ×1.5/rank; 100 always when SM off) + transient per-UNITAI
  effective ledgers on `CvPlayerAI` and `CvArea`, mirrored at the exact `m_aiNumAIUnits`
  bookkeeping sites + `changeExtraGroup`, recounted in `AI_recalculateUnitCounts` (and on
  load — the ledgers are NOT serialized). Gated `[UNT/sm]` logging for raw-vs-effective
  divergence at the flipped gates.
- **Phase B — flip the classified consumers.** Demand/production gates and floating-defender
  supply read effective counts (floor when rounding — conservative per ruling 1).
  Redundancy floors (`AI_minDefenders`, `AI_guardCityMinDefender`) stay RAW body count: a
  merged unit still soaks one attack at a time; floors are small and "overdefended beats
  underdefended" (#384). Coverage demand (`AI_neededExplorers` comparisons) stays raw.
  Hunters: capacity-based per ruling 3.
- **Phase C — garrison consolidation pass** (ruling 2) + split-trigger retune. City-level,
  in the #384 garrison machinery: while garrison strength minus the would-be merge loss
  stays ≥ `AI_neededDefenseStrength × GARRISON_RELEASE_MARGIN_PERCENT`, merge surplus
  primary-defender triples; hysteresis vs the (retuned) overwhelmed-split.
- **Phase D — attack-side need-merge** (ruling 4): `CvUnitAI::AI_smMergeToBreachCity`,
  called from `AI_attackCityMove` when adjacent to the target with group odds ≤ 75. Merges
  a triple only when it flips the strength duel against the city's best defender (single
  ≤ defender < single×1.5); the actual attack stays odds-gated next evaluation. Logs
  `[UNT/merge2breach]`.
- **Phase E — mix sanity.** `EVAL_MERGE_FACTOR` retired (honest accounting replaces the
  thumb on the scale; `unit-ai-valuation.md` §B1 updated); healer demand scales with stack
  HP-mass (ruling 5): one healer per started 10 base-body-equivalents, capped at 3, in the
  `AI_attackCityMove` broker request (HP scales by the same ×1.5/rank as strength —
  `CvUnit::setSMHPValue` — so the effective-count weight IS the HP-mass weight).

Verification: AI-vs-human benchmark census (`/units` + `players_timeseries`) before/after —
expected: CITY_DEFENSE body count falls toward ~1/3 in consolidated cities, upkeep drops,
`[PERF/unitai]` CITY_DEFENSE n falls, no city-loss regression; `[UNT/merge]`/`[UNT/split]`
lines audit every action (`ai-logging-reference.md`).

Related: #384 (garrison tiers), #400 (sortie odds floor), `unit-ai-valuation.md` §A5/§B,
`turn-time-optimization.md` (defender economy), `ai-vs-human-benchmarking.md`.
