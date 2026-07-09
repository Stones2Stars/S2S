#pragma once
#ifndef CV_JSON_TRAIT_INFO_H
#define CV_JSON_TRAIT_INFO_H

//
//	CvJsonTraitInfo -- the per-type cascade info for TRAITS (the COMMON base of the two trait sets). Composes the
//	section units a trait authors (edges / grants / modifier families / the §9 `policies` bool block -- the SAME
//	pure-empire-STATE set a civic enacts; a trait grants them permanently while held). Extension: the `negativeTrait`
//	alignment flag (the PURE_TRAITS gate). The two DISTINCT trait sets are CvJsonSimpleTraitInfo /
//	CvJsonComplexTraitInfo (their ids collide; the active set is chosen by GAMEOPTION_LEADER_COMPLEX_TRAITS). The
//	cascade NEVER reads the engine CvJsonTraitInfo for trait values (its runtime CvInfoReplacements swap can't represent
//	this clean split).
//
//	STUB Note (owner 2026-07-01): the legacy `freeSpecialistPer{World,National,Team}Wonder` keys under a trait `policies`
//	block are EFFECTS (free specialists scaled per wonder, CvCity:5764), not pure states -> they reclassify to a
//	`freeSpecialists` modifier family via the curator; until then they ride here in `policies` harmlessly (no consumer
//	this pass). (`nonStateReligionCommerce` is VERIFIED a pure STATE -- a Free-Church permission -- so it correctly stays.)
//

#include "CvJsonInfo.h"
#include "Defines/CvEnums.h"          // NUM_COMMERCE_TYPES / NUM_YIELD_TYPES / the enum id types + NO_* sentinels
#include "Defines/CvStructs.h"        // BuildingModifier / UnitModifier / SpecialUnitModifier / CivicOptionTypeBool / UnitCombatModifier / DisallowedTraitType
#include "Infrastructure/IDValueMap.h"
#include <map>
#include <set>
#include <string>
#include <vector>

class CvJsonTraitInfo : public CvJsonInfo
{
public:
	CvJsonTraitInfo();
	bool negativeTrait;                 // StoneBase NegativeTrait -- PURE_TRAITS drops a negative trait's positive values / a positive trait's negative
	bool civilizationTrait;             // identity.civilizationTrait -- a civilization (not leader) trait (curate_trait.py IDENTITY_FLAGS bCivilizationTrait)
	virtual void mapFrom(const picojson::value& entity);

	// ============================================================================================================
	//	Mirrored legacy CvTraitInfo getters -- the FULL consumer surface (SourceArchive/Infos/CvTraitInfo.h,
	//	signatures matched EXACTLY). REAL DATA wherever the curator (Tools/Migration/curate_trait.py) emits a plain
	//	family/scope/[member/]unit address, read once in mapFrom via the shared jsonFamVal/jsonFamMemberVal (json.md
	//	§6) into a member. STUB-DEFAULT (owner-sanctioned, priority-rule "EVERYTHING ELSE") for: RevIdx* (RevolutionDCM,
	//	out of scope), runtime-game-option gates ([DEC-json-not-cascade] -- a pure-DATA poco must not read
	//	GC.getGame().isOption), conditioned/ranked/unit-qualified list leaves (not a plain scalar), textual-reference
	//	FKs (store-inverted -> enables/succession, not composed here), and the KEYED IDValueMap / struct-list families
	//	(real KEYED data exists in m_modifiers but is not reconstructed into the legacy keyed shape this pass).
	//	NOTE: PURE_TRAITS value-filtering is a cascade/consumer concern (modifier.md §4), NEVER applied here.
	// ============================================================================================================

	// --- scalar modifier getters (REAL; curate_trait.py SCALAR / STATE_RELIGION tables) ---
	int getHealth() const { return m_iHealth; }                                            // health.empire.flat
	int getHappiness() const { return m_iHappiness; }                                      // happiness.empire.flat
	int getMaxAnarchy() const { return m_iMaxAnarchy; }                                     // identity.maxAnarchy (default -1)
	int getMinAnarchy() const { return m_iMinAnarchy; }                                     // identity.minAnarchy (default 0)
	int getUpkeepModifier() const { return m_iUpkeepModifier; }                             // upkeep.empire.civic.percent
	int getLevelExperienceModifier() const { return m_iLevelExperienceModifier; }          // experience.empire.levelModifier.percent
	int getGreatPeopleRateModifier() const { return m_iGreatPeopleRateModifier; }          // greatPeopleRate.empire.percent
	int getGreatGeneralRateModifier() const { return m_iGreatGeneralRateModifier; }        // greatGeneralRate.empire.percent
	int getDomesticGreatGeneralRateModifier() const { return m_iDomesticGreatGeneralRateModifier; } // greatGeneralRate.empire.domestic.percent
	int getMaxGlobalBuildingProductionModifier() const { return m_iMaxGlobalBuildingProductionModifier; } // buildRate.empire.worldWonder.percent
	int getMaxTeamBuildingProductionModifier() const { return m_iMaxTeamBuildingProductionModifier; }     // buildRate.empire.teamWonder.percent
	int getMaxPlayerBuildingProductionModifier() const { return m_iMaxPlayerBuildingProductionModifier; } // buildRate.empire.nationalWonder.percent
	int getWarWearinessAccumulationModifier() const { return m_iWarWearinessAccumulationModifier; }       // diplomacy.empire.warWeariness.percent
	int getCivicAnarchyTimeModifier() const { return m_iCivicAnarchyTimeModifier; }        // durations.empire.civicAnarchy.percent
	int getReligiousAnarchyTimeModifier() const { return m_iReligiousAnarchyTimeModifier; }// durations.empire.religiousAnarchy.percent
	int getImprovementUpgradeRateModifier() const { return m_iImprovementUpgradeRateModifier; } // improvementUpgradeRate.empire.percent
	int getWorkerSpeedModifier() const { return m_iWorkerSpeedModifier; }                   // workRate.empire.percent
	int getMaxConscript() const { return m_iMaxConscript; }                                 // conscript.empire.flat
	int getDistanceMaintenanceModifier() const { return m_iDistanceMaintenanceModifier; }   // maintenance.empire.distance.percent
	int getNumCitiesMaintenanceModifier() const { return m_iNumCitiesMaintenanceModifier; } // maintenance.empire.numCities.percent
	int getCorporationMaintenanceModifier() const { return m_iCorporationMaintenanceModifier; } // maintenance.empire.corporation.percent
	int getStateReligionGreatPeopleRateModifier() const { return m_iStateReligionGreatPeopleRateModifier; } // stateReligion.empire.greatPeopleRate.percent
	int getFreeExperience() const { return m_iFreeExperience; }                             // experience.empire.flat
	int getFreeUnitUpkeepCivilian() const { return m_iFreeUnitUpkeepCivilian; }             // upkeep.empire.freeCivilian.flat
	int getFreeUnitUpkeepMilitary() const { return m_iFreeUnitUpkeepMilitary; }             // upkeep.empire.freeMilitary.flat
	int getFreeUnitUpkeepCivilianPopPercent() const { return m_iFreeUnitUpkeepCivilianPopPercent; } // upkeep.empire.freeCivilian.perPopulation
	int getFreeUnitUpkeepMilitaryPopPercent() const { return m_iFreeUnitUpkeepMilitaryPopPercent; } // upkeep.empire.freeMilitary.perPopulation
	int getCivilianUnitUpkeepMod() const { return m_iCivilianUnitUpkeepMod; }               // upkeep.empire.unitCivilian.percent
	int getMilitaryUnitUpkeepMod() const { return m_iMilitaryUnitUpkeepMod; }               // upkeep.empire.unitMilitary.percent
	int getFreeSpecialist() const { return m_iFreeSpecialist; }                             // freeSpecialists.empire.any (0 when merged into a per-wonder list)
	int getTradeRoutes() const { return m_iTradeRoutes; }                                   // tradeRoutes.empire.flat
	int getStateReligionHappiness() const { return m_iStateReligionHappiness; }             // stateReligion.empire.happiness.flat
	int getNonStateReligionHappiness() const { return m_iNonStateReligionHappiness; }       // happiness.empire.nonStateReligion.flat
	int getStateReligionUnitProductionModifier() const { return m_iStateReligionUnitProductionModifier; }     // stateReligion.empire.unitProduction.percent
	int getStateReligionBuildingProductionModifier() const { return m_iStateReligionBuildingProductionModifier; } // stateReligion.empire.buildingProduction.percent
	int getStateReligionFreeExperience() const { return m_iStateReligionFreeExperience; }   // stateReligion.empire.freeExperience.flat
	int getExpInBorderModifier() const { return m_iExpInBorderModifier; }                   // experience.empire.inBorder.percent
	int getCityDefenseBonus() const { return m_iCityDefenseBonus; }                         // defense.empire.amount.percent
	int getMilitaryProductionModifier() const { return m_iMilitaryProductionModifier; }     // buildRate.empire.military.percent
	int getAttitudeModifier() const { return m_iAttitudeModifier; }                         // diplomacy.empire.attitude.flat
	int getEspionageDefense() const { return m_iEspionageDefense; }                         // combat.empire.espionageDefense.percent
	int getMaxTradeRoutesChange() const { return m_iMaxTradeRoutesChange; }                 // tradeRoutes.empire.max.flat
	int getGoldenAgeDurationModifier() const { return m_iGoldenAgeDurationModifier; }       // goldenAge.empire.percent
	int getGreatPeopleRateChange() const { return m_iGreatPeopleRateChange; }               // greatPeopleRate.empire.flat (untyped national pool)
	int getHurryAngerModifier() const { return m_iHurryAngerModifier; }                     // hurry.empire.anger.percent
	int getHurryCostModifier() const { return m_iHurryCostModifier; }                       // hurry.empire.cost.percent
	int getEnemyWarWearinessModifier() const { return m_iEnemyWarWearinessModifier; }       // diplomacy.empire.enemyWarWeariness.percent
	int getForeignTradeRouteModifier() const { return m_iForeignTradeRouteModifier; }       // tradeRoutes.empire.foreign.percent
	int getBombardDefense() const { return m_iBombardDefense; }                             // defense.empire.bombardDefense.percent
	int getUnitUpgradePriceModifier() const { return m_iUnitUpgradePriceModifier; }         // upkeep.empire.upgradePrice.percent
	int getCoastalTradeRoutes() const { return m_iCoastalTradeRoutes; }                     // tradeRoutes.empire.coastal.flat
	int getGlobalPopulationgrowthratepercentage() const { return m_iGlobalPopulationgrowthratepercentage; } // growth.empire.percent
	int getCityStartCulture(bool /*bForLoad*/ = false) const { return m_iCityStartCulture; }// cityFounding.empire.startCulture.flat
	int getGlobalAirUnitCapacity() const { return m_iGlobalAirUnitCapacity; }               // unitCapability.empire.airUnitCapacity.flat
	int getHolyCityofStateReligionXPModifier() const { return m_iHolyCityofStateReligionXPModifier; } // stateReligion.empire.holyCityXP.percent
	int getBonusPopulationinNewCities() const { return m_iBonusPopulationinNewCities; }     // cityFounding.empire.startPopulation.flat
	int getMissileRange() const { return m_iMissileRange; }                                 // combat.empire.missileRange.flat
	int getFlightOperationRange() const { return m_iFlightOperationRange; }                 // combat.empire.flightRange.flat
	int getNavalCargoSpace() const { return m_iNavalCargoSpace; }                           // combat.empire.navalCargo.flat
	int getMissileCargoSpace() const { return m_iMissileCargoSpace; }                       // combat.empire.missileCargo.flat
	int getNationalCaptureProbabilityModifier() const { return m_iNationalCaptureProbabilityModifier; } // combat.empire.captureProbability.percent
	int getNationalCaptureResistanceModifier() const { return m_iNationalCaptureResistanceModifier; }   // combat.empire.captureResistance.percent
	int getStateReligionSpreadProbabilityModifier() const { return m_iStateReligionSpreadProbabilityModifier; }       // stateReligion.empire.spreadProbability.percent
	int getNonStateReligionSpreadProbabilityModifier() const { return m_iNonStateReligionSpreadProbabilityModifier; } // stateReligion.empire.nonStateSpreadProbability.percent
	int getFreedomFighterChange() const { return m_iFreedomFighterChange; }                 // revolution.empire.freedomFighter.flat
	int getLinePriority() const { return m_iLinePriority; }                                 // succession.priority (developing-leaders line ordering)
	int getFlavorValue(int i) const                                                         // ai.flavours {FLAVOR:int}
	{ std::map<int, int>::const_iterator it = m_flavours.find(i); return it != m_flavours.end() ? it->second : 0; }

	// --- positional yield/commerce arrays (REAL; curate_trait.py SPLIT_ARRAY / GROUPED_YIELD_ARRAY) ---
	int getCommerceChange(int i) const   { return (i >= 0 && i < NUM_COMMERCE_TYPES) ? m_aiCommerceChange[i] : 0; }   // {commerce}.empire.flat
	int getCommerceModifier(int i) const { return (i >= 0 && i < NUM_COMMERCE_TYPES) ? m_aiCommerceModifier[i] : 0; } // {commerce}.empire.percent
	int getYieldChange(int i) const      { return (i >= 0 && i < NUM_YIELD_TYPES) ? m_aiYieldChange[i] : 0; }         // {yield}.empire.flat
	int getYieldModifier(int i) const    { return (i >= 0 && i < NUM_YIELD_TYPES) ? m_aiYieldModifier[i] : 0; }       // {yield}.empire.percent
	int getTradeYieldModifier(int i) const { return (i >= 0 && i < NUM_YIELD_TYPES) ? m_aiTradeYieldModifier[i] : 0; }// {yield}.empire.tradeRoute.percent
	int getExtraYieldThreshold(int i) const { return (i >= 0 && i < NUM_YIELD_TYPES) ? m_aiExtraYieldThreshold[i] : -1; } // extraYieldThreshold.empire.{yield}.flat (default -1)
	int getLessYieldThreshold(int i) const  { return (i >= 0 && i < NUM_YIELD_TYPES) ? m_aiLessYieldThreshold[i] : 0; }   // lessYieldThreshold.empire.{yield}.flat
	int getSpecialistExtraYield(int i) const { return (i >= 0 && i < NUM_YIELD_TYPES) ? m_aiSpecialistExtraYield[i] : 0; } // {yield}.empire.specialist.perSpecialist
	int getSpecialistExtraCommerce(int i) const { return (i >= 0 && i < NUM_COMMERCE_TYPES) ? m_aiSpecialistExtraCommerce[i] : 0; } // {commerce}.empire.specialist.perSpecialist
	int getGoldenAgeYieldChanges(int i) const { return (i >= 0 && i < NUM_YIELD_TYPES) ? m_aiGoldenAgeYieldChange[i] : 0; }         // {yield}.empire.goldenAge.flat
	int getGoldenAgeCommerceChanges(int i) const { return (i >= 0 && i < NUM_COMMERCE_TYPES) ? m_aiGoldenAgeCommerceChange[i] : 0; }// {commerce}.empire.goldenAge.flat

	// --- policy STATE flags (REAL; the §9 `policies` bool block, curate_trait.py POLICIES) ---
	bool isNonStateReligionCommerce() const { return m_policies.has("nonStateReligionCommerce"); }
	bool isUpgradeAnywhere() const          { return m_policies.has("upgradeAnywhere"); }
	bool isMilitaryFoodProduction() const   { return m_policies.has("militaryFoodProduction"); }
	bool isAllowsInquisitions() const       { return m_policies.has("allowInquisitions"); }
	bool isCitiesStartwithStateReligion() const { return m_policies.has("citiesStartWithStateReligion"); }
	bool isDraftsOnCityCapture() const      { return m_policies.has("draftsOnCityCapture"); }
	bool isExtraGoody() const               { return m_policies.has("extraGoody"); }
	bool isAllReligionsActive() const       { return m_policies.has("allReligionsActive"); }
	bool isBansNonStateReligions() const    { return m_policies.has("bansNonStateReligions"); }
	bool isFreedomFighter() const           { return m_policies.has("freedomFighter"); }

	// --- identity flags (REAL; curate_trait.py IDENTITY_FLAGS) ---
	bool isCivilizationTrait() const { return civilizationTrait; }                          // identity.civilizationTrait
	inline bool isNegativeTrait() const { return negativeTrait; }                           // identity.negativeTrait
	bool isImpurePropertyManipulators() const { return m_bImpurePropertyManipulators; }     // identity.impurePropertyManipulators
	bool isImpurePromotions() const { return m_bImpurePromotions; }                         // identity.impurePromotions
	bool isBarbarianSelectionOnly() const { return m_bBarbarianSelectionOnly; }             // identity.barbarianSelectionOnly

	// identity.shortDescription -- a TXT_KEY string (resolved by the caller via gDLL->getText, CvGameTextMgr.cpp
	// precedent); "" when unmapped (CvJsonInfo's m_szTextKey is a localized CvWString -- a different type/meaning).
	const char* getShortDescription() const { return m_szShortDescription.c_str(); }

	// STUB empty -- property engine, XML-era manipulator data deferred (CvJsonCorporationInfo precedent).
	const CvPropertyManipulators* getPropertyManipulators() const { return &m_PropertyManipulators; }

	// --- RevIdx* (RevolutionDCM) -- REAL (curate_trait.py SCALAR -> revolution.empire.<member>.<unit>; fRev* floats) ---
	int getRevIdxLocal() const { return m_iRevIdxLocal; }                       // revolution.empire.local.flat
	int getRevIdxNational() const { return m_iRevIdxNational; }                 // revolution.empire.national.flat
	int getRevIdxDistanceModifier() const { return m_iRevIdxDistanceModifier; }// revolution.empire.distanceModifier.percent
	int getRevIdxHolyCityGood() const { return m_iRevIdxHolyCityGood; }        // revolution.empire.holyCityGood.flat
	int getRevIdxHolyCityBad() const { return m_iRevIdxHolyCityBad; }          // revolution.empire.holyCityBad.flat
	float getRevIdxNationalityMod() const { return m_fRevIdxNationalityMod; }  // revolution.empire.nationalityMod.percent
	float getRevIdxBadReligionMod() const { return m_fRevIdxBadReligionMod; }  // revolution.empire.badReligionMod.percent
	float getRevIdxGoodReligionMod() const { return m_fRevIdxGoodReligionMod; }// revolution.empire.goodReligionMod.percent

	// --- conditioned / ranked / unit-qualified leaves -- REAL (the legacy getter returns the raw magnitude; the legacy
	// consumer applies the predicate/ranked gate itself). capital = `{fam}.empire.percent` + enabled:IS_CAPITAL;
	// holyCity-nonState = enabled:{all:[IS_HOLY_CITY, !IS_STATE_RELIGION_HOLY_CITY]}; largestCity = the bare-number
	// `happiness.empire.cities.flat` (ranked siblings max/orderedBy); happyPerMil = unit:IS_MILITARY entry on the same
	// leaf; seaPlot = `{yield}.empire.plots.flat` + enabled:IS_WATER. (See mapFrom.) ---
	int getCapitalXPModifier() const { return m_iCapitalXPModifier; }
	int getHolyCityofNonStateReligionXPModifier() const { return m_iHolyCityofNonStateReligionXPModifier; }
	int getLargestCityHappiness() const { return m_iLargestCityHappiness; }
	int getHappyPerMilitaryUnit() const { return m_iHappyPerMilitaryUnit; }
	int getSeaPlotYieldChanges(int i) const { return (i >= 0 && i < NUM_YIELD_TYPES) ? m_aiSeaPlotYieldChanges[i] : 0; }
	int getCapitalYieldModifier(int i) const { return (i >= 0 && i < NUM_YIELD_TYPES) ? m_aiCapitalYieldModifier[i] : 0; }
	int getCapitalCommerceModifier(int i) const { return (i >= 0 && i < NUM_COMMERCE_TYPES) ? m_aiCapitalCommerceModifier[i] : 0; }

	// --- STUB game-option gate getters ([DEC-json-not-cascade] -- a pure-DATA poco never reads GC.getGame().isOption;
	// OnGameOptions/NotOnGameOptions/Categories are curator-DROPPED, the simple/complex folder split IS the gate). ---
	bool isValidTrait(bool /*bGameStart*/ = false) const { return false; }
	int getNotOnGameOption(int /*i*/) const { return -1; }
	int getNumNotOnGameOptions() const { return 0; }
	bool isNotOnGameOption(int /*i*/) const { return false; }
	int getOnGameOption(int /*i*/) const { return -1; }
	int getNumOnGameOptions() const { return 0; }
	bool isOnGameOption(int /*i*/) const { return false; }
	int getCategory(int /*i*/) const { return -1; }
	int getNumCategories() const { return 0; }
	bool isCategory(int /*i*/) const { return false; }

	// --- textual-reference FKs ---
	// STUB prereqs: curator store-INVERTS TraitPrereq/PrereqTech onto the prereq entity's enables.traits (DROP set), so
	// they are NOT authored on this trait -> NO_TRAIT/NO_TECH. NOT a curator-gap (the data lives on the other entity).
	TraitTypes getPrereqTrait() const { return NO_TRAIT; }
	TraitTypes getPrereqOrTrait1() const { return NO_TRAIT; }
	TraitTypes getPrereqOrTrait2() const { return NO_TRAIT; }
	TechTypes getPrereqTech() const { return NO_TECH; }
	// REAL: succession.promotionLine / grants.eraAdvanceFreeSpecialist / grants.goldenAgeOnBirthOfGreatPerson /
	// greatPeopleRate.empire.units.{UNIT} (see mapFrom).
	PromotionLineTypes getPromotionLine() const { return m_ePromotionLine; }
	SpecialistTypes getEraAdvanceFreeSpecialistType() const { return m_eEraAdvanceFreeSpecialistType; }
	int getGreatPeopleUnitType() const { return m_iGreatPeopleUnitType; }
	int getGoldenAgeonBirthofGreatPeopleType() const { return m_iGoldenAgeonBirthofGreatPeopleType; }

	// --- ai.behaviour.coastalAIInfluence -- REAL (curate_trait.py bCoastalAIInfluence) ---
	bool isCoastalAIInfluence() const { return m_bCoastalAIInfluence; }

	// --- FreeSpecialistPer{World,National,Team}Wonder -- REAL, recovered from the reclassified per-wonder MODIFIER
	// (curate_trait.py FREE_SPEC_PER_WONDER: freeSpecialists.empire.any list entry with per.type == <WONDER_TOKEN>). ---
	bool isFreeSpecialistperWorldWonder() const { return m_bFreeSpecialistperWorldWonder; }
	bool isFreeSpecialistperNationalWonder() const { return m_bFreeSpecialistperNationalWonder; }
	bool isFreeSpecialistperTeamProject() const { return m_bFreeSpecialistperTeamProject; }

	// --- 2D specialist/improvement yield/commerce -- REAL (per actual type id, read live off the parsed m_modifiers by
	// the target's GC type string; see .cpp). The int* bulk-array views stay NULL (no live cascade consumer -- the
	// legacy copyNonDefaults path; the per-index getters above carry the data). ---
	int getSpecialistYieldChange(int i, int j) const;                          // {yield}.empire.specialists.{SPEC}.flat
	int getSpecialistCommerceChange(int i, int j) const;                       // {commerce}.empire.specialists.{SPEC}.flat
	int getImprovementYieldChange(int i, int j) const;                         // {yield}.empire.improvements.{IMP}.flat
	bool isAnySpecialistYieldChanges() const;
	bool isAnySpecialistCommerceChanges() const;
	int* getSpecialistYieldChangeArray(int /*i*/) const { return NULL; }
	int* getSpecialistCommerceChangeArray(int /*i*/) const { return NULL; }
	int* getImprovementYieldChangeArray(int /*i*/) const { return NULL; }

	// --- int* positional-array views (bulk-copy accessors) -- return the backing member array (the per-index getters
	// above hold the same data). const_cast mirrors the archived non-const int* return; callers only read for copy. ---
	int* getYieldModifierArray() const { return const_cast<int*>(m_aiYieldModifier); }
	int* getCapitalYieldModifierArray() const { return const_cast<int*>(m_aiCapitalYieldModifier); }
	int* getCapitalCommerceModifierArray() const { return const_cast<int*>(m_aiCapitalCommerceModifier); }
	int* getSpecialistExtraCommerceArray() const { return const_cast<int*>(m_aiSpecialistExtraCommerce); }
	int* getSpecialistExtraYieldArray() const { return const_cast<int*>(m_aiSpecialistExtraYield); }
	int* getSeaPlotYieldChangesArray() const { return const_cast<int*>(m_aiSeaPlotYieldChanges); }
	int* getGoldenAgeYieldChangesArray() const { return const_cast<int*>(m_aiGoldenAgeYieldChange); }
	int* getGoldenAgeCommerceChangesArray() const { return const_cast<int*>(m_aiGoldenAgeCommerceChange); }

	// --- target-KEYED families the consumer iterates by list-index -- REAL, enumerated into typed std::vectors in
	// mapFrom (buildRate.empire.{buildings|units|specialUnits|unitCombats}.percent, experience.empire.unitCombats.flat,
	// upkeep.empire.civicOptions.enabler, top-level `excludes`). getNum + indexed accessor over the materialized list. ---
	int getNumBuildingProductionModifiers() const { return (int)m_aBuildingProductionModifiers.size(); }
	BuildingModifier getBuildingProductionModifier(int i) const
	{ if (i >= 0 && i < (int)m_aBuildingProductionModifiers.size()) return m_aBuildingProductionModifiers[i]; BuildingModifier m; m.eBuilding = NO_BUILDING; m.iModifier = 0; return m; }
	int getNumUnitProductionModifiers() const { return (int)m_aUnitProductionModifiers.size(); }
	UnitModifier getUnitProductionModifier(int i) const
	{ if (i >= 0 && i < (int)m_aUnitProductionModifiers.size()) return m_aUnitProductionModifiers[i]; UnitModifier m; m.eUnit = NO_UNIT; m.iModifier = 0; return m; }
	int getNumSpecialUnitProductionModifiers() const { return (int)m_aSpecialUnitProductionModifiers.size(); }
	SpecialUnitModifier getSpecialUnitProductionModifier(int i) const
	{ if (i >= 0 && i < (int)m_aSpecialUnitProductionModifiers.size()) return m_aSpecialUnitProductionModifiers[i]; SpecialUnitModifier m; m.eSpecialUnit = NO_SPECIALUNIT; m.iModifier = 0; return m; }
	int getNumUnitCombatFreeExperiences() const { return (int)m_aUnitCombatFreeExperiences.size(); }
	UnitCombatModifier getUnitCombatFreeExperience(int i) const
	{ if (i >= 0 && i < (int)m_aUnitCombatFreeExperiences.size()) return m_aUnitCombatFreeExperiences[i]; UnitCombatModifier m; m.eUnitCombat = NO_UNITCOMBAT; m.iModifier = 0; return m; }
	int getNumUnitCombatProductionModifiers() const { return (int)m_aUnitCombatProductionModifiers.size(); }
	UnitCombatModifier getUnitCombatProductionModifier(int i) const
	{ if (i >= 0 && i < (int)m_aUnitCombatProductionModifiers.size()) return m_aUnitCombatProductionModifiers[i]; UnitCombatModifier m; m.eUnitCombat = NO_UNITCOMBAT; m.iModifier = 0; return m; }
	int getNumCivicOptionNoUpkeepTypes() const { return (int)m_aCivicOptionNoUpkeepTypes.size(); }
	CivicOptionTypeBool isCivicOptionNoUpkeepType(int i) const
	{ if (i >= 0 && i < (int)m_aCivicOptionNoUpkeepTypes.size()) return m_aCivicOptionNoUpkeepTypes[i]; CivicOptionTypeBool c; c.eCivicOption = NO_CIVICOPTION; c.bBool = false; return c; }
	int getNumDisallowedTraitTypes() const { return (int)m_aDisallowedTraitTypes.size(); }
	DisallowedTraitType isDisallowedTraitType(int i) const
	{ if (i >= 0 && i < (int)m_aDisallowedTraitTypes.size()) return m_aDisallowedTraitTypes[i]; DisallowedTraitType d; d.eTrait = NO_TRAIT; return d; }

	// isFreePromotionUnitCombats(iPromotion, iUnitCombat) -- REAL (grants.freePromotions {PROMOTION:[UNITCOMBAT,...]};
	// archived indexing is [promotion][unitCombat], consumer CvPlayerAI.cpp:23744). PURE_TRAITS filtering is a
	// consumer concern ([DEC-json-not-cascade]), not applied here.
	bool isFreePromotionUnitCombats(int i, int j) const
	{ std::map<int, std::set<int> >::const_iterator it = m_freePromotionUnitCombats.find(i); return it != m_freePromotionUnitCombats.end() && it->second.count(j) != 0; }

	// --- reference-returning IDValueMap<X,int>[::filtered] getters -- REAL, populated in mapFrom via IDValueMap::setValue
	// per keyed entry (improvementUpgradeRate.empire.improvements.{I}.percent, workRate.empire.builds.{B}.percent,
	// experience.empire.domains.{D}.flat, buildRate.empire.{domains,specialBuildings}.{X}.percent,
	// researchRate.empire.techs.{T}.percent, happiness.empire.buildings.{B}.flat, and the per-bonus
	// happiness.empire.flat entries). The `filtered` view is an always-pass view over the now-populated member (no
	// PURE_TRAITS runtime gate -- a consumer concern, modifier.md §4). ---
	const IDValueMap<BuildingTypes, int>& getBuildingHappinessModifiers() const { return m_aBuildingHappinessModifiers; }
	const IDValueMap<BuildingTypes, int>::filtered getBuildingHappinessModifiersFiltered() const;
	const IDValueMap<ImprovementTypes, int>::filtered getImprovementUpgradeModifiers() const;
	const IDValueMap<BuildTypes, int>::filtered getBuildWorkerSpeedModifiers() const;
	const IDValueMap<DomainTypes, int>::filtered getDomainFreeExperience() const;
	const IDValueMap<DomainTypes, int>::filtered getDomainProductionModifiers() const;
	const IDValueMap<TechTypes, int>::filtered getTechResearchModifiers() const;
	const IDValueMap<SpecialBuildingTypes, int>::filtered getSpecialBuildingProductionModifiers() const;
	const IDValueMap<BonusTypes, int>::filtered getBonusHappinessChanges() const;

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
	CvJsonEdges     m_edges;
	CvJsonGrants    m_grants;
	CvJsonModifiers m_modifiers;
	CvJsonBoolBlock m_policies;

	// scalar modifier values (REAL)
	int m_iHealth, m_iHappiness, m_iMaxAnarchy, m_iMinAnarchy, m_iUpkeepModifier, m_iLevelExperienceModifier;
	int m_iGreatPeopleRateModifier, m_iGreatGeneralRateModifier, m_iDomesticGreatGeneralRateModifier;
	int m_iMaxGlobalBuildingProductionModifier, m_iMaxTeamBuildingProductionModifier, m_iMaxPlayerBuildingProductionModifier;
	int m_iWarWearinessAccumulationModifier, m_iCivicAnarchyTimeModifier, m_iReligiousAnarchyTimeModifier;
	int m_iImprovementUpgradeRateModifier, m_iWorkerSpeedModifier, m_iMaxConscript;
	int m_iDistanceMaintenanceModifier, m_iNumCitiesMaintenanceModifier, m_iCorporationMaintenanceModifier;
	int m_iStateReligionGreatPeopleRateModifier, m_iFreeExperience;
	int m_iFreeUnitUpkeepCivilian, m_iFreeUnitUpkeepMilitary, m_iFreeUnitUpkeepCivilianPopPercent, m_iFreeUnitUpkeepMilitaryPopPercent;
	int m_iCivilianUnitUpkeepMod, m_iMilitaryUnitUpkeepMod, m_iFreeSpecialist, m_iTradeRoutes;
	int m_iStateReligionHappiness, m_iNonStateReligionHappiness, m_iStateReligionUnitProductionModifier;
	int m_iStateReligionBuildingProductionModifier, m_iStateReligionFreeExperience, m_iExpInBorderModifier;
	int m_iCityDefenseBonus, m_iMilitaryProductionModifier, m_iAttitudeModifier, m_iEspionageDefense;
	int m_iMaxTradeRoutesChange, m_iGoldenAgeDurationModifier, m_iGreatPeopleRateChange, m_iHurryAngerModifier;
	int m_iHurryCostModifier, m_iEnemyWarWearinessModifier, m_iForeignTradeRouteModifier, m_iBombardDefense;
	int m_iUnitUpgradePriceModifier, m_iCoastalTradeRoutes, m_iGlobalPopulationgrowthratepercentage;
	int m_iCityStartCulture, m_iGlobalAirUnitCapacity, m_iHolyCityofStateReligionXPModifier;
	int m_iBonusPopulationinNewCities, m_iMissileRange, m_iFlightOperationRange, m_iNavalCargoSpace, m_iMissileCargoSpace;
	int m_iNationalCaptureProbabilityModifier, m_iNationalCaptureResistanceModifier;
	int m_iStateReligionSpreadProbabilityModifier, m_iNonStateReligionSpreadProbabilityModifier;
	int m_iFreedomFighterChange, m_iLinePriority;
	// RevIdx* (REAL; RevolutionDCM -- fRev* are floats)
	int m_iRevIdxLocal, m_iRevIdxNational, m_iRevIdxDistanceModifier, m_iRevIdxHolyCityGood, m_iRevIdxHolyCityBad;
	float m_fRevIdxNationalityMod, m_fRevIdxBadReligionMod, m_fRevIdxGoodReligionMod;
	// textual-reference FKs (REAL; succession / grants / greatPeopleRate.units)
	int m_iGreatPeopleUnitType, m_iGoldenAgeonBirthofGreatPeopleType;
	PromotionLineTypes m_ePromotionLine;
	SpecialistTypes m_eEraAdvanceFreeSpecialistType;
	// conditioned / ranked / unit-qualified leaves (REAL; the consumer applies the gate)
	int m_iCapitalXPModifier, m_iHolyCityofNonStateReligionXPModifier, m_iLargestCityHappiness, m_iHappyPerMilitaryUnit;
	int m_aiCapitalYieldModifier[NUM_YIELD_TYPES];
	int m_aiCapitalCommerceModifier[NUM_COMMERCE_TYPES];
	int m_aiSeaPlotYieldChanges[NUM_YIELD_TYPES];
	// AI-behaviour flag + reclassified per-wonder free-specialist flags + freePromotions map (REAL)
	bool m_bCoastalAIInfluence, m_bFreeSpecialistperWorldWonder, m_bFreeSpecialistperNationalWonder, m_bFreeSpecialistperTeamProject;
	std::map<int, std::set<int> > m_freePromotionUnitCombats;   // promotion id -> unit-combat ids (grants.freePromotions)
	// positional arrays (REAL)
	int m_aiCommerceChange[NUM_COMMERCE_TYPES];
	int m_aiCommerceModifier[NUM_COMMERCE_TYPES];
	int m_aiSpecialistExtraCommerce[NUM_COMMERCE_TYPES];
	int m_aiGoldenAgeCommerceChange[NUM_COMMERCE_TYPES];
	int m_aiYieldChange[NUM_YIELD_TYPES];
	int m_aiYieldModifier[NUM_YIELD_TYPES];
	int m_aiTradeYieldModifier[NUM_YIELD_TYPES];
	int m_aiExtraYieldThreshold[NUM_YIELD_TYPES];
	int m_aiLessYieldThreshold[NUM_YIELD_TYPES];
	int m_aiSpecialistExtraYield[NUM_YIELD_TYPES];
	int m_aiGoldenAgeYieldChange[NUM_YIELD_TYPES];
	std::map<int, int> m_flavours;                    // FlavorTypes -> weight (ai.flavours)
	// identity flags (REAL)
	bool m_bImpurePropertyManipulators, m_bImpurePromotions, m_bBarbarianSelectionOnly;
	std::string m_szShortDescription;
	// target-KEYED list-index families materialized from JSON (REAL; enumerated in mapFrom)
	std::vector<BuildingModifier>     m_aBuildingProductionModifiers;    // buildRate.empire.buildings.{B}.percent
	std::vector<UnitModifier>         m_aUnitProductionModifiers;        // buildRate.empire.units.{U}.percent
	std::vector<SpecialUnitModifier>  m_aSpecialUnitProductionModifiers; // buildRate.empire.specialUnits.{SU}.percent
	std::vector<UnitCombatModifier>   m_aUnitCombatProductionModifiers;  // buildRate.empire.unitCombats.{UC}.percent
	std::vector<UnitCombatModifier>   m_aUnitCombatFreeExperiences;      // experience.empire.unitCombats.{UC}.flat
	std::vector<CivicOptionTypeBool>  m_aCivicOptionNoUpkeepTypes;       // upkeep.empire.civicOptions.{CO}.enabler
	std::vector<DisallowedTraitType>  m_aDisallowedTraitTypes;           // top-level `excludes`
	// STUB empty members (keep reference/filtered/property-engine getters safe)
	CvPropertyManipulators m_PropertyManipulators;
	IDValueMap<BuildingTypes, int>        m_aBuildingHappinessModifiers;
	IDValueMap<ImprovementTypes, int>     m_aImprovementUpgradeModifierTypes;
	IDValueMap<BuildTypes, int>           m_aBuildWorkerSpeedModifierTypes;
	IDValueMap<DomainTypes, int>          m_aDomainFreeExperiences;
	IDValueMap<DomainTypes, int>          m_aDomainProductionModifiers;
	IDValueMap<TechTypes, int>            m_aTechResearchModifiers;
	IDValueMap<SpecialBuildingTypes, int> m_aSpecialBuildingProductionModifiers;
	IDValueMap<BonusTypes, int>           m_aBonusHappinessChanges;
};

#endif // CV_JSON_TRAIT_INFO_H
