# Constructibility & the prerequisite model

How "can this be built/trained, and what gates it?" works today, after #195. The
forward-looking design/rollout/measurements live in the plan
[`../plans/unified-prerequisites-and-constructibility.md`](../plans/unified-prerequisites-and-constructibility.md);
this note is the as-shipped behaviour.

## The unified prerequisite model

`Sources/ConstructRequirement.h` — `ConstructRequirement { GOMTypes eGOM;
ConstructRequirementOp eOp (REQUIRE_ALL / REQUIRE_ANY / FORBID / REQUIRE_COUNT);
std::vector<int> aiIds; int iCount; }`. One introspectable prerequisite expressed over a
game-object-modifier (GOM) type. It is a **read-only description, not an evaluator** —
`CvCity::canConstruct` / `CvPlayer::canTrain` still do the real evaluation and keep their
`probabilityEverConstructable` hints and `bExposed`/`bTestVisible` gate stratification.

- `CvBuildingInfo::getConstructRequirements()` — built once at load in
  `doPostLoadCaching` (`buildConstructRequirements`) from the typed `Prereq*` fields:
  GOM_BUILDING (InCity=ALL, Or=ANY, NotInCity=FORBID, NumOf=COUNT), GOM_TECH, GOM_BONUS
  (And/Or), GOM_RELIGION, GOM_CORPORATION, GOM_CIVIC (And/Or), GOM_OPTION, GOM_TERRAIN
  (And/Or), GOM_FEATURE, GOM_IMPROVEMENT, GOM_HERITAGE.
- `CvUnitInfo::getTrainRequirements()` — the train-side analogue.
- **Not modelled** (bespoke semantics, no consumer): vicinity / raw-vicinity bonus,
  state-religion, and the non-GOM prereqs (population, culture level, properties, war,
  power). These keep their typed handling.

## The static enabler reverse-index (turn-time)

`cvInternalGlobals::buildConstructibilityEnablerIndex()` (CvGlobals) derives, once at the
end of `doPostLoadCaching`, two reverse-indices from the model + each construct/train
condition's involved GOMs:

- `getBuildingsEnabledBy(BuildingTypes B)` → buildings whose constructibility B (or a free
  bonus B grants) can flip true.
- `getUnitsEnabledBy(BuildingTypes B)` → units B can help train.

These are a pure function of info data → identical on every client (lockstep/OOS safe),
never rebuilt during play. The CABV PreLoop (`CvCityAI::CalculateAllBuildingValues`) builds
the constructible set from `getBuildingsEnabledBy` (O(dependents)) instead of the old
O(buildings²) inner re-scan, and the unit-enabler value loops iterate `getUnitsEnabledBy(B)`
instead of all units. This was the #195 Phase 1 turn-time win (~390× on the PreLoop;
~11.7 s/turn of enabler re-derivation eliminated). The enabler relationship is introspected
from conditions via `BoolExpr::getInvolvedGOMs` (the gather visitor sibling of
`getInvolvesGOM`).

## Help-text consumption

`CvGameTextMgr::buildBuildingRequiresString` renders a building's prerequisites from the
model:

- `appendVicinityRequirementHelp` — terrain/feature/improvement "in city vicinity".
- `appendRequirementHelp` + `buildRequirementItemLink` — status-aware list of **unmet**
  items as clickable links, filtered by `CvGameObject::hasGOM` (the same oracle the
  construct-condition evaluates against, so displayed have/need status == real
  constructibility). Used for Or-/InCity-buildings and bonuses.
- `appendCivicRequirementHelp` — civics' show-all colour-coded UX (have = green, need =
  red) + met-status for the "requires active civics" note.
- Still hand-rolled by design: tech (`bTechChooserText` chooser gating + the model merges
  PrereqAndTech/AndTechs), religion (symbol group), NotInCity (FORBID inverse), and the
  non-GOM / vicinity-bonus / state-religion blocks.

## Verifying the model backs the index

`cvInternalGlobals::logConstructRequirementFidelity()` emits a one-shot `[PERF/reqmodel]`
audit (building + unit, model vs typed fields) from the CABV PreLoop when
`Autolog__LogLevelPerf >= 1`. `mismatches=0` is the pass condition. It is **logged, not
asserted**, so it works in FinalRelease (where `FASSERT` compiles out). See
[`ai-logging-reference.md`](ai-logging-reference.md).
