# Event spine — the one dispatch primitive

> A **core spec.** The event spine is **where the consumers get their events** — it is *not* logging; logging is one
> consumer of it (so are the tally and grants). One `emit`, fanned out by KIND to every registered consumer.

## The primitive
A caller `emit`s an event; every consumer that registered interest in that event's **KIND** receives it. KIND is
declared **at the call site, never inferred**.

## KIND — the OOS firewall
Civ4 multiplayer is deterministic lockstep, so an authoritative count that differs per machine is a desync. KIND
keeps the synced and unsynced streams apart:

| KIND | meaning | synced? | consumed by |
|---|---|---|---|
| **`DOMAIN`** | game **state** changed (building built, unit created, tech researched) | yes — deterministic | the **tally** (gate-eligible) + logging + grants |
| **`DIAGNOSTIC`** | **code** ran (a function entered, a decision re-evaluated) | no — execution trace | **logging only** — never counted, never gates |
| **`TRACE`** | fine-grained "every step" | no | logging only |

Only `DOMAIN` events feed the authoritative [tally](tally.md) and the gates. The payload is **raw** (typed fields,
never a pre-formatted string) so the costly index→text formatting defers to the gated [logging](logging.md)
consumer — when a gate is off, nothing expensive ran.

## The `IEventConsumer` contract
Consumers attach through **one C++03 interface, `IEventConsumer`** (a pure-virtual base, no data members) — the
tally, `grants`, and logging are independent implementations pluggable behind it (the realized exemplar of the
project's [interface-contract pattern](../architecture/patterns.md)). **Build order:** spine + accumulator (the scope/tally accumulator the [tally](tally.md) maintains) →
logging (broad) → tally (selective, `DOMAIN`-only) → grants → [modifier](modifier.md) → [enabler](enabler.md).

## The C++ shape (`CvEventSpine.{h,cpp}`)
- **`CvCascadeEvent`** is a POD with **two payload modes**: DOMAIN (`iType`/`iA`/`iB`/`iC`) vs logging
  (`iDomainTag`/`iEventId`/`aFields[]`, `SPINE_MAX_FIELDS = 16`). A field is `{int eTag; union{int i; float f; char* s; wchar_t* w;}}` (8B/POD).
  The `iType`/`iA`/`iB`/`iC` mode is for **`DOMAIN`** events; the `aFields[]` mode is for **`DIAGNOSTIC`/`TRACE`** (logging) events.
- **Per-domain isolation:** a domain registers via `spineRegisterDomain` (a line-prefix fn + a field-info fn with
  typed index kinds `SFT_BUILDING`/`UNIT`/`BONUS`/…); `cascadeRenderEventLine` formats. **Zero global field registry,
  zero shared edits per domain** — adding a domain touches only that domain.
- **Interest guard:** an `m_iInterestMask` bit-test gates dispatch, so the verbose call-site `if(logLevel)` gates
  vanish structurally.
- **Allocation-free hot path** (stack-buffer formatting, a bounded `/events` queue) — 32-bit ceiling discipline.
- **Name-change event** (`CASCADE_EVT_NAME_CHANGE`): the four set-name choke points emit `(NameChangeKind, owner,
  entity_id)` — **string-free** (carry the ID, let the consumer resolve the name); the tally ignores it (`switch` default).
- **Build status:** spine = DONE; logging = registered first; tally = PARTIAL (buildings + units); grants = NOT BUILT.

## See also
- [logging.md](logging.md) — the broad consumer (what to log). [tally.md](tally.md) — the `DOMAIN`-only authoritative
  consumer. [../architecture/patterns.md](../architecture/patterns.md) — the `IEventConsumer` interface pattern.
