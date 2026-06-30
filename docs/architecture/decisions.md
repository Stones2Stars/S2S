# Decisions ledger — the canonical ID'd home for cross-cutting rulings

> ⛔ **Every `DEC-*` is a HARD RULE — binding by default, not advice to weigh.** The only thing that relaxes one is
> the owner explicitly saying so; a question you have posed to the owner is a HARD STOP until answered.
>
> **This is an INDEX, not a re-statement.** Each entry is one decision's *pure consequence* + a pointer to its
> authoritative home. **Before adding any cross-cutting ruling anywhere, grep this file first.** A doc that needs a
> ruling links `[DEC-id]`; it does not re-articulate it.

---

### DEC-fixedpoint-x100

All cascade value math is integer fixed-point ×100; JSON is human-readable; the single human→×100 conversion lives
only in readJson. **Home:** [fixed-point-and-scales.md](../specs/curators/fixed-point-and-scales.md).

### DEC-curator-owns-descale

The curator absorbs all per-100 scaling once and emits uniformly human numbers; readJson has zero per-field scale
knowledge. **Home:** [fixed-point-and-scales.md §1](../specs/curators/fixed-point-and-scales.md).

### DEC-deliveryguy

A cross-entity modifier lives on whoever DELIVERS it, keyed by the target ("who brings this to the table?"); the
other entity is a referenced `enabled`/`requires` condition, never the home. **Home:** [modifier.md §6](../specs/modifier.md).

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
enters only at the comparison boundary, never as a cascade input. **Home:** [validation.md](../specs/validation.md).

### DEC-kraken

Skipping/assuming/guessing/shortcuts is the cardinal sin; maximal rigor by default until the owner explicitly relaxes
it. **Home:** [AGENTS.md](../../AGENTS.md).

### DEC-fast-is-slow-slow-is-fast

Read each subsystem doc in full before acting; skimming is never the faster path. **Home:** [AGENTS.md](../../AGENTS.md).

### DEC-map-before-delete

A legacy maintainer is validated to parity (dry-calc now, live-shadow next) before it is cut — you cannot delete what
you cannot fully observe. **Home:** [AGENTS.md](../../AGENTS.md).

### DEC-parity

Parity is the only goal — exact match, no tolerance band  a divergence is a data-collection gap, never
a formula difference. **Home:** [validation.md](../specs/validation.md).

### DEC-mirror-then-redesign

The migration reproduces the engine's existing behaviour exactly; behavioural redesign ("should it behave this way at
all?") is deferred to post-migration, never done during it. **Home:** [validation.md](../specs/validation.md).

### DEC-stonebase-follows-spec

The validation authority chain is ONE-WAY — SPEC → StoneBase → engine-oracle: StoneBase *implements* the spec (the
blueprint for the C++ port), never reverse-engineers the engine's internal procedure; the engine fixes only the
RESULT (mirror phase). A divergence is a curated-data gap mapped to a named source, or a deliberate spec-change-FIRST
— never a creative StoneBase tweak. Same-result is necessary but NOT sufficient (a green sweep over a spec-divergent
impl is the trap); if StoneBase drift were used to judge the spec, the loop self-corrupts (the "multikraken").
**Home:** [validation.md](../specs/validation.md).

### DEC-no-parity-results-in-docs

Parity-pass results (divergence counts, checklists, pilot numbers) stay out of the durable docs. **Home:** [validation.md](../specs/validation.md).

### DEC-tally-serializes-nothing

The modifier scope accumulators serialize nothing — rebuilt from loaded state. The **tally** serializes AND stores
nothing: it is a read-only accessor over the object-owned counts (`CvPlayer::getBuildingCount`, …) rolled up the spine
— no duplicate store, no seed, no shadow (a count shadow would be tautological). **Home:** [tally.md](../specs/tally.md).

### DEC-save-remove-is-soft

Removing a serialized field/Type is soft in the name-keyed save format; only four cases are hard. **Home:** [engine.md](../reference/engine.md).

### DEC-derived-never-trusted

Derived data is never trusted from a save — `reset()` marks it dirty on load and recomputes from live state. **Home:** [engine.md](../reference/engine.md).

### DEC-obs-scale

The Observability Scale (0 Oblivious … 5 Meta) + the "Orwell" bar: rebuild game state from endpoints + `/events` +
gated logs, never the screen. **Home:** [observability.md](../reference/observability.md).

### DEC-obs-hook-shapes

Three canonical observability hook shapes: a snapshot field, a gated `[TAG]` log tee, a mailbox `/diagnostic`
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
structure (the gameobject side-table shadowed green yet was structurally wrong). LOAD verifies the static + initial
setup (readJson mapped at load + the tally reading the object-owned counts — loading a save suffices); END TURN
verifies only LIVE integration (surviving engine sees the data; the to-be-replaced gates `canTrain`/`canConstruct` +
modifier rates shadowed in the AI's real calls, with the new build lists logged at the Python consumer layer before the
enabler swap). Structure is gated by spec-fidelity, never by a green shadow. **Home:** [validation.md](../specs/validation.md).

### DEC-conditions-are-predicates

A deposit's condition is expressed as a **PREDICATE** in `enabled`/`disabled`/`requires` (the predicate registry is
**EXTENSIBLE — define new predicates freely** when one isn't named verbatim yet); it is **NEVER** encoded as a
bespoke sub-scope **MEMBER** (`empire.capital`, `perMilitaryUnit`, …). Adding a predicate
*extends* the model within the existing structure; inventing a condition-carrying member *changes the core
structure* — the kraken way (owner ruling 2026-06-28). A condition-like member that crept in is an agent invention
to retire (the `byEra` / `empire.capital` class), per [DEC-stonebase-follows-spec]. **Exception:** golden age
(`empire.goldenAge`) stays a member-mirror, **deferred** — it is engine-core, not data-defined, so predicate-modelling
it is post-migration work (see [modifier.md §3](../specs/modifier.md) / [golden-age.md](../reference/golden-age.md)).
**Home:** [modifier.md §3](../specs/modifier.md), [json.md §3.5](../specs/json.md).
