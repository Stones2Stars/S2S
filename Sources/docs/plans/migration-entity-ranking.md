# Migration entity ranking — full-context, annotated (owner-verified 2026-06-15)

The top-down curation order for #428, **annotated with the current agent's full context** so the next
agent inherits the reasoning (edges, modifier surface, status, decisions) rather than a bare list.

> **ROLE: this is THE authoritative curation-order artifact for #428 — the working document the curating
> agent executes from.** Resume via `handover-2026-06-15-pm.md` first (orientation), then read the two
> LOCKED specs (`modifier-cascade-spec.md` v3 + `enabler-cascade-spec.md` v0.3), then work through this
> ranking top-down, entity by entity.

**Ordering principle:** config & sources first, monster targets last. A source is migrated before what it
`enables`/conditions (so the first-migrated entity owns a shared edge; later entities conform). Derived
from the `enables` spine (`store.PREREQ_FIELDS`) + the modifier containment spine
(`world→team→empire→area→city→plot{improvement|feature|terrain|route}→building/specialist/unit`). Read
against the two LOCKED specs: `modifier-cascade-spec.md` (v3) + `enabler-cascade-spec.md` (v0.3).

**Status legend:** ✅ curated pre-reset (curator exists in `Tools/Migration/`, RE-VERIFY against v3 + the
drop re-check) · ◐ heavy, partially analysed · ☐ not yet curated. *(The branch reset cleared generated
`Assets/Data` JSON; the curators survive and are the starting point.)*

**⚠ Two cross-cutting rules for EVERY entity:** (1) the grounding's "dead" calls are C++-only — re-check
each dropped field against `Assets/Python` + intent, sorting into the 4 categories (modifier-spec §8). (2)
Before authoring any keyed/inverted edge, grep the already-committed curators so the FIRST-migrated owner
isn't double-authored.

**⛔ HARD RULE — infos are converted STRICTLY SERIALLY, one at a time; NEVER in parallel (owner ruling
2026-06-15).** Parallel/mass conversion (many infos at once) has been tried before — the abandoned
mass-migration detour. **The catastrophe was NOT the conversions themselves; it was being UNABLE TO FULLY
VERIFY that each conversion had been done properly.** Doing many at once outran verification, and correctness
silently eroded because no one could confirm each info against its live consumer. So serial conversion exists
to keep every info VERIFIABLE: curate → **verify against the live C++/Python consumer (the load-bearing
step)** → commit ONE info, then the next. Verification is the point, not throughput. This is non-negotiable:
do not fan out infos across agents, do not batch-curate, do not "knock out the easy ones together" — and
remember **nothing here is ever easy** (AGENTS.md: "Nothing here is ever just a one-liner — expect hidden
consequences"); every info is a tightly-coupled curation with hidden ripples, which is exactly why each must
be verified before the next. (Secondary: the config→…→monsters ordering only holds value if entities settle
one at a time so later ones conform to earlier edges — the inversion rule.)

**⛔ TWO HANDOVER/RESUME GATES — every handover MUST state them, every resuming agent MUST honor them (owner
ruling 2026-06-15; the handover process is the countermeasure to context-poisoning on compaction, and it must
not drift):**
1. **READ ALL THE SURROUNDING DOCUMENTATION before touching anything** — not just a skim of the resume list.
   Mandatory each resume: the two specs, `building-cascade-conversion.md`, this ranking, **`migration-renames.md`
   (the canonical old→new registry + the decisions already made)**, the entity's
   `Tools/Migration/classifications/*.json`, and the prior handovers. Look concepts UP in the docs; never
   reconstruct them from memory or from how the C++ currently reads/loads (the C++ is reworked to fit the data,
   NOT ground truth). A spec line that conflicts with a later owner ruling is stale — flag it, the ruling wins.
2. **THE OWNER VISUALLY INSPECTS THE WRITTEN JSON AND EXPLICITLY APPROVES BEFORE EVERY COMMIT.** curate →
   `--write` → owner inspects the actual `Assets/Data/.../*.json` → explicit approval → commit. Approval of the
   MODEL/scope in discussion is NOT approval to commit the JSON. Never commit without the inspection + go-ahead.

**⛔ THREE GOVERNING RULINGS for what the JSON IS (owner, 2026-06-15) — they OVERRIDE any "make it match the
code" instinct:**
1. **Author the data for WHAT IT IS, not how the current C++ fetches/combines it.** Choose the unit/shape from
   the datum's own nature. A value stored as a percentage is `percent` even if today's engine happens to apply
   it multiplicatively — do NOT reverse-engineer the unit from the consumer's combination math. *(Concrete: the
   GameSpeed `iSpeedPercent` 100/200/…/1000 values are `percent` — "1000%" — NOT `multiplier`, despite the
   engine currently doing `×p/100` by product. That product/additive behaviour is engine combine-mode metadata
   (§7), reworked to fit the data — it is NOT the per-value unit.)*
2. **The C++ data-fetching is reworked to fit the JSON — NEVER the JSON reshaped to fit existing fetching.** The
   data model leads; the engine adapts to it. Reading the C++ consumer is to learn what the datum MEANS, never
   to make the JSON conform to how the code currently reads it.
3. **The JSON must make sense to a MODDER reading the file COLD** — no understanding of the codebase internals.
   Self-explanatory keys/values, modder-natural names; never a structure that only parses if you know the
   engine. If a shape needs codebase knowledge to understand, it is wrong — simplify it.

**What "verify" means at THIS phase (owner, 2026-06-15) — we CANNOT runtime-test against the live game yet**
(the cutover is atomic; the DLL still loads XML until then). Verification now = **structural adherence + ZERO
invention**: (a) output conforms EXACTLY to the locked v3/v0.3 shapes; (b) **nothing is invented** — every
family / scope / token / address / edge traces to the locked spec vocabulary OR a real field in the actual
XML; if something does not fit the locked structure, FLAG it, do not improvise a new shape; (c) faithfulness
cross-checked by READING the static C++/Python consumer + XML, not by running the game. This **supersedes**
the older `building-cascade-conversion.md` "data-phase shapes need only be reasonable + faithful, NOT perfect"
language, which predated the structure lock — adherence is now strict.

---

## Why this order (the reasons) — owner-approved 2026-06-15

The order is a TOPOLOGICAL SORT of two dependency axes, so that whatever an entity references already exists
and is settled when its turn comes:

1. **Config & global axes first.** They gate/categorize others but are themselves gated by (almost) nothing.
   Settling them first means every downstream entity can reference fixed config.
2. **The first-migrated entity OWNS a shared edge; later entities conform** (the inversion-vs-target-keying
   rule). A cross-entity edge (tech→building, bonus→building) is authored exactly ONCE — on the
   SOURCE/CONDITIONER — so the source must precede the target or the edge gets double-authored. This is the
   single biggest reason sources precede targets.
3. **Tech is the spine root.** It `enables` nearly everything, so migrating it first lays the backbone every
   other entity hangs its `enables` / `requires.build` off.
4. **Conditioners before the conditioned.** A Building/Unit that requires a Bonus inverts that edge ONTO the
   bonus; so Bonus — and Civic/Religion/Corporation/CultureLevel, all conditioners — precede Building/Unit.
5. **Resources / map-substrate before producers.** Terrain/Feature/Improvement/Route are the plot leaves the
   city producers and the monsters reference.
6. **Unit-plane sources before Unit.** Promotion/UnitCombat/SpecialUnit deposit onto units (the self-
   accumulator stack, §5), so they must be settled before the Unit monster consumes them.
7. **`Special*` ride their parent monster.** SpecialBuilding is a per-player-capped building GROUP (shares
   building vocab); SpecialUnit deposits onto the loaded unit. Neither is meaningful apart from its monster.
8. **Monsters (Building, then Unit) LAST.** They are the most-targeted entities (every tier above
   `enables`/conditions/deposits onto them), so doing them last means every edge they consume is already
   settled AND the structure is fully proven on the simpler entities first — de-risking the two biggest,
   gnarliest curations.
9. **Modifier containment axis.** Aggregating scopes (`world→team→empire→area→city`) and the producers
   (`building`/`specialist`/`unit`) are touched in containment order, so a deposit's target scope is defined
   before anything deposits onto it.

---

## Tier A — Pure config / global axes (gate or categorize; safe first)

1. **GameSpeed** ✅ — no enabler edges. Global percent multipliers → `costs` family (cost-style combine).
   ~150 consumption sites; conformance is a late C++ concern, not data.
2. **Handicap** ✅ — config. Difficulty multipliers; `advancedStart`→`identity`. Scalar property-collapse
   can't carry gated/multi-source property deposits (flagged vs the list shape — property/#429 pass).
3. **Era** ✅ — config + WORLD-STATE bools (`bNoGoodies`/`bNoBarbUnits`/`bNoBarbCities` → D9 world-state
   section, NOT additive families; `bNoAnimals` → separate "disable animals" issue, §8-iii). `byEra`
   condition-as-member retrofits to `enabled`. `iBarbarianCityCreationTurnsElapsed` → a turns-elapsed gate.
4. **Process** ✅ — `TechPrereq` (tech→process). Production→commerce conversion rates.
5. **Victory** ✅ — config (13 live `testVictory` fields). Non-cascade `victory` section.
6. **Vote** ✅ — diplo-vote resolutions. The world-state bools (`bFreeTrade`/`bNoNukes`/… `ForceCivics`) =
   one-time/reversible world-state actions → **enables-family**, not continuous modifiers.
7. **CultureLevel** ✅ — `PrereqCultureLevel` (culturelevel→building: it's a CONDITIONER you must HAVE, so it
   inverts onto the level). `iCityRadius` → `identity` (lookup-with-override, not a deposit).
8. **Hurry** ✅ — config. `iGoldPerProduction`/`iProductionPerPopulation` (cost/production), `bAnger` gate.
9. **BonusClass** ✅ — categorization. `iUniqueRange` → `mapGeneration` (live map-gen spacing gate).
10. **CivicOption** ✅ — the civic SLOT/category axis (C++ uses `CivicOptionTypes` as the per-slot enum;
    civics page grouped Python-side). Text+identity, but structural (not inert).
11. **Property** ☐ — defines the `PROPERTY_*` channels (each → a split family). Diffusion/`ChangePropagators`
    (NEAR/SPREAD/GATHER) → **#429** (spatial leakage). Preserve the crime/disease unit→city emission via
    containment. PropertyManipulators = a self-reading sub-document, never a cascade channel.
12. **Civilization** ✅ — game-start `grants` + per-civ `policies` (playable/aiPlayable/stronglyRestricted,
    non-cascade). Source entity.

## Tier B — Top-of-cascade sources (enabled by tech; enable/condition downstream)

13. **Tech** ✅ — **THE spine root.** Enables techs(`And/OrPreReqs`)/buildings/units/builds/civics/religions/
    corporations/projects/processes/promotions/promotionLines/heritages/specialBuildings/improvements +
    bonus reveal/trade. Obsoletes buildings/units/builds/bonuses/corporations. **RETROFIT: retain child
    `AndPreReqs`/`OrPreReqs` as `requires.build.all`/`.any`** (store currently flattens to `enables.techs`,
    losing AND/OR — the tech-tree multi-parent fix). Tech modifiers are DOWNWARD deposits (`TechYield/
    Commerce/Happiness/HealthChanges` → `when:hasTech`-style `enabled` deposits authored ON the tech, §0.4/
    CREST — NOT inverted-onto-target, NOT a building reaching up). `FreeSpecialistCounts` → `freeSpecialists`
    modifier (NOT dead — Python-live). `bEnableDarkAges` → drop (TB mod, dead).
14. **Civic** ✅ (first heavy) — `TechPrereq`. Empire scope. Enables buildings/units (`PrereqCivic`/And/Or).
    maintenance/upkeep (cost-style), great-people, free-specialist, happiness/health. `revolution` kept
    faithful (Python→C++ port pending). `BonusCommerceModifiers` (CREST) → keep-on-civic via `per`/`enabled`
    (per §6, NOT folded). `policies` section. `iRevIdxSwitchTo`→`grants`/event; `iAnarchyLength`→`identity`.
15. **Religion** ✅ — `TechPrereq`. Enables buildings/units (`PrereqReligion`). Conditional commerce
    (stateReligion/holyCity/shrine) → `enabled` (was condition-as-member). `StateReligionCommerces` →
    `gold|…` deposits `enabled:{stateReligion}`.
16. **Corporation** ✅ — `TechPrereq` + `PrereqBonuses` + `PrereqBuildings`; `ObsoleteTech` (latent).
    `GlobalCorporationCommerce` → `per:{type:CORPORATION,scope:world}`-style count-scaled commerce.
17. **Trait** ✅ ("Mount Doom") — `TraitPrereq`/`TraitPrereqOr1/2` + `PrereqTech` (developing-leaders line).
    Source/enabler like civic (NEVER a target). ONE `CvTraitInfo` for both trait systems via
    `ReplacementID`/`CvInfoReplacements` (base + complex Types + `replacedBy`). Many bonus/tech/state-
    conditioned effects → keep-on-trait via `enabled`/`per` (the densest CREST set — confirm at #430).

## Tier C — Resources & map substrate (conditioners/leaves; before consumers)

18. **Bonus** ✅ — `TechReveal`/`TechCityTrade` (tech→reveal/trade); `TechObsolete`. Enables buildings/units/
    routes (it's a CONDITIONER → those invert onto the bonus). `buildRate` (the un-folded
    `BonusProductionModifiers` now authored on building/unit/project instead — **drop the fold from
    `curate_bonus`**). `BonusHealth/HappinessChanges`. ~45% are CULTURE intermediary bonuses
    (`Assets/Data/bonuses/cultures/`) — migrate the building→bonus→building chain faithfully; collapse in the
    post-migration purge. A resource is never a target — it only `enables` or amplifies (the "coal" test).
19. **Route** ✅ — `BonusType`/`PrereqOrBonuses` (bonus→route). Base movement = `identity` (own-stat, not a
    `movement` family). `TechMovementChanges` inverts onto the tech (drop from Route's authored JSON). On-map
    road art = a SEPARATE art entity (`CvRouteModelInfo`), not migrated here.
20. **Terrain** ☐ heavy — plot-leaf. Yields buried in `identity` (terrain is a TARGET producing yields, read
    directly — not a cascading source). `iHealthPercent` is genuinely DEAD on Terrain (never called against
    it). `iMovement` → identity. *(The live `healthPercent` is on Improvement/Feature/Specialist — §8-iv.)*
21. **Feature** ☐ heavy — plot-leaf. Yields, health/happiness/defense/movement the feature produces.
22. **Improvement** ☐ heavy (≡ its Build) — `PrereqTech`. `Improvement/GlobalImprovementYieldChanges`,
    tech-conditioned yields (→ `enabled:hasTech` deposits), `ImprovementFreeSpecialists`. `ImprovementUpgrade`
    = lifecycle (deferred outcome system, not a modifier). **`healthPercent` BALANCE-CUT from improvements**
    (the modifier capability is kept in general; not sourced from improvements — §8-iv). `DiscoverRand`/
    per-bonus `DepletionRand` = RNG odds, out of cascade but the per-bonus depletion-rand is KEPT (live gated
    mechanic, `MODDERGAMEOPTION_RESOURCE_DEPLETION`); root `m_iDepletionRand` is dead → drop.
23. **Build** ✅ — `PrereqTech`, `PrereqBonusTypes`, `FeatureStructs`/`TerrainStructs` PrereqTech; `ObsoleteTech`.
    The action laying improvements.

## Tier D — City/unit producers & unit-plane sources

24. **Specialist** ✅ — yields/commerce the specialist produces (`specialist` scope leaf), experience by
    unitCombat (the unitcombat is the TARGET → stays keyed). `FreeSpecialistCount` from Civic/Tech/Event =
    a grant on the SOURCE. `YieldChanges` dead-structure (read() reads only `<Yields>`).
25. **Heritage** ✅ — `PrereqTech` + `PrereqOrHeritage` (heritage→heritage succession). `byEra` conditional
    commerce → `enabled`; property deposits with active gates (gated-list shape).
26. **Project** ✅ — `TechPrereq` + `PrereqProjects/iNeeded` (N-of count → tally). `victory` (non-cascade).
    `YieldModifiers` → **DROP (nuked — a +10-commerce-per-plot wonder buff is rejected as nutty);** empire
    yield buffs, if ever wanted, are cheap in the locked structure later. `iTechShare` → enables/requires.
27. **PromotionLine** ☐ (rides Promotion, dead-last-ish) — `PrereqTech`. A grouping/hierarchy of promotions;
    accreted tech/prune gates, `buildUp`. `*ContractChanceChanges` → drop (dead, tooltip-only, system
    unimplemented). The line NEVER enables a building / adds no yield-commerce-property modifier (the
    individual PROMOTION owns property modifiers).
28. **Promotion** ☐ heavy — `TechPrereq`, `PrereqBonusTypes`, `PrereqPlotBonusTypes`. Combat bonuses buried in
    identity; the promotion owns city/plot property modifiers. **Unit-plane self-accumulator (§5).**
    `iDamageperTurn`/`iWeakenperTurn` → drop (dead BATTLEWORN). Invisibility/visibility LOS tables (the 2D
    `{Terrain|Feature|Improvement}[Range]` × InvisibleType) → non-cascade sub-object for the visibility
    RESOLVER (§0.6), NOT additive modifiers. `FreePromotionUnitCombatTypes` → `grants`.
29. **UnitCombat** ☐ heavy — combat mods buried (mis-classified as identity by the grounding). Free-experience,
    extra-strength deposits → unit-plane. `m_PropertyManipulators` → drop (dead write, not iterated).
    `bForMilitary`/`bForNavalMilitary` → identity (AI tags). Holds the Size-Matters base ranks
    (`getQualityBase`/`GroupBase`/`SizeBase`, `-10` sentinel) — **create-unit-subroutine data (§0.6), kept
    as-is** pending a Size-Matters pass; no `override` unit. **DEFINE the unit modifier vocabulary here**
    (combat/withdrawal/bombard/air-defense/movement/first-strike/… — the §5 gap).
30. **LeaderHead** ☐ — 90+ AI personality/diplo params → `ai` group. ~zero per-turn-effect cascade fields.

## Tier E — The monsters (most-targeted; depend on everything above)

31. **SpecialBuilding** ✅ (re-curate WITH Building) — `TechPrereq`/`TechPrereqAnyone`. A per-player-capped
    building-GROUP (`getMaxPlayerInstances`, enforced by `isBuildingGroupMaxedOut`/`getBuildingGroupCount`) —
    NOT an inert POCO. Shares building vocab; rides the Building pass.
32. **Building** ◐ MONSTER — target of tech/civic/religion/bonus/corp/cultureLevel; inter-building edges
    (`PrereqInCity/Amount/OrBuildings`) + OR/NOT `ConstructCondition`. The deepest modifier surface (~101
    channels; the grounding mapped it in full — see `modifier-cascade-mapping.json`). `iMinDefense` clamp
    lives in the `defense` family structure (additive `amount` + `min` floor). `CommerceChangeDoubleTimes` →
    a second age-gated `enabled` deposit (created-timestamp). `iPillageGoldModifier` → REVIVE as
    `pillageGold.empire.percent` (world-wonder, all units). One-shot pulses (population/goldenAge/founding)
    → `grants` (engine event-hooks, not info). `BUILDING_EFFECT_*` threshold pseudobuildings KEPT as-is
    (→ `PropertyEffect` later).
33. **SpecialUnit** ☐ (rides Unit) — combat%/withdrawal deposits onto the LOADED unit (unit-self scope);
    shares Promotion/UnitCombat combat/withdrawal vocab.
34. **Unit** ☐ MONSTER, **LAST** — target of everything (tech, building-prereq, bonus, religion, civic,
    promotion/unitcombat). `upgradesTo` = `succession` (manual, NOT `replaces`). GP action magnitudes
    (discover/hurry/trade/greatWork) + spawn pulses → `grants` (one-shot, not per-turn). `CorporationSpreads`.
    `iInstanceCostModifier` → `costs.empire.perInstance` `per:{type:SELF}` (the priority count-scaled case).
    `iWorkRate` base → identity (deltas are modifiers). RNG params (`iAirBombDefense`, discover/depletion
    rands, `nukeExplosionRand`) → out of cascade. Unit `requires.build` only (no `operate`/dormancy yet;
    future fuel-disable would be `requires.operate`).

## Phase F — FINAL ALIGNMENT PASS (after all infos migrated, BEFORE #430 parsing; owner 2026-06-16)

**Finish the info migration FIRST, then sweep back over every already-migrated info to bring them ALL into line with
the latest conventions.** The model evolves *during* migration — each entity sharpens a rule — so earlier-migrated
entities lag the conventions later ones established. One consistency sweep at the end is the countermeasure: *"I
don't want a future agent to go 'hurr?!?' because it's different"* (owner). Known divergences to reconcile (the list
grows as more accumulate):
- **Predicate modularity** (enabler-spec §3): treat Religion / Corporations / Traits (simple + complex) — and any
  concept we define as a system — as isolated systems; bare-string predicate forms; per-system self-documentation;
  ignore-not-false degradation. (The `workedBy: SELF` predicate also lands here / at Buildings.)
- **Art blocks — DONE 2026-06-16 (for all migrated entities).** Flat `art.*` → the three **top-level** blocks
  `ui` / `world` / `sound` (`art` a sub-block within `ui`/`world`; canonical map `curate_common.ART_BLOCK`, shared
  `put_art`/`emit_art` helpers). Applied to the cc-curated set AND all 11 art-bearing bespoke curators; entities not
  yet curated (Unit — heaviest — Building, Improvement, …) adopt it natively via `ART_BLOCK` at their pass. No
  retrofit owed. (building-cascade-conversion "ART BLOCKS … DONE".)
- **Any shape an entity locked AFTER an earlier entity was committed** — e.g. the family names
  defense/movement/cultureDistance/buildTime/`vision` (blessed at Terrain/Feature), the deliveryguy/semantic-sense
  ownership rule (modifier-spec §6.1), the dedicated-block rule (§0.8).
Done BEFORE #430 so the parser implements against uniform data — no mid-parse retrofit churn. **⛔ HARD GATE (owner,
emphatic 2026-06-16): do NOT start "whatever is next" (the #430 parser, or any new phase) until this alignment —
INCLUDING the art-block update — is complete.** Finish migration → align everything (arts included) → only then move on.

---

*Method per entity: curation, not transcription — ask "what do the two cascades need from this entity?",
pull that, then keep/drop/relocate/derive every remaining legacy field (drop re-check mandatory). Capture
each new ruling in the right spec (modifier→modifier-spec, enabler→enabler-spec, both immutable-unless-
conscious). Re-render `modifier-cascade-mapping.json` to the locked shapes before it feeds a curator.*
