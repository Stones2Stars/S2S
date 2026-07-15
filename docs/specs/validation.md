# Validation — the live-verification discipline

> **Project-specific (owner).** Validation is how a migration work item is proven DONE: its effect is observed in the
> running game, through the endpoints, on a real save and a real turn. It is not a separate test suite you run on the
> side — it is the acceptance discipline the cutover work answers to. Most of it retires when the migration is done;
> StoneBase's spec-compliance role for modders is the part that outlives it. (Specs aren't permanent; they exist so
> agents don't get yoinked. This one lives in the **one** specs surface, never a siloed project folder — that concept
> failed catastrophically.)
>
> **⚑ Parity and shadow are CLOSED — do not re-run them ([DEC-verify-in-game-not-reshadow](../architecture/decisions.md#dec-verify-in-game-not-reshadow)).**
> The confirmation the migration needed already exists: StoneBase strongly verified the event-spine STRUCTURE, and the
> shadow strongly verified the CALCULATIONS reach the right numbers. That is finished and sufficient. The original
> readJson-direct shadow read JSON straight, bypassing the loaded info objects — a bypass that must not be repeated.
> Re-invoking parity/shadow, or framing remaining work as "drive shadow to zero," sends agents rollerskating back into
> offline validation instead of building the real info-object-backed runtime. The legacy XML Info classes are archived
> as the red ratchet ([DEC-red-ratchet](../architecture/decisions.md#dec-red-ratchet)), so there is no legacy oracle
> left to shadow against on the cut surfaces anyway. **What remains is live verification.**

**Done = observable in the running game.** A work item is complete only when its effect is observable in the RUNNING
GAME via an endpoint poll — never because "the code path exists" or "the data loads." "Straight up missing" means it
does not show in-game even if it loads; the break is then downstream, in apply/display, and is found by the poll, not
asserted. Acceptance is an endpoint-observable pass/fail on a real save, a real turn
([DEC-done-is-observable](../architecture/decisions.md#dec-done-is-observable)). This observation is PROGRAMMATIC: the
existing `/computed` oracle endpoints already expose the real engine values (yields, wellbeing, tally, unit skills,
heal, unit promotions) as game-thread snapshots, so a manifestation check is a poll-and-assert against them — never
eyeballing the screen. A value not yet on the surface (e.g. free-promotion grants, grants-applied) must be EMITTED
first; emitting it is step one of that item's fix.

**The `(scope,channel)` calc-count gate.** The second live acceptance test is performance: every calculation logs its
`(scope, channel)`, and the per-turn count is a standing gate and regression tripwire — over ~50k calculations for
anything in a single turn is near-certainly a failure (a blanket recompute has crept back), a quiet turn approaches
zero, steady-state tracks EVENT volume (thousands) not entity count (millions)
([DEC-calc-count-gate](../architecture/decisions.md#dec-calc-count-gate),
[observability.md](../reference/observability.md)).

## The observation surface — `/state` inputs, `/computed` oracle

Live verification reads two endpoint families ([http-endpoints.md](http-endpoints.md)):

- **`/state/*`** — the raw game state (raw inputs only, no computed outputs). **`/state/info?type=X` is the
  INFO-OBJECT check**: the loaded info's edge data must MATCH the entity's authored `Assets/Data` JSON — the
  standing verification that readJson put the data where the machines read it (a diff = a load defect, found
  live, never accepted).
- **`/computed/*`** — the engine's actual values as game-thread snapshots: the per-source yield/commerce
  decomposition (`/computed/cities/yields`), the wellbeing decomposition (`/computed/cities/wellbeing`), the
  `/computed/can*` gate verdicts, `/computed/tally`, `/computed/unitSkills`, `/computed/units/heal`. These are the
  oracle: they show what the game actually computed, so a manifestation check compares the expected effect against the
  real value it produced.

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

> **Mirror, don't redesign ([DEC-mirror-then-redesign](../architecture/decisions.md#dec-mirror-then-redesign)).** The
> migration reproduces the engine's *existing* behaviour exactly. Behavioural redesign — *should this behave this way
> at all?* — is deferred to **post-migration**, never done during it. **Why it matters now:** the port must introduce
> zero side-effects. Once the migration is over the ground truth FLIPS: the **JSON spec** (not the legacy engine)
> becomes authoritative, and *that* is when we — and modders — deliberately diverge from C2C, with StoneBase guarding
> spec-compliance. Mirror the engine to get here; thereafter the spec leads.
>
> **⛔ Data migration is NEVER deferred ([DEC-data-first](../architecture/decisions.md#dec-data-first)).** The strict
> complement of mirror-then-redesign: you defer *redesign*, you **never** defer *data migration*. ANY known
> curator/JSON item not yet updated — a legacy field not converted, a reclassification not applied, a legacy shape
> still emitted — is the highest-priority task, handled BEFORE any downstream work. A deferred data item forces every
> downstream consumer to ASSUME its eventual shape, and an assumption in this codebase is the kraken's shortcut
> ([DEC-no-guessing], [DEC-kraken]). Finish the data (curators + JSON) first, then build on solid, known data. This is
> the specific case of the general rule that nothing is "deferred" ([DEC-no-deferred](../architecture/decisions.md#dec-no-deferred)).
>
> **⛔ Touching legacy is a LAST RESORT, never an agent's judgement call.** Only after a source is FULLY mapped — every
> writer read, the value reproduced and shown to be genuinely non-deterministic (history/order-dependent, demonstrated
> across MANY instances, with the engine code proving why) — may "streamline the legacy to be deterministic" even be
> *proposed*, and it then requires explicit owner authorization for that specific case. It is NOT a general licence to
> change numbers, and this exception must never be cited to skip the mapping work. (The one sanctioned instance + its
> evidence: [legacy-value-calc-map §1.5](../plans/structural-cleanup/legacy-value-calc-map.md).)

## StoneBase — the performance layer and the modder spec-check

StoneBase (the `Sources`-mirroring .NET cascade at `http://localhost:8229`) reached parity and proved the model; that
job is done. It is now **repurposed to two live roles** ([DEC-verify-in-game-not-reshadow](../architecture/decisions.md#dec-verify-in-game-not-reshadow)):

- **The user-visible PERFORMANCE layer.** StoneBase holds an SSE stream on the game's `/events`, ingests the per-turn
  `(scope,channel)` calc-counts, and renders them live (Razor + SignalR dashboard) — the histogram naming the culprit
  scope/channel on a 50k breach. The same dashboard renders the `/computed` oracle values (yields / wellbeing / tally
  / promotions) it already ingests, so **performance (the counts) and correctness (the oracle values) are one live
  window** — the "never look at the game screen" observability bar, realized in one UI.
- **The modder spec-compliance check (its lasting role).** After the migration, when a modder authors/tests a JSON,
  StoneBase tells them whether it violates the spec. It is the single tool for this — superseding every earlier
  attempt (the dead Python dry-calc variants, the first-version .NET validator).

> **⛔ StoneBase FOLLOWS the spec — the authority chain is strictly ONE-WAY: SPEC → StoneBase → engine-oracle**
> ([DEC-stonebase-follows-spec](../architecture/decisions.md#dec-stonebase-follows-spec)). The spec leads; StoneBase
> *implements* it; the engine is only the result-oracle. StoneBase's STRUCTURE is taken from the spec and is never
> reverse-engineered from the engine's ordering (the two-pass GENERATE→GATE model vs the engine's flat
> `canTrain`/`canConstruct` procedure). It follows that a divergence is never resolved by a creative tweak in
> StoneBase: it is a curated-data gap mapped to a named source, or — if the spec is genuinely incomplete — a spec
> change made FIRST, deliberately, then re-implemented. **Same-result is necessary but NOT sufficient**: a green sweep
> over a spec-divergent implementation is the trap. If StoneBase drifts from the spec *and* its output judges the spec,
> the loop self-corrupts — the "multikraken."

## The pollution guardrail — engine-computed data never rides in

**The cascade computes ALL its active state itself and never reads a legacy COMPUTED output as an input**
([DEC-calc-zero-ride-in](../architecture/decisions.md#dec-calc-zero-ride-in)). Engine-calculated data may enter ONLY at
the observation boundary (the manifestation check), never as a cascade input — otherwise the cascade would be
validated against itself. The trap is the CAMOUFLAGED case: a DERIVED value masquerading as raw state — above all a
building's ACTIVE/DORMANT verdict, which is a pure function of `requires.operate` and must be COMPUTED by the enabler,
never read from the engine.

> **⛔ In the LIVE cascade this is DISCIPLINE, not a structural wall — so be SUPER-PEDANTIC.** StoneBase enforces
> zero-ride-in structurally (Clean-Architecture: its cascade domain physically cannot reference the live-state DTOs;
> only the composition root reads the snapshot). The in-engine cascade has **direct access** to every live object +
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
  the AI's real per-turn calls (end-turn so the AI calls them), observed via `/computed/can*` and the rate endpoints.
  Under the computed-getter flip ([cutover.md](../plans/structural-cleanup/cutover.md) — the gate BODIES flip,
  consumers never rewire) the Python layer reads the SAME contracts, so its view is identical by construction.
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

## The three observation levels

- **Unit** — a single calc: one modifier value, one enabler gate, one tech's availability.
- **Integration** — a subsystem: a city's full yield re-derived from its plots, a player's happiness.
- **End-to-end** — the whole snapshot: the full game state observed via the endpoints.

## See also

- [http-endpoints.md](http-endpoints.md) — `/state` (inputs) vs `/computed` (oracle); the observation surface.
- [observability.md](../reference/observability.md) — the observability bar + the `(scope,channel)` calc-count gate.
- [enabler.md](enabler.md) · [modifier.md](modifier.md) · [tally.md](tally.md) — the machines this verifies.
