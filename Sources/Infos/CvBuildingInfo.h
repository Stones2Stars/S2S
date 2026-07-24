#pragma once
#ifndef CV_JSON_BUILDING_INFO_H
#define CV_JSON_BUILDING_INFO_H

//
//	CvBuildingInfo -- the per-type cascade info for BUILDINGS (ports StoneBase's BuildingInfo). Composes the
//	section units a building authors (requires / edges / allowed / grants / provides / modifier families /
//	whenObsolete / attributes / capabilities -- the data-grounded table); this adds the typed flags + the curator
//	`identity` block, SELF-CONTAINED (the engine getGlobalReligionCommerce / getReligionType /
//	getGlobalCorporationCommerce / getStateReligionCommerce / getCommerceDoubleTime reads are RETIRED).
//	shrine/corpHQ/religion are FK ids (-1 none); the commerce blocks are {channel:value} maps.
//
//	#430 legacy-getter mirror: the ENTIRE archived CvBuildingInfo consumer surface (SourceArchive/Infos/CvBuildingInfo.h)
//	is reproduced here. REAL DATA where the poco already maps it (typed identity/cost members, the composed
//	edges/allowed/attributes/capabilities units, and the §6 modifier families read via the sumUnconditioned helper in
//	the .cpp); every remaining getter is an owner-sanctioned STUB-DEFAULT (bool->false, int->0, FK->-1/NO_*,
//	const char*->"", int*->NULL, reference->an empty backing member) tagged with why. Hotkey/action getters are
//	inherited from CvHotkeyInfo and NOT redeclared. The XML load/serialize machinery (read/getDataMembers/getCheckSum/
//	copyNonDefaults/doPostLoadCaching) and the Python cy* list wrappers are NOT reproduced (the JSON `mapFrom` path
//	replaces the former; the latter is Python-binding surface outside this consumer set).
//

#include "CvInfo.h"
#include "Engine/ConstructRequirement.h"   // #195 Phase 2 GOM-derived requirement list (STUB empty; see getConstructRequirements)
#include "Defines/CvStructs.h"             // HealUnitCombat / BonusAidModifiers / AidRateChanges / EnabledCivilizations / YieldArray / CommerceArray
#include <vector>
#include <map>

class CvArtInfoBuilding;
class CvArtInfoMovie;
class CvGameObject;
class BoolExpr;
class CityContext;      // the per-city live VICINITY/local state the valuation endpoints evaluate against (owned by CvCity)
class EmpireContext;    // the per-player empire-scope live state -- passed alongside CityContext (owned by CvPlayer)
class CvPlotGroup;      // the existing trade-network context -- passed alongside the contexts, supplies the TRADED bonuses
class CvJsonModEntry;   // one parsed §3.9 conditioned deposit (value100 + enabled/disabled) -- m_cond points at these

// ============================ the sane-info GROUPED SCALAR vocabulary (patterns.md § coherent surface) ============
// Each multi-entry scalar FAMILY is its own single-tier group: a flat int[] indexed by a small kind-enum, read by
// ONE parameterized getter (getDefense(DEFENSE_BOMBARD)), x100-native, one direct index -- never N individual
// getters, never a bundling struct. 1-2-entry stragglers with no family live in m_scalars (SCALAR_*). Extensible:
// a new scalar family is a new enum + member; a new straggler is a new SCALAR_* entry. These kind-enums are
// GENERAL (a defense/maintenance modifier is not building-specific); they live here for the building build and
// lift to a shared vocabulary header when the pattern generalizes across infos.
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

// The CONDITIONED own-output deposits (patterns.md § coherent surface). A building yield/commerce/wellbeing gated by a
// PREDICATE (HAS_POWER / HAS_RIVER / HAS_TECH / HAS_BONUS / vicinity, ...) is NOT a bespoke member per predicate: every
// conditioned deposit is ONE typed index entry pointing at its already-parsed m_modifiers entry (value100 x100-native +
// its enabled/disabled tree). The (CityContext) getters sum the entries whose condition holds, via the ONE
// cascadeEvalCondition -- no copy, no per-map scale fudge, one evaluator.
enum CondFamily { COND_YIELD = 0, COND_COMMERCE, COND_HAPPINESS, COND_HEALTH };
enum CondTarget { COND_TGT_CITY = 0, COND_TGT_PLOTS };
struct CondDeposit
{
	unsigned char family;    // CondFamily
	unsigned char index;     // YieldTypes / CommerceTypes (0 for the wellbeing families)
	unsigned char unit;      // CvCascUnit (FLAT / PERCENT)
	unsigned char target;    // CondTarget
	unsigned char pred;      // plots-target ONLY: the single CvCascPredKind of the deposit's condition (0xFF = none/compound) -- the plotAttrs.count() key
	const CvJsonModEntry* e; // -> the owned m_modifiers entry (value100 + enabled/disabled); never copied
};

// defense.city.counterDamage -- the trap counter-damage effect, a STRUCTURED member of the DEFENSE family (not a
// scalar): damage + chance + who (all attackers, else the unitCombats list). damageTarget.{UNITCOMBAT} fills unitCombats.
struct CounterDamage
{
	int damage, chance;
	bool allAttackers, present;
	std::vector<int> unitCombats;
	CounterDamage() : damage(0), chance(0), allAttackers(false), present(false) {}
};

class CvBuildingInfo : public CvInfo
{
public:
	CvBuildingInfo()
		: notConstructible(false), governmentCenter(false), forceNoPrereqScaling(false),
		  shrineReligion(-1), corpHQ(-1), religion(-1), freeStartEra(-1), conquestProbability(50), maxPlayerInstancesExtra(0),
		  m_bForceTeamVoteEligible(false),
		  voteSourceType(-1),
		  autoBuild(false),
		  m_iMaxStartEra(-1), m_iAdvisorType(-1), m_iGreatPeopleUnitType(-1), m_iPromotionLineType(-1),
		  m_iExtendsBuilding(-1), m_iProductionContinueBuilding(-1), m_iSpecialBuilding(-1),
		  m_iAirlift(0), m_iAirUnitCapacity(0), m_iDCMAirbombMission(0), m_iLineOfSight(0), m_iWorkableRadius(0),
		  m_iNumPopulationEmployed(0), m_iLinePriority(0), m_iAssetValue(0), m_iPowerValue(0),
		  m_iProductionCost(0), m_iProductionCostSize(0), m_iProductionCostCount(0), m_iProductionCostMaterials(0),
		  m_iProductionCostComplexity(0), m_iAIWeight(0),
		  m_bCenterInCity(false), m_bAllowsNukes(false), m_bNoLimit(false),
		  m_fVisibilityPriority(0.0f), m_iMissionType(-1),
		  m_iNumUnitFullHeal(0), m_iPropertySpawnUnit(-1), m_iPropertySpawnProperty(-1)
	{
		for (int i = 0; i < NUM_YIELD_TYPES; ++i)
		{
			m_flatYields[i] = m_yieldModifiers[i] = m_areaYieldModifiers[i] = 0;
			m_globalYieldModifiers[i] = m_seaPlotYields[i] = 0;
		}
		for (int i = 0; i < NUM_COMMERCE_TYPES; ++i)
		{
			m_flatCommerce[i] = m_commerceModifiers[i] = m_globalCommerceModifiers[i] = 0;
			m_specialistCommerce[i] = m_commerceHappiness[i] = 0;
			m_commerceDoubleTime[i] = m_stateReligionCommerce[i] = 0;
		}
		// zero the grouped scalar arrays -- mapFrom `=`-assigns every slot on a real entity (STATE_RELIGION_HAPPINESS
		// is reset at mapFrom start + accumulated in reconstructFromComposed); this is the defensive floor.
		for (int i = 0; i < NUM_DEFENSE_KINDS; ++i)         m_defense[i]      = 0;
		for (int i = 0; i < NUM_MAINTENANCE_KINDS; ++i)     m_maintenance[i]  = 0;
		for (int i = 0; i < NUM_TRADE_ROUTE_KINDS; ++i)     m_tradeRoutes[i]  = 0;
		for (int i = 0; i < NUM_GREAT_PEOPLE_KINDS; ++i)    m_greatPeople[i]  = 0;
		for (int i = 0; i < NUM_WAR_WEARINESS_KINDS; ++i)   m_warWeariness[i] = 0;
		for (int i = 0; i < NUM_HURRY_KINDS; ++i)           m_hurry[i]        = 0;
		for (int i = 0; i < NUM_CITY_CAPTURE_KINDS; ++i)    m_capture[i]      = 0;
		for (int i = 0; i < NUM_REVOLUTION_KINDS; ++i)      m_revolution[i]   = 0;
		for (int i = 0; i < NUM_BUILD_RATE_KINDS; ++i)      m_buildRate[i]    = 0;
		for (int i = 0; i < NUM_BUILDING_SCALAR_KINDS; ++i) m_scalars[i]      = 0;
		for (int i = 0; i < NUM_WELLBEING_KINDS; ++i)       m_wellbeing[i]    = 0;
		m_bGrantsGoldenAge = false;
		m_iGrantPopulationCity = m_iGrantPopulationEmpire = m_iGrantFreeTechs = 0;
		m_iGrantFreeSpecialTech = -1;   // NO_TECH
	}

	bool notConstructible, governmentCenter, forceNoPrereqScaling;   // notConstructible/forceNoPrereqScaling <- identity; governmentCenter <- `attributes` (IS_GOVERNMENT_CENTER)
	std::string specialBuildingType;
	int shrineReligion;                                  // top-level `shrine` -> religion FK
	int corpHQ;                                          // top-level `headquarters` -> corporation FK
	int religion;                                        // identity.religion -> religion FK (state-religion match)
	std::map<std::string, int> stateReligionCommerce;    // identity.stateReligionCommerce {channel:value}
	std::map<std::string, int> commerceDoubleTime;       // identity.commerceDoubleTime {channel:years}
	int freeStartEra;         // identity.freeStartEra -> EraTypes FK
	int conquestProbability;  // identity.conquestProbability
	int maxPlayerInstancesExtra;  // identity.maxPlayerInstancesExtra -- the extra-per-player instance-cap component (only PALACE authors it)
	CounterDamage m_counterDamage;                      // defense.city.counterDamage.{damage,chance} + damageTarget.{UNITCOMBAT}
	bool m_bForceTeamVoteEligible;                      // enables.votes marker "FORCE_TEAM_ELIGIBLE" (non-INFOTYPE, edge-dropped)
	int voteSourceType;       // identity.diploVoteType -> VoteSourceTypes FK
	bool autoBuild;           // identity.autoBuild
	virtual void mapFrom(const picojson::value& entity);

	// ============================ #430 mirrored legacy CvBuildingInfo getters ============================
	// (consumer surface; hotkey/action getters are inherited from CvHotkeyInfo and NOT redeclared)

	// --- bool flags: REAL where the `attributes` block / identity carry them; else STUB false ---
	// attribute flags: O(1) generated-id bit tests (CLS_HAS; ATTRIBUTE_* ids from the ClassificationRegistry)
	bool isForceNoPrereqScaling() const    { return forceNoPrereqScaling; }
	bool isNoLimit() const                 { return m_bNoLimit; }
	bool isAutoBuild() const               { return autoBuild; }
	bool isForceTeamVoteEligible() const   { return m_bForceTeamVoteEligible; }   // enables.votes marker "FORCE_TEAM_ELIGIBLE" (read raw in mapFrom -- the edge parser drops the non-INFOTYPE id)
	bool isGovernmentCenter() const        { return governmentCenter; }
	bool isCenterInCity() const            { return m_bCenterInCity; }
	bool isAllowsNukes() const             { return m_bAllowsNukes; }
	bool isGoldenAge() const               { return m_bGrantsGoldenAge; }   // REAL grants.goldenAge (materialized at mapFrom)
	bool isDamageAllAttackers() const      { return m_counterDamage.allAttackers; }   // counterDamage with NO `units` selector
	bool isDamageAttackerCapable() const   { return m_counterDamage.present; }        // a defense.city.counterDamage object exists
	bool EnablesOtherBuildings() const     { const std::vector<int>* v = getEdges()->find(EDGEF_ENABLES, EDGEB_BUILDINGS); return v != NULL && !v->empty(); }   // REAL enables.buildings edge
	bool EnablesUnits() const              { const std::vector<int>* v = getEdges()->find(EDGEF_ENABLES, EDGEB_UNITS); return v != NULL && !v->empty(); }         // REAL enables.units edge

	// --- typed identity / cost members -- REAL data ---
	int getReligionType() const              { return religion; }                // identity.religion FK (state-religion match)
	int getGlobalCorporationCommerce() const { return corpHQ; }                  // NB legacy-misnamed: a CORPORATION FK (this building IS that corp's HQ), not a commerce amount
	int getGlobalReligionCommerce() const    { return shrineReligion; }          // NB legacy-misnamed: the SHRINE's RELIGION FK (values live on the religion), mirrors getGlobalCorporationCommerce
	EraTypes getFreeStartEra() const         { return (EraTypes)freeStartEra; }
	int getMaxStartEra() const               { return m_iMaxStartEra; }
	int getConquestProbability() const       { return conquestProbability; }
	int getVoteSourceType() const            { return voteSourceType; }
	int getAdvisorType() const               { return m_iAdvisorType; }
	int getGreatPeopleUnitType() const       { return m_iGreatPeopleUnitType; }
	PromotionLineTypes getPromotionLineType() const { return (PromotionLineTypes)m_iPromotionLineType; }
	BuildingTypes getExtendsBuilding() const        { return (BuildingTypes)m_iExtendsBuilding; }
	BuildingTypes getProductionContinueBuilding() const { return (BuildingTypes)m_iProductionContinueBuilding; }
	SpecialBuildingTypes getSpecialBuilding() const { return (SpecialBuildingTypes)m_iSpecialBuilding; }
	int getAirlift() const                   { return m_iAirlift; }
	int getAirUnitCapacity() const           { return m_iAirUnitCapacity; }
	int getDCMAirbombMission() const         { return m_iDCMAirbombMission; }
	int getLineOfSight() const               { return m_iLineOfSight; }
	int getWorkableRadius() const            { return m_iWorkableRadius; }
	int getNumPopulationEmployed() const     { return m_iNumPopulationEmployed; }
	int getLinePriority() const              { return m_iLinePriority; }
	int getAssetValue() const                { return m_iAssetValue * 100; }     // archive returns the raw worth ×100
	int getPowerValue() const                { return m_iPowerValue * 100; }
	int getProductionCost() const            { return m_iProductionCost; }
	int getProductionCostSize() const        { return m_iProductionCostSize; }
	int getProductionCostCount() const       { return m_iProductionCostCount; }
	int getProductionCostMaterials() const   { return m_iProductionCostMaterials; }
	int getProductionCostComplexity() const  { return m_iProductionCostComplexity; }
	float getVisibilityPriority() const      { return m_fVisibilityPriority; }
	const char* getConstructSound() const    { return m_szConstructSound.c_str(); }

	// --- the declarative instance cap (`allowed` unit) + the store edges -- REAL data ---
	int getMaxGlobalInstances() const        { return getAllowed()->cap("world"); }
	int getMaxTeamInstances() const          { return getAllowed()->cap("team"); }
	int getMaxPlayerInstances() const        { return getAllowed()->cap("empire"); }   // NB folds +extra (curator); the split is not separately authored
	int getExtraPlayerInstances() const      { return maxPlayerInstancesExtra; }   // identity.maxPlayerInstancesExtra (separate from the allowed.empire fold; survives it -- e.g. PALACE, empty allowed block)
	int getFoundsCorporation() const;        // enables.corporations edge (REAL)
	TechTypes getObsoleteTech() const;       // obsoletedBy.techs edge (REAL)
	BuildingTypes getObsoletesToBuilding() const;  // obsoletedBy.buildings edge (REAL)

	// --- EXE-bound art surface (world.art.icon -> ARTFILEMGR shim, mirrors CvBonusInfo) -- REAL data ---
	const char* getArtDefineTag() const      { return m_szArtDefineTag.c_str(); }
	const CvArtInfoBuilding* getArtInfo() const;
	const char* getButton() const;   // art-define button (else CvInfoBase's empty m_szButton -> missing icon)
	const char* getMovieDefineTag() const    { return m_szMovieDefineTag.c_str(); }   // REAL ui.art.movie.defineTag
	const char* getMovie() const;                                                     // REAL getMovieInfo()->getPath()
	const CvArtInfoMovie* getMovieInfo() const;                                       // REAL ARTFILEMGR.getMovieArtInfo(tag)

	// --- RUNTIME (set post-load, NOT JSON; mirrors CvCorporationInfo) ---
	int getMissionType() const               { return m_iMissionType; }
	void setMissionType(int iType)           { m_iMissionType = iType; }

	// --- grouped SCALAR reads: ONE parameterized getter per family, x100-native, one direct index (patterns.md
	// § coherent surface). Replaces the ~60 scattered legacy scalar getters below. ---
	int getDefense(DefenseKind eKind) const           { return (eKind >= 0 && eKind < NUM_DEFENSE_KINDS)         ? m_defense[eKind]      : 0; }   // gate COMBAT_REALISTIC_SIEGE at the consumer
	int getMaintenance(MaintenanceKind eKind) const   { return (eKind >= 0 && eKind < NUM_MAINTENANCE_KINDS)     ? m_maintenance[eKind]  : 0; }
	int getTradeRoutes(TradeRouteKind eKind) const    { return (eKind >= 0 && eKind < NUM_TRADE_ROUTE_KINDS)     ? m_tradeRoutes[eKind]  : 0; }
	int getGreatPeople(GreatPeopleKind eKind) const   { return (eKind >= 0 && eKind < NUM_GREAT_PEOPLE_KINDS)    ? m_greatPeople[eKind]  : 0; }
	int getWarWeariness(WarWearinessKind eKind) const { return (eKind >= 0 && eKind < NUM_WAR_WEARINESS_KINDS)   ? m_warWeariness[eKind] : 0; }
	int getHurry(HurryKind eKind) const               { return (eKind >= 0 && eKind < NUM_HURRY_KINDS)           ? m_hurry[eKind]        : 0; }
	int getCapture(CityCaptureKind eKind) const       { return (eKind >= 0 && eKind < NUM_CITY_CAPTURE_KINDS)    ? m_capture[eKind]      : 0; }
	int getRevolution(RevolutionKind eKind) const     { return (eKind >= 0 && eKind < NUM_REVOLUTION_KINDS)      ? m_revolution[eKind]   : 0; }
	int getBuildRate(BuildRateKind eKind) const       { return (eKind >= 0 && eKind < NUM_BUILD_RATE_KINDS)      ? m_buildRate[eKind]    : 0; }
	int getScalar(BuildingScalarKind eKind) const     { return (eKind >= 0 && eKind < NUM_BUILDING_SCALAR_KINDS) ? m_scalars[eKind]      : 0; }
	int getWellbeing(WellbeingKind eKind) const       { return (eKind >= 0 && eKind < NUM_WELLBEING_KINDS)       ? m_wellbeing[eKind]    : 0; }

	// --- classification (json.md §8): the NAME encodes direction (owner) -- an attribute is something the building
	// HAS; a capability is something it PROVIDES to the empire. Singular parameterized check (O(1) id bit test) +
	// plural collection. ---
	bool hasAttribute(int iAttributeId) const           { return m_attributes.hasId(iAttributeId); }
	const CvJsonBoolBlock* hasAttributes() const        { return &m_attributes; }
	bool providesCapability(int iCapabilityId) const    { return m_capabilities.hasId(iCapabilityId); }
	const CvJsonBoolBlock* providesCapabilities() const { return &m_capabilities; }

	// --- defense counter-damage (the trap effect) -- REAL data (materialized into m_counterDamage at mapFrom) ---
	int getDamageAttackerChance() const { return m_counterDamage.chance; }   // defense.city.counterDamage.chance
	int getDamageToAttacker() const     { return m_counterDamage.damage; }   // defense.city.counterDamage.damage

	// per-YIELD / per-COMMERCE indexed family reads -- the UNCONDITIONED base (bare materialized ×100 read). The
	// building's ACTUAL contextual output (base + conditioned deposits) is the per-GROUP valuation block below.
	int getFlatYield(YieldTypes eYield) const;
	int getYieldModifier(YieldTypes eYield) const;
	int getAreaYieldModifier(YieldTypes eYield) const;
	int getGlobalYieldModifier(YieldTypes eYield) const;
	int getSeaPlotYield(YieldTypes eYield) const;
	int getFlatCommerce(CommerceTypes eCommerce) const;
	int getCommerceModifier(CommerceTypes eCommerce) const;
	int getGlobalCommerceModifier(CommerceTypes eCommerce) const;
	int getSpecialistCommerce(CommerceTypes eCommerce) const;
	int getCommerceDoubleTime(CommerceTypes eCommerce) const;   // commerceDoubleTime map (REAL)
	int getStateReligionCommerce(CommerceTypes eCommerce) const;      // stateReligionCommerce map (REAL)

	// --- VALUATION: the building's ACTUAL per-turn output in a given city, ONE endpoint per GROUP (owner). Pass the
	// live contexts -- CityContext (vicinity/local + the river/water/... plot-attr COUNTS), EmpireContext (empire-scope
	// state), CvPlotGroup (traded bonuses) -- and get the whole group's ×100 values: the UNCONDITIONED base PLUS every
	// conditioned m_cond deposit whose condition holds, via the ONE evaluator (MMKernel::applies over the ctx the
	// contexts fill). Fills aiOut. (Traded bonuses resolve through the bound city's plot-group; the CvPlotGroup arg is
	// the reserved explicit source for when the evaluator reads it directly.) ---
	void expectedFlatYields(const CityContext& cityContext, const EmpireContext& empireContext, const CvPlotGroup& plotGroup, int aiOut[NUM_YIELD_TYPES]) const;
	void expectedYieldModifiers(const CityContext& cityContext, const EmpireContext& empireContext, const CvPlotGroup& plotGroup, int aiOut[NUM_YIELD_TYPES]) const;
	void expectedPlotYields(const CityContext& cityContext, const EmpireContext& empireContext, const CvPlotGroup& plotGroup, int aiOut[NUM_YIELD_TYPES]) const;   // <yield>.city.plots = flat × cityContext.plotAttrs.count(predicate)
	void expectedFlatCommerce(const CityContext& cityContext, const EmpireContext& empireContext, const CvPlotGroup& plotGroup, int aiOut[NUM_COMMERCE_TYPES]) const;
	void expectedWellbeing(const CityContext& cityContext, const EmpireContext& empireContext, const CvPlotGroup& plotGroup, int aiOut[NUM_WELLBEING_KINDS]) const;

	// ai.flavours -- REAL data.
	int getFlavorValue(FlavorTypes eFlavor) const { return mapGet(m_flavours, eFlavor); }



	// clean grant/repeatable + AI reads (materialized at mapFrom from grants/ai) -- REAL.
	int getAIWeight() const                { return m_iAIWeight; }             // REAL ai.behaviour.weight
	int getGlobalPopulationChange() const  { return m_iGrantPopulationEmpire; }   // REAL grants.population.empire
	int getFreeTechs() const               { return m_iGrantFreeTechs; }          // REAL grants.freeTechs
	int getNumUnitFullHeal() const         { return m_iNumUnitFullHeal; }      // REAL grants.repeatable[] heal:"full" count
	int getPopulationChange() const        { return m_iGrantPopulationCity; }   // REAL grants.population.city
	TechTypes getFreeSpecialTech() const   { return (TechTypes)m_iGrantFreeSpecialTech; }   // REAL grants.techs (FreeSpecialTech)
	UnitTypes getPropertySpawnUnit() const     { return (UnitTypes)m_iPropertySpawnUnit; }        // REAL grants.repeatable[].unit (property-spawn)
	PropertyTypes getPropertySpawnProperty() const { return (PropertyTypes)m_iPropertySpawnProperty; }  // REAL grants.repeatable[].chance.per property FK

	// --- reference / pointer returning getters -- REAL (backing members materialized from the composed units) ---
	const std::vector<ConstructRequirement>& getConstructRequirements() const { return m_constructRequirements; }  // STUB empty -- #195 GOM derivation deferred (cascade-level)
	const IDValueMap<BonusTypes, int>& getFreeBonuses() const                { return m_freeBonuses; }             // REAL -- materializeFromComposed fills m_freeBonuses from provides.bonuses (owner ruling 2026-07-11)
	const std::vector<MapCategoryTypes>& getMapCategories() const            { return m_aeMapCategoryTypes; }      // REAL (identity.mapCategories)
	const std::vector<FreePromoTypes>& getFreePromoTypes() const             { return m_aFreePromoTypes; }         // REAL grants.freePromotions (populated in mapFrom)
	const std::vector<TraitTypes>& getFreeTraitTypes() const                 { return m_aiFreeTraitTypes; }        // REAL enables.traits (populated in mapFrom)
	const CvProperties* getProperties() const                { return &m_Properties; }              // EMPTY by ruling -- one-shots re-classified per-turn (property-audit.md one-shot ruling); nothing is held
	const CvProperties* getPropertiesAllCities() const        { return &m_PropertiesAllCities; }      // EMPTY by ruling -- ditto (the empire entries ride getPropertyManipulatorsAllCities)
	const CvProperties* getPrereqMinProperties() const        { return &m_PrereqMinProperties; }      // STUB empty
	const CvProperties* getPrereqMaxProperties() const        { return &m_PrereqMaxProperties; }      // STUB empty
	const CvProperties* getPrereqPlayerMinProperties() const  { return &m_PrereqPlayerMinProperties; }// STUB empty
	const CvProperties* getPrereqPlayerMaxProperties() const  { return &m_PrereqPlayerMaxProperties; }// STUB empty
	const CvPropertyManipulators* getPropertyManipulators() const { return &m_PropertyManipulators; } // fed from the PROPERTY_*.{city|plot} families in mapFrom (property-audit.md increments B+4)
	// The building's EMPIRE-scope per-turn sources (the converted <PropertiesAllCities> one-shots, curated
	// PROPERTY_X.empire.flat): delivered in EVERY city of the owner by the CvGameObjectCity::foreachManipulator
	// all-cities walk, count-scaled (property-audit.md one-shot ruling / revived increment 5).
	const CvPropertyManipulators* getPropertyManipulatorsAllCities() const { return &m_PropertyManipulatorsAllCities; }

	// --- coherent materialized reads that survive the legacy-mirror cut ---
	int getCommerceHappiness(CommerceTypes eCommerce) const;                              // REAL commerceHappiness.city.<commerce>.flat
	int getVictoryThreshold(int i) const { return mapGet(m_victoryThresholds, i); }   // REAL identity.victoryThresholds {VICTORY_X:n}

	// --- replacement / counter-damage / heal / enabled-civ list accessors -- REAL (composed units + own data) ---
	int getReplacementBuilding(int i) const         { const std::vector<int>& d = getRequires()->dormantTriggers; return (i >= 0 && i < (int)d.size()) ? d[i] : -1; }  // REAL requires.operate.dormant
	short getNumReplacementBuilding() const         { return (short)getRequires()->dormantTriggers.size(); }
	void setReplacedBuilding(int i)                 { m_replacedBuildings.push_back(i); }   // RUNTIME reverse index (set post-load by the replacers)
	int getReplacedBuilding(int i) const            { return vecAt(m_replacedBuildings, i); }
	short getNumReplacedBuilding() const            { return (short)m_replacedBuildings.size(); }
	int getMayDamageAttackingUnitCombatType(int i) const { return (i >= 0 && i < (int)m_counterDamage.unitCombats.size()) ? m_counterDamage.unitCombats[i] : -1; }   // defense.city.counterDamage.units.unitCombats[]
	int getNumMayDamageAttackingUnitCombatTypes() const  { return (int)m_counterDamage.unitCombats.size(); }
	bool isMayDamageAttackingUnitCombatType(int i) const { for (size_t j = 0; j < m_counterDamage.unitCombats.size(); ++j) if (m_counterDamage.unitCombats[j] == i) return true; return false; }
	int getNumHealUnitCombatTypes() const           { return (int)m_healUnitCombats.size(); }   // REAL grants.repeatable[] unitCombat heal
	const HealUnitCombat& getHealUnitCombatType(int iIndex) const;
	int getNumEnabledCivilizationTypes() const      { return (int)m_enabledCivTypes.size(); }   // REAL identity.enabledCivilizations
	const EnabledCivilizations& getEnabledCivilizationType(int iIndex) const;
	bool isHurry(int i) const   // REAL enables.hurries edge (HURRY_* FK ids)
	{ const std::vector<int>* v = getEdges()->find(EDGEF_ENABLES, EDGEB_HURRIES); if (!v) return false;
	  for (std::size_t k = 0; k < v->size(); ++k) if ((*v)[k] == i) return true; return false; }

	// --- the composed section units (by value; the base's mapFrom dispatch writes them via mut*) ---
	virtual const CvJsonRequires*  getRequires()     const { return &m_requires; }
	virtual const CvJsonEdges*     getEdges()        const { return &m_edges; }
	virtual const CvJsonAllowed*   getAllowed()      const { return &m_allowed; }
	virtual const CvJsonGrants*    getGrants()       const { return &m_grants; }
	virtual const CvJsonProvides*  getProvides()     const { return &m_provides; }
	virtual const CvJsonModifiers* getModifiers()    const { return &m_modifiers; }
	virtual const CvJsonModifiers* getWhenObsolete() const { return &m_whenObsolete; }
	virtual const CvJsonBoolBlock* getAttributes()   const { return &m_attributes; }
	virtual const CvJsonBoolBlock* getCapabilities() const { return &m_capabilities; }

protected:
	virtual CvJsonRequires*  mutRequires()     { return &m_requires; }
	virtual CvJsonEdges*     mutEdges()        { return &m_edges; }
	virtual CvJsonAllowed*   mutAllowed()      { return &m_allowed; }
	virtual CvJsonGrants*    mutGrants()       { return &m_grants; }
	virtual CvJsonProvides*  mutProvides()     { return &m_provides; }
	virtual CvJsonModifiers* mutModifiers()    { return &m_modifiers; }
	virtual CvJsonModifiers* mutWhenObsolete() { return &m_whenObsolete; }
	virtual CvJsonBoolBlock* mutAttributes()   { return &m_attributes; }
	virtual CvJsonBoolBlock* mutCapabilities() { return &m_capabilities; }

private:
	static int mapGet(const std::map<int, int>& m, int k)
	{ std::map<int, int>::const_iterator it = m.find(k); return it != m.end() ? it->second : 0; }
	static bool vecHas(const std::vector<int>& v, int x)
	{ for (std::size_t i = 0; i < v.size(); ++i) if (v[i] == x) return true; return false; }
	static int vecAt(const std::vector<int>& v, int i)
	{ return (i >= 0 && i < (int)v.size()) ? v[i] : -1; }
	// Read one slot of an array-valued IDValueMap without instantiating getValue (which returns the scalar
	// defaultValue and would not convert to Yield/CommerceArray). Iterates the public pair range.
	template <class MapT> static int arrSlot(const MapT& m, int id, int slot, int n)
	{
		if (slot < 0 || slot >= n) return 0;
		for (typename MapT::const_iterator it = m.begin(); it != m.end(); ++it)
			if ((int)it->first == id) return it->second[slot];
		return 0;
	}
	static int nestedGet(const std::map<int, std::map<int, int> >& m, int i, int j)
	{ std::map<int, std::map<int, int> >::const_iterator t = m.find(i); if (t == m.end()) return 0;
	  std::map<int, int>::const_iterator s = t->second.find(j); return s != t->second.end() ? s->second : 0; }
	template <class MapT> static int idvCount(const MapT& m)
	{ int n = 0; for (typename MapT::const_iterator it = m.begin(); it != m.end(); ++it) ++n; return n; }

	// Materialize the typed members that read the composed units ONCE at load ([DEC-materialize-at-mapfrom]): the
	// provides-bonus supply (m_freeBonuses), the KEEP-legacy property manipulators, and the conditioned own-output
	// index m_cond the per-group valuation endpoints sum. Called at the end of mapFrom. NO legacy-shaped mirror.
	void materializeFromComposed();

	CvJsonRequires  m_requires;
	CvJsonEdges     m_edges;
	CvJsonAllowed   m_allowed;
	CvJsonGrants    m_grants;
	CvJsonProvides  m_provides;
	CvJsonModifiers m_modifiers;
	CvJsonModifiers m_whenObsolete;
	CvJsonBoolBlock m_attributes;
	CvJsonBoolBlock m_capabilities;

	// --- the grouped SCALAR storage (patterns.md § coherent surface): single-tier flat arrays, one direct index
	// per read, x100-native; materialized once at mapFrom. Replaces the ~60 scattered legacy scalar members. ---
	int m_defense[NUM_DEFENSE_KINDS];
	int m_maintenance[NUM_MAINTENANCE_KINDS];
	int m_tradeRoutes[NUM_TRADE_ROUTE_KINDS];
	int m_greatPeople[NUM_GREAT_PEOPLE_KINDS];
	int m_warWeariness[NUM_WAR_WEARINESS_KINDS];
	int m_hurry[NUM_HURRY_KINDS];
	int m_capture[NUM_CITY_CAPTURE_KINDS];
	int m_revolution[NUM_REVOLUTION_KINDS];
	int m_buildRate[NUM_BUILD_RATE_KINDS];
	int m_scalars[NUM_BUILDING_SCALAR_KINDS];
	int m_wellbeing[NUM_WELLBEING_KINDS];

	// --- typed members mapped from the JSON (REAL data) ---
	std::string m_szArtDefineTag;   // world.art.icon (ART_DEF_* tag; the ARTFILEMGR lookup key)
	std::string m_szMovieDefineTag; // ui.art.movie.defineTag (ART_DEF_MOVIE_* tag)
	std::string m_szConstructSound; // sound.construct
	int m_iMaxStartEra, m_iAdvisorType, m_iGreatPeopleUnitType, m_iPromotionLineType;
	int m_iExtendsBuilding, m_iProductionContinueBuilding, m_iSpecialBuilding;
	int m_iAirlift, m_iAirUnitCapacity, m_iDCMAirbombMission, m_iLineOfSight, m_iWorkableRadius;
	int m_iNumPopulationEmployed, m_iLinePriority, m_iAssetValue, m_iPowerValue;
	int m_iProductionCost, m_iProductionCostSize, m_iProductionCostCount, m_iProductionCostMaterials, m_iProductionCostComplexity;
	int m_iAIWeight;                // ai.behaviour.weight
	bool m_bCenterInCity, m_bAllowsNukes, m_bNoLimit;
	float m_fVisibilityPriority;
	std::map<int, int> m_flavours;                        // FlavorTypes -> weight (ai.flavours)
	std::map<int, int> m_victoryThresholds;               // VictoryTypes -> threshold (identity.victoryThresholds)
	std::vector<MapCategoryTypes> m_aeMapCategoryTypes;   // identity.mapCategories
	int m_iMissionType;             // RUNTIME (set post-load, NOT JSON; mirrors CvCorporationInfo)

	// --- MATERIALIZED §6 scalar/positional members (mapFrom-filled via JsonModScan, same address table as the old
	// per-call reads; the getters are bare member reads -- per-call string-address walks are banned from getters) ---
	int m_flatYields[NUM_YIELD_TYPES], m_yieldModifiers[NUM_YIELD_TYPES], m_areaYieldModifiers[NUM_YIELD_TYPES];
	int m_globalYieldModifiers[NUM_YIELD_TYPES], m_seaPlotYields[NUM_YIELD_TYPES];
	int m_flatCommerce[NUM_COMMERCE_TYPES], m_commerceModifiers[NUM_COMMERCE_TYPES], m_globalCommerceModifiers[NUM_COMMERCE_TYPES];
	int m_specialistCommerce[NUM_COMMERCE_TYPES], m_commerceHappiness[NUM_COMMERCE_TYPES];
	int m_commerceDoubleTime[NUM_COMMERCE_TYPES], m_stateReligionCommerce[NUM_COMMERCE_TYPES];
	bool m_bGrantsGoldenAge;   // grants.goldenAge flag (materialized at mapFrom)
	int m_iGrantPopulationCity, m_iGrantPopulationEmpire, m_iGrantFreeTechs, m_iGrantFreeSpecialTech;   // the first-build provisions (materialized at mapFrom)

	// --- backing members for the reference-returning getters (materialized from the composed units at load) ---
	std::vector<ConstructRequirement> m_constructRequirements;
	IDValueMap<BonusTypes, int> m_freeBonuses;
	std::vector<FreePromoTypes> m_aFreePromoTypes;
	std::vector<TraitTypes> m_aiFreeTraitTypes;
	CvProperties m_Properties;
	CvProperties m_PropertiesAllCities;
	CvProperties m_PrereqMinProperties;
	CvProperties m_PrereqMaxProperties;
	CvProperties m_PrereqPlayerMinProperties;
	CvProperties m_PrereqPlayerMaxProperties;
	CvPropertyManipulators m_PropertyManipulators;
	CvPropertyManipulators m_PropertyManipulatorsAllCities;   // empire-scope per-turn sources (converted <PropertiesAllCities>)

	// runtime reverse index (set post-load by the replacers) + identity.enabledCivilizations + the grant/repeatable reads.
	std::vector<int> m_replacedBuildings;
	std::vector<EnabledCivilizations> m_enabledCivTypes;   // identity.enabledCivilizations
	int m_iNumUnitFullHeal, m_iPropertySpawnUnit, m_iPropertySpawnProperty;
	std::vector<HealUnitCombat> m_healUnitCombats;         // grants.repeatable[] unitCombat heal

	// the CONDITIONED own-output index (patterns.md § coherent surface): every predicate-gated yield/commerce/wellbeing
	// deposit as a typed pointer into m_modifiers' owned entries (materialized at mapFrom). The per-group valuation
	// endpoints sum the entries whose condition holds -- x100-native.
	std::vector<CondDeposit> m_cond;
};

#endif // CV_JSON_BUILDING_INFO_H
