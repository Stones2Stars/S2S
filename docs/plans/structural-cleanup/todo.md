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
> — **91 of 201 authored identity keys carry effects, 5,073 authorings** (the other ~74k are text, display and
> genuine metadata, and stay). Each key re-homes to the block that already exists for its kind; the count is a
> worklist, never a licence. ⚠ That count is the sweep's SCOPE, not a defect tally — the bullets below have since
> resolved several of the keys it counts as metadata, carve-outs, or channels that already exist. Read the
> disposition, not the number.

- **Magnitudes whose family ALREADY EXISTS — a curator move, not a design question.**
  - **A carrier whose restriction has no base capacity to sit on — 31 units, flagged
    `cargo_restriction_no_capacity`.** The restriction folds onto the `cargo.space` entry as its `{unit: …}`
    qualifier, so a carrier with `iCargo: 0` has no entry to carry it and the rule does not author. The split:
    **19 ancient naval transports** declaring a DOMAIN they carry (Trireme, Quinquereme, Bireme, Dromon) and
    **12 modern warships** declaring a special group (missile destroyers/battleships, an ironclad, a
    seaplane-carrying battlecruiser).
    ⚑ **NOT leftovers — this is the normal shape for that whole line.** Those galleys earn their hold from
    `PROMOTION_TRANSPORT1/2/3` (`cargo.space.flat: 1` each, on `UNITCOMBAT_WOODEN_SHIPS`, which every one of them
    carries): the CARRIER declares what, the PROMOTION supplies how much. The composition is ruled — a
    restriction governs the carrier's whole hold, promotion-granted space included
    ([modifier.md §6](../../specs/modifier.md)) — so what is missing is only the SHAPE: the §3.9 entry grammar
    has no payload-less form for a restriction with no amount of its own.
    ⚠ **The declared domain may overstate what they take.** `DomainCargo: DOMAIN_LAND` permits any land unit,
    but the owner recalls an unpromoted galley taking only a settler or other civilian — **explicitly an
    UNCONFIRMED recollection ("it has been a loooong while"), so it is a question to settle in-game, never a
    premise to author against.** What IS verified in the data: `UNIT_TRIREME` authors no `iCargo` at all, while
    `UNIT_GALLEY` authors `3` — so the galley plainly carries and the trireme's hold, if any, comes from the
    promotion line. Settle whether a civilians-only rule exists before deciding the shape must express it.
    ⚑ Nothing that worked is lost: all 31 sat inert in `identity`, read by nothing; the flag makes them loud.
  - `espionagePoints` (24, UNIT) → the **`espionage`** family — one of the four commerce channels (owner), so
    the family already exists. The value is an espionage-commerce amount delivered as a ONE-SHOT payload, which
    [json.md §8](../../specs/json.md) already covers ("reused families for one-shot yields"). ⚠ Its CARRIER is
    `MISSION_INFILTRATE` — `canInfiltrate` gates on it being non-zero and `infiltrate()` spends it — so the
    authoring home rides the missions/`CvOutcome` PERMANENT carve-out, not this sweep. The channel is settled;
    only where it is written waits.
- **`conscription` (247, UNIT) is NOT an effect — it is a SELECTION WEIGHT, and it stays in identity.** Its one
    consumer picks the best draftable unit: `CvCity` walks the enabler's LISTED frontier and keeps the highest
    `getConscriptionValue()`. It deposits nothing and produces nothing; it RANKS. That is the same class as
    `worth` / `militaryWorth`, which the inert whitelist already treats as metadata — so the census bucketing it
    as an effect was the inconsistency, not the data.
- **`cultureRange` (15, IMPROVEMENT) rides the SPATIAL carve-out.** It is the radius of
    `pushCultureFromImprovement` — a `rect(iRange, iRange)` walk pushing culture onto every plot within
    `plotDistance <= range`. Plot-culture SPREAD is the #429 spatial plane
    ([legacy-value-calc-map §9.4](../../reference/legacy-value-calc-map.md)), not a deposit down the scope spine,
    so it moves with that rework rather than into a family.
- **The city WORKABLE RADIUS is PURE STATE (owner) — the building side is DONE, the culture side is config.**
    Per [contexts.md](../../architecture/contexts.md) *"if it is current state, it is the CONTEXT's, there is no
    third home"*, the resolved radius is `CityContext` business, driven by culture expansion; the doc already
    leans on this, maintaining the vicinity tiers off the culture-level fact BECAUSE the radius grows with
    culture. `cityRadius` (19, CULTURELEVEL) is that mechanic's config parameter and stays with it.
    ⚑ The building override is now the **`adds3rdRing` attribute** (owner): every one of the 12 authorings was
    the same number 3, so the field carried no information — what a building says is the boolean "this city gets
    the third ring early". Only `METROPOLITAN_ADMINISTRATION` (renaissance) does real work with it; the other 11
    are transhuman-and-later, by which point culture already grants 3 on its own.
- **`controlPoints` (28) is NOT a magnitude — it is the COMMANDER system's per-turn budget.** A Great
  Commander backs one combat per point: a unit fighting within `commandRange` spends one via
  `tryUseCommander`, at zero the commander supports nobody else that turn, and `restoreControlPoints` refills
  at turn start (a commodore twin exists for the naval side). So it is a capacity + a spend + a refill —
  closest in shape to the WAREHOUSE carve-out ([north-star.md](../../architecture/north-star.md)): the capacity
  could be a channel, but the per-turn ledger is the object's own business.
  ⛔ Do NOT mint a commander family for this one key — that is the machinery-for-one-mechanic move declined for
  counter-damage. The commander/commodore system is simply UNMAPPED: it appears in no carve-out list and no
  spec, so it wants a pass of its own before any of its data is re-homed.
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

## Vision

> The model: [vision.md](../../specs/vision.md) — ONE `vision` family whose kinds are STRENGTH (the observer's,
> memberless), `elevation` (height, positional — the ground's or a city's) and `obstruction` (what the ground
> costs to see through). A budget spent walking outward, exactly as movement is spent. Data, spec, engine read
> path and the pedia render are all on it; what is below is what is NOT.

- **The hide-and-seek CONSUMER census: 26 `CvPlayerAI` valuation reads + 7 `CvGameTextMgr` help reads.** The
  four per-type intensity getters are DELETED, so these are compile errors rather than silent zeroes — ordinary
  consumer debt, sequenced with the AI cut, and a dangling site here is intended output. Their replacement is
  `CvUnit::concealment()` (one number) and `CvUnit::detectionAgainst(method)`. ⚠ The AI sites SUM inside a loop
  over every `INVISIBLE_*`, so a mechanical swap would count concealment fourteen times — the loop collapses to
  one read, which is why this is a rewrite rather than a rename.
- **The hide-and-seek help text still enumerates per type** — spot intensity, spot range and same-tile, one
  block per `INVISIBLE_*`. It renders values that are now always 0, and it is the exact thing the pairing was
  written down to make sayable: a detection entry renders itself through `appendEntryLines`.
- **Three AI valuation reads of the deleted improvement getters** (`CvCityAI` once, `CvUnitAI` twice) — the
  compiler census, sequenced with the rest of the AI consumer cut, not fixed on sight. Their replacement is the
  improvement's compiled `vision` entries, the same source the pedia now renders from.
- **Nothing is verified.** The walk, the budgets and the render are wired but untestable until the tree is
  green ([DEC-done-is-observable](../../architecture/decisions.md#dec-done-is-observable)). First checks when it
  is: a unit on flat open ground sees 1 plot, on a peak 4; a jungle costs 2; a city with tree platforms sees 2.

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

## Data — the substrate movement cost reads a member NOTHING authors

⛔ **`CvTerrainInfo` / `CvFeatureInfo` / `CvRouteInfo::getMovementCost()` return `m_iMovementCost`, mapped from
`identity.movementCost` — which no entity authors any more.** The curator moved every substrate to the
`movement` family (102 terrains · 88 features · 21 routes author `movement.plot`; **zero** author the identity
key), and the engine getters were never rewired, so all 211 answer **0** and `CvPlot::movementCost` loses terrain
differentiation entirely. A silent 0 is the masked-hole class
([DEC-no-legacy-masking](../../architecture/decisions.md#dec-no-legacy-masking)); the spec is unambiguous that a
substrate's base cost IS the family ([modifier.md §6](../../specs/modifier.md)).

⛔ **The fix is NOT to re-point `getMovementCost()` at the family — that is the computed-getter FLIP, and it is
dead** ([superseded-ideas #15](../../architecture/superseded-ideas.md),
[DEC-new-getter-surface](../../architecture/decisions.md#dec-new-getter-surface)). Keeping the legacy signature
and swapping its body leaves every call site untouched, which is the half-migration tell, not the win. The getter
is the wrong SHAPE regardless of what feeds it: a per-channel scalar on an info is the very thing
[patterns.md](../../architecture/patterns.md)'s DATA-OUT contract replaces with a per-GROUP parameterized read.

⇒ **`getMovementCost()` is on the DELETION list.** The substrate serves the family through the coherent surface —
`getFlatMovement(MovementKind, CvCascScope)`, mirroring the `getFlatCombat` already in tree — and
`CvPlot::movementCost` is re-expressed onto it rather than left believing it still reads a scalar.

⚠ The SCALE conversion rides along and does not lead: the family is ×100 while `m_iMovementCost` was human, and
the resolver mixes it with `MOVE_DENOMINATOR`, the hills/river/peak extras, the route `min`-override and the
unit's own moves — so the reduce belongs at that consumer, once, with no compensating constant left at any mixing
site ([fixed-point-and-scales §4c-bis](../../specs/curators/fixed-point-and-scales.md)).

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
- **`CvGameTextMgr` composers onto rendered entry lines** — the per-entry renderer exists
  (`Sources/UI/CvEntryText`) and `CvGameTextMgr::appendEntryLines` is the shared consumer, but only the
  VISION family has moved onto it. The other info-help composer families still hand-assemble from getters.
  ⚑ Each move DELETES composer code rather than porting it: a rendered line already carries magnitude,
  unit, target, scope, per-scaler and conditions, so a new channel needs no composer edit at all.
- **Re-point the unit consumer getters onto `resolvedValue()`** (`Sources/Cascade/CvUnitResolved`).
- **The unit power-value plane** — its readers are ordinary consumer debt on a deliberately red tree.
- **The Python data-fetching library** — built COMPLETE, then the `Cy*` surface disconnected whole. Contract:
  [patterns.md § THE PYTHON READ BOUNDARY](../../architecture/patterns.md). Build it for the pedia (a SHAPE oracle,
  NOT a coverage oracle — the appendix is enumerable). Read maps: [pedia-map.md](../../reference/pedia-read-map.md) ·
  [python-read-map.md](../../reference/python-read-map.md).

## UnitCombat distillation

> The concept + the target model: [engine.md § UnitCombat](../../reference/engine.md). The MINIMUM that unblocks
> the stuck consumers is the cascade-QUERY surface, not a full re-taxonomy.

⛔ **NOT #430 WORK — the class PURGE and the vs-modifier re-expression are OUT OF SCOPE (owner):** *"they can live
side by side for a good long while, I am not purging unitcombats in the 430 work."* Coexistence is the sanctioned
end state for this rework's duration, not a half-state to close ([engine.md § UnitCombat](../../reference/engine.md)),
and the mapping being additive is what makes it cost nothing. ⚠ Tags taking over the identifier role was the
purge's stated GATE, so meeting that gate reads like a green light — it is not one. Do not open it, and do not
treat [unitcombat-merge-candidates.md](unitcombat-merge-candidates.md) as a live worklist.

- **Reconcile the double flags** — `bSpy` lives on both the unit and the unit-combat; unify onto the `spy` tag,
  same for `outlaw`/criminal.

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
- **MOVEMENT — the SHAPE is converted; the SCALE is not.** `getFlatMovement(MovementKind, CvCascScope)` now serves
  the family on terrain / feature / route and the 24 consumers read it, each reducing `÷100` at its point of use.
  That is behaviour-preserving: every authored value is an exact multiple of 100, and the two float readers are
  PEDIA DISPLAY ONLY — they render a cost, they do not move a unit.

  ⛔ **The real question is that MOVEMENT IS ALREADY A PER-100 VALUE (owner) — `MOVE_DENOMINATOR` is its fixed
  point, and always was.** That is why routes author 5–100: they are already denominator units expressing part
  steps. So the cascade's ×100 sits on top of a denominator the mechanic already had, and the family slot now
  holds **two scales, each ×100'd**: terrain/feature as whole moves (1–6), routes as denominator units (5–100).
  ⛔ Do NOT "finish" this by carrying ×100 deeper into the resolver — that compounds the double-scaling instead
  of resolving it. What has to be decided first is which single denominator movement speaks in, and that is a
  CURATOR question (does terrain author denominator units too?), not a consumer sweep.
  ⚑ Also untouched: `ROUTE_VACTRAIN`'s conditioned `-4 @TECH_SKYROADS` entry is NOT read by the point getter;
  the live equivalent is `CvTeam::getRouteChange`, so consuming it would double-count until that accumulator is cut.
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
