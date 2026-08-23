# Patterns — interface contracts in C++03 (poor-man's DI)

> Depend on interfaces, not concretions — the concrete shape below under the frozen C++03/VC7.1 toolchain.

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
> flatten it. Binding: [the DRY single-implementation law](#dry--one-implementation-per-calculation--evaluation-the-single-source-law).

**The law.** Every calculation and every evaluation exists **exactly once**, as a **pure static function fed its
inputs** (data + context → value), reachable by every consumer. No machine reimplements another's logic; a machine that
needs a fact FEEDS it to the one function, it never re-derives it.

1. **One evaluator for conditions/predicates.** `cascadeEvalCondition` is the **sole** place a condition/predicate is
   evaluated. The enabler and the modifier **delegate** to it (`EnablerKernel::requiresMet`, `MMKernel::applies` are
   thin wrappers) — they
   never re-read a predicate. A machine that needs a fact the evaluator uses (`hasVicinityBonus`/`isGovernmentCenter`/
   active-building) **supplies it through the eval context** (the precomputed operating-building set), never evaluates it itself.
   ⚠ `BoolExpr` still exists and still serves the KEEP-legacy property engine — it is not the cascade evaluator,
   and translating a `CvCondition` back into one so another solver can evaluate it is a SECOND evaluation surface,
   whatever it is named.
2. **One function per calculation**, mirroring StoneBase's `src/Application/Features/Calc/*` packages **1:1**:
   `PercentStack` · `YieldBasePackages` · `YieldRate` · `YieldSplit` · `CommerceSplit` · `CommercePackages` ·
   `BuildingPackage` · `CalcContributions`. No parallel or near-duplicate calc anywhere.
3. **Pure static functions, no hidden state.** A calculator/evaluator takes everything it needs as parameters and
   returns a value, holding **no data members** — data lives in the `InfoRepo`, counts in the tally; that purity
   *is* the DRY guarantee. Grouping is fine as a **static-methods class** (à la StoneBase's `static class
   PercentStack`), never a namespace (funky name-mangling risk under VC7.1/Boost/`boost::python`). Forbidden: an
   instance, any member field, a namespace grouping, or a file-`static` function no other unit can reach.
4. **Exposed, never file-`static`-hidden.** Each calculator/evaluator is a declared surface (a header) reachable by
   every consumer — a file-`static` calculator is a DRY hazard the next consumer can't see, so it reimplements it,
   the exact mechanism of the C2C rot. *(Realized: the deposit-read side — `MMKernel` (the per-deposit leaf
   primitives), `Data/CvDepositRead.h`; `InfoValuation`, `Data/CvInfoValuation.h`, carrying StoneBase's `Calc/*` packages (the per-group walk, `YieldRate`
   §2a's `cityRate` combine, `CommerceSplit`'s `commerceSplit`, the plot-as-base package, the cross-scope roll-up)
   — and the enabler (`EnablerKernel` + `TechEnabler`/`BuildingEnabler`/`UnitEnabler`/`CivicEnabler`/
   `ProcessEnabler`/`ProjectEnabler`/`PromotionEnabler`/`BuildEnabler`, `Sources/Enabler/Cv<X>Enabler.{h,cpp}`,
   mirroring StoneBase `CascadingEnabler/*`) are both split this way.)*
5. **Harness ≠ calc.** The observability surface and the spine logging are
   **separate consumers** of the calc surface, never folded into the calc functions.
6. **Single source of "active".** "Is X active / available / connected / non-dormant" is computed **once, by the
   enabler**; the modifier reads it, never recomputing from the live engine or the engine's *dormancy verdict*
   (the camouflaged ride-in, [the pollution guardrail](../specs/validation.md#the-pollution-guardrail--engine-computed-data-never-rides-in)). *(Realized: for
   buildings, `EnablerKernel::recomputeOperatingBuildingsInto` derives active/dormant from `requires.operate` +
   dormant triggers into `CvCascadeEvalCtx::activeBuildings` (twin of `waivedPrereqBuildings`), read via
   `cascadeIsBuildingActive` (never `isActiveBuilding`); the same pass fills `vicinityProvidedBonuses` for
   in-vicinity `provides` (json §5a). Two
   states stay ENGINE-OWNED inputs instead, because the cascade does not model their driver: route/trade
   `CONNECTED` (the network), and CORPORATION active/dormant (per-turn spread state, like religion —
   `isActiveCorporation`; [culture-religion-research.md](../reference/culture-religion-research.md)).)*
7. **No duplication is sanctioned.** During the migration the legacy shadow was the one sanctioned duplication (the
   cascade running *alongside* legacy, diffed, with a defined death — [the map-before-delete discipline](../../AGENTS.md#cascade-observability--the-total-observability-orwell-bar));
   **the shadow phase has ended** ([validation](../specs/validation.md)), so no duplication is sanctioned at all.
8. **Composition root names concretes** ([the interface-contracts pattern](#the-interface-shape-composability)) — the
   active-set / game-option swaps are picked there; a leaked concrete `#include` into a consumer breaks the single wiring point.
9. **⛔ ONE PATH PER MUTATION — EVENTS AND WORLDBUILDER GO THROUGH THE SAME ONE (owner).** The law above governs
   reads and evaluations; it governs WRITES identically. A given state change has ONE published path, and every
   caller uses it: a random event granting a tech, a WorldBuilder screen setting it, and any other mutator are the
   SAME call, never a per-caller variant.
   ⚑ **The reason is that a second path is how the two drift into disagreeing about what the mutation MEANS** —
   one remembers to announce the crossing, refresh the dependents or refcount the grantor and the other does not,
   so the editor produces a state the game can never reach and a bug reproducible only through one of them. That
   is the C2C disease in write form, and it is worse than the read form: a divergent read is wrong, a divergent
   write is CORRUPTING.
   ⚠ **A WorldBuilder caller is not licence to bypass the path** because "it is only an editor". If a mutation is
   safe to perform, it is safe through the shared path; if the shared path refuses it, the editor must not be
   doing it either. ⇒ Where an editor genuinely needs a capability gameplay lacks, that is a MISSING VERB on the
   shared surface to add deliberately, never a private setter beside it.

**Enforcement (how to keep certainty).** The data-machine trees (`Sources/Data/`, `Sources/Conditions/`,
`Sources/Enabler/`) should read like `StoneBase/src` — one unit per `Calc` package, one evaluator. To verify: grep for a second implementation of any calc/predicate; confirm every
machine's condition gate routes through `cascadeEvalCondition`; confirm no calculator holds state. **A new
"does-the-same-thing" function is the failure** — reuse the existing one, or lift it to the shared surface. This is the
anti-rollerskate check an agent runs before adding cascade calc/eval code.

## The INFO DATA-OUT contract — what an info hands to the cascade

> **This section is the home of [parsing/holding info data is info-side, never cascade-side](#the-info-data-out-contract--what-an-info-hands-to-the-cascade)** — parsing and holding
> the info data is INFO-side, and cascade runtime never lives on an info.
>
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
3. **What an info CANNOT hold** — per-owner state, a computed total, a staleness flag, a cache. Not by rule: by
   construction. There is nowhere on the object to put it, because the outbound surface is the only surface.

**Why the boundary is load-bearing, not tidiness.** An info is write-once-at-load and shared; cascade runtime is
per-owner mutable derived state. Storing the latter on the former silently makes an immutable, shared object
mutable **per game rather than per load** — and it is the third copy of the same static numbers, after the
authored JSON and the compiled deposit index.

### ⛔ WRITE-ONCE-AT-LOAD — A READ NEVER CREATES, AND AN UNANSWERABLE READ FAILS LOUD

> Binding: [the info plane is write-once-at-load](#-write-once-at-load--a-read-never-creates-and-an-unanswerable-read-fails-loud). The read-side twin of
> [exactly one JSON reader](#the-one-reader--the-load-pipeline-law) — that rule says there is exactly ONE writer; this one
> says everybody else is a reader, and says what a reader does when it cannot be served.

**Two access paths, and they are not interchangeable.** `InfoRepo::edit`/`editPtr` are **get-or-create**: they
resize the array and `new` a payload for whatever id they are handed. That is correct for the LOAD pipeline, which
is what they were written for, and they belong to exactly three callers — the one reader (`loadJson`), the reverse
pass, and the classification registry. **Every other caller is a READ and uses `atPtr` / `get`.** A read that
creates violates all three of the contract's clauses at once: it writes to the plane outside the one writer, it
does work on a path specified as a bare fetch ([state-repositories.md](../cascade.md)), and it answers
quietly where it should fail ([legacy must fail loud, never mask a cascade gap](../specs/validation.md#legacy-must-fail-loud-never-mask-a-cascade-gap)).

**What get-or-create actually answers with is a BLANK info, and that is why this is a hard rule rather than a
preference.** A blank is indistinguishable from a real one until something asks it a question — and then
`CvInfoBase::getType()` returns **NULL**, which crosses into the EXE's UI and into boost::python and is
dereferenced *there*. The failure therefore surfaces in someone else's frame, as an access violation at an address
in the EXE's image, with nothing left pointing back at the id that caused it. ⚠ **That address is the bait:** it
reads as an EXE defect, and an agent who stops there is chasing the closed binary for a NULL we supplied.

**⛔ Worse on an ALIASED repo, which is most of them.** The backing IS `GC.m_pa<X>Info`, and `getNum<X>Infos()`
returns that vector's `size()` — so creating on read **moves the registry's own bound**. Every bounded walk over
the registry then runs off into the entries the walk itself created; the observed signature is a walk that never
terminates and dies as an out-of-memory rather than as a bad id. ⚠ And the bounds assert that looks like it guards
this (`FASSERT_BOUNDS`) is **compiled out of `Release`/`FinalRelease`** (`fbuild.bff`), i.e. absent from every
build the game is actually played in — so a fail-loud here is a real function, never an assert.

**⚖ THE RULING (owner): crash at the main menu because things are not loaded, rather than manually incrementing
the registry to limp past it.** An unanswerable read is a LOAD defect — some pass did not fill the slot — and the
only useful thing to do with it is name it, at the bad read, while the registry and the id are still known. It is
reported into `Exceptions.log` beside the handler's own entries and into `Loading.log` beside the `[READJSON]`
census that built the plane, then raised NONCONTINUABLE so the ordinary unhandled path writes the minidump and the
Python callstack.

**⛔ And the corollary that is easiest to get wrong: DO NOT DEFER THE READ TO MAKE THE FAILURE QUIET.** Moving a
read later — screens built on first use instead of at `earlyInit`, a plane consulted lazily "to get to the menu" —
initializes *nothing*. It relocates the failure from a named Python `AttributeError` at the menu to an access
violation deep in the interface, and buys the appearance of a load that got further. **A load that reaches further
by asking fewer questions has not got further.** Screens are therefore constructed eagerly, on the engine's entry
path, precisely so an incompletely stood-up info plane fails where the failure is legible
([python-load-sequence.md](../reference/python-load-sequence.md)).

**The failure this closes.** Asking each info type for its data through a DIFFERENT accessor is the same defect as
a hand-named scalar per channel ([every derived cache is one shape](../cascade.md#-every-derived-store-is-one-shape--a-keyed-accumulator-maintained-by-a-delta-owner)): it cannot be
addressed uniformly, so every type needs bespoke read code, and the cascade ends up shaped by the info surface
instead of the other way round.

### An info is STYLED FOR THE JSON, not the legacy field set (owner)

The info's MEMBERS mirror the **JSON entity anatomy** ([json.md §2](../specs/json.md): availability ·
provisions · effects = the modifier families · intrinsic · classification · auxiliary), each held as its proper
typed structure. It is **not** a scalar-per-legacy-XML-field. The turnaround is the whole of "make the infos sane":
the JSON model drives the info's shape; the legacy variable set is gone, not force-fed.

- **The exemplar is the classification block — generalize it.** `m_attributes` is a **JSON-derived bitset** (the
  `ClassificationRegistry` ids minted from the authored `attributes` block,
  [the classification-infos registry](../specs/json.md#8-classification--unit-skillstagsstate-building-attributes--empire-capabilities)), read by the parameterized
  `CvInfo::hasAttribute(id)` over `clsHasId` — never a legacy `m_bDestroyedOnCapture`, and never a named per-key
  body. Every block gets this shape, one member per block the entity authors (`m_attributes` beside
  `m_amenities` on a building, [json.md §8](../specs/json.md)).
- **The defect the rebuild removes** is the legacy-named scalar-per-field with a comment mapping it back to a JSON
  address (`m_iDamageToAttacker` ← `defense.city.counterDamage.damage`; `m_aiRiverPlotYieldChange[]` ←
  `<yield>.city.plots` flats). Those are JSON parsed and **scattered into individually-named legacy variables**;
  the sane form holds the JSON structure and reads it, so a new field is DATA, not a new member + getter.
- **An info holds only ITS OWN side — cross-entity own-output lives on the TARGET (owner).** A building does not
  project yield onto an improvement; the **improvement** says *"I produce this much now, because a building is
  present"* — own-output, the building's presence a condition on the improvement's own deposit ([the deliveryguy ownership rule](../cascade.md#4-ownership--the-deliveryguy-rule),
  modifier.md §4). So `CvBuildingInfo` carries no `improvements`/`terrains` yield map. This needs **no curator
  re-home**: the **load-time reverse structure** ([reverse lookups are populated once, at load](../cascade.md#1-one-step-deposit-down-accumulate-read-o1)) builds cross-entity links both ways at
  readJson, so a modder may author *either* side and the relationship is landed on the other programmatically — the
  improvement ends up owning its yield regardless of which side authored it. A target-keyed map survives on the
  source **only** where the source is the genuine deliverer with no target-owner (governing-deliverer, modifier.md §4).

### The coherent surface — grouped storage, parameterized getters (owner: CLARITY AND PREDICTABILITY IS KING)

The numbers and the booleans are **organized into named groups, each read by ONE getter parameterized over the
group's natural index** — never N individual getters for a groupable set. This is the whole shape of a sane info;
`getYield(YIELD)` is right, `getFoodYield()`/`getProductionYield()`/… is the disease, and so is
`isNukeImmune()`/`isZoneOfControl()`/… for the boolean blocks.

- **The AUTHORED form is the JSON anatomy; the ANATOMY WALK IS LOAD-ONLY.** The [json.md §6](../specs/json.md)
  deposit model — per family, the [§3.9](../specs/json.md) entries under their FULL five-axis address
  `<family>.<scope>[.<target>|.<targetType>.{TARGET}][.<member>].<unit>` — is what the reader parses, with
  **every string key interned to a typed id** (family/member → the shared kind-enum vocabulary, scope → the
  scope enum, named-entity targets → FK-resolved ids, conditions → parsed trees;
  [materialize at mapFrom](#materialize-at-mapfrom--no-runtime-string-reads-in-info-getters-the-single-source-laws-load-time-sibling)), nothing flattened away, the §3.9
  mechanism UNREDUCED (`per`, the `ai` sibling, the `enabled`/`disabled` twin trees in their spec'd order —
  `enabled` first, a holding `disabled` OVERRIDES; a plural-target filter is the entry's own `enabled`
  predicate, [json.md §6.1](../specs/json.md)).
- **The ONE load COMPILE pass walks those entries ONCE and produces the runtime forms — after load, nothing
  ever walks the anatomy.** A **null-condition entry's value folds STRAIGHT into its group's compiled member
  array** — the enum-keyed `[kind × family-scope-set]` unconditioned ×100 sums, the grouped member pattern,
  scope-free kind names (Σflat vs Σpercent separate slots — the unit is part of the slot key,
  [modifier.md §2](../cascade.md)). A **conditioned entry** lands in the group's compiled conditioned
  list, its condition tree prebuilt, evaluated ONLY at event-driven package rebuild and the per-decision
  `expected*` read — never re-parsed, never re-derived. Classification compiles to JSON-derived bitsets
  (`m_attributes` / `m_capabilities` / `m_skills` / `m_policies`); edges to the load-populated forward/reverse
  families ([reverse lookups are populated once, at load](../cascade.md#1-one-step-deposit-down-accumulate-read-o1)); intrinsic lone values to plain typed
  members. No string, no parse node, and no anatomy tree survives into a runtime read path.
- **⛔ THE SCOPE AXIS — a kind-enum names its CONCEPT ONLY; scope is a separate dimension**
  ([scope is a separate axis, never folded into the kind](#the-coherent-surface--grouped-storage-parameterized-getters-owner-clarity-and-predictability-is-king)). Scope is its own axis of the deposit address + a
  spelled-out getter parameter, NEVER a fragment of an enum, member, or getter name — a scope word (`GLOBAL`,
  `ALL_CITY`, `WORLD`, `AREA`, …) inside a kind name collapses two of the address's axes into bespoke per-pair
  entries. `getDefense(DEFENSE_AMOUNT, SCOPE_CITY)` — kind and scope are separate arguments, exactly as the
  JSON's own `<family>.<scope>.<member>` separates them.
- **⚖ THE DECISION PROTOCOL IS TWO STAGES, IN ORDER (owner): *"first it should ask enabler what is possible,
  and then it asks cascade 'what if I do this?'"*** Every AI decision is that pair — the ENABLER narrows to the
  candidates ([enabler.md §6](../specs/enabler.md): the frontier is the shared choice set, iterated instead of
  the entity database), and the CASCADE values each survivor through the what-if. ⛔ Neither half substitutes
  for the other: scoring what cannot be done wastes the expensive half on candidates the cheap half would have
  dropped, and gating without valuing picks an option the AI cannot weigh. ⚑ **This is what makes the what-if
  affordable at all** — it runs over a small maintained set, never over everything.
- **THE WHAT-IF DRIVER — the AI's planning asks are STRAIGHT RESPONSES, 0 calculation (owner).** The two
  most-asked questions in the engine both answer from compiled structures: *"what can I do next after getting
  this?"* is the FUNDAMENTAL enabler-tree read — the info's load-compiled `enables`/reverse edge families + the
  enabler's maintained domain vectors, a pure list fetch ([enabler.md §7](../specs/enabler.md): every read is an
  O(1) lookup that never calls a calculator; the tree is conditional-free by design). **The ONE calculation in
  that whole flow is the `requires` gate** — very few things have a single prerequisite, so a newly-proposed
  candidate is confirmed against its remaining prerequisites — **and it runs at HAVE-CHANGE time**, over only the
  affected candidates via the `EDGEF_REQUIRED_BY` re-gate ([enabler.md §7.1](../specs/enabler.md)), never at ask
  time: when the AI asks, the verdict already sits in the tri-state vector. *"What do I gain from building
  this?"* fetches the compiled unconditioned sums straight — one load per slot — and only the compiled
  CONDITIONED tail is ever evaluated (through the ONE evaluator against the contexts,
  [contexts.md](../cascade.md)), at per-decision cadence in the `expected*` read. The entity-level active/dormant
  verdict stays the ENABLER's, fed in via the precomputed operating set — a what-if read never re-evaluates
  `requires`.
- **THE GETTER SETUP — one exemplar shape for every info (the aim). Four read categories, nothing else:**
  1. **Sections** — whole typed objects the enabler + grants/provides machinery read: `getRequires()` /
     `getEdges()` / `getAllowed()` / `getGrants()` / `getProvides()` / `getWhenObsolete()`.
  2. **Classification** — O(1) bitset tests, the **name encoding hold-vs-provide** (owner, json.md §8): what the
     entity HAS is `hasAttribute(id)`/`hasAttributes()` (building) and `hasSkill(id)`/`hasTag(id)` (unit); what it
     PROVIDES to something else is `providesCapability(id)`/`providesCapabilities()` (to the empire) and
     `providesSkill(id)` (a grantor handing a skill on).
     > **⛔ THE PARAMETERIZED READ IS THE CONSUMER SURFACE — a consumer NEVER asks for a key by name (owner).** A
     > consumer asks `kUnitInfo.hasSkill(CLS_SKILL_BLITZ)`; the id is a compile-time constant from the
     > **generated** `Infos/CvClassificationIds.h`. Seven reads on `CvInfo` cover every domain
     > (`hasSkill` / `hasTag` / `hasAttribute` / `providesAmenity` / `hasCharacteristic` / `providesCapability` /
     > `providesPolicy`, plus `revokesSkill` for the §4 revoke plane), each a NULL-safe `clsHasId` over the
     > block — so a block-less info answers FALSE and the read is total.
     > ⛔ **A new authored key is a REGENERATED TABLE ENTRY, never a new function.** Do not add a named getter,
     > and do not re-introduce a per-key read class: that shape is what this replaced.
     > ⚑ **How the open registry still hands out a compile-time id:** the blocker was the id ORDER being
     > DISCOVERED at load, not the openness. `Tools/Migration/curate_classification_ids.py` pins the order (as
     > `curate_order.py` pins `_order.json`); `ClassificationRegistry::buildAndResolve` SEEDS from that table
     > before minting, so the constant IS the runtime id. The category stays OPEN
     > ([the classification-infos registry](../specs/json.md#8-classification--unit-skillstagsstate-building-attributes--empire-capabilities)): an unlisted key still mints at load,
     > appended after the seeded block — regenerating just promotes it to a constant, save-neutral since nothing
     > serializes.
     > ⚠ **The constants carry a reserved `CLS_` namespace** (`CLS_TAG_MILITARY`) distinct from the runtime
     > INFOTYPE STRING (`TAG_MILITARY`): the property engine owns global `TagTypes`/`AttributeTypes` in
     > `CvEnums.h`, and an OPEN registry could otherwise collide with any engine enumerator.
     > ⚑ A **spec-named key that no data authors yet** (skills.md §1/§2 headroom — `noSelfHeal`, `freeDrop`, …) is
     > seeded from the generator's `HEADROOM` list so a legitimate consumer still compiles and reads false, until
     > data authors it.
     >
     > ⛔ **THE INFO-SIDE NAMED `CLS_HAS` BODIES ARE GONE TOO — the whole per-key shape is retired, both halves.**
     > ⚠ The names permanently COLLIDE with live game-object methods: `isCapital` is a building AMENITY *and*
     > `CvCity::isCapital`; `isNukeImmune` is a building amenity, a plot-substrate CHARACTERISTIC *and*
     > `CvUnit::isNukeImmune`; `isGovernmentCenter` is an amenity *and* a `CvCity` method. The RECEIVER decides
     > which one a call site means, never the name — a textual sweep destroys the game-object half. Convert
     > receiver-by-receiver, with an allowlist of info-typed receivers.
     >
     > **⚖ AND FOR AN AMENITY THE RECEIVER IS THE QUESTION ITSELF (json.md §8, [contexts.md](../cascade.md)).** An
     > amenity is CITY-HELD, grantor-PROVIDED: *"does THIS CITY have it"* is answered by the city's FOLD
     > (`CityContext::hasAmenity`), never by asking a grantor — re-pointing such a gate at
     > `kBuilding.providesAmenity(...)` would leave it unchanged while reading as migrated. ⚑ The converse holds
     > for most sites: an AI VALUATION and a FOLD APPLY (`processBuilding`) legitimately ask the GRANTOR, since
     > the candidate's own block IS the answer there. Classify by the QUESTION: gate → the city; valuation/apply/
     > display → the grantor.
  3. **Modifier groups — three reads per group, all over the LOAD-COMPILED forms:**
     - the **straight point read** over the compiled unconditioned sum — `getDefense(DefenseKind eKind,
       ScopeKind eScope)` → one array load, **0 calculation** (kind and scope separate arguments,
       [scope is a separate axis, never folded into the kind](#the-coherent-surface--grouped-storage-parameterized-getters-owner-clarity-and-predictability-is-king));
     - the **compiled conditioned list** (`defenseConditioned()` / `yieldConditioned()` / … — the typed entries
       with prebuilt condition trees; what the package rebuild, the pedia, and the valuation walk);
     - the **what-if valuation** — the [contexts.md](../cascade.md) per-GROUP endpoints
       (`expectedFlatYields(cityContext, empireContext, plotGroup, flatYields)` and siblings): the compiled
       sums fetched straight PLUS the group's conditioned tail through the ONE evaluator, `plots`-targets scaled
       by `cityContext.plotAttrs`, scopes folded into the experienced-here answer, the active/dormant verdict fed
       from the enabler. This IS the AI's *"what do I gain from building this?"* read.
  4. **Intrinsic** — bare typed reads (`getAirlift`, the shrine/corpHQ FKs, flavours), plus `getScalar(SCALAR_X)`
     for the 1–2-entry stragglers (genuinely lone unconditioned values).
     > **⛔ A TEXT read NAMES WHICH SIDE OF THE BOUNDARY IT IS ON — `*Key()` returns a TXT_KEY, the bare form
     > returns RESOLVED TEXT (owner).** *"Update these text namings to actually specify that you are getting a
     > key, not the actual text — so we are clear when text is fetched, or when key is fetched."* TXT is an
     > unmigrated system the JSON only REFERENCES ([json.md §7](../specs/json.md)), so an INFO holds keys and
     > resolution belongs to the text manager; a name that hides which one you are holding is how a raw key ends
     > up rendered to a player, or a resolved string ends up fed back into `getText`.
     > ⚑ The convention is already the tree's: `getCivilopediaKey`/`getHelpKey`/`getStrategyKey`/
     > `getShortDescriptionKey`/`getAdjectiveKey` return keys beside the `DllExport` bare forms that return text.
     > ⚠ The four bare EXE-bound reads (`getTextKeyWide`, `getDescription`, `getText`, `getHelp` on
     > `CvInfoBase`) are FIXED BY ABI and are not renameable — check `DllExport` before proposing any text
     > rename ([engine.md § Is a symbol really EXE-bound?](../reference/engine.md)).
  5. **The per-entry TEXT render (owner: "so that tooltips work properly")** — every compiled entry renders
     itself as ONE localized detail line (`+25% Production — while Coal connected`), the `detailLines` pattern
     of the combat calculator (`CvCombatModel::computeCombatPreview`'s itemised per-modifier breakdown),
     through ONE shared renderer ([the DRY single-implementation law](#dry--one-implementation-per-calculation--evaluation-the-single-source-law)) — the
     tooltip/pedia composers consume rendered entry lines, never hand-assemble from getters. Cold path:
     spell-back segments + TXT keys are the honest cost there. **Structural consequence: the compiled entry
     list is COMPLETE — unconditioned entries are RETAINED as entries** (the folded sums are the derived fast
     plane beside them, never a replacement) — per-entry text and per-entry attribution both require the list.

     > **⚖ THE DIVISION OF LABOUR — `CvGameTextMgr` KEEPS THE BLOCKS AND LOSES THE SUB-BLOCKS (owner).** The
     > renderer removes *"the vast majority of bespoke work GameTextMgr used to do"*: the text manager
     > *"should only care about TXT_KEY replacements, and be the `Cy` target for actual string content — it
     > should not need to manually convert entries for each individual tooltip, or text box, when that text
     > conversion can be built programmatically."*
     > ⛔ **But the BLOCKS STAY: *"the blocks are different sources put together"*.** A block is a COMPOSITION —
     > one heading over contributions from several distinct sources (building, civic, trait all feeding one
     > happiness block) — so which sources compose it, in what order, under which TXT_KEY heading, is genuinely
     > the text manager's job. ⛔ What must never be hand-built is every SUB-BLOCK — one `appendEntryLines` call
     > per (source, family), and a block simply issues several.
     > ⚑ The test: `getText` around a MAGNITUDE is a hand-built sub-block and wrong; `getText` for a HEADING or
     > choosing which sources belong together is the block, and right.
     >
     > **⛔ A BREAKDOWN ITEMISES WHAT THE OBJECT HAS, AND NOTHING ELSE (owner).** It lists sources DELIVERING a
     > realized value — never a candidate that WOULD deliver one; the two read identically once rendered, so a
     > panel carrying both is unusable, not richer. ⚠ A separator does not rescue it, nor an option defaulting on.
     > ⚑ The discriminator is the ENABLER STATE the line was selected by: a source the object HOLDS belongs in
     > the breakdown; anything off the frontier (`STATE_LISTED`) is a WHAT-IF, for the valuation surface
     > answering *"what do I gain from this?"* — never the account of what a city already has.
     > ⛔ A whole-entity "render every family at once" dump is NOT the shape — it flattens the composition the
     > blocks exist to express, which is why the surface is per-family.
     >
     > **⛔ THE ACCEPTANCE TEST ON A COMPOSER IS *DOES IT STILL READ A LEGACY GETTER*, NEVER *DOES IT READ
     > NICELY* (owner): "we just want to make sure that we don't rely on legacy, and have legacy purged, when
     > creating tooltips."** A composer rendering identically but still reaching a legacy accessor is NOT done;
     > one whose wording changed but whose legacy reads are gone IS
     > ([legacy must fail loud, never mask a cascade gap](../specs/validation.md#legacy-must-fail-loud-never-mask-a-cascade-gap)). ⚑ A conversion DELETES rather than ports:
     > each converted band removes its legacy accessor call with it. ⛔ Altered visible text is never a reason
     > to hesitate — say what changed and move on.
     >
     > **⚖ TOOLTIPS ARE NOW THE INSTRUMENT — FEATURE-COMPLETE, AND THEY MUST LOOK RIGHT (owner): *"we are now
     > so far along that tooltips are now very relevant … I want them to look right, get all the data, so we can
     > find the final missing pieces."*** A tooltip carrying every term of a value is a DECOMPOSITION CENSUS in
     > UI form, reached through the screen instead of the wire — an incomplete tooltip does not merely look
     > sparse, it **hides the gap it exists to reveal**.
     > ⛔ The acceptance test is EVERY FAMILY THE ENTITY CARRIES RENDERS, not the subset a composer happens to
     > have been converted onto. ⚑ MOVEMENT is the named exemplar (owner: *"especially with things like
     > movement, it's easy to spot when it's wrong"*) — a player checks it against the unit in front of them, so
     > it is the canary for the rest.
     > ⚠ The composer that renders NOTHING is the one to hunt, and it is silent — an empty tooltip logs no line,
     > fails no build, and reads like an entity with nothing to say. Enumerate composers mechanically rather
     > than trusting a screen looks populated.
     >
     > ⛔ **The retired ruling, recorded because its ABSENCE will look like drift:** tooltips were once
     > explicitly end-stage — *"how tooltips are rendered is fairly irrelevant right now, these are bugs we
     > catch at the end"*. That was an ANTI-RATHOLE GUARD, not a judgement of value: written when *"everything
     > was fucked"* and agents *"consistently and constantly tried to 'keep the game running' despite it not
     > even compiling"*. A red tree renders nothing, so attention spent there was unrecoverable. ⇒ The guard was
     > scoped to a CONDITION that is over — the tree builds, so the work it deferred is now the work.
     > ⚠ What does NOT come back: legacy PARITY — the tooltip SET stays demand-driven (owner: *"we will figure
     > out what tooltips we need … from community requests and playtests"*), so a legacy line a cut removed is
     > not a regression. Completeness is measured against what the ENTITY CARRIES, never what the legacy
     > composer used to print.
     >
     > **⚖ THE DEMAND ORDER, MEASURED BY USE (owner) — worker · combat · plot · building · unit.** That is the
     > order they are hovered in, so it is the order they are worked in; COMBAT is the standardized exemplar, so
     > live work is worker → plot → building → unit. ⚑ WORKER is two composers, not one: what a worker CAN do
     > here (`CvDLLWidgetData::parseActionHelp`) and what it IS doing (`CvGameTextMgr::setUnitHelp`'s instance
     > form).
     >
     > **⚖ A TOOLTIP IS AN ORDERED SET OF BLOCKS, AND THE BLOCKS ARE THE DESIGN (owner): *"we do design tooltips
     > around the concepts of blocks"*.** A composer's deliverable is a BLOCK LIST; per block there are exactly
     > three decisions — which sources compose it, what heading it sits under, and WHEN IT SHOWS.
     > ⛔ The content is NOT the concern (owner: *"most of the info should be able to be programmatically
     > generated — it is the final structuring of the tooltips themselves, and when they show, that is the
     > trick"*) — `appendEntryLines` needs no design; a pass spent generating lines worked the half that was
     > never the problem.
     > ⚠ A run-on comma-separated line is NOT a block structure, however complete its content. ⛔ A SHOW
     > CONDITION is a design call, asked never inferred.
     >
     > **⚖ THE DLL DOES NOT CONVERT FOR DISPLAY — THE CONSUMER CONVERTS ITSELF (owner: "let python convert
     > themselves").** A composer doing `(float)value / 100 / denominator` to print `%.2f` is the DLL doing the
     > presentation layer's arithmetic, in FLOAT, for a value the engine holds as an integer. ⚠ Not an OOS risk
     > while display-only — exactly why it survives unnoticed — but it is the wrong side of the boundary: remove
     > it as each composer moves, never copy it into a new one.

  ```cpp
  // SECTIONS — whole typed objects
  const CvRequires*  getRequires() const;
  const CvProvides*  getProvides() const;
  // CLASSIFICATION — O(1) bitset, hold-vs-provide in the name
  bool hasAttribute(int attributeId) const;
  bool providesCapability(int capabilityId) const;
  // MODIFIER GROUPS — straight compiled reads, the conditioned list, the what-if valuation
  int getDefense(DefenseKind eKind, ScopeKind eScope) const;      // one load, 0 calculation
  const CvModEntries& defenseConditioned() const;                 // prebuilt trees; walked at bounded cadences
  void expectedFlatYields(const CityContext& cityContext, const EmpireContext& empireContext,
                          const CvPlotGroup* plotGroup, int (&flatYields)[NUM_YIELD_TYPES]) const;
  // INTRINSIC — bare typed reads (×100 where a magnitude)
  int getAirlift() const;
  ```

  **A point getter reads the LOAD-COMPILED sum and nothing else** — compiled once from the anatomy by the one
  compile pass, never runtime-summed, never a second hand-maintained store beside it. And no read anywhere on
  the surface does a per-call string address, map walk, or channel resolution
  ([materialize at mapFrom](#materialize-at-mapfrom--no-runtime-string-reads-in-info-getters-the-single-source-laws-load-time-sibling)).
- **THE SINGLE-THREAD BUDGET — why this shape is efficient on the one game thread.** The layering is the
  efficiency: (1) repeated hot reads (a BUILT thing's realized value) hit the package caches on the game objects
  ([state-repositories.md](../cascade.md)) — O(1) bare fetches, never an info read; (2) **the anatomy
  walk is LOAD-ONLY** — every runtime ask is a straight fetch of a compiled structure: the point reads over the
  compiled sums, the edge-family lists, the enabler's maintained frontier vectors — **0 calculation on the
  straight asks**; (3) the ONLY thing ever evaluated is the compiled CONDITIONED tail — condition evaluation is
  irreducible (the answer depends on the asking city) and runs at exactly two bounded cadences: event-driven
  package rebuild (EVENT volume), and the per-decision `expected*` read, bounded by **frontier × cities**
  ([enabler.md §6](../specs/enabler.md)), never database × cities; (4) every evaluator predicate is an O(1)
  CONTEXT fetch (`plotAttrs` counts, the `policies` union, the operating set) — a predicate that walks
  plots/units per call is the efficiency defect to reject in review. **Consumer call discipline:** `expected*` is
  a per-DECISION read — once per (city, candidate) per pass; an AI needing repeated score access caches its OWN
  scores (the sanctioned AI-heuristic residual, [superseded-ideas #1](superseded-ideas.md)) — it never re-asks the
  what-if in an inner loop. A regression in any of this surfaces where every performance regression surfaces — the
  per-turn wall clock ([turn time is king](../cascade.md#-the-per-scope-package-model--the-cascades-founding-design-1-stated-as-cache-architecture)).
- **Every AMOUNT getter is ×100 native; a PERCENT is never scaled, and there is no `getX`/`getX100` pair**
  ([the ×100 fixed-point model](../specs/curators/fixed-point-and-scales.md#1-the-model--integer-100-for-amounts-human-only-at-the-in-and-out-boundaries) —
  [fixed-point-and-scales.md](../specs/curators/fixed-point-and-scales.md) has the full boundary/unit rules and
  the silent-failure tell of a `÷100` landing on a percent slot). The name says the VALUE, never the scale; the
  flat-vs-modifier split lives in the member name (`getFlatYield` vs `getYieldModifier`), never a scale suffix.
- **⚑ A LEGACY `Global*` / `Area*` / `National*` PREFIX IS A SCOPE FRAGMENT — its successor is the SAME kind read
  with a scope ARGUMENT.** This is the single most common disposition in the compiler census, and reading it as a
  missing member instead sends an agent looking for a getter that was never meant to come back:
  `getGlobalYieldModifier` → `getYieldModifier(eYield, CASC_SCOPE_EMPIRE)`, `getCommerceChange` →
  `getFlatCommerce(eCommerce, CASC_SCOPE_CITY)`, `getGlobalFreeSpecialist` / `getAreaFreeSpecialist` →
  `getFreeSpecialistsAny(CASC_SCOPE_EMPIRE)` (area authors at EMPIRE — a landmass is not a scope,
  [state-repositories.md](../cascade.md)). The name lost the fragment because scope became an axis
  ([scope is a separate axis, never folded into the kind](#the-coherent-surface--grouped-storage-parameterized-getters-owner-clarity-and-predictability-is-king)); nothing was removed.
  ⚠ Confirm the KIND enum at the call site rather than pattern-matching the name — the prefix tells you the
  SCOPE, never which kind the value is, and a wrong kind compiles clean and reports a plausible wrong number.
- **Extensible by DATA, not by new members/getters.** A new scalar family is a new `m_scalars` enum entry; a new
  property is a new id in `m_properties`; a new attribute is a new bitset key. The getter surface does not grow.
- Intrinsic self-description (`getAirlift`, `getMaxStartEra`, the shrine/corpHQ FKs, flavours) stays a bare typed
  read — genuine lone values, not a groupable set. The ~300 hand-named getters mirroring the legacy `CvXInfo`
  contract collapse into this surface, and consumers rewire onto it
  ([build a new getter surface, never widen a legacy one](#-the-two-read-roles--one-grammar-two-answers-owner)) — the info half of the access surface.

## ⚖ THE TWO READ ROLES — ONE GRAMMAR, TWO ANSWERS (owner)

> The keystone of the ACCESS surface. The section above is the INFO half; this states what the info half and the
> GAME-OBJECT half share, and what must stay different. Binding:
> [build a new getter surface, never widen a legacy one](#-the-two-read-roles--one-grammar-two-answers-owner).

**⛔ The new surface is NOT a replacement mapping of the existing getters (owner).** No legacy getter name,
signature, or shape survives into it. The measured 622 channel-shaped declarations on `CvCity`/`CvPlayer` are a
**DELETION LIST and a COVERAGE CHECKLIST** — the set of values that must be answerable somewhere on the new
surface — never a per-getter migration worklist. Mapping legacy→new one signature at a time is the
half-migration reflex in its purest form: it lets the legacy contract dictate the replacement's shape, which is
precisely how that surface accumulated.

> **⚖ AND THE DELIVERABLE IS THE SURVIVING SURFACE, NOT "A GETTER CUT" (owner): *"we should ensure that we
> just have the getters we need, and have an understandable structure."*** The `CvCity`/`CvPlayer` work is
> judged by its END STATE — a surface carrying exactly the reads consumers genuinely NEED, organized so a
> reader can tell where a value lives — never by how much legacy was deleted. The deletion of the legacy
> channel-shaped names is the CONSEQUENCE of consumers moving onto that surface; it is not the unit of work
> and not the acceptance test.
> ⚑ It is the SAME ruling the Python boundary below already carries — *"minimal amount of endpoints is not
> the target here, properly organized is"* — applied to the C++ game-object half: NEED decides membership
> (demand-driven, freely given where a consumer genuinely wants a read, and never derived from the legacy
> list), and COMPREHENSIBILITY decides structure (the group reads and named concepts of the grammar below).
> ⛔ Two failure modes it bans equally, because both measure the wrong thing: sweeping getters for deletion's
> own sake (a falling def count read as progress), and sparing a legacy name because deleting it is work. A
> read nothing needs GOES; a read something needs is served on the coherent surface; the census is DEMAND,
> never the surviving getter count.

**The two roles are DISTINCT, and the distinction is load-bearing:**

| role | asks | answers from |
|---|---|---|
| **INFO** | *"what do I CARRY?"* | authored data, compiled once at load — static, per-owner-agnostic, shared by every player |
| **GAME OBJECT** | *"what do I HAVE, right now?"* | live realized state — the roll-up over the ~5 scope packages, with per-city gates applied at the combine |

⛔ **They are NOT interchangeable and must never LOOK interchangeable.** Giving both the identical signature
invites a consumer to treat authored data and live state as the same answer — the shared-vocabulary trap that
[the pollution guardrail](../specs/validation.md#the-pollution-guardrail--engine-computed-data-never-rides-in) exists to police from the other direction. Two roles,
two surfaces.

**What IS standardized is the GRAMMAR — both surfaces obey all of it:**

1. **⛔ ONE GETTER PER GROUP — the getter IS the group (owner).** `getYields()`, `getProperties()`,
   `getCommerces()`, … — the read hands back **the whole group**, and there is **NO scalar getter per channel**.
   A consumer wanting one value takes the group and indexes it. This is the standardization: the surface grows by
   GROUPS (a handful), never by channels (hundreds), and the per-channel scalar getter is the very shape the
   rebuild is deleting.
   > **⚖ THE ONE QUALIFICATION — A GATED VALUE EARNS AN EXPLICIT GETTER, BECAUSE A GATE NEEDS A TAP POINT
   > (owner).** Where a STATUS gates what a source delivers, the gated value gets a named read for something else
   > to attach to; a channel-indexed group read offers no such point. The full ruling, its ungated/gated shape and
   > why the announced crossing follows the GATED value live at its home,
   > [state.md](../specs/state.md) § A STATUS IS MIDDLEWARE.
   > ⛔ This is NOT licence to grow the per-channel surface back (owner: *"what I don't want is to have the getter
   > spaghetti we used to have"*). **The test is whether the getter carries a CONCEPT something else attaches
   > to** — a gate tapping it, a predicate resolving through it — never whether a caller would like one value
   > without indexing. A getter that only names a channel is the spaghetti; a getter that names the thing a
   > status suppresses is a seam.
   >
   > **⛔ AND A GROUP IS A VECTOR OF ONE KIND OF QUANTITY — NEVER A BAG OF UNRELATED STATUS BITS (owner).**
   > `getYields()` is a group because every slot answers the same question in the same unit. A flags list does
   > not: `CITY_FLAG_POWER` and `CITY_FLAG_OCCUPATION` are DIFFERENT QUESTIONS sharing a bus. So a flags read
   > serves a caller that genuinely wants MANY BITS FROM ONE FETCH — a status bar drawing four icons — and a
   > single status question is asked by NAME. ⛔ Answering `is this city powered?` with
   > `getFlags()[CityFlagKind.CITY_FLAG_POWER]` is the banned shape even though the value is right.
   >
   > **⚑ THE REASON IS THE MODDER, and it is why this is a hard line rather than a preference (owner): *"the
   > moment we start using a generic getFlags for things like isPowered, is the day we gonna end up being
   > screwed by a modder taking that too far."*** An indexed bag teaches every consumer that city state is a
   > bit array addressed by ordinal — and once mod code indexes it, the LAYOUT is frozen: a flag cannot be
   > reordered, retired, or split without breaking code we do not control. A named getter keeps the layout an
   > implementation detail and leaves exactly one thing published: the question.
   >
   > **⚖ THE FRAME — C++ IS THE API FOR THE FRONTEND, and it answers like a normal web API (owner)** (*"yes I
   > know there are differences"*). A web API publishes NAMED resources whose internal representation stays
   > private; it does not hand back an array the client indexes by magic number and call that an endpoint.
   > ⇒ Read every published binding as an endpoint someone else will build against, and the two halves above
   > follow from it directly: name the question, keep the layout yours.
2. **The EXISTING ENGINE ENUM indexes the RESULT, not the call** (`YieldTypes`, `CommerceTypes`, …); a family
   with no engine enum uses its own kind enum (`CvInfoKinds.h`). So the enum stays the consumer's vocabulary
   while the call itself carries no channel argument. The data-minted channel id remains the CACHE's internal
   key and is never something a consumer learns.
3. **×100 native, always** — no `100` in any name, no `getX`/`getX100` pair, no scale variant
   ([the ×100 fixed-point model](../specs/curators/fixed-point-and-scales.md#1-the-model--integer-100-for-amounts-human-only-at-the-in-and-out-boundaries)). A reader ÷100s at the point of use.
4. **Scope is a spelled-out ARGUMENT, never a name fragment** ([scope is a separate axis, never folded into the kind](#the-coherent-surface--grouped-storage-parameterized-getters-owner-clarity-and-predictability-is-king)).
5. **⛔ THE VALUATION PROTOCOL — THE LIVE CONTEXTS GO IN, THE PROPOSED INCREASE COMES OUT (owner).** The caller
   passes the live [contexts](../cascade.md) and gets back **the DELTA** — what this candidate would ADD — never
   the raw percentage and never the new total.
   - **⛔ THE CONTEXT *IS* THE CURRENT VALUE — that is the whole point of it (owner).** A percent deposit has no
     value on its own: *"+25% production"* is worth a little in a small city and a lot in a large one, so it
     only becomes a number against the base it multiplies. The context supplies that base, because it is the
     bound live-state surface for its scope. ⛔ **Do NOT pass current amounts as a separate parameter** — that
     hands the read data the context already carries, and re-introduces the ad-hoc state-reach the contexts
     exist to end. A context that cannot answer a base the resolution needs is a **CONTEXT GAP to close by
     adding the forward** ([contexts.md](../cascade.md)), never a reason to widen the signature.
   - **Why the DELTA comes out:** the question is *"what do I gain from this?"*. A delta is directly weighable;
     a new total forces every caller to subtract against a base it must fetch separately.
   - The contexts serve BOTH halves in one pass: they carry the base the percent resolves against (the
     `CityContext` forwards the city's CURRENT REALIZED YIELDS for exactly this, [contexts.md](../cascade.md)),
     and they are what the compiled CONDITIONED tail is evaluated over (*"+25% more while coal is connected"*).
   - **⚖ A CITY-LESS VIEW EVALUATES AGAINST THE CAPITAL (owner).** The valuation needs a `CityContext`, and a
     player-level "all buildings" view (the build list) has no city bound. The rule is the AI's own precedent
     made explicit: **the bound city if there is one, else the player's CAPITAL** — and it lives in ONE place
     every criterion reads ([the DRY single-implementation law](#dry--one-implementation-per-calculation--evaluation-the-single-source-law)), never
     re-derived per filter or per sort. A player with no capital has no valuation to give, and the criterion
     ranks neutral rather than inventing one.
   - **⚑ TWO CONSUMERS, ONE CALL (owner): the AI's evaluation AND the build-list HOVER TOOLTIP.** The same
     valuation answers *"what do I gain from this?"* for the AI weighting it and for the player reading the
     tooltip. That is not a convenience — it is what makes the displayed number and the acted-on number the
     SAME number, structurally. The classic failure it removes is a UI advertising one value while the AI plans
     against another, which no amount of care prevents once they are two implementations
     ([the DRY single-implementation law](#dry--one-implementation-per-calculation--evaluation-the-single-source-law)). It is also why the resolved DELTA is
     the right return: it is simultaneously what an AI weight multiplies and what a tooltip line prints.
   - **⛔ A "HOW VALUABLE IS THIS YIELD" WEIGHT IS ASKED AT MOST ONCE PER YIELD, AT THE START OF A doTurn
     (owner).** *"Those 'how valuable is this yield' questions is a question that at most should be asked once
     per yield at start of a doTurn, at absolute most."* This is the CADENCE half of the protocol, and it is a
     ceiling rather than a target: once per (yield × turn) is the most that is ever legitimate, and less is
     better. ⛔ Per CANDIDATE is the banned shape — a weight describes the EMPIRE's standing, so it cannot
     differ between two buildings scored in the same pass, and asking it per candidate multiplies whatever it
     costs by the frontier.
     ⚠ It must therefore not be keyed on anything that moves WITHIN a turn (a treasury balance is the tempting
     one), or the ceiling is silently lost the moment that input twitches.
   - **⛔ AND THE WEIGHT FOLLOWS WHAT THE EMPIRE *NEEDS*, NEVER A TOTAL OF WHAT EVERY OTHER CITY HAS (owner):**
     *"It should not start caring about what all others have, but what that empire needs."* A need is a
     property of the asking empire — its obligations against its means — so it is answerable from that
     empire's own standing. ⛔ Deriving it by re-totalling every member's realized output is the wrong
     question wearing the right answer's clothes: it makes a per-empire constant cost `O(cities)`, and at the
     receiver Σ that is `O(cities)` per ask ([state-repositories.md](../cascade.md) § A CROSS-SCOPE
     receiver total).
     ⚑ **The measured case this rules on:** the gold-value weight reached the empire's realized gold commerce,
     which re-sums all 185 cities' §2a combines — asked once per BUILDING CANDIDATE, it was the whole of a
     45-second `AI_chooseProduction` on the standing save.
6. **⛔ A GROUP HANDS OUT ITS CHANNELS; A FINAL-STATE CALCULATION IS DOWNSTREAM OF IT (owner).** The wellbeing
   group returns `happiness` and `anger` as **two separate numbers** (and `health`/`unhealth` likewise) — *"then
   you will know the results from that"*. The realized end-state values (`angryPopulation`, `healthRate`) are
   **NOT group entries and NOT getters**: they are a final-state calculation over numbers the group already
   handed out ([modifier.md §2b](../cascade.md) specs the arithmetic). ⛔ Folding a final-state value into
   the channel array is a category error — it puts a computed OUTCOME in a slot that means "a channel a source
   deposited into", and it hides the opposing-pair structure the four channels exist to express. The calculation
   still exists **exactly once** as a pure static function on the calc surface
   ([the DRY single-implementation law](#dry--one-implementation-per-calculation--evaluation-the-single-source-law)); it is simply not part of the read.
7. **The group read FILLS A CALLER-OWNED ARRAY** — one call in, the whole group out, indexed by the group's
   enum. Passing state once and getting the whole resolved group back is also what keeps a future
   whole-candidate snapshot possible without building it now; a design answering one scalar per call would
   foreclose it *and* would re-resolve the same state per channel.
6. **Extensible by DATA, not by new members/getters** — a new channel is a new id, not a new function.
7. **Parameters spelled in full**, index parameters named for the enum they key
   ([contexts.md](../cascade.md) naming rule).

**The GAME-OBJECT half's realized shape.** Each scope owner (`CvPlot` / `CvCity` / `CvPlayer` / `CvTeam`)
carries **one group read per modifier FAMILY whose channels the data authors AT
that scope** — the set comes from the census scope masks + the minted channel sets, never a hand-written list.
Every group folds through the **ONE cross-scope roll-up on the calc surface** (`InfoValuation::realizedAt*`,
beside the `cityRate` combine it specializes): modifier.md §1's downward roll realized AT READ over the chain the
object sits under (city = team + empire + city · empire = team + empire · team/plot = itself;
WORLD is CONFIG and carries no package, and PLOT never enters an upper chain — a per-plot value resolves in
isolation first). A channel the scope **CONSUMES** answers its maintained receiver sum instead, and which side of
a channel is the answer comes from the vocabulary's canonical-unit verdict (`infoKindUnit`): a percent-unit
channel IS the additive stack, a flat-unit channel is the flat sum that stack scales. **Naming:** an
engine-enum-indexed group takes the engine plural (`getYields`, `getCommerces`); a kind-enum-indexed group says so
(`get<Family>Kinds`), which also keeps this surface from colliding with — or overloading — the legacy scalar
getters that hold the bare family name.

## ⛔ THE PYTHON READ BOUNDARY — ONE COMPLETE DATA-FETCHING LIBRARY (owner)

> **⚖ READ THIS SECTION AS THE TARGET, NOT AS TODAY'S ACCEPTANCE BAR — THE RULES CANNOT BE FOLLOWED TO THE
> LETTER RIGHT NOW (owner): *"we cannot follow these rules to the letter currently, because the python is a
> gigantic clusterfuck, but we want to lay the groundwork; python reorganization is a separate independent
> pass."*** Everything below states where the boundary is GOING. The tree it describes does not satisfy it and
> is not expected to, so an agent measuring current code against these rules will read ordinary sanctioned work
> as violation.
>
> ⇒ **What binds NOW is the groundwork, and it is a low bar deliberately: serve the read the call site needs,
> NAME it, and put it somewhere a later pass can move cheaply.** That is the whole obligation. The homing, the
> per-type accessor split and the import conversion are the SEPARATE INDEPENDENT PASS, taken as its own piece of
> work when the demand map is known — never as a rider on a repair, and never opportunistically mid-sweep.
>
> ⛔ **It is NOT licence to skip a fix, and that is the failure this callout most needs to prevent (owner): *"the
> amount of fixes getting skipped because of that excuse is starting to drive me nuts — we have a write
> surface."*** The mutation surface is published and live (`set*`/`change*`/`do*`/`create*`/`push*` defs across
> `CvPythonPlayerLoader`/`CvPythonPlotLoader`/`CyGame`/`CyTeam`/`CyMap`/`CyArea`/`CyAct`); it was never cut, since
> the cut above was directional and took the READ bindings only. "The surface is not organized yet" is never a
> reason to leave a broken handler broken, and "it cannot be done properly yet" is never a reason to do nothing.
> ⛔ Equally it is NOT licence to call the current shape correct, or to ADD to the disorder knowingly: the point
> of laying groundwork is that the later pass stays MECHANICAL, and every unnamed or unfindable read added
> meanwhile is what makes it stop being mechanical.
> ⚠ Owner-ruled SEQUENCING with a named end state, so ["deferred" is banned](../../AGENTS.md#design) does not
> reach it — the same standing as the golden-age / anarchy status carve-out ([state.md](../specs/state.md)).
>
> The Python half of the access surface. Binding: [the Cy* surface is not a fixed contract](#-the-python-read-boundary--one-complete-data-fetching-library-owner) — the `Cy*`
> `.def` surface is NOT a contract to preserve. **This is a REBUILD, not an invention:** Python has always fetched
> through a binding layer and that MECHANISM (boost::python) is fine and stays. What is wrong is the SHAPE —
> scattered per-type interfaces, one getter per legacy field, no coherent payload anywhere. ⛔ "It kind of exists
> already" is never licence to widen or build on a `Cy*` binding.

> **⚖ WHY THE KILL IS HARD — it is a FORCING FUNCTION, not a tidiness goal (owner).** *"What I want is a
> consistent surface all the way through, so I force a hard kill on the python surface, because otherwise you
> will take shortcuts."* A live `Cy` surface is an ESCAPE HATCH: while it answers, the cheap move is always to
> bend the new design so Python keeps working, and the result is a surface that is consistent nowhere. Killing it
> removes the option, which is the point — the consistency is what is being bought, and the kill is how it is
> paid for.
> ⛔ **So a good-sounding reason to spare one binding is the failure, every time.** "Python still calls it",
> "cutting it breaks a screen", "wait until the replacement lands" — each is the shortcut wearing caution, and
> each leaves the hatch open. ⚠ Note what is NOT implied: nothing requires the kill to be one atomic operation.
> Piecemeal cutting is fine; what is banned is bending anything to keep the old surface functional.
>
> ⚑ **AND CUTTING WRONG IS CHEAP — "if we delete a working binding, we re-add it; or more probably, REPLACE
> it" (owner).** The second half is the point: a binding that turns out to be needed comes back as the NEW
> surface serving that read, not as the old `.def` restored. So a deletion is not merely reversible, it is how
> a genuine requirement gets DISCOVERED and moved — the cut converts an assumed dependency into a named one.
> Re-adding the legacy binding is the fallback, never the default, or the cut has bought nothing.
> ⚑ The cheapness is structural, not optimism: Python takes the surface with `from CvPythonExtensions import *`
> — **169 files star-import it against 3 with an explicit list** — so NOTHING declares a dependency on any
> single binding. A removed `.def` causes no import-time failure at all; it surfaces at the one call site that
> used it. ⛔ So do NOT slow a cut down to protect a binding, and do not build a resolver to prove one is safe
> first — the compiler names the dead ones for free, and being wrong costs one call site, not a regression.

Four words carry the whole requirement:

- **ONE SURFACE.** A single library IS the Python-facing read boundary — not the per-type `Cy*` interfaces it
  replaces, not a widened binding, never two live surfaces for one read.
  > **⛔ "ONE SURFACE" MEANS ONE LIBRARY, NOT ONE CLASS — AND READING IT AS ONE CLASS IS WHAT PRODUCED THE
  > MISHOMED STATE SURFACE (owner).** *"CyState comes from an agent being overzealous in the interpretation of
  > the reworked and one-surface rulings."* The word bans a SECOND live answer for the same read; it says
  > nothing about how many accessors the one library is composed of, and the per-type accessor ruling below
  > says plainly that it is composed of several. ⇒ A flat class accumulating every type's reads behind an
  > `(owner, id)` address satisfies the word and violates the design — which is exactly the failure
  > [a game object's own data is read from its own accessor](#-the-python-read-boundary--one-complete-data-fetching-library-owner) now makes mechanically checkable.
  > ⚠ Read "one surface" as ONE LIBRARY, COHERENTLY HOMED. A game object's data lives on that object's
  > accessor, and the library is one because there is no second place to ask — never because there is one class.
  >
  > **⛔ SO THE FLAT STATE CLASS IS BEING DISSOLVED, NOT TRIMMED (owner): *"the more I see the use of it, the
  > more I realize it will contribute to the exact getter spaghetti and lack of understanding of where things
  > come from that I want to avoid."*** An address-keyed flat class makes every call site say WHICH object it
  > means and never WHAT KIND OF THING it is asking, so it reproduces the two failures this boundary exists to
  > end — spaghetti, and unreadable provenance — while looking organized because the endpoints are named.
  > ⇒ **Each type's re-home is one pass of that dissolution**, and what is left behind after a pass is NOT a
  > sanctioned residue: it is the next pass. ⛔ Do not add a read to the flat class because a neighbour is still
  > there; add it to the accessor for the type it addresses, which is where it will live anyway.
- **COMPLETE.** The END STATE is that every read Python performs has an answer here, so no gap forces a
  reach-around into legacy — that reach-around IS the second live surface the ruling forbids, the half-migration
  re-created at the last seam.
  ⛔ **But completeness is the DESTINATION, never a GATE ON CUTTING (owner) — the disconnect is not gated at
  all.** A dead legacy binding is an OUTLAW and is shot on sight; reading this word as "cut only once the library
  is complete" inverts the ruling into a shield for the surface being removed. **The cut is the forcing function
  that DRIVES completeness** (below: killing it removes the option to shortcut), so it never waits on it — and
  when the library cannot answer a read, the move is to ADD the read, never to borrow legacy meanwhile.
- **DATA FETCHING, not gameplay.** It serves reads/payloads; Python-authoritative gameplay (Revolution, events)
  stays Python and becomes a CONSUMER of it.
- **⛔ ENUM OPERATIONS ARE FIRST CLASS** — name→type/enum resolution is a supported operation, covering
  **resolution AND EXTENSION**: BUG resolves `WidgetTypes`/`InputTypes`/`InterfaceDirtyBits` by name from config
  strings *and* MINTS new `WidgetTypes` members at runtime, handing them back as widget ids. A read-only lookup
  does not serve that. It generalizes `getInfoTypeForString` and mirrors the load-minted classification
  registries ([the classification-infos registry](../specs/json.md#8-classification--unit-skillstagsstate-building-attributes--empire-capabilities)).
  ⚑ **THE ENUM VOCABULARY IS A PREREQUISITE OF THE READ SURFACE, not a convenience beside it.** The group reads
  are specified as `getYields()[YieldTypes.YIELD_FOOD]` (§ THE TWO READ ROLES: the existing engine enum indexes
  the RESULT), so until the enum TYPES are published the replacement surface cannot be consumed at all — a
  script cannot name the slot it wants, and neither can any screen that would render one.
  ⛔ **Publishing them is therefore NOT a survival of the banned surface.**
  [the Cy* surface is not a fixed contract](#-the-python-read-boundary--one-complete-data-fetching-library-owner) bans the `.def` GETTER contract; a publication carrying zero
  `.def` and zero `class_` is CONSTANTS, not reads. ⚠ The distinction is worth stating because the enum
  publication was once swept up in the binding purge as though it were one, which takes the whole Python layer
  down with it — every module names an engine constant.
  ⚑ **EXTENSION needs no API of its own, which is why none is minted.** A published boost enum is a real Python
  type, so BUG's existing construct-from-int + `setattr` mechanism adds members the moment the type exists; a
  mint verb would be machinery for a caller that does not exist.
  ⚠ **Of the three, only TWO are ours** — `WidgetTypes` and `InterfaceDirtyBits` are in `CvEnums.h`;
  `InputTypes` is the EXE's and was never DLL-published. The DLL owes the vocabulary for the enums it DEFINES,
  and a name absent from `CvEnums.h` is the EXE's to serve, never a hole in the library.

> **⛔⛔ `import *` IS THE REAL ENEMY — IT OUTRANKS EVERY OTHER CONCERN ON THIS BOUNDARY (owner): *"we want
> python imports to be named, so that we know what we are fetching; `import *` is the real enemy."*** Read the
> rest of this section against that ranking: named endpoints are never a problem, and endpoint HOMING is a real
> but secondary one. ⚑ **It is also what CAUSES the duplication ruling below**, which is why the two are one
> subject rather than two: a star import erases where a name came from, so the next modder cannot find the
> endpoint that already answers their question and mints a near-synonym instead. Fix the import surface and the
> duplicate-endpoint failure loses its mechanism.
> ⚠ The SEQUENCING below is unchanged and is itself an owner ruling — the conversion follows the demand map
> rather than preceding it — so this names the target, not a new order to do it in.
>
> **⛔ EXPLICIT IMPORTS, ALWAYS — A MODULE'S BINDINGS MUST SHOW WHAT IT USES (owner): *"I will always prefer
> explicit imports, so you see what is used in python."*** This is what the `Cy*` cut was actually FOR, and
> stating it as "no getter per registry" understated it in one direction and overstated it in the other.
> ⚑ **What was wrong with the old surface was the GC COUPLING and the OPACITY, not per-info accessors (owner):**
> *"I don't mind having a CyAccessor per info; what I didn't like was how it was coupled together with GC, so you
> didn't actually see the imports, and didn't know what you would fetch."* `GC.getBuildingInfo(i).getFoo()`
> declares nothing — a reader cannot tell from the module which registries it touches, and the god object hands
> out everything.
> ⇒ **So a PER-INFO accessor, explicitly bound at module scope, is the WANTED shape** — the bindings list then IS
> the module's dependency list. ⛔ What stays banned is unchanged and is a different axis: the ~300 hand-named
> getters mirroring the legacy per-FIELD contract ([build a new getter surface, never widen a legacy one](#-the-two-read-roles--one-grammar-two-answers-owner)).
> A named accessor per info TYPE is not that.
> ⚠ **And an opaque SLOT enum re-creates the very fault it was meant to cure.**
> `INFO.getIntrinsic("WORLD_", id, PYINT_CORP_MAINT_PERCENT)` is decoupled from `GC` and still fails the test —
> the call site names a slot rather than the thing, so a reader again "doesn't know what you would fetch".
> ⇒ Reserve the generic prefix-addressed plane for what is genuinely UNIFORM across every registry (identity
> text, edge families); a value that belongs to ONE type is named on that type's accessor.
>
> **⚖ A CLASSIFICATION TEST IS NAMED, ONE ENDPOINT PER KEY — AND ENDPOINT COUNT IS EXPLICITLY NOT THE TARGET
> (owner): *"you can easily make a Cy wrapper for a specific skill such as hidden nationality; I want the Cy
> endpoints to be understandable — minimal amount of endpoints is not the target here, properly organized
> is."*** So a consumer asks **`INFO.isHiddenNationality(unitId)`**, never a parameterized test carrying an id.
> ⛔ **This settles the question of how Python names a classification id by REMOVING it: Python never names one
> at all.** Both former precedents are retired for this plane — the generated-enum form
> (`hasSkill(prefix, id, CLS_SKILL_HIDDEN_NATIONALITY)`) and the authored-key-string form
> (`hasSkill(prefix, id, "hiddenNationality")`) are each the opaque-slot shape above wearing a different
> costume, and neither survives the *"does the call site say what it fetches"* test.
> ⚑ **It is the SAME ruling as the per-info accessor above, one level down**, and it is why endpoint count is
> the wrong axis: the surface is judged on whether a reader can trace it, so N understandable endpoints beat
> one parameterized endpoint that hides N meanings. ⚠ Read together with the C++ side
> (§ THE GETTER SETUP category 2), which is the exact OPPOSITE and correctly so: there the id IS a
> compile-time constant, so `hasSkill(CLS_SKILL_BLITZ)` names the thing at the call site. Python has no such
> constant, which is why the two planes diverge rather than one being wrong.
> ⛔ The parameterized `CyInfo::hasSkill`/`hasTag` are not the consumer surface; a named endpoint is added
> **on demand, for the call site that wants it** (the § THE IDENTITY SET rule), never pre-emptively across a
> registry.
>
> **⚖ THE ENEMY IS UNORGANIZED SPAGHETTI, NOT ENDPOINTS — AND A PER-TYPE ACCESSOR OBJECT IS WANTED, GAME
> OBJECTS INCLUDED (owner): *"I do not mind named endpoints, or a CyCity object with named endpoints related to
> city; what I don't like is when we get full getter spaghetti without organization or structure."*** This
> widens the per-INFO accessor ruling to the whole boundary — endpoint count is not the axis (as above); what
> binds is HOMING: an endpoint belongs on the accessor for the type it addresses. A flat class accumulating
> UNIT, BUILDING and HANDICAP reads side by side is the spaghetti wearing named endpoints — organized is the
> requirement, "named" alone does not satisfy it.
>
> **⛔ AND THE FAILURE ORGANIZATION EXISTS TO PREVENT IS DUPLICATION, WHICH IS WHAT MAKES THIS MORE THAN TASTE
> (owner): *"we need the endpoints we need, that is never a problem; what is a problem is 3 similarly named
> endpoints that in essence do the same thing because the previous modder didn't know where to look."*** So the
> test on a new endpoint is never *how many are there* — it is **could someone find the one that already
> answers this?** An unfindable endpoint is re-minted under a near-synonym, and the surface then carries three
> spellings of one question that drift apart.
> ⇒ **Two obligations follow, and the first is the cheap one: LOOK BEFORE YOU ADD.** Read the accessor for the
> type you are about to serve; a near-synonym you did not find is the defect you are about to file.
> ⚑ **It is [the DRY single-implementation law](#dry--one-implementation-per-calculation--evaluation-the-single-source-law) on the boundary** — *"a
> file-`static` calculator is a DRY hazard: the next consumer can't see it, so it reimplements it — the exact
> mechanism of the C2C rot"* — and the mechanism is identical whether the thing reimplemented is a calculator
> or a read. ⚠ The in-tree worked case is C++ rather than Python and is the more convincing for it:
> `modSegmentCached` existed as THREE separate file-static copies, one per consumer, each written because its
> author could not see the others. Nobody decided to duplicate it; the shape did.
> ⚠ So a genuine near-pair must SAY why it is two — `isShrine` (29 buildings) beside `isReligiousBuilding`
> (213) is two questions, and the comment carries the discriminator precisely so the next reader does not
> "consolidate" them or mint a third.
>
> **⚖ THE ORGANIZING PASS IS SCHEDULED, AND THE CURRENT PILE IS ACKNOWLEDGED DEBT (owner): *"we have created
> some of that ourselves now, and we will go back to wire that up properly in a final pass when everything is
> actually in and working."*** ⇒ Keep ADDING named reads wherever a call site demands one; homing is corrected
> wholesale later, not negotiated per endpoint — the same discovery-first sequencing as import conversion below
> (§ THE SEQUENCING IS DISCOVERY-FIRST), applied to accessor layout instead of imports. ⛔ Not licence to call
> the flat pile correct, nor a reason to withhold a read a call site needs meanwhile.
>
> **⚖ THE REASON IS TRACING, AND IT IS THE POINT THE OTHER TWO SERVE (owner): *"it infinitely helps tracing —
> for me, other modders, and agents; when you read the python code today anyone is hard pressed to figure out
> where things come from."*** Not style, not hygiene: the question a reader must be able to answer is **where
> does this come from**, and today they cannot. ⛔ **The root is named: GLOBAL imports and IMPLICIT imports.**
> They are two different failures wearing one symptom, and only the first is the star import:
> - a **GLOBAL/star import** leaves the name but erases its ORIGIN — `CyInfo` could come from anywhere, and the
>   module does not say;
> - an **IMPLICIT import** was never written in Python at all. This tree's extreme form is the config- and
>   XML-bound dispatch — the BUG `lookupModule`/`lookupFunction` graph and the `<PythonCallback>` family
>   ([python-read-map.md](../reference/python-read-map.md) §5.3/§5.4) — which no grep of the Python can see.
> ⚑ That second one is why *"just read the code"* fails here, and why a read found is a read to SERVE while a
> read not found is never evidence of absence.
> ⚑ **The standard is the ordinary one: named imports, as any JavaScript project expects** —
> `from CvPythonExtensions import CyInfo, CyState, CyVictoryInfo` — so the import block IS the dependency list.
>
> **⚖ BUT THE SEQUENCING IS DISCOVERY-FIRST, AND THAT IS A RULING TOO (owner): *"I do not need the import
> structure to be perfect yet — restructuring the Cy layer is not hard after we know what we need and where to
> get it from."*** The expensive work is finding every read and homing it on the right surface; re-pointing the
> layer afterwards is mechanical. ⛔ So converting imports AHEAD of the demand map is the failure — it is done
> twice, and the second pass is the expensive one. ⚠ Equally, this is not licence to call the star import
> acceptable: it is a real defect with a scheduled fix, not a sanctioned shape
> (["deferred" is banned](../../AGENTS.md#design) does not reach an owner-ruled ORDERING).
>
> ⚑ **The corroboration, because it shows the cost is not hypothetical: the espionage advisor crash.**
> `INFO.getIntrinsic("ESPIONAGEMISSION_", i, PYINT_COST)` names a SLOT, so nothing at the call site said where
> the value came from — and `PYINT_COST` was wired for `BUILDING_` only. The unwired prefix fell through to the
> shared `-1`, which is indistinguishable from a real answer, so every mission failed its guard, none was
> classified, and a `-1` mission id reached the engine: an ACCESS_VIOLATION inside a boost::python call, in a
> different screen from the read that was wrong. **A named accessor cannot fail that way — an unwired read does
> not compile.** Provenance at the call site is what turns that from an archaeology exercise into a compile error.

**⚑ BUILD IT FOR THE PEDIA — but know exactly what that proves (owner).** The pedia's purpose is to display every
entity exhaustively, so it is not a sample of the info surface, it **is** the info surface rendered. Therefore:

- **SHAPE — complete by construction.** Nothing in Python needs a payload shape the pedia does not already force,
  so serving the pedia SETTLES the library's structure; no later consumer introduces a new kind of read.
- **⚠ COVERAGE — NOT proven, and the gap is enumerable.** The pedia is ~99.7% a static reader: it exercises a
  fraction of STATE, almost no COMPUTED and no MUTATION, so a large part of the Python surface sits in planes it
  never touches. The residue is an appendix — whole info types with no pedia page (map-gen, game-config,
  diplomacy/victory/vote, command/UI-action) plus per-field reads. **Serving the pedia completes the INFO plane
  and the shapes; it does not complete the boundary.** Treating it as a coverage oracle is the mistake to avoid.

> **⛔ WHAT IS ACTUALLY WRONG WITH THE OLD SURFACE IS THE LOOPING, NOT THE READS (owner): *"if it is naming, or
> gametext, we reintroduce it — they are not the root of evil here, the looping of all infos are."*** This is the
> ruling that scopes the whole rewire, and getting it backwards wastes the effort in both directions.
> - **A TEXT or NAMING read is CHEAP and is simply SERVED.** An entity's authored identity text — description,
>   help, civilopedia, strategy, adjective, short description ([json.md §7](../specs/json.md)) — is content, not a
>   legacy getter contract, so it goes on the identity plane without deliberation. ⛔ Do not ration it, and do not
>   file one as "per-type tail" merely because it is absent from `CvInfoBase`: several are genuinely distinct
>   authored strings (a civilization's NAME, SHORT name and ADJECTIVE are three different texts, and the dynamic
>   naming composes from them), so collapsing them destroys content. Their `uiForm` argument is carried through —
>   it selects the grammatical variant localization needs.
> - **A WHOLE-REGISTRY LOOP is the actual defect.** Sweeping every id to ask a per-id predicate re-derives what the
>   entity already carries ([reverse lookups are populated once, at load](../cascade.md#1-one-step-deposit-down-accumulate-read-o1)), and it does not survive the
>   rewire: the per-id reads it walks are the ones being deleted. It converts to the maintained set, the entity's
>   own compiled entries, or its reverse edge families — never to a faster per-id getter, which leaves the loop
>   doing exactly what it did before while reading as migrated.
>
> **⚖ THE PEDIA IS THE ONE PLACE A FULL SCAN IS UNAVOIDABLE, AND IT IS NOT A DEFECT (owner)** — *"it is the pedia,
> it is where all info is stored, as an encyclopedia."* Enumerating a registry to display every entity IS its job,
> so those loops STAY. ⚑ What still changes there is the COST, not the shape: an enumeration that crosses the
> boundary once per entity becomes ONE crossing via the per-type index read, since a `boost::python` call costs far
> more than the lookup inside it. ⛔ And the carve-out is for ENUMERATION only — a pedia page walking a DIFFERENT
> registry to find "what needs me" is a cross-link, which the load-time reverse families already answer
> ([pedia-read-map.md](../reference/pedia-read-map.md) finding 2 separates the two motives).

**⛔ THE CUT IS DIRECTIONAL — only the READ surface dies (owner).** The bridge runs both ways, and #430 owns one
of them:

- **Python → engine READS** (the `Cy*` info/state bindings) — this is what the library replaces, and the binding
  surface is GONE.
- **Engine → Python CALLBACKS** — **NEEDED, and kept (owner: "we need eventreporter, we need mapscript, amongst
  other things")**. `CvEventReporter`, the map-script hooks and `CvOutcome`'s Python outcomes are what makes
  Python-authoritative gameplay possible at all, so this is REQUIRED FUNCTIONALITY, not a deferral —
  ["deferred" is banned](../../AGENTS.md#design) does not apply to it and it is not a thing to "finish later".
  ⚠ The list is open ("amongst other things"): treat a callback you find as kept unless ruled otherwise.
  ⚖ **KEPT THROUGH #430, not kept forever — the successor is named (owner): `CvEventReporter` is replaced by
  the TRIGGERS machine and events move INTO C++, "but that is not 430."** So this is a SCOPE boundary with a
  known destination, not a permanent Python carve-out — do not read "permanent carve-out" on the event surface
  as "Python owns this forever", and equally do not start the move inside #430. The triggers machine
  ([triggers.md](../specs/triggers.md)) is where it lands when its own work item is taken.
- ⚑ Consequence: the `Cy*` WRAPPER classes (`CyCity`/`CyUnit`/`CyPlayer`/…) STAY while the legacy per-field
  binding contract does not — 33 engine files hold them for that direction. Reading that as a half-cut to
  complete would delete working gameplay.

  > **⛔ THE IDENTITY SET IS THE FLOOR, NOT THE CEILING — AND READING IT AS THE CEILING IS STALE (owner): the
  > `Cy*` bindings are the literal API surface for Python, so a type publishes the GET / PUT / POST it is
  > required to.** The identity set is what a handle must ALWAYS carry so a legacy consumer can name its object;
  > it was never a cap on what the accessor answers. ⚑ **The tree settles it** — `CyPlayer` publishes 332
  > endpoints, `CyCity` 157 (the coherent group reads: `getYields`, `getCommerces`, `getWellbeing`,
  > `getScalars`, `getDefenseKinds`, … plus its mutators), `CyTeam` 116, `CyPlot` 106. That IS the per-type
  > accessor this section's own next ruling prescribes, already built.
  > ⇒ So a thin wrapper is an UN-RE-HOMED TYPE, never a finished one: `CyUnit` at 8 endpoints against 58 legacy
  > declarations its Python still calls is the work outstanding, not the design achieved. The burndown is
  > countable — `python Tools/verify-python-bindings.py` (Validation).

  > **⚖ THE IDENTITY SET — A HANDLE ALWAYS PUBLISHES OWNER + ID + POSITION, AND THAT ALONE IS WHAT KEEPS A
  > LEGACY CONSUMER WORKING (owner).**
  > *"Carry identity set — if that is what it takes for legacy to keep working then it's an obvious tradeoff. I
  > only want to refactor the python I have to, otherwise we **never** will be done."* A legacy consumer holding
  > a handle must be able to say WHICH object it holds; re-pointing every such site onto the read planes is
  > refactoring that buys nothing and does not converge.
  > ⛔ **It is the ADDRESS, and it is what a legacy consumer needs to name its object.** Owner, id, position.
  > [the Cy* surface is not a fixed contract](#-the-python-read-boundary--one-complete-data-fetching-library-owner)'s ban on the legacy info/state GETTER contract is untouched:
  > what a handle must never become is the old per-field surface restored wholesale.
  >
  > **⛔ BUT A GAME OBJECT'S OWN DATA IS READ FROM ITS OWN ACCESSOR — `CyCity`, NEVER A STATE CLASS KEYED BY
  > ADDRESS (owner).** *"That CyState holds city data is wrong — that should stay on CyCity."* A city's
  > population, name, maintenance and food are the CITY's data, so they are asked of the city.
  > ⛔ **THE TEST IS THE METHOD NAME, and it is mechanical: *"the moment you have `getAnotherObjectSomething`, we
  > have failed"* (owner).** A method whose name carries a DIFFERENT object's noun is homed wrong by
  > construction — `CyState::getCityPopulation(owner, id)` is `get<ANOTHER OBJECT><Something>`, while
  > `CyCity::getPopulation()` names only what the receiver already is. ⚑ The prefix is the tell precisely because
  > it exists to disambiguate a receiver that should never have been holding the read: an accessor that owns its
  > subject needs no noun in its verbs.
  > ⇒ So the per-type accessor ruling above is not a preference about tidiness — it is the SAME rule stated from
  > the naming side, and the two are checkable against each other: if the name needs the noun, the endpoint is on
  > the wrong class.
  > ⚖ **THE HANDLE CHAIN IS THE POINT, NOT A COST — IT SHOWS THE HIERARCHY (owner).** A caller resolves the
  > object and then asks it (`PLAYER.getCity(id).getPopulation()`), and that chain STATES where the value comes
  > from: a city read is reached THROUGH the player that owns it, so containment is visible at the call site
  > instead of being flattened into an `(owner, id)` argument pair. ⛔ So the resolve step is not a two-hop to
  > optimize away — it is the provenance the flat address-keyed class destroyed, and it is what makes a script
  > readable by someone who did not write it.
  > ⚑ **Consequence: publishing the ADDRESS→HANDLE path is part of homing the reads, never a separate favour.**
  > An accessor nobody can obtain serves nothing, and an event payload hands over the identity PAIR rather than a
  > handle — so the resolver is what makes the kept callback direction usable against the read surface at all.
  > **⛔ AND A LEGACY DECLARATION IS KILL-ON-SIGHT — THE `.def` IS NOT THE ONLY OUTLAW (owner).** An unpublished
  > legacy method on a wrapper is not harmless dead weight: it is the per-field contract still written down, so
  > the next agent reads it as the surface, a re-homed read COLLIDES with it, and "just publish what is already
  > declared" looks like the cheap fix at exactly the moment the new surface arrives. ⇒ The declaration AND its
  > body go, the moment they are seen — not scheduled as a later tidy-up
  > ([leave no evidence of the abandoned path](../../AGENTS.md#design): leftover evidence of the abandoned
  > path is what the next agent rollerskates off).
  > ⚑ **So a re-home does not have a collision PROBLEM — the collision is the work.** The legacy name dies as the
  > coherent read takes its place, and the wrapper converges on exactly the identity set plus the new surface.
  >
  > **⚖ AND THE COUNTERWEIGHT, WITHOUT WHICH KILL-ON-SIGHT UNDER-SERVES: WE MAKE SURE THE DECLARATIONS WE HAVE
  > ARE WHAT WE NEED, AND WE ARE NOT STINGY (owner) — we simply do not follow the legacy declarations because
  > they are "already used somewhere", which MOST OF THEM NO LONGER ARE.** The surface is designed from DEMAND
  > and freely given where a consumer genuinely needs a read
  > (§ endpoint COUNT is explicitly not the target — properly organized is); what it is never derived from is the
  > legacy list.
  > ⇒ **The two rules are one move, and each fails alone:** killing without serving pushes the next consumer
  > back onto legacy ([legacy must fail loud, never mask a cascade gap](../specs/validation.md#legacy-must-fail-loud-never-mask-a-cascade-gap)), and serving by preserving
  > the legacy set re-creates the per-field contract
  > ([build a new getter surface, never widen a legacy one](#-the-two-read-roles--one-grammar-two-answers-owner): a DELETION list *plus a COVERAGE checklist*).
  > ⛔ So "something might still call it" is NOT a reason to keep a declaration — verify the demand, and where a
  > read is genuinely wanted, ADD it as the coherent read rather than sparing the legacy one.
  > ⚠ **What is NOT killed with them:** the `class_<>` REGISTRATION and the identity set (the kept engine→Python
  > direction depends on both), and anything the ENGINE itself calls on the wrapper — the compiler names those,
  > and a compile error there is a worklist entry like any other.
  > ⚠ **What this does NOT license** is reviving the legacy `.def` field-by-field contract on the handle
  > ([build a new getter surface, never widen a legacy one](#-the-two-read-roles--one-grammar-two-answers-owner)): the reads a game-object accessor carries are
  > the coherent GROUP reads and named concepts of the new surface, homed on their own object — never the ~300
  > per-legacy-field getters re-registered because a screen once called them.
  > ⚑ **Each publish lives in the file named for its type** (`CyCity::pythonPublish`, the `CyInfo` pattern), never
  > piled into the composition root — the numbered-bucket shape (`CyGameCoreInterface1/2/3`) is the disorganization
  > this avoids (owner: *"when it's called CyPythonLoad1-4 or whatever, that's when it's silly, and
  > unorganized"*).
  > ⚠ This SUPERSEDES the earlier "a wrapper with no binding is the correct end state": zero-`.def` registration
  > was right about the DIRECTION (the read surface is gone) and wrong about the ADDRESS (a handle that cannot
  > name itself makes every legacy consumer a rewrite).

  > **⛔ "NO BINDING" MEANS NO `.def` — IT DOES NOT MEAN NO `class_<>`. The TYPE REGISTRATION IS THE KEPT
  > DIRECTION'S CARRIER, AND CUTTING IT BREAKS THAT DIRECTION.** A boost::python `class_<CyX>("CyX")` carrying
  > **zero `.def`s** is not a read surface — it publishes no getter and answers no question. It is the type
  > IDENTITY that lets an object cross the boundary at all: the marshaller (`Cy::PyWrap` → `makePythonObject`)
  > is `python::object(pObj)`, which **throws at runtime unless the type has a registered converter**. So the
  > engine→Python direction depends on the registration exactly as much as it depends on the callback.
  > ⚑ **The measure is mechanical, not a judgement:** a `Cy*` type is registration-REQUIRED iff any engine call
  > site passes it — `DECLARE_PY_WRAPPER(CyX, CvX*)` with at least one live `args << pCvX`, or a
  > `CvGameObject::createPythonWrapper` branch. `CyCity`, `CyUnit` and `CyPlot` each carry dozens of such sites
  > (the `CvDllPythonEvents` payloads, the `CvOutcome` hooks); `CySelectionGroup` declares the wrapper and has
  > **zero** call sites, so it genuinely needs none.
  > ⚠ **The same defect class reaches every published method whose RETURN type is a `Cv*`/`Cy*` object** — an
  > art-info accessor, an info-object accessor, a handle returning a city. Publishing the accessor without
  > registering what it returns yields a def that resolves and then raises at conversion: a `TypeError` where a
  > reader expects an `AttributeError`, which is why it reads as a mystery rather than as a missing binding.
  > ⇒ **When cutting a read surface, cut the `.def`s and KEEP the `class_<>` for any type the engine hands
  > across or hands back.** Deleting a whole registrar file takes both halves, and the second half is not yours
  > to take.
  >
  > **⚖ THE PLAIN VALUE STRUCTS ARE THE SAME RULE ONE LEVEL DOWN, AND THEIR FIELDS ARE NOT A READ SURFACE.**
  > The purge deleted the struct registrar whole (`NiPoint3`/`NiPoint2`/`NiColorA`/`POINT`/`IDInfo`/`OrderData`/
  > `MissionData`/…), and those are the MARSHALLING VOCABULARY, not handles: a coordinate pair or an RGBA
  > quadruple answers no question about game state, so `def_readwrite` on it is the VALUE ITSELF and
  > [the Cy* surface is not a fixed contract](#-the-python-read-boundary--one-complete-data-fetching-library-owner)'s ban — which is on the info/state GETTER contract —
  > does not reach it. ⛔ A struct registered without its fields is useless: Python cannot read the point it
  > was handed, nor build the colour it must pass.
  > ⚑ **They fail in BOTH directions, which is why the absence is easy to misread.** Python CONSTRUCTS some
  > (`NiColorA(0,0,0,0)` for the dot-map overlay) — those raise `NameError` at IMPORT. The engine RETURNS others
  > (`Win32::getCursorPos` → `POINT`, still published) — those resolve and then throw at CONVERSION, at first
  > use, far from the cut. Restore on DEMAND, named by the call site that wanted it.
  >
  > **⚖ AND WHERE A MAP SCRIPT DRAWS THROUGH THE HANDLE, THE OPERATION STAYS ON THE HANDLE — a named endpoint
  > beside it is the near-synonym duplication, not the fix.** `CvRandom` is the worked case: it is registered so
  > the handle can cross (`getMapRand` → the EXE's `shuffleList`), and Python also DRAWS from it — the map
  > scripts alone do so at dozens of sites (`CvMapGeneratorUtil`'s `mapRand`). Those are an OPEN EXTENSION
  > POINT whose contract is the named callbacks (§ MAP SCRIPTS below), so a third-party script cannot be
  > re-pointed and `get` has to exist on the type regardless.
  > ⛔ **So publishing a tidier `getASyncRandNum` on the config context and re-pointing the in-tree callers is the
  > wrong move twice over:** it leaves every map script still broken, and it creates a second spelling of one job
  > — *"3 similarly named endpoints that in essence do the same thing"*, which this section names as the actual
  > failure. ⚑ The test to apply: **can every caller be re-pointed?** If a map script or any other open extension
  > point is among them, the answer is no and the operation belongs on the type.
  > ⚠ It does reach the SYNCHRONIZED stream (`getSorenRand` hands that one across), but `getSorenRandNum` is
  > already published, so restoring the draw adds a SPELLING and not a POWER. What still binds is where a given
  > draw belongs: a cosmetic pick — which greeting variant a leader uses — is `getASyncRand`, because the synced
  > stream's draw COUNT is shared save state ([the synchronized RNG is shared state](../reference/engine.md#-the-synchronized-rng-is-shared-save-state--do-not-touch-the-draws-owner)).

**⛔ TWO THINGS THE LIBRARY DOES NOT OWN:**

- **TEXT/localization.** `getText`-style key→string resolution is not info data, and decisively: **TXT and ART
  keys are NOT MIGRATED** — both remain XML-side systems the JSON only REFERENCES ([json.md §7](../specs/json.md);
  [naming.md](../specs/naming.md)). So the library serves already-RENDERED lines and the raw key reference;
  resolution stays with the existing managers and Python screen chrome keeps calling the text system directly.
  This is an unmigrated system BOUNDARY, not a hole in the library.
  > **⛔ A FONT GLYPH IS TEXT-PLANE, NOT INFO DATA — and it is the case most likely to send a reader after a
  > deleted info accessor.** `CvYieldInfo::getChar()` and its kin LOOK like authored data and are not: the glyph
  > is a runtime GameFont slot the `CvGameTextMgr` symbol pass assigns via `setChar`, for seven registries
  > (yield · commerce · religion · corporation · property · invisible · bonus) that straddle the JSON/XML line —
  > so it is not info data on EITHER side, and no `get<X>Info` revival is the way to ask.
  > ⛔ **THREE ROUTES SERVE IT, SPLIT BY WHETHER THE REGISTRY IS FIXED-COUNT — and a reader who knows only one
  > concludes the glyph is unserved.** The split is not stylistic: a token is a literal STRING, so a registry
  > whose size is a runtime count can only be addressed by ID.
  > - **`CyGame.getSymbolID(FontSymbols.X)`** — the fixed engine symbols (happy, bullet, strength, …).
  > - **`CyTranslator().getText("[ICON_X]", ())`** — the `[ICON_*]` token map (`CvDllTranslator::initializeTags`,
  >   ours): the fixed symbols again, the 3 YIELDS and 4 COMMERCES by name, plus a token built PER ENTITY at load
  >   for **property** and **invisible** (`[ICON_<TYPE>]`, from the type key). ⚠ Those two are the only registries
  >   whose per-entity token exists, so `[ICON_" + typeKey + "]"` composes for them and for nothing else.
  > - **`CyGameTextMgr().getSymbolChar(prefix, id)`** — the symbol pass's own read, covering the five registries it
  >   assigns by id: `YIELD_` · `COMMERCE_` · `RELIGION_` · `CORPORATION_` · `BONUS_`. ⛔ **RELIGION, CORPORATION
  >   and BONUS have NO `[ICON_*]` token of any kind** — they are variable-count, so this is their ONLY route, and
  >   `[ICON_RELIGION]` is the generic religion symbol rather than a per-religion glyph. It returns an INT, so it
  >   substitutes for a `getChar()` under an existing `%c` with no format surgery.
  > ⚑ **Two registries carry a SECOND, distinct glyph**, each its own read: a religion's HOLY-CITY marker
  > (`getHolyCitySymbolChar`) and a corporation's HEADQUARTERS marker (`getHeadquarterSymbolChar`).
- **REVOLUTION's distance mechanic.** ⚠ `revolution.distanceMod` is **NOT dead** — Revolutions is
  Python-authoritative and consumes it through the player/city aggregates, which makes the read INVISIBLE to any
  engine-side grep. It is the standing exhibit for why an engine-read census cannot prove Python coverage. **Both
  distance kinds STAY AS-IS, untouched by any stage (owner):** Revolutions is due its own rework, and that rework
  owns every revolution-data question, including the two-spelling nuance.
- **MAP SCRIPTS.** They read map-gen types nothing else reads, run BEFORE most game state exists, are
  WRITE-dominated (they build the map; this is a read surface), and `eval` script-supplied expressions as an open
  extension point. Their contract stays the named Python CALLBACKS ([engine.md](../reference/engine.md)), so
  third-party scripts are unaffected by the `Cy*` cut and their types leave this library's coverage appendix.

## Materialize at mapFrom — no runtime string reads in info getters (the single-source law's load-time sibling)

> Binding: [materialize at mapFrom](#materialize-at-mapfrom--no-runtime-string-reads-in-info-getters-the-single-source-laws-load-time-sibling). Owner ruling: *"all of these should
> use the standardized jsonreader and be loaded properly into the info — remapping directly from a json read is a
> gigantic nono."*

**The law.** A `CvJson<X>Info` GETTER never does a per-call string-keyed read — no modifier-address sum
(`"happiness.city"` lookups), no bool-block `std::set<string>` walk, no grants/allowed bucket-string fetch, no raw
picojson re-read. Every such value is **materialized ONCE at `mapFrom`** into a typed member (scalar, positional
array, sparse id-keyed map, or a classification-id bitset), and the getter is a **bare member read**. The measured
why: these getters sit under the EXE frame loop (`unit.isInvisible` ~98M calls/turn-window), the pathfinder's
per-step gates, and the AI's per-candidate scans — a heap-string construction + map walk per call was a real
turn-time/FPS tax.

- **The ONE load-time scan source is the compiled `CvModifiers` entry list** (`entries()` — every §3.9 deposit
  as a typed `CvModEntry` with interned family/kind/scope/unit/target axes). A load-time pass (the DepositIndex
  push, the reverse passes, a poco materialization) iterates the typed entries; a getter never walks them — it
  reads the compiled `(family, kind, scope, unit)` slot sums (`sum100`) or its own materialized members.
- **Classification blocks read by GENERATED ID** — the §8/§9 bool blocks resolve their keys to the
  `ClassificationRegistry`'s runtime-minted ids ([the classification-infos registry](../specs/json.md#8-classification--unit-skillstagsstate-building-attributes--empire-capabilities)),
  and the getters are `CLS_HAS`/`CLS_COUNT` bit tests (memoized id + O(1) bitset read; the pre-resolve load window
  falls back to the string set so early consumers stay correct).
- mapFrom is idempotent by contract, so the materialized members are fully redefined on every (re-)map —
  clear-first for accumulating containers, unconditional assignment for scalars.
- **A CROSS-ENTITY value materializes in the REVERSE PASS's post-map derivation step, not at mapFrom.**
  `mapFrom` structurally cannot serve a value derived from *another* info's edges — it runs while the reverse
  view is still being built, so the view it would read is incomplete. The one home is a `rp_derive*` sub-pass
  inside `reversePassRun()` (`Data/CvReversePass.cpp`), calling the type's `deriveAtRegistryComplete()` once
  every entity is mapped and the RELATED/REQUIRED_BY families are landed; where the derivation needs a
  cross-registry fact, the PASS computes it once and FEEDS it in (the DRY shape — a machine never re-derives
  what another can hand it). Idempotent like its siblings: it fully redefines every member it fills.
  ⛔ The alternative — resolving on first read behind a memo — is BANNED, and not as untidiness: a memo puts a
  cache **and a staleness flag** on an info, which the INFO DATA-OUT contract above forbids *by construction*.
  ⛔ And it is ONE step, not a per-type habit: minting a second post-map hook beside this pass is the
  does-the-same-thing failure the enforcement check below exists to catch — reuse `deriveAtRegistryComplete`.
  *(Realized: the unit plane's SM base sums / derived era / upgrade-chain closure, and `CvHeritageInfo`'s
  acquisition prereqs — the tech and predecessor heritages whose `enables.heritages` list it.)*
- The cascade's own gated sums are NOT this surface — they are `MMKernel` over the compiled `DepositIndex`,
  running at mark-rebuild cadence, not per read.

## The ONE reader — the load pipeline law

> Binding: [exactly one JSON reader](#the-one-reader--the-load-pipeline-law). Owner rulings: exactly one JsonReader exists;
> JSON is read at GAME LOAD only; no string matching on any read path.

- **Exactly ONE JSON reader exists** — the load pipeline in `Sources/Data`, entry point `loadJson()`. The
  reader is **readJson**, the first of the four systems ([north-star.md](north-star.md)) — it is NOT the
  cascade, and no reader name carries a `cascade` prefix ([the enabler and the modifier cascade are two separate systems](../specs/enabler.md)'s
  naming guard, applied one system over). It enumerates `Assets/Data` once,
  parses each file ONCE into memory, registers every type→id before any `mapFrom` (the two-pass rule), maps every
  entity, runs the full-registry FK/reverse pass over the RETAINED in-memory parse (never a disk re-read), and
  compiles the routing index. **Every JSON-shaped object is freed before load ends.** A second parse call site
  anywhere in the tree is a defect, whatever it is named.
- **Fail-loud key coverage.** The reader accounts EVERY top-level key of every entity to exactly one consumer (a
  reserved-section parser or the modifier-family walk); an unconsumed key is a loud load-time report. "The info
  matches the JSON structure" is thereby a mechanical check, never an agent's self-assertion.
- **The `Json` name-fragment is reserved for the load-time parse surface** (the reader + the parse walkers). A
  runtime-resident type carries no `Json` in its name — so a `Json*`-named type living past load is, by its own
  name, misnamed or misplaced.
- **After load, nothing string-shaped remains readable** — the reader's half of
  [materialize at mapFrom](#materialize-at-mapfrom--no-runtime-string-reads-in-info-getters-the-single-source-laws-load-time-sibling): every served value is typed, id-resolved,
  and ×100 before the first turn runs.
