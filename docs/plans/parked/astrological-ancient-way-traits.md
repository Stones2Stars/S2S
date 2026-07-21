# Parked — the Astrological-Influence & Ancient-Way trait/wonder system

> **Status:** CUT from the game data, kept for reimplementation. Owner ruling 2026-07-21: the system is not
> fleshed out past the Renaissance, so shipping a half-built version is worse than cutting it — remove it whole,
> document it here, reimplement properly later.

## What it was

A **building-granted civilization-trait** system: a "group wonder" you build confers a civ trait (`civilizationTrait: true`)
that grants some empire yield/GP-rate plus a **free promotion** to `UNITCOMBAT_LAW_ENFORCEMENT` units. Two themed groups,
each a `SPECIALBUILDING_*` cap group (one of the group may exist):

- **Astrological Influences** (12, the zodiac) — `BUILDING_ASTROLOGICAL_INFLUENCE_OF_<SIGN>` →
  `TRAIT_ASTROLOGICAL_INFLUENCE_OF_<SIGN>` → a `PROMOTION_INFLUENCE_OF_<PLANET>` (7 planet promotions, shared across signs).
  Classical era; gated on `TECH_ASTROLOGY` + `TECH_LAW_ENFORCEMENT` + an astrology building (school/observatory), Earth-map only.
- **Ancient Ways** (10, martial "ways") — `BUILDING_ANCIENT_WAY_OF_THE_<X>` → `TRAIT_ANCIENT_WAY_OF_THE_<X>` →
  `PROMOTION_WAY_OF_THE_<X>`. Prehistoric→ancient era.

The wonders also carried minor side effects (e.g. `warWeariness -25%`, `PROPERTY_EDUCATION +3`).

## Why it was cut

The trait/promotion ladders stop at the Renaissance — there is no coverage for the later ~two-thirds of the game, so the
mechanic is a dead end mid-game. Rather than ship it half-built (or spend balance effort completing a system that wants a
proper redesign), it is removed whole and recorded here.

## What was removed (the full inventory — the reimplementation checklist)

| Kind | Count | Ids |
|---|---|---|
| Traits | 22 | `TRAIT_ASTROLOGICAL_INFLUENCE_OF_*` (12) + `TRAIT_ANCIENT_WAY_OF_THE_*` (10) |
| Wonders (buildings) | 22 | `BUILDING_ASTROLOGICAL_INFLUENCE_OF_*` (12) + `BUILDING_ANCIENT_WAY_OF_THE_*` (10) |
| Promotions | 17 | `PROMOTION_INFLUENCE_OF_*` (7 planets) + `PROMOTION_WAY_OF_THE_*` (10) — exclusive to this system |
| SpecialBuilding groups | 2 | `SPECIALBUILDING_GROUP_ASTROLOGICAL_INFLUENCES`, `SPECIALBUILDING_GROUP_ANCIENT_WAYS` |

**Ripple cleaned on regen:** the prereq buildings that enabled the wonders (`BUILDING_ASTROLOGY_SCHOOL`,
`BUILDING_OBSERVATORY`), `BONUS_DOG` (a wonder's inverted prereq edge), and the techs whose `enables` listed the cut
entities (`TECH_ASTROLOGY`, `TECH_LAW_ENFORCEMENT`, the lifestyle techs, `TECH_DUMMY`, `TECH_GAME_START`, …).

## How the cut was applied (and how to revive it)

- **Cut at the STORE, not per-curator** — `Tools/Migration/store.py` `is_dropped_type()` (`DROPPED_TYPE_PREFIXES` /
  `DROPPED_TYPES`) skips these Types as the XML loads, so **no curator emits them AND the enable/obsolete inversion never
  produces a dangling FK**. A per-curator output drop would have left the granting techs pointing at deleted ids.
- **The source XML is LEFT IN PLACE** as the reimplementation reference (`GroupWonders_CIV4BuildingInfos.xml`,
  `CIV4TraitInfos.xml`, the promotion XML). Git history + this doc are the record.
- **Art defines are KEPT** (owner) — `ART_DEF_*` for the wonders/movies stay in the ART XML.
- **GameText** for the cut ids is removed (unreferenced once the entities are gone).

**To revive:** delete the entries from `store.is_dropped_type` (+ restore the GameText), then reimplement the trait/promotion
ladders across the FULL era range before re-enabling — the whole point of the park is that the missing coverage is the work.
