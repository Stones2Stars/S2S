# Stones2Stars — docs

The condensed spec surface, **one concept per home** — which is a statement about how these files are ORGANISED
(a concept split across TIERS grew two accounts that disagreed), and **never a statement about how much you
read**.

A large concept's home is a **directory with a hub page**: `cascade.md` is a map, and the spec itself is the
pages in `cascade/`. The concept still lives in exactly one place — it is PAGINATED for reading, never scattered,
and no part of it is filed under another tier. ⛔ **A hub carries no ruling of its own; the parts ARE the spec**,
so stopping at the hub is reading the table of contents and calling it the book.

⛔ **GREP THE WHOLE OF `docs/` FOR WHAT YOU ARE ABOUT TO TOUCH, AND READ EVERY FILE THAT HITS, END TO END** — the
subsystem, the symbols, the mechanism. The number of relevant files is something you FIND, not something you
DECIDE, and it is regularly more than one: a behaviour lives in the spec that designs it, the reference that
records how it behaves today, and often a plan doc that measured it. Then confirm all of it against `Sources/`,
because [the tree outranks the doc](../AGENTS.md).
⛔ **Any wording — here or anywhere — that lets an agent SELECT which core docs to read is a defect, and is
deleted on sight** ([AGENTS.md](../AGENTS.md)). What was retired is re-reading the whole corpus at SESSION START;
that moved the reading to work-start and made it exhaustive *for the subsystem*, and it has been misread as
permission to read one file and stop.

> Rules & conventions for agents/contributors live in the root **[AGENTS.md](../AGENTS.md)** (the one rule home),
> never here. This is the *knowledge* map.

## Start here if you are new

**[overview.md](overview.md) — the guided tour.** How the data is stored, how "can I build it?" and "how much?"
are answered, how state changes are announced, how things get handed over — and what each of those replaced. It
is the fastest way to get a real map of the engine and to work out WHICH file below owns your subsystem.
⛔ It is a PRIMER and **never an authority**: it links every ruling to the doc that owns it rather than restating
one. Where it and a spec disagree, the spec wins; where a spec and the tree disagree, the tree wins.

## Where a concept lives — and which home to trust

The homes are not interchangeable, and the difference decides what a stale line costs you:

- **TOP LEVEL = a concept whose spec and design are ONE thing.** [cascade.md](cascade.md) and
  [spine.md](spine.md) live here because splitting either across `specs/` and `architecture/` is what let two
  accounts of one machine drift apart. A concept gets one file; where that file sits is a detail.
- **`specs/` + `architecture/` = the DESIGN and the RULINGS. Timeless, authoritative, and kept free of
  implementation status.** What is BUILT belongs in `plans/`, not here. This is deliberate: when an
  implementation is archived or reverted, a spec carrying build-status silently becomes a lie that the next
  agent conforms to. If you find status prose in a spec, that is a defect — move it, don't extend it.
- **`reference/` = how the ENGINE behaves today.** Independent of the cascade rework; the legacy mechanics,
  toolchain constraints, and formulas. Stable.
- **`plans/` = the TODO tier — what is NOT done yet, as short bulleted lists**
  ([a doc is a SPEC or a TODO, never both](../AGENTS.md#docs)). ⛔ **A plan doc is not a progress record:**
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
- **[cascade.md](cascade.md)** — the **"how much?"** machine: deposit-down and the combine arithmetic, the
  conditioning (dormancy) model, the deliveryguy ownership rule, the MAINTAINED SUM (derived state moved by the
  fact that names its source, never marked and never recomputed), and the per-scope
  `PlotContext`/`CityContext`/`EmpireContext` live-state read surface.
- **[specs/tally.md](specs/tally.md)** — the **"how many?"** machine (counts roll up, serializes nothing).
- **[specs/vision.md](specs/vision.md)** — the **"how far can I see?"** machine (a budget spent walking outward,
  exactly as movement works; the STRENGTH vs ELEVATION split).
- **[specs/triggers.md](specs/triggers.md)** — the **provisions** machine (trigger → chance → action; a grant is a
  trigger with a null condition), incl. the game-start START PACKAGES.
- **[specs/save.md](specs/save.md)** — the name-keyed save format + the **soft-remove** discipline (`savemigration.txt` drain, no `WRAPPER_SKIP_ELEMENT`, derived-serializes-nothing).
- **[specs/validation.md](specs/validation.md)** — the live-verification discipline: done-is-observable endpoint polls + turn time (parity and shadow are closed).
- **[spine.md](spine.md)** — the **event spine**: the one dispatch primitive + KIND firewall, the fact vocabulary,
  what to log (the Orwell bar, hook shapes), and the live tag registry / gate knobs / HTTP server / field census /
  PlotSnapshot as they exist today.
- **[specs/http-endpoints.md](specs/http-endpoints.md)** — the HTTP transport + its two standing invariants, and
  ⛔ **why the route surface is EMPTY and must stay empty** (an endpoint is a live consumer: a route keeps a legacy
  member alive past the compiler census). Four STORED-side decomposition censuses are the whole surface today;
  the `oracle` twins are dead ([superseded-ideas #33](architecture/superseded-ideas.md)).
- Unit classification — **[skills](specs/skills.md)** (mutable abilities) · **[tags](specs/tags.md)** (immutable
  membership) · **[state](specs/state.md)** (transient) · **[capabilities](specs/capabilities.md)** (empire).
- **[specs/curators/](specs/curators/README.md)** — the migration conversion spec (**transient**; the old→new field
  map lives in the curator *code*). Dropped when the migration completes.

## `reference/` — how it behaves today
- **[reference/engine.md](reference/engine.md)** — the engine constraints (toolchain, save-load, pathfinding, properties, gamespeed, unitcombat).
- **[reference/economy.md](reference/economy.md)** — maintenance, upkeep, happiness, health, war-weariness, pollution.
- **[reference/yields-growth.md](reference/yields-growth.md)** — civics, food, improvements/plot yields, city production, golden ages & era.
- **[reference/citizen-assignment.md](reference/citizen-assignment.md)** — how a city seats its population: plots and
  specialists as ONE scored priority list walked with two cursors (a specialist repeats, a plot is consumed), and why
  an emphasis must both promote and suppress or it does nothing.
- **[reference/golden-age.md](reference/golden-age.md)** — the complete golden-age reference: its 3 base-yield additions (incl. the per-plot **pre-improvement** threshold), faster growth & great people, zero-anarchy civic swaps, all triggers/duration. (So we stop re-deriving it from the engine.)
- **[reference/bonuses.md](reference/bonuses.md)** — what a resource IS: the plot group owns the number and nothing mirrors it; the two mutually-exclusive origins (`trade` vs `onSite`) and why `vicinity` is a different axis; manufactured bonuses being the same class as any other; every route a resource takes IN and OUT; trading, and the three facts of which only one is a crossing.
- **[reference/culture-religion-research.md](reference/culture-religion-research.md)** — culture, religion, research/tech, heritage, corporations.
- **[reference/special-systems.md](reference/special-systems.md)** — espionage, great people, promotions/XP, vision, trade, diplomacy, victory.
- **[reference/unit-lifecycle.md](reference/unit-lifecycle.md)** — a unit's birth, the five-operation death sequence (only `die()` kills), delayed death vs delayed DELETION, the off-map unit, and the re-entrancy routes.
- **[reference/mission-outcome-system.md](reference/mission-outcome-system.md)** — the `CvOutcome` mission/outcome system (feeds the json.md §8 `missions` block).
- **[reference/memory-footprint.md](reference/memory-footprint.md)** — where the RAM goes under the 32-bit ceiling: the static clusters (info classes, per-object arrays, cascade caches) vs the per-turn churn; textures/icons are loaded once (shared).
- **[reference/unit-rendering.md](reference/unit-rendering.md)** — how the DLL drives unit graphics: one centre unit per plot, real vs dummy scene entities, every call that creates/sets up/places/destroys a node and when; **graphics paging ON vs OFF** as a two-column table, the ranked list of code that is not gated on paging but only behaves with it on, the load and new-game timelines, where the tree differs from the owner's working model of rendering (a lens, not a ruling), and the doc-vs-tree contradictions this census found.
- **[reference/external-tools-and-workflows.md](reference/external-tools-and-workflows.md)** — crash-dump symbolization, FpkBuilder.
- **[reference/release-deploy.md](reference/release-deploy.md)** — how a build reaches players: the AppVeyor → SVN → GitHub pipeline, the FPK patch step, and the **batched** SVN commit (SourceForge 504s on a whole-release transaction) incl. the ordering rules that make each batch legal and the non-atomicity that follows.
- **The LEGACY censuses** — how the legacy behaves today, so the cascade can replace it:
  **[triggers.md](specs/triggers.md)** (where provisions are handed
  over) · **[pedia-read-map](reference/pedia-read-map.md)** +
  **[python-read-map](reference/python-read-map.md)** (what the Python surface consumes).
- **[reference/python-load-sequence.md](reference/python-load-sequence.md)** — the C++/Python boundary MECHANISM
  and ORDER: the **two producers** of `CvPythonExtensions` (ours and the closed EXE's), the ordered DLL load
  (premenu → menu → postmenu → game start → the consumer-registration contract), the Python entry cascade, and
  the marshalling contract that decides which types must stay registered.

## `architecture/` — the design compass
- **[architecture/north-star.md](architecture/north-star.md)** — the structural compass (data side vs AI side; the three machines; Clean Architecture in C++03).
- **[architecture/superseded-ideas.md](architecture/superseded-ideas.md)** — the don't-revive registry.
- **[architecture/patterns.md](architecture/patterns.md)** — interface contracts in C++03 (poor-man's DI) + the DRY single-implementation law.

## `plans/` — mutable work state
- **[plans/structural-cleanup/](plans/structural-cleanup/README.md)** — now just the
  owner-LOCKED property audit; the migration's remaining work lives as short per-concept bullets in
  `specs/`/`reference/`, not as a standing worklist here.
- **[plans/parked/](plans/parked/README.md)** — un-killed forward design intent (the backlog). Carried AS-IS: stale
  paths and stale status are expected, and each is re-grounded only when its initiative becomes active.

## Also at this level
- **[MOD-README.md](MOD-README.md)** — the mod's front-door / build-pipeline readme (the code repo's mirror).
- **[CHANGELOG.md](CHANGELOG.md)** — the mod changelog.
- The hosted catalogs (DESPAIR / REALISM / COMPLEXITY) → **[`/indexes/`](../indexes/)** (repo root, served via Pages).
