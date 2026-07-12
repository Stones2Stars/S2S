# `readJson` — the data-feed reader (build plan)

> **⛔ SUPERSEDED — THE XML SEAM IS GONE (owner ruling 2026-07-08).** Every line below saying the XML path
> "stays authoritative until the atomic cutover" predates the ratchet: the XML info classes are ARCHIVED
> (`SourceArchive/Infos/`), HEAD deliberately does not compile, and the JsonInfos + getter wiring are the only
> road to green (root `AGENTS.md` Build And Test ⛔). Never restore the archive.
>
> **⛔ SUPERSEDED IN PART — JSON-vs-CASCADE SEPARATION ([DEC-json-not-cascade](../../architecture/decisions.md#dec-json-not-cascade), owner ruling 2026-07-07).** Below, `readJson`
> is described as mapping into a `CvJsonInfo` that holds the **deposit tree** + the `CvCascadeCondition`. The 2026-07-07
> split changed both homes: `CvJsonInfo` (relocated `Cascade/` → `Sources/JsonInfo/`) is the JSON-info BASE holding ONLY
> the availability model + the (renamed) info-owned typed **`CvJsonCondition`** — the modifier DATA is now **real typed
> members on the `CvJson<X>Info` subclasses**, NOT a generic `deposits` vector on `CvJsonInfo`. readJson populates the
> subclass typed members (virtual `mapFrom`); the cascade builds the DepositIndex/packages by reading those pocos in its
> own setup. Read "CvJsonInfo carries the deposit tree" below with that split.
>
> ⛔ **SUPERSEDED FRAMING — the conditionals are a PORT of StoneBase's typed `Condition` model, NOT `BoolExpr` (owner
> ruling 2026-06-30; see [cascade-engine-430.md §2b](cascade-engine-430.md)).** This doc was written "BoolExpr-routed";
> that was a DETOUR. The condition vocabulary + predicate evaluation are a faithful C++ port of StoneBase's
> `Domain/Conditions/Condition.cs` + `Domain/Conditions/ConditionParser.cs` + `CascadingEnabler/ConditionEvaluator.cs` →
> `Cascade/CvCascadeCondition.{h,cpp}` (model) + `Cascade/CvCascadeConditionParse.{h,cpp}` (the ONE human→data boundary)
>
> + `Cascade/CvCascadeConditionEval.{h,cpp}` (the live-engine walk). "the logic works because of StoneBase, we just port
> the C# code."
>
> **✅ DONE — the port is WIRED + observable in the running game.** `readJson` now parses every conditional (`requires.build`/
> `operate` + each deposit's `enabled`/`disabled`) through `cascadeParseCondition` into a **typed `CvCascadeCondition`
> tree** (NOT `BoolExpr`); `CvJsonInfo` carries `CvCascadeCondition*`; and the enabler/modifier gates (`en_requiresMet`,
> `mm_applies`) evaluate via `cascadeEvalCondition` against the live engine. The whole `BoolExpr` translator
> (`rj_translate`/`rj_fold`/`rj_translateClause`/the GOM/Tag/IntExpr leaves) and `CvCascadeCountExpr` are DELETED — the
> count leaf folded into the evaluator's `ev_countOf`. The `BoolExpr` mechanics described below are the deleted earlier
> design (kept only as the increment history); the §5.2.c "design fork" is RESOLVED = StoneBase's evaluator, now ported.
> **Observable in the running game** — the cascade computes its verdict live, verified against the engine via the endpoints
> ([DEC-done-is-observable](../../architecture/decisions.md#dec-done-is-observable)); no legacy mechanism is altered until the cutover.
> *Follow-on (with the cascade-machine port):* `requires.operate.dormant` is parsed-but-ignored here (faithful to
> StoneBase — it is NOT folded into the operate condition); its `DormantTriggers` extraction + the GENERATE dormancy pass
> land with the machine port.
>
> The first #428/#430 build item after the StoneBase dry-calc validation (`CvGame.cpp:5822`). It is the **data-feed prerequisite**: the
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
StoneBase-verified model the C++ port is the blueprint *of*. **The C++ `Tools/ReadJson/readjson.cpp` harness is FROZEN**
(owner ruling 2026-06-29 — its render role moved to StoneBase `/render`, its conformance role is done) and is **stale**
(its `any` is the retired OR-of-AND-groups shape). Use it ONLY as a **C++03/picojson traversal skeleton** reference —
never as the grammar truth; follow `json.md` + StoneBase on every shape.

The in-DLL `readJson` mirrors that traversal but emits a `BoolExpr` tree + fresh runtime structures (not text).

**Lift the SKELETON only (the frozen harness as a C++03/picojson example):**

+ The **picojson traversal idiom** (`v.is<picojson::object>()` / `.get<>()` / `picojson::array` walks; the `mget`
  safe-lookup at `readjson.cpp:267`) — the DLL already links picojson (via the PCH umbrella `CvGameCoreDLL.h:310`,
  header `Sources/include/picojson.h`), so the parse + traversal skeleton ports unchanged.
+ The **vocabulary tables** (`readjson.cpp:34-84`): `RESERVED_SECTIONS`/`INTRINSIC`/`SCOPES`/`UNITS`/`ATOM_KEYS`/
  `PER_KEYS`/`ENABLES_BUCKETS`/`PRED_BARE`/`PRED_PARAM`/`MEMBERSHIP`/`ALLOWED_CATEGORIES` — the recognizer surface.
+ The **recursive checker shape**: `walk_entity` (`:361`, top-level key dispatch — object-valued unknown key ⇒
  modifier family, scalar unknown ⇒ flag/text), `check_condition` (`:125`), `check_requires` (`:156`),
  `check_enables` (`:166`), `check_allowed` (`:179`), `check_per` (`:191`), `check_deposit`/`check_value`
  (`:200`/`:216`), `check_family` (`:230`), `check_grants` (`:240`). This tree IS the parse tree the builder walks.

**Build fresh / differ (the harness can't link the DLL, so it renders text — `readjson.cpp:255-259`):**

+ **Conditionals → `BoolExpr`.** The harness's `render_cond` (`:277-308`) is the exact control flow; map it to
  `Sources/Infrastructure/BoolExpr.h` (classes at `BoolExpr.h:91/113/143/164`): **`all`→`And`**, **`any`→`Or`**,
  **`noneOf`→`Not(Or(…))`**, **`disabled`→`Not`**, leaves → `Has`(GOM presence/count atom) / predicate / `{PRED:param}`.
  Evaluated in-game against any `CvGameObject` (the same engine path `legacyBlockReason` uses for `constructCondition`).
  + ⛔ **Follow `json.md` §3.4, NOT the harness, on `any`.** The harness still parses `any` as an **array-of-AND-groups**
    (OR-of-ANDs, `readjson.cpp:133-139`) — that shape is **RETIRED**. `any` is a **plain recursive boolean tree** (a
    flat `||` over its direct children, each a leaf or nested `all`/`any`/`noneOf`). The `!X` prefix is shorthand for
    `noneOf:[X]` on one leaf. The in-DLL parser builds the recursive tree; do not carry the harness's group shape.
  + **Membership sugar must be DESUGARED** (the harness recognizes but does not desugar it, `:147-150`):
    `{terrain|feature|bonus:[A,B]}` → `Or(HAS_<KEY>:A, HAS_<KEY>:B)`.
+ **×100 fixed-point at the leaf.** The harness does **no** scaling (`num` `:260` prints raw). `readJson` performs the
  **single human→`int×100` conversion** at each magnitude leaf (`flat`/`percent`/`multiplier`; `json.md` §3.6 —
  "a ×100 value in a JSON file is a bug"). This is the ONE place the conversion happens (determinism;
  [DEC-fixedpoint-x100](../../architecture/decisions.md#dec-fixedpoint-x100)).
+ **Fresh runtime structures, not strings.** `flatten`/`render_entity` (`:310`/`:325`) become builders of: the
  **modifier-family deposit tree** (`<family>.<scope>[.<target>|.{TARGET}][.<member>].<unit>` → the per-`(family,
  member, unit, scope/target)` deposits the modifier machine sums, `modifier.md` §6); the **`enables`-family** buckets
  (`enables`/`obsoletes`/`replaces`/`disables` per-kind lists, `json.md` §4.1-4.2); the **`requires`** `build`/`operate`
  `BoolExpr` trees + the `allowed` caps; the **`grants`** provisions; the `per` count-scalers.
+ **Real error handling, not QA tables.** Drop the harness's `Report`/`flag()`/`print_map`/exit-code gate
  (`readjson.cpp:95-108`, `:474-486`) — those are conformance scaffolding. The in-DLL reader asserts / logs a real
  parse error.
+ **Asset path, not Win32 glob.** Drop `find_json` (`:383`, `FindFirstFileA`) + the `main`/argv/`--complex` I/O — the
  DLL gets its file list from the mod's own asset path.

**Caveats the harness can't supply (it is light-touch there):** `grants` (`:240`, only tallies keys + recurses
`repeatable[].enabled`) and `identity` (`:371`, skipped) get **no full grammar** from the harness — the builder must
add real handling per `json.md` §5 (grants: lists, numeric pulses, `foundBuildings`, `repeatable` with `interval`/
`chance`, property pulses with `on`/`relation`/`distance`) and §7 (identity flags, incl. `notConstructible`/`autoBuild`).

---

## 3. The EXE boundary (the only fixed constraint)

+ **FK resolution via the kept type registry.** `readJson` resolves `INFOTYPE_NAME` ids through
  `GC.getInfoTypeForString` (`Defines/CvGlobals.cpp:2682`, decl `CvGlobals.h:1418`) over `m_infosMap`
  (`CvGlobals.h:929`) — the EXE binds the same indices, so the fresh structures key on the same `int` ids the engine
  uses. (`setInfoTypeFromString` `:2708` registers any new ids.)
+ **✅ THE XML SHELL READ IS GONE for the 23 replaced infos (owner ruling — landed).** They load from
  `Assets/Data/<folder>/*.json` via **`CvXMLLoadUtility::LoadGlobalClassInfoJson<T>`** — a per-category sibling of
  `LoadGlobalClassInfo` at the SAME call sites/order in `LoadPre/PostMenuGlobals`: scan the folder → sort by type
  (deterministic ids, MP-safe) → `setInfoTypeFromString(type,id)` → `new Cv<X>Info` → `mapFrom(json)`. The
  replaced-type `LoadGlobalClassInfo(GC.m_pa<X>Info, "CIV4<X>Infos", …)` XML calls are DELETED (reading a replaced
  info's XML into the game is HARD BANNED — [DEC-no-xml-into-game]); the legacy XMLs stay in the tree as curator input
  only. `mapFrom` IS the per-info read — `CvInfo::read()` (the old `CvHotkeyInfo::read(XML)` + mapFrom hook) and its
  `cascadeJsonForType` index are REMOVED. **`cascadeLoadJson` now only wires foreign keys** (the reverse-index tail +
  the census + DepositIndex push); it no longer maps aliased types (they are mapped by the loader). FK-resolution
  timing is preserved because the per-category loaders run in the same order the XML loads did.
+ **`BoolExpr` is the deliberate reuse** (`cascade-engine-430.md` §2b) — the one existing piece pulled out, not rebuilt.

---

## 4. Validation — the same two legs, never mixed

+ **Intent surface:** StoneBase **`GET /render?type=<TYPE>`** (`src/Application/Features/Render/RenderEntity.cs`) states
  in plain English what a file *says* it does (enables / requires / allowed / modifiers / cost) — the modder "is this
  what I meant?" check + pedia seed, read from the same parsed model the cascade uses. (This replaces the frozen
  `readjson.exe --render`; §2.) It is the third leg beside the two below.
+ **StoneBase (closed):** StoneBase verified the curated JSON → cascade values against `/computed` — that job is done.
  `readJson` feeding the in-engine machines reproduces the SAME values StoneBase computes from the SAME JSON — StoneBase
  is the blueprint, now repurposed to the perf/spec-check layer ([validation.md](../../specs/validation.md)).
+ **LIVE (in-engine, per consuming machine):** once a machine reads `readJson`'s structures, its per-turn values are
  observable via the event-spine `[TAG]` lines ([`logging.md`](../../specs/logging.md)) — **not** a `/shadow/*`
  endpoint (retired). `readJson` itself emits no value (it is a feed, not a calc); it is proven by the render
  intent + the live manifestation of the machines it feeds ([DEC-done-is-observable](../../architecture/decisions.md#dec-done-is-observable)).

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
2. **The conditional port** ✅ **DONE (wired + observable in the running game)** — a faithful C++ port of StoneBase's typed
   `Condition` model, replacing the deleted `BoolExpr` translator (which was the DETOUR). Three files:
   + **`CvCascadeCondition.{h,cpp}`** (model = `Condition.cs`): a C++03 tagged struct — `kind` ∈
     {GROUP, PRESENCE, PREDICATE}; GROUP holds `all`/`anyOf`/`noneOf` child vectors + `enabled`/`disabled`; PRESENCE
     holds `type`/`scope`/`min`/`max`/`connection`/`vicinity` (+ the parse-resolved engine `id`); PREDICATE holds
     `predKind`/`param`. Noncopyable, owns its children (dtor recurses).
   + **`CvCascadeConditionParse.{h,cpp}`** (`ConditionParser.cs`): `cascadeParseCondition(picojson::value)` — the ONE
     human→data boundary. Normalizes every convenience (bare type-string → `PresenceAtom` w/ implied scope; bare/`!`-
     prefixed predicate; atom object; membership sugar `{terrain|feature|bonus:[…]}` → an `anyOf`; numeric
     `{latitude}`/`{existedFor}`/`{HAS_COAST:{minArea}}`; single-key `{HAS_BONUS:X}`/…) and FK-resolves each type/param
     via `GC.getInfoTypeForString`. `readJson` calls it for `requires.build`/`operate` + each deposit's
     `enabled`/`disabled`; after this the cascade sees only typed nodes.
   + **`CvCascadeConditionEval.{h,cpp}`** (`ConditionEvaluator.cs`): `cascadeEvalCondition(cond, ctx, flags)` walks the
     tree against the **live engine** (`CvCity`/`CvPlayer`/`CvTeam`/`CvPlot`/`CvUnit` via a `CvCascadeEvalCtx`) where
     the C# reads its `EvalState` snapshot — the ONLY substantive porting decision. It implements the full
     `EvalPresence`/`Present`/`CountOf`/`EvalPredicate` surface: type-prefix presence dispatch, the vicinity
     discriminator (owned/neutral/foreign/connected/worked), `ev_countOf` (POPULATION/CITY/TEAM/AREA_SIZE/ERA/
     religion-levels + the **empire/team tally** count for BUILDING_/UNIT_ — this is where the deleted
     `CvCascadeCountExpr` count leaf folded to), and the predicate switch (`IS_CAPITAL`/`HAS_POWER`/`STATE_RELIGION`/
     `IS_HOLY_CITY`/`HAS_RIVER`/`latitude`/…). An unknown predicate is `CASC_PRED_UNKNOWN` → **IGNORED (true)**, never
     false (json §3.5). The two flag readings: enabler `requires.build` is **strict** (`STATE_RELIGION` must match);
     the modifier is **lenient**.

   So the increment-2 "5-bucket gap survey" (count/value thresholds, plot predicates, city/player state, no-GOM kinds,
   the `dormant` clause) is **moot** — the typed evaluator has no "unmapped leaf": predicates the `BoolExpr` translator
   stubbed to a `true` constant (`IS_CAPITAL`/`HAS_POWER`/`STATE_RELIGION`/vicinity/counts) are now **actually
   evaluated** against the live engine. *In-flight by spec, not a port gap:* `MAPCATEGORY_*` + the space-map plot
   filters (`HAS_COAST{minArea}` etc.) stay ignored where json.md §3.5 flags them not-yet-fleshed.
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
5. **`grants` + `provides` + full-coverage census** ✅ **DONE** — `rj_walkGrants` is GENERIC by value-shape (id
   lists / numeric pulses / single-id grants / flags / entry-arrays = `foundBuildings`+`repeatable`+property-pulse /
   structured objects), FK-resolving every id — **no shape is "unknown."** `provides` (§5a) rides the edge walker (its
   `bonuses` bucket). A full top-level-key **CENSUS** (`[READJSON/key]`) classifies every key kind across all entities
   (edge/allowed/grants/requires/provides/intrinsic/family/flag) — **verified live: 0 `UNCLASSIFIED` (complete spec
   coverage).** The intrinsic/classification/auxiliary blocks (§7–§9) are recognized + censused; their **deep** parse
   is their own systems' job (json.md §2/§5.5). By-design non-resolvers (`TECH_GAME_START`, `COMPLEX_` variants,
   non-infotype tokens) surface in the unresolved lists as expected.
   + **NB the readers are SPEC + StoneBase-verified:** each reader mirrors the StoneBase-verified
     parser, so they are NOT re-verified in isolation per-segment — the spec/StoneBase IS the proof; the live
     probe confirms coverage + FK-resolution holistically, in one pass.
6. **The cutover seam** — serve the EXE accessor surface from the fresh structures; delete the XML `read()` path. (Last,
   atomic — `cascade-engine-430.md` §2/§3.)

---

## See also

+ [`cascade-engine-430.md`](cascade-engine-430.md) — the parent #430 build + demolition plan (this is its §1.4 item).
+ [`json.md`](../../specs/json.md) — the authoritative shape `readJson` parses.
+ [`modifier.md`](../../specs/modifier.md) / [`enabler.md`](../../specs/enabler.md) / [`tally.md`](../../specs/tally.md)
  — the machines it feeds. [`event-spine.md`](../../specs/event-spine.md) — the dispatch the consumers ride.
