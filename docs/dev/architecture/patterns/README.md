# Design patterns — the landed-pattern library (1 place only)

> **Status:** living · **Grounding:** [`AGENTS.md`](../../../../AGENTS.md) + the realized cascade
> (`Sources/Cascade/`).
>
> The single home for **landed** S2S design patterns. A pattern is "landed" once iterating on the codebase
> has shown it works — at that moment it is recorded here, in the same change, and nowhere else (the RULE:
> [`../README.md`](../README.md)). Subsystem docs **link** a pattern, never re-describe it; that is how
> patterns stay consistent instead of drifting into N slightly-different copies.

## Patterns

| Pattern | One-line | Doc |
|---|---|---|
| **Faking DI** | poor-man's dependency injection in C++03 — `if`/`switch` composition root assigns a concrete to a contract pointer; no container | [faking-di.md](faking-di.md) |
| **Composability** | compose behaviour from small pure-virtual interface contracts via MI-as-`implements`; isolate systems behind one shared contract | [composability.md](composability.md) |

*(More land here as the rework surfaces them.)*

## Template for a new pattern doc
```markdown
# <Pattern name>
> Status / Grounding (cite the realized code where it exists)
**Intent:** one line — the problem it solves.
**Context:** when it applies (and the C++03/EXE constraints that shape it).
**The pattern:** how to do it, concretely.
**Guardrails / what it is NOT:** the failure modes.
**Realized example:** where it already lives in the code.
## See also
```
