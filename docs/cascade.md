# The cascade — deposits, the maintained sum, and the live-state contexts

The cascade machine that computes **per-turn magnitudes** — a city's food, a unit's strength, a property's
level. Sources **deposit** values; a target reads the **combined total**. It reads the modifier families
authored per the [json spec](specs/json.md) §6; this doc is everything downstream of that authoring: the
deposit flow and combine arithmetic, the conditioning (dormancy) model, the ownership rule that decides *where*
a cross-entity modifier is authored, the **maintained-sum** mechanism that keeps every derived slot correct with
nothing ever marked or recomputed, and the per-scope **contexts** a reader goes to for an entity's live state.

**At heart, a modifier is a [`requires`](specs/enabler.md) gate plus an output.** It uses the *exact same*
condition vocabulary as the enabler — `all`/`any`/`noneOf`, atoms, predicates, scopes — so once `requires` is
nailed the modifier follows for free; the only thing it adds is a **magnitude** to deposit when the gate holds.
So this doc leans on that shared vocabulary (defined in [enabler](specs/enabler.md) / [json](specs/json.md)) and
spends its effort on the **output half**: how a magnitude deposits, accumulates, combines, stays current, and is
read back.

**One owner, one design, stated once.** The deposit machine (formerly `specs/modifier.md`), the maintenance
mechanism that keeps every derived slot correct (formerly `architecture/state-repositories.md`), and the
per-scope live-state read surface (formerly `architecture/contexts.md`) are one concept split across three
files that kept re-establishing the same context for each other. This page is that concept, in one place.

---

## 1. One step: deposit DOWN, accumulate, read O(1)

Where the [enabler](specs/enabler.md) is two passes, the modifier is **one step**: each source drops its deposit
onto a target, the deposits **accumulate**, and the target reads an **O(1) summed total**. No source needs the
whole picture; order doesn't matter (sums are commutative).

Magnitudes flow **DOWN** the scope spine (`world → … → city → plot | unit`). An empire-scope deposit on a civic
rolls down to each of the player's cities; a city-scope deposit lands locally; a `plots`-target deposit lands on
each matching worked plot (§5). The target reads a combined value — it never re-walks the sources.

> **⚖ STORAGE SEMANTICS — the SCOPE PRINCIPLE.** Deposits **ACCUMULATE** in a package **AT THEIR OWN SCOPE** — one
> uniform package format (Σflat / Σpercent per channel, §2) held on each scope object (world / team / empire /
> city / plot), each package **event-MAINTAINED** at its own scope only: the fact names the source, the compiled
> index names that source's deposits, and applying them keeps the slot current with nothing marked or deferred
> ([the maintained sum](#-the-maintained-sum--three-planes-one-slot-and-nothing-is-ever-recomputed)). The downward "roll" is
> realized **AT READ TIME**: the realized value is the trivial sum of the ~5 scope packages, with per-city gates
> (state-religion-in-city, coastal, connected, area membership) applied live at the combine. **A lower
> scope never STORES an upper scope's sums** — that would force downward fan-out and "break
> the principle of the cascade in the first place." LOAD is not a rebuild either: the reseed's in-read facts
> apply through the same path play uses.
> (Mechanics: § THE MAINTAINED SUM, below — the maintained-sum model + the two planes, only one of which is ever
> evaluated.)
>
> **⛔ THE ORIGIN RULE — THE PURE CASCADE DESIGN (owner).** Which half of a package a scope ever fills is not
> incidental, it IS the model:
> - **YIELDS come from exactly three sources — PLOT, SPECIALISTS, and BUILDINGS (city).** Nothing else produces a
>   yield, so the flat/yield side exists at **plot** and **city** only.
> - **MODIFIERS come from everything BUT plot** — city, empire, team, world. The percent side exists at
>   every scope except plot.
>
> Plot and the upper scopes are mirror images (yield-only vs percent-only); **CITY is the one scope carrying
> both**. This is why every scope can hold the SAME package type while many stay half-empty — emptiness is a
> property of the origin rule, never a reason to omit a scope's package or to hand-shape a bespoke struct for it.
> *(The extended form of this rule — the three-package split within CITY itself, the four-provider law, and why
> a trade route is a provider with no package — lives in § THE READ PATH, below.)*
>
> **⚖ WHAT THE RULE GOVERNS — ONLY THE CHANNELS THAT ACTUALLY PRODUCE OUTPUT (owner).** *"Only commerce yields
> and base yields actually produce output."* So the rule binds exactly those: the **base yields**
> (food / production / commerce) and the **commerce yields** (gold / research / culture / espionage). Their flats
> are authored at plot and city only, and none authors a percent at plot — that is the origin rule, in full.
>
> ⛔ **Every other family is NOT output, so it is not bound by it — and this is a CATEGORY difference, not a list
> of exceptions.** **Happiness is the worked case: it is a TRANSIENT STATE, not a yield that produces anything**
> — a condition the city is *in*, which changes how other things behave (growth, anger, food consumption) while
> producing no output of its own. Nothing is *made* by happiness. So "where output originates" simply has no
> claim on it, and wellbeing authoring **flats at EMPIRE and AREA** (the civic/tech/trait grants that roll down,
> §2b) is the model working, not an exception to it. The same holds for **plot-scope PERCENTS** — health's
> feature-fallout class (§2b), defense, the property plane.
>
> ⚖ **PROPERTIES are the honest IN-BETWEEN, and the test does not need to resolve them (owner).** You *could*
> argue a property produces output — a value genuinely accumulates and propagates — but what it ultimately does
> is **affect a transient state**, so it sits between the two. That ambiguity costs nothing here: either way it
> is not an output-producing YIELD, so the origin rule does not bind it (the property plane authors plot-scope
> percents), and the property engine is **self-contained by design** — what happens inside it stays inside it
> ([engine.md](reference/engine.md)), so no classification of it needs to leak outward. ⛔ Do not force
> properties to one side to make the taxonomy tidy; the in-between is the accurate answer.
> ⚠ **"Self-contained" scopes the engine's MATH, never its INPUTS.** Each property is a CHANNEL in this machine
> and the cascade owns which sources apply and what they sum to; the engine owns integrating that rate — decay,
> diffusion, the ordered solver passes ([property-audit.md](plans/structural-cleanup/property-audit.md), the
> governing model). Reading this paragraph as "property sources are the engine's too" is what leaves the source
> side re-derived per turn.
>
> ⚠ **The word "yield" carries TWO senses, and conflating them is what makes this look like a contradiction.**
> [every modifiable number is a yield](#1-one-step-deposit-down-accumulate-read-o1)'s *"every modifiable number is a
> yield"* means **every such number is a CHANNEL in the one machine** — a statement about carriage. The origin
> rule's "yields" means **output-producing yields** — a statement about where output comes from. A family is
> classified by asking *"does this produce output?"*, never by which list it appears on.
>
> **How a non-output family's sides are enforced: BY THE DATA.** Each scope's channel set is **minted from the
> compiled deposits** (KEYS ONLY WHERE NEEDED, § THE READ PATH), so which sides a scope fills is answered at
> load, and a read of a side no source authored answers 0 with no storage existing anywhere. ⛔ **A read-side
> roll-up therefore never hand-gates a scope out of its chain** — the channel set is the gate; a hand-written one
> silently deletes an authored family's contribution, and with no runtime to catch it (the empire wellbeing flats
> are the case that bites: 558 authorings).
> **Consequence (owner requirement): every modifier/yield cache consolidates to ONE shape** — the per-family
> hand-named scalar members (`scGpBaseBld`, `scDefense`, `scMaintModCity`, …) collapse into the same
> Σflat/Σpercent-per-channel form the yields and commerce already use, so a new scope or channel is DATA rather
> than a new struct.

This is purely top-down: a condition *inside* a deposit (`enabled`/`per`) is a forward **read** of state, never
an upward cascade-walk. **The reverse view ("who references/modifies me") is derived once at load, never on a
hot path** — realized as reverse edge FAMILIES on the referenced info object itself, populated by the readJson
reverse pass (`EDGEF_RELATED` = the display/pedia candidate lists the tooltips iterate; `EDGEF_REQUIRED_BY` =
the enabler's requires-reverse-index). After load every info ALREADY CARRIES its reverse lookups; no consumer
builds its own scan or side index ([reverse lookups are populated once, at load](#1-one-step-deposit-down-accumulate-read-o1)).

**Three governing rules:** (a) **purely top-down** — sources deposit DOWN, targets read an O(1) accumulator; the
reverse index is cold-path only. (b) **tech-inflation is a downward DEPOSIT, not an upward gate** — a researched
tech deposits down onto everything below it (cheaper/better); the lower thing never reaches UP with a `hasTech`
gate. (c) **info DATA vs engine MACHINERY is a hard boundary** — the JSON carries only values + relationships;
the producers, evaluators, and tally that consume them are engine-side, so authoring stays declarative.

**Every modifiable number is a yield.** ANY number game mechanics modify — base yields, commerce, free XP, free
specialists, property magnitudes, combat percents, heal rates — is a channel in this ONE machine, carried in the
ONE uniform package format (Σflat / Σpercent per channel per scope; the unit is part of the slot key). A number
still computed by a legacy ad-hoc path outside the machine is a shortcut to fold in
([every modifiable number is a yield](#1-one-step-deposit-down-accumulate-read-o1)).

> ⚠ **Two shapes get mistaken for exemptions from that last sentence; neither is one.** A **PARTIAL leg** (a
> pre-improvement / nature-only yield) is still yield compute — it is a SEGMENT of the scope's own package
> (§ THE CONTEXTS, below), never a per-call walk kept outside the machine because it answers a narrower question.
> A **WHAT-IF** (*"what would this improvement yield here"*) is yield compute too: the what-if plane is a READ of
> this machine (the `expected*` valuations, [patterns.md](architecture/patterns.md)), not licence for a consumer
> to keep its own yield arithmetic. ⇒ The test is the QUESTION, never the caller or the name — a `calculate*` on
> a game object that sums info getters per read is by construction the legacy path this replaces.

**The output-seam.** Where the engine performs placement/application, the machine owns the two ends and the engine
the middle: (1) authored INPUTS are source-centric deposits (a package); (2) placement/application is engine
infrastructure (free-specialist assignment; the golden-age plot-base-yield-threshold "+1"), not modeled; (3) the
OUTPUT yields flow back as a package, consumed exactly like plot yields. Free specialists (amount + forced type →
deposits; engine places; output yields = package) and golden age (length + grant = JSON inputs; plot-threshold
effect = engine middle; extra plot yield = output package) are the exemplars.

---

## The maintained sum — a package is never dirtied and recalculated

> **⚖ THE FOUNDING CORRECTION (owner) — A PACKAGE IS NEVER DIRTIED AND RECALCULATED. IT IS A COMPILED SUM THAT
> IS ALWAYS CURRENT, BECAUSE EVERY EVENT THAT MOVES IT UPDATES IT.** *"What I got wrong is that I thought the
> yield packages had to be marked, and recalculated all the time, when it is in essence just a compiled sum
> that is always updated, based on incoming spine events."*
>
> A package slot is Σ over the scope's sources of their deposit into that `(channel, unit)`. A DOMAIN event
> NAMES the source, and the compiled index already holds that source's deposits — so applying them is a handful
> of adds and the slot is correct **at that instant**. There is nothing to mark, because there is nothing
> deferred. The staleness-flag / recompute protocol this section once specified is RETIRED
> ([superseded-ideas](architecture/superseded-ideas.md) #30); what stands in its place is § THE MAINTAINED SUM
> below.

This is the **design the cascade plane is built to**, stated independently of any one implementation of it. The ONE
uniform package (`Sources/Cascade/CvCascadePackage.h`, channel-indexed Σflat (×100) / Σpercent (unscaled) slots)
is a data member on team / player / city / plot; the per-scope channel sets are minted from the
compiled deposits at load (`CvCascadeChannelRegistry`, the ClassificationRegistry precedent); the package carries
**apply verbs and no other writer**, so the modifier's own spine consumer (`CvModifierConsumer`, load-active)
applies a moved source's deposits directly into the slots they feed; and the combine lives on the calc surface
(`InfoValuation::cityRate` / `groupSumAt`).

### The problem: no unified `dataChanged` trigger

Every derived value in the legacy engine is a **hand-maintained cache with ad-hoc, gappy invalidation**. There is no
single "the source changed, refresh me" primitive, so caches drift out of sync with the data they derive from — one
disease, many instances: a dormant building's improvement-yield never decremented; a building value change leaving
the cache on the old value; transition-only stamps (`doVicinityBonus`) missing build-after-connect orders; two
surfaces reporting different worked-plot yields for the same city at the same moment. The legacy incremental
serialized accumulators additionally carry **history pollution** — values no live data source can produce (the
improvement-yield accumulators hold phantom yields, per-plot, bit-exact; the wellbeing accumulators the same class —
§2b). Recompute-from-source is the cure; recompute-every-read getters (the squirrelBanana class) were the
workaround for a cache nobody could trust — correct, but paying full cost on the hot path.

### The model

> **the state changes → the fact is emitted → the fact's own source updates the slot it feeds → every consumer
> reads that one value as a bare fetch.** One announcement, one application, one source of truth.

A derived cache in this model is:

1. **EVENT-MAINTAINED, not mark-driven.** The DOMAIN fact names the SOURCE; the compiled index names that
   source's deposits; applying them IS the maintenance. A READ is a **bare fetch** and never recomputes
   (an ensure-on-read protocol is tombstoned), and there is no staleness flag on the unconditioned plane to gate
   anything on. **The work is proportional to what CHANGED (one source's handful of deposits), never to what
   EXISTS (every source at the scope).**
2. **Recompute-only, NOT serialized** — the [derived data is never trusted from a save](specs/save.md#5-derived-data-serializes-nothing-) rule,
   applied per-field. Neither the value nor the flag is saved; on load the flag is marked by default, so the first read
   recomputes from current state — **never stale-from-save**. Drop serialization by the **soft-remove**
   ([the soft-remove save discipline](specs/save.md#3-removing-a-serialized-field--the-soft-remove-via-assetssavemigrationtxt-), [save.md §3](specs/save.md)): FULL-DELETE the
   read + write and NAME the tag in `Assets/savemigration.txt`, which drains an old save's orphan bytes by name so
   nothing after it shifts (a no-op on a new save that never wrote it). **No `WRAPPER_SKIP_ELEMENT`** (it leaves the
   dead member named — a rollerskate target); and just deleting the read/write *without* the `savemigration.txt` entry
   desyncs the whole downstream read.
   **This is UNIVERSAL, not per-field-optional (owner ruling): NO cache is ever serialized** — so nothing derived
   is ever read from a save, and there is correspondingly nothing for a blanket recompute to purge.
   ⛔ **No blanket recompute of derived state exists anywhere in the engine, and none is ever to be built**
   ([self-heal is not a backstop](#-a-self-heal-is-the-fossil-of-a-missing-emit--so-it-is-a-search-not-just-a-ban)): the event spine builds the state, LOAD is the only
   full pass (§ THE CAPSTONE RULE, below), and a missed invalidation must stay visible instead of being swept away. ⛔ A
   wipe-the-totals-and-reapply pass over live game objects is therefore never a maintenance path to add or extend
   — *"it is inherently obsolete under the event-driven system, since the new system recalcs on load anyway"*
   (owner). It is the exact shape this model replaces, and it is worst where it looks most useful: firing on the
   saves most likely to have drifted is what would hide the missed emits the spine exists to expose. Each
   remaining serialized cache converts by the same move: skip the read, rebuild at load from source state through
   the live entry points (the bonus-network cluster — the plot-group counts AND membership, the bonus-fed
   wellbeing/modifier accumulators, power, the dormancy verdicts — is the realized exemplar: the load-end rebuild
   in `CvGame::onFinalInitialized` recolors the groups from current state, folds the counts as each plot joins,
   and reconciles dormancy to the enabler's operate fixpoint, firing the ordinary crossing emits; the city holds
   no bonus mirror at all — its read is a plot-group relay, [enabler.md §8](specs/enabler.md)). A serialized
   store survives ONLY for genuine non-derivable state (the event/WB grant stores, e.g.
   `CvCity::m_paiFreeBonusEvents`).
3. **The single source — PULL, not push, and the rule is CROSS-SCOPE.** ⛔ **What is banned is a scope pushing
   its total into ANOTHER scope's store**: an upper scope's package never lands in a lower one, and a receiver
   total is the Σ of its MEMBERS' realized values read at the receiver, never deltas the members push upward.
   That is where "push + a parallel cache double-count and drift" actually bites — two stores holding one fact,
   one of them maintained by someone else.
   ⚑ **A deposit landing in its OWN scope's slot is NOT that, and reading it as that is the misreading this
   clause exists to prevent.** By the scope principle (§1) a city-scope deposit BELONGS at city scope; the
   package is the only thing holding it, so there is no parallel cache to diverge from and nothing to
   double-count. Applying the fact to the slot the fact feeds is the maintenance, not a push.

**Worked shape (the plot-yield cache):** `getYield()` = `return cached` — a bare fetch, always O(1), because the
fact that moved the plot already applied its deposits into the slot;
**⚖ A CROSS-SCOPE RECEIVER TOTAL IS RE-SUMMED AT READ, AND NO SLOT HOLDS IT (owner).** *"I believe it will cost
more to cache such a number in most cases than it would to just do the sum of all cities."* Each channel has ONE
consuming scope (production → city; the commerces further up), and where that scope is ABOVE its members the
total is the Σ of their realized values, taken at the read — there is no `sum` slot, no `readSum`, and no
`applySum`, and none is to be built.
⚑ **The arithmetic is why, not thrift:** a member's realized value is the §2a combine, which is NOT linear in the
deposits, so a cached total could not be moved by a deposit delta at all — it would have to be re-derived on
every fact that touches any member, which is strictly more work than summing the members when someone asks.
⚖ **THE THRESHOLD, so this is re-derivable rather than remembered (owner): *"I do not think for a million years
it would ever be worth caching a value that loops X cities for 1 number and sums it, unless the number of cities
is in the thousands."*** An empire holds tens of cities, so the Σ is tens of adds over values each member already
holds. ⛔ That bar is nowhere near met, and it applies to a HAND-ROLLED bank of the same number just as much as
to a package slot — caching it anywhere is the move being refused, not merely caching it in the cascade.
⚑ **And the VOLATILITY settles it independently of the count (owner): *"especially a number like a commerce
yield, that pretty much constantly fluctuates."*** A cache pays off in proportion to how long an entry stays
valid; a commerce yield moves on nearly every fact in the economy, so a stored total would be re-derived about
as often as it is read and would spend the rest of its life WRONG. ⇒ The two tests compose: cache-worthiness
needs both a large member count and a stable value, and a receiver total has neither. ⚠ A staler variant — a
once-per-turn snapshot of the same Σ — is the worse answer, not the safer one: it trades the cost for a value
that is knowingly out of date on a number that never stops moving.
⛔ So the cost of a receiver read is the MEMBER COUNT, and that is accepted. What is NOT accepted is asking it
per candidate in a scoring loop ([patterns.md](architecture/patterns.md) § THE VALUATION PROTOCOL: a how-valuable
weight is asked at most once per turn) — the cadence is the defect, never the Σ.

⛔ **What is banned is a HAND-NAMED field holding that same number** — a `CvCity::m_plotYieldSum`-shaped member is
the defect [every derived cache is one shape](#-every-derived-store-is-one-shape--a-keyed-accumulator-maintained-by-a-delta-owner) names (it cannot be addressed by the
derived mask, so it forces a bespoke invalidation path) and a second maintenance surface for a fact the modifier
consumer already routes. ⛔ Equally banned is the other direction: re-summing per read. **Cache it — in the slot
that already exists.** The push-maintained `m_aiBaseYieldRate` is dead, and a legacy tier-1 accessor over it
(`getPlotYield`) is a DELETION, not a value to re-home: its consumers read the channel at its receiving scope
([build a new getter surface, never widen a legacy one](architecture/patterns.md#-the-two-read-roles--one-grammar-two-answers-owner)). ⛔ The pull must be a CACHE at EVERY level, never a per-read walk: re-summing the radius on every
`getPlotYield` call turns the game's hottest read O(radius) — measured at 913M plot reads in one turn inside the
governor's valuation, the cost class this whole doc exists to prevent. The engine's actual base yield thereby equals the build-order-independent value the cascade computes —
stale-cache divergences resolved **at the source**, behaviour-preserving
([the completeness+attribution bar](specs/validation.md#the-observation-surface)).

### ⛔ A STALENESS FLAG IS THE FOSSIL OF AN INCOMPLETE EMIT SURFACE — the same rule, one level up

> **⛔ AND THE WORD GOES WITH THE MECHANISM — WE DO NOT USE "DIRTY" AS A TERM, FULL STOP (owner).** The only
> survivor is **the one the EXE needs for GRAPHICS**: `InterfaceDirtyBits` and the repaint helpers over it
> (`setDirty(X_DIRTY_BIT)`, `setLayoutDirty`, `setFlagDirty`, `setInfoDirty`) — EXE-bound, and resolved BY NAME
> from BUG config strings, so it is a published vocabulary rather than ours
> ([python-read-map.md](reference/python-read-map.md)). **Every DERIVED-STATE use goes**, whatever its blast
> radius: the mark/rebuild protocol, `markMaintenanceDirty`, `setCommerceDirty`, the AI re-evaluation flags.
> ⚑ **The word is not being tidied — it is being removed with the thing it names.** A term that survives its
> mechanism is exactly the evidence-of-the-abandoned-path that teaches the next agent to reach for it
> ([leave no evidence of the abandoned path](../AGENTS.md#design)), and this one names a CLAIM the
> engine can no longer make.
>
> **⛔ AND NEITHER DO WE CALL A READ "HOT" (owner) — A PACKAGE READ IS JUST A READ.** *"They are not a hottest
> read, they are just a read."* A read can only be HOT if reading does WORK, so the word asserts there is
> something to recalculate — it smuggles the recompute model back in over code that has none, exactly as "dirty"
> and "cache" do. Under the maintained sum a read is a bare fetch, so its FREQUENCY is not a property worth
> naming: nothing is saved by reading less often and nothing is spent by reading more.
> ⚠ **The tell is a justification, not a slur:** the moment a slot is defended on the grounds that it keeps some
> read cheap, the reasoning has left the model — a slot exists because a FACT applies a delta into it, and that
> is the whole of the argument for it. Performance framing around a package read is how "cache it" comes back.

> **⚖ THE PROTOCOL IS SUPERSEDED, NOT A ROLLERSKATE — full archaeology in [superseded-ideas #30](architecture/superseded-ideas.md)
> (contrast #14, the ensure-on-read protocol, which genuinely was one).** *"I did not recognize that marking became
> obsolete the moment we landed on eventspine for everything"* (owner): it was correctly designed and faithfully
> built for a world with no unified eventing, and the premise dissolved SILENTLY the moment the spine went
> universal — it kept producing correct numbers at unnecessary cost, with no error or symptom to chase.

**A staleness flag is a CLAIM THAT WE DO NOT KNOW WHAT CHANGED.** Once every mutation announces itself, the FACT
is strictly more informative than the memo — it names the SOURCE, and the compiled index names that source's
deposits — so the flag becomes a lossy summary of an answer already in hand.

⇒ **The mechanical test, and it applies to the whole engine, not just this plane: every staleness bit, staleness
stamp, epoch counter and version number is asserting that what changed is unknowable. Under a complete spine that
assertion is FALSE BY CONSTRUCTION.** So each surviving one is exactly two things and never a third: a **missing
emit wearing a flag** (wire the fact — [an event gap is closed the moment it is found](spine.md#-a-fact-names-the-happening--something-changed-is-not-a-fact-owner)), or
**dead weight** (delete it). ⛔ It is never a mechanism to keep because it works.

### ⛔ A SELF-HEAL IS THE FOSSIL OF A MISSING EMIT — so it is a SEARCH, not just a ban

**Where self-heal came from (owner):** the old branch was full of blanket recalculations *because agents did not
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
   variant getter** ([legacy must fail loud, never mask a cascade gap](specs/validation.md#legacy-must-fail-loud-never-mask-a-cascade-gap): anything sneaking a legacy value
   back in is an ERROR, never a safety net; on a red tree a wrong/empty cascade value is the CORRECT exposed outcome).
3. **HARD-DELETE** the member and its maintainers.
4. **FULL-DELETE the read + write** and NAME the tag in `Assets/savemigration.txt` — the reader drains the orphan
   transparently ([save.md §3](specs/save.md)). No `WRAPPER_SKIP_ELEMENT`; an UNLISTED deleted-read orphan is the
   one hard desync.
5. **The COMPILER is the census** — every surviving consumer is a compile error to rewire; you cannot
   flip-and-pretend. Done = endpoint-observable on a loaded save, not "it compiles."

⚠ **Audit each deleted `change*`/`update*` BODY for side effects first** — legacy changers carry non-obvious riders
(trade-network recompute, UI-dirty, power) the surviving trigger site must still fire ([save.md §6](specs/save.md)).

**Incremental-accumulate ledgers convert to recompute-from-source.** A serialized player ledger that replays its
accumulator onto the loaded value double-counts by build order. The conversion is the uniform one above: recompute
from the player's own held sources on the mark, make the changer trigger-only, and have the cities PULL it.

**Event/vote grants are NOT cached — they are a SEPARATELY PERSISTED store.** A per-building commerce change has
two sources of fundamentally different nature: the **empire** grant (`GlobalBuildingExtraCommerces`, civics) is
DERIVABLE → the recompute-from-source cache; the **event/vote** grant (fires ONCE) is **genuine one-shot state, NOT
derivable** — *"having events just be stored in the cache is lunacy"* (a recompute cache would wipe them). They live
in their own serialized field (`CvCity::m_aBuildingCommerceChangeEvents`), outside the recompute path; the reader
sums `player-recompute (empire) + city event/vote (persisted)`.

### ⛔ THE READ IS A BARE FETCH — AND WHAT ONCE STOOD BESIDE IT IS DEAD (owner)

The recompute-and-diff endpoint pair, the read-side `ensure()` protocol, and treating a divergence as an in-DLL
HAPPENING are all retired — see [validation.md](specs/validation.md) for the live THREE-LEG check (the LOGS,
the JSON INFO, and WHAT STATE EXPECTS, all three agreeing) and [superseded-ideas](architecture/superseded-ideas.md) #14/#19/#33
for why each died. **"The ensures were some of the earliest rollerskates"** — measured: an ensure-per-read
protocol on AI-hot paths ground unit automation. What this section adds, because it binds specifically here:

**A read is a BARE FETCH, unconditionally** — there is no gate test on it, because there is nothing on the read
path to gate.

⛔ **NEVER emit a divergence as a spine event — that is a GUARANTEED LICENSE TO BUILD SELF-HEALING (owner).** An
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

### ⛔ THE AI PLANE IS NOT EXEMPT — AN AI CACHE IS INVALIDATED BY SPINE FACTS, LIKE ANYTHING ELSE (owner)

> *"AI loops should not run the full run all the time, and if we cache the AI data, they should be invalidated
> by the relevant spineEvents like anything else."*

Two halves, and the second is the one that is easy to get wrong. **An AI loop re-running its full pass every
time is the defect** — the same O(what EXISTS) shape the maintained sum deletes everywhere else. **And the
cache that fixes it is an ordinary spine CONSUMER**: it declares the facts that move it and applies them, in
exactly the shape [a context dictionary is a spine consumer](#what-a-context-stores-vs-forwards---a-context-is-an-event-built-store-not-a-forwarding-facade-owner) specifies for every other
store.

⛔ **So a hand-set staleness flag on an AI cache is NOT the sanctioned residual.** The residual
([superseded-ideas](architecture/superseded-ideas.md) #1) is that the AI may keep its OWN SCORES — it is about WHAT is
cached, never about being excused from HOW every derived store is maintained. A `mark*Stale()` the AI calls
itself is [a staleness flag is the fossil of a missing emit](#-a-staleness-flag-is-the-fossil-of-an-incomplete-emit-surface--the-same-rule-one-level-up) on the AI plane: it asserts we cannot know what
changed, which a saturated emit surface falsifies, and it drifts the moment a new fact moves the value and
nobody remembers to call it.
⇒ **The disposition is the usual one and needs no new mechanism:** name the facts that genuinely move the
score, register for them, and the flag has nothing left to do.

⚑ **AND IT REUSES THE STORE, NOT JUST THE DISCIPLINE (owner): *"there is nothing at all stopping us from
using ContextDict, or something similar, for the AI data, and have them invalidate on the spine events they
care about."*** The AI plane is a tenant of the SAME replacement as everything else
([ContextDict replaces CvDerivedCache](#-cvderivedcache-is-replaced-by-contextdict--virtually-everywhere-owner)) -- a keyed store
fed by the facts it declares -- so an AI cache needs no bespoke machinery and gets none.

⚠ **"Or something similar" is the load-bearing half, and collapsing it to "use ContextDict" would be the
conflation this document already warns about** (§ THEY BEHAVE SIMILARLY AND ARE NOT THE SAME, below): what varies
is what the slot HOLDS. `ContextDict` is a REFCOUNT -- `add(id, ±1)`, read `has()`, and **deliberately no `set`**,
because a `set` overwrites a refcount. An AI SCORE is not a refcount: it is REPLACED wholesale when its inputs
move, so it wants a sibling with assignment, not the refcount type with a `set` bolted on.
⇒ **What is shared is the MAINTENANCE RULE and the key space, never the value semantics** -- which is exactly
[every derived store is a keyed accumulator](#-every-derived-store-is-one-shape--a-keyed-accumulator-maintained-by-a-delta-owner)'s own statement that the possession and magnitude
planes are one structure differing in key space and value type. ⛔ Do NOT add `set` to `ContextDict` to make an
AI score fit it.

⚠ **This does not license caching EARLY.** [legacy decache poisons perf measurement](#-legacy-decache-poisons-perf-measurement--and-converts-an-ai-loop-into-a-hang-owner)
sequences it: run uncached, let the hot paths announce themselves, fix the READS that should never have
computed, and only THEN let the AI plane cache its own scores. This rules how that cache is maintained when it
lands, not when it lands.

⚖ **AND UNTIL IT LANDS, THE EXISTING AI VALUATION MEMOS SELF-HEAL — ruled (owner): *"AI valuation should self
heal for now, it is not part of cascade."*** The turn-scoped memo clears (tech values, mission targets, civic
values, build values, unit counts, trade routes, resource consumption) are the sanctioned interim: an AI
VALUATION is a heuristic the asking side owns, not cascade/derived game state, so
[self-heal is not a backstop](#-a-self-heal-is-the-fossil-of-a-missing-emit--so-it-is-a-search-not-just-a-ban) does not reach it at this stage. ⛔ Do not strip the memo
clears meanwhile, and do not convert one onto fact-driven invalidation ahead of the sequencing above — this
section rules the shape the AI cache takes WHEN the plane converts, never that it converts now.

### ⛔ LEGACY DECACHE POISONS PERF MEASUREMENT — AND CONVERTS AN AI LOOP INTO A HANG (owner)

**Home of [legacy decache poisons perf measurement](#-legacy-decache-poisons-perf-measurement--and-converts-an-ai-loop-into-a-hang-owner).**

The #430 cut NUKED the serialized accumulators legacy calcs depended on for O(1) reads (`m_iBuildingGoodHappiness`
and its cluster, …). Stripped of those caches, a surviving legacy calc (`happyLevelLegacy`, `badHealthLegacy`, …)
recomputes from scratch on EVERY call — so ANY perf measurement taken while legacy still runs in a read path
measures **legacy's decache penalty, not the cascade** (proven: the unit-selection lag was legacy
`unhappyLevel(iExtra)`/`badHealth(bNoAngry)` what-if re-sums per read; it vanished the instant the getters went
cascade-only). All turn-time/FPS/lag numbers gathered with legacy on any hot read path are POISONED. Clean perf
is only measurable AFTER legacy is fully purged — so the violent purge is a PREREQUISITE for the perf hunt, not
merely a correctness/tidiness step. Sharpens [turn time is king](#-the-per-scope-package-model--the-cascades-founding-design-1-stated-as-cache-architecture).

⛔ **AND IT DOES NOT ONLY POISON MEASUREMENT — IT CONVERTS AN AI LOOP INTO A HANG (owner): the AI loops "looping
all the things when they don't need to" are a SYMPTOM, and they surface now "because we do not serialize their
caches anymore."** The loops were always shaped this way; every inner read used to hit a serialized accumulator
and cost O(1), so the shape was merely wasteful. Strip the accumulators and each read RECOMPUTES, so an
`O(candidates × cities)` loop becomes `O(candidates × cities × cities)` and stalls outright.

⇒ **Both halves are the fix, and neither alone is:** the READ must be an O(1) maintained slot again
([the maintained sum](#-the-maintained-sum--three-planes-one-slot-and-nothing-is-ever-recomputed)), AND the caller must stop asking a scope-wide question per
candidate. ⚑ **Expect MANY** — three surfaced in one session from one root (`AI_isFinancialTrouble` re-walking
every city, `readFlat` doing a tree lookup, `cityReceiverRate` re-walking the plot ring), each found only by
attaching a debugger to a spinning process, because a spin EMITS NOTHING and every log goes silent at once.
⚠ So a hang with a saturated core and dead logs is this class until proven otherwise — and the CPU reading is
per-core, so one pinned core reads as ~0% in Task Manager on a many-core box.

⚖ **AND THE UNCACHED STATE IS AN INSTRUMENT, NOT ONLY A COST (owner): *"it is useful to run through like this
without caching to see where the hottest path is."*** This is the half that inverts the entry above. Behind a
serialized accumulator an `O(n³)` loop is INVISIBLE — it merely costs a slice of every turn forever, and nothing
ever points at it. Strip the accumulator and the same shape becomes a HANG, which is locatable in minutes with a
debugger attach. The decache did not create these; it made them findable.

⇒ **Consequence for sequencing, and it is the actionable half: do NOT hurry caching back in.** Every cache
restored re-blinds the surface it covers, so the order is (1) run uncached, (2) let the hot paths announce
themselves as stalls, (3) fix the READS that should never have computed, (4) only then let the AI plane cache its
own scores, simply ([ai-architecture-north-star.md](plans/parked/ai-architecture-north-star.md)). A cache
added while a wrong-shaped read is still underneath it hides the read instead of fixing it — the
[self-heal is not a backstop](#-a-self-heal-is-the-fossil-of-a-missing-emit--so-it-is-a-search-not-just-a-ban) failure one plane over.

## ⚖ EVERY DERIVED STORE IS ONE SHAPE — a KEYED ACCUMULATOR maintained by a delta (owner)

**A count is a sum.** The possession plane and the magnitude plane are not two mechanisms — they are one
structure over different payloads, and the only things that vary are the key space and the value type:

| store | key → value | the delta arrives from |
|---|---|---|
| the plot group's bonuses | `id → count` | a member plot/city joining or leaving |
| `CityContext.amenities` | `id → count` | a grantor starting or stopping conferring |
| `CityContext`'s vicinity tiers (all/owned/foreign/worked/onSite) | `id → count` | a radius plot's bonus, ownership or served-resource verdict moving |
| `EmpireContext.policies` | `id → count` | a civic / trait / project / wonder |
| the enabler's membership planes | `id → (enable, remove)` | a HAVE-change |
| `OperatingBuildings::providedCount` | `id → count` | an active flip |
| **the cascade packages** | `channel → Σvalue` | a source's compiled deposits |

⇒ **`ContextDict` and `CvCascadePackage` share a MAINTENANCE RULE**, so
[the maintained sum](#-the-maintained-sum--three-planes-one-slot-and-nothing-is-ever-recomputed) is the MAGNITUDE case of one general rule, never a
cascade-only one.

> **⛔ THEY BEHAVE SIMILARLY AND ARE NOT THE SAME — sharing a mechanism is not sharing an identity (owner).**
> The rule above governs HOW a derived store stays current. It says NOTHING about which store a value belongs
> in, and reading it as licence to merge them is the conflation this callout exists to stop:
>
> | | context dictionary | package channel |
> |---|---|---|
> | the KEY is | a minted **classification** id — a named FEATURE | a minted **cascade channel** — a named QUANTITY |
> | the VALUE is | grantors present, or a held strength | a summed magnitude in a unit |
> | the SCALE rule | none — a count is a count | [the ×100 fixed-point model](specs/curators/fixed-point-and-scales.md#1-the-model--integer-100-for-amounts-human-only-at-the-in-and-out-boundaries): flats ×100, percents unscaled |
> | READ by | gates, conditions, `per` scalers | the combine, the realized value |
> | AUTHORED in | the `amenities` / classification block ([json.md §8](specs/json.md)) | a family address `<family>.<scope>.<unit>` |
>
> ⚠ **The SCALE row is what bites silently if they merge** — ×100 semantics landing on a refcount, or dropping
> off a magnitude, both staying entirely plausible.
> ⚑ **The worked case: AIRLIFT CAPACITY.** A building's airlift is a NUMBER it carries and the city's total is a
> SUM of numbers — so it is a modifier-family CHANNEL and retires onto the city's PACKAGE, exactly like the other
> hand-named scalars ([every derived cache is one shape](#-every-derived-store-is-one-shape--a-keyed-accumulator-maintained-by-a-delta-owner)). ⛔ It is NOT an amenity,
> however volumetric it looks: putting it in the dictionary would make an `AMENITY_*` id carry a magnitude and
> break what that registry means.
> ⚠ **Consequence for the volumetric headroom, stated so it is not mis-planned:** power becoming a CAPACITY a
> city draws against would not be an amenity carrying a magnitude — it would be power CHANGING PLANES, from a
> classification key to a channel. Do not "future-proof" the dictionary for a change that would relocate the
> value. ⛔ [every derived cache is one shape](#-every-derived-store-is-one-shape--a-keyed-accumulator-maintained-by-a-delta-owner)'s scope of *"every derived
cache on the cascade plane"* was drawn too narrowly: the plane boundary is not real, and the one store that
drifted onto a different mechanism is the one that boundary excluded.

### ⛔ THE SEMIBOOLEAN STATE — the read is BOOLEAN, the storage is NOT (owner)

**That mismatch IS the trap: storing the thing as what it READS like is the whole error.** The contract:

- **STORED** `id → count`, an int.
- **READ** `has(id)` ≡ `count > 0`.
- **WRITTEN** ±1 as a grantor starts or stops participating — never `set`, never `clear`, never a recount.
- **ZEROED at owner reset** — a delta store is correct only from a known zero, and `CvCity` is recycled out of an
  `FFreeListTrashArray`, so a reused slot inherits the previous occupant's counts and **no later delta can ever
  correct them** (§ THE CONTEXTS, below).

⛔ **THE READ SURFACE IS A BOOLEAN GETTER, AND CONSUMERS NEVER SEE THE INT (owner).** *"The dictionary literally
needs to have a boolean getter that says whether it's there."* `has(id)` ≡ `count > 0` IS the contract; the count
exists so MAINTENANCE can be correct, not so a reader can inspect it. ⚠ The moment a consumer reads the number
the representation leaks — `count == 1` / `count > 2` logic appears, and then **volumetric can never land**,
because changing what the number MEANS breaks readers that were never meant to see it. The one legitimate reader
of the int is the genuinely volumetric one.
⇒ **The surface: `has(id)` → bool for every consumer · `add(id, ±1)` for maintenance · `count(id)` reserved for
a volumetric reader · and NO `set`.**
⛔ **`set(id, n)` IS THE FOOTGUN AND DOES NOT BELONG ON THIS TYPE** — it overwrites a refcount, so a key that
several grantors confer is cleared by the first one to leave. The live case is the THIRD RING
(`CLS_AMENITY_ADDS_3RD_RING`, read through `CvCity::hasThirdRing`): several buildings confer it, so an assignment
would shrink a city's workable radius the moment it lost ONE of TWO grantors, where the refcount keeps the ring.
A type that PERMITS the banned move forces the rule to be remembered; removing the verb makes it
unsayable, which is the enforcement model this project keeps choosing
([patterns.md](architecture/patterns.md): a contract, not a prohibition).

⛔ **ALWAYS A COUNT, NEVER A BIT — and the deciding argument is not "some keys have several grantors."** It is
that **you can never safely answer NO**: these registries are OPEN by design
([the classification-infos registry](specs/json.md#8-classification--unit-skillstagsstate-building-attributes--empire-capabilities)), so a key with one grantor today gains a
second the moment someone AUTHORS data, with no engine change. A bitset breaks silently on a data edit, in a
build nobody touched. The count is not a concession to the multi-grantor cases; it is the only representation
that survives an open registry.
⚑ Two properties fall out free, and both are already ruled for the amenity instance: **VOLUMETRIC needs no
reshape** (the slot is already an int, so a state that becomes a QUANTITY only changes what the number means),
and the **REMOVAL-WINS trap is structurally absent**.
⚠ **The masking to recognise:** a set-shaped store survives only while something RECOMPUTES it whole. Convert
such a store to delta maintenance without converting its STORAGE and it breaks immediately — so the two halves
land together or not at all.

## ⚖ THE MAINTAINED SUM — THREE PLANES, ONE SLOT, AND NOTHING IS EVER RECOMPUTED

Every slot is one identity, and reading it settles the whole maintenance question:

> **`slot` = Σ over the scope's LIVE sources `S`, over `S`'s compiled deposits `d`, of
> `value(d) × multiplier(S) × perScale(d) × [condition(d) holds]`**

**All four operands are ALREADY MAINTAINED BY AN EVENT.** `value(d)` is compiled at load (the deposit index);
`multiplier(S)` and `perScale(d)` are COUNTS the game objects and the context dictionaries (§ THE CONTEXTS,
below) hold; the condition verdict reads the contexts' own stored predicate state. Nothing on the right-hand side
arrives unannounced, so there is nothing left for a recompute to discover —
[a staleness flag is the fossil of a missing emit](#-a-staleness-flag-is-the-fossil-of-an-incomplete-emit-surface--the-same-rule-one-level-up)'s test applied to the VALUE plane rather than
to a flag.

The compiled data splits every deposit by WHICH OPERAND VARIES, and the split decides its **ROUTE, never its
storage**. ⛔ All three planes apply into the SAME slot: there is no per-plane segment and no per-source
decomposition (§ THE CROSS-SCOPE RECEIVER, below, bans one, and this shape needs none — that it adds no storage
at all is the tell that the cut is drawn in the right place).

| plane | the deposit | the fact that moves it | the delta applied |
|---|---|---|---|
| **A — CONSTANT** | null-condition, unscaled | the SOURCE arriving or leaving | `±value` |
| **B — SCALED** | `value × count(key)` — a `per` scaler, a `plots`-target, a keyed count | the source, **and the COUNT** | source: `±value × count` · **count: `±value × Δcount`** |
| **C — CONDITIONED** | gated on a predicate | the source, **and the ATOM's verdict crossing** | `±value`, over the deposits that atom gates |

⚑ **PLANE B IS WHAT THE DICTIONARIES BUY, AND IT IS WHY A COUNT FACT EXISTS AT ALL (owner).** `Δ(v × c) = v × Δc`
is EXACT — `v` is a compiled constant and `Δc` is what the fact carries — so a `ContextDict::add(id, ±1)` IS a
yield delta of `Σ(deposits keyed on id) × ±1`. *"+1 food per river tile"* stops being a re-derivation and becomes
one multiply the moment a river bit moves. **This is the reason a population-changed fact is emitted** (owner):
a `per: {POPULATION}` scaler is plane B, and the fact carries the delta that resolves it.

### ⛔ THE INVARIANT — the slot is correct at every instant, which is what makes plane C delta-able

> **At every instant `slot == Σ resolve(d, state_now)`, because every operand's move applies its own delta at the
> moment it moves.**

It is inductive, and it holds only if EVERY operand has a route — which is exactly what a saturated emit surface
buys. Four consequences:

- **A WITHDRAWAL IS ALWAYS EXACT.** `emit()` dispatches SYNCHRONOUSLY ([spine.md](spine.md)),
  so no two operands are ever in flight together: when a fact arrives, every other operand still holds the value
  the stored contribution was computed against.
- **⚖ THE CONDITIONED TAIL IS THEREFORE DELTA'D TOO, PER ATOM (owner) — it is NOT re-resolved.** The earlier
  ruling that plane C could only re-resolve rested on *"`perScale` at deposit time is gone"*, and that is true
  only where a count can move WITHOUT announcing. Under plane B it always announces, so the state is never gone.
  ⛔ **B AND C ARE COUPLED — deliver both, or neither.** Delta-ing C while a count can still move unrouted
  reproduces precisely the drift the earlier ruling guarded against: the slot loses an amount it was never told
  about, and nothing re-derives it ([self-heal is not a backstop](#-a-self-heal-is-the-fossil-of-a-missing-emit--so-it-is-a-search-not-just-a-ban)).
- **ORDER-INDEPENDENCE SURVIVES, which is why LOAD is not a special case.** Source-then-count and
  count-then-source converge: the source applies `value × count_now` (0 if the count has not arrived yet), and
  the count applies `value × Δcount` for every deposit whose source is already live. A count route therefore
  tests the source's liveness at that owner — an O(1) `has()` — and applies for nobody else.
- ⚠ **THE HAZARD IS DOUBLE APPLICATION, NOT DRIFT.** One fact drives exactly ONE route class. Where a happening
  moves both a source and a count they are two distinct FACTS
  ([a fact names the happening](spine.md#-a-fact-names-the-happening--something-changed-is-not-a-fact-owner)), each applying its own — never one fact
  applying both.

- **⛔ NO PLANE HAS AN EVALUATION MOMENT TO DEFER, WHICH IS WHY NONE OF THEM CARRIES A STALENESS FLAG.** There is
  nothing to be stale ABOUT: every operand is compiled or maintained, so a slot is either current or was never
  told — and "never told" is a MISSING EMIT that must stay visible, not a state to schedule work against.
- **⚖ THE COMPLEXITY SHIFTS FROM O(WHAT EXISTS) TO O(WHAT CHANGED), AND THAT IS THE PERFORMANCE CASE (owner).**
  A rebuild re-walks the scope's sources, so its cost scales with how much a city HAS; an application touches the
  moved source's own deposits, so it costs the same in a 900-building city as in a 3-building one — **the walks
  disappear rather than getting faster**. This is § THE CONTEXTS's payoff one plane up: there, storing
  a fact made cost track EVENT volume instead of READ volume; here, applying a fact makes it track event volume
  instead of SOURCE volume.
  ⚑ **It also makes a promise the specs already print come TRUE.** [validation.md](specs/validation.md) states
  that *"the only path to a rebuild is a mark, so per-turn cost tracks what CHANGED — mark volume, which is event
  volume"* — which holds only if a mark is cheap. While a mark triggers a walk the real cost is
  `events × sources-at-scope`, i.e. the dominant term is the one the sentence omits. Under the maintained sum the
  sentence is literally true, which is [turn time is king](#-the-per-scope-package-model--the-cascades-founding-design-1-stated-as-cache-architecture)
  getting the property it was written for.
  ⚠ **What legitimately still walks, so the claim is not overstated:** the CONDITIONED tail evaluates when its
  dependency moves (bounded by the reverse index, never by the scope); the cross-scope roll-up at read sums the
  ~5 packages the object sits under, which IS the design (§1).
- **⚑ PLANES B AND C ARE WHAT THE SOURCE FACT CANNOT ANSWER, and together they are the whole of the residue.** A
  Forge's `+1 happiness while powered` moves when the POWER moves though the Forge did nothing, so it rides the
  ATOM's route (plane C); a `per: {POPULATION}` deposit moves when the population moves, so it rides the COUNT's
  route (plane B). Neither rides the building's.
  ⛔ Both routes are REVERSE INDICES derived from the compiled deposit index — atom → the deposits it gates,
  count-key → the deposits it scales — never a sweep of the scope's deposits asking each whether it cares, and
  never hand-written ([reverse lookups are populated once, at load](#1-one-step-deposit-down-accumulate-read-o1)).
- **⚖ ORDER-INDEPENDENCE IS FREE, and it is what makes LOAD stop being a special case.** Addition commutes, so an
  accumulate needs no arrival order — exactly the property [spine.md](spine.md) already
  demands of facts. The banked-marks bracket existed because *a rebuild mid-read evaluates against
  half-deserialized state*; an application of a compiled constant evaluates nothing, so it has no such hazard.
  **Only the CONDITIONED tail genuinely needs the `GAME_LOAD_STARTED`..`FINISHED` bracket**, because only it
  reads state the stream may not have delivered yet.
  ⚠ **Consumer registration order remains a contract for that half** (consumers dispatch in registration order):
  **contexts → enabler → modifier → triggers**. Anything that EVALUATES a condition registers after the contexts
  whose stores that condition reads.

### ⚖ WHY DELTA-DERIVING FAILED BEFORE — two preconditions, both now met (owner)

> *"The reason delta-deriving failed in the old model was because there was no unified eventing system, and
> random event yields was baked in, and not as a separate source."*

This is the archaeology that makes the retired protocol legible, and it matters because without it a reader
concludes delta was TRIED AND FOUND WANTING. It was not: it was unavailable.

1. **No unified eventing.** With no complete fact stream, the only honest statement a system could make was
   *"something in here moved"* — which is exactly what a staleness flag encodes. The flag was the best available
   statement, not a preference. ⇒ **The spine was never only an observability project; it is the precondition
   that makes the cache unnecessary**, which is why it had to land first.
2. **⛔ ONE-SHOT EVENT GRANTS WERE BAKED INTO THE SAME ACCUMULATOR AS THE DERIVABLE DEPOSITS — and that is the
   one that actually poisoned it.** Such a slot can be maintained by NEITHER mechanism: you cannot DELTA it,
   because the event contribution has no live source to withdraw against; and you cannot RECOMPUTE it either,
   because recomputing WIPES the grant. The accumulator becomes unrecoverable — the history pollution
   [the uniform legacy-accumulator cut](#-the-legacy-accumulator-cut--every-accumulator-one-uniform-mechanism) describes. ⚑ So the old model was not
   choosing recompute OVER delta; with a baked-in grant both were broken, and recompute was the one that failed
   quietly.

**Both preconditions are now satisfied** — the spine carries the facts, and the event/vote grant has been split
into its own persisted store outside the derivation (§ Event/vote grants, above: *"having events just be stored
in the cache is lunacy"*; the reader sums `derivable + persisted`). The model that failed then is not the model
specified here.

⛔ **THE GUARD, so it cannot recur — and it is the test to run on any slot, not a historical note.**
**Can EVERY contribution to this slot be attributed to a live source that announces itself?**
- **YES** ⇒ the maintained sum holds.
- **NO** ⇒ the non-derivable part is a SEPARATE STORE and is never folded in.

⚠ The failure is silent, which is why it needs a test rather than vigilance: a baked-in one-shot grant leaves the
number entirely plausible while making the slot unmaintainable by any mechanism at all.

### ⛔ THE COST IS THE FORCING FUNCTION — a saturated emit surface is now STRUCTURAL, not a discipline (owner)

> *"We have to take that cost — the system will by its very definition collapse if we do not saturate with
> events."*

A maintained sum fails differently from a recomputed one, and the difference is the POINT:

| | a MISSED emit leaves | how it reads |
|---|---|---|
| recompute-on-mark | a stale but internally consistent value | **plausible forever** — nobody looks |
| **the maintained sum** | a phantom contribution nothing later clears, compounding on repetition | **loud, and louder over time** |

⚑ **That is [self-heal is not a backstop](#-a-self-heal-is-the-fossil-of-a-missing-emit--so-it-is-a-search-not-just-a-ban) carried to its conclusion rather than a weakness
accepted against it.** The rule already says a missed invalidation must surface as a live divergence instead of
being swept away; between two failure modes, the one that ANNOUNCES itself is the one the rule asks for. ⛔ So
this is never a licence to relax the emit surface "because the number self-corrects" — nothing self-corrects,
and that is deliberate.

⚑ **It also promotes the roadmap's ordering from a preference to a law.** *"The EMIT surface comes first; the
cache build is the step AFTER"* was sequencing advice under recompute; under a maintained sum an unsaturated
spine cannot produce a correct number **at all**, so completeness of the emit surface is a PRECONDITION of the
cascade being right rather than a quality target it trends toward.
⇒ Every ruling that pushes the emit surface toward exhaustive — *"add all the events, ever"*, *"too many events
is better than not enough"*, [an event gap is closed the moment it is found](spine.md#-a-fact-names-the-happening--something-changed-is-not-a-fact-owner) — is load-bearing
on this model, not enthusiasm.

⚠ **The bound on the damage, so the trade is stated honestly: a phantom lives at most ONE SESSION.** Nothing
derived is serialized, so LOAD rebuilds every slot from the reseed's own facts — the history pollution that makes
a legacy serialized accumulator unrecoverable ([the uniform legacy-accumulator cut](#-the-legacy-accumulator-cut--every-accumulator-one-uniform-mechanism))
cannot accrue here. What NAMES it inside that session is the THREE-LEG check — the logs, the JSON info and what
state expects ([http-endpoints.md](specs/http-endpoints.md)).

### ⚖ AND IT IS THE EASIER CORRECTNESS PROBLEM — the deciding argument (owner)

> *"It is far easier to ensure we have all the events, than to ensure that we have all packages correctly
> marked."*

This holds independently of the cost trade above, and it is the reason to prefer the maintained sum even where
the two models would perform alike. The mark model needs **two** censuses complete; the maintained sum needs
**one** — and the one it drops is the harder of the pair:

|  | the EMIT census | the MARK census |
|---|---|---|
| the question | *does this mutation choke point announce?* | *does this fact reach every slot it could move, at every scope, for every owner?* |
| where it is answerable | **LOCALLY**, at the setter — read it and you know | **NOWHERE local** — the answer lives in the authored data |
| moves with the DATA? | no — an emit is engine mechanism | **YES** — a newly authored deposit can silently need a new route |
| safe to over-include? | **YES** — a surplus emit costs one consumer branch that declines | **NO** — a surplus mark is a real rebuild on the turn path |

⛔ **That last row is decisive, and it is already the spec's own rule** ([spine.md](spine.md):
*"emit liberally, mark precisely"*). Over-inclusion is the technique that makes a completeness census tractable —
it is how the enabler's reverse index is allowed to be safe ([enabler.md §5](specs/enabler.md): over-inclusion
is SAFE, a miss is the bug) — and the mark derivation is the one surface that cannot use it. A census that must
be EXACT, over a surface that moves with authored data, has no cheap verification at all.

⚑ **The emit census is owed ANYWAY, which is what makes dropping the other one a pure deletion.** The enabler,
the contexts, the trigger plane, the file log, the `/events` stream and the out-of-process replay all already
depend on the emit surface being complete. The mark derivation was a SECOND census serving one consumer, whose
correctness nothing else in the engine was ever checking.

⚑ **And a missing EMIT is multiply-observable** — a wrong availability verdict, an empty context store, a silent
`/events` frame, a missing log line — while a missing MARK is observable in exactly one package, on one plane,
and only by someone already looking at it. The easier failure to find is the one to keep.

- **ONE read surface, and it is a bare fetch.** `CvCascadePackage::readFlat/readPercent` is the whole of
  it: a package has no second, rebuild-triggering read to reach for, so a cross-scope input needs no ordering
  guarantee and the load bracket has nothing to drain. **THERE IS NO GATE ON A READ** — nothing is tested on it,
  because nothing on it can recompute.
- **The served surfaces are STORED-side only** (`/computed/*`, [http-endpoints.md](specs/http-endpoints.md)):
  each serves what the events built, DECOMPOSED term by term (`CvCascadePackage::readValuesInto`,
  `EnablerKernel::operatingBuildings`, `CascadeCapabilities::storedUnion`, and the yield census), rendered in
  `Sources/Tools/CvStateEndpoints.cpp`, never in the server file.
- ⛔ **THERE IS NO RECOMPUTE-FROM-SOURCE TWIN BESIDE THEM, AND NONE COMES BACK**
  ([superseded-ideas #33](architecture/superseded-ideas.md)): an endpoint cannot replay the event chain, so a from-source
  recompute served beside the stored value is not a second derivation of the same quantity — it answers a number
  that was never comparable, and diffing it produces confident nonsense at scale. **Correctness is the THREE-LEG
  check instead** ([http-endpoints.md](specs/http-endpoints.md)).
  ⚑ Three rulings from that dead shape are kept, because they bind ANY future verification and not just the one
  that died: a check must be **INDEPENDENT** (one that consumes the stored values is partly built on the very
  state it exists to check, so a wrong input is silently inherited and the two sides quietly share a derivation
  again); its **COST IS IRRELEVANT** — *"correct is correct"* (owner) — since it is invoked deliberately and
  never on a turn path, so it is never trimmed, sampled or memoized to look cheap; and it **ANNOUNCES NOTHING**,
  emitting no `[CASCADE] rebuilt` line, because a verification must not move the numbers that describe real work.

## ⚖ THE SPATIAL CARVE-OUT — a PATH is not a maintained sum, so it is a LEGITIMATE cache (owner)

> *"We should have some pathfinding cache, because it is the most expensive, and at the same time
> unmaintainable thing we can do — it has to scan plots by its very definition."*

Everything above says derived state is a MAINTAINED SUM and a cache is a defect. **SPATIAL results are the one
class that rule does not reach, and the reason is structural rather than an exemption granted to them:**

- **A path is not a Σ over sources, so there is no delta to apply.** It moves NON-LOCALLY — one terrain change
  or one new route re-routes paths that do not touch the changed plot at all — so no fact can name the set of
  results it invalidated. [the maintained sum](#-the-maintained-sum--three-planes-one-slot-and-nothing-is-ever-recomputed) needs `Δ(v × c) = v × Δc`; a
  shortest path has no such identity.
- **And it is the most expensive thing the engine does**, because computing one *requires* scanning plots. That
  is the definition of the operation, not an implementation that could be improved into a fetch.

⇒ **So a pathfinding cache is WANTED, and deleting one is a regression.** `PATHFINDING_CACHE` /
`PATHFINDING_VALIDITY_CACHE` are legitimate; so is `CvPlot`'s path-validity memo and the culture-distance
cache — `cultureDistance`, culture spread and the property propagators are all SPATIAL permanent carve-outs,
for exactly this reason.

⛔ **What the carve-out does NOT license.** It is scoped to results that are genuinely spatial:
- **not** an ordinary derived value that merely feels expensive — if a fact can name what moved it, it is a
  maintained sum and the cache is the defect ([a staleness flag is the fossil of a missing emit](#-a-staleness-flag-is-the-fossil-of-an-incomplete-emit-surface--the-same-rule-one-level-up));
- **not** a read-side `ensure()` — a spatial cache is filled at its own INVALIDATION point, never lazily on a
  read that is specified as a bare fetch (the tombstoned protocol, [superseded-ideas](architecture/superseded-ideas.md) #14);
- **not** freedom from invalidation. Being unmaintainable-by-delta means it is CLEARED, wholesale, by the
  events that can move it (terrain, route, ownership) — a spatial cache still has to be wrong for nobody.

## ⛔ `CvDerivedCache` IS REPLACED BY `ContextDict` — VIRTUALLY EVERYWHERE (owner)

> *"`CvDerivedCache` should be replaced by `ContextDict` virtually everywhere needed, and we just need to start
> taking one cluster at a time with event wiring."*

**`CvDerivedCache` (`Sources/Infrastructure/CvDerivedCache.h`) no longer exists.** It was a templated
mark→recompute value-holder — a `markDirty` that triggered a recompute over the owner's current state, exactly
the calculation a fact was supposed to make unnecessary. Every tenant converted, one cluster (an entity's facts
plus the store they feed) at a time: its events re-cut to name their happenings
([spine.md](spine.md) § A FACT NAMES THE HAPPENING), its store re-expressed as a keyed
accumulator ([every derived store is a keyed accumulator](#-every-derived-store-is-one-shape--a-keyed-accumulator-maintained-by-a-delta-owner)) or a channel slot in
`CvCascadePackage` (§ THE MAINTAINED SUM, above), and its recompute deleted in the same change. The legacy
`CvCity` hand-rolled staleness caches (`m_aiCommerceRate`, `m_aiBuildingCommerce100`, squirrelBanana) went the
same way.

⛔ **It is not reintroduced, and not reached for "just this once."** A recompute is only ever necessary when
inputs arrive UNANNOUNCED, which a saturated emit surface makes impossible
([a staleness flag is the fossil of a missing emit](#-a-staleness-flag-is-the-fossil-of-an-incomplete-emit-surface--the-same-rule-one-level-up)) — a surviving tenant would be a MISSING EMIT wearing a
component, the same shape a staleness flag wears one level out. The boundary between the two replacements —
keyed count vs summed magnitude — is § EVERY DERIVED STORE IS ONE SHAPE, above.

---

## 2. The combine arithmetic

Per `(family, member, unit, target)`, the slot composes the three value units ([json](specs/json.md) §3.6):

> **`effective = (base + Σflat) × (100 + Σpercent)/100 × Π(multiplier/100)`**

`flat`s sum into the base; `percent`s (additive deltas) sum then apply once; `multiplier`s compose by product.
`Σflat`, `Σpercent`, and `Πmultiplier` (flats + multipliers stored ×100, identity 100; a PERCENT is NOT
scaled — [the ×100 fixed-point model](specs/curators/fixed-point-and-scales.md#1-the-model--integer-100-for-amounts-human-only-at-the-in-and-out-boundaries)) are each their own accumulated number —
**the `unit` is part of the slot KEY (per `(family, member, unit, target)`), so a flat sum and a percent sum
are SEPARATE slots, never fields of one mixed struct** — the separation is what lets invalidation split
percent-vs-flat (§1). One `deposit(unit, value)` folds a value into its unit's slot; `effective(base)`
combines them at read.

All integer, ×100 fixed-point throughout ([the ×100 fixed-point model](specs/curators/fixed-point-and-scales.md#1-the-model--integer-100-for-amounts-human-only-at-the-in-and-out-boundaries)):
the single human→×100 conversion happened once in `readJson` ([json](specs/json.md) §3.6); the slot does pure integer
math and never sees the human boundary.

> **⛔ PLOT SCALING CAN ONLY AFFECT ITSELF — A HARD RULE (owner).** *"I do not think there is any scenario where a
> plot gives 1 hammer per 5 commerce, and as such we codify that as a hard rule, that the plot scaling can only
> effect itself."* A per-plot scaling of a channel reads that channel's own value on that plot and grants THAT
> CHANNEL. There is no cross-channel plot scaling, and none may be authored: a threshold on commerce cannot pay
> out in production.
> ⚑ **It is a structural simplification, not a restriction to police.** With the input and the output on one
> channel, the whole mechanic is plot-local — it needs no cross-scope reach at resolve time, no ordering between
> channels, and no fan-out when one channel moves another. That is what lets it live in the package.
>
> **⚖ THE MECHANIC IS TWO SEPARATE NUMBERS, BOTH FED IN (owner): a THRESHOLD and an AMOUNT.** *"You maintain the
> per-yield threshold, and the amount you get on the per-yield — treat them as 2 separate numbers that get fed
> in."* The interval is "per how much" of the plot's own value; the amount is what each whole interval grants.
> ⚖ **The AMOUNT comes from the `EXTRA_YIELD` global define, and that is fine (owner):** *"we can live with the
> EXTRA_YIELD define for now — we don't need to change that at this point."* ⛔ So a define read here is NOT a
> gap to close and NOT a missing authoring surface; do not "fix" it into curated data. What the ruling requires is
> that it stays a SEPARATE number the plane carries per channel — which it is — so that authoring it later is a
> data change and never a reshape.
> ⚠ **The THRESHOLD does not combine additively, and this is the trap:** the engine selects **the SMALLEST
> POSITIVE threshold held** (`CvPlayer::updateExtraYieldThreshold`), so two sources at 7 and 5 yield 5, never 12.
> A plain flat channel SUMS, so reading one through the ordinary roll-up is wrong by construction — it needs the
> non-additive family metadata this section already defines for `defense`'s floor kind. The AMOUNT is an ordinary
> additive number; only the threshold is a min.
> ⚖ **AND IT HAS TWO LEGS, ONE RAISING AND ONE LOWERING — `extraYieldThreshold` and `lessYieldThreshold`.** They
> are ONE mechanic with opposite signs: each selects the smallest positive threshold its owner holds, and each
> moves the SAME `EXTRA_YIELD` amount, one adding it and one subtracting it. Both are real authored data — the
> agricultural line raises, the lazy / gluttonous / excessive / nomad lines lower — so a plane carrying only the
> raising leg silently drops every downside a negative trait is meant to impose.
> ⛔ They are **two pairs, not one signed pair**: an owner can hold both at once at different thresholds, so one
> `(interval, amount)` slot per channel cannot express them. The lowering leg resolves on the value the raising
> one produced, which is the engine's own order (its second branch tests the already-raised running yield).
>
> **A plot's yield is ONE base package, resolved in isolation BEFORE the city modifiers.**
> All output from a single plot is computed in **complete isolation** as one base-yield package — `CvPlot::calculateYield`
> per plot (nature = terrain+feature+river+hills/peak + bonus; + improvement, floored at `-nature`; + route + the
> keyed/plots flats, `max(0,·)`) — and that result is passed **up the chain**: the city SUMS its worked-plot
> packages into the §1 `base`.
> **The plot yields ARE "the base the rest is calculated from."** So anything that scales a *specific improvement or
> plot component* resolves **inside** this per-plot package, **before** the city-level `(100+Σpercent)` stack ever runs.
> ⚖ **The CITY-CENTRE constant is the legacy `calculateYield` city block, inside this same isolated resolve,
> reading the plot's OWN city-ness LIVE (owner: "the flooring should be on the plot itself, not on the
> cascade")** — three terms on a city plot's yield channels: the YieldInfo `CityChange` constant (food −1 /
> production +1 / commerce +1) **plus** `population / PopulationChangeDivisor` (food /5, production /2,
> commerce /4 — integer division), both added BEFORE the plot scaling so the threshold plane tests the total
> legacy tested; and the `MinCity` floor (3/1/1) applied LAST. City-ness is the plot's own state, so none of it
> is mirrored onto the package as a fed operand; the `SEVT_PLOT_CITY` pair and the city's `SEVT_CITY_POPULATION`
> facts are RE-RESOLVE routes only (the refresh-an-operand shape), each folding the exact delta into the working
> city's worked-plot Σ. *(A founded city physically clears its plot's improvement, so the legacy city-block
> improvement exclusion needs no resolve leg; route flats stay in the base per this row.)*
> ⚖ **THE PLOT PACKAGE STORES FOUR SEGMENTS, AND THE FOURTH EXISTS FOR ONE OPERAND: nature · improvement ·
> ROUTE · rest.** Route and the owner's plot flats sum and floor identically, so the split buys nothing on the
> TOTAL — what it buys is the engine's per-plot GOLDEN-AGE threshold, which tests the **pre-improvement,
> pre-route** running yield (`nature + the city block + the owner's plot flats`,
> [golden-age.md](reference/golden-age.md)). That operand is inexpressible while route and the owner's flats
> share a sum, and at the authored threshold of 1 the difference is very nearly every improved tile.
> ⛔ The SCALING is the opposite case and stays on the FULL total (the row above: *terrain + feature +
> improvement + route*) — the two thresholds deliberately take different operands, so do not "unify" them.
> ⚠ For REPORTING, `plotRest` keeps its meaning — the owner's flats **plus** route — so the segments still sum
> against `plotBase`; `plotRoute` is a breakdown of it, never a fourth term beside it
> ([http-endpoints.md](specs/http-endpoints.md)).
>
> Today every component-specific buff is **flat** (so the package is a pure sum); should a per-improvement *percentage*
> ever be needed, it applies **here, inside the isolated plot calc** — **never** in the city `(100+Σpercent)` stack,
> which only ever scales the already-summed base. Consequence: a `basePlotYield` divergence is *necessarily* a per-plot
> **flat** miscount (missing or double-counted), because no city-level percentage exists that could move a single plot.
>
> **Completeness is the bar ([represent, don't fit](specs/validation.md#the-observation-surface)).**
> Multiplier deposits are treated as identity on the yield/commerce channels — no source authors one, so the cascade
> is additive, exactly matching legacy. Live acceptance is done-is-observable
> ([done = observable in the running game](specs/validation.md)) + turn time
> ([turn time is king](#-the-per-scope-package-model--the-cascades-founding-design-1-stated-as-cache-architecture)): [validation](specs/validation.md).

**Non-additive combine, declared as FAMILY metadata (never per-deposit):** a `min` member that floors the
combined total (e.g. `defense`'s floor kind). Authors write signed values; the mode wires the combiner.
⛔ **`naturalDefense` is NOT one of these and never was a kind.** There is no natural-defense channel: BUILDINGS
and CULTURE LEVELS author the SAME `defense.city.amount`, so the cascade holds one additive stack and the legacy
`max(buildingDefense, naturalDefense)` has no counterpart — a data-led behaviour change, not a combiner to
build. ⚠ A worst/best-across-sources combiner is **not part of the model** — do not read this paragraph as
licence to add one speculatively; mint it only if and when a family's data actually needs it.

> **⚖ THE FREE-AMOUNT SIGN CONVENTION (owner) — one convention per kind, never a per-source flip.** The
> `upkeep.freeMilitary` / `upkeep.freeCivilian` kinds carry **free-amount semantics throughout**: a POSITIVE entry
> GRANTS free upkeep, a NEGATIVE entry SHRINKS the free allowance. Entries sum like any other channel, and the
> **group total floors at zero as family-combine metadata** (the `min` mechanism above) — distinct from, and
> applied before, the engine's own `net = max(0, upkeep − Σfree)` floor. **Two floors, deliberately: one on the
> group, one at the consumption site.** A pop-scaled source authors `{P, per: {POPULATION, each: 100}}` keeping
> its own sign. ⚠ This is an owner-ruled INTENTIONAL divergence from the legacy asymmetric rounding helper
> (whose `mod<0` branch computed `v×100/(100−mod)`): the ruled shape is **additive linear**, attributed and never
> bit-chased ([validation.md](specs/validation.md) intentional class).

> **⛔ There is NO `polarity` mode — wellbeing is FOUR ORDINARY CHANNELS (owner):** `happiness`, `anger`,
> `health`, `unhealth`. Happiness sums against anger, health against unhealth, at the verdict (§2b). A negative
> deposit is routed to the opposing channel **at fill**, so the split is a routing rule, never a storage shape —
> no good/bad plane, no duplicated positions, no per-family combiner. **The routing granularity is PER ENTRY**
> (a deposit IS an entry, [json.md §3.9](specs/json.md)) — a mixed-sign author SPLITS across the pair rather than
> netting, and per-entry is the only delta-able form, so the apply, the valuation and every other fill aggregate
> identically by construction. It reaches the FLAT side only: a negative PERCENT scales its own channel down and
> is never re-homed to the twin. This is what keeps
> [every modifiable number is a yield](#1-one-step-deposit-down-accumulate-read-o1) literal: wellbeing is four yields like
> any other, on the one uniform package
> ([every derived cache is one shape](#-every-derived-store-is-one-shape--a-keyed-accumulator-maintained-by-a-delta-owner)).

---

## 2a. The realized RATE — what is BASE, what is added AFTER the percentages

The §2-combine above is the *generic* slot. A city's **per-channel yield/commerce RATE** — `InfoValuation::cityRate`
for yields and `InfoValuation::commerceSplit` for the commerce channels, the value a citizen's worked output
finally becomes — is that combine applied with a **sharp two-tier shape**. This is the model the rate computation
must reproduce, and the order is load-bearing (it decides what the percent stack scales and what it doesn't):

> **`rate100 = (BASE + specialists) × modifier⁄100  +  100 × ⌊EXTRA100 ⁄ 100⌋`**
>
> `modifier = max(0, 100 + Σpercent)` (so `×modifier⁄100` ≡ `×(1 + Σ%)`). Everything is ×100 fixed-point integer.

### TIER 1 — BASE (everything the percent stack MULTIPLIES)

| BASE source | origin | base vs computed |
|---|---|---|
| **worked-plot yields** (`basePlotYield`) | Σ over the city's worked plots of each plot's ONE isolated base package (§2 plot-as-base): `max(0, terrain+feature+bonus)` nature + improvement (floored at −nature) + route + keyed building/civic/trait `plot`-flats + `plots`-target + city-centre constant + threshold/golden-age per-plot | **computed** from the curated plot substrate + engine plot state |
| **trade-route yield** (`tradeYield`) | engine-generated (the trade network) — ⚖ **already carrying its OWN percent layer, see below** | **input** — out-of-scope: the cascade cannot re-derive the network, so the calc *folds the route yield in*, never derives it. **The ONE live-yield input** — a clean addition at the very end of the base, and the sole sanctioned exception to the pollution guardrail ([validation](specs/validation.md), [the pollution guardrail](specs/validation.md#the-pollution-guardrail--engine-computed-data-never-rides-in)). ⚠ **The route COUNT is the OPPOSITE case (owner): `getMaxTradeRoutes` — game + player + coastal + `city.extra` slot deposits — is a modifier-influenced value the cascade COMPUTES, its own `tradeRoutes` channel.** Trade YIELD is read from the engine package; the trade-route COUNT is calculated here. Do not conflate them |
| **free-city yield** (`freeCityYield`) | Σ the player's active traits' `YieldChanges` (`{ch}.empire.flat`) | **computed** — derivable from the trait JSON, so it is COMPUTED, never read off the engine; consuming the live value would leave the trait→yield derivation unvalidated ([validation](specs/validation.md) pollution guardrail). ⚠ NAMING: "free-city" here = the legacy trait accumulator (`CvPlayer::m_aiFreeCityYield`, free yield granted in every city) — **NOT** the WLTKD celebration ("We Love the King/Emperor Day"), whose sole gameplay effect is zero city maintenance ([economy.md](reference/economy.md)) |
| **golden-age yield** | trait `goldenAge` member (`{ch}.empire.goldenAge.flat`) while in golden age | **computed** (`empire.goldenAge` member-mirror, §3 golden-age carve-out) |
| **specialist yields** (`specialist`) | per assigned specialist: `intrinsic × (100 + specialist-%)⁄100` + building-local (gated `city.flat`) + per-type (`empire.cities.flat` — the `cities` target lands it in the HOLDING city; a bare `empire.flat` on a specialist would roll down to EVERY city and cascade with city count) + perAll + trait governing-deliverer | **computed**. NOTE the specialist carries its **own** percent layer (its intrinsic ×`(100+specialist-%)`) *before* it joins BASE and takes the city `modifier` — two distinct percent stacks |

> **⚖ HOW `tradeYield` STAYS CURRENT — the ONE value the cascade FEEDS but does not HOLD, so it is REBUILT, not
> delta'd.** It is the engine's network OUTPUT (`CvCity::m_aiTradeYield`, ×100 like any amount), not a package
> slot, so the maintained sum does not reach it and no compiled deposit index can name what moves it. Its
> rebuild has four moments and they are the whole set: ONCE at the end of load (against the final cascade);
> TARGETED at the owner whenever a fact moves a `tradeRoutes` channel; on every plot-group / network change
> (which is what covers a city being FOUNDED or ACQUIRED — both reach `updatePlotGroups`); and once per player
> in `doTurn`.
> ⛔ **The per-turn rebuild is NOT the banned blanket** ([self-heal is not a backstop](#-a-self-heal-is-the-fossil-of-a-missing-emit--so-it-is-a-search-not-just-a-ban)):
> that rule bans papering over a MISSED invalidation, and this one exists because a genuine INPUT advances every
> turn — `getPeaceTradeModifier` scales with the at-peace counter, so a foreign route's profit legitimately
> differs turn to turn until it saturates. There is no fact to route it to; the turn IS the fact.
> ⛔ **AND THE CASCADE NEVER TRANSCRIBES THE PER-CHANNEL FORMULA EITHER — it folds `getTradeYield`, full stop.**
> The engine's `CvCity::calculateTradeYield` (profit × the player's per-yield trade modifier) is the ONE
> implementation, and it is engine-owned by the same KEEP ruling that puts the network there
> ([north-star.md](architecture/north-star.md)). ⚠ A copy of that arithmetic on the CALC SURFACE reads like
> the canonical home — it sits beside the genuine `§2a` seams and looks like the one they all point at — and it
> is the opposite: a second implementation of a calculation this spec says the cascade must not own. One was
> built and never called; it is deleted rather than wired, because there was no consumer to wire it TO.
> ⚠ **City POPULATION deliberately gets NO route, and that is a cadence ruling rather than an omission.** It
> feeds the profit on both sides (`getBaseTradeProfit` reads the PARTNER's population, `getPopulationTradeModifier`
> the city's own), so a route would have to rebuild the owner AND every player trading with it — and it would
> fire once per city GROWTH, i.e. once per city per turn, each firing a full network walk. The mid-turn snapshot
> rule already answers it (§ EAGERLY BUILD ALL CACHES AT LOAD, below: *"getting a yield event in the middle of a
> turn is not retroactive; start of next turn is what is expected"*), and the next `doTurn`
> is that start.

### TIER 2 — EXTRA (flat, added AFTER the percentages, NEVER multiplied)

| EXTRA source | origin |
|---|---|
| **building flat yields** (`BuildingFlatYield100`) | Σ active (non-dormant) buildings' `{ch}.city.flat` + `{ch}.city.perPopulation` × population |

The EXTRA is held ×100; the `100 × ⌊EXTRA100⁄100⌋` **truncates it to whole units** before re-scaling (the engine's
`getExtraYield100` order — a documented integer-truncation gotcha, not a rounding choice).

> For **§2 commerce** the same two-tier shape holds with the channel's own pieces: BASE = the COMMERCE-yield
> (`InfoValuation::cityRate`'s COMMERCE channel) × the channel slider + the §2 baseExtra sub-terms (religion, corporation, golden-age,
> state-religion pool, player-extra, the building-commerce block); EXTRA (post-modifier) = `production × prodToCommerce`.
> The building-commerce block is itself a pure per-building sum over the building's OWN entries (own-flat + tech +
> bonus + perPop + shrine + corp-HQ + the `CommerceChangeDoubleTime` whole-doubling) — and the building-keyed boosts
> (a wonder/civic/tech granting a channel to a building TYPE, `{c}.<scope>.buildings.{B}`) are part of that sum as
> the TARGET building's own reverse-landed conditioned entries: authored deliverer-side (§4), landed at CITY scope
> by the readJson reverse pass, gated on the source's presence at the authored scope. Civil disorder forces the
> whole rate to 0 before any of this.
>
> ⛔ **THE SPLIT IS A CITY/EMPIRE CONCERN — THE PLOT AND THE BUILDING DO NOT CARE (owner).** *"The plot itself does
> not need to care about the commerce split, nor the building, beyond what is written in the tooltip."* A plot
> produces its isolated base package; a building deposits into its channels. **Neither knows or needs to know**
> that the city's COMMERCE yield is later divided into gold / research / culture / espionage by the player's
> sliders — that division happens where the sliders live, at CITY and EMPIRE. So the split never propagates
> downward into a plot or building read, no plot/building surface grows a per-commerce-channel shape for it, and
> the dependency it creates is bounded to **(city commerce yield + slider + active process) → the empire's
> commerce receivers**. The ONE place a lower scope's contribution meets the split is **DISPLAY** — a tooltip
> saying what this building is worth — and that is the [valuation](architecture/patterns.md) answering a
> resolved delta, not the plot or building carrying split knowledge of its own.
>
> ⛔ **AND THEREFORE A SLIDER MOVE RE-EVALUATES NOTHING — no citizen re-assignment, no plot re-scoring
> (owner).** *"Moving a slider should not really need to reassign citizens; it does not change commerce
> outputs at all, and plots are not evaluated on the commerce yields themselves."* The slider re-divides a
> COMMERCE yield it does not change, and the plot valuation never reads the commerce channels, so every input
> to a citizen decision is exactly what it was. The realized rates pick the new split up at the COMBINE, which
> is the whole of the work a slider causes.
> ⚑ **The measured cost of getting this wrong, because it is the reason the rule is written down:** the setter
> flagged every one of the player's cities for re-assignment, so ONE slider tick re-ran the full citizen
> assignment across the empire and **stalled for fifteen seconds**, of which the entire observable result was a
> couple of dozen facts — a handful of cities moving a citizen, which is the churn of a re-decision that had no
> new input to decide on.
> ⚠ It is the [a staleness flag is the fossil of a missing emit](#-a-staleness-flag-is-the-fossil-of-an-incomplete-emit-surface--the-same-rule-one-level-up) shape on the AI plane: the
> flag asserted that something a citizen cares about had moved, and nothing had.

### How the percentages "smash together" — ONE additive stack

`modifier` is **a single additive sum** — every active source's `{channel}.<scope>.percent`, added together, then
`max(0,·)`:

- **active buildings** (this city, non-dormant): `city.percent`
- **empire buildings** (every building the player owns anywhere — rolls DOWN to each city): `empire.percent`
- **adopted civics**: `empire.percent`
- **the player's active traits** (the option-selected set, pure-filtered §4): `empire.percent`
- **projects** (commerce channels only; yields find none): `empire.percent`

They are **purely additive** — `+30% +20% −10% = +40%`, applied **once** as `×140⁄100`. The engine keeps these in
*separate accumulators* (`modBuilding`, `modPlayer`, `modCapital`, `modBonus`, `modFromBuildings`, …); the cascade
**unifies them into this one sum** because addition is associative — the per-accumulator split changes nothing the
result can see. `multiplier` deposits (`Π(multiplier⁄100)`, §2) exist in the generic model but are **identity here**:
no yield/commerce source authors a multiplier, so the stack is additive-only and matches legacy exactly.

The two tiers + the single additive stack ARE the coherent shape: a BASE assembled from its sources, scaled **once**
by the unified percent total, with the building FLATs bolted on **after** — never inside — the percentages.

---

## 2b. The WELLBEING channels — health + happiness (signed-split, the §2a sibling)

The city's **health** and **happiness** levels are the §2 combine over **FOUR ORDINARY CHANNELS (owner)** —
`happiness`, `anger`, `health`, `unhealth` — summed in **opposing pairs** at the verdict: happiness against anger,
health against unhealth. They are four yields like any other, carried on the one uniform package with no special
storage: a source depositing a negative value is routed to the opposing channel **at fill**, so nothing about the
combine or the cache is wellbeing-specific.

**⛔ A CHANNEL *IS* THE LEVEL — there is no separate verdict getter, and the distinction that remains is
DEPOSITS vs REALIZED.** Two reads, and conflating them double-counts:

| read | answers | composes with |
|---|---|---|
| the GROUP read (`getWellbeing`) | the DEPOSITS only — the cascade's roll-up over the scope chain | a CANDIDATE's `expectedWellbeing`, which answers in the same vocabulary ([patterns.md](architecture/patterns.md) § THE TWO READ ROLES) |
| the REALIZED read (`realizedWellbeing`) | deposits **+** the raw-state inputs below | nothing — it is this city's own level |

The raw-state inputs are folded at the REALIZED read, exactly where the engine folds them, so a consumer never
re-derives one. A consumer wanting one side of a pair indexes the array; there is **no per-side getter**.

The **opposing-pair NETS** (`InfoValuation::netHappiness` / `netHealth`) live once on the calc surface, are fed
the four channels rather than an object — which is what lets the same implementation net a city's realized set
and a candidate's expected delta — and are **signed** (a surplus is as meaningful as a deficit). The realized
end-state values are the clamps over them, and are a final-state CALCULATION, never a channel or a getter
([patterns.md](architecture/patterns.md) rule 6): `healthRate = min(0, health − unhealth)`;
`angryPopulation = clamp(anger − happiness, 0, pop)`.

⚠ The wellbeing channel has no decomposition census yet ([http-endpoints](specs/http-endpoints.md)); when the route
table is rebuilt it wants one field per named engine term, so a divergence localises to a single source.

**The TARGET/INPUT split (the tradeYield precedent, [validation](specs/validation.md) input rules):**

- **DEPOSIT-COMPUTED (the cascade's targets)** — everything a live source's `health`/`happiness` family deposits
  produce: **buildings** (city `flat`/`perPopulation` + the empire-scope rollups + conditioned entries incl.
  `HAS_STATE_RELIGION`-gated and the reverse-landed source-keyed boosts — a wonder/civic/tech `buildings.{B}`
  wellbeing deposit is authored deliverer-side (§4) but the readJson reverse pass lands it on the TARGET building
  as a CITY-scope conditioned entry gated on the source's presence at the authored scope, so it reads
  building-side under this term), **civics** (empire flats — incl. the tax-anger deposit, a `happiness.empire`
  entry per-scaled on `GOLD_RATE`, re-booked by the slider-rate count route — + the keyed/heterogeneous members
  read civic-side: `features.{F}`, `nonStateReligion`, the `cities.{unit: IS_MILITARY}` per-unit scaler, the
  ranked `cities` scaler — the civic's `buildings.{B}` member lands building-side per the above), **traits** (same member
  vocabulary), **features** (`health.plot.percent` — summed over radius plots, ÷100 — the fallout class),
  **bonuses** (`empire.cities` flats, presence-gated — ⛔ NEVER a bare `empire` flat: that lands in the empire
  package and rolls DOWN to every city, while the engine applies it on the per-city presence fact, so one
  connected luxury is counted once per holding city and the product handed back to every city. The `cities`
  target lands it in the HOLDING city's package, which is what a luxury means — the cities that HAVE it are
  happier — the same precedent the specialist `cities`-target deposit sets one entity
  over), **specialists** (city flats; the fractional values are the
  curator's ÷100 de-scale of the legacy latent-×100 — the engine `…/100` at use), **corporations**
  (`HAS_CORPORATION`-conditioned city flats), **techs**/**projects** (empire — projects also the lone `world`
  scope)/**handicaps** (empire flats), and **military units** (`happiness.empire.cities.{unit: IS_MILITARY}`
  §3.7). **Religion happiness has NO religion-side data** (verified: legacy religion info carries none) — the
  state/non-state religion terms derive from CIVIC/TRAIT/BUILDING configs × religion presence.
  ⚖ **Improvement health is a BALANCE-CUT (curator ruling, `curate_improvement.py`):** legacy `iHealthPercent`
  is deliberately dropped from the data, so the engine's `improvementGood/Bad` term is an **intentional
  divergence** — attributed by the engine's own `improvementGood100/Bad100` terms, shown, never chased
  ([validation](specs/validation.md) intentional-model-change class); the term dies at the channel's legacy cut.
  ⚖ **Improvement HAPPINESS, by contrast, IS represented** (owner ruling — no gaps): the intrinsic per-radius
  improvement happiness (`happiness.plot.flat` on the improvement) and the civic per-improvement happiness
  (`happiness.empire.improvements.{I}.flat`) are **folded into the feature happiness terms** (`featSubstrate` +
  `featMember`) — because the legacy `getFeatureGoodHappiness` bundles feature + improvement happiness into ONE
  number. Structurally live end-to-end; **zero data carries it today** (schema-only civic field, no improvement
  authors `iHappiness`), so the verdict is unchanged — the path is future-proof for any modder data.
  **Celebrity happiness** is an INPUT; the `skills.celebrity` unit-scan port (the CvCity scan) finishes it.
- **RAW-STATE INPUTS (folded, never derived)** — the runtime timers/counters no deposit produces: the **anger
  percents** (overcrowding = f(pop), noMilitary, foreign-culture, enemy-religion, hurry/conscript/defy/
  revRequest timers, war-weariness, revIndex, civic anger%), the **happiness timer** (`getHappinessTimer` —
  the same countdown shape as the anger timers above, folded on the happiness side: `GC.getTEMP_HAPPY()` while
  the timer runs), the **espionage counters**, **event anger**
  (one-shot event state), **foreign-culture anger**, **landmark anger** (option-gated —
  ⚖ KEEP through the migration: the existing engine implementation stays, *"straight up state derived from the
  plot in question"*; the landmark data pass is a sanctioned separate data pass (#448); the engine impl KEEPS),
  **city-over-limit**, and **vassal** terms. These are saved/derived-from-saved state — legitimate inputs, since
  no deposit produces them and nothing about them is a cascade output ([validation](specs/validation.md) pollution
  guardrail) — and the calc folds them at the level combine exactly where the engine does.
  ⚖ **The `extraHappiness`/`extraHealth` accumulators are EVENT-GRANTED persisted state (owner ruling), a
  SANCTIONED read, not a ride-in:** the CITY `getExtraHappiness`/`getExtraHealth` are written ONLY by `applyEvent`
  (an event granting extra happiness/health) — genuine one-shot non-derivable state (the event-store class,
  § THE MAINTAINED SUM, above); the PLAYER accumulator additionally bundles the
  DERIVABLE trait+tech, which the calc NETS OUT (− engine trait/tech + the cascade nets), keeping only the
  event/unattributed residual. Wiring these as proper cascade event grants is **event-rework scope** (#425 events
  stay Python / the F3 grants apply-loop), NOT a modifier-cut ride-in to fix here.
- **GATE FLAGS** — the `abolished<Channel>` amenity family ([json.md §8](specs/json.md)) zeroes its side wholesale.
  They are **HARD OFF-SWITCHES, never modifiers (owner)**: while a live grantor confers one *"unhappiness does
  not exist in the city"* — the side ceases to exist rather than being reduced, so the combine drops the whole
  channel instead of subtracting from it.
  ⛔ **The gate asks the CITY, never a grantor** — `CvCity::isNoUnhappiness` /
  `isNoUnhealthyPopulation` / `isBuildingOnlyHealthy` are folds over the city's `amenities`
  (§ THE CONTEXTS, below), so a WHERE rides the grant's own `enabled` condition and is
  evaluated per receiver at fold time. There is no hand-named counter and no per-key grantor read to reach for.
  ⚑ **No BUILDING authors one, and that is DELIBERATE — the mechanic is "wildly overpowered" (owner)** — so
  finding the building side unauthored is never licence to author one, and equally never a reason to purge the
  key as unused. ⚠ The CHANNEL is nonetheless LIVE: a civic confers `abolishedAnger` gated `IS_CAPITAL`, which
  is what retired the legacy key that baked the capital into its name
  ([conditions are predicates, never bespoke members](specs/json.md#35-predicates--a-systems-runtime-state-query)).
- **`unhealthyPopulation`** (= `max(0, pop − angryPop)` unless flagged) enters the BAD side as the engine's
  population term — a state-derived input (it reads the happiness verdict; the calc computes it from its own
  happiness result, never reads the engine's).

⚠ Two engine quirks the calc reproduces verbatim — named here so the reproduction is DELIBERATE and visible rather
than accidental. Whether they survive is a SPEC decision (the spec leads), never a silent "fix" at a call site:
`badHealth` adds `min(0, extraBuildingBadHealth)` **twice** (once inside `totalBadBuildingHealth`, once
directly); and the anger percents scale by `pop/PERCENT_ANGER_DIVISOR` with truncating integer division.

**⛔ TRAVELING UNIT MODIFIERS RIDE ON TOP (GENERAL — all channels).** A modifier that
TRAVELS with a unit (unit-sourced happiness, anger, property emission, and any future unit-carried channel
value) is **never part of a cached cascade computation**: it is computed LIVE at read and **added on top as a
FLAT term, after and outside every percentage modification**. Two structural consequences: (1) unit movement
never dirties any cache — the cached sums are unit-free by construction; (2) the traveling value is a plain
flat addition to the realized number, never an input to a percent stack. The implementation shape: the cache
stores the unit-free number (+ any epoch-stable per-unit multiplier, e.g. a civic's per-military-unit VALUE);
the read folds `perUnit × liveCount` / the live unit walk on top (an O(1)-ish live engine read).
**The AUTHORING BAN that keeps this coherent: no unit gives — or can ever be
ALLOWED to give — PERCENTAGES to yields of any kind.** A unit-carried value is always a raw flat number on
top; a unit-authored percent would force units back inside the cached percent stacks and break the whole
on-top model. Enforceable at the curator/validation layer: a `units/**` JSON authoring a yield/commerce
`percent` deposit is a data error.
Ledgered as [unit-carried modifiers apply on top, live, never cached](#2b-the-wellbeing-channels--health--happiness-signed-split-the-2a-sibling).

> **⚖ THE COMMANDER RIDES ON TOP OF A UNIT EXACTLY AS A UNIT RIDES ON TOP OF A CITY (owner).** *"Whatever a
> commander does is on top, it is not part of the unit itself — it is literally the combat calc's job to check
> if the commander has points left to add to the attack."* So this is the SAME rule one scope down, not a new
> one: the commander→unit relationship is the unit→city relationship, and everything above applies unchanged.
> - The unit's RESOLVED values (§ THE READ PATH, below — UNIT plane) are
>   **COMMANDER-FREE by construction**: they gather the unit's own info ∪ its promotions ∪ its unit-combat
>   classes, and nothing else. A commander attaching, detaching or moving is neither a promotion nor a
>   combat-class change, so it must never be a cache input — there is no fact that would move it, and baking it
>   in yields a plausible, permanently stale number the moment the commander moves.
> - The commander's contribution is added **LIVE, ON TOP, at the COMBAT CALC**, which is also the only place
>   that can ask the question the mechanic actually turns on: **has this commander got control points left to
>   spend on this attack?** A stat read cannot answer that, which is why the fold does not belong in one.
> ⛔ So a per-unit stat getter that reaches through `getCommander()` and adds the commander's own accumulator is
> the wrong shape twice over: it puts a traveling modifier inside the unit's own number, and it spends the
> commander's points without ever checking whether any remain.

**UNIT-driven wellbeing is END-TURN cadence.** The military/unit-count happiness
term recomputes **once per turn** (the substrate's turn-roll), NEVER per unit move — a per-move mark hook made
every post-move rate read pay the wellbeing walk (a measured unit-automation collapse) and is banned. The
within-turn lag this leaves on the wellbeing slots (a handful of cities whose garrison changed mid-turn) is the
RULED cadence, not a freshness hole; the getter flip proceeds with it.

**The STORED-ACCUMULATOR DRIFT class.** The legacy wellbeing terms are
INCREMENTAL SERIALIZED accumulators (`m_iBonusGood/BadHappiness`, `m_iBuildingGood/BadHappiness`,
`m_paiStateReligionHappiness`, `m_iExtraBuilding*FromTech`, …) — event-sourced numbers that carry decades of
save history. **The old cache model folded event-type grants DIRECTLY into these caches** (there is no separate
event-yield data — the per-building `m_aBuildingHappy/HealthChange` ledgers carry nothing on real saves), so a
stored value that disagrees with its current-state recompute is **DRIFT (history pollution), never
event state to preserve**. ⛔ The `*Recomputed` twin that once stood beside each incremental accumulator is
GONE, and so is the comparison surface it fed ([superseded-ideas #17](architecture/superseded-ideas.md): zero
such symbols remain). A stored-vs-recompute divergence of this class is **engine-wrong / cascade-right** and is
repaired by the slots recomputing from data — never by re-adding a twin to measure it with.

---

## The contexts — the per-scope live-state read surface

> The live-state object a cascade getter and the one condition evaluator read to compute an entity's ACTUAL value in
> a given place. One per game-object scope that needs it: **PlotContext** (`CvPlot`), **CityContext** (`CvCity`),
> **EmpireContext** (`CvPlayer`). Owner rulings; this is the concrete shape the "make the infos sane"
> `(cityContext, plotGroup)` getters ([patterns.md § INFO DATA-OUT](architecture/patterns.md)) read.

**A context is cascade OUTPUT, not a separate "input" kind (owner):** *"contexts, when thinking about it,
are in essence the output of the cascade."* Same scopes, same spine, never serialized, rebuilt by the same
reseed, read as the same bare fetch — and maintained the same way, by the fact that names the source
([the maintained sum](#-the-maintained-sum--three-planes-one-slot-and-nothing-is-ever-recomputed)). What differs is only WHO CONSUMES the
value: a package answers a magnitude, a context store answers a gate. ⚠ That is a statement about the consumer,
never about the kind of thing being stored, and treating it as two planes is what let them drift onto opposite
maintenance mechanisms before this page merged them back into one.

### The one idea — isolate the CHANGEABLE state a reader needs, per scope, in ONE understandable place

A building's output getter computes the ACTUAL benefit in a city, which depends on that city's live state (its
connected/vicinity bonuses, river/coast plots, power, religions, …). Rather than every getter reaching into the
`CvCity`/`CvPlayer` god-objects ad hoc, each game object that a reader needs owns **one context** — the single,
predictable home for that object's changeable state. The **symmetry IS the value**: a reader always knows where to
go (city state → `CityContext`, empire state → `EmpireContext`, plot state → `PlotContext`).

**Isolation is for RESPONSIBILITY, not decoupling (owner).** The context is bound to its game object by pointer and
freely reaches into it — coupling is fine when the structure is ironclad. The goal is a clean responsibility line
(this object is THE state surface for its scope), never running detached from the live object.

### What a context STORES vs FORWARDS — ⛔ a context is an EVENT-BUILT STORE, not a forwarding facade (owner)

**"Context should be built on events — that is the design of it."** And the purpose of storing is that the state
becomes DISTINGUISHABLE: *"so that an info can say 'yes, I will actually deliver this, based on this state.'"*
A context that merely forwards to its bound object delivers none of that — it is the same pointer hop with an
extra name, so the design collapses into "pass the god-object like always."

> **⚖ THERE SHOULD VIRTUALLY NEVER BE AN ORDERING PROBLEM — EVERYTHING IS POPULATED BY THE REPLAY OF SPINE
> EVENTS (owner).** That is what makes consumer registration order almost irrelevant: each consumer builds its
> own state from the SAME fact stream, so no consumer waits on another's build.
> ⛔ **The anti-pattern that manufactures ordering is a store that RE-DERIVES by READING another system's built
> state.** It cannot run until that system is built, which instantly turns registration order into a dependency —
> and the dependencies go both ways (the enabler gates THROUGH these stores, so a store reading the enabler is
> circular). ⇒ **A store LISTENS and applies a delta; it does not read a set and recount.** The city's
> `amenities` fold is the worked case: as a delta off the per-building fact it builds itself identically at load
> (the save read's own emits) and at play, with no phase ordering; written as a re-derivation over the enabler's
> operating set it could not build at load at all.
> ⚠ **The exception is a HARD COUNTER, and it is SERIALIZED STATE (owner): a city's POPULATION, its CULTURE, its
> STORED PRODUCTION — "these kinds of things have to obviously just be serialized out."** They are not derived
> from anything, so the VALUE comes back off the save directly and is FORWARDED (below) rather than stored.
> **⛔ BUT THEY DO EMIT, AND THE SAVE READ IS WHERE (owner).** Reading the counter off the stream fires
> `CITY_POPULATION_ADDED <the stored amount>` — the ordinary `_ADDED` fact with its magnitude
> ([spine.md](spine.md)), not a bespoke load verb. ⚑ **The counter needing no event and its
> CONSUMERS needing one are different questions, and conflating them is what left a hole:** every deposit
> scaled `per: {POPULATION}` is maintained from ZERO by applying, so without that fact a loaded city's
> population-scaled deposits would all be missing — the value present on the object and absent from every sum
> derived off it. ⚑ It also needs no load special case: the same `_ADDED` fact the growth path emits, with the
> save's amount instead of 1 ([the load reseed](spine.md#5-the-load-reseed) — read, emit, populate). That raises no ordering question at all.
> ⇒ The three-way test, and the exception confirms the split rather than bending it: **DERIVED ⇒ built by the
> event replay, never serialized** ([derived data is never trusted from a save](specs/save.md#5-derived-data-serializes-nothing-)); **genuine
> non-derivable state ⇒ serialized, and forwarded live** ([save.md §5](specs/save.md) — a serialized store
> survives ONLY for state no derivation can produce); a context never stores the second kind.

The split is by **DERIVED vs RAW**, not by convenience:

- **STORE — every DERIVED fact the evaluation reads.** Predicate verdicts, aggregates, unions: computed ONCE by
  the ONE derivation for that fact and maintained by the spine events, never recomputed at read. This is the
  context's substance. It is derived state, so it is **never serialized** and is rebuilt at load by the reseed
  ([derived data is never trusted from a save](specs/save.md#5-derived-data-serializes-nothing-), [the load reseed](spine.md#5-the-load-reseed)).
- **FORWARD — only the object's OWN RAW data** that it already maintains O(1) (the substrate ids a parameterized
  predicate keys on, population, …). Forwarding raw data is not duplication; storing a second copy of it would be.

⚑ **THE PAYOFF — this is why the design earns its cost (owner): once contexts are PURELY event-updated, an
enormous class of per-read CALCULATION becomes obsolete.** Not "gets faster" — ceases to exist. Every read-time
scan/union/walk collapses into a stored value some event already maintained, and reads become bare fetches
(§ THE MAINTAINED SUM, above; [turn time is king](#-the-per-scope-package-model--the-cascades-founding-design-1-stated-as-cache-architecture)).
The in-tree exhibits are not hypothetical: `isCoastalLand()` is an 8-neighbour scan **per predicate
evaluation**, and the §5a vicinity check is a radius union **per check**. The win is STRUCTURAL: once the fact
is stored there is no read-time work left to do, so cost tracks EVENT volume (what changed), never read volume
(how often it is asked) — and it is observed where every performance claim is observed, on the per-turn wall
clock ([turn time is king](#-the-per-scope-package-model--the-cascades-founding-design-1-stated-as-cache-architecture)).

⛔ **BUT THE TEST IS A SCAN, NOT A HOP — and `getNumBonuses` is the case that marks the line.** What earns a
store is read-time work that GROWS with something (neighbours, radius tiles, a registry). A read that resolves
through a POINTER to the object which already owns the number O(1) is not that, and storing it anyway makes a
third copy of one fact ([enabler.md §8](specs/enabler.md) RESIDENCY: the plot group owns the network count,
the city relays it, the context forwards the relay). ⚠ This one had a store on exactly that mistaken reading,
and it cost a sweep of every bonus on every fact that could move one — strictly more work than the hop it was
avoiding. **Ask what the read WALKS; if the answer is "one pointer", forward it.**

> **⛔ SO A CONSUMER NEVER WALKS AN INFO'S KEYED LIST TO ASK A PER-ITEM LIVE-STATE QUESTION — THE EVENT-BUILT
> READ-ONLY STATE ANSWERS IT (owner).** *"There should be no iterating like that; the eventspine-built read-only
> should be able to handle that."* The shape to recognise is `foreach_(key in someInfo.getKeyedList()) { …
> liveStateRead(key) … }` — the info supplies the keys and the loop asks the live state once per key. That is the
> per-read scan this whole section deletes, merely sourced from an info instead of from the map.
> ⚑ **The worked case is the corporation's consumed bonuses:** `foreach_(bonus in corp's bonuses) getNumBonuses(bonus)`
> re-executes [enabler.md §8](specs/enabler.md)'s hottest cluster once PER BONUS, and where the question is a
> MAGNITUDE the answer is already authored — the rate carries a `per:{anyOf: consumed bonuses}` scaler, so the
> valuation resolves rate × count in one call and the loop simply disappears.
> ⛔ **And renaming the receiver is NOT the fix.** A walk that compiles against the new getter reads as migrated
> while doing exactly what it did before — the half-migration
> ([build a new getter surface, never widen a legacy one](architecture/patterns.md#-the-two-read-roles--one-grammar-two-answers-owner)), and it hides the hole the maintained read has
> not yet filled ([legacy must fail loud, never mask a cascade gap](specs/validation.md#legacy-must-fail-loud-never-mask-a-cascade-gap)). Leave such a site DANGLING as the
> census entry it is until the maintained fetch exists.

⛔ **A forwarded read that COMPUTES is the defect this rule exists to kill.** `PlotContext::hasCoast()` forwarding
to `CvPlot::isCoastalLand()` — an 8-neighbour scan with an `area()->getNumTiles()` call per neighbour, on every
predicate evaluation — is the worked example, and it directly contradicts
[patterns.md](architecture/patterns.md): *"every evaluator predicate is an O(1) CONTEXT fetch … a predicate that walks
plots/units per call is the efficiency defect to reject in review."*

**The storage is keyed by the CONDITION VOCABULARY** — that is what makes the state distinguishable. One key
space (`CASC_PRED_*` / the classification ids), three granularities of the same design:

| context | stores | granularity |
|---|---|---|
| **PlotContext** | the plot's own predicate verdicts | a `CASC_PRED_*` BITSET |
| **CityContext** | `plotAttrs` | per-predicate COUNTS over the same keys — the FOLD of its member plots' bits |
| **EmpireContext** | `policies` | the `POLICY_*` id set (the union over live civics + held traits) |

So the city aggregate is not a second derivation: `CvCity::onCityPlotChanged(plot, ±1)` folds the plot's bits,
and the two granularities cannot drift.

⚠ **Adjacency-derived predicates fan out.** `HAS_COAST` / fresh-water depend on NEIGHBOURS, so the event that
changes a plot re-derives that plot's own bits AND its adjacent plots' adjacency bits. Bounded (8 neighbours) and
event-driven — never a read-time scan, and never left on the old accessor as an interim.

| context | owner | STORES (unique aggregate) | FORWARDS (read through the bound object / its owner) |
|---|---|---|---|
| **CityContext** | `CvCity` | `plotAttrs` — per-predicate plot COUNTS (the fold of member plots' bits) · **`amenities`** — the `AMENITY_*` id→COUNT fold over the city's OPERATING buildings + the empire-scope grantors (json §8; the count is load-bearing — see the callout below) · **the VICINITY BONUSES available in the city** (owner) — the §5a radius union, MAP half (see the split below) · the **AREA facts** (area id, its tile count, the coastal water-body size) · the **holy-city and HEADQUARTERS counts** — how many religions / corporations name this city, each a delta store fed ±1 by its own fact · the **CORPORATION-ACTIVE verdicts** — the remembered per-corp `{HAS_CORPORATION}` verdict, held ONLY so its crossing can announce (`SEVT_CITY_CORPORATION_ACTIVE_ADDED / _REMOVED`, the fact plane C routes on): each leg's fact triggers a re-read of the ONE engine implementation (`CvCity::isActiveCorporation`, the sanctioned engine-owned input — the read side stays the live forward), and a re-read that moved nothing announces nothing | population, power, religion presence, holy-city-of, corporation, capital, government-centre, fresh-water access, property value (raw, `CvCity`-owned, O(1)); state religion (→ owner `CvPlayer`); **the TRADED count** — the gated network number, forwarded through `CvCity::getNumBonuses`, which relays to the PLOT GROUP that owns it ([enabler.md §8](specs/enabler.md) RESIDENCY: nothing mirrors the group); **the CURRENT REALIZED YIELDS** (owner) — the city's own O(1) group read, forwarded so a valuation can resolve a percent against a real base (below); **the CURRENT REALIZED COMMERCE** — `CvCity::getCommerces`, the per-commerce SPLIT of that commerce yield by the empire's sliders plus each channel's own deposits (§2a), forwarded for the same reason |
| **EmpireContext** | `CvPlayer` | `policies` — the empire's enacted-policy set (the derived UNION over live civics'/traits' policy blocks, stored nowhere else) · **the HELD-TRAIT set** — the `TRAIT_` id→COUNT fold, a delta store fed ±1 by the trait facts (§ the callout below: enumerating what a player holds is a SCAN even though testing one trait is a hop) | state religion (single enum → `CvPlayer::getStateReligion`), civics/**trait presence**/heritages, the team-held facts; **the CURRENT REALIZED COMMERCE** — `CvPlayer::getCommerces`, the four empire RECEIVER totals: the city-yields forward's empire twin, so an empire-scope percent resolves against a real base; **the COMMERCE SLIDER PERCENTAGES** (owner) — the player's gold / research / culture / espionage rates, the `GOLD_RATE`/`RESEARCH_RATE`/`CULTURE_RATE`/`ESPIONAGE_RATE` tokens ([json.md §3.1](specs/json.md)); a group keyed by `CommerceTypes`, forwarded because `CvPlayer` owns them O(1) |
| **PlotContext** | `CvPlot` | the `CASC_PRED_*` verdict **BITSET** — the OWN-PLOT block (water/land/relief/hills/peak/river/irrigation/feature-present/landmark/owned/**worked**) plus the ADJACENCY block (coast, fresh-water) · **`workableBy`** — the cities whose potential work area this plot is in, set by `CvCity::changeWorkableArea` and announced per plot (§ the VICINITY store) | the RAW substrate a parameterized predicate keys on — terrain/feature/improvement/route/bonus ids, owner, latitude, nature yield — plus city-presence, the one verdict with no mutation event a bit could be maintained from (→ `CvPlot`); **the plot's CURRENT REALIZED YIELDS** — `CvPlot::getYields`, the whole isolated per-plot base package as a bare cache fetch. ⛔ The PRE-IMPROVEMENT leg (`natureYield`) is a SECOND SLOT of that same package, never a per-call computation: it is asked per (plot × improvement × yield) by the placement gate and both improvement valuations, which is the cost class this whole section deletes. A read that recomputes it is the forwarded-read-that-COMPUTES defect above, and the number is already in the package |

⛔ **THE VICINITY SPLIT — the context holds the MAP half, the enabler holds the BUILDING half.** The §5a in-vicinity
supply is a union of two independently-owned halves, and storing either one twice is the duplication the model bans:

- **MAP providers** (a bonus on a radius tile providing itself) are per-scope live state with no other home, so
  `CityContext` holds them — tiered by the §3.4 ownership discriminator (`owned` / owned+neutral / `crossBorder` /
  `worked`), since the `vicinity` band selects which tiles count — the plot-set axis, distinct from `connection`.
- **ACTIVE BUILDING providers** (`provides.bonuses`) are the operate/provides **least fixpoint**, which only the
  enabler can resolve — an operate condition may consume a bonus another active building provides. They stay
  `OperatingBuildings::provided`, reached through `CvCascadeEvalCtx::vicinityProvidedBonuses`.

The reader unions the two. A mirror of the building half on the context would also *drift*, because the enabler
mutates its set in place as the fixpoint ripples.

> **⚖ THE MAP HALF IS TWO DICTIONARIES, NOT ONE — bonuses, and natural features (owner).** *"There is nothing
> wrong with having 2 dictionaries, 1 for bonuses and 1 for natural features; what I don't want is the constant
> rewalk."* So the vicinity store is a **`BONUS_*`-keyed** dictionary beside a **`CASC_PRED_*`-keyed** one (the
> vicinity twin of `plotAttrs` — river / coast / hills / peak / fresh water), each an ordinary `ContextDict`.
> ⛔ **They are NOT merged into one dictionary**, and the reason is the one `ContextDict` already states as its
> first: the two key spaces are DISJOINT REGISTRIES both starting at 0, so a merged store re-opens the
> cross-registry id collision the `CLS_` prefix closed by construction. One dict per area of responsibility.
> ⚑ **The objection the ruling answers is the REWALK, never the count of dictionaries** — a second dictionary
> costs one more `add(id, ±1)` on a fact that is already being handled, while the absence of one costs a radius
> scan per read. Adding a dictionary is how the walk disappears.
> ⚠ **The ownership TIERS partition; they do not nest in storage.** [json.md §3.4](specs/json.md) defines
> `owned ⊂ owned+neutral ⊂ crossBorder`, so storing them as overlapping tiers would double-count on a fold.
>
> **⚖ NEUTRAL IS THE DEFAULT STATE — IF THERE IS NO OWNER IT IS NEUTRAL (owner).** So neutral is **not stored and
> needs no fact**: the store holds `all` (the bonus is on a radius tile at all, moved only by the BONUS facts)
> beside the two ownership bands `owned` and `foreign`, and the neutral count is the RESIDUAL
> `all − owned − foreign`. The bands are then carved out of the total — `crossBorder` IS `all`, the default band
> is `all − foreign`, `owned` is itself.
> ⚑ **That is what makes the store maintainable at all.** `SEVT_PLOT_OWNER_ADDED / _REMOVED` are both guarded on
> `!= NO_PLAYER`, so they announce only the OWNED ends — a *stored* neutral tier would have no announced
> transition across `unowned ⇄ owned` and no delta could keep it correct. As a residual it needs none: all four
> transitions (`unowned→A`, `A→unowned`, `A→B`, and a bonus arriving) balance exactly, because each fact names
> the owner ITS half is about while the plot's own `m_eOwner` has already moved.
> ⛔ Two workarounds were considered and are wrong wirings: reading the dict to decide a withdrawal
> (`if (neutral.has(b)) add(b,-1)`) makes a GATE read the refcount and picks the wrong plot when two radius tiles
> carry one bonus; and composing the neutral end from the `IS_OWNED` predicate crossing double-applies on
> `unowned → owned`, where that crossing AND `OWNER_ADDED` both fire.
>
> ⚖ **THE SEEDING MOMENT IS `SEVT_PLOT_CITY_ADDED`, and it is ordering rather than taste.** `CvCity::init` sits
> the city on its plot and only THEN claims its radius through `updateCultureLevel`, while `emitCityFounded` comes
> later still from `CvPlayer::found`. That fact is therefore the one window where the city is already visible to
> the radius inverse and its radius has NOT yet taken ownership: the seed books what is already there, and the
> ownership claims that follow apply their bands for exactly the tiles that change. ⛔ Seeding at `CITY_FOUNDED`
> would double every band the claim had announced. ⚠ It is guarded to the non-load path, with the
> `GAME_LOAD_FINISHED` fold as its load-time twin — the map streams before the players, so at load the radius
> facts reach no city at all (the amenity fold guards its play-time fan the same way, for the same reason).
>
> `worked` and `onSite` are different predicates rather than ownership bands, so they stay their own — and each is
> its OWN stored dictionary. **`onSite` = an OWNED radius tile whose IMPROVEMENT trades the resource**, which is
> strictly stronger than `owned` (raw presence, improved or not) and therefore not a filter over it: two owned
> radius tiles can carry one resource with only one improved, so only a count answers it.
> ⛔ **IT NEVER CONSULTS THE NETWORK, and that is the ruling rather than an omission (owner): onSite and traded are
> two COMPLETELY SEPARATE LISTS, neither derivable from the other** — you can hold a resource on site and not in
> trade, *having traded your only copy to another civ*. A mounted unit needs horses ON SITE; a swordsman only needs
> iron wares in the NETWORK ([json.md §3.4](specs/json.md): the two are ORTHOGONAL, not nested).
> ⚑ **The tile's half is a VERDICT the PLOT owns and announces**, exactly as its predicate bits are: `PlotContext`
> holds the SERVED RESOURCE — an id, because a plot carries at most one bonus — derived from the bonus and
> improvement axes and announced as `SEVT_PLOT_SERVED_BONUS_ADDED / _REMOVED`. ⛔ A city-side derivation is
> impossible for the same reason the per-bit fact exists: by the time any consumer runs the plot already holds the
> new value, so the old contribution is gone. The OWNERSHIP half stays the CITY's, applied where the asker's own
> owner is known — no per-plot verdict can answer it for every city that may work the tile.
> ⚠ It is NOT gated on the tile being WORKED: a fort cannot be worked by definition, and a fort is exactly how a
> resource gets served (owner).
>
> **⛔ THE TWO ARE NOT PEERS, AND THAT IS THE WHOLE CONFUSION — VICINITY IS THE PLOTS, `onSite` IS A CONNECTION
> THROUGH THEM (owner):** *"vicinity is the plots actually in vicinity; if a bonus is on site, it means it's
> connected to a city via this vicinity band."* So a vicinity BAND selects WHICH PLOTS COUNT (the ownership
> tiers, and `worked`), while `onSite` is a VERDICT ABOUT THE BONUS reached through that band — the resource is
> available to this city because a tile in the band serves it, or an active building supplies it. One names a
> plot set; the other names how a resource arrives.
> ⚠ **The orthogonality is against the NETWORK, never against vicinity** — `onSite` and `connection:"trade"`
> are the two independent routes a bonus takes to a city (above). Reading `onSite` as "orthogonal to vicinity",
> or as merely a stricter band of it, are the two halves of the same mistake.
>
> **⛔⛔ AND HERE IS WHERE EVERY AGENT SCREWS UP, WHICH IS THE REASON THE BONUS LIST IS CALLED `onSite` AT ALL
> (owner): A BONUS SUPPLIED BY A BUILDING IN THE CITY IS *ALSO* VICINITY.** *"Agents could not get that bonuses
> given from buildings in a city is also vicinity, which is why it was changed to onSite for the bonuslist."*
> The word "vicinity" reads as TILES, so agent after agent took the building-supplied half to be something else
> — a different mechanism, a special case, or simply absent — and wrote reads that answer only the map half.
> ⇒ **The name was changed to stop that**: `onSite` says *the resource is available here*, without implying by
> what route, so the two halves sit under one word that cannot be misread as "on a tile".
> ⛔ So a reader that answers only the map half is WRONG whatever it is called, and "it says vicinity, so it
> means tiles" is the specific inference to refuse. The union is not an implementation detail of the read — it
> IS the read ([json.md §5a](specs/json.md): a herd BUILDING and an improved herd TILE are the same act).
> ⛔ **THE AXIS IS SPLIT, so the wrong call cannot be spelled at all** — `onSite` is a value of `connection` (the
> ORIGIN axis: `trade` = the network has it, `onSite` = it comes from the city itself, mutually exclusive), and
> `vicinity` is the PLOT-SET axis and nothing else. A gate wanting either origin states two atoms under an `any`,
> deliberately; there is no combined selector and none may be reintroduced
> ([json.md §3.4](specs/json.md)).
> ⚑ **Why prose was not enough, recorded because it is the general lesson** — a tier-less `hasVicinityBonus`
> silently answered the on-site verdict, and the first fix was a comment telling the next caller to name its
> tier. It recurred ([the hard-typing-or-rollerskate rule](../AGENTS.md#design): prose is the
> weakest rung, binding only an agent who reads it, believes it, and still remembers it). The split is what makes
> it unsayable instead.
> ⇒ **The NAME binds too: a function whose answer can be the on-site verdict does not carry `vicinity` in its
> name** — `EnablerKernel::cityHasBonusOnSite` for a caller holding a `CvCity*`, `ev_vicinityHas` for the
> evaluator, which must stay ctx-shaped because the eval ctx is forbidden a game object (§ THE EVAL CTX, below). Two
> entry points because they reach different planes; ⛔ neither may answer with one half.

⚖ **`CityContext.amenities` — THE CITY'S OWN FEATURE LIST, AND THE CITY IS WHAT GETS CHECKED (owner).** A
grantor's `amenities` block ([json.md §8](specs/json.md)) is static info data; what a consumer actually asks
is *"does THIS CITY have this?"*. So the city holds the FOLD — over its **operating** buildings, plus the
empire-scope grantors (civic / trait / tech) that reach every city — and every gate reads it O(1). ⛔ A consumer
must never loop the city's buildings asking each one, and a grantor's per-key named getter is not the consumer
surface: the fold is the ONE reader of the grantor side
([the DRY single-implementation law](architecture/patterns.md#dry--one-implementation-per-calculation--evaluation-the-single-source-law)), alongside display/pedia.

> **⛔ IT IS A ContextDict (id→COUNT), NOT A BITSET (owner) — absent or 0 is false, anything else true.** Several
> grantors can confer the SAME amenity, so a removal DECREMENTS rather than clears: *losing one power plant must
> not darken a city that has two.* A bitset cannot express that — an "amenity removed" fact would clear a bit
> another live grantor still justifies. ⚑ So it is the ordinary `ContextDict` this doc already specifies
> (`has(id)` ≡ `count > 0`), the same refcount shape the enabler's membership formula and the operating set's
> provided-bonus counts use, and the semantic legacy already had in its per-flag counters.

> **⛔ THE DICTIONARY IS THE FINAL STOPPING PLACE — IT IS WHERE THE DATA ACTUALLY LIVES (owner).** Every grantor
> fact lands here and comes to REST: the building leg off the enabler's active↔dormant crossing (a dormant
> building confers nothing, [enabler.md §3.2](specs/enabler.md)), the civic / trait / tech legs off their own
> facts. ⛔ It is NOT a projection of some other system's truth, and it is NOT relayed from the enabler — the
> enabler is a SOURCE OF FACTS, never the home of this answer. One dictionary, every leg, one mechanism, and
> every reader — the enabler's own gate included — reads it HERE.
> **⛔ AND IT IS ITSELF A SPINE CONSUMER THAT KNOWS EXACTLY WHICH EVENTS TO LOOK FOR (owner).** A dictionary
> REGISTERS on the spine and DECLARES the precise set of facts that maintain it; it is not fed by a central
> switch that fans out to whichever store a case happens to name. ⚑ **The interest set IS the maintenance
> contract, which is what makes it auditable at all:** with a fan-out, "does this fact reach the store that
> needs it" is answerable only by reading the router, so a missing route hides in a `switch` that looks
> complete; with a self-declaring dict the gap is visible AT the dict. It is also what makes the RECEIVED line
> name something useful — the consumer that acted is the dictionary, by name
> ([spine.md](spine.md) § THE RECEIVED LINE).
> ⚑ It is the same move as the spine's own per-domain isolation: adding a domain touches only that domain, and
> adding a dictionary now touches only that dictionary — no shared edit, no central case to remember.
> ⚠ **REGISTRATION ORDER REMAINS A CONTRACT and self-registration must not quietly break it.** The enabler's
> load-end gate pass evaluates THROUGH these stores, so every dictionary registers inside the CONTEXTS band of
> `contexts → enabler → modifier → triggers` ([enabler.md §8](specs/enabler.md)) — ordering is a property of
> the band, never of which translation unit happened to initialize first.
> ⛔ This does not license one consumer per SYSTEM being violated ([the enabler and the modifier cascade are two separate systems](specs/enabler.md)):
> that ban is on one consumer routing TWO MACHINES, not a cap of one consumer per machine. Several dictionaries
> inside the contexts band are still exactly one system's worth of maintenance.
>
> **⛔ A CONTEXT DICTIONARY ONLY EVER CONSUMES; IT NEVER EMITS (owner) — which is why it can close no loop.**
> Facts go in, state comes out, nothing goes back. A later read of that state by the machine whose fact fed it is
> an ordinary read of CURRENT state, not feedback. ⚠ **The ordering ban at the top of this section does NOT
> reach it, and reading it as though it does is the misapplication to avoid:** that ban is on a store that
> RE-DERIVES BY READING another system's built set — which cannot run until that system is built. A
> delta-CONSUMING store has no such dependency; it builds identically whenever the facts arrive, which is
> precisely why the delta form is the one this document prescribes.
> ⚑ Distinct from `ecOp.activeBuildings = NULL`, which breaks a genuine RECURSION INSIDE THE EVALUATOR (an
> operate condition asking for the very set being computed). A dictionary updated by an earlier synchronous fold
> is not recursion; it is simply current.

> **⚖ THE FOLD HAS TWO LEGS, BECAUSE THE GRANTORS SIT AT DIFFERENT SCOPES — one implementation, two triggers.**
> A BUILDING confers on its OWN city, so its leg is a pure delta off the per-building fact and needs nothing
> else. A CIVIC confers on EVERY city of the empire ([json.md §8](specs/json.md)), and that leg cannot ride
> the grantor fact alone: **at load the civic facts fire from `CvPlayer::read` BEFORE the cities deserialize**,
> so there is no city to fan to. It therefore folds from the other side — **when a CITY starts existing** (the
> load build, and city-founded) it folds what its owner already holds — while the grantor fact fans the delta
> (`−`old, `+`new) over the cities that already stand.
> ⚠ **Both halves are needed, and the load ordering is NOT uniform across grantors:** the civic reseed emits
> before the cities, but the TRAIT reseed emits *after* them. So the play-time fan is guarded to the non-load
> path — unguarded, a trait would be counted twice against the load build.
> ⚑ Reading the owner's adopted civics there is a **FORWARD of raw, object-owned state**, not the banned
> re-derivation. What is forbidden is a store reading ANOTHER SYSTEM's built state (the enabler's operating
> set) — that is what manufactures an ordering dependency; `policies` already makes exactly this read.

> **⚖ POWER IS AN AMENITY, AND IS TREATED AS ONE (owner).** `CvCity::getPowerCount` reads the `providesPower`
> fold rather than a hand-named counter, and the counter, its changer and its Python binding are gone. ⚑ The
> REFCOUNT is what earns it: losing one of two power plants must leave the city powered, which is precisely the
> failure a plain counter or a bitset cannot express.
> ⚑ **The fold ANNOUNCES its crossings** (0 ⇄ non-zero, never a second grantor of a key the city already holds),
> because a consumer routing on an amenity must not re-derive which key moved — the modifier's `HAS_POWER`
> dependency route and the enabler's power gate both ride that fact.
> ⛔ **Where a STATUS gates delivery, the announced crossing is the GATED verdict's, not the store's** — for power,
> `CvCity::isPowered` rather than the refcount ([state.md](specs/state.md) § A STATUS IS MIDDLEWARE). The two
> genuinely differ, so announcing the store would put the fact and every consumer's read on different values; the
> status reaches this fold for that reason alone and never becomes a store entry or a cascade input.
> ⛔ The crossing is emitted by the FOLD, not by a mutation site: the fold IS the maintenance path, so an emit
> anywhere else would be a second one.

**Every boolean city attribute of this shape is generalized onto the ONE fold, not just power:**
`governmentCenter`, `abolishedAnger`, `abolishedUnhealthFromPopulation` and `abolishedUnhealthFromBuildings` are
all ordinary `CITY_HAS_AMENITY` keys (`CvCity::isGovernmentCenter` / `isNoUnhappiness` /
`isNoUnhealthyPopulation` / `isBuildingOnlyHealthy`), so a NEW attribute of this shape costs no engine
change — it is pure data, the open-registry promise ([the classification-infos registry](specs/json.md#8-classification--unit-skillstagsstate-building-attributes--empire-capabilities))
reaching the consumer side. ⛔ There is no surviving hand-named counter for any of them — no
`changeGovernmentCenterCount` / `changeNoUnhappinessCount` / `changeNoUnhealthyPopulationCount` member on
`CvCity` — the fold replaced them, exactly what
[every derived cache is one shape](#-every-derived-store-is-one-shape--a-keyed-accumulator-maintained-by-a-delta-owner) asks for.

⚠ **Two attributes that LOOK like the same family are deliberately NOT on this fold, and neither is a hole to
close by routing it there.** `governmentCenterDistance()` is a separately STORED value (§ the sanctioned-recalc
exemplar, below) because it answers a MIN over the player's centres — a different question from
`isGovernmentCenter()`, which the amenity fold already answers. And `HAS_FRESHWATER` is a **`PlotContext`** bit,
not a `CityContext` amenity, that deliberately keeps calling the live `CvPlot::isFreshWater` engine predicate
rather than folding onto a dictionary (§ the adjacency callout, below —
[the DRY single-implementation law](architecture/patterns.md#dry--one-implementation-per-calculation--evaluation-the-single-source-law)).

**Pass by reference/pointer, never by value (owner).** Passing a bound context is far cheaper than snapshotting
values; a context is never a value copy — that is *why* it forwards rather than mirrors.

**⚖ THE TWO PASS-IN SCENARIOS (owner) — a context crosses a call boundary in exactly TWO places, the two
condition-evaluation sites:** (1) **the VALUATION** — the `expected*` per-group reads and the package rebuild's
conditioned-deposit evaluation (the same machinery at event cadence); (2) **the `requires` edge** — the
enabler's build/operate gate incl. the operating-set fixpoint, re-run at HAVE-change over the affected
candidates. Both go through the ONE evaluator over the eval ctx the contexts fill. Every other read on every
surface is a straight compiled fetch and NEVER takes a context parameter — a context in any other signature is
the mechanical smell that condition evaluation (or an ad-hoc state reach) is happening where it doesn't belong.

### ⛔ THE EVAL CTX CARRIES CONTEXTS, NOT GAME OBJECTS (owner) — the contract must be STRUCTURAL

**"Otherwise we can just pass the full player, city, and whatever other objects again, without any
distinguishing."** That is the whole test, and it is a CONTRACT, not a prohibition: if the evaluation context
holds a `CvCity*`/`CvPlayer*`, then "the reader goes through the context" is enforced only by reviewer memory —
the god-object is right there, and reaching past the context is one `->` away (a derived
`&ctx.city->getCityContext()` is the tell: the ctx never held a context at all). The isolation must be
**unsayable to violate**, exactly as [patterns.md](architecture/patterns.md) states the info DATA-OUT contract: there is no
member to reach through.

So `CvCascadeEvalCtx` carries **`const CityContext*` / `const EmpireContext*` / `const PlotContext*`** — never
the bound game objects. Consequences, each already implied by the model above:

- **TEAM facts route through `EmpireContext`** (team is deliberately not a context; team-held techs are read
  through the player) — the ctx carries no `CvTeam*`.
- **`CvPlotGroup` STAYS a first-class ctx member** — it is the reserved explicit traded-bonus source (§ the
  read, below), not a scope whose state a context owns.
- **`CvUnit` stays raw FOR NOW** — units are the deliberate FUTURE role-specific scope; when that context
  lands it replaces the pointer, and until then this is the one acknowledged hole, not a licence for others.
- **The enabler's precomputed sets** (operating/active/obsolete buildings, vicinity-provided bonuses) stay ctx
  members: they are the ENABLER's derived output fed to the evaluator, never per-scope live state.
- **⚖ THE SOURCE SLOTS — a predicate about the CARRIER needs the carrier named, because an entry cannot name
  itself.** Neither a compiled entry nor an info knows its own engine id, so a condition asking about the
  DEPOSITING entity rather than the target (`existedFor` — how long has THIS building stood) has no other way
  to ask. The ctx therefore carries a slot per such axis — `religion` (the §3.7 counted-kind filter),
  `sourceBuilding`, and `civic` (the `{CIVIC_CATEGORY}` predicate's carrier, set by the civic-upkeep resolve) —
  **set per-iteration by the walk that knows the id, on a LOCAL COPY of the ctx, and -1
  everywhere else**; the shared ctx is never mutated. ⛔ -1 means "no carrier in hand" and a source-predicate
  must answer FALSE there: a scope-wide read that never set it would otherwise resolve against whichever
  entity the walk happened to reach last.
  ⚠ **A source slot is only meaningful where the carrier is SINGULAR.** The city fold resolves one building at
  a time, so it sets one; the EMPIRE fold walks a building the player may hold N copies of, each acquired at a
  different moment, so the question has no single answer and the slot stays unset by design. That is a
  structural limit, not an unwired leg — do not "complete" it by setting the slot there.
- **A context that cannot answer a needed fact is a CONTEXT GAP to close** (add the forward), never a reason to
  re-add an object pointer. That is the forcing function the structural form buys.

**⚖ THE HAVE AXIS LIVES IN THE CONTEXTS (owner).** What a scope POSSESSES — the city's buildings-present /
religions / corporations / bonuses, the empire's civics / traits / heritages, the team-held techs (read through
the player's team — team is deliberately not a context) — is read through that scope's context, never by an
ad-hoc reach into the game object. The STORES-vs-FORWARDS discipline above is unchanged: possession state the
object already owns O(1) is FORWARDED, and only a homeless aggregate is stored (`policies` is the realized
exemplar). The context is the RESPONSIBILITY home — the one place every reader (the evaluator's atoms, the
enabler's gates, the `expected*` valuations) goes for HAVE. The enabler's DERIVED sets (the domain vectors, the
operating-building set) remain enabler-owned ([enabler.md §7](specs/enabler.md)); the contexts serve the raw
possession facts those machines gate against.

**⚖ IF IT IS CURRENT STATE, IT IS THE CONTEXT'S — there is no third home (owner).** A value that looks like it
needs a new category almost always just IS current state, and current state has one home. The worked case: the
city's **thresholds** (growth / culture / great-people — what is REQUIRED right now, moving with population,
gamespeed and era), its **turns-left** projections, and its cross-city **ranks** are none of them a new kind of
read — they are this city's state now, so they are asked of the context like every other state fact. The
STORES-vs-FORWARDS split then decides each one on its own merits (a value the object already computes O(1) is
FORWARDED; only a homeless aggregate is stored) — the classification is never "invent a shape for it".
⚠ **Ranks are the case that STRESSES the rule**, and stressing it is not breaking it: a rank is a comparison
ACROSS cities, so no single city owns the answer and a stored rank would need maintaining on every sibling's
change. Read it at the scope that can actually answer it — the player — rather than bending the city's context
around it.

### COUNTS, not objects — "how many, not which" (owner)

An aggregate holds **counts keyed by id**, never the objects themselves. A building cares HOW MANY river plots /
vicinity bonuses it has, never WHICH. So a `plots`-target (or keyed) deposit's output is `flat × count(id)`, and a
gate is `has(id)` (count > 0).

> **⚖ THAT PRODUCT IS A YIELD, AND THE DICTIONARY IS ONE OF ITS TWO OPERANDS (owner).** `Δ(flat × count) = flat ×
> Δcount` is exact — this is plane **B** of the maintained sum
> (§ THE MAINTAINED SUM, above), and it is why a count fact is emitted at
> all: an `add(id, ±1)` IS a yield delta, never a re-derivation.
> ⛔ **So a dictionary is not merely a gate store beside the value plane — it IS part of the value plane.** A
> count with no route leaves every deposit scaled on it permanently wrong, exactly as a missing source fact does. The uniform keyed dictionary is **`ContextDict`** (`id → count`, read `has`/`count`,
maintained `add(id, ±1)`, zeroed `clear()` at owner reset — **there is deliberately no `set`**, which would
overwrite a refcount) — ONE kind, shared by every context, so the read is uniform and each family's key set is
OPEN (a new predicate/type is a new key, never a reshape). It is also the destination the mark-and-recompute
component retires ONTO ([ContextDict replaces CvDerivedCache](#-cvderivedcache-is-replaced-by-contextdict--virtually-everywhere-owner)). `plotAttrs` keys on the `CASC_PRED_*` HAS_/IS_ plot
predicate ids; `policies` on the `POLICY_*` classification ids.

Non-dictionary scalars stay plain: population/power are `int` (power carries 0/1 today but stays `int` so a future
**volumetric** model needs no reshape); state religion is a **single enum**, not a dictionary (there is exactly one).

### Maintained EVENT-DRIVEN — never a per-turn recompute

> **⛔ WE DO NOT DIRTY CONTEXTS — THAT IS THE BOTTOM LINE (owner).** A context store carries **no staleness
> mechanism of any kind**: no flag, no stamp, no epoch, no rebuild entry point, and no `refresh*`. **The FACT
> SETS the bit it names and MOVES the count it names**, and that is the ENTIRE maintenance path
> ([a context is never marked or refreshed](#maintained-event-driven--never-a-per-turn-recompute),
> ["dirty" is not a term we use](#-a-staleness-flag-is-the-fossil-of-an-incomplete-emit-surface--the-same-rule-one-level-up)) — the same rule § A
> STALENESS FLAG IS THE FOSSIL OF AN INCOMPLETE EMIT SURFACE states above, one plane over: what varies is only
> whether the slot holds a magnitude or a gate, never how it is kept current.
> ⛔ Re-deriving a whole BLOCK because something in its vicinity happened is the legacy read path RESCHEDULED
> from read-time to event-time, not deleted — the same single error the packages express per CHANNEL and the
> contexts express per BLOCK.
>
> **⚖ A PLOT'S PREDICATES FOLLOW MEMBERSHIP, AND OWNERSHIP IS A MEMBERSHIP FACT (owner).** *"When a city gains or
> loses ownership, the `HAS_RIVER`, `HAS_COAST` and whatever other predicates associated with that plot need to
> be added to / removed from the city in question — that is how it has to work."* So the ONE applier
> (`CvCity::onCityPlotChanged(plot, ±1)`, which folds the plot's STORED bitset) fires on **every membership
> change**, not on the worked-radius relation alone:
>
> | membership fact | what moves |
> |---|---|
> | the plot gains / loses this city's OWNERSHIP | the whole of that plot's bits, `±1` each |
> | the plot enters / leaves the worked radius | the same fold, same applier |
> | a MEMBER plot's own bits move | **the PLOT announces the bit** -- `add(bit, ±1)`, nothing re-derived |
>
> ⚑ **THE FIRST TWO ROWS ARE ONE FACT, not two routes — `CvPlot::setOwner` CALLS `updateWorkingCity`.** So an
> ownership change re-assigns the working city and announces
> `SEVT_PLOT_WORKING_CITY_ADDED / _REMOVED`, which is the membership fold; a city cannot work a plot it does not
> own, so the two triggers cannot come apart. ⛔ **Adding a second route on `SEVT_PLOT_OWNER_*` into the same
> applier would therefore DOUBLE-COUNT** — the wrong-wiring class
> ([neither playability nor compiling gates removing legacy](specs/validation.md#playability-not-a-gate)), not a gap to close.
> ⚠ The ORDER composes exactly, which is worth knowing rather than re-deriving: `setOwner` writes `m_eOwner`
> first, so the `IS_OWNED` bit crosses and is withdrawn by its own predicate fact BEFORE `updateWorkingCity`
> folds the remaining bits out — no bit is subtracted twice.
>
> ⚑ **One applier, several facts** — never one fact per relation with its own derivation, and never a re-scan of
> the city's plots to find out what it now has.

> **⚖ THE SANCTIONED EXCEPTION — AN EVENT-TRIGGERED RECALC, WHERE THE FACT CANNOT NAME WHAT MOVES (owner).**
> *"It is the best example of event triggered recalc we need."* The rule above assumes the fact NAMES the thing
> that moved, so the applier can set it. Where that assumption fails the recalc is CORRECT, and banning it on the
> word `refresh` mistakes the name for the mechanism.
> ⚑ **The exemplar is `DISTANCE_TO_GOVERNMENT_CENTER`** (`CityContext::refreshGovernmentCenterDistance`, driven by
> `SEVT_CITY_AMENITY_ADDED / _REMOVED` filtered to the `governmentCenter` key): the value is a
> **MIN over the player's government centres**, so
> a centre appearing in ONE city can shorten the distance for EVERY city, and one disappearing forces a re-derive
> against the remaining set. The fact names the city that gained or lost the designation; the values that move
> belong to all the others.
> ⇒ **THE TEST, and it is narrow:** a recalc is sanctioned when (1) a genuine DOMAIN fact triggers it, (2) the
> consequence is NON-LOCAL — the fact cannot name the values that move — and (3) no finer route exists to derive,
> because the quantity is an aggregate over a set (a min, a nearest, a wholesale identity reassignment) rather
> than a sum a delta could carry. `SEVT_AREAS_RECALCULATED` is the other instance, for the same reason.
> ⛔ **What stays banned is unchanged, and none of it is this:** a recalc with NO naming fact (per-turn, blanket,
> or on-read); one that papers over a MISSED invalidation
> ([self-heal is not a backstop](#-a-self-heal-is-the-fossil-of-a-missing-emit--so-it-is-a-search-not-just-a-ban)); and one that RE-DERIVES A WHOLE BLOCK because something in
> its vicinity happened when the fact could simply have set what it named — the legacy read path rescheduled to
> event time, which is what the callouts above retire.
> ⚠ So *"a context carries no `refresh*`"* is about the MECHANISM, never the spelling: the question to ask of one
> is **what triggers it, and could the fact have named the value instead** — not what it is called.

The stored aggregate rides events, exactly like the rest of the spine; a missed event drifts it, but that is the
event spine's **baseline invariant** (plot-groups and vicinity drift the same way if events are incomplete), not a
context-specific weakness. There is **no blanket per-turn rebuild** and no recompute-on-read.

⛔ **AND NOTHING HEALS A MISS — that is what makes incomplete wiring safe to grow (owner).** No periodic or per-turn
context refresh, no "rebuild if it looks stale", no lazy recompute-on-read when a store looks empty, no staleness-timer
sweep, no validity/epoch stamp that triggers re-derivation, and no "recompute once per turn to be safe" backstop —
not as a safety net, not transitionally, not "just for load". If a store ever seems to need a "make sure it's
current" call, that is a **missing fact to report**, never a recompute to add — the full reasoning (why a missing
emit is the failure that should survive) is § A SELF-HEAL IS THE
FOSSIL OF A MISSING EMIT, above ([self-heal is not a backstop](#-a-self-heal-is-the-fossil-of-a-missing-emit--so-it-is-a-search-not-just-a-ban); CAPSTONE — LOAD is the only full build).

- **`PlotContext`'s verdict bitset** ← the plot-substrate DOMAIN facts — terrain / feature / improvement / route /
  bonus / owner / **plot type / river / irrigation / landmark / worked** — consumed by `PlotContext` ITSELF
  ([a context dictionary is a spine consumer](#what-a-context-stores-vs-forwards---a-context-is-an-event-built-store-not-a-forwarding-facade-owner)), which sets the bits the announcing fact FEEDS
  and nothing else.
  > **⚖ THE ROUTING IS DERIVED FROM A PER-BIT TABLE, never hand-written per event.** Each bit declares its own
  > derivation AND the substrate AXES it reads, side by side; a fact re-derives exactly the rows whose axes it
  > moved. That is what answers the hazard the retired whole-block derivation was right about — a hand-written
  > per-event bit mask drifting from what the bits actually read — without recomputing everything to avoid it. A
  > new bit is one row; a new fact is one axis.
  > **⚖ HAS_COAST IS SYMMETRIC: LAND WITH ADJACENT WATER, *OR WATER WITH ADJACENT LAND* (owner).** Off the stored
  > bits that is ONE statement — **a neighbour whose `IS_WATER` differs from mine**, i.e. the plot sits on the
  > land/water boundary — so the verdict reads entirely off blocks the stores already hold.
  > ⚠ It also fixes a live defect: the derivation this replaced called `isCoastalLand()`, which returns false for
  > a water plot outright, so **every water tile read `HAS_COAST` false**.
  > ⚑ **And it is what deletes the deferred-drain machinery.** The only reason the old derivation touched `CvArea`
  > at all was `isCoastal`'s `>= iMinWaterSize` test, and the bare predicate passes `-1` — a comparison no existing
  > area can fail. With no area dereference there is no unsettled-map window to defer against, so the mark/drain
  > pass, its per-plot byte vector and the `isFinalInitialized` gate all go. *(The city-scope
  > `{HAS_COAST:{minArea:N}}` form is the one that genuinely needs the water-body SIZE, and it stays
  > `CityContext`'s.)*
  > ⚖ **`HAS_FRESHWATER` keeps calling `CvPlot::isFreshWater`, deliberately** — a seven-leg verdict the ENGINE
  > still consults for irrigation and farm gates, so re-expressing it over stored bits would fork a live predicate
  > into two implementations that drift ([the DRY single-implementation law](architecture/patterns.md#dry--one-implementation-per-calculation--evaluation-the-single-source-law)).
  > Deriving a bit by calling the ONE accessor is what that rule asks for; what is banned is re-deriving the WHOLE
  > BLOCK, which per-bit routing is precisely what stops. Its neighbour leg is the one walk left, and it is now
  > paid only when a fact that actually feeds it arrives.
  > **⛔ THE PRICE OF CALLING A LIVE ENGINE PREDICATE FROM A LOAD-STREAM FACT: IT MUST BE TOTAL AGAINST THE
  > NOT-YET-READ SENTINEL.** `CvMap::read` fills the map ONE PLOT AT A TIME and each plot announces as it lands,
  > so a derivation that reaches a NEIGHBOUR reaches one that may still hold `NO_TERRAIN` / `NO_FEATURE` / no
  > city. Every such leg tests its sentinel and answers false; an unguarded `getTerrainInfo(NO_TERRAIN)` is a
  > fail-loud info-plane read ([the info plane is write-once-at-load](architecture/patterns.md#-write-once-at-load--a-read-never-creates-and-an-unanswerable-read-fails-loud)) and kills the
  > load outright. ⚠ **The self-correcting load order does NOT cover this** — it guarantees the VALUE converges
  > (the neighbour's own fact fans back and both sides re-derive), which is worth nothing if the first pass
  > raised. Convergence is about the answer; totality is about surviving to give one.
  > ⚑ **The distinction that decides which bits are exposed: what the leg READS.** `HAS_COAST` reads the
  > neighbour's STORED `IS_WATER` bit, so an unread neighbour reads false and the row is safe by construction;
  > `HAS_FRESHWATER` reaches through a live `CvPlot` accessor into the info plane, so it is not. That is the cost
  > of the [the DRY single-implementation law](architecture/patterns.md#dry--one-implementation-per-calculation--evaluation-the-single-source-law) carve-out above, and it is worth
  > paying — but it is paid HERE, by making the engine leg total, never by re-introducing a deferred drain pass.
  > **⛔ THE FACT SETS THE BIT — it does not trigger a callback that goes and asks (owner).** *"Those 'refresh'
  > functions are legacy-inspired rollerskating."* Re-deriving the WHOLE block through the same `CvPlot`
  > accessors a read used to call is the legacy read path RESCHEDULED from read-time to event-time, not
  > deleted — and this document bans that exact computation two sections up (§ a forwarded read that COMPUTES,
  > whose worked example is `isCoastalLand()`'s 8-neighbour scan). Running it once per EVENT instead of once per
  > READ is the same defect on a different clock.
  > ⚑ **It is ONE error on two planes, not two errors (owner):** recalculate-instead-of-delta-derive, which the
  > packages expressed per CHANNEL and the contexts express per BLOCK. ⚠ Their ORIGINS differ and that is worth
  > keeping straight — the package protocol was designed that way and faithfully built
  > ([superseded-ideas](architecture/superseded-ideas.md) #30: a superseded design, not a rollerskate), while these imported
  > the legacy read path. Same shape, different provenance, one fix.
  > ⚑ **It is [a staleness flag is the fossil of a missing emit](#-a-staleness-flag-is-the-fossil-of-an-incomplete-emit-surface--the-same-rule-one-level-up) wearing a second costume: both
  > throw away the fact's identity.** A staleness flag reduces the fact to *"something moved"*; a whole-block
  > re-derivation ignores WHICH bit the fact names. The spine already carries the answer: the fact NAMES the new
  > terrain, so a terrain fact SETS `IS_WATER` and never calls back to ask what the terrain is.
  > ⚠ **What the retired justification was right about, so the fix does not re-introduce it:** *"one uniform
  > derivation, never a bespoke per-event bit mask"* guarded against a hand-written per-event mask drifting from
  > what the bits actually read — the same hazard [every derived cache is one shape](#-every-derived-store-is-one-shape--a-keyed-accumulator-maintained-by-a-delta-owner)
  > names. The answer is the packages' answer: **DERIVE the routing, never hand-write it.** What each bit reads is
  > declared beside that bit's own derivation — a small, checkable, per-BIT statement (eleven of them), never a
  > per-EVENT judgement call.
  > ⚑ **The ADJACENCY half cannot be set from one plot's payload and does not need to be rescanned either:** a
  > neighbour's coast / fresh-water verdict reads the announcing plot's **STORED block**, never a fresh walk back
  > through `CvPlot`. Same move, one hop out.
  > **⚖ THE FAN-OUT RIDES THE AXIS, NEVER A BIT'S OWN CROSSING — that is what BOUNDS it to one hop.** Only
  > `TYPE` and `TERRAIN` are neighbour-visible (a neighbour's `HAS_COAST` reads my `IS_WATER`; its
  > `HAS_FRESHWATER` reads my water + fresh-terrain state), so those two axes re-derive the 8 neighbours'
  > adjacency rows and nothing else does. An adjacency verdict is read by nobody's adjacency verdict, so a
  > cascade is structurally impossible rather than merely avoided — and `IS_WORKED` is excluded by construction
  > rather than by an exclusion anyone has to remember.
  > ⚑ **It also makes the LOAD ORDER self-correcting, which is what retires the drain pass:** whichever plot of a
  > boundary pair is read second re-derives BOTH sides, so a stream that fills the map in any order converges with
  > no deferral, no marks and no final sweep.
  **The store's own consumer is the ONLY maintenance entry** — every plot mutation that moves a stored verdict
  emits its own DOMAIN fact, so no choke point calls a derivation directly
  ([the DRY single-implementation law](architecture/patterns.md#dry--one-implementation-per-calculation--evaluation-the-single-source-law)).
  > **⛔ THE CHOKE POINT ANNOUNCES; IT DOES NOT APPLY.** `CvPlot::updateWorkingCity` used to fold DIRECTLY beside
  > its emit, with the consumer skipping the fact at play to compensate. That is a SECOND surface maintaining one
  > fact, and it double-counts the instant the consumer grows the route — the exact failure
  > [a context dictionary is a spine consumer](#what-a-context-stores-vs-forwards---a-context-is-an-event-built-store-not-a-forwarding-facade-owner) exists to prevent. The mutation site owns the
  > SOURCE, never the store. ⚑ Nothing is lost by moving it: `emit()` dispatches SYNCHRONOUSLY, so the fold still
  > lands at that instant, and each side resolves ITS city from the fact's own payload rather than from
  > `m_workingCity`, which has already moved by then.
  > ⚠ **The consumer's play-time SKIP was the tell.** It ignored the membership fact "because the choke point
  > already applied it", which is what a second maintenance surface always looks like from the store's side.
  > A wrong wiring like this is removed ON SIGHT and an interim double-count is accepted rather than weighed
  > ([neither playability nor compiling gates removing legacy](specs/validation.md#playability-not-a-gate)).

  A MEMBER plot's bits moving reaches the counts through the **PLOT's own announcement**
  (`SEVT_PLOT_PREDICATE_ADDED / _REMOVED`, carrying the `CASC_PRED_*` id): when a member plot's verdict bit moves,
  the PLOT says so and the dictionary applies `add(bit, ±1)`.
  > **⛔ THE PLOT SENDS IT UP THE CHAIN; THE CITY NEVER REACHES DOWN FOR IT (owner).** A city-side maintainer
  > that "unfolds the old bits and refolds the new ones" cannot work and must not be built: by the time any
  > consumer runs, the plot's bitset already holds the NEW value, so the old bits are gone and recovering them
  > means re-deriving the block -- the legacy read path rescheduled from read-time to event-time, which this
  > document bans two sections up. Let the object care about itself
  > ([tally.md](specs/tally.md)) and the dictionary consume the fact
  > ([a context dictionary is a spine consumer](#what-a-context-stores-vs-forwards---a-context-is-an-event-built-store-not-a-forwarding-facade-owner)).
  > ⚠ **THE FAILURE IF IT IS MISSING IS NOT A STALE GATE -- IT IS A COMPOUNDING MAGNITUDE.** `plotAttrs` is
  > plane B's COUNT (§ THE MAINTAINED SUM, above), so a bit that is
  > never withdrawn leaves every deposit scaled on it (`+1 food per flatland plot`) inflated permanently, and
  > inflated further on every subsequent substrate change.
  > ⚑ The MEMBERSHIP case is different and needs no announcement of its own: a plot joining or leaving folds
  > that plot's CURRENT bits, which are readable where they are.
- **`CityContext`'s other blocks** ← each maintained by the fact that names what moved, routed through the same
  consumer. ⚠ These are on the same re-derive-whole shape the callout above retires, and they convert the same
  way — the target is the fact SETTING what it names, never a re-run of the block's whole derivation because
  something in its vicinity happened:
  - the **VICINITY store** ← the radius tiles' bonus / owner / worked facts, each applying `±1` through the ONE
    write point.
    > **⚖ THE CITY DEFINES ITS OWN POTENTIAL WORK AREA, AND THAT IS UNAVOIDABLE (owner) — because the cross
    > GROWS** (culture level, `adds3rdRing` — the two sources `CvCity::hasThirdRing` owns), so no fixed geometry
    > can answer it.
    > The city hands that definition to the plots as `CvPlot`'s **`workableBy`** membership, announced per plot as
    > `SEVT_PLOT_WORKABLE_BY_ADDED / _REMOVED`; the fold then reads the plot's own list and is EXACT.
    > ⛔ **There is no radius inverse and no membership test** — a store keyed on the radius folds a DELTA, and a
    > radius GROWING is an ordinary fact rather than something a walk must rediscover.
    > ⚑ **THE ADDRESSING IS WHAT MAKES IT CHEAP, and it is already defined (owner): the city-plot table is
    > RING-ORDERED** — index 0 the city, 1–8 ring 1, 9–20 ring 2, 21–36 ring 3 — so a radius IS a prefix of it and
    > a level change is exactly the index range `[oldCount, newCount)`. Nothing geometric is rebuilt;
    > `CvCity::changeWorkableArea` walks that range and is the whole maintenance surface, reached from three sites
    > (the city starting to exist, ceasing to, and `setCultureLevelInternal`).
    > ⚑ **The same route SEEDS the store**, so there is no separate build pass: a city establishing its work area
    > announces one membership fact per plot, at birth and again at `GAME_LOAD_FINISHED` — where the map streamed
    > before the players, so nothing could have announced to a city that did not yet exist.
    > ⚠ It is DERIVED: zeroed at `CvPlot::reset` and never serialized, since a recycled plot would otherwise name
    > a city from the previous world ([derived data is never trusted from a save](specs/save.md#5-derived-data-serializes-nothing-)).
    > ⚠ `m_iCityRadiusCount` / `m_aiPlayerCityRadiusCount` keep their own readers and stay — what this replaces is
    > the vicinity fold's need to re-derive membership, never those counters.
  - the **AREA facts** ← the plot-TYPE fact near the city, the per-area **`SEVT_AREA_TILE_ADDED / _REMOVED`**
    (one area's tile count moved — only the cities IN that area re-read), and the wholesale
    **areas-recalculated** fact below.
    > ⛔ **The per-area route DECLINES while `CvMap::recalculateAreas` is mid-pass** (`isRecalculatingAreas`).
    > That pass clears every plot's area and reassigns every id, firing the per-area fact once per plot, so a
    > per-tile refresh inside it would be O(plots × cities) of work against a map that does not exist yet — and
    > the wholesale fact closes the bracket by refreshing every city once, which is the answer for that window.
    > ⚑ The EMIT is untouched: the fact fires and the CONSUMER declines it
    > ([spine.md](spine.md) — never suppress an emit to fix a consumer).
  - the **holy-city and HEADQUARTERS counts** ← their own facts, applied `±1`.
    > **⚖ THE DESIGNATION LIVES ON `CvGame`, AND THE CITY HOLDS ONLY HOW MANY NAME IT.** The authoritative
    > assignment is `CvGame`'s, keyed by religion / corporation — exactly one city each, so uniqueness is
    > STRUCTURAL there and a per-city bit could never guarantee it. What the city needs is the bare verdict,
    > and that is a count.
    > ⛔ **The bare verdict is NOT asked of `CvGame` per entry.** `CvCity::isHolyCity()` / `isHeadquarters()`
    > used to walk the WHOLE religion / corporation registry asking `getHolyCity(r) == this` once per entry —
    > on AI paths, and forwarded to by `CityContext::isHeadquartersAny()`, so a context whose premise is O(1)
    > bare fetches was forwarding to a registry scan. That is the forwarded-read-that-COMPUTES defect this
    > document names, and the test settles it: **ask what the read WALKS** — one pointer forwards, a registry
    > scan earns a store.
    > ⛔ **And it is a DELTA store, not a refreshed one.** The holy-city count was previously maintained by a
    > `refreshHolyCity()` the fact CALLED — the legacy read path rescheduled from read-time to event-time, not
    > deleted. The fact now applies `±1` and nothing re-derives. ⚠ Consequently there is **no build pass** for
    > either count at city-founded or load-finish: the facts already carry them (`CvCity::read` announces every
    > designation the city holds), and a rebuild beside a delta store doubles it.
- **`EmpireContext.policies`** ← the **civic / trait / player-init DOMAIN facts**, consumed by the policy store
  itself, which applies each grantor's policy block as a DELTA — never a refill, which would recount every time
  and so hide the multi-grantor case the count exists for. It is the single
  source the one policy read (`ev_playerHasPolicy`) uses — reads never re-walk the grantors. The **player-init** fact
  is load-bearing on its own: a player's INITIAL traits are written straight into the has-array rather than through
  the trait setter, so that fact is the only announcement they ever make.
  ⛔ It is deliberately **not** maintained from `CvPlayer::setCivics` / `setHasTrait`: a direct hook beside an event
  is a second maintenance surface for one fact, and the fact already exists.
  ⚑ **This store is the ONE home of the per-flag policy verdicts** — the boolean getters (`isNoForeignTrade`,
  `isStateReligion`, `isInquisitionConditions` and their kin) read `has`, and the AI civic-value what-ifs read
  `count` with an unconditional vacuum subtraction of the option slot's own civic. The serialized per-flag
  counters `processCivics`/`processTrait` once pushed beside it were the second-surface class and are cut
  ([the uniform legacy-accumulator cut](#-the-legacy-accumulator-cut--every-accumulator-one-uniform-mechanism)); their changers' side-effect riders
  live at the adoption site (`setCivics`), diff-gated on the policy actually moving. Genuine non-cascade state
  on that same class (the revolution index, `changeMaxConscript`, `changeSpecialistValidCount`, hurry counts)
  stays serialized as-is.
- **The HELD-TRAIT set** ← the **same trait / player-init facts**, consumed by its own store beside the policy one
  (one dictionary per area of responsibility — the `TRAIT_` and `POLICY_` key spaces are disjoint registries).
  > **⚖ IT IS THE CASE WHERE ONE AXIS SPLITS ACROSS THE SCAN-vs-HOP TEST, and reading the FORWARD row as settling
  > it is the mistake to avoid.** *Does this player hold `TRAIT_X`* resolves through one pointer, so trait
  > PRESENCE is correctly a forward and earns no store. *Which traits does this player hold* is a different
  > question with the same subject: off `m_pabHasTrait` it walks all 369 trait records to rediscover the handful a
  > leader carries — the O(registry) sweep this document names on the unit plane (§ THE READ PATH, below: *"the
  > sum walks what the unit HOLDS, never the registry"*). ⇒ **Ask what the READ walks, never what the
  > SUBJECT is:** the same axis can forward one question and store another.
  > ⚑ **Its reader is the keyed-deposit walk** (§5): a trait's target-keyed
  > deposits stay SOURCE-side (the §4 per-set carve-out), so the read asks each LIVE SOURCE what it deposits onto
  > that key. That read is cheap *"because it iterates the handful an entity AUTHORED"* — which holds only if
  > discovering the live sources is itself cheap.
  > ⛔ **The sign comes from the fact's IDENTITY, never from a re-read.** `setHasTraitInternal` writes the
  > has-array BEFORE it emits, so a handler asking `hasTrait` would read the NEW value on both ends and never
  > withdraw — the same reason a city can never reach down for a plot's old bits.
- **AREAS are announced WHOLESALE.** `CvMap::recalculateAreas` clears every plot's area, empties the area list and
  recalculates, so it emits **`SEVT_AREAS_RECALCULATED`** (no payload — the fact IS "all of them") and every holder
  of an area id re-reads. Areas are virtually never recalculated (terrain levelled to sea level — the WMD mechanic —
  plus map generation), so the blanket costs nothing at its real frequency, and it is **not** the banned self-heal: a
  wholesale identity reassignment is not addressable per-source, so no finer route exists to derive
  ([self-heal is not a backstop](#-a-self-heal-is-the-fossil-of-a-missing-emit--so-it-is-a-search-not-just-a-ban) bans papering over a MISSED invalidation, not announcing a
  genuine wholesale one).
- **Forwarded** fields need no maintenance — they read the live source.
- **Load** — `EmpireContext.policies` rebuilds from the **in-read civic/trait/player-init emits** as they stream (a
  derived aggregate recomputes from source on load, [derived data is never trusted from a save](specs/save.md#5-derived-data-serializes-nothing-),
  never trusted from a save) — through the consumer, not a second build mechanism beside the event stream.
  `CityContext`'s other blocks build once at `GAME_LOAD_FINISHED`, because each reads state that is only complete when
  the whole stream has ended (the areas deserialize after the plots). That single pass IS the load build — the only
  full build there is — after which the facts alone maintain them.
  ⚑ The VICINITY dictionaries, `onSite` included, are not among them: they seed through the ORDINARY membership
  route, because re-establishing each city's work area announces one `SEVT_PLOT_WORKABLE_BY_ADDED` per plot and the
  applier folds that tile's CURRENT bonus and served resource. One route, both jobs — no separate build pass to keep
  in step ([the load reseed](spine.md#5-the-load-reseed): never a second build mechanism beside the event stream).
  `CityContext.plotAttrs` builds from the in-read DOMAIN events
  ([the load reseed](spine.md#5-the-load-reseed)): each `CvPlot::read` announces its deserialized working-city
  fact (`SEVT_PLOT_WORKING_CITY_ADDED / _REMOVED` — the genuine read site emits), and `CityContext`'s own consumer
  BUFFERS the load bracket's membership facts and folds them through the one applier
  (`CvCity::onCityPlotChanged`) at `GAME_LOAD_FINISHED` — the cities stream AFTER the map, so the fold applies once
  after the stream ends (the [enabler §7.1](specs/enabler.md) order rule's second option, never the mixed form).
  ⚠ **The buffer is an ORDERING fact, not a staleness mechanism** — there is no city to fold into while the map
  streams. The per-bit facts need no such treatment and are simply dropped inside the bracket: by the drain every
  plot has announced its substrate, so the fold reads FINAL bits and applying them earlier would only count them
  twice. There is never a blanket per-turn recompute.

### Scope set — plot / city / player now; units FUTURE (role-specific); no AreaContext (owner)

Contexts exist today on **plot, city, and player**. There is deliberately **no `AreaContext`**, and the reason
generalizes: **an area is not a scope at all.** A scope must be unambiguously OWNABLE — universal (world) or
owned by exactly one player up the chain — and a landmass is shared by several empires at once, so anything on it
is a per-(landmass × player) cross-product rather than a scope
(§ THE READ PATH, below). An area is therefore a bare **id**, "a really big plot" to
reference, and an area-shaped effect authors at **empire**.

> **⛔ AND TEAM IS NOT A CONTEXT EITHER — `CvTeam` IS THE TECH BRIDGE; `CvPlayer` HOLDS THE CONTEXT (owner).**
> A team's job on this plane is to hold the shared TECH/war facts and hand them across its members. It owns no
> live-state surface, so **every team fact a reader needs is asked of the PLAYER** — team-held techs through
> `EmpireContext::teamHasTech`, and everything else forwarded the same way.
> ⇒ The team carries the unified TECH and PROJECT lists as MEMBERSHIP; anything DERIVED off them — the capability
> union, the skill planes — is the PLAYER's. A derived store landing on `CvTeam` is misplaced by construction,
> whatever maintains it.
> ⛔ **Consequence, and it is structural: `CvCascadeEvalCtx` carries NO `CvTeam*`**, and no getter, evaluator,
> predicate or valuation reaches a team to answer a state question. A player that cannot answer one is a
> **CONTEXT GAP to close by adding the forward** (§ THE EVAL CTX, above), never a reason to reach a team.
> ⚠ **Do NOT read the DEPOSIT spine as licence.** `world › team › empire › city › plot` is the containment spine
> for MAGNITUDES, and a team genuinely carries a package (§ THE READ PATH, below: three
> channels) — so "team is a scope" is true of deposits and false of state. Conflating the two is what puts a
> `CvTeam*` back in a reader's hands; the same distinction is stated on `CvTeam` itself.

⚑ **What the contexts DO carry is the area FACT** — the city's area id, its tile count and the coastal
water-body size, forwarded by `CityContext` for the `AREA_SIZE` token and the adjacency reads. *"We rather use
the area id"* (owner): the id is a fact a city reads, never a place state lives.

**Units are a deliberate FUTURE scope, held off on purpose (owner).** A unit context must be **ROLE-SPECIFIC**: the
goal is that a unit no longer carries ALL the data (the ~247-field fat-unit problem) — each unit holds only the state
its role needs. Working out that role-partitioning is *why* it waits, rather than wiring a fat unit context now.

**⚖ IDENTIFIED MEMBER — the UPGRADE resolution belongs to the UNIT CONTEXT (owner).** *"Upgrade should live in the
unit context."* **The DIRECTION is the ruling (owner): the UNIT asks.** *"When a unit asks if they can do their
upgrade in a city somewhere, then the unit has to check if a city has whatever requirement it needs."* This is
built: `CvUnit::getUpgradeCity` drives the search and fans out to `GET_PLAYER(...).cities()`, asking each
candidate's own `getUnitAvailability(eUnit)` — a city is a place the query LOOKS, never the owner of the
question.

⚑ **AND IT IS PURELY AN AI-LOOP CONCERN (owner)** — the AI deciding whether, and where, to send a unit to
upgrade. That settles its cost class: any caching this resolution earns is **AI-heuristic caching**, the
sanctioned residual ([superseded-ideas #1](architecture/superseded-ideas.md)), NOT engine state and NOT a derived cache on
the cascade plane — it would carry no staleness protocol and answer to no invalidation contract.

---

## The read — the per-GROUP valuation, and the cascade provides while the game object sums

> This is where the misunderstanding that has cost repeated rebuilds lives: agents treat the cascade as the
> thing that COMPUTES a yield and leave the game objects as passengers. It is the opposite.

An info's ACTUAL contextual output is read **one endpoint per GROUP of channels** (owner), never per single channel:
`expectedFlatYields` / `expectedYieldModifiers` / `expectedPlotYields` / `expectedFlatCommerce` / `expectedWellbeing`.
Each takes the three live contexts and fills that group's ×100 array — **you pass the contexts in, you get the group's
expected values out**: `(CityContext, EmpireContext, CvPlotGroup)` → the group's values.

- **CityContext** — vicinity + local state AND the river/water/… plot-attr COUNTS (`plotAttrs`). A building reads the
  CITY context for "how many river tiles", **never a PlotContext directly** (owner) — the plot-count sums live in the
  city context. It also answers the city's **traded** bonuses (through the city's own plot-group-backed reads).
- **EmpireContext** — the empire-scope state (civics/traits/policies/state religion).
- **CvPlotGroup** — the trade-network object; the reserved explicit **traded**-bonus source (`connection:"trade"` vs
  `"vicinity"`, [json.md §3.4](specs/json.md)). Traded state is **NEVER mirrored into `CityContext`**. The
  valuation seam fills it into the eval ctx (`CvCascadeEvalCtx::plotGroup`): a `connection:"trade"` atom reads the
  city's own plot-group-backed RELAY when a city is bound (`CityContext::tradedBonusCount` forwards to
  `CvCity::getNumBonuses` — the tech-gate/minted/corp layer over the group's count), and the passed group directly
  for the city-less what-if.

Each endpoint returns the UNCONDITIONED ×100 base PLUS every conditioned `m_cond` deposit whose condition holds — summed
via the **one** evaluator (`MMKernel::applies`) over a `CvCascadeEvalCtx` the contexts fill (`CityContext::fillEvalCtx`
= city/plot, `EmpireContext::fillEvalCtx` = player/team) — so the contexts ARE the eval state, not a raw-pointer ctx
built beside them. `expectedPlotYields` scales each plots-target deposit by `cityContext.plotAttrs.count(predicate)`.

> **Everything an info holds is ×100** ([the ×100 fixed-point model](specs/curators/fixed-point-and-scales.md#1-the-model--integer-100-for-amounts-human-only-at-the-in-and-out-boundaries)) — readJson converts
> human→×100 once at load; the info never de-scales; a reader `÷100`s at the point of use. So these endpoints add
> `value100` directly, and the materialized base members are `value100`.

> **Naming — no abbreviated parameters (owner).** Parameters are spelled in full (`cityContext`, `empireContext`,
> `plotGroup`), never `cx`/`pg`: short names are only defensible inside a tightly-scoped lambda, which C++03 lacks.
> Index parameters likewise name the enum they key (`YieldTypes eYield`, `DefenseKind eKind`), reusing the
> existing engine + family enums — a new family mints one typed enum, and the group's entries + its `expected*`
> array both key off it ([patterns.md § THE GETTER SETUP](architecture/patterns.md)).

### ⚖ THE READ PATH — THE CASCADE PROVIDES, THE GAME OBJECT SUMS (owner, LOCKED)

- **The cascade is the PACKAGE STORE, nothing more.** Per `(scope × channel × combine-position)` it holds one
  standing sum — how a yield is influenced, and by how much, from every source. It answers *what influences
  this*, and it **never computes a final number**.
- **The GAME OBJECT sums.** The consuming object fetches the packages it sits under and applies its channel's
  combine formula. That arithmetic is the object's, not the store's.
- **ONE reporting surface, read identically by both consumers** — the game object summing, and the endpoints
  decomposing — so the number a city computes and the breakdown an endpoint renders are the SAME bytes. Two
  surfaces would be two derivations, and they would drift.

  Plot and the upper scopes are therefore mirror images (yield-only vs percent-only), and **CITY is the single
  scope carrying both**. That is why "whether a scope's packages are empty is irrelevant" is not hand-waving: the
  shape is uniform, and the origin rule says which half any given scope ever fills.
  ⚖ **The rule governs the YIELD/RATE plane; for every other family the sides are the DATA's and the minted
  channel sets enforce them** (wellbeing authors empire flats; health/defense/property author plot percents)
  — §1. ⛔ Consequence for any read-side roll-up: **the channel set is the
  gate, never a hand-written per-scope filter.**

  **⛔ THE CONSOLIDATION REQUIREMENT (owner): every modifier/yield cache is ONE shape** — one flats dictionary
  per YIELD ORIGIN the scope carries plus one percents dictionary, each an int keyed by channel. Every scope but
  CITY carries a single flats dictionary, because only the city has more than one yield origin (below). The
  requirement is SAMENESS OF SHAPE, never a count of dictionaries: what is banned is a bespoke struct or a
  hand-named field, not a second dictionary of the same uniform type distinguished by its origin. The drift it replaces is the ~33
  hand-named scalar fields (`scGpBaseBld`, `scDefense`, `scDefBombard`, `scMaintModCity`, `scTradeCity`,
  `brCityMilitary`, …): a hand-named field cannot be addressed uniformly, so it forces a bespoke invalidation
  path per field, which is how that many accumulated. A new scope or channel must be DATA, not a new struct.

  **⛔ KEYS ONLY WHERE THEY ARE NEEDED (owner) — the storage is NOT a global dense index.** The channel set is
  DATA-DEFINED (`PROPERTY_*` is one channel per property info) and no object uses more than a fraction of it, so
  a dense array over every channel on every object is mostly zeros — on 9,600 plots that is ~7 MB of nothing.
  Each scope carries ONLY the channels authored AT that scope, both the channel ids and the per-scope sets
  derived from the data at load (the `ClassificationRegistry` minting precedent), never hand-listed. The
  layout is OPEN-ENDED: slot indices are append-only ints with no fixed bit budget, so the per-scope counts
  grow with the authored data — read them off the load's `[MODIFIER]` channel-census line
  (`Cascade.log`, one line per scope: authored / slots / receivers), never from a remembered figure.

  **⛔ A SCOPE MUST BE UNAMBIGUOUSLY OWNABLE — WHICH IS WHY A LANDMASS IS NOT ONE (owner).** This is the test a
  candidate scope has to pass, and it explains the whole spine at once:
  - **WORLD passes by being UNIVERSAL** — *"game scope works, because it affects everyone, always"*, so the
    question of who owns the value never arises.
  - **team / empire / city / plot pass by being OWNED BY EXACTLY ONE PLAYER** up the chain, which is what lets a
    deposit roll DOWN and a target read one combined total.
  - **A LANDMASS passes NEITHER.** *"It knows no borders"* — one landmass spans several empires at once, so an
    effect on it *"affects individual players"* and is inherently a per-(landmass × player) **CROSS-PRODUCT**
    rather than a scope. Modelling it as one forces a bespoke slot into the MIDDLE of the containment spine, and
    that bespoke slot is the TELL, not the solution.

  So **there is no area scope**: `"area"` is not a scope token, no object carries an area package, and the
  containment spine is `world › team › empire › city › plot` ([json.md §3.2](specs/json.md)). The legacy
  `iArea*` authorings were modders reading "area" as "player" — they author at **EMPIRE** — and the ONE genuine
  area concept is a PHYSICAL CONTIGUITY constraint (you cannot run power lines across an ocean), which is the
  engine-side clean-power counter and never a cascade channel.
  ⚑ **The area ID SURVIVES as a plain FACT**, and that is the whole of what an area is to the cascade: a bare id
  plus its tile count, forwarded by `CityContext` for the `AREA_SIZE` token and the coastal water-body read
  (§ THE CONTEXTS, above). ⛔ The city carries that ID, never a `CvArea*` or a per-read `area()` chase — a
  per-read `area()->getNumTiles()` dereferences a whole object to answer a counter an int already holds.
  **⚑ Areas are VIRTUALLY NEVER recalculated (owner)** — `CvMap::recalculateAreas` exists for the extreme case
  of terrain levelled to sea level (the WMD mechanic), plus map generation; a landmass does not otherwise split
  or merge in play. Treat a rebuild as RARE-but-real: it does `m_areas.removeAll()` and reassigns every id.
  **So the rebuild announces itself as a DOMAIN fact (owner): emit "areas recalculated" and force the recheck** —
  every holder of an area id re-reads, rather than each cache inventing its own staleness test. Being rare, the
  blanket costs nothing; and it is not the banned self-heal: a wholesale identity reassignment is not
  addressable per-source, so no finer route exists to derive ([self-heal is not a backstop](#-a-self-heal-is-the-fossil-of-a-missing-emit--so-it-is-a-search-not-just-a-ban)
  bans papering over a
  MISSED invalidation, not announcing a genuine wholesale one).

  **⛔ TWO SCOPES ARE DELIBERATELY NOT PACKAGES (owner):**
  - **WORLD is CONFIG** — cost multipliers and the like, carried by eras / gamespeeds / handicaps. It changes
    essentially never and is read from its sources, not cached behind a staleness protocol. A project granting
    something to every player is NOT world-scope state: it authors the plural TARGET `world.empires`
    ([json.md §3.3](specs/json.md)) and lands in each PLAYER's package. The handful of `health.world` /
    `happiness.world` / `tradeRoutes.world` project authorings are mis-scoped data, a curator fix
    ([recurate on every decision](../AGENTS.md#git--delivery)).
  - **UNIT is RESOLVED VALUES, not a package** — "when the number is put on the unit, no more percentages or
    whatever is involved, the data just IS". The exact set of numbers a unit carries is known, so they are summed
    and stored individually, and they move on a different trigger from everything else: ONLY when a promotion or
    combat class changes — plus ONE seeding gather at BIRTH (`SEVT_UNIT_CREATED` at play; the END of the unit's
    own `read()` at load, the one point its full held set has streamed in — the consumer's mark cannot serve a
    loaded unit, since its getUnit lookup runs while the player's unit list is still mid-stream), because the
    non-delta slots (vision above all) carry the unit's own BASE: a unit holding no promotion and no extra
    combat class would otherwise never gather and read 0. It is the most static plane in the engine.
    ⛔ **THE SUM WALKS WHAT THE UNIT HOLDS, NEVER THE REGISTRY.** The contributors are the unit's own type plus
    its held promotions and held combat classes, enumerated from the containers the unit already keys them in —
    not discovered by sweeping every promotion and every class asking "do I have this?". That sweep costs the
    DATABASE per gather to rediscover a handful, which is the O(registry) shape the event-built state exists to
    delete (§ THE CONTEXTS, above: a read that walks per call is the efficiency defect to reject in review)
    and the own-data inversion [reverse lookups are populated once, at load](#1-one-step-deposit-down-accumulate-read-o1) bans one plane over.
    The unit's storage is therefore NOT a
    bespoke struct awaiting consolidation — it is correctly its own shape, and the 12 unit-only families
    (`strength`, `movement`, `withdrawal`, `firstStrike`, `capture`, `collateral`, `heal`, `bombard`, `air`,
    `cargo`, `range`, `pillage`, …) never enter a scope's channel set.
    ⚖ **STRENGTH'S BASE IS PER-UNIT STATE AND IS DELIBERATELY SERIALIZED (owner ruling).** Every other resolved
    slot takes the unit's own TYPE from the gather, because it is a pure function of that type. Base strength is
    not: **WorldBuilder edits an individual unit's strength**, and the WBS scenario format persists the result
    (`CombatStr=`, written only when it differs from the type). *"You want people to be able to do things in
    WorldBuilder."* So the base lives on `CvUnit` as the serialized `m_iBaseCombat`, the resolved plane carries
    the promotion / unit-combat **DELTA ONLY**, and the consumer adds the two. ⛔ This is the ONE carve-out in an
    otherwise uniform gather, and it is load-bearing: letting the type contribute to the strength slot as well
    silently DOUBLE-COUNTS every unit's authored base. ⚠ It is therefore NOT a
    [derived data is never trusted from a save](specs/save.md#5-derived-data-serializes-nothing-) violation — the value is genuine
    per-unit state that no amount of re-derivation can reconstruct, which is exactly why it is stored.
    ⛔ **AND A SECOND ONE IS NOT ADMITTED: AN INVISIBLE ADDITION TO A BASE STAT IS BAD DESIGN WHICHEVER WAY
    YOU PUT IT (owner).** A per-unit stat change is expressed as a CARRIER — a promotion or a status — *"so you
    actually see what is going on with the unit and why"*, which is also why the gather walks the carriers it
    does: each is visible on the unit. ⇒ A mechanic that would force a second carve-out is the MECHANIC that
    goes — an event handing one unit a stat outright is source-less one-shot state, and *"I would be inclined
    to nuke such an event."* ⚠ It has no claim anyway: its only delivery is a promotion
    ([state.md](specs/state.md)), which is already what serializes.
    ⚖ **A NEW SPECIAL CASE SHIPS WITH THE MEANS TO SHOW IT, OR IT IS NOT ADDED (owner): *"if we want to
    support special casing, we also need to support the ability to show it — so if we want to add that in the
    WorldBuilder, then we need to create tooling for it."***
    ⛔ **STRENGTH ITSELF STAYS FOR NOW AND IS NOT TO BE TOUCHED — it works (owner)**: illegible in exactly the
    way this dislikes, and knowingly kept, so an agent "fixing" it is undoing a decision rather than closing a
    gap. ⚖ It is NOT permanent — **when a real pass at WorldBuilder special-case additions is taken, strength
    folds into it** (owner): sequencing with a named destination, so
    ["deferred" is banned](../AGENTS.md#design) does not reach it. ⛔ Do not start that fold early or take
    it opportunistically.
    ⚖ **A SECTION FOLDS BESIDE THE SLOT TABLE, ON THE SAME MARK — it does not become a slot, and it does not
    become a hand-named pair either.** The slot table addresses modifier-FAMILY entries by
    `(family, kind, scope, unit)`; a `hideAndSeek`-shaped SECTION ([json.md §9](specs/json.md)) has no such
    address, so it cannot ride the table — and a scalar pair beside it would be the shape
    [every derived cache is one shape](#-every-derived-store-is-one-shape--a-keyed-accumulator-maintained-by-a-delta-owner) calls a defect. It gets its own cached block
    on the SAME two facts, so ONE route maintains one unit's whole resolved state.
    ⚑ **What earns a section that block is the CARRIER SET, and it is the test to apply to the next one:** it
    folds over exactly the unit's own info ∪ held promotions ∪ held unit-combat classes — the same three the slot
    table folds over — so the two facts that move the table are precisely the two that move it, with no third
    trigger to find. ⛔ A per-read fold over those carriers is the O(registry) walk this plane exists to delete,
    and converting it to walk the HELD containers instead is NOT the fix — that is the same walk with a better
    receiver (§ THE CONTEXTS, above). The read becomes a bare fetch or nothing has been done.
  ⚠ Hand-maintained duplicates DRIFT — that is not theoretical: the maintenance decomposition and its cached fill
  duplicated five terms, and the L8 home/otherArea overlay landed in one and not the other, so `/computed`
  under-reported by 39 against the served value until the duplicate was replaced by a delegation.
  Full rebuild of everything = LOAD ONLY.
  **⛔ THE FIX IS NEVER "ADD ANOTHER STRUCT" — that is the failure mode this ruling exists to close.** The previous
  substrate grew ONE BESPOKE STRUCT PER SCOPE, each with hand-named per-channel members instead of channel-indexed
  Σflat/Σpercent; it is archived and must not be reconstructed ([superseded-ideas](architecture/superseded-ideas.md) #14).
  **A missing scope is a SYMPTOM of that, not the disease:** with one uniform package, giving a scope its packages
  is a single member; with bespoke structs every scope is its own project — which is exactly why a small scope
  (team, at three channels) never got one, and why its sums leaked into whichever neighbour already had
  a struct. So the package TYPE is unified FIRST (one owner-templated, channel-indexed package on
  `CvDerivedCacheSet<TOwner>`), after which every scope falls out of the same member. Adding a further per-scope
  struct deepens the divergence this closes.

### ⚖ THE CAPSTONE RULE: the cascade is built and kept current ENTIRELY from events — no blanket rebuild, ever, and no per-slot rebuild either

On LOAD the cascade is stood up by the **event reseed** — the save read fires
the DOMAIN events for every fact as it deserializes and each fact applies its source's deposits
([spine.md](spine.md) / [the load reseed](spine.md#5-the-load-reseed)); the old
recompute-on-load / warm-up recalc (`playerSliceRebuild` + `worldRebuild`) was a stabilize-the-drift STOPGAP
and is REMOVED. Post-load, a fact reaches exactly the slots its deposits feed and **nothing else runs at all** —
no full per-player rebuild on `doTurn`, no mark-all, no per-slice blanket, no turn-roll self-heal
([self-heal is not a backstop](#-a-self-heal-is-the-fossil-of-a-missing-emit--so-it-is-a-search-not-just-a-ban)). A missed emit surfaces as a live divergence, never a
silently self-healed cost — which is precisely why the event spine must be COMPLETE (every mutation emits) and
is built proper and FIRST.
⚑ **Under the maintained sum that sentence hardens from a design preference into a PRECONDITION:** an
unsaturated spine does not merely leave a value stale, it leaves the sum wrong with nothing that could ever
correct it ([the maintained sum](#-the-maintained-sum--three-planes-one-slot-and-nothing-is-ever-recomputed)).
Reads are BARE NUMBER FETCHES during the turn (an ensure-per-read protocol on AI-hot paths measurably ground
unit automation). ⚑ *"It's the percentage recalcs that hurt"* is answered at the root rather than mitigated:
the compiled deposit carries its channel AND its unit, so a flat fact touches a flat slot and no percent stack
is ever walked — there is no mask to split, because there is no recalc to narrow.

### ⚖ THE PER-SCOPE PACKAGE MODEL — the cascade's founding design (§1), stated as cache architecture

A package lives ON EVERY SCOPED ITEM, every level (world → team → player
→ city → plot); the cascade loads **yield packages in ONE UNIFORM FORMAT** (Σflat and Σpercent each their OWN
package per channel; the unit is part of the slot key) into each scope's cache; each package is maintained
from events at its OWN scope (a world-scope fact moves the world package while every other level stands). **The
only live calculation is adding the ~5 packages together at read.**

⛔ **AND THE ORIGIN RULE'S EXTENDED FORM (owner) — CITY ITSELF SPLITS INTO THREE PACKAGES, NOT ONE:** *"a city's
yields are based on 3 packages, the yields from plots, the yields from specialists, and yields from buildings,
that is 3 separate packages."* The plot origin is the per-plot package summed over the city's worked plots; the
SPECIALIST and BUILDING origins are two distinct flat packages ON THE CITY.
⚑ **This is forced, not stylistic.** §2a puts specialists in TIER 1
(inside the percent stack) and buildings in TIER 2 (added after it), and § THE MAINTAINED SUM bans a
per-source decomposition — so once both origins land in ONE Σ slot the two tiers can never be separated
again. A single city flat package therefore cannot express the rate at all.
⛔ **The failure it produces, which is the reason this is spelled out:** the specialist half gets recovered
by a hand-walk over the city's specialists at read time, and the SAME authored deposit is then counted twice —
once inside the stack and once outside it. That walk is also the O(what EXISTS) read-time shape this whole
document deletes; a maintained specialist package needs no walk at all.
⛔ **ENFORCED BY TYPE, NEVER BY CONVENTION** ([the hard-typing-or-rollerskate rule](../AGENTS.md#design)):
the two planes are DIFFERENT TYPES (the package template carries its ORIGIN), so a specialist deposit reaching
the building plane does not compile. A comment saying which package a source belongs in is exactly what has
been re-corrected repeatedly and does not hold.

MODIFIERS still come from everything BUT plot — city, empire, team, world. So the percent side exists at
every scope except plot, and it is ONE dictionary: percents combine into a single additive stack
(§2a), so they have no origin to keep apart.

**⛔ THE GENERAL FORM — PACKAGE IDENTITY IS `(scope × COMBINE POSITION × channel)`.** Origin is the yield
plane's instance of a wider law: within a scope, packages stay isolated **per combine position** and never
merge into one per-scope number. The city's yield positions are the two-then-three origins above; every channel
family defines its own positions (wellbeing's opposing channels, the scalar stacks), and a scope's packages
follow that channel's positions. ⛔ A per-scope blob is the defect — whatever is summed together can never be
told apart again, and the combine is what needs them apart.

**⛔ THE FOUR-PROVIDER LAW — only PLOTS, SPECIALISTS, BUILDINGS and TRADE ROUTES provide yield.** They are
what physically produces yield in game. Every other source kind — trait, civic, tech, religion, corporation —
only MODIFIES or CONDITIONS a provider's output, so **every yield deposit resolves onto a PROVIDER-KIND
package**: a trait's specialist boost lands on the SPECIALIST package, a civic's building-keyed percent on the
BUILDING percent stack. ⚑ This is what decides which package a deposit joins, and therefore which leg the
percent stack multiplies — the question the origin split exists to answer.
**⚖ FOUR PROVIDERS, THREE PACKAGES — the TRADE ROUTE is a provider that is NOT a package (owner): *"trade
route yields are always provided by the ENGINE; the trade route buffs happen BEFORE it arrives, as its
complete package."*** The engine owns the network calculation and applies the route's own buffs, so the
cascade receives a FINISHED value and folds it at the combine — it is the one live yield INPUT, never derived
(§2a). ⛔ **So no trade-route package exists and none is to be built.**
Nothing deposits into it: a package with no depositors is an empty slot inviting a future deposit to be routed
somewhere the engine already answered, which would double the route's yield.
⚑ **WHAT THE CASCADE OWNS IS THE COUNT, AND ONLY THE COUNT (owner): *"we only tell the engine how many trade
routes we can have."*** The `tradeRoutes` channel — how many routes a city may run — is cascade-computed like
any other modifier-influenced value; the YIELD those routes then produce is entirely the engine's.
⛔ **Do not conflate them** (§2a states this at length and is the
authority). ⚠ The trap is one-directional and worth naming: listing trade routes among the PROVIDERS reads as
licence to give them a package, because the other three have one. They are a provider of yield and a consumer
of a cascade COUNT — never a home for deposits.
⚖ **The golden-age and free-city TRAIT FLATS need no provider home (owner):** they are plain flat bonuses
riding the flat yield packages outside the provider chain, joining BASE at the combine. Golden age is a core
engine mechanic and stays simple. ⚠ "free-city" is the trait yield accumulator, NOT the WLTKD celebration.

**⚖ THE CITY-REALIZATION LAW — a deposit whose CONDITION references the CITY is a city-realized join,
whatever its authored scope.** State-religion-in-city, a city building's presence, any city predicate:
evaluating such a deposit once at PLAYER scope resolves it against one city's context and mis-serves every
other city. So all conditioned percent stacks realize PER CITY, in the city's package, against that city's
own context; the player scope holds only the genuinely city-AGNOSTIC sums. ⚠ Measured, not theoretical: the
player-scope evaluation left persistent +18..+27 percent errors on every non-capital city.

- **⚖ THE KEY IS SAMENESS (owner ruling): every store is the SAME OBJECT TYPE everywhere, and they ALL MAINTAIN
  the SAME WAY.** That — not the per-scope layout — is the requirement the whole model rests on. One templated
  channel-indexed slot table on every owner, and ONE application path driving all of it, derived from the deposit
  index. What varies between scopes is only WHICH SLOTS carry a value; the type and the protocol never vary.
  - **A RECEIVER IS NOT A STORED SLOT AT ALL WHERE IT SUMS MEMBERS (owner).** A scope that consumes a channel
    from BELOW it — the empire's research / gold / culture / espionage over its cities — answers by summing its
    members' realized values at the read (§ A CROSS-SCOPE RECEIVER TOTAL, above). There is no "receiver mechanism" to
    build, and that is because there is no receiver STORE, not because one is shared.
    ⚠ Do not read the city's own realized rate as an instance of this: a city consuming production is combining
    ITS OWN packages, not summing members, so it is an ordinary package read and no member count enters it.
  - **⛔ THIS IS WHY HAND-NAMED SCALAR FIELDS ARE THE DEFECT, not just untidy.** A named field cannot be addressed
    uniformly, so it forces its own bespoke maintenance path — which is precisely how 33 of them accumulated.
    Channel-indexed slots are reached by the deposit's own compiled address, with no per-field code.
  - **The receiving scope is NOT the storing scope.** A package never moves to its consumer (that breaks the scope
    principle); the consumer SUMS its members at the read and stores nothing.
  - **⛔ A CROSS-SCOPE receiver total is the Σ of its MEMBERS' REALIZED values — and NOTHING beside that Σ.** The
    empire's gold / research / culture / espionage sums are Σ over the player's cities of each city's realized
    rate of that channel, re-summed at the read. The
    per-city quantity for a commerce channel is the whole §2a split — the
    slider share of the city's COMMERCE yield, the channel's own deposits, and the process conversion — not the
    channel's deposits alone. ⛔ **An upper scope's own package is NEVER added on top of that Σ:** its deposits
    roll DOWN (§1) and are therefore already inside every member's realized
    value, so adding them again at the receiving scope counts each empire-scope deposit once per city PLUS once
    more — a silent multiplication that compiles, runs, and simply reports wrong numbers.
  - **⛔ NOT a push accumulator, and NOT a per-CANDIDATE ask** (§ A CROSS-SCOPE RECEIVER TOTAL, above: the cost is
    the member count, the cadence is the only defect). Rejecting the legacy incremental accumulator does not
    license an AI loop asking the Σ per candidate in a scoring pass: that caller hoists it once per pass into its
    own scratch (the sanctioned AI-heuristic residual), never a stored slot on the machine.
  - **⚖ THE CROSS-SCOPE RECEIVER — SUPPRESSION IS SETTLED; ONLY THE DELTA QUESTION IS OPEN.** A receiver total is
    the Σ of its members' **REALIZED** values (§ A CROSS-SCOPE receiver total, above), and a realized value is the §2a
    combine over the member's packages, not a stored deposit sum. Two of its apparent obstacles dissolve:
    - **⛔ DISORDER AND WLTKD ARE NOT TERMS IN THE COMBINE — they are a PARTICIPATION GATE ON THE Σ (owner):**
      *"disorder is easy, it just means that the packages that the city under disorder is simply not sent."* The
      city's stored value stays the real one and the sum declines to take it
      ([economy.md](reference/economy.md): *"the package is sent out to the rest of the cascade only if no
      status negates it"*).
    - **⛔ THE GATE BELONGS AT THE Σ, NOT ON A MAINTAINED MEMBERSHIP DELTA — and the reason is already ruled.**
      WLTKD is a ONE-TURN status re-applied every turn by its trigger ([state.md](specs/state.md)), so
      maintaining participation as a delta would flip a member in and out of the Σ every single turn *over a
      number that never moved* — precisely the thrash [economy.md](reference/economy.md) refuses to mark on
      (*"it suppresses the CONSUMPTION of the value, never its contents — so neither is a cache input and neither
      marks it"*). The filter therefore runs where the participation question is actually asked: at the sum.
    **⚖ THE RECEIVER RE-SUMS ITS PARTICIPATING MEMBERS, AND NOTHING IS BUILT TO AVOID THAT (owner).** *"The
    summing is so trivial that it would cost more to try some efficiency shenanigans."* The read side is ~5 int
    adds for the cross-scope roll-up and one combine per participating member for a receiver Σ — against the
    per-source walk the maintained sum deletes, that is not a cost to design around.
    ⛔ **So do NOT build a per-source decomposition plane.** A `(scope × channel × SOURCE)` breakdown exists only
    to make WITHDRAWAL cheap, and withdrawal is only expensive if summing is. Its source axis dwarfs the channel
    axis that KEYS ONLY WHERE NEEDED (above) already rejected as mostly-zeros, and the shape is the
    add-another-struct failure [every derived cache is one shape](#-every-derived-store-is-one-shape--a-keyed-accumulator-maintained-by-a-delta-owner) names. The
    unconditioned plane re-applies its compiled constant; the conditioned tail re-resolves; nothing stores a
    per-source breakdown.
    ⛔ **And do NOT push the realized delta upward** — that is the shenanigan the triviality makes pointless, and
    it is a push up the chain. The third shape is barred outright: a member EMITTING *"my realized value
    changed"* ([spine.md](spine.md): *"yield is a computed RESULT, never an event"*).
    > **⚑ THIS IS NOT A DEFERRAL, and reading it as one is the misreading to prevent (owner): *"IF it shows that
    > the summing requires any kind of serious power, we deal with it then."*** ["deferred" is banned](../AGENTS.md#design)
    > bans parking work KNOWN to be needed; this declines to build machinery for a cost nobody has demonstrated
    > exists — which is what the #430 roadmap already required (*"build the
    > base first; the most efficient way comes AFTER … do not build, investigate, or pre-shape it ahead of the
    > base"*) and what the triggers spec requires of hypothetical machinery. You cannot defer
    > work whose necessity is unestablished.
    > ⚑ **The REVISIT TRIGGER is named and it is a MEASUREMENT, never an argument:** a turn-time cost on the
    > standing late-game save, attributed to the summing, on the wall clock
    > ([turn time is king](#-the-per-scope-package-model--the-cascades-founding-design-1-stated-as-cache-architecture)). ⛔ Until that exists, a proposal to optimize
    > the sum is speculative structure — and an AI loop asking a receiver Σ per candidate is answered by the
    > CALLER caching its own scores ([patterns.md](architecture/patterns.md), the sanctioned heuristic residual), never by
    > reshaping the machine.
  - **Which scope receives a channel is spec'd, not chosen per site:** one consuming scope per channel
    (food/production → city; gold/research/espionage/**maintenance** → empire), with **culture the lone
    dual-consumer** (the city sums it for plot culture + border expansion, the empire for civ culture + traits —
    two independent sums over the same packages).
    ⚑ **MAINTENANCE is the one NON-commerce receiver, and it is what makes the rule general rather than a
    commerce habit.** The empire's total maintenance is the Σ over its cities of each city's realized
    maintenance — precisely the cross-scope receiver shape above — re-summed at the read like its commerce
    siblings, never a hand-named cache beside the packages
    ([every derived cache is one shape](#-every-derived-store-is-one-shape--a-keyed-accumulator-maintained-by-a-delta-owner)).
    ⚠ Its per-city quantity is the one a package cannot answer alone: a city's realized maintenance composes the
    three component KINDS (distance / numCities / colony) each against its own modifiers, takes the `amount`
    stack over the total, and declines wholesale under WLTKD/disorder ([economy.md](reference/economy.md)).
    ⛔ **`MAINTENANCE_CORPORATION` is NOT one of them** — corporate maintenance is its own pre-inflation expense
    beside total maintenance, so the city total SKIPS that kind. Its deposit is a city-scope FLAT and therefore
    sits in the city's package like any other: a read that folded every maintenance kind would charge the same
    corporate gold twice in one expense total, plausibly and silently. So the Σ asks the CITY for its realized value —
    which is what "the Σ of its members' REALIZED values" already says.
    ⚠ **A receiver read is therefore NOT interchangeable with a rolled-legs read on the same channel.** The
    cross-scope roll-up answers a receiver channel with its maintained SUM, so a consumer that wants the
    channel's percent STACK at that scope must read the legs directly — asking the roll-up would hand back the
    realized total instead, silently and plausibly.
- **⚖ THE TWO DERIVED PLANES SHARE ONE MECHANISM — what differs is their CONTENT, never their maintenance:**
  - **The yield + percent packages** are maintained by applying the moved source's deposits to the slot they
    feed — § THE MAINTAINED SUM. ⛔ The earlier framing called this "an INPUT/OUTPUT value cache: memoize,
    mark-invalidate on a source event, recompute from inputs" and pointed it at `CvDerivedCache`. That framing
    is RETIRED, and it is what let the modifier alone keep a staleness protocol while both of its siblings ran
    without one ([superseded-ideas](architecture/superseded-ideas.md) #30).
  - **The ENABLER's sets (the frontier + the operating-building set)** are maintained by **TARGETED
    PROPAGATION**: each HAVE-change ripples through the **affected subset only** (re-check the affected
    candidates / ripple the fixpoint), updating the authoritative dataset **in place** via the reverse-index
    ([enabler.md](specs/enabler.md) §7). NEVER blanket-recomputed, NEVER a parallel shadow-delta.
  - ⚑ **So the honest difference is what a slot HOLDS** — refcounted set MEMBERSHIP versus a summed MAGNITUDE —
    and each is maintained in place by the fact that moved it. ⚠ That is why the enabler was able to run without
    a staleness protocol from the start, and it is the model the packages now match rather than the exception they
    were measured against.
  ⛔ Blanket-recomputing the whole operating-building fixpoint for every city on every event runs the enabler's set AS an
  input/output cache — **"burning down the library of Alexandria" (DESPAIR_INDEX #2)**. The fix is targeted
  propagation, the shape the frontier ALREADY uses (`onBuildingChanged` / `recheckHave` off the reverse-index). It
  is likewise **not a given** the yield-package shape fits any OTHER non-package channel (the unit plane,
  properties); each is decided per-channel, only AFTER the spec is fully in place.
- **⛔ THERE IS NO BATCHED TURN-END REBUILD PASS, AND NONE IS TO BE BUILT.** A "flags all turn, one unified
  rebuild at turn end, in dependency order" phase is the recompute model wearing a better cadence — it presumes
  a rebuild exists to schedule. Under the maintained sum there is nothing to batch: the slot was already correct
  when the fact arrived ([superseded-ideas](architecture/superseded-ideas.md) #30).
- **THE APPLICATION IS DERIVED FROM THE DATA, never hand-wired.** A DOMAIN event carries its SOURCE; the source's
  compiled deposits (the load-time strings→ints index, `Data/CvDepositIndex.{h,cpp}` — per-deposit interned
  segments + FK-resolved target id + the resolved channel/scope slot, compiled at readJson push-time) name exactly
  the channels × scopes × targets it feeds — **so what to apply, and where, falls out of the deposit addresses.**
  The routing is a pure function of the index; a hand-coded hook per event site is a per-site bespoke path of
  exactly the kind [every derived cache is one shape](#-every-derived-store-is-one-shape--a-keyed-accumulator-maintained-by-a-delta-owner) forbids. Derive it from the
  index.
- **Mid-turn read freshness: the per-player-slice SNAPSHOT** — *"getting a yield event in the middle of a turn is
  not retroactive; start of next turn is what is expected"*. A newly-founded city is the one ruled exception (it
  must read correct values the turn it exists, so its packages build eagerly at creation rather than waiting for
  the next slice).
- **EAGERLY BUILD ALL CACHES AT LOAD — the general policy stands.** *"I am happy to add even MINUTES to load time
  in order to have caches eagerly built on load in general."* ALL caches are warmed at load: a game-object's own
  derived cache (the plot-yield cache) eagerly from that object's own state, and the **cascade** eagerly by the
  **event reseed** — the spine fires every present-fact, so the cache-build/invalidation consumer populates every
  cascade package and turn 1 runs warm. What changed is ONLY the cascade's population MECHANISM: the
  recompute-from-state recalc (`playerSliceRebuild` + `worldRebuild`) is REMOVED (the CAPSTONE above); the eventspine
  reseed replaces it. No design ever serializes a derived value to save load time. **The perf LAW: "the name of any
  game in this town will always be TURN TIMES — if game load takes 50% longer it matters nothing if we can shave
  5-10-15% on turn time, because there is only 1 game load, but many many many turns."** Turn time is the objective
  EVERY perf decision optimizes; load time is the currency that pays for it. Ledgered as
  [turn time is king](#-the-per-scope-package-model--the-cascades-founding-design-1-stated-as-cache-architecture).

This is the Clean-Architecture north-star applied to engine state: the repository **is** the contract, and it is the
lever for thinning the `Cv*` god-classes without touching the closed-EXE-bound `CvPlot`/`CvCity` layout. See
[north-star](architecture/north-star.md).

---

## 3. Conditioning — re-applied when its own dependency moves (the dormancy model)

A deposit may carry `enabled` / `disabled` / `per` ([json](specs/json.md) §3.7, §3.9). A deposit's condition uses the
**same vocabulary** as the enabler's `requires` — the same `all`/`any`/`noneOf` tree over the same atoms and
predicates — so a conditioned deposit is, in essence, **a `requires`-shaped gate with an output attached**: the
enabler resolves that shape to *availability* ("can I?"), the modifier resolves the *same* shape to a *magnitude*
("how much?").

> **⛔ A condition is a PREDICATE, never a bespoke sub-scope MEMBER ([conditions are predicates, never bespoke members](specs/json.md#35-predicates--a-systems-runtime-state-query)).**
> A deposit that applies only under some game state carries that state as a **predicate** in its `enabled`/`disabled`
> (or a `per`/`unit:` scaler, [json](specs/json.md) §3.7), at the deposit's normal scope — `{family}.empire.percent` +
> `enabled:"IS_CAPITAL"`, never a bespoke `{family}.empire.capital.percent` member. Encoding the condition as a new
> member instead *changes the core structure* — the kraken way. Full ruling, the extensible predicate registry, and
> the golden-age exception (`empire.goldenAge` — a PERMANENT engine member-mirror): [json](specs/json.md) §3.5.

**But they are SEPARATE FIELDS, not one condition** — because a thing can **require one condition yet gate its
effect (a buff *or* a nerf) on another**: a Forge `requires` connected iron to *operate*, but its +1 happiness is
`enabled` by *power*, not iron — and the magnitude can equally be negative (e.g. −production while polluted). So
the entity carries its `requires` once (whole-entity availability — the [enabler](specs/enabler.md)'s job), and each
deposit carries its **own** `enabled`/`disabled` (does *this effect* apply). Same condition language, two
independent fields.

**⛔ NOTHING IS RE-CHECKED ON A RECOMPUTE, BECAUSE THERE IS NO RECOMPUTE.** A conditioned deposit is applied
`±value` by **the ATOM's own verdict crossing**, and a `per`-scaled one by `±value × Δcount` from **the COUNT's
own fact** — the two routed planes of the maintained sum
(§ THE MAINTAINED SUM, above;
[the maintained sum](#-the-maintained-sum--three-planes-one-slot-and-nothing-is-ever-recomputed)). That re-application *is* the dormancy
model: a deposit whose `enabled` stops holding (or whose `disabled` starts) is withdrawn from the slot at that
instant — the source goes quiet without being removed.
⚑ **Both routes are reverse indices derived from the compiled deposit index** (atom → the deposits it gates,
count-key → the deposits it scales), so the cost is the deposits that atom or count actually touches — never a
walk of the scope's deposits asking each whether it cares, and never a sweep of the entity database.

> **⛔ THE TWO INDICES ARE KEYED THE SAME WAY AND ARE NOT INTERCHANGEABLE — asking the wrong one answers EMPTY,
> which is indistinguishable from "nothing is conditioned on this".** A condition atom's `type` interns into the
> **ATOM** index (`gatedByType`); a `per` scaler's token interns into the **COUNT** index (`gatedByToken`). Both
> are keyed by a plain string, so `"ERA"` is a legal key in either — and a route that reaches for the wrong one
> compiles, runs, reports nothing, and moves nothing.
> ⚑ **The tell is that a bare TOKEN can appear on both sides.** Most atoms are `INFOTYPE_NAME` ids and most
> count-keys are tokens, so the two key spaces look disjoint until a family uses a token as a THRESHOLD:
> `{type: "ERA", max: 1}` is a condition (atom index), while `per: {type: "ERA"}` would be a scaler (count index).
> ⇒ **When wiring a route, decide which QUESTION the deposits ask — "is this gate true?" or "how many?" — and
> take the matching index. Where a family is authored both ways, route BOTH.**
> ⚠ An empty list is silent by design (the route census reports nothing when the list size is zero), so a
> mis-keyed route leaves no trace at all. **Report the real list size, never a placeholder** — that count is the
> only thing that distinguishes a route with nothing to do from a route asking the wrong question.
>
> **⛔ AND A THRESHOLD IS NOT A PRESENCE CROSSING, so it cannot ride the ±1 atom route.** An `ERA`/`POPULATION`
> threshold has no held/not-held verdict for the as-if-held hypothetical to pin: when the counter moves, some
> deposits turn OFF and others turn ON in the same step. Such a gate is **RE-RESOLVED against the new state and
> moved by the DIFFERENCE** from what the slot already holds — which handles both directions in one pass and is
> idempotent if the fact is seen twice. The `±value` crossing form is only ever correct for a genuine presence
> atom.

- **`enabled` then `disabled`** — `enabled` is read first, `disabled` second; a `disabled` that holds overrides
  ([json](specs/json.md) §3.9).
- **`per`** scales the deposit by a count — local at `city`/`plot`, via the [tally](specs/tally.md) at cross-city scopes.
- Whole-entity availability (is this building active at all?) is the [enabler](specs/enabler.md)'s `requires`, not a
  per-deposit condition: a dormant entity deposits nothing, so the modifier machine never special-cases it.
- **Age-gated deposits** — legacy `CommerceChangeDoubleTimes` ("double after N YEARS") is **not** a timer/stage
  but a SECOND deposit on the same slot with `enabled:{existedFor:{min:N}}` (no post-sum multiply). ⚠ The unit is GAME
  YEARS, not turns — the age is measured against the stored build YEAR, and that is what the tooltip has
  always promised ([json.md §3.5](specs/json.md)).

  > **⚖ THE TURN BOUNDARY IS THE AGE GATE'S FACT, AND IT CARRIES EVERYTHING THE GATE NEEDS (owner).** *"Start
  > turn should be an event, like anything else, that has turn number, which should give cascade what it needs
  > to figure it out."*
  > ⚑ **This is the one condition class whose dependency is ELAPSED TIME.** No source moves, no count moves and
  > no atom crosses when a build becomes due — so there is nothing else in the engine that could announce it,
  > and the age gate is the only member of the family that needs a cadence fact at all. The turn number is the
  > whole of the input; the deposit's own stored build year supplies the rest.
  > ⇒ **It rides the PLAYER-scoped turn-started fact**, whose cities are the ones whose builds can come due, and
  > it is a RE-BOOK by value difference rather than a `±1` crossing (an age gate has no held/not-held verdict to
  > pin, exactly as a threshold has none).
  > ⛔ **It is NOT the banned per-turn blanket** ([self-heal is not a backstop](#-a-self-heal-is-the-fossil-of-a-missing-emit--so-it-is-a-search-not-just-a-ban)):
  > the worklist is exactly the deposits the `existedFor` reverse index names, and a turn on which nothing came
  > due moves nothing. It satisfies the sanctioned event-triggered recalc test
  > (§ THE SANCTIONED EXCEPTION, above) — a genuine DOMAIN fact, a NON-LOCAL consequence the fact cannot
  > name, and no finer route to derive.
  >
  > **⛔ THE APPLY PATH MUST SET THE CARRIER SLOT, OR THE GATE ANSWERS FALSE EVERYWHERE.** `existedFor` asks about
  > the DEPOSITING entity, so it reads `sourceBuilding` off the eval ctx and answers FALSE when nothing set it
  > (§ THE SOURCE SLOTS, above — deliberately, since resolving it against
  > whichever entity a walk reached last is worse than declining). Every walk that resolves a building's entries
  > therefore sets it: the plane-A city apply and the re-book routes alike. ⚠ A walk that resolves a building's
  > entries WITHOUT setting it silently answers FALSE for this whole class, so every deposit gated on
  > `existedFor` goes missing — a divergence no missed emit explains.

---

## 4. Ownership — the deliveryguy rule

> **This doc is the home of the deliveryguy ruling.**

A cross-entity modifier (X-keyed-by-Y) — does it live on X or fold onto Y? The test is **semantic sense: who
BRINGS this modifier to the table?** That deliverer **owns** it; the other entity is referenced as a
**condition** (`enabled` / `requires`), never the home. Two shapes, chosen per case by what reads cleanly:

- **own-output** — an entity's *own* produced output (a specialist's yield, an improvement's tile yield, a
  unit's strength) lives on **that entity**, with tech/civic/building as an `enabled` condition. *A civic
  boosting a Merchant's commerce → on the **specialist**, `enabled:{civic}` — NOT on the civic.*
- **governing-deliverer** — an entity that *delivers/governs* an effect on others lives on **the actor**, keyed
  by the target. *A route upgrading improvements → on the **route**, keyed by improvement.*

Plot-substrate entities (terrain / feature / improvement / route) each own their own `plot`-scope output. The
rule has **no special cases** — every cross-entity modifier lands by it.

**Conditioner axis:** a **tech** conditions on the **enabling** axis (`enabled:{tech}`, monotonic — once you
have it, you keep it); a **religion / resource** conditions on the **requiring** axis (`requires.operate`,
reversible — it can be lost).

**Data ≠ runtime.** The JSON is organised for a human (one home per relationship); `readJson` builds the links
both ways at parse so the machine reads top-down. Any "land it on the target" is a **parse transform**, never an
authored shape.

> **⛔ THE TWO TRAIT SETS ARE COMPLETELY SEPARATE — SEPARATED BY ID, NOT ONLY BY FOLDER (owner).**
> A leader's traits resolve to ONE `CvTraitInfo` table from *either* its simple set (`traits/simple/`, the
> `DefaultTraits`) *or* its complex/Thunderbrd set (`traits/complex/`, the `DefaultComplexTraits`), chosen at runtime
> by **`GAMEOPTION_LEADER_COMPLEX_TRAITS`**. The curator emits both as **two cleanly-separated, self-complete folders**
> (`traits/simple/` + `traits/complex/`); a consumer **loads the one active folder** by the live game option — this is
> NOT an entity-level option gate and NOT a mid-game swap (any WorldBuilder mid-game trait swap is a post-migration
> concern).
> **A complex trait KEEPS ITS OWN `TRAIT_COMPLEX_` IDENTITY** ([naming.md](specs/naming.md): `TRAIT_` is a simple trait,
> `TRAIT_COMPLEX_` a complex one). ⛔ It is NEVER re-keyed onto the base trait's id: that re-key is what
> manufactured the colliding-id problem — two genuinely different entities answering to one name — which then
> forced every reader to disambiguate by game option and made a wrong read silently return wrong magnitudes.
> Distinct ids remove the ambiguity by construction rather than by discipline.
> ⚖ **A COMPLEX-ONLY RUNG OF A SPLIT LINE TAKES THE PREFIX TOO (owner).** A developing line's upper rungs exist
> only in the complex set (the simple ladder tops out early), so they are not `CvInfoReplacements` variants — and
> keeping their authored id left a chain reading `TRAIT_COMPLEX_SEAFARING` → `TRAIT_COMPLEX_SEAFARING1` →
> `TRAIT_SEAFARING2`. **The LINE is the complex variant, so every rung of it is**, whether or not that particular
> rung has a simple twin. The test is the rung's LINE, never the rung's own id.
> **⚖ IT IS A TYPE RENAME, AND THE SAVELOAD MECHANISM TRANSLATES IT (owner).** A renamed Type is NOT a removed
> one: the record still exists under a new id, so resolving the old name to `-1` and letting the allow-missing
> class read drop the slot would throw away a rung the player still holds. The old id is mapped to the new one in
> `Assets/savemigration.txt` (a `TYPE::INFOTYPE_NAME` key — the `TYPE::` namespace satisfies the parser's `::`
> guard and cannot collide with a `Class::field` rename) and
> applied at the ONE stored-Type resolution point the class reads share.
> ⚠ The distinction generalizes beyond traits, and [save.md §7](specs/save.md)'s three removal classes do not cover it:
> that decision procedure asks what to do when a Type is GONE. Ask first whether it is gone or merely RENAMED —
> only the first is a removal.
> ⛔ **The re-key has ONE definition, on the STORE (`Store::trait_rekey`), applied where the inverted edges are
> handed out** — because a trait id is named from several curators, above all the TECH edge that GATES a rung
> ([enabler.md](specs/enabler.md): without it every upper rung is permanently unreachable, and silently so). A
> per-curator copy would drift and emit an id no record defines.
> ⛔ **A COMPLEX ID IS DERIVED FROM THE SIMPLE ONE, NEVER READ FROM THE AUTHORED `<ReplacementID>` (owner: "use
> the simple names as base, because that is the base of the names").** `complex_variant_id` (a module-level
> function in `Tools/Migration/store.py`, not a `Store` method) is that one derivation — the base's own stem
> under the `TRAIT_COMPLEX_` prefix — and both callers go through it: the
> replacement variant keyed at load, and the re-key of a complex-ONLY record.
> ⚑ **The authored `<ReplacementID>` is not even unique** — `TRAIT_EXCESSIVE` and `TRAIT_EXCESSIVE1` name the SAME
> replacement, so keying on it folded a whole rung into the base with nothing reporting the loss. That was
> invisible while the engine hot-swapped these in memory (the id was only ever FOLLOWED, never read); it costs a
> record the moment the sets are separated BY ID.
>
> ⛔ **A LINE MEMBER'S `PromotionLine` / `bNegativeTrait` IS SOURCE DATA THAT CAN BE WRONG, AND BOTH FAIL
> SILENTLY.** A rung tagged onto a NEIGHBOURING line leaves its own ladder with a hole; the fix is to RESTORE THE
> TAG, never to delete the rung or teach the classifier around it (the `TRAIT_TIMID1` precedent, below). ⚑ Both
> are found by comparing a record against its LINE SIBLINGS — a member whose line disagrees with its stem's
> majority, or whose negativity disagrees with its line's BASE, never the local arm (a negative line whose deeper
> rungs lost the flag can leave the untagged rungs outnumbering the tagged ones).
> **⛔ EVERY RECORD IN THE COMPLEX SET CARRIES `TRAIT_COMPLEX_`, WITH NO EXCEPTIONS (owner).** *"If it was built
> as complex, it's complex, no matter what."* The prefix STATES THE SET — it is not a marker for "is a variant of
> a simple trait" — so a complex-ONLY line with no simple counterpart is `TRAIT_COMPLEX_` like every other record
> in the folder.
> ⛔ **THE TWO SETS ARE COMPLETELY SELF-SUFFICIENT, IN EVERY WAY (owner) — they share NO id.** A simple trait
> with no complex variant is still copied into `complex/`, but under its OWN `TRAIT_COMPLEX_` id: the copy is
> identical in content and distinct in identity. `TRAIT_BARBARIAN` was the last shared id and is one no longer.
> ⚑ **The reason is empirical, not aesthetic: *"it is impossible for agents to actually not conflate the 2."***
> A shared id is the one thread that keeps the sets tied together, and every reader who meets it has to
> reconstruct which set is meant. Distinct ids make the conflation UNSAYABLE rather than forbidden.
> ⚑ **AND THIS IS WHY THE SPLIT WORKS AT ALL (owner): a trait is purely a collection of BUFFS — it unlocks no
> promotion, building or unit.** Nothing's availability hangs off a particular trait id, so duplicating the id
> space across two sets breaks no edge. ⚠ The dependency runs the other way and is real: a TECH names trait ids
> to gate a developing rung, so those edges must name the ACTIVE set's ids — which is why a re-key regenerates
> techs, not just traits.
> **⚖ A SAVE IS RESOLVED INTO THE ACTIVE SET AT LOAD (owner): *"for savegames, if you see it is a complex trait
> game, you make sure the trait is the complex version."*** A stored plain `TRAIT_X` in a game running
> `GAMEOPTION_LEADER_COMPLEX_TRAITS` resolves into the active set. This is distinct from the
> `savemigration.txt` rename plane (which id, not which SET), so it lives at the ONE stored-Type resolution
> point (`sm_resolveStoredType`), beside the rename lookup rather than inside it — otherwise a loaded save could
> hold simple rank-1 rungs beside complex rank-2/3 ones.
> ⛔ **The sets are SEPARATE and complex carries no rung 0** ([the separate-trait-sets rule](#4-ownership--the-deliveryguy-rule)),
> so this resolution may NOT assume a prefixed id always exists — the retired superset claim is exactly what
> made it look free. **Which rung a stored un-digited `TRAIT_X` resolves to in a complex game is UNDECIDED and
> is the owner's call**; do not infer one.
> ⚠ **Leaderheads DO author traits** — 118 of 120 carry both a `traits` and a `complexTraits` list, so a NEW
> GAME takes its held ids from the leaderhead and the save is not the only source. *(The retired claim that
> leaderheads author none was used to argue the save-side resolution was sufficient on its own.)*
>
> ⚠ A record that does not obey this is a CURATOR defect, and fixing it rides the curator + regen in the same
> work item ([recurate on every decision](../AGENTS.md#git--delivery)); the id change is
> a TYPE RENAME the save layer translates via `Assets/savemigration.txt` (the rename rule below), never a removal.
> (The enabler is unaffected either way: it reads trait *presence*; only the modifier cascade reads trait
> *family values*.)
>
> **⛔ Inverted-onto-a-SHARED-entity boosts stay on the TRAIT, per set — the own-output carve-out.**
> The [deliveryguy rule](#4-ownership--the-deliveryguy-rule) normally puts a trait's boost of *another* entity's output
> ON that entity as **own-output** (a trait boosting a Merchant's commerce → on the **specialist**, `enabled:{trait}`).
> But a **specialist is ONE shared file**, while a split trait's `SpecialistYield/CommerceChange` has **different values
> in the simple vs complex set** — so inverting it onto the specialist would force a single value across both systems and
> break the clean separation. Therefore, for a TRAIT keyed to a specialist (or any shared sub-city target with a per-set
> value), the deposit takes the **governing-deliverer** shape instead: it lives **on the trait, keyed by the target** —
> `yield.empire.specialists.{SPECIALIST_X}.flat` (and `commerce.…`) — authored in **each set's folder** (simple = the
> base value; complex = the **replacement's** value — a **whole-Info swap, NO base-fill**, per the legacy
> replacement semantics: a field the replacement
> omits is **inherited from the base**, §4-bis). The cascade reads it from the **active** trait
> set and applies it × the city's count of that specialist. *(Building/civic specialist boosts have no
> simple/complex split, so they keep the ordinary own-output inversion onto the specialist.)*
>
> **⛔ Trait option resolution — the curator translates the CRAZY → sensible; the cascade applies only CLEAN gates
> (this is the volcano every agent rollerskates into — read it before touching trait values).**
> Several `GAMEOPTION_LEADER_*` options can be live at once (complex, developing, pure, no-negative, …) and each
> mutates a trait's *effective* values. The TB implementation was a runtime hack — **deleted from this tree**, and
> described here only so it is never rebuilt: a base trait carried an inline replacement id + a `BoolExpr` condition,
> and a global re-run swapped the WHOLE `CvTraitInfo` in place for the first replacement whose condition held. **We do
> NOT emulate that hack anywhere in the cascade.** The split of responsibility is absolute:
>
> - **CRAZY → curator (`curate_trait`), offline, once.** The replacement/promotion-line machinery is dissolved into
>   sensible JSON:
>   - **Simple/complex split** by `COMPLEX_TRAITS` — the two `DefaultTraits`/`DefaultComplexTraits` sets become
>     `traits/simple/` + `traits/complex/`.
>
>     > **⛔⛔ THE TWO SETS ARE COMPLETELY SEPARATE AND EACH IS SELF-COMPLETE ON ITS OWN TERMS — THERE IS NO
>     > OVERLAY, NO BASE-FILL, AND NO SUPERSET RELATIONSHIP (owner).** A complex record is NOT a simple record
>     > with tags laid over it, and a simple record is never copied into `complex/` to make the sets line up.
>     > Each set is authored, emitted and read as its own table; the only thing they share is the game option
>     > that selects which one is live.
>     > ⛔ **The overlay/`<ReplacementID>` machinery was TB's workaround for not knowing how to do this properly.
>     > It is NOT the model and is NOT reproduced** — do not rebuild it, and do not reason from it.
>     > **⛔ A COMPLEX GAME HAS NEVER USED RUNG 0 OF ANY TRAIT — A LINE IS `1 → 2 → 3` (owner).** The un-digited
>     > record is the SIMPLE set's base; in `complex/` it is not a lower rung, it is the simple trait leaking in,
>     > and nothing in a complex game ever holds it. So a leaderhead's `complexTraits` names rung 1 and above,
>     > never a base beside it.
>     > ⚠ **This has been corrected repeatedly, and every recurrence traced back to THIS PARAGRAPH still carrying
>     > the retired model** ([rulings go to the repo immediately](../AGENTS.md#documentation--knowledge--keep-it-in-the-repo)): the
>     > ruling was given in conversation and the spec was left standing, so each new agent read the overlay model
>     > here and rebuilt it in good faith. A ruling that is not written down is a ruling that gets re-litigated.
>     > ⚑ **The measurable tell that the leak is present:** `complex/` carrying an un-digited record for a line
>     > that has numbered rungs. Only a line with NO rungs may legitimately have a bare record.
>
>     **Folder classification** keys on the `OnGameOptions: COMPLEX` gate /
>     replacement-variant; a developing-line (`PromotionLine`) member that UNIQUELY lacks the gate its siblings carry is
>     a SOURCE-data bug to fix (restore the tag), not a classifier change (the `TRAIT_TIMID1` case). The active set is
>     chosen by the live option (callout above).
>     **⛔ TRAITS ARE NOT CONTENT-LOCKED — THE CURATOR IS THE AUTHORITY AND THE FOLDERS ARE REGENERATED (owner).**
>     A hand-maintained lock let an edge in one set point at an entity only the other has, with nothing regenerable
>     to correct it; `curate_trait` reads the legacy XML like every other curator and `--write` rewrites both
>     folders. ⚑ **Its input is the ARCHIVED XML** (`SourceArchive/Assets/**`, searched by `store.py` alongside the
>     live roots) — curator INPUT only, never a game load path
>     ([reading a replaced info's XML into the game is banned](../AGENTS.md#build-and-test)), and unrelated to the red-ratchet
>     ban on reviving a `CvXInfo` from `SourceArchive/Infos/`. ⚠ Community-owned trait CONTENT still lands through
>     `_additions/` like any other post-curation authoring ([curators/README.md](specs/curators/README.md)) — a
>     regenerable base with an overlay, not a frozen folder.
>   - **⛔ THE LADDER EDGE IS RESOLVED FROM LINE MEMBERSHIP, NEVER FROM THE ID SPELLING.** A rung `enables` the rung
>     above it ([json.md §9](specs/json.md): a ladder is an `enables` edge, not a section), and which rung that IS comes
>     from the line's members ordered by `iLinePriority` — restricted to the FOLDER being emitted, so a chain simply
>     ends where that set ends (`simple/` tops out at rung 1) and never reaches into the other set. The base rung is
>     `iLinePriority` 0/absent, and the two arms (`+1,+2,+3` and `-1,-2,-3`) each chain outward from it.
>     ⚠ Deriving the successor by string arithmetic on the id (`TRAIT_X1` → `TRAIT_X2`) fails silently on a
>     mid-chain RENAME (`TRAIT_NOMAD1` → `TRAIT_NOMADIC2`, a fabricated edge to an id no record defines), a rank
>     SKIP (the link is lost entirely), or a top rung (an edge to a rung that does not exist).
>   - **Developing line — do NOT auto-develop (engine-verified).** A `PromotionLine` is a chain of trait *levels*
>     (`TRAIT_NOMAD1`→`TRAIT_NOMADIC2`→`…`, ordered by `iLinePriority`, each with a `PrereqTech`+`TraitPrereq`), but
>     **researching a level's `PrereqTech` does NOT advance the held trait**. The **held trait the engine reports IS
>     the authoritative level**; the cascade uses its payload as-is. ⚠️ A tech-gated "collapse" that folds higher
>     levels into the entry is the WRONG model (it re-levels traits the engine leaves alone). Levels advance by some
>     other gameplay progression, not by tech alone; until that's mapped, trust the engine's own reading.
>   - **Complete, not pre-filtered.** The JSON carries ALL values — positive AND negative — plus the `negativeTrait`
>     flag, so the runtime gates below have the full data to act on. The curator never bakes in a pure/no-negative pass.
> - **CLEAN gates → cascade, at eval (its ordinary condition-eval, NOT hack emulation).**
>   - **`PURE_TRAITS` gate (implemented)** — when `GAMEOPTION_LEADER_PURE_TRAITS` is live, drop each trait value whose
>     alignment opposes the trait's: a `negativeTrait`'s **upside** values drop and a positive trait's **downside**
>     values drop.
>     > **⛔ ALIGNMENT IS FAMILY METADATA, NOT THE SIGN — `infoKindAlignmentInverted(family, kind)` is the one
>     > table.** On an **INVERTED** (family, kind) a POSITIVE value is the DOWNSIDE, because the number counts a
>     > cost, a penalty, a timer or a worse-when-higher threshold. Grounded row-for-row in the legacy per-getter
>     > filters (`CvTraitInfo`): 48 members guard `isNegativeTrait() && x > 0` and **19** guard
>     > `isNegativeTrait() && x < 0`. Inverted today: `maintenance` · `costs` · `hurry` · `lessYieldThreshold` ·
>     > `growth` · `anarchy` (whole families); `upkeep` **except** its free-amount kinds (§2: a positive
>     > `freeMilitary`/`freeCivilian` GRANTS, so it stays normal); `experience.levelModifier` only;
>     > `durations`' two ANARCHY timers; `diplomacy.warWeariness` only — its `enemyWarWeariness` twin is a GAIN
>     > and `attitude` an ordinary upside; and `revolution`'s unrest kinds, whose `*Good` twins stay normal.
>     > ⛔ **Three families carry BOTH polarities** (`revolution`, `diplomacy`, `durations`), which is why this is
>     > declared per (family, KIND) and can never be collapsed to a per-family flag.
>     > ⚠ **`growth` is the trap worth naming:** it is the growth THRESHOLD percentage — higher means more food
>     > per citizen, i.e. slower — so it reads as an upside and is not. It surfaced on the FOOD tooltip, where a
>     > positive trait was losing its upside and keeping its downside on the one number displayed.
>     > ⛔ **Derive the list from the legacy getters, never from the authored SIGNS:** traits author both
>     > directions in every one of these families, so the data cannot tell you the polarity — only the 48/19
>     > getter split can.
>     > ⚑ **A sign-only gate is wrong in BOTH directions and silently so:** it keeps `lessYieldThreshold: +5` on a
>     > positive trait as though a gain, and drops `maintenance.distance: −10%` — a genuine upside — as though a
>     > penalty. Neither shows as an anomaly; the totals stay plausible.
>     Concretely for thresholds: an
>     `extraYieldThreshold` is an UPSIDE → dropped from a negative trait; a `lessYieldThreshold` is a DOWNSIDE →
>     dropped from a positive trait (engine `getLessYieldThreshold` 2132-2147 sets it to −1). The cascade reads the
>     `negativeTrait` flag (`NegativeTraits*` in the repo) + the live option. This is how "parity comes to us": a
>     legacy behaviour we judge correct is reproduced by a clean gate, never re-implemented as the hack.

**`production` vs `buildRate`.** `production` = `InfoValuation::cityRate`'s PRODUCTION channel (total city output — scales every
build every turn; a flat ADD or city-wide percent). `buildRate` = `getProductionModifier(eItem)` (shrinks the
COST of a SPECIFIC item, never a per-turn yield), sub-shapes `buildRate.self` /
`.<scope>.{units|buildings|domains|unitCombats}.{TARGET}` (keyed) / `.<scope>.{units|buildings}` predicate-filtered
(the §5 plural target) / `.<scope>.{worldWonder|teamWonder|nationalWonder}` (category). (The "Versailles bug" =
filing an item discount under `production.city`.)

> **⛔ AN EVENT AUTHORS YIELDS, NEVER A PRODUCTION MODIFIER — WE DO NOT DO `buildRate` ON EVENTS (owner).** An
> event's payload is the ordinary yield vocabulary; it does not reach this family at all, at any scope.
> ⚑ **The reason is the SHAPE, not the size of the effect.** A `buildRate` deposit is alive-while-its-source-is
> — the continuous-deposit model this whole doc describes — while an event is a one-shot happening with no
> surviving source to withdraw against, so a slot fed by one can be maintained by neither mechanism
> (§ WHY DELTA-DERIVING FAILED BEFORE, above: a baked-in
> one-shot grant is precisely what makes an accumulator unrecoverable).
> ⛔ **And if it is ever wanted, it is built PROPERLY — not in a roundabout fashion (owner).** The legacy shape
> reached it by pushing into a hand-named per-player accumulator behind an `applyEvent` write, which is the
> STORED-ACCUMULATOR DRIFT class ([the uniform legacy-accumulator cut](#-the-legacy-accumulator-cut--every-accumulator-one-uniform-mechanism));
> reviving that is the banned move, and a genuine event-driven build discount would author on the trigger plane
> ([triggers.md](specs/triggers.md)) like every other happening-fired effect.

> **⛔ `military` AND `space` ARE NOT CATEGORIES — `units` IS THE BASE TARGET AND THEY ARE PREDICATES ON IT
> (owner): *"military is not a base category, units is."*** Both legacy tags answer WHICH UNITS build faster, so
> both author the ordinary plural target with a filter — `buildRate.<scope>.units.percent`, entry
> `{value, enabled: IS_MILITARY | IS_SPACE}` — which is [§6.1](specs/json.md)'s own `units {IS_WATER}` exemplar and
> needs no vocabulary of its own.
> ⛔ **A category member per legacy tag NAME is the curator minting a kind off a spelling** — the
> condition-as-member shape [conditions are predicates, never bespoke members](specs/json.md#35-predicates--a-systems-runtime-state-query)
> retires, and it is what the §6 member-triage test already rejects: a member is a KIND only if it answers WHICH
> COMPONENT, never WHICH TARGET or WHEN.
> ⚠ **The buildRate MECHANIC is legit and is not under review (owner): *"buildRate is a legit mechanic, we do not
> kill them all."*** This narrows how two members are ADDRESSED; it removes no effect.
> ⚑ **Spacecraft are not a class outside the military one**, which is why space is a sibling predicate rather
> than a tier of its own. ⛔ And the legacy consumer's gate — `CvProjectInfo::isSpaceship`, the vanilla
> space-VICTORY spaceship parts — **does not apply**: that is vanilla Civ's victory machinery, not the space
> units the boost describes.

---

## 5. Targets — scope-wide, object-plural, or keyed

A deposit lands in one of three ways ([json](specs/json.md) §6.1):

> **⚖ AN EMPIRE→CITIES DEPOSIT HAS TWO LEGS, FOR THE SAME REASON THE AMENITY FOLD DOES**
> (§ THE FOLD HAS TWO LEGS, above). A source above city scope delivers its
> CITY-scope deposits by fanning over the owner's cities — which reaches exactly the cities standing **at that
> moment**, and that is not all of them:
> - **at LOAD the emit order is not uniform**, and nothing makes it so: some empire-level facts are announced
>   before the cities deserialize and some after, so one grantor's fan lands and the next one's iterates an empty
>   list. A fan alone therefore delivers a subset decided by where a member happens to sit in a read.
> - **at PLAY a city that starts existing later** — founded, or acquired — receives nothing from what its owner
>   already holds, permanently.
>
> ⇒ **The second leg is the CITY's: when a city starts existing it folds the city-scope deposits of every source
> its owner already holds.** The trigger is the city's own OWNERSHIP fact, which is the one announcement common
> to founding, conquest and the save read alike — so there is no separate load pass and no city-founded special
> case beside it.
> ⛔ **It must be IDEMPOTENT rather than guarded.** The package already records which sources have deposited into
> it (the same liveness key planes B and C test), so the fold SKIPS what the fan already delivered. Suppressing
> the fan during load instead would work only while a hand-written guard stays in step with an emit order nobody
> controls; the package's own record cannot disagree with what was applied.
> ⚠ This is not a rebuild and not a recompute — the worklist is the owner's HELD sources, each resolved through
> the one per-entry evaluator ([the DRY single-implementation law](architecture/patterns.md#dry--one-implementation-per-calculation--evaluation-the-single-source-law)).

- **scope-wide** — no target: the scope object itself (the city is the common case).
- **plural object-target** (`plots` / `units` / …, predicate-filtered) — realized by evaluating the predicate
  against **every object of that kind in scope** and depositing onto each match. One uniform mechanism: an
  empire-wide sea-tile buff is `production.empire.plots {IS_WATER}`, applied to every worked water plot. This
  retires all the legacy per-plot-type / per-tile accumulators.
- **named-entity key** (`improvements.{FARM}`, `terrains.{…}`, `buildings.{…}`) — a deposit onto a specific
  named target, kept on the source (the deliveryguy, §4).

> **⛔ A KEYED DEPOSIT IS READ AS AN ENTRY-LIST READ OVER THE LIVE SOURCES — never off a scope package.** Outside
> PLOT scope a keyed entry deliberately does **not** fold into the scope's Σflat/Σpercent slots (only the plot's own
> substrate keys resolve there, §2 plot-as-base; the `empires` fan is the one target whose fold IS the deposit). So
> a consumer answering *"how much does this source give THIS target"* asks each live source what IT deposits onto
> that key — the city's OPERATING buildings, its assigned specialists × count, the empire's held traits — and sums.
>
> **⚖ A KEYED ROW'S REACH IS ITS AUTHORED SCOPE, AND BOTH SCOPES ARE REAL ON A BUILDING (owner).** A building is
> a per-city source, so a CITY-scope keyed row means faster HERE — *"units are scoped on the city the building is
> in"* — and the read over the city's own OPERATING buildings is exactly that semantic. An EMPIRE-scope keyed row
> on the same building means faster in EVERY city of the owner, and is answered player-side.
> ⛔ **So the two halves are read at DIFFERENT SCOPES and must each filter to their own**, or the city holding the
> source claims the empire rows a second time on top of the player's answer. This is the live case the
> `collectKeyedTarget` scope filter exists for, and the reason the point form needs it too.
> ⚠ **Neither half is mis-authored data, and re-scoping one to "simplify" the read is a BALANCE CHANGE wearing a
> cleanup.** The empire half is the classic wonder effect (a wonder cheapening a building across the empire) and
> it is populated; the city half is the local one. A cut that collapses them would silently delete whichever
> mechanic it did not keep.
> ⚑ CIVIC- and TRAIT-authored keyed rows are empire-scope by nature — those sources have no city — so the
> player-side walk is the only thing that could serve them, and it does.
> ⚑ **Why it is a rule and not a detail: folding a keyed entry into the scope slot is silently, plausibly WRONG.**
> A building's `experience.city.unitCombats.{UNITCOMBAT_MELEE}` folded scope-wide would hand EVERY unit trained
> there the melee-only experience — a number that looks reasonable, breaks no invariant, and nothing catches. The
> package answers the scope-wide leg; the keyed axes are read beside it.
> ⚠ The read is per-source and cheap because it iterates the handful an entity AUTHORED
> ([materialize at mapFrom](architecture/patterns.md#materialize-at-mapfrom--no-runtime-string-reads-in-info-getters-the-single-source-laws-load-time-sibling)); it is never a walk of a
> keyed container the info no longer holds, which is the own-data inversion
> ([pedia-read-map](reference/pedia-read-map.md) finding 2).
>
> ⛔ **A KEYED READ SERVES THE UNCONDITIONED ENTRIES ONLY — the conditioned tail is the VALUATION's, exactly as
> it is on the point plane** ([patterns.md § THE GETTER SETUP](architecture/patterns.md): the compiled sum,
> the conditioned list and the `expected*` what-if are three distinct reads, and a keyed deposit needs all three
> just as a scope-wide one does). A keyed walk that sums the tail applies every tech-gated and age-gated deposit
> from turn 0 — silently, because the number stays plausible. ⚑ The keyed twin of `expected*` is what serves
> that tail (through the ONE evaluator against the contexts); until it exists a keyed+conditioned deposit is
> honestly UNSERVED, which is the correct exposed state rather than a gap to paper over with an unconditional
> sum ([legacy must fail loud, never mask a cascade gap](specs/validation.md#legacy-must-fail-loud-never-mask-a-cascade-gap)).
>
> ⚠ **THE DIRECT-KEYED ADDRESS IS A REAL SHAPE, AND ITS SENTINEL MUST NOT COLLIDE WITH "NOT AUTHORED".** A
> named-entity key may sit straight under the scope with no plural container token
> (`allowedSpecialists.city.{SPECIALIST_X}`, `religion.city.{RELIGION_X}`), so the compiled entry carries NO
> target-segment. A read that treats "no segment" as a failure answers 0 for every such address while the caller
> passes the right family, kind and target — invisible, because nothing errors. The two meanings are opposite
> intents and each needs its own value: *this address carries no container token* vs *that token was never
> authored anywhere*.

---

## 6. The unit plane — a self-accumulator

A `unit`-scope deposit is a **self-accumulator**: source == target. A unit's promotions and unit-combat class
deposit their stat changes onto the unit itself (the existing additive promotion stack), summed for O(1)
concatenation as each promotion is added — not a downward cascade.

**Host-from-occupants** effects — what a city gets *per unit stationed in it* (military happiness/anger) — are
**not** a bespoke host-family: they're an ordinary deposit on the source (the civic/trait), scaled by a
predicate-filtered unit count and targeting `cities`: `happiness.empire.cities.{unit: IS_MILITARY, flat: N}`
([json](specs/json.md) §3.7). The **carrier↔cargo** behaviour splits across the two systems. The carry *ability* is a unit **skill** — whether
the unit may use the **load/unload** action is `is_cargo_vessel`, and the attack restriction it brings is
`defend_only` (both skills, [json](specs/json.md) §8). The *amounts* live in the **`cargo`** modifier family (a unit
self-accumulator, set on the unit or a promotion), with two complementary members:

- **`cargo.space`** — how much the unit **carries** *and what*: `cargo.space.{unit: IS_<domain>, flat: N}` — a
  carrier is `cargo.space.{unit: IS_AIR, flat: N}` (*you can't transport a plane on a landing craft*); an
  unrestricted hold is just `cargo.space.flat`. (From legacy `iCargo` + `DomainCargo`.)
  > **⚖ THE RESTRICTION IS THE CARRIER'S AND GOVERNS ITS WHOLE HOLD — including capacity a PROMOTION grants
  > (owner).** WHAT a carrier may take is a property of the carrier; HOW MUCH sums from every source. So a
  > restriction never binds only the entry it is written on: an ancient galley that carries civilians carries
  > civilians in the space `PROMOTION_TRANSPORT1` adds too, never a warrior in the promoted slot.
  > ⚑ **This is a real mechanic, not an edge case:** the whole ancient-navy transport line has **zero base
  > `iCargo`** and earns its hold by promotion (TRANSPORT1/2/3 on `UNITCOMBAT_WOODEN_SHIPS`), so the carrier
  > declaring WHAT and the promotion supplying HOW MUCH is the normal shape there, not an anomaly.
  >
  > ⚖ **A PROMOTION ADDS SPACE, NEVER PERMISSION — an INTENTIONAL divergence from legacy (owner: "we go with
  > yours, it's cleaner").** In the legacy game a transport promotion WIDENS the class carried: an unpromoted
  > galley takes a settler, a promoted one takes military. The ruled model does not reproduce that — WHAT is the
  > carrier's, fixed, and a promotion only ever changes HOW MUCH. ⛔ So do not "repair" this back by letting a
  > promotion author a wider qualifier: the behaviour change is chosen, and the reason is that a permission that
  > moves with promotions puts WHAT in two places and makes a carrier's rule unreadable from the carrier
  > ([validation.md](specs/validation.md) intentional-model-change class; the spec leads, legacy behaviour is not
  > preserved for its own sake).
  > ⚠ Consequence: a carrier whose base capacity is 0 still has a restriction to state, and the §3.9 entry
  > grammar has no payload-less form for it — an open item for the json spec.
  ⚖ **The "what" is ALWAYS a TAG predicate — that is what tags are for (owner).** The legacy restriction by
  `SPECIALUNIT_*` group (`SpecialCargo` / `SMNotSpecialCargo`) brings no new qualifier form with it: it authors as
  the same `{unit: IS_<TAG>}` shape as the domain case. ⚠ It does require the tag to exist AND to be
  DISCRIMINATING — several legacy groups are indistinguishable on the current tag set (people and troops are both
  merely `landUnit`; fighters and seaplanes both merely air/military), so converting one before its tag is minted
  silently WIDENS what the carrier accepts. Mint the tag first; that is ordinary open-registry authoring
  ([tags.md](specs/tags.md)).
  ⚖ **Capacity has ONE home, and Size Matters DERIVES from it (owner):** `smSpace` follows from how many units
  the carrier can hold, so it is never a second authored number ([json.md §9](specs/json.md)).
- **`cargo.size`** — the unit's cargo **footprint** (room it occupies when loaded), **defaulting to 1** if unset.
  (SizeMatters extends cargo via `smSpace`/`volume`/`volumeModifier` — a separate rework.)

No bespoke host↔cargo family is needed. The full unit-stat family vocabulary
(`strength`/`withdrawal`/`firstStrike`/… ) is [json](specs/json.md) §6; this is the largest surface and lands last.

> **Movement & range** are their own resolver subsystem, not ordinary downward families: `moveCost` is computed
> **per `(unit, edge)`** with a route `min`-override, double-move divisors, and a floor — it doesn't fit the
> "deposit DOWN → O(1) summed read" shape. **But the RESOLVER being bespoke does not make its INPUTS intrinsic
> (owner): a plot substrate's base movement cost IS the `movement` family** — `movement.plot.flat` on the
> terrain / feature / route — and it composes with the cascading deltas (tech route changes, promotion move
> bonuses) in the ordinary way, as the §3.9 entry list. The route case shows it directly: the base cost is the
> bare number and a tech-gated change is a conditioned entry beside it, in one slot.
> ⚑ The distinction to hold: **the resolver reads the family and applies its own arithmetic** (min-override,
> divisors, floor). What was wrong was parking the base value in `identity`, which carries no effects
> ([json.md §7](specs/json.md)) — a movement cost is plainly one.

### Specialist counts

- **`freeSpecialists:{<scope>:{any:N, SPECIALIST_X:M, …}}`** — granted specialists; `any` = an assignable-slot
  bucket, a typed entry is auto-assigned. Leaf is a count (a list when conditioned). ⚠ Here `any` is a **count key**
  (an untyped specialist slot), **NOT** the [json](specs/json.md) §3.4 condition combinator.
  > **⛔ `any` IS AN AMOUNT, NOT A TARGET — and that decides whether the family works at all.** The untyped
  > bucket is N slots whose specialist type the ENGINE picks at placement (the two-part seam below), so it
  > carries no target: it decodes as the **memberless scope-wide amount**, exactly like any other magnitude.
  > ⚑ The consequence is structural rather than cosmetic. A deposit carrying a TARGET segment is excluded from
  > its scope's package by construction (only point-foldable entries fold), so registering `any` as a target
  > token strands the amount outside the package plane — no scope roll-up can answer it, and the only read left
  > is a per-call walk of every authoring source. A TYPED `SPECIALIST_X` entry is genuinely keyed and correctly
  > stays an entry-list read (§5); `any` is not, and must never be given the same treatment.
- **`allowedSpecialists:{<scope>:{SPECIALIST_X:N}}`** — the manual-assign cap, per-type only (no `any`).
- `free` lives ON TOP of `allowed` (independent). Normally a modifier leaf is `<scope>.<unit>` (e.g. a bare
  number or `.flat`); specialist counts instead use a **count-by-type** leaf (the `SPECIALIST_*` type — or `any`
  — IS the key, its value the count) — the one sanctioned exception, chosen for legibility.
- **freeSpecialists are MODIFIERS, never grants.** A free specialist is alive **only as
  long as its source is** — building present / civic adopted / trait active — the continuous-deposit shape, not a
  handed-out provision. Every legacy `changeFreeSpecialistCount` apply (civic/trait/building) classifies to THIS
  family; none belongs to the grants machine (`specialists` is not in the [json.md §5](specs/json.md) grants vocabulary; *if
  anything is ever found that genuinely grants PERMANENT free specialists — surviving source destruction — we deal
  with it then*; no hypothetical machinery).
- **⚖ THE TWO-PART SEAM (the promotion-SPA seam pattern applied to specialists).**
  Free specialists split cascade-vs-engine in two parts: **(1) the AMOUNT** of free specialists is the
  CASCADE's — the summed `freeSpecialists` deposits (per type + the `any` bucket) from live sources;
  **(2) the PLACEMENT** — the engine decides how to place them within the parameters it has (typed entries
  auto-assign; the `any` bucket + citizen assignment ride the existing, reliable engine infrastructure);
  **(3)** consumers then *"simply deal with the OUTPUT of that"* — the realized per-type counts
  (`getSpecialistCount + getFreeSpecialistCount`) are a **sanctioned output-seam read**, never a
  self-containment ride-in. Demolition consequence: the cut replaces WHO MAINTAINS THE AMOUNTS (the cascade's
  summed deposits replace the `changeFreeSpecialistCount` process-applies feeding the placement); the
  placement machinery and its output reads stay.

---

## See also

- [json.md](specs/json.md) — the data this machine reads: the modifier-family address, `flat`/`percent`/`multiplier`
  units, `enabled`/`disabled`/`per` conditioning, `plots`/`units` targets, and the `buildRate` vs `production`
  split (§3, §6).
- [enabler.md](specs/enabler.md) — the "can I?" machine. Availability is upstream of magnitude: an unavailable or
  dormant entity deposits nothing.
- [tally.md](specs/tally.md) — the count machine a `per` scaler reads at cross-city scopes.
- [naming.md](specs/naming.md) — the `INFOTYPE_NAME` ids used as deposit keys and condition atoms.
- [patterns.md](architecture/patterns.md) — the INFO DATA-OUT contract + the per-group valuation surface that
  reads the contexts.
- [spine.md](spine.md) — the event dispatch primitive every consumer on this page (the modifier consumer, every
  context dictionary, the package apply path) draws its facts from.

