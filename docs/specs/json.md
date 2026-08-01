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
| **Provisions** | `grants` · `triggers` · `provides` | `grants` = pure payload on the source's considered action; `triggers` = trigger → chance → action (happening-fired / rolled / state-conditioned effects); `provides` = a continuous in-vicinity SUPPLY while active (e.g. a building or map bonus that makes a `BONUS_*` available in the city) |
| **Effects** | every **modifier family** key (`food`, `production`, `happiness`, …, one per `PROPERTY_*`) | per-turn magnitudes this deposits onto targets |
| **Intrinsic** ("what am I") | `identity` (incl. all TEXT) · `cost` · `ui` · `world` · `sound` · `ai` | empire-agnostic self-description, art, audio, AI metadata |
| **Classification** | `skills` (UNIT, mutable abilities) · `tags` (UNIT, immutable type membership) · `status` (UNIT, a per-turn counter -- applied, ticks down, over) · `attributes` (BUILDING, what the building itself is/does) · `amenities` (CITY-held, grantor-provided) · `characteristics` (PLOT SUBSTRATE, held plot-scope intrinsics) · `capabilities` (TEAM, grantor-provided) | §8 — the classification model; scope carried by the section name |
| **Applicability** | entity-level `enabled` · `disabled` | the whole entity applies only while `enabled` holds and `disabled` does not (the §3.9 pair at entity level) — the canonical whole-entity game-option gate: `"enabled": "GAMEOPTION_X"` |
| **Auxiliary / bespoke** | `policies` · `succession` · `excludes` · `produces` · `condition` · `effect` · `outcomes` · `mapGeneration` · `replacedBy` · `promotionLine` · `buildUp` · `shrine` · `headquarters` · `properties` · `voteSource` · `threshold` · `role` · `victory` · `targetLevel` · `conversion` · `cityFounding` · `unitCapability` · `canTrade` (tech → the trade-table/deal system: tradeable items + agreements — `techs`/`openBorders`/`rightOfPassage`/`embassy`/`bonuses`/…) · `canTradeOn` (tech → trade-route system; terrain refs) · `canWorkOn` (tech → the city `canWork` gate; workable plot classes — `water`/`peaks`/…) — all three [capabilities.md](capabilities.md) | data read by their own systems, not the cascade |

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
  sequence; eras are ordered data defined in `Assets/Data/eras/`), **the commerce slider rates** `GOLD_RATE` /
  `RESEARCH_RATE` / `CULTURE_RATE` / `ESPIONAGE_RATE` (the player's current slider percents as plain counters —
  a wellbeing deposit per-scaled on one is "happiness per 10% culture rate" / "anger per gold rate"),
  **`CULTURE_PERCENTAGE`** (the city's OWN-culture percent of its plot — the city-scope culture-share driver;
  foreign share authors as the `100 −` telescoping pair, never an inverse unit),
  **`CITY_LIMIT`** (SOURCE-resolved: the depositing civic's own city limit — its base-limit config × the
  world-size scale percent; the `per.above` threshold for the over-limit unhappiness class),
  **`DISTANCE_TO_GOVERNMENT_CENTER`** (plot distance from the city to its owner's NEAREST government centre,
  0 in one — the driver of distance maintenance; served from a maintained `CityContext` store, never a
  per-read walk),
  **`TARGET_NUM_CITIES`** (the world-size's
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
world › team › empire › city › plot{improvement|feature|terrain|route} › building | specialist | unit
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
> could *in theory* carry a terrain **and** a peak, e.g. grassland+peak; it just doesn't happen in practice). The
> `HAS_COAST`/`HAS_RIVER`/`HAS_PEAK`/`HAS_HILLS` target-filters follow the §3.5 predicate semantics; `MAPCATEGORY_`
> is an XML-only Type referenced from `requires.build` ([naming.md](naming.md)). Their space-map extensions are
> defined by the space-map work.

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
    **`IS_HOME_AREA`** (the city's area is the player's capital's area — the home-continent test; "other areas" is
    the plain negation `"!IS_HOME_AREA"`, never a second predicate. Retires the `homeArea`/`otherArea`
    condition-as-member authoring on `maintenance` — a maintenance modifier scoped to home/other areas authors as an
    ordinary conditioned deposit on this predicate) ·
    **`IS_REBEL`** (the city's owner is in revolt against a parent civ — the empire-state gate the rebel
    maintenance discount authors on, replacing four hardcoded halvings with one conditioned deposit) ·
    **`IS_HOLY_CITY`** (the *bare* form = the city is a holy city of **any** religion — `CvCity::isHolyCity()`; the
    parameterized `{IS_HOLY_CITY: RELIGION_X}` below keys a specific religion) · **`IS_STATE_RELIGION_HOLY_CITY`** (the
    city is the holy city **of the player's state religion** — `isHolyCity(stateReligion)`; distinct from
    `STATE_RELIGION_IN_CITY`, which is merely *present*). *(The composed "holy city of a NON-state religion" — engine
    `isHolyCity() && !isHolyCity(stateReligion)` — is `all: ["IS_HOLY_CITY", "!IS_STATE_RELIGION_HOLY_CITY"]`, the canonical use of the `!` operator §3.4.)*
    The first two are **DISTINCT**: `IS_CAPITAL` = the city is the player's capital; `IS_GOVERNMENT_CENTER` = the city
    holds a government-center building (Palace or a pseudo-palace), runtime-evaluated. Government-center buildings gate
    on `requires.build.disabled: "IS_GOVERNMENT_CENTER"` (one can't be built where a government center already exists —
    a gov-center test, not an `IS_CAPITAL` one).
  - **trade route** (evaluated against the ROUTE/its partner city): **`IS_FOREIGN`** (the route's partner belongs to
    another team — the engine's foreign-trade gate, `CvCity::totalTradeModifier`; domestic routes are the plain
    negation `"!IS_FOREIGN"`, never a second predicate) · **`SHARES_CIVIC`** (the route partner's owner runs the
    deposit's SOURCE civic — the shared-civic trade bonus; only meaningful on a deposit a civic authors).
- **parameterized** `{ PREDICATE: param }`: `{HAS_FEATURE: FEATURE_X}` · `{HAS_TERRAIN: TERRAIN_X}` ·
  `{HAS_IMPROVEMENT: IMPROVEMENT_X}` (the plot carries that improvement — the plots-filter twin of terrain/feature) ·
  `{HAS_BONUS: BONUS_X}` · `{HAS_RELIGION: RELIGION_X}` · `{STATE_RELIGION: RELIGION_X}` · `{IS_HOLY_CITY: RELIGION_X}` ·
  `{IS_HEADQUARTERS: CORPORATION_X}` (the city is that corporation's HQ — the corp analog of `{IS_HOLY_CITY: …}`;
  carries the corp-HQ revenue condition) ·
  `{HAS_CORPORATION: CORPORATION_X}` · `{latitude:{min,max}}` · `{existedFor:{min:N}}` (GAME YEARS since built -- what the player has always been told: *"doubles in 1000 years"*. The city stores the build YEAR (`getGameTurnYear`) and every authored threshold is a year count; a turn's year is derived, never stored ([engine.md](../reference/engine.md)), so nothing needs converting) ·
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
"per": "SPECIALIST"                                              // bare-string sugar: × count, each:1, own scope
"per": { "type": "POPULATION", "each": 5, "scope": "city" }      // value × (count / 5)
"per": { "anyOf": ["BONUS_COW","BONUS_PIG"], "scope": "city" }   // value × (summed count of any listed)
```

A bare string is the common case collapsed (the §3.4 bare-atom sugar, applied here): `"per": "SPECIALIST"` ≡
`{ "type": "SPECIALIST", "each": 1 }` at the deposit's own scope. Keep the object form only when a real `each`
quantum or a non-default scope forces it.

**`above:` — the over-threshold scaler (owner).** `"per": { "type": "CITY", "above": N }` scales by the count
EXCEEDING the threshold — `value × max(0, count − above)` — for the "per city over the limit" formula class.
The threshold is a literal or a **token** (the §3.3 threshold-token rule): `"above": "CITY_LIMIT"` reads the
depositing civic's own resolved limit. Composes with `each` (`(count − above) / each`) and the §3.9 gates.

`each` is the quantum ("per 5 population" → `each: 5`); state it explicitly. `scope` defaults to the deposit's own
scope; cross-city scopes (empire/team/world) resolve via the [tally](tally.md), `city`/`plot` are local.

**`unit: <predicate>`** qualifies a deposit by a **unit predicate** — the *same* `unit:` qualifier cargo uses
(`cargo.space.{unit: IS_AIR, …}`, [modifier](modifier.md) §6). On a count-scaling family it reads **per unit
matching**: `happiness.empire.cities.{unit: IS_MILITARY, flat: N}` = "N happiness per *military unit* stationed" —
the unit-presence effect lives on the civic/trait that grants it, targeting each city.
The qualifier generalizes by counted kind — the field NAMES what is counted and holds the filtering predicate:
`happiness.empire.cities.{religion: "!IS_STATE_RELIGION", flat: N}` = "N happiness per city religion matching"
(here: per non-state religion present in the city).

> **Predicates vs tags.** `IS_*` predicates are **independent queries**, *not* tag-membership: `IS_LAND`
> (used by cargo above) matches an intrinsic *domain*, not a `tag`. But a predicate **may be defined to encompass
> tags** (e.g. `IS_MILITARY` set up to match the `military` tag + similar) — predicates have **definitions**.
> **Post-migration:** make predicates **definable as JSON objects** and support **predicate groups** (compose
> them); for **migration they are HARDCODED** (`IS_MILITARY`/`IS_LAND`/`IS_AIR` baked into the curator).

### 3.8 Recurrence lives on the TRIGGER plane

There is no `interval` field. Anything recurring is a `triggers` entry (§5): the cadence is the trigger
(`"onTurn"`, `{ "onTurn": N }` = every N turns), the odds are its `chance`, the payload its `action`.

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
- **Target qualifiers ride the ENTRY too** — the §3.3 ranked-subset qualifiers (`max:` / `orderedBy` /
  `orderedByDescending`) and a counted-kind filter (`religion:` / `unit:` §3.7) may sit on an individual entry;
  a qualifier written at the target-node level is shorthand applying to every entry that carries none of its
  own. This is what lets ONE plural-target node hold differently-qualified deposits side by side (a largest-
  cities entry beside an every-city per-religion entry on the same `cities` node) — the entry is the universal
  carrier.
- **`enabled` is read before `disabled`** — the enable is evaluated first, the disable second; a `disabled`
  that holds **overrides** (the deposit is suppressed even if `enabled` was satisfied). Author them in that
  order: `enabled` first in the list, then `disabled`.
- **There is no `enabled: false`** — to conditionally SUPPRESS a deposit use `disabled` (its twin); an absent
  `enabled` means always-on.
- A leaf is a single entry **or a LIST of entries** (several conditioned values into one slot). **The list IS
  the formula mechanism (owner):** a composite formula authors as the SUM of its entries — a base term, `per`
  terms, negative companions, threshold-gated bands — composed side by side; there is deliberately NO
  expression syntax. The telescoping pair is the canonical idiom: `V × (count − N)` = `{V, per: <counter>}` +
  the flat companion `{−V×N}`, both under the same gate. What entry sums cannot express is a separate
  MULTIPLICATIVE stage — that is an engine-formula parameter (a config value), never forced into entries:

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

## 5. `grants` — pure payload on the considered action · `triggers` — when/why → odds → effect

> **⚖ TRIGGER IS THE TOP-LEVEL CONCEPT — A GRANT IS A TRIGGER WITH A NULL CONDITION (owner).** The two are one
> plane in the MACHINE: `triggers` is the general form (a happening, odds, an action) and `grants` is its
> **degenerate case** — the happening is implicit (the source's own considered action), there is no condition and
> no roll, so the action simply applies. That is why one engine and one spine domain (`SD_TRIGGERS`, the
> `[TRIGGERS/*]` tags) serve both, and why nothing about a grant needs its own machinery.
> ⚑ **`grants` remains a first-class AUTHORING shape** — *"we allow grants directly, but it works out of
> triggers"*. A modder writes the plain `grants` block below for the overwhelmingly common "acquiring me gives
> this" case and never spells out a trigger; the split that follows is therefore about AUTHORING, not about two
> runtime mechanisms.
>
> **The split (owner).** **`grants` holds ONLY what is given on the CONSIDERED ACTION** — the source's own
> realization moment, whatever that is: a building's construction, a tech's research, a civic's adoption, a
> settler's FOUNDING, a mission's execution. No trigger field, no odds, no recurrence — "acquiring/doing me
> gives this," full stop. **Everything that fires on anything ELSE is a `triggers` entry**: the WHEN/WHY comes
> FIRST (`per` — the cadence/happening: a turn, a population growth, …— plus `chance`, the odds), THEN what it
> does if successful — and the effect plane is wider than granting (burn down a building, spawn a unit, move a
> property, grant something). Odds live on the trigger, never inside a payload.
>
> **The entry anatomy is three named parts, in reading order: `trigger` → `chance` → `action`.**
>
> ```jsonc
> "triggers": [
>   { "trigger": { "type": "PROPERTY_FLAMMABILITY", "min": 200 },      // WHEN/WHY -- the shared §3 vocabulary
>     "chance": 5,                                                      // the odds (percent; may carry a per-scaler)
>     "action": { "destroy": { "building": "random" } } },              // WHAT IT DOES if successful
>   { "trigger": "onTurn",
>     "chance": { "value": 5, "per": { "type": "PROPERTY_CRIME", "scope": "city" } },
>     "action": { "grant": { "units": ["UNIT_PROPERTY_CRIMINAL"] } } }
> ]
> ```
>
> - **`trigger`** — the when/why, two composable forms and no bespoke dialect: an **`on<Happening>` token** —
>   the spine's DOMAIN events in authoring form (`onCreation`, `onFound`, `onTurn`, `onPopulationGrowth`, … an
>   OPEN registry, one name per spine fact) — and/or a **state condition** in the shared §3 vocabulary
>   (atoms/predicates/combinators — the fire band above); a state-only trigger is evaluated each turn. An
>   `on*` happening may be ANOTHER entity's moment in scope (a building acting `onCreation` of a unit in its
>   city; a trait acting `onFound` of each new city) — that is exactly what distinguishes a trigger from a
>   grant (the source's OWN considered action, implicit, never written).
> - **`chance`** — the odds, always here on the trigger, never inside a payload; scalable by the §3.7 `per`. A
>   `chance` carrying ONLY a `per` (no `value`) means the scaled count IS the odds — the roll is
>   per-count-derived (the property-spawn shape the data authors).
> - **`action`** — an OPEN verb registry: **`destroy`** · **`grant`** (the §5 payload vocabulary nested whole) ·
>   `spawn` / `place` / `promote` / property deltas / anything else the data needs — one verb vocabulary shared
>   with the §8 outcome plane (the outcome verb `triggers` renames to **`fires`** to clear this section's name).
>   ⚖ **The action's SUBJECT defaults to the entity the trigger is authored on — `"destroy": "self"` (owner).**
>   A verb needs no way to name its own carrier, exactly as a `grants` happening never names the source whose
>   considered action it is. ⛔ So do NOT extend the `SELF` count-token (§3.1) into a target vocabulary; the
>   off-spine `self` scope (§3.2) is the word, and the carrier is implicit everywhere else.
>
> ⚖ **A trigger may read an event from UP the containment spine — the plot hears its city (owner).** The spine
> (§3.2) puts `plot` directly under `city`, so a city's happening reaching the plot beneath it is an ordinary
> downward flow, not a special case: *"the city knows what plot it's on, so the plot can handle the events from
> the city — if a feature is on the plot, it reads the pop-increase event from the city, and voila"*. The worked
> case is a feature destroyed as its city grows: `trigger: {type: POPULATION, scope: city, min: N}` +
> `action: {destroy: self}`, off the `SEVT_POPULATION_CHANGED` fact the spine already carries.
> ⚑ This is what lets ONE condition replace a legacy special case: the engine destroyed such a feature at
> FOUNDING when its threshold was 0 or 1 and at `newPop >= N` thereafter — two code paths that are uniformly
> "city population ≥ N", since a founded city always has population ≥ 1.
> - The OUTCOME plane already conforms — a mission's roll (`chance` + the per-promotion `odds` table) is the
>   trigger, its verbs the action; nothing re-homes there.
>
> ⚖ **DIRECTION (owner, not yet specced): coin **`removes`** as the first-class MIRROR OF `grants`** — *"we can
> coin 'removes'; it's needed for triggers."* Today the take-away side exists only as scattered partial verbs
> (the `destroy` action verb here, `consumes` on the §8 outcome plane) while the give side has a whole named
> vocabulary; a trigger that takes something away has no counterpart to `grants` to reach for.
> ⛔ **It belongs on the PAYLOAD plane, NOT the enabler's — "it's different than the enabler loop" (owner), and
> that distinction is the thing to get right.** The enabler's `disables` / `obsoletes` / `replaces` are
> **availability RULES**: standing edges, evaluated continuously, governing whether a thing may be in the tree or
> keep running ([enabler.md §2](enabler.md)) — repeal the law and the building comes back. `removes` is a
> **one-shot PAYLOAD**: this action, now, takes this away, with no ongoing rule and nothing to re-evaluate —
> exactly the way `grants` is a payload rather than an availability edge. Building it as an enabler edge would
> make a momentary effect into a permanent rule.
> ⚠ If it lands it is a data-model change: the curator + regen ride in the SAME work item
> ([DEC-recurate-on-decision](../architecture/decisions.md#dec-recurate-on-decision)), and the scattered
> `destroy`/`consumes` verbs are the migration input.

> **`grants` is ONLY genuine provisions handed out on the considered action.** What does NOT belong here (and where it lives
> instead): unit `buildings` (MISSION_CONSTRUCT) → the **`constructs`** outcome verb under `outcomes.actions[]` (§8,
> one action per building — a construct is a mission-action producing an outcome, not a provision; emitted by
> `curate_unit.py`, no longer `grants.buildings`); `greatPersonAction` / `goldenAge` → **`missions`** (§8 — the rest of
> the mission-CONCEPT unification is a PERMANENT carve-out: missions/CvOutcome ground-up rework);
> `builds` → the **`builds`** block (§8); promotion `unitCombats`/`removesUnitCombats` → **`skills`**; project
> `grantsSpecialBuilding` → **`enables.specialBuildings`** (flips SpecialBuildingValid — unlocks, hands out nothing);
> corp `bonusProduced` → **`provides.bonuses`** (continuous supply, §5a); building `holyCity` → **`requires.build`**
> (a read-only "only in RELIGION_X's holy city" gate — `canConstruct`, `CvCity.cpp:2728`; the holy city is set by
> religion FOUNDING, never a building); building `traits` → **`enables.traits`** (held-trait, §8).
> **`freePromotions`** (building-list + trait-dict) is a **`triggers`** entry (end-turn presence → promote). And a
> **mission carries its `grants`** as its outcome (§8), so `grants` is both an entity-level handout and a
> mission's outcome payload.

```jsonc
"grants": {
  "techs": ["TECH_POTTERY"], "units": ["UNIT_WARRIOR"],   // entity lists
  "population": 1, "revolution": -100,                     // numeric pulses: grants.<channel>: value
  "buildings": [ { "building": "BUILDING_PALACE" } ]       // on a SETTLER: the considered action IS founding
}
```

- **lists** — `buildings · units · techs · civics · promotions · traits · bonuses · specialists`.
  ⛔ **`specialists` is a NARROW carve-out, and the test is the LIFETIME — never the payload.** Free specialists
  are ordinarily the `freeSpecialists` MODIFIER family ([modifier.md §6](modifier.md)): alive-with-source, dying
  with the building or civic that pays for them, on the two-part amount/placement seam. Authoring THOSE as a
  grant is the retired shape ([superseded-ideas #10](../architecture/superseded-ideas.md)) and stays retired.
  A specialist is a GRANT in exactly one case: a **persisted PULSE that outlives its source** — handed over
  once, never reclaimed. ⚑ The live instance is the trait's ERA-ADVANCE specialist: it fires on the era
  advancing (`onEraChanged`, §5's trigger plane — not the trait's own considered action) and lands in the
  city's **UNATTRIBUTED** typed-free ledger, so losing the trait does not take it back. ⛔ So the question to
  ask is never "is this a specialist?" but "does removing the source remove it?" — if it does, it is the
  modifier family and belongs nowhere near `grants`.
- **numeric pulses** — `grants.<channel>: value` (`grants.revolution: -100`, `grants.goldenAge`).
- **Founder buildings are PLAIN `grants.buildings` on the settler** — the settler's considered action IS
  founding, so no bespoke section exists; an entry may carry `enabled` (`{ "building": BUILDING_X, "enabled"?:
  <condition> }`). **A settler granting buildings at settle time is a NEW mechanic coined for this rework
  (owner) — there is no legacy engine apply to port.** Do not go looking for one: the legacy `bNewCityFree` path
  (`CvPlayer::found`, gated on `isNewCityFree()`) is a DIFFERENT, now-dead mechanic that merely sits at the same
  call site. This lands with the grants machine's apply-loop
  ([grants-machine.md](triggers.md) increment 5); the data is authored and
  waiting. The settler ALONE carries the founder buildings — no civilization authors a duplicate in its own
  `grants.buildings`.
- **Recurring / chance-rolled / state-conditioned handouts are NOT grants — they are `triggers` entries**
  (trigger → chance → `action.grant`): the old `repeatable` wrapper and its `interval` field dissolve into the
  trigger; the old building `freePromotions` (promotions to every unit present at end-turn — one mechanism, no
  on-move flag) is a `triggers` entry whose action promotes the units present.
- **Property pulses are `triggers` entries carrying spatial intent in the action** — a per-turn `PROPERTY_*`
  change an entity emits (the engine's `PropertyManipulator`):
  `{ "trigger": "onTurn", "action": { "PROPERTY_AIR_POLLUTION": -5, "on": "plot", "relation": "near",
  "distance": 1 } }`. **Properties are first-class** (early design decision) — a property source is **never a
  parked raw block**; the action carries the `on`/`relation`/`distance` so the (#429) spatial distribution reads
  its target from here. A scaling (non-`CONSTANT`) source carries a `per` count-scaler in its `chance`/value; a
  flat (`CONSTANT`) source is the bare amount. *(Curator migration from the legacy parked `properties` array —
  `curate_improvement.py` et al. via the shared property-source cleaner — is pending; tracked as curator-to-spec.)*

---

## 5a. `provides` — continuous in-vicinity supply

What an entity makes AVAILABLE in its city *while active* — distinct from `grants` (a one-shot/recurring handout).
The canonical case is a building or map bonus that supplies a `BONUS_*`: a tamed-animal herd / industrial farm
supplies its animal bonus, and a map bonus on a workable plot supplies itself. One uniform surface, so a
`connection:"vicinity"` requirement is satisfied by *any* provider in the city — plot bonus **or** active building.

```jsonc
"provides": { "bonuses": ["BONUS_CAMEL", { "BONUS_MOVIE": 6 }] }
```

- **`bonuses`** — `BONUS_*` ids this supplies in-vicinity. A consumer's vicinity check unions, over the city radius,
  every provider's `provides.bonuses` (active buildings; map bonuses providing themselves). **Active only** — a
  building that is dormant/obsolete supplies nothing.
- **Supply QUANTITY** — an entry is a bare `BONUS_*` string (count **inferred 1**, the common case) **or** a single-key
  object `{ BONUS_X: N }` carrying an explicit supply count. The count is the tradeable-supply amount (`getNumBonuses`
  += N), NOT vicinity presence — vicinity is presence-only, so it reads the *keys*. The canonical count case is a
  wonder that supplies several copies of a luxury (HOLLYWOOD → 6 movies; the legacy `iNumFreeBonuses`).

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
  member; `defense` uses an `amount` member, with a `min` member for the floor.
  ⚑ **`defense.amount` SUMS LIKE A FLAT and is APPLIED as a percentage (owner: "it is not a percentage in
  calculations, it's a flat sum added as a percentage for the defense calc") — which is exactly what the
  `percent` unit already means** (§3.6 / [modifier.md §2](modifier.md): percents are ADDITIVE DELTAS that sum
  and apply ONCE, never a per-source multiplier). So it authors `percent` and accumulates on the percent side;
  the value is measured in defense points, not scaled ([DEC-fixedpoint-x100](../architecture/decisions.md#dec-fixedpoint-x100):
  a percent is never ×100).
  ⚑ **The stack has a FLOOR and NO CEILING — verified, because the absence is load-bearing.** The floor is the
  `min` kind (`getExtraMinDefense`), applied at the realized read.
  ⛔ **THE WHOLE FAMILY IS `percent` — EVERY member, no exceptions to remember (owner).** The values are
  *technically* flat additive sums, and they are APPLIED as a percentage — *"it does increase combat of
  defending units by the percentage anyway"* — and **defense never carries decimals**, so the ×100 that the
  `flat` unit exists to buy ([DEC-fixedpoint-x100](../architecture/decisions.md#dec-fixedpoint-x100): the
  scaling exists ONLY to carry two decimals) is worth exactly nothing here. ⚑ **Uniformity is the requirement,
  not the label** — *"it does not really matter at the end of the day, as long as all defense modifiers do the
  same thing."*
  ⚖ **THE DECISION TEST, and it generalizes to every family (owner): does it SUM, or does it COMPOSE?** *"We
  are summing percentages; it would have been different if they were multiplicative."* An additive delta that
  sums and applies once is `percent` (unscaled); a factor that composes by product is `multiplier` (×100,
  identity 100) — §3.6. ⛔ Decide a kind's unit by that question, NEVER by the shape the legacy XML tag
  happened to have: the tags that read like modifiers were curated `percent` and the rest `flat`, which is the
  name-eyeballing [fixed-point-and-scales.md §3](curators/fixed-point-and-scales.md) bans outright.
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
  data-led behaviour change ([validation.md](validation.md): the spec leads), not an omission to restore.
- **⛔ THE MEMBER TRIAGE TEST — a member is a KIND only if it answers *WHICH component does this modify* (owner).**
  `defense.bombardDefense` and `maintenance.distance` name components, so they are genuine kinds. A member that
  answers **WHEN or WHERE** the value applies is a **condition-as-member rollerskate** — the predicate simply has
  not been defined yet ([DEC-conditions-are-predicates](../architecture/decisions.md#dec-conditions-are-predicates)),
  and it re-authors as a conditioned deposit (the worked case: `maintenance.empire.{homeArea,otherArea}` →
  `enabled: "IS_HOME_AREA"` / `"!IS_HOME_AREA"`, §3.5). Run this test on every proposed member: the scope axis and
  the conditions must never inflate a family's vocabulary. ⚑ A **`per<X>`-named member is its own verdict** — it IS
  a §3.7 `per` count-scaler (`perPopulation` → `per:{type:POPULATION}`), never a kind.
- **⛔ `strength` is the BASE; `combat` is everything that MODIFIES it (owner).** `strength.unit.flat` is the
  unit's base value and is absent if it cannot fight; every semantic modifier (`attack`, `defense`, `cityAttack`,
  `cityDefense`, `hillsAttack`/`hillsDefense`, `stealth`, `flanking`, `lunge`, …) plus the type-keyed vs-entries
  (`UNITCOMBAT_*`/`UNIT_*`/`TERRAIN_*`/`FEATURE_*`/`DOMAIN_*`) is `combat`, at unit/empire/team/city scope. ⛔ A
  concept with its own family never hides as a `combat` member: capture → `capture`, cargo → `cargo`, ranges →
  `air`/`range`, espionage defense → its own family.
  > **⚖ FLANKING IS KEYED BY UNITCOMBAT, NEVER BY UNIT (owner).** `combat.<scope>.flanking.{UNITCOMBAT_X}` is the
  > authored shape — *"mounted should be able to flank siege, is the very short answer."* A per-UNIT key names
  > individual units, so every unit nobody thought to list is silently un-flankable: *"right now flanking units
  > counter specific other units, which leaves grand-canyon-sized gaps in what is efficient for flanking."* The
  > class key closes them by construction, which is the whole reason it is the axis.
  > ⛔ **The per-unit table was encoding a BALANCE THEORY, and the theory is rejected (owner):** it existed so
  > that knights and horsemen could not flank cannons — *"but that is, pardon the pun, horseshit; if you manage
  > to get horses close, the artilleryman is just as fucked as any previous siege."* So there is no era gate and
  > no per-unit carve-out to preserve: getting cavalry onto a siege crew is the mechanic, and which century the
  > crew is from does not enter into it.
  > ⚑ This is the [engine.md](../reference/engine.md) UnitCombat distinction doing its job — a UNITCOMBAT is the
  > good/bad-AGAINST column, so a "vs" modifier keys on a CLASS. ⚠ It does NOT make flanking a per-target
  > enumeration: [skills.md §1](skills.md)'s note tying flanking to `targets`' narrow per-target granularity
  > describes the shape being retired, not the one to author.
- **⚖ `capture` carries BOTH WHAT YOU GET AND THE ODDS (owner).** `capture.unit.becomes` names the unit you
  receive for capturing this one; `capture.unit.probability` / `.resistance` are the odds. The two belong
  together because they are one mechanic answered from one place — splitting the result off into `identity` (or
  a bespoke block) would leave a reader holding the chance of an outcome the data never names.
  ⚑ So a family member is not required to be a magnitude: `becomes` is an FK, and that is the family stating its
  own outcome rather than a foreign concept hiding inside it.
- **THE COST CLUSTER IS THREE PLANES — do not merge them.** (1) The **actual cost** is the reserved `cost` section
  plus the entity's own self-data (`hurryCost` = "hurrying ME"; `buildTime`). (2) **What CHANGES a cost** is the ONE
  `costs` modifier family, kinds by category (`train`/`construct`/`create`/`build`/`research`/`improvementUpgrade`/
  `hurry`/`upgrade`), with **scope as the axis** — never a `world*`-prefixed kind
  ([DEC-scope-is-an-axis](../architecture/decisions.md#dec-scope-is-an-axis)). (3) The **derived price** (upgrade
  gold, hurry gold/pop) is engine-computed from planes 1 × 2; its formula parameters are world/handicap config, never
  vocabulary.
- **`underworld` is the in-city criminal contest** (criminals burrow, investigators drag them out): kinds
  `insidiousness` + `investigation`, at **city AND unit scope** — the city is the arena, the unit carries the
  stat (`UNITCOMBAT_CRIMINAL` vs `UNITCOMBAT_LAW_ENFORCEMENT`, resolved by
  `CvUnit::doInsidiousnessVSInvestigationCheck`). ⚠ **`detection` belongs to the hide-and-seek plane** — the
  map-level spotting of hidden units is a different system with its own block (`hideAndSeek`, §9), and must not be folded in here.
  > **⛔ UNDERWORLD IS NOT ESPIONAGE, AND THE LINE IS THE DEFINITION OF ESPIONAGE (owner): "espionage is only
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
  `adjective`, `shortDescription`), display/pedia placement, and metadata ABOUT the entity that produces nothing on
  its own (`conquestProbability`, `mapCategories`, AI worth).
  ⛔ **`identity` is STRICTLY self-description — NEVER a catch-all** (owner): a datum that isn't "what am I"
  (e.g. per-religion spread strength) does NOT go here; it gets its own block (`spread`, §9). Reaching for `identity`
  because a value has no obvious home is the anti-pattern.
  ⚖ **The worked case that PASSES — a unit's `domain` (owner).** Where a unit operates is empire-agnostic
  self-description that produces nothing on its own, so it is a genuine identity member rather than a value
  parked there for want of a home. ⛔ It is deliberately NOT a [tag](tags.md): a tag says what a unit IS, a
  domain says where it OPERATES, and answering the second from the tag set means filtering every tag for what
  one field holds. It is exclusive (no unit has two), and crossing a domain is a SKILL — a helicopter is a land
  unit with `canMoveAllTerrain` ([skills.md](skills.md)), never an air one.
  ⚖ **A DOMAIN IS THE MEDIUM, NOT THE PLACE (owner).** LAND is any solid surface — Earth, Mars, the Moon — so a
  planetary surface is categorised as land wherever it orbits, and infantry standing on it are LAND units.
  SPACE is the actual VOID, so only spaceships operate there (and they move over land AND space, the ordinary
  cross-domain SKILL shape above). ⛔ So "it is in space" is never a reason to give a surface unit a different
  domain, and a space domain is a question about SPACECRAFT, never about where a foot soldier is deployed.
  ⛔ **AND IT CARRIES NO EFFECTS AT ALL (owner).** Not "few", not "only intrinsic ones" — **none**. A value that
  DOES something has a home already and `identity` is never it:
  - a **held boolean ability** is a classification block — unit `skills`, building `attributes`, empire
    `capabilities` (§8). *`nukeImmune` is the worked case, and the data already proves the rule: **1367 buildings
    author it in `attributes` and 6 units in `skills`** — correct. The only misplaced ones are **29 FEATURES**
    authoring it in `identity`.* ⚠ Two facts that case exposes, both open: **plot substrate has no classification
    block** (§8 covers unit / building / empire and stops), and the feature's meaning is **not** the building's —
    a building's `nukeImmune` makes its CITY immune, a feature's means the FEATURE survives the blast. One word,
    two mechanics, different carriers; re-homing must not merge them.
  - a **magnitude** is a modifier family (§6) — a radius, a movement cost, a sight range, a cargo amount.
  - a **constraint on what may exist or be built** is `requires` / `allowed` (§4).
  - a **capability to trade / work / travel on something** is its own root block (`canTrade`, `canTradeOn`,
    `canWorkOn` — [capabilities.md](capabilities.md)).

  ⚠ The shipped data does not yet fully obey this — an effect authored into `identity` is a data error, and the
  re-home worklist lives in [todo.md](../plans/structural-cleanup/todo.md), never as a count here.
  Two buildability flags: `notConstructible` (excluded from the player production queue; placed by another system)
  and `autoBuild` (the placing system is the engine's own auto-placement); `autoBuild ⊂ notConstructible`.
  ⛔ **A `notConstructible` entity carries NO `requires.build` — placement is UNCONDITIONAL and DORMANCY decides
  everything** (owner). It is placed in every city and its `requires.operate` then makes it active or dormant, the
  uniform band model ([enabler.md §3](enabler.md)) applied to the whole queue-excluded class. `build` only ever
  greys a QUEUE candidate and is checked once; a queue-excluded entity is never a queue candidate, so a clause left
  in `build` would never be evaluated again. Authoring one is a data error — the curator folds it into `operate`.
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
  fly-over-water, …). *Promotion-grantable ⇒ skill.* **A skill is a PURE BOOLEAN ENABLER — the unit mirror of empire
  [`capabilities`](capabilities.md)** ("can walk over river", "can fly over water", "can pass peaks"): it carries
  **no value**, so a **UNIT authors `skills` as an ARRAY OF STRINGS** (`["pillage","blitz"]`), never `{name:true}` —
  a skill cannot be `false` (absent ⇒ not held). Anything that carries a value is **not a skill**: keyed
  targeting/immunity (`targets`/`unitTargets`/`defenders`) → the combat (`strength`) family; a per-type variant that
  collapses to one enabler stays a skill (`collateralImmune` = immune to the siege-variant collateral). Only where
  **revoke** is real — a **PROMOTION** granting *or removing* a skill — is the object form (`{name:true|false}`) used
  (the grant/revoke plane, skills.md §4). Glossary: [skills.md](skills.md).
- **`tags`** — **immutable, type-derived** membership: set at creation, re-set on **upgrade**, **purely for
  accounting** — overlapping (`military`/`civilian`/`worker`/`spy`/`landUnit`/`gunpowder`/`mechanized`/…), counted by
  the engine/tally, **no behaviour or modifiers**. Like skills, a tag is pure membership with **no value**, so a unit
  authors `tags` as an **ARRAY OF STRINGS that is ALWAYS PRESENT** (empty `[]` until the unitcombat→tags distillation
  fills it — never optional/absent; there is no real unit with zero tags, but the schema keeps the array mandatory).
  A tag is **queried via its `IS_<TAG>` predicate** — a unit `IS_MILITARY` ⟺ it has the `military` tag (§3.5), a unit
  `IS_LAND` ⟺ `landUnit` (the domain `DOMAIN_*` is a tag: `landUnit`/`seaUnit`/`airUnit`; the enum stays engine-side
  for movement/stacking) — while the *generic* `IS_*` predicates (`IS_WATER`) read game state, not a tag. *Not*
  promotion-grantable (a swordsman must upgrade to a rifleman to gain `gunpowder`).
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

A **building** additionally has its own **`attributes`** block — what the building **IS or DOES ITSELF**, held and
immutable: `teamShare`, `destroyedOnCapture`, `orbital`, `orbitalInfrastructure`. Plain booleans, like
`skills`/`capabilities`, and again the section name carries the scope (building). The **hold-vs-provide
distinction is load-bearing**, and it runs in three directions: `attributes` are the building's own property,
`capabilities` are what it *hands to the empire*, and `amenities` (below) are what it *hands to its own city*. So
`destroyedOnCapture` is an `attribute` (a fact about the building), `setCultureRate` is a `capability` (handed to
the empire), and `nukeImmune` is an `amenity` (it makes the CITY immune — the building is not the thing protected).

> **⚖ A UNIT CARRIES `status`, AND A STATUS IS A PER-TURN COUNTER (owner).** It is **a specific counter that gets
> DECREMENTED EVERY TURN** — applied to the unit, ticking down, and over when it reaches zero. That is the whole
> of what separates it from the unit's other blocks.
> ⚑ **So it is an id→COUNT like a city's `amenities`, but the COUNT MEANS SOMETHING ELSE, and the difference is
> the model:** an amenity's count is a REFCOUNT of live grantors (it moves when a grantor is added or repealed,
> which is what events do to it), while a status's count is **TURNS REMAINING** and moves on its own, every
> turn, with no grantor involved after the moment it was applied. ⛔ Do not fold the two onto one mechanism on
> the strength of both being id→COUNT: one expires, the other is held.
> ⚑ **The READ is therefore the ordinary `ContextDict` one (owner): the status HOLDS while its value is above
> zero** — `hasStatus(id)` ≡ `count > 0` ([contexts.md](../architecture/contexts.md)), so nothing needs a
> separate present/absent plane beside the counter. Expiry is the counter reaching 0, not a second fact.
> ⛔ A status is emphatically **NOT a [skill](skills.md)**: a skill is an ability the unit HAS, a status is a
> condition something PUT ON it, for a number of turns.
> ⚑ The worked case is **`paralyze`** (immobilises the unit — `setImmobileTimer`), applied by an EVENT through a
> status pseudo-promotion: the promotion is the DELIVERY mechanism, never the holder, so the flag must never
> land in a `skills` block. ⚠ It has been mis-filed as a skill more than once, so the curator no longer maps it
> — an unmapped tag reports LOUDLY rather than emitting into the wrong block. Glossary: [state.md](state.md).

A **CITY** has its own **`amenities`** block — the **city-HELD, grantor-PROVIDED** counterpart, standing to the
city exactly as `capabilities` stands to the empire. The hold-vs-provide axis otherwise stops one scope short:
`attributes` is what the building *is/does itself*, `capabilities` is what it *hands to the empire*, and
`amenities` is what it *hands to its own CITY* — which is what most authored keys actually do. The two split
by asking whose property it is:

- **`attributes` — about the BUILDING** — `teamShare` · `destroyedOnCapture` · `orbital` · `orbitalInfrastructure`.
- **`amenities` — conferred on the CITY** — `nukeImmune` · `governmentCenter` · `providesFreshWater` ·
  `providesPower` · `abolishedAnger` · `abolishedUnhealthFromPopulation` · `abolishedUnhealthFromBuildings` ·
  `adds3rdRing` · `borderObstacle` · `forceAllTradeRoutes` · `capital` · `protectedCulture` · `zoneOfControl`.

> **⚖ THE WELLBEING OFF-SWITCHES ARE ONE NAMED FAMILY — `abolished<Channel>` optionally `From<Source>` (owner:
> "a group of names that all tell the same story for different targets").** They are HARD off-switches, not
> modifiers ([modifier.md §2b](modifier.md)): the side ceases to exist rather than being reduced. The unqualified
> form abolishes the channel from EVERY source; a `From<Source>` suffix narrows it to one:
>
> | key | abolishes |
> |---|---|
> | `abolishedAnger` | anger, all sources |
> | `abolishedUnhealth` | unhealth, all sources |
> | `abolishedUnhealthFromPopulation` | the population term only |
> | `abolishedUnhealthFromBuildings` | the building term only |
>
> ⚑ It extends to the remaining wellbeing sources (features, bonuses, corporations, specialists — §2b's deposit
> list) without coining a new spelling each time, which is the whole point of the family: a reader meeting an
> unfamiliar one already knows what it does.
> ⛔ **The legacy spellings it retires each hid something in the NAME.** `noUnhappiness`/`noUnhealthyPopulation`
> used the `no…` negation; `buildingOnlyHealthy` named a CONSEQUENCE ("buildings are only ever healthy") rather
> than the mechanic; and `noCapitalUnhappiness` baked the WHERE in — which is the condition-as-member shape
> [DEC-conditions-are-predicates](../architecture/decisions.md#dec-conditions-are-predicates) retires, and it is
> now `abolishedAnger` gated `IS_CAPITAL`. ⛔ A future narrowing is a PREDICATE or a target, never a new key
> spelling.

⚑ **The grantor is not only a building, and the SCOPE says how far it reaches (owner: "a city or cities").** A
building's `amenities` land on its OWN city; a civic / trait / tech authoring the same block reaches EVERY city
of the empire — the ordinary scope spine (§3.2), on the same derived-union-over-live-sources mechanic
`capabilities` uses, so no new machinery and no per-grantor special case.

> **⛔ STORED AS AN ID→COUNT DICTIONARY, NEVER A BITSET (owner) — absent or 0 is false, anything else true.**
> *Several* sources can confer the same amenity, so the city holds a COUNT per amenity id and a removal
> decrements it: losing one power plant must not darken a city that has two. A bitset cannot express that — an
> "amenity removed" fact would clear a bit another live source still justifies. ⚑ This is the existing
> `ContextDict` (`id → count`, `has(id)` ≡ `count > 0`, [contexts.md](../architecture/contexts.md)), the same
> refcount shape the enabler's membership formula and the operating set's provided-bonus counts already use —
> and the semantic legacy had right all along in its per-flag counters. The city read is therefore O(1) and the
> ⛔ **REMOVAL-WINS trap is structurally absent**, exactly as it is for enabler membership.
> ⚑ **AND THE DICTIONARY IMMEDIATELY SOLVES VOLUMETRIC (owner) — a second payoff, free.** Because the slot is
> already an int rather than a bit, an amenity that later becomes a QUANTITY (power CAPACITY a city draws
> against, rather than a yes/no) needs **no reshape** — only a change in what the number means. That is the
> reasoning [contexts.md](../architecture/contexts.md) already applies to power specifically ("power carries 0/1
> today but stays `int` so a future volumetric model needs no reshape"), generalized to the whole block. ⛔ So
> the count is never to be "optimized" into a bitset on the grounds that every value happens to be 0 or 1 today.
>
> ⚠ `nukeImmune` is the standing exhibit for why the split is load-bearing: the same key means two DIFFERENT
> mechanics on two carriers — a BUILDING's makes its **city** immune (so it is an `amenity`), while a plot
> substrate's means the **feature itself** survives (a `characteristic`). One word, two mechanics, kept
> separable only by the blocks being distinct.

> **⚖ A GRANT MAY BE CONDITIONED — the ENTRY carries it, never the KEY (owner: "make the block carry the
> condition").** A classification entry's value may be the §3.9 entry object instead of a bare `true`, so a
> grantor states *when* its grant applies:
>
> ```jsonc
> "amenities": { "abolishedAnger": { "enabled": "IS_CAPITAL" } }   // a civic: the CAPITAL only
> ```
>
> ⛔ **The condition is evaluated PER RECEIVER, at FOLD time, on the grant AND on the repeal** — not once at
> load. A city that gains or loses capital status must therefore re-fold, exactly as one that gains or loses the
> grantor does; a gate read only on arrival strands the amenity wherever it last landed.
> ⚑ **This is what retires a WHERE baked into a key NAME.** The legacy `noCapitalUnhappiness` encoded its
> condition in its spelling — the condition-as-member shape
> [DEC-conditions-are-predicates](../architecture/decisions.md#dec-conditions-are-predicates) retires — so it is
> ONE key, `abolishedAnger`, gated by `IS_CAPITAL`, never a second key meaning "the same thing but over there".
> ⛔ And the reverse is equally binding: dropping the condition to keep the block a plain bitset would abolish
> anger in EVERY city. A shape whose only faithful reading is the wrong behaviour must not ship.

The **PLOT SUBSTRATE** has its own **`characteristics`** block — the **HELD**, immutable, **plot-scope** intrinsics
of whatever IS the plot: **terrain · feature · improvement · route**, all four carriers, one block and one registry
(the §3.2 spine's `plot{improvement|feature|terrain|route}`). Plain booleans like its siblings, the section name
carrying the scope. `countsAsPeak`, `actsAsCity`, `bombardable`, `zoneOfControl`, `ignoreTerrainCulture`,
`nukeImmune`, …

> **⚖ ITS OWN BLOCK, NOT `attributes` EXTENDED — names are never conflated (owner).** The substrate is a distinct
> carrier at a distinct scope, so it gets a distinct word rather than borrowing the building's. The cost of one
> more vocabulary entry is trivial; the cost of a name that means two things is paid forever by every reader who
> has to work out which one is meant.
> ⚠ **`nukeImmune` is the standing exhibit for why: the SAME key names two DIFFERENT mechanics on two carriers.**
> A BUILDING's `nukeImmune` makes its **city** immune; a plot substrate's means the **feature itself survives** the
> blast. Separate blocks keep them separable — a single shared block would have quietly merged them.

⛔ **The bound — does it describe THE THING, or WHERE THE THING MAY GO?**

- **A CHARACTERISTIC describes the substrate itself**, intrinsically and always, including what may be done *to*
  or *on* it: `unfoundable` (no city may be founded here), `unimprovable` (no improvement may be built here),
  `prohibitsBonus`, `bombardable`, `nukeImmune`, `actsAsCity`, `countsAsPeak`. These are read BY other systems —
  the founding gate, the worker-build gate, bonus placement — but they are properties of the feature, not
  conditions the feature is tested against.
- **A PLACEMENT CONDITION says where this substrate may EXIST**, and is evaluated against the plot's live state:
  `requiresFlatlands`, `validTerrains`, `noCoast` / `coastalOnly`, `noRiver` / `requiresRiver`, `noAdjacent`.
  Those author in `requires` (§4) like any other condition.

⚑ **The grammatical tell tracks the real one:** a characteristic is an adjective about the thing (`bombardable`,
`unfoundable`) — a placement condition names the ground it needs. ⚠ Do not read "it restricts something else" as
disqualifying: `unfoundable` restricts city founding and is still a characteristic, exactly as `bombardable`
restricts nothing and is one. The question is whose property it is, never who is affected by it.

**Two further unit blocks:**

- **`builds`** — the unit's per-type **`BUILD_*` repertoire** (which worker-builds it can perform), owned **per unit-type**
  (tech gates *which builds are unlocked* — via `enables.builds` / the BUILD's own prereq; `builds` is *which THIS unit
  can do*; NOT "all workers, tech-gated"), promotion-augmentable. Wired as an **intrinsic key** (the readJson base skips
  it; `CvJsonUnitInfo` parses it). Same shared-vocabulary word as `enables.builds` — a `BUILD_*` list either way; the
  enclosing section gives the relationship (`enables.builds` = "unlocks these," unit `builds` = "can perform these").
- **`missions`** *(PERMANENT carve-out — missions/CvOutcome ground-up rework)* — the actions a unit **performs**, each
  producing an **outcome**; **a mission carries its `grants`** — the outcome (what lands) IS the mission's grant payload.
  Unifies the hardcoded mission-abilities (MISSION_CONSTRUCT/DISCOVER/GOLDEN_AGE — mis-filed today as `grants.buildings` /
  `greatPersonAction` / `goldenAge`) with the **`CvOutcome`** system (`CvUnitInfo` `KillOutcomes` + `m_aOutcomeMissions`
  — data-driven MISSION→outcome-list with cost/conditions/kill). The CvOutcome DATA is ALREADY JSON-migrated into the
  `outcomes` block below (owner 2026-07-20); what this future `missions` block adds is the CONCEPT unification with the
  hardcoded abilities.
  The distinction from `skills`: a **skill** is a standing/permanent property; a **mission** is an action (often
  consuming the unit). `grants` is therefore BOTH an entity-level handout AND a mission's outcome payload.
  > **⚖ OUTCOME PAYLOAD VOCABULARY (owner 2026-07-20) — the `outcomes` block uses clean VERB-PER-PAYLOAD keys.**
  > `outcomes.kill[]` (combat-kill) / `outcomes.actions[]` (missions), each entry
  > `{ requires:{outcome:OUTCOME_*, plot?, unit?}, chance, <reward verbs> }`. Each effect is a verb, collision-checked
  > against the reserved words (avoiding `builds`/`provides`/`grants`/`construct`): **`constructs`** a building
  > (LOCKED) · **`spawns`** `{unit,toCity?}` · **`places`** a bonus · **`promotes`** · **`triggers`** an event ·
  > **`consumes`** the unit · reused families for one-shot yields (`food`/`production`/`commerce`/`gold`/…),
  > `greatPeople`/`population`/`revolution`, `happiness:{duration}`, `PROPERTY_*`; `{python}` for Python-authoritative
  > outcomes. **The engine CONSUMES this** — the `CvOutcome` classes are fed from it via `mapFrom` (the CvOutcome
  > engine/dispatch is unchanged, just JSON-loaded; conditions eval through `cascadeEvalCondition`, no BoolExpr
  > round-trip). `Adapt*` gamespeed scaling is pure-engine, applied at grant time — never in the data. See
  > [mission-outcome-system.md](../reference/mission-outcome-system.md).

A **building's** `grants.traits` (a whole trait conferred on the **OWNER** empire *while the building is active*, reverting
on loss — `owner.setHasTrait`, civ-traits only, `CvCity.cpp:4614`) is the same grantor-**provides** / empire-**holds**
pattern as `capabilities` — but the held thing is a full **trait** (effect-bundle), not a boolean ability. So it re-homes to
**`enables.traits`** (the building contributes trait T to the empire's active-trait HAVE, which the modifier already reads),
NOT `grants` (it is held-while-active, not a one-shot handout).

```jsonc
"skills": [ "amphibious", "blitz" ],                        // UNIT skills: pure boolean ENABLERS -> array of strings
"tags":   [ "military", "gunpowder", "landUnit" ],          // UNIT tags: immutable membership -> always-present string array
"combatClass":  "UNITCOMBAT_GUN",                           // UNIT: primary combat class -> ROOT (not identity)
"combatClasses": [ "UNITCOMBAT_SPECIES_HUMAN", "…" ],       // UNIT: sub combat classes -> ROOT
"strength": { "unit": { "flat": 26 } },                     // UNIT base STRENGTH (a modifier family, not identity.base; absent if it can't attack/defend)
"movement": { "unit": { "flat": 1 } },                      // UNIT base MOVES (the movement subsystem, not identity.base)
"attributes":   { "teamShare": true, "destroyedOnCapture": true }, // BUILDING: what it IS/DOES itself (held)
"amenities":    { "nukeImmune": true, "providesPower": true },     // city-HELD, grantor-PROVIDED (id->COUNT on the city)
"capabilities": { "moveOnWater": true, "setCultureRate": true }    // empire-HELD, grantor-PROVIDED (tech/civic/building)
```

> **⚖ The classification categories are RUNTIME-GENERATED INFOS ([DEC-classification-infos]).** Every distinct
> authored block key mints one generated info at load — the camelCase key becomes an `INFOTYPE_NAME` id
> (`"setScienceRate"` → `CAPABILITY_SET_SCIENCE_RATE`; prefixes `SKILL_` / `TAG_` / `ATTRIBUTE_` / `CHARACTERISTIC_` / `CAPABILITY_` /
> `POLICY_`, [naming.md](naming.md)) registered in the global infotype map and created as an info in its category's
> repo — *"clear data to refer to, even if they are only in essence a boolean switch."* Nothing is authored per
> category (no data folder): the registry derives from the union of keys across all entities
> (`ClassificationRegistry`, minted append-only per load), and every entity's blocks resolve their names to by-id
> bitsets, so the whole classification getter surface is an O(1) bit test — never a per-call string lookup
> ([DEC-materialize-at-mapfrom](../architecture/decisions.md#dec-materialize-at-mapfrom)). A block authors BOTH
> planes: `true` = grant, `false` = revoke (the skills.md §4 grant/revoke pairs ride the same two-plane block).
>
> **⛔ These registries are OPEN BY DESIGN — the member set grows with authored data, permanently (owner).** Because
> the categories mint from the union of authored keys, identifying new **tags / skills / capabilities / attributes / characteristics /
> policies** is an ONGOING activity expected to continue for the life of the mod — a new one is authored data that
> mints its info, never an engine change. So a glossary ([tags](tags.md) / [skills](skills.md) /
> [capabilities](capabilities.md) / [state](state.md)) is **never "incomplete" against a finish line**: it catalogues
> the members identified so far, and more arriving is the normal state, not a gap to close. The building counterpart
> of unit skills is **`attributes`** (held city-scope) — same open-registry rule.

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
  policy. Some legacy trait keys under `policies` are EFFECTS, not states: `freeSpecialistPer{World,National,Team}Wonder`
  (free specialists scaled by wonder count, CvCity:5764) belong to the `freeSpecialists` modifier family, keyed by
  the `WORLD_WONDER`/`NATIONAL_WONDER`/`TEAM_WONDER` count token (§3.1).
  (NB `nonStateReligionCommerce` was *suspected* an effect but is VERIFIED a pure STATE — a Free-Church permission that
  non-state religions' `stateReligionCommerce` applies — so it correctly STAYS a policy.)*
- **`succession`** — `{ upgradesTo, promotionLine, priority }` (manual upgrade / promotion-line link).
- **`promotionLine`** (PROMOTION) — `{ PROMOTIONLINE_X: rank }`, the promotion's rung on a named ladder.
  **⚖ A LINE IS A LADDER, AND HOLDING A RUNG IMPLIES THE RUNGS BENEATH IT** — each level's `requires.build`
  names the level below (`ACCURACY3` → `ACCURACY2` → `ACCURACY`), so a unit carrying the top of a line carries
  the whole chain. ⇒ **What that unit HAS from the promotion is the SUM down the line**, not the rung's own
  value.
  ⚑ **That is why there are TWO reads, answering different questions** — the promotion's own getters say what
  THIS RUNG contributes (what the pedia says *about* a promotion), and the line accrual says what a unit
  HOLDING it actually has (what the unit's tooltip shows). Neither approximates the other; a consumer picking
  the wrong one displays a number the unit does not have. The accrual's membership is derived ONCE at load and
  summed in exactly one place ([DEC-single-implementation](../architecture/decisions.md#dec-single-implementation)),
  never rebuilt per read.
  ⛔ **A STATUS promotion accrues only ITSELF** — status / affliction / equipment lines are parallel states
  rather than a ladder, so summing them would invent a compounding that does not exist.
- **`excludes`** — same-tier mutual exclusion (conflicting traits).
- **`produces`** — a Build's outcome FKs (what laying it creates).
- **`replacedBy`** — a conditional whole-entity swap (an alternate Info under a culture level / game option; e.g.
  `CULTURELEVEL_ALT_POOR`). *(NOT the building `ReplacementBuildings`, which is reversible dormancy → `requires.operate.dormant`, §4.2/§4.3.)*
- **`condition`** (Victory) · **`effect`** (Vote) · **`outcomes`** (mission results) · **`mapGeneration`**
  (placement/spawn config). *(**`vision`** is NOT here — it is an ordinary modifier family with its own machine,
  [vision.md](vision.md): a sight budget spent walking outward, exactly as movement works.)*
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
    where authored. ⚖ **`smSpace` is DERIVED, not authored (owner): under Size Matters a carrier's space follows
    from how many units it can carry** — so it derives from the `cargo` family's capacity, the same
    derived-at-load class as the ranks below. The data agrees: **no unit authors it** (legacy `iSMCargo` appears
    in no unit record); the only authorings are the 23 PROMOTION deltas, which are the delta plane working as
    intended. ⛔ So carrying capacity has ONE home — **`cargo`** ([modifier.md §6](modifier.md)) — and the SM
    figure is read off it, never a second authored number to keep in step.
    The unit's quality/group/size **RANK is DERIVED at load, never stored**: `Σ` over the unit's
    combat classes (primary `combatClass` + the `combatClasses` subs) of each `*Base` where `> −10` — reproducing the
    engine post-load pass (`CvUnitInfo`: `m_iBaseGroupRank += getGroupBase()`). The group rank feeds `getUnitCountSM`
    (`count ⁄ 3^(groupRank−1)`), so a stubbed `0` divides by zero — the getter MUST return the real derived value.
  - **Promotion** (the SM **deltas** a promotion applies): `quality`/`group`/`sizeModifier`/`maxHP` +
    `combatModifier`/`cargo` — same keys, applied as changes when the promotion is gained.

  Effective runtime rank = the derived info base + `Σ` held-promotion changes + the engine merge/split accumulators
  (`getExtraQuality`/`Group`/`Size` — live engine state, **never** data). Block absent ⇒ the entity carries no SM
  data. *(This is the pattern for every game-option-specific system — each gets its own block; `hideAndSeek`
  below is its sibling.)*
- **`hideAndSeek`** — the concealment-vs-detection CONTEST (gated by `GAMEOPTION_COMBAT_HIDE_AND_SEEK`), the
  own-block sibling of `sizeMatters`. **Two members, one per side of the equation:** `concealment` (how well
  this unit hides) and `detection` (how well it finds a hidden one, per method it answers, each entry qualified
  `{unit: HAS_<SKILL>}`). Both are graduated magnitudes and both may be NEGATIVE — a negative detection deposit
  is counter-detection, a negative concealment strips cover (the `WANTED` line does exactly that).
  ⚑ **The METHOD is not in this block at all — it is a [skill](skills.md)** (`camouflage`, `cloaked`,
  `disguised`, …), because a promotion can grant one and optical camouflage is precisely that
  ([vision.md §4](vision.md)). A [tag](tags.md) could not hold it: tags are not promotion-grantable, and 73
  promotions author a method.
  ⛔ **It is NOT part of `vision`, and the separation is load-bearing (owner).** `vision` answers *how far do you
  see*; this answers *do I perceive what is standing there*. The legacy engine's two evaluations bled into each
  other for years, so expressing them as one family is what lets that bleed re-form. The contest READS the
  [vision](vision.md) budget for reach and never the reverse.

  > **⚖ TRAINING A UNIT ABOVE ITS BASE RANK IS AN OFFSET, `base + x` — NEVER AN ABSOLUTE RANK (owner).** A base
  > group rank is DERIVED per unit from its combat classes, so an absolute number means a different thing for
  > every unit — and a DOWNGRADE for one whose base already exceeds it — while an offset stays correct when
  > re-tagging a combat class moves that base. ⚑ The engine already agrees: its merge ceiling
  > (`CvUnit::eraGroupMergeLimit`) was written in exactly this form long before the build side wanted it, which
  > is why an absolute rank would have contradicted the live cap rather than merely read oddly.
  > ⚑ **The ERA bounds `x`** — it decides how many merges are reachable — so the offer is per-ERA while the base
  > is per-UNIT. Two sources, one number; collapsing them into an absolute rank loses both.
  > ⚖ **WHY IT EXISTS: the merge GRIND, not the cost (owner)** — merging hundreds of units by hand in the late
  > game is the problem, and building at `base + x` is the shortcut past it. ⇒ **The cost is therefore the
  > EQUIVALENCE, not a free choice:** it must come out the same as building `3^x` units and merging them, or the
  > shortcut is a trap or an exploit. That falls straight out of the rank geometry above (`count / 3^(rank−1)`),
  > so it is derived rather than balanced.
  > ⛔ It is a QUANTITY term, NOT a training-PACE percent: the Size-Matters pace discount still applies on top,
  > because a directly-built ranked unit IS the merged result and that discount exists precisely because units
  > merge. ⚠ The ceiling, the offer range and the equivalent cost must be ONE implementation shared with the
  > merge gate, or the price and the reachable rank can disagree
  > ([DEC-single-implementation](../architecture/decisions.md#dec-single-implementation)) — note the ceiling
  > lives on `CvUnit` today, which a build menu cannot ask, having no unit yet.
  >
  > **⚖ A PRE-MERGED BUILT UNIT IS INDISTINGUISHABLE FROM A NORMALLY MERGED ONE (owner)** — that is the
  > acceptance bar for the build side, and it decides the implementation rather than merely describing the goal.
  > ⛔ **RANK IS NOT A STORED FIELD — it is carried by PROMOTIONS**, so there is no "set the rank" to write.
  > `CvUnit::mergeUnits` raises it by applying its offset through `normalizeUnitPromotions` over the
  > group-upgrade/downgrade promotions, and a built rank-up reaches the same state ONLY by going through that
  > same application. A bespoke loop beside it is a second implementation of the rank that will drift.
  > ⚠ **Only the GROUP rank rises on a merge** (its offset starts at 1; the QUALITY offset starts at 0 and
  > merely carries the sources' own quality promotions forward, and size is not touched). So `base + x` moves
  > the group axis — do not "complete" it by moving quality and size as well.
  > ⚑ The rest of the merged object needs no faking: a merge averages its three sources' XP and keeps the
  > promotions all three shared, and three FRESH units average to fresh XP and share only their free promotions
  > — so a newly-built ranked unit already equals "three fresh units merged" without inventing any history.
- **bespoke** object-sections, each read by its own system: `promotionLine` · `buildUp` · `shrine` · `headquarters` ·
  `spread` · `properties` · `voteSource` · `threshold` · `role` · `victory` · `targetLevel` · `conversion` ·
  `cityFounding` · `unitCapability` · `sizeMatters` · `hideAndSeek`.

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
marble; +10 culture, doubling after it has stood 1000 years.*

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
`replaces` · `disables` · `requires` · `allowed` · `grants` · `triggers` · `skills` · `tags` · `state` · `attributes` ·
`amenities` · `characteristics` · `capabilities` · `shrine` · `headquarters` · *(modifier families)* · *(auxiliary/bespoke, §9)*

**Scope (singular)** — `world › team › empire › city › plot{improvement|feature|terrain|route} › building|specialist|unit` · off-spine `self` = the entity's own build
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
