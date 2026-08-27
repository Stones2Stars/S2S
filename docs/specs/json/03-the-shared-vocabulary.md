# 3. The shared vocabulary

> Part of the **[json](../json.md)** spec.

The same atoms compose `requires`, the `enabled`/`disabled` conditions, the `per` count-scalers, `grants`, and
modifier targets — **not separate shapes.** Learn them once.

### 3.1 Types, tokens, `SELF`

- **Data Types** — `INFOTYPE_NAME` ids (`BONUS_COAL`, `UNIT_AXEMAN`, `BUILDING_FORGE`). The leading **infotype**
  prefix identifies the kind and routes the reference; the full prefix glossary (`UNIT_` = unit, `TRAIT_` =
  simple trait, `TRAIT_COMPLEX_` = complex, …) is the **[naming spec](../naming.md)**.
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
  me exist"). It is **not** used in `requires` — a "one of me" cap is [`allowed`](04-availability.md#44-allowed--caps), not a
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
> Engine note: the cascade adds the sort/select step in **parsing**, one place for all ranking metrics.
> Implementation TODO: [plans/parked/ranked-target-selection.md](../../plans/parked/ranked-target-selection.md).

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
{ "type": "BONUS_IRON", "scope": "city", "connection": "trade" }   // an atom
```

An **atom** is `{ type, scope?, min?, max?, connection? }`. **Scope is IMPLIED from the type's domain** (derived from
the ID prefix) — TECH→`team`, civic/heritage→`empire`, building/bonus/religion/corporation→`city`. One data-driven
override: a building carrying `identity.empireLevel` (§7) has EMPIRE as its domain, so an atom naming one implies
`empire` — the player-held set is the only place its presence exists
([enabler.md §2](../enabler.md), [empire-level buildings](../enabler/02-pass-1-generate-the-frontier-the.md#2-pass-1--generate-the-frontier-the-enables-family)).
State `scope` explicitly ONLY when it differs from that default (e.g. a `world`-scope victory, a `player`-scope tech).
So a **plain default-scope presence collapses to a bare type-string** — author the common case as a simple string
array: `"all": ["BUILDING_FORGE", "TECH_ASTRONOMY"]` ≡ `[{type:"BUILDING_FORGE"},{type:"TECH_ASTRONOMY",scope:"team"}]`.
Keep the object form only when a special case forces it: a `connection`, a count (`min`/`max`), or a non-default
scope. **Forcing a redundant `{type, scope}` only invites authoring bugs.** *(Plot-substrate
`{type:"TERRAIN_…"/"FEATURE_…"/"IMPROVEMENT_…"}` and `{type:"MAPCATEGORY_…"}` stay object-form — they are plot predicates, §3.5.)*

- **presence** = `min: 1` ("have ≥ 1"). Authoring presence this way keeps it future-proof if a resource later
  gains amounts.
- **count thresholds** — `min: N` (≥ N) and/or `max: N` (≤ N), both inclusive. Exact-N = `min` and `max` together.
- `connection` (resources only) ∈ `"trade"` | `"onSite"`. **The two are MUTUALLY EXCLUSIVE** — a gate wanting
  either states TWO atoms under an `any`, never one combined selector. `vicinity` is a separate field, not a
  `connection` value. What each means: [bonuses.md](../../reference/bonuses.md).
- **`vicinity`** — a separate field, carried with or without a `connection`: which tiles of
  the city's workable radius count. A radius tile's
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
  - `"onSite"` = the resource is **actually available AT this city** — however it got there. Improving a resource
    on a workable plot puts it here, and so does a building in the city that supplies it (a herd, a factory —
    `provides.bonuses`, §5a): those are the SAME act as far as this list is concerned, and the list cares only
    about what is there, never about provenance.
    > **⛔ IT IS NAMED `onSite` BECAUSE "vicinity" AND "connected" BOTH MISLEAD.** The two sets are
    > ORTHOGONAL: **onSite** = the resource is here; **`connection:"trade"`** = the plot group reaches it. A
    > resource can be either without the other — a mounted unit needs horses ON SITE, a swordsman only needs
    > iron wares in the NETWORK.
    > ⚠ The retired spelling was `"connected"`, which took the trade side's word for a local question and is
    > what made the two read as one thing. `owned` (raw presence, improved or not) stays its own tier and is
    > strictly weaker than `onSite`.
    >
    > **⛔⛔ `onSite` IS AN ENABLER-SIDE CONCEPT ONLY. NO MODIFIER GATES ON IT — NOT ONE.** A DEPOSIT
    > conditioned on a resource asks whether the CITY HAS IT, which is the TRADED question and is spelled as the
    > bare `{type, scope:"city", min:1}` atom. `onSite` belongs to `requires` GATES and is *"almost purely a
    > concept that creating mounted units have to deal with, very little else"* — horses must be physically here;
    > the mine building is the other explicit case. Both are enabler-side.
    > ⚠ **This has been stated repeatedly and re-derived wrongly anyway**, because the tier list above reads as a
    > menu any atom may pick from. It is not: a modifier picks the bare atom, full stop. The measured cost of
    > getting it wrong is silent and total — the curator mapped legacy `VicinityBonusYieldChanges` to
    > `vicinity:"onSite"` on the strength of its XML name (its engine read, `hasVicinityBonus`, actually means
    > *obtained* in vicinity, i.e. connected), and every one of those deposits then refused against a resource the
    > city demonstrably held: London carried 96 resources in trade and 14 on site, so Cannery's apple, crab,
    > lemons and olives were each refused while the city was trading all four.
    > ⚑ The refusal is invisible in every total — a deposit that never applies leaves no trace — which is why this
    > survives review and why the yield tooltip and `/computed/city/yield` now list the refused deposits WITH the
    > atom that refused them.

  ```jsonc
  { "type": "BONUS_SHRIMP",   "scope": "city", "connection": "vicinity", "vicinity": "owned" }      // raw presence on an owned tile
  { "type": "BONUS_GOLD_ORE", "scope": "city", "connection": "vicinity", "vicinity": "onSite" }     // must be available here
  ```

- **`PROPERTY_*` band atom** `{type:PROPERTY_X, scope, min?, max?}` — its "count" is the city's property value;
  **absent `min` = no lower bound** (a max-only band), the one exception to the presence=`min:1` convention.
  Authored in `requires.operate`.

> **Counts vs caps.** `min`/`max` express what you **need** (a count of *some other* type, e.g. "≥12 Barracks").
> "How many of THIS may exist" is **not** a condition — it is the [`allowed`](04-availability.md#44-allowed--caps) cap.

### 3.5 Predicates — a system's runtime-state query

A predicate asks the game state a yes/no question a static file can't hold ("is this the capital? a river?"). It
is **evaluated against the deposit's target** and so carries **no `_PLOT`/`_UNIT` suffix** — the target supplies
context: `IS_WATER` on `plots` = a water tile, on `units` = a sea unit. An **unknown/missing predicate is
IGNORED**, never treated as false — retiring a system never spuriously disables unrelated data.

> **The predicate registry is EXTENSIBLE — and a condition is ALWAYS a predicate, never a bespoke member
> ([conditions are predicates, never bespoke members](#35-predicates--a-systems-runtime-state-query)).** When a deposit's condition has no predicate named verbatim
> below yet, **define a new predicate** (add it here, wire it in the evaluator, and emit the state fact it reads).
> Adding a predicate *extends* the model within the structure. What you must NOT do is encode the condition as a new
> sub-scope **member** (`{family}.empire.capital.percent`, `perMilitaryUnit`) — that changes the core structure (the
> kraken way; see [modifier.md §3](../../cascade.md), which also notes the **golden-age exception**: `empire.goldenAge`
> is a PERMANENT engine member-mirror (effect-only), because the yield effect is engine-core and not data-defined;
> golden-age length + grant ARE curated JSON (`goldenAge.empire.percent`, `grants.goldenAge`)).
>
> **`IS_*` vs `HAS_*` — literal English.** Plain English picks the prefix: `IS_*` = whether the
> target **is** something (a plot `IS_WATER`, a city `IS_CAPITAL`); `HAS_*` = whether it **has** something (a plot
> `HAS_RIVER`, `HAS_PEAK`, `HAS_COAST`). These semantics span **every target group**, not just plots — a *unit*
> `IS_MILITARY` (defined by the `military` [tag](../tags.md)), a *city* `IS_CAPITAL` — so a **tag-backed predicate
> reads its tag** (that is how a [tag](../tags.md) is queried inside a condition). The `HAS_*` set is the
> `<scope>.<target>` filter layer, so `HAS_COAST`
> matches **any** coastal-adjacent plot (water *or* land). The **target is the plot**:
> `HAS_PEAK` = the plot has a peak (a special case — a peak behaves as *both* a feature and a terrain, so a plot
> could *in theory* carry a terrain **and** a peak, e.g. grassland+peak; it just doesn't happen in practice). The
> `HAS_COAST`/`HAS_RIVER`/`HAS_PEAK`/`HAS_HILLS` target-filters follow the §3.5 predicate semantics; `MAPCATEGORY_`
> is an XML-only Type referenced from `requires.build` ([naming.md](../naming.md)). Their space-map extensions are
> defined by the space-map work.

> **⛔ A GENERALIZED PLOT PREDICATE RESOLVES THROUGH A `foldTargets` INFO — WE NEVER FOLD ONTO A BOOLEAN
>.** *"We can never fold onto a boolean predicate, we need a target to fold onto."* A deposit lands on a
> concrete substrate ENTITY (a terrain, an improvement), so a predicate that names a CATEGORY rather than an
> entity — `IS_WATER`, `IS_LAND`, `HAS_HILLS`, `HAS_PEAK`, `IS_FLATLANDS`, and the space/planet domains — has
> nothing to attach to and silently delivers NOTHING. ⚑ **The failure is total and silent, which is why this is a
> hard rule:** every `IS_WATER`-conditioned plot deposit in the shipped data (Lighthouse, Pier, Seawalls,
> Fisherman's Hut, the Seafaring achievement) resolved on ZERO plots, while the river/irrigation deposits beside
> them — whose plots carry an improvement to fold onto — applied normally. Nothing errored and no value looked
> wrong; the yield was simply absent.
> ⇒ **Each generalized plot type is a PREDETERMINED INFO** under `Assets/Data/foldtargets/`, one object per file,
> naming the concrete substrate entities it means (`IS_WATER` → the ocean / sea / coast / trench / lake terrains).
> The evaluator resolves the predicate against that set, so the fold always has a real target.
> ⚑ **The point is MODDER LEGIBILITY, not engine convenience: *"this gives understandable options for the
> modders going forward."*** It is §1's one promise applied to the plot plane — the data reads cold, so what
> `IS_WATER` MEANS is readable in a file instead of being a hidden engine table. ⛔ So it is DATA, never a
> hardcoded id list in C++: a new water terrain joins by being named there, with no engine change.
> ⚠ **A relief predicate needs no carve-out** — `TERRAIN_HILL` and `TERRAIN_PEAK` are real authored terrains, so
> `HAS_HILLS`/`HAS_PEAK` fold exactly like the rest and nothing special-cases them.
> ⚑ **A file exists for a predicate the DATA authors, never speculatively** — the registry is open like its
> siblings (§8), so `IS_LAND` / `IS_FLATLANDS` / `HAS_HILLS` get one the moment a deposit names them.
> ⚖ **THE GRANULAR DIFFERENTIATION IS THE SECOND STEP, DELIBERATELY: *"I want this to just work first,
> then … use the capability to differentiate between similar types, the way the old xml did super granularly."***
> The fold set is what makes that reachable — once a predicate resolves to a NAMED set, distinguishing coast from
> ocean from deep-sea is authoring another set rather than an engine change. ⛔ Do not build the granular split
> ahead of the plain one working; this is an owner-ruled ORDERING, so
> ["deferred" is banned](../../../AGENTS.md#design) does not reach it.

- **bare** (parameter-free string), four groups:
  - **environment / domain** `IS_<where>` (target-relative): `IS_WATER` · `IS_LAND` · `IS_AIR` · `IS_SPACE` · `IS_LUNAR` · `IS_MARS`
    (extensible) — each a `foldTargets` info per the ruling above.
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
  `{HAS_CORPORATION: CORPORATION_X}` ·
  **`{CIVIC_CATEGORY: CIVICOPTION_X}`** (the CIVIC whose value is being resolved sits in that category — the gate a
  trait's *"religion civics cost no upkeep"* authors, as `upkeep.empire.civic.percent: -100` conditioned on it).
  ⛔ It carries the **FULL `CIVICOPTION_` id**, never a bare `RELIGION`, so it can never be read as a `RELIGION_`
  type. ⚑ It is a SOURCE-SLOT predicate ([contexts.md](../../cascade.md) § THE SOURCE SLOTS): the walk
  resolving a civic sets the slot, and with no civic in hand it answers **FALSE** — resolving it against whichever
  civic a walk reached last would be worse than declining. ⚠ The legacy shape was a target-keyed
  `upkeep.civicOptions.{CIVICOPTION_X}` member, which is the condition-as-member rollerskate
  ([conditions are predicates, never bespoke members](#35-predicates--a-systems-runtime-state-query)) AND matched no kind
  row, so it parsed, reported `unkinded-member` and produced nothing ·
  `{latitude:{min,max}}` · `{existedFor:{min:N}}` (GAME YEARS since built -- what the player has always been told: *"doubles in 1000 years"*. The city stores the build YEAR (`getGameTurnYear`) and every authored threshold is a year count; a turn's year is derived, never stored ([engine.md](../../reference/engine.md)), so nothing needs converting) ·
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

> **Values are human-readable. Always.** `7`, `25`, `1.5` — never ×100. readJson performs the one human→×100
> conversion at load ([the ×100 fixed-point model](../curators/fixed-point-and-scales.md#1-the-model--integer-100-for-amounts-human-only-at-the-in-and-out-boundaries)). **A ×100 value in
> a JSON file is a bug.**

### 3.7 `per` — count-scaling

```jsonc
"per": "SPECIALIST"                                              // bare-string sugar: × count, each:1, own scope
"per": { "type": "POPULATION", "each": 5, "scope": "city" }      // value × (count / 5)
"per": { "anyOf": ["BONUS_COW","BONUS_PIG"], "scope": "city" }   // value × (summed count of any listed)
```

A bare string is the common case collapsed (the §3.4 bare-atom sugar, applied here): `"per": "SPECIALIST"` ≡
`{ "type": "SPECIALIST", "each": 1 }` at the deposit's own scope. Keep the object form only when a real `each`
quantum or a non-default scope forces it.

**`above:` — the over-threshold scaler.** `"per": { "type": "CITY", "above": N }` scales by the count
EXCEEDING the threshold — `value × max(0, count − above)` — for the "per city over the limit" formula class.
The threshold is a literal or a **token** (the §3.3 threshold-token rule): `"above": "CITY_LIMIT"` reads the
depositing civic's own resolved limit. Composes with `each` (`(count − above) / each`) and the §3.9 gates.

`each` is the quantum ("per 5 population" → `each: 5`); state it explicitly. `scope` defaults to the deposit's own
scope; cross-city scopes (empire/team/world) resolve via the [tally](../tally.md), `city`/`plot` are local.

**`unit: <predicate>`** qualifies a deposit by a **unit predicate** — the *same* `unit:` qualifier cargo uses
(`cargo.space.{unit: IS_AIR, …}`, [modifier](../../cascade.md) §6). On a count-scaling family it reads **per unit
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
  the formula mechanism:** a composite formula authors as the SUM of its entries — a base term, `per`
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

