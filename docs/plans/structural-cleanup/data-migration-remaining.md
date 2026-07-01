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
- **era**: `bNoGoodies`/`bNoBarbUnits`/`bNoBarbCities` → `identity` (live world-gen gates, no destination model; safe
  only because currently all-zero).
- **tech**: `TechMovementChanges`/`TechSpecialistChanges` inverted onto tech where no consumer reads (non-functional).
- **corporation**: `iSpread`/`iSpreadFactor`/`CompetingCorporations` → `identity`.
- **improvement**: `iAirBombDefense`/`iFeatureGrowth`/`iCultureRange`/per-bonus `depletionRand` → `identity`.
- **project**: `AnyonePrereqProject` dropped, per-edge `iNeeded` count lost.
- **unit**: `iCargo` → **`cargo.space.flat`** (+ `DomainCargo`→`cargo.space.{unit:IS_<domain>}`, modifier §6) —
  owner-RULED 2026-07-01, **DO-NOW** (currently UNHANDLED on 90 units). `EnabledCivilizationTypes` is **NOT** the
  unique-unit system (that's UnitClass/CivilizationInfo) — the train gate fires ONLY under
  `isNPC() && isStronglyRestricted()` (`CvCity.cpp:2231`), inert for real civs — so it folds with `stronglyRestricted`
  (Tier 3, → `requires.build` when NPC civs wired); stays `identity` interim. cargo restriction → `identity`.
- **promotion**: `iCelebrityHappy` → `identity` (real happiness mod); `iPoisonProbabilityModifierChange` inert (kept).
- **handicap**: `advancedStart` (`iAdvancedStartPointsMod`, `iAIAdvancedStartPercent`) → `identity`, no consumer wired.

---

## Tier 3 — BLOCKED (needs a prerequisite / decision first)

- **`paralyze`** → `state` block — the `state` block SHAPE is greenfield/undefined (state.md). Left as-is (inert; no
  data lost, no structure moved — owner 2026-07-01). Unblocks when the `state` model is designed.
- **`mechanized`/`gunpowder`/`mounted` tags** — derived from unitcombats in the post-migration **unitcombat→tag pass**
  (`curate_unitcombat.py` emits no tags yet).
- **`stronglyRestricted`** (NPC build-lockdown) → a `requires.build` civ-membership gate (paired with
  `EnabledCivilization`) — deferred until **NPC civilizations are wired**.
- **Property pulses** (legacy `properties` array → `grants.repeatable{on,relation,distance}`) — BLOCKED on the
  **#429 spatial rework** (`engine.property_source_v3` currently *raises* on `RELATION_NEAR`).
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
