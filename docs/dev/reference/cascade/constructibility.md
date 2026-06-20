# Constructibility & the prerequisite model — the legacy "can I build/train this?" surface

> **Status:** reference   ·   **Verified against:** as-shipped behaviour after #195 (carried from `docs/dev/reference/constructibility-and-prerequisites.md`; re-confirm against the named functions before relying on a detail).
> **Grounding:** the live DLL functions named below (`CvCity::canConstruct`, `CvPlayer::canTrain`, `cvInternalGlobals::buildConstructibilityEnablerIndex`, `CvBuildingInfo::buildConstructRequirements`, `CvGameTextMgr::buildBuildingRequiresString`). Citations are by **function name** — line numbers drift, so confirm the function, not any integer.
> This is the **legacy** constructibility machinery (the introspectable `ConstructRequirement` model, the static enabler reverse-index, and the help-text consumers) and how it maps onto the cascade's enabler "can I?" surface. It is the as-shipped behaviour the cascade enabler is replacing — read this to understand the mechanism being shadowed/cut, not a description of the cascade itself.

The cascade's enabler is the "can I?" machine — HAS / CAN GET / HAS THE MEANS TO, with `enables` vs `requires` — defined in [`../../explanation/cascade-architecture.md` §2](../../explanation/cascade-architecture.md). This doc does **not** re-explain that model; it documents the **legacy** code that the enabler maps onto: the introspectable `ConstructRequirement` description, the `getBuildingsEnabledBy`/`getUnitsEnabledBy` reverse-index, and the help-text renderers. The real evaluation still lives in `CvCity::canConstruct` / `CvPlayer::canTrain`.

---

## The unified prerequisite model (the introspectable description)

`Sources/ConstructRequirement.h` — `ConstructRequirement { GOMTypes eGOM; ConstructRequirementOp eOp (REQUIRE_ALL / REQUIRE_ANY / FORBID / REQUIRE_COUNT); std::vector<int> aiIds; int iCount; }`. One introspectable prerequisite expressed over a game-object-modifier (GOM) type.

It is a **read-only description, not an evaluator** — `CvCity::canConstruct` / `CvPlayer::canTrain` still do the real evaluation and keep their `probabilityEverConstructable` hints and `bExposed`/`bTestVisible` gate stratification. The model exists so the prerequisite relationships can be *introspected* (for the reverse-index and the help text) without re-running the evaluator.

- `CvBuildingInfo::getConstructRequirements()` — built once at load in `doPostLoadCaching` (via `buildConstructRequirements`) from the typed `Prereq*` fields: GOM_BUILDING (InCity=ALL, Or=ANY, NotInCity=FORBID, NumOf=COUNT), GOM_TECH, GOM_BONUS (And/Or), GOM_RELIGION, GOM_CORPORATION, GOM_CIVIC (And/Or), GOM_OPTION, GOM_TERRAIN (And/Or), GOM_FEATURE, GOM_IMPROVEMENT, GOM_HERITAGE.
- `CvUnitInfo::getTrainRequirements()` — the train-side analogue.
- **Not modelled** (bespoke semantics, no consumer): vicinity / raw-vicinity bonus, state-religion, and the non-GOM prereqs (population, culture level, properties, war, power). These keep their typed handling — the model does not cover them.

## The static enabler reverse-index (turn-time)

`cvInternalGlobals::buildConstructibilityEnablerIndex()` (in `CvGlobals`) derives, once at the end of `doPostLoadCaching`, two reverse-indices from the model plus each construct/train condition's involved GOMs:

- `getBuildingsEnabledBy(BuildingTypes B)` → buildings whose constructibility B (or a free bonus B grants) can flip true.
- `getUnitsEnabledBy(BuildingTypes B)` → units B can help train.

These are a **pure function of info data → identical on every client** (lockstep / OOS-safe), never rebuilt during play. The CABV PreLoop (`CvCityAI::CalculateAllBuildingValues`) builds the constructible set from `getBuildingsEnabledBy` (O(dependents)) instead of the old O(buildings²) inner re-scan, and the unit-enabler value loops iterate `getUnitsEnabledBy(B)` instead of all units. This was the #195 Phase 1 turn-time win (~390× on the PreLoop; ~11.7 s/turn of enabler re-derivation eliminated). The enabler relationship is introspected from conditions via `BoolExpr::getInvolvedGOMs` (the gather-visitor sibling of `getInvolvesGOM`).

This reverse-index is the legacy analogue of the cascade enabler's **CAN GET** generation — "for each thing you HAVE, what does it unlock?" — see [`../../explanation/cascade-architecture.md` §2](../../explanation/cascade-architecture.md). Determinism here is the same OOS-load-bearing constraint the cascade math carries ([DEC-fixedpoint-x100](../../architecture/decisions.md#dec-fixedpoint-x100)); the index being a pure function of static info data is what keeps it lockstep-safe.

## Help-text consumption

`CvGameTextMgr::buildBuildingRequiresString` renders a building's prerequisites from the model:

- `appendVicinityRequirementHelp` — terrain / feature / improvement "in city vicinity".
- `appendRequirementHelp` + `buildRequirementItemLink` — status-aware list of **unmet** items as clickable links, filtered by `CvGameObject::hasGOM` (the same oracle the construct-condition evaluates against, so displayed have/need status == real constructibility). Used for Or-/InCity-buildings and bonuses.
- `appendCivicRequirementHelp` — civics' show-all colour-coded UX (have = green, need = red) plus met-status for the "requires active civics" note.
- **Still hand-rolled by design:** tech (`bTechChooserText` chooser gating + the model merges PrereqAndTech/AndTechs), religion (symbol group), NotInCity (FORBID inverse), and the non-GOM / vicinity-bonus / state-religion blocks. These are *not* driven from the model.

## Verifying the model backs the index

`cvInternalGlobals::logConstructRequirementFidelity()` emits a one-shot `[PERF/reqmodel]` audit (building + unit, model vs typed fields) from the CABV PreLoop when `Autolog__LogLevelPerf >= 1`. `mismatches=0` is the pass condition. It is **logged, not asserted**, so it works in FinalRelease (where `FASSERT` compiles out).

## See also
- [`../../explanation/cascade-architecture.md`](../../explanation/cascade-architecture.md) — the cascade enabler's "can I?" model (HAS / CAN GET / HAS THE MEANS TO, `enables` vs `requires`); this legacy `canConstruct`/`canTrain` + reverse-index machinery is what that enabler maps onto and is replacing.
- [`fixed-point-and-scales.md`](fixed-point-and-scales.md) — the cascade scale registry; the OOS-determinism constraint that makes the reverse-index a static pure-function-of-data lives in the same lockstep regime ([DEC-fixedpoint-x100](../../architecture/decisions.md#dec-fixedpoint-x100)).
- [`../../architecture/decisions.md`](../../architecture/decisions.md) — the rulings ledger; in particular [DEC-cascade-bidirectional](../../architecture/decisions.md#dec-cascade-bidirectional) (the cascade `requires`/AND model that supersedes this legacy gate evaluation) and [DEC-map-before-delete](../../architecture/decisions.md#dec-map-before-delete) (why this legacy mechanism is shadowed, not yet cut).
- [`../../README.md`](../../README.md) — the comprehension map / overview-of-overviews.
