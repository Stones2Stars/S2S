# Decisions ledger — the canonical ID'd home for cross-cutting rulings

> ⛔ **Every `DEC-*` is a HARD RULE — binding by default, not advice to weigh.** The only thing that relaxes one is
> the owner explicitly saying so; a question you have posed to the owner is a HARD STOP until answered.
>
> **This is an INDEX, not a re-statement.** Each entry is one decision's *pure consequence* + a pointer to its
> authoritative home. **Before adding any cross-cutting ruling anywhere, grep this file first.** A doc that needs a
> ruling links `[DEC-id]`; it does not re-articulate it.

---

### DEC-fixedpoint-x100

×100 fixed-point is the engine's NATIVE representation for every **AMOUNT** — the cascade, the realized getters, and
the consumers all carry ×100. **The ×100 exists for ONE reason: so an amount can carry TWO DECIMALS at the edge
(owner).** JSON is human; human→×100 converts once at readJson (the IN boundary); ×100→human converts
only at the OUT boundary — any READER (UI / `/computed` HTTP fields / `Cy*` Python) does a trivial `÷100`.
**NO getter reduces, and there are NO discrete carve-outs — EVERY channel works the same way (owner ruling; this
uniformity is the core of the rework).**
⛔ **A PERCENTAGE IS NOT SCALED (owner): *"percentages should not have decimals."*** It is a whole number, so the
×100 buys nothing and costs a second identity constant (`100 + Σpercent` → `10000 + Σpercent`) at every combine.
This is **per-UNIT, not per-channel** — it applies identically everywhere, so the uniformity above is intact.
⛔ **readJson OWNS the scale and a CALCULATION never scales (owner):** *"you should not need to scale any value
inside any actual calculation — it is literally readJson's job to ensure it's scaled."* A `/100` or `×100` added
inside a calculation to make two operands agree is the defect, not the fix. A value that is physically a whole game count (angry citizens, a food
modifier) reduces at the POINT OF USE that consumes it as a whole number, never inside the getter that every other
consumer reads. **No getter has a ×100 "variant"** (never a `getX`+`getX100` pair); reducing at the getter forces that
split and lets the cascade be shoehorned into legacy-shaped getters — the half-migration reflex. **Blast radius never
limits the conversion** — the mapped consumer surface is the worklist, not a warning. **Home:**
[fixed-point-and-scales.md](../specs/curators/fixed-point-and-scales.md) (which also carries the CONVERT-BY-CLUSTER
method and the boundary-enumeration audit).

### DEC-curator-owns-descale

The curator absorbs all per-100 scaling once and emits uniformly human numbers; readJson has zero per-field scale
knowledge. **Home:** [fixed-point-and-scales.md §1](../specs/curators/fixed-point-and-scales.md).

### DEC-deliveryguy

A cross-entity modifier lives on whoever DELIVERS it, keyed by the target ("who brings this to the table?"); the
other entity is a referenced `enabled`/`requires` condition, never the home. **Home:** [modifier.md §4](../specs/modifier.md).

### DEC-cascade-bidirectional

The cascade is bidirectional — the enabler resolves `requires` by a `require` callback UP the scope chain (this is
the AND mechanism); never simplify to down-only. **Home:** [north-star.md §2](north-star.md).

### DEC-no-guessing

Never guess/infer/assume — at a gap, VERIFY against ground truth or ASK; a posed question is a HARD STOP. Binds
agents and minions. **Home:** [AGENTS.md](../../AGENTS.md).

### DEC-all-means-all

"ALL" means exhaustive: enumerate every item, recursing every aggregate to its leaf sources, never judgment-filtered;
prove completeness adversarially, not by self-assertion. **Home:** [AGENTS.md](../../AGENTS.md).

### DEC-maintenance-bookkeeping

Maintenance and inflation are separate bookkeeping channels computed outside the commerce chain, never folded into
gold-commerce; `maintenance` is its own family. **Home:** [economy.md](../reference/economy.md).

### DEC-calc-zero-ride-in

The CASCADE computes every value from JSON + game state with zero legacy-computed ride-in: engine-computed data is
never a cascade INPUT. The trap is the CAMOUFLAGED case — a DERIVED value
masquerading as raw state, above all a building's ACTIVE/DORMANT verdict, which is a pure function of
`requires.operate` and must be COMPUTED, never read from the engine. **Home:** [validation.md](../specs/validation.md).

### DEC-hard-typing-or-rollerskate

**Anything not enforced by HARD TYPING gets rollerskated (owner, learned the hard way).** A rule stated in a
doc or a comment constrains only an agent who reads it, believes it, and remembers it at the moment of writing
the code — so a design invariant that MATTERS is expressed as a TYPE that makes the wrong move fail to compile,
never as a convention. Prefer, in order: a type that cannot express the error · a missing verb (the banned
operation is unsayable, as `ContextDict` has no `set`) · a mechanical check (`Tools/verify-*.py`) · and only
last, prose. ⚑ The worked case: the city's SPECIALIST and BUILDING yield origins are separate PACKAGE TYPES
([state-repositories.md](state-repositories.md) § THE ORIGIN RULE) because prose saying "specialists do not live
in the building package" had to be re-corrected more times than the owner cares to count.
**Home:** [AGENTS.md](../../AGENTS.md) Conventions §Design.

### DEC-kraken

Skipping/assuming/guessing/shortcuts is the cardinal sin; maximal rigor by default until the owner explicitly relaxes
it. **Home:** [AGENTS.md](../../AGENTS.md).

### DEC-fast-is-slow-slow-is-fast

Read each subsystem doc in full before acting; skimming is never the faster path. **Home:** [AGENTS.md](../../AGENTS.md).

### DEC-map-before-delete

A legacy maintainer is validated to parity before it is cut — you cannot delete what you cannot fully observe.
(The shadow phase that enforced this has ended — [validation.md](../specs/validation.md); the observability bar
stands.) **Home:** [AGENTS.md](../../AGENTS.md).

### DEC-parity

The COMPLETENESS + ATTRIBUTION bar: every value's sources are fully attributed, with no tolerance band and no agent
grading of acceptability; a divergence is a data-collection gap (a missing source), never a formula difference to
tweak away. Parity/shadow as an ACTIVE validation phase is CLOSED ([DEC-verify-in-game-not-reshadow](#dec-verify-in-game-not-reshadow));
what survives is this completeness bar, now verified live via the endpoints. **Home:** [validation.md](../specs/validation.md).

### DEC-no-parity-results-in-docs

Parity-pass results (divergence counts, checklists, pilot numbers) stay out of the durable docs. **Home:** [validation.md](../specs/validation.md).

### DEC-tally-serializes-nothing

The modifier scope accumulators serialize nothing — rebuilt from loaded state. The **tally** serializes AND stores
nothing: it is a read-only accessor over the object-owned counts rolled up the spine — no duplicate store, no seed,
no shadow. **Home:** [tally.md](../specs/tally.md).

### DEC-save-remove-is-soft

Removing a serialized field is soft via `Assets/savemigration.txt`: FULL-DELETE the member + read + write and NAME
the tag there — the save reader (`CvTaggedSaveFormatWrapper::sm_isCut`) drains the orphan tag transparently at load,
so **no `WRAPPER_SKIP_ELEMENT`** (a lingering skip still names the dead member — a rollerskate target) and **no
save-break-flush** (save-breaking is obsolete; the old two-stage model is retired). The one hard case: an UNLISTED
deleted-read orphan desyncs the whole downstream read. **Home:** [save.md](../specs/save.md).

### DEC-no-float-in-sync

**No FLOAT where it can reach SYNCHRONIZED state (owner):** *"using float in any calc that is used in any kind of
multiplayer scenario sounds like a gigantic no."* Civ4 is deterministic lockstep, CPU-dependent float differs in
the last bits, and truncating that to an int turns it into a different answer — an OOS. ⚖ **The discriminator is
synchronized state, not "gameplay"** (owner: *"gameplay path does not always mean multiplayer"*): a STATE
MUTATION or a DECISION every client computes is banned — **an AI decision counts**, because the AI runs on all
clients — while a value that dies at the screen (symbol offsets, animation times, the `*Float` combat reads
behind the odds display) is fine. ⚑ The conversion shape: a curve that factorizes into terms each depending on
ONE input becomes compile-time integer tables in ×100xx fixed point, multiplied in `int64_t` and reduced once.
⚠ Acceptance is the ORDERING, never bit-equality with the float version — that version had no well-defined answer
across clients, so it is not a baseline. **Home:**
[engine.md § NO FLOAT WHERE IT CAN REACH SYNCHRONIZED STATE](../reference/engine.md).

### DEC-synced-rng-is-shared-state

⛔ **Do not touch the synchronized RNG's draws (owner).** `CvGame`'s `m_sorenRand` seed is SERIALIZED into the save
and advances in lockstep on every client, so the NUMBER of values drawn, their ORDER, and whether a draw happens
at all are shared game state — adding, removing, reordering or short-circuiting a `getSorenRandNum` desyncs
multiplayer and makes a save stop replaying. (`GC.getASyncRand()` is the unserialized UI/cosmetic stream and must
never decide a gameplay outcome; `m_mapRand` is world generation.) ⚑ "It draws from `SorenRand`" is therefore a
LIVE NAMED REASON to leave a body's shape alone — one of the few [superseded-ideas](superseded-ideas.md) #22
accepts in place of the dead mirror-the-legacy argument. **⛔ And the RNG is NOT DATA: no JSON authors a seed,
stream or draw, and neither the cascade nor the curator owns any part of it.** What JSON authors is the ODDS — the
number the engine's roll compares against; the roll itself is engine mechanism.
**Home:** [engine.md § THE SYNCHRONIZED RNG](../reference/engine.md).

### DEC-derived-never-trusted

Derived data is never trusted from a save — a derived store starts EMPTY on load and is rebuilt by the reseed's own facts. **Home:** [save.md](../specs/save.md).

### DEC-obs-scale

The Observability Scale (0 Oblivious … 5 Meta) + the "Orwell" bar: rebuild game state from endpoints + `/events` +
gated logs, never the screen. **Home:** [observability.md](../reference/observability.md).

### DEC-obs-hook-shapes

Three canonical observability hook shapes: a snapshot field, a gated `[TAG]` log tee, a mailbox snapshot
endpoint. **Home:** [observability.md](../reference/observability.md).

### DEC-interface-contracts

Depend on interfaces, not concretions — a C++03 interface is an abstract base with pure-virtuals and no data, MI is
the `implements` axis, wiring is poor-man's-DI at a composition root, grafted onto DLL-derived classes never EXE-bound
bases. **Home:** [patterns.md](patterns.md).

### DEC-proper-once

Build the proper structure once; reject transitional shims that only defer the real design. **Home:** [AGENTS.md](../../AGENTS.md).

### DEC-keep-unkilled-ideas

Retire a doc only if it is reconstructible-from-code-and-unneeded or an explicitly killed idea; un-killed forward
intent is kept. **Home:** [AGENTS.md](../../AGENTS.md).

### DEC-docs-current-truth

Docs state CURRENT TRUTH only — no dated rulings, no supersession trails, no session logs, no parity numbers.
Outdated content is DELETED, not annotated; git history is the archaeology; superseded-ideas.md is the only
tombstone registry; status chronicles live in `docs/plans/`. **Home:** [AGENTS.md](../../AGENTS.md) Conventions §Docs.

### DEC-spec-plus-todo

A doc is a SPEC (the timeless design) or a TODO (a short bulleted list of what is NOT done) — never both, and
never status woven through prose. Status claims drift by nature, so a status-heavy doc rots and misleads with
authority; no `LANDED`/`✅ DONE`/completion ledger/build-status table is written into a doc. A finished item is
DELETED from the todo and anything durable it established moves into the spec; the todo measures what is LEFT,
git history records what was done. Verify any status claim against the tree before acting on it, and prefer
deleting it to updating it. The progress-facing half of
[DEC-docs-current-truth](#dec-docs-current-truth). **Home:** [AGENTS.md](../../AGENTS.md) Conventions §Docs.

### DEC-WF-rulings-to-repo

Every owner ruling → the right repo doc immediately and unprompted, in the same work item; memory-only is unfinished.
**Home:** [AGENTS.md](../../AGENTS.md).

### DEC-WF-surface-sprawl

When a change sprawls or the target structure is undefined, STOP and surface it to the owner; do not overcompensate
with more partial fixes. **Home:** [AGENTS.md](../../AGENTS.md).

### DEC-WF-branch-safety

Never switch branches while the owner may be mid-build (the game is live) — it silently strips your changes from
their build; verify the current branch immediately before every commit. **Home:** [AGENTS.md](../../AGENTS.md).

### DEC-represent-dont-fit

A dry-calc divergence means the cascade is MISSING a mechanic — trace it to its named engine source and represent it;
never skip/drop/invent a mechanic to fit the data. **Home:** [validation.md](../specs/validation.md).

### DEC-baseline-is-a-smell-test

A remembered figure ("hammers were ~5000") is a SMELL TEST that says go look, never an acceptance target; the only
question is whether a read is correct, and where the number lands is an output. Bending the implementation toward
one -- or hesitating over a correct fix because it would overshoot -- is the banned move. **Home:**
[validation.md](../specs/validation.md).

### DEC-per-mechanic-parity

Parity is verified mechanic-by-mechanic against the engine's per-mechanic value, never by comparing or averaging
aggregate outputs. **Home:** [validation.md](../specs/validation.md).

### DEC-structure-before-shadow

Stand up the proper, spec-faithful cascade STRUCTURE first; a per-change in-game shadow can FALSELY confirm a wrong
structure. LOAD verifies the static + initial setup; END TURN verifies only LIVE integration. Structure is gated by
spec-fidelity, never by a green shadow. **Home:** [validation.md](../specs/validation.md).

### DEC-conditions-are-predicates

A deposit's condition is expressed as a **PREDICATE** in `enabled`/`disabled`/`requires` (the predicate registry is
EXTENSIBLE — define new predicates freely); it is NEVER encoded as a bespoke sub-scope MEMBER. Adding a predicate
*extends* the model; a condition-carrying member *changes the core structure*. **Exception:** golden age's YIELD
EFFECT (`empire.goldenAge`) stays an engine member-mirror PERMANENTLY (owner-ruled engine-core, not data-defined) —
the effect is a plot base-yield-threshold additive the XML/JSON never modeled. NARROW: only the yield effect is
carved out; golden-age LENGTH + grant ARE curated JSON (`goldenAge.empire.percent`, `grants.goldenAge`).
**Home:** [modifier.md §3](../specs/modifier.md), [json.md §3.5](../specs/json.md).

### DEC-single-implementation

Every cascade calculation/evaluation exists **exactly once**, as a pure static function exposed on a shared surface —
a purely-organizational static-methods class (never a namespace: VC7.1/Boost/EXE-ABI name-mangling risk; never
file-`static`-hidden: the next consumer reimplements it). ONE evaluator (`cascadeEvalCondition`) evaluates all
conditions/predicates. No duplication is sanctioned — the shadow phase, which once sanctioned the cascade running
alongside legacy, has ended.
**Home:** [patterns.md § DRY](patterns.md).

### DEC-data-first

Data migration (curators + JSON) is NEVER deferred: any known un-migrated field / reclassification / still-emitted
legacy shape is the #1 priority, handled BEFORE any downstream cascade / shadow / observability / parity work. The
complement of [DEC-no-deferred](#dec-no-deferred) — data is never deferred, and neither is anything else. **Home:**
[validation.md](../specs/validation.md).

### DEC-recurate-on-decision

Any ruling that changes what the data model carries (a new grantor kind, a re-homed field, a widened block) triggers
the curator update + regen IN THE SAME work item — never "the curator catches up later." The per-decision twin of
[DEC-data-first](#dec-data-first). **Home:** [AGENTS.md](../../AGENTS.md) Conventions.

### DEC-turn-time-is-king

Turn time is the objective every performance decision optimizes; load time is the currency that pays for it
("there is only 1 game load, but many many many turns"). **Home:** [state-repositories.md](state-repositories.md).

### DEC-unit-modifiers-on-top

A modifier that TRAVELS with a unit (unit-sourced happiness, anger, property emission, any unit-carried channel
value) is NEVER part of a cached cascade computation: computed LIVE at read, added ON TOP as a FLAT term, after and
outside every percentage modification. Unit movement therefore never dirties ANY cache — including the legacy ones.
**Home:** [modifier.md](../specs/modifier.md) §2b.

### DEC-entity-gate

A whole-entity game-option gate authors as the ENTITY-LEVEL `enabled`/`disabled` condition pair (`"enabled":
"GAMEOPTION_X"`), evaluated live — never a bespoke section and never smuggled into `requires` (which holds only
genuine needs). **Home:** [json.md §2](../specs/json.md) (the Applicability row) + [enabler.md §7](../specs/enabler.md).

### DEC-no-spec-restating-comments

A CODE comment never restates a spec ruling, and one that CONTRADICTS the spec is ROLLERSKATING LICENSE: the
spec states a rule once for the whole engine, a call-site copy DRIFTS, and a drifted copy authorizes the next
agent to act against the spec while believing they conform — strictly worse than no comment. The worked class
is SCALE annotation (*"this is ×100"*), which [DEC-fixedpoint-x100](#dec-fixedpoint-x100) already answers
universally; delete one you pass. A comment may carry what the spec cannot know (why THIS site is an edge),
never the rule. The CODE application of [DEC-docs-current-truth](#dec-docs-current-truth). **Home:**
[AGENTS.md](../../AGENTS.md) Conventions §Docs.

### DEC-no-rollerskate-evidence

Leave NO evidence of a previous rollerskate — dead / commented-out old code, superseded dual surfaces, transitional
shims, and `renamed from X` / `was Y` / `(formerly …)` trails are all REMOVED, in CODE as well as docs. **This
includes a comment that NARRATES a deletion** (`// m_iX removed`, `// X is cut`, `// … no longer …`, `// was m_iX`):
naming the dead member is itself the bait — the next agent reads the name and re-treads the very thing you killed
("comments about dead things just lead to rollerskating about the same dead things", owner). Keep the forward,
current-behavior statement; strip the dead name. Code and docs read as if built right the first time. The rule is
load-bearing, not tidiness: leftover evidence of the abandoned path is exactly what the next agent finds and
rollerskates off — it caused much of the drift this project is digging out of. The delete-don't-annotate half of [DEC-docs-current-truth](#dec-docs-current-truth) extended to code;
strengthens [DEC-proper-once](#dec-proper-once). **Home:** [AGENTS.md](../../AGENTS.md) Conventions.

### DEC-no-deferred

Anything marked deferred / parked / not-yet-landed / blocked / "later" / "acceptable for now" / TODO /
pending is a FAILURE to fix, not a backlog item — the word agents hide behind to skip hard work hoping it lacks
impact. The general form of [DEC-data-first](#dec-data-first) (which bans it for data specifically), now extended to
ALL work. The only exceptions are owner-ruled PERMANENT design carve-outs, recorded as such (e.g. the golden-age
yield-effect member-mirror; Python-authoritative gameplay staying Python). **Home:** [AGENTS.md](../../AGENTS.md) Conventions.

### DEC-universal-yield

ANY number modified by game mechanics is a yield — base yields, commerce, free XP, free specialists, properties, and
any other — carried by ONE machine in ONE uniform package format (Σflat / Σpercent per channel per scope). A number
computed by a legacy ad-hoc path OUTSIDE the machine is a shortcut/failure — the COMPUTE, not only the storage. The **OUTPUT-SEAM**: where the engine does
placement/application (free-specialist assignment; the golden-age plot-base-yield-threshold "+1"), the cascade owns
the authored INPUTS + the OUTPUT yields — both live in the machine — and ONLY the middle mechanism is engine-owned.
**Home:** [modifier.md](../specs/modifier.md).

### DEC-done-is-observable

Done = the effect is observable in the RUNNING GAME via an endpoint poll — never "the code path exists" or "the data
loads." "Straight up missing" means it does not show in-game even if it loads (the break is downstream, in
apply/display). Every work item's acceptance is an endpoint-observable pass/fail on a real save, a real turn — the
strict complement of [DEC-verify-in-game-not-reshadow](#dec-verify-in-game-not-reshadow). Programmatic already: the
four `/computed` DECOMPOSITION CENSUSES serve what the events built, term by term, as game-thread snapshots (a
blind value is EMITTED first, step one of its fix). ⛔ They are STORED-side and there is no `oracle` beside them
([superseded-ideas #33](superseded-ideas.md)).
⚖ **AND IT IS A SNAPSHOT, NEVER A PROPERTY — AN EVALUATION PATH IS NEVER "DONE" (owner): *"I don't think any
evaluation path can ever be called done."*** The reason is structural rather than cautious: the classification
registries and the modifier families are OPEN BY DESIGN
([DEC-classification-infos](#dec-classification-infos)), so a valuation that reads every source today becomes
incomplete the moment data authors a new one — with no code change, nothing failing, and no build that could
name it. Completeness DECAYS on a data edit.
⇒ **So the deliverable is the INSTRUMENT, not the claim**: the load-time censuses (`unkinded-member`, the FK
and unconsumed-key counts), attribution to a named source with numbers, and the three-leg check
([http-endpoints.md](../specs/http-endpoints.md)) keep working as the data moves; a completion statement does
not. ⛔ Report an evaluation path as *"no known divergence, on this save, on this turn"* — never as done, which
asserts a property the model cannot have. It is the same reason a remembered figure is a smell test rather than
a target ([DEC-baseline-is-a-smell-test](#dec-baseline-is-a-smell-test)).
**Home:** [validation.md](../specs/validation.md).

### DEC-cy-not-fixed

The `Cy*` info-binding contract (the boost::python `.def` surface) is NOT a fixed contract to preserve; freezing it
forced the JSON pocos to mirror the entire legacy `CvXInfo` field contract (a stub per legacy field). Redesign the
boundary around the cascade/JSON model + rewire the Python info-CONSUMERS; fix the stub-fed wrong values. DISTINCT
from the computed-getter flip strategy (which keeps those contracts and rewires bodies, not call sites). Python
gameplay stays Python. ⛔ **NOTHING GATES THE DISCONNECT** — a dead legacy binding is an OUTLAW, shot on sight,
and the replacement library's COMPLETENESS is its END STATE, never a permission slip to cut; reading it as a gate
inverts the ruling into a shield for the surface being removed.
**Home:** [roadmap.md](../plans/structural-cleanup/roadmap.md).

### DEC-new-getter-surface

**REUSING A LEGACY GETTER IS THE MECHANISM THAT PRODUCES THE HALF-MIGRATED STATE** (owner) — not a shortcut that
merely risks one. A legacy getter's contract encodes legacy assumptions (its scale, its granularity, its combine,
its one-channel shape), so pointing the cascade at it forces the CASCADE to bend to that shape; the result is a
surface that is half cascade and half legacy and reads as nearly done. Every channel-shaped getter on
`CvCity`/`CvPlayer` is one such contract. Therefore: **build a NEW uniform, parameterized getter set over
the channel index, move consumers onto it, and DISCONNECT the old set** — never re-body a legacy getter, never
keep both surfaces live, and never widen a legacy getter to fit. Python is rewired onto the same uniform set
(the [DEC-cy-not-fixed](#dec-cy-not-fixed) ban generalized from the `Cy*` bindings to the whole getter surface).
This SUPERSEDES the computed-getter-flip strategy ("rewire the body, never the call sites"), which was correct
only while the cascade had no uniform vocabulary to rewire consumers ONTO. The general form of
[DEC-fixedpoint-x100](#dec-fixedpoint-x100)'s "reducing at the getter lets the cascade be shoehorned into
legacy-shaped getters — the half-migration reflex". **Home:** [roadmap.md](../plans/structural-cleanup/roadmap.md).

### DEC-no-self-heal

Self-heal is NOT a backstop the cascade keeps. No blanket per-turn/per-slice rebuild (`playerSliceRebuild`, the epoch
bump, the turn-roll self-heal) papers over a missed invalidation — those blankets are REMOVED, not graded as
"acceptable interims." Correctness comes ONLY from complete, targeted, spine-routed per-source-mask invalidation; a
missed invalidation must surface as a live divergence, never be silently rebuilt away. ⚑ **A self-heal is the
FOSSIL of a missing emit** — it appears because a fact was not announced and recomputing was the cheapest way to
stop the symptom — so finding one is a SEARCH: wire the un-announced fact, and the recalc falls out as a
consequence rather than being deleted as the fix. Sharpens the CAPSTONE RULE (LOAD is the only full pass).
**Home:** [state-repositories.md](state-repositories.md).

### DEC-uniform-cache-shape

Every derived cache on the cascade plane is the **SAME OBJECT TYPE** everywhere and they **ALL MAINTAIN THE SAME
WAY** — one templated channel-indexed slot table on every owner, driven by ONE application derived from the
deposit index; only WHICH SLOTS carry a value varies by scope. A hand-named scalar field is therefore a DEFECT,
not untidiness: it cannot be addressed uniformly, so it forces a bespoke maintenance path per field. ⛔ A
**CROSS-SCOPE RECEIVER TOTAL IS NOT ONE OF THESE SLOTS AND IS NOT STORED AT ALL** (owner): a scope consuming a
channel from BELOW it — the empire's gold/research/culture/espionage over its cities — SUMS ITS MEMBERS' REALIZED
VALUES AT THE READ, because a member's realized value is the §2a combine and is not linear in the deposits, so a
stored total could not be moved by a deposit delta and would cost more to keep than to re-sum. The member count
is therefore the honest cost of that read, and the defect is only ever asking it at the wrong CADENCE
([patterns.md](patterns.md) § THE VALUATION PROTOCOL). ⚠ A scope combining its OWN packages (a city's production
rate) is not this — that is an ordinary package read. **Home:** [state-repositories.md](state-repositories.md).

### DEC-flag-is-fossil

**A STALENESS FLAG is a CLAIM THAT WE DO NOT KNOW WHAT CHANGED, and a complete emit surface falsifies that claim
by construction.** Every staleness bit, epoch counter and version number is therefore exactly one of two things —
a **MISSING EMIT wearing a flag** (wire the fact, [DEC-close-event-gaps-now](#dec-close-event-gaps-now)) or
**DEAD WEIGHT** (delete it) — never a third thing kept because it works. **Home:**
[state-repositories.md](state-repositories.md) § A STALENESS FLAG IS THE FOSSIL OF AN INCOMPLETE EMIT SURFACE.

### DEC-no-staleness-vocabulary

**"DIRTY" IS NOT A TERM WE USE, FULL STOP (owner)** — removed WITH the mechanism it names. The only survivor is
`InterfaceDirtyBits` (the EXE-bound GRAPHICS repaint vocabulary, published by name from BUG config strings, so it
is not ours to retire). Every DERIVED-STATE use goes, blast radius included. **Home:**
[state-repositories.md](state-repositories.md) § A STALENESS FLAG IS THE FOSSIL OF AN INCOMPLETE EMIT SURFACE.

### DEC-contexts-are-never-marked

**A CONTEXT IS NEVER MARKED, RE-DERIVED OR REFRESHED — "we do not dirty contexts" (owner).** No staleness
mechanism of any kind; the FACT sets the bit/count it names, and that is the entire maintenance path — the same
error one plane over from [DEC-maintained-sum](#dec-maintained-sum). **Home:** [contexts.md](contexts.md) §
Maintained EVENT-DRIVEN.

### DEC-facts-name-happenings

**A DOMAIN fact must name WHAT HAPPENED, not that some state moved — *"an event that is not specific relies on
actual calculation to happen"* (owner).** A `*_CHANGED` event carrying only a direction or a delta hands the
consumer a question instead of an answer, and the only way to answer a question is to CALCULATE, so the
per-consumer calculation the spine exists to delete reappears everywhere at once. It is
[DEC-flag-is-fossil](#dec-flag-is-fossil) on the EMIT side: a staleness flag says *"something in this
bucket moved"*, a non-specific event says *"something about this entity moved"*, and both discard the identity of
the happening. ⇒ **Where several distinct happenings reach one choke point they are SEVERAL FACTS, never one
fact with a discriminator field** — a payload int a consumer must branch on is the calculation relocated into a
`switch`. ⛔ Splitting one event into its happenings is NOT the banned duplicate (that is ONE happening announced
twice; this is several announced as one). ⚑ The UNIT plane is the in-tree exemplar to converge on
(`UNIT_CREATED` / `UNIT_KILLED` / `UNIT_ENTERED_CITY` / `UNIT_LEFT_CITY`), and `SEVT_CITY_FOUNDED` already
records the argument: a constellation of `*_CHANGED` side-effects could not substitute for the named happening.
⚠ NOT reopened by it: a scalar fact carrying its new value, a boolean verdict crossing carrying the verdict,
or the count-GRANULARITY ruling (a different axis). ⛔ A SLOT REPLACEMENT **is** reopened where the slot holds
a SOURCE: the plot substrate splits into `ADDED`/`REMOVED`, because each end is its own consumer work and a
withdrawal must be announced while the old state still holds ([event-spine.md](../specs/event-spine.md)). **Home:**
[event-spine.md](../specs/event-spine.md) § A FACT NAMES THE HAPPENING.

### DEC-contextdict-replaces-derivedcache

**`CvDerivedCache` is replaced by `ContextDict` (or a channel-indexed package) EVERYWHERE — it no longer exists
in `Sources/`.** A recompute is needed only when inputs arrive UNANNOUNCED, which a saturated emit surface makes
impossible, so the component has no niche left; a surviving tenant would be a MISSING EMIT wearing a component
([DEC-flag-is-fossil](#dec-flag-is-fossil) one level out). ⚠ Bounded by what the slot HOLDS: a summed magnitude
is a package channel, not a dictionary. **Home:** [state-repositories.md](state-repositories.md) §
`CvDerivedCache` IS REPLACED BY `ContextDict`.

### DEC-dict-is-a-consumer

**A context dictionary IS a spine consumer that declares exactly the facts it looks for (owner)** —
self-registering, never fed by a central switch, so a missing route is visible AT the dictionary. Registration
ORDER stays a contract (`contexts → enabler → modifier → triggers`). Does not weaken
[DEC-enabler-not-cascade](#dec-enabler-not-cascade)'s one-consumer-per-SYSTEM rule — several dictionaries inside
one machine's band is still one system. **Home:** [contexts.md](contexts.md) § THE DICTIONARY IS THE FINAL
STOPPING PLACE.

### DEC-keyed-accumulator

**EVERY derived store in the engine is ONE shape — a KEYED ACCUMULATOR maintained by a DELTA from the fact that
names the grantor/source — because a count IS a sum.** The possession dictionaries (the plot group's bonuses,
`CityContext.amenities`, its vicinity tiers, `EmpireContext.policies`, the enabler's membership planes,
`providedCount`) and the magnitude packages differ only in key space and value type; `ContextDict` and
`CvCascadePackage` are the same structure. So [DEC-maintained-sum](#dec-maintained-sum) is the MAGNITUDE case of
this rule, and [DEC-uniform-cache-shape](#dec-uniform-cache-shape)'s *"on the cascade plane"* scope is too
narrow — the plane boundary is not real. **THE SEMIBOOLEAN CONTRACT:** stored `id → count` (an int), read
`has(id)` ≡ `count > 0`, written ±1 per grantor participation change (never set, never clear, **never a
recount**), and ZEROED at owner reset — a delta store is correct only from a known zero, and a recycled
`FFreeListTrashArray` slot inherits counts no later delta can fix. ⛔ **ALWAYS a count, NEVER a bit**, and the
deciding argument is not multi-grantor cases but that **you can never safely answer no**: the registries are
OPEN ([DEC-classification-infos](#dec-classification-infos)), so a one-grantor key gains a second when data is
AUTHORED, with no engine change — a bitset breaks silently on a data edit. ⚑ Volumetric then needs no reshape and
removal-wins is structurally absent. ⚠ A set-shaped store survives only while something RECOMPUTES it whole, so
storage and maintenance convert together or not at all. **Home:**
[state-repositories.md](state-repositories.md) § EVERY DERIVED STORE IS ONE SHAPE.

### DEC-spatial-is-cached

**A SPATIAL result is the ONE carve-out from the maintained sum, and its cache is WANTED (owner):** *"we should
have some pathfinding cache, because it is the most expensive, and at the same time unmaintainable thing we can
do — it has to scan plots by its very definition."* A path is not a Σ over sources, so there is no delta to
apply, and it moves NON-LOCALLY (one terrain or route change re-routes paths that never touch the changed plot),
so no fact can name what it invalidated. ⛔ Deleting a pathfinding / culture-distance / propagator cache as
"a cache is a defect" is therefore a REGRESSION, not cleanup. ⚠ Scoped: it does not cover an ordinary derived
value that merely feels expensive, does not license a read-side `ensure()`, and does not excuse invalidation —
a spatial cache is CLEARED wholesale by the events that can move it. **Home:**
[state-repositories.md](state-repositories.md) § THE SPATIAL CARVE-OUT.

### DEC-maintained-sum

A cascade package is a **MAINTAINED SUM, never a marked-and-recalculated cache**: the DOMAIN fact names the
SOURCE, the compiled deposit index names its deposits, and applying them IS the maintenance — correct the instant
the fact arrives, nothing marked, deferred or batched (RETIRED: the staleness-flag/mask/mark-then-rebuild
protocol, [superseded-ideas](superseded-ideas.md) #30). Three routed planes land in the SAME slot — **A
CONSTANT** (`±value`), **B SCALED** (`±value × Δcount`, the context dictionaries' half), **C CONDITIONED**
(`±value` on a verdict crossing) — coupled so the slot equals `Σ resolve(d, state_now)` at every instant.
PULL-not-push still binds CROSS-SCOPE. **Home:** [state-repositories.md](state-repositories.md) § THE
MAINTAINED SUM.

### DEC-spine-reseed

On load, the cascade is built from events that come from **inside the save read itself** — each slot deserializes
into a local and is handed to that slot's **INTERNAL SETTER** (commit + maintain + announce, no effect fan-out),
which fires the same DOMAIN fact play does (`CvGame::read` → `CvPlayer`/`CvCity`/`CvPlot::read`). ⛔ **The CRUD is
not the event; what happened is** — the stream is authoritative for base state and the fact is testimony in the
past tense. The earlier north-star *"the event SETTING the state (read → emit → populate)"* is RETIRED: it made an
effect the thing that mutates base state, and a read that writes members raw puts the emit and the derived-state
maintenance in a second place that drifts (five `CvPlot` slots announced nothing, and a whole-object recompute
stood in for the maintenance). ⚠ Such a recompute may never survive beside the setters — both apply, and an
XOR-maintained value cancels to zero. A `SAVELOAD`-kind line may report what the stream CONTAINED, consumed by
logging alone; it builds no state.
It is NOT a separate post-deserialization pass that fabricates events by walking already-populated objects — that
pseudo-emit is banned ([superseded-ideas](superseded-ideas.md) #13) — and equally NOT a warm-up "seed" that walks
has-lists into a consumer's cache beside the event stream (an invented second build mechanism that leaves the
consumer deaf to the reseed and defeats the missed-emit tripwire). The load lifecycle is bracketed by
`GAME_LOAD_STARTED` / `GAME_LOAD_FINISHED` spine events; result-producers (grants) rely PURELY on the spine and
suppress between them (a grant is a RESULT of a genuine in-play acquisition; a load is not one), while the
cache-build consumer stays load-active. New game builds the same way (real init fires the same events, grants
active).
**Home:** [event-spine.md](../specs/event-spine.md).

### DEC-close-event-gaps-now

An event gap is CLOSED THE MOMENT IT IS FOUND — never recorded as a todo and left. Binds all three forms: a
missing EMIT, a missing FIELD on an existing fact (the old-value case), and a missing CONSUMER ROUTE (the fact
fires and the store that needs it ignores it). All three leave a stored value permanently wrong with nothing to
re-derive it ([DEC-no-self-heal](#dec-no-self-heal)), so the todo entry naming the hook IS the duration of the
bug. Closing costs almost nothing while the trace is in hand and never gets cheaper. It does not license
guessing a structure — surface a genuine design question — but the gap still closes in the same work item.
Sharpens the emit-liberally ruling. **Home:** [event-spine.md](../specs/event-spine.md).

### DEC-verify-in-game-not-reshadow

Parity + shadow are CLOSED — their job is FINISHED and they are NOT to be re-run, re-invoked, or used to frame any
remaining work. The confirmation already exists and is sufficient: the event-spine STRUCTURE was strongly verified
and the shadow strongly verified the CALCULATIONS reach the right numbers — the design is proven
achievable. The original `readJson`-direct shadow read JSON STRAIGHT, bypassing the loaded `CvJson<X>Info` objects;
that bypass was itself a rollerskate and must NOT be repeated. ⛔ Do not re-shadow and do not re-frame work as
"parity"/"shadow": that framing sends agents rollerskating back into offline validation instead of building the real
info-object-backed runtime. Remaining verification is the LIVE game ONLY — manifestation via the endpoints
([DEC-done-is-observable](#dec-done-is-observable)) and turn time
([DEC-turn-time-is-king](#dec-turn-time-is-king)).
**Home:** [validation.md](../specs/validation.md).

### DEC-no-legacy-masking

Legacy outputs must FAIL LOUD, never be preserved or snuck in via getters/fallbacks. A realized getter reads the
CASCADE ONLY — no `*Legacy` fallback, no pre-init/what-if legacy path; a cascade gap returns a wrong/empty value
(exposed), never a legacy-correct one (masked). Legacy masking a wrong cascade is WORSE than legacy failing: the mask
hides the defect and defers the fix (the wellbeing panel reading legacy hid a 2× cascade inflation). Purge legacy
**violently** so what is missing/wrong is immediately visible. Blast radius is never a reason to keep a legacy path
alive. **The legacy XML is REMOVED (the red ratchet), so a legacy fallback cannot even RUN — it is BAIT that substitutes
a nonexistent answer and masks the hole** ([DEC-red-ratchet](#dec-red-ratchet)); a realized gate/getter is therefore
a PURE cascade read (the six availability gates carry no `*Legacy` fallback, no pre-init guard, no what-if path).
Corollary of [DEC-playability-not-a-gate](#dec-playability-not-a-gate) for the READ surface. **Home:** [validation.md](../specs/validation.md).

### DEC-legacy-decache-poisons-perf

The #430 cut NUKED the serialized accumulators legacy calcs depended on for O(1) reads (`m_iBuildingGoodHappiness` &
its cluster, …). Stripped of those caches, a surviving legacy calc (`happyLevelLegacy`, `badHealthLegacy`, …)
recomputes from scratch on EVERY call — so ANY perf measurement taken while legacy still runs in a read path measures
**legacy's decache penalty, not the cascade** (proven: the unit-selection lag was legacy `unhappyLevel(iExtra)`/
`badHealth(bNoAngry)` what-if re-sums per read; it vanished the instant the getters went cascade-only). All turn-time/
FPS/lag numbers gathered with legacy on any hot read path are POISONED. Clean perf is only measurable AFTER legacy is
fully purged — so the violent purge is a PREREQUISITE for the perf hunt, not merely a correctness/tidiness step.
Sharpens [DEC-turn-time-is-king](#dec-turn-time-is-king).
⛔ **AND IT DOES NOT ONLY POISON MEASUREMENT — IT CONVERTS AN AI LOOP INTO A HANG (owner): the AI loops "looping
all the things when they don't need to" are a SYMPTOM, and they surface now "because we do not serialize their
caches anymore."** The loops were always shaped this way; every inner read used to hit a serialized accumulator
and cost O(1), so the shape was merely wasteful. Strip the accumulators and each read RECOMPUTES, so an
`O(candidates × cities)` loop becomes `O(candidates × cities × cities)` and stalls outright.
⇒ **Both halves are the fix, and neither alone is:** the READ must be an O(1) maintained slot again
([DEC-maintained-sum](#dec-maintained-sum)), AND the caller must stop asking a scope-wide question per
candidate. ⚑ **Expect MANY** — three surfaced in one session from one root (`AI_isFinancialTrouble` re-walking
every city, `readFlat` doing a tree lookup, `cityReceiverRate` re-walking the plot ring), each found only by
attaching a debugger to a spinning process, because a spin EMITS NOTHING and every log goes silent at once.
⚠ So a hang with a saturated core and dead logs is this class until proven otherwise — and the CPU reading is
per-core, so one pinned core reads as ~0% in Task Manager on a many-core box.
⚖ **AND THE UNCACHED STATE IS AN INSTRUMENT, NOT ONLY A COST (owner): *"it is useful to run through like this
without caching to see where the hottest path is."*** This is the half that inverts the entry above. Behind a
serialized accumulator an `O(n³)` loop is INVISIBLE — it merely costs a slice of every turn forever, and
nothing ever points at it. Strip the accumulator and the same shape becomes a HANG, which is locatable in
minutes with a debugger attach. The decache did not create these; it made them findable.
⇒ **Consequence for sequencing, and it is the actionable half: do NOT hurry caching back in.** Every cache
restored re-blinds the surface it covers, so the order is (1) run uncached, (2) let the hot paths announce
themselves as stalls, (3) fix the READS that should never have computed, (4) only then let the AI plane cache
its own scores, simply ([ai-architecture-north-star.md](../plans/parked/ai-architecture-north-star.md)).
⛔ A cache added while a wrong-shaped read is still underneath it hides the read instead of fixing it — the
[DEC-no-self-heal](#dec-no-self-heal) failure one plane over.
**Home:** [roadmap.md](../plans/structural-cleanup/roadmap.md).

### DEC-accumulator-cut-uniform

Every legacy serialized incremental accumulator (serialized + `change*`/`update*`/`process*`-maintained + a per-turn
cascade-owned quantity — the STORED-ACCUMULATOR DRIFT class, [modifier.md §2b](../specs/modifier.md)) is cut by ONE
uniform mechanism: re-point the getter to a cascade fresh-gather accessor (÷100 at the reader), hard-delete the member
+ its maintainers (audit each body for side-effect riders — [engine.md](../reference/engine.md)),
full-delete the read + write and name the tag in `Assets/savemigration.txt` (the reader drains it — NO
`WRAPPER_SKIP_ELEMENT`, [DEC-save-remove-is-soft](#dec-save-remove-is-soft)), and let the COMPILER census the consumers. **NOT
wellbeing-specific — they ALL work exactly the same way**; wellbeing is the pilot. **Blast radius is the SIGNAL** (a
cut that does not reach broadly is not cutting the legacy), never a limit; anything sneaking a legacy value back in is
an ERROR ([DEC-no-legacy-masking](#dec-no-legacy-masking)). The recompute-from-source application of
[DEC-universal-yield](#dec-universal-yield). **Home:** [state-repositories.md](state-repositories.md).

### DEC-playability-not-a-gate

Neither playability nor COMPILING is a gate on
removing legacy — "it would break the game / needs a playtest first" is a rollerskate excuse, and **green is the
bait**: chasing it is what makes an agent shoehorn the new implementation into legacy, so everything goes in place
first and the tree compiles at the END, as the result of the completed rewire. ⛔ A red tree during a cut is an
ACCEPTED state, never a defect to fix by re-attaching what was archived (owner: *"I could not possibly care less if
this compiles; having a clean slate to do this right is the target."*), and equally **"get it building" is not a
milestone** — a green tree is the by-product of a finished rewire, not evidence of progress toward one.
**WHILE THE TREE IS RED, WIRED OUTRANKS CORRECT** — a machine's facts emitted, consumer registered and surface
reachable beats knowing its output is right (owner: *"it is more important that triggers are wired than knowing if
they give the correct result."*), because correctness is endpoint-observable and so cannot be tested until green.
⇒ **AND A WRONG WIRING IS REMOVED ON SIGHT, WITH AN INTERIM WRONG NUMBER ACCEPTED** (owner: *"it does not really
matter if we temporarily doublecount, it is more important that things are wired correctly"*). A double-count is
never a reason to keep a second maintenance surface alive and is never a thing to weigh: the wrong number is
temporary and loud, the second surface permanent and quiet. The shape is a mutation choke point MAINTAINING a
derived store beside the fact it emits, and its tell is a consumer that deliberately ignores that fact "because
the choke point already applied it" — the skip IS the compensation. Removed in the same change, never recorded
as a todo. **Home:** [roadmap.md](../plans/structural-cleanup/roadmap.md) § LEGACY STILL BREATHING.
⚠ That SEQUENCES the acceptance bar, it does not relax it — and it lapses the moment the tree builds, when
correctness becomes testable and therefore owed. Removal is DELETE-DRIVEN: hard-delete the member (save-safe via
`savemigration.txt`), and the COMPILER is the census (every consumer still on it is a compile error —
un-self-certifiable, so you cannot flip-and-pretend), so a compile error is a WORKLIST ENTRY, never a reason to
re-shape what is being built. Done = compiler-complete rewire onto the cascade + endpoint-observable correctness on
a LOADED save (not *playing*). The only legacy that stays is an owner-ruled carve-out. **Home:**
[roadmap.md](../plans/structural-cleanup/roadmap.md); the acceptance bar it sequences:
[validation.md](../specs/validation.md).

### DEC-red-ratchet

The XML `CvXInfo` classes are archived (`SourceArchive/Infos/`) as a fallback-proof ratchet: **never restore them,
never re-add a `CvXInfo`, never treat a red build as a defect to fix by reviving one.** Green is reached ONLY by
finishing the JsonInfo structure + the full getter/consumer wiring — never by re-adding the legacy fallback. (The
ratchet rule stands regardless of build state.) **Home:**
[AGENTS.md](../../AGENTS.md) Build And Test.

### DEC-no-xml-into-game

Reading a REPLACED info's legacy XML **into the running game is HARD BANNED**. The `CIV4<X>Infos.xml` files are kept
in the tree as **curator INPUT ONLY** (removing them broke the curator); the game registers + populates replaced
infos from the **JSON** load path, never from `LoadGlobalClassInfo(GC.m_pa<X>Info, "CIV4<X>Infos", …)`. Their presence
for the curator is not license to load them at runtime — the recurring rollerskate. **Home:** [AGENTS.md](../../AGENTS.md) Build And Test.

### DEC-one-reverse-view

Reverse lookups ("who references me") are populated ONCE, at the JSON read, as reverse edge FAMILIES on the
referenced info object itself (`EDGEF_RELATED` = the display/pedia candidate lists; `EDGEF_REQUIRED_BY` = the
enabler's requires-reverse-index). After load every info ALREADY CARRIES its reverse lookups: a consumer
(CvGameTextMgr, an enabler, anything) reads its info's own lists — never a whole-database scan on a hot path,
and never a bespoke reverse view or side index of its own (especially not inside an enabler).
**Home:** [modifier.md §1](../specs/modifier.md).

### DEC-materialize-at-mapfrom

An info getter NEVER does a per-call string-keyed read (modifier-address sums, bool-block string walks,
grants/allowed bucket fetches, raw JSON re-reads) — every such value materializes ONCE at mapFrom into a typed
member and the getter is a bare member read. The compiled `CvModifiers` entry list is the one load-time scan
source; classification blocks read by generated id (`CLS_HAS`). **Home:** [patterns.md § Materialize at mapFrom](patterns.md).

### DEC-json-not-cascade

Parsing and HOLDING the info data is **INFO-side, never cascade-side** (owner). An info is a pure data source with
one outbound surface: authored data resolved to typed members at `mapFrom`, handed out ASKED-FOR-BY-CHANNEL. It
carries **zero cascade runtime** — no DepositIndex, no evaluator, no per-owner state, no computed total, no staleness
flag, no cache — and it never learns what a cascade, a scope or an owner is. The compiled deposit index is the
mirror image: **cascade-side ONLY**, populated from the spec model at push time. The boundary is load-bearing
because an info is write-once-at-load and SHARED by every player, so parking per-owner derived state on one makes
an immutable shared object mutable per GAME rather than per LOAD — and it would be a third copy of the same static
numbers, after the authored JSON and the compiled index. Stated as a CONTRACT, not a prohibition: there is no
member to write to. **Home:** [patterns.md § The INFO DATA-OUT contract](patterns.md).

### DEC-one-json-reader

Exactly ONE JSON reader exists — the single load-time pipeline in `Sources/Data` (enumerate once, parse each file
once, register→mapFrom→retained-parse FK/reverse pass→routing compile, fail-loud key coverage) — and JSON is read
at GAME LOAD ONLY: every JSON-shaped object is freed before load ends. Inventing a second reader/parse site is the
defect (it has happened repeatedly). The `Json` name-fragment is reserved for the load-time parse surface; a
runtime-resident `Json*`-named type is misnamed or misplaced. **Home:** [patterns.md § The ONE reader](patterns.md).

### DEC-info-plane-read-only

The info plane is WRITE-ONCE-AT-LOAD: `edit`/`editPtr` (get-or-create) belong to the ONE reader, the reverse pass
and the classification registry, and **a read NEVER creates or grows a registry**. A read that cannot be answered is
a LOAD defect and FAILS LOUD, naming the registry + id, in every config — never answered with a freshly-minted blank
info. Two reasons it is a hard rule: a blank info's `getType()` returns NULL, which is dereferenced in the EXE's
frame or in boost::python with nothing left naming the id that caused it; and on an ALIASED repo the backing IS
`GC.m_pa<X>Info`, so creating moves `getNum<X>Infos()` — a READ silently redefines the registry's own bound and
bounded walks run off into entries the walk itself created. **Corollary (owner): crash at the main menu because
things are not loaded, rather than manually incrementing the registry to limp past it** — and never defer a read
(the lazy-screen shortcut) to make the failure go quiet, which moves the failure somewhere illegible without
initializing anything. The read-side twin of [DEC-one-json-reader](#dec-one-json-reader); the fail-loud application
of [DEC-no-legacy-masking](#dec-no-legacy-masking). **Home:** [patterns.md § The INFO DATA-OUT contract](patterns.md).

### DEC-scope-is-an-axis

A kind/scalar/yield enum names its CONCEPT only; the SCOPE a value is authored at is a separate axis of the deposit
address and a spelled-out getter parameter — never a fragment of an enum/member/getter name (no `_GLOBAL`,
`_ALL_CITY`, `_WORLD` kinds). Kind and scope separate in storage and API exactly as the JSON's
`<family>.<scope>.<member>` separates them. **Home:** [patterns.md § coherent surface](patterns.md).

### DEC-classification-infos

The §8/§9 classification categories (skills / tags / attributes / amenities / characteristics / capabilities / policies)
exist as RUNTIME-GENERATED INFOS: one info per distinct authored block key, minted at load into the global infotype map
(`SKILL_`/`TAG_`/`ATTRIBUTE_`/`AMENITY_`/`CHARACTERISTIC_`/`CAPABILITY_`/`POLICY_` + UPPER_SNAKE of the camelCase key) and its
category's InfoRepo — referenceable like any authored info, with every entity's blocks resolved to by-id bitsets. Nothing is
hand-authored per category; the registry derives from the data. **The id ORDER is PINNED by a generated table**
(`Tools/Migration/curate_classification_ids.py` → `Infos/CvClassificationIds.h`, the `_order.json` precedent) which the
registry SEEDS from before minting, so each `CLS_<DOMAIN>_<KEY>` constant IS the runtime id and the whole consumer surface
is ONE parameterized read per domain (`info.hasSkill(CLS_SKILL_BLITZ)`) rather than a getter per key. Openness is intact —
a key absent from the table still mints at load, appended after the seeded block — and nothing serializes, so regenerating
is save-neutral. ⚠ A **HOLDER'S** side is not always a bitset: where several
grantors can confer the same key the holder stores an id→COUNT dictionary instead (the city's `amenities`, json.md §8) —
the by-id resolution is what is uniform, not the storage width. **Home:** [json.md §8](../specs/json.md).

### DEC-enabler-not-cascade

*(One instance of [EACH IS ITS OWN SYSTEM](north-star.md) — kept as its own entry because the NAMING guard below
is load-bearing on its own.)* The **enabler** ("can I?" — the generate-then-gate availability machine: the frontier + operating-building sets) and
the **modifier cascade** ("how much?" — the magnitude machine) are TWO SEPARATE SYSTEMS that agents routinely
conflate — a top cause of the read-path rollerskates. To kill the ambiguity: **"cascade" names the MODIFIER system
ONLY**; the availability machine is **"the enabler"**, never "the enabler cascade." Its classes carry no `Cascade`
prefix (`EnablerKernel` / `BuildingEnabler` / `UnitEnabler` / `TechEnabler`), and it handles the `enables` forward
walk + the `requires` gate.
Availability getters (`canConstruct`/`canTrain`/`canResearch`/…) read the enabler's OWN cached sets directly (the same
"read your own cache" shape the modifier getters use for the game-object modifier caches — [DEC-no-self-heal](#dec-no-self-heal),
[state-repositories.md](state-repositories.md)). **One consumer per system:** the enabler has its own spine consumer;
a shared one welds the two machines and forces one load policy onto two that differ
([superseded-ideas](superseded-ideas.md) #16). **Home:** [enabler.md](../specs/enabler.md).

### DEC-scope-contexts

Each game-object scope a cascade reader needs — **plot / city / player** (NEVER area — a landmass is shared by
several empires, so it is not an ownable SCOPE at all, only a bare id + tile count a city reads as a FACT, and
area-shaped effects author at empire; **NEVER team — `CvTeam` is the TECH BRIDGE and `CvPlayer` holds the context,
so the eval ctx carries no `CvTeam*` and every team fact is asked of the player**; units are a FUTURE role-specific
scope) — owns ONE per-scope live-state CONTEXT
(`PlotContext` / `CityContext` / `EmpireContext`), the single home a getter/evaluator reads for that scope's
changeable state. A context STORES only its uniquely-owned AGGREGATE (COUNTS keyed by id via the shared
`ContextDict`; state with no home elsewhere — `CityContext.plotAttrs`, `EmpireContext.policies`) and FORWARDS
everything already O(1) on the bound game object — never duplicated. Bound by pointer, passed by reference (never a
value copy); maintained EVENT-DRIVEN (no per-turn recompute; load builds via the reseed). Isolation is for
RESPONSIBILITY + reader symmetry, not decoupling. **The HAVE axis reads through the contexts**: each scope's
possession state is asked of its context (forwarded from the owning object; stored only where homeless), never
reached ad hoc off the game object. The building getter's `(cityContext, plotGroup)` reads
`cityContext` for vicinity/local, `plotGroup` (`CvPlotGroup`) for traded — parameters spelled in full (no `cx`/`pg`
abbreviations; short names are only for scoped lambdas, which C++03 lacks), index params named for the enum they key
(`getFlatYield(YieldTypes eYield)`). **Home:** [contexts.md](contexts.md).
