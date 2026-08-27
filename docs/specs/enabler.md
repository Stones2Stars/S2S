# The enabler — "can I?"

> **⛔ NAMING.** This is **the
> enabler** — a system SEPARATE from the modifier **cascade**. The two are routinely conflated; do not. "cascade"
> names the modifier ("how much?") system ONLY. The enabler's classes carry no `Cascade` prefix
> (`EnablerKernel`/`BuildingEnabler`/`UnitEnabler`/`TechEnabler`), and its availability getters read the enabler's
> OWN cached sets directly.

The enabler is the machine that decides **what an entity is allowed to do or build right now** — research a tech,
train a unit, construct a building, adopt a civic, lay an improvement. It answers one question per candidate: *"can I
take this action this turn?"* — and as a byproduct, *why not* (greyed / hidden).

It **reads** the availability data authored on entities — `enables`, `obsoletes`, `replaces`, `disables`,
`requires`, `allowed` (the [json spec](json.md) §4 owns their shape). This doc is the **machine** that consumes
them; it does not restate the JSON syntax.

---

⛔ **The pages below ARE the spec — this page is a map and carries no ruling of its own.**
Read the parts your work touches END TO END; the count that applies is something you FIND, not something
you decide ([AGENTS.md](../../AGENTS.md)).

## The parts

| part | what it settles |
|---|---|
| **[the one idea generate then gate](enabler/01-the-one-idea-generate-then-gate.md)** | 1. The one idea: GENERATE, then GATE |
| **[pass 1 generate the frontier the](enabler/02-pass-1-generate-the-frontier-the.md)** | 2. Pass 1 — GENERATE the frontier (the `enables` family) |
| **[pass 2 gate each candidate](enabler/03-pass-2-gate-each-candidate.md)** | 3. Pass 2 — GATE each candidate (`requires`) |
| **[the allowed cap](enabler/04-the-allowed-cap.md)** | 4. The `allowed` cap |
| **[the load bearing asymmetry](enabler/05-the-load-bearing-asymmetry.md)** | 5. The load-bearing asymmetry — bidirectional, not down-only |
| **[greying the build list tri state](enabler/06-greying-the-build-list-tri-state.md)** | 6. Greying — the build-list tri-state falls out for free |
| **[recompute cadence the runtime](enabler/07-recompute-cadence-the-runtime.md)** | 7. Recompute cadence + the runtime realization — event-maintained vectors over `f(HAVE)` |
| **[the machines shape components host](enabler/08-the-machines-shape-components-host.md)** | 8. The machine's shape — components, host, and the read surface |

## See also
- [json.md](json.md) — the data this machine reads: `enables`/`obsoletes`/`replaces`/`disables` (§4.1–4.2),
  `requires` build/operate (§4.3), `allowed` (§4.4), and the `all`/`any`/`noneOf` + atom/predicate vocabulary (§3).
- [tally.md](tally.md) — the count machine the `requires` count-atoms and the `allowed` cap read at cross-city scopes.
- [modifier.md](../cascade.md) — the sibling "how much?" machine. A dormant/unavailable entity (per this doc)
  simply deposits no modifiers.
- [naming.md](naming.md) — the `INFOTYPE_NAME` ids that fill the `enables` buckets and `requires` atoms.
