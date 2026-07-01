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
- **Already-verified DONE** (curator code confirms): `bSpy`→`spy` tag · `freeSpecialistPer*Wonder`→`freeSpecialists`
  · `EraCommerceChanges`→ERA-threshold flats · corp `iMaintenance` de-scale · `GlobalBuildingExtraCommerces`→
  `empire.buildings.{B}.flat` · trait `nonStateReligionCommerce` stays a policy.

---

## Tier 2 — DECISION-NEEDED (owner ruling: is-it-live? / where-does-it-go?)

These are real gameplay values the curators **parked in `identity` or dropped** on a re-verify claim. Each needs an
owner call before a curator edit — per [DEC-no-guessing] the agent must not decide "dead" or invent a home.

### Confirmed real losses (completeness sweep, verified against live C++)

| # | Field (curator) | Live C++ ground truth | Proposed home |
|---|---|---|---|
| D1 | **`iMaxNationalWondersOCC`** (`curate_culturelevel`) | `CvCity.cpp:2172` `getMaxNumWonders`: under `GAMEOPTION_CHALLENGE_ONE_CITY` swaps the national cap to a **doubled** value (2× across all 19 levels). Curator rationale "OCC forces limits off" is FALSE. | an OCC variant in the `allowed` category-cap block (json §4.4) — a per-game-option override of `nationalWonders`. Needs the shape agreed. |
| D2 | **`iOperationalRange{Min,Max}`** (`curate_property`) | `CvCityAI.cpp:14789-14817,14962` — live per-turn AI: normalizes a property value into this band to score decisions. Mis-swept into the `#429` drop (it's the AI value band, NOT propagation). | carry as a per-property `operationalRange:{min,max}` (sibling to `targetLevel`), OR confirm the AI band is deliberately retired with #429. C++ says it is **not** dead. |

### Real data parked in `identity` / dropped (from the curator-code audit — need is-it-live + home)

- **building**: ~30 capability bools (`nukeImmune`, `zoneOfControl`, `borderObstacle`, `governmentCenter`,
  `providesFreshWater`, …) → `identity` "revisit Phase F"; commerce markers (`stateReligionCommerce`,
  `commerceDoubleTime`, `shrine`/GlobalReligionCommerce, `commerceFlexible`, `corporationHQ`,
  `damageAttackingUnitCombats`) → `identity`; **`BonusAidModifiers`/`AidRateChanges` DROPPED** (real C2C "aid" mechanic).
- **leaderhead**: simple→complex trait mirroring deferred — **117 leaders get NO `complexTraits` under the complex
  option until this lands.** (Largest single gap.)
- **era**: `bNoGoodies`/`bNoBarbUnits`/`bNoBarbCities` → `identity` (live world-gen gates, no destination model; safe
  only because currently all-zero).
- **tech**: `TechMovementChanges`/`TechSpecialistChanges` inverted onto tech where no consumer reads (non-functional).
- **corporation**: `iSpread`/`iSpreadFactor`/`CompetingCorporations` → `identity`.
- **improvement**: `iAirBombDefense`/`iFeatureGrowth`/`iCultureRange`/per-bonus `depletionRand` → `identity`.
- **project**: `AnyonePrereqProject` dropped, per-edge `iNeeded` count lost.
- **unit**: `EnabledCivilizationTypes` → `identity` (ignored by train gate); `iCargo` still **UNHANDLED on 90 units**
  → `cargo.space.flat` (+ `DomainCargo`→`cargo.space.{unit:IS_<domain>}`), part of the unit-stat family (modifier §6);
  cargo restriction → `identity`.
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
