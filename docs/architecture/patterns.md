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
   — the exact mechanism of the C2C rot. *(Realized: BOTH data-machines are split into static-methods classes — the
   **deposit-read side** (`MMKernel`, the per-deposit leaf primitives, `Data/CvDepositRead.h`; `InfoValuation`,
   `Data/CvInfoValuation.h`, carrying StoneBase's higher `Calc/*` packages — the per-group walk, the `YieldRate`
   §2a combine `cityRate`, the `CommerceSplit` combine `commerceSplit`, the plot-as-base package, and the
   cross-scope roll-up) and the **enabler**
   (`EnablerKernel` + `TechEnabler` / `BuildingEnabler` / `UnitEnabler` / `CivicEnabler` / `ProcessEnabler` /
   `ProjectEnabler` / `PromotionEnabler` / `BuildEnabler`, each `Sources/Enabler/Cv<X>Enabler.{h,cpp}`,
   mirroring StoneBase `CascadingEnabler/*`).)*
5. **Harness ≠ calc.** The observability surface and the spine logging are
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
- **An info holds only ITS OWN side — cross-entity own-output lives on the TARGET (owner).** A building does not
  project yield onto an improvement; the **improvement** says *"I produce this much now, because a building is
  present"* — own-output, the building's presence a condition on the improvement's own deposit ([DEC-deliveryguy],
  modifier.md §4). So `CvBuildingInfo` carries no `improvements`/`terrains` yield map. This needs **no curator
  re-home**: the **load-time reverse structure** ([DEC-one-reverse-view]) builds cross-entity links both ways at
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
  [DEC-materialize-at-mapfrom](decisions.md#dec-materialize-at-mapfrom)), nothing flattened away, the §3.9
  mechanism UNREDUCED (`per`, the `ai` sibling, the `enabled`/`disabled` twin trees in their spec'd order —
  `enabled` first, a holding `disabled` OVERRIDES; a plural-target filter is the entry's own `enabled`
  predicate, [json.md §6.1](../specs/json.md)).
- **The ONE load COMPILE pass walks those entries ONCE and produces the runtime forms — after load, nothing
  ever walks the anatomy.** A **null-condition entry's value folds STRAIGHT into its group's compiled member
  array** — the enum-keyed `[kind × family-scope-set]` unconditioned ×100 sums, the grouped member pattern,
  scope-free kind names (Σflat vs Σpercent separate slots — the unit is part of the slot key,
  [modifier.md §2](../specs/modifier.md)). A **conditioned entry** lands in the group's compiled conditioned
  list, its condition tree prebuilt, evaluated ONLY at event-driven package rebuild and the per-decision
  `expected*` read — never re-parsed, never re-derived. Classification compiles to JSON-derived bitsets
  (`m_attributes` / `m_capabilities` / `m_skills` / `m_policies`); edges to the load-populated forward/reverse
  families ([DEC-one-reverse-view](decisions.md#dec-one-reverse-view)); intrinsic lone values to plain typed
  members. No string, no parse node, and no anatomy tree survives into a runtime read path.
- **⛔ THE SCOPE AXIS — a kind-enum names its CONCEPT ONLY; scope is a separate dimension**
  ([DEC-scope-is-an-axis](decisions.md#dec-scope-is-an-axis)). Scope is its own axis of the deposit address + a
  spelled-out getter parameter, NEVER a fragment of an enum, member, or getter name — a scope word (`GLOBAL`,
  `ALL_CITY`, `WORLD`, `AREA`, …) inside a kind name collapses two of the address's axes into bespoke per-pair
  entries. `getDefense(DEFENSE_AMOUNT, SCOPE_CITY)` — kind and scope are separate arguments, exactly as the
  JSON's own `<family>.<scope>.<member>` separates them.
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
  [contexts.md](contexts.md)), at per-decision cadence in the `expected*` read. The entity-level active/dormant
  verdict stays the ENABLER's, fed in via the precomputed operating set — a what-if read never re-evaluates
  `requires`.
- **THE GETTER SETUP — one exemplar shape for every info (the aim). Four read categories, nothing else:**
  1. **Sections** — whole typed objects the enabler + grants/provides machinery read: `getRequires()` /
     `getEdges()` / `getAllowed()` / `getGrants()` / `getProvides()` / `getWhenObsolete()`.
  2. **Classification** — O(1) bitset tests, the **name encoding hold-vs-provide** (owner, json.md §8): what the
     entity HAS is `hasAttribute(id)`/`hasAttributes()` (building) and `hasSkill(id)`/`hasTag(id)` (unit); what it
     PROVIDES to something else is `providesCapability(id)`/`providesCapabilities()` (to the empire) and
     `providesSkill(id)` (a grantor handing a skill on).
  3. **Modifier groups — three reads per group, all over the LOAD-COMPILED forms:**
     - the **straight point read** over the compiled unconditioned sum — `getDefense(DefenseKind eKind,
       ScopeKind eScope)` → one array load, **0 calculation** (kind and scope separate arguments,
       [DEC-scope-is-an-axis](decisions.md#dec-scope-is-an-axis));
     - the **compiled conditioned list** (`defenseConditioned()` / `yieldConditioned()` / … — the typed entries
       with prebuilt condition trees; what the package rebuild, the pedia, and the valuation walk);
     - the **what-if valuation** — the [contexts.md](contexts.md) per-GROUP endpoints
       (`expectedFlatYields(cityContext, empireContext, plotGroup, flatYields)` and siblings): the compiled
       sums fetched straight PLUS the group's conditioned tail through the ONE evaluator, `plots`-targets scaled
       by `cityContext.plotAttrs`, scopes folded into the experienced-here answer, the active/dormant verdict fed
       from the enabler. This IS the AI's *"what do I gain from building this?"* read.
  4. **Intrinsic** — bare typed reads (`getAirlift`, the shrine/corpHQ FKs, flavours), plus `getScalar(SCALAR_X)`
     for the 1–2-entry stragglers (genuinely lone unconditioned values).
  5. **The per-entry TEXT render (owner: "so that tooltips work properly")** — every compiled entry renders
     itself as ONE localized detail line (`+25% Production — while Coal connected`), the `detailLines` pattern
     of the combat calculator (`CvCombatModel::computeCombatPreview`'s itemised per-modifier breakdown),
     through ONE shared renderer ([DEC-single-implementation](decisions.md#dec-single-implementation)) — the
     tooltip/pedia composers consume rendered entry lines, never hand-assemble from getters. Cold path:
     spell-back segments + TXT keys are the honest cost there. **Structural consequence: the compiled entry
     list is COMPLETE — unconditioned entries are RETAINED as entries** (the folded sums are the derived fast
     plane beside them, never a replacement) — per-entry text and per-entry attribution both require the list.

     > **⚖ THE DIVISION OF LABOUR — `CvGameTextMgr` KEEPS THE BLOCKS AND LOSES THE SUB-BLOCKS (owner).** The
     > renderer is expected to remove *"the vast majority of bespoke work GameTextMgr used to do"*: the text
     > manager *"should only care about TXT_KEY replacements, and be the `Cy` target for actual string
     > content — it should not need to manually convert entries for each individual tooltip, or text box, when
     > that text conversion can be built programmatically."*
     > ⛔ **But the BLOCKS STAY, and the reason is what makes the line findable: *"the blocks are different
     > sources put together"*.** A block is a COMPOSITION — one heading over contributions from several
     > distinct sources (the building, the civic, the trait all feeding one happiness block) — so deciding
     > which sources compose it, in what order, under which TXT_KEY heading, is genuinely the text manager's
     > job and cannot be derived from any single entry list. ⛔ **What must never be hand-built is every
     > SUB-BLOCK** — the per-source render inside the block. That is one `appendEntryLines` call per (source,
     > family), and a block simply issues several.
     > ⚑ The practical test on any composer edit: if you are writing `getText` around a MAGNITUDE, you are
     > building a sub-block by hand and it is wrong; if you are writing `getText` for a HEADING or choosing
     > which sources belong together, that is the block and it is right.
     > ⛔ So a whole-entity "render every family at once" dump is NOT the shape — it flattens the composition
     > the blocks exist to express, which is why the surface is per-family.

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
  ([DEC-materialize-at-mapfrom](decisions.md#dec-materialize-at-mapfrom)).
- **THE SINGLE-THREAD BUDGET — why this shape is efficient on the one game thread.** The layering is the
  efficiency: (1) repeated hot reads (a BUILT thing's realized value) hit the package caches on the game objects
  ([state-repositories.md](state-repositories.md)) — O(1) bare fetches, never an info read; (2) **the anatomy
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
  per-turn wall clock ([DEC-turn-time-is-king](decisions.md#dec-turn-time-is-king)).
- **Every getter IS ×100** ([DEC-fixedpoint-x100](decisions.md#dec-fixedpoint-x100)) — no `getX`/`getX100` pair, no
  `100` suffix; the name says the VALUE, never the scale (always ×100). A reader wanting human does ÷100 at the
  boundary. The split lives in the flat-vs-modifier member name (`getFlatYield` vs `getYieldModifier`), never a
  scale suffix.
- **Extensible by DATA, not by new members/getters.** A new scalar family is a new `m_scalars` enum entry; a new
  property is a new id in `m_properties`; a new attribute is a new bitset key. The getter surface does not grow.
- Intrinsic self-description (`getAirlift`, `getMaxStartEra`, the shrine/corpHQ FKs, flavours) stays a bare typed
  read — genuine lone values, not a groupable set. The ~300 hand-named getters mirroring the legacy `CvXInfo`
  contract collapse into this surface, and consumers rewire onto it
  ([DEC-new-getter-surface](decisions.md#dec-new-getter-surface)) — the info half of the access surface.

## ⚖ THE TWO READ ROLES — ONE GRAMMAR, TWO ANSWERS (owner)

> The keystone of the ACCESS surface. The section above is the INFO half; this states what the info half and the
> GAME-OBJECT half share, and what must stay different. Binding:
> [DEC-new-getter-surface](decisions.md#dec-new-getter-surface).

**⛔ The new surface is NOT a replacement mapping of the existing getters (owner).** No legacy getter name,
signature, or shape survives into it. The measured 622 channel-shaped declarations on `CvCity`/`CvPlayer` are a
**DELETION LIST and a COVERAGE CHECKLIST** — the set of values that must be answerable somewhere on the new
surface — never a per-getter migration worklist. Mapping legacy→new one signature at a time is the
half-migration reflex in its purest form: it lets the legacy contract dictate the replacement's shape, which is
precisely how that surface accumulated.

**The two roles are DISTINCT, and the distinction is load-bearing:**

| role | asks | answers from |
|---|---|---|
| **INFO** | *"what do I CARRY?"* | authored data, compiled once at load — static, per-owner-agnostic, shared by every player |
| **GAME OBJECT** | *"what do I HAVE, right now?"* | live realized state — the roll-up over the ~5 scope packages, with per-city gates applied at the combine |

⛔ **They are NOT interchangeable and must never LOOK interchangeable.** Giving both the identical signature
invites a consumer to treat authored data and live state as the same answer — the shared-vocabulary trap that
[DEC-calc-zero-ride-in](decisions.md#dec-calc-zero-ride-in) exists to police from the other direction. Two roles,
two surfaces.

**What IS standardized is the GRAMMAR — both surfaces obey all of it:**

1. **⛔ ONE GETTER PER GROUP — the getter IS the group (owner).** `getYields()`, `getProperties()`,
   `getCommerces()`, … — the read hands back **the whole group**, and there is **NO scalar getter per channel**.
   A consumer wanting one value takes the group and indexes it. This is the standardization: the surface grows by
   GROUPS (a handful), never by channels (hundreds), and the per-channel scalar getter is the very shape the
   rebuild is deleting.
2. **The EXISTING ENGINE ENUM indexes the RESULT, not the call** (`YieldTypes`, `CommerceTypes`, …); a family
   with no engine enum uses its own kind enum (`CvInfoKinds.h`). So the enum stays the consumer's vocabulary
   while the call itself carries no channel argument. The data-minted channel id remains the CACHE's internal
   key and is never something a consumer learns.
3. **×100 native, always** — no `100` in any name, no `getX`/`getX100` pair, no scale variant
   ([DEC-fixedpoint-x100](decisions.md#dec-fixedpoint-x100)). A reader ÷100s at the point of use.
4. **Scope is a spelled-out ARGUMENT, never a name fragment** ([DEC-scope-is-an-axis](decisions.md#dec-scope-is-an-axis)).
5. **⛔ THE VALUATION PROTOCOL — THE LIVE CONTEXTS GO IN, THE PROPOSED INCREASE COMES OUT (owner).** The caller
   passes the live [contexts](contexts.md) and gets back **the DELTA** — what this candidate would ADD — never
   the raw percentage and never the new total.
   - **⛔ THE CONTEXT *IS* THE CURRENT VALUE — that is the whole point of it (owner).** A percent deposit has no
     value on its own: *"+25% production"* is worth a little in a small city and a lot in a large one, so it
     only becomes a number against the base it multiplies. The context supplies that base, because it is the
     bound live-state surface for its scope. ⛔ **Do NOT pass current amounts as a separate parameter** — that
     hands the read data the context already carries, and re-introduces the ad-hoc state-reach the contexts
     exist to end. A context that cannot answer a base the resolution needs is a **CONTEXT GAP to close by
     adding the forward** ([contexts.md](contexts.md)), never a reason to widen the signature.
   - **Why the DELTA comes out:** the question is *"what do I gain from this?"*. A delta is directly weighable;
     a new total forces every caller to subtract against a base it must fetch separately.
   - The contexts serve BOTH halves in one pass: they carry the base the percent resolves against (the
     `CityContext` forwards the city's CURRENT REALIZED YIELDS for exactly this, [contexts.md](contexts.md)),
     and they are what the compiled CONDITIONED tail is evaluated over (*"+25% more while coal is connected"*).
   - **⚑ TWO CONSUMERS, ONE CALL (owner): the AI's evaluation AND the build-list HOVER TOOLTIP.** The same
     valuation answers *"what do I gain from this?"* for the AI weighting it and for the player reading the
     tooltip. That is not a convenience — it is what makes the displayed number and the acted-on number the
     SAME number, structurally. The classic failure it removes is a UI advertising one value while the AI plans
     against another, which no amount of care prevents once they are two implementations
     ([DEC-single-implementation](decisions.md#dec-single-implementation)). It is also why the resolved DELTA is
     the right return: it is simultaneously what an AI weight multiplies and what a tooltip line prints.
6. **⛔ A GROUP HANDS OUT ITS CHANNELS; A FINAL-STATE CALCULATION IS DOWNSTREAM OF IT (owner).** The wellbeing
   group returns `happiness` and `anger` as **two separate numbers** (and `health`/`unhealth` likewise) — *"then
   you will know the results from that"*. The realized end-state values (`angryPopulation`, `healthRate`) are
   **NOT group entries and NOT getters**: they are a final-state calculation over numbers the group already
   handed out ([modifier.md §2b](../specs/modifier.md) specs the arithmetic). ⛔ Folding a final-state value into
   the channel array is a category error — it puts a computed OUTCOME in a slot that means "a channel a source
   deposited into", and it hides the opposing-pair structure the four channels exist to express. The calculation
   still exists **exactly once** as a pure static function on the calc surface
   ([DEC-single-implementation](decisions.md#dec-single-implementation)); it is simply not part of the read.
7. **The group read FILLS A CALLER-OWNED ARRAY** — one call in, the whole group out, indexed by the group's
   enum. Passing state once and getting the whole resolved group back is also what keeps a future
   whole-candidate snapshot possible without building it now; a design answering one scalar per call would
   foreclose it *and* would re-resolve the same state per channel.
6. **Extensible by DATA, not by new members/getters** — a new channel is a new id, not a new function.
7. **Parameters spelled in full**, index parameters named for the enum they key
   ([contexts.md](contexts.md) naming rule).

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

> The Python half of the access surface. Binding: [DEC-cy-not-fixed](decisions.md#dec-cy-not-fixed) — the `Cy*`
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
> — **169 files star-import it against 6 with an explicit list** — so NOTHING declares a dependency on any
> single binding. A removed `.def` causes no import-time failure at all; it surfaces at the one call site that
> used it. ⛔ So do NOT slow a cut down to protect a binding, and do not build a resolver to prove one is safe
> first — the compiler names the dead ones for free, and being wrong costs one call site, not a regression.

Four words carry the whole requirement:

- **ONE SURFACE.** A single library IS the Python-facing read boundary — not the per-type `Cy*` interfaces it
  replaces, not a widened binding, never two live surfaces for one read.
- **COMPLETE.** Every read Python performs today has an answer **before** the legacy surface is disconnected.
  Completeness is the GATE, not an aspiration: one gap forces a reach-around into legacy, and that reach-around
  IS the second live surface the ruling forbids — the half-migration re-created at the last seam.
- **DATA FETCHING, not gameplay.** It serves reads/payloads; Python-authoritative gameplay (Revolution, events)
  stays Python and becomes a CONSUMER of it.
- **⛔ ENUM OPERATIONS ARE FIRST CLASS** — name→type/enum resolution is a supported operation, covering
  **resolution AND EXTENSION**: BUG resolves `WidgetTypes`/`InputTypes`/`InterfaceDirtyBits` by name from config
  strings *and* MINTS new `WidgetTypes` members at runtime, handing them back as widget ids. A read-only lookup
  does not serve that, and three engine enums are reachable ONLY this way — so a library lacking it forces the
  banned reach-around. It generalizes `getInfoTypeForString` and mirrors the load-minted classification
  registries ([DEC-classification-infos](decisions.md#dec-classification-infos)).

**⚑ BUILD IT FOR THE PEDIA — but know exactly what that proves (owner).** The pedia's purpose is to display every
entity exhaustively, so it is not a sample of the info surface, it **is** the info surface rendered. Therefore:

- **SHAPE — complete by construction.** Nothing in Python needs a payload shape the pedia does not already force,
  so serving the pedia SETTLES the library's structure; no later consumer introduces a new kind of read.
- **⚠ COVERAGE — NOT proven, and the gap is enumerable.** The pedia is ~99.7% a static reader: it exercises a
  fraction of STATE, almost no COMPUTED and no MUTATION, so a large part of the Python surface sits in planes it
  never touches. The residue is an appendix — whole info types with no pedia page (map-gen, game-config,
  diplomacy/victory/vote, command/UI-action) plus per-field reads. **Serving the pedia completes the INFO plane
  and the shapes; it does not complete the boundary.** Treating it as a coverage oracle is the mistake to avoid.

**⛔ THE CUT IS DIRECTIONAL — only the READ surface dies (owner).** The bridge runs both ways, and #430 owns one
of them:

- **Python → engine READS** (the `Cy*` info/state bindings) — this is what the library replaces, and the binding
  surface is GONE.
- **Engine → Python CALLBACKS** — **NEEDED, and kept (owner: "we need eventreporter, we need mapscript, amongst
  other things")**. `CvEventReporter`, the map-script hooks and `CvOutcome`'s Python outcomes are what makes
  Python-authoritative gameplay possible at all, so this is REQUIRED FUNCTIONALITY, not a deferral —
  [DEC-no-deferred](decisions.md#dec-no-deferred) does not apply to it and it is not a thing to "finish later".
  ⚠ The list is open ("amongst other things"): treat a callback you find as kept unless ruled otherwise.
  ⚖ **KEPT THROUGH #430, not kept forever — the successor is named (owner): `CvEventReporter` is replaced by
  the TRIGGERS machine and events move INTO C++, "but that is not 430."** So this is a SCOPE boundary with a
  known destination, not a permanent Python carve-out — do not read "permanent carve-out" on the event surface
  as "Python owns this forever", and equally do not start the move inside #430. The triggers machine
  ([triggers.md](../specs/triggers.md)) is where it lands when its own work item is taken.
- ⚑ Consequence: the `Cy*` WRAPPER classes (`CyCity`/`CyUnit`/`CyPlayer`/…) STAY while their bindings do not —
  33 engine files hold them for that direction. **A wrapper with no binding is the CORRECT end state here**, and
  reading it as a half-cut to complete would delete working gameplay.

**⛔ TWO THINGS THE LIBRARY DOES NOT OWN:**

- **TEXT/localization.** `getText`-style key→string resolution is not info data, and decisively: **TXT and ART
  keys are NOT MIGRATED** — both remain XML-side systems the JSON only REFERENCES ([json.md §7](../specs/json.md);
  [naming.md](../specs/naming.md)). So the library serves already-RENDERED lines and the raw key reference;
  resolution stays with the existing managers and Python screen chrome keeps calling the text system directly.
  This is an unmigrated system BOUNDARY, not a hole in the library.
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

- **The ONE load-time scan source is the compiled `CvModifiers` entry list** (`entries()` — every §3.9 deposit
  as a typed `CvModEntry` with interned family/kind/scope/unit/target axes). A load-time pass (the DepositIndex
  push, the reverse passes, a poco materialization) iterates the typed entries; a getter never walks them — it
  reads the compiled `(family, kind, scope, unit)` slot sums (`sum100`) or its own materialized members.
- **Classification blocks read by GENERATED ID** — the §8/§9 bool blocks resolve their keys to the
  `ClassificationRegistry`'s runtime-minted ids ([DEC-classification-infos](decisions.md#dec-classification-infos)),
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
  cache **and a dirty flag** on an info, which the INFO DATA-OUT contract above forbids *by construction*.
  ⛔ And it is ONE step, not a per-type habit: minting a second post-map hook beside this pass is the
  does-the-same-thing failure the enforcement check below exists to catch — reuse `deriveAtRegistryComplete`.
  *(Realized: the unit plane's SM base sums / derived era / upgrade-chain closure, and `CvHeritageInfo`'s
  acquisition prereqs — the tech and predecessor heritages whose `enables.heritages` list it.)*
- The cascade's own gated sums are NOT this surface — they are `MMKernel` over the compiled `DepositIndex`,
  running at dirty-rebuild cadence, not per read.

## The ONE reader — the load pipeline law

> Binding: [DEC-one-json-reader](decisions.md#dec-one-json-reader). Owner rulings: exactly one JsonReader exists;
> JSON is read at GAME LOAD only; no string matching on any read path.

- **Exactly ONE JSON reader exists** — the load pipeline in `Sources/Data`, entry point `loadJson()`. The
  reader is **readJson**, the first of the four systems ([north-star.md](north-star.md)) — it is NOT the
  cascade, and no reader name carries a `cascade` prefix ([DEC-enabler-not-cascade](decisions.md#dec-enabler-not-cascade)'s
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
  [DEC-materialize-at-mapfrom](decisions.md#dec-materialize-at-mapfrom): every served value is typed, id-resolved,
  and ×100 before the first turn runs.
