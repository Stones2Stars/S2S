# Architecture — the single, constantly-maintained guiding core

> **Status:** living (constantly maintained) · **Grounding:** [`AGENTS.md`](../../../AGENTS.md) architectural
> rulings + the realized cascade (`Sources/Cascade/`).
>
> This is the **one place** that holds what S2S is being wrangled *toward*. The codebase is, right now, a
> clown-fiesta — a decades-deep Civ4/BTS/C2C tangle. It is being pulled toward something that resembles
> sense **over time, while iterating on it** — and the only way that convergence stays coherent is if the
> guiding core lives in exactly one, maintained place. This is that place. It holds three things:

| | What | Where |
|---|---|---|
| **Ideas** | the overarching direction — Clean Architecture in a frozen C++03 engine | [`north-star.md`](north-star.md) |
| **Designs** | the **landed design patterns** — the reusable "how we build" | [`patterns/`](patterns/README.md) |
| **Rulings** | the decisions ledger — every binding ruling, ID'd | [`decisions.md`](decisions.md) |

## ⛔ THE RULE — a landed design pattern is recorded HERE, and ONLY here (owner ruling 2026-06-19)

You can only *land* a design pattern while iterating on the beast — you discover it works by doing the
work. **The moment a pattern lands, record it in [`patterns/`](patterns/README.md), in the same change.**
One pattern, one home, here. Never re-describe a pattern in a subsystem doc — that is how patterns drift
out of consistency, which is the exact rot we are escaping. A subsystem doc that uses a pattern **links**
its pattern doc.

This is the engineering-pattern application of the project-wide single-source-of-truth discipline
([DEC-WF-rulings-to-repo](decisions.md#dec-wf-rulings-to-repo),
[DEC-keep-unkilled-ideas](decisions.md#dec-keep-unkilled-ideas)). It is **living**: as the rework surfaces
patterns, decisions, and refinements, they are folded in here, continuously — this area is never "done."

> **Three "guidance" homes — keep them distinct:** *here* = how to STRUCTURE CODE (ideas/patterns/rulings);
> [`../_meta/CONVENTIONS.md`](../_meta/CONVENTIONS.md) = how to WRITE DOCS; [`AGENTS.md`](../../../AGENTS.md)
> Conventions = workflow / build / git. When unsure where a new rule belongs: is it about *code shape* →
> here; *doc writing* → `_meta`; *process* → `AGENTS.md`.

## See also
- [`../README.md`](../README.md) — the comprehension map; this area is the core it orbits.
