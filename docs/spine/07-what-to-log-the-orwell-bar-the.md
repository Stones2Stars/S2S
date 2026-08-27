# 7. What to log — the Orwell bar, the scale, and the three hook shapes

> Part of the **[spine](../spine.md)** spec.

The **observability surface** for the whole cascade rework is *what the game exposes*. It is not polish: without
total observability the cascade ([enabler](../specs/enabler.md)/[modifier](../cascade.md)/[tally](../specs/tally.md))
cannot prove it replicates the legacy machinery it replaces — so it cannot safely replace it.

### The reconstruction bar ("Orwell")

> **Reconstruct full game state from the HTTP endpoints + the `/events` stream + the gated logs ALONE — never by
> looking at the screen.**

The canonical test is an **AI-only autoplay**: with no human and no UI in the loop, every piece of state the AI
acts on *must* be readable from the wire, or it is invisible — which is exactly the bar. An AI-player read is
also a *purer* cascade-vs-engine comparison (no UI-display artifacts).

### The observability scale (0–5)

A system is rated on how deeply it can be observed: **0 Oblivious · 1 Telescreen · 2 Informant · 3 Big Brother ·
4 Thought Police · 5 Meta.** Most game systems sit at Tier 1 (a coarse snapshot, no *why*); climbing means adding
hooks (below) that expose the decomposition behind a number. Rate two axes **separately**: the
cascade/buildability surface, and the whole-game-state surface — a high score on one says nothing about the other.

### The three hook shapes

Every observability hook is one of these — cheap, gated, **off by default**:

1. **Snapshot field** — a read-only field on a served snapshot document (a game-thread copy).
2. **Gated `[TAG]` log line** — emitted under a log-level gate (`gPlayerLogLevel`/`gCityLogLevel`/…) and teed to
   `/events` so it streams live.
3. **Mailbox snapshot endpoint** — an on-demand snapshot computed on the game thread via the single-slot mailbox,
   depending on **no** log file or gate.

The HTTP transport, its standing invariants, and the routes that exist today are
[http-endpoints.md](../specs/http-endpoints.md). ⚠ There is **no route catalogue** — the route table was purged and a
route is defined with the access surface it serves, so shapes 1 and 3 stay sparse by design rather than growing
a registry of their own.

Logging is one **`IEventConsumer`** behind the spine (§2) — so are grants and the `/events` stream; it does not
own the dispatch. It is the **broad** FILE consumer: it takes `DOMAIN`, `SAVELOAD`, `DIAGNOSTIC`, and `TRACE`
events and formats the raw typed payload to text **only when its gate is on** (an off gate costs nothing).

