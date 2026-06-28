# Ranked target selection (`max:` + `orderedBy`/`orderedByDescending`) — design LOCKED, impl pending

> **Status:** design **LOCKED (owner 2026-06-28)** and spec'd in [json.md §3.3](../../specs/json.md). This note now
> tracks the **implementation TODO** (not yet built). It **extends** the existing `max:` (used by grants + conditions),
> so nothing existing breaks. Spelling: **`orderedBy`** (ascending) / **`orderedByDescending`** (descending) — the
> standardized LINQ-style UX; the earlier `rankedBy` working name is superseded.

## The need
Some effects target the **top-N cities by a metric**, not a boolean per-city condition:
- **largestCity happiness** — engine `getLargestCityHappiness` (`CvCity.cpp:5551`) applies a flat to a city whose
  `findPopulationRank() ≤ world TargetNumCities` (i.e. the empire's largest *cities*, plural — top-N, not the single
  largest). This is the [DEC-conditions-are-predicates] retirement target for the `largestCity` member — **blocked on
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

## Open / to formulate
- Exact key spelling (`rankedBy` vs `orderBy`), metric token names, and whether `min:` gets a symmetric "bottom-N".
- How `N = TargetNumCities` is expressed (world token) + the `/state` emission of it.
- The cascade selection mechanism (rank the empire's cities by the metric at projection) + parity tiebreak vs
  engine `findPopulationRank`.

## Related
- [DEC-conditions-are-predicates](../../architecture/decisions.md#dec-conditions-are-predicates) — the invention sweep this unblocks (`largestCity`).
- `Tools/Migration/curate_civic.py` / `curate_trait.py` — `iLargestCityHappiness` stays a `largestCity` member **until this lands**.
