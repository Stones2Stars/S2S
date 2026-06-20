# The event spine — the cascade's front door & the `IEventConsumer` contract

> **Status:** reference (the spine is REALIZED; consumers at varying maturity) · **Verified against:**
> `Sources/Cascade/CvEventSpine.{h,cpp}`, `Sources/Cascade/CvCascadeTally.{h,cpp}` (2026-06-20); design
> reconciled with old `docs/dev/plans/event-spine-spec.md`. · **Grounding:** every behavioural claim carries a
> `file:line` to live source. Line numbers **drift** — confirm the named function/symbol, not the integer.
>
> The event spine is the **front door** in front of the tally, `grants`, and logging. Callers `emit` events;
> consumers read the kinds they care about. This doc is the DEEP reference: the `IEventConsumer` contract shape,
> the OOS-firewall KIND axis, the raw-field payload, the consumers and build order, and the on-delivery model.
> The one-paragraph "how it fits the machines" lives in the overview
> ([cascade-architecture §5](../../explanation/cascade-architecture.md)); this is the detail behind it.

**BLUF.** The spine is one dispatch primitive: `eventSpine().emit(KIND, …)` and every consumer that registered
interest in that KIND gets `onEvent`. **KIND is the OOS firewall** — `DOMAIN` (synced state change) is the only
kind that may feed the authoritative tally and gate; `DIAGNOSTIC`/`TRACE` are unsynced and go to **logging only**.
KIND is declared at the call site, never inferred. The payload is **raw** (typed-field slots, never a
pre-formatted string) so the costly index→text formatting defers to the gated logging consumer. Consumers attach
through one contract, **`IEventConsumer`** — the spine, tally, `grants`, and logging are independent
implementations pluggable behind it, the realized exemplar of the project's
[composability](../../architecture/patterns/composability.md) /
[faking-DI](../../architecture/patterns/faking-di.md) patterns
([DEC-interface-contracts](../../architecture/decisions.md#dec-interface-contracts)). Pure C++03/STL, allocation-
free hot path, with a cheap interest-guard so a dormant trace firehose costs ~nothing.

---

## 1. The shape — one front door, explicit KIND, selective consumers

```
  caller ──emit(KIND, type, raw payload)──▶  EVENT SPINE  ──▶ consumers read the kinds they care about
                                                               • TALLY    : SELECTIVE — DOMAIN kinds → counts (authoritative)
                                                               • LOGGING  : BROAD     — sees all, OUTPUTS per the logging gates
                                                               • GRANTS   : the kinds that fire provisions
```

- **The spine is upstream of the tally.** The tally is a pure *consumer* — it reads the kinds it cares about and
  never reaches into game state itself; the events come to it. (This is the engine realization of "the
  have-builder reports to the tally" — the report *is* an emitted event.)
- **KIND is declared at the call site, never inferred** — the same "explicit, never infer" rule the cascade
  atoms follow. That one declaration does three jobs: routes the event, enforces the OOS firewall (§2), and tells
  each consumer whether it cares.
- **Two appetites, one spine.** Logging is **broad** (sees everything, outputs subject to the existing log
  gates); the tally is **selective** (takes only the DOMAIN kinds it counts). One front door; the consumers
  differ in what they take.

## 2. The OOS firewall — KIND splits synced from unsynced

The tally is **authoritative** (it gates what is buildable — `allowed` caps, `requires` count-thresholds), so a
tally computed differently or stale on one machine is a **desync**. The firewall keeps that safe. The KIND enum
is `EventKind` (`CvEventSpine.h:28`):

| KIND | meaning | synced? | who consumes |
|---|---|---|---|
| **`EVENTKIND_DOMAIN`** | game STATE changed (building built, unit created, tech researched, rename) | **yes — deterministic** | TALLY counts it (gate-eligible) + logging + grants |
| **`EVENTKIND_DIAGNOSTIC`** | CODE ran (a function entered, a decision re-evaluated N times) | **no — execution trace, differs per machine** | logging only — **never gates, never counted into the authoritative tally** |
| **`EVENTKIND_TRACE`** | fine-grained "show me every step" | no | logging only; **the tally ignores it entirely** |

**The bright line:** only `DOMAIN` events feed the authoritative tally / can gate. The tally enforces this
structurally — its `wantedKinds()` returns only `(1 << EVENTKIND_DOMAIN)` (`CvCascadeTally.h:49`), so the spine
never even dispatches a `DIAGNOSTIC`/`TRACE` to it. The KIND is declared at the call site, so the firewall is
enforced there, not sniffed.

## 3. The Event — raw typed-field payload, never a pre-formatted string

The event is a small POD, `CvCascadeEvent` (`CvEventSpine.h:113`). It has **two payload modes, both raw**:

- **DOMAIN count events** use `iType` + `iA`/`iB`/`iC` (e.g. building count: `iType` = `BuildingTypes`, `iB` =
  delta, `iC` = `PlayerTypes` — `CvEventSpine.h:173`). The tally reads these directly.
- **Logging events** (`DIAGNOSTIC`/`TRACE`) use `iDomainTag` + `iEventId` (→ a constant line prefix) +
  `aFields[iFieldCount]` (the variable `name=value` fields).

**Why raw, not a formatted string.** The costly part of logging is resolving indices to text + composing the
line. Keeping the payload raw defers all of that to the gated consumer — so when a gate is off, *nothing
expensive ran*. Each consumer takes what it needs from the same raw payload (the tally reads the count key;
logging renders the fields).

**The field payload** (`CvCascadeEventField`, `CvEventSpine.h:66`): a generic typed slot — `{ int eTag; union{int
i; float f; const char* s; const wchar_t* w;} v; }`, POD/8B on x86. `CvCascadeEvent` carries
`aFields[SPINE_MAX_FIELDS]` with `SPINE_MAX_FIELDS = 16` (the 97th-pct of operational AI lines;
`CvEventSpine.h:108`), plus `iDomainTag` + `iLevel` (the 1–4 surveillance level the line emits at). Fields are
appended with `addI`/`addF`/`addStr`/`addWStr` (`CvEventSpine.h:139-160`).

**Per-domain field resolution — no global registry.** The slot's `eTag` is a **domain-LOCAL** field tag — there
is no global field enum. Each migrated domain registers (via `spineRegisterDomain`, `CvEventSpine.h:105`) its own
`SpineLinePrefixFn` (eventId → constant `[TAG]` prefix), destination `.log` file, AND a `SpineFieldInfoFn`
(local tag → name + `SpineFieldType`). The generic logging consumer renders `prefix + name=value …` via
`cascadeRenderEventLine` (`CvEventSpine.h:166`); `SpineFieldType` typeIndex kinds (`SFT_BUILDING`/`UNIT`/`BONUS`/…,
`CvEventSpine.h:46`) resolve an index to a type name in the consumer. This per-domain isolation (vs a global
typed-tag enum) is the [proper-once](../../architecture/decisions.md#dec-proper-once) choice: **zero shared edits
per domain** ⇒ parallel-safe migration, no 3-way-sync debt. The spine itself never names a domain.

**The composition rule — "the call site never COMPOSES the line," not "no strings"** (owner 2026-06-19). The
invariant is that the line is assembled in **one place, one gate — the consumer**; the call site only hands over
*ingredients*. So passing an **existing string pointer** (`SFT_STR`/`SFT_WSTR` — a literal, or a `szReason`
already built for other logic) is allowed: it is an ingredient, pointer-cheap, lifetime-safe (the consumer
renders **synchronously on the game thread** at emit, so the pointee is valid). What stays banned is the call
site *building* the final formatted line. Type/instance data still travels as a raw id so the consumer renders
`name(id)` (the additive win); only genuinely free-text data uses a string field.

## 4. No verbose `if(loglevel)` gates — and why they vanish

Today's scattered `if (gLogLevel >= N) { buildString(); log(); }` exists for one reason: to stop expensive
string-building when the gate is off. In the spine model the call site builds **no string** — it emits raw fields
in one clean line, and all formatting happens downstream in the gated logging consumer. With nothing expensive at
the call site, **there is nothing to guard** — the `if(loglevel)` disappears *structurally*.

**Cost when gates are off:** the cheap raw-payload build + a "is anyone listening?" **interest-guard** inside
`emit`. The spine holds `m_iInterestMask` = OR of all registered consumers' `wantedKinds()`; `anyInterest(kind)`
is a single bit-test (`CvEventSpine.h:216-220`). DOMAIN events always have a listener (the tally), so they always
flow — cheap and required anyway. A dormant DIAGNOSTIC/TRACE firehose with no logging listener costs ~a function
call + a bool test, never the format.

## 5. The `IEventConsumer` contract — the realized interface exemplar

Consumers attach through **one C++03 virtual interface** (`CvEventSpine.h:197`):

```cpp
class IEventConsumer
{
public:
    virtual ~IEventConsumer() {}
    virtual int  wantedKinds() const = 0;        // bitmask of (1 << EventKind)
    virtual void onEvent(const CvCascadeEvent&) = 0;
};
```

This is the project's interface-contract shape made real (an abstract base, pure-virtuals + virtual dtor, **no
data members** — state lives on the concrete; [composability](../../architecture/patterns/composability.md),
[DEC-interface-contracts](../../architecture/decisions.md#dec-interface-contracts)). `wantedKinds()` is what makes
a consumer selective (the tally) vs broad (logging). No lambdas, no Boost — `std::vector<IEventConsumer*>` holds
the registered consumers; the substrate counts live in `CvScopedAccumulator` (`<map>`). The PCH pulls Boost in
transitively, so the spine deliberately never *names* a Boost type or a generic identifier like `bind`/`function`.

The spine (`CvEventSpine`, `CvEventSpine.h:208`) is the single engine-wide instance `eventSpine()`. `emit`
dispatches to every registered consumer whose `wantedKinds()` matches, and **skips entirely** when no consumer
wants the kind (the interest-guard; `CvEventSpine.cpp` `emit`, dispatch loop at `:50`).

## 6. The consumers & build order

The spine + `CvScopedAccumulator` are the shared substrate, built first; consumers register once at startup via
`cascadeRegisterConsumers()` (idempotent, `CvEventSpine.cpp:293`). The four consumers:

1. **spine** + `CvScopedAccumulator` — **DONE (slice 1).** `Sources/Cascade/CvEventSpine.{h,cpp}`
   (`EventKind` / `CvCascadeEvent` / `IEventConsumer` / `CvEventSpine` + `eventSpine()`), pure C++03/STL,
   allocation-free hot path + interest-guard.
2. **logging** consumer — **broad, registered first** (`CvEventSpine.cpp:301`). `CvCascadeLogConsumer` renders
   raw field events to `Cascade.log` + the live `/events` tee, gated on the 0–5 surveillance level. It resolves
   names **live and synchronously on the game thread** (`onEvent` → `cascadeRenderEventLine`, ~`CvEventSpine.cpp:226`),
   so an id resolves to `name(id)` exactly as-of-emit and touches no live object off-thread. The per-domain
   migration of the legacy `[TAG]` channels (HAI/WAI/CIT/UNT/…) onto raw-field emits is in progress (see the old
   `event-spine-spec` history + `logging-surface-inventory` for the per-channel status).
3. **tally** — the **first SELECTIVE (DOMAIN-only)** consumer (`CvEventSpine.cpp:305`); DOMAIN count events →
   counts, shadow-diffed vs `getXCount`. **PARTIAL — buildings + units, player-leaf** (see [tally.md](tally.md)).
4. **grants** — fires provisions (a free bonus/unit/pulse) **on delivery**, on its kinds. **NOT BUILT** — no
   `grants` consumer is registered today; it is the planned next consumer behind the same `IEventConsumer`
   contract.

Then **modifier**, then **enabler** (which reads the tally), per the engine plan.

**The name-change DOMAIN event.** A rename (player/civ/city/unit) IS an observable state change an out-of-process
observer must see to keep its id→name table accurate, so it is a `DOMAIN` event `CASCADE_EVT_NAME_CHANGE`
emitted from the four set-name choke points via `cascadeEmitNameChange()` (`CvEventSpine.h:192`, `.cpp:308`). The
payload is string-free (`iType` = `NameChangeKind`, `iA` = owner, `iB` = entity id; `CvEventSpine.h:182`); the
logging consumer resolves the NEW name live, the tally **ignores it** (its `switch` default returns;
`iC = 0` keeps the player-slot guard happy). The `kind=civ` emit (from `setCivName`) doubles as a lever for the
long-standing "empire name not refreshing on civic change" bug.

## 7. Shadow discipline — superset, diffed, cut only at cutover

- **Old logging + old counters (`m_pai*Count`) stay live and untouched** as ground truth, gated off in normal
  play.
- The spine emits a **superset alongside** them (every field the current channels emit, plus what only the tally
  adds — per-event counts, frequency, anomaly flags); consumers migrate **incrementally**, one call site at a
  time, each **diffed** against the old (field-diff for logging, count-diff for the tally — the tally's
  `shadowVerify`, [tally §4](tally.md)).
- The old machinery is removed **only at the atomic cutover**, never piecemeal during shadow. No big-bang
  ([DEC-map-before-delete](../../architecture/decisions.md#dec-map-before-delete)).

## 8. Memory & the 32-bit ceiling

Speed is a given; the harder constraint is **memory on a 32-bit (LAA ~4GB) process** — avoid bloat / allocation
failure. The design holds to it:

- **Allocation-free hot path** — `CvCascadeEvent` is a small POD passed by const-ref (no heap per emit); the
  interest-guard makes a dormant firehose ≈ one bit-test; the logging consumer formats into a stack buffer.
- **Bounded observability buffers** — the `/events` queue is capped + self-draining (it drains even with no
  client, so it can't grow unbounded). See [event-spine §9a-equivalent below].
- **Compact counts** — the tally weighs sparse (`std::map`) vs dense (`int[]`) per domain when each lands.

## 9. Observability — `CvHttpServer` is the formal live layer

`CvHttpServer` is the cascade's formal **live-observability / query layer**, not an experimental bolt-on. The
spine's logging consumer streams through the shared `streamLogTee` (`BetterBTSAI.{h,cpp}`), so a log line reaches
the live `/events` SSE stream by one canonical path, gated by `gStreamLogLevel`. The tally will expose a `/tally`
snapshot endpoint the same publish-and-serve way when it lands (**planned, not built** — DOMAIN emits are already
observable as `[SPINE/DOMAIN]` frames on `/events`). Any snapshot must publish from the **game thread** — never
read `cascadeTally()` on the server thread (the snapshot-isolation HARD constraint). Full surface:
[`../observability/README.md`](../observability/README.md) and
[DEC-obs-hook-shapes](../../architecture/decisions.md#dec-obs-hook-shapes).

## See also
- [cascade-architecture §5](../../explanation/cascade-architecture.md) — the one-paragraph overview of the spine
  and `IEventConsumer`; this doc is the detail behind it.
- [tally.md](tally.md) — the spine's first authoritative consumer; its `DOMAIN`-only `wantedKinds()` is the
  firewall (§2) in action, and its rebuild-on-load model complements the event stream.
- [`../../architecture/patterns/faking-di.md`](../../architecture/patterns/faking-di.md) &
  [`../../architecture/patterns/composability.md`](../../architecture/patterns/composability.md) — `IEventConsumer`
  is the realized exemplar of both patterns (program to the contract; wire the concrete at a composition root —
  `cascadeRegisterConsumers`).
- [data-model.md](data-model.md) — the JSON `grants` section the (planned) grants consumer fires on delivery.
- [DEC-interface-contracts](../../architecture/decisions.md#dec-interface-contracts) ·
  [DEC-map-before-delete](../../architecture/decisions.md#dec-map-before-delete) — the rulings this doc applies.
- [`../observability/README.md`](../observability/README.md) — the live surface the logging consumer publishes to.
