# AI build-queue parity — the AI uses the HUMAN's queue + overflow mechanics

> **Status:** parked intent (owner rulings 2026-07-03, captured verbatim). Not scheduled; belongs to the AI
> rework lane (see [ai-architecture-north-star.md](ai-architecture-north-star.md)).

**The ruling:** *"I want to have AI set up the same kind of build queue, and have overflow instead of 'always
use all your prod' — so it actually simulates human behaviour in this regard."* The asymmetry it kills:
*"humans have 0 say in the matter during 'production time' — the AI does"* — in essence, **"the AI basically
turns production choice into an RTS-style choice instead of a turn-based choice."**

**⚠ The AI QUEUES — it does not re-decide per completed building.** `CvCityAI::AI_chooseBuilding` walks the
sorted candidate list and APPENDS `ORDER_CONSTRUCT` orders up to `AI_BUILDING_SHORTLIST_DEPTH`, and
`CvCity::doProduction` re-invokes `AI_chooseProduction` only when the queue EMPTIES. So the scoring is paid
ONCE per shortlist rather than once per build, and a city coasts on the shortlist between decisions.

**⛔ THE DEPTH IS A COUNT, NEVER A PRODUCTION-TURNS BUDGET (owner).** A turns budget makes the depth shrink as
a city's output grows — the more production it has, the fewer builds the budget covers, the sooner its queue
empties, and the MORE often it re-decides. Queue depth would be inversely proportional to output, which is the
opposite of what depth is for. The count is taken from the queue's own standing `ORDER_CONSTRUCT` entries, so
each rung of the decision cascade tops up only what is missing and later rungs add nothing once it is full.
⚑ Whether an order LANDED is read off the queue length, never assumed from having asked: `pushOrder` refuses a
candidate its availability gate or duplicate guard rejects, which is how a wonder completed elsewhere falls out
of the shortlist without any second gate at this site ([enabler.md §6](../../specs/enabler.md)).

**What remains parked:** production **overflow** carrying the way the human's does, and the turn-boundary
snapshot half below — a shortlist re-derived at the turn boundary and frozen within the turn, so the mid-turn
re-decision privilege disappears entirely rather than merely becoming rarer.

**The target processing model (owner, same day):** *"build processing then uses the CACHE, until all buildings
it can has been produced — and then the cache gets recalced in expectation of the next cycle."* I.e. reads
inside the production cycle serve the turn-boundary SNAPSHOT (no mid-cycle freshness at all — mutations
accumulate invisibly), and ONE recalc runs at cycle end, priming the next boundary. That inverts today's
dirty-on-mutation/lazy-refresh model into snapshot-then-recalc — the cache becomes the fairness mechanism
itself, not just a perf device.
**Save-safety constraint (owner):** *"this is only possible with a serialized cache (which we don't want) or a
full cache build on load (acceptable)"* — the cycle-end snapshot is load-bearing across turns, so a load must
reproduce it; with serialization ruled out ([DEC-derived-never-trusted](../../architecture/decisions.md#dec-derived-never-trusted)),
the **eager full cache build at load-end** (already the general policy, state-repositories.md) is the
correctness PREREQUISITE of this model, not just a perf trade.

**The governing principle (owner, same day):** *"we should not allow AI to calculate next build based on just
getting a new building mid-processing, because humans do not get to do that either — they have already gotten
the dump at that point."* Decision INPUTS are turn-boundary state; mid-processing mutations are invisible to
deciders until the next boundary. (This generalizes past production choice — any AI decision that reads
freshly-mutated mid-phase state holds an information privilege no human has.)

Side benefits observed while building the modifier substrate (2026-07-03): the live re-decision is also a
significant read-storm driver (each completion triggers immediate sibling-city rate evaluations — the
`[SLOT]`-measured staleness windows and a chunk of the AI's turn cost). Queue-following AI = fewer, batchable
decision points.
