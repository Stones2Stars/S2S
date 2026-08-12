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

- **The mechanic is GONE from the engine.** `CvGame::doGlobalWarming`, its turn-loop call, the declaration, the
  five `GLOBAL_WARMING_*` global defines and the feature-side `getWarmingDefense` input are all removed; the
  curator drops `iWarmingDefense`, so no JSON carries it. Recover the implementation from git if it is ever
  wanted again.
- ⚖ **The NUKE COUNTER is not part of it and stays** — `CvGame::getNukesExploded`/`changeNukesExploded` is raised
  by a real detonation and is owner-ruled worth keeping even with no consumer
  ([economy.md](../../reference/economy.md)).

## What still references global warming

- **XML curator INPUT only:** `<iWarmingDefense>` on 6 features (`Assets/XML/Terrain/CIV4FeatureInfos.xml`) + the
  schema entry. The curator DROPS the field, and the legacy XML is never read into the game
  ([DEC-no-xml-into-game](../../architecture/decisions.md#dec-no-xml-into-game)), so these are inert by design.
- **GameText** `TXT_KEY_MISC_GLOBAL_WARMING_NEAR_CITY` (`Global_CIV4GameText.xml`) — TXT is an unmigrated system
  boundary; an unreferenced key is inert.

**FOOTPRINT TO AUDIT (interconnected — finesse required, do NOT blind-delete):** the grep hits ~27 files, and
several are NOT pure dead code — they tie into still-live systems and need per-reference judgement:

- the **air-pollution property** (`Assets/Data/properties/property_air_pollution.json`, `CIV4PropertyInfos.xml`) —
  pollution is a live property; only its *global-warming consumer* is dead.
- **Events** (`CIV4EventInfos.xml` / `CIV4EventTriggerInfos.xml` / `Events_CIV4GameText.xml` /
  `CvRandomEventInterface.py`), **Buildings** (`SpecialBuildings_CIV4BuildingInfos.xml` + GameText), **Audio**
  (`AudioDefines.xml` / `Audio2DScripts.xml`), **Concepts/Tech/Bonus GameText**.
  Each needs "is this reference only-for-global-warming, or shared?" before removal.

## Recommendation

1. **Audit then prune** the interconnected footprint (events/property/buildings/audio) carefully — keep anything
   the live pollution system still uses.
2. **Re-implement later, properly**, if wanted: a tuned pollution→warming feedback wired into the first-class
   property system (#428/#429), never a bolted-on guard (feature `#ifdef`s are banned —
   [AGENTS.md](../../../AGENTS.md) Conventions §Design). Capture the design before it's lost (this note).
   - **Owner direction (2026-06-16): a re-implemented Global Warming gets its OWN base object/entity** for
     global-warming-specific data — e.g. `WarmingDefense` would live on that entity, NOT as a field on
     `CvFeatureInfo`. Accordingly, **#428 DROPS `iWarmingDefense` from the curated Feature JSON** (we don't need
     it in the new model); nothing is owed to Feature when the mechanic returns.

*(Despair-Index candidate: a mechanic compiled out for years that still renders a do-nothing stat in the
Civilopedia.)*
