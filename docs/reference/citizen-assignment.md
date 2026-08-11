# Citizen assignment — how a city seats its population

> How `CvCityAI` decides which citizen works which plot and which becomes a specialist. Behaviour as it is
> today. The VALUATION the decision reads is [legacy-value-calc-map](legacy-value-calc-map.md) §1; this page is
> the ASSIGNMENT machine on top of it.

## The one idea — ONE priority list, values calculated once

**A plot and a specialist are one set of options, not two questions.** A citizen may take a workable plot or a
specialist slot, both score on the same comparable scale, so the assignment SCORES every option once, ORDERS
them by value, and WALKS the order.

| step | what it does |
|---|---|
| `AI_scoreCitizenOptions` | the ONE scoring body — every free workable plot + every valid specialist type, scored into a caller-owned list ([DEC-single-implementation](../architecture/decisions.md#dec-single-implementation)) |
| `AI_fillCitizensByPriority` | sorts that list descending and seats the whole unassigned population from it |
| `AI_addBestCitizen` | the single-placement entry point (the juggle pass uses it); reads the SAME scoring body |

⛔ **The retired shape searched each side for its OWN winner and compared the two winners, once per citizen** —
a priority list of length two, rebuilt from scratch for every placement. A 40-population city paid ~2,400 scored
evaluations (40 specialist types + ~20 free plots, per citizen) to re-derive an ordering that had barely moved.

## ⛔ THE TWO OPTION KINDS CONSUME DIFFERENTLY — a sort alone does not express it

This is the part a plain "sort and walk" gets wrong, and it is why the walk carries two cursors rather than one
index:

- **A SPECIALIST slot is REPEATABLE at a constant score** — a city can hold many merchants — so the walk **holds
  position** on it while it stays valid.
- **A PLOT is UNIQUE** — so it is consumed and the walk **advances past** it.

Both cursors move only forward, so a whole fill is `O(options)` rather than `O(citizens × options)`.

## ⚖ EMPHASIS — an emphasis PROMOTES what was asked for and SUPPRESSES what was not (owner)

**Both halves, or it does nothing.** An emphasis is a ratio shift between channels, so promoting one channel
without suppressing its rivals moves the ranking only by the promotion — which is how food emphasis came to be
roughly half the strength of the other two and read as inert (owner: *"emphasis has never really worked
properly for the longest time"*).

| emphasis | promotes | suppresses |
|---|---|---|
| production | production ×1.30 | food ×0.75 · commerce ×0.60 |
| commerce | commerce ×1.30 | production ×0.75 · food ×0.80 |
| **food** | food ×1.30 | production ×0.75 · commerce ×0.60 |

⚑ **The suppression factor is keyed on the channel being SUPPRESSED, never on who is suppressing it** — which
is what makes the table derivable rather than three hand-tuned sets, and what let food join it without
inventing a magnitude. ⛔ **Each channel is suppressed AT MOST ONCE**, structurally: one pass per channel, not
one pass per emphasizer.

> **⛔ AN EMPHASIS MUST REACH THE DECISIONS TAKEN *BEFORE* THE MULTIPLIERS, NOT ONLY THE MULTIPLIERS.** The
> SLAVERY TRANSLATION decides whether a tile's food counts as food or is re-booked as production (whip
> fodder), and it ran ahead of the emphasis stack — so emphasis could never reach it, and the food-emphasis
> block then scaled the slavery term, which is added to the production value. **Asking for food raised the
> value of working food AS HAMMERS and left food itself zeroed.** Emphasizing food therefore REFUSES the
> translation outright: the player has said grow.
> ⚠ Read this as the general shape, not one quirk — an emphasis that is applied only as a final multiplier
> cannot influence any branch that already ran, and the branches are where the ranking is actually decided.

> **⛔ A TILE THAT CANNOT FEED ITS OWN WORKER IS NOT A FOOD TILE, SO EMPHASIS DOES NOT EXEMPT IT.** The ÷16
> penalty on a plot failing `AI_potentialPlot` used to be waived while emphasizing food. That test fails a tile
> precisely when working it costs more food than it returns, so the waiver asked a city that wants to grow to
> seat citizens on net LOSSES. It was invisible only because the test it guards could not answer false; the
> waiver is gone.

## What re-orders the list, and what does not

- **The GROWTH GATES re-order it.** `AI_avoidGrowth()` / `AI_ignoreGrowth()` are what every score is conditioned
  on, so they are re-read per citizen (cheap) and a flip triggers exactly ONE re-score.
- ⛔ **A specialist hitting its cap does NOT.** It simply leaves the list.
- ⚠ **`isSpecialistValid` reads a CITY-WIDE total-specialist cap** (`getSpecialistCount(e) + iExtra <=
  getMaxSpecialistCount()`), not only the per-type one — so taking ANY specialist can close EVERY specialist
  option. That is an O(1) re-check per assignment; it is never a reason to re-score.

## ⛔ A NON-POSITIVE OPTION IS NOT TAKEABLE — a rule, not a tie-break

The valuation seeds both bests at `0` and compares with `>`, so an option scoring **`≤ 0` can never win** and the
citizen is left **UNASSIGNED** instead. ⚠ This is not a corner case: a third of recorded decisions have no
positively-valued plot available at all, so dropping the rule seats citizens on tiles the valuation has already
judged worthless.

## Order of operations in `AI_assignWorkingPlots`

1. `verifyWorkingPlots` — drop plots no longer workable.
2. Force the authored specialist minimums; cap any type over its maximum.
3. Always work the home (centre) plot.
4. `AI_removeWorstCitizen` while over the population limit.
5. **`AI_fillCitizensByPriority`** — the score-once/order/walk fill.
6. Remaining free specialists seated via `AI_addBestCitizen`.
7. `AI_juggleCitizens` (AI or automated cities only) — remove-worst-then-add-best passes.

⚑ **The whole run is bracketed by `startCitizenJuggling` / `endCitizenJuggling`**, which defers the side-effect
layer so a run's probe mutations replay their NET once rather than churning consumers per probe.

## The valuation the walk reads

`AI_plotValue` and `AI_specialistValue` both bottom out in `AI_yieldValue` (memoized per city in a 16-entry LRU
keyed on the yield vector + the condition flags, cleared on every specialist and worked-plot change), and each
then adds its own kind-specific terms:

| side | shared term | added on top |
|---|---|---|
| plot | `AI_yieldValue(yields, NULL, …)` | improvement-upgrade blend, the `/16` potential-plot penalty, bonus-discovery adds, the upgrade bonus |
| specialist | `AI_yieldValue(yields, commerce, …)` | great-people rate, keyed XP, wellbeing, property sources, underworld, the ×1.75 emphasis |

### ⛔ EVERY INPUT ARRIVES ×100, AND THE EVALUATION NEVER SCALES

Both scores are built entirely from ×100 cascade values — plot yields via the `getYields()` group read,
specialist yields/commerce from the ×100-native `CvPlayer::specialistYield` / `specialistCommerce`, and the
GPP / keyed-XP / wellbeing / underworld terms straight off the info. **Nothing reduces anywhere in the chain**
([DEC-fixedpoint-x100](../architecture/decisions.md#dec-fixedpoint-x100)); the single `÷100` lives at the read
edge (Python / the `Cy` bindings).

⚑ **A score is only ever compared against another score, so its absolute scale CANCELS.** That is why no
conversion is needed: a calibration constant that MULTIPLIES its yield (`iBaseProductionValue`,
`iBaseCommerceValue[]`, `iMaxFoodValue`) carries the scale for free, so whether a weight multiplies 15 or 1500
ranks identically.

> **⛔ SCALE-INVARIANCE IS A PROPERTY OF MULTIPLIED TERMS ONLY — AN ADDITIVE CONSTANT IS NOT INVARIANT, AND
> NEITHER IS A COMPARISON.** *"The calibration constants all multiply their yield"* was the premise the ×100
> conversion was made on, and it is FALSE for three shapes that sit in the same arithmetic:
> - a **BARE ADDEND** (`iValue += 2048`) — it keeps its old magnitude while everything around it grew 100×, so
>   it silently stops mattering;
> - a **COMPARISON against a whole-number threshold** (`iFoodPerTurn > iHighGrowthThreshold`) — the test flips
>   to always-true or always-false, and whatever it gated becomes unconditional;
> - a **`min`/`max` whose arms are on different planes** — one arm wins every time and the other clause is
>   unreachable.
>
> ⚑ **Each fails SILENTLY and in a different direction, which is why they need enumerating rather than
> watching for.** The measured instances: the clause that FORCES a starving city onto its moderate-food tiles
> became worth a few percent of an ordinary plot; the damper meant for cities *already* growing fast pinned at
> its floor and took **×0.26 off every city's food growth value**; and the bad-plot filter
> (`AI_potentialPlot`) could only answer false for a tile yielding literally nothing.
> ⇒ **When converting a scoring function to ×100, the census is every ADDEND, every literal COMPARAND and every
> mixed `min`/`max` — not the multipliers, which are the ones that need no attention.** A whole-number operand
> LIFTS to meet the yields; the yields are never reduced to meet it
> ([DEC-fixedpoint-x100](../architecture/decisions.md#dec-fixedpoint-x100)).

⛔ **The one requirement is that every input shares ONE scale — and a partial conversion is worse than none.**
The worked failure: five of the specialist's six info reads carried a `/100` while the keyed XP read did not,
so XP entered 100× larger than everything it was summed with and became **94% of the specialist's score**
(measured: `xpPart` 1083 against `yieldPart` 61). Specialists then beat plots ~90% of the time despite LOSING
the shared yield term 686 to 62 — the plot valuation was never at fault.
⚠ **A `× percent / 100` is applying a percentage, not reducing a scale** (the ×1.75 emphasis, the
military-production modifier, the 40/60 improvement blend). Those stay.
⚠ And the tell that a reduce is misplaced is a caller **re-inflating** it: a `* 100` on the far side of a
getter that just divided by 100 means the reduce belongs at neither site.

⚑ **Unifying a fractured scale is BEHAVIOUR-NEUTRAL wherever the data carries no decimals, and that is
checkable rather than asserted.** The plot substrate (terrains · features · improvements · bonuses · routes)
authors **zero** fractional yields, so `CvPlot::getYield`'s retired `÷100` was lossless and every threshold
lifted ×100 ranks identically — the conversion changed no decision. Where the data DOES carry decimals the
change is the repair, not a rebalance: **20 specialist flats are fractional**, and the reductions were
flattening `1.5 → 1` and `0.4 → 0`.

> **⚖ WELLBEING IS COUNTED IN WHOLE FACES, AND THAT IS THE EXPECTED BEHAVIOUR (owner).**
> `healthValue` / `happynessValue` iterate ONCE PER health or happiness face
> (`for (iI = 0; iI < iAddedHealth; ++iI)`), so their first argument is a **LOOP BOUND, not a magnitude** and
> reduces at that point of use — you cannot iterate 1.5 times. ⛔ This is NOT the banned interior reduction and
> must not be "fixed" by passing the ×100 value: doing so runs the loop a hundred times over and inflates every
> wellbeing term by the same factor. ⚠ A fractional authored face is therefore floored HERE by design; the other
> reader of the same data (`AI_countGoodSpecialists`) sums rather than loops, so it keeps the fraction.

## Observability

- **`[CIT/assign/cand]`** — one placement decided: both kinds' best REMAINING option with their values on one
  line, so the specialist-vs-plot ratio is directly readable.
- **`[CIT/assign/specval]` / `[CIT/assign/plotval]`** — one candidate's score split into the shared **yield
  term** and the **final** value. `final − yieldPart` is that kind's non-yield contribution, which is the axis
  the two sides differ on.
- **`[CIT/assign/run]`** — one completed run (runs per city per turn is the churn shape).

All are level 3 (the per-candidate tier, [observability.md](observability.md)), so they cost nothing until asked
for.

## See also
- [legacy-value-calc-map.md](legacy-value-calc-map.md) §1 — the yield/commerce valuation these scores read.
- [yields-growth.md](yields-growth.md) — the food/growth mechanics the growth gates test.
- [../specs/modifier.md](../specs/modifier.md) §6 — `freeSpecialists` / `allowedSpecialists`, the deposits that
  set the caps this walk re-checks.
