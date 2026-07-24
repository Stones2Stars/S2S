# Per-scope live-state contexts — the cascade read surface

> The live-state object a cascade getter and the one condition evaluator read to compute an entity's ACTUAL value in
> a given place. One per game-object scope that needs it: **PlotContext** (`CvPlot`), **CityContext** (`CvCity`),
> **EmpireContext** (`CvPlayer`). Owner rulings; this is the concrete shape the "make the infos sane"
> `(cityContext, plotGroup)` getters ([patterns.md § INFO DATA-OUT](patterns.md)) read.

## The one idea — isolate the CHANGEABLE state a reader needs, per scope, in ONE understandable place

A building's output getter computes the ACTUAL benefit in a city, which depends on that city's live state (its
connected/vicinity bonuses, river/coast plots, power, religions, …). Rather than every getter reaching into the
`CvCity`/`CvPlayer` god-objects ad hoc, each game object that a reader needs owns **one context** — the single,
predictable home for that object's changeable state. The **symmetry IS the value**: a reader always knows where to
go (city state → `CityContext`, empire state → `EmpireContext`, plot state → `PlotContext`).

**Isolation is for RESPONSIBILITY, not decoupling (owner).** The context is bound to its game object by pointer and
freely reaches into it — coupling is fine when the structure is ironclad. The goal is a clean responsibility line
(this object is THE state surface for its scope), never running detached from the live object.

## What a context STORES vs FORWARDS — ⛔ don't duplicate (owner)

A context **STORES only its uniquely-owned AGGREGATE** — state that has no home elsewhere and would otherwise be
recomputed by every reader. Everything already O(1) on the game object is **FORWARDED** (read through the bound
pointer), never copied. Duplicating already-available state is the exact anti-pattern this avoids.

| context | owner | STORES (unique aggregate) | FORWARDS (read through the bound object / its owner) |
|---|---|---|---|
| **CityContext** | `CvCity` | `plotAttrs` — per-predicate plot COUNTS (how many river/water/hills/… plots); no `CvCity` accessor provides it | population, power, religion presence, holy-city, corporation, vicinity bonus (→ `CvCity`); state religion, policies (→ owner `CvPlayer`) |
| **EmpireContext** | `CvPlayer` | `policies` — the empire's enacted-policy set (the derived UNION over live civics'/traits' policy blocks, stored nowhere else) | state religion (single enum → `CvPlayer::getStateReligion`) |
| **PlotContext** | `CvPlot` | plot-scope aggregate — *defined when built* | plot facts already on `CvPlot` |

**Pass by reference/pointer, never by value (owner).** Passing a bound context is far cheaper than snapshotting
values; a context is never a value copy — that is *why* it forwards rather than mirrors.

## COUNTS, not objects — "how many, not which" (owner)

An aggregate holds **counts keyed by id**, never the objects themselves. A building cares HOW MANY river plots /
vicinity bonuses it has, never WHICH. So a `plots`-target (or keyed) deposit's output is `flat × count(id)`, and a
gate is `has(id)` (count > 0). The uniform keyed dictionary is **`ContextDict`** (`id → count`, with
`has`/`count`/`add`/`set`) — ONE kind, shared by every context, so the read is uniform and each family's key set is
OPEN (a new predicate/type is a new key, never a reshape). `plotAttrs` keys on the `CASC_PRED_*` HAS_/IS_ plot
predicate ids; `policies` on the `POLICY_*` classification ids.

Non-dictionary scalars stay plain: population/power are `int` (power carries 0/1 today but stays `int` so a future
**volumetric** model needs no reshape); state religion is a **single enum**, not a dictionary (there is exactly one).

## Maintained EVENT-DRIVEN — never a per-turn recompute

The stored aggregate rides events, exactly like the rest of the spine; a missed event drifts it, but that is the
event spine's **baseline invariant** (plot-groups and vicinity drift the same way if events are incomplete), not a
context-specific weakness. There is **no blanket per-turn rebuild** and no recompute-on-read.

- **`CityContext.plotAttrs`** ← `CvPlot::updateWorkingCity`: a plot entering/leaving the city's owned worked-radius
  set fires `CvCity::onCityPlotChanged(plot, ±1)`, which folds the plot's stable HAS_/IS_ attributes.
- **`EmpireContext.policies`** ← the civic/trait change event (rebuild the union).
- **Forwarded** fields need no maintenance — they read the live source.
- **Load** builds the contexts from the **reseed**: the same events fire from inside the save read
  ([DEC-spine-reseed](decisions.md#dec-spine-reseed)); never a post-load recompute.

## Scope set — plot / city / player now; units FUTURE (role-specific); no AreaContext (owner)

Contexts exist today on **plot, city, and player**. There is deliberately **no `AreaContext`**: an area is not
specific to any player — it is a bare **id**, "a really big plot" to reference. An area-scoped effect (**power** is
virtually the only driver of area-scope modifiers) maps onto the **player**, never an area context. Team is likewise
not a context.

**Units are a deliberate FUTURE scope, held off on purpose (owner).** A unit context must be **ROLE-SPECIFIC**: the
goal is that a unit no longer carries ALL the data (the ~247-field fat-unit problem) — each unit holds only the state
its role needs. Working out that role-partitioning is *why* it waits, rather than wiring a fat unit context now.

## The read — `(cityContext, plotGroup)`: vicinity vs traded

A building-output getter takes `(const CityContext& cityContext, const CvPlotGroup& plotGroup)`: `cityContext`
supplies **vicinity + local** state, `plotGroup` (the existing trade-network object, already engine-event-maintained)
supplies the **traded** bonuses. This is the json `connection: "vicinity"` vs `"trade"` split
([json.md §3.4](../specs/json.md)), resolved by default from whichever context owns the fact — **traded state is
NEVER mirrored into `CityContext`**. Empire facts a city getter needs (state religion, policies) forward through
`cityContext` to the owner's `EmpireContext`; an empire-scope getter takes an `EmpireContext` directly.

> **Naming — no abbreviated parameters (owner).** The parameters are spelled in full (`cityContext`, `plotGroup`),
> never `cx`/`pg`: short names are only defensible inside a tightly-scoped lambda, which the C++03 toolchain does not
> have. Index parameters likewise name the enum they key (`getFlatYield(YieldTypes eYield)`,
> `getDefense(DefenseKind eKind)`), reusing the existing engine + family enums — a new family mints one typed enum,
> the member array and its getter both key off it.

## See also
- [patterns.md](patterns.md) — the INFO DATA-OUT contract + the `(cityContext, plotGroup)` getter surface that reads these contexts.
- [state-repositories.md](state-repositories.md) — the derived-cache (OUTPUT-value) plane; the contexts are the
  INPUT-state read surface (distinct: contexts hold input facts the getters/evaluator read, not cached output values).
- [../specs/modifier.md](../specs/modifier.md) — the deposits the getters sum; [../specs/enabler.md](../specs/enabler.md)
  — the availability machine that reads the same state; [decisions.md](decisions.md#dec-scope-contexts) — the ruling.
