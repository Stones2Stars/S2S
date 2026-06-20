# The code-structure north-star — where S2S is going, structurally

> **Status:** ideas / direction (the compass for all structural work) · **Grounding:**
> [`AGENTS.md`](../../../AGENTS.md) architectural rulings + the realized cascade (`Sources/Cascade/`).
> **Read before any structural change** — this is the grain of the wood; working against it is how you
> rework something 2–3 times instead of building it right once ([DEC-proper-once](decisions.md#dec-proper-once)).
>
> ⚠ **Landed-DESIGN ≠ implemented.** Much of the data side below is **design that has landed** while the
> **implementation is still in progress**. Assuming a structure is built when only its design is landed is
> itself a head-bite — each section marks which it is.

---

## 1. The engine has two sides

The whole engine resolves into two halves, and keeping them separate *is* the architecture:

- **The DATA side** — holds and computes game state (what a building enables, how much a modifier deposits,
  how many of a thing exist). **This side is being moved onto the cascade + tally model** (§2) —
  declarative, top-down, observable. **Design landed; implementation in progress.**
- **The AI side** — decision and behaviour, operating *on* that data (what to build, where to attack). Out
  of *active* scope right now and partitioned (`../plans/ai/`); it is the consumer of the data side.

A clean boundary between "compute the data" and "decide on the data" is the destination. Today they are
tangled through the Civ4 god-classes; untangling them is the long arc.

## 2. The landed CORE STRUCTURES of the data engine (front and center)

These are **the** core principle for the data side — what every data-side change moves toward. **Design has
landed; not all implemented yet.**

- **The CASCADERS** — cascades replacing ~7–8k lines of imperative "maintainer" machinery:
  - **modifier** — "how much?": magnitudes deposit **DOWN** the scope spine, integer fixed-point
    ([DEC-fixedpoint-x100](decisions.md#dec-fixedpoint-x100)). A cross-entity modifier lives on whoever
    **delivers** it (the *deliveryguy*), keyed by the target — not inverted onto the target
    ([DEC-deliveryguy](decisions.md#dec-deliveryguy)).
  - **enabler** — "can I?" (generate-then-gate, 2-pass). The top-down **narrows the data in three stages**:
    **(1) have** — what you actually possess (built / researched), determined by four states: **enabled**
    *adds* to the list, while **replaced**, **obsoleted**, and **disabled** each *remove* from it (in their
    own way) → **(2) can get** —
    everything *enabled*, mostly through **techs, traits, and civics** (the enabled subset) → **(3) can
    actually do** — where **`requires` comes in**: it checks **only against the "can get" subset** (never
    the whole data space), resolved by a **`require` callback back UP the chain** (querying that enabled
    up-scope state and the tally counts).
    That is how the cascade models **AND**: a requirement tests the already-enabled ("can get") subset
    up-chain rather than re-deriving it — and it stays cheap precisely because it only ranges over what is
    already enabled.
    **Why not down-only** (it was the original design): down-only models **OR** via enablers but **cannot
    reliably model AND**, and it forces a modder to maintain every requirement at the **top of the chain** —
    a maintenance nightmare. The upward `require` callback is load-bearing, not optional
    ([DEC-cascade-bidirectional](decisions.md#dec-cascade-bidirectional)).
- **The TALLY** — "how many?": counts roll **UP** the spine, feeding the enabler's `require` callbacks.
  Serializes nothing; rebuilt from loaded objects on load
  ([DEC-tally-serializes-nothing](decisions.md#dec-tally-serializes-nothing)).

So the data engine flows **both ways**: modifiers deposit *down*; the tally's counts and the enabler's
`require` callbacks resolve *up*. A down-only mental model is wrong — that asymmetry is the whole point.
- **ORWELLIAN LOGGING** — total observability: reconstruct full game state from HTTP endpoints + `/events`
  + gated logs, **never the screen** ([DEC-obs-scale](decisions.md#dec-obs-scale)). A **landed principle**,
  not a nicety: it is the only way a legacy maintainer can be *safely* deleted — shadow the new structure
  against the live engine until clean, then cut ([DEC-map-before-delete](decisions.md#dec-map-before-delete)).

The machines are fed by an **event spine** and program to one contract (`IEventConsumer`) — the realized
exemplar of §4. Detailed design: `../explanation/cascade-architecture.md` *(pending rebuild)*;
[`../reference/observability/README.md`](../reference/observability/README.md);
[`../reference/cascade/fixed-point-and-scales.md`](../reference/cascade/fixed-point-and-scales.md).

## 3. The one fixed constraint, and what it does NOT constrain

The only unmovable boundary is the **closed Firaxis `.exe` ABI** — it binds the `DllExport` surface and the
vtable/layout of the EXE-bound base classes (`CvCity`, `CvUnit`, …). The frozen C++03/VC7.1 toolchain
limits the **syntax** (virtual interfaces not templates, manual wiring not a container) — **not the
architecture.** Clean Architecture is fully achievable here in C++03 clothes. Conflating "old compiler"
with "must stay a god-class tangle" is the mistake this doc kills.

## 4. HOW the structures are built — Clean Architecture in C++03

Depend on interfaces, not concretions; compose behaviour from small contracts; isolate layers. The concrete
patterns live in [`patterns/`](patterns/README.md) — the **one** home for landed design patterns:
- **[Faking DI](patterns/faking-di.md)** — poor-man's DI: an `if`/`switch` composition root assigns a
  concrete to a contract pointer; no container.
- **[Composability](patterns/composability.md)** — the C++03 interface shape (pure-virtual base, no data),
  MI-as-`implements`, the two ABI guardrails, and isolate-systems-behind-one-contract.

The cascade is this made concrete: `IEventConsumer` is the contract; spine / tally / grants / logging are
pluggable behind it (`Sources/Cascade/`). → [DEC-interface-contracts](decisions.md#dec-interface-contracts).

## 5. Standing goals (ASPIRATIONAL — direction, not current state)

- **Dissolve `CvCityAI` / `CvUnitAI` into interface-bounded composition** (the AI side; "shrink the god
  classes") — the graft-onto-derived lane (`patterns/composability.md`) is the mechanism.
- A **pluggable external AI backend** — the dream the contracts eventually enable.
- **Retire the `CvInfos.h` umbrella** — include the specific `CvXInfo.h` directly.
- Continue converting imperative maintainers into top-down, interface-bounded machines (the cascade is the
  first, not the last).

## See also
- [`README.md`](README.md) — this area (ideas + designs + rulings), and the recording rule.
- [`patterns/`](patterns/README.md) — the landed design patterns (the "how").
- [`../README.md`](../README.md) — the comprehension map.
- `../plans/ai/` *(out of active scope)* — the AI-side direction, downstream of this compass.
