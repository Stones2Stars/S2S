# #430 roadmap — the cascade rebuild

> **The master plan for the active work.** Mandated session-start reading (root `AGENTS.md`). It states the DESIGN
> the code must conform to and the rulings that govern how it is built. Governing rulings are ledgered as `DEC-*`;
> this doc links them, it does not re-articulate them.
>
> ⛔ **This doc carries NO status and NO worklist** ([DEC-spec-plus-todo](../../architecture/decisions.md#dec-spec-plus-todo)):
> what is not done lives in the [todo](todo.md), and what is BUILT is answered by the tree, never by a table here
> that drifts. Verify any claim against the code before acting on it.
>
> **⛔ Compiling is not a gate, green is the bait, and a compile error is a worklist entry** —
> [DEC-playability-not-a-gate](../../architecture/decisions.md#dec-playability-not-a-gate) states that ruling in
> full, including the wired-outranks-correct sequencing and why chasing green is what shoehorns the new
> implementation into legacy. It is not restated here.
>
> **⛔ THE AI LOOPS ARE ORDINARY CONSUMERS — CONVERT THEM (owner).** `CvPlayerAI`/`CvUnitAI`/`CvCityAI`/`CvTeamAI`
> are the BULK of the consumer surface, and treating them as untouchable is what fragments the work: a single
> defect spanning `CvPlayer` + `CvCityAI` + `CvUnitAI` is ONE fix, and splitting it by which folder the site
> happens to sit in leaves the AI halves rotting indefinitely. **Rewriting an AI loop to fit the new surface IS
> the work** — the contexts exist precisely so it can be rewritten.
> ⛔ **The ONE thing that is banned is the opposite move: bending the new surface to preserve an AI call's old
> shape** — accepting a what-if argument, an ignore-this-clause flag, a bool where the new answer is richer
> ([DEC-new-getter-surface](../../architecture/decisions.md#dec-new-getter-surface)). That, not the folder, is
> the failure mode.
> ⚠ **A dangling AI call site is a WORKLIST ENTRY like any other** (the compiler is the census). What genuinely
> waits is only a read whose REPLACEMENT MACHINE does not exist yet — name the missing machine when you leave
> one, never the file it lives in.

## Context — why this rebuild exists

The #430 design is specced and accepted; the DATA is solid (every gameplay entity curated, no placeholders —
`Assets/Data/**`, `Tools/Migration/curate_*.py`). What drifted was the C++ runtime, and it drifted in one
characteristic direction: **each scope, channel, and read site grew its own bespoke shape.** Five per-scope package
structs with hand-named per-channel members, ~33 hand-named scalar fields, a read-side `ensure()` protocol, one
shared spine consumer routing two different machines, and a legacy-getter surface the cascade was bent to fit.

Each of those was individually defensible and collectively fatal: a hand-named field cannot be addressed
uniformly, so every one of them forced its own invalidation path, which is precisely how that many accumulated.

**⚑ THE ENABLER IS THE WORKED CASE, AND IT IS WHY THE INFO REDESIGN WAS FORCED (owner).** The enabler was
already BUILT ONCE — and then *"tried to be forced into legacy endpoints,"* which is what triggered the full info
redesign and the proper setup this rebuild is. The machine was not the problem; the boundary it was pushed
through was. A finished machine wired into legacy-shaped call sites inherits every assumption those signatures
encode, and the result reads as done while being half-migrated
([DEC-new-getter-surface](../../architecture/decisions.md#dec-new-getter-surface)).
⛔ **The recurrence risk is highest exactly when a machine is finished and the call sites are waiting** — the
consumer sweep. The pressure shows up as a legacy signature the new surface "just needs" to accept: a what-if
argument, an ignore-this-clause flag, a bool where the new answer is richer. **Widening the new getter to fit the
old call is the failure**; the call site is re-expressed instead, or the question is re-homed to whichever machine
actually owns it ([enabler.md §8](../../specs/enabler.md): can-I-now is the enabler's, can-I-ever and by-what-path
are the picking logic's).

**So the substrate was archived rather than patched.** The proven machines were separated into their own trees, the
engine game-object classes were reverted to `main`, and the drifted modifier substrate was moved out of the
codebase entirely. This is not a redesign — the design below is unchanged and still authoritative. It is a rebuild
of the *implementation* against it, once, properly ([DEC-proper-once](../../architecture/decisions.md#dec-proper-once)).

**⚑ THE INFOS WERE RIPPED WHOLE AND REBUILT FROM SCRATCH (owner) — so EVERY compile error is rooted in the INFO
CONSUMER SURFACE.** That is a GUARANTEE about the census, not an observation about one build: there is no second
population of unrelated breakage to sort out first, and no subsystem that "also broke" and needs its own theory.
⇒ A missing member therefore always resolves through the disposition test ([todo.md](todo.md) § How to work the
compiler census): a RENAMED successor on the rebuilt info · an info genuinely MISSING a member whose data is still
authored · or a term that is DEAD and goes. ⛔ Reaching for any other explanation is the wasted motion this
guarantee exists to prevent.

## Governing principles

⛔ **These are LINKS, not summaries.** The rulings that govern this rebuild live in the
[decisions ledger](../../architecture/decisions.md) and are read there — a restatement here is a second copy that
drifts from its home and then gets trusted, which is the exact failure the ledger exists to end.

[DEC-no-deferred](../../architecture/decisions.md#dec-no-deferred) ·
[DEC-no-self-heal](../../architecture/decisions.md#dec-no-self-heal) ·
[DEC-spine-reseed](../../architecture/decisions.md#dec-spine-reseed) ·
[DEC-uniform-cache-shape](../../architecture/decisions.md#dec-uniform-cache-shape) ·
[DEC-universal-yield](../../architecture/decisions.md#dec-universal-yield) ·
[DEC-new-getter-surface](../../architecture/decisions.md#dec-new-getter-surface) ·
[DEC-enabler-not-cascade](../../architecture/decisions.md#dec-enabler-not-cascade) ·
[DEC-done-is-observable](../../architecture/decisions.md#dec-done-is-observable) ·
[DEC-turn-time-is-king](../../architecture/decisions.md#dec-turn-time-is-king)

The one thing this rebuild adds that is not a ledger entry — the KEYSTONE it is organized around: **each package
is its own self-invalidating cache**, and a DOMAIN event marks exactly the packages its source feeds, per the
deposit index. *"This is the basis of EVERYTHING."*

## The accepted foundational design (unchanged — the code conforms to this)

Authority: [state-repositories.md](../../architecture/state-repositories.md), [modifier.md](../../specs/modifier.md),
[enabler.md](../../specs/enabler.md), [event-spine.md](../../specs/event-spine.md).

- **The event spine is the foundation.** One `emit(KIND,…)` fanned by KIND to `IEventConsumer`s. Nothing else in the
  engine detects changes.
- **The EMIT surface comes first; the cache build is the step AFTER.** Caches cannot build from events until the
  events are completely emitted — during play AND from inside the save read.
- **Load and new-game are the SAME path.** Facts are order- and prerequisite-independent; the reseed is the ONE
  full build.
- **Per-scope packages, one uniform format** — Σflat / Σpercent per channel, cached at each scope's OWN object,
  each invalidated at its own scope only. The downward roll is realized AT READ: the realized value is the trivial
  sum of the ~5 scope packages. A lower scope never STORES an upper scope's sums.
- **The ORIGIN RULE** — yields come from PLOT, SPECIALISTS and BUILDINGS (city) only; modifiers come from
  everything BUT plot. Plot and the upper scopes are mirror images; CITY is the one scope carrying both.
- **KEYS ONLY WHERE NEEDED** — the channel set is data-defined and no object uses more than a fraction of it; each
  scope carries only the channels authored AT that scope, derived from the data at load, never hand-listed.
- **A cascade is a cache, two kinds** — the yield/percent packages are a VALUE cache (memoize, event-invalidate,
  recompute-from-inputs). The ENABLER's sets are maintained by TARGETED PROPAGATION in place, NEVER
  blanket-recomputed. Do not conflate them.
- **THE OUTPUT-SEAM** — where the engine does placement/application, the cascade owns the authored INPUTS and the
  OUTPUT yields; only the middle mechanism is engine-owned (free-specialist assignment; the golden-age plot
  threshold).

## ⛔ LEGACY STILL BREATHING — the standing DEMOLITION ORDER

**Surviving legacy is never a gap, a stage, a transitional shape, or a thing to keep working while the
replacement matures — it is a DEFECT, and the only correct action on it is DELETION** ([DEC-no-legacy-masking](../../architecture/decisions.md#dec-no-legacy-masking),
[DEC-no-deferred](../../architecture/decisions.md#dec-no-deferred),
[DEC-proper-once](../../architecture/decisions.md#dec-proper-once)).

⛔ **Purge it VIOLENTLY.** A legacy path left alive MASKS the hole its replacement has not yet filled, so keeping
it "until the new thing is ready" is the exact move that produces a half-migrated branch which reads as nearly
done. What is missing or wrong must be IMMEDIATELY VISIBLE, which means legacy has to FAIL LOUD rather than quietly
answer. **Blast radius is the SIGNAL the cut reached, never a reason to soften it**
([DEC-accumulator-cut-uniform](../../architecture/decisions.md#dec-accumulator-cut-uniform)), and neither
playability nor a broken build is a reason to soften it
([DEC-playability-not-a-gate](../../architecture/decisions.md#dec-playability-not-a-gate)).

**What is still breathing, and the order on each, is the [todo](todo.md).** This section is the STANDING
RULE the todo answers to, never the inventory.

⛔ **THE WORST OFFENDERS ARE THE ONES OFF THE CORE LOOP (owner) — prioritize them, do not discount them.** A legacy
path that runs every turn is exercised constantly, so a defect in it surfaces fast. One that fires occasionally —
a property spawn on a dice roll, in some cities, some turns — is **never exercised hard enough to fail visibly**,
so it sits there doing a wrong or duplicate thing indefinitely. It is invisible on BOTH axes at once: unexercised,
**and uninstrumented** — the replacement announces itself on the spine, while the legacy path rolls its dice in
silence, so *"we can track with eventspine that a propertyspawn is being evaluated; we have no idea if legacy does
it"*. The worked case: the property-unit spawn existed TWICE, both live, both rolling the same RNG — detectable
only by noticing a doubled spawn rate that nobody watches.
⚑ **So "the replacement is not fully in place" is NOT a reason to keep one** — it is the reason to cut: while
legacy answers, the replacement's gaps are masked and nothing forces them out
([DEC-no-legacy-masking](../../architecture/decisions.md#dec-no-legacy-masking)). Delete it, let the hole show,
then fill the hole.

⚠ **The list is KNOWN-INCOMPLETE and is not a completeness gate.** Legacy found anywhere else is killed on the
same terms; add it here when you find it. ⛔ What is NOT allowed is discovering a legacy surface and recording it
as acceptable, scheduled, or "kept until X" — that reframing is the failure this section exists to prevent.

## ⛔ THE OPEN ITEM — the ACCESS surface

> *"What we ultimately want is settled in the spec. What is not done is defining what and how things are
> accessed."* (owner)

Everything above is settled. The GRAMMAR is now settled too — [patterns.md § THE TWO READ ROLES](../../architecture/patterns.md)
— and the GAME-OBJECT half stands: one group read per family on plot / city / player / team, each folding
through the one cross-scope roll-up (`InfoValuation::realizedAt*`), beside the endpoint oracle. ⛔ The count of
those reads is not recorded here; it drifts, and the headers are the census.

⛔ **What is NOT done is the DISCONNECT — the legacy getter set is STILL STANDING beside it (the KILL LIST above).**
⚠ An agent reading only that the group reads exist would conclude the access surface is done and build on a
half-migration.

⚖ **The `CvCity` / `CvPlayer` / `CvPlot` consumer work PROCEEDS NOW — the wait is over (owner): "we *have* most
of the structure, there is no point waiting … delaying the changes in CvCity, CvPlayer and CvPlot at this stage
achieves very little."** The structure-first rule earned its keep while the core systems were missing — plugging
holes then meant plugging them with the only thing standing there, legacy — but the machines are in place, so
holding these back now buys nothing and leaves legacy breathing behind a gate that has already been passed.
⛔ So a `CvCity`/`CvPlayer`/`CvPlot` consumer or counter is NOT to be deferred on ordering grounds any more; the
standing rules that still bind are the ones about HOW (a NEW surface rather than a re-shaped legacy one,
[DEC-new-getter-surface](../../architecture/decisions.md#dec-new-getter-surface), and the delete-driven cut with
the compiler as census).
⚑ **And a fair few of those getters CONSOLIDATE ON THEIR OWN (owner): they are INFO-BACKED reads, so restructuring
the infos already collapses them** — the count is not a worklist of independent items. ⛔ Do not plan a per-getter
sweep for work that falls out of wiring the rebuilt infos through; measure what actually survives that first, then
cut the genuine residue. (The same reason the channel-shaped getter set is a DELETION list + COVERAGE checklist, never a per-getter
migration worklist — [DEC-new-getter-surface](../../architecture/decisions.md#dec-new-getter-surface).)

The rest of the boundary every consumer meets:

- **The DELETION of the legacy channel-shaped getter set** on `CvCity`/`CvPlayer`. They are a DELETION LIST plus
  a COVERAGE CHECKLIST, never a per-getter migration worklist. ⚠ Two axes of that set the package does NOT have:
  a getter naming its SOURCE, and a getter carrying a target-id argument — so neither converts by re-pointing.
  Two owner rulings fix the direction of what replaces them:
  - **⛔ NOT the existing getters — ONE NEW COHERENT SURFACE, STANDARDIZED ACROSS THE INFOS (owner).** The design
    task is **not** "find a replacement for each one": no legacy getter name, signature, or shape survives
    into it. They are a **DELETION LIST plus a COVERAGE CHECKLIST** — the set of values that must be answerable
    somewhere on the new surface — and nothing more. The shape comes from ONE standard, taken from the info
    exemplar ([patterns.md § THE GETTER SETUP](../../architecture/patterns.md)) and applied as uniformly as it can
    be made to go: per-GROUP reads parameterized over the group's natural index, ×100 native with no `100` in any
    name, scope a spelled-out argument
    ([DEC-scope-is-an-axis](../../architecture/decisions.md#dec-scope-is-an-axis)), extensible by DATA rather than
    by new members. ⚠ **Mapping legacy getter → new getter is the half-migration reflex in its purest form**
    ([DEC-new-getter-surface](../../architecture/decisions.md#dec-new-getter-surface)): it lets the legacy
    contract shape the replacement one getter at a time, which is exactly how the surface got here.
  - **⚖ EXISTING ENGINE ENUMS ARE THE PARAMETER VOCABULARY where one exists (owner)** — `YieldTypes`,
    `CommerceTypes` and their kin are what a group read is keyed on; a family with no engine enum uses its own
    kind enum (`CvInfoKinds.h`). The data-minted channel id stays the CACHE's internal key, never something every
    consumer learns.
  - **⚖ BUILD THE BASE FIRST; "the most efficient way" comes AFTER (owner).** The AI eventually wants *"a full
    'this is the improvements' snapshot for a building, or a set of buildings, so it can evaluate the results
    against its weights"* — a one-call read a caller weights itself. It is a real direction to **KEEP IN MIND,
    not to actively solve** (owner): tuning the shape of a surface that does not yet exist optimizes nothing.
    ⛔ Do not build, investigate, or pre-shape it ahead of the base. **What "keep in mind" DOES bind:** the base
    must not FORECLOSE it — a per-group read that can be filled into a caller-owned array leaves the door open;
    a design that could only ever answer one scalar per call would close it.
    **The efficiency that matters is ALREADY BANKED IN THE SPEC** — the event-driven frontier is the structural
    win (the AI iterates a small maintained choice set instead of scanning the entity database; every read is an
    O(1) lookup that never calls a calculator, [enabler.md §6/§7](../../specs/enabler.md)), alongside
    event-built contexts turning per-read scans into stored fetches
    ([contexts.md](../../architecture/contexts.md)). A snapshot is a refinement on top of that, not the source
    of the gain.
- **How a scope owner carries its cache** — the member, its binding, and its mark derivation, uniform across
  world / team / empire / area / city / plot.
- **The per-scope live-state CONTEXTS the getters + evaluator read** ([contexts.md](../../architecture/contexts.md),
  [DEC-scope-contexts](../../architecture/decisions.md#dec-scope-contexts)) — one per scope that needs it
  (plot / city / player; NO area — a bare id whose effects map to the player; units are a FUTURE role-specific
  scope). A context is an **EVENT-BUILT STORE, not a forwarding facade** (owner): it STORES every DERIVED fact the
  evaluation reads — computed once, maintained by spine facts, never recomputed at read — and FORWARDS only the
  object's own RAW O(1) data. Nothing is serialized; the reseed rebuilds it.
  The per-context stores, their maintenance triggers and their open gaps are [contexts.md](../../architecture/contexts.md)
  and [todo.md](todo.md) — not a census here, which drifts. ⚠ The network bonus COUNT is the deliberate
  non-instance: it FORWARDS to the plot group that owns it rather than being stored a third time
  ([enabler.md §8](../../specs/enabler.md) RESIDENCY).
- **How the INFO side hands its data to the cascade — "make the infos sane".** The target — **an info STYLED FOR
  THE JSON**: members mirror the JSON anatomy, amount getters ×100-native (no `100` in any name) and coherent
  (data-out by channel), classification read as a parameterized bitset test — is
  [patterns.md § The INFO DATA-OUT contract](../../architecture/patterns.md); the remaining work is
  [todo.md](todo.md). Consumers move onto the coherent surface, never the reverse
  ([DEC-new-getter-surface](../../architecture/decisions.md#dec-new-getter-surface)).
  An info holds **only its own side**: cross-entity own-output (a building's improvement/terrain yields) is NOT a
  building member — the improvement owns its yield, conditioned on the building's presence
  ([DEC-deliveryguy](../../architecture/decisions.md#dec-deliveryguy)). The info is shaped to that NOW; it is not distorted to hold data it shouldn't just because
  the delivery mechanism isn't built.
- **The GENERAL modifier own-output reverse-map** — ONE general load-time pass. At load, any source's target-keyed own-output deposit is reverse-landed on the TARGET as a compiled
  conditioned own-output entry ("+X while the source is present" — the source's presence is the entry's prebuilt
  `enabled` condition), so a modder authors either side and both carry it. Two landing classes: a yield-channel
  flat keyed `<yield>.<scope>.{improvements|terrains|features|routes}.{TARGET}` on a building/civic/tech lands
  plot-scope (where every component-specific buff resolves) — building→improvement yields land; and a
  buildings-keyed output-channel deposit (gold/culture/research/espionage/commerce/food/production/happiness/
  health on a building/civic/tech — the wonder/civic/tech → building-type boosts, the reverse-pass landing rules)
  lands on the target BUILDING at CITY scope (building output per modifier.md §2a/§2b), same family/value/unit,
  presence-gated at the AUTHORED deposit's scope axis, any authored condition composed in.
  Governing-deliverer keyed maps stay source-side ([DEC-deliveryguy](../../architecture/decisions.md#dec-deliveryguy), modifier.md §4): `buildRate` keyed targets,
  every TRAIT keyed deposit (the per-set carve-out), route→improvement yields (the §4 exemplar; the legacy
  improvement-side readers are fed by the pass's compat rows), and the civic feature-happiness keyed member (the
  §2b one-term bundling). The same pass generalizes `EDGEF_RELATED` over
  every compiled surface (edges/requires/deposits/grants/provides/triggers — the retired tech-only bespoke
  inversions and their legacy-mirror getter reads are deleted), carries the `EDGEF_REQUIRED_BY` gate walk, and
  owns the forward compat reconstructions ([DEC-one-reverse-view](../../architecture/decisions.md#dec-one-reverse-view)).
  **The cut is FULL (owner): the new coherent surface is built and every consumer rewired onto it in the same
  pass, the legacy getter names disconnected — never a thin-compat layer left breathing.** Blast radius is the
  SIGNAL the cut reached; a change that left consumers untouched would be the half-migration tell.
- **⛔ NOTHING GATES THE DISCONNECT — A DEAD LEGACY PYTHON GETTER IS AN OUTLAW, SHOT ON SIGHT (owner).** There is
  no sequencing, no permission, no "wait until the library answers it": a binding to a getter that no longer
  exists is a dangling reference the compiler already named, and for a `Cy` binding the ONLY fix is DELETION
  ([DEC-cy-not-fixed](../../architecture/decisions.md#dec-cy-not-fixed) forbids re-pointing or widening).
  ⚑ **Cutting wrong is CHEAP** — *"if we delete a working binding, we re-add it; or more probably, REPLACE it"* —
  so a cut is not merely reversible, it is how a genuine requirement gets DISCOVERED and named. Piecemeal cutting
  is fine; what is banned is bending anything to keep the old surface functional.
  ⛔ **So COMPLETENESS IS THE LIBRARY'S END STATE, NEVER A GATE ON CUTTING.** Reading it as "cut only once the
  library is complete" inverts the ruling into a shield for the very surface being removed — and produces the
  three failures below. **The cut is the FORCING FUNCTION that drives completeness; it does not wait on it.**
  ⛔ **Failure 1 — avoiding the library.** ADD TO IT WHENEVER A READ MAKES SENSE. It is built incrementally like
  everything else, and a read added early is the library getting closer to complete, never work out of order.
  ⛔ **Failure 2 — borrowing LEGACY as a "temporary solution"** because the library does not answer something
  yet. That is the half-migration in its purest form
  ([DEC-no-legacy-masking](../../architecture/decisions.md#dec-no-legacy-masking)): a legacy read taken to fill a
  gap MASKS the gap and outlives the excuse. Add the read instead.
  ⛔ **Failure 3 — "we do Python later", parking thousands of compiler errors. This CLOGS THE CENSUS EVERYTHING
  ELSE DEPENDS ON (owner).** MSVC stops at 100 errors per translation unit, so parked Python-side errors SPEND
  the reporting budget the real worklist needs, and which errors survive is decided by whatever compiled first —
  the census stops naming the work and starts hiding it. Python debt is the most expensive kind to defer, not the
  cheapest. Clear it as you meet it.
- **The new Python surface — ONE COMPLETE DATA-FETCHING LIBRARY (owner)** replacing the scattered per-type `Cy*`
  interfaces ([DEC-cy-not-fixed](../../architecture/decisions.md#dec-cy-not-fixed)). COMPLETE against the census
  (screens + pedia + the Python-authoritative systems) is the END STATE it is built toward, so that no read is
  left needing a reach-around into legacy — a surviving gap re-creates the two live surfaces the ruling forbids.
  Data fetching only; Python gameplay stays Python and consumes it.
  Not a widened binding, not a shim beside it. Detail + acceptance: [todo.md](todo.md).
- **The endpoint route table**, which reads the same uniform getters as everything else.

⛔ **A per-site rewire is the half-migration this rebuild exists to undo** — it is what every previous attempt did
while believing it was conforming. Consumers move onto the new surface as a surface, never one call site bent to
fit. ⚠ This is a rule about HOW, not a gate on WHEN: the `CvCity`/`CvPlayer`/`CvPlot` consumer work proceeds now
(above).

## Observability / acceptance

Unchanged in principle, but note the surface it depends on is currently purged:

1. **Manifestation polls** — PROGRAMMATIC against the `/computed` oracle endpoints, never eyeballing the screen. A
   blind value is EMITTED first; emitting it is step one of that item's fix.
2. **Turn time** — the whole performance signal, on the wall clock; the target and its sequencing are in
   §Verification targets below. The process-memory gauge rides beside it, once re-emitted.
3. **Parity and shadow are CLOSED** and are NOT re-run, re-invoked, or used to frame remaining work
   ([DEC-verify-in-game-not-reshadow](../../architecture/decisions.md#dec-verify-in-game-not-reshadow)).

## Verification targets

- **Turn time (owner): ≤ 2 minutes wall per turn** on the standing late-game test save. The
  [DEC-turn-time-is-king](../../architecture/decisions.md#dec-turn-time-is-king) objective made concrete. The perf
  hunt resumes only after the caches are event-wired and the game runs behaviourally as it used to.
- **The MEMORY hunt stays parked** by the same sequencing ruling: chasing per-turn memory is pointless until legacy
  is gone and everything runs on the cascade + enabler, because the growth is turn-processing-borne and that
  processing is what the rebuild replaces ([memory-footprint.md](../../reference/memory-footprint.md)).
- Build: `powershell.exe -NoProfile -ExecutionPolicy Bypass -File "../Tools/_Build.ps1" <Config> <verb>` from
  `Sources/` (Release for interactive testing; FinalRelease for perf). Runtime verification needs per-session owner
  permission and goes through `agentstart.bat` only.

## Scope decisions

1. **Backlog scope = the #430 critical path only.** The `docs/plans/parked/` forward-FEATURE backlog is OUT.
1b. **⚖ WORLDBUILDER IS NOT A CONSTRAINT ON THIS REWORK (owner)** — *"WorldBuilder is something that will need a
   real review and pass, post rework; if we temporarily kind of break it, I can live with it."* So a WB path is
   never a reason to preserve a shape, keep a legacy call alive, or narrow a cut — it gets its own deliberate
   review pass afterwards. ⛔ This does NOT license breaking it carelessly or leaving it undocumented: when a
   change knowingly breaks a WB path, SAY SO in the change so the later pass has a worklist.
   **⚑ WHY it needs its OWN pass rather than riding along (owner): *"WorldBuilder can add or remove anything, at
   will."*** That is a categorically different relationship to the model than any other consumer. Every other
   surface reaches state through a genuine acquisition — a building is CONSTRUCTED, a unit is TRAINED, a tech is
   RESEARCHED — and the whole event spine is built on that: one fact, emitted at the genuine mutation choke
   point. WB instead mutates arbitrary state directly and at will, so it can violate every invariant the model
   rests on: it can make an entity appear with no acquisition, vanish with no death, or change owner with no
   conquest. A WB edit that changes state silently leaves every cache, context and enabler set wrong, exactly as
   a missing emit does — WB is simply the surface that can produce that condition deliberately, on any field, in
   one click.
   **⛔ THE REQUIREMENT, therefore: WORLDBUILDER ADDING OR REMOVING ANYTHING EMITS, exactly as the normal path
   does — with **no WB special case anywhere**.** ⛔ Do not build a "WorldBuilder mode" that suppresses or
   reroutes facts: a second, quieter mutation path is precisely the hole this model exists to close.
   - **ADDING is "grants on demand" (owner)** — the grants machine hands an entity over on a genuine
     acquisition; WB hands the same entity over on a click. From the model's side they are the SAME event, so a
     WB addition is a genuine acquisition: same DOMAIN fact, every consumer reacting identically.
   - ⚠ **REMOVING is the mirror concept — *"grants that remove", kind of thing (owner) — and we do not have any.***
     That names it exactly: WB removal is an inverse grant, and the machine has no such notion, so the remove
     side cannot lean on an existing precedent the way the add side can. ⛔ The answer is NOT to build
     grant-removal machinery for WB's sake — it is that the removal **FACT** must exist and be emitted, the same
     fact a genuine in-play removal would announce. Its facts are THINNEST
     exactly where WB is most arbitrary, because normal gameplay rarely removes: a tech is monotonic in play but
     WB can un-research one; the same holds for anything else acquired-and-kept. So the pass must expect to FIND
     MISSING removal facts rather than merely route existing ones — and per [event-spine.md](../../specs/event-spine.md)
     (*"add all the events, ever"*), the answer to a missing one is to add it.
   So the WB pass is not "check WB still works" — it is: **does every arbitrary WB mutation, in BOTH directions,
   go through a genuine emitting choke point?** (It reaches deep: it is among the Python unit-kill sites,
   and it drove paths the docs wrongly credited elsewhere — the retired modifier-recalc was claimed to have a
   WorldBuilder invoker and did not.)
2. **Python = boundary redesign + fix values.** Do NOT pull Python-authoritative gameplay into the DLL.
3. **⛔ ART IS OUT OF SCOPE — leave it alone (owner).** The art defines (`CIV4ArtDefines_*`), their `ART_`/
   `EFFECT_` tag ids, and the asset files are UNTOUCHED by this rework: JSON carries only the tag id, the
   definitions stay in the ART XML, and `ARTFILEMGR` keeps resolving them ([json.md §7](../../specs/json.md),
   [naming.md](../../specs/naming.md)). This includes **not** cleaning up art that becomes orphaned when a
   consumer is removed — an unreferenced define is inert, and pruning it is neither this rework's job nor a
   tidiness licence. Same standing as TXT: an unmigrated system boundary, not a gap
   ([patterns.md § THE PYTHON READ BOUNDARY](../../architecture/patterns.md)).
4. **NOT failures — deliberate, owner-ruled permanent carve-outs:** the golden-age YIELD-EFFECT member-mirror
   ([golden-age.md](../../reference/golden-age.md)); the mission-CONCEPT unification and the Python-authoritative
   outcome hooks; random EVENTS; Revolution. *(The `CvOutcome` DATA itself IS migrated to JSON.)* Plus:
   - **HEAL stays KEEP-legacy.** *"As long as units and buildings still heal units, I'm happy; it needs a broader
     rework to make sense anyway."* So #430 builds no heal model, and heal is **EXEMPT from exact parity** — the
     acceptance bar is behavioural and two-sided: heal must not be LOST (a rate collapsing toward 0) and must not
     be WILDLY OVERPOWERED (a ×100 or double-count).
     ⛔ **THE CARVE-OUT IS THE MECHANIC, NOT THE FEED (owner): *"we don't change how heal works, but we have to
     change how the data is fed to heal."*** So a heal READ moves onto the rebuilt info like every other read —
     the keyed rows through the ONE collect, the support amount through its kind — while the arithmetic that
     consumes them is left exactly as it stands. ⚠ Reading "KEEP-legacy" as "leave the heal reads alone" is the
     misreading to avoid: it would strand them on getters that no longer exist, which is not keeping the mechanic
     working, it is breaking it. ⚑ And the feed is precisely where this carve-out's own acceptance bar bites —
     the amounts are ×100, so the reader reduces at its point of use; that IS the "wildly overpowered" half of
     the bar, not a separate concern. ⚑ **The redesign is a COUPLED CLUSTER, not standalone:** it
     depends on dismantling the unit-combat complexity (the per-UnitCombat heal-as-type dimension is a symptom of
     it) and on tags being fleshed out. Doing it earlier would just re-encode the current mess — a dependency
     order, not a punt.
   - **The CORPORATION system** — a principle-level rework of the corp MODEL (owner: *"don't like how corporations
     work in principle"*), its own deliberate piece of work; the corp-HQ revenue rides it.
   - **UNIT MAINTENANCE** — owner: *"we want to have unit maintenance make more sense in the future, so we leave
     it like this for now"*. Per-unit upkeep is FLAT today (base + flat extra) because the percentage stages that
     multiplied it were removed ([superseded-ideas](../../architecture/superseded-ideas.md) #26) — that cleared an
     incoherent model, it did not settle a coherent one. ⚑ The named exhibit of what the redesign must resolve:
     **FREE UNITS did not take Size Matters into account**, so the free-unit allowance counted units while SM
     changed what a unit IS. ⛔ Until that rework is taken, do not re-introduce a per-unit upkeep multiplier of
     any kind — including "restoring" the SM one, which was NOT the thing that broke.
   - **NPC build-lockdown** (`stronglyRestricted`) — ⚖ losing it during the enabler flip is **ACCEPTED**: *"I truly
     don't care about NPC barbarians or neanderthals being locked down or not… we may after all want to do it in a
     better manner anyway."* So it is never a reason to preserve a legacy gate clause.
5. **⛔ PURELY-PYTHON, NEVER-XML EFFECTS ARE OUT OF SCOPE (owner).** #430 migrates the **XML-dealt-with surface**
   (XML data + the DLL machinery that reads it). Gameplay living ONLY in Python that reads NO XML field is a
   separate surface the cascade never touches — the hardcoded per-turn wonder spawns/grants in `CvEventManager`,
   and effects that are structurally INEXPRESSIBLE in the model (a culture burst when a unit dies has no home in
   the `grants`/modifier/enabler vocabulary, which declares provisions on standard triggers, not arbitrary event
   reactions). ⚑ **Their absence from any migration inventory is the scope boundary working, not a gap** — and
   there is **no exposure by construction**: the grants machine applies only what is in the XML-derived JSON, so
   an effect that was never in XML never enters the JSON and can never double-up or be lost.
