# Property solver — the per-turn predict/correct cycle for the property system

> **Status:** reference   ·   **Verified against:** `Sources/Engine/CvPropertySolver.{h,cpp}`, `CvGame.{h,cpp}` — re-walked 2026-06-20.
> **Grounding:** the solve order + phase pairs were read from `CvPropertySolver::gatherAndSolve` / `doTurn` (`Sources/Engine/CvPropertySolver.cpp:421`, `:448`); the per-turn drive site from `CvGame::doTurn` (`Sources/Engine/CvGame.cpp:5954`). The `Cv*` engine files moved into `Sources/Engine/` since the original authoring; paths below reflect that home.
>
> Bottom line up front: `CvPropertySolver` is the engine that advances every object's [`CvProperties`](properties.md) one turn. Each turn it **gathers** every active source/interaction/propagator manipulator from every live game object, then solves **propagators → interactions → sources**, each in a **predict / compute / correct / apply** pass, committing the net change into the real containers.

> **Citations drift.** Line numbers are "the function named here, around this line" — confirm the **function**, not the integer.

---

## How it is reached

> ⚠ **Stale-flag correction vs the old doc.** The previous note described the solver as a *global
> singleton accessed through `GC.getPropertySolver()`*. That accessor does **not** exist in the live source.
> The solver is a **plain member of `CvGame`** — `CvGame::m_PropertySolver` (`Sources/Engine/CvGame.h:558`)
> — driven directly by `CvGame::doTurn`, which calls `m_PropertySolver.doTurn()` once per game turn
> (`Sources/Engine/CvGame.cpp:5954`, immediately before `map->doTurn()`). There is one per game; it is not
> reachable via `GC`.

## The solve cycle

`CvPropertySolver::doTurn` (`CvPropertySolver.cpp:448`) is the entry point. It:

1. `resetPropertyChanges()` — clears pending change state.
2. `gatherGlobalManipulators()` — collects manipulators attached globally (game-wide rules).
3. `gatherAndSolve()` — the main pass (below).
4. clears the global-manipulator scratch list.

`gatherAndSolve` (`CvPropertySolver.cpp:421`) first calls `gatherActiveManipulators`, which walks **every
game-object type** (`for i in [0, NUM_GAMEOBJECTS)`) and instantiates the active manipulator *contexts* from
each live object's `CvPropertyManipulators`. It then solves the three manipulator kinds **in a fixed order**,
each as its own four-step pass:

| Order | Phase | Steps |
|---|---|---|
| 1 | **Propagators** | `predictPropagators()` → `computePredictValues()` → `correctPropagators()` → `applyChanges()` → clear contexts |
| 2 | **Interactions** | `predictInteractions()` → `computePredictValues()` → `correctInteractions()` → `applyChanges()` → clear contexts |
| 3 | **Sources** | `predictSources()` → `computePredictValues()` → `correctSources()` → `applyChanges()` → clear contexts |

The **predict** half of each pass runs every context's `doPredict`, accumulating proposed changes into the
`PropertySolverMap`; `computePredictValues` collapses them into predicted next-turn values; the **correct**
half runs every context's `doCorrect`, computing the *actual* change given those predictions; `applyChanges`
commits the net deltas to the live `CvProperties` containers. (This predict-then-correct split is what lets a
manipulator react to the *projected* post-turn value rather than the raw current one — e.g. a drain that
should not overshoot zero.)

> The order is **propagators first, sources last** — spread and cross-effects are resolved against the
> pre-source values, then the per-turn sources/drains are applied last. This is the opposite of the
> intuitive "produce then spread"; it is the order the live `gatherAndSolve` uses.

## The helper classes

**`PropertySolverMap`** (`CvPropertySolver.h:24`) — maps `CvGameObject*` → `CvProperties` for two parallel
maps: predicted values (`m_mapProperties`) and pending changes (`m_mapPropertyChanges`).

| Method | Description |
|---|---|
| `getPredictValue(pObject, eProperty)` | The predicted next-turn value for a property on an object. |
| `addChange(pObject, eProperty, iChange)` | Accumulate a pending change. |
| `computePredictValues()` | Collapse accumulated changes into predicted values. |
| `applyChanges()` | Commit all pending changes to the real `CvProperties` containers. |

**Manipulator contexts** — one runtime-state wrapper per (manipulator, object) pair, each with a
`doPredict`/`doCorrect` and held in its own vector on the solver:

- `PropertySourceContext` (`CvPropertySolver.h:40`) — wraps a `CvPropertySource`; carries `getData1/2` /
  `setData1/2` scratch ints the source uses across predict→correct.
- `PropertyInteractionContext` (`CvPropertySolver.h:62`) — wraps a `CvPropertyInteraction`; tracks the
  current source-side and target-side amounts.
- `PropertyPropagatorContext` (`CvPropertySolver.h:79`) — wraps a `CvPropertyPropagator`; resolves and
  caches its set of target objects (`getTargetObjects`) and their per-target amounts.

The solver also exposes two scratch caches (`getCache1`/`getCache2`) the manipulators borrow during a pass,
and `addChange`/`getPredictValue` pass-throughs onto the solver map.

## Relationship to the cascade

The property solver is the imperative per-turn maintainer for the property variables. Like the property-band
auto-placement it feeds (see [`properties.md`](properties.md)), it sits in the class of machinery the
[cascade](../../explanation/cascade-architecture.md) is built to replace — and is **shadowed before
deletion** ([DEC-map-before-delete](../../architecture/decisions.md#dec-map-before-delete)). This reference
documents the solver **as it behaves today**, the ground truth a shadow diffs against.

## See also
- [Properties](properties.md) — the `CvProperties` containers this solver reads/writes and the manipulator kinds it solves; its storage layer.
- [`cascade-architecture.md`](../../explanation/cascade-architecture.md) — the data engine that supersedes this per-turn maintainer; not re-explained here, linked.
- [`observability/README.md`](../observability/README.md) — the surveillance surface a solver shadow reports through.
- [`decisions.md`](../../architecture/decisions.md) — [DEC-map-before-delete] (shadow-then-cut), [DEC-no-guessing] (attribute divergences to a named source).
- [`README.md`](../../README.md) — the docs2 comprehension map.
