# #430 roadmap — the cascade rebuild

> **The master plan for the active work.** Mandated session-start reading (root `AGENTS.md`). It states the design
> the code must conform to, what exists against it today, and what is open. Governing rulings are ledgered as
> `DEC-*`; this doc links them, it does not re-articulate them.
>
> **⚑ Branch `cascade-rebuild` is a deliberate CLEAN SLATE and does not compile. That is the intended state**
> (owner): *"I could not possibly care less if this compiles; having a clean slate to do this right is the target."*
> A red tree is NOT a defect to fix by re-attaching what was archived — see §What was archived, and why.

## Context — why this rebuild exists

The #430 design is specced and accepted; the DATA is solid (every gameplay entity curated, no placeholders —
`Assets/Data/**`, `Tools/Migration/curate_*.py`). What drifted was the C++ runtime, and it drifted in one
characteristic direction: **each scope, channel, and read site grew its own bespoke shape.** Five per-scope package
structs with hand-named per-channel members, ~33 hand-named scalar fields, a read-side `ensure()` protocol, one
shared spine consumer routing two different machines, and a legacy-getter surface the cascade was bent to fit.

Each of those was individually defensible and collectively fatal: a hand-named field cannot be addressed
uniformly, so every one of them forced its own invalidation path, which is precisely how that many accumulated.

**So the substrate was archived rather than patched.** The proven machines were separated into their own trees, the
engine game-object classes were reverted to `main`, and the drifted modifier substrate was moved out of the
codebase entirely. This is not a redesign — the design below is unchanged and still authoritative. It is a rebuild
of the *implementation* against it, once, properly ([DEC-proper-once](../../architecture/decisions.md#dec-proper-once)).

## Governing principles (ledgered as DECs)

1. **No "deferred."** Anything marked deferred / parked / blocked / "later" / TODO / pending is a
   **failure to fix**, not a backlog item ([DEC-no-deferred](../../architecture/decisions.md#dec-no-deferred)).
2. **No self-heal.** No blanket per-turn/per-slice rebuild papers over a missed invalidation; correctness comes only
   from complete, targeted, spine-routed invalidation, and a miss must surface as a live divergence
   ([DEC-no-self-heal](../../architecture/decisions.md#dec-no-self-heal)).
3. **The cascade is built and kept ENTIRELY from events.** Full build ONLY on load (the reseed); post-load only
   dirty packages rebuild ([DEC-spine-reseed](../../architecture/decisions.md#dec-spine-reseed)). Steady-state
   per-turn cost tracks *what changed* (thousands), never *what exists* (millions).
4. **The keystone — self-invalidating per-package caches.** Each package is its own cache; a DOMAIN event marks
   exactly the packages its source (per the deposit index) feeds. *"This is the basis of EVERYTHING."*
5. **ONE cache shape everywhere** — the same object type on every owner, all invalidating the same way, only WHICH
   SLOTS carry a value varying by scope. A hand-named scalar field is a DEFECT
   ([DEC-uniform-cache-shape](../../architecture/decisions.md#dec-uniform-cache-shape)).
6. **Universal yield.** ANY number game mechanics modify is a channel in ONE machine in ONE uniform package format
   ([DEC-universal-yield](../../architecture/decisions.md#dec-universal-yield)).
7. **A NEW getter surface; the old one disconnected.** Reusing a legacy getter is the MECHANISM that produces the
   half-migrated state, not a shortcut that merely risks one
   ([DEC-new-getter-surface](../../architecture/decisions.md#dec-new-getter-surface)).
8. **One consumer per system.** The enabler and the modifier are two separate machines
   ([DEC-enabler-not-cascade](../../architecture/decisions.md#dec-enabler-not-cascade)); a shared consumer welds
   them and forces one load policy onto two that genuinely differ.
9. **Done = observable in the running game** via an endpoint poll — never "the code path exists" or "the data
   loads" ([DEC-done-is-observable](../../architecture/decisions.md#dec-done-is-observable)), and the per-turn
   `(scope,channel)` calc-count stays under the 50k gate
   ([DEC-calc-count-gate](../../architecture/decisions.md#dec-calc-count-gate)).

## The accepted foundational design (unchanged — the code conforms to this)

Authority: [state-repositories.md](../../architecture/state-repositories.md), [modifier.md](../../specs/modifier.md),
[enabler.md](../../specs/enabler.md), [event-spine.md](../../specs/event-spine.md).

- **The event spine is the foundation.** One `emit(KIND,…)` fanned by KIND to `IEventConsumer`s. Nothing else in the
  engine detects changes.
- **The EMIT surface comes first; the cache build is the step AFTER.** Caches cannot build from events until the
  events are completely emitted — during play AND from inside the save read.
- **Load and new-game are the SAME path.** Facts are order- and prerequisite-independent; the reseed is the ONE
  full build.
- **Per-scope packages, one uniform format** — Σflat / Σpercent per channel, cached at each scope's OWN object,
  each invalidated at its own scope only. The downward roll is realized AT READ: the realized value is the trivial
  sum of the ~5 scope packages. A lower scope never STORES an upper scope's sums.
- **The ORIGIN RULE** — yields come from PLOT, SPECIALISTS and BUILDINGS (city) only; modifiers come from
  everything BUT plot. Plot and the upper scopes are mirror images; CITY is the one scope carrying both.
- **KEYS ONLY WHERE NEEDED** — the channel set is data-defined and no object uses more than a fraction of it; each
  scope carries only the channels authored AT that scope, derived from the data at load, never hand-listed.
- **A cascade is a cache, two kinds** — the yield/percent packages are a VALUE cache (memoize, event-invalidate,
  recompute-from-inputs). The ENABLER's sets are maintained by TARGETED PROPAGATION in place, NEVER
  blanket-recomputed. Do not conflate them.
- **THE OUTPUT-SEAM** — where the engine does placement/application, the cascade owns the authored INPUTS and the
  OUTPUT yields; only the middle mechanism is engine-owned (free-specialist assignment; the golden-age plot
  threshold).

## What EXISTS on this branch (verified against the tree)

| Machine | Home | State |
|---|---|---|
| Event spine + KIND firewall + `IEventConsumer` | `Sources/Spine/` | BUILT |
| DOMAIN emit surface + the in-read load reseed + the load bracket | `Sources/Spine/` + the engine read paths | BUILT |
| Enabler (8 domains, kernel, own consumer, operating-buildings) | `Sources/Enabler/` | BUILT — **hostless**, see below |
| Condition evaluator (`cascadeEvalCondition`, eval ctx, predicates) | `Sources/Conditions/` | BUILT |
| Deposit index + deposit-read calcs (`MMKernel`/`PercentStack`/…) | `Sources/Data/` | BUILT |
| readJson + the two-pass loader + the full-registry re-map | `Sources/Data/` | BUILT |
| Info pocos + repos (all 23 replaced types + the 11 uniformity types) | `Sources/Infos/`, `Sources/Repos/` | BUILT |
| Tally (read-only accessor over object-owned counts) | `Sources/Tally/` | BUILT |
| Grants engine | `Sources/Grants/` | Resolver BUILT · **apply-loop NOT built** |
| Property feed + channel | `Sources/Property/` | BUILT (engine math is KEEP-legacy) |
| Save soft-remove drain (`savemigration.txt` + `sm_isCut`) | `Sources/Infrastructure/` | BUILT |
| Derived-cache component (`CvDerivedCache`/`Set`/`Vec`) | `Sources/Infrastructure/` | BUILT |
| HTTP transport (sockets, mailbox, `/events` SSE, `/` liveness) | `Sources/Tools/` | BUILT — **routes purged** |

## What does NOT exist (the deliberate gap)

- **The modifier substrate.** Archived. There is no package storage, no accumulator, no modifier spine consumer.
- **The graft onto the game objects.** `CvCity` / `CvPlayer` / `CvPlot` / `CvArea` / `CvTeam` are reverted to
  `main`, so they carry no cache member and no enabler member. **The enabler is complete and hostless** — it is
  waiting on the access surface, not on enabler work.
- **The endpoint route table.** Purged wholesale; the transport survives.
- **The read surface itself** — see below. This is the open item.

## ⛔ THE OPEN ITEM — the ACCESS surface

> *"What we ultimately want is settled in the spec. What is not done is defining what and how things are
> accessed."* (owner)

Everything above is settled. What is NOT defined is the boundary every consumer meets:

- **What the uniform getter set looks like** — the parameterized read over the channel index that replaces the 360
  channel-shaped getters on `CvCity.h` + `CvPlayer.h` alone. Its shape decides what the packages must store, so it
  is upstream of re-grafting anything.
- **How a scope owner carries its cache** — the member, its binding, and its mark derivation, uniform across
  world / team / empire / area / city / plot.
- **The per-scope live-state CONTEXTS the getters + evaluator read** ([contexts.md](../../architecture/contexts.md),
  [DEC-scope-contexts](../../architecture/decisions.md#dec-scope-contexts)) — one per scope that needs it
  (plot / city / player; NO area — a bare id whose effects map to the player; units are a FUTURE role-specific
  scope). Each STORES only its uniquely-owned aggregate (COUNTS via the shared `ContextDict` —
  `CityContext.plotAttrs`, `EmpireContext.policies`) and FORWARDS everything already O(1) on the bound game object;
  maintained EVENT-DRIVEN, no recompute. **BUILT:** `ContextDict` + `CityContext` (on `CvCity`, forwarding; its
  `plotAttrs` wired via `CvPlot::updateWorkingCity` → `CvCity::onCityPlotChanged`) + `EmpireContext` (on `CvPlayer`,
  forwarding `stateReligion`), both bound in `reset()`. **OPEN:** `PlotContext`; the `EmpireContext.policies` union
  maintenance; the load reseed of both; and the `(cx, pg)` getter bodies that read them.
- **How the INFO side hands its data to the cascade — "make the infos sane" (active).** Today an info IS the legacy
  variable set (220 members on `CvBuildingInfo`, 247 on `CvUnitInfo`), with JSON force-fed into it and a
  ~300-getter surface mirroring the legacy `CvXInfo` contract. The target — **an info STYLED FOR THE JSON**:
  members mirror the JSON anatomy, getters are ×100-native (no `100` in any name) and coherent (data-out by
  channel), generalizing the in-tree `CLS_HAS`/bitset classification pattern — is
  [patterns.md § The INFO DATA-OUT contract](../../architecture/patterns.md). The build sequence: (1) target shape
  written; (2) rebuild `CvBuildingInfo` to it as the proven pattern (fattest, and it already carries both the
  legacy-scalar defect and the sane `CLS_HAS` cure side by side); (3) roll across the other infos + rewire
  consumers onto the coherent surface ([DEC-new-getter-surface](../../architecture/decisions.md#dec-new-getter-surface)).
  An info holds **only its own side**: cross-entity own-output (a building's improvement/terrain yields) is NOT a
  building member — the improvement owns its yield, conditioned on the building's presence
  ([DEC-deliveryguy]). The info is shaped to that NOW; it is not distorted to hold data it shouldn't just because
  the delivery mechanism isn't built (a red tree loses no live data).
- **The GENERAL modifier own-output reverse-map — required work (owner: "we will generalize it").** At load, any
  source's target-keyed own-output deposit (`<yield>.<scope>.{improvements|terrains|…}.{TARGET}`) is reverse-landed
  on the TARGET as a conditioned own-output ("+X while the source is present"), so a modder authors either side and
  both carry it. Today the reverse pass (`CvReadJson.cpp`) does this only via **hand-built per-relationship
  indexes** (route←bonus, improvement←route-yield) plus the edge reverses — NOT a general mechanism, so building→
  improvement yields are not landed. Generalizing it to one pass over every source's compiled deposits is what
  makes the own-output model true for every info at once and lets each drop its target-own-output maps for free
  ([DEC-one-reverse-view], modifier.md §4). Verified live, never on a promise.
  **The cut is FULL (owner): the new coherent surface is built and every consumer rewired onto it in the same
  pass, the legacy getter names disconnected — never a thin-compat layer left breathing.** The red tree makes the
  blast radius free to absorb; a change that left consumers untouched would be the half-migration tell.
- **The new Python surface** — built from the cascade/JSON model, with the legacy `Cy*` surface CUT AWAY
  ([DEC-cy-not-fixed](../../architecture/decisions.md#dec-cy-not-fixed)). Not a widened binding, not a shim
  beside it, not two live surfaces.
- **The endpoint route table**, which reads the same uniform getters as everything else.

⛔ **Do not start re-attaching machines to the game objects before this is defined.** A per-site rewire is exactly
the half-migration this rebuild exists to undo, and it is what every previous attempt did while believing it was
conforming.

## Work that survives the rebuild (unaffected by the substrate archive)

These are data/curator/audit items whose subject never lived in the archived substrate:

- **F7 — the data tail.** [data-migration-remaining.md](data-migration-remaining.md): NPC civs /
  `stronglyRestricted`, `state`/paralyze, the corporation rework, the leaderhead trait remap, ranked-target
  selection. Leaders ship TRAITLESS by owner ruling; the community re-adds traits post-merge.
- **UnitCombat distillation** — [unitcombat-distillation.md](unitcombat-distillation.md) + the tag-mapping and
  merge-candidate worklists. Owner realization: the `UnitCombat` god-group must be distilled before the migration
  can finish, because it is the common blocker under the keyed "vs unit-combat-class" combat modifiers, the upkeep
  military/civilian bucketing, and the `IS_<tag>` predicate surface.
- **The grants apply-loop** — [grants-machine.md](grants-machine.md) + [grant-apply-sites.md](grant-apply-sites.md)
  + [start-packages.md](start-packages.md). Still the biggest feature gap: grants resolves but does not apply.
- **Poco stubs** — [stub-census.md](stub-census.md). Getters returning a constant where legacy computed a real
  value, feeding live consumers wrong numbers. **Reproduce-not-default:** a poco getter whose value is not curated
  JSON must reproduce legacy's mechanism, never a stand-in 0/-1/empty.
- **The property source data** — [property-audit.md](property-audit.md) (LOCKED, owner-approved). The engine math
  is intact and must NOT be rewritten.

## Observability / acceptance

Unchanged in principle, but note the surface it depends on is currently purged:

1. **The 50k `(scope,channel)` calc gate** — reset at `doTurn` top, exposed on the perf endpoint. Total/turn under
   50k (thousands steady-state, ~0 on a quiet turn); the histogram names the culprit on a breach.
2. **Manifestation polls** — PROGRAMMATIC against the `/computed` oracle endpoints, never eyeballing the screen. A
   blind value is EMITTED first; emitting it is step one of that item's fix.
3. **StoneBase** — repurposed from offline parity oracle to the user-visible PERFORMANCE layer
   ([DEC-verify-in-game-not-reshadow](../../architecture/decisions.md#dec-verify-in-game-not-reshadow)). Parity and
   shadow are CLOSED and are NOT re-run, re-invoked, or used to frame remaining work.

## Verification targets

- **Turn time (owner): ≤ 2 minutes wall per turn** on the standing late-game test save. The
  [DEC-turn-time-is-king](../../architecture/decisions.md#dec-turn-time-is-king) objective made concrete. The perf
  hunt resumes only after the caches are event-wired and the game runs behaviourally as it used to.
- **The MEMORY hunt stays parked** by the same sequencing ruling: chasing per-turn memory is pointless until legacy
  is gone and everything runs on the cascade + enabler, because the growth is turn-processing-borne and that
  processing is what the rebuild replaces ([memory-footprint.md](../../reference/memory-footprint.md)).
- Build: `powershell.exe -NoProfile -ExecutionPolicy Bypass -File "../Tools/_Build.ps1" <Config> <verb>` from
  `Sources/` (Release for interactive testing; FinalRelease for perf). Runtime verification needs per-session owner
  permission and goes through `agentstart.bat` only.

## Scope decisions

1. **Backlog scope = the #430 critical path only.** The `docs/plans/parked/` forward-FEATURE backlog is OUT.
2. **Python = boundary redesign + fix values.** Do NOT pull Python-authoritative gameplay into the DLL.
3. **NOT failures — deliberate, owner-ruled permanent carve-outs:** the golden-age YIELD-EFFECT member-mirror
   ([golden-age.md](../../reference/golden-age.md)); the mission-CONCEPT unification and the Python-authoritative
   outcome hooks; random EVENTS; Revolution. *(The `CvOutcome` DATA itself IS migrated to JSON.)*
