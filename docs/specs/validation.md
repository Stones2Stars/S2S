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
- **Oracle** — the engine's actual values from [`/extractor`](http-endpoints.md) (the yield-loaded state) and the
  [`/can/*`](http-endpoints.md) gate queries.
- **Bar — PARITY, full stop (owner ruling 2026-06-23).** Per entity/instance, **0 in-scope mismatches** — *exact*,
  not "close / same ballpark." There is **no tolerance band and no agent grading of acceptability** — that framing
  (and the retired six-rung "care scale") was constantly abused to wave a mismatch through as good-enough. No bug has
  surfaced in any actual legacy *calculation*, so the math matches; **a divergence is therefore a data-collection
  gap** — a source the cascade didn't gather — never a formula difference. Map it to the named source and close it.
  Out-of-scope (depends on a lower, not-yet-validated layer — e.g. a tech needing a `BUILDING_` prereq) is
  **deferred: shown, never silently dropped**.

> **Mirror, don't redesign (`DEC-mirror-then-redesign`).** The migration reproduces the engine's *existing*
> behaviour exactly. Behavioural redesign — *should this behave this way at all?* (e.g. should blackened-skies dorm
> an observatory) — is deferred to **post-migration**, never done during it.

## The shadow (the live counterpart)
The dry-calc above is the **offline** test. Its **in-game** twin is the **shadow**: each legacy behaviour gets a
surface (a `/shadow/*` endpoint + a per-turn `[TAG]` line via the [event spine](event-spine.md)) that computes the
**cascade's** answer and diffs it against the **live engine's**, turn over turn, per scope-instance, **decomposed to
named sources**. The legacy stays authoritative until its shadow is **clean (parity)**; then it is cut at an
**atomic** cutover, never piecemeal. **Attribute, never guess:** a divergence is mapped to a named source with
numbers on both sides — if the data to attribute it isn't emitted, the first step is to emit it (the
[logging](logging.md) observability bar is the prerequisite).

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

## Build order (per the model)
Replay events to populate the [tally](tally.md) **fully first**, *then* run the [enabler](enabler.md) (its required
side reads aggregated counts — correct only once the tally is complete). The [modifier](modifier.md) machine rides
the same spine, with a value outcome instead of a true/false. The only live-vs-dry difference is **"when"** (live
events carry the turn; the dry replay doesn't, and the cascade doesn't consume it).

## See also
- [http-endpoints.md](http-endpoints.md) — `/state` (inputs) vs `/extractor` (oracle); the verification flow.
- [enabler.md](enabler.md) · [modifier.md](modifier.md) · [tally.md](tally.md) — the machines this rebuilds and proves.
- [event-spine.md](event-spine.md) — the per-turn `[TAG]` events the shadow reads from. [logging.md](logging.md) — the observability surface those events ride.
