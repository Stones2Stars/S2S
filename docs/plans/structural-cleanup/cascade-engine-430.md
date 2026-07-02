# Cascade engine (#430) — implementation plan

> ## ⛔ RESUMING AFTER A CONTEXT COMPACTION? RE-READ EVERYTHING FIRST.
>
> If your context was just compacted mid-session, do NOT act from the summary. Re-read the full spec set (the machine
> specs `enabler.md` / `modifier.md` / `tally.md` + `event-spine.md` + this doc + `json.md`) AND the live code in
> `Sources/Cascade/` before touching anything — compaction has poisoned context before; a stale line loses to a later
> owner ruling AND to the live code. (Owner 2026-06-17; this gate is deliberately tested.)

**Status: PROTOTYPE PURGED — restarting on a proper footing.** The first cascade + tally + modifier prototype was
written, shadowed, and then **removed** (it predated a proper BoolExpr-routed `readJson` and the decision to validate
via the external StoneBase dry-calc — removal tombstones at `CvGame.cpp:5822`, `CvEventSpine.cpp:301-302`). Rebuilt
fresh since (this branch): the **event spine** + the **tally** (`CvCascadeTally`, shadow-green) + **`readJson`**
(`CvCascadeReadJson`, full-coverage parse/survey — but NOT yet mapping JSON→runtime data) + the building/unit count
`DOMAIN` emits + the legacy `#195` reverse-index overlay. The modifier/enabler/grants machines are NOT built. The DESIGN is
authoritative in the machine specs — `enabler.md` + `modifier.md` + `tally.md` + `event-spine.md` — and the reference
IMPLEMENTATION is now **StoneBase** (the .NET dry-calc, parity-validated for city yields + commerce). This doc is the
engine BUILD plan + the validated demolition map of the **legacy** machinery the cascade replaces; it is **not** a
status record of built cascade code (there is almost none — see the table).

---

## ⓘ IMPLEMENTATION STATUS — re-verified against `Sources/` on 2026-06-29 (post-purge)

> The HONEST ground truth. The previous status table self-certified a 2026-06-19 sweep and described a prototype that
> has since been PURGED; it paraded retired/banned machinery as built. This table is the post-purge reality from an
> adversarial docs-vs-code sweep (2026-06-29, fanned out, every claim ground-tested). The machine specs describe the
> TARGET shape; almost none of it is built in the engine yet.

| Component | Status | Reality (current `file:line`) |
|---|---|---|
| **Event spine** | ✅ **BUILT (spine + logging only)** | `Sources/Cascade/CvEventSpine.{h,cpp}`: `IEventConsumer` (`CvEventSpine.h:197`), `CvEventSpine` (`:208`), `eventSpine()` (`:224`), `cascadeRegisterConsumers()` (`:229`) registers ONLY the log consumer (`CvEventSpine.cpp:300`). KINDs `DOMAIN`/`DIAGNOSTIC`/`TRACE` (`:28`). **The only DOMAIN (counted) events** = `CASCADE_EVT_BUILDING_COUNT`/`UNIT_COUNT`/`NAME_CHANGE` (`:171`), emitted at `Engine/CvPlayer.cpp:13737` / `:13642` and `Engine/CvCity.cpp:13391`. The `[TAG]` logging domains (`SpineDomainTag`, `:73`) are mostly pre-allocated; only AI-trace domains (hunter/worker/war/…) self-register today. |
| **Scoped accumulator** | ❌ **NOT BUILT (purged)** | The prototype's `CvScopedAccumulator` was removed; no such file exists. Design = the additive scope-accumulator substrate (§1.0). |
| **Tally** | ✅ **BUILT (read-only accessor, buildings + units)** | `Sources/Cascade/CvCascadeTally.{h,cpp}`: a stateless standardized aggregate-count surface — `buildingCount`/`unitCount(iEntity, type, scope)` READ the object-owned counts (`CvPlayer::getBuildingCount`/`getUnitCount`) and roll UP the spine (empire / team / world). **NOT** a store/consumer: no `IEventConsumer`, no `rebuild`/`onEvent`/`shadowDiff` (all removed — a count shadow would be tautological). Reading a raw count is a raw INPUT, not pollution. Building/unit domains; others read from their object owner when added (`tally.md` §5). |
| **Modifier** | ✅ **BUILT + PORTED (2026-07-01)** | The full StoneBase `Calc` port — `MMKernel`/`PercentStack`/`YieldBasePackages`/`BuildingPackage`/`YieldRate`/`CommerceCalc` (`Sources/Cascade/`): all §1 BASE tiers + the assembler + the §2 commerce stage. Port-completeness-audited vs StoneBase → faithful (the last gaps — civic building-keyed percent, projects, the minPosThreshold MIN — now closed). The holistic shadow (`cvCascadeModifierShadow`) diffs `yieldRate100`/`commerceRate100` vs legacy, un-run (parity deferred to the end). **Computes ON-DEMAND per city — the scope-accumulator substrate is a post-shadow cutover optimization, NOT a blocker.** Legacy accumulators untouched (§4). |
| **Enabler** | ✅ **BUILT + PORTED (2026-07-01)** | Generate→gate over the mapped `CvJsonInfo`: `EnablerKernel` + `TechCascade`/`BuildingCascade`/`UnitCascade` (`Sources/Cascade/`); the frontier gates + dormancy/reachability. Port-completeness-audited vs StoneBase → faithful + self-contained (reads its own JSON flags, not the live engine). Deferred (NOT port gaps): CultureLevel category caps (StoneBase lacks them), `MAPCATEGORY_` (XML-only, §7.4), SpecialBuilding team/world caps (data-benign, no group index). Legacy `can*` gates untouched (§4); the `#195` `ConstructRequirement` reverse-index survives as the `enables` seed. |
| **Grants** | ❌ **ABSENT** | No grants consumer (never built). |
| **readJson** | ✅ **BUILT — PARSE + MAP (full json.md coverage), runs at LOAD** | `Sources/Cascade/CvCascadeReadJson.{h,cpp}`: a one-shot gated probe (`cascadeReadJsonProbe`, at **`onFinalInitialized`** — load-time) that parses EVERY `Assets/Data` entity through BoolExpr — `rj_translate` (conditions, incl. `IntExpr` value/tally-count bands + the `dormant` clause); `rj_walkModNode` (×100 modifier-family deposits); `rj_walkEnableEdge` (enables/obsoletes/replaces/disables/obsoletedBy/provides); `rj_walkAllowed`; `rj_walkGrants` (generic by shape) — FK-resolving all ids. **Verified live: 0 `UNCLASSIFIED` top-level keys, all resolved bar 3 by-design.** ✅ It **MAPS** each entity to a **`CvJsonInfo`** (deposits/requires/edges/allowed/grants) held in the entity's per-type **`InfoRepo<CvXInfo>`** (`Repos/InfoRepo.h`) — a parallel layer, NOT on the info objects; the side-table is **RETIRED** (§3). readJson is **static-data-only** (no `CvGameObject`). The modifier/enabler read it via `InfoRepo<…>::get().get(id)`. Standalone `Tools/ReadJson/readjson.cpp` harness is FROZEN. |

**The honest #430 roadmap.** Build the cascade FRESH per the specs, porting from the parity-proven StoneBase reference,
in spec build order: **`readJson` (typed-condition port) + the scope-accumulator substrate → tally → modifier → enabler →
grants** (§1). Each is interface-bounded and wired at the composition root (§2b). Validation has **two distinct legs,
never mixed** (§2): the **external StoneBase dry-calc** (offline PARITY oracle — done for yields/commerce) and, per
machine, an **in-engine SHADOW** (the cascade computed alongside legacy, diffed per-turn via spine `[TAG]` lines — NOT
a `/shadow/*` endpoint; those were a rollerskating conflation of the two legs and are retired). Nothing below "event
spine" exists yet.

---

## 1. THE ROOT SYSTEM — one substrate, three machines (owner 2026-06-16)

The new system roots in a shared **scope-accumulator substrate** with **three machines** on it. They are not three
tangles — they are three instantiations of one primitive, which is what makes the engine coherent. (DESIGN; only the
event spine of §1.0 is built.)

### 0. Substrate — the scope spine + an additive accumulator + the EVENT SPINE (interface-bounded)

- **Scope spine:** `world → team → empire → area → city → plot{improvement|feature|terrain|route} → building | specialist | unit`.
- **Additive accumulator:** deposit → O(1) summed read, parameterized by what it sums. One primitive, three instances.
  The enabler additionally walks the SIDEWAYS/progression axis (tech tree, build chain — `enabler.md` §2); modifiers
  are containment-only. **(NOT BUILT — the prototype's accumulator was purged; rebuild fresh.)**
- **Event spine — the dispatch FRONT DOOR (`event-spine.md`).** `emit(KIND, …)` → consumers read the kinds they care
  about. The tally, `grants`, and logging are all CONSUMERS; the spine sits IN FRONT OF the tally (the tally never
  reaches into game state — domain events come to it). **(BUILT: `Sources/Cascade/CvEventSpine.{h,cpp}`; only the
  logging consumer is registered. The tally/grants consumers are unbuilt.)**

### 1. Tally — counts, roll UP  *(build FIRST after substrate; spec: `tally.md`)*

Per-type counts, **READ** from the object-owned aggregates (`CvPlayer::getBuildingCount`, …) and rolled UP the spine.
Serves: `requires` count-thresholds (`min(BUILDING_X,12)`, empire/team) + the higher-scope HAS sets, the modifier's
cross-city `per` count-scaler, and demographics/AI/score. First because the enabler depends on it (`tally.md` §2/§3). A
**read-only accessor — no store** (`tally.md` §1): serializes AND stores nothing, so there is nothing to seed, rebuild,
or shadow (`tally.md` §4).

### 2. Modifier — magnitudes, deposit DOWN

Per `(family, member, unit, scope)` summed deposits; targets read O(1).
`effective = (base + Σflat) × (100 + Σpercent)/100 × Π(multiplier/100)` (`modifier.md` §2). The realized per-channel
yield/commerce RATE has the two-tier BASE/AFTER shape of `modifier.md` §2a. Replaces the CvCity yield/commerce/health/
happiness/defense/maintenance accumulators + the `process*` apply-loops + the unit extra-stat stack (§4). The StoneBase
`Calc` packages + `ModifierMath` are the parity-proven blueprint for the C++ port.

### 3. Enabler — availability, 2-pass

gather **HAS** (reads the tally for higher scopes) → generate **CAN GET** (the `enables`-family forward index) → gate
**`requires`** (`enabler.md` §1-3). One shared frontier, read by UI greying + AI `doProduction`. Replaces the scattered
`can*` gates + the PreLoop + the caches (§4). NB the `replaces` edge is now **LIVE** (unit succession — `enabler.md`
§2/§ units), no longer a reserved-but-unused family.

### 4. readJson — the DATA INPUT *(owner 2026-06-16: "we HAVE to do this before we go anywhere")*

A FRESH picojson reader that parses the full new vocabulary (`enables`/`obsoletes`/`requires` trees — plus `replaces`;
the modifier families `<family>.<scope>[.<member>].<unit>`; `grants`; the predicate tokens; count atoms; scopes) into
the runtime structures the three machines consume. The JSON conditionals are **parsed into the typed `CvCascadeCondition`
tree** (the StoneBase `Condition` port, §2b) — the prior prototype's hand-rolled `vector<vector<leaf>>` was exactly the
AND-of-ORs mistake `enabler.md` §3.1 warns against.
`readJson` is a pure CONSUMER of the modder-authored shape (defined by `json.md`, not by `readJson`). **(NOT BUILT —
prototype purged; this is the first build item — detailed build plan: [`readjson.md`](readjson.md).)**

**Build order: `readJson` + substrate → tally → modifier → enabler → grants.** Each interface-bounded, each deleting
its slice of the demolition map (§4) as it lands and passes its shadow.

---

## 2. DEVELOPMENT STRATEGY — two validation legs, never mixed; the hard JSON switch is LAST

The key to building a large atomic replacement safely: **don't build it blind in one shot.** There are **two distinct
validation legs** (the previous agent collapsed them into one `/shadow/*` endpoint surface — wrong on both axes; that
surface is retired, `CvHttpServer.cpp:26`):

- **PARITY — external, offline (the StoneBase dry-calc).** A separate process/codebase (`StoneBase`, .NET) rebuilds
  the cascade from the curated `Assets/Data` JSON + raw `/state` and diffs against the engine's `/computed` oracle. It
  is the spec's reference implementation and the C++ port's blueprint; **done for city yields + commerce** (the
  packages + `ModifierMath`). See `validation.md`.
- **SHADOW — in-engine, live (per machine, as it is ported).** Each rebuilt machine runs **alongside** the legacy
  machinery behind gated logging (off in normal play), emitting a per-turn cascade-vs-legacy diff as spine `[TAG]`
  lines (the `event-spine.md`/`logging.md` surface — **not** a `/shadow/*` endpoint, **not** the banned care-scale
  grading). The legacy stays authoritative until its shadow is **clean (parity)**, then it is cut.

- **Atomic-deliverable rule:** the DATA cutover stays atomic (one flip at the very end); the ENGINE is developed
  incrementally and shadow-validated first. The new paths are gated instrumentation until the flip.
- **Shadow data source = PER MACHINE (corrected 2026-06-29).** The **tally** reads raw object counts
  (`m_paiBuildingCount`, …) + the spine's `DOMAIN` events — **no JSON** — so it shadows directly against the live
  objects (apples-to-apples, isolating the count-machine logic; it needs no `readJson`). The **modifier + enabler**
  read `readJson`'s JSON-parsed structures (they consume the family/tree vocabulary the XML objects don't carry) — and
  the JSON-vs-XML data equivalence is exactly what the StoneBase dry-calc already proved offline, so this is sound. The
  XML `read()` path stays authoritative for the EXE-bound accessor surface until the atomic cutover (§3). Build FRESH;
  actively AVOID `CvInfoUtil`/the old `read()` path (§2b).
- **Exact parity IS the success metric** ([DEC-parity](../../architecture/decisions.md#dec-parity) /
  [DEC-mirror-then-redesign](../../architecture/decisions.md#dec-mirror-then-redesign)): the migration MIRRORS the
  engine exactly. A surviving divergence is a data-collection gap (or a new-side bug), mapped to its named source and
  closed — never tolerated as a formula difference, never graded on a "care scale" (banned). Per
  [DEC-no-parity-results-in-docs](../../architecture/decisions.md#dec-no-parity-results-in-docs), divergence NUMBERS
  live in the run, never in this doc.
- **The `readjson.exe` cleartext RENDER is the JSON-INTENT surface** (`Tools/ReadJson/readjson.cpp`): `--render TYPE`
  states in plain English what an entity's JSON *says* it does. That is the intent; the StoneBase parity + the in-engine
  shadow are what the engine *actually does*. A render/behaviour divergence is a triage item.
- **The hard switch (last step):** `readJson` replaces `readXml` at the load seam (§3) **and** the rebuilt machines
  become the sole source as the demolished machinery (§4) is deleted. One atomic landing.

---

## 2b. BUILD PRINCIPLES — from scratch, interface-bounded, poor-man's DI (owner 2026-06-16, 2026-06-29)

- **★ THE MOD FITS THE NEW STRUCTURE — NOT THE REVERSE.** The cascade data model + engine are authoritative; the mod's
  data/content is RESHAPED to fit them. Two guardrails: (a) **PRESERVE SAVES** where possible (name-tagged soft
  format; `@SAVEBREAK` only when unavoidable); (b) **PRESERVE HOW THE GAME WORKS** (the intended player experience
  stays recognizable). Structure & internals are free; the played game is not.
- **Build the components FROM SCRATCH, interface-bounded.** `readJson` + tally + modifier + enabler + grants are
  written fresh as `IEventConsumer`/contract implementations — never threaded through `CvInfoUtil` / the old `read()` /
  `SetGlobalClassInfo` (demolition fodder, §4, both still present: `CvInfoUtil.h`, `Infrastructure/CvXMLLoadUtilitySet.cpp:1516`).
- **Poor-man's DI — the composition root (owner 2026-06-29; the entire DI/composition setup is in scope).** No DI
  container exists (C++03/VC7.1; the EXE binds concretes), so depend on **interfaces** (pure-virtual base, no data —
  `IEventConsumer` is the realized exemplar at `CvEventSpine.h:197`), hold a **pointer to the interface**, and pick the
  concrete with a literal `if`/`switch` at the **composition root**. `cascadeRegisterConsumers()` (`CvEventSpine.cpp:292`)
  IS that root for the spine's consumers — extend it as the tally/grants are built. The full shape + guardrails:
  [`architecture/patterns.md`](../../architecture/patterns.md). The canonical option-gated swap is the trait
  simple/complex split (§7).
- **`picojson` for JSON** (header-only) — `Sources/include/picojson.h`, included via the PCH umbrella
  (`CvGameCoreDLL.h:310`), already proven (it backs `CvHttpServer`). Reuse it; don't bend the XML machinery to JSON.
- **★ THE CONDITIONS/EVALUATORS ARE A FAITHFUL C++ PORT OF STONEBASE — this is the BASIS, not a footnote (owner ruling
  2026-06-30).** "The logic works because of StoneBase; we just port the C# code." So the cascade's condition vocabulary
  + predicate evaluation are a **direct transcription of StoneBase's parity-proven `Domain/Conditions/Condition.cs` +
  `CascadingEnabler/ConditionEvaluator.cs`** (`StoneBase/` is a sibling repo at `/c/code/s2s/StoneBase`): the **typed
  `Condition` tree** (`Cascade/CvCascadeCondition.{h,cpp}` — group/presence/predicate, a C++03 tagged struct, which
  StoneBase was explicitly written to translate to) + the **`ConditionEvaluator` walk** (`Cascade/CvCascadeConditionEval.{h,cpp}`),
  reading the LIVE engine (`CvCity`/`CvPlayer`/`CvPlot`/`CvTeam`) where the C# reads `EvalState`. There are **no design
  decisions to make** — every predicate's semantics (vicinity discriminator, `STATE_RELIGION` strict-vs-lenient, the
  obsolete/civic-waived-prereq filter, `CountOf`) is already decided + validated in the C#; porting wrong is the only
  failure mode. This **supersedes the earlier "`BoolExpr` deliberate reuse"** sketch (`readjson.md`): routing JSON
  through the engine's `BoolExpr` (`And`/`Or`/`Not` over `Has(GOM)`/`Is(tag)`) was a DETOUR — it forced the predicate
  registry into `TagTypes`/`GOM` slots that don't exist for half of json.md §3.5 (`IS_CAPITAL`/`HAS_POWER`/state-
  religion/properties/…), surfacing as the readjson §5.2.c "design fork." The fork is RESOLVED: port StoneBase's
  evaluator; `BoolExpr` is not the condition back-end. `picojson` parses the JSON into the typed `Condition` tree.
- **The derived-data REPOSITORY (`Engine/CvDerivedData.h` — classes `CvDataRepository<T>` / `CvGameDataRepository` /
  `TLazy`; the *file* is `CvDerivedData.h`, there is no class of that name) is NOT built upon — and NOT removed yet.**
  Its `init()`/`reset()` are still wired in the lifecycle (`CvGame.cpp:62`/`:898`, `CvPlayer.cpp:160`/`:763`,
  `CvCity.cpp:55`/`:475`, `CvTeam.cpp:36`/`:190`). The cascade's substrate accumulator is authoritative additive
  aggregation and borrows none of the repository's lazy/dirty machinery; removal of the empty skeleton is a §4
  demolition item, deferred to cutover.
- **The ONLY shared/kept pieces are the hard EXE boundary (§3)** — the type registry + the EXE-bound accessor surface.
  Everything else is fresh.

---

## 3. EXE BOUNDARY — the only fixed constraint (re-verified 2026-06-29)

- **`readJson` will be a FRESH reader** (picojson → fresh runtime structures), NOT a reuse of `CvInfoUtil`/`CvXMLLoadUtility`/
  the old `read()` path. It runs IN ADDITION to the XML load during shadow; at cutover the XML path (`SetGlobalClassInfo`
  → `read()`, `Infrastructure/CvXMLLoadUtilitySet.cpp:1588`) is deleted. (`read()` is NOT DllExport — `Infos/CvInfoBase.h:78/85/133/154`.)
- **★ THE MAPPING/CUTOVER MODEL (owner ruling 2026-06-29; home finalized 2026-06-30).** `readJson` maps each
  entity's JSON to a fresh **`CvJsonInfo`** (deposit lists, the typed `CvCascadeCondition` requires structures, the grant
  provisions) — the **new calc implementations** (modifier/enabler/grants) read it; the **OLD Info variables**
  (XML-populated `m_ai*`/`m_ab*`) are **cut only AFTER** (a) the data is mapped, (b) the new calc is built, and (c) the
  per-machine SHADOW reaches parity. Until all three hold the XML load stays authoritative (cutting now breaks the game
  — nothing maps JSON→engine data yet); the cut is the atomic last step.
  - **⛔ HOME = the per-info-type `InfoRepo`, a SEPARATE parallel layer — NOT on the info objects (owner ruling
    2026-06-30, superseding the earlier "on `CvInfoBase`" sketch). ✅ DONE.** Terminology (owner-pinned): **`CvInfoBase`**
    is the base of the engine's XML-derived **info** objects; **`GameObject`/`CvGameObjectCity`** is the base of the
    runtime **INSTANCES** the conditions EVALUATE against. The JSON-mapped data is a **`CvJsonInfo`** (`Cascade/CvJsonInfo.h`
    — the JSON counterpart to a `CvXInfo`), held in a **per-info-type `InfoRepo<CvXInfo>`** (`Repos/InfoRepo.h`): a
    `get()` singleton per type holding a `std::vector<CvJsonInfo*>` **PARALLEL to `GC.m_pa<X>Info`**, indexed by the
    same id → O(1). readJson `edit()`s the entry; the machines `get()` it.
    - **Why a SEPARATE parallel layer, not a member on the info:** it keeps the migration boundary clean (the engine's
      XML info stays pure; the XML-vs-JSON shadow is **two structures**, swapped cleanly at cutover), it is **immune to
      the `CvInfoReplacements` info-pointer swap** (an array indexed by id stays put while `aInfos[id]` is overwritten),
      access is standardized, and it touches no foundational header. It also **establishes the proper repository
      pattern** the codebase lacked (the old `BuildingsRepo`-style "repos" were experiments; reality was bare arrays
      looped over) — *"it just makes overall structuring more uniform."*
    - **The standard EXE-required fields** (`type`/getType(), description, the DllExport getters) are **NOT duplicated** —
      they stay on the engine `CvXInfo` at the same id; a consumer holding a `CvJsonInfo` (by domain+id) reads them there.
    - **Scope:** the InfoRepo is established as the home for the JSON data; retrofitting the existing engine arrays+loops
      onto the pattern is a separate, later initiative.
  - **⚠ BLAST RADIUS — the trade-off to be careful about (owner ruling 2026-06-30).** The separate parallel layer gives
    **clean drop candidates** at cutover (the engine's XML `m_ai*`/`m_ab*` members drop cleanly — nothing of theirs is
    intermingled with the new data) **but** is a **bigger blast radius** to keep correct. The named care points:
    - **(a) re-map safety ✅ HARDENED.** The hazard: `InfoRepo::edit()` get-or-**creates** without clearing + the walkers
      `push_back`, so re-populating would **DOUBLE** every deposit vector. readJson now calls **`rj_clearAllRepos()`**
      (frees every InfoRepo) before mapping, so the map is **re-run-safe** — no longer relying on the one-shot guard,
      ready for the unconditional load-time map at cutover. (No-op on the one-shot first run.)
    - **(b) index alignment.** `InfoRepo[id]` mirrors `GC.m_pa<X>Info` by id — correct only because the map runs AFTER
      all infos load and resolves ids via `getInfoTypeForString` (the same id space). Breaks if anything reorders the
      info arrays post-map.
    - **(c) the cutover IS the real blast radius.** Every engine site reading an XML info member switches to the
      InfoRepo at once — the clean separation makes each *drop* clean, but the *switch* is the largest atomic change
      (map-before-delete, per-machine shadow-to-parity, atomic).
    - **(d) new info types** must be added to readJson's **`RJ_REPO_TYPES`** X-macro or they silently get no InfoRepo.
      That table is now the **single source** driving edit + get + clear-all together (no dispatch-vs-clear drift) — so
      it's one place to add a type, but still a place that can be *forgotten* (the quiet corner below).
    - **Mitigant — misses mostly YELL LOUDLY (owner 2026-06-30).** The cutover is the *least* dangerous in this sense: a
      site still reading a **deleted** XML member is a **compile error** (loudest possible — it can't build), and
      doubled/wrong values surface as **shadow divergences**. So most of the blast radius is self-announcing. The **one
      QUIET corner is (d)**: a missing `RJ_REPO_DISPATCH` entry (or a silently-empty repo) just makes that entity
      **contribute nothing** — no crash, only a shadow divergence on *that* entity. Watch newly-added / un-dispatched types.
  - **ABI — Infos are NOT EXE-ABI-sensitive (owner-asserted + verified 2026-06-30).** The infos are DLL-allocated,
    stored as `vector<CvXInfo*>` (`CvGlobals.h:1006`), and reached only via the `DllExport` getters — the EXE never
    sizes / value-copies / offset-reads them (a DLL-compiled `getType()` reads `m_szType` wherever it sits; brand-new
    info classes inheriting `CvInfoBase` have been added without trouble). So the earlier "mid-class crash / append-only
    / append-test" caution was **over-drawn** — that crash was a DLL-internal / stale-build artifact, not an EXE-offset
    constraint. (The InfoRepo was chosen over an on-`CvInfoBase` member for the SEPARATION reasons above, not for ABI.)
  - **⏳ NEXT (refinement, within the InfoRepo home):** `CvJsonInfo` is still a flat deposit vector internally; indexing
    it (`modifiers` keyed by family/scope/unit, `enables` by bucket — StoneBase's typed `ModifierFamily` shape) so the
    machine asks the struct and gets the value without a per-access list walk is a follow-up.
  - **★ TWO HOMES — definitions in the InfoRepo, ACCUMULATED STATE on instances (owner ruling 2026-06-30).** The static
    **definitions** (the `CvJsonInfo` from JSON) live in the **per-type InfoRepo** (above). The runtime **accumulated
    state** — the cascade modifier accumulators (a city's summed per-`(family,scope)` totals) — lives on the **game
    INSTANCE objects**. Per §2b those go on the **DLL-internal derived** instance classes (`CvCityAI`/`CvPlayerAI`/
    `CvUnitAI`), **never** the EXE-bound bases (`CvCity`/`CvPlayer`/`CvUnit`). ⏳ The modifier **side-table** is retired
    (→ the InfoRepo, DONE); the modifier's runtime accumulators move onto the instances when that machine is built.
  - **★ The TALLY is the exception — it is a READ-ONLY accessor, NOT new instance state (owner ruling 2026-06-30).**
    Counts are NOT a separate accumulator to home on the instances: the OBJECT already owns its count, O(1)
    (`CvPlayer::getBuildingCount` = `m_paiBuildingCount`, maintained + already emitting its `DOMAIN` event;
    `getUnitCount`; techs on `CvTeam`). So the tally **reads** those and rolls UP the spine (empire/team/world) — a
    standardized accessor, no store, no seed, no shadow. Its old player-leaf singleton-maps are **retired entirely**
    (not relocated): *"creating something new when we already have it is pointless — better to standardize and make
    access predictable."* Reading a raw count is a raw INPUT, not the pollution anti-pattern. ✅ DONE
    (`Cascade/CvCascadeTally.{h,cpp}` is now the read+roll-up surface; the `IEventConsumer` store/`rebuild`/`shadowDiff`
    are gone). Full model: [tally.md](../../specs/tally.md).
  - **★ readJson is STATIC-DATA-ONLY; the live engine is LIVE STATE (owner ruling 2026-06-30).** readJson maps JSON →
    the **`CvJsonInfo` in the InfoRepo** (above) and **never references `CvCity`/`CvPlayer`/the tally**. It parses each
    conditional into the typed **`CvCascadeCondition` tree** (a tree IS static data — `cascadeParseCondition`, the
    StoneBase port); the instance-walking EVALUATION lives entirely on the live-state side in the
    **`cascadeEvalCondition`** evaluator (`Cascade/CvCascadeConditionEval.{h,cpp}`), which the shadow gates call against
    the live engine. (This was the gameobject-vs-info conflation: a static reader must not own instance-walking
    evaluation. The earlier `BoolExpr` trees + the `CvCascadeCountExpr` count leaf are DELETED — the count folded into
    the evaluator's `ev_countOf` tally read.) ⟹ **LOAD-time setup:** readJson maps the static info data
    at **load** (`onFinalInitialized`, moved off `doTurn`), and the **tally** is a read-only accessor over the
    object-owned counts (no seed — the bullet above / [tally.md §4](../../specs/tally.md)); the **enabler**'s HAVE set
    is likewise established from the loaded objects. So **loading a save verifies the static cascade state**
    ([validation.md](../../specs/validation.md) cadence,
    [DEC-structure-before-shadow](../../architecture/decisions.md#dec-structure-before-shadow)); only the live shadows
    of the surviving/replaced engine (`canTrain`/`canConstruct` + modifier rates, with the new build lists logged at the
    **Python consumer layer** before the enabler swap) need an end turn.
- **The shared/kept pieces (EXE-bound):** the **type registry** `GC.getInfoTypeForString` (`Defines/CvGlobals.cpp:2682`,
  decl `CvGlobals.h:1418`) / `setInfoTypeFromString` (`CvGlobals.cpp:2708`, decl `:252`) over `m_infosMap`
  (`CvGlobals.h:929`) — `readJson` uses it for FK resolution because the EXE binds the same indices; and the **EXE-bound
  accessor surface**: `CvInfoBase` DllExport getters `getType`/`getTextKeyWide`/`getDescription`/`getText`/`getHelp`
  (`Infos/CvInfoBase.h:55/71/72/73/75`), the `getNum*Infos()`/`get*Info()` pairs, a few art getters. During shadow the
  OLD objects serve the EXE; at cutover the fresh structures must serve that surface (or it is reworked) — a cutover
  detail. Everything outside this boundary is freely built fresh.

---

## 4. DEMOLITION MAP — what the engine deletes/rewires (re-verified 2026-06-29)

> All legacy paths below are **present and untouched** — the cascade replacement has not happened. Paths are now
> subdirectory-qualified (the tree was reorganized into `Sources/{Engine,AI,Tools,Infos,Defines,Infrastructure,UI,Python}/`);
> line numbers re-verified 2026-06-29 (±, they drift as the files change).

**Enabler (→ §1.3 machine):** `CvCity::canConstruct`/`canConstructInternal` (`Engine/CvCity.cpp:2496`/`:2557`) +
`CvPlayer` (`Engine/CvPlayer.cpp:6513`/`:6572`); `canTrain` (`Engine/CvCity.cpp:2357` + the UnitCombat overload `:2453`;
`Engine/CvPlayer.cpp:6374`); `canEverResearch` (`Engine/CvPlayer.cpp:8260`, `Engine/CvGame.cpp:11276`); `canDoCivics`
(`Engine/CvPlayer.cpp:8449`); `canFoundReligion` (`Engine/CvPlayer.cpp:10105`); `canCreate` (`Engine/CvCity.cpp:3034`,
`Engine/CvPlayer.cpp:6804`); `canFound` (`Engine/CvPlayer.cpp:6199`). Caches: `CvPlayer` `m_bCanConstruct*` (4 arrays,
`CvPlayer.h:2404-2407`, cleared `:28056`/`:28076`); `CvCity` canTrain cache (`m_canTrainCacheUnits`/`…Populated`,
`CvCity.h:2014`/`:2017`, gated `#ifdef CAN_TRAIN_CACHING`) + the `m_bCanConstruct` map (`CvCity.h:2029`, flush
`CvCity.cpp:2474`); the `VALIDATE_BUILDING_CACHE_CONSISTENCY` shadow check (`AI/CvCityAI.cpp:4829`/`:5158`).
`CvCityAI::CalculateAllBuildingValues` PreLoop (`AI/CvCityAI.cpp:12839`, PreLoop profile `:12962`, body runs past
`:14278`). `ConstructRequirement` + the **#195 enabler index** (**partly KEPT** — it *is* the `enables` generation
index seed: `Engine/ConstructRequirement.h`, built in `Defines/CvGlobals.cpp`). `setHasBuilding` extension/replace
chain-walk (`Engine/CvCity.cpp:14563`; extensions `:14609-14622`, replace `:14625-14654`).

**Modifier (→ §1.2 machine):** the CvCity yield/commerce/health/happiness/defense/maintenance accumulator arrays
(`Engine/CvCity.h`: `m_aiBaseYieldRate:1747`, `m_aiExtraYield:1759`, `m_aiYieldRateModifier:1762`,
`m_aiCommerceRate:1769`, `m_aiCommerceRateModifier:1782`) + `getBaseCommerceRateFromBuilding100` (`Engine/CvCity.cpp:12357`);
`processBuilding` (`Engine/CvCity.cpp:4525`, body ends ~`:5145`) / `processSpecialist` (`:5157`) / `processBonus`
(`:4421`); `CvPlayer::setCivics` (`Engine/CvPlayer.cpp:14289`) / `processTrait` (`:28449`); `CvTeam::processTech`
(`Engine/CvTeam.cpp:5902`) / `CvPlayer::processTech` (`Engine/CvPlayer.cpp:30909`); the `CvUnit` `changeExtra*` stack
(**exactly 91 setters**, `Engine/CvUnit.cpp:11386…30937`). *(The old map's `processCorporation` and `getBuildingYield`
do not exist — removed/never-present; the real building-yield getters are `getBuildingYieldModifier`/`…Change`.)*
*(Active-rework signal: `m_aiBuildingBonusVicinityYield100` (`CvCity.h:1749`) is a newly-added recompute-only,
non-serialized field — the vicinity-build-order fix; this accumulator block is being reworked, not frozen.)*

**Tally (→ §1.1 machine):** the cross-city `getNum*` prereq count **loops** inside the gate functions
(`CvPlayer::getBuildingPrereqBuilding` `:7306`; call sites `CvPlayer.cpp:6718`/`:6783`, `CvCity.cpp:1504`) + the
demographics/score scans (`CvPlayer::calculateScore` `:4417`, `getPopScore`/`getLandScore`/`getWondersScore`/
`getTechScore` `:11506`/`:11546`/`:11587`/`:11610`) collapse to reads of the one tally accessor. ⚠ The object-owned
count itself (`getBuildingCount` = `m_paiBuildingCount` `:7329`, `getUnitCount`, `CvTeam` techs) **STAYS** — the object
"cares about itself", and the tally *reads* it (it is the source, not demolition fodder). Only the scattered re-scan
**loops** are subsumed, not the aggregates they read.

---

## 5. HARD BOUNDARIES (cannot rewire)

- **EXE ABI** (§3) — the closed Firaxis `.exe` binds the DllExport surface + base classes.
- **Save format** — name-tagged (`CvTaggedSaveFormatWrapper.h:14-17`); intentional breaks → `@SAVEBREAK`.
  Derived/accumulator state serializes nothing (recomputed on load). **⛔ "Removing a serialized member is soft"
  is NOT reliably true (proven 2026-07-02):** deleting the CvTeam capability counters' `WRAPPER_READ` entries
  DESYNCED loading a save that carried the tags — the whole downstream state read wrong (empty tech lists,
  gutted cities). Root cause (grounded): `Expect()` treats any mismatch as "code ahead of stream" and never
  consumes the unexpected element (`CvTaggedSaveFormatWrapper.cpp:3830`), so a stale tag stalls every later read
  in the object at its default. Retire serialized members in TWO STAGES: drop the `WRAPPER_WRITE` + replace the
  read with a **named `WRAPPER_SKIP_ELEMENT(wrapper, "ClassName", memberName, SAVE_VALUE_ANY)`** (drains the tag
  on old saves, no-ops on new) + ledger the field in `savemigration.txt`; flush skips + ledger at the next
  save-compat break. Full mechanism: [engine.md](../../reference/engine.md) §Save/load.
- **OOS / lockstep determinism** — integer math only; synced Soren RNG. `readJson` **will convert** readable→`int×100`
  ONCE at load (deterministic). No runtime float. Canonical: [DEC-fixedpoint-x100](../../architecture/decisions.md#dec-fixedpoint-x100).
- **Toolchain** — C++03, 32-bit/x86, vendored VC7.1, Python 2.4, Boost 1.32/1.55, raw Win32 (no `std::thread`/C++11+).
  (`Sources/fbuild.bff:7/47/48/325`.)

---

## 6. THE TRAIT SIMPLE/COMPLEX SPLIT — engine fix (owner ruling 2026-06-29)

Legacy still resolves traits via the `CvInfoReplacements` runtime hack and **must** be moved to the option-selected
curated-set model as part of this migration. The current mechanism (mapped 2026-06-29):

- **The swap:** `CvInfoReplacements<T>::updateReplacements` overwrites `aInfos[id]` with the first replacement whose
  `BoolExpr` condition holds (`Infos/CvInfoReplacements.h:191` — backup `:190`, restore `:173`; condition `m_pCondition`
  `:27`, evaluated `:63`). The driver `cvInternalGlobals::updateReplacements()` (`Defines/CvGlobals.cpp:753`) fans out
  to 36 per-type tables incl. **`m_TraitInfoReplacements.updateReplacements(m_paTraitInfo)`** (`:759`). Re-run on state
  changes at `CvGame.cpp:238`/`:1124`/`:11572`. XML parse of `ReplacementID`/`ReplacementCondition` at
  `Infrastructure/CvXMLLoadUtilitySet.cpp:1568-1584` (`addReplacement` `:1604`).
- **The target.** The curator already emits two self-complete folders (`traits/simple/` + `traits/complex/`); the
  engine should **load the one active set** chosen by `GAMEOPTION_LEADER_COMPLEX_TRAITS` and inject it behind a trait
  query-surface at the composition root (poor-man's DI, §2b) — the exact "isolate systems → both implement one contract
  → switch at the root" worked example in `patterns.md`. This is the StoneBase `ModifierMath.ActiveTraitSet` model
  ported into the engine. The `CvInfoReplacements` trait swap (and its per-turn re-run for traits) is then demolition
  fodder. (`modifier.md` §4 owns the curator-side CRAZY→sensible translation + the CLEAN gates the cascade applies;
  this §6 owns the engine-side replacement-hack removal.)

---

## 7. NEXT

1. **`readJson` (typed-condition port)** ✅ **PARSE + MAP DONE** — full json.md coverage; maps each entity's JSON to a
   **`CvJsonInfo`** held in the entity's per-type **`InfoRepo<CvXInfo>`** (`Repos/InfoRepo.h`) — the side-table is
   **RETIRED** (§3 home). ✅ **Conditionals are the StoneBase `Condition` port (2026-06-30)** — `cascadeParseCondition`
   parses each `requires`/`enabled`/`disabled` into a typed **`CvCascadeCondition`** tree (NOT `BoolExpr`), evaluated by
   `cascadeEvalCondition` against the live engine in the shadow gates; the `BoolExpr` translator + `CvCascadeCountExpr`
   are DELETED (the count folded into the evaluator's tally read). ✅ **readJson is STATIC-DATA-ONLY** — it no longer
   includes `CvCity`/`CvPlayer`/the tally (§3 boundary). ✅ **readJson now maps at LOAD** (the end of `LoadPostMenuGlobals` — the LAST XML stage; moved off `doTurn`, then off
   `onFinalInitialized`→`doPostLoadCaching`, and finally 2026-07-02 off `doPostLoadCaching` too: that fires PRE-MENU,
   before processes/votes/espionage/spawns register, so their FK edges silently dropped — the canMaintain
   empty-frontier bug; the live `[MODIFIER/repo]` census now re-emits `unresolvedFks` so a map-before-registration
   hole can never hide in the dark load burst again) — loading a save verifies it (validation.md cadence). ✅ The synthetic **`TECH_GAME_START`** root (the XML-less no-tech-prereq node — a
   non-resolver with no engine id) is homed in **`cascadeStartNode()`** (off the InfoRepo) so its `enables` survive the
   map (json.md §5 / readjson.md §5.1). ⛔ **NOT yet mapped: the classification blocks** (`skills`/`tags`/`state`,
   json.md §8 — `capabilities` **IS** mapped, see the enabler item below) — currently recognized + skipped. See the
   classification-wiring item below. ⏳ **NEXT (refinement) + THE SHADOW-PERF HUNT (owner 2026-07-02 "chase the needless repeat calcs BEFORE parity").**
   The [MODIFIER/perf] census (counters + PerfAccumTimer stopwatches, flushed per turn) drove this so far:
   yieldRate100 was 444 calls × ~164ms ≈ 73s/turn; the turn-scoped memos (facts: 91% hit; rate: halved to ~37s)
   bought back the REPEAT calls. The residual is the PER-CALL ~164ms, and the sub-costs measured so far bound it:
   percentStack ≈ 6ms and commerceRate ≈ 7ms per call, so the bulk sits in the BASE packages — **`basePlot` was CONVICTED and fixed**: it
   walked ALL ~5202 buildings PER WORKED PLOT (×3 keyed-deposit walks) — the per-channel building-CANDIDATE cache
   (which buildings carry any deposit for a channel is static readJson data) cut the per-call cost ~164ms → ~50ms
   (turn total 73s → ~12s, with the memos). **ANTI-MEMO-SKEW**: the doTurn shadow clears the turn memos at its top
   (`YieldRate::memoClear` + `EnablerKernel::factsMemoClear`) — memo values frozen from early-turn calls vs
   end-of-turn legacy read as false divergences until it did. Next perf levers if wanted: the per-plot civic/trait
   loops + the substrate walks (same candidate-cache pattern). The deposit-vector index (§3) stays a refinement
   but is demoted as the lever. ⚠ All memos are SHADOW-PHASE-ONLY (stale on mid-turn building changes) —
   event-invalidate before any consumer cut.
2. **Tally** ✅ **DONE — reworked to a READ-ONLY accessor (owner ruling 2026-06-30)** (buildings + units). It READS the
   object-owned counts (`CvPlayer::getBuildingCount`/`getUnitCount`) and rolls UP the spine (empire/team/world) — no
   store, no `IEventConsumer`, no `rebuild`/`onEvent`/`shadowDiff` (a count shadow was tautological — the duplicate-store
   model is retired; `tally.md`). The standardized DOMAIN event emitters stay (observability/invalidation/offline). Other
   count domains read from their object owner when added (`tally.md` §5).
3. **Modifier** — build plan [`modifier-machine.md`](modifier-machine.md). The readJson→`CvJsonInfo` (InfoRepo) mapping is DONE;
   the **percent stack** (increment 1) is in + attributed (building tier bit-exact). **Strategy (owner ruling 2026-06-30):
   port the WHOLE StoneBase `Calc` in, THEN compare the in-DLL shadow vs StoneBase's parity-proven results** (port
   fidelity) — do NOT chase per-increment parity. **Parity = full ATTRIBUTION + a showable diff, NOT bit-exact**
   (validation.md; the specialist-bucket move makes bit-exact impossible; StoneBase proved attribution-parity is reachable).
3a. **★ WIRE THE NEW CLASSIFICATION + POLICY BLOCKS so the engine consumes them exactly as it uses the legacy flags
   today (owner ruling 2026-06-30 — SUPER IMPORTANT: these easily get LOST, and a wrong/missing one DETERIORATES the
   game experience).** Map `skills` / `tags` / `capabilities` (json.md §8) + `policies` (§9, civic law toggles) onto the
   game object as the new arrays, and make each engine SYSTEM read the new array the same way it reads the legacy
   per-flag: a unit's `skills.blitz` → multiple-attacks (as the legacy blitz); empire `capabilities` → the team ability;
   `policies.noForeignTrade` → the trade-route engine; unit `tags` → the `IS_<TAG>` accounting. **empire capabilities +
   unit skills especially.** The classification blocks are currently parsed-but-skipped (not mapped) — this is the wiring.
4. **Enabler** (generate-then-gate, on the validated tally) + **grants**.
   ✅ **StoneBase CascadingEnabler PORTED for tech/building/unit (2026-06-30)** — `en_techAvailable` (TechCascade),
   `en_buildingBuildable` (BuildingCascade + the AugmentState facts, which the live evaluator reads directly via
   `hasVicinityBonus`/`isGovernmentCenter`), and `en_unitTrainable` (UnitCascade: GATE-availability → GENERATE-frontier
   minus `replaces` → GATE not-dormant via the reachable upgrade closure) **REPLACE the first-cut enables-frontier for
   their domains.** Per StoneBase: the FRONTIER is the **WHOLE domain** (all techs / all buildings / all units) gated by
   `requires`, NOT an enables-frontier — the engine's canConstruct/canTrain have none, and an enables-frontier
   under-offers a no-enabler entity (PALACE). ✅ **SELF-CONTAINMENT (2026-07-01):** the never-buildable exclude reads the
   cascade's OWN JSON flags — building `identity.notConstructible` (`CvJsonBuildingInfo::notConstructible`) and unit
   `identity.spawnOnly` (`CvJsonUnitInfo::spawnOnly`) — matching StoneBase (`NotConstructible` set / `u.SpawnOnly`), NOT the
   engine `productionCost<0`/`isAutoBuild()` markers (DEC-calc-zero-ride-in; the cascade must stand after legacy is cut).
   VALUE-EQUIVALENT on current data (spot-checked): the curator sets `identity.notConstructible` when `iCost==-1`
   (== `productionCost<0`), and **all 181 `autoBuild:true` buildings carry `notConstructible:true`** (json §7
   `autoBuild ⊂ notConstructible`), so `productionCost<0 || isAutoBuild()` == `notConstructible`; **525 units** carry
   `identity.spawnOnly:true` (== `productionCost<0`). (Also fixed the `CvJsonUnitInfo::mapFrom` parse: `spawnOnly` reads
   from `identity`, `unlimitedException` from `skills` — both were mis-read at top-level and silently found nothing.)
   Dormant triggers are extracted into `CvJsonInfo.dormantTriggers` (the `requires.{operate|build}.dormant` key the
   condition parser drops). The tech oracle moved `canEverResearch`→**`canResearch`** (the all-techs+requires set is
   "researchable now"). ✅ **PORTED (2026-06-30/07-01):** prereq-AMOUNT scaling (`ScaledPrereq`, verbatim), the
   multi-queue exclude (this city's `ORDER_CONSTRUCT` queue), the civic-special-building waiver
   (`enables.specialBuildingsWaived`) + the obsolete-by-held-tech prereq waiver (`AugmentState`), and the SpecialBuilding
   group cap at **PLAYER scope**. ⏳ **SpecialBuilding team/world group caps — BLOCKED (2026-07-01):** needs a
   group→members index + the group's `allowed` available to the cascade (StoneBase `SpecialBuildingGroup` team+world caps),
   but `SPECIALBUILDING_` is not in readJson's `RJ_REPO_TYPES` (no `InfoRepo<CvSpecialBuildingInfo>` / group `allowed` /
   members index parsed). **DATA-BENIGN:** all 7 group special-buildings are `allowed.empire:1` only — no team/world group
   cap exists, so the player-scope check is complete for current data. ⏳ **CultureLevel category caps — correctly
   DEFERRED:** a post-port spec feature StoneBase itself lacks (the wonder-category `allowed` keys, e.g. `worldWonders`);
   adding it now would DIVERGE from StoneBase. ⏳ **`MAPCATEGORY_` gate — DEFERRED:** XML-only, not yet in the JSON data
   (in-flight per json §3.5). **Builds stay the per-plot `canBuild` gate** (it subsumes BuildCascade's unlock set); civics/projects/
   processes/promotions/hurries keep the generic enables-frontier `en_gateSet` (no StoneBase reference).
   ✅ **FRONTIER GATES BUILT — the 6 same-shape
   gates** (`Cascade/CvCascadeEnabler.{h,cpp}`): **city-scope** `canConstruct`/`canTrain`/`canCreate`/`canMaintain`
   + **player-scope** `canResearch`/`canDoCivics`. ONE GENERATE→GATE primitive over a **bucket-keyed** `enables`
   collection: GENERATE `CAN GET` from the InfoRepo `enables.<bucket>` over HAVE (team techs + adopted civics [+ the
   city's buildings for city-scope] **+ the universal `TECH_GAME_START` start-node `cascadeStartNode`, seeded for every
   player — the no-tech-prereq root, since every civ grants it via `grants.techs`; closes the start-tech `canResearch`
   diffs** and is the home for the no-prereq starting set across buckets) minus obsoletes/replaces/disables (+ the
   target-side `obsoletedBy.techs` prune), then GATE by `requires` (the typed-condition evaluator vs the city/player/unit/plot live ctx) + `allowed`
   (tally cap). Emits the available set per gate, shadowed vs the live engine (`[ENABLER/shadow]` per-gate
   diverging/checked + **per-gate-capped** `[ENABLER/diff]` samples, at doTurn). Kept structurally two-pass (NOT a
   per-entity output-match — [DEC-stonebase-follows-spec]); names mirror the existing engine gates. ✅ **+
   `canAcquirePromotion`** — the per-UNIT shape (HAVE = the unit's held promotions + team techs + unitcombat → GENERATE
   enables.promotions → GATE `requires` vs the unit game object; shadowed vs `isPromotionValid`). So **7 gates built**.
   ✅ **+ `canBuild`(worker)** — the PLOT-scope shape: GENERATE the owner's `enables.builds` (techs) → GATE the build's
   `requires` vs the **plot** game object (terrain predicates), over sampled owned non-city plots; shadow vs
   `CvPlot::canBuild`. So **8 gates built**. ✅ **+ 2 more PLAYER-scope gates (so 10 built), both clean parity first try:**
   **`canHurry`** (GENERATE `enables.hurries` over HAVE — mostly civics; the player-level "is the hurry type enabled" gate
   that lights the two Python hurry buttons / the AI check; the city-level gold/slavery AMOUNT is runtime; shadow vs
   `CvPlayer::canHurry`=`getHurryCount>0`); and **`canFoundReligion`** — a **player-wide STATE predicate** (≥1 city, not
   NPC, not first-3-turns, the `RELIGION_LIMITED` holy-city rule), reproduced from raw state (not a JSON frontier) and
   shadowed vs the engine. ⏳ **`canAddHeritage` is a SEPARATE move** (traced 2026-06-30): `CvPlayer::canAddHeritage` is only
   a **permissive prereq-check** — the tech-rooted 22 heritages invert cleanly (`TECH_TAXONOMY`/`TECH_ORAL_TRADITION` →
   `enables.heritages`, verified), but the ~91 no-prereq folklore heritages are *really* gated by the subdued-animal
   `MISSION_HERITAGE` / `CvOutcome` (a misnomer — the actual MISSION system), unmodeled. Done with the outcome/mission system, not the frontier. ✅ The `capabilities` block is **MAPPED + QUERYABLE**:
   `CvJsonInfo.capabilities` (per-tech grant names; `[READJSON/cap]`) + `en_empireHasCapability(team, cap)` (union over
   the team's held techs). **Verified by a clean CAPABILITY shadow** — `canFoundOnPeaks` (granted by **TECH_ALGEBRA**, a
   tech *capability* — corrected: NOT a policy / external source) vs the engine `CvTeam::isCanFoundOnPeaks` flag
   (`[ENABLER/shadow] cap:canFoundOnPeaks`). ⏳ **`canFound` DEFERRED (owner ruling 2026-06-30)** — its capability half is
   done (above); the remainder is the founding **RULE** (nearby-city distance, area, water/peak validity), whose engine
   logic is *"a bit all over the place"* + not cleanly spec'd — a look-at-later gate, not modelled now.
   (`found`/`foundCoast`/`foundFreshWater` are unit `skills`, a separate axis.)
   **⏳ PARITY-CHASE notes (2026-07-02 shadow run):** ✅ canCreate/canResearch/canDoCivics/canHurry/canFoundReligion
   clean (the canCreate fix = project `allowed:{world/team}` caps, a Gate-1 data find — were parked in identity).
   Three OPEN finds the instruments cannot re-derive: **(a)** the enabler diff's `who=` renders BLANK for the
   city-scope gates (an emit bug — fix before chasing per-city diffs); **(b)** ~~the spawnOnly model question~~ RESOLVED (owner pushback 2026-07-02, verified): spawnOnly was cleared correctly (== productionCost==-1); the divergence was the SHADOW ORACLE passing bIgnoreCost=true, which disables legacy's `!bIgnoreCost && productionCost==-1` gate — the exact spawnOnly semantic. Oracle flags fixed to false on canTrain + canConstruct — after which BOTH went to diverging=0 (41,616 + 16,584 checks): the flagship gates are AT PARITY. Remaining enabler chases: canMaintain 40 (the processes trace), canAcquirePromotion, canBuild; **(c)** canMaintain refuses all 5 processes for REAL civs holding the
   enabling techs (NPC pollution ruled out — counts identical after the NPC skip) — OWNER SEMANTIC (2026-07-02): processes convert hammers to commerce-type yields and only the LATEST version of each family is choosable -- legacy enforces only-latest IN CODE (no data-side supersession edges exist: process JSONs verified edge-free; currency->WEALTH, trade->WEALTH_LESSER enables verified clean). TWO chases: (1) the only-latest rule needs a data model (the ReplacementBuildings->dormant pattern on processes, or the gate mirrors legacy) + (2) trace where GENERATE loses
   `enables.processes` (data verified present on 12 techs) or the gate drops them; NB PROCESS_IDLE has NO tech
   enabler, so the generic enables-frontier structurally under-offers it (the PALACE lesson — processes may need
   the whole-domain frontier like the ported cascades). **(d)** verify the multi-queue exclude matches the legacy oracle flag semantics on canConstruct (owner 2026-07-02: queue-driven variation otherwise reads as noise). **(e)** the city-scope ctx.plot fix (2026-07-02) cleared the coastal/freshwater refusal class; the canConstruct/canTrain bulk remaining is per-type chase work. **(f)** THE FACTS FIXPOINT (2026-07-02): the modifier percent-stack divergence attributed to `computeCityBuildingFacts` evaluating operate conditions against an incomplete vicinity supply — the active set and the `provides` supply are MUTUALLY dependent (a per-building dry-calc proved data+eval exact on the engine-active set), so the facts now iterate a bounded LEAST fixpoint (empty supply → grow until stable). StoneBase never faced this: its AugmentState reads the ENGINE dormant set (the "StoneBase cheated on dormancy" owner call) — the self-contained cascade must solve the fixpoint itself. **(f2)** ⚖ STALE ENGINE EVENT-STATE — ACCEPTED, MONITOR (owner ruling 2026-07-02): the surviving stack/dorm
   disagreements attributed to the engine's event-driven building-disabled state violating its own rules (corp
   outlets enabled with the corp absent; bonus-loss dorms never re-enabled on trade re-gain) are a KNOWN
   consequence — *"we dropped some event states, because we had to fix caching (event yields were only written
   to the cache)"*. A derived calc cannot reproduce state dependent on unreachable event history; the cascade's
   rule-derived verdict is the correct one and the cutover REPAIRS these leaks. Disposition: accept the
   divergence class, monitor it (the [MODIFIER/dorm] per-city attribution lines name each disagreeing building);
   never freeze the stale verdicts into the cascade. (Per-building dry-calcs proved data+eval EXACT on the
   engine-active set — the residue is legacy state, not cascade logic.)
   **(g)** SHADOW SCOPING (2026-07-02): canBuild diffs UNLOCK-half vs UNLOCK-half (the oracle mirrors CvPlayer::canBuild's disabled/obsoleteTech/techPrereq block; the plot-validity half survives cutover — diffing vs full CvPlot::canBuild compared two different questions), and canAcquirePromotion rides `isPromotionValid(pr, bFree=true)` (the bespoke unit-state half — qualified/disqualified unitcombats, game options, unit-state caps) on BOTH sides, isolating the diff to the tech/enables frontier the cascade owns; the qualified-combat DATA is parked in `identity.unitCombats` pending its Gate-3 model. Tails attributed: promotions — the no-qualified-CC "event-injection-only" clause (`bValid=bFree`) is mirrored in the shadow (with the free-promotion carve-out); the PALACE lesson applied (a no-enabler promo is always-unlocked — the enables-frontier under-offered COMBAT1-5). canBuild — the residual is builds whose `requires.build` carry PLOT-scope atoms (BONUS_RAPA_NUI-on-plot class): the cascade correctly refuses on the sampled plot while the unlock-half oracle doesn't ask the plot question — both sides right at their own scope, attributed shadow-scoping noise on a non-cut gate.
   ✅ **PARITY REACHED — the enabler plane (2026-07-02).** The Gate-2 shadow campaign drove EVERY §4-demolition
   gate to diverging=0, stable across consecutive turns: canConstruct / canTrain / canCreate / canMaintain /
   canResearch / canDoCivics / canHurry / canFoundReligion / cap:canFoundOnPeaks — plus canAcquirePromotion
   (whole-domain via the PALACE whitelist + the event-only mirror). The one non-zero gate is canBuild's small
   attributed residual (the plot-scope-atoms scoping class, note (g)) on a NON-cut gate. The modifier percent
   stack's residual decomposes ENTIRELY into the accepted classes — stale dropped-event state (note (f2)) + the
   deferred PESTS property bands — with per-building dry-calcs proving data+eval exact on the engine-active set;
   the getter/rate residuals ride the same accepted classes (incl. the event-granted per-building commerce, the
   documented honest divergence). Numbers live in the run (DEC-no-parity-results-in-docs); the classes and their
   dispositions are the durable record above.
   **Build EARLY, a CO-REQUISITE with the
   modifier (owner ruling 2026-06-30), not a later step.** Without the enabler the cascade does not know **what is
   ACTIVE** — which bonuses are connected/available, which buildings are non-dormant — and the modifier's conditions
   (`enabled:{HAS_BONUS}`, `connection:vicinity`, dormancy) depend on exactly that. **The modifier shadow must read the
   ENABLER's active state, NOT the live engine (owner ruling 2026-06-30)** — so the cascade is self-contained (it must
   keep working after the legacy state is cut); the enabler's active state is **independently shadowed vs the engine to
   prove they are EQUAL** (StoneBase proved this is achievable). (The current percent-stack reads the live engine as an
   INTERIM shim until the enabler exists — modifier-machine §2.) So sequence the enabler alongside the modifier.
5. **The trait simple/complex engine fix (§6)** — retire the `CvInfoReplacements` trait swap for option-selected
   injection — sequenced with the modifier/enabler work (it changes which trait values both read).
6. **The atomic cutover** — `readJson` replaces `readXml`; the demolished machinery (§4) is deleted in one landing.

Each machine carries the alignment fixes it touches as it is wired. Parity is proven offline by StoneBase and live by
the per-machine shadow before anything legacy is cut.
