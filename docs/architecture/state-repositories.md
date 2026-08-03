# State repositories — recompute-only caches with a dirty trigger

The pattern for **derived engine state**: how a domain object's derived data (yields, commerce, health, …) is
computed and kept coherent. `CvPlot` and `CvCity` are **domain objects** — the in-game data entities — and they
**stay**. This is not about dissolving them (that's `CvCityAI`'s eventual job); it is about the derived layer.

This is the **design the cascade plane is built to**, stated independently of any one implementation of it. The
component (`CvDerivedCache`, `Sources/Infrastructure/`) is live, and the value-cache plane is built on it: the ONE
uniform package (`Sources/Cascade/CvCascadePackage.h`, channel-indexed Σflat (×100) / Σpercent (unscaled) slots + receiver sums
on the 64-bit `CvDerivedCacheSet`) is a data member on team / player / city / plot; the per-scope channel sets are minted from the compiled deposits at load
(`CvCascadeChannelRegistry`, the ClassificationRegistry precedent); the mark derivation lives on the DepositIndex
(`routeFor` + the condition-dependency routes); the modifier's own spine consumer (`CvModifierConsumer`,
load-active) applies the derived masks; the gather (`CvCascadeGather`) is the one rebuild implementation and the
combine lives on the calc surface (`InfoValuation::cityRate` / `groupSumAt`).

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

> **domain object mutates → marks the derived value → THE MARK REBUILDS IT → consumers up the chain read that one
> value as a bare fetch.** One trigger, one refresh path, one source of truth.

A derived cache in this model is:

1. **Mark-driven, and the MARK is what rebuilds.** A trigger marks the component and the recompute runs there; a
   READ is a **bare fetch** and never recomputes (§ `ensure()` below — an ensure-on-read protocol is tombstoned).
   The expensive recompute runs **once per change**, never per read. The one deferral is the load bracket: inside
   `GAME_LOAD_STARTED`..`FINISHED` the marks are BANKED (a mid-read recompute would read half-deserialized state)
   and each system drains its own banked marks once at `GAME_LOAD_FINISHED`.
2. **Recompute-only, NOT serialized** — the [DEC-derived-never-trusted](decisions.md#dec-derived-never-trusted) rule,
   applied per-field. Neither the value nor the flag is saved; on load the flag is dirty by default, so the first read
   recomputes from current state — **never stale-from-save**. Drop serialization by the **soft-remove**
   ([DEC-save-remove-is-soft](decisions.md#dec-save-remove-is-soft), [save.md §3](../specs/save.md)): FULL-DELETE the
   read + write and NAME the tag in `Assets/savemigration.txt`, which drains an old save's orphan bytes by name so
   nothing after it shifts (a no-op on a new save that never wrote it). **No `WRAPPER_SKIP_ELEMENT`** (it leaves the
   dead member named — a rollerskate target); and just deleting the read/write *without* the `savemigration.txt` entry
   desyncs the whole downstream read.
   **This is UNIVERSAL, not per-field-optional (owner ruling): NO cache is ever serialized** — so nothing derived
   is ever read from a save, and there is correspondingly nothing for a blanket recompute to purge.
   ⛔ **No blanket recompute of derived state exists anywhere in the engine, and none is ever to be built**
   ([DEC-no-self-heal](decisions.md#dec-no-self-heal)): the event spine builds the state, LOAD is the only full
   pass (§ THE CAPSTONE RULE), and a missed invalidation must stay visible instead of being swept away. ⛔ A
   wipe-the-totals-and-reapply pass over live game objects is therefore never a maintenance path to add or extend
   — *"it is inherently obsolete under the event-driven system, since the new system recalcs on load anyway"*
   (owner). It is the exact shape this model replaces, and it is worst where it looks most useful: firing on the
   saves most likely to have drifted is what would hide the missed emits the spine exists to expose. Each
   remaining serialized cache
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

**Worked shape (the plot-yield cache):** `getYield()` = `return cached` — a bare fetch, always O(1);
`updateYield()` is the **trigger** (marks the slot, which is what rebuilds it, and fires the downstream marks the
old push carried — no push); **⚖ THE SUM OF THE PACKAGES IS CACHED AT ITS TARGET — as a RECEIVER SLOT, never as a member beside it (owner).**
Each channel has ONE consuming scope (production → city; the commerces further up), and the Σ that lands there is
cached in that scope's OWN package (`CvCascadePackage::sum`, read `readSum`, marked `markSum`) — **invalidated by
the EVENT SPINE per YIELD TYPE and LOCATION**, i.e. the derived mask names the channel and the owner it fires on
(`scopeReceiversFedBy(CASC_SCOPE_CITY, CASC_SCOPE_PLOT)` for the plot-fed city sums). Its inputs are read through
their own marks (`CvPlot::getCascadePackage().sourceFlat(channel)` — the gather's `baseFlat` plot leg), so an input
the same event marked rebuilds before it is summed.

⛔ **What is banned is a HAND-NAMED field holding that same number** — a `CvCity::m_plotYieldSum`-shaped member is
the defect [DEC-uniform-cache-shape](decisions.md#dec-uniform-cache-shape) names (it cannot be addressed by the
derived mask, so it forces a bespoke invalidation path) and a second maintenance surface for a fact the modifier
consumer already routes. ⛔ Equally banned is the other direction: re-summing per read. **Cache it — in the slot
that already exists.** The push-maintained `m_aiBaseYieldRate` is dead, and a legacy tier-1 accessor over it
(`getPlotYield`) is a DELETION, not a value to re-home: its consumers read the channel at its receiving scope
([DEC-new-getter-surface](decisions.md#dec-new-getter-surface)). ⛔ The pull must be a CACHE at EVERY level, never a per-read walk: re-summing the radius on every
`getPlotYield` call turns the game's hottest read O(radius) — measured at 913M plot reads in one turn inside the
governor's valuation, the cost class this whole doc exists to prevent. The engine's actual base yield thereby equals the build-order-independent value the cascade computes —
stale-cache divergences resolved **at the source**, behaviour-preserving
([DEC-parity](decisions.md#dec-parity)).

### ⛔ A SELF-HEAL IS THE FOSSIL OF A MISSING EMIT — so it is a SEARCH, not just a ban

**Where self-heal came from (owner):** the old branch was full of blanket recalculations *because agents did not
properly wire the events and shortcut by adding a self-heal calc instead*. That is the causal direction, and it is
what makes [DEC-no-self-heal](decisions.md#dec-no-self-heal) findable rather than merely prohibitive:

> **A recalc does not appear because someone wanted a recalc. It appears because a fact was not announced, the
> value went wrong, and recomputing was the cheapest way to make the symptom stop.**

⇒ **Every self-heal marks the spot where an emit is missing.** So when you find one — a periodic rebuild, a
"refresh if stale", a runaway cap that "recovers next slice", a wipe-and-reapply — do NOT simply delete it and
declare the rule enforced. **Find the fact it was compensating for and wire THAT**; the recalc then has nothing
left to do and is removed as a consequence, not as the fix.

⚠ And a self-heal is worse than the bug it hides, which is why it is banned rather than tolerated: the missed emit
would have surfaced as a visibly wrong value that someone could chase, whereas the recalc converts it into
permanent invisible drift **and** reinstates exactly the per-read/per-turn work the caches exist to delete.
⛔ A comment claiming a recalc "heals" something is itself suspect twice over — the healer may not even exist any
more (a slice rebuild that was since removed), leaving a truncation that repairs nothing and announces nothing.

### ⛔ THE LEGACY-ACCUMULATOR CUT — every accumulator, ONE uniform mechanism

> Binding: [DEC-accumulator-cut-uniform](decisions.md#dec-accumulator-cut-uniform). **NOT wellbeing-specific — it is
> EVERY legacy serialized incremental accumulator, and they all work exactly the same way.** Wellbeing was the pilot
> that proved it; the same cut then repeats UNCHANGED. **Blast radius is not a concern — it is the SIGNAL: a cut
> that does NOT reach broadly means the legacy is not actually being cut.**

**What an accumulator IS — the three-part test.** A member that is ALL of: (1) **serialized** in `read()`/`write()`;
(2) **incrementally maintained** by `change*`/`update*`/`process*` deltas, never recomputed-from-source; (3) a
**per-turn game quantity the cascade now owns**. These are the STORED-ACCUMULATOR DRIFT class
([modifier.md §2b](../specs/modifier.md)): they carry decades of save history no live source can reproduce, so a
stored-vs-recompute diff is **DRIFT (history pollution), never state to preserve** — the recompute is the correct side.

**The uniform mechanism:**

1. Add the cluster's **fresh-gather accessor** returning its term from the cascade, ×100 internally.
2. **Re-point the realized getter** to it, reducing `÷100` at the reader boundary — **no `*Legacy` fallback, no
   variant getter** ([DEC-no-legacy-masking](decisions.md#dec-no-legacy-masking): anything sneaking a legacy value
   back in is an ERROR, never a safety net; on a red tree a wrong/empty cascade value is the CORRECT exposed outcome).
3. **HARD-DELETE** the member and its maintainers.
4. **FULL-DELETE the read + write** and NAME the tag in `Assets/savemigration.txt` — the reader drains the orphan
   transparently ([save.md §3](../specs/save.md)). No `WRAPPER_SKIP_ELEMENT`; an UNLISTED deleted-read orphan is the
   one hard desync.
5. **The COMPILER is the census** — every surviving consumer is a compile error to rewire; you cannot
   flip-and-pretend. Done = endpoint-observable on a loaded save, not "it compiles."

⚠ **Audit each deleted `change*`/`update*` BODY for side effects first** — legacy changers carry non-obvious riders
(trade-network recompute, UI-dirty, power) the surviving trigger site must still fire ([save.md §6](../specs/save.md)).

**Incremental-accumulate ledgers convert to recompute-from-source.** A serialized player ledger that replays its
accumulator onto the loaded value double-counts by build order. The conversion is the uniform one above: recompute
from the player's own held sources on dirty, make the changer trigger-only, and have the cities PULL it.

**Event/vote grants are NOT cached — they are a SEPARATELY PERSISTED store.** A per-building commerce change has
two sources of fundamentally different nature: the **empire** grant (`GlobalBuildingExtraCommerces`, civics) is
DERIVABLE → the recompute-from-source cache; the **event/vote** grant (fires ONCE) is **genuine one-shot state, NOT
derivable** — *"having events just be stored in the cache is lunacy"* (a recompute cache would wipe them). They live
in their own serialized field (`CvCity::m_aBuildingCommerceChangeEvents`), outside the recompute path; the reader
sums `player-recompute (empire) + city event/vote (persisted)`.

## ⛔ THE RECOMPUTE IS AN ENDPOINT ORACLE — NOT THE READ PATH, AND NEVER AN IN-DLL DIFF (owner)

**"The ensures were some of the earliest rollerskates."** Read-side `ensure()` is tombstoned by name
([superseded-ideas](superseded-ideas.md) #14: *"never re-add … an `ensure`-on-read protocol"*) and measured:
an ensure-per-read protocol on AI-hot paths ground unit automation. A read is a **BARE FETCH, unconditionally** —
there is no gate test on it, because there is nothing on the read path to gate.

**The ruling (owner): keep the recompute, but it is called ONLY BY AN ENDPOINT, and the COMPARISON HAPPENS
EXTERNALLY.** Two endpoints, one external diff:

- **the STORED endpoint** serves what the EVENTS built — the live package/set values, read exactly as a consumer
  reads them;
- **the ORACLE endpoint** runs the recompute FROM SOURCE **into SCRATCH** and serves that;
- **an external consumer diffs the two.** A disagreement is a **missed emit**, named by scope + channel + owner.

⛔ **NEVER emit a divergence as a spine event — that is a GUARANTEED LICENSE TO BUILD SELF-HEALING (owner), and
it is the reason this is a hard rule rather than a preference.** An event is an **invitation to a consumer**.
Put a divergence on the spine and the next agent writes the consumer that handles it — and "handling" a value
known to be wrong means CORRECTING it. Self-heal then arrives because the SHAPE invited it, not because anyone
decided to add it, and it arrives wearing the authority of the event spine
([DEC-no-self-heal](decisions.md#dec-no-self-heal)). **A PULL (an endpoint someone asks) cannot grow that
consumer; a PUSH (an event fanned to whoever registers) grows it by default.** So the DLL neither compares nor
reports: a divergence is not a happening, it is an OBSERVATION an external reader makes about two served
numbers, and it has no in-DLL representation at all — no diff, no log line, no event, no field.

⛔ **Frame this as REBUILD. Shadow is dead and cutover is dead (owner) — neither is a lens for any remaining
work.** Do not describe this oracle as "shadowing" the cascade, do not reach for cutover staging, and do not
revive either vocabulary to justify a comparison: the sanctioned shape is simply *two served surfaces, diffed
outside* ([http-endpoints.md](../specs/http-endpoints.md)).

⚑ **Serving into SCRATCH is what makes "never repairs" STRUCTURAL.** The oracle cannot write the stored slots
because it is not given them — no snapshot, no restore, no discipline to remember and no window in which a
half-finished recompute could leave a repaired value behind. A verifier that repairs is self-heal wearing a
different hat ([DEC-no-self-heal](decisions.md#dec-no-self-heal)); this one structurally cannot.

⚑ **What makes this comparison REAL:** it pits **event-built state against a fresh recompute-from-source** — two
genuinely different derivations of the same number, which agree only while every mutation emitted. A comparison
whose two sides share a derivation can never turn red and verifies nothing; this one can, and that is its job.

**The identity a divergence needs to be actionable:** "some city's production flats are wrong" across 185 cities
identifies nothing, so every served value carries its owner, **interpreted per scope** as the spine's DOMAIN ints
are interpreted per event: city = `(owner, cityId)` · empire = `(playerId, —)` · team = `(—, teamId)` ·
plot = `(x, y)` (a plot has no owner-independent id, and the map index needs a map that does
not exist at bind). Identity is passed IN at bind — the scope owners share no common id accessor.

⚠ **Consequence, and it is not optional: the REBUILD MOVES ONTO THE MARK.** The event that marks a slot is what
rebuilds it — the same shape the contexts use — with the batched turn-end sweep as the later refinement
(§ THE TARGET END-STATE).

**The realized shape.** `ensure()` is gone as a name; what stands in its place:

- **`CvDerivedCacheSet::markDirty(mask)` marks AND rebuilds.** The rebuild is `rebuildMarked(mask)` (clear the
  mark first, then run the owner's refresh). Marking without rebuilding is not an available move — which is the
  invariant that lets every read be bare.
- **THE ONE DEFERRAL — the load bracket.** Inside `GAME_LOAD_STARTED`..`FINISHED` `markDirty` BANKS the mark and
  does not rebuild: a rebuild mid-read evaluates against half-deserialized state (the context stores, the areas
  and the plot-group network complete only when the stream ends), and with no read-side recompute that wrong
  value would then stand forever. Each system drains its OWN banked marks at `GAME_LOAD_FINISHED`
  (`CvModifierConsumer::mc_drainLoadMarks`) — this is the reseed's eager load build, **not** a blanket: only bits
  an in-read event actually marked rebuild, and a package no event reached stays unbuilt and visibly wrong.
  ⚠ **Consumer registration order is therefore a contract, and it binds BOTH state-building machines** (consumers
  dispatch in registration order): **contexts → enabler → modifier**. The contexts' consumer BUILDS the stores on
  `GAME_LOAD_FINISHED`; the enabler's load-end gate pass evaluates its conditions THROUGH those stores
  (`BuildingEnabler` → `getCityContext().fillEvalCtx`), and the modifier's drain does the same for every package
  the reseed marked. Either machine registered ahead of the contexts evaluates against EMPTY stores — and with a
  read being a bare fetch and no self-heal existing, nothing re-derives it afterwards. **Anything that reads a
  context store registers after the contexts.**
- **TWO read surfaces, and only one of them is a read path.** `CvCascadePackage::readFlat/readPercent/readSum`
  are the CONSUMER read path — bare fetches. `sourceFlat/sourcePercent/sourceSum` are the **rebuild-path input
  reads**, called only from `CascadeGather`: a combine runs inside a rebuild, so it reads a cross-scope input
  through that input's own mark. That is what makes "there is no dependency-ordered rebuild pass" true — the mark
  ORDER within an event is irrelevant, and the load drain needs no ordering either. It is **not** the retired
  ensure-on-read: it can only fire for a slot something DID mark, so a MISSED emit still reads clean and stays
  visibly wrong.
- **THERE IS NO GATE ON A READ.** A read is a bare fetch unconditionally — nothing is tested on it, because
  nothing on it can recompute.
- **The two served surfaces, per plane** (`/computed/*`, [http-endpoints.md](../specs/http-endpoints.md)):
  `.../stored` serves what the events built (`CvCascadePackage::readValuesInto`,
  `EnablerKernel::operatingBuildings`, `CascadeCapabilities::storedUnion`) and `.../oracle` serves the
  from-source recompute into a buffer the endpoint owns (`CascadeGather::gather*Into`,
  `EnablerKernel::recomputeOperatingSetInto`, `CascadeCapabilities::refreshInto`). Both sides render through
  ONE renderer per plane (`Sources/Tools/CvOracleEndpoints.cpp`), so the documents are diffable field by field.
- **Where the oracle lives is decided by where the STORAGE lives.** `CvDerivedCacheSet` owns only the mark
  protocol, so the storage owner supplies the scratch (the gather's `gather*Into` for the cascade packages);
  the single-flag forms expose `recomputeInto(buffer)` directly.
- **An oracle run is a FULL RECALC, by design (owner) — it reads NOTHING off the stored surface.** Every input,
  including every cross-scope one, is recomputed from source. ⛔ The tempting alternative — recompute only the
  asked-about object and read its cross-scope inputs off the stored packages — is **WRONG, and wrong in the
  specific way that killed the old twin surface**: an oracle that consumes stored values is partly built on the
  very state it exists to check, so a wrong input is silently INHERITED and the two sides quietly share a
  derivation again. **Independence is the entire value of the oracle**; nothing may be traded for it.
  *(Attribution is not lost by going full: a drifted low scope diverges on its OWN row too, so the external
  differ walks from the lowest diverging scope up to find the root cause — and attribution is the EXTERNAL
  reader's job anyway, which is the whole point of taking the comparison out of the DLL.)*
- **⛔ An oracle run's COST IS IRRELEVANT — "correct is correct" (owner).** It is invoked deliberately, by a
  human or a tool asking a question, never on a turn path. So it is never trimmed, sampled, memoized, or made
  incremental to look cheap: a full recompute that takes as long as it takes IS the deliverable. Do not optimize
  it, and never let a performance argument reshape it — that is how an oracle stops being independent.
- **An oracle run ANNOUNCES nothing.** It emits no `[CASCADE] rebuilt` line: nothing was rebuilt, and a
  verification sweep must not move the numbers that describe real work.

## The standardized `CvDerivedCache` component

**One reusable C++03 component** (`Sources/Infrastructure/CvDerivedCache.h`) rather than hand-rolled per cache — a
templated value-holder with the recompute injected as a member-function-pointer (poor-man's-DI-adjacent,
[patterns](patterns.md)). Three forms: the single-flag **`CvDerivedCache<TOwner,T,N>`** (leaf caches — the plot
yield), the partial-dirty **`CvDerivedCacheSet<TOwner>`** (component-granular, see Refinements), and the
runtime-sized **`CvDerivedCacheVec<TOwner,T>`** (the recompute receives the vector and fully sizes+defines it — the
player building-commerce ledger).

```cpp
template <class TOwner, class T, int N>
class CvDerivedCache {                       // recompute-only, mark-driven, NEVER serialized; the single PULL source
    mutable T    m_data[N];
    mutable bool m_marked;
    TOwner*      m_owner;
    void (TOwner::*m_recompute)(T*) const;   // fills m_data from CURRENT state (needs owner; stays owner-side)
public:
    void bind(TOwner* o, void (TOwner::*fn)(T*) const);
    void markDirty() const {                 // the trigger — call at every input-change site. THE MARK REBUILDS.
        m_marked = true;
        if (!spineGameLoadInProgress()) rebuildMarked();   // banked inside the load bracket; drained at its end
    }
    void rebuildMarked() const {             // clear the mark FIRST, then recompute (contract rule 1)
        if (m_marked && m_owner) { m_marked = false; (m_owner->*m_recompute)(m_data); }
    }
    T get(int i) const { return m_data[i]; }               // A BARE FETCH — never a recompute, never a gate test
    void recomputeInto(T* out) const;        // THE ORACLE — the same recompute over the CALLER's buffer, no gate
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
  (`playerSliceRebuild` + `worldRebuild`) was a stabilize-the-drift STOPGAP and is REMOVED. Post-load, an event marks only the package(s) its deposits touch, and **ONLY marked (dirty) packages
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
  as cache architecture.** A `CvDerivedCache` lives ON EVERY SCOPED ITEM, every level (world → team → player
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
  - **MODIFIERS come from everything BUT plot** — city, empire, team, world. So the percent side exists at
    every scope except plot.

  Plot and the upper scopes are therefore mirror images (yield-only vs percent-only), and **CITY is the single
  scope carrying both**. That is why "whether a scope's packages are empty is irrelevant" is not hand-waving: the
  shape is uniform, and the origin rule says which half any given scope ever fills.
  ⚖ **The rule governs the YIELD/RATE plane; for every other family the sides are the DATA's and the minted
  channel sets enforce them** (wellbeing authors empire flats; health/defense/property author plot percents)
  — [modifier.md §1](../specs/modifier.md). ⛔ Consequence for any read-side roll-up: **the channel set is the
  gate, never a hand-written per-scope filter.**

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
  from `Assets/Data`: plot **13** · city **40** · empire **50** · team **3** · self 1 — the distinct non-unit
  channels, with no object carrying more than 50. ⚠ city and empire exceed a 32-bit dirty mask, so the
  shared `CvDerivedCacheSet` mask widens to 64-bit (every existing user occupies few bits and is unaffected).

  **⛔ A SCOPE MUST BE UNAMBIGUOUSLY OWNABLE — WHICH IS WHY A LANDMASS IS NOT ONE (owner).** This is the test a
  candidate scope has to pass, and it explains the whole spine at once:
  - **WORLD passes by being UNIVERSAL** — *"game scope works, because it affects everyone, always"*, so the
    question of who owns the value never arises.
  - **team / empire / city / plot pass by being OWNED BY EXACTLY ONE PLAYER** up the chain, which is what lets a
    deposit roll DOWN and a target read one combined total.
  - **A LANDMASS passes NEITHER.** *"It knows no borders"* — one landmass spans several empires at once, so an
    effect on it *"affects individual players"* and is inherently a per-(landmass × player) **CROSS-PRODUCT**
    rather than a scope. Modelling it as one forces a bespoke slot into the MIDDLE of the containment spine, and
    that bespoke slot is the TELL, not the solution.

  So **there is no area scope**: `"area"` is not a scope token, no object carries an area package, and the
  containment spine is `world › team › empire › city › plot` ([json.md §3.2](../specs/json.md)). The legacy
  `iArea*` authorings were modders reading "area" as "player" — they author at **EMPIRE** — and the ONE genuine
  area concept is a PHYSICAL CONTIGUITY constraint (you cannot run power lines across an ocean), which is the
  engine-side clean-power counter and never a cascade channel.
  ⚑ **The area ID SURVIVES as a plain FACT**, and that is the whole of what an area is to the cascade: a bare id
  plus its tile count, forwarded by `CityContext` for the `AREA_SIZE` token and the coastal water-body read
  ([contexts.md](contexts.md)). ⛔ The city carries that ID, never a `CvArea*` or a per-read `area()` chase — a
  per-read `area()->getNumTiles()` dereferences a whole object to answer a counter an int already holds.
  **⚑ Areas are VIRTUALLY NEVER recalculated (owner)** — `CvMap::recalculateAreas` exists for the extreme case
  of terrain levelled to sea level (the WMD mechanic), plus map generation; a landmass does not otherwise split
  or merge in play. Treat a rebuild as RARE-but-real: it does `m_areas.removeAll()` and reassigns every id.
  **So the rebuild announces itself as a DOMAIN fact (owner): emit "areas recalculated" and force the recheck** —
  every holder of an area id re-reads, rather than each cache inventing its own staleness test. Being rare, the
  blanket costs nothing; and it is not the banned self-heal: a wholesale identity reassignment is not
  addressable per-source, so no finer route exists to derive ([DEC-no-self-heal](decisions.md#dec-no-self-heal)
  bans papering over a
  MISSED invalidation, not announcing a genuine wholesale one).

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
    ⚖ **STRENGTH'S BASE IS PER-UNIT STATE AND IS DELIBERATELY SERIALIZED (owner ruling).** Every other resolved
    slot takes the unit's own TYPE from the gather, because it is a pure function of that type. Base strength is
    not: **WorldBuilder edits an individual unit's strength**, and the WBS scenario format persists the result
    (`CombatStr=`, written only when it differs from the type). *"You want people to be able to do things in
    WorldBuilder."* So the base lives on `CvUnit` as the serialized `m_iBaseCombat`, the resolved plane carries
    the promotion / unit-combat **DELTA ONLY**, and the consumer adds the two. ⛔ This is the ONE carve-out in an
    otherwise uniform gather, and it is load-bearing: letting the type contribute to the strength slot as well
    silently DOUBLE-COUNTS every unit's authored base. ⚠ It is therefore NOT a
    [DEC-derived-never-trusted](decisions.md#dec-derived-never-trusted) violation — the value is genuine
    per-unit state that no amount of re-derivation can reconstruct, which is exactly why it is stored.
  ⚠ Hand-maintained duplicates DRIFT — that is not theoretical: the maintenance decomposition and its cached fill
  duplicated five terms, and the L8 home/otherArea overlay landed in one and not the other, so `/computed`
  under-reported by 39 against the served value until the duplicate was replaced by a delegation.
  Full rebuild of everything = LOAD ONLY.
  **⛔ THE FIX IS NEVER "ADD ANOTHER STRUCT" — that is the failure mode this ruling exists to close.** The previous
  substrate grew ONE BESPOKE STRUCT PER SCOPE, each with hand-named per-channel members instead of channel-indexed
  Σflat/Σpercent; it is archived and must not be reconstructed ([superseded-ideas](superseded-ideas.md) #14).
  **A missing scope is a SYMPTOM of that, not the disease:** with one uniform package, giving a scope its packages
  is a single member; with bespoke structs every scope is its own project — which is exactly why a small scope
  (team, at three channels) never got one, and why its sums leaked into whichever neighbour already had
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
  - **⛔ A CROSS-SCOPE receiver total is the Σ of its MEMBERS' REALIZED values — and NOTHING beside that Σ.** The
    empire's gold / research / culture / espionage sums are Σ over the player's cities of each city's realized
    rate of that channel: exactly the quantity the retired per-read city walk answered, merely cached. The
    per-city quantity for a commerce channel is the whole [modifier.md §2a](../specs/modifier.md) split — the
    slider share of the city's COMMERCE yield, the channel's own deposits, and the process conversion — not the
    channel's deposits alone. ⛔ **An upper scope's own package is NEVER added on top of that Σ:** its deposits
    roll DOWN ([modifier.md §1](../specs/modifier.md)) and are therefore already inside every member's realized
    value, so adding them again at the receiving scope counts each empire-scope deposit once per city PLUS once
    more — a silent multiplication that compiles, runs, and simply reports wrong numbers.
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
    (food/production → city; gold/research/espionage/**maintenance** → empire), with **culture the lone
    dual-consumer** (the city sums it for plot culture + border expansion, the empire for civ culture + traits —
    two independent sums over the same packages).
    ⚑ **MAINTENANCE is the one NON-commerce receiver, and it is what makes the rule general rather than a
    commerce habit.** The empire's total maintenance is the Σ over its cities of each city's realized
    maintenance — precisely the cross-scope receiver shape above — so it is a SLOT in the empire's own package
    cache, never a hand-named cache beside it ([DEC-uniform-cache-shape](decisions.md#dec-uniform-cache-shape):
    a named field cannot be addressed by a derived mask, so it forces its own bespoke invalidation path).
    ⚠ Its per-city quantity is the one a package cannot answer alone: a city's realized maintenance composes the
    three component KINDS (distance / numCities / colony) each against its own modifiers, takes the `amount`
    stack over the total, and declines wholesale under WLTKD/disorder ([economy.md](../reference/economy.md)).
    ⛔ **`MAINTENANCE_CORPORATION` is NOT one of them** — corporate maintenance is its own pre-inflation expense
    beside total maintenance, so the city total SKIPS that kind. Its deposit is a city-scope FLAT and therefore
    sits in the city's package like any other: a read that folded every maintenance kind would charge the same
    corporate gold twice in one expense total, plausibly and silently. So the Σ asks the CITY for its realized value —
    which is what "the Σ of its members' REALIZED values" already says — and the oracle recomputes both halves
    from source rather than reading either off the stored surface.
    ⚠ **A receiver read is therefore NOT interchangeable with a rolled-legs read on the same channel.** The
    cross-scope roll-up answers a receiver channel with its maintained SUM, so a consumer that wants the
    channel's percent STACK at that scope must read the legs directly — asking the roll-up would hand back the
    realized total instead, silently and plausibly.
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
- **THE TARGET END-STATE — flags all turn, ONE unified rebuild at turn end.** ⚠ This is the NEXT increment, not
  what stands: **today the mark rebuilds immediately** (§ `ensure()` above), so an event does pay its recompute
  mid-turn. The whole model in one line: *"if things have not changed, cache is not stale; if it has, rebuild."*
  Events become pure flag-sets (the DOMAIN-event → markDirty pattern, no mid-turn recompute); reads serve the
  standing snapshot all turn; ONE batched rebuild pass at turn end sweeps every flagged cache **in dependency
  order** (plot caches → city components → player aggregates), priming the next cycle. The load drain built for
  the bracket is the same pass at a different cadence, so the batch lands as a re-cadencing rather than new
  machinery. Consequences: no lazy-refresh reentrancy, no mid-turn freshness questions, rebuild cost is
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
