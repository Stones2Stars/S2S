# The contexts — the per-scope live-state read surface

> Part of the **[cascade](../cascade.md)** spec.

> The live-state object a cascade getter and the one condition evaluator read to compute an entity's ACTUAL value in
> a given place. One per game-object scope that needs it: **PlotContext** (`CvPlot`), **CityContext** (`CvCity`),
> **EmpireContext** (`CvPlayer`). Owner rulings; this is the concrete shape the "make the infos sane"
> `(cityContext, plotGroup)` getters ([patterns.md § INFO DATA-OUT](../architecture/patterns.md)) read.

**A context is cascade OUTPUT, not a separate "input" kind:** *"contexts, when thinking about it,
are in essence the output of the cascade."* Same scopes, same spine, never serialized, rebuilt by the same
reseed, read as the same bare fetch — and maintained the same way, by the fact that names the source
([the maintained sum](05-three-planes.md#-the-maintained-sum--three-planes-one-slot-and-nothing-is-ever-recomputed)). What differs is only WHO CONSUMES the
value: a package answers a magnitude, a context store answers a gate. ⚠ That is a statement about the consumer,
never about the kind of thing being stored, and treating it as two planes is what let them drift onto opposite
maintenance mechanisms before this page merged them back into one.

### The one idea — isolate the CHANGEABLE state a reader needs, per scope, in ONE understandable place

A building's output getter computes the ACTUAL benefit in a city, which depends on that city's live state (its
connected/vicinity bonuses, river/coast plots, power, religions, …). Rather than every getter reaching into the
`CvCity`/`CvPlayer` god-objects ad hoc, each game object that a reader needs owns **one context** — the single,
predictable home for that object's changeable state. The **symmetry IS the value**: a reader always knows where to
go (city state → `CityContext`, empire state → `EmpireContext`, plot state → `PlotContext`).

**Isolation is for RESPONSIBILITY, not decoupling.** The context is bound to its game object by pointer and
freely reaches into it — coupling is fine when the structure is ironclad. The goal is a clean responsibility line
(this object is THE state surface for its scope), never running detached from the live object.

