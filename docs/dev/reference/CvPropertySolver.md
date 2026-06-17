# CvPropertySolver

**File:** `CvPropertySolver.h` / `CvPropertySolver.cpp`  
**Standalone singleton**

## Overview
The `CvPropertySolver` is the engine that drives the [property system](CvProperties.md)
each turn. It collects all active `CvPropertySource`, `CvPropertyInteraction`, and
`CvPropertyPropagator` rules from every game object, runs a predict–correct cycle, and
applies the resulting value changes back to each object's `CvProperties` container.

The class is accessed as a global singleton through `GC.getPropertySolver()`.

## Core Solve Cycle
Each game turn the solver performs two passes:

1. **Predict** — calls `doPredict()` on every `PropertySourceContext`, computing
   predicted next-turn values into `PropertySolverMap`.
2. **Correct** — calls `doCorrect()` to apply actual changes and commit them via
   `PropertySolverMap::applyChanges()`.

## `PropertySolverMap`
An internal helper class that maps `CvGameObject*` → `CvProperties` for both predicted
values and pending changes.

| Method | Description |
|---|---|
| `getPredictValue(pObject, eProperty)` | Returns the predicted next-turn value for a property on an object. |
| `addChange(pObject, eProperty, iChange)` | Accumulates a pending change for a property. |
| `computePredictValues()` | Collapses accumulated changes into predicted values. |
| `applyChanges()` | Commits all pending changes to the actual `CvProperties` containers. |

## `PropertySourceContext`
Carries runtime state for a single `CvPropertySource` operating on a specific
`CvGameObject`.

| Method | Description |
|---|---|
| `PropertySourceContext(pSource, pObject)` | Constructor; stores source and owner pointers. |
| `getSource()` | Returns the `CvPropertySource*`. |
| `getObject()` | Returns the `CvGameObject*` this source acts on. |
| `getData1()` / `getData2()` | Returns internal scratch data used by the source. |
| `setData1(int)` / `setData2(int)` | Sets internal scratch data. |
| `doPredict(pSolver)` | Calls the source's predict step; queues the predicted change into the solver. |
| `doCorrect(pSolver)` | Calls the source's correct step; finalises the actual change. |

## Related
- [`CvProperties`](CvProperties.md) — per-object property storage  
- [`CvPropertySource`](../CvPropertySource.h) — interface for property input sources  
- [`CvPropertyInteraction`](../CvPropertyInteraction.h) — interface for property cross-effects  
- [`CvPropertyPropagator`](../CvPropertyPropagator.h) — interface for property spread  
- [`CvGlobals`](CvGlobals.md) — exposes `getPropertySolver()` globally  
