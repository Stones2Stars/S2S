# The JSON data model — the authored entity shape & vocabulary

> **Status:** reference   ·   **Verified against:** old `docs/dev/plans/data-model-spec.md` (the **v3-LOCKED** consolidated spec, 2026-06-16) reconciled with the source sub-specs (`modifier-cascade-spec` §1–4, `enabler-cascade-spec` §3/§5/§6, `tally-cascade-spec`); re-confirm against those specs and the live `readJson` parser before relying on a detail.
> **Grounding:** the spec citations carry `spec §N` references; the few engine facts carry `file:line` to the **live source** (`CvCity.cpp`, `CvGameCoreUtils.cpp`, `CvBuildingInfo.cpp`, `CvPlayer.cpp`). Line numbers **drift** — confirm the named function, not the integer.
> This is the **authoring surface**: the JSON vocabulary a modder writes — the reserved sections, the modifier families, the shared atom/condition/count vocabulary, the scopes, and the human-readable value convention. It is the data *shape*; the machines that **consume** it (enabler / modifier / tally) are [`../../explanation/cascade-architecture.md`](../../explanation/cascade-architecture.md), not re-explained here.

**BLUF.** Every game entity (a building, unit, tech, civic, …) is **one JSON object** whose top-level keys are a fixed set of **reserved sections** (`enables`/`obsoletes`/`replaces`/`disables`/`requires`/`allowed`/`grants`/`identity`/…) plus flat **modifier families** (`food`, `production`, `happiness`, one per `PROPERTY_*`, …). One shared **vocabulary** of atoms, conditions, scopes, and counts composes `requires`, the `enabled`/`disabled` conditions, the `per` count-scalers, and `grants` — they are not separate shapes. Values are **human-readable** (`7`, `25`, `1.5`); the ×100 conversion happens once at load, never in the JSON. The **cold-modder rule** is the bar: the JSON must read correctly to a modder with zero codebase knowledge.

> **Scope note.** This documents the *data shape*. It deliberately omits the demolition/migration-status material (which XML field maps to which key, what is built vs pending, the per-field de-scale list) — that is roadmap and curator-internals, and lives in the one-time [`json-migration/`](../../json-migration/README.md) folder (esp. the [rename registry](../../json-migration/migration-renames.md)) and in the scale registry, not in this reference.

---

## 1. An entity is a JSON object — reserved SECTIONS + modifier FAMILIES

Top-level keys are a fixed set of **reserved sections** plus flat **modifier families**. The classification is **deterministic and harness-enforced** — a non-reserved key is a **modifier family iff its value is an OBJECT** (then it is scope-keyed, §4); a non-reserved key with a **bare value** (bool/string/number) is a capability/skill flag or a text field, never a family. A family named like a reserved word is a build-time error (modifier-spec §1.1).

| group | sections | what they are |
|---|---|---|
| **Availability** (the *enabler*) | `enables` · `obsoletes` · `replaces` · `disables` · `requires` · `allowed` | what this unlocks / removes (source side); what it needs to be built & to keep operating (target side, `requires`); the **cap** on how many may exist (`allowed`, §3.4) |
| **Provisions** | `grants` | one-shot / recurring things this hands out (units, buildings, pulses) — §3.5 |
| **Effects** (the *modifier*) | every **family** key (`food`, `production`, `happiness`, `maintenance`, `strength`, one per `PROPERTY_*`, …) | per-turn magnitudes this deposits onto targets — §4 |
| **Intrinsic** ("what am I") | `identity` (incl. **TEXT**: `description`/`help`/`civilopedia`/`message`/`quote`/`strategy`/`adjective`/`shortDescription`) · `cost` · `ui` · `world` · `sound` · `ai` | empire-agnostic self-description, art, AI metadata. TEXT lives under `identity` — text *is* "what am I" |
| **Capabilities** (boolean abilities; scope is carried by the SECTION NAME) | `capabilities` (**TEAM**, tech-unlocked: found-on-peaks, pass-peaks, move-on-water, tech-trading, irrigation, …) · `skills` (**UNIT**, innate: blitz, walk-on-mountains, fly-over-water, amphibious, …) | a civ has *capabilities* (team-wide unlock), a unit has *skills* (innate); the name carries the scope so the parser never infers it |
| **Auxiliary structural** (non-cascade) | `loadPrune` · `policies` · `succession` · `excludes` · `produces` · `condition` · `effect` · `vision` · `outcomes` · `mapGeneration` · `replacedBy` · bespoke: `promotionLine` · `buildUp` · `shrine` · `properties` · `voteSource` · `threshold` · `role` · `victory` · `targetLevel` · `conversion` · `cityFounding` · `unitCapability` | gate-but-don't-cascade data read by their **own** systems (load prune, the LOS resolver, the outcome system, …). The bespoke entries are object-valued but NOT scope-keyed families |

The three machines read **only the cascade sections** (`enables`-family, `requires`, the modifier families, the count-bearing clauses, `grants`); the auxiliary + intrinsic sections feed their own systems. `readJson` parses *all* of them. The `capabilities`-vs-`skills` split (team-unlock vs innate) is a deliberate two-system separation — the name carries the scope.

---

## 2. The shared vocabulary — ONE vocabulary, reused everywhere

The whole point: the same atoms compose `requires`, the `enabled`/`disabled` conditions, the `per` count-scalers, and `grants` — not separate shapes (modifier-spec §1.4).

### 2.1 Types & tokens

- **Data Types** — `BONUS_COAL`, `UNIT_AXEMAN`, `TECH_POTTERY`, `BUILDING_FORGE`, … (the `PREFIX_NAME` ids). Resolved to engine indices via the shared type registry (`getInfoTypeForString`).
- **Catch-all tokens** — engine concepts that aren't data Types: `TURN`, `POPULATION`, `MILITARY`, `CITY`, `TEAM`, `UNIT_LEVEL`, `AREA_SIZE`, … A code-side, engine-resolved, extensible registry — uniform vocabulary, not info special-cases.
- **`PROPERTY_X` BAND atom** — `{type:PROPERTY_CRIME, scope:city, min, max}`: a count atom whose "count" is the context city's current value for that property (`CvProperties::getValueByProperty`), gated by the `min`/`max` band. Unlike a presence atom, an **absent `min` means no lower bound** (a max-only band), not `≥1`. Its home is `requires.operate` — a property-effect building goes dormant/active as the value enters/leaves the band.
- **`SELF`** — the owning entity's own type, resolved per-entity at evaluation. Its live use is the `per` count-scaler (`per:{type:SELF,scope:world}` = count of own type at scope, §2.6). **SELF no longer appears in `requires`** (the old `noneOf:[{type:SELF,…}]` global-uniqueness idiom is withdrawn in favour of the declarative `allowed` cap, §3.4).

### 2.2 Scopes — the containment spine

`world → team → empire → area → city → plot{improvement|feature|terrain|route} → building | specialist | unit`.

- `empire` = the player (all cities). `unit` is a self-accumulator (a unit-scope deposit sums onto the unit itself).
- A `scope` value names *where* an atom counts / a deposit lands. The directional flow of each machine over this spine (counts roll UP, magnitudes deposit DOWN, the `require` callback resolves UP) is the machines' business — see [`cascade-architecture.md` §1](../../explanation/cascade-architecture.md).

### 2.3 Value units (a magnitude names what the value IS, not how it combines)

- `flat` — an additive amount · `percent` — an additive percent delta (`+50%` = `50`) · `multiplier` — a true ×factor, identity `100` (`×2` = `200`). (+ rare `postMultiplier`/`rawPercent`, an engine detail.)
- The combine math (sum / product / cost-asymmetric) is **family metadata the engine owns**, never the per-value unit. Values are authored **human-readable**; the human→×100 conversion and the per-field scale rules are the **scale registry's** concern, not restated here — see [`fixed-point-and-scales.md`](fixed-point-and-scales.md) and [DEC-fixedpoint-x100](../../architecture/decisions.md#dec-fixedpoint-x100). A ×100 value appearing in a JSON file is a curator bug (registry §1).

### 2.4 Conditions — the `requires`/`enabled`/`disabled` tree (ONE serialization, reused)

A bounded boolean tree: **`{ all:[…], any:[[…]], noneOf:[…] }`** (AND of clauses; `any` = a list of OR-groups; `noneOf` = none-present). Leaves are one of:

- **a count / presence ATOM** — `{ type, scope, min?, max?, connection? }`. Presence is the `min:1` case. `connection` ∈ `trade` | `vicinity` | `"trade|vicinity"` (for resource atoms). Atoms are **full + explicit + self-describing** — they always carry their own `type`+`scope`; the parser never infers context (enabler-spec §6.1).
- **a PREDICATE** — a system's runtime-state query (§2.5).

### 2.5 Predicates — each a system's isolated query-surface

- **bare** (parameter-free, desugars to `{PRED:true}`): `IS_WATER` · `IS_FRESHWATER` · `IS_FLATLANDS` · `IS_HILLS` · `IS_PEAK` · `HAS_RIVER` · `HAS_IRRIGATION` · `COASTAL_LAND` · `IS_COASTAL` · `IS_CAPITAL` · `HAS_POWER` · `HAS_STATE_RELIGION` · `STATE_RELIGION_IN_CITY`.
- **parameterized** `{PRED: param}`: `{HAS_FEATURE:FEATURE_X}` · `{HAS_TERRAIN:TERRAIN_X}` · `{HAS_IMPROVEMENT:IMPROVEMENT_X}` · `{HAS_BONUS:BONUS_X}` · `{HAS_MAP_CATEGORY:MAPCATEGORY_X}` · `{HAS_RELIGION:RELIGION_X}` · `{STATE_RELIGION:RELIGION_X}` · `{HOLY_CITY:RELIGION_X}` · `{HAS_CORPORATION:CORP_X}` · `{latitude:{min,max}}` · `{natureYield:{…}}` · `{workedBy:SELF}`.
- **membership SUGAR** `{terrain|feature|bonus:[Type,…]}` — the compact "the plot's terrain/feature/bonus is ONE OF these" form. **Desugars to `any`-of the canonical single-valued predicate** (`{terrain:[A,B]}` ≡ `any:[{HAS_TERRAIN:A},{HAS_TERRAIN:B}]`). `HAS_TERRAIN`/`HAS_FEATURE`/`HAS_BONUS` is canonical; the list is bounded sugar. Bare `HAS_FEATURE` ("has ANY feature") coexists, complementary to the list ("has one of THESE").
- **SCOPE of the plot-substrate predicates:** `HAS_TERRAIN`/`HAS_FEATURE`/`HAS_IMPROVEMENT` are **VICINITY** (the city's current workable radius, enabler-spec §8). `HAS_MAP_CATEGORY` is **CENTER-plot** (the city's own plot, legacy `isMapCategory`) and treats an **uncategorized plot as valid**.
- **negation** = the `disabled` twin / `noneOf` container — never a `false` value. A **missing/unknown predicate is IGNORED, not false** (a retired system's references go quiet, never spuriously disable — enabler-spec §3).
- **`IS_CAPITAL` is computed from BUILDING presence:** the engine answers it as "the city has a Palace or government-center building" (from an `identity.{capital,governmentCenter}` building, which also hard-moves the capital to wherever the Palace is). Palace-type buildings gate on its negation — `requires.build.disabled: "IS_CAPITAL"` (§3.3).

> **UNVERIFIED / forward (carried from the spec, not yet a locked decision):**
> - **Real IMPROVEMENT GROUPS** (modelled like promotion lines / the `Category` system) so `requires` can predicate on a group (`{HAS_IMPROVEMENT_GROUP:G}` ≈ "any farm") and the same group can be a modifier TARGET (the building→improvement-group plot buff). With per-member **ranks**, the buff is computed from rank (auto-including new members). Generalizes to any ranked type and could streamline obsoletion/succession edges. *Flagged as direction, not built.*
> - A string DSL `&`/`|` for compound HAS sequences was **DEFERRED** (would reintroduce operator-precedence the structured `all`/`any`/`noneOf` replaced); revisit only after the structure settles.

### 2.6 `per` — the count-scaler

`per: { type | anyOf:[TYPE…], each, scope }` — scale a deposit by `count(type) / each` at `scope`. Cross-city scopes (empire/team/world) resolve via the **tally**; `city`/`plot` = the local count. `each` = the quantum ("per 5 population" = `each:5`); state it explicitly.

### 2.7 `interval` — the temporal scaler (for `grants.repeatable`)

`interval: { perTurn: N }` (every N turns); bare `interval: perTurn` ≡ `{perTurn:1}`.

### 2.8 The ONE entry shape — used by every deposit, condition-bearing clause, and grant

```jsonc
{ <payload>, "scope"?, "per"?, "enabled"?, "disabled"?, "ai"? }
```

- **payload** — a modifier magnitude (`flat`/`percent`/`multiplier`: value), OR a grant (`type`+`count`), OR a predicate.
- **`scope`** default = the containing scope · **`per`** default ×1 · **`enabled`** default true · **`disabled`** default false · **`ai`** an optional AI-only sibling deposit (same inner shape; applies on top, AI players only).
- A leaf is a single entry **OR a cumulative LIST of entries** (multiple conditioned deposits to one slot).

---

## 3. The availability & provision sections

### 3.1 `enables` family — what a SOURCE unlocks/removes (forward)

Authored on the source, per target-kind:

- **`enables`** — CONSTRUCTIVE, permanent: `enables:{buildings:[],units:[],builds:[],techs:[],civics:[],religions:[],corporations:[],projects:[],processes:[],promotions:[],improvements:[],bonuses:[],…}`. **Tech lives here.**
- **`obsoletes`** — passive supersession (new builds barred; existing instances persist).
- **`replaces`** — succession (a successor takes the predecessor's slot; transitive chain).
- **`disables`** — destructive reversible ban (LATENT today — only `Civilization.disables.techs`).

How these compose into the candidate frontier (`CAN GET = union(enables) − (obsoletes ∪ replaces ∪ disables)`, read forward) is the **enabler machine's** job — [`cascade-architecture.md` §2](../../explanation/cascade-architecture.md). The reverse view ("who unlocks me") is a cold-path / pedia index, never the hot path.

### 3.2 `requires` — what a TARGET needs (the reversible means gate)

```jsonc
"requires": {
  "build":   { "all":[ {"type":"BONUS_STONE","scope":"city","connection":"trade|vicinity"} ] },  // one-time: greys if missing
  "operate": { "all":[ {"type":"CIVIC_GUILDS","scope":"empire"} ] }                               // continuous: dorms if lost
}
```

- **`build`** = the one-time construction gate (greying); **`operate`** = the continuous gate (dormancy — lose it and the built thing goes inactive, not demolished). Each is an `{all/any/noneOf}` tree (§2.4). **Units carry `build` only** (leaf actions, no dormancy).
- **`disabled` / `enabled` as a `build`/`operate` clause** — the bare-predicate negation twin (§2.5); cleaner than `noneOf:[…]` for one bare predicate.
- **Tech** appears in `requires.build` only as a per-candidate CONFIRM (multi-parent AND/OR), never as a generation driver (generation is `enables`-driven, §3.1).

### 3.3 Worked case — PALACE-TYPE (government-center) buildings

`BUILDING_PALACE` + the 8 `bGovernmentCenter` pseudo-palaces (`FORBIDDEN_PALACE`, `EL_ESCORIAL`, `VERSAILLES`, …) carry `requires.build.disabled: "IS_CAPITAL"` — you can't player-BUILD one where a government center already exists (`CvCity.cpp:2654`). This is the **player build gate ONLY** — the engine's FORCED palace relocation (capital falls → hard-move, even into a gov-center city) is an UNGATED actor that bypasses `requires` (the placement-gate invariant: the gate is the caller's job; engine outcomes that must always happen bypass it).

### 3.4 `allowed` — the declarative INSTANCE CAP

The ceiling on **how many may exist** — distinct from `requires` ("what I NEED"). `allowed` names the cap with the **real number** (engine permits a build while `count < allowed`). Two shapes, told apart by the **key namespace**:

- **Self-cap** — `allowed: { <scope>: N }` (scope key `world`/`team`/`empire`) = "at most N of ME at scope." For a building the cap scope ALSO derives its wonder category (`world`→worldWonder, `team`→teamWonder, `empire`→nationalWonder — literally `isWorldWonder == getMaxGlobalInstances() != -1`, `CvGameCoreUtils.cpp:340-369`). A unique unit is `allowed:{empire:1}`.
  - **Units have NO `team` cap** (a team-scope cap is meaningless — units belong to PLAYERS, not teams): unit caps are `world` or `empire` only; the migration folds any team cap into `empire`. The world cap reads LIFETIME-CREATED (`CvCascadeTally::countForCap` → `getUnitCreatedCount`); empire reads the live count. Buildings keep all three scopes.
- **Category count-cap** — `allowed: { <wonderCategory>: N }` (`worldWonders`/`teamWonders`/`nationalWonders`; reserved `totalWonders`) = a **per-city** cap on how many of that category a city may hold. Authored on **CultureLevel** (a culture level grants the city its allowance); city scope is implicit.
- **Group cap (SpecialBuilding group)** — a `SpecialBuilding` is a building GROUP with a shared cap (pick ONE of N members). Each member authors `identity.specialBuildingType: SPECIALBUILDING_X`; the GROUP entity holds the cap (`allowed:{empire:N}`). member→group is authored; group→members is the derived reverse index. Coexists with a member's own self-cap.
- **Absent ⇒ uncapped.** The engine owns everything dynamic (ignoring the cap under `NO_WONDER_LIMIT`/`CHALLENGE_ONE_CITY`/…, era-scaling, `+extra` bumps) — none touch the parser. Enforcement reads the **tally** count at scope.

> The full SpecialBuilding-group model (uniform group-gate inheritance: cap + group TechPrereq → `requires.build` + ObsoleteTech → obsoletion + waiver, materialized at curation/load) is an **upgraded pre-hard-switch deliverable** still being resolved — that scope lives in the plans, not this reference.

### 3.5 `grants` — one-shot / recurring provisions (top-down, NOT cascading)

- **lists**: `grants:{buildings,units,techs,civics,specialists,promotions,traits,bonuses,freePromotions,foundBuildings,…}`.
- **numeric pulses**: `grants.<channel>: value` (e.g. `grants.revolution: -100`, `grants.population: 1`, `grants.goldenAge`).
- **`grants.repeatable`**: `[ {<payload>, interval, chance?, enabled?} ]` — recurring (a PropertySpawn criminal; per-turn heals). `chance` reuses the §2.6 `per` scaler.
- **`grants.foundBuildings`**: a founder's settle-time building seed (each `{building, enabled}`).
- **`outcomes:{kill,actions}`** — DEFERRED mission-triggered grants; carried RAW today, reworked at the #430 outcome-system pass.

---

## 4. Modifier families — the effects surface

`<family>.<scope>[.<targetType>.{TARGET}][.<member>].<unit> = value`. A **flat per-family surface, no `modifiers` wrapper** (modifier-spec §1.2):

```jsonc
"happiness":  { "city": { "flat": 2 } },                                  // single-concept family
"production": { "city": { "percent": [ 25, {"value":25,"enabled":{"min":["BONUS_COAL",1]}} ] } },
"food":       { "city": { "improvements": { "IMPROVEMENT_FARM": { "flat": 1 } } } },  // entity-targeted (keyed)
"maintenance":{ "empire": { "distance": { "percent": -10 } } }            // grouped family, member `distance`
```

- **SPLIT families**: `yield`→`food`/`production`/`commerce`; `commerce`→`gold`/`research`/`culture`/`espionage`; `property`→ one family per `PROPERTY_*`. **GROUPED families** keep members (`maintenance`/`defense`/…). Entity-targeted deposits stay **keep-on-source** keyed by `targetType.{TARGET}`.
- The **unit plane** adds its own family set (`strength`/`withdrawal`/`firstStrike`/`bombard`/`collateral`/`air`/`heal`/`movement`/`experience`/`workRate`/`cargo`/`vision`/`capture`/…). Families are **extensible** (one per concept), so this doc gives the *kinds*, not a frozen enumeration; the exhaustive per-entity list lives in the [migration rename registry](../../json-migration/migration-renames.md).
- **Ownership** of a cross-entity (entity-keyed) modifier is by **semantic sense** — it lives on whoever DELIVERS it (the *deliveryguy*), keyed by the target, not inverted onto the target — [DEC-deliveryguy](../../architecture/decisions.md#dec-deliveryguy). How deposits flow, combine, and condition (`enabled`/`disabled`/`per`) is the **modifier machine's** job — [`cascade-architecture.md` §3](../../explanation/cascade-architecture.md).

---

## 5. Buildability flags — `notConstructible` & `autoBuild`

Two orthogonal `identity` flags about a non-queued building (they overlap but neither contains the other):

- **`identity.notConstructible` — the GATE: "is it offered in the player's build queue?"** Set by the curator when legacy `iCost == -1` (the `CvPlayer::canConstruct` `getProductionCost()==-1` gate, `CvPlayer.cpp:6667`; XML read default -1, `CvBuildingInfo.cpp:1764`). The building twin of the unit `identity.spawnOnly`. The cascade excludes a `notConstructible` building from the buildable frontier — it is instantiated by some OTHER system (autobuild, property-effect spawn, GP/event relic, outcome grant, doctrine toggle), never the production queue.
- **`identity.autoBuild` — one PLACEMENT behavior: "auto-place me in every city where my `requires` holds."** Author the condition the normal way (`requires.build.all:[TECH_X]`, a civic, a resource) and flag `autoBuild`; the engine auto-instantiates the building wherever that condition is met (e.g. *research Sanitation → every city auto-gets the Sewer*). A placed autobuild's subsequent lifecycle (stay / dormant / destroyed) is governed by the SAME structures every building uses — `requires.operate` for dormancy, `disables`/`obsoletes`/`replaces` for destruction — independently of `autoBuild`.
- **Relationship:** `autoBuild ⊂ notConstructible` (an autobuild is also not queued), but `notConstructible` is broader. `notConstructible` answers *is it queueable*; `autoBuild` / `grants` (§3.5) / property-spawn answer *then how is it placed* (`autoBuild` = self-driven by the building's own condition; `grants` = a specific source places it).

The legacy buildability/prerequisite machinery these flags map onto (`CvCity::canConstruct`, the `ConstructRequirement` model, the enabler reverse-index) is documented in [`constructibility.md`](constructibility.md) — that is the as-shipped mechanism being shadowed/cut; this section is only the authored data shape.

---

## 6. Worked example — a building, everything composing

```jsonc
{
  "type": "BUILDING_FORGE",
  "identity": { "description": "TXT_KEY_BUILDING_FORGE", "capital": false },
  "enables": { "units": ["UNIT_CROSSBOWMAN"] },
  "requires": { "operate": { "all": [ {"type":"BONUS_IRON","scope":"city","connection":"trade|vicinity"} ] } },
  "production": { "city": { "percent": 25 } },
  "happiness":  { "city": { "flat": 1, "enabled": "HAS_POWER" } },
  "grants": { "repeatable": [ {"unit":"UNIT_PROPERTY_CRIMINAL","interval":"perTurn",
                              "chance":{"per":{"type":"PROPERTY_CRIME","scope":"city"}}} ] },
  "cost": { "production": 120 }
}
```

Reads cold: *a Forge describes itself; unlocks the Crossbowman; needs connected iron to keep operating; +25% production and (while powered) +1 happiness in its city; each turn may spawn a criminal scaled by crime; costs 120 hammers.* No engine knowledge required — that is the bar for every authored entity (the cold-modder rule).

---

## See also

- [`../../explanation/cascade-architecture.md`](../../explanation/cascade-architecture.md) — the three machines (enabler/modifier/tally) + event spine that **consume** this shape. This doc is the authored JSON; that doc is the machinery. The directional flows, `enables`-vs-`requires` coexistence, deposit combine math, and the HAS / CAN GET / HAS THE MEANS TO model are owned there.
- [`fixed-point-and-scales.md`](fixed-point-and-scales.md) — the scale registry: the canonical home for the human↔×100 value convention, the `flat`/`percent`/`multiplier` unit table, and the per-field de-scale rules referenced from §2.3. Don't re-derive a scale; link this.
- [`constructibility.md`](constructibility.md) — the **legacy** `canConstruct`/`canTrain` + `ConstructRequirement` + enabler reverse-index machinery the `requires`/`allowed`/buildability-flag data (§3, §5) maps onto; the mechanism being shadowed and replaced.
- [`../../architecture/decisions.md`](../../architecture/decisions.md) — the rulings ledger: [DEC-fixedpoint-x100](../../architecture/decisions.md#dec-fixedpoint-x100) (value scale), [DEC-deliveryguy](../../architecture/decisions.md#dec-deliveryguy) (modifier ownership), [DEC-cascade-bidirectional](../../architecture/decisions.md#dec-cascade-bidirectional) (`requires` = the AND mechanism, resolved up-chain).
- [`../../README.md`](../../README.md) — the comprehension map / overview-of-overviews; this doc is its "Data model & migration" entry.
