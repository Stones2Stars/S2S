# Developer documentation

> ## ⛔ RESUMING AFTER A CONTEXT COMPACTION? RE-READ EVERYTHING FIRST.
>
> If your context was just compacted/summarized mid-session, **STOP — do not touch code or make decisions from the
> summary alone.** Compaction has poisoned context before. Before any further work on the **#428/#430 cascade**,
> re-read the full set in `docs/dev/plans/`: `data-model-spec.md`, `enabler-cascade-spec.md`, `modifier-cascade-spec.md`,
> `tally-cascade-spec.md`, `event-spine-spec.md`, `cascade-engine-430.md`, `migration-renames.md`,
> `migration-entity-ranking.md` — plus the live code in `Sources/Cascade/`. Look concepts UP; never reconstruct them
> from the summary or from how the C++ currently reads (the C++ is reworked to fit the data, not ground truth). A
> stale spec line loses to a later owner ruling. (Owner 2026-06-17 — this re-read gate is being deliberately tested.)

Notes for people working on the **DLL and engine** (C++/Python internals).
Player- and end-user-facing documentation lives separately in the top-level
[`docs/`](../../docs/) folder — keep the two apart.

This folder is split by intent:

- **[`reference/`](reference/)** — *how the code works today.* Durable
  architecture notes tied to specific classes/systems. Update these when the
  behaviour they describe changes.
- **[`plans/`](plans/)** — *forward-looking work.* Refactor scopes, rollouts,
  removal maps, design captures, and standing initiatives. These describe
  intended or in-progress change, not the current state.

> Line numbers in these notes are anchors at time of writing; they drift. Treat
> them as "around line N" and confirm against the named function.

## Reference — how it works

| Doc | Subject |
|---|---|
| [CvGlobals](reference/CvGlobals.md) | Master singleton (`GC`): info arrays, engine interfaces, services |
| [CvMap](reference/CvMap.md) | Map grid, areas, plot groups, viewport, multi-map support |
| [CvPlot](reference/CvPlot.md) | Individual tile: terrain, units, properties, path cache |
| [CvGameAI](reference/CvGameAI.md) | Global AI helpers; normalised combat value; dirty-work propagation |
| [CvPlayerAI](reference/CvPlayerAI.md) | Per-player AI: research, diplomacy, military, danger caching |
| [CvCityAI](reference/CvCityAI.md) | City management: citizen assignment, production, building scoring |
| [CvUnitAI](reference/CvUnitAI.md) | Unit behaviour: missions, combat, contracts, promotion, upgrade |
| [CvTeamAI](reference/CvTeamAI.md) | Team strategy: war planning, diplomacy, area classification |
| [CvSelectionGroupAI](reference/CvSelectionGroupAI.md) | Group coordination: attacks, garrison, mission state |
| [CvWorkerAI](reference/CvWorkerAI.md) | Per-player worker cache + build-claim ledger |
| [CvArmy](reference/CvArmy.md) | Multi-stack coordinated assault groups |
| [CvContractBroker](reference/CvContractBroker.md) | Publish/subscribe unit-need dispatch between cities and units |
| [CvPathGenerator / CvPath](reference/CvPathGenerator.md) | Pluggable A\* pathfinder with per-unit cost/validity callbacks |
| [pathfinding](reference/pathfinding.md) | Engine FAStar finders (step/route/area/plot-group), the callback contract, team-restricted distance |
| [CvOutcome](reference/CvOutcome.md) | Probabilistic mission results defined in XML |
| [CvProperties](reference/CvProperties.md) | Generic extensible property container (crime, pollution, …) |
| [CvPropertySolver](reference/CvPropertySolver.md) | Per-turn solver for the property simulation system |
| [PlotSnapshot](reference/PlotSnapshot.md) | Plot-state snapshot schema + call-site conventions for logging |
| [ai-logging-reference](reference/ai-logging-reference.md) | The structured/tagged AI decision logs as they exist today |
| [http-server](reference/http-server.md) | `CvHttpServer` — the live observability/query layer: `127.0.0.1:7227` endpoints (`/units`/`/players`/`/cities`/`/events`), the `/events` SSE log-tee (#419), gating (`Autolog__HttpServer`/`Autolog__LogLevelStream`), snapshot isolation, and the planned `/tally` |
| [UnitAI_Selection](reference/UnitAI_Selection.md) | How the AI picks a unit's `UNITAI` and concrete unit type |
| [UnitSelection_Mechanics](reference/UnitSelection_Mechanics.md) | The selection step itself: role → chosen `UnitTypes` |
| [doProduction](reference/doProduction.md) | `CvCity::doProduction` — the per-turn city production step |
| [constructibility-and-prerequisites](reference/constructibility-and-prerequisites.md) | The unified `ConstructRequirement` model, the static enabler reverse-index (CABV turn-time), and help-text rendering |
| [declarative-info-loading](reference/declarative-info-loading.md) | `CvInfoUtil` / `getDataMembers` — how `Cv*Info` classes load from XML, the wrapper catalog, and the byte-identical migration recipe (#196) |
| [calendar-and-gamespeed](reference/calendar-and-gamespeed.md) | Era-driven turn counts & calendar (`CvEraInfo` pacing fields, `CvGameSpeedInfo` derived accessors, `CvDate` interpolation, Adapt channels) |
| [MapScript_Process](reference/MapScript_Process.md) | End-to-end lifecycle of a C2C mapscript (`C2C_World`) |
| [handicaps](reference/handicaps.md) | The difficulty system: per-player vs game handicap, the human-field/AI-field split, a per-field "what each value does" reference (+ #423 cascade-migration fit), and what setting a non-Noble AI actually changes |
| [unitcombat](reference/unitcombat.md) | What `UnitCombatType` is: a core Civ4 combat-class axis (promotion-gating, vs-class bonuses, AI) that C2C grew into a 636-entry, many-to-many, ~150-field "innate-promotion tag" class mirroring `CvPromotionInfo`; + its #428 migration fit |
| [save-load-format](reference/save-load-format.md) | How saves work: the name-keyed `CvTaggedSaveFormatWrapper`, soft-vs-hard change rules, and the derived-state-serializes-nothing lever (the tally rebuilds on load). *Recovered 2026-06-17; branch-only until the cascade save handling finalizes.* |

## Plans — work in flight

### #428 / #430 — data migration + cascade engine (this branch's core work)

The locked / in-flight specs the whole migration builds on. (Resuming after a context compaction? Re-read these
first — see the banner at the top of this file.)

| Doc | Subject |
|---|---|
| [data-model-spec](plans/data-model-spec.md) | The canonical object & vocabulary reference (consolidated): reserved sections + modifier families, the shared atom / condition / predicate vocabulary, scopes, the one entry shape. The #430 prototype interface + modder-docs foundation. |
| [enabler-cascade-spec](plans/enabler-cascade-spec.md) | Availability (v0.3 baseline): HAS → CAN GET → HAS THE MEANS TO; the `enables` family (constructive/destructive) + reversible `requires` means + the `allowed` cap; greying; the empire/team-building tier (§5). |
| [modifier-cascade-spec](plans/modifier-cascade-spec.md) | Magnitude (v3, LOCKED): the standardized object/vocabulary, split families, `enabled`/`disabled` + `per` count-scaling, keep-on-source / deliveryguy (§6/§6.1), the demolition list (§9). |
| [tally-cascade-spec](plans/tally-cascade-spec.md) | The additive count machine: presence/counts at any scope, the report + roll-up, the `requires`/`allowed`/`per` readers, and save handling (§9 — rebuilt on load, serializes nothing). |
| [event-spine-spec](plans/event-spine-spec.md) | The #430 front-door event dispatch: `emit(KIND,…)`, the DOMAIN/DIAGNOSTIC/TRACE OOS firewall, raw payloads, consumers (logging / tally / grants). Slice-1 built in `Sources/Cascade/`. |
| [cascade-engine-430](plans/cascade-engine-430.md) | The #430 engine implementation plan: substrate → tally → modifier → enabler; build order, the `readJson` consume path, shadow discipline. |
| [building-cascade-conversion](plans/building-cascade-conversion.md) | The #428 building → cascade/JSON conversion plan + **THE MODEL** (locked 2026-06-14) the whole migration builds on. |
| [migration-renames](plans/migration-renames.md) | The canonical old→new RENAME REGISTRY — every Info's fields mapped to the new shape. |
| [migration-entity-ranking](plans/migration-entity-ranking.md) | The serial entity-conversion ranking + cross-cutting migration rules (one info at a time, verify-before-commit). |

### Other plans

| Doc | Subject |
|---|---|
| [ai-architecture-north-star](plans/ai-architecture-north-star.md) | The coherence frame for the AI/data rework: goal, hard constraints, module taxonomy, roadmap |
| [ai-vs-human-benchmarking](plans/ai-vs-human-benchmarking.md) | Live playthrough observation: does the AI run ahead of or lag the player; competence-not-handicaps principle |
| [codebase-bug-hunt](plans/codebase-bug-hunt.md) | Standing C++/Python bug-sweep initiative + GitHub issue convention |
| [combat-simplification-scope](plans/combat-simplification-scope.md) | Consolidated scope of the combat-system simplification |
| [combat-model-sketch](plans/combat-model-sketch.md) | `CvCombatModel` engine API sketch (for review) |
| [combat-odds-baseline](plans/combat-odds-baseline.md) | Pre-refactor combat-odds baseline (Phase 0 regression reference) |
| [combat-phase3b-plan](plans/combat-phase3b-plan.md) | Route the AI's win-% through the binomial engine |
| [ai-logging-rollout](plans/ai-logging-rollout.md) | Plan to roll the tagged-logging structure across the AI codebase |
| [sea-ai-rework](plans/sea-ai-rework.md) | Naval AI weaknesses, the attack-sea cascade, and the logging driving the rework |
| [dead-code-xml-pass](plans/dead-code-xml-pass.md) | Tiered dead-code / dead-XML removal plan (feeds the clean-XML data model) |
| [unified-civilopedia](plans/unified-civilopedia.md) | Game-side of the unified Civilopedia: clean single-source content (XML/GameText/NewConceptInfo) + declarative loading. The website/converter is the separate `s2swebsite` project. |
| [gamespeed-simplification](plans/gamespeed-simplification.md) | `CvGameSpeedInfo`: Percents→named fields, GameTurnInfos collapse, derived calendar table (#196) |
| [fight-or-flight](plans/fight-or-flight.md) | `FIGHT_OR_FLIGHT` design capture (removal + plugin reimplementation) |
| [surround-destroy-removal-map](plans/surround-destroy-removal-map.md) | `SURROUND_DESTROY` removal map (deferred) |
| [specialist-rebalance](plans/specialist-rebalance.md) | Why specialists are weak + the lever plan: yield-modifier parity (done), GPP rebalance, city-amplifier mechanics (#317) |
| [derived-data-repository](plans/derived-data-repository.md) | Change-driven derived-data repository on the base objects (+ the `/units` HTTP endpoint) |
| [turn-time-optimization](plans/turn-time-optimization.md) | Turn-time hotspot tracking and the remaining levers |
| [unit-ai-valuation](plans/unit-ai-valuation.md) | Living report on unit-AI valuation bugs (hunter/dog persistence, tech-value fall-through, …) |
| [size-matters-ai](plans/size-matters-ai.md) | SM-literate AI (#395): strength-weighted force accounting, garrison consolidation, need-driven merges |
| [subdued-animal-ai](plans/subdued-animal-ai.md) | AI handling of subdued animals: resource-building spread, butchering valuation |
| [improvement-category-yields](plans/improvement-category-yields.md) | Building→improvement yield lever: Category-group replacement for per-improvement tags |
| [worker-stranded-tiles-reachability](plans/worker-stranded-tiles-reachability.md) | AI border tiles unimproved because unreachable by land; reachability + efficiency fix |
| [unified-prerequisites-and-constructibility](plans/unified-prerequisites-and-constructibility.md) | Unify the ~38 `Prereq*` families into the introspectable requirement model (#195) |
| [multimap-zone-rework](plans/multimap-zone-rework.md) | One-map viewport-region approach to multi-zone play (vs separate-CvMap switching) |
| [cross-entity-inversion-blueprint](plans/cross-entity-inversion-blueprint.md) | #428 cross-entity reference catalog (245 refs, consumer-site-verified). *Recovered 2026-06-17; inversion verdicts superseded by keep-on-source/deliveryguy (modifier-spec §6/§6.1) — kept for the catalog, not the verdicts.* |
| [cascade-modifier-inventory](plans/cascade-modifier-inventory.md) | Coverage checklist: every flat/percent modifier + its scopes + source accessors. *Recovered 2026-06-17; map rows to modifier families (modifier-spec §1), not the old `CASCADEFLAT_/CASCADEMOD_` bundle.* |
| [team-buildings](plans/team-buildings.md) | Empire/team-scope "buildings" as the per-city-autobuild replacement (#421). *Recovered 2026-06-17; concept current, the `CvCascadingModifierBundle` implementation superseded — see enabler-spec §5.* |
| [global-warming-mod](plans/global-warming-mod.md) | Global Warming mod scrapped — vestiges to remove (#436) + a concept worth revisiting. |

## Where docs go

- A note about **how existing code behaves** → `reference/`.
- A note about **a change you intend to make** (plan, scope, rollout, removal) → `plans/`.
- **Player-facing** rules, manuals, FAQs, key bindings → top-level [`docs/`](../../docs/).

## The repo is the single source of truth — mirror knowledge here

**Durable project knowledge must live in this repository, not in any one
developer's (or AI assistant's) private/local notes.** If a finding, decision,
plan, taxonomy, or "hard-won fact" is worth remembering, it belongs in a committed
file so everyone — every contributor and every agent — sees the same thing.

- Learned how something works, or why? → add/update a `reference/` doc.
- Started or scoped an initiative, or captured a design decision? → a `plans/` doc.
- Cross-cutting, must-not-rediscover facts → the relevant `AGENTS.md`
  ("Key Subsystem Knowledge").
- Player-facing rules → top-level [`docs/`](../../docs/).

Local assistant memory (e.g. a tool's per-developer memory store) is a personal
*index/cache* only — it is **not** a substitute for the in-repo copy, and the
in-repo copy is authoritative. When you record something locally, mirror the
shareable part here in the same change. Keep these docs current as the code moves.
