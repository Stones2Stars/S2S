# Architectural north-star — the structural compass

> Read before any structural change. Condensed from the old north-star.

The engine resolves into **two halves**: the **data side** (cascade + tally — top-down declarative, the active
rework) and the **AI side** (a consumer of data, out of active scope). Keeping that boundary clean **is** the
architecture.

**Three core data-engine structures:**
- **modifier** — magnitudes deposit DOWN the scope spine (integer ×100 fixed-point); the deliveryguy owns
  cross-entity modifiers keyed by target ([modifier](../specs/modifier.md)).
- **enabler** — 2-pass generate-then-gate; narrows via enabled/replaced/obsoleted/disabled; `requires` checks only
  the "can get" subset via a `require` callback **UP** the chain (the AND mechanism — *why it is bidirectional*; a
  down-only model expresses OR but not AND and forces maintainers to the top of the chain) ([enabler](../specs/enabler.md)).
- **tally** — counts roll UP; serializes nothing; rebuilt on load ([tally](../specs/tally.md)).

The engine is **bidirectional**: modifiers down, tally counts + `require` callbacks up. A down-only mental model is wrong.

**Orwellian logging** (total observability) is a landed prerequisite, not a nicety — it is what makes safe legacy
deletion (shadow-until-clean) possible ([logging](../specs/logging.md)).

**The one unmovable constraint:** the closed Firaxis `.exe` ABI freezes the C++03/VC7.1 toolchain — it constrains
**syntax, not architecture** ([engine](../reference/engine.md)). Clean Architecture **is** achievable here; "old
compiler = must stay a god-class tangle" is the mistake this kills.

**How to build it (Clean Architecture in C++03):** depend on interfaces not concretions; compose from small
contracts; isolate layers ([patterns](patterns.md)). The cascade (`IEventConsumer` + spine/tally/grants/logging
behind it) is the realized exemplar.

**Standing goals:** dissolve `CvCityAI`/`CvUnitAI` into interface-bounded composition (the graft-onto-derived lane);
a pluggable external AI backend; retire the `CvInfos.h` umbrella; keep converting imperative maintainers into
interface-bounded machines.
