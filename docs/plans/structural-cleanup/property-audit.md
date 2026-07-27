# Property source-data migration — LOCKED SPEC (owner-approved 2026-07-11)

> **Mandate:** the property *engine* (decay math, spatial-diffusion math, the turn-solver `CvPropertySolver`) is
> intact and **must NOT be rewritten** (owner, standing). The bug: the property **SOURCE DATA** was wrongly stubbed
> empty in every JSON poco (a prior agent conflated "defer the engine rework" with "defer the data migration" — the
> owner has **NEVER** wanted the data deferred). Feed the legacy engine its authored source data from JSON. It only
> surfaced now because the JSON-load crash (fixed `8f80100ea`) previously stopped the game before a turn ran.
> Symptom: `+264 PROPERTY_CRIME`, `-198 PROPERTY_EDUCATION`/turn in Canterbury (crime never decays, buildings never
> cut it); commerce craters **downstream** (education→science, crime→happiness/maintenance), not an independent bug.
>
> **Owner decisions (2026-07-11):** `CvCascadeProperty.cpp` is diagnostic-only (`CvHttpServer.cpp:1778`) — NOT the
> fix, do not extend.
>
> **DIFFUSION IS KEPT (owner 2026-07-11, final):** the owner briefly considered dropping property spread, then ruled
> *"it seems to be fairly ingrained in how properties work, so may as well keep it."* So the earlier `#429` drop in
> `curate_property.py` is **overridden** — the curator now emits the diffuse propagators + `changePropagation` into the
> approved `properties` block, and `CvPropertyInfo` reads them into `CvPropertyPropagatorDiffuse` + the change-prop
> table. (Plot-scope sources have "no real purpose at this time" but ride along — they cost nothing.)
>
> **Build order (owner "curator next"):** (1) ✅ the city-scope C++ bridge — building/unit flats + decay + population
> baseline (already-curated data, DONE, compiling: increments 1+2). (2) ✅ curator emits `properties.diffuse[]` /
> `changePropagation[]`. (3) ✅ `CvPropertyInfo` reads them. (4) ✅ the BoolExpr translator
> (`Cascade/CvCascadePropertyBridge` — conditioned building/unit flats incl. the 78 tech-gated education entries,
> the IS_OWNED diffuse gates, the per-POPULATION `each>1` IntExpr amounts, the changePropagation table; verified
> live: ANCIENT_CUSTOMS = exactly its 3 authored sources, folklore = their 2 gated education sources).
> (5) ✅ the all-cities gather (revived by the one-shot ruling below): `PROPERTY_X.empire.flat` → the building's
> all-cities manipulator container → the load-built `GC.getAllCitiesManipBuildings` index → the count-scaled
> `CvGameObjectCity::foreachManipulator` walk; observable at load via the yields payload's
> `allCitiesManipBuildings` root map + per-building `propertyManipEmpire`. (6) validate — the turn-level pass
> (per-turn `PROPERTY_*` deltas attributed, education/crime normalise) runs on the next played turns.
>
> **⚖ THE ONE-SHOT RULING (owner 2026-07-16 — ruled earlier but never written down, twice, so it was "re-found"
> a third time): the legacy one-shot `<Properties>`/`<PropertiesAllCities>` semantic is DEAD — EVERY such value
> RE-CLASSIFIES as a PER-TURN source. No exceptions: flammability converts too and gets REBALANCED later
> (accepted — its values will climb until the rebalance; the mechanic is dormant today). The rebalance's known
> shape (owner): flammability's problem is NO early-game counters — every reducer is late (fire code / smoke
> detector / fire service) — so counter-values get added to a few early entities (a data/balance pass).**
> Why: the ORIGINAL property design made all pollution-class `<Properties>` one-shots, but building designers
> after the original design authored against the same block ASSUMING per-turn — the shipped XML is mixed-intent
> data sharing one shape, and the one-shot semantic "makes no sense whatsoever". A sanctioned intentional
> divergence from legacy behaviour — the DECIDED model, needing no carve-out: the spec leads, so conforming to it
> is the default and legacy behaviour is not a thing to preserve for its own sake.
> Consequences:
> - The curator's fold of `<Properties>` (city) into the `PROPERTY_X.city.flat` families **IS the decided model**
>   (`curate_building.py` — NO curator change), and the bridge feeding them to the per-turn solver is CORRECT —
>   incl. the 353 flammability adders, GERM_TRAPS' disease +25, GARDENS_BY_THE_BAY's air −50.
> - The engine's one-shot held path (`processBuilding` add/subtract via `getProperties()`/
>   `getPropertiesAllCities()`, `CvCity.cpp:4717`) stays STUB-FED — now correct by ruling: nothing is held.
> - **Increment 5 REVIVES for the 6 `<PropertiesAllCities>` entries** (curated `PROPERTY_X.empire.flat` — the
>   fire services etc., now per-turn in EVERY city of the owner): the bridge reads `empire`-scope families into
>   an all-cities manipulator container on the building, and `CvGameObjectCity::foreachManipulator` additionally
>   walks the owning player's buildings for those (count-scaled — one gather per instance, mirroring the legacy
>   per-instance add). NO player-scope manipulator source exists in the XML (census: DEFAULT 2210 / CITY 79 /
>   PLOT 72), so this gather serves exactly these converted one-shots.
>
> **⚠ FOUND (mapFrom idempotency): the increment-1/2 bridges were append-only** — the aliased full-registry
> `mapFrom` re-run duplicated every property source ~3× (live-verified: ANCIENT_CUSTOMS 9 sources for 3 authored).
> Fixed: `CvPropertyManipulators::clear()` + clear-and-refill at the top of each bridge walk (the CvInfo.h
> contract).
>
> **✅ THE CARRIER BRIDGES (owner 2026-07-16: "stubbing is straight up not allowed"):** every legacy
> property-source carrier delivers from its curated JSON through the ONE shared walk
> (`CascadePropertyBridge::bridgeFamilies` / `bridgePulses`), mirroring each category's legacy delivery shape:
> civics/traits/heritages CITY+RELATION_ASSOCIATED (player gather → every owner city; heritage's legacy XML
> carried NO GameObjectType, so legacy deposited into the unread PLAYER property bag — the curated `city` scope
> is the delivered intent), specialists/promotions CITY|PLOT+RELATION_SAME_PLOT, buildings NO_RELATION (+ the
> empire all-cities container), units SAME_PLOT, feature/improvement `grants.repeatable` pulses PLOT+NEAR (the
> json §5 shape stays the authored home; the bridge feeds the KEEP-legacy solver until the F3/#429 rework).
> HANDICAPS are still legacy-XML-loaded (not a replaced type) — already delivering, no bridge.
> Load-verified via the yields payload's `propertySourceCensus`: civics 39/63, heritages 22/44, specialists
> 16/30, features 27/31, improvements 18/18, traits 171/252 (active complex set) — each exactly the authored
> count, no duplication. ⚠ FOLLOW-UP: promotions census 106 infos/202 sources vs the XML's 74 manipulator
> blocks/124 sources — the curated families fold MORE than the manipulator blocks (curate_promotion.py is the
> map); attribute the delta to its named curator rule.

## The five legacy source channels (all stub-empty today)

1. Property's **own** decay + population baseline + spatial diffusion — `CvPropertyInfo::m_PropertyManipulators`.
2. Property's **change-propagation** table (City→Player rollup) — `CvPropertyInfo::getChangePropagator` (`CvPropertyInfo.h:31`).
3. **Building** flat/conditioned deposits — `CvBuildingInfo::m_PropertyManipulators` (`CvBuildingInfo.h:611`).
4. **Unit** flat deposits (emit to the unit's city+plot) — `CvUnitInfo::m_PropertyManipulators` (`CvUnitInfo.h:546`).
5. Corporation deposits — `CvCorporationInfo::m_PropertyManipulators`. **Zero real data anywhere** (XML or JSON) — a
   stub matching an empty legacy reality; leave stubbed, nothing to migrate.

## Legacy target classes (READ-ONLY — never touch the solving math)

- `CvPropertySource.h` + `.cpp`: `CvPropertySourceConstant` (`IntExpr* m_pAmountPerTurn` — the FLAT kind),
  `CvPropertySourceDecay` (`iPercent` self-decay toward `iNoDecayAmount`), `CvPropertySourceAttributeConstant`
  (`ATTRIBUTE_POPULATION × iAmountPerTurn` — the population baseline). `CvPropertySourceConstantLimited` = **dead**
  (0 real data).
- `CvPropertyPropagator.h`+`.cpp`: `CvPropertyPropagatorDiffuse` (`.cpp:420-543`) — the ONLY live propagator (equalizes
  a % of source↔target difference/turn). `Spread`/`Gather` = **dead** (0 real data).
- `CvPropertyInteraction.*` = **dead** (0 real data — `PropertyInteractionType` is schema-only).
- `CvPropertyManipulators.h`+`.cpp`: the container (`vector<CvPropertySource*/Interaction*/Propagator*>`, `addSource`
  factory `.cpp:58-79`). **Only an XML `read()` populates it — NO programmatic construction path exists.**
- `CvPropertySolver.*`: the authoritative integrator, `CvGame.cpp:6008` calls `doTurn()` once/turn. Untouched.
- **Manipulator gather roster** — `CvGameObject.cpp:626-747`: City walks its OWN `getHasBuildings()` (`:666`),
  present religions/corps/specialists; Unit walks its UnitInfo + promotions; Plot walks terrain/feature/improvement/
  route/bonus; Player walks civics/stateReligion/traits/heritage/handicap. ⚠ **No path rolls an `empire`-scope
  building deposit to every city** (owner-decision #3 below).

## What the JSON already carries (curated — bridge only, no shape change)

- **Property own decay + population baseline** in `Assets/Data/properties/*.json` (7 files): decay = `PROPERTY_X.city.percent`
  / `.plot.percent`; population baseline = `PROPERTY_X.city.flat:{value,per:{type:POPULATION,each}}`. Matches XML exactly.
- **Building/unit flat deposits**: ordinary `PROPERTY_*` modifier families — `PROPERTY_X.city.flat` / `.plot.flat` —
  on ~250 building + 114 unit files. Verified exact vs XML (`building_3d_printing_mill` air-pollution +2;
  `unit_police_dog` crime city −5/plot −3 = the implicit unit `RELATION_SAME_PLOT`).
- **Genuine authoring gaps** (need curator + the new block): spatial **diffusion** (nothing in JSON), the one
  **changePropagation** (`PROPERTY_FLAMMABILITY` City→Player 100%), and the `properties`-block gate predicates.

## APPROVED JSON shape — the `properties` bespoke section (json.md §9 reserves the name)

On the property's own entity (`Assets/Data/properties/<X>.json`):

```jsonc
"properties": {
  "diffuse": [
    { "from": "city", "to": "plots", "relation": "near", "distance": 1, "percent": 5 },
    { "from": "plot", "to": "city", "relation": "samePlot",              "percent": 10 },
    { "from": "plot", "to": "plots", "relation": "near", "distance": 1, "percent": 4, "enabled": "IS_OWNED" }
  ],
  "changePropagation": [ { "from": "city", "to": "empire", "percent": 100 } ]  // FLAMMABILITY only
}
```

- `from`/`to` = the two objects (diffuse is two-object, unlike a `grants` pulse); `relation`+`distance` mirror the
  `grants` property-pulse vocabulary (§5). `enabled` = an ordinary condition (§3.4/§3.5).
- The legacy `PLOT→PLOT` `Active` tag-BoolExpr gates map onto EXISTING json.md predicates (no new predicates):
  `TAG_OWNED`→`IS_OWNED`/vicinity-owned, `TAG_PEAK`→`HAS_PEAK`, `TAG_WATER`+`TAG_CITY`→`{all:["IS_WATER",…]}`.
  Curator applies a fixed translation table.

## Implementation (one landing)

### A. Engine — one small ADDITIVE method (no solver/read/math change)

`CvPropertyManipulators` (+ `CvPropertySource`/`CvPropertyPropagator`): add a **programmatic constructor path** —
e.g. `addConstantSource(PropertyTypes, int iAmount, GameObjectTypes=NO_GAMEOBJECT, RelationTypes=NO_RELATION, int
iData=0, const BoolExpr* =NULL)`, `addDecaySource(...)`, `addAttributeConstantSource(...)`, `addDiffusePropagator(...)`
— mirroring the `addSource(PropertySourceTypes)` factory (`.cpp:58-79`) but building the object graph directly. This is
the ONLY engine touch (plus B3, C3 below). Does not touch `read()`, `CvPropertySolver`, or any predict/correct math.

### B. Pocos — `mapFrom` bridge (after `CvInfo::mapFrom` has parsed `m_modifiers`)

1. **Building** (`CvBuildingInfo.cpp:336`-ish, mirror the river/plot-type array bridge at `:71-90`) + **Unit**
   (`CvUnitInfo.cpp:336`): walk `getModifiers()->entries()`; for each `PROPERTY_*.{city|plot}.flat` `CvModEntry`,
   `jsonResolveId` the property, then:
   - plain (`!hasPer && !enabled && !disabled`) → `addConstantSource(prop, value100/100, [unit: GAMEOBJECT_CITY|PLOT +
     RELATION_SAME_PLOT])`.
   - per/conditioned → build the `IntExpr`(`per:{POPULATION,each}` → `IntExprMult(IntExprAttribute(POPULATION),
     const)`) and/or the `BoolExpr` via the translator (B3) — do NOT drop or always-apply (the ~44 conditioned
     building files: Folklore tech-gates, Foundling-Hospital per-pop).
2. **PropertyInfo** (`CvPropertyInfo.cpp:29-91`): own-family walk → decay (`.city.percent`→`addDecaySource`),
   population baseline (`.city.flat` +`per:POPULATION`→`addAttributeConstantSource`); + read the new
   `properties.diffuse[]`→`addDiffusePropagator` and `properties.changePropagation[]`→the `getChangePropagator` table.
3. **JSON→legacy-BoolExpr translator** (NEW, small, scoped — owner-decision #1/#2 APPROVED): translate a
   `CvCondition` (the pocos' `enabled`/`disabled`) into a legacy `const BoolExpr*` for the known predicate set
   (the 4 diffuse tag-gates + the building tech-gates). One `BoolExprIs`-shaped node type; NOT a general bridge.

### C. Curator + data

1. Emit `properties.diffuse[]` / `properties.changePropagation[]` into `Assets/Data/properties/*.json` from
   `CIV4PropertyInfos.xml` (`PROPERTYPROPAGATOR_DIFFUSE` + the one `ChangePropagators`), applying the tag→predicate
   table. Recurate + regen; commit the regenerated data.
2. **Empire-scope building props (5 files, `PROPERTY_FLAMMABILITY`)** — owner-decision #3: add a small gather in
   `CvGameObjectCity::foreachManipulator` to also walk the owning player's `empire`-scope-flagged building deposits
   (the minimal engine touch), so those 5 deliver. *(Confirm with owner at implement time if a non-engine route exists.)*

### D. Explicitly SKIP (dead legacy surface — 0 real data; not phantom gaps)

`CvPropertySourceConstantLimited`, `CvPropertyPropagatorSpread`/`Gather`, all `CvPropertyInteraction`, and Corporation
manipulators. `PropertyBuildings`/`PropertyPromotions` value-bands stay the separately-flagged curator-gap
(`CvPropertyInfo.h:49-57`), not folded here.

## Increment 4 — the JSON→legacy-`BoolExpr` / `IntExpr` translator (LOCKED design, next build)

The source/propagator bridge DEFERS-and-COUNTS three conditioned classes; this increment lands them. Node types:
`BoolExpr.h` (`BoolExprIs(TagTypes)`, `BoolExprHas(GOMTypes,id)`, `BoolExprAnd`/`Or`/`Not`, `BoolExprConstant`) and
`IntExpr.h` (`IntExprConstant`, `IntExprAttribute(ATTRIBUTE_POPULATION)`, `IntExprMult`, `IntExprDiv`).

1. **Gated diffuse** (`jsonBuildPropertyPropagators`, currently `if(enabled) defer`): the `enabled` is a bare
   predicate STRING in the `properties.diffuse` entry. Map string→`TagTypes` → `BoolExprIs` → `pp->setActive(...)`:
   `IS_OWNED→TAG_OWNED`, `HAS_PEAK→TAG_PEAK`, `IS_WATER→TAG_WATER`, `IS_CITY→TAG_CITY` (verify the `TagTypes` names +
   how a tag string resolves — `BoolExprIs::read`/the tag registry). 4 tags, ~20 lines. (These strings do NOT parse
   into `CvCondition` — `CASC_PRED_IS_OWNED`/`IS_CITY` aren't in the predicate enum — so translate the raw string.)
2. **Tech-gated building flats** (~35 files, `jsonBuildPropertyManipulators` conditioned branch): the `enabled` IS a
   `CvCondition*` (from the modifier-family parse). Write `cascadeJsonCondToBoolExpr(const CvCondition*)`:
   - `CASC_COND_PRESENCE` `TECH_*` → `BoolExprHas(GOM_TECH, cond->id)` (confirm the `GOMTypes` for tech/bonus/building —
     grep `GOM_`); `BONUS_*`/`BUILDING_*` likewise if any appear.
   - `CASC_COND_PREDICATE` → `BoolExprIs`/other per the predKind (only if a building gate uses one).
   - `CASC_COND_GROUP`: `all`→fold `BoolExprAnd`, `anyOf`→`BoolExprOr`, `noneOf`→`BoolExprNot(Or…)`.
   - Then `src->setActive(expr)` on the `CvPropertySourceConstant`. Unknown/empty → skip the source (don't over-apply).
3. **Per-population building flats** (~9 files, e.g. Foundling Hospital `each>1`): build the amount `IntExpr` —
   `IntExprDiv(IntExprMult(IntExprConstant(value), IntExprAttribute(ATTRIBUTE_POPULATION)), IntExprConstant(each))` —
   into `CvPropertySourceConstant(prop, thatExpr)`. (The `each==1` property-baseline already uses `AttributeConstant`.)
4. **`changePropagation` table** (FLAMMABILITY only): add storage to `CvPropertyInfo` (a small `from×to→percent` map)
   - a real `getChangePropagator(from,to)`; populate from the deferred `changePropagation[]`.
5. **Empire-scope building props** (5 `FLAMMABILITY` files): the minimal gather touch in
   `CvGameObjectCity::foreachManipulator` (`CvGameObject.cpp:662-672`) to also walk the owning player's `empire`-scope
   building deposits — OR accept as out-of-scope (owner call at build time).

## Validate

Rebuild (Assert compile-check, then Release/agentstart), end a turn: the property sources are verified LIVE via the
endpoints (the per-turn `PROPERTY_*` deltas / property-source decomposition) — none lost, each attributed to a named
source; Canterbury crime/education normalise; commerce recovers on its own.

## Reference

- [json.md](../../specs/json.md) §5 (grants pulses `on`/`relation`/`distance`), §9 (`properties` bespoke section),
  §6 (families) · [modifier.md](../../specs/modifier.md) (`per`). Legacy: `CvPropertySource`/`CvPropertyManipulators`/
  `CvPropertySolver`/`CvGameObject.cpp:626-747` · `CIV4PropertyInfos.xml` (curator input, never read at runtime).
- Doc nit found: `Sources/Mainpage.md:373` links a non-existent `docs/reference/CvPropertySolver.md`.
