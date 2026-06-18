# Modifier cascade — formal spec (v3, owner-LOCKED 2026-06-15)

> **STATUS: structure LOCKED (owner alignment session 2026-06-15).** Companion to
> `enabler-cascade-spec.md` — the enabler answers *"can I"*; the modifier answers *"how much"*. Grounded by
> the `modifier-cascade-grounding` workflow (1062 fields / 33 entities), then walked to a settled object
> structure + vocabulary. Engine *combination arithmetic* and a few named cross-cutting pieces pin at #430
> (§9); the **data structure** here is fixed.
>
> The old field → new-modifier mapping is in `modifier-cascade-mapping.json` (this dir). **⚠ that file still
> renders the pre-alignment `when`/`perCountOf`/`count`-leaf shapes — re-point it to the LOCKED
> `enabled`/`disabled` + `per:{type,scope}` shapes (this doc) before it feeds a curator.**

---

## 0. Governing rules — the spine

1. **Strict ENABLER / MODIFIER separation.** The enabler decides *availability* ("can this be / is it
   operating"); the modifier decides *magnitude* ("how much"). Neither leaks into the other. *Worked
   example — Size Matters:* the **game option** + the **merge action** are **enabler** (availability;
   `GAMEOPTION_SIZE_MATTERS` is load-stable → `loadPrune`); the **size promotion** merging applies is a
   **modifier** (unit-plane stat deposit via the existing promotion structure). Two cascades, no overlap.
2. **Enabler objects are reused VERBATIM and are STABLE.** Wherever a condition is needed, the modifier side
   embeds the enabler's `requires`-style object unchanged (same `all`/`any`, `min`/`max`, presence atoms,
   object serialization). The enabler structure changes only by a deliberate, conscious decision to alter
   the enabler itself — never as a side effect of modifier work.
3. **The modifier cascade is purely TOP-DOWN; it never requires UPWARD.** Sources deposit DOWN the
   containment spine; targets read an O(1) summed accumulator; the reverse index is cold-path/pedia only. A
   condition embedded in a deposit is a **forward HAS-membership read**, not an upward cascade-walk.
4. **Tech-inflation is a DOWNWARD deposit, not a gate.** "Researching tech T makes everything below better"
   is the tech depositing down onto its targets (authored on the tech, §6/CREST). It is never the lower
   thing reaching up with a `hasTech` gate.
5. **Drop only CLEARLY-dead data; KEEP the ambiguous.** Clearly dead (grep-confirmed no consumer, or a
   vestige doing nothing) → drop. Ambiguous (possibly live, gated-but-functional, purpose-unclear) → keep,
   migrate faithfully. If a dropped thing is ever needed, reintroduce it in the proper structure — never
   resurrect a vestige just-in-case. *(See §8 for the four-way drop framework this produced.)*
6. **Info DATA vs engine MACHINERY — a hard boundary.** Infos (the JSON) carry only **values, payloads, and
   relationships**. The machinery that *consumes* them is engine-side and never authored in the data: the
   **producers/creators** (the "create unit" subroutine — default promotions, starting XP, Size-Matters
   base ranks), the **evaluators/resolvers** (combat; hide-and-seek / visibility detection best-of), the
   **event-hook dispatch** (fires `grants` + maintains the `tally`), and the **tally** itself. Litmus: a
   "producer/creator/evaluator problem, not an info problem" does NOT enter the cascade data.
7. **The DATA leads; the engine is reworked to fit it; the file must read cold to a MODDER (owner,
   2026-06-15).** Three coupled rulings: (a) **author the unit/shape for what the datum IS, not how today's C++
   fetches/combines it** — a percentage-valued field is `percent` even if the engine currently applies it
   multiplicatively (that combine behaviour is §7 family metadata, reworked to fit; it is NOT the per-value
   unit). (b) **The C++ data-fetching is reworked to fit the JSON, NEVER the JSON reshaped to match existing
   fetching** — reading the consumer tells you what the datum MEANS, never how to conform to it. (c) **The JSON
   must make sense to a modder reading the file COLD**, with zero codebase knowledge — self-explanatory keys and
   values; a shape that only parses if you know the engine is wrong, simplify it. *(These caught a real error:
   GameSpeed `iSpeedPercent` was mis-authored `multiplier` by reverse-engineering the engine's `×p/100` product;
   it is `percent` — "1000%". See the §1.4 reconciliation flag.)*
8. **DEDICATED BLOCKS — system-specific data lives in ITS OWN block, never scattered onto unrelated entities
   (owner, 2026-06-16).** When a datum belongs to a specific *system*, it is grouped under that system's dedicated
   block (a family/section, or its own Info/base object), not bolted onto whatever entity happened to hold it in the
   legacy data. Worked cases: feature line-of-sight → a **`vision`** block (`iSeeThrough` → `vision.plot.seeThrough`),
   grouped so the rest of the vision rework lands beside it; a future **Global Warming** mechanic gets its OWN base
   object for its data (`WarmingDefense` would live there, not on `CvFeatureInfo` — so #428 simply drops the dead
   field); each `PROPERTY_*` is its own family. The complement to the §6.1 deliveryguy rule: ownership says *which
   entity* holds a modifier; this says *system-coherent data clusters into its own block* rather than smearing across
   the entity that legacy code parked it on. Keeps each system's data discoverable and self-contained.
   - **The aspiration (owner, 2026-06-16): MODULAR SYSTEMS with ISOLATED SURFACES.** A system whose data is its own
     block can be developed, swapped, or removed *as a unit* without touching the entities it decorates — add or
     retire Global Warming wholesale via its own object; rework vision behind the `vision` surface; a modder extends
     one system without reading the rest. The dedicated-block discipline is what makes the surfaces isolated.
   - **CAVEAT — PRODUCED data lives WHERE IT'S PRODUCED, tagged by a system PREDICATE (owner, 2026-06-16).** A datum
     the engine reads at a specific *location* — a tile yield, produced by the terrain/feature on its plot — cannot
     move into an abstract system block; it stays on the producing entity. Its system membership is expressed instead
     by a **system-specific predicate** on the deposit: the river yield lives on the terrain/feature
     (`commerce.plot.flat`), tagged `enabled: HAS_RIVER` (the river system's surface), NOT relocated to a "river
     block". So the split is: **pure system data → its own block** (`WarmingDefense`, `vision`); **produced data →
     stays put, gated by the system's predicate** (enabler-spec §3).
   - **TOP-LEVEL KEYS ARE CATEGORY OBJECTS; an object-module's PRESENCE is its on/off signal (owner, 2026-06-16).**
     Two coupled rules for the dedicated blocks: **(a)** a top-level key should *almost always* be a **category-like
     object**, not a bare scalar — rare exceptions aside (the universal reserved text fields `type`/`description`/
     `help`), keeping the top level to categories is what keeps the structure coherent and a system queryable/purgeable
     as a unit. **(b) OBJECT-MODULE ACTIVATION:** a module's block **IS** its boolean — the module is **active iff its
     object EXISTS and is NON-EMPTY**; **absent or empty ⇒ false**. So a module needs **no separate `active`/`enabled`
     flag** — presence carries it. *Worked example (PromotionLine #27): `buildUp` is a dedicated object module; a
     build-up line is marked by the `buildUp` object's presence. (It currently carries an interim `buildUp.active:true`
     only because the line record holds no other build-up data — WHAT it builds up lives on its promotions, added at
     the Promotion pass; once present, `active` is redundant under this rule and drops.)* The corollary: don't emit an
     empty module object as a marker (it reads as false) — give it content, or it shouldn't be there.

---

## 1. ★ THE STANDARDIZED OBJECT STRUCTURE & VOCABULARY — the core

**The whole point of the migration: ONE object structure and ONE shared vocabulary, reused everywhere —
modifier deposits, `grants`, conditions, count-scaling. They are not separate shapes; they are the same
composition.** Nailing this is what ends the JSON re-flopping.

### 1.1 Top-level object — reserved sections + flat modifier families

An entity's top level is a fixed set of **RESERVED SECTION keywords** — `enables` · `obsoletes` ·
`replaces` · `requires` · `grants` · `text` · `cost` · `ui` · `world` · `sound` · `identity` · `ai` — and
**every other top-level key is a MODIFIER FAMILY**
*(the former single `art` section was split into the three dedicated top-level blocks `ui` / `world` / `sound`
per §0.8 — `art` is a SUB-block within `ui`/`world` so non-art members sit beside it; canonical tag→path map is
`curate_common.ART_BLOCK`)* — (`food`, `production`, `commerce`, `gold`, `research`, `culture`,
`espionage`, `happiness`, `health`, `growth`, `buildRate`, `maintenance`, `upkeep`, `defense`, `combat`,
`tradeRoutes`, …, plus one per `PROPERTY_*`). **No `modifier:` wrapper** (minimal nesting). The reserved set
makes "section vs family" deterministic — a family named like a reserved word is an explicit build-time
error, never a silent clash. The reserved list is **non-exhaustive / provisional** (grows as new sections
appear); no family↔reserved overlap exists today (2026-06-15).

### 1.2 A family addresses as

`<family>.<scope>.<targetType>.{TARGET}.<member>.<unit>` = value
(`<targetType>.{TARGET}` omitted for scope-wide deposits — the identifier lives in the family name;
`<member>` omitted for single-concept families.)

### 1.3 An entry (deposit / grant) — one shape everywhere

```
{ <payload>, scope?, per?, enabled?, disabled? }     // scope/per/enabled/disabled OPTIONAL, default no-op
```

- **payload** — a **modifier magnitude** (a `flat`/`percent`/`multiplier` value) **OR a grant** (`type` +
  `count`).
- **`scope`** — where it applies (default: the entry's containing scope).
- **`per`** — count-scaler `{ type, scope }`, default ×1 (§4).
- **`enabled` / `disabled`** — the enabler condition object, verbatim; default-absent (§3).
- A leaf is a single entry **or a cumulative LIST of entries**.

**`ai` — the AUDIENCE qualifier (OPTIONAL leaf-level; owner extension 2026-06-15).** At any leaf, the bare unit
keys (`flat`/`percent`/…) are the **base, applying to ALL players**; an OPTIONAL sibling **`ai` block** (same
inner `<unit>: value` shape) holds an **AI-only** deposit — an extra modifier stacked for AI players, or the
sole value for an AI-only field. Default-absent → applies to all. Example: `unit: { percent: 100, ai: { percent:
120 } }` = base unit-upkeep 100% for everyone, AI players an additional ×120%. Born from the Handicap human/AI
difficulty split (the own-vs-game-handicap SOURCING that decides which record a player reads is **engine
fetching, not data**, §0.7). The audience axis is `all` (bare, default) vs `ai`; a `human`-only audience is not
yet needed (add consciously if a case appears).

### 1.4 Vocabulary — shared by all of the above

- **Types** = data Types (`BONUS_*`, `UNIT_*`, `RELIGION_*`, `BUILDING_*`, …) **+ engine CATCH-ALL tokens**
  (`TURN`, `POPULATION`, `MILITARY`, `SELF`, …). Data Types resolve via `getInfoTypeForString`; the
  catch-all tokens are a code-side registry (designed in #430, extensible) resolved by the engine, never
  info-level special-cases — so they are uniform vocabulary (no Uniformity-Law violation, §0.6/enabler §6.1).
- **`SELF` — the ENTITY-RELATIVE token = "the owning entity's OWN type" (resolved per-entity at evaluation).**
  Unlike the fixed tokens (`TURN`/`POPULATION`/…), `SELF` resolves against whichever entity carries it. It is
  ONE token used wherever a Type appears, with the natural reading in each place:
  - in a **`per`** count-scaler → the COUNT of the owning entity's own type at the scope
    (`per:{type:SELF, scope:world}` = how many of this thing exist in the world).
  - **⚠ SUPERSEDED (owner 2026-06-17): SELF no longer appears in `requires`/`enabled`/`disabled`.** Its former
    canonical use — the global-uniqueness gate `requires.build.noneOf:[{type:SELF,scope:world}]` (tech `bGlobal`,
    world wonders, unique units) — is now the declarative `allowed` CAP (`allowed:{world:1}`; enabler-spec §5a,
    data-model §4.2a). The SELF-`requires` atom conflated *needed* with *allowed* (forced "cap 1" as `max:0`) and is
    withdrawn. SELF survives ONLY in the `per` count-scaler above.
- **Scopes** = `world | team | empire | area | city | plot{improvement|feature|terrain|route} | building |
  specialist | unit`. `empire` = player = all cities. `unit` is a SELF-ACCUMULATOR (§5).
- **Conditions** (`enabled`/`disabled`; the `requires` gate; `per`'s implicit count) = the enabler
  `requires` object, **verbatim, object form** (`all`/`any`, `min`/`max`, presence). One serialization
  across both cascades.
- **Units** (modifier magnitude):
  - **`flat`** — additive amount (sums into base).
  - **`percent`** — additive percent DELTA, summed (`+50%` = `percent: 50`).
  - **`multiplier`** — true MULTIPLICATIVE factor, full-scale (`×2` = `multiplier: 200`, identity `100`),
    composed by PRODUCT — distinct from `percent`.
  - (+ rare `postMultiplier` / `rawPercent` — §9, engine detail.)
  - **⚠ RECONCILIATION FLAG (owner ruling §0.7, 2026-06-15):** the unit names **what the value IS** (a percentage
    → `percent`; an amount → `flat`; a value authored AS a multiplicative factor → `multiplier`), **NOT how the
    engine combines it.** The "additive-delta-summed" vs "composed-by-product" phrasing above describes
    *combination*, which §0.7 says is **engine/family combine-mode metadata (§7), reworked to fit the data** —
    not the per-value unit. So a percentage-valued global scalar (GameSpeed `iSpeedPercent` = 1000%) is `percent`
    even though it scales multiplicatively. The combination-based wording here needs an owner pass to separate
    "unit = data nature" from "combine-mode = §7 engine metadata"; until then, classify the unit by the datum's
    nature (§0.7a), and let the family's combine-mode carry the math.

### 1.5 Worked example (a building) — everything composing

```jsonc
{
  "enables":  { "units": ["UNIT_CROSSBOWMAN"] },
  "requires": { "build": { "all": [ { "bonus": "BONUS_STONE", "scope": "city" } ] } },
  "grants":   [ { "type": "POPULATION", "count": 1, "scope": "empire",
                  "enabled": { "min": ["TECH_SANITATION", 1] } } ],

  "happiness":  { "city": { "flat": 2 } },                                    // always-on
  "production": { "city": { "percent": [
      25,
      { "value": 25, "enabled": { "min": ["BONUS_COAL", 1] } }                // +25% more while coal connected
  ] } },
  "gold":       { "city": { "flat":
      { "value": 1, "per": { "type": "RELIGION_*", "scope": "world" } } } },  // +1 gold per world religion
  "maintenance":{ "empire": { "all": { "percent": -10 } } }                   // cost-style; engine combines (§7)
}
```

### 1.6 Why this is the win

The same composition — *payload + scope + per + enabled* — expresses a coal-gated production bonus, a tech's
free unit (`Tracking`), Great Baths' empire population grant, a state-religion happiness deposit, a
wonder's empire-wide unit modifier. No per-concept shapes, no nesting fiesta, no parallel gate languages:
**one structure, one vocabulary, composed.** Everything below is detail under this.

---

## 2. The cascade

A modifier is a per-turn effect a source deposits onto a TARGET (anything with a modifiable per-turn
effect). It flows DOWN the containment spine and accumulates; the target reads the summed accumulator. One
step (deposit→accumulate), versus the enabler's two-step (generate→gate).

**Accumulation rule (our design choice; exact arithmetic/ordering pins at #430):**
`effective = (base + Σflat) × (100 + Σpercent)/100 × Π(multiplier/100)` per `(family, member, unit, item)`.
Flats sum into base; percents (additive deltas) sum then apply once; multipliers compose by product
(a deliberate new capability beyond the legacy additive-only consumer). Parity with the legacy result is
NOT a goal — the cascade frequently *corrects* latent bugs.

The **SPLIT-families** rule is load-bearing: `yield`→`food`/`production`/`commerce`,
`commerce`→`gold`/`research`/`culture`/`espionage`, `property`→one per `PROPERTY_*`. It absorbs one axis of
every X×yield / X×commerce 2D map (collapsing ~17 of ~36 "2D" fields to a single key). Coherent multi-member
families stay GROUPED (`maintenance`, `defense` [`amount`/`min`/…], `combat`, …); single-concept families
are top-level.

---

## 3. Conditioning — the `enabled` / `disabled` keystone

Conditioning lives **near the value, per deposit**, as two OPTIONAL fields, and **nowhere else** — this is
what keeps the construct from becoming a nesting fiesta.

- **`enabled`** — defaults **true**; present → the deposit applies only while its expression holds.
- **`disabled`** — defaults **false**; present → the deposit is suppressed while its expression holds.
- **Value of each = the enabler `requires` object, verbatim** (§0.2). **Absent = unconditioned** (the common
  case stays clean). Effective = `enabled holds ∧ ¬disabled holds`, re-evaluated each recompute (dormancy).
- **BARE-STRING predicate shorthand (owner 2026-06-16; enabler-spec §3).** A parameter-free / unambiguously-scoped
  object-predicate is a bare string — `enabled: HAS_RIVER`, `enabled: IS_CAPITAL` — not `{HAS_RIVER: true}`. A
  parameterized predicate keeps the object form (`{HOLY_CITY: RELIGION_X}`). Its **negation is the `disabled` twin**
  (`disabled: HAS_RIVER`), so the bare form never needs a `false` value.
- There is **no group wrapper** — the only nesting is the enabler expression itself (bounded `all`/`any`,
  `min`/`max`). A modder cannot build condition-trees.
- **The same `enabled`/`disabled` + list-of-entries pattern is used by `grants` too** (§ enabler-spec §6) —
  it is the universal entry shape, not a modifier-only feature.
- **No per-deposit gate beyond this.** Whole-entity availability/operation conditions (`powered`,
  `stateReligion`, `capital`, `isWorker` when they gate the *whole* thing) live in the entity's enabler
  `requires`; a dormant/unavailable entity simply deposits nothing.

---

## 4. Count-scaling — `per: { type, each, scope }`

"Increase/decrease in something based on the NUMBER of something else." An OPTIONAL count-scaler on a
deposit, default ×1 — the same opt-in philosophy as `enabled`.

```jsonc
"food": { "city": { "flat": { "value": 1, "per": { "type": "BONUS_COAL", "each": 1, "scope": "plot" } } } }   // +1 food per coal on the plot
"happiness": { "city": { "flat": { "value": 1, "per": { "type": "POPULATION", "each": 5, "scope": "city" } } } } // +1 happy per 5 population
```

- **`per: { type, each, scope }`** scales the deposit value by the COUNT of `type` at `scope`, in quanta of
  `each`: **effect = `value × (count(type) / each)`**.
- **`per: { anyOf: [TYPE…], scope }`** — scale by the SUMMED count of *any* of a SET of types (owner 2026-06-15;
  parallels the enabler `any`). **effect = `value × Σ count(t) for t in anyOf`.** Born from Corporation, whose
  per-bonus output scales by the total of its PrereqBonuses present in the city (`YieldProduced × Σ getNumBonuses`).
  `type` (single) and `anyOf` (set) are the two forms; `each` may accompany either.
- **`each` — the QUANTUM, "per how many of `type`" (owner 2026-06-15: a bare `per:{type}` "does not say how
  much per").** `each: 1` = per each one; `each: 5` = per 5 (e.g. +1 happy per 5 population). **State it
  explicitly** — the block is incomplete without it (a bare `per:{type}` is ambiguous between "per 1" and "per
  N"). (Property-source attribute scaling is always `each: 1` — the engine computes `attribute × amount`, a
  per-each-one multiply; `value` = that per-unit amount.)
- **`scope`** defaults to the deposit's own scope; state it only when the count comes from a *different*
  scope. Cross-city scopes (empire/team/world) resolve via the **tally module** (the same additive roll-up
  the enabler `requires` count-thresholds read — one module, two readers); `city`/`plot` = the local count.
- **`per` SUBSUMES the per-X units:** `perPopulation`→`per:{type:POPULATION}`,
  `perMilitaryUnit`→`per:{type:MILITARY}`, per-turn emission→`per:{type:TURN}`. There is **no separate
  `perPopulation`/`perMilitaryUnit`/`perTurn` unit** — one mechanism scales by the count of anything (a
  data Type at a scope, or a catch-all token). `SELF` = count of the owning entity's own type.

### 4.1 REPEATABLE GRANTS + the `interval` temporal primitive (owner-blessed 2026-06-16, Building #32)

The one-shot `grants` (enabler-spec §6) fires ONCE on an event. The deferred `outcomes` (UnitCombat #29) parks on a
unit and fires when a mission is pushed. The THIRD grant-lifecycle variant — **`grants.repeatable`** — fires
**recurringly on an INTERVAL**, optionally gated by a CHANCE the engine rolls. It introduces ONE new model
primitive — **`interval`**, the temporal sibling of `per` (count, §4):

- **`interval`** — when/how-often the grant fires. **`interval: { perTurn: N }` = every N turns**; the bare string
  **`interval: perTurn` DESUGARS to `{ perTurn: 1 }`** (every turn) — the same bare-shorthand pattern as the §3
  predicates. (Other interval kinds — e.g. `everyEra` — grow per case; `perTurn` is the only one #428 needs.)
- **A `grants.repeatable` entry** = `{ <payload>, interval, chance?, enabled? }`:
  - **payload** — what is granted EACH interval: a `unit` (PropertySpawn — a criminal), a `heal` count
    (iNumUnitFullHeal — fully heal N units/turn; HealUnitCombat — +N HP/turn to a unit-combat class), etc.
  - **`chance`** *(optional)* — the probability the engine rolls (`getSorenRandNum`) before granting; **reuses the
    §4 `per` count-scaler** so the chance can scale with a count — e.g. `chance: { per: { type: PROPERTY_CRIME,
    scope: city } }` (spawn odds rise with the city's crime). Absent ⇒ fires unconditionally each interval.
  - **`enabled`** *(optional)* — the usual §3 conditional.
- **TWO-STAGE gate (owner):** the **building's own `requires`** is the OUTER gate (is this building active at all —
  e.g. a crime effect-building's `requires.operate` crime-band dormancy); then the repeatable entry's **`chance`/
  `enabled`** is the INNER gate, re-checked each interval. Together = the "fully fledged system."
- **§0.6 engine boundary:** the data carries only payload + interval + chance-driver; the engine owns the
  `getSorenRandNum` roll, the interval ticking, and owner-selection (a NEGATIVE property like crime spawns the unit
  **barbarian-owned**, `PropertyInfo.AIWeight < 0`). Exact chance arithmetic (crime `value/2`, criminal-count
  damping) pins at #430.
- **Worked example (PropertySpawn → criminal):**

  ```jsonc
  "grants": { "repeatable": [
    { "unit": "UNIT_PROPERTY_CRIMINAL", "interval": "perTurn",
      "chance": { "per": { "type": "PROPERTY_CRIME", "scope": "city" } } }
  ] }
  ```

- **NOT a repeatable grant: a continuous count-scaled RATE is just a `per`-scaled MODIFIER** (owner 2026-06-16) — the
  *shrine* (`GlobalReligionCommerce`) adds commerce every turn scaled by cities holding its religion, which is
  exactly `commerce.city.flat: { value, per: { type: RELIGION_X, scope: world } }`. A continuous rate is a modifier;
  `grants.repeatable` is for DISCRETE per-interval provisions (a unit, a heal) that cannot be a standing accumulator.

---

## 5. The `unit` plane — a SELF-ACCUMULATOR

~380 fields author at `*.unit.*` (all Promotion/UnitCombat + most Unit). `unit` is a spine leaf, but a
unit-scope deposit is a **self-accumulator** (source == target, via the existing promotion/unitcombat
additive stack) — NOT a downward cascade. Cross-edges use `byOccupant` (a host-scope family summed over
plot-occupant units — celebrity/garrison/military-happiness) and `byCargo` (host-unit, SpecialUnit→carrier).
Size promotions are the worked example (§0.1). **The unit-stat FAMILY vocabulary (combat/withdrawal/bombard/
air-defense/movement/first-strike/…) — DEFINED at Promotion #28 (2026-06-16), shared by UnitCombat #29 + SpecialUnit.**
The combat family is **`strength`** ("the strength of something, or weakness on/into/against something", owner) — it
absorbs general/flat/SM/situational(city·hills)/vs-keyed(terrain·feature·unitCombat·domain·flanking) + the S&D/TB
combat sub-stats; alongside `withdrawal`/`firstStrike`/`bombard`/`collateral`/`air`/`heal`/`movement`/`experience`/
`workRate`/`cargo`/`upkeep`/`vision`/`capture`(gradient)/`poison`/`espionage`/`trap`. Two unit-plane shapes settled:
(a) **CAPABILITIES = a separate BOOLEAN group** (grant=`true`/revoke=`false`) — pure abilities, not magnitude
modifiers (§3 "pure capabilities"); (b) the hide-&-seek **LOS tables → a non-cascade `vision` resolver sub-object**
(§7), not additive families. **DESIGN PRINCIPLE (owner 2026-06-16): the unit-plane definition is authored for O(1)
runtime CONCATENATION onto the unit, never apply-time post-processing** — the static promotion's `unit`-scope deposits
SUM, capabilities UNION, and the **`promotionLine: {LINE: rank}` object** MERGES (a clean bottom→up rank overwrite +
line-hierarchy check) as each promotion is added to the one unit. Full vocabulary + per-field map: renames §Promotion.

---

## 6. CREST / stay-vs-invert — RESOLVED (keep-on-source)

`enabled`/`per` are exactly the clean "keep-on-source" expression the dilemma was missing, so the fork is
**closed**:

- A deposit keyed by entity B that LANDS ON B (B is the target) STAYS on the source keyed by B.
- A **tech**-conditioned effect is a downward deposit *from* the tech (§0.4) — no dilemma.
- A **bonus/religion/civic-conditioned** effect STAYS on source A, referencing the conditioner via
  `enabled: min(BONUS_X,1)` (presence) or `per: {type: BONUS_X}` (count). **Nothing inverts onto B.**
- The one committed `BonusProductionModifiers → Bonus.buildRate` fold is **UN-FOLDED** to this rule:
  authored on the building/unit/project itself, gated/scaled by the bonus. Action: drop the `buildRate` fold from
  `curate_bonus`; author on the source at the Building/Unit/Project pass. **(Done — see §6.2 for the home.)**

### 6.2 `production` (total city OUTPUT) vs `buildRate` (build a TARGET faster) — owner ruling 2026-06-16

**Two distinct concepts the first-pass migration flattened into `production.city` (the "Versailles bug",
DESPAIR_INDEX #12).** Pinned against the C++ (applied in different places):

- **`production` = TOTAL CITY OUTPUT** — `CvCity::getYieldRate100(PRODUCTION)`: a hammer ADD (`production.city.flat` ←
  `YieldChanges[PRODUCTION]`) or a city-wide multiplier on *everything* (`production.city.percent` ←
  `YieldModifiers[PRODUCTION]` = Factory, Power/Area/Capital yield-rate). Scales every build, every turn.
- **`buildRate` = FASTER TO BUILD A TARGET/CATEGORY** — `CvCity::getProductionModifier(eItem)` shrinks the COST of the
  *specific* item under construction; never the per-turn yield. Homes by what's produced: **`buildRate.self`**
  (build THIS faster, gated by a bonus — Versailles+marble, Worker+donkey; `BonusProductionModifiers`),
  `buildRate.{scope}.{units|buildings|domains|unitCombats}.{TARGET}` (keyed), `buildRate.{scope}.{military|space|
  worldWonder|teamWonder|nationalWonder}` (category). `buildRate` already existed (§1 family list) for unit/domain
  mods; the ruling unifies **all** "faster-to-build" fields into it, reserving `production.city` for total output.
  The one-off `militaryProduction`/`spaceProduction` families are folded into `buildRate`. **Rule of thumb:** if it
  changes how fast a *particular thing* is built → `buildRate`; if it changes the city's *whole hammer output* →
  `production.city`. (Curators corrected across building/unit/civic/project/trait; full table in
  `migration-renames.md` "Production vs buildRate".)

### 6.1 The DELIVERYGUY — who OWNS an entity-keyed modifier (owner ruling 2026-06-16)

Resolved while curating Route (#19) and Terrain (#20). **The root test is SEMANTIC SENSE (owner 2026-06-16): where
does this modifier sensibly belong?** The anchoring case is that **a thing ON A PLOT OWNS ITS OWN MODIFIERS** — *"it
doesn't make sense that things on a plot don't own their own modifiers."* The operational reading of that sense:
"an X-keyed-by-Y modifier — does it live on X or fold onto Y?" → **"who actually BRINGS this modifier to the table?"**
(the deliverer) — that entity OWNS it — **"and then what ENABLES it?"** (the condition). Not the atom kind; the
*ownership*, judged by what reads sensibly.

**The toolkit now supports BOTH expression modes, and the choice between them IS the semantic judgement, not a rigid
rule (owner 2026-06-16):** (a) **keep-on-source** — the source owns the modifier and references the other entity as a
*condition* (`enabled` presence / `per` count); (b) **fold-onto-the-owner** — the modifier lives on the entity that
semantically owns it (the deliveryguy / the thing on the plot), keyed by the source. Both are first-class and equally
expressible; per case, author it wherever it **makes semantic sense** as the home.

- **Abstract enabler-sources (civic, trait, religion) OWN their buffs**, even when keyed by a target or *improved
  by* a resource. *"+happiness from this civic, if you also have `BONUS_X`"* is the **civic's** buff, conditioned on
  the resource via `enabled`/`per` — it **STAYS on the civic**. (The "something to make it work" is the existing
  conditioning machinery, §3/§4 — no new mechanism.) This is the keep-on-source case of §6.
- **A physical/structural source that DELIVERS a modifier owns it where it is the deliveryguy.** A route making an
  improvement better → the **route** is the deliveryguy → the boost lives on the route (Route #19:
  `ImprovementInfo.RouteYieldChanges` folds onto the route, keyed by improvement). A building making a *terrain's*
  tiles yield more → the **building** is the deliveryguy → it **STAYS on the building** keyed by terrain (it is
  **NOT** folded onto the terrain — Terrain #20 carries no inbound boost). Likewise a building scaling with
  river-tile count owns *that* (`per`-scaled, on the building).
- **Plot-substrate entities (terrain, feature, improvement, route) CARRY THEIR OWN modifiers at `plot` scope.** The
  terrain forms the plot's base (hill → hammers); a feature then modifies it (forest: −food, +hammers — *first the
  plot has the terrain's modifiers, then the feature modifies them*); improvement/route layer on. Each owns its own
  contribution; a terrain/feature is the deliveryguy for its OWN intrinsic output, never for another entity's
  modifier. *(A bonus is **not** a plot-modifier owner in this sense — it is a resource/conditioner sitting on the
  plot, above it in the spine.)*
- **RIVER is a CONDITIONAL modifier, not its own entity** — a plot edge-attribute "just added on" (not a feature,
  not a terrain). Each river-side yield (`FeatureInfo.RiverYieldChange`, `ImprovementInfo.RiverSideYieldChange`,
  `BuildingInfo.RiverPlotYieldChanges`) stays on its **deliveryguy**, gated by the new **`HAS_RIVER`** plot-state
  predicate (enabler-spec §3). There is no river field on `CvTerrainInfo`.

So §6.1 REFINES §6: keep-on-source is the deliveryguy rule for abstract enablers; the deliveryguy can ALSO be a
physical plot-substrate entity (route/improvement) onto which a boost folds. The discriminator is **ownership (who
delivers)**, not whether the keyed entity is "conditioner" vs "target". *(Consequence flagged for the Building pass:
`BuildingInfo.{TerrainYieldChanges, ImprovementYieldChanges, …}` — currently set to invert onto the target in
`mapping/BuildingInfo.json` — are deliveryguy-owned by the BUILDING and should be authored on the building keyed by
the target, NOT inverted. Re-decide each at the Building pass against this rule.)*

---

## 7. Resolved accommodations (the former mechanical tail)

The grounding's seven "accommodations" mostly collapsed into the keystone/`per`/§0.6 boundary:

- **Non-additive:** (a) clamp-on-channel-total (`iMinDefense` floor, caps) is specified **in the family's own
  structure** (e.g. `defense` declares additive `amount` + a `min` member that floors the total) — not a
  generic rule. (b) min/max-across-sources (anarchy turns, yield thresholds, `naturalDefense`,
  `noEntryLevel`) → a §-engine channel **`combine` mode** (`sum` default | `max` | `min`), not a unit. (c)
  REPLACE/SET: culture caps/radius → `identity`; **no `override` unit** (Size-Matters base ranks are
  create-unit-subroutine data, §0.6, kept as-is pending a dedicated SM pass). (d) RNG/probability params →
  not modifiers, classify OUT.
- **Temporal:** per-turn emission uses `per:{type:TURN}`; `CommerceChangeDoubleTimes` "doubling" is a
  second **age-gated `enabled` deposit** (`enabled: { existedFor: { min: N } }`, read from the building's
  created-timestamp — created where absent). **No `timers` section, no non-additive engine stage.**
  Event-pulses → `grants` (next).
- **Event-pulses → `grants`**, fired by the engine **event-hook system the tally already requires**
  (one infrastructure for tally-maintenance + grant pulses). The hooks are **engine machinery, NOT on the
  Infos** (§0.6) — the Info carries only the grant payload; the engine fires it at the right time.
- **Combination semantics** (`combineStyle` rate/cost; `polarity` sign-split; `flatPlacement`
  base/postMultiplier; the `combine` mode) are **ENGINE behaviour (§0.6), not info** — solved when the
  cascaders are built (#430), with the structured data in hand. They do NOT block the migration.
- **Genuine Type×Type 2D residue:** the ~18 invisibility/visibility LOS tables → a non-cascade structured
  sub-object read by the visibility **resolver** (§0.6); `FreePromotionUnitCombatTypes` → a `grants`
  free-promotion grant. (Hide-and-seek "best-of" is resolver-side mechanics, not a cascade combinator.)

---

## 8. Out of scope / relocated / drops

**Spatial leakage → #429 (redesigned, not cut):** the property `NEAR` radius-diffusion (crime/disease/
pollution leaking city↔plot↔plot — inert today) is dropped from the containment cascade and rebuilt in the
dedicated "sideways static influence" pass; preserve the crime/disease unit→city emission via containment.
**Adjacency YIELD** (4 farms buffing each other) is a *future want* built fresh in #429 — no known existing
system, but verify in curation (this codebase hides things). Threshold pseudobuildings (`BUILDING_EFFECT_*`)
are **kept** (work as-is as buildings; reworked to `PropertyEffect` later). **Vicinity bonus** = a presence
check (enabler/`enabled`), NOT leakage — stays. **Policy: any spatial/adjacency mechanic, existing or found,
is #429's — never accommodated in the #428 modifier structure.**

**The dead-list re-splits FOUR ways (the grounding verified "dead" against C++ only — re-verify each against
`Assets/Python` AND intent; ~4 of 15 were misclassified):**

- **(i) TRULY dead → drop:** Building `iMaxPopulationAllowed`/`iMaxPopulationChange` ·
  Bonus `m_piImprovementChange` · Promotion `iDamageperTurn`/`iWeakenperTurn`/`iStrAdjperTurn` · UnitCombat
  `m_PropertyManipulators` · PromotionLine `*ContractChanceChanges` · root Improvement `m_iDepletionRand`.
  - **⚠ CLARIFICATION (Building #32, 2026-06-16): Building `iNukeExplosionRand` is NOT dead-CODE, but is dead-DATA in
    the migrated set → NOT EMITTED (effectively dropped).** The **meltdown** mechanic IS wired and runs every turn
    (`CvCity::doMeltdown` is called from `doTurn`, CvCity.cpp:1353; it rolls `getSorenRandNum(10000) < iNukeExplosionRand`
    per building to trigger a nuclear-plant meltdown; pedia at CvGameTextMgr.cpp:16279). BUT the field is populated by
    **only the EXCLUDED `Bad_Karma/Building_Meltdown` module** (5 buildings, `bLoad=0`,
    `store.EXCLUDED_MODULE_SUBPATHS`) — so **no INCLUDED building sets it**, and the curator never emits it (like any
    all-zero field, cf. Terrain's "0/42 authored → not emitted"). The third category beyond dead-code/live-data:
    **live-code + excluded-module-only-data**. (If Bad_Karma were ever included, its home would be `identity.meltdownChance`
    — a per-turn RNG disaster chance, RNG out of cascade §7d. The original §8(i) "truly dead" was wrong on the *reason*,
    right on the *outcome*.)
- **(ii) UNWIRED MODIFIERS (legit intent, never wired — per-item):** Tech `FreeSpecialistCounts` → **revive**
  as `freeSpecialists` modifier (Python-live; "adding a specialist is a modifier"). Building
  `iPillageGoldModifier` → **revive** as `pillageGold.empire.percent` (a world wonder boosting pillage gold
  for ALL empire units — the empire-wide UNIT-modifier shape that is the intended vehicle for future
  wonder-influence features, e.g. "astrological influence of wonders" / "way of war"). Project
  `YieldModifiers` → **drop** (a +10-commerce buff on every plot is rejected as nutty; empire yield buffs,
  if wanted, are cheap in the existing structure later).
- **(iii) UNWIRED WORLD-STATE FEATURES → separate future issues:** `bNoAnimals` ("disable animals" as a
  BUG/game option) + the Era/vote world-state bools (D9 family) — wanted, but not modifier data, not #428.
- **(iv) DELIBERATE BALANCE CUTS:** `healthPercent` the **modifier is wanted in general** (the `health`
  family supports a `percent` unit), but it is **not sourced from improvements** ("a balancing nightmare") —
  keep the capability, cut the specific source. (Terrain `iHealthPercent` is separately truly dead;
  Feature/Specialist `healthPercent` = per-entity call at their passes.)

**Relocated (not deleted — moved to another section):** → `grants`: Building population/golden-age pulses,
Civic `iRevIdxSwitchTo`, Unit `GroupSpawnUnitCombatTypes`. → enabler `requires.operate`: Building
`iMaxPopAllowed` (dormancy). → `enables`/`requires`: Project `iTechShare`, Vote world-state bools. →
`identity`: Civic `iAnarchyLength`, Terrain `iMovement`+`Yields`, CultureLevel `iCityRadius`, UnitCombat
`bForMilitary`/`bForNavalMilitary`. → `ai`: LeaderHead (~90 fields). → tech (inverted): Route
`TechMovementChanges`. → probability/identity (RNG, out of cascade): Improvement `iAirBombDefense`,
`DiscoverRand` *(per-bonus depletion-rand KEPT; only root dead — resource depletion is a live gated
mechanic, `MODDERGAMEOPTION_RESOURCE_DEPLETION`)*.

---

## 9. Demolition list (what the cascade DELETES — parallel to enabler §14)

- **CvCity yield/commerce accumulator + per-building reads** (`getBaseCommerceRateFromBuilding100`,
  `getBuildingYield`; CvCity.cpp:11326-11364, 12104-12203, 12272-12291) → the split yield/commerce cascade
  accumulators (additive deposit → O(1) summed read).
- **CvCity health/happiness accumulators incl. good/bad-split** (CvCity.cpp:514-525, 5637-5985, 8411-8697,
  9022, 9086, 19917-19930) → health/happiness families with reader-side `polarity=signed-split`.
- **CvCity defense channel reads** (`getDefenseModifier`/`getNaturalDefense`/`getTotalDefense`,
  `changeExtraMinDefense`, `setMinimumDefenseLevel`; CvCity.cpp:4730-4839, 10184-10200, 20583-20592) →
  defense family (clamp in the family structure; `noEntryLevel` `bestHigh`).
- **CvCity / CvPlayer maintenance + upkeep** (CvCity.cpp:7580-7846, CvPlayer.cpp:7398-7399, 14219-14262,
  10345-10374, 18029-18034, 28498-28503) → maintenance/upkeep families (cost-style combine).
- **CvPlayer trait/civic processing** — `processTrait` (28410-28707), `setCivics` (17993-18230), ~150
  `change*` calls → empire-scope deposits from the civic/trait JSON; setters deleted.
- **CvTeam/CvPlayer `processTech`** (CvTeam.cpp:5936-6093, CvPlayer.cpp:30872-30894) → team/empire deposits
  (the §0.4 downward tech deposits).
- **CvCity `processBuilding`/`processSpecialist`/`processBonus`/`processCorporation`** (CvCity.cpp:4317-5198,
  12174-12613, 14547-15203, 20921-20936) → the unified containment cascade; per-entity loops deleted.
- **CvCity tech-conditioned recompute loops** (CvCity.cpp:4935-4957, 12174, 22994-23088) → §0.4 downward
  tech deposits.
- **CvCity power-/state-religion-gated sub-accumulators** (4648/11216, 8454-8456/12178) → per-deposit
  `enabled` / entity `requires`.
- **CvUnit promotion/unitcombat extra-stat stack** — ~200 `changeExtra*`/`setHas*` setters
  (CvUnit.cpp:18281-19110, 26273-30689) → the unit-plane self-accumulator (§5).
- **`getModifiedIntValue` cost-asymmetry sites** (CvGameCoreDLL.cpp:689-700 callers) → `combineStyle=cost`
  family-metadata applied once per cost channel.
- **CvArea per-player good/bad health/happiness pools** (CvPlayer.cpp:7412-7457) → area-scope deposits.
- **CvGame world-scope vote/project/tech accumulators** (CvGame.cpp:7917-7988, 7923) → world-scope deposits
  - enables-family for the reversible-doctrine vote actions.
- **CvCity `CommerceChangeDoubleTimes` post-sum ×2 path** (CvCity.cpp:12197-12203) → a second age-gated
  `enabled` deposit reading the created-timestamp — no engine stage.
- **CvPlayer MIN/MAX combinator updaters** (`updateMaxAnarchyTurns`/`updateMinAnarchyTurns`/
  `updateExtraYieldThreshold`/`updateLessYieldThreshold`; CvPlayer.cpp:9553-9585, 12851-12892) → `combine:
  max|min`.
- **CvGameTextMgr upward cross-entity pedia SCANS** (CvGameTextMgr.cpp:16966-17861) → the derived reverse
  index (cold path), built once on load from the forward deposits.

---

## 10. Banked for later phases (not migration blockers)

- **Enabler-spec additions** (record there — the enabler objects are reused verbatim, changed only
  consciously): an **age/duration predicate** (`existedFor` + a created-timestamp, created where an entity
  lacks one); a **`firstToResearch`/`firstToDiscover`** predicate; the `grants` family includes **bonuses**
  (`ExtraFreeBonuses` — wonder/culture grants a tradeable resource, fired ON DELIVERY).
- **Unit modifier vocabulary** — the unit-stat families/channels (§5) — owned by the Unit / Promotion /
  UnitCombat curation pass.
- **#430 (cascader build):** the catch-all token registry; the event-hook + tally infrastructure;
  combination semantics (`combineStyle`/`polarity`/`flatPlacement`/`combine`); the `multiplier`
  composition arithmetic; whether a distinct `multiplier` unit needs further forms.
- **Separate future issues:** "disable animals" BUG/game option (§8 iii); empire-wide yield buffs (if ever
  wanted); the #429 spatial-leakage / adjacency-yield system; the `PropertyEffect`/`BaseEffect` rework of
  threshold pseudobuildings; a dedicated Size-Matters pass.
