# `readJson` — the BoolExpr-routed data-feed reader (build plan)

> The first #430 build item after the StoneBase dry-calc validation (`CvGame.cpp:5822`: *"redesigned properly — JSON
> parsed through BoolExpr — after the dry-calc (StoneBase) validation"*). It is the **data-feed prerequisite**: the
> modifier + enabler machines consume the new-vocabulary structures `readJson` produces, so **nothing below it
> computes until it exists** (the tally is the one exception — it rolls up raw counts and needs no JSON, so it is the
> cheap parallel first-shadow; see [`cascade-engine-430.md`](cascade-engine-430.md) §7).
>
> **Status: NOT BUILT.** The first prototype (`CvCascadeReadJson.cpp`) was **purged** for predating a proper
> BoolExpr-routed design (tombstones `Engine/CvGame.cpp:5822`, `Cascade/CvEventSpine.cpp:302`). This is the rebuild.
> DESIGN authority: [`json.md`](../../specs/json.md) (the shape it parses) + [`enabler.md`](../../specs/enabler.md) /
> [`modifier.md`](../../specs/modifier.md) / [`tally.md`](../../specs/tally.md) (the structures it feeds). Reference
> implementations: the offline **`Tools/ReadJson/readjson.cpp`** grammar harness (proven) + the **StoneBase** dry-calc.

---

## 1. What it is

A **fresh** picojson reader that parses each `Assets/Data/<type>/*.json` entity ([`json.md`](../../specs/json.md) §1-9)
into the in-memory runtime structures the three machines read — **routing every conditional through `BoolExpr`** and
converting human values to **×100 fixed-point at the leaf**. It is built **from scratch, interface-bounded**, never
threaded through `CvInfoUtil` / `CvXMLLoadUtility` / the old `read()` path (those are demolition fodder,
[`cascade-engine-430.md`](cascade-engine-430.md) §4). Home: `Sources/Cascade/` (alongside `CvEventSpine`).

It is a pure **consumer** of the modder-authored shape — `json.md` defines the shape, `readJson` loads it; it never
defines the shape. One shape, parsed here, read by tally/modifier/enabler.

---

## 2. The grammar reference — mirror `json.md` + StoneBase's parser, emit `BoolExpr` + fresh structures

**The authoritative grammar reference is [`json.md`](../../specs/json.md) + StoneBase's live parser** (`ConditionParser`,
`ModifierFamilyParser`, the typed `Condition`/`ModifierFamily` model + the `/render` renderer) — the spec-current,
parity-proven model the C++ port is the blueprint *of*. **The C++ `Tools/ReadJson/readjson.cpp` harness is FROZEN**
(owner ruling 2026-06-29 — its render role moved to StoneBase `/render`, its conformance role is done) and is **stale**
(its `any` is the retired OR-of-AND-groups shape). Use it ONLY as a **C++03/picojson traversal skeleton** reference —
never as the grammar truth; follow `json.md` + StoneBase on every shape.

The in-DLL `readJson` mirrors that traversal but emits a `BoolExpr` tree + fresh runtime structures (not text).

**Lift the SKELETON only (the frozen harness as a C++03/picojson example):**
- The **picojson traversal idiom** (`v.is<picojson::object>()` / `.get<>()` / `picojson::array` walks; the `mget`
  safe-lookup at `readjson.cpp:267`) — the DLL already links picojson (via the PCH umbrella `CvGameCoreDLL.h:310`,
  header `Sources/include/picojson.h`), so the parse + traversal skeleton ports unchanged.
- The **vocabulary tables** (`readjson.cpp:34-84`): `RESERVED_SECTIONS`/`INTRINSIC`/`SCOPES`/`UNITS`/`ATOM_KEYS`/
  `PER_KEYS`/`ENABLES_BUCKETS`/`PRED_BARE`/`PRED_PARAM`/`MEMBERSHIP`/`ALLOWED_CATEGORIES` — the recognizer surface.
- The **recursive checker shape**: `walk_entity` (`:361`, top-level key dispatch — object-valued unknown key ⇒
  modifier family, scalar unknown ⇒ flag/text), `check_condition` (`:125`), `check_requires` (`:156`),
  `check_enables` (`:166`), `check_allowed` (`:179`), `check_per` (`:191`), `check_deposit`/`check_value`
  (`:200`/`:216`), `check_family` (`:230`), `check_grants` (`:240`). This tree IS the parse tree the builder walks.

**Build fresh / differ (the harness can't link the DLL, so it renders text — `readjson.cpp:255-259`):**
- **Conditionals → `BoolExpr`.** The harness's `render_cond` (`:277-308`) is the exact control flow; map it to
  `Sources/Infrastructure/BoolExpr.h` (classes at `BoolExpr.h:91/113/143/164`): **`all`→`And`**, **`any`→`Or`**,
  **`noneOf`→`Not(Or(…))`**, **`disabled`→`Not`**, leaves → `Has`(GOM presence/count atom) / predicate / `{PRED:param}`.
  Evaluated in-game against any `CvGameObject` (the same engine path `legacyBlockReason` uses for `constructCondition`).
  - ⛔ **Follow `json.md` §3.4, NOT the harness, on `any`.** The harness still parses `any` as an **array-of-AND-groups**
    (OR-of-ANDs, `readjson.cpp:133-139`) — that shape is **RETIRED**. `any` is a **plain recursive boolean tree** (a
    flat `||` over its direct children, each a leaf or nested `all`/`any`/`noneOf`). The `!X` prefix is shorthand for
    `noneOf:[X]` on one leaf. The in-DLL parser builds the recursive tree; do not carry the harness's group shape.
  - **Membership sugar must be DESUGARED** (the harness recognizes but does not desugar it, `:147-150`):
    `{terrain|feature|bonus:[A,B]}` → `Or(HAS_<KEY>:A, HAS_<KEY>:B)`.
- **×100 fixed-point at the leaf.** The harness does **no** scaling (`num` `:260` prints raw). `readJson` performs the
  **single human→`int×100` conversion** at each magnitude leaf (`flat`/`percent`/`multiplier`; `json.md` §3.6 —
  "a ×100 value in a JSON file is a bug"). This is the ONE place the conversion happens (determinism;
  [DEC-fixedpoint-x100](../../architecture/decisions.md#dec-fixedpoint-x100)).
- **Fresh runtime structures, not strings.** `flatten`/`render_entity` (`:310`/`:325`) become builders of: the
  **modifier-family deposit tree** (`<family>.<scope>[.<target>|.{TARGET}][.<member>].<unit>` → the per-`(family,
  member, unit, scope/target)` deposits the modifier machine sums, `modifier.md` §6); the **`enables`-family** buckets
  (`enables`/`obsoletes`/`replaces`/`disables` per-kind lists, `json.md` §4.1-4.2); the **`requires`** `build`/`operate`
  `BoolExpr` trees + the `allowed` caps; the **`grants`** provisions; the `per` count-scalers.
- **Real error handling, not QA tables.** Drop the harness's `Report`/`flag()`/`print_map`/exit-code gate
  (`readjson.cpp:95-108`, `:474-486`) — those are conformance scaffolding. The in-DLL reader asserts / logs a real
  parse error.
- **Asset path, not Win32 glob.** Drop `find_json` (`:383`, `FindFirstFileA`) + the `main`/argv/`--complex` I/O — the
  DLL gets its file list from the mod's own asset path.

**Caveats the harness can't supply (it is light-touch there):** `grants` (`:240`, only tallies keys + recurses
`repeatable[].enabled`) and `identity` (`:371`, skipped) get **no full grammar** from the harness — the builder must
add real handling per `json.md` §5 (grants: lists, numeric pulses, `foundBuildings`, `repeatable` with `interval`/
`chance`, property pulses with `on`/`relation`/`distance`) and §7 (identity flags, incl. `notConstructible`/`autoBuild`).

---

## 3. The EXE boundary (the only fixed constraint)

- **FK resolution via the kept type registry.** `readJson` resolves `INFOTYPE_NAME` ids through
  `GC.getInfoTypeForString` (`Defines/CvGlobals.cpp:2682`, decl `CvGlobals.h:1418`) over `m_infosMap`
  (`CvGlobals.h:929`) — the EXE binds the same indices, so the fresh structures key on the same `int` ids the engine
  uses. (`setInfoTypeFromString` `:2708` registers any new ids.)
- **Runs IN ADDITION to the XML load during shadow.** The XML path (`SetGlobalClassInfo` → `read()`,
  `Infrastructure/CvXMLLoadUtilitySet.cpp:1516`/`:1588`) stays authoritative; `readJson` populates the cascade's own
  fresh structures in parallel. At the **atomic cutover**, the XML path is deleted and the fresh structures serve the
  **EXE-bound accessor surface** (`CvInfoBase` DllExport getters `Infos/CvInfoBase.h:55/71/72/73/75`; `read()` is NOT
  DllExport, so it goes). Serving that surface from fresh structures (or reworking it) is a **cutover** detail, not a
  shadow one.
- **`BoolExpr` is the deliberate reuse** (`cascade-engine-430.md` §2b) — the one existing piece pulled out, not rebuilt.

---

## 4. Validation — the same two legs, never mixed

- **Intent surface:** StoneBase **`GET /render?type=<TYPE>`** (`src/Application/Features/Render/RenderEntity.cs`) states
  in plain English what a file *says* it does (enables / requires / allowed / modifiers / cost) — the modder "is this
  what I meant?" check + pedia seed, read from the same parsed model the cascade uses. (This replaces the frozen
  `readjson.exe --render`; §2.) It is the third leg beside the two below.
- **PARITY (offline):** StoneBase already validates the curated JSON → cascade values vs `/computed` to the cent
  (yields/commerce). `readJson` feeding the in-engine machines must reproduce the SAME values StoneBase computes from
  the SAME JSON — StoneBase is the blueprint.
- **SHADOW (in-engine, per consuming machine):** once a machine reads `readJson`'s structures, it runs alongside legacy
  and emits a per-turn `[TAG]` diff via the event spine ([`logging.md`](../../specs/logging.md)) — **not** a `/shadow/*`
  endpoint (retired). `readJson` itself has no shadow diff (it is a feed, not a calc); it is proven by the render
  intent + the machine shadows it enables.

---

## 5. Build increments (each compiles + is validated before the next)

1. **Entity-reader skeleton** ✅ **DONE** — `Sources/Cascade/CvCascadeReadJson.{h,cpp}`: picojson load of every
   `Assets/Data/**/*.json` (located via `gDLL->getModName(true)` + a recursive Win32 walk) → a fresh per-entity record;
   the top-level `walk_entity` classification; type-registry FK resolution via `GC.getInfoTypeForString`. A one-shot,
   gated `[READJSON]` probe at `doTurn` proves it. **Verified live: every file parses and all type ids resolve, save a
   few by-design non-resolvers** (the probe surfacing them is the point, not a bug) — curated-only constructs the
   engine registry doesn't carry as primary entries: `TECH_GAME_START` (the synthetic
   cascade start node, validation.md), `CULTURELEVEL_ALT_POOR` (a `replacedBy` alternate Info, json.md §9), and
   `PROMOTION_COMPLEX_AGGRESSIVE` (a `COMPLEX_` option-selected variant). Next increments populate the fresh record.
2. **The conditional translator** ✅ **DONE (+ gap survey)** — `rj_translate` maps a JSON condition onto the engine
   `BoolExpr` tree: `all`/`any`/`noneOf`→`And`/`Or`/`Not` (binary, left-folded); type atom / type-param predicate →
   `BoolExprHas(GOM,id)` (GOM by infotype prefix); relief/water/city predicate → `BoolExprIs(TAG)`; membership
   `{terrain|feature|bonus:[…]}` → `Or` of `Has`; `!X` → `Not`. Proven live via `buildDisplayString` renders; the bulk
   of leaves map cleanly. The unmapped leaves are SURVEYED (`[READJSON/cond-gap]`), in **5 buckets** that drive the
   next increments: **(a)** count/value thresholds (`POPULATION`/`CITY`/`TEAM` + count thresholds + `PROPERTY_*`
   bands) → `BoolExprGreaterEqual`+`IntExpr`,
   the counts reading the **tally** → **increment 3**; **(b)** plot predicates with no `TagTypes`
   (`HAS_RIVER`/`HAS_IRRIGATION`/`HAS_FEATURE`/`HAS_COAST{minArea}`/`latitude`/`natureYield`) → extend `TagTypes`;
   **(c)** city/player state (`HAS_POWER`/`STATE_RELIGION`/`STATE_RELIGION_IN_CITY`) → new leaf / predicate-eval;
   **(d)** type-kinds with no GOM (`CULTURELEVEL_*`, `MAPCATEGORY_*` (space, not-fleshed), `VICTORY_*`); **(e)**
   structural: the `dormant` clause (handle as `requires.operate.dormant`, enabler.md §3, not a leaf). Unmapped leaves
   stand in as a `true` constant for the SURVEY ONLY (json.md: an unknown predicate is ignored) — a placeholder, not
   the design.
   - **Gap-closing (2.a) ✅** — bucket **(a)** value/band atoms: `PROPERTY_*` → `IntExprProperty` and `POPULATION`/
     `HEALTH`/`HAPPINESS` → `IntExprAttribute`, each compared `≥min` (`BoolExprGreaterEqual`) / `≤max`
     (`Not(BoolExprGreater)`) / both (`And`); via the existing engine `IntExpr` leaves. Removes the `PROPERTY_*` +
     `POPULATION` gaps.
   - **Gap-closing (2.b) ✅** — the **cross-city TALLY-backed count**: a fresh `IntExprCascadeCount` leaf reads
     `cascadeTally()` for the evaluated object's owner (`CvGameObjectPlayer::getPlayer()`, a new accessor) — `≥N`-of-a-
     building/unit-type → `BoolExprGreaterEqual`/`Not(Greater)` over the tally count. **Verified live: the building/unit
     count thresholds now map via the tally leaf** (the residual are non-tally-domain counts). EMPIRE scope; team/world
     rollup, city-local, the `CITY`/`TEAM` tokens, and non-building/unit domains remain follow-ons. NOT yet evaluated
     in a live gate (the enabler is later) — the probe builds + renders it.
   - **Gap-closing (2.e) ✅** — the structural **`dormant`** clause (`requires.operate.dormant: X`, enabler.md §3 /
     json.md §4.3): "go dormant WHILE X present" → the clause contributes `AND NOT(trigger)`. `rj_translateClause`
     peels `dormant` (it is NOT a predicate — it was being mis-surveyed as one), folds `Not(translate(trigger))`, and
     handles it as a sibling of `all`/`any`, the sole key, or a tree (the unit `requires.build.dormant.all`).
     **Verified live: the `dormant` gap is gone and its trigger leaves now map.**
   - **Remaining gaps are by design, NOT readJson's to close** (re-grounded against json.md §3.5 + enabler.md §3.1):
     an unknown predicate is **ignored, never false** (so an unmapped leaf is spec-correct), and predicate EVALUATION
     belongs to the **enabler** machine (it evaluates conditions against the `CvGameObject` target), not readJson's
     translation. Buckets **(b)** plot relief/adjacency (`HAS_RIVER`/`HAS_IRRIGATION`/`HAS_FEATURE`/`HAS_COAST{minArea}`)
     + **(d)** `MAPCATEGORY_*` are **spec-flagged in-flight** (json.md line 212 — "not yet fully fleshed out,
     space-map-related"); **(c)** city/player-state (`HAS_POWER`/`STATE_RELIGION`/…) + `CULTURELEVEL_*`/`VICTORY_*` +
     `CITY`/`TEAM`/`latitude`/`natureYield` are predicate-evaluator work that lands with the enabler. Do NOT extend
     `TagTypes` speculatively for these.
3. **Modifier families** ✅ **DONE (+ survey)** — `rj_walkModNode`/`rj_parseMag` parse a modifier-family key into a
   GENERIC deposit-address tree (mirroring StoneBase's `ModifierFamilyParser`: scope/target/member/entity-key are
   opaque child nodes; only the fixed magnitude `Units` set — `flat`/`percent`/`multiplier`/`postMultiplier`/
   `rawPercent`/`perPopulation`/`perSpecialist`/`perCorporationLevel` — become leaves). Each leaf does the single
   human→×100 conversion (`rj_x100`, a BLANKET ×100 per fixed-point-and-scales.md §1/§3.2); `enabled`/`disabled`
   reuse the condition translator; `per` + count-by-type bare leaves (`freeSpecialists`/`allowedSpecialists`)
   recognized. Classification tightened — `tags`/`state` (§8) + `provides` (§5a) added to the skip set so only true
   families are walked. **Verified live via the `[READJSON/mod]` renders + `[READJSON/mod-survey]`** (×100 confirmed:
   `+1`→`100`, `-1`→`-100`, `+3`→`300`); no `multiplier` is authored anywhere (the combine is additive in practice,
   modifier.md §2a). The persistent deposit structure the modifier machine reads is built at the cutover.
4. **`enables`-family + `requires` + `allowed`** ✅ **DONE (+ survey)** — `rj_walkEnableEdge` parses each
   `enables`/`obsoletes`/`replaces`/`disables` edge's per-kind id buckets (json.md §4.1) and FK-resolves the referenced
   ids; `rj_walkAllowed` counts the `allowed` caps (scope self-cap + wonder-category, §4.4). `requires` build/operate is
   already translated (increment 2). **Verified live via `[READJSON/edge]` renders + `[READJSON/edge-survey]`** — the
   bucket ids resolve cleanly bar a few by-design non-resolvers (`PROMOTION_COMPLEX_AGGRESSIVE`, a `COMPLEX_` option
   variant; `FORCE_TEAM_ELIGIBLE`, a non-infotype vote token); all json.md §4.1 bucket kinds + the scope/wonder cap
   kinds seen. The persistent buckets the enabler's GENERATE pass reads are built at the cutover.
5. **`grants`** (real grammar — the harness's light-touch gap) + the remaining intrinsic/classification blocks
   (`identity`/`skills`/`tags`/`capabilities`) as their consumers need them.
6. **The cutover seam** — serve the EXE accessor surface from the fresh structures; delete the XML `read()` path. (Last,
   atomic — `cascade-engine-430.md` §2/§3.)

---

## See also
- [`cascade-engine-430.md`](cascade-engine-430.md) — the parent #430 build + demolition plan (this is its §1.4 item).
- [`json.md`](../../specs/json.md) — the authoritative shape `readJson` parses.
- [`modifier.md`](../../specs/modifier.md) / [`enabler.md`](../../specs/enabler.md) / [`tally.md`](../../specs/tally.md)
  — the machines it feeds. [`event-spine.md`](../../specs/event-spine.md) — the dispatch the consumers ride.
