# Decisions ledger — the canonical ID'd home for cross-cutting rulings

> ⛔ **Every `DEC-*` is a HARD RULE — binding by default, not advice to weigh.** The only thing that relaxes one is
> the owner explicitly saying so; a question you have posed to the owner is a HARD STOP until answered.
>
> **This is an INDEX, not a re-statement.** Each entry is one decision's *pure consequence* + a pointer to its
> authoritative home. **Before adding any cross-cutting ruling anywhere, grep this file first.** A doc that needs a
> ruling links `[DEC-id]`; it does not re-articulate it.

---

### DEC-fixedpoint-x100

×100 fixed-point is the engine's NATIVE representation EVERYWHERE — the cascade, the realized getters, and the
consumers all carry ×100. JSON is human; human→×100 converts once at readJson (the IN boundary); ×100→human converts
only at the OUT boundary — any READER (UI / `/computed` HTTP fields / `Cy*` Python) does a trivial `÷100`.
**NO getter reduces, and there are NO discrete carve-outs — EVERY channel works the same way (owner ruling; this
uniformity is the core of the rework).** A value that is physically a whole game count (angry citizens, a food
modifier) reduces at the POINT OF USE that consumes it as a whole number, never inside the getter that every other
consumer reads. **No getter has a ×100 "variant"** (never a `getX`+`getX100` pair); reducing at the getter forces that
split and lets the cascade be shoehorned into legacy-shaped getters — the half-migration reflex. **Blast radius never
limits the conversion** — the mapped consumer surface is the worklist, not a warning. **Home:**
[fixed-point-and-scales.md](../specs/curators/fixed-point-and-scales.md); the standing conversion worklist:
[fixed-point-conformance.md](../plans/structural-cleanup/fixed-point-conformance.md).

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

The dry-calc computes every value from JSON + game state with zero legacy-computed ride-in; engine-computed data
enters only at the comparison boundary, never as a cascade input. The trap is the CAMOUFLAGED case — a DERIVED value
masquerading as raw state, above all a building's ACTIVE/DORMANT verdict, which is a pure function of
`requires.operate` and must be COMPUTED, never read from the engine. **Home:** [validation.md](../specs/validation.md).

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

### DEC-mirror-then-redesign

The migration reproduces the engine's existing behaviour exactly; behavioural redesign ("should it behave this way at
all?") is deferred to post-migration, never done during it. **Home:** [validation.md](../specs/validation.md).

### DEC-stonebase-follows-spec

The validation authority chain is ONE-WAY — SPEC → StoneBase → engine-oracle: StoneBase *implements* the spec, never
reverse-engineers the engine's internal procedure; the engine fixes only the RESULT. A divergence is a curated-data
gap mapped to a named source, or a deliberate spec-change-FIRST — never a creative StoneBase tweak. Same-result is
necessary but NOT sufficient. **Home:** [validation.md](../specs/validation.md).

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

### DEC-derived-never-trusted

Derived data is never trusted from a save — `reset()` marks it dirty on load and recomputes from live state. **Home:** [save.md](../specs/save.md).

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
strict complement of [DEC-mirror-then-redesign](#dec-mirror-then-redesign) (defer redesign, never data). **Home:**
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
computed by a legacy ad-hoc path OUTSIDE the machine is a shortcut/failure. The **OUTPUT-SEAM**: where the engine does
placement/application (free-specialist assignment; the golden-age plot-base-yield-threshold "+1"), the cascade owns
the authored INPUTS + the OUTPUT yields — both live in the machine — and ONLY the middle mechanism is engine-owned.
**Home:** [modifier.md](../specs/modifier.md).

### DEC-done-is-observable

Done = the effect is observable in the RUNNING GAME via an endpoint poll — never "the code path exists" or "the data
loads." "Straight up missing" means it does not show in-game even if it loads (the break is downstream, in
apply/display). Every work item's acceptance is an endpoint-observable pass/fail on a real save, a real turn — the
strict complement of [DEC-verify-in-game-not-reshadow](#dec-verify-in-game-not-reshadow). Programmatic already: the
`/computed` oracle endpoints expose the real engine values as game-thread snapshots (a blind value is EMITTED first,
step one of its fix); StoneBase's frontend renders them alongside the calc-counts. **Home:** [validation.md](../specs/validation.md).

### DEC-calc-count-gate

Every calculation logs its `(scope, channel)` (scope ∈ world/team/empire/area/city/plot/building/unit/specialist;
channel = every modifiable number). The per-turn count is a standing acceptance gate + regression tripwire: >50k/turn
is near-certainly a failure (a blanket recompute), a quiet turn approaches zero, steady-state tracks EVENT volume
(thousands) not entity count (millions). Exposed live via the StoneBase performance dashboard. **Home:** [observability.md](../reference/observability.md).

### DEC-cy-not-fixed

The `Cy*` info-binding contract (the boost::python `.def` surface) is NOT a fixed contract to preserve; freezing it
forced the JSON pocos to mirror the entire legacy `CvXInfo` field contract (a stub per legacy field). Redesign the
boundary around the cascade/JSON model + rewire the Python info-CONSUMERS; fix the stub-fed wrong values. DISTINCT
from the computed-getter flip strategy (which keeps those contracts and rewires bodies, not call sites). Python
gameplay stays Python. **Home:** [roadmap.md](../plans/structural-cleanup/roadmap.md).

### DEC-new-getter-surface

**REUSING A LEGACY GETTER IS THE MECHANISM THAT PRODUCES THE HALF-MIGRATED STATE** (owner) — not a shortcut that
merely risks one. A legacy getter's contract encodes legacy assumptions (its scale, its granularity, its combine,
its one-channel shape), so pointing the cascade at it forces the CASCADE to bend to that shape; the result is a
surface that is half cascade and half legacy and reads as nearly done. The 360 channel-shaped getters on
`CvCity`/`CvPlayer` alone are 360 such contracts. Therefore: **build a NEW uniform, parameterized getter set over
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
missed invalidation must surface as a live divergence, never be silently rebuilt away. Sharpens the CAPSTONE RULE
(LOAD is the only full pass). **Home:** [state-repositories.md](state-repositories.md).

### DEC-uniform-cache-shape

Every derived cache on the cascade plane is the **SAME OBJECT TYPE** everywhere and they **ALL INVALIDATE THE SAME
WAY** — one templated `CvDerivedCacheSet<TOwner>` over a channel-indexed slot table on every owner, driven by ONE
mark derivation; only WHICH SLOTS carry a value varies by scope. A hand-named scalar field is therefore a DEFECT,
not untidiness: it cannot be addressed uniformly, so it forces a bespoke invalidation path per field. A **RECEIVER**
(the scope that consumes a channel) is not a different kind of cache — it caches its realized sum as one variable
per channel in the same cache beside the packages (`CvPlayer` research/gold/culture/espionage; `CvCity`
production/culture), so there is no separate receiver mechanism. Rejecting the legacy push accumulator never
licenses a per-read walk instead. ONE event marks the packages it touches AND the sum slots they feed, so no
dependency-ordered rebuild pass exists. **Home:** [state-repositories.md](state-repositories.md).

### DEC-spine-reseed

On load, the cascade is built from events that come from **inside the save read itself** — reading a fact off the
stream is what fires its DOMAIN event (`CvGame::read` → `CvPlayer`/`CvCity`/`CvPlot::read`); the north-star is
the event SETTING the state (read → emit → populate); object-populated-by-events is out of the current scope.
It is NOT a separate post-deserialization pass that fabricates events by walking already-populated objects — that
pseudo-emit is banned ([superseded-ideas](superseded-ideas.md) #13) — and equally NOT a warm-up "seed" that walks
has-lists into a consumer's cache beside the event stream (an invented second build mechanism that leaves the
consumer deaf to the reseed and defeats the missed-emit tripwire). The load lifecycle is bracketed by
`GAME_LOAD_STARTED` / `GAME_LOAD_FINISHED` spine events; result-producers (grants) rely PURELY on the spine and
suppress between them (a grant is a RESULT of a genuine in-play acquisition; a load is not one), while the
cache-build consumer stays load-active. New game builds the same way (real init fires the same events, grants
active). **The reseed emits + the bracket are BUILT and live** (verified in `Cascade.log`).
**Home:** [event-spine.md](../specs/event-spine.md).

### DEC-verify-in-game-not-reshadow

Parity + shadow are CLOSED — their job is FINISHED and they are NOT to be re-run, re-invoked, or used to frame any
remaining work. The confirmation already exists and is sufficient: StoneBase strongly verified the event-spine
STRUCTURE and the shadow strongly verified the CALCULATIONS reach the right numbers — the design is proven
achievable. The original `readJson`-direct shadow read JSON STRAIGHT, bypassing the loaded `CvJson<X>Info` objects;
that bypass was itself a rollerskate and must NOT be repeated. ⛔ Do not re-shadow and do not re-frame work as
"parity"/"shadow": that framing sends agents rollerskating back into offline validation instead of building the real
info-object-backed runtime. Remaining verification is the LIVE game ONLY — manifestation via the endpoints
([DEC-done-is-observable](#dec-done-is-observable)) + the per-turn `(scope,channel)` calc-count gate. StoneBase itself
is repurposed: PROMOTED from offline parity oracle to the USER-VISIBLE PERFORMANCE LAYER (the live
`(scope,channel)`/turn-time dashboard — Razor + SignalR); it MAY still spot-verify parity against known-good as a
sanity check, but that is not the migration's validation path. **Home:** [validation.md](../specs/validation.md).

### DEC-oracle-tautology

With the legacy accumulators DELETED (red ratchet), the `*Legacy` oracle getters + the `*Recomputed`/`*Leg` twins on
the `/computed` endpoints read the cascade-derived flipped getters, NOT legacy — so every remaining oracle-vs-cascade
comparison is cascade-vs-cascade: a structural always-parity that can never turn red (the false-confirmation trap). Oracle
parity is NOT a verification signal (only served-value sanity + the compiler census are). The oracle surface is REMOVED,
not maintained (a dedicated sweep). **An oracle endpoint is NOT a real consumer:** a legacy member whose only live
consumer is an oracle endpoint is a self-referential keep-alive that fools the compiler census — remove the endpoint WITH
the member; the oracle is never a reason to keep legacy alive. **Home:** [validation.md](../specs/validation.md).

### DEC-no-legacy-masking

Legacy outputs must FAIL LOUD, never be preserved or snuck in via getters/fallbacks. A realized getter reads the
CASCADE ONLY — no `*Legacy` fallback, no pre-init/what-if legacy path; a cascade gap returns a wrong/empty value
(exposed), never a legacy-correct one (masked). Legacy masking a wrong cascade is WORSE than legacy failing: the mask
hides the defect and defers the fix (the wellbeing panel reading legacy hid a 2× cascade inflation). Purge legacy
**violently** so what is missing/wrong is immediately visible. Blast radius is never a reason to keep a legacy path
alive. **The legacy XML is REMOVED (the red ratchet), so a legacy fallback cannot even RUN — it is BAIT that substitutes
a nonexistent answer and masks the hole** ([DEC-red-ratchet](#dec-red-ratchet)); a realized gate/getter is therefore
a PURE cascade read (the six availability gates carry no `*Legacy` fallback, no pre-init guard, no what-if path).
Corollary of [DEC-playability-not-a-gate](#dec-playability-not-a-gate) + [DEC-oracle-tautology](#dec-oracle-tautology)
for the READ surface. **Home:** [validation.md](../specs/validation.md).

### DEC-legacy-decache-poisons-perf

The #430 cut NUKED the serialized accumulators legacy calcs depended on for O(1) reads (`m_iBuildingGoodHappiness` &
its cluster, …). Stripped of those caches, a surviving legacy calc (`happyLevelLegacy`, `badHealthLegacy`, …)
recomputes from scratch on EVERY call — so ANY perf measurement taken while legacy still runs in a read path measures
**legacy's decache penalty, not the cascade** (proven: the unit-selection lag was legacy `unhappyLevel(iExtra)`/
`badHealth(bNoAngry)` what-if re-sums per read; it vanished the instant the getters went cascade-only). All turn-time/
FPS/lag numbers gathered with legacy on any hot read path are POISONED. Clean perf is only measurable AFTER legacy is
fully purged — so the violent purge is a PREREQUISITE for the perf hunt, not merely a correctness/tidiness step.
Sharpens [DEC-turn-time-is-king](#dec-turn-time-is-king). **Home:** [roadmap.md](../plans/structural-cleanup/roadmap.md).

### DEC-accumulator-cut-uniform

Every legacy serialized incremental accumulator (serialized + `change*`/`update*`/`process*`-maintained + a per-turn
cascade-owned quantity — the STORED-ACCUMULATOR DRIFT class, [modifier.md §2b](../specs/modifier.md)) is cut by ONE
uniform mechanism: re-point the getter to a cascade fresh-gather accessor (÷100 at the reader), hard-delete the member
+ its maintainers (audit each body for side-effect riders — [engine.md](../reference/engine.md)),
full-delete the read + write and name the tag in `Assets/savemigration.txt` (the reader drains it — NO
`WRAPPER_SKIP_ELEMENT`, [DEC-save-remove-is-soft](#dec-save-remove-is-soft)), and let the COMPILER census the consumers. **NOT
wellbeing-specific — they ALL work exactly the same way**; wellbeing is the pilot. **Blast radius is the SIGNAL** (a
cut that does not reach broadly is not cutting the legacy), never a limit; anything sneaking a legacy value back in is
an ERROR ([DEC-no-legacy-masking](#dec-no-legacy-masking)). The recompute-from-source
([state-repositories.md](state-repositories.md)) application of [DEC-universal-yield](#dec-universal-yield). **Home:**
[fixed-point-conformance.md](../plans/structural-cleanup/fixed-point-conformance.md).

### DEC-playability-not-a-gate

The `json-data-migration` branch is knowingly not playable, and playability is NOT a gate on removing legacy —
"it would break the game / needs a playtest first" is a rollerskate excuse. Removal is DELETE-DRIVEN: hard-delete the
member (save-safe via `savemigration.txt`), and the COMPILER is the census (every consumer still on it is a compile
error — un-self-certifiable, so you cannot flip-and-pretend). Done = compiler-complete rewire onto the cascade +
endpoint-observable correctness on a LOADED save (not *playing*). The only legacy that stays is an owner-ruled
carve-out. **Home:** [validation.md](../specs/validation.md).

### DEC-red-ratchet

The XML `CvXInfo` classes are archived (`SourceArchive/Infos/`) as a fallback-proof ratchet: **never restore them,
never re-add a `CvXInfo`, never treat a red build as a defect to fix by reviving one.** Green is reached ONLY by
finishing the JsonInfo structure + the full getter/consumer wiring — never by re-adding the legacy fallback. (The
tree currently LINKS green via that correct path; the ratchet rule stands regardless of build state.) **Home:**
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

### DEC-one-json-reader

Exactly ONE JSON reader exists — the single load-time pipeline in `Sources/Data` (enumerate once, parse each file
once, register→mapFrom→retained-parse FK/reverse pass→routing compile, fail-loud key coverage) — and JSON is read
at GAME LOAD ONLY: every JSON-shaped object is freed before load ends. Inventing a second reader/parse site is the
defect (it has happened repeatedly). The `Json` name-fragment is reserved for the load-time parse surface; a
runtime-resident `Json*`-named type is misnamed or misplaced. **Home:** [patterns.md § The ONE reader](patterns.md).

### DEC-scope-is-an-axis

A kind/scalar/yield enum names its CONCEPT only; the SCOPE a value is authored at is a separate axis of the deposit
address and a spelled-out getter parameter — never a fragment of an enum/member/getter name (no `_GLOBAL`,
`_ALL_CITY`, `_WORLD` kinds). Kind and scope separate in storage and API exactly as the JSON's
`<family>.<scope>.<member>` separates them. **Home:** [patterns.md § coherent surface](patterns.md).

### DEC-classification-infos

The §8/§9 classification categories (skills / tags / attributes / capabilities / policies) exist as
RUNTIME-GENERATED INFOS: one info per distinct authored block key, minted at load into the global infotype map
(`SKILL_`/`TAG_`/`ATTRIBUTE_`/`CAPABILITY_`/`POLICY_` + UPPER_SNAKE of the camelCase key) and its category's
InfoRepo — referenceable like any authored info, with every entity's blocks resolved to by-id bitsets. Nothing is
hand-authored per category; the registry derives from the data. **Home:** [json.md §8](../specs/json.md).

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

Each game-object scope a cascade reader needs — **plot / city / player** (NEVER area: a bare id, "a really big plot,"
whose effects map to the player; units are a FUTURE role-specific scope) — owns ONE per-scope live-state CONTEXT
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
