#pragma once
#ifndef CV_JSON_CIVIC_INFO_H
#define CV_JSON_CIVIC_INFO_H

//
//	CvCivicInfo -- the per-type cascade info for CIVICS. Composes the section units a civic authors (edges /
//	grants / modifier families / the section-9 `policies` bool block). `policies` are the pure empire STATES this
//	civic ENACTS (noForeignTrade / noCorporations / fixedBorders / ...), active while the civic is adopted -- ONE
//	meaning with two grantors (a civic enacts them, a trait grants them permanently), so CvTraitInfo composes
//	the SAME unit. A policy is a pure state, NEVER a parameterized/targeted rule (that is an enabler `requires` concern).
//
//	#430 legacy-getter mirror: the ENTIRE archived CvCivicInfo consumer surface (SourceArchive/Infos/CvCivicInfo.h)
//	is reproduced here (signatures matched EXACTLY; every disposition grounded against Tools/Migration/curate_civic.py
//	+ the shipped Assets/Data/civics/**/*.json + the base/module CIV4CivicInfos.xml occurrence counts -- never guessed).
//
//	REAL DATA (read live off the composed units, or off a typed member set in mapFrom):
//	  - identity scalars: civicOption FK, upkeepLevel FK, anarchyLength, weLoveTheKing, ai.behaviour.weight.
//	  - the section-6 scalar modifier families (maintenance/upkeep/trade/hurry/happiness/health/experience/revolution/
//	    stateReligion/...): the unconditioned scalar recovered from the composed m_modifiers by summing only the
//	    UN-conditioned, non-`per`, un-qualified entries at the curated address (the CvBuildingInfo sumUnconditioned
//	    approach; civics use the SAME families/addressing). ALL RevIdx*/RevViolentMod are REAL (revolution.empire.*).
//	  - the split yield/commerce arrays (YieldModifier/CommerceModifier/TradeYieldModifier/SpecialistExtraCommerce).
//	  - the CONDITIONED leaves the curator merged into a shared address, recovered by matching the condition SHAPE
//	    (pure DATA inspection of the parsed predicate, never a runtime GC.getGame() read -- [DEC-json-not-cascade] holds):
//	    CapitalYieldModifier/CapitalCommerceModifier (the IS_CAPITAL-gated entry), LandmarkHappiness (the
//	    GAMEOPTION_MAP_PERSONALIZED-gated entry), LandmarkYieldChanges (the plots sub-scope), LargestCityHappiness
//	    (the unqualified `cities` entry) and HappyPerMilitaryUnit (the unit:IS_MILITARY-qualified `cities` entry).
//	  - the target-KEYED section-6 families, as per-index scalar getters (address built live off the target's own GC
//	    type string) AND as the populated foreach_ containers the hot-path consumers iterate: the sparse (id,value)
//	    vectors (BuildingHappinessChangesSparse / BuildingHealthChangesSparse / FeatureHappinessChangesSparse) and the
//	    getBuildingProductionModifiers IDValueMap (both filled in mapFrom from the composed m_modifiers).
//	  - the dense positional arrays the UI-help bulk accessors return (getYieldModifierArray / CommerceModifierArray /
//	    Capital* / TradeYieldModifierArray / SpecialistExtraCommerceArray / LandmarkYieldChangesArray): materialized in
//	    mapFrom from the per-index getters, returned NULL-when-all-zero to match the archived dense-storage semantics.
//	  - the enables-edge derived bools (isSpecialistValid / isSpecialBuildingNotRequired / isHurry).
//	  - the section-9 policy STATE flags + ai.flavours.
//
//	NAMED GAPS (each flagged inline BY NAME -- no silent placeholder):
//	  - getTechPrereq: CURATOR-GAP by design -- curate_civic.py DROPs TechPrereq (store-inverted onto tech.enables.civics);
//	    not authored on the civic JSON, not recoverable here. -> NO_TECH.
//	  - getCityLimit(ePlayer): the raw datum IS in JSON (happiness.empire.cityLimit.flat) but this getter's archived
//	    body is a GAME-STATE FORMULA (isOption(GAMEOPTION_EXP_OVEREXPANSION_PENALTIES) * world-size scaling) a pure-data
//	    poco must not evaluate; 0 is behaviour-identical whenever that option is OFF (its default). Needs a consumer-side
//	    formula rework off a raw-datum getter -- surface-sprawl for the owner.
//	  - getEnslavementChance / getReligionSpreadRate / getCorporationSpreadRate / getBonusMintedPercent /
//	    getImprovementHappinessChanges / getImprovementHealthPercentChanges / getBuildingCommerceChange: NOT AUTHORED
//	    anywhere in shipped data (0 occurrences in base + all module CIV4CivicInfos.xml) AND no curator mapping table
//	    entry -> the faithful value is 0 for every civic; if ever needed, add a curate_civic.py mapping.
//	  - getBonusCommerceModifier / getSpecialistYieldPercentChanges / getSpecialistCommercePercentChanges: curator DROP
//	    (BonusCommerceModifiers inverted onto the bonus; the specialist percent changes folded onto the specialist --
//	    double-author avoidance). -> 0.
//	  - getCategory / getNumCategories / isCategory: curator DROP "Categories".
//	  - getCivicAttitudeReason: the cosmetic <Description> reason sibling is dropped by the curator (not gameplay data).
//	  - isPolicy: the bPolicy schema element is NEVER authored (0 occurrences) -> the class-member default false.
//	  - getPropertyManipulators: fed from the curated PROPERTY_* FAMILIES in mapFrom via the ONE shared walk
//	    (CascadePropertyBridge::bridgeFamilies) -- the KEEP-legacy solver's player gather delivers to every owner city.
//	  - getCivicAttitudeChanges (the int* form): NULL -- it has NO runtime consumer (the per-index getCivicAttitudeChange(i)
//	    carries the data); a dense NumCivics-sized owned array would be dead code.
//	  - getBonusCommerceModifierArray: NULL -- curator DROP (BonusCommerceModifiers inverted onto the bonus).
//
//	Hotkey getters are inherited from CvHotkeyInfo and NOT redeclared. The XML load/serialize machinery
//	(read/getDataMembers/getCheckSum/copyNonDefaults + the load-time-only CivicAttitude*Vector* pass3 helpers) is NOT
//	reproduced.
//

#include "CvInfo.h"
#include "Defines/CvEnums.h"           // NO_CIVICOPTION / NO_TECH / TechTypes / PlayerTypes / FeatureTypes / BuildingTypes
#include "Infrastructure/IDValueMap.h" // IDValueMap<BuildingTypes,int> / BuildingModifier2 -- the keyed map + sparse vectors (filled in mapFrom)
#include <map>
#include <string>
#include <vector>

class CvCivicInfo : public CvInfo
{
public:
	CvCivicInfo();

	// --- identity / typed FKs -- REAL data (identity block + ai.behaviour.weight; see mapFrom) ---
	int getCivicOptionType() const { return m_iCivicOptionType; }   // identity.civicOption FK (curate_civic.py IDENTITY)
	int getAnarchyLength() const { return m_iAnarchyLength; }        // identity.anarchyLength
	int getUpkeep() const { return m_iUpkeep; }                      // identity.upkeepLevel (UPKEEP_* FK, GC-registered info type)
	int getAIWeight() const { return m_iAIWeight; }                  // ai.behaviour.weight
	int getMaxConscript() const;                                    // conscript.empire.flat (curate_civic.py SCALAR)
	bool isHurry(int i) const;                                      // enables.hurries edge (curate_civic.py ENABLE_LISTS)
	const CvPropertyManipulators* getPropertyManipulators() const { return &m_PropertyManipulators; } // fed from the PROPERTY_* families in mapFrom (player gather -> every owner city)

	// getTechPrereq: CURATOR-GAP by design -- store-inverted onto tech.enables.civics; not authored on the civic. -> NO_TECH.
	TechTypes getTechPrereq() const { return NO_TECH; }
	// getCityLimit: game-state FORMULA (isOption + world-size scaling) -- MOVED to CvGame::getCivicCityLimit (owner
	// 2026-07-11): a pure-data poco must not read GC.getGame()/getMap() ([DEC-json-not-cascade]). This stub is dead;
	// callers use CvGame::getCivicCityLimit, which scales the raw base below by world size.
	int getCityLimit(PlayerTypes /*ePlayer*/) const { return 0; }
	int getCityLimitBase() const;   // raw base datum (happiness.empire.cityLimit.flat) the CvGame formula scales

	std::wstring pyGetWeLoveTheKing() const { return getWeLoveTheKing(); }
	const wchar_t* getWeLoveTheKing() const { return m_szWeLoveTheKingKey; }       // CvWString -> const wchar_t* implicit (getTextKeyWide precedent)
	const wchar_t* getWeLoveTheKingKey() const { return m_szWeLoveTheKingKey; }

	// --- section-9 policy STATE flags -- REAL (the composed m_policies block; curate_civic.py POLICIES table) ---
	bool isStateReligion() const            { return getPolicies()->has("stateReligion"); }
	bool isNoForeignTrade() const           { return getPolicies()->has("noForeignTrade"); }
	bool isNoCorporations() const           { return getPolicies()->has("noCorporations"); }
	bool isNoForeignCorporations() const    { return getPolicies()->has("noForeignCorporations"); }
	bool isFreeSpeech() const               { return getPolicies()->has("freeSpeech"); }
	bool IsFixedBorders() const             { return getPolicies()->has("fixedBorders"); }
	bool isMilitaryFoodProduction() const   { return getPolicies()->has("militaryFoodProduction"); }
	bool isBuildingOnlyHealthy() const      { return getPolicies()->has("buildingOnlyHealthy"); }
	bool isNoUnhealthyPopulation() const    { return getPolicies()->has("noUnhealthyPopulation"); }
	bool isNoCapitalUnhappiness() const     { return getPolicies()->has("noCapitalUnhappiness"); }
	bool isNoLandmarkAnger() const          { return getPolicies()->has("noLandmarkAnger"); }
	bool isCommunism() const                { return getPolicies()->has("communism"); }
	bool isCanDoElection() const            { return getPolicies()->has("canDoElection"); }
	bool isUpgradeAnywhere() const          { return getPolicies()->has("upgradeAnywhere"); }
	bool isNoNonStateReligionSpread() const { return getPolicies()->has("noNonStateReligionSpread"); }
	bool isAllowInquisitions() const        { return getPolicies()->has("allowInquisitions"); }
	bool isDisallowInquisitions() const     { return getPolicies()->has("disallowInquisitions"); }
	bool isAllReligionsActive() const       { return getPolicies()->has("allReligionsActive"); }
	bool isBansNonStateReligions() const    { return getPolicies()->has("bansNonStateReligions"); }
	bool isFreedomFighter() const           { return getPolicies()->has("freedomFighter"); }
	// isPolicy: the bPolicy schema element is NEVER authored (0 occurrences base+modules) -> class-member default.
	bool isPolicy() const { return false; }

	// --- enables-edge derived bools -- REAL (curate_civic.py ENABLE_LISTS -> enables.specialists / specialBuildingsWaived) ---
	bool isSpecialistValid(int i) const;
	bool isAnySpecialistValid() const;
	bool isSpecialBuildingNotRequired(int i) const;

	// --- section-6 scalar modifier-family reads -- REAL (live over the composed m_modifiers; see .cpp for the address
	// table, grounded against curate_civic.py's SCALAR / STATE_RELIGION dicts) ---
	int getGreatPeopleRateModifier() const;
	int getGreatGeneralRateModifier() const;
	int getDomesticGreatGeneralRateModifier() const;
	int getStateReligionGreatPeopleRateModifier() const;
	int getDistanceMaintenanceModifier() const;
	int getNumCitiesMaintenanceModifier() const;
	int getHomeAreaMaintenanceModifier() const;
	int getOtherAreaMaintenanceModifier() const;
	int getCorporationMaintenanceModifier() const;
	int getExtraHealth() const;
	int getFreeExperience() const;
	int getWorkerSpeedModifier() const;
	int getImprovementUpgradeRateModifier() const;
	int getMilitaryProductionModifier() const;
	int getFreeUnitUpkeepCivilian() const;
	int getFreeUnitUpkeepMilitary() const;
	int getFreeUnitUpkeepCivilianPopPercent() const;
	int getFreeUnitUpkeepMilitaryPopPercent() const;
	int getCivilianUnitUpkeepMod() const;
	int getMilitaryUnitUpkeepMod() const;
	int getWarWearinessModifier() const;
	int getFreeSpecialist() const;
	int getTradeRoutes() const;
	int getCivicPercentAnger() const;
	int getStateReligionHappiness() const;
	int getNonStateReligionHappiness() const;
	int getStateReligionUnitProductionModifier() const;
	int getStateReligionBuildingProductionModifier() const;
	int getStateReligionFreeExperience() const;
	int getExpInBorderModifier() const;
	int getRevIdxLocal() const;
	int getRevIdxNational() const;
	int getRevIdxDistanceModifier() const;
	int getRevIdxHolyCityGood() const;
	int getRevIdxHolyCityBad() const;
	int getRevIdxSwitchTo() const;         // one-time revolution BURST on switch-TO -> grants["revolution"] pulse
	int getRevReligiousFreedom() const;
	int getRevLaborFreedom() const;
	int getRevEnvironmentalProtection() const;
	int getRevDemocracyLevel() const;
	int getAttitudeShareMod() const;
	int getForeignerUnhappyPercent() const;
	int getCityOverLimitUnhappy() const;
	int getForeignTradeRouteModifier() const;
	int getTaxRateUnhappiness() const;
	int getPopulationgrowthratepercentage() const;
	int getCivicHappiness() const;
	int getDistantUnitSupportCostModifier() const;
	int getExtraCityDefense() const;
	int getNationalCaptureProbabilityModifier() const;
	int getNationalCaptureResistanceModifier() const;
	int getInflationModifier() const;
	int getHurryInflationModifier() const;
	int getHurryCostModifier() const;
	int getSharedCivicTradeRouteModifier() const;
	int getFreedomFighterChange() const;

	float getRevIdxNationalityMod() const;
	float getRevIdxBadReligionMod() const;
	float getRevIdxGoodReligionMod() const;
	float getRevViolentMod() const;

	// --- split yield/commerce arrays -- REAL (curate_civic.py SPLIT_ARRAY) ---
	int getYieldModifier(int i) const;
	int getCommerceModifier(int i) const;
	int getTradeYieldModifier(int i) const;
	int getSpecialistExtraCommerce(int i) const;

	// --- conditioned / ranked / qualified leaves -- REAL (recovered by matching the parsed condition SHAPE; pure data,
	// never a runtime GC.getGame() read). See the .cpp for the exact curator merge each one un-picks. ---
	int getCapitalYieldModifier(int i) const;      // <yield>.empire.percent, entry gated IS_CAPITAL (SPLIT_ARRAY_COND)
	int getCapitalCommerceModifier(int i) const;   // <commerce>.empire.percent, entry gated IS_CAPITAL
	int getLandmarkHappiness() const;              // happiness.empire.flat, entry gated GAMEOPTION_MAP_PERSONALIZED (SCALAR_COND)
	int getLargestCityHappiness() const;           // happiness.empire.cities.flat, unqualified entry (ranked `cities` target)
	int getHappyPerMilitaryUnit() const;           // happiness.empire.cities.flat, entry qualified unit:IS_MILITARY
	int getLandmarkYieldChanges(int i) const;      // <yield>.empire.plots.flat (landmark plots deposit)

	// --- target-KEYED section-6 families -- REAL (per-index scalar read off a live-built address) ---
	int getBuildingHappinessChanges(int i) const;
	int getBuildingHealthChanges(int i) const;
	int getBuildingProductionModifier(BuildingTypes e) const;
	int getBuildingCommerceModifier(int i, int j) const;      // i=BuildingTypes, j=CommerceTypes
	int getFeatureHappinessChanges(int i) const;
	int getImprovementYieldChanges(int i, int j) const;       // i=ImprovementTypes, j=YieldTypes
	int getTerrainYieldChanges(int i, int j) const;           // i=TerrainTypes, j=YieldTypes
	int getUnitProductionModifier(int i) const;
	int getUnitCombatProductionModifier(int i) const;
	int getCivicAttitudeChange(int i) const;                  // i=CivicTypes (the OTHER civic)
	int getFreeSpecialistCount(int i) const;                  // i=SpecialistTypes

	bool isAnyBuildingHappinessChange() const;
	bool isAnyBuildingHealthChange() const;
	bool isAnyFeatureHappinessChange() const;
	bool isAnyImprovementYieldChange() const;

	// --- NOT AUTHORED anywhere in shipped data (0 occurrences base+modules) AND no curator mapping -> faithful 0 ---
	int getEnslavementChance() const { return 0; }
	int getReligionSpreadRate() const { return 0; }
	int getCorporationSpreadRate() const { return 0; }
	int getBonusMintedPercent(int /*i*/) const { return 0; }
	int getImprovementHappinessChanges(int /*i*/) const { return 0; }
	int getImprovementHealthPercentChanges(int /*i*/) const { return 0; }
	int getBuildingCommerceChange(int /*i*/, int /*j*/) const { return 0; }   // distinct from getBuildingCommerceModifier (real)

	// --- curator DROP (data inverted/folded elsewhere -- double-author avoidance; curate_civic.py DROP set) -> 0 ---
	int getBonusCommerceModifier(int /*i*/, int /*j*/) const { return 0; }             // inverted onto the bonus
	int getSpecialistYieldPercentChanges(int /*i*/, int /*j*/) const { return 0; }     // folded onto the specialist
	int getSpecialistCommercePercentChanges(int /*i*/, int /*j*/) const { return 0; }  // folded onto the specialist

	// --- ai.flavours -- REAL data ---
	int getFlavorValue(int i) const { return mapGet(m_flavours, i); }

	// --- curator DROP: the cosmetic CivicAttitudeChange <Description> reason sibling (not gameplay data) ---
	CvString getCivicAttitudeReason(int /*i*/) const { return CvString(""); }

	// --- curator DROP "Categories" ---
	int getCategory(int /*i*/) const { return -1; }
	int getNumCategories() const { return 0; }
	bool isCategory(int /*i*/) const { return false; }

	// --- reference-returning KEYED getters ---
	// getBuildingProductionModifiers: POPULATED in mapFrom (REAL) from buildRate.empire.buildings.<B>.percent via
	// IDValueMap::setValue -- the foreach_ processCivics/AI/UI consumers iterate this. Agrees with the per-index
	// getBuildingProductionModifier(e) (same address, same unconditioned filter).
	const IDValueMap<BuildingTypes, int>& getBuildingProductionModifiers() const { return m_aBuildingProductionModifier; }
	// The sparse vectors ARE populated in mapFrom (REAL) -- the hot-path processCivics consumers iterate these.
	const std::vector<BuildingModifier2>& getBuildingHappinessChangesSparse() const { return m_vBuildingHappinessChangesSparse; }
	const std::vector<BuildingModifier2>& getBuildingHealthChangesSparse() const { return m_vBuildingHealthChangesSparse; }
	const std::vector<std::pair<FeatureTypes, int> >& getFeatureHappinessChangesSparse() const { return m_vFeatureHappinessChangesSparse; }
	void invalidateSparseLists() { }   // no-op -- WRITE-ONCE AT LOAD, IMMUTABLE AFTER (no mutation path re-fills the caches)

	// --- int* bulk-array views -- REAL: dense positional arrays materialized in mapFrom (from the per-index getters),
	// returned NULL-when-all-zero to match the archived dense-storage semantics (the UI-help `if(aList)` guards). ---
	int* getYieldModifierArray() const;
	int* getCapitalYieldModifierArray() const;
	int* getTradeYieldModifierArray() const;
	int* getCommerceModifierArray() const;
	int* getCapitalCommerceModifierArray() const;
	int* getSpecialistExtraCommerceArray() const;
	int* getLandmarkYieldChangesArray() const;
	int* getCivicAttitudeChanges() const { return NULL; }               // no runtime consumer; per-index getCivicAttitudeChange(i) carries the data
	int* getBonusCommerceModifierArray(int /*i*/) const { return NULL; } // curator DROP (BonusCommerceModifiers inverted onto the bonus)

	virtual void mapFrom(const picojson::value& entity);

	// --- the composed section units (by value; the base's mapFrom dispatch writes them via mut*) ---
	virtual const CvJsonEdges*     getEdges()     const { return &m_edges; }
	virtual const CvJsonGrants*    getGrants()    const { return &m_grants; }
	virtual const CvJsonModifiers* getModifiers() const { return &m_modifiers; }
	virtual const CvJsonBoolBlock* getPolicies()  const { return &m_policies; }

protected:
	virtual CvJsonEdges*     mutEdges()     { return &m_edges; }
	virtual CvJsonGrants*    mutGrants()    { return &m_grants; }
	virtual CvJsonModifiers* mutModifiers() { return &m_modifiers; }
	virtual CvJsonBoolBlock* mutPolicies()  { return &m_policies; }

private:
	static int mapGet(const std::map<int, int>& m, int k)
	{ std::map<int, int>::const_iterator it = m.find(k); return it != m.end() ? it->second : 0; }

	CvJsonEdges     m_edges;
	CvJsonGrants    m_grants;
	CvJsonModifiers m_modifiers;
	CvJsonBoolBlock m_policies;
	CvPropertyManipulators m_PropertyManipulators;   // fed from the PROPERTY_* families (CascadePropertyBridge::bridgeFamilies)

	// --- typed members mapped from the JSON (REAL data; see mapFrom) ---
	int m_iCivicOptionType;
	int m_iAnarchyLength;
	int m_iUpkeep;
	int m_iAIWeight;
	CvWString m_szWeLoveTheKingKey;
	std::map<int, int> m_flavours;   // FlavorTypes -> weight (ai.flavours)

	// --- dense positional arrays materialized in mapFrom (the int* bulk-array getters return these NULL-when-all-zero) ---
	int m_aiYieldModifier[NUM_YIELD_TYPES];
	int m_aiCapitalYieldModifier[NUM_YIELD_TYPES];
	int m_aiTradeYieldModifier[NUM_YIELD_TYPES];
	int m_aiLandmarkYieldChanges[NUM_YIELD_TYPES];
	int m_aiCommerceModifier[NUM_COMMERCE_TYPES];
	int m_aiCapitalCommerceModifier[NUM_COMMERCE_TYPES];
	int m_aiSpecialistExtraCommerce[NUM_COMMERCE_TYPES];

	// --- keyed backing members: the map + sparse vectors are populated in mapFrom from the composed m_modifiers ---
	IDValueMap<BuildingTypes, int> m_aBuildingProductionModifier;
	std::vector<BuildingModifier2> m_vBuildingHappinessChangesSparse;
	std::vector<BuildingModifier2> m_vBuildingHealthChangesSparse;
	std::vector<std::pair<FeatureTypes, int> > m_vFeatureHappinessChangesSparse;
};

#endif // CV_JSON_CIVIC_INFO_H
