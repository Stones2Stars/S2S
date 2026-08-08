#pragma once
#ifndef CV_INFO_KINDS_H
#define CV_INFO_KINDS_H

//
//	CvInfoKinds -- the SHARED modifier-family / kind-enum vocabulary (patterns.md § coherent surface;
//	info-rebuild.md toolkit item 4). Transcribed from the live family census over Assets/Data
//	(`python Tools/Migration/family_census.py`) under the owner's enum-walk rulings (info-rebuild.md):
//	  - A kind names its CONCEPT only; the SCOPE a value is authored at is a SEPARATE axis of the deposit
//	    address and a spelled-out getter parameter ([DEC-scope-is-an-axis]). No scope word ever appears in a
//	    kind name; the per-family scope PARTICIPATION mask is declared beside each family's vocabulary.
//	  - Families the engine already enumerates REUSE the engine enum (ruling 1): food/production/commerce key
//	    YieldTypes; gold/research/culture/espionage key CommerceTypes; domain-keyed deposits key DomainTypes
//	    through their FK-resolved target ids. No new vocabulary where the engine already speaks one.
//	  - A member is a KIND only if it answers "WHICH component does this modify" (ruling 2). Condition-as-member
//	    authorings (homeArea/otherArea, cityLimit, tradeRoute, perInstance, ...) are NOT kinds -- they re-author
//	    as conditioned deposits / per-scalers on the curator batches; until the data moves they flow through the
//	    compile pass as interned member ids and surface in the load-time kind-coverage diagnostic.
//	  - Type-keyed members (UNITCOMBAT_*/UNIT_*/TERRAIN_*/BUILD_*/RELIGION_*/...) are interned DATA ids
//	    (FK-resolved deposit targets), never enum entries.
//	  - 1-2-entry straggler families read through the ONE InfoScalar enum (patterns.md getScalar).
//
//	Every enum's entry 0 is the family's scope-wide AMOUNT (the memberless deposit); families that author no
//	memberless deposit simply never fill slot 0.
//

#include "CvCondition.h"   // CvCascScope -- the ONE containment-spine scope enum (json.md §3.2); the deposit
                           // axis this vocabulary pairs with. Reused, never re-minted.
#include <set>
#include <string>

// Scope-participation bit for a CvCascScope value (the per-family masks below OR these).
#define INFO_SCOPE_BIT(eScope) (1 << (int)(eScope))

// json §3.6 units -- the NATURE of a magnitude (the combine math is family metadata, not carried here). FLAT/
// PERCENT/MULTIPLIER are the authoring set; the rest are the engine-internal / count-scaling units readJson
// recognizes. Part of the compiled slot KEY: a flat sum and a percent sum are separate slots (modifier.md §2).
enum CvCascUnit
{
	CASC_UNIT_UNKNOWN = 0,
	CASC_UNIT_FLAT, CASC_UNIT_PERCENT, CASC_UNIT_MULTIPLIER,
	CASC_UNIT_POST_MULTIPLIER, CASC_UNIT_RAW_PERCENT,
	CASC_UNIT_PER_POPULATION, CASC_UNIT_PER_SPECIALIST, CASC_UNIT_PER_CORPORATION_LEVEL,
	CASC_UNIT_COUNT   // the modifier.md §6 count-by-type leaf (freeSpecialists/allowedSpecialists) -- synthesized
	                  // by the CvModifiers walk for a bare-number leaf; never an authored unit key
};

//
// ============================ the CLOSED modifier-family vocabulary ============================
// One entry per census family (71) + the open per-property plane (one family per PROPERTY_* info, keyed by
// the property's FK id). Mirrors the reader's CJK_FAMILY_KEYS table (CvJsonParse.cpp) one-to-one -- the two
// stay in lock-step with the census.
//
enum ModifierFamily
{
	MODFAM_NONE = -1,
	MODFAM_AIR = 0,
	MODFAM_ALLOWED_SPECIALISTS,
	MODFAM_ANARCHY,
	MODFAM_BARBARIANS,
	MODFAM_BOMBARD,
	MODFAM_BUILD_RATE,
	MODFAM_CAPTURE,
	MODFAM_CARGO,
	MODFAM_CITY_CAPTURE,   // ruling 16: onCapture trigger-plane chance data -- stays in the vocabulary (1 authored
	                       // entity would otherwise become an unknown-key ERROR) until the trigger-plane re-home
	MODFAM_COLLATERAL,
	MODFAM_COMBAT,
	MODFAM_COMMERCE,
	MODFAM_CONSCRIPT,
	MODFAM_COSTS,
	MODFAM_CULTURE,
	MODFAM_CULTURE_DISTANCE,
	MODFAM_DEFENSE,
	MODFAM_DIPLOMACY,
	MODFAM_DOMAIN_MOVES,
	MODFAM_DURATIONS,
	MODFAM_ESPIONAGE,
	MODFAM_ESPIONAGE_DEFENSE,
	MODFAM_EVENT_CHANCE,
	MODFAM_EXPERIENCE,
	MODFAM_EXTRA_YIELD_THRESHOLD,
	MODFAM_FEATURE_PRODUCTION,
	MODFAM_FIRST_STRIKE,
	MODFAM_FOOD,
	MODFAM_FOOD_KEPT,
	MODFAM_FREE_SPECIALISTS,
	MODFAM_GOLD,
	MODFAM_GOLDEN_AGE,
	MODFAM_GREAT_GENERAL_RATE,
	MODFAM_GREAT_PEOPLE_RATE,
	MODFAM_GROWTH,
	MODFAM_HAPPINESS,
	MODFAM_HEAL,
	MODFAM_HEALTH,
	MODFAM_HURRY,
	MODFAM_HURRY_ANGER,
	MODFAM_IMPROVEMENT_UPGRADE_RATE,
	MODFAM_INFLATION,
	MODFAM_LESS_YIELD_THRESHOLD,
	MODFAM_MAINTENANCE,
	MODFAM_MISSION_YIELD_MULTIPLIER,
	MODFAM_MOVEMENT,
	MODFAM_OCCUPATION_TIME,
	MODFAM_ODDS,
	MODFAM_PILLAGE,
	MODFAM_POPULATION_GROWTH_RATE,
	MODFAM_PRODUCTION,
	MODFAM_RANGE,
	MODFAM_RELIGION,
	MODFAM_RESEARCH,
	MODFAM_RESEARCH_RATE,
	MODFAM_REVOLT_PROTECTION,
	MODFAM_REVOLUTION,
	MODFAM_SPAWN_RATE,
	MODFAM_SPEED,
	MODFAM_STATE_RELIGION,
	MODFAM_STRENGTH,
	MODFAM_SURVIVOR,   // ruling 16: onDeath survive-chance trigger-plane data -- stays in the vocabulary (1 authored
	                   // entity would otherwise become an unknown-key ERROR) until the trigger-plane re-home
	MODFAM_TRADE_MISSION,
	MODFAM_TRADE_ROUTES,
	MODFAM_UNDERWORLD,
	MODFAM_UPKEEP,
	MODFAM_VISION,   // vision.md: how well an OBSERVER sees -- its STRENGTH, and the unit plane's only source
	                 // (base stat + promotions), the ground's elevation and obstruction, and the
	                 // hide-and-seek pair. One family; the KINDS below are what it is made of.
	MODFAM_WAR_WEARINESS,
	MODFAM_WITHDRAWAL,
	MODFAM_WORK_RATE,
	MODFAM_PROPERTY,   // the OPEN per-property plane (family key = the PROPERTY_* type; FK id carried beside)
	NUM_MODIFIER_FAMILIES
};

//
// ============================ the kind enums (multi-kind families) ============================
// Kind names the CONCEPT only ([DEC-scope-is-an-axis]); the scope-participation mask beside each enum records
// WHERE the census authors the family. Entry 0 is always the scope-wide amount.
//

// The FOUR wellbeing channels (modifier.md §2b -- owner: FOUR ORDINARY CHANNELS, no polarity mode). The AUTHORED
// families are happiness/health only (wellbeing mints ZERO kinds, ruling 12); a negative deposit ROUTES to the
// opposing channel at fill/valuation -- a routing rule, never a storage shape -- so ANGER/UNHEALTH mint no family
// and no slot. This enum keys the per-group wellbeing OUT-array (expectedWellbeing) and every four-channel read.
enum WellbeingChannel
{
	WELLBEING_HAPPINESS = 0,
	WELLBEING_ANGER,
	WELLBEING_HEALTH,
	WELLBEING_UNHEALTH,
	NUM_WELLBEING_CHANNELS
};

// The engine-enum channel families (food/production/commerce -> YieldTypes; gold/research/culture/espionage ->
// CommerceTypes) share ONE kind pair: the channel amount, plus the ledgered PERMANENT golden-age member-mirror
// (modifier.md §3 -- golden-age yield/commerce is engine-applied, effect-only, never retired to a predicate).
// CHANNEL_CORPORATION is authored by the COMMERCE family only (ruling 15: legacy corporationRevenue lands as
// the `corporation` source-component kind on the commerce side, the R11 pattern) -- the member table gives the
// row to MODFAM_COMMERCE alone, so the yield/split-commerce families never resolve it.
enum ChannelKind
{
	CHANNEL_AMOUNT = 0,
	CHANNEL_GOLDEN_AGE,
	CHANNEL_CORPORATION,
	CHANNEL_GARRISON,     // CULTURE only -- a unit's garrison culture (culture.unit.garrison); see MEMBERS_CULTURE
	NUM_CHANNEL_KINDS
};
const int YIELD_FAMILY_SCOPES    = INFO_SCOPE_BIT(CASC_SCOPE_CITY) | INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE) | INFO_SCOPE_BIT(CASC_SCOPE_PLOT);
// PLOT rides the commerce-channel class for culture alone (census: 29 improvement culture.plot flats); gold/
// research author city|empire only, espionage carries its own ESPIONAGE_SCOPES. Culture's unit-scope garrison
// authoring (46 units, the ruling-2 condition-as-member triage pending) rides the per-family census mask
// (infoFamilyScopeMask), not this class-wide const.
const int COMMERCE_FAMILY_SCOPES = INFO_SCOPE_BIT(CASC_SCOPE_CITY) | INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE) | INFO_SCOPE_BIT(CASC_SCOPE_PLOT);

// espionage is BOTH a commerce channel and the unit-plane spy-stat family (the scope axis separates them:
// city/empire = the commerce channel, unit = the spy stats).
enum EspionageKind
{
	ESPIONAGE_AMOUNT = 0,
	ESPIONAGE_GOLDEN_AGE,
	// ⛔ NO insidiousness / investigation here: those are MODFAM_UNDERWORLD (the in-city criminal contest),
	// a separate mechanic that shares no stat with espionage.
	NUM_ESPIONAGE_KINDS
};
const int ESPIONAGE_SCOPES = INFO_SCOPE_BIT(CASC_SCOPE_CITY) | INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE) | INFO_SCOPE_BIT(CASC_SCOPE_UNIT);

enum DefenseKind
{
	DEFENSE_AMOUNT = 0,             // the additive defense % (member `amount`)
	DEFENSE_MIN,                    // the defense floor
	DEFENSE_AIR,                    // defense against air attack (authored `air` at plot, `airDefense` at city --
	                                // one concept, two census spellings; the scope axis separates the consumers).
	                                // SCOPE-SPLIT UNIT (census + engine): plot = FLAT rolled magnitude (101
	                                // improvement flats; the opposed getSorenRandNum dice, CvUnit.cpp:7126),
	                                // city = PERCENT damage modifier (17 building percents) -- infoDefenseUnit's
	                                // scope-aware overload carries the split
	DEFENSE_BOMBARD,                // bombard-damage resistance
	DEFENSE_NUKE,                   // nuke-damage resistance
	DEFENSE_DYNAMIC,                // the dynamic-defense contribution
	DEFENSE_NO_ENTRY_LEVEL,         // the no-entry threshold (distinct from the floor, owner)
	DEFENSE_RIVER_PENALTY,          // river-attack defense penalty
	DEFENSE_BUILDING_RECOVERY,      // building-defense recovery rate
	DEFENSE_CITY_RECOVERY,          // city-defense recovery rate
	DEFENSE_ADJACENT_DAMAGE,        // damage dealt to adjacent attackers
	DEFENSE_COUNTER_DAMAGE,         // counter-damage dealt to attackers (nested counterDamage.damage)
	DEFENSE_COUNTER_DAMAGE_CHANCE,  // counter-damage trigger chance (nested counterDamage.chance)
	NUM_DEFENSE_KINDS
};
const int DEFENSE_SCOPES = INFO_SCOPE_BIT(CASC_SCOPE_CITY) | INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE) | INFO_SCOPE_BIT(CASC_SCOPE_PLOT);

enum MaintenanceKind
{
	MAINTENANCE_AMOUNT = 0,         // the scope-wide maintenance amount/modifier (census alias `all`)
	MAINTENANCE_NUM_CITIES,
	MAINTENANCE_DISTANCE,
	MAINTENANCE_CORPORATION,
	MAINTENANCE_COLONY,
	NUM_MAINTENANCE_KINDS
};
const int MAINTENANCE_SCOPES = INFO_SCOPE_BIT(CASC_SCOPE_CITY) | INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE);

// Ruling 18: ONE costs family, kinds by category; scope is the axis (the world*-prefixed legacy kinds are
// retired by it). perInstance is a §3.7 per-scaler (ruling 4), never a kind.
enum CostsKind
{
	COSTS_AMOUNT = 0,
	COSTS_TRAIN,
	COSTS_CONSTRUCT,
	COSTS_CREATE,
	COSTS_BUILD,
	COSTS_RESEARCH,
	COSTS_IMPROVEMENT_UPGRADE,
	COSTS_RESEARCH_CUT_BELOW_ERA,
	COSTS_HURRY,
	COSTS_UPGRADE,
	NUM_COSTS_KINDS
};
const int COSTS_SCOPES = INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE) | INFO_SCOPE_BIT(CASC_SCOPE_WORLD) | INFO_SCOPE_BIT(CASC_SCOPE_UNIT);

// Ruling 5: combat = everything that MODIFIES the base strength. The type-keyed vs-entries
// (unitCombat/flankingUnit/vsUnit/feature/terrain/domain containers) are interned data ids. Ruling 16
// (trigger-plane chance data, pending the trigger-plane re-home): `combat.world.subdueAnimal` (8 handicaps)
// and `combat.team.nukeInterception` (4 projects) are NEVER kinds here -- they flow through as unkinded
// members (kept parseable so the authored data never becomes an unknown-key error), exactly like the
// survivor/cityCapture family rows.
enum CombatKind
{
	COMBAT_AMOUNT = 0,
	COMBAT_ATTACK,
	COMBAT_DEFENSE,
	COMBAT_CITY_ATTACK,
	COMBAT_CITY_DEFENSE,
	COMBAT_HILLS_ATTACK,
	COMBAT_HILLS_DEFENSE,
	COMBAT_STEALTH,
	COMBAT_STEALTH_STRIKES,
	COMBAT_LUNGE,
	COMBAT_UNNERVE,
	COMBAT_ENCLOSE,
	COMBAT_TAUNT,
	COMBAT_DYNAMIC_DEFENSE,
	COMBAT_DAMAGE_MODIFIER,
	COMBAT_BREAKDOWN_CHANCE,
	COMBAT_BREAKDOWN_DAMAGE,
	COMBAT_LIMIT,                     // member `combatLimit` -- the unit's combat-limit change (flat)
	COMBAT_KAMIKAZE,
	COMBAT_RELIGIOUS,
	COMBAT_VS_BARBS,
	COMBAT_ANIMAL,
	COMBAT_BARBARIAN,
	COMBAT_FREE_WINS_VS_BARBS,
	NUM_COMBAT_KINDS
};
const int COMBAT_SCOPES = INFO_SCOPE_BIT(CASC_SCOPE_UNIT) | INFO_SCOPE_BIT(CASC_SCOPE_CITY) | INFO_SCOPE_BIT(CASC_SCOPE_TEAM)
                        | INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE) | INFO_SCOPE_BIT(CASC_SCOPE_WORLD);

// heal.city (ruling 13: the city contribution of the one heal calc) + the unit-plane heal stats. The
// UNITCOMBAT_HEALS_AS_* entries under the unitCombat container are interned data ids.
enum HealKind
{
	HEAL_AMOUNT = 0,
	HEAL_RATE,                // member `heal` -- the unit's own heal-rate change
	HEAL_SUPPORT,
	HEAL_ADJACENT,            // member `adjacentHeal`
	HEAL_ADJACENT_TILE,
	HEAL_SAME_TILE,
	HEAL_SELF_MODIFIER,
	HEAL_ENEMY_TERRITORY,     // member `enemy`
	HEAL_FRIENDLY_TERRITORY,  // member `friendly`
	HEAL_NEUTRAL_TERRITORY,   // member `neutral`
	HEAL_VICTORY,
	HEAL_VICTORY_STACK,
	HEAL_VICTORY_ADJACENT,
	NUM_HEAL_KINDS
};
const int HEAL_SCOPES = INFO_SCOPE_BIT(CASC_SCOPE_UNIT) | INFO_SCOPE_BIT(CASC_SCOPE_CITY);

// perPopulation is a §3.7 per-scaler (ruling 4); upkeep.upgrade* absorbed into costs.upgrade (ruling 18);
// the civicOptions container keys CIVICOPTION_* data ids.
enum UpkeepKind
{
	UPKEEP_AMOUNT = 0,
	UPKEEP_CIVIC,
	UPKEEP_FREE_MILITARY,
	UPKEEP_FREE_CIVILIAN,
	UPKEEP_UNIT_MILITARY,
	UPKEEP_UNIT_CIVILIAN,
	UPKEEP_UNIT,
	UPKEEP_INFLATION,
	UPKEEP_SUPPLY,
	UPKEEP_EXTRA,
	NUM_UPKEEP_KINDS
};
const int UPKEEP_SCOPES = INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE) | INFO_SCOPE_BIT(CASC_SCOPE_UNIT);

// Ruling 11: ONE tradeRoutes family; the variant members (foreign/coastal/sharedCivic/...) are CONDITIONS,
// re-authored as predicates on the curator batch -- never kinds. Entry 0 is the flat route count.
// Ruling 27: `modifier` carries the CHANNEL axis (YieldTypes reuse) for the §2a tradeYield input fold --
// authored `tradeRoutes.<scope>.modifier.<channel>.<unit>`. The three channel kinds sit CONTIGUOUS in
// YieldTypes order (FOOD, PRODUCTION, COMMERCE), so the parameterized read is
// TRADE_ROUTE_MODIFIER_FOOD + eYield. The bare TRADE_ROUTE_MODIFIER stays the channel-AGNOSTIC route-PROFIT
// stage (building iTradeRouteModifier + the IS_FOREIGN/SHARES_CIVIC conditioned entries -- totalTradeModifier).
enum TradeRouteKind
{
	TRADE_ROUTE_AMOUNT = 0,        // the scope-wide flat ROUTE COUNT (the memberless deposit -- kind 0 IS the
	                               // count, ruling 11; the transient `routes` member spelling died with the
	                               // reconciliation curator fix, so no member row exists for slot 0)
	TRADE_ROUTE_MODIFIER,          // channel-agnostic route-PROFIT %
	TRADE_ROUTE_MAX,               // the cap
	TRADE_ROUTE_MODIFIER_FOOD,     // per-channel route-yield % (YieldTypes order from here)
	TRADE_ROUTE_MODIFIER_PRODUCTION,
	TRADE_ROUTE_MODIFIER_COMMERCE,
	NUM_TRADE_ROUTE_KINDS
};
const int TRADE_ROUTES_SCOPES = INFO_SCOPE_BIT(CASC_SCOPE_CITY) | INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE) | INFO_SCOPE_BIT(CASC_SCOPE_WORLD);

enum MovementKind
{
	MOVEMENT_MOVES = 0,       // base/extra moves (memberless + the `moves` spelling)
	MOVEMENT_MOVE_DISCOUNT,
	MOVEMENT_DROP_RANGE,
	NUM_MOVEMENT_KINDS
};
const int MOVEMENT_SCOPES = INFO_SCOPE_BIT(CASC_SCOPE_UNIT) | INFO_SCOPE_BIT(CASC_SCOPE_PLOT);

// Vision (vision.md) -- ONE family, the sight budget spent walking outward exactly as movement is. The three
// kinds are what the model is made of, and the SCOPE says whose: a unit's STRENGTH is its base stat plus its
// promotions and nothing else; ELEVATION is positional (a peak is 2, so a unit standing there has 2 and only
// there) or a city's own, raised by its buildings -- which is why a building can never elevate a unit passing
// through; OBSTRUCTION is what the ground costs to see through. Strength and elevation are interchangeable
// currencies against obstruction: a jungle demands extra budget, payable by seeing better or standing higher.
enum VisionKind
{
	VISION_STRENGTH = 0,      // how well the observer sees (memberless -- the family's default kind)
	VISION_ELEVATION,         // how high it is; grants sight to whoever looks from it
	VISION_OBSTRUCTION,       // what it costs to see THROUGH this ground
	NUM_VISION_KINDS
};
// ⛔ THE HIDE-AND-SEEK PAIR IS NOT IN THIS FAMILY (owner). `concealment`/`detection` are the `hideAndSeek` BLOCK
// (json.md §9, CvHideAndSeekSection): this family answers how FAR you see, that block answers whether you
// PERCEIVE what stands inside the reach -- a graduated contest with its OWN evaluation. The legacy engine's two
// evaluations bled into each other for years, so a shared family is exactly what would let that re-form. The
// contest reads this budget for reach and never the reverse.
const int VISION_SCOPES = INFO_SCOPE_BIT(CASC_SCOPE_UNIT) | INFO_SCOPE_BIT(CASC_SCOPE_PLOT) | INFO_SCOPE_BIT(CASC_SCOPE_CITY);
// THE VISION SCALE. One plot of open ground costs 100 -- the same "one step = 100" denominator movement already
// uses (MOVE_DENOMINATOR), so the two families read alike. A baseline of 1 was the thing that had to go: it left
// no room between values, and the data shows what that cost -- ALL 78 obstruction-authoring features carried the
// identical `1`, because forest simply could not be made cheaper than jungle. At 100 it can (forest 60, jungle
// 100), and a promotion can add a quarter of a plot.
// ⛔ ONE SCALE, ALWAYS -- every vision number is in this unit. Movement fractured into two (terrain in whole
// moves, routes in denominator units) precisely because its baseline could not express a part-step; vision has
// no such split to inherit and must never grow one. ×100 fixed point still sits underneath for anything finer.
const int VISION_PLOT = 100;                             // the authored (human) cost of one open plot
const int VISION_OPEN_GROUND_COST = VISION_PLOT * 100;   // the same in the engine's native ×100

// Ruling 5 re-homes: flightRange/missileRange land here beside intercept/evasion.
enum AirKind
{
	AIR_AMOUNT = 0,
	AIR_INTERCEPT,
	AIR_EVASION,
	AIR_RANGE,
	AIR_NUKE_RANGE,
	NUM_AIR_KINDS
};
const int AIR_SCOPES = INFO_SCOPE_BIT(CASC_SCOPE_UNIT) | INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE);

// Ruling 5 re-homes: captureProbability/captureResistance live here (distinct from the trigger-plane
// cityCapture odds, ruling 16).
enum CaptureKind
{
	CAPTURE_AMOUNT = 0,
	CAPTURE_PROBABILITY,
	CAPTURE_RESISTANCE,
	NUM_CAPTURE_KINDS
};
const int CAPTURE_SCOPES = INFO_SCOPE_BIT(CASC_SCOPE_UNIT) | INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE);

// Ruling 5 re-homes: missileCargo/navalCargo land here beside the §6 space/size pair (modifier.md §6).
enum CargoKind
{
	CARGO_AMOUNT = 0,
	CARGO_SPACE,
	CARGO_SIZE,
	CARGO_MISSILE,
	CARGO_NAVAL,
	NUM_CARGO_KINDS
};
const int CARGO_SCOPES = INFO_SCOPE_BIT(CASC_SCOPE_UNIT) | INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE);

enum CollateralKind
{
	COLLATERAL_AMOUNT = 0,
	COLLATERAL_DAMAGE,
	COLLATERAL_LIMIT,
	COLLATERAL_MAX_UNITS,
	COLLATERAL_PROTECTION,
	NUM_COLLATERAL_KINDS
};
const int COLLATERAL_SCOPES = INFO_SCOPE_BIT(CASC_SCOPE_UNIT);

enum BombardKind
{
	BOMBARD_AMOUNT = 0,
	BOMBARD_RATE,
	BOMBARD_AIR_BOMB_RATE,
	NUM_BOMBARD_KINDS
};
const int BOMBARD_SCOPES = INFO_SCOPE_BIT(CASC_SCOPE_UNIT);

enum ExperienceKind
{
	EXPERIENCE_AMOUNT = 0,
	EXPERIENCE_IN_BORDER,
	EXPERIENCE_LEVEL_MODIFIER,
	NUM_EXPERIENCE_KINDS
};
const int EXPERIENCE_SCOPES = INFO_SCOPE_BIT(CASC_SCOPE_CITY) | INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE) | INFO_SCOPE_BIT(CASC_SCOPE_UNIT);

enum RevolutionKind
{
	REVOLUTION_AMOUNT = 0,
	// local and national are DISTINCT MECHANICS, not scope variants (owner): LOCAL tracks a single city
	// rising in revolt; NATIONAL tracks the country fracturing (multiple cities seceding together). Two
	// indices, two outcomes -- never collapse them onto the scope axis. Revolutions is Python-authoritative
	// and due its own rework; this vocabulary only carries the data faithfully.
	REVOLUTION_LOCAL,
	REVOLUTION_NATIONAL,
	REVOLUTION_DISTANCE_MODIFIER,
	REVOLUTION_HOLY_CITY_GOOD,
	REVOLUTION_HOLY_CITY_BAD,
	REVOLUTION_DEMOCRACY_LEVEL,
	REVOLUTION_GOOD_RELIGION,
	REVOLUTION_BAD_RELIGION,
	REVOLUTION_NATIONALITY,
	REVOLUTION_RELIGIOUS_FREEDOM,
	REVOLUTION_ENVIRONMENTAL_PROTECTION,
	REVOLUTION_VIOLENT,
	REVOLUTION_LABOR_FREEDOM,
	NUM_REVOLUTION_KINDS
};
const int REVOLUTION_SCOPES = INFO_SCOPE_BIT(CASC_SCOPE_CITY) | INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE);

enum StateReligionKind
{
	STATE_RELIGION_AMOUNT = 0,
	STATE_RELIGION_GREAT_PEOPLE_RATE,
	STATE_RELIGION_HAPPINESS,
	STATE_RELIGION_HOLY_CITY_XP,
	STATE_RELIGION_BUILDING_PRODUCTION,
	STATE_RELIGION_UNIT_PRODUCTION,
	STATE_RELIGION_SPREAD_PROBABILITY,
	STATE_RELIGION_NON_STATE_SPREAD_PROBABILITY,
	STATE_RELIGION_FREE_EXPERIENCE,
	NUM_STATE_RELIGION_KINDS
};
const int STATE_RELIGION_SCOPES = INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE);

// The civics container keys CIVIC_* data ids.
enum DiplomacyKind
{
	DIPLOMACY_AMOUNT = 0,
	DIPLOMACY_ATTITUDE,
	DIPLOMACY_ATTITUDE_SHARE,
	DIPLOMACY_WAR_WEARINESS,
	DIPLOMACY_ENEMY_WAR_WEARINESS,
	DIPLOMACY_DECLARE_WAR,
	DIPLOMACY_NO_TECH_TRADE,
	DIPLOMACY_TECH_TRADE_KNOWN,
	DIPLOMACY_TECH_SHARE,
	NUM_DIPLOMACY_KINDS
};
const int DIPLOMACY_SCOPES = INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE) | INFO_SCOPE_BIT(CASC_SCOPE_TEAM);

enum DurationsKind
{
	DURATIONS_AMOUNT = 0,
	DURATIONS_CIVIC_ANARCHY,
	DURATIONS_RELIGIOUS_ANARCHY,
	DURATIONS_ANGER,
	NUM_DURATIONS_KINDS
};
const int DURATIONS_SCOPES = INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE) | INFO_SCOPE_BIT(CASC_SCOPE_WORLD);

// buildRate.self = build THIS entity faster (the off-spine self scope); the category kinds ride the ordinary
// scopes; keyed unit/building/domain/unitCombat targets are interned data ids (modifier.md §4).
enum BuildRateKind
{
	BUILD_RATE_AMOUNT = 0,
	BUILD_RATE_MILITARY,
	BUILD_RATE_SPACE,
	BUILD_RATE_WORLD_WONDER,
	BUILD_RATE_TEAM_WONDER,
	BUILD_RATE_NATIONAL_WONDER,
	NUM_BUILD_RATE_KINDS
};
const int BUILD_RATE_SCOPES = INFO_SCOPE_BIT(CASC_SCOPE_SELF) | INFO_SCOPE_BIT(CASC_SCOPE_CITY) | INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE);

enum BarbariansKind
{
	BARBARIANS_AMOUNT = 0,
	BARBARIANS_ANIMAL_ATTACK_PROB,
	BARBARIANS_WATER_TILES_PER_UNIT,
	BARBARIANS_TILES_PER_CITY,
	BARBARIANS_CITY_CREATION_TURNS,
	BARBARIANS_CITY_CREATION_PROB,
	BARBARIANS_DEFENDERS,
	NUM_BARBARIANS_KINDS
};
const int BARBARIANS_SCOPES = INFO_SCOPE_BIT(CASC_SCOPE_WORLD);

// Ruling 3: the in-city criminal game (criminals burrow, are investigated, arrested); the stray 2-entity
// `investigation` family merged in. `detection` stays RESERVED for the map-level hide-and-seek plane.
enum UnderworldKind
{
	UNDERWORLD_AMOUNT = 0,
	UNDERWORLD_INSIDIOUSNESS,
	UNDERWORLD_INVESTIGATION,
	NUM_UNDERWORLD_KINDS
};
const int UNDERWORLD_SCOPES = INFO_SCOPE_BIT(CASC_SCOPE_CITY);

//
// ============================ the straggler scalars (patterns.md getScalar) ============================
// Families whose vocabulary is one or two genuinely lone values. Each entry maps to its (family, kind) slot
// through infoScalarSlot; the read is the same compiled-sum fetch as every point getter.
//
enum InfoScalar
{
	SCALAR_ANARCHY = 0,               // anarchy.empire % -- a building anywhere shortens the empire's anarchy
	SCALAR_CONSCRIPT,                 // conscript.empire flat
	SCALAR_CULTURE_DISTANCE,          // cultureDistance.plot flat
	SCALAR_ESPIONAGE_DEFENSE,         // espionageDefense (city flat / empire %)
	SCALAR_EVENT_CHANCE,              // eventChance.world flat
	SCALAR_FEATURE_PRODUCTION,        // featureProduction.empire %
	SCALAR_FIRST_STRIKE_CHANCES,      // firstStrike.unit.chance flat (member `chance`, 115 authored; the memberless
	                                  // slot 0 is unauthored -- census, reconciliation item 2)
	SCALAR_FIRST_STRIKES,             // firstStrike.unit.strikes flat
	SCALAR_FOOD_KEPT,                 // foodKept.city %
	SCALAR_GOLDEN_AGE_LENGTH,         // goldenAge.empire % (length modifier; the grant is grants.goldenAge)
	SCALAR_GREAT_GENERAL_RATE,        // greatGeneralRate %
	SCALAR_GREAT_GENERAL_RATE_DOMESTIC, // greatGeneralRate.domestic %
	SCALAR_GREAT_PEOPLE_RATE,         // greatPeopleRate (keyed UNIT_* targets stay entry-list reads)
	SCALAR_GROWTH,                    // growth %
	SCALAR_HURRY_ANGER,               // hurry.anger %
	SCALAR_HURRY_INFLATION,           // hurry.inflation %
	SCALAR_HURRY_ANGER_MODIFIER,      // hurryAnger.city %
	SCALAR_IMPROVEMENT_UPGRADE_RATE,  // improvementUpgradeRate.empire %
	SCALAR_INFLATION,                 // inflation.empire %
	SCALAR_MISSION_YIELD_MULTIPLIER,  // missionYieldMultiplier.world %
	SCALAR_OCCUPATION_TIME,           // occupationTime.city %
	SCALAR_PILLAGE,                   // pillage.unit flat
	SCALAR_POPULATION_GROWTH_RATE,    // populationGrowthRate.city %
	SCALAR_RANGE,                     // range flat
	SCALAR_RESEARCH_RATE,             // researchRate.empire % (keyed TECH_* targets stay entry-list reads)
	SCALAR_REVOLT_PROTECTION,         // revoltProtection.unit %
	SCALAR_SPAWN_RATE_NPC_PEACE,      // spawnRate.npcPeace %
	SCALAR_SPEED,                     // speed.world %
	SCALAR_STRENGTH,                  // strength.unit flat (ruling 5: the unit's BASE value only)
	SCALAR_SURVIVOR,                  // survivor.unit % (ruling 16: trigger-plane chance data -- row stays until the re-home)
	SCALAR_TRADE_MISSION,             // tradeMission.empire %
	SCALAR_WAR_WEARINESS,             // warWeariness %
	SCALAR_ENEMY_WAR_WEARINESS,       // warWeariness.enemy %
	SCALAR_WITHDRAWAL,                // withdrawal.unit %
	SCALAR_WORK_RATE,                 // workRate % (keyed BUILD_*/TERRAIN_* targets stay entry-list reads)
	SCALAR_WORK_RATE_HILLS,           // workRate.hills %
	NUM_INFO_SCALARS
};

//
// ============================ resolution API (implemented in CvInfoKinds.cpp) ============================
//

// The family key -> the closed vocabulary (MODFAM_PROPERTY for a PROPERTY_* key; MODFAM_NONE otherwise).
ModifierFamily infoFamilyFromKey(const std::string& szKey);
// The family's authored key spelling (NULL for MODFAM_PROPERTY -- the property's own type string is the key).
const char* infoFamilyKey(ModifierFamily eFamily);
// The census scope-participation mask (INFO_SCOPE_BIT bits) for the family.
int infoFamilyScopeMask(ModifierFamily eFamily);
// Member path (post-scope non-key segments joined with '.') -> the family's kind id; -1 = not in the
// vocabulary (a batch-pending condition-as-member, a per-scaler spelling, or a data typo -- the caller keeps
// the interned member id and records the miss via infoNoteUnkindedMember).
int infoResolveKind(ModifierFamily eFamily, const std::string& szMemberPath);
// Ruling 1 engine reuse: the YieldTypes / CommerceTypes value a channel family keys (-1 = not that class).
int infoFamilyYield(ModifierFamily eFamily);
int infoFamilyCommerce(ModifierFamily eFamily);
// The ruling-1 REVERSE lookups: the channel family a YieldTypes / CommerceTypes value authors under
// (MODFAM_NONE for an out-of-range index). The parameterized channel getters (getFlatYield(eYield, eScope) et
// al.) and the per-group valuation endpoints key their family axis through these.
ModifierFamily infoYieldFamily(int iYield);
ModifierFamily infoCommerceFamily(int iCommerce);
// The wellbeing channel's AUTHORED family (modifier.md §2b): HAPPINESS/ANGER -> MODFAM_HAPPINESS,
// HEALTH/UNHEALTH -> MODFAM_HEALTH -- the opposing channels share one authored family; the SIGN routes.
ModifierFamily infoWellbeingFamily(WellbeingChannel eChannel);
// A straggler scalar's (family, kind) slot.
void infoScalarSlot(InfoScalar eScalar, ModifierFamily& eFamilyOut, int& iKindOut);
// The canonical AUTHORED unit of a defense kind (the typed point getter's unit axis). SCOPE-AWARE: DEFENSE_AIR
// is the one scope-split defense kind (plot = FLAT rolled magnitude, city = PERCENT damage modifier -- see the
// enum row). The scope-blind overload answers with the CITY plane (the dominant consumer class) and is the
// transitional form the wave getters still call -- their move to the scope-aware call is the reported follow-up.
CvCascUnit infoDefenseUnit(DefenseKind eKind, CvCascScope eScope);
CvCascUnit infoDefenseUnit(DefenseKind eKind);
// The canonical AUTHORED unit of any (family, kind, scope) slot -- the generalization infoDefenseUnit
// specializes: the per-group typed point getters (getMaintenanceModifier / getUpkeep / getRevolution / ...)
// resolve their unit axis here, so the census's flat-vs-percent verdict lives ONCE beside the vocabulary, never
// re-decided per getter. THE SCOPE AXIS IS PART OF THE ANSWER where the census authors one kind at different
// units per scope (capture: unit flat / empire percent; experience amount: unit percent / city+empire flat;
// maintenance corporation: city flat / empire percent; defense air: plot flat / city percent). A kind that
// carries BOTH units at ONE (kind, scope) slot keeps the split in the getter NAME instead (getFlatYield vs
// getYieldModifier -- [DEC-fixedpoint-x100]); for those dual slots this table answers the DOMINANT authored
// plane (each documented at its row). The scope-blind overload delegates with the family's dominant authored
// scope -- the transitional form for call sites that predate the scope axis.
CvCascUnit infoKindUnit(ModifierFamily eFamily, int iKind, CvCascScope eScope);
CvCascUnit infoKindUnit(ModifierFamily eFamily, int iKind);
// Family-combine FLOOR metadata (modifier.md §2: non-additive combine modes are FAMILY metadata, never
// per-deposit): true = the (family, kind) slot's COMBINED total floors at zero -- max(0, Σ). Declared here
// beside the vocabulary because the mode belongs to the kind, not to any value. Current rows: the pop-scaled
// free-unit-upkeep pair upkeep.freeMilitary / upkeep.freeCivilian (ruling 28: entries are free-amount
// semantics, positive grants / negative shrinks, summed then floored -- free >= 0; the engine's SECOND floor,
// net = max(0, upkeep - free) per class, stays in the engine formula at the combine site, CvPlayer.cpp:10295/:10315).
bool infoCombineFloorAtZero(ModifierFamily eFamily, int iKind);
// ⛔ ALIGNMENT POLARITY -- does a POSITIVE value on this (family, kind) HELP its owner, or hurt it?
// FAMILY metadata like the floor above, and for the same reason: the answer belongs to the kind, never to a
// value. True = INVERTED, i.e. a positive number is a DOWNSIDE (a cost, a penalty, a timer, a threshold at
// which you get LESS).
// ⚑ It exists because the PURE_TRAITS gate drops "the values whose alignment opposes the trait's", and
// alignment is NOT the sign: `maintenance.distance: -10%` is an UPSIDE and `lessYieldThreshold: +5` is a
// DOWNSIDE. A gate testing the sign alone keeps every inverted-family downside on a positive trait and drops
// its upsides -- backwards on exactly the families where being wrong is invisible, because the number stays
// plausible either way.
// ⚑ Grounded row-for-row in the legacy per-getter filters (`CvTraitInfo`, main): 48 members guard
// `isNegativeTrait() && x > 0` (NORMAL) and 19 guard `isNegativeTrait() && x < 0` (INVERTED). Every member a
// trait actually authors is covered below; a family absent from this table is NORMAL.
bool infoKindAlignmentInverted(ModifierFamily eFamily, int iKind);
// Is this address segment a deposit-target token (a §3.3 plural target, a keyed-target container, or a
// targeting-config block)? Target tokens carry data ids below them, never kind vocabulary.
bool infoIsTargetToken(const std::string& szSegment);
// FAMILY-SCOPED keyed-container tokens -- container spellings that are target tokens ONLY inside one family,
// because the same word is a MEMBER of another (combat `targets`/`unitTargets`/`defenders` are the §8 keyed
// targeting/immunity membership maps, while `defenders` is also the barbarians.world.defenders KIND). The
// address decode consults this beside the global table; the global table stays family-agnostic.
bool infoIsFamilyTargetToken(ModifierFamily eFamily, const std::string& szSegment);

// Load-time kind-coverage diagnostic (the Orwell bar): every member the vocabulary does not carry is recorded
// as "<familyKey>.<memberPath>" and surfaced by the reader's coverage summary -- flowed through, never dropped.
void infoNoteUnkindedMember(const std::string& szFamilyKey, const std::string& szMemberPath);
const std::set<std::string>& infoUnkindedMembers();
void infoResetKindDiag();

#endif // CV_INFO_KINDS_H
