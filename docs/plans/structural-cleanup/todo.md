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

- Rule on the river-attack term for a CITY defender (`CvUnit::getDefenderCombatValues`). Its two branches
  disagree: attacking across a river hands an ordinary defender `-RIVER_ATTACK_MODIFIER`, while the city branch
  is `min(0, riverDefensePenalty - RIVER_ATTACK_MODIFIER)` — capped at zero, so a city defender can never
  receive that bonus and the building value only bites if authored below the define. Every authoring is above
  it, so the term is inert whatever its scale. ⛔ Not an agent fix: changing it changes combat, which
  [validation.md](../../specs/validation.md) makes a per-case owner authorization, so the choice (drop the
  mechanic, or correct the branch to reduce the crossing penalty) is yours.
- Build RANKED-UP Size-Matters units from the build menu, so a late-game player is not merging hundreds of units
  by hand. The model is settled ([json.md §9](../../specs/json.md)) — this closes by IMPLEMENTING it, never by
  re-deciding it.
  ⚖ Not wanted yet (owner) — waits until wanted; nothing else waits on it and no half is in the tree.

## Data — curator

- Attach the ruling-16 trigger-plane set (`survivor`, `cityCapture`, `combat.subdueAnimal`,
  `combat.nukeInterception`, `diplomacy.techShare`) to its trigger's `chance`
  ([triggers.md](../../specs/triggers.md)).
  ⚠ `techShare` additionally needs its KIND retired: unlike the rest of the set it was minted as
  `DIPLOMACY_TECH_SHARE`, so the re-home is a curator change AND a vocabulary removal.
- Give the §3.9 entry grammar a payload-less form so a carrier can state a cargo RESTRICTION with no capacity of
  its own, then author the flagged carriers ([modifier.md §6](../../specs/modifier.md)).
  ⚠ Settle in-game first whether the ancient transports are civilians-only — unconfirmed, do not author against
  a guess.
  ⛔ Do the grammar + authoring FIRST. Only then: make the load/board gates evaluate the `unit:`-qualified
  `cargo.space` entries per candidate (today only the unqualified capacity sum is read), and strip the
  promotion-side WHAT overrides (domain / special-unit / SM-not-special) — CURATOR + INFO + ENGINE together,
  since a 0-capacity carrier needs the payload-less entry to keep its restriction once they're gone.
- Re-home the remaining `identity` EFFECT keys to the block that already exists for each
  ([json.md §7](../../specs/json.md)): constraints → `requires`/`allowed`; `diploVoteType` → the top-level
  `voteSource` section (and rename the getter off the legacy XML tag); `tradeable` → the `canTrade` block;
  `advancedStart` → resolve the curator's parked flag; `base.airCombat` → the `strength` family, where every
  other unit's base value already lives.
  ⚠ `espionagePoints` rides the missions/`CvOutcome` carve-out — its channel is settled, only its authoring home waits.
- Author the leader→trait assignments. The chain is wired and the slots are authorable; the CONTENT is
  community-owned, so this closes by AUTHORING and never by reconstructing the tables the curator dropped.
- Author per-leader `ai.personality.researchSearchDepth` ([enabler.md §8](../../specs/enabler.md)). Same shape as
  the trait assignments: the read is wired and an unauthored leader takes the default, so this closes by
  AUTHORING, never by touching the enablement valuation.
- Retire `MISSION_RANGE_ATTACK` (`canRangeStrike` / `rangeStrike` / `INTERFACEMODE_RANGE_ATTACK`) — a SECOND
  ranged-attack mechanic sitting beside the removed ranged bombard in every mission switch, which the bombard cut
  did not cover. `canRangeStrike` refuses `DOMAIN_AIR`, so it is the non-airplane class
  [superseded-ideas #24](../../architecture/superseded-ideas.md) already rules out.
  ⛔ Do not assume the bombard cut reached it.
- Rename the `dcmFighterEngage` skill and the `DCM_FIGHTER_ENGAGE` global off the mod-provenance prefix
  ([skills.md](../../specs/skills.md)) — the mechanic stays, only the name changes.
- Retire `DOMAIN_IMMOBILE` — immobile is not a domain ([json.md §7](../../specs/json.md)), a domain is the medium
  a unit operates in.
  ⚠ Units still author it, so re-author the data FIRST; only then cut the consumers: the enum entry, ~21
  engine/AI sites, `CIV4DomainInfos.xml`, its game text, and a Python read. It is terminal in the enum (before
  `NUM_DOMAIN_TYPES`), so removal shifts no other id.
  ⛔ Several sites treat it as a live case, not a dead one — read each site, never a delete-the-case sweep.
- Decide what an EMPIRE-scope `range` deposit would mean, if one is ever authored — none exists today, every
  authored `range` is unit-scope.
  ⛔ Not a reader to build ahead of data ([DEC-no-guessing](../../architecture/decisions.md#dec-no-guessing)); it
  closes through `curate_trait` + a regen ([modifier.md §4](../../specs/modifier.md)), never by hand-editing the
  emitted JSON.

## Legacy still breathing — delete it


> The standing rule (purge violently; blast radius is the signal; the worst offenders are the ones OFF the core
> loop) is [roadmap.md § LEGACY STILL BREATHING](roadmap.md). ⚠ KNOWN-INCOMPLETE — legacy found anywhere else is
> killed on the same terms. ⛔ Never record a found legacy surface as acceptable or "kept until X".

- Serve the team improvement-yield GRANT through the new Python surface. The wonder events call it from
  `CvEventManager` and the binding they called is gone — by design
  ([DEC-cy-not-fixed](../../architecture/decisions.md#dec-cy-not-fixed)): it comes back as the library's own
  read, never a restored `CyTeam` binding. Until then the engine stores and serves the value but nothing can
  grant one.
  ⚑ Grants recorded without an event id carry `-1` (unattributed) by design — the events rework threads the
  real id; the store already carries the field for it.
- Repair the `savemigration.txt` REPLACEMENT-OBLIGATION notes that no longer resolve
  ([save.md §3](../../specs/save.md)). The wellbeing cut notes are the known cluster to start from — they name
  the archived bespoke substrate ([superseded-ideas #14](../../architecture/superseded-ideas.md)) rather than
  what actually serves them now.
- Serve the PER-SCALER RATE for a given count-key — "how much does this source deposit per unit of
  `per: {type: X}`". ⛔ Not `keyedTargetSum`: that matches a NAMED TARGET, this filters on the `per` key and
  wants the value UNSCALED (the rate, not rate × current count). The live demand is an AI valuation — "what
  would one more plot of this improvement give me" — which the presence-pinning `CvCascadeHypothetical` cannot
  express either.
- Serve the ATOM-CROSSING WHAT-IF — "what would this scope's already-held deposits deliver if predicate X
  became true" — the atom-axis sibling of the per-scaler rate above. The apply side is built and live
  (`DepositIndex::gatedByPredicate` → `mc_applyGated`, plane C); what is missing is the VALUATION read over
  the same index, resolving each gated entry as-if-held for the sources the scope actually holds
  (`hasAppliedSource` is the O(1) liveness test the apply walk already uses).
  ⚑ The live demand is a candidate that flips a STATE rather than depositing: pricing a power plant is
  "what do this city's `HAS_POWER`-gated deposits pay once it is powered". Its accumulator is cut, so the
  term is currently absent rather than wrong.
  ⛔ `CvCascadeHypothetical` cannot express it — it pins entity ids per edge bucket, and a bare predicate is
  not an entity ([contexts.md](../../architecture/contexts.md) § THE EVAL CTX). Widening it to carry
  predicates is a STRUCTURE call, not a sweep: settle the shape before building either read, since the two
  differ only in which index they walk.
- Converge `CvCity`/`CvPlayer` on the read surface we actually NEED, understandably structured: move consumers
  onto the group reads, serve genuine demand on the coherent surface, and the hand-named channel-shaped names
  fall away as nothing needs them ([patterns.md § THE TWO READ ROLES](../../architecture/patterns.md), owner).
- Retire the direct `gDLL->logMsg` / BetterBTSAI log-helper call sites and the log-level globals they gate,
  wholesale as each domain migrates onto the spine — never tidied in place.

## Not built yet

- **Teach the unresolved-FK census the catch-all-token vocabulary** ([json.md §3.1](../../specs/json.md)) so it
  stops reporting engine tokens (`CITY`, `TEAM`, `ERA`, the slider rates, `CULTURE_PERCENTAGE`,
  `DISTANCE_TO_GOVERNMENT_CENTER`, `CORPORATION_LEVEL`, …) as unresolved ids — they bury the genuinely unresolved
  ones. ⚠ Settle the `TAG_*` entries in the same pass — they are classification ids minted at load, same
  resolvability question.

- **De-serialize `CvPlayer::m_bonusExport` / `m_bonusImport`.** Re-derive both maps from the player's held deals
  on load instead ([enabler.md §8](../../specs/enabler.md) — the deal stays serialized, the derived counts do
  not). Uniform soft-remove cut: delete member + read + write, name the tags in `Assets/savemigration.txt`, no
  `WRAPPER_SKIP_ELEMENT` ([save.md §3](../../specs/save.md)).

- **Cut the DEFERRED BONUS-PROCESSING BRACKET together with `processBonus`.**
  `CvCity::startDeferredBonusProcessing` / `endDeferredBonusProcessing` exist only to keep the legacy per-bonus
  apply (`processBonus`) off the intermediates of a merge or load-time rebuild — once nothing calls
  `processBonus`, the bracket has no remaining purpose.
  ⛔ Do not cut it first: cutting it while `processBonus` is still live would run the legacy fold on every
  intermediate of a rebuild.

- **Cut the citizen valuation's WHIP term (`iSlaveryValue`)** — the whip/draft trade it models does not hold at
  S2S's real costs ([citizen-assignment.md § THE WHIP TERM IS NEVER WORTH TAKING](../../reference/citizen-assignment.md)).
  ⚠ It is the tail, not the head: whipping/drafting are what want revisiting, and this term goes with whatever
  that decides — do not tune it in place.

- **Watch the five decisions `AI_countGoodTiles` now feeds** — the ×100 conversion made them live for the first
  time ([citizen-assignment.md](../../reference/citizen-assignment.md), the `AI_getPlotMagicValue` measured
  instance). ⛔ Not a scale defect to repair — it is new behaviour to OBSERVE. Judge it on the `[CIT/assign]`
  census from a real save, never against a remembered figure
  ([DEC-baseline-is-a-smell-test](../../architecture/decisions.md#dec-baseline-is-a-smell-test)).

- **Wire `AI_setAssignWorkDirty` onto the event spine, per the ruled trigger set**
  ([citizen-assignment.md § Dirtying the assignment](../../reference/citizen-assignment.md)) — replace the
  hand-wired call sites and the unconditional per-turn sweep with spine listeners for that set
  ([DEC-flag-is-fossil](../../architecture/decisions.md#dec-flag-is-fossil)).

- **Retire the `CARGO_NAVAL` / `CARGO_MISSILE` kinds onto the tag-predicate shape** — `CvUnit::cargoSpace`'s
  "+N hold when the CARRIER is `DOMAIN_SEA`" / "when its cargo is `SPECIALUNIT_MISSILE`" author as a plural
  `units` target with a tag predicate instead (`cargo.space.empire.units {IS_SEA}`), per the ruled shape
  ([modifier.md §6](../../specs/modifier.md)).
  ⛔ Do not carry the `navalCargo` name forward — it is an empire-wide hold bonus scoped by the carrier's
  domain, not naval cargo or a naval-unit hold.

- **Share `InfoValuation::plotScaledYield`'s two operands with the what-if plot read.** `plotBaseYields` — the
  isolated plot-as-base calc every what-if goes through — does not take them yet, so a what-if answers the
  pre-scaling number while the live plot answers the scaled one
  ([DEC-single-implementation](../../architecture/decisions.md#dec-single-implementation): the arithmetic is
  shared, the operands must be too).
- **Land the UNKINDED MEMBERS — authored deposits `readJson` drops.** ⛔ Read the `[READJSON] unkinded-member
  <family>.<member>` census before anything else on this — it is the authoritative worklist, one grep of the
  load log. Each one resolves to exactly one disposition:
  - **the trigger-plane set** (`cityCapture.resistance`, `combat.subdueAnimal`, `combat.nukeInterception`, …)
    stays deliberately unkinded ([triggers.md](../../specs/triggers.md)); minting a kind for one is the banned
    move.
  - **⛔ a CONDITION-AS-MEMBER rollerskate, which is what most of them turn out to be.** A member answering
    WHERE or WHAT-IS-COUNTED rather than WHICH COMPONENT is never a kind (§6's triage test): it re-authors as
    the predicate or qualifier that already says the same thing, at the CURATOR, and no kind is minted.
    ⚠ **The census cannot tell this class from a genuinely missing kind** — both surface as the same
    `unkinded-member` line — so run the triage test on every entry before minting anything. ⚑ Where a sibling
    curator emits the same mechanic correctly, that is the shape to copy: the outlier is the defect, not the
    vocabulary.

- Serve a city's OFFERED RESOURCES, and give the city screen a VICINITY tab showing them (owner) — no readable
  list exists today for what a city's plot group supplies.
  ⛔ READ only — nothing mirrors onto the city ([enabler.md §8](../../specs/enabler.md) RESIDENCY); a stored
  per-city copy is banned.
  ⛔ VICINITY unions two independently-owned halves per [contexts.md](../../architecture/contexts.md) THE
  VICINITY SPLIT — do not mirror one half onto the other.
  ⛔ Needs a LIST accessor (`getVicinityBonuses`) — `hasVicinityBonus` is per-bonus only, so a tab built on it
  sweeps every bonus id per render ([DEC-one-reverse-view](../../architecture/decisions.md#dec-one-reverse-view)).
  ⛔ Serve through the NEW Python surface, never a revived `Cy` binding
  ([DEC-cy-not-fixed](../../architecture/decisions.md#dec-cy-not-fixed)).

- Convert the PLOT-YIELD AI CALLERS, the residue of that plane's cut. `calculateNatureYield` still has AI
  callers reading legacy `nature + calculateImprovementYieldChange` math (improvement valuations, feature/hill-
  food heuristics, the `bIgnoreFeature` CHOP what-ifs) that no stored segment can answer.
  ⛔ Do NOT re-point them at the segment — that wires legacy AI calc onto new machinery. They convert with the
  valuation, or they go.
  ⚑ `calculateImprovementYieldChange` collapses to ONE `InfoValuation::plotOwnYield(improvementModifiers,
  family, ctx)` with a plot-anchored ctx (`fillEvalCtxAtPlot`); its `bOptimal`/`bBestRoute` args are the
  AS-IF-HELD what-if the valuation already takes, not new parameters.
  ⛔ Do not build a per-route × per-improvement yield term into the valuation — route valuation is ruled
  movespeed-only ("AI valuation of ROUTES", AGENTS.md), never a yield tradeoff.
  ⚠ Separately, on the DATA side: `RouteYieldChanges` (route entries keyed by improvement,
  [modifier.md §4](../../specs/modifier.md)) currently reach no plot yield — `plotBaseYields`'s route leg is
  `plotOwnYield`, the route's untargeted output only. Serving it needs the improvement's FK passed beside its
  modifiers so `keyedTargetSum` can resolve the keyed entry.

- Serve the SLIDER-SCALED SHARE of a channel — the part of a city's realized happiness contributed by a deposit
  carrying `per:{CULTURE_RATE, each:100}` ([json.md §3.1](../../specs/json.md)). The share resolves at gather;
  no point read or valuation exposes it, so a consumer can neither discount it nor value one slider point.
  ⚑ Two AI reads need it: the civic valuation (discounting slider-removable happiness) and the culture-slider
  chooser (today only the culture-victory branch moves the slider; it cannot otherwise answer "how much RATE
  clears this city's anger").
  ⛔ Not a getter per channel — it is the `per`-scaled share of a group the read already hands out.

- Route every fat curator's `--write` block through `wipe_entity_json` — ~20 still write in place
  (`curate_civic`, `curate_era`, `curate_handicap`, and their kin; census: grep the `--write` blocks for the
  missing call). A bespoke write skips the drop-before-rewrite AND the additions-overlay registration
  ([curators/README.md](../../specs/curators/README.md)) — inert only while the type has no `_additions` file,
  and the first overlay someone authors for one of those types silently drops on the next regen (the
  `curate_unit` settler-overlay case, now fixed there and in `curate_unitcombat`).

- Evaluate the ROUTE's tech-gated movement tail at the movement read. A route's base move cost and its
  tech-gated delta are ONE slot (the bare number plus a conditioned entry, [modifier.md §6](../../specs/modifier.md)),
  but `CvPlot::movementCost` takes the point read (unconditioned entries only), so the gated delta never applies.
  ⛔ Needs the conditioned tail evaluated against the asking team, the same shape every other conditioned read uses.

- Apply the PER-CITY GATES AT THE COMBINE. [modifier.md §1](../../specs/modifier.md) specifies the realized
  value as the scope-package sum with per-city gates (state-religion-in-city, coastal, connected, area
  membership) applied live at the combine; `InfoValuation::rolledLegsAtCity` is a bare package sum, so an
  upper-scope deposit gated on a city predicate never fires.
  ⛔ Not a package-rebuild question — an empire package has no city bound, so the condition can't be evaluated
  when the package is built; it belongs at the COMBINE, where the asking city is known.
  ⚠ Until it lands the affected deposits are unserved; `savemigration.txt` carries the replacement obligation
  for the one whose accumulator is already cut.

- Charge the improvement UPGRADE cost from somewhere that can tell an upgrade from a build. Nothing charges it
  today — the only implementation sat in the improvement-set choke point, which can't tell the two apart (a
  worker building a farm on a seed camp paid twice) and fired on unowned plots with no owner to charge.
  ⛔ Wants the intermediate that only the upgrade path calls, never the shared setter.

- Give GAME OPTIONS and CONFIG VALUES a standardized read surface (owner: *"having standardized getters for
  gameoption, and config values is not a bad idea"*), shaped like the existing `CvGameSpeedScale` /
  `CvTraitSelection` pattern ([engine.md § Consuming-system calcs](../../reference/engine.md)): a
  purely-organizational static-methods class, one getter per group.
  ⛔ Keep the three kinds SEPARATE, never one `getSetting` — GAME OPTION (`GAMEOPTION_*`, fixed at setup,
  [DEC-entity-gate](../../architecture/decisions.md#dec-entity-gate)), CONFIG (authored data on
  eras/gamespeeds/handicaps, read from its sources — [state-repositories.md](../../architecture/state-repositories.md)
  WORLD is CONFIG), and LIVE (user-changeable mid-game, incl. `MODDERGAMEOPTION_*` despite the name —
  [event-spine.md](../../specs/event-spine.md)) — nothing STATIC may depend on a LIVE option.
  ⛔ Add a readJson check refusing a `MODDERGAMEOPTION_` condition, so the split is unsayable to violate.

- The PLAYER-ALERT consumer, and the alerts owed to it: "power restored" (`TXT_KEY_MISC_POWER_RESTORED`, now
  hangs on `SEVT_CITY_STATUS_REMOVED` carrying `CITYSTATUS_POWER_DISABLED`); the CAN_RETRAIN/NO_RETRAIN pairs
  the promotion KEEP gate used to emit per failing axis (terrain / feature / plot bonus /
  improvement-or-local-building / promotion prereq, plus two more the axis list does not name); and "your
  building was obsoleted" (`SEVT_CITY_BUILDING_OBSOLETED_ADDED / _REMOVED`, emitted for exactly this). All are
  authored and were rendering — a real loss of player-facing information.
  ⛔ They do NOT come back as a per-axis walk beside the gate (rebuilds the legacy battery) — re-attach to the
  fact, never re-inlined at a mutation site ([event-spine.md](../../specs/event-spine.md)).
- Decide WHERE the citizen-assignment re-check is asked for — a CALL-SITE question, never a removal:
  `AI_setAssignWorkDirty` is called from across the engine while `AI_updateAssignWork` re-runs the FULL
  assignment for every marked city, so the flips are a turn-time cost in their own right. The ruled trigger set,
  the canWork gate exceptions, the unit-movement carve-out and the census instrument are all specced in
  [citizen-assignment.md § Dirtying the assignment](../../reference/citizen-assignment.md) — implement that set,
  do not re-derive it.
  ⛔ Do NOT re-add an ungated flip to replace a cut maintainer's gated one.
- The amenity CONSUMER side: re-point consumers onto the CITY read, and retire the per-flag `CvCity` counters
  and their bespoke per-attribute predicates/facts ([contexts.md](../../architecture/contexts.md)). The apply
  sites are the `process*` functions — `changeZoCCount`, `changeProtectedCultureCount`,
  `setWorkableRadiusOverride` — against keys the data already authors (`zoneOfControl`, `protectedCulture`,
  `adds3rdRing`).
  ⚑ POWER is already converted and is the pattern to copy (contexts.md § POWER IS AN AMENITY).
  ⛔ What's left is the GENERALIZATION — one parameterized amenity fact carrying the id, replacing the
  per-attribute ones as each converts; do not grow a second bespoke fact per key.
  ⚠ Check each counter's FEEDERS first — power's conversion was a read-swap (only a dead `CyCity` binding still
  wrote it); the others may not be.
  ⚠ The workable-radius counter is a live BUG, not just an unmigrated shape: it is a plain SET rather than the
  id→COUNT refcount, so a city holding two radius grantors and losing one drops the override to 0 and loses a
  ring a live grantor still justifies.
- Re-fold a conditioned amenity on a BUILDING grantor when its condition moves (the empire half is covered). It
  wants the condition-dependency route the modifier consumer already derives.
- Decide whether `CvTraitInfo` / `CvTechInfo` carry `m_amenities` and its fold leg. ⚑ Answerable NOW: readJson
  reports an entity authoring a block its type cannot hold, so the tree already says whether any does — read it,
  then either wire the leg or record that the block does not belong on those types.
- The endpoint route table, beyond the four decomposition censuses. It routes through the ACCESS SURFACE, which
  does not exist yet; building it is the actual work item here
  ([roadmap.md § THE OPEN ITEM — the ACCESS surface](roadmap.md#-the-open-item--the-access-surface)).
- Give the ctx-taking KEYED SUM (`keyedTargetSum`) the scope filter its collecting twin (`collectKeyedTarget`)
  and the point form (`keyedTarget`) both carry. `collectKeyedTarget` takes an `iScope` (-1 = any) because the
  same family+target is authored at two scopes with two different consumers; `keyedTargetSum` — the one serving
  the CONDITIONED tail through the ONE evaluator — takes none, so a caller that must pin a leg to one scope
  falls back to the unconditioned collect and silently loses every gated row.
  ⚑ Live case: the free-specialist split authors BOTH a city-scope row and an empire-scope row on the same
  building, so the empire read must pin the scope or the two legs double-count.
  ⛔ Not a second read — one parameter on the existing one, matching the other two's spelling.
- Serve the EMPIRE-scope `buildRate.<scope>.units` rows a capped WONDER authors (`{enabled: IS_SPACE}`). The
  city-scope rows are served off the city's operating buildings, but an empire row means faster in EVERY city of
  the owner, so it is answered PLAYER-side ([modifier.md](../../specs/modifier.md) §5) — and no player-side read
  walks the owner's buildings for one. A CONDITIONED deposit cannot ride the empire package
  ([modifier.md](../../specs/modifier.md) §5: anything qualified is the valuation's), so the package is not the
  answer either; what is missing is the read.
  ⛔ Do NOT re-scope the data to city — a wonder boosting only its own city is a different mechanic.
  ⛔ Do NOT walk every city's operating buildings per call: that is `O(cities × buildings)` inside a production
  decision, the O(what EXISTS) shape the maintained sum exists to delete
  ([state-repositories.md](../../architecture/state-repositories.md)).
  ⚠ Its old consumer was `getProductionModifier(ProjectTypes)` gated on `isSpaceship()`, which
  [modifier.md](../../specs/modifier.md) §4 rules DOES NOT APPLY (vanilla space-VICTORY parts, not space units),
  so this is a mechanic to SERVE correctly for the first time, never a regression to restore.
- Ranked-target-selection EVALUATION ([parked/ranked-target-selection.md](../parked/ranked-target-selection.md))
  — a ranked entry applies unranked until it lands.
- The Python data-fetching library (below).

## The game-object READ SURFACE — game objects + AI

> The deliverable is the SURVIVING surface — just the getters we need, understandably structured — never "a
> getter cut" for its own sake; the legacy deletion is the consequence of consumers moving
> ([patterns.md § THE TWO READ ROLES](../../architecture/patterns.md), owner). Sequencing, and the ban on
> bending the new surface to fit an old call: [roadmap.md](roadmap.md).
> ⛔ The unit of work is the CLASS of read, never the individual getter — many collapse as the rebuilt infos
> wire through ([DEC-new-getter-surface](../../architecture/decisions.md#dec-new-getter-surface)).

- Give the CityContext its id-keyed RADIUS DICTIONARIES, then move the improvement count onto them.
  `CityContext::improvedPlotCount` still walks `CvCity::countNumImprovedPlots` per call (`CityContext.cpp`); the
  dictionary is the standing target ([contexts.md](../../architecture/contexts.md)) and this read is the one
  that wants it.
- Fold the city's TYPED-FREE specialists into `CityContext::specialistCount` — it still answers the ASSIGNED
  population alone (`m_city->getSpecialistPopulation()`), and the legacy multiplier counted both. The
  prerequisite is done: `CvCity::getFreeSpecialistCount` is now the typed-free ledger's home (derivable +
  one-shot halves); the fold itself has not been made.
  ⚑ The shape is already ruled ([tally.md](../../specs/tally.md)): give the OBJECT the aggregate and forward it
  through `CityContext`, never a tally side-store.
- Make the empire GREAT-GENERAL rate a RECEIVER SUM over the player's cities, not an empire-package read.
  `CvPlayer::getScalars`/`InfoValuation::realizedAtEmpire` reads only the empire-scope package, while the
  building deposit (`SCALAR_GREAT_GENERAL_RATE_DOMESTIC`) is authored at CASC_SCOPE_CITY — a city-scope deposit
  that never rolls up (magnitudes flow DOWN the scope spine, [modifier.md](../../specs/modifier.md)).
  ⚑ Great general is NOT great people (owner): great PEOPLE accrue per city, while great general points sum from
  cities plus battlefield experience into the player's own counter — the cross-scope receiver shape
  ([state-repositories.md](../../architecture/state-repositories.md)).
  ⚠ Until it exists the empire read MISSES the city-authored building deposits — a wrong number, not a dangling
  site.
  ⛔ Do NOT re-scope the building data to empire (the city authoring is correct) and do not touch great people's
  own city/empire split — it is right as it stands and out of scope.
- Hoist the per-commerce valuation in `getBuildingCommerceValue` — it runs once per (candidate × channel): both
  `kBuilding.expectedYieldModifiers(...)` and `getYields(aiRealizedYields)` inside it depend only on the
  candidate and the city, not on the channel `iI`, yet the caller (`AI_getBuildingCommerceValue`) invokes it once
  per commerce channel (up to 5×), recomputing the same result each time.

## Stage 4 — the Python surface

- **Re-home the UNIT / BUILDING / PLAYER reads onto their own accessors, and finish dissolving the flat state
  class** ([DEC-accessor-homing](../../architecture/decisions.md#dec-accessor-homing)).
  ⚖ **OWNER-RULED ORDERING: this comes AFTER things work.** *"That some getters live on CyState or whatever is
  less of a priority than things working; re-pointing that is a trivial job for after we are done."* Which
  accessor a served read sits on is homing, and homing is corrected wholesale in the scheduled pass
  ([patterns.md § THE ORGANIZING PASS IS SCHEDULED](../../architecture/patterns.md)). ⛔ It does NOT license
  leaving a read UNSERVED — serving a call site that raises is the functional work and is never this item.
  A game object's data is
  read from its OWN accessor; a method whose name carries another object's noun is homed wrong by construction,
  and the flat address-keyed class reproduces the getter spaghetti and unreadable provenance the boundary exists
  to end ([patterns.md § THE PYTHON READ BOUNDARY](../../architecture/patterns.md)). The CITY pass is the worked
  precedent; each of these is the same shape.
  - ⛔ **It is a MOVE, never an addition.** The state class loses each read as the accessor gains it, or both are
    live at once — the two surfaces the one-surface ruling forbids. Re-point every consumer in the same pass.
  - ⛔ **Not the legacy per-field contract restored** ([DEC-new-getter-surface](../../architecture/decisions.md#dec-new-getter-surface)):
    what an accessor carries is the coherent GROUP reads and named concepts, homed on their own object.
  - ⚠ **UNITS have no accessor to move to yet** — they are the deliberate FUTURE role-specific scope
    ([contexts.md](../../architecture/contexts.md)) and `CyUnit` carries only its identity set, so the unit plane
    is blocked on that scope being taken, not on the re-home. What stands on the flat class meanwhile is the NEXT
    pass, never a sanctioned residue.
  - ⛔ **THE WRAPPER'S LEGACY DECLARATIONS ARE KILLED ON SIGHT, DECL AND BODY (owner)** — not only the ones a
    re-homed name collides with, and not scheduled as a later tidy-up
    ([patterns.md](../../architecture/patterns.md)). An unpublished legacy method is the per-field contract still
    written down, which is what makes "just publish what is already declared" look cheap exactly when the new
    surface arrives. ⇒ The wrapper converges on the identity set PLUS the new reads, and nothing else.
    ⚠ Keep the `class_<>` registration and the identity set (the kept engine→Python direction needs both), and
    let the COMPILER name anything the engine itself calls on the wrapper.
  - ⛔ **A DEF COUNT IS NEVER THE SIZE OF A PASS (owner): much of the surviving player surface exists only to feed
    OLD LOOPS and is not needed.** A whole-registry sweep is the actual defect
    ([patterns.md](../../architecture/patterns.md)), so it converts to the maintained set / the entity's own
    compiled entries / its reverse edge families — and the per-id endpoints it was walking DIE WITH IT rather than
    being re-homed. ⇒ Size each pass by what survives once its loops convert, never by counting what is published
    ([DEC-new-getter-surface](../../architecture/decisions.md#dec-new-getter-surface): a DELETION list plus a
    COVERAGE checklist, never a per-getter worklist).

- **Serve the domestic advisor's per-city COLUMN TABLE as a columnar payload, and drop its `eval` dispatch**
  ([python-read-map.md §5.2](../../reference/python-read-map.md)). The table holds an engine method NAME per
  column and builds the call as a string, so the column set is invisible to every grep — and it does not close by
  swapping the `eval` for a `getattr`: most of the names it dispatches are legacy PER-CHANNEL getters
  (`findYieldRateRank`, `getCommerceRate`, `getPlotYield`) that the new surface answers as GROUPS, and a
  string-built call cannot express indexing into one.
  ⛔ Do NOT re-add a per-channel getter to make the strings resolve — that is the shape the surface exists to
  delete ([DEC-new-getter-surface](../../architecture/decisions.md#dec-new-getter-surface)); the read-map's own
  answer is one columnar per-entity payload over a city set, which also collapses the table's N reads per city
  into one crossing.
  ⚠ The bare `except:` wrapping the draw loop swallows exactly the failure this would surface — remove it with
  the dispatch, so a dead column reports instead of rendering blank.

- **Verify map generation actually works — start a NEW GAME.** Nothing on the standing save exercises
  `CvMapGeneratorUtil.py` (the DLL's map-gen fallback, [engine.md](../../reference/engine.md)) or the
  game-start grants (free techs/units/gold, `freePopulation`, `FreeStartEra` —
  [legacy-grant-apply-sites.md §5](../../reference/legacy-grant-apply-sites.md)); only a new game observes them
  ([DEC-done-is-observable](../../architecture/decisions.md#dec-done-is-observable)).
  ⛔ Include `PrivateMaps/` in the sweep — it sits outside `Assets/`, is easy to miss, and reads the same named
  accessors as everything else ([python-read-map.md §7 ruling 1](../../reference/python-read-map.md)).

- **Build the keyed twin of the `expected*` valuation** ([modifier.md §5](../../specs/modifier.md)) — the
  conditioned-tail read a keyed deposit is missing. Closes an improvement's BONUS-conditioned yield, a
  feature's HAS_RIVER-conditioned yield, and the dead handles three PerfectWorld-lineage map scripts
  (`C2C_PerfectMongoose_v310` · `C2C_PerfectWorld2f` · `C2C_Totestra`) keep waiting on it.
  ⛔ Do not close it by summing the conditioned tail directly — that applies every tech/age-gated deposit from
  turn 0, silently ([DEC-no-legacy-masking](../../architecture/decisions.md#dec-no-legacy-masking)).
  ⛔ Those same three scripts also call `isRequiresFlatlands()` on a BONUS — no `CvBonusInfo` has ever carried
  it (it's a FEATURE member). Pre-existing crash, not a migration casualty; leave it standing, don't invent
  which predicate was meant.

> Contract: [patterns.md § THE PYTHON READ BOUNDARY](../../architecture/patterns.md). Read maps:
> [pedia-read-map.md](../../reference/pedia-read-map.md) · [python-read-map.md](../../reference/python-read-map.md).
>
> ⛔ **"Stage 4" is a grouping, NOT a phase to wait for — nothing gates the disconnect.** A dead legacy Python
> getter is an OUTLAW, shot on sight; cutting wrong is cheap and is how a real dependency gets named
> ([roadmap.md](roadmap.md)). ADD to the library whenever a read makes sense, and clear Python-side compile debt
> as you meet it — parking it spends the census budget the rest of the worklist needs. ⛔ Never borrow legacy as
> a "temporary solution" for a read the library does not answer yet: add the read.

- **Re-point the surviving `GC.get<X>Info` reads.** Published NOWHERE, so each is an `AttributeError` the
  moment its handler fires. Identity reads (description, text key, civilopedia, strategy, type key, button)
  re-point mechanically onto `CyInfo` for every registry.
  ⛔ The rest is a DELETION list plus a COVERAGE checklist
  ([DEC-new-getter-surface](../../architecture/decisions.md#dec-new-getter-surface)), never a port list — each
  remaining name is answered by a group read, an intrinsic slot, an edge family, or a classification test, or
  it's dead. Adding a getter per legacy name is the half-migration reflex.
  ⛔ A cross-link reading a terrain/feature/building's `requires` back at the referencer is `EDGEF_RELATED`,
  never `EDGEF_REQUIRED_BY` — the plot substrate carries no `REQUIRED_BY` edge at all
  ([enabler.md](../../specs/enabler.md), `rp_requiredByRefInfo`'s routing). Not a gap; do not add one.

- **Re-point `CyState`'s city/plot/empire reads onto the contexts.** Python currently asks the `CvCity*` /
  `CvUnit*` object directly, which is exactly what
  [DEC-scope-contexts](../../architecture/decisions.md#dec-scope-contexts) forbids — the HAVE axis is asked of
  the scope's context, never reached ad hoc off the game object. `CityContext`
  ([contexts.md](../../architecture/contexts.md)) already answers most of what Python currently derives the
  long way.
  ⛔ Where a context cannot answer, that's a CONTEXT GAP to close by adding the forward — never a reason to
  reach past it.
  ⚠ UNITS have no context yet (a future role-specific scope): the unit plane reads the unit's own held
  containers meanwhile — O(held), never O(registry).

- **Give WorldBuilder's UNIT editing a home on the new surface.** `CyUnit` carries only the identity set
  ([patterns.md](../../architecture/patterns.md)), no reads — expected, not a defect
  ([DEC-cy-not-fixed](../../architecture/decisions.md#dec-cy-not-fixed)). Owner-ruled scope: editing an
  individual unit's strength, persisted via the WBS scenario field (engine state already serialized,
  [state-repositories.md](../../architecture/state-repositories.md)).
  ⛔ Do not re-register the legacy `CyUnit` binding to fix this.
  ⛔ Do not reproduce the legacy setter's bug: `setBaseCombatStr` writes the BASE, but `baseCombatStr()` returns
  the fully-composed value (base + promotion/unit-combat delta + SizeMatters) — they are not inverses. The new
  surface needs a base-in/base-out pair, or an explicit set-to-absolute.

- **Serve the INFO-OBJECT accessor plane's per-type tail** (`getEra`, `getGridX/Y`, `getWorldSize`,
  `getPrereqAndTech`, `isVisible`, `getColorType`, `getActionInfoIndex`, …) — `GC.get<X>Info(id).<method>()` is
  the dominant remaining unserved read; `CyInfo` already answers the generic identity reads by prefix.
  ⛔ `getChar` is NOT part of this — TEXT-plane, served off `CyGameTextMgr`
  ([patterns.md](../../architecture/patterns.md)).
  ⚑ Re-derive demand with `python Tools/census-python-boundary.py`.
- Serve the free-function map helpers (plot direction / XY / distance / step distance) — their registrar went
  with the binding purge; the map-generation utilities call them throughout. A failed read here is silent,
  falling to DLL-default generation ([python-load-sequence.md](../../reference/python-load-sequence.md)), so a
  wrong map is the only symptom.
- **Give WorldBuilder's two `CyMapGenerator` calls a home.** Its registrar is gone (no `class_<>`, no `.def`),
  so `CyMapGenerator()` raises `NameError` at the click; the class itself still compiles as a dead TU nobody
  reaches.
  ⛔ Do not re-register the wrapper ([DEC-cy-not-fixed](../../architecture/decisions.md#dec-cy-not-fixed)) —
  serve both calls on the new surface.
  ⚠ The two callers want different things: `CvWBDesc.py`'s `addBonuses()` is a real method on this wrapper;
  `WBGameDataScreen.py`'s `eraseGoodies()` is NOT — it's `CvMapGenerator::eraseGoodies`, reachable through
  `CyMap`, not this class. Re-registering the wrapper would not have fixed the second one.
- Bind the XML-named callbacks that resolve to no `def` in the module the DLL names, and the DLL-named
  game-utils callbacks with no Python definition at all
  ([python-load-sequence.md](../../reference/python-load-sequence.md) — a name resolves only in the module the
  DLL names). ⚑ Unconfirmed either way: `Tools/XMLTools/verify-python-callbacks.py` is the tool that answers
  this, but it needs a Python-2-compatible `ast` (it fails immediately under the Python 3 on this machine on a
  `print` statement in `CvDiplomacy.py`). Settle by running it under Python 2, or by hand-diffing the
  `<PythonCallback>`/event-trigger callback names in `Assets/XML/Events/` against `Assets/Python` `def`s.
- Add the missing `CvEventManager` handlers for the DOMAIN events the reporter emits and nothing handles. They
  drop silently through the dispatch-map miss path. ⚑ Concretely: `BugEventManager.__init__` registers
  `unitCaptured` / `combatWithdrawal` / `combatRetreat` / `combatLogCollateral` / `combatLogFlanking` via
  `addEvent()` (an empty handler list), and the only `on*` handlers for them live in the module's own
  triple-quoted "Sample Event Handlers" block — never executed, never wired via `addEventHandler`. Each fires
  from `CvEventReporter`/`CvDllPythonEvents` into a zero-length list and is silently dropped.
- **Serve the disabled-citizen tooltip's "which building opens this slot" list** — a REVERSE cross-link: read
  the specialist's own `EDGEF_RELATED`, never a whole-building scan. Also **wire a bonus's "what needs me"
  block onto its own `EDGEF_RELATED`** (`setBonusHelp`/`setBonusTradeHelp` render only the city-plane families
  via `appendEntityBlocks`, which deliberately skips `EDGEF_RELATED`/`EDGEF_REQUIRED_BY`) and **wire a
  promotion's per-unit accrual lines onto `CvPromotionAccrual::sum`** — that function has zero callers anywhere
  in the tree, and `parsePromotionHelpInternal` ignores its own `bAccrueLines` parameter and calls only
  `appendEntityBlocks`.
  ⚠ The rest of this composer rebuild is done: `appendEntryLines`/`appendEntryLinesFiltered` are implemented and
  called throughout, the requires-block composer (`buildRequiresClauses`) renders `requiresBuild`/`requiresOperate`
  with a live-city verdict, the four WELLBEING composers already render as BLOCKS
  (`setBadHealthHelp`/`setGoodHealthHelp`/`setAngerHelp`/`setHappyHelp`), and the wellbeing deposit total is no
  longer mislabelled "from buildings" — its composer comment now states the full source set (buildings, civics,
  traits, features, bonuses, specialists, corporations, techs).
- Re-point the unit power-value plane's readers. `Assets/Python/Screens/Advisors/CvDomesticAdvisor.py` still
  carries the comment "`getPowerValue` has no published read yet; ranks on cost alone until one exists" —
  `CvUnit::getPowerValue`/`CvUnit.h` has no `CyUnit`/`CyUnitInfo` counterpart.

## Triggers / grants

- Build START PACKAGES: the entity type, its folder + prefix + repo row + manifest, and the shipped defaults
  ([triggers.md](../../specs/triggers.md)). Two content decisions ride it — which units the defaults name, and
  NPC/barbarian starts.
- Retire the engine start selection (the whole-database scan + AI scoring, and the per-role starting counts)
  once packages carry the identities.
- Walk the player's HELD TRAITS in the per-unit promote pass. `tr_promoteOneUnit` walks the city's OPERATING
  BUILDINGS only, so no trait entry is ever consulted and trait free promotions reach nobody — the data is
  authored, so this is the whole of what is missing. The era-advance resolver is the same PRESENCE read, never
  the banned own-data inversion.
  ⚑ The per-class filter needs no new mechanism: it is the entry's own `enabled: "IS_<TAG>"` predicate, which
  `tr_promoteFromEntries` already evaluates for the building leg.
  ⛔ Do not answer this by restoring a trait-side promotion×unitcombat MAP — that is the legacy mechanism. The
  legacy removal half needs no take-away verb ([triggers.md](../../specs/triggers.md)).
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
- ⛔ **FIND AND FIX EVERY AI EVALUATION LOOP THAT SCANS A WHOLE REGISTRY (owner).** A scan asks the entity
  database a question the maintained state already answers; it no longer works and is not needed.
  ⚑ Two populations, different fix, do not treat alike: a loop over an **enabler domain**
  (units/buildings/techs/civics/projects/processes/promotions/builds) re-points onto the maintained FRONTIER
  ([enabler.md §6](../../specs/enabler.md)); a loop over any OTHER registry
  (unitcombats/specialists/terrains/features/bonuses/religions/properties/invisibles) is the OWN-DATA INVERSION
  and re-points onto the entity's own compiled entries via `InfoValuation::collectKeyedTarget`/`collectKeyedCombat`
  ([modifier.md §5](../../specs/modifier.md)).
  ⛔ Exception: a PLOT loop is not this class and must not be swept ([enabler.md §7.1](../../specs/enabler.md) —
  the worker-builds carve-out).
  ⛔ The compiler will never name one of these — this closes only by being searched for, never by the
  error-driven census.
  ⚠ A loop calling a gate with WHAT-IF args cannot simply swap to the frontier — it wants the as-if-held
  overlay or the gate twin instead ([enabler.md](../../specs/enabler.md)).
- Move `AI_baseBonusVal`'s per-kind loops off the whole-database driver onto the frontier, and off the dead
  prereq getters onto the bonus's own `EDGEF_REQUIRED_BY` ([DEC-one-reverse-view](../../architecture/decisions.md#dec-one-reverse-view)).
  **Partly done:** the Unit/Building/Project Value sections (`CvPlayerAI.cpp`) already walk `EDGEF_RELATED`/
  `EDGEF_REQUIRED_BY` and value through the with/without hypothetical. **Still a whole-database driver:** the
  Route Value section still does `for (iI = 0; iI < GC.getNumBuildInfos(); iI++)` and reads
  `getPrereqBonus()`/`getPrereqOrBonuses()` directly instead of the bonus's own `EDGEF_REQUIRED_BY`.
- `AI_techValue` (`CvPlayerAI.cpp`) still has whole-registry loops over specialists, terrains (×2), features
  (×2), bonuses, improvements, and religions (`for (iI = 0; iI < GC.getNum*Infos(); iI++)`) — restore the
  members these sweeps read, then drive them from the tech's own edges.
- The active-set work-list ripple (`ek_recheckActiveSet`, `CvEnablerKernel.cpp`) still exists and still runs its
  own fixpoint before announcing crossings. ⚠ Its current code comment argues FOR keeping this shape (announcing
  mid-loop would hand consumers a half-settled `provided` set) — read that reasoning before retiring it; this is
  not a straightforward "delete the machine" any more. The one item that WAS fixed: the runaway cap's comment no
  longer claims to self-heal — it now correctly states the fixpoint is left WRONG and must be fixed at its cause
  ([DEC-no-self-heal](../../architecture/decisions.md#dec-no-self-heal)).
- Converge the enabler's bespoke per-id reverse indices onto `EDGEF_REQUIRED_BY`
  ([DEC-one-reverse-view](../../architecture/decisions.md#dec-one-reverse-view) — a side index is banned
  "especially not inside an enabler"). There are **five**, in two files, all rebuilt by re-scanning every info at
  load: `s_operateBonusConsumers`, `s_operateBuildingDependents`, `s_operateDormantTriggeredBy`
  (`CvEnablerKernel.cpp`) and `s_udUnitDeps`, `s_udUpgradePred` (`CvUnitEnabler.cpp`) — all five confirmed still
  present and still populated at load.
  ⚑ The canonical reverse pass already inverts both `requires.build`/`requires.operate` and `dormantTriggers()`,
  so the answer these five recompute is already on the info — the only delta is they are operate-only while the
  canonical edge merges build+operate, which is safe per over-inclusion ([enabler.md §5](../../specs/enabler.md)).
  ⛔ Read [enabler.md §8](../../specs/enabler.md) "The reverse index, and what is deliberately NOT one" first —
  the axis-flag lists, `s_operatePropertyBandConsumers`, and `s_specialBuildingMembers` are explicitly NOT
  convergence targets. Sweeping those together with the five is the documented mistake.
- Delete the hand-rolled condition walks and route them through `CvConditionQuery`
  ([DEC-single-implementation](../../architecture/decisions.md#dec-single-implementation)): `ii_collectPredicateIds`
  (`CvImprovementInfo.cpp`) still duplicates the canonical leaf test verbatim, covering only `all`+`anyOf` where
  the canonical `collectPredicateIds` also walks `noneOf`/`enabled`/`disabled` — the duplicate is not merely
  redundant, it is NARROWER. (`collectPredicateIds`/`namesId`/`bucketForType` now each have real callers
  elsewhere — CyInfo.cpp, CvGameTextMgr.cpp, CvEnablerKernel.cpp — so they are no longer zero-caller; what has
  NOT happened is routing the three duplicates below through them.)
  ⚑ Same family: the three-way copy of the bonus/building presence-id collector in `CvImprovementInfo`,
  `CvBuildInfo` (`collectPrereqBonuses`) and `CvCorporationInfo` (`corpCollectSpreadBuildings`) — each re-testing
  an inline `compare(0,N,"PREFIX_")` that `bucketForType` exists to answer. The corp variant also
  needs the clause's `min`, which the shared surface does not yet return: LIFT that onto the shared surface, never
  keep the private copy for it. ⛔ `CvTechInfo`'s walk is NOT in this family — it reconstructs AND-vs-OR structure,
  which `CvConditionQuery` deliberately refuses to expose.
## Tree / include hygiene

- Retire the `CvInfos.h` umbrella — a hand-careful pass; the lessons and hard bans are in
  [AGENTS.md](../../../AGENTS.md) Conventions §Design.
- Run the dead-code / dead-XML pass — tooling generates CANDIDATES only; every removal verified against
  source/data and test-loaded, one subsystem at a time.
## Green-up (after the structure, never ahead of it)

- Engine-repair debt: the bare Engine includes · the property-manipulator helpers · `CvCity.h`'s functor row.
- The vocabulary TXT keys (one per family/kind/predicate/token) — polish on a working machine; the renderer's
  spell-back fallback is the accepted output until then.

## Spec gaps to close

- Give the mod-data design invariants a spec home — a requirement may not unlock after the thing requiring it;
  replacements are explicit, never implicit; a replacing entity must be better. The checks are gone; the
  invariants belong in [json.md](../../specs/json.md).
