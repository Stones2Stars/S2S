# Event spine — the one dispatch primitive

> A **core spec.** The event spine is **where the consumers get their events** — it is *not* logging; logging is one
> consumer of it (grants, cache-invalidation, and the out-of-process replay are others). One `emit`, fanned out by
> KIND to every registered consumer. *(The in-engine **tally** is NOT a consumer — it reads the object-owned counts
> directly; [tally.md](tally.md).)*

## The primitive
A caller `emit`s an event; every consumer that registered interest in that event's **KIND** receives it. KIND is
declared **at the call site, never inferred**.

## KIND — the OOS firewall
Civ4 multiplayer is deterministic lockstep, so an authoritative count that differs per machine is a desync. KIND
keeps the synced and unsynced streams apart:

| KIND | meaning | synced? | consumed by |
|---|---|---|---|
| **`DOMAIN`** | game **state** changed (building built, unit created, tech researched) | yes — deterministic | logging + grants + cache-invalidation + out-of-process replay (NOT the in-engine tally — it reads the object-owned counts) |
| **`DIAGNOSTIC`** | **code** ran (a function entered, a decision re-evaluated) | no — execution trace | **logging only** — never counted, never gates |
| **`TRACE`** | fine-grained "every step" | no | logging only |

Only `DOMAIN` events carry authoritative synced state-changes (for observability, cache-invalidation, and the
out-of-process replay). The in-engine [tally](tally.md) does **not** consume them — it reads the object-owned counts
directly. The payload is **raw** (typed fields,
never a pre-formatted string) so the costly index→text formatting defers to the gated [logging](logging.md)
consumer — when a gate is off, nothing expensive ran.

## The `IEventConsumer` contract
Consumers attach through **one C++03 interface, `IEventConsumer`** (a pure-virtual base, no data members) — the
`grants` and logging are independent implementations pluggable behind it (the realized exemplar of the
project's [interface-contract pattern](../architecture/patterns.md)); the [tally](tally.md) is **not** a consumer (it
reads objects). **Build order:** spine + the modifier scope accumulator → logging (broad) → grants →
[modifier](modifier.md) → [enabler](enabler.md). *(The tally is a read-only accessor, not a step on the spine.)*

## The C++ shape (`CvEventSpine.{h,cpp}`)
- **`CvSpineEvent`** is a POD carrying **two payloads, not two exclusive modes**: the raw **DOMAIN state ints**
  (`iType`/`iA`/`iB`/`iC` + `iSrcLoc` = WHERE), which `grants` and the cache-invalidation consumer read; **and** the
  **render payload** (`iDomainTag`/`iEventId`/`aFields[]`, `SPINE_MAX_FIELDS = 16`; a field is `{int eTag; union{int
  i; float f; char* s; wchar_t* w;}}`, 8B/POD) that the one logging path formats. A **`DOMAIN`** event carries BOTH —
  its state ints for the machine consumers **and** a domain tag + fields so it renders through the same registered
  path as everything else; a **`DIAGNOSTIC`/`TRACE`** event carries only the render payload. There is no
  inline-formatted event: the spine's own DOMAIN events register under `SD_SPINE` exactly like an AI domain.
- **Per-domain isolation:** a domain registers via `spineRegisterDomain` (a line-prefix fn + a field-info fn with
  typed index kinds `SFT_BUILDING`/`UNIT`/`BONUS`/…); `spineRenderEventLine` formats. **Zero global field registry,
  zero shared edits per domain** — adding a domain touches only that domain. **The logging consumer is exactly
  `gate(iLevel) → spineRenderEventLine → write`** — no per-event branch, no inline `sprintf`; a line's identity is
  entirely its registered prefix + fields.
- **Interest guard:** an `m_iInterestMask` bit-test gates dispatch, so the verbose call-site `if(logLevel)` gates
  vanish structurally.
- **Allocation-free hot path** (stack-buffer formatting, a bounded `/events` queue) — 32-bit ceiling discipline.
- **Name-change event** (`SEVT_NAME_CHANGE`): the four set-name choke points emit `(NameChangeKind, owner,
  entity_id)` in the DOMAIN ints (an out-of-process consumer keys on those). Because the logging consumer is generic,
  the `emitNameChange` endpoint resolves the NEW name + kind LIVE and passes them as render fields (`SFT_STR` kind +
  `SFT_WSTR` name — the emit render is synchronous on the game thread, so the borrowed pointers outlive it). This is
  the one place a spine endpoint does resolution at emit rather than deferring to the gated render — justified because
  a rename is rare (four low-frequency choke points), not a hot-path firehose.
- **Build status:** the spine primitive + KIND firewall + `IEventConsumer` = DONE; the **DOMAIN emit surface**
  (source-carrying endpoints + every mutation choke point wired) = DONE, incremental emits verified firing live from
  **real in-play state-changes**. The **load reseed is BUILT and LIVE**: the in-read emits fire from inside the
  save read (`CvPlayer::read` per-held-tech/projects/civics/traits, `CvCity::read` per building/religion/corp/
  bonus/culture, `CvPlot::read` substrate — verified in `Cascade.log`: ~190k DOMAIN events inside the load
  bracket), wrapped by the **load-lifecycle bracket** `GAME_LOAD_STARTED` / `GAME_LOAD_FINISHED` (also built —
  `emitGameLoadStarted/Finished`; result-producers suppress inside it via `spineIsLoadActive`). Logging =
  registered first; the **tally** = a read-only accessor (buildings + units), NOT a spine consumer
  (`Cascade/CvCascadeTally.{h,cpp}`); grants = resolver built, the apply-loop not built
  ([grants-machine.md](../plans/structural-cleanup/grants-machine.md)); the **cache-invalidation consumer** =
  built for routing (`CvCascadeInvalidation.cpp` — the enabler domains consume the reseed through it; the
  modifier-package reseed build is in flight).

## The DOMAIN emit surface + the load RESEED

**The spine is the SINGLE place a state change is announced.** Every game state change emits ONE source-carrying
DOMAIN event through a clean endpoint (`emitBuildingChanged`, `emitTechChanged`, `emitImprovementChanged`,
`emitCityOwnerChanged`, …); the event names WHAT (`iType`), WHO (`iC`, owner/triggering player), and WHERE
(`iSrcLoc` = cityId | plotId | -1). `emit()` dispatches **synchronously** — it is not an async listener bus; it calls
each interested consumer's `onEvent` inline at the mutation site. So nothing else in the engine detects changes: the
hand-wired per-site invalidation is retired in favour of this one surface.

**Events are FACTS, not causal steps.** "This building is here", "this tech is held" — order-independent,
prerequisite-free. Prerequisites are evaluated ONLY by the enabler (`canConstruct`/`canTrain`/`canResearch` — the
"*can* I?" question), never by a has-been-done fact; so the emit stream carries no ordering and no prereq logic.
Corollary — **yield is a computed RESULT, never an event**: emit the CAUSES (improvement/terrain/feature/route
changed), and a consumer computes the yield downstream.

**The load RESEED — event-source the save READ (BUILT, live).** A loaded save deserializes state directly into the
`CvCity`/`CvPlot` objects — the incremental setters never fire, so the **cascade** (its value packages AND its enabler
side) would have nothing to build from. The reseed fixes this **from inside the save read itself**: reading a fact off
the stream is what fires its DOMAIN event (`CvGame::read` → `CvPlayer::read` → `CvCity::read` / `CvPlot::read`; tech
is team-held but emitted per-self from each member's `CvPlayer::read`, one emit per alive member; projects the same).
The north-star is that the event itself SETS the state — read → emit → populate, one mechanism for the
game object AND the cascade; the object-populated-by-events end is a known **step too far** for now, but the events
come from the genuine read.

⛔ What the reseed is **NOT**: a separate pass that walks already-deserialized objects and **fabricates** events from
their populated state (a "for each building present, emit built"). That pseudo-emit feeds the cascade reconstructed
lies and trains the next agent to reconstruct more — it is banned
([superseded-ideas](../architecture/superseded-ideas.md)). There is no clean middle between it and the real
event-sourced read, so the read-driven reseed is built as its own step, never shimmed.

**The load lifecycle is bracketed by two spine events — `GAME_LOAD_STARTED` / `GAME_LOAD_FINISHED`.** Result-producers
(grants, and any future on-event side-effect machinery) rely **purely on the spine**: they see `LOAD_STARTED` →
suppress, `LOAD_FINISHED` → resume, so nothing is granted during reconstruction (a grant is a RESULT of a genuine
in-play acquisition, and a load is not an acquisition). The **cache-build consumer** is the load-active one — it
consumes the in-read events to build the cascade. New game builds the same way: its real init fires the same events,
with grants active because those are genuine acquisitions. Ledgered as
[DEC-spine-reseed](../architecture/decisions.md#dec-spine-reseed).

## See also
- [logging.md](logging.md) — the broad consumer (what to log). [tally.md](tally.md) — the read-only count accessor
  (reads object-owned counts; NOT a spine consumer). [../architecture/patterns.md](../architecture/patterns.md) — the `IEventConsumer` interface pattern.
