# Cascade engine (#430) — implementation plan

**Status: design-complete, implementation starting (owner architecture session 2026-06-16).** The DESIGN lives in THREE
specs — `enabler-cascade-spec.md` (v0.3) + `modifier-cascade-spec.md` (v3) + `tally-cascade-spec.md` (the count machine,
consolidated 2026-06-16) — one per machine. This is the IMPLEMENTATION roadmap — the runtime engine that consumes the #428
JSON and replaces ~7–8k lines of scattered availability + modifier machinery. Read the three specs first; this doc is the
build plan + the validated load/demolition map, not a re-derivation of the model.

---

## 1. THE ROOT SYSTEM — one substrate, three machines (owner 2026-06-16)

The entire new system roots in a shared **scope-accumulator substrate** with **three machines** on it. They are not three
tangles — they are three instantiations of one primitive, which is what makes the engine coherent.

### 0. Substrate — the scope spine + an additive accumulator (interface-bounded)
- **Scope spine:** `world → team → empire → area → city → plot{improvement|feature|terrain|route} → building | specialist | unit`.
- **Additive accumulator:** deposit → O(1) summed read, parameterized by what it sums. One primitive, three instances. The
  enabler additionally walks the SIDEWAYS/progression axis (tech tree, build chain — enabler-spec §9); modifiers are
  containment-only.

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
populates them. Co-designed with the machines' input model (what `readJson` produces == what tally/modifier/enabler read).
Plugs into the load seam at the single choke point (§3); during shadow it runs IN ADDITION to the XML load (§2).

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
  apples-to-apples new-vs-old compare. The JSON only becomes the source at the FINAL cutover, when `readJson` populates the
  same `CvInfoUtil` wrappers (format-agnostic — §3).
- **Parity is NOT the success metric** (the cascade is *expected* to correct latent bugs). The shadow log surfaces
  DIVERGENCES for triage — bug-in-old vs bug-in-new — never byte-parity enforcement.
- **The hard switch (last step):** `readJson` replaces `readXml` at the load seam (§3) **and** the shadow accumulators
  become the sole source as the demolished machinery (§4) is deleted. One atomic landing.

---

## 3. LOAD SEAM + EXE BOUNDARY — the fixed constraint (verified 2026-06-16)

- **Single choke point:** `CvXMLLoadUtilitySet::SetGlobalClassInfo` → `pClassInfo->read(this)` (CvXMLLoadUtilitySet.cpp:1588).
  `readJson` plugs in HERE — a `CvJsonLoadUtility` parallel to `CvXMLLoadUtility`, or a `readJson` path populating the **same
  `CvInfoUtil` field wrappers** (the wrappers are format-agnostic; only the reader changes).
- **Type resolution (reused as-is):** `GC.getInfoTypeForString` / `setInfoTypeFromString` (`m_infosMap`), then the post-parse
  `GC.linkAllInfos()` (CvInfoUtil deferred-FK) + `GC.resolveDelayedResolution()` (legacy `SetOptionalVectorWithDelayedResolution`)
  passes (CvXMLLoadUtilitySet.cpp:879/882). `readJson` resolves `enables`/`requires` FK refs through the same mechanism.
- **EXE-bound surface (MUST preserve — the only hard data constraint):** `CvInfoBase` DllExport getters
  (`getType`/`getTextKeyWide`/`getDescription`/`getText`/`getHelp`), the ~89 `getNum*Infos()`/`get*Info()` accessor pairs +
  `getInfoTypeForString`, and a few art getters (`getArtInfo`, …). `read()` is NOT DllExport. Everything else is internal →
  freely re-architected.

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
  (deterministic; #432 owns de-scaling). No runtime float introduced.
- **Toolchain** — C++03, 32-bit/x86, vendored VC7.1, Python 2.4, Boost 1.32/1.55, raw Win32 (no `std::thread`/C++11+).

---

## 6. NEXT
Build **substrate + tally** first (interface-bounded), running in SHADOW with a gated comparison log against the current
cross-city count consumers (the `getNum*` prereq scans + demographics). Validate, then **modifier**, then **enabler**. Each
machine carries the Phase-F alignment fixes it touches (ranking "Phase F", now light/iterative) as it's wired.
