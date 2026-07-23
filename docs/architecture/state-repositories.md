# State repositories — recompute-only caches with a dirty trigger

The pattern for **derived engine state**: how a domain object's derived data (yields, commerce, health, …) is
computed and kept coherent. `CvPlot` and `CvCity` are **domain objects** — the in-game data entities — and they
**stay**. This is not about dissolving them (that's `CvCityAI`'s eventual job); it is about the derived layer.

Realized on: the plot-yield cache (`CvPlot::m_yieldCache`, the exemplar), the specialist commerce/yield getters,
the building commerce + yield caches, the per-building empire commerce-change ledger
(`CvPlayer::m_buildingCommerceChange`), the cascade city packages (`CvCity::m_cascadeCityPackages`), and the
operating-building set (`CvCity::m_operatingBuildings`).

## The problem: no unified `dataChanged` trigger

Every derived value in the legacy engine is a **hand-maintained cache with ad-hoc, gappy invalidation**. There is no
single "the source changed, refresh me" primitive, so caches drift out of sync with the data they derive from — one
disease, many instances: a dormant building's improvement-yield never decremented; a building value change leaving
the cache on the old value; transition-only stamps (`doVicinityBonus`) missing build-after-connect orders; two
surfaces reporting different worked-plot yields for the same city at the same moment. The legacy incremental
serialized accumulators additionally carry **history pollution** — values no live data source can produce (the
improvement-yield accumulators hold phantom yields, per-plot, bit-exact; the wellbeing accumulators the same class —
[modifier.md §2b](../specs/modifier.md)). Recompute-from-source is the cure; recompute-every-read getters
(the squirrelBanana class) were the workaround for a cache nobody could trust — correct, but paying full cost on
the hot path.

## The model

> **domain object mutates → flips a dirty flag → the derived value recomputes lazily on next read → consumers up the
> chain read that one value.** One trigger, one refresh path, one source of truth.

A derived cache in this model is:

1. **Lazy + dirty-flagged.** A trigger flips `dirty`; the value recomputes on the next read and clears the flag. The
   expensive recompute runs **once per change-then-read**, never per change and never per read.
2. **Recompute-only, NOT serialized** — the [DEC-derived-never-trusted](decisions.md#dec-derived-never-trusted) rule,
   applied per-field. Neither the value nor the flag is saved; on load the flag is dirty by default, so the first read
   recomputes from current state — **never stale-from-save**. Drop serialization by the **soft-remove**
   ([DEC-save-remove-is-soft](decisions.md#dec-save-remove-is-soft), [save.md §3](../specs/save.md)): FULL-DELETE the
   read + write and NAME the tag in `Assets/savemigration.txt`, which drains an old save's orphan bytes by name so
   nothing after it shifts (a no-op on a new save that never wrote it). **No `WRAPPER_SKIP_ELEMENT`** (it leaves the
   dead member named — a rollerskate target); and just deleting the read/write *without* the `savemigration.txt` entry
   desyncs the whole downstream read.
   **This is UNIVERSAL, not per-field-optional (owner ruling): NO cache is ever serialized.** `CvGame::
   recalculateModifiers()` existed to purge drifted serialized derived data; with derived data never read from a
   save there is nothing to purge — **the recalc is RETIRED as a concept** (never invoke it, never extend it, never
   cite it as a heal; the event spine + the load-time rebuild replace it). Each remaining serialized cache
   converts by the same move: skip the read, rebuild at load from source state through the live entry points
   (the bonus-network cluster — the plot-group counts AND membership, the bonus-fed wellbeing/modifier
   accumulators, power, the dormancy verdicts — is the realized exemplar: the load-end rebuild in
   `CvGame::onFinalInitialized` recolors the groups from current state, folds the counts as each plot joins,
   and reconciles dormancy to the enabler's operate fixpoint, firing the ordinary crossing emits; the city
   holds no bonus mirror at all — its read is a plot-group relay, [enabler.md §8](../specs/enabler.md)). A
   serialized store survives ONLY for genuine non-derivable state (the event/WB grant stores, e.g.
   `CvCity::m_paiFreeBonusEvents`).
3. **The single source — PULL, not push.** Things up the chain (the city, the diagnostics, the cascade oracle) **read**
   the value; the source does not **push** deltas into them. Push + a parallel cache double-count and drift; pull from
   one authoritative value cannot.

**Worked shape (the plot-yield cache):** `getYield()` = `if (dirty) recompute; return cached` — O(1) when clean;
`updateYield()` is the **trigger only** (flips dirty, fires the downstream dirties the old push carried — no eager
recompute, no push); `CvCity::getPlotYield()` reads the CITY-side worked-plot Σ cache (`CvCity::m_plotYieldSum`, a
`CvDerivedCache` marked by worked-plot flips + working-plot yield changes) — the push-maintained `m_aiBaseYieldRate`
is dead. ⛔ The pull must be a CACHE at EVERY level, never a per-read walk: the interim shape that re-summed the
radius on every `getPlotYield` call turned the game's hottest read O(radius) and was measured at 913M plot reads in
one turn inside the governor's valuation — the cost class this whole doc exists to prevent. The engine's actual base yield thereby equals the build-order-independent value the cascade computes —
stale-cache divergences resolved **at the source**, behaviour-preserving
([DEC-parity](decisions.md#dec-parity), [DEC-mirror-then-redesign](decisions.md#dec-mirror-then-redesign)).

**Incremental-accumulate ledgers convert to recompute-from-source.** The serialized player ledger
`m_ppiBuildingCommerceChange` double-counted by build order (the accumulator replayed onto the loaded value); it is
now a recompute-from-source cache (Σ over the player's buildings' `GlobalBuildingExtraCommerces` on dirty), the
changer is trigger-only, and the cities PULL it.

**Event/vote grants are NOT cached — they are a SEPARATELY PERSISTED store.** A per-building commerce change has
two sources of fundamentally different nature: the **empire** grant (`GlobalBuildingExtraCommerces`, civics) is
DERIVABLE → the recompute-from-source cache; the **event/vote** grant (fires ONCE) is **genuine one-shot state, NOT
derivable** — *"having events just be stored in the cache is lunacy"* (a recompute cache would wipe them). They live
in their own serialized field (`CvCity::m_aBuildingCommerceChangeEvents`), outside the recompute path; the reader
sums `player-recompute (empire) + city event/vote (persisted)`.

## The standardized `CvDerivedCache` component

**One reusable C++03 component** (`Sources/Infrastructure/CvDerivedCache.h`) rather than hand-rolled per cache — a
templated value-holder with the recompute injected as a member-function-pointer (poor-man's-DI-adjacent,
[patterns](patterns.md)). Three forms: the single-flag **`CvDerivedCache<TOwner,T,N>`** (leaf caches — the plot
yield), the partial-dirty **`CvDerivedCacheSet<TOwner>`** (component-granular, see Refinements), and the
runtime-sized **`CvDerivedCacheVec<TOwner,T>`** (the recompute receives the vector and fully sizes+defines it — the
player building-commerce ledger).

```cpp
template <class TOwner, class T, int N>
class CvDerivedCache {                       // recompute-only, dirty-flagged, NEVER serialized; the single PULL source
    mutable T    m_data[N];
    mutable bool m_dirty;
    TOwner*      m_owner;
    void (TOwner::*m_recompute)(T*) const;   // fills m_data from CURRENT state (needs owner; stays owner-side)
public:
    void bind(TOwner* o, void (TOwner::*fn)(T*) const) { m_owner = o; m_recompute = fn; m_dirty = true; }
    void markDirty() { m_dirty = true; }     // the trigger — call at every input-change site (no eager recompute, no push)
    T    get(int i) const { if (m_dirty) { (m_owner->*m_recompute)(m_data); m_dirty = false; } return m_data[i]; }
};
```

**Contract rules (in the header; each plugged a real hole):**

1. **Clear-dirty BEFORE recompute** — clear-after recurses on read-back and loses mid-recompute dirties.
2. **The recompute must fully define its output every call** — zero-fill on can't-compute; an early-return that
   leaves stale values behind a clean flag is the bug class this kills.
3. **NONCOPYABLE** — a copied cache keeps the ORIGINAL owner's pointer (dangling-owner footgun).
4. `data()` pointers stay valid but values mutate — never cache across state changes; game-thread only.
5. Fixed compile-time N in the array form; a runtime-sized domain uses the Vec form.

- **It is a DATA MEMBER** on `CvCity`/`CvPlot` — fine: the [patterns](patterns.md) guardrail bars adding vtable
  *bases* to EXE-bound classes, **not** data members.
- **Never serialized.** The owner's `read()`/`write()` drop the legacy field entirely and name its tag in
  `Assets/savemigration.txt` (the soft-remove, [save.md §3](../specs/save.md)); dirty-on-construct means a loaded game
  recomputes on first read.
- **Converged onto the component:** the plot-yield cache; `CascadeCityPackages` (a mutable `CvCity` member, the cascade
  math module-side behind the one `cascadeRefreshPackages` delegate — the pattern every modifier channel reuses); the
  operating-building set (event-invalidated via targeted propagation; the epoch-stamp + turn-roll self-heal are
  removed per [DEC-no-self-heal](decisions.md#dec-no-self-heal)); the player building-commerce
  ledger (Vec form). **Deliberately NOT converged:** the legacy CvCity hand-rolled dirty caches (`m_aiCommerceRate`,
  `m_aiBuildingCommerce100`, squirrelBanana) — demolition fodder at the modifier cut; polishing them is backwards
  investment. **Remaining live work:** the specialist getters get the targeted spine-routed invalidation mechanism at the
  turn-end unified rebuild (the F0 foundation), not the Vec form.

## ⚖ Refinements

- **PARTIAL DIRTYING.** A cache whose value composes from several isolated **plugin numbers** (each package a
  standing value; "the rest of the pipe stays the same") carries a **dirty BITMASK, one bit per component**, and a
  trigger marks only the components its event feeds — the `CvDerivedCacheSet` form; the realized exemplar is
  `CascadeAccumulator`'s `AccDirty` bits over the modifier packages
  ([modifier-substrate.md](../plans/structural-cleanup/modifier-substrate.md)). The single-flag form stays for leaf
  caches.
- **⚖ THE CAPSTONE RULE: the cascade is built and kept current ENTIRELY from events — no blanket rebuild, ever.**
  On LOAD the cascade is stood up by the **event reseed** — the save read fires the DOMAIN events for every fact as it
  deserializes, and each package builds from its own deposits ([event-spine.md](../specs/event-spine.md) /
  [DEC-spine-reseed](decisions.md#dec-spine-reseed)); the old recompute-on-load / warm-up recalc
  (`playerSliceRebuild` + `worldRebuild`) was a stabilize-the-drift STOPGAP and is REMOVED, and the cascade no longer
  uses `recalculateModifiers` as a heal. (The legacy `recalculateModifiers` FUNCTION still exists in
  `CvGame`/`CvTeam`/`CvPlayer`/`CvCity`, invoked only by the WorldBuilder / asset-checksum-mismatch popup path
  (`CvMessageData.cpp`) — it is never the cascade's population path.) Post-load, an event marks only the package(s) its deposits touch, and **ONLY marked (dirty) packages
  rebuild** — there is NO full per-player rebuild on `doTurn`, NO mark-all, NO per-slice blanket, and NO turn-roll
  self-heal ([DEC-no-self-heal](decisions.md#dec-no-self-heal)): those blankets (`playerSliceRebuild`, the EPOCH
  bump, the RATE turn-roll) are REMOVED, each replaced by targeted, spine-routed per-source-mask invalidation. A
  missed invalidation surfaces as a live divergence, never a silently self-healed cost — which is precisely why the
  event spine must be COMPLETE (every mutation emits) and is built proper and FIRST. Reads are BARE NUMBER FETCHES
  during the turn (an ensure-per-read protocol on AI-hot paths measurably ground unit automation). "It's the
  percentage recalcs that hurt" — the mask derivation splits percent-vs-flat so a flat-only event never rebuilds a
  percent stack. **The granularity TARGET: per-(package × CHANNEL)** — the compiled deposits carry the channel, so
  the dirty bits split per yield/commerce channel; the bit-layout split is the increment after the bare-fetch shape
  verifies.
- **⚖ THE PER-SCOPE PACKAGE MODEL — the cascade's FOUNDING DESIGN ([modifier.md](../specs/modifier.md) §1), stated
  as cache architecture.** A `CvDerivedCache` lives ON EVERY SCOPED ITEM, every level (world → team → player → area
  → city → plot); the cascade loads **yield packages in ONE UNIFORM FORMAT** (Σflat and Σpercent each their OWN
  package per channel; the unit is part of the slot key) into each scope's cache; each cache knows its own staleness
  from events at its OWN scope (a world change rebuilds the world package while every other level stands). **The
  only live calculation is adding the ~5 packages together at read.**
  **⛔ EVERY scope carries packages — whether a given scope's packages are EMPTY is IRRELEVANT (owner ruling).**
  The uniformity IS the design: it is what makes "the only live calc is summing the packages" literally true and keeps
  the read path identical at every level. A scope is never skipped because its packages look empty — an absent package
  forces the read to source that scope some OTHER way, and both ways are defects: a per-read walk (the cost class this
  doc exists to prevent), or an upper scope's sum stored in a lower one (breaking the scope principle,
  [modifier.md §1](../specs/modifier.md), which forces downward invalidation fan-out).
  **⛔ WORLD, TEAM and AREA carry NO yields directly — they can only apply MODIFIERS (owner ruling).** Yields
  originate at the scopes that actually produce them (plot / city / empire / unit); the upper three contribute
  percentage stacks (and, for area, enabler concerns) onto those yields. So their packages are percent-side by
  design — which is a REASON they still carry packages, not an excuse to omit them.
  Full rebuild of everything = LOAD ONLY.
  **Status against this ruling:** city / player / world / unit each sit on a `CvDerivedCacheSet` (their "ONE dirty
  protocol"), plot on the single-flag `CvDerivedCache`. **The two gaps: `CvArea` carries NO cache at all, and
  `CvTeam` carries only `CascadeTeamCaps` (capabilities, not a package).**
  **Where the AREA sums live instead — two different wrong homes, both cached (so neither is a per-read walk, and
  both break the scope principle):** the area YIELD percents fold into the CITY package (`yPctCity` — "bonus /
  building / event / power / area / capital all fold into yPctCity as scope/conditioned deposits",
  `CvCascadeAccumulator.cpp`), while the area MAINTENANCE percents sit on the PLAYER package as maps keyed by area id
  (`ps.maintAreaPct` / `ps.maintOtherAreaPct`). So filling the gap is not "add an empty package": it is
  (1) `CascadeAreaPackages` on a `CvDerivedCacheSet<CvArea>` holding the area's OWN percent Σ per channel,
  (2) the city read SUMS that package instead of area deposits folding into `yPctCity`, and
  (3) the player-side area-keyed maintenance maps retire into it. The mechanical pattern is the 6-step world-scope
  shape: struct + `CvDerivedCacheSet` → owner member → `cascadeRefresh<X>` delegate → `bind` + `markAllDirty` in the
  owner's reset → the `CascadeAccumulator` gather. `CvTeam` follows the same shape for its team-scope percents
  (`m_iTradeModifier`, `m_iEnemyWarWearinessModifier`, … — still legacy accumulators, §B1 of
  [legacy-cut-worklist.md](../plans/structural-cleanup/legacy-cut-worklist.md), which cannot be cut until the package
  exists to hold their values).
- **⚖ TWO DISTINCT KINDS OF DERIVED CACHE — do not conflate them:**
  - **The yield + percent packages are an INPUT/OUTPUT (value) cache** — memoize the computed number,
    dirty-invalidate on a source event, recompute from inputs on next read. This is what `CvDerivedCache` is FOR.
  - **The ENABLER's sets (the frontier + the operating-building set) are themselves derived state** —
    but maintained by **TARGETED PROPAGATION**: computed once (the walk-down), then each HAVE-change propagates
    through the **affected subset only** (re-check the affected candidates / ripple the fixpoint), updating the
    authoritative dataset **in place** via the reverse-index ([enabler.md](../specs/enabler.md) §7). They are
    NEVER blanket-invalidated-and-recomputed, and NEVER a parallel shadow-delta.
  ⛔ Blanket-recomputing the whole operating-building fixpoint for every city on every event runs the enabler's set AS an
  input/output cache — **"burning down the library of Alexandria" (DESPAIR_INDEX #2)**. The fix is targeted
  propagation, the shape the frontier ALREADY uses (`onBuildingChanged` / `recheckHave` off the reverse-index). It
  is likewise **not a given** the yield-package shape fits any OTHER non-package channel (the unit plane,
  properties); each is decided per-channel, only AFTER the spec is fully in place.
- **THE TARGET END-STATE — flags all turn, ONE unified rebuild at turn end.** The whole model in one line: *"if
  things have not changed, cache is not stale; if it has, rebuild."* Events are pure flag-sets (the DOMAIN-event →
  markDirty pattern, no mid-turn recompute); reads serve the standing snapshot all turn; ONE batched rebuild pass at
  turn end sweeps every flagged cache **in dependency order** (plot caches → city components → player aggregates),
  priming the next cycle. Consequences: no lazy-refresh reentrancy, no mid-turn freshness questions, rebuild cost is
  one measurable phase. Pairs with the [AI build-queue-parity model](../plans/parked/ai-build-queue-parity.md) — the
  snapshot IS the fairness mechanism; this end-state lands WITH that rework. **The event→cache routing is DERIVED
  FROM THE DATA, never hand-wired:** a DOMAIN event carries its SOURCE; the source's compiled deposits (the
  load-time strings→ints index, `Cascade/CvCascadeDepositIndex.{h,cpp}` — per-deposit interned segments +
  FK-resolved target id, compiled at readJson push-time) name exactly the channels × scopes × targets it touches —
  the dirty flags fall out of the deposit addresses. Today's hand-coded hook masks are the interim shape of that
  derivation; deriving the routing masks from the index lands with the turn-end unified rebuild.
- **Mid-turn read freshness:** the scope-packages model runs the per-player-slice SNAPSHOT (*"getting a yield event
  in the middle of a turn is not retroactive; start of next turn is what is expected"*), with the city-creation
  eager ensure (`CascadeAccumulator::cityCreated`) the one ruled exception. Full consequences:
  [scope-packages.md](../plans/structural-cleanup/scope-packages.md) §1.
- **EAGERLY BUILD ALL CACHES AT LOAD — the general policy stands.** *"I am happy to add even MINUTES to load time
  in order to have caches eagerly built on load in general."* ALL caches are warmed at load: a game-object's own
  derived cache (the plot-yield cache) eagerly from that object's own state, and the **cascade** eagerly by the
  **event reseed** — the spine fires every present-fact, so the cache-build/invalidation consumer populates every
  cascade package and turn 1 runs warm. What changed is ONLY the cascade's population MECHANISM: the
  recompute-from-state recalc (`playerSliceRebuild` + `worldRebuild`) is REMOVED (the CAPSTONE above); the eventspine
  reseed replaces it. No design ever serializes a derived value to save load time. **The perf LAW: "the name of any
  game in this town will always be TURN TIMES — if game load takes 50% longer it matters nothing if we can shave
  5-10-15% on turn time, because there is only 1 game load, but many many many turns."** Turn time is the objective
  EVERY perf decision optimizes; load time is the currency that pays for it. Ledgered as
  [DEC-turn-time-is-king](decisions.md#dec-turn-time-is-king).

This is the Clean-Architecture north-star applied to engine state: the repository **is** the contract, and it is the
lever for thinning the `Cv*` god-classes without touching the closed-EXE-bound `CvPlot`/`CvCity` layout. See
[north-star](north-star.md).
