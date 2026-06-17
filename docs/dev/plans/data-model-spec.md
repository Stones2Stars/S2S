# Data model — the canonical object & vocabulary reference (consolidated 2026-06-16)

**Status: CONSOLIDATION (owner 2026-06-16), the prototype starting point.** The #428 data model is specified piecemeal
across `modifier-cascade-spec.md` (§1–4), `enabler-cascade-spec.md` (§3/§5/§6), `tally-cascade-spec.md`, and
`building-cascade-conversion.md` (§2–3); the per-entity field→shape mapping is in `migration-renames.md`. This doc gathers
the *shape* into ONE reference. **Zero new invention** — anything that would extend the design is flagged ⚑.

**TWO audiences, one document:**
1. **The #430 PROTOTYPE** — this is the shared interface: what `readJson` parses the JSON into, and what the three machines
   (tally / modifier / enabler) read. The four components prototype against §1–4 (shape) + §6 (runtime + read-interface).
2. **MODDER DOCS foundation** — the authoritative "how to author the data" reference. It must honor the **cold-modder rule**
   (the JSON reads to a modder with ZERO codebase knowledge): self-explanatory sections, the full vocabulary, worked
   examples. §1–5 are the modder-facing half.

---

## 1. An entity is a JSON object — reserved SECTIONS + modifier FAMILIES

Every entity (a building, unit, tech, civic, …) is a JSON object whose top-level keys are a fixed set of **reserved
sections** plus flat **modifier families**. "Section vs family" is deterministic — a family named like a reserved word is a
build-time error (modifier-spec §1.1).

| group | sections | what they are |
|---|---|---|
| **Availability** (the *enabler*) | `enables` · `obsoletes` · `replaces` · `disables` · `requires` · `allowed` | what this unlocks / removes (source side), what it needs to be built & to keep operating (target side), and the **cap** on how many may exist (`allowed`, §4.2a) |
| **Provisions** | `grants` | one-shot / recurring things this hands out (units, buildings, pulses) |
| **Effects** (the *modifier*) | every **family** key (`food`, `production`, `happiness`, `maintenance`, `strength`, one per `PROPERTY_*`, …) | per-turn magnitudes this deposits onto targets |
| **Intrinsic** ("what am I") | `identity` (incl. **TEXT**: `description`/`help`/`civilopedia`/`message`/`quote`/`strategy`/`adjective`/`shortDescription`) · `cost` · `ui` · `world` · `sound` · `ai` | empire-agnostic self-description, art, AI metadata. **TEXT fields live under `identity`** (decision A 2026-06-16) — text *is* "what am I". |
| **Capabilities** (boolean abilities — scope by SECTION NAME, never checked) | `capabilities` (**TEAM**, tech-unlocked: found-on-peaks, pass-peaks, move-on-water, tech-trading, irrigation, bridge-building, river-trade) · `skills` (**UNIT**, innate: blitz, walk-on-mountains, fly-over-water/helicopter, amphibious, can-move-impassable, …) | decision B 2026-06-16: a civ has *capabilities*, a unit has *skills* — DIFFERENT mechanics (team-wide unlock vs innate), the name carries the scope so the parser never infers it. |
| **Auxiliary structural** (non-cascade) | `loadPrune` · `policies` · `succession` · `excludes` · `produces` · `condition` · `effect` · `vision` · `outcomes` · `mapGeneration` · `replacedBy` · **bespoke**: `promotionLine` · `buildUp` · `shrine` · `properties` · `voteSource` · `threshold` · `role` · `victory` · `targetLevel` · `conversion` · `cityFounding` · `unitCapability` | gate-but-don't-cascade data read by their own systems (load prune, the LOS resolver, the outcome system, …). The bespoke entries (decision C) are object-valued but NOT scope-keyed families. |

The **three machines read only the cascade sections** (`enables`-family, `requires`, the modifier families, the count-bearing
clauses, `grants`); the auxiliary + intrinsic sections feed their own systems. `readJson` parses *all* of them.

**The unambiguous classification rule (harness-enforced):** a non-reserved top-level key is a **modifier family iff its value
is an OBJECT** (then it is scope-keyed, §4.3); a non-reserved key with a **bare value** (bool/string/number) is a capability/
skill flag or a text field, never a family. So "family vs flag" is decided structurally by the value, and team-vs-unit
capability scope is decided by the section name (`capabilities`/`skills`) — neither requires the parser to infer scope.

---

## 2. The shared vocabulary — ONE vocabulary, reused everywhere

The whole point of the migration: the same atoms compose `requires`, `enabled`/`disabled` conditions, `per` count-scalers,
and `grants` — they are not separate shapes (modifier-spec §1.4).

### 2.1 Types
- **Data Types** — `BONUS_COAL`, `UNIT_AXEMAN`, `TECH_POTTERY`, `BUILDING_FORGE`, … (the `PREFIX_NAME` ids). Resolved to
  engine indices via the shared type registry (`getInfoTypeForString`).
- **Catch-all tokens** — engine concepts that aren't data Types: `TURN`, `POPULATION`, `MILITARY`, `CITY`, `TEAM`,
  `UNIT_LEVEL`, `AREA_SIZE`, … A code-side registry (extensible, engine-resolved) — uniform vocabulary, not info special-cases.
- **`SELF`** — the owning entity's OWN type, resolved per-entity at evaluation (e.g. `requires.build.noneOf:[{type:SELF,
  scope:world}]` = global-uniqueness, the world-wonder / religion-founding-once idiom).

### 2.2 Scopes — the containment spine
`world → team → empire → area → city → plot{improvement|feature|terrain|route} → building | specialist | unit`.
`empire` = the player (all cities). `unit` is a self-accumulator (a unit-scope deposit sums onto the unit itself).

### 2.3 Units (a modifier magnitude names what the value IS, not how it combines)
- `flat` — an additive amount. · `percent` — an additive percent delta (`+50%` = `50`). · `multiplier` — a true ×factor,
  identity `100` (`×2` = `200`). (+ rare `postMultiplier`/`rawPercent`, engine detail.) The combine math (sum / product /
  cost-asymmetric) is **family metadata** the engine owns (modifier-spec §7), never the per-value unit.

### 2.4 Conditions — the `requires` object (the ONE serialization, reused as `requires` / `enabled` / `disabled`)
A bounded boolean tree: **`{ all:[…], any:[[…]], noneOf:[…] }`** (AND of clauses; `any` = a list of OR-groups; `noneOf` =
none-present). Leaves are one of:
- **a count / presence ATOM** — `{ type, scope, min?, max?, connection? }`. Presence is the `min:1` case (volumetric-ready).
  `connection` ∈ `trade` | `vicinity` | `"trade|vicinity"` (for resource atoms). Atoms are **full + explicit + self-describing**
  (always carry their own `type`+`scope`; the parser never infers context — enabler-spec §6.1).
- **a PREDICATE** — a system's runtime-state query (§2.5).

### 2.5 Predicates — each a system's isolated query-surface
- **bare** (parameter-free, desugars to `{PRED:true}`): `IS_WATER` · `IS_FRESHWATER` · `IS_FLATLANDS` · `IS_HILLS` ·
  `IS_PEAK` · `HAS_RIVER` · `HAS_IRRIGATION` · `COASTAL_LAND` · `IS_COASTAL` · `IS_CAPITAL` · `HAS_POWER` ·
  `HAS_STATE_RELIGION` · `STATE_RELIGION_IN_CITY`.
- **parameterized** `{PRED: param}`: `{HAS_FEATURE:FEATURE_X}` · `{HAS_TERRAIN:TERRAIN_X}` · `{HAS_BONUS:BONUS_X}` ·
  `{HAS_RELIGION:RELIGION_X}` · `{STATE_RELIGION:RELIGION_X}` · `{HOLY_CITY:RELIGION_X}` · `{HAS_CORPORATION:CORP_X}` ·
  `{latitude:{min,max}}` · `{natureYield:{…}}` · `{workedBy:SELF}`.
- **membership SUGAR** `{terrain|feature|bonus:[Type,…]}` — the compact authoring form for "the plot's terrain/feature/
  bonus is ONE OF these" (improvement placement make-valid sets, e.g. valid on 17 terrains). **Desugars to `any`-of-the
  canonical single-valued predicate** (`{terrain:[A,B]}` ≡ `any:[{HAS_TERRAIN:A},{HAS_TERRAIN:B}]`). The `HAS_TERRAIN`/
  `HAS_FEATURE`/`HAS_BONUS` predicate is canonical (one engine predicate per system, uniform w/ `HAS_RELIGION`/
  `HAS_CORPORATION`); the list is the bounded sugar (owner 2026-06-16, hole #1 RESOLVED). Bare `HAS_FEATURE` ("has ANY
  feature") coexists, complementary to the list ("has one of THESE").
- **negation** = the `disabled` twin / `noneOf` container (never a `false` value). A **missing/unknown predicate is IGNORED,
  not false** (a retired system's references go quiet, never spuriously disable — enabler-spec §3).
  - *(Phase-F membership reconcile RESOLVED 2026-06-16: `HAS_TERRAIN`/`HAS_FEATURE`/`HAS_BONUS` canonical + `{…:[…]}` sugar,
    above. `COASTAL_LAND` is UNUSED in real data (0 occurrences) → moot; `IS_COASTAL` (city-coastal, `CvCity::isCoastal`)
    stays, distinct from a plot predicate.)*
- **⚑ DEFERRED (maybe), owner 2026-06-16:** a string DSL `&`/`|` for compound HAS sequences was considered and put OFF
  until the structure is fully nailed — it would reintroduce operator-precedence/parens (the BoolExpr expression-tree the
  structured `all`/`any`/`noneOf` replaced) and a second condition surface. Revisit only after the structure settles.
- **`IS_CAPITAL` is computed from BUILDING presence (owner 2026-06-16).** The engine answers `IS_CAPITAL` as "the city has a
  Palace or palace-adjacent (government-center) building" — i.e. from the presence of an `identity.{capital,governmentCenter}`
  building (and it hard-moves the capital to wherever the Palace is). The palace-type buildings then gate on its negation —
  `requires.build.disabled: "IS_CAPITAL"` (§4.2). One concept, two ends: the flag says "I make this a capital," the
  requirement says "but not where one already exists."

### 2.6 `per` — the count-scaler (modifier-spec §4)
`per: { type | anyOf:[TYPE…], each, scope }` — scale a deposit by `count(type) / each` at `scope`. Cross-city scopes
(empire/team/world) resolve via the **tally**; `city`/`plot` = the local count. `each` = the quantum ("per 5 population" =
`each:5`); state it explicitly.

### 2.7 `interval` — the temporal scaler (modifier-spec §4.1, for `grants.repeatable`)
`interval: { perTurn: N }` (every N turns); bare `interval: perTurn` ≡ `{perTurn:1}`.

---

## 3. The ONE entry shape — used by every deposit, condition-bearing clause, and grant

```jsonc
{ <payload>, "scope"?, "per"?, "enabled"?, "disabled"?, "ai"? }
```
- **payload** — a modifier magnitude (`flat`/`percent`/`multiplier`: value), OR a grant (`type`+`count`), OR a predicate.
- **`scope`** default = the containing scope · **`per`** default ×1 · **`enabled`** default true · **`disabled`** default
  false · **`ai`** an optional AI-only sibling deposit (same inner shape; applies on top, AI players only).
- **A leaf is a single entry OR a cumulative LIST of entries** (multiple conditioned deposits to one slot).

---

## 4. The sections in detail (with modder examples)

### 4.1 `enables` family — what a SOURCE unlocks/removes (forward; drives generation)
Authored on the source, per target-kind:
- **`enables`** — CONSTRUCTIVE (permanent): `enables:{buildings:[],units:[],builds:[],techs:[],civics:[],religions:[],
  corporations:[],projects:[],processes:[],promotions:[],improvements:[],bonuses:[],…}`. **Tech lives here.**
- **`obsoletes`** — passive supersession (new builds barred; existing instances persist).
- **`replaces`** — succession (a successor takes the predecessor's slot; transitive chain).
- **`disables`** — destructive reversible ban (LATENT today — only `Civilization.disables.techs`).
- The candidate frontier **CAN GET = union(`enables` over HAS) − (`obsoletes` ∪ `replaces` ∪ `disables` over HAS)**, read
  FORWARD. The reverse view ("who unlocks me") is a cold-path/pedia index, never the hot path.

### 4.2 `requires` — what a TARGET needs (the reversible means gate)
```jsonc
"requires": {
  "build":   { "all":[ {"type":"BONUS_STONE","scope":"city","connection":"trade|vicinity"} ] },  // one-time: greys if missing
  "operate": { "all":[ {"type":"CIVIC_GUILDS","scope":"empire"} ] }                               // continuous: dorms if lost
}
```
- **`build`** = the one-time construction gate (greying); **`operate`** = the continuous gate (dormancy — lose it and the
  built thing goes inactive, not demolished). Each is an `{all/any/noneOf}` tree (§2.4). Units carry `build` only (leaf
  actions, no dormancy). ⚑ continuous resource/power gates currently sit in `build` but should be `operate` (Phase F).
- **`disabled` / `enabled` as a requires CLAUSE — the bare-predicate negation twin (§2.5).** Besides `all`/`any`/`noneOf`,
  a `build`/`operate` gate may carry a `disabled` (or `enabled`) clause for a single predicate — cleaner than `noneOf:[…]`
  for one bare predicate. **Worked case: PALACE-TYPE (government-center) buildings** — `BUILDING_PALACE` + the 8
  `bGovernmentCenter` pseudo-palaces (`FORBIDDEN_PALACE`, `EL_ESCORIAL`, `VERSAILLES`, `EDINBURGH_CASTLE`, …) — carry
  `requires.build.disabled: "IS_CAPITAL"`: you can't player-BUILD one where a government center already exists
  (CvCity.cpp:2654). **⚑ This is the PLAYER build gate ONLY** — the engine's FORCED palace relocation (capital falls →
  hard-move, even into a gov-center city) is an UNGATED actor that bypasses `requires` (the #437 placement-gate invariant:
  the gate is the caller's job; engine outcomes that must always happen bypass it).
- **Tech** appears in `requires.build` only as a per-candidate CONFIRM (multi-parent AND/OR), never as a generation driver.

### 4.2a `allowed` — the declarative INSTANCE CAP (owner 2026-06-17)
The ceiling on **how many may exist** — a distinct concept from `requires` ("what I NEED"). `allowed` names the cap with the
**real number**, so the engine permits a build while `count < allowed`; nobody authors the off-by-one neighbor (the SELF
`requires` atom forced "cap 1" to be written `max:0`, which conflated *needed* with *allowed* — withdrawn). Two shapes, told
apart by the **key namespace**:
- **Self-cap** — `allowed: { <scope>: N }` (scope key: `world`/`team`/`empire`) = "at most N of ME at scope." For a building
  the cap scope ALSO derives its wonder category (`world`→worldWonder, `team`→teamWonder, `empire`→nationalWonder; this is
  literally `isWorldWonder == getMaxGlobalInstances() != -1`, CvGameCoreUtils.cpp:340-369). A unique unit is `allowed:{empire:1}`.
- **Category count-cap** — `allowed: { <wonderCategory>: N }` (category key: `worldWonders`/`teamWonders`/`nationalWonders`,
  + reserved `totalWonders`) = a **per-city** cap on how many of that category a city may hold. Authored on **CultureLevel**
  (a culture level grants the city its allowance); city scope is implicit. (Replaces CultureLevel's old `identity.maxWorldWonders…`.)
- **Absent ⇒ uncapped.** Engine owns everything dynamic: ignoring the cap under game options
  (`NO_WONDER_LIMIT`/`NO_NATIONAL_UNIT_LIMIT`/`CHALLENGE_ONE_CITY`), era-scaling the base, and `+extra` bumps — none touch the
  parser (§0.6). Enforcement reads the **tally** count of the (SELF or category) at scope. **OCC-specific limits are NOT
  authored here** — OCC simply forces wonder limits off; any future game-option-specific override is a *game-option-specific
  JSON* the engine loads when the option is enabled (the override-by-design mechanism, generalizing `replacedBy`).

### 4.3 Modifier families — `<family>.<scope>[.<targetType>.{TARGET}][.<member>].<unit> = value`
Flat per-family surface, **no `modifiers` wrapper** (modifier-spec §1.2). Examples:
```jsonc
"happiness":  { "city": { "flat": 2 } },                                  // single-concept family
"production": { "city": { "percent": [ 25, {"value":25,"enabled":{"min":["BONUS_COAL",1]}} ] } },
"food":       { "city": { "improvements": { "IMPROVEMENT_FARM": { "flat": 1 } } } },  // entity-targeted (keyed)
"maintenance":{ "empire": { "distance": { "percent": -10 } } }            // grouped family, member `distance`
```
- **SPLIT families**: `yield`→`food`/`production`/`commerce`; `commerce`→`gold`/`research`/`culture`/`espionage`;
  `property`→ one family per `PROPERTY_*`. **GROUPED families** keep members (`maintenance`/`defense`/…). Entity-targeted
  deposits stay **keep-on-source** keyed by `targetType.{TARGET}` (modifier-spec §6.1). The unit plane adds its own family
  set (`strength`/`withdrawal`/`firstStrike`/`bombard`/`collateral`/`air`/`heal`/`movement`/`experience`/`workRate`/`cargo`/
  `vision`/`capture`/…). The **exhaustive per-entity family list is in `migration-renames.md`** — families are extensible
  (one per concept), so this doc gives the *kinds*, not a frozen enumeration.

### 4.4 `grants` — one-shot / recurring provisions (top-down, NOT cascading)
- **lists**: `grants:{buildings,units,techs,civics,specialists,promotions,traits,bonuses,freePromotions,foundBuildings,…}`.
- **numeric pulses**: `grants.<channel>: value` (e.g. `grants.revolution: -100`, `grants.population: 1`, `grants.goldenAge`).
- **`grants.repeatable`**: `[ {<payload>, interval, chance?, enabled?} ]` — recurring (PropertySpawn → a criminal; per-turn
  heals). `chance` reuses the §2.6 `per` scaler.
- **`grants.foundBuildings`**: a founder's settle-time building seed (each `{building, enabled}`).
- **`outcomes:{kill,actions}`** — DEFERRED mission-triggered grants; carried RAW today, reworked at the #430 outcome-system pass.

### 4.5 How counts read the TALLY
A count atom (`{type,scope,min/max}`) at empire/team/world scope, and a `per` scaler at those scopes, read the **tally**
(the additive count machine, `tally-cascade-spec.md`); city/plot counts read the local city. Presence = `min:1`.

### 4.6 Intrinsic + auxiliary sections (read by their own systems, not the 3 machines)
`text`/`cost`/`ui`/`world`/`sound`/`identity`/`ai` (intrinsic; the 3 art blocks via `curate_common.ART_BLOCK`); `loadPrune`
(load-time game-option prune); `policies` (civ playability); `succession` (`upgradesTo`/promotion-line); `excludes`
(mutual exclusion); `produces` (a Build's outcome FKs); `condition` (Victory); `effect` (Vote); `vision` (the LOS resolver
sub-object); `capabilities` (unit boolean abilities); `mapGeneration`; `replacedBy` (the `CvInfoReplacements` conditional swap).

---

## 5. Worked example — a building, everything composing
```jsonc
{
  "type": "BUILDING_FORGE",
  "text": { "description": "TXT_KEY_BUILDING_FORGE" },
  "enables": { "units": ["UNIT_CROSSBOWMAN"] },
  "requires": { "operate": { "all": [ {"type":"BONUS_IRON","scope":"city","connection":"trade|vicinity"} ] } },
  "production": { "city": { "percent": 25 } },
  "happiness":  { "city": { "flat": 1, "enabled": "HAS_POWER" } },
  "grants": { "repeatable": [ {"unit":"UNIT_PROPERTY_CRIMINAL","interval":"perTurn",
                              "chance":{"per":{"type":"PROPERTY_CRIME","scope":"city"}}} ] },
  "cost": { "production": 120 },
  "identity": { "capital": false }
}
```
Reads cold: *a Forge describes itself; unlocks the Crossbowman; needs connected iron to keep operating; +25% production and
(while powered) +1 happiness in its city; each turn may spawn a criminal scaled by crime; costs 120 hammers.* No engine
knowledge required — that is the bar for every authored entity.

---

## 6. The runtime layer (for the #430 prototype)

### 6.1 How `readJson` CONSUMES the shape (picojson → in-memory structures of the SAME shape)
`readJson` does not *produce* a shape — the shape is defined by §1–4 (and authored by modders); `readJson` **consumes** it,
loading each entity's JSON into an in-memory structure that **mirrors that shape** (it does not transform into a different
model). The only load-time work on the values: FK Type-strings resolved to indices via the shared type registry; readable
values converted to `int×100` ONCE (deterministic; #432 owns de-scaling); condition trees materialized as evaluable
`all/any/noneOf` nodes with atom/predicate leaves; modifier families materialized as deposit records `(family, scope,
[targetType,TARGET], [member], unit, value, per?, enabled?, disabled?, ai?)`; `enables`-family/`grants` as edge/provision
lists. These are FRESH structures (picojson, NOT the old `CvXxxInfo` layout / `CvInfoUtil` — cascade-engine-430.md §2b/§3),
but their shape IS this data model — one shape, consumed here, read by the machines (§6.4).

### 6.2 Derived indices (built once on load)
The **`enables` FORWARD index** keyed by the HAS-side type (generation; the #195 enabler index, partly kept) + the
**reverse index** (cold-path: pedia "unlocked by / required by / obsoleted by").

### 6.3 Registries (engine-resolved, extensible) ⚑ #430
The catch-all **token registry** (`TURN`/`POPULATION`/`SELF`/…) and the per-system **predicate registry**
(`HAS_RIVER`/`IS_CAPITAL`/…). Each system documents + owns its tokens/predicates; unknown ⇒ ignored.

### 6.4 The machine read-interface (the contract the 4 components share)
- **tally** ← count atoms (`{type,scope,min/max}` cross-city) + `per` scalers (cross-city). Roll-up UP the spine.
- **modifier** ← deposit records; flow DOWN the spine; targets read O(1) summed accumulators per `(family,member,unit)`;
  `enabled`/`disabled`/`per` re-evaluated each recompute. `effective = (base+Σflat)×(100+Σpercent)/100×Π(mult/100)`.
- **enabler** ← `enables`-family forward index (generate CAN GET) + `requires` trees (gate), over the HAS sets (+ tally for
  higher scopes). One shared frontier read by UI greying + AI `doProduction`.

### 6.5 Boundary — engine machinery is NOT in the data (§0.6)
The data carries only payloads / conditions / relationships. The **tally, the event-hook system (fires `grants` +
`repeatable`, maintains the tally), the cascade arithmetic (combine modes, multiplier composition), the type/token/predicate
registries, and the producers/resolvers (create-unit, the vision LOS best-of, the outcome system)** are engine-side, built
at #430 — never authored.

---

## 7. OPEN / flagged for the prototype
- ⚑ token + predicate **registries** concrete form (code-side, extensible) — #430.
- ⚑ cascade **arithmetic / combine modes** (`sum`/`max`/`min`, cost-asymmetric, multiplier composition, `flatPlacement`) —
  modifier-spec §7/§9.
- ⚑ tally **storage** (current vs lifetime counts) — tally-spec §6.
- ⚑ Phase-F data fixes the machines will want: continuous gates `build`→`operate`; grey-vs-hide; predicate reconcile (§2.5).
- ⚑ the fresh runtime object's relationship to the **EXE-bound `CvXxxInfo` surface** at cutover — cascade-engine-430.md §3.
- ⚑ `outcomes` (deferred to the #430 outcome-system pass) + the event system (deferred to #425) are NOT in the machine
  read-interface yet.
- ⚑ **Per-info-type SCHEMA / section allowlist (owner 2026-06-17, a "when everything is nailed" step).** Once the structure
  is fully settled, ship per-info-type specifications the parser matches against — which reserved sections + modifier
  families are valid on each info type — so misplaced keys (building-only data on a unit, etc.) are REJECTED rather than
  silently ignored. (Distinct from the `allowed` cap §4.2a; this is structural validation. Modder-facing note in
  `docs/modders/datastructure/README.md`.) Not built during the migration; a post-settle hardening pass.
```
