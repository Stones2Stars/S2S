# Cascade engine (#430) — implementation plan

> ## ⛔ RESUMING AFTER A CONTEXT COMPACTION? RE-READ EVERYTHING FIRST.
>
> If your context was just compacted mid-session, do NOT act from the summary. Re-read the full spec set (the
> machine specs + `event-spine-spec.md` + this doc + `migration-renames.md` + `migration-entity-ranking.md`) and the
> live code in `Sources/Cascade/` before touching anything — compaction has poisoned context before; a stale line
> loses to a later owner ruling. (Owner 2026-06-17; this gate is being deliberately tested.)

**Status: implementation IN PROGRESS (see the status table below).** The DESIGN lives in the
machine specs — `enabler-cascade-spec.md` (v0.3) + `modifier-cascade-spec.md` (v3) + `tally-cascade-spec.md` (the count
machine, consolidated 2026-06-16) — one per machine, plus **`event-spine-spec.md`** (the front-door event system the tally /
`grants` / logging all consume — design session 2026-06-17). This is the IMPLEMENTATION roadmap — the runtime engine that
consumes the #428 JSON and replaces ~7–8k lines of scattered availability + modifier machinery. Read the specs first; this
doc is the build plan + the validated load/demolition map, not a re-derivation of the model.

---

## ⓘ IMPLEMENTATION STATUS — verified against `Sources/Cascade/` on 2026-06-19

> **The machine specs describe the TARGET shape; read as *status* they OVER-STATE completeness.** This table is the
> ground truth from an adversarial docs-vs-code sweep (2026-06-19) — trust it over any "DONE/built" phrasing in the
> design specs. (Owner session model: holes found are bucketed small=plugged / big=noted here + on the fix-now list.)

| Component | Status | Reality |
|---|---|---|
| **Substrate** (spine + accumulator) | ✅ **BUILT** | `CvEventSpine` + `CvScopedAccumulator`; logging + tally consumers registered (`cascadeRegisterConsumers`). |
| **Tally** | ⚠ **PARTIAL** (model decided) | **Player-leaf model ACCEPTED (owner 2026-06-19)** — the shipped per-player tally is the design now (tally-spec rewritten to match; CITY/PLOT read direct, widening to city-leaf later is a contained no-save-break change). Wired + load-bearing for **buildings + units ONLY** (2 of the ~6 count domains §7 needs; tech/civic/religion/bonus/project/specialist absent → atoms over those silently read 0 — the remaining build item). §9 save handling (rebuild-on-load) DONE, hooked at `onFinalInitialized`. |
| **Enabler** | ◐ **GATE built, GENERATOR inert** | `cascadeBuildable = requiresBuild ∧ requiresOperate ∧ allowed-cap` + `cascadeOperational` (dormancy) mature; allowed-cap enforced via tally. BUT the `enables`-family **CAN-GET generator is not built** (`cascadeTechReachable` dead, no frontier producer); **`disables` unparsed**; `notConstructible` parsed but never consumed. |
| **Modifier** | ◐ **PILOT BUILT + SHADOWED** (city-yields) | Increments 1–3 of [`modifier-cascade-shadow-spec.md`](../reference/cascade/shadow.md) DONE (`23245bcd2`/`3f1abc344`/`9536c0552` + increment-3 shadow): `CvModifierSlot` combine + the data-driven deposit-flow (`cascadeModifierCitySlot`/`cascadeModifierEffective`, parity-mode + swappable `cascadeModifierApply`/`ModifierCalcFlow`=legacy-flat-outside) for **food/production/commerce**, with sources wired incrementally (R-M1): city-scope **BUILDING** deposits; the base now includes **specialist** yield (legacy `getYieldRate100` parity); the player's active **CIVICS'** empire-scope deposits (empire→city roll-down). Plus the shadow surface (`cascadeModifierShadow` → per-turn `[MODSHADOW]`, `GET /diagnostic/modifierSweep` + `/diagnostic/modifier`, cause-tags + the Fine→Meltdown care scale). **Remaining sources** (the residual `missingDeposit`): TRAIT/TECH/BONUS/AREA/POWER/CAPITAL + building-empire deposits, civic sub-scopes, the clamp; **then** PLOT + other scopes; the commerce-split / health-happiness / defense / maintenance / unit-plane channels (§2.2); the multiplier-capability flip (Mode B). |
| **Grants** | ❌ **ABSENT** | No `grants` consumer registered (the spine diagram lists it; not built). |
| **readJson** | ◐ **gate-surface + PILOT modifier feed** | Parses `requires.build/operate`, `allowed`, `identity.{spawnOnly,notConstructible,autoBuild}` + four ad-hoc reverse-index scans (tech/group/replace/upgrade) AND the PILOT modifier families (`cascadeReadJsonModifiers`: food/production/commerce flat/percent at city scope, with `enabled`/`disabled`). Does **not** yet parse the other modifier scopes/channels or `grants`. Header marks it TEMPORARY. |

**Big remaining #430 build items** (the honest roadmap): the modifier machine BEYOND the city-yields pilot (PLOT + other
scopes, non-building sources, the §2.2 channel inventory, the Mode-B capability flip — the pilot increments 1–3 ARE built +
shadowed); the `grants` consumer; tally **domain coverage** (the player-leaf-vs-city-leaf model decision is RESOLVED
— player-leaf accepted 2026-06-19); the enabler **`enables`-family generator** + **`disables`** parsing. **Smaller gaps the
sweep found:** `ATOMDOMAIN_HERITAGE` implemented-but-undocumented (data-model §2.1); `workedBy`/`natureYield` predicates
specced-but-unparsed; `CvCascadeReadJson.cpp` still `#include "CvInfos.h"` (contradicts the retire-umbrella ruling — folded
into [`sources-structural-cleanup.md`](../plans/structural-cleanup.md) 1B).

---

## 1. THE ROOT SYSTEM — one substrate, three machines (owner 2026-06-16)

The entire new system roots in a shared **scope-accumulator substrate** with **three machines** on it. They are not three
tangles — they are three instantiations of one primitive, which is what makes the engine coherent.

### 0. Substrate — the scope spine + an additive accumulator + the EVENT SPINE (interface-bounded)

- **Scope spine:** `world → team → empire → area → city → plot{improvement|feature|terrain|route} → building | specialist | unit`.
- **Additive accumulator:** deposit → O(1) summed read, parameterized by what it sums. One primitive, three instances. The
  enabler additionally walks the SIDEWAYS/progression axis (tech tree, build chain — enabler-spec §9); modifiers are
  containment-only. (Implemented: `Sources/Cascade/CvScopedAccumulator`.)
- **Event spine — the dispatch FRONT DOOR (added 2026-06-17; full design: `event-spine-spec.md`).** `emit(KIND,type,payload)`
  → consumers read the kinds they care about. The tally, `grants`, and logging are all CONSUMERS of it; the spine sits IN
  FRONT OF the tally (the tally never reaches into game state — domain events come to it). So the build order below reads
  "tally" as *the first COUNTING machine*, but **the event spine + accumulator (the substrate) come before it**, and the
  tally is the spine's first authoritative consumer. (Implemented: `Sources/Cascade/CvEventSpine`.)

### 1. Tally — counts, roll UP  *(build FIRST; spec: `tally-cascade-spec.md`)*

Per-type had-counts, additive roll-up the spine. Serves three readers: `requires` count-thresholds (`min(BUILDING_X,12)`,
empire/team) + the **higher-scope HAS sets** (the empire/team/world HAS *is* the tally), the modifier's cross-city `per`
count-scaler, and demographics/AI/score (wanted regardless). First because the enabler depends on it (tally-spec §2/§7).

### 2. Modifier — magnitudes, deposit DOWN

Per `(family, member, unit, scope)` summed deposits; targets read O(1).
`effective = (base + Σflat) × (100 + Σpercent)/100 × Π(multiplier/100)` (modifier-spec §2). Replaces the CvCity yield/
commerce/health/happiness/defense/maintenance accumulators + the `process*` apply-loops + the unit extra-stat stack.

### 3. Enabler — availability, 2-pass

gather **HAS** (reads the tally for higher scopes) → generate **CAN GET** (the `enables`-family forward index) → gate
**`requires`** (enabler-spec §2). One shared frontier, read by UI greying + AI `doProduction`. Replaces the scattered
`can*` gates + the PreLoop + the caches.

### 4. readJson — the DATA INPUT *(owner 2026-06-16: "we HAVE to do this before we go anywhere")*

Extend `readJson` to implement **all** the new JSON-based logic — parse the full new vocabulary (`enables`/`obsoletes`/
`replaces`/`requires` trees, the modifier families `<family>.<scope>[.<member>].<unit>`, `grants`/`grants.repeatable`, the
predicate tokens, count atoms, scopes) into the runtime structures the three machines consume. It is the **data-feed
prerequisite**: the machines operate on the NEW vocabulary, not the old XML fields, so nothing computes until `readJson`
populates them. `readJson` is a pure CONSUMER — it loads the modder-authored JSON *shape* (defined by `data-model-spec.md`,
NOT by `readJson`) into in-memory structures of that same shape; the machines read those. One shape, consumed here, read by
the three. Runs as its own load path IN ADDITION to the XML load during shadow (§2/§3).

**Build order: readJson + substrate → tally → modifier → enabler.** `readJson` and the substrate come first (no machine
computes without parsed data on a scope spine); then the three machines, each interface-bounded, each deleting its slice of
the demolition map (§4) as it lands.

---

## 2. DEVELOPMENT STRATEGY — shadow + gated logging; the hard JSON switch is LAST (owner 2026-06-16)

The key to building a 7–8k-line atomic replacement safely: **don't build it blind in one shot.** Each machine runs **in
SHADOW** alongside the existing XML-driven machinery, behind **gated logging** (off in normal play, like every other
`[PERF]`/`log<Domain>AI` channel — the keep-instrumentation rule). Each accumulator computes in parallel and emits a
**new-vs-old comparison log**, so it is VALIDATED against the live game *before* anything is cut over.

- **Reconciles with the atomic-deliverable rule:** the DATA cutover stays atomic (one flip at the very end); the ENGINE is
  developed incrementally and shadow-validated first. This is engine development, NOT shipping data slices (which the rule
  forbids) — the new paths are gated instrumentation until the flip.
- **Shadow data source = the CURRENT (XML-loaded) info objects** (the same getters the old machinery reads) → an
  apples-to-apples new-vs-old compare. The JSON only becomes the source at the FINAL cutover, when `readJson`'s FRESH
  structures become the source the machines read. **(Corrected 2026-06-17: an earlier draft of this line said readJson
  "populates the same `CvInfoUtil` wrappers" — that CONTRADICTS §2b/§3, which is the governing ruling: build FRESH,
  actively AVOID `CvInfoUtil`/the old `read()` path. `readJson` is its own fresh reader; at cutover the fresh structures
  serve the EXE-bound accessor surface (§3), they do NOT repopulate `CvInfoUtil`.)**
- **Parity is NOT the success metric** (the cascade is *expected* to correct latent bugs). The shadow log surfaces
  DIVERGENCES for triage — bug-in-old vs bug-in-new — never byte-parity enforcement.
- **The readJson cleartext RENDER is the JSON-INTENT surface, cross-checked against the shadow LOGS (owner 2026-06-17).**
  `readjson.exe --render TYPE` (→ `Tools/ReadJson/testOutput/`) states in plain English what an entity's JSON *says* it does
  ("Versailles: allowed 1 world; builds faster with marble; +10 culture, doubled after 1000 turns"). That is the **intent**;
  the gated shadow comparison logs are what the engine **actually does**. Validation = the render conforms to the logged
  behavior — a third leg beside new-vs-old: *intent (render) ↔ new-engine behavior (shadow log) ↔ old-engine behavior*. A
  render/log divergence is a triage item (data wrong, or engine wrong). Keep renders for the entities under active wiring.
- **The hard switch (last step):** `readJson` replaces `readXml` at the load seam (§3) **and** the shadow accumulators
  become the sole source as the demolished machinery (§4) is deleted. One atomic landing.

---

## 2b. BUILD PRINCIPLES — from scratch, avoid existing code (owner 2026-06-16)

- **★ THE MOD FITS THE NEW STRUCTURE — NOT THE REVERSE (owner 2026-06-16).** The cascade data model + engine are
  authoritative; the mod's data/content is RESHAPED to fit them. We never constrain the new structure to match how the
  current mod happens to work. **Two guardrails keep this honest:** (a) **PRESERVE SAVES** where possible (the name-tagged
  soft save-format; `@SAVEBREAK` only when genuinely unavoidable); (b) **PRESERVE HOW THE GAME WORKS** — the *intended*
  gameplay / player experience stays recognizable. (a)+(b) bound the rewrite: structure & internals are free, the played
  game is not. *(This sharpens §2's "parity isn't the goal": we don't preserve the old IMPLEMENTATION's buggy numeric
  outputs — the cascade corrects those — but we DO preserve the intended design the game is supposed to express.)*
- **Build the 4 components FROM SCRATCH.** `readJson` + tally + modifier + enabler are written fresh, interface-bounded.
- **External libraries OK where they make sense** — use **`picojson`** (header-only) for JSON parsing, rather than bending
  the existing `CvXMLLoadUtility` XML machinery to JSON. **Already vendored and proven** under the VC7.1/C++03 toolchain — it
  backs the existing `CvHttpServer` — so it's a known-good dependency, not a risk. Reuse that.
- **ACTIVELY AVOID reusing existing engine code.** Do NOT thread the new path through `CvInfoUtil` / the old `read()` /
  `SetGlobalClassInfo`. The old machinery is demolition fodder (§4), not a foundation. The new path is its OWN, parallel and
  independent of the old during shadow (§2).
- **The derived-data REPOSITORY (`CvDerivedData` / `TLazy` / `dataRepository()`) is NOT built upon — and NOT removed yet
  (owner 2026-06-17).** The cascade's substrate ACCUMULATOR is *not* a repository tenant and borrows none of its
  version/dirty/staleness machinery (it is authoritative additive aggregation, the repository is advisory lazy memoization;
  `CvScopedAccumulator.h` carries this boundary). We iterated the repository through several structures (v1-on-AI-subclasses →
  v2-on-base-objects) — that prior tinkering must NOT poison the clean accumulator design. **BUT the skeleton stays in place
  during shadow:** its `init()`/`reset()` wiring is live in the game lifecycle, and we never remove live machinery before the
  atomic cutover (§2) — its removal is a §4 demolition item, deferred. The cascade (accumulators + tally + the enabler's
  generated frontier) subsumes the repository's *intended* purposes; the one genuinely-useful leftover idea — a build-list
  cache for UI responsiveness on selection-change — IS the enabler frontier, cacheable cleanly later if measured, not a
  reason to keep the empty skeleton past cutover.
- **The ONLY things kept/shared are the hard EXE boundary (§3)** — the type registry + the accessor surface the closed
  `.exe` binds. Everything else is fresh.
- **DELIBERATE REUSE EXCEPTION — `BoolExpr` for the conditionals (owner 2026-06-16).** The JSON conditionals
  (`requires`/`enabled`/`disabled`: `all`/`any`/`noneOf` over atoms + predicates) are **isomorphic to the engine's existing
  `BoolExpr`** (`And`/`Or`/`Not` over `Has`(GOM)/`Is`(tag)) — see `Sources/BoolExpr.{h,cpp}`. So the runtime `readJson`
  **translates a JSON conditional directly into a `BoolExpr` tree** and evaluates it against any in-game object; `BoolExpr`
  is the one existing piece we pull out and reuse (not rebuild). The isolated harness (`Tools/ReadJson/`) can't link
  `BoolExpr`, so it proves the same parse by rendering the conditional to **clear text** (the litmus test: "BUILDING_X
  requires (one of: …) AND NONE of (…)"). One conditional shape, two back-ends: `BoolExpr` in-game, text offline.
- **Sequence (owner): finish the specs, THEN prototype the 4 components.** The 3 machine specs are done (enabler/modifier/
  tally); the `readJson` runtime-data-model is the remaining design (§1.4) before prototyping.

## 3. EXE BOUNDARY — the only fixed constraint (verified 2026-06-16)

- **`readJson` is a FRESH reader** (picojson → fresh runtime structures), NOT a reuse of `CvInfoUtil`/`CvXMLLoadUtility`/the
  old `read()` path. It is its own load path, run IN ADDITION to the XML load during shadow (§2); at cutover the XML path
  (`SetGlobalClassInfo` → `read()`, CvXMLLoadUtilitySet.cpp:1588) is deleted.
- **The shared/kept pieces (EXE-bound, NOT "existing code to avoid"):** the **type registry** `GC.getInfoTypeForString` /
  `setInfoTypeFromString` (`m_infosMap`, name↔index) — `readJson` uses it for FK resolution because the EXE binds the same
  indices; and the **EXE-bound accessor surface**: `CvInfoBase` DllExport getters (`getType`/`getTextKeyWide`/`getDescription`/
  `getText`/`getHelp`), the ~89 `getNum*Infos()`/`get*Info()` pairs, a few art getters (`getArtInfo`, …). `read()` is NOT
  DllExport. During shadow the OLD objects still serve the EXE; at cutover the fresh structures must serve that surface (or it
  is reworked) — a cutover detail, not a shadow one. Everything outside this boundary is freely built fresh.

---

## 4. DEMOLITION MAP — what the engine deletes/rewires (verified 2026-06-16, ~7–8k lines)

**Enabler (→ §1.3 machine):** `CvCity::canConstruct`/`Internal` (CvCity.cpp:2470-3005) + `CvPlayer` (6509-6798);
`canTrain` (CvCity 2162-2465, CvPlayer 6370-6506); `canEverResearch` (CvPlayer 8258, CvGame 11310); `canDoCivics` (8447);
`canFoundReligion` (10103); `canCreate` (CvCity 3008, CvPlayer 6800); `canFound` (6195). Caches: CvPlayer `m_bCanConstruct*`
arrays, CvCity canTrain cache (+`VALIDATE_*` shadow checks). `CvCityAI::CalculateAllBuildingValues` PreLoop
(CvCityAI.cpp:12688-14187, ~1500). `ConstructRequirement` + the #195 enabler index (**partly KEPT** — it *is* the `enables`
generation index). `setHasBuilding` extension/replace chain-walk (CvCity.cpp:14386-14479).

**Modifier (→ §1.2 machine):** CvCity yield/commerce/health/happiness/defense/maintenance accumulators +
`getBaseCommerceRateFromBuilding100`/`getBuildingYield`; `processBuilding` (4499-5116) / `processSpecialist` (5129) /
`processBonus` (4395) / `processCorporation`; CvPlayer `setCivics` (14279) / `processTrait` (28407); CvTeam/CvPlayer
`processTech` (5929/30867); CvUnit `changeExtra*` stack (**91 setters**, 11385-30923 — spec's "~200" was 2× over).

**Tally (→ §1.1 machine):** the cross-city count loops inside the gate functions (the `getNum*` prereq scans) +
demographics/score scans become reads of the one tally.

---

## 5. HARD BOUNDARIES (cannot rewire)

- **EXE ABI** (§3) — the closed Firaxis `.exe` binds the DllExport surface + base classes.
- **Save format** — name-tagged (`CvTaggedSaveFormatWrapper`); removing a serialized member is soft; intentional breaks →
  `@SAVEBREAK`. Derived/accumulator state serializes nothing (recomputed on load).
- **OOS / lockstep determinism** — integer math only; synced Soren RNG. `readJson` converts readable→`int×100` ONCE at load
  (deterministic; #432 owns de-scaling). No runtime float introduced. Canonical: [DEC-fixedpoint-x100](../architecture/decisions.md#dec-fixedpoint-x100).
- **Toolchain** — C++03, 32-bit/x86, vendored VC7.1, Python 2.4, Boost 1.32/1.55, raw Win32 (no `std::thread`/C++11+).

---

## 6. NEXT

Build **substrate + tally** first (interface-bounded), running in SHADOW with a gated comparison log against the current
cross-city count consumers (the `getNum*` prereq scans + demographics). Validate, then **modifier**, then **enabler**. Each
machine carries the Phase-F alignment fixes it touches (ranking "Phase F", now light/iterative) as it's wired.
