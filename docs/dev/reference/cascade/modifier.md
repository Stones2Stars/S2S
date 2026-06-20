# The modifier machine — "how much?", in full detail

> **Status:** reference (the modifier cascade — structure **v3-LOCKED**, owner alignment 2026-06-15) ·
> **Verified against:** old `docs/dev/plans/modifier-cascade-spec.md` (the v3-locked spec) reconciled with the
> landed `Sources/Cascade/CvCascadeModifier.{h,cpp}` — 2026-06-19. · **Grounding:** the data structure is
> design-LANDED and locked; the engine is **partially built** — the combine primitive + a city-yields pilot
> exist, the rest is parsed-and-pending. Each claim below marks landed-vs-built. The `Sources/` tree was
> reorganized (`Cv*` → `Sources/Engine/`, cascade → `Sources/Cascade/`); citations use the new prefixes.
> Line numbers **drift** — confirm the named function/struct, not the integer.
>
> **OOS non-negotiable (touches this subsystem).** All modifier math is **integer fixed-point ×100, no float
> ever** — Civ4 MP is deterministic lockstep and CPU-dependent float math desyncs. The human→×100 conversion
> lives once in `readJson`; the combine math here is pure integer. → [DEC-fixedpoint-x100](../../architecture/decisions.md#dec-fixedpoint-x100).

**BLUF.** The modifier is the cascade machine that answers **"how much?"** — magnitudes deposit **DOWN** the
scope spine and accumulate; a target reads an O(1) summed accumulator. A modifier value is addressed as
**`<family>.<scope>[.<targetType>.{TARGET}][.<member>].<unit> = value`**, conditioned by the optional
`enabled`/`disabled`/`per` fields. This doc owns the **deep detail**: the families × scopes × channels
structure, the §[Arithmetic](#3-the-combine-arithmetic-fixed-point) combine math, the
[conditioning](#4-conditioning--enabled--disabled--per) model, and the full
**[deliveryguy / keep-on-source ownership rule](#6-ownership--the-deliveryguy-rule)** — the canonical home of
[DEC-deliveryguy](../../architecture/decisions.md#dec-deliveryguy). For the *why* and how the three machines fit
together, see the [cascade overview](../../explanation/cascade-architecture.md) §3 (not re-explained here); for
the JSON authoring surface as a whole, see [data-model](data-model.md); for value scales, the
[scale registry](fixed-point-and-scales.md).

---

## 1. What a modifier IS, and the one-step flow

A modifier is a **per-turn effect a source deposits onto a TARGET** (anything with a modifiable per-turn
effect — a city's yield, a unit's strength, a property's accrual). It flows **DOWN** the containment spine and
accumulates; the target reads the summed accumulator. This is **one step** (deposit → accumulate), versus the
enabler's two-step (generate → gate).

Three governing rules pin the machine ([from the spec §0](#9-source--governing-rules)):

- **Purely TOP-DOWN; never requires UPWARD.** Sources deposit DOWN; targets read an O(1) summed accumulator;
  the reverse index ("who modifies me") is cold-path / pedia only. A condition embedded in a deposit is a
  **forward HAS-membership read**, not an upward cascade-walk.
- **Tech-inflation is a DOWNWARD deposit, not a gate.** "Researching tech T makes everything below better" is
  the tech depositing down onto its targets (authored on the tech) — never the lower thing reaching up with a
  `hasTech` gate.
- **Info DATA vs engine MACHINERY is a hard boundary.** The JSON carries only values, payloads, and
  relationships. The machinery that *consumes* them — producers/creators, evaluators/resolvers, the event-hook
  dispatch, the tally — is engine-side and never authored in the data.

---

## 2. The address — families × scopes × channels

A modifier value is addressed as:

```
<family>.<scope>.<targetType>.{TARGET}.<member>.<unit> = value
```

- `<targetType>.{TARGET}` is **omitted for scope-wide deposits** (the identifier lives in the family name).
- `<member>` is **omitted for single-concept families**.

This is the whole structure. Each axis:

### 2.1 Families — the top-level effect keys

Every top-level key on an entity is either a **reserved section** (`enables`/`requires`/`grants`/`identity`/…
— the enabler & intrinsic surface, see [data-model §1](data-model.md)) or a **modifier family**. There is **no
`modifier:` wrapper** — families sit flat at the top level (minimal nesting). The reserved set makes
"section vs family" deterministic; a family named like a reserved word is a build-time error.

**SPLIT families (load-bearing).** A 2D legacy "X × yield" / "X × commerce" map collapses by splitting one
axis into separate top-level families:

| legacy 2D family | split into |
|---|---|
| `yield` | `food` · `production` · `commerce` |
| `commerce` | `gold` · `research` · `culture` · `espionage` |
| `property` | one family per `PROPERTY_*` |

This absorbs one axis of every X×yield / X×commerce map (collapsing ~17 of ~36 "2D" fields to a single key).

**GROUPED families** keep multiple members under one key (`maintenance`, `defense` [`amount`/`min`/…],
`combat`/`strength`, …); **single-concept** families are top-level scalars-by-scope (`happiness`, `health`,
`growth`, `buildRate`, `upkeep`, `tradeRoutes`, …). The unit plane adds its own family set
(`strength`/`withdrawal`/`firstStrike`/`bombard`/`collateral`/`air`/`heal`/`movement`/`experience`/`workRate`/
`cargo`/`vision`/`capture`/…). Families are **extensible** — one per concept; the exhaustive per-entity
enumeration lives in the [migration rename registry](../../json-migration/migration-renames.md), not here.

### 2.2 Scopes — the containment spine

`world | team | empire | area | city | plot{improvement|feature|terrain|route} | building | specialist |
unit`. (`empire` = the player = all cities.)

The spine is encoded **complete** in the engine — `ModifierScope` in `Sources/Cascade/CvCascadeModifier.h`
(`MODSCOPE_WORLD … MODSCOPE_UNIT`) — so the scope model has no gaps to rediscover; the parser + deposit-flow
consume each scope as the data needs it. Two scopes are not ordinary city-output scopes:

- **`plot`** is a **FIRST-CLASS** scope (owner 2026-06-19). A worked tile self-contains its modifier detail and
  reports ONE rolled-up yield to the city ("this is what you get from me this turn"); the city never sees which
  plot got buffed by which improvement — the same report-isolation as the tally. Keyed plot sub-targets
  (terrain/feature/improvement/route) resolve *inside* the plot.
- **`unit`** is a **SELF-ACCUMULATOR** — see §5.
- **`self`** (`MODSCOPE_SELF`) is the entity's own build cost (`buildRate.self`), not a city-output scope (§6.2).

### 2.3 Channels (units) — what the value IS

A leaf's `<unit>` names a magnitude. **The unit names what the value IS, not how the engine combines it** (a
reconciliation owner-ruling, [spec §0.7](#9-source--governing-rules)) — a percentage-valued field is `percent`
even if the engine applies it multiplicatively; that combine behaviour is family metadata, reworked to fit.

| unit | meaning | engine internal |
|---|---|---|
| `flat` | additive amount (sums into base) | `MODUNIT_FLAT` |
| `percent` | additive percent DELTA, summed (`+50%` = `50`) | `MODUNIT_PERCENT` |
| `multiplier` | true MULTIPLICATIVE factor, full-scale (`×2` = `200`, identity `100`), composed by PRODUCT | `MODUNIT_MULTIPLIER` |

(+ rare `postMultiplier` / `rawPercent` — engine detail, pins at #430.) The exact scale rules (human→×100, the
closed per-100 set, per-field de-scale) are the [scale registry](fixed-point-and-scales.md)'s concern, **not
restated here** — a ×100 value in a JSON file is a curator bug → [DEC-fixedpoint-x100](../../architecture/decisions.md#dec-fixedpoint-x100).

> **The `multiplier` channel is a deliberate NEW capability** beyond the legacy additive-only consumer. In the
> landed pilot it is gated OFF behind a parity flag (§3) so the engine stays additive-only while the wiring is
> shadow-proven; the new product behaviour is enabled after parity. **Built, but disabled in parity mode.**

### 2.4 Vocabulary shared with the rest of the data

The address's Types, scopes, and conditions are the **one shared vocabulary** ([data-model §2](data-model.md)),
not a modifier-only language:

- **Types** = data Types (`BONUS_*`, `UNIT_*`, `BUILDING_*`, …; resolved via `getInfoTypeForString`) **+
  engine CATCH-ALL tokens** (`TURN`, `POPULATION`, `MILITARY`, `SELF`, … — a code-side registry, designed at
  #430, resolved by the engine).
- **`SELF`** = the owning entity's own type, resolved per-entity at evaluation. Its **only** live use is the
  `per` count-scaler (`per:{type:SELF, scope:world}` = count of own type at scope). SELF no longer appears in
  `requires`/`enabled`/`disabled` — the old global-uniqueness idiom is withdrawn in favour of the declarative
  `allowed` cap (data-model §3.4).
- **Conditions** (`enabled`/`disabled`/`per`'s implicit count) = the enabler `requires` object **verbatim**,
  same `all`/`any`/`noneOf` serialization across both cascades (§4).

### 2.5 The ONE entry shape

A deposit (and a `grant`, and a condition clause) is one shape everywhere:

```jsonc
{ <payload>, "scope"?, "per"?, "enabled"?, "disabled"?, "ai"? }
```

- **payload** — a modifier magnitude (`flat`/`percent`/`multiplier`: value) OR a grant (`type`+`count`).
- **`scope`** — default = the entry's containing scope · **`per`** — count-scaler, default ×1 (§4) ·
  **`enabled`/`disabled`** — the enabler condition object, verbatim; default-absent (§4).
- **`ai`** — an OPTIONAL leaf-level AUDIENCE qualifier (owner extension 2026-06-15): the bare unit keys apply to
  **all** players; an `ai` sibling block (same inner `<unit>: value` shape) holds an **AI-only** deposit stacked
  on top (or the sole value for an AI-only field). E.g. `unit: { percent: 100, ai: { percent: 120 } }` = 100%
  for everyone, AI players an additional ×120%. Born from the Handicap human/AI split. The axis is `all` (bare)
  vs `ai`; a `human`-only audience is not yet needed.
- A leaf is a **single entry OR a cumulative LIST of entries** (multiple conditioned deposits to one slot).

### 2.6 Worked address example

```jsonc
"production": { "city": { "percent": [
    25,                                                        // always-on +25%
    { "value": 25, "enabled": { "min": ["BONUS_COAL", 1] } }  // +25% more while coal connected
] } },
"gold": { "city": { "flat":
    { "value": 1, "per": { "type": "RELIGION_*", "scope": "world" } } } },  // +1 gold per world religion
"maintenance": { "empire": { "all": { "percent": -10 } } }                  // cost-style; engine combines (§7)
```

The same composition (payload + scope + per + enabled) expresses a coal-gated production bonus, a per-count
gold deposit, and an empire-wide cost reduction. **One structure, one vocabulary, composed** — no per-concept
shapes, no nesting fiesta, no parallel gate languages. That is the migration's whole win.

---

## 3. The combine arithmetic (fixed-point)

**The accumulation rule** (the design choice; exact ordering pins at #430), per `(family, member, unit,
item)`:

```
effective = (base + Σflat) × (100 + Σpercent)/100 × Π(multiplier/100)
```

- **flats** sum into base;
- **percents** (additive deltas) sum, then apply once;
- **multipliers** compose by PRODUCT.

**This is LANDED in code.** `CvModifierSlot` (`Sources/Cascade/CvCascadeModifier.h`) is the one accumulation
slot: it carries `iFlat` (Σflat), `iPercent` (Σpercent), `iMultiplierX100` (Π, stored ×100, identity 100);
`deposit(unit, value)` folds a value in, `effective(base)` applies the formula above. The slot does pure
integer math and **never knows about ×100 at the human boundary** — readJson did that conversion once.

- **Per-target slot build (pilot).** `cascadeModifierCitySlot(family, ctx, slotOut)` folds a city's present
  buildings' city-scope deposits + the player's active civics' empire-scope deposits (the empire→city
  roll-down) into one slot, each whose `enabled` holds and `disabled` doesn't (re-evaluated per turn — the
  dormancy model). **PILOT only**: city-scope yields; widens to plot + trait/tech/building-empire sources +
  other families later.
- **The pre-modifier BASE.** `cascadeModifierCityBase(city, family)` returns the legacy
  `(getBaseYieldRate + getSpecialistYieldTotal)` pair the city modifier applies to (mirrors
  `getYieldRate100`, `Sources/Engine/CvCity.cpp` ≈ `:11253`) — ONE definition the shadow + both endpoints share
  so the base can't drift.
- **Parity mode (`cascadeModifierParityMode`).** A build-time const: when true, MULTIPLIER deposits are treated
  as identity (skipped) so the engine is **additive-only — matching legacy** — to prove the deposit-flow wiring
  before the new multiplier capability is enabled. Yields author no multipliers, so it is a no-op for the pilot;
  the flag is the framework hook (`modifier-cascade-shadow-spec` §6, R-M1).

**Parity with the legacy result is NOT a goal** — the cascade frequently *corrects* latent bugs (the legacy
consumer was additive-only; ±10% is not "parity-adjacent") → [DEC-parity-not-goal](../../architecture/decisions.md#dec-parity-not-goal).

---

## 4. Conditioning — `enabled` / `disabled` / `per`

Conditioning lives **near the value, per deposit**, as a few OPTIONAL fields and **nowhere else** — this is
what keeps the construct from becoming a nesting fiesta.

### 4.1 `enabled` / `disabled` — the keystone

- **`enabled`** — defaults **true**; present → the deposit applies only while its expression holds.
- **`disabled`** — defaults **false**; present → the deposit is suppressed while its expression holds.
- **Value of each = the enabler `requires` object, verbatim** (same `all`/`any`/`noneOf`, `min`/`max`,
  presence atoms, predicates). Absent = unconditioned (the common case stays clean). Effective =
  `enabled holds ∧ ¬disabled holds`, **re-evaluated each recompute** (this IS the dormancy model).
- **BARE-STRING predicate shorthand** — a parameter-free / unambiguously-scoped predicate is a bare string
  (`enabled: HAS_RIVER`, `enabled: IS_CAPITAL`), not `{HAS_RIVER: true}`. A parameterized predicate keeps the
  object form (`{HOLY_CITY: RELIGION_X}`). Its **negation is the `disabled` twin** (`disabled: HAS_RIVER`) —
  the bare form never needs a `false` value.
- **No group wrapper.** The only nesting is the enabler expression itself (bounded `all`/`any`). A modder
  cannot build condition-trees.
- The same `enabled`/`disabled` + list-of-entries pattern is used by **`grants`** too — it is the universal
  entry shape, not a modifier-only feature.
- **No per-deposit gate beyond this.** Whole-entity availability/operation conditions (`powered`,
  `stateReligion`, `capital`, … when they gate the *whole* thing) live in the entity's enabler `requires`; a
  dormant/unavailable entity simply deposits nothing.

**Code:** a deposit carries `CvCascadeCondition enabled` + `disabled` (`CvCascadeModifierDeposit`,
`Sources/Cascade/CvCascadeModifier.h`); empty = always-on / never-off. The slot builders re-check both against
the per-turn context. **Built (pilot).**

### 4.2 `per` — count-scaling

"Increase/decrease in something based on the NUMBER of something else." An OPTIONAL count-scaler on a deposit,
default ×1 — the same opt-in philosophy as `enabled`.

```jsonc
"food":      { "city": { "flat": { "value": 1, "per": { "type": "BONUS_COAL", "each": 1, "scope": "plot" } } } } // +1 food per coal on the plot
"happiness": { "city": { "flat": { "value": 1, "per": { "type": "POPULATION", "each": 5, "scope": "city" } } } } // +1 happy per 5 population
```

- **`per: { type, each, scope }`** scales the value by the COUNT of `type` at `scope`, in quanta of `each`:
  **effect = `value × (count(type) / each)`**.
- **`per: { anyOf: [TYPE…], scope }`** — scale by the SUMMED count of *any* of a SET of types (parallels the
  enabler `any`): **effect = `value × Σ count(t) for t in anyOf`**. Born from Corporation (per-bonus output
  scales by the total of its PrereqBonuses present). `type` (single) and `anyOf` (set) are the two forms;
  `each` may accompany either.
- **`each` — the QUANTUM, "per how many of `type`"** — `each: 1` = per each one; `each: 5` = per 5. **State it
  explicitly** (a bare `per:{type}` is ambiguous between "per 1" and "per N"). (Property-source attribute
  scaling is always `each: 1`.)
- **`scope`** defaults to the deposit's own scope; state it only when the count comes from a *different* scope.
  Cross-city scopes (empire/team/world) resolve via the **tally** (the same additive roll-up the enabler
  `requires` count-thresholds read — one module, two readers); `city`/`plot` = the local count.
- **`per` SUBSUMES the per-X units:** `perPopulation`→`per:{type:POPULATION}`,
  `perMilitaryUnit`→`per:{type:MILITARY}`, per-turn emission→`per:{type:TURN}`. There is **no separate
  `perPopulation`/`perMilitaryUnit`/`perTurn` unit** — one mechanism scales by the count of anything.

### 4.3 `grants.repeatable` + the `interval` temporal primitive

The third grant-lifecycle variant (alongside one-shot `grants` and deferred `outcomes`) fires **recurringly on
an INTERVAL**, optionally gated by a CHANCE the engine rolls. It introduces ONE new model primitive —
**`interval`**, the temporal sibling of `per`:

- **`interval: { perTurn: N }`** = every N turns; bare **`interval: perTurn` DESUGARS to `{ perTurn: 1 }`**
  (same bare-shorthand pattern as the §4.1 predicates). Other interval kinds (`everyEra`, …) grow per case.
- **A `grants.repeatable` entry** = `{ <payload>, interval, chance?, enabled? }`. **payload** = what is granted
  each interval (a `unit` PropertySpawn, a `heal` count, …); **`chance`** *(optional)* = the probability the
  engine rolls before granting, **reusing the §4.2 `per` count-scaler** so it can scale with a count (e.g.
  `chance: { per: { type: PROPERTY_CRIME, scope: city } }` — spawn odds rise with crime); **`enabled`** = the
  usual §4.1 conditional.
- **TWO-STAGE gate:** the building's own `requires` is the OUTER gate (is this building active at all); the
  repeatable entry's `chance`/`enabled` is the INNER gate, re-checked each interval.
- **Engine boundary:** the data carries payload + interval + chance-driver only; the engine owns the
  `getSorenRandNum` roll, the interval ticking, and owner-selection (a NEGATIVE property like crime spawns the
  unit **barbarian-owned**, `PropertyInfo.AIWeight < 0`). Exact chance arithmetic pins at #430.

```jsonc
"grants": { "repeatable": [
  { "unit": "UNIT_PROPERTY_CRIMINAL", "interval": "perTurn",
    "chance": { "per": { "type": "PROPERTY_CRIME", "scope": "city" } } }
] }
```

**NOT a repeatable grant: a continuous count-scaled RATE is just a `per`-scaled MODIFIER** — the shrine adding
commerce every turn scaled by cities holding its religion is exactly
`commerce.city.flat: { value, per: { type: RELIGION_X, scope: world } }`. `grants.repeatable` is for DISCRETE
per-interval provisions (a unit, a heal) that cannot be a standing accumulator. **Design-landed; engine pins
at #430.**

---

## 5. The `unit` plane — a self-accumulator

~380 fields author at `*.unit.*` (all Promotion/UnitCombat + most Unit). `unit` is a spine leaf, but a
unit-scope deposit is a **self-accumulator** (source == target, via the existing promotion/unitcombat additive
stack) — NOT a downward cascade. Cross-edges use `byOccupant` (a host-scope family summed over plot-occupant
units — celebrity/garrison/military-happiness) and `byCargo` (host-unit, SpecialUnit→carrier). Size promotions
are the worked example (the game-option + merge action are *enabler*; the size promotion it applies is the
*modifier*).

The unit-stat family vocabulary (defined at the Promotion pass, shared by UnitCombat + SpecialUnit): the combat
family is **`strength`** ("the strength of something, or weakness on/into/against something") — it absorbs
general/flat/SM/situational/vs-keyed combat sub-stats; alongside `withdrawal`/`firstStrike`/`bombard`/
`collateral`/`air`/`heal`/`movement`/`experience`/`workRate`/`cargo`/`upkeep`/`vision`/`capture`/`poison`/
`espionage`/`trap`. Two unit-plane shapes settled: (a) **CAPABILITIES = a separate BOOLEAN group**
(grant=`true`/revoke=`false`) — pure abilities, not magnitude modifiers; (b) the hide-&-seek **LOS tables → a
non-cascade `vision` resolver sub-object** (§7), not additive families.

**Design principle:** the unit-plane definition is authored for **O(1) runtime CONCATENATION onto the unit**,
never apply-time post-processing — a static promotion's `unit`-scope deposits SUM, capabilities UNION, and the
`promotionLine: {LINE: rank}` object MERGES as each promotion is added to the one unit. **Design-landed;** the
full vocabulary + per-field map is owned by the Promotion/UnitCombat curation pass (last per the shadow-spec,
the largest surface).

---

## 6. Ownership — the deliveryguy rule

> **This section is the canonical home of [DEC-deliveryguy](../../architecture/decisions.md#dec-deliveryguy).**
> Resolved while curating Route (#19) and Terrain (#20); owner ruling 2026-06-16.

The question: **an X-keyed-by-Y modifier — does it live on X or fold onto Y?** The root test is **SEMANTIC
SENSE: where does this modifier sensibly belong?** The anchoring case is that **a thing ON A PLOT OWNS ITS OWN
MODIFIERS** — *"it doesn't make sense that things on a plot don't own their own modifiers."* The operational
reading: **"who actually BRINGS this modifier to the table?"** (the deliverer) — that entity OWNS it — **"and
then what ENABLES it?"** (the condition). Not the atom kind; the *ownership*, judged by what reads sensibly.

### 6.1 Two equally-first-class expression modes

The toolkit supports BOTH, and the choice between them IS the semantic judgement, not a rigid rule:

- **(a) keep-on-source** — the source owns the modifier and references the other entity as a *condition*
  (`enabled` presence / `per` count). *"+happiness from this civic, if you also have `BONUS_X`"* is the
  **civic's** buff, conditioned on the resource via `enabled`/`per` — it **STAYS on the civic**. (The "something
  to make it work" is just the §4 conditioning machinery — no new mechanism.)
- **(b) fold-onto-the-deliveryguy** — the modifier lives on the entity that semantically owns it (the
  deliveryguy / the thing on the plot), keyed by the source.

Per case, author it wherever it **makes semantic sense** as the home.

### 6.2 Who owns what

- **Abstract enabler-sources (civic, trait, religion) OWN their buffs**, even when keyed by a target or
  *improved by* a resource. This is the keep-on-source case.
- **A physical/structural source that DELIVERS a modifier owns it where it is the deliveryguy.**
  - A route making an improvement better → the **route** is the deliveryguy → the boost lives on the route
    (Route #19: `ImprovementInfo.RouteYieldChanges` folds onto the route, keyed by improvement).
  - A building making a *terrain's* tiles yield more → the **building** is the deliveryguy → it **STAYS on the
    building** keyed by terrain (it is **NOT** folded onto the terrain — Terrain #20 carries no inbound boost).
    Likewise a building scaling with river-tile count owns *that* (`per`-scaled, on the building).
- **Plot-substrate entities (terrain, feature, improvement, route) CARRY THEIR OWN modifiers at `plot` scope.**
  The terrain forms the plot's base (hill → hammers); a feature then modifies it (forest: −food, +hammers);
  improvement/route layer on. Each owns its own contribution; a terrain/feature is the deliveryguy for its OWN
  intrinsic output, never for another entity's modifier. (A **bonus** is *not* a plot-modifier owner in this
  sense — it is a resource/conditioner sitting on the plot, above it in the spine.)
- **RIVER is a CONDITIONAL modifier, not its own entity** — a plot edge-attribute "just added on". Each
  river-side yield (`FeatureInfo.RiverYieldChange`, `ImprovementInfo.RiverSideYieldChange`,
  `BuildingInfo.RiverPlotYieldChanges`) stays on its **deliveryguy**, gated by the **`HAS_RIVER`** plot-state
  predicate. There is **no river field on `CvTerrainInfo`**.

So the discriminator is **ownership (who delivers)**, NOT whether the keyed entity is "conditioner" vs
"target". This **superseded the earlier "inversion" approach** (a deposit was inverted onto its target). *Flag
for the Building pass:* `BuildingInfo.{TerrainYieldChanges, ImprovementYieldChanges, …}` are deliveryguy-owned
by the BUILDING and should be authored on the building keyed by the target, **NOT inverted** — re-decide each
at the Building pass against this rule.

### 6.3 Related: keep-on-source for tech, and dedicated blocks

- A **tech**-conditioned effect is a downward deposit *from* the tech (§1) — no dilemma; the lower thing never
  reaches up with a `hasTech` gate. The one committed `BonusProductionModifiers → Bonus.buildRate` fold was
  **UN-FOLDED** to this rule (authored on the building/unit/project itself, gated/scaled by the bonus —
  home §6.4 `buildRate`).
- **DEDICATED BLOCKS** (the §0.8 complement): system-specific data lives in ITS OWN block, never scattered onto
  unrelated entities (feature line-of-sight → a `vision` block; each `PROPERTY_*` is its own family; a future
  Global Warming mechanic gets its own base object). Ownership (§6) says *which entity* holds a modifier; the
  dedicated-block rule says *system-coherent data clusters into its own block*. **Caveat:** PRODUCED data (a
  tile yield, produced by the terrain/feature on its plot) stays on the producing entity, tagged by a
  **system predicate** (the river yield lives on the terrain/feature, `commerce.plot.flat`, tagged
  `enabled: HAS_RIVER`).

### 6.4 `production` (total city OUTPUT) vs `buildRate` (build a TARGET faster)

Two distinct concepts the first-pass migration flattened into `production.city` (the "Versailles bug"), pinned
against the C++ (applied in different places):

- **`production` = TOTAL CITY OUTPUT** — `CvCity::getYieldRate100(PRODUCTION)`
  (`Sources/Engine/CvCity.cpp`): a hammer ADD (`production.city.flat`) or a city-wide multiplier on
  *everything* (`production.city.percent` — Factory, Power/Area/Capital yield-rate). Scales every build, every
  turn.
- **`buildRate` = FASTER TO BUILD A TARGET/CATEGORY** — `CvCity::getProductionModifier(eItem)` shrinks the COST
  of the *specific* item under construction; never the per-turn yield. Homes by what's produced:
  **`buildRate.self`** (build THIS faster, gated by a bonus — Versailles+marble),
  `buildRate.{scope}.{units|buildings|domains|unitCombats}.{TARGET}` (keyed),
  `buildRate.{scope}.{military|space|worldWonder|teamWonder|nationalWonder}` (category). The one-off
  `militaryProduction`/`spaceProduction` families fold into `buildRate`.

**Rule of thumb:** changes how fast a *particular thing* is built → `buildRate`; changes the city's *whole
hammer output* → `production.city`.

### 6.5 The HOME RULE — own-output vs governing-deliverer (owner refinements 2026-06-20)

> **⛔ Special cases are the root of all evil.** The model is **permissive but restrictive**: every legitimate
> effect lands in a category by a *rule*, never by a per-entity creative-liberty call. A field falls into exactly
> one of three categories — a **modifier family** (anything *contributed that accumulates*: yields, commerce,
> happiness/health, defense, movement-cost, range, …), an **enabler edge** (`enables`/`disables`/`obsoletes`/
> `replaces` + `requires`), or **identity** (a *genuine non-accumulated invariant* only). "Treat this one
> differently" is forbidden; the rule decides.

Within the modifier category, the home of a **cross-entity** modifier follows ONE rule:

> **A modifier lives on the entity that OWNS/GOVERNS the thing it modifies; a conditioning entity is *referenced*
> (`enabled`/`disabled`/`requires`), never the home.**

The rule produces two shapes — the discriminator is the §6 deliveryguy test: *does the other entity merely
ENABLE more of something this entity already produces (→ condition), or does it DELIVER/GOVERN the effect itself
(→ home)?*

- **own-output** — an entity's *own produced output* (a specialist's yield, an improvement's tile yield, a
  unit's strength). The producing entity is the home; tech/civic/trait/bonus/building are `enabled` conditions:
  - civic that boosts a Merchant's commerce → on the **specialist**, `enabled:{CIVIC active}` — **NOT on the civic**.
  - building that boosts a specialist → on the **specialist**, `enabled:{building in city}`.
  - tech that boosts an improvement's tile yield → on the **improvement**, `enabled:{tech}`.
- **governing-deliverer** — an entity that *delivers/governs an effect on others*. The acting entity is the home,
  keyed by target:
  - route that upgrades improvements → on the **route**, keyed by improvement(-group).
  - building that delivers a religion's influence → on the **building**, keyed by religion (`requires` the
    religion / the all-religions waiver — §6.2-style dormancy).

**⚠ Correction to §6.2's worked list:** `BuildingInfo.SpecialistYieldChanges` is **own-output**, not deliveryguy —
it is the *specialist's* output conditioned by the building's presence, so it lives **on the specialist**,
`enabled:{building}`, **not** keep-on-building keyed by specialist. (The curator double-author this surfaced is
[`../../json-migration/`](../../json-migration/README.md) Phase-F work.)

**Conditioner AXES (which reference form):** a **tech** conditions on the **enabling axis** (giving end, monotonic
→ `enabled:{tech}`); a **religion/resource** conditions on the **requiring axis** (receiving end, reversible →
`requires.operate` dormancy). The three layers — pass-1 `enables` gates / pass-2 `requires` / a modifier deposit's
`enabled`/`disabled` — are kept terminologically distinct (enabler [§2.0](enabler.md)).

**DATA ≠ RUNTIME (the seam is `readJson`).** The JSON is organised for **human** understandability — *one* home per
relationship, *one* uniform shape — and does **not** mirror the runtime cascade ("nobody cares about that except
the process itself"). `readJson` builds the links **both ways at parse** so the cascade reads top-down reliably;
any target-landing inversion is a **parse transform**, never a data-authoring shape. (This is *why* the
derived-data repository was retired — the parse-built scoped cascade IS the derived-aggregation mechanism;
[`../../architecture/superseded-ideas.md`](../../architecture/superseded-ideas.md).)

### 6.6 Movement & RANGE — two SEPARATE concerns (not one "reach")

Movement and range are different mechanics; forcing them under one abstraction tangles both → special cases. They
are **two independent families**, each uniform *internally* (keep-on-source, base group + modifiers, conditioners
via `enabled`):

- **`movement` = a LEDGER in one normalized currency, "movement points" (owner 2026-06-20).** Two sides, same
  unit: a unit-scope **credit** `movement` — *how many movement points the unit may spend* (base + modifiers) — and
  a **debit `moveCost`** — *how many movement points a move/plot charges*. Same currency ⇒ the ledger balances
  directly (spend points along a path until the budget is gone). **Normalization is the converter's job:** legacy
  smears scales (`iMoves` small ints, plot costs ×`MOVE_DENOMINATOR`, a road costing ⅓) → flatten ALL of it into
  clean integer movement points on both sides, so nothing downstream ever sees `MOVE_DENOMINATOR` again. Every
  contributor is then a credit or a debit in that currency: terrain + feature + hills = **+cost**; a route = a cheap
  **`min`** cost override; the hunter promotion's "−1 base movement cost" = a unit-side **−cost** discount (legacy
  `moveDiscount`/`extraMoveDiscount`); a movement promotion = **+points** credit — **floored** so cost can't go below
  the minimum. **Resolver rule (keeps current behaviour, owner 2026-06-20):** the ledger is NOT "must afford the
  full cost" — with **any** balance remaining (`> 0`) a unit may ALWAYS step to the next plot regardless of its
  `moveCost`, and **completing that step sets the remaining points to 0** (the classic "a unit can always move at
  least one tile" guarantee). The two-sided "inside+outside" lives ONLY here; both sides are quantified, uniform,
  modifier-driven. Route `TechMovementChanges` is a tech-`enabled` `moveCost` debit on the route, never identity.
- **`range` = one family (radius).** A unit-scope single value: *how far a ranged attack reaches*. It **defines
  "ranged-ness" unambiguously** (siege = `range:1`, true ranged = `2+`) and **deletes the engine's siege-vs-ranged
  special case** — every unit carries a `range`; the value decides reach. (Air/bombard reach are the same radius
  concept; per-kind runtime nuance like air round-trip stays in the resolver, not the data.) **The unification is
  CONFIRMED honest (verified 2026-06-20 vs `CvUnit::airStrike`/`rangeStrike`, `Sources/Engine/CvUnit.cpp`).** Ground
  ranged (`rangeStrike`/`canRangeStrikeAt`) and air (`airStrike`) are parallel functions, not one — but they share
  every load-bearing piece: both derive **reach from `airRange()`** (so ground "ranged-ness" *is* `iAirRange > 0` +
  `airBaseCombatStr() > 0`), both select the target via `airStrikeTarget`, cap at `airCombatLimit`, run
  `collateralCombat`, take no counterattack, and apply damage with the identical `max(dmg, min(dmg+X, limit))` shape.
  The **only** divergence is the damage-magnitude function (`airCombatDamage` vs `rangeCombatDamage`) — exactly the
  resolver-side per-kind nuance the model keeps OUT of the data. So one `range` family over `airRange()` is faithful;
  the converter's `iAirRange → range` is the right lever, and siege just needs `iAirRange` set on the ground unit.
  - **CONVERTER STATUS (landed 2026-06-20): air-only.** `curate_unit` lifts `iAirRange → range` (top-level family,
    human tile value, `unit` scope; the ×100 is the engine's parse job, data≠runtime) — 53 air units. `iAirRangeChange`
    is a *change* field → a promotion-side `range` modifier (standard shape, `curate_promotion`), not a unit base.
  - **`range:1` for siege is DEFERRED, deliberately (owner 2026-06-20).** Generalizing range to ground units is a
    *data* one-liner but has **AI implications — the AI runs ranged combat very poorly** — so it stays out until the
    AI can drive it. The model is uniform (range universal); only the *rollout* is gated by AI capability, not a gap.

**Conversion = a straightforward renumber (owner 2026-06-20).** Both axes are ×100 fixed-point like every other
family, so the converter just rescales legacy numbers and lets the standard modifier machinery carry the rest:
- **movement points = `iMoves` × 100** — DECIDED: `base = 100 = MOVE_DENOMINATOR` (owner 2026-06-20). This is not an
  approximation we introduce; the engine **already** computes movement in integer `MOVE_DENOMINATOR` units and merely
  *displays* a fraction, so base-100 is exactly the current math. `range` base = the unit's range × 100.
  - **Known baseline (owner 2026-06-20): a road lets a 1-move unit cross 3 plots ⇒ a road tile costs ≈ `base / 3`.**
    The base does **NOT** need to be divisible by 3 — the always-move-one rule + the **clamp-to-0 on the final move**
    absorb the remainder, so `base/3` may be approximate and still resolve to 3 plots. A normal flat plot costs the
    full `base` (= one move). That single fact anchors the base choice and the terrain/feature normalization.
  - **Scaling is linear (owner 2026-06-20):** budget = `iMoves × base`, road = `base/3`, so a 1-move unit crosses 3
    road tiles, a 2-move unit 6, an N-move unit `3N` — the same arithmetic covers every route tier.
  - **The finest late-game route, and why 100 still fits (owner 2026-06-20).** Some late routes let a 1-move unit
    cross ~11 tiles (road ≈ `100/11 ≈ 9`). At base 100 that rounds — but the error is **≤ ±1 plot at the extreme end,
    and at 11 tiles nobody notices ±1**. Gameplay > precision, so 100 is accepted: no larger base or exact
    divisibility needed (the clamp-to-0 already absorbs the remainder).
- **a route REPLACES terrain/feature on `moveCost`** (the verified override, kept deliberately so movement cost stays
  consistent), then **terrain/feature costs are normalized to the chosen base afterward** so non-route distances match legacy.
- every *change* (promotion move/range bonus, etc.) is the **standard modifier shape** over that base — a normal
  `movement`/`range` flat/percent deposit, `enabled` as usual — **no bespoke path**.
- **`moveCost` combine (RE-VERIFIED line-by-line vs `CvPlot::movementCost`, `Sources/Engine/CvPlot.cpp::movementCost`,
  2026-06-20 — supersedes the looser earlier note; the **full decomposition** is now dumped + cross-checked live by
  [`/diagnostic/movementSweep`](../observability/http-server.md), so every claim here is a NAMED component with numbers,
  not a guess). `MOVE_DENOMINATOR = 100` (verified, `GlobalDefines.xml`). The function is a branch tree, NOT one sum:
  - **Early returns:** a `flatMovementCost()` or `DOMAIN_AIR` unit costs exactly `MOVE_DENOMINATOR` (the *only* branch
    skipping the trailing `max(1)`); an unrevealed-to-a-human or invalid-domain-for-location plot costs `maxMoves()`;
    invalid-domain-for-action costs `MOVE_DENOMINATOR`.
  - **ROUTE OVERRIDE branch** (both plots routed AND (no river-crossing OR bridge-building tech)):
    `cost = min(MOVE_DENOMINATOR, min(routeCost, routeFlatCost))` — a TWO-term min, not one. `routeCost =
    route.getMovementCost() + team.getRouteChange(route)` (taking the **max** of from/to when they differ);
    `routeFlatCost = max(fromFlat, toFlat) × baseMoves`. **The per-tech route delta is `CvTeam::getRouteChange`, NOT
    "Σ getTechMovementChange"** (correction). The route REPLACES the terrain path with its own cheaper cost.
  - **REGULAR (terrain) branch** — ADDITIVE pre-denominator stack: `terrain + feature + HILLS_EXTRA_MOVEMENT(hills) +
    RIVER_EXTRA_MOVEMENT(river-crossing) + PEAK_EXTRA_MOVEMENT(peak ∧ ¬moveFastPeaks) + 3(peak, unconditional)`; then
    `max(1, sum − getExtraMoveDiscount())` (the unit −cost discount); then `× MOVE_DENOMINATOR`; then a **double-move
    divisor** — `/4` for feature/hills double-move, `/2` for terrain double-move; then **floored at `max(90, …)`**.
  - **⚠ Two facts the model previously omitted, both load-bearing for the converter:** (1) the regular-branch floor is
    **90, NOT `MOVE_DENOMINATOR`/clamp-to-0** — 0.9 of a move is the cheapest a *terrain* tile can cost (distinct from
    the unit-side always-move-one resolver clamp above, which zeroes *remaining points* after a step — do not conflate
    the two clamps); (2) **double-move** (`isFeatureDoubleMove`/`isTerrainDoubleMove`/`isHillsDoubleMove`,
    promotion-driven) halves/quarters the terrain cost — a real `moveCost` modifier the converter must carry.
  (The "divider" intuition comes from the UI showing *speed* as `MOVE_DENOMINATOR / moves`; the underlying *cost* math
  is the branch tree above.) [`legacy-value-calc-map.md` §10.1](legacy-value-calc-map.md) carries the same
  decomposition (independently verified) — they agree.
- today **only airplanes carry `range` modifiers** — every other unit is base-only. `CvUnit::airRange()` sums **four**
  contributors, not two: `UnitInfo.getAirRange() + getExtraAirRange() + team.getExtraMoves(DOMAIN_AIR) +
  national{Missile|Flight}RangeChange` (the missile-vs-flight national split by special-unit type). The air-only
  converter today lifts the unit base (`iAirRange`) + the promotion *change* (`iAirRangeChange`); the **team-extra and
  national contributions are additional modifier sources still to map** (verified 2026-06-20).

Both are the **unit-plane + plot-cost subsystem** (the large, later surface): curators classify movement/range
fields *into* these homes, built when that subsystem lands — never special-cased to identity meanwhile.

**Validation = observe the legacy, never hand-hunt units (owner 2026-06-20).** Before converting, the legacy
movement/range is made OBSERVABLE from the running game — dump each unit's effective movement points + `range` and
each plot's `moveCost` (with the discount / `min` / floor decomposition) — so the converter is diffed against
ground truth **systematically** (a movement/range shadow, the magnitude analogue of `/diagnostic/modifierSweep`),
never by hunting specific in-game unit cases by hand. That observe-then-shadow step is the map-before-delete bar
([DEC-map-before-delete](../../architecture/decisions.md#dec-map-before-delete),
[DEC-obs-scale](../../architecture/decisions.md#dec-obs-scale)); skipping it is exactly the "cast iron" case-by-case
slog it avoids.

**STATUS — the OBSERVE surface is BUILT (2026-06-20).** Phase-3 step-1/2 landed: per-unit effective movement points +
`range` are on **`/units`** (`baseMoves`/`maxMoves`/`movesLeft`/`moveDiscount`/`range`/`domain`), and the systematic
per-`(unit, edge)` `moveCost` decomposition (every component above + the engine's own `movementCost()` as the
authority, with a per-edge self-check) is the new **`/diagnostic/movementSweep?type=full&player=N`** endpoint —
the magnitude analogue of `/diagnostic/modifierSweep`, in `Sources/Tools/CvHttpServer.cpp`
([http-server.md](../observability/http-server.md)). The decomposition is self-validated against the engine's own
`movementCost()` per edge: **`edgeMismatchHuman == 0`** (verified live 2026-06-20, 2248 human edges) proves the mirror
is faithful.

> **⚠ MAPPED ENGINE QUIRK — the non-human `movementCost` result-cache collides (verified 2026-06-20, via the new
> sweep + `CvUnit::m_movementCharacteristicsHash`).** `CvPlot::movementCost` caches its result **only for non-human
> units**, keyed by `m_movementCharacteristicsHash` — and that hash folds in only the base-unit zobrist + promotions/
> unitcombats flagged `changesMoveThroughPlots()`, **NOT `getExtraMoveDiscount` nor `baseMoves`**. So AI units differing
> only in move-discount/base-moves share a cache entry and return **each other's** `moveCost` (observed: three
> `UNIT_BTR80` with `baseMoves` 4/3/4 and discount 2 vs 1 all returned the same cached cost). The sweep surfaces this
> as a small `edgeMismatchNonHuman` (the AI-cached `engineCost` ≠ the fresh formula `cost`). **Consequence for phase-3
> step-3/4:** the movement shadow MUST diff the cascade against the **FRESH** cost (the decomposition / human path),
> never the AI-cached `engineCost`, or it will chase phantom cache-collision divergences. (A *pre-existing* legacy
> imperfection the observe surface exposed — exactly its job; not a converter or emitter bug.)

**Model VALIDATED against the live distribution (2026-06-20, via the sweep across a human + 2 AI players, ~29k
edges).** The observed `moveCost` landscape is exactly what the formula predicts: a discrete ×100-anchored set —
`100` (terrain=1 single move), `200/300/400` (terrain 2/3/4), the `90` floor, and sub-100 route values
(`12/16/24/32/36/40` = route-type cost + tech changes) — with no surprise magnitudes. Notable for the converter:
**route-branch edges DOMINATE (~73%)** (so the route `min`-override is the hot path, not an edge case); **the unit
−cost discount affects ~40% of edges** (discount 1–2 — a major factor, not rare); double-move is marginal
(~1.6% `/2`, ~0.01% `/4`); range is air-only (~1.8% of units: fighters, range 5/11/12). Real road costs are 24–40,
NOT a uniform `base/3≈33` — the model's "base/3" is an approximation; the converter normalizes the actual
route-type values.

**STATUS — the CASCADE RESOLVER + SHADOW are BUILT (cut 1, 2026-06-20, Assert-clean).** Phase-3 step-4 landed the
plot-substrate channel: `Sources/Cascade/CvCascadeMovement.{h,cpp}` is a `moveCost` **resolver** that reproduces
the `CvPlot::movementCost` branch tree but sources the **terrain/feature/route** cost from the migrated JSON
(`identity.movementCost` + route `identity.flatMovementCost`), and `/diagnostic/movementSweep` now carries the
**cascade-vs-legacy diff column** (`cascadeCost`/`cascadeDelta`/`cascadeCause`/`cascadeCare` per edge + the
`cascadeDiverge` count, cause/care histograms, and `cascadeSubstrate` parse stats). The shadow diffs the cascade
against the **FRESH** legacy `iFinal` (the `cost` field), never the AI-cached `engineCost` (the cache-collision
quirk above). **Cut-1 scope:** the resolver reads the **unit-side** aggregate (baseMoves, moveDiscount, the
double-move/ignore/flat predicates) and the hills/river/peak/denominator globals from the engine/GC — those are
the unit-plane MODIFIER FAMILY (the self-accumulator, "largest surface, last") and the Tier-G config globals,
neither migrated yet — so a divergence **localises to the terrain/feature/route migration** (exactly the deletable
plot-substrate reads). The route per-tech delta (`CvTeam::getRouteChange`) likewise stays engine-read (a
tech-`enabled` `moveCost` modifier, not yet migrated).

**STATUS — the UNIT-PLANE movement/range shadow is BUILT (cut 2, 2026-06-20, Assert-clean).** The unit-side
credit/discount/range/caps are the modifier-family slice; `cascadeUnitMoveAgg` (in `CvCascadeMovement`)
reconstructs the engine's `m_iExtra*` stack from the migrated JSON — summing the unit **type** + each **promotion**
+ each **unitcombat** the live unit has (`movement.unit.{moves,moveDiscount}`, `range.unit`/`air.unit.range`, the
`capabilities`; double-move is TYPE-KEYED). `/diagnostic/movementSweep` per-unit now carries the **shadow**
(`cascadeMovesMigrated`/`engineMovesMigrated`/`cascadeMovesDelta`, the discount + range twins, the cap bools) vs the
engine's **migrated** parts (`UnitInfo` + own `getExtra*`), plus the summary `cascadeUnitDiverge` + cause/care
histograms + `cascadeUnitData` parse stats — AND the **META** rung `cascadeSources[]` (each contributing
type/promotion/unitcombat's exact contribution: which promo gave +1 move, which unitcombat the keyed double-move).
The team/national scopes + the commander/commodore cross-edge (`getExtraMoves` folds them) + the flying-runtime
`canFliesToMove()` OR are engine-only (NOT migrated) and deliberately excluded from the migrated diff — a
commander unit is cause-tagged `commanderCrossEdge` (care Weird, expected), so a real `moves`/`moveDiscount`/`range`
divergence isolates the unit-plane data migration. **Remaining unit-side data to migrate** (the shadow's expected
residue): `team.getExtraMoves(domain)` (team-scope), the four-term `airRange` team/national contributions, and the
route `CvTeam::getRouteChange` tech delta — each a separate modifier-source migration.

### 6.7 Specialist COUNTS — two independent additive families (owner 2026-06-20)

**Framing — a specialist is a VOLUMETRIC building (owner 2026-06-20).** A specialist is "almost a volumetric
resource that produces its own output" — most accurately a *volumetric building*: where a building is 0-or-1 (you
have it or you don't), a specialist is 0-to-N (a stackable volume), and **each instance produces building-like
output** (the same modifier families — food/production/commerce/… — authored on the specialist, the §6.5 own-output
that `SPECIALIST_BOOSTS` now lands in `enabled` form). The clean-design consequence: **the per-instance OUTPUT is
SEPARATE from the COUNT of instances.** Two orthogonal axes —
- **output** = the specialist's modifier families (what ONE instance contributes), and
- **count** = the families below (how MANY instances) —

and the tally rolls up `count × per-instance-output`. Keeping them apart is *why* `freeSpecialists`/
`allowedSpecialists` are their own count families and never entangle the yield families.

Two distinct per-specialist count families, both **additive**, keep-on-source, conditioners via **`enabled`** (never a
keyed sub-scope — the keyed-conditioner cleanup, §6.5):

- **`freeSpecialists: { <scope>: { any: N, SPECIALIST_X: M, … } }`** — *granted free* specialists: an **`any`**
  assignable-slot bucket plus specific auto-assigned types. (Half-modelled today as three shapes — a bare `flat`
  ⇒ `any`; a `buildings.{B}`/`improvements.{I}` granter ⇒ an `enabled` condition; the gnarly tech
  `buildings.{UNIVERSITY}.specialists.{SCIENTIST}` ⇒ `freeSpecialists.<scope>.SPECIALIST_SCIENTIST` with an
  `enabled` clause. The uniform object dissolves all three — and the deferred `TechSpecialistChanges` flag with them.)
- **`allowedSpecialists: { <scope>: { SPECIALIST_X: N, … } }`** — the *cap* on how many pop may be **manually
  assigned** as each type (per-type only; no `any` — a cap is inherently per-type).

**Leaf spelling — (A), the specialist-type IS the leaf key (owner ruling 2026-06-20).** Specialists are their own
"yield-ish" thing, so this family carries its OWN shape — *provided* it stays unambiguous, uses the system's own
keywords (`enabled`/`value`/the scope names), and `freeSpecialists`/`allowedSpecialists` share ONE structure. Under
the scope, the key is `any` or a `SPECIALIST_*` type; its value is the **count** — a bare int when unconditioned,
a `[{value, enabled}, …]` list when conditioned (e.g. an improvement- or tech-gated grant). It's an additive object,
easily read: `freeSpecialists.city = { any: 2, SPECIALIST_SCIENTIST: [{value:1, enabled:{…}}] }`. (Chosen over the
`.flat`-unit spelling for terseness + legibility; the count-by-type leaf is the one sanctioned departure from the
`<scope>.<unit>` leaf, and only because specialists are their own category.)

**Free lives ON TOP of allowed (owner ruling 2026-06-20):** a free specialist is *additional* and does **not** consume
an `allowedSpecialists` slot. This is a deliberate simplification of legacy (where a free specialist could take an
allowed slot), accepted under [DEC-parity-not-goal](../../architecture/decisions.md#dec-parity-not-goal) — the two
families are then **fully independent**, no interaction to model.

---

## 7. Resolved accommodations (combine modes, temporal, resolvers)

The grounding's "accommodations" mostly collapse into the keystone / `per` / engine-boundary. The residue is
**engine behaviour (not info)**, solved when the cascaders are built (#430) — it does NOT block the migration:

- **Non-additive combine.** (a) clamp-on-channel-total (`iMinDefense` floor, caps) is specified **in the
  family's own structure** (`defense` declares additive `amount` + a `min` member that floors the total).
  (b) min/max-across-sources (anarchy turns, yield thresholds, `naturalDefense`, `noEntryLevel`) → a channel
  **`combine` mode** (`sum` default | `max` | `min`), not a unit. (c) REPLACE/SET: culture caps/radius →
  `identity`; **no `override` unit**. (d) RNG/probability params → not modifiers, classify OUT.
- **Temporal.** Per-turn emission uses `per:{type:TURN}`; `CommerceChangeDoubleTimes` "doubling" is a second
  **age-gated `enabled` deposit** (`enabled: { existedFor: { min: N } }`, read from a created-timestamp). **No
  `timers` section, no non-additive engine stage.**
- **Event-pulses → `grants`**, fired by the engine event-hook system the tally already requires (one
  infrastructure for tally-maintenance + grant pulses). The hooks are engine machinery, NOT on the Infos.
- **Combination semantics** (`combineStyle` rate/cost; `polarity` sign-split; `flatPlacement`
  base/postMultiplier; the `combine` mode) are **ENGINE behaviour, not info** — solved at #430 with the
  structured data in hand.
- **Genuine Type×Type 2D residue:** the ~18 invisibility/visibility LOS tables → a non-cascade structured
  sub-object read by the visibility **resolver**; `FreePromotionUnitCombatTypes` → a `grants` free-promotion
  grant. (Hide-and-seek "best-of" is resolver-side mechanics, not a cascade combinator.)

---

## 8. Drops, relocations, and the demolition map

**Spatial leakage → #429 (redesigned, not cut).** The property `NEAR` radius-diffusion (crime/disease/
pollution leaking city↔plot — inert today) is dropped from the containment cascade and rebuilt in the dedicated
"sideways static influence" pass; the crime/disease unit→city emission is preserved via containment. **Adjacency
YIELD** (4 farms buffing each other) is a *future want* built fresh in #429. Threshold pseudobuildings
(`BUILDING_EFFECT_*`) are **kept** (reworked to `PropertyEffect` later). **Vicinity bonus** = a presence check
(enabler/`enabled`), NOT leakage — stays. **Policy: any spatial/adjacency mechanic is #429's — never
accommodated in the #428 modifier structure.**

**The dead-list re-splits four ways** (re-verify each against `Assets/Python` AND intent — the grounding only
checked C++):

- **(i) TRULY dead → drop:** Building `iMaxPopulationAllowed`/`iMaxPopulationChange`; Bonus
  `m_piImprovementChange`; Promotion `iDamageperTurn`/`iWeakenperTurn`/`iStrAdjperTurn`; UnitCombat
  `m_PropertyManipulators`; PromotionLine `*ContractChanceChanges`; root Improvement `m_iDepletionRand`.
  - *Clarification:* Building `iNukeExplosionRand` is **live-CODE but dead-DATA** (the meltdown mechanic runs
    every turn — `CvCity::doMeltdown` from `doTurn` — but the field is populated ONLY by the EXCLUDED
    `Bad_Karma/Building_Meltdown` module, so no INCLUDED building sets it → **not emitted**, effectively
    dropped). The third category: live-code + excluded-module-only-data.
- **(ii) UNWIRED MODIFIERS (legit intent, never wired):** Tech `FreeSpecialistCounts` → **revive** as
  `freeSpecialists`; Building `iPillageGoldModifier` → **revive** as `pillageGold.empire.percent` (the
  empire-wide UNIT-modifier shape, the intended vehicle for future wonder-influence features); Project
  `YieldModifiers` → **drop**.
- **(iii) UNWIRED WORLD-STATE FEATURES → separate future issues:** `bNoAnimals`, the Era/vote world-state bools
  — wanted, but not modifier data, not #428.
- **(iv) DELIBERATE BALANCE CUTS:** `healthPercent` the *modifier* is wanted (the `health` family supports
  `percent`), but it is **not sourced from improvements** ("a balancing nightmare") — keep the capability, cut
  the source.

**Relocated (moved, not deleted):** → `grants`: Building population/golden-age pulses, Civic `iRevIdxSwitchTo`,
Unit `GroupSpawnUnitCombatTypes`. → enabler `requires.operate`: Building `iMaxPopAllowed` (dormancy). →
`enables`/`requires`: Project `iTechShare`, Vote world-state bools. → `identity`: Civic `iAnarchyLength`,
Terrain `iMovement`+`Yields`, CultureLevel `iCityRadius`, UnitCombat `bForMilitary`/`bForNavalMilitary`. →
`ai`: LeaderHead (~90 fields). → tech (inverted): Route `TechMovementChanges`. → probability/identity (RNG,
out of cascade): Improvement `iAirBombDefense`, `DiscoverRand`.

**Demolition list (what the cascade DELETES — parallel to the enabler's).** The legacy imperative maintainers
the modifier cascade replaces, grounded against `Sources/Engine/` (functions, not line integers — names drift):

- `CvCity` yield/commerce accumulator + per-building reads (`getBaseCommerceRateFromBuilding100`,
  `getBuildingYield`) → the split yield/commerce cascade accumulators.
- `CvCity` health/happiness accumulators incl. good/bad-split → health/happiness families with reader-side
  `polarity=signed-split`.
- `CvCity` defense channel reads (`getDefenseModifier`/`getNaturalDefense`/`getTotalDefense`,
  `changeExtraMinDefense`, `setMinimumDefenseLevel`) → defense family (clamp in the family structure).
- `CvCity` / `CvPlayer` maintenance + upkeep → maintenance/upkeep families (cost-style combine).
- `CvPlayer` trait/civic processing (`processTrait`, `setCivics`, ~150 `change*` calls) → empire-scope deposits
  from the civic/trait JSON; setters deleted.
- `CvTeam`/`CvPlayer` `processTech` → team/empire downward tech deposits (§1).
- `CvCity` `processBuilding`/`processSpecialist`/`processBonus`/`processCorporation` → the unified containment
  cascade; per-entity loops deleted.
- `CvCity` tech-conditioned recompute loops → downward tech deposits.
- `CvCity` power-/state-religion-gated sub-accumulators → per-deposit `enabled` / entity `requires`.
- `CvUnit` promotion/unitcombat extra-stat stack (~200 `changeExtra*`/`setHas*` setters) → the unit-plane
  self-accumulator (§5).
- `getModifiedIntValue` cost-asymmetry sites → `combineStyle=cost` family-metadata applied once per cost
  channel.
- `CvArea` per-player good/bad health/happiness pools → area-scope deposits.
- `CvGame` world-scope vote/project/tech accumulators → world-scope deposits.
- `CvCity` `CommerceChangeDoubleTimes` post-sum ×2 path → a second age-gated `enabled` deposit.
- `CvPlayer` MIN/MAX combinator updaters (`updateMaxAnarchyTurns`/`updateMinAnarchyTurns`/`update…YieldThreshold`)
  → `combine: max|min`.
- `CvGameTextMgr` upward cross-entity pedia SCANS → the derived reverse index (cold path, built on load from the
  forward deposits).

**Status:** the demolition list is the *plan* — nothing here is cut until its behaviour is shadow-clean
([DEC-map-before-delete](../../architecture/decisions.md#dec-map-before-delete)).

---

## 9. Source & governing rules

This doc consolidates `docs/dev/plans/modifier-cascade-spec.md` (v3, owner-LOCKED 2026-06-15). The spec's §0
governing rules ("the spine") are summarized in §1 above; the build + shadow-test plan is
`modifier-cascade-shadow-spec.md` (the parity-first scaffold, the city-yields pilot,
`/diagnostic/modifierSweep`, the Fine→Meltdown care scale). The two spec-level reconciliation flags carried
forward:

- **§0.7 unit-vs-combine flag (PENDING owner pass):** the value unit names what the value IS (`percent` for a
  percentage even if it scales multiplicatively); the combine *behaviour* is family metadata. The
  "additive-delta-summed vs composed-by-product" phrasing in §2.3 describes combination, which is §7 engine
  metadata — until the owner pass separates them, classify the unit by the datum's nature and let the family's
  combine-mode carry the math. (This flag caught GameSpeed `iSpeedPercent` mis-authored `multiplier`; it is
  `percent` = "1000%".)
- The old field→modifier mapping (`modifier-cascade-mapping.json`) still renders pre-alignment
  `when`/`perCountOf`/`count`-leaf shapes — it must be re-pointed to the LOCKED `enabled`/`disabled` + `per`
  shapes before it feeds a curator. (Migration-internal; tracked in plans.)

---

## See also

- [`../../explanation/cascade-architecture.md`](../../explanation/cascade-architecture.md) §3 — the modifier
  OVERVIEW (the *why*, and how the three machines + event spine fit). This doc is the deep detail under that.
- [`fixed-point-and-scales.md`](fixed-point-and-scales.md) — the **scale registry**: the human↔×100 convention,
  the `flat`/`percent`/`multiplier` unit table, and the per-field de-scale rules referenced from §2.3/§3. Don't
  re-derive a scale; link this.
- [`data-model.md`](data-model.md) — the full authored JSON shape (reserved sections, the shared
  atom/condition/scope vocabulary, the `per`/`interval` scalers, `grants`). This doc details the *modifier
  families*; that doc is the whole entity surface.
- [`../../architecture/decisions.md`](../../architecture/decisions.md) — the rulings ledger.
  [DEC-deliveryguy](../../architecture/decisions.md#dec-deliveryguy) is **homed in §6 here**;
  [DEC-fixedpoint-x100](../../architecture/decisions.md#dec-fixedpoint-x100) (value scale) and
  [DEC-parity-not-goal](../../architecture/decisions.md#dec-parity-not-goal) (parity not the bar) govern §3.
- [`../../README.md`](../../README.md) — the comprehension map / overview-of-overviews.
