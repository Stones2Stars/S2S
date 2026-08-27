# 3.3 Targets — *what kind of object* receives a deposit (always PLURAL)

> Part of the **[03-the-shared-vocabulary](../03-the-shared-vocabulary.md)** spec.

The differentiator between scope and target is **grammatical number.** A scope is singular (`empire`, `city`,
`plot`). A **target** is plural — `plots` · `units` · `cities` · `areas` · `empires` — and means **all objects of
that kind in the scope**, filtered by an optional predicate. So `empire` (singular) is unmistakably the *reach*
and `plots`/`units` (plural) the *receivers*, even when a root is shared. A deposit with **no** plural target
lands on the scope object itself (the city — the common case). Full deposit syntax: §6.

> **Ranked subset — `orderedBy` / `orderedByDescending` + `max:`.** A plural target may be
> narrowed to the **top-N (or bottom-N) by a metric** via an ordering qualifier plus the existing `max:` count:
> `cities.{ max: 5, orderedByDescending: CITY_SIZE }` = the **5 largest cities** (by population). `orderedBy` =
> ascending, `orderedByDescending` = descending (the standardized LINQ-style spelling); `max: N` caps the selection
> to N after ordering. This is a pure **extension** of `max:` (which both `grants` and conditions already use) — a
> bare `max:` with no ordering stays a plain count threshold, so nothing existing changes. Usable in **grants,
> conditions, and modifier targets**. **Metrics** are an extensible registry, `CITY_SIZE` (population) first. `N` is a
> literal (`max: 5`) or a world **token** when it tracks an engine constant — the *largest-cities* case is
> `max: "TARGET_NUM_CITIES"` (the world-size target city count; engine `getLargestCityHappiness` =
> `findPopulationRank() ≤ TargetNumCities`, i.e. the empire's largest *cities*, plural — **not** the single largest).
> Engine note: the cascade adds the sort/select step in **parsing**, one place for all ranking metrics.
> Implementation TODO: [plans/parked/ranked-target-selection.md](../../../plans/parked/ranked-target-selection.md).

