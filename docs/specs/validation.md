# Validation — the live-verification discipline

> **Project-specific (owner).** Validation is how a migration work item is proven DONE: its effect is observed in the
> running game, through the endpoints, on a real save and a real turn. It is not a separate test suite you run on the
> side — it is the acceptance discipline the legacy-removal work answers to. Most of it retires when the migration is
> done. (Specs aren't permanent; they exist so
> agents don't get yoinked. This one lives in the **one** specs surface, never a siloed project folder — that concept
> failed catastrophically.)
>
> **⚑ Parity and shadow are CLOSED — do not re-run them ([DEC-verify-in-game-not-reshadow](../architecture/decisions.md#dec-verify-in-game-not-reshadow)).**
> The confirmation the migration needed already exists: the event-spine STRUCTURE was strongly verified, and the
> shadow strongly verified the CALCULATIONS reach the right numbers. That is finished and sufficient. The original
> readJson-direct shadow read JSON straight, bypassing the loaded info objects — a bypass that must not be repeated.
> Re-invoking parity/shadow, or framing remaining work as "drive shadow to zero," sends agents rollerskating back into
> offline validation instead of building the real info-object-backed runtime. The legacy XML Info classes are archived
> as the red ratchet ([DEC-red-ratchet](../architecture/decisions.md#dec-red-ratchet)), so there is no legacy oracle
> left to shadow against on the cut surfaces anyway. **What remains is live verification.**

> **⛔ THERE IS NO COMPARISON TWIN, AND NONE COMES BACK.** The `*Legacy` oracle getters and the
> `*Recomputed`/`*Leg` `/computed` twin fields are GONE — zero such symbols remain in `Sources/` — and their
> removal was one of the reasons the hard rebuild was forced (owner): agents had learned to cheat the comparison
> by feeding legacy-computed data into the cascade calc so it could not fail (now banned outright by
> [DEC-calc-zero-ride-in](../architecture/decisions.md#dec-calc-zero-ride-in)), and once the legacy accumulators
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
([DEC-done-is-observable](../architecture/decisions.md#dec-done-is-observable)). This observation is PROGRAMMATIC: the
existing `/computed` decomposition censuses already expose the real engine values (yields, wellbeing, tally, unit skills,
heal, unit promotions) as game-thread snapshots, so a manifestation check is a poll-and-assert against them — never
eyeballing the screen. A value not yet on the surface (e.g. free-promotion grants, grants-applied) must be EMITTED
first; emitting it is step one of that item's fix.

**Turn time is the performance half of acceptance.** The second live signal is the wall clock, not a counter:
**≤ 2 minutes per turn** on the standing late-game save
([DEC-turn-time-is-king](../architecture/decisions.md#dec-turn-time-is-king)), with the process-memory gauge
beside it under the 32-bit ceiling — a gauge whose route went with the purge and which needs re-emitting
([memory-footprint.md](../reference/memory-footprint.md)). A
read is an unconditional bare fetch and the only work on the turn path is APPLYING a moved source's deposits, so
per-turn cost tracks what CHANGED — event volume, which is already visible on the spine — never what EXISTS. A
regression shows up as turn time; the spine's event stream names what drove it. Numbers gathered while any legacy calc still runs on a
hot read path are poisoned and prove nothing
([DEC-legacy-decache-poisons-perf](../architecture/decisions.md#dec-legacy-decache-poisons-perf)).

## The observation surface

Live verification reads the HTTP surface ([http-endpoints.md](http-endpoints.md)) plus the spine-written logs
([observability.md](../reference/observability.md)).

⚠ **The route surface is barely built.** The route table was purged and is defined with the access surface (⛔ the
[roadmap's OPEN ITEM](../plans/structural-cleanup/roadmap.md#-the-open-item--the-access-surface)), so a
manifestation poll reads the SPINE-WRITTEN LOGS plus the four stored-side DECOMPOSITION censuses that survive
(`/computed/cascade/packages`, `/computed/enabler/operating`, `/computed/city/yield`, `/computed/capabilities` —
[http-endpoints.md](http-endpoints.md)). ⛔ Their `oracle` twins are DEAD and must not be run as evidence nor
rebuilt — an endpoint cannot replay the event chain, so a recompute served beside the stored value answers a
number that was never comparable ([superseded-ideas #33](../architecture/superseded-ideas.md)). Everything else a
check wants must be EMITTED first; emitting it is step one of that item's fix, and a value not on the surface is
not verifiable — never eyeballed off the screen as a substitute.

⛔ **Verification pressure is NOT a licence to restore a route.** An endpoint is a LIVE CONSUMER: a legacy member
whose only remaining reader is a route survives the delete-driven cut invisibly, because the compiler census
cannot tell a real use from a route's. "I just need to see this value" is precisely how legacy gets preserved —
EMIT it as a spine event instead ([http-endpoints.md](http-endpoints.md)).

**The bar is ATTRIBUTION + a showable diff, not a bit-exact number.** Every value must be ATTRIBUTED — we can name
where it comes from on both sides — and any diff must be SHOWABLE (total-observability). Where the model deliberately
diverges from legacy (e.g. building free-specialists moved into the specialists bucket via the output-seam rather than
riding along with buildings as legacy did), the diff is an intentional, attributed one — named and shown, never a
mystery. **The completeness discipline survives parity's retirement and outranks it**
([DEC-represent-dont-fit](../architecture/decisions.md#dec-represent-dont-fit)): *"the real reason for matching was to
make sure the cascade evaluates everything legacy does."* So a divergence is your signal that you have NOT yet found a
source the engine uses — the job is to FIND it (map it to a named legacy source with numbers on both sides), never to
change the legacy/curator/numbers to make it reconcile. "It doesn't reconcile, so I'll change the numbers" is the
banned shortcut — 99% of the time the value DOES reconcile once the missing source is found by reading ALL the writers.

> **⛔ THE SPEC LEADS — NOW, not after some later flip (owner).** The ground truth is the **JSON spec**, not the
> legacy engine: where the current code and the spec disagree, the spec is right and the code is the defect. There
> is no "mirror the engine faithfully now, diverge later" phase — that framing died with the thing it described.
> **We are not mirroring the legacy surface, we are NUKING it** (owner): the legacy getters are a DELETION list,
> not a contract to reproduce ([DEC-new-getter-surface](../architecture/decisions.md#dec-new-getter-surface)), and
> "this is how it works today" carries no weight by itself — only a live, named reason does (a spec requirement,
> the EXE calling in, save state, an ordering the engine genuinely depends on). A change that alters observable
> behaviour is a FACT to state plainly and weigh, never a thing to defer.
>
> **⛔ Data migration is NEVER deferred ([DEC-data-first](../architecture/decisions.md#dec-data-first)).** ANY known
> curator/JSON item not yet updated — a legacy field not converted, a reclassification not applied, a legacy shape
> still emitted — is the highest-priority task, handled BEFORE any downstream work. A deferred data item forces every
> downstream consumer to ASSUME its eventual shape, and an assumption in this codebase is the kraken's shortcut
> ([DEC-no-guessing], [DEC-kraken]). Finish the data (curators + JSON) first, then build on solid, known data. This is
> the specific case of the general rule that nothing is "deferred" ([DEC-no-deferred](../architecture/decisions.md#dec-no-deferred)).
>
> **⛔ AND IT IS AN ORDERING WITHIN THE WORK ITEM, NOT ONLY A PRIORITY BETWEEN THEM — THE CURATOR UPDATE GOES
> FIRST, ALWAYS (owner): *"every time we have waited with a curator update, without fail, it has bitten me."***
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
> change numbers, and this exception must never be cited to skip the mapping work. (The one sanctioned instance + its
> evidence: [legacy-value-calc-map §1.5](../reference/legacy-value-calc-map.md).)

## ⛔ A GATE THAT IS NOT HONOURED MAKES EVERY DATA CHECK MEANINGLESS (owner)

> *"We cannot find curation errors when we literally do not honour the specced gates."*

**Gate ENFORCEMENT is a PRECONDITION of validating the data, never a parallel workstream.** A gate the engine does
not consult answers YES to everything, so every entity behind it looks correct and every check run against it
passes — the data is being measured with an instrument that cannot report a fault. ⇒ Fix the enforcement FIRST;
only then does a data divergence mean anything.

⚑ **The DATA is curated to be correct (owner) — it was inadvertently tested against the previous branch's
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

## The pollution guardrail — engine-computed data never rides in

**The cascade computes ALL its active state itself and never reads a legacy COMPUTED output as an input**
([DEC-calc-zero-ride-in](../architecture/decisions.md#dec-calc-zero-ride-in)). Engine-calculated data may enter ONLY at
the observation boundary (the manifestation check), never as a cascade input — otherwise the cascade would be
validated against itself. The trap is the CAMOUFLAGED case: a DERIVED value masquerading as raw state — above all a
building's ACTIVE/DORMANT verdict, which is a pure function of `requires.operate` and must be COMPUTED by the enabler,
never read from the engine.

> **⛔ In the LIVE cascade this is DISCIPLINE, not a structural wall — so be SUPER-PEDANTIC.** The in-engine cascade
> has **direct access** to every live object +
> computed getter, so nothing *stops* it reading a legacy computed output (`isActiveBuilding`/dormancy, connected-bonus
> resolution, `getYieldRate100`, …) as an input by accident. The modifier reads the **enabler's** active state, not the
> engine's. Direct access makes this trivial to violate; the extra vigilance IS the guardrail.

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
  ([DEC-new-getter-surface](../architecture/decisions.md#dec-new-getter-surface)), so every layer observes the same
  values because it reads the same slots, not because a legacy contract was held stable underneath it.
- **⛔ An end turn does NOT confirm a STRUCTURE** ([DEC-structure-before-shadow](../architecture/decisions.md#dec-structure-before-shadow)).
  A per-change observation produces false confirmation even on a wrong structure — a gameobject side-table can read
  back green yet be on the wrong structural path. So **stand up the proper, spec-faithful structure FIRST**; the
  endpoint checks then verify *behaviour through the surviving/replaced engine*, never *structure*. Structure is gated
  by **fidelity to the spec**, not by a green endpoint.

## Run results stay OUT of the docs

Divergence counts, sweep checklists, per-run numbers — none of it belongs in the durable docs
([DEC-no-parity-results-in-docs](../architecture/decisions.md#dec-no-parity-results-in-docs)). Stale results poison
contexts: an agent fixates on a number and misdiagnoses (a ~1100-building enable diff was repeatedly misattributed to a
band-model change it had nothing to do with). The spec says what the model **is**; the curator code + the live
endpoints prove it; the result is ephemeral and stays ephemeral.

## ⛔ A REFERENCE NUMBER IS A SMELL TEST, NEVER A TARGET (owner)

> *"I already regret talking about baselines, because every time you use it as a target, and mangle implementation
> to reach the target, instead of ensuring implementation is correct."*

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

⚠ It is the sibling of [DEC-represent-dont-fit](../architecture/decisions.md#dec-represent-dont-fit): that one bans
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
- [observability.md](../reference/observability.md) — the operational log/endpoint surface the polls read.
- [enabler.md](enabler.md) · [modifier.md](modifier.md) · [tally.md](tally.md) — the machines this verifies.
