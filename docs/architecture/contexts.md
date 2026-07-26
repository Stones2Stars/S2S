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

## What a context STORES vs FORWARDS — ⛔ a context is an EVENT-BUILT STORE, not a forwarding facade (owner)

**"Context should be built on events — that is the design of it."** And the purpose of storing is that the state
becomes DISTINGUISHABLE: *"so that an info can say 'yes, I will actually deliver this, based on this state.'"*
A context that merely forwards to its bound object delivers none of that — it is the same pointer hop with an
extra name, so the design collapses into "pass the god-object like always."

The split is by **DERIVED vs RAW**, not by convenience:

- **STORE — every DERIVED fact the evaluation reads.** Predicate verdicts, aggregates, unions: computed ONCE by
  the ONE derivation for that fact and maintained by the spine events, never recomputed at read. This is the
  context's substance. It is derived state, so it is **never serialized** and is rebuilt at load by the reseed
  ([DEC-derived-never-trusted](decisions.md#dec-derived-never-trusted), [DEC-spine-reseed](decisions.md#dec-spine-reseed)).
- **FORWARD — only the object's OWN RAW data** that it already maintains O(1) (the substrate ids a parameterized
  predicate keys on, population, …). Forwarding raw data is not duplication; storing a second copy of it would be.

⚑ **THE PAYOFF — this is why the design earns its cost (owner): once contexts are PURELY event-updated, an
enormous class of per-read CALCULATION becomes obsolete.** Not "gets faster" — ceases to exist. Every read-time
scan/union/walk collapses into a stored value some event already maintained, and reads become bare fetches
([state-repositories.md](state-repositories.md); [DEC-turn-time-is-king](decisions.md#dec-turn-time-is-king)).
The in-tree exhibits are not hypothetical: `isCoastalLand()` is an 8-neighbour scan **per predicate
evaluation**; the §5a vicinity check is a radius union **per check**; and `getNumBonuses` is recorded in
[enabler.md §8](../specs/enabler.md) as *"the turn wall's hottest cluster under the governor's read volume"* —
a tech gate → two-hop plot-group resolution → group sum → minted gate → corp add-on, **re-executed on every
call**. The `(scope,channel)` calc-count gate ([DEC-calc-count-gate](decisions.md#dec-calc-count-gate)) is how
the win is observed: steady-state cost must track EVENT volume (thousands), never read volume (millions).

⛔ **A forwarded read that COMPUTES is the defect this rule exists to kill.** `PlotContext::hasCoast()` forwarding
to `CvPlot::isCoastalLand()` — an 8-neighbour scan with an `area()->getNumTiles()` call per neighbour, on every
predicate evaluation — is the worked example, and it directly contradicts
[patterns.md](patterns.md): *"every evaluator predicate is an O(1) CONTEXT fetch … a predicate that walks
plots/units per call is the efficiency defect to reject in review."*

**The storage is keyed by the CONDITION VOCABULARY** — that is what makes the state distinguishable. One key
space (`CASC_PRED_*` / the classification ids), three granularities of the same design:

| context | stores | granularity |
|---|---|---|
| **PlotContext** | the plot's own predicate verdicts | a `CASC_PRED_*` BITSET |
| **CityContext** | `plotAttrs` | per-predicate COUNTS over the same keys — the FOLD of its member plots' bits |
| **EmpireContext** | `policies` | the `POLICY_*` id set (the union over live civics + held traits) |

So the city aggregate is not a second derivation: `CvCity::onCityPlotChanged(plot, ±1)` folds the plot's bits,
and the two granularities cannot drift.

⚠ **Adjacency-derived predicates fan out.** `HAS_COAST` / fresh-water depend on NEIGHBOURS, so the event that
changes a plot re-derives that plot's own bits AND its adjacent plots' adjacency bits. Bounded (8 neighbours) and
event-driven — never a read-time scan, and never left on the old accessor as an interim.

| context | owner | STORES (unique aggregate) | FORWARDS (read through the bound object / its owner) |
|---|---|---|---|
| **CityContext** | `CvCity` | `plotAttrs` — per-predicate plot COUNTS (the fold of member plots' bits) · **the VICINITY BONUSES available in the city** (owner) — the §5a union over the radius of every provider's `provides.bonuses`, i.e. DERIVED by construction, so the context HOLDS it | population, power, religion presence, holy-city, corporation (raw, `CvCity`-owned, O(1)); state religion (→ owner `CvPlayer`) |
| **EmpireContext** | `CvPlayer` | `policies` — the empire's enacted-policy set (the derived UNION over live civics'/traits' policy blocks, stored nowhere else) | state religion (single enum → `CvPlayer::getStateReligion`) |
| **PlotContext** | `CvPlot` | the `CASC_PRED_*` verdict **BITSET** — the OWN-PLOT block (water/land/relief/hills/peak/river/irrigation/feature-present/landmark/owned) plus the ADJACENCY block (coast, fresh-water) | the RAW substrate a parameterized predicate keys on — terrain/feature/improvement/route/bonus ids, owner, latitude, nature yield — plus worked/city, which carry no mutation event a bit could be maintained from (→ `CvPlot`) |

**Pass by reference/pointer, never by value (owner).** Passing a bound context is far cheaper than snapshotting
values; a context is never a value copy — that is *why* it forwards rather than mirrors.

**⚖ THE TWO PASS-IN SCENARIOS (owner) — a context crosses a call boundary in exactly TWO places, the two
condition-evaluation sites:** (1) **the VALUATION** — the `expected*` per-group reads and the package rebuild's
conditioned-deposit evaluation (the same machinery at event cadence); (2) **the `requires` edge** — the
enabler's build/operate gate incl. the operating-set fixpoint, re-run at HAVE-change over the affected
candidates. Both go through the ONE evaluator over the eval ctx the contexts fill. Every other read on every
surface is a straight compiled fetch and NEVER takes a context parameter — a context in any other signature is
the mechanical smell that condition evaluation (or an ad-hoc state reach) is happening where it doesn't belong.

## ⛔ THE EVAL CTX CARRIES CONTEXTS, NOT GAME OBJECTS (owner) — the contract must be STRUCTURAL

**"Otherwise we can just pass the full player, city, and whatever other objects again, without any
distinguishing."** That is the whole test, and it is a CONTRACT, not a prohibition: if the evaluation context
holds a `CvCity*`/`CvPlayer*`, then "the reader goes through the context" is enforced only by reviewer memory —
the god-object is right there, and reaching past the context is one `->` away (a derived
`&ctx.city->getCityContext()` is the tell: the ctx never held a context at all). The isolation must be
**unsayable to violate**, exactly as [patterns.md](patterns.md) states the info DATA-OUT contract: there is no
member to reach through.

So `CvCascadeEvalCtx` carries **`const CityContext*` / `const EmpireContext*` / `const PlotContext*`** — never
the bound game objects. Consequences, each already implied by the model above:

- **TEAM facts route through `EmpireContext`** (team is deliberately not a context; team-held techs are read
  through the player) — the ctx carries no `CvTeam*`.
- **`CvPlotGroup` STAYS a first-class ctx member** — it is the reserved explicit traded-bonus source (§ the
  read, above), not a scope whose state a context owns.
- **`CvUnit` stays raw FOR NOW** — units are the deliberate FUTURE role-specific scope; when that context
  lands it replaces the pointer, and until then this is the one acknowledged hole, not a licence for others.
- **The enabler's precomputed sets** (operating/active/obsolete buildings, vicinity-provided bonuses) stay ctx
  members: they are the ENABLER's derived output fed to the evaluator, never per-scope live state.
- **A context that cannot answer a needed fact is a CONTEXT GAP to close** (add the forward), never a reason to
  re-add an object pointer. That is the forcing function the structural form buys.

**⚖ THE HAVE AXIS LIVES IN THE CONTEXTS (owner).** What a scope POSSESSES — the city's buildings-present /
religions / corporations / bonuses, the empire's civics / traits / heritages, the team-held techs (read through
the player's team — team is deliberately not a context) — is read through that scope's context, never by an
ad-hoc reach into the game object. The STORES-vs-FORWARDS discipline above is unchanged: possession state the
object already owns O(1) is FORWARDED, and only a homeless aggregate is stored (`policies` is the realized
exemplar). The context is the RESPONSIBILITY home — the one place every reader (the evaluator's atoms, the
enabler's gates, the `expected*` valuations) goes for HAVE. The enabler's DERIVED sets (the domain vectors, the
operating-building set) remain enabler-owned ([enabler.md §7](../specs/enabler.md)); the contexts serve the raw
possession facts those machines gate against.

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

- **`PlotContext`'s verdict bitset** ← the plot-substrate DOMAIN facts (terrain / feature / improvement / route /
  bonus / owner), routed to the contexts' consumer (`Engine/ContextConsumer`), which re-derives the announcing
  plot's WHOLE block through the same `CvPlot` accessors a read used to call — one uniform derivation, never a
  bespoke per-event bit mask — and then, **only if a bit moved**, the ADJACENCY block of the 8 neighbours. That gate
  is exact: a neighbour's coast / fresh-water verdict reads nothing but facts held in this plot's own block, so the
  fan-out is one hop and cannot cascade. The plot mutations that carry **no** DOMAIN fact today (plot type, river
  crossings, irrigation, landmark) drive the same one maintenance entry directly from their choke point, the shape
  `updateWorkingCity` already uses for the fold below; promoting them to genuine emits is the spine's own gap to
  close, not a second derivation. Derivation is DEFERRED while the map is unsettled (world generation, mid-save-read,
  a `recalculateAreas` window) — the adjacency leg dereferences an adjacent water plot's `CvArea` — and the deferred
  plots drain on the first event after the game reports final-initialized.
- **`CityContext.plotAttrs`** ← `CvPlot::updateWorkingCity`: a plot entering/leaving the city's owned worked-radius
  set fires `CvCity::onCityPlotChanged(plot, ±1)` — the ONE applier, folding the plot's stored BITSET, so `plotAttrs`
  is literally the sum of the member plots' bits and the two granularities of one vocabulary cannot drift — and
  emits the `SEVT_WORKING_CITY_CHANGED` DOMAIN fact (every mutation emits; the contexts' consumer ignores play-time
  events, the choke-point fold having already applied). A MEMBER plot's bits moving reaches the counts through the
  same applier: the maintainer unfolds the old bits and refolds the new ones around every derivation.
- **`EmpireContext.policies`** ← the civic/trait change choke points (`CvPlayer::setCivics` / `setHasTrait` →
  `EmpireContext::rebuildPolicies`), which refills the WHOLE union over the player's live civics + held (active-set)
  traits. It is the single source the one policy read (`ev_playerHasPolicy`) uses — reads never re-walk the grantors.
- **Forwarded** fields need no maintenance — they read the live source.
- **Load** — `EmpireContext.policies` rebuilds from the loaded civics/traits at the end of `CvPlayer::read` (a derived
  aggregate recomputes from source on load, [DEC-derived-never-trusted](decisions.md#dec-derived-never-trusted), never
  trusted from a save). `CityContext.plotAttrs` builds from the in-read DOMAIN events
  ([DEC-spine-reseed](decisions.md#dec-spine-reseed)): each `CvPlot::read` announces its deserialized working-city
  fact (`SEVT_WORKING_CITY_CHANGED` — the genuine read site emits), and the contexts' OWN spine consumer
  (`Engine/ContextConsumer`, one consumer per system) buffers the load bracket's facts and folds them through the
  same applier (`CvCity::onCityPlotChanged`) at `GAME_LOAD_FINISHED` — the cities stream AFTER the map, so the fold
  applies once after the stream ends (the [enabler §7.1](../specs/enabler.md) order rule's second option, never the
  mixed form). There is never a blanket per-turn recompute.

## Scope set — plot / city / player now; units FUTURE (role-specific); no AreaContext (owner)

Contexts exist today on **plot, city, and player**. There is deliberately **no `AreaContext`**: an area is not
specific to any player — it is a bare **id**, "a really big plot" to reference. An area-scoped effect (**power** is
virtually the only driver of area-scope modifiers) maps onto the **player**, never an area context. Team is likewise
not a context.

**Units are a deliberate FUTURE scope, held off on purpose (owner).** A unit context must be **ROLE-SPECIFIC**: the
goal is that a unit no longer carries ALL the data (the ~247-field fat-unit problem) — each unit holds only the state
its role needs. Working out that role-partitioning is *why* it waits, rather than wiring a fat unit context now.

## The read — the per-GROUP valuation: `(CityContext, EmpireContext, CvPlotGroup)` → the group's values

An info's ACTUAL contextual output is read **one endpoint per GROUP of channels** (owner), never per single channel:
`expectedFlatYields` / `expectedYieldModifiers` / `expectedPlotYields` / `expectedFlatCommerce` / `expectedWellbeing`.
Each takes the three live contexts and fills that group's ×100 array — **you pass the contexts in, you get the group's
expected values out**:

- **CityContext** — vicinity + local state AND the river/water/… plot-attr COUNTS (`plotAttrs`). A building reads the
  CITY context for "how many river tiles", **never a PlotContext directly** (owner) — the plot-count sums live in the
  city context. It also answers the city's **traded** bonuses (through the city's own plot-group-backed reads).
- **EmpireContext** — the empire-scope state (civics/traits/policies/state religion).
- **CvPlotGroup** — the trade-network object; the reserved explicit **traded**-bonus source (`connection:"trade"` vs
  `"vicinity"`, [json.md §3.4](../specs/json.md)). Traded state is **NEVER mirrored into `CityContext`**. The
  valuation seam fills it into the eval ctx (`CvCascadeEvalCtx::plotGroup`): a `connection:"trade"` atom reads the
  city's own plot-group-backed maintained count when a city is bound (`CityContext::tradedBonusCount` — the
  tech-gate/minted/corp relay), and the passed group directly for the city-less what-if.

Each endpoint returns the UNCONDITIONED ×100 base PLUS every conditioned `m_cond` deposit whose condition holds — summed
via the **one** evaluator (`MMKernel::applies`) over a `CvCascadeEvalCtx` the contexts fill (`CityContext::fillEvalCtx`
= city/plot, `EmpireContext::fillEvalCtx` = player/team) — so the contexts ARE the eval state, not a raw-pointer ctx
built beside them. `expectedPlotYields` scales each plots-target deposit by `cityContext.plotAttrs.count(predicate)`.

> **Everything an info holds is ×100** ([DEC-fixedpoint-x100](decisions.md#dec-fixedpoint-x100)) — readJson converts
> human→×100 once at load; the info never de-scales; a reader `÷100`s at the point of use. So these endpoints add
> `value100` directly, and the materialized base members are `value100`.

> **Naming — no abbreviated parameters (owner).** Parameters are spelled in full (`cityContext`, `empireContext`,
> `plotGroup`), never `cx`/`pg`: short names are only defensible inside a tightly-scoped lambda, which C++03 lacks.
> Index parameters likewise name the enum they key (`YieldTypes eYield`, `DefenseKind eKind`), reusing the
> existing engine + family enums — a new family mints one typed enum, and the group's entries + its `expected*`
> array both key off it ([patterns.md § THE GETTER SETUP](patterns.md)).

## See also
- [patterns.md](patterns.md) — the INFO DATA-OUT contract + the per-group valuation surface that reads these contexts.
- [state-repositories.md](state-repositories.md) — the derived-cache (OUTPUT-value) plane; the contexts are the
  INPUT-state read surface (distinct: contexts hold input facts the getters/evaluator read, not cached output values).
- [../specs/modifier.md](../specs/modifier.md) — the deposits the getters sum; [../specs/enabler.md](../specs/enabler.md)
  — the availability machine that reads the same state; [decisions.md](decisions.md#dec-scope-contexts) — the ruling.
