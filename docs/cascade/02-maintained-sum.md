# The maintained sum — a package is never dirtied and recalculated

> Part of the **[cascade](../cascade.md)** spec.

> **⚖ THE FOUNDING CORRECTION — A PACKAGE IS NEVER DIRTIED AND RECALCULATED. IT IS A COMPILED SUM THAT
> IS ALWAYS CURRENT, BECAUSE EVERY EVENT THAT MOVES IT UPDATES IT.** *"What I got wrong is that I thought the
> yield packages had to be marked, and recalculated all the time, when it is in essence just a compiled sum
> that is always updated, based on incoming spine events."*
>
> A package slot is Σ over the scope's sources of their deposit into that `(channel, unit)`. A DOMAIN event
> NAMES the source, and the compiled index already holds that source's deposits — so applying them is a handful
> of adds and the slot is correct **at that instant**. There is nothing to mark, because there is nothing
> deferred. The staleness-flag / recompute protocol this section once specified is RETIRED
> ([superseded-ideas](../architecture/superseded-ideas.md) #30); what stands in its place is § THE MAINTAINED SUM
> below.

This is the **design the cascade plane is built to**, stated independently of any one implementation of it. The ONE
uniform package (`Sources/Cascade/CvCascadePackage.h`, channel-indexed Σflat (×100) / Σpercent (unscaled) slots)
is a data member on team / player / city / plot; the per-scope channel sets are minted from the
compiled deposits at load (`CvCascadeChannelRegistry`, the ClassificationRegistry precedent); the package carries
**apply verbs and no other writer**, so the modifier's own spine consumer (`CvModifierConsumer`, load-active)
applies a moved source's deposits directly into the slots they feed; and the combine lives on the calc surface
(`InfoValuation::cityRate` / `groupSumAt`).

### The problem: no unified `dataChanged` trigger

Every derived value in the legacy engine is a **hand-maintained cache with ad-hoc, gappy invalidation**. There is no
single "the source changed, refresh me" primitive, so caches drift out of sync with the data they derive from — one
disease, many instances: a dormant building's improvement-yield never decremented; a building value change leaving
the cache on the old value; transition-only stamps (`doVicinityBonus`) missing build-after-connect orders; two
surfaces reporting different worked-plot yields for the same city at the same moment. The legacy incremental
serialized accumulators additionally carry **history pollution** — values no live data source can produce (the
improvement-yield accumulators hold phantom yields, per-plot, bit-exact; the wellbeing accumulators the same class —
§2b). Recompute-from-source is the cure; recompute-every-read getters (the squirrelBanana class) were the
workaround for a cache nobody could trust — correct, but paying full cost on the hot path.

### The model

> **the state changes → the fact is emitted → the fact's own source updates the slot it feeds → every consumer
> reads that one value as a bare fetch.** One announcement, one application, one source of truth.

A derived cache in this model is:

1. **EVENT-MAINTAINED, not mark-driven.** The DOMAIN fact names the SOURCE; the compiled index names that
   source's deposits; applying them IS the maintenance. A READ is a **bare fetch** and never recomputes
   (an ensure-on-read protocol is tombstoned), and there is no staleness flag on the unconditioned plane to gate
   anything on. **The work is proportional to what CHANGED (one source's handful of deposits), never to what
   EXISTS (every source at the scope).**
2. **Recompute-only, NOT serialized** — the [derived data is never trusted from a save](../specs/save.md#5-derived-data-serializes-nothing-) rule,
   applied per-field. Neither the value nor the flag is saved; on load the flag is marked by default, so the first read
   recomputes from current state — **never stale-from-save**. Drop serialization by the **soft-remove**
   ([the soft-remove save discipline](../specs/save.md#3-removing-a-serialized-field--the-soft-remove-via-assetssavemigrationtxt-), [save.md §3](../specs/save.md)): FULL-DELETE the
   read + write and NAME the tag in `Assets/savemigration.txt`, which drains an old save's orphan bytes by name so
   nothing after it shifts (a no-op on a new save that never wrote it). **No `WRAPPER_SKIP_ELEMENT`** (it leaves the
   dead member named — a rollerskate target); and just deleting the read/write *without* the `savemigration.txt` entry
   desyncs the whole downstream read.
   **This is UNIVERSAL, not per-field-optional: NO cache is ever serialized** — so nothing derived
   is ever read from a save, and there is correspondingly nothing for a blanket recompute to purge.
   ⛔ **No blanket recompute of derived state exists anywhere in the engine, and none is ever to be built**
   ([self-heal is not a backstop](03-no-staleness-no-selfheal.md#-a-self-heal-is-the-fossil-of-a-missing-emit--so-it-is-a-search-not-just-a-ban)): the event spine builds the state, LOAD is the only
   full pass (§ THE CAPSTONE RULE, below), and a missed invalidation must stay visible instead of being swept away. ⛔ A
   wipe-the-totals-and-reapply pass over live game objects is therefore never a maintenance path to add or extend
   — *"it is inherently obsolete under the event-driven system, since the new system recalcs on load anyway"*
. It is the exact shape this model replaces, and it is worst where it looks most useful: firing on the
   saves most likely to have drifted is what would hide the missed emits the spine exists to expose. Each
   remaining serialized cache converts by the same move: skip the read, rebuild at load from source state through
   the live entry points (the bonus-network cluster — the plot-group counts AND membership, the bonus-fed
   wellbeing/modifier accumulators, power, the dormancy verdicts — is the realized exemplar: the load-end rebuild
   in `CvGame::onFinalInitialized` recolors the groups from current state, folds the counts as each plot joins,
   and reconciles dormancy to the enabler's operate fixpoint, firing the ordinary crossing emits; the city holds
   no bonus mirror at all — its read is a plot-group relay, [enabler.md §8](../specs/enabler.md)). A serialized
   store survives ONLY for genuine non-derivable state (the event/WB grant stores, e.g.
   `CvCity::m_paiFreeBonusEvents`).
3. **The single source — PULL, not push, and the rule is CROSS-SCOPE.** ⛔ **What is banned is a scope pushing
   its total into ANOTHER scope's store**: an upper scope's package never lands in a lower one, and a receiver
   total is the Σ of its MEMBERS' realized values read at the receiver, never deltas the members push upward.
   That is where "push + a parallel cache double-count and drift" actually bites — two stores holding one fact,
   one of them maintained by someone else.
   ⚑ **A deposit landing in its OWN scope's slot is NOT that, and reading it as that is the misreading this
   clause exists to prevent.** By the scope principle (§1) a city-scope deposit BELONGS at city scope; the
   package is the only thing holding it, so there is no parallel cache to diverge from and nothing to
   double-count. Applying the fact to the slot the fact feeds is the maintenance, not a push.

**Worked shape (the plot-yield cache):** `getYield()` = `return cached` — a bare fetch, always O(1), because the
fact that moved the plot already applied its deposits into the slot;
**⚖ A CROSS-SCOPE RECEIVER TOTAL IS RE-SUMMED AT READ, AND NO SLOT HOLDS IT.** *"I believe it will cost
more to cache such a number in most cases than it would to just do the sum of all cities."* Each channel has ONE
consuming scope (production → city; the commerces further up), and where that scope is ABOVE its members the
total is the Σ of their realized values, taken at the read — there is no `sum` slot, no `readSum`, and no
`applySum`, and none is to be built.
⚑ **The arithmetic is why, not thrift:** a member's realized value is the §2a combine, which is NOT linear in the
deposits, so a cached total could not be moved by a deposit delta at all — it would have to be re-derived on
every fact that touches any member, which is strictly more work than summing the members when someone asks.
⚖ **THE THRESHOLD, so this is re-derivable rather than remembered: *"I do not think for a million years
it would ever be worth caching a value that loops X cities for 1 number and sums it, unless the number of cities
is in the thousands."*** An empire holds tens of cities, so the Σ is tens of adds over values each member already
holds. ⛔ That bar is nowhere near met, and it applies to a HAND-ROLLED bank of the same number just as much as
to a package slot — caching it anywhere is the move being refused, not merely caching it in the cascade.
⚑ **And the VOLATILITY settles it independently of the count: *"especially a number like a commerce
yield, that pretty much constantly fluctuates."*** A cache pays off in proportion to how long an entry stays
valid; a commerce yield moves on nearly every fact in the economy, so a stored total would be re-derived about
as often as it is read and would spend the rest of its life WRONG. ⇒ The two tests compose: cache-worthiness
needs both a large member count and a stable value, and a receiver total has neither. ⚠ A staler variant — a
once-per-turn snapshot of the same Σ — is the worse answer, not the safer one: it trades the cost for a value
that is knowingly out of date on a number that never stops moving.
⛔ So the cost of a receiver read is the MEMBER COUNT, and that is accepted. What is NOT accepted is asking it
per candidate in a scoring loop ([patterns.md](../architecture/patterns.md) § THE VALUATION PROTOCOL: a how-valuable
weight is asked at most once per turn) — the cadence is the defect, never the Σ.

⛔ **What is banned is a HAND-NAMED field holding that same number** — a `CvCity::m_plotYieldSum`-shaped member is
the defect [every derived cache is one shape](04-derived-stores.md#-every-derived-store-is-one-shape--a-keyed-accumulator-maintained-by-a-delta) names (it cannot be addressed by the
derived mask, so it forces a bespoke invalidation path) and a second maintenance surface for a fact the modifier
consumer already routes. ⛔ Equally banned is the other direction: re-summing per read. **Cache it — in the slot
that already exists.** The push-maintained `m_aiBaseYieldRate` is dead, and a legacy tier-1 accessor over it
(`getPlotYield`) is a DELETION, not a value to re-home: its consumers read the channel at its receiving scope
([build a new getter surface, never widen a legacy one](../architecture/patterns/05-the-two-read-roles-one-grammar-two.md#-the-two-read-roles--one-grammar-two-answers)). ⛔ The pull must be a CACHE at EVERY level, never a per-read walk: re-summing the radius on every
`getPlotYield` call turns the game's hottest read O(radius) — measured at 913M plot reads in one turn inside the
governor's valuation, the cost class this whole doc exists to prevent. The engine's actual base yield thereby equals the build-order-independent value the cascade computes —
stale-cache divergences resolved **at the source**, behaviour-preserving
([the completeness+attribution bar](../specs/validation.md#the-observation-surface)).

