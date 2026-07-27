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
   loads" ([DEC-done-is-observable](../../architecture/decisions.md#dec-done-is-observable)) — and the per-turn
   wall clock stays inside the target ([DEC-turn-time-is-king](../../architecture/decisions.md#dec-turn-time-is-king)).

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
| DOMAIN emit surface + the in-read load reseed + the load bracket | `Sources/Spine/` + the engine read paths | ENDPOINTS BUILT — CALL SITES SEVERED by the revert (2 rewired; the bracket unemitted at BOTH ends): [info-rebuild.md](info-rebuild.md) audit ledger |
| Enabler (8 domains, kernel, own consumer, operating-buildings) | `Sources/Enabler/` | BUILT — **hostless**, see below |
| Condition evaluator (`cascadeEvalCondition`, eval ctx, predicates) | `Sources/Conditions/` | BUILT |
| Deposit index + deposit-read calcs (`MMKernel`/`PercentStack`/…) | `Sources/Data/` | BUILT |
| readJson + the two-pass loader + the full-registry re-map | `Sources/Data/` | BUILT |
| Info pocos + repos (all 23 replaced types + the 11 uniformity types) | `Sources/Infos/`, `Sources/Repos/` | BUILT |
| Tally (read-only accessor over object-owned counts) | `Sources/Tally/` | BUILT |
| Grants engine | `Sources/Grants/` | Resolver + APPLIERS built (tech first-discover, building first-build, per-turn, spawn, full-heal), consuming the restored DOMAIN emits and suppressed inside the load bracket so a reseed never re-grants; remaining increments: [grants-machine.md](grants-machine.md) |
| Property feed + channel | `Sources/Property/` | BUILT (engine math is KEEP-legacy) |
| Save soft-remove drain (`savemigration.txt` + `sm_isCut`) | `Sources/Infrastructure/` | BUILT |
| Derived-cache component (`CvDerivedCache`/`Set`/`Vec`) | `Sources/Infrastructure/` | BUILT |
| HTTP transport (sockets, mailbox, `/events` SSE, `/` liveness) | `Sources/Tools/` | BUILT — **routes purged** |

## What does NOT exist (the deliberate gap)

- **The modifier substrate.** Archived. There is no package storage, no accumulator, no modifier spine consumer.
- **The ENABLER's graft onto the game objects.** `CvCity` carries no `m_operatingBuildings` and `CvTeam` no
  `m_cascadeTeamCaps`, so `CvCapabilities` / `CvEnablerKernel` reference members that do not exist. **The enabler is
  complete and hostless** — it is waiting on the access surface, not on enabler work.
  ⚠ The MODIFIER half of the graft **has since landed**: `CvCity` / `CvPlayer` / `CvPlot` / `CvTeam` each carry
  `m_cascadePackage` and `CvArea` carries `m_cascadeSlots[MAX_PLAYERS]`, bound in each owner's `reset()`, alongside
  the three contexts. Only the enabler-side members are still missing.
- **The endpoint route table.** Purged wholesale; the transport survives.
- **The read surface itself** — see below. This is the open item.

## ⛔ THE OPEN ITEM — the ACCESS surface

> *"What we ultimately want is settled in the spec. What is not done is defining what and how things are
> accessed."* (owner)

Everything above is settled. What is NOT defined is the boundary every consumer meets:

- **What the uniform getter set looks like** — the parameterized read that replaces the hand-named channel-shaped
  getters on `CvCity.h` + `CvPlayer.h`: **622 declarations / 586 distinct names, measured** (wellbeing alone is 23%;
  235 name a SOURCE and 255 carry a target-id argument — two axes the package does not have). Its shape decides what the packages must store, so it is upstream of
  re-grafting anything. Two owner rulings fix its direction:
  - **⛔ NOT the existing getters — ONE NEW COHERENT SURFACE, STANDARDIZED ACROSS THE INFOS (owner).** The design
    task is **not** "find a replacement for each of the 622": no legacy getter name, signature, or shape survives
    into it. The 622 are a **DELETION LIST plus a COVERAGE CHECKLIST** — the set of values that must be answerable
    somewhere on the new surface — and nothing more. The shape comes from ONE standard, taken from the info
    exemplar ([patterns.md § THE GETTER SETUP](../../architecture/patterns.md)) and applied as uniformly as it can
    be made to go: per-GROUP reads parameterized over the group's natural index, ×100 native with no `100` in any
    name, scope a spelled-out argument
    ([DEC-scope-is-an-axis](../../architecture/decisions.md#dec-scope-is-an-axis)), extensible by DATA rather than
    by new members. ⚠ **Mapping legacy getter → new getter is the half-migration reflex in its purest form**
    ([DEC-new-getter-surface](../../architecture/decisions.md#dec-new-getter-surface)): it lets the legacy
    contract shape the replacement one getter at a time, which is exactly how the surface got here.
  - **⚖ EXISTING ENGINE ENUMS ARE THE PARAMETER VOCABULARY where one exists (owner)** — `YieldTypes`,
    `CommerceTypes` and their kin are what a group read is keyed on; a family with no engine enum uses its own
    kind enum (`CvInfoKinds.h`). The data-minted channel id stays the CACHE's internal key, never something every
    consumer learns.
  - **⚖ BUILD THE BASE FIRST; "the most efficient way" comes AFTER (owner).** The AI eventually wants *"a full
    'this is the improvements' snapshot for a building, or a set of buildings, so it can evaluate the results
    against its weights"* — a one-call read a caller weights itself. It is a real direction to **KEEP IN MIND,
    not to actively solve** (owner): tuning the shape of a surface that does not yet exist optimizes nothing.
    ⛔ Do not build, investigate, or pre-shape it ahead of the base. **What "keep in mind" DOES bind:** the base
    must not FORECLOSE it — a per-group read that can be filled into a caller-owned array leaves the door open;
    a design that could only ever answer one scalar per call would close it.
    **The efficiency that matters is ALREADY BANKED IN THE SPEC** — the event-driven frontier is the structural
    win (the AI iterates a small maintained choice set instead of scanning the entity database; every read is an
    O(1) lookup that never calls a calculator, [enabler.md §6/§7](../../specs/enabler.md)), alongside
    event-built contexts turning per-read scans into stored fetches
    ([contexts.md](../../architecture/contexts.md)). A snapshot is a refinement on top of that, not the source
    of the gain.
- **How a scope owner carries its cache** — the member, its binding, and its mark derivation, uniform across
  world / team / empire / area / city / plot.
- **The per-scope live-state CONTEXTS the getters + evaluator read** ([contexts.md](../../architecture/contexts.md),
  [DEC-scope-contexts](../../architecture/decisions.md#dec-scope-contexts)) — one per scope that needs it
  (plot / city / player; NO area — a bare id whose effects map to the player; units are a FUTURE role-specific
  scope). A context is an **EVENT-BUILT STORE, not a forwarding facade** (owner): it STORES every DERIVED fact the
  evaluation reads — computed once, maintained by spine facts, never recomputed at read — and FORWARDS only the
  object's own RAW O(1) data. Nothing is serialized; the reseed rebuilds it.
  **BUILT:** `PlotContext` holds the `CASC_PRED_*` verdict BITSET (own-plot + adjacency blocks, an 8-neighbour
  fan-out on the adjacency half); `CityContext` holds `plotAttrs` (the literal FOLD of member plots' bits), the
  tiered VICINITY-bonus sets, the traded-bonus count, the area id + tile count, the largest-adjacent-water size
  and the holy-city count; `EmpireContext` holds `policies`. All maintained solely by the contexts' own spine
  consumer — no direct hooks anywhere — off the complete plot substrate surface (terrain / feature / improvement
  / route / bonus / owner / type / river / irrigation / landmark / worked) plus `SEVT_AREAS_RECALCULATED`.
  The payoff is the point: `hasVicinityBonus` was a full radius scan + a 5,202-building scan per check;
  `isCoastalLand` an 8-neighbour scan per predicate; `getNumBonuses` the turn wall's hottest cluster — all now
  bare fetches. **OPEN:** the id-keyed radius dictionaries that would collapse `ev_cityPlotHas`' remaining
  per-check scan (terrain/feature/improvement/route prereqs), and the context gaps in the
  [info-rebuild.md](info-rebuild.md) ledger.
- **How the INFO side hands its data to the cascade — "make the infos sane" (active).** Today an info IS the legacy
  variable set (220 members on `CvBuildingInfo`, 247 on `CvUnitInfo`), with JSON force-fed into it and a
  ~300-getter surface mirroring the legacy `CvXInfo` contract. The target — **an info STYLED FOR THE JSON**:
  members mirror the JSON anatomy, getters are ×100-native (no `100` in any name) and coherent (data-out by
  channel), generalizing the in-tree `CLS_HAS`/bitset classification pattern — is
  [patterns.md § The INFO DATA-OUT contract](../../architecture/patterns.md). The build sequence: (1) target shape
  written; (2) rebuild `CvBuildingInfo` to it as the proven pattern (fattest, and it already carries both the
  legacy-scalar defect and the sane `CLS_HAS` cure side by side); (3) roll across the other infos + rewire
  consumers onto the coherent surface ([DEC-new-getter-surface](../../architecture/decisions.md#dec-new-getter-surface)).
  The ordered worklist (the one-reader consolidation, the interning pass, the scope-free vocabulary, the
  `Json`-prefix rename sweep, acceptance): [info-rebuild.md](info-rebuild.md).
  An info holds **only its own side**: cross-entity own-output (a building's improvement/terrain yields) is NOT a
  building member — the improvement owns its yield, conditioned on the building's presence
  ([DEC-deliveryguy]). The info is shaped to that NOW; it is not distorted to hold data it shouldn't just because
  the delivery mechanism isn't built (a red tree loses no live data).
- **The GENERAL modifier own-output reverse-map — LANDED in code (`Sources/Data/CvReversePass.cpp`, the ONE
  general pass `loadJson` calls); runtime verification still pending on the red tree — verified live, never on a
  promise.** At load, any source's target-keyed own-output deposit is reverse-landed on the TARGET as a compiled
  conditioned own-output entry ("+X while the source is present" — the source's presence is the entry's prebuilt
  `enabled` condition), so a modder authors either side and both carry it. Two landing classes: a yield-channel
  flat keyed `<yield>.<scope>.{improvements|terrains|features|routes}.{TARGET}` on a building/civic/tech lands
  plot-scope (where every component-specific buff resolves) — building→improvement yields land; and a
  buildings-keyed output-channel deposit (gold/culture/research/espionage/commerce/food/production/happiness/
  health on a building/civic/tech — the wonder/civic/tech → building-type boosts, info-rebuild.md ruling 19)
  lands on the target BUILDING at CITY scope (building output per modifier.md §2a/§2b), same family/value/unit,
  presence-gated at the AUTHORED deposit's scope axis, any authored condition composed in.
  Governing-deliverer keyed maps stay source-side ([DEC-deliveryguy], modifier.md §4): `buildRate` keyed targets,
  every TRAIT keyed deposit (the per-set carve-out), route→improvement yields (the §4 exemplar; the legacy
  improvement-side readers are fed by the pass's compat rows), and the civic feature-happiness keyed member (the
  §2b one-term bundling). The same pass generalizes `EDGEF_RELATED` over
  every compiled surface (edges/requires/deposits/grants/provides/triggers — the retired tech-only bespoke
  inversions and their legacy-mirror getter reads are deleted), carries the `EDGEF_REQUIRED_BY` gate walk, and
  owns the forward compat reconstructions ([DEC-one-reverse-view]).
  **The cut is FULL (owner): the new coherent surface is built and every consumer rewired onto it in the same
  pass, the legacy getter names disconnected — never a thin-compat layer left breathing.** The red tree makes the
  blast radius free to absorb; a change that left consumers untouched would be the half-migration tell.
- **The new Python surface — ONE COMPLETE DATA-FETCHING LIBRARY (owner), built as its own STEP** before the
  legacy `Cy*` surface is CUT AWAY ([DEC-cy-not-fixed](../../architecture/decisions.md#dec-cy-not-fixed)): ONE
  surface replacing the scattered per-type `Cy*` interfaces, COMPLETE against the census (screens + pedia +
  the Python-authoritative systems) so no read is left needing a reach-around into legacy — a gap re-creates
  the two live surfaces the ruling forbids. Data fetching only; Python gameplay stays Python and consumes it.
  Not a widened binding, not a shim beside it. Detail + acceptance: [info-rebuild.md](info-rebuild.md).
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

1. **Manifestation polls** — PROGRAMMATIC against the `/computed` oracle endpoints, never eyeballing the screen. A
   blind value is EMITTED first; emitting it is step one of that item's fix.
2. **Turn time** — the whole performance signal, on the wall clock; the target and its sequencing are in
   §Verification targets below. The process-memory gauge on `/computed/perf` rides beside it.
3. **Parity and shadow are CLOSED** and are NOT re-run, re-invoked, or used to frame remaining work
   ([DEC-verify-in-game-not-reshadow](../../architecture/decisions.md#dec-verify-in-game-not-reshadow)).

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
3. **⛔ ART IS OUT OF SCOPE — leave it alone (owner).** The art defines (`CIV4ArtDefines_*`), their `ART_`/
   `EFFECT_` tag ids, and the asset files are UNTOUCHED by this rework: JSON carries only the tag id, the
   definitions stay in the ART XML, and `ARTFILEMGR` keeps resolving them ([json.md §7](../../specs/json.md),
   [naming.md](../../specs/naming.md)). This includes **not** cleaning up art that becomes orphaned when a
   consumer is removed — an unreferenced define is inert, and pruning it is neither this rework's job nor a
   tidiness licence. Same standing as TXT: an unmigrated system boundary, not a gap
   ([info-rebuild.md](info-rebuild.md) § the Python library).
4. **NOT failures — deliberate, owner-ruled permanent carve-outs:** the golden-age YIELD-EFFECT member-mirror
   ([golden-age.md](../../reference/golden-age.md)); the mission-CONCEPT unification and the Python-authoritative
   outcome hooks; random EVENTS; Revolution. *(The `CvOutcome` DATA itself IS migrated to JSON.)*
