# Load-end reconciliation

> Part of the **[08-the-machines-shape-components-host](../08-the-machines-shape-components-host.md)** spec.

- **Neither the counts NOR plot-group MEMBERSHIP are trusted from a save** (membership is derived state: routes +
  terrain-trade capabilities + ownership). The deserialized groups are drained and discarded; a load-end rebuild
  RE-COLORS membership from current state (`CvPlotGroup::colorRegion`, a flood fill from each plot) and folds
  the counts through the live entry points as each plot joins, announcing every bonus fact as a genuine crossing
  emit before the `GAME_LOAD_FINISHED` gate pass.
  ⛔ **This full demolish-and-repaint is the LOAD PATH ONLY** (`reInitialize` has exactly one caller,
  `CvGame::onFinalInitialized`) — every in-play group change is incremental (`recalculatePlots`'s early-out,
  `CvPlot::updatePlotGroup`'s targeted join). Reading the load teardown as the ordinary shape invites
  "optimizing" a full rebuild that does not run during play.
  > **⛔ THE RE-COLOR RE-FOLDS THE TILE HALF ONLY, SO THE BUILDING-SUPPLIED HALF MUST BE RE-PUSHED BEHIND IT.**
  > `CvPlot::updatePlotGroupBonus` folds a plot's own extracted resource, a city's free bonuses and the capital's
  > import/export — and nothing else. Every resource an ACTIVE BUILDING supplies through `provides.bonuses`
  > (§5a) was pushed into the DESERIALIZED group as that building resolved its dormancy in-read, and the
  > demolish-and-repaint throws it away: by re-color time the operating set has already CONVERGED, so
  > re-confirming a dormant/active verdict is a no-op that crosses and announces nothing (§3.2) — the
  > `GAME_LOAD_FINISHED` gate pass re-confirms `provided` and the supply is simply gone. The signature is a whole
  > CLASS of resource going invisible, never a wrong number: a resource supplied only by an active building reads
  > ≤ 0 in every member city's traded store, while tile-supplied resources beside it are unaffected.
  > ⇒ **The fix is a load-end re-push through `CvPlotGroup::changeNumBonuses`** (the same live entry point
  > `provides.bonuses` normally uses) — walking each city's converged `providedCount` into its NEW group, after
  > the re-color, so the crossing is announced as a genuine `SEVT_PLOTGROUP_BONUS_ADDED` rather than seeded
  > ([the load reseed](../../../spine/05-the-load-reseed.md#5-the-load-reseed) bans a warm-up walk that leaves consumers
  > deaf; a real crossing emit is not one).
- **The DORMANCY VERDICT is the operating-building fixpoint** (§3.2,
  [the pollution guardrail](../../validation.md#the-pollution-guardrail--engine-computed-data-never-rides-in)) — applied through the engine's
  disabled-building flag, never a hand re-derivation from legacy prereq getters, plus the two runtime-state legs
  the authored data does not carry (employed-population composition; the banned-non-state-religion policy). The
  load-end cross-city fixpoint — iterate {re-fixpoint each city's operating set → apply flips → the provides
  injections adjust the network} until stable — reconciles the serialized flags to the computed verdict inside
  the load bracket (a manufactured chain lights tier by tier: ore → wares → firearms). The iteration is
  WORK-LIST driven, each flip keeps the FULL per-flip side-effect surface (power, freshwater, employed
  population, traits, provides), and convergence is declared ONLY by a quiet FULL verify pass.
  ⛔ **BAKED-CONSUMER RE-RUNS:** an engine consumer that BAKES state on modifier changes (the trade-route
  ASSIGNMENT) runs during this fixpoint against not-yet-warmed packages and its baked result self-heals never;
  every such consumer is re-run ONCE after the load-end package warm.
- **The dynamic operate axes ride their events** — connectivity via the plot-group/network bonus events,
  vicinity (radius growth) via the culture-level event — routed into the operate re-check of dependents.

⚠ **A WHAT-IF asker can never iterate the frontier.** The frontier answers the CURRENT verdict only, so a gate
called with hypothetical arguments is served by `EnablerOverlay` (§8, "WHAT THE ENABLER IS NOT") — not by a swap
to `listedIds`.

---

