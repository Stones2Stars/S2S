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
  > **⛔ A CITY'S SUPPLIED RESOURCES MOVE WITH ITS OWN TILE — `CvPlot::setPlotGroup` IS THE ONE PLACE.**
  > `CvPlot::updatePlotGroupBonus` folds a plot's extracted resource, a city's free bonuses and the capital's
  > import/export. What an ACTIVE BUILDING supplies through `provides.bonuses` (§5a) is not in that fold: it
  > sits on the network because the CITY does, so when the city's own tile joins, leaves or changes group,
  > `setPlotGroup` subtracts the city's `providedCount` set from the group it left and adds it to the one it
  > joined. That covers the load-end re-color and every in-play merge or split alike.
  > ⚑ **The signature when this is missing is a producer's OWN output vanishing from trade while it stays on
  > site.** The rebuilt group lacks the supplied inputs, so a producer that needs one switches off and its
  > LOST crossing subtracts from a group that never held it (−1); when the input returns, the GAINED crossing
  > only brings it back to 0. A producer with no traded input is untouched, which is why some survive.
  > ⛔ **Do NOT put the supply back with a separate pass after the re-color.** It can only see the producers
  > still running at that moment, and anything it adds is doubled by the move above.
  > ⚠ The move walks a SNAPSHOT of `providedCount`: each push re-runs the operate fixpoint, which inserts into
  > and erases from that map.
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

