#pragma once

#ifndef CV_HANDICAP_INFO_H
#define CV_HANDICAP_INFO_H

#include "CvInfo.h"   // JSON-info base (mapFrom); on /I -> bare include

namespace picojson { class value; }

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  class : CvHandicapInfo
//
//  DESC:   A difficulty level. #430: JSON-fed (Assets/Data/handicaps/*.json via
//          mapFrom); no XML read. A CONFIG entity (enables nothing) -- modifiers only,
//          carrying the pervasive human/AI dual-leaf duality (handicaps.md / curate_handicap.py).
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
class CvHandicapInfo : public CvInfo
{
	//---------------------------PUBLIC INTERFACE---------------------------------
public:

	CvHandicapInfo();

	int getFreeWinsVsBarbs() const { return m_iFreeWinsVsBarbs; }
	int getAnimalAttackProb() const { return m_iAnimalAttackProb; }
	int getAdvancedStartPointsMod() const { return m_iAdvancedStartPointsMod; }
	int getStartingGold() const { return m_iStartingGold; }
	int getUnitUpkeepPercent() const { return m_iUnitUpkeepPercent; }
	int getDistanceMaintenancePercent() const { return m_iDistanceMaintenancePercent; }
	int getNumCitiesMaintenancePercent() const { return m_iNumCitiesMaintenancePercent; }
	int getColonyMaintenancePercent() const { return m_iColonyMaintenancePercent; }
	int getMaxColonyMaintenance() const { return m_iMaxColonyMaintenance; }
	int getCorporationMaintenancePercent() const { return m_iCorporationMaintenancePercent; }
	int getCivicUpkeepPercent() const { return m_iCivicUpkeepPercent; }
	int getInflationPercent() const { return m_iInflationPercent; }
	int getRevolutionIndexPercent() const { return m_iRevolutionIndexPercent; }
	int getHealthBonus() const { return m_iHealthBonus; }
	int getHappyBonus() const { return m_iHappyBonus; }
	int getAttitudeChange() const { return m_iAttitudeChange; }
	int getNoTechTradeModifier() const { return m_iNoTechTradeModifier; }
	int getTechTradeKnownModifier() const { return m_iTechTradeKnownModifier; }
	int getUnownedWaterTilesPerBarbarianUnit() const { return m_iUnownedWaterTilesPerBarbarianUnit; }
	int getUnownedTilesPerBarbarianCity() const { return m_iUnownedTilesPerBarbarianCity; }
	int getBarbarianCityCreationTurnsElapsed() const { return m_iBarbarianCityCreationTurnsElapsed; }
	int getBarbarianCityCreationProb() const { return m_iBarbarianCityCreationProb; }
	int getAnimalCombatModifier() const { return m_iAnimalCombatModifier; }
	int getBarbarianCombatModifier() const { return m_iBarbarianCombatModifier; }
	int getAIAnimalCombatModifier() const { return m_iAIAnimalCombatModifier; }
	int getSubdueAnimalBonusAI() const { return m_iSubdueAnimalBonusAI; }
	int getAIBarbarianCombatModifier() const { return m_iAIBarbarianCombatModifier; }

	int getStartingDefenseUnits() const { return m_iStartingDefenseUnits; }
	int getStartingWorkerUnits() const { return m_iStartingWorkerUnits; }
	int getStartingExploreUnits() const { return m_iStartingExploreUnits; }
	int getAIStartingDefenseUnits() const { return m_iAIStartingDefenseUnits; }
	int getAIStartingWorkerUnits() const { return m_iAIStartingWorkerUnits; }
	int getAIStartingExploreUnits() const { return m_iAIStartingExploreUnits; }
	int getBarbarianInitialDefenders() const { return m_iBarbarianInitialDefenders; }
	int getAIDeclareWarProb() const { return m_iAIDeclareWarProb; }
	int getAIWorkRateModifier() const { return m_iAIWorkRateModifier; }
	int getAIGrowthPercent() const { return m_iAIGrowthPercent; }
	int getAITrainPercent() const { return m_iAITrainPercent; }
	int getAIWorldTrainPercent() const { return m_iAIWorldTrainPercent; }
	int getAIConstructPercent() const { return m_iAIConstructPercent; }
	int getAIWorldConstructPercent() const { return m_iAIWorldConstructPercent; }
	int getAICreatePercent() const { return m_iAICreatePercent; }
	int getAIResearchPercent() const { return m_iAIResearchPercent; }
	int getAIWorldCreatePercent() const { return m_iAIWorldCreatePercent; }
	int getAICivicUpkeepPercent() const { return m_iAICivicUpkeepPercent; }
	int getAIUnitUpkeepPercent() const { return m_iAIUnitUpkeepPercent; }
	int getAIUnitSupplyPercent() const { return m_iAIUnitSupplyPercent; }
	int getAIUnitUpgradePercent() const { return m_iAIUnitUpgradePercent; }
	int getAIInflationPercent() const { return m_iAIInflationPercent; }
	int getAIWarWearinessPercent() const { return m_iAIWarWearinessPercent; }
	int getAIPerEraModifier() const { return m_iAIPerEraModifier; }
	int getAIAdvancedStartPercent() const { return m_iAIAdvancedStartPercent; }

	int getNumGoodies() const { return (int)m_piGoodies.size(); }
	int getGoodies(int i) const { return (i >= 0 && i < (int)m_piGoodies.size()) ? m_piGoodies[i] : -1; }

	const CvPropertyManipulators* getPropertyManipulators() const { return &m_PropertyManipulators; }

	virtual void mapFrom(const picojson::value& entity);

	// --- the composed section units (by value; the base's mapFrom dispatch writes them via mut*) ---
	virtual const CvJsonModifiers* getModifiers() const { return &m_modifiers; }
	// `grants` (§5 numeric pulses + the `ai` scoped overrides) was NOT composed: the section reached only the
	// private scalars, so the grants machine's game-start resolution read 0 for every handicap pulse and no-op'd.
	virtual const CvJsonGrants* getGrants() const { return &m_grants; }

protected:
	virtual CvJsonModifiers* mutModifiers() { return &m_modifiers; }
	virtual CvJsonGrants*    mutGrants()    { return &m_grants; }

	//----------------------PRIVATE MEMBER VARIABLES------------------------------
private:

	CvJsonModifiers m_modifiers;                 // §6 families (the base dispatch fills this; feeds the PROPERTY_* bridge)
	CvJsonGrants    m_grants;                    // §5 game-start pulses (startingGold/units + the `ai` overrides)
	CvPropertyManipulators m_PropertyManipulators;  // fed from the PROPERTY_* families (CascadePropertyBridge::bridgeFamilies)

	int m_iFreeWinsVsBarbs;
	int m_iAnimalAttackProb;
	int m_iAdvancedStartPointsMod;
	int m_iStartingGold;
	int m_iUnitUpkeepPercent;
	int m_iDistanceMaintenancePercent;
	int m_iNumCitiesMaintenancePercent;
	int m_iColonyMaintenancePercent;
	int m_iMaxColonyMaintenance;
	int m_iCorporationMaintenancePercent;
	int m_iCivicUpkeepPercent;
	int m_iInflationPercent;
	int m_iRevolutionIndexPercent;
	int m_iHealthBonus;
	int m_iHappyBonus;
	int m_iAttitudeChange;
	int m_iNoTechTradeModifier;
	int m_iTechTradeKnownModifier;
	int m_iUnownedWaterTilesPerBarbarianUnit;
	int m_iUnownedTilesPerBarbarianCity;
	int m_iBarbarianCityCreationTurnsElapsed;
	int m_iBarbarianCityCreationProb;
	int m_iAnimalCombatModifier;
	int m_iBarbarianCombatModifier;
	int m_iAIAnimalCombatModifier;
	int m_iSubdueAnimalBonusAI;
	int m_iAIBarbarianCombatModifier;

	int m_iStartingDefenseUnits;
	int m_iStartingWorkerUnits;
	int m_iStartingExploreUnits;
	int m_iAIStartingDefenseUnits;
	int m_iAIStartingWorkerUnits;
	int m_iAIStartingExploreUnits;
	int m_iBarbarianInitialDefenders;
	int m_iAIDeclareWarProb;
	int m_iAIWorkRateModifier;
	int m_iAIGrowthPercent;
	int m_iAITrainPercent;
	int m_iAIWorldTrainPercent;
	int m_iAIConstructPercent;
	int m_iAIWorldConstructPercent;
	int m_iAICreatePercent;
	int m_iAIResearchPercent;
	int m_iAIWorldCreatePercent;
	int m_iAICivicUpkeepPercent;
	int m_iAIUnitUpkeepPercent;
	int m_iAIUnitSupplyPercent;
	int m_iAIUnitUpgradePercent;
	int m_iAIInflationPercent;
	int m_iAIWarWearinessPercent;
	int m_iAIPerEraModifier;
	int m_iAIAdvancedStartPercent;

	std::vector<int> m_piGoodies;   // identity.goodies -> resolved GOODY_* ids
};

#endif // CV_HANDICAP_INFO_H
