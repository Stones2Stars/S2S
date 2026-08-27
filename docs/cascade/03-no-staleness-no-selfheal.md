# ⛔ A STALENESS FLAG IS THE FOSSIL OF AN INCOMPLETE EMIT SURFACE — the same rule, one level up

> Part of the **[cascade](../cascade.md)** spec.

> **⛔ AND THE WORD GOES WITH THE MECHANISM — WE DO NOT USE "DIRTY" AS A TERM, FULL STOP.** The only
> survivor is **the one the EXE needs for GRAPHICS**: `InterfaceDirtyBits` and the repaint helpers over it
> (`setDirty(X_DIRTY_BIT)`, `setLayoutDirty`, `setFlagDirty`, `setInfoDirty`) — EXE-bound, and resolved BY NAME
> from BUG config strings, so it is a published vocabulary rather than ours
> ([python-read-map.md](../reference/python-read-map.md)). **Every DERIVED-STATE use goes**, whatever its blast
> radius: the mark/rebuild protocol, `markMaintenanceDirty`, `setCommerceDirty`, the AI re-evaluation flags.
> ⚑ **The word is not being tidied — it is being removed with the thing it names.** A term that survives its
> mechanism is exactly the evidence-of-the-abandoned-path that teaches the next agent to reach for it
> ([leave no evidence of the abandoned path](../../AGENTS.md#design)), and this one names a CLAIM the
> engine can no longer make.
>
> **⛔ AND NEITHER DO WE CALL A READ "HOT" — A PACKAGE READ IS JUST A READ.** *"They are not a hottest
> read, they are just a read."* A read can only be HOT if reading does WORK, so the word asserts there is
> something to recalculate — it smuggles the recompute model back in over code that has none, exactly as "dirty"
> and "cache" do. Under the maintained sum a read is a bare fetch, so its FREQUENCY is not a property worth
> naming: nothing is saved by reading less often and nothing is spent by reading more.
> ⚠ **The tell is a justification, not a slur:** the moment a slot is defended on the grounds that it keeps some
> read cheap, the reasoning has left the model — a slot exists because a FACT applies a delta into it, and that
> is the whole of the argument for it. Performance framing around a package read is how "cache it" comes back.

> **⚖ THE PROTOCOL IS SUPERSEDED, NOT A ROLLERSKATE — full archaeology in [superseded-ideas #30](../architecture/superseded-ideas.md)
> (contrast #14, the ensure-on-read protocol, which genuinely was one).** *"I did not recognize that marking became
> obsolete the moment we landed on eventspine for everything"*: it was correctly designed and faithfully
> built for a world with no unified eventing, and the premise dissolved SILENTLY the moment the spine went
> universal — it kept producing correct numbers at unnecessary cost, with no error or symptom to chase.

**A staleness flag is a CLAIM THAT WE DO NOT KNOW WHAT CHANGED.** Once every mutation announces itself, the FACT
is strictly more informative than the memo — it names the SOURCE, and the compiled index names that source's
deposits — so the flag becomes a lossy summary of an answer already in hand.

⇒ **The mechanical test, and it applies to the whole engine, not just this plane: every staleness bit, staleness
stamp, epoch counter and version number is asserting that what changed is unknowable. Under a complete spine that
assertion is FALSE BY CONSTRUCTION.** So each surviving one is exactly two things and never a third: a **missing
emit wearing a flag** (wire the fact — [an event gap is closed the moment it is found](../spine/03-the-domain-emit-surface-every-fact.md#-a-fact-names-the-happening--something-changed-is-not-a-fact)), or
**dead weight** (delete it). ⛔ It is never a mechanism to keep because it works.

### ⛔ A SELF-HEAL IS THE FOSSIL OF A MISSING EMIT — so it is a SEARCH, not just a ban

**Where self-heal came from:** the old branch was full of blanket recalculations *because agents did not
properly wire the events and shortcut by adding a self-heal calc instead*. That is the causal direction, and it is
what makes [self-heal is not a backstop](#-a-self-heal-is-the-fossil-of-a-missing-emit--so-it-is-a-search-not-just-a-ban) findable rather than merely prohibitive:

> **A recalc does not appear because someone wanted a recalc. It appears because a fact was not announced, the
> value went wrong, and recomputing was the cheapest way to make the symptom stop.**

⇒ **Every self-heal marks the spot where an emit is missing.** So when you find one — a periodic rebuild, a
"refresh if stale", a runaway cap that "recovers next slice", a wipe-and-reapply — do NOT simply delete it and
declare the rule enforced. **Find the fact it was compensating for and wire THAT**; the recalc then has nothing
left to do and is removed as a consequence, not as the fix.

⚠ And a self-heal is worse than the bug it hides, which is why it is banned rather than tolerated: the missed emit
would have surfaced as a visibly wrong value that someone could chase, whereas the recalc converts it into
permanent invisible drift **and** reinstates exactly the per-read/per-turn work the caches exist to delete.
⛔ A comment claiming a recalc "heals" something is itself suspect twice over — the healer may not even exist any
more (a slice rebuild that was since removed), leaving a truncation that repairs nothing and announces nothing.

### ⛔ THE LEGACY-ACCUMULATOR CUT — every accumulator, ONE uniform mechanism

> Binding: [the uniform legacy-accumulator cut](#-the-legacy-accumulator-cut--every-accumulator-one-uniform-mechanism). **NOT wellbeing-specific — it is
> EVERY legacy serialized incremental accumulator, and they all work exactly the same way.** Wellbeing was the pilot
> that proved it; the same cut then repeats UNCHANGED. **Blast radius is not a concern — it is the SIGNAL: a cut
> that does NOT reach broadly means the legacy is not actually being cut.**

**What an accumulator IS — the three-part test.** A member that is ALL of: (1) **serialized** in `read()`/`write()`;
(2) **incrementally maintained** by `change*`/`update*`/`process*` deltas, never recomputed-from-source; (3) a
**per-turn game quantity the cascade now owns**. These are the STORED-ACCUMULATOR DRIFT class
(§2b): they carry decades of save history no live source can reproduce, so a
stored-vs-recompute diff is **DRIFT (history pollution), never state to preserve** — the recompute is the correct side.

**⚑ HOW TO FIND THEM — a MECHANICAL detector, not a reading exercise.** The three-part test above says what an
accumulator IS; this says how to enumerate the ones that have already gone dead, and it needs no judgement:

> **a mutator with NO remaining call site + a member that is still SERIALIZED + a getter that is still READ
> = a consumer being served a FROZEN SAVE VALUE.**

Each leg is a grep. The mutator has no caller because the cascade replaced whatever used to call it (a
`processBuilding` / `processTech` feeder that now deposits instead); the member still deserializes, so the value is
whatever history the save carries; and the getter still has consumers, so that history is what they read. On a NEW
game the same member is simply frozen at zero — the two failure modes look completely different and are one defect.

⚠ **It is invisible from every direction that normally catches things.** It compiles (the getter exists), it runs
(the value is plausible), the compiler census says nothing (no symbol was deleted), and the decomposition
censuses cannot see it (the value is not in a package at all). ⛔ So it is not found by reading code around a bug — it is
found by running the detector over the whole class.

> **⛔ RUN THE THIRD LEG AGAINST THE MEMBER, NEVER ONLY AGAINST THE GETTER — a reader is often named for the
> ANSWER it computes rather than for the member it reads.** The leg asks whether the value is still consumed,
> and a getter-name grep answers a narrower question: it finds the readers that spell the member's stem and
> misses every one that does not. ⚑ Worked, and it changed the disposition rather than merely the count: a
> bonus-keyed city map showed a callerless changer and a callerless getter — apparently a plain two-way
> deletion — while a THIRD function iterated the map directly and carried none of the stem in its name. It too
> turned out to have no callers, so the cut stood; had it had one, the member would have been live and the
> "dead both ways" reading would have deleted a consumed value.
> ⇒ **Grep the MEMBER, take the union of what touches it, and only then ask which legs are dead.** ⚠ This is the
> same class as the two blind spots already known on the getter side (an INLINE header getter, and a getter
> whose name does not contain the member stem) — three faces of one mistake: trusting a NAME to enumerate a
> READ. A fourth face is the Python/`Cy` consumer, which no engine-side grep sees at all.

⚑ **THE DATA SIDE OF THE SAME DEFECT IS THE `unkinded-member` CENSUS**, and the two should be read together: a
family member the parser cannot kind is an authored deposit dropped at load, and the dead accumulator beside it is
the legacy carrier that used to hold that very value. Where they pair, the quantity is missing END TO END — the
data lands nowhere, the carrier is fed by nothing, and the consumers read save history. *(The worked pair: the
authored `hurry.cost` deposits are dropped as unkinded, `CvPlayer::m_iHurryCostModifier` has no writer left, and its
consumers read it regardless.)*

**The uniform mechanism:**

1. Add the cluster's **fresh-gather accessor** returning its term from the cascade, ×100 internally.
2. **Re-point the realized getter** to it, reducing `÷100` at the reader boundary — **no `*Legacy` fallback, no
   variant getter** ([legacy must fail loud, never mask a cascade gap](../specs/validation.md#legacy-must-fail-loud-never-mask-a-cascade-gap): anything sneaking a legacy value
   back in is an ERROR, never a safety net; on a red tree a wrong/empty cascade value is the CORRECT exposed outcome).
3. **HARD-DELETE** the member and its maintainers.
4. **FULL-DELETE the read + write** and NAME the tag in `Assets/savemigration.txt` — the reader drains the orphan
   transparently ([save.md §3](../specs/save.md)). No `WRAPPER_SKIP_ELEMENT`; an UNLISTED deleted-read orphan is the
   one hard desync.
5. **The COMPILER is the census** — every surviving consumer is a compile error to rewire; you cannot
   flip-and-pretend. Done = endpoint-observable on a loaded save, not "it compiles."

⚠ **Audit each deleted `change*`/`update*` BODY for side effects first** — legacy changers carry non-obvious riders
(trade-network recompute, UI-dirty, power) the surviving trigger site must still fire ([save.md §6](../specs/save.md)).

**Incremental-accumulate ledgers convert to recompute-from-source.** A serialized player ledger that replays its
accumulator onto the loaded value double-counts by build order. The conversion is the uniform one above: recompute
from the player's own held sources on the mark, make the changer trigger-only, and have the cities PULL it.

**Event/vote grants are NOT cached — they are a SEPARATELY PERSISTED store.** A per-building commerce change has
two sources of fundamentally different nature: the **empire** grant (`GlobalBuildingExtraCommerces`, civics) is
DERIVABLE → the recompute-from-source cache; the **event/vote** grant (fires ONCE) is **genuine one-shot state, NOT
derivable** — *"having events just be stored in the cache is lunacy"* (a recompute cache would wipe them). They live
in their own serialized field (`CvCity::m_aBuildingCommerceChangeEvents`), outside the recompute path; the reader
sums `player-recompute (empire) + city event/vote (persisted)`.

### ⛔ THE READ IS A BARE FETCH — AND WHAT ONCE STOOD BESIDE IT IS DEAD

The recompute-and-diff endpoint pair, the read-side `ensure()` protocol, and treating a divergence as an in-DLL
HAPPENING are all retired — see [validation.md](../specs/validation.md) for the live THREE-LEG check (the LOGS,
the JSON INFO, and WHAT STATE EXPECTS, all three agreeing) and [superseded-ideas](../architecture/superseded-ideas.md) #14/#19/#33
for why each died. **"The ensures were some of the earliest rollerskates"** — measured: an ensure-per-read
protocol on AI-hot paths ground unit automation. What this section adds, because it binds specifically here:

**A read is a BARE FETCH, unconditionally** — there is no gate test on it, because there is nothing on the read
path to gate.

⛔ **NEVER emit a divergence as a spine event — that is a GUARANTEED LICENSE TO BUILD SELF-HEALING.** An
event is an **invitation to a consumer**. Put a divergence on the spine and the next agent writes the consumer
that "handles" a value known to be wrong by CORRECTING it — self-heal then arrives wearing the authority of the
event spine ([self-heal is not a backstop](#-a-self-heal-is-the-fossil-of-a-missing-emit--so-it-is-a-search-not-just-a-ban)). **A PULL (something a reader asks for) cannot
grow that consumer; a PUSH (an event fanned to whoever registers) grows it by default.** So there is no diff, no
log line, no event and no field for one.

⚠ **And anything that RECOMPUTES in order to check must not be able to write back what it computed** — a
verifier that repairs is self-heal wearing a different hat. That is a property to get from the STRUCTURE (compute
into scratch the caller owns) rather than from a discipline anyone has to remember.

**The identity a divergence needs to be actionable:** "some city's production flats are wrong" across 185 cities
identifies nothing, so every served value carries its owner, **interpreted per scope** as the spine's DOMAIN ints
are interpreted per event: city = `(owner, cityId)` · empire = `(playerId, —)` · team = `(—, teamId)` ·
plot = `(x, y)` (a plot has no owner-independent id, and the map index needs a map that does
not exist at bind). Identity is passed IN at bind — the scope owners share no common id accessor.

⚠ **Consequence, and it is not optional: the STORED side is built by APPLIES ONLY.** The fact that names a source
applies that source's deposits — the same shape the contexts use — and there is no rebuild anywhere for a sweep to
batch (§ THE MAINTAINED SUM).

### ⛔ THE AI PLANE IS NOT EXEMPT — AN AI CACHE IS INVALIDATED BY SPINE FACTS, LIKE ANYTHING ELSE

> *"AI loops should not run the full run all the time, and if we cache the AI data, they should be invalidated
> by the relevant spineEvents like anything else."*

Two halves, and the second is the one that is easy to get wrong. **An AI loop re-running its full pass every
time is the defect** — the same O(what EXISTS) shape the maintained sum deletes everywhere else. **And the
cache that fixes it is an ordinary spine CONSUMER**: it declares the facts that move it and applies them, in
exactly the shape [a context dictionary is a spine consumer](11-context-stores-vs-forwards.md#what-a-context-stores-vs-forwards---a-context-is-an-event-built-store-not-a-forwarding-facade) specifies for every other
store.

⛔ **So a hand-set staleness flag on an AI cache is NOT the sanctioned residual.** The residual
([superseded-ideas](../architecture/superseded-ideas.md) #1) is that the AI may keep its OWN SCORES — it is about WHAT is
cached, never about being excused from HOW every derived store is maintained. A `mark*Stale()` the AI calls
itself is [a staleness flag is the fossil of a missing emit](#-a-staleness-flag-is-the-fossil-of-an-incomplete-emit-surface--the-same-rule-one-level-up) on the AI plane: it asserts we cannot know what
changed, which a saturated emit surface falsifies, and it drifts the moment a new fact moves the value and
nobody remembers to call it.
⇒ **The disposition is the usual one and needs no new mechanism:** name the facts that genuinely move the
score, register for them, and the flag has nothing left to do.

⚑ **AND IT REUSES THE STORE, NOT JUST THE DISCIPLINE: *"there is nothing at all stopping us from
using ContextDict, or something similar, for the AI data, and have them invalidate on the spine events they
care about."*** The AI plane is a tenant of the SAME replacement as everything else
([ContextDict replaces CvDerivedCache](06-spatial-and-contextdict.md#-cvderivedcache-is-replaced-by-contextdict--virtually-everywhere)) -- a keyed store
fed by the facts it declares -- so an AI cache needs no bespoke machinery and gets none.

⚠ **"Or something similar" is the load-bearing half, and collapsing it to "use ContextDict" would be the
conflation this document already warns about** (§ THEY BEHAVE SIMILARLY AND ARE NOT THE SAME, below): what varies
is what the slot HOLDS. `ContextDict` is a REFCOUNT -- `add(id, ±1)`, read `has()`, and **deliberately no `set`**,
because a `set` overwrites a refcount. An AI SCORE is not a refcount: it is REPLACED wholesale when its inputs
move, so it wants a sibling with assignment, not the refcount type with a `set` bolted on.
⇒ **What is shared is the MAINTENANCE RULE and the key space, never the value semantics** -- which is exactly
[every derived store is a keyed accumulator](04-derived-stores.md#-every-derived-store-is-one-shape--a-keyed-accumulator-maintained-by-a-delta)'s own statement that the possession and magnitude
planes are one structure differing in key space and value type. ⛔ Do NOT add `set` to `ContextDict` to make an
AI score fit it.

⚠ **This does not license caching EARLY.** [legacy decache poisons perf measurement](#-legacy-decache-poisons-perf-measurement--and-converts-an-ai-loop-into-a-hang)
sequences it: run uncached, let the hot paths announce themselves, fix the READS that should never have
computed, and only THEN let the AI plane cache its own scores. This rules how that cache is maintained when it
lands, not when it lands.

⚖ **AND UNTIL IT LANDS, THE EXISTING AI VALUATION MEMOS SELF-HEAL — ruled: *"AI valuation should self
heal for now, it is not part of cascade."*** The turn-scoped memo clears (tech values, mission targets, civic
values, build values, unit counts, trade routes, resource consumption) are the sanctioned interim: an AI
VALUATION is a heuristic the asking side owns, not cascade/derived game state, so
[self-heal is not a backstop](#-a-self-heal-is-the-fossil-of-a-missing-emit--so-it-is-a-search-not-just-a-ban) does not reach it at this stage. ⛔ Do not strip the memo
clears meanwhile, and do not convert one onto fact-driven invalidation ahead of the sequencing above — this
section rules the shape the AI cache takes WHEN the plane converts, never that it converts now.

### ⛔ LEGACY DECACHE POISONS PERF MEASUREMENT — AND CONVERTS AN AI LOOP INTO A HANG

**Home of [legacy decache poisons perf measurement](#-legacy-decache-poisons-perf-measurement--and-converts-an-ai-loop-into-a-hang).**

The #430 cut NUKED the serialized accumulators legacy calcs depended on for O(1) reads (`m_iBuildingGoodHappiness`
and its cluster, …). Stripped of those caches, a surviving legacy calc (`happyLevelLegacy`, `badHealthLegacy`, …)
recomputes from scratch on EVERY call — so ANY perf measurement taken while legacy still runs in a read path
measures **legacy's decache penalty, not the cascade** (proven: the unit-selection lag was legacy
`unhappyLevel(iExtra)`/`badHealth(bNoAngry)` what-if re-sums per read; it vanished the instant the getters went
cascade-only). All turn-time/FPS/lag numbers gathered with legacy on any hot read path are POISONED. Clean perf
is only measurable AFTER legacy is fully purged — so the violent purge is a PREREQUISITE for the perf hunt, not
merely a correctness/tidiness step. Sharpens [turn time is king](16-package-model.md#-the-per-scope-package-model--the-cascades-founding-design-1-stated-as-cache-architecture).

⛔ **AND IT DOES NOT ONLY POISON MEASUREMENT — IT CONVERTS AN AI LOOP INTO A HANG: the AI loops "looping
all the things when they don't need to" are a SYMPTOM, and they surface now "because we do not serialize their
caches anymore."** The loops were always shaped this way; every inner read used to hit a serialized accumulator
and cost O(1), so the shape was merely wasteful. Strip the accumulators and each read RECOMPUTES, so an
`O(candidates × cities)` loop becomes `O(candidates × cities × cities)` and stalls outright.

⇒ **Both halves are the fix, and neither alone is:** the READ must be an O(1) maintained slot again
([the maintained sum](05-three-planes.md#-the-maintained-sum--three-planes-one-slot-and-nothing-is-ever-recomputed)), AND the caller must stop asking a scope-wide question per
candidate. ⚑ **Expect MANY** — three surfaced in one session from one root (`AI_isFinancialTrouble` re-walking
every city, `readFlat` doing a tree lookup, `cityReceiverRate` re-walking the plot ring), each found only by
attaching a debugger to a spinning process, because a spin EMITS NOTHING and every log goes silent at once.
⚠ So a hang with a saturated core and dead logs is this class until proven otherwise — and the CPU reading is
per-core, so one pinned core reads as ~0% in Task Manager on a many-core box.

⚖ **AND THE UNCACHED STATE IS AN INSTRUMENT, NOT ONLY A COST: *"it is useful to run through like this
without caching to see where the hottest path is."*** This is the half that inverts the entry above. Behind a
serialized accumulator an `O(n³)` loop is INVISIBLE — it merely costs a slice of every turn forever, and nothing
ever points at it. Strip the accumulator and the same shape becomes a HANG, which is locatable in minutes with a
debugger attach. The decache did not create these; it made them findable.

⇒ **Consequence for sequencing, and it is the actionable half: do NOT hurry caching back in.** Every cache
restored re-blinds the surface it covers, so the order is (1) run uncached, (2) let the hot paths announce
themselves as stalls, (3) fix the READS that should never have computed, (4) only then let the AI plane cache its
own scores, simply ([ai-architecture-north-star.md](../plans/parked/ai-architecture-north-star.md)). A cache
added while a wrong-shaped read is still underneath it hides the read instead of fixing it — the
[self-heal is not a backstop](#-a-self-heal-is-the-fossil-of-a-missing-emit--so-it-is-a-search-not-just-a-ban) failure one plane over.

