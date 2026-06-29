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
   gated `[READJSON]` probe at `doTurn` proves it. **Verified live: 13,473 files parsed (0 failures), 13,470/13,473
   type ids resolve.** The **3 by-design non-resolvers** (the probe surfacing them is the point, not a bug) are
   curated-only constructs the engine registry doesn't carry as primary entries: `TECH_GAME_START` (the synthetic
   cascade start node, validation.md), `CULTURELEVEL_ALT_POOR` (a `replacedBy` alternate Info, json.md §9), and
   `PROMOTION_COMPLEX_AGGRESSIVE` (a `COMPLEX_` option-selected variant). Next increments populate the fresh record.
2. **The conditional translator** — `all`/`any`/`noneOf` (recursive tree, spec §3.4) + atoms + bare/parameterized
   predicates + membership desugar → `BoolExpr`. Prove against the harness `--render` for a spread of `requires` trees.
3. **Modifier families** — the deposit-address parse (`<family>.<scope>[.…].<unit>`) + the ×100 leaf conversion + the
   `enabled`/`disabled`/`per` conditioning. This is what the **modifier** machine consumes; cross-check leaf values
   against StoneBase.
4. **`enables`-family + `requires` + `allowed`** — the buckets, the `build`/`operate` split, the caps. Feeds the
   **enabler**.
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
