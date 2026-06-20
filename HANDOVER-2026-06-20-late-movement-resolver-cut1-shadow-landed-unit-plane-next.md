# Handover — 2026-06-20 late — movement RESOLVER cut-1 + cascade shadow LANDED (Assert-clean); unit-plane channel next

> **Transient relay (a task list), not a source of truth.** All durable status is already in the repo docs cited
> below (`modifier.md` §6.6 STATUS block + `cascade-migration.md` phase-3 step-3/4). This file is deletable once read.
> Supersedes `HANDOVER-2026-06-20-night-movement-data-migrated-engine-consumption-next.md` (its Next #1 is now done).

## Where we are (branch `json-data-migration`, working tree, UNCOMMITTED)

Phase-3 of the cascade rework (movement/range). The previous handover's big bite — **the engine movement
CONSUMPTION (resolver) + the shadow diff column** — is **DONE for cut 1 (the plot-substrate channel) and
Assert-clean**. Operating mode stays BUILD-WHOLESALE / validate-after (`cascade-migration.md` §2). Owner
builds/validates; do not commit (per BUILD-WHOLESALE the cut to `main` is the owner's later step).

## Landed this session

1. **`Sources/Cascade/CvCascadeMovement.{h,cpp}` — the movement RESOLVER (new module, picked up by the recursive
   fbuild glob, no `.bff` edit).** Reproduces the `CvPlot::movementCost` branch tree faithfully (the same tree as
   `CvHttpServer::decomposeMoveCost`) but sources the **plot-substrate** cost — terrain/feature `identity.movementCost`,
   route `identity.movementCost` + `identity.flatMovementCost` — from the **migrated JSON** (`cascadeMovementSubstrate()`,
   a parse-once cache over `Assets/Data/{terrains,features,routes}`). `cascadeResolveMoveCost()` is the per-(unit,edge)
   resolver; `cascadeMoveClassify()` cause-tags + care-grades a divergence (reuses `ModifierCareLevel`).
2. **`/diagnostic/movementSweep` cascade-vs-legacy SHADOW column** (`Sources/Tools/CvHttpServer.cpp`): per edge —
   `cascadeCost`/`cascadeDelta`/`cascadeCause`/`cascadeCare` (+ `cascadeSubstrateMiss`/`CASCADE_DIVERGE` flags); summary —
   `cascadeDiverge` count, `cascadeCauseHistogram`, `cascadeCareHistogram`, `cascadeSubstrate` parse stats
   (`terrainParsed`/`featureParsed`/`routeParsed`/`missing`). **Diffs the FRESH legacy `cost` (= `mc.iFinal`), NEVER the
   AI-cached `engineCost`** (the cache-collision quirk — `modifier.md` §6.6).
3. **CUT 2 — the UNIT-PLANE movement/range shadow (Assert-clean).** `cascadeUnitMoveAgg` (CvCascadeMovement)
   reconstructs the engine `m_iExtra*` stack from the migrated JSON — sums the unit TYPE + each held promotion +
   each held unitcombat (`movement.unit.{moves,moveDiscount}.flat`, `range.unit.flat` (unit base) + `air.unit.range.flat`
   (promo/combat delta), the `capabilities`; double-move is TYPE-KEYED). `/diagnostic/movementSweep` per-unit now
   carries the migrated-parts shadow (`cascadeMovesDelta`/`cascadeDiscountDelta`/`cascadeRangeDelta` + cap bools vs
   `UnitInfo` + own `getExtra*`) + summary `cascadeUnitDiverge`/`cascadeUnitCauseHistogram`/`cascadeUnitCareHistogram`/
   `cascadeUnitData` (parse stats), AND the **META** `cascadeSources[]` per-source attribution (each contributing
   type/promo/unitcombat's exact moves/discount/range/caps). Engine-only residue excluded from the migrated diff:
   `team.getExtraMoves`, `airRange` team/national, the commander/commodore cross-edge (cause `commanderCrossEdge`,
   care Weird), the flying-runtime `ignoreTerrainCost` OR. So a real `moves`/`moveDiscount`/`range` divergence
   isolates the unit-plane DATA migration.
4. **Docs updated (same change, branch-coupled → stay on branch):** `modifier.md` §6.6 (cut-1 + cut-2 STATUS blocks)
   + `cascade-migration.md` phase-3 step-3 ✅ / step-4 ✅.

**Cut-1 scope (the clean component boundary):** the resolver reads the **unit-side** aggregate (`baseMoves`,
`getExtraMoveDiscount`, the `isTerrain/Feature/HillsDoubleMove` predicates, `ignoreTerrainCost`, `flatMovementCost`) +
the hills/river/peak/denominator globals + the route per-tech delta (`CvTeam::getRouteChange`) **from the engine/GC** —
those are the unit-plane modifier family (self-accumulator, "largest surface, last") + Tier-G config + a tech-`enabled`
route modifier, NONE migrated yet. So **a divergence localises to the terrain/feature/route migration** = exactly the
deletable plot-substrate reads. This is the per-channel build (`shadow.md` §8), not a shim.

## ✅ FIRST VALIDATION — SHADOW CLEAN (2026-06-20, turn 1336, via `data-reader`)

Pulled `/diagnostic/movementSweep?type=full` for the human (player 0: 281 units / 2248 edges) and an AI
(player 1: 2361 units / 18888 edges). **`cascadeDiverge = 0` on BOTH** — every edge `cause=match` / `care=Fine`.
`edgeMismatchHuman = 0` (legacy self-check intact); AI `edgeMismatchNonHuman = 17` = the mapped AI movementCost-
cache quirk, confirmed unrelated by `cascadeDiverge=0`. `cascadeSubstrate`: terrainParsed 102/102, routeParsed
21/21, featureParsed 88, **missing 13**. The 13 missing are FEATURES, all **mapped to legacy `iMovement == 0`**
(verified vs `CIV4FeatureInfos.xml`: 12 have no `<iMovement>` tag → default 0; `FEATURE_CRATER_MEDIUM` is modular/
absent → 0) — the curator correctly omits the 0, the resolver falls back to legacy 0, the shadow stays clean. So
**the terrain/feature/route moveCost migration is validated faithful end-to-end** (the plot-substrate channel is
shadow-clean; the deletable legacy terrain/feature/route `getMovementCost()` reads are mapped). Re-validate after
any curator regen / on more saves before the owner signs off the channel.

## Validate again later (owner's human step — do NOT hand-hunt units)

Owner: build `Release` + `rebuild deploy`, launch, enable logging, end a turn. Then (delegate the read to the cheap
**`data-reader`** minion — never pull the raw dump into an expensive context):
`curl -s "http://127.0.0.1:7227/diagnostic/movementSweep?type=full&player=N"` → check **`cascadeDiverge`** + the
**`cascadeCauseHistogram`**/`cascadeCareHistogram`. Expectation: most edges `cause=match` (`care=Fine`); any
`terrainSubstrate`/`routeSubstrate` divergence is a terrain/feature/route `identity.movementCost` JSON value that ≠
legacy → fix in the **curator** (`curate_{terrain,feature,route}.py`, regen — never hand-edit `Assets/Data`), regen,
re-sweep. `substrateMiss` (care `Weird`) = an entity with no cascade `moveCost` datum (resolver fell back to legacy);
`cascadeSubstrate.missing` counts them. `edgeMismatchHuman` (the legacy self-check) must still be 0.

## Next — start here

The movement/range SHADOW (both channels: plot-substrate edges + unit-plane per-unit, with Meta per-source
attribution) is BUILT and Assert-clean. The owner is in BUILD-WHOLESALE mode (implement all, then one validate
pass + fix outliers — don't piecemeal diagnostics / restart the game N times). So:

1. **Validate the full sweep once** (data-reader): `cascadeDiverge` (edges) + `cascadeUnitDiverge` (units) + both
   cause/care histograms. Fix any `terrain/routeSubstrate` (curator regen) or `moves/moveDiscount/range/ignoreTerrain/
   flatMoveCost` (unit-plane data) outliers — map each to a named source via `cascadeSources[]`, never guess.
2. **Migrate the remaining unit-side modifier SOURCES** (the shadow's expected residue, separate from the resolver):
   `team.getExtraMoves(domain)` (team-scope movement credit), the `airRange` team/national contributions
   (`getNational{Missile|Flight}RangeChange`), and the route `CvTeam::getRouteChange` tech delta (a tech-`enabled`
   route `moveCost` modifier). Each is a curator/source-migration item; until migrated, the resolver/agg reads them
   from the engine (clean), and units affected tag `commanderCrossEdge` / show team-extra deltas.
3. Then the rest of the migration wholesale: **Improvement** (Tier C remaining, heavy — last plot-substrate),
   **Tier-G stragglers**, **GlobalDefines→config** (hills/river/peak/floor-90/MOVE_DENOMINATOR the resolver reads from GC).
4. Trajectory = build all → validate-after → owner signs off → cut to `main` → teardown.

## Watch-outs

- ⛔ **Everything UNCOMMITTED** (the two new Cascade files, `CvHttpServer.cpp`, the two docs). Owner builds from the
  working tree — don't switch branches, don't commit.
- ⛔ **`engine.py --write` CLOBBERS the whole curated DB** — never run it. Use per-entity `curate_<e>.py --write`
  (`Tools/Migration/README.md`). `python` here is real 3.14.
- The cascade resolver is a **parse-once** cache (`g_cmSubstrate`) — a reload in the same process keeps the prior
  parse (static data; harmless for the diagnostic). If the curated terrain/feature/route data changes, restart the game.
- The legacy `decomposeMoveCost` (in `CvHttpServer.cpp`) stays as the authority the cascade mirrors — the shadow is
  add-alongside, the legacy reads are not cut until the shadow is clean (map-before-delete).
