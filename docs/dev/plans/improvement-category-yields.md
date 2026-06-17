# Improvement-category yield bonuses (building → improvement group)

## Goal

Let a building grant a yield bonus to **a group of improvements** (e.g. "all farmland",
"all forest-production tiles", "all mines") instead of naming each improvement and each
of its upgrade stages individually. This is the general form of the per-improvement
`<ImprovementYieldChanges>` lever (see `docs/dev/plans/` siblings and the
`Yield-parity` blocks in `Regular_CIV4BuildingInfos.xml`) and it cleanly solves the
"bonus must follow the tile when it upgrades" problem by membership rather than by
walking the upgrade chain.

## What already exists (don't rebuild it)

- **Generic `Category` system.** `Assets/XML/GameInfo/CIV4CategoryInfos.xml` defines
  `CategoryInfo` types (`CATEGORY_*`). Categories may themselves belong to parent
  categories (nestable), so a hierarchy is possible. Schema: `Categories` → `Category`
  (text) in `C2C_CIV4GameInfoSchema.xml`.
- **It is cross-cutting and already wired into improvements.** `CvImprovementInfo`
  reads `<Categories>` (`CvImprovementInfo.cpp:921` `SetOptionalVector(&m_aiCategories,
  L"Categories")`) and exposes `getCategory(i)` / `getNumCategories()` /
  `isCategory(i)` (`CvImprovementInfo.cpp:497-509`). The same trio exists on Building,
  Bonus, Trait, UnitCombat, Build, Civic, Corporation, Feature, Project, Promotion,
  PromotionLine, Religion, Route.
- **`<ImprovementYieldChanges>` / `<GlobalImprovementYieldChanges>`** building tags
  already deliver per-improvement, city-scoped and player-wide yields
  (`IDValueMap<ImprovementTypes,YieldArray>` on `CvBuildingInfo`; accumulated into
  `CvCity::m_improvementYieldChanges`, applied in `CvCity::getYieldChangeAt`; player
  path via `CvPlayer::changeImprovementYieldChange`). Display lives in
  `CvGameTextMgr` building help; AI valuation in `CvCityAI::AI_buildingYieldValue`.

## The gap

1. No improvement-oriented categories are defined (only `CATEGORY_SPECIALIST_*` and
   `CATEGORY_UNITCOMBAT_*` exist today).
2. No improvement is tagged with any category.
3. There is **no effect** that reads improvement categories — nothing says "grant yield Y
   to every worked plot whose improvement is in category C." `isCategory` is currently
   used only for UI/promotion-line/specialist grouping, never for plot yields.

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

### 3. DLL: a category-keyed yield effect on buildings

Mirror the existing improvement path exactly, but keyed by category.

- `CvBuildingInfo`: add `IDValueMap<CategoryTypes, YieldArray> m_aImprovementCategoryYieldChanges`
  (+ global variant if wanted), getters, `cyGet`, `readPairedArrays(pXML,
  L"ImprovementCategoryYieldChanges", L"CategoryType", L"YieldChanges")`, copyNonDefaults,
  CheckSumC. (Note: building Categories use delayed resolution at `CvBuildingInfo.cpp:2958`;
  category *keys* in a paired array likewise need delayed resolution since categories are
  GameInfo.)
- Schema: add `ImprovementCategoryYieldChanges` element to `C2C_CIV4BuildingsSchema.xml`
  BuildingInfo list (near the existing `ImprovementYieldChanges`).
- **Application — two options:**
  - **(a) Expand at city accumulation (recommended).** In `CvCity::processBuilding`,
    for each `(category, yields)` on the building, walk all improvement infos once and,
    for any improvement with `isCategory(category)`, fold `yields` into the existing
    `m_improvementYieldChanges` map (reuse `changeImprovementYieldChanges`). Then
    `getYieldChangeAt`, display, and AI all keep working unchanged because everything
    still flows through the per-improvement map. Cost: one improvement-list scan per
    building process (cheap, off the hot path).
  - **(b) Resolve at yield time.** Keep a separate per-city category map and, in
    `getYieldChangeAt`, sum over the plot improvement's categories. More work on the hot
    path; only needed if we want category membership to change at runtime (we don't).

  Option (a) is preferred: it localizes all new logic to `processBuilding`, leaves the
  hot yield path and the display/AI code untouched, and makes category bonuses
  indistinguishable downstream from hand-listed improvement bonuses.

### 4. Display + AI

With option (a), no changes are strictly required — category bonuses materialize as
per-improvement entries. For nicer help text we can optionally add a building-help line
that names the category and counts matching worked plots (mirror the
`ImprovementYieldChanges` block in `CvGameTextMgr`). AI valuation already values the
folded per-improvement entries via `AI_buildingYieldValue`.

## Relationship to the upgrade auto-propagate option

The previously-chosen "DLL auto-propagate along `ImprovementUpgrade`" is an *implicit*
grouping by upgrade lineage. The Category system is the *explicit* superset:

- Categories handle non-linear groups (e.g. all mine-like tiles regardless of upgrade
  links), one tile in multiple groups, and the planned separate Pasture/Plantation
  lines — none of which a single linear chain-walk expresses.
- If we adopt categories, the upgrade auto-propagate becomes **unnecessary** for this
  feature (membership already spans the chain). Auto-propagate stays a valid lighter
  interim if we want upgrade-following before investing in category data.

Recommendation: adopt the Category-based design as the durable foundation; skip
auto-propagate unless we want a stopgap this cycle.

**Status: auto-propagate shipped (interim).** Implemented in `CvBuildingInfo::doPostLoadCaching`
(`CvBuildingInfo.cpp`): after all infos load, each `<ImprovementYieldChanges>` /
`<GlobalImprovementYieldChanges>` entry is folded onto every downstream improvement in its
`ImprovementUpgrade` (+ `AlternativeImprovementUpgradeTypes`) closure via the new
`IDValueMap::addArrayValue`. Because the building's map itself is expanded once at load,
the city accumulation, player accumulation, `CvCityAI::AI_buildingYieldValue`, and the
`CvGameTextMgr` help text all pick up the full chain with no per-site changes. The Category
system below remains the planned durable replacement; when adopted, this post-load expansion
can be removed in favour of category membership.

## Rollout

1. DLL: add `m_aImprovementCategoryYieldChanges` + schema + `processBuilding` expansion
   (option a). Build (Assert) to verify.
2. Data: define the improvement categories; tag the improvement upgrade chains.
3. Migrate the existing `Yield-parity` `<ImprovementYieldChanges>` blocks to the category
   form where a whole group is intended (Farm line → `CATEGORY_IMPROVEMENT_FARMLAND`,
   wood line → `CATEGORY_IMPROVEMENT_FOREST`, etc.); keep per-improvement blocks only
   where a single specific tile is intended.
4. XML validate; spot-check in-game.

## Open questions

- Granularity: one category per "tile family" vs finer (e.g. split early vs modern
  mines). Start coarse; nest later if needed.
- Should `GlobalImprovementCategoryYieldChanges` (player-wide) exist too, or city-scoped
  only for now?
- Convergence cleanup: once Pasture/Plantation/Winery get their own upgrade lines, retag
  so they no longer share Vertical Farm's category.
