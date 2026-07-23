# Patterns — interface contracts in C++03 (poor-man's DI)

> The concrete shape of [DEC-interface-contracts](decisions.md) under the frozen C++03/VC7.1 toolchain. Condensed
> from `composability.md` + `faking-di.md`.

## The interface shape (composability)

- A C++03 **interface** = an abstract base class with only pure-virtuals + a virtual dtor and **NO data members**
  (`IEventConsumer` is the realized model).
- **MI as `implements`:** one concrete satisfies several role-contracts via MI of their stateless interface bases —
  the compose-roles axis, **NOT** a DI substitute.
- **Two guardrails:** (1) MI **only** of stateless pure-virtual bases — MI of stateful concretes invites the
  diamond / layout / virtual-base mess; (2) graft interfaces onto the **DLL-internal derived** classes
  (`CvCityAI`/`CvUnitAI`), **never** onto EXE-bound bases (`CvCity`/`CvUnit` — the closed `.exe` binds their
  vtable/layout). The derived side is the safe lane and the lever for shrinking the god-classes.
- **Isolate-systems recipe:** when two systems entangle, give each its own data block + predicate query-surface,
  have both implement the one shared contract, and switch at the composition root. (Worked example: simple traits vs
  complex/Thunderbrd traits.)

## Poor-man's DI (faking-di)

No DI container exists (C++03/VC7.1; the EXE binds concretes), so:

1. Define the dependency as an **interface** (pure-virtual base, no data).
2. The consumer holds a **pointer to the interface**, never to a concrete.
3. At the **composition root**, a literal `if`/`switch` picks the concrete and assigns it — that `if`/`switch` is
   the manual "container." (Canonical use: game-option override-by-design swaps — one option check selects the impl;
   the consumer sees only the contract.)

- **Guardrails:** MI is not a DI substitute (you still inject via a base pointer); the decoupling is real even
  without a container ("no container" is never an excuse to `#include` the concrete into the consumer); the
  composition root is the **only** place that names concretes (a leaked concrete = the root is no longer the single
  wiring point).

## DRY — one implementation per calculation / evaluation (the single-source law)

> The law that keeps the cascade from becoming C2C again. C2C's decades-old disease is **N evaluators computing the
> same thing slightly differently**; this rule forbids it. Grounded in the reference impl: **StoneBase already has this
> separation** (one exposed unit per `Calc/*` package, one `ConditionEvaluator`) — the C++ port must carry it over, not
> flatten it. Binding: [DEC-single-implementation](decisions.md#dec-single-implementation).

**The law.** Every calculation and every evaluation exists **exactly once**, as a **pure static function fed its
inputs** (data + context → value), reachable by every consumer. No machine reimplements another's logic; a machine that
needs a fact FEEDS it to the one function, it never re-derives it.

1. **One evaluator for conditions/predicates.** `cascadeEvalCondition` is the **sole** place a condition/predicate is
   evaluated. The enabler and the modifier **delegate** to it (`en_requiresMet`, `mm_applies` are thin wrappers) — they
   never re-read a predicate. A machine that needs a fact the evaluator uses (`hasVicinityBonus`/`isGovernmentCenter`/
   active-building) **supplies it through the eval context** (the precomputed operating-building set), never evaluates it itself.
   *(Holds today — one evaluator, both machines delegate; the old `BoolExpr` duplicate was deleted.)*
2. **One function per calculation**, mirroring StoneBase's `src/Application/Features/Calc/*` packages **1:1**:
   `PercentStack` · `YieldBasePackages` · `YieldRate` · `YieldSplit` · `CommerceSplit` · `CommercePackages` ·
   `BuildingPackage` · `CalcContributions`. No parallel or near-duplicate calc anywhere.
3. **Pure static functions, no hidden state.** A calculator/evaluator takes everything it needs as parameters and
   returns a value; it holds **no data members** — data lives in the `InfoRepo`, counts in the tally. That purity is
   *why* one implementation is callable everywhere: it **is** the DRY guarantee. **Grouping them is fine and encouraged**
   — as a **purely-organizational static-methods class** (a named holder, à la StoneBase's `static class PercentStack`):
   **no data members, never instantiated, no per-instance state.** **Use a static-methods class, NOT a namespace**:
   namespaces can produce funky name-mangling under the frozen VC7.1 toolchain + Boost / `boost::python`
   bindings + the closed EXE ABI; a static class sidesteps it. The container is *organization only*. Forbidden: an
   instance, any member field, a namespace grouping, or a file-`static` function no other unit can reach.
4. **Exposed, never file-`static`-hidden.** Each calculator/evaluator is a **declared surface** (a header) reachable by
   every consumer. **A file-`static` calculator is a DRY hazard**: the next consumer can't see it, so it reimplements it
   — the exact mechanism of the C2C rot. *(Realized: BOTH data-machines are split into per-package
   static-methods classes — the **deposit-read side** (`MMKernel` / `PercentStack` / `YieldBasePackages` / `BuildingPackage` /
   `CommerceCalc` -- declared in `Data/CvDepositRead.h`, mirroring StoneBase `Calc/*`) and the **enabler**
   (`EnablerKernel` + `TechEnabler` / `BuildingEnabler` / `UnitEnabler` / `CivicEnabler` / `ProcessEnabler` /
   `ProjectEnabler` / `PromotionEnabler` / `BuildEnabler`, each `Sources/Enabler/Cv<X>Enabler.{h,cpp}`,
   mirroring StoneBase `CascadingEnabler/*`).)*
5. **Harness ≠ calc.** The performance/observability surface (the StoneBase dashboard) and the spine logging are
   **separate consumers** of the calc surface, never folded into the calc functions.
6. **Single source of "active".** "Is X active / available / connected / non-dormant" is computed **once, by the
   enabler**; the modifier **reads** it — it never recomputes from the live engine, and above all never reads the
   engine's *dormancy verdict* (the camouflaged ride-in, [DEC-calc-zero-ride-in](decisions.md#dec-calc-zero-ride-in)).
   *(Realized for building active/dormant — `EnablerKernel::recomputeOperatingBuildingsInto` derives it from
   `requires.operate` + dormant triggers into `CvCascadeEvalCtx::activeBuildings` (the precomputed operating-building
   set, twin of `waivedPrereqBuildings`); the modifier + evaluator read `cascadeIsBuildingActive`, never
   `isActiveBuilding`. Vicinity-`provides` (an active building providing a bonus ⇒ in-vicinity, json §5a) is
   likewise computed — `vicinityProvidedBonuses`, filled with `activeBuildings` in one
   `recomputeOperatingBuildingsInto` pass feeding both machines.
   Two active-states stay ENGINE-OWNED inputs the modifier reads (not cascade-computed), because the engine drives
   them and the cascade does not model the driver: the route/trade `CONNECTED` "obtained" case (the network we don't
   model), and **CORPORATION active/dormant** (engine-driven per-turn spread state, like religion — `isActiveCorporation`,
   owner ruling; [culture-religion-research.md](../reference/culture-religion-research.md) Corporations). These are the
   sanctioned-input class, distinct from the BUILDING dormancy verdict the cascade owns.)*
7. **No duplication is sanctioned.** During the migration the legacy shadow was the one sanctioned duplication (the
   cascade running *alongside* legacy, diffed, with a defined death — [DEC-map-before-delete](decisions.md#dec-map-before-delete));
   **the shadow phase has ended** ([validation](../specs/validation.md)), so no duplication is sanctioned at all.
8. **Composition root names concretes** ([DEC-interface-contracts](decisions.md#dec-interface-contracts)) — the
   active-set / game-option swaps are picked there; a leaked concrete `#include` into a consumer breaks the single wiring point.

**Enforcement (how to keep certainty).** The data-machine trees (`Sources/Data/`, `Sources/Conditions/`,
`Sources/Enabler/`) should read like `StoneBase/src` — one unit per `Calc` package, one evaluator. To verify: grep for a second implementation of any calc/predicate; confirm every
machine's condition gate routes through `cascadeEvalCondition`; confirm no calculator holds state. **A new
"does-the-same-thing" function is the failure** — reuse the existing one, or lift it to the shared surface. This is the
anti-rollerskate check an agent runs before adding cascade calc/eval code.

## The INFO DATA-OUT contract — what an info hands to the cascade

> The **infos** row of [EACH IS ITS OWN SYSTEM](north-star.md): readJson puts data into infos, infos SERVE that
> data, the cascade sums, the enabler resolves availability. This section is that row's concrete surface.
> **It is stated as a CONTRACT, not a prohibition** — a prohibition has to be remembered by every future agent,
> the enforcement model this project keeps watching fail; a contract makes the violation unsayable rather than
> forbidden, because there is no member to write to.

**An info is a pure DATA SOURCE with one outbound surface.** It is loaded once, immutable thereafter, and shared
by every player — so it can carry authored data and nothing else. Concretely:

1. **What an info holds** — the availability model (the `enables` family, `requires`/`allowed`, the load-derived
   reverse edge families) and its own authored modifier data, resolved to typed members at `mapFrom`.
2. **What an info hands out** — its data, ASKED FOR BY CHANNEL: *"give me your flats / your percents for these
   channels."* The cascade points at a LIST of infos and sums what comes back. It never reaches inside an info's
   per-type shape, and an info never learns what a cascade, a scope, or an owner is.
3. **What an info CANNOT hold** — per-owner state, a computed total, a dirty flag, a cache. Not by rule: by
   construction. There is nowhere on the object to put it, because the outbound surface is the only surface.

**Why the boundary is load-bearing, not tidiness.** An info is write-once-at-load and shared; cascade runtime is
per-owner mutable derived state. Storing the latter on the former silently makes an immutable, shared object
mutable **per game rather than per load** — and it is the third copy of the same static numbers, after the
authored JSON and the compiled deposit index.

**The failure this closes.** Asking each info type for its data through a DIFFERENT accessor is the same defect as
a hand-named scalar per channel ([DEC-uniform-cache-shape](decisions.md#dec-uniform-cache-shape)): it cannot be
addressed uniformly, so every type needs bespoke read code, and the cascade ends up shaped by the info surface
instead of the other way round.

### An info is STYLED FOR THE JSON, not the legacy field set (owner)

The info's MEMBERS mirror the **JSON entity anatomy** ([json.md §2](../specs/json.md): availability ·
provisions · effects = the modifier families · intrinsic · classification · auxiliary), each held as its proper
typed structure. It is **not** a scalar-per-legacy-XML-field. The turnaround is the whole of "make the infos sane":
the JSON model drives the info's shape; the legacy variable set is gone, not force-fed.

- **The realized exemplar is already in-tree — generalize it.** The classification blocks are styled for the JSON:
  `m_attributes` is a **JSON-derived bitset** (the `ClassificationRegistry` ids minted from the authored
  `attributes` block, [DEC-classification-infos](decisions.md#dec-classification-infos)), and `isZoneOfControl()`
  is `CLS_HAS(m_attributes, "zoneOfControl")` — a coherent read over that structure, never a legacy
  `m_bZoneOfControl`. Every block gets this shape.
- **The defect the rebuild removes** is the legacy-named scalar-per-field with a comment mapping it back to a JSON
  address (`m_iDamageToAttacker` ← `defense.city.counterDamage.damage`; `m_aiRiverPlotYieldChange[]` ←
  `<yield>.city.plots` flats). Those are JSON parsed and **scattered into individually-named legacy variables**;
  the sane form holds the JSON structure and reads it, so a new field is DATA, not a new member + getter.

### Getters are ×100-native and coherent — the scale is never in the name (owner)

- **Every getter IS ×100** ([DEC-fixedpoint-x100](decisions.md#dec-fixedpoint-x100)). There is **no `getX`/`getX100`
  pair and no `100` suffix on any name** — a getter names its VALUE, never its scale, because the scale is
  universal (the answer is always ×100). The 5 surviving `…100()` names lose the suffix and become the single
  getter for that value; a reader wanting human does ÷100 at the boundary.
- **The surface is coherent, not per-field.** Modifier data is handed out **by channel** (the data-out contract
  above); classification by `CLS_HAS`; intrinsic values by a bare typed-member read. The ~300 hand-named getters on
  a fat poco ARE the legacy `CvXInfo` contract; the coherent surface replaces them and the consumers rewire onto it
  ([DEC-new-getter-surface](decisions.md#dec-new-getter-surface)) — the info half of the access surface, not a
  follow-up to it.

## Materialize at mapFrom — no runtime string reads in info getters (the single-source law's load-time sibling)

> Binding: [DEC-materialize-at-mapfrom](decisions.md#dec-materialize-at-mapfrom). Owner ruling: *"all of these should
> use the standardized jsonreader and be loaded properly into the info — remapping directly from a json read is a
> gigantic nono."*

**The law.** A `CvJson<X>Info` GETTER never does a per-call string-keyed read — no modifier-address sum
(`"happiness.city"` lookups), no bool-block `std::set<string>` walk, no grants/allowed bucket-string fetch, no raw
picojson re-read. Every such value is **materialized ONCE at `mapFrom`** into a typed member (scalar, positional
array, sparse id-keyed map, or a classification-id bitset), and the getter is a **bare member read**. The measured
why: these getters sit under the EXE frame loop (`unit.isInvisible` ~98M calls/turn-window), the pathfinder's
per-step gates, and the AI's per-candidate scans — a heap-string construction + map walk per call was a real
turn-time/FPS tax.

- **The ONE load-time scan surface is `JsonModScan`** (`Sources/Infos/CvJsonModScan.{h,cpp}`) — the
  unconditioned/keyed/condition-shape family walkers, shared by every poco's materialization pass (the per-file
  `civSum*`/`sumUnconditioned` duplicates are gone). It is **load-time only**: a getter never calls into it.
- **Classification blocks read by GENERATED ID** — the §8/§9 bool blocks resolve their keys to the
  `ClassificationRegistry`'s runtime-minted ids ([DEC-classification-infos](decisions.md#dec-classification-infos)),
  and the getters are `CLS_HAS`/`CLS_COUNT` bit tests (memoized id + O(1) bitset read; the pre-resolve load window
  falls back to the string set so early consumers stay correct).
- mapFrom is idempotent by contract, so the materialized members are fully redefined on every (re-)map —
  clear-first for accumulating containers, unconditional assignment for scalars.
- The cascade's own gated sums are NOT this surface — they are `MMKernel` over the compiled `DepositIndex`,
  running at dirty-rebuild cadence, not per read.
