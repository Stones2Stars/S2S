# F4 — the unit-plane modifier machine (the unit-scope self-accumulator)

> **Status:** build doc (owner-approved design 2026-07). The realization of [roadmap §F4](roadmap.md) — the last
> unbuilt modifier scope. Brings the unit stat plane onto the ONE modifier machine
> ([DEC-universal-yield](../../architecture/decisions.md#dec-universal-yield)); today it is the biggest surface
> still on legacy per-unit accumulators.

## 1. The situation — data built, consumer absent

The **data side is complete**; the **self-accumulator consumer does not exist**. This is a greenfield build, not an
unpick:

- `CvPromotionInfo` / `CvUnitCombatInfo` / `CvUnitInfo` each carry the address-keyed `CvJsonModifiers m_modifiers`
  (the deposit form) **and** the materialized `get*Change()` scalar mirrors. `DepositIndex::pushInfo` already
  compiles their deposits at readJson — **the push side needs zero extension**.
- The legacy apply-loops read the materialized scalars **live today**: `CvUnit::processPromotion`
  (`CvUnit.cpp:18713`), `CvUnit::processUnitCombat` (`:18283`), `CvUnit::processLoadedSpecialUnit` (`:28851`), each
  folding `kX.getYChange() * iChange` into ~91 `changeExtra*` sinks (`grep '^void CvUnit::changeExtra'` = exactly
  91, span `:11386…:30937`).
- `Sources/Cascade/CvCascadeScopePackages.h` defines `CascadeCityPackages` / `CascadePlayerScope` /
  `CascadeWorldScope` — **no unit scope**. That absence IS F4.

The full channel inventory (24 groups, adversarially verified) lives in
[code-cut-map.md](code-cut-map.md) (§ the unit-plane BLOCKED inventory) — this doc does not duplicate it; it states
the **design** the build executes against it.

## 2. The approved design (owner 2026-07)

**A unit-scope package (`CascadeUnitPackages`) on `CvUnit`, backed by `CvDerivedCache` — the same
recompute-only / dirty-flagged / never-serialized model as the plot-yield and city caches
([state-repositories.md](../../architecture/state-repositories.md)).** One cache philosophy across every scope.

- **Sources** = the unit's small held-set that the legacy delta accumulator was fed from: **held promotions +
  held unit-combat classes** — the LIVE set enumerated via `isHasUnitCombat` (which captures the intrinsic primary +
  subs AND promotion-granted combat classes; the exact set the legacy `processUnitCombat` fired on — no double-count
  with the promotion loop, legacy folded both). Bounded per unit (a handful), unlike a city's many sources.
  ⛔ **The gather does NOT include the unit TYPE's base scalar** (`CvUnitInfo` base values). A base stat like
  withdrawal/firstStrike is authored as a `<channel>.unit.*` family AND materialized into the info base scalar, and
  the consumer already adds that base separately (`m_pUnitInfo->getX() + getExtra*()`) — so folding the type into the
  cache would DOUBLE-COUNT it (this bit the first cut; caught + fixed 2026-07-19). The cache reproduces the legacy
  `m_iExtra*` DELTA (promotions + unit-combats) exactly, and the unit-type base stays with the consumer's `base +
  extra` composite. Folding the type base INTO the machine (whole-value ownership,
  [DEC-universal-yield](../../architecture/decisions.md#dec-universal-yield)) is a deferred proper-once redesign —
  it means repointing the composite `*Probability()` getter itself onto the machine and giving the AI weighting
  consumers a separate delta query — NOT part of flipping the legacy delta getters. ⚠ The legacy `processLoadedSpecialUnit` (`CvUnit.cpp:28875`) also folded
  a `CvSpecialUnitInfo` withdrawal change, but that is a **DEAD source in current data** (verified live 2026-07-19):
  SPECIALUNIT is not deposit-ported (no readJson type-prefix, never pushed to `DepositIndex`, and `GC.getSpecialUnitInfo`
  returns a legacy `CvInfoBase`, not a deposit-carrying `CvInfo`), and zero special-unit JSON authors withdrawal — so
  the legacy fold summed a perpetual zero and the cascade gather correctly EXCLUDES it (the fold SITE still migrates to
  `markDirty` for uniformity/future-safety). *Open verify:* confirm no legacy specialunit XML authored `iWithdrawalChange`
  — genuinely-dead vs a curator gap ([DEC-data-first](../../architecture/decisions.md#dec-data-first)); exotic, an F7
  tail item if real.
- **Fold shape — GATHER-ON-DIRTY (option A, approved; NOT incremental push).** A promotion / unitcombat / type
  change flips the unit's cache **dirty**; the next read gathers `Σ` over the held-set via `MMKernel`
  (`sumUnit100` / `sumKeyed4U` / `perScale` / `modifiedInt`), gated by `cascadeEvalCondition`, combined by the §2
  formula `(base+Σflat)×(100+Σpercent)/100`. Reads are O(1) when clean. Rejected: incremental push-on-gain
  (`modifier.md §6`'s "O(1) concatenation as added") — it reintroduces the maintained running-total push-accumulator
  [state-repositories.md](../../architecture/state-repositories.md) is retiring, for no gain (the held-set is small
  enough that gather-on-dirty is effectively O(1)).
- **Never serialized** → the accumulated totals **re-derive from held promotions/unitcombats at load**
  ([DEC-derived-never-trusted](../../architecture/decisions.md#dec-derived-never-trusted)); dirty-on-construct means
  the first read after load recomputes. The serialized `m_iExtra*` block (`CvUnit.cpp` WRAPPER_READ ~`:19445` /
  WRITE ~`:20456`) is removed by the **standard soft-remove — NOT a save-break**
  ([DEC-save-remove-is-soft](../../architecture/decisions.md#dec-save-remove-is-soft)): full-delete the member +
  read + write and NAME the drained tags in `Assets/savemigration.txt`; the reader drains old-save orphan bytes
  transparently, so old saves load clean forever with no version bump. The only divergence from the city plane is
  that this stack WAS serialized; the removal itself is the same drain-text every accumulator cut uses.

**Reused wholesale (single-implementation law — never reimplement, [patterns.md](../../architecture/patterns.md)):**
`MMKernel` (all leaf primitives are channel-agnostic), `DepositIndex` (`depositsFor(j)` already returns each
promotion/unitcombat's compiled deposits), `cascadeEvalCondition` (the one gate; only the eval **context** needs a
unit-scope fact surface — confirm `CvCascadeEvalCtx` accepts a unit when wiring), and the `CvDerivedCache`
component. **New** = the unit scope struct + the `CvUnit` cache binding + the gather-over-held-set walk + the
load-time re-derive path.

## 3. Build order (incremental, each verified live in-game against the legacy consumer it replaces)

1. ✅ **LANDED (2026-07-19, live-verified) — withdrawal + firstStrike + heal.** The scope struct
   (`CascadeUnitPackages` on `CvUnit`, `CvDerivedCacheSet`), the `CascadeAccumulator::refreshUnitPackages`
   gather-on-dirty over the held-set, and the full vertical slice for these three groups (8 members: getters flipped
   to cache reads with the commander folds/clamps preserved, apply-loop folds retired to one `markDirty(UPK_ALL)`
   per loop, members + changers + serialization deleted, 8 tags drained in `savemigration.txt`, the pillage
   transient on a suppress flag) are DONE and Release-build clean. **Live-verified** on the loaded ~1338-era save:
   `/computed/units/heal` shows the cascade-gathered `sameTile` terms matching the held promotions to the number
   (18→18, 13→13 across sampled healer/recipient pairs, zero stuck-zeros); withdrawal/firstStrike ride the identical
   gather (address-only difference). ⚠ The first cut ALSO folded the unit-TYPE base into the gather, DOUBLE-COUNTING
   any type-authored withdrawal/firstStrike base (the consumer adds it separately) — caught + fixed 2026-07-19: the
   gather is promotions + unit-combats ONLY (see §2). Heal was unaffected (no type authors territory heal, so its
   type fold was 0). ✅ **evasion / intercept / collateral(damage) / capture ALSO LANDED (2026-07-19)** on the
   corrected delta-only pattern (Assert-build clean): 5 members (`m_iExtra{Intercept,Evasion,CollateralDamage,
   CaptureProbabilityModifier,CaptureResistanceModifier}`) deleted; `getExtraEvasion` / `getExtraIntercept` /
   `getExtraCollateralDamage` flipped to cache reads (commander/commodore fold preserved; the MAX clamps stay in the
   `evasionProbability()` / `maxInterceptionProbability()` composites); `captureProbabilityTotal` /
   `captureResistanceTotal` flipped to cache reads (type base + commander + national + local-city + `max(0,·)` all
   preserved on top); the 5 changers + their apply-loop folds retired into the existing `markDirty(UPK_ALL)`;
   serialization drained (`savemigration.txt`). Collateral migrated ONLY the `damage` member — the
   limit/maxUnits/protection siblings stay legacy (different members/families). **Remaining:** **bombard + cargo defer
   to the SizeMatters carve-out (§4)** and **vision-range to step 3** (its `changeAdjacentSight` plot side-effect) —
   flagged, not forced.
   *(original step-1 scope, for the record:)* **Stand up the scope + the gather-on-dirty fold**, validated on the
   **simplest scalar groups first** — no side-effects, no keying: withdrawal / firstStrike (both members: `strikes`
   count + `chance` probability) /
   evasion / intercept, heal (territory family: enemy/neutral/friendly/sameTile/adjacentTile), collateral /
   bombard, capture / vision-range / cargo-size scalars. **Each group ships as a FULL VERTICAL SLICE (owner ruling
   2026-07):** flip its getter(s) to read the cache + retire its apply-loop fold (→ `markDirty`) + delete its
   `m_iExtra*` member(s) + drain the tag(s) in `Assets/savemigration.txt` — all in this step, per group. The
   serialized-stack removal (old step 4) is therefore folded PER-GROUP here, not batched at the end — no
   dead-but-serialized member ever lingers ([DEC-save-remove-is-soft](../../architecture/decisions.md#dec-save-remove-is-soft)
   makes the drain zero-risk, so there is no reason to defer it). Verified findings: step-1 deposits are
   **UNCONDITIONED** (no `enabled`/`disabled` on these channels — plain `MMKernel::sumUnit`, so unit-predicate
   evaluation is a step-2 concern, not step-1's); the getters **compose commander/commodore inheritance** on top of
   the accumulator — that live cross-unit fold STAYS, riding on top of the cached own-gathered value
   ([DEC-unit-modifiers-on-top](../../architecture/decisions.md#dec-unit-modifiers-on-top)); the three
   `getExtra{Enemy,Neutral,Friendly}Heal` getters are **reused as spy-mission strengths** (`CvPlayer.cpp:16117/16128/16140`) —
   transparent to the flip (same number, cascade-sourced). ⚠ **The withdrawal mid-combat transient**
   (`CvUnit.cpp:7648-51`, sea-pillage counter-attack: `changeExtraWithdrawal(-withdrawalProbability())` … `attack()` …
   restore) cannot mutate the now-read-only cache — preserved via a **small transient suppress flag on `CvUnit`**
   (owner ruling 2026-07) honored by `withdrawalProbability()`, set/cleared around the pillage attack (live runtime
   override on top, never an accumulator).
2. **Core strength + combat-percent** groups (the largest consumer — `maxCombatStr`'s situational calc reads these
   aggregates). ✅ **Design resolved (map 2026-07-19): this is the step-1 standing-accumulator pattern with more
   fields — NO combat-time gating.** The situational percents are authored as SEPARATE address families
   (`strength.unit.cityAttack.percent`, `strength.unit.hillsDefense.percent`, `strength.unit.{attack,defense,vsBarbs,
   religious,stealth}.percent`, …), NOT gated `strength` deposits — **verified 0 of ~1230 promotion/unitcombat
   strength deposits carry any `enabled`/`disabled`**. The situation is carried by the address NAME and applied by
   `maxCombatStr`'s EXISTING plot guards (`isCity`/`isHills`/attacked-plot), so the gather needs no combat context:
   each situation is a plain `Σ sumUnit("strength.unit.<situation>","percent")` over the held-set, one cache field
   per situation; the consumers (`maxCombatStr`/`airMaxCombatStr`/the `*Modifier()`/`*Total()` composites) are
   UNTOUCHED (they keep the info-base add, the situational guards, and the commander/commodore fold). ⚠ Preserve the
   changers' one rider — `setInfoBarDirty(true)` — at the promotion/unitcombat change site. *(The doc's earlier
   "IS_CITY/IS_HILLS enabled predicates" line was an assumption about the spec's predicate model; the curated data
   uses separate families instead.)* Base-strength FLATS (`strength.unit.flat` → `m_iExtraStrength`, feeding
   `baseCombatStr`) are a separable follow-in of the same shape (verifiable via `/computed/units/combat` `combatEff`);
   the KEYED terrain/feature/unitcombat/domain percents are step 3.
3. **Keyed groups — ⛔ the unit-classification half is BLOCKED (owner-flagged 2026-07-19; confirmed against
   [code-cut-map.md](code-cut-map.md) §group 11/18/34/51, [json.md §3.7](../../specs/json.md), [tags.md](../../specs/tags.md)).**
   These are NOT a straightforward `sumKeyed4U` gather. Split by target kind:
   - **Unit-classification "vs" modifiers (unitcombat / domain)** — a "vs mounted" / "vs land" combat modifier is the
     spec's `strength` deposit qualified by a `unit:` PREDICATE reading a TAG (`{unit: IS_MOUNTED}`, `{unit: IS_LAND}`),
     NOT a raw `strength.unit.unitCombat.{X}.percent` keyed channel. The `mounted`/`gunpowder`/`mechanized` tags come
     from the **unitcombat→tags distillation** — a BLOCKED data-migration tail (`curate_unitcombat.py` emits no tags
     today, [data-migration-remaining.md](data-migration-remaining.md); code-cut-map §51/§group 34). So these are
     **double-blocked**: they need the tag (the distillation pass) AND the `unit:` predicate wiring. Building them as
     raw unit-combat/domain keyed channels REINSTATES exactly the legacy class-targeting the tag model dissolves — do NOT.
   - **Plot-substrate keyed (terrain/feature attack/defense/work)** — keyed by real `TERRAIN_`/`FEATURE_` entities (not
     a unit classification). code-cut-map §group 11 gives the replacement as a target-predicate deposit; confirm the
     exact authored form (predicate vs keyed) + whether the 5-segment address recurates BEFORE building. In
     code-cut-map's BLOCKED section, no consumer built.
   - **vision/invisibility** (15 setters, `updateSpotIntensity` side-effects) — its own sub-phase.

   ⚠ The scalar plane (steps 1–2, LANDED) correctly uses unit-combat as a deposit **SOURCE** (code-cut-map §group 18 /
   line 1285 — the modifier-VALUE half, specced by [modifier.md §6](../../specs/modifier.md)); that is DISTINCT from,
   and unaffected by, this classification/target-keying blocker.
4. **Serialized-stack removal is now PER-GROUP (folded into steps 1–3, owner ruling 2026-07), not a batched final
   step.** Each group's slice already full-deletes ITS `m_iExtra*` member(s) (member + WRAPPER_READ + WRITE) and
   names the drained tag(s) in `Assets/savemigration.txt`
   ([DEC-save-remove-is-soft](../../architecture/decisions.md#dec-save-remove-is-soft); the reader drains old-save
   orphans transparently — old saves load clean forever, no version bump, no `WRAPPER_SKIP_ELEMENT`). So this step
   is the FINAL RECONCILE, not a removal: once the last group lands, confirm zero `m_iExtra*` remnants remain and
   the three apply-loops (`processPromotion`/`processUnitCombat`/`processLoadedSpecialUnit`) have fully retired onto
   the cache's dirty-mark (each shed its folds group-by-group; the loops themselves vanish when their last folded
   channel migrates).
5. **Upkeep — lands WITH F4** ([fixed-point-conformance.md](fixed-point-conformance.md), named F4 dependency, not a
   separate slice). It is the forcing case for a unit-scope channel feeding back UP into a player-scope
   accumulator: `CvUnit::m_iExtraUpkeep100` (deposit-fed) → `calcUpkeep100` (`:15808`) → pushes the delta into
   `CvPlayer::m_iUnitUpkeep{Civilian,Military}100` bucketed by `isMilitaryBranch()` (`isMilitarySupport` tag).
   Design the player sums as recompute-Σ over live units' cascade-derived upkeep (or targeted-propagation push from
   the unit package), and restructure `getFinalUnitUpkeepChange` (the AI disband marginal-cost what-if,
   `CvPlayer.cpp:10192`). Base upkeep (`cost.upkeep`) is already curated.

## 4. Explicitly OUT / separate (do not fold into the package build)

- **Movement / range** — its own `(unit,edge)` resolver ([modifier.md §6](../../specs/modifier.md); the movement
  subsystem doc is pending), NOT the deposit-DOWN shape. Needs its own design pass.
- **SizeMatters** — a separate rework layered on the same infra later ([json.md §9](../../specs/json.md),
  code-cut-map group 21). The promotion-delta half reuses the package; its load-time base-rank derivation (Σ over
  combat classes' `*Base`, feeding `getUnitCountSM` = `count / 3^(groupRank−1)`) + the runtime merge/split
  accumulators (`getExtraQuality/Group/Size`, live engine state) are its own. ⚠ `changeExtraGroup` carries a
  `changeUnitCountSM`/`AI_changeEffNumAIUnitsTimes100` side-effect to preserve.
- **Empire unit-count "ride-on-top" families** (military happiness/anger, garrison counts) — already correctly
  modeled ([DEC-unit-modifiers-on-top](../../architecture/decisions.md#dec-unit-modifiers-on-top); realized for
  military happiness in `CvCascadeWellbeing.cpp:303/722`: the per-unit VALUE folds at gather, the live count
  multiplies at read). Untouched by F4. ⛔ Enforce the hard ban: no unit-authored `percent` deposit to yields/commerce.
- **⛔ The unit-combat → TAGS/SKILLS distillation (and the keyed "vs classification" modifiers that depend on it) —
  OUT of #430/F4 scope, needs its own serious grounded plan (owner 2026-07-19: NOT "giga rollerskating in").** Design
  direction (owner): a **UnitCombat is a "general unit-group"** — a unit with no strength / unitdata of its own,
  **just pure modifiers** applied to a group of member units. Two axes distill from it, and only ONE is in F4's
  scope: (1) the **modifier-VALUE half** — the group depositing its stat modifiers onto its members — IS the
  self-accumulator SOURCE the scalar plane (steps 1–2) already uses correctly ([modifier.md §6](../../specs/modifier.md),
  code-cut-map §group 18); (2) the **classification half** — extracting the `mounted`/`gunpowder`/`mechanized`
  **tags** and the ability **skills** out of the ~150-field UnitCombat, and re-expressing every "vs unit-combat-class"
  modifier as a `{unit: IS_<TAG>}` PREDICATE deposit — is a **separate data-migration + design pass**
  (`curate_unitcombat.py` emits no tags today; [tags.md](../../specs/tags.md), [skills.md](../../specs/skills.md),
  [data-migration-remaining.md](data-migration-remaining.md)). Until that pass has its own grounded plan and lands,
  the keyed unit-classification modifiers (step 3, above) stay BLOCKED — never built as raw unit-combat/domain keyed
  channels.

## 5. Open classifications to resolve before wiring their channel

- `changeExtraBuildType` — skill/capability (can-perform-build) vs modifier magnitude (code-cut-map group 7).
- `changeExtraNoDefensiveBonusCount` — gate-count vs magnitude.
- Bespoke C2C families (trap/breakdown/insidiousness/poison) — confirm a named json family exists (or mint one)
  before wiring; a data-migration-tail risk, not just an unbuilt consumer.

## 6. Acceptance

Per item, verified LIVE ([validation.md](../../specs/validation.md)): the cascade-derived unit stat matches the
legacy consumer's value on a loaded save + a real combat/turn, via the endpoints (the unit combat/movement channel
decomposition is added when its channel is worked — [http-endpoints.md](../../specs/http-endpoints.md)); the
`(scope,channel)` calc-count stays event-proportional (a unit's cache recomputes only on promotion/unitcombat
change, not per read/move). The removed serialized tags are named in `Assets/savemigration.txt` (the soft-remove
drain — no save-break, [DEC-save-remove-is-soft](../../architecture/decisions.md#dec-save-remove-is-soft)).
