# Global Warming mod — scrapped, vestiges to remove (+ a concept worth revisiting)

**Status (2026-06-16):** the Global Warming mechanic is **compiled OUT** and effectively dead, but its vestiges
are scattered across ~27 files. This note captures (a) what the mechanic was, (b) why it's inert today, (c) the
vestige inventory for a clean removal, and (d) why the *concept* is worth a proper future implementation. Surfaced
while curating `FeatureInfo` for #428 (the `iWarmingDefense` field).

Tracking issue: **(file via `gh issue create`; link here once created).**

## The concept (worth keeping in mind)

Pollution accumulates globally; once it crosses a threshold each turn there's a chance a random land plot
**downgrades** (forest → plains → desert, ice melts, etc.). **Features resist** the downgrade via a per-feature
`WarmingDefense` value (forests especially — the "tree-hugger" defence bonus), and nukes spike it
("nuclear winter"). It's a genuinely cool systemic-feedback mechanic — runaway pollution visibly reshaping the
map. The problem was never the idea; it was that a good version needs careful tuning + a coherent tie-in to the
property/pollution system (it requires finesse — a quality this codebase has historically rationed).

## Why it's inert today

- The whole system is gated on `#define GLOBAL_WARMING`, and that define is **commented out**:
  `Sources/CvGameCoreDLL.h:232` → `// #define GLOBAL_WARMING`.
- So `CvGame::doGlobalWarming()` (`CvGame.cpp:6556-6819`), its turn-loop call (`CvGame.cpp:5959-5960`), and the
  declaration (`CvGame.h:513-514`) are **never compiled**. The mechanic does not run.
- But the data + a few always-compiled hooks remain, so a value that drives **nothing** is still authored and
  **shown to the player** (the classic "dead but on display" signature).

## Vestige inventory (for the removal issue)

**Dead C++ (inside `#ifdef GLOBAL_WARMING`, compiled out):** `CvGame::doGlobalWarming` + call site + decl
(`CvGame.cpp` 5959-5960 / 6556-6819, `CvGame.h` 513-514).

**Orphaned-but-LIVE traces (always compiled — these are the real cleanup risk):**

- `CvFeatureInfo` `m_iWarmingDefense` + `getWarmingDefense()` (`CvFeatureInfo.h/.cpp`) — read only by the dead pass.
- Python binding `getWarmingDefense` (`CyInfoInterface2.cpp:249`) — exposes the getter to a non-existent mechanic.
- Pedia display (`Assets/Python/Screens/Pedia/PediaFeature.py:156`) — renders the inert value to players.
- XML field `<iWarmingDefense>` on 6 features (`Assets/XML/Terrain/CIV4FeatureInfos.xml`) + the schema entry
  (`C2C_CIV4TerrainSchema.xml`). **(#428 already DROPS this field from the curated Feature JSON — category-i dead.)**
- `GlobalDefines.xml`: `GLOBAL_WARMING_UNHEALTH_WEIGHT` / `_BONUS_WEIGHT` / `_FOREST` / `_PROB` / `_NUKE_WEIGHT`.
- GameText `TXT_KEY_MISC_GLOBAL_WARMING_NEAR_CITY` (+ any other `*GLOBAL_WARMING*` keys).

**FOOTPRINT TO AUDIT (interconnected — finesse required, do NOT blind-delete):** the grep hits ~27 files, and
several are NOT pure dead code — they tie into still-live systems and need per-reference judgement:

- the **air-pollution property** (`Assets/Data/properties/property_air_pollution.json`, `CIV4PropertyInfos.xml`) —
  pollution is a live property; only its *global-warming consumer* is dead.
- **Events** (`CIV4EventInfos.xml` / `CIV4EventTriggerInfos.xml` / `Events_CIV4GameText.xml` /
  `CvRandomEventInterface.py`), **Buildings** (`SpecialBuildings_CIV4BuildingInfos.xml` + GameText), **Audio**
  (`AudioDefines.xml` / `Audio2DScripts.xml`), **Concepts/Tech/Bonus GameText**.
  Each needs "is this reference only-for-global-warming, or shared?" before removal.

## Recommendation

1. **Remove** the dead C++ + the orphaned getter/binding/pedia/define/schema/field vestiges (low risk).
2. **Audit then prune** the interconnected footprint (events/property/buildings/audio) carefully — keep anything
   the live pollution system still uses.
3. **Re-implement later, properly**, if wanted: a tuned pollution→warming feedback wired into the first-class
   property system (#428/#429), not a bolted-on `#ifdef`. Capture the design before it's lost (this note).
   - **Owner direction (2026-06-16): a re-implemented Global Warming gets its OWN base object/entity** for
     global-warming-specific data — e.g. `WarmingDefense` would live on that entity, NOT as a field on
     `CvFeatureInfo`. Accordingly, **#428 DROPS `iWarmingDefense` from the curated Feature JSON** (we don't need
     it in the new model); nothing is owed to Feature when the mechanic returns.

*(Despair-Index candidate: a mechanic compiled out for years that still renders a do-nothing stat in the
Civilopedia.)*
