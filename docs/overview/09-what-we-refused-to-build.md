# 9. What we refused to build

> Part of the **[overview](../overview.md)** spec.

**Self-healing recalculation.** These never appear because someone wanted a recalculation; they appear because a
fact was not announced, the value went wrong, and recomputing was the cheapest way to stop the symptom. **Every
self-heal marks the spot where an emit is missing** — so finding one is a search, not a deletion. It is worse
than the bug it hides: the missed emit would have surfaced as a visibly wrong value somebody could chase; the
recalculation converts it into permanent invisible drift *and* reinstates the work the caches exist to delete.

**The word "dirty."** Removed with the mechanism it names, except the graphics repaint bits the executable
requires. A term that survives its mechanism teaches the next contributor to reach for it. Same for calling a
package read "hot" — a read can only be hot if reading does work, so the word smuggles the recompute model back
in over code that has none.

**Divergence as an event.** An event is an invitation to a consumer, so the next person writes the consumer that
*handles* a value known to be wrong by correcting it — self-heal wearing the authority of the spine. Anything
that recomputes to check must be structurally unable to write back what it computed.

**The in-engine profiler.** The macro family behaves differently per build configuration and some call sites
bypass every scope gate, including a per-frame one. Compiling it into a release build ran those ungated, per
frame, with a critical section per call; it crashed on end-turn and was reverted the same day. What replaced it
is a gated per-turn census teed to the event stream.

**The one cache we kept on purpose.**

> We should have some pathfinding cache, because it is the most expensive, and at the same time unmaintainable
> thing we can do — it has to scan plots by its very definition.

A path is not a sum over sources, so there is no delta to apply, and it moves non-locally: one terrain change
re-routes paths that never touch the changed tile, so no fact can name what it invalidated. That is structural,
not an exemption — and being unmaintainable-by-delta means such a cache is *cleared*, wholesale, by the events
that can move it.

---

