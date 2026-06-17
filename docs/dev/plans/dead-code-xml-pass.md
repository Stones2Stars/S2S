# Dead Code & Dead XML Removal Pass — Plan

Status: **candidate generation started (2026-06-11)** — Tier-1 items verified, a
dead-GlobalDefines sweep done, and a batch of dead members found by the #196
migration campaign filed as issues (#352–#359). No removals yet. Companion to the
standing bug-hunt (`docs/dev/plans/codebase-bug-hunt.md`). Same discipline:
tooling/grep *generates candidates*, but every removal is **personally verified
against source before deletion**, in small reviewable PRs, Assert-build before
each C++ PR.

## Why now

The mod is C2C-derived and has accumulated zombie code: disabled `#if 0` blocks,
leftovers from already-removed subsystems (ACO histogram, BBAI logging), orphan
data, and dead-branch logic. Removing it shrinks the surface area before any
larger rework.

**North-star (deferred, not this pass):** move data storage from **XML → JSON** to
simplify a future **web-based Civilopedia** port. The XML structure is also
acknowledged to be haphazard in places and would benefit from a structural rework
— but neither the rework nor the migration is happening yet. What *is* worth doing
now is the dead-data pass, because:

1. You don't want to port garbage — migrate a clean, minimal dataset.
2. The detection tooling below (enumerate every tag the DLL actually reads, every
   `<Type>`, every `TXT_KEY_*`, every define) **is** the data-model extractor a
   JSON migration needs later. Build it reusably with that second use in mind.

## Method

- One subsystem/category per PR. `Fixes`/`Refs` the relevant issue when one exists.
- C++: Assert-build before PR (`Tools/_Build.ps1 Assert build`).
- XML/Python: `Tools/XmlValidator.exe -a` and
  `Tools/XMLTools/verify-python-callbacks.py` after changes.
- Tooling **emits candidate lists (CSV), never auto-deletes.** Human-verify +
  test-load a save before removing data.

## Tier 1 — high-confidence, immediate (low risk)

Quick wins. **Verified 2026-06-11** (incl. the Boost.Python `.def()` layer):

- **Disabled blocks** — **29** `//#if 0` blocks inventoried, clustered in
  `CvGameCoreDLL.cpp` (8, profiler instrumentation) and `CvGameCoreUtils.cpp`
  (8, pathfinding short-circuits/profiling); rest scattered (CvPlayerAI ×2,
  CvUnitAI ×4, CvPlot, CvPlotGroup, CvViewport, FInputDevice.h, FProfiler.h,
  CvGameCoreDLLDefNew.h). Plus **14** commented-out `logBBAI()` calls in
  `CvUnitAI.cpp` (~13019–13449, 17563, 17576) left from the deleted BBAI logging.
- ~~Orphan Python file `Assets/Python/MapScriptToolsOld.py`~~ — **NOT DEAD,
  do not delete**: live `import MapScriptToolsOld as mst` in
  `PrivateMaps/C2C_Inland_Sea.py:36` and `PrivateMaps/C2C_Tectonics.py:92`.
  (Could be folded into the current MapScriptTools someday — separate work.)
- **Marked-unused functions — CONFIRMED DEAD** (no callers, no Python `.def()`):
  `CvCity::calculateTotalTradeYield` (`CvCity.h:880`, `CvCity.cpp:11708`) and
  `CvPlayer::calculateTotalTradeYield` (`CvPlayer.h:1691`, `CvPlayer.cpp:24474`).
- **billw-disabled stubs — CONFIRMED DEAD** (both fully commented out):
  `CvPlayer.cpp:3812-3815` (`AI_doCentralizedProduction` call) and
  `CvPlayerAI.cpp:1547-1632` (wonder-city block; the containing function
  early-returns before it).
- **Dead info-class members (batch, found by the #196 migration)** — see issue
  **#358**: `CvBonusInfo::m_piImprovementChange`, `CvHandicapInfo::m_szHandicapName`,
  `CvCivilizationInfo::m_iNumLeaders`, `CvEventInfo::m_iUnitPromotion`,
  `CvCivicInfo::m_szGlobalDefine`, `CvUnitInfo::m_aVisibilityIntensityRangeTypes`,
  `CvTraitInfo::m_piBonusHappinessChangesFiltered`, `CvAttachableInfo::m_fUpdateRate`,
  `CvPromotionInfo::m_iNumPromotionOverwrites`.
- **Dead class** — `CvDiplomacyTextInfo` (never instantiated, no loader/getter
  can reach it) — see issue **#359**.

## Tier 2 — audit required (medium risk)

- **ACO BUG-options surface — REMOVED (2026-06-09).** The old Advanced Combat Odds
  options screen was fully dead once the preview became a pure `computeCombatPreview`
  renderer (only `ACO__SwapViews` was still read, and it merely inverted the Shift-gated
  "needed rounds" line). Removed: `BugACOOptionsTab.py` (+ its `Python.pyproj` entry),
  `Assets/Config/Advanced Combat Odds.xml`, the two `init.xml` registration lines, ~21
  `TXT_KEY_BUG_OPT_ACO__*`/`TXT_KEY_BUG_OPTTAB_ACO` GameText keys, the whole `ACO_*`
  GlobalDefine block in `GlobalDefinesAlt.xml`, the `CONCEPT_ADVANCED_COMBAT_ODDS`
  Civilopedia concept (NewConceptInfo + 2 GameText keys), and a stray ACO sentence
  mis-glued onto `TXT_KEY_CONCEPT_ACTIVE_DEFENSE_PEDIA`. The `SwapViews` C++ read was
  dropped (`iView` now comes straight from `shiftKey()`).
  **KEEP (live, not ACO cruft):** the `Art/ACO/{green,yellow,red}_bar_*.dds` bars (the
  `computeCombatPreview` renderer draws them, `CvGameTextMgr.cpp` ~3158) and the
  `TXT_ACO_*` renderer text keys (e.g. `TXT_ACO_VICTORY`). These are the new lean
  preview's assets — the `ACO` here is just retained naming.
- **Dead-branch zombies (handle via the bug backlog, not bulk deletion):**
  - `#105` `m_bGameStart` / `checkGameStart()` — the flag is never set true so the
    branch is dead. Already `needs-signoff` (semantics decision: needs `>` not `>=`
    plus init-true). Resolve there.
  - `#139` (**Python**, `Revolution.py spawnRevolutionaries` — `isNPC() and not
    isNPC()` always-false guard) and `#64` (`CvCity::removeWorstCitizenActualEffects`
    inverted-while) — re-verify; these are logic-dead, fix-or-remove via the backlog.
- **Removed-game-option code paths** — code gated on options that no longer exist.
- **Loader/merge bugs found by the #196 migration (route via the bug backlog):**
  #352 (CvProjectInfo readPass3 wrong-category bounds check), #353
  (CvPromotionLineInfo pair-vector merge pushes the loop index as the type id),
  #354 (CvBuildingInfo dtor frees BonusYieldModifier with the Specialist count +
  read path never derives bDamageAttackerCapable), #355 (CvEventTriggerInfo
  modular merge sorts TriggerTexts and their eras independently), #356
  (CvWorldPickerInfo WaterLevelGloss merge pushes into the Decals vector), #357
  (CvDiplomacyInfo FindResponseIndex bOnlyText poisoning + pointer-assign typo).

## Tier 3 — data orphans (needs tooling)

Build small Python audit tools (extend the `verify-python-callbacks.py` pattern).
Each emits a candidate CSV; nothing is auto-removed.

1. **Dead schema tags** — enumerate every tag the DLL reads (grep
   `GetChildXmlValByName` / `SetVariableListTagPair` string-literal tag names in
   `Sources/`), diff against tags present in the schema/XML. **Caveat:** generic
   loaders and `BoolExpr`/`IntExpr` expression reads are not literal-greppable —
   maintain an allowlist of tags consumed generically.
2. **Orphan `<Type>` entries** — collect every `<Type>` per category, cross-ref
   against all XML references + Python literals. **Caveats:** savegame
   compatibility (a removed Type can crash old saves — test-load); modular
   `z*`-prefixed XML appended to base lists; Type strings built by concatenation in
   Python.
3. **Dead `TXT_KEY_*`** — defined (`GameText/*`) vs referenced (XML
   `<Description>/<Help>`, Python `getText`/`TRNSLTR`). **Caveat:** dynamic key
   construction (`"TXT_KEY_" + type`) — not statically visible.
4. **Dead GlobalDefines** — `<DefineName>` in `GlobalDefines*.xml` vs `getDefine*`
   uses (C++ + Python). **Caveat:** `FVariableSystem` string lookups aren't
   statically visible.

### Tier 3.4 sweep results (2026-06-11): dead-GlobalDefines candidates

515 defines scanned (414 `GlobalDefines.xml`, 102 `GlobalDefinesAlt.xml` + others);
all `getDefine*` call sites use literal strings (no dynamic construction found), so
the static analysis is complete modulo the EXE: **defines consumed by the closed
Firaxis engine (graphics/camera/audio knobs) may look dead from the DLL side but be
live** — anything in the CAMERA/RENDER/AIR/FLAG/FOW families below must be assumed
EXE-read unless proven otherwise; test in-game before deleting. The DLL-side-dead
candidates (zero hits in Sources/, Assets/Python/, non-GlobalDefines XML):

- *Engine-knob families (EXE-risk, verify in-game)*: AIR_DEFEND_DISTANCE,
  AIR_DEFEND_FINISH, AIR_EXECUTE_DISTANCE, AIR_EXECUTE_FINISH, AIR_IDLE_HEIGHT,
  AIR_PATROL_HEIGHT, AIR_PATROL_RADIUS, AIR_PATROL_SPEED, CAMERA_BATTLE_ZOOM_IN_DISTANCE,
  CAMERA_CITY_NO_PITCH, CAMERA_CITY_TURN, CAMERA_CITY_ZOOM_IN_DISTANCE,
  CAMERA_FORCE_TO_SMALLEST_MAX_DISTANCE, CAMERA_MAX_SCROLL_SPEED, CAMERA_MIN_SCROLL_SPEED,
  CAMERA_NEAR_FAR_PLANE_RATIO, CAMERA_SHRINE_ZOOM_IN_DISTANCE, CAMERA_SMALLEST_MAX_DISTANCE,
  EFFECT_DEFAULT_SIZE, FLAG_OFFSET_X/Y, FLAG_OFFSET2_X/Y, FOW_MINIMAP_WAS_VISIBLE_COLOR,
  FOW_WAS_VISIBLE_COLOR, IMPROVEMENT_SCALE, BONUS_SCALE, RENDER_AREABORDER_UNDER_FEATURES,
  RENDER_GLOBEVIEW_CLOUDS, RENDER_WATER, RIVER_Z_BIAS, ROUTE_Z_BIAS,
  SINGLE_UNIT_GFX_EXTRA_SCALE, UNIT_ANIM_PAGE_MAX, DEFAULT_ANIM_PAGE_MAX,
  UNIT_TRAIL_RESOLUTION, MINIMAP_RENDER_SIZE (had 2 ambiguous hits — recheck).
- *UI / misc (likely DLL-or-Python history, safer)*: LEADERHEAD_RANDOM,
  TURN_LOG_MAX_HEIGHT, TURN_LOG_MAX_WIDTH, TURN_LOG_MIN_HEIGHT, QUICKSAVE,
  EVENT_MESSAGE_STAGGER_TIME, FORCE_UNOWNED_CITY_TIMER, CIV4_VERSION, MIN_VERSION,
  SAVE_VERSION, MAX_PAGING_FRAME_TIME_MS (has an explicit "currently unused" comment).
- *GlobalDefinesAlt (BUG options)*: BUG_CDA_ZOOM_CITY_DETAILS,
  BUG_CITYBAR_CULTURE_TURNS, BUG_CITY_SCREEN_BASE_COMMERCE_HOVER.

## Hard caveats (a detection script MUST account for these)

- **Boost.Python bindings:** a C++ function with no C++ caller may be live via a
  `Cy*Interface.cpp` `.def()` + Python. Check both layers before removing.
- **Generic tag loaders** (`SetVariableListTagPair`) and **expression reads**
  (`BoolExpr`/`IntExpr`) consume tags not visible as literals.
- **Modular `z*` XML** is appended to base info lists.
- **Savegame compatibility:** test-load existing saves after any data removal.
- **Dynamic strings** in Python (Type and TXT_KEY concatenation).

## Suggested order

Tier 1 (fast, low-risk, builds momentum) → Tier 3 tooling (generate candidate
lists, reusable for the eventual JSON extraction) → Tier 2 audits. Keep each PR
small and per-category.
