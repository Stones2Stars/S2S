# Data-migration remaining — the curator/JSON worklist

> **The #1-priority tier ([DEC-data-first](../../architecture/decisions.md#dec-data-first)).** Every item here is
> curator/JSON data work that must be finished **before** downstream cascade/shadow/observability work — a deferred
> data item forces every consumer to ASSUME its eventual shape. This doc is the consolidated output of the
> 2026-07-01 audit (the full spec surface read + the authoritative curator-code audit + two adversarial completeness
> sweeps). It is **migration-transient** — deleted when the migration completes.

## Method / completeness attestation

Audited: the whole `docs/specs/` surface (read in full) + the curator code (`Tools/Migration/curate_*.py`) + two
completeness sweeps for hidden/lost data:

- **Curator inventory** — 34 curators, one per `✅` naming.md infotype, all output folders populated. No orphan gaps.
- **Silent identity-sink sweep** (the 8 thin `cc.main` curators — `curate_common.py:413-416` routes unrecognized
  tags into `identity` with no report): **CLEAN — 0 parked gameplay data.** Every sink field is a documented,
  deliberate `identity`/`mapGeneration`/deferred-subsystem disposition per json.md §7.
- **Whitelist silent-drop sweep** (the 9 no-report, no-catch-all curators, where an unread field vanishes without
  trace): **2 real losses found** (culturelevel, property — below); the other 7 drop nothing.

So the "hidden data" question is closed: the only un-migrated data is the enumerated list below.

> **STATUS — the DECISION-NEEDED (🔴) tier is CLEARED (owner rulings, 2026-07-01).** Every parked/dropped field across
> every entity has been ruled and migrated (buildings, leaderhead, corp/route/tech, property-pulses, improvement,
> project, era, handicap, promotion/celebrity, unit-cargo). What remains is only the **BLOCKED/deferred** tier below
> (prerequisite-gated: state/paralyze, unitcombat→tags, NPC civs, corp-system rework, ranked-target, leaderhead trait
> remap, the unit **`missions`**/`CvOutcome` migration [grants-pass discovery]) + the **post-migration engine follow-ups**
> (e.g. the celebrity-skill CvCity scan, the `enables.traits`→HAVE self-containment step from the grants pass — the
> `IS_HOLY_CITY` eval is already wired). Per [DEC-data-first] the
> data foundation is now complete — the machine backlog can proceed on solid data.

---

## Tier 1 — DONE (committed)

- **Trap family** (11 tags) → `DROP` in `curate_promotion.py` — dead mechanic (traps removed). Zero data delta (no
  live promotion XML carries trap values). Commit `b9c2804c0`.
- **`military{Happiness,Production,Support}`** dropped from `curate_unit.py` `CAP_BOOL` — reclassified to the
  `IS_MILITARY` predicate/tag (json §3.5); `militaryTrade` kept; `bMilitarySupport`'s tag-signal read (`:655`)
  preserved. 1352 units regenerated. Commit `b9c2804c0`.
- **`BUILDING_PALACE`** dropped from ~48 civilizations' `grants.buildings` — redundant with the settler's
  `foundBuildings` (json §5). Commit `b9c2804c0`.
- **`iOperationalRange{Min,Max}`** (property) → the **`ai` block** (`ai.operationalRange:{min,max}`) — AI-only
  decision-scoring band (`CvCityAI`), NOT the #429 propagation drop (owner 2026-07-01). 7 property JSONs regenerated.
- **`iMaxNationalWondersOCC`** (culturelevel) → justified **DROP** — One City Challenge is not feasible in this mod
  (owner 2026-07-01); the mislabeled curator rationale ("OCC forces limits off") corrected (comment-only, no data change).
- **Already-verified DONE** (curator code confirms): `bSpy`→`spy` tag · `freeSpecialistPer*Wonder`→`freeSpecialists`
  · `EraCommerceChanges`→ERA-threshold flats · corp `iMaintenance` de-scale · `GlobalBuildingExtraCommerces`→
  `empire.buildings.{B}.flat` · trait `nonStateReligionCommerce` stays a policy.
- **`grants` classification pass (2026-07-01)** — the survey found `grants` was a **34-key grab-bag** (~half off-grammar).
  Re-homed the mis-classified keys to their real blocks: promotion `unitCombats`/`removesUnitCombats`→**skills**; project
  `grantsSpecialBuilding`→**`enables.specialBuildings`** (flips SpecialBuildingValid — unlocks, hands out nothing); corp
  `bonusProduced`→**`provides.bonuses`** (continuous supply §5a); unit `builds`→a dedicated **`builds`** block (readJson
  `CJK_INTRINSIC_KEYS` + `CvJsonUnitInfo` parse); building `holyCity`→**`requires.build`** (`IS_HOLY_CITY` predicate, a
  build gate not a setter — verified `CvCity.cpp:2728`) + `traits`→**`enables.traits`** (held-trait — `owner.setHasTrait`
  while active); `freePromotions`→**`repeatable`**. All 0-stale, new-home counts == originals; json.md §5/§8 updated;
  Assert green. **Consumer-wiring:** the enabler `IS_HOLY_CITY` eval is ALREADY wired (verified — parser
  `CASC_PRED_IS_HOLY_CITY` + evaluator `ev_evalPredicate` `CvCascadeConditionEval.cpp:246`), so the `holyCity` gate
  works out of the box. The one remaining follow-up is the self-contained **`enables.traits`→empire-active-trait HAVE**
  computation (today the modifier reads engine `hasTrait`, which already includes the building's `setHasTrait`, so the
  effect flows; the cascade-computed active-trait set is a cutover self-containment step, not a data gap).
- **⏳ DEFERRED (grants-pass discovery) — unit `missions` + the `CvOutcome` system.** The grants pass found the unit
  *activated-mission* keys — `buildings` (MISSION_CONSTRUCT), `greatPersonAction`, `goldenAge` — are **missions**, NOT
  grants (a `skill` is a permanent property; a **mission** is an action producing an OUTCOME, often consuming the unit),
  and the engine's **`CvOutcome`** system (`CvUnitInfo` `KillOutcomes` + `m_aOutcomeMissions` — *"outcome system (no
  wrapper)"*) is **entirely un-migrated**. Owner ruling: a **`missions`** block (json §8) unifies the hardcoded mission-
  abilities AND CvOutcome. **Owner ruling (updated 2026-07-01): the CvOutcome outcome model is NOT ported** — the
  outcomes stay in the OLD XML and legacy keeps applying them (too gnarly to port); the whole subsystem, **like the
  random-events system**, is **isolated enough to leave for a CLEAN PASS AFTER #430**. When it runs, the `missions`
  block just **LISTS which missions a unit can use** — BOTH the data-driven outcome-missions (`<Actions>`) and the
  hardcoded mission-abilities (`greatPeople` included); the `greatPersonAction` `base`/`multiplier` magnitudes are
  **left as-is** (used-or-not TBD). Until the pass runs, the four deferred keys stay in `grants` untouched (the grants
  machine ignores them). Behaviour reference: [`../../reference/mission-outcome-system.md`](../../reference/mission-outcome-system.md)
  — the earlier "7-question full-port" design there is **SCRAPPED**.

---

## Tier 2 — DECISION-NEEDED (owner ruling: is-it-live? / where-does-it-go?)

These are real gameplay values the curators **parked in `identity` or dropped** on a re-verify claim. Each needs an
owner call before a curator edit — per [DEC-no-guessing] the agent must not decide "dead" or invent a home.

### Confirmed real losses — RESOLVED (owner 2026-07-01, see Tier 1)

The whitelist completeness sweep found exactly two, both now ruled and landed:
- **`iMaxNationalWondersOCC`** → DROP (OCC not feasible in this mod).
- **`iOperationalRange{Min,Max}`** → `ai.operationalRange:{min,max}` (AI-only).

### Real data parked in `identity` / dropped (from the curator-code audit — need is-it-live + home)

- **building — DONE (owner rulings 2026-07-01, executed):**
  - NEW **`attributes`** block (json §8) — building-**HELD** city-scope capability bools (16): `nukeImmune`,
    `borderObstacle`, `protectedCulture`, `noUnhappiness`, `noUnhealthyPopulation`, `buildingOnlyHealthy`,
    `forceAllTradeRoutes`, `quarantine`, `mapCentering`, `teamShare`, `orbital`, `orbitalInfrastructure`,
    `governmentCenter`, `capital`, `zoneOfControl`, `providesFreshWater` (fresh water is NOT a `BONUS_`, so NOT `provides`).
  - `noHolyCity` → `requires.build.disabled: IS_HOLY_CITY` (a placement predicate, not an attribute).
  - `applyFreePromotionOnMove` → `grants` pulse (folds with the building's `FreePromotions`: a unit that stays in the
    city gets the promotion).
  - `commerceFlexible` → **`capabilities` PROVIDED to the empire**, as discrete booleans `setCultureRate`/
    `setEspionageRate` (and `setScienceRate`, which rides `TECH_GAME_START` — every civ has it). **Capabilities are
    empire-HELD, grantor-PROVIDED** (tech/civic/**building**); a building **provides**, never **holds** — the opposite of
    `attributes` (which the building holds). json §8 to state this distinction.
  - `shrine` (GlobalReligionCommerce) → promote to the top-level **`shrine`** bespoke section (json §9 reserves it).
  - `corporationHQ` (GlobalCorporationCommerce) → NEW **`headquarters`** bespoke section (mirror of `shrine`).
  - counter-damage (`bDamageAllAttackers` + `damageAttackingUnitCombats`) → fold into the **`defense`** family with the
    already-migrated `iDamageToAttacker`/`iDamageAttackerChance` (one mechanic, one home).
  - KEEP as cascade markers in identity: `stateReligionCommerce`, `commerceDoubleTime` (a flat family provably can't
    model the pool×count / whole-commerce doubling — documented + cascade-read).
  - STAY in identity (buildability/placement, json §7): `autoBuild`, `noInstanceLimit`, `forceNoPrereqScaling`, `centerInCity`.
  - DROP (confirmed DEAD): the aid mechanic (`BonusAidModifiers`/`AidRateChanges` — city arrays saved but ZERO
    write-from-building + ZERO read-for-effect; only AI-valuation/pedia read the raw Info) + the `DROP_DEAD`/`DROP_MODULE` set.
  - `EnabledCivilizationTypes` → identity interim (→ `requires.build` when NPC civs wired); `bAllowsNukes` → `requires.build.disabled` (done).
- **leaderhead — DONE (owner ruling 2026-07-01): ALL traits stripped.** Every leader trait assignment (`Traits`,
  `DefaultTraits`, `DefaultComplexTraits` — simple AND complex) is dropped from the JSON; **no leader carries traits**.
  The leader↔trait mapping (incl. simple→complex) goes to a dedicated POST-MIGRATION pass (which re-adds a
  `grants.traits` emit). Safe pre-cutover (the game runs traits off XML meanwhile). 118 leaderheads regenerated.
- **era — DONE (owner ruling 2026-07-01):** `bNoGoodies`/`bNoBarbUnits`/`bNoBarbCities` → a bespoke **`worldGen`** block
  (LIVE C++ world-RULE gates: goody/barb placement — "bespoke worldgen works better", not identity/modifiers). All-false
  in every era today → mapping migrated, 0 output change (zero-drop).
- **handicap — DONE (owner ruling 2026-07-01):** `advancedStart` (`iAdvancedStartPointsMod`, `iAIAdvancedStartPercent`)
  **stays `identity`** (owner: "can stay where it is") — pre-game points config, no consumer wired, handicap-intrinsic.
- **tech**: `TechMovementChanges`/`TechSpecialistChanges` inverted onto tech where no consumer reads (non-functional).
- **corporation** (owner rulings 2026-07-01): `iSpread`→`identity.spreadFactor` (concept-parallel to religion's
  spread), `iSpreadFactor`→`identity.competingSpreadCostPercent` (fix the misnomer — it's a competing-corp spread-cost
  inflation, `CvUnit.cpp:8687`), `iSpreadCost`→`cost.spread`, `CompetingCorporations`→**`excludes`** (json §9 same-tier
  mutual exclusion; empty in base XML, so the MAPPING migrates but shipped output is unchanged) — all executing.
  **NB: the corporation SYSTEM deserves a principle-level rework later** (owner 2026-07-01: "don't like how corporations
  work in principle") — that is POST-migration ([DEC-mirror-then-redesign]: migrate faithfully now, redesign the corp
  model after); the corp-HQ revenue (`HeadquarterCommerces`) rides that rework.
- **improvement — DONE (owner rulings 2026-07-01):** `iAirBombDefense` → **`defense.plot.air.flat`** (101 improvements;
  the air-bomb defense magnitude, `CvUnit.cpp:7127`). `iFeatureGrowth` / `iCultureRange` / per-bonus `depletionRand` →
  **STAY in `identity`** (owner: "leave them in identity") — improvement-INTRINSIC mechanics read by their own `CvPlot`
  systems (feature-regrowth / culture-seed / depletion), NOT cascade modifiers, so identity is the correct home.
  (`cultureRange` verified: the real culture-spread was nuked ~4yr ago — `pushCultureFromImprovement` is now only a
  cosmetic 1-culture placement seed, `CvPlot.cpp:4062`; the field is a live-but-vestigial remnant.)
- **project — DONE (owner ruling 2026-07-01):** `AnyonePrereqProject` → **`requires.build: {type, scope:"world"}`** —
  a single project that ANY player must have built (world-scope presence, `CvPlayer.cpp:6868` blocks when world
  `getProjectCreatedCount==0`; NOT the `.any` combinator — it's one project). Empty in base XML → mapping migrated,
  0 output change. `PrereqProjects` (the ALL case) already modeled via store→`enables` inversion; per-edge `iNeeded`
  is all-`1` today (a `count>1` would need a count-bearing edge — flagged, not lost).
- **unit**: `iCargo` → **`cargo.space.flat`** (+ `DomainCargo`→`cargo.space.{unit:IS_<domain>}`, modifier §6) —
  owner-RULED 2026-07-01, **DO-NOW** (currently UNHANDLED on 90 units). `EnabledCivilizationTypes` is **NOT** the
  unique-unit system (that's UnitClass/CivilizationInfo) — the train gate fires ONLY under
  `isNPC() && isStronglyRestricted()` (`CvCity.cpp:2231`), inert for real civs — so it folds with `stronglyRestricted`
  (Tier 3, → `requires.build` when NPC civs wired); stays `identity` interim. cargo restriction → `identity`.
- **promotion — DONE (owner ruling 2026-07-01):** `iCelebrityHappy` (the numeric per-unit celebrity-happiness stat) →
  a boolean **`skills.celebrity`** (3 promotions: INSPIRE3/6/9; unit-combats carry the same field → `skills.celebrity`
  too, 0 today). The AMOUNT is dropped ("not a random field on a unit"); ⏳ **POST-MIGRATION engine fix:** `CvCity`
  scans for celebrity-skilled units and owns the happiness (replacing the `CvCity.cpp:5718` per-unit-stat sum).
  `iPoisonProbabilityModifierChange` inert (kept).
- **handicap**: `advancedStart` (`iAdvancedStartPointsMod`, `iAIAdvancedStartPercent`) → `identity`, no consumer wired.

---

## Tier 3 — BLOCKED (needs a prerequisite / decision first)

- **`paralyze`** → `state` block — the `state` block SHAPE is greenfield/undefined (state.md). Left as-is (inert; no
  data lost, no structure moved — owner 2026-07-01). Unblocks when the `state` model is designed.
- **`mechanized`/`gunpowder`/`mounted` tags** — derived from unitcombats in the post-migration **unitcombat→tag pass**
  (`curate_unitcombat.py` emits no tags yet).
- **`stronglyRestricted`** (NPC build-lockdown) → a `requires.build` civ-membership gate (paired with
  `EnabledCivilization`) — deferred until **NPC civilizations are wired**.
- **Property pulses = repeatable grants (owner ruling 2026-07-01) — DATA is DO-NOW, NOT #429-blocked.** A per-turn
  `PROPERTY_*` pulse an entity emits → a `grants.repeatable` entry carrying its spatial intent
  (`{ PROPERTY_X: N, interval, on, relation, distance }`, json §5). Capturing the pulse + spatial fields is pure DATA
  and is unblocked. Work: a shared **property-source cleaner** — fix `engine.property_source_v3` to EMIT spatial
  sources as `grants.repeatable` instead of *raising* on `RELATION_NEAR`, and route the improvement/feature/building
  property arrays through it (replacing the verbatim `properties` parking). **#429 is ONLY the ENGINE
  spatial-distribution** that later reads `on`/`relation`/`distance` — a separate consumer, not a data blocker.
- **Corp HQ revenue** (`HeadquarterCommerces`) → the corp-rework pass.
- **`ranked-target-selection`** (`max:`/`orderedBy`) — design locked, impl pending; blocks retiring `largestCity`
  (so `curate_civic.py`/`curate_trait.py` still emit `iLargestCityHappiness`). See `../parked/ranked-target-selection.md`.

---

## Tier 4 — VERIFY (promise-based cross-curator holes)

- `curate_bonus` actually inverts civic `BonusCommerceModifiers` (the civic curator drops it on that promise).
- The yield resolver reads `identity.movementCost` (feature/terrain `iMovement` → `identity.movementCost`).
- `PropertyPropagator`/`ChangePropagators` re-home at the unit/building passes actually happens (else lost with #429).
- PropertyBuilding `iMinValue`/`iMaxValue` are consumed by the Building pass (the building-side `requires` value-band).
- Every bespoke PASS2 tag has live emit code (not just set-membership) — esp. the building PASS2 audit.
- Stale docs: `Tools/Migration/README.md:86-88` references a non-existent `curate_pocos.py`; `curate_building.py:22-24`
  docstring says PASS2 tags "show as UNHANDLED" but they're mostly implemented.

---

## Downstream (explicitly BENEATH this tier, per DEC-data-first)

The machine backlog does not start until the data tier is closed: substrate rebuild → full modifier port
(AFTER/BASE/assembler/commerce) → **A5 wire `skills`/`tags`/`capabilities`/`policies` onto the game object** ("SUPER
IMPORTANT, easily LOST") → enabler parity pass → grants machine → trait simple/complex engine fix → atomic cutover +
§4 deletions. Plus the observability dump gaps. Tracked in `cascade-engine-430.md` / `modifier-machine.md`.
