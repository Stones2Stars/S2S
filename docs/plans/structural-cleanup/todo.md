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

> The ruling: `identity` carries NO effects ([json.md §7](../../specs/json.md)). The shipped data does not fully
> obey it yet: **196 distinct identity keys are authored across ~78k authorings**, the overwhelming majority of
> which are text, display and genuine metadata and STAY. The effect-carrying residue is the bullets below — each
> re-homes to the block that already exists for its kind.
> ⚠ **Read the disposition, not a headline count.** Successive passes have resolved keys the old counts booked as
> effects — some were metadata all along (`conscription`), some ride carve-outs (`cultureRange`, `controlPoints`),
> and some are simply gone (`captures`, `cargo`, `movementCost` and `special` now author **zero** identity
> entries). A count is the sweep's SCOPE, never a defect tally.

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

- **The hide-and-seek CONSUMER census (re-measured): 11 dangling reads in `CvPlayerAI` + 17 in `CvGameTextMgr`.**
  The four per-type intensity getters are DELETED from the infos (verified: no declaration survives in
  `Sources/Infos/`), so these are compile errors rather than silent zeroes — ordinary consumer debt, sequenced
  with the AI cut, and a dangling site here is intended output. Their replacement is `CvUnit::concealment()` (one
  number) and `CvUnit::detectionAgainst(method)`. ⚠ The AI sites SUM inside a loop over every `INVISIBLE_*`, so a
  mechanical swap would count concealment fourteen times — the loop collapses to one read, which is why this is a
  rewrite rather than a rename.
  ⛔ **Do NOT sweep the neighbouring `getInvisibleType` / `getSeeInvisibleType` / `getNumSeeInvisibleTypes` calls
  with them** (14 in `CvPlayerAI`, 10 in `CvGameTextMgr`): those are still-LIVE `CvUnit` methods, not info
  getters, and they compile. They sit in the same blocks, which is exactly why they invite a mechanical
  search-and-replace that would break working code.
- **The hide-and-seek help text still enumerates per type** — spot intensity, spot range and same-tile, one
  block per `INVISIBLE_*`. It renders values that are now always 0, and it is the exact thing the pairing was
  written down to make sayable: a detection entry renders itself through `appendEntryLines`.
- **8 AI valuation reads of the deleted vision getters, across THREE files** (re-measured): `CvCityAI` 2
  (`improvement.getVisibilityChange` + `.getSeeFrom` on one line), `CvUnitAI` 2 (`getVisibilityChange`), and
  **`CvPlayerAI` 4 — which the earlier census missed entirely**: they read `getVisibilityChange` on a PROMOTION
  and a UNITCOMBAT, not an improvement, so a sweep scoped to "the improvement getters" walks straight past them.
  Neither getter is declared anywhere in `Sources/Infos/` any more. Compiler census, sequenced with the rest of
  the AI consumer cut, not fixed on sight; the replacement is the entity's compiled `vision` entries, the same
  source the pedia renders from.
- **⛔ `CvFeatureInfo::getSeeThroughChange()` is a SECOND masked zero — the movement defect's twin, and this one
  has a live consumer.** The member maps from `vision.plot.seeThrough.flat`, an address **no entity authors**:
  all 78 vision-authoring features emit `vision.plot.obstruction` (the spec'd kind — a feature's see-through value
  IS its obstruction, [vision.md §5](../../specs/vision.md), which retires the `seeThrough` member outright). So
  it answers **0 for every feature**. ⚠ Its one reader is `CvPlot.cpp:7032`, the visibility-dirty test
  `old.getSeeThroughChange() != new.getSeeThroughChange()` — with both sides permanently 0 that comparison is
  **always false, so a feature change never refreshes line-of-sight**. Delete the member and re-express the check
  on the `obstruction` entries; `curate_feature.py`'s docstring still advertises the retired `seeThrough` address
  and is corrected with it.
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

## Legacy still breathing — the KILL LIST

> The standing rule (purge violently; blast radius is the signal; the worst offenders are the ones OFF the core
> loop) is [roadmap.md § LEGACY STILL BREATHING](roadmap.md). ⚠ KNOWN-INCOMPLETE — legacy found anywhere else is
> killed on the same terms; add it here. ⛔ Never record a found legacy surface as acceptable or "kept until X".

- **The hand-named channel-shaped getter set** on `CvCity.h`/`CvPlayer.h` — the new group reads stand beside them
  today, which is the two-live-surfaces state
  ([DEC-new-getter-surface](../../architecture/decisions.md#dec-new-getter-surface)) forbids. Move every consumer,
  delete the old names.
- **The `Cy*` binding surface is CUT AT THE REGISTRATION, but 7 loader files still stand.** The 23
  `Cy*Interface*.cpp` files ARE deleted and `CvDLLPython::DLLPublishToPython` no longer defines a single
  `class_<Cy*>` — so nothing is published. ⚠ **But `Sources/Infrastructure/CvPython*Loader.cpp` was never deleted**
  (the nuke commit said "interface/loader files" and only trimmed the loaders): six of them still carry
  **1,324 `.def` bindings** — City 356 · Player 348 · Misc 261 · Unit 137 · GlobalContext 111 · Plot 111 — plus
  `CvPythonEnumLoader.cpp`. **Verified: ZERO callers of any of their entry points anywhere in the tree**, and
  fbuild globs recursively, so all seven still COMPILE. They are corpses by the standing rule (a `.def` for a
  deleted getter is cut on sight; piecemeal cutting is not forbidden) — cut them.
  ⚑ **One is NOT merely a corpse: `CvPythonEnumLoader` publishes the engine ENUMS**, and enum resolution AND
  EXTENSION is a first-class requirement of the replacement library
  ([patterns.md](../../architecture/patterns.md)) — BUG reaches `WidgetTypes`/`InputTypes`/`InterfaceDirtyBits`
  only this way and MINTS new members at runtime. Deleting it without the library serving that leaves those reads
  with no path at all, so it is a COVERAGE obligation, not just a deletion.
  What remains to BUILD is the replacement ([Stage 4](#stage-4--the-consumer-cut-sequenced-last-see-the-roadmaps-order-ruling)),
  and its coverage checklist is measured, not estimated: **285 of the 399 removed info-binding names were still
  called from `Assets/Python`** (heaviest: `RevolutionWatchAdvisor` 50, `PediaBuilding` 38, `CvMainInterface` 31,
  `CvTechChooser` 30). Those calls were already dead — the getters behind them had gone — so the list is a
  requirements set, never a regression report.
  ⚑ **The `Cy*` WRAPPER classes STAY, and that is correct, not a half-cut:** 33 engine files hold them for the
  engine→Python CALLBACK direction, which is out of scope
  ([patterns.md](../../architecture/patterns.md): the cut is directional).
  ⚖ **A `.def` for a DELETED getter is cut ON SIGHT (owner: "it's not like the game currently works").** A
  binding to a getter that no longer exists is a dangling reference the compiler census already named, and for a
  `Cy` binding the only way to fix a named consumer is to DELETE it
  ([DEC-cy-not-fixed](../../architecture/decisions.md#dec-cy-not-fixed) forbids re-pointing or widening).
  ⛔ **WHAT IS ACTUALLY FORBIDDEN: shoehorning anything into legacy so the PYTHON GETTERS KEEP WORKING (owner).**
  That is the ban — not the method of cutting. Keeping a dead binding alive *because Python calls it*, widening a
  getter to satisfy a `.def`, or holding a cut back to spare the `Cy` surface are all the same move, and it is
  the one that produces the half-migrated state ([DEC-cy-not-fixed](../../architecture/decisions.md#dec-cy-not-fixed)).
  ⚖ **PIECEMEAL CUTTING IS NOT FORBIDDEN — do not read a prohibition in (owner: "I have never forbidden piecemeal
  cutting").** "Cut away WHOLE" states the END STATE — complete disconnection, nothing left breathing — and the
  SHAPES banned on the way there (a widened binding, a shim beside it, two parallel live surfaces). It does not
  require one atomic operation, and inventing that requirement blocks exactly the dead-binding sweep this surface
  needs. Cut what is provably dead when you find it.
  ⚑ The Python side is already broken regardless: `Revolution.py:1140` calls `getFlatMovementCost`, removed with
  the route flat-movement mechanic ([superseded-ideas #23](../../architecture/superseded-ideas.md)), so that block
  dies on an `AttributeError` before reaching anything else. Preserving a binding "for live Python" is fiction
  wherever the getter is gone.
- **`CvCity`'s hand-rolled dirty caches** — demolition fodder, never conversion targets; cut when the channel that
  replaces them lands.
- **The direct `gDLL->logMsg` / BetterBTSAI log-helper call sites** and the log-level globals they gate — retired
  WHOLESALE as each domain migrates onto the spine, never tidied in place.

## Not built yet

- **The endpoint route table** beyond the six stored-vs-oracle documents — it stays empty until the access surface
  can be read THROUGH, never restored to reach around it ([http-endpoints.md](../../specs/http-endpoints.md)).
- **The Python data-fetching library** — see Stage 4 below.

## The GETTER cut — game objects + AI (⚖ owner: MORE PRESSING THAN THE PYTHON LAYER)

> ⚖ **Sequencing ruling (owner): the game-object and AI getters are updated BEFORE the Python library is built.**
> Not a deferral of the library — an ORDER call, and the roadmap's own (*design surface → contexts → THEN the AI
> calls*). The reason is the library's gate: it must be **COMPLETE**
> ([patterns.md](../../architecture/patterns.md)), and every value the new surface cannot answer is one a binding
> would reach around into legacy for — re-creating the two live surfaces the hard kill exists to close. So the
> read surface is finished first, and the library is written against a settled one.

**Measured state (verify before acting; counts drift):**

| surface | new group reads | legacy channel-shaped names still standing |
|---|---:|---:|
| `CvCity.h` | 14 | **164** (of 574 total `get*/is*/has*`) |
| `CvPlayer.h` | 20 | **103** (of 617) |
| `CvPlot.h` / `CvTeam.h` | 6 / 2 | — |

**AI consumption: 825 channel-getter call sites** — `CvPlayerAI` 386 · `CvCityAI` 370 · `CvUnitAI` 62 ·
`CvWorkerAI` 2 · `CvTeamAI` 1.

⛔ **This is NOT a 267-item worklist.** Per the roadmap, many collapse as the rebuilt infos wire through —
measure what survives, then cut the genuine residue. The classes below are the unit of work, never the getter.

- **① The WHAT-IF valuation — the identified FIRST block, and the cleanest.** The **26 `getAdditional*By*`
  getters** on `CvCity` (`…BaseYieldByBuilding`, `…CommerceBySpecialist`, `…HappinessByCivic`,
  `…HealthByFeature`, `…DefenseByBuilding`, …) ARE the legacy what-if valuation: each returns a **delta**, which
  is exactly the [valuation protocol](../../architecture/patterns.md)'s "contexts in, proposed increase out".
  Their replacement — `expectedFlatYields` / `expectedYieldModifiers` / `expectedPlotYields` /
  `expectedFlatCommerce` / `expectedWellbeing` — is **BUILT and has ZERO callers**, which is why the two pass-in
  scenarios currently hold vacuously.
  ⚑ **Its consumers are precisely the TWO patterns.md names as ONE call:** the AI weighting a candidate and the
  build-list HOVER TOOLTIP (`CvBuildingFilters`, `CvBuildingSort`, `CvDLLWidgetData`), plus `CvGameTextMgr`.
  Wiring them onto one valuation is what makes the displayed number and the acted-on number the same number
  structurally.
  ⚠ **`CvPlayerAI`'s 3 `getAdditionalEventChance` hits are NOT this family** — that is `CvEventInfo`'s event-chance
  list, a different mechanic a `getAdditional*` grep sweeps up by accident. The AI what-if sites are 7 in
  `CvCityAI` and 13 in `CvPlayerAI`.
  - **The WELLBEING half is converted** — the 6 `CvCityAI` happiness/health sites now ask the INFO
    (`kBuilding.expectedWellbeing(cityContext, empireContext, plotGroup, …)`), one group read serving both
    channels per candidate, with the opposing-pair nets added ONCE to the calc surface
    (`InfoValuation::netHappiness` / `netHealth`) rather than open-coded per site.
  - ⛔ **The YIELD half does NOT convert mechanically, and this is the real design item.**
    `getAdditionalYieldByBuilding` is composite: base + extra + base-MODIFIER deltas, **minus the same for every
    `getReplacedBuilding` the city already has fully active**. That supersession-netting leg is the problem — it
    asks *"what do I gain, net of what this supersedes"*, and the info-side valuation structurally cannot answer
    it (an info knows what it carries, never what the asking city already has). ⛔ Do NOT widen `expected*` with a
    replaced-buildings argument — that is the roadmap's named failure ("a what-if argument, an ignore-this-clause
    flag"). The supersession fact belongs to the ENABLER, which already owns `replacedBy`; the open call is
    whether the netting re-homes there or the call site composes two valuations itself.
  - **The CIVIC half carries 6- and 9-argument legacy signatures**
    (`getAdditionalHappinessByCivic(eCivic, bDifferenceToCurrent, bCivicOptionVacuum, eStateReligion, iExtraPop,
    iMilitaryHappinessUnits)`; `getAdditionalHealthByCivic` with `iIgnoreNoUnhealthyPopulationCount` /
    `iIgnoreBuildingOnlyHealthyCount`). Those flags encode a civic-SWAP simulation, not a candidate's
    contribution. Same ban applies — re-express the call site, never absorb the arguments.
- **⚖ AI LOOPS ARE DESIGNED TOWARD THE NEW SURFACE, AND ANY FULL-RECALC-OF-ALL-THINGS IS NUKED (owner).** Two
  rulings that govern this whole section, and they cut the other way from "don't touch the AI":
  **(a) changing an AI loop to accommodate the surface is NOT banned** — the contexts exist precisely so it can;
  **(b) there is no shim — there is only the new surface, which the AI loops accommodate.** ⛔ So the failure mode
  is inverted from what it looks like: bending the surface to keep an AI loop's shape is the rollerskate, and
  rewriting the loop is the work. A converted read wedged into an otherwise-legacy expression is the same defect
  one level down — convert the ARITHMETIC CLUSTER, not the operand
  ([fixed-point-and-scales §4c-bis](../../specs/curators/fixed-point-and-scales.md)).
- **The BUILDING-PREREQ TABLE is retired, and its residue is outside the AI.** The per-building prereq-count map
  (a `min` on a `requires` atom in the authored model) has no getter on the info any more, so
  `CvPlayer::getBuildingPrereqBuilding` reads a member that does not exist — it is DANGLING, not working code, and
  it goes. The AI is already off it: "which buildings need me" is the candidate's own
  `edge(EDGEF_REQUIRED_BY, EDGEB_BUILDINGS)`, and "do I have enough prereqs" is the enabler's LISTED verdict.
  ⚠ Still on it, as ordinary consumer debt: **`CvGameTextMgr`** (3 sites — the pedia's prereq lines, which belong
  with the composer move onto rendered entry lines) and `CyPlayer` (inside the orphaned loader above). Delete the
  `CvPlayer` function with the last of them; do NOT revive the table to keep a text line rendering.
- **The BONUS residency/counting work is [enabler.md §8](../../specs/enabler.md) open item 2 — read it there, and
  do not re-derive it.** The spec's target, stated once: **`CvCity::getNumBonuses` becomes a BARE FETCH of a
  maintained, never-serialized per-city count, kept current by the crossing fan-out plus the tech / minted / corp
  events that move its gates.** The `CvPlotGroup` stays the ONLY authoritative list for trade resources; VICINITY
  is the city's own local-presence fact, satisfying `connection:"vicinity"` atoms and nothing else (one pasture is
  ONE horse, never vicinity + network = 2).
  ⛔ **The gates are not "kept" and not "deleted" — they MOVE THE NUMBER ON EVENTS.** Every per-read re-plumbing of
  this (route the read through a context, read the plot group at the call site, drop a gate because its data
  re-homed) is the wrong axis, and each was tried and backed out here. The per-read chain — TechCityTrade gate →
  two-hop plot-group resolution → group sum → minted gate → corp add-on, on EVERY call — is named in the spec as
  the turn wall's hottest cluster, and it is what the maintained number replaces.
  ⚠ `CityContext::tradedBonusCount` re-derives every bonus on refresh, so it is on the wrong side of this too.
- **② Realized-value reads** (`getYieldRate`/`…100`, `getCommerceRate`/`…TimesTimes100`, `getMaintenanceTimes100`,
  `getTotalDefense`/`getDefenseModifier`) — already answerable by the existing group reads; these are a consumer
  move, not new surface.
- **③ Per-SOURCE decomposition terms** (`getBuildingHappiness`, `getBonusGoodHealth`, `getFeatureGoodHappiness`,
  `getReligionHappiness`, `getSpecialistHappiness`, `getCivicHappiness`, `getMilitaryHappiness`,
  `getCelebrityHappiness`, `getVassalHappiness`, `getStateReligionHappiness`, …) — the legacy accumulators, cut by
  [DEC-accumulator-cut-uniform](../../architecture/decisions.md#dec-accumulator-cut-uniform). ⛔ They do NOT each
  earn a replacement getter: the group read answers the TOTAL, and per-source attribution is the ORACLE
  endpoint's job, not the read surface's.
- **④ The genuine residue needing NEW surface** — the slider math (`getCommerceFromPercent`,
  `getCommerceRateAtSliderPercent`), the espionage counters, the live combat state (`getDefenseDamage`,
  `getLastDefenseDamage`), `getHappinessTimer`, and the `CvPlayer` unit-upkeep family. These are what the
  classification is FOR: only they need design.

## Stage 4 — the consumer cut (sequenced LAST; see the roadmap's ORDER ruling)

- **The `CvCity`/`CvPlayer` getter consolidation** — known work, not the primary focus, and a fair few collapse on
  their own as the rebuilt infos wire through. Measure what survives that before planning a sweep.
- **The AI call sites** — the largest consumer of the info surface, deliberately last. A dangling AI call site is
  intended output, not a defect to fix on sight.
- **`CvGameTextMgr` composers onto rendered entry lines** — the per-entry renderer exists
  (`Sources/UI/CvEntryText`) and `CvGameTextMgr::appendEntryLines` is the shared consumer, but only the
  VISION and MOVEMENT families have moved onto it (5 call sites). The other info-help composer families still
  hand-assemble from getters.
  ⚑ Each move DELETES composer code rather than porting it: a rendered line already carries magnitude,
  unit, target, scope, per-scaler and conditions, so a new channel needs no composer edit at all.
  ⚖ **THE DLL DOES NOT CONVERT FOR DISPLAY — the consumer converts itself (owner: "let python convert
  themselves").** A composer doing `(float)value / 100 / denominator` to print `%.2f` is the DLL performing the
  presentation layer's arithmetic, and it puts FLOAT in the DLL for a value the engine holds as an integer.
  ⚠ Not an OOS risk while it is display-only, which is exactly why it survives unnoticed — but it is the wrong
  side of the boundary, and it is the shape to remove as each composer moves, never to copy into a new one.
  Live instance: the plot-help revealed-route cost in `CvGameTextMgr` — a LIVE-STATE read, not a composer, so it
  does not retire with a composer move and needs its own re-expression.
- **Re-point the unit consumer getters onto `resolvedValue()`** (`Sources/Cascade/CvUnitResolved`).
- **The unit power-value plane** — its readers are ordinary consumer debt on a deliberately red tree.
- **The Python data-fetching library** — built COMPLETE, then the `Cy*` surface disconnected whole. Contract:
  [patterns.md § THE PYTHON READ BOUNDARY](../../architecture/patterns.md). Build it for the pedia (a SHAPE oracle,
  NOT a coverage oracle — the appendix is enumerable). Read maps: [pedia-map.md](../../reference/pedia-read-map.md) ·
  [python-read-map.md](../../reference/python-read-map.md).
  ⚖ **Sequenced AFTER the getter cut above (owner).** Two INFO-side holes are already named and would shape the
  library wrong if it were written first: **(a) there is no `requires` RENDERER** — `CvEntryText` renders modifier
  entries and conditions only, and its own header defers the requires/gate render, yet a pedia page's prereq block
  is ~70 sites plus the Python `getGOMReqs` BoolExpr walk the structured tree is meant to delete; **(b) category /
  sort metadata has no home** — the hub derives groupings from ~60 heuristics (`getEra`+1, `getBonusClassType`,
  `getProductionCost() <= 0`, `getMaxGlobalInstances() == 1`, grid X/Y), which
  [pedia-read-map finding 4](../../reference/pedia-read-map.md) flags as needing a decision on where category tags
  live.

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
  the family on terrain / feature / route, and its **17 consumer reads across 7 files** each reduce `÷100` at their
  own point of use. That is behaviour-preserving: every authored value is an exact multiple of 100, and the one
  surviving float reader is DISPLAY ONLY (the plot-help revealed-route cost) — it renders a cost, it does not move
  a unit.

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
- **The whole-database BUILDING sweeps LEFT outside `CvCityAI`** — the enablement valuations now ask the asking
  entity's own compiled edge ([DEC-one-reverse-view](../../architecture/decisions.md#dec-one-reverse-view)); what
  remains is THREE distinct classes, and treating them as one job is the trap:
  - **`CvPlayerAI::AI_baseBonusVal`'s per-bonus building loop.** Every term it reads is a bonus-keyed value table
    (`getBonusYieldModifier` / `getBonusYieldChanges` / `getBonusCommerceModifier` / `getBonusHappinessChanges` /
    `getBonusHealthChanges` / `getBonusDefenseChanges` / `getBonusProductionModifier` / `getPowerBonus`), and
    **not one of those getters is declared on any info** — so the loop is dangling consumer debt whose
    replacement valuation is unbuilt, sequenced with the AI cut. Its eventual driver is the bonus's
    `EDGEF_RELATED` (the candidate SUPERSET — consumers keep their exact predicates over it). ⛔ Converting the
    driver ahead of the valuation wires the loop to a moving target (the roadmap's ORDER ruling).
  - **The HELD-building sweeps** (`AI_civicValue`'s religion-disabling blocks) scan the database testing
    `getBuildingCount(x) > 0`. That is a HAVE read, not a frontier one, and the aggregate it wants — a
    PLAYER-level held-building list — does not exist (`CvCity::getHasBuildings` is the city-scope one). Give the
    OBJECT the aggregate ([tally.md](../../specs/tally.md): "let an object care about itself"), never a side-store.
  - **The OWN-DATA inversions** — `kCivic.getBuildingCommerceModifier` / `getBuildingHappinessChanges` /
    `getBuildingHealthChanges` are read by enumerating every building id to find the CIVIC's own authored keys.
    That is the keyed-container inversion [pedia-read-map finding 2](../../reference/pedia-read-map.md) names; it
    dies to the container being served whole, not to a frontier swap. ⚠ The `CvEventInfo` twins
    (`getBuildingYieldChange`/`…CommerceChange`/`…HappyChange`/`…HealthChange`) ride the EVENTS carve-out.
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
