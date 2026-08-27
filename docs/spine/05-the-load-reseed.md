# 5. The load reseed

> Part of the **[spine](../spine.md)** spec.

**The load RESEED — the save read goes through the INTERNAL SETTERS.** A loaded save used to deserialize straight
into the `CvCity`/`CvPlot` members, so the setters never fired and the **cascade** (its value packages AND its
enabler side) had nothing to build from. The reseed is fixed **at the read itself**: each slot deserializes into a
LOCAL and is handed to that slot's **internal setter** — the ONE body that commits the member, maintains whatever
derived state the object owns, and announces the fact.

> **⛔ THE CRUD IS NOT THE EVENT; WHAT HAPPENED IS.** The event does NOT set the state, and the earlier
> north-star that said so — *read → emit → populate* — was **backward**. It made an EFFECT the thing that mutates
> base state, which violates the principle the whole model rests on, and it is precisely what the old `*_CHANGED`
> payload existed to serve (an old value beside a new one, so a consumer could drive the mutation). **The stream
> is authoritative for base state; the fact is TESTIMONY about a completed act, in the past tense.**
>
> **⚖ THE PRINCIPLE, AND WHY THE OTHER ORDER COLLAPSES: state is set DIRECTLY, in one request, and the
> event fires as a RESULT of that state having been set.** That is the core principle violated: setting state with events produces real con
>
> **⛔ AND THE LINE IS BASE STATE vs DERIVED — THIS IS THE SPLIT THE CASCADE AND THE ENABLER ARE BUILT ON
>.** The two halves take OPPOSITE rules, and collapsing them in either direction breaks the model:
>
> | | set by | the event is |
> |---|---|---|
> | **BASE state** — a building actually placed, population, research progress | its own SETTER, directly | TESTIMONY, after the fact |
> | **DERIVED state** — the cascade packages, the enabler's sets, the context stores | **the events themselves** | the MAINTENANCE path |
>
> ⇒ *"The derivation can be set from events; the actual base state cannot."* So
> [the maintained sum](../cascade/05-three-planes.md#-the-maintained-sum--three-planes-one-slot-and-nothing-is-ever-recomputed) is not an exception to the principle above
> — it is the principle's other half. A package slot moving because a fact arrived is the design; a BUILDING
> existing because a fact arrived is the retired order.
> ⚠ **The misreading to avoid in each direction:** reading "events do not set state" as reaching the derived
> plane would ban the maintained sum outright; reading "events build the state" as reaching base state restores
> the collapse this callout describes. The save stream is authoritative for the first, the fact stream for the
> second, and neither is authoritative for the other's half.
> ⚑ **The failure is COMPOUNDING, not local, which is what makes it worth stating as a principle rather than a
> preference.** An event that SETS state is a mutation whose ordering is now a scheduling question, so every
> consumer that needs the result must be told — with another event — and each of those is another mutation
> needing its own announcement. The surface grows events to service events, and the ordering hazards multiply
> with it.
> ⇒ Under commit-then-announce there is nothing to schedule: the state is already correct at the instant the
> fact fires, so a consumer DERIVES from settled state and announces nothing back. That is what makes
> order-independence structural (the maintained sum's operands all converge regardless of arrival order) rather
> than something the load has to compensate for.
> ⚠ **The compensation is recognisable in the tree, so this is a thing to FIND and not only to avoid:** a
> load-time BANK that replays facts later exists because a fact was once expected to do the work, and it survives
> as pure cost once the setters announce instead. Read one as evidence of the retired order, never as a mechanism
> to extend.
>
> ⇒ **An INTERNAL setter is commit + maintain + announce, and NOTHING else.** The public setter is its guard,
> then the internal one, then its EFFECTS (plot groups, areas, sight, graphics, cascading type changes). The read
> calls the internal one directly — which is exactly what lets it: no effect gets to decide any part of the state
> the save is authoritative for.
> ⚑ **And the drift it closes was real, not theoretical.** While the read wrote members raw, the emit and the
> derived-state maintenance lived in a SECOND place that had to be kept in step BY HAND — and was not: five
> `CvPlot` slots deserialized with no fact at all, and the movement hash needed a whole rebuild pass at the end of
> the read to paper over the maintenance the setters would have done. The pass is gone with the bypass that
> caused it. ⛔ Such a rebuild can NEVER stand beside the setters: both would apply, and for an XOR-maintained
> value that cancels it to zero — silently, on every object.
> ⚠ **The save TAG does not move when the destination does.** `NormalizeName` strips the address-of and any cast
> (*"m_thingy on save should match `(int*)&m_thingy` on load"*), so a `WRAPPER_READ_DECORATED` naming the tag
> explicitly reads the byte-identical tag the plain form produced. Keep each local the MEMBER'S OWN TYPE — the
> wrapper picks its read overload from the destination, and a wider local asks the stream for a type code the
> writer never wrote ([save.md](../specs/save.md)).

Tech is team-held but emitted per-self from each member's `CvPlayer::read`, one emit per alive member; projects
the same. Beside the DOMAIN facts the setters emit, the read may announce what the STREAM CONTAINED as
`SAVELOAD` lines — a log of loading, consumed by logging alone.

**The load reseed, concretely:** `CvPlayer::read` per held tech / project / civic / trait / heritage + era /
golden-age / state-religion / nukes / commerce sliders; `CvCity::read` per building / religion + holy-city /
corp / specialist / population / power / culture-level; `CvPlot::read` the whole substrate + owner +
working-city — through the INTERNAL SETTERS, so the read announces nothing itself; `CvProperties::readWrapper`
per stored property value on every owner scope — wrapped by the `GAME_LOAD_STARTED` / `GAME_LOAD_FINISHED`
bracket. ⚠ The property block emits from its own read because a property value is DERIVED FROM NOTHING; nothing
else could announce it. A stored 0 is skipped, for the same reason the setter suppresses a no-op (the owner's
`reset()` emptied the bag, so 0 → 0 is not a change); the per-turn CHANGE ledger beside it is deliberately silent,
being an accumulation of deltas the value facts already carry. ⛔ **ONE SERIALIZED SLOT, ONE FACT — there is no
slot a neighbour's fact covers for.** A consumer must never reach back through an object to re-derive a block
from a fact that did not name it — a fact sets ONLY the bit it names ([contexts.md](../cascade.md)),
so a slot nothing announces is a slot nothing knows about.
⚑ **The fix is structural, not a hand-maintained emit list:** the emit lives in the slot's INTERNAL SETTER, so a
newly serialized field cannot be added without going through the one body that announces it.
⚠ **The WORKED set is the CITY's slot, so it reseeds from `CvCity::read`, not from the plot's.** The plot
carries the `IS_WORKED` verdict but only the city can attribute it, so the array's in-read landing is where
the fact comes from — and the cities stream AFTER the map, which is exactly what makes the plot available to
carry the bit and puts the fact before the `GAME_LOAD_FINISHED` fold that reads each plot's FINAL block.

**The reseed grew the matching in-read halves** wherever the setter cannot run on a load: `CvCity::read` (the
disabled-power timer — a save can be taken mid-blackout — and the headquarters designation, tested off the
loaded IDInfo via `CvGame::isHeadquartersByOwnerId`, the `isHolyCityByOwnerId` precedent, because `CvGame`'s
array deserializes before the cities), `CvPlayer::read` (anarchy turns — a save can load mid-revolution — the
golden-age twin), `CvPlot::read` (the plot's city, whose fact no other slot covers — no slot ever covers for
another), `CvTeam::read` (the member count, landed through `changeNumMembers` after `m_eID` deserializes rather
than beside the count, since the id the fact hangs on is only valid from there — and that is the team's WHOLE
reseed, because `EmpireContext` forwards exactly three team facts and the other two, techs and projects, are
announced per-self from `CvPlayer::read`), and **`CvUnit::read`, which previously emitted NOTHING** — the
instance, its promotion set and its combat-class set, each at its own genuine per-element read.
⛔ Three in-read halves are deliberately ABSENT and are not oversights: **the world unit-created counter**
(nothing stores a derivative of it — the cap reads it live, so there is nothing to seed), **the area tile count**
(`SEVT_AREAS_RECALCULATED` is a WHOLESALE fact by construction — it names no source, so every area-id holder
re-reads on it and a per-area announcement would say nothing the wholesale one has not), and **the
fold-announced verdicts** (power / government centre / fresh water — the amenity fold rebuilds from the load's
own facts and announces each verdict crossing itself, so an in-read emit beside it would announce the same
crossing twice). ⛔ Neither is the retired *"another slot's fact covers it"* argument, which is why they are
stated in their own terms: one has no consumer to seed, the other already has a fact. **No slot is ever covered
by a neighbour's.**
⚠ **One endpoint is deliberately unwired: `emitLoadPipeline`** — every one of its arguments is produced by the
archived load-time warm-up/rebuild pass, which the CAPSTONE rule removed
([cascade.md](../cascade.md)); the event reseed replaced that pass, so the
endpoint has no honest caller. Open follow-ups: the tile-driven vicinity backstop, and the per-city enabler
priming that preceded the reseed emits.

⛔ **What the reseed is NOT:** a separate pass that walks already-deserialized objects and **fabricates** events
from their populated state (a "for each building present, emit built"). That pseudo-emit feeds the cascade
reconstructed lies and trains the next agent to reconstruct more — it is banned
([superseded-ideas](../architecture/superseded-ideas.md)). There is no clean middle between it and the real
event-sourced read, so the read-driven reseed is built as its own step, never shimmed.

> **⚖ AI RE-EVALUATION IS A RESULT-PRODUCER TOO — IT RUNS ONCE THE GAME HAS LOADED, NEVER DURING THE SAVE READ
>.** *"The AI needs to be allowed to do work; the important part is to not have the AI do work during
> saveload."* A citizen assignment, a production choice, a re-scored plan are DECISIONS taken over base state —
> and while the stream is still arriving that state is incomplete, so the decision is paid for in full and then
> invalidated by the next fact.
> ⚖ **The CITIZEN ASSIGNMENT (workers + specialists) IS re-decided at load END** — the saved assignment
> was decided against the values the save's DLL computed, so `CvGame::onFinalInitialized` marks every city after
> the load-end rebuilds settle and the first post-load sweep re-runs it against the rebuilt state. Still only a
> MARK inside the load path; the work runs past the bracket like everything else here.
> ⚑ **The flag STAYS SET; only the WORK is suppressed.** Whoever marked the city still wants the work, so it is
> DEFERRED to the first sweep past `GAME_LOAD_FINISHED` — the identical suppress-then-resume shape grants take
> off this bracket, applied to the AI plane.
> ⛔ **Guard the WORK, not the mark.** A guard on each marking site is a rule every future caller must remember,
> and the marks arrive from whole-scope fans as well as direct calls; a guard at the one place the work is
> performed covers every path by construction. ⚠ It is NOT a licence to suppress the FACTS — the reseed's
> DOMAIN events are what build the state, and they are exactly what must keep flowing.
> ⚑ **The measured shape this closes:** the load fanned a full-empire citizen-reassignment mark once per
> empire-scope yield-modifier deposit, concentrated in the load's closing seconds — thousands of marks over
> state that no citizen had yet been placed against.

**The load lifecycle is bracketed by two spine events — `GAME_LOAD_STARTED` / `GAME_LOAD_FINISHED`.** Result-producers
(grants, and any future on-event side-effect machinery) rely **purely on the spine**: they see `LOAD_STARTED` →
suppress, `LOAD_FINISHED` → resume, so nothing is granted during reconstruction (a grant is a RESULT of a genuine
in-play acquisition, and a load is not an acquisition). The **cache-build consumer** is the load-active one — it
consumes the in-read events to build the cascade. New game builds the same way: its real init fires the same
events, with grants active because those are genuine acquisitions. Ledgered as
[the load reseed](#5-the-load-reseed).

> **⛔ `spineGameLoadInProgress()` IS RESULT-PRODUCER SUPPRESSION, AND AGENTS KEEP MISCONSTRUING IT (repeatedly, across sessions).** It answers ONE question: *would acting on this fact HAND SOMETHING OUT for a
> load, which is not an acquisition?* That is why the trigger/grant machinery consults it. ⛔ It is **NOT** a
> licence for a LOAD-ACTIVE consumer to skip work the reseed exists to perform — reaching for it there asserts
> that the load cannot be trusted to build the state, which is precisely the claim
> [the load reseed](#5-the-load-reseed) exists to falsify.
> ⚑ **THE MECHANISM OF THE MISTAKE IS COPYING, not reasoning** — the guard is read off an adjacent case in the
> same `switch` and carried into a new one, so it spreads without anyone deciding it. ⇒ **Never inherit one from
> a neighbouring case. Ask what YOUR handler does**, and answer the test below.
>
> **THE TEST — what does acting on this fact PRODUCE?**
>
> | the handler | verdict |
> |---|---|
> | hands an entity / payload over, or takes an AI DECISION | **guard it** — a load is not an acquisition, and the AI re-decides against state the save already carries |
> | BUILDS derived state (a context store, an enabler domain, a package) | ⛔ **no guard** — this is the reseed's whole job |
> | needs an object the stream has not delivered yet | ⛔ **not a guard — a BUFFER with a load-end DRAIN.** The two are not the same shape: a guard DROPS the fact, a buffer KEEPS it. Dropping a fact you needed is a permanent hole ([self-heal is not a backstop](../cascade/03-no-staleness-no-selfheal.md#-a-self-heal-is-the-fossil-of-a-missing-emit--so-it-is-a-search-not-just-a-ban)) |
>
> ⚠ **The over-correction is equally wrong, so three legitimate non-result-producer uses are named here rather
> than rediscovered:** the ORDERING BUFFER above (the city membership fold, the modifier's `plots` fan); the
> TWO-LEG FOLD whose play-time fan would otherwise double-count against the load build
> ([contexts.md](../cascade.md)); and reading the bracket as the **new-game-vs-load discriminator**,
> which suppresses nothing at all. ⛔ Do not sweep those out in the name of this rule.
> ⛔ **And a guard must never suppress an EMIT** — that is the separate, absolute ban (§6: emit every distinct
> fact, decide handling per consumer). A consumer that would double-apply on an in-read fact is a CONSUMER
> defect; silencing the fact hides it from every other consumer too.
> ⚑ **When a handler genuinely needs neither, say so where the next reader will look.** A comment stating why a
> guard is ABSENT is what stops the copy: the enabler's consumer already carries one, and the guards that grew
> underneath it are what that line was meant to prevent.

