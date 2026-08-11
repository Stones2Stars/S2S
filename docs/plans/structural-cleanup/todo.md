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
> ⛔ **NO PACING IN THIS FILE EITHER — a todo says WHAT, never WHEN (owner).** *"If things are not properly in
> now, for whatever reason, then that is an ERROR, not something to postpone until whatever unicorn arrives
> later."* So a line NEVER carries a `when X is wired` / `once Y lands` / `until Z exists` clause: that is
> [DEC-no-deferred](../../architecture/decisions.md#dec-no-deferred) wearing a schedule, and it is worse than a
> plain deferral because it reads as a PLAN — the next agent sees a precondition rather than a defect and moves
> on. Something not properly in the tree is an ERROR; write it as one.
> ⚠ **A genuine DEPENDENCY is stated as a dependency, not as a date:** *"X cannot be expressed until the verb
> exists, so the verb is the work"* names a thing to DO. *"X, when the verb lands"* names nobody's job and
> nothing to do. ⛔ The ONE exception is an explicit owner-ruled ORDERING, and it is marked as the owner's
> ruling rather than as a property of the item.
> ⚑ **These lines ROT the worst**, which is why the ban is here beside the no-state one: a condition nobody
> owns is never met, so the item ages into scenery — and the tree grows a hole that reads as scheduled.
>
> ⛔ **The tree is the authority, always.** Before acting on any line here, confirm it against the code
> ([DEC-no-guessing](../../architecture/decisions.md#dec-no-guessing)) — and if it is already done, delete the
> line rather than updating it. Sequencing and governing rulings: [roadmap.md](roadmap.md).

## Blocked on an owner ruling

- Rule on how an ENACTED / HELD state pseudo-building expresses the CHOICE behind it — the ordinances, the
  culture `C_AC_*` set, the folklore requirement. This is the live ENABLE-SIDE OVER-OFFER the owner sees
  (a Speakeasy offered without alcohol banned; a culture building offered in a city with no culture; the
  folklore line offered outright), and it is NOT enabler machinery: the gate faithfully evaluates what the data
  says, and the data says every city has banned alcohol and holds every culture.
  ⚑ **The mechanism, and it is one cause for all of them:** each prerequisite is `notConstructible`, so
  [enabler.md §3](../../specs/enabler.md) places it in EVERY city UNCONDITIONALLY and dormancy alone decides
  anything after that — while its `requires.operate` names only a tech and a map category. So it is not merely
  PRESENT everywhere, it is ACTIVE everywhere, and a `requires.build` naming it can never refuse.
  ⛔ **So reading ACTIVE instead of PRESENT at the gate fixes NOTHING**, which is the tempting one-line repair and
  the reason this entry spells the mechanism out: both answers are yes.
  ⚖ What is missing is the CHOICE itself — an ordinance is ENACTED, a culture is HELD — and no authored condition
  expresses it, so the ruling is what that condition should BE (and whether the engine still owes the state it
  reads). ⚠ A data-model answer triggers the curator + regen in the same work item
  ([DEC-recurate-on-decision](../../architecture/decisions.md#dec-recurate-on-decision)).
  ⛔ Do NOT invent an ordinance-enactment mechanism to close it.
- Decide the ENABLER's load-gating policy, because the code and its own stated contract disagree.
  `CvEnablerConsumer`'s header declares the consumer LOAD-ACTIVE *with no `spineGameLoadInProgress()`
  suppression* — and cases below it, plus much of `CvBuildingEnabler` / `CvUnitEnabler`, suppress on exactly
  that. One of the two is wrong and an agent cannot tell which.
  ⛔ **It is NOT a delete-the-guards sweep**, and reading it as one is the trap: [enabler.md §7.1](../../specs/enabler.md)
  sanctions BOTH policies — every event's re-gates applying as they arrive, or gating running once after the
  stream ends — and says **the MIX is the bug**. The guards implement the second consistently, with the load-end
  pass as their twin, so removing them SWITCHES policy rather than tidying: every candidate would then re-gate
  repeatedly during the read, against half-built state.
  ⚑ So the deliverable is the RULING plus making the two agree — whichever way it goes, the header comment and
  the guards stop contradicting each other. The gate's own meaning is settled and is not what is open here
  ([event-spine.md](../../specs/event-spine.md) § `spineGameLoadInProgress()` IS RESULT-PRODUCER SUPPRESSION).
- Rule on the operating-set seed's ANNOUNCE suppression in `CvEnablerKernel`, which silences the emit while the
  save streams. Suppressing an EMIT to fix a CONSUMER is banned outright ([event-spine.md](../../specs/event-spine.md)),
  and the reason given is real rather than lazy: the set is idempotent but the DEPOSITS are not, so an in-read
  announce double-applies into the modifier packages — and the enabler's own stored-vs-oracle tripwire is blind
  to it, because the damage lands on the other plane. ⇒ The fix belongs on the CONSUMER side (the modifier
  declining an in-read activation it will fold at the drain), never on the emit; which shape it takes is the
  ruling.
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
  flag; `pillageGold` → drop; `base.airCombat` → the `strength` family, where every other unit's base value
  already lives. ⚠ The air one is the case that shows the cost of the mis-home rather than just its untidiness:
  `strength.unit.flat` is a family read while an air unit's base sits in `identity`, so the two planes of ONE
  question are reached by two different kinds of read, and a consumer must know which before it can ask.
  ⚠ `espionagePoints` rides the missions/`CvOutcome` carve-out — its channel is settled, only its authoring home waits.
- Re-home the CIVIC per-population wellbeing deposits onto the `cities` target. They author a BARE
  `happiness.empire` flat scaled `per: {type: POPULATION, each: 100, scope: city}` — an empire-scope deposit whose
  count is per-CITY, which has no single answer at empire scope: the package is the owner's, and there is no city
  bound to count the population of. The ruled shape is the one [modifier.md §2b](../../specs/modifier.md) already
  states for the bonus case — `empire.cities`, which resolves PER CITY so the entry's own `per` scaler and
  conditions resolve against THAT city and the value lands in its package.
  ⚠ It is the same authored mistake the bonus ruling names, one family over, so it closes by applying that ruling
  rather than deciding anything new. ⛔ Recurate and regenerate in the same change
  ([DEC-recurate-on-decision](../../architecture/decisions.md#dec-recurate-on-decision)).
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

## Legacy still breathing — delete it

- BUILD THE MISSING PLACERS for the queue-excluded, self-capped entities. `notConstructible` bars the production
  queue and says nothing about placement ([enabler.md §3](../../specs/enabler.md)), so a capped entity is placed
  by the system that OWNS it — and for most of them no such system exists in C++ or Python. A corporate HQ has
  one (`CvGame::setHeadquarters`); the achievements, relics, traditions, national beliefs and the `C_AD_*`
  culture set have none, so they are placed NOWHERE and their effects reach nobody.
  ⚑ The relic/`constructs` half is the outcome verb already tracked below — the achievements and the culture set
  need their own award path, which is the same question as how an ENACTED / HELD state expresses its choice
  (above).
  ⛔ Do NOT answer it by putting them back in the blanket pass: that is the retired reading, and it multiplies
  every scope-wide deposit they carry by the city count ([modifier.md §5](../../specs/modifier.md)).

> The standing rule (purge violently; blast radius is the signal; the worst offenders are the ones OFF the core
> loop) is [roadmap.md § LEGACY STILL BREATHING](roadmap.md). ⚠ KNOWN-INCOMPLETE — legacy found anywhere else is
> killed on the same terms. ⛔ Never record a found legacy surface as acceptable or "kept until X".

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

- **⛔ THE SPECIALIST YIELD HAS TWO MAINTENANCE SURFACES FEEDING ONE RATE, AND THEY DISAGREE — resolve which
  one is the model before touching either.** The modifier consumer applies a specialist's deposits into the
  CITY PACKAGE on the specialist fact (source = the specialist, scope = city, multiplicity = the count), which
  the roll-up then reads as the city's own flats — TIER-2, OUTSIDE the percent stack. `InfoValuation::specialistTerm`
  independently sums the same specialist's `city.flat` intrinsic — TIER-1, INSIDE it.
  ⛔ **BOTH PLANES ARE LIVE AT ONCE, so wherever a specialist authors an output channel the city DOUBLE-COUNTS
  it** — once inside the percent stack and once outside. That is worse than the disagreement this entry first
  described, and it is what the `[MODIFIER] specialistRead` census shows: its per-type rows ARE the TIER-1
  term, and those same specialists appear as `citySpecialistAdded` deposits feeding the TIER-2 side.
  ⚠ The doubling is NOT uniform across channels — a channel no specialist authors is untouched — so a rate
  that looks right on one yield says nothing about the others.
  ⚑ [modifier.md §2a](../../specs/modifier.md) is unambiguous that specialists are a TIER-1 BASE term and that
  TIER-2 EXTRA is BUILDINGS only, so the package application is the surface that looks wrong — but confirm
  against the apply path rather than deleting on the strength of the spec alone, because the apply is what the
  authored entries actually reach.
  ⚠ Whichever survives owes the rest of §2a's row, none of which `specialistTerm` carries: the intrinsic's own
  PERCENT layer, the per-type `empire.cities.flat`, the `perAll` bucket, and the TRAIT's specialist-keyed
  governing-deliverer deposits. And its MULTIPLIER counts `getSpecialistCount` alone where the spec and the
  engine both say `getSpecialistCount + getFreeSpecialistCount` — so the whole output of the `freeSpecialists`
  family rides on whichever path is kept.
  ⛔ Do NOT settle this by whether the resulting number moves toward a remembered figure
  ([DEC-baseline-is-a-smell-test](../../architecture/decisions.md#dec-baseline-is-a-smell-test)); settle it by
  which surface the spec names and which entries each one actually reaches.

- **The unresolved-FK census reports the §3.1 CATCH-ALL TOKENS as unresolved ids.** `CITY`, `TEAM`,
  `POPULATION`, `ERA`, `SPECIALIST`, `WORLD_WONDER`/`NATIONAL_WONDER`/`TEAM_WONDER`, the slider rates
  (`GOLD_RATE`/`RESEARCH_RATE`/`CULTURE_RATE`/`ESPIONAGE_RATE`), `CULTURE_PERCENTAGE`,
  `DISTANCE_TO_GOVERNMENT_CENTER`, `CORPORATION_LEVEL` are engine TOKENS ([json.md §3.1](../../specs/json.md)),
  not entity ids — `jsonResolveId` is being handed them and reports each as a miss. They are the majority of the
  census, so the genuinely unresolved ids are buried in false positives. Same disease the `amenities` unknown-key
  line had: the instrument does not know a vocabulary the parse path does. ⚠ Settle the `TAG_*` entries with it —
  those are classification ids minted at load, so whether they can resolve at FK time is the same question.

- **Give citizen plot ASSIGNMENT the trade-route treatment — it is a poll, not a trigger.** The work is driven by
  hand-wired `AI_setAssignWorkDirty` setters across the engine, drained by a FRAME-LOOP sweep over every city of
  every player, plus an unconditional per-turn blanket in `CvPlayer::doTurn` that consults no flag at all. That
  is [DEC-flag-is-fossil](../../architecture/decisions.md#dec-flag-is-fossil) at scale: every setter is a claim
  that we cannot know what changed, answered by re-sweeping everything.
  ⚑ Staleness is NOT its failure mode (polled every frame, rebuilt every turn) — the cost is turn time and the
  structure. The target is the trade-route shape: the fact that moved a city's plot basis re-assigns THAT city.

- **Retire the `CARGO_NAVAL` / `CARGO_MISSILE` kinds onto the tag-predicate shape.** They are read in
  `CvUnit::cargoSpace` as "+N hold when the CARRIER is `DOMAIN_SEA`" and "+N hold when its special cargo is
  `SPECIALUNIT_MISSILE`" — a filter on WHO CARRIES, not on what is carried. ⛔ [modifier.md §6](../../specs/modifier.md)
  rules that the "what" is ALWAYS a tag predicate and never a kind, and §5 already gives the shape for "every
  object of a kind in scope", so this authors as a plural `units` target with a predicate
  (`cargo.space.empire.units {IS_SEA}`) and both kinds go.
  ⚠ **The NAME is actively misleading and should not survive the retire**: `navalCargo` describes neither naval
  cargo nor a naval-unit hold — it is an empire-wide hold bonus scoped by the carrier's domain. A reader who
  trusts the name models the wrong axis.
  ⚑ The deposits currently REACH this reader (they were dropped as `combat.navalCargo` until the address was
  fixed), so this is a shape correction, not a restoration.

- **Share `InfoValuation::plotScaledYield` with the what-if plot read.** The package resolve calls it, but
  `plotBaseYields` — the isolated plot-as-base calc every what-if goes through — does not take the two operands
  yet, so a what-if answers the pre-scaling number while the live plot answers the scaled one
  ([DEC-single-implementation]: the arithmetic is shared, the operands must be too).
- **Re-point `CvPlayer::getExtraYieldThresholds` / `getLessYieldThresholds`, or delete them.** They read the
  channel through `realizedAtEmpire`, which SUMS — but the interval is the smallest positive one held, so the
  summed answer is wrong by construction. Their only callers are the Python bindings; the live feed uses
  `updateExtraYieldThreshold`'s min selection instead.

- **Make `tradeRoutes` the base section the spec now declares it to be** ([json.md §2](../../specs/json.md)) and
  move every trade-route authoring into it. The curator currently splits them across three places: the count/cap
  rows emit under `tradeRoutes` with member spellings no kind table carries (`coastal`, `foreign`), and
  `TradeYieldModifiers` emits under the YIELD families as `<channel>.<scope>.tradeRoute` — so a per-yield route
  modifier is addressed as a member of food. All of it lands as `unkinded-member` and produces nothing.
  ⚠ The route-KIND variants become CONDITIONS in the same pass (Ruling 11 / json.md §2), and `coastal` is a CITY
  predicate while `foreign`/`sharedCivic` are ROUTE predicates — settle where each is evaluated before authoring.
  ⛔ Recurate and regenerate in the same change ([DEC-recurate-on-decision](../../architecture/decisions.md#dec-recurate-on-decision)).
- **Cut the DEAD ACCUMULATORS — legacy members with no writer left, still serialized, still read.** Run the
  mechanical detector in [state-repositories.md](../../architecture/state-repositories.md) § THE LEGACY-ACCUMULATOR
  CUT (a mutator with no call site + a serialized member + a live getter) over `CvCity` and `CvPlayer`; every hit is
  a consumer being served save history where the cascade holds the truth, and the cut is the uniform one already
  specified there. ⚑ **Work it TOGETHER with the unkinded-member list below** — they are the data side and the
  carrier side of one defect, and where a pair matches, the quantity is missing end to end.
  ⚠ Each hit still needs its disposition decided: a value the cascade genuinely owns is re-pointed, while genuine
  one-shot event state (an `applyEvent` writer) correctly stays serialized ([save.md §3](../../specs/save.md)).
- **Land the UNKINDED MEMBERS — authored deposits the parser drops, and it says so on every load.** `readJson`
  reports each as `[READJSON] unkinded-member <family>.<member>`; the member simply has no row in its family's
  kind table, so the deposit is parsed, reported and then produces nothing. ⛔ **Read that census before
  anything else on this** — it is the authoritative list and it costs one grep of the load log.
  Each one resolves to exactly one of three dispositions, and they are NOT interchangeable:
  - **a genuine KIND** that needs its row minted (the combat defensive/capture members, `gold.headquarters` and
    `culture.headquarters` — both riding the corporation rework);
    ⚠ **Check the address before minting: three of this list were NOT kinds.** `upkeep.upgradePrice` was the
    wrong FAMILY (an upgrade price is a COST — `costs.empire.upgrade` already existed);
    `happiness.nonStateReligion` cannot be a kind at all (wellbeing mints ZERO kinds, ruling 12) and is the
    §3.7 counted-kind filter `{religion: "!IS_STATE_RELIGION"}`; and `cityCapture.resistance` is DELIBERATELY
    unkinded (ruling 16, trigger-plane). Minting a row for any of them would have carved the rollerskate in;
  - **a VARIANT that must become a CONDITION, never a kind** — `tradeRoutes.foreign` / `.coastal` are Ruling 11
    verbatim (*"the variant members are CONDITIONS, re-authored as predicates on the curator batch — never
    kinds"*), so this is the ruling not being implemented rather than a new decision. ⚠ The two are not the same
    predicate shape: `coastal` is a CITY verdict, while `foreign`/`sharedCivic` are per-ROUTE and are evaluated
    against the partner city inside `totalTradeModifier` — settle where each is asked before authoring either;
  - **deliberately unkinded pending a re-home** — the trigger-plane set (`combat.subdueAnimal`,
    `combat.nukeInterception`, …) is held that way ON PURPOSE ([triggers.md](../../specs/triggers.md)); minting a
    kind for one is the banned move.
  ⚑ **`<channel>.tradeRoute` is its own question:** a per-trade-route yield is a `per` SCALER on the channel, not
  a member of it — so it lands as the channel's ordinary deposit scaled by the route count, and needs the route
  count wired as a count-key first.

- **Settle what the stored-vs-oracle pair is FOR, because it cannot be what the spec currently claims (owner):
  *"oracle will never work like you want it, because it would require a republish of every event to rebuild."***
  A package slot is the sum of what the FACTS applied — which plots were worked, which buildings operated, what
  membership held at each moment. A gather that walks sources and sums their deposits is not a second derivation
  of that number, it is a different question, so the two cannot be expected to agree and a diff between them
  cannot name a missed emit. ⚠ [state-repositories.md](../../architecture/state-repositories.md) § THE RECOMPUTE
  IS AN ENDPOINT ORACLE states the opposite (an independent full recompute as the acceptance mechanism) and needs
  correcting to whatever the pair IS good for — the acceptance bar for the rebuild depends on the answer.
  ⛔ Do NOT "fix the gather" to make the two agree, and do not bend the stored side to it: agreement bought by
  replaying events is not independence, it is the same derivation twice (the false-confirmation trap
  [validation.md](../../specs/validation.md) names).
- **Route the remaining `per` COUNT scalers.** Deposits scale on the commerce-slider rates
  (`GOLD_RATE`/`RESEARCH_RATE`/`CULTURE_RATE`/`ESPIONAGE_RATE`), on the wonder counts
  (`WORLD_WONDER`/`NATIONAL_WONDER`/`TEAM_WONDER`) and on per-improvement counts, and none of those counts moves
  the deposits it scales. The facts already exist (`SEVT_EMPIRE_COMMERCE_PERCENT_*`,
  `SEVT_EMPIRE_BUILDING_COUNT_*`, the plot improvement pair); what is missing is the consumer route
  ([DEC-close-event-gaps-now](../../architecture/decisions.md#dec-close-event-gaps-now) — the third gap form).
- **Route `SEVT_PROPERTY_ADDED / _REMOVED` and `SEVT_CITY_PROPERTY_BAND_ADDED / _REMOVED` into the modifier.**
  They fire from the `CvProperties` choke points and the band registry into a consumer set that carries no case
  for either, so a threshold-conditioned deposit on a property never moves
  ([event-spine.md](../../specs/event-spine.md) § THE RECEIVED LINE names this as the worked instance).
- **Delete the atom-fan BANK on the city plane.** The load-bracket bank replays an empire-level crossing's city
  half at `GAME_LOAD_FINISHED`, but plane A already applies a conditioned deposit when its SOURCE arrives and
  books it — so the drain finds every entry already booked and applies none. It is a second maintenance surface
  for work another plane does ([roadmap.md](roadmap.md): a wrong wiring is removed on sight). ⚠ Its PLOT half is
  NOT redundant and must survive the cut — that is the only route reaching plot-scope deposits gated by an
  empire-level atom.

- Serve a city's OFFERED RESOURCES, and give the city screen a VICINITY tab that shows them (owner) — a city
  currently has no readable list of what its plot group supplies, so the axis most `requires` gates on is the one
  nothing can be checked against. ⚑ The point is TRACKING: the gate is only trustworthy if the supply behind it
  is visible.
  ⛔ **It is a READ, and nothing is mirrored onto the city** ([enabler.md §8](../../specs/enabler.md) RESIDENCY:
  the plot group is the ONLY authoritative list for trade resources and NOTHING mirrors it; `CvCity::getNumBonuses`
  is a relay). A stored per-city copy is the third copy of one number and is the banned shape.
  ⛔ **VICINITY is TWO independently-owned halves and the reader UNIONS them** ([contexts.md](../../architecture/contexts.md)
  THE VICINITY SPLIT): the MAP half is `CityContext`'s radius bonus dictionary, the BUILDING half is
  `OperatingBuildings::provided` (the operate/provides fixpoint, which only the enabler can resolve). Neither may
  be mirrored onto the other — the enabler mutates its set in place as the fixpoint ripples.
  ⛔ The tab shows VICINITY, which is NOT traded — the split, its two halves and the never-sum rule are
  [enabler.md §8](../../specs/enabler.md) + [json.md §5a/§3.4](../../specs/json.md); do not restate them here.
  ⚑ The accessor is `getVicinityBonuses` — the bonuses that ORIGINATE FROM the city — and the stores already
  answer it, so it is a listing over what exists, never a new store. ⚠ What is missing is any LIST accessor at
  all: `hasVicinityBonus` is per-bonus, so a tab built on it sweeps every bonus id per render — the own-data
  inversion ([DEC-one-reverse-view](../../architecture/decisions.md#dec-one-reverse-view)).
  ⛔ It is served through the NEW Python surface, never a revived `Cy` binding
  ([DEC-cy-not-fixed](../../architecture/decisions.md#dec-cy-not-fixed)).

- Model the `MAPCATEGORY_` gate. `cascadeEvalCondition` returns TRUE for every `MAPCATEGORY_` atom — the clause
  is not evaluated at all — while it is the most-authored plot atom in the data by a wide margin.
  ⚠ **Its impact is ZERO until the off-world content is reachable (owner)**, so this is LATENT, not a live wrong
  verdict: nothing that gates on a map category has been researched yet.
  ⛔ **That is what makes it worth writing down rather than leaving to be noticed** — it is unexercised AND
  uninstrumented, so no playtest of the standing save can ever surface it, and the clause will start silently
  admitting everything the moment the space line becomes buildable ([roadmap.md](roadmap.md) § the worst
  offenders are the ones off the core loop).
  ⚑ The re-gate ROUTE is already wired (a terrain fact seeds the atom, [enabler.md §8](../../specs/enabler.md)),
  so this closes by giving the predicate a body, not by touching the enabler. ⚠ `MAPCATEGORY_` is XML-only
  ([naming.md](../../specs/naming.md)) and a plot's set is derived from its terrain, so the evaluator's input is
  the terrain's own list — there is no plot-side store to build.

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

- Set `ctx.civic` where a CIVIC's value is resolved, so `{CIVIC_CATEGORY: CIVICOPTION_X}` can answer. The
  predicate, the ctx slot and the authored deposit all exist (a trait waiving religion-civic upkeep authors
  `upkeep.empire.civic.percent: -100` gated on it), but no walk sets the slot, so it answers FALSE everywhere and
  the waiver is inert. ⚑ FALSE is the CORRECT unset answer (contexts.md § THE SOURCE SLOTS) — the gap is the
  consumer, not the predicate. The civic-upkeep resolve is the walk that knows the civic; it sets the slot on a
  LOCAL COPY of the ctx, exactly as the religion and sourceBuilding slots are set.

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

- Make `hideAndSeek` a CACHED BLOCK on the CITY, as the UNIT side already is
  ([state-repositories.md](../../architecture/state-repositories.md) — a SECTION folds beside the slot table on
  the same mark). The city side marks on its building facts.
  ⚠ `getInvisibleType()` still reads the INFO alone, so a **promotion-granted invisibility type does not work
  at all** — which is the whole reason the method is a skill. It wants the same folded read.
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
- Decide whether `CvTraitInfo` / `CvTechInfo` carry `m_amenities` and its fold leg. ⚑ The question is answerable
  NOW rather than on a future authoring: readJson reports an entity authoring a block its type cannot hold, so
  the tree already says whether any does — read it, then either wire the leg or record that the block does not
  belong on those types.
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
- Move the pedia hub's CATEGORY CLASSIFIER onto `identity.pediaCategory`. The home is settled and the read is
  published ([pedia-read-map.md](../../reference/pedia-read-map.md) finding 4), so what is left is the consumer:
  the hub still derives its groupings from a pile of heuristics over legacy per-field getters — era banding,
  cost tests, instance-cap tests, promotion-line flags — and, for three buckets, a SUBSTRING MATCH ON THE
  LOCALIZED DISPLAY NAME, which is silently wrong in every non-English localization.
  ⛔ **The banned repair is publishing those legacy getters so the classifier resolves** — that preserves the
  name-matched buckets while reading as migrated ([DEC-new-getter-surface](../../architecture/decisions.md#dec-new-getter-surface)).
  ⚠ The era SUB-category stays derived from the entity's own era and is not a second authored field.
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

- ⛔ **NO MAP HAS BEEN GENERATED SINCE THE MAP-GEN READS MOVED ONTO THE LIBRARY.** `CvMapGeneratorUtil.py` is
  the DLL's own fallback implementation ([engine.md](../../reference/engine.md): undefined callbacks fall back
  to it), so a broken read there is not one screen failing — it is the whole generation path, and **nothing on
  the standing save exercises it**. Only starting a NEW GAME can observe it
  ([DEC-done-is-observable](../../architecture/decisions.md#dec-done-is-observable)).
  ⚠ **Map scripts are NOT outside the library — only their CALLBACK contract is separate**
  ([python-read-map.md](../../reference/python-read-map.md) ruling 1). The old `GC.get<X>Info` endpoints are
  not coming back, so their reads live on the named surface like every other consumer's.
  ⚖ **What IS genuinely theirs is the ENUMERATION: a map script really does have to iterate every bonus,
  terrain and feature**, so the whole-registry loop is correct here and STAYS — the pedia's carve-out, second
  instance. ⛔ Do not convert a map-gen sweep to an edge read; only where each value comes from changes.
  ⚑ The same run is the ONLY way to observe everything that fires at game start — the era/civ free techs, the
  starting units and gold, `freePopulation`, `FreeStartEra`
  ([legacy-grant-apply-sites.md](../../reference/legacy-grant-apply-sites.md) §5): none is exercisable on the
  standing save, so none can be observed until a new game is started.
  ⛔ **The MAP SCRIPTS (`PrivateMaps/`) are part of that path and are easy to miss — they sit OUTSIDE `Assets/`,**
  so a sweep rooted there reports clean while every selectable map type is still dead. They read the same named
  accessors as everything else.

- ⛔ **THE KEYED + CONDITIONED READ IS UNSERVED, AND THREE MAP SCRIPTS DANGLE ON IT.** A keyed deposit serves its
  UNCONDITIONED entries only; the conditioned tail belongs to the `expected*` valuation
  ([modifier.md §5](../../specs/modifier.md)), and no keyed twin of it exists. Two reads have no bare successor:
  an improvement's BONUS-conditioned yield, and a feature's HAS_RIVER-conditioned yield — both plain members
  once, both now conditioned entries.
  ⚠ The PerfectWorld-lineage starting-position normalizers (`C2C_PerfectMongoose_v310` · `C2C_PerfectWorld2f` ·
  `C2C_Totestra`) each keep ONE dead handle for exactly this, commented at the site. ⛔ Do NOT close them by
  summing the conditioned tail: that applies every tech- and age-gated deposit from turn 0, plausibly and
  silently ([DEC-no-legacy-masking](../../architecture/decisions.md#dec-no-legacy-masking) — a visible break
  beats a wrong number).
  ⚑ Those same three scripts also call `isRequiresFlatlands()` on a BONUS, which no `CvBonusInfo` has ever
  carried (it is a FEATURE member) — a PRE-EXISTING crash, not a migration casualty. Which bonus predicate was
  meant is a data question, so it is left standing rather than given an invented meaning.

> Contract: [patterns.md § THE PYTHON READ BOUNDARY](../../architecture/patterns.md). Read maps:
> [pedia-read-map.md](../../reference/pedia-read-map.md) · [python-read-map.md](../../reference/python-read-map.md).
>
> ⛔ **"Stage 4" is a grouping, NOT a phase to wait for — nothing gates the disconnect.** A dead legacy Python
> getter is an OUTLAW, shot on sight; cutting wrong is cheap and is how a real dependency gets named
> ([roadmap.md](roadmap.md)). ADD to the library whenever a read makes sense, and clear Python-side compile debt
> as you meet it — parking it spends the census budget the rest of the worklist needs. ⛔ Never borrow legacy as
> a "temporary solution" for a read the library does not answer yet: add the read.

- **RE-POINT THE SURVIVING `GC.get<X>Info` READS.** They are published NOWHERE, so each one is an
  `AttributeError` the moment its handler fires — not a slow read, and not something a playtest surfaces until
  that exact code path runs. ⚑ The IDENTITY plane is already total across both halves of the prefix dispatch
  (JSON-backed and XML-only alike), so an identity read — description, text key, civilopedia, strategy, type
  key, button — re-points mechanically onto `CyInfo` for EVERY registry, and a caller never learns which half
  answered.
  ⚠ **The rest is not a port list.** What remains under those call sites is the legacy per-field getter
  contract, which is a DELETION LIST plus a COVERAGE CHECKLIST
  ([DEC-new-getter-surface](../../architecture/decisions.md#dec-new-getter-surface)) — each one is answered by
  a GROUP read, an intrinsic slot, an edge family or a classification test, or it is dead. ⛔ Adding a getter
  per legacy name is the half-migration reflex in its purest form.
  ⚑ **A CROSS-LINK ON THE PLOT SUBSTRATE READS `EDGEF_RELATED`, AND THE ABSENCE OF `EDGEF_REQUIRED_BY` THERE IS
  NOT A GAP.** The two axes have different routers: `EDGEF_REQUIRED_BY` is landed by a per-prefix resolver that
  covers the gate-bearing kinds only, while `EDGEF_RELATED` is landed through the BROAD repo routing, so a
  terrain or feature named by a building's `requires` gets that building on its own RELATED list. A page asking
  *"which buildings name this terrain"* is therefore the ordinary
  [DEC-one-reverse-view](../../architecture/decisions.md#dec-one-reverse-view) re-point, not a dangling
  consumer. ⚠ RELATED is a MERGED bucket ([enabler.md §2](../../specs/enabler.md)), so it is safe for a DISPLAY
  list with ANY semantics and never for a consumer with ALL semantics — a panel that split mandatory from
  one-of loses that split, which is a stated display change and not a reason to hesitate
  ([patterns.md](../../architecture/patterns.md)).
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
- Give WorldBuilder's two `CyMapGenerator` calls a home. The wrapper's **BINDING** is gone — it publishes no
  `class_<>`, no `.def` and has no `pythonPublish` — so `CyMapGenerator()` raises `NameError` at the click
  ([roadmap.md](roadmap.md) § scope decision 1b requires a knowingly-broken WB path to be recorded rather than
  silently left). ⚠ **The CLASS itself is NOT gone** — `Sources/Python/CyMapGenerator.{h,cpp}` are still in the
  tree and still compile; what was cut is only the registrar. ⛔ So do not read the surviving file as evidence
  the binding lives, and do not read this entry as licence to re-register it
  ([DEC-cy-not-fixed](../../architecture/decisions.md#dec-cy-not-fixed) — the answer is the new surface).
  ⚑ It is a DEAD TRANSLATION UNIT meanwhile: nothing but its own `.cpp` includes the header, so it compiles a
  class nobody can reach, and it carries a commented-out constructor
  ([DEC-no-rollerskate-evidence](../../architecture/decisions.md#dec-no-rollerskate-evidence)).
  ⚠ **The two callers want DIFFERENT things, and only one of them ever existed on this wrapper:**
  `CvWBDesc.py`'s `addBonuses()` is a real method on it; `WBGameDataScreen.py`'s `eraseGoodies()` **is not on
  `CyMapGenerator` at all** — it is `CvMapGenerator::eraseGoodies`, and the Python-reachable route to it is
  `CyMap`, not this wrapper. So the goody-erase would have failed on the missing METHOD even with the binding
  restored, which is why "re-register the wrapper" was never the fix.
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
  ⚠ They label their whole DEPOSIT total "from buildings", which is untrue — every source deposits into those
  channels (civics, traits, bonuses, corporations, specialists, features, techs, projects), so the line names one
  source for a sum that carries them all. ⛔ The fix is NOT to point the city source-walker at it: that walker
  covers the buildings/civics/traits/culture-level set, which is complete for `defense` and PARTIAL for
  wellbeing — and partial attribution rendered as complete is the defect the census exists to prevent. Either
  widen the walker to wellbeing's full source set, or relabel the lump for what it is.
- Re-point the unit power-value plane's readers.

## Triggers / grants

- Build START PACKAGES: the entity type, its folder + prefix + repo row + manifest, and the shipped defaults
  ([triggers.md](../../specs/triggers.md)). Two content decisions ride it — which units the defaults name, and
  NPC/barbarian starts.
- Retire the engine start selection (the whole-database scan + AI scoring, and the per-role starting counts)
  once packages carry the identities.
- Walk the player's HELD TRAITS in the per-unit promote pass. `tr_promoteOneUnit` walks the city's OPERATING
  BUILDINGS only, so no trait entry is ever consulted and trait free promotions reach nobody — the data is
  authored and in the ruled shape, so this is the whole of what is missing. The era-advance resolver is that
  walk, and the same PRESENCE read, never the banned own-data inversion.
  ⚑ The per-class filter needs NO new mechanism: it is the entry's own `enabled: "IS_<TAG>"` predicate, which
  `tr_promoteFromEntries` already evaluates for the building leg. ⛔ Do not answer this by restoring a
  trait-side promotion×unitcombat MAP — that is the legacy mechanism.
  ⚠ The legacy removal half has no counterpart: a promotion that stops being valid is dropped by the PROMOTION
  SYSTEM itself ([triggers.md](../../specs/triggers.md)), which is why the payload plane needs no take-away verb.
  ⚠ `CvUnit::setFreePromotion`'s trait legs dangle naming exactly this.

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
