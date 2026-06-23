# Properties — the generic per-object attribute system (crime / disease / education / pollution)

> **Status:** reference   ·   **Verified against:** `Sources/Engine/CvProperties.{h,cpp}`, `CvPropertyManipulators.h`, `CvPropertySource.h`, `CvCity.cpp` — re-walked 2026-06-20.
> **Grounding:** the `CvProperties` API + the `PropertyBuilding`/`PropertyPromotion` band structs were read from the live header; the band-placement loop from `CvCity::checkPropertyBuildings` (`Sources/Engine/CvCity.cpp:1500`). The `Cv*` engine files moved into `Sources/Engine/` since the original authoring; paths below reflect that home.
>
> Bottom line up front: a `CvProperties` is a generic attribute bag attachable to **any** game object (game / team / player / city / unit / plot). Each entry is a `PropertyTypes` keyed pair — a **current value** and a **per-turn change rate**. It carries the simulation variables that don't fit the yield/commerce model (crime, disease, education, pollution, tourism), and it is the substrate the per-turn [property solver](property-solver.md) reads and writes. The band-placement structs on `CvPropertyInfo` are what drive the property-band auto-placement buildings.

> **Citations drift.** Line numbers are "the function named here, around this line" — confirm the **function**, not the integer.

---

## What it is

`CvProperties` (`Sources/Engine/CvProperties.h`) is a **standalone container**, not part of the
`CvGame`/`CvPlayer` hierarchy. An instance holds two parallel vectors of `(PropertyTypes, int)` pairs —
`m_aiProperty` (current values) and `m_aiPropertyChange` (per-turn change rates) — plus a back-pointer to
the `CvGameObject*` it belongs to. It is constructed for a specific owner type via the explicit
constructors:

```cpp
CvProperties()                    // standalone / unowned
explicit CvProperties(CvGame*)
explicit CvProperties(CvTeam*)
explicit CvProperties(CvPlayer*)
explicit CvProperties(CvCity*)
explicit CvProperties(CvUnit*)
explicit CvProperties(CvPlot*)
```

The owner back-pointer matters for `propagateChange` (below), which spreads a delta to configured
propagators on the owning object's `CvGameObject`.

## API surface

**Read** (`Sources/Engine/CvProperties.h:52`): `getNumProperties`, `getProperty(index)`,
`getValue(index)`, `getChange(index)`, `getPositionByProperty(eProp)` (list index or `-1` if absent),
`getValueByProperty(eProp)`, `getChangeByProperty(eProp)`.

**Mutate:** `setValue`/`setChange` (by index), `setValueByProperty`/`setChangeByProperty` (by name),
`changeValue`/`changeValueByProperty`/`changeChangeByProperty` (additive deltas), and
`propagateChange(eProp, iChange)` — applies the change to this object **and** any configured propagators.

**Aggregate:** `addProperties` / `subtractProperties` (fold another container's values+changes in/out),
`isEmpty`, `clear`, `clearChange` (zero all change rates), `clearForRecalculate` (reset change rates for a
full recompute), `copyNonDefaults`.

**Comparison — asymmetric, deliberately.** The header warns the operators are **not symmetric**: only
properties defined in the *right-hand* operand are considered, so any object is simultaneously `<` and `>`
the empty-property object (`CvProperties.h:78`). These back the property-`requires` min/max display strings
(`buildRequiresMinString` / `buildRequiresMaxString`).

**Serialisation & checksum:** `read`/`write` (+ `readWrapper`/`writeWrapper`) for the save stream; a second
`read(CvXMLLoadUtility*, szTagName)` overload loads XML definitions; `getCheckSum(uint32_t&)` folds the
property state into the OOS/save-validation checksum. (Save format: [`save-load-format.md`](save-load-format.md).)

## Property manipulators — where values come from

A `CvProperties` container is passive storage. The *rules* that change it are
**`CvPropertyManipulators`** (`Sources/Engine/CvPropertyManipulators.h`), attached to info objects (e.g.
`CvBuildingInfo`, `CvHandicapInfo`, `CvBonusInfo`). A manipulator block holds three kinds of rule, each an
abstract base with concrete XML-driven subclasses:

| Manipulator | Header | Role |
|---|---|---|
| **source** (`CvPropertySource`) | `Sources/Engine/CvPropertySource.h` | Where a property's value **comes from** — a per-turn input/drain (e.g. a building producing crime). Pure-virtual `getSourcePredict` / `getSourceCorrect`. |
| **interaction** (`CvPropertyInteraction`) | `Sources/Engine/CvPropertyInteraction.h` | How one property's value **affects another** on the same object (a cross-effect between properties). |
| **propagator** (`CvPropertyPropagator`) | `Sources/Engine/CvPropertyPropagator.h` | How a property **spreads** to related objects (e.g. crime/disease bleeding between adjacent plots or from city to city). |

The [property solver](property-solver.md) gathers the active manipulators from every live object each turn
and runs them; this doc describes the storage and the band structs, the solver doc describes the solve
cycle.

## Property-band auto-placement buildings

`CvPropertyInfo` (`Sources/Infos/CvPropertyInfo.{h,cpp}`) carries a list of **`PropertyBuilding`** band
entries (and a parallel `PropertyPromotion` list for units). The structs live on `CvProperties.h`:

```cpp
struct PropertyBuilding  { int iMinValue; int iMaxValue; BuildingTypes eBuilding; };
struct PropertyPromotion { int iMinValue; int iMaxValue; PromotionTypes ePromotion; };
```

Each band says: *while this property's value sits in `[iMinValue, iMaxValue]`, this city should have this
building.* The per-turn enforcement is `CvCity::checkPropertyBuildings` (`Sources/Engine/CvCity.cpp:1500`),
called from `CvCity::doAutobuild` (`CvCity.cpp:1467`) — **skipped for NPC players** (the guard at
`CvCity.cpp:1465`). For every property and every band on its `CvPropertyInfo`:

- `bInRange = (iValue >= iMinValue) && (iValue <= iMaxValue)` for the city's current property value.
- If the city **already has** the band building and it is now **out of range** (or no longer
  `canConstruct`), the building is **removed** (`changeHasBuilding(..., false)`).
- If the city **lacks** it and is now **in range** and `canConstruct`, the building is **added**
  (`changeHasBuilding(..., true)`).

This is how crime/disease/education/pollution property levels silently grant and revoke their consequence
buildings (the `building_crime_*`, `building_disease_*`, `building_education_*` family in `Assets/Data`)
without a player action.

## Relationship to the cascade

The property-band auto-placement is exactly a §14-style **maintainer** — imperative per-turn machinery that
mutates the build state — of the kind the [cascade](../../explanation/cascade-architecture.md) is built to
replace. Under the cascade rework, the band-placement verdict is **shadowed** against the live engine until
clean, then the legacy mechanism is cut ([DEC-map-before-delete](../../architecture/decisions.md#dec-map-before-delete);
[DEC-no-guessing](../../architecture/decisions.md#dec-no-guessing) governs how a divergence is attributed —
map it to a named source, never guess). This reference documents the **legacy mechanism as it behaves
today**, which is what the shadow diffs against.

## See also
- [Property solver](property-solver.md) — the per-turn cycle that reads these containers and writes them back via the manipulators; this doc is its storage layer.
- [`cascade-architecture.md`](../../explanation/cascade-architecture.md) — the data engine that supersedes the band-placement maintainer; property bands are a shadowed §14 maintainer, not re-explained here.
- [`save-load-format.md`](save-load-format.md) — the name-keyed format `read`/`write`/`getCheckSum` serialise into.
- [`decisions.md`](../../architecture/decisions.md) — [DEC-map-before-delete], [DEC-no-guessing] govern the shadow-then-cut and divergence-attribution discipline.
- [`observability/README.md`](../observability/README.md) — the surveillance surface the band-placement shadow reports through.
- [`README.md`](../../README.md) — the docs2 comprehension map.
