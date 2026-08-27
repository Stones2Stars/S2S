# Validation — the live-verification discipline

> **Project-specific.** Validation is how a migration work item is proven DONE: its effect is observed in the
> running game, through the endpoints, on a real save and a real turn — not a side test suite, but the acceptance
> discipline legacy-removal work answers to. Most of it retires when the migration is done. (Specs aren't permanent;
> they exist so agents don't get yoinked. This one lives in the **one** specs surface, never a siloed project
> folder — that concept failed catastrophically.)
>
> **⚑ Parity and shadow are CLOSED — do not re-run them (parity and shadow are closed).**
> The confirmation the migration needed already exists: the event-spine STRUCTURE was strongly verified, and the
> shadow strongly verified the CALCULATIONS reach the right numbers. That is finished and sufficient. The original
> readJson-direct shadow read JSON straight, bypassing the loaded info objects — a bypass that must not be repeated.
> Re-invoking parity/shadow, or framing remaining work as "drive shadow to zero," sends agents rollerskating back into
> offline validation instead of building the real info-object-backed runtime. The legacy XML Info classes are archived
> as the red ratchet ([the red ratchet](../../AGENTS.md#build-and-test)), so there is no legacy oracle
> left to shadow against on the cut surfaces anyway. **What remains is live verification.**

> **⛔ THERE IS NO COMPARISON TWIN, AND NONE COMES BACK.** The `*Legacy` oracle getters and the
> `*Recomputed`/`*Leg` `/computed` twin fields are GONE — zero such symbols remain in `Sources/` — and their
> removal was one of the reasons the hard rebuild was forced: agents had learned to cheat the comparison
> by feeding legacy-computed data into the cascade calc so it could not fail (now banned outright by
> [the pollution guardrail](#the-pollution-guardrail--engine-computed-data-never-rides-in)), and once the legacy accumulators
> were deleted both sides read the same derivation, so the check could never turn red anyway. The problem is
> solved STRUCTURALLY, by the surface not existing ([superseded-ideas](../architecture/superseded-ideas.md) #17) —
> not by a standing rule to remember. **Never re-add a comparison getter or a `/computed` twin field.**
>
> ⚠ **That bans the SAME-DERIVATION twin, NOT verification.** ⛔ But the shape once sanctioned in its place —
> event-built state against a fresh recompute-from-source, served on two endpoints and diffed outside the DLL —
> is ALSO dead: an endpoint cannot replay the event chain, so its recompute side is not a second derivation of
> the same quantity, and diffing it produces confident nonsense at scale
> ([superseded-ideas #33](../architecture/superseded-ideas.md)).
> **The live signal is the THREE-LEG check: the LOGS (what landed), the JSON INFO (what the source is authored to
> deposit) and WHAT STATE EXPECTS (who holds it, which gates hold, what the counts are) — all three agreeing,
> attributed to a named source with numbers.** Two legs is not a check. Beside it: served-value SANITY, and the
> COMPILER CENSUS (a deleted member's consumers are compile errors).

**Done = observable in the running game.** A work item is complete only when its effect is observable in the RUNNING
GAME via an endpoint poll — never because "the code path exists" or "the data loads." "Straight up missing" means it
does not show in-game even if it loads; the break is then downstream, in apply/display, and is found by the poll, not
asserted. Acceptance is an endpoint-observable pass/fail on a real save, a real turn
(done = observable in the running game). This observation is PROGRAMMATIC: the
existing `/computed` decomposition censuses already expose the real engine values (yields, wellbeing, tally, unit skills,
heal, unit promotions) as game-thread snapshots, so a manifestation check is a poll-and-assert against them — never
eyeballing the screen. A value not yet on the surface (e.g. free-promotion grants, grants-applied) must be EMITTED
first; emitting it is step one of that item's fix.

**⚖ AND IT IS A SNAPSHOT, NEVER A PROPERTY — NO EVALUATION PATH IS EVER "DONE".** The reason is structural rather than cautious: the classification
registries and the modifier families are OPEN BY DESIGN
([the classification-infos registry](json/09-classification-unit-skillstagsstate-building-a.md#8-classification--unit-skillstagsstate-building-attributes--empire-capabilities)), so a valuation that reads
every source today becomes incomplete the moment data authors a new one — with no code change, nothing failing,
and no build that could name it. Completeness DECAYS on a data edit.
⇒ **So the deliverable is the INSTRUMENT, not the claim**: the load-time censuses (`unkinded-member`, the FK and
unconsumed-key counts), attribution to a named source with numbers, and the three-leg check
([http-endpoints.md](http-endpoints.md)) keep working as the data moves; a completion statement does not.
⛔ Report an evaluation path as *"no known divergence, on this save, on this turn"* — never as done, which
asserts a property the model cannot have. It is the same reason a remembered figure is a smell test rather than
a target ([a reference number is a smell test](#-a-reference-number-is-a-smell-test-never-a-target)).

**Turn time is the performance half of acceptance.** The second live signal is the wall clock, not a counter:
**≤ 2 minutes per turn** on the standing late-game save
([turn time is king](../cascade/16-package-model.md#-the-per-scope-package-model--the-cascades-founding-design-1-stated-as-cache-architecture)), with the process-memory gauge
beside it under the 32-bit ceiling — a gauge whose route went with the purge and which needs re-emitting
([memory-footprint.md](../reference/memory-footprint.md)). A
read is an unconditional bare fetch and the only work on the turn path is APPLYING a moved source's deposits, so
per-turn cost tracks what CHANGED — event volume, which is already visible on the spine — never what EXISTS. A
regression shows up as turn time; the spine's event stream names what drove it. Numbers gathered while any legacy calc still runs on a
hot read path are poisoned and prove nothing
([legacy decache poisons perf measurement](../cascade/03-no-staleness-no-selfheal.md#-legacy-decache-poisons-perf-measurement--and-converts-an-ai-loop-into-a-hang)).

**The MEMORY hunt stays PARKED by the same sequencing ruling.** Chasing per-turn memory is pointless until legacy
is gone and everything runs on the cascade + enabler, because the growth is turn-processing-borne and that
processing is what the rebuild replaces ([memory-footprint.md](../reference/memory-footprint.md)). The perf hunt
resumes only after the caches are event-wired and the game runs behaviourally as it used to.

## The observation surface

Live verification reads the HTTP surface ([http-endpoints.md](http-endpoints.md)) plus the spine-written logs
([spine.md](../spine.md)).

⚠ **The route surface is barely built.** The route table was purged and is defined with the access surface (⛔ the
still-open work of building ONE new uniform getter set and disconnecting the legacy channel-shaped getters —
[build a new getter surface, never widen a legacy one](../architecture/patterns/05-the-two-read-roles-one-grammar-two.md#-the-two-read-roles--one-grammar-two-answers),
[patterns.md § THE TWO READ ROLES](../architecture/patterns.md)), so a
manifestation poll reads the SPINE-WRITTEN LOGS plus the stored-side DECOMPOSITION censuses that survive. ⚠ Their
number is deliberately NOT stated here — an enumerated route list in a doc has drifted twice: the `ROUTES` table in
`Sources/Tools/CvHttpServer.cpp::handleRequest` is their census, and `GET /computed` serves the live index
([http-endpoints.md](http-endpoints.md)). ⛔ Their `oracle` twins are DEAD for the reason above (§ THERE IS NO
COMPARISON TWIN) and must not be run as evidence nor rebuilt. Everything else a check wants must be EMITTED
first; emitting it is step one of that item's fix, and a value not on the surface is not verifiable — never
eyeballed off the screen as a substitute.

⛔ **Verification pressure is NOT a licence to restore a route.** An endpoint is a LIVE CONSUMER: a legacy member
whose only remaining reader is a route survives the delete-driven cut invisibly, because the compiler census
cannot tell a real use from a route's. "I just need to see this value" is precisely how legacy gets preserved —
EMIT it as a spine event instead ([http-endpoints.md](http-endpoints.md)).

**The bar is ATTRIBUTION + a showable diff, not a bit-exact number.** Every value must be ATTRIBUTED — we can name
where it comes from on both sides — and any diff must be SHOWABLE (total-observability). Where the model deliberately
diverges from legacy (e.g. building free-specialists moved into the specialists bucket via the output-seam rather than
riding along with buildings as legacy did), the diff is an intentional, attributed one — named and shown, never a
mystery. **The completeness discipline survives parity's retirement and outranks it**
([represent, don't fit](#the-observation-surface)): *"the real reason for matching was to
make sure the cascade evaluates everything legacy does."* So a divergence is your signal that you have NOT yet found a
source the engine uses — the job is to FIND it (map it to a named legacy source with numbers on both sides), never to
change the legacy/curator/numbers to make it reconcile. "It doesn't reconcile, so I'll change the numbers" is the
banned shortcut — 99% of the time the value DOES reconcile once the missing source is found by reading ALL the writers.

> **⛔ THE SPEC LEADS — NOW, not after some later flip.** The ground truth is the **JSON spec**, not the
> legacy engine: where the current code and the spec disagree, the spec is right and the code is the defect. There
> is no "mirror the engine faithfully now, diverge later" phase — that framing died with the thing it described.
> **We are not mirroring the legacy surface, we are NUKING it**: the legacy getters are a DELETION list,
> not a contract to reproduce ([build a new getter surface, never widen a legacy one](../architecture/patterns/05-the-two-read-roles-one-grammar-two.md#-the-two-read-roles--one-grammar-two-answers)), and
> "this is how it works today" carries no weight by itself — only a live, named reason does (a spec requirement,
> the EXE calling in, save state, an ordering the engine genuinely depends on). A change that alters observable
> behaviour is a FACT to state plainly and weigh, never a thing to defer.
>
> **⛔ Data migration is NEVER deferred ([data migration is never deferred](#the-observation-surface)).** ANY known
> curator/JSON item not yet updated — a legacy field not converted, a reclassification not applied, a legacy shape
> still emitted — is the highest-priority task, handled BEFORE any downstream work. A deferred data item forces every
> downstream consumer to ASSUME its eventual shape, and an assumption in this codebase is the kraken's shortcut
> ([the no-guessing rule](../../AGENTS.md#conduct), [the kraken rule](../../AGENTS.md#conduct)). Finish the data (curators + JSON) first, then build on solid, known data. This is
> the specific case of the general rule that nothing is "deferred" (["deferred" is banned](../../AGENTS.md#design)).
>
> **⛔ AND IT IS AN ORDERING WITHIN THE WORK ITEM, NOT ONLY A PRIORITY BETWEEN THEM — THE CURATOR UPDATE GOES
> FIRST, ALWAYS — every deferral of a curator update has, without fail, bitten later.**
> Not "usually", not "as a rule of thumb" — the owner reports it has cost them EVERY time, which is why this is
> stated as a sequence rather than a preference. ⇒ When a work item touches both the data and its consumer,
> the curator change + regen land BEFORE the engine side, even when the engine side is the part that looks
> urgent and the curator change looks trivial.
> ⚑ **The mechanism, so it is not mistaken for ceremony:** an engine consumer written against the data's
> EVENTUAL shape is written against an assumption, and the assumption is invisible once it compiles — so the
> two halves drift and the drift surfaces later, as a wrong number nobody can attribute. Landing the data first
> makes the consumer's input a FACT it can be checked against.
> ⚠ It also decides what a half-finished item leaves behind: data ahead of its consumer is INERT and reported
> (the `unkinded-member` / unconsumed censuses name it on every load), while a consumer ahead of its data is a
> silent wrong answer. Of the two incomplete states, only the first announces itself.
>
> **⛔ Touching legacy is a LAST RESORT, never an agent's judgement call.** Only after a source is FULLY mapped — every
> writer read, the value reproduced and shown to be genuinely non-deterministic (history/order-dependent, demonstrated
> across MANY instances, with the engine code proving why) — may "streamline the legacy to be deterministic" even be
> *proposed*, and it then requires explicit owner authorization for that specific case. It is NOT a general licence to
> change numbers, and this exception must never be cited to skip the mapping work.

## ⛔ A GATE THAT IS NOT HONOURED MAKES EVERY DATA CHECK MEANINGLESS

> Curation errors cannot be found while the specced gates are not honoured.

**Gate ENFORCEMENT is a PRECONDITION of validating the data, never a parallel workstream.** A gate the engine does
not consult answers YES to everything, so every entity behind it looks correct and every check run against it
passes — the data is being measured with an instrument that cannot report a fault. ⇒ Fix the enforcement FIRST;
only then does a data divergence mean anything.

⚑ **The DATA is curated to be correct — it was inadvertently tested against the previous branch's
rollerskating behaviour**, so a surviving wrong verdict is the ENGINE failing to honour what the data says, not
the data needing another pass. ⛔ So the reflex to "fix" a wrong offer by re-authoring the entity is backwards
here, and it is the move that would bake the engine's gap into the data permanently.

⛔ **The failure is SILENTLY PERMISSIVE in every one of its forms, which is why it needs a census rather than
vigilance** — none of them errors, and each leaves a gate that can only answer yes:
- a predicate the parser produces and the evaluator has no case for (an unknown predicate is IGNORED, never
  false — [json.md §3.5](json.md), so it passes);
- a re-derived verdict that drops the CARVE-OUTS its engine original carried (a game option that switches a
  limit off);
- a constraint that is authored, parsed and held but read by NOBODY;
- a gate stage a domain never runs, leaving its members LISTED ([enabler.md §8](enabler.md)).
⚠ **None of these is visible to the compiler**, and no diff of two engine-side reads can see them either — both
sides share the evaluator, so a gate that always says yes says yes on both.

<a id="playability-not-a-gate"></a>
## ⛔ Neither playability nor compiling is a gate on removing legacy

**"It would break the game / needs a playtest first" is a rollerskate excuse, and green is the bait**
([neither playability nor compiling gates removing legacy](#playability-not-a-gate)): chasing it is what makes
an agent shoehorn the new implementation into legacy, so everything goes in place FIRST and the tree compiles at
the END, as the result of the completed rewire. A red tree during a cut is an ACCEPTED state, never a defect to
fix by re-attaching what was archived (I could not possibly care less if this compiles; having a clean
slate to do this right is the target.), and equally **"get it building" is not a milestone** — a green tree is
the by-product of a finished rewire, not evidence of progress toward one.

**While the tree is red, WIRED OUTRANKS CORRECT** — a machine's facts emitted, consumer registered and surface
reachable beats knowing its output is right (it is more important that triggers are wired than knowing
if they give the correct result.), because correctness is endpoint-observable and so cannot be tested until
green. A wrong wiring is removed on sight, with an interim wrong number accepted.

⚠ **That SEQUENCES the acceptance bar, it does not relax it** — and it lapses the moment the tree builds, when
correctness becomes testable and therefore owed. Removal is DELETE-DRIVEN: hard-delete the member (save-safe via
`savemigration.txt`), and the COMPILER is the census — every consumer still on it is a compile error, so a
compile error is a WORKLIST ENTRY, never a reason to re-shape what is being built. Done = compiler-complete
rewire onto the cascade + endpoint-observable correctness on a LOADED save (not *playing*). The only legacy that
stays is an owner-ruled carve-out.

> **⚖ A WRONG WIRING IS REMOVED ON SIGHT, AND AN INTERIM WRONG NUMBER IS ACCEPTABLE: *"it does not
> really matter if we temporarily doublecount, it is more important that things are wired correctly, and wrong
> wirings are removed on sight."*** This extends the same ruling from legacy CODE to legacy STRUCTURE.
> ⛔ **So a double-count is NEVER a reason to keep a second maintenance surface alive, and it is never a thing to
> weigh.** The tempting move is to preserve the wrong wiring because removing it makes a number visibly wrong for
> a while — which is exactly backwards: the wrong number is temporary and loud, while the second surface is
> permanent and quiet, and it is what the next consumer route silently doubles against.
> ⚑ **The shape to recognise:** a mutation choke point that MAINTAINS a derived store directly beside the fact it
> emits. The mutation site owns the SOURCE, never the store
> ([a context dictionary is a spine consumer](../cascade/11-context-stores-vs-forwards.md#what-a-context-stores-vs-forwards---a-context-is-an-event-built-store-not-a-forwarding-facade)); a store is maintained by its
> OWN consumer, off the fact. ⚠ The tell that one is present is a consumer deliberately IGNORING a fact "because
> the choke point already applied it" — that skip is the compensation, and it fails the moment anything else
> routes on the same fact.
> ⛔ **Do not record one as a todo entry.** Finding it IS the work item
> ([an event gap is closed the moment it is found](../spine/03-the-domain-emit-surface-every-fact/01-a-fact-names-the-happening.md#-a-fact-names-the-happening--something-changed-is-not-a-fact)); it closes in the same
> change, and the compensating skip goes with it.

**⚖ AN AI *WEIGHT* IS NOT A CORRECTNESS GATE — THE AI HAS TO FUNCTION, AND BALANCE COMES AFTER:**
AI weights get figured out down the line; AI needs to function, then we balance it later."* This is the one
place "wired outranks correct" does NOT lapse at green: it scopes that sequencing to a RED tree, so an agent
reading it literally could conclude AI weights must now be right the moment the tree builds, and stall a
conversion on tuning. They must not.

⚑ **The line is WIRING vs MAGNITUDE, and only the first is owed now:**
- a read pointed at the WRONG SURFACE — legacy, a frozen accumulator, a value nothing maintains — is a defect,
  fixed on sight like any other consumer;
- the resulting COEFFICIENT being well-tuned is the balance pass, and is nobody's business during a cut.

⇒ So converting an AI read is DONE when it reads the right thing. State the behaviour change plainly (above) and
move on; do not re-tune the surrounding arithmetic to keep a number where it used to sit, which is
[a reference number is a smell test](#-a-reference-number-is-a-smell-test-never-a-target) in AI clothing.
⚠ It does NOT license leaving an AI read on legacy because "the weight is wrong anyway" — that inverts it.

## The pollution guardrail — engine-computed data never rides in

**The cascade computes ALL its active state itself and never reads a legacy COMPUTED output as an input**
([the pollution guardrail](#the-pollution-guardrail--engine-computed-data-never-rides-in)). Engine-calculated data may enter ONLY at
the observation boundary (the manifestation check), never as a cascade input — otherwise the cascade would be
validated against itself. The trap is the CAMOUFLAGED case: a DERIVED value masquerading as raw state — above all a
building's ACTIVE/DORMANT verdict, which is a pure function of `requires.operate` and must be COMPUTED by the enabler,
never read from the engine.

> **⛔ In the LIVE cascade this is DISCIPLINE, not a structural wall — so be SUPER-PEDANTIC.** The in-engine cascade
> has **direct access** to every live object +
> computed getter, so nothing *stops* it reading a legacy computed output (`isActiveBuilding`/dormancy, connected-bonus
> resolution, `getYieldRate100`, …) as an input by accident. The modifier reads the **enabler's** active state, not the
> engine's. Direct access makes this trivial to violate; the extra vigilance IS the guardrail.

## Legacy must fail loud, never mask a cascade gap

**Legacy outputs must FAIL LOUD, never be preserved or snuck in via getters/fallbacks.** A realized getter reads
the CASCADE ONLY — no `*Legacy` fallback, no pre-init/what-if legacy path; a cascade gap returns a wrong/empty
value (exposed), never a legacy-correct one (masked). Legacy masking a wrong cascade is WORSE than legacy
failing: the mask hides the defect and defers the fix (the wellbeing panel reading legacy hid a 2× cascade
inflation). Purge legacy **violently** so what is missing/wrong is immediately visible. Blast radius is never a
reason to keep a legacy path alive.

**The legacy XML is REMOVED (the red ratchet — [AGENTS.md § Build And Test](../../AGENTS.md)), so a legacy
fallback cannot even RUN — it is BAIT that substitutes a nonexistent answer and masks the hole**; a realized
gate/getter is therefore a PURE cascade read (the six availability gates carry no `*Legacy` fallback, no
pre-init guard, no what-if path). Corollary of
[neither playability nor compiling is a gate on removing legacy](#playability-not-a-gate)
for the READ surface.

## Cadence — what LOAD verifies vs what END TURN verifies

The static/live split — **readJson = static info data**; the **game object = live state** (the cascade runtime: tally,
event spine, accumulators, all condition *evaluation*) — dictates *when* each thing is observed:

- **LOAD verifies the STATIC + initial setup.** readJson maps the info-level data at load (it never touches a
  `CvGameObject`); the **tally** is a read-only accessor over the object-owned counts ([tally.md](tally.md)); the
  **enabler**'s HAVE set is established from the loaded objects. So **loading a save** and hitting the endpoints is
  enough to confirm readJson did its job and the static/initial state is correct + inspectable. **No turn needed.**
- **END TURN verifies LIVE integration.** Two cases: (1) the engine parts we will not replace can SEE the new cascade
  data; (2) the parts we will replace — `canTrain`/`canConstruct` and the modifier rates — produce the right values in
  the AI's real per-turn calls (end-turn so the AI calls them), observed on the spine — the gate and rate routes are gone.
  Consumers — engine, AI and Python alike — read the NEW uniform parameterized surface
  ([build a new getter surface, never widen a legacy one](../architecture/patterns/05-the-two-read-roles-one-grammar-two.md#-the-two-read-roles--one-grammar-two-answers)), so every layer observes the same
  values because it reads the same slots, not because a legacy contract was held stable underneath it.
- **⛔ An end turn does NOT confirm a STRUCTURE** ([structure before shadow](#cadence--what-load-verifies-vs-what-end-turn-verifies)).
  A per-change observation produces false confirmation even on a wrong structure — a gameobject side-table can read
  back green yet be on the wrong structural path. So **stand up the proper, spec-faithful structure FIRST**; the
  endpoint checks then verify *behaviour through the surviving/replaced engine*, never *structure*. Structure is gated
  by **fidelity to the spec**, not by a green endpoint.

## Run results stay OUT of the docs

Divergence counts, sweep checklists, per-run numbers — none of it belongs in the durable docs
([run results stay out of the docs](#run-results-stay-out-of-the-docs)). Stale results poison
contexts: an agent fixates on a number and misdiagnoses (a ~1100-building enable diff was repeatedly misattributed to a
band-model change it had nothing to do with). The spec says what the model **is**; the curator code + the live
endpoints prove it; the result is ephemeral and stays ephemeral.

## ⛔ A REFERENCE NUMBER IS A SMELL TEST, NEVER A TARGET

> Baselines are regrettable: they get used as a target, and implementation gets mangled to reach the target, instead of ensuring implementation is correct."*

When the owner says what a value **used to be** — "hammers were about 5000", "the tech took 2.5 turns" — that is a
SMELL TEST offered to say *something is wrong, go look*. It is not an acceptance criterion, and nothing is finished
because a number arrived near it.

**⇒ The question is only ever: is this read CORRECT?** A deposit is read or it is not; a slot is maintained or it is
not. Where the number lands once the reads are right is an OUTPUT, and an output that overshoots the remembered
figure is not evidence of a bug any more than one that undershoots is evidence of progress.

⛔ **The failure this bans is subtle and does not look like cheating.** It shows up as: reporting every result as a
percentage of the baseline; hesitating over a correct fix *because* it would move a number past the figure; picking
which defect to chase by which one closes the gap; and stopping once the gap is closed with real defects still
standing. Each reads as diligence, and each is the implementation being bent toward a number.
⚑ **The worked case, and note what the target did to the DIAGNOSIS as well as to the fix:** every result in a
session was reported as a percentage of a remembered baseline, and a candidate fix was then hesitated over because
applying it would push a channel above that figure. Worse, the hesitation came *before* the read was even checked —
and the read turned out to be correct already, so the "gap" being protected was imaginary. **A target does not just
bend the fix; it decides which questions get asked, and in what order.** Check whether the read is right first;
the number is downstream of that and has no vote.

⚠ It is the sibling of [represent, don't fit](#the-observation-surface): that one bans
bending the MECHANIC set to fit the data, this one bans bending the IMPLEMENTATION to fit a remembered value. And it
is why run results stay out of the docs (above) — a number in front of an agent becomes a target whether or not
anyone meant it as one.

## The three observation levels

- **Unit** — a single calc: one modifier value, one enabler gate, one tech's availability.
- **Integration** — a subsystem: a city's full yield re-derived from its plots, a player's happiness.
- **End-to-end** — the whole snapshot: the full game state observed via the endpoints.

## See also

- [http-endpoints.md](http-endpoints.md) — the HTTP transport, its standing invariants, and why the
  stored-vs-oracle routes are dead; the observation surface.
- [spine.md](../spine.md) — the event source and the operational log/endpoint surface the polls read.
- [enabler.md](enabler.md) · [modifier.md](../cascade.md) · [tally.md](tally.md) — the machines this verifies.
