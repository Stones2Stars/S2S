# AI build-queue parity — the AI uses the HUMAN's queue + overflow mechanics

> **Status:** parked intent, owner rulings captured verbatim. Not scheduled; belongs to the AI rework lane
> (see [ai-architecture-north-star.md](ai-architecture-north-star.md)).

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

**What remains parked:** production **overflow** carrying the way the human's does, and the retained-scoring
half below (§ THE DOUBLE QUEUE) — which is what makes the mid-processing re-decision privilege disappear
entirely rather than merely becoming rarer.

> **⚖ THE OPEN QUESTION THE DEPTH RAISES — DOES A STANDING BUILDING QUEUE SQUEEZE UNITS OUT (owner)?** The
> depth is what makes the scoring cheap, and it has a cost on the other side: `AI_chooseProduction` is the ONE
> cascade that decides units as well as buildings, and `CvCity::doProduction` only re-enters it when the queue
> EMPTIES. So a city holding three construct orders does not weigh a unit for three completions, and a shallow
> queue — which is what the production-turns budget produced — was implicitly buying responsiveness.
> ⚑ **What decides whether it actually bites is the INSERT path, and that is where a review starts:** unit
> orders mostly APPEND like buildings do, but one site pushes with `bAppend = false` and therefore jumps a
> standing queue. If the urgent cases (defence demand, a contract-broker tender) all reach that path, a deep
> queue defers only DISCRETIONARY units and the risk is small; if they append, they wait behind three buildings.
> ⛔ **It is not settleable on the standing save.** The effect is a shifted unit/building MIX over many turns,
> not a value a turn's census can show, so it needs exposure rather than a measurement — which is why this
> belongs to the pre-ship pass and not to the perf thread that produced the depth.
> ⚑ **AND THE DEPTH IS THE WRONG LEVER FOR THE COST IT WAS REACHING FOR (owner).** Queue depth buys cheap
> scoring by suppressing the DECISION, which is what spends the responsiveness. The scoring is what should be
> retained instead, and the shape that does it is § THE DOUBLE QUEUE below — which is also where this risk
> goes away, since a decision that still runs still weighs units.

## ⚖ THE DOUBLE QUEUE — a retained SCORING stack beside the production queue (owner)

**The shape (owner):** *"a stack, with the highest scoring item on top, that gets popped and pushed to queue
when previous item is finished; we only reevaluate buildings when that stack is empty."* Earlier phrasing of
the same model: *"build processing then uses the CACHE, until all buildings it can has been produced — and
then the cache gets recalced in expectation of the next cycle."* It is
[state-repositories.md](../../architecture/state-repositories.md) § THE AI PLANE IS NOT EXEMPT's sanctioned
residual made concrete for this one consumer — the AI keeping its OWN scores — and it is the successor to the
queue-DEPTH lever above, which bought cheap scoring by suppressing the decision instead of retaining the
score.

⛔ **THE STACK RETAINS THE SCORING, NEVER THE DECISION — and that is what dissolves the unit question above.**
`AI_chooseProduction` is the ONE cascade that weighs units as well as buildings, and `CvCity::doProduction`
re-enters it only when the production queue EMPTIES. A pop that refills the queue by itself means the queue
never empties, so the unit half is never reached at all — the depth's own defect, taken further. So the pop
goes THROUGH the decision: a completion re-enters `AI_chooseProduction` as it does today, and the building
half CONSULTS the stack instead of re-scoring.
⚑ **The cost argument is measured and it is one-sided:** scoring is essentially the whole of a choose, so
keeping the decision costs the sliver that is not scoring while restoring every unit evaluation. Suppressing
it saves that sliver and buys back the unit-squeeze risk this document already records.

⚖ **THE RECALC IS ONCE PER TURN — FOR NOW, AND FRESHNESS IS THE REASON (owner).** The clock is the STAND-IN
for the spine invalidation that does not exist yet: a purely drain-driven stack would outlive its inputs
indefinitely, so the turn boundary is what bounds the staleness until an interest set can.

⛔ **AND THE INVALIDATION IS OUT OF #430 — ITS OWN LANE, NOT A LATER STEP OF THIS ONE (owner): *"I do plan to
introduce eventspine and cache management the same way to the AI side, but that is not for #430, that would
probably end up killing me — 3 months of rollerskate wrangling is enough for 1 go."*** The destination is
named and wanted; its POSITION is the ruling.
⚠ **So the clock is not a short-lived interim — it is the shape for the duration of #430**, and the ~3× below
is a STANDING cost rather than a temporary one. ⇒ Inside #430 the double queue buys FAIRNESS and unit
responsiveness and pays scoring for them; the optimization arrives only in the later lane.
⚠ **This is owner-ruled SEQUENCING with a named end state, so [DEC-no-deferred](../../architecture/decisions.md#dec-no-deferred)
does not reach it** — the same standing as the golden-age / anarchy status carve-out
([state.md](../../specs/state.md)). ⛔ It is equally NOT licence to start the AI-plane spine work opportunistically
while in here.

⚠ **THE CLEAR IS LAZY, so the cost is ~3× today's scoring — not the empire's city count.** A city that
completes nothing in a turn never asks for a building answer and never touches its stack, so the clear costs
one scoring per *(city × turn in which it completed something)*, never one per city per turn. What it does
cost is the AMORTIZATION: a depth-3 queue spreads ONE scoring across THREE completions, while a one-turn
horizon spreads it across at most one turn's completions.
⇒ **So the per-turn form is a FAIRNESS change that costs perf, not a perf change** — it buys units weighed at
EVERY completion and a staleness bound of one turn, and pays back what the depth was amortizing. ⛔ Do not
report it as the optimization; the optimization is the horizon extending, and the structure landing now is
what that plugs into.

⚖ **FRESHNESS BECOMES THE SPINE'S JOB (owner: *"later we can derive ways to have that stack invalidated on
other eventspine events"*), and it is what RETIRES the clock rather than merely refining it.**
⚠ **The risk to design against is an interest set that degenerates to "everything".** A building's score reads
the enabler frontier, the what-if valuation and the empire's standing, so a naive declaration invalidates on
nearly every fact and the stack stops being worth keeping. [patterns.md](../../architecture/patterns.md)'s
cadence ruling points at the bound: key it on the coarse facts that move the CHOICE SET or the empire's
standing, never on anything that twitches within a turn. A slightly stale ORDERING is acceptable — this is an
AI heuristic ([superseded-ideas #1](../../architecture/superseded-ideas.md)), not a cascade value.

**The store shape, pinned by rulings that already exist:**
- ⛔ **A sibling of `ContextDict`, never `ContextDict` itself.** A score is REPLACED wholesale when its inputs
  move, so it wants assignment; the dictionary is a refcount and deliberately has no `set`
  ([state-repositories.md](../../architecture/state-repositories.md) § THE AI PLANE IS NOT EXEMPT).
- ⛔ **Never serialized, and CLEARED in `CvCity::reset()`** — a `CvCity` is recycled out of an
  `FFreeListTrashArray`, so an uncleared slot inherits the previous city's shortlist
  ([DEC-derived-never-trusted](../../architecture/decisions.md#dec-derived-never-trusted); the enabler's
  domains carry the same requirement for the same reason, [enabler.md §8](../../specs/enabler.md)).
- ⚑ **A LAZY rebuild retires the save question the earlier phrasing raised.** *"This is only possible with a
  serialized cache (which we don't want) or a full cache build on load (acceptable)"* — a stack that refills
  whenever it is found empty needs neither: a load starts every city empty and the first choose that asks
  fills it. No eager load-end build is owed, and the per-turn clear needs no load special case either.

⚖ **PRIORITY (owner): *"the calculation itself is now relatively minor, so it is more of a 'medium'
optimization step."*** ⚠ Read that as SCHEDULING WEIGHT, not as a claim about the kind of change: under the
per-turn clock the scoring cost goes UP, so what is being weighed is a fairness gain against it (above).
⛔ Its position in the sequence is unchanged either way —
[DEC-legacy-decache-poisons-perf](../../architecture/decisions.md#dec-legacy-decache-poisons-perf) puts "let
the AI plane cache its own scores" LAST, after the wrong-shaped reads are fixed, and a cache added over one of
those hides it ([DEC-no-self-heal](../../architecture/decisions.md#dec-no-self-heal) one plane over).

**The governing principle (owner, same day):** *"we should not allow AI to calculate next build based on just
getting a new building mid-processing, because humans do not get to do that either — they have already gotten
the dump at that point."* Decision INPUTS are frozen as of the last recalc, so mid-processing mutations are
invisible to deciders — which the stack satisfies by construction, since a pop re-derives nothing. (This
generalizes past production choice — any AI decision that reads freshly-mutated mid-phase state holds an
information privilege no human has.)

Side benefits observed while building the modifier substrate (2026-07-03): the live re-decision is also a
significant read-storm driver (each completion triggers immediate sibling-city rate evaluations — the
`[SLOT]`-measured staleness windows and a chunk of the AI's turn cost). Queue-following AI = fewer, batchable
decision points.
