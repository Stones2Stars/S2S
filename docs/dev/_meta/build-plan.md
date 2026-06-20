# docs2 rebuild — triage, migration tracker, and swap process

> The plan for rebuilding the dev docs into `docs2/` grounded and best-practice (see
> [`CONVENTIONS.md`](CONVENTIONS.md)), then retiring the old set outside the repo to stop poisoning.
> Owner mandate 2026-06-19: complete, correct agent understanding is the primary goal; footprint is a
> bonus. Rebuild — don't patch — and ground every claim in code.

---

## 1. The scope boundary (what gets rebuilt vs retired)

The current work is the **info-handling / #428-#430 cascade rework** and its verification surface. That is
orthogonal to **how the AI behaves**. So:

- **REBUILD (grounded) into `docs2/`:** cascade & info-handling, data model & migration, observability /
  logging, the engine **state** classes the rework touches, save format, and the cross-cutting decisions.
- **RETIRE (archive, rebuild-from-code when a task needs it):** the **AI-behaviour** docs — how the `Cv*AI`
  classes *decide*. They reference already-fixed code, drift fast, and are out of scope. The behaviour is
  read straight from `Cv*AI.cpp` under trust-but-verify when actually needed. This is owner-authorised
  (2026-06-19): *"I'd rather rebuild them again when needed."*

## 2. Triage

### 2a. RETIRE — AI-behaviour REFERENCE docs only (archive; rebuild from code when needed)
ONLY the docs that describe how the AI *decides today* — reconstructible straight from `Cv*AI.cpp`, drift
fast, reference already-fixed code: `CvCityAI`, `CvUnitAI`, `CvPlayerAI`, `CvGameAI`, `CvTeamAI`,
`CvSelectionGroupAI`, `CvWorkerAI`, `CvContractBroker`, `CvArmy`, `UnitAI_Selection`,
`UnitSelection_Mechanics`, `doProduction`.

> The load-bearing *rulings* these carried (the garrison-tiers two-ledger rule #384, the fallback-terminal
> rule) already live in `AGENTS.md` Key Subsystem Knowledge + the decisions ledger — those stay. Only the
> detailed behaviour *docs* are retired.

### 2a-bis. KEEP — AI-DESIGN plans (out of active scope, NOT retired) — owner ruling 2026-06-19
Forward design intent is **not reconstructible from code**, so it is kept — just partitioned as outside
the active info-handling scope (an agent on the cascade work is told it's a separate domain). Carried to
`docs2/plans/parked/` (a clearly-marked out-of-active-scope partition — broadened from `ai/`, since
combat-model / multimap / gamespeed aren't AI, just not the active cascade work): `ai-architecture-north-star`,
`ai-logging-rollout`, `ai-vs-human-benchmarking`, `unit-ai-valuation`, `sea-ai-rework`, `size-matters-ai`,
`subdued-animal-ai`, `fight-or-flight`, `combat-model-sketch`, `combat-odds-baseline`,
`combat-phase3b-plan`, `combat-simplification-scope`, `worker-stranded-tiles-reachability`,
`surround-destroy-removal-map`. (Rebuilt to the grounding standard when the AI work becomes active; carried
as-is until then so the intent is never lost.)

### 2b. REBUILD — into `docs2/reference/cascade/`
`cascade-fixed-point` → **`fixed-point-and-scales.md`** *(BUILT — the scale registry + grounding
exemplar)*; `legacy-value-calc-map`; `constructibility-and-prerequisites`; `declarative-info-loading`;
`unitcombat` (combat-class data, migration-relevant); the data-model (from `data-model-spec`). The cascade
**specs** (`enabler/modifier/tally-cascade-spec`, `event-spine-spec`, `cascade-engine-430`,
`building-cascade-conversion`, `calc-emulator-spec`, the inventories, `migration-*`) become grounded
reference + `docs2/plans/` roadmap entries as each lands.

### 2c. REBUILD — into `docs2/reference/observability/`
The 22 per-system maps + their shared scaffold `README.md`; `http-server`; `logging-field-catalog`;
`logging-surface-inventory`; `ai-logging-reference`; **`plotsnapshot.md`** (reframed: a **core** map/city
reference logging mechanic, not an AI appendage — owner 2026-06-19).

### 2d. REBUILD — into `docs2/reference/engine/`
The engine **state** classes the rework reads/writes: `CvCity`, `CvPlayer`, `CvPlot`, `CvGame`, `CvTeam`,
`CvMap`, `CvGlobals`, `CvUnit`, `CvSelectionGroup`, `CvOutcome`, `CvProperties`, `CvPropertySolver`;
movement mechanics `pathfinding` + `CvPathGenerator`; `MapScript_Process`; build & toolchain
(`fbuild`/`_Build.ps1` + `boost-situation`); `handicaps`; `calendar-and-gamespeed`; `save-load-format`.
*(Rebuilt grounded as a task touches each — not all up front.)*

### 2e. REBUILD — into `docs2/plans/` and `docs2/explanation/`
`explanation/cascade-architecture.md` (the design narrative, the 3 machines + spine + tally). Roadmap:
the cascade migration status, `dead-code-xml-pass`, `sources-structural-cleanup`,
`derived-data-repository`, etc. — cascade/info-handling-relevant plans only.

### 2f. CARRY AS-IS
[`decisions.md`](../architecture/decisions.md) — the rulings ledger (already grounded/ID'd) becomes the docs2 canonical
ledger.

## 3. Status

| Artifact | State |
|---|---|
| `README.md` (comprehension map) | ✅ built |
| `_meta/CONVENTIONS.md` (grounding standard) | ✅ built |
| `_meta/build-plan.md` (this) | ✅ built |
| `architecture/` (north-star, patterns, superseded-ideas, decisions ledger) | ✅ built |
| `explanation/cascade-architecture.md` | ✅ built (the consolidated full cascade design) |
| `reference/cascade/` — fixed-point-and-scales, constructibility, legacy-value-calc-map, data-model, **enabler, modifier, tally, event-spine, shadow** | ✅ built (the full cascade design reference) |
| `reference/engine/` — save-load-format, declarative-info-loading, handicaps | ✅ built |
| `reference/observability/` — README scaffold, http-server | ✅ built |
| `reference/observability/` — all 22 per-system maps | ✅ 22/22 built |
| `reference/observability/` — PlotSnapshot (reframed core obs), ai-logging-reference, logging-field-catalog, logging-surface-inventory | ✅ built |
| `reference/engine/` — properties, property-solver, pathfinding, path-generator, calendar-and-gamespeed, boost-situation, unitcombat, map-script-process + `reference/external-tools-and-workflows` | ✅ built |
| `reference/engine/` — the deep-dive state classes (CvCity/CvPlayer/CvPlot/CvGame/CvTeam/CvUnit/CvMap/CvGlobals/CvSelectionGroup/CvOutcome), pathfinding/CvPathGenerator, calendar-gamespeed, boost-situation, unitcombat, MapScript | ☐ rebuild-AS-TOUCHED (§2d — deep-dive class reference; NOT needed for the coherent shape, rebuilt from code when a task needs one) |
| `plans/` — README, cascade-migration, structural-cleanup ✅; `parked/` (23 initiatives carried) ✅; cascade-roadmap docs consolidated + non-cascade triage complete | ✅ |

> **⚠ GROUNDING NOTE for every rebuild batch (discovered 2026-06-20):** the `Sources/` tree has been
> **reorganized** since the old docs were written — `Cv*` engine classes now live under `Sources/Engine/`,
> Info classes under `Sources/Infos/`, defines under `Sources/Defines/`, the save wrapper under
> `Sources/Infrastructure/`. Old docs carry stale `Sources/CvX.{h,cpp}` paths. Rebuild agents MUST verify
> the file's current location and update the path prefix (line numbers then drift — keep the
> "confirm the function" caveat). This re-grounding is a primary value of the rebuild, not a side effect.

## 4. The swap (DONE) and the old-set retirement

1. Build out `docs2/` per §2 until it covers the in-scope corpus. ✅
2. Validate: every doc grounded + cited; the comprehension map resolves; no dangling links. ✅ (the
   non-parked rebuilt set is link-clean; remaining broken links live only in `plans/parked/` — un-rebuilt,
   carried-as-is material, see §2a-bis.)
3. Promote `docs2/` → `docs/dev/` and repoint the read-gate manifest
   (`.claude/read-gates/cascade.json` `docsDirs`) to the new set. ✅
4. **The old set was kept INSIDE the repo as `old-docs/` at the repo root** — NOT archived to a sibling
   folder (the earlier plan). This was deliberate: the old set is the source material for the **adversarial
   parity-validation pass** (diff `old-docs/` against `docs/dev/`: is anything still-current and load-bearing
   missing from the rebuild?). `old-docs/` is **NOT authoritative** and must not be read as truth — the
   read-gate dirs exclude it, so it is never force-read. **Once parity is confirmed, the OWNER removes
   `old-docs/` — moving it OUT of the repo (owner ruling 2026-06-20), not an agent `rm`.** Do not delete it.

Until the owner moves `old-docs/` out (parity confirmed first), the old set stays in-repo as diff source;
`docs/dev/` is authoritative for everything.
