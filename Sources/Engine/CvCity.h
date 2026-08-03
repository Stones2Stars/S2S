#pragma once

// city.h

#ifndef CIV4_CITY_H
#define CIV4_CITY_H

#include "Infrastructure/LinkedList.h"
#include "CvEventGrants.h"
#include "Infrastructure/CvDLLEntity.h"
#include "CvGameObject.h"
#include "Engine/CvStatus.h"   // CityStatus + the status model (an applied counter, ticking down)
#include "CvProperties.h"
#include "UI/CvBuildingList.h"
#include "UI/CvUnitList.h"
#include "CvDerivedData.h"
#include "UI/CityOutputHistory.h"
#include "CvGameObject.h"
#include "CityContext.h"
#include "CvCascadePackage.h"   // the CITY-scope cascade package + receiver sums (state-repositories.md)
#include "Infrastructure/CvDerivedCache.h"  // the ONE derived-cache component (mark-driven, never serialized)
#include "Enabler/CvEnabler.h"              // CityEnabler -- the per-city buildings/units tri-state domains
#include "Enabler/CvOperatingBuildings.h"   // the ACTIVE-building set the modifier reads (enabler.md §3.2)

class CvArea;
class CvArtInfoBuilding;
class CvPlot;
class CvPlotGroup;
class CvUnit;
class CvUnitSelectionCriteria;

#define CITY_MAX_YIELD_RATE    99000000
#define CITY_MAX_YIELD_RATE100 1900000000

// BUG - start
void addGoodOrBad(int iValue, int& iGood, int& iBad);
void subtractGoodOrBad(int iValue, int& iGood, int& iBad);
// BUG - end

struct ProductionCalc
{
	enum flags {
		None = 0,
		FoodProduction = 1 << 0,
		Overflow = 1 << 1,
		Yield = 1 << 2
	};
};

DECLARE_FLAGS(ProductionCalc::flags);


class CvCity
	: public CvDLLEntity
	, private bst::noncopyable // disable copy: we have owned pointers so we can't use the default copy implementation
{
public:
	CvCity();
	virtual ~CvCity();

	void init(int iID, PlayerTypes eOwner, int iX, int iY, bool bBumpUnits, bool bUpdatePlotGroups);
	void uninit();
	void reset(int iID = 0, PlayerTypes eOwner = NO_PLAYER, int iX = 0, int iY = 0, bool bConstructorCall = false);
	void setupGraphical();

	CvGameObjectCity* getGameObject() { return &m_GameObject; }
	const CvGameObjectCity* getGameObject() const { return &m_GameObject; }

	//	City-level derived-data repository (see CvDerivedData.h). Constructible set, building
	//	values, declared needs; empty for now.
	CvCityDataRepository&       dataRepository()       { return m_dataRepository; }
	const CvCityDataRepository& dataRepository() const { return m_dataRepository; }

	int getNumWorkers() const { return m_workers.size(); }
	std::vector<int> getWorkers() const { return m_workers; }
	void setWorkerHave(const int iUnitID, const bool bNewValue);


private:
	bool canHurryInternal(const HurryTypes eHurry) const;

	std::vector<int> m_workers;

protected:
	CvGameObjectCity m_GameObject;
	CvCityDataRepository m_dataRepository;
	CityOutputHistory m_outputHistory;


public:
	int getRevolutionIndex() const;
	void setRevolutionIndex( int iNewValue );
	void changeRevolutionIndex( int iChange );

	int getLocalRevIndex() const;
	void setLocalRevIndex( int iNewValue );
	void changeLocalRevIndex( int iChange );

	int getRevIndexAverage() const;
	void setRevIndexAverage( int iNewValue );
	void updateRevIndexAverage( );

	int getRevIndexDistanceMod() const { return m_iRevIndexDistanceMod; }
	void changeRevIndexDistanceMod(const int iChange);

	int getRevolutionCounter() const;
	void setRevolutionCounter( int iNewValue );
	void changeRevolutionCounter( int iChange );

	int getReinforcementCounter() const;
	void setReinforcementCounter( int iNewValue );
	void changeReinforcementCounter( int iChange );

	bool isRecentlyAcquired() const;

	void kill(bool bUpdatePlotGroups, bool bUpdateCulture = true);
	void killTestCheap(); // For testing, do not call in a game situation

	void doTurn();

	// PLACE the queue-excluded buildings -- every `identity.notConstructible` building is placed in EVERY city,
	// UNCONDITIONALLY, and its `requires.operate` then decides whether it is active or dormant (owner ruling;
	// enabler.md §3, the band model generalized to the whole class). There is no placement gate to evaluate and
	// nothing is ever auto-removed: dormancy is reversible and removal is not, so a band leaving its range goes
	// DORMANT rather than being torn down and rebuilt.
	//
	// ⛔ It replaces the per-turn add/remove churn wholesale -- the property-band sweep AND the autobuild pass,
	// which between them re-evaluated a placement gate for every band in every city every turn and could remove a
	// building it had just placed. Nothing calls this per turn: placement is idempotent and permanent, so it runs
	// ONCE per city.
	//
	// Idempotent by construction (it places only what is absent), which is what lets it double as the LOAD
	// BACKFILL for a save taken before the ruling -- the same shape as the `TECH_GAME_START` backfill
	// (enabler.md §2), and the reason no migration pass is needed.
	void placeSystemBuildings();

	bool isCitySelected() const;
	DllExport bool canBeSelected() const;
	DllExport void updateSelectedCity(bool bTestProduction);


	void updateVisibility();
	bool isVisibilitySetup() const;

	void createGreatPeople(UnitTypes eGreatPersonUnit, bool bIncrementThreshold, bool bIncrementExperience);

	void doTask(TaskTypes eTask, int iData1 = -1, int iData2 = -1, bool bOption = false, bool bAlt = false, bool bShift = false, bool bCtrl = false);

	// Base iterator type for iterating over city plots, returning valid ones only
	template < class Value_ >
	struct city_plot_iterator_base :
		public bst::iterator_facade<city_plot_iterator_base<Value_>, Value_*, bst::forward_traversal_tag, Value_*>
	{
		city_plot_iterator_base() : m_centerX(-1), m_centerY(-1), m_curr(nullptr), m_idx(0), m_numPlots(0) {}
		explicit city_plot_iterator_base(int centerX, int centerY, int numPlots, int idx) : m_centerX(centerX), m_centerY(centerY), m_numPlots(numPlots), m_curr(nullptr), m_idx(idx)
		{
			increment();
		}

	private:
		friend class bst::iterator_core_access;
		void increment()
		{
			m_curr = nullptr;
			while (m_curr == nullptr && m_idx < m_numPlots/*NUM_CITY_PLOTS*/)
			{
				m_curr = plotCity(m_centerX, m_centerY, m_idx);
				++m_idx;
			}
		}
		bool equal(city_plot_iterator_base const& other) const
		{
			return (this->m_centerX == other.m_centerX
				&& this->m_centerY == other.m_centerY
				&& this->m_idx == other.m_idx)
				|| (this->m_curr == nullptr && other.m_curr == nullptr);
		}

		Value_* dereference() const { return m_curr; }

		const int m_centerX;
		const int m_centerY;
		const int m_numPlots;
		Value_* m_curr;
		int m_idx;
	};
	typedef city_plot_iterator_base<CvPlot> city_plot_iterator;

	city_plot_iterator beginPlots(int numPlots, bool skipCityHomePlot) const { return city_plot_iterator(getX(), getY(), numPlots, skipCityHomePlot); }
	city_plot_iterator endPlots() const { return city_plot_iterator(); }

	typedef bst::iterator_range<city_plot_iterator> city_plot_range;
 	city_plot_range plots(int numPlots, bool skipCityHomePlot = false) const { return city_plot_range(beginPlots(numPlots, skipCityHomePlot), endPlots()); }
	city_plot_range plots(bool skipCityHomePlot = false) const { return city_plot_range(beginPlots(getNumCityPlots(), skipCityHomePlot), endPlots()); }

	int getCityPlotIndex(const CvPlot* pPlot) const;
	// Prefer to use plots() range instead of this for loops, searching etc.
	CvPlot* getCityIndexPlot(int iIndex) const;

	bool canWork(const CvPlot* pPlot) const;
	void verifyWorkingPlot(int iIndex);
	void verifyWorkingPlots();
	void clearWorkingOverride(int iIndex);
	int countNumImprovedPlots(ImprovementTypes eImprovement = NO_IMPROVEMENT, bool bPotential = false) const;
	int countNumWaterPlots() const;
	int countNumRiverPlots() const;

	int findPopulationRank() const;
	int findBaseYieldRateRank(YieldTypes eYield) const;
	int findYieldRateRank(YieldTypes eYield) const;
	int findCommerceRateRank(CommerceTypes eCommerce) const;


	// WHICH upgrade this city can train instead of eUnit -- the resolution behind carrying COMMITTED PRODUCTION
	// across when a queued unit stops being offered. It is not an availability question and not a second upgrade
	// closure: the tri-state has already folded the upgrade-tree dormancy that made eUnit un-offered
	// (enabler.md §8), so the answer is the NEAREST of its upgrades this city actually LISTS. Breadth-first, so
	// a chain resolves to the closest trainable rung, and cycle-safe by construction (a visited set).
	UnitTypes trainableUpgradeFor(UnitTypes eUnit) const;

	// THE HYPOTHETICAL -- "could this city construct eCandidate if it ALSO had eExtraBuilding?", the AI's
	// what-would-this-unlock valuation. It is the enabler.md §8 overlay pattern, not a what-if flag on a read:
	// the maintained operating set is COPIED into the caller's own scratch, the hypothetical building added to
	// the copy, and the ONE gate re-evaluated over it. ⛔ It never writes the maintained planes -- a hypothetical
	// that mutated them would leave the real frontier describing a game state that never happened.
	// ⚠ It EVALUATES (unlike every read above), so it is a per-decision call and never an inner-loop one.
	bool couldConstructWith(BuildingTypes eCandidate, BuildingTypes eExtraBuilding) const;
	bool isWorldWondersMaxed() const;
	bool isTeamWondersMaxed() const;
	bool isNationalWondersMaxed() const;
	int getMaxNumWorldWonders() const;
	int getMaxNumTeamWonders() const;
	int getMaxNumNationalWonders() const;



	//	KOSHLING - cache can build results

	bool canCreate(ProjectTypes eProject, bool bContinue = false, bool bTestVisible = false) const;
	bool canMaintain(ProcessTypes eProcess) const;

	int getFoodTurnsLeft() const;
	bool isProduction() const;
	bool isProductionLimited() const;
	bool isProductionUnitCombat(int iIndex) const;
	bool isProductionUnit() const;
	bool isProductionBuilding() const;
	bool isProductionProject() const;
	bool isProductionProcess() const;

	bool canContinueProduction(const OrderData& order) const;
	int getProductionExperience(UnitTypes eUnit = NO_UNIT) const;
	void addProductionExperience(CvUnit* pUnit, bool bConscript = false);

	UnitTypes getProductionUnit() const;
	UnitAITypes getProductionUnitAI() const;
	BuildingTypes getProductionBuilding() const;
	ProjectTypes getProductionProject() const;
	ProcessTypes getProductionProcess() const;
	const wchar_t* getProductionName() const;
	const wchar_t* getProductionNameKey() const;
	int getGeneralProductionTurnsLeft() const;

	bool isFoodProduction() const;
	bool isFoodProduction(const OrderData& order) const;
	bool isFoodProduction(UnitTypes eUnit) const;

	int getFirstUnitOrder(UnitTypes eUnit) const;
	int getFirstBuildingOrder(BuildingTypes eType) const;
	int getFirstProjectOrder(ProjectTypes eProject) const;
	int getNumTrainUnitAI(UnitAITypes eUnitAI) const;

	int getProductionProgress() const;
	int getProductionNeeded() const;
	int getProductionNeeded(const OrderData& order) const;
	int getProductionNeeded(UnitTypes eUnit) const;
	int getProductionNeeded(BuildingTypes eType) const;
	int getProductionNeeded(ProjectTypes eProject) const;
	int getOrderProductionTurnsLeft(const OrderData& order, int iIndex = 0) const;

	// For fractional production calculations:
	int getTotalProductionQueueTurnsLeft() const;
	int getProductionTurnsLeft() const;
	int getProductionTurnsLeft(UnitTypes eUnit, int iNum) const;
	int getProductionTurnsLeft(BuildingTypes eType, int iNum) const;
	int getProductionTurnsLeft(ProjectTypes eProject, int iNum) const;
	int getProductionTurnsLeft(int iProductionNeeded, int iProduction, int iFirstProductionDifference, int iProductionDifference) const;

	void setProductionProgress(int iNewValue);
	void changeProduction(int iChange);
	int numQueuedUnits(UnitAITypes contractedAIType, const CvPlot* contractedPlot) const;

	int getProductionModifier(const OrderData& order) const;
	int getProductionModifier() const;

	int getProductionModifier(UnitTypes eUnit) const;
	int getProductionModifier(BuildingTypes eType) const;
	int getProductionModifier(ProjectTypes eProject) const;

	int getProductionPerTurn(ProductionCalc::flags flags) const;

	int getProductionDifference(const OrderData& orderData, ProductionCalc::flags flags) const;
	int getCurrentProductionDifference(ProductionCalc::flags flags) const;

	bool canHurry(const HurryTypes eHurry, const bool bTestVisible = false) const;
	int64_t getHurryGold(const HurryTypes eHurry, int iHurryCost = -1) const;
	void hurry(HurryTypes eHurry);
	bool hurryOverflow(HurryTypes eHurry, int* iProduction, int* iGold, bool bCountThisTurn = false) const;

	UnitTypes getConscriptUnit() const;
	CvUnit* initConscriptedUnit();
	int getConscriptPopulation() const;
	int flatConscriptAngerLength() const;
	bool canConscript(bool bOnCapture = false) const;
	void conscript(bool bOnCapture = false);
	void emergencyConscript();

	void processBonus(BonusTypes eBonus, int iChange);

	void processBuilding(const BuildingTypes eType, const int iChange, const bool bReligiously = false, const bool bAlphaOmega = false);
	void processProcess(ProcessTypes eProcess, int iChange);
	void processSpecialist(SpecialistTypes eSpecialist, int iChange);

	HandicapTypes getHandicapType() const;
	CivilizationTypes getCivilizationType() const;
	LeaderHeadTypes getPersonalityType() const;
	DllExport ArtStyleTypes getArtStyleType() const;
	CitySizeTypes getCitySizeType() const;
	DllExport const CvArtInfoBuilding* getBuildingArtInfo(BuildingTypes eType) const;
	DllExport float getBuildingVisibilityPriority(BuildingTypes eType) const;

	bool hasTrait(TraitTypes eTrait) const;
	bool isNPC() const;
	bool isHominid() const;
	bool isHuman() const;
	DllExport bool isVisible(TeamTypes eTeam, bool bDebug) const;

	bool isCapital() const;
	bool isCoastal(int iMinWaterSize) const;
	bool isDisorder() const;
	bool isHolyCity(ReligionTypes eIndex) const;
	bool isHolyCity() const;
	bool isHeadquarters(CorporationTypes eIndex) const;
	bool isHeadquarters() const;
	void setHeadquarters(CorporationTypes eIndex);

	int getOvercrowdingPercentAnger(int iExtra = 0) const;
	int getNoMilitaryPercentAnger() const;
	int getCulturePercentAnger() const;
	int getReligionPercentAnger() const;
	int getHurryPercentAnger(int iExtra = 0) const;
	int getConscriptPercentAnger(int iExtra = 0) const;
	int getDefyResolutionPercentAnger(int iExtra = 0) const;
	int getWarWearinessPercentAnger() const;
	int getAngerPercent(const int iExtra = 0) const;

	int getRevRequestPercentAnger(int iExtra = 0) const;
	int getRevIndexPercentAnger() const;
	int getRevSuccessHappiness() const;

	int getVassalHappiness() const;
	int getVassalUnhappiness() const;

	int getCelebrityHappiness() const;

	// THE REALIZED wellbeing channels -- the DEPOSITS (getWellbeing) plus the raw-state inputs no deposit
	// produces (modifier.md §2b). Distinct from getWellbeing on purpose: that one answers in the vocabulary a
	// CANDIDATE also answers in, so the two compose; this one is the city's own level and composes with nothing.
	// A consumer wanting ONE side of a pair reads the array -- there is no per-side getter, and the four legacy
	// level getters it replaces are deleted, not renamed ([DEC-new-getter-surface]).
	void realizedWellbeing(int iExtraPopulation, int (&wellbeing)[NUM_WELLBEING_CHANNELS]) const;
	// The opposing-pair NETS, in WHOLE faces / health points (signed -- a surplus is as meaningful as a
	// deficit). The pairing itself lives once on the calc surface (InfoValuation::netHappiness/netHealth).
	int netHappiness(int iExtraPopulation = 0) const;
	int netHealth(int iExtraPopulation = 0) const;
	int angryPopulation(int iExtra = 0) const;

	int visiblePopulation() const;
	int totalFreeSpecialists() const;
	int extraPopulation() const;
	int extraSpecialists() const;
	int extraFreeSpecialists() const;

	int unhealthyPopulation(int iExtra = 0) const;
	int totalBadBuildingHealth() const;
	int healthRate(int iExtra = 0) const;
	int getPopulationPlusProgress(const int iExtra) const;
	int getFoodConsumedPerPopulation(const int iExtra = 0) const;
	int getFoodConsumedByPopulation(const int iExtra = 0) const;
	int foodConsumption(const bool bNoAngry=false, const int iExtra=0, const bool bIncludeWastage=true) const;
	int foodDifference(const bool bBottom=true, const bool bIncludeWastage=true, const bool bIgnoreFoodBuildOrRev=false) const;
	float foodWastage(int surplass = -1) const;
	int growthThreshold(const int iPopChange = 0) const;

	int productionLeft() const;
	int hurryCost() const;
	int getHurryCostModifier() const;
	int hurryPopulation(HurryTypes eHurry) const;
	int hurryProduction(HurryTypes eHurry) const;
	int flatHurryAngerLength() const;
	int hurryAngerLength(HurryTypes eHurry) const;
	int maxHurryPopulation() const;

	int netRevoltRisk(PlayerTypes cultureAttacker) const;
	int baseRevoltRisk(PlayerTypes eCultureAttacker) const;
	int unitRevoltRiskModifier(PlayerTypes eCultureAttacker) const;

	//	Note arrival or leaving of a unit
	void noteUnitMoved(const CvUnit* pUnit) const;
	int getGlobalSourcedProperty(PropertyTypes eProperty) const;
	int getTotalBuildingSourcedProperty(PropertyTypes eProperty) const;
	int getTotalUnitSourcedProperty(PropertyTypes eProperty) const;
	bool isActiveBuilding(BuildingTypes eIndex) const;
	bool isDormantBuilding(BuildingTypes eIndex) const;
	bool hasActiveWorldWonder() const;

	int getNumActiveWorldWonders() const;

	bool processGreatWall(bool bIn, bool bForce = false, bool bSeeded = true);

	int getReligionCount() const;
	int getCorporationCount() const;


	int getUnitCombatProductionModifier(UnitCombatTypes eIndex) const;
	void changeUnitCombatProductionModifier(UnitCombatTypes eIndex, int iChange);
	int getUnitCombatDefenseAgainstModifierTotal(UnitCombatTypes eIndex) const;
	void changeUnitCombatDefenseAgainstModifierTotal(UnitCombatTypes eIndex, int iChange);




	//TB Combat Mods (Buildings) end
	bool isQuarantined() const;
	int getQuarantinedCount() const;
	void changeQuarantinedCount(int iChange);
	// The CITIZEN-JUGGLE bracket (see CvCity.cpp): the governor's probe run defers its side-effect layer and
	// replays the run's NET once at the close. Refcounted so a nested bracket cannot close the outer one.
	void startCitizenJuggling();
	void endCitizenJuggling();
	bool isCitizenJuggling() const { return m_iCitizenJugglingCount > 0; }

	void resetQuarantinedCount();


	DllExport int getID() const;
	int getIndex() const;
	DllExport IDInfo getIDInfo() const;
	void setID(int iID);

	DllExport int getViewportX() const;
	inline int getX() const { return m_iX; }
	DllExport int getViewportY() const;
	inline int getY() const { return m_iY; }
	bool isInViewport() const;
	bool at(int iX, int iY) const;
	bool at(const CvPlot* pPlot) const;
	CvPlot* plot() const;
	DllExport CvPlot* plotExternal() const;
	CvPlotGroup* plotGroup(const PlayerTypes ePlayer) const;
	bool isConnectedTo(const CvCity* pCity) const;
	bool isConnectedToCapital(const PlayerTypes ePlayer = NO_PLAYER) const;
	int getArea() const;
	CvArea* area() const;
	CvArea* waterArea(const bool bNoImpassable = false) const;
	CvArea* secondWaterArea() const;
	CvArea* sharedWaterArea(const CvCity* pCity) const;
	bool isBlockaded() const;

	CvPlot* getRallyPlot() const;
	void setRallyPlot(const CvPlot* pPlot);

	int getGameTurnFounded(const bool bACalendar = false) const;
	void setGameTurnFounded(const int iNewValue, const bool bACalendar = false);

	int getGameDateFounded(const bool bACalendar = false) const;
	void setGameDateFounded(const int iNewValue, const bool bACalendar = false);

	int getGameTurnAcquired(const bool bHistoricalCalendar = false) const;
	int getGameDateAcquired(const bool bHistoricalCalendar = false) const;
	void setGameTurnAcquired(const int iNewValue, const bool bHistoricalCalendar = false);

	int getPopulation() const;
	void setPopulation(int iNewValue, bool bNormal = true);
	void changePopulation(int iChange);

	int64_t getRealPopulation() const;

	int getHighestPopulation() const;
	void setHighestPopulation(int iNewValue);

	int getWorkingPopulation() const;
	void changeWorkingPopulation(int iChange);

	int getSpecialistPopulation() const;
	void changeSpecialistPopulation(int iChange);

	int getNumGreatPeople() const;
	void changeNumGreatPeople(int iChange);

	int getBaseGreatPeopleRate() const;
	int getGreatPeopleRate() const;
	int getTotalGreatPeopleRateModifier() const;

	int getGreatPeopleRateModifier() const;
	void changeGreatPeopleRateModifier(int iChange);

	CvProperties* getProperties();
	const CvProperties* getPropertiesConst() const;

	int getAdditionalGreatPeopleRateByBuilding(BuildingTypes eType) const;
	int getAdditionalBaseGreatPeopleRateByBuilding(BuildingTypes eType) const;
	int getAdditionalGreatPeopleRateModifierByBuilding(BuildingTypes eType) const;

	int getAdditionalGreatPeopleRateBySpecialist(SpecialistTypes eSpecialist, int iChange) const;
	int getAdditionalBaseGreatPeopleRateBySpecialist(SpecialistTypes eSpecialist, int iChange) const;

	int getGreatPeopleProgress() const;
	void changeGreatPeopleProgress(int iChange);

	int getNumWorldWonders() const;
	void changeNumWorldWonders(int iChange);

	int getNumTeamWonders() const;
	void changeNumTeamWonders(int iChange);

	int getNumNationalWonders() const;
	void changeNumNationalWonders(int iChange);

	int getNumBuildings() const;
	void changeNumBuildings(int iChange);

	bool isGovernmentCenter() const;

	int getSavedMaintenanceByBuilding(BuildingTypes eType) const;
	int getSavedMaintenanceTimes100ByBuilding(BuildingTypes eType) const;

	// ⛔ A BARE FETCH of the derived cache -- never a gate test, never a recompute on the read path (the
	// `ensure()`-on-read protocol is tombstoned BY NAME, [superseded-ideas #14]). The MARK is what rebuilds:
	// markMaintenanceDirty() marks AND recomputes, which is the invariant that lets this read be bare.
	int64_t getMaintenance() const;
	int64_t getMaintenanceTimes100() const;
	// The realized value of ONE maintenance KIND -- the per-component breakdown read.
	int64_t maintenanceOfKind(int iKind) const;
	void markMaintenanceDirty() const;
	// The city's ONE additive maintenance percent stack, rolled over the scope chain the city sits under
	// (team + empire + city) by the cross-scope roll-up. It replaces the hand-summed city + player + area +
	// connected-city legs: there is no area scope ([state-repositories.md]), an `area` modifier authors at
	// EMPIRE, and the connected-city leg was never a KIND -- it is a CONDITION on an ordinary deposit
	// ([DEC-conditions-are-predicates]).
	int maintenancePercentStack(int iKind) const;
	void maintenanceLegs(int iKind, int64_t& flatSum, int64_t& percentSum) const;

	int getWarWearinessModifier() const;
	void changeWarWearinessModifier(int iChange);

	int getHurryAngerModifier() const;

	int getHealRate() const;
	void changeHealRate(int iChange);

	int getHealUnitCombatTypeTotal(UnitCombatTypes eUnitCombat) const;
	void changeHealUnitCombatTypeVolume(UnitCombatTypes eUnitCombat, int iChange);

	int getEspionageHealthCounter() const;
	void changeEspionageHealthCounter(int iChange);

	int getEspionageHappinessCounter() const;
	void changeEspionageHappinessCounter(int iChange);

	int getFreshWaterGoodHealth() const;
	void updateFreshWaterHealth();

// BUG - Feature Health - start
	void calculateFeatureHealthPercent(int& iGood, int& iBad) const;
	void calculateFeatureHealthPercentChange(int& iGood, int& iBad, CvPlot* pIgnorePlot = NULL) const;
	int getAdditionalHealthByFeature(FeatureTypes eFeature, int iChange) const;
	int getAdditionalHealthByFeature(FeatureTypes eFeature, int iChange, int& iGood, int& iBad) const;
	int getAdditionalHealth(int iGoodPercent, int iBadPercent, int& iGood, int& iBad) const;
// BUG - Feature Health - end

// BUG - Actual Effects - start
	int getAdditionalAngryPopuplation(int iGood, int iBad) const;
	int getAdditionalSpoiledFood(int iGood, int iBad, int iHealthAdjust = 0) const;
	int getAdditionalStarvation(int iSpoiledFood, int iFoodAdjust = 0) const;
// BUG - Actual Effects - end

	int getBuildingHealth(BuildingTypes eType) const;


	int getMilitaryHappiness() const;
	int getMilitaryHappinessUnits() const;
	void changeMilitaryHappinessUnits(int iChange);

	int getBuildingHappiness(BuildingTypes eType) const;


	int getAdditionalHealthByPlayerNoUnhealthyPopulation(int iExtraPop = 0, int iIgnoreNoUnhealthyPopulationCount = 0) const;
	int getAdditionalHealthByPlayerBuildingOnlyHealthy(int iIgnoreBuildingOnlyHealthyCount = 0) const;

	int getAdditionalHappinessByBuilding(BuildingTypes eType) const;
	int getAdditionalHappinessByBuilding(BuildingTypes eType, int& iGood, int& iBad, int& iAngryPop) const;


	int getAdditionalHealthByBuilding(BuildingTypes eType) const;
	int getAdditionalHealthByBuilding(BuildingTypes eType, int& iGood, int& iBad, int& iSpoiledFood, int& iStarvation) const;




	int getExtraHappiness() const;
	void changeExtraHappiness(int iChange);

	int getExtraHealth() const;
	void changeExtraHealth(int iChange);

	int getHurryAngerTimer() const;
	void changeHurryAngerTimer(int iChange);

	int getRevRequestAngerTimer() const;
	void changeRevRequestAngerTimer(int iChange);

	int getRevSuccessTimer() const;
	void changeRevSuccessTimer(int iChange);

	int getConscriptAngerTimer() const;
	void changeConscriptAngerTimer(int iChange);

	int getDefyResolutionAngerTimer() const;
	void changeDefyResolutionAngerTimer(int iChange);
	int flatDefyResolutionAngerLength() const;

	int getHappinessTimer() const;
	void changeHappinessTimer(int iChange);

	bool isNoUnhappiness() const;

	bool isNoUnhealthyPopulation() const;
	// The CITY-ONLY halves of the two composed verdicts above, for the what-if health calc, which weighs the city's
	// own amenity against the owner's separately and so must not read the OR.
	bool cityHasNoUnhealthyPopulation() const;
	bool cityHasBuildingOnlyHealthy() const;

	bool isBuildingOnlyHealthy() const;

	int getFood() const;
	void setFood(int iNewValue);
	void changeFood(int iChange, const bool bHandleGrowth = false);

	inline int getFoodKept() const { return m_iFoodKept; }
	void changeFoodKept(int iChange);

	int getFoodKeptPercent() const;

	int getMaxProductionOverflow() const;

	int getOverflowProduction() const;
	void setOverflowProduction(int iNewValue);
	void changeOverflowProduction(int iChange);

	int getFeatureProduction() const;
	void setFeatureProduction(int iNewValue);
	void changeFeatureProduction(int iChange);

	int getMilitaryProductionModifier() const;
	void changeMilitaryProductionModifier(int iChange);

	int getSpaceProductionModifier() const;
	void changeSpaceProductionModifier(int iChange);

	int getExtraTradeRoutes() const;

	int getMaxTradeRoutes() const;


	int getForeignTradeRouteModifier() const;
	void changeForeignTradeRouteModifier(int iChange);

	int getBuildingDefense() const;

	int getBombardDefense() const;
	int getAdditionalBombardDefenseByBuilding(BuildingTypes eType) const;


	int getCurrAirlift() const;
	void setCurrAirlift(int iNewValue);
	void changeCurrAirlift(int iChange);

	int getMaxAirlift() const;
	void changeMaxAirlift(int iChange);

	int getAirModifier() const;
	void changeAirModifier(int iChange);

	int getSMAirUnitCapacity(TeamTypes eTeam) const;
	int getAirUnitCapacity(TeamTypes eTeam) const;
	void changeAirUnitCapacity(int iChange);

	int getNukeModifier() const;
	void changeNukeModifier(int iChange);

	int getFreeSpecialist() const;

	int getPowerCount() const;
	bool isPower() const;

	// The per-city ISOLATED live-state object -- the (cityContext, plotGroup) building-output getters + the evaluator read it.
	// Maintained EVENT-DRIVEN (never a per-turn recompute): population on setPopulation, plot attributes on plot
	// enter/leave, vicinity bonuses on vicinity-supply events, religions/holyCity/corporations on their own events.
	// Vicinity/local only -- traded stays on CvPlotGroup.
	const CityContext& getCityContext() const { return m_cityContext; }
	// Plot ENTER (+1) / LEAVE (-1) -- fold the plot's HAS_/IS_ attributes into this city's context (plotAttrs, the one
	// stored aggregate). THE ONE APPLIER: fired from CvPlot::updateWorkingCity as a plot joins/leaves the city's
	// worked set at play, and by the contexts' spine consumer draining the in-read working-city facts at the
	// load-finish reseed (Engine/ContextConsumer -- DEC-spine-reseed).
	void onCityPlotChanged(const CvPlot* pPlot, int iSign) { m_cityContext.onPlotChanged(pPlot, iSign); }

	// The CITY-scope cascade package -- the one scope carrying BOTH sides of the origin rule (yield flats from
	// buildings + the percent stacks), PLUS this city's RECEIVER sums (the realized rates it consumes:
	// food/production/commerce/culture) riding the same cache beside the packages ([DEC-uniform-cache-shape]).
	// Marked ONLY by the modifier consumer's derived masks; recompute-only, never serialized.
	const CvCascadePackage<CvCity>& getCascadePackage() const { return m_cascadePackage; }
	// The package's refresh delegate (the CvDerivedCacheSet contract) -- delegates to the ONE gather.
	void refreshCascadePackage(int64_t iMask) const;

	// ---- THE ENABLER'S PER-CITY STATE (enabler.md §7.1) -- the "can I?" machine's host on this scope owner.
	// ⛔ NOT a value cache and carrying NO dirty protocol: there is no dirty->recompute path at all. Both are
	// built by the load reseed's events through the same O(delta) appliers play uses ([DEC-spine-reseed]) and
	// maintained by TARGETED PROPAGATION in place; a read is a bare O(1) fetch, so a propagation that fails to
	// fire leaves the set VISIBLY WRONG rather than silently healed ([DEC-no-self-heal]). Neither is serialized
	// -- empty from birth, so a loaded game is populated by the events, never from the save.
	// They are PUBLIC and MUTABLE by requirement, not laxity: the domain enablers reach them for write through a
	// `const CvCity&` (EnablerKernel / BuildingEnabler / UnitEnabler), the city being the owner of the storage
	// and not of the delta LOGIC.
	mutable CityEnabler m_enabler;                     // the constructible + trainable tri-state domains
	mutable OperatingBuildings m_operatingBuildings;   // the ACTIVE (non-dormant) set + its provided bonuses, at the
	                                                   // operate/provides least fixpoint -- what the MODIFIER reads
	                                                   // to decide which buildings deposit (enabler.md §3.2)

	// ---- THE AVAILABILITY READ SURFACE -- the ENABLER's "can I, right now?" half of the GAME-OBJECT read role
	// (patterns.md § THE TWO READ ROLES). Distinct from the INFO role's authored what-do-I-CARRY answer and from
	// the modifier's how-much groups beside it; the two machines stay separate ([DEC-enabler-not-cascade]).
	//
	// ONE READ PAIR PER DOMAIN -- the DOMAIN is the group, and the existing engine enum is the consumer's
	// vocabulary (BuildingTypes / UnitTypes indexes the question). The domain set is FIXED and small, so it grows
	// by DOMAIN, never by candidate; there is no per-candidate getter and no what-if argument.
	//
	// ⛔ EVERY READ IS A BARE O(1) LOOKUP THAT NEVER CALLS A CALCULATOR (enabler.md §7): the verdict already sits
	// in the maintained tri-state, put there by the events. A read never gates, never recomputes and never
	// evaluates `requires` -- so a missed propagation leaves a visibly wrong verdict rather than being silently
	// recomputed away ([DEC-no-self-heal]).
	//
	// The TRI-STATE is returned whole rather than reduced to a bool: HIDDEN vs GREYED is the "why not" the build
	// list needs, and §6's greying falls out of the same gate at no extra cost. Collapsing it here would force a
	// second read to recover it. It answers TREE + GATE only.
	// ⛔ The QUEUED overlay is deliberately NOT folded into it. EnablerDomain keeps FLAG_QUEUED separate from
	// FLAG_GATE_FAILED precisely so "already in the queue" stays distinguishable from "requires unmet"; mapping a
	// queued candidate onto GREYED here would destroy that distinction and lie about why it is not offered. The
	// overlay therefore rides the two reads that actually care -- the frontier, and the continue verdict.
	EnablerDomain::State getBuildingAvailability(BuildingTypes eBuilding) const;
	EnablerDomain::State getUnitAvailability(UnitTypes eUnit) const;

	// THE FRESH OFFER -- the small frontier the AI's production decision iterates INSTEAD of scoring the whole
	// entity database (enabler.md §6: one shared choice set for UI and AI), with queued candidates excluded.
	// Caller-owned vector, cleared and filled, so a hot caller reuses one buffer.
	void getAvailableBuildings(std::vector<int>& buildings) const;
	void getAvailableUnits(std::vector<int>& units) const;

	// THE CONTINUE VERDICT -- "may this in-progress build carry on?", reading PAST the queued overlay. It is its
	// own question, not a flag on the offer: a building in the queue IS queued by definition, so asking the fresh
	// offer about it would cancel every in-progress build on the production-check sweep.
	bool isBuildingContinuable(BuildingTypes eBuilding) const;

	// THE REALIZED YIELD GROUP -- the GAME-OBJECT read role's answer to "what do I HAVE, right now?" (patterns.md
	// § THE TWO READ ROLES). It is NOT the INFO role's authored what-do-I-CARRY answer and must never look like it.
	// ONE GETTER PER GROUP: the call carries NO channel argument -- YieldTypes indexes the RESULT -- and there is
	// no scalar getter per channel; a caller wanting one value indexes the group. The group grows by DATA, never
	// by a new getter. Values are x100 NATIVE ([DEC-fixedpoint-x100]): no `100` in the name, no scale variant, a
	// reader divides by 100 at the point of use. A BARE FETCH per channel, unconditionally -- nothing on a read
	// path recomputes, gates, or ensures (state-repositories.md; superseded-ideas #14).
	void getYields(int (&yields)[NUM_YIELD_TYPES]) const;

	// THE REST OF THE CITY'S GROUP SURFACE -- same read role, same grammar, one getter per modifier FAMILY the
	// CITY scope carries channels of (the set is the data's, CvInfoKinds.h's census scope masks). Each fills its
	// group's ×100 array through the ONE cross-scope roll-up (InfoValuation::realizedAtCity -- modifier.md §1's
	// downward roll realized AT READ over team + empire + (area × owner) + city), except the four channels the
	// city CONSUMES, which answer their maintained receiver sum. A channel this scope's data never authored
	// answers 0 with no storage anywhere.
	// NAMING: a group indexed by an EXISTING ENGINE enum takes the engine plural (getYields / getCommerces); a
	// group indexed by its family's own kind enum says so (get<Family>Kinds), which also keeps the whole
	// game-object group surface visually distinct from the legacy scalar getters it will replace -- four of
	// which (getMaintenance / getTradeRoutes / getDiplomacy / getStateReligion) hold the bare family name, and
	// overloading THOSE would make the two read roles look interchangeable.
	// ⛔ A FINAL-STATE value is NOT in any of these arrays (patterns.md rule 6): angryPopulation / healthRate and
	// their kin are computed DOWNSTREAM from the channels the wellbeing group hands out, never folded into a slot.
	// The four commerce channels are the ONE exception to the roll-up rule above, and not a fifth receiver slot:
	// the city consumes the COMMERCE YIELD, and the EMPIRE'S SLIDER PERCENTAGES split that yield into gold /
	// research / culture / espionage, each channel adding its own deposits on top (modifier.md §2a's commerce
	// paragraph; the arithmetic is InfoValuation::commerceSplit, never restated here). Culture is the lone dual
	// consumer: its deposits term is this city's maintained culture receiver sum, which the slider never scales.
	void getCommerces(int (&commerces)[NUM_COMMERCE_TYPES]) const;
	void expectedCommercesAtSliders(const int (&sliderPercents)[NUM_COMMERCE_TYPES],
									int (&commerces)[NUM_COMMERCE_TYPES]) const;
	// The four wellbeing channels (modifier.md §2b): happiness/anger/health/unhealth as four ORDINARY channels,
	// each a positive magnitude -- the opposing pairs are summed at the verdict, which is not a read.
	void getWellbeing(int (&wellbeing)[NUM_WELLBEING_CHANNELS]) const;
	void getDefenseKinds(int (&defenses)[NUM_DEFENSE_KINDS]) const;
	void getMaintenanceKinds(int (&maintenances)[NUM_MAINTENANCE_KINDS]) const;
	void getBuildRateKinds(int (&buildRates)[NUM_BUILD_RATE_KINDS]) const;
	void getCombatKinds(int (&combats)[NUM_COMBAT_KINDS]) const;
	void getExperienceKinds(int (&experiences)[NUM_EXPERIENCE_KINDS]) const;
	void getRevolutionKinds(int (&revolutions)[NUM_REVOLUTION_KINDS]) const;
	void getTradeRouteKinds(int (&tradeRoutes)[NUM_TRADE_ROUTE_KINDS]) const;
	void getHealKinds(int (&heals)[NUM_HEAL_KINDS]) const;
	void getUnderworldKinds(int (&underworlds)[NUM_UNDERWORLD_KINDS]) const;
	// Vision (vision.md): the city as an OBSERVER -- VISION_ELEVATION is what its buildings raise, and a
	// building can never elevate a unit passing through, which is why this is city-scoped and not the plot's.
	void getVisionKinds(int (&visions)[NUM_VISION_KINDS]) const;
	int sight() const;   // the city's sight BUDGET: its strength + what its buildings elevate
	// The straggler-scalar group (patterns.md getScalar, read as ONE group): every InfoScalar slot, each answered
	// at THIS scope -- the entries whose family the city carries hold a value, the rest answer 0.
	void getScalars(int (&scalars)[NUM_INFO_SCALARS]) const;
	// The RAW-STATE groups: live engine counters and the current order, which no deposit produces. They are
	// grouped for the same reason the deposit families are -- one read per group, indexed by the group's own
	// kind enum -- so the surface grows by groups rather than by a getter per counter.
	void getCountdowns(int (&countdowns)[NUM_CITY_COUNTDOWN_KINDS]) const;
	void getOrderRead(int (&order)[NUM_CITY_ORDER_READS]) const;
	// The hurry QUOTE for one method. eHurry selects WHICH hurry, so it is a call argument rather than a slot:
	// the registry is sparse and a city answers about one method at a time.
	void getHurryQuote(HurryTypes eHurry, int (&quote)[NUM_CITY_HURRY_QUOTES]) const;

	bool isAreaCleanPower() const;

	int getDefenseDamage() const;
	void changeDefenseDamage(int iChange);
	void changeDefenseModifier(int iChange);

	int getLastDefenseDamage() const;
	void setLastDefenseDamage(int iNewValue);

	bool isBombardable(const CvUnit* pUnit) const;
	int cascadeValue(ModifierFamily eFamily, int eKind) const;   // ANY (family,kind) slot; unit per infoKindUnit
	int keyedExperience(int iTargetSegment, int iTargetFk) const;   // the keyed experience.<scope>.{unitCombats|domains} legs
	int getDomainExperience(DomainTypes eDomain) const;             // experience.<scope>.domains.{DOMAIN}, every live source
	// Every (unitCombat, experience) this city's live sources deposit, collected ONCE. ⛔ A consumer wanting the
	// military subset filters THESE rows -- it never asks per id over the unitcombat registry, which is the
	// own-data inversion (pedia-read-map finding 2) and would be O(registry x sources) per call.
	void collectUnitCombatExperience(std::vector<std::pair<int, int> >& rows) const;
	int cascadeDefense(int eKind) const;
	int getTotalDefense(bool bIgnoreBuilding) const;
	int getDefenseModifier(bool bIgnoreBuilding) const;

	int getOccupationTimer() const;
	bool isOccupation() const;
	void setOccupationTimer(int iNewValue);
	void changeOccupationTimer(int iChange);

	int getCultureUpdateTimer() const;
	void setCultureUpdateTimer(int iNewValue);
	void changeCultureUpdateTimer(int iChange);

	int getCitySizeBoost() const;
	void setCitySizeBoost(int iBoost);

	bool isNeverLost() const;
	void setNeverLost(bool bNewValue);

	bool isBombarded() const;
	void setBombarded(bool bNewValue);

	bool isDrafted() const;
	void setDrafted(bool bNewValue);

	bool isAirliftTargeted() const;
	void setAirliftTargeted(bool bNewValue);

	bool isWeLoveTheKingDay() const;
	void setWeLoveTheKingDay(bool bNewValue);

	// --- CITY STATUS (Engine/CvStatus.h) -- an applied counter, ticking down, over at zero. The same shape
	// CvUnit carries one scope down; the gate IS the counter, so there is no per-status named accessor.
	int getStatus(CityStatus eStatus) const;
	bool hasStatus(CityStatus eStatus) const;
	void setStatus(CityStatus eStatus, int iTurns);
	void changeStatus(CityStatus eStatus, int iChange);
	void doStatusTurn();

	bool isCitizensAutomated() const;
	void setCitizensAutomated(bool bNewValue);

	bool isProductionAutomated() const;
	void setProductionAutomated(bool bNewValue);

	/* allows you to programatically specify a cities walls rather than having them be generated automagically */
	DllExport bool isWallOverride() const;
	void setWallOverride(bool bOverride);

	DllExport bool isInfoDirty() const;
	DllExport void setInfoDirty(bool bNewValue);

	DllExport bool isLayoutDirty() const;
	DllExport void setLayoutDirty(bool bNewValue);

	bool isPlundered() const;
	void setPlundered(bool bNewValue);

	DllExport inline PlayerTypes getOwner() const { return m_eOwner; }
	DllExport TeamTypes getTeam() const;

	PlayerTypes getPreviousOwner() const;
	void setPreviousOwner(PlayerTypes eNewValue);

	PlayerTypes getOriginalOwner() const;
	void setOriginalOwner(PlayerTypes eNewValue);

	CultureLevelTypes getCultureLevel() const;
	int getCultureThreshold() const;
	void setCultureLevel(CultureLevelTypes eNewValue, bool bUpdatePlotGroups);
	void updateCultureLevel(bool bUpdatePlotGroups);

	int getAdditionalYieldByBuilding(YieldTypes eIndex, BuildingTypes eType, bool bFilter = false) const;

	int getYieldBySpecialist(YieldTypes eIndex, SpecialistTypes eSpecialist) const;

	int getBaseYieldRateModifier(YieldTypes eIndex, int iExtra = 0) const;


	void onYieldChange();

	int getYieldRateModifier(YieldTypes eIndex) const;
	void changeYieldRateModifier(YieldTypes eIndex, int iChange);

	int getPowerYieldRateModifier(YieldTypes eIndex) const;
	void changePowerYieldRateModifier(YieldTypes eIndex, int iChange);


	int getTradeYield(YieldTypes eIndex) const;
	int totalTradeModifier(const CvCity* pOtherCity = NULL) const;
	int getPopulationTradeModifier() const;
	int getPeaceTradeModifier(TeamTypes eTeam) const;
	int getBaseTradeProfit(const CvCity* pCity) const;
#ifdef _MOD_FRACTRADE
	int calculateTradeProfitTimes100(const CvCity* pCity) const;
#endif
	int calculateTradeProfit(const CvCity* pCity) const;
	int calculateTradeYield(YieldTypes eIndex, int iTradeProfit) const;
	void calculateTradeTotals(YieldTypes eIndex, int& iDomesticYield, int& iDomesticRoutes, int& iForeignYield, int& iForeignRoutes, PlayerTypes eWithPlayer = NO_PLAYER, bool bRound = false, bool bBase = false) const;
	int calculateTotalTradeYield(YieldTypes eIndex, PlayerTypes eWithPlayer = NO_PLAYER, bool bRound = false, bool bBase = false) const;
	void setTradeYield(YieldTypes eIndex, int iNewValue);


	int getTotalCommerceRateModifier(CommerceTypes eIndex) const;

	int getProductionToCommerceModifier(CommerceTypes eIndex) const;
	void changeProductionToCommerceModifier(CommerceTypes eIndex, int iChange);

	int getAdditionalCommerceByBuilding(CommerceTypes eIndex, BuildingTypes eType) const;
	int getAdditionalCommerceRateModifierByBuilding(CommerceTypes eIndex, BuildingTypes eType) const;

	int getAdditionalCommerceBySpecialist(CommerceTypes eIndex, SpecialistTypes eSpecialist, int iChange) const;
	int getAdditionalBaseCommerceRateBySpecialist(CommerceTypes eIndex, SpecialistTypes eSpecialist, int iChange) const;


	void updateCorporation();
	void updateCorporationBonus();

	int getCommerceRateModifier(CommerceTypes eIndex) const;
	void recordCommerceRateModifierGrant(EventTypes eEvent, CommerceTypes eIndex, int iChange);




	int getDomainProductionModifier(DomainTypes eIndex) const;
	void changeDomainProductionModifier(DomainTypes eIndex, int iChange);

	int64_t getCulture(PlayerTypes eIndex) const;
	int64_t getCultureTimes100(PlayerTypes eIndex) const;
	int64_t countTotalCultureTimes100() const;
	PlayerTypes findHighestCulture() const;
	int calculateCulturePercent(PlayerTypes eIndex) const;
	int calculateTeamCulturePercent(TeamTypes eIndex) const;
	void setCulture(PlayerTypes eIndex, int64_t iNewValue, bool bPlots, bool bUpdatePlotGroups, bool bNationalSet = false);
	void setCultureTimes100(PlayerTypes eIndex, int64_t iNewValue, bool bPlots, bool bUpdatePlotGroups, bool bNationalSet = false);
	void changeCulture(PlayerTypes eIndex, int64_t iChange, bool bPlots, bool bUpdatePlotGroups);
	void changeCultureTimes100(PlayerTypes eIndex, int64_t iChange, bool bPlots, bool bUpdatePlotGroups);

	int getNumRevolts(PlayerTypes eIndex) const;
	void changeNumRevolts(PlayerTypes eIndex, int iChange);

	bool isTradeRoute(PlayerTypes eIndex) const;
	void setTradeRoute(PlayerTypes eIndex, bool bNewValue);

	bool isEverOwned(PlayerTypes eIndex) const;
	void setEverOwned(PlayerTypes eIndex, bool bNewValue);

	DllExport bool isRevealed(TeamTypes eIndex, bool bDebug) const;
	void setRevealed(TeamTypes eIndex, bool bNewValue);

	bool getEspionageVisibility(TeamTypes eTeam) const;
	void setEspionageVisibility(TeamTypes eTeam, bool bVisible, bool bUpdatePlotGroups);
	void updateEspionageVisibility(bool bUpdatePlotGroups);

	DllExport const CvWString getName(uint uiForm = 0) const;
	DllExport const wchar_t* getNameKey() const;
	void setName(const wchar_t* szNewValue, bool bFound = false);
	void doFoundMessage();

	// Script data needs to be a narrow string for pickling in Python
	std::string getScriptData() const;
	void setScriptData(std::string szNewValue);

	int getFreeBonus(BonusTypes eIndex) const;
	void changeFreeBonus(BonusTypes eIndex, int iChange);
	// The event/WorldBuilder grant path (persisted, see the member note). Distinct from changeFreeBonus, which
	// is the derived building supply -- routing a one-shot grant through that one would lose it on reload.
	void changeFreeBonusEvent(BonusTypes eIndex, int iChange);

	void processNumBonusChange(BonusTypes eIndex, int iOldValue, int iNewValue);
	void endDeferredBonusProcessing();
	void startDeferredBonusProcessing();
	int getNumBonusesFromBase(BonusTypes eIndex, int iBaseNum) const;

	int getNumBonuses(BonusTypes eIndex) const;
	bool hasBonus(BonusTypes eIndex) const;

	// WHAT THE NETWORK SUPPLIES THIS CITY, read straight off the plot group.
	// The CvPlotGroup is the ONLY authoritative list for trade resources and the city holds no mirror of it
	// ([state-repositories.md]) -- the network's content is aggregated UP from the member plots and cities
	// (CvPlot::updatePlotGroupBonus feeds CvPlotGroup::changeNumBonuses), so a second per-city copy of the same
	// number would be a duplicate of authoritative state with nothing but drift to gain ([tally.md]: the game
	// objects already own their counts; creating something new when we already have it is pointless).
	// This is the RAW held count -- before the minted gate (getNumBonusesFromBase), the TechCityTrade gate and
	// the per-city corporation add-on that getNumBonuses layers on top.
	int getNetworkBonusCount(BonusTypes eBonus) const;

	// The crossings. The city stores no count, so these exist ONLY to fire the presence transition
	// (processBonus + the corporation re-check) that a relayed read cannot announce for itself.
	// A count moving between two non-zero values announces nothing, by ruling ([event-spine.md]).
	void onNetworkBonusChanged(BonusTypes eBonus, int iOldCount, int iNewCount);
	void onNetworkSupplyChanged(const CvPlotGroup* pOldSupply, const CvPlotGroup* pNewSupply);
	// The city ITSELF arriving on / departing from its plot. UNCONDITIONAL, unlike the two above: the deferred
	// bracket works by snapshotting this city's live relayed read, which is already final the moment the city
	// exists -- so the snapshot/compare pass cannot see a supply that arrives WITH the city. It is an
	// initialization, not a delta.
	void onNetworkSupplyAcquired(const CvPlotGroup* pSupply);
	void onNetworkSupplyLost(const CvPlotGroup* pSupply);

	int getCorpBonusProduction(const BonusTypes eBonus) const;
	bool isCorporationBonus(BonusTypes eBonus) const;
	bool isActiveCorporation(CorporationTypes eCorporation) const;

	// How many hammers already put into production of the building
	int getProgressOnBuilding(const BuildingTypes eType) const;
	void setProgressOnBuilding(const BuildingTypes eType, int iNewValue);
	void changeProgressOnBuilding(const BuildingTypes eType, const int iChange);

	int getDelayOnBuilding(const BuildingTypes eType) const;
	void endDelayOnBuilding(const BuildingTypes eType);
	void tickDelayOnBuilding(const BuildingTypes eType, const bool bIncrement = true);

	bool isBuildingProductionDecay(BuildingTypes eIndex) const;
	int getBuildingProductionDecay(BuildingTypes eIndex) const;
	int getBuildingProductionDecayTurns(BuildingTypes eIndex) const;

	BuiltBuildingData getBuildingData(const BuildingTypes eType) const;

	int getProgressOnUnit(const UnitTypes eUnit) const;
	void setProgressOnUnit(const UnitTypes eUnit, int iNewValue);
	void changeProgressOnUnit(const UnitTypes eUnit, const int iChange);

	int getDelayOnUnit(const UnitTypes eUnit) const;
	void endDelayOnUnit(const UnitTypes eUnit);
	void tickDelayOnUnit(const UnitTypes eUnit, const bool bIncrement = true);

	bool isUnitProductionDecay(UnitTypes eIndex) const;
	int getUnitProductionDecay(UnitTypes eIndex) const;
	int getUnitProductionDecayTurns(UnitTypes eIndex) const;

	int getProjectProduction(ProjectTypes eIndex) const;
	void setProjectProduction(ProjectTypes eIndex, int iNewValue);
	void changeProjectProduction(ProjectTypes eIndex, int iChange);

	int getGreatPeopleUnitRate(UnitTypes eIndex) const;
	void setGreatPeopleUnitRate(UnitTypes eIndex, int iNewValue);
	void changeGreatPeopleUnitRate(UnitTypes eIndex, int iChange);

	int getGreatPeopleUnitProgress(UnitTypes eIndex) const;
	void setGreatPeopleUnitProgress(UnitTypes eIndex, int iNewValue);
	void changeGreatPeopleUnitProgress(UnitTypes eIndex, int iChange);

	int getSpecialistCount(SpecialistTypes eIndex) const;
	void setSpecialistCount(SpecialistTypes eIndex, int iNewValue);
	void changeSpecialistCount(SpecialistTypes eIndex, int iChange);
	void alterSpecialistCount(SpecialistTypes eIndex, int iChange);

	int getMaxSpecialistCount() const;
	int getMaxSpecialistCount(SpecialistTypes eIndex) const;
	bool isSpecialistValid(SpecialistTypes eIndex, int iExtra = 0) const;

	int getForceSpecialistCount(SpecialistTypes eIndex) const;
	bool isSpecialistForced() const;
	void setForceSpecialistCount(SpecialistTypes eIndex, int iNewValue);

	int getFreeSpecialistCount(SpecialistTypes eIndex) const;
	void setFreeSpecialistCount(SpecialistTypes eIndex, int iNewValue);
	void changeFreeSpecialistCount(SpecialistTypes eIndex, int iChange, bool bUnattributed = false);
	int getAddedFreeSpecialistCount(SpecialistTypes eIndex) const;

	int getImprovementFreeSpecialists(ImprovementTypes eIndex) const;
	void changeImprovementFreeSpecialists(ImprovementTypes eIndex, int iChange);

	uint32_t getReligionInfluence(ReligionTypes eIndex) const;
	void changeReligionInfluence(ReligionTypes eIndex, int iChange);



	bool isFreePromotion(PromotionTypes eIndex) const;


	int getEspionageDefenseModifier() const;

	bool isWorkingPlot(int iIndex) const;
	bool isWorkingPlot(const CvPlot* pPlot) const;
	void setWorkingPlot(int iCityPlotIndex, bool bNewValue);
	void alterWorkingPlot(int iIndex);
	void processWorkingPlot(int iPlot, int iChange, bool yieldsOnly = false);

	bool hasFullyActiveBuilding(const BuildingTypes eType) const;
	bool hasBuilding(const BuildingTypes eType) const;
	void changeHasBuilding(const BuildingTypes eType, const bool bNewValue);
	void setHasBuilding(const BuildingTypes eType, const bool bNewValue, const PlayerTypes eOriginalOwner, const int iOriginalTime, const bool bFirst = true);
	void setupBuilding(const CvBuildingInfo& kBuilding, const BuildingTypes eType, const bool bNewValue, const bool bFirst);
	void handleBuildingCounts(const BuildingTypes eType, const int iChange, const bool bWonder);

	void alterBuildingLedger(const BuildingTypes eType, const bool bAdd, const PlayerTypes eOwner = NO_PLAYER, const int iTime = MIN_INT);
	std::map<BuildingTypes, BuiltBuildingData> getBuildingLedger() const { return m_buildingLedger; }
	std::vector<BuildingTypes> getHasBuildings() const { return m_hasBuildings; }



	bool isHasReligion(ReligionTypes eIndex) const;
	void setHasReligion(ReligionTypes eIndex, bool bNewValue, bool bAnnounce, bool bArrows = true);

	void applyReligionModifiers(const ReligionTypes eIndex, const bool bValue);

	bool isHasCorporation(CorporationTypes eIndex) const;
	void setHasCorporation(CorporationTypes eIndex, bool bNewValue, bool bAnnounce, bool bArrows = true);
	void applyCorporationModifiers(CorporationTypes eIndex, bool bValue);

	CvCity* getTradeCity(int iIndex) const;
	int getTradeRoutes() const;
	void clearTradeRoutes();
	void updateTradeRoutes();

	void clearOrderQueue();
	void pushOrder(OrderTypes eOrder, int iData1, int iData2, bool bSave, bool bPop, bool bAppend, bool bForce = false, CvPlot* deliveryDestination = NULL, UnitAITypes contractedAIType = NO_UNITAI, uint8_t contractFlags = 0);
	void popOrder(int iNum, bool bFinish = false, bool bChoose = false, bool bResolveList = true);
	void startHeadOrder();
	void stopHeadOrder();

	int getOrderQueueLength() const;
	bst::optional<OrderData> getHeadOrder() const { return m_orderQueue.empty() ? bst::optional<OrderData>() : m_orderQueue.front(); }
	bst::optional<OrderData> getTailOrder() const { return m_orderQueue.empty() ? bst::optional<OrderData>() : m_orderQueue.back(); }
	OrderData getOrderAt(int index) const { return m_orderQueue[index]; }

	//CLLNode<OrderData>* nextOrderQueueNode(CLLNode<OrderData>* pNode) const;
	//CLLNode<OrderData>* headOrderQueueNode() const;
	DllExport int getNumOrdersQueued() const { return m_orderQueue.size(); };
	DllExport OrderData getOrderData(int iIndex) const;
	bool pushFirstValidBuildListOrder(int iListID);

	// fill the kVisible array with buildings that you want shown in city, as well as the number of generics
	// This function is called whenever CvCity::setLayoutDirty() is called
	DllExport void getVisibleBuildings(std::list<BuildingTypes>& kVisible, int& iNumGenerics);

	// Fill the kEffectNames array with references to effects in the CIV4EffectInfos.xml to have a
	// city play a given set of effects. This is called whenever the interface updates the city billboard
	// or when the zoom level changes
	DllExport void getVisibleEffects(ZoomLevelTypes eCurrentZoom, std::vector<const char*>& kEffectNames);


	// Billboard appearance controls
	DllExport void getCityBillboardSizeIconColors(NiColorA& kDotColor, NiColorA& kTextColor) const;
	DllExport const char* getCityBillboardProductionIcon() const;
	DllExport bool getFoodBarPercentages(std::vector<float>& afPercentages) const;
	DllExport bool getProductionBarPercentages(std::vector<float>& afPercentages) const;
	DllExport NiColorA getBarBackgroundColor() const;
	DllExport bool isStarCity() const;


	void setWallOverridePoints(const std::vector< std::pair<float, float> >& kPoints); /* points are given in world space ... i.e. PlotXToPointX, etc */
	DllExport const std::vector< std::pair<float, float> >& getWallOverridePoints() const;

	bool isEventTriggerPossible(EventTriggerTypes eTrigger) const;
	int getTriggerValue(EventTriggerTypes eTrigger) const;
	bool canApplyEvent(EventTypes eEvent, const EventTriggeredData& kTriggeredData) const;
	void applyEvent(EventTypes eEvent, const EventTriggeredData* pTriggeredData);
	bool isEventOccured(EventTypes eEvent) const;
	void setEventOccured(EventTypes eEvent, bool bOccured);

	int getBuildingYieldChange(BuildingTypes eType, YieldTypes eYield) const;
	void setBuildingYieldChange(BuildingTypes eType, YieldTypes eYield, int iChange);
	void changeBuildingYieldChange(BuildingTypes eType, YieldTypes eYield, int iChange);
	int getBuildingCommerceChange(BuildingTypes eType, CommerceTypes eCommerce) const;
	void setBuildingCommerceChange(BuildingTypes eType, CommerceTypes eCommerce, int iChange);
	void changeBuildingCommerceChange(BuildingTypes eType, CommerceTypes eCommerce, int iChange);
	void recordBuildingCommerceGrant(EventTypes eEvent, BuildingTypes eBuilding, CommerceTypes eCommerce, int iChange);
	int getBuildingHappyChange(BuildingTypes eType) const;
	void setBuildingHappyChange(BuildingTypes eType, int iChange);
	int getBuildingHealthChange(BuildingTypes eType) const;
	void setBuildingHealthChange(BuildingTypes eType, int iChange);

	// What ONE building delivers to this city's wellbeing, resolved through the ONE valuation against this
	// city's contexts (modifier.md §2b: four channels, the sign routing applied at fill). The two health
	// readers below split that one signed authored number; neither reads an info's health member, because
	// there is none to read -- a building's wellbeing IS its authored deposits.
	void buildingWellbeing(BuildingTypes eBuilding, int (&wellbeing)[NUM_WELLBEING_CHANNELS]) const;
	int getBuildingGoodHealth(BuildingTypes eBuilding) const;
	int getBuildingBadHealth(BuildingTypes eBuilding) const;

	PlayerTypes getLiberationPlayer(bool bConquest) const;
	void liberate(bool bConquest);

	DllExport int getMusicScriptId() const;
	DllExport int getSoundscapeScriptId() const;
	DllExport void cheat(bool bCtrl, bool bAlt, bool bShift);

	DllExport void getBuildQueue(std::vector<std::string>& astrQueue) const;

	int getAdditionalDefenseByBuilding(BuildingTypes eType) const;
	int getNumCityPlots() const;
	int getPopulationgrowthratepercentage() const;

	void changeFreshWater(int iChange);
	bool hasFreshWater() const;

	bool canUpgradeUnit(UnitTypes eUnit) const;

	int getBuildingProductionModifier(const BuildingTypes eIndex) const;
	void changeBuildingProductionModifier(const BuildingTypes eIndex, const int iChange);

	int getUnitProductionModifier(const UnitTypes eIndex) const;
	void changeUnitProductionModifier(const UnitTypes eIndex, const int iChange);

	bool hadVicinityBonus(BonusTypes eIndex) const;
	bool hadRawVicinityBonus(BonusTypes eIndex) const;

	int getBonusDefenseChanges(const BonusTypes eIndex) const;
	void changeBonusDefenseChanges(const BonusTypes eIndex, const int iChange);

	bool isBuiltFoodProducedUnit() const;
	void setBuiltFoodProducedUnit(bool bNewValue);
	void clearLostProduction();
	bool isProductionWonder() const;
	void updateYieldRate(BuildingTypes eType, YieldTypes eYield, int iChange);
	//int getImprovementYieldChange(ImprovementTypes eIndex1, YieldTypes eIndex2) const;
	//void changeImprovementYieldChange(ImprovementTypes eIndex1, YieldTypes eIndex2, int iChange);
	int calculateBonusDefense() const;

	void setCivilizationType(int iCiv);

	int getRevTrend() const;
	bool isInquisitionConditions() const;

	BuildTypes findChopBuild(FeatureTypes eFeature) const;
	int calculateBonusCommerceRateModifier(CommerceTypes eIndex) const;
	int getLandmarkAngerTimer() const;
	void changeLandmarkAngerTimer(int iChange);
	int getLandmarkAnger() const;
	void clearVicinityBonusCache(BonusTypes eBonus);
	bool hasVicinityBonus(BonusTypes eBonus) const;
	void clearRawVicinityBonusCache(BonusTypes eBonus);
	bool hasRawVicinityBonus(BonusTypes eBonus) const;
	void doVicinityBonus();
	bool isDevelopingCity() const;

	int getMintedCommerce() const;

	int getUnitCombatExtraStrength(UnitCombatTypes eIndex) const;
	void changeUnitCombatExtraStrength(UnitCombatTypes eIndex, int iChange);




	bool isZoneOfControl() const;

	int getAdjacentDamagePercent() const;

	int getWorkableRadiusOverride() const;
	void setWorkableRadiusOverride(int iNewVal);

	int getProtectedCultureCount() const;
	bool isProtectedCulture() const;
	void changeProtectedCultureCount(int iChange);


	void doAttack();

	void doCorporation();
	int getCorporationInfluence(CorporationTypes eCorporation) const;
	int64_t calcCorporateMaintenance() const;

	int getDisabledPowerTimer() const;
	void changeDisabledPowerTimer(int iChange);
	void doDisabledPower();

	int getWarWearinessTimer() const;
	void changeWarWearinessTimer(int iChange);
	void doWarWeariness();

	int getEventAnger() const;
	void changeEventAnger(int iChange);

	int getNonHolyReligionCount() const;

	void calculateExtraTradeRouteProfit(int iExtra, int* &aiTradeYields) const;

	int getMinimumDefenseLevel() const;
	void setMinimumDefenseLevel(int iNewValue);

	SpecialistTypes getBestSpecialist(int iExtra) const;


	void removeWorstCitizenActualEffects(int iNumCitizens, int& iGreatPeopleRate, int& iHappiness, int& iHealthiness, int*& aiYields, int*& aiCommerces) const;

	void changeHealthPercentPerPopulation(int iChange);
	int calculatePopulationHealth() const;
	void changeHappinessPercentPerPopulation(int iChange);
	int calculatePopulationHappiness() const;

	int getAssignedSpecialistCount() const;


	bool isAutomatedCanBuild(BuildTypes eBuild) const;
	void setAutomatedCanBuild(BuildTypes eBuild, bool bNewValue);

	virtual bool AI_isEmphasizeAvoidAngryCitizens() const = 0;
	virtual bool AI_isEmphasizeAvoidUnhealthyCitizens() const = 0;

	virtual int AI_plotValue(const CvPlot* pPlot, bool bAvoidGrowth, bool bRemove, bool bIgnoreFood = false, bool bIgnoreGrowth = false, bool bIgnoreStarvation = false) const = 0;

	virtual int AI_getMilitaryProductionRateRank() const = 0;
	virtual int AI_getNavalMilitaryProductionRateRank() const = 0;

	virtual bool AI_isMilitaryProductionCity() const = 0;
	virtual void AI_setMilitaryProductionCity(bool bNewVal) = 0;
	virtual bool AI_isNavalMilitaryProductionCity() const = 0;
	virtual void AI_setNavalMilitaryProductionCity(bool bNewVal) = 0;


	// The two-phase stream read: identity (the id alone) so the loader can register this city before the
	// body streams, then the body ([DEC-spine-reseed]). `read` is the single-phase entry for direct callers.
	void readIdentity(FDataStreamBase* pStream);
	void readBody(FDataStreamBase* pStream);
	void read(FDataStreamBase* pStream);
	void write(FDataStreamBase* pStream);

	virtual void AI_init() = 0;
	virtual void AI_reset() = 0;
	virtual void AI_doTurn() = 0;
	virtual void AI_assignWorkingPlots() = 0;
	virtual void AI_updateAssignWork() = 0;
	virtual bool AI_avoidGrowth() = 0;
	virtual int AI_specialistValue(SpecialistTypes eSpecialist, bool bAvoidGrowth, bool bRemove) const = 0;
	virtual void AI_chooseProduction() = 0;
	//	KOSHLING - initialisation called on every city prior to performing unit mission allocation logic
	//	This allows caches that will remian valid for the procesign of teh current turn's units to be cleared
	virtual void AI_preUnitTurn() = 0;
	virtual void AI_trained(UnitTypes eUnitType, UnitAITypes eUnitAIType) = 0;
	virtual UnitTypes AI_bestUnit(int& iBestValue, int iNumSelectableTypes = -1, UnitAITypes* pSelectableTypes = NULL, bool bAsync = false, UnitAITypes* peBestUnitAI = NULL, bool bNoRand = false, bool bNoWeighting = false, const CvUnitSelectionCriteria* criteria = NULL) = 0;
	virtual UnitTypes AI_bestUnitAI(UnitAITypes eUnitAI, int& iBestValue, bool bAsync = false, bool bNoRand = false, const CvUnitSelectionCriteria* criteria = NULL) = 0;


	// Represents a building with associated score as measured by the AI
	struct ScoredBuilding
	{
		ScoredBuilding(BuildingTypes building = NO_BUILDING, int64_t score = -1) : building(building), score(score) {}
		bool operator<(const ScoredBuilding& other) const { return score < other.score; }

		BuildingTypes building;
		int64_t score;

		// Get some interesting stats about a set of scored buildings
		static void averageMinMax(const std::vector<ScoredBuilding>& scores, float& averageScore, int64_t& minScore, int64_t& maxScore)
		{
			averageScore = 0;
			minScore = LLONG_MAX;
			maxScore = LLONG_MIN;
			foreach_(const ScoredBuilding& itr, scores)
			{
				averageScore = averageScore + itr.score / scores.size();
				minScore = std::min(minScore, itr.score);
				maxScore = std::max(maxScore, itr.score);
			}
		}
	};

	// Evaluate a predefined list of buildings based on the specified criteria, returning a sorted list of the buildings and their scores
	virtual bool AI_scoreBuildingsFromListThreshold(std::vector<ScoredBuilding>& scoredBuildings, const std::vector<BuildingTypes>& possibleBuildings, int iFocusFlags, int iMaxTurns, int iMinThreshold, bool bAsync, AdvisorTypes eIgnoreAdvisor = NO_ADVISOR, bool bMaximizeFlaggedValue = false, PropertyTypes eProperty = NO_PROPERTY) = 0;
	virtual int AI_buildingValue(BuildingTypes eType, int iFocusFlags = 0, bool bForTech = false, bool bDebug = false) = 0;
	virtual int AI_projectValue(ProjectTypes eProject) const = 0;
	virtual int AI_neededSeaWorkers() const = 0;
	// iNeedModifierPercent scales the needed-strength bar (>100 = demand a surplus before
	// answering "defended" - release hysteresis for garrison members, #384).
	virtual bool AI_isDefended(int iExtra = 0, bool bAllowAnyDefenders = true, int iNeedModifierPercent = 100) = 0;

	virtual bool AI_isAirDefended(bool bCountLand = 0, int iExtra = 0) = 0;
	virtual bool AI_isAdequateHappinessMilitary(int iExtra = 0) const = 0;

	virtual bool AI_isDanger() const = 0;
	virtual int evaluateDanger() = 0;
	virtual int AI_neededDefenders() = 0;
	virtual int AI_neededAirDefenders() const = 0;
	virtual int AI_minDefenders() const = 0;
	virtual bool AI_isEmphasizeAvoidGrowth() const = 0;
	virtual bool AI_isAssignWorkDirty() const = 0;
	virtual CvCity* AI_getRouteToCity() const = 0;
	virtual void AI_setAssignWorkDirty(bool bNewValue) = 0;
	virtual bool AI_isChooseProductionDirty() const = 0;
	virtual void AI_setChooseProductionDirty(bool bNewValue) = 0;
	virtual bool AI_isEmphasize(EmphasizeTypes eIndex) const = 0;
	virtual void AI_setEmphasize(EmphasizeTypes eIndex, bool bNewValue) = 0;
	virtual bool AI_isEmphasizeSpecialist(SpecialistTypes eIndex) const = 0;
	virtual void AI_setEmphasizeSpecialist(SpecialistTypes eIndex, bool bNewValue) = 0;
	virtual int AI_getBestBuildValue(int iIndex) const = 0;
	virtual void AI_markBestBuildValuesStale() = 0;

	virtual int AI_getTargetSize() const = 0;
	virtual int AI_getGoodTileCount() const = 0;
	virtual int AI_getImprovementValue(const CvPlot* pPlot, ImprovementTypes eImprovement, int iFoodPriority, int iProductionPriority, int iCommercePriority, int iFoodChange) const = 0;
	virtual void AI_getYieldMultipliers(int &iFoodMultiplier, int &iProductionMultiplier, int &iCommerceMultiplier, int &iDesiredFoodChange) const = 0;

	virtual int AI_totalBestBuildValue(const CvArea* pArea) const = 0;
	virtual int AI_countBestBuilds(const CvArea* pArea) const = 0;
	virtual BuildTypes AI_getBestBuild(int iIndex) const = 0;
	virtual void AI_updateBestBuild() = 0;
	virtual int AI_cityValue() const = 0;
	virtual int AI_clearFeatureValue(int iIndex) const = 0;

	virtual int AI_calculateCulturePressure(bool bGreatWork = false) const = 0;
	virtual int AI_calculateWaterWorldPercent() const = 0;
	virtual int AI_countNumBonuses(BonusTypes eBonus, bool bIncludeOurs, bool bIncludeNeutral, int iOtherCultureThreshold, bool bLand = true, bool bWater = true) const = 0;
	virtual int AI_yieldMultiplier(YieldTypes eYield) const = 0;
	virtual int AI_playerCloseness(PlayerTypes eIndex, int iMaxDistance = 7) = 0;
	virtual int AI_cityThreat(TeamTypes eTargetTeam = NO_TEAM, int* piThreatModifier = NULL) = 0;
	virtual BuildingTypes AI_bestAdvancedStartBuilding(int iPass) = 0;

	virtual int AI_getWorkersNeeded() const = 0;

	virtual int AI_getBuildPriority() const = 0;

	bool hasShrine(ReligionTypes eReligion) const;
	bool hasOrbitalInfrastructure() const;
	void processVoteSourceBonus(VoteSourceTypes eVoteSource, bool bActive);

	void invalidatePopulationRankCache();
	void invalidateYieldRankCache(YieldTypes eYield = NO_YIELD);
	void invalidateCommerceRankCache(CommerceTypes eCommerce = NO_COMMERCE);

	int getBestYieldAvailable(YieldTypes eYield) const;

	/*   note: recalculateCultureDistance must be const as it is called from cultureDistance, a     */
	/*     const function; this means that the actual cached structure must be mutable in order to  */
	/*     be modified in the const method                                                          */
	void recalculateCultureDistances(int iMaxDistance) const;
	int calculateCultureDistance(const CvPlot* mainPlot, int iMaxDistance) const;
	void clearCultureDistanceCache();
	int cultureDistance(const CvPlot& plot) const;

	void setBuildingListInvalid();
	bool getBuildingListFilterActive(BuildingFilterTypes eFilter) const;
	void setBuildingListFilterActive(BuildingFilterTypes eFilter, bool bActive);
	BuildingGroupingTypes getBuildingListGrouping();
	void setBuildingListGrouping(BuildingGroupingTypes eGrouping);
	BuildingSortTypes getBuildingListSorting();
	void setBuildingListSorting(BuildingSortTypes eSorting);
	int getBuildingListGroupNum();
	int getBuildingListNumInGroup(int iGroup);
	BuildingTypes getBuildingListType(int iGroup, int iPos);
	int getBuildingListSelectedBuildingRow();
	int getBuildingListSelectedWonderRow();
	void setBuildingListSelectedBuilding(BuildingTypes eType);
	void setBuildingListSelectedWonder(BuildingTypes eWonder);
	BuildingTypes getBuildingListSelectedBuilding();
	BuildingTypes getBuildingListSelectedWonder();

	void setUnitListInvalid();
	bool getUnitListFilterActive(UnitFilterTypes eFilter) const;
	void setUnitListFilterActive(UnitFilterTypes eFilter, bool bActive);
	UnitGroupingTypes getUnitListGrouping();
	void setUnitListGrouping(UnitGroupingTypes eGrouping);
	UnitSortTypes getUnitListSorting();
	void setUnitListSorting(UnitSortTypes eSorting);
	int getUnitListGroupNum();
	int getUnitListNumInGroup(int iGroup);
	UnitTypes getUnitListType(int iGroup, int iPos);
	int getUnitListSelectedRow();
	void setUnitListSelected(UnitTypes eUnit);
	UnitTypes getUnitListSelected();

	bool isDirectAttackable() const;

	void markForDestruction() { m_bMarkedForDestruction = true; }
	bool isMarkedForDestruction() const { return m_bMarkedForDestruction; }

protected:

	int m_iID;
	int m_iX;
	int m_iY;
	int m_iRallyX;
	int m_iRallyY;
	int m_iGameTurnFounded;
	int m_iGameTurnAcquired;
	int m_iPopulation;
	CityContext m_cityContext;   // per-city isolated live state (see getCityContext); event-maintained, never a per-turn recompute
	// the CITY-scope cascade package + receiver sums (see getCascadePackage); recompute-only, never serialized
	CvCascadePackage<CvCity> m_cascadePackage;
	int m_iHighestPopulation;
	int m_iWorkingPopulation;
	int m_iSpecialistPopulation;
	int m_iNumGreatPeople;
	int m_iGreatPeopleRateModifier;
	int m_iGreatPeopleProgress;
	int m_iNumWorldWonders;
	int m_iNumTeamWonders;
	int m_iNumNationalWonders;
	int m_iNumBuildings;
	int m_iWarWearinessModifier;
	int m_iHealRate;
	int m_iEspionageHealthCounter;
	int m_iEspionageHappinessCounter;
	int m_iFreshWaterGoodHealth;
	int m_iHurryAngerTimer;
	int m_iRevRequestAngerTimer;
	int m_iRevSuccessTimer;
	int m_iConscriptAngerTimer;
	int m_iDefyResolutionAngerTimer;
	int m_iHappinessTimer;
	int m_iMilitaryHappinessUnits;
	int m_iExtraHappiness;
	int m_iExtraHealth;
	int m_iFood;
	int m_iFoodKept;

	int m_iOverflowProduction;
	int m_iFeatureProduction;

	int m_iLostProductionModified;
	int m_iGoldFromLostProduction;
	int m_iCiv;

	bool m_bBuiltFoodProducedUnit;
	bool m_bResetTechs;
	int m_iLandmarkAngerTimer;
	int m_iWorkableRadiusOverride;
	int m_iProtectedCultureCount;
	int m_iDisabledPowerTimer;
	int m_iWarWearinessTimer;
	int m_iMinimumDefenseLevel;
	int m_iHappinessPercentPerPopulation;
	int m_iHealthPercentPerPopulation;

	int m_iLostProduction;
	int m_iEventAnger;

	int m_iFreshWater;


	// The free-bonus supply, split by NATURE (savemigration.txt: the old mixed ledger was retired).
	//   m_paiFreeBonus       -- the BUILDING half. Derived: maintained as buildings process their
	//                           `provides.bonuses`, so it serializes NOTHING and rebuilds from the reseed.
	//   m_paiFreeBonusEvents -- the EVENT/WorldBuilder half. Genuine one-shot NON-DERIVABLE state (save.md §5:
	//                           the one class that legitimately serializes), so it is SAVED separately. A
	//                           recompute cache would wipe it -- the m_aBuildingCommerceChangeEvents precedent.
	// getFreeBonus() sums the two.
	int* m_paiFreeBonus;
	int* m_paiFreeBonusEvents;
	mutable int* m_cachedPropertyNeeds;
	bool* m_pabHadVicinityBonus;
	bool* m_pabHadRawVicinityBonus;
	mutable bool* m_pabHasVicinityBonusCached;
	mutable bool* m_pabHasRawVicinityBonusCached;
	mutable bool* m_pabHasVicinityBonus;
	mutable bool* m_pabHasRawVicinityBonus;

	bool* m_bHasBuildings;
	int* m_paiUnitCombatExtraStrength;
	bool* m_pabAutomatedCanBuild;

	std::vector<BuildingTypes> m_hasBuildings;

	std::map<short, int> m_bonusDefenseChanges;
	std::map<short, int> m_buildingProductionMod;
	std::map<short, int> m_unitProductionMod;

	std::map<BuildingTypes, BuiltBuildingData> m_buildingLedger;

	int m_iMilitaryProductionModifier;
	int m_iSpaceProductionModifier;
	int m_iForeignTradeRouteModifier;
	int m_iCurrAirlift;
	int m_iMaxAirlift;
	int m_iAirModifier;
	int m_iAirUnitCapacity;
	int m_iNukeModifier;
	int m_iDefenseDamage;
	int m_iLastDefenseDamage;
	int m_iOccupationTimer;
	int m_iCultureUpdateTimer;
	int m_iCitySizeBoost;
	int m_iSpecialistInsidiousness;
	int m_iSpecialistInvestigation;
	// Mutable as its used in caching
	mutable int m_icachedPropertyNeedsTurn;

	int m_iQuarantinedCount;

	bool m_bNeverLost;
	bool m_bPropertyControlBuildingQueued;
	bool m_bBombarded;
	bool m_bDrafted;
	bool m_bAirliftTargeted;
	// CityStatus -> TURNS REMAINING. Replaces the hand-named WLTKD bool: WLTKD is a ONE-TURN status re-applied
	// each turn while its conditions hold, so the bool was its legacy shape (state.md).
	int m_aiStatusTurns[NUM_CITY_STATUSES];
	bool m_bCitizensAutomated;
	bool m_bProductionAutomated;
	bool m_bWallOverride;
	bool m_bInfoDirty;
	bool m_bLayoutDirty;
	bool m_bPlundered;
	bool m_bPopProductionProcess;

	PlayerTypes m_eOwner;
	PlayerTypes m_ePreviousOwner;
	PlayerTypes m_eOriginalOwner;
	CultureLevelTypes m_eCultureLevel;

	int m_iRevolutionIndex;
	int m_iLocalRevIndex;
	int m_iRevIndexAverage;
	int m_iRevIndexDistanceMod;
	int m_iRevolutionCounter;
	int m_iReinforcementCounter;

	//TB Combat Mod (Buildings)
	int* m_paiAidRate;
	int** m_ppaaiExtraBonusAidModifier;
	int* m_paiUnitCombatProductionModifier;
	int* m_paiUnitCombatDefenseAgainstModifier;
	//TB Building Tags
	int m_iExtraLocalCaptureProbabilityModifier;
	int m_iExtraLocalCaptureResistanceModifier;
	int m_iModifiedBuildingDefenseRecoverySpeedCap;

	int** m_ppaaiLocalSpecialistExtraYield;
	int m_iPrioritySpecialist;
	int* m_paiSpecialistBannedCount;
	int* m_paiHealUnitCombatTypeVolume;

	int* m_aiYieldRateModifier;
	int* m_aiPowerYieldRateModifier;
	int* m_aiTradeYield;
	int* m_aiProductionToCommerceModifier;
	int* m_aiDomainProductionModifier;
	int64_t* m_aiCulture;   // per-player culture, x100 and NEVER decaying -- an AMOUNT, so 64-bit
	int* m_aiNumRevolts;

	bool* m_abEverOwned;
	bool* m_abTradeRoute;
	bool* m_abRevealed;
	bool* m_abEspionageVisibility;

	CvWString m_szName;
	CvString m_szScriptData;

	int* m_paiProjectProduction;
	int* m_paiUnitProduction;
	int* m_paiGreatPeopleUnitRate;
	int* m_paiGreatPeopleUnitProgress;
	// Citizen-juggle bracket state -- purely transient run state: NEVER serialized, cleared by reset().
	int m_iCitizenJugglingCount;
	bool m_bJuggleDeferredSpec;
	bool m_bJuggleDeferredWork;
	std::vector<int> m_juggleSpecialistStart;
	std::vector<bool> m_juggleWorkedStart;
	int* m_paiSpecialistCount;
	int* m_paiForceSpecialistCount;
	int* m_paiFreeSpecialistCountUnattributed;
	int* m_paiImprovementFreeSpecialists;
	int* m_paiReligionInfluence;

	bool* m_pabWorkingPlot;
	bool* m_pabHasReligion;
	bool* m_pabHasCorporation;

	int	m_deferringBonusProcessingCount;
	int* m_paiStartDeferredSectionNumBonuses;

	CvProperties m_Properties;
	CvBuildingList m_BuildingList;
	CvUnitList m_UnitList;

	std::vector<IDInfo> m_paTradeCities;

	typedef std::vector<OrderData> OrderQueue;
	OrderQueue m_orderQueue;

	std::vector< std::pair<float, float> > m_kWallOverridePoints;
	std::vector< std::pair<BuildingTypes, int> > m_progressOnBuilding;
	std::vector< std::pair<BuildingTypes, int> > m_delayOnBuilding;
	std::vector< std::pair<UnitTypes, int> > m_progressOnUnit;
	std::vector< std::pair<UnitTypes, int> > m_delayOnUnit;
	std::vector< std::pair<BonusTypes, int> > m_corpBonusProduction;

	std::vector<EventTypes> m_aEventsOccured;
	std::vector<BuildingYieldChange> m_aBuildingYieldChange;
	std::vector<BuildingCommerceChange> m_aBuildingCommerceChange;

	// Values PYTHON EVENTS granted to this city -- one-shot state, kept out of every derived accumulator.
	CvEventGrantStore m_eventGrants;
	BuildingChangeArray m_aBuildingHappyChange;
	BuildingChangeArray m_aBuildingHealthChange;

	// CACHE: cache frequently used values
	mutable int	m_iPopulationRank;
	mutable bool m_bPopulationRankValid;
	int*	m_aiBaseYieldRank;
	bool*	m_abBaseYieldRankValid;
	int*	m_aiYieldRank;
	bool*	m_abYieldRankValid;
	int*	m_aiCommerceRank;
	bool*	m_abCommerceRankValid;

	mutable std::map<const CvPlot*,int> m_aCultureDistances;

	void doCulture();
	void doPlotCulture(PlayerTypes ePlayer, int iCultureRate);
	static int cultureDistanceDropoff(int baseCultureGain, int rangeOfSource, int distanceFromSource);
	void doProduction(bool bAllowNoProduction);
	void doDecay();
	void doReligion();
	void doGreatPeople();
	bool doCheckProduction();

	int getHurryCostModifier(UnitTypes eUnit) const;
	int getHurryCostModifier(BuildingTypes eType) const;
	int getHurryCostModifier(int iBaseModifier, int iExtraMod) const;
	int getHurryCost(UnitTypes eUnit) const;
	int getHurryCost(BuildingTypes eType) const;
	int getHurryCost(int iProductionLeft, int iHurryModifier) const;
	int getHurryPopulation(HurryTypes eHurry, int iHurryCost) const;
	bool canHurryUnit(HurryTypes eHurry, UnitTypes eUnit) const;
	bool canHurryBuilding(HurryTypes eHurry, BuildingTypes eType) const;
	void recalculateMaxFoodKeptPercent();
	virtual bool AI_addBestCitizen(bool bWorkers, bool bSpecialists, int* piBestPlot = NULL, SpecialistTypes* peBestSpecialist = NULL) = 0;
	virtual bool AI_removeWorstCitizen(SpecialistTypes eIgnoreSpecialist = NO_SPECIALIST) = 0;


	//TB Building tags
	void setExtraLocalCaptureProbabilityModifier(int iValue);
	void changeExtraLocalCaptureProbabilityModifier(int iChange);

	void setExtraLocalCaptureResistanceModifier(int iValue);
	void changeExtraLocalCaptureResistanceModifier(int iChange);

	short m_iZoCCount;
	void changeZoCCount(short iChange);

	bool m_bMarkedForDestruction;

public:
	int localCitizenCaptureResistance() const;
	int getLocalSpecialistExtraYield(SpecialistTypes eSpecialist, YieldTypes eYield) const;

private:


	void updateExtraTechHappiness();
	int getTechHealth(TechTypes eTech) const;
	void changeLocalSpecialistExtraYield(SpecialistTypes eSpecialist, YieldTypes eYield, int iChange);

public:
	int specialistCount(SpecialistTypes eSpecialist) const;
	int specialistYield(SpecialistTypes eSpecialist, YieldTypes eYield) const;
	int specialistCommerce(SpecialistTypes eSpecialist, CommerceTypes eCommerce) const;
	int specialistYieldTotal(SpecialistTypes eSpecialist, YieldTypes eYield) const;

	int getPrioritorizedSpecialist() const;
	void setPrioritorizedSpecialist(SpecialistTypes eSpecialist);

	bool isSpecialistBanned(SpecialistTypes eSpecialist) const;
	void banSpecialist(SpecialistTypes eSpecialist);
	void removeSpecialistBan(SpecialistTypes eSpecialist);


#ifdef YIELD_VALUE_CACHING
	virtual void AI_NoteWorkerChange() = 0;
	virtual void AI_NoteSpecialistChange() = 0;
public:
	virtual void ClearYieldValueCache() = 0;
#ifdef _DEBUG
	virtual void CheckYieldValueCache(char* label) = 0;
#define CHECK_YIELD_VALUE_CACHE(label) CheckYieldValueCache(label);
#else
#define CHECK_YIELD_VALUE_CACHE(label) ;
#endif
#endif

public:
	int getExtraLocalCaptureProbabilityModifier() const;
	int getExtraLocalCaptureResistanceModifier() const;


	int getExtraLocalDynamicDefense() const;

	int getExtraRiverDefensePenalty() const;

	int getExtraMinDefense() const;

	int getExtraBuildingDefenseRecoverySpeedModifier() const;

	int getModifiedBuildingDefenseRecoverySpeedCap() const;
	void setModifiedBuildingDefenseRecoverySpeedCap(int iValue);
	void changeModifiedBuildingDefenseRecoverySpeedCap(int iChange);

	int getExtraCityDefenseRecoverySpeedModifier() const;


	int cityDefenseRecoveryRate() const;
	int getInvestigationTotal(bool bActual = false) const;

	int getExtraInsidiousness() const;

	int getExtraInvestigation() const;

	int getSpecialistInsidiousness() const;
	void changeSpecialistInsidiousness(int iChange);
	int getSpecialistInvestigation() const;
	void changeSpecialistInvestigation(int iChange);

	int getPropertyNeed(PropertyTypes eProperty) const;

	void AI_setPropertyControlBuildingQueued(bool bSet);
	bool AI_isPropertyControlBuildingQueued() const;

	const CityOutputHistory* getCityOutputHistory() const;

private:
	mutable stdext::hash_map<UnitTypes,UnitTypes> m_eCachedAllUpgradesResults;
	mutable stdext::hash_map<UnitTypes,UnitTypes> m_eCachedAllUpgradesResultsRoot;

	mutable std::map<int,int> m_buildingSourcedPropertyCache;
	mutable std::map<int,int> m_unitSourcedPropertyCache;

	bool m_bIsGreatWallSeed;
	bool m_bVisibilitySetup;

	// The city's realized MAINTENANCE, on the ONE standardized derived-cache component -- never a hand-rolled
	// dirty-flag/value pair beside it ([DEC-uniform-cache-shape]: a hand-named scalar cannot be addressed
	// uniformly, so it forces a bespoke invalidation path per field, which is how ~33 of them accumulated).
	// Recompute-only and NEVER serialized ([DEC-derived-never-trusted] / [save.md §5]) -- dirty-on-construct
	// means a loaded game recomputes from current state rather than trusting a save's stale number.



	// The UPGRADE cache (the AI's upgrade-resolution memo) -- distinct from any trainability memo: it caches the
	// upgrade-chain resolution, not an availability verdict, so it stands on its own.
public:
	void clearUpgradeCache(UnitTypes eUnit) const;
protected:
	void invalidateCachedCanTrainForUnit(UnitTypes eUnit) const;

public:
	//
	// Algorithm/range helpers
	//
	struct fn {
		DECLARE_MAP_FUNCTOR(CvCity, void, startDeferredBonusProcessing);
		DECLARE_MAP_FUNCTOR(CvCity, void, endDeferredBonusProcessing);
		DECLARE_MAP_FUNCTOR(CvCity, void, doTurn);
		DECLARE_MAP_FUNCTOR(CvCity, void, updateCorporation);
		DECLARE_MAP_FUNCTOR(CvCity, void, onYieldChange);
		DECLARE_MAP_FUNCTOR(CvCity, void, clearTradeRoutes);
		DECLARE_MAP_FUNCTOR(CvCity, void, setBuildingListInvalid);
		DECLARE_MAP_FUNCTOR(CvCity, void, ClearYieldValueCache);
		DECLARE_MAP_FUNCTOR(CvCity, void, AI_preUnitTurn);
		DECLARE_MAP_FUNCTOR(CvCity, void, AI_assignWorkingPlots);
		DECLARE_MAP_FUNCTOR(CvCity, void, AI_updateAssignWork);
		DECLARE_MAP_FUNCTOR(CvCity, void, AI_markBestBuildValuesStale);
		DECLARE_MAP_FUNCTOR(CvCity, void, setupGraphical);
		DECLARE_MAP_FUNCTOR(CvCity, void, invalidatePopulationRankCache);
		DECLARE_MAP_FUNCTOR(CvCity, void, invalidateYieldRankCache);
		DECLARE_MAP_FUNCTOR(CvCity, void, invalidateCommerceRankCache);

		DECLARE_MAP_FUNCTOR_1(CvCity, void, setLayoutDirty, bool);
		DECLARE_MAP_FUNCTOR_1(CvCity, void, AI_setAssignWorkDirty, bool);
		DECLARE_MAP_FUNCTOR_1(CvCity, void, AI_setChooseProductionDirty, bool);
		DECLARE_MAP_FUNCTOR_1(CvCity, void, AI_setMilitaryProductionCity, bool);
		DECLARE_MAP_FUNCTOR_1(CvCity, void, AI_setNavalMilitaryProductionCity, bool);
		DECLARE_MAP_FUNCTOR_1(CvCity, void, kill, bool);
		DECLARE_MAP_FUNCTOR_1(CvCity, void, setInfoDirty, bool);
		DECLARE_MAP_FUNCTOR_1(CvCity, void, changeFood, int);

		DECLARE_MAP_FUNCTOR_2(CvCity, void, setBuildingListFilterActive, BuildingFilterTypes, bool);
		DECLARE_MAP_FUNCTOR_2(CvCity, void, changeFreeSpecialistCount, SpecialistTypes, int);
		DECLARE_MAP_FUNCTOR_2(CvCity, void, processVoteSourceBonus, VoteSourceTypes, bool);

		DECLARE_MAP_FUNCTOR_CONST(CvCity, bool, isCapital);
		DECLARE_MAP_FUNCTOR_CONST(CvCity, bool, isNoUnhappiness);
		DECLARE_MAP_FUNCTOR_CONST(CvCity, bool, hasOrbitalInfrastructure);
		DECLARE_MAP_FUNCTOR_CONST(CvCity, bool, isConnectedToCapital);
		DECLARE_MAP_FUNCTOR_CONST(CvCity, bool, isGovernmentCenter);
		DECLARE_MAP_FUNCTOR_CONST(CvCity, bool, AI_isMilitaryProductionCity);
		DECLARE_MAP_FUNCTOR_CONST(CvCity, bool, AI_isNavalMilitaryProductionCity);
		DECLARE_MAP_FUNCTOR_CONST(CvCity, int64_t, calcCorporateMaintenance);
		DECLARE_MAP_FUNCTOR_CONST(CvCity, int, getID);
		DECLARE_MAP_FUNCTOR_CONST(CvCity, int, getPopulation);
		DECLARE_MAP_FUNCTOR_CONST(CvCity, int64_t, getRealPopulation);
		DECLARE_MAP_FUNCTOR_CONST(CvCity, int, netHappiness);
		DECLARE_MAP_FUNCTOR_CONST(CvCity, int, netHealth);
		DECLARE_MAP_FUNCTOR_CONST(CvCity, const CvWString, getName);
		DECLARE_MAP_FUNCTOR_CONST(CvCity, const CvArea*, area);
		DECLARE_MAP_FUNCTOR_CONST(CvCity, const CvPlot*, plot);

		DECLARE_MAP_FUNCTOR_CONST_1(CvCity, bool, hasFullyActiveBuilding, BuildingTypes);
		DECLARE_MAP_FUNCTOR_CONST_1(CvCity, bool, isHasReligion, ReligionTypes);
		DECLARE_MAP_FUNCTOR_CONST_1(CvCity, bool, isHasCorporation, CorporationTypes);
		DECLARE_MAP_FUNCTOR_CONST_1(CvCity, bool, hasBonus, BonusTypes);
		DECLARE_MAP_FUNCTOR_CONST_1(CvCity, void, clearUpgradeCache, UnitTypes);
		DECLARE_MAP_FUNCTOR_CONST_1(CvCity, bool, isCoastal, int);
		DECLARE_MAP_FUNCTOR_CONST_1(CvCity, bool, hasBuilding, BuildingTypes);
		DECLARE_MAP_FUNCTOR_CONST_1(CvCity, bool, isActiveBuilding, BuildingTypes);
		DECLARE_MAP_FUNCTOR_CONST_1(CvCity, int, getCultureTimes100, PlayerTypes);
		DECLARE_MAP_FUNCTOR_CONST_1(CvCity, const CvPlotGroup*, plotGroup, PlayerTypes);

		DECLARE_MAP_FUNCTOR_CONST_2(CvCity, bool, isRevealed, TeamTypes, bool);
	};
};

#endif
