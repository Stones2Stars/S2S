# The reverse index, and what is deliberately NOT one

> Part of the **[08-the-machines-shape-components-host](../08-the-machines-shape-components-host.md)** spec.

**The canonical reverse axis is `EDGEF_REQUIRED_BY`** ([reverse lookups are populated once, at load](../../../cascade/01-deposit-and-read.md#1-one-step-deposit-down-accumulate-read-o1)),
and a per-id bucket that duplicates it is a defect. ⛔ But the axis-flag lists (power / golden age / state
religion / the coarse religion-civic-tech lists) and the PROPERTY band index are **NOT** convergence targets and
must not be swept into one: the reverse pass deliberately excludes engine tokens, the plot substrate and
`PROPERTY_` bands, and **a coarse list matches a coarse event**. Reading the two populations as one uniform
"operate index" is exactly the mistake the spelled-out naming rule exists to prevent
([Sources/AGENTS.md](../../../../Sources/AGENTS.md) § Code Style).

⚑ **`civicAny` is coarse by the same logic, and that coarseness is a known gap for AI VALUATION, not just
re-gating.** `CascadeCondDeps::civicAny` unions every `requires civic` clause into one bool — enough to re-gate,
but not enough to answer "which civic gates this candidate." `CvPlayerAI::AI_civicValue`'s civic-choice building
valuation dropped its cross-category half-value damper (civic valuations are linearly combined across
categories, so a building gated by civics in two options could be counted at full value from both, risking
oscillating choices) without replacing it with per-civic precision. If choices start oscillating, the principled
fix is an id-keyed `civics` set on `CascadeCondDeps` — never reviving the whole-civic-database sweep that such a
set would replace.

⛔ **THE PLOT PLANE CARRIES NO `EDGEF_REQUIRED_BY` AT ALL, AND ITS COARSE LIST IS THE `(kind, id)` PLOT-ATOM
INDEX.** `CvReversePass::rp_requiredByRefInfo` routes nine infotype prefixes and returns NULL for every other,
so **no terrain / feature / improvement / route / mapcategory info ever gains a REQUIRED_BY edge.** The coarse
list this section prescribes is therefore built by the enabler itself: `scanCondDeps` records each substrate id
the `requires` names, and each domain compiles `(PlotAtomKind, id) → candidates` — read by
`onPlotAtomChanged`, fanned over the plot's own `workableByCities()`.
⚑ **A TERRAIN fact also seeds the MAPCATEGORY atoms**, because a plot's categories are derived from its terrain
(`CvPlot::getMapCategories` forwards to the terrain info) and have no fact of their own; `plotAtomSeeds` is the
one place that hop lives.
⚑ **The bare plot BITS ride the verdict fact, not a substrate id.** `HAS_RIVER` / `HAS_COAST` / `IS_WATER` and
their kin name no entity, so they index by their `CASC_PRED_*` id and re-gate off
`SEVT_PLOT_PREDICATE_ADDED / _REMOVED` — which is exactly why that fact exists beside the substrate ones
([spine.md](../../../spine.md): one says what the tile CARRIES, the other what it MEANS).
⚠ **Reading the empty reverse edge instead FAILS SILENTLY, which is why this is spelled out**: the walk
succeeds, finds nothing, and re-gates nobody — indistinguishable from "no candidate needed re-gating" at every
observation point, including a census read taken when nothing has changed since load. The index
therefore reports its own size at load (`[ENABLER/plotatoms] atomKeys=… atomEntries=…`), so an index that
compiled EMPTY says so.

⚑ **And this is what keeps `GATE_DYNAMIC` meaning what §7.1 says it means.** `scanCondDeps` marks `dynamic` for
any atom it does not NAME, so every axis that later gained a precise route must also gain a case there — or it
keeps marking the catch-all, and the "small load-compiled set" becomes the whole registry (the plot substrate
alone put every building in it, and every fact routed through the class then re-gated everything). ⛔ So when
you wire a new route, remove its axis from the catch-all in the same change; the residue is the genuinely live
state — `existedFor`, `IS_CAPITAL`, the count tokens, connection.

> **⛔ AN AXIS HAS TWO SPELLINGS AND THEY MUST NOT DISAGREE — this is the failure mode, not a tidiness point.**
> `scanCondDeps` meets most axes twice: as a PRESENCE atom (`BONUS_IRON`) and as a PREDICATE
> (`{HAS_BONUS: BONUS_IRON}`). Narrowing one and leaving the other keeps the whole axis in the catch-all while
> the code reads as though it were routed — and the note justifying the surviving half is typically the one
> already retired beside it. ⚑ **Measured: the bonus axis had exactly that split, and closing it took the class
> from 2,674 of 5,180 buildings to 423.** ⇒ When you route an axis, grep BOTH branches.
>
> **⚖ THE THIRD DISPOSITION IS *STATIC*, and forgetting it is what puts a never-moving axis in a live class.**
> §3.2's rule is that an axis either has a fact and is routed on it, or is STATIC for the city's life and gated
> once at creation. A static axis therefore marks **nothing at all** — a plot's LATITUDE cannot change and a city
> cannot move, and a VICTORY condition is fixed at setup, so neither has a crossing to wait for and marking them
> dynamic bought a re-gate that could never change a verdict.
> ⚠ **`existedFor` is the neighbour that is NOT static and must stay in the residue:** the game YEAR advances, so
> an age-gated candidate genuinely crosses a threshold with no fact naming it.
>
> ⚑ **THE CLASS SIZE IS INSTRUMENTED, so a widening is observable rather than suspected** —
> `[ENABLER/gateclass] domain=… class=… members=… of=…` at load, beside `[ENABLER/plotatoms]`. Read `members`
> against `of`: a class approaching the registry size is not a bounded re-gate set, and every fact routed through
> it re-gates nearly everything. ⛔ Do not narrow this class by reasoning alone — the number is one line in
> `Cascade.log`, and the last two attempts to estimate it from the authored JSON were both wrong.

