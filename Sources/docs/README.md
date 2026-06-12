# Developer documentation

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
| [UnitAI_Selection](reference/UnitAI_Selection.md) | How the AI picks a unit's `UNITAI` and concrete unit type |
| [UnitSelection_Mechanics](reference/UnitSelection_Mechanics.md) | The selection step itself: role → chosen `UnitTypes` |
| [doProduction](reference/doProduction.md) | `CvCity::doProduction` — the per-turn city production step |
| [constructibility-and-prerequisites](reference/constructibility-and-prerequisites.md) | The unified `ConstructRequirement` model, the static enabler reverse-index (CABV turn-time), and help-text rendering |
| [declarative-info-loading](reference/declarative-info-loading.md) | `CvInfoUtil` / `getDataMembers` — how `Cv*Info` classes load from XML, the wrapper catalog, and the byte-identical migration recipe (#196) |
| [calendar-and-gamespeed](reference/calendar-and-gamespeed.md) | Era-driven turn counts & calendar (`CvEraInfo` pacing fields, `CvGameSpeedInfo` derived accessors, `CvDate` interpolation, Adapt channels) |
| [MapScript_Process](reference/MapScript_Process.md) | End-to-end lifecycle of a C2C mapscript (`C2C_World`) |

## Plans — work in flight

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
| [dead-code-xml-pass](plans/dead-code-xml-pass.md) | Tiered dead-code / dead-XML removal plan; XML→JSON web-Civilopedia north-star |
| [gamespeed-simplification](plans/gamespeed-simplification.md) | `CvGameSpeedInfo`: Percents→named fields, GameTurnInfos collapse, derived calendar table (#196) |
| [fight-or-flight](plans/fight-or-flight.md) | `FIGHT_OR_FLIGHT` design capture (removal + plugin reimplementation) |
| [surround-destroy-removal-map](plans/surround-destroy-removal-map.md) | `SURROUND_DESTROY` removal map (deferred) |
| [specialist-rebalance](plans/specialist-rebalance.md) | Why specialists are weak + the lever plan: yield-modifier parity (done), GPP rebalance, city-amplifier mechanics (#317) |
| [derived-data-repository](plans/derived-data-repository.md) | Change-driven derived-data repository on the base objects (+ the `/units` HTTP endpoint) |
| [turn-time-optimization](plans/turn-time-optimization.md) | Turn-time hotspot tracking and the remaining levers |
| [unit-ai-valuation](plans/unit-ai-valuation.md) | Living report on unit-AI valuation bugs (hunter/dog persistence, tech-value fall-through, …) |
| [subdued-animal-ai](plans/subdued-animal-ai.md) | AI handling of subdued animals: resource-building spread, butchering valuation |
| [improvement-category-yields](plans/improvement-category-yields.md) | Building→improvement yield lever: Category-group replacement for per-improvement tags |
| [worker-stranded-tiles-reachability](plans/worker-stranded-tiles-reachability.md) | AI border tiles unimproved because unreachable by land; reachability + efficiency fix |
| [unified-prerequisites-and-constructibility](plans/unified-prerequisites-and-constructibility.md) | Unify the ~38 `Prereq*` families into the introspectable requirement model (#195) |
| [multimap-zone-rework](plans/multimap-zone-rework.md) | One-map viewport-region approach to multi-zone play (vs separate-CvMap switching) |

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
