# Stones2Stars — docs

The condensed spec surface. **Session-start protocol (AGENTS.md banner): read ALL of `specs/`, `architecture/`,
and `reference/` in full before any work**, plus the #430 [roadmap](plans/structural-cleanup/roadmap.md) — these
exist so an agent ends up with a correct model of the system, not a reverse-engineered guess.

> Rules & conventions for agents/contributors live in the root **[AGENTS.md](../AGENTS.md)** (the one rule home),
> never here. This is the *knowledge* map.

## How the three tiers differ — and which one to trust

The tiers are not interchangeable, and the difference decides what a stale line costs you:

- **`specs/` + `architecture/` = the DESIGN and the RULINGS. Timeless, authoritative, and kept free of
  implementation status.** What is BUILT belongs in `plans/`, not here. This is deliberate: when an
  implementation is archived or reverted, a spec carrying build-status silently becomes a lie that the next
  agent conforms to. If you find status prose in a spec, that is a defect — move it, don't extend it.
- **`reference/` = how the ENGINE behaves today.** Independent of the cascade rework; the legacy mechanics,
  toolchain constraints, and formulas. Stable.
- **`plans/` = the TODO tier — what is NOT done yet, as short bulleted lists**
  ([DEC-spec-plus-todo](architecture/decisions.md#dec-spec-plus-todo)). ⛔ **A plan doc is not a progress record:**
  no `LANDED`/`✅ DONE`/completion ledger belongs in one — a finished item is DELETED and anything durable it
  established moves into the SPEC. The list measures what is LEFT; git history records what was done.
  ⛔ **Verify any claim against the tree before acting on it** — branch `cascade-rebuild` archived the substrate
  several plan docs were written against, so a doc can name symbols that no longer exist while reading as
  authoritative. The cheap mechanical check: grep one or two of the symbols it is anchored on; if they live only in
  `SourceArchive/`, the doc describes a dead world. Prefer DELETING a stale status claim to updating it.

⛔ **Where two docs disagree, the spec wins over the plan, and the LIVE CODE settles a question of what exists.**
Verify against the tree before acting on any claim that something is built.

## `specs/` — the data model + the system
- **[specs/json.md](specs/json.md)** — **THE JSON authoring model**: sections, scopes, conditions
  (`all`/`any`/`noneOf`), predicates, modifier families, the entry shape, the unit classification (§8). Start here.
- **[specs/naming.md](specs/naming.md)** — the infotype id-prefix glossary (`UNIT_`/`BONUS_`/`BUILDING_`/…).
- **[specs/enabler.md](specs/enabler.md)** — the **"can I?"** machine (2-pass generate→gate; `enables`/`requires`/`allowed`).
- **[specs/modifier.md](specs/modifier.md)** — the **"how much?"** machine (deposit-down, combine, the deliveryguy ownership rule).
- **[specs/tally.md](specs/tally.md)** — the **"how many?"** machine (counts roll up, serializes nothing).
- **[specs/vision.md](specs/vision.md)** — the **"how far can I see?"** machine (a budget spent walking outward,
  exactly as movement works; the STRENGTH vs ELEVATION split).
- **[specs/triggers.md](specs/triggers.md)** — the **provisions** machine (trigger → chance → action; a grant is a
  trigger with a null condition), incl. the game-start START PACKAGES.
- **[specs/event-spine.md](specs/event-spine.md)** — the one dispatch primitive consumers draw events from (the KIND firewall).
- **[specs/save.md](specs/save.md)** — the name-keyed save format + the **soft-remove** discipline (`savemigration.txt` drain, no `WRAPPER_SKIP_ELEMENT`, derived-serializes-nothing).
- **[specs/logging.md](specs/logging.md)** — **what to log** (the Orwell observability bar, hook shapes, the coverage scale).
- **[specs/validation.md](specs/validation.md)** — the live-verification discipline: done-is-observable endpoint polls + turn time (parity and shadow are closed).
- **[specs/http-endpoints.md](specs/http-endpoints.md)** — the HTTP transport + its two standing invariants, and
  ⛔ **why the route surface is EMPTY and must stay empty** (an endpoint is a live consumer: a route keeps a legacy
  member alive past the compiler census). The six stored-vs-oracle cache documents are the whole surface today.
- Unit classification — **[skills](specs/skills.md)** (mutable abilities) · **[tags](specs/tags.md)** (immutable
  membership) · **[state](specs/state.md)** (transient) · **[capabilities](specs/capabilities.md)** (empire).
- **[specs/curators/](specs/curators/README.md)** — the migration conversion spec (**transient**; the old→new field
  map lives in the curator *code*). Dropped when the migration completes.

## `reference/` — how it behaves today
- **[reference/engine.md](reference/engine.md)** — the engine constraints (toolchain, save-load, pathfinding, properties, gamespeed, unitcombat).
- **[reference/economy.md](reference/economy.md)** — maintenance, upkeep, happiness, health, war-weariness, pollution.
- **[reference/yields-growth.md](reference/yields-growth.md)** — civics, food, improvements/plot yields, city production, golden ages & era.
- **[reference/golden-age.md](reference/golden-age.md)** — the complete golden-age reference: its 3 base-yield additions (incl. the per-plot **pre-improvement** threshold), faster growth & great people, zero-anarchy civic swaps, all triggers/duration. (So we stop re-deriving it from the engine.)
- **[reference/culture-religion-research.md](reference/culture-religion-research.md)** — culture, religion, research/tech, heritage, corporations.
- **[reference/special-systems.md](reference/special-systems.md)** — espionage, great people, promotions/XP, vision, trade, diplomacy, victory.
- **[reference/unit-lifecycle.md](reference/unit-lifecycle.md)** — a unit's birth, the five-operation death sequence (only `die()` kills), delayed death vs delayed DELETION, the off-map unit, and the re-entrancy routes.
- **[reference/mission-outcome-system.md](reference/mission-outcome-system.md)** — the `CvOutcome` mission/outcome system (feeds the json.md §8 `missions` block).
- **[reference/observability.md](reference/observability.md)** — the operational tag registry / gate knobs / live server / field census / PlotSnapshot.
- **[reference/memory-footprint.md](reference/memory-footprint.md)** — where the RAM goes under the 32-bit ceiling: the static clusters (info classes, per-object arrays, cascade caches) vs the per-turn churn; textures/icons are loaded once (shared).
- **[reference/external-tools-and-workflows.md](reference/external-tools-and-workflows.md)** — crash-dump symbolization, FpkBuilder.
- **The LEGACY censuses** — how the legacy behaves today, so the cascade can replace it:
  **[legacy-value-calc-map](reference/legacy-value-calc-map.md)** (which getter computes each per-turn value, and
  from what) · **[legacy-grant-apply-sites](reference/legacy-grant-apply-sites.md)** (where provisions are handed
  over) · **[pedia-read-map](reference/pedia-read-map.md)** +
  **[python-read-map](reference/python-read-map.md)** (what the Python surface consumes).
- **[reference/python-load-sequence.md](reference/python-load-sequence.md)** — the C++/Python boundary MECHANISM
  and ORDER: the **two producers** of `CvPythonExtensions` (ours and the closed EXE's), the ordered DLL load
  (premenu → menu → postmenu → game start → the consumer-registration contract), the Python entry cascade, and
  the marshalling contract that decides which types must stay registered.

## `architecture/` — the design compass
- **[architecture/decisions.md](architecture/decisions.md)** — the **DECISIONS LEDGER** (the `DEC-*` index — grep it before adding any ruling).
- **[architecture/north-star.md](architecture/north-star.md)** — the structural compass (data side vs AI side; the three machines; Clean Architecture in C++03).
- **[architecture/superseded-ideas.md](architecture/superseded-ideas.md)** — the don't-revive registry.
- **[architecture/patterns.md](architecture/patterns.md)** — interface contracts in C++03 (poor-man's DI) + the DRY single-implementation law.
- **[architecture/state-repositories.md](architecture/state-repositories.md)** — the MAINTAINED SUM: derived state moved by the fact that names its source, never marked and never recomputed.

## `plans/` — mutable work state
- **[plans/structural-cleanup/roadmap.md](plans/structural-cleanup/roadmap.md)** — 🔝 **the master plan** (the only
  session-start read in this tier): the design and the governing rulings. It carries NO status.
- **[plans/structural-cleanup/todo.md](plans/structural-cleanup/todo.md)** — what is NOT done, as short bullets.
- **[plans/structural-cleanup/](plans/structural-cleanup/README.md)** — the rest of the tier: the decision
  worklists awaiting owner input, and the owner-LOCKED property audit.
- **[plans/parked/](plans/parked/README.md)** — un-killed forward design intent (the backlog). Carried AS-IS: stale
  paths and stale status are expected, and each is re-grounded only when its initiative becomes active.

## Also at this level
- **[MOD-README.md](MOD-README.md)** — the mod's front-door / build-pipeline readme (the code repo's mirror).
- **[CHANGELOG.md](CHANGELOG.md)** — the mod changelog.
- The hosted catalogs (DESPAIR / REALISM / COMPLEXITY) → **[`/indexes/`](../indexes/)** (repo root, served via Pages).
