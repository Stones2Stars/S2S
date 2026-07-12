# F0 — the event-spine-driven single cache-invalidation foundation

> The build plan for making the event spine drive ALL cache invalidation, to spec
> ([event-spine.md](../../specs/event-spine.md), [state-repositories.md](../../architecture/state-repositories.md)).
> Grounded against the live code (branch `json-data-migration`). ⛔ The existing code is SUSPECT, not a baseline —
> where it diverges from this design, rebuild to the design.

## The target (single invalidation philosophy)

**state change → DOMAIN `emit` → the cache-invalidation `IEventConsumer` → routed by the deposit index → exactly the
affected packages mark themselves dirty.** One event surface, one consumer, one routing index; every other
invalidation path deleted. A cache invalidates **iff** a DOMAIN event whose source (per the deposit index) feeds it
fires. Consequences: the per-turn cost is event-proportional by construction; completeness is non-negotiable (a
missing emit = a cache that never invalidates, [DEC-no-self-heal](../../architecture/decisions.md#dec-no-self-heal)).

**The SAME emit surface builds the cascade on load, too — one surface, two consumptions of state.** During play,
incremental DOMAIN events drive the invalidation consumer above (only touched packages rebuild). On LOAD the
**reseed** ([DEC-spine-reseed](../../architecture/decisions.md#dec-spine-reseed), R6) — the save read itself fires the
DOMAIN events for every fact as it deserializes — so the same consumer builds the WHOLE cascade once, the ONE full
build. Load's events come from the read, play's from live state-changes; there is no separate load path. This **RETIRES the recompute-on-load**
(`playerSliceRebuild` + `worldRebuild` + `recalculateModifiers` — a drift-stabilizing stopgap) and there is **NO
per-turn blanket**: `doTurn` never does a full per-player rebuild; only dirty packages ensure. Building the proper,
COMPLETE spine FIRST is what makes both safe — no unhooked mutation, so nothing stales and no self-heal is needed.

## Current state — the top-line divergence

The target is **not implemented**. The spine and the invalidation machinery are two disjoint systems:

- `CvEventSpine` emits DOMAIN events but its ONLY registered consumers are logging + grants
  (`CvEventSpine.cpp:297-313`). **No cache-invalidation consumer exists.**
- Invalidation is a parallel hand-wired path: direct static calls `CascadeAccumulator::buildingProcessed /
  dirtyCity / markPlayerScopeAndCities / cityHaveChanged` wired straight into the `Cv*` mutation choke points —
  never through `emit`/`onEvent`.

F0 collapses these into one.

## Reusable as-is (spec-conformant — keep)

- The spine dispatch primitive: `emit` / `IEventConsumer` (`CvEventSpine.h:207-213`) / the KIND enum
  (`EVENTKIND_DOMAIN/DIAGNOSTIC/TRACE`, `h:27-33`) / the interest-mask. ✅ Matches event-spine.md.
- The plot-yield PULL model (plot substrate lazy-dirties `m_yieldCache`; city yield combine PULLs `getPlotYield`
  live — `CvCascadeAccumulator.cpp:357`). No city mark needed; conformant to state-repositories.
- The package substrate `CvCascadeScopePackages.h` — **flat and percent packages already carry SEPARATE bits**
  (the percent-vs-flat split), ready for granular routing. The dirty-bit vocabulary IS the (scope × channel) axis:
  - city `CascadeCityPkg` (`:91-116`): `CPK_YPCT/YSPEC/YEXTRA/CSPEC/CPCT/CBASE/WB/SCFLAT/SCPCT/SCSPEC/BR/FRONT_B/U/PP`
  - player `CascadePlayerPkg` (`:195-209`): `PSC_YFLAT/CFLAT/WB/SC/BR/FRONT_P/PROMO`
  - world `CascadeWorldPkg` (`WSC_ALL`)
- The operating-buildings targeted propagation (`EnablerKernel::recomputeOperatingBuildingsInto`; ripples via
  `onBuildingChangedActive`/`onHaveChangedActive`/`onPlayerScopeChangedActive`).
- ✅ **Epochs/stamps are already deleted** (increment I). Grep for `bumpEpoch`/`CvCascadePlayerStamp`/`s_iEpoch` =
  zero live defs. The only blankets left are the `markAllDirty` calls in `playerSliceRebuild` + `markPlayerScopeAndCities`.
- The perf census plumbing/flush (`cvCascadeModifierPerfCensus`, `CvCascadeModifierMath.cpp:160-210`) + the
  `CascadePerf` struct (`CvCascadePerfCount.h`) — the right home for the new reporting counters.

## Must rebuild (the real F0)

### R1. `CvCascadeEvent` DOMAIN payload → carry the SOURCE LOCATION
Today the DOMAIN payload is empire-count-shaped: `CASCADE_EVT_BUILDING_COUNT` carries `iType=building, iA=empire
count, iB=delta, iC=player` (`CvEventSpine.h:177-186`). It **cannot route** to a specific `CvCity`'s packages — no
city/plot/area id. Extend the DOMAIN payload to a source-carrying shape: `(sourceKind, sourceId, ownerPlayer,
cityId|plotId|areaId, delta)`. Keep the logging-mode payload as-is (`aFields[]`).

### R2. Reverse routing surface on `DepositIndex`
Today `DepositIndex` is a FORWARD map: `source CvInfo* → its CascadeDeposit records` (compiled interned segments
family/scope/member/target + FK — `CvCascadeDepositIndex.h:42-73`, built at readJson push `cpp:163-174`). There is
**no reverse `source-type → (scope, packageBit, channel, targetFk)` routing** and no query API a consumer could call
to go "building X changed → these package bits." Today the routing is re-derived INLINE, by hand, **for buildings
only**, in `buildingProcessed` (`CvCascadeAccumulator.cpp:870-916`); tech/civic/trait/bonus/project use the blanket.
Build the inversion once at push time: a compiled reverse index `sourceKind → list of (packageScope, packageBit,
channel, targetFk)`, queried O(1) by the invalidation consumer to produce the dirty masks. The raw material (compiled
segments) exists; the inversion does not.

### R3. The cache-invalidation `IEventConsumer`
`wantedKinds() = 1<<EVENTKIND_DOMAIN`; `onEvent(e)` queries the R2 reverse index for `e.sourceKind/id` → the affected
(scope, packageBits) → marks them dirty on the right owner (via `dirtyCity` / player-scope / world), including the
operating-buildings ripple where relevant. Registered in `cascadeRegisterConsumers()`. This REPLACES the hand-wired
choke-point calls and both blankets.

### R4. Complete the DOMAIN emit surface (the completeness gap)
The spec wants a DOMAIN event on EVERY state change. Current emits (10): building-count, unit-count, civic, tech,
religion-founded, player-init (`CvPlayer.cpp:1872/8891/13695/13800/14397`, `CvTeam.cpp:5187`) + 4 name-change
(`CvPlayer.cpp:3349/3418`, `CvCity.cpp:13611`, `CvUnit.cpp:17141`). **These are coarse (empire-count/player level).**
The fine per-city/per-plot mutations that actually drive package staleness are wired to the accumulator directly, not
the spine — they must become source-carrying DOMAIN emits. **The GAP LIST — source types with NO invalidation at
their choke point today (the emits/marks to ADD):**

1. **BONUS network** — `CvCity::processBonus` (`CvCity.cpp:4523`), `CvCity::doVicinityBonus` (`:21788`). Bonus-gated
   health/happiness + bonus-gated building yields go stale.
2. **PROJECT / PROCESS completion** — `CvTeam::processProjectChange` (`CvTeam.cpp:4287`; only `worldScope.markAllDirty`
   at `:4358` today). Project-granted modifiers (Internet class, GA-from-project, project percents).
3. **STATE-RELIGION switch** — `CvPlayer::setLastStateReligion` (`CvPlayer.cpp:12579`). SR-pool sums / `iCSrMatch`
   realization / SR-conditioned percent stacks.
4. **TRAIT runtime change** — `CvPlayer::processTrait` (`:28517`) / `setHasTrait` (`:29388`). Trait flats/percents on
   an event/leader-driven runtime change (static assignment is covered by PLAYER_INIT + load warm-up).
5. **POWER (partial)** — `CvCity::changePowerCount` (`:10306`) marks frontier + operating-buildings only; the
   rate/WB/scalar packages are NOT marked (relies on the blanket).

The others (building/pop/specialist/religion/corp/tech/civic/GA/plot-substrate) DO mark today — but several via the
blanket or a conservative-all mask, not a derived one (see R2/delete-list).

### R6. The load RESEED — event-source the save read
A loaded save deserializes the game objects directly; the incremental setters never fire, so the cascade (packages +
enabler) has nothing to build from. The reseed makes the events come from **inside the save read** — `CvGame::read` →
`CvPlayer`/`CvCity`/`CvTeam`/`CvPlot::read` fire the DOMAIN event as they deserialize each fact (north-star: the event
SETS the state, read → emit → populate — object-populated-by-events is a step too far for now, but the events must
still originate from the genuine read). The R3 consumer (load-active) builds the cascade from them. This is the ONE
full build; it REPLACES `worldRebuild` + `playerSliceRebuild` + the `recalculateModifiers` content
([DEC-spine-reseed](../../architecture/decisions.md#dec-spine-reseed)). (The plot-yield cache is a game-object cache
and keeps its own dirty-on-load recompute — not the cascade recalc being removed.)
⛔ NOT a separate post-deserialization pass that fabricates events by walking already-populated objects — banned
([superseded-ideas](../../architecture/superseded-ideas.md) #13).
**⚠ Sequencing:** the object `read()`s run BEFORE `onFinalInitialized`, where consumers register today — so the
load-active consumers (the R3 cache-build) must be set up at `GAME_LOAD_STARTED` (before the reads) for the in-read
events to land. **Load lifecycle:** emit `GAME_LOAD_STARTED` before the read + `GAME_LOAD_FINISHED` after;
result-producers (grants) rely purely on the spine and suppress between them (a load is not a genuine acquisition).

**The in-read emit shape, by fact storage.** A scalar field emits right after its own read (the terrain prototype,
`CvPlot::read`). A field read **element-by-element in a loop** emits INSIDE that loop (buildings, the `CvCity::read`
BuildingLedger loop — the cleanest hook). A field read **WHOLESALE** (`WRAPPER_READ_CLASS_ARRAY`, no per-element
hook) emits from a loop placed **immediately after** that read, still inside the object's `read()` and co-located
with its deserialization (religion/corporation/bonus/specialist in `CvCity::read`). All three are "the events come
from the genuine read"; NONE is the banned separate post-deserialization walk over already-populated objects
([superseded-ideas](../../architecture/superseded-ideas.md) #13) — the distinction is the emit is coupled to *this*
object's read, not a decoupled global pass. Present/nonzero-gated (an absent feature / zero count is no fact).

**Reseed emit map (per read site):**
- `CvPlot::read` — terrain (prototype), feature, improvement, route, **plot resource** (`SEVT_PLOT_BONUS_CHANGED`).
  ✅ wired. *(Plot resource is a genuine plot-substrate event: a Great-Farmer build places one, a discovery event
  reveals one from an improvement, removal clears one — all route through `CvPlot::setBonusType`, which now emits the
  SAME event play-time; the reseed fires it too. Distinct from the city resource-ACCESS count `m_paiNumBonuses`.)*
- `CvCity::read` — buildings (ledger loop); religion, corporation, bonus, specialist (wholesale-array loops);
  population, power (scalars). ✅ wired.
- **Tech** — reseeded from `CvPlayer::read`, NOT `CvTeam::read`. **VERIFIED: the EXE reads teams BEFORE players are
  set up** (a per-member emit from `CvTeam::read` fired 0 — no alive members yet). So the team's techs ARE loaded by
  the time a player reads: each player emits `techChanged` per-self for every tech its team holds
  (`GET_TEAM(getTeam()).isHasTech`) — the owner's "one per alive member player" ruling, realized player-side. Proper
  end-state remains a **team-scope cascade component**. ✅ wired.
- `CvPlayer::read` — traits (wholesale-array loop), civics (per-slot, AFTER the load-time civic fixup), state-religion
  (scalar), golden-age + tech (emitted AFTER `m_eID` reads + `updateTeamType()`). ✅ wired.
  **⚠ GOTCHA (cost real time):** `reset()` at the top of `CvPlayer::read` clears `m_eID` to `NO_PLAYER`, and it is
  re-read partway down; `m_eTeamType` is NOT saved and is rebuilt by `updateTeamType()` further down. So `getID()` is
  `-1` until its read and `getTeam()` is `NO_TEAM` until `updateTeamType()` — any reseed emit that needs owner/team
  must sit AFTER both, not at the top of `read()` (an emit at the top renders `owner=?(-1)`).
- Change-shaped owner events (`null → current`): plot owner + working-city (`CvPlot::read`), city owner
  (`CvCity::read`). ✅ wired.

**Representation rulings (owner-resolved):**
1. **Change-shaped events on reseed fire as `null → current`** — exactly like a real acquisition: `plotOwnerChanged` /
   `cityOwnerChanged` / `workingCityChanged` emit `old = NO_PLAYER/NO_TEAM/no-city (-1) → new = the loaded owner`.
   ✅ wired.
2. **Plot resource IS a first-class plot-substrate event** — added as `SEVT_PLOT_BONUS_CHANGED` (see the `CvPlot::read`
   map row). ✅ resolved + wired.

### R5. The `(scope,channel)` reporting counter + `/computed/perf` endpoint
Today's `CascadePerf` counters (`CvCascadePerfCount.h:32-67`) count refresh PASSES + eval leaves, split by CALLER
domain (`CascadeCondCaller`), NOT by (scope × channel). Add a `(scope,channel)` calc-count: a counter keyed on
`(scopeLevel, packageBit)` — the package-bit vocabulary above — incremented at each package REFRESH (where
`accRefresh`/`scRefresh`/`pctStack`/`operatingBuildingsRecomputed` already fire, the refresh knows its scope + which
bits it recomputes). Reset per turn (extend `CascadePerf::reset`). Flush: extend the census
(`cvCascadeModifierPerfCensus`, mind the 16-field-per-event spine cap — add lines or a structured emit). Expose at a
new `/computed/perf` route (add a `ROUTES[]` row `CvHttpServer.cpp:4194` + a `strcmp` branch `:1616`, mirror the
`tally` handler `:4069`; game-thread mailbox snapshot), returning the `scope × channel` histogram + total; tee to
`/events`. **The 50k gate** (DEC-calc-count-gate): total/turn < 50k (thousands steady-state, ~0 on a quiet turn);
histogram names the culprit scope/channel on a breach.

## Delete-list (verifiable — grep must show zero residual after F0)
- The `markAllDirty` BLANKETS: `playerSliceRebuild` (`CvCascadeAccumulator.cpp:965-985`) and
  `markPlayerScopeAndCities` (`:918-932`) — DELETED. Replaced by R3's derived per-source marks; there is no
  per-turn / per-slice blanket ([DEC-no-self-heal](../../architecture/decisions.md#dec-no-self-heal)).
- The recompute-on-load CASCADE recalc — `worldRebuild` + `playerSliceRebuild` (the cascade half of the
  `CvGame::onFinalInitialized` warm-up block) + the `recalculateModifiers` content — DELETED. The **reseed** (R6) is
  the cascade's load build. (The plot-yield cache's own dirty-on-load recompute is a game-object cache and stays.)
- The inline hand-derived routing in `buildingProcessed` (`:870-916`) — replaced by R2/R3.
- `CvCity::refreshOperatingBuildings` mask-ignoring reseed (`CvCity.cpp:11399`) — honor the mask / rely on ripples.
- KEEP (not blankets): `dirtyCity` (the mark primitive — masks become index-derived), `cityHaveChanged`/
  `unitCountChanged` (frontier targeted re-check), `cityCreated` (the one ruled eager-ensure).

## Ordered increments
1. **R5 first (observability)** — the `(scope,channel)` counter + `/computed/perf` + `/events` tee. Measure the
   current blanket baseline (prove the millions) BEFORE touching anything, and make every later step provable.
2. **The EMIT surface FIRST — the spine emits ALL events, during play AND from the load process** (R1 + R4 + R6's
   emit half): the source-carrying payload, the complete play-time emit surface (the R4 gaps), and the **load reseed
   emitting every present-fact**. This is the PREREQUISITE — the caches cannot build from events until the events are
   actually, completely emitted. VERIFY the emit is complete + correct (observable: every present-fact emitted on
   load) BEFORE anything consumes it.
3. **THEN the cache build/invalidation consumer** (R2 + R3) — the reverse routing surface + the one consumer that
   consumes those events to POPULATE the caches (the eager full build on load) and INVALIDATE them (incremental
   during play). The step AFTER the emit surface is proven complete.
4. **Delete-list** — remove the blankets + inline routing + the recompute-on-load recalc. Grep proves zero residual.
5. **Verify LIVE** — the counter collapses to event-proportional (quiet turn ≈ 0, never > 50k); the `/computed`
   oracle values stay correct (manifestation). NO re-shadow ([DEC-verify-in-game-not-reshadow]).

## Ambiguities to resolve during build (do not guess — verify)
1. `setHasBuilding` (`CvCity.cpp:14785`) vs `processBuilding` (`:4627`) — which is the true count choke point
   carrying the mark.
2. Power (`changePowerCount:10306`) — verify whether power-gated yield/commerce deposits exist that need a rate mask
   (currently only frontier/operating-buildings marked).
3. State-religion — confirm whether the SR-pool / `iCSrMatch` realization needs an explicit fill on a switch (the
   live gates cover most reads).

## Acceptance bar (all four, proven — never "kinda")
1. **Complete** — the completeness map has ZERO gaps (every state-change source emits), proven by an adversarial
   second pass.
2. **Singular** — every other invalidation path deleted, proven by grep (no residual `markAllDirty` blanket /
   hand-wired routing).
3. **Observable** — the `(scope,channel)` counter proves it live: quiet turn ≈ 0, event-proportional, never > 50k.
4. **Manifesting** — the `/computed` oracle values are correct in the running game.
