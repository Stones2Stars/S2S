# Cascade dry-calc — the .NET validation tooling

> **Model authority: [`../explanation/cascade-architecture.md`](../explanation/cascade-architecture.md)** (and the
> per-machine specs in [`../reference/cascade/`](../reference/cascade/)). That doc DEFINES the cascade/tally/modifier
> model — the three machines, the bidirectional flows, `enables`-vs-`requires`, the event spine, the rebuild-on-load
> tally. **THIS doc describes only the .NET dry-calc that implements and validates that model — the tooling, not the
> model.** Where it touches the model it **defers**; nothing here is a competing definition.
>
> Status: techs / civics / builds at 11/11 parity; resources-seen computed. Lives in `validation/` (a .NET 10
> solution, `S2S.Validation.slnx`) — NOT in `Sources/` (the live DLL only).

## Purpose

**Dry-model the cascade offline and validate it against the live engine.** We rebuild the three machines (enabler /
tally / modifier — defined in the model doc) in C# from the curated `Assets/Data/**` JSON + raw-observed state, then
diff our verdict against the engine's. The point is to prove the model equivalent so the legacy C++ maintainers can be
deleted (map-before-delete). Engine-calculated data enters **only at the comparison boundary**, never as a cascade
input — enforced structurally (see the guardrail below).

## How the dry-calc realizes the model

Faithful to the model's event-spine + rebuild-on-load (`cascade-architecture.md` §4–§5), exercised in .NET:

- The snapshot's held techs are **replayed through `researchCompleted(playerId, tech)`** — the *same* rebuild the live
  game runs on load (the tally is not serialized; it is replayed from events). `TechTally` takes the per-player events
  as the source of truth and **derives** the per-team held set + the world count.
- **Two-phase, per the model's build order** (tally → … → enabler): replay ALL events to fully populate the tally,
  *then* run the enabler — the required side reads aggregated counts, correct only once the tally is complete.
- The enabler is computed forward (`enable`/`disable`/`obsolete`/`replace`); **only the required side reads the tally**
  (the world cap, count thresholds) — the core enabler is tally-free. The non-count `requires` (bonus connection,
  terrain, property bands) is "can I do it *here*" and stays out of scope.
- The only live-vs-dry difference is **"when"** — live events carry the turn; the dry replay doesn't, and the cascade
  doesn't consume it.
- Scope-correct naming carries into the tally classes: `TeamTally` (techs, team scope) · `EmpireTally` (civics,
  buildings, units, resources, empire scope) · `WorldTally` ("added by someone", world scope). "global" = world, never
  the legacy global-means-empire mislabel.

## Clean Architecture layering — the pollution guardrail

The layering is the *enforcement* for "the cascade may never be fed live gamestate": the domain physically cannot
reference the live-state DTOs, so engine data can only enter at the comparison boundary, and only when granted. The
strike rule (no rollerskating) is enforced by the project graph, not by discipline.

| Project | Role | References |
|---|---|---|
| `S2S.Model` | curated definitions (`TechInfo`, `BuildInfo`, `ConditionEvaluator`, …) | — |
| `S2S.Application` | contracts only (`ITally`/`ITeamTally`/`IEmpireTally`/`IWorldTally`, `IResearchCompleted`) | — |
| `S2S.Tally` | tally implementations (`TechTally`, `Team/Empire/WorldTally`) | Application |
| `S2S.Enabler` | the cascade domain (`TechCascade`/`CivicCascade`/`ResourceCascade`) — **pure** | Model, Application |
| `S2S.Parity` | live-state DTOs (`Snapshot`) + the comparison harness | Model |
| `S2S.Extractor` | pulls live state over HTTP | — |
| `S2S.Cascade.Cli` | composition root: replay events → run enabler → compare | all of the above |

**`S2S.Enabler` does NOT reference `S2S.Parity`** — it cannot see the `Snapshot`. The one place that reads the live
snapshot (translating it into events + the player→team map) is the composition root.

## Comparison oracles (engine-calculated — comparison ONLY, never an input)

Emitted per empire by the extractor (`Sources/Tools/CvHttpServer.cpp`): `availableTechs` (`canResearch`),
`availableCivics` (`canDoCivics`), `availableBuilds` (the build's `getTechPrereq` held — the *availability* half of
`canBuild`, not the per-plot half). Each is used ONLY to diff against our isolated computation.

## Out-of-scope detection

A tech/build whose `requires.build` references a **non-tech** prereq (a `BUILDING_`/`BONUS_` atom or a runtime
predicate) depends on a lower, not-yet-validated layer; it is detected from the data and **deferred** (shown, never
silently dropped). Surfaced: `TECH_LEAD_GLASS` (needs a glassblower building), `TECH_WATERPROOF_CONCRETE`, and the
worker-wonder builds `BUILD_GEOGLYPH`/`MACHU_PICCHU`/`MOAI_STATUES`.

## CLI modes (`S2S.Cascade.Cli`)

`<CityName>` / `compare` (techs), `civics` / `compare-civics`, `builds` / `compare-builds`, `resources`. The `compare*`
modes diff the isolated cascade against the engine oracle, both directions, bar = 0 in-scope mismatches.

## Next

Extend the spine forward (per the model): a `buildingCompleted` event → building tally, then the buildings enabler
(forward edges; the required side — wonder uniqueness, count thresholds — reaching via the tally). Then units.
Eventually the modifier machine on the same spine (the required side with a value outcome instead of true/false).

## See also
- [`../explanation/cascade-architecture.md`](../explanation/cascade-architecture.md) — **the model this validates** (authority).
- [`../reference/cascade/`](../reference/cascade/) — the per-machine specs (`enabler`/`tally`/`modifier`/`event-spine`/`data-model`).
