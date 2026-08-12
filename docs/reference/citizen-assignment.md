# Citizen assignment — how a city seats its population

> How `CvCityAI` decides which citizen works which plot and which becomes a specialist. Behaviour as it is
> today. This page is the ASSIGNMENT machine; the valuation it reads is `AI_yieldValue` and its two callers,
> decomposed under "The valuation the walk reads" below.

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

## ⚖ THE WHIP TERM IS NEVER WORTH TAKING (owner)

**"With how whipping currently functions, it is never worth it."** `iSlaveryValue` re-books a tile's food value
as PRODUCTION whenever pop-rush is available and the city is happy, steering citizen assignment toward whip
fodder for an action a rational player will not take. A whip costs upwards of eight population, and drafting the
same — *"it harms production for eras going forward."* Against an 8-population cost the food/production trade
the term models does not exist, so no coefficient tunes it into shape; the term is sized for a BTS-era 1–2
population whip. ⚠ Emphasis already refuses it outright — emphasis never cares about whipping.

## ⚖ GROWTH BEATS ALMOST ANYTHING — THE STANDING WEIGHT OF THIS GAME (owner)

**"The way this game works, growth > almost anything."** S2S is a very long game with an enormous build tree,
so a citizen added early compounds for the rest of it — which makes FOOD the dominant term in a citizen
decision by default, not merely a competitive one. ⛔ A valuation that ranks a food-rich tile below a
specialist, or below a hammer tile, is wrong on this game's own terms however defensible its arithmetic looks.
⚑ The weights already encode the intent — `iMaxFoodValue = 3 × iBaseProductionValue − 1`, i.e. food is worth
~3× production per unit — so a defect here is almost never the RATIO. It is something CAPPING or ZEROING the
food term before the ratio is ever applied, and both known instances did exactly that (below).
⚠ **It governs the DEFAULT.** An explicit emphasis is the player overriding this, so suppressing food while
production is emphasized is the model working — the ruling binds where nothing was asked for.

## ⚖ A BASE YIELD ALWAYS OUTWEIGHS A COMMERCE YIELD (owner)

**Food and production are worth more than the commerce channels they compete with, in every citizen decision.**
A plot's worth is what it PRODUCES, and a tile carrying real food and hammers must not lose to an option whose
output is gold or research — so no state of the city may drive a base-yield term below the commerce terms it is
being ranked against.

> **⛔ THE `min` ON FOOD IS THE SHAPE THAT BREAKS THIS.** `iFoodValue = min(iFoodGrowthValue, iMaxFoodValue ×
> food)` makes food worth **the lesser** of its growth contribution and its own quantity, so the moment the city
> stops wanting to grow — avoid-growth, the happy/health cap, the growth damper — food is worth approximately
> NOTHING and the tile is valued on its hammers alone.
> ⚑ **Measured:** a 7-food / 2-production / 3-commerce sea plot caps at 44 × 700 = 30,800 on the food leg, but
> under a collapsed growth value scores ~3,000 (its production) + ~30 (its commerce) — against specialists at
> ~5,400 in the same city. The best tile available goes unworked, and it is always the same tile class, because
> the verdict is a pure function of the yields and the growth state.
> ⇒ **Growth value is an ADDITION on top of food's intrinsic worth, never a CEILING over it.** The food a tile
> yields does not stop existing when growth is unwanted: it offsets consumption and feeds the very specialists
> that are out-scoring it.
> ⚑ **THE DECIDING FACT IS THE ENGINE'S OWN (owner): SURPLUS FOOD IS NOT WASTED.** `CvCity::doGrowth`
> SUBTRACTS the threshold and the remainder rolls into the next bar, so every point of food eventually
> becomes a citizen — which is why a cap on food's value can never be right in general.
> ⚖ **The ONE state where a cap IS right is the one the engine discards food in:** under avoid-growth it
> PINS `m_iFood` at the threshold, so surplus above it genuinely evaporates. The growth-value cap applies
> there and nowhere else.
> ⚠ **Emphasis MASKS this rather than fixing it** — emphasizing food lifts the term back above the specialists,
> which is why the defect reads as "emphasis is required to get sane assignment" instead of as a food-valuation
> bug.

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
> its floor and took **×0.26 off every city's food growth value**; the bad-plot filter
> (`AI_potentialPlot`) could only answer false for a tile yielding literally nothing; and
> `AI_getPlotMagicValue`'s `min(consumptionPerPop, 2×bestYield)` reductant took the ×1 arm against a ×1
> numerator and always exceeded it, so `AI_countGoodTiles` returned 0 on every call for the life of the mod —
> and every one of its five consumers (the settler gate `bGrowMore`, `allowedShrinkRate`, the whip gate, the
> food-emphasis auto-trigger, the growth clamp on `iPopToGrow`) was reading a constant — until the ×100
> conversion put both arms on the same plane and made all five live for the first time.
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

## Dirtying the assignment — the ruled trigger set (owner)

`AI_setAssignWorkDirty` marks a city for a full `AI_updateAssignWork` re-run; today it is set from hand-wired
call sites across the engine plus an unconditional per-turn sweep. The mechanism (a dirty mark drained by a
re-run) is right and stays — the AI needs a way to be told the best plots may have moved.

⚖ **It LISTENS TO THE EVENT SPINE; no AI loop ever touches it directly (owner).** This is a ROUTING job, never a
judgement re-made per call site — the same shape the player-alert re-attach uses
([event-spine.md](../specs/event-spine.md)).
⚑ **Every trigger below is already a DOMAIN fact** — the plot substrate, the building/population/civic/trait
facts, the culture-level fact and the working-city fact all announce today. No new emit is a prerequisite here.

**The ruled set (owner):**
- a PLOT CHANGED inside the city's WORKABLE SET — not an upgrade only; pillage, bonus depletion and a chop are
  the same fact in the other direction, and the substrate already announces each per plot;
- a BUILDING FINISHED that actually changes specialist slots or plot output — tested against the building's own
  compiled entries, never fired on every completion (a building that authors neither changes nothing about
  which plots are best);
- POP ADDED (and symmetrically POP REMOVED);
- a CIVIC change;
- a TRAIT change;
- symmetrically, a such-building DESTROYED, and golden age starting or ending (it moves per-plot yields through
  the threshold bonus, [golden-age.md](golden-age.md)).

⚑ **`CvCity::canWork`'s gates name what the set above does not reach:** the workable SET ITSELF moving (working-city
reassignment; the radius growing with culture / `adds3rdRing`, adding tiles that were never candidates — no
per-plot fact announces this) and the water-work TEAM capability.

⛔ **Two gates are UNIT-MOVEMENT driven and must NOT ride this routing:** an enemy unit sieging a plot, and a
naval blockade. Unit movement never dirties a cache
([DEC-unit-modifiers-on-top](../architecture/decisions.md#dec-unit-modifiers-on-top)) — decide these
deliberately rather than discovering the churn in a profile.

⚑ **The instrument for finding today's live call sites is already built:** the setter emits the caller's
module-relative return address on every false→true transition, resolvable offline against the PDB.
⛔ Do NOT re-add an ungated flip to replace a cut maintainer's gated one — the legacy calls fire only when a
value actually CHANGED, so an ungated stand-in adds to the very churn the instrument measures.

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
- [yields-growth.md](yields-growth.md) — the food/growth mechanics the growth gates test.
- [../specs/modifier.md](../specs/modifier.md) §6 — `freeSpecialists` / `allowedSpecialists`, the deposits that
  set the caps this walk re-checks.
