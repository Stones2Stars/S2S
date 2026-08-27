# 6. Effects — modifier families

> Part of the **[json](../json.md)** spec.

A modifier is how an entity **changes a game quantity** — a city's food, a unit's strength, a property's level.
The entity **deposits** a value onto a **target** — drops a contribution onto it; when several sources deposit
onto the same target, the target **sums** them and reads the combined total each turn (a Forge's +25% production and a Factory's +50% both land on
the city's production, which combines them — §6.3). A **modifier family** is one such effect, named by what it
changes (`food`, `production`, `happiness`, one per `PROPERTY_*`, …).

The full address of a deposit:

```
<family>.<scope>[.<target>|.<targetType>.{TARGET}][.<member>].<unit> = value
```

```jsonc
"happiness":  { "city": { "flat": 2 } },                                   // single-concept, scope-wide
"production": { "city": { "percent": 25 } },                               // a city-wide multiplier on output
"food":       { "city": { "improvements": { "IMPROVEMENT_FARM": { "flat": 1 } } } }, // named-entity target (keyed)
"maintenance":{ "empire": { "distance": { "percent": -10 } } }             // grouped family (member `distance`)
```

- **Split families** — one concept per key: yields are `food`/`production`/`commerce`; commerce splits into
  `gold`/`research`/`culture`/`espionage`; each property is its own family (`PROPERTY_CRIME`, …).
- **Grouped families** keep `<member>` parts (`maintenance`, `defense`, …): `maintenance` uses a `distance`
  member; `defense` uses an `amount` member, with a `min` member for the floor.
  ⚑ **`defense.amount` SUMS LIKE A FLAT and is APPLIED as a percentage (it is not a percentage in
  calculations, it's a flat sum added as a percentage for the defense calc) — which is exactly what the
  `percent` unit already means** (§3.6 / [modifier.md §2](../../cascade.md): percents are ADDITIVE DELTAS that sum
  and apply ONCE, never a per-source multiplier). So it authors `percent` and accumulates on the percent side;
  the value is measured in defense points, not scaled ([the ×100 fixed-point model](../curators/fixed-point-and-scales.md#1-the-model--integer-100-for-amounts-human-only-at-the-in-and-out-boundaries):
  a percent is never ×100).
  ⚑ **The stack has a FLOOR and NO CEILING — verified, because the absence is load-bearing.** The floor is the
  `min` kind (`getExtraMinDefense`), applied at the realized read.
  ⛔ **THE WHOLE FAMILY IS `percent` — EVERY member, no exceptions to remember.** The values are
  *technically* flat additive sums, and they are APPLIED as a percentage — *"it does increase combat of
  defending units by the percentage anyway"* — and **defense never carries decimals**, so the ×100 that the
  `flat` unit exists to buy ([the ×100 fixed-point model](../curators/fixed-point-and-scales.md#1-the-model--integer-100-for-amounts-human-only-at-the-in-and-out-boundaries): the
  scaling exists ONLY to carry two decimals) is worth exactly nothing here. ⚑ **Uniformity is the requirement,
  not the label** — *"it does not really matter at the end of the day, as long as all defense modifiers do the
  same thing."*
  ⚖ **THE DECISION TEST, and it generalizes to every family: does it SUM, or does it COMPOSE?** *"We
  are summing percentages; it would have been different if they were multiplicative."* An additive delta that
  sums and applies once is `percent` (unscaled); a factor that composes by product is `multiplier` (×100,
  identity 100) — §3.6. ⛔ Decide a kind's unit by that question, NEVER by the shape the legacy XML tag
  happened to have: the tags that read like modifiers were curated `percent` and the rest `flat`, which is the
  name-eyeballing [fixed-point-and-scales.md §3](../curators/fixed-point-and-scales.md) bans outright.
  ⚠ **The mixed state that ruling ends was a live ×100**, and it is the shape to recognise elsewhere: the UNIT
  plane authors every defense member `percent`, while the CITY plane had `dynamicDefense` as `flat` — and
  `dynamicDefenseTotal` SUMS the city value into the unit one. A single non-uniform member inside a family that
  adds across a seam is a silent 100×, not a tidiness question.
  ⚑ The one member outside the rule is the **`air` PLOT leg**, and it is outside because it is not a summed
  modifier at all: it is the magnitude fed to the opposed `getSorenRandNum` air-bombard dice, so nothing sums
  it and the test above does not reach it (the city `airDefense` percent is an ordinary member).
  There is **no cap of any kind** on accumulated
  defense: the contribution sites are unbounded `+=`, the total is unclamped, and the only `max`-shaped constant
  nearby — `MAX_CITY_DEFENSE_DAMAGE` — bounds DAMAGE DEALT TO defense (and is the decay denominator), a
  different axis entirely. ⚠ So an unbounded additive stack is the correct model here; do not "restore" a
  ceiling on the assumption that one was lost in the cut, and do not read the damage constant as one.
  ⛔ **There is NO separate natural-defense kind, and no max-combine.** `DEFENSE_AMOUNT` is one channel that
  BUILDINGS (153) and CULTURE LEVELS (18) both author, so the cascade holds ONE additive stack. The legacy
  `max(buildingDefense, naturalDefense)` has no counterpart here and does not survive the cut — a deliberate,
  data-led behaviour change ([validation.md](../validation.md): the spec leads), not an omission to restore.
- **⛔ THE MEMBER TRIAGE TEST — a member is a KIND only if it answers *WHICH component does this modify*.**
  `defense.bombardDefense` and `maintenance.distance` name components, so they are genuine kinds. A member that
  answers **WHEN or WHERE** the value applies is a **condition-as-member rollerskate** — the predicate simply has
  not been defined yet ([conditions are predicates, never bespoke members](03-the-shared-vocabulary.md#35-predicates--a-systems-runtime-state-query)),
  and it re-authors as a conditioned deposit (the worked case: `maintenance.empire.{homeArea,otherArea}` →
  `enabled: "IS_HOME_AREA"` / `"!IS_HOME_AREA"`, §3.5). Run this test on every proposed member: the scope axis and
  the conditions must never inflate a family's vocabulary. ⚑ A **`per<X>`-named member is its own verdict** — it IS
  a §3.7 `per` count-scaler (`perPopulation` → `per:{type:POPULATION}`), never a kind.
- **⛔ `strength` is the BASE; `combat` is everything that MODIFIES it.** `strength.unit.flat` is the
  unit's base value and is absent if it cannot fight; every semantic modifier (`attack`, `defense`, `cityAttack`,
  `cityDefense`, `hillsAttack`/`hillsDefense`, `stealth`, `flanking`, `lunge`, …) plus the type-keyed vs-entries
  (`UNITCOMBAT_*`/`UNIT_*`/`TERRAIN_*`/`FEATURE_*`/`DOMAIN_*`) is `combat`, at unit/empire/team/city scope. ⛔ A
  concept with its own family never hides as a `combat` member: capture → `capture`, cargo → `cargo`, ranges →
  `air`/`range`, espionage defense → its own family.
  > **⚖ FLANKING IS KEYED BY UNITCOMBAT, NEVER BY UNIT.** `combat.<scope>.flanking.{UNITCOMBAT_X}` is the
  > authored shape — *"mounted should be able to flank siege, is the very short answer."* A per-UNIT key names
  > individual units, so every unit nobody thought to list is silently un-flankable: *"right now flanking units
  > counter specific other units, which leaves grand-canyon-sized gaps in what is efficient for flanking."* The
  > class key closes them by construction, which is the whole reason it is the axis.
  > ⛔ **The per-unit table was encoding a BALANCE THEORY, and the theory is rejected:** it existed so
  > that knights and horsemen could not flank cannons — *"but that is, pardon the pun, horseshit; if you manage
  > to get horses close, the artilleryman is just as fucked as any previous siege."* So there is no era gate and
  > no per-unit carve-out to preserve: getting cavalry onto a siege crew is the mechanic, and which century the
  > crew is from does not enter into it.
  > ⚑ This is the [engine.md](../../reference/engine.md) UnitCombat distinction doing its job — a UNITCOMBAT is the
  > good/bad-AGAINST column, so a "vs" modifier keys on a CLASS. ⚠ It does NOT make flanking a per-target
  > enumeration: [skills.md §1](../skills.md)'s note tying flanking to `targets`' narrow per-target granularity
  > describes the shape being retired, not the one to author.
- **⚖ `capture` carries BOTH WHAT YOU GET AND THE ODDS.** `capture.unit.becomes` names the unit you
  receive for capturing this one; `capture.unit.probability` / `.resistance` are the odds. The two belong
  together because they are one mechanic answered from one place — splitting the result off into `identity` (or
  a bespoke block) would leave a reader holding the chance of an outcome the data never names.
  ⚑ So a family member is not required to be a magnitude: `becomes` is an FK, and that is the family stating its
  own outcome rather than a foreign concept hiding inside it.
- **THE COST CLUSTER IS THREE PLANES — do not merge them.** (1) The **actual cost** is the reserved `cost` section
  plus the entity's own self-data (`hurryCost` = "hurrying ME"; `buildTime`). (2) **What CHANGES a cost** is the ONE
  `costs` modifier family, kinds by category (`train`/`construct`/`create`/`build`/`research`/`improvementUpgrade`/
  `hurry`/`upgrade`), with **scope as the axis** — never a `world*`-prefixed kind
  ([scope is a separate axis, never folded into the kind](../../architecture/patterns/04-the-info-data-out-contract-what-an.md#the-coherent-surface--grouped-storage-parameterized-getters-clarity-and-predictability-is-king)). (3) The **derived price** (upgrade
  gold, hurry gold/pop) is engine-computed from planes 1 × 2; its formula parameters are world/handicap config, never
  vocabulary.
- **`underworld` is the in-city criminal contest** (criminals burrow, investigators drag them out): kinds
  `insidiousness` + `investigation`, at **city AND unit scope** — the city is the arena, the unit carries the
  stat (`UNITCOMBAT_CRIMINAL` vs `UNITCOMBAT_LAW_ENFORCEMENT`, resolved by
  `CvUnit::doInsidiousnessVSInvestigationCheck`). ⚠ **`detection` belongs to the hide-and-seek plane** — the
  map-level spotting of hidden units is a different system with its own block (`hideAndSeek`, §9), and must not be folded in here.
  > **⛔ UNDERWORLD IS NOT ESPIONAGE, AND THE LINE IS THE DEFINITION OF ESPIONAGE: "espionage is only
  > things that SPY UNITS can do", plus the espionage-POINT ratios that govern how much of an opponent you can
  > see.** A criminal hiding from an investigator is neither, so `insidiousness`/`investigation` are underworld
  > on BOTH planes and espionage carries only its commerce channel. ⛔ Do not re-file them by proximity: several
  > unit types carry both, which is exactly what makes the mistake easy.
  > ⚑ This was mis-filed independently in THREE curators while two others had it right — the signature of a
  > boundary that was never written down. It is written down here now; a member name resolves to exactly one
  > family (the espionage kind-enum no longer carries these words at all).
- The **unit plane** has its own family set (`strength`, `withdrawal`, `firstStrike`, `bombard`, `collateral`,
  `air`, `heal`, `movement`, `experience`, `workRate`, `cargo`, `vision`, `capture`, …); a `unit`-scope deposit is
  a self-accumulator.
- **`buildRate` vs `production` — keep them distinct.** `production.city` is the city's *total* output (scales
  every build); **`buildRate`** only speeds up *building a specific target*: `buildRate.self` (build **this**
  entity faster — the off-spine `self` scope), or keyed by what's built (`buildRate.<scope>.buildings.{BUILDING}`,
  or a category like `military`).
- **Era-dependent values use the `ERA` COUNTER, not a bespoke key.** Era is a plain
  counter (1…X, §3.1) like `POPULATION`/`TURN`; a value that changes with era is authored as ordinary conditioned
  deposits gated on an `ERA` count-threshold — `flat: [ {value, enabled:{type:ERA, min:N}}, … ]` — so the bands
  **accumulate for free** through normal deposit summation (every entry whose `min` ≤ the current era applies). No
  special resolver, no `world.eras` lookup. (The curator converts a legacy `EraCommerceChanges` band-table into
  era-threshold flats, mapping each era Type to its counter index.)

### 6.1 Two ways a deposit picks WHAT it lands on

- **plural object-target** (`plots`/`units`/…, predicate-filtered) = *every object of that kind in the scope*:

  ```jsonc
  "production": { "empire": { "plots": { "flat": { "value": 1, "enabled": "IS_WATER" } } } } // +1 to every empire water plot
  "food":       { "city":   { "plots": { "flat": { "value": 1, "enabled": {"all":["VICINITY","IS_WORKED"]} } } } }
  "movement":   { "empire": { "units": { "flat": { "value": 1, "enabled": "IS_WATER" } } } } // +1 move to every naval unit
  ```

- **named-entity key** (`improvements.{IMPROVEMENT_FARM}`, `terrains.{…}`, `features.{…}`, `bonus.{…}`,
  `buildings.{…}`) = a deposit onto a specific named target, kept on the source.

> **The rule:** a plot/unit **fact** (water, river, worked, hills) → a **`plots`/`units`-target predicate**; a
> **named entity** (a farm, grassland) → its **entity-key**. There is no `plotTypes`/`seaPlot` — a water plot is
> `plots {IS_WATER}`, a hill `plots {HAS_HILLS}`.

### 6.2 Ownership — the deliveryguy rule

A cross-entity modifier lives on **whoever brings it to the table**, keyed by the target — never inverted onto the
target; the other entity is only ever an `enabled`/`requires` condition, never the home. Full ruling, the
own-output vs governing-deliverer split, and the plot-substrate case: [modifier.md §4](../../cascade.md)
([the deliveryguy ownership rule](../../cascade/18-ownership.md#4-ownership--the-deliveryguy-rule)).

### 6.3 How values combine

The combine arithmetic — flats sum into base, percents (additive deltas) sum and apply once, multipliers compose
by product — is the machine's, not the authoring model's: [modifier.md §2](../../cascade.md). You author the values;
the engine combines them, and the combine mode is **family metadata**, never the per-value unit (§6's member-triage
test above).

---

