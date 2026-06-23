# Logging — the observability surface

> The **goal** is *Orwellian* total-surveillance observability (§1); the doc is just **logging** now (all the old
> logging is already nuked, so there is only one logging system to name).

The **verification substrate** for the whole rework. The load-bearing rule is **map-before-delete**: you cannot
safely delete a legacy maintainer you cannot fully observe, so every behaviour is **shadowed** against the live
engine until clean, *then* cut. That demands reconstructing game state from the wire — never the screen. This doc
is that surface, plus the **event spine** that feeds the machines and the logging.

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

## 4. The event spine & the `IEventConsumer` contract

The machines and the logging are fed by one dispatch primitive — the **event spine**: a caller `emit`s an event,
and every consumer that registered interest in that event's KIND gets it. KIND is declared at the call site,
never inferred.

**KIND is the OOS firewall** (Civ4 multiplayer is deterministic lockstep — an authoritative count that differs
per machine is a desync):

| KIND | meaning | synced? | consumed by |
|---|---|---|---|
| **`DOMAIN`** | game **state** changed (building built, unit created, tech researched) | yes — deterministic | the **tally** (gate-eligible) + logging + grants |
| **`DIAGNOSTIC`** | **code** ran (a function entered, a decision re-evaluated) | no — execution trace | **logging only** — never counted, never gates |
| **`TRACE`** | fine-grained "every step" | no | logging only |

So only `DOMAIN` events feed the authoritative [tally](tally.md) and the gate. The payload is **raw** (typed
fields, never a pre-formatted string) so the costly index→text formatting defers to the gated logging consumer —
when a gate is off, nothing expensive ran.

Consumers attach through **one C++03 interface, `IEventConsumer`** (a pure-virtual base, no data members) — the
spine, tally, `grants`, and logging are independent implementations pluggable behind it (the realized exemplar of
the project's interface-contract pattern). Build order: **spine + accumulator → logging (broad) → tally
(selective, `DOMAIN`-only) → grants → modifier → enabler**.

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

## 6. The shadow — how a maintainer is proven before it's cut

Each legacy behaviour gets a **shadow**: a `/diagnostic` surface (and per-turn `[TAG]` line) that computes the
**cascade's** answer and diffs it against the **live engine's**, turn over turn, per scope-instance, decomposed
to named sources. The legacy stays authoritative until its shadow is clean; then it is cut at an **atomic**
cutover, never piecemeal.

- **Attribute, never guess.** A divergence is mapped to a **named source with numbers** (emit the full
  decomposition both sides); if the data to attribute it isn't emitted, the first step is to emit it.
- **The bar is parity-*adjacent*, not parity.** The cascade deliberately corrects latent legacy bugs, so the end
  state is "close, same ballpark," not byte-identical — and ±10% is **not** "adjacent," the bar is sharper. A
  six-rung **care scale** (Fine · Rounding · Better · Weird · Bug · Meltdown) dispositions each surviving
  divergence; the **owner** assigns the verdict — the shadow surfaces facts, it does not self-judge.

---

## See also
- [http-endpoints.md](http-endpoints.md) — the clean endpoint catalogue (the `/diagnostic/*`, `/players`,
  `/cities`, `/units`, `/events` structure) this surface publishes through.
- [tally.md](tally.md) — the first authoritative `DOMAIN` consumer; its `DOMAIN`-only interest *is* the firewall
  in action, and its rebuild-on-load complements the event stream.
- [enabler.md](enabler.md) / [modifier.md](modifier.md) — the behaviours that get shadowed before their legacy
  maintainers are cut.
