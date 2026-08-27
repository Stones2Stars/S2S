# Pedia read-map census (stage-4 input)

> Evidence base for the stage-4 Python library ([patterns.md § THE PYTHON READ BOUNDARY](../architecture/patterns.md)) — the evidence base for the stage-4
> Python surface, pedia slice. Per [the Cy* surface is not a fixed contract](../architecture/patterns/06-the-python-read-boundary-one.md#-the-python-read-boundary--one-complete-data-fetching-library) this maps
> **NEEDS, not getters to port**: every binding named below is obsolete by ruling; the counts say what a pedia
> page must be SERVED, not what calls survive. Counts are grep-derived static call sites at the censused tree.

## 1. The wired surface

**One active pedia** — the Toffer90/C2C pedia: hub `Assets/Python/Screens/Pedia/Pedia.py` (1,790 lines) +
19 per-type pages in the same folder + `Index_Pedia.py` (A–Z index) + the two surviving
`Assets/Python/Screens/Sevopedia/` pages (`SevoPediaRoute.py`, `SevoPediaSpecialist.py`), both imported by the
hub — the rest of the old Sevopedia set no longer exists. **6,588 lines total** across the two folders.

Wiring (all verified live, no dead screens):
- `Assets/Python/EntryPoints/CvScreensInterface.py`: `screenMap[PEDIA] = Pedia.Pedia(PEDIA)` + the entry points
  `pediaShow`/`pediaMain`/`pediaBack`/`pediaForward`/`linkToPedia`/`pediaShowHistorical` and the engine-invoked
  jumps `pediaJumpToBuilding`/`Unit`/`Tech`/`Promotion`/`Bonus`/`Terrain`.
- Every page-body widget jump rides `WidgetTypes.WIDGET_PEDIA_JUMP_TO_*` (17 widget families used by the index
  alone) — hover help on those widgets is served DLL-side (CvDLLWidgetData → the same CvGameTextMgr composers).
- Helpers pulled in by the hub: `Assets/Python/Contrib/UnitUpgradesGraph.py` (619 lines — the building/unit/
  promotion tree pages), `PythonToolTip` (the hub renders its OWN list-item tooltips through the text feeders,
  player-context mode — `Pedia.py` handleInput). The old `getGOMReqs` BoolExpr walker is gone (§3) — requires
  panels now read `INFO.getRequiresIdsInClause` directly.
- Hub page anatomy: category list → sub-category list → item list (whole-DB scan + filter per category) →
  entity page rendered by the per-type screen class.

## 2. The C++ text feeders (referenced, not re-censused)

Every entity page's **effect-lines body is exactly ONE `CyGameTextMgr` call** with `bCivilopediaText=True` —
the same composer families already censused in [patterns.md](../architecture/patterns.md) (the per-entry renderer) (~15,000 lines /
~1,450 hand-assembled `getText` blocks across 18 info-help composer families; heaviest: `setBuildingHelp`
2805/269, `setBasicUnitHelp*` 2134/224, `parsePromotionHelpInternal` 2071/221, `parseCivicInfo` 1555/158,
`parseTraits` 1493/179). Feeders the pedia calls (`Sources/Python/CyGameTextMgrInterface.cpp`):

| Feeder | Page(s) | Also called by the hub in tooltip mode |
|---|---|---|
| `getBuildingHelp` | PediaBuilding | yes |
| `getUnitHelp` | PediaUnit | yes |
| `getTechHelp` | PediaTech | yes |
| `getPromotionHelp` | PediaPromotion | yes |
| `getBonusHelp` | hub tooltip ONLY — the bonus PAGE body is fully hand-assembled (98 sites, the exception) | yes |
| `parseCivicInfo` | PediaCivic | yes |
| `parseTraits` | PediaTrait | yes |
| `parseReligionInfo` | PediaReligion | yes |
| `parseCorporationInfo` | PediaCorporation | yes |
| `parseLeaderTraits` | PediaLeader | yes |
| `getImprovementHelp` / `getFeatureHelp` / `getTerrainHelp` / `getRouteHelp` / `getProjectHelp` / `getHeritageHelp` / `getUnitCombatHelp` / `getSpecialistHelp` | one page each | yes (most) |
| `buildHintsList` | hub (hints category) | — |

**Two render modes of the SAME composers**: full-page body (`bCivilopediaText=True`, no player context) and
hover tooltip (`False`, active-player context). The new surface must serve both from one source.

## 3. Per-screen read inventory

Call sites = every non-UI `get*/is/has/parse*` call in the file (includes `TRNSLTR.getText` localization
pulls and `getButton` art pulls). DB scans = `xrange(GC.getNum<T>Infos())` loops in the file.

| Screen (lines) | Sites | DB scans | Heaviest need-classes |
|---|---|---|---|
| **Pedia.py hub** (1790) | 369 | 7 loops + per-category list generators over 20+ types | category/sort metadata (~60 sites: `getEra` 20, `getBonusClassType` 10, wonder/special/AI-type/cost/instance tests); identity/text (`getText` 67, `getDescription` 22, `getType` 11); 18 feeder tooltip calls; art/symbols |
| **PediaBuilding** (451) | 57 | 3 (Civic, Terrain, Feature — own-requires inversions, §finding 2) | requires (and/or techs, religion, corp, bonuses, in-city/or-buildings, civics/terrain/feature tests, `INFO.getRequiresIdsInClause` reads per edge bucket — the old GOM walk is gone); stats (cost, flat/percent yields+commerces, happiness/health, GP, goldenAge); cross-links (replaced/replacement building lists); art (`getButton` 17) |
| **PediaUnit** (397) | 46 | 3 (UnitCombat, Civic, Promotion) | requires (and/or techs/bonuses/buildings, civics test-loop, religion, `INFO.getRequiresIdsInClause` reads — the old GOM walk is gone); cross-links (upgrades fwd list, qualified-promotion scan, subcombat scan, builds lists); stats (moves, cost, strength ×100, air range, workRate, conscription, capture); art (`getButton` 9) |
| **PediaBonus** (379) | 98 | 3 (Building, Unit, Improvement) | cross-links = the whole page (who-needs-me: buildings/units via `EDGEF_REQUIRED_BY`, related buildings/units via `EDGEF_RELATED`, improvements by bonus-yield/trade); own requires (techReveal/cityTrade/obsolete, latitudes); stats (happiness/health, yield tables) |
| **PediaImprovement** (404) | 77 | 7 (Build, Tech, Civic, Bonus, Route, Terrain, Feature) | own keyed-container enumerations (yield changes by tech/civic/route/bonus, makesValid by terrain/feature, builds-that-create-me); validity flags; defense |
| **PediaTech** (333) | 73 | 3 (Building, Project, Unit) | cross-links (who-unlocks-me: buildings/projects/units via `EDGEF_ENABLES`; `leadsTo` fwd list is the same edge family read forward); requires (and/or techs, prereq buildings w/ minima); stats (research cost — player-context OR info, happiness/health/tradeRoutes/workerSpeed); quote |
| **SevoPediaRoute** (226) | 54 | 0 (iterates routes for compare table) | route-vs-route stats table, yield changes, prereq bonus/tech, builds |
| **PediaBuild** (270) | 51 | 2 (Unit, Feature) | stats × game-state (cost/time × gamespeed/era percents); links (improvement/route made, units-with-build scan, feature-chop table) |
| **PediaPromotion** (197) | 41 | 1 (Promotion) | requires (prereq + or1/or2, tech, state religion); cross-links (children scan, qualified/disqualified unitcombat lists) |
| **PediaEra** (162) | 36 | 0 | game-config stats (starting units/gold × handicap × gamespeed of the RUNNING game — computed state, §finding 5) |
| **PediaLeader** (123) | 35 | 1 (Civilization) | AI personality config dump (`getMaxWarRand` etc., 11 leaderhead numbers); favorites; civ cross-link scan |
| **PediaFeature** (164) | 29 | 1 (Building) | growth/disappear odds × gamespeed; yields; building cross-link scan |
| **PediaCorporation** (170) | 29 | 2 (Building, Unit) | HQ/founds/spread cross-link scans; prereq bonus list |
| **PediaUnitCombat** (130) | 23 | 1 (Unit) | member-units scan + per-unit stat columns |
| **PediaProject** (111) | 22 | 0 | stats + tech prereq |
| **PediaReligion** (159) | 22 | 2 (Building, Unit) | cross-link scans by prereqReligion; active player's state religion |
| **PediaCivic** (92) | 19 | 0 | upkeep/civicOption intrinsics, tech prereq |
| **PediaHeritage** (162) | 18 | 1 (Building) | building cross-link scan via `EDGEF_REQUIRED_BY` |
| **PediaCivilization** (99) | 15 | 1 (LeaderHead) | leaders scan, city-name list |
| **PediaTerrain** (105) | 13 | 1 (Building) | yields; building cross-link scan |
| **PediaTrait** (97) | 11 | 1 (LeaderHead) | `parseTraits` body + who-has-me scan |
| **SevoPediaSpecialist** (87) | 9 | 0 | feeder body + civilopedia text |
| **Index_Pedia** (198) | 2 distinct | 19 type-scans | identity + art ONLY (`getDescription` + `getButton` per entity, jump-widget payloads) |
| **UnitUpgradesGraph** (619) | ~90 | 3 (Building, Unit, Promotion) | pure edges + identity/art: replacement chains, unit upgrades, promotion prereq trees → graph nodes/edges |

**Totals: ~1,149 static call sites** across the 23 screen files (+ ~90 in UnitUpgradesGraph);
**~42 whole-DB scan loops**; heaviest five screens: hub 369 · Bonus 98 · Improvement 77 · Tech 73 ·
Building 57.

## 4. NEED → NEW-SURFACE mapping

| Need-class | Census weight | Answered by |
|---|---|---|
| Identity/text (name, type key, civilopedia/strategy text, quote) | every page; ~200 sites | identity intrinsics — one identity block per entity |
| Effect lines (the modifier body) | 1 feeder call/page, backed by the ~1,450-block composer census | **the per-entry renderer** (`CvEntryText`, patterns.md category 5) — rendered entry lines replace the composer body. Pedia wants MORE than a flat tooltip blob: pages hand-place SOME groups separately (yields/commerce tables, happiness/health, GP) → entries must arrive **tagged by family/kind so the page can group and lay out by family**, and must render in BOTH modes (full-page, player-context tooltip) |
| Requires/prereqs display | ~70 sites | **the `CvRequires` section object** rendered as a structured tree — replaces the old Python-side `getGOMReqs` BoolExpr recursion AND the keyed-container inversion loops (`isPrereqAndCivics(i)` over all civics etc.) |
| Cross-links (who-unlocks-me / who-needs-me / who-replaces-me) | ~42 DB scans + fwd lists | **`EDGEF_RELATED` / `EDGEF_REQUIRED_BY` edge families** ([reverse lookups are populated once, at load](../cascade/01-deposit-and-read.md#1-one-step-deposit-down-accumulate-read-o1)) — the reverse pass already generalizes over requires trees, deposits, grants, provides, triggers, so the former GOM-walking scans (tech page walking every building's construct condition) are now exactly the `EDGEF_REQUIRED_BY`/`EDGEF_ENABLES` reads. Forward chains (unit upgrades, tech leadsTo, building replacements, qualified unitcombats) are forward edge families |
| Art/buttons | ~150 sites (`getButton`, chars/symbols, movies) | identity/art intrinsics; font-symbol chars stay engine reads |
| Stats/costs | ~120 sites | point reads over compiled sums + intrinsics (cost, moves, base strength, upkeep, latitudes) — ×100 convention, format at the render boundary |
| Category/sort metadata (the hub's grouping system) | ~60 sites | **NO clean answer yet** — see finding 4 |
| Computed game state (player/team/gamespeed/handicap/options) | ~15 sites | NOT info data — stays a game/context read beside the info payload; see finding 5 |

## 5. Shape recommendation (needs, not binding design)

A pedia page is served by a HANDFUL of coherent reads, replacing the hundreds of scalar calls:

1. **One entity-page payload** per (type, id): identity block (name, type key, pedia/strategy text,
   button/movie ref) + rendered entry lines **grouped/tagged by family** + the requires section as a
   structured display tree + the edge lists (RELATED both directions, REQUIRED_BY, the forward chains) +
   the intrinsic stats block. This single read covers §3's identity, effect, requires, cross-link, art, and
   stats columns for every per-type page.
2. **One per-type index payload**: [(id, name, button, category tags)] — answers the hub's item lists, the
   A–Z index (19 type-scans reading exactly this pair today), and the category filters in one read, killing
   the list-generation scans.
3. **Edge-list reads for the three graph pages** (building/unit/promotion trees) — nodes (id, name, button) +
   typed edges; UnitUpgradesGraph consumes edges only.
4. **The tooltip form** of the same rendered entry lines (player-context filtered) — the hub's list-item
   tooltips and the DLL widget-help path converge on the same renderer output.

## Findings (batched)

1. **The effect body is already centralized** — every entity page delegates its modifier prose to ONE
   composer call; the migration cost of that half is the ruling-29 composer rewiring, not the pedia. What the
   pedia adds on top is the hand-placed stats/requires/cross-link blocks censused here.
2. **Two distinct motives hide inside the ~42 whole-DB scans** — genuine reverse cross-links (bonus page
   scanning all buildings) AND **keyed-container inversions of the entity's OWN data** forced by the binding
   shape (building page looping every civic asking `isPrereqAndCivics(i)`; improvement page looping every
   tech/civic/route for its own yield-change tables). The first class dies to edge families; the second dies
   to sections/typed containers being served whole.
3. **CLOSED — requires display now reads the structured `CvRequires` object, not a condition-tree walk.** The
   old `HelperFunctions.getGOMReqs` walker (recursing `CyBoolExpr` trees from `getConstructCondition`/
   `getTrainCondition`) is deleted; pages read `INFO.getRequiresIdsInClause` per edge bucket instead (the
   REQUIRED_BY inversion still answers the cross-link direction) — confirming no boolean-expression API
   belongs on the new surface.
4. **CLOSED — the category home is `identity.pediaCategory`** ([json.md §7](../specs/json.md), owner). The
   taxonomy becomes AUTHORED DATA the curator derives once, so no consumer re-derives it; the era sub-category
   stays derived from the entity's own era. ⛔ The banned repair is publishing the legacy getters so the Python
   classifier resolves — it reads as migrated while preserving the substring-match-on-display-name buckets.
   The original finding, kept because it is the evidence for what the field replaces: the hub derives groupings from
   heterogeneous heuristics: `getEra`+1, `getBonusClassType`, `getSpecialBuildingType`, `getMapCategories`
   (space test), `getDefaultUnitAIType` (animal/missionary buckets), `getProductionCost() <= 0` (misc),
   `getMaxGlobalInstances() == 1` (world unit/wonder), promotion-line `isBuildUp`/`isStatus`, `isDisable`,
   `isGraphicalOnly`, grid X/Y (chronology sort). The pedia's taxonomy should be a classification/identity
   read (the `identity.categories` / `CLS_HAS` plane), not recomputed Python heuristics — needs a stage-4
   decision on where category tags live.
5. **Computed-game-state reads to keep OUT of the info payload**: player-context research cost
   (`CyTeam.getResearchCost`), the hub's `getCurrentResearch` jump, active player's state religion
   (religion page), `getTotalModifiedCombatStrength100(isOption(SIZE_MATTERS))` (unit page — option-gated
   computed stat, json.md §9: gates live at the consuming system), era/build/feature pages scaling by the
   running game's gamespeed/handicap percents. These are context/game reads beside the info payload.
6. **The leader page dumps AI personality config** (11 `getMaxWarRand`-family leaderhead numbers) — wave-D
   config-type reads, served as handicap/leaderhead intrinsics.
7. **Concept/hints categories read text-only types** (`CvConceptInfo`/`CvNewConceptInfo` `getCivilopedia`,
   `buildHintsList`) — pure TXT payloads, no modifier surface involved.
8. **No dead screens found**: all 21 Pedia/ classes are registered in the hub's `mapScreenFunctions`/
   `mapListGenerators`; Sevopedia is already reduced to the 2 wired files; Index_Pedia and
   UnitUpgradesGraph are both reached from the hub.
