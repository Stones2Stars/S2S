# Architectural north-star — the structural compass

> Read before any structural change. Condensed from the old north-star.

The engine resolves into **two halves**: the **data side** (cascade + tally — top-down declarative, the active
rework) and the **AI side** (a consumer of data, out of active scope). Keeping that boundary clean **is** the
architecture.

## ⛔ THE RULING: EACH IS ITS OWN SYSTEM

Everything else here is downstream of one ruling (owner). The data side is **four separate systems**, each with
exactly one job, chained in one direction:

| system | its ONE job | ends at |
|---|---|---|
| **readJson** | puts the authored data **into** the infos | the info is populated — nothing else is its business |
| **infos** | **serve** that data, in the shape consumers need | handing data out; an info never computes with it |
| **cascade** | **sums modifiers** — "how much?" | a magnitude |
| **enabler** | figures out **what we HAVE, and what we CAN GET** — "can I?" | an availability verdict |

*(The [tally](../specs/tally.md) — "how many?" — is not a fifth system with state of its own: it is the read-only
count accessor the other two ask, reading counts the game objects already own.)*

**Every boundary defect this project keeps hitting is one system doing another's job**, which is why they present
as unrelated bugs and get fixed one at a time:

- cascade runtime stored on an info = **infos doing the cascade's job**. An info is write-once-at-load and shared
  by every player; cascade state is per-owner and mutable, so this also makes a shared immutable object mutable
  per game rather than per load.
- "the enabler cascade", or one spine consumer routing both = **two systems named and wired as one**. They differ
  concretely (the enabler is load-ACTIVE and maintains sets by targeted propagation; the modifier is a
  dirty→recompute value cache), so welding them forces one policy onto two that genuinely need different ones.
- the cascade re-deriving whether a building is active = **the cascade doing the enabler's job**. It asks
  ([DEC-calc-zero-ride-in](decisions.md#dec-calc-zero-ride-in)).
- a getter that reaches past the info surface into a per-type shape = **a consumer doing readJson's job**.

**So the test for any new code is one question: whose job is this?** If the answer names two systems, the design
is wrong — not the implementation. This ruling is what the individual boundary rulings are instances of; state it
once, here, rather than re-deriving a fresh prohibition at each seam.

**⛔ "KEEP" means OUTSIDE the four systems — nothing else (owner).** The ONLY legacy that is legitimately kept is
work that is none of the four systems' job: the **trade-route network calculation** (the cascade cannot re-derive
the network, so it folds the route yield as an INPUT — [modifier §2a `tradeYield`](../specs/modifier.md)), the
**property engine's internal math** (the decay / diffusion / solver, owner-locked — [property-audit](../plans/structural-cleanup/property-audit.md)),
and the **WAREHOUSE mechanics** below.

**⚖ THE WAREHOUSE CARVE-OUT — accumulating a rate and spending it is the GAME OBJECT's job, not the cascade's.**
The cascade owns the RATE; what the object does with that rate over time is its own mechanic. Two instances, one
shape: **production banking** (hammers accumulate into a build order, overflow spills) and **CULTURE** (the
per-turn culture rate is a cascade commerce channel, but the banked culture VALUE, the culture LEVEL derived from
it, and the border/plot-ownership it buys are the city's warehouse). A warehouse reads a cascade rate and keeps
its own ledger, so it passes the whose-job test by naming no system — do not mistake the banked value for an
unmigrated cascade channel and do not move the accumulation into a package.
These pass the test above by naming NO system: ask "whose job is this?" and the answer is neither readJson, nor
infos, nor cascade, nor enabler. **Anything a system SHOULD touch is never KEEP — it is wired or it is open.** A
count is the tally's job, so a bespoke engine count-loop is an unwired tally domain, not a KEEP; availability is
the enabler's, magnitudes the cascade's, state-changes the spine's. "KEEP — engine-owned, revisit later" applied
to in-scope work is the deferral-in-disguise this rules out ([DEC-no-deferred](decisions.md#dec-no-deferred)):
the honest label is OPEN, never KEEP. (A system reading a KEEP thing as a raw INPUT — the cascade folding trade
yield, the enabler reading the property value across a band — is the boundary working, not a KEEP of the reading
system's own work.)

**Three core data-engine structures:**
- **modifier** — magnitudes deposit DOWN the scope spine (integer ×100 fixed-point); the deliveryguy owns
  cross-entity modifiers keyed by target ([modifier](../specs/modifier.md)).
- **enabler** — 2-pass generate-then-gate; narrows via enabled/replaced/obsoleted/disabled; `requires` checks only
  the "can get" subset via a `require` callback **UP** the chain (the AND mechanism — *why it is bidirectional*; a
  down-only model expresses OR but not AND and forces maintainers to the top of the chain) ([enabler](../specs/enabler.md)).
- **tally** — counts roll UP; a read-only accessor over the object-owned counts (no store/seed/shadow) ([tally](../specs/tally.md)).

The engine is **bidirectional**: modifiers down, tally counts + `require` callbacks up. A down-only mental model is wrong.

**Orwellian logging** (total observability) is a landed prerequisite, not a nicety — it is what made safe legacy
deletion possible and remains the verification ground truth ([logging](../specs/logging.md)).

**The one unmovable constraint:** the closed Firaxis `.exe` ABI freezes the C++03/VC7.1 toolchain — it constrains
**syntax, not architecture** ([engine](../reference/engine.md)). Clean Architecture **is** achievable here; "old
compiler = must stay a god-class tangle" is the mistake this kills.

**How to build it (Clean Architecture in C++03):** depend on interfaces not concretions; compose from small
contracts; isolate layers ([patterns](patterns.md)). The cascade (`IEventConsumer` + spine/tally/grants/logging
behind it) is the realized exemplar.

**Standing goals:** dissolve `CvCityAI`/`CvUnitAI` into interface-bounded composition (the graft-onto-derived lane);
a pluggable external AI backend; retire the `CvInfos.h` umbrella; keep converting imperative maintainers into
interface-bounded machines.

**Engine state** follows the same discipline: a domain object's *derived* data (yields, commerce, …) lives in a
**recompute-only, dirty-flagged, never-serialized** cache that is the single **PULL** source up the chain — the
[state-repositories](state-repositories.md) pattern (the plot-yield cache is the landed first instance). The unified
`dataChanged` trigger it builds toward is the missing primitive behind the entire "stale cache" bug class. `CvPlot`/
`CvCity` stay as the domain objects; only the derived layer and `Cv*AI` change.
