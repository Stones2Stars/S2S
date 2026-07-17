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
| **Provisions** | `grants` · `provides` | `grants` = one-shot / recurring things this hands out (units, buildings, pulses); `provides` = a continuous in-vicinity SUPPLY while active (e.g. a building or map bonus that makes a `BONUS_*` available in the city) |
| **Effects** | every **modifier family** key (`food`, `production`, `happiness`, …, one per `PROPERTY_*`) | per-turn magnitudes this deposits onto targets |
| **Intrinsic** ("what am I") | `identity` (incl. all TEXT) · `cost` · `ui` · `world` · `sound` · `ai` | empire-agnostic self-description, art, audio, AI metadata |
| **Classification** | `skills` (UNIT, mutable abilities) · `tags` (UNIT, immutable type membership) · `state` (UNIT, transient) · `attributes` (BUILDING, held city-scope intrinsics) · `capabilities` (TEAM, grantor-provided) | §8 — the classification model; scope carried by the section name |
| **Applicability** | entity-level `enabled` · `disabled` | the whole entity applies only while `enabled` holds and `disabled` does not (the §3.9 pair at entity level) — the canonical whole-entity game-option gate: `"enabled": "GAMEOPTION_X"` |
| **Auxiliary / bespoke** | `policies` · `succession` · `excludes` · `produces` · `condition` · `effect` · `vision` · `outcomes` · `mapGeneration` · `replacedBy` · `promotionLine` · `buildUp` · `shrine` · `headquarters` · `properties` · `voteSource` · `threshold` · `role` · `victory` · `targetLevel` · `conversion` · `cityFounding` · `unitCapability` · `canTrade` (tech → the trade-table/deal system: tradeable items + agreements — `techs`/`openBorders`/`rightOfPassage`/`embassy`/`bonuses`/…) · `canTradeOn` (tech → trade-route system; terrain refs) · `canWorkOn` (tech → the city `canWork` gate; workable plot classes — `water`/`peaks`/…) — all three [capabilities.md](capabilities.md) | data read by their own systems, not the cascade |

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
  `TEAM`, `UNIT_LEVEL`, `AREA_SIZE`, **`ERA`** (the player's current era as a plain **counter 1…X** — the era
  sequence; eras are ordered data defined in `Assets/Data/eras/`), **`TARGET_NUM_CITIES`** (the world-size's
  expected city count — `CvWorldInfo.getTargetNumCities`, e.g. 2/3/4/6/8/11/14/18 per world size; a runtime-resolved
  world constant, used as a `max:` in ranked selection §3.3), **`WORLD_WONDER`** / **`NATIONAL_WONDER`** /
  **`TEAM_WONDER`** (the count of wonders of that category in the scope — `city` = `CvCity::getNum{World,National,Team}Wonders`;
  the existing engine terms, pedia display names TBD; used as a `per` count-scaler, e.g. a trait's free-specialist-per-
  wonder), … (an engine-resolved, extensible registry).
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

> **Ranked subset — `orderedBy` / `orderedByDescending` + `max:`.** A plural target may be
> narrowed to the **top-N (or bottom-N) by a metric** via an ordering qualifier plus the existing `max:` count:
> `cities.{ max: 5, orderedByDescending: CITY_SIZE }` = the **5 largest cities** (by population). `orderedBy` =
> ascending, `orderedByDescending` = descending (the standardized LINQ-style spelling); `max: N` caps the selection
> to N after ordering. This is a pure **extension** of `max:` (which both `grants` and conditions already use) — a
> bare `max:` with no ordering stays a plain count threshold, so nothing existing changes. Usable in **grants,
> conditions, and modifier targets**. **Metrics** are an extensible registry, `CITY_SIZE` (population) first. `N` is a
> literal (`max: 5`) or a world **token** when it tracks an engine constant — the *largest-cities* case is
> `max: "TARGET_NUM_CITIES"` (the world-size target city count; engine `getLargestCityHappiness` =
> `findPopulationRank() ≤ TargetNumCities`, i.e. the empire's largest *cities*, plural — **not** the single largest).
> Engine note: the cascade adds the sort/select step in **parsing**, one place for all ranking metrics; the
> `TARGET_NUM_CITIES` token needs a `/state` scalar not emitted today (batched engine add) — until then the
> largest-cities selection is inert. *(Status + open impl items: [plans/parked/ranked-target-selection.md](../plans/parked/ranked-target-selection.md).)*

### 3.4 Conditions — `all` / `any` / `noneOf`

A **recursive boolean tree**, identical wherever a condition is needed (`requires`, and a deposit's `enabled`/`disabled`). Three combinators;
each holds a list of **children**, and a child is **either a leaf** (a count/presence atom or a predicate, §3.5)
**or another combinator node** — nesting is allowed to any depth:

- **`all`** = **AND** (`&&`) — every child must hold.
- **`any`** = **OR** (`||`) — at least one child must hold. A plain OR over its **direct children** — *not*
  "OR-groups AND-ed together".
- **`noneOf`** = **NONE** — no child may hold.
- **`!` prefix** = **NOT** on a single leaf-string — `"!IS_STATE_RELIGION"` negates the
  predicate inline, so `all: ["IS_HOLY_CITY", "!IS_STATE_RELIGION"]` reads naturally ("a holy city that is NOT the
  state religion"). It is pure **shorthand for `noneOf:[X]`** on one leaf (the parser rewrites `"!X"` → `noneOf:[X]`),
  reusing the boolean tree — for negating a *group*, use `noneOf` with a nested node. (Less obvious at a glance than
  `noneOf`, but the standardized terse form; documented here.)

```jsonc
{ "all": [ leafA, { "any": [ leafB, leafC ] }, { "noneOf": [ leafD ] } ] }
//  ≡  leafA && (leafB || leafC) && !leafD
{ "all": [ "IS_HOLY_CITY", "!IS_STATE_RELIGION" ] }   // ≡ all:[ "IS_HOLY_CITY", {noneOf:["IS_STATE_RELIGION"]} ]
```

So `any` is exactly `||` on what is directly below it:

- `any: [BONUS_COPPER, BONUS_IRON]` = `copper || iron`.
- `any: [ {all:[STONE,IRON]}, {any:[COPPER,WOOD]} ]` = `(stone && iron) || (copper || wood)`.

To require BOTH "(copper or iron)" AND "(forge or foundry)", **nest two `any` nodes under an `all`** —
`all: [ {any:[COPPER,IRON]}, {any:[FORGE,FOUNDRY]} ]`. `any` never means AND — it is a plain recursive boolean
tree (`all`/`any`/`noneOf`, nestable to any depth).
Each leaf is **either** a count/presence **atom** or a **predicate** (§3.5):

```jsonc
{ "type": "BONUS_IRON", "scope": "city", "connection": "trade|vicinity" }   // an atom
```

An **atom** is `{ type, scope?, min?, max?, connection? }`. **Scope is IMPLIED from the type's domain** (derived from
the ID prefix) — TECH→`team`, civic/heritage→`empire`, building/bonus/religion/corporation→`city`.
State `scope` explicitly ONLY when it differs from that default (e.g. a `world`-scope victory, a `player`-scope tech).
So a **plain default-scope presence collapses to a bare type-string** — author the common case as a simple string
array: `"all": ["BUILDING_FORGE", "TECH_ASTRONOMY"]` ≡ `[{type:"BUILDING_FORGE"},{type:"TECH_ASTRONOMY",scope:"team"}]`.
Keep the object form only when a special case forces it: a `connection`, a count (`min`/`max`), or a non-default
scope. **Forcing a redundant `{type, scope}` only invites authoring bugs.** *(Plot-substrate
`{type:"TERRAIN_…"/"FEATURE_…"/"IMPROVEMENT_…"}` and `{type:"MAPCATEGORY_…"}` stay object-form — they are plot predicates, §3.5.)*

- **presence** = `min: 1` ("have ≥ 1"). Authoring presence this way keeps it future-proof if a resource later
  gains amounts.
- **count thresholds** — `min: N` (≥ N) and/or `max: N` (≤ N), both inclusive. Exact-N = `min` and `max` together.
- `connection` (resources only) ∈ `"trade"` | `"vicinity"` | `"trade|vicinity"`. `trade` = the city has the bonus via
  the trade network. `vicinity` = the bonus is on a tile in the city's radius.
- **`vicinity` DISCRIMINATOR** — `connection:"vicinity"` scopes a
  bonus to the city's workable radius; the optional sibling `vicinity:` field selects WHICH tiles count. A radius tile's
  ownership is one of three — and the distinction is load-bearing: **owned** (the city's team), **neutral** (unowned,
  `NO_TEAM`), or **foreign** (another team). The ownership selectors nest `owned ⊂ owned+neutral ⊂ owned+neutral+foreign`:
  - **absent** = **owned + neutral** — the **DEFAULT**: the city's own tiles plus unclaimed land,
    but NOT another team's. This mirrors the engine's vicinity (feature prereqs count neutral tiles too — `neutral`
    flag, `CvHttpServer.cpp`; terrain/improvement/peak/hill are `owned`-only via the next selector).
  - `"owned"` = **owned only** — strictly the city's own tiles (centre or owned radius tile; **no** connection or
    improvement needed), excluding even neutral. A raw owned-presence.
  - `"crossBorder"` = owned + neutral + **foreign** (any ownership) — the opt-in that ADDS foreign tiles, counting
    beyond the city's borders. **No current use-case, kept for completeness.** Name avoids the
    `all`/`any`/`noneOf` combinators (§3.4). A foreign tile's bonus is revealed per its OWN team, so it can read
    differently per asking city — exactly why foreign is gated behind this explicit opt-in rather than the default.
  - `"worked"` = a tile a citizen **works** this turn (implies owned).
  - `"connected"` = the bonus is **obtained** — owned + valid + connected to the city. *(`owned` and `connected` have
    OPPOSITE strictness — raw owned-presence vs a fully-obtained resource — so they are distinct, never folded.)*

  ```jsonc
  { "type": "BONUS_SHRIMP",   "scope": "city", "connection": "vicinity", "vicinity": "owned" }      // raw presence on an owned tile
  { "type": "BONUS_GOLD_ORE", "scope": "city", "connection": "vicinity", "vicinity": "connected" }  // must be obtained
  ```

- **`PROPERTY_*` band atom** `{type:PROPERTY_X, scope, min?, max?}` — its "count" is the city's property value;
  **absent `min` = no lower bound** (a max-only band), the one exception to the presence=`min:1` convention.
  Authored in `requires.operate`.

> **Counts vs caps.** `min`/`max` express what you **need** (a count of *some other* type, e.g. "≥12 Barracks").
> "How many of THIS may exist" is **not** a condition — it is the [`allowed`](#44-allowed--caps) cap.

### 3.5 Predicates — a system's runtime-state query

A predicate asks the game state a yes/no question a static file can't hold ("is this the capital? a river?"). It
is **evaluated against the deposit's target** and so carries **no `_PLOT`/`_UNIT` suffix** — the target supplies
context: `IS_WATER` on `plots` = a water tile, on `units` = a sea unit. An **unknown/missing predicate is
IGNORED**, never treated as false — retiring a system never spuriously disables unrelated data.

> **The predicate registry is EXTENSIBLE — and a condition is ALWAYS a predicate, never a bespoke member
> ([DEC-conditions-are-predicates]).** When a deposit's condition has no predicate named verbatim
> below yet, **define a new predicate** (add it here, wire it in the evaluator, and emit the `/state` fact it reads).
> Adding a predicate *extends* the model within the structure. What you must NOT do is encode the condition as a new
> sub-scope **member** (`{family}.empire.capital.percent`, `perMilitaryUnit`) — that changes the core structure (the
> kraken way; see [modifier.md §3](modifier.md), which also notes the **golden-age exception**: `empire.goldenAge`
> is a PERMANENT engine member-mirror (effect-only), because the yield effect is engine-core and not data-defined;
> golden-age length + grant ARE curated JSON (`goldenAge.empire.percent`, `grants.goldenAge`)).
>
> **`IS_*` vs `HAS_*` — literal English.** Plain English picks the prefix: `IS_*` = whether the
> target **is** something (a plot `IS_WATER`, a city `IS_CAPITAL`); `HAS_*` = whether it **has** something (a plot
> `HAS_RIVER`, `HAS_PEAK`, `HAS_COAST`). These semantics span **every target group**, not just plots — a *unit*
> `IS_MILITARY` (defined by the `military` [tag](tags.md)), a *city* `IS_CAPITAL` — so a **tag-backed predicate
> reads its tag** (that is how a [tag](tags.md) is queried inside a condition). The `HAS_*` set is the
> `<scope>.<target>` filter layer, so `HAS_COAST`
> matches **any** coastal-adjacent plot (water *or* land). The **target is the plot**:
> `HAS_PEAK` = the plot has a peak (a special case — a peak behaves as *both* a feature and a terrain, so a plot
> could *in theory* carry a terrain **and** a peak, e.g. grassland+peak; it just doesn't happen in practice). ⏳ The
> `HAS_COAST`/`HAS_RIVER`/`HAS_PEAK`/`HAS_HILLS` target-filters + `MAP_CATEGORY` are **not yet fully fleshed out**
> (space-map-related, in-flight) — author against them with that caveat.

- **bare** (parameter-free string), four groups:
  - **environment / domain** `IS_<where>` (target-relative): `IS_WATER` · `IS_LAND` · `IS_AIR` · `IS_SPACE` · `IS_LUNAR` · `IS_MARS`
    (extensible).
  - **relief form** `IS_FLATLANDS`: a plot with **no relief** — neither hills nor peak.
    It is relief-only (water is also relief-free), so **flat land** composes it with the domain: `{all:["IS_LAND","IS_FLATLANDS"]}`.
    The engine's per-plot-TYPE `PLOT_LAND` accumulator maps to exactly that pair.
  - **plot attributes** `HAS_<attr>` (relief & adjacency a plot carries, orthogonal to environment so they
    compose): `HAS_PEAK` · `HAS_HILLS` · `HAS_COAST` (adjacent to water) · `HAS_RIVER` · `HAS_FRESHWATER` ·
    `HAS_IRRIGATION` · `HAS_FEATURE` ("has *any* feature") · `HAS_LANDMARK` (the plot is an auto-detected geographic
    **landmark** — `getLandmarkType() != NO_LANDMARK`, i.e. bay/forest/jungle/peak/mountain-range/desert/lake; used by
    landmark-yield, which is also `GAMEOPTION_MAP_PERSONALIZED`-gated. NOT a natural wonder).
  - **plot city-relative state** (nested `VICINITY ⊇ WORKABLE ⊇ IS_WORKED`): `VICINITY` (in the city's workable
    radius) · `WORKABLE` (in radius and eligible to be worked) · `IS_WORKED` (a citizen works it this turn).
  - **world:** `NO_NUKES` (the world no-nukes verdict — true under the UN ban, false once nukes are enabled by anyone
    building the Manhattan Project). A nuke-enabling building (Manhattan) carries `requires.build.disabled: "NO_NUKES"`
    — it can't be built while nukes are forbidden.
  - **city / player:** `IS_CAPITAL` · `IS_GOVERNMENT_CENTER` · `HAS_POWER` · `HAS_STATE_RELIGION` · `STATE_RELIGION_IN_CITY` ·
    `IS_GOLDEN_AGE` (the player is in a golden age) ·
    **`IS_HOLY_CITY`** (the *bare* form = the city is a holy city of **any** religion — `CvCity::isHolyCity()`; the
    parameterized `{IS_HOLY_CITY: RELIGION_X}` below keys a specific religion) · **`IS_STATE_RELIGION_HOLY_CITY`** (the
    city is the holy city **of the player's state religion** — `isHolyCity(stateReligion)`; distinct from
    `STATE_RELIGION_IN_CITY`, which is merely *present*). *(The composed "holy city of a NON-state religion" — engine
    `isHolyCity() && !isHolyCity(stateReligion)` — is `all: ["IS_HOLY_CITY", "!IS_STATE_RELIGION_HOLY_CITY"]`, the canonical use of the `!` operator §3.4.)*
    The first two are **DISTINCT**: `IS_CAPITAL` = the city is the player's capital; `IS_GOVERNMENT_CENTER` = the city
    holds a government-center building (Palace or a pseudo-palace), runtime-evaluated. Government-center buildings gate
    on `requires.build.disabled: "IS_GOVERNMENT_CENTER"` (one can't be built where a government center already exists —
    a gov-center test, not an `IS_CAPITAL` one).
- **parameterized** `{ PREDICATE: param }`: `{HAS_FEATURE: FEATURE_X}` · `{HAS_TERRAIN: TERRAIN_X}` ·
  `{HAS_IMPROVEMENT: IMPROVEMENT_X}` (the plot carries that improvement — the plots-filter twin of terrain/feature) ·
  `{HAS_BONUS: BONUS_X}` · `{HAS_RELIGION: RELIGION_X}` · `{STATE_RELIGION: RELIGION_X}` · `{IS_HOLY_CITY: RELIGION_X}` ·
  `{HAS_CORPORATION: CORPORATION_X}` · `{latitude:{min,max}}` · `{existedFor:{min:N}}` (turns since built) ·
  `{HAS_COAST:{minArea:N}}` (the city is adjacent to a water body of **≥ N tiles**; a bare `HAS_COAST` is coastal at
  the default threshold, so an entity needing a *larger* sea body carries the size here).
- **membership sugar** `{ terrain|feature|bonus: [TYPE,…] }` = "the plot's terrain/feature/bonus is one of these";
  equivalent to an `any` of the matching `HAS_*` predicate.
- **composition is the win:** a Martian peak is `{all:["IS_MARS","HAS_PEAK"]}`; coastal land
  `{all:["IS_LAND","HAS_COAST"]}`; flat land = `{all:["IS_LAND","IS_FLATLANDS"]}` (domain + relief). No bespoke
  "mars-peak"/"coastal-land" type.
- **negation** uses the `disabled` twin (§3.9) or `noneOf` — never a `false` value.
- (a `PROPERTY_*` band atom is the one exception to presence=`min:1` — absent `min` = no lower bound; §3.4.)

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
the unit-presence effect lives on the civic/trait that grants it, targeting each city.

> **Predicates vs tags.** `IS_*` predicates are **independent queries**, *not* tag-membership: `IS_LAND`
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
- **There is no `enabled: false`** — to conditionally SUPPRESS a deposit use `disabled` (its twin); an absent
  `enabled` means always-on.
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
- **`whenObsolete`** — the built instance's **fate once obsolete** (its `obsoletedBy` tech is held), authored
  **target-side** as a **full modifier tree in the §6 grammar** (channels · scopes · units · `enabled`/`disabled`
  predicates — a *separate* tree, not a gate on the normal families). When the building is obsolete its **normal
  modifier families stop and this tree applies instead**; the surviving output is authored directly here and may
  differ from the working values. **Absent/empty ⇒ the obsolete building contributes nothing — fully gone** (the
  default, matching the engine's remove-on-obsolete). The canonical use is a wonder keeping culture/tourism while it
  loses its working bonus: `"whenObsolete": { "culture": { "city": { "flat": 5 } } }`. *(The engine's clunky
  `getObsoletesToBuilding` culture-shell swap is what the curator reads to emit this tree.)*
- **`replaces`** — succession removal: a successor takes the predecessor's slot, removing it from the buildable set
  once the successor is itself buildable. Authored **target-side** as **`replacedBy.{kind}`** (the entities that
  hard-replace this one, e.g. `replacedBy.units`), mirroring `obsoletedBy`. The §9 `replacedBy` (whole-entity Info-swap
  under a culture-level / game-option) is the **same hard-replace mechanic** — one entity supersedes another — just a
  different trigger. *(Distinct from dormancy, where the predecessor stays inactive-but-kept: `requires.operate.dormant`, §4.3.)*
- **`disables`** — a **law/ban** that **destroys** the target (a policy forbidding a building; repeal ⇒ rebuilt
  from scratch). It is **not** the dormancy mechanism: a target that should go **dormant** while a condition holds
  (e.g. an observatory under blackened skies — it parks and auto-resumes, never nuked-from-orbit) carries
  `requires.operate.dormant` (§4.3), not a `disables`. The choice of *mechanism* IS the fate (enabler §2/§3).

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
  (inactive, not destroyed) and wakes when it returns.
- **`spread`** *(CORPORATIONS only)* — needed to **spread** into a city, evaluated by the spread system at
  spread time against the target city's owner — never the enabler. Same condition vocabulary; the grounded
  legacy case is the per-building empire-count need (`{type: BUILDING_X, scope: empire, min: N}`, from the
  corp `PrereqBuildings` table — authored by no shipped corp, served so future data lands live).
- **`build` and `operate` share the SAME conditional vocabulary**, including the **`dormant`** sub-clause. **Units
  carry `build` only** (a trained unit never goes dormant on resource loss; on-map behaviour is out of the cascade's
  `canTrain` scope). A unit's two upgrade relationships are **distinct gates** (the machine: [enabler](enabler.md)):
  - **`requires.build.dormant.all`** = the unit's *direct* upgrades (minus any that also `replace` it): dormant out of
    the buildable set only when **every** one resolves to a reachable-trainable unit. Fail-safe — default *not*-dormant.
  - **`replacedBy.units`** (the §4.2 `replaces` edge) = the superseders: a genuine removal, dropping the unit from
    buildable the moment any superseder is buildable.

  *(`identity.spawnOnly` (§7) is a separate never-buildable flag, not dormancy. A game-option prereq is a declarative
  `GAMEOPTION_X` condition in `requires.build`; a unit's resource/corp prereq `{HAS_CORPORATION:X}` requires the corp
  ACTIVE, vs a building's bare `CORPORATION_` = present.)*

Each is an `all`/`any`/`noneOf` tree (§3.4). A single bare predicate may be given as a `disabled`/`enabled` clause:

```jsonc
"requires": { "build": { "disabled": "IS_CAPITAL" } }   // can't be built in a city that is already a capital
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

- **SpecialBuilding group cap** — each member authors `identity.specialBuildingType: SPECIALBUILDING_X`; the group
  entity holds the cap (`allowed:{empire:N}`). Member→group is authored, group→members derived.
- **Units have no `team` cap** (units belong to players) — unit caps are `world`/`empire` only; for units `world`
  reads the lifetime-created count and `empire` the live count (buildings keep all three scopes).

The engine owns ignoring caps under the relevant game options, era-scaling, and per-entity exceptions — you just
declare the number. Enforcement reads the [tally](tally.md) count.

---

## 5. `grants` — provisions

One-shot or recurring things an entity hands out (not per-turn modifiers).

> **`grants` is ONLY genuine provisions handed out on a trigger.** What does NOT belong here (and where it lives
> instead): unit `buildings` (MISSION_CONSTRUCT) / `greatPersonAction` / `goldenAge` → **`missions`** (§8 — PERMANENT carve-out: missions/CvOutcome ground-up rework);
> `builds` → the **`builds`** block (§8); promotion `unitCombats`/`removesUnitCombats` → **`skills`**; project
> `grantsSpecialBuilding` → **`enables.specialBuildings`** (flips SpecialBuildingValid — unlocks, hands out nothing);
> corp `bonusProduced` → **`provides.bonuses`** (continuous supply, §5a); building `holyCity` → **`requires.build`**
> (a read-only "only in RELIGION_X's holy city" gate — `canConstruct`, `CvCity.cpp:2728`; the holy city is set by
> religion FOUNDING, never a building); building `traits` → **`enables.traits`** (held-trait, §8).
> **`freePromotions`** (building-list + trait-dict) folds into **`repeatable`**. And a **mission carries its
> `grants`** as its outcome (§8), so `grants` is both an entity-level handout and a mission's outcome payload.

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

- **lists** — `buildings · units · techs · civics · promotions · traits · bonuses · freePromotions ·
  foundBuildings`. ⛔ **`specialists` is NOT in the grants vocabulary:** free specialists never fit the grants
  model — they are the `freeSpecialists` MODIFIER family ([modifier.md §6](modifier.md), alive-with-source + the
  two-part amount/placement seam). *If anything is ever found that genuinely GRANTS permanent free specialists —
  surviving the destruction of its wonder/building/source — we deal with it then*; no machinery is built for the
  hypothetical.
- **`freePromotions`** (a building) — promotions granted at **END-TURN to every unit present in the city**.
  One mechanism, no on-move flag: a unit trained there is present at end-turn, and a unit that
  walks in and stays is covered the **same** way — there is no separate mid-turn/on-move trigger.
- **numeric pulses** — `grants.<channel>: value` (`grants.revolution: -100`, `grants.goldenAge`).
- **`foundBuildings`** — entry shape `{ "building": BUILDING_X, "enabled"?: <condition> }` (absent `enabled` =
  always placed). Lives on the **settler-type unit** (the founder), NOT on the civ. ⚠ Tolerated sugar, not
  load-bearing — plain `grants.buildings` on the settler would suffice (the engine iterates the founder's buildings
  at settle-time). **Curator follow-up:** `BUILDING_PALACE` (+ the other founder buildings) is currently *also* in
  ~48 **civilizations'** `grants.buildings` — the **wrong/redundant** placement; the settler's `foundBuildings`
  already carries it, so the civ-grant duplicate should be dropped. (Does NOT affect the enabler: the engine realizes
  the palace into the capital regardless, so the cascade's HAVE sees it either way.)
- **`repeatable`** — `[ { <payload>, interval, chance?, enabled? } ]`: fires each interval (a spawned unit, a
  heal), optionally gated by a rolled `chance` (which may scale with a `per`).
- **property pulses** — a per-turn `PROPERTY_*` change an entity emits (the engine's `PropertyManipulator`) is a
  `repeatable` entry carrying its **spatial intent**:
  `{ "PROPERTY_AIR_POLLUTION": -5, "interval": "perTurn", "on": "plot", "relation": "near", "distance": 1 }`.
  **Properties are first-class** (early design decision) — a property source is **never a parked raw block**; the
  grant carries the `on`/`relation`/`distance` so the (#429) spatial distribution reads its target from here. A
  scaling (non-`CONSTANT`) source carries a `per` count-scaler; a flat (`CONSTANT`) source is the bare amount.
  *(Curator migration from the legacy parked `properties` array — `curate_improvement.py` et al. via the shared
  property-source cleaner — is pending; tracked as curator-to-spec.)*

---

## 5a. `provides` — continuous in-vicinity supply

What an entity makes AVAILABLE in its city *while active* — distinct from `grants` (a one-shot/recurring handout).
The canonical case is a building or map bonus that supplies a `BONUS_*`: a tamed-animal herd / industrial farm
supplies its animal bonus, and a map bonus on a workable plot supplies itself. One uniform surface, so a
`connection:"vicinity"` requirement is satisfied by *any* provider in the city — plot bonus **or** active building.

```jsonc
"provides": { "bonuses": ["BONUS_CAMEL"] }
```

- **`bonuses`** — `BONUS_*` ids this supplies in-vicinity. A consumer's vicinity check unions, over the city radius,
  every provider's `provides.bonuses` (active buildings; map bonuses providing themselves). **Active only** — a
  building that is dormant/obsolete supplies nothing.

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
- **Grouped families** keep `<member>` parts (`maintenance`, `defense`, …): `maintenance` uses a `distance`
  member; `defense` uses an `amount` member (the additive defense %), with a `min` member for the floor.
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
  ⛔ **`identity` is STRICTLY self-description — NEVER a catch-all** (owner 2026-07-11): a datum that isn't "what am I"
  (e.g. per-religion spread strength) does NOT go here; it gets its own block (`spread`, §9). Reaching for `identity`
  because a value has no obvious home is the anti-pattern.
  Two buildability flags: `notConstructible` (excluded from the player production queue; placed by another system)
  and `autoBuild` (auto-placed in every city where `requires.build` holds); `autoBuild ⊂ notConstructible`.
  **Civilization selectability** lives here too: `playable` / `aiPlayable` (can a human / the AI pick this civ) —
  **load-only metadata, no gameplay relevance** (animals/barbarians/neanderthals are technically civilizations), so
  it is intrinsic self-description, not a `policy`.
- **`cost`** — what it costs to make (`production`, and cost sub-fields).
- **`ui`** — interface art/sound (icons, buttons, movies) · **`world`** — **on-map 3D art**: the `world.art` block
  carries the on-map art **tag ids** — the `ART_DEF_*` **art-define tag** plus the model / texture references it
  spans (art is more than the icon — models and textures too). **Only the tag ids live in JSON**; the art
  *definitions* stay in the ART XML (`CIV4ArtDefines_*`), resolved by `ARTFILEMGR` from the id (a `BUILDING_`/`UNIT_`
  entity keeps `getArtInfo()` = `ARTFILEMGR.get<X>ArtInfo(<the id>)`). `ART_`/`EFFECT_` ids are XML-only Types
  ([naming.md](naming.md)), *referenced* from here. · **`sound`** — audio assets.
- **`ai`** — AI-only metadata (flavours, weights, personality); never affects rules, only AI behaviour.

---

## 8. Classification — unit `skills`/`tags`/`state`, building `attributes` & empire `capabilities`

A unit's classification splits into **three blocks, distinguished by lifecycle**. The **operative test: *can a
promotion grant it?***

- **`skills`** — **mutable** unit abilities, gained/lost via promotions (`blitz`, amphibious, walk-on-mountains,
  fly-over-water, …). *Promotion-grantable ⇒ skill.* Glossary: [skills.md](skills.md).
- **`tags`** — **immutable, type-derived** membership: set at creation, re-set on **upgrade**, **purely for
  accounting** — overlapping (`military`/`civilian`/`worker`/`spy`/`gunpowder`/`mechanized`/…), counted by the
  engine/tally, **no behaviour or modifiers**. A tag is **queried via its `IS_<TAG>` predicate** — a unit
  `IS_MILITARY` ⟺ it has the `military` tag (§3.5) — while the *generic* `IS_*` predicates (`IS_WATER`) read game
  state, not a tag. *Not* promotion-grantable (a swordsman must upgrade to a rifleman to gain `gunpowder`).
- **`state`** — **transient** conditions (fired → counted down → over: `paralyze`/immobilise). **Greenfield** —
  never first-class; historically faked via pseudo-promotions + Python events.

The **empire** counterpart to unit `skills` is **`capabilities`** — **team-wide, unlocked** civilization
abilities (found-on-peaks, pass-peaks, move-on-water, tech-trading, irrigation, bridge-building, river-trade,
and the commerce sliders `setScienceRate`/`setCultureRate`/`setEspionageRate`). **Capabilities are empire-HELD but
grantor-PROVIDED** — a **tech**, a **civic**, or a **building** *provides* one, and the empire then *holds* it. A
grantor **provides**, never **holds**; a capability appears in a grantor's `capabilities` block to mean "I hand this
to the empire." (This is exactly parallel to a tech granting an ability — the same block, three grantor kinds.) The
**section name carries the scope**, so the engine never guesses. **Behaviourally nothing is *granted*:** the
empire's active set is **derived on query** — an enabler-style union over the currently-live sources (the enabler's
HAVE axis); "provides" is the data direction, not an apply event, so a capability lapses with its last live source —
headroom only: in practice no capability is ever disabled today. See [capabilities.md](capabilities.md).

A **building** additionally has its own **`attributes`** block — the building's **HELD**, immutable, **city-scope**
intrinsic capabilities: `nukeImmune`, `zoneOfControl`, `governmentCenter`, `providesFreshWater`, `borderObstacle`, …
Plain booleans, like `skills`/`capabilities`, and again the section name carries the scope (building). The
**hold-vs-provide distinction is load-bearing**: `attributes` are what the building *is/does itself* (held), while
its `capabilities` are what it *hands to the empire* (provided) — the opposite direction. So a building's
`nukeImmune` is an `attribute` (the building holds it), but its `setCultureRate` is a `capability` (the building
provides the slider to the empire).

**Two further unit blocks:**

- **`builds`** — the unit's per-type **`BUILD_*` repertoire** (which worker-builds it can perform), owned **per unit-type**
  (tech gates *which builds are unlocked* — via `enables.builds` / the BUILD's own prereq; `builds` is *which THIS unit
  can do*; NOT "all workers, tech-gated"), promotion-augmentable. Wired as an **intrinsic key** (the readJson base skips
  it; `CvJsonUnitInfo` parses it). Same shared-vocabulary word as `enables.builds` — a `BUILD_*` list either way; the
  enclosing section gives the relationship (`enables.builds` = "unlocks these," unit `builds` = "can perform these").
- **`missions`** *(PERMANENT carve-out — missions/CvOutcome ground-up rework)* — the actions a unit **performs**, each
  producing an **outcome**; **a mission carries its `grants`** — the outcome (what lands) IS the mission's grant payload.
  Unifies the hardcoded mission-abilities (MISSION_CONSTRUCT/DISCOVER/GOLDEN_AGE — mis-filed today as `grants.buildings` /
  `greatPersonAction` / `goldenAge`) AND the un-migrated **`CvOutcome`** system (`CvUnitInfo` `KillOutcomes` +
  `m_aOutcomeMissions` — data-driven MISSION→outcome-list with cost/conditions/kill; *"outcome system (no wrapper)"*).
  The distinction from `skills`: a **skill** is a standing/permanent property; a **mission** is an action (often
  consuming the unit). `grants` is therefore BOTH an entity-level handout AND a mission's outcome payload.

A **building's** `grants.traits` (a whole trait conferred on the **OWNER** empire *while the building is active*, reverting
on loss — `owner.setHasTrait`, civ-traits only, `CvCity.cpp:4614`) is the same grantor-**provides** / empire-**holds**
pattern as `capabilities` — but the held thing is a full **trait** (effect-bundle), not a boolean ability. So it re-homes to
**`enables.traits`** (the building contributes trait T to the empire's active-trait HAVE, which the modifier already reads),
NOT `grants` (it is held-while-active, not a one-shot handout).

```jsonc
"skills": { "amphibious": true, "blitz": true },   // UNIT, mutable
"tags":   { "military": true, "gunpowder": true }, // UNIT, immutable (type)
"attributes":   { "nukeImmune": true, "zoneOfControl": true }, // BUILDING, held city-scope intrinsic
"capabilities": { "moveOnWater": true, "setCultureRate": true } // empire-HELD, grantor-PROVIDED (tech/civic/building)
```

> **⚖ The classification categories are RUNTIME-GENERATED INFOS ([DEC-classification-infos]).** Every distinct
> authored block key mints one generated info at load — the camelCase key becomes an `INFOTYPE_NAME` id
> (`"setScienceRate"` → `CAPABILITY_SET_SCIENCE_RATE`; prefixes `SKILL_` / `TAG_` / `ATTRIBUTE_` / `CAPABILITY_` /
> `POLICY_`, [naming.md](naming.md)) registered in the global infotype map and created as an info in its category's
> repo — *"clear data to refer to, even if they are only in essence a boolean switch."* Nothing is authored per
> category (no data folder): the registry derives from the union of keys across all entities
> (`ClassificationRegistry`, minted append-only per load), and every entity's blocks resolve their names to by-id
> bitsets, so the whole classification getter surface is an O(1) bit test — never a per-call string lookup
> ([DEC-materialize-at-mapfrom](../architecture/decisions.md#dec-materialize-at-mapfrom)). A block authors BOTH
> planes: `true` = grant, `false` = revoke (the skills.md §4 grant/revoke pairs ride the same two-plane block).

Full glossaries: [skills.md](skills.md) · [tags.md](tags.md) · [state.md](state.md) · [capabilities.md](capabilities.md).

---

## 9. Auxiliary & bespoke sections

Data read by a specific system, not the cascade. Use only when the entity needs it:

- **`policies`** — **pure empire STATES** an entity enacts: named declarative on/off conditions of the whole
  civilization (`noForeignTrade`, `noCorporations`, `allReligionsBanned`, `fixedBorders`, `noNonStateReligionSpread`,
  …). Granted by a **civic** (adopted — active while the civic is in force) or a **trait** (permanent while the trait
  is held) — one meaning, two grantors, exactly parallel to a tech granting a [capability](capabilities.md). **A policy
  is a PURE STATE, never a parameterized/targeted rule:** `allReligionsBanned` ✓;
  `onlyAllowedToBuildReligion: X` ✗ — a *targeted* restriction that carries a WHAT is an [enabler](enabler.md) concern
  (`enables`/`disables`/`requires`), not a policy. This is the group-unambiguity discipline (each group name = exactly
  one meaning; cf. empire `capabilities` vs unit `skills` vs `tags`). *(NOT here: civilization selectability
  `playable`/`aiPlayable` → `identity` §7, load-only; the NPC `stronglyRestricted` build-lockdown is a `requires.build`
  civ-membership gate paired with `EnabledCivilization`, folded into the enabler when civilizations are wired — not a
  policy. ⏳ Some legacy trait keys under `policies` are EFFECTS not states: `freeSpecialistPer{World,National,Team}Wonder`
  add free specialists scaled by wonder count (CvCity:5764) → reclassify to a `freeSpecialists` modifier family.
  (NB `nonStateReligionCommerce` was *suspected* an effect but is VERIFIED a pure STATE — a Free-Church permission that
  non-state religions' `stateReligionCommerce` applies — so it correctly STAYS a policy.)*
- **`succession`** — `{ upgradesTo, promotionLine, priority }` (manual upgrade / promotion-line link).
- **`excludes`** — same-tier mutual exclusion (conflicting traits).
- **`produces`** — a Build's outcome FKs (what laying it creates).
- **`replacedBy`** — a conditional whole-entity swap (an alternate Info under a culture level / game option; e.g.
  `CULTURELEVEL_ALT_POOR`). *(NOT the building `ReplacementBuildings`, which is reversible dormancy → `requires.operate.dormant`, §4.2/§4.3.)*
- **`condition`** (Victory) · **`effect`** (Vote) · **`vision`** (line-of-sight) · **`outcomes`** (mission
  results) · **`mapGeneration`** (placement/spawn config).
- **`shrine`** — the building is a religion's SHRINE: `shrine: RELIGION_X` (the religion FK). The per-commerce
  VALUES live on the **religion** (`religion.shrine`), scaled per city holding the religion; the building declares
  only the relationship. A top-level section, not an `identity` marker — the shrine relationship IS the data.
- **`headquarters`** — the corp-HQ analog of `shrine`: the building is a corporation's HEADQUARTERS,
  `headquarters: CORPORATION_X` (the corporation FK). The per-commerce values live on the **corporation**, scaled
  per corporation presence. Same FK-relationship shape as `shrine`, one for religion and one for corporation.
- **`spread`** (UNIT) — the unit's per-religion / per-corporation **spread strength** (a standing capability, NOT a
  timed handout): `spread.religion: { RELIGION_X: N }` / `spread.corporation: { CORPORATION_X: N }` — keyed magnitude
  maps (`N` = the legacy `iReligionSpread`/`iCorporationSpread`). Its **own** block on purpose (owner 2026-07-11):
  burying spread strength under `grants` (one-shot/recurring handouts) misleads a modder — it is what the unit is
  *able* to spread and *how strongly*, read by the missionary / corporate-executive spread systems.
- **`sizeMatters`** — the data the **Size-Matters** combat system needs (gated by `GAMEOPTION_COMBAT_SIZE_MATTERS`),
  a dedicated block per the own-block rule below. It is **cross-entity** — "size matters is mostly governed in
  unitcombat" — so the **base ranks are authored on UnitCombat** (the source) and summed onto the unit at load, while
  **Promotion** carries the runtime deltas:
  The block's keys are the same across entity kinds; the **base-vs-delta semantic is carried by the entity**, not a
  key suffix. Members: base ranks `qualityBase`/`groupBase`/`sizeBase` (UnitCombat only); the scalars `quality`,
  `group`, `sizeModifier`, `maxHP`; `combatModifier: { perSizeMore, perSizeLess, perVolumeMore, perVolumeLess }`;
  `cargo: { smSpace, volume, volumeModifier }`.
  - **UnitCombat** (the intrinsic **base ranks** + SM combat data): carries `qualityBase`/`groupBase`/`sizeBase` plus
    `maxHP`/`combatModifier`/`cargo`. A base equal to the legacy `−10` "unset" sentinel is emitted **absent** (never
    `0` — `0` is a real rank).
  - **Unit** (its own SM fields): `combatModifier` (its per-rank combat mods), plus `groupSize`/`baseCargoVolume`
    where authored. The unit's quality/group/size **RANK is DERIVED at load, never stored**: `Σ` over the unit's
    combat classes (primary `combatClass` + the `combatClasses` subs) of each `*Base` where `> −10` — reproducing the
    engine post-load pass (`CvUnitInfo`: `m_iBaseGroupRank += getGroupBase()`). The group rank feeds `getUnitCountSM`
    (`count ⁄ 3^(groupRank−1)`), so a stubbed `0` divides by zero — the getter MUST return the real derived value.
  - **Promotion** (the SM **deltas** a promotion applies): `quality`/`group`/`sizeModifier`/`maxHP` +
    `combatModifier`/`cargo` — same keys, applied as changes when the promotion is gained.

  Effective runtime rank = the derived info base + `Σ` held-promotion changes + the engine merge/split accumulators
  (`getExtraQuality`/`Group`/`Size` — live engine state, **never** data). Block absent ⇒ the entity carries no SM
  data. *(This is the pattern for every game-option-specific system — each gets its own block, e.g. `hideAndSeek`
  when `GAMEOPTION_COMBAT_HIDE_AND_SEEK` returns.)*
- **bespoke** object-sections, each read by its own system: `promotionLine` · `buildUp` · `shrine` · `headquarters` ·
  `spread` · `properties` · `voteSource` · `threshold` · `role` · `victory` · `targetLevel` · `conversion` ·
  `cityFounding` · `unitCapability`.

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
`replaces` · `disables` · `requires` · `allowed` · `grants` · `skills` · `tags` · `state` · `attributes` · `capabilities` ·
`shrine` · `headquarters` · *(modifier families)* · *(auxiliary/bespoke, §9)*

**Scope (singular)** — `world › team › empire › area › city › plot{improvement|feature|terrain|route} › building|specialist|unit` · off-spine `self` = the entity's own build
**Target (plural)** — `plots · units · cities · areas · empires` = all of that kind in the scope, predicate-filtered
**Combinators** — `all` (AND `&&`) · `any` (OR `||`) · `noneOf` (NONE), each over its direct children (leaf or nested node); a recursive boolean tree, nestable to any depth
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
