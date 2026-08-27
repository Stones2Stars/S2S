# 3.5 Predicates — a system's runtime-state query

> Part of the **[03-the-shared-vocabulary](../03-the-shared-vocabulary.md)** spec.

A predicate asks the game state a yes/no question a static file can't hold ("is this the capital? a river?"). It
is **evaluated against the deposit's target** and so carries **no `_PLOT`/`_UNIT` suffix** — the target supplies
context: `IS_WATER` on `plots` = a water tile, on `units` = a sea unit. An **unknown/missing predicate is
IGNORED**, never treated as false — retiring a system never spuriously disables unrelated data.

> **The predicate registry is EXTENSIBLE — and a condition is ALWAYS a predicate, never a bespoke member
> ([conditions are predicates, never bespoke members](#35-predicates--a-systems-runtime-state-query)).** When a deposit's condition has no predicate named verbatim
> below yet, **define a new predicate** (add it here, wire it in the evaluator, and emit the state fact it reads).
> Adding a predicate *extends* the model within the structure. What you must NOT do is encode the condition as a new
> sub-scope **member** (`{family}.empire.capital.percent`, `perMilitaryUnit`) — that changes the core structure (the
> kraken way; see [modifier.md §3](../../../cascade.md), which also notes the **golden-age exception**: `empire.goldenAge`
> is a PERMANENT engine member-mirror (effect-only), because the yield effect is engine-core and not data-defined;
> golden-age length + grant ARE curated JSON (`goldenAge.empire.percent`, `grants.goldenAge`)).
>
> **`IS_*` vs `HAS_*` — literal English.** Plain English picks the prefix: `IS_*` = whether the
> target **is** something (a plot `IS_WATER`, a city `IS_CAPITAL`); `HAS_*` = whether it **has** something (a plot
> `HAS_RIVER`, `HAS_PEAK`, `HAS_COAST`). These semantics span **every target group**, not just plots — a *unit*
> `IS_MILITARY` (defined by the `military` [tag](../../tags.md)), a *city* `IS_CAPITAL` — so a **tag-backed predicate
> reads its tag** (that is how a [tag](../../tags.md) is queried inside a condition). The `HAS_*` set is the
> `<scope>.<target>` filter layer, so `HAS_COAST`
> matches **any** coastal-adjacent plot (water *or* land). The **target is the plot**:
> `HAS_PEAK` = the plot has a peak (a special case — a peak behaves as *both* a feature and a terrain, so a plot
> could *in theory* carry a terrain **and** a peak, e.g. grassland+peak; it just doesn't happen in practice). The
> `HAS_COAST`/`HAS_RIVER`/`HAS_PEAK`/`HAS_HILLS` target-filters follow the §3.5 predicate semantics; `MAPCATEGORY_`
> is an XML-only Type referenced from `requires.build` ([naming.md](../../naming.md)). Their space-map extensions are
> defined by the space-map work.

> **⛔ A GENERALIZED PLOT PREDICATE RESOLVES THROUGH A `foldTargets` INFO — WE NEVER FOLD ONTO A BOOLEAN
>.** A fold can never land on a boolean predicate; it needs a target to fold onto. A deposit lands on a
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
> ⚖ **THE GRANULAR DIFFERENTIATION IS THE SECOND STEP, DELIBERATELY: it works first, and the capability is only then used to differentiate similar types, the**
> The fold set is what makes that reachable — once a predicate resolves to a NAMED set, distinguishing coast from
> ocean from deep-sea is authoring another set rather than an engine change. ⛔ Do not build the granular split
> ahead of the plain one working; this is an owner-ruled ORDERING, so
> ["deferred" is banned](../../../../AGENTS.md#design) does not reach it.

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
  type. ⚑ It is a SOURCE-SLOT predicate ([contexts.md](../../../cascade.md) § THE SOURCE SLOTS): the walk
  resolving a civic sets the slot, and with no civic in hand it answers **FALSE** — resolving it against whichever
  civic a walk reached last would be worse than declining. ⚠ The legacy shape was a target-keyed
  `upkeep.civicOptions.{CIVICOPTION_X}` member, which is the condition-as-member rollerskate
  ([conditions are predicates, never bespoke members](#35-predicates--a-systems-runtime-state-query)) AND matched no kind
  row, so it parsed, reported `unkinded-member` and produced nothing ·
  `{latitude:{min,max}}` · `{existedFor:{min:N}}` (GAME YEARS since built -- what the player has always been told: *"doubles in 1000 years"*. The city stores the build YEAR (`getGameTurnYear`) and every authored threshold is a year count; a turn's year is derived, never stored ([engine.md](../../../reference/engine.md)), so nothing needs converting) ·
  `{HAS_COAST:{minArea:N}}` (the city is adjacent to a water body of **≥ N tiles**; a bare `HAS_COAST` is coastal at
  the default threshold, so an entity needing a *larger* sea body carries the size here).
- **membership sugar** `{ terrain|feature|bonus: [TYPE,…] }` = "the plot's terrain/feature/bonus is one of these";
  equivalent to an `any` of the matching `HAS_*` predicate.
- **composition is the win:** a Martian peak is `{all:["IS_MARS","HAS_PEAK"]}`; coastal land
  `{all:["IS_LAND","HAS_COAST"]}`; flat land = `{all:["IS_LAND","IS_FLATLANDS"]}` (domain + relief). No bespoke
  "mars-peak"/"coastal-land" type.
- **negation** uses the `disabled` twin (§3.9) or `noneOf` — never a `false` value.
- (a `PROPERTY_*` band atom is the one exception to presence=`min:1` — absent `min` = no lower bound; §3.4.)

