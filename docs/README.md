# Stones2Stars — docs

The condensed spec surface (rebuilt 2026-06-23). **Read the spec for whatever you touch BEFORE working** — these
exist so an agent ends up with a correct model of the system, not a reverse-engineered guess.

> Rules & conventions for agents/contributors live in the root **[AGENTS.md](../AGENTS.md)** (the one rule home),
> never here. This is the *knowledge* map.

## `specs/` — the data model + the system
- **[specs/json.md](specs/json.md)** — **THE JSON authoring model**: sections, scopes, conditions
  (`all`/`any`/`noneOf`), predicates, modifier families, the entry shape, the unit classification (§8). Start here.
- **[specs/naming.md](specs/naming.md)** — the infotype id-prefix glossary (`UNIT_`/`BONUS_`/`BUILDING_`/…).
- **[specs/enabler.md](specs/enabler.md)** — the **"can I?"** machine (2-pass generate→gate; `enables`/`requires`/`allowed`).
- **[specs/modifier.md](specs/modifier.md)** — the **"how much?"** machine (deposit-down, combine, the deliveryguy ownership rule).
- **[specs/tally.md](specs/tally.md)** — the **"how many?"** machine (counts roll up, serializes nothing).
- **[specs/event-spine.md](specs/event-spine.md)** — the one dispatch primitive consumers draw events from (the KIND firewall).
- **[specs/logging.md](specs/logging.md)** — **what to log** (the Orwell observability bar, hook shapes, the coverage scale).
- **[specs/validation.md](specs/validation.md)** — the dry-calc test + the live shadow + the **parity** bar.
- **[specs/http-endpoints.md](specs/http-endpoints.md)** — the endpoint catalogue (`/state`, `/extractor`, `/shadow`, …).
- Unit classification — **[skills](specs/skills.md)** (mutable abilities) · **[tags](specs/tags.md)** (immutable
  membership) · **[state](specs/state.md)** (transient) · **[capabilities](specs/capabilities.md)** (empire/tech).
- **[specs/curators/](specs/curators/README.md)** — the migration conversion spec (**transient**; the old→new field
  map lives in the curator *code*). Dropped when the migration completes.

## `reference/` — how it behaves today
- **[reference/engine.md](reference/engine.md)** — the engine constraints (toolchain, save-load, pathfinding, properties, gamespeed, unitcombat).
- **[reference/economy.md](reference/economy.md)** — maintenance, upkeep, happiness, health, war-weariness, pollution.
- **[reference/yields-growth.md](reference/yields-growth.md)** — civics, food, improvements/plot yields, city production, golden ages & era.
- **[reference/culture-religion-research.md](reference/culture-religion-research.md)** — culture, religion, research/tech, heritage, corporations.
- **[reference/special-systems.md](reference/special-systems.md)** — espionage, great people, promotions/XP, vision, trade, diplomacy, victory.
- **[reference/observability.md](reference/observability.md)** — the operational tag registry / gate knobs / live server / field census / PlotSnapshot.
- **[reference/external-tools-and-workflows.md](reference/external-tools-and-workflows.md)** — crash-dump symbolization, FpkBuilder, GameTracker.

## `architecture/` — the design compass
- **[architecture/decisions.md](architecture/decisions.md)** — the **DECISIONS LEDGER** (the `DEC-*` index — grep it before adding any ruling).
- **[architecture/north-star.md](architecture/north-star.md)** — the structural compass (data side vs AI side; the three machines; Clean Architecture in C++03).
- **[architecture/superseded-ideas.md](architecture/superseded-ideas.md)** — the don't-revive registry.
- **[architecture/patterns.md](architecture/patterns.md)** — interface contracts in C++03 (poor-man's DI).

## `plans/`
- **[plans/structural-cleanup/](plans/structural-cleanup/README.md)** — the bulldozer reference (what gets deleted at the cascade cutover).
- **[plans/parked/](plans/parked/README.md)** — un-killed forward design intent (the backlog).

## Also at this level
- **[MOD-README.md](MOD-README.md)** — the mod's front-door / build-pipeline readme (the code repo's mirror).
- **[CHANGELOG.md](CHANGELOG.md)** — the mod changelog.
- The hosted catalogs (DESPAIR / REALISM / COMPLEXITY) → **[`/indexes/`](../indexes/)** (repo root, served via Pages).
