# State repositories — recompute-only caches with a dirty trigger

The pattern for **derived engine state**: how a domain object's derived data (yields, commerce, health, …) is
computed and kept coherent. `CvPlot` and `CvCity` are **domain objects** — the in-game data entities — and they
**stay**. This is not about dissolving them (that's `CvCityAI`'s eventual job); it is about the derived layer.

This is the **design the cascade plane is built to**, stated independently of any one implementation of it. The
component (`CvDerivedCache`, `Sources/Infrastructure/`) is live, and the value-cache plane is built on it: the ONE
uniform package (`Sources/Cascade/CvCascadePackage.h`, channel-indexed Σflat100/Σpercent100 slots + receiver sums
on the 64-bit `CvDerivedCacheSet`) is a data member on team / player / area / city / plot (area per
(area × player), `CvCascadeAreaSlot`); the per-scope channel sets are minted from the compiled deposits at load
(`CvCascadeChannelRegistry`, the ClassificationRegistry precedent); the mark derivation lives on the DepositIndex
(`routeFor` + the condition-dependency routes); the modifier's own spine consumer (`CvModifierConsumer`,
load-active) applies the derived masks; the gather (`CvCascadeGather`) is the one rebuild implementation and the
combine lives on the calc surface (`InfoValuation::cityRate100` / `groupSum100At`).

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
is dead. ⛔ The pull must be a CACHE at EVERY level, never a per-read walk: re-summing the radius on every
`getPlotYield` call turns the game's hottest read O(radius) — measured at 913M plot reads in one turn inside the
governor's valuation, the cost class this whole doc exists to prevent. The engine's actual base yield thereby equals the build-order-independent value the cascade computes —
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
- **Every derived cache on the cascade plane uses this component** — there is no second cache mechanism, and a
  hand-rolled dirty-flag pair beside it is a defect ([DEC-uniform-cache-shape](decisions.md#dec-uniform-cache-shape)).
  The legacy `CvCity` hand-rolled dirty caches (`m_aiCommerceRate`, `m_aiBuildingCommerce100`, squirrelBanana) are
  **demolition fodder**, never conversion targets: they are cut when the channel that replaces them lands, not
  polished on the way.

## ⚖ Refinements

- **PARTIAL DIRTYING.** A cache whose value composes from several isolated **plugin numbers** (each package a
  standing value; "the rest of the pipe stays the same") carries a **dirty BITMASK, one bit per component**, and a
  trigger marks only the components its event feeds — the `CvDerivedCacheSet` form. The single-flag form stays for
  leaf caches.
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
  **⛔ THE ORIGIN RULE — THIS IS THE PURE CASCADE DESIGN (owner), not a constraint bolted onto it:**
  - **YIELDS come from exactly three sources: PLOT, SPECIALISTS, and BUILDINGS (city).** Nowhere else produces a
    yield. So the flat/yield side of a package exists at **plot** and **city** only.
  - **MODIFIERS come from everything BUT plot** — city, area, empire, team, world. So the percent side exists at
    every scope except plot.

  Plot and the upper scopes are therefore mirror images (yield-only vs percent-only), and **CITY is the single
  scope carrying both**. That is why "whether a scope's packages are empty is irrelevant" is not hand-waving: the
  shape is uniform, and the origin rule says which half any given scope ever fills.

  **⛔ THE CONSOLIDATION REQUIREMENT (owner): every modifier/yield cache is ONE shape** — TWO DICTIONARIES per
  scope object, one flats and one percents, each an int keyed by channel. The drift it replaces is the ~33
  hand-named scalar fields (`scGpBaseBld`, `scDefense`, `scDefBombard`, `scMaintModCity`, `scTradeCity`,
  `brCityMilitary`, …): a hand-named field cannot be addressed uniformly, so it forces a bespoke invalidation
  path per field, which is how that many accumulated. A new scope or channel must be DATA, not a new struct.

  **⛔ KEYS ONLY WHERE THEY ARE NEEDED (owner) — the storage is NOT a global dense index.** The channel set is
  DATA-DEFINED (`PROPERTY_*` is one channel per property info) and no object uses more than a fraction of it, so
  a dense array over every channel on every object is mostly zeros — on 9,600 plots that is ~7 MB of nothing.
  Each scope carries ONLY the channels authored AT that scope, both the channel ids and the per-scope sets
  derived from the data at load (the `ClassificationRegistry` minting precedent), never hand-listed. Measured
  from `Assets/Data`: plot **13** · city **40** · empire **50** · area **3** · team **3** · self 1 — 76 distinct
  non-unit channels, but no object carrying more than 50. ⚠ city and empire exceed a 32-bit dirty mask, so the
  shared `CvDerivedCacheSet` mask widens to 64-bit (every existing user occupies few bits and is unaffected).
  *Area and team being THREE channels each is also why they never got packages: as a bespoke struct each was a
  project; as three keys each is trivial.*

  **⛔ TWO SCOPES ARE DELIBERATELY NOT PACKAGES (owner):**
  - **WORLD is CONFIG** — cost multipliers and the like, carried by eras / gamespeeds / handicaps. It changes
    essentially never and is read from its sources, not cached behind a dirty protocol. A project granting
    something to every player is NOT world-scope state: it authors the plural TARGET `world.empires`
    ([json.md §3.3](../specs/json.md)) and lands in each PLAYER's package. The handful of `health.world` /
    `happiness.world` / `tradeRoutes.world` project authorings are mis-scoped data, a curator fix
    ([DEC-recurate-on-decision](decisions.md#dec-recurate-on-decision)).
  - **UNIT is RESOLVED VALUES, not a package** — "when the number is put on the unit, no more percentages or
    whatever is involved, the data just IS". The exact set of numbers a unit carries is known, so they are summed
    and stored individually, and they dirty on a different trigger from everything else: ONLY when a promotion or
    combat class changes. It is the most static plane in the engine. The unit's storage is therefore NOT a
    bespoke struct awaiting consolidation — it is correctly its own shape, and the 12 unit-only families
    (`strength`, `movement`, `withdrawal`, `firstStrike`, `capture`, `collateral`, `heal`, `bombard`, `air`,
    `cargo`, `range`, `pillage`, …) never enter a scope's channel set.
  ⚠ Hand-maintained duplicates DRIFT — that is not theoretical: the maintenance decomposition and its cached fill
  duplicated five terms, and the L8 home/otherArea overlay landed in one and not the other, so `/computed`
  under-reported by 39 against the served value until the duplicate was replaced by a delegation.
  Full rebuild of everything = LOAD ONLY.
  **⛔ THE FIX IS NEVER "ADD ANOTHER STRUCT" — that is the failure mode this ruling exists to close.** The previous
  substrate grew ONE BESPOKE STRUCT PER SCOPE, each with hand-named per-channel members instead of channel-indexed
  Σflat/Σpercent; it is archived and must not be reconstructed ([superseded-ideas](superseded-ideas.md) #14).
  **A missing scope is a SYMPTOM of that, not the disease:** with one uniform package, giving a scope its packages
  is a single member; with bespoke structs every scope is its own project — which is exactly why the small scopes
  (area, team — three channels each) never got one, and why their sums leaked into whichever neighbour already had
  a struct. So the package TYPE is unified FIRST (one owner-templated, channel-indexed package on
  `CvDerivedCacheSet<TOwner>`), after which every scope falls out of the same member. Adding a further per-scope
  struct deepens the divergence this closes.
- **⚖ THE KEY IS SAMENESS (owner ruling): every cache is the SAME OBJECT TYPE everywhere, and they ALL invalidate
  the SAME WAY.** That — not the per-scope layout — is the requirement the whole model rests on. One templated
  cache type (`CvDerivedCacheSet<TOwner>` over a channel-indexed slot table) on every owner, and ONE mark
  derivation driving all of it. What varies between scopes is only WHICH SLOTS carry a value; the type and the
  protocol never vary.
  - **A RECEIVER is not a different kind of cache — it is the same cache holding a different slot (owner).**
    Whatever scope CONSUMES a channel caches its realized sum as **one variable per channel**, in the same cache
    beside the packages: `CvPlayer` caches research / gold / culture / espionage; `CvCity` caches production /
    culture and the other sums it consumes. The city's realized yield rate (`yRate100[]`) is the general shape,
    not a special case — so there is no separate "receiver mechanism" to build.
  - **⛔ THIS IS WHY HAND-NAMED SCALAR FIELDS ARE THE DEFECT, not just untidy.** A named field cannot be addressed
    uniformly, so it forces its own bespoke invalidation path — which is precisely how 33 of them accumulated.
    Channel-indexed slots invalidate by derived mask with no per-field code.
  - **The receiving scope is NOT the storing scope.** A package never moves to its consumer (that breaks the scope
    principle); the consumer stores only its own realized TOTAL — one cheap variable per channel.
  - **⛔ NOT a push accumulator, and NOT a per-read walk — the cached sum sits between those two failures.**
    Rejecting the legacy incremental accumulator does not license recomputing on every read: an empire-scope
    getter that re-walks every city per call (`CvPlayer::getCommerceRate`) is the per-read-walk cost class this
    doc exists to prevent, merely relocated one scope up.
  - **ONE EVENT MARKS BOTH LEVELS (owner).** The event's derived mask names the packages it touches **and** the
    sum slots those packages feed — one mark derivation, two targets (for a cross-scope aggregate, two owners).
    There is **no** dependency-ordered rebuild pass: both are dirty, and a sum's rebuild reads its packages
    through their own lazy dirty-check, so the package refreshes first by construction and a sum can never sit
    stale behind a clean package.
  - **Which scope receives a channel is spec'd, not chosen per site:** one consuming scope per channel
    (food/production → city; gold/research/espionage → empire), with **culture the lone dual-consumer** (the city
    sums it for plot culture + border expansion, the empire for civ culture + traits — two independent sums over
    the same packages).
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
  load-time strings→ints index, `Data/CvDepositIndex.{h,cpp}` — per-deposit interned segments + FK-resolved target
  id + the resolved channel/scope slot, compiled at readJson push-time) name exactly the channels × scopes × targets
  it touches — **the dirty flags fall out of the deposit addresses.** The routing is a pure function of the index;
  a hand-coded hook mask per event site is a per-site bespoke path of exactly the kind
  [DEC-uniform-cache-shape](decisions.md#dec-uniform-cache-shape) forbids. Derive it from the index.
- **Mid-turn read freshness: the per-player-slice SNAPSHOT** — *"getting a yield event in the middle of a turn is
  not retroactive; start of next turn is what is expected"*. A newly-founded city is the one ruled exception (it
  must read correct values the turn it exists, so its packages build eagerly at creation rather than waiting for
  the next slice).
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
