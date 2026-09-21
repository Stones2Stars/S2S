# The S2S Complexity Index™

A rigorously unscientific ranking of Stones2Stars systems by the complexity they inflict on
anyone who dares to understand them, measured in centi-Gordians.

---

Complexity is measured in **centi-Gordians (cg)** — the SI unit of structural complexity,
calibrated against a single Gordian Knot (100 cg): a structure that cannot be untied, only
cut.

We tried to do this properly. We lined up the individual offenders to score them — the trait
system, the 410 phantom resources, the conditional whole-object replacement engine, the 117
hand-written info loaders, the combat math shared across four subsystems, the save format
that keys on names, the graphics that page before they initialize, the build that lies about
its own compiler — and we went to rank them.

They all tied for first.

So we abandoned the ranking and entered the only specimen the methodology could honestly
support.

---

## 1. The Entire Codebase — 9001 cg *(out of 100)*

A Civ4 / Caveman2Cosmos mod accreted across more than fifteen years by many hands, most of
them no longer reachable and none of them in agreement. There is no subsystem you can touch
in isolation. Combat math is shared between the UI, the AI, and resolution. The save format
is a name-keyed tag soup. Enums must be registered in two separate files or fail silently at
runtime. The build groups source into unity batches that expose missing includes at random.
Graphics paths run before the engine that backs them exists. And the single most
authoritative-*looking* file in the repository — the project file — describes a compiler the
build does not use.

Every thread, pulled, turns out to be load-bearing for three others. Every "small fix" sits
one semitone from a save-break, an out-of-sync, or a crash that only happens with graphics
paging off. The official engineering guidance, written into `AGENTS.md` in self-defense,
reduces to a single sentence: *nothing here is ever "just a one-liner."*

This is not a flaw in the codebase. It **is** the codebase. Complexity here is not the
exception that earns a place on a list; it is the ambient condition — the medium, the water,
the rule. Which is why this index has exactly one entry, and always will.

*Status: the entire Stones2Stars rework — the JSON data migration (#428), the cascade model,
the AI lift to player level, the combat simplification, the dead-code and dead-XML sweeps —
exists for the single purpose of making this number go down. Progress is counted in Gordian
Knots cut, never untied.*

---

*Scale calibration note: 100 cg = 1 Gordian Knot. The instrument was built to read to 100.
The inaugural measurement returned 9001. The instrument has been retired; the reading stands.*

*Contribution policy: there is no contribution policy. Everything qualifies — that is the
finding.*

*Sibling publications: the [Despair Index](DESPAIR_INDEX.md) (things the code does wrong) and
the [Realism Index](REALISM_INDEX.md) (things the code does exactly as designed, which is
somehow worse).*
