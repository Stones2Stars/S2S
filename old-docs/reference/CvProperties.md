# CvProperties

**File:** `CvProperties.h` / `CvProperties.cpp`  
**Standalone class** (not in the `CvGame` / `CvPlayer` hierarchy)

## Overview

A generic, extensible attribute container that can be attached to *any* game object
(game, team, player, city, unit, or plot). Each property is identified by a
`PropertyTypes` enum value and stores both a **current value** and a **per-turn change
rate**. The system is used for simulation variables such as crime, pollution, and
tourism that do not fit into the standard yield / commerce model.

The global singleton [`CvPropertySolver`](CvPropertySolver.md) iterates all live game
objects each turn and applies source, interaction, and propagation rules to update
property values.

## Construction

`CvProperties` provides explicit constructors for each game-object type it can be owned
by:

```cpp
CvProperties()                    // standalone / unowned
explicit CvProperties(CvGame*)
explicit CvProperties(CvTeam*)
explicit CvProperties(CvPlayer*)
explicit CvProperties(CvCity*)
explicit CvProperties(CvUnit*)
explicit CvProperties(CvPlot*)
```

## Data Access

| Method | Description |
|---|---|
| `getNumProperties()` | Returns the number of distinct properties currently stored. |
| `getProperty(int index)` | Returns the `PropertyTypes` at a given list index. |
| `getValue(int index)` | Returns the current value at a list index. |
| `getChange(int index)` | Returns the per-turn change at a list index. |
| `getPositionByProperty(PropertyTypes)` | Returns the list index for a given `PropertyTypes`, or `-1` if absent. |
| `getValueByProperty(PropertyTypes)` | Returns the current value for a property type directly. |
| `getChangeByProperty(PropertyTypes)` | Returns the per-turn change for a property type directly. |

## Mutation

| Method | Description |
|---|---|
| `setValue(int index, int)` | Sets the value at a list index. |
| `setChange(int index, int)` | Sets the change rate at a list index. |
| `setValueByProperty(PropertyTypes, int)` | Sets the value for a named property. |
| `setChangeByProperty(PropertyTypes, int)` | Sets the change rate for a named property. |
| `changeValue(int index, int)` | Adds a delta to the value at a list index. |
| `changeValueByProperty(PropertyTypes, int)` | Adds a delta to a named property's value. |
| `changeChangeByProperty(PropertyTypes, int)` | Adds a delta to a named property's change rate. |
| `propagateChange(PropertyTypes, int)` | Propagates a change to this object and any configured propagators. |

## Aggregate Operations

| Method | Description |
|---|---|
| `addProperties(const CvProperties*)` | Adds all values and changes from another instance. |
| `subtractProperties(const CvProperties*)` | Subtracts all values and changes from another instance. |
| `isEmpty()` | Returns `true` if no properties are stored. |
| `clear()` | Removes all properties. |
| `clearChange()` | Resets all per-turn change rates to zero. |
| `clearForRecalculate()` | Resets change rates in preparation for a full recalculation. |

## Comparison

The comparison operators are **asymmetric**: only properties defined in the *right-hand*
operand are considered. This means any object is simultaneously less-than and
greater-than the empty-property object.

| Operator | Description |
|---|---|
| `operator<(const CvProperties&)` | Returns `true` if this instance is dominated by the other. |

## Serialisation & Checksum

| Method | Description |
|---|---|
| `read(CvXMLLoadUtility*)` | Loads property definitions from XML. |
| `write(FDataStreamBase*)` | Serialises property data to a save stream. |
| `getCheckSum(uint32_t&)` | Folds property state into a running checksum for save validation. |

## Related

- [`CvPropertySolver`](CvPropertySolver.md) — solves the per-turn update for all properties  
- [`CvPropertySource`](../CvPropertySource.h) — defines where properties come from  
- [`CvPropertyInteraction`](../CvPropertyInteraction.h) — defines how properties interact  
- [`CvPropertyPropagator`](../CvPropertyPropagator.h) — defines how properties spread  
- [`CvPropertyManipulators`](../CvPropertyManipulators.h) — attaches sources/interactions/propagators to info objects  
