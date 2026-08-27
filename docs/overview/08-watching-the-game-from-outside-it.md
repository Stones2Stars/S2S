# 8. Watching the game from outside it

> Part of the **[overview](../overview.md)** spec.

→ [specs/http-endpoints.md](../specs/http-endpoints.md)

> Reconstruct full game state from the endpoints, the event stream and the logs alone — never by looking at the
> screen.

This was a precondition, not a nicety. You cannot safely delete a maintainer you cannot observe, because you
cannot prove a replacement does the same job. Every deletion was paid for by an instrument built first.

- **Log files** — spine-written domains render on the game thread and hand off to a writer thread, so they are
  *readable while the game runs*. This is the one that captures the whole load reseed.
- **A live event stream** — `/events`, server-sent, on `127.0.0.1:7227`. State facts stream unconditionally;
  diagnostics ride their own knob. Bounded slots, and a dropped frame is reported as a gap.
- **On-demand snapshots** — computed on the game thread through a single-slot mailbox, depending on no log file
  and no gate.

### Why the route table is nearly empty

A legacy data member whose only remaining reader is an HTTP route is not actually still used — but the compiler
census cannot tell the difference. It survives self-referentially: the member exists because the route reads it,
and the route exists to read the member. A route is the perfect hiding place for exactly the legacy this rebuild
removes, and it hides it from the one census we trust.

So the surface stays sparse, and what it serves is **decompositions**, never totals — a city's yield rate
published as the six independent quantities that collapsed into it, with the refused deposits listed beside the
atom that refused each one.

---

