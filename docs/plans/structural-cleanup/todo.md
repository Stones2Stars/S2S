# #430 — the TODO

> **What is NOT done. Nothing else** ([DEC-spec-plus-todo](../../architecture/decisions.md#dec-spec-plus-todo)).
> The DESIGN lives in the specs; this list measures what is LEFT. A finished item is **DELETED** from here — never
> ticked, never annotated — and anything durable it established moves into its owning spec first. Git history is
> the record of work done.
>
> ⛔ **Verify before you act.** Every line here is a claim about the tree; confirm it against the code before
> building on it ([DEC-no-guessing](../../architecture/decisions.md#dec-no-guessing)). Sequencing and the
> governing rulings: [roadmap.md](roadmap.md).

## Blocked on an owner ruling

- **The `savemigration.txt` parser is PREFIX-FREE** — it skips only `|`/`=`/`#` lines and registers the first
  whitespace-delimited token containing `::`, so the file's own documented `CUT:`/`RENAME:` prefixes are IGNORED
  and a wrapped prose line beginning with a live `Class::m_member` token would silently drain that field on every
  load. The fix (require the documented prefix) changes save-load behaviour, so it is an owner call.

## Data — curator batches

- `culture.unit.garrison` · `costs.empire.perInstance` — flagged in-code, awaiting their batch.
- The ruling-16 trigger-plane set (`survivor`, `cityCapture`, `combat.subdueAnimal`, `combat.nukeInterception`) —
  each attaches to its trigger's `chance`; authoring shapes finalize with the trigger system's build-out.

## Data — the `identity` effect re-home

> The ruling: `identity` carries NO effects ([json.md §7](../../specs/json.md)). The shipped data does not obey it
> — **103 of 213 authored identity keys carry effects, 5,571 authorings** (the other ~74k are text, display and
> genuine metadata, and stay). Each key re-homes to the block that already exists for its kind; the count is a
> worklist, never a licence.

- **Held booleans → the classification blocks.** The plot substrate's block now exists (`characteristics`,
  [json.md §8](../../specs/json.md)) and the six unambiguous ones have moved. What is LEFT is the substrate keys
  that are held-boolean-SHAPED but may be constraints instead — `flatMovementCost` (route), and deciding whether
  `noImprovement` / `noCity` / `noBonus` (feature) are characteristics or `requires` gates. The spec's bound is
  the test: what the substrate IS or DOES is a characteristic; what may exist THERE is a gate.
- **Magnitudes → modifier families** — `sightRange` (vision), `cargo`, `captures`, `conscription`,
  `controlPoints`, `espionagePoints`, the radii (`cityRadius`, `workableRadius`, `cultureRange`).
- **Constraints → `requires` / `allowed`** — `terrainImpassable`, `featureImpassable`, `requiresFlatlands`,
  `validTerrains`, `minAreaSize`, `distanceToLand`, the `found*` gates.
- **Keys with a home already specced** — `tradeable` (910, TECH) is the `canTrade` block
  ([capabilities.md](../../specs/capabilities.md)); `commerceDoubleTime` is a second deposit gated on
  `existedFor` ([json.md §3](../../specs/json.md)); `advancedStart` is already flagged *"parked in identity …
  pending review"* by `curate_handicap.py`; `pillageGold` (131, IMPROVEMENT) is recorded as an ORPHANED dead
  field ([legacy-value-calc-map §10.3](../../reference/legacy-value-calc-map.md)) and drops rather than moves.
- **The inert test's identity whitelist is a SYMPTOM of this** (`curate_common`): it exists only because identity
  currently carries effects. When the re-home lands the carve-out goes with it and the section test alone is
  enough.

## Data — blocked on a prerequisite

- **`paralyze` → the `state` block** — blocked on the greenfield `state` model ([state.md](../../specs/state.md)),
  which is work to BUILD. No data is lost meanwhile.
- **The FLAGGED unitcombat remainder** — the taxonomy families (weapon/size/species/quality/group) and the
  ambiguous individual classes; map the obvious, flag the unsure, never blunt-purge
  ([unitcombat-tag-mapping.md](unitcombat-tag-mapping.md)).
- **`stronglyRestricted` → a `requires.build` civ-membership gate** — pending NPC civilizations being wired.
- **Property pulses** — a shared property-source cleaner so spatial sources emit as trigger entries carrying
  `on`/`relation`/`distance`, instead of being parked verbatim. Pure DATA and unblocked; the ENGINE spatial
  distribution that later reads those fields is a separate consumer.
- **Corp HQ revenue** (`HeadquarterCommerces`) — rides the corporation rework carve-out.
- **`largestCity` cannot retire** until ranked-target-selection EVALUATION lands, so the civic/trait curators still
  emit the legacy member.

## Data — cross-curator claims to VERIFY

- `curate_bonus` actually inverts the civic bonus-commerce modifiers (the civic curator drops them on that promise).
- The yield resolver reads `identity.movementCost`.
- The property propagator/change-propagator re-homes actually happen at the unit/building passes.
- PropertyBuilding min/max value bands are consumed by the building pass.
- Every bespoke second-pass tag has live emit code, not just set membership.
- Stale tooling docs: `Tools/Migration/README.md` references a non-existent `curate_pocos.py`, and
  `curate_building.py`'s docstring claims second-pass tags show as UNHANDLED when they are mostly implemented.

## Legacy still breathing — the KILL LIST

> The standing rule (purge violently; blast radius is the signal; the worst offenders are the ones OFF the core
> loop) is [roadmap.md § LEGACY STILL BREATHING](roadmap.md). ⚠ KNOWN-INCOMPLETE — legacy found anywhere else is
> killed on the same terms; add it here. ⛔ Never record a found legacy surface as acceptable or "kept until X".

- **The hand-named channel-shaped getter set** on `CvCity.h`/`CvPlayer.h` — the new group reads stand beside them
  today, which is the two-live-surfaces state
  ([DEC-new-getter-surface](../../architecture/decisions.md#dec-new-getter-surface)) forbids. Move every consumer,
  delete the old names.
- **The `Cy*` info binding surface** (`Sources/Python/`) — cut away WHOLE once the new library lands; never
  widened, shimmed beside, or left breathing.
- **`CvCity`'s hand-rolled dirty caches** — demolition fodder, never conversion targets; cut when the channel that
  replaces them lands.
- **The direct `gDLL->logMsg` / BetterBTSAI log-helper call sites** and the log-level globals they gate — retired
  WHOLESALE as each domain migrates onto the spine, never tidied in place.

## Not built yet

- **The endpoint route table** beyond the six stored-vs-oracle documents — it stays empty until the access surface
  can be read THROUGH, never restored to reach around it ([http-endpoints.md](../../specs/http-endpoints.md)).
- **The Python data-fetching library** — see Stage 4 below.

## Stage 4 — the consumer cut (sequenced LAST; see the roadmap's ORDER ruling)

- **The `CvCity`/`CvPlayer` getter consolidation** — known work, not the primary focus, and a fair few collapse on
  their own as the rebuilt infos wire through. Measure what survives that before planning a sweep.
- **The AI call sites** — the largest consumer of the info surface, deliberately last. A dangling AI call site is
  intended output, not a defect to fix on sight.
- **`CvGameTextMgr` composers onto rendered entry lines** — the per-entry renderer exists (`Sources/UI/CvEntryText`);
  the ~18 info-help composer families still hand-assemble from getters.
- **Re-point the unit consumer getters onto `resolvedValue()`** (`Sources/Cascade/CvUnitResolved`).
- **The unit power-value plane** — its readers are ordinary consumer debt on a deliberately red tree.
- **The Python data-fetching library** — built COMPLETE, then the `Cy*` surface disconnected whole. Contract:
  [patterns.md § THE PYTHON READ BOUNDARY](../../architecture/patterns.md). Build it for the pedia (a SHAPE oracle,
  NOT a coverage oracle — the appendix is enumerable). Read maps: [pedia-map.md](../../reference/pedia-read-map.md) ·
  [python-read-map.md](../../reference/python-read-map.md).

## UnitCombat distillation

> The concept + the target model: [engine.md § UnitCombat](../../reference/engine.md). The MINIMUM that unblocks
> the stuck consumers is the cascade-QUERY surface, not a full re-taxonomy.

- **Emit `tags` from `curate_unitcombat.py`** — it emits families/skills/vision/outcomes/sizeMatters/identity and
  zero tags today, so the tech/equipment classes (`mounted`/`gunpowder`/`mechanized`) are unauthored. Greenfield:
  there is no legacy flag to migrate from, and a reasonable FIRST PASS is fine (a wrong tag is a quick data edit,
  not a perfection-gated blocker). Worklist: [unitcombat-tag-mapping.md](unitcombat-tag-mapping.md).
- **Fold combat-class tags into a unit's effective tag set** (unit-level ∪ primary ∪ subs ∪ promotion-granted).
- **Re-express the keyed "vs unit-combat-class" modifiers onto TAG predicates**, retiring `UNITCOMBAT_*` as a
  modifier target. ⚖ **POST-REWORK, its own dedicated pass (owner)** — until it runs, tags and unitcombats live
  side by side, which is sanctioned and costs nothing (the mapping is additive).
- **Reconcile the double flags** — `bSpy` lives on both the unit and the unit-combat; unify onto the `spy` tag,
  same for `outlaw`/criminal.
- **Purge the classes that exist only to IDENTIFY** — expected to be hundreds of files, and **GATED on tags taking
  over the identifier role**, not opportunistic: purging ahead of that removes classes still doing identifier duty
  (the blunt purge that over-reached and was reverted). Bounded by the unreferenced-is-not-dead caveat — module XML
  holds assignments. Candidates: [unitcombat-merge-candidates.md](unitcombat-merge-candidates.md).

## Triggers / grants

- **Start packages are DESIGN, not built** ([triggers.md](../../specs/triggers.md) § Game-start provisions): the
  entity type, its folder + prefix + repo row + manifest, and the shipped default packages. Two content decisions
  ride it — which units the defaults name, and NPC/barbarian starts (not authored in a grants block today).
- **Retiring the engine start selection** — the whole-database scan + AI scoring, and the per-role starting counts,
  retire once packages carry the identities. Until then they remain the live path.

## Scale conversion

> Method: [fixed-point-and-scales.md § CONVERT BY ARITHMETIC CLUSTER](../../specs/curators/fixed-point-and-scales.md).
> Mechanism for cutting an accumulator: [state-repositories.md](../../architecture/state-repositories.md).

- **Convert the remaining human-twin getters cluster by cluster**, never getter by getter. The clusters that still
  mix: yield/food/wellbeing (the keystone — food consumption subtracts angry population and health rate), commerce
  (joins it at the production→commerce term), gold/maintenance/upkeep (gold IS a yield, so it rides with commerce),
  trade profit, war weariness. Unit experience is self-contained and is the one safely parallelizable cluster.
  Acceptance per cluster: ZERO new fudge factors at the mixing sites.
- **⚠ Needs an owner ruling before being swept in:** the `…Times100` on AI unit counts and plot strength carries
  fractional SizeMatters counts, not a modifier channel — same shape, different nature.
- **`getFinalExpense` folds a ×10000 inflation modifier** — a third scale to reconcile when the gold cluster converts.

## Tree / include hygiene

- **Retire the `CvInfos.h` umbrella** (~177 includers) — a hand-careful pass; the lessons and the hard bans are in
  [AGENTS.md](../../../AGENTS.md) Conventions §Design.
- **The dead-code / dead-XML pass** — tooling generates CANDIDATES only; nothing is auto-deleted, every removal is
  verified against source/data and test-loaded against a save, one subsystem at a time.
- **The CTB `/events` blind spot** — the `[CTB/work/intransit]` block gates on a different log global than every
  other CTB gate and its line stays legacy-only, so it never reaches `/events`.

## Green-up (after the structure, never ahead of it)

- Engine-repair debt: the bare Engine includes · `CvOutcomeMission::mapFrom` · the property-manipulator helpers ·
  `CvCity.h`'s functor row.
- The vocabulary TXT keys (one per family/kind/predicate/token) — polish on a working machine, sequenced after the
  stages complete; the renderer's spell-back fallback is the accepted output until then.

## Enabler

- **The frontier's every-turn full rebuild** — the GENERATE walk is a pure function of HAVE and must be computed
  once per HAVE-change, but events that do not affect it blanket-dirty the unit frontier, forcing a full re-walk on
  the next `canTrain` read. Buildings have an incremental path; **units never got one**. The operating-building
  fixpoint rides the same triggers and recomputes alongside. (The old root-cause trace named the archived
  substrate — re-locate the trigger sites on the rebuilt consumer before acting.)
- **The AI production decision iterates the finished set** — named frontier reads returning the maintained LISTED
  set, and the `AI_chooseProduction` focus-ladder collapse into ONE unified scoring pass
  ([enabler.md §6/§8](../../specs/enabler.md)). The focus-ladder collapse is an AI-architecture change, not a
  per-loop rewrite.
- **⛔ THE ACTIVE-SET WORK-LIST RIPPLE IS A SECOND PROPAGATION MECHANISM, and it exists only because a fact is
  missing (owner).** A building's operate verdict depends solely on its OWN operate atoms, so it can know by
  itself: the event names the changed atom, `EDGEF_REQUIRED_BY` names the dependents, each re-evaluates itself —
  which is machinery the building enabler ALREADY has and uses. The provides→operate chain needs no fixpoint
  work-list either: if each active flip announces its supply change, the chain propagates through the spine one
  fact at a time and terminates naturally, because a no-op write emits nothing.
  **The missing fact that justified it is now emitted** — a present building going active↔dormant announces its
  supply crossing, so the routed path reaches the vicinity-conditioned packages and the `requires.build` gates that
  the work-list could never reach. **What is LEFT is retiring the parallel machine itself:** the enabler hand-rolled
  this propagation because it could not route the flip
  ([DEC-single-implementation](../../architecture/decisions.md#dec-single-implementation)), and that reason is gone.
  - **Its runaway cap "self-heals at the slice boundary"** — a self-heal
    ([DEC-no-self-heal](../../architecture/decisions.md#dec-no-self-heal)), and the slice-boundary rebuild it names
    was REMOVED. Nothing heals it: if the cap trips, the operating set stays silently wrong. It is an assert today,
    which surfaces the trip in `Assert`/`Debug` only — the shipped builds compile `FASSERT` out
    ([Sources/AGENTS.md](../../../Sources/AGENTS.md)), so a live trip stays silent until the work-list goes.
  ⚠ **The one real design constraint:** `emit()` dispatches SYNCHRONOUSLY, inline at the mutation site, so an event
  chain recurses on the call stack where the work-list iterated. Depth is the chain length (the manufactured
  ore→wares→firearms ladder). Design for that — it is not a reason to keep the parallel machine.

- **The operate reverse index — NARROWER than it looks, and one part of it is a perf trap.** Verified in tree: the
  building and unit buckets are ALREADY converged and gone; what remains is the operate index in
  `CvEnablerKernel.cpp`, and it splits into two genuinely different classes:
  - **Per-id buckets** — the two per-id buckets (BONUS→buildings, BUILDING→buildings) — are
    the only true duplicates of `EDGEF_REQUIRED_BY`
    ([DEC-one-reverse-view](../../architecture/decisions.md#dec-one-reverse-view)).
    ⚑ **The over-inclusion is MEASURED and small, so this convergence is cheap.** The canonical axis is
    `requires`-GENERAL (it records the dependent's KIND but not the build-vs-operate TIMING, so the distinction is
    unrecoverable from the edge) while these buckets are operate-SPECIFIC — so the swap drags build-only dependents
    into the provides-ripple fixpoint. Across the authored data that is **196 build-only edges against 3,921
    operate ones (~5%)**: the resource requirements are overwhelmingly `operate`, because a resource gate folds
    into operate so the building DORMS when supply is lost (the band model working). Safe by the over-inclusion
    invariant ([enabler.md §5](../../specs/enabler.md)) and cheap by measurement.
  - **Axis-flag lists** (population / power / golden age / state religion / live-state, and the coarse
    religion / corporation / civic / tech lists) plus the PROPERTY band index are **NOT convergence targets**: the
    reverse pass deliberately excludes engine tokens, the plot substrate and `PROPERTY_` bands, and the coarse
    lists match coarse events (a religion/civic CHANGED fact names no id). Making those per-id would be a
    REFINEMENT of the re-gate, not a convergence — and it is the lever on the operate fixpoint's cost.
- The remaining §8 open items: residency/counting, plot-group membership not trusted from a save, the load-end
  dormancy fixpoint, the dynamic operate axes ([enabler.md §8](../../specs/enabler.md)).

## Spec gaps to close

- **The mod-data design invariants have no spec home.** `TestCode.py` encoded ~50 consistency checks — a
  requirement may not unlock after the thing requiring it; replacements are explicit, never implicit; a replacing
  entity must be better — and the JSON spec does not currently state them. The checks are gone; the invariants
  belong in the spec.

## Known gaps carried deliberately

- **Game-option flips carry no DOMAIN event** — a mid-game toggle would not re-mark. An emit endpoint is the fix
  if/when WorldBuilder option toggling is in scope.
- **Ranked-target-selection EVALUATION** is parked ([parked/ranked-target-selection.md](../parked/ranked-target-selection.md));
  a ranked entry applies unranked until it lands.
- **The `expected*` endpoints have no callers yet**, so the two pass-in scenarios hold vacuously — the seam is
  built ahead of the package graft by design.
