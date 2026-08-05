# #430 — the TODO

> **A LITERAL WORKLIST. What has to be DONE — nothing else** ([DEC-spec-plus-todo](../../architecture/decisions.md#dec-spec-plus-todo)).
>
> ⛔ **NO STATE IN THIS FILE.** No counts, no censuses, no `file:line`, no "measured", no "verified in tree", no
> worked cases, no dispositions, no record of what was achieved. Every one of those DRIFTS, and a drifted claim
> reads as authoritative long after it is fiction — which is how this list started manufacturing work that no
> longer existed. **The DESIGN lives in the specs; the STATE lives in the tree; git history is the record of
> work done.** An item that is finished is DELETED, never ticked, and anything durable it established moves
> into its owning spec first.
>
> ⛔ **The tree is the authority, always.** Before acting on any line here, confirm it against the code
> ([DEC-no-guessing](../../architecture/decisions.md#dec-no-guessing)) — and if it is already done, delete the
> line rather than updating it. Sequencing and governing rulings: [roadmap.md](roadmap.md).

## Blocked on an owner ruling

- Make the `savemigration.txt` parser honour its documented `CUT:` / `RENAME:` prefixes. It changes save-load
  behaviour, so it is an owner call ([save.md §3](../../specs/save.md)).
- Decide whether the `…Times100` on AI unit counts and plot strength is swept with the scale conversion — same
  shape, different nature (fractional SizeMatters counts, not a modifier channel).
- Rule on the river-attack term for a CITY defender (`CvUnit::getDefenderCombatValues`). Its two branches
  disagree: attacking across a river hands an ordinary defender `-RIVER_ATTACK_MODIFIER`, while the city branch
  is `min(0, riverDefensePenalty - RIVER_ATTACK_MODIFIER)` — capped at zero, so a city defender can never
  receive that bonus and the building value only bites if authored below the define. Every authoring is above
  it, so the term is inert whatever its scale. ⛔ Not an agent fix: changing it changes combat, which
  [validation.md](../../specs/validation.md) makes a per-case owner authorization, so the choice (drop the
  mechanic, or correct the branch to reduce the crossing penalty) is yours.
- Build RANKED-UP Size-Matters units from the build menu, so a late-game player is not merging hundreds of
  units by hand — ⚖ **not wanted yet (owner): the task is bigger than it looks, so it waits until we want it.**
  The MODEL is settled and spec'd ([json.md §9](../../specs/json.md)) — `base + x`, the era bounding `x`, and
  the cost as the build-and-merge equivalence — so this closes by IMPLEMENTING that spec, never by re-deciding
  it. Nothing else waits on it and no half of it is in the tree.

## Data — curator

- Mint the `garrison` kind in the culture vocabulary. The curator already emits `culture.unit.garrison.flat`
  and units author it, but no kind exists, so the deposits reach no getter.
- Attach the ruling-16 trigger-plane set (`survivor`, `cityCapture`, `combat.subdueAnimal`,
  `combat.nukeInterception`, `diplomacy.techShare`) to its trigger's `chance`
  ([triggers.md](../../specs/triggers.md)).
  ⚠ `techShare` additionally needs its KIND retired: unlike the rest of the set it was minted as
  `DIPLOMACY_TECH_SHARE`, so the re-home is a curator change AND a vocabulary removal.
- Give the §3.9 entry grammar a payload-less form so a carrier can state a cargo RESTRICTION with no capacity of
  its own, then author the flagged carriers ([modifier.md §6](../../specs/modifier.md)).
  ⚠ Settle in-game first whether the ancient transports are civilians-only; the owner's recollection is explicitly
  unconfirmed and must not be authored against.
  ⚑ The ENGINE half waits on the same thing and is what dangles today: WHAT a carrier may take is the `unit:`
  predicate qualifier on its `cargo.space` entries, so the load/board gates want the QUALIFIED entry read
  evaluated against each cargo candidate — the point sum is the unqualified capacity plane by construction, so
  it cannot answer the restriction.
  ⛔ The PROMOTION-SIDE WIDENING rides the same item and CANNOT be dropped from the engine alone. A promotion
  adds SPACE, never PERMISSION ([modifier.md §6](../../specs/modifier.md) — an owner-ruled intentional
  divergence from legacy), so the per-promotion overrides of WHAT may be carried (domain / special-unit /
  SM-not-special) have no home in the ruled model. But the curator still emits all three, the rebuilt promotion
  info still serves them, and promotions still author them — so this is a coupled CURATOR + INFO + ENGINE
  removal, and it is GATED on the payload-less entry form above: strip the promotion overrides first and a
  carrier whose base capacity is 0 (the whole ancient-navy transport line, and the fighter/missile transports)
  can state no restriction at all and simply stops carrying. ⚠ Do the grammar and the authoring FIRST, then
  remove all three planes together.
- Re-home the remaining `identity` EFFECT keys to the block that already exists for each
  ([json.md §7](../../specs/json.md)): constraints → `requires`/`allowed`; `diploVoteType` → the top-level
  `voteSource` section (and rename the getter off the legacy XML tag); `tradeable` → the `canTrade` block;
  `advancedStart` → resolve the curator's parked
  flag; `pillageGold` → drop.
  ⚠ `espionagePoints` rides the missions/`CvOutcome` carve-out — its channel is settled, only its authoring home waits.
- Bring `curate_trait`'s trade-route rows onto ruling 11, as `curate_building` already is: `iCoastalTradeRoutes`
  and `iForeignTradeRouteModifier` still emit `coastal` / `foreign` as MEMBERS, and neither has a kind in the
  vocabulary — so a trait authoring one emits an address that resolves to nothing and is dropped in silence.
  The ruled shape is the conditioned deposit (the memberless route count gated `HAS_COAST`; the modifier kind
  gated `IS_FOREIGN`) — a WHERE-member is the condition-as-member rollerskate
  ([DEC-conditions-are-predicates](../../architecture/decisions.md#dec-conditions-are-predicates)).
  ⛔ This is LIVE data loss, not latent: traits DO author both tags, and every one of those deposits resolves to
  nothing and is dropped in silence. It closes by fixing the mapping and REGENERATING (`curate_trait.py --write`)
  — traits are no longer content-locked ([modifier.md §4](../../specs/modifier.md)).

- Author the leader→trait assignments. The chain is wired and the slots are authorable; the CONTENT is
  community-owned, so this closes by AUTHORING and never by reconstructing the tables the curator dropped.
- Author per-leader `ai.personality.researchSearchDepth` ([enabler.md §8](../../specs/enabler.md)). Same shape as
  the trait assignments: the read is wired and an unauthored leader takes the default, so this closes by
  AUTHORING. ⚑ It is the beelining dial — a leader that should not commit five techs deep for one unlock is
  expressed HERE, as data, rather than by weakening the enablement valuation for everyone.
- Rule on `MISSION_RANGE_ATTACK` (`canRangeStrike` / `rangeStrike` / `INTERFACEMODE_RANGE_ATTACK`). It is a
  SECOND ranged-attack mechanic, distinct from the removed ranged bombard and sitting beside it in every mission
  switch — so the bombard cut did NOT cover it and must not be assumed to have.
  ⚑ What decides it is already ruled: *"if it uses the ranged attack, and is not an airplane, it goes — vanilla
  airplanes have ranged attack"* ([superseded-ideas](../../architecture/superseded-ideas.md) #24). `canRangeStrike`
  REFUSES `DOMAIN_AIR` outright while running on the air-range/air-combat stats, i.e. it is a non-airplane ranged
  attack — the surviving member of exactly the class ranged bombard was removed for.
- Rename the `dcmFighterEngage` skill and the `DCM_FIGHTER_ENGAGE` global off the mod-provenance prefix. The
  mechanic is the vanilla airplane ranged attack and STAYS; only the name carries `dcm`, and a live mechanic
  wearing a dead plane's prefix is what makes the next sweep mis-scope it
  ([skills.md](../../specs/skills.md)).
- Re-home `stronglyRestricted` to a `requires.build` civ-membership gate, when NPC civilizations are wired.
- Move corp-HQ revenue (`HeadquarterCommerces`) with the corporation rework, and with it the two corp shapes
  no corporation authors: the HQ FREE UNIT — a `grants` payload, so it lands on the trigger plane off the
  headquarters fact ([triggers.md](../../specs/triggers.md)), never as an info getter — and corp-vs-corp
  EXCLUSION, whose home is the §9 `excludes` block ([json.md §9](../../specs/json.md)). Competition currently
  answers from the consumed-bonus overlap alone. ⛔ Neither is machinery to build ahead of data authoring it.
- Retire `DOMAIN_IMMOBILE`. Immobile is not a domain ([json.md §7](../../specs/json.md)) — a domain is the
  MEDIUM a unit operates in. ⚠ UNITS STILL AUTHOR IT, so the data re-authoring comes FIRST; only then the
  consumers: the enum entry, ~21 engine/AI sites, the `CIV4DomainInfos.xml` record, its game text, and a Python
  read. ⚑ It is TERMINAL in the enum (immediately before `NUM_DOMAIN_TYPES`), so removing it shifts no other id
  — which matters because `DomainTypes` crosses the ABI through `DllExport CvUnit::getDomainType()`.
  ⚠ Several sites treat it as a live case rather than a dead one (`isDomain`-style switches, an `FAssert` that
  ACCEPTS it beside `DOMAIN_LAND`), so this is a per-site read, never a delete-the-case sweep.
- Map the flagged unitcombat remainder — map the obvious, flag the unsure, never blunt-purge
  ([unitcombat-tag-mapping.md](unitcombat-tag-mapping.md)).
- Decide what an EMPIRE-scope `range` deposit would mean, if one is ever authored. ⚠ None exists today — every
  authored `range` is unit-scope, and no trait carries a `range` block at all. ⚑ On the aerial line it looks like a duplicate of the
  `air.empire.range` beside it; elsewhere it does not, which is why this is a DATA question and not a reader
  to bolt on ([DEC-no-guessing](../../architecture/decisions.md#dec-no-guessing)).
  ⚠ It closes through `curate_trait` + a regen ([modifier.md §4](../../specs/modifier.md) — traits are no longer
  content-locked), never by hand-editing the emitted JSON, which a regen would overwrite.

## The new surface, wired wrong — fix these first

> These are not legacy and not gaps: they are built machines connected to nothing, or connected wrongly. Each
> compiles, runs, and produces a plausible wrong answer, which is why none of them surfaced on its own.
> ⛔ Symbols, never line numbers — a symbol survives an edit and a line number does not.

- **⛔ CONVERT THE PACKAGES TO THE MAINTAINED SUM — the THREE PLANES**
  ([DEC-maintained-sum](../../architecture/decisions.md#dec-maintained-sum);
  [state-repositories.md](../../architecture/state-repositories.md) § THE MAINTAINED SUM; the retired protocol is
  [superseded-ideas](../../architecture/superseded-ideas.md) #30). The addressing half already exists —
  `DepositIndex::depositsFor` IS "the fact names the source, the index names its deposits" — so what is missing is
  the WRITER and two of the three ROUTES.
  **⛔ CUT FIRST, THEN FILL — do NOT build to keep the current numbers alive.** `CascadeGather::refresh*` is a
  whole-registry sweep (it asks every building / tech / civic / trait / bonus / religion / corporation whether the
  owner has it, per mark), which contradicts the spec, so it is cut EARLY and the packages bind CLEAN. Every
  deposit without a route then reads **ZERO, visibly**, and that census is what drives the rest of this item.
  `gather*Into` survives as the ORACLE and nothing else.
  1. **`CvCascadePackage::apply(channel, unit, ±value)`** — a pure add. Today the only writer into a slot is the
     gather's zero-then-refold; `sourceFlat`/`sourcePercent`/`sourceSum` are rebuild-path READS, not writers.
  2. **Plane A — the SOURCE route.** `±value` on the source arriving or leaving. ⚑ There is no withdrawal input to
     audit any more: every fact's direction IS its id, so the route is picked by which event arrived and the payload
     is read only for HOW MANY ([DEC-facts-name-happenings](../../architecture/decisions.md#dec-facts-name-happenings);
     [event-spine.md](../../specs/event-spine.md)). ⛔ So a consumer must never re-derive a direction from a payload
     sign, an old-vs-new id pair or a presence bool — those conventions are gone from the surface, and reintroducing
     one at a consumer rebuilds the branch the split deleted.
  3. **Plane B — the COUNT route.** A `count-key → the deposits it scales` reverse index off the compiled deposit
     index, so a `ContextDict::add(id, ±1)` applies `±value × Δcount` to every deposit scaled on it whose SOURCE is
     live at that owner (an O(1) `has()` test — this is what keeps the two arrival orders convergent).
  4. **Plane C — the ATOM route.** `±value` on a condition atom's verdict crossing, over the deposits that atom
     gates. ⛔ **B and C land TOGETHER or neither** — C is delta-able only because B guarantees no count moves
     unannounced ([DEC-maintained-sum](../../architecture/decisions.md#dec-maintained-sum)).
  5. **Retire the staleness protocol** — the mask derivation, the banked-marks bracket, and `CvCascadePackage`'s
     `CvDerivedCacheSet` member.
  ⚠ **THE RIDER THE CUT DROPPED, owed by the apply path.** The four `refreshCascadePackage` delegates carried a
  side effect past their one-line delegation: a MAINTENANCE channel moving at city / empire / team scope also
  moved the empire's maintenance TOTAL. That obligation is real ([save.md §6](../../specs/save.md): audit a
  deleted body for riders) and it is exactly "ONE EVENT REACHES BOTH LEVELS" — an apply into a maintenance
  channel must also apply to the empire's maintenance RECEIVER SUM. ⛔ It does NOT come back as a hand-named
  cache: the empire total is a receiver SLOT in the player's own package
  ([state-repositories.md](../../architecture/state-repositories.md) § THE CROSS-SCOPE RECEIVER), which is also
  what retires `markMaintenanceDirty` and its banned spelling.
  ⛔ **Build NO per-source decomposition plane and NO upward push** — the receiver re-sums its participating
  members, and the summing is trivial enough that avoiding it costs more than it saves
  ([state-repositories.md](../../architecture/state-repositories.md) § THE CROSS-SCOPE RECEIVER). ⚠ That is a
  measurement question if it ever reopens, never an argument: a turn-time cost on the standing save, attributed
  to the summing.
  ⚑ **Start in the corner that is the PLOT package**: the smallest channel set, no percent side (the origin rule),
  five sources that all announce with old values, no receiver sums, and it exercises the `substrateFlat` segment.
  Then city, then empire/team.
- **⛔ SWEEP THE BANNED TERM OUT OF THE CODE** ([DEC-no-staleness-vocabulary](../../architecture/decisions.md#dec-no-staleness-vocabulary)).
  KEEP only the graphics/interface repaint vocabulary the closed EXE needs and BUG resolves by name
  (`InterfaceDirtyBits` and the `setDirty`/`setLayoutDirty`/`setFlagDirty`/`setInfoDirty` helpers over it).
  Everything derived-state goes with its mechanism: `CvDerivedCacheSet`'s `markDirty`/`isDirty`/`markAllDirty`,
  `markMaintenanceDirty`, `setCommerceDirty`, and the AI re-evaluation flags `AI_setAssignWorkDirty` /
  `AI_makeAssignWorkDirty` / `AI_setChooseProductionDirty`. ⛔ A surviving derived-state site gets NO synonym —
  name it for the job it does, or delete it with the mechanism.
- **⛔ Route every domain through `EnablerKernel::gateSet` — the declared GENERATE→GATE primitive has ZERO
  callers while SIX file-static copies of it exist**, one per domain (`bd_`/`bl_`/`ce_`/`pc_`/`pj_`/`ud_gateSet`).
  The kernel's own header calls these "the single-implementation enabler primitives"; the gate ORDER
  (obsolete-check → `requires` → `allowed` cap) now lives in six independent bodies free to drift apart
  ([DEC-single-implementation](../../architecture/decisions.md#dec-single-implementation)). This is the
  reinvented-machine signature at the centre of the enabler.
- **Nothing can enumerate the GREYED tier.** `EnablerDomain::inTreeIds` (LISTED + GREYED — the visible tri-state)
  has no caller, while its sibling `listedIds` has many. The "go get copper / adopt this civic" greyed build-list
  entries that [enabler.md §6](../../specs/enabler.md) specifies cannot be rendered by anyone.
- **The enabler's validation-oracle surface is unrunnable** — `BuildingEnabler::verifyCity`, `TechEnabler::available`
  and `UnitEnabler::explain` all have zero callers, and no route reaches them. ⛔ This is the load-bearing one:
  `CvTechEnabler`'s header states the design contract that the enabler consumes ONLY events *precisely so* a
  missed emit surfaces as a visibly wrong enabler, with the oracle diff as the tripwire. **The event-only design
  is resting on a tripwire nothing pulls.**
- **Emit the load-pipeline diagnostic.** `emitLoadPipeline` is the ONE spine endpoint with no emitter anywhere —
  the kind, the renderer and the field decode are all built. So load-stage timings, fixpoint pass/flip/converge
  counts and the verify-catch count reach neither the log nor `/events`, and load time is the currency that pays
  for turn time ([DEC-turn-time-is-king](../../architecture/decisions.md#dec-turn-time-is-king)).
- **Consult `EnablerKernel::cityHasVicinityBonus` instead of re-deriving it.** Its header names it the ONE home
  for "is this bonus in vicinity here?"; it has no caller, and the evaluator re-implements the same two-half union
  inline. Behaviour agrees today — the divergence risk is structural.
- **Wire the `InfoValuation` fold seams or delete them**: `tradeRouteChannelYield`, `combinedGroupSum` and
  `netUpkeepAfterFree` are the declared canonical math "so the package rebuild and the `expected*` endpoints call
  the SAME math", and all three have no caller while the arithmetic is hand-written at the consumers. ⚑ Knock-on:
  the combine-floor metadata table is reachable ONLY from the dead `combinedGroupSum`, so adding a floored
  (family, kind) row today would have zero effect, silently.
- **Wire `DepositIndex::segIdFor*`** — a five-entry string-elimination optimizer whose header says it "kills all
  string handling in the per-plot keyed walks". It has no caller and the per-read `lookupSegment(std::string(...))`
  it was built to remove is still on the per-plot turn path.
- **Give `ev_countCore` a `TAG_` branch** so `CvCascadeTally::countUnitsWithTag` is reachable. The count-atom
  dispatcher branches on specialist/building/tech/unit only, so an authored `{TAG_X, min:N}` count silently falls
  through to presence and answers 0/1. ⚠ Latent — no data authors one yet.
- **Wire the `EnablerOverlay` DELTA reads** (`unlocks` / `unlockedIds`) or delete them. The overlay's `addHave`
  half is live in the civic what-if; the delta half — the header's "deliberately the primitive rather than
  something each caller re-derives" — has no consumer because the tech-picking one does not exist yet.
- **Fix the stale comment pointing at `CvPlot::setWorkingCity`** on the working-city event — no such method
  exists (only the override variant), and naming a dead site is the bait
  ([DEC-no-rollerskate-evidence](../../architecture/decisions.md#dec-no-rollerskate-evidence)).
- **Wire the operating-building seed and its maintenance hooks.** `EnablerKernel::seedOperatingBuildings` has no
  caller, and the caller its own comment names — `CvCity::refreshOperatingBuildings` — does not exist. The six
  `on*Active` hooks are equally unreached. `operatingBuildings()` is a bare fetch by ruling, so nothing rescues
  it: the set is empty for every city, so every building reads DORMANT, `wireOperatingBuildings` fills the eval
  ctx with empty active/provided sets, `cityHasVicinityBonus` answers false, and the trigger plane grants
  nothing. ⚑ Diff `/computed/enabler/operating/{stored,oracle}` to confirm — that pair exists for exactly this.
- **⛔ Stop reading a NEGATIVE band bound as "no bound" — this is the landmine directly behind the item above.**
  `CvJsonConditionParse` defaults an unauthored `min`/`max` to **-1**, and the PROPERTY band atom in
  `CvConditionEval` then treats *any* negative as absent (`a->min < 0 || …`). Its own comment four lines up says a
  `PROPERTY_*` value can be legitimately negative, so the parse sentinel and the atom contradict each other. Every
  `PROPERTY_EDUCATION` low-education tier is authored with a negative min **and** max, so BOTH bounds are dropped
  and the band clause is unconditionally true; the positive-side bands are unaffected, which is exactly why it
  reads as working. Those tiers are `notConstructible`, so `CvCity::placeSystemBuildings` puts them in every city
  at founding, and the `dormant` successor ladder then leaves the DEEPEST tier — the one with no successors —
  permanently active regardless of the city's real education. ⚑ The seed item above is what hides it — deposits
  only flow for buildings in the operating set. **Fix both in the same change, or wiring the seed lights up a
  crippling city-wide penalty and takes the blame for it.** The fix is a real absent-sentinel (a has-bound flag,
  or `INT_MIN`/`INT_MAX`), never a `< 0` test — see [enabler.md](../../specs/enabler.md) §3.
- **Delete `CvPropertyInfo::getPropertyBuildings`, `m_aPropertyBuildings` and their CURATOR-GAP comment.** Nothing
  reads the getter, and the comment promises a resolution the band model supersedes
  ([enabler.md](../../specs/enabler.md) §3).
- **Give `CityContext::amenities` a load path and a working removal.** Its feeder is the operate crossing, which
  the dead seed above is what fires on load — so the fold is empty after every load and `isGovernmentCenter`,
  `getPowerCount`, `isNoUnhappiness` and the health flags all read false. The removal leg is gated on the same
  empty operating set, so amenities never decrement while additions fire: the dictionary grows monotonically for
  the life of a game.
  ⛔ **The building leg RIDES THE ENABLER'S CROSSING, and that is correct — do not "fix" it by moving the leg onto
  a fact the city emits from its own read.** The enabler is a SOURCE OF FACTS, never the home of this answer, and
  the dictionary is the final stopping place ([contexts.md](../../architecture/contexts.md)). The ordering ban
  applies to a store that RE-DERIVES by reading another system's built set; a store that CONSUMES a delta has no
  such dependency and builds identically whenever the facts arrive.
- **Give the vicinity store its `CASC_PRED_*`-keyed twin** — the vicinity counterpart of `plotAttrs` (river / coast
  / hills / peak / fresh water over the radius tiles), beside the `BONUS_*`-keyed one that now exists
  ([contexts.md](../../architecture/contexts.md), owner). ⛔ The two are NOT merged: the key spaces are disjoint
  registries both starting at 0, so one store re-opens the cross-registry id collision the `CLS_` prefix closed
  (`ContextDict.h`). Nothing reads it yet, so it lands when a consumer wants it.
- **Collapse the per-dictionary plumbing into one mechanism** — owner resolution, dispatch, consumer class and
  registration are hand-written per family.
- **Retire `CvDerivedCache` by emptying it, one tenant at a time**
  ([DEC-contextdict-replaces-derivedcache](../../architecture/decisions.md#dec-contextdict-replaces-derivedcache)).
- **Emit the RECEIVED line** so the whole event flow is auditable live off `/events`
  ([event-spine.md](../../specs/event-spine.md) § THE RECEIVED LINE): a consumer announces that it ACTED on a
  fact, and a DOMAIN fact with no matching received line names a missing consumer route — the one gap form with
  no other observable signature. ⛔ `DIAGNOSTIC`, never `DOMAIN`.
- **Collapse the TWO disjoint segment interners** — `modSegmentIntern`/`s_modSegments` and
  `DepositIndex::internSegment`/`s_segs` intern the same concept in different id-spaces with different slot
  counts, only one carrying spell-back, and `di_pushFamilies` round-trips ints → string → ints between them
  ([DEC-single-implementation](../../architecture/decisions.md#dec-single-implementation)).
  ⚠ **Neither is the SEGMENT of "a package is made up of its segments"** — those two intern ADDRESS FRAGMENTS,
  while a package segment is a named SLICE of a channel total (`CvCascadePackage::substrateFlat` is the one
  realized instance). One word, two unrelated concepts; do not converge them by name.
- **Delete `te_applyDelta`'s hand-rolled copy of the membership fold** and route the TECH domain through
  `EnablerKernel::applyEdges` like every other domain — techs are the largest HAVE axis, so the one domain that
  bypasses the single writer is the one that matters most
  ([DEC-single-implementation](../../architecture/decisions.md#dec-single-implementation)).
- **Clamp the enabler's membership refcounts, or make the guard survive the shipped build.** The negative-refcount
  `FAssertMsg` compiles out of `Release`/`FinalRelease` (`FASSERT_ENABLE` is Assert/Debug/Testing only) and
  neither changer clamps, so in a shipping build an over-release leaves the enable count negative and the next
  legitimate acquisition lands at zero — silently absenting the candidate.
- **Delete `CvCity::doVicinityBonus`** — a per-turn blanket clear plus lazy recompute-on-read beside the
  event-built vicinity store ([DEC-no-self-heal](../../architecture/decisions.md#dec-no-self-heal)). Nothing is
  missing behind it: the maintained store already exists, and this cache's own comment claims a reader that has none.
- **Repair the `savemigration.txt` obligations naming `CascadeWellbeing`** — that class lives only in
  `SourceArchive/`, so those cut fields have no source at all ([save.md §3](../../specs/save.md)).
- **Sweep the writerless serialized accumulators** by the two-grep test: serialized and read, with no caller on
  the changer. Beyond the two already named, `CvPlayer::changeHappyPerMilitaryUnit` and
  `CvCity::changeImprovementFreeSpecialists` are the same shape.
- **Stop handing `CvPlot*` out of `CityContext`** (`cityPlot`, `radiusPlot`) — the evaluator uses that hole to
  reach a second context, while the eval ctx already carries one. The isolation is meant to be structural.
- **De-instantiate `CvCascadeTally`** — a calculator is a static-methods holder, never an instance
  ([DEC-single-implementation](../../architecture/decisions.md#dec-single-implementation)).
- **Reset the enabler's build-once latches**, or make them re-entrant: `rj_clearAllRepos` re-maps every info on
  the postmenu pass while `buildActiveIndex`, `bd_buildGateClasses`, `ud_buildClasses`, `bd_sbMembers` and
  `bd_cappedBuildings` latch permanently. `di_ensureDependencies` is a literal ensure-on-read behind four reads.
- **Collapse the second condition-evaluation surface.** `CvPropertyBridge` translates the cascade's own
  `CvCondition` trees back into `BoolExpr` for the legacy solver to evaluate, with different semantics — count
  thresholds and connections drop silently.

## ⛔ THE PROPERTY-BAND ECOSYSTEM IS BROKEN FOUR INDEPENDENT WAYS

> The crime / disease / pollution / education building ecosystem — the largest `requires.operate` population in
> the data — is severed at four points at once. Fix them together; any one alone leaves it dead, and fixing the
> seed alone lights up the negative-band defect (below) as a crippling city-wide penalty.

- **`SEVT_PROPERTY_ADDED / _REMOVED` is emitted into the void.** The fact fires from the `CvProperties` mutation choke
  points, but repo-wide the id exists ONLY as an enum entry, a log spelling, and the emitter — **no consumer
  carries a case for it**. So a property value moving never re-checks the bands it crosses.
- **The receiving machine is built and has zero callers**: `EnablerKernel::onPropertyBandHitActive` and
  `propertyBandThresholds` (the threshold union the watermark was to read) are reached by nobody, and the
  `s_operateNeedsLiveState` bucket that would otherwise catch these is a DEAD STORE — pushed to at load, never
  read. The kernel opted out of the dynamic path in a comment promising "the watermark emits a targeted band-hit
  on a threshold crossing". The watermark does not exist.
  ⚑ **The un-announced fact is: "property P crossed one of its registered band boundaries in city C."** The
  threshold table is already built; emit the crossing and route it — do not add a per-turn re-scan
  ([DEC-no-self-heal](../../architecture/decisions.md#dec-no-self-heal)).
- ⚠ **It fails INVISIBLY, not cleanly.** The re-check re-evaluates the WHOLE operate condition for whatever
  buildings some other event happens to seed — so a crime building that also requires a tech gets its crime band
  re-read incidentally the next time any tech lands. The verdict is right at unpredictable moments and wrong in
  between, per city.
- **A second band on the same property in one `requires.operate` tree silently overwrites the first** — the band
  map is filled by assignment, not accumulation.
- **`-1` as "no bound" recurs in the band THRESHOLD table**, same shape as the atom's `< 0` test. Fix both with
  one real absent marker (a has-bound flag, or the `MIN_INT` shape already used correctly for build-year).

## ⛔ SHAPE VIOLATIONS — the bespoke thing forcing bespoke machinery

- **⛔ The ONE condition evaluator dispatches on RUNTIME STRINGS.** `CvCondition` keeps `type`/`param` as
  `std::string` into the runtime, and the evaluator routes every atom through prefix compares — dozens of them in
  one file, on both the package-rebuild path and the per-decision `expected*` read.
  ⚑ **This project already MEASURED this exact defect and fixed it elsewhere**: `CvCapabilities` records that a
  per-call `std::string` construction on the pathfinder "4x'd the turn", and precomputes flags for it. The same
  shape survives in the evaluator. `CvCondition` already carries the FK-resolved `id` and a `predKind` — route on
  an interned kind, and let no string survive load
  ([DEC-materialize-at-mapfrom](../../architecture/decisions.md#dec-materialize-at-mapfrom),
  [DEC-one-json-reader](../../architecture/decisions.md#dec-one-json-reader)).
- **⛔ `EnAllowedCap` fuses KIND × SCOPE into one enum, and it has already produced a WRONG GATE.** Two ladders
  collapse the team and empire arms into one branch, so a project's `empire:N` cap is enforced against a TEAM
  count while a tech's `team:N` cap is enforced against an EMPIRE count. That is
  [DEC-scope-is-an-axis](../../architecture/decisions.md#dec-scope-is-an-axis) failing as a live behavioural
  defect, not a naming preference. Re-cut as `cap(ALLOWEDCAP_SELF, <scope>)` — the tally already takes a scope, so
  all four hand-written kind→scope ladders delete.
- **⛔ Delete `CvDerivedData` — it is a REVIVED KILLED IDEA.** `superseded-ideas.md` #1 records the derived-data
  repository (`TLazy`/version/staleness aggregation) as killed with an explicit *"don't revive the repository"*, and
  the file is back: four empty repositories, members on four game objects, zero tenants, and doc-comments
  teaching ensure-on-read plus a "bounded staleness" periodic rebuild as the sanctioned architecture. It also
  cites a plan doc that does not exist. ⚑ It executes nothing, which is exactly why it is dangerous — it is what
  the next agent reads when asking how derived caches work here.
- **The city's vicinity fact is derived THREE times.** `CityContext` stores it properly, while `CvCity` keeps a
  hand-rolled ensure-on-read memo gated on a multiplayer option AND a per-turn blanket wipe over every bonus in
  every city — under a comment claiming no stored context copy exists, which the live `CityContext` dictionaries
  refute. ⚠ The `had*VicinityBonus` arrays it feeds are SERIALIZED derived state
  ([DEC-derived-never-trusted](../../architecture/decisions.md#dec-derived-never-trusted)) and have **zero**
  readers in the whole tree — a per-turn radius scan and a save payload feeding nothing.
- **Give `capabilities` its generated classification ids.** The team caps carry three runtime
  `std::set<std::string>` rebuilt by heap-copying strings per team per mark, to serve three accessors with zero
  call sites — while the string-keyed source getters ARE called ~80 times from AI diplomacy with string literals.
  [DEC-classification-infos](../../architecture/decisions.md#dec-classification-infos) makes the union a bitset OR
  and deletes the key table, the three sets and the flag mirror together.
- **Materialize the remaining per-call string reads in info getters** — the hide-and-seek detection row resolving
  an infotype by string at read time (on every visibility check), the deposit-address `lookupSegment` built fresh
  at its call site (the DEC's own worked example, reconstructed literally), and the trigger-promotion scans
  comparing `happening` strings per call.
- **Retire the `Global*` / `National*` scope fragments in names** — including newly-written `mapFrom` code that
  had the scope in hand as a parameter and threw it into the member name instead.
- **Re-home the condition-as-kind survivors** — kinds that answer WHERE/WHEN rather than WHAT (in-border
  experience, the three territory heal kinds, hills work-rate, the holy-city and NPC-peace scalars). Each has an
  existing predicate, and json.md rules a hill is `plots {HAS_HILLS}`, never its own kind.
- **Kill the `getX`/`getX100` pairs** — the maintenance getter reduces `Times100` internally, which is the exact
  pair [DEC-fixedpoint-x100](../../architecture/decisions.md#dec-fixedpoint-x100) forbids, plus the saved-maintenance
  and unit-upkeep twins.
- **Rename or delete `clearCanConstructCache`** — both overloads ignore their parameters, have identical bodies,
  and say in their own text that there is no cache to clear. A name advertising a dead cache is the bait.
- **Spell out the bare single-letter identifiers** in the enabler/capability code (`j` for the info, `c`, `r`,
  `s`, `sb`, `jg`, `a`, `mem`, `wcap`). Per Sources/AGENTS.md this is a review-blocker on sight, and it is NOT
  the sanctioned exception — that covers a file-anchored PREFIX, never a bare parameter name.

## ⛔ COMMENTS THAT CONTRADICT THEIR OWN CODE

> The highest-signal rollerskate: the right intent written down, something else implemented, and the comment
> reassuring every subsequent reader. ⚑ Two cite a `DEC-*` id that does not exist in the ledger at all.

- **A phantom `DEC-json-not-cascade` is cited in seven places and defined nowhere.** A second citation invokes
  `DEC-mirror-then-redesign`, which is not a ruling either — it is in `superseded-ideas.md` as DEAD, under
  "never re-argue that a shape must be preserved because it is what the engine does today". Both are load-bearing
  justifications resting on authority that does not exist.
- **A false "verified" claim, refuted 35 lines below it in the same file** — the reverse pass asserts no
  corporation headquarters registry exists and nothing asks for one; the registry, its feeder and four consumers
  are all there. ⛔ The word "verified" is what pre-empts the check, so an agent trusting it builds a second
  registry.
- **A cached-read contract on an uncached read** — the city maintenance getter's header promises "a BARE FETCH of
  the derived cache — never a gate test, never a recompute", while the body gates and loops every kind through
  the cross-scope legs, and concedes in its own text that nothing is cached. It misdirects the turn-time hunt.
- **A live enum documented as dead** — the GREYED tri-state is marked "unused until it lands" while it is
  assigned and wired across eight domains, with the same stale claim repeated on two sibling headers.
- **A component and a hook that never existed** — the enabler header attributes trait maintenance to a
  `TraitEnabler` with an `onTraitChanged` hook; neither has ever existed in the tree. Every sibling line around it
  resolves, which is what makes it invisible.
- **The evaluator is described as a class in three files**; it is a free function. And the promotion
  negative-effects derivation is blocked by a comment saying it "waits on the firstStrike.chance vocabulary row"
  — that row is live and already read elsewhere. Only the comment blocks it.
- **The likely SEED of the ×100 cluster**: the JSON parse header claims "a BLANKET ×100 at every magnitude leaf".
  It is not blanket — percent leaves are skipped. Fix this one first; it is what the next agent reads before
  writing another divide.

## ⛔ SELF-HEAL FOSSILS — each one is a missing emit wearing a per-turn sweep

- **A negative empire commerce total is rewritten to ~2 billion.** A legacy wrapped-int "false accumulate"
  detector (`< -9999` → `MAX_COMMERCE_RATE_VALUE`) survives on sums that are now accumulated in `int64_t` and
  cannot wrap — so the only thing it still discriminates is a GENUINELY negative total, which the realized city
  rate can legitimately produce (neither `cityRate` nor `commerceSplit` floors the flat/deposit legs). When it
  fires, the AI's production and tech weighting, the demographics history, and the Python surface all read
  positive two billion. Delete the detector; it can no longer detect what it was for.
- **The whole map's visibility is wiped and re-applied EVERY TURN** in `CvGame::doTurn`, under a comment that
  says outright it is "a stickytape - can't find where it's skewing visibility counts". ⚑ The comment names the
  fix: an unpaired visibility increment/decrement leaves the per-plot counter outside its stated 0–1 invariant.
  Find the unpaired mutation; the sweep then deletes itself. Until then it costs a full-map clear plus a whole-map
  re-sight per turn ([DEC-turn-time-is-king](../../architecture/decisions.md#dec-turn-time-is-king)).
- **The empire commerce total is an `ensure()`-on-read behind a `MAX_INT` sentinel, cleared by a per-turn
  blanket** — the tombstoned read-side rebuild, a hand-named scalar array, and a blanket standing in for the
  missing mark, all on one value, with the comment citing `state-repositories.md` while implementing what it bans.
  Wire the mark that a member city's realized commerce moved to also mark the empire's receiver slot. ⚠ Its
  sibling total-yield read has NO cache at all and re-walks every city per call, inside a players×yields double
  loop.
- **Delete `CvDerivedData` — a rival cache framework with zero tenants.** `TLazy<>` is declared on four owners and
  `reset()` on all four, and not one datum has ever been declared in it. ⛔ It is a rollerskate GENERATOR: it is
  what the next agent finds when asking how derived caches work here, and it documents recompute-on-read plus a
  "bounded staleness" periodic rebuild as the sanctioned architecture — exactly what
  [DEC-no-self-heal](../../architecture/decisions.md#dec-no-self-heal) and
  [DEC-uniform-cache-shape](../../architecture/decisions.md#dec-uniform-cache-shape) refuse. It also points twice
  at a plan doc that does not exist.
- **The per-turn victory-city recount** wipes and refills two hand-named scalars whose only inputs are victory
  validity and immutable info data. Cheap, same shape, same missing emit.
- **Owner call needed on the AI turn-scoped memo clears** (tech values, mission targets, civic values, build
  values, unit counts, trade routes, resource consumption). They memoize AI *valuations* rather than derived game
  state, so whether the no-self-heal rule binds them is a ruling, not an agent's call. ⚑ Start with the mission
  target cache — its own comment says it is force-recalculated "for reliabilty reasons (more robust to bugs)".

## ⛔ VALUE CORRUPTION — the ×100 cluster's REMAINDER

> ⚑ The contract, so it is never re-guessed: `mod_valueForUnit` returns a `CASC_UNIT_PERCENT` read as a PLAIN
> HUMAN PERCENT; a FLAT is ×100 and reduces at its point of use. **Ask the KIND's unit, never the family's** — a
> family-wide blanket on a per-kind-split family produced every defect in this cluster
> ([DEC-fixedpoint-x100](../../architecture/decisions.md#dec-fixedpoint-x100)).

## ⛔ DOUBLE-COUNTED VALUES — the cascade folds it, then legacy adds it again

> One shape: a `process*` function reads a rebuilt info's compiled deposits and pushes them into the legacy
> accumulator the cascade replaced — while the gather folds the SAME entries into the scope package, and one
> consumer sums both. In three of four an in-tree comment asserts the legacy leg is sanctioned, which is exactly
> why they survived ([DEC-accumulator-cut-uniform](../../architecture/decisions.md#dec-accumulator-cut-uniform)).

- **City over-limit anger — counted twice, and the magnitude is wrong.** The ruling is written FIVE LINES ABOVE
  the feeder: the over-limit member is a PRESENCE COUNT and "the anger MAGNITUDE now lives in the cascade's
  CITY_LIMIT `per.above` deposits, never here." Every other consumer obeys it (`==0` / `>0` / `<1`). The
  wellbeing verdict multiplies the presence count as a per-city anger amount, on top of the cascade's own
  deposit — as does the UI attribution. Serve it from the wellbeing read alone.
- **Tech health and happiness — counted twice behind a camouflaging comment.** `processTech` pushes the tech's
  compiled empire-scope wellbeing into `m_iExtraHealth`/`m_iExtraHappiness`, which the city then adds on top of
  the cascade fold of the same entries. ⛔ The comment calling these "genuine one-shot event state" is TRUE of the
  random-event feeders (the owner-ruled carve-out) and FALSE since `processTech` started writing the same member.
  Cut the `processTech` lines; keep the member for the event grants.
- **Trade routes — counted twice, and one leg is at the wrong scope.** `m_iTradeRoutes` is fed from building
  deposits at empire scope AND from tech deposits read at CITY scope into an empire accumulator, while the city
  read already rolls team+empire+city. Serve from the cascade read alone and soft-remove the member.
- **Building-keyed happiness — the reverse pass lands it, then `processBuilding` lands it again.** The keyed
  happiness deposit is reverse-landed onto the TARGET building and folded with that building's own output; the
  player-side keyed accumulator adds it a second time. ⛔ **Do NOT cut the neighbouring
  `changeBuildingProductionModifier` on the same logic** — `buildRate` is excluded from the output-channel
  landing and keyed entries are skipped by the fold outside plot scope, so its keyed accumulator is its only home.

## ⛔ A LEGACY PLANE ALIVE ONLY FOR PYTHON — and a value silently lost

- **Random-event yield modifiers reach nothing.** `CvCity`'s `m_aiYieldRateModifier` is written ONLY by random
  events and read ONLY by the `Cy*` binding — `getBaseYieldRateModifier` is a pure cascade read that never
  consults it. So event yield modifiers are stored, serialized, shown to Python, and have ZERO gameplay effect.
  This is legacy-left-breathing masking a real functional loss, not merely a duplicate.
- **The player twin is a serialized plane maintained solely to feed a binding.** `CvPlayer`'s
  `m_aiYieldRateModifier` is fed from building deposits and has no engine consumer at all — its only reader is
  `CyPlayer`. Exactly the shape [DEC-cy-not-fixed](../../architecture/decisions.md#dec-cy-not-fixed) and
  [DEC-new-getter-surface](../../architecture/decisions.md#dec-new-getter-surface) ban.
- **Cascade computes it, legacy is what consumers read** — the cascade slot beside each is dead and therefore
  unverified: the space-production modifier, the team enemy-war-weariness modifier (whose own comment concedes it
  is "what the legacy accumulator held"), the city production-to-commerce modifier, and the building commerce
  change.
- **Rule on the revolution mirror.** It is plausibly the sanctioned Python-authoritative mirror, but one leg
  reads CITY scope into an empire accumulator and, unlike every sibling, applies no `/100`. Even if the mirror
  stays, that leg is wrong.

## Rollerskates — the abandoned path, still in the tree

> Evidence of a path someone tried and left behind
> ([DEC-no-rollerskate-evidence](../../architecture/decisions.md#dec-no-rollerskate-evidence)). It holds the NAMES
> of dead things, which is what sends the next agent re-treading them — and being preprocessor-skipped or
> commented, none of it is visible to the compiler census.

- **Delete the `#ifdef` attic.** Each guard below is defined NOWHERE — not in `Sources/`, not in `fbuild.bff`, and
  not even as a commented-out `#define` — so no switch ever existed and the block is an abandoned alternate:
  `USE_BOTH_TECHBUILDING_EVALUATIONS` (a whole second, older tech-building valuation parked inside
  `AI_techBuildingValue` — the very function the enablement-valuation ruling governs), `USE_OLD_PATH_GENERATOR`
  (the old pathfinder, across `CvUnitAI`/`CvGameCoreUtils`/`CvSelectionGroup`/`CvUnit`, with live `#else`
  branches), `VALIDITY_CHECK_NEW_ATTACK_SEARCH` (a migration-era harness that re-runs the old brute-force search
  and diffs it), `TEMP_DEBUGGING_SUPPORT` (a parked `StreamWrapper : FDataStreamBase`), `EXTREME_PAGING`,
  `EXPERIMENTAL_FEATURE_ON_PEAK`, `DEBUG_TECH_CHOICES`.
  ⛔ **Do NOT sweep the OFF-SWITCHES with them.** A guard that HAS a commented-out `#define` is un-killed forward
  intent and the disposition is the owner's ([DEC-keep-unkilled-ideas](../../architecture/decisions.md#dec-keep-unkilled-ideas)):
  `ENABLE_FOGWAR_DECAY`, `USE_MEMMANAGER`, `GLOBAL_WARMING`, `THE_GREAT_WALL`, `USE_INTERNAL_PROFILER`,
  `NO_RANDOM`, `VALIDATION_FOR_PLOT_GROUPS`, `VERIFY_CAN_BUILD_CACHE_RESULTS`, `VERIFY_PLOT_DANGER_CACHE_RESULTS`,
  `VERIFY_YIELD_CACHE_RESULTS`, `DYNAMIC_PATH_STRUCTURE_VALIDATION`, `LIGHT_VALIDATION`. The mechanical test is
  the commented `#define`, never the guard's name.
- **Record WHY each off-switch is off, in its subsystem's reference doc.** Only `ENABLE_FOGWAR_DECAY` has its
  reason written down; the rest do not, and an unexplained off-switch is what the next sweep eats.
- **Strip the comment trails that name a DEAD symbol.** Keep the forward statement, delete the dead name — naming
  it is the bait. Worst offenders: `CvCity`
  (narrates a removed generic-citizen loop AND spells out how to revive it), `CvUnitAI` (`AI_bestCityBuild`
  "retained for any external callers but no longer used"), `CvPlayerAI` ×3, `CyTeam`, `CvPlayer`,
  `CvTriggerEngine`, `CvHttpServer`.
- **Rewrite the `CyGameTextMgr` half-state comment** — it says the composer bodies "were cut and are being
  rebuilt, so a method here answers empty until its composer lands." A doc/comment describing a half-state reads
  as a sanctioned shape; state what IS, and put the missing composer in this list instead.
- **Sweep the commented-out code**, worst in `CvUnitAI`, `CvPlayerAI`, `CvUnit`, `CvPlayer`. ⚑ Start with
  `CvPlayerAI`'s commented-out `AI_getHealthWeight` + its `getAdditionalHealthByCivic` calls — it sits directly on
  the wellbeing surface just cut, so it is the segment most likely to send someone after a getter that is gone.
  Two entire commented-out `AI_Potential*` functions kept "just in case" are the other exemplar. Most of the bulk
  is inherited C2C rot rather than #430 residue, so it is its own pass.

## Whole-database scans where the forward edge already answers

> [DEC-one-reverse-view](../../architecture/decisions.md#dec-one-reverse-view) + [enabler.md §6](../../specs/enabler.md):
> ask the edge or the maintained frontier, never the registry. These are RESIDUE — in every case the same file
> already does it right somewhere else, which is the tell that the conversion stopped short rather than skipped.

- **`CvUnit::canAcquirePromotionAny` sweeps every promotion, on the hottest path there is.** `testPromotionReady`
  calls it TWICE (the second is redundant once the first sets the flag), and it fires on every XP change, every
  unit produced, and from a dozen other sites. ⚑ `CvUnitAI::AI_promote` sitting beside it already iterates the
  player's maintained UNLOCKED-promotion set and says so in its own comment — then its entry guard
  `isPromotionReady` sweeps the database anyway. Convert the guard onto `getUnlockedPromotions`.
- **`CvTeam::processTech` asks four questions BACKWARDS** — `ObsoletePromotions` and `ObsoleteCorporations` scan
  every promotion/corporation testing `getObsoleteTech()`, and two more scans test every build's `getTechPrereq`
  and every improvement's `getPrereqTech`. All four answers are compiled onto the TECH
  (`EDGEF_OBSOLETES`/`EDGEF_ENABLES`/`EDGEF_RELATED`) and are already read forward from `CvPlayerAI`.
  ⚑ The tell: the SAME function already reads `enables.specialBuildings` off the tech's own edge a few lines
  below, citing the ruling.

## Legacy still breathing — delete it

> The standing rule (purge violently; blast radius is the signal; the worst offenders are the ones OFF the core
> loop) is [roadmap.md § LEGACY STILL BREATHING](roadmap.md). ⚠ KNOWN-INCOMPLETE — legacy found anywhere else is
> killed on the same terms. ⛔ Never record a found legacy surface as acceptable or "kept until X".

- Cut the PLAYER's trade-route count accumulator — it DOUBLE-COUNTS every empire-scope route deposit today.
  Its feeders read the building / trait / tech EMPIRE deposit and push it into the player member, while the city
  ALSO adds its own realized roll-up of the same channel — and that roll-up already includes the empire leg, so
  each deposit lands once per city PLUS once more
  ([state-repositories.md](../../architecture/state-repositories.md): an upper scope's package is never added on
  top of a Σ that already contains it). ⚑ The city read wants the world CONFIG leg plus the rolled channel and
  nothing else. ⚠ The `INITIAL_TRADE_ROUTES` baseline pushed into the same member is a global define, not a
  deposit — decide its home rather than sweeping it in. ⛔ Deleting the accumulator fires no `updateTradeRoutes`
  rider, so confirm the trade-route ASSIGNMENT still re-runs off the surviving triggers
  ([save.md §6](../../specs/save.md); the baked-consumer re-run in [enabler.md §8](../../specs/enabler.md)).

- Retire the building-COST-modifier accumulator and move its readers. Its writer is already gone: the curator
  re-homed the legacy source-keyed cost map onto the TARGET, as a conditioned own-cost entry gated on
  possessing the source, so nothing feeds the accumulator and every reader now sees zero. The value lives in
  the target building's OWN `costs` entries, so the readers ask the target rather than a player-side map.
  ⚠ Distinct from its buildRate sibling, which stays source-keyed and converts to an entry-list read — the two
  look alike and do not resolve the same way.
- Serve the team improvement-yield GRANT through the new Python surface. The wonder events call it from
  `CvEventManager`, and the binding they called is gone — which is correct
  ([DEC-cy-not-fixed](../../architecture/decisions.md#dec-cy-not-fixed)): it comes back as the library's own
  read, never as a restored `CyTeam` binding. Until it does, the engine side stores and serves the value but
  nothing can grant one.
  ⚑ Grants recorded without an event id carry `-1` (unattributed) by design — the events rework threads the
  real id, and the store already carries the field for it.
- Repair the `savemigration.txt` REPLACEMENT-OBLIGATION notes that no longer resolve. Each note records WHICH
  named replacement now serves a cut value; where that replacement was archived or never built, the field is
  gone from every save and the value has NO source, which nothing catches ([AGENTS.md](../../../AGENTS.md)).
  The wellbeing cut notes are the known cluster — they name the archived bespoke substrate
  ([superseded-ideas](../../architecture/superseded-ideas.md) #14) rather than what actually serves them now.
- Cut `CvPlayer::processCivics` — the legacy civic ACCUMULATOR PUSH. A civic's deposits reach its cities by
  rolling DOWN the scope chain ([modifier.md §1](../../specs/modifier.md)), so pushing them into player-side
  accumulators is the STORED-ACCUMULATOR DRIFT class
  ([DEC-accumulator-cut-uniform](../../architecture/decisions.md#dec-accumulator-cut-uniform)). ⚑ It is already
  HALF-converted — `InfoValuation::collectKeyedTarget`, `providesAmenity(CLS_AMENITY_*)` and `resolvedCityLimit`
  sit beside the legacy reads — so the shape to finish is established, not invented.
  ⛔ **THREE KINDS, and collapsing them is the mistake:**
  1. **modifier-channel pushes** (yield/commerce/capital/building/bonus/terrain/specialist-percent/production
     modifiers, the wellbeing terms) — the cascade carries these; the push GOES.
  2. **per-flag POLICY counters** (`changeFixedBordersCount`, `changeNoForeignTradeCount`,
     `changeNoCorporationsCount`, `changeStateReligionCount`, `changeAllReligionsActiveCount`, …) — these are the
     hand-named counters that retire onto the `EmpireContext.policies` derived union
     ([contexts.md](../../architecture/contexts.md)), i.e. the SAME family as the amenity counters above; they do
     not simply vanish.
  3. **genuine non-cascade state** — the revolution index members (permanent Python-authoritative carve-out),
     `changeMaxConscript`, `changeSpecialistValidCount`, the hurry counts.
  ⚠ It also spans the per-TU error cap, so until it clears, lines ~16.9k–28.9k of `CvPlayer.cpp` have NEVER been
  compiled — it gates visibility into the rest of the file, not just its own errors.
- Sweep the WRITERLESS SERIALIZED ACCUMULATORS — a member that is still declared, still serialized and still
  READ, whose `change*` has NO CALLER left. The feeder was cut; the store was not.
  ⛔ **The compiler will NEVER name one** — it compiles cleanly and always will — so this class is invisible to
  the error-driven census and closes only by being looked for, exactly like the whole-registry AI scans below.
  ⚠ **It is WORSE than a value that reads 0.** On a new game it does read 0, which looks harmless; on an
  existing save it returns decades of accumulated history that no live source can reproduce and nothing
  re-derives — the STORED-ACCUMULATOR DRIFT class, wearing a plausible number
  ([DEC-accumulator-cut-uniform](../../architecture/decisions.md#dec-accumulator-cut-uniform)).
  ⚑ **The mechanical test is two greps, not a judgement:** the member has a `WRAPPER_READ`/`WRITE`, and its
  `change*`/`set*` has no caller outside its own definition. Known instances to start from:
  `CvCity::getBonusDefenseChanges` and `CvPlayer::getBonusCommerceModifier`.
  ⛔ Do NOT read "it compiles and something calls it" as evidence a value is maintained — that is the reasoning
  this entry exists to correct.
- Serve the UNIT-QUALIFIED entry sum — the ON-TOP read that `unit:`-qualified deposits have no consumer for.
  The compiled entry carries the qualifier (`CvModEntry::unitQual`) and BOTH the gather and the valuation
  deliberately skip it, because a unit-carried value rides live on top and is never cached
  ([DEC-unit-modifiers-on-top](../../architecture/decisions.md#dec-unit-modifiers-on-top)). That half is right;
  what is missing is the read that sums the entries matching a given unit predicate over the live sources, so
  those deposits currently reach nobody.
  ⚑ The live case is the per-military-unit happiness a civic or trait grants
  (`happiness.empire.cities.{unit: IS_MILITARY}`) — the CvModEntry comment names it verbatim. Its consumer wants
  the epoch-stable per-unit VALUE, which the city then folds as `perUnit × liveCount`; the writerless player
  accumulator it replaces is the last of that family.
  ⛔ It is NOT `keyedTargetSum` with a wider signature: that answers a NAMED target id, while this filters on a
  PREDICATE evaluated per candidate unit — a different axis, so folding them collapses two questions into one.
- Move every consumer off the hand-named channel-shaped getters on `CvCity`/`CvPlayer`, then delete the old names.
- Cut the hide-and-seek per-type intensity ACCUMULATORS on `CvUnit` (serialized — the cut carries a
  `savemigration.txt` step; confirm the tag spelling against the stream first). Their replacements are built.
  ⚡ The four accessor families have no call sites at all, so this is a plain deletion.
  ⛔ Leave `getInvisibleType` alone — a live `CvUnit` method, unrelated.
- Retire the direct `gDLL->logMsg` / BetterBTSAI log-helper call sites and the log-level globals they gate,
  wholesale as each domain migrates onto the spine — never tidied in place.

## Not built yet

- Convert the PLOT-YIELD AI CALLERS, the residue of that plane's cut.
  ⚠ **`calculateNatureYield` still has its AI callers, and they are NOT a segment re-point.** The genuine
  consumers of the plot's actual substrate value are served; what is left on the legacy walk is AI yield
  arithmetic — the improvement valuations that compute `nature + calculateImprovementYieldChange` (that sum IS
  `expected*`), the feature-food/hill-food heuristics, and the `bIgnoreFeature` CHOP what-ifs, which no stored
  segment can answer because they ask about a plot that does not exist. Re-pointing them at the segment would
  wire legacy AI calc onto new machinery — the "bending the new surface to preserve an AI call's old shape"
  failure. They convert with the valuation, or they go.
  ⚠ `calculateImprovementYieldChange` is the same rule's other half, and it needs NOTHING new — it collapses to
  ONE `InfoValuation::plotOwnYield(improvementModifiers, family, ctx)` with a plot-anchored ctx
  (`fillEvalCtxAtPlot`). Every term it reconstructs by hand is now a deposit on the improvement: its own yield,
  the riverside and irrigated variants (conditioned on HAS_RIVER / HAS_IRRIGATION), and the tech / civic /
  player improvement boosts, which the reverse pass lands on the improvement as conditioned entries —
  `plotOwnYield` serves exactly that, reverse-landed entries included.
  ⚠ Its `bOptimal` / `bBestRoute` arguments are WHAT-IF variations (best available route, irrigation assumed),
  so they are the AS-IF-HELD hypothetical the valuation already takes, never extra parameters on the read.
  ⚖ **THE ROUTE→IMPROVEMENT YIELD IS NOT AN AI CONCERN — THE AI EVALUATES A ROUTE ON MOVE SPEED (owner):**
  *"ai does not need to factor in that it gets more yield from route for some improvements in some cases, it
  should evaluate routes on movespeed."* So the improvement what-if does NOT owe that term, and a per-route ×
  per-improvement yield term must not be built into the valuation for it.
  ⚑ **Two reasons, and they are why this does not get re-litigated.** (1) **There is never a movespeed-vs-gold
  TRADEOFF** — a route is laid for movement, so nothing is being weighed against the yield and no comparison
  needs the number. (2) **The yield only happens ABOVE A THRESHOLD**, so it is incidental rather than a
  competing objective the AI could steer by. ⇒ Modelling it in the valuation buys no better decision at
  per-(route × improvement) cost on a hot loop.
  ⚠ Distinct from that, and still open on the DATA side: `RouteYieldChanges` are governing-deliverer
  SOURCE-side entries on the route (`{yield}.plot.improvements.{IMP}.flat`, modifier.md §4), and a KEYED entry
  never folds scope-wide (§5). `plotBaseYields`'s route leg is `plotOwnYield` — the route's UNTARGETED output —
  so that authored data currently reaches no plot yield at all. Serving it needs the improvement's FK passed
  beside its modifiers so the route's keyed entry can be resolved (`keyedTargetSum` already exists); it is a
  realized-yield question, NOT a blocker on any AI decision.

- Serve the SLIDER-SCALED SHARE of a channel — the part of a city's realized happiness contributed by a deposit
  carrying `per:{CULTURE_RATE, each:100}` ([json.md §3.1](../../specs/json.md): a slider rate is a count-scaler
  TOKEN, so "+N happiness at 100% culture" is an ordinary happiness deposit). The share resolves at gather and no
  point read or valuation exposes it on its own, so a consumer can neither discount it nor ask what one slider
  point is worth. ⚑ Two AI reads want exactly this: the civic valuation, which weighs happiness that a slider
  move could take away, and the culture-slider chooser, whose whole term is "how much RATE would clear this
  city's anger" — that chooser cannot answer at all today, so only the culture-victory branch moves the slider.
  ⛔ Not a getter per channel: it is the `per`-scaled share of a group the read already hands out.

- Migrate the `constructs` outcome data onto the unit's grants. `constructs` is the dominant
  `outcomes.actions[]` verb ([json.md §8](../../specs/json.md)) and reaches nothing, while the has-building
  surface it needs already exists on the rebuilt unit info and is read by the construct mission. ⚡ So this is a
  CURATOR item: the units authoring `constructs` vastly outnumber those authoring `grants.buildings`. ⛔ Do NOT answer it by giving `CvOutcome` a building member: that would make the
  data-driven outcome plane carry a hardcoded ability's payload, which is the carve-out the mission-concept
  rework owns.

- Evaluate the ROUTE's tech-gated movement tail at the movement read. A route's base move cost and its
  tech-gated delta are ONE slot on the route — the bare number plus a conditioned entry beside it
  ([modifier.md §6](../../specs/modifier.md), the worked case) — but `CvPlot::movementCost` takes the point read,
  which serves the unconditioned entries only, so the gated delta applies to nobody. ⚑ It wants the conditioned
  tail evaluated against the asking team, the same shape every other conditioned read uses.

- Apply the PER-CITY GATES AT THE COMBINE. [modifier.md §1](../../specs/modifier.md) specifies the realized value
  as the sum of the scope packages **with the per-city gates (state-religion-in-city, coastal, connected, area
  membership) applied live at the combine**; `InfoValuation::rolledLegsAtCity` is a bare package sum, so an
  UPPER-scope deposit gated on a CITY predicate never fires. ⚑ The gate is what a per-city condition on an
  empire-scope entry is FOR — the curator already authors them (a building's coastal trade routes are an empire
  entry gated `HAS_COAST`), so the deposits exist and are read by nothing.
  ⛔ It is NOT a package-rebuild question: an empire package has no city bound, so the condition cannot be
  evaluated when the package is built — that is precisely why the spec puts it at the COMBINE, where the asking
  city is known. ⚠ Until it lands the affected deposits are UNSERVED, and `savemigration.txt` carries the
  replacement obligation for the one whose accumulator has already been cut.


- Charge the improvement UPGRADE cost from somewhere that can tell an upgrade from a build. Nothing charges it
  today. The only implementation sat in the improvement-set choke point, which cannot tell the two apart, so a
  worker building a farm on a seed camp paid twice — once for the build, once again because the camp lists the
  farm as its upgrade — and it also fired on unowned plots, where there is no owner to charge. It wants the
  intermediate that only the upgrade path calls, never the shared setter.

- Give GAME OPTIONS and CONFIG VALUES a standardized read surface (owner: *"having standardized getters for
  gameoption, and config values is not a bad idea"*). The reads are scattered — `isOption` sites, string-keyed
  `getDefineINT`/`getDefineFLOAT` lookups, and modder-option reads — and the string-keyed half is the same
  per-call map walk [DEC-materialize-at-mapfrom](../../architecture/decisions.md#dec-materialize-at-mapfrom)
  already killed on the info side. Shape it like its two existing instances, `CvGameSpeedScale` and
  `CvTraitSelection` ([engine.md](../../reference/engine.md) § Consuming-system calcs): a purely-organizational
  static-methods class, one getter per group parameterized over the group's index
  ([patterns.md](../../architecture/patterns.md)).
  ⛔ **Three kinds, and collapsing them is the defect the surface must prevent, not a tidiness question.** A GAME
  OPTION (`GAMEOPTION_*`) is fixed at setup, so JSON may gate on it
  ([DEC-entity-gate](../../architecture/decisions.md#dec-entity-gate)); a CONFIG value is authored data on eras /
  gamespeeds / handicaps (read from its sources, never cached behind a staleness protocol —
  [state-repositories.md](../../architecture/state-repositories.md): WORLD is CONFIG); a LIVE option is
  user-changeable **mid-game**, which is why nothing STATIC may depend on one — a value that moves under authored
  data is not something authored data can be gated on. One undifferentiated `getSetting` would erase the
  distinction at exactly the point a reader needs it.
  ⚠ **`MODDERGAMEOPTION_*` is a LIVE option, not a game option — the NAME says otherwise and that is the trap.**
  It is set from the BUG menu at any time (`Afforess/ANewDawnSettings.py` → `setModderGameOption` + a net message
  for MP sync), so it belongs with `setDefineINT` on the live side despite sharing a prefix with the setup-fixed
  kind. Tunables are still MIGRATING into it — the leader-promotion culture threshold moved out of XML into a BUG
  option — so the boundary matters going forward, not just historically. Authored data honours it today, but
  nothing ENFORCES it: add a readJson check refusing a `MODDERGAMEOPTION_` condition, so the split is unsayable
  to violate rather than remembered.
  ⛔ **Grepping the two apart needs a negative lookbehind** — `MODDERGAMEOPTION_` CONTAINS `GAMEOPTION_`, so a
  naive scan reports every modder option as a game option and silently overstates the second.
  ⚑ A live-option flip now ANNOUNCES (`SEVT_GAME_GLOBAL_DEFINE_ADDED / _REMOVED`), so a consumer that must answer one finally
  can — but that closes the *reactability* hole only. It does not make a live option a legitimate gate for static
  data, and reading "it emits now" as permission to author against one is the misreading to avoid.
  ⚠ HANDICAP is two values, not one, and a single `getHandicap()` silently picks the wrong one half the time: the
  per-player handicap (saved) drives human-facing economics, while every `getAI*` advantage reads the GAME handicap
  (the average of alive humans) — [engine.md](../../reference/engine.md). Keep them separately named.

- Make `hideAndSeek` a CACHED BLOCK on the UNIT and the CITY, not a per-read walk of the info (owner). Today
  `CvUnit::concealment()` / `detectionAgainst(methodSkill)` re-walk the unit's info ∪ promotions ∪ combat
  classes on every call, and `getInvisibleType()` reads the INFO alone — so a **promotion-granted method does
  not work at all**, which is the whole reason the method is a skill.
  ⚑ The mark triggers already exist and are exactly right: a unit's resolved values move ONLY on a promotion
  or combat-class change (`SEVT_UNIT_PROMOTION_ADDED / _REMOVED` / `SEVT_UNIT_COMBAT_ADDED / _REMOVED`,
  [state-repositories.md](../../architecture/state-repositories.md)), and those are precisely the three
  carriers the block gathers over. The CITY side dirties on its building facts.
  ⛔ It is a SECTION, so it cannot ride the `URS_*` resolved table, which gathers modifier-FAMILY entries —
  it wants its own cached block on the same mark protocol, never a hand-named scalar pair beside it
  ([DEC-uniform-cache-shape](../../architecture/decisions.md#dec-uniform-cache-shape)).
  ⚠ `isInvisible` is one of the hottest reads in the engine, which is why the walk must not stay on it.
- The PLAYER-ALERT consumer, and the alerts owed to it — including "power restored"
  (`TXT_KEY_MISC_POWER_RESTORED`), which rode the per-turn maintainer the blackout status replaced and now hangs
  on `SEVT_CITY_STATUS_REMOVED` carrying `CITYSTATUS_POWER_DISABLED`; and the CAN_RETRAIN / NO_RETRAIN pairs the
  promotion KEEP gate used to emit per failing axis (terrain / feature / plot bonus / improvement-or-local-building
  / promotion prereq, plus two more the axis list does not name). All are authored and were rendering, so this is a real loss of
  player-facing information, not a dead-key cleanup: a unit now loses a promotion without being told which
  condition it lost. ⛔ They do NOT come back as a per-axis walk beside the gate — that rebuilds the legacy
  battery to caption a failure — and the "your building was obsoleted" message,
  which rides `SEVT_CITY_BUILDING_OBSOLETED_ADDED / _REMOVED` (emitted for exactly this, and for logging; it drives no apply) — they
  re-attach to the OPERATE CROSSING fact, never
  re-inlined at a mutation site ([event-spine.md](../../specs/event-spine.md)). Expect the owed list to GROW as
  each legacy mutator is cut; add them together on the facts.
- Decide WHERE the citizen-assignment re-check is asked for. The mechanism itself is right and stays — the AI
  needs a way to be told the best plots to work may have moved — so this is a CALL-SITE question, never a
  removal. `AI_setAssignWorkDirty` is called from across the engine while `AI_updateAssignWork` re-runs the FULL
  assignment for every marked city, so the flips are a turn-time cost in their own right.
  ⚖ **It LISTENS TO THE EVENT SPINE, and no AI loop ever touches it (owner)** — a consumer marking off the
  facts, exactly as the player alerts re-attach to the fact rather than being re-inlined at a mutation site
  ([event-spine.md](../../specs/event-spine.md)). So this is a ROUTING job, never a judgement re-made per site.
  ⚑ **Every trigger below is ALREADY a DOMAIN fact, so this needs NO new emits** (owner) — the whole plot
  substrate, the building/population/civic/trait facts, the culture-level fact and the working-city fact all
  announce today. ⛔ So do not open this expecting an emit-surface prerequisite: what is missing is the
  consumer, and the work is deciding which facts it acts on.
  ⚖ **The ruled set (owner): a PLOT CHANGED inside the city's WORKABLE SET — not an upgrade only · a BUILDING
  FINISHED that makes actual changes to specialists or plots · POP ADDED · a CIVIC change · a TRAIT change.**
  ⚑ Stating the plot leg as *changed* rather than *upgraded* is what makes it complete: pillage, bonus
  depletion and a chop are the same fact in the other direction, and the substrate already announces each per
  plot. The filter is MEMBERSHIP of the workable set, which the city context already folds
  ([contexts.md](../../architecture/contexts.md)).
  ⚑ **`CvCity::canWork`'s gates name what even that does not reach** — the workable SET ITSELF moving
  (working-city reassignment; the radius growing with culture / `adds3rdRing`, adding tiles that were never
  candidates, which no per-plot fact announces) and the water-work TEAM capability. Symmetric cases likewise:
  pop REMOVED, such a building DESTROYED, and golden age starting or ending (it moves per-plot yields through
  the threshold bonus, [golden-age.md](../../reference/golden-age.md)).
  ⛔ **Two gates are UNIT-MOVEMENT driven and must NOT ride the same routing** — an enemy unit sieging a plot
  and a naval blockade. Unit movement never dirties a cache
  ([DEC-unit-modifiers-on-top](../../architecture/decisions.md#dec-unit-modifiers-on-top)), so routing these
  like the rest reinstates per-move churn; decide them deliberately rather than discovering it in a profile.
  ⛔ **The BUILDING leg is conditional, and that is the whole difficulty: it is not "a building completed", it
  is "a building that actually changes specialist slots or plot output".** A building completing changes
  nothing about which plots are best unless it authors that, so the test reads the building's OWN compiled
  entries; firing on every completion is exactly the spray being removed.
  ⚑ The instrument is already built and needs nothing added: the setter emits the caller's module-relative
  return address on every false→true transition, so what fires TODAY resolves offline against the PDB — read it
  to find what to remove, now that what to KEEP is settled.
  ⛔ Do NOT re-add a flip to replace a cut maintainer's gated one: the legacy calls fire only when a value
  actually CHANGED, so an ungated stand-in adds to the very churn the instrument is measuring.
- The amenity CONSUMER side: re-point consumers onto the CITY read and retire the per-flag `CvCity` counters and
  their bespoke per-attribute predicates/facts ([contexts.md](../../architecture/contexts.md)). The apply sites
  are the `process*` functions, whose remaining pushes are largely this family — `changeZoCCount`,
  `changeProtectedCultureCount`, `setWorkableRadiusOverride` — against keys the data already authors
  (`zoneOfControl`, `protectedCulture`, `adds3rdRing`).
  ⚑ POWER is converted and is the worked pattern to copy: the fold announces the 0 ⇄ non-zero CROSSING and the
  existing consumers route on it unchanged. ⛔ The generalization left is the FACT — one parameterized amenity
  fact carrying the id, replacing the per-attribute ones (government centre, fresh water) as each converts. Do
  not grow a second bespoke fact per key on the way.
  ⚠ Check each counter's FEEDERS first: power's had none left in the engine at all (only a dead `CyCity`
  binding wrote it), so the conversion was a read-swap rather than a migration — the others may not be.
  ⚠ **The workable-radius one is a live BUG, not just an unmigrated shape, and it is the case that proves the
  model:** it is a plain SET rather than a refcount, so a city holding TWO radius buildings and losing one sets
  the override to 0 and loses the ring a live grantor still justifies — exactly the failure the id→COUNT
  dictionary exists to prevent. ⚑ It also carries a VALUE (the radius) rather than presence, which that
  dictionary already handles, being an int slot rather than a bit.
- Re-fold a conditioned amenity on a BUILDING grantor when its condition moves (the empire half is covered).
  It wants the condition-dependency route the modifier consumer already derives.
- Add `m_amenities` (and its fold leg) to `CvTraitInfo` / `CvTechInfo` WHEN data authors one — not before.
  readJson already reports an entity authoring a block its type cannot hold.
- The endpoint route table, beyond the stored-vs-oracle documents — it stays empty until the access surface can
  be read THROUGH ([http-endpoints.md](../../specs/http-endpoints.md)).
- The `requires` BLOCK COMPOSER — deciding heading, ordering and which clauses compose one block, which is the
  text manager's own job ([patterns.md](../../architecture/patterns.md) THE DIVISION OF LABOUR). ⚠ The CONDITION
  renderer it calls already exists and takes exactly what `CvRequires` holds, so this is a composer to write,
  never a renderer to build — reading it as the latter overstates the dependency and parks work that is doable.
- Give the ctx-taking KEYED SUM the scope filter its collecting twin already has. `collectKeyedTarget` takes an
  `iScope` (-1 = any) precisely because the same family+target is authored at two scopes with two different
  consumers; the ctx-taking `keyedTargetSum` — the one that serves the CONDITIONED tail through the ONE
  evaluator — takes none, so a caller that must pin a leg to one scope has to fall back to the unconditioned
  collect and silently loses every gated row.
  ⚑ The live case is the free-specialist split: a building authors BOTH a city-scope row (counted by the city
  holding it) and an empire-scope row (counted by the player), so the empire read MUST pin the scope or the two
  legs double-count — and pinning it today costs the conditioned tail.
  ⛔ Not a second read: it is one parameter on the existing one, matching the collect's spelling, so the two
  keyed reads stop differing on an axis the data actually uses.
- A home for pedia category / sort metadata ([pedia-read-map.md](../../reference/pedia-read-map.md) finding 4).
- Ranked-target-selection EVALUATION ([parked/ranked-target-selection.md](../parked/ranked-target-selection.md))
  — a ranked entry applies unranked until it lands.
- The Python data-fetching library (below).

## The GETTER cut — game objects + AI

> Sequencing, and the ban on bending the new surface to fit an old call: [roadmap.md](roadmap.md).
> ⛔ The unit of work is the CLASS of read, never the individual getter — many collapse as the rebuilt infos
> wire through ([DEC-new-getter-surface](../../architecture/decisions.md#dec-new-getter-surface)).

- Move the IMPROVEMENT tech-yield reads onto the keyed what-if twin. One of them asks a THIRD question the twin
  does not answer: the `bOptimal` branch wants "assume every condition holds", which is neither the live
  evaluation nor a hypothetical over one named id. Decide whether that is a MODE of the same read or an
  entry-list sum before converting it — ⛔ never a second evaluator beside the one the twin already uses
  ([DEC-single-implementation](../../architecture/decisions.md#dec-single-implementation)).
- Watch civic-choice STABILITY now that the cross-category half-value damper is gone (owner: drop it; if the
  problem shows, add it back properly). ⚠ It existed because civic valuations are linearly combined across
  categories, so a building gated by civics in two options could be counted at full value from both. If choices
  start oscillating, the principled fix is a `civics` id set on `CascadeCondDeps` — which is what the removed
  whole-civic-database sweep was reconstructing by hand — never restoring that sweep.
- ⛔ Neither is a VALUATION question, and filing them as one is what sent a reader looking for a magnitude
  machine that was never the dependency: both ask AVAILABILITY under a hypothetical HAVE, which is the
  enabler's ([DEC-enabler-not-cascade](../../architecture/decisions.md#dec-enabler-not-cascade)).
- Re-express the specialist EXPERIENCE reads as the ENTRY-LIST read over the specialist's own authored entries.
  ⛔ Not an arity fix — folding a keyed entry scope-wide is the silently-plausible-wrong case
  ([modifier.md §5](../../specs/modifier.md)).
- Give the CityContext its id-keyed RADIUS DICTIONARIES, then move the improvement count onto them. The count
  domain is wired but walks the city's radius per call; it runs at rebuild/per-decision cadence rather than on a
  read path, so it is not the banned read-time scan, but the dictionary is the standing target
  ([contexts.md](../../architecture/contexts.md)) and this read is the one that wants it.
- Fold the city's TYPED-FREE specialists into the specialist count once that ledger has a home again — the count
  answers the ASSIGNED population alone until then, and the legacy multiplier counted both.
  ⚑ The shape is already ruled ([tally.md](../../specs/tally.md)): give the OBJECT the aggregate and forward it
  through `CityContext`, never a tally side-store. ⚠ The count the legacy multiplier used is assigned **plus
  typed-free** specialists, so the city's assigned-only population counter is NOT the number — forwarding it
  would under-count silently.
- Make the empire GREAT-GENERAL rate a RECEIVER SUM over the player's cities, not an empire-package read.
  ⚑ Great general is NOT great people (owner): great PEOPLE accrue per city, while great general points are
  **summed from cities** plus battlefield experience into the player's own counter — so the empire figure is the
  cross-scope receiver shape ([state-repositories.md](../../architecture/state-repositories.md): a receiver total
  is the Σ of its MEMBERS' realized values), not the team+empire roll-up.
  ⚠ Until it exists the empire read sees only the civic/trait deposits and MISSES the city-authored building
  ones, i.e. it under-counts silently rather than failing — so this is a wrong number, not a dangling site.
  ⛔ Do NOT "fix" it by re-scoping the building data to empire: the city authoring is correct, the SUM is what
  is missing. (Great people's own city/empire split is right as it stands and is not part of this.)
- Move the realized-value reads onto the existing group reads (a consumer move, no new surface).
- Delete the per-SOURCE decomposition accumulators: member, `change*`/`get*`, read + write, and the tag named in
  `savemigration.txt` ([DEC-accumulator-cut-uniform](../../architecture/decisions.md#dec-accumulator-cut-uniform)).
  ⚠ Audit each `change*` BODY for side-effect riders first ([save.md §6](../../specs/save.md)).
  ⛔ They do NOT each earn a replacement getter — the group read answers the TOTAL, and per-source attribution is
  the ORACLE's job. Their last maintainers (`processBonus`, `processSpecialist`) go with them.
- Design the genuine residue that needs NEW surface: the slider math, the espionage counters, the live combat
  state, and `getHappinessTimer`.
- Hoist the per-commerce valuation in `getBuildingCommerceValue` — it runs once per (candidate × channel) where
  the caller already threads other per-candidate arrays for exactly this reason.

### How to work the compiler census

> Method, not a count to drive to zero. Regenerate it, never trust a stale number:
> `Assert build` from `Sources/`, then `grep -o "error C2039: '[^']*' : is not a member of '[^']*'" <log> | sort -u`.

- ⛔ **The census UNDER-REPORTS by an order of magnitude** — MSVC stops at 100 errors per TU (`C1003`), several
  TUs truncate, and a symbol can be broken with NO diagnostic at all. `grep -rn` over `Sources/` is the authority
  for whether a symbol is gone; never conclude a file is clean from its absence.
- ⛔ **A DROP in the count is not progress either** — the unity batches share the error budget, so deleting code
  anywhere changes WHICH files get to report. Count distinct `(member, class)` pairs, and treat a pair as cleared
  only when `grep` says the symbol is gone.
- **The disposition test, in order:** (1) grep the owning info's header for a RENAMED successor — the rebuilt
  infos are named for the JSON, so a "missing member" is most often a re-point; (2) if none, check whether the
  DATA is still authored — if it is, the INFO is missing a member and that is the defect, not the call site;
  (3) only then is the term dead — DELETE it, never comment it out.
- ⛔ **A blanket rename across `Sources/` is the trap** — info getter names are also live methods on game objects
  (some `DllExport`, i.e. EXE-bound), and the same name can be dead on one info and live on another.
  Distinguishing them is SEMANTIC (what is the receiver?), never textual: leave those and let the compiler name
  the sites individually.
- ⛔ A dangling site whose replacement MACHINE is unbuilt stays dangling — that is the census working. Name the
  missing machine, never the folder.

## Stage 4 — the Python surface

> Contract: [patterns.md § THE PYTHON READ BOUNDARY](../../architecture/patterns.md). Read maps:
> [pedia-read-map.md](../../reference/pedia-read-map.md) · [python-read-map.md](../../reference/python-read-map.md).
>
> ⛔ **"Stage 4" is a grouping, NOT a phase to wait for — nothing gates the disconnect.** A dead legacy Python
> getter is an OUTLAW, shot on sight; cutting wrong is cheap and is how a real dependency gets named
> ([roadmap.md](roadmap.md)). ADD to the library whenever a read makes sense, and clear Python-side compile debt
> as you meet it — parking it spends the census budget the rest of the worklist needs. ⛔ Never borrow legacy as
> a "temporary solution" for a read the library does not answer yet: add the read.

- **⛔ THE PYTHON READ SURFACE DOES NOT GO THROUGH THE CONTEXTS, and that is the whole point of them (owner):**
  *"it was kind of the point of the rework, to have these contexts, so we no longer had to loop infinitely
  everywhere, and then we have not actually wired the python to read from the contexts."* Every `CyState` read
  resolves a `CvCity*` / `CvUnit*` and asks the OBJECT, which is exactly what
  [DEC-scope-contexts](../../architecture/decisions.md#dec-scope-contexts) forbids — the HAVE axis is asked of
  the scope's CONTEXT, "never reached ad hoc off the game object".
  ⚑ **The cost is not style, it is the loops coming back.** A context STORES its aggregate and hands it over, so
  a possession question is a fetch; asking the object re-derives it. `CityContext` already holds
  `operatingBuildings()`, `hasVicinityBonus()`, `tradedBonusCount()`, `hasAmenity()`/`amenityCount()`,
  `power()`, `isCoastal()`, `areaId()`/`areaSize()`, `governmentCenterDistance()`, `isHolyCityAny()` — every one
  of which the Python surface currently answers the long way or not at all.
  ⚠ Three hangs came from the same shape on the state plane (a registry swept to rediscover what an entity
  already holds), which is [DEC-one-reverse-view](../../architecture/decisions.md#dec-one-reverse-view)'s rule
  one plane over. Re-point `CyState`'s city/plot/empire reads at the contexts; where a context cannot answer,
  that is a CONTEXT GAP to close by adding the forward ([contexts.md](../../architecture/contexts.md)), never a
  reason to reach past it.
  ⚠ UNITS have no context yet (a FUTURE role-specific scope, DEC-scope-contexts), so the unit plane reads the
  unit's own held containers meanwhile — O(held), never O(registry).

- **WorldBuilder's UNIT editing needs a home on the new surface.** ⚑ `CyUnit` carries the bare zero-`.def` type
  registration (the kept engine→Python direction's carrier, [patterns.md](../../architecture/patterns.md)) and
  nothing more, so `WBUnitScreen` / `CvWBDesc` reach no unit READS. **That is the EXPECTED state of the rework,
  not a defect** — the legacy binding surface is being disconnected, not repaired
  ([DEC-cy-not-fixed](../../architecture/decisions.md#dec-cy-not-fixed)).
  ⛔ **Do NOT "fix" this by re-registering the legacy `CyUnit`.** A dead legacy binding is an outlaw; the answer
  is the new library, never a revived `.def` surface.
  ⚖ What the new surface OWES, because the owner has ruled it supported: **editing an individual unit's strength**
  — *"you want people to be able to do things in WorldBuilder"* — plus the WBS scenario field that persists it.
  The engine state for it is in place (the serialized per-unit base, `state-repositories.md`).
  ⛔ Do NOT reproduce the legacy pair's bug when building it: the WB nudge did
  `setBaseCombatStr(baseCombatStr() + iChange)`, but the setter writes the BASE while the getter returns the
  fully-composed value (base + promotion/unit-combat delta + SizeMatters). They are not inverses, so every nudge
  corrupted the base. The new surface needs a base-in/base-out pair, or an explicit set-to-absolute.

- Build the ONE data-fetching library toward COMPLETE (its end state, not a gate on cutting). Build it for the
  pedia (a SHAPE oracle, NOT a coverage oracle — the appendix is enumerable).
  ⛔ **RESTORING THE ENGINE→PYTHON CALLBACK DIRECTION IS GATED ON THE INFO PLANE, and the reason is the IMPORT
  CHAIN, not the callbacks themselves.** The engine enters Python at `CvEventInterface`, which imports
  `BugEventManager` → `CvEventManager` → `CvScreensInterface`, i.e. the whole screen tree — so EVERY module on
  that path must import cleanly before ANY callback fires, and each builds its own global from the legacy god
  object at module scope. Those modules are INFO-dominated, so the chain cannot come back until `CyInfo` serves
  the info plane; the live-state and availability halves alone do not reach it.
  ⚑ That makes the direction patterns.md calls REQUIRED AND KEPT currently DOWN, which is worth knowing rather
  than rediscovering: the cut is meant to be one-way, and until this closes it is not.
  ⛔ It is NOT closed by publishing the legacy god object ([DEC-cy-not-fixed]) and NOT by a per-module shim
  ([DEC-no-legacy-masking]) — a module comes off it by having its reads served, one module at a time.
- Restore the `class_<>` TYPE REGISTRATIONS the binding purge took along with the `.def` surfaces. A registration
  carrying zero `.def`s is not a read surface — it is what lets the engine hand an object ACROSS
  ([patterns.md](../../architecture/patterns.md) THE PYTHON READ BOUNDARY). Without it the kept engine→Python
  direction raises at conversion instead of running. ⛔ Register the bare type; do NOT re-add getters with it.
  ⚠ The same hole reaches any published accessor whose RETURN type is an object — the art-info classes behind the
  art manager, and any info-object handle still published. A def that resolves and then raises reads as a mystery
  rather than as a missing binding, which is why this class hid.
  ⚑ **The PLAIN VALUE STRUCTS are in this set and take their FIELDS with them** — the purge deleted the struct
  registrar whole, and for a coordinate pair or an RGBA quadruple the members ARE the value rather than a getter
  over game state, so "do NOT re-add getters" does not bite there
  ([patterns.md](../../architecture/patterns.md) THE PYTHON READ BOUNDARY). Restore on demand, named by the call
  site that wanted it.
  ⚑ The test is mechanical: a type needs registration iff some engine call site passes or returns it. A wrapper
  whose `DECLARE_PY_WRAPPER` has no call site genuinely needs none.
- Serve the INFO-OBJECT accessor plane. `GC.get<X>Info(id).<method>()` is the dominant remaining Python read, and
  the global context hands out no info objects by design ([DEC-cy-not-fixed]) — so every one of them is an
  AttributeError at FIRST USE, not at import. `CyInfo` answers the generic reads by infotype prefix
  (`getDescription` / `getType` / `getButton`); what is unserved is the per-type tail (`getEra`, `getGridX/Y`,
  `getWorldSize`, `getPrereqAndTech`, `isVisible`, `getColorType`, `getActionInfoIndex`, …).
  ⛔ `getChar` is NOT part of this — the font glyph is TEXT-plane and is served off `CyGameTextMgr`
  ([patterns.md](../../architecture/patterns.md) THE PYTHON READ BOUNDARY), so it never closes by reviving an
  info accessor.
  ⚑ Re-derive the demand with `python Tools/census-python-boundary.py`; the accessor histogram is what ranks the
  work, and the four generic methods dominate it.
- Serve the free-function map helpers (plot direction / XY / distance / step distance). Their registrar went with
  the binding purge; the map-generation utilities call them throughout. ⚠ A map script's failure is SILENT — it
  lands inside the override protocol and falls back to DLL-default generation, so a wrong map is the symptom.
- Widen `CyInfo` to the per-type INDEX shape the whole-registry screens need — the text key and the button
  reference beside the description and type key it already serves. That shape is what every enumeration screen
  renders, across every registered category, and the prefix dispatch already reaches them all.
- Serve the reverse EDGE families through the info surface. The pedia derives "what needs me" / "what unlocks me"
  by scanning whole registries and asking a per-id predicate; the load-time reverse pass already lands those
  families on the info ([DEC-one-reverse-view](../../architecture/decisions.md#dec-one-reverse-view)). It is a
  served answer to an unserved question — the scans go when the read exists.
- Bind the XML-named callbacks that resolve to no `def` in the module the DLL names, and the DLL-named game-utils
  callbacks with no Python definition at all. ⚠ A name defined in ANOTHER module does not resolve, however
  reachable it looks from Python — the module the DLL names is the only one consulted.
- Add the missing `CvEventManager` handlers for the DOMAIN events the reporter emits and nothing handles. They
  drop silently through the dispatch-map miss path.
- Shoot the dead `Cy*` bindings on sight as the compiler names them. ⚠ Distinguish DEAD from UNREGISTERED first:
  a wrapper the engine still hands across is not dead, it is missing its registration (above).
- Serve the UPGRADE-AWARE building count the random-event surface reads. Its binding is already unpublished, so
  those reads answer nothing today — the cut NAMED the requirement rather than creating it.
  ⛔ It does not come back as the legacy count. "Including upgrades" resolves through a building's DORMANT
  TRIGGERS, and that bucket merges two populations: the successors that supersede a building, and the unrelated
  buildings whose mere presence dorms it (a pollution band dorming an observatory —
  [enabler.md §2](../../specs/enabler.md) unified the two deliberately, so the distinction does not survive into
  the data). Summing the bucket counts the band as an upgrade — plausibly, silently, and wrong. Decide what the
  read MEANS before serving it.
- REBUILD the emptied `CvGameTextMgr` composers on `appendEntryLines` + the requires block composer. The bodies
  that read legacy getters were CUT, not migrated, so this is writing them fresh — never "moving" one, and never
  restoring a body to see what it used to say.
  ⚑ DEMAND-DRIVEN and END-STAGE (owner): the tooltip set comes from playtests and community requests, and a red
  tree renders nothing, so appearance is unverifiable guesswork until it is green
  ([patterns.md](../../architecture/patterns.md) THE DIVISION OF LABOUR). ⛔ Do not open a layout pass.
  ⛔ A composer's acceptance test is that it reads NO legacy getter, never that it reads nicely.
  ⚑ The WIDGET-help composers are in the same rebuild, and one owes a line: the disabled-citizen tooltip's
  "one of these buildings would open the slot" list. It is a REVERSE cross-link, so it reads the specialist's own
  `EDGEF_RELATED` ([DEC-one-reverse-view](../../architecture/decisions.md#dec-one-reverse-view)) — never the
  whole-building scan asking each one, which is the own-data inversion it was.
  ⛔ The four WELLBEING composers are NOT `appendEntryLines` targets — a realized per-scope aggregate has no
  entry list to render from, and is a BLOCK. A bonus's "what needs me" block reads the bonus's own
  `EDGEF_RELATED` ([DEC-one-reverse-view](../../architecture/decisions.md#dec-one-reverse-view)), never a
  database scan; a promotion's per-unit lines read `CvPromotionAccrual::sum`, never a re-rolled rung loop
  ([DEC-single-implementation](../../architecture/decisions.md#dec-single-implementation)).
- Re-point the unit power-value plane's readers.

## Triggers / grants

- Build START PACKAGES: the entity type, its folder + prefix + repo row + manifest, and the shipped defaults
  ([triggers.md](../../specs/triggers.md)). Two content decisions ride it — which units the defaults name, and
  NPC/barbarian starts.
- Retire the engine start selection (the whole-database scan + AI scoring, and the per-role starting counts)
  once packages carry the identities.
- Finish the TRAIT free-promotion path on the trigger plane. ⛔ The DATA COMES FIRST: no trait authors a promote
  action today, and no `units.unitCombats` filter exists in the data, the compiled entry or the parser — the
  engine half below cannot be verified until a trait authors one. Two engine gaps then remain:
  1. the per-unit promote pass walks only the city's OPERATING BUILDINGS, so no trait entry is ever consulted —
     it wants the player's HELD-TRAIT walk beside it (the era-advance resolver is that walk, and the same
     PRESENCE read, never the banned own-data inversion);
  2. the promote applier IGNORES the entry's `units.unitCombats` filter, so a trait that arms one combat class
     would promote every unit in the city. ⛔ Landing (1) without (2) is worse than landing neither.
  ⚑ The CADENCE is the ruled one and is deliberately NOT the legacy one ([json.md §5](../../specs/json.md):
  promotions to the units present at END-TURN, one mechanism) — legacy instead fanned every unit of the player
  the moment the trait moved. ⚠ So the legacy removal half has no counterpart either: a promotion that stops
  being valid is dropped by the PROMOTION SYSTEM itself ([triggers.md](../../specs/triggers.md)), which is why
  the payload plane needs no take-away verb.
  ⚠ Until it lands, trait-granted promotions reach nobody, and `CvUnit::setFreePromotion`'s trait legs dangle
  naming exactly this. ⛔ Do not answer them by restoring a trait-side promotion×unitcombat map: that is the
  legacy mechanism whose data has already moved.

## Scale conversion

> Method: [fixed-point-and-scales.md § CONVERT BY ARITHMETIC CLUSTER](../../specs/curators/fixed-point-and-scales.md).
> Acceptance per cluster: ZERO new fudge factors at the mixing sites.

- Convert the remaining human-twin getters CLUSTER BY CLUSTER, never getter by getter: yield/food/wellbeing (the
  keystone), commerce, gold/maintenance/upkeep, trade profit, war weariness. Unit experience is self-contained
  and is the one safely parallelizable cluster.
- Decide which single denominator MOVEMENT speaks in — a curator question, and the prerequisite for finishing
  that family's scale ([fixed-point-and-scales.md](../../specs/curators/fixed-point-and-scales.md)).
- Reconcile `getFinalExpense`'s ×10000 inflation modifier when the gold cluster converts.

## Enabler

- Point the AI production decision at the maintained LISTED set, and collapse the `AI_chooseProduction`
  focus-ladder into ONE unified scoring pass ([enabler.md §6/§8](../../specs/enabler.md)). The collapse is an
  AI-architecture change, not a per-loop rewrite.
- ⛔ **FIND AND FIX EVERY AI EVALUATION LOOP THAT SCANS A WHOLE REGISTRY (owner).** They do not merely cost
  time — they *will not work any more*, and they are *not needed*. A scan asks the entity database a question
  the maintained state already answers, so it now walks ids whose backing reads are gone and re-derives what
  something else owns.
  ⚑ **The population splits in two, and the halves have DIFFERENT answers — do not treat them alike:**
  - a loop over an **enabler domain** (units / buildings / techs / civics / projects / processes / promotions /
    builds) reads the maintained FRONTIER instead — `listedIds` / `getAvailable*` / the tri-state
    ([enabler.md §6](../../specs/enabler.md): the AI's decisions iterate ONLY the frontier).
  - a loop over any OTHER registry (unitcombats / specialists / terrains / features / bonuses / religions /
    properties / invisibles) is the OWN-DATA INVERSION — it asks every id whether the entity deposits onto it,
    when the entity's own compiled entries name the handful it authored. `InfoValuation::collectKeyedTarget` /
    `collectKeyedCombat` are that read ([modifier.md §5](../../specs/modifier.md);
    [pedia-read-map.md](../../reference/pedia-read-map.md) finding 2).
  ⛔ **THE EXCEPTION IS PLOTS — a plot loop is NOT this class and must not be swept (owner).** There is no
  enabler structure for plots and there is deliberately not going to be one: a maintained set over ~10k plots
  is waste, and the decisions that walk plots already have to
  ([enabler.md §7.1](../../specs/enabler.md), the worker-builds carve-out — the plot-validity half stays a
  LIVE per-plot gate). So iterating plots is the correct shape, not a scan to convert; what a plot loop must
  not do is ask a per-plot question the CityContext already folds
  ([contexts.md](../../architecture/contexts.md): `plotAttrs` is the fold of its member plots' bits).
  ⛔ **The COMPILER WILL NEVER NAME ONE.** Every such loop compiles clean today and always will, so this class
  is invisible to the error-driven sweep and only closes by being looked for. ⚠ A loop calling a gate with
  WHAT-IF args cannot simply swap to the frontier — the frontier answers the CURRENT verdict, so those want the
  as-if-held overlay or the gate twin instead.
- Move `AI_baseBonusVal`'s per-kind loops off the whole-database driver onto the frontier, and off the dead
  prereq getters onto the bonus's own `EDGEF_REQUIRED_BY` ([DEC-one-reverse-view](../../architecture/decisions.md#dec-one-reverse-view)).
  ⚠ Its "would this bonus UNLOCK this" half needs the AS-IF-HELD overlay above and dangles until that lands;
  the rest does not wait on it. ⛔ The two halves are separable — treating the whole loop as blocked parks
  convertible work behind a dependency only part of it has.
- Restore the members `AI_techValue`'s remaining sweeps read, then drive them from the tech's own edges.
- Retire the active-set work-list ripple: the fact that justified it is now emitted, so the parallel propagation
  machine goes ([DEC-single-implementation](../../architecture/decisions.md#dec-single-implementation)).
  ⚠ `emit()` dispatches SYNCHRONOUSLY, so an event chain recurses on the call stack where the work-list iterated
  — design for that; it is not a reason to keep the machine.
  ⛔ Its runaway cap claims to "self-heal at the slice boundary"; that rebuild was REMOVED and nothing heals it.
- Converge the enabler's bespoke per-id reverse indices onto `EDGEF_REQUIRED_BY`
  ([DEC-one-reverse-view](../../architecture/decisions.md#dec-one-reverse-view) — a side index is banned
  "especially not inside an enabler"). There are **five**, in two files, all rebuilt by re-scanning every info at
  load: `s_operateBonusConsumers`, `s_operateBuildingDependents`, `s_operateDormantTriggeredBy`
  (`CvEnablerKernel.cpp`) and `s_udUnitDeps`, `s_udUpgradePred` (`CvUnitEnabler.cpp`).
  ⚑ The reverse pass already inverts BOTH `requires.build`/`requires.operate` AND `dormantTriggers()`, and
  `CvBuildingEnabler` reads that canonical edge for the frontier half **in the same file family** — so the answer
  these five recompute is already sitting on the info. The only delta is that they are operate-only while the
  canonical edge merges build+operate, and [enabler.md §5](../../specs/enabler.md) pre-answers it: over-inclusion
  in the reverse index is SAFE, a MISS is the bug.
  ⛔ Read [enabler.md §8](../../specs/enabler.md) "The reverse index, and what is deliberately NOT one" first —
  the axis-flag lists and `s_operatePropertyBandConsumers` are explicitly NOT convergence targets, and
  `s_specialBuildingMembers` is a sanctioned group→members derivation ([json.md §4.4](../../specs/json.md)).
  Sweeping those together with the five is the documented mistake.
- Delete the hand-rolled condition walks in `CvImprovementInfo` and route them through `CvConditionQuery`
  ([DEC-single-implementation](../../architecture/decisions.md#dec-single-implementation)). `ii_collectPredicateIds`
  duplicates the canonical leaf test **verbatim** while the canonical `collectPredicateIds` sits at ZERO callers —
  and `namesId` / `bucketForType` are equally unused. ⚠ The private walk covers only `all`+`anyOf`; the canonical
  also walks `noneOf`/`enabled`/`disabled`, so the duplicate is not merely redundant, it is NARROWER.
  ⚑ Same family: the three-way copy of the bonus/building presence-id collector in `CvImprovementInfo`,
  `CvBuildInfo` (`collectPrereqBonuses`) and `CvCorporationInfo` (`corpCollectSpreadBuildings`) — each re-testing
  an inline `compare(0,N,"PREFIX_")` that the zero-caller `bucketForType` exists to answer. The corp variant also
  needs the clause's `min`, which the shared surface does not yet return: LIFT that onto the shared surface, never
  keep the private copy for it. ⛔ `CvTechInfo`'s walk is NOT in this family — it reconstructs AND-vs-OR structure,
  which `CvConditionQuery` deliberately refuses to expose.
- Build out [enabler.md §8](../../specs/enabler.md) "Load-end reconciliation": plot-group membership derived
  rather than trusted from the save, the load-end dormancy fixpoint, and the dynamic operate axes on their events.

## Tree / include hygiene

- Retire the `CvInfos.h` umbrella — a hand-careful pass; the lessons and hard bans are in
  [AGENTS.md](../../../AGENTS.md) Conventions §Design.
- Run the dead-code / dead-XML pass — tooling generates CANDIDATES only; every removal verified against
  source/data and test-loaded, one subsystem at a time.
- Delete the `#ifdef` ATTICS — a guarded block whose symbol is defined nowhere AND has no commented-out
  `#define` either, i.e. no switch ever existed ([AGENTS.md](../../../AGENTS.md) Conventions §Design). Both
  halves of that test are mechanical, so the set is enumerable in one sweep.
  ⛔ A guard WITH a commented-out `#define` is a deliberate off-switch and is NOT in this sweep — it is a
  feature or a diagnostic, its disposition is the owner's, and the reason it is off gets recorded in the
  owning subsystem doc so the next sweep does not eat it. ⚠ `GLOBAL_WARMING` carries one and is nonetheless
  owner-ruled DEAD ([economy.md](../../reference/economy.md)), so it goes WITH the attics — the switch marks a
  candidate, never a verdict. ⚑ Its nuke counter (`getNukesExploded` and its changer) is live outside the
  feature and STAYS; only the warming machinery and the orphaned `GLOBAL_WARMING_*` defines go.
  ⚠ Exclude the compiler/tooling predefines and the vendored third-party files — those guards are defined by
  somebody, just not by us.
  ⛔ Why it is worth doing: these blocks are invisible to the compiler census (the preprocessor skips them),
  so they hold the names of cut members indefinitely and no build will ever name one.
- Route the `[CTB/work/intransit]` block onto the same gate as every other CTB line so it reaches `/events`.

## Green-up (after the structure, never ahead of it)

- Engine-repair debt: the bare Engine includes · the property-manipulator helpers · `CvCity.h`'s functor row.
- The vocabulary TXT keys (one per family/kind/predicate/token) — polish on a working machine; the renderer's
  spell-back fallback is the accepted output until then.

## Spec gaps to close

- Give the mod-data design invariants a spec home — a requirement may not unlock after the thing requiring it;
  replacements are explicit, never implicit; a replacing entity must be better. The checks are gone; the
  invariants belong in [json.md](../../specs/json.md).
