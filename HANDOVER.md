# Handover 2026-07-03 — the modifier machine is LIVE; capability plane cut; the caching doctrine specced

> **Transient one-time relay** (AGENTS.md handover rules): a task list, nothing more. Every ruling, design, and
> result below lives in the durable docs cited inline — this file is deletable without loss. Branch:
> `json-data-migration` (60+ unpushed commits; pushing is the owner's call). The game runs the latest build from
> the working tree; the test save reloads the same turn every time (failure is free).

## State of the world (verify, don't trust — re-read the cited docs first)

1. **The modifier getter pair RUNS ON the cascade, live.** `CvCity::getYieldRate100` / `getCommerceRateTimes100`
   return the ACCUMULATOR — the modifier.md §1 substrate (standing plugin-number components on
   `CvCity::m_cascadeRateSlots`, `CvDerivedCacheSet`-driven, per-player epochs, plots/trade/slider read live,
   eager load-end warm-up in `CvGame::onFinalInitialized`). Full record incl. the measured road (three perf
   iterations, one measured revert): **`docs/plans/structural-cleanup/modifier-substrate.md`**.
   Steady state: pctStack ~4.6k calls/~25s, accRefresh ~10k/turn, turn feel at baseline.
   Nets standing: `[SLOT]` (accumulator vs the calculator-oracle) ~31-36 tiny diffs; `[GETTER]` (vs the legacy
   in-body expression) ~360-434/1295 = the accepted-class repair map. Legacy machinery is INTACT (flip, not cut).
2. **The capability plane is fully cascade** (flip waves #1+#2 cut and proof-turned; ONE union; zero legacy
   oracles remain): **`docs/specs/capabilities.md`**. Retired save fields ledgered in **`savemigration.txt`**;
   the save-retirement mechanism (named `WRAPPER_SKIP_ELEMENT`, two-stage) grounded in
   **`docs/reference/engine.md`** §Save/load — ⛔ read it before ANY serialization-touching cut.
3. **`CvDerivedCache` is BUILT** (`Sources/Infrastructure/CvDerivedCache.h`, both forms, spec holes plugged as
   header contract rules); consumers: the CvPlot yield cache + the city rate slots. Spec + the whole caching
   doctrine (turn-boundary principle, snapshot-then-recalc, ONE unified turn-end rebuild, data-derived
   event→cache routing, eager load builds as save-safety): **`docs/architecture/state-repositories.md`**.
4. **Parked with full articulation:** the AI build-queue-parity rework (kills the RTS-style live production
   choice; the cache becomes the fairness mechanism): **`docs/plans/parked/ai-build-queue-parity.md`**. The
   empire-scope half of the loophole is ALREADY dead (no epoch bump on building completions — measured 5x
   regression, reverted); the own-city half stays fresh for parity until that rework.

## Next tasks, in recommended order

1. **`[SLOT]` component-decomposed diff sampling** — extend the `[MODIFIER/slot]` diff emit with per-component
   slot-vs-calc pairs (PCT/CBASE/CSPEC/yc100) so the diverging component names itself. Target: the SYSTEMATIC
   Seoul research **+1049** (five identical sightings) + the ~30 tiny commerce residues. Map, don't guess.
2. **The compiled deposit index** (load-time strings→ints over `CvJsonInfo::deposits`) — the deep perf lever
   (percentStack walks all ~1000 building infos with string compares per call) AND the generator of the
   data-derived event→cache routing the owner specced (state-repositories.md end-state).
3. **Next modifier channels onto the proven pattern** (health/happiness/defense/maintenance/GP-rate — ALL ruled
   pre-cutover, cutover.md rulings): per channel, spec → StoneBase parity → components on the slots + recompute
   fns → nets → flip. The substrate is the template; each channel is now mostly mechanical.
4. **The enabler-gate flip** (gates at recorded 0; NPC lockdown loss ACCEPTED —
   `docs/plans/structural-cleanup/data-migration-remaining.md` stronglyRestricted ruling) and/or the
   **building-attributes lane** (static-info getter flips; data proven; `isMapCentering` rides with it).
5. **Follow-up migrations onto `CvDerivedCache`**: the specialist commerce/yield getters, the
   building-commerce recompute (state-repositories.md names them).
6. **Housekeeping:** 60+ unpushed commits (owner's call); unresolvedFks=2 named data typos
   (FORCE_TEAM_ELIGIBLE, PROMOTION_COMPLEX_AGGRESSIVE); the modifier legacy CUT (accumulators/apply-loops,
   cascade-engine-430.md §4) waits until the owner rules the flip proven long enough — the two binding
   serialized-member rules (capabilities.md) apply when it comes.

## Operational reminders (the ones that bit)

- Build from `Sources/`: `powershell.exe -NoProfile -ExecutionPolicy Bypass -File "../Tools/_Build.ps1" <Config> <verbs>`
  — the cwd drifts after commits (cd back). Kill the game BEFORE `Release deploy` (DLL held open); verify with
  `md5sum Build/Release/CvGameCoreDLL.dll Assets/CvGameCoreDLL.dll`; relaunch ONLY via the absolute-path
  `cmd //c "C:\\code\\s2s\\s2s\\agentstart.bat"`; poll `http://127.0.0.1:7227/` until `hello world`.
- ⛔ Never live-read the `.log` files; use ONE auto-reconnect SSE capture loop on `/events` (connect BEFORE the
  turn ticks) + the `/state`+`/computed` endpoints; delegate big pulls to the `data-reader` agent.
- The owner runs the end-turns; ask for them explicitly and read the per-turn flushes from the capture.
- `PROFILE_FUNC` is NOT wired to `[PERF/phase]` (that is `PERF_SCOPE`); cascade internals are visible via the
  `CascadePerf` counters in `[MODIFIER/perf]` (now incl. `accRefresh`).
- The unity build re-batches when files are added — a new `LNK`/type error in an untouched file is usually a
  latent missing include exposed by re-batching (fix the include).
