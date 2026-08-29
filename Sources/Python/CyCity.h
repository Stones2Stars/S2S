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
	//	The COMMERCE half of that census, indexed by CityCommerceTerm. Same document the yield tooltip and the
	//	commerce split itself read -- a view that re-derived a term from raw members would be a second answer.
	python::list getCommerceTerms(int iCommerce) const;
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

	int countNumImprovedPlots() const;

	int findBaseYieldRateRank(YieldTypes eYield) const;



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
	int getFirstBuildingOrder(int /*BuildingTypes*/ eBuilding) const;
	int getProductionProgress() const;
	int getProductionNeeded() const;
	int getProductionTurnsLeft() const;
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

	bool isNPC() const;
	bool isHuman() const;
	bool isVisible(int /*TeamTypes*/ eTeam, bool bDebug) const;

	bool isCapital() const;
	bool isCoastal(int iMinWaterSize) const;
	bool isDisorder() const;
	bool isHolyCityByType(int /*ReligionTypes*/ iIndex) const;
	bool isHolyCity() const;
	bool isHeadquartersByType(int /*CorporationTypes*/ iIndex) const;
	int getWarWearinessPercentAnger() const;

	int getRevIndexPercentAnger() const;

	int foodDifference(bool bBottom) const;
	int growthThreshold() const;
	int productionLeft() const;
	int64_t getHurryGold(int /*HurryTypes*/ iHurry) const;
	int hurryPopulation(int /*HurryTypes*/ iHurry) const;
	int hurryProduction(int /*HurryTypes*/ iHurry) const;
	int flatHurryAngerLength() const;

	int hasBuilding(int /*BuildingTypes*/ iIndex) const;
	bool isActiveBuilding(int /*BuildingTypes*/ iIndex) const;
	int getID() const;
	int getX() const;
	int getY() const;
	CyPlot* plot() const;
	bool isConnectedTo(const CyCity& kCity) const;
	bool isConnectedToCapital(int /*PlayerTypes*/ ePlayer) const;
	CyArea* area() const;

	int getGameDateFounded() const;
	int getPopulation() const;
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
	bool isGovernmentCenter() const;

	int getMaintenance() const;
	int getMaintenanceTimes100() const;

	void changeEspionageHealthCounter(int iChange);
	void changeEspionageHappinessCounter(int iChange);


	int getMilitaryHappinessUnits() const;
	int getExtraHappiness() const;
	int getExtraHealth() const;
	void changeExtraHappiness(int iChange);
	void changeExtraHealth(int iChange);
	int getHurryAngerTimer() const;

	int getRevRequestAngerTimer() const;
	void changeRevRequestAngerTimer(int iChange);
	void changeRevSuccessTimer(int iChange);

	int getConscriptAngerTimer() const;
	void changeConscriptAngerTimer(int iChange);
	void changeDefyResolutionAngerTimer(int iChange);
	void changeHappinessTimer(int iChange);

	int getFood() const;
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
	void changeOccupation(int iChange);
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
	int /*CultureLevelTypes*/ getCultureLevel() const;
	int getCultureThreshold() const;

	int getBaseYieldRateModifier(int /*YieldTypes*/ eIndex, int iExtra) const;

	int getCommerceRateModifier(int /*CommerceTypes*/ eIndex) const;


	int getArea() const;


	void changeEventAnger(int iChange);



	bool isAutomatedCanBuild(int /*BuildTypes*/ eIndex) const;
	void setAutomatedCanBuild(int /*BuildTypes*/ eIndex, bool bNewValue);

	PlayerTypes findHighestCulture() const;
	int calculateCulturePercent(int eIndex) const;

	int getNumRevolts(int playerIdx) const;
	void changeNumRevolts(int playerIdx, int iChange);

	bool isRevealed(int /*TeamTypes*/ eIndex, bool bDebug) const;
	// ⚖ THE IDENTITY SET -- the ONLY thing this handle publishes (owner). A wrapper is a marshalling handle, and
	// the READ planes (CyInfo / CyEnabler / the object's own accessor) are where data is asked for; but a legacy consumer holding a
	// handle needs to say WHICH object it holds, and re-pointing every such site is refactoring we are not doing
	// ("I only want to refactor the python I have to, otherwise we never will be done").
	// ⛔ SO THIS STAYS AN IDENTITY SET AND NEVER GROWS INTO THE LEGACY GETTER SURFACE: owner + id + position, the
	// axes that ADDRESS a city (docs/architecture/patterns.md §THE PYTHON READ BOUNDARY (Cy* is not a fixed contract) still bans the info/state getter contract). A consumer wanting
	// DATA asks the object's own accessor by that address; anything else added here is the escape hatch reopening.
	static void pythonPublish();

	std::wstring getName() const;
	std::wstring getNameKey() const;
	int getFreeBonus(int /*BonusTypes*/ eIndex) const;
	void changeFreeBonus(int /*BonusTypes*/ eIndex, int iChange);
	int getNumBonuses(int /*BonusTypes*/ iBonus) const;
	bool hasBonus(int /*BonusTypes */ iBonus) const;

	int getProgressOnBuilding(int /*BuildingTypes*/ iIndex) const;
	void setProgressOnBuilding(int /*BuildingTypes*/ iIndex, int iNewValue);


	int getProgressOnUnit(int iIndex) const;
	void setProgressOnUnit(int iIndex, int iNewValue);


	void setGreatPeopleUnitProgress(int /*UnitTypes*/ iIndex, int iNewValue);
	void changeGreatPeopleUnitProgress(int /*UnitTypes*/ iIndex, int iChange);
	int getSpecialistCount(int /*SpecialistTypes*/ eIndex) const;
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

	bool isHasReligion(int /*ReligionTypes*/ iIndex) const;
	void setHasReligion(int /*ReligionTypes*/ iIndex, bool bNewValue, bool bAnnounce, bool bArrows);
	bool isHasCorporation(int /*CorporationTypes*/ iIndex) const;
	void setHasCorporation(int /*CorporationTypes*/ iIndex, bool bNewValue, bool bAnnounce, bool bArrows);
	bool isActiveCorporation(int /*CorporationTypes*/ eCorporation) const;
	CyCity* getTradeCity(int iIndex) const;

	void clearOrderQueue();
	void popOrder(int iNum, bool bFinish, bool bChoose);
	int getOrderQueueLength() const;
	OrderData getOrderFromQueue(int iIndex) const;

	void setBuildingYieldChange(int /*BuildingTypes*/ eBuilding, int /*YieldTypes*/ eYield, int iChange);
	void setBuildingCommerceChange(int /*BuildingTypes*/ eBuilding, int /*CommerceTypes*/ eCommerce, int iChange);
	int getBuildingHappyChange(int /*BuildingTypes*/ eBuilding) const;
	int getBuildingHealthChange(int /*BuildingTypes*/ eBuilding) const;


	bool AI_isEmphasize(int iEmphasizeType) const;


	bool getBuildingListFilterActive(int /*BuildingFilterTypes*/ eFilter);
	int /*BuildingGroupingTypes*/ getBuildingListGrouping();
	int /*BuildingTypes*/ getBuildingListType(int iGroup, int iPos);

	bool getUnitListFilterActive(int /*UnitFilterTypes*/ eFilter);
	int /*UnitTypes*/ getUnitListType(int iGroup, int iPos);

	bool isEventOccured(int /*EventTypes*/ eEvent) const;

	std::string getScriptData() const;


	//	==== THE CITY WRITE SURFACE -- the receiver IS the city ====
	bool addFreeSpecialist(int iSpecialist, int iChange);
	// Give a freshly-created unit the experience a unit BUILT in this city would start with. Its call site is the
	// CRUSADE wonder's per-turn spawn, which has always handed the new crusader the city's production XP -- so
	// this keeps existing behaviour working rather than authoring any.
	bool addUnitProductionExperience(int iUnit, bool bConscript);
	// ⚠ ADDITIVE, and distinct from setCityCulture above -- the Alamo grant ADDS to whatever the city holds.
	// Culture is int64_t and ×100 on both sides ([culture-religion-research.md]: it accumulates and never decays,
	// which is why it is 64-bit at all), so the delta is int64_t too.
	bool changeCulture(int iForPlayer, int64_t iChange, bool bPlots);
	// The whip-anger countdown a demolition/event charges (CvCity::changeHurryAngerTimer).
	bool changeHurryAngerTimer(int iChange);
	bool changePopulation(int iChange);
	// bHandleGrowth defaults FALSE in the engine, matching every caller here: an event handing a city food is
	// topping up the store, not resolving a growth step this instant.
	bool changeStoredFood(int iChange);
	// The city CEASES TO EXIST. ⛔ Routed through CvPlayer::disband -- the same path TASK_DISBAND takes -- and NOT
	// through CvCity::kill, because disband owns bookkeeping kill() does not: it clears foundedFirstCity for a
	// player losing their last city, and registers the name in the destroyed-city registry.
	// ⚑ Every Python caller used to reach bare kill(), so each was silently skipping both.
	bool disband();
	bool invalidateBuildingList();
	// Mark a build list stale so the next read rebuilds it. This is the screen ASKING for work, which is why it
	// is an action and not folded into the read -- a read that rebuilt itself would be the self-healing shape
	// the whole surface avoids (docs/cascade.md §A SELF-HEAL IS THE FOSSIL OF A MISSING EMIT).
	bool invalidateUnitList();
	// Push a build order. ⚠ bAppend is load-bearing rather than a detail: the scenario copier replays a
	// city's WHOLE queue one order at a time, so appending is what makes the queue survive the copy -- a
	// fixed replace would leave only the last order.
	bool pushOrder(int iOrderType, int iId, int iData2, bool bSave, bool bPop, bool bAppend, bool bForce);
	// Make this city the selected one (the city screen / camera follow it). The engine's own selectCity takes a
	// CvCity*, which script cannot hold -- so the pair is resolved here and the engine called on its behalf.
	// Answers whether the city resolved, so a caller can tell "did nothing" from "did it".
	bool select(bool bTestProduction);
	bool setBuildingGrantedCommerce(int iBuilding, int iCommerce, int iValue);
	bool setBuildingGrantedWellbeing(int iBuilding, int iKind, int iValue);
	bool setBuildingGrantedYield(int iBuilding, int iYield, int iValue);
	// ---- The city screen's LIST VIEW state: which filter/sort/grouping the player left its lists on. ----
	// ⚖ These are the one place this surface writes, and they are deliberately narrow: VIEW state, not
	// gameplay. What the owner ruled banned is DEVELOPING game logic in Python; keeping the existing logic
	// working is not ([roadmap] scope decision 6), and a list's sort order is not game logic by any reading --
	// it changes what the player SEES, never what the game does. The matching READS are above, so the
	// lists both render and respond to a click through one coherent pair.
	bool setBuildingListFilterActive(int iFilter, bool bActive);
	bool setBuildingListSorting(int iSorting);
	// PRESENCE of a building in this city, both directions. ⛔ ONE bool-parameterized verb, because the ENGINE
	// models it as one (CvCity::changeHasBuilding) -- an add-only verb with a remove twin bolted beside it would be
	// two Python spellings of a single transition, and the two drift (docs/architecture/patterns.md §DRY (single implementation)).
	// ⚑ The removal leg is NOT a field poke: it runs the ledger, the setup and processBuilding(-1), so the
	// contribution is withdrawn and the domain fact fires exactly as a demolition in-game does.
	bool setBuilding(int iBuilding, bool bNewValue);
	bool setCorporation(int iCorporation, bool bHeadquarters);
	// The city's culture HELD BY ONE PLAYER. ⚠ It is ×100 and 64-bit on both sides -- the exact twin of
	// getCultureForPlayer above, so a scenario round-trips the value it was handed rather than a rescaled one
	// ([culture-religion-research.md]: city culture accumulates the ×100 rate and never decays, which is why it
	// is int64_t at all).
	bool setCulture(int iForPlayer, int64_t iCulture);
	bool setDefenseDamage(int iDamage);
	// The one-shot EVENT/VOTE grant store -- the twin of getGrantedExtras above. A scenario that could
	// read them and not write them back would drop them on every round trip.
	bool setGrantedExtra(int iKind, int iValue);
	// ---- The SCENARIO APPLY: what WorldBuilder puts back when it reads a .CivBeyondSwordWBSave ----
	// ⚖ These earn their place the way this surface requires -- by existing call sites that need them
	// (`CvWBDesc.CvCityDesc.apply`). They keep EXISTING behaviour working; nothing here is game logic authored
	// in script, and the scenario format is the same one the engine has always round-tripped.
	// ⛔ WorldBuilder is NOT a lower tier of consumer, and its breakage is not accepted (owner: "we cannot
	// accept actually breaking worldbuilder stuff, we fix things we see" -- [roadmap] scope decision 1b).
	// ⚠ WB mutates ARBITRARY state directly, which is exactly why it goes through the engine's own setters
	// here: each one emits the fact the normal path emits, so no cache, context or enabler set is left
	// describing a world that no longer exists ([roadmap] 1b: WB adding or removing anything EMITS, with no WB
	// special case anywhere).
	bool setName(std::wstring szName);
	bool setOccupation(int iTurns);
	///<summary>Re-stamps who FOUNDED the city. Used when a barbarian city becomes a real civ's, so the
	/// emergent player reads as the founder rather than as a conqueror.</summary>
	///<returns>false when the city handle does not resolve, or the new owner is out of range.</returns>
	bool setOriginalOwner(int iOriginalOwner);
	// ⛔ BOTH shapes are published for population and stored food because the ENGINE has both, and a caller that
	// means a DELTA must be able to say so. Making it read-then-write instead would turn one atomic mutation into
	// two steps that another consumer can interleave -- a different operation wearing the same name.
	bool setPopulation(int iPopulation);
	bool setReligion(int iReligion, bool bHolyCity);
	bool setScriptData(std::string szData);
	bool setStoredFood(int iFood);
	bool setWeLoveTheKingDay(bool bNewValue);
	bool setUnitListFilterActive(int iFilter, bool bActive);
	bool setUnitListGrouping(int iGrouping);
	bool setUnitListSorting(int iSorting);

private:
	CvCity* m_pCity;
};

// A city ALSO crosses to Python as its (owner, id) identity, which is what an event payload carries.
DECLARE_PY_IDENTITY(CvCity*, getOwner(), getID());

#endif // CyCity_h__