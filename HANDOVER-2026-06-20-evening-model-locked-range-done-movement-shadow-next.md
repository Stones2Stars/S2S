# Handover — 2026-06-20 evening — modifier model LOCKED, specialist reshape + range converter done, movement/range OBSERVABILITY is next

> **This is a transient relay (a task list), not a source of truth.** All durable knowledge is already in the repo
> docs cited below — this file is deletable once read. Nothing here is load-bearing.

## Where we are (branch `json-data-migration`)

The **modifier MODEL is fully locked and written down**, the **data side reshape is executed + committed**, and the
next phase is **movement/range observability → shadow → engine code** (a deliberate fresh start).

Two commits landed this session:
- `079ac4a53` — `#432 data: normalize specialists to the keep-on-source model + lock the movement/range model`
- `ad44c216e` — `#430 data: range converter (air-only) + finalize movement/range model`

(The roadmap update + this handover are the only docs left uncommitted at handover time; pre-existing
`Sources/` engine pilot, `.claude/`, old handovers, `Tools/ModifierCalc/` are intentionally NOT ours to commit.)

## Durable docs — READ THESE, do not reconstruct from this handover

- **The model:** [`docs/dev/reference/cascade/modifier.md` §6.5–6.7](docs/dev/reference/cascade/modifier.md) — home
  rule (own-output vs governing-deliverer; conditioner via `enabled`), `data≠runtime`, no-special-cases; **movement**
  (points/cost ledger, route override VERIFIED vs `CvPlot::movementCost`, `base=100=MOVE_DENOMINATOR`, road=`base/3`,
  linear `3N`, clamp-to-0); **range** (one family, air-only, siege=1 deferred for AI); volumetric-specialist (output
  vs count axes); `freeSpecialists`/`allowedSpecialists` `(A)` shape.
- **Ledger:** [`docs/dev/architecture/decisions.md` DEC-deliveryguy](docs/dev/architecture/decisions.md#dec-deliveryguy) (refined 2026-06-20).
- **Enabler pass-1 order:** [`docs/dev/reference/cascade/enabler.md` §2.0](docs/dev/reference/cascade/enabler.md).
- **Roadmap + the NEXT-PHASE plan:** [`docs/dev/json-migration/cascade-migration.md`](docs/dev/json-migration/cascade-migration.md)
  → "Update 2026-06-20" in the BLUF (the phase-3 steps live there, durably).

## Done this session (data side)

- **Specialist OUTPUT axis** — `SPECIALIST_BOOSTS` now lands building/civic/trait boosts ON the specialist in
  `enabled` form (new `curate_common.accumulate_conditioned`, deposit-vs-condition scope split). Zero keyed
  conditioners left.
- **Specialist COUNT axis** — `freeSpecialists {any,…specific}` + `allowedSpecialists {…specific}`, additive `(A)`
  leaf, free-on-top. `TechSpecialistChanges` corrected to `allowedSpecialists` (verified via inner `<SpecialistCounts>`).
  100% `(A)` clean across building/civic/trait/tech/specialist (generic handler in `curate_common` also routed).
- **Home corrections** — improvement tech-yields keep-on-source; building drops specialist-keyed (own-output);
  `RELIGION_BOOSTS` deleted; `TECH_BOOSTS` reduced to the one deferred route row; the ×100 tech-building double
  removed at source.
- **Range converter** — `curate_unit`: `iAirRange → range` top-level family (53 air units). `iAirRangeChange` →
  promotion-side modifier (no unit base data). Siege=1 deferred (AI runs ground ranged poorly).

## Next (start fresh) — phase 3, in order

Per `cascade-migration.md` "Update 2026-06-20" → NEXT PHASE:
1. **MAP** the existing observability surface for unit movement/range + plot `moveCost` (read `docs/dev/reference/observability/`:
   `plotsnapshot`, `path-generator`, `pathfinding`, `unit-upkeep-supply`, `http-server`). What's exposed vs the gap.
2. **ADD emitters** for the gaps — per-unit effective movement points + `range`, per-plot `moveCost` decomposition
   (terrain/feature/hills/route/discount/floor). C++ engine + a `/diagnostic` endpoint.
3. **Build the movement/range SHADOW** (analogue of `/diagnostic/modifierSweep`) and diff the converter output vs the
   live game systematically (data-reader minion for the bulk reads).
4. **Build + tune the engine consumption** of `movement`/`moveCost`/`range` (the unit-plane), shadow-gated.

One open trust-but-verify carried forward: the **ground ranged-attack mission vs `airStrike`** (owner believes shared
code → if so the unified `range` family is honest).

## Watch-outs

- `engine.py --write` still CLOBBERS the curated DB — never run it (use the per-entity `curate_*.py --write`).
- Range/movement are **data-landed but UNCONSUMED** — the engine ignores them today, so the reshape is inert until
  phase-3 builds consumption. That's why phase-3 leads with observe-then-shadow, not a blind engine build.
- `base=100` is the engine's *existing* `MOVE_DENOMINATOR` math, not a new approximation.
