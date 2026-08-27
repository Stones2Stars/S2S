# The Cascade Rebuild — how the S2S engine works, and what it replaced

> **What this is.** A guided tour of the engine's data side: how game data is stored, how "can I build
> it?" and "how much does it produce?" are answered, how state changes are announced, and how things get
> handed over. It exists so a newcomer — human or agent — can get a real overview in one read.
>
> ⛔ **It is a PRIMER, not an authority.** Every ruling below is stated in full in the doc that owns it, and
> this page LINKS there rather than restating it. Where this page and a spec disagree, **the spec wins**; where
> a spec and the tree disagree, **the tree wins** ([AGENTS.md](../AGENTS.md)). Use this to find the right file,
> then read that file end to end.
>
> **The owning docs:** [json.md](specs/json.md) (the data model) · [enabler.md](specs/enabler.md) ("can I?") ·
> [cascade.md](cascade.md) ("how much?") · [spine.md](spine.md) (the event spine) ·
> [triggers.md](specs/triggers.md) (provisions) · [north-star.md](architecture/north-star.md) (the compass).

Stones2Stars is a mod of *Civilization IV: Beyond the Sword*, descended from Caveman2Cosmos. Over one
development cycle its derived-state layer was rebuilt from the ground up. This is what changed and why each
piece is shaped the way it is.

| | |
|---|---|
| entities migrated XML → JSON | 13,088 across 37 types |
| legacy XML retired | ~570,000 lines |
| distinct state facts on the spine | 161 |

---

⛔ **The pages below ARE the spec — this page is a map and carries no ruling of its own.**
Read the parts your work touches END TO END; the count that applies is something you FIND, not something
you decide ([AGENTS.md](../AGENTS.md)).

## The parts

| part | what it settles |
|---|---|
| **[the machine we inherited](overview/01-the-machine-we-inherited.md)** | 1. The machine we inherited |
| **[four machines one job each](overview/02-four-machines-one-job-each.md)** | 2. Four machines, one job each |
| **[the data reads cold](overview/03-the-data-reads-cold.md)** | 3. The data reads cold |
| **[the enabler generate then gate](overview/04-the-enabler-generate-then-gate.md)** | 4. The enabler: generate, then gate |
| **[the spine a fact names the](overview/05-the-spine-a-fact-names-the.md)** | 5. The spine: a fact names the happening |
| **[the cascade nothing is ever](overview/06-the-cascade-nothing-is-ever.md)** | 6. The cascade: nothing is ever recalculated |
| **[triggers the plane that acts](overview/07-triggers-the-plane-that-acts.md)** | 7. Triggers: the plane that acts |
| **[watching the game from outside it](overview/08-watching-the-game-from-outside-it.md)** | 8. Watching the game from outside it |
| **[what we refused to build](overview/09-what-we-refused-to-build.md)** | 9. What we refused to build |
| **[going forward](overview/10-going-forward.md)** | 10. Going forward |

