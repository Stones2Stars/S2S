# AI build-queue parity — the AI uses the HUMAN's queue + overflow mechanics

> **Status:** parked intent (owner rulings 2026-07-03, captured verbatim). Not scheduled; belongs to the AI
> rework lane (see [ai-architecture-north-star.md](ai-architecture-north-star.md)).

**The ruling:** *"I want to have AI set up the same kind of build queue, and have overflow instead of 'always
use all your prod' — so it actually simulates human behaviour in this regard."* The asymmetry it kills:
*"humans have 0 say in the matter during 'production time' — the AI does"* — in essence, **"the AI basically
turns production choice into an RTS-style choice instead of a turn-based choice."**

Today the AI re-decides its next build **live, mid-production-phase** — the moment a building completes it
re-evaluates against the freshly-mutated city state and spends every hammer, an ability no human has (a human's
queue is set before the turn resolves; overflow carries). The intent: the AI plans into the **same queue
mechanism** the human uses, production **overflow** carries the same way, and the mid-turn re-decision privilege
disappears.

**The governing principle (owner, same day):** *"we should not allow AI to calculate next build based on just
getting a new building mid-processing, because humans do not get to do that either — they have already gotten
the dump at that point."* Decision INPUTS are turn-boundary state; mid-processing mutations are invisible to
deciders until the next boundary. (This generalizes past production choice — any AI decision that reads
freshly-mutated mid-phase state holds an information privilege no human has.)

Side benefits observed while building the modifier substrate (2026-07-03): the live re-decision is also a
significant read-storm driver (each completion triggers immediate sibling-city rate evaluations — the
`[SLOT]`-measured staleness windows and a chunk of the AI's turn cost). Queue-following AI = fewer, batchable
decision points.
