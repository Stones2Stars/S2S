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

- Curate `culture.unit.garrison` (flagged in-code, awaiting its batch).
- Attach the ruling-16 trigger-plane set (`survivor`, `cityCapture`, `combat.subdueAnimal`,
  `combat.nukeInterception`, `diplomacy.techShare`) to its trigger's `chance`
  ([triggers.md](../../specs/triggers.md)).
  ⛔ Do not unblock the dangling `getSubdueAnimalBonusAI` / `getTechShare` consumers by minting a kind for them.
  ⚠ `techShare` additionally needs its KIND retired: unlike the rest of the set it was minted as
  `DIPLOMACY_TECH_SHARE`, so the re-home is a curator change AND a vocabulary removal.
- Give the §3.9 entry grammar a payload-less form so a carrier can state a cargo RESTRICTION with no capacity of
  its own, then author the flagged carriers ([modifier.md §6](../../specs/modifier.md)).
  ⚠ Settle in-game first whether the ancient transports are civilians-only; the owner's recollection is explicitly
  unconfirmed and must not be authored against.
- Re-home the remaining `identity` EFFECT keys to the block that already exists for each
  ([json.md §7](../../specs/json.md)): constraints → `requires`/`allowed`; `diploVoteType` → the top-level
  `voteSource` section (and rename the getter off the legacy XML tag); `tradeable` → the `canTrade` block;
  `advancedStart` → resolve the curator's parked
  flag; `pillageGold` → drop; **`allowsNukes` → `requires.build.disabled: "NO_NUKES"`**, the predicate
  [json.md §3.5](../../specs/json.md) already defines for exactly this ("a nuke-enabling building carries
  `requires.build.disabled: NO_NUKES`"). ⚠ Its legacy engine gate is CUT, so until the curator re-homes it the
  no-nukes bar does not apply to projects at all — the hole is deliberate and visible
  ([DEC-no-legacy-masking](../../architecture/decisions.md#dec-no-legacy-masking)), not an oversight.
  ⚠ `espionagePoints` rides the missions/`CvOutcome` carve-out — its channel is settled, only its authoring home waits.
- Bring `curate_trait`'s trade-route rows onto ruling 11, as `curate_building` already is: `iCoastalTradeRoutes`
  and `iForeignTradeRouteModifier` still emit `coastal` / `foreign` as MEMBERS, and neither has a kind in the
  vocabulary — so a trait authoring one emits an address that resolves to nothing and is dropped in silence.
  The ruled shape is the conditioned deposit (the memberless route count gated `HAS_COAST`; the modifier kind
  gated `IS_FOREIGN`) — a WHERE-member is the condition-as-member rollerskate
  ([DEC-conditions-are-predicates](../../architecture/decisions.md#dec-conditions-are-predicates)).
  ⚠ No trait authors either tag today, so this is latent rather than live data loss — and traits are
  content-LOCKED, so it closes by fixing the mapping, not by a regen.

- Emit property pulses through the shared property-source cleaner as trigger entries carrying
  `on`/`relation`/`distance`, instead of parking them verbatim.
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
- Retire the legacy `largestCity` member once ranked-target-selection EVALUATION lands.
- Re-home `stronglyRestricted` to a `requires.build` civ-membership gate, when NPC civilizations are wired.
- Move corp-HQ revenue (`HeadquarterCommerces`) with the corporation rework, and with it the two corp shapes
  no corporation authors: the HQ FREE UNIT — a `grants` payload, so it lands on the trigger plane off the
  headquarters fact ([triggers.md](../../specs/triggers.md)), never as an info getter — and corp-vs-corp
  EXCLUSION, whose home is the §9 `excludes` block ([json.md §9](../../specs/json.md)). Competition currently
  answers from the consumed-bonus overlap alone. ⛔ Neither is machinery to build ahead of data authoring it.
- Retire `DOMAIN_IMMOBILE`. Immobile is not a domain ([json.md §7](../../specs/json.md)) — a domain is the
  MEDIUM a unit operates in — and no unit authors it any more, so nothing keeps the member alive but its
  consumers: the enum entry, ~21 engine/AI sites, the `CIV4DomainInfos.xml` record, its game text, and a Python
  read. ⚑ It is TERMINAL in the enum (immediately before `NUM_DOMAIN_TYPES`), so removing it shifts no other id
  — which matters because `DomainTypes` crosses the ABI through `DllExport CvUnit::getDomainType()`.
  ⚠ Several sites treat it as a live case rather than a dead one (`isDomain`-style switches, an `FAssert` that
  ACCEPTS it beside `DOMAIN_LAND`), so this is a per-site read, never a delete-the-case sweep.
- Map the flagged unitcombat remainder — map the obvious, flag the unsure, never blunt-purge
  ([unitcombat-tag-mapping.md](unitcombat-tag-mapping.md)).

## Legacy still breathing — delete it

> The standing rule (purge violently; blast radius is the signal; the worst offenders are the ones OFF the core
> loop) is [roadmap.md § LEGACY STILL BREATHING](roadmap.md). ⚠ KNOWN-INCOMPLETE — legacy found anywhere else is
> killed on the same terms. ⛔ Never record a found legacy surface as acceptable or "kept until X".

- Re-point the city's WORKED-PLOT yield sum, the last leg of the city rate standing on a cut member. The VALUE
  has a source — a plot carries its context and its own realized-yield group read — so what is missing is only
  the city-side Σ over its worked plots. ⛔ Do NOT answer it from the city's own realized group read: that is
  the receiver total the rate is being computed FOR, so reading it here is circular. ⛔ And do not re-sum the
  radius per call — that is the measured cost class ([state-repositories.md](../../architecture/state-repositories.md):
  the pull must be a CACHE at every level, marked by worked-plot flips and by a working plot's yield changing).
  ⚑ The rest of the city rate hangs off this one, so it is the keystone of the yield cluster rather than one
  more accessor.
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
- Finish the HALF-CUT accessors: a member whose declaration, serialization and `savemigration.txt` tag are all
  gone, but whose accessors and consumers were left standing in its own class. They are compile errors that the
  per-TU error cap hides, so the census only names them a few at a time — sweep them from the migration ledger
  instead of waiting. `CvCity` and `CvPlayer` carry the remainder.
  ⚑ **The COMPILER enumerates them directly, and more cheaply than the ledger does:** a cut member's surviving
  accessor is a `C2065`/`C3861` on the bare `m_` symbol, not the `C2039` the member-census greps for — so
  extracting those names gives the exact live set, and every one should also appear in `savemigration.txt` (a
  name that does NOT is the likelier defect of the two, since it means a member was dropped without its tag).
  ⚠ Check each member's FEEDERS before converting — `resolvedValue` covers a unit's info ∪ promotions ∪
  unit-combat classes and NOTHING else, so a member fed from anywhere else loses that leg silently.
  ⚠ And check the slot's UNIT: a FLAT slot is ×100 and the reader reduces at its point of use, a percent slot
  does not ([DEC-fixedpoint-x100](../../architecture/decisions.md#dec-fixedpoint-x100)) — the legacy feeds
  divided at the WRITE, so a bare re-point is 100× on every flat channel.
- Wire the COUNT a building's per-improvement free-specialist deposit scales by. The read is wired through the
  ONE `per` resolver, but the count it resolves against does not count correctly yet, so those entries
  contribute 0 — wired, not right, which is the ordering while the tree is red
  ([roadmap.md](roadmap.md) WIRED OUTRANKS CORRECT); the value is checked from the cascade over HTTP once green.
  ⚑ The defect is the same one the TRAIT per-specialist deposits had: the deposit does not state its plural
  TARGET, so the count resolves at the wrong granularity. The trait emit passes `target="cities"`; the
  building's improvement-scaled free-specialist emit passes no target at all, so nothing expresses "each city,
  by its OWN count". ⛔ Fixing only the reader is inert — that was proven on the trait side, where the address
  had to carry the target before any reader change could do anything ([modifier.md §5](../../specs/modifier.md)).
- Give the PLAYER's improvement-yield change a source again. It is the DERIVABLE half — the recompute over
  adopted civics, active traits and cascade-active buildings' global rows — so it is a `CvDerivedCacheVec`
  ([state-repositories.md](../../architecture/state-repositories.md)), never a restored accumulator and never
  the event store beside it. Its reader dangles until it exists; the TEAM half is already the grant store.
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
- Move every consumer off the hand-named channel-shaped getters on `CvCity`/`CvPlayer`, then delete the old names.
- Cut the hide-and-seek per-type intensity ACCUMULATORS on `CvUnit` (serialized — the cut carries a
  `savemigration.txt` step; confirm the tag spelling against the stream first). Their replacements are built.
  ⛔ The AI sites SUM inside a loop over every `INVISIBLE_*`, so this is a rewrite, not a rename — a mechanical
  swap would count concealment once per type.
  ⛔ Do NOT sweep the neighbouring `getInvisibleType` / `getSeeInvisibleType` calls: those are live `CvUnit`
  methods sitting in the same blocks.
- Retire the direct `gDLL->logMsg` / BetterBTSAI log-helper call sites and the log-level globals they gate,
  wholesale as each domain migrates onto the spine — never tidied in place.
- Delete the legacy `ConstructRequirement` / construct-condition surface once the `requires` RENDERER exists
  (its last consumers are the prereq-block composers).
- Delete `CvPlayer::getBuildingPrereqBuilding` with the last of its text consumers. ⛔ Do not revive the prereq
  table to keep a text line rendering.

## Not built yet

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
  gamespeeds / handicaps (read from its sources, never cached behind a dirty protocol —
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
  ⚑ A live-option flip now ANNOUNCES (`SEVT_GLOBAL_DEFINE_CHANGED`), so a consumer that must answer one finally
  can — but that closes the *reactability* hole only. It does not make a live option a legitimate gate for static
  data, and reading "it emits now" as permission to author against one is the misreading to avoid.
  ⚠ HANDICAP is two values, not one, and a single `getHandicap()` silently picks the wrong one half the time: the
  per-player handicap (saved) drives human-facing economics, while every `getAI*` advantage reads the GAME handicap
  (the average of alive humans) — [engine.md](../../reference/engine.md). Keep them separately named.

- Make `hideAndSeek` a CACHED BLOCK on the UNIT and the CITY, not a per-read walk of the info (owner). Today
  `CvUnit::concealment()` / `detectionAgainst(methodSkill)` re-walk the unit's info ∪ promotions ∪ combat
  classes on every call, and `getInvisibleType()` reads the INFO alone — so a **promotion-granted method does
  not work at all**, which is the whole reason the method is a skill.
  ⚑ The dirty triggers already exist and are exactly right: a unit's resolved values dirty ONLY on a promotion
  or combat-class change (`SEVT_UNIT_PROMOTION_CHANGED` / `SEVT_UNIT_COMBAT_CHANGED`,
  [state-repositories.md](../../architecture/state-repositories.md)), and those are precisely the three
  carriers the block gathers over. The CITY side dirties on its building facts.
  ⛔ It is a SECTION, so it cannot ride the `URS_*` resolved table, which gathers modifier-FAMILY entries —
  it wants its own cached block on the same mark protocol, never a hand-named scalar pair beside it
  ([DEC-uniform-cache-shape](../../architecture/decisions.md#dec-uniform-cache-shape)).
  ⚠ `isInvisible` is one of the hottest reads in the engine, which is why the walk must not stay on it.
- The PLAYER-ALERT consumer, and the alerts owed to it — including the "your building was obsoleted" message,
  which rides `SEVT_BUILDING_OBSOLETED` (emitted for exactly this, and for logging; it drives no apply) — they
  re-attach to the OPERATE CROSSING fact, never
  re-inlined at a mutation site ([event-spine.md](../../specs/event-spine.md)). Expect the owed list to GROW as
  each legacy mutator is cut; add them together on the facts.
- Decide WHERE the citizen-assignment re-check is asked for. The mechanism itself is right and stays — the AI
  needs a way to be told the best plots to work may have moved — so this is a CALL-SITE question, never a
  removal. `AI_setAssignWorkDirty` is called from across the engine while `AI_updateAssignWork` re-runs the FULL
  assignment for every dirty city, so the flips are a turn-time cost in their own right.
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
- Give the `requires` TREE ONE SHARED WALK — "which ids / which predicates does this entity's `requires` name?".
  ⚑ This is NOT a missing machine: `CvRequires` exposes `build`/`operate` as public `CvCondition*` and the node
  is a plain tagged struct, so any call site CAN walk it. What is missing is that ONE of them does
  ([DEC-single-implementation](../../architecture/decisions.md#dec-single-implementation)) — otherwise the
  building river/coastal counts, the corp prereq leg and the block composer each grow their own recursion.
  ⛔ It is NOT a boolean-expression API ([pedia-read-map.md](../../reference/pedia-read-map.md) finding 3).
  ⚠ It needs BOTH shapes, and the id half alone does not cover its consumers: an ID LIST per HAVE-axis, and a
  PREDICATE test (the river/coastal building counts ask whether the tree names `HAS_RIVER` / `HAS_COAST`, which
  no id list answers).
  ⚑ Distinct from the REVERSE question ("who requires this bonus"), which `EDGEF_REQUIRED_BY` already answers —
  do not read one as covering the other.
- Move the BONUS-KEYED modifier consumers onto the AS-IF-HELD delta. "How much of this comes from the entry
  gated on THIS id" is answered by `CvCascadeHypothetical` — the `expected*` reads take one, and its `absent`
  set forces an id un-held, so the attribution IS the difference between two calls
  ([patterns.md](../../architecture/patterns.md) THE VALUATION PROTOCOL: the caller gets a DELTA).
  ⛔ The read exists; what is wrong is the CALL-SITE SHAPE. The resource-consumption pass asks per
  (city × building × channel), and `expected*` is a per-DECISION read — one per (city, candidate) per pass — so
  a per-channel `expectedModifier` there would run the evaluator in an inner loop. Convert through the GROUP
  reads (`expectedFlatYields` / `expectedYieldModifiers` / …), which fill a whole array per call: two calls per
  building, not two per channel.
  ⚠ A point read excludes these entries by construction (only null-condition entries fold into the compiled
  sum), so re-pointing one to the point read silently drops the bonus's whole contribution.
- A home for pedia category / sort metadata ([pedia-read-map.md](../../reference/pedia-read-map.md) finding 4).
- Ranked-target-selection EVALUATION ([parked/ranked-target-selection.md](../parked/ranked-target-selection.md))
  — a ranked entry applies unranked until it lands.
- The Python data-fetching library (below).

## The GETTER cut — game objects + AI

> Sequencing, and the ban on bending the new surface to fit an old call: [roadmap.md](roadmap.md).
> ⛔ The unit of work is the CLASS of read, never the individual getter — many collapse as the rebuilt infos
> wire through ([DEC-new-getter-surface](../../architecture/decisions.md#dec-new-getter-surface)).

- Move the what-if valuation consumers onto `expected*` — the AI candidate weighting and the build-list hover
  tooltip are ONE call ([patterns.md](../../architecture/patterns.md) THE VALUATION PROTOCOL).
- Move the IMPROVEMENT tech-yield reads onto the keyed what-if twin. One of them asks a THIRD question the twin
  does not answer: the `bOptimal` branch wants "assume every condition holds", which is neither the live
  evaluation nor a hypothetical over one named id. Decide whether that is a MODE of the same read or an
  entry-list sum before converting it — ⛔ never a second evaluator beside the one the twin already uses
  ([DEC-single-implementation](../../architecture/decisions.md#dec-single-implementation)).
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
  so the typed-free ledger reads empty whatever is granted into it. ⛔ Re-adding the member alone would restore
  a legacy accumulator. ⚑ It is the half of the seam the cascade does NOT own: the untyped AMOUNT is a summed
  deposit, but a TYPED free specialist is genuine one-shot state (a Great-Person join consumes its unit, an
  era advance is a persisted pulse — [legacy-grant-apply-sites.md](../../reference/legacy-grant-apply-sites.md)),
  so this wants a store, not a channel.
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
- Cut the `CvPlayer` unit-upkeep accumulators onto the UPKEEP cascade — ⛔ upkeep is NOT residue awaiting a new
  surface: it is **its own cascade channel, together with MAINTENANCE** (owner), and the vocabulary is already
  minted (`UPKEEP_UNIT_MILITARY` / `UPKEEP_UNIT_CIVILIAN` / `UPKEEP_FREE_MILITARY` / `UPKEEP_FREE_CIVILIAN` /
  `UPKEEP_AMOUNT`, at `EMPIRE | UNIT`). So `m_iUnitUpkeepCivilian100` / `m_iUnitUpkeepMilitary100` and their
  `*UpkeepMod` percent stages are the ordinary STORED-ACCUMULATOR DRIFT class
  ([DEC-accumulator-cut-uniform](../../architecture/decisions.md#dec-accumulator-cut-uniform)), and the empire
  total is a RECEIVER SLOT in the player's own package beside maintenance's — the second non-commerce receiver,
  which is what makes that rule general rather than a maintenance special case
  ([state-repositories.md](../../architecture/state-repositories.md), [economy.md](../../reference/economy.md)).
  ⚠ The free allowances are SIGNED free-amount kinds whose group floors at zero as combine metadata, applied
  BEFORE the consumption site's own `max(0, upkeep − Σfree)` — two floors, deliberately ([modifier.md §2](../../specs/modifier.md)).
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
- Dispatch a PROJECT's grants. The trigger engine has no project front door at all, so a project's
  `grantsSpecialUnit` payload (the completion-time special-unit unlock) reaches nothing and its consumer stays
  dangling. ⚑ Its sibling on the same completion — `enables.specialBuildings` — is an availability EDGE and is
  already read off the project, so only the payload half waits: the split is [triggers.md](../../specs/triggers.md)'s
  grant-vs-edge line, not a missing project concept.

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
- Close the remaining [enabler.md §8](../../specs/enabler.md) items: plot-group membership not trusted from a
  save, the load-end dormancy fixpoint, the dynamic operate axes.

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

- Engine-repair debt: the bare Engine includes · `CvOutcomeMission::mapFrom` · the property-manipulator helpers ·
  `CvCity.h`'s functor row.
- The vocabulary TXT keys (one per family/kind/predicate/token) — polish on a working machine; the renderer's
  spell-back fallback is the accepted output until then.

## Spec gaps to close

- Give the mod-data design invariants a spec home — a requirement may not unlock after the thing requiring it;
  replacements are explicit, never implicit; a replacing entity must be better. The checks are gone; the
  invariants belong in [json.md](../../specs/json.md).
