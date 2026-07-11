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

Parity is the only goal — exact match, no tolerance band; a divergence is a data-collection gap, never
a formula difference. **Home:** [validation.md](../specs/validation.md).

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

Removing a serialized field/Type is soft in the name-keyed save format; only a handful of cases are hard, and a
deleted field's read needs a named `WRAPPER_SKIP_ELEMENT`. **Home:** [engine.md](../reference/engine.md).

### DEC-derived-never-trusted

Derived data is never trusted from a save — `reset()` marks it dirty on load and recomputes from live state. **Home:** [engine.md](../reference/engine.md).

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
*extends* the model; a condition-carrying member *changes the core structure*. **Exception:** golden age
(`empire.goldenAge`) stays a member-mirror, deferred to post-migration — it is engine-core, not data-defined.
**Home:** [modifier.md §3](../specs/modifier.md), [json.md §3.5](../specs/json.md).

### DEC-single-implementation

Every cascade calculation/evaluation exists **exactly once**, as a pure static function exposed on a shared surface —
a purely-organizational static-methods class (never a namespace: VC7.1/Boost/EXE-ABI name-mangling risk; never
file-`static`-hidden: the next consumer reimplements it). ONE evaluator (`cascadeEvalCondition`) evaluates all
conditions/predicates. The legacy shadow is the only sanctioned duplication, scheduled to die at the atomic cutover.
**Home:** [patterns.md § DRY](patterns.md).

### DEC-json-not-cascade

`CvJsonInfo` is the JSON-info BASE (availability model + info-owned typed condition ONLY, zero cascade runtime); the
per-type modifier DATA lives as real typed members on the `CvJson<X>Info` subclasses; the cascade RUNTIME is built by
the cascade's own setup reading those pocos — never stored on / mixed into the JSON info. **Home:**
[cascade-engine-430.md §3](../plans/structural-cleanup/cascade-engine-430.md).

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

### DEC-red-ratchet

The tree deliberately does NOT compile: the XML `CvXInfo` classes are archived (`SourceArchive/Infos/`) as a
fallback-proof ratchet — never restore them, never re-add a `CvXInfo`; green is reached ONLY by finishing the
JsonInfo structure + the full getter/consumer wiring. **Home:** [AGENTS.md](../../AGENTS.md) Build And Test.

### DEC-no-xml-into-game

Reading a REPLACED info's legacy XML **into the running game is HARD BANNED**. The `CIV4<X>Infos.xml` files are kept
in the tree as **curator INPUT ONLY** (removing them broke the curator); the game registers + populates replaced
infos from the **JSON** load path, never from `LoadGlobalClassInfo(GC.m_pa<X>Info, "CIV4<X>Infos", …)`. Their presence
for the curator is not license to load them at runtime — the recurring rollerskate. **Home:** [AGENTS.md](../../AGENTS.md) Build And Test.
