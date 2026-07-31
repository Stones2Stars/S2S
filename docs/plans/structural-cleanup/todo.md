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

## Data — curator

- Curate `culture.unit.garrison` and `costs.empire.perInstance` (flagged in-code, awaiting their batch).
- Attach the ruling-16 trigger-plane set (`survivor`, `cityCapture`, `combat.subdueAnimal`,
  `combat.nukeInterception`) to its trigger's `chance` ([triggers.md](../../specs/triggers.md)).
  ⛔ Do not unblock the dangling `getSubdueAnimalBonusAI` consumer by minting a kind for it.
- Give the §3.9 entry grammar a payload-less form so a carrier can state a cargo RESTRICTION with no capacity of
  its own, then author the flagged carriers ([modifier.md §6](../../specs/modifier.md)).
  ⚠ Settle in-game first whether the ancient transports are civilians-only; the owner's recollection is explicitly
  unconfirmed and must not be authored against.
- Re-home the remaining `identity` EFFECT keys to the block that already exists for each
  ([json.md §7](../../specs/json.md)): constraints → `requires`/`allowed`; `diploVoteType` → the top-level
  `voteSource` section (and rename the getter off the legacy XML tag); `tradeable` → the `canTrade` block;
  `commerceDoubleTime` → a second deposit gated on `existedFor`; `advancedStart` → resolve the curator's parked
  flag; `pillageGold` → drop.
  ⚠ `espionagePoints` rides the missions/`CvOutcome` carve-out — its channel is settled, only its authoring home waits.
- Emit property pulses through the shared property-source cleaner as trigger entries carrying
  `on`/`relation`/`distance`, instead of parking them verbatim.
- Author the leader→trait assignments. The chain is wired and the slots are authorable; the CONTENT is
  community-owned, so this closes by AUTHORING and never by reconstructing the tables the curator dropped.
- Author per-leader `ai.personality.researchSearchDepth` ([enabler.md §8](../../specs/enabler.md)). Same shape as
  the trait assignments: the read is wired and an unauthored leader takes the default, so this closes by
  AUTHORING. ⚑ It is the beelining dial — a leader that should not commit five techs deep for one unlock is
  expressed HERE, as data, rather than by weakening the enablement valuation for everyone.
- Author a PROJECT's victory MEMBERSHIP, which the curator drops. The legacy `VictoryPrereq` tag said "this
  project belongs to, and needs, this victory condition"; nothing in the emitted JSON carries it, and
  `victory.thresholds` is a DIFFERENT concept (how much this project contributes), so a project that is part of a
  victory but contributes no threshold has no representation at all. The spec already answers the shape — it is
  the entity-level applicability gate over a world-scope victory atom
  ([json.md §2](../../specs/json.md) Applicability, [DEC-entity-gate](../../architecture/decisions.md#dec-entity-gate)),
  never a revived `getVictoryPrereq`. ⚠ Its AI consumer dangles until this lands; that is the census working.
- Settle what FLANKING is keyed BY, then move its consumers. The legacy getter is keyed by UNITCOMBAT
  (`getFlankingStrengthbyUnitCombatType`, read in a loop over that registry) while the authored data keys by
  UNIT — `combat.unit.flankingUnit.{UNIT_X}`, which is where all of it lives. So the two sides disagree about
  the axis, and re-pointing the consumer would silently answer a different question rather than the same one.
  ⚑ [skills.md §1](../../specs/skills.md) ties flanking to `targets` (per-combat-class), which is the reading
  the DATA does not match — so this is a model question to answer before any consumer moves, not a rename.
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
- Retire the legacy `largestCity` member once ranked-target-selection EVALUATION lands.
- Re-home `stronglyRestricted` to a `requires.build` civ-membership gate, when NPC civilizations are wired.
- Move corp-HQ revenue (`HeadquarterCommerces`) with the corporation rework.
- Map the flagged unitcombat remainder — map the obvious, flag the unsure, never blunt-purge
  ([unitcombat-tag-mapping.md](unitcombat-tag-mapping.md)).

## Legacy still breathing — delete it

> The standing rule (purge violently; blast radius is the signal; the worst offenders are the ones OFF the core
> loop) is [roadmap.md § LEGACY STILL BREATHING](roadmap.md). ⚠ KNOWN-INCOMPLETE — legacy found anywhere else is
> killed on the same terms. ⛔ Never record a found legacy surface as acceptable or "kept until X".

- Finish the HALF-CUT accessors: a member whose declaration, serialization and `savemigration.txt` tag are all
  gone, but whose accessors and consumers were left standing in its own class. They are compile errors that the
  per-TU error cap hides, so the census only names them a few at a time — sweep them from the migration ledger
  instead of waiting. `CvUnit`, `CvCity` and `CvPlayer` carry the bulk.
- Repair the `savemigration.txt` REPLACEMENT-OBLIGATION notes that no longer resolve. Each note records WHICH
  named replacement now serves a cut value; where that replacement was archived or never built, the field is
  gone from every save and the value has NO source, which nothing catches ([AGENTS.md](../../../AGENTS.md)).
  The wellbeing cut notes are the known cluster — they name the archived bespoke substrate
  ([superseded-ideas](../../architecture/superseded-ideas.md) #14) rather than what actually serves them now.
- Reconcile the ledger entries that contradict the tree: a tag listed as CUT whose member is still serialized.
  Two are DECORATED per-element arrays inside live enum-remapping loops, which [save.md §3](../../specs/save.md)
  says must never be listed at all (their entries cannot match, so they are inert but false); one is a plain tag
  whose read and write were never actually deleted, so it writes a tag that the reader then drains.
- Move every consumer off the hand-named channel-shaped getters on `CvCity`/`CvPlayer`, then delete the old names.
- Cut the hide-and-seek per-type intensity ACCUMULATORS on `CvUnit` (serialized — the cut carries a
  `savemigration.txt` step; confirm the tag spelling against the stream first). Their replacements are built.
  ⛔ The AI sites SUM inside a loop over every `INVISIBLE_*`, so this is a rewrite, not a rename — a mechanical
  swap would count concealment once per type.
  ⛔ Do NOT sweep the neighbouring `getInvisibleType` / `getSeeInvisibleType` calls: those are live `CvUnit`
  methods sitting in the same blocks.
- Replace the per-type hide-and-seek help text with the detection entry's own render.
- Cut `CvCity`'s other hand-rolled dirty caches — demolition fodder, never conversion targets.
- Retire the direct `gDLL->logMsg` / BetterBTSAI log-helper call sites and the log-level globals they gate,
  wholesale as each domain migrates onto the spine — never tidied in place.
- Delete the legacy `ConstructRequirement` / construct-condition surface once the `requires` RENDERER exists
  (its last consumers are the prereq-block composers).
- Delete `CvPlayer::getBuildingPrereqBuilding` with the last of its text consumers. ⛔ Do not revive the prereq
  table to keep a text line rendering.

## Not built yet

- The PLAYER-ALERT consumer, and the alerts owed to it — they re-attach to the OPERATE CROSSING fact, never
  re-inlined at a mutation site ([event-spine.md](../../specs/event-spine.md)). Expect the owed list to GROW as
  each legacy mutator is cut; add them together on the facts.
- The amenity CONSUMER side: re-point consumers onto the CITY read and retire the per-flag `CvCity` counters and
  their bespoke per-attribute predicates/facts ([contexts.md](../../architecture/contexts.md)).
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
- A home for pedia category / sort metadata ([pedia-read-map.md](../../reference/pedia-read-map.md) finding 4).
- Ranked-target-selection EVALUATION ([parked/ranked-target-selection.md](../parked/ranked-target-selection.md))
  — a ranked entry applies unranked until it lands.
- A DOMAIN event on a game-option flip, if/when WorldBuilder option toggling is in scope.
- The Python data-fetching library (below).

## The GETTER cut — game objects + AI

> Sequencing, and the ban on bending the new surface to fit an old call: [roadmap.md](roadmap.md).
> ⛔ The unit of work is the CLASS of read, never the individual getter — many collapse as the rebuilt infos
> wire through ([DEC-new-getter-surface](../../architecture/decisions.md#dec-new-getter-surface)).

- Move the what-if valuation consumers onto `expected*` — the AI candidate weighting and the build-list hover
  tooltip are ONE call ([patterns.md](../../architecture/patterns.md) THE VALUATION PROTOCOL).
- Decide where the YIELD what-if's supersession-netting lives — the enabler owns `replacedBy`, or the call site
  composes two valuations. ⛔ Not by widening `expected*` with a replaced-buildings argument.
- Watch civic-choice STABILITY now that the cross-category half-value damper is gone (owner: drop it; if the
  problem shows, add it back properly). ⚠ It existed because civic valuations are linearly combined across
  categories, so a building gated by civics in two options could be counted at full value from both. If choices
  start oscillating, the principled fix is a `civics` id set on `CascadeCondDeps` — which is what the removed
  whole-civic-database sweep was reconstructing by hand — never restoring that sweep.
- Build the GATE twin of that overlay — re-evaluating a candidate's `requires` with a hypothetical HAVE injected
  into the eval ctx, which is what answers "would this BONUS let me build X". ⛔ It is NOT the membership
  overlay and must not be folded into it: the bonus axis is GATE-ONLY
  ([enabler.md §8](../../specs/enabler.md) resolved forks), so a bonus changes no membership and the overlay
  refuses one outright. ⚠ The data DOES carry bonus `enables` edges the runtime ignores, so an overlay that
  accepted a bonus would manufacture unlocks the real frontier never grants — which is why the refusal is
  structural rather than a documented caution.
- ⛔ Neither is a VALUATION question, and filing them as one is what sent a reader looking for a magnitude
  machine that was never the dependency: both ask AVAILABILITY under a hypothetical HAVE, which is the
  enabler's ([DEC-enabler-not-cascade](../../architecture/decisions.md#dec-enabler-not-cascade)).
- Re-express the specialist EXPERIENCE reads as the ENTRY-LIST read over the specialist's own authored entries.
  ⛔ Not an arity fix — folding a keyed entry scope-wide is the silently-plausible-wrong case
  ([modifier.md §5](../../specs/modifier.md)).
- Make `CvCity::getNumBonuses` a BARE FETCH of a maintained per-city count ([enabler.md §8](../../specs/enabler.md)
  open item 2). ⛔ Every per-read re-plumbing of this is the wrong axis and has been backed out before.
  ⚠ `CityContext::tradedBonusCount` re-derives on refresh and is on the wrong side of it too.
- Rule on WHICH specialist count the per-specialist commerce/yield bonuses mean, then curate to it. Both counts
  are now answerable — a city-scope scaler reads that city's own specialists, a cross-city one rolls up through
  the tally — so this is purely a DATA question. The deposits author at EMPIRE scope with a bare `per`, and a
  bare `per` takes the DEPOSIT's scope ([json.md §3.7](../../specs/json.md)), so they currently ask for the
  EMPIRE's specialists and every city then experiences that whole amount. The legacy mechanic instead gave each
  city its OWN specialist count. Matching it needs the scaler to name the city scope explicitly, which the
  grammar already allows; keeping the empire reading is a deliberate behaviour change to state, not to drift
  into ([validation.md](../../specs/validation.md): the spec leads). Either way the curator + regen ride the
  ruling ([DEC-recurate-on-decision](../../architecture/decisions.md#dec-recurate-on-decision)).
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
- Restore a home for the city's TYPED FREE specialist counts. The getter and setter reference a member the city
  no longer declares, and the setter computes its new value, fires its side effects and then stores nothing —
  so the typed-free ledger reads empty whatever is granted into it. ⛔ Decide it WITH the free-specialist
  AMOUNT item below rather than separately: the two are one seam ([modifier.md §6](../../specs/modifier.md)),
  and re-adding the member alone would restore a legacy accumulator.
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
  state, `getHappinessTimer`, the `CvPlayer` unit-upkeep family, and the FREE-SPECIALIST AMOUNT.
  ⚑ The free-specialist amount is CASCADE RESPONSIBILITY (owner) — so the empire/area figure is NOT an info read
  to restore, and a per-scope getter must not be added to the building info to satisfy the call sites. What is
  missing is the cascade read: `MODFAM_FREE_SPECIALISTS` exists in the vocabulary and the building materializes
  only its `city.any` leaf, so nothing answers the summed amount over the scope chain. Until it exists the
  push accumulators (`CvPlayer::m_iFreeSpecialist` + the area twin, fed from building/civic/trait `processX`)
  and their consumers dangle — that is the census working ([modifier.md §6](../../specs/modifier.md): the AMOUNT
  is the cascade's half of the two-part seam, PLACEMENT stays the engine's).
  ⚠ The accumulators are NOT serialized, so this cut carries no `savemigration.txt` step.
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

- Build the ONE data-fetching library toward COMPLETE (its end state, not a gate on cutting). Build it for the
  pedia (a SHAPE oracle, NOT a coverage oracle — the appendix is enumerable).
- Shoot the dead `Cy*` bindings on sight as the compiler names them.
- Serve enum resolution AND EXTENSION as a first-class operation — BUG reaches three engine enums only that way
  and MINTS new members at runtime, so a library without it forces the banned reach-around.
- REBUILD the emptied `CvGameTextMgr` composers on `appendEntryLines` + the requires block composer. The bodies
  that read legacy getters were CUT, not migrated, so this is writing them fresh — never "moving" one, and never
  restoring a body to see what it used to say.
  ⚑ DEMAND-DRIVEN and END-STAGE (owner): the tooltip set comes from playtests and community requests, and a red
  tree renders nothing, so appearance is unverifiable guesswork until it is green
  ([patterns.md](../../architecture/patterns.md) THE DIVISION OF LABOUR). ⛔ Do not open a layout pass.
  ⛔ A composer's acceptance test is that it reads NO legacy getter, never that it reads nicely.
  ⛔ The four WELLBEING composers are NOT `appendEntryLines` targets — a realized per-scope aggregate has no
  entry list to render from, and is a BLOCK. A bonus's "what needs me" block reads the bonus's own
  `EDGEF_RELATED` ([DEC-one-reverse-view](../../architecture/decisions.md#dec-one-reverse-view)), never a
  database scan; a promotion's per-unit lines read `CvPromotionAccrual::sum`, never a re-rolled rung loop
  ([DEC-single-implementation](../../architecture/decisions.md#dec-single-implementation)).
- Re-point the unit consumer getters onto `resolvedValue()`.
- Re-point the unit power-value plane's readers.

## Triggers / grants

- Build START PACKAGES: the entity type, its folder + prefix + repo row + manifest, and the shipped defaults
  ([triggers.md](../../specs/triggers.md)). Two content decisions ride it — which units the defaults name, and
  NPC/barbarian starts.
- Retire the engine start selection (the whole-database scan + AI scoring, and the per-role starting counts)
  once packages carry the identities.

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

- Give the UNIT frontier an incremental path — events that do not affect it currently blanket-dirty it, forcing a
  full re-walk. The operating-building fixpoint rides the same triggers.
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
- Give the OBJECT a player-level held-building aggregate for the HELD-building sweeps
  ([tally.md](../../specs/tally.md): let an object care about itself) — never a side-store.
- Restore the members `AI_techValue`'s remaining sweeps read, then drive them from the tech's own edges.
- Retire the active-set work-list ripple: the fact that justified it is now emitted, so the parallel propagation
  machine goes ([DEC-single-implementation](../../architecture/decisions.md#dec-single-implementation)).
  ⚠ `emit()` dispatches SYNCHRONOUSLY, so an event chain recurses on the call stack where the work-list iterated
  — design for that; it is not a reason to keep the machine.
  ⛔ Its runaway cap claims to "self-heal at the slice boundary"; that rebuild was REMOVED and nothing heals it.
- Converge the operate reverse index's two PER-ID buckets onto `EDGEF_REQUIRED_BY`.
  ⛔ The axis-flag lists and the PROPERTY band index are NOT convergence targets — a coarse list matches a coarse
  event, and the reverse pass deliberately excludes engine tokens and the plot substrate.
- Close the remaining [enabler.md §8](../../specs/enabler.md) items: residency/counting, plot-group membership
  not trusted from a save, the load-end dormancy fixpoint, the dynamic operate axes.

## Tree / include hygiene

- Retire the `CvInfos.h` umbrella — a hand-careful pass; the lessons and hard bans are in
  [AGENTS.md](../../../AGENTS.md) Conventions §Design.
- Run the dead-code / dead-XML pass — tooling generates CANDIDATES only; every removal verified against
  source/data and test-loaded, one subsystem at a time.
- Route the `[CTB/work/intransit]` block onto the same gate as every other CTB line so it reaches `/events`.

## Green-up (after the structure, never ahead of it)

- Engine-repair debt: the bare Engine includes · `CvOutcomeMission::mapFrom` · the property-manipulator helpers ·
  `CvCity.h`'s functor row.
- The vocabulary TXT keys (one per family/kind/predicate/token) — polish on a working machine; the renderer's
  spell-back fallback is the accepted output until then.

## Spec gaps to close

- Give the mod-data design invariants a spec home — a requirement may not unlock after the thing requiring it;
  replacements are explicit, never implicit; a replacing entity must be better. The checks are gone; the
  invariants belong in [json.md](../../specs/json.md).
