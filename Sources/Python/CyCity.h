#pragma once

#ifndef CyCity_h__
#define CyCity_h__

//
// Python wrapper class for CvCity
//

class CityOutputHistory;
class CvCity;
//class CvProperties;
class CyPlot;
class CyArea;
class CyUnit;
struct OrderData;

class CyCity
{
public:
	DllExport explicit CyCity(CvCity* pCity);		// Call from C++

	CvCity* getCity() const { return m_pCity; }	// Call from C++

	//	==== THE CITY READ SURFACE ====
	//
	//	The "what do I HAVE, right now?" half of the GAME-OBJECT read role, homed on the object it describes
	//	(docs/architecture/patterns.md §THE PYTHON READ BOUNDARY (accessor homing)). A city's data is asked OF THE CITY: a caller resolves the object and then asks it
	//	(GC.getPlayer(i).getCity(id).getYields()), and that chain STATES the containment instead of flattening it
	//	into an (owner, id) argument pair.
	//
	//	THE GRAMMAR (patterns.md § THE TWO READ ROLES), unchanged:
	//	  - ONE READ PER GROUP, and the getter IS the group -- there is NO scalar getter per channel; a script
	//	    wanting one value indexes the returned list. The surface grows by GROUPS, never by channels.
	//	  - THE EXISTING ENGINE ENUM INDEXES THE RESULT, never the call: getYields()[YieldTypes.YIELD_FOOD]. A
	//	    family with no engine enum is indexed by its own kind enum, and the name says so (get<Family>Kinds).
	//	  - EVERY AMOUNT IS x100 NATIVE (docs/specs/curators/fixed-point-and-scales.md §1 (the x100 fixed-point model)) -- no `100` in any name, no scale variant, and no
	//	    read reduces. A PERCENT is not scaled, so a percent-unit channel is already the number you want.
	//	  - EVERY READ IS A BARE FETCH: nothing here gates, ensures or recomputes, so a missed invalidation shows
	//	    up in script as a visibly wrong number rather than being repaired at the boundary (docs/cascade.md §A SELF-HEAL IS THE FOSSIL OF A MISSING EMIT).
	//
	//	⛔ NO METHOD HERE CARRIES ANOTHER OBJECT'S NOUN. The receiver already IS the city, so `getPopulation()`,
	//	never `getCityPopulation()` -- a noun in the name is the mechanical tell that a read is homed on the wrong
	//	class (docs/architecture/patterns.md §THE PYTHON READ BOUNDARY (accessor homing)).

	python::list getYields() const;
	python::list getCommerces() const;
	python::list getWellbeing() const;
	python::list getDefenseKinds() const;
	python::list getMaintenanceKinds() const;
	python::list getBuildRateKinds() const;
	python::list getCombatKinds() const;
	python::list getExperienceKinds() const;
	python::list getRevolutionKinds() const;
	python::list getTradeRouteKinds() const;
	python::list getScalars() const;
	python::list getHealKinds() const;
	python::list getUnderworldKinds() const;
	python::list getVisionKinds() const;

	//	The REALIZED wellbeing: the deposits above PLUS the raw-state inputs no deposit produces (modifier.md
	//	§2b). Distinct from getWellbeing on purpose; a script that adds the two double-counts. iExtraPopulation
	//	projects onto a population the city is ABOUT to reach -- the anger terms are non-linear in population, so
	//	it cannot be derived from the 0 answer.
	python::list getRealizedWellbeing(int iExtraPopulation) const;
	//	The two FINAL-STATE values DOWNSTREAM of that group -- calculations over the four channels, never slots in
	//	them (patterns.md rule 6), published so a script does not become a second implementation of the rule.
	int getHealthRate(int iExtraPopulation) const;
	int getAngryPopulation(int iExtraPopulation) const;
	python::list getYieldModifiers() const;
	//	THE CITY YIELD CENSUS for ONE channel -- the same decomposition the /computed census renders, so a tooltip
	//	reads the SAME DOCUMENT rather than recomputing its own breakdown. Indexed by CityYieldTerm.
	python::list getYieldTerms(int iYield) const;
	int getSight() const;
	int getLiberationPlayer() const;
	int64_t getRealizedMaintenance() const;

	//	Where this city places among its OWNER'S cities for each channel. The engine enum indexes the RESULT, so
	//	the whole group comes back and no channel is ever named in the call. A rank is an ORDINAL, never x100.
	python::list getYieldRateRanks() const;
	python::list getBaseYieldRateRanks() const;
	python::list getCommerceRateRanks() const;

	python::list getPosition() const;
	python::list getPlots() const;
	int getImprovedPlotCount() const;
	int getWaterPlotCount() const;

	//	The AI's OWN HEURISTIC SCORES -- advice, never a fact about the city (superseded-ideas par.1).
	int getAiValue() const;
	int getAiBestBuildCount() const;

	python::list getCountdowns() const;
	python::list getGrowth() const;
	python::list getCounts() const;
	python::list getGrantedExtras() const;
	python::list getOutputHistory() const;

	python::list getBuildingReads(int iBuilding) const;
	python::list getUnitReads(int iUnit) const;
	python::list getSpecialistReads(int iSpecialist) const;
	python::list getBuildingGrantedWellbeing(int iBuilding) const;
	python::list getBuildingGrantedYields(int iBuilding) const;
	python::list getBuildingGrantedCommerces(int iBuilding) const;
	int getBuildingBuiltTime(int iBuilding) const;
	int getAddedFreeSpecialists(int iSpecialist) const;
	int getGreatPeopleUnitProgress(int iUnitType) const;

	python::list getUnitListGroups() const;
	python::list getBuildingListGroups() const;
	bool isUnitQueued(int iUnitType) const;

	int getBestUnit() const;
	int getBestUnitForRole(int iUnitAI) const;
	int getProductionTurnsLeftFor(int iOrder, int iType, int iNum) const;

	python::list getOrder() const;
	int getBuildingListSorting() const;
	int getUnitListGrouping() const;
	int getUnitListSorting() const;

	python::list getFlags() const;
	python::list getProperties() const;
	python::list getReligions() const;
	python::list getCorporations() const;
	python::list getCultureReads() const;
	//	Rows of [partnerOwner, partnerCity, profitTimes100]. Routes are live STATE, so they are walked rather
	//	than looked up -- nothing authors a route.
	python::list getTradeRoutes() const;
	python::list getHurryQuote(int iHurry) const;
	int getDateFounded(bool bHistoricalCalendar) const;
	int64_t getCultureForPlayer(int iForPlayer) const;
	int getCulturePercent(int iForPlayer) const;
	int getTradeYield(int iYield, int iProfitTimes100) const;
	//	The MAINTAINED traded count, not the engine relay -- a reader answering from a different source than the
	//	deposits do is a second truth, not a second opinion (docs/architecture/patterns.md §DRY (single implementation)).
	int getNumBonusesAvailable(int iBonus) const;
	bool hasCorporationPresent(int iCorporation) const;
	int getProjectProductionFor(int iProject) const;
	int getHandicapLevel() const;
	bool isRevealedTo(int iTeam) const;
	bool isCoastalTo(int iMinWaterSize) const;
	bool isEmphasizing(int iEmphasize) const;

	void kill();

	void setRevolutionIndex(int iNewValue);
	void changeRevolutionIndex(int iChange);

	int getLocalRevIndex() const;
	void setLocalRevIndex(int iNewValue);

	void setRevIndexAverage(int iNewValue);
	void updateRevIndexAverage();

	int getRevIndexDistanceMod() const;

	int getRevolutionCounter() const;
	void setRevolutionCounter(int iNewValue);
	void changeRevolutionCounter(int iChange);

	int getReinforcementCounter() const;
	void setReinforcementCounter(int iNewValue);
	void changeReinforcementCounter(int iChange);

	CyPlot* getCityIndexPlot(int iIndex) const;
	bool canWork(const CyPlot* pPlot) const;
	int countNumImprovedPlots() const;
	int countNumWaterPlots() const;

	int findBaseYieldRateRank(YieldTypes eYield) const;
	int findYieldRateRank(YieldTypes eYield) const;
	int findCommerceRateRank(CommerceTypes eCommerce) const;

	int getMaxNumWorldWonders() const;
	int getMaxNumNationalWonders() const;


	bool canCreate(ProjectTypes eProject, bool bContinue, bool bTestVisible) const;
	bool canMaintain(ProcessTypes eProcess) const;
	int getFoodTurnsLeft() const;
	bool isProduction() const;
	bool isProductionUnit() const;
	bool isProductionBuilding() const;
	bool isProductionProject() const;
	bool isProductionProcess() const;

	int getProductionExperience(UnitTypes eUnit) const;
	void addProductionExperience(const CyUnit& kUnit, bool bConscript);

	UnitTypes getProductionUnit() const;
	BuildingTypes getProductionBuilding() const;
	ProjectTypes getProductionProject() const;
	ProcessTypes getProductionProcess() const;
	std::wstring getProductionName() const;
	std::wstring getProductionNameKey() const;
	int getGeneralProductionTurnsLeft() const;
	bool isFoodProduction() const;
	int getFirstUnitOrder(int /*UnitTypes*/ eUnit) const;
	int getNumTrainUnitAI(int /*UnitAITypes*/ eUnitAI) const;
	int getFirstBuildingOrder(int /*BuildingTypes*/ eBuilding) const;
	int getProductionProgress() const;
	int getProductionNeeded() const;
	int getProductionTurnsLeft() const;
	int getUnitProductionTurnsLeft(int /*UnitTypes*/ iUnit, int iNum) const;
	int getBuildingProductionTurnsLeft(int /*BuildingTypes*/ iBuilding, int iNum) const;
	int getProjectProductionTurnsLeft(int /*ProjectTypes*/ eProject, int iNum) const;
	void setProductionProgress(int iNewValue);
	void changeProduction(int iChange);
	int getCurrentProductionDifference(bool bIgnoreFood, bool bOverflow) const;

	bool canHurry(int /*HurryTypes*/ iHurry, bool bTestVisible) const;
	int /*UnitTypes*/ getConscriptUnit() const;
	int flatConscriptAngerLength() const;
	bool canConscript() const;
	int /* HandicapTypes */ getHandicapType() const;
	int /* CivilizationTypes */ getCivilizationType() const;
	int /*LeaderHeadTypes*/ getPersonalityType() const;
	int /*ArtStyleTypes*/ getArtStyleType() const;

	bool hasTrait(int /*TraitTypes*/ iTrait) const;
	bool isNPC() const;
	bool isHominid() const;
	bool isHuman() const;
	bool isVisible(int /*TeamTypes*/ eTeam, bool bDebug) const;

	bool isCapital() const;
	bool isCoastal(int iMinWaterSize) const;
	bool isDisorder() const;
	bool isHolyCityByType(int /*ReligionTypes*/ iIndex) const;
	bool isHolyCity() const;
	bool isHeadquartersByType(int /*CorporationTypes*/ iIndex) const;
	int getNoMilitaryPercentAnger() const;
	int getWarWearinessPercentAnger() const;

	int getRevIndexPercentAnger() const;

	int totalFreeSpecialists() const;
	int healthRate(int iExtra) const;
	int foodConsumption(bool bNoAngry, int iExtra) const;
	int foodDifference(bool bBottom) const;
	int growthThreshold() const;
	int productionLeft() const;
	int64_t getHurryGold(int /*HurryTypes*/ iHurry) const;
	int hurryPopulation(int /*HurryTypes*/ iHurry) const;
	int hurryProduction(int /*HurryTypes*/ iHurry) const;
	int flatHurryAngerLength() const;

	void changeHasBuilding(int /*BuildingTypes*/ iIndex, bool bNewValue);
	int hasBuilding(int /*BuildingTypes*/ iIndex) const;
	bool isActiveBuilding(int /*BuildingTypes*/ iIndex) const;
	int getID() const;
	int getX() const;
	int getY() const;
	CyPlot* plot() const;
	bool isConnectedTo(const CyCity& kCity) const;
	bool isConnectedToCapital(int /*PlayerTypes*/ ePlayer) const;
	CyArea* area() const;
	CyArea* waterArea() const;

	int getGameTurnFounded() const;
	int getGameDateFounded() const;
	int getPopulation() const;
	void setPopulation(int iNewValue);
	void changePopulation(int iChange);
	int64_t getRealPopulation() const;

	int getHighestPopulation() const;
	void setHighestPopulation(int iNewValue);
	int getWorkingPopulation() const;
	int getSpecialistPopulation() const;
	int getNumGreatPeople() const;
	int getBaseGreatPeopleRate() const;
	int getGreatPeopleRate() const;
	int getGreatPeopleProgress() const;
	void changeGreatPeopleProgress(int iChange);
	int getNumWorldWonders() const;
	int getNumNationalWonders() const;
	int getNumBuildings() const;
	bool isGovernmentCenter() const;

	int getMaintenance() const;
	int getMaintenanceTimes100() const;

	void changeEspionageHealthCounter(int iChange);
	void changeEspionageHappinessCounter(int iChange);

	int getBuildingHealth(int iBuilding) const;

	int getMilitaryHappinessUnits() const;
	int getBuildingHappiness(int iBuilding) const;
	int getExtraHappiness() const;
	int getExtraHealth() const;
	void changeExtraHappiness(int iChange);
	void changeExtraHealth(int iChange);
	int getHurryAngerTimer() const;
	void changeHurryAngerTimer(int iChange);

	int getRevRequestAngerTimer() const;
	void changeRevRequestAngerTimer(int iChange);
	void changeRevSuccessTimer(int iChange);

	int getConscriptAngerTimer() const;
	void changeConscriptAngerTimer(int iChange);
	void changeDefyResolutionAngerTimer(int iChange);
	int flatDefyResolutionAngerLength() const;
	void changeHappinessTimer(int iChange);
	bool isNoUnhappiness() const;

	int getFood() const;
	void setFood(int iNewValue);
	void changeFood(int iChange);
	int getFoodKept() const;
	int getMaxProductionOverflow() const;
	int getOverflowProduction() const;
	void setOverflowProduction(int iNewValue);
	int getFeatureProduction() const;
	void setFeatureProduction(int iNewValue);
	int getExtraTradeRoutes() const;
	int getMaxTradeRoutes() const;
	int getBuildingDefense() const;
	int getMaxAirlift() const;
	bool isPowered() const;
	void changeDefenseDamage(int iChange);
	int getTotalDefense(bool bIgnoreBuilding) const;
	int getDefenseModifier(bool bIgnoreBuilding) const;

	bool isOccupation() const;
	void setOccupationTimer(int iNewValue);
	void changeOccupationTimer(int iChange);
	bool isNeverLost() const;
	void setNeverLost(bool bNewValue);

	bool isBombarded() const;
	void setBombarded(bool bNewValue);
	bool isDrafted() const;
	void setDrafted(bool bNewValue);
	bool isAirliftTargeted() const;
	void setAirliftTargeted(bool bNewValue);
	bool isCitizensAutomated() const;
	void setCitizensAutomated(bool bNewValue);
	bool isProductionAutomated() const;
	void setProductionAutomated(bool bNewValue);
	bool isWallOverride() const;
	void setWallOverride(bool bOverride);
	bool isPlundered() const;
	void setPlundered(bool bNewValue);
	int /*PlayerTypes*/getOwner() const;
	int /*TeamTypes*/getTeam() const;
	int /*PlayerTypes*/getPreviousOwner() const;
	int /*PlayerTypes*/getOriginalOwner() const;
	void setOriginalOwner(int /*PlayerTypes*/ iPlayer);
	int /*CultureLevelTypes*/ getCultureLevel() const;
	int getCultureThreshold() const;

	int getBaseYieldRateModifier(int /*YieldTypes*/ eIndex, int iExtra) const;
	int getYieldRateModifier(int /*YieldTypes*/ eIndex) const;

	int getProductionToCommerceModifier(int /*CommerceTypes*/ eIndex) const;
	int getCommerceRateModifier(int /*CommerceTypes*/ eIndex) const;


	int getArea() const;

	bool isWeLoveTheKingDay() const;
	void setWeLoveTheKingDay(bool bWeLoveTheKingDay);
	int64_t calcCorporateMaintenance() const;

	void changeEventAnger(int iChange);



	bool isAutomatedCanBuild(int /*BuildTypes*/ eIndex) const;
	void setAutomatedCanBuild(int /*BuildTypes*/ eIndex, bool bNewValue);

	PlayerTypes findHighestCulture() const;
	int calculateCulturePercent(int eIndex) const;
	void setCulture(int /*PlayerTypes*/ eIndex, int iNewValue, bool bPlots);
	void setCultureTimes100(int /*PlayerTypes*/ eIndex, int iNewValue, bool bPlots);
	void changeCulture(int /*PlayerTypes*/ eIndex, int iChange, bool bPlots);

	int getNumRevolts(int playerIdx) const;
	void changeNumRevolts(int playerIdx, int iChange);

	bool isRevealed(int /*TeamTypes*/ eIndex, bool bDebug) const;
	void setRevealed(int /*TeamTypes*/ eIndex, bool bNewValue);
	bool getEspionageVisibility(int /*TeamTypes*/ eIndex) const;
	// ⚖ THE IDENTITY SET -- the ONLY thing this handle publishes (owner). A wrapper is a marshalling handle, and
	// the READ planes (CyInfo / CyState / CyEnabler) are where data is asked for; but a legacy consumer holding a
	// handle needs to say WHICH object it holds, and re-pointing every such site is refactoring we are not doing
	// ("I only want to refactor the python I have to, otherwise we never will be done").
	// ⛔ SO THIS STAYS AN IDENTITY SET AND NEVER GROWS INTO THE LEGACY GETTER SURFACE: owner + id + position, the
	// axes that ADDRESS a city (docs/architecture/patterns.md §THE PYTHON READ BOUNDARY (Cy* is not a fixed contract) still bans the info/state getter contract). A consumer wanting
	// DATA asks CyState by that address; anything else added here is the escape hatch reopening.
	static void pythonPublish();

	std::wstring getName() const;
	std::wstring getNameForm(int iForm) const;
	std::wstring getNameKey() const;
	void setName(std::wstring szNewValue, bool bFound);
	int getFreeBonus(int /*BonusTypes*/ eIndex) const;
	void changeFreeBonus(int /*BonusTypes*/ eIndex, int iChange);
	int getNumBonuses(int /*BonusTypes*/ iBonus) const;
	bool hasBonus(int /*BonusTypes */ iBonus) const;

	int getProgressOnBuilding(int /*BuildingTypes*/ iIndex) const;
	void setProgressOnBuilding(int /*BuildingTypes*/ iIndex, int iNewValue);
	int getDelayOnBuilding(int /*BuildingTypes*/ eIndex) const;
	bool isBuildingProductionDecay(int /*BuildingTypes*/ eIndex) const;
	int getBuildingProductionDecayTurns(int /*BuildingTypes*/ eIndex) const;

	int getBuildingOriginalOwner(int /*BuildingTypes*/ iIndex) const;
	int getBuildingOriginalTime(int /*BuildingTypes*/ iIndex) const;

	int getProgressOnUnit(int iIndex) const;
	void setProgressOnUnit(int iIndex, int iNewValue);
	int getDelayOnUnit(int /*UnitTypes*/ eIndex) const;
	bool isUnitProductionDecay(int /*UnitTypes*/ eIndex) const;
	int getUnitProductionDecayTurns(int /*UnitTypes*/ eIndex) const;

	int getProjectProduction(int /*ProjectTypes*/ iIndex) const;

	void setGreatPeopleUnitProgress(int /*UnitTypes*/ iIndex, int iNewValue);
	void changeGreatPeopleUnitProgress(int /*UnitTypes*/ iIndex, int iChange);
	int getSpecialistCount(int /*SpecialistTypes*/ eIndex) const;
	int getMaxSpecialistCount(int /*SpecialistTypes*/ eIndex) const;
	bool isSpecialistValid(int /*SpecialistTypes*/ eIndex, int iExtra) const;
	int getForceSpecialistCount(int /*SpecialistTypes*/ eIndex) const;
	void setForceSpecialistCount(int /*SpecialistTypes*/ eIndex, int iNewValue);
	int getFreeSpecialistCount(int /*SpecialistTypes*/ eIndex) const;
	//	Koshling - removed direct set of free specialist count - it's HIGHLY unsafe - use changeFreeSpecialistCount
	//void setFreeSpecialistCount(int /*SpecialistTypes*/ eIndex, int iNewValue);
	void changeFreeSpecialistCount(int /*SpecialistTypes*/ eIndex, int iChange);
	int getAddedFreeSpecialistCount(int /*SpecialistTypes*/ eIndex) const;
	int getReligionInfluence(int /*ReligionTypes*/ iIndex) const;
	void changeReligionInfluence(int /*ReligionTypes*/ iIndex, int iChange);


	int getEspionageDefenseModifier() const;

	bool isWorkingPlot(const CyPlot& kPlot) const;
	bool isHasReligion(int /*ReligionTypes*/ iIndex) const;
	void setHasReligion(int /*ReligionTypes*/ iIndex, bool bNewValue, bool bAnnounce, bool bArrows);
	bool isHasCorporation(int /*CorporationTypes*/ iIndex) const;
	void setHasCorporation(int /*CorporationTypes*/ iIndex, bool bNewValue, bool bAnnounce, bool bArrows);
	bool isActiveCorporation(int /*CorporationTypes*/ eCorporation) const;
	CyCity* getTradeCity(int iIndex) const;

	void clearOrderQueue();
	void pushOrder(OrderTypes eOrder, int iData1, int iData2, bool bSave, bool bPop, bool bAppend, bool bForce);
	void popOrder(int iNum, bool bFinish, bool bChoose);
	int getOrderQueueLength() const;
	OrderData getOrderFromQueue(int iIndex) const;

	void setBuildingYieldChange(int /*BuildingTypes*/ eBuilding, int /*YieldTypes*/ eYield, int iChange);
	void setBuildingCommerceChange(int /*BuildingTypes*/ eBuilding, int /*CommerceTypes*/ eCommerce, int iChange);
	int getBuildingHappyChange(int /*BuildingTypes*/ eBuilding) const;
	void setBuildingHappyChange(int /*BuildingTypes*/ eBuilding, int iChange);
	int getBuildingHealthChange(int /*BuildingTypes*/ eBuilding) const;
	void setBuildingHealthChange(int /*BuildingTypes*/ eBuilding, int iChange);


	bool AI_isEmphasizeSpecialist(int /*SpecialistTypes*/ iIndex) const;
	bool AI_isEmphasize(int iEmphasizeType) const;
	int AI_countBestBuilds(const CyArea& kArea) const;
	int AI_cityValue() const;

	const CityOutputHistory* getCityOutputHistory() const;

	bool getBuildingListFilterActive(int /*BuildingFilterTypes*/ eFilter);
	void setBuildingListFilterActive(int /*BuildingFilterTypes*/ eFilter, bool bActive);
	int /*BuildingGroupingTypes*/ getBuildingListGrouping();
	void setBuildingListGrouping(int /*BuildingGroupingTypes*/ eGrouping);
	void setBuildingListSorting(int /*BuildingSortTypes*/ eSorting);
	int getBuildingListGroupNum();
	int getBuildingListNumInGroup(int iGroup);
	int /*BuildingTypes*/ getBuildingListType(int iGroup, int iPos);

	void setUnitListInvalid();
	bool getUnitListFilterActive(int /*UnitFilterTypes*/ eFilter);
	void setUnitListFilterActive(int /*UnitFilterTypes*/ eFilter, bool bActive);
	void setUnitListGrouping(int /*UnitGroupingTypes*/ eGrouping);
	void setUnitListSorting(int /*UnitSortTypes*/ eSorting);
	int getUnitListGroupNum();
	int getUnitListNumInGroup(int iGroup);
	int /*UnitTypes*/ getUnitListType(int iGroup, int iPos);

	bool isEventOccured(int /*EventTypes*/ eEvent) const;

	std::string getScriptData() const;
	void setScriptData(std::string szNewValue);

	int AI_bestUnit() const;
	int AI_bestUnitAI(UnitAITypes eUnitAITypes) const;

private:
	CvCity* m_pCity;
};

// A city crosses to Python as its (owner, id) IDENTITY, not as a CyCity handle: CyCity carries zero defs, so a
// script handed one could ask it nothing -- while every read on the library is addressed by exactly this pair.
DECLARE_PY_IDENTITY(CvCity*, getOwner(), getID());

#endif // CyCity_h__