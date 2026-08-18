# Ranked target selection (`max:` + `orderedBy`/`orderedByDescending`) — design LOCKED, impl pending

> **Status:** design **LOCKED (owner 2026-06-28)** and spec'd in [json.md §3.3](../../specs/json.md). This note now
> tracks the **implementation TODO** (not yet built). It **extends** the existing `max:` (used by grants + conditions),
> so nothing existing breaks. Spelling: **`orderedBy`** (ascending) / **`orderedByDescending`** (descending) — the
> standardized LINQ-style UX; the earlier `rankedBy` working name is superseded.

## The need
Some effects target the **top-N cities by a metric**, not a boolean per-city condition:
- **largestCity happiness** — engine `getLargestCityHappiness` (`CvCity.cpp:5551`) applies a flat to a city whose
  `findPopulationRank() ≤ world TargetNumCities` (i.e. the empire's largest *cities*, plural — top-N, not the single
  largest). This is the [conditions are predicates, never bespoke members](../../specs/json.md#35-predicates--a-systems-runtime-state-query) retirement target for the `largestCity` member — **blocked on
  this design**.
- **Wonders that grant to the X largest cities** — same selection shape on the `grants` side.

## Why NOT a predicate
`IS_LARGEST_CITY` as a bare predicate was tried and **rejected (owner 2026-06-28): "does not fly fundamentally."**
Ranking is a *selection/threshold* concern, not a yes/no state query — and it would need a world constant
(`TargetNumCities`) baked into a boolean. (The bare-predicate wiring was reverted.)

## The converged direction
**`max:` already exists in BOTH grants and conditions** (a count threshold, json §3.4). Extend it with an optional
**`rankedBy:` ordering** over an obvious metric:

```jsonc
"grants": { "cities": { "max": 5, "orderedByDescending": "CITY_SIZE" } }   // grant to the 5 largest cities (by population)
"happiness": { "empire": { "cities": { "flat": V, "max": "TARGET_NUM_CITIES", "orderedByDescending": "CITY_SIZE" } } }  // top-N cities get +V
```

- `max: N` + `rankedBy: METRIC` ⇒ **the top-N objects** of the plural target ordered by `METRIC` (descending).
  Without `rankedBy`, `max:` stays a plain count threshold (backward-compatible — nothing existing breaks).
- **Metrics:** `CITY_SIZE` (population) first; an **extensible registry** — "general rankings for more things as
  needed" (owner 2026-06-28).
- **N source:** a literal (wonders: `5`) **or** a world token for the largestCity-happiness case (the engine's
  `TargetNumCities`) — exact token spelling TBD when formulated (a `/state` world scalar; `targetNumCities` is **not**
  emitted today, so this also needs a (batched) engine `/state` addition).
- **Implementation hook (owner 2026-06-28):** the **sort/ranking step is added into cascade PARSING** — the parser
  recognizes `rankedBy` on a plural target and the cascade ranks the in-scope objects by the metric, selecting the
  top-N. One place, general for all future ranking metrics.

## ⚖ ONE ORDERING MECHANISM, USED EVERYWHERE (owner)

**"There is no reason why we can't use the same sorting/filtering in all places."** The ranked TARGET selection
here and the BUILD-LIST UI's filter/sort are the same operation — *order a set of objects by a named metric,
optionally keep the top N* — so they are ONE implementation with one extensible metric registry, not two
([the DRY single-implementation law](../../architecture/patterns.md#dry--one-implementation-per-calculation--evaluation-the-single-source-law)). json.md §3.3 already
calls the metric set "an extensible registry", so this is the registry being taken at its word rather than a new
concept.

⚑ **The one real difference to design for, and it is not a blocker:** a metric is either a PROPERTY of an object
that already exists (`CITY_SIZE` = population — the ranked-target case) or a VALUATION of a candidate that does
not exist yet (what a building would give me — the build-list case, answered by `expected*` through
[CvBuildListValuation](../../../Sources/UI/CvBuildListValuation.h)). Both are "metric(object) -> int"; only the
source differs. The registry must therefore be keyed by metric, not assume an intrinsic read.

⛔ Do NOT build a second ordering step for the UI. If the UI's sort needs a metric the registry lacks, ADD THE
METRIC — that is the extension point.

⚖ **THE CONTRACT IS SET-IN → SET-OUT (owner):** *"it is full set in -> filtered or sorted out."* The operation
takes the FULL candidate set and returns the narrowed/ordered one; it does not iterate a database, does not know
what produced the set, and does not know who consumes the result. That is what lets one implementation serve
both callers, because both already hold a full set:

- the **enabler frontier** hands out exactly that — `getAvailableBuildings` / `getAvailableUnits` /
  `getAvailableTechs` fill a caller-owned `std::vector<int>` ([enabler.md §8](../../specs/enabler.md)); the
  build list passes it straight in;
- the **modifier's plural target** resolves the in-scope objects (the empire's cities) and passes those in.

⚑ Consequence worth stating, because it decides the signature: the set element is an ID, and the metric is what
knows how to score an id. So the surface is `(set, metric, ordering, max)` in and a set out — never a
per-call-site comparator, and never a bespoke overload per object kind.

## What is BUILT vs what is LEFT

**Built:** the authoring vocabulary and the whole PARSE side. The curators emit it
(`curate_civic.py` / `curate_trait.py` write `max: "TARGET_NUM_CITIES"` + `orderedByDescending: "CITY_SIZE"`),
civics carry it in `Assets/Data`, and the compiled `CvModEntry` retains it (`orderedBySeg`, `rankMaxToken`)
through the reverse pass. Spelling is SETTLED — `orderedBy` / `orderedByDescending` (json.md §3.3); the earlier
`rankedBy` working name is dead and any remaining use above is stale wording, not an open question.

**Left:**
- The SELECTION step itself — rank the in-scope objects by the metric and keep the top N, at the ONE parse/
  projection hook, shared with the UI per the ruling above.
- The metric REGISTRY (`CITY_SIZE` first; the build-list valuation metrics join it).
- `TARGET_NUM_CITIES` as a resolvable world token — `CvWorldInfo::getTargetNumCities` exists as info data but the
  token is not emitted on `/state`, so a ranked entry currently applies UNRANKED.
- Whether `min:` gets a symmetric "bottom-N", and the tiebreak against engine `findPopulationRank` for the
  largestCity-happiness parity case.

## Related
- [conditions are predicates, never bespoke members](../../specs/json.md#35-predicates--a-systems-runtime-state-query) — the invention sweep this unblocks (`largestCity`).
- `Tools/Migration/curate_civic.py` / `curate_trait.py` — `iLargestCityHappiness` stays a `largestCity` member **until this lands**.
