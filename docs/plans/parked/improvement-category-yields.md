# Improvement-category yield bonuses (building → improvement group)

## Goal

Let a building grant a yield bonus to **a group of improvements** (e.g. "all farmland",
"all forest-production tiles", "all mines") instead of naming each improvement and each
of its upgrade stages individually. This is the general form of the per-improvement
`<ImprovementYieldChanges>` lever (see `docs/plans/` siblings and the
`Yield-parity` blocks in `Regular_CIV4BuildingInfos.xml`) and it cleanly solves the
"bonus must follow the tile when it upgrades" problem by membership rather than by
walking the upgrade chain.

## What already exists (don't rebuild it)

- **A generic `Category` system** (`CvCategoryInfo`, `CATEGORY_*`) is cross-cutting
  and already wired into improvements and several other info kinds
  (`isCategory`/`getCategory`), currently used for UI/promotion-line/specialist
  grouping — never for plot yields. This is the natural substrate for
  "membership, not chain-walking" (below), but it is not part of the curator/JSON
  data pipeline today (`CATEGORY_*` is not a curated field), so any implementation
  needs to design that path rather than assume it exists.
- Per-improvement yield bonuses on buildings exist in some form today (the
  cascade/modifier model, not the legacy `IDValueMap`/`readPairedArrays` shape this
  plan was originally written against — see `docs/cascade.md` for the current
  yield-effect surface). Any implementation of the category idea below should target
  *that* surface, not the legacy accumulator shape described in earlier drafts of
  this plan.

## The gap

There is no effect that reads improvement categories — nothing says "grant yield Y to
every worked plot whose improvement is in category C." Solving this by chain-walking
each improvement's upgrade line by hand is brittle (a bonus must be re-listed at every
upgrade stage, and breaks the moment a new stage or a non-linear branch is added).

## Proposed design

### 1. Data: define improvement-group categories

Add to `CIV4CategoryInfos.xml`, e.g.:

- `CATEGORY_IMPROVEMENT_FARMLAND` — Farm, Vertical Farm, Seed Camp …
- `CATEGORY_IMPROVEMENT_FOREST` — Wood Gatherer, Lumberjack, Lumbermill, Treefarm, Hybrid Forest
- `CATEGORY_IMPROVEMENT_MINE` — Mine, Shaft/Modern/Core Mine, Mountain Mine line
- `CATEGORY_IMPROVEMENT_QUARRY`, `CATEGORY_IMPROVEMENT_PASTURE`,
  `CATEGORY_IMPROVEMENT_PLANTATION`, `CATEGORY_IMPROVEMENT_WORKSHOP`,
  `CATEGORY_IMPROVEMENT_WINERY` …

Optionally nest under a parent `CATEGORY_IMPROVEMENT` for "any improvement" effects.

### 2. Data: tag improvements

Add `<Categories><Category>CATEGORY_IMPROVEMENT_FARMLAND</Category></Categories>` to each
improvement, including every stage of its upgrade chain. **Membership replaces
chain-walking** — when Farm upgrades to Vertical Farm, both are in the farmland category,
so the bonus persists automatically. This also accommodates the planned dedicated
Pasture/Plantation upgrade lines: just tag the new tiles into the right category.

### 3. Engine: a category-keyed yield effect on buildings

The mechanism, at the concept level: a building's effect names a category instead of
a list of improvements; at the point where a building's per-improvement yield
entries are folded into a city's live improvement-yield state, expand each
category entry by walking the category's improvement membership once and folding
the yield into every matching improvement — the same shape as the per-improvement
case, just membership-driven instead of hand-listed. This keeps the hot per-plot
yield read, the help text, and AI valuation all working unchanged, since they only
ever see resolved per-improvement entries.

**This needs re-scoping onto the current cascade/modifier surface** (`docs/cascade.md`)
before it is actionable — the accumulator/schema shape this plan originally specified
(`IDValueMap`, `readPairedArrays`, `processBuilding`) belongs to a data model that
predates the JSON/cascade migration and no longer exists.

## Relationship to chain-walking

Categories are the *explicit* superset of walking an improvement's upgrade chain by
hand: they handle non-linear groups (e.g. all mine-like tiles regardless of upgrade
links), a tile belonging to multiple groups, and separate upgrade lines sharing a
category — none of which a linear chain-walk expresses. Any interim "auto-propagate
along the upgrade chain" approach is a lighter stopgap only; adopt the category-based
design as the durable foundation instead of building both.

## Open questions

- Granularity: one category per "tile family" vs finer (e.g. split early vs modern
  mines). Start coarse; nest later if needed.
- Should `GlobalImprovementCategoryYieldChanges` (player-wide) exist too, or city-scoped
  only for now?
- Convergence cleanup: once Pasture/Plantation/Winery get their own upgrade lines, retag
  so they no longer share Vertical Farm's category.
