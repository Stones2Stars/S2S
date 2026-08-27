# Identifier naming — the `INFOTYPE_NAME` convention

Every game entity has a string **id** of the form **`<INFOTYPE>_<NAME>`**: the leading segment is the
**infotype** (what kind of thing it is), the rest is the specific name. `UNIT_AXEMAN` is a unit named Axeman,
`BUILDING_FORGE` a building named Forge, `TRAIT_COMPLEX_SEAFARING` a complex trait. **This structure is fixed —
every id starts with its infotype generalization.** The prefix is how the engine and `readJson` route a
reference to the right registry, so it is never inferred from context (the overall id structure will not change).

The same ids serve two roles: the `type` field of an entity's own file, and the value used to **reference** that
entity from anywhere — a `requires` atom, an `enables` list, a modifier target. The atom/condition/target
vocabulary that consumes them is the [json spec](json.md) §3.

---

## Infotype prefixes — and where each lives

The **where** column doubles as the porting map: `✅ Assets/Data/<folder>/` = ported to JSON; `☐ XML only` =
not yet ported (still authored in `Assets/XML`, referenced from JSON by id). Verified against the live
`Assets/Data` folders and `type` ids.

| prefix | identifies | where to look |
|---|---|---|
| `BONUS_` | a resource (bonus) | ✅ `bonuses/` |
| `BONUSCLASS_` | a resource category (bonus class) | ✅ `bonusclasses/` |
| `BUILD_` | a worker build action | ✅ `builds/` |
| `BUILDING_` | a building (incl. `BUILDING_EFFECT_*` — the **property pseudo-buildings**: `BUILDING_` infotype, live in `buildings/`) | ✅ `buildings/` |
| `C2C_ERA_` | an era | ✅ `eras/` |
| `CIVIC_` | a civic | ✅ `civics/` |
| `CIVICOPTION_` | a civic category / slot | ✅ `civicoptions/` |
| `CIVILIZATION_` | a civilization | ✅ `civilizations/` |
| `CORPORATION_` | a corporation | ✅ `corporations/` |
| `CULTURELEVEL_` | a culture level | ✅ `culturelevels/` |
| `FEATURE_` | a terrain feature | ✅ `features/` |
| `FOLDTARGET_` | a generalized plot predicate's fold set — what `IS_WATER` and its kin MEAN, as the concrete substrate entities a deposit can land on ([json.md §3.5](json.md): we never fold onto a boolean) | ✅ `foldtargets/` |
| `GAMESPEED_` | a game speed | ✅ `gamespeeds/` |
| `HANDICAP_` | a handicap (difficulty) | ✅ `handicaps/` |
| `HERITAGE_` | a heritage | ✅ `heritages/` |
| `HURRY_` | a production-rush (hurry) type | ✅ `hurries/` |
| `IMPROVEMENT_` | a tile improvement | ✅ `improvements/` |
| `LEADER_` | a leader (leaderhead) | ✅ `leaderheads/` |
| `OUTCOME_` | a mission / combat-kill outcome (the `CvOutcome` gate + identity + replace-tier tag — [mission-outcome-system.md](../reference/mission-outcome-system.md)) | ✅ `outcomes/` |
| `PROCESS_` | a process | ✅ `processes/` |
| `PROJECT_` | a project | ✅ `projects/` |
| `PROMOTION_` | a unit promotion | ✅ `promotions/` |
| `PROMOTIONLINE_` | a promotion line (ordered chain) | ✅ `promotionlines/` |
| `PROPERTY_` | a city property (`PROPERTY_CRIME`, …) | ✅ `properties/` |
| `RELIGION_` | a religion | ✅ `religions/` |
| `ROUTE_` | a route | ✅ `routes/` |
| `SPECIALBUILDING_` | a special-building group (shared cap) | ✅ `specialbuildings/` |
| `SPECIALIST_` | a specialist | ✅ `specialists/` |
| `SPECIALUNIT_` | a special-unit group | ✅ `specialunits/` |
| `TECH_` | a technology | ✅ `techs/` |
| `TERRAIN_` | a terrain | ✅ `terrains/` |
| **`TRAIT_`** | a **simple**-set trait. ⛔ The prefix STATES THE SET, and the folder and the prefix agree by construction ([modifier.md §4](../cascade.md)) — every record in `traits/complex/` carries `TRAIT_COMPLEX_`, including a complex-ONLY line with no simple counterpart. Otherwise a held id cannot say which set it came from, and a simple-set leak is indistinguishable from a legitimate plain id | ✅ `traits/simple/` |
| **`TRAIT_COMPLEX_`** | the complex (Thunderbrd) VARIANT of a simple trait | ✅ `traits/complex/` |
| `UNIT_` | a unit | ✅ `units/` |
| `UNITCOMBAT_` | a unit-combat class | ✅ `unitcombats/` |
| `VICTORY_` | a victory condition | ✅ `victories/` |
| `VOTE_` | a diplomatic proposal (vote) | ✅ `votes/` |
| `WORLDSIZE_` | a world size (map dimensions + per-size config) | ✅ `worlds/` |
| `SKILL_` | a unit skill (`SKILL_BLITZ`) | ⚙ runtime-GENERATED — minted at load from the union of authored `skills` block keys (json.md §8, `ClassificationRegistry`); no data folder |
| `TAG_` | a unit tag (`TAG_MILITARY`) | ⚙ runtime-GENERATED (from `tags` block keys) |
| `ATTRIBUTE_` | a building attribute (`ATTRIBUTE_TEAM_SHARE`) | ⚙ runtime-GENERATED (from `attributes` block keys) |
| `AMENITY_` | a city-held, grantor-provided amenity (`AMENITY_PROVIDES_POWER`) | ⚙ runtime-GENERATED (from `amenities` block keys; grantors: building/civic/trait/tech) |
| `CHARACTERISTIC_` | a plot-substrate characteristic (`CHARACTERISTIC_ACTS_AS_CITY`) | ⚙ runtime-GENERATED (from `characteristics` block keys; carriers: terrain/feature/improvement/route) |
| `CAPABILITY_` | an empire capability (`CAPABILITY_SET_SCIENCE_RATE`) | ⚙ runtime-GENERATED (from `capabilities` block keys) |
| `POLICY_` | an empire policy (`POLICY_NO_FOREIGN_TRADE`) | ⚙ runtime-GENERATED (from `policies` block keys) |
| `EFFECT_` | a map graphics effect (`EFFECT_BIRDSCATTER`) | ☐ XML only — referenced from a feature's `world.art.effect`. *(NOT `BUILDING_EFFECT_*`, the property pseudo-buildings — those are the `BUILDING_` infotype.)* |
| `MAPCATEGORY_` | a map category (`MAPCATEGORY_EARTH`) | ☐ XML only — referenced from a building's `requires.build` |
| `EVENT_` | an event (and its trigger) | ☐ XML only — PERMANENT carve-out (#425 event rework — events stay Python, out of #430) |
| `ART_` | an art define (`ART_DEF_*`, `ART_PEDIA`, …) | ☐ XML only — referenced from `ui` / `world` art blocks |

Paths are relative to `Assets/Data/` (ported) or `Assets/XML/` (not). The legacy XML holds the **complete** id
set; when an XML-only infotype is ported it follows the same `INFOTYPE_NAME` rule and gains a `✅` row.

The catch-all engine tokens that are **not** infotype ids (`TURN`, `POPULATION`, `MILITARY`, `AREA_SIZE`,
`UNIT_LEVEL`, `SELF`, …) live in the [json spec](json.md) §3.1, not here.

---

## ⛔ THE VERB SAYS WHICH ENTITY — `build` is an IMPROVEMENT, `construct` is a BUILDING

**In Civ4 vocabulary `build` means a WORKER BUILD** — the `BUILD_` infotype above, a plot improvement action
(`BuildTypes`, `CvPlot::canBuild`, `MISSION_BUILD`). **A BUILDING is CONSTRUCTED** (`BuildingTypes`,
`canConstruct`). Two registries, two gates, so a name saying `build` while meaning a building points the reader
at the wrong entity and the wrong machine.

⚠ **The shortened form reads fine, which is the trap.** `buildList` looks like an unremarkable abbreviation of
`buildingList` and is a different concept. ⇒ Never shorten `building` to `build` in a name YOU write.
⛔ **The INHERITED offenders are NOT a rename backlog: *"that the naming is ass is unfortunately not
something we can do a lot with."*** The C2C-era UI classes (`BuildingFilterCanBuild`, `CvBuildingList`, …) carry
this throughout and a sweep buys nothing. The rule binds what you NAME, not what you find.
