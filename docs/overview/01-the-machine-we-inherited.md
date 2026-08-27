# 1. The machine we inherited

> Part of the **[overview](../overview.md)** spec.

### A compiler from 2003

The game executable is closed and was compiled in 2003, and everything we ship must stay ABI-compatible with
it. So the DLL is built with the Microsoft Visual C++ Toolkit 2003: genuine C++03, 32-bit, against Python 2.4
and Boost 1.32. No `std::thread`, no lambdas, no `auto`, no `nullptr`, and a ~3.2 GB address-space ceiling we
actually hit.

This is a hard compiler limit, not a style preference. It constrains **syntax**. It turns out it does not
constrain architecture nearly as much as everyone assumed. (Details: [reference/engine.md](../reference/engine.md).)

### Data as 570,000 lines of XML

Civ4's data model is XML: a set of `CIV4<Thing>Infos.xml` files, each validated against a hand-written schema,
each loaded into a C++ class with one member per tag. Across this mod that is 63 info files and roughly 570,000
lines of XML.

The C++ side is what decided how much a change cost. `CvBuildingInfo` alone was **4,008 lines with 218 data
members and 292 getters**. Adding one field to a building meant touching the XML, the schema, a member, the
reader, a getter, and usually a Python binding — six places in lockstep for one number.

And the format leaked the implementation. From the real, shipped Forge:

```xml
<YieldModifiers>
    <iYield>0</iYield>
    <iYield>15</iYield>
</YieldModifiers>
```

That is *+15% production*. You can only know it if you know yields are an enum whose second entry is
production — the array is positional, the labels are absent, and a trailing zero is simply omitted. Nothing in
the file says "production".

### The relationships were stored backwards

The Forge's XML entry is 144 lines and contains **no statement of what the Forge unlocks**. The nineteen
buildings it leads to each name the Forge in their own prerequisite list, pointing the other way.

So the question every tech tree, build list, tooltip and AI evaluation actually asks — *what does this
unlock?* — could not be answered by reading the thing. It was answered by scanning every building in the
database and testing each one. That pattern is pervasive; there are still **628** registry-bounded loops in the
AI and engine code today (`python Tools/verify-registry-scans.py`).

### Three ways to say "no longer available"

The same Forge entry carries `ObsoleteTech`, `ObsoletesToBuilding` and `ReplacementBuildings` — three tags for
three different concepts, none stating its own semantics, two actively misleading:

- `ReplacementBuildings` looks like removal and is not. The engine only *disabled* the building while the
  successor existed and re-enabled it when the successor went away. Reversible dormancy, wearing the name of a
  deletion.
- `ObsoletesToBuilding` reads like "what obsoletes me" and means the opposite: *what I turn into* when my own
  tech obsoletes me. A destination, named like a cause.

⚑ Writing this section is what exposed a live contradiction in the shipped data — the successor was listed
*both* as obsoleting the predecessor and as parking it, in **1,521 of 1,522** buildings. Both cannot be true:
obsolescence is checked before the still-running verdict, so it wins and destroys what the chain meant to park.
The rule now is that no building ever obsoletes a building ([enabler.md §2](../specs/enabler.md)).

Requirements were split across two unrelated mechanisms in one entity too: `PrereqBonuses`, a flat list with
implicit OR checked continuously, and `ConstructCondition`, a bespoke nested expression language with its own
vocabulary (`<GOMType>GOM_BONUS</GOMType>`) checked only at build time.

### Derived state nobody could trust

Every derived value — a city's yields, its happiness, its maintenance — was a hand-maintained cache with
ad-hoc, gappy invalidation. A building is constructed and a `processBuilding` routine adds its contributions
into per-city accumulators; it is removed and a matching routine subtracts them. There is no single "the source
changed, refresh me" primitive anywhere.

One disease, many symptoms:

- A building goes dormant and its improvement yield is never decremented — the city keeps producing from
  something that stopped working.
- Two screens report different worked-plot yields for the same city at the same moment, because they walked
  different accumulators.
- Those accumulators are **serialized into the save**, so they carry years of history no live source can
  reproduce. Recomputing and diffing doesn't reveal a bug; it reveals the stored number has been drifting for
  as long as the save has existed, with no way to tell which side is right.
- The workaround for a cache nobody could trust was a getter that recomputes on every read — correct, and
  paying full price on the hottest paths. One was measured at **913 million plot reads in a single turn**
  inside the governor's valuation loop.

### Nothing announced anything

There were interface "dirty" bits — flags telling the UI to repaint, carrying no information about *what*
changed — and a reporting hub that called into Python for a fixed list of happenings, so a fact reaching Python
was invisible to every C++ consumer. Between them, no component could ask "what just changed?" and get an
answer. Which is why every cache had to guess.

### And you could not watch it run

The engine's log files are held open by the process, so they cannot be read while the game runs. There was no
state endpoint and no event stream. To learn what the engine believed, you looked at the screen and inferred —
which makes deleting anything dangerous, because you cannot demonstrate that a replacement does the same job.

### The shape of the problem

> A cache is only necessary when inputs can arrive **unannounced**. Every staleness flag is a formal claim that
> we do not know what changed. If every mutation announced itself, that claim would be false by construction —
> and the flag would be a lossy summary of an answer already in hand.

That sentence is the whole rebuild. Everything below is what it costs to make it true.

### The same five questions

| The question | Before | After |
|---|---|---|
| What does this unlock? | Not stored. Scan every entity and test each one. | A forward edge on the entity — `enables`. |
| Can I build it? | Scattered ad-hoc checks re-scanning the database, answering yes or no. | One two-pass machine over a small frontier, answering *and saying why not*. |
| How much does it produce? | Hand-maintained accumulators, serialized, drifting. | One summed slot per channel per scope, applied by the fact that moved it. |
| What just changed? | Unanswerable. Repaint flags and a Python callback hub. | 161 named facts on one spine. |
| What does the engine believe now? | Look at the screen and infer. | Read it off an HTTP endpoint, a live stream, or a log written while it runs. |

---

