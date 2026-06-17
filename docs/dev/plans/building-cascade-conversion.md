# Building → cascade/JSON conversion plan (#428)

Output of the `classify-building-fields` workflow (2026-06-13): every field in
`CvBuildingInfo::getDataMembers` (lines 1631–1926) classified for migration to the JSON
cascading-modifier structure, cross-checked against `cascade-modifier-inventory.md` and the live
`change*`/`set*` accumulation sites in `CvCity.cpp`. Companion to [[json-native-modifier-data]].

## THE MODEL (locked 2026-06-14) — read this first; it governs everything below

Captured from the owner design session on 2026-06-14. This supersedes the framing in the rest of
this file; the later sections retain useful field-level inventories and per-entity verdicts but are
subordinate to this when they conflict (e.g. whole-entity `writeJson`, byte/effect-parity gating,
and mass-migration are NOT how we proceed).

---
### ⇒ CHECKPOINT & RESUME (2026-06-14) — START HERE to pick up the migration

> **⏩ LATEST: read [`handover-2026-06-14-pm.md`](handover-2026-06-14-pm.md) FIRST** — the PM-marathon
> session handover (Trait DONE; equipment-tier combat purge; the "unreferenced ≠ dead" lessons;
> SpecialBuilding re-curate-with-Building-pass flag; combat-class audit + #434/#435; lighter-four
> classified-not-curated; indices live on main). It supersedes the per-line state below where they differ.

**State:** branch **`json-data-migration`** (~19 commits off `origin/main`, working tree clean). The #421/#423
cascade **C++** stays on `buildings-json` for its own PR — this branch is JSON DATA + the offline Python
toolkit only (C++ readers come dead-last). The model below is LOCKED; read §0–§7 + "HEAVY-ENTITY CAUTIONS".

**DONE (19 entities — curate → verify-vs-C++ → write → commit):**
- Gameplay (careful, verified against the live C++ consumer): **Tech 943 · Bonus 907 · Handicap 12 ·
  GameSpeed 9 · Era 14 · Process 13.**
- POCO data-holders (fast-path, `curate_pocos.py`, text/identity/art only): **BonusClass 12 · CivicOption 15 ·
  Hurry 2 · Victory 10 · PromotionLine 334 · SpecialBuilding 36.**
- **Light batch — COMPLETE (2026-06-14, all 6 medium entities, each its own bespoke curator + commit, verified
  vs the C++ consumer):** **Route 21 · Project 25 · Heritage 113 · Religion 29 · Specialist 39 · Corporation 23.**
  See "LIGHT-BATCH DECISIONS (2026-06-14)" just below the NEXT block — several are now cross-entity CONVENTIONS
  (cost-vs-costs, condition-as-member, inversion-vs-target-keying, dead-structure drops, gated-property lists).
- **Civic — FIRST HEAVY entity, DONE (2026-06-14): 175, category-foldered.** Both enabler+modifier; empire scope;
  `policies` section; `revolution` kept faithful (Python→C++ port pending); grouped `stateReligion`. Full detail
  + all rulings in the §7 "Civic" note. The big architecture work it triggered (the **cascade ontology** — one
  cascade, every per-turn-effect producer is a target, sources/enablers never targets, the coal test, the
  stay-vs-invert rule, and the unresolved **"CREST"**) is recorded in **§0**.
- **Trait — SECOND HEAVY entity / "Mount Doom", DONE (2026-06-14): 390** (87 base + 240 developing-line + 63
  complex). ONE `CvTraitInfo` class for both trait systems (vanilla DefaultTraits vs developing DefaultComplexTraits,
  game-option-gated — assignment is consumer-side). The two-system link is the **`ReplacementID`/`CvInfoReplacements`
  conditional whole-Info replacement** the classify workflow MISSED (and `store.py` was Frankenstein-merging) — now
  split into base + complex Types + a `replacedBy` edge. Full rulings + that discovery in the §7 "Trait" note. Riders:
  `store.py` (ReplacementID-aware + trait prereq edges), `curate_bonus.py` (BonusHappiness fold), tech
  `workerSpeed`→`workRate`.
- **⚠ OWNER RULING (2026-06-14) — NO ENTITY IS TRULY A POCO; it can only PRETEND to be.** A "this is just a
  data-holder" verdict is a HYPOTHESIS to disprove against the live C++ consumer, NEVER a fast-path to trust. The
  first-pass `mapping/*.json` 0-channel classification is a FILTER, not proof — it systematically under-classifies
  hidden gameplay. Vindicated repeatedly: the "lighter four" (SpecialUnit/CultureLevel/Vote/Civilization) ALL turned
  out to be SOURCES, and **SpecialBuilding — curated as a POCO — is actually a building-GROUP with a per-player build
  cap** (`getMaxPlayerInstances`; enforced by `CvPlayer::isBuildingGroupMaxedOut`/`getBuildingGroupCount`,
  CvPlayer.cpp:13784-13867). **CONSEQUENCE:** every entity already fast-pathed via `curate_pocos.py` (BonusClass,
  CivicOption, Hurry, Victory, PromotionLine, SpecialBuilding) must be RE-VERIFIED against `getDataMembers` + its
  consumers before being trusted as inert — a POCO must be PROVEN inert, not assumed. (SpecialBuilding re-curates
  with the Building pass; see `handover-2026-06-14-pm.md`.)
  - **AUDIT DONE (2026-06-14 PM, wf verify-pocos — 5-entity parallel classify + adversarial verify): of the 6
    fast-pathed POCOs, ZERO are truly inert data-holders** — five carry mis-homed gameplay and the sixth (CivicOption) is a
    structural category/slot axis (not inert either; see below). Mis-classified:
    - **BonusClass** — `iUniqueRange` is a live map-gen placement gate (`CvMapGenerator::canPlaceBonusAt`,
      CvMapGenerator.cpp:69, enforces min class-spacing). Data IS preserved (under identity) but it is NOT a
      pure data-holder; `iUniqueRange` wants a `mapGeneration` home.
    - **Hurry** — `iGoldPerProduction` + `iProductionPerPopulation` (cost/production modifiers,
      CvCity.cpp:4089/4090/6086-6119) + `bAnger` (anger-duration gate, CvCity.cpp:6144).
    - **Victory** — 13 live fields read every turn by `testVictory` (CvGame.cpp:6081): 5 capability-gate bools
      (conquest/endScore/targetScore/diploVote/totalVictory) + 8 numeric thresholds (religionPercent/cityCulture/
      numCultureCities/totalCultureRatio/populationPercentLead/landPercent/minLandPercent + victoryDelayTurns).
      A pure CONFIG entity (victory conditions), like Handicap/GameSpeed/Era.
    - **PromotionLine** — 8 live fields: Prereq/Obsolete tech gates (PrereqTech already inverted to top-down
      `enables` in store; ObsoleteTech flows to promotions via CvPromotionInfo.cpp:653), `bBuildUp`,
      NotOn{GameOption,Domain,UnitCombat} prune gates, unitCombat/tech `contractChanceChanges`. **Modifier
      surface (owner 2026-06-14, for the Promotion pass):** a PromotionLine can be ENABLED from many places but
      NEVER enables a building and adds NO yield/commerce/property modifier — the individual PROMOTION owns any
      property modifier (city/plot), NOT the line. The line SHOULD be just a grouping/hierarchy of promotions, but
      (the recurring "group-ish but not really" pattern) it has accreted extra fields — tech gates, prune gates,
      buildUp, contractChanceChanges, a `PropertyType` ref; units-only. Sort it at the Promotion pass.
    - **CivicOption** — the civic CATEGORY/SLOT axis (the lone survivor, no smuggled modifier). Per-record data
      is correctly just text+identity, BUT it is NOT inert: it defines the civic SLOTS (C++ uses
      `CivicOptionTypes` as the per-slot active-civic enum index — `getCivics`/`setCivics`, CvPlayer.cpp:479/
      4086-4094) and the civics-page organization is read PYTHON-side (the page was redone long ago —
      `CivicData.py:15` groups civics by `getCivicOptionType`); C++ reads only TEXT off the `CvCivicOptionInfo`
      object. `m_bPolicy` is dead (shadowed by per-civic `isPolicy`). So the POCO output (text+identity) is RIGHT
      for it, but it is a structural axis — confirming there is NO truly-inert entity (the "no true POCO" rule at
      6-for-6).
    - **⚠ KEY: the fast-path did NOT delete these — `curate_common` dumps every unclassified field into
      `identity`, so the DATA is preserved but MIS-HOMED** (a modifier/gate parked in `identity`, which the
      cascade engine won't read as a modifier). The fix is RE-HOMING (proper curators), not data recovery.
  - **`Special*` entities RIDE WITH THEIR PARENT MONSTER (owner ruling, 2026-06-14 PM).** SpecialBuilding → the
    Building pass (a per-player-capped build-GROUP of buildings, sharing building vocab); **SpecialUnit → the
    Unit pass** (its `iCombatPercent`/`iWithdrawalChange` deposit onto the *loaded unit* — a unit-stat source
    sharing Promotion/UnitCombat's combat/withdrawal family vocab, with the unit-self scope the Unit phase
    defines). Do NOT curate either early/standalone — defer both to LAST with their parent. So of the "lighter
    four", only **CultureLevel · Vote · Civilization** are curated now; SpecialUnit defers.
  - **Net curate_pocos action:** Victory + Hurry → bespoke config curators (done); BonusClass `iUniqueRange` →
    `mapGeneration`. Then `POCOS` settles at `[CivicOptionInfo, PromotionLineInfo, SpecialBuildingInfo]` — only
    `CivicOption` is the permanent text+identity axis; the other two ride their PARENT monster (owner ruling):
    **PromotionLine → the Promotion pass, DEAD LAST (bottom of the totem pole; only affects units)**;
    SpecialBuilding → the Building pass; SpecialUnit → the Unit pass. All three stay as mis-homed POCO
    placeholders until their parent pass re-curates them (preserved-but-mis-homed in `identity`; nothing reads
    them yet).

**TOOLKIT** (`Tools/Migration/`, all offline Python, run with `python curate_<x>.py --sample <TYPE>` / `--write`):
- `store.py` — XML-as-DB: loads every gameplay Info from base XML + `Assets/Modules` (merged by Type, content
  preserved), builds the generic **enable/obsolete reverse-index** by inverting prereq fields. `ENTITIES` dict
  registers each entity + its glob; add a new entity there before curating it.
- `curate_common.py` — the SHARED CORE: `apply_channel` (scope-wide → flat family), `accumulate_boosts`
  (entity-targeted inversions, family-first), `EntityConfig` (driven by `mapping/<Entity>.json`), `curate`
  (assembles type/text/enables/obsoletes/**families**/grants/ai/art/identity), `SPLIT_FAMILIES = {yield,
  commerce}`, `FAMILY_ORDER`. Thin entities ride this via a tiny config (curate_tech/bonus/process).
- bespoke curators (own field tables) for the special ones: `curate_handicap.py`, `curate_gamespeed.py`,
  `curate_era.py`. `curate_pocos.py` batches the data-holders. `engine.py` = shared helpers
  (text/generic/named_array/formula_node/clean_property_source/FIELD_RENAME/plural).
- `mapping/<Entity>.json` — first-pass field classification from the 2026-06-13 workflow. **A FILTER, NOT
  GOSPEL** — it under-classified real gameplay (Promotion/Terrain/UnitCombat/Property) and mis-scoped many
  fields. Always verify against the live C++ consumer.
- `classifications/` — **the resume analysis**: adversarial-workflow per-field dispositions for **Era**
  (`era-classification.json`) and the **7-entity light batch** (`light-batch-classification.json`). Curate the
  remaining light entities FROM these (don't re-run the ~1.1M-token workflows).

**WORKFLOW PATTERN (for heavy/uncertain entities):** author a `classify-<entity>-fields` Workflow — parallel
"understand" agents (C++ consumer / consumption sites / XML+mapping) + an adversarial "verify" agent — that
returns per-field dispositions; then write the curator from them. Proven on Era + the light batch (caught real
scope/modifier-vs-identity errors the mapping had). The scripts are under
`<session>/workflows/scripts/classify-*.js` (reuse as templates).

**NEXT — heavy entities** (the light batch is DONE; see below). Skip straight to the "THEN — heavy entities"
block. The medium-batch curators (`curate_route/project/heritage/religion/specialist/corporation.py`) are
worked examples of every pattern the heavy entities reuse.

**LIGHT-BATCH DECISIONS (2026-06-14) — now cross-entity conventions; consult before the heavy entities:**
- **`cost` (intrinsic base) ≠ `costs` (multiplier family).** A thing's own base cost is a `cost` section
  (Project `cost.create`, Corp `cost.spread`); the GameSpeed/Era percent MULTIPLIER is the `costs` family.
  Don't file a base gold/hammer cost under `costs`. (Corrected the classification twice.)
- **Condition-as-member** for a condition that isn't a scope: Heritage `<commerce>.empire.byEra.<era>.flat`
  (era = threshold, `eEra>=band`); Religion `<commerce>.city.{stateReligion|holyCity|shrine}.flat` (the 3
  conditional commerce tables). Parallel to the model's `byTech`. Era keys are the era Type verbatim
  (`C2C_ERA_*`) — a data-driven Type referenced like `BONUS_*`/`TECH_*` (owner convention), matching how techs
  carry `era: C2C_ERA_*`; resolve to the era file via `type.lower()`.
- **Inversion vs target-keying — the rule that prevents double-authoring:** a field on entity A keyed by
  entity B inverts ONTO B (and is DROPPED from A) IFF B is the CONDITIONER (a gate you must have). If B is the
  TARGET being modified, it STAYS on A keyed by target. Worked: Project `BonusProductionModifiers` → already on
  the BONUS (`buildRate`), dropped from Project. Specialist `TechHappiness/HealthTypes` → already on the TECH
  (`curate_tech` TECH_BOOSTS), dropped from Specialist. BUT Specialist `UnitCombatExperienceTypes` → the
  unitcombat is the TARGET → stays as `experience.city.unitCombats.{UC}.flat`. **The entity migrated FIRST owns
  the shared edge; later entities conform** (this is why the migration order is config→…→monsters). Always grep
  the already-committed curators (`curate_tech`/`curate_bonus` BOOSTS, the source's own curator) before
  authoring a keyed field, to avoid writing it twice.
- **Dead-STRUCTURE drops** (verify no member/getter/consumer first): Project `YieldModifiers` +
  `bTechShareWithHalfCivs`; Specialist `YieldChanges` (read() reads only `<Yields>`). Live values are never
  lost — these fields are genuinely unread.
- **Gated property deposits:** a property family is a per-scope LIST of `{unit: amount, active?: <gate>}` dicts
  preserving the `<Active><Has>{GOM_*:…}</Has></Active>` condition (Heritage/Religion/Specialist). Handicap's
  scalar-collapse can't carry the gate or multiple same-property sources — so the scalar-vs-list property-family
  shape across entities is a FLAGGED reconciliation for the property/#429 pass.
- **Non-cascade structural section** for gate-but-don't-sum data: Project `victory`
  (thresholds/minThresholds/delayPercent/successRate), read by victory resolution, not a modifier family.
- **advanced-start** → `identity.advancedStart` everywhere (Route joined Handicap/Era); a separate pre-game
  points currency pending the advanced-start review.
- **Movement base cost** is identity, not a `movement` family (Route): an intrinsic own-stat read directly, not
  summed onto a scope accumulator; the cascading movement modifier is the per-tech route delta (on the tech).
- **DEFERRED to the heavy phase** (source-side edges other entities declare, not the migrated entity's data):
  Corp's `BuildingInfo.FoundsCorporation` (HQ-founding), `UnitInfo.CorporationSpreads`,
  `BuildingInfo.GlobalCorporationCommerce`; Specialist's `FreeSpecialistCount` (Civic/Tech/Event — a grant on
  the source, not a per-turn modifier). Author these when Building/Unit/Civic are curated.

**TOOLKIT GROWTH this batch** (`curate_common` stayed the shared core; bespoke curators model the rest):
- `curate_common.EntityConfig` gained `families` (verified per-field specs OVERRIDING the under-classified
  mapping channels), `id_rename` (per-entity identity key names), `to_identity` (force a field into identity at
  a dotted path, overriding cost/art classification); `apply_channel` honours an explicit grouped `member`.
- `store.py` gained edges: `HeritageInfo.PrereqOrHeritage/Type` (heritage→heritage succession),
  `CorporationInfo.PrereqBuildings/BuildingType` + `BuildingInfo.PrereqCorporation` (corp↔building), and
  registered `TraitInfo` (table access for Specialist's trait inbound boosts; no enabler edges).
- C2C container-tag inconsistency bites `accumulate_boosts`: Building/Trait `Specialist…Changes` are PLURAL
  containers; always check the real container tag (`rec.find` returns it, `_boost_entries` iterates its rows).

**THEN — heavy entities (EACH gets its OWN `classify-<entity>` workflow — see HEAVY-ENTITY CAUTIONS in §7):**
Civic (decades of cruft, 104ch) · Trait (TWO game-option-alternative trait SYSTEMS over one shared `trait`
interface — VERIFY same `CvTraitInfo` class vs a separate "complex trait" class; 68ch) · Improvement · Terrain
(yields buried in identity) · UnitCombat (combat mods buried) · Promotion (combat bonuses buried) · Build ·
Property (diffusion/`ChangePropagators` — #429-adjacent) · LeaderHead (90+ AI params → `ai` group) → then the
MONSTERS **Building** (101ch + 22 inversions + 47 prereqs; inter-building A→B + OR/NOT `ConstructCondition`) +
**Unit** LAST.

**ART BLOCKS `ui` / `world` / `sound` — DONE for all migrated entities (owner 2026-06-16; final shape locked).** The
flat `art` section regrouped into THREE **top-level** dedicated blocks (NOT `art.ui`/`art.world`/`art.sound` — the
SUBSYSTEM is the top-level block, with `art` a SUB-block *within* `ui`/`world` so non-art members like key-triggers
sit BESIDE art):
- **`ui`** — on-screen surface. `ui.art.icon` (UI icon ← `Button`/`Texture`), `ui.art.texture` (a 2nd distinct UI
  icon where one exists — Specialist), `ui.art.movie.{file,sound}`, `ui.art.{techButton,genericTechButton,advisor,
  fontButton,tgaIndex}`, plus non-art `ui.{hotkey,altDown,…}` key triggers.
- **`world`** — on-map / 3D. `world.art.icon` (on-map graphic ← `ArtDefineTag`), `world.art.{style,unitStyle,
  playerColor}`, `world.art.effect.{type,probability}`.
- **`sound`** — FLAT (audio is itself the asset): `sound.{footsteps,soundscape,construct,onCompletion,sound,action,
  selection,unitVictory,unitDefeat,soundtracks,introSoundtrack,citySoundscapes,…}`.

This is the dedicated-block rule (modifier-spec §0.8) applied to art, and it **dissolves the "icon headache"**: both
icons are named `icon`, disambiguated by the block — **`ui.art.icon`** (UI, from `Button`) vs **`world.art.icon`**
(on-map, from `ArtDefineTag`) — so the old per-entity `art_rename` hack (Terrain's `ArtDefineTag→artDefineTag`,
Feature collapsing both to `icon`) is GONE; the block split resolves the collision uniformly. **Canonical tag→dotted-path
map = `curate_common.ART_BLOCK`**; bespoke curators route through the shared `curate_common.put_art`/`emit_art`
helpers, cc-curated entities through `curate()` — one shape everywhere. Empty/`NONE` audio is dropped
(`curate_common.drop_empty_audio`). **STATUS:** applied to every migrated entity — the cc-curated set (tech/bonus/
route/terrain/feature/process/bonusclass/civicoption/pocos) AND all 11 art-bearing bespoke curators (trait/civic/
hurry/heritage/victory/religion/corporation/specialist/era/project/civilization), 2026-06-16. Entities not yet
curated (Unit — the art-heaviest: models+icons+sounds — Building, Improvement, …) adopt the shape NATIVELY via
`ART_BLOCK` when their pass runs; no retrofit owed. Related to the art-DATA-entities deferral below.

**DEFERRED — ART-DATA ENTITIES (a SEPARATE pass, NOT the gameplay cascade; verified 2026-06-14):** A gameplay
Info's on-screen ICON and its on-MAP / 3D art are DIFFERENT entities. Verified on Route: `CvRouteInfo` (the
gameplay route, migrated) has exactly ONE art field — `Button` → `ui.art.icon` (the UI icon) — and **no** model
reference at all; the on-map road models live in a SEPARATE DLL `CvInfo`, **`CvRouteModelInfo`** (`Art/
CIV4RouteModelInfos.xml`, ~840 rows: one `.nif` per route-type × connection-orientation, `ModelFile`/
`Connections`/`Rotations`, keyed UP to the route by `RouteType`). So `ui.art.icon` on a curated gameplay entity is
the UI icon ONLY — its placed/3D art is owned by a distinct art entity. This is NOT a migration drop (the model
art was never a `CvRouteInfo` field); it is an entire **unmigrated art tier** that was never on the gameplay
roster:
- **Model / landscape / effect infos** (on-map rendering geometry): `RouteModelInfo` (+ a `P2K_` module file),
  `RiverModelInfo`, `LandscapeInfo`, `EffectInfo`.
- **Art-define infos** (the `ArtDefineTag` indirection target — the `CvArtInfo*` family from
  `CIV4ArtDefines_*.xml`): Bonus, Building, Civilization, Feature, Improvement, Interface, Leaderhead, Misc,
  Movie, Terrain, Unit (+ base Asset/ScalableAsset/GenericBuilding/GenericCity).
These carry ZERO cascade data (no families/scopes/enables) — pure rendering. Guidance for the eventual art
pass: migrate each as its OWN faithful entity keyed by its gameplay type (do NOT fold into the gameplay entity's
`ui.art.icon`); if the pedia/website wants the link, derive a reverse edge (route → its models via the `RouteType`
FK). This dovetails with the already-noted "align everything to art-defines" cleanup (§2). LOW priority,
orthogonal to the cascade; do it after the gameplay entities, or never if the engine keeps loading the art XML.

**RESUME CHECKLIST:** (1) read THE MODEL (§0–§7) + HEAVY-ENTITY CAUTIONS + LIGHT-BATCH DECISIONS above;
(2) `git branch --show-current` = `json-data-migration`; (3) **light batch DONE** — next is the heavy entities;
pick one and run its `classify-<entity>` workflow FIRST (parallel understand + adversarial verify), then write
the curator from the dispositions; (4) before authoring any keyed field, grep the committed curators to avoid
double-authoring (inversion-vs-target-keying rule); (5) register new source entities in `store.ENTITIES` and
add prereq/obsolete edges to `store.PREREQ_FIELDS`/`OBSOLETE_FIELDS` as you drop upward refs (atomicity);
(6) verify branch + commit per entity (one commit each, regenerate with `--write`, JSON-parse-check the output);
(7) the C++ readers (`readJson`, the cascade engine #430, the GameSpeed ~150-site consumption conformance) come
AFTER all data is migrated.

---
### 0. Core premise — two strictly top-down cascades
The migration *is* the migration of two top-down cascades from XML into JSON; the JSON object shapes
merely serialize them, and **remodeling those shapes to fit the cascades is expected** — the handicap
prototype, `Tools/Migration/engine.py` output, and `mapping/*.json` are reshape-able drafts, not fixed
targets.

**⛔ ATOMIC DELIVERABLE — nothing ships until everything ships (owner ruling, 2026-06-14).** The XML→JSON
data migration AND the cascading enabler/modifier engine (#430) AND the C++ `readJson` readers are ONE
indivisible deliverable — a fundamental "rip everything up and do it right" cutover, not an incremental
roll-out. There is no shipping entity-by-entity: the per-entity commits on `json-data-migration` are
working-tree progress toward the single atomic cutover, never independently releasable units. Consequences:
(a) no entity's JSON is "done" in a shippable sense until the readers + cascade consume it and the whole
replaces the XML load wholesale; (b) do NOT PR/playtest slices of it as standalone features (the DLL still
loads XML until the cutover, so a half-migrated state runs nothing new); (c) sequencing stays config→…→
Building/Unit-last precisely so the model is fully proven before the one big landing. Plan the work as one
coherent rebuild, not a pipeline of shippable increments. The JSON migration and the cascade engine are
*tightly coupled* — optimizing for "ship something small" is irrelevant here; there is no small to ship. And
because the engine + C++ wiring land in the SAME cutover, **data-phase cascade shapes need only be reasonable +
faithful, NOT perfect (owner 2026-06-14):** any mismatch/wonkiness in the authored families/scopes/enablers gets
RECONCILED when the actual cascades are built and the C++ objects are wired (#430 / `readJson`) — capture the
data faithfully and move on; don't over-perfect a shape now. The codebase is far too tangled ("too much
spaghetti", owner) to land a clean ruling on EVERY shape at the data-migration stage; the cascade-build phase is
where the boundaries actually get pinned, so a reasonable-but-uncertain shape here is acceptable and expected.

**Corollary — fix at the ROOT, never patch the convoluted system (owner ruling, 2026-06-14).** Do NOT spend
effort repairing behaviour that runs on the old convoluted machinery — a band-aid label fix, a tweak inside
the legacy additive-combat channels, a one-off loader fix — when the root rebuild replaces that machinery
wholesale; the time is better spent at the root. So an issue that is really a *symptom* of the old structure
is resolved AS PART OF the root rework, not as an isolated standalone patch. Concrete: **#434 (swapped
vs-class/vs-type combat help labels) is fixed inside the #435 combat-class rework / the migration, not as a
standalone band-aid on the system #435 deletes.** (Sharpens the existing "success metric = loaders DELETED,
the fix is structural, target the craziest loaders first" rulings below — those say fix structurally; this
says don't even bother fixing the doomed structure's symptoms first.)

**The cascade IS the goal; the JSON is only the means — migrate SELECTIVELY (owner, 2026-06-14).** The
objective is the two top-down cascades in a data structure *purpose-built for them*, **NOT a 1:1 XML port.**
Carry forward ONLY what the cascade model + the gameplay/pedia consumers specifically need; **DROP the
legacy XML cruft** (dead/never-read fields, vestigial members, redundant or derivable data, wrong-side
references). Per-entity method = **curation, not transcription**: ask "what does the cascade need from this
entity?", pull that, then review every remaining legacy field for keep / drop / relocate / derive. Measure
success by what's **ELIMINATED** (loaders deleted, dead XML shed), not JSON files added — the `mapping/*.json`
inventories are a FILTER, not a port-list. (This is the scope guard against the abandoned "port everything"
mass-migration that poisoned the prior attempt.)

**Boundary — drop STRUCTURE, never live DATA (owner, 2026-06-14).** "Drop the crap" = dead/vestigial
*structure* (never-read fields, redundant tags, wrong-side refs), **NEVER live data/content.** All real
content is preserved — **including everything the in-tree modules contribute** (baked into core, §6): curate
by dropping dead fields from the fully-merged data, not by dropping entities/values. **Sequencing: purging
unwanted DATA/content (entities or values we decide to cut) is a SEPARATE, DELIBERATE pass AFTER the
migration completes** — during the migration, preserve ALL content; shedding dead *structure* is the only
dropping that happens during the move. Don't make "should this entity exist" calls while reshaping it.

**Post-migration purge backlog (capture, don't act yet):**
- **The "culture" intermediary bonuses** (`BONUS_ABBASID`/`BONUS_ATOMPUNK`/… one per Culture). Verified
  mechanism: a Culture national wonder (`SPECIALBUILDING_C2C_CULTURE`) grants its `BONUS_*` via
  `ExtraFreeBonuses`, purely so the buildings that require it (PrereqAndBonus `<Bonus>`) become buildable —
  i.e. `building → bonus → building`. The bonus tier is collapsible to a direct **building → building**
  enable, which also kills the "AI tries to trade a culture as a resource" oddity (owner-recalled, rare/
  stale). Defer to the post-migration purge; migrate the `building→bonus→building` chain faithfully meanwhile.
  **Now ISOLATED in `Assets/Data/bonuses/cultures/` — 410 of 907 bonuses (~45%!) are culture bonuses**, so
  removal is one contained folder. (The rest split `map/` = spawns on the map vs `manufactured/` = produced.)
- **Plot-substrate entities that blur the TERRAIN / FEATURE / BONUS boundary (owner observation 2026-06-16):**
  a few entities sit ambiguously across Terrain/Feature/Bonus, and there is a legit case to RECLASSIFY some into
  `Bonus` (or `Feature`) where that fits better. The named example is **`tar`** — owner: *"I think tar is terrain,
  it may also be feature… point is… yes"* (i.e. its exact bucket is itself unsettled, which is the point). Per the
  content-vs-structure rule (§0), **#428 migrates each FAITHFULLY in its CURRENT entity** (structure-only migration —
  do NOT make "should this be a terrain/feature/bonus" calls mid-move); the reclassification is a deliberate
  **POST-migration CONTENT pass**. Capture the specific candidates (tar, …) during the Terrain/Feature curation and
  revisit here. Mirrors the culture-intermediary-bonuses deferral above.
- **Cascading enablers** — a source declares what it `enables` downstream; availability flows DOWN.
  Nothing queries upward. **Canonical top-down enabler order (owner proposal, 2026-06-14)** — the
  topological spine of the enabler DAG; every enabler edge runs top→down in this order, which is also
  the load/resolution order (acyclic by construction):
  `tech → civic → religion → resource → bonus → plot → feature → improvement(build) → building → unit → unit-promotion`.
  - **⇄↓ ENABLER TOPOLOGY — "sideways, THEN down" (owner ruling 2026-06-14; THE key distinction from the modifier
    cascade):** the spine above is the DOWN axis (across tiers). But unlike the modifier cascade — which is
    DOWN-ONLY along the containment spine — the enabler ALSO has a SIDEWAYS axis: each tier has its own internal
    left-to-right PROGRESSION (the tech tree, the build chain, the promotion line, …). Resolution starts from the
    OLDEST/root (leftmost) and moves RIGHT through the tier's progression; **when it cannot go further right, it
    drops DOWN to the next tier**, resolves that tier's progression rightward, and repeats. So a tech enabling the
    next tech is a SIDEWAYS edge (within the tech tier); a tech enabling a building is a DOWN edge (to the next
    tier) — the vocabulary MUST distinguish the two axes. (Modifiers have no sideways axis: containment is
    strictly nested.) **Sequencing (owner):** formalize this enabler vocabulary BEFORE the heavy entities, then
    RETROFIT the 21 already-converted infos to it. A `formalize-enabler-vocabulary` workflow is gathering the full
    edge inventory to slot under this topology.
  It is a *ranking*, not a claim that every adjacent pair is a direct edge (e.g. religion's real
  enabler is its tech, two ranks up). `improvement` ≡ its `build`. TO PRESSURE-TEST: the
  `resource`/`bonus` split (one tier or two?) and the `resource→bonus→plot→feature→improvement`
  sub-order against actual game edges (terrain/plot arguably gates which bonuses/features appear).
- **Cascading modifiers — ONE cascade, EVERY level is a target (owner, 2026-06-14, expanded).** A source
  declares effect deposits that flow DOWN the full containment hierarchy
  `world → team → empire → area → city → plot → {improvement · feature · terrain · route}`, plus the
  city-level producers **building** and **specialist**. **Every node is a legitimate modifier target — in
  essence, ANYTHING WITH A PER-TURN EFFECT that can be modified** (yield, commerce, happiness/health, property
  accumulation, upkeep/maintenance, great-people rate, experience, … — not just yields): the aggregating
  scopes world…city AND the leaf producers plot/improvement/feature/terrain/route/building/specialist. A
  deposit at any level applies
  to everything contained beneath it; **addressing is UNIFORM at every level**
  (`<family>.<scope>.<targetType>.{TARGET}.<unit>`) — an empire-scope deposit onto a building is shaped like a
  city-scope one. **The SAME cascade resolves enablers too** (availability flows down the same hierarchy) — one
  cascader catches all, not just modifiers. Properties flow this same spine to the plot leaf (via
  `SAME_PLOT`/containment); the ONLY dropped relation is the `NEAR` spatial-radius leak (§5). (`world` replaces
  the prototype's `game`; `empire` = the player level, retained over `player`.)
  - **Target vs CONDITIONER — the stay-vs-invert rule (owner, 2026-06-14):** if the effect LANDS ON the keyed
    thing (it is the per-turn-effect producer being modified — improvement/feature/terrain/building/specialist/city/…),
    the modifier STAYS on the source keyed by that target. If the effect lands ELSEWHERE, gated merely by
    HAVING the keyed thing (a CONDITIONER you possess — a bonus/resource, a tech, a religion), it INVERTS onto
    the conditioner. Example: Civic `BonusCommerceModifiers` → the bonus (commerce lands in the CITY, gated by
    owning the resource → invert), whereas Civic `ImprovementYieldChanges` STAYS (the yield lands ON the
    improvement's tile → it is a target, not a conditioner). A civic's improvement/feature/terrain/building/
    specialist modifiers are ALL legitimate target deposits — they stay on the civic, keyed by the target.
  - **Sources & enablers are NEVER targets (owner, 2026-06-14).** Resources/bonuses, civics, traits, techs,
    religions PRODUCE modifiers and ENABLE the actual targets — they have no per-turn effect of their own to be
    modified, so nothing ever targets them. (A modifier keyed by one of them is the conditioner case above: it
    folds onto that source/enabler, which owns its own contribution.) Targets = the per-turn-effect producers;
    sources/enablers = what deposits onto and unlocks them. This is the same split when Trait migrates (a trait
    is a source/enabler, like a civic — never a target).
    - **The clearest test (owner articulation, 2026-06-14):** a RESOURCE in complete isolation does nothing —
      it only says **"yes"** (enables a target — *coal lets you build the mine / the coal-fired plant*) or
      **"now you can do better"** (boosts a target — *coal makes the mine/city produce more*). The
      IMPROVEMENT it enables (the **mine on the coal**) is the target; the coal is never the target, it points
      DOWN at one. If a thing produces/aggregates a per-turn effect → target; if it merely unlocks or amplifies
      other things → source/enabler.
    - **⚠ KNOWN UNRESOLVED "CREST" — codify when building/testing #430 (owner, 2026-06-14):** the stay-vs-invert
      rule is NOT yet properly codified for the case where **source A's effect is conditioned/scaled by
      POSSESSING source/enabler B** (e.g. Civic `BonusCommerceModifiers` = "+commerce per resource owned";
      building/unit/project `BonusProductionModifiers` = "build faster with bonus Y"). Two valid slopes meet at
      a ridge: (a) **fold onto B** keyed by A — the COMMITTED convention (curate_bonus folds building/unit/
      project `BonusProductionModifiers` onto the bonus as `buildRate`); vs (b) **keep on A** with B as a
      `byBonus` CONDITION (cleaner ownership — A owns its modifier, B stays a pure condition, never holding
      another source's effect). Open sub-question: is **tech different** (a top-of-cascade UNLOCKER — "research X
      → these get better" — genuinely a source, so `byTech` folds onto the tech and HOLD) vs a **bonus** (a
      possessed CONDITION, so it reverses to (b))? **Not blocking the data migration** (the committed convention
      stands; Civic `BonusCommerceModifiers` is empty in the data anyway). RESOLVE this when the cascading
      enabler+modifier engine (#430) is built and tested end-to-end — that is what will actually pin the
      boundary; until then, leave the committed folds as-is and revisit Bonus/Project if (b)/tech-split wins.
      **Expect MORE of these crest cases at LEADER-TRAIT migration** (traits are sources/enablers like civics,
      with many bonus/tech/state-conditioned effects) — gather the full set there before codifying the rule, so
      the boundary is drawn against the complete pattern rather than this one instance.
- **NOT a cascade — spatial/adjacency "leakage" (owner idea, 2026-06-14; FUTURE, separate system — tracked in #429):**
  a THIRD graph — **"sideways static influence"**: LATERAL peer↔adjacent-peer effects set by fixed map
  geometry (radius / adjacency), **orthogonal** to the two cascades' VERTICAL top-down flow, and ideally
  **one-hop** (a thing influences its neighbour by where it sits; it does NOT re-propagate onward like the
  old `NEAR` diffusion). Examples: a pollution radius around a city/improvement, or clustered-farm
  adjacency bonuses (4 farms boosting each other). These get their OWN isolated, deliberately-designed system and are NOT
  expressed through the two top-down cascades. A containment spine cannot express neighbor-geometry, so
  this is *why* `NEAR` kept needing special cases. The migration keeps the cascades containment-only and
  leaves this as a clean future seam — it absorbs the dropped `NEAR` relation (§5).

### 1. Governing invariant — the HOT PATH is top-down only
| | Hot path (per-turn compute) | Cold path (open a screen / pedia / website) |
|---|---|---|
| What | effective modifier values + "is it enabled" | build-list rendering, pedia cross-links, greying |
| Direction | top-down ONLY: sources deposit DOWN, targets read O(1) summed accumulators; no target asks "who affects me" | bidirectional, reads the derived reverse index |
| Reverse edges | never touched | the whole point |
| Built | deposit-on-event (rare) | the reverse index, built once on load |

Reverse edges cannot leak into the hot path because the hot path never reads them.

**Repositories de-emphasized (owner, 2026-06-14).** The cascade engine's reads are already cheap —
O(1)-ish summed scope accumulators (deposit-on-event; turn-start recompute only for the few drifting
contributors), derived and recompute-on-load. The new structure therefore **significantly reduces** the
need for the derived-data **repository** (`TLazy` / `CvDerivedData` caching). It is NOT a foundational part
of this work — treat it as an **optional, measured optimization added AFTER the datastructure + cascading
engine (#430) are in**, and only where a specific recompute proves hot. Do not design around repositories
or put weight on them. **If the cascade obviates the repository's caching role entirely, removing it is a
valid end outcome (owner, 2026-06-14)** — the cache is not preserved for its own sake (it is a perf cache,
not gated instrumentation, so the keep-instrumentation rule does not apply). The repository's HTTP
*observability* endpoint is separate instrumentation and is unaffected by this.

### 2. Authored object shape — top-down OUT-edges + intrinsic; nothing upward or reverse
| Section | Cascade | Contents |
|---|---|---|
| `enables` | enabler | what this entity makes available (`{buildings:[],units:[],builds:[]}`) |
| `modifiers` | modifier | scope-wide: `modifiers.{scope}.{channel}.{unit}`. **Entity-targeted** ones (the old `buildingBoosts`) fold in under the scope, keyed by target type — `modifiers.{scope}.{targetType}.{TARGET}.{channel}.{unit}` — so there is ONE `modifiers` section, no separate `boosts` (convention: one modifier vocabulary). Keys are clean (no `i`/`b` Hungarian prefixes); flags are JSON booleans (`tradeable: true`). |
| `grants` | top-down, NOT cascading | one-shot event grants (FirstFreeUnit, free techs) — fire once, don't sum/propagate |
| `ai` | — | ALL AI metadata in one group, subgrouped (unified cross-entity model): `ai.flavours` (FLAVOR list) + `ai.behaviour` (`tradeModifier`/`weight`, …) + future subgroups (attitude, strategy) |
| text / `cost` / `ui` / `world` / `sound` / `identity` | — | intrinsic ("what am I", empire-agnostic). Art split into the three dedicated blocks `ui`/`world`/`sound` (`art` a sub-block within `ui`/`world`; §0.8, canonical map `curate_common.ART_BLOCK`). `identity` is the shrinking catch-all (era, grid, the `worth`/`militaryWorth` demographics — a future `score` group candidate) |

`enabledBy`/`obsoletedBy`/`boostedBy` and prereq-style upward refs **never appear in the authored
object** — they are derived into the on-load reverse index (§4).

**Naming conventions (clean, no Hungarian — owner, 2026-06-14):** keys drop the `i`/`b`/`f` prefixes;
flags are JSON booleans (`tradeable: true`, not `bTrade: 1`); and demographic contributions use the **`worth`
convention** — bare `worth` = the entity's OVERALL worth (the "Assets" demographic, `iAsset`); a
domain-specific worth is `{field}Worth`, so `iPower` (the "Power"/military demographic) is `militaryWorth`.
Never `asset` (reads as an art asset) or bare `power` (collides with the electricity mechanic). Shared
rename map: `engine.FIELD_RENAME`. The display-art reference is unified to **`icon`** (tech's direct
`Button` path AND bonus/building's `ArtDefineTag` both → `icon`; shared `ART_RENAME`).
*(How scoring actually consumes `worth` is not yet understood — material for a future "how scoring works"
doc; low priority.)*
*(Look-at-later: techs reference art by a DIRECT path while bonuses/buildings use an `ArtDefineTag`
indirection — owner wants to align EVERYTHING to art-defines; a future cleanup, low priority.)*

### 3. Standardized modifier structure
> **RESHAPED 2026-06-14 — FLAT FAMILY layout (owner ruling).** There is **no `modifiers` wrapper**.
> The old `"modifiers": {scope:{channel:{unit}}}` (shown below) is replaced by **one top-level section per
> modifier FAMILY** — the *kind* of thing modified — each in the uniform shape
> **`<family>.<scope>.<member>.<unit>`** (`<member>` omitted for single-concept families):
> ```jsonc
> "maintenance": { "empire": { "colony": { "percent": 60, "cap": 120 }, "distance": { "percent": 60 } } },
> "upkeep":      { "empire": { "unit": { "percent": 60, "ai": { "percent": 120 } } } },
> "diplomacy":   { "empire": { "attitude": { "flat": 1 } }, "team": { "noTechTrade": { "percent": 100 } } },
> "happiness":   { "empire": { "flat": 7 } },                  // single-concept family: no member level
> "property":    { "city": { "PROPERTY_CRIME": { "perTurn": <formula> } } }
> ```
> **Why:** the families are genuinely distinct KINDS (you never add a 5th commerce into `yield`); they
> interact but never merge. A flat per-family surface keeps each focused and stops one concern — a gold
> COST (maintenance/upkeep), a behavioural knob (diplomacy) — from bleeding into the output modifiers; a
> modifier does not magically poison an unrelated family. The `ai:` audience override is unchanged. The
> accumulation rule, named yield/commerce keys, ×100, and parity-not-a-goal (all below) STILL HOLD.
>
> **SPLIT vs GROUPED (owner ruling 2026-06-14).** The base `yield` and `commerce` buckets, and `property`,
> are **SPLIT into per-identifier families** — `yield`→`food`/`production`/`commerce`, `commerce`→`gold`/
> `research`/`culture`/`espionage`, `property`→ one family per `PROPERTY_*` — with **no wrapper and no member
> level** (the identifier lives only in the family name, never duplicated inside the leaf). This kills the
> ambiguity where `commerce`/`production` were both a `yield` member AND a family, and flattens parsing. The
> renames it forces: bonus production-rate `production`→**`buildRate`**, AI tech-cost `research`→**`techCost`**
> (so `production`/`research` mean the yield/commerce). **Coherent-concept families stay GROUPED with members**
> — `maintenance` (distance/numCities/colony/corporation), `upkeep` (unit/civic/inflation/…), `diplomacy`,
> `combat`, `barbarians`, `vicinityYield`, **`defense`** (city-defense; members `amount` = additive extra-defense
> percent + `min` = the floor clamp some buildings raise above 0, e.g. ≥25% — owner 2026-06-14, authored at the
> Building pass via `iMinDefense`). Single-concept families (`happiness`/`health`/`growth`/…) are
> top-level with no member. (Some grouped-family names PROVISIONAL — "nail down later", owner.)
> `enables`/`obsoletes`/`grants`/`identity` are NOT modifier families and remain their own sections; a
> grouped family "adheres to the same structure" — modifier is the STRUCTURE, the family layout is the data.

The original framing (now reshaped per the note above — `channel` → `family`+`member`, no wrapper):
```jsonc
"modifiers": { "<scope>": { "<channel>": { "<unit>": <value> } } }
```
- **scope/target hierarchy** ∈ `world | team | empire | area | city | plot | building | specialist`, where
  the `plot` carries the producers `improvement | feature | terrain | route`. **EVERY node is a legitimate
  modifier target** (anything with a modifiable PER-TURN EFFECT — yield/commerce/happiness/health/property/
  upkeep/GP-rate/…, not just yields); a deposit at any level cascades DOWN to
  all contained targets, addressed uniformly `<family>.<scope>.<targetType>.{TARGET}.<unit>` (§0). Modifiers
  AND properties (AND enablers — one cascade) reach the targets via **containment**; the only dropped relation
  is `NEAR` passive diffusion (§5). Keyed deposits STAY on the source when the keyed thing is the target the
  effect lands on; they INVERT only when it is a CONDITIONER you merely possess (bonus/tech/religion — §0).
- **channel** = the affected quantity: `yield | commerce | unitProduction | happiness | health |
  maintenance | …`, **and each property is its own channel** (`PROPERTY_CRIME`, …) — properties are
  base-accumulator channels "like any other resource".
- **unit** ∈ `flat | percent | perPopulation | percentPerPopulation | perMilitaryUnit | perTurn |
  decay | enabler`. The unit comes from the VALUE element, never the wrapper tag name.
- **`<value>`** is one of: a number (scalar channel) · a formula tree `{op:[operands]}` · a per-item
  keyed map `{ITEM: number|formula}` for indexed channels.
- **Per-item multiplier** = indexed channel + `percent` unit + keyed map, e.g.
  `unitProduction.percent.UNIT_AXEMAN: 25` ("+25% Axeman production"), same machinery as
  `yield.percent.production: 50` or `PROPERTY_CRIME.percent: 50`.
- **Yield/commerce keys are NAMED IDENTIFIERS, never a positional/enum array (owner, 2026-06-13,
  reaffirmed 2026-06-14).** The 3 yields (`food`, `production`, `commerce`) and 4 commerces (`gold`,
  `research`, `culture`, `espionage`) are **hard requirements all the way from the `.exe` — immutable,
  and we have no intention of changing them** — which makes their name↔index table the ONE mapping safe
  to hardcode permanently. The JSON / data / modder / website surface uses the **named identifiers**
  (short, lowercase, no `YIELD_`/`COMMERCE_` prefix — to set them apart from data-driven Type keys like
  `BONUS_COAL`/`TECH_POTTERY`). The **positional enum index is walled off in the C++ reader only**:
  `readJson` maps name→index when handing the EXE its ABI-bound arrays; nothing else — not the data, the
  modder, or the future website — ever sees an index. So an indexed yield/commerce value is always
  `{"food": 2, "production": 1}` / `{"gold": 25}`, never `[2, 1, 0]`.
- **JSON stores human-readable values; the EXE's fixed-point ×100 scaling is reader-only (owner,
  2026-06-14).** Several engine quantities are stored as `int × 100` fixed-point (2-decimal precision in
  an int-only EXE — e.g. `getTechYieldChanges100`, where XML `700` means `+7.00`). **Carrying that ×100
  int into the JSON just because it was the legacy int-math way of representing fractions is the WRONG
  approach (owner, 2026-06-14)** — the data expresses the real quantity; fixed-point is an EXE
  implementation artifact, not a data-model truth. The JSON / data / modder / website surface uses the
  **natural readable value** (`7`, `7.5`, `0.6`), NOT the ×100 int — in the END STATE. **But that is DEFERRED to a post-migration refactor (#432), NOT
  done during #428 (owner, 2026-06-14):** de-scaling now is risky — it needs a complete inventory of every
  scaled field (the **pedia tooltip / `CvGameTextMgr` rendering**, which ÷100s and formats decimals, is a
  good source for which fields are int'ed decimals), a matching `readJson` conversion that does not exist
  yet, and untangling the engine's pervasive ×100 int-math. Per the sequencing rule, **#428 carries the
  ×100 values faithfully** (the JSON temporarily holds `700`); #432 flips them to readable values later.
  **So the migration converter does NO de-scaling — it copies values as-is**; the `*100` getters (and the
  pedia rendering) are #432's inventory. **The ×100 int math is also OOS-load-bearing** — Civ4 MP is
  deterministic lockstep and float math is CPU-dependent — so #432 changes ONLY the data representation:
  `readJson` converts readable→`int×100` once at load (deterministic; same JSON + parse on every client),
  and the engine's **runtime math stays integer**. No runtime float is introduced → no OOS risk.

**Hot-path accumulation rule (additive commutativity, our DESIGN choice — not derived from legacy):**
per `(channel, item)` → `effective = (base + Σflat) × (100 + Σpercent) / 100`. Flats sum; percents
SUM then multiply ONCE; order irrelevant (deposit in any order, even parallel). *Verified the live
consumer already works this way* (2026-06-14): CvCity sums all percent modifiers before a single
multiply for yields/commerce/production/maintenance — **no per-source compounding, no last-writer-wins
anywhere in core channels** (`CvCity.cpp:11326-11364`, `12104-12140`, `3857-3948`, `7690-7727`;
`getModifiedIntValue` `CvGameCoreDLL.cpp:689-700`).

**Parity with the legacy result is explicitly NOT a goal, and is *very unlikely* (owner, 2026-06-14)** —
testing the cascade model against the handicaps and one building already surfaced latent BUGS in the OLD
calculations, so divergence from the old result is expected and is frequently a *correction*, not a
regression. Verification is effect-sensibility (no value silently dropped to default), never byte/value-parity.

**Two conscious simplifications the uniform rule makes (flagged, owner to confirm — NOT silent):**
1. **Building flat yields become multipliable.** Today plot+specialist yields are inside the
   multiply but building flat / per-pop / corp yields are added RAW post-multiplier
   (`CvCity.cpp:11363`); the uniform rule folds all flats into base, so a yield% modifier now
   amplifies a building's flat `+2`. *Recommend ACCEPT* (it is the point of the uniform model);
   a `postMultiplier` flat kind is the escape hatch if a specific balance needs the raw add.
2. **Cost channels need asymmetric negative percents.** Maintenance/build-cost apply
   `×(100+m)/100` for m≥0 but `×100/(100−m)` for m<0. Tag channels `rate-style` (clamp ≥0) vs
   `cost-style` (asymmetric). Consumer-side detail; data shape unchanged.

**Non-additive effects classify OUT of the cascade:** REPLACE/override (`setWorkableRadiusOverride`),
min/max clamps, pure capabilities → `enabler` kind or non-cascade treatment; they are not additive
channels. Flat-only channels: happiness/health carry no `percent` unit (consumer splits good/bad and
sums flats — `CvCity.cpp:5933-5985`, `8543-8581`).

### 4. The reverse index — derived, cold path, display-only
Built ONCE on load by inverting the typed top-down edges (owner-confirmed cheap). **Never used to
compute enablement** — the top-down forward index does that. It exists to power the in-game pedia +
city UI and the future web-Civilopedia, and it must reproduce every feature the game has today.

**Evidence — what today's Civilopedia navigates upward (2026-06-14 inventory, `CvGameTextMgr.cpp`):**
the pedia performs expensive cross-entity SCANS the reverse index replaces — the building page
iterates ALL civics (health/happiness from civic, `17369-17401`), ALL traits (production/happiness
modifiers, `17825-17861`), and ALL units ("required to train", `isPrereqAndBuilding`). Plus direct
upward reads: prereq tech(s) `18810-18831`, obsolete tech `18103-18121`, obsoletes-to building
`18108`, tech-conditioned yield/commerce/health/happiness/specialist boosts `16966-17363`, free
special tech grant `16096`, via-SpecialBuilding tech/obsolete `18437-18450`.

**Reverse-edge set the index must provide:** `enabledByTech(s)`, `obsoletedByTech`,
`upgradesFrom`/`upgradesTo` (+ on-obsolete), `boostedBy{tech,civic,trait,bonus,religion}`,
`grantedBy`, `requiredToTrain{units}`, the forward `enables` (for "Unlocks" lines), and via-
`specialBuilding` tech/obsolete.

**City build list is tri-state** (the feature pinning the requirement detail): listed = not-built ∩
enabled-by-researched-tech ∩ not-obsoleted; **greyed** = listed but missing a required bonus/resource
(or other unmet gate); hidden otherwise. The reverse record carries each entity's requirement atoms so
the screen evaluates met/unmet per atom for greying — off the hot path, never the buildability gate.

### 5. Open questions + decisions locked this session (2026-06-14)
- **Property scope — RESOLVED (owner, 2026-06-14; verified inert):** properties are scope-spine
  accumulator channels reaching the leaf targets via **containment only** — `SAME_PLOT` to the plot leaf,
  `ASSOCIATED` to the city — exactly like a yield. **The `NEAR` "passive-leak" diffusion is DROPPED**
  (also `TRADE`, the other network relation; `WORKING_*` folds into city↔plot containment). This is a
  **no-loss cleanup**: a 2026-06-14 inventory confirmed plot-level properties are *mechanically inert
  today* — nothing reads a plot's property value for yields/output/events (`CvPlot::calculateYield` never
  checks properties), the `NEAR` diffusion spreads values nothing consumes, and the pollutions/flammability
  are dormant (`CvHttpServer.cpp`, 2026-06-11); the aspirational global-warming threshold was never wired.
  The dropped spatial behaviour moves to the separate **"sideways static influence" / leakage** system (§0, tracked in #429).
  **One live path to preserve in the property pass:** crime/disease are real CITY-level gameplay emitted by
  *units* onto their plot — re-home that emission to the containing city (containment) so criminal/diseased
  units still raise city crime/disease once the plot→city diffusion is gone; verify before deleting.
- **§3 conscious simplifications** — owner to confirm building-flat-multiply (recommend accept) and
  cost-style channel tagging.
- **Section layout:** keep `identity` as a named catch-all, or force every field into a real section.
  *Partly resolved (owner, 2026-06-14):* `iAsset` read like an art **asset** → renamed `worth`; AI hints
  are now pulled into a unified **`ai` group** (`ai.flavours` + `ai.behaviour{tradeModifier, weight}`,
  extensible — attitude/strategy subgroups later). Remaining: the demographic ratings (`worth`,
  `militaryWorth`) want a `score`/`demographics` group (still pending). **DONE for map-generation (owner,
  2026-06-14):** the bonus placement fields are now grouped under **`mapGeneration`** (`validTerrains`/
  `validFeatures`/`validPlacementOn`, `tilesPer`, `constAppearance`, placement `rands`, lat/area, the
  flatlands/hills/peaks/normalize flags), leaving bonus `identity` as just `bonusClassType`. It's a shared,
  opt-in `EntityConfig.map_gen` group, reusable by other map-placed entities. Some `mapGeneration` names are
  de-Hungarian-only (`placementOrder`/`constAppearance`/`rands.iRandApp*`…) — clearer names TBD. (The "great
  farmer" mechanic is a *separate* special-case — owner: "a bit of an abomination" — not these; future cleanup.)
- **OR/NOT prereq composition minority:** the AND-of-presence majority is top-down `enables`; OR/NOT
  keeps a small target-side BoolExpr (a query over the top-down atoms, not a relocatable reference).
- **Same-tier lifecycle/succession chains — IMPORTANT, in-scope (owner, 2026-06-14):** a distinct
  relationship family — *sideways* (peer↔peer, same tier) but STRUCTURAL, not spatial (do NOT confuse
  with the spatial "leakage" sideways, #429). Concrete: **unit `upgradesTo`** (the upgrade chain),
  **building `replacedBy`** (superseded by a successor), plus `extends`/`obsoletesToBuilding`. NOT
  cascades (they don't sum or enable) and NOT dropped — the game needs them (upgrade UI/AI, obsolescence)
  and the pedia needs both directions. Model: author ONE canonical end (Civ4 convention: unit declares
  `upgradesTo`, building declares `replacedBy`), derive the reverse (`upgradesFrom`/`replaces`) into the
  cold-path reverse index.
- **Promotion ranks — RESOLVED (verified 2026-06-14): DELTA / additive, no special case.** A unit *keeps*
  all lower ranks (Combat I→II→III are prereq-chained, not replaced) and each promotion's modifier sums via
  `changeExtraCombatPercent(getCombatPercent()*iChange)` (`CvUnit.cpp:18873`). So ranks are ordinary
  additive modifier sources — the cascade absorbs them with zero special handling.
- **Succession-chain authoring direction — verified (2026-06-14):** units list what they upgrade TO
  (`UnitUpgrades`/`getUnitUpgrade`, consumed by `AI_upgrade` `CvUnitAI.cpp:806`); buildings declare their
  successor (`ObsoletesToBuilding` `CvBuildingInfo.cpp:1697`, `ProductionContinueBuilding`). Author those
  ends, derive the reverse.
- **The 3 `PrereqOrBuildings` techs** (`TECH_OBSIDIAN_WEAPONS`, `TECH_WATERPROOF_CONCRETE`,
  `TECH_LEAD_GLASS`): **DECIDED (owner 2026-06-14) — remove.** Drops the resource-research gate and
  deletes the whole `Prereq(Or)Buildings`-on-tech mechanism (field + loader); the one upward arrow is gone.
- **Scope naming — RESOLVED (owner 2026-06-14):** `world → team → empire → area → city`. `world` replaces
  `game`; `empire` retained (the brief `player` was a slip). Update the handicap prototype
  (`cascadeModifiers`→`modifiers`; scope keys already `empire`).
- **Delayed resolution (#362) — RESOLVED by the model:** the per-ref `isDelayedResolutionRequired` hack
  + the hand-maintained category load order are not needed. The on-tech-reached modifier family inverts
  onto the conditioner and resolves at runtime/deposit (every Info already loaded), and enabler prereqs
  are acyclic by construction → resolve in topological load order / a single link pass. Physical removal
  of the mechanism is #428 reader-phase work.
- **#195 / #196 closed as superseded (owner 2026-06-14):** #195's prereq unification is achieved by
  inverting simple edges to top-down `enables` + reverse-index introspection (the 24 `Prereq*` fields
  collapse) — **`BoolExpr` is KEPT** for the OR/NOT composition minority. #196's per-class
  declarative-XML loading is moot once data moves to JSON; its `CvInfoUtil` wrapper substrate is kept
  (it underpins `readJson`). Open #196 sub-issues (#221/#236/#268/#301/#309) closed obsolete.
- **Long enabler lists — building-group helper (DEFERRED, post-core-migration):** one tech can `enable`
  ~100 buildings (e.g. Language), making the flat `enables.buildings` list long. This is *authoring
  ergonomics*, NOT correctness — a flat id list works and the enabler index is indifferent to length. A
  future **"building group"** is a purely organizational **data helper** (a named set that expands to
  building ids at load), **NOT a full Info and NOT a node in either cascade or the reverse index**.
  Guardrail: load-time expansion only; most top-down shape is *buildings declaring their own membership*
  (so "enable a group" collects members) rather than a central group→buildings list. Revisit as sugar
  once the core migration is done.

### 6. File layout & loading (owner, 2026-06-14)
- **One JSON per Info** (individual files), **loaded recursively**.
- **Era sub-folders ONLY WHEN APPLICABLE** — i.e. for entity types that are BOTH *numerous* AND
  *unlockable/era-tied* (buildings, units): `Assets/Data/<type>/<era>/<entity>.json`, era derived from the
  enabling tech. Small or non-unlockable sets (handicaps, eras, gamespeeds, …) stay a flat plural folder —
  **no era split**. The loader recurses the era dirs.
- **Modules baked into core — DATA PRESERVED, system retired (owner, 2026-06-14; preservation invariant).**
  Retiring modules kills the module *system* (load-order/overlay layering), **NOT the module *data*.** Every
  live value the in-tree modules contribute (new entities, field overrides) is **merged into canonical core
  data — zero data loss.** Mechanism (the safeguard): the migration curates the **fully-merged** entity — the
  result of the existing module-overlay `copyNonDefaults` merge — so all module contributions are captured
  BEFORE anything is dropped. The offline converter must **reproduce that merge** (or source from the merged
  state). Modules are not a modularity boundary we ship; third-party modmods are the external replacement
  (below, #431).
- **Third-party modmods = a separate EXTERNAL overlay tier** (player-installed, loaded after core via the
  same JSON overlay machinery) — what *other users* add to the mod, NOT something we distribute.
  Formalization + authoring guidance tracked in **#431**.
- **Ship loose** — the engine enumerates with raw `FindFirstFile`, which only sees loose files; data ships
  loose like the XML does (if packing is ever needed, switch enumeration to a manifest/VFS).

### 7. Implementation status (2026-06-14)
Tooling under `Tools/Migration/`:
- **`store.py`** — the XML-as-DB store: loads every gameplay Info from base XML + modules, merges module
  overlays by Type (preserving content — 1567 of 5211 buildings are module-added), and builds the generic
  **enabler + obsolete reverse-indexes** (17 entities: tech/bonus/building/civic/corp/project/process/
  promotion/heritage/specialbuilding/… → what they enable; `ObsoleteTech` → what a tech obsoletes).
- **`curate_tech.py`** — the FIRST curated entity (Tech) = the template. Emits top-down JSON per tech
  (`enables` / `obsoletes` / `modifiers` / `boosts` / `grants` / `cost` / `flavors` / `art` / `identity`;
  NO prereqs, NO reverse edges) to `Assets/Data/techs/<era>/`. **All 943 written.** Boost units verified
  against the C++ consumer (workflow wf_7bbae202 — caught a `PrereqTech`-key silent-drop and a
  `TechCommerceChanges` percent/flat collision); ×100 carried **faithfully** (de-scale = #432).
- **Known-remaining (niche, non-blocking):** unit `TerrainPassableTechs`/`FeaturePassableTechs` (a tech→unit
  *capability* boost) not yet wired; `Trait.TechResearchModifiers` is trait-side (belongs to trait curation,
  surfaces on a tech only as a derived `boostedBy`); module-merge is v1 replace-by-tag (list-append + exact
  C2C load-order are deferred refinements).
- **Generalized (2026-06-14):** the curator is now `curate_common.py` (shared core, driven by
  `mapping/<Entity>.json` + a small `EntityConfig`); `curate_tech.py` / `curate_bonus.py` are thin configs,
  so conventions can't drift. **Done: Tech (943) + Bonus (907)**, each verified against the C++ consumer
  (workflows wf_7bbae202, wf_e0e79ae3) — which caught real silent bugs: Tech's `PrereqTech`-key drop +
  `TechCommerceChanges` flat/percent collision; Bonus's unit `<BonusType>`-spelling bug (`enables.units`
  was empty; enable coverage 344→778 after fix) + `BonusCommercePercentChanges` flat/×100 mislabel.
- **#432 trap (found 2026-06-14):** `BonusCommercePercentChanges` is ×100 *without* a `*100` getter (it's
  added in ×100 space inside `getBuildingCommerce100`), so the "`*100` getters mark the scaled fields"
  heuristic is INCOMPLETE — #432's inventory must also scan ×100-space addends.
- **Handicap (3rd, done 2026-06-14):** `curate_handicap.py` — a CONFIG entity (enables nothing), in the
  **FLAT FAMILY** layout (§3): every modifier family is a top-level section in `<family>.<scope>.<member>.<unit>`,
  no `modifiers` wrapper. Families emitted: `maintenance` (distance/numCities/colony{+`cap`}/corporation),
  `upkeep` (unit/civic/inflation/supply/upgrade), `diplomacy` (attitude/declareWar/warWeariness `empire`
  + noTechTrade/techTradeKnown `team`), `combat` (animal/barbarian `world` + freeWinsVsBarbs `empire`),
  `barbarians` (`world` spawn rules), each `PROPERTY_*` a top-level family (`city`; SPLIT, no `property`
  wrapper), `happiness`/`health` (`empire` flat), and the AI-economy singletons `growth`/`techCost`/`workRate`/
  `buildCost`/`perEra`/`revolution` (names PROVISIONAL). Human/AI duality = `ai:` audience; one-shot setup →
  `grants` (+ `grants.ai`); advanced-start → `identity.advancedStart` (not a modifier, needs review); `Goodies`
  → `identity`. All 12 written, every tag classified. Scope = APPLICATION level (empire/team/world/city),
  DISTINCT from the SOURCING handicap (own vs game vs team — a consumption rule in
  `docs/dev/reference/handicaps.md`: base ← own, `ai` ← game, EXCEPT `advancedStart.ai` ← own).
  `maintenance` mirrors `CvCity::calculateBaseMaintenanceTimes100` (components each modified, then a
  whole-maintenance modifier = the future `all` member); GOLD-only, post-income; `maxColony` caps the colony
  component. Verified vs 3 ground truths: the hand-built prototype (value-for-value), `CvHandicapInfo.h`,
  `handicaps.md`. `revolution` is INCOMPLETE (a WIP mechanic with a tracking issue) — kept, **NOT dead**
  (`handicaps.md`'s "dead" note is being corrected).
- **DONE (2026-06-14): flat-family layout propagated to `curate_common`; Tech (943) + Bonus (907) re-curated.**
  Base yield/commerce SPLIT into per-identifier families (food/production/commerce/gold/research/culture/
  espionage); bonus production-rate → `buildRate`; `vicinityYield` stays grouped. All three entities now
  share the flat-family surface; `curate_common` carries `SPLIT_FAMILIES` + the family-first boost fold.
- **GameSpeed (4th, done 2026-06-14):** `curate_gamespeed.py` (bespoke — one scalar fans out to several family
  members), a CONFIG/GLOBAL entity depositing at `world` scope (it sits atop the cascade and multiplies
  everything beneath). `iSpeedPercent` → **`costs`** (`world.{train,construct,create,research}.percent`) +
  **`growth`** (`world.percent`, food-to-grow) + **`durations`** (`world.{anger,decay,happiness}.percent`,
  members PROVISIONAL); `iUnitYieldScalePercent` → `unitYieldScale.world.percent` (a YIELD scale, NOT a cost).
  All 9 written. **`costs` is a NEW universal family — the MULTIPLIER on the base costs that live on the infos
  themselves** (unit/building/project production cost, tech research cost). **Cost members are FINE-GRAINED
  (owner ruling 2026-06-14): train/construct/create/research(/build/improvementUpgrade from Era)** — each a
  distinct produced-thing/cost-type; GameSpeed sets all = iSpeedPercent, Era sets them per-era. Food-to-grow is
  the `growth` family, NOT `costs.food`. Verified vs
  `CvGameSpeedInfo.{h,cpp}`: exactly 2 stored scalars; `getHammerCostPercent` (= `iSpeedPercent` + optional
  `UPSCALED_HAMMER_COST_MODIFIER` game-option) and all turn/calendar accessors are DERIVED.
  **CONSUMPTION does NOT conform — readers-phase rewrite (owner-flagged):** ~150 ad-hoc
  `quantity * getSpeedPercent()/100` sites (CvCity/CvPlayer/CvGame/CvPlayerAI/…) hand-apply the multiplier on
  heterogeneous quantities; conforming = read `costs`/`durations` from the cascade via a uniform accessor.
  That rewrite also completes the `durations` member set. Deferred (C++ readers come last); the data is the target.
  **Rider (2026-06-14 PM):** GameSpeed gained a `cultureThreshold.world.percent` = iSpeedPercent member (its own
  family) — see the CultureLevel note below (the per-speed culture scale "belongs in GameSpeed", owner).
- **CultureLevel (lighter-three #1, done 2026-06-14 PM):** `curate_culturelevel.py` (bespoke), 19 records (incl. the
  `CULTURELEVEL_ALT_POOR` `ReplacementID` split + the REALISTIC_SPREAD-gated levels). NOT a POCO (the fast-path would
  have mis-homed it). Verdicts:
  - **`iCityDefenseModifier` → `defense.city.amount.percent`** — the one live additive modifier (CvCity.cpp:10184).
    `defense` is a NEW top-level family with two members (owner ruling): `amount` (additive extra-defense %) + `min`
    (the floor clamp some buildings raise above 0; authored at the Building pass via `iMinDefense`). NOT under `combat`.
  - **SpeedThresholds DROPPED — the per-speed scale MOVED to GameSpeed (owner ruling 2026-06-14).** The 171-value
    table was pure precomputation of `base(Normal) × iSpeedPercent/100` (values identical to `iSpeedPercent`; the
    GameSpeed XML even notes the coupling "…update CultureLevelInfos.xml"). GameSpeed now carries
    `cultureThreshold.world.percent` = iSpeedPercent; CultureLevel keeps only the NORMAL `identity.cultureThreshold`
    scalar; the reader derives `threshold = base × GameSpeed.cultureThreshold/100`. 0 levels break the ratio.
  - **Caps/radius KEPT in `identity` for now (owner ruling 2026-06-14 — DEFERRED, not final):** `cityRadius`
    (REPLACE/override) + the 4 wonder caps (`maxWorld/Team/National[/OCC]Wonders`) are non-additive (§3) and stay in
    identity; proper modelling needs extra finnicking + post-move fixing because 3–4 mutually-exclusive
    culture-on-plots game options change how culture/radius behave. Revisit with that rework.
  - **`enables.buildings`** from the wired `BuildingInfo.PrereqCultureLevel` store edge (5 culture-gated buildings);
    **`replacedBy`** `{cultureLevel, onGameOption}` for `CULTURELEVEL_POOR` → `ALT_POOR` (store `ReplacementID`).
    `PrereqGameOption` → `identity.prereqGameOption` (load-prune). `m_iLevel` is runtime-derived → not emitted.
- **Hurry (lighter cleanup, done 2026-06-14 PM):** `curate_hurry.py` (bespoke), 2 records. NOT a POCO — the
  fast-path dumped 3 gameplay fields into identity. Re-homed: the two rush RATES (`iGoldPerProduction` /
  `iProductionPerPopulation`, mutually-exclusive per hurry, the 0/not-applicable dropped) → `conversion`;
  `bAnger` → `causesAnger`; `Button` → `ui.art.icon`. The cascade MODIFIER on hurry cost is elsewhere
  (`BuildingInfo.iHurryCostModifier`, a city-scope %); these are the intrinsic BASE rates. Dropped from
  `curate_pocos.POCOS`.
  - **Grants are source-side (done / deferred):** hurries are GRANTED by civics (`enables.hurries` — all 20
    hurry-granting civics across Currency/Economy/Power grant `HURRY_GOLD`) AND by a special building
    (`HURRY_POPULATION` comes ONLY from a Slavery special-building via `BuildingInfo.isHurry`, CvPlayer.cpp:7503)
    — wire `BuildingInfo → enables.hurries` at the Building pass, parallel to the civic edge. The grantor is
    `BUILDING_WORLDVIEW_SLAVERY`; more of the SLAVERY "system" lives around the worldview buildings — sleeping
    dog until the Building pass, map it there (owner 2026-06-14).
- **Victory (lighter cleanup, done 2026-06-14 PM):** `curate_victory.py` (bespoke), 10 records. NOT a POCO — the
  fast-path dumped its win-condition fields into `identity`. All gameplay fields gather under one **`condition`**
  section — structural data consumed by the EXISTING victory-resolution system ("what is a victory" /
  `CvGame::testVictory`, read every turn), NOT a cascade family. KIND/property booleans (conquest/targetScore/
  endScore/diploVote/totalVictory/permanent) + numeric THRESHOLDS (land/minLand/populationPercentLead/
  religionPercent/numCultureCities/delayTurns) + `cityCulture` (a CultureLevel ref). `VictoryMovie` → `art.movie`.
  - **NOT modifiers, NOT enablers (owner ruling 2026-06-14):** a victory never deposits a cascade modifier, and a
    victory CONDITION is itself NOT an enabler edge — it lives in the victory system above (and AI victory-PURSUIT
    — which condition to steer for — stays in existing `CvPlayerAI`; both are OUTSIDE this migration's scope, which
    only moves the condition DATA). A victory MAY be
    ENABLED, and **that enabler is ALWAYS `world` scope BY DESIGN** (victory conditions are global — identical for
    every player — so an enabler can only live at world). The enabler is owned by the SOURCE, never the victory:
    - `VICTORY_SPACE_RACE` ← the space **Projects** — ALREADY source-captured (each `project_ss_*`/Apollo carries
      a `victory` section: `thresholds`/`minThresholds`/`successRate`/`delayPercent` keyed by `VICTORY_SPACE_RACE`);
      the project's `VictoryPrereq` is the REVERSE (a load-prune gating the parts when the victory is off).
    - `VICTORY_DIPLOMATIC` ↔ the **United Nations** wonder — the enabler story resolves at the Building/WONDER pass
      (+ VoteSource), NOT here. The UN wonder is enabled by EITHER the diplomatic victory being active OR the
      "United Nations without Diplomatic Victory" game option (one of 2 enable paths) — both WORLD-scope, an OR
      composition (→ a building-side BoolExpr). The UN then creates the `DIPLOVOTE_UN` VoteSource that hosts the
      diplomatic-victory resolution. So an ACTIVE victory is itself a world-scope ENABLER of the UN wonder (distinct
      from its non-enabler condition); defer the wiring to Buildings/Wonders (owner 2026-06-14).
  - **Quirks (faithful):** `iTotalCultureRatio` has a live getter but NO record sets it → never emitted (dormant);
    `bPermanent` getter appears UNREAD → purge candidate, kept; `VICTORY_SCIENTIFIC` has zero condition fields
    (trigger elsewhere). Dropped from `curate_pocos.POCOS`.
- **Vote (lighter-three #2, done 2026-06-14 PM):** `curate_vote.py` (bespoke), 30 records. A self-contained
  "**DiplomaticProposal**" (owner: the entity should be RENAMED Vote→DiplomaticProposal — DEFERRED to another day;
  "relatively neatly packaged"). Participates in NEITHER cascade — no cascade modifier, no cascade enabler (owner
  ruling 2026-06-14); it is config for the EXISTING diplo-vote subsystem. Traced the lifecycle: raise
  (`doDiploVote`) → tally (`countVote`) → on pass, record result (`setVoteOutcome`) + apply effects DIRECTLY in
  **`CvGame::processVote`** (:7917-8023) / `doVoteResults`. **NOT `CvOutcome`** (a hypothesis we checked and
  disproved) — `VoteInfo` has no `OutcomeList`; the only "outcome" in CvGame is `m_paiVoteOutcome` = the vote
  RESULT, a different beast from the `CvOutcome` kill/action system. Fields, all intrinsic to the proposal:
  - `voteSource` (DiploVotes; DIPLOVOTE_UN/POPE/CVIENNA) — which council may raise it (read by `isVoteSourceType`);
    kept ON the vote (self-contained), NOT a cascade enabler. The reverse (votesource → its votes) derives
    cold-path when VoteSource migrates — no double-author.
  - `threshold` — `population` / `minVoters` / `stateReligionPercent` (0 dropped).
  - EXACTLY ONE of: `role` (`secretaryGeneral` | `victory` — resolution class) OR `effect` (the on-pass OUTCOME
    fed to processVote: a boolean toggle freeTrade/noNukes/defensivePact/openBorders/forcePeace/forceNoTrade/
    forceWar/assignCity, `tradeRoutes` +N to the game trade-route pool [an OUTCOME, NOT a cascade modifier], or
    `forceCivics` [force the civic on pass — an effect, NOT an enabler]).
  - `mode` (cityVoting/civVoting) — LIVE consumer (`CvPlayer::getVotes`) but all-zero today → emitted only if true
    (none now; kept live, NOT dropped).
- **BonusClass (lighter cleanup, done 2026-06-14 PM):** `curate_bonusclass.py` (bespoke), 12 records. NOT a POCO —
  the bonus CATEGORY/class AXIS (resource taxonomy: MISC/CROP/LIVESTOCK/SEAFOOD/STRATEGIC/LUXURY/PRODUCTION + the
  "not placed on map" producer classes MANUFACTURED/CULTURE/GENMODS/WONDER). The categorization is consumed via
  the BONUS's `bonusClassType` (bonus-side, already migrated) — AI resource awareness/interest (CvPlayerAI:4408/
  4973, CvPlayer:25590-25606) + display filtering (CvGameTextMgr:4107). The class ENTITY carries exactly ONE data
  field (verified vs `CvBonusClassInfo.h`): `iUniqueRange` → `mapGeneration.uniqueRange` — the min-spacing
  preventing same-class bonuses from stacking in close proximity (a C2C_World mapscript feature; CvMapGenerator:
  60-101; 0 dropped; parallel to the bonus's own uniqueRange). No `Description` (pure axis + the one field). "Not
  placed on map" is HARDCODED in consumers + XML comments, not a class field. Dropped from `curate_pocos.POCOS` →
  POCOS now `[CivicOptionInfo, PromotionLineInfo, SpecialBuildingInfo]` (last two are deferred-to-parent placeholders).
- **Civilization (lighter-three #3, done 2026-06-14 PM — CLOSES the lighter cleanup batch):**
  `curate_civilization.py` (bespoke), 54 records, 0 leftover tags (curator has a leftover-tag check). NOT a POCO
  (the mapping saw only the spawnRate pair) — a SOURCE that grants game-start state + gates capability. Shape:
  text (description/short/adjective/civilopedia); `grants` (flat, one-shot at game start) — `buildings`
  (FreeBuildings → the capital; Palace + civ-class) / `civics` (InitialCivics → empire, one per civic-option slot)
  / `techs` (FreeTechs → team); `spawnRate.empire.{general,npcPeace}.percent` (the ONE cascade modifier; barb/NPC
  civs only, 6/54); `policies` (`playable`/`aiPlayable` + `stronglyRestricted` NPC build-lockdown); **`disables`**
  (a `{techs:[…]}` object, symmetric with `grants` so it extends to other kinds later — owner; per-civ research
  ban, NOT a cascade enabler/modifier); `world`/`sound` art (playerColor/artDefine/styles/sounds); `identity` (leaders /
  cityNames / derivativeCiv). **Owner finding:** `FreeTechs` + `disables.techs`
  are Neanderthal-ONLY — `CIVILIZATION_NPC_NEANDERTHAL` starts with primitive techs (CAVE_DWELLING/GATHERING/
  NOMADISM/SCAVENGING/LANGUAGE) and can never research `TECH_SEDENTARY_LIFESTYLE` (stays nomadic; no Palace
  either); every other civ starts at base. Both LIVE (CvGame:1226 / CvPlayer:8266) but minimal — kept faithfully.
  - **Forward civ-capability semantic (owner 2026-06-14 — LOOSE/FUTURE, not in today's data):** `grants` (gives)
    + `disables` (forbids) are the first two of a symmetric capability vocabulary; future siblings — **`replaces`**
    (a unique unit/building SUBSTITUTES a default), **`allows`** (a civ-specific enable), and a **`unique`**
    semantic to finish. **This leads NEATLY into the existing cascading ENABLER (owner):** `allows`/`replaces` ARE
    enabler edges (the civ enables/substitutes a target), `disables` is the anti-enabler — so each could become
    its OWN cascader, slotting into the #430 enabler engine. The current S2S `CivilizationInfo` has NO
    replace/unique/allows fields (the 0-leftover check confirms — no `UnitClass`/`BuildingClass` override blocks;
    civ-specific buildings ride the `grants.buildings` civ-class entries instead). Vocabulary to grow into, not
    data to migrate now.
  - **UI gotcha (owner):** the Python city-screen hurry button supports ONE hurry and takes the FIRST by
    `HurryTypes` enum order; the engine technically lets a player hold several. So the hurries' "mutual
    exclusivity" is a UI/practical property, NOT Hurry-entity data — nothing to author on the entity.
  - **⚠ TWO mechanics share the verb "hurry" (audit finding 2026-06-14 PM — do NOT conflate):** (1) the
    `CvHurryInfo`/`HurryTypes` gold/pop city-rush (THIS entity); (2) a UNIT instant-construction —
    `UnitInfo.iBaseHurry` + `iHurryMultiplier` (great-engineer "Extra Construction": consume the unit in a city
    to add `BaseHurry + HurryMultiplier×pop` production), via `CvUnit::getHurryProduction` and its OWN
    `CvUnit::canHurry(plot)` (a same-named method on a different class). Entirely separate data + code path; carry
    `iBaseHurry`/`iHurryMultiplier` at the **Unit pass**, never under the Hurry entity. (Despair-index candidate.)
- **Era (5th, done 2026-06-14):** `curate_era.py` (bespoke), CONFIG/GLOBAL, enables nothing, world-scope —
  classified by the `classify-era-fields` workflow (5-agent understand + adversarial verify vs
  `CvEraInfo.{h,cpp}` + live consumers). Feeds the SAME world families as GameSpeed: `costs.world.{train,
  construct,create,research,build,improvementUpgrade}` (each cost member carries its OWN era-resolution rule —
  produced-unit / owner / project-prereq era — recorded for the consumption rewrite), `growth`,
  `greatPeopleRate` (GP/GG threshold), `durations.anger`, `eventChance` (FLAT, a probability). Plus
  `costs.world.researchCutBelowEra` = the summed-across-era-bands tech-cost CUT (DIFFERENT math from the
  research base — distinct member, not folded). `grants` (startingGold/units/multiplier/freePopulation — stack
  with Handicap's). `sound` = era audio (soundtracks / introSoundtrack / citySoundscapes / victory+defeat sounds,
  carried as name strings) + `ui.art.icon`. `identity` = pacing inputs (historicalStartYear/EndYear/normalSpeedTurns — turns/calendar
  DERIVE downstream on GameSpeed/CvDate) + advancedStart (parked, like Handicap). 14 written, 0 module eras.
  DROP `bNoAnimals` (no C++ consumer). The barbarian world-gates + initial-city-maintenance are 0/absent in
  every era → not emitted (boolean-world-gate family placement deferred until an era actually sets one).
- **`ai:` audience convention (new, 2026-06-14):** where one channel carries distinct human vs AI values,
  the AI value lives in an `ai:` sub-object beside the units (`unitUpkeep: {percent: 60, ai: {percent: 120}}`);
  an AI-only channel is just `{ai: {...}}`. Generalizes to any future entity needing an audience split.
- **Civic (FIRST HEAVY entity, done 2026-06-14):** `curate_civic.py` (bespoke), 175 civics, classified by the
  `classify-civic` workflow (9 field-slice agents + dead/double-author auditors + adversarial verify) against
  CvCivicInfo + the CvCity/CvPlayer/CvTeam consumers. **Civics are BOTH enablers AND modifiers** — each object
  carries an `enables` block (buildings/units it gates via PrereqCivic + capability lists `hurries`/
  `specialists`/`specialBuildingsWaived`) AND ~83 modifier families. All modifiers are **EMPIRE scope** (a civic
  deposits via CvPlayer::processCivics — the mapping's player/city scopes were systematically wrong). Owner
  rulings: **Revolution** (RevolutionDCM, ~14 iRev*/fRev*) kept FAITHFUL under a `revolution` family — the logic
  lives in Python only because the original modder didn't know C++, and it is slated to move **Python→C++
  (Python = presentation only)**, so it's live data, NOT cruft (floats carried verbatim). Boolean policy flags
  (~17 bStateReligion/bNoForeignTrade/bAllowInquisitions/…) → a dedicated **`policies`** section (empire-scope
  booleans). The 5 iStateReligion* effects → a grouped **`stateReligion`** family (gated on having a state
  religion; civic-owned, NOT inverted onto a religion). Files **foldered by CivicOption category**
  (`Assets/Data/civics/<government|economy|…>/`, 15 categories; folder = short name, `identity.civicOption` =
  full Type). Dead-drops (verified no consumer): `iProductionModifier`/`iHappinessChange`/`iHealthChange` (not
  top-level — value-elements of keyed maps), `Categories`, `isAnyImprovementYieldChange`. Double-author drops:
  `SpecialistYield/CommercePercentChanges` (already on the specialist). `BonusCommerceModifiers` → inverted onto
  the bonus (new curate_bonus row; empty in data — see the §0 "CREST" note for the open ownership question).
  Smaller calls: `CivicAttitudeChanges`→`diplomacy` keyed by civic (the cosmetic per-edge `Description` label
  dropped), `iMaxConscript`→`conscript`, `iFreeSpecialist`→`freeSpecialists`, `iAnarchyLength`/`Upkeep`/
  `WeLoveTheKing`→identity. Store gains civic→building/unit enables (`PrereqCivic`/And/Or).
- **Trait (heavy entity #2 — "Mount Doom", DONE 2026-06-14):** `curate_trait.py` (bespoke), classified by the
  `classify-trait` workflow (`wf_cc8659b5`: 3 ground-truth agents + 6 field slices, each adversarially verified,
  + a coverage/conflict/CREST/dev-leader audit) against `CvTraitInfo` + `CvPlayer::processTrait` + the
  CvCity/CvGameTextMgr consumers. Analysis saved to `Tools/Migration/classifications/trait-classification.json`.
  **ONE `CvTraitInfo` class serves BOTH trait systems** — the "developing/complex leaders" system is the SAME
  class, assigned differently: `CvLeaderHeadInfo` carries `DefaultTraits` AND `DefaultComplexTraits` (both lists of
  `TraitTypes`), picked by `GAMEOPTION_LEADER_COMPLEX_TRAITS` (CvPlayer.cpp:439/:1583). Assignment (fixed vs
  accumulating) is CONSUMER-side; the data is one uniform `trait` surface. A trait is an EMPIRE-scope
  SOURCE/ENABLER (deposits via `processTrait`); never a target. Conventions MIRROR civic (production vs
  unitProduction split; grouped `stateReligion` family; `PropertyManipulators`→per-`PROPERTY_*` families).
  **390 files** = 87 base + 240 developing-line + 63 complex. Owner rulings:
  - **⚠ THE `ReplacementID` DISCOVERY — the classify workflow MISSED it (not a `getDataMembers` field).** The
    vanilla↔complex link lives in a SEPARATE generic class **`CvInfoReplacements<CvTraitInfo>`** (CvGlobals.h:992),
    driven by the XML tags `<ReplacementID>`+`<ReplacementCondition>`: **64 vanilla traits each carry a "complex"
    alter-ego defined INLINE under the SAME `<Type>`** (`ReplacementID=TRAIT_COMPLEX_*`, which exists NOWHERE as
    its own `<Type>`), swapped in WHOLESALE when `LEADER_COMPLEX_TRAITS` is on — a FRESH full Info, no base merge
    (CvXMLLoadUtilitySet.cpp:1587-1604; `CvInfoReplacements::updateReplacements`). `store.py` merged by Type → it
    FUSED each overlay onto its base (64 Frankenstein records: a base description with a complex strategy).
    **Fix (owner: "split + `replacedBy`"):** `store.py` is now `ReplacementID`-aware — it re-keys the overlay as
    its OWN Type (the complex variant, curated as a full trait) and records the base
    `replacedBy: {trait: TRAIT_COMPLEX_*, onGameOption: LEADER_COMPLEX_TRAITS}` edge; the base stays clean; the
    reverse `replaces` derives cold-path. Generic for Building/Unit later (only traits populate it today — 0
    elsewhere in the migrated set). All 64 conditions are identical (`Has`/`GOM_OPTION`/`LEADER_COMPLEX_TRAITS`).
  - **Dev-leaders relations (inventory→decided "accept the fits"):** `TraitPrereq`/`TraitPrereqOr1`/`2` +
    `PrereqTech` invert via the store to top-down `enables.traits` on the prereq trait / tech (dropped trait-side;
    2 "lifestyle" techs gate ~152 developing traits; 150 trait→trait edges). `DisallowedTraitTypes` → a same-tier
    `excludes` set (author one end, derive reverse — a STRUCTURAL mutual-exclusion, NOT the #429 spatial sideways).
    `OnGameOptions`/`NotOnGameOptions` → a `loadPrune` gate (bulk = `LEADER_COMPLEX_TRAITS`). `PromotionLine` +
    `iLinePriority` → a `succession` block (the line's own `PrereqTech` is the derived tech enabler, owned by
    PromotionLineInfo). `Categories` → DROP (dead: zero C++ readers; civic drops it too). The culture-req
    progression (`GAMEOPTION_NEXT_TRAIT_CULTURE_REQ_PERCENT`) is a game option, NOT a trait field.
    `GreatPeopleUnitType`+`GreatPeopleRateChange` FOLD → `greatPeopleRate.empire.units.{UNIT}.flat` (keyed by the
    GP unit; CvPlayer.cpp:28606-28610, only when >0).
  - **CREST:** `BonusHappinessChanges` (the ONLY fresh bonus-conditioner; 26 traits set it) FOLDS onto the bonus
    (new `curate_bonus` `BONUS_BOOSTS` row, `city` scope, keyed by trait — dropped trait-side; matches the
    building/civic→bonus fold). `TechResearchModifiers` is crest-adjacent but STAYS trait-side per the HANDOFF
    (`research.empire.byTech.{TECH}.percent`). NOTE: traits surfaced only TWO crest cases (not the volume §0
    expected), so traits alone do NOT fully codify the crest boundary — revisit at #430 / Building/Unit.
  - Smaller calls: **`cityFounding`** family for `CityStartCulture`/`BonusPopulationinNewCities` (standing empire
    accumulators applied at every founding, NOT one-shot grants). `GoldenAge{Yield,Commerce}Changes` → a
    `goldenAge` condition-member (player STATE, NOT a crest). `MaxAnarchy`/`MinAnarchy` → clean identity keys
    (max default −1 carried verbatim). `fRev*` floats carried verbatim. Double-author DROP:
    `SpecialistYield/CommerceChanges` (the specialist owns them, `curate_specialist.py:80-81`).
  - **`workRate` is the standard worker-speed family (owner ruling):** TECH re-curated `workerSpeed`→`workRate`
    (`mapping/TechInfo.json`); civic was already `workRate`. So `workRate` is now universal (trait scalar +
    `workRate.empire.builds.{BUILD}` from `BuildWorkerSpeedModifiers`).
  - **NEXT heavy: Improvement.**
- **Migration order — owner ruling 2026-06-14: top-down along the enabler spine, HIGHER-ORDER / config
  entities FIRST, defer the monsters (Building, Unit) to LAST.** Every simpler entity exercises and pins
  down the model + curator before the hard ones, so by the time we reach Building the toolkit/conventions
  are proven, not improvised — this REDUCES DRIFT. Order: Tech ✓ · Bonus ✓ · Handicap ✓ · GameSpeed ✓ · Era ✓ ·
  6 POCOs ✓ · then early-chain enablers (Civic, Religion) + light-gameplay (Route/Process/…) · … · Building, Unit LAST.
- **Loader-order CORRECTION — top-down beats craziest-LOADERS-first; AUDIT properly on the way down (owner
  ruling 2026-06-14 PM).** The earlier "target the craziest LOADERS first" prioritization (success-metric
  section below, retracted there too) was a BAD idea. Going top-to-bottom along the cumulative enabler hierarchy
  (config → … → Building/Unit, above) is the proven order. The real gap was NOT the order — it was skipping a
  proper per-entity AUDIT on the way down (fast-pathing "looks inert" entities without checking the live
  consumer). That gap unearthed the surprises: the combat-class circus (#435), SpecialBuilding (a build-group-
  with-a-cap, not a POCO), and the POCO mis-classifications (0 of 6 truly inert). RULE: every entity gets a real
  consumer-audit before curation, however trivial it looks — NO fast-paths on faith. **The "group-ish but not
  really" pattern recurs (owner):** SpecialBuilding is "kind of a group but not really" (a per-player-capped
  build-group); combat-class is expected to be the same shape — a category pretending to be a simple grouping
  while carrying real, irregular gameplay. Treat every "grouping/category" entity as guilty-until-audited.
- **HEAVY-ENTITY CAUTIONS (owner-flagged 2026-06-14 — each gets its own `classify-<entity>` workflow, NOT a
  mechanical pass):**
  - **Trait — ✅ DONE 2026-06-14 (see the §7 "Trait" note + the CHECKPOINT).** Resolved: ONE `CvTraitInfo` class
    (not two); the two systems differ only in ASSIGNMENT (consumer-side, game-option-gated). The real two-system
    link the workflow MISSED is the **`ReplacementID`/`CvInfoReplacements` conditional whole-Info replacement**
    (64 vanilla→complex pairs) — modeled as split Types + a `replacedBy` edge; `store.py` is now ReplacementID-aware
    (was Frankenstein-merging). Crest was small (2 cases). Original caution kept below for history:
  - **Trait** — TWO alternative trait SYSTEMS selected by a GAME OPTION (a game runs ONE or the other; a leader
    does NOT take one from each): the vanilla fixed leader-trait set (classic Civ4, easier) vs a
    developing-leaders system where traits ACCUMULATE over the game (harder). The GOAL is for both to express
    effects through ONE shared `trait` interface (uniform definitions + modifiers; ASSIGNMENT — fixed vs
    accumulating, game-option-gated — is CONSUMER-side, not the data shape). **OPEN, verify FIRST: it is
    UNKNOWN whether the developing system even uses the same `CvTraitInfo` class or a SEPARATE one** —
    `LeaderHeadInfo` carries both `DefaultTraits` AND `DefaultComplexTraits`, hinting at a possible separate
    "complex trait" class. The Trait workflow's FIRST job is to establish one-class-vs-two, then reconcile to a
    shared `trait` interface if they're separate — **both sets must align their authored surface in the code**
    (uniform `trait` definitions). (68 channels — big modifier surface.) Traits are SOURCES/ENABLERS (§0 cascade
    ontology — never targets), so expect the conditioner **"CREST"** cases (§0) IN FORCE here; gather the full
    set of them at this entity before codifying the stay-vs-invert boundary. **OWNER FRAMING (2026-06-14): this
    is Mount Doom — the HARDEST entity, worse than Building/Unit despite their far larger count**, because the
    pain is the TWO-SYSTEM reconciliation + TB-mega-mod decades-of-cruft, NOT the entry count (Building/Unit are
    more numerous but more uniform/mechanical). **Do it in a FRESH, uncompacted context**: run the
    `classify-trait` workflow FIRST (one-class-vs-two → reconcile both sets to one `trait` surface → per-field
    dispositions), confirm the structural calls with the owner, THEN curate. Budget the whole entity ≈ a session.
  - **Civic** — "on actual drugs for a couple of decades": decades of accumulated cruft / weird interacting
    fields (104 channels). Needs real PONDERING of what each field still does + heavy cleanup judgment, not a
    fast port. Expect dead/vestigial fields and non-obvious cross-effects.
  - **Building (101 ch + 22 inversions + 47 prereqs) / Unit** — the monsters; the inter-building A→B effects and
    the OR/NOT `ConstructCondition` BoolExpr. Each is ~a session on its own.
- **Branch (2026-06-14):** the migration lives on its own clean main-based branch **`json-data-migration`**
  (the #421/#423 cascade C++ stays on `buildings-json` for its own PR) — one unified surface.

---

> **The section below ("THE MIGRATION SPEC" and the inversion-per-wave material after it) is the
> earlier 2026-06-13 framing.** It is retained for its field inventories and per-entity verdicts, but
> where it conflicts with "THE MODEL (locked 2026-06-14)" above, the locked model wins.

## THE MIGRATION SPEC (consolidated — earlier framing; see note above)

> Reconstructed 2026-06-13 after the export drifted into a partial. The detailed owner rulings below are the
> source; this is the single checkable summary. **If an implementation diverges from this, the implementation is wrong.**

**Goal.** Every entity's **FULL** data moves XML → JSON, single source, XML deleted. Not just the modifier subset —
**modifiers, prerequisites, cost, art defines, FKs, identity — all of it.**

**Per-entity JSON shape** (building example):
```json
{
  "type": "BUILDING_X",
  "modifiers": {                                  // NOT "cascadeModifiers" -- see naming ruling
    "city":   { "happiness": {"flat": 3},
                "yield":     {"flat": {"food": 2}},
                "maintenance":{"percent": -25},
                "noUnhappiness": {"enabler": true} },
    "area":   { ... },
    "player": { ... }
  },
  "prerequisites": { ... },   // tech / bonus(connected vs vicinity) / building / civic / ... (BoolExpr tree)
  "cost":          { ... },   // iCost + cost modifiers
  "art":           { ... },   // button / movie / sound / model defines
  "identity":      { ... }    // special-building, obsolete, extends/upgrade FKs, flags
}
```
- **`modifiers`, not `cascadeModifiers` (owner ruling 2026-06-13).** A modder follows the *scope* rule (`city`/`area`/
  `player`/…); they never need to know the value cascades. "Cascading" is an internal C++ concern, not modder vocabulary.
- **Modifier shape:** `scope → channel → kind`, kind ∈ {`flat`, `percent`, `enabler`}; indexed channels are keyed objects.
- **Keys:** yield/commerce SHORT + enum-free (`food`, `gold`); data-driven Types FULL (`BONUS_COAL`, `TECH_POTTERY`).
- **Sections are ALL the data** — `modifiers` is one of several; prerequisites/cost/art/identity are equally required.

**File layout.** `Assets/Data/<type>/<ERA>/<entity>.json` — **era sub-folders** (derive era from prereq tech). Loader
**recurses** the era dirs. **Ship loose** (raw `FindFirstFile` only sees loose). Modules **consolidated** to merged
state into the era layout (retired). Modmods = a separate later overlay tier (outside this repo).

**This is a RESTRUCTURING, not a faithful copy (owner clarification 2026-06-13).** The data deliberately comes out
DIFFERENT — inversions relocate effects to their conditioner, the channel-object reshapes modifiers, enabler edges
flip. So there is no "XML == JSON" round-trip to verify, and a faithful `writeJson` export is the WRONG tool. The
migration is a **coordinated change in two halves that must line up**: (a) the C++ Infos move to the new structure
(`CvTechInfo.buildingBoosts`, building's `getDataMembers` → channel-object, building loses `TechYieldChanges`); and
(b) the data transforms to that same structure. The Info loads the new JSON; they must match.

**Engine.** Generic `readJson` (load) on the `CvInfoUtil` wrappers — already built — **plus an OFFLINE Python
converter** that transforms XML → the new JSON structure (it reads the Type-strings straight from XML, no
enum-reverse; iterates in seconds, no C++/game-launch cycle). **No C++ `writeJson`** — there is nothing to
faithfully export, the structure is changing. The C++ `exportBuildingsToJson` prototype RETIRES.

**Migration atomicity (owner, 2026-06-13): the unit is the RELATIONSHIP, not the entity.** A relationship spans two
entities (tech↔building, religion↔building), so pulling `building.PrereqTech` off without adding `tech.enabledBuildings`
declares the edge NOWHERE — the system stops knowing the building is buildable. But this does NOT force a big-bang of
buildings+techs+religions in one shot. The constructibility consumer reads an enabler **index** ("which buildings does
this tech enable"), not the building's prereq directly. So: move each edge ATOMICALLY (out of the old home, into the
new, together); feed the index from BOTH structures during transition (old building-prereqs + new tech-`enabledBuildings`);
as edges move, the index reflects the current state and constructibility stays correct; delete each structure when it
empties. The migration is INCREMENTAL (edge by edge), the index is the decoupling layer that covers the in-between,
and the **only** forbidden move is the *isolated* one (one end without the other).

**On #195 (owner, 2026-06-13):** #195 is relevant in the ABSTRACT, transitional in the concrete. Its *implementation*
(derive the reverse-index by inverting building-side prereqs) assumed the OLD model and is superseded — the new model
**declares** the enabler canonically on the source (`tech.enabledBuildings`), nothing to derive. What survives: the
index *shape* (now declared, not derived), `BoolExpr` for the AND/OR/NOT composition minority, and the perf win (no
O(buildings²) PreLoop — a canonical `enabledBuildings` is O(1), no derivation pass). #195's derivation keeps ONE live
role — the migration BRIDGE that feeds the index from the old building-prereq side until every edge reaches the
canonical home, then it RETIRES. Do not build on #195's inversion as permanent.

**#195, #196, and the #196 sub-issues are SEMI-OBSOLETE (owner ruling 2026-06-13).** Both assumed the OLD data model
stays intact; the restructuring supersedes their goals. **What we keep:** #196's `CvInfoUtil` wrapper infrastructure
(`readJson` is built on it — the reusable lever), and #195's index *shape* + `BoolExpr` (the composition minority).
**What's superseded:** the per-class "load the old structure declaratively" goal (#196 sub-issues) and the
"derive the index from building prereqs" mechanism (#195). They are NOT treated as constraints on this work — take
what's useful, mark the rest semi-obsolete, and get the migration done without bending the new model to fit the old plans.

**Static-prune vs dynamic-query — resolve every gate at the latest moment it CANNOT change, not later (owner insight
2026-06-13).** A gate splits by WHEN its condition is fixed:
- **LOAD-STABLE gates** (GameOption on/off, module/map/world/speed setup — changed only RARELY and by hand:
  WorldBuilder, or a one-off BUG-menu toggle; never an automatic gameplay loop): resolve at load against the current
  config. If false, **do not load the data at all** — prune the whole entity (a promotion `NotOnGameOptions` an active
  option → never materialized) or the single gated effect/channel. Absent data = absent capability; ZERO runtime checks
  and a smaller working set (C2C ships 1131 promotions / 1105 units / 3869 buildings — pruning the disabled ones is a
  real memory + per-loop-iteration win, not just tidiness). NOT immutable: on any such rare manual change (WorldBuilder,
  BUG menu) just **rebuild the data** (re-run the loader from the on-disk JSON). It is an editor-only action, so a full
  rebuild is fine — no bespoke slice-tracking, the loader already does it. (Whether mid-game option-changing is even
  worth supporting is a separate open question; rebuild-on-change makes keeping it cheap, so the data model needn't
  force that call. Disallowing it — options fixed at game creation — is equally valid and simpler.) The win holds
  across all normal play; only the rare editor event pays.
- **DYNAMIC gates** (tech / era / civic / bonus presence — AND **handicap under flexible difficulty**, which re-rates
  it automatically every few turns — flip DURING play): cannot be pruned (they change again, often automatically) →
  runtime queries (the cascade enabler / #195 index).

**The real discriminator is mutation FREQUENCY, not mutability.** (1) no mutation path → prune; (2) RARE manual/editor
mutation (WorldBuilder, a one-off BUG-menu toggle) → prune + REBUILD-on-change (amortizes to ~never); (3)
FREQUENT/AUTOMATIC mutation (flexible difficulty thrashing handicap inside the turn loop) → do NOT prune, query at
runtime — rebuild-on-change would fire in the gameplay loop. Handicap is the cautionary case: it LOOKS like fixed setup
config but a feature makes it hot. So enumerate EVERY mutation path (and its frequency) before declaring a gate prunable.

**Two practical refinements:** (a) a prune can be CONDITIONAL on a load-stable META-option — "is flexible difficulty
enabled?" is itself a load-stable game option, so the handicap prune is decided at load: flex-diff OFF → prune
handicaps to the active one; flex-diff ON → load all 12 and select the active at runtime. The dynamic case is just
"load all," chosen once at load by reading a load-stable option — no rebuild-thrash. (b) Prune is a SIZE-PROPORTIONAL
win: handicap is 12 tiny records, so "load all" costs nothing — don't bother pruning small sets, spend the machinery on
the big ones (promotions/units/buildings) where it actually moves memory and per-loop cost.

So the model carries TWO gate kinds: **static → PRUNE-at-load, dynamic → QUERY-at-runtime.** It is dead-DATA
elimination per game config, and static edges never enter the runtime enabler index (keeping it lean). Granularity:
prune at BOTH the whole-entity level (drop the promotion) and the per-channel level (don't deposit a game-option-gated
modifier). Catch: pruning an entity orphans inbound FKs (a unit's `FreePromotions` naming the pruned promotion) —
soft by construction (the name-keyed format drains orphan Type refs on load via `_ALLOW_MISSING`), but the reader must
prune refs consistently. Mapping impact: split the prereq bucket — tag GAME-STATIC eligibility
(`OnGameOptions`/`NotOnGameOptions`/`PrereqGameOption`/module-enablement) as `loadTimePrune`, distinct from dynamic
prereqs (post-mapping refinement; agents currently lump both as prereq). The pruning itself is a READER behaviour
(C++ reader, dead last) — but the data model must mark which gates are prunable now.

**Verification.** FUNCTIONAL / effect-preservation, in-game — NOT byte-parity. A reshaped value preserves its
effective value (building still `happy=3`); a relocated effect preserves its EFFECT (the tech still boosts the
building the same, now via `buildingBoosts`). The game computes effective values from the new structure; we confirm
behaviour is preserved. **Move-and-delete** (XML tag deleted once JSON carries the value) gated by this functional
check, not a round-trip.

**Cross-entity.** Conditional cross-entity effects invert to their conditioner (the conditioner rule + the structural
principles below).

## CURRENT STATE vs SPEC — re-verified 2026-06-13 (the honest gap list)

| Spec requirement | Status |
|---|---|
| Modifier structure (bundle: scope→channel→kind) on buildings | ⚠️ ONLY happy/health (3 scopes) + `noUnhappiness` enabler bundle-ified; the other ~95 modifier fields are still loose ints |
| `readJson` across wrappers | ⚠️ scalar/enabler/enum/enum-as-int/string/char-array/vector/scalar-`IDValueMap` done; **BoolExpr, paired-array (2D), struct-vector NOT done** |
| `writeJson` across wrappers (the export engine) | ❌ **not built** — the export hand-emits only the modifier slice from getters |
| Whole-entity export (modifiers + prerequisites + cost + art + identity) | ❌ **only the modifier slice** — missing art defines, prereqs, cost, identity, FKs |
| `modifiers` naming (not `cascadeModifiers`) | ❌ loader (`s_buildBuildingFlat`), export, and the test file all still say `cascadeModifiers` |
| Era sub-folders + recursive loader | ❌ export writes **flat** `Data/buildings/`; loader is non-recursive |
| Move-and-delete / verify-before-delete | ❌ not started — **no XML deleted; nothing parity-gated** |
| Real data produced | ⚠️ a partial modifier-slice export exists; not the agreed whole-entity data |

**Conclusion:** what exists is a *prototype* — the modifier bundle on a few fields, partial `readJson`, and a
modifier-slice export — plus the architecture/blueprint. The agreed migration (whole-entity, all 236 fields, era-folders,
`writeJson`, move-and-delete) is largely **ahead of us**, not behind. The recent export was a partial that skipped the spec.

## OWNER RULING (2026-06-13): whole-entity JSON, single source, no split surface

The target is **the entire `CvBuildingInfo` in JSON, zero building data left in XML** — not just the modifier
subset. Reasons (owner): data living in both XML and JSON at once is a modder trap — "they would not know what
value actually changes something." So:

- **Move-and-delete, never overlay.** When a field's data lands in JSON, its XML tag is DELETED in the same step.
  No field ever exists in both places. The overlay-on-top-of-XML used to prove the pipe was prototype scaffolding,
  not the end state. (The loader already supports this: an absent XML tag defaults, then JSON fills it — no loader
  change needed.)
- **Verify-before-delete invariant.** Every deleted XML tag MUST be reproduced by JSON or the value silently drops
  to default. Safe sequence per field: generate JSON from XML → confirm parity (endpoint / `BuildingJson.log`
  old→new) → then delete the XML tag. Verification gates the deletion.
- **The "ass" maps get BETTER in JSON.** The complex/indexed families are ugly in XML (element-soup + positional
  arrays), clean in JSON (keyed objects): `<BonusHealthChange><BonusType>BONUS_COAL</BonusType><iHealth>1</iHealth>`
  → `"bonusHealth": {"BONUS_COAL": 1}`; positional `<iYield>1</iYield>...` → `"yield": {"flat": {"FOOD": 1}}`. The
  split-surface the owner rejects and the map ugliness are removed by the same move.
- **The A-vs-B (indexed cascade surface) question is moot as a user-facing call** — the modder JSON is identical
  either way. Take the light path: migrate all indexed/map DATA to JSON now; defer the internal indexed-cascade
  datastructure until a consumer needs cross-scope indexed cascading.

### Proper architecture: complete `readJson` across the CvInfoUtil wrappers (not per-field hacks)
`CvInfoUtil` already loads every field type from XML via typed wrappers (IntWrapper, EnablerWrapper [done],
EnumWrapper / EnumWithDelayedResolutionWrapper, IntInfoClassWrapper [addEnumAsInt], StringWrapper /
FixedCharArrayWrapper, VectorWrapper, IDValueMapWrapper / …, the BoolExpr wrapper). Add the JSON mirror
(`readJson`) to EACH wrapper ONCE — then the whole building, and every other declarative Info (traits, units,
civics), is JSON-native generically. This is the reusable lever; it also makes the developing-leader traits litmus
JSON-native for free.

### Staged plan
1. **Foundation** — complete `readJson` across all wrappers (enum/FK by-name, string, vector, IDValueMap, BoolExpr).
2. **Schema** — the full building JSON shape: `cascadeModifiers` + `prerequisites` + `cost` + `art` + `identity`.
3. **Convert + verify** — emit complete JSON per building from current XML, parity-check (endpoint/log).
4. **Delete XML** — strip the migrated building data; JSON becomes the sole source.

## OWNER RULING (2026-06-13): conditional cross-entity effects live on the CONDITIONER, not the building

`CvBuildingInfo` is static template data (identical for every player). An effect *conditional on player
state* — "this building's yield depends on which techs you've researched" — does not belong on a static
template; it bakes player-state into data that should answer "what am I?" without knowing whose empire it is
in. **The rule: a building holds its INTRINSIC effects; an effect conditional on another entity lives on that
entity and cascades/indexes in.** A tech is a player-scope source, so a tech-conditional building effect is a
tech effect: *the tech says "I boost `BUILDING_X`"*.

**Moves OFF the building → onto the tech** (and they CONSOLIDATE — four parallel building maps become one
per-building bundle on `CvTechInfo`):
- `TechYieldChanges`, `TechHappinessChanges`, `TechHealthChanges`, `TechCommerceChanges`
  → `CvTechInfo.buildingBoosts: { BUILDING_X: { yield: {food:1}, happiness: 1, health: 1, commerce: {gold:2} } }`.

**Same rule, pending the owner's call on each** (candidates to move to their conditioner):
- `BonusHealthChanges` / `BonusHappinessChanges` → the bonus? · `ReligionChanges` → the religion? ·
  civic-gated / vicinity-bonus-gated effects → their conditioner?

**Implementation note (coordinated migration, its own task — does NOT block the building core):** the consumer
inverts. Instead of the building reading "my tech bonuses," the tech deposits its building-boosts into a
player-scope per-building index (the cascade / derived repository) and the building reads its *effective* yield.
Same source→target philosophy as the rest of the cascade.

**The payoff (the real reason for the inversion):** the building/city yield calc becomes **player-state-agnostic**
— no per-recompute scan of "which techs/religions/civics touch me," just an `O(1)` read of effective bonuses. The
conditional resolves ONCE at the event (tech acquired → deposit), moving cost off the hot path onto the rare
event. The deposited bonuses are **derived, not serialized** (recompute-on-load from techs × buildings, #423) —
so no save-break, no migration; the state reconstitutes on load. This is the enabler/source→target index pattern
again, and it cuts the building calc free of the entire player-state web.

Until the tech-side migration lands, these fields stay on the building XML (NOT migrated into building JSON — that
would entrench the wrong home); the building JSON simply omits them. So they drop out of the building conversion's
complex-map flag-list (§4) entirely.

## OWNER RULING (2026-06-13): a gnarly structure gets REVIEWED, not mirrored

The conversion is **not** a mechanical XML→JSON mirror. If a structure is gnarly, we review whether it is the
RIGHT structure first — **existing structure (inherited from C2C/BtS) does not mean correct structure.** Gnarliness
is usually a *symptom*: a 2D map or an awkward keyed collection is often a relationship declared from the wrong
side. **References (FKs, cross-entity maps) get special scrutiny** — they are exactly where a relationship lives on
the wrong entity (`TechYieldChanges` was the first: gnarly *because* it belonged on the tech). So every
gnarly/reference building field is reviewed for right-owner + right-shape **before** migration; many will relocate
(per the conditioner rule above) or reshape, not just move into building JSON. [[project-identity-s2s-not-c2c]]:
inherited conventions are not constraints — only the closed EXE binds.

**Success metric (owner, 2026-06-13): the inversions exist to ELIMINATE the craziest LOADERS.** A bespoke/creative
XML loader (nested element-soup, delayed resolution, 2D-map readers, the hand-written `getDataMembers` remainder) is
the loader *coping* with a wrong-side, awkward shape. Put the reference on the correct entity → the shape collapses
to a plain keyed map → the loader is a one-line generic `readJson`/`writeJson`. **We measure the migration by loaders
DELETED, not JSON files added** — the gnarly loader is not reimplemented in JSON, it is *removed*. The fix is
structural (correct home), never more cleverness. ~~This also drives PRIORITIZATION: target the craziest loaders
first.~~ **SUPERSEDED (owner, 2026-06-14 PM): "craziest LOADERS first" proved a BAD idea — go TOP-DOWN along the
enabler hierarchy with a proper per-entity consumer-audit on the way down (see "Loader-order CORRECTION" in §7).
The structural-fix point above still holds.**

## OWNER RULING (2026-06-13): file layout — era sub-folders, modules retired, ship loose

- **Granularity scales with count.** Handfuls (handicaps, eras) → per-entity or single file. Thousands (buildings,
  units) → grouped. One-time load cost is a non-issue (the 50k-line XML parse already isn't cheap; picojson is no
  worse, likely faster) — the only bar is "doesn't get truly out of hand."
- **Era sub-folders.** `Assets/Data/buildings/<era>/<building>.json`. Game-meaningful axis, bounds per-folder file
  count to hundreds (no hard FS limit, but huge single folders are operationally annoying). Loader recurses era
  dirs; the XML→JSON migration derives each building's era from its prereq tech to file it.
- **Modules retired, not preserved.** The existing C2C modules are baseline-adjacent (inactive authors, more
  confusion than help) — NOT a modularity boundary to keep. (The module system was an SVN-single-branch-era
  workaround so contributors editing one shared tree didn't collide; git branches make it obsolete.)
- **Modmods are a separate, supported tier (owner) — not the old modules.** Retiring in-tree modules is NOT
  rejecting extensibility. CORE data (this repo) is consolidated single-source; **the data in the core mod IS this
  repo.** A THIRD-PARTY modmod is a clean overlay PACKAGE — new Types and/or field overlays on existing Types —
  player-installed à la ComfyUI/GitHub plugins, loaded AFTER core. The **same JSON overlay machinery is the
  primitive** (overlay-core-then-modmods). "No split surface" governs core only; a modmod overlay is an EXPLICIT,
  opt-in, ATTRIBUTABLE layer (provenance logging names which modmod set a field — see the provenance/divergence
  logging section) — categorically different from the old implicit baseline split. Modmods live OUTSIDE this repo. The migration consolidates each entity FULLY MERGED
  (after the existing module-overlay merge) into the era layout; module load-order/override layering is baked into
  canonical data and retired. JSON-native is the replacement modding surface.
- **Ship loose (pending verify).** The loader enumerates with raw `FindFirstFile`, which only sees LOOSE files; if
  data were FPK-packed it would silently miss them. Data must ship loose like the XML does (verify the deploy/FPK
  path). If packed is ever required, switch enumeration to the engine VFS or a manifest.

## Cascade structural principles (owner, 2026-06-13) — the rules the whole pass adheres to

Three invariants emerged while fixing the bonus-health leak; they govern the entire model:

1. **Direction is enforced by the structure, not by discipline.** Each cross-entity relationship has ONE
   canonical direction, and the schema only provides the canonical slot — a specialist has `buildingBoosts`;
   a building has NO `specialistBoosts` slot, because it doesn't exist. So you literally cannot declare an
   effect the wrong way ("no creativity the other way"). Mis-homing (the TechYieldChanges / bonus-health
   class) becomes **impossible by construction**, not merely discouraged.
2. **Two graphs, fixed roles.** ENABLER (presence / `BoolExpr` / #195) is a DIRECTED DAG following enablement:
   the physical base (plot / terrain / feature / improvement / bonus) → buildings → units. A building never
   enables the base; specialists enable nothing (zero enabler out-edges). MODIFIER (the cascade) reaches every
   target; **specialists live here as buff-sources only** — "specialists buff other things" (a specialist→building
   buff is declared on the specialist, caught by the correct inversion). Base enables, specialists buff,
   buildings aggregate. **Enabler edges carry a SCOPE:** the same `bonus→building` enablement is PLAYER-scope when
   the resource is trade-connected (enables empire-wide — `PrereqAndBonus`) and CITY-scope when it's a vicinity
   bonus (enables only the local city — `PrereqVicinityBonus`/`RawVicinityBonus`). So an enabler is a **scoped
   presence atom**: the cascade scope chain is WHERE its presence is queried, exactly as it's where modifiers are
   summed. The `BoolExpr Has(X)` leaf carries which scope it asks at; the chain gives Area ("a bonus on the
   continent") / Team / Game for free. **One spine (Game→Team→Player→Area→City), two verbs: modifiers SUM down it,
   enablers QUERY presence at a point on it.**
   **Enabler EDGES are top-down too (owner, 2026-06-13).** Just as a tech declares its modifier `buildingBoosts`, it
   declares what it ENABLES — `tech enables building` — same direction, source-declared, building reads. So the simple
   `PrereqTech`/`PrereqBonus` edges INVERT off the building (the wrong-side `m_iPrereqAndTech` goes away) and the
   #195 enabler reverse-index becomes CANONICAL on the source, not derived from building prereqs. The honest split:
   the enabler ATOMS are top-down (the source's enabling presence); the building's complex prereq COMPOSITION
   (`BoolExpr` AND/OR/NOT) stays building-side because it is the building's own requirement LOGIC — a query OVER the
   top-down atoms, not a relocatable reference. (Revises the earlier "all prereqs stay on the building": simple
   enabler edges go top-down; only the AND/OR/NOT composition stays.)
   **Concrete shape (owner):** `CvTechInfo.enabledBuildings: ["BUILDING_GRANARY", "BUILDING_FORGE"]` — a flat array
   of building ids (a trivial `VectorWrapper`; the building's `PrereqTech` FK disappears). The tech then declares
   both verbs side by side: `enabledBuildings` (what it enables) + `buildingBoosts` (what it modifies). A building
   is constructible when ALL entities that list it are satisfied (implicit AND — the #195 reverse-index, now
   canonical from these top-down lists). OR/NOT prereqs are the minority that can't be a flat list and keep a
   building-side `BoolExpr`.
   **No callbacks; units are terminal (owner, 2026-06-13).** The target never references its source — the building
   doesn't know the tech exists, the unit doesn't know the building exists. The source declares `enabledX`
   (top-down), the enabler index derives availability, the target reads its enabled-status. This is the enabler twin
   of the modifier payoff (read effective state, never query upstream) — it eliminates the bottom-up "do I have my
   prereq?" **callback** on both verbs. The pattern repeats per tier: `building.enabledUnits: ["UNIT_AXEMAN"]` mirrors
   `tech.enabledBuildings`. **Three clean tiers, every edge top-down:**
   `base/tech/bonus/improvement --enabledBuildings--> BUILDINGS --enabledUnits--> UNITS (terminal, enable nothing)`.
   Topological load/resolution order is that chain left-to-right.
   **The one upward exception is REMOVED, not honored (owner, 2026-06-13).** `CvTechInfo.PrereqOrBuildings`/
   `PrereqBuildings` — a building→tech *research gate* (a resource-building must exist before the tech can be
   researched) — is the lone arrow that points the wrong way (a building gating a tech). Only 3 techs use it:
   `TECH_LEAD_GLASS`, `TECH_WATERPROOF_CONCRETE`, `TECH_OBSIDIAN_WEAPONS`. The real-life logic (need the resource to
   research) is understandable but costs an entire extra tier + bespoke loader for 3 techs. Decision: **drop the
   `PrereqOrBuildings` block from all three** → the tech researches freely on its normal tech prereq, and the
   resource still gates the *building* it unlocks (normal `bonus→building` build-gate). Removing all three **deletes
   the whole `Prereq(Or)Buildings`-on-tech mechanism** (field + loader) — a success-metric win.
3. **Order is free (additive commutativity).** All channels compound additively — flats sum, percents sum
   (not per-scope compounding). Addition commutes, so the order sources deposit in is IRRELEVANT; siblings can be
   swept in any order, even in parallel. The ONLY ordering is consumer-side and fixed: `(base + Σflat) × (100 +
   Σpercent) / 100`. Adding a source is `+1 deposit`, never a reorder. This is exactly what the hand-maintained
   cache lacked (implicit order + three writers) and the single-recompute model guarantees.
4. **The enabler side has ONE topological order — reuse it everywhere.** Unlike modifiers, enablers
   (dependencies) are stricter: a thing can only be enabled/resolved AFTER its prerequisites, so the enabler DAG
   has a topological order (base → buildings → units, ≈ the existing load order). **Rule 1 guarantees this order
   exists** — no reverse edges → acyclic → a topological sort always exists. Since modifiers are order-indifferent
   (rule 3), adopt the enabler's topological order universally: it satisfies the strict side and costs the loose
   side nothing. **Payoff (with the catch the owner flagged):** the inversions INTRODUCE back-edges — a tech now
   keys buildings (`buildingBoosts`) while a building prereqs the tech, so the *full* FK graph is NOT acyclic; one
   load order can't resolve all of it. It still resolves, by splitting on WHEN a reference is needed: enabler
   PREREQS (needed at load; acyclic by rule 1) resolve immediately in topological load order; the back-references
   (inverted modifier keys, grants like `FreeBuilding`) are needed at deposit/runtime when every type is loaded, so
   they resolve immediately *then* (or via a `getOrCreate` placeholder at load) and never needed load-time
   resolution. So delayed (load-deferred) resolution goes away via the **load-time(prereq) / runtime(modifier-key)
   split**, not a single magic order. **Invariant to PRESERVE:** no load-time-needed reference may sit in a cycle —
   rule 1 keeps the enabler prereqs acyclic, so the load-time set stays sortable; the day a cyclic ref needs
   load-time resolution, delayed resolution is back.
5. **The scope chain ends at City; below it are TARGETS, not more chain.** The containment spine
   (Game→Team→Player→Area→City) is where scope-aggregate effects sum. Below the city, plots / specialists /
   buildings are NOT a linear extension of it — they're **parallel targets within the city**, related by the two
   graphs above, not by nesting: the **enabler DAG** runs base → building → unit (a plot and what sits on it —
   improvement/feature/bonus — enables buildings; buildings enable units; specialists enable nothing); **modifiers**
   reach all of them order-free (specialist→building, building→city, improvement→plot-yield). So the tempting linear
   order "city → specialists → plots → buildings" was explored and **dissolved**: modifiers don't care about order
   (rule 3, commutative), and enablers follow the DAG (base→building), not a city→specialist→plot→building line.
   "All the way down" means the two graphs reach every target — not that the spine grows more links. (A plot is the
   one below-city entity that is itself a mini-scope — it holds an improvement/feature/bonus — and it belongs to the
   Area/map, not nested under the city.)

## Scope discipline — the repository is NOT the cascade plan (owner course-correction 2026-06-13)

The derived-data **repository** (`TLazy` / `CvDerivedData`) is a *separate, pre-existing* caching project
(building-values, constructible sets). The cascade/JSON migration does **not** require it. Clean boundary:
- **Cascade/JSON = the deliverable** — data lives on its owner, in JSON, aggregated by recompute over sources.
  Repository-FREE by default.
- **Repository = an optional, selective perf cache** — added only where a recompute proves hot. Most cascade
  values never touch it.
- The bonus-health/happiness `TLazy` tenants are a **one-off**: they replaced a genuinely hot serialized cache
  *and* fixed DESPAIR_INDEX #11. Keep them, but do NOT replicate "make it a repository tenant" as the pattern.
  Do not let perfecting the cache displace the actual job: **migrate the data (XML → JSON) + wire the cascade.**

## STATUS: owner green-light, Wave 0 starting (2026-06-13)

The 7 owner decisions (blueprint §7) are RESOLVED with the recommended choices:
1. **Improvement/Terrain effects** → KEEP building-side, keyed by improvement/terrain (NOT inverted onto
   Improvement/Terrain), unless a cross-building aggregation win emerges later.
2. **ReligionChanges** → INVERT to `CvReligionInfo` (conditioner rule).
3. **UnitProductionModifiers** (Building→Unit) → KEEP building-side keyed by Unit (conditioning is weak).
4. **RouteYieldChanges** → owned by `CvRouteInfo` (confirmed; static, not cascaded).
5. **Project.VictoryThresholds** → STAY on `CvProjectInfo`.
6. **"Must stay" blessed** — leader personality biases, own-stat combat/movement/invisibility maps, and
   PropertyManipulators-self-reading: the pass must NOT invert or channel-ify these.
7. **CultureLevel.SpeedThresholds** → derived GameSpeed scaling (base × `GameSpeed.cultureThresholdScalePercent`),
   explicit per-speed override only when present.

Executing the [[cross-entity-inversion-blueprint]] wave plan. **Wave 0 = cascade infrastructure** (scoped per-Type
effective-value index + deposit-on-event + recompute-on-load + `[CASCADE/*]` parity logging), built on the existing
derived-data-repository skeleton. The gate: nothing converts until parity logging is green.

## The surface — 236 fields

| kind | count | disposition |
|---|---|---|
| flatScalar | 34 | **Wave 1** — drop-in `flat` channel |
| percentModifier | 49 | **Wave 1** — drop-in `percent` channel |
| enabler | 12 | **Wave 1** — drop-in `enabler` channel |
| indexedFamily | 16 | **Wave 3** — needs `channel → {INDEX: value}` model (per-Yield / per-Commerce) |
| complexMapVector | 30 | **Wave 4** (effect maps) / §5 (prereq vectors = non-modifier) |
| nonModifier | 95 | **out of scope** — stays XML (graphics, cost, prereqs, FKs, build-rule flags) |

So "convert all the buildings" splits cleanly: **~40% mechanical (95 scalars/enablers), ~13% needs the
indexed model (1 design decision), ~47% is not modifier data** (prereqs/graphics/identity — stays put).

## Wave 1 — the 95 drop-in channels (mechanical `tag → path`)

**city/enabler (11):** bNoUnhappiness→city.unhappiness.enabler · bNoUnhealthyPopulation→city.unhealthyPopulation ·
bBuildingOnlyHealthy→city.buildingUnhealth · bNukeImmune→city.nukeImmunity · bNoEnemyPillagingIncome→city.enemyPillageIncome ·
bProvidesFreshWater→city.freshWater · bForceAllTradeRoutes→city.forceAllTradeRoutes · bZoneOfControl→city.zoneOfControl ·
bProtectedCulture→city.protectedCulture · bDamageAllAttackers→city.damageAllAttackers ⚠(derives m_bDamageAttackerCapable —
recompute on load) · bQuarantine→city.quarantine.
**area/enabler (1):** bBorderObstacle→area.borderObstacle.
**city/flatScalar (22):** iGreatPeopleRateChange, iExperience, iAirlift, iAirUnitCapacity, iFreeSpecialist, iHealRateChange,
iHealth, iHappiness, iStateReligionHappiness, iTradeRoutes, iCoastalTradeRoutes, iRevIdxLocal, iLineOfSight, iNumUnitFullHeal,
iNumPopulationEmployed, iLocalDynamicDefense, iRiverDefensePenalty, iMinDefense (verified additive), iDamageAttackerChance,
iDamageToAttacker, iMaxPopulationChange, iPopulationChange.
**area/flatScalar (3):** iAreaFreeSpecialist, iAreaHealth, iAreaHappiness.
**player/flatScalar (8):** iGlobalExperience, iGlobalFreeSpecialist, iGlobalHealth, iGlobalHappiness, iGlobalTradeRoutes,
iGlobalPopulationChange, iFreeTechs, iRevIdxNational.
**team/flatScalar (1):** iWorldTradeRoutes — sole team-scope scalar (see §flag: real team tier vs fold into player.tradeRoutes).
**city/percentModifier (27):** iHurryCostModifier, iHurryAngerModifier, iGreatPeopleRateModifier, iFoodKept, iAirModifier,
iNukeModifier, iMaintenanceModifier, iWarWearinessModifier, iMilitaryProductionModifier, iSpaceProductionModifier,
iTradeRouteModifier, iForeignTradeRouteModifier, iDefense, iBombardDefense, iEspionageDefense, iUnitUpgradePriceModifier,
iRevIdxDistanceModifier, iPillageGoldModifier, iPopulationgrowthratepercentage, iAdjacentDamagePercent, iOccupationTimeModifier,
iHealthPercentPerPopulation, iHappinessPercentPerPopulation, iLocalCaptureProbabilityModifier, iLocalCaptureResistanceModifier,
iBuildingDefenseRecoverySpeedModifier, iCityDefenseRecoverySpeedModifier.
**player/percentModifier (21):** iGreatGeneralRateModifier, iDomesticGreatGeneralRateModifier, iGlobalGreatPeopleRateModifier,
iAnarchyModifier, iGoldenAgeModifier, iGlobalHurryModifier, iGlobalMaintenanceModifier, iOtherAreaMaintenanceModifier,
iDistanceMaintenanceModifier, iNumCitiesMaintenanceModifier, iCoastalDistanceMaintenanceModifier, iConnectedCityMaintenanceModifier,
iGlobalWarWearinessModifier, iEnemyWarWearinessModifier, iWorkerSpeedModifier, iGlobalSpaceProductionModifier, iAllCityDefense,
iInflationModifier, iGlobalPopulationgrowthratepercentage, iNationalCaptureProbabilityModifier, iNationalCaptureResistanceModifier.
**area/percentModifier (1):** iAreaMaintenanceModifier.

Wave-1 risk control: land a `[PERF/reqmodel]`-style parity log (old getter vs new channel read) before deleting any legacy getter.

## Wave 3 — indexed families (16; need `channel → {INDEX: value}`)

**Two kinds of index, one JSON shape (owner, 2026-06-13):** `YieldTypes`/`CommerceTypes` are FIXED hardcoded enums
(FOOD/PRODUCTION/COMMERCE; GOLD/RESEARCH/CULTURE/ESPIONAGE), locked compile-time order — positional array ↔ named
key is a deterministic static mapping, zero fragility. Data-driven keys (Bonus/Tech/Religion/Building/Improvement)
resolve at load via `getInfoTypeForString`. Both render as one keyed-JSON shape and load through ONE keyed reader
so the gameplay-heaviest tags (YieldChanges / CommerceModifiers) are among the *easiest* to convert cleanly, not
the hardest.

**Key naming ruling (owner, 2026-06-13):** the `YieldTypes`/`CommerceTypes` enum is an **EXE-binding detail walled
off in C++ only** (the closed EXE reads enum-indexed yield/commerce arrays as ABI, so the DLL keeps the enum) — the
data/JSON/modder surface never sees it. Yield/commerce are *the only types in the game that will never change in any
capacity* (3 yields, 4 commerces, locked forever), which makes their name↔index table the one mapping safe to
**hardcode permanently**. Therefore:
- Yield/commerce keys are **short, enum-free names** in JSON: `{"food": 2, "production": 1}`, `{"gold": 25}`. No
  `YIELD_` prefix — they are first-class named quantities, the game's universal vocabulary.
- Data-driven keys (Bonus/Tech/Building/Religion/Improvement) use the **full Type string** (`BONUS_COAL`,
  `TECH_POTTERY`), resolved at load via `getInfoTypeForString` — there is no shorter stable identity for them.
This is not an inconsistency: the fixed bedrock enums get clean names; the runtime-resolved Types keep their Type id.

By `YieldTypes` (8): YieldChanges, YieldModifiers, YieldPerPopChanges, RiverPlotYieldChanges, PowerYieldModifiers,
AreaYieldModifiers(area), GlobalYieldModifiers(player), GlobalSeaPlotYieldChanges(player).
By `CommerceTypes` (8): CommerceChanges, CommerceModifiers, CommercePerPopChanges, CommerceHappinesses, SpecialistExtraCommerces,
StateReligionCommerces, GlobalCommerceModifiers(player), CommerceChangeDoubleTimes ⚠(value is a timer, not a magnitude — own treatment).

## Wave 4 — keyed/2D effect maps (the real-modifier subset of the 30 complexMapVector)

BonusHealth/HappinessChanges, TechHappiness/HealthChanges, ReligionChanges, ImprovementFreeSpecialists, ExtraFreeBonuses,
GlobalBuildingExtraCommerces(2D building×commerce), UnitCombatFreeExperiences, UnitCombatExtraStrengths, BuildingHappinessChanges,
Building/GlobalBuildingProductionModifiers, GlobalBuildingCostModifiers, Tech Yield/Commerce maps(2D), TerrainYieldChanges,
Improvement/GlobalImprovementYieldChanges. `(PropertyManipulators)` stays a self-reading sub-object — never a cascade channel.

## FLAGGED for owner (decide before converting — each is a silent data-drop or scope error if guessed)

**False negatives** (inventory said non-modifier, but the committed cascade design DOES cover them — verified in C++):
1. **bPower** → `city.power.enabler` (CvCity.cpp:4656 changePowerCount → getPowerCount>0 presence; doc CASCADEFLAT_PROVIDES_POWER).
2. **iInsidiousness / iInvestigation** → `city.insidiousness.flat` / `city.investigation.flat` (committed CASCADEFLAT_EXTRA_*).
3. **iConquestProb** → `city.conquestProbability.percent` (committed CASCADEMOD_CONQUEST_PROBABILITY).

**True conflict — doc is wrong, not the field:**
4. **iWorkableRadius** — `cascade-modifier-inventory.md` lists `CASCADEFLAT_WORKABLE_RADIUS` (additive), but CvCity.cpp:4864 uses
   `setWorkableRadiusOverride` = REPLACE (last-writer-wins, resets to 0). It must NOT join the additive cascade; **fix the inventory doc.**

**Structural confirms:**
5. **bDamageAllAttackers** derived companion `m_bDamageAttackerCapable` → recompute-on-load, not separately persisted.
6. **iWorldTradeRoutes** team scope → does the bundle get a real `team` tier, or fold into `player.tradeRoutes.flat` at team→player merge?
7. **CommerceChangeDoubleTimes** → timer, not a magnitude; separate handling even inside the indexed model.

## The single gating decision (blocks Waves 3–4, ~half the effect surface)

**Do the indexed/conditional families become indexed rate-table channels on the cascade surface, or stay the consumer's
local indexed effects?** (cascade-inventory §8 OPEN.) Reframing that de-risks it: the **JSON data migration** of these
families is NOT blocked — they can load from the channel-object into their existing arrays now ("JSON reflects the data").
Only their *cascade-surface participation* (cascading across Game→…→City) is the open call, and it only buys anything for the
few genuinely scoped ones (AreaYieldModifiers, Global*Modifiers). Wave 1 is unblocked regardless.

## Order
1. Wave 1 — 95 scalar/enabler drop-ins (mechanical, parity-logged).
2. Wave 2 — reconcile §flag false-negatives (bPower, iInsidiousness, iInvestigation, iConquestProb) + fix iWorkableRadius doc.
3. Wave 3 — indexed families (after the gating decision + the indexed-channel model).
4. Wave 4 — keyed/2D effect maps (most design-heavy).
