# Validation — the cascade test suite (dry-calc)

> **Project-specific & temporary (owner).** Validation is the migration's **unit / integration / end-to-end
> tests**: it proves the new cascade reproduces the legacy engine so the legacy C++ maintainers can be deleted
> (**map-before-delete**). When the migration is done, validation in this form is **stone dead** and gets removed —
> *that deletion is the finish line*. We need it **now**. (Specs aren't permanent; they exist so agents don't get
> yoinked — this one rides with the project and dies with it. It lives in the **one** specs surface, never a
> siloed project folder — that concept failed catastrophically.)

## What it is

A **dry-calc**: rebuild the cascade (the three machines — [enabler](enabler.md) / [modifier](modifier.md) /
[tally](tally.md)) **offline from the raw inputs**, compute the *expected* output, and **diff it against the live
engine's actual output**. Our calculator is the test; the engine is the oracle.

- **Inputs** — the curated `Assets/Data/**` JSON + the raw game state from [`/state`](http-endpoints.md)
  (raw inputs only, no computed outputs).
- **Oracle** — the engine's actual values from [`/computed/*`](http-endpoints.md): the per-source yield/commerce
  decomposition (`/computed/cities/yields`) + the `/computed/can*` gate verdicts.
- **Bar — PARITY, full stop (owner ruling 2026-06-23).** Per entity/instance, **0 in-scope mismatches** — *exact*,
  not "close / same ballpark." There is **no tolerance band and no agent grading of acceptability** — that framing
  (and the retired six-rung "care scale") was constantly abused to wave a mismatch through as good-enough. No bug has
  surfaced in any actual legacy *calculation*, so the math matches; **a divergence is therefore a data-collection
  gap** — a source the cascade didn't gather — never a formula difference. Map it to the named source and close it.
  Out-of-scope (depends on a lower, not-yet-validated layer — e.g. a tech needing a `BUILDING_` prereq) is
  **deferred: shown, never silently dropped**.
- **⛔ PARITY = full ATTRIBUTION + a showable diff, NOT bit-exact (owner clarification 2026-06-30).** "Parity" does
  **not** mean the cascade reproduces the legacy number to the bit. It means **every source is ATTRIBUTED** — we can
  name where each value comes from on both sides — and **any diff is SHOWABLE** (total-observability). **Bit-exact is
  impossible by design** where the model deliberately diverges: e.g. building free-specialists were **moved into the
  specialists bucket** (instead of riding along with buildings as legacy does), an accepted structural change with a
  known commerce overshoot. **StoneBase PROVED we can reach parity in this sense** (every source attributed, every diff
  named) — that is the achievable bar, and the in-DLL shadow's job is to **reproduce StoneBase's attribution + its
  (accepted, intentional) diffs**, not to drive a bit-exact 0 over a model that intentionally differs. So: the `0` of
  "0 in-scope mismatches" is for the **faithful-mirror** sources; the **intentional** model changes are out-of-scope
  diffs that are attributed + shown, never chased to zero.

> **Mirror, don't redesign (`DEC-mirror-then-redesign`).** The migration reproduces the engine's *existing*
> behaviour exactly. Behavioural redesign — *should this behave this way at all?* (e.g. should blackened-skies dorm
> an observatory) — is deferred to **post-migration**, never done during it.
>
> **Why parity is non-negotiable *now* (owner 2026-06-25).** The pedantry exists so the initial port introduces
> **zero side-effects** — the new cascade must do exactly what C2C did, no surprises. Once the migration is over the
> ground truth FLIPS: the **JSON spec** (not the legacy engine) becomes authoritative, and *that* is when we — and
> modders — deliberately **diverge from C2C**, with StoneBase guarding spec-compliance. So: mirror the engine to get
> here; thereafter the spec leads.
>
> **Parity is the COMPLETENESS test (owner 2026-06-28) — which RAISES the bar, it does not lower it.** *"The true
> reason for parity matching is to make sure that cascade evaluates everything legacy does, and parity overall is the
> best way to ensure that."* Parity is the *instrument* that proves the cascade gathers every source the legacy engine
> does. **So a divergence is your signal that you have NOT yet found a source the engine uses — the job is to FIND it
> (map it to a named legacy source with numbers on both sides), exactly per [`DEC-parity`](#) above.** "It doesn't
> reconcile, so I'll change the legacy/curator/numbers instead" is the **banned shortcut** — the kraken's bait — and
> 99% of the time the value DOES reconcile once the missing source is found (the path here was a string of *wrong*
> "it's stale / it's a bug" guesses that each dissolved when the next source was mapped — `CvPlayer:27896` was the
> real source, found only by reading ALL the writers).
>
> **⛔ Touching legacy is a LAST RESORT, never an agent's judgement call.** Only after the source is FULLY mapped —
> every writer read, the value reproduced offline and shown to be **genuinely non-deterministic** (history/order-
> dependent: a stale incremental cache that survives a recalc, demonstrated across MANY instances, with the engine
> code proving why) — may "streamline the legacy to be deterministic, then parity, then balance" even be *proposed*.
> It then requires **explicit owner authorization for that specific case** (as given 2026-06-28 for the
> specialist-commerce percent stale cache). It is NOT a general licence to change numbers, and the existence of this
> exception must never be cited to skip the mapping work. (The one sanctioned instance + its full evidence:
> [legacy-value-calc-map §1.5](../plans/structural-cleanup/legacy-value-calc-map.md).)

## The tool — StoneBase (the reference dry-calc) & status

The dry-calc is implemented as **StoneBase** (the `Sources`-mirroring .NET cascade at `http://localhost:8229`): it
loads the curated `Assets/Data/**` JSON + the live `/state`, rebuilds the cascade with a typed, layered model
(Domain `Condition` → `IConditionEvaluator` → `BuildingCascade`), and diffs its verdict against the engine oracle
(`/computed/canConstruct`, `/computed/cities/yields`). **StoneBase is THE — and only — validator** (owner ruling 2026-06-25).

> **⛔ StoneBase FOLLOWS the spec — the authority chain is strictly ONE-WAY (owner ruling 2026-06-25).** The flow is
> **SPEC → StoneBase → engine-oracle**, never reversed. The spec leads; StoneBase *implements* it (it is the
> pseudo-code blueprint for the C++ port); the engine is only the **result-oracle** — it confirms that *StoneBase +
> curated data* reproduce the engine's TRUE-set during the mirror phase ([`DEC-mirror-then-redesign`](../architecture/decisions.md#dec-mirror-then-redesign)).
> The cascade is a **distinct model** from how the engine internally handles `enables`/`disables`/`replaces`/dormancy
> — the spec's two-pass **GENERATE→GATE** machine vs the engine's flat `canTrain`/`canConstruct` procedure — so
> StoneBase's STRUCTURE is taken from the spec and is **never** reverse-engineered from the engine's ordering (the
> engine fixes only the *result*). It follows that a parity divergence is **never** resolved by a creative tweak in
> StoneBase: it is a curated-data gap mapped to a named source, or — if the spec is genuinely incomplete — a **spec
> change made FIRST, deliberately**, then re-implemented. **Same-result is necessary but NOT sufficient**: a green
> sweep over a spec-divergent implementation is the trap (the unit `replacedBy` post-gate prune was exactly this —
> output-equal yet structurally wrong; reined back to a GENERATE-pass removal). If StoneBase ever drifts from the
> spec *and* its output is used to judge the spec, the validation loop self-corrupts and spec & impl diverge
> endlessly — the **"multikraken."** Ledgered as [`DEC-stonebase-follows-spec`](../architecture/decisions.md#dec-stonebase-follows-spec).

**The validation order, machine by machine:** the **cascading enabler** first (the "can I build/train?" gate —
buildings ✓ below, then units, …), **then** the **cascading modifiers** (the "how much?" math). Each machine must
reach **parity in StoneBase first** — *then* it is built inside the C++ codebase as a
[shadow](#the-shadow-the-live-counterpart) (the in-engine twin) and cut over. StoneBase is the typed prototype that
C++ shadow mirrors.

**StoneBase's lasting role (beyond the migration):** it persists as the tool that validates the curated JSON against
the live codebase, tracks discrepancies in general, and — ultimately — gives a **modder** a spec-compliance check:
when they author/test a JSON, StoneBase tells them whether it **violates the spec**. It is the single tool for all of
this — superseding **every** earlier attempt (the dead Python dry-calc variants, the first-version .NET validator).

**Cascading-enabler validation — done (buildings + units).** Both gates are reproduced from the JSON + raw state and
confirmed against the engine. **Units (`canTrain`) reuse the building enabler wholesale** — only the inputs differ —
so it was curated-JSON spec-alignment, not new machinery. The semantics this surfaced are pinned **durably**:
[json.md](json.md) §3.4/§3.5 (the vicinity discriminator, the IS_WORKED GOM rule, `{HAS_COAST:{minArea}}`, the
`NO_NUKES` predicate, the instance-cap making-count) and [enabler.md](enabler.md) (the unit enabler model:
`requires.build`-only; `obsoletedBy`; `requires.build.dormant.all` = direct upgrades minus superseders mirroring
`allUpgradesAvailable`; `SupersedingUnits` → the first real use of the **`replaces`** edge; declarative
`GAMEOPTION_X` gates; active-corp `{HAS_CORPORATION}`; `TECH_GAME_START` as the per-civ cascade start point).
**Next: the cascading MODIFIER pass** ("how much?") — the magnitude machine, on the validated enabler foundation.
*(Per the rule below, sweep pass/divergence numbers stay in the run, never in this doc.)*

## The shadow (the live counterpart)
The dry-calc above is the **offline** PARITY leg (external StoneBase vs `/computed`). Its **in-game** twin is the
**SHADOW**: each legacy behaviour gets a per-turn `[TAG]` diff line via the [event spine](event-spine.md) that computes
the **cascade's** answer and diffs it against the **live engine's**, turn over turn, per scope-instance, **decomposed to
named sources**. These are **two distinct legs, never mixed** — PARITY is a separate process (StoneBase vs `/computed`);
SHADOW is in-DLL (cascade-vs-legacy via the spine/gated logging). *(There is **no `/shadow/*` endpoint** — an earlier
attempt conflated the two legs into one route surface and was retired; the shadow rides the gated logging, not a
dedicated endpoint.)* The legacy stays authoritative until its shadow is **clean (parity)**; then it is cut at an
**atomic** cutover, never piecemeal. **Attribute, never guess:** a divergence is mapped to a named source with
numbers on both sides — if the data to attribute it isn't emitted, the first step is to emit it (the
[logging](logging.md) observability bar is the prerequisite).

## Validation cadence — what LOAD verifies vs what END TURN verifies (owner ruling 2026-06-30)

The static/live split — **readJson = static info data**; the **game object = live state** (the cascade runtime: tally,
event spine, accumulators, all condition *evaluation*) — dictates *when* each thing is verified, and it is the cure for
the false-confirmation trap below:

- **LOAD verifies the STATIC + initial setup.** readJson maps the info-level data at load (it never touches a
  `CvGameObject`); the **tally** is a read-only accessor over the object-owned counts ([tally.md](tally.md) — no seed,
  no store); the **enabler**'s HAVE set is established from the loaded objects. So **loading a save** is enough to
  confirm readJson did its job and the cascade's static/initial state is correct + inspectable. **No turn needed.**
- **END TURN verifies LIVE integration — and ONLY that.** Two cases: **(1)** the engine parts we will **not** replace
  can **see** the new cascade data; **(2)** the parts we **will** replace — `canTrain`/`canConstruct` and the modifier
  rates — are **shadowed in the AI's real per-turn calls** (end-turn so the AI calls them). **Before the enabler is
  swapped over, its new build lists are LOGGED at the PYTHON consumer layer** (the AI/UI's view of buildability), so the
  cutover is proven against what the actual consumers see — not just the C++ shadow.
- **⛔ An end turn does NOT confirm a STRUCTURE.** A per-change game shadow produces **false confirmation even on a
  wrong structure** — the gameobject side-table shadowed green (`TALLY diverging=0`, building tier bit-exact) yet was on
  the wrong structural path. So **stand up the proper, spec-faithful structure FIRST**; the shadows then verify
  *behaviour through the surviving/replaced engine*, never *structure*. Structure is gated by **fidelity to the spec**,
  not by a green shadow. Ledgered as [DEC-structure-before-shadow](../architecture/decisions.md#dec-structure-before-shadow).

## ⛔ Parity-pass results stay OUT of the docs (owner ruling 2026-06-23)
Divergence counts, parity checklists, per-pass pilot numbers — **none of it belongs in the durable docs.** Stale
results **poison contexts**: an agent fixates on a number and misdiagnoses (a ~1100-building enable diff was
repeatedly misattributed to a band-model change it had nothing to do with). The spec says what the model **is**; the
**curator code + the live shadow** prove it; the result is ephemeral and stays ephemeral. (This is why the migration
verification collapses to **comments in the curators** — the old→new map — rather than a documented result set.)

## The three test levels
- **Unit** — a single calc: one modifier value, one enabler gate, one tech's availability.
- **Integration** — a subsystem: a city's full yield re-derived from its plots, a player's happiness.
- **End-to-end** — the whole snapshot: the calculator over full game state vs the extractor.

## ⛔ The pollution guardrail — structural, not disciplinary

**Engine-calculated data may enter ONLY at the comparison boundary — never as a cascade input.** (The same rule
[`/state`](http-endpoints.md) enforces by excluding computed yields.) If the calculator could read the engine's
answer it would validate the game *against itself*. The dry-calc enforces this **structurally** via
Clean-Architecture layering: the cascade domain physically **cannot reference the live-state DTOs** — only the
composition root reads the snapshot (translating it into events + the player→team map). The no-rollerskating rule
is the **project graph**, not discipline.

> **⛔ In the in-DLL shadow it is DISCIPLINE, not the graph — so be SUPER-PEDANTIC (owner ruling 2026-06-30).** The
> in-engine cascade has **direct access** to every live object + computed getter; the structural wall StoneBase enjoys
> **does not exist** here, so nothing *stops* it reading a legacy COMPUTED output as an input. Therefore the in-DLL
> shadow must follow **StoneBase's exact input rules + implementation**, enforced by **manual pedantry**: only the
> named raw INPUTs + the comparison boundary; **never** a legacy computed output (`isActiveBuilding`/dormancy,
> connected-bonus resolution, `getYieldRate100`, …) as a cascade input — the modifier reads the **enabler's** active
> state, not the engine's. Direct access makes this trivial to violate by accident; the extra vigilance IS the guardrail.

## Build order (per the model)
The counts must be available **before** the [enabler](enabler.md) runs (its required side reads aggregated counts). The
two legs reach that differently: **offline (StoneBase)** has no engine objects, so it **replays the `DOMAIN` events to
populate its own count model first**, then runs the enabler; the **in-engine** [tally](tally.md) **reads the
object-owned counts** (always available — no replay/seed needed). The [modifier](modifier.md) machine rides the same
data, with a value outcome instead of a true/false. The only live-vs-dry difference is **"when"** (live events carry the
turn; the dry replay doesn't, and the cascade doesn't consume it).

## See also
- [http-endpoints.md](http-endpoints.md) — `/state` (inputs) vs `/computed` (oracle); the verification flow.
- [enabler.md](enabler.md) · [modifier.md](modifier.md) · [tally.md](tally.md) — the machines this rebuilds and proves.
- [event-spine.md](event-spine.md) — the per-turn `[TAG]` events the shadow reads from. [logging.md](logging.md) — the observability surface those events ride.
