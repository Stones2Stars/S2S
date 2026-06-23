# JSON data spec — the authoritative entity shape & vocabulary

The single source of truth for what a Stones2Stars data file may **contain**. Every game entity — a building,
unit, tech, civic, religion, terrain, … — is **one JSON object in its own file** under `Assets/Data/<type>/`.
The three **cascade** machines — the engine systems that read this data: [enabler](enabler.md) ("can I build
it?"), [modifier](modifier.md) ("how much?"), [tally](tally.md) ("how many?") — plus `readJson` consume exactly
this shape; the future modder reference is *derived* from this spec, never the other way round.

**The one promise — the data reads cold.** A well-authored file is understandable with zero engine knowledge.
Keys say what they mean; values say what they are. If a shape only makes sense once you know the C++, it is
wrong — the engine is built to fit the data.

> **Validate while you author.** `readjson.exe Assets/Data --render BUILDING_FORGE` parses a file, flags anything
> unrecognized, and renders it to plain English (*"Forge: +25% production; +1 happiness while powered; unlocks
> Crossbowman"*) so you can check "is this what I meant?".

---

## 1. The big picture

An entity's JSON is a **flat set of top-level keys**. Each key is exactly one of two things:

- a **reserved section** — a fixed keyword with a defined meaning (`enables`, `requires`, `allowed`, `grants`,
  `identity`, `cost`, …); or
- a **modifier family** — a per-turn effect this entity produces (`food`, `production`, `happiness`,
  `maintenance`, one per `PROPERTY_*`, …).

**The classification rule (deterministic, parser-enforced):** a non-reserved top-level key whose value is an
**object** is a **modifier family** (it is scope-keyed, §6); a non-reserved key whose value is a **bare**
bool/string/number is a **capability/skill flag or a text field**, never a family. So the *value's shape* decides
"family vs flag," and a *section's name* decides its meaning. A family colliding with a reserved word is an error.

Everything below is detail under one idea: **one object structure, one shared vocabulary, composed everywhere.**

The three machines read only the **cascade** sections (`enables`-family, `requires`, the modifier families, the
count-bearing clauses, `grants`); intrinsic and auxiliary sections feed their own systems. `readJson` parses all
of them.

---

## 2. Anatomy of an entity

| group | sections | what they are |
|---|---|---|
| **Availability** | `enables` · `obsoletes` · `replaces` · `disables` · `requires` · `allowed` | what this unlocks/removes; what it needs to be built & to keep running; the cap on how many may exist |
| **Provisions** | `grants` | one-shot / recurring things this hands out (units, buildings, pulses) |
| **Effects** | every **modifier family** key (`food`, `production`, `happiness`, …, one per `PROPERTY_*`) | per-turn magnitudes this deposits onto targets |
| **Intrinsic** ("what am I") | `identity` (incl. all TEXT) · `cost` · `ui` · `world` · `sound` · `ai` | empire-agnostic self-description, art, audio, AI metadata |
| **Classification** | `skills` (UNIT, mutable abilities) · `tags` (UNIT, immutable type membership) · `state` (UNIT, transient) · `capabilities` (TEAM, tech/civic-unlocked) | §8 — the four-block classification model; scope carried by the section name |
| **Auxiliary / bespoke** | `loadPrune` · `policies` · `succession` · `excludes` · `produces` · `condition` · `effect` · `vision` · `outcomes` · `mapGeneration` · `replacedBy` · `promotionLine` · `buildUp` · `shrine` · `properties` · `voteSource` · `threshold` · `role` · `victory` · `targetLevel` · `conversion` · `cityFounding` · `unitCapability` | data read by their own systems, not the cascade |

`type` (the entity's own id, e.g. `"BUILDING_FORGE"`) and the TEXT fields are present where relevant.

---

## 3. The shared vocabulary

The same atoms compose `requires`, the `enabled`/`disabled` conditions, the `per` count-scalers, `grants`, and
modifier targets — **not separate shapes.** Learn them once.

### 3.1 Types, tokens, `SELF`

- **Data Types** — `INFOTYPE_NAME` ids (`BONUS_COAL`, `UNIT_AXEMAN`, `BUILDING_FORGE`). The leading **infotype**
  prefix identifies the kind and routes the reference; the full prefix glossary (`UNIT_` = unit, `TRAIT_` =
  simple trait, `TRAIT_COMPLEX_` = complex, …) is the **[naming spec](naming.md)**.
- **Catch-all tokens** — engine concepts that aren't data Types: `TURN`, `POPULATION`, `MILITARY`, `CITY`,
  `TEAM`, `UNIT_LEVEL`, `AREA_SIZE`, … (an engine-resolved, extensible registry).
- **`SELF`** — "this entity's own type," resolved per-entity. Used only in a `per` count-scaler ("per how many of
  me exist"). It is **not** used in `requires` — a "one of me" cap is [`allowed`](#44-allowed--caps), not a
  condition.

### 3.2 Scopes — the containment spine (always SINGULAR)

```
world › team › empire › area › city › plot{improvement|feature|terrain|route} › building | specialist | unit
```

A **scope** says *where* something applies or *where* a count is taken. `empire` = the player (all their
cities). A `unit`-scope effect is a **self-accumulator** (it lands on the unit itself). A `plot`-scope effect is
the plot's own intrinsic output. One off-spine scope exists: **`self`** — the entity's *own* build (e.g.
`buildRate.self` = "build *this* entity faster"), not a place in the containment hierarchy. *(Distinct from the
`SELF` count token in §3.1.)*

### 3.3 Targets — *what kind of object* receives a deposit (always PLURAL)

The differentiator between scope and target is **grammatical number.** A scope is singular (`empire`, `city`,
`plot`). A **target** is plural — `plots` · `units` · `cities` · `areas` · `empires` — and means **all objects of
that kind in the scope**, filtered by an optional predicate. So `empire` (singular) is unmistakably the *reach*
and `plots`/`units` (plural) the *receivers*, even when a root is shared. A deposit with **no** plural target
lands on the scope object itself (the city — the common case). Full deposit syntax: §6.

### 3.4 Conditions — `all` / `any` / `noneOf`

A bounded boolean tree, identical wherever a condition is needed (`requires`, and a deposit's `enabled`/`disabled`).
**Deliberately plain-English ("normal human being") semantics — `any` = *any of these*, `all` = *all of these*:**

```jsonc
{ "all":    [ … ],   // AND  — every clause holds
  "any":    [ … ],   // OR   — at least one clause holds
  "noneOf": [ … ] }  // NONE — none of these may be present
```

`all` and `any` are **flat** lists of clauses: `any:[A,B,C]` = "A **or** B **or** C"; `all:[A,B,C]` = "A **and**
B **and** C". For anything more complex (AND-of-ORs), **nest** — that is the win: "copper **or** iron, **and** a
forge" is `{all:[ {any:[BONUS_COPPER,BONUS_IRON]}, BUILDING_FORGE ]}`. Each leaf is **either** a count/presence
**atom** or a **predicate** (§3.5):

```jsonc
{ "type": "BONUS_IRON", "scope": "city", "connection": "trade|vicinity" }   // an atom
```

An **atom** is `{ type, scope, min?, max?, connection? }`, always **fully explicit** — it carries its own `type`
and `scope`; the engine never infers them.

- **presence** = `min: 1` ("have ≥ 1"). Authoring presence this way keeps it future-proof if a resource later
  gains amounts.
- **count thresholds** — `min: N` (≥ N) and/or `max: N` (≤ N), both inclusive. Exact-N = `min` and `max` together.
- `connection` (resources only) ∈ `"trade"` | `"vicinity"` | `"trade|vicinity"`. `vicinity` = present on any plot
  in the city's current workable radius.

> **Counts vs caps.** `min`/`max` express what you **need** (a count of *some other* type, e.g. "≥12 Barracks").
> "How many of THIS may exist" is **not** a condition — it is the [`allowed`](#44-allowed--caps) cap.

### 3.5 Predicates — a system's runtime-state query

A predicate asks the game state a yes/no question a static file can't hold ("is this the capital? a river?"). It
is **evaluated against the deposit's target** and so carries **no `_PLOT`/`_UNIT` suffix** — the target supplies
context: `IS_WATER` on `plots` = a water tile, on `units` = a sea unit. An **unknown/missing predicate is
IGNORED**, never treated as false — retiring a system never spuriously disables unrelated data.

- **bare** (parameter-free string), four groups:
  - **environment / domain** `IS_<where>` (target-relative): `IS_WATER` · `IS_LAND` · `IS_AIR` · `IS_SPACE` · `IS_LUNAR` · `IS_MARS`
    (extensible).
  - **plot attributes** `HAS_<attr>` (relief & adjacency a plot carries, orthogonal to environment so they
    compose): `HAS_PEAK` · `HAS_HILLS` · `HAS_COAST` (adjacent to water) · `HAS_RIVER` · `HAS_FRESHWATER` ·
    `HAS_IRRIGATION` · `HAS_FEATURE` ("has *any* feature").
  - **plot city-relative state** (nested `VICINITY ⊇ WORKABLE ⊇ IS_WORKED`): `VICINITY` (in the city's workable
    radius) · `WORKABLE` (in radius and eligible to be worked) · `IS_WORKED` (a citizen works it this turn).
  - **city / player:** `IS_CAPITAL` · `HAS_POWER` · `HAS_STATE_RELIGION` · `STATE_RELIGION_IN_CITY`.
- **parameterized** `{ PREDICATE: param }`: `{HAS_FEATURE: FEATURE_X}` · `{HAS_TERRAIN: TERRAIN_X}` ·
  `{HAS_BONUS: BONUS_X}` · `{HAS_RELIGION: RELIGION_X}` · `{STATE_RELIGION: RELIGION_X}` · `{HOLY_CITY: RELIGION_X}` ·
  `{HAS_CORPORATION: CORPORATION_X}` · `{latitude:{min,max}}` · `{existedFor:{min:N}}` (turns since built).
- **membership sugar** `{ terrain|feature|bonus: [TYPE,…] }` = "the plot's terrain/feature/bonus is one of these";
  equivalent to an `any` of the matching `HAS_*` predicate.
- **composition is the win:** a Martian peak is `{all:["IS_MARS","HAS_PEAK"]}`; coastal land
  `{all:["IS_LAND","HAS_COAST"]}`; flat land = `IS_LAND` with no `HAS_HILLS`/`HAS_PEAK`. No bespoke
  "mars-peak"/"coastal-land" type.
- **negation** uses the `disabled` twin (§3.9) or `noneOf` — never a `false` value.

### 3.6 Units — what a modifier value *is*

A magnitude names the **nature** of the value, not how the engine combines it (§6 owns the combine math):

- **`flat`** — additive amount (`+2` = `2`).
- **`percent`** — additive percent delta (`+50%` = `50`).
- **`multiplier`** — true ×factor, identity `100` (`×2` = `200`).

(Plus `postMultiplier` / `rawPercent` — rare **engine-internal** units, **not for normal authoring**; ignore them
unless porting a specific engine quirk.)

> **Values are human-readable. Always.** `7`, `25`, `1.5` — never ×100. **Rule (fixed-point):** all engine math
> is integer fixed-point ×100 (no float — Civ4 MP is deterministic lockstep and float desyncs), but the single
> human→×100 conversion happens once at load in `readJson`. **A ×100 value in a JSON file is a bug.**

### 3.7 `per` — count-scaling

```jsonc
"per": { "type": "POPULATION", "each": 5, "scope": "city" }      // value × (count / 5)
"per": { "anyOf": ["BONUS_COW","BONUS_PIG"], "scope": "city" }   // value × (summed count of any listed)
```

`each` is the quantum ("per 5 population" → `each: 5`); state it explicitly. `scope` defaults to the deposit's own
scope; cross-city scopes (empire/team/world) resolve via the [tally](tally.md), `city`/`plot` are local.

**`unit: <predicate>`** qualifies a deposit by a **unit predicate** — the *same* `unit:` qualifier cargo uses
(`cargo.space.{unit: IS_AIR, …}`, [modifier](modifier.md) §6). On a count-scaling family it reads **per unit
matching**: `happiness.empire.cities.{unit: IS_MILITARY, flat: N}` = "N happiness per *military unit* stationed" —
the unit-presence effect lives on the civic/trait that grants it, targeting each city. (Supersedes the earlier
`perUnit:` spelling, unifying with cargo's `unit:`.)

> **Predicates vs tags (owner).** `IS_*` predicates are **independent queries**, *not* tag-membership: `IS_LAND`
> (used by cargo above) matches an intrinsic *domain*, not a `tag`. But a predicate **may be defined to encompass
> tags** (e.g. `IS_MILITARY` set up to match the `military` tag + similar) — predicates have **definitions**.
> **Post-migration:** make predicates **definable as JSON objects** and support **predicate groups** (compose
> them); for **migration they are HARDCODED** (`IS_MILITARY`/`IS_LAND`/`IS_AIR` baked into the curator).

### 3.8 `interval` — recurrence (for repeatable grants)

`interval: { perTurn: N }` = every N turns; bare `interval: "perTurn"` = every turn.

### 3.9 The one entry shape

Every deposit, grant, or conditioned value is the same shape:

```jsonc
{ <payload>, "scope"?, "per"?, "enabled"?, "disabled"?, "ai"? }
```

- **payload** — a unit magnitude: a **bare number**, or `{ "value": N, … }` when conditioned or in a list — the
  **unit** (`flat`/`percent`/`multiplier`) is the key *above* the entry, and `value` carries the magnitude inside
  it. OR a grant (`type`+`count`), OR a predicate.
- **`scope`** default = the containing scope · **`per`** default ×1 · **`enabled`** default true (applies only
  while the condition holds) · **`disabled`** default false (suppressed while it holds) · **`ai`** an optional
  sibling block applied for AI players only (same inner shape).
- **`enabled` is read before `disabled`** — the enable is evaluated first, the disable second; a `disabled`
  that holds **overrides** (the deposit is suppressed even if `enabled` was satisfied). Author them in that
  order: `enabled` first in the list, then `disabled`.
- A leaf is a single entry **or a LIST of entries** (several conditioned values into one slot):

```jsonc
"production": { "city": { "percent": [
    25,                                                            // always +25% (a bare number)
    { "value": 25, "enabled": { "type": "BONUS_COAL", "min": 1 } }  // +25% more while coal is connected
] } }
```

---

## 4. Availability

The availability sections decide **what is offered** — what unlocks what, what an entity needs, and how many may
exist. (The two-pass machine that consumes them is the [enabler](enabler.md).)

### 4.1 `enables` — what this unlocks (permanent, source-side)

```jsonc
"enables": { "units": ["UNIT_CROSSBOWMAN"], "buildings": ["BUILDING_BANK"] }
```

Buckets: `buildings · units · builds · techs · civics · religions · corporations · projects · processes ·
promotions · promotionLines · heritages · specialBuildings · specialBuildingsWaived · improvements · bonuses ·
routes · votes · hurries · traits · specialists`. **Tech unlocks live here** (a tech `enables` what it researches).

### 4.2 `obsoletes` / `replaces` / `disables` — removal (permanent, source-side)

Same per-kind bucket shape as `enables`.

- **`obsoletes`** — supersession: new builds barred; **existing instances persist** (an obsolete unit stays on the map).
- **`replaces`** — succession: a successor takes the predecessor's slot (transitive). Wins over `obsoletes`.
- **`disables`** — a reversible removal whose fate is set by the **source's nature**: a **law/ban** source (a
  policy forbidding a building) **destroys** the target (repeal ⇒ rebuild); an **effect** source (e.g. blackened
  skies disabling telescopes) sends it **dormant** — parked, auto-resumes when the disabler clears, never rebuilt.

### 4.3 `requires` — what this NEEDS (reversible, target-side)

The means a target needs. Two timings:

```jsonc
"requires": {
  "build":   { "all": [ {"type":"BONUS_STONE","scope":"city","connection":"trade|vicinity"} ] },
  "operate": { "all": [ {"type":"CIVIC_GUILDS","scope":"empire"} ] }
}
```

- **`build`** — needed to construct it; **greyed** if missing. Checked once, at build.
- **`operate`** — needed to construct **and** keep running; if lost later the built thing goes **dormant**
  (inactive, not destroyed) and wakes when it returns. (Units carry `build` only.)

Each is an `all`/`any`/`noneOf` tree (§3.4). A single bare predicate may be given as a `disabled`/`enabled` clause:

```jsonc
"requires": { "build": { "disabled": "IS_CAPITAL" } }   // can't build where a capital already exists
```

`requires` holds genuine **needs** (resources, civics, religion, count thresholds of *other* types). It does not
hold "how many of myself" — that's `allowed`.

### 4.4 `allowed` — caps

How many of this entity may **exist**. Author the **real number** — the engine permits a build while the current
count is below it. Absent ⇒ uncapped. Two shapes, told apart by the key:

```jsonc
"allowed": { "world": 1 }     // self-cap: at most ONE of me anywhere (a world wonder; a globally-unique tech)
"allowed": { "empire": 1 }    //           at most one per player (a national wonder; a unique unit)
"allowed": { "team": 1 }      //           at most one per team (a team wonder)
"allowed": { "worldWonders": 3, "teamWonders": 2, "nationalWonders": 8 }   // category cap (on CultureLevel)
```

- **self-cap** — a **scope** key (`world`/`team`/`empire`) = "at most N of *me* at that scope." For a building the
  cap scope also makes it a world / team / national wonder.
- **category count-cap** — a **wonder-category** key (`worldWonders`/`teamWonders`/`nationalWonders`; `totalWonders`
  reserved), on `CultureLevel`, caps how many of a category a *city* may hold.

The engine owns ignoring caps under the relevant game options, era-scaling, and per-entity exceptions — you just
declare the number. Enforcement reads the [tally](tally.md) count.

---

## 5. `grants` — provisions

One-shot or recurring things an entity hands out (not per-turn modifiers).

```jsonc
"grants": {
  "techs": ["TECH_POTTERY"], "units": ["UNIT_WARRIOR"],   // entity lists
  "population": 1, "revolution": -100,                     // numeric pulses: grants.<channel>: value
  "foundBuildings": [ {"building":"BUILDING_PALACE"} ],    // a founder's settle-time building seed
  "repeatable": [                                          // recurring, optionally chance-rolled
    { "unit": "UNIT_PROPERTY_CRIMINAL", "interval": "perTurn",
      "chance": { "per": { "type": "PROPERTY_CRIME", "scope": "city" } } } ]
}
```

- **lists** — `buildings · units · techs · civics · specialists · promotions · traits · bonuses · freePromotions ·
  foundBuildings`.
- **numeric pulses** — `grants.<channel>: value` (`grants.revolution: -100`, `grants.goldenAge`).
- **`repeatable`** — `[ { <payload>, interval, chance?, enabled? } ]`: fires each interval (a spawned unit, a
  heal), optionally gated by a rolled `chance` (which may scale with a `per`).

---

## 6. Effects — modifier families

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
- **Grouped families** keep `<member>` parts (`maintenance`, `defense`, …).
- The **unit plane** has its own family set (`strength`, `withdrawal`, `firstStrike`, `bombard`, `collateral`,
  `air`, `heal`, `movement`, `experience`, `workRate`, `cargo`, `vision`, `capture`, …); a `unit`-scope deposit is
  a self-accumulator.
- **`buildRate` vs `production` — keep them distinct.** `production.city` is the city's *total* output (scales
  every build); **`buildRate`** only speeds up *building a specific target*: `buildRate.self` (build **this**
  entity faster — the off-spine `self` scope), or keyed by what's built (`buildRate.<scope>.buildings.{BUILDING}`,
  or a category like `military`).

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

A cross-entity modifier lives on **whoever brings it to the table** (the deliveryguy), keyed by the target — never
inverted onto the target. The other entity is referenced as a **condition** (`enabled`/`requires`), never the home.
Two shapes, chosen by what reads sensibly:

- **own-output** — an entity's own produced output (a specialist's yield, an improvement's tile yield, a unit's
  strength) lives on that entity; tech/civic/building are `enabled` conditions. *A civic that boosts a Merchant's
  commerce → on the specialist, `enabled:{civic}` — not on the civic.*
- **governing-deliverer** — an entity that delivers/governs an effect on others lives on the actor, keyed by the
  target. *A route upgrading improvements → on the route, keyed by improvement.*

Plot-substrate entities (terrain/feature/improvement/route) each own their own `plot`-scope output.

### 6.3 How values combine

```
effective = (base + Σflat) × (100 + Σpercent)/100 × Π(multiplier/100)
```

flats sum into base; percents (additive deltas) sum then apply once; multipliers compose by product. You author
the values; the engine combines them — the combine mode is **family metadata**, never the per-value unit.

---

## 7. Intrinsic

Empire-agnostic self-description. Read directly — never summed or cascaded.

- **`identity`** — "what am I": all **TEXT** (`description`, `help`, `civilopedia`, `message`, `quote`, `strategy`,
  `adjective`, `shortDescription`) + intrinsic flags/values (radii, classifications, capability bools, base stats).
- **`cost`** — what it costs to make (`production`, and cost sub-fields).
- **`ui`** — interface art/sound (icons, buttons, movies) · **`world`** — on-map 3D art · **`sound`** — audio assets.
- **`ai`** — AI-only metadata (flavours, weights, personality); never affects rules, only AI behaviour.

---

## 8. Unit classification — `skills`, `tags`, `state` (& empire `capabilities`)

A unit's classification splits into **three blocks, distinguished by lifecycle**. The **operative test: *can a
promotion grant it?***

- **`skills`** — **mutable** unit abilities, gained/lost via promotions (`blitz`, amphibious, walk-on-mountains,
  fly-over-water, …). *Promotion-grantable ⇒ skill.* Glossary: [skills.md](skills.md).
- **`tags`** — **immutable, type-derived** membership: set at creation, re-set on **upgrade**, **purely for
  accounting** — overlapping (`military`/`civilian`/`worker`/`spy`/`gunpowder`/`mechanized`/…), read by
  `IS_<TAG>` predicates, **no behaviour or modifiers**. *Not* promotion-grantable (a swordsman must upgrade to a
  rifleman to gain `gunpowder`).
- **`state`** — **transient** conditions (fired → counted down → over: `paralyze`/immobilise). **Greenfield** —
  never first-class; historically faked via pseudo-promotions + Python events.

The **empire** counterpart to unit `skills` is **`capabilities`** — **team-wide, tech/civic-unlocked** civilization
abilities (found-on-peaks, pass-peaks, move-on-water, tech-trading, irrigation, bridge-building, river-trade).
The **section name carries the scope**, so the engine never guesses.

```jsonc
"skills": { "amphibious": true, "blitz": true },   // UNIT, mutable
"tags":   { "military": true, "gunpowder": true }, // UNIT, immutable (type)
"capabilities": { "moveOnWater": true }            // TEAM/empire
```

Full glossaries: [skills.md](skills.md); the `tags` / `state` / `capabilities` glossaries are pending.

---

## 9. Auxiliary & bespoke sections

Data read by a specific system, not the cascade. Use only when the entity needs it:

- **`loadPrune`** — `{ onGameOptions, notOnGameOptions }`: drop this entity at load under given game options.
- **`policies`** — civ meta (`playable`, `aiPlayable`) **or** player-state law toggles (per entity).
- **`succession`** — `{ upgradesTo, promotionLine, priority }` (manual upgrade / promotion-line link).
- **`excludes`** — same-tier mutual exclusion (conflicting traits).
- **`produces`** — a Build's outcome FKs (what laying it creates).
- **`replacedBy`** — a conditional whole-entity swap (an alternate Info under a culture level / game option).
- **`condition`** (Victory) · **`effect`** (Vote) · **`vision`** (line-of-sight) · **`outcomes`** (mission
  results) · **`mapGeneration`** (placement/spawn config).
- **bespoke** object-sections, each read by its own system: `promotionLine` · `buildUp` · `shrine` · `properties` ·
  `voteSource` · `threshold` · `role` · `victory` · `targetLevel` · `conversion` · `cityFounding` · `unitCapability`.

A dedicated system's data lives in its **own block** — a module is "on" iff its block exists and is non-empty — so
a system can be added, swapped, or removed as a unit.

---

## 10. Worked examples

### A building

```jsonc
{
  "type": "BUILDING_FORGE",
  "identity": { "description": "TXT_KEY_BUILDING_FORGE" },
  "enables": { "units": ["UNIT_CROSSBOWMAN"] },
  "requires": { "operate": { "all": [ {"type":"BONUS_IRON","scope":"city","connection":"trade|vicinity"} ] } },
  "production": { "city": { "percent": 25 } },
  "happiness":  { "city": { "flat": 1, "enabled": "HAS_POWER" } },
  "cost": { "production": 120 }
}
```
*Unlocks the Crossbowman; needs connected iron to keep operating; +25% production and (while powered) +1 happiness
in its city; costs 120 hammers.*

### A world wonder (a cap + a conditional bonus)

```jsonc
{
  "type": "BUILDING_VERSAILLES",
  "allowed": { "world": 1 },
  "requires": { "build": { "disabled": "IS_CAPITAL" } },
  "buildRate": { "self": { "percent": 100, "enabled": { "type": "BONUS_MARBLE", "scope": "city", "min": 1 } } },
  "culture": { "city": { "flat": [ 10, { "value": 10, "enabled": { "existedFor": { "min": 1000 } } } ] } }
}
```
*Only one may exist in the world; can't be built where a capital already sits; builds twice as fast with connected
marble; +10 culture, doubling after it has stood 1000 turns.*

### A culture level (per-city wonder caps)

```jsonc
{
  "type": "CULTURELEVEL_DEVELOPING",
  "enables": { "buildings": ["BUILDING_TOWN_HALL"] },
  "allowed": { "worldWonders": 2, "teamWonders": 2, "nationalWonders": 8 },
  "defense": { "city": { "amount": { "percent": 12 } } }
}
```
*A city at this level may hold up to 2 world / 2 team / 8 national wonders, and gets +12% defense.*

---

## 11. Quick reference

**Top-level keys** — `type` · `identity` · `cost` · `ui` · `world` · `sound` · `ai` · `enables` · `obsoletes` ·
`replaces` · `disables` · `requires` · `allowed` · `grants` · `skills` · `tags` · `state` · `capabilities` · *(modifier families)* ·
*(auxiliary/bespoke, §9)*

**Scope (singular)** — `world › team › empire › area › city › plot{improvement|feature|terrain|route} › building|specialist|unit` · off-spine `self` = the entity's own build
**Target (plural)** — `plots · units · cities · areas · empires` = all of that kind in the scope, predicate-filtered
**Combinators** — `all` (AND) · `any` (OR) · `noneOf` (NONE) · nest for AND-of-ORs
**Atom** — `{ type, scope, min?, max?, connection? }` · presence = `min:1`
**Predicate** — bare (`IS_*`/`HAS_*`/`VICINITY`/`IS_CAPITAL`…), `{PREDICATE: param}`, or membership `{terrain|feature|bonus:[…]}`
**Units** — `flat` (amount) · `percent` (+% delta) · `multiplier` (×, identity 100). Human-readable; ×100 is a bug.
**Entry** — `{ <payload>, scope?, per?, enabled?, disabled?, ai? }`
**`requires`** — `build` (greys) / `operate` (greys + dormancy)
**`allowed`** — `{scope:N}` self-cap, or `{worldWonders|teamWonders|nationalWonders:N}` per-city category cap

---

*The machines that consume this shape: [enabler](enabler.md) (can I?) · [modifier](modifier.md) (how much?) ·
[tally](tally.md) (how many?). The legacy XML→JSON field mapping is migration-transient and lives with the
migration, not here.*
