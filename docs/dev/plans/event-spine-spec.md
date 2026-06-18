# Event spine — spec (#430 cascade; design session 2026-06-17)

**Status: design-converged (owner thinking session 2026-06-17), pre-implementation.** The event spine is the
**front door** that sits in front of the tally (and the modifier's `grants`, and logging). Callers `emit` events
to it; consumers read the kinds they care about. This doc captures the converged model so we build against it.

Companion to `cascade-engine-430.md` (the engine plan), `tally-cascade-spec.md` (the count machine — the spine's
first authoritative consumer), and `modifier-cascade-spec.md` (§7 — the event-hook system that fires `grants` +
maintains the tally; the spine *is* that system, named).

---

## 1. The shape (one front door, explicit kind, selective consumers)

```
  caller ──emit(KIND, type, raw payload)──▶  EVENT SPINE  ──▶ consumers read the kinds they care about
                                                               • TALLY    : SELECTIVE — domain/state kinds → counts (authoritative)
                                                               • LOGGING  : BROAD     — sees all, OUTPUTS per the logging gates
                                                               • GRANTS   : the kinds that fire provisions
```

- **The spine is upstream of the tally.** The tally is a pure *consumer* — it reads the event kinds it cares about
  and never reaches into game state itself; the events come to it (resolves enabler-spec "the have-builder reports
  to the tally" — the report *is* an emitted event).
- **KIND is declared at the call site, never inferred** (the same "explicit, never infer" rule the cascade atoms
  follow). That one declaration does three jobs: routes the event, enforces the OOS firewall (§2), and tells each
  consumer whether it cares.
- **Two appetites, not two spines:** **logging is BROAD** (can see everything, outputs subject to the existing
  logging gates — the comprehensive observer); the **tally is SELECTIVE** (takes only the domain kinds it counts,
  ignores the rest). One front door; the consumers differ in what they take.

## 2. The OOS firewall — KIND splits synced from unsynced

The tally is **authoritative** (it gates what is buildable — `allowed` caps, `requires` count-thresholds), so in
multiplayer a tally computed differently/stale on one machine is a **desync**. The firewall keeps that safe:

| KIND | meaning | synced? | who consumes |
|---|---|---|---|
| **DOMAIN** | game STATE changed (building built, unit created, tech researched) | **yes — deterministic** | TALLY counts it (gate-eligible) + logging + grants |
| **DIAGNOSTIC** | CODE ran (a function entered, a decision re-evaluated N times) | **no — execution trace, differs per machine** | logging only (count for anomaly/threshold) — **never gates, never counted into the authoritative tally** |
| **TRACE** | fine-grained "show me every step" | no | logging only; **the tally ignores it entirely** |

**The bright line:** only `DOMAIN` events feed the authoritative tally / can gate. `DIAGNOSTIC`/`TRACE` are
unsynced and may be counted *for logging policy* but never cross into game state. The KIND is declared, so this is
enforced at the call site, not sniffed.

## 3. The Event — raw payload, never a pre-formatted string

```
Event = { KIND, type, <raw payload fields> }
```

- **`type`** — the data Type index (building/unit/…); the KIND axis stays tiny (the firewall axis), `type` carries
  the specificity.
- **payload = RAW fields** (ints, type-indices, plot ids, a damage value) — **never a pre-formatted string.** The
  costly part of logging is resolving indices to text + composing the line; keeping the payload raw defers all of
  that to the consumer. Both consumers take what they need from the same raw payload (the tally reads the count
  key; logging renders the fields).
- ⚑ Exact payload representation (a small self-describing field set vs a tagged union vs per-kind structs) pins at
  implementation — the rule is only: **raw, self-describing, no strings.**

## 4. NO verbose `if(loglevel)` gates (owner 2026-06-17) — and why they vanish

Today's scattered `if (gLogLevel >= N) { buildString(); log(); }` exists for ONE reason: to stop expensive
**string-building** from running when the gate is off. In the spine model the call site builds **no string** — it
emits raw fields in **one clean line** (`emit(KIND, type, a, b)`), and all formatting happens downstream **in the
gated logging consumer**. With nothing expensive at the call site, **there is nothing to guard** — the
`if(loglevel)` disappears *structurally*, not by discipline. That is the win: clean one-line call sites, no
boilerplate, and zero wasted formatting (it never runs unless a gate is on).

**Cost when gates are off:** building the cheap raw payload + a cheap "is anyone listening?" interest-check inside
`emit`. Domain events always have a listener (the tally needs them) so they always flow — cheap, and required
anyway. Diagnostic/trace have no tally interest, so they format only when their logging gate is on. The interest-
check is a precomputed per-KIND bool, so a dormant trace point costs ~a function call + a bool test. **Exception:**
an extreme high-volume trace point that must cost *nothing* when off can wrap a thin guard macro — the rare case,
not the rule. (⚑ verify VC7.1 variadic-macro support if we ever need that macro.)

## 5. Logging = superset of today, validated by shadow; trace stays a tier

- The spine-driven logging reproduces **every field the current channels emit** (`[WAI]`/`[CIT]`/`[DAI]`/`[HAI]`/
  `[UNT]`/`[PERF]`) **plus** what only the tally adds (per-event counts, per-turn frequency, threshold/anomaly
  flags). It is a **superset**.
- **TRACE remains its own tier** — a kind the logging consumer emits (gated) and the tally ignores; the deep
  "show me everything" capability is preserved, never routed into counting.
- ⚑ Pre-work: catalog the existing channels' fields so the payloads reproduce them exactly (then the line-diff can
  prove parity).

## 6. C++03 / VC7.1 — no lambdas, no Boost

- **No lambdas** (C++11). Consumers attach via a **virtual interface** `IEventConsumer { virtual void onEvent(const Event&) = 0; }`
  — interface-bounded (the north-star style), stateful (state lives in the consumer), pure C++03.
- **No Boost** (owner 2026-06-17 — avoid `boost::function`/`bind` until we have a handle on it). The spine is plain
  STL: `std::vector<IEventConsumer*>` for the registered consumers, the `CvScopedAccumulator` (`<map>`) for counts,
  a POD-ish `Event`. The substrate primitive (`Sources/Cascade/CvScopedAccumulator.h`) is already Boost-free.
  - **Footgun (do not trip):** the PCH (`CvGameCoreDLL.h`) pulls Boost in transitively and puts some names at
    global scope — a bare `bind` once resolved to `boost::bind` instead of winsock's (`CvHttpServer` lesson). We
    can't stop the PCH; we just never *name* Boost types and avoid generic identifiers like `bind`/`function`.

## 7. Shadow discipline — "do not break all the things" (owner 2026-06-17)

- **Old logging + old counters (`m_pai*Count`) stay live and untouched** as ground truth, gated off in normal play.
- The spine emits a **superset alongside** them; consumers migrate **incrementally**, one channel/call-site at a
  time, each **diffed** against the old (field-diff for logging, count-diff for the tally — the same shadow-verify
  the #195 enabler index used via gated `[PERF]`).
- The old machinery is removed **only at the atomic cutover** (the §4/§14 demolition step), never piecemeal during
  shadow. No big-bang.

## 8. Build order + consumers

The spine is the foundational piece **in front of** the tally, so it comes first among the consumers' shared
machinery (it + `CvScopedAccumulator` are the substrate). Then:

1. **spine** + `CvScopedAccumulator` — **DONE (slice 1, 2026-06-17):** `Sources/Cascade/CvEventSpine.{h,cpp}`
   (`EventKind`/`CvCascadeEvent`/`IEventConsumer`/`CvEventSpine` + `eventSpine()`), pure C++03/STL, allocation-free
   hot path + interest-guard. First consumer = the broad **logging consumer** (Cascade.log + the live `/events`
   tee). Proof emit in `CvGame::doTurn`. Compiles + links (Assert).
2. **tally** — first authoritative consumer; domain events → counts; shadow-diff vs `m_pai*Count`.
3. **logging** consumer — broad/gated; reproduces channel fields + counts; shadow-diff vs the old lines.
4. **grants** — fires provisions on its kinds.
5. **modifier**, then **enabler** (read the tally) — per `cascade-engine-430.md`.

## 9a. Observability — `CvHttpServer` is the FORMAL live layer (owner 2026-06-17)

`CvHttpServer` is **not an experimental bolt-on** — it is the cascade's formal **live-observability / query layer**,
and consumers publish to it. (Full endpoint/gating/architecture reference:
[`../reference/http-server.md`](../reference/http-server.md).)

- **Assessed: it needs NO redesign.** It already has the right architecture for this role — publish-and-serve with
  **snapshot isolation** (the server thread never touches game objects), a **bounded** event queue
  (`EVENT_QUEUE_CAP = 2048`, ~few-hundred-KB transient, and it **drains even with no client** so it can't bloat),
  the `/events` SSE stream, and the **#419 live-log mechanism** (raw gated log lines teed onto `/events` for
  out-of-process parsing — "the counter-strike way"). The owner's original cost worry (CPU/MAF/bloat) proved
  unfounded; it's already 32-bit-safe. So we **extend + formalize**, not rebuild.
- **The spine streams through the shared `streamLogTee`** (`BetterBTSAI.{h,cpp}`, promoted from file-local `static`
  to a shared public tee 2026-06-17): the BBAI log helpers AND the spine's logging consumer both call it, so a log
  line goes to the live `/events` stream via one canonical path (gated by `gStreamLogLevel`). Post-cutover the spine
  is the central logging path and that tee lives in one place.
- **The tally will expose a `/tally` snapshot endpoint** the same publish-and-serve way (like `/cities`) when it lands —
  **PLANNED, not yet built** (live routes today are only `/`, `/units`, `/players`, `/cities`, `/events`). Tally DOMAIN
  *emits* are already observable via `/events` (the `[SPINE/DOMAIN]` `log` frames); the missing piece is the snapshot GET
  of current aggregated counts. It must publish from the game thread (never read `cascadeTally()` on the server thread —
  the snapshot-isolation HARD CONSTRAINT). See [`../reference/http-server.md`](../reference/http-server.md).

## 9b. SPEED + the 32-bit ceiling (owner 2026-06-17) — non-negotiable

Speed is a given; the harder constraint is **memory on a 32-bit (LAA ~4GB) process** — avoid bloat / allocation
failure. The design holds to it:

- **Allocation-free hot path:** `CvCascadeEvent` is a small POD passed by const-ref (no heap per emit); the
  interest-guard makes a dormant DIAGNOSTIC/TRACE firehose ≈ one bit-test; the logging consumer formats into a stack
  buffer. No per-event `new`.
- **Bounded observability buffers:** the `/events` queue is capped + self-draining (above). No ever-growing
  in-memory log.
- **Compact counts:** the tally weighs sparse (`std::map`) vs dense (`int[]`) per domain against memory
  (per-node overhead × scopes × players adds up on 32-bit) when it lands — pick per domain.

## 9. Open / flagged

- ⚑ Event payload concrete representation (self-describing field set vs tagged union vs per-kind struct) — §3.
- ⚑ KIND taxonomy beyond the firewall axis (do we need domain sub-kinds, or does `type` carry it?) — lean: keep
  KIND tiny, `type` carries specificity.
- ⚑ Whether `emit` dispatches push (spine → consumer `onEvent`) or consumers pull a queue — lean push (direct
  virtual call), simplest; revisit only if a queued/deferred pass is needed (e.g. the future parallel read).
- ⚑ The catalog of existing logging-channel fields (the §5 superset target).
- ⚑ VC7.1 variadic-macro support, only if the §4 extreme-firehose guard macro is ever needed.
