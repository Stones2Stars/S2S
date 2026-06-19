# Migration entity ranking — full-context, annotated (owner-verified 2026-06-15)

The top-down curation order for #428, **annotated with the current agent's full context** so the next
agent inherits the reasoning (edges, modifier surface, status, decisions) rather than a bare list.

> **ROLE: this is THE authoritative curation-order artifact for #428 — the working document the curating
> agent executes from.** Orient via the [`docs/dev/README.md`](../README.md) "re-read after a compaction" banner
>
> + the LOCKED specs (`data-model-spec.md`, `enabler-cascade-spec.md` v0.3, `modifier-cascade-spec.md` v3,
> `tally-cascade-spec.md`), then work through this ranking top-down, entity by entity.

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

**On WHEN to write a handover (owner ruling 2026-06-16): write one when AT RISK OF CONTEXT COMPACTION, NOT every
session.** Compaction has consistently POISONED the context in the past, so the handover is the countermeasure deployed
*against that specific risk* — not a per-session ritual and not written unprompted at a session's end. Write a fresh
handover when the conversation is growing long enough that a compaction looks likely (or when the owner calls for one);
otherwise don't manufacture one. *(This refines the older "each session writes a new handover" framing, which overstated
the cadence.)*

**On HANDOVER MUTABILITY (owner clarification 2026-06-16) — read this so the next agent doesn't over-apply it.**
When a handover IS written it is a NEW file, so prior ones *rarely need* editing — but **prior handovers are NOT
immutable, and neither is any living plan doc.** "Rarely needed" ≠ "forbidden": edit/correct a prior handover (or any of these plan
docs) when there's a genuine reason — a misleading or over-absolute statement, a now-stale shape, a recorded ruling
that drifted. The cautionary incident behind the original "never edit a handover" line was *needless rewriting*, not a
ban; that absolute itself got misread as immutability and over-applied to `building-cascade-conversion.md` (a living,
frequently-edited doc). **"We didn't need to do X before" is not "X must never be done"** — a general guard against
hardening observations into false rules. (Dated per-session *status* notes inside a living doc are still best treated
as records of what that session did; correct them when they actively mislead, but don't manufacture churn.)

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

**Module inclusion verdicts (owner 2026-06-15) — which `Assets/Modules` migrate is CASE BY CASE, NOT a
folder-name heuristic.** `store.py` bakes in module content, but the authority for what the game actually
loads is `Assets/Modules/MLF_CIV4ModularLoadingControls.xml` — consult it per module. Resolved verdicts
(`store.EXCLUDED_MODULE_SUBPATHS = ["/modules/zwip/", "/modules/bad_karma/", "/modules/p2k_multimaps_test/"]`):
- **INCLUDE:** Cultures, Pepper2000, Thunderbrd (its loaded `Traits` sub), Alt_Timelines, NotSoGood.
- **EXCLUDE — zWIP:** work-in-progress, not loaded in real games; had smuggled in `PROPERTY_FRUIT/HUNT/
  MATERIAL/LORE` + `BONUSCLASS_EXTINCTION`.
- **EXCLUDE — Bad_Karma:** top-level `bLoad=1` but ALL content subs `bLoad=0`, so nothing loads.
- **EXCLUDE — P2K_Multimaps_Test:** `bLoad=0`; its 92 "space" units are a 100% duplicate of Pepper2000's
  (already loaded), zero P2K-unique — excluding the dead duplicate is fidelity-correct (net roster change zero).

Why it matters: modules add huge volumes to the heavy entities (BuildingInfo ~1567 module-added, UnitInfo
~1031) — migrating WIP/disabled module content commits dead experimental data. Re-check before each heavy pass.

**Old JSON is NOT a baseline (owner ruling 2026-06-15).** The committed `Assets/Data/*.json` predate the
locked v3/v0.3 model (and even post-lock ones can carry curator bugs); they were first-attempt drafts that
failed before the structure was set. "Matches the old file" proves nothing about correctness — validate by
structural adherence to the locked shapes + ZERO invention + reading the live C++/Python consumer, NEVER by
diffing against the old/committed JSON. Overwriting an old committed JSON to the correct v3 form is expected,
not a regression. ("Byte-identical to committed" keeps ONE narrow use: a safety check that a SHARED-code edit
didn't accidentally touch *other* entities — never a correctness signal.)

**Resume discipline — ascertain, don't appease (owner 2026-06-15).** When reversing a position on resume,
root the reversal in documented facts (the locked specs / owner rulings — look them up; they're almost always
already written down), NEVER to placate the owner. A fact-driven reversal is correct; reversing to appease,
or oscillating a value to please, is the error. Establish what is actually true FIRST, present the finding
with its source, then WAIT. (This is why the full-docs read above is mandatory — reconstructing the model
from memory or from how the C++ reads things is what produced the appeasing flip-flops.)

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
23. **Build** ✅ **(curated 2026-06-16 #4, `curate_build.py`)** — the worker ACTION. New `produces` FK section
    (improvement/route/terrain/feature + per-outcome tech/time/production) cleanly separated from those entities;
    own `requires.build` (tech + connected-bonus). Foldered by outcome (bonus/forts/routes/features/terraform/
    improvements/clearing). `PrereqTech`/`PrereqBonusTypes`/per-struct PrereqTech → store; `ObsoleteTech` → store.

## Tier D — City/unit producers & unit-plane sources

24. **Specialist** ✅ — yields/commerce the specialist produces (`specialist` scope leaf), experience by
    unitCombat (the unitcombat is the TARGET → stays keyed). `FreeSpecialistCount` from Civic/Tech/Event =
    a grant on the SOURCE. `YieldChanges` dead-structure (read() reads only `<Yields>`).
25. **Heritage** ✅ — `PrereqTech` + `PrereqOrHeritage` (heritage→heritage succession). `byEra` conditional
    commerce → `enabled`; property deposits with active gates (gated-list shape).
26. **Project** ✅ — `TechPrereq` + `PrereqProjects/iNeeded` (N-of count → tally). `victory` (non-cascade).
    `YieldModifiers` → **DROP (nuked — a +10-commerce-per-plot wonder buff is rejected as nutty);** empire
    yield buffs, if ever wanted, are cheap in the locked structure later. `iTechShare` → enables/requires.
27. **PromotionLine** ✅ **(curated 2026-06-16 #4, `curate_promotionline.py`)** — grouping/hierarchy axis for
    promotions. `loadPrune.onGameOptions`; `buildUp` = dedicated OBJECT MODULE (`{active:true}` interim marker);
    applicability gates parked in identity (→ Promotion pass). Dropped the dead OA-mod affliction line.
    `*ContractChanceChanges`/Categories/etc. dropped. The line NEVER enables a building / adds no modifier (the
    individual PROMOTION owns property modifiers). ⚑ `buildUp` baselines + applicability re-homing land at #28.
28. **Promotion** ✅ **(curated 2026-06-16, `curate_promotion.py`; 1229 records)** — the unit-plane stat SOURCE.
    DEFINED the §5 unit-stat family vocabulary (shared by UnitCombat #29 + SpecialUnit): **`strength`** = the combat
    family (general/flat/SM/situational/vs-keyed + S&D/TB sub-stats) + withdrawal/firstStrike/bombard/collateral/air/
    heal/movement/experience/workRate/cargo/upkeep/vision/`capture`(gradient)/poison/espionage/trap. **CAPABILITIES =
    separate boolean group**; LOS tables → non-cascade `vision` resolver (§7); **properties → scoped modifier deposits**
    (`PROPERTY_X.{plot,city}.flat`, via `property_source_v3` extended for `SAME_PLOT`); **`promotionLine:{LINE:rank}`**
    object (accumulator-shaped); availability PARKED in identity (deferred to the unit-plane enabling pass). Drops:
    BATTLEWORN trio + Categories + the `iStealthCombatModifier` typo. `ObsoleteTech` → new store edge. Full map: renames
    §Promotion. Original notes: **Unit-plane self-accumulator (§5).**
    `iDamageperTurn`/`iWeakenperTurn` → drop (dead BATTLEWORN). Invisibility/visibility LOS tables (the 2D
    `{Terrain|Feature|Improvement}[Range]` × InvisibleType) → non-cascade sub-object for the visibility
    RESOLVER (§0.6), NOT additive modifiers. `FreePromotionUnitCombatTypes` → `grants`.
29. **UnitCombat** ✅ **(curated 2026-06-16, `curate_unitcombat.py`; 814 records)** — the unit-combat-CLASS;
    REUSES Promotion #28's §5 vocabulary VERBATIM (the curator IMPORTS its tables, so they can't drift). `*Base`
    ranks → `identity.base` (§0.6 create-unit data; ⚑ Size-Matters clusters here → a dedicated cross-entity SM-module
    pass). `KillOutcomes`/`Actions` → `outcomes` (a DEFERRED mission-triggered/unit-accumulated grant — distinct from
    `grants`=now-if-enabled; the commerce burst is `AdaptUnitYield`/`missionYieldMultiplier`-scaled like a merchant
    mission → outcome-system pass + merchant `UnitInfo` later). religion/culture/era + `forMilitary`/`forNaval` + GGpts
    + defaultStatuses → identity (parked). `PropertyManipulators` empty/dead (0 emitted, §8-i confirmed). Drops:
    Categories + the wrong-tag FeatureAttacks/FeatureDefenses/iWithdrawalProb. Registered in `store.ENTITIES`. Full
    map: renames §UnitCombat. Original notes: combat mods buried (mis-classified as identity by the grounding). Free-experience,
    extra-strength deposits → unit-plane. `m_PropertyManipulators` → drop (dead write, not iterated).
    `bForMilitary`/`bForNavalMilitary` → identity (AI tags). Holds the Size-Matters base ranks
    (`getQualityBase`/`GroupBase`/`SizeBase`, `-10` sentinel) — **create-unit-subroutine data (§0.6), kept
    as-is** pending a Size-Matters pass; no `override` unit. **DEFINE the unit modifier vocabulary here**
    (combat/withdrawal/bombard/air-defense/movement/first-strike/… — the §5 gap).
30. **LeaderHead** ✅ **(curated 2026-06-16 #4, `curate_leaderhead.py`)** — ~90 AI personality/diplo params → the
    granular `ai` group (npc/flavours/personality/war/victory/trade/attitude/refuse/memory/contact/noWarProb/
    unitWeights/improvementWeights/favorites). ~zero cascade fields. Traits → `grants.{traits,developingTraits,
    complexTraits}` (faithful; simple→complex mirror deferred to #430). `LeaderHeadInfo` registered in store.
    The `ai` subgroup vocab is PROVISIONAL (reworked at the load-writing phase) — not locked in spec.

## Tier E — The monsters (most-targeted; depend on everything above)

31. **SpecialBuilding** ✅ (re-curate WITH Building) — `TechPrereq`/`TechPrereqAnyone`. A per-player-capped
    building-GROUP (`getMaxPlayerInstances`, enforced by `isBuildingGroupMaxedOut`/`getBuildingGroupCount`) —
    NOT an inert POCO. Shares building vocab; rides the Building pass.
32. **Building** ✅ **(curated 2026-06-16 #6, `curate_building.py`; 5202 records + SpecialBuilding #31's 36)** — MONSTER — target of tech/civic/religion/bonus/corp/cultureLevel; inter-building edges
    (`PrereqInCity/Amount/OrBuildings`) + OR/NOT `ConstructCondition`. The deepest modifier surface (~101
    channels; the grounding mapped it in full — see `modifier-cascade-mapping.json`). `iMinDefense` clamp
    lives in the `defense` family structure (additive `amount` + `min` floor). `CommerceChangeDoubleTimes` →
    a second age-gated `enabled` deposit (created-timestamp). `iPillageGoldModifier` → REVIVE as
    `pillageGold.empire.percent` (world-wonder, all units). One-shot pulses (population/goldenAge/founding)
    → `grants` (engine event-hooks, not info). `BUILDING_EFFECT_*` threshold pseudobuildings KEPT as-is
    (→ `PropertyEffect` later).
33. **SpecialUnit** ✅ **(curated 2026-06-16 #6, `curate_unit.py` `curate_special_unit`)** — cargo-load config (bValid/cityLoad/smLoadSame); registered in store.
34. **Unit** ✅ **(curated 2026-06-16 #6, `curate_unit.py`; 2073 records)** MONSTER, **LAST** — target of everything (tech, building-prereq, bonus, religion, civic,
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

+ **Predicate modularity** (enabler-spec §3): treat Religion / Corporations / Traits (simple + complex) — and any
  concept we define as a system — as isolated systems; bare-string predicate forms; per-system self-documentation;
  ignore-not-false degradation. (The `workedBy: SELF` predicate also lands here / at Buildings.) **✅ Membership-predicate
  reconcile DONE 2026-06-16 (owner, hole #1):** `HAS_TERRAIN`/`HAS_FEATURE`/`HAS_BONUS` are canonical single-valued
  predicates; Improvement #22's `{terrain|feature|bonus:[…]}` is the compact membership SUGAR (desugars to `any`-of-`HAS_X`,
  no data churn). `COASTAL_LAND` unused in real data (0)→moot; `IS_COASTAL` (`CvCity::isCoastal`) stays distinct. Cleared
  229 conformance flags. (data-model-spec §2.5, enabler-spec §3.)
+ **Art blocks — DONE 2026-06-16 (for all migrated entities).** Flat `art.*` → the three **top-level** blocks
  `ui` / `world` / `sound` (`art` a sub-block within `ui`/`world`; canonical map `curate_common.ART_BLOCK`, shared
  `put_art`/`emit_art` helpers). Applied to the cc-curated set AND all 11 art-bearing bespoke curators; entities not
  yet curated (Unit — heaviest — Building, Improvement, …) adopt it natively via `ART_BLOCK` at their pass. No
  retrofit owed. (building-cascade-conversion "ART BLOCKS … DONE".)
+ **Instance/category CAPS → `allowed` — DONE 2026-06-17 (owner).** Instance caps are NOT a `requires` SELF-atom (that
  conflated *needed* with *allowed* and forced an off-by-one — `max:0` for "cap 1"). They are the declarative `allowed`
  ceiling, authoring the REAL number: self-cap `allowed:{<scope>:N}` (Building/Unit/Tech `bGlobal`/SpecialBuilding group
  cap — `SELF` left `requires` entirely), per-city category-cap `allowed:{worldWonders|teamWonders|nationalWonders:N}`
  (CultureLevel, moved out of `identity`). Engine enforces (build while `tally.count < allowed`) + owns the opt-outs
  (`NO_WONDER_LIMIT`/`NO_NATIONAL_UNIT_LIMIT`/`CHALLENGE_ONE_CITY`), era-scaling, `+extra`, and the per-unit
  `unlimitedException`. OCC carries no separate cap (forces limits off; future option-specific limits → game-option-specific
  JSON via override-by-design). Applied across Building/SpecialBuilding/Tech/CultureLevel/Unit; harness recognizes/renders
  `allowed`; 0 conformance flags held. (data-model-spec §4.2a, enabler-spec §5a/§13.7, tally-spec §2, renames §§Building/
  SpecialBuilding/CultureLevel/Tech/Unit.)
+ **Any shape an entity locked AFTER an earlier entity was committed** — e.g. the family names
  defense/movement/cultureDistance/buildTime/`vision` (blessed at Terrain/Feature), the deliveryguy/semantic-sense
  ownership rule (modifier-spec §6.1), the dedicated-block rule (§0.8).
+ **MODIFIER OWNERSHIP REVIEW — up/down placement, "who BRINGS the modifier" (owner ruling 2026-06-16).** Once all
  infos are moved, do a deliberate pass to confirm the correct OWNER (and scope direction, up vs down) of every
  cross-entity modifier against the deliveryguy/§6.1 rule. **Specifically the TECH-MODIFIER GATES** (the `Tech*Changes`
  inverted ONTO the tech: `curate_tech` TECH_BOOSTS — Building/Improvement/Specialist/Route `TechYieldChanges` etc.,
  §0.4) were decided BEFORE the §6.1 "who brings the modifier" refinement, so they are PROVISIONAL — re-judge whether
  each belongs on the tech (downward deposit) or keep-on-source on the delivering entity. Carry the affected entities'
  inversions as-is until this pass; flag at each entity (Improvement #22 drops `TechYieldChanges`, deferring to the
  existing tech gate pending this review).
Done BEFORE #430 so the parser implements against uniform data — no mid-parse retrofit churn. (Earlier framed as a hard
"finish all alignment before any #430 work" gate; the art-block update IS complete.)

**↻ REFRAMED — Phase F is a LIGHT pass, NOT an exhaustive hunt (owner 2026-06-16).** A quick enabler/requires placement
AUDIT (2026-06-16, across all 34 migrated entities) confirmed the **structure is fundamentally SOUND**: every conditioner/
generator carries the `enables` family on the SOURCE, every buildable target carries `requires`; tech only ever appears in
`requires.build` as a confirm (never a generator); the build/operate split is right for the fluid means (civic/religion/
corp → operate dormancy; units → build-only as leaf actions). So Phase F doesn't need to find everything up front — **we
will surface and fix more alignment issues as we walk through WIRING #430** (owner: don't over-invest in an exhaustive
audit). Known items to carry into the wiring (fix when hit, not as a blocking pre-pass):

+ **build-vs-operate for CONTINUOUS building gates:** building **resource** (`bonus`, ~6.6k) and **power** (`HAS_POWER`,
  ~1k) prereqs currently sit in `requires.build` (grey-only); per the grounding they are the continuous `CvCity::isActiveBuilding`
  gates → they should `operate` (grey at build AND go dormant when the input is lost). Verify vs `isActiveBuilding`, then move.
+ **grey-vs-hide consistency:** buildings/units put bonus prereqs in `requires` (grey); routes invert theirs to
  `bonus.enables.routes` (hide). Same concept, two treatments — pick one convention.
+ the previously-listed items (predicate modularity + `IS_COASTAL`/`HAS_FEATURE` reconcile, modifier-ownership/tech-gate
  review, family names) — handle as encountered.
+ minor: the SpecialBuilding per-group instance cap is in `identity.maxPlayerInstances`, not a member-building `requires` `max`.

## Tier G — UNRANKED gameplay infos / "stragglers" (SCOPE VERIFIED 2026-06-16)

**The actual remaining scope, established by a verification pass (2026-06-16):** cross-referenced the FULL info universe
(`CvGlobals.h` vector registry — ~91 `Cv*Info` classes) against the 35 migrated entities (#1–34 + partial `YieldInfo`),
then adversarially triaged every non-art remainder against the real source (record counts incl. modules, data members,
live consumers, dead-or-not). **"Nothing is easy" held — several earlier Tier-G guesses were WRONG** (corrections below).
Of ~56 unmigrated classes, ~38 are the art/UI/engine tier (out); the real remaining work is the ~13 small/medium
gameplay entities below + a little config. **Owner rulings folded in: the EVENT SYSTEM is deferred to its own rework
(#425); `CvSpawnInfo` is the barb/animal/wildlife spawn system.** Each still triaged per-info before any drop (§0 "no
true POCO"; entity purges are a SEPARATE deliberate pass, not done mid-migration).

**GAMEPLAY — to migrate (the small/medium tail).** *(`CvOutcomeInfo` was attempted but DEFERRED to #430 — see "Deferred" below.)*

+ **`CvSpawnInfo`** (327) — **the BARBARIAN / ANIMAL / WILDLIFE map-spawn system** (owner 2026-06-16); 28 fields,
  tech/date/terrain/density-gated, `TreatAsBarbarian`/neutral-only + a BoolExpr spawn condition. Large, dense.
+ **`CvGoodyInfo`** (106) — the DEFINITIONS of the possible goody-hut grants (gold/xp/heal/unit/research, era/tech-gated) —
  a `grants`-shaped entity. **WHICH goodies appear + their weighting lives on the Handicap, NOT here** (owner 2026-06-16:
  "possible grants based on naming of goodyhut, referenced in handicap") — VERIFIED: `CvHandicapInfo` has `<Goodies>` and
  the migrated Handicap #2 already carries it as `identity.goodies` (per-difficulty `GOODY_*` ref lists). So GoodyInfo
  migrates only the grant DEFINITIONS those refs point at. Medium.
+ **`CvEspionageMissionInfo`** (29) — espionage missions; PROCEDURAL one-shot effects (not modifiers) + tech/option gate. Small.
+ **`CvEmphasizeInfo`** (8) — city production-emphasis (yield/commerce modifiers + avoid-growth/angry/unhealthy flags);
  **LIVE AI + UI** (#367–370). *(Was wrongly in the OUT list.)*
+ **`CvVoteSourceInfo`** (3) — UN / Apostolic Palace / Congress; free specialist + per-religion yields/commerce (Vote #6 deferred it here).
+ **`CvCommerceInfo`** (4) — the 4 commerces' config (initialPercent, initialHappiness, AI weight, flexible). Sibling to `YieldInfo`.
+ **`CvUpkeepInfo`** (4) — civic-upkeep tiers (populationPercent / cityPercent), read every turn for civic gold cost.
+ **`CvWorldInfo`** (8) — **PREGAME config (owner 2026-06-16): map SIZE is chosen at setup then FIXED** — limited *ongoing*
  gameplay impact; its 8 "modifiers" (distance/colony/corp/numCities maintenance%, conscript%, trade%, anarchy%,
  building-prereq%, city-limits scale%) are map-size-DERIVED CONSTANTS, not dynamic cascade deposits. **SPLIT** the map-gen
  grid/grain half from the gameplay-constant half; migrate as a pregame-config section, NOT cascade. *(Was not flagged at all.)*
+ **`CvMapCategoryInfo`** (17) — **PREGAME config (owner 2026-06-16):** a pure enum (Type/Description) that gates which
  buildings/units/bonuses exist on which map kind (`m_aeMapCategoryTypes`) — set by the map at setup, fixed; "pregame, kinda"
  not ongoing gameplay. Migrate as a slim enum registry; the membership lives on the gated entity. *(Was in the OUT list.)*
+ **`CvInvisibleInfo`** (14) — thin registry; the LOS resolver keys on the `INVISIBLE_*` ENUM, only the `intrinsic` flag is
  gameplay → slim registry, LOW priority. *(Not the meaty gameplay entity earlier assumed.)*
+ **finish `CvYieldInfo`** — the min-city/golden-age/trade/colour/symbols remainder (hills/peak/river already on the terrains).

**CONFIG / map-gen (migrate as config sections, NOT cascade):** `CvClimateInfo` (5), `CvSeaLevelInfo` (4) — pure map-gen
latitude/sea params; `CvTurnTimerInfo` (6) — UI turn-pacing; the `CvWorldInfo` grid half.

**DEFERRED to their OWN system reworks, NOT migrated faithfully in #428 (owner rulings 2026-06-16):**

+ **`CvOutcomeInfo`** (134) → the **#430 outcome-system pass.** A standalone migration was AUTHORED then BACKED OUT. The
  entity is **misnamed and is only the GATING HALF of a two-part system.** "Outcome" implies a RESULT, but `CvOutcomeInfo`
  holds **no result** — it is the *eligibility/definition* side: WHEN may a result-type fire and at what odds (tech/civic/
  building prereqs + ObsoleteTech, allowed territory, on/off-city, capture flag, per-promotion chance deltas, supersession)
  + a display message. The **actual CONSEQUENCE** (subdue → `UNIT_SUBDUED_X`; hunting-kill → yields; …) lives **per-unit on
  the `CvOutcome` INSTANCE** in each unit's `KillOutcomes`/actions — already carried into the unit JSON, but in RAW faithful
  form (`iChance` / positional `Yields` / `Constant`/`Random` expr trees), NOT cleaned. Cleaning the gating half alone, under
  a misleading name, while the result half sits raw, is half a job — **#430 reworks BOTH halves (and the name) together.**
  Owner 2026-06-16: *"Outcome is the worst naming possible — it's the mission IN, not the result of a mission."* (The backed-out
  gating shape, if useful later: requires.build + dedicated `territory`/`location` + `extraChance` + `replaces.outcomes` +
  `identity.{capture,toCoastalCity}`.)
+ **`CvEventInfo`** (873) + **`CvEventTriggerInfo`** (509) → the **EVENT-SYSTEM REWORK (#425).** The random-event monster: ~half of all remaining records,
  48/65+ fields, sparse building-yield vectors + property prereqs, and **766 Python callbacks** (433 + 333). The system is
  "OOS-prone and catastrophically coded" (#425), so it gets a REWORK, not a port — its data migration rides that pass.

**DEAD — flagged for the separate purge pass (verified zero runtime consumers; NOT auto-dropped, §0):**
`CvIdeaInfo` (2) + `CvIdeaClassInfo` (2) — save/load enumeration only; `CvCategoryInfo` (56) — no `getCategory` consumer
(the `Categories` lists on entities are never queried — confirms the long-suspected dead-Categories). *(Idea/IdeaClass were
mis-listed as gameplay candidates.)*

**OUT of cascade (art/UI/graphics/audio/engine — no migration):** GameOption/MPOption/PlayerOption/GraphicOption/ForceControl,
World(grid half)/Map(→ multimap rework)/ModLoadControl, Action/Advisor/Animation*/Camera*/Color/Command/Control/Cursor/
EntityEvent/HallOfFame/InterfaceMode/Landscape/MainMenu/Mission/PlayerColor/Popup/Replay/*Model/SlideShow*/SpaceShip/
ThroneRoom*/UnitArtStyle/UnitFormation/WaterPlane/WorldPicker/TerrainPlane/Automate; **`CvAttachableInfo`** (72, art/FX
particle paths, `DllExport getPath` — NOT equipment, corrected); **`CvDiplomacyInfo`** (110, live UI diplomacy TEXT only —
note #359's "dead class" is the sibling `CvDiplomacyTextInfo`, not this). + the deferred ART-DEFINE tier (building-cascade-conversion).

---

*Method per entity: curation, not transcription — ask "what do the two cascades need from this entity?",
pull that, then keep/drop/relocate/derive every remaining legacy field (drop re-check mandatory). Capture
each new ruling in the right spec (modifier→modifier-spec, enabler→enabler-spec, both immutable-unless-
conscious). Re-render `modifier-cascade-mapping.json` to the locked shapes before it feeds a curator.*
