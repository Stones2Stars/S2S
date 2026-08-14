# Property source-data migration — LOCKED SPEC (owner-approved 2026-07-11)

> **⚖ THE GOVERNING MODEL — EACH PROPERTY IS A CHANNEL IN THE CASCADE, AND THE CASCADE FEEDS WHEREVER THE
> PROPERTIES ARE SUPPOSED TO GO (owner).** That is not a new axis: it is
> [DEC-universal-yield](../../architecture/decisions.md#dec-universal-yield) applied to the plane it already
> names — every number game mechanics modify is a channel in the ONE machine — and
> [state-repositories.md](../../architecture/state-repositories.md) already carries `PROPERTY_*` as one channel
> per property info in the minted channel sets.
> ⇒ **The line falls where the WORK differs, and both halves keep their owner:**
> - **the CASCADE owns WHICH SOURCES APPLY and their summed per-turn contribution** — a maintained sum like any
>   other channel, correct the instant a fact arrives, never re-derived;
> - **the PROPERTY ENGINE owns INTEGRATING that rate** — decay, spatial diffusion, the solver's ordered
>   predict/compute/correct/apply. Untouched, owner-locked, exactly as the mandate below says.
> ⚑ **This IS "feed the engine, do not remodel it"** — the engine keeps every piece of math it owns and stops
> re-discovering its inputs. ⛔ So it is NOT licence to touch the solver, the decay curve or the propagators.
> ⚠ **What it supersedes is the per-turn REDISCOVERY, not the bridge's job.** `CvPropertySolver::doTurn` today
> clears its whole context set, walks EVERY game object of every kind (game · players · cities · units · plots)
> through `CvGameObjectCity::foreachManipulator` and its siblings, instantiates a `PropertySourceContext` per
> source, solves, and discards the lot — `O(what EXISTS)` per turn, over the whole world, which is precisely the
> shape the maintained sum deletes. ⚑ `foreachManipulator` itself is honestly named and is not the defect: it is
> an object reporting its OWN inventory. The rebuild is its caller (`gatherActiveManipulators`), whose mild name
> is what let a full-world per-turn re-derivation read as an aggregation step.
> ⇒ **Consequence for the EMPIRE-SCOPE FAN below: do NOT patch it into `foreachManipulator`.** Under the channel
> model an empire-scope property deposit rolls DOWN like any other deposit and dormancy is already the enabler's
> verdict, so the fan falls out for free — while the five-line version would add work to the rebuild being
> removed AND need a player-scope active count built only to be thrown away.
>
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
> **Built and compiling:** the city-scope C++ bridge (building/unit flats + decay + population baseline); the
> curator emitting `properties.diffuse[]` / `changePropagation[]`; `CvPropertyInfo` reading them; and the
> JSON→legacy-`BoolExpr`/`IntExpr` translator (`Sources/Property/CvPropertyBridge.cpp`,
> `CascadePropertyBridge::condToBoolExpr`) — verified live: ANCIENT_CUSTOMS = exactly its 3 authored sources,
> folklore = their 2 gated education sources.
>
> **Still open — the EMPIRE-SCOPE PROPERTY FAN** (called "the all-cities gather" here until the name was found to
> read backwards — see the callout below). Three of its four links are built and the fourth is absent:
> the 5 capped buildings authoring `PROPERTY_FLAMMABILITY.empire.flat` (`asteroid_deflection_system` −20 ·
> `department_of_water` −8 · `national_fire_service` −5 · `sentinel` −10 · `solar_weather_monitor` −10, every one
> `allowed:{empire:1}`); the bridge routing those entries into `m_PropertyManipulatorsAllCities`
> (`CvBuildingInfo.h:305`); and the load-built `GC.getAllCitiesManipBuildings()` index (`CvGlobals.h:400`) —
> but **that index has ZERO consumers**: `CvGameObjectCity::foreachManipulator` (`CvGameObject.cpp:721`) walks the
> city's OWN buildings only.
> ⛔ **So those five deliver NOWHERE — not empire-wide, and not even locally**, because the empire-scope entries
> are routed OUT of the ordinary container into the all-cities one. The visible consequence is that the national
> fire-service class reduces flammability in no city at all, which is worth holding beside the rebalance above:
> its complaint was that *"every reducer arrived late"*, and these reducers do not arrive.
> ⚠ The 6th legacy `<PropertiesAllCities>` block is NOT missing — it sits on an uncapped ORDINANCE, which
> `_deposit_scope` correctly emits at city scope (an ordinance is placed in every city already, so city scope is
> the same delivery). Do not "fix" that one to empire.
>
> > **⛔ TWO DIFFERENT MECHANISMS SHARE THE WORDS "ALL CITIES", AND CONFLATING THEM IS WHY THIS ENTRY READ AS A
> > MISSING GATHER (owner).** Get the DIRECTION right and they separate instantly:
> > - **the RECEIVER Σ — many cities → one empire total.** The empire's gold / research / culture / espionage
> >   (and maintenance) summed from its cities' realized values at the read
> >   ([state-repositories.md](../../architecture/state-repositories.md) § A CROSS-SCOPE RECEIVER). It is REQUIRED
> >   and it is BUILT (`InfoValuation::realizedAtEmpire`, gated on `!isDisorder()`) — *"otherwise research would
> >   have failed"* (owner).
> > - **the PROPERTY FAN — one source → many cities.** A single building's property source applying in every city
> >   of its owner. That is the item above, and it is a FAN, never a gather.
> > ⛔ Nothing gathers properties FROM cities today, and this entry never asked for one.
> > ⚖ **Doing so is WANTED but only as STATS (owner): *"there is nothing stopping us to gather it all, just for
> > shits and giggles, stats more than anything else."*** So an empire-wide per-property total is un-killed
> > forward intent for the demographics/observability surface
> > ([DEC-keep-unkilled-ideas](../../architecture/decisions.md#dec-keep-unkilled-ideas)) — ⛔ never a cascade input
> > and never a thing the solver reads, or it becomes a second maintenance surface for values the cities own.
>
> **Then validate** — the turn-level pass (per-turn `PROPERTY_*` deltas attributed, education/crime normalise) on
> played turns.
>
> **⚖ THE ONE-SHOT RULING (owner 2026-07-16 — ruled earlier but never written down, twice, so it was "re-found"
> a third time): the legacy one-shot `<Properties>`/`<PropertiesAllCities>` semantic is DEAD — EVERY such value
> RE-CLASSIFIES as a PER-TURN source. No exceptions: flammability converts too.**
>
> **The flammability rebalance has TWO halves and they are complements, not alternatives.** Its problem was that
> every reducer arrived late (fire code / smoke detector / fire service), so the fix is a curator SCALING rule plus
> early-game COUNTERS:
> - **positive adders `/5` (rounded), negative reducers unchanged** — a rule in `curate_building.py`, since there
>   is no separate `PROPERTY_FIRE`: all fire data is `PROPERTY_FLAMMABILITY` flowing through the generic
>   `PROPERTY_*` fold;
> - **early-game reducers** authored through the post-curation additions layer (`Assets/Data/_additions/`).
>
> ⛔ Both live in the CURATOR/additions pipeline, never in the derived JSON — hand-editing `Assets/Data/**` is
> banned, and any further balance move is a curator change → recurate + regen
> ([DEC-recurate-on-decision](../../architecture/decisions.md#dec-recurate-on-decision)).
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
> **THE CARRIER BRIDGES (owner 2026-07-16: "stubbing is straight up not allowed"):** every legacy
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
  > **⚖ FLAMMABILITY AUTHORS NEITHER — ITS BASELINE IS 0 (owner).** No decay, no population baseline — the
  > city's flammability is a PURE ACCUMULATOR over its source deltas: the per-turn building adders push it up
  > and the reducers pull it down, and nothing else moves it. ⚖ A REAL decay/baseline for it is a LATER owner
  > design decision, not a gap to fill by matching the siblings — the owner also notes flammability is a bit
  > too NARROW as a property compared to the others, so the shape may be revisited wholesale. ⛔ Until that
  > decision, do not author a decay or baseline into `property_flammability.json`.
  > ⚑ Consequence to hold when reading a city's value: under the per-turn model a fresh game (or a save whose
  > sessions predate the source feed) legitimately reads 0 everywhere until turns have run — an empty
  > flammability row is the accumulator starting from its 0 baseline, not evidence the feed is broken.
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
manipulators. The `PropertyBuildings` value-bands are not folded here either — they are the BUILDING's
`requires.operate` clause ([enabler.md §3](../../specs/enabler.md)), never a property-side member.

## Validate

Rebuild (Assert compile-check, then Release/agentstart), end a turn: the property sources are verified LIVE via the
endpoints (the per-turn `PROPERTY_*` deltas / property-source decomposition) — none lost, each attributed to a named
source; Canterbury crime/education normalise; commerce recovers on its own.

## Reference

- [json.md](../../specs/json.md) §5 (grants pulses `on`/`relation`/`distance`), §9 (`properties` bespoke section),
  §6 (families) · [modifier.md](../../specs/modifier.md) (`per`). Legacy: `CvPropertySource`/`CvPropertyManipulators`/
  `CvPropertySolver`/`CvGameObject.cpp:626-747` · `CIV4PropertyInfos.xml` (curator input, never read at runtime).
- Doc nit found: `Sources/Mainpage.md:373` links a non-existent `docs/reference/CvPropertySolver.md`.
