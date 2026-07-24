#pragma once
#ifndef CV_INFO_SCALAR_KINDS_H
#define CV_INFO_SCALAR_KINDS_H

// ============================ the sane-info GROUPED SCALAR vocabulary (patterns.md § coherent surface) ============
// The SHARED grouped-scalar vocabulary every info reads through. Each multi-entry scalar FAMILY is its own
// single-tier group: a flat int[] indexed by a small kind-enum, read by ONE parameterized getter
// (getDefense(DEFENSE_BOMBARD)), x100-native, one direct index -- never N individual getters, never a bundling
// struct. 1-2-entry stragglers with no family live in an info's own m_scalars (SCALAR_*). Extensible: a new
// scalar family is a new enum + member; a new straggler is a new SCALAR_* entry. These kind-enums are GENERAL (a
// defense/maintenance modifier is not building-specific), so every info that groups these families reads the same
// vocabulary from here; an info-specific scalar CATCH-ALL enum stays on that info.
enum DefenseKind {                     // m_defense -- COMBAT_REALISTIC_SIEGE-gated at the consumer
	DEFENSE_AMOUNT = 0,                // defense.city.amount %
	DEFENSE_BOMBARD,                   // defense.city.bombardDefense %
	DEFENSE_ALL_CITY,                  // defense.empire.amount %
	DEFENSE_NUKE,                      // defense.city.nukeDefense %
	DEFENSE_AIR,                       // defense.city.airDefense %
	DEFENSE_MIN,                       // defense.city.min flat (the defense FLOOR)
	DEFENSE_NO_ENTRY_LEVEL,            // defense.city.noEntryLevel flat (the no-entry THRESHOLD -- distinct, owner)
	DEFENSE_LOCAL_DYNAMIC,             // defense.city.dynamicDefense flat
	DEFENSE_RIVER_PENALTY,             // defense.city.riverDefensePenalty flat
	DEFENSE_BUILDING_RECOVERY,         // defense.city.buildingDefenseRecovery %
	DEFENSE_CITY_RECOVERY,             // defense.city.cityDefenseRecovery %
	DEFENSE_ADJACENT_DAMAGE,           // defense.city.adjacentDamage %
	NUM_DEFENSE_KINDS
};
enum MaintenanceKind {                 // m_maintenance (all %)
	MAINTENANCE_CITY = 0,              // maintenance.city
	MAINTENANCE_GLOBAL,                // maintenance.empire
	MAINTENANCE_AREA,                  // maintenance.area
	MAINTENANCE_OTHER_AREA,            // maintenance.area.otherArea
	MAINTENANCE_DISTANCE,              // maintenance.empire.distance
	MAINTENANCE_NUM_CITIES,            // maintenance.empire.numCities
	MAINTENANCE_COASTAL_DISTANCE,      // maintenance.empire.coastalDistance
	MAINTENANCE_CONNECTED_CITY,        // maintenance.empire.connectedCity
	NUM_MAINTENANCE_KINDS
};
enum TradeRouteKind {                  // m_tradeRoutes
	TRADE_ROUTE_CITY = 0,             // tradeRoutes.city flat
	TRADE_ROUTE_COASTAL,              // tradeRoutes.empire.coastal flat
	TRADE_ROUTE_GLOBAL,               // tradeRoutes.empire flat
	TRADE_ROUTE_WORLD,                // tradeRoutes.world flat
	TRADE_ROUTE_MODIFIER,             // tradeRoutes.city.modifier %
	TRADE_ROUTE_FOREIGN_MODIFIER,     // tradeRoutes.city.foreignModifier %
	NUM_TRADE_ROUTE_KINDS
};
enum GreatPeopleKind {                 // m_greatPeople
	GREAT_PEOPLE_RATE_CHANGE = 0,     // greatPeopleRate.city flat
	GREAT_PEOPLE_RATE_MODIFIER,       // greatPeopleRate.city %
	GREAT_PEOPLE_GLOBAL_RATE_MODIFIER,// greatPeopleRate.empire %
	GREAT_GENERAL_RATE,               // greatGeneralRate.city %
	GREAT_GENERAL_DOMESTIC_RATE,      // greatGeneralRate.city.domestic %
	NUM_GREAT_PEOPLE_KINDS
};
enum WarWearinessKind {                // m_warWeariness (all %)
	WAR_WEARINESS_CITY = 0,           // warWeariness.city
	WAR_WEARINESS_GLOBAL,             // warWeariness.empire
	WAR_WEARINESS_ENEMY,              // warWeariness.city.enemy
	NUM_WAR_WEARINESS_KINDS
};
enum HurryKind {                       // m_hurry (all %)
	HURRY_COST = 0,                   // hurryCost.city
	HURRY_GLOBAL_COST,                // hurryCost.empire
	HURRY_ANGER,                      // hurryAnger.city
	NUM_HURRY_KINDS
};
enum CityCaptureKind {                 // m_capture (all %)
	CITY_CAPTURE_NATIONAL_PROBABILITY = 0, // cityCapture.empire.probability
	CITY_CAPTURE_NATIONAL_RESISTANCE,      // cityCapture.empire.resistance
	CITY_CAPTURE_LOCAL_PROBABILITY,        // cityCapture.city.probability
	CITY_CAPTURE_LOCAL_RESISTANCE,         // cityCapture.city.resistance
	NUM_CITY_CAPTURE_KINDS
};
enum RevolutionKind {                  // m_revolution
	REVOLUTION_LOCAL = 0,             // revolution.city flat
	REVOLUTION_NATIONAL,              // revolution.empire flat
	REVOLUTION_DISTANCE_MODIFIER,     // revolution.city.distanceModifier %
	NUM_REVOLUTION_KINDS
};
enum BuildRateKind {                   // m_buildRate -- buildRate CATEGORY cost modifiers (all %); NOT production yield
	BUILD_RATE_MILITARY = 0,          // buildRate.city.military
	BUILD_RATE_SPACE,                 // buildRate.city.space
	BUILD_RATE_GLOBAL_SPACE,          // buildRate.empire.space
	NUM_BUILD_RATE_KINDS
};
enum BuildingScalarKind {              // m_scalars -- the 1-2-entry stragglers with no family
	SCALAR_HEAL_RATE = 0,             // healing.city flat
	SCALAR_FOOD_KEPT,                 // foodKept.city %
	SCALAR_ANARCHY,                   // anarchy.city %
	SCALAR_GOLDEN_AGE,                // goldenAge.empire %
	SCALAR_INFLATION,                 // inflation.empire %
	SCALAR_OCCUPATION_TIME,           // occupationTime.city %
	SCALAR_WORKER_SPEED,              // workRate.empire %
	SCALAR_POP_GROWTH,                // populationGrowthRate.city %
	SCALAR_GLOBAL_POP_GROWTH,         // populationGrowthRate.empire %
	SCALAR_ESPIONAGE_DEFENSE,         // espionageDefense.city flat
	SCALAR_UNIT_UPGRADE_PRICE,        // unitUpgradePrice.empire %
	SCALAR_FREE_EXPERIENCE,           // experience.city flat
	SCALAR_GLOBAL_FREE_EXPERIENCE,    // experience.empire flat
	SCALAR_FREE_SPECIALIST,           // freeSpecialists.city.any count
	SCALAR_AREA_FREE_SPECIALIST,      // freeSpecialists.area.any count
	SCALAR_GLOBAL_FREE_SPECIALIST,    // freeSpecialists.empire.any count
	SCALAR_INSIDIOUSNESS,             // copsAndRobbers.city.insidiousness flat
	SCALAR_INVESTIGATION,             // copsAndRobbers.city.investigation flat
	NUM_BUILDING_SCALAR_KINDS
};
enum WellbeingKind {                   // m_wellbeing -- the building's happiness/health contributions (flat)
	WELLBEING_HAPPINESS = 0,          // happiness.city flat
	WELLBEING_AREA_HAPPINESS,         // happiness.area flat
	WELLBEING_GLOBAL_HAPPINESS,       // happiness.empire flat
	WELLBEING_HEALTH,                 // health.city flat
	WELLBEING_AREA_HEALTH,            // health.area flat
	WELLBEING_GLOBAL_HEALTH,          // health.empire flat
	WELLBEING_HAPPINESS_PER_POPULATION, // happiness.city perPopulation (raw legacy scale; consumer /100s)
	WELLBEING_HEALTH_PER_POPULATION,    // health.city perPopulation
	WELLBEING_STATE_RELIGION_HAPPINESS, // happiness.city flat gated STATE_RELIGION (not yet materialized -- mirrors legacy stub)
	NUM_WELLBEING_KINDS
};

#endif // CV_INFO_SCALAR_KINDS_H
