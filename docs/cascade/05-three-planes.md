# ⚖ THE MAINTAINED SUM — THREE PLANES, ONE SLOT, AND NOTHING IS EVER RECOMPUTED

> Part of the **[cascade](../cascade.md)** spec.

Every slot is one identity, and reading it settles the whole maintenance question:

> **`slot` = Σ over the scope's LIVE sources `S`, over `S`'s compiled deposits `d`, of
> `value(d) × multiplier(S) × perScale(d) × [condition(d) holds]`**

**All four operands are ALREADY MAINTAINED BY AN EVENT.** `value(d)` is compiled at load (the deposit index);
`multiplier(S)` and `perScale(d)` are COUNTS the game objects and the context dictionaries (§ THE CONTEXTS,
below) hold; the condition verdict reads the contexts' own stored predicate state. Nothing on the right-hand side
arrives unannounced, so there is nothing left for a recompute to discover —
[a staleness flag is the fossil of a missing emit](03-no-staleness-no-selfheal.md#-a-staleness-flag-is-the-fossil-of-an-incomplete-emit-surface--the-same-rule-one-level-up)'s test applied to the VALUE plane rather than
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

⚑ **PLANE B IS WHAT THE DICTIONARIES BUY, AND IT IS WHY A COUNT FACT EXISTS AT ALL.** `Δ(v × c) = v × Δc`
is EXACT — `v` is a compiled constant and `Δc` is what the fact carries — so a `ContextDict::add(id, ±1)` IS a
yield delta of `Σ(deposits keyed on id) × ±1`. *"+1 food per river tile"* stops being a re-derivation and becomes
one multiply the moment a river bit moves. **This is the reason a population-changed fact is emitted**:
a `per: {POPULATION}` scaler is plane B, and the fact carries the delta that resolves it.

### ⛔ THE INVARIANT — the slot is correct at every instant, which is what makes plane C delta-able

> **At every instant `slot == Σ resolve(d, state_now)`, because every operand's move applies its own delta at the
> moment it moves.**

It is inductive, and it holds only if EVERY operand has a route — which is exactly what a saturated emit surface
buys. Four consequences:

- **A WITHDRAWAL IS ALWAYS EXACT.** `emit()` dispatches SYNCHRONOUSLY ([spine.md](../spine.md)),
  so no two operands are ever in flight together: when a fact arrives, every other operand still holds the value
  the stored contribution was computed against.
- **⚖ THE CONDITIONED TAIL IS THEREFORE DELTA'D TOO, PER ATOM — it is NOT re-resolved.** The earlier
  ruling that plane C could only re-resolve rested on *"`perScale` at deposit time is gone"*, and that is true
  only where a count can move WITHOUT announcing. Under plane B it always announces, so the state is never gone.
  ⛔ **B AND C ARE COUPLED — deliver both, or neither.** Delta-ing C while a count can still move unrouted
  reproduces precisely the drift the earlier ruling guarded against: the slot loses an amount it was never told
  about, and nothing re-derives it ([self-heal is not a backstop](03-no-staleness-no-selfheal.md#-a-self-heal-is-the-fossil-of-a-missing-emit--so-it-is-a-search-not-just-a-ban)).
- **ORDER-INDEPENDENCE SURVIVES, which is why LOAD is not a special case.** Source-then-count and
  count-then-source converge: the source applies `value × count_now` (0 if the count has not arrived yet), and
  the count applies `value × Δcount` for every deposit whose source is already live. A count route therefore
  tests the source's liveness at that owner — an O(1) `has()` — and applies for nobody else.
- ⚠ **THE HAZARD IS DOUBLE APPLICATION, NOT DRIFT.** One fact drives exactly ONE route class. Where a happening
  moves both a source and a count they are two distinct FACTS
  ([a fact names the happening](../spine/03-the-domain-emit-surface-every-fact/01-a-fact-names-the-happening.md#-a-fact-names-the-happening--something-changed-is-not-a-fact)), each applying its own — never one fact
  applying both.

- **⛔ NO PLANE HAS AN EVALUATION MOMENT TO DEFER, WHICH IS WHY NONE OF THEM CARRIES A STALENESS FLAG.** There is
  nothing to be stale ABOUT: every operand is compiled or maintained, so a slot is either current or was never
  told — and "never told" is a MISSING EMIT that must stay visible, not a state to schedule work against.
- **⚖ THE COMPLEXITY SHIFTS FROM O(WHAT EXISTS) TO O(WHAT CHANGED), AND THAT IS THE PERFORMANCE CASE.**
  A rebuild re-walks the scope's sources, so its cost scales with how much a city HAS; an application touches the
  moved source's own deposits, so it costs the same in a 900-building city as in a 3-building one — **the walks
  disappear rather than getting faster**. This is § THE CONTEXTS's payoff one plane up: there, storing
  a fact made cost track EVENT volume instead of READ volume; here, applying a fact makes it track event volume
  instead of SOURCE volume.
  ⚑ **It also makes a promise the specs already print come TRUE.** [validation.md](../specs/validation.md) states
  that *"the only path to a rebuild is a mark, so per-turn cost tracks what CHANGED — mark volume, which is event
  volume"* — which holds only if a mark is cheap. While a mark triggers a walk the real cost is
  `events × sources-at-scope`, i.e. the dominant term is the one the sentence omits. Under the maintained sum the
  sentence is literally true, which is [turn time is king](16-package-model.md#-the-per-scope-package-model--the-cascades-founding-design-1-stated-as-cache-architecture)
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
  never hand-written ([reverse lookups are populated once, at load](01-deposit-and-read.md#1-one-step-deposit-down-accumulate-read-o1)).
- **⚖ ORDER-INDEPENDENCE IS FREE, and it is what makes LOAD stop being a special case.** Addition commutes, so an
  accumulate needs no arrival order — exactly the property [spine.md](../spine.md) already
  demands of facts. The banked-marks bracket existed because *a rebuild mid-read evaluates against
  half-deserialized state*; an application of a compiled constant evaluates nothing, so it has no such hazard.
  **Only the CONDITIONED tail genuinely needs the `GAME_LOAD_STARTED`..`FINISHED` bracket**, because only it
  reads state the stream may not have delivered yet.
  ⚠ **Consumer registration order remains a contract for that half** (consumers dispatch in registration order):
  **contexts → enabler → modifier → triggers**. Anything that EVALUATES a condition registers after the contexts
  whose stores that condition reads.

### ⚖ WHY DELTA-DERIVING FAILED BEFORE — two preconditions, both now met

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
   [the uniform legacy-accumulator cut](03-no-staleness-no-selfheal.md#-the-legacy-accumulator-cut--every-accumulator-one-uniform-mechanism) describes. ⚑ So the old model was not
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

### ⛔ THE COST IS THE FORCING FUNCTION — a saturated emit surface is now STRUCTURAL, not a discipline

> *"We have to take that cost — the system will by its very definition collapse if we do not saturate with
> events."*

A maintained sum fails differently from a recomputed one, and the difference is the POINT:

| | a MISSED emit leaves | how it reads |
|---|---|---|
| recompute-on-mark | a stale but internally consistent value | **plausible forever** — nobody looks |
| **the maintained sum** | a phantom contribution nothing later clears, compounding on repetition | **loud, and louder over time** |

⚑ **That is [self-heal is not a backstop](03-no-staleness-no-selfheal.md#-a-self-heal-is-the-fossil-of-a-missing-emit--so-it-is-a-search-not-just-a-ban) carried to its conclusion rather than a weakness
accepted against it.** The rule already says a missed invalidation must surface as a live divergence instead of
being swept away; between two failure modes, the one that ANNOUNCES itself is the one the rule asks for. ⛔ So
this is never a licence to relax the emit surface "because the number self-corrects" — nothing self-corrects,
and that is deliberate.

⚑ **It also promotes the roadmap's ordering from a preference to a law.** *"The EMIT surface comes first; the
cache build is the step AFTER"* was sequencing advice under recompute; under a maintained sum an unsaturated
spine cannot produce a correct number **at all**, so completeness of the emit surface is a PRECONDITION of the
cascade being right rather than a quality target it trends toward.
⇒ Every ruling that pushes the emit surface toward exhaustive — *"add all the events, ever"*, *"too many events
is better than not enough"*, [an event gap is closed the moment it is found](../spine/03-the-domain-emit-surface-every-fact/01-a-fact-names-the-happening.md#-a-fact-names-the-happening--something-changed-is-not-a-fact) — is load-bearing
on this model, not enthusiasm.

⚠ **The bound on the damage, so the trade is stated honestly: a phantom lives at most ONE SESSION.** Nothing
derived is serialized, so LOAD rebuilds every slot from the reseed's own facts — the history pollution that makes
a legacy serialized accumulator unrecoverable ([the uniform legacy-accumulator cut](03-no-staleness-no-selfheal.md#-the-legacy-accumulator-cut--every-accumulator-one-uniform-mechanism))
cannot accrue here. What NAMES it inside that session is the THREE-LEG check — the logs, the JSON info and what
state expects ([http-endpoints.md](../specs/http-endpoints.md)).

### ⚖ AND IT IS THE EASIER CORRECTNESS PROBLEM — the deciding argument

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

⛔ **That last row is decisive, and it is already the spec's own rule** ([spine.md](../spine.md):
*"emit liberally, mark precisely"*). Over-inclusion is the technique that makes a completeness census tractable —
it is how the enabler's reverse index is allowed to be safe ([enabler.md §5](../specs/enabler.md): over-inclusion
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
- **The served surfaces are STORED-side only** (`/computed/*`, [http-endpoints.md](../specs/http-endpoints.md)):
  each serves what the events built, DECOMPOSED term by term (`CvCascadePackage::readValuesInto`,
  `EnablerKernel::operatingBuildings`, `CascadeCapabilities::storedUnion`, and the yield census), rendered in
  `Sources/Tools/CvStateEndpoints.cpp`, never in the server file.
- ⛔ **THERE IS NO RECOMPUTE-FROM-SOURCE TWIN BESIDE THEM, AND NONE COMES BACK**
  ([superseded-ideas #33](../architecture/superseded-ideas.md)): an endpoint cannot replay the event chain, so a from-source
  recompute served beside the stored value is not a second derivation of the same quantity — it answers a number
  that was never comparable, and diffing it produces confident nonsense at scale. **Correctness is the THREE-LEG
  check instead** ([http-endpoints.md](../specs/http-endpoints.md)).
  ⚑ Three rulings from that dead shape are kept, because they bind ANY future verification and not just the one
  that died: a check must be **INDEPENDENT** (one that consumes the stored values is partly built on the very
  state it exists to check, so a wrong input is silently inherited and the two sides quietly share a derivation
  again); its **COST IS IRRELEVANT** — *"correct is correct"* — since it is invoked deliberately and
  never on a turn path, so it is never trimmed, sampled or memoized to look cheap; and it **ANNOUNCES NOTHING**,
  emitting no `[CASCADE] rebuilt` line, because a verification must not move the numbers that describe real work.

