# Logging — the observability surface

> The **goal** is *Orwellian* total-surveillance observability (§1); the doc is just **logging** now (all the old
> logging is already nuked, so there is only one logging system to name).

The **observability surface** for the whole rework — *what the game exposes*. The load-bearing rationale is
**map-before-delete**: you cannot safely delete a legacy maintainer you cannot fully observe, so the game must be
fully observable from the wire — never the screen. This doc specs **what to log**; the **event spine** it draws
events from is [event-spine.md](event-spine.md), and how the cascade is **measured against legacy** (the shadow +
the parity bar) is [validation.md](validation.md).

It is not polish. Without total observability the cascade ([enabler](enabler.md)/[modifier](modifier.md)/
[tally](tally.md)) cannot prove it replicates the legacy machinery it replaces — so it cannot safely replace it.

---

## 1. The reconstruction bar ("Orwell")

> **Reconstruct full game state from the HTTP endpoints + the `/events` stream + the gated logs ALONE — never by
> looking at the screen.**

The canonical test is an **AI-only autoplay**: with no human and no UI in the loop, every piece of state the AI
acts on *must* be readable from the wire, or it is invisible — which is exactly the bar. An AI-player read is
also a *purer* cascade-vs-engine comparison (no UI-display artifacts).

---

## 2. The observability scale (0–5)

A system is rated on how deeply it can be observed: **0 Oblivious · 1 Telescreen · 2 Informant · 3 Big Brother ·
4 Thought Police · 5 Meta.** Most game systems sit at Tier 1 (a coarse snapshot, no *why*); climbing means adding
hooks (§3) that expose the decomposition behind a number. Rate two axes **separately**: the cascade/buildability
surface, and the whole-game-state surface — a high score on one says nothing about the other.

---

## 3. The three hook shapes

Every observability hook is one of these — cheap, gated, **off by default**:

1. **Snapshot field** — a read-only field on the `/players` | `/cities` | `/units` snapshot (a game-thread copy).
2. **Gated `[TAG]` log line** — emitted under a log-level gate (`gPlayerLogLevel`/`gCityLogLevel`/…) and teed to
   `/events` so it streams live.
3. **Mailbox `/diagnostic/*` endpoint** — an on-demand snapshot computed on the game thread (the
   `/diagnostic/sweep` pattern), depending on **no** log file or gate.

The endpoint *catalogue* + the clean **route structure** is [http-endpoints.md](http-endpoints.md) — it is
canonical and **supersedes the illustrative legacy names above**: `/players`/`/cities`/`/units` → `/state/*`;
`/diagnostic/*` → `/shadow/*` + `/decompose/*` + `/extractor/*`.

---

## 4. Logging is a consumer of the event spine

Logging does **not** own the dispatch — it is one **`IEventConsumer`** behind the **[event spine](event-spine.md)**
(so are the tally and grants). Logging is the **broad** consumer: it takes `DOMAIN`, `DIAGNOSTIC`, and `TRACE`
events and formats the raw typed payload to text **only when its gate is on** (an off gate costs nothing), teeing
to `/events`. The spine itself — the KIND firewall (`DOMAIN`/`DIAGNOSTIC`/`TRACE`), the `IEventConsumer` contract,
the C++ shape — is specced in [event-spine.md](event-spine.md).

---

## 5. Reading the live surface — the rules

> **The running game holds its `.log` files OPEN — never live-read them.** Tailing `Cascade.log` mid-session
> gives stale/empty/partial results; do not infer "logging is off" from a quiet log file.

The two reliable live reads:

- **`/diagnostic/*` endpoints** — an on-demand snapshot via the game-thread mailbox; depends on no log file and
  no gate. The most reliable read — when in doubt about a value, hit the endpoint.
- **`/events` SSE stream** — the gated `[TAG]` lines, live. The per-turn shadow lines burst at the **top of
  `doTurn`**, so you must **connect *before* the turn ticks** (connect-then-end-turn).

> **Delegate bulk reads to the cheap `data-reader` sub-agent.** A sweep dump is tens of KB; pulling it raw into
> an expensive (orchestrator) context burns budget for nothing. The reader curls/greps, aggregates, and returns a
> compact distilled summary (histograms, cause-tags, anomalies). It must fail **honestly** (distinguish
> "surface down" from "reader error", never fabricate a clean summary).

---

## See also
- [event-spine.md](event-spine.md) — the event source logging consumes. [validation.md](validation.md) — the shadow
  + parity bar that *uses* this observability to prove a maintainer before it's cut.
- [http-endpoints.md](http-endpoints.md) — the clean endpoint catalogue (`/state`, `/extractor`, `/shadow`,
  `/decompose`, `/events`) this surface publishes through.
- [tally.md](tally.md) — the first authoritative `DOMAIN` consumer; its `DOMAIN`-only interest *is* the KIND firewall in action.
