//
//	CvInfoKinds -- the shared vocabulary's resolution tables (see the header). Family keys mirror the reader's
//	CJK_FAMILY_KEYS (CvJsonParse.cpp) one-to-one; the member tables transcribe the live family census
//	(`python Tools/Migration/family_census.py`) under the owner's enum-walk rulings (info-rebuild.md). A member
//	outside the vocabulary resolves to -1 and flows through the compile pass as an interned segment (surfaced by
//	the load-time kind-coverage diagnostic) -- it is never dropped and never silently minted a kind.
//

#include "CvGameCoreDLL.h"
#include "CvInfoKinds.h"
#include "Defines/CvEnums.h"   // YieldTypes / CommerceTypes -- the ruling-1 engine-enum kind axes

// ============================ the family key table (enum order) ============================
namespace
{
	const char* const INFO_FAMILY_KEYS[NUM_MODIFIER_FAMILIES] = {
		"air",                      // MODFAM_AIR
		"allowedSpecialists",       // MODFAM_ALLOWED_SPECIALISTS
		"anarchy",                  // MODFAM_ANARCHY
		"barbarians",               // MODFAM_BARBARIANS
		"bombard",                  // MODFAM_BOMBARD
		"buildRate",                // MODFAM_BUILD_RATE
		"capture",                  // MODFAM_CAPTURE
		"cargo",                    // MODFAM_CARGO
		"cityCapture",              // MODFAM_CITY_CAPTURE
		"collateral",               // MODFAM_COLLATERAL
		"combat",                   // MODFAM_COMBAT
		"commerce",                 // MODFAM_COMMERCE
		"conscript",                // MODFAM_CONSCRIPT
		"costs",                    // MODFAM_COSTS
		"culture",                  // MODFAM_CULTURE
		"cultureDistance",          // MODFAM_CULTURE_DISTANCE
		"defense",                  // MODFAM_DEFENSE
		"diplomacy",                // MODFAM_DIPLOMACY
		"domainMoves",              // MODFAM_DOMAIN_MOVES
		"durations",                // MODFAM_DURATIONS
		"espionage",                // MODFAM_ESPIONAGE
		"espionageDefense",         // MODFAM_ESPIONAGE_DEFENSE
		"eventChance",              // MODFAM_EVENT_CHANCE
		"experience",               // MODFAM_EXPERIENCE
		"extraYieldThreshold",      // MODFAM_EXTRA_YIELD_THRESHOLD
		"featureProduction",        // MODFAM_FEATURE_PRODUCTION
		"firstStrike",              // MODFAM_FIRST_STRIKE
		"food",                     // MODFAM_FOOD
		"foodKept",                 // MODFAM_FOOD_KEPT
		"freeSpecialists",          // MODFAM_FREE_SPECIALISTS
		"gold",                     // MODFAM_GOLD
		"goldenAge",                // MODFAM_GOLDEN_AGE
		"greatGeneralRate",         // MODFAM_GREAT_GENERAL_RATE
		"greatPeopleRate",          // MODFAM_GREAT_PEOPLE_RATE
		"growth",                   // MODFAM_GROWTH
		"happiness",                // MODFAM_HAPPINESS
		"heal",                     // MODFAM_HEAL
		"health",                   // MODFAM_HEALTH
		"hurry",                    // MODFAM_HURRY
		"hurryAnger",               // MODFAM_HURRY_ANGER
		"improvementUpgradeRate",   // MODFAM_IMPROVEMENT_UPGRADE_RATE
		"inflation",                // MODFAM_INFLATION
		"lessYieldThreshold",       // MODFAM_LESS_YIELD_THRESHOLD
		"maintenance",              // MODFAM_MAINTENANCE
		"missionYieldMultiplier",   // MODFAM_MISSION_YIELD_MULTIPLIER
		"movement",                 // MODFAM_MOVEMENT
		"occupationTime",           // MODFAM_OCCUPATION_TIME
		"odds",                     // MODFAM_ODDS
		"pillage",                  // MODFAM_PILLAGE
		"populationGrowthRate",     // MODFAM_POPULATION_GROWTH_RATE
		"production",               // MODFAM_PRODUCTION
		"range",                    // MODFAM_RANGE
		"religion",                 // MODFAM_RELIGION
		"research",                 // MODFAM_RESEARCH
		"researchRate",             // MODFAM_RESEARCH_RATE
		"revoltProtection",         // MODFAM_REVOLT_PROTECTION
		"revolution",               // MODFAM_REVOLUTION
		"spawnRate",                // MODFAM_SPAWN_RATE
		"speed",                    // MODFAM_SPEED
		"stateReligion",            // MODFAM_STATE_RELIGION
		"strength",                 // MODFAM_STRENGTH
		"survivor",                 // MODFAM_SURVIVOR
		"tradeMission",             // MODFAM_TRADE_MISSION
		"tradeRoutes",              // MODFAM_TRADE_ROUTES
		"underworld",               // MODFAM_UNDERWORLD
		"upkeep",                   // MODFAM_UPKEEP
		"vision",                   // MODFAM_VISION
		"warWeariness",             // MODFAM_WAR_WEARINESS
		"withdrawal",               // MODFAM_WITHDRAWAL
		"workRate",                 // MODFAM_WORK_RATE
		"",                         // MODFAM_PROPERTY -- the open plane; the PROPERTY_* type string IS the key
	};

	// The census scope-participation masks (enum order).
	const int INFO_FAMILY_SCOPES[NUM_MODIFIER_FAMILIES] = {
		INFO_SCOPE_BIT(CASC_SCOPE_UNIT) | INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE),                                     // air
		INFO_SCOPE_BIT(CASC_SCOPE_CITY),                                                                          // allowedSpecialists
		INFO_SCOPE_BIT(CASC_SCOPE_CITY),                                                                          // anarchy
		INFO_SCOPE_BIT(CASC_SCOPE_WORLD),                                                                         // barbarians
		INFO_SCOPE_BIT(CASC_SCOPE_UNIT),                                                                          // bombard
		INFO_SCOPE_BIT(CASC_SCOPE_SELF) | INFO_SCOPE_BIT(CASC_SCOPE_CITY) | INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE),    // buildRate
		INFO_SCOPE_BIT(CASC_SCOPE_UNIT) | INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE),                                      // capture
		INFO_SCOPE_BIT(CASC_SCOPE_UNIT) | INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE),                                      // cargo
		INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE),                                                                        // cityCapture
		INFO_SCOPE_BIT(CASC_SCOPE_UNIT),                                                                          // collateral
		INFO_SCOPE_BIT(CASC_SCOPE_UNIT) | INFO_SCOPE_BIT(CASC_SCOPE_CITY) | INFO_SCOPE_BIT(CASC_SCOPE_TEAM)
			| INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE) | INFO_SCOPE_BIT(CASC_SCOPE_WORLD),                               // combat
		INFO_SCOPE_BIT(CASC_SCOPE_CITY) | INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE) | INFO_SCOPE_BIT(CASC_SCOPE_PLOT),    // commerce
		INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE),                                                                        // conscript
		INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE) | INFO_SCOPE_BIT(CASC_SCOPE_WORLD) | INFO_SCOPE_BIT(CASC_SCOPE_UNIT),   // costs
		INFO_SCOPE_BIT(CASC_SCOPE_CITY) | INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE) | INFO_SCOPE_BIT(CASC_SCOPE_UNIT)
			| INFO_SCOPE_BIT(CASC_SCOPE_PLOT),                                                                    // culture
		INFO_SCOPE_BIT(CASC_SCOPE_PLOT),                                                                          // cultureDistance
		INFO_SCOPE_BIT(CASC_SCOPE_CITY) | INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE) | INFO_SCOPE_BIT(CASC_SCOPE_PLOT),    // defense
		INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE) | INFO_SCOPE_BIT(CASC_SCOPE_TEAM),                                      // diplomacy
		INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE),                                                                        // domainMoves
		INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE) | INFO_SCOPE_BIT(CASC_SCOPE_WORLD),                                     // durations
		INFO_SCOPE_BIT(CASC_SCOPE_CITY) | INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE) | INFO_SCOPE_BIT(CASC_SCOPE_UNIT),    // espionage
		INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE) | INFO_SCOPE_BIT(CASC_SCOPE_CITY),                                      // espionageDefense
		INFO_SCOPE_BIT(CASC_SCOPE_WORLD),                                                                         // eventChance
		INFO_SCOPE_BIT(CASC_SCOPE_CITY) | INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE) | INFO_SCOPE_BIT(CASC_SCOPE_UNIT),    // experience
		INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE),                                                                        // extraYieldThreshold
		INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE),                                                                        // featureProduction
		INFO_SCOPE_BIT(CASC_SCOPE_UNIT),                                                                          // firstStrike
		INFO_SCOPE_BIT(CASC_SCOPE_CITY) | INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE) | INFO_SCOPE_BIT(CASC_SCOPE_PLOT),    // food
		INFO_SCOPE_BIT(CASC_SCOPE_CITY),                                                                          // foodKept
		INFO_SCOPE_BIT(CASC_SCOPE_CITY) | INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE)
			| INFO_SCOPE_BIT(CASC_SCOPE_TEAM),                                                                    // freeSpecialists
		INFO_SCOPE_BIT(CASC_SCOPE_CITY) | INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE),                                      // gold
		INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE),                                                                        // goldenAge
		INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE) | INFO_SCOPE_BIT(CASC_SCOPE_CITY),                                      // greatGeneralRate
		INFO_SCOPE_BIT(CASC_SCOPE_CITY) | INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE) | INFO_SCOPE_BIT(CASC_SCOPE_WORLD),   // greatPeopleRate
		INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE) | INFO_SCOPE_BIT(CASC_SCOPE_WORLD),                                     // growth
		INFO_SCOPE_BIT(CASC_SCOPE_CITY) | INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE)
			| INFO_SCOPE_BIT(CASC_SCOPE_WORLD),                                                                   // happiness
		INFO_SCOPE_BIT(CASC_SCOPE_UNIT) | INFO_SCOPE_BIT(CASC_SCOPE_CITY),                                        // heal
		INFO_SCOPE_BIT(CASC_SCOPE_CITY) | INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE) | INFO_SCOPE_BIT(CASC_SCOPE_PLOT)
			| INFO_SCOPE_BIT(CASC_SCOPE_WORLD),                                                                   // health
		INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE),                                                                        // hurry
		INFO_SCOPE_BIT(CASC_SCOPE_CITY),                                                                          // hurryAnger
		INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE),                                                                        // improvementUpgradeRate
		INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE),                                                                        // inflation
		INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE),                                                                        // lessYieldThreshold
		INFO_SCOPE_BIT(CASC_SCOPE_CITY) | INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE),                                      // maintenance
		INFO_SCOPE_BIT(CASC_SCOPE_WORLD),                                                                         // missionYieldMultiplier
		INFO_SCOPE_BIT(CASC_SCOPE_UNIT) | INFO_SCOPE_BIT(CASC_SCOPE_PLOT),                                        // movement
		INFO_SCOPE_BIT(CASC_SCOPE_CITY),                                                                          // occupationTime
		0,                                                                                                        // odds (outcome-plane data, ruling 7)
		INFO_SCOPE_BIT(CASC_SCOPE_UNIT),                                                                          // pillage
		INFO_SCOPE_BIT(CASC_SCOPE_CITY),                                                                          // populationGrowthRate
		INFO_SCOPE_BIT(CASC_SCOPE_CITY) | INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE) | INFO_SCOPE_BIT(CASC_SCOPE_PLOT),    // production
		INFO_SCOPE_BIT(CASC_SCOPE_UNIT) | INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE),                                      // range
		INFO_SCOPE_BIT(CASC_SCOPE_CITY),                                                                          // religion
		INFO_SCOPE_BIT(CASC_SCOPE_CITY) | INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE),                                      // research
		INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE),                                                                        // researchRate
		INFO_SCOPE_BIT(CASC_SCOPE_UNIT),                                                                          // revoltProtection
		INFO_SCOPE_BIT(CASC_SCOPE_CITY) | INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE),                                      // revolution
		INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE),                                                                        // spawnRate
		INFO_SCOPE_BIT(CASC_SCOPE_WORLD),                                                                         // speed
		INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE),                                                                        // stateReligion
		INFO_SCOPE_BIT(CASC_SCOPE_UNIT),                                                                          // strength
		INFO_SCOPE_BIT(CASC_SCOPE_UNIT),                                                                          // survivor
		INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE),                                                                        // tradeMission
		INFO_SCOPE_BIT(CASC_SCOPE_CITY) | INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE) | INFO_SCOPE_BIT(CASC_SCOPE_WORLD),   // tradeRoutes
		INFO_SCOPE_BIT(CASC_SCOPE_CITY),                                                                          // underworld
		INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE) | INFO_SCOPE_BIT(CASC_SCOPE_UNIT),                                      // upkeep
		VISION_SCOPES,                                                                                            // vision
		INFO_SCOPE_BIT(CASC_SCOPE_CITY) | INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE),                                      // warWeariness
		INFO_SCOPE_BIT(CASC_SCOPE_UNIT),                                                                          // withdrawal
		INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE) | INFO_SCOPE_BIT(CASC_SCOPE_UNIT),                                      // workRate
		INFO_SCOPE_BIT(CASC_SCOPE_CITY) | INFO_SCOPE_BIT(CASC_SCOPE_PLOT) | INFO_SCOPE_BIT(CASC_SCOPE_EMPIRE),    // PROPERTY plane
	};

	// One member-spelling -> kind row; a family's table is a NULL-terminated array of these.
	struct InfoMemberRow
	{
		const char* member;
		int kind;
	};

	// The ruling-1 channel families share the ChannelKind pair.
	const InfoMemberRow MEMBERS_CHANNEL[] = { { "goldenAge", CHANNEL_GOLDEN_AGE }, { 0, 0 } };

	// COMMERCE alone adds the ruling-15 corporation source-component kind (legacy corporationRevenue --
	// authored `commerce.empire.corporation`, tech_stock_brokering); the other channel families never carry it.
	const InfoMemberRow MEMBERS_COMMERCE[] = {
		{ "goldenAge", CHANNEL_GOLDEN_AGE },
		{ "corporation", CHANNEL_CORPORATION },
		{ 0, 0 } };

	// ⛔ `insidiousness` / `investigation` are NOT espionage members (owner): they are the UNDERWORLD family --
	// the in-city criminal contest, a criminal's stealth against an investigator's catch. Espionage never
	// "becomes" insidiousness; the two mechanics merely ride some of the same units. MEMBERS_UNDERWORLD owns
	// both words, so a member name resolves to exactly one family.
	const InfoMemberRow MEMBERS_ESPIONAGE[] = {
		{ "goldenAge", ESPIONAGE_GOLDEN_AGE },
		{ 0, 0 } };

	const InfoMemberRow MEMBERS_DEFENSE[] = {
		{ "amount", DEFENSE_AMOUNT },
		{ "min", DEFENSE_MIN },
		{ "air", DEFENSE_AIR },
		{ "airDefense", DEFENSE_AIR },   // the city-scope spelling of the same concept; the scope axis separates
		{ "bombardDefense", DEFENSE_BOMBARD },
		{ "nukeDefense", DEFENSE_NUKE },
		{ "dynamicDefense", DEFENSE_DYNAMIC },
		{ "noEntryLevel", DEFENSE_NO_ENTRY_LEVEL },
		{ "riverDefensePenalty", DEFENSE_RIVER_PENALTY },
		{ "buildingDefenseRecovery", DEFENSE_BUILDING_RECOVERY },
		{ "cityDefenseRecovery", DEFENSE_CITY_RECOVERY },
		{ "adjacentDamage", DEFENSE_ADJACENT_DAMAGE },
		{ "counterDamage.damage", DEFENSE_COUNTER_DAMAGE },
		{ "counterDamage.chance", DEFENSE_COUNTER_DAMAGE_CHANCE },
		{ 0, 0 } };

	const InfoMemberRow MEMBERS_MAINTENANCE[] = {
		{ "all", MAINTENANCE_AMOUNT },   // the census alias for the scope-wide amount
		{ "numCities", MAINTENANCE_NUM_CITIES },
		{ "distance", MAINTENANCE_DISTANCE },
		{ "corporation", MAINTENANCE_CORPORATION },
		{ "colony", MAINTENANCE_COLONY },
		{ 0, 0 } };

	const InfoMemberRow MEMBERS_COSTS[] = {
		{ "train", COSTS_TRAIN },
		{ "construct", COSTS_CONSTRUCT },
		{ "create", COSTS_CREATE },
		{ "build", COSTS_BUILD },
		{ "research", COSTS_RESEARCH },
		{ "improvementUpgrade", COSTS_IMPROVEMENT_UPGRADE },
		{ "researchCutBelowEra", COSTS_RESEARCH_CUT_BELOW_ERA },
		{ "hurry", COSTS_HURRY },
		{ "upgrade", COSTS_UPGRADE },
		{ 0, 0 } };

	const InfoMemberRow MEMBERS_COMBAT[] = {
		{ "attack", COMBAT_ATTACK },
		{ "defense", COMBAT_DEFENSE },
		{ "cityAttack", COMBAT_CITY_ATTACK },
		{ "cityDefense", COMBAT_CITY_DEFENSE },
		{ "hillsAttack", COMBAT_HILLS_ATTACK },
		{ "hillsDefense", COMBAT_HILLS_DEFENSE },
		{ "stealth", COMBAT_STEALTH },
		{ "stealthStrikes", COMBAT_STEALTH_STRIKES },
		{ "lunge", COMBAT_LUNGE },
		{ "unnerve", COMBAT_UNNERVE },
		{ "enclose", COMBAT_ENCLOSE },
		{ "taunt", COMBAT_TAUNT },
		{ "dynamicDefense", COMBAT_DYNAMIC_DEFENSE },
		{ "damageModifier", COMBAT_DAMAGE_MODIFIER },
		{ "breakdownChance", COMBAT_BREAKDOWN_CHANCE },
		{ "breakdownDamage", COMBAT_BREAKDOWN_DAMAGE },
		{ "combatLimit", COMBAT_LIMIT },
		{ "kamikaze", COMBAT_KAMIKAZE },
		{ "religious", COMBAT_RELIGIOUS },
		{ "vsBarbs", COMBAT_VS_BARBS },
		{ "animal", COMBAT_ANIMAL },
		{ "barbarian", COMBAT_BARBARIAN },
		{ "freeWinsVsBarbs", COMBAT_FREE_WINS_VS_BARBS },
		{ 0, 0 } };

	const InfoMemberRow MEMBERS_HEAL[] = {
		{ "heal", HEAL_RATE },
		{ "support", HEAL_SUPPORT },
		{ "adjacentHeal", HEAL_ADJACENT },
		{ "adjacentTile", HEAL_ADJACENT_TILE },
		{ "sameTile", HEAL_SAME_TILE },
		{ "selfModifier", HEAL_SELF_MODIFIER },
		{ "enemy", HEAL_ENEMY_TERRITORY },
		{ "friendly", HEAL_FRIENDLY_TERRITORY },
		{ "neutral", HEAL_NEUTRAL_TERRITORY },
		{ "victory", HEAL_VICTORY },
		{ "victoryStack", HEAL_VICTORY_STACK },
		{ "victoryAdjacent", HEAL_VICTORY_ADJACENT },
		{ 0, 0 } };

	const InfoMemberRow MEMBERS_UPKEEP[] = {
		{ "civic", UPKEEP_CIVIC },
		{ "freeMilitary", UPKEEP_FREE_MILITARY },
		{ "freeCivilian", UPKEEP_FREE_CIVILIAN },
		{ "unitMilitary", UPKEEP_UNIT_MILITARY },
		{ "unitCivilian", UPKEEP_UNIT_CIVILIAN },
		{ "unit", UPKEEP_UNIT },
		{ "inflation", UPKEEP_INFLATION },
		{ "supply", UPKEEP_SUPPLY },
		{ "extra", UPKEEP_EXTRA },
		{ 0, 0 } };

	const InfoMemberRow MEMBERS_TRADE_ROUTES[] = {
		{ "modifier", TRADE_ROUTE_MODIFIER },   // channel-agnostic route-PROFIT % (totalTradeModifier stage)
		{ "max", TRADE_ROUTE_MAX },
		// ruling 27: the channel axis on `modifier` (YieldTypes reuse) -- authored
		// tradeRoutes.<scope>.modifier.<channel>.<unit>, decoded as the dotted member path
		{ "modifier.food", TRADE_ROUTE_MODIFIER_FOOD },
		{ "modifier.production", TRADE_ROUTE_MODIFIER_PRODUCTION },
		{ "modifier.commerce", TRADE_ROUTE_MODIFIER_COMMERCE },
		{ 0, 0 } };

	const InfoMemberRow MEMBERS_MOVEMENT[] = {
		{ "moves", MOVEMENT_MOVES },
		{ "moveDiscount", MOVEMENT_MOVE_DISCOUNT },
		{ "dropRange", MOVEMENT_DROP_RANGE },
		{ 0, 0 } };

	const InfoMemberRow MEMBERS_VISION[] = {
		{ "elevation", VISION_ELEVATION },
		{ "obstruction", VISION_OBSTRUCTION },
		{ "concealment", VISION_CONCEALMENT },
		{ "detection", VISION_DETECTION },
		{ 0, 0 } };

	const InfoMemberRow MEMBERS_AIR[] = {
		{ "intercept", AIR_INTERCEPT },
		{ "evasion", AIR_EVASION },
		{ "range", AIR_RANGE },
		{ "nukeRange", AIR_NUKE_RANGE },
		{ 0, 0 } };

	const InfoMemberRow MEMBERS_CAPTURE[] = {
		{ "probability", CAPTURE_PROBABILITY },
		{ "resistance", CAPTURE_RESISTANCE },
		{ 0, 0 } };

	const InfoMemberRow MEMBERS_CARGO[] = {
		{ "space", CARGO_SPACE },
		{ "size", CARGO_SIZE },
		{ "missileCargo", CARGO_MISSILE },
		{ "navalCargo", CARGO_NAVAL },
		{ 0, 0 } };

	const InfoMemberRow MEMBERS_COLLATERAL[] = {
		{ "damage", COLLATERAL_DAMAGE },
		{ "limit", COLLATERAL_LIMIT },
		{ "maxUnits", COLLATERAL_MAX_UNITS },
		{ "protection", COLLATERAL_PROTECTION },
		{ 0, 0 } };

	const InfoMemberRow MEMBERS_BOMBARD[] = {
		{ "rate", BOMBARD_RATE },
		{ "airBombRate", BOMBARD_AIR_BOMB_RATE },
		{ 0, 0 } };

	const InfoMemberRow MEMBERS_EXPERIENCE[] = {
		{ "inBorder", EXPERIENCE_IN_BORDER },
		{ "levelModifier", EXPERIENCE_LEVEL_MODIFIER },
		{ 0, 0 } };

	const InfoMemberRow MEMBERS_REVOLUTION[] = {
		{ "local", REVOLUTION_LOCAL },
		{ "national", REVOLUTION_NATIONAL },
		{ "distanceModifier", REVOLUTION_DISTANCE_MODIFIER },
		{ "distanceMod", REVOLUTION_DISTANCE_MODIFIER },   // the census's short spelling of the same concept
		{ "holyCityGood", REVOLUTION_HOLY_CITY_GOOD },
		{ "holyCityBad", REVOLUTION_HOLY_CITY_BAD },
		{ "democracyLevel", REVOLUTION_DEMOCRACY_LEVEL },
		{ "goodReligionMod", REVOLUTION_GOOD_RELIGION },
		{ "badReligionMod", REVOLUTION_BAD_RELIGION },
		{ "nationalityMod", REVOLUTION_NATIONALITY },
		{ "religiousFreedom", REVOLUTION_RELIGIOUS_FREEDOM },
		{ "environmentalProtection", REVOLUTION_ENVIRONMENTAL_PROTECTION },
		{ "violentMod", REVOLUTION_VIOLENT },
		{ "laborFreedom", REVOLUTION_LABOR_FREEDOM },
		{ 0, 0 } };

	const InfoMemberRow MEMBERS_STATE_RELIGION[] = {
		{ "greatPeopleRate", STATE_RELIGION_GREAT_PEOPLE_RATE },
		{ "happiness", STATE_RELIGION_HAPPINESS },
		{ "holyCityXP", STATE_RELIGION_HOLY_CITY_XP },
		{ "buildingProduction", STATE_RELIGION_BUILDING_PRODUCTION },
		{ "unitProduction", STATE_RELIGION_UNIT_PRODUCTION },
		{ "spreadProbability", STATE_RELIGION_SPREAD_PROBABILITY },
		{ "nonStateSpreadProbability", STATE_RELIGION_NON_STATE_SPREAD_PROBABILITY },
		{ "freeExperience", STATE_RELIGION_FREE_EXPERIENCE },
		{ 0, 0 } };

	const InfoMemberRow MEMBERS_DIPLOMACY[] = {
		{ "attitude", DIPLOMACY_ATTITUDE },
		{ "attitudeShare", DIPLOMACY_ATTITUDE_SHARE },
		{ "warWeariness", DIPLOMACY_WAR_WEARINESS },
		{ "enemyWarWeariness", DIPLOMACY_ENEMY_WAR_WEARINESS },
		{ "declareWar", DIPLOMACY_DECLARE_WAR },
		{ "noTechTrade", DIPLOMACY_NO_TECH_TRADE },
		{ "techTradeKnown", DIPLOMACY_TECH_TRADE_KNOWN },
		{ "techShare", DIPLOMACY_TECH_SHARE },
		{ 0, 0 } };

	const InfoMemberRow MEMBERS_DURATIONS[] = {
		{ "civicAnarchy", DURATIONS_CIVIC_ANARCHY },
		{ "religiousAnarchy", DURATIONS_RELIGIOUS_ANARCHY },
		{ "anger", DURATIONS_ANGER },
		{ 0, 0 } };

	const InfoMemberRow MEMBERS_BUILD_RATE[] = {
		{ "military", BUILD_RATE_MILITARY },
		{ "space", BUILD_RATE_SPACE },
		{ "worldWonder", BUILD_RATE_WORLD_WONDER },
		{ "teamWonder", BUILD_RATE_TEAM_WONDER },
		{ "nationalWonder", BUILD_RATE_NATIONAL_WONDER },
		{ "specialBuildings", BUILD_RATE_SPECIAL_BUILDINGS },
		{ 0, 0 } };

	const InfoMemberRow MEMBERS_BARBARIANS[] = {
		{ "animalAttackProb", BARBARIANS_ANIMAL_ATTACK_PROB },
		{ "waterTilesPerUnit", BARBARIANS_WATER_TILES_PER_UNIT },
		{ "tilesPerCity", BARBARIANS_TILES_PER_CITY },
		{ "cityCreationTurns", BARBARIANS_CITY_CREATION_TURNS },
		{ "cityCreationProb", BARBARIANS_CITY_CREATION_PROB },
		{ "defenders", BARBARIANS_DEFENDERS },
		{ 0, 0 } };

	const InfoMemberRow MEMBERS_UNDERWORLD[] = {
		{ "insidiousness", UNDERWORLD_INSIDIOUSNESS },
		{ "investigation", UNDERWORLD_INVESTIGATION },
		{ 0, 0 } };

	// The 1-2-kind straggler families (their public names are the InfoScalar entries).
	const InfoMemberRow MEMBERS_WAR_WEARINESS[] = { { "enemy", 1 }, { 0, 0 } };
	const InfoMemberRow MEMBERS_HURRY[] = { { "anger", 1 }, { "inflation", 2 }, { 0, 0 } };
	const InfoMemberRow MEMBERS_GREAT_GENERAL_RATE[] = { { "domestic", 1 }, { 0, 0 } };
	const InfoMemberRow MEMBERS_FIRST_STRIKE[] = { { "strikes", 1 }, { "chance", 2 }, { 0, 0 } };
	const InfoMemberRow MEMBERS_SPAWN_RATE[] = { { "npcPeace", 1 }, { 0, 0 } };
	const InfoMemberRow MEMBERS_WORK_RATE[] = { { "hills", 1 }, { 0, 0 } };

	// The per-family member table (enum order; NULL = no member vocabulary -- memberless-only or ruled empty).
	// The ruling-1 engine-enum axes are resolved in infoResolveKind below, not table rows.
	const InfoMemberRow* const INFO_FAMILY_MEMBERS[NUM_MODIFIER_FAMILIES] = {
		MEMBERS_AIR,                // air
		0,                          // allowedSpecialists (count-by-type leaves)
		0,                          // anarchy
		MEMBERS_BARBARIANS,         // barbarians
		MEMBERS_BOMBARD,            // bombard
		MEMBERS_BUILD_RATE,         // buildRate
		MEMBERS_CAPTURE,            // capture
		MEMBERS_CARGO,              // cargo
		0,                          // cityCapture (trigger-plane data, ruling 16 -- no kinds minted)
		MEMBERS_COLLATERAL,         // collateral
		MEMBERS_COMBAT,             // combat
		MEMBERS_COMMERCE,           // commerce (ChannelKind pair + the ruling-15 corporation kind)
		0,                          // conscript
		MEMBERS_COSTS,              // costs
		MEMBERS_CHANNEL,            // culture
		0,                          // cultureDistance
		MEMBERS_DEFENSE,            // defense
		MEMBERS_DIPLOMACY,          // diplomacy
		0,                          // domainMoves (DomainTypes keys ride the domains container)
		MEMBERS_DURATIONS,          // durations
		MEMBERS_ESPIONAGE,          // espionage
		0,                          // espionageDefense
		0,                          // eventChance
		MEMBERS_EXPERIENCE,         // experience
		0,                          // extraYieldThreshold (YieldTypes axis, infoResolveKind)
		0,                          // featureProduction
		MEMBERS_FIRST_STRIKE,       // firstStrike
		MEMBERS_CHANNEL,            // food
		0,                          // foodKept
		0,                          // freeSpecialists (count-by-type leaves)
		MEMBERS_CHANNEL,            // gold
		0,                          // goldenAge
		MEMBERS_GREAT_GENERAL_RATE, // greatGeneralRate
		0,                          // greatPeopleRate (UNIT_* keys are data ids)
		0,                          // growth
		0,                          // happiness (wellbeing mints ZERO kinds, ruling 12)
		MEMBERS_HEAL,               // heal
		0,                          // health (wellbeing mints ZERO kinds, ruling 12)
		MEMBERS_HURRY,              // hurry
		0,                          // hurryAnger
		0,                          // improvementUpgradeRate
		0,                          // inflation
		0,                          // lessYieldThreshold (YieldTypes axis, infoResolveKind)
		MEMBERS_MAINTENANCE,        // maintenance
		0,                          // missionYieldMultiplier
		MEMBERS_MOVEMENT,           // movement
		0,                          // occupationTime
		0,                          // odds (outcome-plane data, ruling 7)
		0,                          // pillage
		0,                          // populationGrowthRate
		MEMBERS_CHANNEL,            // production
		0,                          // range
		0,                          // religion (RELIGION_* keys are data ids)
		MEMBERS_CHANNEL,            // research
		0,                          // researchRate (TECH_* keys are data ids)
		0,                          // revoltProtection
		MEMBERS_REVOLUTION,         // revolution
		MEMBERS_SPAWN_RATE,         // spawnRate
		0,                          // speed
		MEMBERS_STATE_RELIGION,     // stateReligion
		0,                          // strength (the unit's BASE value only, ruling 5)
		0,                          // survivor
		0,                          // tradeMission
		MEMBERS_TRADE_ROUTES,       // tradeRoutes
		MEMBERS_UNDERWORLD,         // underworld
		MEMBERS_UPKEEP,             // upkeep
		MEMBERS_VISION,             // vision
		MEMBERS_WAR_WEARINESS,      // warWeariness
		0,                          // withdrawal
		MEMBERS_WORK_RATE,          // workRate
		0,                          // PROPERTY plane (memberless flats/percents only)
	};

	// The §3.3 plural targets + the keyed-target containers -- an address segment carrying data ids below it,
	// never kind vocabulary.
	const char* const INFO_TARGET_TOKENS[] = {
		"plots", "units", "cities", "areas", "empires",
		"improvements", "terrains", "features", "bonus", "bonuses", "buildings", "domains", "unitCombats",
		"specialists", "routes", "civics", "techs", "builds",
		"unitCombat", "flankingUnit", "vsUnit", "feature", "terrain", "domain",
		"any",   // the untyped free-specialist slot bucket (modifier.md §6 count key)
		0
	};

	// FAMILY-SCOPED keyed-container tokens (see the header): the combat §8 targeting/immunity membership maps
	// (json §8: keyed targeting/immunity -> the combat family) -- `targets`/`unitTargets` (UNITCOMBAT_*/UNIT_*
	// bool maps, 452/83 authored) + `defenders` (25 authored). NOT in the global table: `defenders` is also the
	// barbarians.world.defenders KIND, so the container reading is combat-only.
	struct InfoFamilyTokenRow
	{
		ModifierFamily family;
		const char* segment;
	};
	const InfoFamilyTokenRow INFO_FAMILY_TARGET_TOKENS[] = {
		{ MODFAM_COMBAT, "targets" },
		{ MODFAM_COMBAT, "unitTargets" },
		{ MODFAM_COMBAT, "defenders" },
		{ MODFAM_COMBAT, "flanking" },
		{ MODFAM_NONE, 0 }
	};

	// The straggler-scalar slot table (InfoScalar order).
	struct InfoScalarRow
	{
		ModifierFamily family;
		int kind;
	};
	const InfoScalarRow INFO_SCALAR_SLOTS[NUM_INFO_SCALARS] = {
		{ MODFAM_ANARCHY, 0 },                    // SCALAR_ANARCHY
		{ MODFAM_CONSCRIPT, 0 },                  // SCALAR_CONSCRIPT
		{ MODFAM_CULTURE_DISTANCE, 0 },           // SCALAR_CULTURE_DISTANCE
		{ MODFAM_ESPIONAGE_DEFENSE, 0 },          // SCALAR_ESPIONAGE_DEFENSE
		{ MODFAM_EVENT_CHANCE, 0 },               // SCALAR_EVENT_CHANCE
		{ MODFAM_FEATURE_PRODUCTION, 0 },         // SCALAR_FEATURE_PRODUCTION
		{ MODFAM_FIRST_STRIKE, 2 },               // SCALAR_FIRST_STRIKE_CHANCES (the `chance` member row; slot 0 is unauthored)
		{ MODFAM_FIRST_STRIKE, 1 },               // SCALAR_FIRST_STRIKES
		{ MODFAM_FOOD_KEPT, 0 },                  // SCALAR_FOOD_KEPT
		{ MODFAM_GOLDEN_AGE, 0 },                 // SCALAR_GOLDEN_AGE_LENGTH
		{ MODFAM_GREAT_GENERAL_RATE, 0 },         // SCALAR_GREAT_GENERAL_RATE
		{ MODFAM_GREAT_GENERAL_RATE, 1 },         // SCALAR_GREAT_GENERAL_RATE_DOMESTIC
		{ MODFAM_GREAT_PEOPLE_RATE, 0 },          // SCALAR_GREAT_PEOPLE_RATE
		{ MODFAM_GROWTH, 0 },                     // SCALAR_GROWTH
		{ MODFAM_HURRY, 1 },                      // SCALAR_HURRY_ANGER
		{ MODFAM_HURRY, 2 },                      // SCALAR_HURRY_INFLATION
		{ MODFAM_HURRY_ANGER, 0 },                // SCALAR_HURRY_ANGER_MODIFIER
		{ MODFAM_IMPROVEMENT_UPGRADE_RATE, 0 },   // SCALAR_IMPROVEMENT_UPGRADE_RATE
		{ MODFAM_INFLATION, 0 },                  // SCALAR_INFLATION
		{ MODFAM_MISSION_YIELD_MULTIPLIER, 0 },   // SCALAR_MISSION_YIELD_MULTIPLIER
		{ MODFAM_OCCUPATION_TIME, 0 },            // SCALAR_OCCUPATION_TIME
		{ MODFAM_PILLAGE, 0 },                    // SCALAR_PILLAGE
		{ MODFAM_POPULATION_GROWTH_RATE, 0 },     // SCALAR_POPULATION_GROWTH_RATE
		{ MODFAM_RANGE, 0 },                      // SCALAR_RANGE
		{ MODFAM_RESEARCH_RATE, 0 },              // SCALAR_RESEARCH_RATE
		{ MODFAM_REVOLT_PROTECTION, 0 },          // SCALAR_REVOLT_PROTECTION
		{ MODFAM_SPAWN_RATE, 1 },                 // SCALAR_SPAWN_RATE_NPC_PEACE
		{ MODFAM_SPEED, 0 },                      // SCALAR_SPEED
		{ MODFAM_STRENGTH, 0 },                   // SCALAR_STRENGTH
		{ MODFAM_SURVIVOR, 0 },                   // SCALAR_SURVIVOR
		{ MODFAM_TRADE_MISSION, 0 },              // SCALAR_TRADE_MISSION
		{ MODFAM_WAR_WEARINESS, 0 },              // SCALAR_WAR_WEARINESS
		{ MODFAM_WAR_WEARINESS, 1 },              // SCALAR_ENEMY_WAR_WEARINESS
		{ MODFAM_WITHDRAWAL, 0 },                 // SCALAR_WITHDRAWAL
		{ MODFAM_WORK_RATE, 0 },                  // SCALAR_WORK_RATE
		{ MODFAM_WORK_RATE, 1 },                  // SCALAR_WORK_RATE_HILLS
	};

	// The load-time kind-coverage diagnostic accumulator (bounded like the CvJsonParse diagnostics).
	std::set<std::string> s_unkindedMembers;
	const size_t INFO_KIND_DIAG_MAX = 4096;
}

ModifierFamily infoFamilyFromKey(const std::string& szKey)
{
	if (szKey.compare(0, 9, "PROPERTY_") == 0)
	{
		return MODFAM_PROPERTY;
	}
	for (int i = 0; i < (int)MODFAM_PROPERTY; ++i)
	{
		if (szKey == INFO_FAMILY_KEYS[i])
		{
			return (ModifierFamily)i;
		}
	}
	return MODFAM_NONE;
}

const char* infoFamilyKey(ModifierFamily eFamily)
{
	if (eFamily <= MODFAM_NONE || eFamily >= NUM_MODIFIER_FAMILIES || eFamily == MODFAM_PROPERTY)
	{
		return 0;
	}
	return INFO_FAMILY_KEYS[(int)eFamily];
}

int infoFamilyScopeMask(ModifierFamily eFamily)
{
	if (eFamily <= MODFAM_NONE || eFamily >= NUM_MODIFIER_FAMILIES)
	{
		return 0;
	}
	return INFO_FAMILY_SCOPES[(int)eFamily];
}

int infoResolveKind(ModifierFamily eFamily, const std::string& szMemberPath)
{
	if (eFamily <= MODFAM_NONE || eFamily >= NUM_MODIFIER_FAMILIES)
	{
		return -1;
	}
	// the ruling-1 engine-enum kind axes: the member spelling IS the engine channel
	if (eFamily == MODFAM_EXTRA_YIELD_THRESHOLD || eFamily == MODFAM_LESS_YIELD_THRESHOLD)
	{
		if (szMemberPath == "food")        return (int)YIELD_FOOD;
		if (szMemberPath == "production")  return (int)YIELD_PRODUCTION;
		if (szMemberPath == "commerce")    return (int)YIELD_COMMERCE;
		return -1;
	}
	const InfoMemberRow* pRow = INFO_FAMILY_MEMBERS[(int)eFamily];
	if (pRow == 0)
	{
		return -1;
	}
	for (; pRow->member != 0; ++pRow)
	{
		if (szMemberPath == pRow->member)
		{
			return pRow->kind;
		}
	}
	return -1;
}

int infoFamilyYield(ModifierFamily eFamily)
{
	if (eFamily == MODFAM_FOOD)        return (int)YIELD_FOOD;
	if (eFamily == MODFAM_PRODUCTION)  return (int)YIELD_PRODUCTION;
	if (eFamily == MODFAM_COMMERCE)    return (int)YIELD_COMMERCE;
	return -1;
}

int infoFamilyCommerce(ModifierFamily eFamily)
{
	if (eFamily == MODFAM_GOLD)       return (int)COMMERCE_GOLD;
	if (eFamily == MODFAM_RESEARCH)   return (int)COMMERCE_RESEARCH;
	if (eFamily == MODFAM_CULTURE)    return (int)COMMERCE_CULTURE;
	if (eFamily == MODFAM_ESPIONAGE)  return (int)COMMERCE_ESPIONAGE;
	return -1;
}

ModifierFamily infoYieldFamily(int iYield)
{
	if (iYield == (int)YIELD_FOOD)        return MODFAM_FOOD;
	if (iYield == (int)YIELD_PRODUCTION)  return MODFAM_PRODUCTION;
	if (iYield == (int)YIELD_COMMERCE)    return MODFAM_COMMERCE;
	return MODFAM_NONE;
}

ModifierFamily infoCommerceFamily(int iCommerce)
{
	if (iCommerce == (int)COMMERCE_GOLD)      return MODFAM_GOLD;
	if (iCommerce == (int)COMMERCE_RESEARCH)  return MODFAM_RESEARCH;
	if (iCommerce == (int)COMMERCE_CULTURE)   return MODFAM_CULTURE;
	if (iCommerce == (int)COMMERCE_ESPIONAGE) return MODFAM_ESPIONAGE;
	return MODFAM_NONE;
}

// The wellbeing channel's AUTHORED family (modifier.md §2b): the opposing channels share one authored family;
// the deposit's SIGN routes between them at fill/valuation (never a storage shape).
ModifierFamily infoWellbeingFamily(WellbeingChannel eChannel)
{
	switch (eChannel)
	{
	case WELLBEING_HAPPINESS:
	case WELLBEING_ANGER:
		return MODFAM_HAPPINESS;
	case WELLBEING_HEALTH:
	case WELLBEING_UNHEALTH:
		return MODFAM_HEALTH;
	default:
		return MODFAM_NONE;
	}
}

void infoScalarSlot(InfoScalar eScalar, ModifierFamily& eFamilyOut, int& iKindOut)
{
	if (eScalar < 0 || eScalar >= NUM_INFO_SCALARS)
	{
		eFamilyOut = MODFAM_NONE;
		iKindOut = -1;
		return;
	}
	eFamilyOut = INFO_SCALAR_SLOTS[(int)eScalar].family;
	iKindOut = INFO_SCALAR_SLOTS[(int)eScalar].kind;
}

CvCascUnit infoDefenseUnit(DefenseKind eKind, CvCascScope eScope)
{
	switch (eKind)
	{
	// ⚖ THE WHOLE FAMILY IS PERCENT (owner): defense is technically a flat additive sum, but it is APPLIED as a
	// percentage -- it raises a defending unit's combat by that percentage -- and it never carries decimals, so
	// the ×100 an amount would buy is worth nothing here. Uniformity is the requirement rather than the label:
	// the UNIT plane already authors every defense member percent (144 unit cityDefense, 41 dynamicDefense, …),
	// and `dynamicDefenseTotal` sums the CITY value into the UNIT one, so a single flat member in the family is a
	// live ×100 across that seam. One unit, no mixing sites, no compensating constants.
	case DEFENSE_AIR:
		// the SCOPE-SPLIT kind (reconciliation item 7): plot = the FLAT rolled magnitude (101 improvement
		// `air` flats; the opposed getSorenRandNum(airBombCurrRate) vs getSorenRandNum(defense) dice,
		// CvUnit.cpp:7126) -- never a percent, never a compensating multiplier; city = the PERCENT air-damage
		// modifier (17 building `airDefense` percents)
		return (eScope == CASC_SCOPE_PLOT) ? CASC_UNIT_FLAT : CASC_UNIT_PERCENT;
	default:
		return CASC_UNIT_PERCENT;
	}
}

// The scope-blind transitional overload (see the header): answers with the CITY plane -- the dominant consumer
// class and the pre-split behaviour everywhere except the plot AIR read, whose wave-B call site moves to the
// scope-aware overload (reported follow-up).
CvCascUnit infoDefenseUnit(DefenseKind eKind)
{
	return infoDefenseUnit(eKind, CASC_SCOPE_CITY);
}

// The canonical AUTHORED unit of any (family, kind, scope) slot (see the header). Grounded in the live census
// (family_census.py + the per-(scope, member, unit) refinement over Assets/Data): each row states what the data
// actually authors on that (kind, scope). The engine-enum channel families carry both units on one kind and
// keep the split in the getter NAME -- they never route through here. Dual slots (both units authored at ONE
// (kind, scope)) answer the DOMINANT plane and are called out inline -- the minority plane reads through the
// name-split getters.
CvCascUnit infoKindUnit(ModifierFamily eFamily, int iKind, CvCascScope eScope)
{
	switch (eFamily)
	{
	case MODFAM_DEFENSE:
		return infoDefenseUnit((DefenseKind)iKind, eScope);
	case MODFAM_COSTS:
	case MODFAM_DURATIONS:
	case MODFAM_BUILD_RATE:
	// the percent straggler families (census: every authored kind is a percent modifier -- greatGeneralRate 218,
	// hurry 63, spawnRate.npcPeace 6, warWeariness 108, workRate 990, anarchy 36, foodKept 21, goldenAge 83,
	// growth 75, improvementUpgradeRate 370, inflation 14, missionYieldMultiplier 9, occupationTime 11,
	// populationGrowthRate 40, researchRate 3275, revoltProtection 92, speed 9, survivor 1, tradeMission 1,
	// withdrawal 933, featureProduction 2, hurryAnger 13; their getScalar callers pass the unit explicitly,
	// this table is the one documentation-truth home)
	case MODFAM_GREAT_GENERAL_RATE:
	case MODFAM_HURRY:
	case MODFAM_SPAWN_RATE:
	case MODFAM_WAR_WEARINESS:
	case MODFAM_WORK_RATE:
	case MODFAM_ANARCHY:
	case MODFAM_FOOD_KEPT:
	case MODFAM_GOLDEN_AGE:
	case MODFAM_GROWTH:
	case MODFAM_IMPROVEMENT_UPGRADE_RATE:
	case MODFAM_INFLATION:
	case MODFAM_MISSION_YIELD_MULTIPLIER:
	case MODFAM_OCCUPATION_TIME:
	case MODFAM_POPULATION_GROWTH_RATE:
	case MODFAM_RESEARCH_RATE:
	case MODFAM_REVOLT_PROTECTION:
	case MODFAM_SPEED:
	case MODFAM_SURVIVOR:
	case MODFAM_TRADE_MISSION:
	case MODFAM_WITHDRAWAL:
	case MODFAM_FEATURE_PRODUCTION:
	case MODFAM_HURRY_ANGER:
		return CASC_UNIT_PERCENT;
	case MODFAM_ESPIONAGE_DEFENSE:
		// SCOPE-SPLIT (census): city = the building flat amount (61); empire = the trait percent modifier (78)
		return (eScope == CASC_SCOPE_CITY) ? CASC_UNIT_FLAT : CASC_UNIT_PERCENT;
	case MODFAM_MAINTENANCE:
		// corporation is SCOPE-SPLIT (city flat 23 -- the corp's own per-city gold amount; empire percent 120 --
		// the civic corp-maintenance modifier). The scope-wide amount at city is DUAL (flat 597 building gold
		// amounts / percent 125 city modifiers) -- the table answers the dominant FLAT plane; empire is percent.
		if (iKind == (int)MAINTENANCE_CORPORATION || iKind == (int)MAINTENANCE_AMOUNT)
		{
			return (eScope == CASC_SCOPE_CITY) ? CASC_UNIT_FLAT : CASC_UNIT_PERCENT;
		}
		return CASC_UNIT_PERCENT;
	case MODFAM_CAPTURE:
		// SCOPE-SPLIT (reconciliation item 1): the unit plane authors FLAT percentage-point chances
		// (probability 240 + resistance 188 = 428 entries); the empire plane authors PERCENT modifiers (158).
		return (eScope == CASC_SCOPE_UNIT) ? CASC_UNIT_FLAT : CASC_UNIT_PERCENT;
	case MODFAM_COMBAT:
		// the census flat-authoring kinds (reconciliation item 1); everything else is a percent modifier.
		switch (iKind)
		{
		case COMBAT_AMOUNT:            // memberless: DUAL at unit scope (flat 13 strengthChange / percent 167
		                               // combat%) -- the name-split getters carry both planes; dominant-plane
		                               // answer here is FLAT (the strengthChange-as-AMOUNT authoring class)
		case COMBAT_STEALTH_STRIKES:   // flat strike count (111)
		case COMBAT_BREAKDOWN_CHANCE:  // flat chance points (17)
		case COMBAT_BREAKDOWN_DAMAGE:  // flat damage (16)
		case COMBAT_TAUNT:             // flat (11)
		case COMBAT_FREE_WINS_VS_BARBS: // flat win count (8, handicap empire scope)
			return CASC_UNIT_FLAT;
		default:
			return CASC_UNIT_PERCENT;
		}
	case MODFAM_UNDERWORLD:
	case MODFAM_CARGO:
		return CASC_UNIT_FLAT;
	case MODFAM_HEAL:
		// selfModifier is the one percent kind (30); every other heal kind is a flat amount
		return (iKind == (int)HEAL_SELF_MODIFIER) ? CASC_UNIT_PERCENT : CASC_UNIT_FLAT;
	case MODFAM_COLLATERAL:
		// damage (125) / protection (21) are percent modifiers; limit / maxUnits (231 each) are flat counts
		return (iKind == (int)COLLATERAL_DAMAGE || iKind == (int)COLLATERAL_PROTECTION) ? CASC_UNIT_PERCENT : CASC_UNIT_FLAT;
	case MODFAM_BOMBARD:
		// rate is a percent modifier (143); airBombRate is a flat rate (33)
		return (iKind == (int)BOMBARD_RATE) ? CASC_UNIT_PERCENT : CASC_UNIT_FLAT;
	case MODFAM_AIR:
		// intercept/evasion are percent modifiers; the range kinds are flat counts (census: trait air.range flat)
		return (iKind == (int)AIR_RANGE || iKind == (int)AIR_NUKE_RANGE) ? CASC_UNIT_FLAT : CASC_UNIT_PERCENT;
	case MODFAM_EXPERIENCE:
		// the scope-wide amount is SCOPE-SPLIT (reconciliation item 1): unit = the PERCENT XP-gain modifier
		// (42, promotions); city/empire = flat free XP (7 + 61). NB empire is DUAL (percent 43 -- the trait
		// XP-gain modifier plane) -- dominant-plane answer FLAT; the percent plane needs the name-split getter
		// on the consuming types (reported follow-up). inBorder/levelModifier are percent modifiers.
		if (iKind == (int)EXPERIENCE_AMOUNT)
		{
			return (eScope == CASC_SCOPE_UNIT) ? CASC_UNIT_PERCENT : CASC_UNIT_FLAT;
		}
		return CASC_UNIT_PERCENT;
	case MODFAM_DIPLOMACY:
		// attitude values + techShare (1, project_encyclopedia) are flat deltas; the rest are percent modifiers
		return (iKind == (int)DIPLOMACY_ATTITUDE || iKind == (int)DIPLOMACY_ATTITUDE_SHARE
		     || iKind == (int)DIPLOMACY_TECH_SHARE) ? CASC_UNIT_FLAT : CASC_UNIT_PERCENT;
	case MODFAM_STATE_RELIGION:
		return (iKind == (int)STATE_RELIGION_HAPPINESS || iKind == (int)STATE_RELIGION_FREE_EXPERIENCE) ? CASC_UNIT_FLAT : CASC_UNIT_PERCENT;
	case MODFAM_REVOLUTION:
		switch (iKind)
		{
		case REVOLUTION_AMOUNT:
			// SCOPE-SPLIT: city = flat index deltas (210 buildings); empire = the handicap PERCENT modifier
			// plane (11 authorings, e.g. handicap_chieftain revolution.empire.percent). NB empire is DUAL --
			// 6 building empire flats (berlin-wall class) ride the flat slot through the name-split getters.
			return (eScope == CASC_SCOPE_CITY) ? CASC_UNIT_FLAT : CASC_UNIT_PERCENT;
		case REVOLUTION_DISTANCE_MODIFIER:
		case REVOLUTION_GOOD_RELIGION:
		case REVOLUTION_BAD_RELIGION:
		case REVOLUTION_NATIONALITY:
		case REVOLUTION_VIOLENT:
			return CASC_UNIT_PERCENT;
		default:
			return CASC_UNIT_FLAT;   // local/national/holyCity*/democracyLevel/religiousFreedom/... are flat index deltas
		}
	case MODFAM_UPKEEP:
		// the free-allowance kinds are flat amounts (ruling 28); `extra` is a flat surcharge (7, unitcombat
		// sea-animal tale); every other kind is a percent modifier
		return (iKind == (int)UPKEEP_FREE_MILITARY || iKind == (int)UPKEEP_FREE_CIVILIAN
		     || iKind == (int)UPKEEP_EXTRA) ? CASC_UNIT_FLAT : CASC_UNIT_PERCENT;
	case MODFAM_TRADE_ROUTES:
		// the route count (kind 0, memberless) + max are flat counts; the modifier kinds (channel-agnostic +
		// per-channel, ruling 27) are percents
		return (iKind == (int)TRADE_ROUTE_AMOUNT || iKind == (int)TRADE_ROUTE_MAX) ? CASC_UNIT_FLAT : CASC_UNIT_PERCENT;
	case MODFAM_BARBARIANS:
		// the two probability kinds are percent; turns/tiles/defenders counts are flat (handicap census)
		return (iKind == (int)BARBARIANS_ANIMAL_ATTACK_PROB || iKind == (int)BARBARIANS_CITY_CREATION_PROB)
			? CASC_UNIT_PERCENT : CASC_UNIT_FLAT;
	case MODFAM_ESPIONAGE:
		// the unit-plane spy stats are flat; the channel amount rides the named commerce getters, not here
		return CASC_UNIT_FLAT;
	default:
		return CASC_UNIT_FLAT;
	}
}

// The scope-blind transitional overload (see the header): delegates with the family's DOMINANT authored scope,
// preserving the pre-split answers for the existing (family, kind)-only call sites; those sites move to the
// scope-aware overload as the consuming getters gain the spelled-out scope (reported follow-up).
CvCascUnit infoKindUnit(ModifierFamily eFamily, int iKind)
{
	switch (eFamily)
	{
	case MODFAM_CAPTURE:
		return infoKindUnit(eFamily, iKind, CASC_SCOPE_UNIT);      // 428 of 586 authorings are unit-plane
	case MODFAM_MAINTENANCE:
		return infoKindUnit(eFamily, iKind, CASC_SCOPE_EMPIRE);    // the modifier plane (the pre-split blanket answer)
	default:
		return infoKindUnit(eFamily, iKind, CASC_SCOPE_CITY);      // scope-independent rows + the city-plane defaults
	}
}

// Family-combine FLOOR metadata (modifier.md §2; see the header). The table is deliberately minimal -- one row
// per ruled floor, extended only by an owner-ruled combine mode, never inferred from data shape.
bool infoCombineFloorAtZero(ModifierFamily eFamily, int iKind)
{
	if (eFamily == MODFAM_UPKEEP)
	{
		// ruling 28: the free-upkeep kinds sum signed free-amount entries, floored at zero as a GROUP
		return iKind == (int)UPKEEP_FREE_MILITARY || iKind == (int)UPKEEP_FREE_CIVILIAN;
	}
	return false;
}

bool infoIsTargetToken(const std::string& szSegment)
{
	for (int i = 0; INFO_TARGET_TOKENS[i] != 0; ++i)
	{
		if (szSegment == INFO_TARGET_TOKENS[i])
		{
			return true;
		}
	}
	return false;
}

bool infoIsFamilyTargetToken(ModifierFamily eFamily, const std::string& szSegment)
{
	for (int i = 0; INFO_FAMILY_TARGET_TOKENS[i].segment != 0; ++i)
	{
		if (INFO_FAMILY_TARGET_TOKENS[i].family == eFamily && szSegment == INFO_FAMILY_TARGET_TOKENS[i].segment)
		{
			return true;
		}
	}
	return false;
}

void infoNoteUnkindedMember(const std::string& szFamilyKey, const std::string& szMemberPath)
{
	if (s_unkindedMembers.size() < INFO_KIND_DIAG_MAX)
	{
		s_unkindedMembers.insert(szFamilyKey + "." + szMemberPath);
	}
}

const std::set<std::string>& infoUnkindedMembers()
{
	return s_unkindedMembers;
}

void infoResetKindDiag()
{
	s_unkindedMembers.clear();
}
