# #430 roadmap — bring `json-data-migration` back into spec conformance

> **The master plan for the active work.** Mandated session-start reading (root `AGENTS.md`). It states the
> current shape of the drift, the foundational design the code must conform to, the failure inventory, and the
> sequencing. Governing rulings are ledgered as `DEC-*`; this doc links them, it does not re-articulate them.

## Context — why this work exists

The `json-data-migration` branch (#430 cascade) is the accepted design's implementation, but it **drifted via
shortcuts**. The curated JSON data is solid (all gameplay entities populated, no placeholders — `Assets/Data/**`,
`Tools/Migration/curate_*.py`). The C++ consumer surface + cascade runtime is half-implemented: caches computed but
not spine-invalidated, the cascade recomputing everything every turn, the event spine used as a logging tee instead
of the dispatch front door, the property feed mangled, the grants apply-loop unbuilt, data that loads but never
manifests in-game, and tasks marked done that are stubs.

**This is NOT a redesign.** The design is specced and accepted. Every gap below is a *shortcut against accepted
spec*: the work is to make the code match the spec it skipped. Each item cites its spec authority and its code
divergence.

## Governing principles (ledgered as DECs)

1. **No "deferred."** Anything marked deferred / parked / not-yet-landed / blocked / post-cutover / "later" / TODO /
   pending is a **failure to fix**, not a backlog item ([DEC-no-deferred](../../architecture/decisions.md#dec-no-deferred)).
2. **No self-heal.** No blanket per-turn/per-slice rebuild papers over a missed invalidation; the blankets are
   REMOVED, not optimized. Correctness comes only from complete, targeted, spine-routed invalidation; a miss surfaces
   as a live divergence ([DEC-no-self-heal](../../architecture/decisions.md#dec-no-self-heal)).
3. **The cascade is built and kept ENTIRELY from events.** Full build ONLY on load (the reseed); post-load only dirty
   packages rebuild; never a full recompute mid-game. Steady-state per-turn cost tracks *what changed* (thousands),
   never *what exists* (millions) ([DEC-spine-reseed](../../architecture/decisions.md#dec-spine-reseed),
   [DEC-calc-count-gate](../../architecture/decisions.md#dec-calc-count-gate)).
4. **The keystone — self-invalidating per-package caches.** Each yield/modifier package is its own cache; a DOMAIN
   event marks exactly the packages its source (per the deposit index) feeds. "This is the basis of EVERYTHING."
5. **Universal yield.** ANY number game mechanics modify — base yields, commerce, free XP, free specialists,
   properties, any other — is a channel in ONE machine, ONE uniform package format. A number computed by a legacy
   ad-hoc path outside the machine is a shortcut/failure ([DEC-universal-yield](../../architecture/decisions.md#dec-universal-yield)).
6. **Done = observable in the running game via an endpoint poll** — never "the code path exists" or "the data loads."
   "Straight up missing" = does not show in the game even if it loads
   ([DEC-done-is-observable](../../architecture/decisions.md#dec-done-is-observable)).
7. **The 50k `(scope,channel)` calc gate.** Every calculation logs its `(scope, channel)`; >50k/turn for anything is
   near-certainly a failure ([DEC-calc-count-gate](../../architecture/decisions.md#dec-calc-count-gate)).
8. **The Cy\* wrapper contract is NOT fixed.** Freezing the `boost::python` `.def` surface forced the JSON pocos to
   mirror the entire legacy `CvXInfo` getter contract (thousands of getters, curator-gap stubs). Redesign the
   boundary around the cascade/JSON model; rewire the Python info-consumers; fix the stub-fed wrong values. Python
   gameplay stays Python ([DEC-cy-not-fixed](../../architecture/decisions.md#dec-cy-not-fixed)).

## The accepted foundational design (what the code must conform to)

Authority: [state-repositories.md](../../architecture/state-repositories.md), [modifier.md](../../specs/modifier.md),
[enabler.md](../../specs/enabler.md), [event-spine.md](../../specs/event-spine.md),
[scope-packages.md](scope-packages.md), [f0-eventspine-invalidation.md](f0-eventspine-invalidation.md).

- **The event spine is the foundation, built proper and FIRST.** One `emit(KIND,…)` fanned by KIND to
  `IEventConsumer`s; DOMAIN events are consumed by logging + grants + **cache build/invalidation** + the OOS replay.
  Nothing else in the engine detects changes — the hand-wired per-site invalidation is retired for this one surface.
- **The EMIT surface comes first; the cache build is the step AFTER.** The caches cannot build from events until the
  events are actually, completely emitted — during play AND from the load process (the reseed emitting every
  present-fact). Get the emit surface complete and observable first, then build the consumer that populates from it.
- **The cascade is rebuilt from events on load (the reseed) — never from the game objects' derived state.** A loaded
  save deserializes the game objects directly; the incremental setters never fire, so the reseed fires the DOMAIN
  events from inside the save read as each fact deserializes, and the cascade builds from its own deposits. New game
  and load are the SAME path (facts are order/prerequisite-independent). This is the ONE full build
  ([DEC-spine-reseed](../../architecture/decisions.md#dec-spine-reseed)).
- **Eagerly build ALL caches at load — the policy stands ([DEC-turn-time-is-king](../../architecture/decisions.md#dec-turn-time-is-king)).**
  Game-object caches (the plot-yield cache) warm from their own state; the cascade warms via the eventspine reseed.
  What was removed is only the cascade's population MECHANISM — the recompute-from-state recalc
  (`playerSliceRebuild` + `worldRebuild` + `recalculateModifiers`) — a stabilize-the-drift stopgap; the reseed
  replaces it.
- **Post-load, ONLY dirty packages rebuild.** No full per-player rebuild on `doTurn`, no mark-all, no per-slice
  blanket, no turn-roll self-heal — all REMOVED. Reads are bare fetches; the event→cache routing is DERIVED from the
  deposit index (source → the channels×scopes×targets it touches → the dirty bits).
- **Per-scope package cache model** ([state-repositories.md](../../architecture/state-repositories.md),
  [scope-packages.md](scope-packages.md)): a `CvDerivedCache` on every scoped item at every level
  (world→team→player→area→city→plot); each level's packages in ONE uniform format (Σflat, Σpercent each their own
  package per channel; unit is part of the slot key); the only live calc is adding the ~5 packages at read.
- **A cascade is a cache, two kinds:** yield/percent packages = value cache (memoize, event-invalidate,
  recompute-from-inputs). The ENABLER's sets (frontier + operating-building set) = maintained by TARGETED
  PROPAGATION through the reverse index, in place — NEVER blanket-recomputed.
- **THE OUTPUT-SEAM PATTERN.** Where the engine does placement/application, the cascade owns the two ends and the
  engine the middle: (1) authored INPUTS → cascade (source-centric deposits); (2) placement/application → engine
  (free-specialist assignment; the golden-age plot-threshold "+1"); (3) OUTPUT yields → cascade package, consumed
  like plot yields. Free specialists (amount+forced-type deposits, engine places, output = package) and golden age
  (LENGTH+grant = JSON inputs; the plot-threshold EFFECT = engine middle carve-out; the extra plot yield = output
  package) are the exemplars.

## The accepted CUT STRATEGY

[cutover.md](cutover.md) is the master cutover doc; every cut executes from [code-cut-map.md](code-cut-map.md). The
cut is NOT one event — each mechanism cuts when ITS verification is clean, verified LIVE in-game.

- **Parity + shadow are CLOSED** ([DEC-verify-in-game-not-reshadow](../../architecture/decisions.md#dec-verify-in-game-not-reshadow)).
  StoneBase verified the event-spine STRUCTURE; shadow verified the CALCULATIONS. Do NOT re-run either — acceptance is
  LIVE-game manifestation + the calc-count gate, never a re-shadow.
- **Gate 3 — classification consumption** (the long pole): rewire every engine/AI/UI consumer of
  `skills/tags/capabilities/attributes/policies` from legacy XML fields to the cascade classification.

**Two getter families, two strategies:**
- **Computed-value getters** (`getYieldRate100`, `hasTrait`, `isPower`, classification getters): flip the BODY to
  return the cascade value → delete the legacy accumulator behind it → engine/AI consumers are NEVER rewired (rewire
  the body, not the call sites).
- **Info-field Cy\* bindings** (`getBuildingInfo(i).getX()`, the ~900 `.def`s):
  [DEC-cy-not-fixed](../../architecture/decisions.md#dec-cy-not-fixed) — redesign the contract around the
  cascade/JSON model, rewire the Python info-consumers, fix the stubs.

**Reproduce-not-default:** a poco getter whose value is NOT curated JSON (runtime-drawn / engine-resolved) MUST
reproduce legacy's mechanism, never a stand-in default (0/-1/empty). The silent-wrong-value stubs feeding Python
(F6/F8) already violate this; fixing them is conformance, not new scope. (Curated-field drops are still faithful
defaults — the complement.)

## Failure inventory (the shortcuts — each cites spec authority + code divergence)

### F0 — FOUNDATION: the event-spine-driven single cache build/invalidation. Everything else is downstream.
The build plan is [f0-eventspine-invalidation.md](f0-eventspine-invalidation.md). The spine emits DOMAIN events but
its only registered consumers are logging + grants — **no cache build/invalidation consumer exists**; invalidation is
a parallel hand-wired path (`buildingProcessed`/`dirtyCity`/`markPlayerScopeAndCities` wired into the mutation choke
points), and the load build is the recompute-from-state recalc. F0: complete the emit surface (play + the load reseed
emit) FIRST, then the one consumer that builds caches on load (the reseed) and invalidates them during play (derived
from the deposit index); delete the blankets + the recompute-on-load recalc.

### F1 — Reach GREEN (the RED build is a by-design ratchet — [DEC-red-ratchet](../../architecture/decisions.md#dec-red-ratchet); NEVER restore an archived `CvXInfo`).
- The 23 archived-replacing pocos are defined + populated via `LoadGlobalClassInfoJson`, mirroring the legacy getter
  contract. Per [DEC-cy-not-fixed](../../architecture/decisions.md#dec-cy-not-fixed) this info-field surface changes
  shape with the Cy\* redesign (F8) — not immutably done.
- **DllExport EXE-bound accessor proxy layer** — narrow but it blocks compile.
- **`enPromotionValid`** + the property cascade reads still on the legacy path.
- **11 uniformity `CvJson<X>Info` pocos** (civilizations/eras/handicaps/gamespeeds/specialbuildings/leaderheads/
  specialunits/victories/votes/hurries/bonusclasses) unpopulated — missing `RJ_REPO_TYPES` dispatch rows. Grants
  reads them.
- Gate consumers for culturelevels/unitcombats/promotionlines.

### F2 — Gate 3 classification consumption (the long pole). [cutover.md](cutover.md) §Gate 3.
Hundreds of engine/AI/UI call-sites still read scattered legacy XML fields instead of
`getSkills()/getTags()/getCapabilities()/getAttributes()/getPolicies()`. `skills`/`tags`/`state` not yet mapped; the
`IS_<TAG>` predicate surface is unbuilt and every rewire blocks on it.

### F2b — The CONSUMER ITERATION sweep the getter flip skipped (whole-database loops → the enabler frontier).
Spec authority: [enabler.md §6](../../specs/enabler.md) — the AI's decisions iterate ONLY the frontier, never the
entity database. Divergence: the availability-getter flip rewired what `can*` READS (bare enabler lookup) but the
CALLERS still iterate whole entity space probing per id — the census counts **262 whole-database iteration loops**
over building/unit/tech/civic/project space in `AI/` + `Engine/` (CvPlayerAI 53, CvPlayer 42, CvCity 26, CvGame 21,
CvTeam 19, CvCityAI 13, …). Measured cost of ONE instance (`AI_bestBuildingsThreshold`, called ~7×/city/turn by
`AI_chooseProduction`'s sequential focus ladder): a 5,202-building `canConstruct` probe scan per attempt ≈ 4.8M
redundant probes/turn, inside the turn-wall's dominant phase (chooseProduction = 96% of city doTurn; measured live
2026-07-16, ~917M `CvPlot::getYield` calls in one turn). The sweep: classify each loop (hot per-turn vs load/init/
UI-rare — full scans are legitimate off the hot path), rewire every hot one to iterate the enabler's LISTED set,
and collapse `AI_chooseProduction`'s focus ladder to ONE scoring pass read seven ways (the scorer's own designed
shape). Exhaustive, adversarially verified ([DEC-all-means-all](../../architecture/decisions.md#dec-all-means-all)).

### F3 — Grants apply-loop UNBUILT. [grants-machine.md](grants-machine.md), [event-spine.md](../../specs/event-spine.md).
The grants machine resolves + shadows only; does NOT apply. ~30 PREREQ rows (religion founder units, game-start
grants, free techs/gold/units/civics/population, trait freePromotions, building `bFirst` grants, settler
foundBuildings, per-turn spawn/heal). **Prime suspect for "free promotions load but don't show"** — attribute via
endpoint, do not assume.

### F4 — Unit-plane modifier machine NOT BUILT. [code-cut-map.md](code-cut-map.md) §BLOCKED unit-plane.
strength/combat-percent/withdrawal/heal/bombard/movement/espionage/keyed-terrain/invisibility/SizeMatters/
promotion/unitcombat apply-loops + serialization. City channels maintenance/defense/health/happiness/GP-trade-air/
buildRate flagged NOT BUILT. Empire civic/trait/tech apply-loops BLOCKED. Under universal-yield each is a channel
through the uniform machine.

### F5 — Property feed mangled. [property-audit.md](property-audit.md) (locked, owner-approved).
Engine math is intact (KEEP-legacy); only the JSON→engine feed is broken, both directions: **over-applies** (one-shot
`<Properties>` replayed every turn — crime-spike/education-crash runaway) and **under-applies** (`changePropagation`
getter hard-returns 0; gated/conditioned entries `continue`-skipped because the increment-4 BoolExpr/IntExpr
translator does not exist). Scoped clean redo against the locked spec.

### F6 — Data that loads but does not manifest. Free XP / promotions case.
Free XP + free promotions load end-to-end; the break is in APPLY/DISPLAY, not load. Real drops:
`isApplyFreePromotionOnMove()` hardcoded `return false` (on-move re-apply branch dead); consumed stub getters return
0/false and silently eat data; a broken cross-curator promise (`BonusCommerceModifiers` dropped by both curators).
Reconcile the stale done-claims to code in the same fix.

### F7 — Data tail (curator/JSON). [data-migration-remaining.md](data-migration-remaining.md).
IN SCOPE (failures): NPC civs / `stronglyRestricted`, unitcombat→`tags` pass, `state`/paralyze, corporation rework,
leaderhead trait remap, ranked-target-selection. NOT failures — deliberate correct scoping: unit `missions`/
`CvOutcome` + random EVENTS + Revolution are Python-authoritative gameplay, deliberately NOT JSON-migrated; they stay
Python. ✅ VERIFIED PRESENT (not gaps): golden-age LENGTH + anarchy-reduction timers + golden-age GRANTS all curated.

### F8 — Python layer rework. RESOLVED: boundary + fix-values only ([DEC-cy-not-fixed](../../architecture/decisions.md#dec-cy-not-fixed)).
Breakage is silent wrong-VALUE, not compile fail (pocos mirror legacy signatures; curator-gap stubs feed defaults —
e.g. `getTotalModifiedCombatStrength100` stubbed 0 → PediaUnit shows zero combat strength). **In scope:** redesign the
Cy\* info-binding contract around the cascade/JSON model; rewire the Python info-CONSUMERS (Pedia/Advisors/display);
fix the stub-fed wrong values. **Out of scope (stays Python):** Revolution, random events, outcomes, missions.

### F-DOCS — doc reconciliation (part of every item, per repo rule).
A doc gap that bit you bites the next contributor; close it in the same change. Stale done-claims → reconcile to code.

## Observability / test layer — the acceptance gates

Two acceptance pillars, per item, verified LIVE in-game ([validation.md](../../specs/validation.md),
[http-endpoints.md](../../specs/http-endpoints.md)):

1. **The 50k `(scope,channel)` calc-counter** — reset at `doTurn` top, exposed at `/computed/perf` + teed to
   `/events`. Total/turn < 50k (thousands steady-state, ~0 on a quiet turn); the histogram names the culprit
   scope/channel on a breach. Standing regression tripwire.
2. **Manifestation polls** — PROGRAMMATIC against the `/computed` oracle endpoints (yields / wellbeing / tally /
   unitSkills / heal / unit promotions). Poll-and-assert, never eyeballing the screen. A BLIND value must be EMITTED
   first — step one of that item's fix.
3. **StoneBase LIVE dashboard** — repurposed from offline parity oracle to the USER-VISIBLE PERFORMANCE LAYER
   ([DEC-verify-in-game-not-reshadow](../../architecture/decisions.md#dec-verify-in-game-not-reshadow)): the live
   `(scope,channel)`/turn-time dashboard (Razor + SignalR) + the `/computed` oracle values in one window — performance
   and correctness in a single "never look at the screen" surface. StoneBase = `C:\code\s2s\StoneBase` (its own git
   repo — commit there, NOT the mod repo).

## Sequencing

0. **Owner rulings landed in repo docs** (AGENTS.md / decisions.md / state-repositories.md) — done.
1. **Observability FIRST** — the `(scope,channel)` counter + `/computed/perf` (the 50k gate), so every later step is
   provable and the current millions-of-calcs state is measured before touching it.
2. **The EMIT surface FIRST** — the event spine emits ALL events completely, during play AND from the load process
   (the reseed emitting every present-fact). Verify observable (every present-fact emitted on load) BEFORE anything
   consumes it. This is F0's prerequisite half.
3. **The cache build/invalidation consumer** — the one consumer that builds all caches on load (the reseed) and
   invalidates only dirty packages during play (derived from the deposit index); delete the blankets +
   `refreshOperatingBuildings` reseed + the recompute-on-load recalc. Prove LIVE via the 50k gate (millions →
   thousands) + manifestation polls green. This completes F0.
4. **F1 reach green** — DllExport proxy, `enPromotionValid`, 11 uniformity dispatch rows, gate consumers (the
   getter-flip cut strategy for computed getters; NEVER restore an archived `CvXInfo`).
5. **F2 classification consumption** (the long pole) + the `IS_<TAG>` predicate surface; cut each machine's legacy
   once its consumers read the cascade AND the live game manifests correctly.
6. **F3 grants apply-loop**, **F4 unit-plane + remaining city channels**, **F5 property clean redo** (against the
   locked [property-audit.md](property-audit.md)). Each channel = a yield in the uniform machine; free specialists via
   the output-seam.
7. **F6 manifestation fixes** (reproduce "doesn't show" as a failing poll → attribute → fix → re-poll green), **F7
   data tail**, **F8 Cy\* boundary redesign + Python info-consumer rewire + stub fixes**.
8. **F-DOCS** folded into each item; each mechanism cuts when ITS gate is clean → push to `main` = endgame of #430.

## Scope decisions

1. **Backlog scope = #430 CRITICAL PATH ONLY.** The `docs/plans/parked/` forward-FEATURE backlog (sea-AI-rework,
   specialist-rebalance, global-warming-mod, …) is OUT — un-started future features that never claimed to be #430.
2. **Python = BOUNDARY + FIX VALUES ONLY.** Redesign the Cy\* binding contract + fix the stubbed getters; do NOT pull
   Python-authoritative gameplay into the DLL.
3. **NOT failures — deliberate correct scoping (grandfathered out of the no-deferred rule):**
   - the golden-age YIELD-EFFECT member-mirror (PERMANENT engine-core carve-out —
     [golden-age.md](../../reference/golden-age.md), [DEC-conditions-are-predicates](../../architecture/decisions.md#dec-conditions-are-predicates));
   - `validation.md` POLICY-deferrals (out-of-scope validation shown-not-dropped; balance redesign post-migration);
   - unit `missions`/`CvOutcome` + random EVENTS + Revolution — Python-authoritative gameplay, deliberately NOT
     JSON-migrated. Stay Python; out of #430.

## Verification (end-to-end)

- **The turn-time TARGET (owner): ≤ 2 minutes wall per turn on the standing late-game test save** (the
  ~1338-era save; measured baseline `[PERF/phase] turn.wall` ~7min, `updateMoves` 124–197s of it). The
  [DEC-turn-time-is-king](../../architecture/decisions.md#dec-turn-time-is-king) objective made concrete;
  the FPS hunt resumes ONLY after F0's caches are event-wired and the game runs behaviorally as it used to
  (owner sequencing ruling).

- Build: `powershell.exe -NoProfile -ExecutionPolicy Bypass -File "../Tools/_Build.ps1" <Config> <verb>` from
  `Sources/` (Release for interactive testing; FinalRelease for perf/turn-lag). Assert build = quick compile check.
- Runtime (per-session owner permission only): `agentstart.bat` → poll `http://127.0.0.1:7227/` until up → verify via
  endpoints. Check `XmlLoad.log` counts, no `Xml_MissingTypes.log`, no new `Asserts.log`.
- The 50k gate + manifestation polls are the standing acceptance tests, verified LIVE. Parity + shadow are CLOSED and
  NOT re-run. Invalidation completeness is proven live: the `(scope,channel)` count stays event-proportional (not
  entity-proportional) and manifestation polls stay green with the self-heal gone.
