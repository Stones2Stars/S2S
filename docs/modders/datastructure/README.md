# S2S Data Structure — Modder Reference

How to author Stones2Stars game data. Every game entity — a building, unit, tech, civic, religion, terrain, … — is
a single **JSON object in its own file** under `Assets/Data/<entity-type>/`. This document is the authoritative
reference for what those files may contain.

**The promise: the data reads cold.** A well-authored entity file should be understandable to someone with *zero*
knowledge of the game engine. Keys say what they mean; values say what they are. If a shape only makes sense once you
know how the C++ works, it is wrong — the engine is built to fit the data, never the reverse.

> **Validate while you author.** The `readjson` tool (`Tools/ReadJson/`) parses your JSON, flags anything it does not
> recognize, and **renders an entity to plain English** (`readjson.exe Assets/Data --render BUILDING_FORGE`) so you can
> check "is this what I meant?" — e.g. *"Temple of Gnarl: builds faster with gnarlite; +2 happiness; allowed 1 world."*
>
> **Planned (once the structure is fully settled):** a **per-info-type schema** — an allowlist of which sections and
> families are valid on each info type — that the parser checks against, so a building-only section placed on a unit (and
> the like) is caught as an error rather than silently ignored.

---

## Table of contents
1. [The big picture](#1-the-big-picture)
2. [Anatomy of an entity](#2-anatomy-of-an-entity)
3. [The shared vocabulary](#3-the-shared-vocabulary)
4. [Availability — `enables`, `requires`, `allowed`, …](#4-availability)
5. [`grants` — provisions](#5-grants--provisions)
6. [Effects — modifier families](#6-effects--modifier-families)
7. [Intrinsic — `identity`, `cost`, `ui`, `world`, `sound`, `ai`](#7-intrinsic)
8. [`capabilities` & `skills`](#8-capabilities--skills)
9. [Auxiliary & bespoke sections](#9-auxiliary--bespoke-sections)
10. [Worked examples](#10-worked-examples)
11. [Quick reference](#11-quick-reference)

---

## 1. The big picture

An entity's JSON has a flat set of **top-level keys**, each of which is one of two things:

- a **reserved section** — a fixed keyword with a defined meaning (`enables`, `requires`, `allowed`, `grants`,
  `identity`, `cost`, …); or
- a **modifier family** — a per-turn effect this entity produces (`food`, `production`, `happiness`, `maintenance`,
  one per `PROPERTY_*`, …).

**How to tell them apart (the classification rule):** a non-reserved top-level key whose value is an **object** is a
**modifier family** (it will be scope-keyed, §6). A non-reserved key whose value is a **bare** bool/string/number is a
**capability/skill flag or a text field**, never a family. So the *shape of the value* decides "family vs flag," and a
section's *name* decides its meaning. A family that collides with a reserved word is an error.

Everything below is detail under this one idea: **one object structure, one shared vocabulary, composed everywhere.**

---

## 2. Anatomy of an entity

| group | sections | what they are |
|---|---|---|
| **Availability** | `enables` · `obsoletes` · `replaces` · `disables` · `requires` · `allowed` | what this unlocks/removes, what it needs to be built & to keep running, and the cap on how many may exist |
| **Provisions** | `grants` | one-shot / recurring things this hands out (units, buildings, pulses) |
| **Effects** | every **modifier family** key (`food`, `production`, `happiness`, …, one per `PROPERTY_*`) | per-turn magnitudes this deposits onto targets |
| **Intrinsic** ("what am I") | `identity` (incl. all TEXT) · `cost` · `ui` · `world` · `sound` · `ai` | empire-agnostic self-description, art, audio, AI metadata |
| **Capabilities** | `capabilities` (TEAM, tech-unlocked) · `skills` (UNIT, innate) | boolean abilities; scope carried by the section name |
| **Auxiliary / bespoke** | `loadPrune` · `policies` · `succession` · `excludes` · `produces` · `condition` · `effect` · `vision` · `outcomes` · `mapGeneration` · `replacedBy` · `promotionLine` · `buildUp` · `shrine` · `properties` · `voteSource` · `threshold` · `role` · `victory` · `targetLevel` · `conversion` · `cityFounding` · `unitCapability` | data read by their own systems, not the cascade |

`type` (the entity's own id, e.g. `"BUILDING_FORGE"`) and the TEXT fields are always present where relevant.

---

## 3. The shared vocabulary

The same atoms compose conditions, count-scalers, grants, and modifier targets. Learn them once.

### 3.1 Types, tokens, and `SELF`
- **Data Types** — the `PREFIX_NAME` ids: `BONUS_COAL`, `UNIT_AXEMAN`, `TECH_POTTERY`, `BUILDING_FORGE`, … The prefix
  identifies the kind (`BUILDING_`, `UNIT_`, `BONUS_`, …).
- **Catch-all tokens** — engine concepts that aren't data Types: `TURN`, `POPULATION`, `MILITARY`, `CITY`, `TEAM`,
  `UNIT_LEVEL`, `AREA_SIZE`, …
- **`SELF`** — "this entity's own type," resolved per-entity. Used in a `per` count-scaler ("per how many of me exist").

### 3.2 Scopes — the containment spine
From widest to narrowest:

```
world → team → empire → area → city → plot{improvement|feature|terrain|route} → building | specialist | unit
```

`empire` = the player (all their cities). A `unit`-scope effect is a **self-accumulator** (it lands on the unit
itself). A scope says *where* something applies or *where* a count is taken.

### 3.3 Conditions — `all` / `any` / `noneOf`
A bounded boolean tree, used identically wherever a condition is needed (`requires`, and the `enabled`/`disabled` on a
deposit):

```jsonc
{ "all": [ … ],        // AND — every clause must hold
  "any": [ [ … ], … ], // OR  — a list of OR-groups (each group: at least one)
  "noneOf": [ … ] }    // NONE of these may be present
```

Each leaf is **either** a count/presence **atom** or a **predicate** (§3.4):

```jsonc
{ "type": "BONUS_IRON", "scope": "city", "connection": "trade|vicinity" }   // an atom
```

An **atom** is `{ type, scope, min?, max?, connection? }` and is always **fully explicit** — it carries its own `type`
and `scope`; the engine never infers them from context.
- **presence** = `min: 1` (i.e. "have at least one"). Authoring presence as `min:1` keeps it future-proof if a resource
  later gains amounts.
- **count thresholds** = `min: N` (≥ N) and/or `max: N` (≤ N). Both are **inclusive**. Exact-N is `min` and `max` together.
- `connection` (resources only) ∈ `"trade"` | `"vicinity"` | `"trade|vicinity"`.

> **Note — counts vs caps.** `min`/`max` here express what you **need** (a prerequisite count of *some other* type,
> e.g. "≥12 Barracks"). "How many of THIS may exist" is **not** a condition — it is the [`allowed`](#44-allowed--caps) cap.

### 3.4 Predicates — a system's runtime-state query
A predicate asks the game state a yes/no question a static file can't hold ("is this the capital? is there a river?").

- **bare** (parameter-free) — written as a plain string:
  `IS_WATER` · `IS_FRESHWATER` · `IS_FLATLANDS` · `IS_HILLS` · `IS_PEAK` · `HAS_RIVER` · `HAS_IRRIGATION` ·
  `IS_COASTAL` · `IS_CAPITAL` · `HAS_POWER` · `HAS_STATE_RELIGION` · `STATE_RELIGION_IN_CITY` · `HAS_FEATURE` ("has *any* feature").
- **parameterized** — written as `{ PREDICATE: parameter }`:
  `{HAS_FEATURE: FEATURE_X}` · `{HAS_TERRAIN: TERRAIN_X}` · `{HAS_BONUS: BONUS_X}` · `{HAS_RELIGION: RELIGION_X}` ·
  `{STATE_RELIGION: RELIGION_X}` · `{HOLY_CITY: RELIGION_X}` · `{HAS_CORPORATION: CORPORATION_X}` ·
  `{latitude: {min, max}}` · `{workedBy: SELF}` · `{existedFor: {min: N}}` (turns since built).
- **membership sugar** — `{ terrain|feature|bonus: [TYPE, …] }` = "the plot's terrain/feature/bonus is one of these"
  (a compact form for placement sets). Equivalent to an `any` of the matching `HAS_*` predicate.
- **negation** uses the `disabled` twin (§3.8) or a `noneOf` container — never a `false` value.
- An **unknown/missing predicate is ignored**, not treated as false — so retiring a system never spuriously disables
  unrelated data.

### 3.5 Units — what a modifier value *is*
A modifier magnitude names the **nature of the value**, not how the engine combines it:
- **`flat`** — an additive amount (`+2` = `2`).
- **`percent`** — an additive percent delta (`+50%` = `50`).
- **`multiplier`** — a true ×factor, identity `100` (`×2` = `200`).

(Plus rare `postMultiplier` / `rawPercent` — engine detail, seldom authored.)

### 3.6 `per` — count-scaling
Scale a value by how many of something exist:

```jsonc
"per": { "type": "POPULATION", "each": 5, "scope": "city" }   // value × (count / 5)
"per": { "anyOf": ["BONUS_COW","BONUS_PIG"], "scope": "city" } // value × (summed count of any listed)
```
`each` is the quantum ("per 5 population" → `each: 5`); state it explicitly. `scope` defaults to the deposit's own scope.

### 3.7 `interval` — recurrence (for repeatable grants)
`interval: { perTurn: N }` = every N turns; the bare string `interval: "perTurn"` = every turn.

### 3.8 The one entry shape
Every deposit, grant, or conditioned value is the same shape:

```jsonc
{ <payload>, "scope"?, "per"?, "enabled"?, "disabled"?, "ai"? }
```
- **payload** — a unit magnitude (`flat`/`percent`/`multiplier`: value), OR a grant (`type` + `count`), OR a predicate.
- **`scope`** — default: the containing scope. **`per`** — default ×1. **`enabled`** — default true (applies only while
  the condition holds). **`disabled`** — default false (suppressed while the condition holds). **`ai`** — an optional
  sibling block applied for AI players only (same inner shape).
- A leaf may be a single entry **or a list of entries** (several conditioned values into one slot).

```jsonc
"production": { "city": { "percent": [
    25,                                                       // always +25%
    { "value": 25, "enabled": { "min": ["BONUS_COAL", 1] } }  // +25% more while coal is connected
] } }
```

---

## 4. Availability

The availability sections decide **what is offered** — what unlocks what, what an entity needs, and how many may exist.

### 4.1 `enables` — what this unlocks (permanent)
Listed on the **source**, per target-kind. Having this entity makes its `enables` targets reachable.

```jsonc
"enables": { "units": ["UNIT_CROSSBOWMAN"], "buildings": ["BUILDING_BANK"] }
```
Buckets: `buildings · units · builds · techs · civics · religions · corporations · projects · processes · promotions ·
promotionLines · heritages · specialBuildings · specialBuildingsWaived · improvements · bonuses · routes · votes ·
hurries · traits · specialists`. **Tech unlocks live here** (a tech `enables` what it researches).

### 4.2 `obsoletes` / `replaces` / `disables` — removal (permanent)
- **`obsoletes`** — supersession: new builds are barred; existing instances persist (an obsolete unit stays on the map).
- **`replaces`** — succession: a successor takes the predecessor's slot.
- **`disables`** — a destructive, reversible **ban** (e.g. a law banning a building): existing instances are removed;
  repeal means rebuild.

Same per-kind bucket shape as `enables`.

### 4.3 `requires` — what this NEEDS (reversible)
The means a target needs, on the **target**. Two timings:

```jsonc
"requires": {
  "build":   { "all": [ {"type":"BONUS_STONE","scope":"city","connection":"trade|vicinity"} ] },
  "operate": { "all": [ {"type":"CIVIC_GUILDS","scope":"empire"} ] }
}
```
- **`build`** — needed to construct it (greyed out if missing). Checked once, at build.
- **`operate`** — needed to construct **and** to keep running; if lost later the built thing goes **dormant** (inactive,
  not destroyed) and wakes when it returns. (Units carry `build` only.)

Each is an `all`/`any`/`noneOf` tree (§3.3). A single bare predicate may also be given as a `disabled`/`enabled` clause:

```jsonc
"requires": { "build": { "disabled": "IS_CAPITAL" } }   // can't build where a capital already exists
```

`requires` holds genuine **needs** (resources, civics, religion, or count thresholds of *other* types). It does **not**
hold "how many of myself" — that's `allowed`.

### 4.4 `allowed` — caps
How many of this entity may **exist**. Author the **real number**; the engine permits a build while the current count
is below it. Absent ⇒ uncapped. Two shapes, told apart by the key:

```jsonc
"allowed": { "world": 1 }     // self-cap: at most ONE of me anywhere (a world wonder; a globally-unique tech)
"allowed": { "empire": 1 }    //           at most one per player (a national wonder; a unique unit)
"allowed": { "team": 1 }      //           at most one per team (a team wonder)
```
- **self-cap** — a **scope** key (`world`/`team`/`empire`) = "at most N of *me* at that scope." For a building, the
  cap scope is also what makes it a world / team / national wonder.
- **category count-cap** — a **wonder-category** key, used on `CultureLevel` to cap how many of a category a *city* may
  hold:

```jsonc
"allowed": { "worldWonders": 3, "teamWonders": 2, "nationalWonders": 8 }
```
(`totalWonders` is reserved for an all-categories cap.)

Caps you do **not** author: the engine owns ignoring caps under the relevant game options, any era-scaling of a base
cap, and per-entity exceptions — you just declare the number.

---

## 5. `grants` — provisions

One-shot or recurring things an entity hands out (not per-turn modifiers).

```jsonc
"grants": {
  "techs": ["TECH_POTTERY"],                 // entity lists
  "units": ["UNIT_WARRIOR"],
  "population": 1,                            // a numeric pulse
  "foundBuildings": [ {"building":"BUILDING_PALACE"} ],
  "repeatable": [                            // recurring, optionally chance-rolled
    { "unit": "UNIT_PROPERTY_CRIMINAL", "interval": "perTurn",
      "chance": { "per": { "type": "PROPERTY_CRIME", "scope": "city" } } }
  ]
}
```
- **lists** — `buildings · units · techs · civics · specialists · promotions · traits · bonuses · freePromotions ·
  foundBuildings`.
- **numeric pulses** — `grants.<channel>: value` (e.g. `grants.revolution: -100`, `grants.goldenAge`).
- **`repeatable`** — `[ { <payload>, interval, chance?, enabled? } ]` — fires each interval (a spawned unit, a heal),
  optionally gated by a rolled `chance` (which may scale with a `per`).

---

## 6. Effects — modifier families

A modifier family is a per-turn effect this entity deposits. It is addressed:

```
<family>.<scope>[.<targetType>.{TARGET}][.<member>].<unit> = value
```

```jsonc
"happiness":  { "city": { "flat": 2 } },                                   // single-concept family
"food":       { "city": { "improvements": { "IMPROVEMENT_FARM": { "flat": 1 } } } }, // targeted (keyed)
"maintenance":{ "empire": { "distance": { "percent": -10 } } }             // grouped family (member `distance`)
```

- **Split families** — one concept per key: yields are `food`/`production`/`commerce`; commerce splits into
  `gold`/`research`/`culture`/`espionage`; each property is its own family (`PROPERTY_CRIME`, `PROPERTY_EDUCATION`, …).
- **Grouped families** keep `<member>` parts (`maintenance`, `defense`, …).
- **Targeted deposits** key by `<targetType>.{TARGET}` (e.g. a building boosting one improvement's food) and stay on
  the source.
- The **unit plane** has its own family set (`strength`, `withdrawal`, `firstStrike`, `bombard`, `collateral`, `air`,
  `heal`, `movement`, `experience`, `workRate`, `cargo`, `vision`, `capture`, …).

Each leaf is the entry shape (§3.8) — so any deposit can carry `enabled`/`disabled`/`per`/`ai`.

**How they combine:** `effective = (base + Σflat) × (100 + Σpercent)/100 × Π(multiplier/100)`. You author the values;
the engine combines them.

---

## 7. Intrinsic

Empire-agnostic self-description. Read directly (never summed or cascaded).

- **`identity`** — "what am I": all **TEXT** lives here (`description`, `help`, `civilopedia`, `message`, `quote`,
  `strategy`, `adjective`, `shortDescription`) plus intrinsic flags/values (radii, classifications, capability bools…).
- **`cost`** — what it costs to make (`production`, and cost sub-fields).
- **`ui`** — interface art/sound (icons, buttons, movies). `world` — on-map 3D art. `sound` — audio assets.
- **`ai`** — AI-only metadata (flavours, weights, personality) — never affects rules, only AI behaviour.

---

## 8. `capabilities` & `skills`

Boolean abilities. The **section name carries the scope**, so the engine never has to guess:
- **`capabilities`** — **team-wide, tech-unlocked** civilization abilities (found-on-peaks, pass-peaks, move-on-water,
  tech-trading, irrigation, bridge-building, river-trade).
- **`skills`** — a **unit's innate** abilities (blitz, walk-on-mountains, fly-over-water, amphibious, move-impassable, …).

```jsonc
"skills": { "amphibious": true, "blitz": true }
```

---

## 9. Auxiliary & bespoke sections

Data read by a specific system, not by the cascade. Use only when the entity needs it:

- **`loadPrune`** — `{ onGameOptions, notOnGameOptions }`: drop this entity at load under given game options.
- **`policies`** — civ meta (`playable`, `aiPlayable`, …) **or** player-state law toggles (depending on entity).
- **`succession`** — `{ upgradesTo, promotionLine, priority }` (manual unit upgrade / promotion-line link).
- **`excludes`** — same-tier mutual exclusion (e.g. conflicting traits).
- **`produces`** — a Build's outcome FKs (what laying it creates).
- **`replacedBy`** — a conditional whole-entity swap (an alternate Info under a culture level / game option).
- **`condition`** (Victory) · **`effect`** (Vote) · **`vision`** (line-of-sight) · **`outcomes`** (mission results) ·
  **`mapGeneration`** (placement/spawn config).
- **bespoke**: `promotionLine` · `buildUp` · `shrine` · `properties` · `voteSource` · `threshold` · `role` · `victory` ·
  `targetLevel` · `conversion` · `cityFounding` · `unitCapability` — object-valued sections each read by their own system.

A dedicated system's data lives in its **own block** (so a system can be added, swapped, or removed as a unit); a module
is "on" iff its block exists and is non-empty.

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
*A Forge unlocks the Crossbowman; needs connected iron to keep operating; +25% production and (while powered) +1
happiness in its city; costs 120 hammers.*

### A world wonder (a cap + a conditional bonus)
```jsonc
{
  "type": "BUILDING_VERSAILLES",
  "identity": { "description": "TXT_KEY_BUILDING_VERSAILLES" },
  "allowed": { "world": 1 },
  "requires": { "build": { "disabled": "IS_CAPITAL" } },
  "buildRate": { "self": { "percent": 100, "enabled": { "type": "BONUS_MARBLE", "scope": "city", "min": 1 } } },
  "culture": { "city": { "flat": [ 10, { "value": 10, "enabled": { "existedFor": { "min": 1000 } } } ] } }
}
```
*Only one Versailles may exist in the world; you can't build it where a capital already sits; it builds itself twice as
fast with connected marble; +10 culture, doubling after it has stood 1000 turns.*

### A culture level (per-city wonder caps)
```jsonc
{
  "type": "CULTURELEVEL_DEVELOPING",
  "enables": { "buildings": ["BUILDING_TOWN_HALL"] },
  "allowed": { "worldWonders": 2, "teamWonders": 2, "nationalWonders": 8 },
  "defense": { "city": { "amount": { "percent": 12 } } }
}
```
*At this culture level a city may hold up to 2 world wonders, 2 team wonders, 8 national wonders, and gets +12% defense.*

---

## 11. Quick reference

**Top-level keys**
`type` · `identity` · `cost` · `ui` · `world` · `sound` · `ai` · `enables` · `obsoletes` · `replaces` · `disables` ·
`requires` · `allowed` · `grants` · `capabilities` · `skills` · *(modifier families)* · *(auxiliary/bespoke, §9)*

**Condition combinators** — `all` (AND) · `any` (OR-of-groups) · `noneOf` (NONE)
**Atom** — `{ type, scope, min?, max?, connection? }` · presence = `min:1`
**Predicate** — bare string, or `{PREDICATE: param}`, or membership `{terrain|feature|bonus:[…]}`
**Units** — `flat` (amount) · `percent` (+% delta) · `multiplier` (×, identity 100)
**Scopes** — `world › team › empire › area › city › plot{improvement|feature|terrain|route} › building|specialist|unit`
**Entry** — `{ <payload>, scope?, per?, enabled?, disabled?, ai? }`
**`requires`** — `build` (greys) / `operate` (greys + dormancy)
**`allowed`** — `{scope:N}` self-cap, or `{worldWonders|teamWonders|nationalWonders:N}` per-city category cap

---

*This is the modder-facing reference. The internal engine rationale and the migration's field-by-field mapping live in
`docs/dev/plans/` (`data-model-spec.md`, `enabler-cascade-spec.md`, `modifier-cascade-spec.md`,
`tally-cascade-spec.md`, `migration-renames.md`) and are not needed to author data.*
