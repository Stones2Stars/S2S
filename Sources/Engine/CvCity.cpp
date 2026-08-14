
#include "Tools/FProfiler.h"
#include "Infos/CvClassificationIds.h"   // the generated SKILL_/TAG_/CAPABILITY_ id table

#include "CvGameCoreDLL.h"
#include "Engine/CvGameSpeedScale.h"
#include "AI/BetterBTSAI.h" // logCityAI ([CIT/produced] / [CIT/waste] production-pipeline logging)
#include "CvArea.h"
#include "UI/CvArtFileMgr.h"
#include "CvBuildingInfo.h"
#include "CvCity.h"
#include "AI/CvContractBroker.h"
#include "Infrastructure/CvDLLEntity.h"
#include "UI/CvEventReporter.h"
#include "AI/CvGameAI.h"
#include "UI/CvGameTextMgr.h"
#include "Defines/CvGlobals.h"
#include "CvImprovementInfo.h"
#include "CvBonusInfo.h"
#include "CvInfos.h"
#include "CvMap.h"
#include "CvPlot.h"
#include "CvPlotGroup.h"                // getNumBonuses -- the network count getNumBonuses relays through
#include "AI/CvPlayerAI.h"
#include "CvPopupInfo.h"
#include "CvProcessInfo.h"              // getProductionToCommerce -- the commerce split's EXTRA-tier conversion rate
#include "Infrastructure/CvPython.h"
#include "CvReachablePlotSet.h"
#include "CvSelectionGroup.h"
#include "AI/CvTeamAI.h"
#include "CvUnit.h"
#include "CvUnitSelectionCriteria.h"
#include "UI/CvViewport.h"
#include "Infrastructure/CvDLLInterfaceIFaceBase.h"
#include "Infrastructure/CvDLLUtilityIFaceBase.h"
#include "CvTraitInfo.h"
#include "Repos/BuildingsRepo.h"
#include "Spine/CvEventSpine.h"
#include "Enabler/CvBuildingEnabler.h"   // onCityCreated -- the per-city domain lifecycle start (enabler.md 7.1)
#include "Enabler/CvUnitEnabler.h"
#include "Enabler/CvEnablerKernel.h"     // EnablerKernel:: -- the operate/gate surface (only fwd-declared above)
#include "Repos/InfoRepo.h"              // InfoRepo<> -- the per-type registry the id reads go through
#include "Conditions/CvConditionEval.h"  // the CvCascadeEvalCtx DEFINITION, not just a forward declaration
#include "CvCascadeChannelRegistry.h"   // channelLookup -- the realized-yield group read's channel identity
#include "CvInfoKinds.h"                // infoYieldFamily / CHANNEL_AMOUNT -- the YieldTypes -> channel family axis
#include "Data/CvInfoValuation.h"       // realizedAtCity -- the ONE cross-scope roll-up the group reads fold through
#ifdef THE_GREAT_WALL
#include "Infrastructure/CvDLLEngineIFaceBase.h"
#endif
#include "UI/CityOutputHistory.h"
#include "Infos/CvClassificationBlock.h"   // CLSD_TAG + the memoized id bit test

// The json.md §8 classification reads this file makes. The consumer holds the memoized generated-id
// (the CvUnitFilters precedent): the info exposes only the parameterized group read, never a named
// getter per key (patterns.md -- a per-key boolean getter is the shape the rebuild deletes).
namespace
{
	// Fold one source's collected (targetFk, value) rows into a running total, scaled by that source's live
	// multiplicity (a specialist's assigned count; 1 for a presence source).
	void mergeKeyedRows(std::vector<std::pair<int, int> >& total,
		const std::vector<std::pair<int, int> >& sourceRows, int iMultiplicity)
	{
		for (size_t iSource = 0; iSource < sourceRows.size(); ++iSource)
		{
			size_t iRow = 0;
			while (iRow < total.size() && total[iRow].first != sourceRows[iSource].first)
			{
				++iRow;
			}
			if (iRow == total.size())
			{
				total.push_back(std::make_pair(sourceRows[iSource].first, 0));
			}
			total[iRow].second += sourceRows[iSource].second * iMultiplicity;
		}
	}
}


//Disable this passed in initialization list warning, as it is only stored in the constructor of CvBuildingList and not used
#pragma warning( disable : 4355 )

// ---- THE CITY'S AVAILABILITY READS (see CvCity.h for the role + the grammar). Every one is a BARE O(1) fetch of
// ---- the maintained tri-state: no gate runs here, no calculator is called, and `requires` is never evaluated --
// ---- the verdict was put there by the events (enabler.md §7). ----

EnablerDomain::State CvCity::getBuildingAvailability(BuildingTypes eBuilding) const
{
	return (EnablerDomain::State)m_enabler.buildings.state((int)eBuilding);
}

EnablerDomain::State CvCity::getUnitAvailability(UnitTypes eUnit) const
{
	return (EnablerDomain::State)m_enabler.units.state((int)eUnit);
}

// WHY it is not offered ([enabler.md] par.6). The gate already knew; storing it is what stops the display -- and
// the AI -- from having to guess which clause refused.
unsigned char CvCity::getBuildingGateReason(BuildingTypes eBuilding) const
{
	return m_enabler.buildings.gateReason((int)eBuilding);
}

unsigned char CvCity::getUnitGateReason(UnitTypes eUnit) const
{
	return m_enabler.units.gateReason((int)eUnit);
}

void CvCity::getAvailableBuildings(std::vector<int>& buildings) const
{
	m_enabler.buildings.listedIds(buildings);
}

// The VISIBLE tri-state -- LISTED plus GREYED, i.e. everything in the tree whether or not its gate holds.
// Distinct from getAvailableBuildings, which is the OFFER: what may be started right now.
void CvCity::getInTreeBuildings(std::vector<int>& buildings) const
{
	m_enabler.buildings.inTreeIds(buildings);
}

void CvCity::getInTreeUnits(std::vector<int>& units) const
{
	m_enabler.units.inTreeIds(units);
}

void CvCity::getAvailableUnits(std::vector<int>& units) const
{
	m_enabler.units.listedIds(units);
}

bool CvCity::isBuildingContinuable(BuildingTypes eBuilding) const
{
	return m_enabler.buildings.listedForContinue((int)eBuilding);
}

// The realized yield group (see CvCity.h for the read role + the grammar it obeys). Each yield channel is a CITY
// RECEIVER (CascadeChannelRegistry's spec'd receiver table: food/production/commerce/culture), so the realized rate
// is the sum slot the gather's §2a combine already wrote AT THE MARK -- one O(1) fetch per channel, with no
// cross-scope roll-up left to do here (state-repositories.md: the receiving scope stores its own realized total).
// The channel axis is resolved by IDENTITY -- the ruling-1 YieldTypes -> family reverse lookup -- never by the
// receiver table's slot order. A channel never minted answers 0, exactly as the package's own read does.
void CvCity::getYields(int (&yields)[NUM_YIELD_TYPES]) const
{
	for (int iYield = 0; iYield < NUM_YIELD_TYPES; ++iYield)
	{
		const int iChannel = CascadeChannelRegistry::channelLookup(infoYieldFamily(iYield), (int)CHANNEL_AMOUNT, -1);
		yields[iYield] = InfoValuation::realizedAtCity(*this, iChannel);
	}
}

// The city's remaining group reads (see CvCity.h for the role + the grammar). Each is the same shape: walk the
// group's own enum, resolve that entry's CHANNEL by identity, and fold it through the ONE cross-scope roll-up --
// which answers a consumed channel from its maintained receiver sum and every other channel from the combine
// over the packages a city sits under. No group read carries a channel argument, a scope argument, or a scalar
// sibling: the enum indexes the RESULT. The COMMERCE group immediately below is the ONE read whose answer is not
// that fold alone -- see its own note.
//
// THE PER-COMMERCE SPLIT'S CITY READ. The four commerce channels are not four independent receivers: the city
// receives the COMMERCE YIELD, and the empire's slider percentages divide that yield across gold / research /
// culture / espionage, each channel adding its own deposits (modifier.md §2a's commerce paragraph). The whole
// arithmetic lives ONCE on the calc surface (InfoValuation::commerceSplit) -- this read only gathers the inputs:
// the two yields through this city's own group read, the sliders through the OWNER'S EMPIRE CONTEXT (the sliders
// are empire state and have no other home), the channel's stack through the ONE city chain walk, its deposits
// through the ONE cross-scope roll-up, and the process conversion off the city's active process.
// The REALIZED read: the live sliders go in. Its what-if sibling below asks the same question against a
// HYPOTHETICAL slider set, and both share the ONE gather so neither can drift from the other
// ([DEC-single-implementation]).
// The LIVE-slider read, with the census optionally kept. Both public reads come through here so the slider
// gather exists once -- a second copy of it is exactly how a census drifts from the number it explains.
void CvCity::commercesAtLiveSliders(int (&commerces)[NUM_COMMERCE_TYPES], CvCommerceSplitTerms* aTermsOut) const
{
	int aiCommerceRates[NUM_COMMERCE_TYPES];
	for (int iCommerce = 0; iCommerce < NUM_COMMERCE_TYPES; ++iCommerce)
	{
		aiCommerceRates[iCommerce] = 0;   // an ownerless city has no empire to slide against: no share, deposits only
	}
	if (getOwner() != NO_PLAYER)
	{
		GET_PLAYER(getOwner()).getEmpireContext().commerceRates(aiCommerceRates);
	}
	expectedCommercesAtSliders(aiCommerceRates, commerces, aTermsOut);
}

void CvCity::getCommerces(int (&commerces)[NUM_COMMERCE_TYPES]) const
{
	commercesAtLiveSliders(commerces, NULL);
}

// ⚖ THE SLIDER WHAT-IF. The empire's slider percentages are what divide the city's COMMERCE yield across the four
// channels (modifier.md §2a), so "what would gold be at 0% / 100%" is answered by feeding the ONE combine a
// hypothetical slider -- never by exposing the combine's internals through a base-rate accessor.
void CvCity::expectedCommercesAtSliders(const int (&sliderPercents)[NUM_COMMERCE_TYPES],
										int (&commerces)[NUM_COMMERCE_TYPES],
										CvCommerceSplitTerms* aTermsOut) const
{
	int aiRealizedYields[NUM_YIELD_TYPES];
	getYields(aiRealizedYields);
	const int (&aiCommerceRates)[NUM_COMMERCE_TYPES] = sliderPercents;
	// The EXTRA tier's rate: the city's active process, which is the ONE source of a hammers->commerce conversion
	// (json §9 `conversion`; a city building anything else converts nothing).
	const ProcessTypes eProcess = getProductionProcess();
	for (int iCommerce = 0; iCommerce < NUM_COMMERCE_TYPES; ++iCommerce)
	{
		const CommerceTypes eCommerce = (CommerceTypes)iCommerce;
		const int iChannel = CascadeChannelRegistry::channelLookup(infoCommerceFamily(iCommerce), (int)CHANNEL_AMOUNT, -1);
		int64_t iChannelFlatSum = 0;      // not this read's answer -- the deposits term comes through the roll-up below,
		int64_t iChannelPercentSum = 0;   // which prefers a consumed channel's maintained receiver sum (culture)
		InfoValuation::rolledLegsAtCity(*this, iChannel, iChannelFlatSum, iChannelPercentSum);
		int iProductionToCommerce = 0;
		if (eProcess != NO_PROCESS)
		{
			iProductionToCommerce = GC.getProcessInfo(eProcess).getProductionToCommerce(eCommerce, CASC_SCOPE_CITY);
		}
		commerces[iCommerce] = (int)InfoValuation::commerceSplit(
			aiRealizedYields[YIELD_COMMERCE],
			aiCommerceRates[iCommerce],
			iChannelPercentSum,
			InfoValuation::realizedAtCity(*this, iChannel),
			aiRealizedYields[YIELD_PRODUCTION],
			iProductionToCommerce,
			(aTermsOut != NULL) ? &aTermsOut[iCommerce] : NULL);
	}
}

void CvCity::getCommerceTerms(CommerceTypes eCommerce, CvCommerceSplitTerms& kTerms) const
{
	if ((int)eCommerce < 0 || (int)eCommerce >= NUM_COMMERCE_TYPES)
	{
		return;
	}
	int aiCommerces[NUM_COMMERCE_TYPES];
	CvCommerceSplitTerms aTerms[NUM_COMMERCE_TYPES];
	commercesAtLiveSliders(aiCommerces, aTerms);
	kTerms = aTerms[eCommerce];
}

void CvCity::getWellbeing(int (&wellbeing)[NUM_WELLBEING_CHANNELS]) const
{
	for (int iChannelIndex = 0; iChannelIndex < NUM_WELLBEING_CHANNELS; ++iChannelIndex)
	{
		const WellbeingChannel eWellbeing = (WellbeingChannel)iChannelIndex;
		// The opposing channels share ONE authored family and are minted as SIGN TWINS beside it (modifier.md
		// §2b: a negative deposit routes to the opposing channel at fill), so anger/unhealth are looked up as
		// the twin of their authored channel rather than as families of their own.
		const int iAuthoredChannel = CascadeChannelRegistry::channelLookup(infoWellbeingFamily(eWellbeing), (int)CHANNEL_AMOUNT, -1);
		int iChannel = iAuthoredChannel;
		if (eWellbeing == WELLBEING_ANGER || eWellbeing == WELLBEING_UNHEALTH)
		{
			iChannel = CascadeChannelRegistry::wellbeingTwin(iAuthoredChannel);
		}
		wellbeing[iChannelIndex] = InfoValuation::realizedAtCity(*this, iChannel);
	}
}

void CvCity::getDefenseKinds(int (&defenses)[NUM_DEFENSE_KINDS]) const
{
	for (int iKind = 0; iKind < NUM_DEFENSE_KINDS; ++iKind)
	{
		const int iChannel = CascadeChannelRegistry::channelLookup(MODFAM_DEFENSE, iKind, -1);
		defenses[iKind] = InfoValuation::realizedAtCity(*this, iChannel);
	}
}

void CvCity::getMaintenanceKinds(int (&maintenances)[NUM_MAINTENANCE_KINDS]) const
{
	for (int iKind = 0; iKind < NUM_MAINTENANCE_KINDS; ++iKind)
	{
		const int iChannel = CascadeChannelRegistry::channelLookup(MODFAM_MAINTENANCE, iKind, -1);
		maintenances[iKind] = InfoValuation::realizedAtCity(*this, iChannel);
	}
}

void CvCity::getBuildRateKinds(int (&buildRates)[NUM_BUILD_RATE_KINDS]) const
{
	for (int iKind = 0; iKind < NUM_BUILD_RATE_KINDS; ++iKind)
	{
		const int iChannel = CascadeChannelRegistry::channelLookup(MODFAM_BUILD_RATE, iKind, -1);
		buildRates[iKind] = InfoValuation::realizedAtCity(*this, iChannel);
	}
}

//	The city's build-rate stack for ONE unit TAG -- what "military units build faster here" is worth, now that
//	military and space are PREDICATES on the `units` target rather than categories with a channel of their own
//	([modifier.md] §4). A filtered entry is CONDITIONED, so it never folds into the package and cannot be a
//	channel read: it is the ordinary entry-list read over the live sources ([modifier.md] §5) -- the OPERATING
//	buildings and ACTIVE corporations here, plus the player's empire tier (held traits + adopted civics).
//	⚠ A capped wonder's EMPIRE-scope row is still NOT included: no player-side read walks the owner's buildings
//	for one (named in the todo).
int CvCity::taggedBuildRate(int iTagId) const
{
	PROFILE_EXTRA_FUNC();
	if (iTagId < 0)
	{
		return 0;
	}
	const int iUnitsSegment = InfoValuation::keyedTargetSegment("units");
	int iRate = GET_PLAYER(getOwner()).taggedBuildRate(iTagId);
	const std::set<int>& kActive = m_operatingBuildings.active;
	for (std::set<int>::const_iterator it = kActive.begin(); it != kActive.end(); ++it)
	{
		iRate += (int)InfoValuation::taggedTargetSum(GC.getBuildingInfo((BuildingTypes)*it).getModifiers(),
			MODFAM_BUILD_RATE, -1, CASC_SCOPE_CITY, CASC_UNIT_PERCENT, iUnitsSegment, iTagId);
	}
	for (int iI = 0; iI < GC.getNumCorporationInfos(); iI++)
	{
		if (isActiveCorporation((CorporationTypes)iI))
		{
			iRate += (int)InfoValuation::taggedTargetSum(GC.getCorporationInfo((CorporationTypes)iI).getModifiers(),
				MODFAM_BUILD_RATE, -1, CASC_SCOPE_CITY, CASC_UNIT_PERCENT, iUnitsSegment, iTagId);
		}
	}
	return iRate;
}

void CvCity::getCombatKinds(int (&combats)[NUM_COMBAT_KINDS]) const
{
	for (int iKind = 0; iKind < NUM_COMBAT_KINDS; ++iKind)
	{
		const int iChannel = CascadeChannelRegistry::channelLookup(MODFAM_COMBAT, iKind, -1);
		combats[iKind] = InfoValuation::realizedAtCity(*this, iChannel);
	}
}

void CvCity::getExperienceKinds(int (&experiences)[NUM_EXPERIENCE_KINDS]) const
{
	for (int iKind = 0; iKind < NUM_EXPERIENCE_KINDS; ++iKind)
	{
		const int iChannel = CascadeChannelRegistry::channelLookup(MODFAM_EXPERIENCE, iKind, -1);
		experiences[iKind] = InfoValuation::realizedAtCity(*this, iChannel);
	}
}

void CvCity::getRevolutionKinds(int (&revolutions)[NUM_REVOLUTION_KINDS]) const
{
	for (int iKind = 0; iKind < NUM_REVOLUTION_KINDS; ++iKind)
	{
		const int iChannel = CascadeChannelRegistry::channelLookup(MODFAM_REVOLUTION, iKind, -1);
		revolutions[iKind] = InfoValuation::realizedAtCity(*this, iChannel);
	}
}

void CvCity::getTradeRouteKinds(int (&tradeRoutes)[NUM_TRADE_ROUTE_KINDS]) const
{
	for (int iKind = 0; iKind < NUM_TRADE_ROUTE_KINDS; ++iKind)
	{
		const int iChannel = CascadeChannelRegistry::channelLookup(MODFAM_TRADE_ROUTES, iKind, -1);
		tradeRoutes[iKind] = InfoValuation::realizedAtCity(*this, iChannel);
	}
}

void CvCity::getHealKinds(int (&heals)[NUM_HEAL_KINDS]) const
{
	for (int iKind = 0; iKind < NUM_HEAL_KINDS; ++iKind)
	{
		const int iChannel = CascadeChannelRegistry::channelLookup(MODFAM_HEAL, iKind, -1);
		heals[iKind] = InfoValuation::realizedAtCity(*this, iChannel);
	}
}

void CvCity::getUnderworldKinds(int (&underworlds)[NUM_UNDERWORLD_KINDS]) const
{
	for (int iKind = 0; iKind < NUM_UNDERWORLD_KINDS; ++iKind)
	{
		const int iChannel = CascadeChannelRegistry::channelLookup(MODFAM_UNDERWORLD, iKind, -1);
		underworlds[iKind] = InfoValuation::realizedAtCity(*this, iChannel);
	}
}

void CvCity::getScalars(int (&scalars)[NUM_INFO_SCALARS]) const
{
	for (int iScalar = 0; iScalar < NUM_INFO_SCALARS; ++iScalar)
	{
		ModifierFamily eFamily = MODFAM_NONE;
		int iKind = -1;
		infoScalarSlot((InfoScalar)iScalar, eFamily, iKind);
		const int iChannel = CascadeChannelRegistry::channelLookup(eFamily, iKind, -1);
		scalars[iScalar] = InfoValuation::realizedAtCity(*this, iChannel);
	}
}


void CvCity::getCountdowns(int (&countdowns)[NUM_CITY_COUNTDOWN_KINDS]) const
{
	countdowns[COUNTDOWN_HURRY_ANGER]                   = getHurryAngerTimer();
	countdowns[COUNTDOWN_HURRY_ANGER_PERIOD]            = flatHurryAngerLength();
	countdowns[COUNTDOWN_CONSCRIPT_ANGER]               = getConscriptAngerTimer();
	countdowns[COUNTDOWN_CONSCRIPT_ANGER_PERIOD]        = flatConscriptAngerLength();
	countdowns[COUNTDOWN_DEFY_RESOLUTION_ANGER]         = getDefyResolutionAngerTimer();
	countdowns[COUNTDOWN_DEFY_RESOLUTION_ANGER_PERIOD]  = flatDefyResolutionAngerLength();
	countdowns[COUNTDOWN_HAPPINESS]                     = getHappinessTimer();
	countdowns[COUNTDOWN_OCCUPATION]                    = getOccupationTimer();
	countdowns[COUNTDOWN_ESPIONAGE_HAPPINESS]           = getEspionageHappinessCounter();
	countdowns[COUNTDOWN_ESPIONAGE_HEALTH]              = getEspionageHealthCounter();
}


void CvCity::getOrderRead(int (&order)[NUM_CITY_ORDER_READS]) const
{
	order[ORDER_READ_TYPE] = NO_ORDER;
	order[ORDER_READ_ID]   = -1;
	if (isProductionUnit())
	{
		order[ORDER_READ_TYPE] = ORDER_TRAIN;
		order[ORDER_READ_ID]   = (int)getProductionUnit();
	}
	else if (isProductionBuilding())
	{
		order[ORDER_READ_TYPE] = ORDER_CONSTRUCT;
		order[ORDER_READ_ID]   = (int)getProductionBuilding();
	}
	else if (isProductionProject())
	{
		order[ORDER_READ_TYPE] = ORDER_CREATE;
		order[ORDER_READ_ID]   = (int)getProductionProject();
	}
	else if (isProductionProcess())
	{
		order[ORDER_READ_TYPE] = ORDER_MAINTAIN;
		order[ORDER_READ_ID]   = (int)getProductionProcess();
	}
	order[ORDER_READ_PRODUCTION_LEFT]     = productionLeft();
	order[ORDER_READ_PRODUCTION_PROGRESS] = getProductionProgress();
	order[ORDER_READ_PRODUCTION_NEEDED]   = getProductionNeeded();
	order[ORDER_READ_GENERAL_TURNS_LEFT]  = getGeneralProductionTurnsLeft();
	order[ORDER_READ_TURNS_LEFT]          = getProductionTurnsLeft();
	//	The two rates the city screen actually needs: everything, and everything-except-food-conversion. Their
	//	DIFFERENCE is the food-derived part, so a third slot would be derivable and therefore redundant.
	order[ORDER_READ_PRODUCTION_PER_TURN]         = getCurrentProductionDifference(ProductionCalc::FoodProduction | ProductionCalc::Overflow);
	order[ORDER_READ_PRODUCTION_PER_TURN_NO_FOOD] = getCurrentProductionDifference(ProductionCalc::Overflow);
	order[ORDER_READ_MAX_OVERFLOW]        = getMaxProductionOverflow();
}


void CvCity::getGrowthRead(int (&growth)[NUM_CITY_GROWTH_READS]) const
{
	growth[GROWTH_READ_FOOD_STORED]        = getFood();
	// A READ EDGE (this group is published to Python and read nowhere else), so the two x100 RATE slots reduce
	// here -- which also keeps the group uniform, since every other member is already a whole quantity: the
	// threshold and turns-left are the whole-unit food BAR's ([DEC-fixedpoint-x100]).
	growth[GROWTH_READ_FOOD_PER_TURN]      = foodDifference() / 100;
	growth[GROWTH_READ_FOOD_CONSUMPTION]   = foodConsumption() / 100;
	growth[GROWTH_READ_THRESHOLD]          = growthThreshold();
	growth[GROWTH_READ_TURNS_LEFT]         = getFoodTurnsLeft();
	growth[GROWTH_READ_IS_FOOD_PRODUCTION] = isFoodProduction() ? 1 : 0;
	//	The food CARRIED OVER on growth -- a whole-unit quantity like every other member of this group.
	growth[GROWTH_READ_FOOD_KEPT]          = getFoodKept();
}


void CvCity::getCultureRead(int (&culture)[NUM_CITY_CULTURE_READS]) const
{
	//	The stored culture is 64-bit at source and narrows HERE, at the read: a city's own culture total is
	//	compared against a 32-bit threshold, so a wider answer buys the reader nothing.
	const PlayerTypes eOwner = getOwner();
	culture[CULTURE_READ_OWNER_AMOUNT] = (eOwner == NO_PLAYER) ? 0 : (int)getCulture(eOwner);
	culture[CULTURE_READ_THRESHOLD]    = getCultureThreshold();
	culture[CULTURE_READ_LEVEL]        = (int)getCultureLevel();
}


void CvCity::getCityFlags(int (&flags)[NUM_CITY_FLAGS]) const
{
	flags[CITY_FLAG_PRODUCING]             = isProduction() ? 1 : 0;
	flags[CITY_FLAG_CITIZENS_AUTOMATED]    = isCitizensAutomated() ? 1 : 0;
	flags[CITY_FLAG_PRODUCTION_AUTOMATED]  = isProductionAutomated() ? 1 : 0;
	flags[CITY_FLAG_CAN_CONSCRIPT]         = canConscript() ? 1 : 0;
	flags[CITY_FLAG_DISORDER]              = isDisorder() ? 1 : 0;
	flags[CITY_FLAG_CAPITAL]               = isCapital() ? 1 : 0;
	flags[CITY_FLAG_GOVERNMENT_CENTER]     = isGovernmentCenter() ? 1 : 0;
	flags[CITY_FLAG_POWER]                 = isPowered() ? 1 : 0;
	flags[CITY_FLAG_OCCUPATION]            = isOccupation() ? 1 : 0;
	flags[CITY_FLAG_PLUNDERED]             = isPlundered() ? 1 : 0;
	flags[CITY_FLAG_QUARANTINED]           = isQuarantined() ? 1 : 0;
	flags[CITY_FLAG_CONNECTED_TO_CAPITAL]  = isConnectedToCapital() ? 1 : 0;
	flags[CITY_FLAG_COASTAL]               = isCoastal(0) ? 1 : 0;
}

void CvCity::getBuildingInCity(BuildingTypes eBuilding, int (&read)[NUM_CITY_BUILDING_READS]) const
{
	read[CITY_BUILDING_HAS]              = hasBuilding(eBuilding) ? 1 : 0;
	read[CITY_BUILDING_ACTIVE]           = isActiveBuilding(eBuilding) ? 1 : 0;
	read[CITY_BUILDING_HAPPINESS]        = getBuildingHappiness(eBuilding);
	read[CITY_BUILDING_HEALTH]           = getBuildingHealth(eBuilding);
	read[CITY_BUILDING_PROGRESS]         = getProgressOnBuilding(eBuilding);
	read[CITY_BUILDING_DELAY]            = getDelayOnBuilding(eBuilding);
	read[CITY_BUILDING_PRODUCTION_DECAY] = isBuildingProductionDecay(eBuilding) ? 1 : 0;
	//	WHO BUILT IT -- the ledger's own record, which is the only home this fact has ever had; there is no
	//	`getBuildingOriginalOwner` on the city and never was, so a caller that wants it asks the ledger.
	read[CITY_BUILDING_ORIGINAL_OWNER]   = (int)getBuildingData(eBuilding).eBuiltBy;
}


void CvCity::getUnitInCity(UnitTypes eUnit, int (&read)[NUM_CITY_UNIT_READS]) const
{
	read[CITY_UNIT_PROGRESS]         = getProgressOnUnit(eUnit);
	read[CITY_UNIT_DELAY]            = getDelayOnUnit(eUnit);
	read[CITY_UNIT_PRODUCTION_DECAY] = isUnitProductionDecay(eUnit) ? 1 : 0;
}

void CvCity::getSpecialistInCity(SpecialistTypes eSpecialist, int (&read)[NUM_CITY_SPECIALIST_READS]) const
{
	read[CITY_SPECIALIST_COUNT]      = getSpecialistCount(eSpecialist);
	read[CITY_SPECIALIST_FORCED]     = getForceSpecialistCount(eSpecialist);
	read[CITY_SPECIALIST_FREE]       = getFreeSpecialistCount(eSpecialist);
	read[CITY_SPECIALIST_VALID]      = isSpecialistValid(eSpecialist) ? 1 : 0;
	read[CITY_SPECIALIST_EMPHASIZED] = AI_isEmphasizeSpecialist(eSpecialist) ? 1 : 0;
}

void CvCity::getCityCounts(int (&counts)[NUM_CITY_COUNT_READS]) const
{
	counts[CITY_COUNT_NATIONAL_WONDERS]     = getNumNationalWonders();
	counts[CITY_COUNT_MAX_NATIONAL_WONDERS] = getMaxNumNationalWonders();
	counts[CITY_COUNT_WORLD_WONDERS]        = getNumWorldWonders();
	counts[CITY_COUNT_MAX_WORLD_WONDERS]    = getMaxNumWorldWonders();
	counts[CITY_COUNT_DEFENSE_MODIFIER]     = getDefenseModifier(false);
	counts[CITY_COUNT_DEFENSE_DAMAGE]       = getDefenseDamage();
	counts[CITY_COUNT_REVOLUTION_INDEX]     = getRevolutionIndex();
	counts[CITY_COUNT_REVOLUTION_AVERAGE]   = getRevIndexAverage();
	counts[CITY_COUNT_GAME_TURN_FOUNDED]    = getGameTurnFounded();
	counts[CITY_COUNT_GAME_TURN_ACQUIRED]   = getGameTurnAcquired();
}

void CvCity::getHurryQuote(HurryTypes eHurry, int (&quote)[NUM_CITY_HURRY_QUOTES]) const
{
	const bool bAllowed = canHurry(eHurry, false);
	quote[HURRY_QUOTE_ALLOWED]            = bAllowed ? 1 : 0;
	quote[HURRY_QUOTE_POPULATION_COST]    = hurryPopulation(eHurry);
	quote[HURRY_QUOTE_PRODUCTION_GAINED]  = hurryProduction(eHurry);
	//	The gold cost is 64-bit at source; it is narrowed HERE, at the read, because a hurry price that
	//	overflows 32 bits is not a price any interface can render either.
	quote[HURRY_QUOTE_GOLD_COST]          = (int)getHurryGold(eHurry);
}


CvCity::CvCity()
	: m_GameObject(this),
	m_BuildingList(NULL, this),
	m_UnitList(NULL, this),
	m_Properties(this),
	m_outputHistory()
{
	m_aiYieldRateModifier = new int[NUM_YIELD_TYPES];
	m_aiTradeYield = new int[NUM_YIELD_TYPES];
	m_aiProductionToCommerceModifier = new int[NUM_COMMERCE_TYPES];

	m_aiCulture = new int64_t[MAX_PLAYERS];
	m_aiNumRevolts = new int[MAX_PLAYERS];
	m_abEverOwned = new bool[MAX_PLAYERS];
	m_abTradeRoute = new bool[MAX_PLAYERS];
	m_abRevealed = new bool[MAX_TEAMS];
	m_abEspionageVisibility = new bool[MAX_TEAMS];

	const int iNumBuildings = GC.getNumBuildingInfos();
	m_bHasBuildings = new bool[iNumBuildings];

	m_paiProjectProduction = NULL;
	m_paiUnitProduction = NULL;
	m_paiGreatPeopleUnitRate = NULL;
	m_paiGreatPeopleUnitProgress = NULL;
	m_paiSpecialistCount = NULL;
	m_paiForceSpecialistCount = NULL;
	m_paiFreeSpecialistCountUnattributed = NULL;
	m_paiReligionInfluence = NULL;
	m_bPropertyControlBuildingQueued = false;

	m_pabWorkingPlot = NULL;
	m_pabHasReligion = NULL;
	m_pabHasCorporation = NULL;


	m_cachedPropertyNeeds = NULL;
	m_paiFreeBonus = NULL;
	m_paiFreeBonusEvents = NULL;
	m_paiUnitCombatExtraStrength = NULL;
	m_pabAutomatedCanBuild = NULL;
	m_paiAidRate = NULL;
	m_ppaaiExtraBonusAidModifier = NULL;
	m_paiUnitCombatDefenseAgainstModifier = NULL;
	m_ppaaiLocalSpecialistExtraYield = NULL;

	m_paiSpecialistBannedCount = NULL;

	m_bVisibilitySetup = false;

	//CvDLLEntity::createCityEntity(this);		// create and attach entity to city

	m_aiBaseYieldRank = new int[NUM_YIELD_TYPES];
	m_abBaseYieldRankValid = new bool[NUM_YIELD_TYPES];
	m_aiYieldRank = new int[NUM_YIELD_TYPES];
	m_abYieldRankValid = new bool[NUM_YIELD_TYPES];
	m_aiCommerceRank = new int[NUM_COMMERCE_TYPES];
	m_abCommerceRankValid = new bool[NUM_COMMERCE_TYPES];

	m_deferringBonusProcessingCount = 0;
	m_paiStartDeferredSectionNumBonuses = NULL;
	m_bMarkedForDestruction = false;

	reset(0, NO_PLAYER, 0, 0, true);
}

CvCity::~CvCity()
{
	if (getEntity() != NULL)
	{
		CvDLLEntity::removeEntity();			// remove entity from engine
		CvDLLEntity::destroyEntity();			// delete CvCityEntity and detach from us
	}

	uninit();

	SAFE_DELETE_ARRAY(m_aiBaseYieldRank);
	SAFE_DELETE_ARRAY(m_abBaseYieldRankValid);
	SAFE_DELETE_ARRAY(m_aiYieldRank);
	SAFE_DELETE_ARRAY(m_abYieldRankValid);
	SAFE_DELETE_ARRAY(m_aiCommerceRank);
	SAFE_DELETE_ARRAY(m_abCommerceRankValid);

	SAFE_DELETE_ARRAY(m_aiYieldRateModifier);
	SAFE_DELETE_ARRAY(m_aiTradeYield);

	SAFE_DELETE_ARRAY(m_aiProductionToCommerceModifier);
	SAFE_DELETE_ARRAY(m_aiCulture);
	SAFE_DELETE_ARRAY(m_aiNumRevolts);
	SAFE_DELETE_ARRAY(m_abEverOwned);
	SAFE_DELETE_ARRAY(m_abTradeRoute);
	SAFE_DELETE_ARRAY(m_abRevealed);
	SAFE_DELETE_ARRAY(m_abEspionageVisibility);

	SAFE_DELETE_ARRAY(m_bHasBuildings);
}


void CvCity::init(int iID, PlayerTypes eOwner, int iX, int iY, bool bBumpUnits, bool bUpdatePlotGroups)
{
	PROFILE_FUNC();

	//--------------------------------
	// Log this event
	if (GC.getLogging())
	{
		char szOut[1024];
		sprintf(szOut, "Player %d City %d built at %d:%d\n", eOwner, iID, iX, iY);
		gDLL->messageControlLog(szOut);
	}

	CvPlot* pPlot = GC.getMap().plot(iX, iY);
	//--------------------------------
	// Init saved data
	reset(iID, eOwner, pPlot->getX(), pPlot->getY());

	// THE ENABLER'S PER-CITY LIFECYCLE START (enabler.md §7.1): size the domains, apply the static exclusions, and
	// fold the CROSS-SCOPE HAVEs no event can carry to a city that did not exist when they were acquired (the
	// team's techs, the player's civics). It must run BEFORE any of this city's own facts emit, because the
	// appliers SKIP an un-init'd domain -- a city whose domains were never init'd stays permanently empty.
	BuildingEnabler::onCityCreated(*this);
	UnitEnabler::onCityCreated(*this);

	CvPlayer& player = GET_PLAYER(eOwner);
	if (player.isHumanPlayer(true))
	{
		player.setIdleCity(getID(), true);
	}
	//--------------------------------
	// Init non-saved data
	setupGraphical();

	//--------------------------------
	// Init other game data
	bool bFound = false;
	if (GC.getGame().isOption(GAMEOPTION_MAP_PERSONALIZED) && player.isModderOption(MODDEROPTION_USE_LANDMARK_NAMES))
	{
		for (int iI = 0; iI < NUM_CITY_PLOTS_2; iI++)
		{
			CvPlot* pLoopPlot = getCityIndexPlot(iI);
			FAssertMsg(pLoopPlot != NULL, CvString::format("pLoopPlot was null for iIndex %d", iI).c_str());
			if (!pLoopPlot->getLandmarkName().empty() && pLoopPlot->getLandmarkType() != NO_LANDMARK)
			{
				setName(pLoopPlot->getLandmarkName());
				if (!getName().empty())
				{
					bFound = true;
					break;
				}
			}
		}
	}
	if (!bFound) setName(player.getNewCityName());

	setEverOwned(getOwner(), true);

	pPlot->setImprovementType(NO_IMPROVEMENT);
	pPlot->setOwner(getOwner(), bBumpUnits, false);
	pPlot->setPlotCity(this);

	updateCultureLevel(false);

	// The city now HAS a work area: hand every plot in it this city's membership. ⚠ The culture-level transition
	// above may already have covered part of the range; setWorkableBy announces only genuine crossings, so the
	// overlap is a no-op rather than a double.
	changeWorkableArea(0, getNumCityPlots());

	pPlot->changeCulture(getOwner(), GC.getFREE_CITY_CULTURE(), bBumpUnits);

	// Immediately put some tiles on adjacent tiles if not 1TF option. Which tiles depends on game options.
	if (!GC.getGame().isOption(GAMEOPTION_CULTURE_1_CITY_TILE_FOUNDING))
	{
		const int iAdjCulture = GC.getFREE_CITY_ADJACENT_CULTURE();
		// Special case: RCS with no MCB gains only tiles a lvl 1 city would gain as defined by RCS
		if (GC.getGame().isOption(GAMEOPTION_CULTURE_REALISTIC_SPREAD) && !GC.getGame().isOption(GAMEOPTION_CULTURE_MIN_CITY_BORDER))
		{
			foreach_(CvPlot* pAdjacentPlot, plot()->cardinalDirectionAdjacent())
			{
				if (cultureDistance(*pAdjacentPlot) == 1)
				{
					pAdjacentPlot->changeCulture(getOwner(), iAdjCulture, bBumpUnits);
					pAdjacentPlot->updateCulture(bBumpUnits, false);
				}
			}
		}
		// All 8 tiles gained. MCB enables this even if RCS also on; 'soft' RCS
		else
		{
			foreach_(CvPlot* pAdjacentPlot, plot()->adjacent())
			{
				pAdjacentPlot->changeCulture(getOwner(), iAdjCulture, bBumpUnits);
				pAdjacentPlot->updateCulture(bBumpUnits, false);
			}
		}
	}

	for (int iI = 0; iI < MAX_PC_TEAMS; iI++)
	{
		if (GET_TEAM(getTeam()).isVassal((TeamTypes)iI))
		{
			pPlot->changeAdjacentSight((TeamTypes)iI, 0, true, NULL, false);
		}
	}

	pPlot->updateCityRoute(false);

	for (int iI = 0; iI < MAX_TEAMS; iI++)
	{
		if (GET_TEAM((TeamTypes)iI).isAlive() && pPlot->isVisible((TeamTypes)iI, false))
		{
			setRevealed((TeamTypes)iI, true);
		}
	}

	{
		// don't use pPlot->plotCount(PUF_isMilitaryHappiness), it doesn't count dead units
		//	and will thus for the AI not recognize units that have just merged on the plot the same turn before it founded the city.
		// hmm, maybe plotCount should always count dead units, need to investigate, could add a new paramater to make it count dead units too.
		int iCount = 0;
		foreach_(const CvUnit* unitX, pPlot->units())
		{
			if (unitX->isMilitaryHappiness())
			{
				iCount++;
			}
		}
		changeMilitaryHappinessUnits(iCount);
	}

	for (int iI = 0; iI < NUM_COMMERCE_TYPES; iI++)
	{
	}

	CvArea* pArea = area();
	pArea->changeCitiesPerPlayer(getOwner(), 1);

	GET_TEAM(getTeam()).changeNumCities(1);

	GC.getGame().changeNumCities(1);

	bool bHistoricalCalendar = GC.getGame().isModderGameOption(MODDERGAMEOPTION_USE_HISTORICAL_ACCURATE_CALENDAR);

	setGameTurnFounded(GC.getGame().getGameTurn(), bHistoricalCalendar);
	setGameTurnAcquired(GC.getGame().getGameTurn(), bHistoricalCalendar);

	setPopulation(GC.getINITIAL_CITY_POPULATION() + GC.getEraInfo(GC.getGame().getStartEra()).getFreePopulation(), false);

	changeAirUnitCapacity(GC.getCITY_AIR_UNIT_CAPACITY());

	updateFreshWaterHealth();


	GC.getMap().updateWorkingCity();

	player.AI_makeAssignWorkDirty();

	player.setFoundedFirstCity(true);

	if (isNPC() || player.getNumCities() == 1)
	{
		for (int iI = 0; iI < (int)GC.getCivilizationInfo(getCivilizationType()).getFreeBuildings().size(); iI++)
		{
			changeHasBuilding(GC.getCivilizationInfo(getCivilizationType()).getFreeBuildings()[iI], true);
		}
	}

	updateEspionageVisibility(false);

	if (bUpdatePlotGroups)
	{
		GC.getGame().updatePlotGroups();
	}
	setCivilizationType(player.getCivilizationType());

	m_UnitList.init();

	AI_init();
}


void CvCity::uninit()
{
	SAFE_DELETE_ARRAY(m_paiProjectProduction);
	SAFE_DELETE_ARRAY(m_paiUnitProduction);
	SAFE_DELETE_ARRAY(m_paiGreatPeopleUnitRate);
	SAFE_DELETE_ARRAY(m_paiGreatPeopleUnitProgress);
	SAFE_DELETE_ARRAY(m_paiSpecialistCount);
	SAFE_DELETE_ARRAY(m_paiForceSpecialistCount);
	SAFE_DELETE_ARRAY(m_paiFreeSpecialistCountUnattributed);
	SAFE_DELETE_ARRAY(m_paiReligionInfluence);
	SAFE_DELETE_ARRAY(m_cachedPropertyNeeds);
	SAFE_DELETE_ARRAY(m_paiFreeBonus);
	SAFE_DELETE_ARRAY(m_paiFreeBonusEvents);
	SAFE_DELETE_ARRAY(m_paiUnitCombatExtraStrength);
	SAFE_DELETE_ARRAY(m_pabAutomatedCanBuild);
	SAFE_DELETE_ARRAY(m_pabWorkingPlot);
	SAFE_DELETE_ARRAY(m_pabHasReligion);
	SAFE_DELETE_ARRAY(m_pabHasCorporation);

	SAFE_DELETE_ARRAY(m_paiAidRate);
	SAFE_DELETE_ARRAY2(m_ppaaiExtraBonusAidModifier, GC.getNumBonusInfos());
	SAFE_DELETE_ARRAY(m_paiUnitCombatDefenseAgainstModifier);
	SAFE_DELETE_ARRAY(m_paiStartDeferredSectionNumBonuses);
	SAFE_DELETE_ARRAY(m_paiSpecialistBannedCount);
	SAFE_DELETE_ARRAY2(m_ppaaiLocalSpecialistExtraYield, GC.getNumSpecialistInfos());
}

// FUNCTION: reset()
// Initializes data members that are serialized.
void CvCity::reset(int iID, PlayerTypes eOwner, int iX, int iY, bool bConstructorCall)
{
	PROFILE_EXTRA_FUNC();
	m_cityContext.bind(this);   // bind the per-city context to its owner (the pointer IS this city; forwarding reads it)
	// ⛔ ZERO ITS DELTA STORES. A CvCity is recycled out of an FFreeListTrashArray, and a keyed accumulator is
	// correct ONLY from a known zero -- a reused slot would inherit the previous occupant's plotAttrs / vicinity
	// counts, which no later ±1 can ever correct ([DEC-keyed-accumulator]).
	m_cityContext.clear();
	// bind the CITY-scope yield planes, one per ORIGIN ([state-repositories.md] § THE ORIGIN RULE). Both start
	// EMPTY and are filled ONLY by the facts ([DEC-maintained-sum]).
	m_buildingYields.bind(CASC_SCOPE_CITY, (int)eOwner, iID);
	m_plotYields.bind(CASC_SCOPE_CITY, (int)eOwner, iID);
	m_cityPercents.bind(CASC_SCOPE_CITY, (int)eOwner, iID);
	m_specialistYields.bind(CASC_SCOPE_CITY, (int)eOwner, iID);
	// The enabler's per-city state starts EMPTY and UN-READY: the domains are init'd by their domain enabler at
	// this city's lifecycle start and filled by DOMAIN events thereafter ([DEC-spine-reseed]) -- never from the
	// save. Clearing here is load-bearing because a CvCity is RECYCLED out of an FFreeListTrashArray: without it
	// a new city would inherit the previous occupant's frontier and operating set.
	m_enabler.reset();
	m_operatingBuildings = OperatingBuildings();
	// The AMENITY context, for exactly the same reason and in the same breath: it is a DELTA store, so it is
	// correct only from a KNOWN ZERO, and a recycled slot would inherit counts no later delta could ever correct
	// ([DEC-keyed-accumulator]). Bound here too -- the pointer IS this city.
	m_amenity.bind(this);
	m_amenity.clear();

	//--------------------------------
	// Uninit class
	uninit();

	if (!bConstructorCall)
	{
		for (int iI = 0; iI < NUM_COMMERCE_TYPES; iI++)
		{
		}
	}
	m_iID = iID;
	m_iX = iX;
	m_iY = iY;
	m_iRallyX = INVALID_PLOT_COORD;
	m_iRallyY = INVALID_PLOT_COORD;
	m_iGameTurnFounded = 0;
	m_iGameTurnAcquired = 0;
	m_iPopulation = 0;
	m_iHighestPopulation = 0;
	m_iWorkingPopulation = 0;
	m_iSpecialistPopulation = 0;
	m_iNumGreatPeople = 0;
	m_iGreatPeopleProgress = 0;
	m_iNumWorldWonders = 0;
	m_iNumTeamWonders = 0;
	m_iNumNationalWonders = 0;
	m_iNumBuildings = 0;
	m_iEspionageHealthCounter = 0;
	m_iEspionageHappinessCounter = 0;
	m_iFreshWaterGoodHealth = 0;
	m_iHurryAngerTimer = 0;
	m_iRevRequestAngerTimer = 0;
	m_iRevSuccessTimer = 0;
	m_iConscriptAngerTimer = 0;
	m_iDefyResolutionAngerTimer = 0;
	m_iHappinessTimer = 0;
	m_iMilitaryHappinessUnits = 0;
	m_iExtraHappiness = 0;
	m_iExtraHealth = 0;
	m_iFood = 0;
	m_iFoodKept = 0;
	m_iOverflowProduction = 0;
	m_iFeatureProduction = 0;
	m_iCurrAirlift = 0;
	m_iMaxAirlift = 0;
	m_iAirUnitCapacity = 0;
	m_iDefenseDamage = 0;
	m_iLastDefenseDamage = 0;
	m_iOccupationTimer = 0;
	m_iCultureUpdateTimer = 0;
	m_iCitySizeBoost = 0;
	m_icachedPropertyNeedsTurn = 0;
	m_iCiv = NO_CIVILIZATION;
	m_iLandmarkAngerTimer = 0;
	m_iLostProduction = 0;
	m_iWorkableRadiusOverride = 0;
	m_iProtectedCultureCount = 0;
	m_iWarWearinessTimer = 0;
	m_iEventAnger = 0;
	m_iMinimumDefenseLevel = 0;
	m_iHealthPercentPerPopulation = 0;
	m_iQuarantinedCount = 0;
	m_bNeverLost = true;
	m_bPropertyControlBuildingQueued = false;
	m_bVisibilitySetup = false;
	m_bBombarded = false;
	m_bDrafted = false;
	m_bAirliftTargeted = false;
	for (int iStatus = 0; iStatus < NUM_CITY_STATUSES; ++iStatus)
	{
		m_aiStatusTurns[iStatus] = 0;
	}
	m_bCitizensAutomated = true;
	m_bProductionAutomated = false;
	m_bWallOverride = false;
	m_bInfoDirty = true;
	m_bLayoutDirty = false;
	m_bPlundered = false;
	m_bPopProductionProcess = false;
	// the citizen-juggle bracket is run state, so a recycled city never inherits a half-open one
	m_iCitizenJugglingCount = 0;
	m_bJuggleDeferredSpec = false;
	m_bJuggleDeferredWork = false;
	m_juggleSpecialistStart.clear();
	m_juggleWorkedStart.clear();

	m_eOwner = eOwner;
	m_ePreviousOwner = NO_PLAYER;
	m_eOriginalOwner = eOwner;
	m_eCultureLevel = NO_CULTURELEVEL;

	m_iRevolutionIndex = 0;
	m_iLocalRevIndex = -1;
	m_iRevIndexDistanceMod = 0;
	m_iRevIndexAverage = 0;
	m_iRevolutionCounter = 0;
	m_iReinforcementCounter = 0;

	//TB Combat Mod (Buildings) begin
	m_iModifiedBuildingDefenseRecoverySpeedCap = 0;
	m_iPrioritySpecialist = NO_SPECIALIST;
	m_icachedPropertyNeedsTurn = 0;
	//TB Combat Mod (Buildings) end

	m_iZoCCount = 0;

	for (int iI = 0; iI < NUM_YIELD_TYPES; iI++)
	{
		m_aiYieldRateModifier[iI] = 0;
		m_aiTradeYield[iI] = 0;
		m_abBaseYieldRankValid[iI] = false;
		m_abYieldRankValid[iI] = false;
		m_aiBaseYieldRank[iI] = -1;
		m_aiYieldRank[iI] = -1;
	}

	for (int iI = 0; iI < NUM_COMMERCE_TYPES; iI++)
	{
		m_aiProductionToCommerceModifier[iI] = 0;
		m_abCommerceRankValid[iI] = false;
		m_aiCommerceRank[iI] = -1;
	}

	for (int iI = 0; iI < NUM_DOMAIN_TYPES; iI++)
	{
	}

	for (int iI = 0; iI < MAX_PLAYERS; iI++)
	{
		m_aiCulture[iI] = 0;
		m_aiNumRevolts[iI] = 0;
		m_abEverOwned[iI] = false;
		m_abTradeRoute[iI] = false;
	}

	for (int iI = 0; iI < MAX_TEAMS; iI++)
	{
		m_abRevealed[iI] = false;
		m_abEspionageVisibility[iI] = false;
	}

	m_hasBuildings.clear();
	m_paTradeCities.clear();
	m_orderQueue.clear();
	m_aEventsOccured.clear();
	m_aBuildingYieldChange.clear();
	m_aBuildingCommerceChange.clear();
	m_aBuildingHappyChange.clear();
	m_aBuildingHealthChange.clear();
	m_Properties.clear();
	m_progressOnBuilding.clear();
	m_delayOnBuilding.clear();
	m_progressOnUnit.clear();
	m_delayOnUnit.clear();
	m_corpBonusProduction.clear();
	m_buildingLedger.clear();

	m_szName.clear();
	m_szScriptData = "";

	m_bPopulationRankValid = false;
	m_iPopulationRank = -1;

	if (!bConstructorCall)
	{
		FAssertMsg(0 < GC.getNumSpecialistInfos(), "GC.getNumSpecialistInfos() is not greater than zero but an array is being allocated in CvCity::reset");
		FAssertMsg(m_ppaaiLocalSpecialistExtraYield == NULL, "about to leak memory, CvCity::m_ppaaiLocalSpecialistExtraYield");
		m_ppaaiLocalSpecialistExtraYield = new int* [GC.getNumSpecialistInfos()];
		m_paiSpecialistBannedCount = new int[GC.getNumSpecialistInfos()];
		for (int iI = 0; iI < GC.getNumSpecialistInfos(); iI++)
		{
			m_ppaaiLocalSpecialistExtraYield[iI] = new int[NUM_YIELD_TYPES];
			for (int iJ = 0; iJ < NUM_YIELD_TYPES; iJ++)
			{
				m_ppaaiLocalSpecialistExtraYield[iI][iJ] = 0;
			}
			m_paiSpecialistBannedCount[iI] = 0;
			for (int iJ = 0; iJ < NUM_COMMERCE_TYPES; iJ++)
			{
			}
		}

		FAssertMsg(0 < GC.getNumBonusInfos(), "GC.getNumBonusInfos() is not greater than zero but an array is being allocated in CvCity::reset");
		m_ppaaiExtraBonusAidModifier = new int* [GC.getNumBonusInfos()];

		for (int iI = 0; iI < GC.getNumBonusInfos(); iI++)
		{
			m_ppaaiExtraBonusAidModifier[iI] = new int[GC.getNumPropertyInfos()];
			for (int iJ = 0; iJ < GC.getNumPropertyInfos(); iJ++)
			{
				m_ppaaiExtraBonusAidModifier[iI][iJ] = 0;
			}
		}

		m_paiProjectProduction = new int[GC.getNumProjectInfos()];
		for (int iI = 0; iI < GC.getNumProjectInfos(); iI++)
		{
			m_paiProjectProduction[iI] = 0;
		}

		for (int iI = GC.getNumBuildingInfos() - 1; iI > -1; iI--)
		{
			m_bHasBuildings[iI] = false;
		}

		m_paiUnitProduction = new int[GC.getNumUnitInfos()];
		m_paiGreatPeopleUnitRate = new int[GC.getNumUnitInfos()];
		m_paiGreatPeopleUnitProgress = new int[GC.getNumUnitInfos()];
		for (int iI = 0; iI < GC.getNumUnitInfos(); iI++)
		{
			m_paiUnitProduction[iI] = 0;
			m_paiGreatPeopleUnitRate[iI] = 0;
			m_paiGreatPeopleUnitProgress[iI] = 0;
		}

		FAssertMsg((0 < GC.getNumSpecialistInfos()), "GC.getNumSpecialistInfos() is not greater than zero but an array is being allocated in CvCity::reset");
		m_paiSpecialistCount = new int[GC.getNumSpecialistInfos()];
		m_paiForceSpecialistCount = new int[GC.getNumSpecialistInfos()];
		m_paiFreeSpecialistCountUnattributed = new int[GC.getNumSpecialistInfos()];
		for (int iI = 0; iI < GC.getNumSpecialistInfos(); iI++)
		{
			m_paiSpecialistCount[iI] = 0;
			m_paiForceSpecialistCount[iI] = 0;
			m_paiFreeSpecialistCountUnattributed[iI] = 0;
		}

		m_paiReligionInfluence = new int[GC.getNumReligionInfos()];
		m_pabHasReligion = new bool[GC.getNumReligionInfos()];
		for (int iI = 0; iI < GC.getNumReligionInfos(); iI++)
		{
			m_paiReligionInfluence[iI] = 0;
			m_pabHasReligion[iI] = false;
		}

		m_pabHasCorporation = new bool[GC.getNumCorporationInfos()];
		for (int iI = 0; iI < GC.getNumCorporationInfos(); iI++)
		{
			m_pabHasCorporation[iI] = false;
		}

		FAssertMsg((0 < GC.getNumUnitCombatInfos()), "GC.getNumUnitCombatInfos() is not greater than zero but an array is being allocated in CvCity::reset");
		m_paiUnitCombatExtraStrength = new int[GC.getNumUnitCombatInfos()];
		m_paiUnitCombatDefenseAgainstModifier = new int[GC.getNumUnitCombatInfos()];

		for (int iI = 0; iI < GC.getNumUnitCombatInfos(); iI++)
		{
			m_paiUnitCombatExtraStrength[iI] = 0;
			m_paiUnitCombatDefenseAgainstModifier[iI] = 0;
		}

		FAssertMsg((0 < GC.getNumPropertyInfos()), "GC.getNumPropertyInfos() is not greater than zero but an array is being allocated in CvCity::reset");
		m_paiAidRate = new int[GC.getNumPropertyInfos()];
		for (int iI = 0; iI < GC.getNumPropertyInfos(); iI++)
		{
			m_paiAidRate[iI] = 0;
		}

		FAssertMsg((0 < GC.getNumInvisibleInfos()), "GC.getNumInvisibleInfos() is not greater than zero but an array is being allocated in CvCity::reset");
		for (int iI = 0; iI < GC.getNumInvisibleInfos(); iI++)
		{
		}

		m_pabWorkingPlot = new bool[NUM_CITY_PLOTS];
		for (int iI = 0; iI < NUM_CITY_PLOTS; iI++)
		{
			m_pabWorkingPlot[iI] = false;
		}
		const int iMaxTradeRoutes = getMaxTradeRoutes();

		FAssertMsg((0 < iMaxTradeRoutes), "Max Trade Routes is not greater than zero but an array is being allocated in CvCity::reset");
		m_paTradeCities = std::vector<IDInfo>(iMaxTradeRoutes);

		m_paiFreeBonus = new int[GC.getNumBonusInfos()];
		m_paiFreeBonusEvents = new int[GC.getNumBonusInfos()];
		for (int iI = 0; iI < GC.getNumBonusInfos(); iI++)
		{
			m_paiFreeBonus[iI] = 0;
			m_paiFreeBonusEvents[iI] = 0;
		}

		m_cachedPropertyNeeds = new int[GC.getNumPropertyInfos()];
		for (int iI = 0; iI < GC.getNumPropertyInfos(); iI++)
		{
			m_cachedPropertyNeeds[iI] = 0;
		}

		FAssertMsg(m_pabAutomatedCanBuild == NULL, "about to leak memory, CvCity::m_pabAutomatedCanBuild");
		m_pabAutomatedCanBuild = new bool[GC.getNumBuildInfos()];
		for (int iI = 0; iI < GC.getNumBuildInfos(); iI++)
		{
			m_pabAutomatedCanBuild[iI] = true;
		}
	}

	if (eOwner != NO_PLAYER)
	{
		m_BuildingList.setPlayerToOwner();
		m_UnitList.setPlayerToOwner();
		m_UnitList.init();
	}

	if (!bConstructorCall)
	{
		m_BuildingList.init();
		AI_reset();
	}

	m_bIsGreatWallSeed = false;
	m_deferringBonusProcessingCount = 0;

	m_outputHistory.reset();
}


//////////////////////////////////////
// graphical only setup
//////////////////////////////////////
void CvCity::setupGraphical()
{
	PROFILE_FUNC();

	if (!GC.IsGraphicsInitialized())
	{
		return;
	}

	if (!isInViewport())
	{
		return;
	}

	if (getEntity() == NULL)
	{
		createCityEntity(this);
	}

	CvDLLEntity::setup();

	setInfoDirty(true);
	setLayoutDirty(true);
}


int CvCity::getRevolutionIndex() const
{
	return m_iRevolutionIndex;
}

void CvCity::setRevolutionIndex(int iNewValue)
{
	if (iNewValue < 0)
		iNewValue = 0;

	m_iRevolutionIndex = iNewValue;
}

void CvCity::changeRevolutionIndex(int iChange)
{
	setRevolutionIndex(getRevolutionIndex() + iChange);
}

int CvCity::getLocalRevIndex() const
{
	return m_iLocalRevIndex;
}

void CvCity::setLocalRevIndex(int iNewValue)
{
	m_iLocalRevIndex = iNewValue;
}

void CvCity::changeLocalRevIndex(int iChange)
{
	setLocalRevIndex(getLocalRevIndex() + iChange);
}

int CvCity::getRevIndexAverage() const
{
	return m_iRevIndexAverage;
}

void CvCity::setRevIndexAverage(int iNewValue)
{
	m_iRevIndexAverage = range(iNewValue, 0, 3400);
}

void CvCity::updateRevIndexAverage()
{
	setRevIndexAverage((2 * getRevIndexAverage() + getRevolutionIndex()) / 3);
}

void CvCity::changeRevIndexDistanceMod(const int iChange)
{
	m_iRevIndexDistanceMod += iChange;
}

int CvCity::getRevolutionCounter() const
{
	return m_iRevolutionCounter;
}

void CvCity::setRevolutionCounter(int iNewValue)
{
	if (iNewValue < 0)
		iNewValue = 0;

	m_iRevolutionCounter = iNewValue;
}

void CvCity::changeRevolutionCounter(int iChange)
{
	setRevolutionCounter(getRevolutionCounter() + iChange);
}

int CvCity::getReinforcementCounter() const
{
	return m_iReinforcementCounter;
}

void CvCity::setReinforcementCounter(int iNewValue)
{
	if (iNewValue < 0)
		iNewValue = 0;

	m_iReinforcementCounter = iNewValue;
}

void CvCity::changeReinforcementCounter(int iChange)
{
	setReinforcementCounter(getReinforcementCounter() + iChange);
}

bool CvCity::isRecentlyAcquired() const
{
	return
	(
		(GC.getGame().getGameTurn() - getGameTurnAcquired())
		<
		12 * CvGameSpeedScale::speedPercent() / 100
	);
}


void CvCity::kill(bool bUpdatePlotGroups, bool bUpdateCulture)
{
	PROFILE_FUNC();

	if (isCitySelected())
	{
		gDLL->getInterfaceIFace()->clearSelectedCities();
	}
	while (m_workers.size() > 0)
	{
		setWorkerHave(m_workers[0], false);
	}
	const PlayerTypes eOwner = getOwner();
	CvPlayer& kOwner = GET_PLAYER(eOwner);

	CvPlot* pPlot = plot();

	// Take this plot out of zobrist hashes for local plot groups
	pPlot->ToggleInPlotGroupsZobristContributors();

	CvPlotGroup* originalTradeNetworkConnectivity[MAX_PLAYERS];
	// Whose trade networks was this city relevant to prior to razing
	if (bUpdatePlotGroups)
	{
		for (int iI = 0; iI < MAX_PLAYERS; iI++)
		{
			originalTradeNetworkConnectivity[iI] = GET_PLAYER((PlayerTypes)iI).isAlive() ? pPlot->getPlotGroup((PlayerTypes)iI) : NULL;
		}
	}

	algo::for_each(
		plots(NUM_CITY_PLOTS) | filtered(CvPlot::fn::getWorkingCityOverride() == this),
		CvPlot::fn::setWorkingCityOverride(NULL)
	);
	setCultureLevel(NO_CULTURELEVEL, false);
	clearCultureDistanceCache();

	for (int iI = 0, iNum = GC.getNumBuildingInfos(); iI < iNum; iI++)
	{
		changeHasBuilding((BuildingTypes)iI, false);
	}

	for (int iI = 0; iI < GC.getNumReligionInfos(); iI++)
	{
		setHasReligion((ReligionTypes)iI, false, false, true);

		if (isHolyCity((ReligionTypes)iI))
		{
			GC.getGame().setHolyCity((ReligionTypes)iI, NULL, false);
		}
	}

	for (int iI = 0; iI < GC.getNumCorporationInfos(); iI++)
	{
		setHasCorporation((CorporationTypes)iI, false, false);

		if (isHeadquarters((CorporationTypes)iI))
		{
			GC.getGame().setHeadquarters((CorporationTypes)iI, NULL, false);
		}
	}

	setPopulation(0);
	//AI_assignWorkingPlots();
	clearOrderQueue();

	if (m_orderQueue.empty() && kOwner.isHumanPlayer(true))
	{
		kOwner.setIdleCity(getID(), false);
	}
	else
	{
		FAssertMsg(!kOwner.isIdleCity(getID()), "City with production is cached as idle!");
	}

	// remember the visibility before we take away the city from the plot below
	std::vector<bool> abEspionageVisibility;
	for (int iI = 0; iI < MAX_TEAMS; iI++)
	{
		abEspionageVisibility.push_back(getEspionageVisibility((TeamTypes)iI));
	}

	// Need to clear trade routes of dead city, else they'll be claimed for the owner forever
	clearTradeRoutes();

	const bool bCapital = isCapital();

	area()->changeCitiesPerPlayer(eOwner, -1);
	GET_TEAM(getTeam()).changeNumCities(-1);
	GC.getGame().changeNumCities(-1);

	// The city is going: withdraw its membership from every plot of its work area, so no store keeps folding a
	// city that no longer exists.
	changeWorkableArea(getNumCityPlots(), 0);

	pPlot->setPlotCity(NULL);

	pPlot->setImprovementType(GC.getIMPROVEMENT_CITY_RUINS());


	CvEventReporter::getInstance().cityLost(this);

	kOwner.deleteCity(getID());

	if (bUpdateCulture)
	{
		pPlot->updateCulture(true, false);

		algo::for_each(pPlot->adjacent(), bind(CvPlot::updateCulture, _1, true, false));
	}

	for (int iI = 0; iI < MAX_PC_TEAMS; iI++)
	{
		if (GET_TEAM(kOwner.getTeam()).isVassal((TeamTypes)iI))
		{
			pPlot->changeAdjacentSight((TeamTypes)iI, 0, false, NULL, false);
		}
	}

	for (int iI = 0; iI < MAX_PC_TEAMS; iI++)
	{
		if (abEspionageVisibility[iI])
		{
			pPlot->changeAdjacentSight((TeamTypes)iI, 0, false, NULL, false);
		}
	}


	GC.getMap().updateWorkingCity();

	kOwner.AI_makeAssignWorkDirty();

	if (bCapital)
	{
		for (int iI = 0; iI < MAX_PC_TEAMS; iI++)
		{
			if (GET_TEAM(kOwner.getTeam()).isHasEmbassy((TeamTypes)iI))
			{
				pPlot->changeAdjacentSight((TeamTypes)iI, 0, false, NULL, false);
			}
		}
		kOwner.findNewCapital();

		GET_TEAM(kOwner.getTeam()).resetVictoryProgress();
	}
	if (bUpdatePlotGroups)
	{
		PROFILE("CvCity::kill.UpdatePlotGroups");
		for (int iI = 0; iI < MAX_PLAYERS; iI++)
		{
			if (GET_PLAYER((PlayerTypes)iI).isAlive())
			{
				if (originalTradeNetworkConnectivity[iI] != NULL)
				{
					originalTradeNetworkConnectivity[iI]->recalculatePlots();
				}
				else if (pPlot->isTradeNetwork(GET_PLAYER((PlayerTypes)iI).getTeam()))
				{
					GET_PLAYER((PlayerTypes)iI).updatePlotGroups(pPlot->area());
				}
			}
		}
	}

	if (eOwner == GC.getGame().getActivePlayer())
	{
		gDLL->getInterfaceIFace()->setDirty(SelectionButtons_DIRTY_BIT, true);
	}
}

void CvCity::killTestCheap()
{
	if (isCitySelected())
	{
		gDLL->getInterfaceIFace()->clearSelectedCities();
	}
	GET_PLAYER(getOwner()).deleteCity(getID());
}

void CvCity::doTurn()
{
	PROFILE("CvCity::doTurn()");
	PERF_SCOPE("city.doTurn", getOwner());

	// Statuses tick FIRST, so a status re-applied later this turn by its own condition survives.
	doStatusTurn();

	// [CIT/proplevel] -- per-city property snapshot at the start of each turn (crime/disease/
	// education/...), so property TRENDS are trackable from the log over turns -- hard to eyeball
	// in-game. One line per active property: val = current level, change = per-turn drift.
	if (gCityLogLevel >= 1)
	{
		const CvProperties* pProps = getPropertiesConst();
		const int iTurn = GC.getGame().getGameTurn();
		for (int iI = 0; iI < pProps->getNumProperties(); iI++)
		{
			const PropertyTypes eProp = pProps->getProperty(iI);
			logCityAI(1, "[CIT/proplevel] turn=%d city=%S owner=%d prop=%s val=%d change=%d",
				iTurn, getName().GetCString(), (int)getOwner(), GC.getPropertyInfo(eProp).getType(),
				pProps->getValueByProperty(eProp), pProps->getChangeByProperty(eProp));
		}
	}

	FAssert(m_deferringBonusProcessingCount == 0);


	//	Fail safe
	m_deferringBonusProcessingCount = 0;

	{
		PERF_SCOPE("city.cacheFlush", getOwner());
		setBuildingListInvalid();
		setUnitListInvalid();
	}

	m_unitSourcedPropertyCache.clear();

	//	Slight hack for now - will need to change as and when we go multi-threaded
	CvPlot::setDeferredPlotGroupRecalculationMode(true);

	if (!isBombarded())
	{
		changeDefenseDamage(-cityDefenseRecoveryRate());
	}

	setLastDefenseDamage(getDefenseDamage());
	setBombarded(false);
	setPlundered(false);
	setDrafted(false);
	setAirliftTargeted(false);
	setBuiltFoodProducedUnit(false);
	//Promotes Units if there is a building that allows it
	//Checks conditions of buildings, may disable or enable some

	//Damages enemy units around the city, if applicable
	{ PERF_SCOPE("city.doAttack", getOwner()); doAttack(); }
	//Heals friendly units in the city extra, if applicable
	//Spreads corporations
	{ PERF_SCOPE("city.doCorporation", getOwner()); doCorporation(); }
	//Counts down the disable power timer


	doWarWeariness();

	{ PERF_SCOPE("city.AI_doTurn", getOwner()); AI_doTurn(); }

	bool bAllowNoProduction;
	{ PERF_SCOPE("city.doCheckProduction", getOwner()); bAllowNoProduction = !doCheckProduction(); }

	// THE WAREHOUSE EDGE -- the x100 per-turn surplus is banked into the whole-unit food BAR, so the reduce is
	// here and the serialized food (and its growth threshold) keep their meaning ([north-star.md] warehouse
	// carve-out; the same edge doGreatPeople uses for the great-people ledger).
	changeFood(foodDifference() / 100, true);

	{ PERF_SCOPE("city.doCulture", getOwner()); doCulture(); }

	// updating after plot culture ensures player always sees correct ownership on plot,
	// but plot could technically wiggle back and forth during AI turns.
	int aiOwnCommerces[NUM_COMMERCE_TYPES];
	getCommerces(aiOwnCommerces);
	{ PERF_SCOPE("city.doPlotCulture", getOwner()); doPlotCulture(getOwner(), aiOwnCommerces[COMMERCE_CULTURE] / 100); }

	//	Force deferred plot group recalculation to happen now before we assess production
	{ PERF_SCOPE("city.plotGroupRecalc", getOwner()); CvPlot::setDeferredPlotGroupRecalculationMode(false); }

	{ PERF_SCOPE("city.doProduction", getOwner()); doProduction(bAllowNoProduction); }

	{ PERF_SCOPE("city.advertiseTender", getOwner()); GET_PLAYER(getOwner()).getContractBroker().advertiseTender(this, AI_getBuildPriority()); }

	{ PERF_SCOPE("city.doDecay", getOwner()); doDecay(); }

	{ PERF_SCOPE("city.doReligion", getOwner()); doReligion(); }

	{ PERF_SCOPE("city.doGreatPeople", getOwner()); doGreatPeople(); }


	updateEspionageVisibility(true);

	setCurrAirlift(0);



	//TB Combat Mod (Buildings) end

	if (getCultureUpdateTimer() > 0)
	{
		changeCultureUpdateTimer(-1);
	}

	if (getOccupationTimer() > 0)
	{
		changeOccupationTimer(-1);
	}

	if (getHurryAngerTimer() > 0)
	{
		changeHurryAngerTimer(-1);
	}

	if (getRevRequestAngerTimer() > 0)
	{
		changeRevRequestAngerTimer(-1);
	}

	if (getRevSuccessTimer() > 0)
	{
		changeRevSuccessTimer(-1);
	}

	if (getConscriptAngerTimer() > 0)
	{
		changeConscriptAngerTimer(-1);
	}

	if (getDefyResolutionAngerTimer() > 0)
	{
		changeDefyResolutionAngerTimer(-1);
	}

	if (getHappinessTimer() > 0)
	{
		changeHappinessTimer(-1);
	}

	if (getLandmarkAngerTimer() > 0)
	{
		changeLandmarkAngerTimer(-1);
	}

	if (getEspionageHealthCounter() > 0)
	{
		changeEspionageHealthCounter(-1);
	}

	if (getEspionageHappinessCounter() > 0)
	{
		changeEspionageHappinessCounter(-1);
	}

	if (isOccupation() || (angryPopulation() > 0) || (healthRate() < 0))
	{
		setWeLoveTheKingDay(false);
	}
	else if ((getPopulation() >= GC.getWE_LOVE_THE_KING_POPULATION_MIN_POPULATION()) && (GC.getGame().getSorenRandNum(GC.getWE_LOVE_THE_KING_RAND(), "Do We Love The King?") < getPopulation()))
	{
		setWeLoveTheKingDay(true);
	}
	else
	{
		setWeLoveTheKingDay(false);
	}

	clearUpgradeCache(NO_UNIT);

	// ONEVENT - Do turn
	{ PERF_SCOPE("city.py.cityDoTurn", getOwner()); CvEventReporter::getInstance().cityDoTurn(this, getOwner()); }
}

// PLACE the queue-excluded buildings (see CvCity.h for the ruling this implements). Every notConstructible
// building is placed here, unconditionally, and DORMANCY decides everything afterwards -- so this evaluates no
// placement gate, and it removes nothing.
//
// The two per-turn passes it replaces did the opposite of both: the property-band sweep re-derived every band's
// range and its full construct gate for every property in every city EVERY TURN, adding and removing buildings as
// values crossed thresholds; the autobuild pass re-ran a construct gate per autobuild and could tear one back out.
// Both are now one operate condition the enabler already maintains (the PROPERTY band atom and the dormant
// successor list the curator authors) -- re-checked by targeted propagation when a value actually moves, never by
// a per-turn scan.
void CvCity::placeSystemBuildings()
{
	PROFILE_EXTRA_FUNC();
	// ⛔ THE POPULATION IS THE PROPERTY BANDS PLUS THE AUTOBUILD SET (owner). `notConstructible` bars the
	// production queue and says nothing about placement (enabler.md §3) -- WHO places a queue-excluded entity
	// belongs to the system that owns it: the grants machine hands over a granted building, setHeadquarters
	// places a corporate HQ in the one city holding it, the outcome `constructs` verb awards an achievement.
	// Two populations genuinely belong in every city, each identified by what the DATA says, never by
	// `notConstructible`: the bands (a `requires.operate` PROPERTY band) and the identity.autoBuild set (the
	// legacy per-turn doAutobuild population -- housing, pests, resources, presence and civic markers, the C_AD
	// adoption markers), minus its world/team-capped members, whose cap is a cross-player race dormancy cannot
	// express (the enabler's census excludes them).
	// ⚑ Placement stays UNCONDITIONAL and removes nothing: placed once, the operate clause decides active vs
	// dormant forever, which is what deletes both legacy per-turn passes.
	// ⚑ bFirst = false: placement is NOT the considered action for this class -- its ACTIVATION is, and the
	// trigger engine fires the considered building-grant leg on that crossing (CvTriggerEngine, enabler.md §3).
	const std::vector<int>& aBands = EnablerKernel::propertyBandBuildings();
	for (size_t iBand = 0; iBand < aBands.size(); ++iBand)
	{
		const BuildingTypes eBuilding = (BuildingTypes)aBands[iBand];
		if (!hasBuilding(eBuilding))
		{
			setHasBuilding(eBuilding, true, getOwner(), GC.getGame().getGameTurnYear(), /*bFirst*/ false);
		}
	}
	const std::vector<int>& aAutoBuilds = EnablerKernel::autoBuildBuildings();
	for (size_t iAuto = 0; iAuto < aAutoBuilds.size(); ++iAuto)
	{
		const BuildingTypes eBuilding = (BuildingTypes)aAutoBuilds[iAuto];
		if (!hasBuilding(eBuilding))
		{
			setHasBuilding(eBuilding, true, getOwner(), GC.getGame().getGameTurnYear(), /*bFirst*/ false);
		}
	}
}

bool CvCity::isCitySelected() const
{
	return gDLL->getInterfaceIFace()->isCitySelected(const_cast<CvCity*>(this));
}


bool CvCity::canBeSelected() const
{
	PROFILE_EXTRA_FUNC();
	if ((getTeam() == GC.getGame().getActiveTeam()) || GC.getGame().isDebugMode())
	{
		return true;
	}

	if (GC.getGame().getActiveTeam() != NO_TEAM)
	{
		if (plot()->isInvestigate(GC.getGame().getActiveTeam()))
		{
			return true;
		}
	}

	// EspionageEffect
	for (int iLoop = 0; iLoop < GC.getNumEspionageMissionInfos(); iLoop++)
	{
		// Check the XML
		if (GC.getEspionageMissionInfo((EspionageMissionTypes)iLoop).isPassive() && GC.getEspionageMissionInfo((EspionageMissionTypes)iLoop).isInvestigateCity())
		{
			// Is Mission good?
			if (GET_PLAYER(GC.getGame().getActivePlayer()).canDoEspionageMission((EspionageMissionTypes)iLoop, getOwner(), plot(), -1, NULL))
			{
				return true;
			}
		}
	}

	return false;
}


/*DllExport*/ void CvCity::updateSelectedCity(bool bTestProduction)
{
	OutputDebugString(CvString::format("Exe updating selected city (bTestProduction=%d)\n", (int)bTestProduction).c_str());
	algo::for_each(plots(), bind(CvPlot::updateShowCitySymbols, _1));
}


// XXX kill this?
void CvCity::updateVisibility()
{
	PROFILE_FUNC();

	if (!GC.IsGraphicsInitialized() || !isInViewport())
	{
		return;
	}

	FAssert(GC.getGame().getActiveTeam() != NO_TEAM);

	if (isVisibilitySetup())
	{
		CvDLLEntity::setVisible(isInViewport() && isRevealed(GC.getGame().getActiveTeam(), true));
	}
	else
	{
		setupGraphical();
		m_bVisibilitySetup = true;
	}

}

bool CvCity::isVisibilitySetup() const
{
	return m_bVisibilitySetup;
}


void CvCity::createGreatPeople(UnitTypes eGreatPersonUnit, bool bIncrementThreshold, bool bIncrementExperience)
{
	GET_PLAYER(getOwner()).createGreatPeople(eGreatPersonUnit, bIncrementThreshold, bIncrementExperience, getX(), getY());
}


void CvCity::doTask(TaskTypes eTask, int iData1, int iData2, bool bOption, bool bAlt, bool bShift, bool bCtrl)
{
	switch (eTask)
	{
	case TASK_RAZE:
		GET_PLAYER(getOwner()).raze(this);
		break;

	case TASK_DISBAND:
		GET_PLAYER(getOwner()).disband(this);
		break;

	case TASK_GIFT:
	{
		if (getLiberationPlayer(false) != iData1)
		{
			GET_PLAYER((PlayerTypes)iData1).acquireCity(this, false, true, true);
		}
		else liberate(false);
		break;
	}
	case TASK_KEEP:
		// In this context: bOption = bConquest; bAlt = bTrade.
		CvEventReporter::getInstance().cityAcquiredAndKept((PlayerTypes)iData1, getOwner(), this, bOption, bAlt);
		break;

	case TASK_LIBERATE:
		liberate(iData1 != 0);
		break;

	case TASK_SET_AUTOMATED_CITIZENS:
		setCitizensAutomated(bOption);
		break;

	case TASK_SET_AUTOMATED_PRODUCTION:
		setProductionAutomated(bOption);
		break;

	case TASK_SET_EMPHASIZE:
		AI_setEmphasize(((EmphasizeTypes)iData1), bOption);
		break;

	case TASK_EMPHASIZE_SPECIALIST:
		AI_setEmphasizeSpecialist((SpecialistTypes)iData1, bOption);
		break;

	case TASK_CHANGE_SPECIALIST:
		alterSpecialistCount(((SpecialistTypes)iData1), iData2);
		break;

	case TASK_CHANGE_WORKING_PLOT:
		alterWorkingPlot(iData1);
		break;

	case TASK_CLEAR_WORKING_OVERRIDE:
		clearWorkingOverride(iData1);
		break;

	case TASK_HURRY:
		hurry((HurryTypes)iData1);
		break;

	case TASK_CONSCRIPT:
		conscript();
		break;

	case TASK_CLEAR_ORDERS:
		clearOrderQueue();
		break;

	case TASK_RALLY_PLOT:
		setRallyPlot(GC.getMap().plot(iData1, iData2));
		break;

	case TASK_CLEAR_RALLY_PLOT:
		setRallyPlot(NULL);
		break;
	default:
		FErrorMsg("eTask failed to match a valid option");
		break;
	}
}


int CvCity::getCityPlotIndex(const CvPlot* pPlot) const
{
	return plotCityXY(this, pPlot);
}


CvPlot* CvCity::getCityIndexPlot(int iIndex) const
{
	return plotCity(getX(), getY(), iIndex);
}


bool CvCity::canWork(const CvPlot* pPlot) const
{
	if (pPlot->getWorkingCity() != this)
	{
		return false;
	}

	FAssertMsg(getCityPlotIndex(pPlot) != -1, "getCityPlotIndex(pPlot) is expected to be assigned (not -1)");

	if (getCityPlotIndex(pPlot) >= getNumCityPlots()) return false; // Just in case FAssertMsg doesn't end the function.

	//in the rare case that a city ends up with an invisible animal inside the city or something, the city plot should be made immune to the following effect.
	if (pPlot != plot())
	{
		if (pPlot->plotCheck(PUF_canSiege, getOwner()) != NULL)
		{
			return false;
		}
	}

	if (pPlot->isWater())
	{
		if (!(GET_TEAM(getTeam()).isWaterWork()))
		{
			return false;
		}

		if (pPlot->getBlockadedCount(getTeam()) > 0)
		{
			return false;
		}

		/* Replaced by blockade mission, above
		if (!(pPlot->plotCheck(PUF_canDefend, -1, -1, NO_PLAYER, getTeam())))
		{
			foreach_(const CvPlot* pLoopPlot, pPlot->adjacent())
			{
				if (pLoopPlot->isWater())
				{
					if (pLoopPlot->plotCheck(PUF_canSiege, getOwner()) != NULL)
					{
						return false;
					}
				}
			}
		}
		*/
	}

	if (!(pPlot->hasYield()))
	{
		return false;
	}

	return true;
}


void CvCity::verifyWorkingPlot(int iIndex)
{
	FASSERT_BOUNDS(0, NUM_CITY_PLOTS, iIndex);

	if (isWorkingPlot(iIndex))
	{
		const CvPlot* pPlot = getCityIndexPlot(iIndex);

		if (pPlot != NULL && !canWork(pPlot))
		{
			setWorkingPlot(iIndex, false);

			AI_setAssignWorkDirty(true);
		}
	}
}


void CvCity::verifyWorkingPlots()
{
	PROFILE_EXTRA_FUNC();
	for (int iI = 0; iI < getNumCityPlots(); iI++)
	{
		verifyWorkingPlot(iI);
	}
}


void CvCity::clearWorkingOverride(int iIndex)
{
	CvPlot* pPlot = getCityIndexPlot(iIndex);

	if (pPlot != NULL)
	{
		pPlot->setWorkingCityOverride(NULL);
	}
}


int CvCity::countNumImprovedPlots(ImprovementTypes eImprovement, bool bPotential) const
{
	PROFILE_EXTRA_FUNC();
	int iCount = 0;

	foreach_(const CvPlot* pLoopPlot, plots())
	{
		if (pLoopPlot->getWorkingCity() == this)
		{
			if (eImprovement != NO_IMPROVEMENT)
			{
				if (pLoopPlot->getImprovementType() == eImprovement ||
					(bPotential && pLoopPlot->canHaveImprovement(eImprovement, getTeam())))
				{
					++iCount;
				}
			}
			else if (pLoopPlot->getImprovementType() != NO_IMPROVEMENT)
			{
				iCount++;
			}
		}
	}

	return iCount;
}


int CvCity::countNumWaterPlots() const
{
	return algo::count_if(plots(),
		bind(CvPlot::getWorkingCity, _1) == this && bind(CvPlot::isWater, _1));
}

int CvCity::countNumRiverPlots() const
{
	return algo::count_if(plots(),
		bind(CvPlot::getWorkingCity, _1) == this && bind(CvPlot::isRiver, _1));
}


int CvCity::findPopulationRank() const
{
	if (!m_bPopulationRankValid)
	{
		const int iRank = 1 + algo::count_if(GET_PLAYER(getOwner()).cities(),
			CvCity::fn::getPopulation() > getPopulation()
			|| (CvCity::fn::getPopulation() == getPopulation() && CvCity::fn::getID() < getID())
		);

		// shenanigans are to get around the const check
		m_bPopulationRankValid = true;
		m_iPopulationRank = iRank;
	}

	return m_iPopulationRank;
}


int CvCity::findBaseYieldRateRank(YieldTypes eYield) const
{
	if (!m_abBaseYieldRankValid[eYield])
	{
		// Ranked on the REALIZED yield off the cascade. No ÷100 anywhere here: a comparison is scale-invariant,
		// so the ×100 values rank identically ([DEC-fixedpoint-x100] -- only a reader that MIXES with a human
		// number converts).
		int aiRealizedYields[NUM_YIELD_TYPES];
		getYields(aiRealizedYields);
		const int iRate = aiRealizedYields[eYield];

		int iRank = 1;
		foreach_(const CvCity* pLoopCity, GET_PLAYER(getOwner()).cities())
		{
			int aiOtherYields[NUM_YIELD_TYPES];
			pLoopCity->getYields(aiOtherYields);

			if (aiOtherYields[eYield] > iRate
			|| (aiOtherYields[eYield] == iRate && pLoopCity->getID() < getID()))
			{
				++iRank;
			}
		}
		m_abBaseYieldRankValid[eYield] = true;
		m_aiBaseYieldRank[eYield] = iRank;
	}
	return m_aiBaseYieldRank[eYield];
}


int CvCity::findYieldRateRank(YieldTypes eYield) const
{
	PROFILE_FUNC();

	if (!m_abYieldRankValid[eYield])
	{
		// Ranked on the realized rate off the cascade; no ÷100, a comparison is scale-invariant.
		int aiRealizedYields[NUM_YIELD_TYPES];
		getYields(aiRealizedYields);
		const int iRate = aiRealizedYields[eYield];

		int iRank = 1;
		foreach_(const CvCity* pLoopCity, GET_PLAYER(getOwner()).cities())
		{
			int aiOtherYields[NUM_YIELD_TYPES];
			pLoopCity->getYields(aiOtherYields);

			if (aiOtherYields[eYield] > iRate
			|| (aiOtherYields[eYield] == iRate && pLoopCity->getID() < getID()))
			{
				++iRank;
			}
		}
		m_abYieldRankValid[eYield] = true;
		m_aiYieldRank[eYield] = iRank;
	}
	return m_aiYieldRank[eYield];
}


int CvCity::findCommerceRateRank(CommerceTypes eCommerce) const
{
	if (!m_abCommerceRankValid[eCommerce])
	{
		int aiOwnCommerces[NUM_COMMERCE_TYPES];
		getCommerces(aiOwnCommerces);
		int iRate = aiOwnCommerces[eCommerce];

		// A rank is a comparison ACROSS cities, so it is scale-invariant and reads the group value directly.
		// The retired per-channel functor row has no successor: each city answers through its own group read.
		int iRank = 1;
		foreach_(const CvCity* pLoopCity, GET_PLAYER(getOwner()).cities())
		{
			int aiLoopCommerces[NUM_COMMERCE_TYPES];
			pLoopCity->getCommerces(aiLoopCommerces);
			const int iLoopRate = aiLoopCommerces[eCommerce];
			if (iLoopRate > iRate || (iLoopRate == iRate && pLoopCity->getID() < getID()))
			{
				iRank++;
			}
		}
		m_abCommerceRankValid[eCommerce] = true;
		m_aiCommerceRank[eCommerce] = iRank;
	}
	return m_aiCommerceRank[eCommerce];
}


// Returns one of the upgrades...
UnitTypes CvCity::trainableUpgradeFor(UnitTypes eUnit) const
{
	std::set<int> seen;
	std::vector<int> frontier;
	frontier.push_back((int)eUnit);
	seen.insert((int)eUnit);
	for (size_t iHead = 0; iHead < frontier.size(); ++iHead)
	{
		const CvInfo* j = InfoRepo<CvUnitInfo>::get().get(frontier[iHead]);
		if (j == NULL) continue;
		const std::vector<int>& dorm = j->dormantTriggers();   // the unit's direct upgrades (requires.build.dormant)
		for (size_t i = 0; i < dorm.size(); ++i)
		{
			if (!seen.insert(dorm[i]).second) continue;
			if (getUnitAvailability((UnitTypes)dorm[i]) == EnablerDomain::STATE_LISTED)
			{
				return (UnitTypes)dorm[i];
			}
			frontier.push_back(dorm[i]);   // not trainable here, but its own upgrades may be
		}
	}
	return NO_UNIT;
}

// The hypothetical (see CvCity.h for the pattern it implements).
bool CvCity::couldConstructWith(BuildingTypes eCandidate, BuildingTypes eExtraBuilding) const
{
	const CvInfo* j = InfoRepo<CvBuildingInfo>::get().get((int)eCandidate);
	if (j == NULL) return false;

	CvCascadeEvalCtx ec;
	getCityContext().fillEvalCtx(ec);                      // city+plot -- the contexts fill the eval state
	GET_PLAYER(getOwner()).getEmpireContext().fillEvalCtx(ec);   // player+team
	EnablerKernel::wireOperatingBuildings(this, ec);

	std::set<int> withExtra;                               // the OVERLAY, owned here and discarded here
	if (ec.activeBuildings != NULL) withExtra = *ec.activeBuildings;
	withExtra.insert((int)eExtraBuilding);
	ec.activeBuildings = &withExtra;

	CvCascadeEvalFlags gateFlags;
	gateFlags.strictStateReligionForBuild = true;
	return cascadeGateOk(j->getGate(), ec, gateFlags) && EnablerKernel::requiresMet(j, ec);
}


int CvCity::getMaxNumWorldWonders() const
{
	return GC.getCultureLevelInfo(getCultureLevel()).getMaxWorldWonders();
}

bool CvCity::isWorldWondersMaxed() const
{
	if (GC.getGame().isOption(GAMEOPTION_CHALLENGE_ONE_CITY))
	{
		return false;
	}
	if (GC.getGame().isOption(GAMEOPTION_NO_WONDER_LIMIT))
	{
		return false;
	}
	if (getNumWorldWonders() >= getMaxNumWorldWonders())
	{
		return true;
	}
	return false;
}


int CvCity::getMaxNumTeamWonders() const
{
	return GC.getCultureLevelInfo(getCultureLevel()).getMaxTeamWonders();
}

bool CvCity::isTeamWondersMaxed() const
{
	if (GC.getGame().isOption(GAMEOPTION_CHALLENGE_ONE_CITY))
	{
		return false;
	}
	if (GC.getGame().isOption(GAMEOPTION_NO_WONDER_LIMIT))
	{
		return false;
	}
	if (getNumTeamWonders() >= getMaxNumTeamWonders())
	{
		return true;
	}
	return false;
}


int CvCity::getMaxNumNationalWonders() const
{
	return GC.getCultureLevelInfo(getCultureLevel()).getMaxNationalWonders();
}

bool CvCity::isNationalWondersMaxed() const
{
	if (GC.getGame().isOption(GAMEOPTION_CHALLENGE_ONE_CITY))
	{
		return false;
	}
	if (GC.getGame().isOption(GAMEOPTION_NO_WONDER_LIMIT))
	{
		return false;
	}
	if (getMaxNumNationalWonders() != -1 && getNumNationalWonders() >= getMaxNumNationalWonders())
	{
		return true;
	}
	return false;
}

void CvCity::clearUpgradeCache(UnitTypes eUnit) const
{
	if (eUnit == NO_UNIT)
	{
		m_eCachedAllUpgradesResults.clear();
		m_eCachedAllUpgradesResultsRoot.clear();
	}
	else
	{
		m_eCachedAllUpgradesResults.erase(eUnit);
		m_eCachedAllUpgradesResultsRoot.erase(eUnit);
	}
}

void CvCity::invalidateCachedCanTrainForUnit(UnitTypes eUnit) const
{
	PROFILE_FUNC();
	clearUpgradeCache(eUnit);
}

//	KOSHLING - cache can construct values
#ifdef _DEBUG
//	Uncomment to add runtime results checking
//#define VALIDATE_CAN_CONSTRUCT_CACHE
#endif

bool CvCity::canCreate(ProjectTypes eProject, bool bContinue, bool bTestVisible) const
{
	if (!GET_PLAYER(getOwner()).canCreate(eProject, bContinue, bTestVisible))
	{
		return false;
	}

	if (!isMapCategory(*plot(), GC.getProjectInfo(eProject)))
	{
		return false;
	}

	return true;
}


bool CvCity::canMaintain(ProcessTypes eProcess) const
{
	if (!GET_PLAYER(getOwner()).canMaintain(eProcess)
	|| Cy::call<bool>(PYGameModule, "cannotMaintain", Cy::Args() << const_cast<CvCity*>(this) << eProcess))
	{
		return false;
	}
	return true;
}


int CvCity::getFoodTurnsLeft() const
{
	// The projection divides into the whole-unit food BAR (threshold - stored), so the x100 rate reduces at this
	// use ([DEC-fixedpoint-x100]). A sub-1.00 surplus floors to 0 and correctly reports no growth rather than
	// dividing by it.
	const int iFoodDifference = foodDifference() / 100;

	if (iFoodDifference <= 0)
	{
		return growthThreshold() - getFood();
	}
	const int iFoodLeft = growthThreshold() - getFood();

	int iTurnsLeft = iFoodLeft / iFoodDifference;

	if (iTurnsLeft * iFoodDifference < iFoodLeft)
	{
		iTurnsLeft++;
	}

	return std::max(1, iTurnsLeft);
}


bool CvCity::isProduction() const
{
	return getHeadOrder();
}


bool CvCity::isProductionLimited() const
{
	bst::optional<OrderData> order = getHeadOrder();

	if (order != NULL)
	{
		switch (order->eOrderType)
		{
		case ORDER_TRAIN:
			return isLimitedUnit((UnitTypes)EXTERNAL_ORDER_IDATA(order->iData1));
			break;

		case ORDER_CONSTRUCT:
			return isLimitedWonder(static_cast<BuildingTypes>(order->iData1));
			break;

		case ORDER_CREATE:
			return isLimitedProject((ProjectTypes)(order->iData1));
			break;

		case ORDER_MAINTAIN:
		case ORDER_LIST:
			break;

		default:
			FErrorMsg("order->m_data.eOrderType failed to match a valid option");
			break;
		}
	}

	return false;
}

bool CvCity::isProductionUnitCombat(int iIndex) const
{
	bst::optional<OrderData> order = getHeadOrder();

	if (order)
	{
		switch (order->eOrderType)
		{
		case ORDER_TRAIN:
			return GC.getUnitInfo(order->getUnitType()).hasCombatClass((UnitCombatTypes)iIndex);
		case ORDER_CONSTRUCT:
			return false;
		case ORDER_CREATE:
			return false;
		case ORDER_MAINTAIN:
			return false;
		case ORDER_LIST:
			return false;
		default:
			FErrorMsg("order->eOrderType failed to match a valid option");
			break;
		}
	}
	return false;
}


bool CvCity::isProductionUnit() const
{
	bst::optional<OrderData> order = getHeadOrder();
	return order && order->eOrderType == ORDER_TRAIN;
}


bool CvCity::isProductionBuilding() const
{
	bst::optional<OrderData> order = getHeadOrder();
	return order && order->eOrderType == ORDER_CONSTRUCT;
}


bool CvCity::isProductionProject() const
{
	bst::optional<OrderData> order = getHeadOrder();
	return order && order->eOrderType == ORDER_CREATE;
}


bool CvCity::isProductionProcess() const
{
	bst::optional<OrderData> order = getHeadOrder();
	return order && order->eOrderType == ORDER_MAINTAIN;
}


bool CvCity::canContinueProduction(const OrderData& order) const
{
	switch (order.eOrderType)
	{
	case ORDER_TRAIN:
		return m_enabler.units.listedForContinue((int)order.getUnitType());
	case ORDER_CONSTRUCT:
		return isBuildingContinuable(order.getBuildingType());
	case ORDER_CREATE:
		return canCreate(order.getProjectType(), true);
	case ORDER_MAINTAIN:
		return canMaintain(order.getProcessType());
	case ORDER_LIST:
		return true;
	default:
		FErrorMsg("order.eOrderType failed to match a valid option");
		break;
	}
	return false;
}


// The keyed `experience.<scope>.{unitCombats|domains}.{TARGET}` deposits, summed over every LIVE source that can
// carry one: this city's OPERATING buildings (a dormant building trains nobody), its assigned specialists scaled
// by count, and the empire's own half. Each source is asked what IT deposits onto this target.
//
// ⛔ Never a walk of a keyed container the info no longer holds (the own-data inversion,
// pedia-read-map finding 2), and never a package read: a keyed entry deliberately does NOT fold into a scope
// package outside plot scope, because folding it would hand EVERY unit the melee-only experience.
int CvCity::keyedExperience(int iTargetSegment, int iTargetFk) const
{
	if (iTargetSegment < 0 || iTargetFk < 0)
	{
		return 0;
	}
	int iExperience = 0;

	const std::set<int>& kActive = m_operatingBuildings.active;
	for (std::set<int>::const_iterator it = kActive.begin(); it != kActive.end(); ++it)
	{
		iExperience += InfoValuation::keyedTarget(
			GC.getBuildingInfo((BuildingTypes)*it).getModifiers(), MODFAM_EXPERIENCE, -1, iTargetSegment, iTargetFk);
	}

	for (int iSpecialist = 0; iSpecialist < GC.getNumSpecialistInfos(); ++iSpecialist)
	{
		const int iCount = getSpecialistCount((SpecialistTypes)iSpecialist);
		if (iCount > 0)
		{
			iExperience += iCount * InfoValuation::keyedTarget(
				GC.getSpecialistInfo((SpecialistTypes)iSpecialist).getModifiers(),
				MODFAM_EXPERIENCE, -1, iTargetSegment, iTargetFk);
		}
	}

	return iExperience + GET_PLAYER(getOwner()).keyedExperience(iTargetSegment, iTargetFk);
}


int CvCity::getDomainExperience(DomainTypes eDomain) const
{
	return keyedExperience(InfoValuation::keyedTargetSegment("domains"), (int)eDomain);
}


// The COLLECT twin of keyedExperience on the unitCombat axis: walk each live source ONCE and merge what it
// authored, rather than asking every source about every unitcombat id.
void CvCity::collectUnitCombatExperience(std::vector<std::pair<int, int> >& rows) const
{
	PROFILE_EXTRA_FUNC();
	rows.clear();
	const int iSegment = InfoValuation::keyedTargetSegment("unitCombats");
	if (iSegment < 0)
	{
		return;
	}
	std::vector<std::pair<int, int> > sourceRows;

	const std::set<int>& kActive = m_operatingBuildings.active;
	for (std::set<int>::const_iterator it = kActive.begin(); it != kActive.end(); ++it)
	{
		InfoValuation::collectKeyedTarget(
			GC.getBuildingInfo((BuildingTypes)*it).getModifiers(), MODFAM_EXPERIENCE, -1, iSegment, sourceRows);
		mergeKeyedRows(rows, sourceRows, 1);
	}
	for (int iSpecialist = 0; iSpecialist < GC.getNumSpecialistInfos(); ++iSpecialist)
	{
		const int iCount = getSpecialistCount((SpecialistTypes)iSpecialist);
		if (iCount > 0)
		{
			InfoValuation::collectKeyedTarget(
				GC.getSpecialistInfo((SpecialistTypes)iSpecialist).getModifiers(), MODFAM_EXPERIENCE, -1, iSegment, sourceRows);
			mergeKeyedRows(rows, sourceRows, iCount);
		}
	}
	const CvPlayer& kPlayer = GET_PLAYER(getOwner());
	for (int iTrait = 0; iTrait < GC.getNumTraitInfos(); ++iTrait)
	{
		if (kPlayer.hasTrait((TraitTypes)iTrait))
		{
			InfoValuation::collectKeyedTarget(
				GC.getTraitInfo((TraitTypes)iTrait).getModifiers(), MODFAM_EXPERIENCE, -1, iSegment, sourceRows);
			mergeKeyedRows(rows, sourceRows, 1);
		}
	}
}


// The experience a unit trained here starts with.
int CvCity::getProductionExperience(UnitTypes eUnit) const
{
	PROFILE_EXTRA_FUNC();
	const CvPlayer& kPlayer = GET_PLAYER(getOwner());

	int64_t lFlatSum = 0;
	int64_t lPercentSum = 0;
	InfoValuation::rolledLegsAtCity(
		*this, CascadeChannelRegistry::channelLookup(MODFAM_EXPERIENCE, EXPERIENCE_AMOUNT, -1), lFlatSum, lPercentSum);

	int64_t lExperience = lFlatSum;

	if (eUnit != NO_UNIT)
	{
		const CvUnitInfo& kUnit = GC.getUnitInfo(eUnit);

		if (kUnit.hasTag(CLS_TAG_SPY) && !GC.isSS_ENABLED())
		{
			return 0;
		}

		if (kUnit.canAcquireExperience())
		{
			// Resolved ONCE per call: the interner is only populated after load, so a file-scope static would latch -1.
			const int iUnitCombatsSegment = InfoValuation::keyedTargetSegment("unitCombats");
			const int iDomainsSegment = InfoValuation::keyedTargetSegment("domains");

			// The ROOT combat-class FKs (json §8: `combatClass` + `combatClasses`, never identity). An absent
			// primary is -1, which keyedExperience already answers 0 for, so it needs no test of its own.
			lExperience += keyedExperience(iUnitCombatsSegment, kUnit.getCombatClass());
			const std::vector<int>& kSubCombats = kUnit.getCombatClasses();
			for (size_t iSub = 0; iSub < kSubCombats.size(); ++iSub)
			{
				lExperience += keyedExperience(iUnitCombatsSegment, kSubCombats[iSub]);
			}
			lExperience += keyedExperience(iDomainsSegment, (int)kUnit.getDomain());
		}
	}

	if (kPlayer.getStateReligion() != NO_RELIGION && isHasReligion(kPlayer.getStateReligion()))
	{
		lExperience += InfoValuation::realizedAtEmpire(
			kPlayer, CascadeChannelRegistry::channelLookup(MODFAM_STATE_RELIGION, STATE_RELIGION_FREE_EXPERIENCE, -1));
	}

	return (int)std::max<int64_t>(0, lExperience * (100 + lPercentSum) / 100);
}

void CvCity::addProductionExperience(CvUnit* pUnit, bool bConscript)
{
	PROFILE_FUNC();

	if (pUnit->canAcquirePromotionAny())
	{
		// getProductionExperience is ×100 and the unit's experience is natively ×100, so the units already cancel
		// and no conversion belongs here ([DEC-fixedpoint-x100]).
		pUnit->changeExperience100(getProductionExperience(pUnit->getUnitType()) / ((bConscript) ? 2 : 1));
	}

}

UnitTypes CvCity::getProductionUnit() const
{
	bst::optional<OrderData> order = getHeadOrder();

	if (order)
	{
		switch (order->eOrderType)
		{
		case ORDER_TRAIN:
			return order->getUnitType();
		case ORDER_CONSTRUCT:
		case ORDER_CREATE:
		case ORDER_MAINTAIN:
		case ORDER_LIST:
			break;

		default:
			FErrorMsg("order->eOrderType failed to match a valid option");
			break;
		}
	}

	return NO_UNIT;
}


UnitAITypes CvCity::getProductionUnitAI() const
{
	bst::optional<OrderData> order = getHeadOrder();

	if (order != NULL)
	{
		switch (order->eOrderType)
		{
		case ORDER_TRAIN:
			return order->getUnitAIType();
		case ORDER_CONSTRUCT:
		case ORDER_CREATE:
		case ORDER_MAINTAIN:
		case ORDER_LIST:
			break;

		default:
			FErrorMsg("order->eOrderType failed to match a valid option");
			break;
		}
	}

	return NO_UNITAI;
}


BuildingTypes CvCity::getProductionBuilding() const
{
	bst::optional<OrderData> order = getHeadOrder();

	if (order != NULL)
	{
		switch (order->eOrderType)
		{
		case ORDER_TRAIN:
			break;
		case ORDER_CONSTRUCT:
			return order->getBuildingType();
		case ORDER_CREATE:
		case ORDER_MAINTAIN:
		case ORDER_LIST:
			break;

		default:
			FErrorMsg("order->eOrderType failed to match a valid option");
			break;
		}
	}

	return NO_BUILDING;
}


ProjectTypes CvCity::getProductionProject() const
{
	bst::optional<OrderData> order = getHeadOrder();

	if (order)
	{
		switch (order->eOrderType)
		{
		case ORDER_TRAIN:
		case ORDER_CONSTRUCT:
			break;
		case ORDER_CREATE:
			return order->getProjectType();
		case ORDER_MAINTAIN:
		case ORDER_LIST:
			break;

		default:
			FErrorMsg("order->eOrderType failed to match a valid option");
			break;
		}
	}

	return NO_PROJECT;
}


ProcessTypes CvCity::getProductionProcess() const
{
	bst::optional<OrderData> order = getHeadOrder();

	if (order)
	{
		switch (order->eOrderType)
		{
		case ORDER_TRAIN:
		case ORDER_CONSTRUCT:
		case ORDER_CREATE:
		case ORDER_LIST:
			break;
		case ORDER_MAINTAIN:
			return order->getProcessType();

		default:
			FErrorMsg("order->eOrderType failed to match a valid option");
			break;
		}
	}

	return NO_PROCESS;
}


const wchar_t* CvCity::getProductionName() const
{
	bst::optional<OrderData> order = getHeadOrder();

	if (order)
	{
		switch (order->eOrderType)
		{
		case ORDER_TRAIN:
			return GC.getUnitInfo(order->getUnitType()).getDescription(getCivilizationType());
		case ORDER_CONSTRUCT:
			return GC.getBuildingInfo(order->getBuildingType()).getDescription();
		case ORDER_CREATE:
			return GC.getProjectInfo(order->getProjectType()).getDescription();
		case ORDER_MAINTAIN:
			return GC.getProcessInfo(order->getProcessType()).getDescription();
		case ORDER_LIST:
			break; // It is never at the head of the list

		default:
			FErrorMsg("order->eOrderType failed to match a valid option");
			break;
		}
	}

	return L"";
}


int CvCity::getGeneralProductionTurnsLeft() const
{
	bst::optional<OrderData> order = getHeadOrder();

	if (order)
	{
		switch (order->eOrderType)
		{
		case ORDER_TRAIN:
			return getProductionTurnsLeft(order->getUnitType(), 0);
		case ORDER_CONSTRUCT:
			return getProductionTurnsLeft(order->getBuildingType(), 0);
		case ORDER_CREATE:
			return getProductionTurnsLeft(order->getProjectType(), 0);
		case ORDER_MAINTAIN:
		case ORDER_LIST:
			return 0;

		default:
			FErrorMsg("order->eOrderType failed to match a valid option");
			break;
		}
	}

	return 0;
}


const wchar_t* CvCity::getProductionNameKey() const
{
	bst::optional<OrderData> order = getHeadOrder();

	if (order)
	{
		switch (order->eOrderType)
		{
		case ORDER_TRAIN:
			return GC.getUnitInfo(order->getUnitType()).getTextKeyWide();
		case ORDER_CONSTRUCT:
			return GC.getBuildingInfo(order->getBuildingType()).getTextKeyWide();
		case ORDER_CREATE:
			return GC.getProjectInfo(order->getProjectType()).getTextKeyWide();
		case ORDER_MAINTAIN:
			return GC.getProcessInfo(order->getProcessType()).getTextKeyWide();
		case ORDER_LIST:
			break; // It is never at the head of the list

		default:
			FErrorMsg("pOrderNode->m_data.eOrderType failed to match a valid option");
			break;
		}
	}

	return L"";
}

bool CvCity::isFoodProduction(const OrderData& order) const
{
	return order.getOrderType() == ORDER_TRAIN && isFoodProduction(order.getUnitType());
}

bool CvCity::isFoodProduction() const
{
	bst::optional<OrderData> order = getHeadOrder();

	return order && isFoodProduction(*order);
}


bool CvCity::isFoodProduction(UnitTypes eUnit) const
{
	return GC.getUnitInfo(eUnit).hasSkill(CLS_SKILL_FOOD)
		|| GET_PLAYER(getOwner()).isMilitaryFoodProduction()
		&& GC.getUnitInfo(eUnit).hasTag(CLS_TAG_MILITARY);
}

namespace {
	bool matchUnitOrder(const OrderData& order, const UnitTypes unitType)
	{
		return order.eOrderType == ORDER_TRAIN && order.getUnitType() == unitType;
	}
	bool matchBuildingOrder(const OrderData& order, const BuildingTypes buildingType)
	{
		return order.eOrderType == ORDER_CONSTRUCT && order.getBuildingType() == buildingType;
	}
	bool matchProjectOrder(const OrderData& order, const ProjectTypes projectType)
	{
		return order.eOrderType == ORDER_CREATE && order.getProjectType() == projectType;
	}
	bool matchUnitAITypeOrder(const OrderData& order, const UnitAITypes unitAIType)
	{
		return order.eOrderType == ORDER_TRAIN && order.getUnitAIType() == unitAIType;
	}
};

int CvCity::getFirstUnitOrder(UnitTypes eUnit) const
{
	OrderQueue::const_iterator order = bst::find_if(m_orderQueue, bind(matchUnitOrder, _1, eUnit));
	if (order == m_orderQueue.end())
	{
		return -1;
	}
	return std::distance(m_orderQueue.begin(), order);
}

int CvCity::getFirstBuildingOrder(BuildingTypes eBuilding) const
{
	OrderQueue::const_iterator order = bst::find_if(m_orderQueue, bind(matchBuildingOrder, _1, eBuilding));
	if (order == m_orderQueue.end())
	{
		return -1;
	}
	return std::distance(m_orderQueue.begin(), order);
}

int CvCity::getFirstProjectOrder(ProjectTypes eProject) const
{
	OrderQueue::const_iterator order = bst::find_if(m_orderQueue, bind(matchProjectOrder, _1, eProject));
	if (order == m_orderQueue.end())
	{
		return -1;
	}
	return std::distance(m_orderQueue.begin(), order);
}

int CvCity::getNumTrainUnitAI(UnitAITypes eUnitAI) const
{
	return algo::count_if(m_orderQueue, bind(matchUnitAITypeOrder, _1, eUnitAI));
}


int CvCity::getProductionProgress() const
{
	bst::optional<OrderData> order = getHeadOrder();

	if (order)
	{
		switch (order->eOrderType)
		{
			case ORDER_TRAIN:
			{
				return getProgressOnUnit(order->getUnitType());
			}
			case ORDER_CONSTRUCT:
			{
				return getProgressOnBuilding(order->getBuildingType());
			}
			case ORDER_CREATE:
			{
				return getProjectProduction(order->getProjectType());
			}
			case ORDER_MAINTAIN:
			case ORDER_LIST:
			{
				break;
			}
			default: FErrorMsg("order->eOrderType failed to match a valid option");
		}
	}
	return 0;
}


int CvCity::getProductionNeeded() const
{
	bst::optional<OrderData> order = getHeadOrder();
	return order ? getProductionNeeded(*order) : MAX_INT;
}

int CvCity::getProductionNeeded(const OrderData& order) const
{
	switch (order.eOrderType)
	{
	case ORDER_TRAIN:
		return getProductionNeeded(order.getUnitType());
	case ORDER_CONSTRUCT:
		return getProductionNeeded(order.getBuildingType());
	case ORDER_CREATE:
		return getProductionNeeded(order.getProjectType());
	case ORDER_MAINTAIN:
	case ORDER_LIST:
		break;

	default:
		FErrorMsg("OrderType failed to match a valid option");
		break;
	}

	return MAX_INT;
}

int CvCity::getProductionNeeded(UnitTypes eUnit) const
{
	return std::max(1, getModifiedIntValue(GET_PLAYER(getOwner()).getProductionNeeded(eUnit), -getProductionModifier(eUnit)));
}

int CvCity::getProductionNeeded(BuildingTypes eBuilding) const
{
	return std::max(1, getModifiedIntValue(GET_PLAYER(getOwner()).getProductionNeeded(eBuilding), -getProductionModifier(eBuilding)));
}

int CvCity::getProductionNeeded(ProjectTypes eProject) const
{
	return std::max(1, getModifiedIntValue(GET_PLAYER(getOwner()).getProductionNeeded(eProject), -getProductionModifier(eProject)));
}

int CvCity::getProductionTurnsLeft() const
{
	bst::optional<OrderData> order = getHeadOrder();
	return order ? getOrderProductionTurnsLeft(*order) : 0;
}

int CvCity::getOrderProductionTurnsLeft(const OrderData& order, int iIndex) const
{
	switch (order.eOrderType)
	{
	case ORDER_TRAIN:
		return getProductionTurnsLeft(order.getUnitType(), iIndex);
	case ORDER_CONSTRUCT:
		return getProductionTurnsLeft(order.getBuildingType(), iIndex);
	case ORDER_CREATE:
		return getProductionTurnsLeft(order.getProjectType(), iIndex);
	case ORDER_MAINTAIN:
		break;
	case ORDER_LIST:
		return 0;
	default:
		FErrorMsg("order.eOrderType failed to match a valid option");
		break;
	}
	return MAX_INT;
}


int CvCity::getTotalProductionQueueTurnsLeft() const
{
	PROFILE_EXTRA_FUNC();
	if (m_orderQueue.empty())
	{
		return 0;
	}

	int currProd = getCurrentProductionDifference(ProductionCalc::FoodProduction | ProductionCalc::Overflow);
	if (currProd == 0)
	{
		return MAX_INT;
	}

	int turns = 1;
	foreach_ (const OrderData & order, m_orderQueue)
	{
		int productionNeeded = getProductionNeeded(order);
		if (productionNeeded > 999)
		{
			return 999;
		}

		while (productionNeeded > 0)
		{
			if (currProd > productionNeeded)
			{
				// Can build this turn
				currProd -= productionNeeded;
				productionNeeded = 0;
			}
			else
			{
				// Next turn
				productionNeeded -= currProd;
				currProd = getProductionDifference(order, ProductionCalc::FoodProduction);
				if (currProd <= 0)
				{
					return MAX_INT;
				}
				++turns;
			}
		}
	}

	return turns;
}

namespace {
	bool matchUnitAtPlot(const OrderData& order, const UnitAITypes contractedAIType, const CvPlot* pDestPlot)
	{
		return order.eOrderType == ORDER_TRAIN
			&& order.unit.contractedAIType == contractedAIType
			&& (pDestPlot == NULL || order.unit.plotIndex == GC.getMap().plotNum(pDestPlot->getX(), pDestPlot->getY()));
	}
};

int CvCity::numQueuedUnits(UnitAITypes contractedAIType, const CvPlot* contractedPlot) const
{
	return algo::count_if(m_orderQueue, bind(matchUnitAtPlot, _1, contractedAIType, contractedPlot));
}

int CvCity::getProductionTurnsLeft(UnitTypes eUnit, int orderIndex) const
{
	const int firstOrderIndex = getFirstUnitOrder(eUnit);
	// We can count production already put towards this if we are looking
	// at the first one enqueued, or it isn't enqueued at all (and therefore would
	// be the first one were it to be)
	const int alreadyDone = (firstOrderIndex == -1 || firstOrderIndex == orderIndex) ? getProgressOnUnit(eUnit) : 0;
	const ProductionCalc::flags foodProd = isFoodProduction(eUnit) ? ProductionCalc::FoodProduction : ProductionCalc::None;
	const int perTurnProduction = getProductionPerTurn(foodProd | ProductionCalc::Yield);
	// If we are looking at the first order then overflow would be applied
	const int nextTurnProduction = (orderIndex == 0) ? getProductionPerTurn(foodProd | ProductionCalc::Overflow | ProductionCalc::Yield) : perTurnProduction;
	return getProductionTurnsLeft(getProductionNeeded(eUnit), alreadyDone, nextTurnProduction, perTurnProduction);
}

int CvCity::getProductionTurnsLeft(BuildingTypes eBuilding, int orderIndex) const
{
	const int firstOrderIndex = getFirstBuildingOrder(eBuilding);
	// We can count production already put towards this if we are looking
	// at the first one enqueued, or it isn't enqueued at all (and therefore would
	// be the first one were it to be)
	const int alreadyDone = (firstOrderIndex == -1 || firstOrderIndex == orderIndex) ? getProgressOnBuilding(eBuilding) : 0;
	const int perTurnProduction = getProductionPerTurn(ProductionCalc::Yield);
	// If we are looking at the first order then overflow would be applied
	const int nextTurnProduction = (orderIndex == 0) ? getProductionPerTurn(ProductionCalc::Overflow | ProductionCalc::Yield) : perTurnProduction;
	return getProductionTurnsLeft(getProductionNeeded(eBuilding), alreadyDone, nextTurnProduction, perTurnProduction);
}


int CvCity::getProductionTurnsLeft(ProjectTypes eProject, int orderIndex) const
{
	const int firstOrderIndex = getFirstProjectOrder(eProject);
	// We can count production already put towards this if we are looking
	// at the first one enqueued, or it isn't enqueued at all (and therefore would
	// be the first one were it to be)
	const int alreadyDone = (firstOrderIndex == -1 || firstOrderIndex == orderIndex) ? getProjectProduction(eProject) : 0;
	const int perTurnProduction = getProductionPerTurn(ProductionCalc::Yield);
	// If we are looking at the first order then overflow would be applied
	const int nextTurnProduction = (orderIndex == 0) ? getProductionPerTurn(ProductionCalc::Overflow | ProductionCalc::Yield) : perTurnProduction;
	return getProductionTurnsLeft(getProductionNeeded(eProject), alreadyDone, nextTurnProduction, perTurnProduction);
}

int CvCity::getProductionTurnsLeft(int totalRequiredProduction, int currentProduction, int nextTurnProduction, int perTurnProduction) const
{
	int remainingProduction = std::max(0, totalRequiredProduction - currentProduction - nextTurnProduction);
	// This doesn't look right...
	if (perTurnProduction == 0)
	{
		return remainingProduction + 1;
	}

	int turnsLeft = remainingProduction / perTurnProduction;
	if (turnsLeft * perTurnProduction < remainingProduction)
	{
		turnsLeft++;
	}

	turnsLeft++;

	return std::max(1, turnsLeft);
}


void CvCity::setProductionProgress(int iNewValue)
{
	if (isProductionUnit())
	{
		setProgressOnUnit(getProductionUnit(), iNewValue);
	}
	else if (isProductionBuilding())
	{
		setProgressOnBuilding(getProductionBuilding(), iNewValue);
	}
	else if (isProductionProject())
	{
		setProjectProduction(getProductionProject(), std::max(0, iNewValue));
	}
}


void CvCity::changeProduction(int iChange)
{
	if (isProductionUnit())
	{
		changeProgressOnUnit(getProductionUnit(), iChange);
	}
	else if (isProductionBuilding())
	{
		changeProgressOnBuilding(getProductionBuilding(), iChange);
	}
	else if (isProductionProject())
	{
		changeProjectProduction(getProductionProject(), iChange);
	}
	else if (isProductionProcess())
	{
		const CvProcessInfo& kProcess = GC.getProcessInfo(getProductionProcess());

		//	Add gold and espionage directly to player totals
		GET_PLAYER(getOwner()).changeGold((kProcess.getProductionToCommerce(COMMERCE_GOLD, CASC_SCOPE_CITY) * iChange) / 100);
		GET_PLAYER(getOwner()).doEspionageOneOffPoints((kProcess.getProductionToCommerce(COMMERCE_ESPIONAGE, CASC_SCOPE_CITY) * iChange) / 100);

		//	Research accrues to the team
		const TechTypes eCurrentTech = GET_PLAYER(getOwner()).getCurrentResearch();
		if (eCurrentTech != NO_TECH)
		{
			GET_TEAM(getTeam()).changeResearchProgress(eCurrentTech,
				(kProcess.getProductionToCommerce(COMMERCE_RESEARCH, CASC_SCOPE_CITY) * iChange) / 100,
				getOwner());
		}

		//	Culture to the city itself
		changeCulture(getOwner(), (kProcess.getProductionToCommerce(COMMERCE_CULTURE, CASC_SCOPE_CITY) * iChange) / 100, false, false);
	}
}

int CvCity::getProductionModifier(const OrderData& order) const
{
	switch (order.getOrderType())
	{
	case ORDER_TRAIN:
		return getProductionModifier(order.getUnitType());
	case ORDER_CONSTRUCT:
		return getProductionModifier(order.getBuildingType());
	case ORDER_CREATE:
		return getProductionModifier(order.getProjectType());
	case ORDER_MAINTAIN:
	case ORDER_LIST:
		break;

	default:
		FErrorMsg("OrderType failed to match a valid option");
		break;
	}

	return 0;
}

int CvCity::getProductionModifier() const
{
	bst::optional<OrderData> order = getHeadOrder();
	return order ? getProductionModifier(*order) : 0;
}

// The CITY's share of an item's buildRate. The empire share is CvPlayer::getProductionModifier (already the
// keyed cascade read over the player's live sources); this is its city twin, over the city's OPERATING buildings
// -- a DORMANT building speeds nothing (enabler.md §3.2).
int CvCity::getProductionModifier(UnitTypes eUnit) const
{
	PROFILE_EXTRA_FUNC();
	const CvUnitInfo& kUnit = GC.getUnitInfo(eUnit);
	int iMultiplier = GET_PLAYER(getOwner()).getProductionModifier(eUnit);
	const bool bTypeMods = !kUnit.hasSkill(CLS_SKILL_NO_NON_TYPE_PROD_MODS);

	const std::set<int>& kActive = m_operatingBuildings.active;
	for (std::set<int>::const_iterator it = kActive.begin(); it != kActive.end(); ++it)
	{
		iMultiplier += (int)InfoValuation::unitBuildRate(
			GC.getBuildingInfo((BuildingTypes)*it).getModifiers(), eUnit, CASC_SCOPE_CITY, bTypeMods);
	}
	//	The ACTIVE corporations' city-scope rows -- ULTSOLDIER's IS_MILITARY-filtered `units` entry. Activeness is
	//	the WALK's own gate (the sanctioned engine-owned input, [culture-religion-research.md]), which is why the
	//	authored entry carries the bare tag filter alone rather than a composed {HAS_CORPORATION} condition the
	//	tagged read would decline fail-closed.
	for (int iI = 0; iI < GC.getNumCorporationInfos(); iI++)
	{
		if (isActiveCorporation((CorporationTypes)iI))
		{
			iMultiplier += (int)InfoValuation::unitBuildRate(
				GC.getCorporationInfo((CorporationTypes)iI).getModifiers(), eUnit, CASC_SCOPE_CITY, bTypeMods);
		}
	}
	// The unit's OWN `buildRate.self.percent` -- the off-spine self scope, read exactly as the building and
	// project twins read theirs. It is NOT gated on `noNonTypeProdMods`: that skill opts out of the
	// domain/combat-class modifiers above, and this one IS the unit's own type.
	iMultiplier += kUnit.expectedModifierAt(
		MODFAM_BUILD_RATE, BUILD_RATE_AMOUNT, CASC_UNIT_PERCENT, CASC_SCOPE_SELF,
		getCityContext(), GET_PLAYER(getOwner()).getEmpireContext(), plotGroup(getOwner()));
	return iMultiplier;
}


int CvCity::getProductionModifier(BuildingTypes eBuilding) const
{
	PROFILE_EXTRA_FUNC();
	int iMultiplier = GET_PLAYER(getOwner()).getProductionModifier(eBuilding);

	// The CITY-scope rows the city's own OPERATING buildings author against this target -- "while I operate,
	// that builds faster HERE". Scope-filtered, because the EMPIRE-scope rows on those same buildings are the
	// player's answer below and would otherwise be claimed twice in the city holding the source.
	const int iBuildingsSegment = InfoValuation::keyedTargetSegment("buildings");
	const std::set<int>& kActive = m_operatingBuildings.active;
	for (std::set<int>::const_iterator it = kActive.begin(); it != kActive.end(); ++it)
	{
		const CvModifiers* pModifiers = GC.getBuildingInfo((BuildingTypes)*it).getModifiers();
		if (pModifiers != NULL)
		{
			iMultiplier += InfoValuation::keyedTarget(pModifiers, MODFAM_BUILD_RATE, -1,
				iBuildingsSegment, (int)eBuilding, (int)CASC_SCOPE_CITY);
		}
	}

	iMultiplier += GET_PLAYER(getOwner()).getBuildingProductionModifier(eBuilding);

	// The building's own `buildRate.self.percent`, which the data authors CONDITIONED on holding a bonus
	// (curate_building's COND_KEYED bonus gate). A point read serves the unconditioned sum only, so the gate is
	// resolved by the ONE evaluator against this city's contexts -- never a loop asking every bonus in the
	// registry whether this building deposits against it (the own-data inversion).
	iMultiplier += GC.getBuildingInfo(eBuilding).expectedModifierAt(
		MODFAM_BUILD_RATE, BUILD_RATE_AMOUNT, CASC_UNIT_PERCENT, CASC_SCOPE_SELF,
		getCityContext(), GET_PLAYER(getOwner()).getEmpireContext(), plotGroup(getOwner()));

	if (GET_PLAYER(getOwner()).getStateReligion() != NO_RELIGION)
	{
		if (isHasReligion(GET_PLAYER(getOwner()).getStateReligion()))
		{
			iMultiplier += GET_PLAYER(getOwner()).getStateReligionBuildingProductionModifier();
		}
	}

	return iMultiplier;
}


int CvCity::getProductionModifier(ProjectTypes eProject) const
{
	PROFILE_EXTRA_FUNC();
	int iMultiplier = GET_PLAYER(getOwner()).getProductionModifier(eProject);

	// The project's own bonus-conditioned `buildRate.self.percent`, resolved by the ONE evaluator -- the same
	// shape as the building twin above.
	iMultiplier += GC.getProjectInfo(eProject).expectedModifierAt(
		MODFAM_BUILD_RATE, BUILD_RATE_AMOUNT, CASC_UNIT_PERCENT, CASC_SCOPE_SELF,
		getCityContext(), GET_PLAYER(getOwner()).getEmpireContext(), plotGroup(getOwner()));
	return iMultiplier;
}


int CvCity::getProductionPerTurn(ProductionCalc::flags flags = ProductionCalc::Yield) const
{
	if (isDisorder())
	{
		return 0;
	}
	// The REALIZED yields in ONE read: the cascade folds the worked-plot Σ, the specialists, the percent
	// stack AND the flat tier itself (modifier.md §2a), so the hand-assembled combine that stood here is gone.
	// ⚠ Behaviour: the flat tier now rides the Yield flag with the rest of the realized value, where the retired
	// form added it even when that flag was clear. Stated, not hidden (validation.md: the spec leads).
	int aiRealizedYields[NUM_YIELD_TYPES];
	getYields(aiRealizedYields);

	// Both rates reduce HERE, where the amount plane meets whole HAMMERS and whole food
	// ([DEC-fixedpoint-x100] -- a reader reduces at its point of use).
	const int iFoodProduction = (flags & ProductionCalc::FoodProduction)
		? std::max(0, (aiRealizedYields[YIELD_FOOD] - foodConsumption(true)) / 100) : 0;
	const int iOverflow = (flags & ProductionCalc::Overflow) ? getOverflowProduction() + getFeatureProduction() : 0;
	const int iYield = (flags & ProductionCalc::Yield) ? aiRealizedYields[YIELD_PRODUCTION] / 100 : 0;

	return std::max(1, iOverflow + iFoodProduction + iYield);
}

int CvCity::getProductionDifference(const OrderData& orderData, ProductionCalc::flags flags) const
{
	const ProductionCalc::flags foodFlag = ((flags & ProductionCalc::FoodProduction) && isFoodProduction(orderData)) ? ProductionCalc::FoodProduction : ProductionCalc::None;
	const ProductionCalc::flags overflowProd = (flags & ProductionCalc::Overflow) ? ProductionCalc::Overflow : ProductionCalc::None;

	return range(getProductionPerTurn(foodFlag | overflowProd | ProductionCalc::Yield), 0, MAX_INT);
}

int CvCity::getCurrentProductionDifference(ProductionCalc::flags flags) const
{
	bst::optional<OrderData> order = getHeadOrder();
	return order ? getProductionDifference(*order, flags) : 0;
}


bool CvCity::canHurryInternal(const HurryTypes eHurry) const
{
	if (!GET_PLAYER(getOwner()).canHurry(eHurry))
	{
		return false;
	}

	if (isDisorder())
	{
		return false;
	}

	if (getPopulation() <= hurryPopulation(eHurry))
	{
		return false;
	}
	return true;
}

bool CvCity::canHurry(const HurryTypes eHurry, const bool bTestVisible) const
{
	if (!canHurryInternal(eHurry))
	{
		return false;
	}

	if (getProductionProgress() >= getProductionNeeded())
	{
		return false;
	}

	if (!bTestVisible)
	{
		if (!isProductionUnit() && !isProductionBuilding())
		{
			return false;
		}

		if (GET_PLAYER(getOwner()).getGold() < getHurryGold(eHurry))
		{
			return false;
		}

		if (maxHurryPopulation() < hurryPopulation(eHurry))
		{
			return false;
		}
	}
	return true;
}

bool CvCity::canHurryUnit(HurryTypes eHurry, UnitTypes eUnit) const
{
	if (!canHurryInternal(eHurry))
	{
		return false;
	}

	if (getProgressOnUnit(eUnit) >= getProductionNeeded(eUnit))
	{
		return false;
	}

	if (GET_PLAYER(getOwner()).getGold() < getHurryGold(eHurry, getHurryCost(eUnit)))
	{
		return false;
	}

	if (maxHurryPopulation() < getHurryPopulation(eHurry, getHurryCost(eUnit)))
	{
		return false;
	}
	return true;
}

bool CvCity::canHurryBuilding(HurryTypes eHurry, BuildingTypes eBuilding) const
{
	if (!canHurryInternal(eHurry))
	{
		return false;
	}

	if (getProgressOnBuilding(eBuilding) >= getProductionNeeded(eBuilding))
	{
		return false;
	}

	if (GET_PLAYER(getOwner()).getGold() < getHurryGold(eHurry, getHurryCost(eBuilding)))
	{
		return false;
	}

	if (maxHurryPopulation() < getHurryPopulation(eHurry, getHurryCost(eBuilding)))
	{
		return false;
	}
	return true;
}


void CvCity::hurry(HurryTypes eHurry)
{
	int64_t iHurryGold = 0;
	int iHurryPopulation = 0;
	int iHurryAngerLength = 0;
	if (!canHurry(eHurry))
	{
		return;
	}

	bool bWhip = (GC.getHurryInfo(eHurry).getProductionPerPopulation() > 0);
	bool bBuy = (GC.getHurryInfo(eHurry).getGoldPerProduction() > 0);

	if (bBuy)
	{
		iHurryGold = getHurryGold(eHurry);
		GET_PLAYER(getOwner()).changeHurriedCount(1);
		GET_PLAYER(getOwner()).changeGold(-iHurryGold);
	}

	if (bWhip)
	{
		iHurryPopulation = hurryPopulation(eHurry);
		iHurryAngerLength = hurryAngerLength(eHurry);
		changePopulation(-(iHurryPopulation));
		changeHurryAngerTimer(iHurryAngerLength);
	}

	changeProduction(hurryProduction(eHurry));

	if (isCitySelected())
	{
		gDLL->getInterfaceIFace()->setDirty(InfoPane_DIRTY_BIT, true);
	}

	// Python Event
	CvEventReporter::getInstance().cityHurry(this, eHurry);
}

// BUG - Hurry Assist - start
bool CvCity::hurryOverflow(HurryTypes eHurry, int* iProduction, int* iGold, bool bCountThisTurn) const
{
	if (!canHurry(eHurry))
	{
		return false;
	}

	if (GC.getHurryInfo(eHurry).getProductionPerPopulation() == 0)
	{
		*iProduction = 0;
		*iGold = 0;
		return true;
	}
	int iTotal, iCurrent, iGoldPercent;

	if (isProductionUnit())
	{
		const UnitTypes eUnit = getProductionUnit();
		FAssertMsg(eUnit != NO_UNIT, "eUnit is expected to be assigned a valid unit type");
		iTotal = getProductionNeeded(eUnit);
		iCurrent = getProgressOnUnit(eUnit);
		iGoldPercent = GC.getMAXED_UNIT_GOLD_PERCENT();
	}
	else if (isProductionBuilding())
	{
		const BuildingTypes eBuilding = getProductionBuilding();
		FAssertMsg(eBuilding != NO_BUILDING, "eBuilding is expected to be assigned a valid building type");
		iTotal = getProductionNeeded(eBuilding);
		iCurrent = getProgressOnBuilding(eBuilding);
		iGoldPercent = GC.getMAXED_BUILDING_GOLD_PERCENT();
	}
	else if (isProductionProject())
	{
		const ProjectTypes eProject = getProductionProject();
		FAssertMsg(eProject != NO_PROJECT, "eProject is expected to be assigned a valid project type");
		iTotal = getProductionNeeded(eProject);
		iCurrent = getProjectProduction(eProject);
		iGoldPercent = GC.getMAXED_PROJECT_GOLD_PERCENT();
	}
	else return false;

	int iOverflow = iCurrent + hurryProduction(eHurry) - iTotal;
	if (bCountThisTurn)
	{
		// include chops and previous overflow here
		iOverflow += getCurrentProductionDifference(ProductionCalc::FoodProduction | ProductionCalc::Overflow);
	}
	const int iMaxOverflow = getMaxProductionOverflow();

	*iProduction = std::min(iOverflow, iMaxOverflow);
	*iGold = std::max(0, iOverflow - iMaxOverflow) * iGoldPercent / 100;

	return true;
}
// BUG - Hurry Assist - end


UnitTypes CvCity::getConscriptUnit() const
{
	PROFILE_EXTRA_FUNC();
	UnitTypes eBestUnit = NO_UNIT;

	int iBestValue = 0;
	// The enabler hands back the finished TRAINABLE set, so there is no database scan and no verdict filter
	// (enabler.md par.6: the AI's decisions iterate ONLY the frontier).
	std::vector<int> trainableUnits;
	getAvailableUnits(trainableUnits);
	foreach_(const int iI, trainableUnits)
	{
		{
			int iValue = GC.getUnitInfo((UnitTypes) iI).getConscription();
			if (iValue > iBestValue)
			{
				iBestValue = iValue;
				eBestUnit = (UnitTypes) iI;
			}
		}
	}
	return eBestUnit;
}


int CvCity::getConscriptPopulation() const
{
	return std::max(0, GC.getCONSCRIPT_POPULATION());
}


int CvCity::flatConscriptAngerLength() const
{
	return std::max(1, GC.getCONSCRIPT_ANGER_DIVISOR() * CvGameSpeedScale::speedPercent() / 100);
}


bool CvCity::canConscript(bool bOnCapture) const
{
	if (!bOnCapture)
	{
		if (isDisorder() || isDrafted())
		{
			return false;
		}
	}

	if (getPopulation() <= getConscriptPopulation())
	{
		return false;
	}

	if (GET_PLAYER(getOwner()).getConscriptCount() >= GET_PLAYER(getOwner()).getMaxConscript())
	{
		return false;
	}

	if (!bOnCapture && plot()->calculateTeamCulturePercent(getTeam()) < GC.getCONSCRIPT_MIN_CULTURE_PERCENT())
	{
		return false;
	}

	if (getConscriptUnit() == NO_UNIT)
	{
		return false;
	}

	return true;
}

CvUnit* CvCity::initConscriptedUnit()
{
	const UnitTypes eConscriptUnit = getConscriptUnit();
	if (NO_UNIT == eConscriptUnit)
	{
		return NULL;
	}

	UnitAITypes eCityAI = NO_UNITAI;
	if (GET_PLAYER(getOwner()).AI_unitValue(eConscriptUnit, UNITAI_ATTACK, area()) > 0)
	{
		eCityAI = UNITAI_ATTACK;
	}
	else if (GET_PLAYER(getOwner()).AI_unitValue(eConscriptUnit, UNITAI_CITY_DEFENSE, area()) > 0)
	{
		eCityAI = UNITAI_CITY_DEFENSE;
	}
	else if (GET_PLAYER(getOwner()).AI_unitValue(eConscriptUnit, UNITAI_CITY_COUNTER, area()) > 0)
	{
		eCityAI = UNITAI_CITY_COUNTER;
	}
	else if (GET_PLAYER(getOwner()).AI_unitValue(eConscriptUnit, UNITAI_CITY_SPECIAL, area()) > 0)
	{
		eCityAI = UNITAI_CITY_SPECIAL;
	}
	else
	{
		eCityAI = NO_UNITAI;
	}

	CvUnit* pUnit = GET_PLAYER(getOwner()).initUnit(eConscriptUnit, getX(), getY(), eCityAI, NO_DIRECTION, GC.getGame().getSorenRandNum(10000, "AI Unit Birthmark"));
	FAssertMsg(pUnit != NULL, "pUnit expected to be assigned (not NULL)");

	if (NULL != pUnit)
	{
		addProductionExperience(pUnit, true);

		pUnit->setMoves(0);
	}

	return pUnit;
}


void CvCity::conscript(bool bOnCapture)
{
	PROFILE_EXTRA_FUNC();
	if (!canConscript(bOnCapture))
	{
		return;
	}
	const int iNumConscripts = getConscriptPopulation();
	const int iAngerLength = flatConscriptAngerLength();
	changePopulation(-1);
	changeConscriptAngerTimer(iAngerLength);

	setDrafted(true);

	if (!bOnCapture)
	{
		GET_PLAYER(getOwner()).changeConscriptCount(1);
	}

	for (int i = 0; i < iNumConscripts; i++)
	{
		CvUnit* pUnit = initConscriptedUnit();
		FAssertMsg(pUnit != NULL, "pUnit expected to be assigned (not NULL)");
	}
}


void CvCity::processBonus(BonusTypes eBonus, int iChange)
{
	// ⚖ WHAT REMAINS. This was the maintainer for the bonus-keyed accumulators -- good/bad health and
	// happiness, the yield and commerce modifiers, and the bonus-conditioned power count -- each of them a
	// WHOLE-DATABASE building scan summing a bonus-keyed building field. Those fields are compiled DEPOSITS
	// now, folded per scope by the cascade, so re-summing them city-side would be a second, drifting copy
	// ([DEC-accumulator-cut-uniform]) -- and the scans go with them.
	// The NETWORK supply presence crossing. processNumBonusChange reaches here only on a genuine 0 <-> non-zero
	// flip and always with +-1, so this is two happenings and never a magnitude ([DEC-facts-name-happenings]).
	if (iChange > 0)
	{
		emitCityBonusAdded(getID(), getOwner(), (int)eBonus);
	}
	else
	{
		emitCityBonusRemoved(getID(), getOwner(), (int)eBonus);
	}
}



// Toffer - AlphaOmega (beginning\end) is only true when this is called from setHasBuilding(...).
//	Added the extra input because I'm not sure if I can get away with changing the m_buildingLedger map 
//	after processsBuilding(..) is called from it rather than the current order which is before the call.
//	It is currently only used in the initial "sanity control" safety net.
void CvCity::processBuilding(const BuildingTypes eBuilding, const int iChange, const bool bReligiously, const bool bAlphaOmega)
{
	PROFILE_FUNC();
	FAssert(iChange == 1 || iChange == -1);

	// Toffer - Sanity control
	if (iChange == -1)
	{
		if (isDormantBuilding(eBuilding) || !bAlphaOmega && !hasBuilding(eBuilding))
		{
			FErrorMsg("Trying to process out a building that haven't been processed in! Code copes, but it shouldn't have to!");
			return;
		}
		if (isDormantBuilding(eBuilding))
		{
			if (bReligiously)
			{
				FErrorMsg("Trying to religiously process out a building that is already religiously processed out! Code copes, but it shouldn't have to!");
				return;
			}
			FErrorMsg("Trying to process out a building that is already religiously processed out! Code copes, but it shouldn't have to!");
		}
	}
	else if (bReligiously)
	{
		if (!isDormantBuilding(eBuilding))
		{
			FErrorMsg("Trying to religiously process in a building that was never religiously processed out! Code copes, but it shouldn't have to!");
			return;
		}
		if (isDormantBuilding(eBuilding))
		{
			FErrorMsg("Trying to religiously process in a building that is disabled! Code copes, but it shouldn't have to!");
			return;
		}
	}
	else if (!bAlphaOmega && isActiveBuilding(eBuilding))
	{
		FErrorMsg("Trying to process in a building that is already processed in! Code copes, but it shouldn't have to!");
		return;
	}
	CvPlayer& owner = GET_PLAYER(getOwner());

	// Process the building
	const CvBuildingInfo& kBuilding = GC.getBuildingInfo(eBuilding);

	// ⚖ WHAT THIS FUNCTION IS NOW. It used to be the maintainer for ~50 per-source accumulators, one `change*`
	// per authored building field. Those fields no longer exist on the rebuilt info -- the values are compiled
	// DEPOSITS the cascade folds per scope, so re-summing them into a city-side store would be a second,
	// drifting copy of data the packages already hold ([DEC-accumulator-cut-uniform]).
	// What legitimately remains is what is NOT a modifier deposit: the supply this building puts into the city's
	// vicinity, the engine-side counters and caps, the cross-scope fan-out, and the announcement.
	if (!bReligiously && kBuilding.hasAttribute(CLS_ATTRIBUTE_ORBITAL_INFRASTRUCTURE))
	{
		owner.noteOrbitalInfrastructureCountDirty();
	}

	// ⛔ THE SUPPLY IS NOT DRIVEN FROM HERE. This function runs on building PRESENCE, and presence is the wrong
	// axis: under the band model a building is placed once and then toggles active/dormant, so supplying from
	// here makes a DORMANT building hand the city its resources. The supply is the ENABLER's operate/provides
	// fixpoint -- it has to be, because one building's supply can satisfy another's operate condition, which
	// only the fixpoint can resolve ([enabler.md] §3.2, [json.md] §5a).
	// ⚠ Both ran, and they disagreed: the enabler's `provided` carried 173 resources for London while the plot
	// group -- fed from here -- carried 96, so an industrial farm's pig, sheep and cow were supplied according to
	// one and absent according to the other. A deposit asking whether the city held pig was answered NO while
	// the farm stood there supplying it. The enabler owns it end to end now
	// ([DEC-single-implementation]).

	changeMaxAirlift(kBuilding.getAirlift() * iChange);
	changeAirUnitCapacity(kBuilding.getAirUnitCapacity() * iChange);

	for (int iI = 0; iI < NUM_COMMERCE_TYPES; iI++)
	{
		const CommerceTypes eCommerceX = static_cast<CommerceTypes>(iI);
		// The player-side ledger legs stay: they are recompute-from-source stores the city PULLS, not
		// per-building accumulators (state-repositories.md).
		changeBuildingCommerceChange(eBuilding, eCommerceX, iChange * owner.getBuildingCommerceChange(eBuilding, eCommerceX));
		owner.invalidateCommerceRankCache();
	}

	//	The slot cap is a fresh read over the operating buildings now, so nothing accumulates here. What DOES
	//	survive is the rider the deleted changer carried ([save.md §6]: audit a changer's whole body): a building
	//	that opens or closes specialist slots moves which plots/slots are worth working, so the citizen
	//	assignment is asked to re-check. ⚑ Gated on the building ACTUALLY authoring the family -- the ruled test
	//	is "a building that makes actual changes to specialists or plots", never every completion
	//	([todo.md] the assignment re-check routing).
	if (InfoValuation::authorsAnySigned(kBuilding.getModifiers(), MODFAM_ALLOWED_SPECIALISTS, 1)
	||  InfoValuation::authorsAnySigned(kBuilding.getModifiers(), MODFAM_ALLOWED_SPECIALISTS, -1))
	{
		AI_setAssignWorkDirty(true);
	}

	if (kBuilding.providesAmenity(CLS_AMENITY_ZONE_OF_CONTROL))
	{
		changeZoCCount(iChange);
	}
	if (!bReligiously)
	{
		if (kBuilding.providesAmenity(CLS_AMENITY_PROTECTED_CULTURE))
		{
			changeProtectedCultureCount(iChange > 0 ? 1 : -1);
		}
		if (kBuilding.getWorkableRadius() > 0)
		{
			setWorkableRadiusOverride(iChange > 0 ? kBuilding.getWorkableRadius() : 0);
		}
	}

	// The cross-scope fan-out: a team-shared building processes for every team member.
	for (int iI = 0; iI < MAX_PLAYERS; iI++)
	{
		if (GET_PLAYER((PlayerTypes)iI).isAliveAndTeam(getTeam()) && (iI == getOwner() || kBuilding.hasAttribute(CLS_ATTRIBUTE_TEAM_SHARE)))
		{
			GET_PLAYER((PlayerTypes)iI).processBuilding(eBuilding, iChange, area(), bReligiously);
		}
	}
	GET_TEAM(getTeam()).processBuilding(eBuilding, iChange, bReligiously);

	if (!bReligiously)
	{
		GC.getGame().processBuilding(eBuilding, iChange);

		const SpecialBuildingTypes eSpecialBuilding = (SpecialBuildingTypes)kBuilding.getSpecialBuildingType();
		if (eSpecialBuilding != NO_SPECIALBUILDING)
		{
			owner.changeBuildingGroupCount(eSpecialBuilding, iChange);
		}
		owner.changeWondersScore(getWonderScore(eBuilding) * iChange);
	}

	m_buildingSourcedPropertyCache.clear();

	// New or removed buildings can affect the assessment of the best plot builds
	AI_markBestBuildValuesStale();

	if (!bReligiously && GC.getGame().isOption(GAMEOPTION_RELIGION_DISABLING))
	{
	}
	setLayoutDirty(true);

	// #430 event spine: announce the PROCESSED (operating-contribution) flip -- fires on construction/destruction's
	// processing leg AND on dormancy disable/enable. NOT a presence fact (those are setHasBuilding's ADDED /
	// REMOVED pair): a disable flip processed through here must never read as the building leaving the city.
	emitCityBuildingProcessed(getID(), getOwner(), (int)eBuilding, (iChange > 0) ? iChange : -iChange);
}


void CvCity::processProcess(ProcessTypes eProcess, int iChange)
{
	PROFILE_EXTRA_FUNC();
	for (int iI = 0; iI < NUM_COMMERCE_TYPES; iI++)
	{
		changeProductionToCommerceModifier(((CommerceTypes)iI), (GC.getProcessInfo(eProcess).getProductionToCommerce((CommerceTypes)iI, CASC_SCOPE_CITY) * iChange));
	}
}


void CvCity::processSpecialist(SpecialistTypes eSpecialist, int iChange)
{
	PROFILE_EXTRA_FUNC();
	// A specialist's entire output -- its yields, commerce, wellbeing, underworld stats and great-people rate --
	// is a compiled DEPOSIT the cascade folds, so nothing is accumulated city-side any more. What remains is the
	// eager updaters, which other call sites drive too and which are not this function's to own.
	if (isCitizenJuggling())
	{
		// The juggle bracket: these three WHOLE-SET recomputes ran once per probe, which is the measured
		// governor churn. They defer to endCitizenJuggling's single batch run for the whole run.
		m_bJuggleDeferredSpec = true;
	}
	else
	{
	}
}


HandicapTypes CvCity::getHandicapType() const
{
	return GET_PLAYER(getOwner()).getHandicapType();
}


CivilizationTypes CvCity::getCivilizationType() const
{
	if (m_iCiv == NO_CIVILIZATION)
	{
		return GET_PLAYER(getOwner()).getCivilizationType();
	}
	return (CivilizationTypes)m_iCiv;
}


LeaderHeadTypes CvCity::getPersonalityType() const
{
	return GET_PLAYER(getOwner()).getPersonalityType();
}


ArtStyleTypes CvCity::getArtStyleType() const
{
	if (getOriginalOwner() != NO_PLAYER)
	{
		return GET_PLAYER(getOriginalOwner()).getArtStyleType();
	}
	return GET_PLAYER(getOwner()).getArtStyleType();
}

CitySizeTypes CvCity::getCitySizeType() const
{
	return ((CitySizeTypes)(range((getPopulation() / 7), 0, (NUM_CITYSIZE_TYPES - 1))));
}

const CvArtInfoBuilding* CvCity::getBuildingArtInfo(BuildingTypes eBuilding) const
{
	return GC.getBuildingInfo(eBuilding).getArtInfo();
}

float CvCity::getBuildingVisibilityPriority(BuildingTypes eBuilding) const
{
	// The float is the EXE's: this accessor is DllExport and the closed EXE resolves it, so the return type is a
	// fixed ABI obligation while the info holds the priority as an int. The conversion is the boundary, made explicit.
	return static_cast<float>(GC.getBuildingInfo(eBuilding).getVisibilityPriority());
}

bool CvCity::hasTrait(TraitTypes eTrait) const
{
	return GET_PLAYER(getOwner()).hasTrait(eTrait);
}

bool CvCity::isNPC() const
{
	return GET_PLAYER(getOwner()).isNPC();
}

bool CvCity::isHominid() const
{
	return GET_PLAYER(getOwner()).isHominid();
}

bool CvCity::isHuman() const
{
	return GET_PLAYER(getOwner()).isHumanPlayer();
}

bool CvCity::isVisible(TeamTypes eTeam, bool bDebug) const
{
	return plot()->isVisible(eTeam, bDebug);
}

bool CvCity::isCapital() const
{
	return (GET_PLAYER(getOwner()).getCapitalCity() == this);
}

bool CvCity::isCoastal(int iMinWaterSize) const
{
	return plot()->isCoastalLand(iMinWaterSize);
}

bool CvCity::isDisorder() const
{
	return (isOccupation() || GET_PLAYER(getOwner()).isAnarchy());
}

bool CvCity::isHolyCity(ReligionTypes eIndex) const
{
	return eIndex != NO_RELIGION && GC.getGame().getHolyCity(eIndex) == this;
}

// The bare verdict reads the city's own MAINTAINED COUNT, not a walk of the religion registry asking CvGame
// once per entry. The count is fed +-1 by the holy-city fact ([contexts.md]); CvGame keeps the authoritative
// per-religion assignment, because exactly one city holds each and uniqueness is structural there.
bool CvCity::isHolyCity() const
{
	return m_cityContext.isHolyCityAny();
}


bool CvCity::isHeadquarters(CorporationTypes eIndex) const
{
	return (GC.getGame().getHeadquarters(eIndex) == this);
}

void CvCity::setHeadquarters(CorporationTypes eIndex)
{
	if (!isHeadquarters(eIndex))
	{
		GC.getGame().setHeadquarters(eIndex, this, true);
	}
}

// The corporation twin of isHolyCity() above, and the same shape: a maintained count, never a registry walk.
bool CvCity::isHeadquarters() const
{
	return m_cityContext.isHeadquartersAny();
}


int CvCity::getOvercrowdingPercentAnger(int iExtra) const
{
	if (getPopulation() + iExtra > 0)
	{
		return GC.getPERCENT_ANGER_DIVISOR();
	}
	return 0;
}


int CvCity::getNoMilitaryPercentAnger() const
{
	if (getMilitaryHappinessUnits() == 0)
	{
		return GC.getNO_MILITARY_PERCENT_ANGER();
	}
	return 0;
}


int CvCity::getCulturePercentAnger() const
{
	PROFILE_EXTRA_FUNC();
	const int64_t iTotalCulture = plot()->countTotalCulture();

	if (iTotalCulture == 0)
	{
		return 0;
	}
	int64_t iAngryCulture = 0;

	for (int iI = 0; iI < MAX_PLAYERS; iI++)
	{
		if (GET_PLAYER((PlayerTypes)iI).isAliveAndTeam(getTeam(), false))
		{
			int64_t iCulture = plot()->getCulture((PlayerTypes)iI);

			if (iCulture > 0)
			{
				if (atWar(GET_PLAYER((PlayerTypes)iI).getTeam(), getTeam()))
				{
					iCulture *= std::max(0, (GC.getAT_WAR_CULTURE_ANGER_MODIFIER() + 100));
					iCulture /= 100;
				}
				iAngryCulture += iCulture;
			}
		}
	}
	// The result is a PERCENT-anger term, bounded by the modifier -- it is the ratio that is returned, never
	// the culture, so the reduce belongs here at the point of use ([DEC-fixedpoint-x100]).
	return static_cast<int>(GC.getCULTURE_PERCENT_ANGER() * iAngryCulture / iTotalCulture);
}


int CvCity::getReligionPercentAnger() const
{
	PROFILE_EXTRA_FUNC();
	if (GC.getGame().getNumCities() == 0)
	{
		return 0;
	}

	const int religionCount = getReligionCount();
	if (religionCount == 0)
	{
		return 0;
	}

	int iCount = 0;
	for (int iI = 0; iI < MAX_PC_PLAYERS; iI++)
	{
		const CvPlayer& playerX = GET_PLAYER((PlayerTypes)iI);

		if (playerX.isAlive() && atWar(playerX.getTeam(), getTeam()))
		{
			FAssertMsg(playerX.getTeam() != getTeam(), "Player is at war with himself! :O");

			if (playerX.getStateReligion() != NO_RELIGION
			&& isHasReligion(playerX.getStateReligion()))
			{
				iCount += playerX.getHasReligionCount(playerX.getStateReligion());
			}
		}
	}
	const int iAnger = GC.getRELIGION_PERCENT_ANGER() * iCount / GC.getGame().getNumCities();

	return iAnger / religionCount;
}


int CvCity::getHurryPercentAnger(int iExtra) const
{
	if (getHurryAngerTimer() == 0)
	{
		return 0;
	}
	return 1 + (1 + (getHurryAngerTimer() - 1) / flatHurryAngerLength()) * GC.getHURRY_POP_ANGER() * GC.getPERCENT_ANGER_DIVISOR() / std::max(1, getPopulation() + iExtra);
}


int CvCity::getConscriptPercentAnger(int iExtra) const
{
	if (getConscriptAngerTimer() == 0)
	{
		return 0;
	}
	return 1 + (1 + (getConscriptAngerTimer() - 1) / flatConscriptAngerLength()) * GC.getCONSCRIPT_POP_ANGER() * GC.getPERCENT_ANGER_DIVISOR() / std::max(1, getPopulation() + iExtra);
}

int CvCity::getDefyResolutionPercentAnger(int iExtra) const
{
	if (getDefyResolutionAngerTimer() == 0)
	{
		return 0;
	}
	return 1 + (1 + (getDefyResolutionAngerTimer() - 1) / flatDefyResolutionAngerLength()) * GC.getDEFY_RESOLUTION_POP_ANGER() * GC.getPERCENT_ANGER_DIVISOR() / std::max(1, getPopulation() + iExtra);
}


int CvCity::getWarWearinessPercentAnger() const
{
	int iAnger = GET_PLAYER(getOwner()).getWarWearinessPercentAnger();

	// The CITY's own rolled scalar, which IS the roll-up over the chain this city sits under (team + empire +
	// city, modifier.md §1) -- so the empire-scope deposits are already inside it and reading the player's
	// scalars beside it would count them twice.
	int aiScalars[NUM_INFO_SCALARS];
	getScalars(aiScalars);
	iAnger *= std::max(0, (aiScalars[SCALAR_WAR_WEARINESS] + 100));
	iAnger /= 100;

	iAnger *= std::max(0, (getWarWearinessTimer() + 100));
	iAnger /= 100;

	return iAnger;
}


int CvCity::getRevRequestPercentAnger(int iExtra) const
{
	if (getRevRequestAngerTimer() == 0)
	{
		return 0;
	}

	int iAnger = GC.getHURRY_ANGER_DIVISOR();
	iAnger *= CvGameSpeedScale::speedPercent();
	iAnger /= 100;

	iAnger = std::max(1, iAnger);

	int iAngerPercent = 2 * (1 + (1 + (getRevRequestAngerTimer() - 1) / iAnger) * GC.getHURRY_POP_ANGER() * GC.getPERCENT_ANGER_DIVISOR() / std::max(1, getPopulation() + iExtra));

	return iAngerPercent;
}

int CvCity::getRevIndexPercentAnger() const
{
	const int iLocalAdjust = std::min(getLocalRevIndex() * 5, 100);

	int iAnger = (125 + iLocalAdjust) * (getRevolutionIndex() - 325) / 7500;
	if (iAnger < 1) return 0;

	iAnger = std::min(iAnger, 40);

	return iAnger * GC.getPERCENT_ANGER_DIVISOR() / 100;
}

int CvCity::getAngerPercent(const int iExtra) const
{
	return
	(
		getHurryPercentAnger(iExtra) +
		getConscriptPercentAnger(iExtra) +
		getDefyResolutionPercentAnger(iExtra) +
		getWarWearinessPercentAnger()
	);
}

int CvCity::getRevSuccessHappiness() const
{
	if (getRevSuccessTimer() == 0)
	{
		return 0;
	}

	int iHappy = GC.getHURRY_ANGER_DIVISOR();

	iHappy *= CvGameSpeedScale::speedPercent();
	iHappy /= 100;
	iHappy = std::max(1, iHappy);

	int iHappyPercent = 2 * (1 + (1 + (getRevSuccessTimer() - 1) / iHappy) * GC.getHURRY_POP_ANGER() * GC.getPERCENT_ANGER_DIVISOR() / std::max(1, getPopulation()));

	return iHappyPercent * getPopulation() / GC.getPERCENT_ANGER_DIVISOR();
}



int CvCity::getVassalHappiness() const
{
	PROFILE_EXTRA_FUNC();
	int iHappy = 0;

	for (int i = 0; i < MAX_TEAMS; i++)
	{
		if (getTeam() != i && GET_TEAM((TeamTypes)i).isVassal(getTeam()))
		{
			iHappy += GC.getVASSAL_HAPPINESS();
		}
	}
	return iHappy;
}

int CvCity::getVassalUnhappiness() const
{
	PROFILE_EXTRA_FUNC();
	int iUnhappy = 0;

	for (int i = 0; i < MAX_TEAMS; i++)
	{
		if (getTeam() != i && GET_TEAM(getTeam()).isVassal((TeamTypes)i))
		{
			iUnhappy += GC.getVASSAL_HAPPINESS();
		}
	}
	return iUnhappy;
}


// TheLadiesOgre - 20.10.2009
namespace
{
	int getCelebrityHappyClamped(const CvUnit* unit)
	{
		return std::max(0, unit->getCelebrityHappy());
	}
}
int CvCity::getCelebrityHappiness() const
{
	return algo::accumulate(plot()->units() | transformed(getCelebrityHappyClamped), 0);
}
// ! TheLadiesOgre


// ⛔ THE REALIZED WELLBEING — the city's own level, in the four-channel vocabulary.
// modifier.md §2b: wellbeing is FOUR ORDINARY CHANNELS (happiness / anger / health / unhealth) summed in
// opposing pairs, and its input list has two halves: the DEPOSIT-COMPUTED half the cascade rolls up, and the
// RAW-STATE INPUTS (runtime timers, ratios and counters no deposit produces). Both feed the channels, so a
// channel here IS the realized level and a consumer reads the pair it cares about.
//
// ⛔ THE SPLIT THAT MATTERS: getWellbeing() hands out the DEPOSITS ONLY, because that is the vocabulary a
// CANDIDATE also answers in (CvBuildingInfo::expectedWellbeing) -- "what do I HAVE" and "what do I CARRY" have
// to compose, which is the whole point of [patterns.md] § THE TWO READ ROLES. The raw-state inputs belong to
// the city alone and are folded HERE, at the level combine, exactly where the engine folds them.
//
// The ORDER is load-bearing: §2b states the unhealth population term as max(0, pop − angryPop), so the
// happiness pair must be complete before the health pair can be folded.
void CvCity::realizedWellbeing(int iExtraPopulation, int (&wellbeing)[NUM_WELLBEING_CHANNELS]) const
{
	PROFILE_FUNC();
	const CvPlayer& owner = GET_PLAYER(getOwner());

	// (1) The deposits. A negative deposit was routed to the opposing channel AT FILL, so the good/bad split the
	// per-source terms used to perform is the CHANNEL's now; re-applying it would be a second implementation.
	getWellbeing(wellbeing);

	// (2) The HAPPINESS pair's raw-state inputs. Every one is a whole-citizen count, so each is lifted ×100 to
	// meet the channels -- never the channels reduced to meet them ([DEC-fixedpoint-x100]: no getter reduces).
	wellbeing[WELLBEING_HAPPINESS] += 100 * std::max(0, getRevSuccessHappiness());
	wellbeing[WELLBEING_HAPPINESS] += 100 * std::max(0, getVassalHappiness());
	// ⚖ Unit-carried happiness is computed LIVE and added ON TOP, outside every cached sum and every percentage
	// ([DEC-unit-modifiers-on-top]) -- which is exactly why unit movement dirties no wellbeing cache.
	wellbeing[WELLBEING_HAPPINESS] += 100 * std::max(0, getMilitaryHappiness());
	wellbeing[WELLBEING_HAPPINESS] += 100 * std::max(0, getCelebrityHappiness());
	if (getHappinessTimer() > 0)
	{
		wellbeing[WELLBEING_HAPPINESS] += 100 * GC.getTEMP_HAPPY();
	}
	// The event-granted accumulators are one signed quantity that splits across the pair by SIGN (§2b: a
	// sanctioned read of genuine one-shot event state, not a ride-in).
	const int iEventGranted = getExtraHappiness() + owner.getExtraHappiness();
	wellbeing[WELLBEING_HAPPINESS] += 100 * std::max(0, iEventGranted);

	// (3) The ANGER side. The abolish gates zero it WHOLESALE -- they are hard off-switches, not modifiers
	// (§2b), so the side ceases to exist rather than being reduced.
	if (isNoUnhappiness())
	{
		wellbeing[WELLBEING_ANGER] = 0;
	}
	else
	{
		int iAngerPercent = 0;
		iAngerPercent += getOvercrowdingPercentAnger(iExtraPopulation);
		iAngerPercent += getNoMilitaryPercentAnger();
		iAngerPercent += getCulturePercentAnger();
		iAngerPercent += getReligionPercentAnger();
		iAngerPercent += getHurryPercentAnger(iExtraPopulation);
		iAngerPercent += getConscriptPercentAnger(iExtraPopulation);
		iAngerPercent += getDefyResolutionPercentAnger(iExtraPopulation);
		iAngerPercent += getWarWearinessPercentAnger();
		iAngerPercent += getRevRequestPercentAnger(iExtraPopulation);
		iAngerPercent += getRevIndexPercentAnger();
		// The truncating integer division is the engine quirk §2b says to reproduce VERBATIM, so it truncates to
		// whole citizens FIRST and the result is lifted after -- scaling first would silently smooth it away.
		wellbeing[WELLBEING_ANGER] += 100 * ((iAngerPercent * (getPopulation() + iExtraPopulation)) / GC.getPERCENT_ANGER_DIVISOR());

		wellbeing[WELLBEING_ANGER] += 100 * std::max(0, getVassalUnhappiness());
		wellbeing[WELLBEING_ANGER] += 100 * std::max(0, getEspionageHappinessCounter());
		wellbeing[WELLBEING_ANGER] += 100 * std::max(0, owner.calculateTaxRateUnhappiness());
		wellbeing[WELLBEING_ANGER] += 100 * std::max(0, getEventAnger());
		wellbeing[WELLBEING_ANGER] -= 100 * std::min(0, iEventGranted);
	}
	if (GC.getGame().isOption(GAMEOPTION_MAP_PERSONALIZED))
	{
		// The landmark terms are one signed quantity too: the happiness half applies whatever the anger gate did.
		wellbeing[WELLBEING_HAPPINESS] += 100 * std::max(0, owner.getLandmarkHappiness());
		if (wellbeing[WELLBEING_ANGER] != 0 || !isNoUnhappiness())
		{
			wellbeing[WELLBEING_ANGER] -= 100 * std::min(0, owner.getLandmarkHappiness());
			if (!owner.isNoLandmarkAnger())
			{
				wellbeing[WELLBEING_ANGER] += 100 * std::max(0, getLandmarkAnger());
			}
		}
	}

	// (4) The HEALTH pair's raw-state inputs -- the event-granted accumulator splitting by sign as above, the
	// espionage counter, and the POPULATION term, which §2b states as max(0, pop − angryPop). That term is why
	// this pair is folded second: it reads the happiness pair's completed verdict.
	const int iEventGrantedHealth = getExtraHealth() + GET_PLAYER(getOwner()).getExtraHealth();
	wellbeing[WELLBEING_HEALTH] += 100 * std::max(0, iEventGrantedHealth);
	wellbeing[WELLBEING_UNHEALTH] -= 100 * std::min(0, iEventGrantedHealth);
	wellbeing[WELLBEING_UNHEALTH] += 100 * std::max(0, getEspionageHealthCounter());

	if (!isNoUnhealthyPopulation())
	{
		const int iAngry = range((wellbeing[WELLBEING_ANGER] - wellbeing[WELLBEING_HAPPINESS]) / 100,
			0, getPopulation() + iExtraPopulation);
		wellbeing[WELLBEING_UNHEALTH] += 100 * std::max(0, getPopulation() + iExtraPopulation - iAngry);
	}
}


int CvCity::netHappiness(int iExtraPopulation) const
{
	int aWellbeing[NUM_WELLBEING_CHANNELS];
	realizedWellbeing(iExtraPopulation, aWellbeing);
	return InfoValuation::netHappiness(aWellbeing);
}


int CvCity::netHealth(int iExtraPopulation) const
{
	int aWellbeing[NUM_WELLBEING_CHANNELS];
	realizedWellbeing(iExtraPopulation, aWellbeing);
	return InfoValuation::netHealth(aWellbeing);
}


int CvCity::angryPopulation(int iExtra) const
{
	PROFILE("CvCityAI::angryPopulation");

	// modifier.md §2b: angryPopulation = clamp(anger − happiness, 0, pop) -- the DEFICIT side of the net,
	// clamped. A final-state calculation over the channels, not a channel of its own ([patterns.md] rule 6).
	return range(-netHappiness(iExtra), 0, getPopulation() + iExtra);
}


int CvCity::visiblePopulation() const
{
	// Only PLOTS and SPECIALISTS can be staffed by population, so pop − angry IS the employed set and the two
	// staffed kinds partition it: what is left after the tile-workers is the specialist pool.
	return getPopulation() - angryPopulation() - getWorkingPopulation();
}


int CvCity::totalFreeSpecialists() const
{
	PROFILE_EXTRA_FUNC();
	if (getPopulation() < 1)
	{
		return 0;
	}
	// Toffer - Negative free specialist effectively reduce pop of city...
	//	That's not an intended effect of the free specialist feature.
	return std::max(0, getFreeSpecialist());
}


int CvCity::extraPopulation() const
{
	return visiblePopulation() + std::min(0, extraFreeSpecialists());
}


int CvCity::extraSpecialists() const
{
	return visiblePopulation() + extraFreeSpecialists();
}


int CvCity::extraFreeSpecialists() const
{
	return totalFreeSpecialists() - getSpecialistPopulation();
}


int CvCity::unhealthyPopulation(int iExtra) const
{
	// modifier.md §2b states this term as max(0, pop − angryPop). The legacy bNoAngry flag that made the
	// subtraction optional is GONE: an ignore-this-clause flag is the exact shape the rebuild refuses to carry
	// into the new surface (roadmap § Context), and the spec leads where spec and code disagree.
	// ⚠ BEHAVIOUR CHANGE, stated rather than deferred: the legacy default passed bNoAngry=false, i.e. it did
	// NOT subtract angry citizens, so an unhappy city now counts fewer unhealthy citizens than it used to.
	if (isNoUnhealthyPopulation())
	{
		return 0;
	}
	return std::max(0, getPopulation() + iExtra - angryPopulation(iExtra));
}


// The unhealth the city's BUILDINGS contribute -- the one slice the realized group read cannot answer, since a
// group read hands back the channel TOTAL over every source. The per-building term is already the cascade read
// (getBuildingBadHealth resolves it through expectedWellbeing), so this sums those rather than a second
// derivation ([DEC-single-implementation]). ⚑ It walks the OPERATING set: a dormant building deposits nothing
// (enabler.md §3.2). The area leg went with the area scope and the player leg with the empire accumulator -- an
// empire-scope deposit rolls DOWN into each building's resolved value rather than being summed beside it.
// ⚠ Per-DECISION cadence only (the AI what-if deltas); it is not a read path.
int CvCity::totalBadBuildingHealth() const
{
	PROFILE_EXTRA_FUNC();
	if (isBuildingOnlyHealthy())
	{
		return 0;
	}
	int iBadHealth = 0;
	foreach_(const BuildingTypes eBuilding, getHasBuildings())
	{
		if (hasFullyActiveBuilding(eBuilding))
		{
			iBadHealth += getBuildingBadHealth(eBuilding);
		}
	}
	return iBadHealth + std::min(0, calculatePopulationHealth());
}


int CvCity::healthRate(int iExtra) const
{
	// modifier.md §2b: healthRate = min(0, health − unhealth) -- the DEFICIT side of the net. Whole health
	// points; the net already reduced at the discrete boundary.
	return std::min(0, netHealth(iExtra));
}


// Toffer - Gradual food consumption change
//	Food consumption should gradually increment, or decrement, during the growth process so that
// 	a pop growth, or decline, doesn't entail an abrumpt jump in food consumption from one turn to the other.
int CvCity::getPopulationPlusProgress(const int iExtra) const
{
	if (iExtra == 0)
	{
		return 100 * getPopulation() + 100 * getFood() / growthThreshold();
	}
	return 100 * (getPopulation() + iExtra) + getFoodKeptPercent();
}

int CvCity::getFoodConsumedPerPopulation(const int iExtra) const
{
	// Treat pop 1.00 or less as zero in this regard.
	const int iPop100 = getPopulationPlusProgress(iExtra) - 100;
	if (iPop100 <= 0)
	{
		return 100 * GC.getFOOD_CONSUMPTION_PER_POPULATION();
	}
	// The above is only strictly needed in case iPop100 becomes a negative value due to negative iExtra value input.
	//	Negative iExtra value is currently never passed to this function, so it's a future proof thing.
	return 100 * GC.getFOOD_CONSUMPTION_PER_POPULATION() + (iPop100 - 100) * GC.getFOOD_CONSUMPTION_PER_POPULATION_PERCENT() / 100;
}

// ⚖ THE FOOD PLANE IS x100 NATIVE, END TO END ([DEC-fixedpoint-x100]: no reduce anywhere but an edge). The whole
// chain -- the yield rate, consumption, the surplus -- speaks in x100, and there are exactly THREE edges out of
// it: the WAREHOUSE deposit (changeFood, the food BAR is a whole-unit ledger -- north-star.md's warehouse
// carve-out, the same edge the great-people progress uses), the WASTAGE table's whole-surplus INDEX, and the UI.
// ⛔ The DISCRETE operands are lifted to meet the rate, never the reverse: angry citizens and health points are
// whole COUNTS, so they convert AT THIS EDGE rather than dragging the rate down to them. The earlier shape did
// the opposite -- it manufactured x100 on both operands inside getFoodConsumedByPopulation and undid both with a
// single /10000 -- which is a calculation scaling its own inputs, the defect [DEC-fixedpoint-x100] names.
int CvCity::getFoodConsumedByPopulation(const int iExtra) const
{
	// TWO x100 operands multiplied is x10000, so the rescale belongs AT THE MULTIPLY and lands x100
	// ([fixed-point-and-scales.md] § 1: never multiply two x100 values without rescaling).
	return getPopulationPlusProgress(iExtra) * getFoodConsumedPerPopulation() / 100;
}

int CvCity::foodConsumption(const bool bNoAngry, const int iExtra, const bool bIncludeWastage) const
{
	return getFoodConsumedByPopulation(iExtra)
		// ⚠ ANGRY CITIZENS are a WHOLE count and LIFT to the x100 food plane; the HEALTH RATE is NOT -- it comes
		// off the wellbeing package (realizedWellbeing -> InfoValuation::netHealth), which is x100 NATIVE like
		// every other channel ([DEC-fixedpoint-x100]). Lifting it again multiplied the health term by 10000 and
		// inflated consumption by 100x the deficit for any unhealthy city -- silently, since the number stayed
		// plausible. The legacy twin genuinely WAS whole (min(0, goodHealth() - badHealth())), which is what made
		// the wrong lift look right.
		- (bNoAngry ? 100 * angryPopulation(iExtra) : 0) // Doesn't belong here, should be extracted out to wherever it is needed
		- healthRate(iExtra)
		+ (bIncludeWastage ? 100 * (int)foodWastage() : 0);
}
// ! Toffer - Gradual food consumption change


// Included by Thunderbrd 6/8/2019, code contributed by Sorcdk
float CvCity::foodWastage(int surplass) const
{
#define MAX_SURPLASS    200
	static float calculatedWaste[MAX_SURPLASS];
	static int calculatedTo = -1;
	const int startWasteAtConsumptionPercent = GC.getWASTAGE_START_CONSUMPTION_PERCENT();
	float wastageGrowthFactor = GC.getWASTAGE_GROWTH_FACTOR();

	if (wastageGrowthFactor == 0)
	{
		wastageGrowthFactor = (float)0.05; // default
	}

	if (startWasteAtConsumptionPercent >= 0)
	{
		if (surplass == -1)
		{
			// AN EDGE: the memo table below is indexed by WHOLE surplus (`calculatedWaste[surplass]`, 0..200), so
			// the x100 food plane reduces to a whole index exactly here and nowhere else in this body.
			surplass = (foodDifference(true, false) - getFoodConsumedByPopulation() * startWasteAtConsumptionPercent / 100) / 100;
		}
	}
	else surplass = -1;

	// Nothing wasted if there is no surplass
	if (surplass <= 0)
	{
		return 0;
	}
	// Cache what we can as it's not a trivially cheap computation
	else if (surplass <= calculatedTo)
	{
		return calculatedWaste[surplass];
	}
	else if (surplass >= MAX_SURPLASS)
	{
		// After the max we shift to from assymtotic behavior toward the limit to the limit of efficiency
		return foodWastage(MAX_SURPLASS - 1) + (foodWastage(MAX_SURPLASS - 1) - foodWastage(MAX_SURPLASS - 2)) * (surplass - MAX_SURPLASS + 1);
	}
	else
	{
		calculatedWaste[surplass] = foodWastage(surplass - 1) + (float)1.0 - (wastageGrowthFactor + ((float)1.0 - wastageGrowthFactor) / ((float)1.0 + (float)0.05 * (float)surplass));
		calculatedTo = surplass;

		return calculatedWaste[surplass];
	}
}


int CvCity::foodDifference(const bool bBottom, const bool bIncludeWastage, const bool bIgnoreFoodBuildOrRev) const
{
	if (!bIgnoreFoodBuildOrRev && isDisorder())
	{
		return 0;
	}
	// x100 THROUGHOUT -- the realized yield enters as it is and the surplus stays on the same scale as the rate
	// it came from. The reduce lives at the three edges named on foodConsumption (the warehouse deposit, the
	// wastage index, the UI), never here.
	int aiYields[NUM_YIELD_TYPES];
	getYields(aiYields);
	const int iFoodRate = aiYields[YIELD_FOOD];

	int iDifference;

	if (!bIgnoreFoodBuildOrRev && isFoodProduction())
	{
		iDifference = std::min(0, iFoodRate - foodConsumption(false, 0, bIncludeWastage));
	}
	else iDifference = iFoodRate - foodConsumption(false, 0, bIncludeWastage);

	if (bBottom && getPopulation() == 1 && getFood() == 0)
	{
		iDifference = std::max(0, iDifference);
	}

	return iDifference;
}


int CvCity::growthThreshold(const int iPopChange) const
{
	const int iThreshold = (
		getModifiedIntValue(
			GET_PLAYER(getOwner()).getGrowthThreshold(getPopulation() + iPopChange),
			getPopulationgrowthratepercentage() + GET_PLAYER(getOwner()).getPopulationgrowthratepercentage()
		)
	);
	if (isHominid())
	{
		return std::max(1, iThreshold / 2); // Those barbarians are just so damned fecund!
	}
	return std::max(1, iThreshold);
}


int CvCity::productionLeft() const
{
	return getProductionNeeded() - getProductionProgress();
}

int CvCity::getHurryCostModifier() const
{
	bst::optional<OrderData> order = getHeadOrder();

	if (order)
	{
		switch (order->eOrderType)
		{
		case ORDER_TRAIN:
			return getHurryCostModifier(order->getUnitType());
		case ORDER_CONSTRUCT:
			return getHurryCostModifier(order->getBuildingType());
		case ORDER_CREATE:
		case ORDER_MAINTAIN:
		case ORDER_LIST:
			break;

		default:
			FErrorMsg("order->eOrderType did not match a valid option");
			break;
		}
	}
	return 100;
}

int CvCity::getHurryCostModifier(UnitTypes eUnit) const
{
	return getHurryCostModifier(GC.getUnitInfo(eUnit).getHurryCostModifier(), getProductionModifier(eUnit));
}

int CvCity::getHurryCostModifier(BuildingTypes eBuilding) const
{
	return getHurryCostModifier(GC.getBuildingInfo(eBuilding).getCostsModifier(COSTS_HURRY, CASC_SCOPE_CITY), getProductionModifier(eBuilding));
}

int CvCity::getHurryCostModifier(int iBaseModifier, int iExtraMod) const
{
	int iModifier = 100 + iBaseModifier;

	if (iExtraMod != 0)
	{
		iModifier = getModifiedIntValue(iModifier, iExtraMod);
	}
	int aiCostKinds[NUM_COSTS_KINDS];
	GET_PLAYER(getOwner()).getCostKinds(aiCostKinds);
	iModifier = getModifiedIntValue(iModifier, aiCostKinds[COSTS_HURRY]);
	iModifier = getModifiedIntValue(iModifier, GET_PLAYER(getOwner()).getHurryCostModifier());

	return std::max(1, iModifier); // Avoid potential divide by 0s
}


int CvCity::hurryCost() const
{
	return getHurryCost(productionLeft(), getHurryCostModifier());
}

int CvCity::getHurryCost(UnitTypes eUnit) const
{
	return getHurryCost(getProductionNeeded(eUnit) - getProgressOnUnit(eUnit), getHurryCostModifier(eUnit));
}

int CvCity::getHurryCost(BuildingTypes eBuilding) const
{
	return getHurryCost(getProductionNeeded(eBuilding) - getProgressOnBuilding(eBuilding), getHurryCostModifier(eBuilding));
}

int CvCity::getHurryCost(int iProductionLeft, int iHurryModifier) const
{
	return std::max(0, (iProductionLeft * iHurryModifier + 99) / 100); // round up
}

int64_t CvCity::getHurryGold(const HurryTypes eHurry, int iHurryCost) const
{
	if (GC.getHurryInfo(eHurry).getGoldPerProduction() == 0)
	{
		return 0;
	}
	if (iHurryCost < 0)
	{
		iHurryCost = hurryCost();
	}
	int64_t iGold = iHurryCost * GC.getHurryInfo(eHurry).getGoldPerProduction();

	FAssert(iGold <= 2000000000); // We'll need to take measures if this comes up

	return std::max<int64_t>(1, iGold);
}


int CvCity::hurryPopulation(HurryTypes eHurry) const
{
	return getHurryPopulation(eHurry, hurryCost());
}

int CvCity::getHurryPopulation(HurryTypes eHurry, int iHurryCost) const
{
	const int prodPerPop = GC.getGame().getProductionPerPopulation(eHurry);
	if (prodPerPop < 1)
	{
		return 0;
	}
	return std::max(1, 1 + (iHurryCost - 1) / prodPerPop);
}

int CvCity::hurryProduction(HurryTypes eHurry) const
{
	const int prodPerPop = GC.getGame().getProductionPerPopulation(eHurry);
	if (prodPerPop > 0)
	{
		const int iProduction = 100 * hurryPopulation(eHurry) * prodPerPop / std::max(1, getHurryCostModifier());
		FAssert(iProduction >= productionLeft());
		return iProduction;
	}
	return productionLeft();
}


int CvCity::flatHurryAngerLength() const
{
	int iAnger = GC.getHURRY_ANGER_DIVISOR();
	iAnger *= CvGameSpeedScale::speedPercent();
	iAnger /= 100;
	iAnger *= std::max(0, 100 + getHurryAngerModifier());
	iAnger /= 100;

	return std::max(1, iAnger);
}


int CvCity::hurryAngerLength(HurryTypes eHurry) const
{
	return GC.getHurryInfo(eHurry).causesAnger() ? flatHurryAngerLength() : 0;
}


int CvCity::maxHurryPopulation() const
{
	return (getPopulation() / 2);
}


// We enter realistic culture here
int CvCity::cultureDistance(const CvPlot& plot) const
{
	// Entry point for realistic culture. Tries to use cached value if possible,
	// if not, must recalc everything in range. Not sure if cache is stored separately
	// for each city or just locally for the function whenever called?
	// In either case need to recompute cache each turn because many things can change distance.
	PROFILE_FUNC();

	if (GC.getGame().isOption(GAMEOPTION_CULTURE_REALISTIC_SPREAD))
	{
		std::map<const CvPlot*, int>::const_iterator itr = m_aCultureDistances.find(&plot);

		if (itr == m_aCultureDistances.end())
		{
			// Need culture level, not coordinate, because maybe shortest route
			// is in a loop of distance greater than coord offset
			recalculateCultureDistances(std::max(0, (int)getCultureLevel()));

			return m_aCultureDistances[&plot];
		}
		else
		{
			return itr->second;
		}
	}
	return plotDistance(getX(), getY(), plot.getX(), plot.getY());
}

void CvCity::recalculateCultureDistances(int iMaxDistance) const
{
	// This function does some special checks for tiles adjacent to city,
	// then (inefficiently, atm) calls calculateCultureDistance to compute specific tiles
	PROFILE_FUNC();

	// MCB w/o 1TF makes 8 adjacent tiles dist 1 ('soft' RCS)
	if (GC.getGame().isOption(GAMEOPTION_CULTURE_MIN_CITY_BORDER) && !GC.getGame().isOption(GAMEOPTION_CULTURE_1_CITY_TILE_FOUNDING))
	{
		foreach_(const CvPlot* plotX, plot()->adjacent())
		{
			m_aCultureDistances[plotX] = 1;
		}
	}
	// City tile itself should always have dist 0
	m_aCultureDistances[plot()] = 0;

	// Blaze: Spiraling outward from center (style of getCityIndexPlot) is more efficient if perf issues exist;
	// 	this implementation is rather brute-force and inefficient. Will need to make edits to existing func tho,
	// 	because getCityIndexPlot(0) == getCityIndexPlot(37), and we'll need a few more plots than that...
	// 	Maybe pass an optional bool arg telling it not to take the modulus or however it loops? Also plots() func, need look into.

	// Currently: Calculate distance values of all tiles in iMaxDistance radial size grid,
	//   recalculating entire grid until no values have changed. This happens ~iMaxDistance times per city per turn.
	bool bHasChanged = iMaxDistance > 0;

	int numLoops = 0;
	// as long as there are changes during the last iteration
	while (bHasChanged)
	{
		// reset the has changed variable to note a new loop cycle has begun
		bHasChanged = false;

		foreach_(const CvPlot* plotX, plot()->rect(iMaxDistance, iMaxDistance))
		{
			// This is a slightly cursed function.
			// Center should never be calculated:
			if (plotX == plot()) continue;
			// MCB w/o 1TF makes adjacent 8 always distance 1; should not be custom calculated
			if (GC.getGame().isOption(GAMEOPTION_CULTURE_MIN_CITY_BORDER) && !GC.getGame().isOption(GAMEOPTION_CULTURE_1_CITY_TILE_FOUNDING))
			{
				bool bCityAdjacent = false;
				foreach_(const CvPlot* plotAdjacent, plotX->adjacent())
				{
					if (plotAdjacent == plot())
					{
						bCityAdjacent = true;
						break;
					}
				}
				if (bCityAdjacent) continue;
			}

			// recalculate the value to determine if it has changed
			const int iNewValue = calculateCultureDistance(plotX, iMaxDistance);

			// if it has changed, save the value and mark that
			//   ALL values (!) should be recomputed since they
			//   may depend on this value
			if (m_aCultureDistances[plotX] != iNewValue)
			{
				m_aCultureDistances[plotX] = iNewValue;
				bHasChanged = true;
			}
		}
		numLoops++;
		if (numLoops > 100)
		{
			bHasChanged = false;
			FAssertMsg(numLoops < 100, "Realistic Culture hit infinte loop trying to update distances! Bypassing...");
		}
	}
}

int CvCity::calculateCultureDistance(const CvPlot* mainPlot, int iMaxDistance) const
{
	// This function is the meat of Realistic Culture, determines extra distance/cost for tiles.
	// Each point of distance corresponds 1:1 with level of city culture to reach it.
	PROFILE_FUNC();

	// if the plot distance is greater than the maximum desired plot distance
	// or if the plot does not exist, then the plot distance is maximal
	if (!mainPlot || plotDistance(getX(), getY(), mainPlot->getX(), mainPlot->getY()) > iMaxDistance)
		return MAX_INT;

	// Potentially don't need to calculate anything if plot has no already calculated neighbors
	const CvTeam& kTeam = GET_TEAM(getTeam());
	const bool bIsWater = mainPlot->isWater();
	const bool bIsCity = mainPlot->isCity();
	const bool bIsBonus = mainPlot->getBonusType(getTeam()) != NO_BONUS;
	const int terrainType = mainPlot->getTerrainType();
	const int featureType = mainPlot->getFeatureType();
	const int improvementType = mainPlot->getImprovementType();
	const int routeType = mainPlot->getRouteType();
	const int owner = mainPlot->getOwner();

// C++03 compatible iteration
bool bHasCalculatedNeighbors = false;
std::vector<std::pair<const CvPlot*, int> > validNeighbors; // Note the space between > >

CvPlot::adjacent_range adjacentPlots = mainPlot->cardinalDirectionAdjacent();
for (CvPlot::adjacent_range::const_iterator it = adjacentPlots.begin(); it != adjacentPlots.end(); ++it)
{
	const CvPlot* adjacentPlot = *it;
	int neighborDist = m_aCultureDistances[adjacentPlot];
	if (neighborDist != MAX_INT && (neighborDist != 0 || adjacentPlot == plot()))
	{
		bHasCalculatedNeighbors = true;
		validNeighbors.push_back(std::make_pair(adjacentPlot, neighborDist));
	}
}

if (!bHasCalculatedNeighbors) return MAX_INT;

	// Terrain/feature/route calculations
	int terrainDistance = 0;
	// Distance from ground tile type (peak or specific terrain)
	if (mainPlot->isAsPeak())
	{
		terrainDistance += GC.getTerrainInfo(GC.getTERRAIN_PEAK()).getScalar(SCALAR_CULTURE_DISTANCE, CASC_SCOPE_PLOT, CASC_UNIT_FLAT) / 100;
		terrainDistance += !kTeam.isMoveFastPeaks()
			+ !kTeam.isCanPassPeaks()
			+ !kTeam.isCanFoundOnPeaks();
	}
	else
	{
		terrainDistance += GC.getTerrainInfo((TerrainTypes)terrainType).getScalar(SCALAR_CULTURE_DISTANCE, CASC_SCOPE_PLOT, CASC_UNIT_FLAT) / 100;
		// Terrain distance increased if can't trade on water terrain, can't see far (optics)
		if (bIsWater)
		{
			if (!kTeam.isTerrainTrade((TerrainTypes)terrainType)) terrainDistance += 2;
			if (!mainPlot->isAdjacentToLand() && !kTeam.isExtraWaterSeeFrom()) terrainDistance += 1;
		}
		else
		{
			// Freshwater penalty acts as an inhibitor; effectively reduced as farms spread irrigation,
			// removed at bAllowsDesertFarming (refrigeration)
			if (!mainPlot->isFreshWater() && !kTeam.isCanFarmDesert()) terrainDistance += 1;
			if (mainPlot->isHills())
			{
				terrainDistance += GC.getTerrainInfo(GC.getTERRAIN_HILL()).getScalar(SCALAR_CULTURE_DISTANCE, CASC_SCOPE_PLOT, CASC_UNIT_FLAT) / 100
					+ !kTeam.isCanFoundOnPeaks()
					+ !kTeam.isBridgeBuilding();
			}
		}
	}

	// Distance from features
	if (featureType != NO_FEATURE)
	{
		// some features cause underlying terrain cost to be ignored; oasis, floodplain, natural wonders
		const CvFeatureInfo& featureInfo = GC.getFeatureInfo((FeatureTypes)featureType);
		if (featureInfo.hasCharacteristic(CLS_CHARACTERISTIC_IGNORE_TERRAIN_CULTURE))
		{
			terrainDistance = 0;
		}
		// penalty for unimproved features
		else if (improvementType == NO_IMPROVEMENT)
		{
			terrainDistance += 1;
		}
		// penalty for improved features if outside city affected territory
		else if (!mainPlot->isInCultureRangeOfCityByPlayer((PlayerTypes)owner)
	 || !mainPlot->isInCultureRangeOfCityByPlayer(getOwner()))
		{
			terrainDistance += 1;
		}
		// Always add base feature cost
		terrainDistance += featureInfo.getScalar(SCALAR_CULTURE_DISTANCE, CASC_SCOPE_PLOT, CASC_UNIT_FLAT) / 100;
	}

	// Route tier modifier
	// Using route value/tier as softer land tile inhibitor by era
	// Base distance (x): 0, 1, 2, 3, 4 ... translates to:
	// Tier 0 (noroute):  0, 1, 3, 5, 7 ... // 2x-1: Always applies to tiles not yet influenced
	// Tier 1 (trail):	  0, 1, 3, 4, 6 ... // 3x/2
	// Tier 2 (path):	  0, 1, 2, 4, 5 ... // 4x/3
	// Tier 3 (road):	  0, 1, 2, 3, 5 ... // 5x/4
	if (!bIsWater && !bIsBonus)
	{
		int routeTierMod = 0;
		if (routeType != NO_ROUTE &&
			(mainPlot->isInCultureRangeOfCityByPlayer((PlayerTypes)owner) ||
				mainPlot->isInCultureRangeOfCityByPlayer(getOwner())))
		{
			routeTierMod = std::max(routeTierMod, GC.getRouteInfo((RouteTypes)routeType).getValue());
		}
		// Penalties applied for low tier routes (pre-paved road)
		if (routeTierMod == 0)
		{
			terrainDistance = std::max(0, 1 + 2 * (terrainDistance - 1));
		}
		else if (routeTierMod < 4)
		{
			terrainDistance = terrainDistance * (routeTierMod + 2) / (routeTierMod + 1);
		}
	}

	// Halve terrain distance if bonus is present
	if (bIsBonus) terrainDistance /= 2;

	// Final neighbor loop
	/* Determine the final cultural distance of given plot:
		1: All directions from given plot are checked (could come from weird direction)
		2: Neighbors with distance of MAX_INT are ignored because they
			don't exist or haven't been calculated yet
		3: Smallest total possible distance is used from all neighbors
		4: Greater river penalty for lacking river trade, bridge building
		5: City tiles are easier to influence, even across river */
	int distance = MAX_INT;
	const int extraRiverPenalty = !kTeam.isBridgeBuilding() + !kTeam.isRiverTrade();

	for (std::vector<std::pair<const CvPlot*, int> >::const_iterator it = validNeighbors.begin(); it != validNeighbors.end(); ++it)
	{
		// An uncalculated tile will either have MAX_INT (no neighbors on prev calc) or 0 (1st iteration).
		// 0 may be used if it's the city tile itself, though.
		const CvPlot* adjacentPlot = it->first;
		int neighborDist = it->second;
		int netDistanceModifier = terrainDistance;
		// If we are adjacent to our own city center, different rules. Cheaper, but not straight 0 cost.

		if (neighborDist == 0)
		{
			netDistanceModifier += mainPlot->isRiverCrossing(directionXY(mainPlot, adjacentPlot)) * (extraRiverPenalty);
			netDistanceModifier /= 3;
		}
		else
		{
			netDistanceModifier += mainPlot->isRiverCrossing(directionXY(mainPlot, adjacentPlot)) * (1 + extraRiverPenalty) * 2;
		}

		// Other city tiles are easier to influence even if across river (city itself crosses river)
		netDistanceModifier /= (1 + bIsCity);

		int totalDist = neighborDist + 1 + std::max(0, netDistanceModifier);
		if (totalDist < distance) distance = totalDist;
	}
	return distance;
}



void CvCity::clearCultureDistanceCache()
{
	m_aCultureDistances.clear();
}
// End realistic culture


int CvCity::netRevoltRisk(PlayerTypes cultureAttacker) const
{
	// Returns 100x % chance of revolt to eCultureAttacker when modified by defending units
	// 108 = 1.08%, 9,876 = 98.76%
	return std::min(10000, std::max(0, baseRevoltRisk(cultureAttacker) * (unitRevoltRiskModifier(cultureAttacker))) / 100);
}


int CvCity::baseRevoltRisk(PlayerTypes eCultureAttacker) const
{
	PROFILE_EXTRA_FUNC();
	// Returns 100x% chance of revolt to eCultureAttacker unmodified by defending units
	// Should probably be less dependent on era or pop.
	int iRisk = (getHighestPopulation() * 2);

	// Presence of 3rd party culture lowers max bonus
	const int iAttackerPercent100 = plot()->calculateCulturePercent(eCultureAttacker, 2);
	int iDefenderPercent100 = plot()->calculateCulturePercent(getOwner(), 2);
	// Adjust defender percent by possible fixed border modifier
	// (otherwise inflated risk when FB city is threatened)
	if (GET_PLAYER(getOwner()).hasFixedBorders())
	{
		iDefenderPercent100 *= (GC.getDefineINT("FIXED_BORDERS_CULTURE_RATIO_PERCENT"));
		iDefenderPercent100 /= 100;
	}

	// If adjacent tiles can be acquired, factor in, else there's an additional min risk
	if (!GC.getGame().isOption(GAMEOPTION_CULTURE_MIN_CITY_BORDER))
	{
		foreach_(const CvPlot* pLoopPlot, plot()->adjacent()
		| filtered(CvPlot::fn::getOwner() == eCultureAttacker))
		{
			iRisk += (GC.getGame().getCurrentEra() + 1);
		}
	}
	else iRisk += (GC.getGame().getCurrentEra() + 1);
	// iRisk is currently something like 10 aka 10%

	// Ranges from 10000 to 1,000,000 as attacker:defender culture % ratio goes from 1:1 to 1:0.01
	// Nonlinear!
	const int iCultureRatioModifier = 10000 * iAttackerPercent100 / std::max(1, iDefenderPercent100);
	// XML to make this even stronker (default 200 doubles *only mod part* of above modifier)
	iRisk *= ((iCultureRatioModifier - 10000) * (GC.getREVOLT_TOTAL_CULTURE_MODIFIER()) / 100 + 10000);
	iRisk /= 100;
	// iRisk is now measured in x100 here aka 505 or 2345 meaning 5.05% or 23.45%.

	// By default (100), attacker having a state religion doubles attacking power
	if (GET_PLAYER(eCultureAttacker).getStateReligion() != NO_RELIGION)
	{
		if (isHasReligion(GET_PLAYER(eCultureAttacker).getStateReligion()))
		{
			iRisk *= std::max(0, (GC.getREVOLT_OFFENSE_STATE_RELIGION_MODIFIER() + 100));
			iRisk /= 100;
		}
	}

	// By default (-50), defender having state religion halves attacker's power
	if (GET_PLAYER(getOwner()).getStateReligion() != NO_RELIGION)
	{
		if (isHasReligion(GET_PLAYER(getOwner()).getStateReligion()))
		{
			iRisk *= std::max(0, (GC.getREVOLT_DEFENSE_STATE_RELIGION_MODIFIER() + 100));
			iRisk /= 100;
		}
	}
	return iRisk;
}


int CvCity::unitRevoltRiskModifier(PlayerTypes eCultureAttacker) const
{
	PROFILE_EXTRA_FUNC();
	// constructed from icultureGarrison of stationed units
	// returns percent modifier on revolt risk due to units
	int iGarrison = 0;

	foreach_ (const CvUnit * unit, plot()->units())
		iGarrison += unit->revoltProtectionTotal();

	// Blaze: This also doubles negative impact of criminal units while at war. Intended? Fix?
	if (atWar(GET_PLAYER(eCultureAttacker).getTeam(), getTeam()))
		iGarrison *= 2;

	// Negative revolt protection increases multiplier (-5% revolt protection -> 105% multiplier)
	if (iGarrison < 0) return 100 - iGarrison;
	// Positive revolt protection has diminishing returns
	// 100% protection -> 50% multiplier, 200% protection -> 33% multiplier
	return (10000 / (100 + iGarrison));
}


bool CvCity::hasActiveWorldWonder() const
{
	PROFILE_EXTRA_FUNC();
	foreach_(const BuildingTypes eType, getHasBuildings())
	{
		if (isWorldWonder(eType) && !isDormantBuilding(eType))
		{
			return true;
		}
	}
	return false;
}


int CvCity::getNumActiveWorldWonders() const
{
	PROFILE_EXTRA_FUNC();
	int iCount = 0;
	foreach_(const BuildingTypes eType, getHasBuildings())
	{
		if (isWorldWonder(eType) && !isDormantBuilding(eType))
		{
			iCount++;
		}
	}
	return iCount;
}


int CvCity::getReligionCount() const
{
	PROFILE_EXTRA_FUNC();
	int iCount = 0;
	for (int iI = 0; iI < GC.getNumReligionInfos(); iI++)
	{
		if (isHasReligion((ReligionTypes)iI))
		{
			iCount++;
		}
	}
	return iCount;
}

int CvCity::getCorporationCount() const
{
	PROFILE_EXTRA_FUNC();
	int iCount = 0;
	for (int iI = 0; iI < GC.getNumCorporationInfos(); iI++)
	{
		if (isHasCorporation((CorporationTypes)iI))
		{
			iCount++;
		}
	}
	return iCount;
}


int CvCity::getID() const
{
	return m_iID;
}


int CvCity::getIndex() const
{
	return (getID() & FLTA_INDEX_MASK);
}


IDInfo CvCity::getIDInfo() const
{
	IDInfo city(getOwner(), getID());
	return city;
}


void CvCity::setID(int iID)
{
	m_iID = iID;
}


int CvCity::getViewportX() const
{
	const CvViewport* pCurrentViewPort = GC.getCurrentViewport();
	FAssert(pCurrentViewPort != NULL);
	FAssert(isInViewport());

	return pCurrentViewPort->getViewportXFromMapX(m_iX);
}


int CvCity::getViewportY() const
{
	const CvViewport* pCurrentViewPort = GC.getCurrentViewport();
	FAssert(pCurrentViewPort != NULL);
	FAssert(isInViewport());

	return pCurrentViewPort->getViewportYFromMapY(m_iY);
}

bool CvCity::isInViewport() const
{
	return GC.getCurrentViewport()->isInViewport(m_iX, m_iY);
}


bool CvCity::at(int iX, int iY) const
{
	return ((getX() == iX) && (getY() == iY));
}


bool CvCity::at(const CvPlot* pPlot) const
{
	return (plot() == pPlot);
}


CvPlot* CvCity::plot() const
{
	return GC.getMap().plotSorenINLINE(getX(), getY());
}


/*DllExport*/ CvPlot* CvCity::plotExternal() const
{
#ifdef _DEBUG
	OutputDebugString("exe is asking for the plot of this city\n");
#endif
	FAssert(isInViewport());
	return GC.getMap().plotSorenINLINE(getX(), getY());
}


CvPlotGroup* CvCity::plotGroup(const PlayerTypes ePlayer) const
{
	return plot()->getPlotGroup(ePlayer);
}


bool CvCity::isConnectedTo(const CvCity* pCity) const
{
	return plot()->isConnectedTo(pCity);
}


bool CvCity::isConnectedToCapital(const PlayerTypes ePlayer) const
{
	return plot()->isConnectedToCapital(ePlayer);
}


int CvCity::getArea() const
{
	return plot()->getArea();
}

CvArea* CvCity::area() const
{
	return plot()->area();
}


CvArea* CvCity::waterArea(const bool bNoImpassable) const
{
	return plot()->waterArea(bNoImpassable);
}

// Expose plot function through city
CvArea* CvCity::secondWaterArea() const
{
	return plot()->secondWaterArea();
}

// Find the largest water area shared by this city and other city, if any
CvArea* CvCity::sharedWaterArea(const CvCity* pOtherCity) const
{
	CvArea* pWaterArea = waterArea(true);
	if (pWaterArea)
	{
		CvArea* pOtherWaterArea = pOtherCity->waterArea(true);
		if (pOtherWaterArea)
		{
			if (pWaterArea == pOtherWaterArea)
			{
				return pWaterArea;
			}
			CvArea* pSecondWaterArea = secondWaterArea();

			if (pSecondWaterArea && pSecondWaterArea == pOtherWaterArea)
			{
				return pSecondWaterArea;
			}
			CvArea* pOtherSecondWaterArea = pOtherCity->secondWaterArea();

			if (pOtherSecondWaterArea && pWaterArea == pOtherSecondWaterArea)
			{
				return pWaterArea;
			}

			if (pSecondWaterArea && pOtherSecondWaterArea && pSecondWaterArea == pOtherSecondWaterArea)
			{
				return pSecondWaterArea;
			}
		}
	}
	return NULL;
}

bool CvCity::isBlockaded() const
{
	return algo::any_of(plot()->adjacent(), CvPlot::fn::getBlockadedCount(getTeam()) > 0);
}


CvPlot* CvCity::getRallyPlot() const
{
	return GC.getMap().plotSorenINLINE(m_iRallyX, m_iRallyY);
}


void CvCity::setRallyPlot(const CvPlot* pPlot)
{
	if (getRallyPlot() != pPlot)
	{
		if (pPlot)
		{
			m_iRallyX = pPlot->getX();
			m_iRallyY = pPlot->getY();
		}
		else
		{
			m_iRallyX = INVALID_PLOT_COORD;
			m_iRallyY = INVALID_PLOT_COORD;
		}

		if (isCitySelected())
		{
			gDLL->getInterfaceIFace()->setDirty(ColoredPlots_DIRTY_BIT, true);
		}
	}
}


int CvCity::getGameTurnFounded(const bool bACalendar) const
{
	if (bACalendar) // Accurate Calendar
	{
		return decodeACTurn(m_iGameTurnFounded);
	}
	return m_iGameTurnFounded;
}


void CvCity::setGameTurnFounded(const int iNewValue, const bool bHistoricalCalendar)
{
	if (bHistoricalCalendar)
	{
		CvDate& turnDate = GC.getGame().getCurrentDate();
		int encodeddate = encodeACDateturn(turnDate.getYear(), iNewValue);
		m_iGameTurnFounded = encodeddate;
		//Calvitix (store the year directly, as the turn=>Date is dynamic with Accurate Calendar)

		GC.getMap().updateWorkingCity();
	}
	else
	{
		if (getGameTurnFounded() != iNewValue)
		{
			m_iGameTurnFounded = iNewValue;
			FASSERT_NOT_NEGATIVE(getGameTurnFounded());

			GC.getMap().updateWorkingCity();
		}
	}
}

int CvCity::getGameDateFounded(const bool bACalendar) const
{
	if (bACalendar) // Accurate Calendar
	{
		return decodeACDate(m_iGameTurnFounded);
	}
	return m_iGameTurnFounded;
}


void CvCity::setGameDateFounded(const int iNewValue, const bool bHistoricalCalendar)
{
	if (bHistoricalCalendar)
	{
		CvDate& turnDate = GC.getGame().getCurrentDate();
		//Calvitix (store the year directly, as the turn=>Date is dynamic with Accurate Calendar)
		m_iGameTurnFounded = encodeACDateturn(turnDate.getYear(), iNewValue);

		GC.getMap().updateWorkingCity();
	}
	else
	{
		if (getGameTurnFounded() != iNewValue)
		{
			m_iGameTurnFounded = iNewValue;
			FASSERT_NOT_NEGATIVE(getGameTurnFounded());

			GC.getMap().updateWorkingCity();
		}
	}
}

int CvCity::getGameTurnAcquired(const bool bHistoricalCalendar) const
{
	if (bHistoricalCalendar) // Accurate Calendar
	{
		return decodeACTurn(m_iGameTurnAcquired);
	}
	return m_iGameTurnAcquired;
}

int CvCity::getGameDateAcquired(const bool bHistoricalCalendar) const
{
	if (bHistoricalCalendar) // Accurate Calendar
	{
		return decodeACDate(m_iGameTurnAcquired);
	}
	return m_iGameTurnAcquired;
}


void CvCity::setGameTurnAcquired(const int iNewValue, const bool bHistoricalCalendar)
{
	if (bHistoricalCalendar)
	{
		CvDate& turnDate = GC.getGame().getCurrentDate();
		m_iGameTurnAcquired = iNewValue;
		//Calvitix (store the year directly, and encode the Date wit hwiseBit)
		m_iGameTurnAcquired = encodeACDateturn(turnDate.getYear(), iNewValue);

	}
	else
	{
		m_iGameTurnAcquired = iNewValue;
		FASSERT_NOT_NEGATIVE(getGameTurnAcquired());
	}
}


int CvCity::getPopulation() const
{
	return m_iPopulation;
}


// ══════════════════════ THE INTERNAL SLOT SETTERS (#430) ══════════════════════
// COMMIT the member, ANNOUNCE the fact -- and nothing else. Each is the ONE body that knows how to land its
// slot, so the public setter and CvCity::readBody reach the identical commit + emit and neither can drift from
// the other. The effects (rank caches, area/player totals, plot updates, alerts, art) stay in the PUBLIC setter,
// which is why the save read can call these: the stream is authoritative for base state and no effect gets to
// decide any part of it.

void CvCity::setPopulationInternal(int iNewValue)
{
	const int iOldPopulation = m_iPopulation;
	if (iOldPopulation == iNewValue)
	{
		return;
	}
	m_iPopulation = iNewValue;
	// The payload is the DELTA as a magnitude, never the new total: a consumer maintaining a sum needs how much
	// MOVED, and the event name supplies the direction.
	if (iNewValue > iOldPopulation)
	{
		emitCityPopulationAdded(getID(), getOwner(), iNewValue - iOldPopulation);
	}
	else
	{
		emitCityPopulationRemoved(getID(), getOwner(), iOldPopulation - iNewValue);
	}
}

void CvCity::setCultureLevelInternal(CultureLevelTypes eNewValue)
{
	const CultureLevelTypes eOldValue = (CultureLevelTypes)m_eCultureLevel;
	if (eOldValue == eNewValue)
	{
		return;
	}
	m_eCultureLevel = eNewValue;
	// THE RADIUS MAY HAVE GROWN OR SHRUNK: hand the plots between the two counts their membership change. Read
	// through the parameterized form, because m_eCultureLevel has already moved -- the OLD work area is only
	// answerable from the old level.
	changeWorkableArea(getNumCityPlotsAtCultureLevel((int)eOldValue),
		getNumCityPlotsAtCultureLevel((int)eNewValue));
	if (eOldValue != NO_CULTURELEVEL)
	{
		emitCityCultureLevelRemoved(getID(), getOwner(), (int)eOldValue);
	}
	if (eNewValue != NO_CULTURELEVEL)
	{
		emitCityCultureLevelAdded(getID(), getOwner(), (int)eNewValue);
	}
}

void CvCity::setHasReligionInternal(ReligionTypes eIndex, bool bNewValue)
{
	if (m_pabHasReligion[eIndex] == bNewValue)
	{
		return;
	}
	m_pabHasReligion[eIndex] = bNewValue;
	if (bNewValue)
	{
		emitCityReligionAdded(getID(), getOwner(), (int)eIndex);
	}
	else
	{
		emitCityReligionRemoved(getID(), getOwner(), (int)eIndex);
	}
}

void CvCity::setHasCorporationInternal(CorporationTypes eIndex, bool bNewValue)
{
	if (m_pabHasCorporation[eIndex] == bNewValue)
	{
		return;
	}
	m_pabHasCorporation[eIndex] = bNewValue;
	if (bNewValue)
	{
		emitCityCorporationAdded(getID(), getOwner(), (int)eIndex);
	}
	else
	{
		emitCityCorporationRemoved(getID(), getOwner(), (int)eIndex);
	}
}

void CvCity::setSpecialistCountInternal(SpecialistTypes eIndex, int iNewValue)
{
	const int iOldValue = m_paiSpecialistCount[eIndex];
	if (iOldValue == iNewValue)
	{
		return;
	}
	m_paiSpecialistCount[eIndex] = iNewValue;
	if (iNewValue > iOldValue)
	{
		emitCitySpecialistAdded(getID(), getOwner(), (int)eIndex, iNewValue - iOldValue);
	}
	else
	{
		emitCitySpecialistRemoved(getID(), getOwner(), (int)eIndex, iOldValue - iNewValue);
	}
}

void CvCity::setPopulation(int iNewValue, bool bNormal)
{
	const int iOldPopulation = getPopulation();

	if (iOldPopulation == iNewValue)
	{
		return;
	}
	// The commit and the fact; then this setter's own EFFECTS below.
	setPopulationInternal(iNewValue);

	FASSERT_NOT_NEGATIVE(iNewValue);

	CvPlayer& owner = GET_PLAYER(getOwner());
	owner.invalidatePopulationRankCache();

	if (iNewValue > getHighestPopulation())
	{
		setHighestPopulation(iNewValue);
	}

	area()->changePopulationPerPlayer(getOwner(), (iNewValue - iOldPopulation));
	owner.changeTotalPopulation(iNewValue - iOldPopulation);
	GET_TEAM(getTeam()).changeTotalPopulation(iNewValue - iOldPopulation);
	GC.getGame().changeTotalPopulation(iNewValue - iOldPopulation);

	if (iNewValue > 0)
	{
		if (bNormal)
		{
		}

		if (
			!isHuman()
		&&
			(
				iOldPopulation == 1 && iNewValue > 1
				||
				iNewValue == 1 && iOldPopulation > 1
				||
				iNewValue > iOldPopulation && owner.getNumCities() <= 2
			)
		) AI_setChooseProductionDirty(true);
	}

	AI_setAssignWorkDirty(true);

	setInfoDirty(true);
	setLayoutDirty(true);

	plot()->plotAction(PUF_makeInfoBarDirty);

	if (isCitySelected())
	{
		gDLL->getInterfaceIFace()->setDirty(SelectionButtons_DIRTY_BIT, true);
		gDLL->getInterfaceIFace()->setDirty(CityScreen_DIRTY_BIT, true);
		gDLL->getInterfaceIFace()->setDirty(InfoPane_DIRTY_BIT, true);
	}
}


void CvCity::changePopulation(int iChange)
{
	setPopulation(getPopulation() + iChange);
}


int64_t CvCity::getRealPopulation() const
{
	//return (((long)(pow((float)getPopulation(), 2.8f))) * 1000);

	//	Koshling - using table provided by Praetyre to give more realistic results
	static int realPopulationTable[] =
	{
		0,
		50,
		100,
		250,
		500,
		750,
		1000,
		1500,
		2000,
		2500,
		3000,
		4000,
		5000,
		7500,
		10000,
		15000,
		20000,
		30000,
		40000,
		50000,
		60000,
		70000,
		80000,
		90000,
		100000,
		125000,
		150000,
		175000,
		200000,
		225000,
		250000,
		300000,
		350000,
		400000,
		450000,
		500000,
		550000,
		600000,
		650000,
		700000,
		750000,
		800000,
		850000,
		900000,
		950000,
		1000000,
		1100000,
		1200000,
		1300000,
		1400000,
		1500000,
		1600000,
		1700000,
		1800000,
		1900000,
		2000000,
		2250000,
		2500000,
		2750000,
		3000000,
		3500000,
		4000000,
		5000000,
		6000000,
		7000000,
		8000000,
		9000000,
		10000000,
		12000000,
		14000000,
		16000000,
		18000000,
		20000000,
		22250000,
		25000000,
		27500000,
		30000000,
		35000000,
		40000000,
		45000000,
		50000000,
		55000000,
		60000000,
		65000000,
		70000000,
		75000000,
		80000000,
		85000000,
		90000000,
		95000000,
		100000000,
		110000000,
		120000000,
		130000000,
		140000000,
		150000000,
		160000000,
		170000000,
		180000000,
		190000000,
		200000000
	};
#define NUM_POP_TABLE_ENTRIES (sizeof(realPopulationTable)/sizeof(int))

	if (getPopulation() < NUM_POP_TABLE_ENTRIES)
	{
		return realPopulationTable[getPopulation()];
	}
	return realPopulationTable[NUM_POP_TABLE_ENTRIES - 1] + ((realPopulationTable[NUM_POP_TABLE_ENTRIES - 1] - realPopulationTable[NUM_POP_TABLE_ENTRIES - 2]) * (getPopulation() - NUM_POP_TABLE_ENTRIES));
}

int CvCity::getHighestPopulation() const
{
	return m_iHighestPopulation;
}


void CvCity::setHighestPopulation(int iNewValue)
{
	m_iHighestPopulation = iNewValue;
	FASSERT_NOT_NEGATIVE(getHighestPopulation());
}


int CvCity::getWorkingPopulation() const
{
	return m_iWorkingPopulation;
}


void CvCity::changeWorkingPopulation(int iChange)
{
	m_iWorkingPopulation += iChange;
	FASSERT_NOT_NEGATIVE(m_iWorkingPopulation);
}


int CvCity::getSpecialistPopulation() const
{
	return m_iSpecialistPopulation;
}


void CvCity::changeSpecialistPopulation(int iChange)
{
	if (iChange != 0)
	{
		m_iSpecialistPopulation += iChange;
		FASSERT_NOT_NEGATIVE(m_iSpecialistPopulation);

		GET_PLAYER(getOwner()).invalidateYieldRankCache();

	}
}


int CvCity::getNumGreatPeople() const
{
	return m_iNumGreatPeople;
}


void CvCity::changeNumGreatPeople(int iChange)
{
	if (iChange != 0)
	{
		m_iNumGreatPeople += iChange;
		FASSERT_NOT_NEGATIVE(m_iNumGreatPeople);

	}
}


int CvCity::getBaseGreatPeopleRate() const
{
	// A FRESH GATHER from the cascade's greatPeopleRate channel, replacing the serialized per-source
	// accumulator ([DEC-accumulator-cut-uniform]). ×100 native, and GPP is a whole count, so the single ÷100
	// is here at the discrete boundary. The realized city read IS the roll-up over the chain this city sits
	// under (team + empire + city, modifier.md §1), so the empire-scope trait deposits are ALREADY inside it —
	// adding an empire aggregate on top would count each of them once per city plus once more.
	// ×100 NATIVE -- a getter never reduces. The reduce belongs at the WAREHOUSE EDGE, i.e. the deposit into
	// m_iGreatPeopleProgress ([north-star.md] the warehouse carve-out: the cascade owns the RATE, the object
	// keeps its own ledger). That is what leaves the SERIALIZED progress human and this conversion save-neutral.
	const int iChannel = CascadeChannelRegistry::channelLookup(MODFAM_GREAT_PEOPLE_RATE, (int)CHANNEL_AMOUNT, -1);
	return std::max(0, InfoValuation::realizedAtCity(*this, iChannel));
}


int CvCity::getGreatPeopleRate() const
{
	if (isDisorder())
	{
		return 0;
	}
	return getBaseGreatPeopleRate() * getTotalGreatPeopleRateModifier() / 100;
}


int CvCity::getTotalGreatPeopleRateModifier() const
{
	const CvPlayer& owner = GET_PLAYER(getOwner());

	int aiScalars[NUM_INFO_SCALARS];
	owner.getScalars(aiScalars);
	// ⛔ THE CHANNEL'S OWN DEPOSITS ARE ALREADY IN THE BASE, so nothing from greatPeopleRate belongs in this
	// stack. getBaseGreatPeopleRate is realizedAtCity on the channel, i.e. the §2a combine -- both the flats
	// and the percents resolve there. Re-adding either here injects a second application of a value the base
	// already carries: the AMOUNT would put a ×100 magnitude into a percentage identity and count the base
	// twice, and a percent would apply the same stack twice over.
	// What legitimately remains are the two terms the channel does NOT carry, below.
	int iModifier = 100;

	if (owner.getStateReligion() != NO_RELIGION && isHasReligion(owner.getStateReligion()))
	{
		int aiStateReligion[NUM_STATE_RELIGION_KINDS];
		owner.getStateReligionKinds(aiStateReligion);
		iModifier += aiStateReligion[STATE_RELIGION_GREAT_PEOPLE_RATE];
	}

	if (owner.isGoldenAge())
	{
		iModifier += GC.getGOLDEN_AGE_GREAT_PEOPLE_MODIFIER();
	}

	return std::max(0, iModifier);
}




int CvCity::getGreatPeopleProgress() const
{
	return m_iGreatPeopleProgress;
}


void CvCity::changeGreatPeopleProgress(int iChange)
{
	m_iGreatPeopleProgress += iChange;
	FAssertMsg(getGreatPeopleProgress() >= 0, CvString::format("City %S m_iGreatPeopleProgress is %d", m_szName.c_str(), m_iGreatPeopleProgress).c_str());
}

CvProperties* CvCity::getProperties()
{
	return &m_Properties;
}

const CvProperties* CvCity::getPropertiesConst() const
{
	return &m_Properties;
}


// BUG - Building Additional Great People - start
/*
 * Returns the total additional great people rate that adding one of the given buildings will provide.
 *
 * Doesn't check if the building can be constructed in this city.
 */
int CvCity::getAdditionalGreatPeopleRateByBuilding(BuildingTypes eBuilding) const
{
	PROFILE_EXTRA_FUNC();
	FASSERT_BOUNDS(0, GC.getNumBuildingInfos(), eBuilding);

	const int iRate = getBaseGreatPeopleRate();
	const int iModifier = getTotalGreatPeopleRateModifier();
	int iExtra =
	(
		(iRate + getAdditionalBaseGreatPeopleRateByBuilding(eBuilding))
		*
		(iModifier + getAdditionalGreatPeopleRateModifierByBuilding(eBuilding))
		/
		100
		-
		iRate * iModifier / 100
	);
	const CvBuildingInfo& building = GC.getBuildingInfo(eBuilding);

	std::vector<int> supersededBuildings;
	EnablerKernel::supersededBy(EDGEB_BUILDINGS, (int)eBuilding, supersededBuildings);
	for (size_t iI = 0; iI < supersededBuildings.size(); iI++)
	{
		const BuildingTypes eBuildingX = static_cast<BuildingTypes>(supersededBuildings[iI]);

		if (hasFullyActiveBuilding(eBuildingX))
		{
			iExtra -= getAdditionalGreatPeopleRateByBuilding(eBuildingX);
		}
	}
	return iExtra;
}

/*
 * Returns the additional great people rate that adding one of the given buildings will provide.
 *
 * Doesn't check if the building can be constructed in this city.
 */
int CvCity::getAdditionalBaseGreatPeopleRateByBuilding(BuildingTypes eBuilding) const
{
	PROFILE_EXTRA_FUNC();
	FASSERT_BOUNDS(0, GC.getNumBuildingInfos(), eBuilding);

	const CvBuildingInfo& kBuilding = GC.getBuildingInfo(eBuilding);
	int iExtraRate = kBuilding.getScalar(SCALAR_GREAT_PEOPLE_RATE, CASC_SCOPE_CITY, CASC_UNIT_FLAT);

	// Specialists
	for (int iI = 0; iI < GC.getNumSpecialistInfos(); ++iI)
	{
		// The TYPED free slots this building opens for this specialist. The address is keyed directly by
		// the type with no container token, which is what the -1 segment selects ([modifier.md §5]).
		// THE READ EDGE: ×100 like every authored amount; a slot count is whole.
		const int iTypedFreeSlots =
			InfoValuation::keyedTarget(kBuilding.getModifiers(), MODFAM_FREE_SPECIALISTS,
				CHANNEL_AMOUNT, -1, iI) / 100;
		if (iTypedFreeSlots != 0)
		{
			iExtraRate += getAdditionalBaseGreatPeopleRateBySpecialist((SpecialistTypes)iI, iTypedFreeSlots);
		}
	}

	// The untyped slots this building opens here -- the engine picks each one's type at placement
	// ([modifier.md §6]), so the loop asks it per slot. ⚠ THE READ EDGE: the accessor is ×100 like every other
	// authored amount and the reader reduces -- a slot count is whole.
	const int iCityFreeSpecialistSlots = kBuilding.getFreeSpecialistsAny(CASC_SCOPE_CITY) / 100;
	for (int iI = 1; iI < iCityFreeSpecialistSlots + 1; iI++)
	{
		const SpecialistTypes eNewSpecialist = getBestSpecialist(iI);
		if (eNewSpecialist == NO_SPECIALIST) break;

		iExtraRate += GC.getSpecialistInfo(eNewSpecialist).getScalar(SCALAR_GREAT_PEOPLE_RATE, CASC_SCOPE_CITY, CASC_UNIT_FLAT);

	}

	return iExtraRate;
}

/*
 * Returns the additional great people rate modifier that adding one of the given buildings will provide.
 *
 * Doesn't check if the building can be constructed in this city.
 */
int CvCity::getAdditionalGreatPeopleRateModifierByBuilding(BuildingTypes eBuilding) const
{
	FASSERT_BOUNDS(0, GC.getNumBuildingInfos(), eBuilding);

	const CvBuildingInfo& kBuilding = GC.getBuildingInfo(eBuilding);

	return kBuilding.getScalar(SCALAR_GREAT_PEOPLE_RATE, CASC_SCOPE_CITY, CASC_UNIT_PERCENT) + kBuilding.getScalar(SCALAR_GREAT_PEOPLE_RATE, CASC_SCOPE_EMPIRE, CASC_UNIT_PERCENT);
}


// BUG - Specialist Additional Great People - start
/*
 * Returns the total additional great people rate that changing the number of the given specialist will provide/remove.
 */
int CvCity::getAdditionalGreatPeopleRateBySpecialist(SpecialistTypes eSpecialist, int iChange) const
{
	const int iRate = getBaseGreatPeopleRate();
	const int iModifier = getTotalGreatPeopleRateModifier();
	const int iExtraRate = getAdditionalBaseGreatPeopleRateBySpecialist(eSpecialist, iChange);

	const int iExtra = ((iRate + iExtraRate) * iModifier / 100) - (iRate * iModifier / 100);

	return iExtra;
}

/*
 * Returns the additional great people rate that changing the number of the given specialist will provide/remove.
 */
int CvCity::getAdditionalBaseGreatPeopleRateBySpecialist(SpecialistTypes eSpecialist, int iChange) const
{
	FASSERT_BOUNDS(0, GC.getNumSpecialistInfos(), eSpecialist);

	return iChange * GC.getSpecialistInfo(eSpecialist).getScalar(SCALAR_GREAT_PEOPLE_RATE, CASC_SCOPE_CITY, CASC_UNIT_FLAT);
}
// BUG - Specialist Additional Great People - end


int CvCity::getNumWorldWonders() const
{
	return m_iNumWorldWonders;
}


void CvCity::changeNumWorldWonders(int iChange)
{
	m_iNumWorldWonders += iChange;
	FASSERT_NOT_NEGATIVE(getNumWorldWonders());
}


int CvCity::getNumTeamWonders() const
{
	return m_iNumTeamWonders;
}


void CvCity::changeNumTeamWonders(int iChange)
{
	m_iNumTeamWonders += iChange;
	FASSERT_NOT_NEGATIVE(getNumTeamWonders());
}


int CvCity::getNumNationalWonders() const
{
	return m_iNumNationalWonders;
}


void CvCity::changeNumNationalWonders(int iChange)
{
	m_iNumNationalWonders += iChange;
	FASSERT_NOT_NEGATIVE(getNumNationalWonders());
}


int CvCity::getNumBuildings() const
{
	return m_iNumBuildings;
}


void CvCity::changeNumBuildings(int iChange)
{
	m_iNumBuildings += iChange;
	FASSERT_NOT_NEGATIVE(getNumBuildings());
}


// The four city-scope AMENITY verdicts, read from the city's own fold (json §8 / contexts.md). Each replaces a
// hand-named serialized counter maintained by its own changer -- the shape [DEC-uniform-cache-shape] calls a
// DEFECT, and the reason a NEW attribute used to cost an engine change (a counter, a fact, a predicate) instead
// of being pure data. The fold is derived, so it serializes nothing ([DEC-derived-never-trusted]).
bool CvCity::isGovernmentCenter() const CITY_HAS_AMENITY(getCityContext(), "governmentCenter")

// BUG - Building Saved Maintenance - start
/*
 * Returns the rounded total additional gold from saved maintenance that adding one of the given buildings will provide.
 *
 * Doesn't check if the building can be constructed in this city.
 */
int CvCity::getSavedMaintenanceByBuilding(BuildingTypes eBuilding) const
{
	return getSavedMaintenanceTimes100ByBuilding(eBuilding) / 100;
}

/*
 * Returns the total additional gold from saved maintenance times 100 that adding one of the given buildings will provide.
 *
 * Doesn't check if the building can be constructed in this city.
 */
int CvCity::getSavedMaintenanceTimes100ByBuilding(BuildingTypes eBuilding) const
{
	FASSERT_BOUNDS(0, GC.getNumBuildingInfos(), eBuilding);

	if (isDisorder() || isWeLoveTheKingDay())
	{
		return 0;
	}
	const CvBuildingInfo& kBuilding = GC.getBuildingInfo(eBuilding);
	const CvPlayer& owner = GET_PLAYER(getOwner());

	// ⚖ ONE call replaces the four hand-summed legacy reads (city + global + area + connected-city). The scope
	// axis merges two of them -- there is no area scope, so an `area` modifier authors at EMPIRE alongside
	// `global` ([state-repositories.md]) and summing both would double-count -- and the connected-city leg was
	// never a KIND: it is a CONDITION on an ordinary deposit, which the what-if evaluates against THIS city
	// through the eval ctx ([DEC-conditions-are-predicates]). That is why this is a valuation, not a point read.
	const int iModifier = kBuilding.expectedModifier(MODFAM_MAINTENANCE, MAINTENANCE_AMOUNT, CASC_UNIT_PERCENT,
		getCityContext(), owner.getEmpireContext(), plotGroup(getOwner()));

	// A building whose upkeep is a gold AMOUNT authors maintenance.city.flat -- the negative-gold fold, already
	// resolved by the curator, so the consumer no longer re-reads a negative commerce and flips its sign.
	const int iDirectMaintenance = kBuilding.getFlatMaintenance(MAINTENANCE_AMOUNT, CASC_SCOPE_CITY);

	if (iModifier == 0 && iDirectMaintenance == 0)
	{
		return 0;
	}
	const int iModOld = maintenancePercentStack(MAINTENANCE_AMOUNT);
	// The base is simply the channel's own FLAT leg now -- there is no separate engine-component sum to add,
	// because the components are ordinary deposits in that same leg.
	int64_t iBaseOld = 0;
	int64_t iPercentIgnored = 0;
	maintenanceLegs((int)MAINTENANCE_AMOUNT, iBaseOld, iPercentIgnored);

	return (int)(
		InfoValuation::realizedChannel(iBaseOld, iModOld, CASC_UNIT_FLAT)
		-
		InfoValuation::realizedChannel(iBaseOld + iDirectMaintenance, iModOld + iModifier, CASC_UNIT_FLAT)
	);
}
// ⚖ WE LOVE THE KING DAY SUPPRESSES THE CONSUMPTION OF THE VALUE, NOT ITS CONTENTS (owner) -- the ONE special
// case maintenance carries over any other cascade channel: *"a city maintenance should just emit 0 instead of
// the cascade block"* while the status holds. It is deliberately NOT a cache input: WLTKD is a ONE-TURN status
// re-applied every turn by its trigger ([state.md] / [CvStatus.h]), so marking on it would thrash the cache
// every single turn over a value that never moved. The stored number stays the real one and the READ declines
// to take it -- which is also why the read stays a bare fetch either way.
int64_t CvCity::getMaintenance() const
{
	return getMaintenanceTimes100() / 100;
}

int64_t CvCity::getMaintenanceTimes100() const
{
	// ⚖ DISORDER SUPPRESSES IT THE SAME WAY (owner) -- a city in disorder produces no output, so it takes no
	// maintenance either. It is the same read-time decline, for the same reason: both are transient conditions
	// the city is IN, never inputs the stored value was built from.
	// ⚑ `isDisorder()` is itself the OR of two ticking counters -- the city's occupation timer and the player's
	// anarchy turns -- i.e. a CITY status and a PLAYER status composed into one verdict ([CvStatus.h]). Once
	// those two counters move onto the status store this reads off it and nothing else here changes.
	if (hasStatus(CITYSTATUS_WE_LOVE_THE_KING_DAY) || isDisorder())
	{
		return 0;
	}
	// ⛔ THE DOWNWARD ROLL IS REALIZED AT READ ([modifier.md] §1), which is why NOTHING is cached here: a lower
	// scope that stored an upper scope's sums would force downward invalidation fan-out and "break the principle
	// of the cascade in the first place" ([state-repositories.md]). An empire- or team-scope maintenance percent
	// moving therefore needs no city mark at all -- its own package is marked at its own scope and this read
	// picks it up on the next bare fetch.
	// ⚑ There are no engine FORMULAS left to fold in: distance, city count and colonial separation are ordinary
	// authored deposits now (TECH_GAME_START's `maintenance` block), so maintenance is what every other channel
	// already was -- a sum to an endpoint.
	// ⚖ TWO TIERS, and the order is the mechanic: each COMPONENT kind resolves against its OWN modifiers (the
	// handicaps author `maintenance.empire.<kind>.percent`, so a difficulty setting scales the distance leg
	// without touching the city-count one), and the TOTAL then takes the empire-wide `amount` stack. Flattening
	// the two would apply every kind's modifier to every other kind's cost.
	int64_t iBase = 0;
	for (int iKind = 0; iKind < (int)NUM_MAINTENANCE_KINDS; ++iKind)
	{
		// AMOUNT is the total's own stack, applied below.
		if (iKind == (int)MAINTENANCE_AMOUNT)
		{
			continue;
		}
		// ⛔ CORPORATION IS NOT A CITY-MAINTENANCE COMPONENT -- it is its OWN pre-inflation expense
		// (`calcCorporateMaintenance` -> `CvPlayer::getCorporateMaintenance`, one of the six additive components
		// `calculatePreInflatedCosts` sums, [economy.md]). Its authored deposit is a CITY-scope flat, so it lands
		// in this package like any other and reads here perfectly plausibly -- summing it would charge the same
		// corporate gold TWICE in one expense total, silently.
		if (iKind == (int)MAINTENANCE_CORPORATION)
		{
			continue;
		}
		int64_t iKindFlat = 0;
		int64_t iKindPercent = 0;
		maintenanceLegs(iKind, iKindFlat, iKindPercent);
		if (iKindFlat != 0)
		{
			iBase += InfoValuation::realizedChannel(iKindFlat, iKindPercent, CASC_UNIT_FLAT);
		}
	}
	int64_t iAmountFlat = 0;
	int64_t iAmountPercent = 0;
	maintenanceLegs((int)MAINTENANCE_AMOUNT, iAmountFlat, iAmountPercent);
	return InfoValuation::realizedChannel(iBase + iAmountFlat, iAmountPercent, CASC_UNIT_FLAT);
}

// The realized value of ONE maintenance kind at this city -- what the per-component breakdowns display. It is
// the same per-kind composition the total sums, exposed once rather than re-walked by each consumer
// ([DEC-single-implementation]).
int64_t CvCity::maintenanceOfKind(int iKind) const
{
	if (hasStatus(CITYSTATUS_WE_LOVE_THE_KING_DAY) || isDisorder())
	{
		return 0;
	}
	int64_t iFlatSum = 0;
	int64_t iPercentSum = 0;
	maintenanceLegs(iKind, iFlatSum, iPercentSum);
	return InfoValuation::realizedChannel(iFlatSum, iPercentSum, CASC_UNIT_FLAT);
}

// The two LEGS of one maintenance KIND over the city's own chain (team + empire + city). ONE description of
// the chain, shared by every maintenance read here -- a consumer that re-walked it would be a second
// description of the same thing ([DEC-single-implementation]).
void CvCity::maintenanceLegs(int iKind, int64_t& flatSum, int64_t& percentSum) const
{
	InfoValuation::rolledLegsAtCity(
		*this, CascadeChannelRegistry::channelLookup(MODFAM_MAINTENANCE, iKind, -1), flatSum, percentSum);
}

int CvCity::maintenancePercentStack(int iKind) const
{
	int64_t iFlatSum = 0;
	int64_t iPercentSum = 0;
	maintenanceLegs(iKind, iFlatSum, iPercentSum);
	return (int)iPercentSum;
}



//	How much longer (or shorter) hurry anger lasts here, as a percent: the ONE roll-up over the chain this city
//	sits under, so a building's `hurryAnger.city.percent` and an empire-scope deposit answer through the same
//	read ([modifier.md] §1 -- the downward roll is realized AT READ, and a lower scope never stores an upper
//	scope's sum). The two hand-summed legs it replaces were a city accumulator plus the player's, which
//	double-counted nothing only because both had already lost their feeders.
//	⚠ BEHAVIOUR: the legacy changer re-scaled an in-flight `m_iHurryAngerTimer` proportionally whenever the
//	modifier moved. A computed value has no change moment to hook, so a timer already running now simply
//	resolves against the CURRENT modifier -- which is the recompute-from-source side, and the side
//	[DEC-accumulator-cut-uniform] rules correct.
//	⚠ The kind is a PERCENT, so it is not ×100 and nothing reduces here ([DEC-fixedpoint-x100]).
int CvCity::getHurryAngerModifier() const
{
	int aiScalars[NUM_INFO_SCALARS];
	getScalars(aiScalars);
	return aiScalars[SCALAR_HURRY_ANGER_MODIFIER];
}


// The city-scope deposit of the heal family's unitCombat-keyed member, read as an entry-list over the live
// sources rather than off a scope package -- a keyed deposit never folds into the scope slots.
int CvCity::getHealUnitCombatTypeTotal(UnitCombatTypes eUnitCombat) const
{
	PROFILE_EXTRA_FUNC();
	FASSERT_BOUNDS(0, GC.getNumUnitCombatInfos(), eUnitCombat);

	int iTotal = 0;
	std::vector<HealByUnitCombat> healRows;
	const std::set<int>& kActive = m_operatingBuildings.active;
	for (std::set<int>::const_iterator it = kActive.begin(); it != kActive.end(); ++it)
	{
		InfoValuation::collectHealByUnitCombat(
			GC.getBuildingInfo((BuildingTypes)*it).getModifiers(), healRows);
		for (size_t iRow = 0; iRow < healRows.size(); ++iRow)
		{
			if (healRows[iRow].iUnitCombat == (int)eUnitCombat)
			{
				iTotal += healRows[iRow].iHeal;
			}
		}
	}
	return iTotal / 100;
}

int CvCity::getEspionageHealthCounter() const
{
	return std::min(8, m_iEspionageHealthCounter);
}

void CvCity::changeEspionageHealthCounter(int iChange)
{
	if (iChange != 0)
	{
		m_iEspionageHealthCounter += iChange;
	}
}

int CvCity::getEspionageHappinessCounter() const
{
	return std::min(8, m_iEspionageHappinessCounter);
}


void CvCity::changeEspionageHappinessCounter(int iChange)
{
	if (iChange != 0)
	{
		m_iEspionageHappinessCounter += iChange;
	}
}


int CvCity::getFreshWaterGoodHealth() const
{
	return m_iFreshWaterGoodHealth;
}


void CvCity::updateFreshWaterHealth()
{
	const int iNewGoodHealth = plot()->isFreshWater() ? GC.getFRESH_WATER_HEALTH_CHANGE() : 0;

	if (getFreshWaterGoodHealth() != iNewGoodHealth)
	{
		m_iFreshWaterGoodHealth = iNewGoodHealth;
		FASSERT_NOT_NEGATIVE(getFreshWaterGoodHealth());

		AI_setAssignWorkDirty(true);

		if (getTeam() == GC.getGame().getActiveTeam())
		{
			setInfoDirty(true);
		}
	}
}


/*
 * Adds the total percentage health effects from existing features to iGood and iBad.
 *
 * Positive values for iBad mean an increase in unhealthiness.
 */
void CvCity::calculateFeatureHealthPercent(int& iGood, int& iBad) const
{
	PROFILE_EXTRA_FUNC();
	foreach_(const CvPlot* loopPlot, plots(NUM_CITY_PLOTS))
	{
		const FeatureTypes eFeature = loopPlot->getFeatureType();

		if (eFeature != NO_FEATURE)
		{
			const int iHealth = GC.getFeatureInfo(eFeature).getWellbeingModifier(WELLBEING_HEALTH, CASC_SCOPE_PLOT);

			if (iHealth > 0)
			{
				iGood += iHealth;
			}
			else
			{
				iBad -= iHealth;
			}
		}
	}
}

namespace {
	bool isBuildingFeatureRemove(const CvUnit* unit, const FeatureTypes eFeature) {
		const BuildTypes eBuild = unit->getBuildType();
		return eBuild != NO_BUILD && GC.getBuildInfo(eBuild).isFeatureRemove(eFeature);
	}

}
/*
 * Subtracts the total percentage health effects of features currently being removed to iGood and iBad.
 * If pIgnorePlot is not NULL, it is not checked for feature removal.
 * Checks only plots visible to this city's owner.
 *
 * Positive values for iBad mean an increase in unhealthiness.
 */
void CvCity::calculateFeatureHealthPercentChange(int& iGood, int& iBad, CvPlot* pIgnorePlot) const
{
	PROFILE_EXTRA_FUNC();
	foreach_(const CvPlot* loopPlot, plots(NUM_CITY_PLOTS))
	{
		if (loopPlot == pIgnorePlot || !loopPlot->isVisible(getTeam(), true))
			continue;

		const FeatureTypes eFeature = loopPlot->getFeatureType();

		if (eFeature != NO_FEATURE)
		{
			const int iHealth = GC.getFeatureInfo(eFeature).getWellbeingModifier(WELLBEING_HEALTH, CASC_SCOPE_PLOT);

			if (iHealth != 0)
			{
				if (algo::any_of(loopPlot->units(), bind(isBuildingFeatureRemove, _1, eFeature)))
				{
					if (iHealth > 0)
					{
						iGood += iHealth;
					}
					else
					{
						iBad -= iHealth;
					}
				}
			}
		}
	}
}

/*
 * Returns the total additional health that adding or removing iChange eFeatures will provide.
 */
int CvCity::getAdditionalHealthByFeature(FeatureTypes eFeature, int iChange) const
{
	int iGood = 0, iBad = 0;
	return getAdditionalHealthByFeature(eFeature, iChange, iGood, iBad);
}

/*
 * Returns the total additional health that adding or removing iChange eFeatures will provide
 * and sets the good and bad levels individually.
 *
 * Doesn't reset iGood or iBad to zero.
 * Positive values for iBad mean an increase in unhealthiness.
 */
int CvCity::getAdditionalHealthByFeature(FeatureTypes eFeature, int iChange, int& iGood, int& iBad) const
{
	FASSERT_BOUNDS(0, GC.getNumFeatureInfos(), eFeature);

	const int iHealth = GC.getFeatureInfo(eFeature).getWellbeingModifier(WELLBEING_HEALTH, CASC_SCOPE_PLOT);

	if (iHealth > 0)
	{
		return getAdditionalHealth(iChange * iHealth, 0, iGood, iBad);
	}
	else
	{
		return getAdditionalHealth(0, -iChange * iHealth, iGood, iBad);
	}
}

/*
 * Returns the total additional health that adding or removing a good or bad health percent will provide
 * and sets the good and bad levels individually.
 *
 * Doesn't reset iGood or iBad to zero.
 * Positive values for iBad and iBadPercent mean an increase in unhealthiness.
 */
int CvCity::getAdditionalHealth(int iGoodPercent, int iBadPercent, int& iGood, int& iBad) const
{
	const int iStarting = iGood - iBad;

	// Fold the city's CURRENT feature percent onto the proposed change, so the pair below is the resulting
	// LEVEL rather than the change alone. This is the whole of the feature term (modifier.md §2b: feature
	// wellbeing is the plot-scope percent summed over the radius, reduced at the point of use) -- the stored
	// accumulators that used to be added on top were a second copy of exactly this sum, so they counted the
	// current level twice against a caller that starts both sides at zero.
	calculateFeatureHealthPercent(iGoodPercent, iBadPercent);

	iGood += iGoodPercent / 100;
	iBad += iBadPercent / 100;

	return iGood - iBad - iStarting;
}
// BUG - Feature Health - end

// BUG - Actual Effects - start
/*
 * Returns the additional angry population caused by the given happiness changes.
 *
 * Positive values for iBad mean an increase in unhappiness.
 */
int CvCity::getAdditionalAngryPopuplation(int iGood, int iBad) const
{
	// The ANGER balance is the negated net. ⚠ netHappiness is ×100 NATIVE while iGood/iBad, iPop and the returned
	// figure are all whole citizens, so the verdict reduces HERE to meet them -- the same cluster boundary this
	// what-if family already declares at buildingWellbeing's fold ([DEC-fixedpoint-x100]). Without it a ×100
	// balance is ranged against a whole population and the result is pinned at the clamp.
	const int iAngerBalance = -netHappiness() / 100;
	const int iPop = getPopulation();

	return range(iAngerBalance + iBad - iGood, 0, iPop) - range(iAngerBalance, 0, iPop);
}

/*
 * Returns the additional spoiled food caused by the given health changes.
 *
 * Positive values for iBad mean an increase in unhealthiness.
 */
int CvCity::getAdditionalSpoiledFood(int iGood, int iBad, int iHealthAdjust) const
{
	// Whole health points throughout (iGood/iBad/iHealthAdjust and the returned food figure), so the ×100-native
	// verdict reduces here to join them ([DEC-fixedpoint-x100]: the reduce lives at the point of use).
	const int iRate = netHealth() / 100 + iHealthAdjust;

	return std::min(0, iRate) - std::min(0, iRate + iGood - iBad);
}

/*
 * Returns the additional starvation caused by the given spoiled food.
 */
int CvCity::getAdditionalStarvation(int iSpoiledFood, int iFoodAdjust) const
{
	// iSpoiledFood and iFoodAdjust are whole food, so the x100 surplus reduces at this use -- as ONE reduce over
	// the whole difference, never per operand.
	int aiYields[NUM_YIELD_TYPES];
	getYields(aiYields);
	int iFood = (aiYields[YIELD_FOOD] - foodConsumption()) / 100 + iFoodAdjust;

	if (iSpoiledFood > 0)
	{
		if (iFood <= 0)
		{
			return iSpoiledFood;
		}
		else if (iSpoiledFood > iFood)
		{
			return iSpoiledFood - iFood;
		}
	}
	else if (iSpoiledFood < 0)
	{
		if (iFood < 0)
		{
			return std::max(iFood, iSpoiledFood);
		}
	}

	return 0;
}
// BUG - Actual Effects - start


int CvCity::getBuildingHealth(BuildingTypes eBuilding) const
{
	int iHealth = getBuildingGoodHealth(eBuilding);

	if (!isBuildingOnlyHealthy())
	{
		iHealth += getBuildingBadHealth(eBuilding);
	}

	return iHealth;
}

// ⚖ ONE valuation replaces the four hand-summed legacy terms. `expectedWellbeing` resolves what this building
// actually delivers HERE -- its authored health, its per-POPULATION entries (a §3.7 `per` count-scaler on the
// same deposit, never the separate `healthPercentPerPopulation` member the legacy read), and any conditioned
// entries -- through the ONE evaluator against this city's contexts.
// ⚑ Good and bad health are ONE signed authored number, not two fields: modifier.md §2b routes a negative
// health deposit to the opposing UNHEALTH channel at fill, so the split falls out of the group read.
// ⚠ The group answers ×100 ([DEC-fixedpoint-x100]); these two getters are whole-health-point readers, so the
// ÷100 belongs here, at the point of use. `getBuildingHealthChange` stays: it is EVENT/VOTE-granted one-shot
// state, not a derivable deposit ([state-repositories.md] -- events in a recompute cache is the banned shape).
void CvCity::buildingWellbeing(BuildingTypes eBuilding, int (&wellbeing)[NUM_WELLBEING_CHANNELS]) const
{
	GC.getBuildingInfo(eBuilding).expectedWellbeing(
		getCityContext(), GET_PLAYER(getOwner()).getEmpireContext(), plotGroup(getOwner()), wellbeing);
}

int CvCity::getBuildingGoodHealth(BuildingTypes eBuilding) const
{
	int aiWellbeing[NUM_WELLBEING_CHANNELS];
	buildingWellbeing(eBuilding, aiWellbeing);

	int iHealth = aiWellbeing[WELLBEING_HEALTH] / 100;
	iHealth += std::max(0, getBuildingHealthChange(eBuilding));
	return iHealth;
}

int CvCity::getBuildingBadHealth(BuildingTypes eBuilding) const
{
	if (isBuildingOnlyHealthy())
	{
		return 0;
	}
	int aiWellbeing[NUM_WELLBEING_CHANNELS];
	buildingWellbeing(eBuilding, aiWellbeing);

	// The channels are POSITIVE magnitudes; this getter's callers expect the bad side signed NEGATIVE.
	int iHealth = -(aiWellbeing[WELLBEING_UNHEALTH] / 100);
	iHealth += std::min(0, getBuildingHealthChange(eBuilding));
	return iHealth;
}

int CvCity::getMilitaryHappinessUnits() const
{
	return m_iMilitaryHappinessUnits;
}


int CvCity::getMilitaryHappiness() const
{
	return (getMilitaryHappinessUnits() * GET_PLAYER(getOwner()).getHappyPerMilitaryUnit());
}


void CvCity::changeMilitaryHappinessUnits(int iChange)
{
	if (iChange != 0)
	{
		m_iMilitaryHappinessUnits += iChange;
		FASSERT_NOT_NEGATIVE(getMilitaryHappinessUnits());

		AI_setAssignWorkDirty(true);
	}
}


int CvCity::getBuildingHappiness(BuildingTypes eBuilding) const
{
	PROFILE_EXTRA_FUNC();
	// The building's own authored happiness, resolved HERE through the ONE valuation -- which carries its
	// per-POPULATION entries too, so the hand-multiplied `happinessPercentPerPopulation` term is gone with the
	// member it read (a `per<X>`-named member IS a §3.7 count-scaler, never a kind -- json.md §6). Happiness and
	// anger are ONE signed authored number (modifier.md §2b), so the net is the answer this getter wants.
	int aiOwnWellbeing[NUM_WELLBEING_CHANNELS];
	buildingWellbeing(eBuilding, aiOwnWellbeing);
	int iHappiness =
	(
		(aiOwnWellbeing[WELLBEING_HAPPINESS] - aiOwnWellbeing[WELLBEING_ANGER]) / 100
		+
		getBuildingHappyChange(eBuilding)
		+
		GET_PLAYER(getOwner()).getExtraBuildingHappiness(eBuilding)
	);

	// ⛔ The per-commerce-rate happiness loop is GONE, not lost: a slider rate is a §3.1 count-scaler TOKEN
	// (GOLD_RATE / RESEARCH_RATE / CULTURE_RATE / ESPIONAGE_RATE), so "happiness per 10% culture rate" is an
	// ordinary happiness deposit carrying a `per`, already resolved into the wellbeing read above. Re-adding it
	// here would count it twice.

	return iHappiness;
}


int CvCity::getAdditionalHealthByPlayerNoUnhealthyPopulation(int iExtraPop, int iIgnoreNoUnhealthyPopulationCount) const
{
	int iHealth = 0;
	if (iIgnoreNoUnhealthyPopulationCount != 0)
	{
		if (GET_PLAYER(getOwner()).getNoUnhealthyPopulationCount() <= iIgnoreNoUnhealthyPopulationCount && !cityHasNoUnhealthyPopulation())
		{
			//std::max(0, ((getPopulation() + iExtra - ((bNoAngry) ? angryPopulation(iExtra) : 0))))
			iHealth += std::max(0, ((getPopulation() + iExtraPop)));
		}
	}
	else
	{
		iHealth += unhealthyPopulation(iExtraPop);
	}
	return iHealth;
}

int CvCity::getAdditionalHealthByPlayerBuildingOnlyHealthy(int iIgnoreBuildingOnlyHealthyCount) const
{
	int iHealth = 0;
	if (iIgnoreBuildingOnlyHealthyCount != 0)
	{
		CvPlayer& kOwner = GET_PLAYER(getOwner());
		int iOwnerBuildingOnlyHealthyCount = kOwner.getBuildingOnlyHealthyCount();
		if (iOwnerBuildingOnlyHealthyCount <= iIgnoreBuildingOnlyHealthyCount && !cityHasBuildingOnlyHealthy())
		{
			kOwner.changeBuildingOnlyHealthyCount(-iOwnerBuildingOnlyHealthyCount, true);
			iHealth -= totalBadBuildingHealth();
			kOwner.changeBuildingOnlyHealthyCount(iOwnerBuildingOnlyHealthyCount, true);
		}
	}
	else
	{
		iHealth -= totalBadBuildingHealth();
	}
	return iHealth;
}

/********************************************************************************/
/* 	New Civic AI												END 			*/
/********************************************************************************/

// BUG - Building Additional Happiness - start
/*
 * Returns the total additional happiness that adding one of the given buildings will provide.
 *
 * Doesn't check if the building can be constructed in this city.
 */
int CvCity::getAdditionalHappinessByBuilding(BuildingTypes eBuilding) const
{
	int iGood = 0, iBad = 0, iAngryPop = 0;
	return getAdditionalHappinessByBuilding(eBuilding, iGood, iBad, iAngryPop);
}

/*
 * Returns the total additional happiness that adding one of the given buildings will provide
 * and sets the good and bad levels individually and any resulting additional angry population.
 *
 * Doesn't reset iGood or iBad to zero.
 * Doesn't check if the building can be constructed in this city.
 */
int CvCity::getAdditionalHappinessByBuilding(BuildingTypes eBuilding, int& iGood, int& iBad, int& iAngryPop) const
{
	PROFILE_FUNC();

	FASSERT_BOUNDS(0, GC.getNumBuildingInfos(), eBuilding);

	const CvBuildingInfo& kBuilding = GC.getBuildingInfo(eBuilding);

	int iStarting = iGood - iBad;
	int iStartingBad = iBad;

	// ⚖ The building's OWN wellbeing, resolved for THIS city -- the valuation, not a point read. A point read
	// serves the compiled UNCONDITIONED sum only ([patterns.md] THE GETTER SETUP), so the conditioned entries
	// would be silently missing: the state-religion happiness this used to hand-gate is authored as a
	// {STATE_RELIGION: <the building's religion>} conditioned deposit (curate_religion), and it is the ONE
	// evaluator against this city's contexts that decides whether it applies. The same call folds the
	// empire-scope leg into the experienced-here answer, so both scopes read once.
	// ⚠ The channels are POSITIVE magnitudes ×100; addGoodOrBad wants ONE signed human number per side.
	{
		int aiBuildingWellbeing[NUM_WELLBEING_CHANNELS];
		buildingWellbeing(eBuilding, aiBuildingWellbeing);
		addGoodOrBad(aiBuildingWellbeing[WELLBEING_HAPPINESS] / 100, iGood, iBad);
		addGoodOrBad(-(aiBuildingWellbeing[WELLBEING_ANGER] / 100), iGood, iBad);
	}

	// Building
	addGoodOrBad(getBuildingHappyChange(eBuilding), iGood, iBad);

	// Other Building
	// The KEYED happiness deposits ([modifier.md §5]): an entry-list read over what this building
	// authors onto NAMED other buildings -- never folded scope-wide. ×100 at the slot, human here.
	static int s_segBuildings = -1;
	std::vector<std::pair<int, int> > kKeyedHappy;
	kBuilding.getModifiers()->targetedSums(MODFAM_HAPPINESS, CHANNEL_AMOUNT, CASC_SCOPE_EMPIRE,
		CASC_UNIT_FLAT, modSegmentCached("buildings", s_segBuildings), kKeyedHappy);
	for (size_t iKeyed = 0; iKeyed < kKeyedHappy.size(); ++iKeyed)
	{
		const BuildingTypes eKeyedBuilding = (BuildingTypes)kKeyedHappy[iKeyed].first;
		const int iKeyedHappy = kKeyedHappy[iKeyed].second / 100;
		addGoodOrBad(iKeyedHappy * (isActiveBuilding(eKeyedBuilding) + (eBuilding == eKeyedBuilding ? 1 : 0)), iGood, iBad);
	}

	// Player Building
	addGoodOrBad(GET_PLAYER(getOwner()).getExtraBuildingHappiness(eBuilding), iGood, iBad);

	// Bonus
	// ⛔ No BONUS-keyed happiness read: the axis has ZERO authorings across the whole data set (keyed happiness
	// authors `empire.buildings` only). The loop existed for data that does not exist.

	// ⛔ No per-commerce-rate loop: a slider rate is a §3.1 count-scaler TOKEN, so that happiness is an ordinary
	// deposit carrying a `per` and is already inside the wellbeing read -- adding it here would double it.

	// War Weariness Modifier -- ONE scalar at two scopes ([DEC-scope-is-an-axis]); the city and the former
	// "global" getter were the same slot, so both scopes are read rather than two members.
	int iWarWearinessModifier =
		kBuilding.getScalar(SCALAR_WAR_WEARINESS, CASC_SCOPE_CITY, CASC_UNIT_PERCENT)
		+ kBuilding.getScalar(SCALAR_WAR_WEARINESS, CASC_SCOPE_EMPIRE, CASC_UNIT_PERCENT);
	if (iWarWearinessModifier != 0)
	{
		int iBaseAngerPercent = 0;

		iBaseAngerPercent += getOvercrowdingPercentAnger();
		iBaseAngerPercent += getNoMilitaryPercentAnger();
		iBaseAngerPercent += getCulturePercentAnger();
		iBaseAngerPercent += getReligionPercentAnger();
		iBaseAngerPercent += getHurryPercentAnger();
		iBaseAngerPercent += getConscriptPercentAnger();
		iBaseAngerPercent += getDefyResolutionPercentAnger();

		int iCurrentAngerPercent = iBaseAngerPercent + getWarWearinessPercentAnger();
		int iCurrentUnhappiness = iCurrentAngerPercent * getPopulation() / GC.getPERCENT_ANGER_DIVISOR();

		int iNewWarAngerPercent = GET_PLAYER(getOwner()).getWarWearinessPercentAnger();
		int aiWarScalars[NUM_INFO_SCALARS];
		getScalars(aiWarScalars);
		iNewWarAngerPercent *= std::max(0, (iWarWearinessModifier + aiWarScalars[SCALAR_WAR_WEARINESS] + 100));
		iNewWarAngerPercent /= 100;
		int iNewAngerPercent = iBaseAngerPercent + iNewWarAngerPercent;
		int iNewUnhappiness = iNewAngerPercent * getPopulation() / GC.getPERCENT_ANGER_DIVISOR();

		iBad += iNewUnhappiness - iCurrentUnhappiness;
	}

	//	KOSHLING - port from K-mod - no unhappiness already => we don't care what this building does
	if (isNoUnhappiness())
	{
		iBad = iStartingBad;
	}
	// No Unhappiness
	else if (kBuilding.providesAmenity(CLS_AMENITY_ABOLISHED_ANGER))
	{
		// override extra unhappiness and completely negate all existing unhappiness
		// The building negates ALL unhappiness: the removal it offers is the whole ANGER side.
		int aNoUnhappyWellbeing[NUM_WELLBEING_CHANNELS];
		realizedWellbeing(0, aNoUnhappyWellbeing);
		iBad = iStartingBad - aNoUnhappyWellbeing[WELLBEING_ANGER] / 100;
	}
	// Effect on Angry Population -- the ×100 verdict reduces to the whole-citizen scale iGood/iBad/iPop use here.
	const int iAngerBalance = -netHappiness() / 100;
	const int iPop = getPopulation();
	iAngryPop += range(iAngerBalance + iBad - iGood, 0, iPop) - range(iAngerBalance, 0, iPop);

	// ⛔ No TECH-keyed happiness read: zero authorings anywhere (keyed happiness is empire.buildings).

	std::vector<int> supersededBuildings;
	EnablerKernel::supersededBy(EDGEB_BUILDINGS, (int)eBuilding, supersededBuildings);
	for (size_t iI = 0; iI < supersededBuildings.size(); iI++)
	{
		const BuildingTypes eBuildingX = static_cast<BuildingTypes>(supersededBuildings[iI]);

		if (isActiveBuilding(eBuildingX))
		{
			addGoodOrBad(-getBuildingHappiness(eBuildingX), iGood, iBad);
		}
	}

	int iSpecialistExtraHappy = 0;

	// The untyped slots this building opens here -- the engine picks each one's type at placement
	// ([modifier.md §6]), so the loop asks it per slot. ⚠ THE READ EDGE: the accessor is ×100 like every other
	// authored amount and the reader reduces -- a slot count is whole.
	const int iCityFreeSpecialistSlots = kBuilding.getFreeSpecialistsAny(CASC_SCOPE_CITY) / 100;
	for (int iI = 1; iI < iCityFreeSpecialistSlots + 1; iI++)
	{
		const SpecialistTypes eNewSpecialist = getBestSpecialist(iI);
		if (eNewSpecialist == NO_SPECIALIST) break;

		iSpecialistExtraHappy += GC.getSpecialistInfo(eNewSpecialist).getFlatWellbeing(WELLBEING_HAPPINESS, CASC_SCOPE_CITY);
	}
	iSpecialistExtraHappy /= 100;
	addGoodOrBad(iSpecialistExtraHappy, iGood, iBad);

	return iGood - iBad - iStarting;
}


/*
 * Returns the total additional health that adding one of the given buildings will provide.
 *
 * Doesn't check if the building can be constructed in this city.
 */
int CvCity::getAdditionalHealthByBuilding(BuildingTypes eBuilding) const
{
	int iGood = 0, iBad = 0, iSpoiledFood = 0, iStarvation = 0;
	return getAdditionalHealthByBuilding(eBuilding, iGood, iBad, iSpoiledFood, iStarvation);
}

/*
 * Returns the total additional health that adding one of the given buildings will provide
 * and sets the good and bad levels individually and any resulting additional spoiled food.
 *
 * Doesn't reset iGood, iBad, iSpoiledFood, iStarvation to zero.
 * Doesn't check if the building can be constructed in this city.
 */
int CvCity::getAdditionalHealthByBuilding(BuildingTypes eBuilding, int& iGood, int& iBad, int& iSpoiledFood, int& iStarvation) const
{
	PROFILE_FUNC();

	FASSERT_BOUNDS(0, GC.getNumBuildingInfos(), eBuilding);

	const CvBuildingInfo& kBuilding = GC.getBuildingInfo(eBuilding);

	const int iStarting = iGood - iBad;
	const int iStartingBad = iBad;

	// The building's authored health as experienced HERE, through the ONE valuation: it folds every scope this
	// city sits under (world/team/empire/city) AND evaluates the conditioned tail against this city's contexts,
	// so the bonus-gated, tech-gated and `per`-scaled entries all resolve inside this one call.
	// ⚠ It is therefore the WHOLE deposit-derived answer -- adding any per-source or per-scope leg beside it
	// counts that leg twice. Good and bad are one signed number (modifier.md §2b), split by addGoodOrBad.
	// ⚠ The ÷100 is a CLUSTER BOUNDARY, not a sanctioned shape: iGood/iBad are still human whole points here
	// (they mix with population and food below), so this reduces to meet them. It goes when the yield/food/
	// wellbeing cluster converts as a unit -- a scale conversion inside a calculation is the defect, never the
	// fix ([DEC-fixedpoint-x100]; fixed-point-and-scales.md § CONVERT BY ARITHMETIC CLUSTER). Do not copy it.
	int aiOwnWellbeing[NUM_WELLBEING_CHANNELS];
	buildingWellbeing(eBuilding, aiOwnWellbeing);
	addGoodOrBad((aiOwnWellbeing[WELLBEING_HEALTH] - aiOwnWellbeing[WELLBEING_UNHEALTH]) / 100, iGood, iBad);

	// Building
	addGoodOrBad(getBuildingHealthChange(eBuilding), iGood, iBad);

	// Player Building
	addGoodOrBad(GET_PLAYER(getOwner()).getExtraBuildingHealth(eBuilding), iGood, iBad);

	// No Unhealthiness from Buildings
	if (isBuildingOnlyHealthy())
	{
		// undo bad from this building
		iBad = iStartingBad;
	}
	if (kBuilding.providesAmenity(CLS_AMENITY_ABOLISHED_UNHEALTH_FROM_BUILDINGS))
	{
		// undo bad from this and all existing buildings
		iBad = iStartingBad + totalBadBuildingHealth();
	}

	// No Unhealthiness from Population
	if (kBuilding.providesAmenity(CLS_AMENITY_ABOLISHED_UNHEALTH_FROM_POPULATION))
	{
		iBad -= getPopulation();
	}

	// Effect on Spoiled Food
	const int iHealthBalance = netHealth() / 100;   // whole health points against a whole food figure
	int aiWhatIfYields[NUM_YIELD_TYPES];
	getYields(aiWhatIfYields);
	int iFood = (aiWhatIfYields[YIELD_FOOD] - foodConsumption()) / 100;
	iSpoiledFood -= std::min(0, iHealthBalance + iGood - iBad) - std::min(0, iHealthBalance);
	if (iSpoiledFood > 0)
	{
		if (iFood <= 0)
		{
			iStarvation += iSpoiledFood;
		}
		else if (iSpoiledFood > iFood)
		{
			iStarvation += iSpoiledFood - iFood;
		}
	}
	else if (iSpoiledFood < 0)
	{
		if (iFood < 0)
		{
			iStarvation += std::max(iFood, iSpoiledFood);
		}
	}

	std::vector<int> supersededBuildings;
	EnablerKernel::supersededBy(EDGEB_BUILDINGS, (int)eBuilding, supersededBuildings);
	for (size_t iI = 0; iI < supersededBuildings.size(); iI++)
	{
		const BuildingTypes eBuildingX = static_cast<BuildingTypes>(supersededBuildings[iI]);

		if (hasFullyActiveBuilding(eBuildingX))
		{
			addGoodOrBad(-getBuildingHealth(eBuildingX), iGood, iBad);
		}
	}

	int iSpecialistExtraHealth = 0;

	// The untyped slots this building opens here -- the engine picks each one's type at placement
	// ([modifier.md §6]), so the loop asks it per slot. ⚠ THE READ EDGE: the accessor is ×100 like every other
	// authored amount and the reader reduces -- a slot count is whole.
	const int iCityFreeSpecialistSlots = kBuilding.getFreeSpecialistsAny(CASC_SCOPE_CITY) / 100;
	for (int iI = 1; iI < iCityFreeSpecialistSlots + 1; iI++)
	{
		const SpecialistTypes eNewSpecialist = getBestSpecialist(iI);
		if (eNewSpecialist == NO_SPECIALIST) break;

		iSpecialistExtraHealth += GC.getSpecialistInfo(eNewSpecialist).getFlatWellbeing(WELLBEING_HEALTH, CASC_SCOPE_CITY);
	}
	iSpecialistExtraHealth /= 100;
	addGoodOrBad(iSpecialistExtraHealth, iGood, iBad);

	return iGood - iBad - iStarting;
}

/*
 * Adds iValue to iGood if it is positive or its negative to iBad if it is negative.
 */
void addGoodOrBad(int iValue, int& iGood, int& iBad)
{
	if (iValue > 0)
	{
		iGood += iValue;
	}
	else if (iValue < 0)
	{
		iBad -= iValue;
	}
}

/*
 * Subtracts iValue from iGood if it is positive or its negative from iBad if it is negative.
 */
void subtractGoodOrBad(int iValue, int& iGood, int& iBad)
{
	if (iValue > 0)
	{
		iGood -= iValue;
	}
	else if (iValue < 0)
	{
		iBad += iValue;
	}
}
// BUG - Building Additional Happiness - end






int CvCity::getExtraHappiness() const
{
	return m_iExtraHappiness;
}


void CvCity::changeExtraHappiness(int iChange)
{
	if (iChange != 0)
	{
		m_iExtraHappiness += iChange;

		AI_setAssignWorkDirty(true);
	}
}

int CvCity::getExtraHealth() const
{
	return m_iExtraHealth;
}


void CvCity::changeExtraHealth(int iChange)
{
	if (iChange != 0)
	{
		m_iExtraHealth += iChange;

		AI_setAssignWorkDirty(true);
	}
}



int CvCity::getHurryAngerTimer() const
{
	return m_iHurryAngerTimer;
}


void CvCity::changeHurryAngerTimer(int iChange)
{
	if (iChange != 0)
	{
		m_iHurryAngerTimer += iChange;
		FASSERT_NOT_NEGATIVE(getHurryAngerTimer());

		AI_setAssignWorkDirty(true);
	}
}


int CvCity::getRevRequestAngerTimer() const
{
	return m_iRevRequestAngerTimer;
}

void CvCity::changeRevRequestAngerTimer(int iChange)
{
	if (iChange != 0)
	{
		m_iRevRequestAngerTimer += iChange;
		FASSERT_NOT_NEGATIVE(getRevRequestAngerTimer());

		AI_setAssignWorkDirty(true);
	}
}

int CvCity::getRevSuccessTimer() const
{
	return m_iRevSuccessTimer;
}

void CvCity::changeRevSuccessTimer(int iChange)
{
	if (iChange != 0)
	{
		m_iRevSuccessTimer += iChange;
		FASSERT_NOT_NEGATIVE(getRevSuccessTimer());

		AI_setAssignWorkDirty(true);
	}
}


int CvCity::getConscriptAngerTimer() const
{
	return m_iConscriptAngerTimer;
}


void CvCity::changeConscriptAngerTimer(int iChange)
{
	if (iChange != 0)
	{
		m_iConscriptAngerTimer += iChange;
		FASSERT_NOT_NEGATIVE(getConscriptAngerTimer());

		AI_setAssignWorkDirty(true);
	}
}

int CvCity::getDefyResolutionAngerTimer() const
{
	return m_iDefyResolutionAngerTimer;
}


void CvCity::changeDefyResolutionAngerTimer(int iChange)
{
	if (iChange != 0)
	{
		m_iDefyResolutionAngerTimer += iChange;
		FASSERT_NOT_NEGATIVE(getDefyResolutionAngerTimer());

		AI_setAssignWorkDirty(true);
	}
}

int CvCity::flatDefyResolutionAngerLength() const
{
	int iAnger = GC.getDEFY_RESOLUTION_ANGER_DIVISOR();

	iAnger *= CvGameSpeedScale::speedPercent();
	iAnger /= 100;

	return std::max(1, iAnger);
}


int CvCity::getHappinessTimer() const
{
	return m_iHappinessTimer;
}


void CvCity::changeHappinessTimer(int iChange)
{
	if (iChange != 0)
	{
		m_iHappinessTimer += iChange;
		FASSERT_NOT_NEGATIVE(getHappinessTimer());

		AI_setAssignWorkDirty(true);
	}
}


bool CvCity::isNoUnhappiness() const CITY_HAS_AMENITY(getCityContext(), "abolishedAnger")


bool CvCity::isNoUnhealthyPopulation() const
{
	// The EMPIRE leg stays an explicit OR: the player-scope flag is fed by its own legacy path, not by this city's
	// fold. It collapses once an empire grantor authors the amenity (it then folds into every city).
	if (GET_PLAYER(getOwner()).isNoUnhealthyPopulation())
	{
		return true;
	}
	static int s_amenityId = -1;
	return getCityContext().hasAmenityKey(s_amenityId, "abolishedUnhealthFromPopulation");
}


bool CvCity::cityHasNoUnhealthyPopulation() const CITY_HAS_AMENITY(getCityContext(), "abolishedUnhealthFromPopulation")
bool CvCity::cityHasBuildingOnlyHealthy() const   CITY_HAS_AMENITY(getCityContext(), "abolishedUnhealthFromBuildings")

bool CvCity::isBuildingOnlyHealthy() const
{
	// The EMPIRE leg stays an explicit OR -- see isNoUnhealthyPopulation above.
	if (GET_PLAYER(getOwner()).isBuildingOnlyHealthy())
	{
		return true;
	}
	static int s_amenityId = -1;
	return getCityContext().hasAmenityKey(s_amenityId, "abolishedUnhealthFromBuildings");
}


int CvCity::getFood() const
{
	return m_iFood;
}


void CvCity::setFood(int iNewValue)
{
	if (m_iFood != iNewValue)
	{
		m_iFood = iNewValue;

		if (getTeam() == GC.getGame().getActiveTeam())
		{
			setInfoDirty(true);
		}
	}
}


void CvCity::changeFood(int iChange, const bool bHandleGrowth)
{
	if (iChange != 0)
	{
		m_iFood += iChange;

		if (bHandleGrowth)
		{
			if (iChange > 0)
			{
				changeFoodKept(std::max(1, iChange * getFoodKeptPercent() / 100));
			}
			else // decay stored food, hardcoded rate for now.
			{
				changeFoodKept(std::min(-1, iChange / 2));
			}

			if (m_iFood < 0)
			{
				while (m_iFood < 0)
				{
					if (getPopulation() > 1)
					{
						changePopulation(-1);
						m_iFood = m_iFood + growthThreshold();
					}
					else m_iFood = 0;
				}
			}
			else
			{
				int iGrowthThreshold = growthThreshold();
				if (m_iFood >= iGrowthThreshold)
				{
					if (isHuman() && AI_avoidGrowth() || AI_isEmphasizeAvoidGrowth())
					{
						m_iFood = iGrowthThreshold;
					}
					else
					{
						while (m_iFood >= iGrowthThreshold)
						{
							m_iFood -= iGrowthThreshold;

							if (m_iFood < getFoodKept())
							{
								const int iDiff = getFoodKept() - m_iFood;
								m_iFood += iDiff;
								changeFoodKept(-iDiff);
							}
							changePopulation(1);
							iGrowthThreshold = growthThreshold();
						}
					}
				}
			}
		}
#ifdef YIELD_VALUE_CACHING
		//	Yield calculation depends on city food stores so invalidate cache
		ClearYieldValueCache();
#endif
	}
}


void CvCity::changeFoodKept(int iChange)
{
	if (iChange != 0)
	{
		m_iFoodKept = range(m_iFoodKept + iChange, 0, growthThreshold() * getFoodKeptPercent() / 100);
	}
}


// The [0,99] clamp is a real rule of the mechanic (never keep everything), so it survives the cut.
int CvCity::getFoodKeptPercent() const { return range(cascadeValue(MODFAM_FOOD_KEPT, CHANNEL_AMOUNT), 0, 99); }


int CvCity::getMaxProductionOverflow() const
{
	// The multiplier (turns of base production that may be banked as overflow before
	// the excess is converted to gold) is a player-configurable BUG option; default 2
	// reproduces the historical hard-coded behaviour.
	// Overflow is banked in whole hammers, so the rate reduces at this use.
	int aiYields[NUM_YIELD_TYPES];
	getYields(aiYields);
	return aiYields[YIELD_PRODUCTION] / 100 * getBugOptionINT("CityScreen__ProductionOverflowLimit", 2);
}


int CvCity::getOverflowProduction() const
{
	return m_iOverflowProduction;
}


void CvCity::setOverflowProduction(int iNewValue)
{
	m_iOverflowProduction = iNewValue;
	FASSERT_NOT_NEGATIVE(m_iOverflowProduction);
}


void CvCity::changeOverflowProduction(int iChange)
{
	setOverflowProduction(m_iOverflowProduction + iChange);
}


int CvCity::getFeatureProduction() const
{
	return m_iFeatureProduction;
}


void CvCity::setFeatureProduction(int iNewValue)
{
	m_iFeatureProduction = iNewValue;
	FASSERT_NOT_NEGATIVE(getFeatureProduction());
}


void CvCity::changeFeatureProduction(int iChange)
{
	setFeatureProduction(getFeatureProduction() + iChange);
}


// ⚠ TRADE_ROUTE_AMOUNT is a FLAT slot (×100) and a trade route is a whole COUNT, so it reduces at this point of
// use -- exactly as the TRADE_ROUTE_MAX sibling below does ([DEC-fixedpoint-x100]).
int CvCity::getExtraTradeRoutes() const { return cascadeValue(MODFAM_TRADE_ROUTES, TRADE_ROUTE_AMOUNT) / 100; }


int CvCity::getMaxTradeRoutes() const
{
	if (getOwner() == NO_PLAYER)
	{
		return GC.getMAX_TRADE_ROUTES();
	}
	// The cap adjustment is a FLAT slot, so it reduces here.
	// ⚖ THE CITY'S OWN ROLL-UP, not the owner's -- the same chain totalTradeModifier reads. `realizedAtCity` folds
	// team + empire + city, so an empire-scope cap change still lands (it rolls DOWN, [modifier.md] §1) while a
	// city-scope one is no longer silently dropped. Asking the PLAYER for a value this city consumes made the two
	// trade-route reads answer off different chains for one family, which is the kind of split that stays invisible
	// until data authors the side nobody reads.
	int aiTradeRoutes[NUM_TRADE_ROUTE_KINDS];
	getTradeRouteKinds(aiTradeRoutes);
	return GC.getMAX_TRADE_ROUTES() + aiTradeRoutes[TRADE_ROUTE_MAX] / 100;
}






// The BUILDINGS' share of the defense stack, computed from the operating set rather than accumulated. It exists
// only to serve the bIgnoreBuilding what-if (defense WITHOUT this city's buildings) -- the realized read is the
// whole stack. A DORMANT building defends nothing, which the operating set already answers.
int CvCity::getBuildingDefense() const
{
	int iDefense = 0;
	const std::set<int>& kActive = m_operatingBuildings.active;
	for (std::set<int>::const_iterator it = kActive.begin(); it != kActive.end(); ++it)
	{
		iDefense += GC.getBuildingInfo((BuildingTypes)*it).getDefense(DEFENSE_AMOUNT, CASC_SCOPE_CITY);
	}
	return iDefense;
}


// The city's bombard resistance -- the WHOLE rolled stack, which is why this carries no source in its name:
// buildings author the kind at city scope and traits author it at empire, and the roll-up folds both. An
// empire-scope leg added on top of that sum would count the trait contribution twice.
// The modder option is a CEILING on the resistance, never a term in it.
int CvCity::getBombardDefense() const
{
	return std::min(GC.getGame().getModderGameOption(MODDERGAMEOPTION_MAX_BOMBARD_DEFENSE),
		cascadeDefense(DEFENSE_BOMBARD));
}

// BUG - Building Additional Bombard Defense - start
int CvCity::getAdditionalBombardDefenseByBuilding(BuildingTypes eBuilding) const
{
	FASSERT_BOUNDS(0, GC.getNumBuildingInfos(), eBuilding);

	const int iBaseDefense = getBombardDefense();

	// cap total bombard defense at 100
	return std::min(GC.getBuildingInfo(eBuilding).getDefense(DEFENSE_BOMBARD, CASC_SCOPE_CITY) + iBaseDefense, 100) - iBaseDefense;
}
// BUG - Building Additional Bombard Defense - end




int CvCity::getCurrAirlift() const
{
	return m_iCurrAirlift;
}


void CvCity::setCurrAirlift(int iNewValue)
{
	m_iCurrAirlift = iNewValue;
	FASSERT_NOT_NEGATIVE(getCurrAirlift());
}


void CvCity::changeCurrAirlift(int iChange)
{
	setCurrAirlift(getCurrAirlift() + iChange);
}


int CvCity::getMaxAirlift() const
{
	return m_iMaxAirlift;
}


void CvCity::changeMaxAirlift(int iChange)
{
	m_iMaxAirlift += iChange;
	FASSERT_NOT_NEGATIVE(getMaxAirlift());
}

int CvCity::getSMAirUnitCapacity(TeamTypes eTeam) const
{
	int iCapacity = getAirUnitCapacity(eTeam);
	iCapacity *= GC.getGame().getBaseAirUnitIncrementsbyCargoVolume();
	return iCapacity;
}

int CvCity::getAirUnitCapacity(TeamTypes eTeam) const
{
	int iCapacity = (getTeam() == eTeam ? m_iAirUnitCapacity : GC.getCITY_AIR_UNIT_CAPACITY());
	iCapacity += GET_PLAYER(getOwner()).getNationalAirUnitCapacity();
	return iCapacity;
}

void CvCity::changeAirUnitCapacity(int iChange)
{
	m_iAirUnitCapacity += iChange;
	FASSERT_NOT_NEGATIVE(getAirUnitCapacity(getTeam()));
}

int CvCity::getFreeSpecialist() const
{
	// The realized free-specialist AMOUNT -- the cascade's half of the two-part seam (modifier.md §6); the
	// engine still picks WHICH specialist each untyped slot becomes. It is the cross-scope roll-up over the
	// chain this city sits under (team + empire + city), so the empire-authored civic / trait / building slots
	// are ALREADY inside it.
	// ⚠ THE READ EDGE: the deposit is ×100 like every authored amount ([fixed-point-and-scales] §2 -- there is
	// no count exemption), and a slot count is whole, so it reduces HERE. This feeds totalFreeSpecialists ->
	// getMaxSpecialistCount, the cap the assignment fills: left scaled, a city seats 100x its specialists.
	const int iChannel = CascadeChannelRegistry::channelLookup(MODFAM_FREE_SPECIALISTS, CHANNEL_AMOUNT, -1);
	return std::max(0, InfoValuation::realizedAtCity(*this, iChannel) / 100);
}


//	Power is an AMENITY (owner ruling): `providesPower` on the grantor, folded id->COUNT onto the city
//	(json.md §8 / contexts.md). The refcount is what makes losing ONE of two power plants leave the city
//	powered -- the failure a plain counter or a bitset cannot express.
int CvCity::getPowerCount() const
{
	return m_cityContext.amenityCount(CLS_AMENITY_PROVIDES_POWER);
}


//	The UNGATED source: does a live grantor supply power here at all. This is the AMENITY's own answer, and a
//	blackout does not touch it -- two power plants are two live grantors throughout an outage.
bool CvCity::hasPowerSource() const
{
	return getPowerCount() > 0;
}


//	⚖ A STATUS IS MIDDLEWARE BETWEEN A SOURCE AND ITS TARGETS (owner): it gates what is DELIVERED, never what is
//	STORED. So a blackout leaves the amenity refcount standing and stops its value reaching the targets --
//	the same shape a city under WLTKD/disorder emitting 0 instead of its maintenance package already has
//	(economy.md: it suppresses the CONSUMPTION of the value, never its contents).
//	⚖ AND THAT IS WHY THIS VALUE EARNS AN EXPLICIT GETTER (owner), against the one-getter-per-group grammar
//	(patterns.md): a gate needs a named point to tap into, which a channel-indexed group read does not offer.
//	⛔ THIS is the definition of "powered" -- the predicate (CASC_PRED_HAS_POWER -> CityContext::isPowered), every
//	consumer, and the CROSSING the amenity fold announces all resolve through it, so a fact and a read cannot
//	disagree. A second expression of it anywhere is the drift this getter exists to prevent.
bool CvCity::isPowered() const
{
	return hasPowerSource() && !hasStatus(CITYSTATUS_POWER_DISABLED);
}



int CvCity::getDefenseDamage() const
{
	return m_iDefenseDamage;
}


void CvCity::changeDefenseDamage(int iChange)
{
	if (iChange != 0)
	{
		m_iDefenseDamage = range((m_iDefenseDamage + iChange), 0, GC.getMAX_CITY_DEFENSE_DAMAGE());

		if (iChange > 0)
		{
			setBombarded(true);
		}

		setInfoDirty(true);

		plot()->plotAction(PUF_makeInfoBarDirty);
	}
}

void CvCity::changeDefenseModifier(int iChange)
{
	if (iChange != 0)
	{
		int iTotalDefense = getTotalDefense(false);

		if (iTotalDefense > 0)
		{
			changeDefenseDamage(-(GC.getMAX_CITY_DEFENSE_DAMAGE() * iChange + (iChange > 0 ? iTotalDefense : -iTotalDefense) / 2) / iTotalDefense);
		}
	}
}


int CvCity::getLastDefenseDamage() const
{
	return m_iLastDefenseDamage;
}


void CvCity::setLastDefenseDamage(int iNewValue)
{
	m_iLastDefenseDamage = iNewValue;
}


bool CvCity::isBombardable(const CvUnit* pUnit) const
{
	if (NULL != pUnit && !pUnit->isEnemy(getTeam()))
	{
		return false;
	}

	return (getDefenseModifier(false) > getExtraMinDefense());
}


// The city's realized value for ANY (family, kind) slot, straight from the cascade. ⚖ The FLAT-vs-PERCENT verdict
// is the VOCABULARY's (infoKindUnit), never re-decided per getter -- which is the whole point of that census
// living once beside the kind enums. Every hand-named per-kind accumulator this replaces had to decide it, and a
// getter deciding it by hand is exactly how one ends up reading a leg nothing deposits into and answering 0.
int CvCity::cascadeValue(ModifierFamily eFamily, int eKind) const
{
	const int iChannel = CascadeChannelRegistry::channelLookup(eFamily, eKind, -1);
	int64_t lFlatSum = 0;
	int64_t lPercentSum = 0;
	InfoValuation::rolledLegsAtCity(*this, iChannel, lFlatSum, lPercentSum);
	return infoKindUnit(eFamily, eKind, CASC_SCOPE_CITY) == CASC_UNIT_PERCENT ? (int)lPercentSum : (int)lFlatSum;
}

int CvCity::cascadeDefense(int eKind) const { return cascadeValue(MODFAM_DEFENSE, eKind); }


// ⚖ ONE ADDITIVE STACK, like every other channel. `defense.city.amount` is a single channel that BUILDINGS and
// CULTURE LEVELS both author (153 and 18 files), so there is no natural-defense channel to weigh a building one
// against -- the legacy max(buildingDefense, naturalDefense) has no counterpart and does not survive the cut
// (validation.md: the spec leads; the behaviour change is data-led and deliberate).
// ⚑ `amount` sums like a flat and is APPLIED as a percentage, which is exactly what the `percent` unit means
// (additive deltas, summed, applied once) -- so it reads the percent leg, and it is NOT ×100.
// ⛔ bIgnoreBuilding is the what-if leg (defense WITHOUT this city's buildings); the cascade has no
// per-source subtraction, so it answers the upper-scope chain alone.
int CvCity::getTotalDefense(bool bIgnoreBuilding) const
{
	const int iChannel = CascadeChannelRegistry::channelLookup(MODFAM_DEFENSE, (int)DEFENSE_AMOUNT, -1);
	int64_t lFlatSum = 0;
	int64_t lPercentSum = 0;
	InfoValuation::rolledLegsAtCity(*this, iChannel, lFlatSum, lPercentSum);
	if (bIgnoreBuilding)
	{
		return (int)lPercentSum - getBuildingDefense();
	}
	return (int)lPercentSum;
}


int CvCity::getDefenseModifier(bool bIgnoreBuilding) const
{
	if (isOccupation())
	{
		return 0;
	}
	return (std::max(getExtraMinDefense(), ((getTotalDefense(bIgnoreBuilding) * (GC.getMAX_CITY_DEFENSE_DAMAGE() - getDefenseDamage())) / GC.getMAX_CITY_DEFENSE_DAMAGE())));
}


int CvCity::getOccupationTimer() const
{
	return m_iOccupationTimer;
}


bool CvCity::isOccupation() const
{
	return m_iOccupationTimer > 0;
}


void CvCity::setOccupationTimer(int iNewValue)
{
	FASSERT_NOT_NEGATIVE(iNewValue);

	if (m_iOccupationTimer != iNewValue)
	{
		const bool wasOccupation = m_iOccupationTimer > 0;

		m_iOccupationTimer = iNewValue;

		if (wasOccupation != isOccupation())
		{
			updateCorporation();
			updateTradeRoutes();

			updateCultureLevel(true);

			AI_setAssignWorkDirty(true);
		}
		setInfoDirty(true);
	}
}


void CvCity::changeOccupationTimer(int iChange)
{
	setOccupationTimer(m_iOccupationTimer + iChange);
}


int CvCity::getCultureUpdateTimer() const
{
	return m_iCultureUpdateTimer;
}


void CvCity::setCultureUpdateTimer(int iNewValue)
{
	m_iCultureUpdateTimer = iNewValue;
	FASSERT_NOT_NEGATIVE(getCultureUpdateTimer());
}


void CvCity::changeCultureUpdateTimer(int iChange)
{
	setCultureUpdateTimer(getCultureUpdateTimer() + iChange);
}


int CvCity::getCitySizeBoost() const
{
	return m_iCitySizeBoost;
}


void CvCity::setCitySizeBoost(int iBoost)
{
	if (getCitySizeBoost() != iBoost)
	{
		m_iCitySizeBoost = iBoost;

		setLayoutDirty(true);
	}
}

bool CvCity::isNeverLost() const
{
	return m_bNeverLost;
}


void CvCity::setNeverLost(bool bNewValue)
{
	m_bNeverLost = bNewValue;
}


bool CvCity::isBombarded() const
{
	return m_bBombarded;
}


void CvCity::setBombarded(bool bNewValue)
{
	m_bBombarded = bNewValue;
}


bool CvCity::isDrafted() const
{
	return m_bDrafted;
}


void CvCity::setDrafted(bool bNewValue)
{
	m_bDrafted = bNewValue;
}


bool CvCity::isAirliftTargeted() const
{
	return m_bAirliftTargeted;
}


void CvCity::setAirliftTargeted(bool bNewValue)
{
	m_bAirliftTargeted = bNewValue;
}


bool CvCity::isPlundered() const
{
	return m_bPlundered;
}


void CvCity::setPlundered(bool bNewValue)
{
	if (bNewValue != isPlundered())
	{
		m_bPlundered = bNewValue;

		updateTradeRoutes();
	}
}


// --- CITY STATUS (Engine/CvStatus.h) -- see CvUnit for the same shape one scope down. ---

int CvCity::getStatus(CityStatus eStatus) const
{
	FASSERT_BOUNDS(0, NUM_CITY_STATUSES, eStatus);
	return m_aiStatusTurns[eStatus];
}

bool CvCity::hasStatus(CityStatus eStatus) const
{
	return getStatus(eStatus) > 0;
}

// The ONE write path for every city status, so the HOLDS-crossing announces from exactly one place -- the tick
// below and every applier alike come through here.
void CvCity::setStatus(CityStatus eStatus, int iTurns)
{
	FASSERT_BOUNDS(0, NUM_CITY_STATUSES, eStatus);
	const bool bWasHeld = m_aiStatusTurns[eStatus] > 0;
	m_aiStatusTurns[eStatus] = std::max(0, iTurns);
	// Only the 0-CROSSING is a fact: a status ticking 5 -> 4 moves nothing a consumer gates on, and the gate IS
	// `count > 0`. The general rule for every timer-backed fact.
	if (bWasHeld == (m_aiStatusTurns[eStatus] > 0))
	{
		return;
	}
	if (m_aiStatusTurns[eStatus] > 0)
	{
		emitCityStatusAdded(getID(), getOwner(), (int)eStatus, m_aiStatusTurns[eStatus]);
	}
	else
	{
		emitCityStatusRemoved(getID(), getOwner(), (int)eStatus, 0);
	}
}

void CvCity::changeStatus(CityStatus eStatus, int iChange)
{
	setStatus(eStatus, getStatus(eStatus) + iChange);
}

void CvCity::doStatusTurn()
{
	for (int iStatus = 0; iStatus < NUM_CITY_STATUSES; ++iStatus)
	{
		if (m_aiStatusTurns[iStatus] > 0)
		{
			// Through setStatus, so the turn a status runs out announces its expiry like any other crossing.
			setStatus((CityStatus)iStatus, m_aiStatusTurns[iStatus] - 1);
		}
	}
}

bool CvCity::isWeLoveTheKingDay() const
{
	return hasStatus(CITYSTATUS_WE_LOVE_THE_KING_DAY);
}


void CvCity::setWeLoveTheKingDay(bool bNewValue)
{
	PROFILE_EXTRA_FUNC();
	if (isWeLoveTheKingDay() != bNewValue)
	{
		// A ONE-TURN status: the condition block below re-applies it every turn while it holds.
		setStatus(CITYSTATUS_WE_LOVE_THE_KING_DAY, bNewValue ? 1 : 0);

		CvPlayer& owner = GET_PLAYER(getOwner());

		CivicTypes eCivic = NO_CIVIC;

		for (int iI = 0; iI < GC.getNumCivicInfos(); iI++)
		{
			if (owner.isCivic((CivicTypes)iI))
			{
				if (!CvWString(GC.getCivicInfo((CivicTypes)iI).getWeLoveTheKingKey()).empty())
				{
					eCivic = ((CivicTypes)iI);
					break;
				}
			}
		}

		if (eCivic != NO_CIVIC)
		{
			CvWString szBuffer = gDLL->getText("TXT_KEY_CITY_CELEBRATE", getNameKey(), GC.getCivicInfo(eCivic).getWeLoveTheKingKey());
			AddDLLMessage(getOwner(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_WELOVEKING", MESSAGE_TYPE_MINOR_EVENT, ARTFILEMGR.getInterfaceArtInfo("INTERFACE_HAPPY_PERSON")->getPath(), GC.getCOLOR_WHITE(), getX(), getY(), true, true);
		}
	}
}


bool CvCity::isCitizensAutomated() const
{
	return m_bCitizensAutomated;
}


void CvCity::setCitizensAutomated(bool bNewValue)
{
	PROFILE_EXTRA_FUNC();
	if (isCitizensAutomated() != bNewValue)
	{
		m_bCitizensAutomated = bNewValue;

		if (isCitizensAutomated())
		{
			AI_assignWorkingPlots();
		}
		else
		{
			for (int iI = 0; iI < GC.getNumSpecialistInfos(); iI++)
			{
				setForceSpecialistCount(((SpecialistTypes)iI), 0);
			}
			// ⛔ RELEASING THE FORCED SPECIALISTS STRANDS THE CITIZENS THAT HELD THOSE SLOTS, so this branch has to
			// re-seat them exactly as the branch above does. `setForceSpecialistCount` only MARKS the city, and a
			// mark is not work: `AI_updateAssignWork` discards it outright while the city screen is up -- which is
			// precisely where automation is toggled -- so relying on it leaves a third of the population unassigned
			// until some later immediate assignment happens to run.
			AI_assignWorkingPlots();
		}

		if (isCitySelected())
		{
			gDLL->getInterfaceIFace()->setDirty(InfoPane_DIRTY_BIT, true);
			gDLL->getInterfaceIFace()->setDirty(CitizenButtons_DIRTY_BIT, true);
		}
	}
}


bool CvCity::isProductionAutomated() const
{
	return m_bProductionAutomated;
}

void CvCity::setProductionAutomated(bool bNewValue)
{
	if (m_bProductionAutomated != bNewValue)
	{
		m_bProductionAutomated = bNewValue;

		if (bNewValue)
		{
			clearOrderQueue();
			AI_chooseProduction();
			gDLL->getInterfaceIFace()->setDirty(InfoPane_DIRTY_BIT, true);
		}
	}
}


bool CvCity::isWallOverride() const
{
	return m_bWallOverride;
}


void CvCity::setWallOverride(bool bOverride)
{
	if (isWallOverride() != bOverride)
	{
		m_bWallOverride = bOverride;

		setLayoutDirty(true);
	}
}


bool CvCity::isInfoDirty() const
{
	return m_bInfoDirty;
}


void CvCity::setInfoDirty(bool bNewValue)
{
	m_bInfoDirty = bNewValue;
}


bool CvCity::isLayoutDirty() const
{
	return m_bLayoutDirty;
}


void CvCity::setLayoutDirty(bool bNewValue)
{
	m_bLayoutDirty = bNewValue;
}


PlayerTypes CvCity::getPreviousOwner() const
{
	return m_ePreviousOwner;
}


void CvCity::setPreviousOwner(PlayerTypes eNewValue)
{
	m_ePreviousOwner = eNewValue;
}


PlayerTypes CvCity::getOriginalOwner() const
{
	return m_eOriginalOwner;
}


void CvCity::setOriginalOwner(PlayerTypes eNewValue)
{
	m_eOriginalOwner = eNewValue;
}


TeamTypes CvCity::getTeam() const
{
	return GET_PLAYER(getOwner()).getTeam();
}


CultureLevelTypes CvCity::getCultureLevel() const
{
	return m_eCultureLevel;
}

int CvCity::getCultureThreshold() const
{
	PROFILE_EXTRA_FUNC();
	const GameSpeedTypes eSpeed = GC.getGame().getGameSpeedType();
	const int64_t iCulture = getCultureTimes100(getOwner()) / 100;
	const int iNumCultureLevels = GC.getNumCultureLevelInfos();

	for (int i = 0; i < iNumCultureLevels; i++)
	{
		const CvCultureLevelInfo& info = GC.getCultureLevelInfo(static_cast<CultureLevelTypes>(i));

		if (info.getLevel() > -1 && iCulture < info.getSpeedThreshold(eSpeed))
		{
			return info.getSpeedThreshold(eSpeed);
		}
	}
	return -1;
}


void CvCity::setCultureLevel(CultureLevelTypes eNewValue, bool bUpdatePlotGroups)
{
	PROFILE_FUNC();

	if (m_eCultureLevel == eNewValue)
	{
		return;
	}
	const CultureLevelTypes eOldValue = m_eCultureLevel;

	// The commit and the fact; then this setter's own EFFECTS below. Culture level is a cascade input (wonder
	// caps, defense, enabler frontier) AND the city's workable RADIUS grows with it -- so this ONE fact is also
	// the vicinity-MEMBERSHIP signal, the city gaining or losing plots into its vicinity.
	setCultureLevelInternal(eNewValue);

	// Culture level change can change our radius requiring recalculation of best builds
	AI_markBestBuildValuesStale();

	// Border expansion alert
	if (GC.getGame().isFinalInitialized() && eNewValue > eOldValue && eNewValue > 1)
	{
		{
			AddDLLMessage(
				getOwner(), false, GC.getEVENT_MESSAGE_TIME(),
				gDLL->getText("TXT_KEY_MISC_BORDERS_EXPANDED", getNameKey(), GC.getCultureLevelInfo(eNewValue).getTextKeyWide()),
				"AS2D_CULTUREEXPANDS", MESSAGE_TYPE_MINOR_EVENT,
				GC.getCommerceInfo(COMMERCE_CULTURE).getButton(),
				GC.getCOLOR_WHITE(), getX(), getY(), true, true
			);
		}
		// Afforess - Update Health and Happiness when culture expands

		// Alert people if max culture level acquired in a known city
		if (getCultureThreshold() == -1)
		{
			for (int iI = 0; iI < MAX_PC_PLAYERS; iI++)
			{
				if (GET_PLAYER((PlayerTypes)iI).isAlive() && GET_PLAYER((PlayerTypes)iI).isHumanPlayer())
				{
					if (isRevealed(GET_PLAYER((PlayerTypes)iI).getTeam(), false))
					{
						AddDLLMessage(
							(PlayerTypes)iI, false, GC.getEVENT_MESSAGE_TIME(),
							gDLL->getText(
								"TXT_KEY_MISC_CULTURE_LEVEL",
								getNameKey(),
								GC.getCultureLevelInfo(eNewValue).getTextKeyWide()
							),
							"AS2D_CULTURELEVEL", MESSAGE_TYPE_MAJOR_EVENT,
							GC.getCommerceInfo(COMMERCE_CULTURE).getButton(),
							GC.getCOLOR_HIGHLIGHT_TEXT(), getX(), getY(), true, true
						);
					}
					else
					{
						AddDLLMessage(
							(PlayerTypes)iI, false, GC.getEVENT_MESSAGE_TIME(),
							gDLL->getText(
								"TXT_KEY_MISC_CULTURE_LEVEL_UNKNOWN",
								GC.getCultureLevelInfo(eNewValue).getTextKeyWide()
							),
							"AS2D_CULTURELEVEL", MESSAGE_TYPE_MAJOR_EVENT,
							GC.getCommerceInfo(COMMERCE_CULTURE).getButton(), GC.getCOLOR_HIGHLIGHT_TEXT()
						);
					}
				}
			}
		}

		// ONEVENT - Culture growth
		CvEventReporter::getInstance().cultureExpansion(this, getOwner());

		//Stop Build Culture
		if (isHuman() && !isProductionAutomated() && isProductionProcess()
		&& GC.getProcessInfo(getProductionProcess()).getProductionToCommerce(COMMERCE_CULTURE, CASC_SCOPE_CITY) > 0)
		{
			m_bPopProductionProcess = true;
		}
	}
	AI_updateBestBuild();
}

// Stores level as indexed by values in xml.
void CvCity::updateCultureLevel(bool bUpdatePlotGroups)
{
	PROFILE_EXTRA_FUNC();
	if (getCultureUpdateTimer() > 0)
	{
		return;
	}
	CvGameAI& GAME = GC.getGame();

	if (!isOccupation())
	{
		const GameSpeedTypes eSpeed = GAME.getGameSpeedType();
		const int64_t iCulture = getCultureTimes100(getOwner()) / 100;

		// Will set culture level to that indexed by xml, but only if matches option of current game
		for (int iI = GC.getNumCultureLevelInfos() - 1; iI > 0; iI--)
		{
			const CvCultureLevelInfo& info = GC.getCultureLevelInfo(static_cast<CultureLevelTypes>(iI));

			if (info.getLevel() > -1 && iCulture >= info.getSpeedThreshold(eSpeed))
			{
				setCultureLevel(static_cast<CultureLevelTypes>(iI), bUpdatePlotGroups);
				return;
			}
		}
	}
	setCultureLevel((CultureLevelTypes)0, bUpdatePlotGroups);
}



/*
 * Returns the total additional yield that adding one of the given buildings will provide.
 *
 * Doesn't check if the building can be constructed in this city.
 */
int CvCity::getAdditionalYieldByBuilding(YieldTypes eIndex, BuildingTypes eBuilding, bool /*bFilter*/) const
{
	PROFILE_EXTRA_FUNC();

	//	TWO CALLS, one per system -- ask each machine only what it owns (north-star: whose job is this?).
	//
	//	1. WHAT  -- the ENABLER answers which ACTIVE buildings this candidate would send dormant. Supersession is
	//	   an availability fact, and it is authored on the SUPERSEDED building (`requires.operate.dormant` names
	//	   its successor -- enabler.md §2), never as a "what I replace" list on the candidate.
	//	2. VALUE -- the VALUATION answers what each one is worth AGAINST THIS CITY, contexts in / delta out
	//	   (patterns.md § THE VALUATION PROTOCOL). The candidate's gain minus what the superseded stop giving.
	//
	//	The hand-assembled `(base + add) * (mod + addMod) - base * mod` combine this replaces is gone with the
	//	base tier it read: the valuation resolves the percent side against the live context itself.
	const CvPlayer& kOwner = GET_PLAYER(getOwner());
	const CvPlotGroup* pPlotGroup = plotGroup(getOwner());

	int aiCandidate[NUM_YIELD_TYPES];
	GC.getBuildingInfo(eBuilding).expectedFlatYields(
		getCityContext(), kOwner.getEmpireContext(), pPlotGroup, aiCandidate);
	int iDelta = aiCandidate[eIndex];

	std::vector<int> kSuperseded;
	EnablerKernel::dormedByBuilding(this, (int)eBuilding, kSuperseded);
	for (size_t iI = 0; iI < kSuperseded.size(); ++iI)
	{
		int aiDormed[NUM_YIELD_TYPES];
		GC.getBuildingInfo((BuildingTypes)kSuperseded[iI]).expectedFlatYields(
			getCityContext(), kOwner.getEmpireContext(), pPlotGroup, aiDormed);
		iDelta -= aiDormed[eIndex];
	}
	return iDelta / 100;   // ÷100 at the reader ([DEC-fixedpoint-x100])
}

int CvCity::getYieldBySpecialist(YieldTypes eIndex, SpecialistTypes eSpecialist) const
{
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, eIndex);
	FASSERT_BOUNDS(0, GC.getNumSpecialistInfos(), eSpecialist);
	return (
		GC.getSpecialistInfo(eSpecialist).getFlatYield(eIndex, CASC_SCOPE_CITY) / 100
		+ GET_PLAYER(getOwner()).getExtraSpecialistYield(eSpecialist, eIndex)
	);
}

// note: player->invalidateYieldRankCache() must be called for anything that is checked here
// so if any extra checked things are added here, the cache needs to be invalidated
// ⚖ ONE ADDITIVE STACK (modifier.md §2a). The legacy form kept the same sum in SEVEN hand-named accumulators
// (bonus / building / event / player / power / area / capital) and added them here; addition is associative, so
// the split changed nothing the result could see -- it only meant seven serialized stores to drift. The cascade
// carries them as ordinary `<yield>.<scope>.percent` deposits, and the ONE cross-scope roll-up folds the city's
// whole chain (team + empire + area×owner + city) with the per-city gates applied at the combine.
// ⛔ The power / area / capital LEGS are not lost: they are ordinary conditioned deposits (HAS_POWER, IS_CAPITAL,
// the empire-scope area authorings), evaluated by the ONE evaluator rather than re-tested here.
int CvCity::getBaseYieldRateModifier(YieldTypes eIndex, int iExtra) const
{
	const int iChannel = CascadeChannelRegistry::channelLookup(infoYieldFamily(eIndex), (int)CHANNEL_AMOUNT, -1);
	int64_t lFlatSum = 0;
	int64_t lPercentSum = 0;
	InfoValuation::rolledLegsAtCity(*this, iChannel, lFlatSum, lPercentSum);
	return std::max(0, 100 + iExtra + (int)lPercentSum);
}

void CvCity::onYieldChange()
{
#ifdef YIELD_VALUE_CACHING
	ClearYieldValueCache();
#endif

	if (getTeam() == GC.getGame().getActiveTeam())
	{
		setInfoDirty(true);
	}
	if (isCitySelected())
	{
		gDLL->getInterfaceIFace()->setDirty(CityScreen_DIRTY_BIT, true);
		gDLL->getInterfaceIFace()->setDirty(InfoPane_DIRTY_BIT, true);
	}
}

int CvCity::getYieldRateModifier(YieldTypes eIndex)	const
{
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, eIndex);
	return m_aiYieldRateModifier[eIndex];
}


void CvCity::changeYieldRateModifier(YieldTypes eIndex, int iChange)
{
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, eIndex);

	if (iChange != 0)
	{
		m_aiYieldRateModifier[eIndex] += iChange;

		GET_PLAYER(getOwner()).invalidateYieldRankCache(eIndex);

		if (eIndex == YIELD_COMMERCE)
		{
		}

		AI_setAssignWorkDirty(true);

		if (getTeam() == GC.getGame().getActiveTeam())
		{
			setInfoDirty(true);
		}
	}
}


void CvCity::setTradeYield(YieldTypes eIndex, int iNewValue)
{
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, eIndex);

	const int iOldValue = m_aiTradeYield[eIndex];

	if (iOldValue != iNewValue)
	{
		m_aiTradeYield[eIndex] = iNewValue;
		FASSERT_NOT_NEGATIVE(m_aiTradeYield[eIndex]);
	}
}

// ×100, like every other amount -- the engine's trade-network OUTPUT, folded into TIER-1 BASE by the combine
// ([modifier.md] §2a). Derived, so it is never serialized; `CvGame::onFinalInitialized` rebuilds it at load.
int CvCity::getTradeYield(YieldTypes eIndex) const
{
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, eIndex);
	return m_aiTradeYield[eIndex];
}


int CvCity::totalTradeModifier(const CvCity* pOtherCity) const
{
	PROFILE_EXTRA_FUNC();
	int iModifier = 100;

	// The channel-agnostic route-PROFIT stage in ONE read: the roll-up over the chain this city sits under
	// (team + empire + city), which is what the two plain accumulators were separately accumulating.
	int aiTradeRoutes[NUM_TRADE_ROUTE_KINDS];
	getTradeRouteKinds(aiTradeRoutes);
	iModifier += aiTradeRoutes[TRADE_ROUTE_MODIFIER];
	iModifier += getPopulationTradeModifier();

	if (isConnectedToCapital())
	{
		iModifier += GC.getCAPITAL_TRADE_MODIFIER();
	}

	if (NULL != pOtherCity)
	{
		if (area() != pOtherCity->area())
		{
			iModifier += GC.getOVERSEAS_TRADE_MODIFIER();
		}

		if (getTeam() != pOtherCity->getTeam())
		{
			// ⚠ The SHARED-CIVIC route bonus is authored as a CONDITIONED `tradeRoutes.empire.modifier.percent`
			// entry gated `all: [IS_FOREIGN, SHARES_CIVIC]` (curate_civic), and both predicates are evaluated
			// against the ROUTE'S PARTNER ([json.md §3.5]) -- an axis CvCascadeEvalCtx carries no slot for, so
			// nothing can evaluate them yet. The group read above serves the UNCONDITIONED sum only.
			// ⛔ It is left UNSERVED rather than re-summed unconditionally here: folding a conditioned entry in
			// without its gate pays the bonus on every foreign route, to partners running different civics
			// ([modifier.md §5] -- honestly unserved is the correct exposed state).
			// MISSING MACHINE: the route-partner axis on the eval ctx, which the IS_FOREIGN / SHARES_CIVIC
			// predicates read.
			iModifier += getPeaceTradeModifier(pOtherCity->getTeam());
		}
	}

	return iModifier;
}

int CvCity::getPopulationTradeModifier() const
{
	return std::max(0, (getPopulation() + GC.getOUR_POPULATION_TRADE_MODIFIER_OFFSET()) * GC.getOUR_POPULATION_TRADE_MODIFIER());
}

int CvCity::getPeaceTradeModifier(TeamTypes eTeam) const
{
	FASSERT_BOUNDS(0, MAX_TEAMS, eTeam);
	FAssert(eTeam != getTeam());

	if (atWar(eTeam, getTeam()))
	{
		return 0;
	}

	const int iPeaceTurns = std::min(GC.getFOREIGN_TRADE_FULL_CREDIT_PEACE_TURNS(), GET_TEAM(getTeam()).AI_getAtPeaceCounter(eTeam));

	if (GC.getGame().getElapsedGameTurns() <= iPeaceTurns)
	{
		return GC.getFOREIGN_TRADE_MODIFIER();
	}

	return ((GC.getFOREIGN_TRADE_MODIFIER() * iPeaceTurns) / std::max(1, GC.getFOREIGN_TRADE_FULL_CREDIT_PEACE_TURNS()));
}

int CvCity::getBaseTradeProfit(const CvCity* pCity) const
{
	int iProfit = std::min(pCity->getPopulation() * GC.getTHEIR_POPULATION_TRADE_PERCENT(), plotDistance(getX(), getY(), pCity->getX(), pCity->getY()) * GC.getWorldInfo(GC.getMap().getWorldSize()).getTradeProfitPercent());

	iProfit *= GC.getTRADE_PROFIT_PERCENT();
	iProfit /= 100;

	iProfit = std::max(100, iProfit);

	return iProfit;
}

// The profit one route carries, ×100 like every other amount. `getBaseTradeProfit` is already ×100 and
// `totalTradeModifier` is a plain percent, so the single `/100` here APPLIES the percentage -- it is not a scale
// reduce, and there is deliberately no human twin beside it ([DEC-fixedpoint-x100]: no getter has a ×100
// variant, and no name carries the scale). A reader wanting whole gold divides at its own point of use.
int CvCity::calculateTradeProfit(const CvCity* pCity) const
{
	int iProfit = getBaseTradeProfit(pCity);

	iProfit *= totalTradeModifier(pCity);
	iProfit /= 100;

	return iProfit;
}

int CvCity::calculateTradeYield(YieldTypes eIndex, int iTradeProfit) const
{
	if (iTradeProfit != 0 && GET_PLAYER(getOwner()).getTradeYieldModifier(eIndex) > 0)
	{
		return iTradeProfit * GET_PLAYER(getOwner()).getTradeYieldModifier(eIndex) / 100;
	}
	return 0;
}

// BUG - Trade Totals - start
/*
 * Adds the yield and count for each trade route with eWithPlayer.
 *
 * The yields accumulate ×100 and the counts are plain counts; neither is reset to zero, so a caller may sum
 * several cities into one pair and reduce ONCE at the surface that shows it ([fixed-point-and-scales.md §4c-bis]).
 * `bBase` asks for the unmodified route profit instead of the channel's yield.
 */
void CvCity::calculateTradeTotals(YieldTypes eIndex, int& iDomesticYield, int& iDomesticRoutes, int& iForeignYield, int& iForeignRoutes, PlayerTypes eWithPlayer, bool bBase) const
{
	PROFILE_EXTRA_FUNC();
	if (!isDisorder())
	{
		int iCityDomesticYield = 0;
		int iCityDomesticRoutes = 0;
		int iCityForeignYield = 0;
		int iCityForeignRoutes = 0;
		const int iNumTradeRoutes = getTradeRoutes();
		const PlayerTypes ePlayer = getOwner();

		for (int iI = 0; iI < iNumTradeRoutes; ++iI)
		{
			const CvCity* pTradeCity = getTradeCity(iI);
			if (pTradeCity && pTradeCity->getOwner() >= 0 && (NO_PLAYER == eWithPlayer || pTradeCity->getOwner() == eWithPlayer))
			{
				int iTradeYield;

				if (bBase)
				{
					iTradeYield = getBaseTradeProfit(pTradeCity);
				}
				else
				{
					const int iTradeProfit = calculateTradeProfit(pTradeCity);
					// ⛔ THE CHANNEL IS THE ONE ASKED FOR, NOT COMMERCE. This read `YIELD_COMMERCE` outright and
					// ignored `eIndex`, so the function promised a per-yield total and answered commerce for every
					// channel -- ask it for FOOD and it returned the commerce number.
					// ⚑ It survived because both C++ callers happen to pass YIELD_COMMERCE, so the wrong answer was
					// never the wrong VALUE there; any other caller (a per-yield route list) silently got commerce
					// beside a food tooltip that had been computed properly, and the two could not reconcile.
					// ⚠ The per-channel modifier is exactly what makes this visible now: food and production carry
					// their own route-yield percentages ([modifier.md] §2a), so the channels genuinely differ where
					// legacy had commerce-only routes and the substitution was invisible.
					iTradeYield = calculateTradeYield(eIndex, iTradeProfit);
				}

				if (pTradeCity->getOwner() == ePlayer)
				{
					iCityDomesticYield += iTradeYield;
					iCityDomesticRoutes++;
				}
				else
				{
					iCityForeignYield += iTradeYield;
					iCityForeignRoutes++;
				}
			}
		}

		// ⛔ THE YIELDS STAY ×100 AND THE COUNTS STAY COUNTS. Callers sum several cities into one pair, so a
		// reduce here would be `Σ trunc(x)` over an aggregation that is still being built -- the amount comes
		// down once, at the surface that shows it ([fixed-point-and-scales.md §4c-bis]). ⚠ And a ROUTE COUNT is
		// not a ×100 amount: reducing it alongside the yields zeroes any count below a hundred routes.
		iDomesticYield += iCityDomesticYield;
		iDomesticRoutes += iCityDomesticRoutes;
		iForeignYield += iCityForeignYield;
		iForeignRoutes += iCityForeignRoutes;
	}
}
// BUG - Trade Totals - end


// ⚖ ONE ADDITIVE STACK, the commerce twin of getBaseYieldRateModifier (modifier.md §2a). The legacy form summed
// SEVEN accumulators and then SUBTRACTED two of them back out again -- the events/buildings terms were folded
// into the player's generic modifier AND tracked separately for the UI, so the read had to undo its own
// double-count. That dedup disappears with the split: the cascade keeps ONE accumulator per channel per scope.
// ⛔ The hand-rolled MIN_INT dirty cache goes with it: a read is a bare fetch over the package, and the package
// is invalidated by the spine ([DEC-uniform-cache-shape]) -- never a second dirty protocol beside it.
int CvCity::getTotalCommerceRateModifier(CommerceTypes eIndex) const
{
	const int iChannel = CascadeChannelRegistry::channelLookup(infoCommerceFamily(eIndex), (int)CHANNEL_AMOUNT, -1);
	int64_t lFlatSum = 0;
	int64_t lPercentSum = 0;
	InfoValuation::rolledLegsAtCity(*this, iChannel, lFlatSum, lPercentSum);
	return std::max(1, 100 + (int)lPercentSum);
}


// The modifier's own dirty flag is gone with the hand-rolled cache above; what remains is the ordinary
// commerce-dirty signal the rate read still uses.

int CvCity::getProductionToCommerceModifier(CommerceTypes eIndex) const
{
	FASSERT_BOUNDS(0, NUM_COMMERCE_TYPES, eIndex);
	return m_aiProductionToCommerceModifier[eIndex];
}


void CvCity::changeProductionToCommerceModifier(CommerceTypes eIndex, int iChange)
{
	FASSERT_BOUNDS(0, NUM_COMMERCE_TYPES, eIndex);

	if (iChange != 0)
	{
		m_aiProductionToCommerceModifier[eIndex] += iChange;

		gDLL->getInterfaceIFace()->setDirty(GameData_DIRTY_BIT, true);
	}
}


/*
 * Returns the total additional commerce times 100 that adding one of the given buildings will provide.
 *
 * Doesn't check if the building can be constructed in this city.
 */
int CvCity::getAdditionalCommerceByBuilding(CommerceTypes eIndex, BuildingTypes eBuilding) const
{
	PROFILE_EXTRA_FUNC();
	// THE VALUATION PROTOCOL ([patterns.md]): the live CONTEXTS go in and the resolved DELTA comes out. The
	// candidate's own compiled entries answer what it would add -- the hand-assembled without/with subtraction
	// over a base-rate decomposition is gone with the tier it read.
	const CvPlayer& kOwner = GET_PLAYER(getOwner());
	const CvPlotGroup* pPlotGroup = plotGroup(getOwner());

	int aiCandidate[NUM_COMMERCE_TYPES];
	GC.getBuildingInfo(eBuilding).expectedFlatCommerce(
		getCityContext(), kOwner.getEmpireContext(), pPlotGroup, aiCandidate);
	int iDelta = aiCandidate[eIndex];

	// A candidate that dorms an incumbent nets that incumbent's contribution back out; the ENABLER owns which
	// buildings it supersedes ([DEC-enabler-not-cascade]).
	std::vector<int> kSuperseded;
	EnablerKernel::dormedByBuilding(this, (int)eBuilding, kSuperseded);
	for (size_t iI = 0; iI < kSuperseded.size(); ++iI)
	{
		int aiDormed[NUM_COMMERCE_TYPES];
		GC.getBuildingInfo((BuildingTypes)kSuperseded[iI]).expectedFlatCommerce(
			getCityContext(), kOwner.getEmpireContext(), pPlotGroup, aiDormed);
		iDelta -= aiDormed[eIndex];
	}
	return iDelta;
}

/*
 * Returns the additional base commerce rate constructing the given building will provide.
 *
 * Doesn't check if the building can be constructed in this city.
 */
/*
 * Returns the additional commerce rate modifier constructing the given building will provide.
 *
 * Doesn't check if the building can be constructed in this city.
 */
int CvCity::getAdditionalCommerceRateModifierByBuilding(CommerceTypes eIndex, BuildingTypes eBuilding) const
{
	PROFILE_EXTRA_FUNC();
	FASSERT_BOUNDS(0, NUM_COMMERCE_TYPES, eIndex);
	FASSERT_BOUNDS(0, GC.getNumBuildingInfos(), eBuilding);

	if (isDormantBuilding(eBuilding))
	{
		return 0;
	}
	const CvBuildingInfo& kBuilding = GC.getBuildingInfo(eBuilding);

	// ONE valuation: the compiled unconditioned sums with every scope folded into the experienced-here
	// answer, plus the conditioned tail resolved against THIS city ([patterns.md] THE GETTER SETUP read 3).
	// The tech-gated and bonus-gated rows are ordinary conditioned entries, so the two walks this replaces
	// were re-deriving by hand what the evaluator already answers. A percent is not scaled.
	return kBuilding.expectedModifier(
		infoCommerceFamily(eIndex), CHANNEL_AMOUNT, CASC_UNIT_PERCENT,
		getCityContext(), GET_PLAYER(getOwner()).getEmpireContext(), plotGroup(getOwner()));
}
// BUG - Building Additional Commerce - end


// Returns the total additional commerce that changing the number of given specialists will provide/remove.
int CvCity::getAdditionalCommerceBySpecialist(CommerceTypes eIndex, SpecialistTypes eSpecialist, int iChange) const
{
	int iExtraRate = getAdditionalBaseCommerceRateBySpecialist(eIndex, eSpecialist, iChange);
	if (iExtraRate == 0)
	{
		return 0;
	}
	return iExtraRate * getTotalCommerceRateModifier(eIndex);
}

// Returns the additional base commerce rate that changing the number of given specialists will provide/remove.
int CvCity::getAdditionalBaseCommerceRateBySpecialist(CommerceTypes eIndex, SpecialistTypes eSpecialist, int iChange) const
{
	FASSERT_BOUNDS(0, NUM_COMMERCE_TYPES, eIndex);
	FASSERT_BOUNDS(0, GC.getNumSpecialistInfos(), eSpecialist);
	return (
		iChange * (
			GC.getSpecialistInfo(eSpecialist).getFlatCommerce(eIndex, CASC_SCOPE_CITY) / 100
		)
	);
}





// XXX can this be simplified???
void CvCity::updateCorporation()
{
	PROFILE_EXTRA_FUNC();
	updateCorporationBonus();
}


void CvCity::updateCorporationBonus()
{
	PROFILE_EXTRA_FUNC();

	m_corpBonusProduction.clear();
	const int iNumBonuses = GC.getNumBonusInfos();
	bool* abHadBonus = new bool[iNumBonuses];
	int* aiLastCorpProducedBonus = new int[iNumBonuses];
	int* aiExtraCorpProducedBonus = new int[iNumBonuses];

	for (int iI = 0; iI < iNumBonuses; ++iI)
	{
		abHadBonus[iI] = hasBonus((BonusTypes)iI);
		aiLastCorpProducedBonus[iI] = getNumBonuses((BonusTypes)iI);
		aiExtraCorpProducedBonus[iI] = 0;
	}

	for (int iIter = 0; iIter < GC.getNumCorporationInfos(); ++iIter)
	{
		for (int iCorp = 0; iCorp < GC.getNumCorporationInfos(); ++iCorp)
		{
			// what a corporation SUPPLIES is its `provides.bonuses` (json §5a); the legacy single-FK member
			// is that list, which no shipped corp authors more than one entry of.
			const CvProvides* pProvides = GC.getCorporationInfo((CorporationTypes)iCorp).getProvides();
			const int iBonusProduced = (pProvides != NULL && !pProvides->bonuses.empty()) ? pProvides->bonuses[0] : -1;

			if (-1 < iBonusProduced
			&& !GET_TEAM(getTeam()).isBonusObsolete((BonusTypes)iBonusProduced)
			&& GET_TEAM(getTeam()).isHasTech((TechTypes)GC.getBonusInfo((BonusTypes)iBonusProduced).getTechCityTrade())
			&& isHasCorporation((CorporationTypes)iCorp) && GET_PLAYER(getOwner()).isActiveCorporation((CorporationTypes)iCorp))
			{
				bool bConsumes = false;

				foreach_(const int iConsumedBonus, GC.getCorporationInfo((CorporationTypes)iCorp).getConsumedBonuses())
				{
					const BonusTypes eBonusConsumed = static_cast<BonusTypes>(iConsumedBonus);
					if (eBonusConsumed != iBonusProduced) // ignore circular xml definiton error.
					{
						bConsumes = true;
						aiExtraCorpProducedBonus[iBonusProduced] += aiLastCorpProducedBonus[eBonusConsumed];
					}
				}
				if (iIter == 0 && !bConsumes) // Only handle this conditionless production once.
				{
					aiExtraCorpProducedBonus[iBonusProduced] += 1;
				}
			}
		}

		bool bChanged = false;
		for (int iI = 0; iI < iNumBonuses; ++iI)
		{
			if (aiExtraCorpProducedBonus[iI] != 0)
			{
				bool bFirst = true;
				for (std::vector< std::pair<BonusTypes, int> >::iterator it = m_corpBonusProduction.begin(); it != m_corpBonusProduction.end(); ++it)
				{
					if ((*it).first == iI)
					{
						(*it).second += aiExtraCorpProducedBonus[iI];
						bFirst = false;
					}
				}
				if (bFirst) m_corpBonusProduction.push_back(std::make_pair(static_cast<BonusTypes>(iI), aiExtraCorpProducedBonus[iI]));

				bChanged = true; // The produced bonus might be consumed by another corp to produce another bonus,
				//	which means we need to loop iIter to check for and handle that case.
			}
			aiLastCorpProducedBonus[iI] = aiExtraCorpProducedBonus[iI];
			aiExtraCorpProducedBonus[iI] = 0;
		}
		if (!bChanged)
		{
			break;
		}
	}

	for (int iI = 0; iI < iNumBonuses; ++iI)
	{
		if (abHadBonus[iI] != hasBonus((BonusTypes)iI))
		{
			if (abHadBonus[iI])
			{
				processBonus((BonusTypes)iI, -1);
			}
			else processBonus((BonusTypes)iI, 1);
		}
	}
	SAFE_DELETE_ARRAY(abHadBonus);
	SAFE_DELETE_ARRAY(aiLastCorpProducedBonus);
	SAFE_DELETE_ARRAY(aiExtraCorpProducedBonus);
}

// The commerce-rate percent an EVENT granted this city. Every DERIVABLE source rolls up through the cascade
// (InfoValuation::rolledLegsAtCity); this answers only the one-shot grants, which nothing can recompute.
int CvCity::getCommerceRateModifier(CommerceTypes eIndex) const
{
	FASSERT_BOUNDS(0, NUM_COMMERCE_TYPES, eIndex);
	return m_eventGrants.sum(EVENTGRANT_COMMERCE_RATE_MODIFIER, eIndex, -1);
}


void CvCity::recordCommerceRateModifierGrant(EventTypes eEvent, CommerceTypes eIndex, int iChange)
{
	FASSERT_BOUNDS(0, NUM_COMMERCE_TYPES, eIndex);

	if (iChange != 0)
	{
		m_eventGrants.add(EVENTGRANT_COMMERCE_RATE_MODIFIER, eEvent, eIndex, -1, iChange);


		AI_setAssignWorkDirty(true);
	}
}


// ⚑ The `< 0 ? MAX_INT` saturating guards these two getters used to carry were the FOSSIL of a live 32-bit
// overflow: city culture accumulates the realized culture commerce every turn and NEVER decays, so on a
// long game it wrapped negative and the guards detected the wrap and clamped. That silently corrupted every
// consumer of the value -- culture percent, cultural ownership, the level thresholds -- because a saturated
// total is not the total. The storage is 64-bit now, so there is no wrap to detect and nothing to clamp.
int64_t CvCity::getCulture(PlayerTypes eIndex) const
{
	FASSERT_BOUNDS(0, MAX_PLAYERS, eIndex);
	return m_aiCulture[eIndex] / 100;
}

int64_t CvCity::getCultureTimes100(PlayerTypes eIndex) const
{
	FASSERT_BOUNDS(0, MAX_PLAYERS, eIndex);
	return m_aiCulture[eIndex];
}


int64_t CvCity::countTotalCultureTimes100() const
{
	PROFILE_EXTRA_FUNC();
	int64_t iTotalCulture = 0;

	for (int iI = 0; iI < MAX_PLAYERS; iI++)
	{
		if (GET_PLAYER((PlayerTypes)iI).isAlive())
		{
			iTotalCulture += getCultureTimes100((PlayerTypes)iI);
		}
	}
	return iTotalCulture;
}


PlayerTypes CvCity::findHighestCulture() const
{
	PROFILE_EXTRA_FUNC();
	int64_t iBestValue = 0;
	PlayerTypes eBestPlayer = NO_PLAYER;

	for (int iI = 0; iI < MAX_PLAYERS; iI++)
	{
		if (GET_PLAYER((PlayerTypes)iI).isAlive())
		{
			const int64_t iValue = getCultureTimes100((PlayerTypes)iI);

			if (iValue > iBestValue)
			{
				iBestValue = iValue;
				eBestPlayer = ((PlayerTypes)iI);
			}
		}
	}
	return eBestPlayer;
}


int CvCity::calculateCulturePercent(PlayerTypes eIndex) const
{
	const int64_t iTotalCulture = countTotalCultureTimes100();

	if (iTotalCulture > 0)
	{
		// A PERCENT is the answer, so it stays int -- but the operands must not narrow on the way in.
		return (int)(getCultureTimes100(eIndex) * 100 / iTotalCulture);
	}
	return 0;
}


int CvCity::calculateTeamCulturePercent(TeamTypes eIndex) const
{
	PROFILE_EXTRA_FUNC();
	int iTeamCulturePercent = 0;

	for (int iI = 0; iI < MAX_PLAYERS; iI++)
	{
		if (GET_PLAYER((PlayerTypes)iI).isAliveAndTeam(eIndex))
		{
			iTeamCulturePercent += calculateCulturePercent((PlayerTypes)iI);
		}
	}
	return iTeamCulturePercent;
}


// The saturating `> MAX_INT / 100 ? MAX_INT` clamp is gone with the overflow it existed for -- 64-bit storage
// has nothing to saturate against, and a clamped culture total is not the total.
void CvCity::setCulture(PlayerTypes eIndex, int64_t iNewValue, bool bPlots, bool bUpdatePlotGroups, bool bNationalSet)
{
	setCultureTimes100(eIndex, 100 * iNewValue, bPlots, bUpdatePlotGroups, bNationalSet);
}

void CvCity::setCultureTimes100(PlayerTypes eIndex, int64_t iNewValue, bool bPlots, bool bUpdatePlotGroups, bool bNationalSet)
{
	FASSERT_BOUNDS(0, MAX_PLAYERS, eIndex);

	const int64_t iOldCulture = getCultureTimes100(eIndex);

	if (iOldCulture != iNewValue)
	{
		FASSERT_NOT_NEGATIVE(iNewValue);

		if (iNewValue < iOldCulture || getCultureThreshold() > -1)
		{
			m_aiCulture[eIndex] = iNewValue;

			updateCultureLevel(bUpdatePlotGroups);

			if (getCultureThreshold() == -1)
			{
				// Toffer - Cap value at max culture threshold
				m_aiCulture[eIndex] = 100 * GC.getGame().getCultureThreshold(getCultureLevel());
			}
		}
		if (bPlots && iNewValue > iOldCulture)
		{
			// A per-turn RATE, not an accumulation -- so it stays int and reduces here, at the point of use.
			doPlotCulture(eIndex, static_cast<int>((iNewValue - iOldCulture) / 100));
		}
	}

	if (!bNationalSet && iOldCulture < iNewValue)
	{
		GET_PLAYER(getOwner()).changeCulture((iNewValue - iOldCulture) / 100);
	}
}


void CvCity::changeCulture(PlayerTypes eIndex, int64_t iChange, bool bPlots, bool bUpdatePlotGroups)
{
	changeCultureTimes100(eIndex, 100 * iChange, bPlots, bUpdatePlotGroups);
}

void CvCity::changeCultureTimes100(PlayerTypes eIndex, int64_t iChange, bool bPlots, bool bUpdatePlotGroups)
{
	if (iChange == 0) return;

	const int64_t iOld = getCultureTimes100(eIndex);

	if (iChange > 99)
	{
		GET_PLAYER(getOwner()).changeCulture(iChange / 100);
	}

	int64_t iNew;
	if (iChange < 0)
	{
		iNew = std::max<int64_t>(0, iOld + iChange);
	}
	else if (LLONG_MAX - iChange > iOld)
	{
		iNew = iOld + iChange;
	}
	else iNew = LLONG_MAX;

	setCultureTimes100(eIndex, iNew, bPlots, bUpdatePlotGroups, true);
}


int CvCity::getNumRevolts(PlayerTypes eIndex) const
{
	FASSERT_BOUNDS(0, MAX_PLAYERS, eIndex);
	return m_aiNumRevolts[eIndex];
}


void CvCity::changeNumRevolts(PlayerTypes eIndex, int iChange)
{
	FASSERT_BOUNDS(0, MAX_PLAYERS, eIndex);
	m_aiNumRevolts[eIndex] += iChange;
	FASSERT_NOT_NEGATIVE(m_aiNumRevolts[eIndex]);
}

bool CvCity::isEverOwned(PlayerTypes eIndex) const
{
	FASSERT_BOUNDS(0, MAX_PLAYERS, eIndex);
	return m_abEverOwned[eIndex];
}


void CvCity::setEverOwned(PlayerTypes eIndex, bool bNewValue)
{
	FASSERT_BOUNDS(0, MAX_PLAYERS, eIndex);
	m_abEverOwned[eIndex] = bNewValue;
}


bool CvCity::isTradeRoute(PlayerTypes eIndex) const
{
	FASSERT_BOUNDS(0, MAX_PLAYERS, eIndex);
	return m_abTradeRoute[eIndex];
}


void CvCity::setTradeRoute(PlayerTypes eIndex, bool bNewValue)
{
	FASSERT_BOUNDS(0, MAX_PLAYERS, eIndex);
	m_abTradeRoute[eIndex] = bNewValue;
}


bool CvCity::isRevealed(TeamTypes eIndex, bool bDebug) const
{
	if (bDebug && GC.getGame().isDebugMode())
	{
		return true;
	}
	FASSERT_BOUNDS(0, MAX_TEAMS, eIndex);
	return m_abRevealed[eIndex];
}


void CvCity::setRevealed(TeamTypes eIndex, bool bNewValue)
{
	FASSERT_BOUNDS(0, MAX_TEAMS, eIndex);

	setupGraphical();

	if (isRevealed(eIndex, false) != bNewValue)
	{
		m_abRevealed[eIndex] = bNewValue;

		updateVisibility();

		if (eIndex == GC.getGame().getActiveTeam())
		{
			algo::for_each(plots(), bind(CvPlot::updateSymbols, _1));
		}
	}
}


bool CvCity::getEspionageVisibility(TeamTypes eIndex) const
{
	FASSERT_BOUNDS(0, MAX_TEAMS, eIndex);
	return m_abEspionageVisibility[eIndex];
}


void CvCity::setEspionageVisibility(TeamTypes eIndex, bool bNewValue, bool bUpdatePlotGroups)
{
	FASSERT_BOUNDS(0, MAX_TEAMS, eIndex);

	if (getEspionageVisibility(eIndex) != bNewValue)
	{
		plot()->updateSight(false, bUpdatePlotGroups);

		m_abEspionageVisibility[eIndex] = bNewValue;

		plot()->updateSight(true, bUpdatePlotGroups);
	}
}

void CvCity::updateEspionageVisibility(bool bUpdatePlotGroups)
{
	PROFILE_EXTRA_FUNC();
	std::vector<EspionageMissionTypes> aMission;

	for (int iI = 0; iI < GC.getNumEspionageMissionInfos(); ++iI)
	{
		if (GC.getEspionageMissionInfo((EspionageMissionTypes)iI).isPassive() && GC.getEspionageMissionInfo((EspionageMissionTypes)iI).getVisibilityLevel() > 0)
		{
			aMission.push_back((EspionageMissionTypes)iI);
		}
	}
	const TeamTypes eTeam = getTeam();

	for (int iI = 0; iI < MAX_PC_TEAMS; ++iI)
	{
		const TeamTypes eTeamX = static_cast<TeamTypes>(iI);
		bool bVisibility = false;

		if (eTeamX != eTeam && isRevealed(eTeamX, false))
		{
			for (int iJ = 0; iJ < MAX_PC_PLAYERS; ++iJ)
			{
				const CvPlayer& playerY = GET_PLAYER((PlayerTypes)iJ);

				if (playerY.isAliveAndTeam(eTeamX))
				{
					foreach_(const EspionageMissionTypes& it, aMission)
					{
						if (playerY.canDoEspionageMission(it, getOwner(), plot(), -1, NULL))
						{
							bVisibility = true;
							break;
						}
					}
					if (bVisibility) break;
				}
			}
		}
		setEspionageVisibility(eTeamX, bVisibility, bUpdatePlotGroups);
	}
}

const wchar_t* CvCity::getNameKey() const
{
	return m_szName;
}


const CvWString CvCity::getName(uint uiForm) const
{
	return gDLL->getObjectText(m_szName, uiForm, true);
}


void CvCity::setName(const wchar_t* szNewValue, bool bFound)
{
	CvWString szName(szNewValue);
	gDLL->stripSpecialCharacters(szName);

	if (!szName.empty())
	{
		if (GET_PLAYER(getOwner()).isCityNameValid(szName, false))
		{
			m_szName = szName;
			emitNameChange(NAMECHANGE_CITY, getOwner(), getID());

			setInfoDirty(true);

			if (isCitySelected())
			{
				gDLL->getInterfaceIFace()->setDirty(CityScreen_DIRTY_BIT, true);
			}
		}
		if (bFound)
		{
			doFoundMessage();
		}
	}
}


void CvCity::doFoundMessage()
{
	CvWString szBuffer = gDLL->getText("TXT_KEY_MISC_CITY_HAS_BEEN_FOUNDED", getNameKey());
	AddDLLMessage(getOwner(), false, -1, szBuffer, ARTFILEMGR.getInterfaceArtInfo("WORLDBUILDER_CITY_EDIT")->getPath(), MESSAGE_TYPE_MAJOR_EVENT, NULL, NO_COLOR, getX(), getY());

	szBuffer = gDLL->getText("TXT_KEY_MISC_CITY_IS_FOUNDED", getNameKey());
	GC.getGame().addReplayMessage(REPLAY_MESSAGE_CITY_FOUNDED, getOwner(), szBuffer, getX(), getY(), GC.getCOLOR_ALT_HIGHLIGHT_TEXT());
}


std::string CvCity::getScriptData() const
{
	return m_szScriptData;
}

// cppcheck-suppress passedByValue
void CvCity::setScriptData(std::string szNewValue)
{
	m_szScriptData = szNewValue;
}


int CvCity::getFreeBonus(BonusTypes eIndex) const
{
	FASSERT_BOUNDS(0, GC.getNumBonusInfos(), eIndex);
	// The derived building supply PLUS the persisted event/WB grants -- the two halves the retired mixed
	// ledger used to conflate (savemigration.txt; the reader sums, exactly as the building-commerce split does).
	return m_paiFreeBonus[eIndex] + m_paiFreeBonusEvents[eIndex];
}


void CvCity::changeFreeBonus(BonusTypes eIndex, int iChange)
{
	FASSERT_BOUNDS(0, GC.getNumBonusInfos(), eIndex);

	if (iChange != 0)
	{
		GET_PLAYER(getOwner()).startDeferredPlotGroupBonusCalculation();

		plot()->updatePlotGroupBonus(false);
		m_paiFreeBonus[eIndex] += iChange;
		FASSERT_NOT_NEGATIVE(getFreeBonus(eIndex));
		plot()->updatePlotGroupBonus(true);

		GET_PLAYER(getOwner()).endDeferredPlotGroupBonusCalculation();
	}
}

void CvCity::changeFreeBonusEvent(BonusTypes eIndex, int iChange)
{
	FASSERT_BOUNDS(0, GC.getNumBonusInfos(), eIndex);

	if (iChange != 0)
	{
		// Same network bracket as the derived half -- the plot group has to see the supply change either way;
		// only WHERE the delta is stored differs (this half is saved, that half is rebuilt from events).
		GET_PLAYER(getOwner()).startDeferredPlotGroupBonusCalculation();

		plot()->updatePlotGroupBonus(false);
		m_paiFreeBonusEvents[eIndex] += iChange;
		FASSERT_NOT_NEGATIVE(getFreeBonus(eIndex));
		plot()->updatePlotGroupBonus(true);

		GET_PLAYER(getOwner()).endDeferredPlotGroupBonusCalculation();
	}
}

int CvCity::getNumBonusesFromBase(BonusTypes eIndex, int iBaseNum) const
{
	if (GET_PLAYER(getOwner()).getBonusMintedPercent(eIndex) > 0)
	{
		return 0;
	}
	return iBaseNum;
}

int CvCity::getNetworkBonusCount(BonusTypes eBonus) const
{
	FASSERT_BOUNDS(0, GC.getNumBonusInfos(), eBonus);

	const CvPlotGroup* pPlotGroup = plotGroup(getOwner());
	return pPlotGroup != NULL ? pPlotGroup->getNumBonuses(eBonus) : 0;
}

int CvCity::getNumBonuses(BonusTypes eIndex) const
{
	FASSERT_BOUNDS(0, GC.getNumBonusInfos(), eIndex);

	if (!GET_TEAM(getTeam()).isHasTech((TechTypes)GC.getBonusInfo(eIndex).getTechCityTrade()))
	{
		return 0;
	}
	return getNumBonusesFromBase(eIndex, getNetworkBonusCount(eIndex)) + getCorpBonusProduction(eIndex);
}

// The HELD SET behind getNumBonuses. It walks the plot group's own sparse holdings -- never the bonus registry --
// and puts each candidate through the SAME read the single-bonus question uses, so the two cannot answer
// differently ([DEC-single-implementation]).
// ⛔ The corporation pass is not belt-and-braces: a corporation PRODUCES a resource the network may not carry at
// all, so it is absent from the group's map however the gate answers, and a walk of the group alone silently
// drops that whole source class from any census built on it.
void CvCity::collectHeldBonuses(std::vector<int>& kHeldOut) const
{
	PROFILE_EXTRA_FUNC();

	kHeldOut.clear();

	const CvPlotGroup* pPlotGroup = plotGroup(getOwner());
	if (pPlotGroup != NULL)
	{
		for (std::map<int, int>::const_iterator itNetwork = pPlotGroup->getBonuses().begin();
			itNetwork != pPlotGroup->getBonuses().end(); ++itNetwork)
		{
			if (getNumBonuses((BonusTypes)itNetwork->first) > 0)
			{
				kHeldOut.push_back(itNetwork->first);
			}
		}
	}

	for (std::vector< std::pair<BonusTypes, int> >::const_iterator itCorp = m_corpBonusProduction.begin();
		itCorp != m_corpBonusProduction.end(); ++itCorp)
	{
		const int iBonus = (int)itCorp->first;
		if (getNumBonuses(itCorp->first) > 0 && !algo::any_of_equal(kHeldOut, iBonus))
		{
			kHeldOut.push_back(iBonus);
		}
	}
}


bool CvCity::hasBonus(BonusTypes eIndex) const
{
	return getNumBonuses(eIndex) > 0;
}

void CvCity::startDeferredBonusProcessing()
{
	PROFILE_EXTRA_FUNC();
	if (0 == m_deferringBonusProcessingCount++)
	{
		SAFE_DELETE_ARRAY(m_paiStartDeferredSectionNumBonuses);

		m_paiStartDeferredSectionNumBonuses = new int[GC.getNumBonusInfos()];

		for (int iI = 0; iI < GC.getNumBonusInfos(); iI++)
		{
			m_paiStartDeferredSectionNumBonuses[iI] = getNetworkBonusCount((BonusTypes)iI);
		}
	}
}

void CvCity::endDeferredBonusProcessing()
{
	PROFILE_EXTRA_FUNC();
	if (0 == --m_deferringBonusProcessingCount)
	{
		for (int iI = 0; iI < GC.getNumBonusInfos(); iI++)
		{
			processNumBonusChange((BonusTypes)iI, m_paiStartDeferredSectionNumBonuses[iI], getNetworkBonusCount((BonusTypes)iI));
		}

		SAFE_DELETE_ARRAY(m_paiStartDeferredSectionNumBonuses);
	}
}

void CvCity::processNumBonusChange(BonusTypes eIndex, int iOldValue, int iNewValue)
{
	if (iOldValue != iNewValue)
	{
		const bool bOldHasBonus = (getNumBonusesFromBase(eIndex, iOldValue) != 0);
		const bool bNewHasBonus = (getNumBonusesFromBase(eIndex, iNewValue) != 0);

		if (bOldHasBonus != bNewHasBonus)
		{
			if (bNewHasBonus)
			{
				processBonus(eIndex, 1);
			}
			else
			{
				processBonus(eIndex, -1);
			}
		}

		if (isCorporationBonus(eIndex))
		{
			updateCorporation();
		}

		//	Linking bonuses may change what is buildable
	}
}

void CvCity::onNetworkBonusChanged(BonusTypes eBonus, int iOldCount, int iNewCount)
{
	FASSERT_BOUNDS(0, GC.getNumBonusInfos(), eBonus);

	// While a deferred section is open the bracket owns the announcement: it snapshotted this city's relayed
	// read on entry and compares it on exit, so a run of moves over the same bonus collapses into one crossing.
	if (m_deferringBonusProcessingCount == 0)
	{
		processNumBonusChange(eBonus, iOldCount, iNewCount);
	}
}

void CvCity::onNetworkSupplyChanged(const CvPlotGroup* pOldSupply, const CvPlotGroup* pNewSupply)
{
	PROFILE_EXTRA_FUNC();

	// Same deferral as the per-bonus crossing above, and for the same reason -- the pointer move IS a change in
	// what this city's relayed read returns, so the bracket's snapshot/compare sees it whole.
	if (m_deferringBonusProcessingCount != 0)
	{
		return;
	}
	for (int iI = 0; iI < GC.getNumBonusInfos(); iI++)
	{
		const int iOldCount = pOldSupply != NULL ? pOldSupply->getNumBonuses((BonusTypes)iI) : 0;
		const int iNewCount = pNewSupply != NULL ? pNewSupply->getNumBonuses((BonusTypes)iI) : 0;

		processNumBonusChange((BonusTypes)iI, iOldCount, iNewCount);
	}
}

void CvCity::onNetworkSupplyAcquired(const CvPlotGroup* pSupply)
{
	PROFILE_EXTRA_FUNC();

	if (pSupply == NULL)
	{
		return;
	}
	for (int iI = 0; iI < GC.getNumBonusInfos(); iI++)
	{
		processNumBonusChange((BonusTypes)iI, 0, pSupply->getNumBonuses((BonusTypes)iI));
	}
}

void CvCity::onNetworkSupplyLost(const CvPlotGroup* pSupply)
{
	PROFILE_EXTRA_FUNC();

	if (pSupply == NULL)
	{
		return;
	}
	for (int iI = 0; iI < GC.getNumBonusInfos(); iI++)
	{
		processNumBonusChange((BonusTypes)iI, pSupply->getNumBonuses((BonusTypes)iI), 0);
	}
}


int CvCity::getCorpBonusProduction(const BonusTypes eBonus) const
{
	PROFILE_EXTRA_FUNC();
	FASSERT_BOUNDS(0, GC.getNumBonusInfos(), eBonus)

	for (std::vector< std::pair<BonusTypes, int> >::const_iterator it = m_corpBonusProduction.begin(); it != m_corpBonusProduction.end(); ++it)
	{
		if ((*it).first == eBonus)
		{
			return (*it).second;
		}
	}
	return 0;
}

bool CvCity::isCorporationBonus(BonusTypes eBonus) const
{
	PROFILE_EXTRA_FUNC();
	FASSERT_BOUNDS(0, GC.getNumBonusInfos(), eBonus);

	for (int iCorp = 0; iCorp < GC.getNumCorporationInfos(); ++iCorp)
	{
		if (GET_PLAYER(getOwner()).isActiveCorporation((CorporationTypes)iCorp)
		&& isHasCorporation((CorporationTypes)iCorp)
		&& algo::any_of_equal(GC.getCorporationInfo((CorporationTypes)iCorp).getConsumedBonuses(), eBonus))
		{
			return true;
		}
	}

	return false;
}

bool CvCity::isActiveCorporation(CorporationTypes eCorporation) const
{
	PROFILE_EXTRA_FUNC();
	FASSERT_BOUNDS(0, GC.getNumCorporationInfos(), eCorporation);

	if (!isHasCorporation(eCorporation))
	{
		return false;
	}

	if (!GET_PLAYER(getOwner()).isActiveCorporation(eCorporation))
	{
		return false;
	}

	if (GC.getCorporationInfo(eCorporation).getObsoleteTech() != NO_TECH)
	{
		if (GET_TEAM(getTeam()).isHasTech(GC.getCorporationInfo(eCorporation).getObsoleteTech()))
		{
			return false;
		}
	}
	bool bRequiresBonus = false;
	bool bHasRequiredBonus = false;

	foreach_(const int iConsumedBonus, GC.getCorporationInfo(eCorporation).getConsumedBonuses())
	{
		const BonusTypes eBonus = static_cast<BonusTypes>(iConsumedBonus);
		bRequiresBonus = true;
		if (getNumBonuses(eBonus) > 0)
		{
			bHasRequiredBonus = true;
			break;
		}
	}
	if (bRequiresBonus && bHasRequiredBonus)
	{
		return true;
	}
	if (!bRequiresBonus)
	{
		return true;
	}

	return false;
}

int CvCity::getProgressOnBuilding(const BuildingTypes eType)	const
{
	PROFILE_EXTRA_FUNC();
	FASSERT_BOUNDS(0, GC.getNumBuildingInfos(), eType);

	for (std::vector< std::pair<BuildingTypes, int> >::const_iterator it = m_progressOnBuilding.begin(); it != m_progressOnBuilding.end(); ++it)
	{
		if ((*it).first == eType)
		{
			return (*it).second;
		}
	}
	return 0;
}

void CvCity::setProgressOnBuilding(const BuildingTypes eType, int iNewValue)
{
	FASSERT_NOT_NEGATIVE(iNewValue);
	if (iNewValue < 0) iNewValue = 0;

	const int iOldValue = getProgressOnBuilding(eType);
	if (iOldValue != iNewValue)
	{
		changeProgressOnBuilding(eType, iNewValue - iOldValue);
	}
}

void CvCity::changeProgressOnBuilding(const BuildingTypes eType, const int iChange)
{
	PROFILE_EXTRA_FUNC();
	FASSERT_BOUNDS(0, GC.getNumBuildingInfos(), eType);
	if (iChange == 0)
	{
		return;
	}
	if (getTeam() == GC.getGame().getActiveTeam())
	{
		setInfoDirty(true);

		if (isCitySelected())
		{
			gDLL->getInterfaceIFace()->setDirty(CityScreen_DIRTY_BIT, true);
		}
	}
	for (std::vector< std::pair<BuildingTypes, int> >::iterator it = m_progressOnBuilding.begin(); it != m_progressOnBuilding.end(); ++it)
	{
		if ((*it).first == eType)
		{
			if ((*it).second <= -iChange)
			{
				m_progressOnBuilding.erase(it);
				endDelayOnBuilding(eType);
			}
			else
			{
				(*it).second += iChange;
			}
			return;
		}
	}
	m_progressOnBuilding.push_back(std::make_pair(eType, iChange));
}


int CvCity::getDelayOnBuilding(const BuildingTypes eType)	const
{
	PROFILE_EXTRA_FUNC();
	FASSERT_BOUNDS(0, GC.getNumBuildingInfos(), eType);

	for (std::vector< std::pair<BuildingTypes, int> >::const_iterator it = m_delayOnBuilding.begin(); it != m_delayOnBuilding.end(); ++it)
	{
		if ((*it).first == eType)
		{
			return (*it).second;
		}
	}
	return 0;
}

void CvCity::endDelayOnBuilding(const BuildingTypes eType)
{
	PROFILE_EXTRA_FUNC();
	FASSERT_BOUNDS(0, GC.getNumBuildingInfos(), eType);

	for (std::vector< std::pair<BuildingTypes, int> >::iterator it = m_delayOnBuilding.begin(); it != m_delayOnBuilding.end(); ++it)
	{
		if ((*it).first == eType)
		{
			m_delayOnBuilding.erase(it);
			return;
		}
	}
}

void CvCity::tickDelayOnBuilding(const BuildingTypes eType, const bool bIncrement)
{
	PROFILE_EXTRA_FUNC();
	FASSERT_BOUNDS(0, GC.getNumBuildingInfos(), eType);

	for (std::vector< std::pair<BuildingTypes, int> >::iterator it = m_delayOnBuilding.begin(); it != m_delayOnBuilding.end(); ++it)
	{
		if ((*it).first == eType)
		{
			if (bIncrement)
			{
				(*it).second += 1;
			}
			else if ((*it).second == 1)
			{
				m_delayOnBuilding.erase(it);
			}
			else (*it).second -= 1;

			return;
		}
	}
	if (!bIncrement)
	{
		FErrorMsg("Trying to decrement past zero for a value where negatives makes no sense!");
		return;
	}
	m_delayOnBuilding.push_back(std::make_pair(eType, 1));
}


// Returns true if the given building will decay this turn.
bool CvCity::isBuildingProductionDecay(BuildingTypes eIndex) const
{
	FASSERT_BOUNDS(0, GC.getNumBuildingInfos(), eIndex);
	return (
		isHuman()
		&& getProductionBuilding() != eIndex
		&& getProgressOnBuilding(eIndex) > 0
		&& (
			100 * getDelayOnBuilding(eIndex)
			>=
			GC.getBUILDING_PRODUCTION_DECAY_TIME() * CvGameSpeedScale::speedPercent()
		)
	);
}

// Returns the amount by which the given building will decay once it reaches the limit.
// Ignores whether or not the building will actually decay this turn.
int CvCity::getBuildingProductionDecay(BuildingTypes eIndex) const
{
	FASSERT_BOUNDS(0, GC.getNumBuildingInfos(), eIndex);
	const int iProduction = getProgressOnBuilding(eIndex);
	return iProduction - iProduction * GC.getBUILDING_PRODUCTION_DECAY_PERCENT() / 100;
}

// Returns the number of turns left before the given building will decay.
int CvCity::getBuildingProductionDecayTurns(BuildingTypes eIndex) const
{
	FASSERT_BOUNDS(0, GC.getNumBuildingInfos(), eIndex);
	return std::max(1, 1 + (GC.getBUILDING_PRODUCTION_DECAY_TIME() * CvGameSpeedScale::speedPercent() + 99) / 100 - getDelayOnBuilding(eIndex));
}


int CvCity::getProjectProduction(ProjectTypes eIndex) const
{
	FASSERT_BOUNDS(0, GC.getNumProjectInfos(), eIndex);
	return m_paiProjectProduction[eIndex];
}

void CvCity::setProjectProduction(ProjectTypes eIndex, int iNewValue)
{
	FASSERT_BOUNDS(0, GC.getNumProjectInfos(), eIndex);

	if (getProjectProduction(eIndex) != iNewValue)
	{
		m_paiProjectProduction[eIndex] = iNewValue;
		FASSERT_NOT_NEGATIVE(getProjectProduction(eIndex));

		if (getTeam() == GC.getGame().getActiveTeam())
		{
			setInfoDirty(true);
		}

		if ((getOwner() == GC.getGame().getActivePlayer()) && isCitySelected())
		{
			gDLL->getInterfaceIFace()->setDirty(CityScreen_DIRTY_BIT, true);
		}
	}
}

void CvCity::changeProjectProduction(ProjectTypes eIndex, int iChange)
{
	FASSERT_BOUNDS(0, GC.getNumProjectInfos(), eIndex);
	setProjectProduction(eIndex, (getProjectProduction(eIndex) + iChange));
}


int CvCity::getProgressOnUnit(const UnitTypes eUnit) const
{
	PROFILE_EXTRA_FUNC();
	FASSERT_BOUNDS(0, GC.getNumUnitInfos(), eUnit)

	for (std::vector< std::pair<UnitTypes, int> >::const_iterator it = m_progressOnUnit.begin(); it != m_progressOnUnit.end(); ++it)
	{
		if ((*it).first == eUnit)
		{
			return (*it).second;
		}
	}
	return 0;
}


void CvCity::setProgressOnUnit(const UnitTypes eUnit, int iNewValue)
{
	FASSERT_NOT_NEGATIVE(iNewValue);
	if (iNewValue < 0) iNewValue = 0;

	const int iOldValue = getProgressOnUnit(eUnit);
	if (iOldValue != iNewValue)
	{
		changeProgressOnUnit(eUnit, iNewValue - iOldValue);
	}
}


void CvCity::changeProgressOnUnit(const UnitTypes eUnit, const int iChange)
{
	PROFILE_EXTRA_FUNC();
	FASSERT_BOUNDS(0, GC.getNumUnitInfos(), eUnit);
	if (iChange == 0)
	{
		return;
	}
	if (getTeam() == GC.getGame().getActiveTeam())
	{
		setInfoDirty(true);

		if (isCitySelected())
		{
			gDLL->getInterfaceIFace()->setDirty(CityScreen_DIRTY_BIT, true);
		}
	}
	for (std::vector< std::pair<UnitTypes, int> >::iterator it = m_progressOnUnit.begin(); it != m_progressOnUnit.end(); ++it)
	{
		if ((*it).first == eUnit)
		{
			if ((*it).second <= -iChange)
			{
				m_progressOnUnit.erase(it);
				endDelayOnUnit(eUnit);
			}
			else
			{
				(*it).second += iChange;
			}
			return;
		}
	}
	m_progressOnUnit.push_back(std::make_pair(eUnit, iChange));
}


int CvCity::getDelayOnUnit(const UnitTypes eUnit) const
{
	PROFILE_EXTRA_FUNC();
	FASSERT_BOUNDS(0, GC.getNumUnitInfos(), eUnit);

	for (std::vector< std::pair<UnitTypes, int> >::const_iterator it = m_delayOnUnit.begin(); it != m_delayOnUnit.end(); ++it)
	{
		if ((*it).first == eUnit)
		{
			return (*it).second;
		}
	}
	return 0;
}

void CvCity::endDelayOnUnit(const UnitTypes eUnit)
{
	PROFILE_EXTRA_FUNC();
	FASSERT_BOUNDS(0, GC.getNumUnitInfos(), eUnit);

	for (std::vector< std::pair<UnitTypes, int> >::iterator it = m_delayOnUnit.begin(); it != m_delayOnUnit.end(); ++it)
	{
		if ((*it).first == eUnit)
		{
			m_delayOnUnit.erase(it);
			return;
		}
	}
}

void CvCity::tickDelayOnUnit(const UnitTypes eUnit, const bool bIncrement)
{
	PROFILE_EXTRA_FUNC();
	FASSERT_BOUNDS(0, GC.getNumUnitInfos(), eUnit);

	for (std::vector< std::pair<UnitTypes, int> >::iterator it = m_delayOnUnit.begin(); it != m_delayOnUnit.end(); ++it)
	{
		if ((*it).first == eUnit)
		{
			if (bIncrement)
			{
				(*it).second += 1;
			}
			else if ((*it).second == 1)
			{
				m_delayOnUnit.erase(it);
			}
			else (*it).second -= 1;

			return;
		}
	}
	if (!bIncrement)
	{
		FErrorMsg("Trying to decrement past zero for a value where negatives makes no sense!");
		return;
	}
	m_delayOnUnit.push_back(std::make_pair(eUnit, 1));
}


// Returns true if the given unit will decay this turn.
bool CvCity::isUnitProductionDecay(UnitTypes eIndex) const
{
	FASSERT_BOUNDS(0, GC.getNumUnitInfos(), eIndex);
	return (
		isHuman()
		&& getProductionUnit() != eIndex
		&& getProgressOnUnit(eIndex) > 0
		&& (
			100 * getDelayOnUnit(eIndex)
			>=
			GC.getUNIT_PRODUCTION_DECAY_TIME() * CvGameSpeedScale::speedPercent()
		)
	);
}

// Returns the amount by which the given unit will decay once it reaches the limit.
// Ignores whether or not the unit will actually decay this turn.
int CvCity::getUnitProductionDecay(UnitTypes eIndex) const
{
	FASSERT_BOUNDS(0, GC.getNumUnitInfos(), eIndex);
	const int iProduction = getProgressOnUnit(eIndex);
	return iProduction - iProduction * GC.getUNIT_PRODUCTION_DECAY_PERCENT() / 100;
}

// Returns the number of turns left before the given unit will decay.
int CvCity::getUnitProductionDecayTurns(UnitTypes eIndex) const
{
	FASSERT_BOUNDS(0, GC.getNumUnitInfos(), eIndex);
	return std::max(1, 1 + (GC.getUNIT_PRODUCTION_DECAY_TIME() * CvGameSpeedScale::speedPercent() + 99) / 100 - getDelayOnUnit(eIndex));
}


int CvCity::getGreatPeopleUnitRate(UnitTypes eIndex) const
{
	FASSERT_BOUNDS(0, GC.getNumUnitInfos(), eIndex);
	int iTotalGreatPeopleUnitRate = 0;
	iTotalGreatPeopleUnitRate += m_paiGreatPeopleUnitRate[eIndex];
	iTotalGreatPeopleUnitRate += GET_PLAYER(getOwner()).getNationalGreatPeopleUnitRate(eIndex);
	return std::max(0, iTotalGreatPeopleUnitRate);
}

void CvCity::setGreatPeopleUnitRate(UnitTypes eIndex, int iNewValue)
{
	FASSERT_BOUNDS(0, GC.getNumUnitInfos(), eIndex);
	m_paiGreatPeopleUnitRate[eIndex] = iNewValue;
}


void CvCity::changeGreatPeopleUnitRate(UnitTypes eIndex, int iChange)
{
	setGreatPeopleUnitRate(eIndex, (m_paiGreatPeopleUnitRate[eIndex] + iChange));
}


int CvCity::getGreatPeopleUnitProgress(UnitTypes eIndex) const
{
	FASSERT_BOUNDS(0, GC.getNumUnitInfos(), eIndex);
	return m_paiGreatPeopleUnitProgress[eIndex];
}


void CvCity::setGreatPeopleUnitProgress(UnitTypes eIndex, int iNewValue)
{
	FASSERT_BOUNDS(0, GC.getNumUnitInfos(), eIndex);
	m_paiGreatPeopleUnitProgress[eIndex] = iNewValue;
	FASSERT_NOT_NEGATIVE(getGreatPeopleUnitProgress(eIndex));
}


void CvCity::changeGreatPeopleUnitProgress(UnitTypes eIndex, int iChange)
{
	setGreatPeopleUnitProgress(eIndex, (getGreatPeopleUnitProgress(eIndex) + iChange));
}


int CvCity::getSpecialistCount(SpecialistTypes eIndex) const
{
	FASSERT_BOUNDS(0, GC.getNumSpecialistInfos(), eIndex);
	return m_paiSpecialistCount[eIndex];
}


void CvCity::setSpecialistCount(SpecialistTypes eIndex, int iNewValue)
{
	FASSERT_BOUNDS(0, GC.getNumSpecialistInfos(), eIndex);

	const int iOldValue = getSpecialistCount(eIndex);

	if (iOldValue != iNewValue)
	{
		if (isCitizenJuggling())
		{
			// ⚠ THE ONE PLACE THE COMMIT IS DELIBERATELY SEPARATED FROM THE ANNOUNCEMENT. A juggle run probes
			// assignments repeatedly, so the per-probe fact is suppressed and the run's NET is announced once at
			// the close -- converged probes then cancel to nothing instead of moving the packages per probe.
			m_paiSpecialistCount[eIndex] = iNewValue;
			m_bJuggleDeferredSpec = true;
		}
		else
		{
			// The commit and the fact; the effects continue below.
			setSpecialistCountInternal(eIndex, iNewValue);
		}
		FASSERT_NOT_NEGATIVE(getSpecialistCount(eIndex));

		changeSpecialistPopulation(iNewValue - iOldValue);
		processSpecialist(eIndex, (iNewValue - iOldValue));

		if (isCitySelected())
		{
			gDLL->getInterfaceIFace()->setDirty(CitizenButtons_DIRTY_BIT, true);
		}

#ifdef YIELD_VALUE_CACHING
		AI_NoteSpecialistChange();
#endif
	}
}


void CvCity::changeSpecialistCount(SpecialistTypes eIndex, int iChange)
{
	setSpecialistCount(eIndex, (getSpecialistCount(eIndex) + iChange));
}


void CvCity::alterSpecialistCount(SpecialistTypes eIndex, int iChange)
{
	PROFILE_EXTRA_FUNC();
	if (iChange == 0)
	{
		return;
	}
	if (isCitizensAutomated())
	{
		int iForcedSpecialists = getForceSpecialistCount(eIndex);
		if (getForceSpecialistCount(eIndex) + iChange < 0)
		{
			FErrorMsg("This shouldn't happen");
			setCitizensAutomated(false);
		}
		else
		{
			bool bAutomated = true;
			if (iChange > 0)
			{
				int iForced = 0;
				for (int iI = 0; iI < GC.getNumSpecialistInfos(); iI++)
				{
					iForced += getForceSpecialistCount((SpecialistTypes)iI);
				}
				if (iForced + iChange > getMaxSpecialistCount())
				{
					setCitizensAutomated(false);
					bAutomated = false;
				}
			}
			else
			{
				changeSpecialistCount(eIndex, iChange);
			}

			if (bAutomated)
			{
				setForceSpecialistCount(eIndex, getForceSpecialistCount(eIndex) + iChange);
				AI_assignWorkingPlots();
				return;
			}
		}
	}

	if (iChange > 0)
	{
		for (int iI = 0; iI < iChange; iI++)
		{
			if ((extraPopulation() > 0 || AI_removeWorstCitizen(eIndex)) && isSpecialistValid(eIndex, 1))
			{
				changeSpecialistCount(eIndex, 1);
			}
		}
	}
	else
	{
		for (int iI = 0; iI < -(iChange); iI++)
		{
			if (getSpecialistCount(eIndex) > 0)
			{
				changeSpecialistCount(eIndex, -1);

				if (eIndex != GC.getDEFAULT_SPECIALIST() && GC.getDEFAULT_SPECIALIST() != NO_SPECIALIST)
				{
					changeSpecialistCount((SpecialistTypes)GC.getDEFAULT_SPECIALIST(), 1);
				}
				else if (extraFreeSpecialists() > 0)
				{
					AI_addBestCitizen(false, true);
				}
				else
				{
					bool bCanWorkPlot = false;

					for (int iJ = SKIP_CITY_HOME_PLOT; iJ < getNumCityPlots(); iJ++)
					{
						if (!isWorkingPlot(iJ))
						{
							const CvPlot* pLoopPlot = getCityIndexPlot(iJ);

							if (pLoopPlot != NULL && canWork(pLoopPlot))
							{
								bCanWorkPlot = true;
								break;
							}
						}
					}

					if (bCanWorkPlot)
					{
						AI_addBestCitizen(true, false);
					}
					else
					{
						AI_addBestCitizen(false, true);
					}
				}
			}
		}
	}
}


int CvCity::getMaxSpecialistCount() const
{
	return totalFreeSpecialists() + getPopulation() - angryPopulation();
}

//	The manual-assign slot cap this city opens for one specialist type: a FRESH GATHER over its OPERATING
//	buildings' own `allowedSpecialists.city.{SPECIALIST_X}` deposits ([DEC-accumulator-cut-uniform] -- the
//	serialized per-city/per-team ledgers this replaces carried save history no live source could reproduce).
//	⚑ It reads through the KEYED TWIN because the family authors both shapes: the plain slots a building always
//	opens, and the tech-gated ones beside them. The twin evaluates that conditioned tail through the ONE
//	evaluator against this city's contexts, so a slot appears exactly when its tech is held -- which is what the
//	team ledger was pre-computing on every tech acquire.
//	⛔ OPERATING buildings only: a dormant or obsolete building confers nothing ([enabler.md §3.2]).
//	⚠ THE READ EDGE: the deposit is ×100 like every authored amount ([fixed-point-and-scales] §2 -- there is no
//	count exemption), and a slot cap is whole, so it reduces HERE. This is the cap the AI seats up to: leaving it
//	scaled seats 100x the specialists.
int CvCity::getMaxSpecialistCount(SpecialistTypes eIndex) const
{
	FASSERT_BOUNDS(0, GC.getNumSpecialistInfos(), eIndex);
	const CvPlayer& kOwner = GET_PLAYER(getOwner());
	CvCascadeEvalCtx evalCtx;
	InfoValuation::fillEvalCtx(getCityContext(), kOwner.getEmpireContext(), plotGroup(getOwner()), evalCtx);
	const OperatingBuildings& kOperating = EnablerKernel::operatingBuildings(this);
	int64_t iTotal = 0;
	for (std::set<int>::const_iterator it = kOperating.active.begin(); it != kOperating.active.end(); ++it)
	{
		iTotal += InfoValuation::keyedTargetSum(GC.getBuildingInfo((BuildingTypes)*it).getModifiers(),
			MODFAM_ALLOWED_SPECIALISTS, CHANNEL_AMOUNT, -1, (int)eIndex, evalCtx);
	}
	return (int)std::max((int64_t)0, iTotal / 100);
}

bool CvCity::isSpecialistValid(SpecialistTypes eIndex, int iExtra) const
{
	return
	(
		(
			GET_PLAYER(getOwner()).isSpecialistValid(eIndex)
			||
			getMaxSpecialistCount(eIndex) > 0 && getSpecialistCount(eIndex) + iExtra <= getMaxSpecialistCount(eIndex)
		)
		&&
		getSpecialistCount(eIndex) + iExtra <= getMaxSpecialistCount()
	);
}


int CvCity::getForceSpecialistCount(SpecialistTypes eIndex) const
{
	FASSERT_BOUNDS(0, GC.getNumSpecialistInfos(), eIndex);
	return m_paiForceSpecialistCount[eIndex];
}


bool CvCity::isSpecialistForced() const
{
	PROFILE_EXTRA_FUNC();
	for (int iI = 0; iI < GC.getNumSpecialistInfos(); iI++)
	{
		if (getForceSpecialistCount((SpecialistTypes)iI) > 0)
		{
			return true;
		}
	}
	return false;
}


void CvCity::setForceSpecialistCount(SpecialistTypes eIndex, int iNewValue)
{
	FASSERT_BOUNDS(0, GC.getNumSpecialistInfos(), eIndex);

	if (getForceSpecialistCount(eIndex) != iNewValue)
	{
		m_paiForceSpecialistCount[eIndex] = std::max(0, iNewValue);

		if (isCitySelected())
		{
			gDLL->getInterfaceIFace()->setDirty(Help_DIRTY_BIT, true);
		}

		AI_setAssignWorkDirty(true);
	}
}


// The city's TYPED free specialists of one type -- the two halves of the free-specialist seam, added
// ([modifier.md §6]). Neither half is a stored city accumulator, which is why the old member is gone:
//   1. the DERIVABLE half -- the typed `freeSpecialists.{SPECIALIST_X}` deposits of the sources that are alive
//      RIGHT NOW: this city's OPERATING buildings (a dormant one grants nothing) plus the empire-scope sources
//      the owner already sums. A typed entry is genuinely keyed, so it stays an entry-list read (§5), never a
//      scope-wide fold.
//   2. the ONE-SHOT half -- the UNATTRIBUTED ledger, which is genuine non-derivable state and correctly stays
//      serialized: a Great-Person join CONSUMES its unit and an era advance is a persisted pulse, so no live
//      source survives to re-derive them ([legacy-grant-apply-sites.md] §4, [save.md §5]).
// ⚠ The count unit is ×100 like every other compiled magnitude, so the derivable half reduces here; the
// one-shot ledger is a whole count and does not.
//	The one-specialist slice of the group read below -- ~30 call sites want a single count. NOT a second
//	implementation ([DEC-single-implementation]).
//	⚠ A caller wanting SEVERAL specialists must take the GROUP: this builds an eval ctx and walks the city's
//	whole operating set AND the empire's sources, so calling it in a loop pays all of that once per specialist.
//	That was the measured stall -- 40 specialists x per CITIZEN, through AI_ignoreGrowth.
int CvCity::getFreeSpecialistCount(SpecialistTypes eIndex) const
{
	FASSERT_BOUNDS(0, GC.getNumSpecialistInfos(), eIndex);
	std::vector<int64_t> aiCounts;
	getFreeSpecialists(aiCounts);
	if (eIndex < 0 || eIndex >= (int)aiCounts.size()) return 0;
	return (int)(aiCounts[eIndex] / 100);   // the READ EDGE -- whole specialists for a whole-count consumer
}

//	THE GROUP READ -- every specialist in ONE pass: one eval ctx, one walk of the operating set, one empire
//	read ([patterns.md] THE TWO READ ROLES rule 1 and rule 7).
void CvCity::getFreeSpecialists(std::vector<int64_t>& aiCounts) const
{
	PROFILE_EXTRA_FUNC();
	const int iNumSpecialists = GC.getNumSpecialistInfos();

	const CvPlayer& kOwner = GET_PLAYER(getOwner());
	CvCascadeEvalCtx evalCtx;
	InfoValuation::fillEvalCtx(getCityContext(), kOwner.getEmpireContext(), plotGroup(getOwner()), evalCtx);

	std::vector<int64_t> aiCityScope((size_t)iNumSpecialists, (int64_t)0);
	const OperatingBuildings& kOperating = EnablerKernel::operatingBuildings(this);
	for (std::set<int>::const_iterator it = kOperating.active.begin(); it != kOperating.active.end(); ++it)
	{
		InfoValuation::collectKeyedTargetSums(GC.getBuildingInfo((BuildingTypes)*it).getModifiers(),
			MODFAM_FREE_SPECIALISTS, CHANNEL_AMOUNT, -1, evalCtx, aiCityScope);
	}
	std::vector<int64_t> aiEmpire;
	kOwner.getFreeSpecialists(aiEmpire);

	//	⛔ NO REDUCE HERE -- the reduce belongs at the read edge.
	//	⚠ The city leg used to be added RAW to an already-reduced empire leg: two operands on different scales,
	//	the surviving-fudge-factor shape (AGENTS.md drift detector 2). With 167 city-scope keyed authorings, it
	//	read Petra's authored 1 priest as 100.
	//	⚠ The UNATTRIBUTED ledger is a WHOLE count and is LIFTED to meet the other two, never the reverse.
	aiCounts.assign((size_t)iNumSpecialists, (int64_t)0);
	for (int iI = 0; iI < iNumSpecialists; iI++)
	{
		const int64_t iDerivable = std::max((int64_t)0, aiCityScope[iI])
			+ (iI < (int)aiEmpire.size() ? aiEmpire[iI] : (int64_t)0);
		aiCounts[iI] = std::max((int64_t)0,
			iDerivable + (int64_t)m_paiFreeSpecialistCountUnattributed[iI] * 100);
	}
}

int CvCity::getAddedFreeSpecialistCount(SpecialistTypes eIndex) const
{
	return m_paiFreeSpecialistCountUnattributed[eIndex];
}

//	Drop the city's PERSISTED free-specialist pulses -- what a city carries into a new ownership that no live
//	source justifies. The derivable half is not touched because it is not storage: it is re-summed from the
//	owner's live sources on the next read.
//	⚑ It replaces a 40-iteration `setFreeSpecialistCount(i, 0)` loop, each iteration of which walked the eval
//	ctx, the operating set and the whole empire to compute a delta against a value nothing stored.
void CvCity::clearAddedFreeSpecialists()
{
	for (int iI = 0; iI < GC.getNumSpecialistInfos(); iI++)
	{
		const int iHeld = m_paiFreeSpecialistCountUnattributed[iI];
		if (iHeld != 0)
		{
			m_paiFreeSpecialistCountUnattributed[iI] = 0;
			changeNumGreatPeople(-iHeld);
			processSpecialist((SpecialistTypes)iI, -iHeld);
		}
	}
	if (isCitySelected())
	{
		gDLL->getInterfaceIFace()->setDirty(CitizenButtons_DIRTY_BIT, true);
	}
}

//	⛔ NO DERIVED READ. The old body reconstructed iChange by reading the city's derived total, adding the
//	player's derived total, then differencing against that same city total -- five full walks (eval ctx +
//	operating set + empire) to recover the delta it was already handed. Under the cascade the amount has no
//	writer, so the only thing to write is the pulse.
void CvCity::changeFreeSpecialistCount(SpecialistTypes eIndex, int iChange, bool bUnattributed)
{
	FASSERT_BOUNDS(0, GC.getNumSpecialistInfos(), eIndex);
	if (iChange == 0)
	{
		return;
	}

	if (bUnattributed)
	{
		m_paiFreeSpecialistCountUnattributed[eIndex] =
			std::max(0, m_paiFreeSpecialistCountUnattributed[eIndex] + iChange);
	}

	changeNumGreatPeople(iChange);
	processSpecialist(eIndex, iChange);

	if (isCitySelected())
	{
		gDLL->getInterfaceIFace()->setDirty(CitizenButtons_DIRTY_BIT, true);
	}
}

uint32_t CvCity::getReligionInfluence(ReligionTypes eIndex) const
{
	FASSERT_BOUNDS(0, GC.getNumReligionInfos(), eIndex);
	// Less than zero is meaningless for this value.
	return std::max(0, m_paiReligionInfluence[eIndex]);
}


void CvCity::changeReligionInfluence(ReligionTypes eIndex, int iChange)
{
	FASSERT_BOUNDS(0, GC.getNumReligionInfos(), eIndex);
	m_paiReligionInfluence[eIndex] += iChange;
}


bool CvCity::isFreePromotion(PromotionTypes ePromo) const
{
	PROFILE_EXTRA_FUNC();
	// "Does an operating building here hand this promotion out unconditionally?" -- read off the `triggers`
	// onUnitEnteredCity promote entries. UNCONDITIONAL only: a conditioned entry may not apply to a given unit, and this
	// answers a blanket question. The operating set already means present AND operating, so the old
	// present-minus-disabled walk is subsumed.
	const OperatingBuildings& ob = EnablerKernel::operatingBuildings(this);
	std::vector<int> aAlways;
	std::vector<int> aConditional;
	for (std::set<int>::const_iterator it = ob.active.begin(); it != ob.active.end(); ++it)
	{
		const CvInfo* j = InfoRepo<CvBuildingInfo>::get().get(*it);
		if (j == NULL) continue;
		j->triggerPromotions(aAlways, aConditional);
		for (size_t i = 0; i < aAlways.size(); ++i)
		{
			if (aAlways[i] == (int)ePromo) return true;
		}
	}
	return false;
}



// ⛔ SCOPE-SPLIT KIND, so it cannot go through cascadeValue: city scope is a FLAT (the building amount, ×100)
// and empire scope is a PERCENT (the trait modifier). cascadeValue asks infoKindUnit at CITY scope, gets FLAT,
// and returns the flat sum -- DISCARDING every trait percent. The consumer applies the result as percentage
// points (`100 + x`), so both legs are summed here in those units: the flat reduces to human, the percent is
// already human ([DEC-fixedpoint-x100]).
int CvCity::getEspionageDefenseModifier() const
{
	const int iChannel = CascadeChannelRegistry::channelLookup(MODFAM_ESPIONAGE_DEFENSE, (int)CHANNEL_AMOUNT, -1);
	int64_t lFlatSum = 0;
	int64_t lPercentSum = 0;
	InfoValuation::rolledLegsAtCity(*this, iChannel, lFlatSum, lPercentSum);
	return (int)(lFlatSum / 100) + (int)lPercentSum;
}


bool CvCity::isWorkingPlot(int iIndex) const
{
	FASSERT_BOUNDS(0, NUM_CITY_PLOTS, iIndex);

	return m_pabWorkingPlot[iIndex];
}


bool CvCity::isWorkingPlot(const CvPlot* pPlot) const
{
	const int iIndex = getCityPlotIndex(pPlot);

	return iIndex != -1 && isWorkingPlot(iIndex);
}

void CvCity::processWorkingPlot(int iPlot, int iChange, bool yieldsOnly)
{
	PROFILE_EXTRA_FUNC();
	CvPlot* pPlot = getCityIndexPlot(iPlot);

	if (pPlot)
	{
		FAssertMsg(pPlot->getWorkingCity() == this, "WorkingCity is expected to be this");

		if (!yieldsOnly)
		{
			if (iPlot != CITY_HOME_PLOT)
			{
				changeWorkingPopulation(iChange);
			}

			// update plot builder special case where a plot is being worked but is (a) unimproved  or (b) un-bonus'ed
			pPlot->updatePlotBuilder();

			if (getTeam() == GC.getGame().getActiveTeam() || GC.getGame().isDebugMode())
			{
				pPlot->updateSymbolDisplay();
			}
		}

		// The worked-plot Σ is the cascade's -- the IS_WORKED fact the internal setter announces is what moves
		// this city's plot-fed sums. Only the UI-side rider stays here.
		onYieldChange();
	}

	if (isCitySelected())
	{
		gDLL->getInterfaceIFace()->setDirty(InfoPane_DIRTY_BIT, true);
		gDLL->getInterfaceIFace()->setDirty(CityScreen_DIRTY_BIT, true);
		gDLL->getInterfaceIFace()->setDirty(ColoredPlots_DIRTY_BIT, true);
	}

#ifdef YIELD_VALUE_CACHING
	AI_NoteWorkerChange();
#endif
}

// COMMIT + ANNOUNCE, and nothing else -- the body the save read hands each deserialized worked slot to.
void CvCity::setWorkingPlotInternal(int iIndex, bool bNewValue)
{
	FASSERT_BOUNDS(0, NUM_CITY_PLOTS, iIndex);

	if (isWorkingPlot(iIndex) == bNewValue)
	{
		return;
	}
	m_pabWorkingPlot[iIndex] = bNewValue;

	CvPlot* pPlot = getCityIndexPlot(iIndex);
	if (bNewValue)
	{
		FAssertMsg(pPlot != NULL, CvString::format("pPlot was null for iIndex %d", iIndex).c_str());
	}
	// The plot's IS_WORKED verdict flipped. The fact belongs to the PLOT (iSrcLoc) but only the city can
	// attribute it, so both ride. A city-radius index off the map edge resolves to no plot -- there is then
	// no plot whose state changed, so there is no fact to announce.
	if (pPlot == NULL)
	{
		return;
	}
	if (isCitizenJuggling())
	{
		m_bJuggleDeferredWork = true;   // the NET flip announces at the close, not this probe
		return;
	}
	if (bNewValue)
	{
		emitPlotWorkedAdded(GC.getMap().plotNum(pPlot->getX(), pPlot->getY()), (int)getOwner(), getID());
	}
	else
	{
		emitPlotWorkedRemoved(GC.getMap().plotNum(pPlot->getX(), pPlot->getY()), (int)getOwner(), getID());
	}
}


void CvCity::setWorkingPlot(int iIndex, bool bNewValue)
{
	FASSERT_BOUNDS(0, NUM_CITY_PLOTS, iIndex);

	if (isWorkingPlot(iIndex) == bNewValue)
	{
		return;
	}
	setWorkingPlotInternal(iIndex, bNewValue);
	// The EFFECT half, which the read deliberately does not run: working population, the plot builder and
	// symbol refresh, the yield rider and the interface dirty bits.
	processWorkingPlot(iIndex, bNewValue ? 1 : -1);
}


void CvCity::alterWorkingPlot(int iIndex)
{
	FASSERT_BOUNDS(0, NUM_CITY_PLOTS, iIndex);

	if (iIndex != CITY_HOME_PLOT)
	{
		CvPlot* pPlot = getCityIndexPlot(iIndex);
		FAssertMsg(pPlot != NULL, CvString::format("pPlot was null for iIndex %d", iIndex).c_str());
		if (pPlot != NULL)
		{
			if (canWork(pPlot))
			{
				setCitizensAutomated(false);

				if (isWorkingPlot(iIndex))
				{
					setWorkingPlot(iIndex, false);

					if (GC.getDEFAULT_SPECIALIST() != NO_SPECIALIST)
					{
						changeSpecialistCount((SpecialistTypes)GC.getDEFAULT_SPECIALIST(), 1);
					}
					else AI_addBestCitizen(false, true);
				}
				else if (extraPopulation() > 0 || AI_removeWorstCitizen())
				{
					setWorkingPlot(iIndex, true);
				}
			}
			else if (pPlot->getOwner() == getOwner())
			{
				pPlot->setWorkingCityOverride(this);
			}
		}
	}
	else setCitizensAutomated(true);
}


// The building OPERATE verdicts, forwarded from the ENABLER's operating set (enabler.md §3.2) -- the one place
// the active/dormant fixpoint is computed. ⛔ Never a second derivation here: re-deriving it from prereq getters
// is the camouflaged ride-in [DEC-calc-zero-ride-in] bans, and it is what the deleted checkBuildings did.
// `active` is present ∧ operate-holds ∧ ¬dormant-trigger ∧ ¬obsolete, so ACTIVE already implies present.
bool CvCity::isActiveBuilding(BuildingTypes eIndex) const
{
	FASSERT_BOUNDS(0, GC.getNumBuildingInfos(), eIndex);
	return m_operatingBuildings.active.count((int)eIndex) != 0;
}

// DORMANT = PRESENT but outside the operating set. The presence test is load-bearing: a building the city does
// not have is not dormant, it is simply absent -- which is exactly what the retired disabled-list answered.
bool CvCity::isDormantBuilding(BuildingTypes eIndex) const
{
	FASSERT_BOUNDS(0, GC.getNumBuildingInfos(), eIndex);
	return hasBuilding(eIndex) && m_operatingBuildings.active.count((int)eIndex) == 0;
}

// The religious limit is no longer a separate store: a building needing a religion is an ordinary
// `requires.operate` clause the enabler resolves, so FULLY-active and active are the same verdict.
bool CvCity::hasFullyActiveBuilding(const BuildingTypes eIndex) const
{
	return isActiveBuilding(eIndex);
}


void CvCity::changeHasBuilding(const BuildingTypes eIndex, const bool bNewValue)
{
	if (bNewValue)
	{
		setHasBuilding(eIndex, true, getOwner(), GC.getGame().getGameTurnYear());
	}
	else setHasBuilding(eIndex, false, NO_PLAYER, MIN_INT);
}


void CvCity::setHasBuilding(const BuildingTypes eType, const bool bNewValue, const PlayerTypes eOriginalOwner, const int iOriginalTime, const bool bFirst)
{
	PROFILE_EXTRA_FUNC();
	FASSERT_BOUNDS(0, GC.getNumBuildingInfos(), eType);

	// DEC-empire-level-buildings: an identity.empireLevel building is held by the PLAYER, once, and is never
	// present in any city -- every placing system funnels through here, so the routing lives here ONCE and the
	// placing systems never learn the tag exists. The city stores nothing.
	if (GC.getBuildingInfo(eType).isEmpireLevel())
	{
		GET_PLAYER(getOwner()).setHasEmpireBuilding(eType, bNewValue, bFirst);
		return;
	}

	if (bNewValue != hasBuilding(eType))
	{
		const CvBuildingInfo& kBuilding = GC.getBuildingInfo(eType);

#ifdef YIELD_VALUE_CACHING
		ClearYieldValueCache(); // A new building can change yield rates
#endif
		invalidateCachedCanTrainForUnit(NO_UNIT);

		alterBuildingLedger(eType, bNewValue, eOriginalOwner, iOriginalTime);

		setupBuilding(kBuilding, eType, bNewValue, bFirst);

		if (bNewValue) // Building addition
		{
			processBuilding(eType, 1, false, true);
		}
		else // Building removal
		{
			// A DORMANT building contributed nothing, so there is nothing to un-process -- only an ACTIVE one
			// needs its contribution removed. The verdict is the ENABLER's operating set (enabler.md §3.2), never
			// a legacy disabled-flag ([DEC-calc-zero-ride-in]).
			if (m_operatingBuildings.active.count((int)eType) != 0)
			{
				processBuilding(eType, -1, false, true);
			}
		}

		// #430 event spine: the PRESENCE fact, at the presence choke point -- fires on EVERY genuine has-flip,
		// including a building removed while disabled (which skips processBuilding entirely). The processed
		// (operating) crossing is the enabler's separate SEVT_BUILDING_ACTIVATED / _DORMANTED.
		// The two directions are two FACTS, so the branch is here ONCE rather than in every consumer's body
		// ([DEC-facts-name-happenings]).
		if (bNewValue)
		{
			emitCityBuildingAdded(getID(), getOwner(), (int)eType, bFirst);
		}
		else
		{
			emitCityBuildingRemoved(getID(), getOwner(), (int)eType);
		}

		// (Buildings REPLACED by this one are not disabled here: a predecessor standing under its successor is
		// reversible DORMANCY, authored as the target's `requires.operate.dormant` and resolved by the enabler's
		// operate fixpoint -- enabler.md §2. Re-deriving it here would be the hand re-derivation
		// [DEC-calc-zero-ride-in] bans.)
	}
}


// Toffer - Called by setupBuilding(), game-count left out as it doesn't need to be maintained here.
void CvCity::handleBuildingCounts(const BuildingTypes eBuilding, const int iChange, const bool bWonder)
{
	GET_PLAYER(getOwner()).changeBuildingCount(eBuilding, iChange);
	GET_TEAM(getTeam()).changeBuildingCount(eBuilding, iChange);
	changeNumBuildings(iChange);

	if (bWonder)
	{
		if (isWorldWonder(eBuilding))
		{
			changeNumWorldWonders(iChange);
		}
		else if (isTeamWonder(eBuilding))
		{
			changeNumTeamWonders(iChange);
		}
		else if (isNationalWonder(eBuilding))
		{
			changeNumNationalWonders(iChange);
		}
	}
}


// Toffer - Function added only for readability reasons.
void CvCity::setupBuilding(const CvBuildingInfo& kBuilding, const BuildingTypes eType, const bool bNewValue, const bool bFirst)
{
	PROFILE_EXTRA_FUNC();
	const int iChange = bNewValue ? 1 : -1;

	handleBuildingCounts(eType, iChange, isLimitedWonder(eType) && !kBuilding.isNoInstanceLimit());

	if (!bNewValue) // Building removal
	{
		if (!isWorldWonder(eType)) // World wonders can only be built once, so the count is essential to keep track of.
		{
			GC.getGame().changeNumBuildings(eType, iChange);
		}
	}
	else // Building addition
	{
		GC.getGame().changeNumBuildings(eType, iChange);

		CascadeCondDeps kVoteDeps;
		EnablerKernel::scanCondDeps(kBuilding.requiresBuild(), kVoteDeps, false, false);
		if (kVoteDeps.stateReligionInCity && kBuilding.getDiploVoteType() > -1)
		{
			const VoteSourceTypes eVoteSource = (VoteSourceTypes) kBuilding.getDiploVoteType();
			if (eVoteSource > NO_VOTESOURCE && GC.getGame().getVoteSourceReligion(eVoteSource) == NO_RELIGION)
			{
				GC.getGame().setVoteSourceReligion(eVoteSource, GET_PLAYER(getOwner()).getStateReligion(), true);
			}
		}

		if (kBuilding.isAllowsNukes())
		{
			GET_PLAYER(getOwner()).makeNukesValid(true);
		}

		// Toffer: Certain things should only apply when the building is built the very first time.
		if (bFirst) // Not city copy on owner change, actually built.
		{
			// ⛔ The first-build PAYLOAD is the trigger engine's, and it is not duplicated here. The local and
			// empire population pulses, the golden age and the free techs are the source's `grants` applied by
			// tr_applyBuildingFirstBuild off SEVT_BUILDING_ADDED, gated exactly as this block gated them
			// (triggers.md: a grant is a trigger with a null condition). Keeping a copy beside it is the
			// double-apply the roadmap names as the worst class of surviving legacy -- two live paths handing
			// out the same thing, detectable only by noticing the effect landed twice.
			// What stays here is what is NOT a payload: the capital designation, the corporation HQ founding,
			// and the wonder replay/announcement chrome.
			if (kBuilding.providesAmenity(CLS_AMENITY_CAPITAL))
			{
				GET_PLAYER(getOwner()).setCapitalCity(this);
			}

			// `enables.corporations` -- the building unlocks a corporation, and founding it here designates this
			// city as its headquarters. The edge family is the load-compiled forward view ([DEC-one-reverse-view]).
			if (const std::vector<int>* pFounds = kBuilding.edge(EDGEF_ENABLES, EDGEB_CORPORATIONS))
			{
				for (std::vector<int>::const_iterator it = pFounds->begin(); it != pFounds->end(); ++it)
				{
					if (!GC.getGame().isCorporationFounded((CorporationTypes)*it))
					{
						setHeadquarters((CorporationTypes)*it);
					}
				}
			}

			if (GC.getGame().isFinalInitialized() && !gDLL->GetWorldBuilderMode())
			{
				if (isWorldWonder(eType))
				{
					GC.getGame().addReplayMessage(
						REPLAY_MESSAGE_MAJOR_EVENT, getOwner(),
						gDLL->getText("TXT_KEY_MISC_COMPLETES_WONDER", GET_PLAYER(getOwner()).getNameKey(), kBuilding.getTextKeyWide()),
						getX(), getY(), GC.getCOLOR_BUILDING_TEXT()
					);
					for (int iI = 0; iI < MAX_PC_PLAYERS; iI++)
					{
						if (GET_PLAYER((PlayerTypes)iI).isAlive() && GET_PLAYER((PlayerTypes)iI).isHumanPlayer())
						{
							if (isRevealed(GET_PLAYER((PlayerTypes)iI).getTeam(), false))
							{
								AddDLLMessage(
									(PlayerTypes)iI, false, GC.getEVENT_MESSAGE_TIME(),
									gDLL->getText("TXT_KEY_MISC_WONDER_COMPLETED", GET_PLAYER(getOwner()).getNameKey(), kBuilding.getTextKeyWide()),
									"AS2D_WONDER_BUILDING_BUILD", MESSAGE_TYPE_MAJOR_EVENT,
									kBuilding.getArtInfo()->getButton(), GC.getCOLOR_BUILDING_TEXT(), getX(), getY(), true, true
								);
							}
							else
							{
								AddDLLMessage(
									(PlayerTypes)iI, false, GC.getEVENT_MESSAGE_TIME(),
									gDLL->getText("TXT_KEY_MISC_WONDER_COMPLETED_UNKNOWN", kBuilding.getTextKeyWide()),
									"AS2D_WONDER_BUILDING_BUILD", MESSAGE_TYPE_MAJOR_EVENT,
									kBuilding.getArtInfo()->getButton(), GC.getCOLOR_BUILDING_TEXT()
								);
							}
						}
					}
				}
			}
		}
	}
#ifdef THE_GREAT_WALL
	//great wall
	if (bFirst) // Not city copy on owner change, actually built or destroyed.
	{
		if (kBuilding.providesAmenity(CLS_AMENITY_BORDER_OBSTACLE))
		{
			bool bHas = false;
			foreach_(const BuildingTypes eTypeX, getHasBuildings())
			{
				if (eType != eTypeX && GC.getBuildingInfo(eTypeX).providesAmenity(CLS_AMENITY_BORDER_OBSTACLE) && !isDormantBuilding(eTypeX))
				{
					bHas = true;
					break;
				}
			}
			if (bNewValue)
			{
				if (!bHas)
				{
					processGreatWall(true, true);
				}
			}
			else if (bHas)
			{
				processGreatWall(true, true);
			}
		}
	}
#endif // THE_GREAT_WALL
}

bool CvCity::processGreatWall(bool bIn, bool bForce, bool bSeeded)
{
	PROFILE_EXTRA_FUNC();
	/*
	> TBNote: I've found both a crash scenario in PBEM and an infinite hang scenario in single player.
	> A player complained about exceedingly strange graphic artifice when they encircle the globe with a singular culture that possesses the GW and the hang looked to have a similar basis.
	> Unfortunately, I'm no front end specialist and I don't have a theory on how to isolate the cause except to say that it's the new way that the GW is being processed that's causing the crashes and hangs.
	> So for now, this is disabled.  I may eventually create a manual option to turn it off or on and that would give us the ability to turn it off only if it's creating problems.
	> Better would be to FIX it but I've got no clue on that.
	See https://github.com/caveman2cosmos/Caveman2Cosmos/issues/44
	*/

#ifdef THE_GREAT_WALL
	if (!bForce && !GC.getENABLE_VIEWPORTS() && !GC.getDefineBOOL("DYNAMIC_GREAT_WALL"))
	{
		return true;
	}

	bool bHasGreatWall = false;
	if (bIn || !bSeeded)
	{
		foreach_(const BuildingTypes eTypeX, getHasBuildings())
		{
			if (GC.getBuildingInfo(eTypeX).providesAmenity(CLS_AMENITY_BORDER_OBSTACLE) && !isDormantBuilding(eTypeX))
			{
				bHasGreatWall = true;
				break;
			}
		}
	}
	else bHasGreatWall = m_bIsGreatWallSeed;


	if (bHasGreatWall)
	{
		CvCity* pUseCity = NULL;

		if (isInViewport())
		{
			pUseCity = this;
		}
		else
		{
			//	Need to find a culturally connected city that IS in the current viewport
			int iDummyVal;
			CvUnitSelectionCriteria	noGrowthCriteria;

			noGrowthCriteria.m_bIgnoreGrowth = true;

			UnitTypes eDummyUnit = AI_bestUnitAI(UNITAI_ATTACK, iDummyVal, true, true, &noGrowthCriteria);

			if (eDummyUnit == NO_UNIT)
			{
				eDummyUnit = AI_bestUnitAI(UNITAI_CITY_DEFENSE, iDummyVal, true, true, &noGrowthCriteria);

				FAssert(eDummyUnit != NO_UNIT);
			}
			if (eDummyUnit != NO_UNIT)
			{
				CvUnit* pTempUnit = GET_PLAYER(getOwner()).getTempUnit(eDummyUnit, getX(), getY());
				CvReachablePlotSet	plotSet(pTempUnit->getGroup(), MOVE_OUR_TERRITORY, MAX_INT);

				for (CvReachablePlotSet::const_iterator itr = plotSet.begin(); itr != plotSet.end(); ++itr)
				{
					const CvCity* pCity = itr.plot()->getPlotCity();

					if (pCity != NULL && pCity->isInViewport())
					{
						pUseCity = pCity;
						break;
					}
				}
				GET_PLAYER(getOwner()).releaseTempUnit();
			}
		}

		//	If no suitable city is within the viewport we'll have to move the viewport
		bool bViewportMoved = false;
		int iOldViewportXOffset = 0;
		int iOldViewportYOffset = 0;

		if (pUseCity == NULL && !bSeeded)
		{
			pUseCity = this;
			bViewportMoved = true;

			GC.getCurrentViewport()->getMapOffset(iOldViewportXOffset, iOldViewportYOffset);
			GC.getCurrentViewport()->setOffsetToShow(getX(), getY());
		}
		//	remove or re-add
		if (pUseCity != NULL)
		{
			if (bIn)
			{
				pUseCity->m_bIsGreatWallSeed = true;
				gDLL->getEngineIFace()->AddGreatWall(pUseCity);
			}
			else
			{
				pUseCity->m_bIsGreatWallSeed = false;
				gDLL->getEngineIFace()->RemoveGreatWall(pUseCity);
			}
		}

		if (bViewportMoved)
		{
			GC.getCurrentViewport()->setMapOffset(iOldViewportXOffset, iOldViewportYOffset);
		}

		return true;
	}
#endif // THE_GREAT_WALL
	return false;
}


// Toffer - ToDo - Would make more sense to store this info in the CvArea object, mapped to player, rather than duplicating the info across all cities in the area.



bool CvCity::isHasReligion(ReligionTypes eIndex) const
{
	FASSERT_BOUNDS(0, GC.getNumReligionInfos(), eIndex);
	return m_pabHasReligion[eIndex];
}



void CvCity::applyReligionModifiers(const ReligionTypes eIndex, const bool bValue)
{
}

void CvCity::setHasReligion(ReligionTypes eIndex, bool bNewValue, bool bAnnounce, bool bArrows)
{
	PROFILE_EXTRA_FUNC();
	FASSERT_BOUNDS(0, GC.getNumReligionInfos(), eIndex);

	if (isHasReligion(eIndex) != bNewValue)
	{
		for (int iVoteSource = 0; iVoteSource < GC.getNumVoteSourceInfos(); ++iVoteSource)
		{
			processVoteSourceBonus((VoteSourceTypes)iVoteSource, false);
		}

		// The commit and the fact; the effects continue below.
		setHasReligionInternal(eIndex, bNewValue);

		for (int iVoteSource = 0; iVoteSource < GC.getNumVoteSourceInfos(); ++iVoteSource)
		{
			processVoteSourceBonus((VoteSourceTypes)iVoteSource, true);
		}

		GET_PLAYER(getOwner()).changeHasReligionCount(eIndex, ((isHasReligion(eIndex)) ? 1 : -1));

		// Religion changes may change what is buildable

		AI_setAssignWorkDirty(true);

		setInfoDirty(true);

		if (isHasReligion(eIndex))
		{
			GC.getGame().makeReligionFounded(eIndex, getOwner());

			if (bAnnounce)
			{
				if (GC.getGame().getHolyCity(eIndex) != this)
				{
					for (int iI = 0; iI < MAX_PC_PLAYERS; iI++)
					{
						if (GET_PLAYER((PlayerTypes)iI).isAlive()
						&&  GET_PLAYER((PlayerTypes)iI).isHumanPlayer()
						&& isRevealed(GET_PLAYER((PlayerTypes)iI).getTeam(), false)
						&& (getOwner() == iI || GET_PLAYER((PlayerTypes)iI).getStateReligion() == eIndex || GET_PLAYER((PlayerTypes)iI).hasHolyCity(eIndex)))
						{
							AddDLLMessage(
								(PlayerTypes) iI, false, GC.getEVENT_MESSAGE_TIME_LONG(),
								gDLL->getText("TXT_KEY_MISC_RELIGION_SPREAD", GC.getReligionInfo(eIndex).getTextKeyWide(), getNameKey()),
								GC.getReligionInfo(eIndex).getSound(), MESSAGE_TYPE_MAJOR_EVENT, GC.getReligionInfo(eIndex).getButton(),
								GC.getCOLOR_WHITE(), getX(), getY(), bArrows, bArrows
							);
						}
					}
				}

				if (isHuman()
				&& GET_PLAYER(getOwner()).getHasReligionCount(eIndex) == 1
				&& GET_PLAYER(getOwner()).canConvert(eIndex)
				&& GET_PLAYER(getOwner()).getStateReligion() == NO_RELIGION)
				{
					CvPopupInfo* pInfo = new CvPopupInfo(BUTTONPOPUP_CHANGERELIGION);
					if (NULL != pInfo)
					{
						pInfo->setData1(eIndex);
						gDLL->getInterfaceIFace()->addPopup(pInfo, getOwner());
					}
				}
			}
		}
		else if (bAnnounce)
		{
			for (int iI = 0; iI < MAX_PC_PLAYERS; iI++)
			{
				if (GET_PLAYER((PlayerTypes)iI).isAlive()
				&&  GET_PLAYER((PlayerTypes)iI).isHumanPlayer()
				&& isRevealed(GET_PLAYER((PlayerTypes)iI).getTeam(), false)
				&& (getOwner() == iI || GET_PLAYER((PlayerTypes)iI).getStateReligion() == eIndex || GET_PLAYER((PlayerTypes)iI).hasHolyCity(eIndex)))
				{
					AddDLLMessage(
						(PlayerTypes) iI, false, GC.getEVENT_MESSAGE_TIME_LONG(),
						gDLL->getText("TXT_KEY_MISC_RELIGION_DECAY", getNameKey(), GC.getReligionInfo(eIndex).getTextKeyWide()),
						GC.getReligionInfo(eIndex).getSound(), MESSAGE_TYPE_MAJOR_EVENT, GC.getReligionInfo(eIndex).getButton(),
						GC.getCOLOR_RED(), getX(), getY(), bArrows, bArrows
					);
				}
			}
		}

		// Python Event
		if (bNewValue)
		{
			CvEventReporter::getInstance().religionSpread(eIndex, getOwner(), this);
		}
		else CvEventReporter::getInstance().religionRemove(eIndex, getOwner(), this);

		applyReligionModifiers(eIndex, bNewValue);

	}

	for (int iI = 0; iI < MAX_PLAYERS; iI++)
	{
		if (!GET_PLAYER((PlayerTypes) iI).isHumanPlayer()
		&& (getTeam() == GET_PLAYER((PlayerTypes)iI).getTeam() || GET_TEAM(getTeam()).isVassal(GET_PLAYER((PlayerTypes)iI).getTeam())))
		{
			GET_PLAYER((PlayerTypes)iI).AI_setHasInquisitionTarget();
		}
	}
}


void CvCity::processVoteSourceBonus(VoteSourceTypes eVoteSource, bool bActive)
{
	PROFILE_EXTRA_FUNC();
	if (!GET_PLAYER(getOwner()).isLoyalMember(eVoteSource))
	{
		return;
	}

	if (GC.getGame().isDiploVote(eVoteSource))
	{
		ReligionTypes eReligion = GC.getGame().getVoteSourceReligion(eVoteSource);

		SpecialistTypes eSpecialist = (SpecialistTypes)GC.getVoteSourceInfo(eVoteSource).getFreeSpecialist();

		if (NO_SPECIALIST != eSpecialist && (NO_RELIGION == eReligion || isHasReligion(eReligion)))
		{
			changeFreeSpecialistCount(eSpecialist, bActive ? 1 : -1);
		}

		if (NO_RELIGION != eReligion && isHasReligion(eReligion))
		{
			for (int iYield = 0; iYield < NUM_YIELD_TYPES; ++iYield)
			{
				int iChange = GC.getVoteSourceInfo(eVoteSource).getReligionYield(iYield);
				if (!bActive)
				{
					iChange = -iChange;
				}

				if (0 != iChange)
				{
					foreach_(const BuildingTypes eBuilding, BuildingsRepo::get().byReligion(eReligion))
					{
						changeBuildingYieldChange(eBuilding, (YieldTypes)iYield, iChange);
					}
				}
			}

			for (int iCommerce = 0; iCommerce < NUM_COMMERCE_TYPES; ++iCommerce)
			{
				int iChange = GC.getVoteSourceInfo(eVoteSource).getReligionCommerce(iCommerce);
				if (!bActive)
				{
					iChange = -iChange;
				}

				if (0 != iChange)
				{
					foreach_(const BuildingTypes eBuilding, BuildingsRepo::get().byReligion(eReligion))
					{
						changeBuildingCommerceChange(eBuilding, (CommerceTypes)iCommerce, iChange);
					}
				}
			}
		}
	}
}


bool CvCity::isHasCorporation(CorporationTypes eIndex) const
{
	FASSERT_BOUNDS(0, GC.getNumCorporationInfos(), eIndex);
	return m_pabHasCorporation[eIndex];
}

void CvCity::setHasCorporation(CorporationTypes eIndex, bool bNewValue, bool bAnnounce, bool bArrows)
{
	PROFILE_EXTRA_FUNC();
	FASSERT_BOUNDS(0, GC.getNumCorporationInfos(), eIndex);

	if (isHasCorporation(eIndex) != bNewValue)
	{
		if (bNewValue)
		{
			bool bReplacedHeadquarters = false;
			for (int iCorp = 0; iCorp < GC.getNumCorporationInfos(); ++iCorp)
			{
				if (iCorp != eIndex && isHasCorporation((CorporationTypes)iCorp)
				&& GC.getGame().isCompetingCorporation((CorporationTypes)iCorp, eIndex))
				{
					if (GC.getGame().getHeadquarters((CorporationTypes)iCorp) == this)
					{
						GC.getGame().replaceCorporation((CorporationTypes)iCorp, eIndex);
						bReplacedHeadquarters = true;
					}
					else setHasCorporation((CorporationTypes)iCorp, false, false);
				}
			}
			if (bReplacedHeadquarters)
			{
				return; // already set the corporation in this city
			}
		}

		// The commit and the fact; the effects continue below. ⚠ The early-return HQ-replace path above commits
		// this city's corporation through a nested setHasCorporation, which lands through here on its own.
		setHasCorporationInternal(eIndex, bNewValue);

		GET_PLAYER(getOwner()).changeHasCorporationCount(eIndex, ((isHasCorporation(eIndex)) ? 1 : -1));

		CvCity* pHeadquarters = GC.getGame().getHeadquarters(eIndex);
		if (NULL != pHeadquarters)
		{
			pHeadquarters->updateCorporation();
		}
		updateCorporation();

		AI_setAssignWorkDirty(true);

		setInfoDirty(true);

		if (isHasCorporation(eIndex))
		{
			GC.getGame().makeCorporationFounded(eIndex, getOwner());
		}

		if (bAnnounce)
		{
			for (int iI = 0; iI < MAX_PC_PLAYERS; iI++)
			{
				if (GET_PLAYER((PlayerTypes)iI).isAlive() && GET_PLAYER((PlayerTypes)iI).isHumanPlayer()
				&& (getOwner() == iI || GET_PLAYER((PlayerTypes)iI).hasHeadquarters(eIndex)))
				{
					if (bNewValue)
					{
						{
							AddDLLMessage(
								(PlayerTypes) iI, false, GC.getEVENT_MESSAGE_TIME(),
								gDLL->getText(
									"TXT_KEY_MISC_CORPORATION_SPREAD",
									GC.getCorporationInfo(eIndex).getTextKeyWide(), getNameKey()
								),
								GC.getCorporationInfo(eIndex).getSound(), MESSAGE_TYPE_MAJOR_EVENT,
								GC.getCorporationInfo(eIndex).getButton(),
								GC.getCOLOR_WHITE(), getX(), getY(), bArrows, bArrows
							);
						}

						if (getOwner() == iI)
						{
							CvWStringBuffer szBonusString;
							GAMETEXT.setCorporationHelpCity(szBonusString, eIndex, this);

							CvWString szBonusList;
							bool bFirst = true;
							foreach_(const int iConsumedBonus, GC.getCorporationInfo(eIndex).getConsumedBonuses())
							{
								const BonusTypes eBonus = static_cast<BonusTypes>(iConsumedBonus);
								CvWString szTemp;
								szTemp.Format(L"%s", GC.getBonusInfo(eBonus).getDescription());
								setListHelp(szBonusList, L"", szTemp, L", ", bFirst);
								bFirst = false;
							}
							if (!bFirst)
							{
								AddDLLMessage(
									(PlayerTypes) iI, false, GC.getEVENT_MESSAGE_TIME(),
									gDLL->getText("TXT_KEY_MISC_CORPORATION_SPREAD_BONUS", GC.getCorporationInfo(eIndex).getTextKeyWide(), szBonusString.getCString(), getNameKey(), szBonusList.GetCString()),
									GC.getCorporationInfo(eIndex).getSound(), MESSAGE_TYPE_MINOR_EVENT, GC.getCorporationInfo(eIndex).getButton(), GC.getCOLOR_WHITE(), getX(), getY(), bArrows, bArrows
								);
							}

						}
					}
					else
					{
						AddDLLMessage(
							(PlayerTypes) iI, false, GC.getEVENT_MESSAGE_TIME(),
							gDLL->getText("TXT_KEY_MISC_CORPORATION_DECAY", GC.getCorporationInfo(eIndex).getTextKeyWide(), getNameKey()),
							GC.getCorporationInfo(eIndex).getSound(), MESSAGE_TYPE_MAJOR_EVENT, GC.getCorporationInfo(eIndex).getButton(),
							GC.getCOLOR_RED(), getX(), getY(), bArrows, bArrows
						);
					}
				}
			}
		}

		// Python Event
		if (bNewValue)
		{
			CvEventReporter::getInstance().corporationSpread(eIndex, getOwner(), this);
		}
		else CvEventReporter::getInstance().corporationRemove(eIndex, getOwner(), this);
	}
}


int CvCity::getNumTradeRouteSlots() const
{
	return (int)m_paTradeCities.size();
}


CvCity* CvCity::getTradeCity(int iIndex) const
{
	FASSERT_BOUNDS(0, (int)m_paTradeCities.size(), iIndex);
	return getCity(m_paTradeCities[iIndex]);
}


int CvCity::getTradeRoutes() const
{
	int iTradeRoutes = GC.getGame().getTradeRoutes();
	iTradeRoutes += GET_PLAYER(getOwner()).getTradeRoutes();
	iTradeRoutes += getExtraTradeRoutes();

	return std::max(0, std::min(iTradeRoutes, getMaxTradeRoutes()));
}


void CvCity::clearTradeRoutes()
{
	PROFILE_EXTRA_FUNC();
	for (int cityIdx = 0; cityIdx < static_cast<int>(m_paTradeCities.size()); cityIdx++)
	{
		CvCity* pLoopCity = getTradeCity(cityIdx);

		if (pLoopCity != NULL)
		{
			pLoopCity->setTradeRoute(getOwner(), false);
		}
	}
	m_paTradeCities = std::vector<IDInfo>(getMaxTradeRoutes());
}


// XXX eventually, this needs to be done when roads are built/destroyed...
void CvCity::updateTradeRoutes()
{
	PROFILE_FUNC();
	const int iMaxTradeRoutes = getMaxTradeRoutes();

	std::vector<int> paiBestValue(iMaxTradeRoutes, 0);

	clearTradeRoutes();

	if (!isDisorder() && !isPlundered() && !isQuarantined())
	{
		const int iTradeRoutes = getTradeRoutes();

		FAssert(iTradeRoutes <= iMaxTradeRoutes);

		for (int iI = 0; iI < MAX_PLAYERS; iI++)
		{
			if (GET_PLAYER(getOwner()).canHaveTradeRoutesWith((PlayerTypes)iI))
			{
				foreach_(CvCity* pLoopCity, GET_PLAYER((PlayerTypes)iI).cities())
				{
					if (pLoopCity != this)
					{
						if (!(pLoopCity->isTradeRoute(getOwner())) || (getTeam() == GET_PLAYER((PlayerTypes)iI).getTeam()))
						{
							if (pLoopCity->plotGroup(getOwner()) == plotGroup(getOwner()) || GC.getIGNORE_PLOT_GROUP_FOR_TRADE_ROUTES())
							{
								const int iValue = calculateTradeProfit(pLoopCity);

								for (int iJ = 0; iJ < iTradeRoutes; iJ++)
								{
									if (iValue > paiBestValue[iJ])
									{
										for (int iK = (iTradeRoutes - 1); iK > iJ; iK--)
										{
											paiBestValue[iK] = paiBestValue[(iK - 1)];
											m_paTradeCities[iK] = m_paTradeCities[(iK - 1)];
										}

										paiBestValue[iJ] = iValue;
										m_paTradeCities[iJ] = pLoopCity->getIDInfo();

										break;
									}
								}
							}
						}
					}
				}
			}
		}
	}

	int iTradeProfit = 0;

	for (int iI = 0; iI < iMaxTradeRoutes; iI++)
	{
		CvCity* pLoopCity = getTradeCity(iI);

		if (pLoopCity != NULL)
		{
			pLoopCity->setTradeRoute(getOwner(), true);

			iTradeProfit += calculateTradeProfit(pLoopCity);
		}
	}

	// ⛔ THE STORED YIELD IS ×100 AND REDUCES NOWHERE. A `/100` on this line floors the city's whole trade
	// contribution to a WHOLE UNIT before it reaches TIER-1 BASE, where the combine lifts it back ×100 and the
	// percent stack multiplies what is left -- the fraction is not deferred to the edge, it is gone
	// ([fixed-point-and-scales.md §4c-bis]: an amount reduces once, at the surface that shows it).
	// ⚑ This total and the per-route list agree BECAUSE both live on the ×100 plane: the list renders
	// hundredths per row, this sums the same hundredths.
	for (int iI = 0; iI < NUM_YIELD_TYPES; iI++)
	{
		setTradeYield(((YieldTypes)iI), calculateTradeYield(((YieldTypes)iI), iTradeProfit)); // XXX could take this out if handled when CvPlotGroup changes...
	}
}


void CvCity::clearOrderQueue()
{
	PROFILE_EXTRA_FUNC();
	while (getOrderQueueLength() > 0)
	{
		popOrder(0, false, false, false);
	}

	if (getTeam() == GC.getGame().getActiveTeam() || GC.getGame().isDebugMode())
	{
		setInfoDirty(true);
	}
}


bool CvCity::pushFirstValidBuildListOrder(int iListID)
{
	PROFILE_EXTRA_FUNC();
	const CvPlayerAI& kPlayer = GET_PLAYER(getOwner());
	int index = kPlayer.m_pBuildLists->getIndexByID(iListID);
	if (index < 0)
		return false;
	const int iNum = kPlayer.m_pBuildLists->getListLength(index);
	for (int i = 0; i < iNum; i++)
	{
		const OrderData* pOrder = kPlayer.m_pBuildLists->getOrder(index, i);

		if (canContinueProduction(*pOrder))
		{
			pushOrder(pOrder->eOrderType, pOrder->iData1, pOrder->iData2, pOrder->bSave, false, true); //Calvitix, append buildings, not insert
			return true;
		}
	}
	return false;
}

void CvCity::pushOrder(OrderTypes eOrder, int iData1, int iData2, bool bSave, bool bPop, bool bAppend, bool bForce, CvPlot* deliveryDestination, UnitAITypes contractedAIType, uint8_t contractFlags)
{
	//bool bBuildingUnit = false;
	//bool bBuildingBuilding = false;

	if (bPop)
	{
		popOrder(0, false, false, false);
	}

	bool bValid = false;
	bool bJustAddedProcess = false;
	CvPlayerAI& owner = GET_PLAYER(getOwner());
	bool bIsHuman = owner.isHumanPlayer(true);

	OrderData order;

	switch (eOrder)
	{
		case ORDER_TRAIN:
		{
			const UnitTypes unitType = static_cast<UnitTypes>(iData1);
			if ((getUnitAvailability(unitType) == EnablerDomain::STATE_LISTED) || bForce)
			{
				const uint16_t iAIType = EXTERNAL_ORDER_IDATA(iData2);
				const UnitAITypes AIType = (iAIType == 0xFFFF) ?
					GC.getUnitInfo(unitType).getDefaultUnitAI()
					:
					static_cast<UnitAITypes>(iAIType)
					;

				contractedAIType = (iAIType == 0xFFFF)?
					static_cast<UnitAITypes>(0xFF)
					:
					contractedAIType
					;

				const short plotIndex = (deliveryDestination != NULL) ?
					order.unit.plotIndex = GC.getMap().plotNum(deliveryDestination->getX(), deliveryDestination->getY())
					:
					order.unit.plotIndex = 0xFFFF;



				order = OrderData::createUnitOrder(unitType, AIType, plotIndex, contractFlags, contractedAIType, bSave);

				int alreadyQueued = 0;
				for (std::vector<OrderData>::const_iterator it = m_orderQueue.begin(); it != m_orderQueue.end(); ++it) {
					if (it->eOrderType == ORDER_TRAIN && it->getUnitType() == order.getUnitType()) {
						alreadyQueued+= 1;
					}
				}

				//don't add the same unit if already 2 of them (if prod take more than 2 turns), and the first item in queue is not already finished, and the cost of the order is more than 3 turns
				if (!bIsHuman && (bAppend && !bForce) && ((alreadyQueued > 1 && getProductionTurnsLeft(unitType, 2) > 2) || alreadyQueued > 4)) {
					// [CIT/push/reject] -- the queue-level anti-spam guard blocked another copy of
					// this unit. A city that repeatedly trips this is the AI trying to spam-queue the
					// same dog/guard/healer; alreadyQueued shows how deep the pile already is.
					logCityAI(2, "[CIT/push/reject] city=%S owner=%d UNIT %S alreadyQueued=%d reason=spamGuard",
						getName().GetCString(), (int)getOwner(), GC.getUnitInfo(unitType).getDescription(), alreadyQueued);
					return;
				}

				owner.changeUnitMaking(unitType, 1);

				area()->changeNumTrainAIUnits(getOwner(), order.getUnitAIType(), 1);
				owner.AI_changeNumTrainAIUnits(order.getUnitAIType(), 1);

				CvEventReporter::getInstance().cityBuildingUnit(this, unitType);
				setUnitListInvalid();
				bValid = true;
			}
			break;
		}
		case ORDER_CONSTRUCT:
		{
			const BuildingTypes buildingType = static_cast<BuildingTypes>(iData1);
			if ((getBuildingAvailability(buildingType) == EnablerDomain::STATE_LISTED) || bForce)
			{
				order = OrderData::createBuildingOrder(buildingType, bSave);

				int alreadyQueued = 0;
				for (std::vector<OrderData>::const_iterator it = m_orderQueue.begin(); it != m_orderQueue.end(); ++it) {
					if (it->eOrderType == ORDER_CONSTRUCT && it->getBuildingType() == order.getBuildingType()) {
						alreadyQueued += 1;
					}
				}

				//don't add the same building after
				if (!bIsHuman && (bAppend && !bForce) && alreadyQueued > 0) {
					// [CIT/push/reject] -- duplicate-building guard blocked re-queuing this building.
					logCityAI(2, "[CIT/push/reject] city=%S owner=%d BUILDING %S alreadyQueued=%d reason=dupGuard",
						getName().GetCString(), (int)getOwner(), GC.getBuildingInfo(buildingType).getDescription(), alreadyQueued);
					return;
				}

				owner.changeBuildingMaking(buildingType, 1);

				const SpecialBuildingTypes eSpecialBuilding = static_cast<SpecialBuildingTypes>(GC.getBuildingInfo(buildingType).getSpecialBuildingType());
				if (eSpecialBuilding != NO_SPECIALBUILDING)
				{
					owner.changeBuildingGroupMaking(eSpecialBuilding, 1);
				}

				CvEventReporter::getInstance().cityBuildingBuilding(this, buildingType);
				setBuildingListInvalid();
				bValid = true;
			}
			break;
		}
		case ORDER_CREATE:
		{
			const ProjectTypes projectType = static_cast<ProjectTypes>(iData1);
			if (canCreate(projectType) || bForce)
			{
				order = OrderData::createProjectOrder(projectType, bSave);
				GET_TEAM(getTeam()).changeProjectMaking(projectType, 1);
				CvEventReporter::getInstance().cityBuildingProject(this, projectType);
				bValid = true;
			}
			break;
		}
		case ORDER_MAINTAIN:
		{
			const ProcessTypes processType = static_cast<ProcessTypes>(iData1);
			if (canMaintain(processType) || bForce)
			{
				order = OrderData::createProcessOrder(processType, bSave);
				CvEventReporter::getInstance().cityBuildingProcess(this, processType);
				bValid = true;
				//if (!bForce) bAppend = true;
				bJustAddedProcess = true;
			}
			break;
		}
		case ORDER_LIST:
		{
			bValid = true;
			break;
		}
		default: FErrorMsg("iOrder did not match a valid option");
	}

	if (!bValid)
	{
		return;
	}

	// [CIT/push] -- an order enters the city queue (intake stream). Catches contract-driven
	// units that bypass the AI_chooseProduction [CIT/order] path, so dog/guard/healer spam
	// pushed by the ContractBroker is visible here too. append=0 means it replaced the head.
	if (gCityLogLevel >= 2)
	{
		const char* szKind = "OTHER";
		const wchar_t* szName = L"-";
		switch (eOrder)
		{
		case ORDER_TRAIN:     szKind = "UNIT";     szName = GC.getUnitInfo(order.getUnitType()).getDescription(); break;
		case ORDER_CONSTRUCT: szKind = "BUILDING"; szName = GC.getBuildingInfo(order.getBuildingType()).getDescription(); break;
		case ORDER_CREATE:    szKind = "PROJECT";  szName = GC.getProjectInfo(order.getProjectType()).getDescription(); break;
		case ORDER_MAINTAIN:  szKind = "PROCESS";  break;
		case ORDER_LIST:      szKind = "LIST";     break;
		default: break;
		}
		logCityAI(2, "[CIT/push] city=%S owner=%d %s %S append=%d force=%d",
			getName().GetCString(), (int)getOwner(), szKind, szName, bAppend ? 1 : 0, bForce ? 1 : 0);
	}

	if (m_orderQueue.empty() && bIsHuman)
	{
		owner.setIdleCity(getID(), false);
	}


	if (bAppend && !m_orderQueue.empty() && !(m_orderQueue.begin()->eOrderType == ORDER_MAINTAIN))
	{
		m_orderQueue.push_back(order);
	}
	else
	{
		stopHeadOrder();
		m_orderQueue.insert(m_orderQueue.begin(), order);
	}

	// #430: the availability frontiers are the per-city/per-player ENABLER domains (event-maintained,
	// enabler.md par.7/8). Membership changes on HAVE events only (held flips on BUILT); a QUEUED building
	// leaves the fresh OFFER via the GATE (par.7.1 step 3) -- this emit triggers the one-id re-gate, whose
	// verdict reads the live queue.
	emitCityOrderAdded(getID(), (int)getOwner(), (int)eOrder, iData1);

	if (!bAppend || getOrderQueueLength() == 1)
	{
		// If the head order is a build list, resolve it
		if (eOrder != ORDER_LIST)
		{
			startHeadOrder();
		}
		else if (!pushFirstValidBuildListOrder(iData1))
		{
			// pop the list if there is nothing to construct on it any more
			popOrder(0);
		}
		else if (!bSave)
		{
			popOrder(1);
		}
	}

	//Check the Queue, and remove process if there's something else to do
	if (!bJustAddedProcess && getOrderQueueLength() > 1 && m_orderQueue[0].eOrderType == ORDER_MAINTAIN)
	{
		const PlayerTypes eOwner = getOwner();
		CvPlayerAI& player = GET_PLAYER(eOwner);
		const bool bFinancialTrouble = player.AI_isFinancialTrouble();
		if (!bFinancialTrouble)
		{ // IF financial Trouble, keep the process if needed
			popOrder(0);
		}
	}


	// Why does this cause a crash???

/*	if (bBuildingUnit)
	{
		CvEventReporter::getInstance().cityBuildingUnit(this, (UnitTypes)iData1);
	}
	else if (bBuildingBuilding)
	{
		CvEventReporter::getInstance().cityBuildingBuilding(this, (BuildingTypes)iData1);
	}*/

	if ((getTeam() == GC.getGame().getActiveTeam()) || GC.getGame().isDebugMode())
	{
		setInfoDirty(true);
	}
	if (isCitySelected())
	{
		gDLL->getInterfaceIFace()->setDirty(InfoPane_DIRTY_BIT, true);
		gDLL->getInterfaceIFace()->setDirty(SelectionButtons_DIRTY_BIT, true);
	}
}

void CvCity::popOrder(int orderIndex, bool bFinish, bool bChoose, bool bResolveList)
{
	PROFILE_FUNC();

	if (m_orderQueue.empty() || m_orderQueue.size() <= static_cast<size_t>(orderIndex))
	{
		return;
	}

	if (orderIndex == -1) orderIndex = m_orderQueue.size() - 1;

	FAssertMsg(orderIndex >= 0 && orderIndex < static_cast<int>(m_orderQueue.size()), "Order index out of bounds");

	const OrderData order = m_orderQueue[orderIndex];
	const OrderData externalOrder = order.to_external();

	const bool bWasFoodProduction = isFoodProduction();

	// [CIT/cancel] -- an order is popped WITHOUT finishing: the city switched/abandoned a
	// build, or doCheckProduction dropped it as obsolete/maxed (both route through here with
	// bFinish=false). progressLost = hammers forfeited. Repeated cancels on the same city =
	// production thrashing (a spam-adjacent outlier: the AI churns the queue instead of
	// committing). Guarded so we don't pay the lookups when city logging is off.
	if (!bFinish && gCityLogLevel >= 1)
	{
		const char* szKind = "OTHER";
		const wchar_t* szName = L"-";
		int iProgressLost = 0;
		switch (order.eOrderType)
		{
		case ORDER_TRAIN:     szKind = "UNIT";     szName = GC.getUnitInfo(order.getUnitType()).getDescription();         iProgressLost = getProgressOnUnit(order.getUnitType()); break;
		case ORDER_CONSTRUCT: szKind = "BUILDING"; szName = GC.getBuildingInfo(order.getBuildingType()).getDescription();  iProgressLost = getProgressOnBuilding(order.getBuildingType()); break;
		case ORDER_CREATE:    szKind = "PROJECT";  szName = GC.getProjectInfo(order.getProjectType()).getDescription(); break;
		default: break;
		}
		logCityAI(1, "[CIT/cancel] city=%S owner=%d %s %S progressLost=%d willChoose=%d",
			getName().GetCString(), (int)getOwner(), szKind, szName, iProgressLost, bChoose ? 1 : 0);
	}

	if (bFinish && order.bSave)
	{
		pushOrder(externalOrder.eOrderType, externalOrder.iData1, externalOrder.iData2, true, false, true);
	}
	CvPlayerAI& owner = GET_PLAYER(getOwner());

	UnitTypes eTrainUnit = NO_UNIT;
	BuildingTypes eConstructBuilding = NO_BUILDING;
	ProjectTypes eCreateProject = NO_PROJECT;

	switch (order.eOrderType)
	{
		case ORDER_TRAIN:
		{
			FAssertMsg(order.getUnitType() > -1 && order.getUnitType() < GC.getNumUnitInfos() && order.getUnitAIType() > -1 && order.getUnitAIType() < NUM_UNITAI_TYPES, "Train unit order is invalid");
			eTrainUnit = order.getUnitType();
			const UnitAITypes eTrainAIUnit = order.getUnitAIType();
			FAssertMsg(eTrainUnit != NO_UNIT, "eTrainUnit is expected to be assigned a valid unit type");
			FAssertMsg(eTrainAIUnit != NO_UNITAI, "eTrainAIUnit is expected to be assigned a valid unit AI type");

			owner.changeUnitMaking(eTrainUnit, -1);

			area()->changeNumTrainAIUnits(getOwner(), eTrainAIUnit, -1);
			owner.AI_changeNumTrainAIUnits(eTrainAIUnit, -1);

			setUnitListInvalid();

			if (bFinish)
			{
				AI_trained(eTrainUnit, eTrainAIUnit);

				const int iProgress = getProgressOnUnit(eTrainUnit);
				const int iRawOverflow = iProgress - getProductionNeeded(eTrainUnit);
				const int iMaxOverflow = getMaxProductionOverflow();
				const int iOverflow = std::min(iMaxOverflow, iRawOverflow);
				if (iOverflow > 0)
				{
					changeOverflowProduction(iOverflow);
				}
				changeProgressOnUnit(eTrainUnit, -iProgress);

				m_iLostProductionModified = std::max(0, iRawOverflow - iMaxOverflow);
				m_iGoldFromLostProduction = m_iLostProductionModified * GC.getMAXED_UNIT_GOLD_PERCENT() / 100;

				CvUnit* pUnit = owner.initUnit(eTrainUnit, getX(), getY(), eTrainAIUnit, NO_DIRECTION, GC.getGame().getSorenRandNum(10000, "AI Unit Birthmark"));
				if (!pUnit)
				{
					FErrorMsg("pUnit is expected to be assigned a valid unit object");
					return;
				}
				if (GC.getGame().isModderGameOption(MODDERGAMEOPTION_MAX_UNITS_PER_TILES)
				&& !pUnit->canEnterPlot(plot(), MoveCheck::IgnoreLocation))
				{
					pUnit->jumpToNearestValidPlot(false);
				}
				pUnit->finishMoves();

				// [CIT/produced] -- a unit actually rolls off the line (completion ground truth,
				// distinct from the AI_chooseProduction [CIT/order] decision). ownerHas (this exact
				// unit type) and aiRoleHas (this UNITAI role) expose unit-spam outliers: a city that
				// keeps completing the same dog/guard/healer drives these counts up turn over turn.
				// overflow/lost = hammers spilled past the cap (lost converted to gold downstream).
				logCityAI(1, "[CIT/produced] city=%S owner=%d UNIT %S unitAI=%d ownerHas=%d aiRoleHas=%d overflow=%d lost=%d",
					getName().GetCString(), (int)getOwner(), GC.getUnitInfo(eTrainUnit).getDescription(),
					(int)eTrainAIUnit, owner.getUnitCount(eTrainUnit), owner.AI_getNumAIUnits(eTrainAIUnit),
					iOverflow, m_iLostProductionModified);

				addProductionExperience(pUnit);

				const short iPlotIndex = order.unit.plotIndex;
				int iFlags;

				CvPlot* pRallyPlot = NULL;
				if (iPlotIndex != (const short)0xFFFF)
				{
					iFlags = MOVE_NO_ENEMY_TERRITORY;
					pRallyPlot = GC.getMap().plotByIndex(iPlotIndex);
				}
				else
				{
					iFlags = 0;
					pRallyPlot = getRallyPlot();
				}

				if (pRallyPlot)
				{
					const bool bIsUnitMission = (order.unit.contractFlags & AUX_CONTRACT_FLAG_IS_UNIT_CONTRACT) != 0;
					if (pRallyPlot != plot())
					{
						pUnit->getGroup()->pushMission(MISSION_MOVE_TO,
							pRallyPlot->getX(),
							pRallyPlot->getY(),
							iFlags,
							false,
							false,
							bIsUnitMission ? MISSIONAI_CONTRACT_UNIT : MISSIONAI_CONTRACT,
							pRallyPlot);
					}
					else
					{
						pUnit->getGroup()->AI_setMissionAI(bIsUnitMission ? MISSIONAI_CONTRACT_UNIT : MISSIONAI_CONTRACT, plot(), NULL);
					}
				}
				/*  There seems to be an issue with AI missionaries not working correctly - forcing automation  */
				/* is a kludgy way to fix this                                                                  */
				if (!isHuman())
				{
					pUnit->automate(AUTOMATE_RELIGION);
				}
				if (isHuman())
				{
					if (owner.isOption(PLAYEROPTION_START_AUTOMATED))
					{
						pUnit->automate(AUTOMATE_BUILD);
					}

					if (owner.isOption(PLAYEROPTION_MISSIONARIES_AUTOMATED))
					{
						pUnit->automate(AUTOMATE_RELIGION);
					}
					if (owner.isOption(PLAYEROPTION_MODDER_2))
					{
						CvPlot* pPlot = plot();
						if (pPlot != NULL)
						{
							if (pUnit->canSleep() || pUnit->canFortify())
							{
								pUnit->getGroup()->setActivityType(ACTIVITY_SLEEP);
							}
						}
					}
				}

				invalidateCachedCanTrainForUnit(eTrainUnit);

				//	KOSHLING - must not hold onto the pointer after the Python call or
				//	a crash occurs if that Python decides to destroy the just-built unit
				int iUnitId = pUnit->getID();

				CvEventReporter::getInstance().unitBuilt(this, pUnit);

				//	Python may have destroyed the unit we just built so refind by id
				pUnit = owner.getUnit(iUnitId);
				if (pUnit != NULL)
				{
					if (GC.getUnitInfo(eTrainUnit).getDomain() == DOMAIN_AIR)
					{
						if (GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
						{
							if (plot()->countNumAirUnitCargoVolume(getTeam()) > getSMAirUnitCapacity(getTeam()))
							{
								//probably need to look into this too.
								pUnit->jumpToNearestValidPlot();
							}
						}
						else if (plot()->countNumAirUnits(getTeam()) > getAirUnitCapacity(getTeam()))
						{
							pUnit->jumpToNearestValidPlot();  // can destroy unit
						}
					}

				}
			}
			break;
		}
		case ORDER_CONSTRUCT:
		{
			eConstructBuilding = order.getBuildingType();

			owner.changeBuildingMaking(eConstructBuilding, -1);

			const SpecialBuildingTypes eSpecialBuilding = static_cast<SpecialBuildingTypes>(GC.getBuildingInfo(eConstructBuilding).getSpecialBuildingType());
			if (eSpecialBuilding != NO_SPECIALBUILDING)
			{
				owner.changeBuildingGroupMaking(eSpecialBuilding, -1);
			}

			if (bFinish)
			{
				if (owner.isBuildingMaxedOut(eConstructBuilding))
				{
					owner.removeBuilding(eConstructBuilding);
				}

				changeHasBuilding(eConstructBuilding, true);

				AI_setPropertyControlBuildingQueued(false);

				const int iProgress = getProgressOnBuilding(eConstructBuilding);
				const int iRawOverflow = iProgress - getProductionNeeded(eConstructBuilding);
				const int iMaxOverflow = getMaxProductionOverflow();
				const int iOverflow = std::min(iMaxOverflow, iRawOverflow);
				if (iOverflow > 0)
				{
					changeOverflowProduction(iOverflow);
				}
				changeProgressOnBuilding(eConstructBuilding, -iProgress);

				m_iLostProductionModified = std::max(0, iRawOverflow - iMaxOverflow);
				m_iGoldFromLostProduction = m_iLostProductionModified * GC.getMAXED_BUILDING_GOLD_PERCENT() / 100;

				// [CIT/produced] -- building completion (the other half of the output stream).
				logCityAI(1, "[CIT/produced] city=%S owner=%d BUILDING %S overflow=%d lost=%d",
					getName().GetCString(), (int)getOwner(), GC.getBuildingInfo(eConstructBuilding).getDescription(),
					iOverflow, m_iLostProductionModified);

				CvEventReporter::getInstance().buildingBuilt(this, eConstructBuilding);
			}

			setBuildingListInvalid();
			break;
		}
		case ORDER_CREATE:
		{
			eCreateProject = order.getProjectType();

			GET_TEAM(getTeam()).changeProjectMaking(eCreateProject, -1);

			if (bFinish)
			{
				OutputDebugString(CvString::format("Project %d (%S) built\n", eCreateProject, GC.getProjectInfo(eCreateProject).getDescription()).c_str());

				// [CIT/produced] -- project completion (wonder/spaceship part/etc.).
				logCityAI(1, "[CIT/produced] city=%S owner=%d PROJECT %S",
					getName().GetCString(), (int)getOwner(), GC.getProjectInfo(eCreateProject).getDescription());

				// Event reported to Python before the project is built, so that we can show the movie before awarding free techs, for example
				CvEventReporter::getInstance().projectBuilt(this, eCreateProject);

				GET_TEAM(getTeam()).changeProjectCount(eCreateProject, 1);

				if (GC.getProjectInfo(eCreateProject).isSpaceship())
				{
					bool needsArtType = true;
					VictoryTypes eVictory = (VictoryTypes)GC.getProjectInfo(eCreateProject).getLaunchesVictory();

					if (NO_VICTORY != eVictory && GET_TEAM(getTeam()).canLaunch(eVictory))
					{
						if (isHuman())
						{
							CvPopupInfo* pInfo = NULL;

							if (GC.getGame().isNetworkMultiPlayer())
							{
								pInfo = new CvPopupInfo(BUTTONPOPUP_LAUNCH, GC.getProjectInfo(eCreateProject).getLaunchesVictory());
							}
							else
							{
								pInfo = new CvPopupInfo(BUTTONPOPUP_PYTHON_SCREEN, eCreateProject);
								pInfo->setText(L"showSpaceShip");
								needsArtType = false;
							}

							gDLL->getInterfaceIFace()->addPopup(pInfo, getOwner());
						}
						else
						{
							owner.AI_launch(eVictory);
						}
					}
					else
					{
						//show the spaceship progress
						if (isHuman())
						{
							if (!GC.getGame().isNetworkMultiPlayer())
							{
								CvPopupInfo* pInfo = new CvPopupInfo(BUTTONPOPUP_PYTHON_SCREEN, eCreateProject);
								pInfo->setText(L"showSpaceShip");
								gDLL->getInterfaceIFace()->addPopup(pInfo, getOwner());
								needsArtType = false;
							}
						}
					}

					if (needsArtType)
					{
						int defaultArtType = GET_TEAM(getTeam()).getProjectDefaultArtType(eCreateProject);
						int projectCount = GET_TEAM(getTeam()).getProjectCount(eCreateProject);
						GET_TEAM(getTeam()).setProjectArtType(eCreateProject, projectCount - 1, defaultArtType);
					}
				}
				const int iRawOverflow = getProjectProduction(eCreateProject) - getProductionNeeded(eCreateProject);
				const int iMaxOverflow = getMaxProductionOverflow();
				const int iOverflow = std::min(iMaxOverflow, iRawOverflow);
				if (iOverflow > 0)
				{
					changeOverflowProduction(iOverflow);
				}
				setProjectProduction(eCreateProject, 0);

				m_iLostProductionModified = std::max(0, iRawOverflow - iMaxOverflow);
				m_iGoldFromLostProduction = m_iLostProductionModified * GC.getMAXED_PROJECT_GOLD_PERCENT() / 100;
			}
			break;
		}
		case ORDER_MAINTAIN:
		case ORDER_LIST:
		{
			break;
		}
		default: FErrorMsg("order.eOrderType is not a valid option");

	}
	const bool bStart = orderIndex == 0;
	if (bStart)
	{
		stopHeadOrder();
	}

	m_orderQueue.erase(m_orderQueue.begin() + orderIndex);
	// #430 enabler queue leg: the dequeue restores the fresh offer -- the one-id re-gate re-reads the queue.
	emitCityOrderRemoved(getID(), (int)getOwner(), (int)order.eOrderType, externalOrder.iData1);

	if (bStart)
	{
		startHeadOrder();
	}

	if (bResolveList)
	{
		// Check if head of queue is a build list and resolve it in that case
		bst::optional<OrderData> nextOrder = getHeadOrder();
		if (nextOrder && nextOrder->eOrderType == ORDER_LIST)
		{
			if (!pushFirstValidBuildListOrder(nextOrder->orderlist.id))
			{
				// pop the list if there is nothing to construct on it any more
				popOrder(0);
			}
			else if (!nextOrder->bSave)
			{
				popOrder(1);
			}
		}
	}

	if (m_orderQueue.empty())
	{
		if (owner.isHumanPlayer(true))
		{
			owner.setIdleCity(getID(), true);
		}

		if (bChoose)
		{
			if (isHuman() && !isProductionAutomated())
			{
				if (bWasFoodProduction)
				{
					AI_assignWorkingPlots();
				}
			}
			else AI_chooseProduction();
		}
	}

	if (bFinish)
	{
		switch (order.eOrderType)
		{
			case ORDER_TRAIN:
			{
				m_outputHistory.addToHistory(ORDER_TRAIN, (uint16_t)order.getUnitType());
				break;
			}
			case ORDER_CONSTRUCT:
			{
				m_outputHistory.addToHistory(ORDER_CONSTRUCT, (uint16_t)order.getBuildingType());
				break;
			}
			case ORDER_CREATE:
			{
				m_outputHistory.addToHistory(ORDER_CREATE, (uint16_t)order.getProjectType());
				break;
			}
			default: FErrorMsg("Can Occur?");
		}
		const char* szIcon = NULL;
		wchar_t szBuffer[1024];
		char szSound[1024];
		bool bCompletionMessage = false;

		if (eTrainUnit != NO_UNIT)
		{
			if (getBugOptionBOOL("Civ4lerts__CompleteUnit", false) || isLimitedUnit(eTrainUnit) && getBugOptionBOOL("Civ4lerts__CompleteSpecial", true))
			{
				swprintf(szBuffer, gDLL->getText("TXT_KEY_MISC_TRAINED_UNIT_IN", GC.getUnitInfo(eTrainUnit).getTextKeyWide(), getNameKey()).GetCString());
				strcpy(szSound, GC.getUnitInfo(eTrainUnit).getArtInfo(0, owner.getCurrentEra(), NO_UNIT_ARTSTYLE)->getTrainSound());
				szIcon = owner.getUnitButton(eTrainUnit);
				bCompletionMessage = true;
			}
		}
		else if (eConstructBuilding != NO_BUILDING)
		{
			if (getBugOptionBOOL("Civ4lerts__CompleteBuilding", false) || isLimitedWonder(eConstructBuilding) && getBugOptionBOOL("Civ4lerts__CompleteSpecial", true))
			{
				swprintf(szBuffer, gDLL->getText("TXT_KEY_MISC_CONSTRUCTED_BUILD_IN", GC.getBuildingInfo(eConstructBuilding).getTextKeyWide(), getNameKey()).GetCString());
				strcpy(szSound, GC.getBuildingInfo(eConstructBuilding).getConstructSound());
				szIcon = GC.getBuildingInfo(eConstructBuilding).getButton();
				bCompletionMessage = true;
			}
		}
		else if (eCreateProject != NO_PROJECT)
		{
			if (getBugOptionBOOL("Civ4lerts__CompleteProject", true) || isLimitedProject(eCreateProject) && getBugOptionBOOL("Civ4lerts__CompleteSpecial", true))
			{
				swprintf(szBuffer, gDLL->getText("TXT_KEY_MISC_CREATED_PROJECT_IN", GC.getProjectInfo(eCreateProject).getTextKeyWide(), getNameKey()).GetCString());
				szIcon = GC.getProjectInfo(eCreateProject).getButton();
				bCompletionMessage = true;
			}
		}
		if (bCompletionMessage)
		{
			if (isProduction() && getBugOptionBOOL("Civ4lerts__CompleteBegunOn", true))
			{
				wchar_t szTempBuffer[1024];
				swprintf(szTempBuffer, gDLL->getText("TXT_KEY_MISC_WORK_HAS_BEGUN", getProductionNameKey()).GetCString());
				wcscat(szBuffer, szTempBuffer);
			}
			AddDLLMessage(getOwner(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, szSound, MESSAGE_TYPE_MINOR_EVENT, szIcon, GC.getCOLOR_WHITE(), getX(), getY(), true, true);
		}
	}

	if ((getTeam() == GC.getGame().getActiveTeam()) || GC.getGame().isDebugMode())
	{
		setInfoDirty(true);
	}
	if (isCitySelected())
	{
		gDLL->getInterfaceIFace()->setDirty(InfoPane_DIRTY_BIT, true);
		gDLL->getInterfaceIFace()->setDirty(SelectionButtons_DIRTY_BIT, true);
	}
}


void CvCity::startHeadOrder()
{
	bst::optional<OrderData> order = getHeadOrder();

	if (order && order->eOrderType == ORDER_MAINTAIN)
	{
		processProcess(((ProcessTypes)(order->iData1)), 1);
		AI_setAssignWorkDirty(true);
	}
}


void CvCity::stopHeadOrder()
{
	bst::optional<OrderData> order = getHeadOrder();

	if (order && order->eOrderType == ORDER_MAINTAIN)
	{
		processProcess(((ProcessTypes)(order->iData1)), -1);
	}
}


int CvCity::getOrderQueueLength() const
{
	return m_orderQueue.size();
}

//CLLNode<OrderData>* CvCity::nextOrderQueueNode(CLLNode<OrderData>* pNode) const
//{
//	return m_orderQueue.next(pNode);
//}
//
//
//CLLNode<OrderData>* CvCity::headOrderQueueNode() const
//{
//	return m_orderQueue.head();
//}
//
//int CvCity::getNumOrdersQueued() const
//{
//	return m_orderQueue.getLength();
//}

OrderData CvCity::getOrderData(int iIndex) const
{
	if (m_orderQueue.size() <= static_cast<size_t>(iIndex))
	{
		return OrderData::InvalidOrder;
	}
	return m_orderQueue[iIndex].to_external();
}

void CvCity::setWallOverridePoints(const std::vector< std::pair<float, float> >& kPoints)
{
	m_kWallOverridePoints = kPoints;
	setLayoutDirty(true);
}

const std::vector< std::pair<float, float> >& CvCity::getWallOverridePoints() const
{
	return m_kWallOverridePoints;
}

// Protected Functions...


void CvCity::doCulture()
{
	PROFILE_FUNC();
	int aiOwnCommerces[NUM_COMMERCE_TYPES];
	getCommerces(aiOwnCommerces);
	changeCultureTimes100(getOwner(), aiOwnCommerces[COMMERCE_CULTURE], false, true);
}


void CvCity::doPlotCulture(PlayerTypes ePlayer, int iCultureRate)
{
	PROFILE_FUNC();
	FASSERT_BOUNDS(0, MAX_PLAYERS, ePlayer);

	int iCultureLevel = GC.getCultureLevelInfo(getCultureLevel()).getLevel();

	clearCultureDistanceCache();

	foreach_(CvPlot* plotX, plot()->rect(iCultureLevel, iCultureLevel))
	{
		const int iCultureDistance = cultureDistance(*plotX);

		if (iCultureDistance <= iCultureLevel && plotX->isPotentialCityWorkForArea(area()))
		{
			// changeCulture includes a check to bump culture value upward
			// to ensure plot cannot be lost thru decay even if culture gain is too small
			plotX->changeCulture(
				ePlayer,
				cultureDistanceDropoff(iCultureRate, iCultureLevel, iCultureDistance),
				// Toffer - Only update plot ownership when the culture of non-owners increase.
				plotX->getOwner() != ePlayer
			);
			plotX->setInCultureRangeOfCityByPlayer(ePlayer);
		}
	}
}


/* This function could probably be improved some.
	Currently is linear dropoff or flat, but nonlinearity might be necessary if culture too strong.
	I want to do something like ((range-distance)/range)^x as a modifier on base gain for
	fractional x around 1 where xml controls x, but... obvious issues with floating point. */
int CvCity::cultureDistanceDropoff(int baseCultureGain, int rangeOfSource, int distanceFromSource)
{
	FASSERT_NOT_NEGATIVE(baseCultureGain);
	FAssertMsg(distanceFromSource <= rangeOfSource, "Calculating culture gain for distance greater than max range.");

	if (baseCultureGain < 1) return 1;

	// Some fraction 0-100 should be distance-modified.
	// At default 75, city flipping may begin at max radius overlap if
	// larger city produces 4x culture of lesser city (25% of larger output equal to 100% of lesser).
	const int iDensityFactor = GC.getCITY_CULTURE_DENSITY_FACTOR();

	// 1->0 multiplier on base rate as distance from source goes 0->max
	const int modifiedCultureGain = (
		baseCultureGain * (rangeOfSource - distanceFromSource) * iDensityFactor
		/
		std::max(100, 100*rangeOfSource)
		+ // The rest is flat base culture rate.
		baseCultureGain * (100 - iDensityFactor) / 100
	);
	return std::max(modifiedCultureGain, 1);
}


bool CvCity::doCheckProduction()
{
	PROFILE_EXTRA_FUNC();
	CvPlayerAI& player = GET_PLAYER(getOwner());
	{
		std::vector< std::pair<UnitTypes, int> > decayUnit;
		for (std::vector< std::pair<UnitTypes, int> >::const_iterator it = m_progressOnUnit.begin(); it != m_progressOnUnit.end(); ++it)
		{
			const UnitTypes eUnitX = (*it).first;

			if (player.isProductionMaxedUnit(eUnitX))
			{
				const int iProgress = (*it).second;
				const int iProductionGold = iProgress * GC.getMAXED_UNIT_GOLD_PERCENT() / 100;

				if (iProductionGold > 0)
				{
					player.changeGold(iProductionGold);
					AddDLLMessage(
						getOwner(), false, GC.getEVENT_MESSAGE_TIME(),
						gDLL->getText(
							"TXT_KEY_MISC_LOST_WONDER_PROD_CONVERTED",
							getNameKey(), GC.getUnitInfo(eUnitX).getTextKeyWide(), iProductionGold
						),
						"AS2D_WONDERGOLD", MESSAGE_TYPE_MINOR_EVENT, GC.getCommerceInfo(COMMERCE_GOLD).getButton(),
						GC.getCOLOR_RED(), getX(), getY(), true, true
					);
				}
				decayUnit.push_back(std::make_pair(eUnitX, -iProgress));
			}
		}
		for (std::vector< std::pair<UnitTypes, int> >::iterator it = decayUnit.begin(); it != decayUnit.end(); ++it)
		{
			changeProgressOnUnit((*it).first, (*it).second);
		}
	}
	{
		std::vector< std::pair<BuildingTypes, int> > decayBuilding;
		for (std::vector< std::pair<BuildingTypes, int> >::iterator it = m_progressOnBuilding.begin(); it != m_progressOnBuilding.end(); ++it)
		{
			const BuildingTypes eTypeX = (*it).first;

			if (player.isProductionMaxedBuilding(eTypeX))
			{
				const int iProgress = (*it).second;
				const int iProductionGold = iProgress * GC.getMAXED_BUILDING_GOLD_PERCENT() / 100;

				if (iProductionGold > 0)
				{
					player.changeGold(iProductionGold);
					AddDLLMessage(
						getOwner(), false, GC.getEVENT_MESSAGE_TIME(),
						gDLL->getText(
							"TXT_KEY_MISC_LOST_WONDER_PROD_CONVERTED",
							getNameKey(), GC.getBuildingInfo(eTypeX).getTextKeyWide(), iProductionGold
						),
						"AS2D_WONDERGOLD", MESSAGE_TYPE_MINOR_EVENT, GC.getCommerceInfo(COMMERCE_GOLD).getButton(),
						GC.getCOLOR_RED(), getX(), getY(), true, true
					);
				}
				decayBuilding.push_back(std::make_pair(eTypeX, -iProgress));
			}
		}
		for (std::vector< std::pair<BuildingTypes, int> >::iterator it = decayBuilding.begin(); it != decayBuilding.end(); ++it)
		{
			changeProgressOnBuilding((*it).first, (*it).second);
		}
	}

	for (int iI = GC.getNumProjectInfos() - 1; iI > -1; iI--)
	{
		const ProjectTypes eTypeX = static_cast<ProjectTypes>(iI);
		if (getProjectProduction(eTypeX) > 0 && player.isProductionMaxedProject(eTypeX))
		{
			const int iProductionGold = ((getProjectProduction(eTypeX) * GC.getMAXED_PROJECT_GOLD_PERCENT()) / 100);

			if (iProductionGold > 0)
			{
				player.changeGold(iProductionGold);
				AddDLLMessage(
					getOwner(), false, GC.getEVENT_MESSAGE_TIME(),
					gDLL->getText(
						"TXT_KEY_MISC_LOST_WONDER_PROD_CONVERTED",
						getNameKey(), GC.getProjectInfo(eTypeX).getTextKeyWide(), iProductionGold
					),
					"AS2D_WONDERGOLD", MESSAGE_TYPE_MINOR_EVENT, GC.getCommerceInfo(COMMERCE_GOLD).getButton(),
					GC.getCOLOR_RED(), getX(), getY(), true, true
				);
			}
			setProjectProduction(eTypeX, 0);
		}
	}

	if (!isProduction() && !isDisorder() && isHuman() && !isProductionAutomated())
	{
		return true;
	}

	for (int iI = GC.getNumUnitInfos() - 1; iI > -1; iI--)
	{
		const UnitTypes unitType = static_cast<UnitTypes>(iI);
		if (getFirstUnitOrder(unitType) != -1)
		{
			const UnitTypes eUpgradeUnit = trainableUpgradeFor(unitType);

			if (eUpgradeUnit != NO_UNIT)
			{
				FAssertMsg(eUpgradeUnit != unitType, "eUpgradeUnit is expected to be different from iI");

				// Move already committed production from old unit type to new unit type
				const int iProgress = getProgressOnUnit(unitType);
				changeProgressOnUnit(unitType, -iProgress);
				changeProgressOnUnit(eUpgradeUnit, iProgress);

				// Change the unit types in the queue
				foreach_(OrderData& order, m_orderQueue | filtered(bind(matchUnitOrder, _1, unitType)))
				{
					player.changeUnitMaking(order.getUnitType(), -1);
					order.setUnitType(eUpgradeUnit);
					if (player.AI_unitValue(eUpgradeUnit, order.getUnitAIType(), area()) == 0)
					{
						area()->changeNumTrainAIUnits(getOwner(), order.getUnitAIType(), -1);
						player.AI_changeNumTrainAIUnits(order.getUnitAIType(), -1);
						order.setUnitAIType(GC.getUnitInfo(eUpgradeUnit).getDefaultUnitAI());
						area()->changeNumTrainAIUnits(getOwner(), order.getUnitAIType(), 1);
						player.AI_changeNumTrainAIUnits(order.getUnitAIType(), 1);
					}
					player.changeUnitMaking(order.getUnitType(), 1);
				}
			}
		}
	}
	bool bOK = true;

	for (int iI = getOrderQueueLength() - 1; iI >= 0; iI--)
	{
		if (!canContinueProduction(getOrderAt(iI)))
		{
			popOrder(iI, false, true);
			bOK = false;
		}
	}
	return bOK;
}


void CvCity::doProduction(bool bAllowNoProduction)
{
	PROFILE_EXTRA_FUNC();
	if (!isHuman() || isProductionAutomated())
	{
		// Koshling - with the unit contracting system we only build units to contractual orders
		//	(apart from a few emergency cases) and we should not change from building them due to new techs, etc.
		if (!isProduction() || isProductionProcess() || AI_isChooseProductionDirty() && !isProductionUnit())
		{
			AI_chooseProduction();
		}
	}

	if (!bAllowNoProduction && !isProduction())
	{
		return;
	}

	if (isProductionProcess())
	{
		if (m_bPopProductionProcess)
		{
			popOrder(0, false, true);
			m_bPopProductionProcess = false;
		}
		return;
	}

	if (isDisorder())
	{
		return;
	}

	if (isProduction())
	{
		changeProduction(getCurrentProductionDifference(ProductionCalc::FoodProduction | ProductionCalc::Overflow));
		setOverflowProduction(0);
		setFeatureProduction(0);

		setBuiltFoodProducedUnit(isFoodProduction());
		clearLostProduction();

		int iCompletionSafety = 0;
		while (productionLeft() <= 0)
		{
			//	Stale-tolerance bound: stale value/canConstruct data can make the AI re-choose a
			//	building it just completed (or a zero-cost item), which the overflow instantly
			//	re-completes -- popOrder(bChoose) then re-picks it and this loop cycles forever
			//	(the never-ending-turn hang the building-value retention experiment hit). Cap the
			//	completions per city-turn; a hit means stale advisory data, not a logic error.
			if (++iCompletionSafety > 50)
			{
				logCityAI(1, "[CIT/spin] city=%S owner=%d reason=produceLoopCap",
					getName().GetCString(), (int)getOwner());
				break;
			}
			popOrder(0, true, true);

			if (!isProduction())
			{
				if (isHuman())
				{
					break;
				}
				AI_chooseProduction();

				if (!isProduction())
				{
					//	The AI failed to establish ANY production (every candidate failed its live
					//	re-check). Defensive: idle this turn, flag a re-decide for next turn.
					AI_setChooseProductionDirty(true);
					logCityAI(1, "[CIT/spin] city=%S owner=%d reason=noProductionChosen",
						getName().GetCString(), (int)getOwner());
					break;
				}
			}

			/* Toffer - Don't think the wonder limit can be breached here just like that.
			// Prevents breaching the wonder limit.
			// Eliminates pre-build exploits for all Wonders and all Projects
			if (isProductionWonder() || isProductionProject())
			{
				break;
			}
			*/

			// Eliminate pre-build exploits for Settlers and Workers
			if (isFoodProduction() && !isBuiltFoodProducedUnit())
			{
				break;
			}
			changeProduction(getOverflowProduction());
			setOverflowProduction(0);
		}

		if (m_iGoldFromLostProduction > 0)
		{
			// [CIT/waste] -- production overflowed past the cap and was burned to gold instead of
			// hammers: a wasted-production outlier (e.g. a city finishing a cheap unit with a huge
			// overflow, or mis-sequenced builds). Pairs with [CIT/produced] lost=.
			logCityAI(1, "[CIT/waste] city=%S owner=%d lostProd=%d -> gold=%d",
				getName().GetCString(), (int)getOwner(), m_iLostProductionModified, m_iGoldFromLostProduction);

			CvWString szBuffer = gDLL->getText("TXT_KEY_MISC_LOST_PROD_CONVERTED", getNameKey(), m_iLostProductionModified, m_iGoldFromLostProduction);
			AddDLLMessage(getOwner(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_WONDERGOLD", MESSAGE_TYPE_MINOR_EVENT, GC.getCommerceInfo(COMMERCE_GOLD).getButton(), GC.getCOLOR_RED(), getX(), getY(), true, true);

			GET_PLAYER(getOwner()).changeGold(m_iGoldFromLostProduction);
			clearLostProduction();
		}
	}
	else
	{
		changeOverflowProduction(getCurrentProductionDifference(ProductionCalc::FoodProduction));
	}
}


void CvCity::doDecay()
{
	PROFILE_EXTRA_FUNC();
	if (!isHuman())
	{
		return;
	}
	{
		std::vector< std::pair<BuildingTypes, int> > decayBuilding;
		for (std::vector< std::pair<BuildingTypes, int> >::iterator it = m_progressOnBuilding.begin(); it != m_progressOnBuilding.end(); ++it)
		{
			const BuildingTypes eTypeX = (*it).first;

			if (getProductionBuilding() != eTypeX)
			{
				tickDelayOnBuilding(eTypeX);

				const int iGameSpeedPercent = CvGameSpeedScale::speedPercent();
				if (100 * getDelayOnBuilding(eTypeX) > GC.getBUILDING_PRODUCTION_DECAY_TIME()* iGameSpeedPercent)
				{
					decayBuilding.push_back(
						std::make_pair(
							eTypeX,
							-((*it).second * (100 - GC.getBUILDING_PRODUCTION_DECAY_PERCENT()) + iGameSpeedPercent - 1) / iGameSpeedPercent
						)
					);
				}
			}
			else if (getDelayOnBuilding(eTypeX) > 0)
			{
				tickDelayOnBuilding(eTypeX, false);
			}
		}
		for (std::vector< std::pair<BuildingTypes, int> >::iterator it = decayBuilding.begin(); it != decayBuilding.end(); ++it)
		{
			changeProgressOnBuilding((*it).first, (*it).second);
		}
	}
	{
		std::vector< std::pair<UnitTypes, int> > decayUnit;
		for (std::vector< std::pair<UnitTypes, int> >::const_iterator it = m_progressOnUnit.begin(); it != m_progressOnUnit.end(); ++it)
		{
			const UnitTypes eUnitX = (*it).first;

			if (getProductionUnit() != eUnitX)
			{
				tickDelayOnUnit(eUnitX);

				const int iGameSpeedPercent = CvGameSpeedScale::speedPercent();
				if (100 * getDelayOnUnit(eUnitX) > GC.getUNIT_PRODUCTION_DECAY_TIME() * iGameSpeedPercent)
				{
					decayUnit.push_back(
						std::make_pair(
							eUnitX,
							-((*it).second * (100 - GC.getUNIT_PRODUCTION_DECAY_PERCENT()) + iGameSpeedPercent - 1) / iGameSpeedPercent
						)
					);
				}
			}
			else if (getDelayOnUnit(eUnitX) > 0)
			{
				tickDelayOnUnit(eUnitX, false);
			}
		}
		for (std::vector< std::pair<UnitTypes, int> >::iterator it = decayUnit.begin(); it != decayUnit.end(); ++it)
		{
			changeProgressOnUnit((*it).first, (*it).second);
		}
	}
}


void CvCity::doReligion()
{
	PROFILE_EXTRA_FUNC();
	CvGame& GAME = GC.getGame();
	const bool bReligionDecay = GAME.isModderGameOption(MODDERGAMEOPTION_RELIGION_DECAY);
	const bool bMultRelSpread = GAME.isModderGameOption(MODDERGAMEOPTION_MULTIPLE_RELIGION_SPREAD);

	const ReligionTypes eStateReligion = GET_PLAYER(getOwner()).getStateReligion();
	const int iReligionCount = getReligionCount();

	const int iNumReligions = GC.getNumReligionInfos();
	int iReligionX = GAME.getSorenRandNum(iNumReligions, "Random start index");
	int iCount = 0;
	while (iCount++ < iNumReligions)
	{
		const ReligionTypes eReligionX = static_cast<ReligionTypes>(iReligionX);

		if (GAME.isReligionFounded(eReligionX))
		{
			if (isHasReligion(eReligionX))
			{
				if (bReligionDecay)
				{
					const CvCity* pHolyCity = GAME.getHolyCity(eReligionX);

					if (eReligionX != eStateReligion && pHolyCity != this && iReligionCount > 1)
					{
						const int iExp = iReligionCount - 2;
						int iDecay = GC.getReligionInfo(eReligionX).getSpreadFactor() + iExp * iExp;

						foreach_(const CvCity* cityX, GET_PLAYER(getOwner()).cities())
						{
							if (cityX->isConnectedTo(this) && cityX->isHasReligion(eReligionX))
							{
								iDecay *= 9;
								iDecay /= 10 + cityX->getReligionInfluence(eReligionX);
							}
						}

						iDecay /= 1 + getReligionInfluence(eReligionX);
						if (pHolyCity != NULL)
						{
							if (pHolyCity->getOwner() == getOwner())
							{
								iDecay /= 2;
							}
							else if (GET_TEAM(getTeam()).isAtWar(pHolyCity->getTeam()))
							{
								iDecay *= 4;
								iDecay /= 3;
							}
						}
						if (iDecay > 0)
						{
							const int iSpreadRand =
							(
								GC.getRELIGION_SPREAD_RAND()
								*
								CvGameSpeedScale::speedPercent()
								/
								100
							);
							if (GAME.getSorenRandNum(iSpreadRand, "Religion Decay") < iDecay)
							{
								setHasReligion(eReligionX, false, true, false);

								// ⚠ The buildings that needed this religion are NOT torn down: PrereqReligion is
								// requires.OPERATE, so losing it makes them DORMANT and they wake if the religion
								// returns (enabler.md §3). A behaviour change from legacy, stated not hidden.
								break;
							}
						}
					}
				}
			}
			else if (bMultRelSpread || iReligionCount == 0)
			{
				if (eReligionX == eStateReligion || !GET_PLAYER(getOwner()).isNoNonStateReligionSpread())
				{
					const int iSpreadFactor = std::max(1, GC.getReligionInfo(eReligionX).getSpreadFactor());
					int iRandThreshold = 0;

					for (int iJ = 0; iJ < MAX_PLAYERS; iJ++)
					{
						if (GET_PLAYER((PlayerTypes)iJ).isAlive())
						{
							foreach_(const CvCity* cityX, GET_PLAYER((PlayerTypes)iJ).cities())
							{
								if (cityX == this || cityX->isConnectedTo(this))
								{
									int iSpread = cityX->getReligionInfluence(eReligionX);
									if (iSpread > 0)
									{
										iSpread *= iSpreadFactor;
										if (cityX == this)
										{
											iSpread = 2 * iSpread / (iReligionCount + 1);
										}
										else
										{
											iSpread /=
											(
												(iReligionCount + 1)
												*
												std::max(
													1,
													GC.getRELIGION_SPREAD_DISTANCE_DIVISOR()
													* plotDistance(getX(), getY(), cityX->getX(), cityX->getY())
													/
													GC.getMap().maxPlotDistance() - 5
												)
											);
										}
										iRandThreshold = std::max(iRandThreshold, iSpread);
									}
								}
							}
						}
					}
					if (iRandThreshold > 0)
					{
						iRandThreshold *= std::max(1, getModifiedIntValue(100, GET_PLAYER(getOwner()).getReligionSpreadRate()));
						iRandThreshold /= 100;

						const int iSpreadRand =
						(
							GC.getRELIGION_SPREAD_RAND()
							*
							CvGameSpeedScale::speedPercent()
							/
							100
						);

						if (GAME.getSorenRandNum(iSpreadRand, "Religion Spread") < iRandThreshold)
						{
							setHasReligion((eReligionX), true, true, true);
							break;
						}
					}
				}
			}
		}
		if (++iReligionX == iNumReligions)
		{
			iReligionX = 0;
		}
	}
}


void CvCity::doGreatPeople()
{
	PROFILE_EXTRA_FUNC();
	if (isDisorder())
	{
		return;
	}
	// THE WAREHOUSE EDGE -- the ×100 per-turn RATE is banked into a HUMAN ledger, so the reduce is here and the
	// serialized progress (and its threshold) keep their meaning ([north-star.md] warehouse carve-out).
	changeGreatPeopleProgress(getGreatPeopleRate() / 100);

	for (int iI = 0; iI < GC.getNumUnitInfos(); iI++)
	{
		changeGreatPeopleUnitProgress(((UnitTypes)iI), getGreatPeopleUnitRate((UnitTypes)iI));
	}

	if (getGreatPeopleProgress() >= GET_PLAYER(getOwner()).greatPeopleThresholdNonMilitary())
	{
		int iTotalGreatPeopleUnitProgress = 0;
		for (int iI = 0; iI < GC.getNumUnitInfos(); iI++)
		{
			iTotalGreatPeopleUnitProgress += getGreatPeopleUnitProgress((UnitTypes)iI);
		}
		int iGreatPeopleUnitRand = GC.getGame().getSorenRandNum(iTotalGreatPeopleUnitProgress, "Great Person");

		UnitTypes eGreatPeopleUnit = NO_UNIT;
		for (int iI = 0; iI < GC.getNumUnitInfos(); iI++)
		{
			if (iGreatPeopleUnitRand < getGreatPeopleUnitProgress((UnitTypes)iI))
			{
				eGreatPeopleUnit = ((UnitTypes)iI);
				break;
			}
			else
			{
				iGreatPeopleUnitRand -= getGreatPeopleUnitProgress((UnitTypes)iI);
			}
		}

		if (eGreatPeopleUnit != NO_UNIT)
		{
			changeGreatPeopleProgress(-(GET_PLAYER(getOwner()).greatPeopleThresholdNonMilitary()));

			for (int iI = 0; iI < GC.getNumUnitInfos(); iI++)
			{
				setGreatPeopleUnitProgress(((UnitTypes)iI), 0);
			}
			createGreatPeople(eGreatPeopleUnit, true, false);
		}
	}
}


// Private Functions...

// The TWO-PHASE stream read (FFreeListTrashArray.h ReadStreamableFFreeListTrashArrayTwoPhase). Phase 1
// deserializes ONLY the id so the loader can REGISTER the city in its owner's list before phase 2 streams the
// rest -- which is what lets the DOMAIN events the body emits from inside its own read resolve through the
// ordinary id lookup ([DEC-spine-reseed]). Same bytes in the same order as a single-phase read; only the
// moment the object becomes resolvable differs.
void CvCity::readIdentity(FDataStreamBase* pStream)
{
	CvTaggedSaveFormatWrapper& wrapper = CvTaggedSaveFormatWrapper::getSaveFormatWrapper();

	wrapper.AttachToStream(pStream);

	WRAPPER_READ_OBJECT_START(wrapper);

	// Init data before load
	reset();

	WRAPPER_READ(wrapper, "CvCity", &m_iID);
}

void CvCity::readBody(FDataStreamBase* pStream)
{
	PROFILE_EXTRA_FUNC();
	CvTaggedSaveFormatWrapper& wrapper = CvTaggedSaveFormatWrapper::getSaveFormatWrapper();

	wrapper.AttachToStream(pStream);

	WRAPPER_READ(wrapper, "CvCity", &m_iX);
	WRAPPER_READ(wrapper, "CvCity", &m_iY);
	WRAPPER_READ(wrapper, "CvCity", &m_iRallyX);
	WRAPPER_READ(wrapper, "CvCity", &m_iRallyY);
	WRAPPER_READ(wrapper, "CvCity", &m_iGameTurnFounded);
	WRAPPER_READ(wrapper, "CvCity", &m_iGameTurnAcquired);
	// ⚠ Deserializes HERE, where the stream puts it, but LANDS through its internal setter further down -- every
	// city fact names m_eOwner, which is read later, and the enabler domains must be sized first. The local
	// carries the value across that gap; it keeps the member's own type so the wrapper picks the same overload.
	int iLoadedPopulation = 0;
	WRAPPER_READ_DECORATED(wrapper, "CvCity", &iLoadedPopulation, "m_iPopulation");
	WRAPPER_READ(wrapper, "CvCity", &m_iHighestPopulation);
	WRAPPER_READ(wrapper, "CvCity", &m_iWorkingPopulation);
	WRAPPER_READ(wrapper, "CvCity", &m_iSpecialistPopulation);
	WRAPPER_READ(wrapper, "CvCity", &m_iNumGreatPeople);
	WRAPPER_READ(wrapper, "CvCity", &m_iGreatPeopleProgress);
	WRAPPER_READ(wrapper, "CvCity", &m_iNumWorldWonders);
	WRAPPER_READ(wrapper, "CvCity", &m_iNumTeamWonders);
	WRAPPER_READ(wrapper, "CvCity", &m_iNumNationalWonders);
	WRAPPER_READ(wrapper, "CvCity", &m_iNumBuildings);
	WRAPPER_READ(wrapper, "CvCity", &m_iEspionageHealthCounter);
	WRAPPER_READ(wrapper, "CvCity", &m_iEspionageHappinessCounter);
	WRAPPER_READ(wrapper, "CvCity", &m_iFreshWaterGoodHealth);


	WRAPPER_READ(wrapper, "CvCity", &m_iHurryAngerTimer);
	WRAPPER_READ(wrapper, "CvCity", &m_iRevRequestAngerTimer);
	WRAPPER_READ(wrapper, "CvCity", &m_iRevSuccessTimer);
	WRAPPER_READ(wrapper, "CvCity", &m_iConscriptAngerTimer);
	WRAPPER_READ(wrapper, "CvCity", &m_iDefyResolutionAngerTimer);
	WRAPPER_READ(wrapper, "CvCity", &m_iHappinessTimer);
	WRAPPER_READ(wrapper, "CvCity", &m_iMilitaryHappinessUnits);
	WRAPPER_READ(wrapper, "CvCity", &m_iExtraHappiness);
	WRAPPER_READ(wrapper, "CvCity", &m_iExtraHealth);
	WRAPPER_READ(wrapper, "CvCity", &m_iFood);
	WRAPPER_READ(wrapper, "CvCity", &m_iFoodKept);
	WRAPPER_READ(wrapper, "CvCity", &m_iOverflowProduction);
	WRAPPER_READ(wrapper, "CvCity", &m_iFeatureProduction);
	WRAPPER_READ(wrapper, "CvCity", &m_iCurrAirlift);
	WRAPPER_READ(wrapper, "CvCity", &m_iMaxAirlift);
	WRAPPER_READ(wrapper, "CvCity", &m_iAirUnitCapacity);

	WRAPPER_READ(wrapper, "CvCity", &m_iDefenseDamage);
	WRAPPER_READ(wrapper, "CvCity", &m_iLastDefenseDamage);
	WRAPPER_READ(wrapper, "CvCity", &m_iOccupationTimer);
	WRAPPER_READ(wrapper, "CvCity", &m_iCultureUpdateTimer);
	WRAPPER_READ(wrapper, "CvCity", &m_iCitySizeBoost);

	WRAPPER_READ(wrapper, "CvCity", &m_bNeverLost);
	WRAPPER_READ(wrapper, "CvCity", &m_bBombarded);
	WRAPPER_READ(wrapper, "CvCity", &m_bDrafted);
	WRAPPER_READ(wrapper, "CvCity", &m_bAirliftTargeted);
	// The statuses deserialize WHOLESALE here and LAND through setStatus below, once m_eOwner is off the stream
	// -- a status fact names the owner like every other city fact. Written straight into the array they would
	// announce nothing, and every consumer gating on one would read a city that is not held.
	WRAPPER_READ_ARRAY(wrapper, "CvCity", NUM_CITY_STATUSES, m_aiStatusTurns);
	EVENT_GRANTS_READ(wrapper, "CvCity", m_eventGrants);
	WRAPPER_READ(wrapper, "CvCity", &m_bCitizensAutomated);
	WRAPPER_READ(wrapper, "CvCity", &m_bProductionAutomated);
	WRAPPER_READ(wrapper, "CvCity", &m_bWallOverride);
	// m_bInfoDirty not saved...
	// m_bLayoutDirty not saved...
	WRAPPER_READ(wrapper, "CvCity", &m_bPlundered);

	WRAPPER_READ(wrapper, "CvCity", (int*)&m_eOwner);
	WRAPPER_READ(wrapper, "CvCity", (int*)&m_ePreviousOwner);
	WRAPPER_READ(wrapper, "CvCity", (int*)&m_eOriginalOwner);
	// Deserializes here, lands through its internal setter below -- the enabler domains are not sized yet
	// (see m_iPopulation above). The clamp applies to the LOADED value, before anything is announced.
	int iLoadedCultureLevel = NO_CULTURELEVEL;
	WRAPPER_READ_DECORATED(wrapper, "CvCity", &iLoadedCultureLevel, "m_eCultureLevel");
	if (iLoadedCultureLevel >= GC.getNumCultureLevelInfos())
	{
		iLoadedCultureLevel = GC.getNumCultureLevelInfos() - 1;
	}

	WRAPPER_READ(wrapper, "CvCity", &m_iRevolutionIndex);
	WRAPPER_READ(wrapper, "CvCity", &m_iLocalRevIndex);
	WRAPPER_READ(wrapper, "CvCity", &m_iRevIndexAverage);
	WRAPPER_READ(wrapper, "CvCity", &m_iRevolutionCounter);
	WRAPPER_READ(wrapper, "CvCity", &m_iReinforcementCounter);

	WRAPPER_READ_ARRAY(wrapper, "CvCity", NUM_YIELD_TYPES, m_aiYieldRateModifier);
	WRAPPER_READ_ARRAY(wrapper, "CvCity", NUM_COMMERCE_TYPES, m_aiProductionToCommerceModifier);
	// Widening a member is SOFT: the reader absorbs the narrower stored form (save.md §8), so this keeps
	// its own tag and an old save's 32-bit culture is read and widened in place.
	WRAPPER_READ_ARRAY(wrapper, "CvCity", MAX_PLAYERS, m_aiCulture);
	WRAPPER_READ_ARRAY(wrapper, "CvCity", MAX_PLAYERS, m_aiNumRevolts);

	WRAPPER_READ_ARRAY(wrapper, "CvCity", MAX_PLAYERS, m_abEverOwned);
	WRAPPER_READ_ARRAY(wrapper, "CvCity", MAX_PLAYERS, m_abTradeRoute);
	WRAPPER_READ_ARRAY(wrapper, "CvCity", MAX_TEAMS, m_abRevealed);
	WRAPPER_READ_ARRAY(wrapper, "CvCity", MAX_TEAMS, m_abEspionageVisibility);

	WRAPPER_READ_STRING(wrapper, "CvCity", m_szName);
	WRAPPER_READ_STRING(wrapper, "CvCity", m_szScriptData);

	WRAPPER_READ_CLASS_ARRAY_ALLOW_MISSING(wrapper, "CvCity", REMAPPED_CLASS_TYPE_PROJECTS, GC.getNumProjectInfos(), m_paiProjectProduction);
	WRAPPER_READ_CLASS_ARRAY_ALLOW_MISSING(wrapper, "CvCity", REMAPPED_CLASS_TYPE_UNITS, GC.getNumUnitInfos(), m_paiUnitProduction);
	WRAPPER_READ_CLASS_ARRAY_ALLOW_MISSING(wrapper, "CvCity", REMAPPED_CLASS_TYPE_UNITS, GC.getNumUnitInfos(), m_paiGreatPeopleUnitRate);
	WRAPPER_READ_CLASS_ARRAY_ALLOW_MISSING(wrapper, "CvCity", REMAPPED_CLASS_TYPE_UNITS, GC.getNumUnitInfos(), m_paiGreatPeopleUnitProgress);
	WRAPPER_READ_CLASS_ARRAY_ALLOW_MISSING(wrapper, "CvCity", REMAPPED_CLASS_TYPE_SPECIALISTS, GC.getNumSpecialistInfos(), m_paiSpecialistCount);
	WRAPPER_READ_CLASS_ARRAY_ALLOW_MISSING(wrapper, "CvCity", REMAPPED_CLASS_TYPE_SPECIALISTS, GC.getNumSpecialistInfos(), m_paiForceSpecialistCount);
	WRAPPER_READ_CLASS_ARRAY_ALLOW_MISSING(wrapper, "CvCity", REMAPPED_CLASS_TYPE_SPECIALISTS, GC.getNumSpecialistInfos(), m_paiFreeSpecialistCountUnattributed);
	WRAPPER_READ_CLASS_ARRAY_ALLOW_MISSING(wrapper, "CvCity", REMAPPED_CLASS_TYPE_RELIGIONS, GC.getNumReligionInfos(), m_paiReligionInfluence);
	WRAPPER_READ_ARRAY(wrapper, "CvCity", NUM_CITY_PLOTS, m_pabWorkingPlot);
	WRAPPER_READ_CLASS_ARRAY_ALLOW_MISSING(wrapper, "CvCity", REMAPPED_CLASS_TYPE_RELIGIONS, GC.getNumReligionInfos(), m_pabHasReligion);
	WRAPPER_READ_CLASS_ARRAY_ALLOW_MISSING(wrapper, "CvCity", REMAPPED_CLASS_TYPE_CORPORATIONS, GC.getNumCorporationInfos(), m_pabHasCorporation);
	// #430 reseed (event-spine.md the load-RESEED): the city's slots LAND here, through their internal setters.
	// The arrays are read WHOLESALE (WRAPPER_READ_CLASS_ARRAY has no per-element hook), so the landing is a loop
	// right after their deserialization -- the in-read reseed, NOT a separate post-load walk over all cities
	// (that pseudo-emit is banned, superseded-ideas #13). The scalars deserialized earlier in read() and were
	// carried here in locals, because nothing may announce before m_eOwner lands and the domains are sized.
	{
		const int iCityId = m_iID;
		const int iCityOwner = (int)m_eOwner;
		int iI;
		// THE ENABLER'S PER-CITY LIFECYCLE START, ahead of the facts (enabler.md §7.1) -- the load twin of the
		// same call in init(). A loaded city never runs init(), so without this its domains are never sized and
		// every reseed emit below lands on an un-init'd domain the appliers SKIP, leaving the city's frontier
		// permanently empty. It also folds the cross-scope HAVEs (team techs, player civics) that no per-city
		// event carries.
		BuildingEnabler::onCityCreated(*this);
		UnitEnabler::onCityCreated(*this);
		// #430 reseed: the city's OWNERSHIP first (null -> current, owner ruling) -- establishes the city belongs to
		// its owner before its contents (population/buildings/religion/...) reseed.
		// ══════════ THE CITY'S SLOTS, THROUGH THE INTERNAL SETTERS ══════════
		// This is the first point at which a city fact can land: m_iID and m_eOwner have deserialized, and the
		// enabler domains are sized. Each slot is handed to the ONE body that commits and announces it, so the
		// read no longer knows how to announce a city and cannot fall out of step with the setters.
		// ⚠ The ARRAY reads are WHOLESALE -- the wrapper fills the member directly -- so a slot that already
		// carries its loaded value is reset to unset and handed straight back to its setter. That keeps the
		// setter the sole author of both the committed value and the fact, which a "commit here, announce
		// there" split cannot: the two drift, and that is exactly what this pass is removing.
		emitCityOwnerAdded(iCityId, iCityOwner);
		setPopulationInternal(iLoadedPopulation);
		for (iI = 0; iI < GC.getNumReligionInfos(); ++iI)
		{
			if (m_pabHasReligion[iI])
			{
				m_pabHasReligion[iI] = false;
				setHasReligionInternal((ReligionTypes)iI, true);
				// The holy city is CvGame's designation, not a CvCity slot, so it has no setter here to land
				// through -- it is announced per-city from the game's own array. The IDInfo test is read-safe
				// at this point, where getHolyCity() cannot yet resolve this city back to itself.
				if (GC.getGame().isHolyCityByOwnerId((ReligionTypes)iI, (PlayerTypes)m_eOwner, iCityId))
				{
					emitCityHolyCityAdded(iCityId, iCityOwner, iI);
				}
			}
		}
		for (iI = 0; iI < GC.getNumCorporationInfos(); ++iI)
		{
			if (m_pabHasCorporation[iI])
			{
				m_pabHasCorporation[iI] = false;
				setHasCorporationInternal((CorporationTypes)iI, true);
				// The headquarters designation is CvGame's too (the holy-city case above): its m_paHeadquarters
				// deserializes before the cities, so setHeadquarters never runs for a loaded city.
				if (GC.getGame().isHeadquartersByOwnerId((CorporationTypes)iI, (PlayerTypes)m_eOwner, iCityId))
				{
					emitCityHeadquartersAdded(iCityId, iCityOwner, iI);
				}
			}
		}
		// (no bonus reseed loop: the counts are no longer read from the save -- the load-end network fold
		// announces every bonus fact through the genuine processBonus crossing emits, one mechanism)
		for (iI = 0; iI < GC.getNumSpecialistInfos(); ++iI)
		{
			if (m_paiSpecialistCount[iI] > 0)
			{
				const int iLoadedSpecialistCount = m_paiSpecialistCount[iI];
				m_paiSpecialistCount[iI] = 0;
				setSpecialistCountInternal((SpecialistTypes)iI, iLoadedSpecialistCount);
			}
		}
		// The WORKED set, landed the same way as the wholesale arrays above. Without this no plot's IS_WORKED
		// verdict is announced by a load at all: the array deserializes whole, setWorkingPlot never runs, and
		// the bit is re-derived by its own fact and by nothing else ([DEC-contexts-are-never-marked]) -- so it
		// read FALSE for the entire session while isWorkingPlot said true.
		// ⚑ The cities stream AFTER the map, so the plot exists to carry the bit; and the fact lands before the
		// GAME_LOAD_FINISHED drain folds each plot's FINAL block into its city's plotAttrs, which is what keeps
		// the count exact rather than short by every worked tile.
		for (iI = 0; iI < NUM_CITY_PLOTS; ++iI)
		{
			if (m_pabWorkingPlot[iI])
			{
				m_pabWorkingPlot[iI] = false;
				setWorkingPlotInternal(iI, true);
			}
		}
		// Culture level carries the city's radius/vicinity footprint with it.
		setCultureLevelInternal((CultureLevelTypes)iLoadedCultureLevel);
		// The held STATUSES, landed the same way as the wholesale arrays above: take the deserialized turns,
		// zero the slot, hand it back to setStatus so the HOLDS-crossing announces from its one write path.
		for (iI = 0; iI < NUM_CITY_STATUSES; ++iI)
		{
			if (m_aiStatusTurns[iI] > 0)
			{
				const int iLoadedStatusTurns = m_aiStatusTurns[iI];
				m_aiStatusTurns[iI] = 0;
				setStatus((CityStatus)iI, iLoadedStatusTurns);
			}
		}
		// (No in-read GOVERNMENT-CENTRE emit: the verdict is no longer a deserialized counter but the city's amenity
		// FOLD, which builds at load from the enabler's operating-building seed. The contexts' consumer watches the
		// verdict across that fold and announces the same crossing, so the fact still fires exactly once -- it has
		// simply moved to where the verdict is now decided.)
	}

	WRAPPER_READ(wrapper, "CvCity", &m_iEventAnger);



	WRAPPER_READ_CLASS_ENUM_ALLOW_MISSING(wrapper, "CvCity", REMAPPED_CLASS_TYPE_CIVILIZATIONS, &m_iCiv);

	WRAPPER_READ(wrapper, "CvCity", &m_iLandmarkAngerTimer);

	WRAPPER_READ(wrapper, "CvCity", &m_iWorkableRadiusOverride);
	WRAPPER_READ(wrapper, "CvCity", &m_iProtectedCultureCount);
	WRAPPER_READ(wrapper, "CvCity", &m_iWarWearinessTimer);

	WRAPPER_READ_CLASS_ARRAY_ALLOW_MISSING(wrapper, "CvCity", REMAPPED_CLASS_TYPE_COMBATINFOS, GC.getNumUnitCombatInfos(), m_paiUnitCombatExtraStrength);
	WRAPPER_READ_CLASS_ARRAY_ALLOW_MISSING(wrapper, "CvCity", REMAPPED_CLASS_TYPE_BUILDS, GC.getNumBuildInfos(), m_pabAutomatedCanBuild);

	WRAPPER_READ(wrapper, "CvCity", &m_iMinimumDefenseLevel);
	WRAPPER_READ(wrapper, "CvCity", &m_iHealthPercentPerPopulation);

	// Read all saved trade routes
	int iNumTradeRoutes = 0;
	WRAPPER_READ(wrapper, "CvCity", &iNumTradeRoutes);
	m_paTradeCities = std::vector<IDInfo>(iNumTradeRoutes);
	for (int iI = 0; iI < iNumTradeRoutes; ++iI)
	{
		WRAPPER_READ(wrapper, "CvCity", (int*)&m_paTradeCities[iI].eOwner);
		WRAPPER_READ(wrapper, "CvCity", &m_paTradeCities[iI].iID);
	}
	// Discard saved trade routes above the max count we allow
	m_paTradeCities.resize(getMaxTradeRoutes());

	int orderQueueSize = 0;
	WRAPPER_READ(wrapper, "CvCity", &orderQueueSize);
	m_orderQueue.clear();
	for (int orderQueueIndex = 0; orderQueueIndex < orderQueueSize; ++orderQueueIndex)
	{
		OrderData orderData;
		WRAPPER_READ_ARRAY_DECORATED(wrapper, "CvCity", sizeof(OrderData), (uint8_t*)&orderData, "OrderData");
		bool bDeleteNode = false;
		switch (orderData.eOrderType)
		{
		case ORDER_TRAIN: {
			orderData.setUnitType(static_cast<UnitTypes>(wrapper.getNewClassEnumValue(REMAPPED_CLASS_TYPE_UNITS, orderData.getUnitType(), true)));
			bDeleteNode = (orderData.getUnitType() == NO_UNIT);
			if (!bDeleteNode && orderData.getUnitAIType() == static_cast<UnitAITypes>(0xFF))
			{
				orderData.unit.AIType = GC.getUnitInfo(orderData.getUnitType()).getDefaultUnitAI();
			}
			break;
		};
		case ORDER_CONSTRUCT: {
			orderData.setBuildingType(static_cast<BuildingTypes>(wrapper.getNewClassEnumValue(REMAPPED_CLASS_TYPE_BUILDINGS, orderData.getBuildingType(), true)));
			bDeleteNode = (orderData.getBuildingType() == NO_BUILDING);
			break;
		};
		case ORDER_CREATE: {
			orderData.setProjectType(static_cast<ProjectTypes>(wrapper.getNewClassEnumValue(REMAPPED_CLASS_TYPE_PROJECTS, orderData.getProjectType(), true)));
			bDeleteNode = (orderData.getProjectType() == NO_PROJECT);
			break;
		};
		case ORDER_MAINTAIN:
		case ORDER_LIST:
		default:
			break;
		};
		if (!bDeleteNode)
		{
			m_orderQueue.push_back(orderData);
		}
	}

	WRAPPER_READ(wrapper, "CvCity", &m_iPopulationRank);
	WRAPPER_READ(wrapper, "CvCity", &m_bPopulationRankValid);
	WRAPPER_READ_ARRAY(wrapper, "CvCity", NUM_YIELD_TYPES, m_aiBaseYieldRank);
	WRAPPER_READ_ARRAY(wrapper, "CvCity", NUM_YIELD_TYPES, m_abBaseYieldRankValid);
	WRAPPER_READ_ARRAY(wrapper, "CvCity", NUM_YIELD_TYPES, m_aiYieldRank);
	WRAPPER_READ_ARRAY(wrapper, "CvCity", NUM_YIELD_TYPES, m_abYieldRankValid);
	WRAPPER_READ_ARRAY(wrapper, "CvCity", NUM_COMMERCE_TYPES, m_aiCommerceRank);
	WRAPPER_READ_ARRAY(wrapper, "CvCity", NUM_COMMERCE_TYPES, m_abCommerceRankValid);

	m_Properties.readWrapper(pStream);

	unsigned int iNumElts = 0;
	WRAPPER_READ(wrapper, "CvCity", &iNumElts);
	m_aEventsOccured.clear();
	for (unsigned int i = 0; i < iNumElts; ++i)
	{
		EventTypes eEvent = NO_EVENT;
		WRAPPER_READ_CLASS_ENUM_ALLOW_MISSING(wrapper, "CvCity", REMAPPED_CLASS_TYPE_EVENTS, (int*)&eEvent);

		if (eEvent != NO_EVENT)
		{
			m_aEventsOccured.push_back(eEvent);
		}
	}

	// ⛔ PRE-ZERO BEFORE EVERY ONE OF THESE READS. Six drain blocks share the single `CvCity::iNumElts` tag, and a
	// tag the stream does not carry leaves its variable UNTOUCHED (save.md §2) -- so without this the block
	// inherits the PREVIOUS block's count and drains elements that were never written. The two blocks below then
	// read RAW off pStream, so a stale count walks the raw stream off its position and the garbage it returns is
	// what finally throws. (The last block already guarded itself; the four here did not.)
	iNumElts = 0;
	WRAPPER_READ(wrapper, "CvCity", &iNumElts);
	m_aBuildingYieldChange.clear();
	for (unsigned int i = 0; i < iNumElts; ++i)
	{
		BuildingYieldChange kChange;
		kChange.read(pStream);

		if (kChange.eBuilding != NO_BUILDING)
		{
			m_aBuildingYieldChange.push_back(kChange);
		}
	}

	iNumElts = 0;
	WRAPPER_READ(wrapper, "CvCity", &iNumElts);
	m_aBuildingCommerceChange.clear();
	for (unsigned int i = 0; i < iNumElts; ++i)
	{
		BuildingCommerceChange kChange;
		kChange.read(pStream);

		if (kChange.eBuilding != NO_BUILDING)
		{
			m_aBuildingCommerceChange.push_back(kChange);
		}
	}

	// ⛔ DRAIN the event/vote per-building commerce store. It was serialized as its own DECORATED count tag
	// followed by N RAW pStream records, and the rebuild's revert-to-main removed the READER while saves written
	// before that still carry it. Unconsumed, the count tag parks at the head of the stream and every later read
	// in this object slides past it (save.md §3) -- which is what corrupted the NEXT city's m_eOwner and sent
	// BuildingEnabler::onCityCreated through a wild GET_PLAYER/GET_TEAM.
	// ⚑ savemigration.txt CANNOT serve this one: it would drain the COUNT and leave the untagged records behind,
	// so the drain LOOP is the mechanism (save.md §4, the PropertySpawns precedent below).
	// ⚑ A save that never wrote it leaves the count 0 and the loop is a no-op (save.md §2).
	{
		unsigned int iNumEltsBCCEvents = 0;
		WRAPPER_READ_DECORATED(wrapper, "CvCity", &iNumEltsBCCEvents, "iNumEltsBCCEvents");
		for (unsigned int i = 0; i < iNumEltsBCCEvents; ++i)
		{
			BuildingCommerceChange kDrain;
			kDrain.read(pStream);
		}
	}

	iNumElts = 0;
	WRAPPER_READ(wrapper, "CvCity", &iNumElts);
	m_aBuildingHappyChange.clear();
	for (unsigned int i = 0; i < iNumElts; ++i)
	{
		int iBuilding = NO_BUILDING;
		WRAPPER_READ_CLASS_ENUM_ALLOW_MISSING(wrapper, "CvCity", REMAPPED_CLASS_TYPE_BUILDINGS, &iBuilding);
		int iChange;
		WRAPPER_READ(wrapper, "CvCity", &iChange);

		if (iBuilding != NO_BUILDING)
		{
			m_aBuildingHappyChange.push_back(std::make_pair((BuildingTypes)iBuilding, iChange));
		}
	}

	iNumElts = 0;
	WRAPPER_READ(wrapper, "CvCity", &iNumElts);
	m_aBuildingHealthChange.clear();
	for (unsigned int i = 0; i < iNumElts; ++i)
	{
		int iBuilding = NO_BUILDING;
		WRAPPER_READ_CLASS_ENUM_ALLOW_MISSING(wrapper, "CvCity", REMAPPED_CLASS_TYPE_BUILDINGS, &iBuilding);
		int iChange;
		WRAPPER_READ(wrapper, "CvCity", &iChange);

		if (iBuilding != NO_BUILDING)
		{
			m_aBuildingHealthChange.push_back(std::make_pair((BuildingTypes)iBuilding, iChange));
		}
	}

	//	Now the owner has been restored from the save set the info on the building list
	m_BuildingList.setPlayerToOwner();
	m_UnitList.setPlayerToOwner();

	// phunny_pharmer:
	// clear the culture distance cache (note that it is not saved in the .sav file)
	clearCultureDistanceCache();

	//TB Combat Mod (Buildings) begin

	for (int i = 0; i < wrapper.getNumClassEnumValues(REMAPPED_CLASS_TYPE_SPECIALISTS); ++i)
	{
		int	iI = wrapper.getNewClassEnumValue(REMAPPED_CLASS_TYPE_SPECIALISTS, i, true);

		if (iI != -1)
		{
			WRAPPER_READ_ARRAY(wrapper, "CvCity", NUM_YIELD_TYPES, m_ppaaiLocalSpecialistExtraYield[iI]);
		}
		else
		{
			//	Consume the values
			WRAPPER_SKIP_ELEMENT(wrapper, "CvCity", m_ppaaiLocalSpecialistExtraYield[iI], SAVE_VALUE_TYPE_INT_ARRAY);
		}
	}


	WRAPPER_READ(wrapper, "CvCity", &m_iPrioritySpecialist);
	WRAPPER_READ_CLASS_ARRAY_ALLOW_MISSING(wrapper, "CvCity", REMAPPED_CLASS_TYPE_SPECIALISTS, GC.getNumSpecialistInfos(), m_paiSpecialistBannedCount);
	WRAPPER_READ(wrapper, "CvCity", &m_iModifiedBuildingDefenseRecoverySpeedCap);
	WRAPPER_READ_CLASS_ARRAY_ALLOW_MISSING(wrapper, "CvCity", REMAPPED_CLASS_TYPE_COMBATINFOS, GC.getNumUnitCombatInfos(), m_paiUnitCombatDefenseAgainstModifier);
	//TB Combat Mod (Buildings) end

	WRAPPER_READ(wrapper, "CvCity", &m_iZoCCount);
	iNumElts = 0;   // a post-cut save writes no count; pre-zero so a missing tag leaves the drain a no-op
	WRAPPER_READ(wrapper, "CvCity", &iNumElts);
	// DRAIN the retired property-spawn store. The spawns are `triggers` entries now (json.md §5); the
	// store is gone, but its bytes must still be consumed or every later read in this object desyncs.
	// ⛔ It CANNOT be soft-removed via savemigration.txt: the count rides the SHARED `CvCity::iNumElts`
	// tag that five other vectors above also use, and the drain eats consecutive same-named tags
	// (save.md §3) -- listing it would swallow theirs. So the loop stays and discards (save.md §4).
	for (unsigned int i = 0; i < iNumElts; ++i)
	{
		PropertySpawns kDrain;
		kDrain.read(pStream);
	}
	// ⛔ m_ppaaiLocalSpecialistExtraCommerce went with the accumulator cut, but an older save still holds one
	// element per specialist -- and BOTH branches were left empty, so NOTHING consumed them. An unconsumed orphan
	// desyncs every later read in the object (save.md §3), and here it ran past the end of this city into the
	// NEXT one's m_eOwner: BuildingEnabler::onCityCreated then resolved GET_PLAYER -> GET_TEAM through a garbage
	// id and called isHasTech on a wild CvTeam.
	// ⚑ Its YIELD twin above is still LIVE and correctly reads; only the COMMERCE side was cut, which is why the
	// two loops look alike and only one of them is a drain.
	for (int i = 0; i < wrapper.getNumClassEnumValues(REMAPPED_CLASS_TYPE_SPECIALISTS); ++i)
	{
		wrapper.getNewClassEnumValue(REMAPPED_CLASS_TYPE_SPECIALISTS, i, true);
		WRAPPER_SKIP_ELEMENT(wrapper, "CvCity", m_ppaaiLocalSpecialistExtraCommerce[iI], SAVE_VALUE_TYPE_INT_ARRAY);
	}
	WRAPPER_READ(wrapper, "CvCity", &m_bVisibilitySetup);
	m_bVisibilitySetup = false;


	for (int i = 0; i < wrapper.getNumClassEnumValues(REMAPPED_CLASS_TYPE_BONUSES); ++i)
	{
		int	iI = wrapper.getNewClassEnumValue(REMAPPED_CLASS_TYPE_BONUSES, i, true);

		if (iI != -1)
		{
			WRAPPER_READ_CLASS_ARRAY_ALLOW_MISSING(wrapper, "CvCity", REMAPPED_CLASS_TYPE_PROPERTIES, GC.getNumPropertyInfos(), m_ppaaiExtraBonusAidModifier[iI]);
		}
		else
		{
			WRAPPER_SKIP_ELEMENT(wrapper, "CvCity", m_ppaaiExtraBonusAidModifier[iI], SAVE_VALUE_TYPE_INT_ARRAY);
		}
	}
	WRAPPER_READ_CLASS_ARRAY_ALLOW_MISSING(wrapper, "CvCity", REMAPPED_CLASS_TYPE_PROPERTIES, GC.getNumPropertyInfos(), m_paiAidRate);
	WRAPPER_READ(wrapper, "CvCity", &m_iQuarantinedCount);
	// The EVENT/WB half only -- the building half is derived and rebuilt by the reseed (save.md par.5).
	WRAPPER_READ_CLASS_ARRAY_ALLOW_MISSING(wrapper, "CvCity", REMAPPED_CLASS_TYPE_BONUSES, GC.getNumBonusInfos(), m_paiFreeBonusEvents);
	WRAPPER_READ(wrapper, "CvCity", &m_bPropertyControlBuildingQueued);

	WRAPPER_READ(wrapper, "CvCity", &m_iRevIndexDistanceMod);

	// Toffer - Read vectors
	{
		short iType = 0;
		{
			uint16_t iCityOutputHistorySize = 0;
			WRAPPER_READ_DECORATED(wrapper, "CvCity", &iCityOutputHistorySize, "CityOutputHistorySize");

			for (uint16_t iI = 0; iI < iCityOutputHistorySize; iI++)
			{
				uint32_t iTurn = 0;
				WRAPPER_READ_DECORATED(wrapper, "CvCity", &iTurn, "RecentOutputTurn");
				m_outputHistory.setRecentOutputTurn(iI, iTurn);

				uint16_t iCityOutputHistoryNumEntries = 0;
				WRAPPER_READ_DECORATED(wrapper, "CvCity", &iCityOutputHistoryNumEntries, "CityOutputHistoryNumEntries");

				for (uint16_t iJ = 0; iJ < iCityOutputHistoryNumEntries; iJ++)
				{
					uint16_t iOrderType = 0;
					uint16_t iType = 0;
					WRAPPER_READ_DECORATED(wrapper, "CvCity", &iOrderType, "OrderType");
					WRAPPER_READ_DECORATED(wrapper, "CvCity", &iType, "Type");

					m_outputHistory.addToHistory(static_cast<OrderTypes>(iOrderType), iType, static_cast<short>(iI));
				}
			}
		}
		short iSize = 0;

		// Buildings

		iSize = 0;
		WRAPPER_READ_DECORATED(wrapper, "CvCity", &iSize, "BuildingProgressSize");
		for (short i = 0; i < iSize; ++i)
		{
			int iValue = 0;
			WRAPPER_READ_DECORATED(wrapper, "CvCity", &iType, "BuildingProgressType");
			WRAPPER_READ_DECORATED(wrapper, "CvCity", &iValue, "BuildingProgressValue");
			const BuildingTypes eBuilding = static_cast<BuildingTypes>(wrapper.getNewClassEnumValue(REMAPPED_CLASS_TYPE_BUILDINGS, iType, true));

			if (eBuilding != NO_BUILDING)
			{
				m_progressOnBuilding.push_back(std::make_pair(eBuilding, iValue));
			}
		}

		iSize = 0;
		WRAPPER_READ_DECORATED(wrapper, "CvCity", &iSize, "DelayOnBuildingSize");
		for (short i = 0; i < iSize; ++i)
		{
			int iValue = 0;
			WRAPPER_READ_DECORATED(wrapper, "CvCity", &iType, "DelayOnBuildingType");
			WRAPPER_READ_DECORATED(wrapper, "CvCity", &iValue, "DelayOnBuildingValue");
			const BuildingTypes eBuilding = static_cast<BuildingTypes>(wrapper.getNewClassEnumValue(REMAPPED_CLASS_TYPE_BUILDINGS, iType, true));

			if (eBuilding != NO_BUILDING)
			{
				m_delayOnBuilding.push_back(std::make_pair(eBuilding, iValue));
			}
		}

		// Units
		iSize = 0;
		WRAPPER_READ_DECORATED(wrapper, "CvCity", &iSize, "UnitProgressSize");
		for (short i = 0; i < iSize; ++i)
		{
			int iValue = 0;
			WRAPPER_READ_DECORATED(wrapper, "CvCity", &iType, "UnitProgressType");
			WRAPPER_READ_DECORATED(wrapper, "CvCity", &iValue, "UnitProgressValue");
			const UnitTypes eUnit = static_cast<UnitTypes>(wrapper.getNewClassEnumValue(REMAPPED_CLASS_TYPE_UNITS, iType, true));

			if (eUnit != NO_UNIT)
			{
				m_progressOnUnit.push_back(std::make_pair(eUnit, iValue));
			}
		}

		iSize = 0;
		WRAPPER_READ_DECORATED(wrapper, "CvCity", &iSize, "WorkersSize");
		for (short i = 0; i < iSize; ++i)
		{
			int iUnitID = 0;
			WRAPPER_READ_DECORATED(wrapper, "CvCity", &iUnitID, "WorkerUnitID");
			m_workers.push_back(iUnitID);
		}

		iSize = 0;
		WRAPPER_READ_DECORATED(wrapper, "CvCity", &iSize, "DelayOnUnitSize");
		for (short i = 0; i < iSize; ++i)
		{
			int iValue = 0;
			WRAPPER_READ_DECORATED(wrapper, "CvCity", &iType, "DelayOnUnitType");
			WRAPPER_READ_DECORATED(wrapper, "CvCity", &iValue, "DelayOnUnitValue");
			const UnitTypes eUnit = static_cast<UnitTypes>(wrapper.getNewClassEnumValue(REMAPPED_CLASS_TYPE_UNITS, iType, true));

			if (eUnit != NO_UNIT)
			{
				m_delayOnUnit.push_back(std::make_pair(eUnit, iValue));
			}
		}

		// Bonuses
		iSize = 0;
		WRAPPER_READ_DECORATED(wrapper, "CvCity", &iSize, "CorpBonusProductionSize");
		for (short i = 0; i < iSize; ++i)
		{
			int iValue = 0;
			WRAPPER_READ_DECORATED(wrapper, "CvCity", &iType, "CorpBonusProductionType");
			WRAPPER_READ_DECORATED(wrapper, "CvCity", &iValue, "CorpBonusProductionValue");
			const BonusTypes eBonus = static_cast<BonusTypes>(wrapper.getNewClassEnumValue(REMAPPED_CLASS_TYPE_BONUSES, iType, true));

			if (eBonus != NO_BONUS)
			{
				m_corpBonusProduction.push_back(std::make_pair(eBonus, iValue));
			}
		}
	}
	// Toffer - Read maps
	{
		short iSize = 0;
		short iType = 0;
		int iCount = 0;
		uint16_t sCountU = 0;
		{
		}

		iSize = 0;
		WRAPPER_READ_DECORATED(wrapper, "CvCity", &iSize, "BuildingLedgerSize");
		while (iSize-- > 0)
		{
			int iPlayer;
			int iTime;
			WRAPPER_READ_DECORATED(wrapper, "CvCity", &iType, "BuildingLedgerType");
			WRAPPER_READ_DECORATED(wrapper, "CvCity", &iPlayer, "BuildingLedgerBuiltBy");
			WRAPPER_READ_DECORATED(wrapper, "CvCity", &iTime, "BuildingLedgerTimeBuilt");
			const BuildingTypes eType = static_cast<BuildingTypes>(wrapper.getNewClassEnumValue(REMAPPED_CLASS_TYPE_BUILDINGS, iType, true));

			if (eType != NO_BUILDING)
			{
				// DEC-empire-level-buildings: an old save's per-city copy of an empire-level building folds to
				// the OWNER (idempotent -- the player read already normalized its count, so this is a no-op on
				// every copy after the first); the city keeps nothing and writes nothing back.
				if (GC.getBuildingInfo(eType).isEmpireLevel())
				{
					GET_PLAYER((PlayerTypes)m_eOwner).setHasEmpireBuilding(eType, true, /*bFirst*/false);
					continue;
				}
				m_bHasBuildings[eType] = true; // quick lookup
				m_hasBuildings.push_back(eType); // quick iteration
				// #430 reseed (event-spine.md the load-RESEED): the per-city building DOMAIN event fires HERE, as each
				// building deserializes off the ledger stream, INSIDE the read loop -- the genuine per-element read
				// (not a later walk over a populated array, superseded-ideas #13). Feeds the cache-build consumer's
				// per-city operating-building set + building packages. m_iID / m_eOwner are read earlier in read().
				emitCityBuildingAdded(m_iID, (int)m_eOwner, (int)eType, /*bFirst*/false);   // a load RESTORES; it is not a first acquisition
				// compressed data all buildings have
				BuiltBuildingData data;
				data.eBuiltBy = (PlayerTypes)iPlayer;
				data.iTimeBuilt = iTime;
				m_buildingLedger.insert(std::make_pair(eType, data));
			}
		}
	}
	WRAPPER_READ_OBJECT_END(wrapper);
	//Example of how to skip an unneeded element
	//WRAPPER_SKIP_ELEMENT(wrapper, "CvCity", m_iMaxFoodKeptPercent, SAVE_VALUE_ANY);	// was present in old formats
}

void CvCity::read(FDataStreamBase* pStream)
{
	readIdentity(pStream);
	readBody(pStream);
}

void CvCity::write(FDataStreamBase* pStream)
{
	PROFILE_EXTRA_FUNC();
	CvTaggedSaveFormatWrapper& wrapper = CvTaggedSaveFormatWrapper::getSaveFormatWrapper();

	wrapper.AttachToStream(pStream);

	WRAPPER_WRITE_OBJECT_START(wrapper);

	WRAPPER_WRITE(wrapper, "CvCity", m_iID);
	WRAPPER_WRITE(wrapper, "CvCity", m_iX);
	WRAPPER_WRITE(wrapper, "CvCity", m_iY);
	WRAPPER_WRITE(wrapper, "CvCity", m_iRallyX);
	WRAPPER_WRITE(wrapper, "CvCity", m_iRallyY);
	WRAPPER_WRITE(wrapper, "CvCity", m_iGameTurnFounded);
	WRAPPER_WRITE(wrapper, "CvCity", m_iGameTurnAcquired);
	WRAPPER_WRITE(wrapper, "CvCity", m_iPopulation);
	WRAPPER_WRITE(wrapper, "CvCity", m_iHighestPopulation);
	WRAPPER_WRITE(wrapper, "CvCity", m_iWorkingPopulation);
	WRAPPER_WRITE(wrapper, "CvCity", m_iSpecialistPopulation);
	WRAPPER_WRITE(wrapper, "CvCity", m_iNumGreatPeople);
	WRAPPER_WRITE(wrapper, "CvCity", m_iGreatPeopleProgress);
	WRAPPER_WRITE(wrapper, "CvCity", m_iNumWorldWonders);
	WRAPPER_WRITE(wrapper, "CvCity", m_iNumTeamWonders);
	WRAPPER_WRITE(wrapper, "CvCity", m_iNumNationalWonders);
	WRAPPER_WRITE(wrapper, "CvCity", m_iNumBuildings);
	WRAPPER_WRITE(wrapper, "CvCity", m_iEspionageHealthCounter);
	WRAPPER_WRITE(wrapper, "CvCity", m_iEspionageHappinessCounter);
	WRAPPER_WRITE(wrapper, "CvCity", m_iFreshWaterGoodHealth);
	WRAPPER_WRITE(wrapper, "CvCity", m_iHurryAngerTimer);
	WRAPPER_WRITE(wrapper, "CvCity", m_iRevRequestAngerTimer);
	WRAPPER_WRITE(wrapper, "CvCity", m_iRevSuccessTimer);
	WRAPPER_WRITE(wrapper, "CvCity", m_iConscriptAngerTimer);
	WRAPPER_WRITE(wrapper, "CvCity", m_iDefyResolutionAngerTimer);
	WRAPPER_WRITE(wrapper, "CvCity", m_iHappinessTimer);
	WRAPPER_WRITE(wrapper, "CvCity", m_iMilitaryHappinessUnits);
	WRAPPER_WRITE(wrapper, "CvCity", m_iExtraHappiness);
	WRAPPER_WRITE(wrapper, "CvCity", m_iExtraHealth);
	WRAPPER_WRITE(wrapper, "CvCity", m_iFood);
	WRAPPER_WRITE(wrapper, "CvCity", m_iFoodKept);
	WRAPPER_WRITE(wrapper, "CvCity", m_iOverflowProduction);
	WRAPPER_WRITE(wrapper, "CvCity", m_iFeatureProduction);
	WRAPPER_WRITE(wrapper, "CvCity", m_iCurrAirlift);
	WRAPPER_WRITE(wrapper, "CvCity", m_iMaxAirlift);
	WRAPPER_WRITE(wrapper, "CvCity", m_iAirUnitCapacity);
	WRAPPER_WRITE(wrapper, "CvCity", m_iDefenseDamage);
	WRAPPER_WRITE(wrapper, "CvCity", m_iLastDefenseDamage);
	WRAPPER_WRITE(wrapper, "CvCity", m_iOccupationTimer);
	WRAPPER_WRITE(wrapper, "CvCity", m_iCultureUpdateTimer);
	WRAPPER_WRITE(wrapper, "CvCity", m_iCitySizeBoost);

	WRAPPER_WRITE(wrapper, "CvCity", m_bNeverLost);
	WRAPPER_WRITE(wrapper, "CvCity", m_bBombarded);
	WRAPPER_WRITE(wrapper, "CvCity", m_bDrafted);
	WRAPPER_WRITE(wrapper, "CvCity", m_bAirliftTargeted);
	WRAPPER_WRITE_ARRAY(wrapper, "CvCity", NUM_CITY_STATUSES, m_aiStatusTurns);
	EVENT_GRANTS_WRITE(wrapper, "CvCity", m_eventGrants);
	WRAPPER_WRITE(wrapper, "CvCity", m_bCitizensAutomated);
	WRAPPER_WRITE(wrapper, "CvCity", m_bProductionAutomated);
	WRAPPER_WRITE(wrapper, "CvCity", m_bWallOverride);
	// m_bInfoDirty not saved...
	// m_bLayoutDirty not saved...
	WRAPPER_WRITE(wrapper, "CvCity", m_bPlundered);
	WRAPPER_WRITE(wrapper, "CvCity", m_eOwner);
	WRAPPER_WRITE(wrapper, "CvCity", m_ePreviousOwner);
	WRAPPER_WRITE(wrapper, "CvCity", m_eOriginalOwner);
	WRAPPER_WRITE(wrapper, "CvCity", m_eCultureLevel);
	WRAPPER_WRITE(wrapper, "CvCity", m_iRevolutionIndex);
	WRAPPER_WRITE(wrapper, "CvCity", m_iLocalRevIndex);
	WRAPPER_WRITE(wrapper, "CvCity", m_iRevIndexAverage);
	WRAPPER_WRITE(wrapper, "CvCity", m_iRevolutionCounter);
	WRAPPER_WRITE(wrapper, "CvCity", m_iReinforcementCounter);

	WRAPPER_WRITE_ARRAY(wrapper, "CvCity", NUM_YIELD_TYPES, m_aiYieldRateModifier);
	WRAPPER_WRITE_ARRAY(wrapper, "CvCity", NUM_COMMERCE_TYPES, m_aiProductionToCommerceModifier);

	WRAPPER_WRITE_ARRAY(wrapper, "CvCity", MAX_PLAYERS, m_aiCulture);
	WRAPPER_WRITE_ARRAY(wrapper, "CvCity", MAX_PLAYERS, m_aiNumRevolts);

	WRAPPER_WRITE_ARRAY(wrapper, "CvCity", MAX_PLAYERS, m_abEverOwned);
	WRAPPER_WRITE_ARRAY(wrapper, "CvCity", MAX_PLAYERS, m_abTradeRoute);
	WRAPPER_WRITE_ARRAY(wrapper, "CvCity", MAX_TEAMS, m_abRevealed);
	WRAPPER_WRITE_ARRAY(wrapper, "CvCity", MAX_TEAMS, m_abEspionageVisibility);

	WRAPPER_WRITE_STRING(wrapper, "CvCity", m_szName);
	WRAPPER_WRITE_STRING(wrapper, "CvCity", m_szScriptData);

	WRAPPER_WRITE_CLASS_ARRAY(wrapper, "CvCity", REMAPPED_CLASS_TYPE_PROJECTS, GC.getNumProjectInfos(), m_paiProjectProduction);
	WRAPPER_WRITE_CLASS_ARRAY(wrapper, "CvCity", REMAPPED_CLASS_TYPE_UNITS, GC.getNumUnitInfos(), m_paiUnitProduction);
	WRAPPER_WRITE_CLASS_ARRAY(wrapper, "CvCity", REMAPPED_CLASS_TYPE_UNITS, GC.getNumUnitInfos(), m_paiGreatPeopleUnitRate);
	WRAPPER_WRITE_CLASS_ARRAY(wrapper, "CvCity", REMAPPED_CLASS_TYPE_UNITS, GC.getNumUnitInfos(), m_paiGreatPeopleUnitProgress);
	WRAPPER_WRITE_CLASS_ARRAY(wrapper, "CvCity", REMAPPED_CLASS_TYPE_SPECIALISTS, GC.getNumSpecialistInfos(), m_paiSpecialistCount);
	WRAPPER_WRITE_CLASS_ARRAY(wrapper, "CvCity", REMAPPED_CLASS_TYPE_SPECIALISTS, GC.getNumSpecialistInfos(), m_paiForceSpecialistCount);
	WRAPPER_WRITE_CLASS_ARRAY(wrapper, "CvCity", REMAPPED_CLASS_TYPE_SPECIALISTS, GC.getNumSpecialistInfos(), m_paiFreeSpecialistCountUnattributed);
	WRAPPER_WRITE_CLASS_ARRAY(wrapper, "CvCity", REMAPPED_CLASS_TYPE_RELIGIONS, GC.getNumReligionInfos(), m_paiReligionInfluence);

	WRAPPER_WRITE_ARRAY(wrapper, "CvCity", NUM_CITY_PLOTS, m_pabWorkingPlot);
	WRAPPER_WRITE_CLASS_ARRAY(wrapper, "CvCity", REMAPPED_CLASS_TYPE_RELIGIONS, GC.getNumReligionInfos(), m_pabHasReligion);
	WRAPPER_WRITE_CLASS_ARRAY(wrapper, "CvCity", REMAPPED_CLASS_TYPE_CORPORATIONS, GC.getNumCorporationInfos(), m_pabHasCorporation);

	WRAPPER_WRITE(wrapper, "CvCity", m_iEventAnger);
	WRAPPER_WRITE_CLASS_ENUM(wrapper, "CvCity", REMAPPED_CLASS_TYPE_CIVILIZATIONS, m_iCiv);

	WRAPPER_WRITE(wrapper, "CvCity", m_iLandmarkAngerTimer);

	WRAPPER_WRITE(wrapper, "CvCity", m_iWorkableRadiusOverride);
	WRAPPER_WRITE(wrapper, "CvCity", m_iProtectedCultureCount);
	WRAPPER_WRITE(wrapper, "CvCity", m_iWarWearinessTimer);

	WRAPPER_WRITE_CLASS_ARRAY(wrapper, "CvCity", REMAPPED_CLASS_TYPE_COMBATINFOS, GC.getNumUnitCombatInfos(), m_paiUnitCombatExtraStrength);
	WRAPPER_WRITE_CLASS_ARRAY(wrapper, "CvCity", REMAPPED_CLASS_TYPE_BUILDS, GC.getNumBuildInfos(), m_pabAutomatedCanBuild);

	WRAPPER_WRITE(wrapper, "CvCity", m_iMinimumDefenseLevel);
	WRAPPER_WRITE(wrapper, "CvCity", m_iHealthPercentPerPopulation);

	const int iNumTradeRoutes = m_paTradeCities.size();
	WRAPPER_WRITE(wrapper, "CvCity", iNumTradeRoutes);
	for (int iI = 0; iI < iNumTradeRoutes; iI++)
	{
		WRAPPER_WRITE(wrapper, "CvCity", m_paTradeCities[iI].eOwner);
		WRAPPER_WRITE(wrapper, "CvCity", m_paTradeCities[iI].iID);
	}

	const int orderQueueSize = m_orderQueue.size();
	WRAPPER_WRITE(wrapper, "CvCity", orderQueueSize);
	foreach_(const OrderData& orderData, m_orderQueue)
	{
		WRAPPER_WRITE_ARRAY_DECORATED(wrapper, "CvCity", sizeof(OrderData), (const uint8_t*)&orderData, "OrderData");
	}

	WRAPPER_WRITE(wrapper, "CvCity", m_iPopulationRank);
	WRAPPER_WRITE(wrapper, "CvCity", m_bPopulationRankValid);
	WRAPPER_WRITE_ARRAY(wrapper, "CvCity", NUM_YIELD_TYPES, m_aiBaseYieldRank);
	WRAPPER_WRITE_ARRAY(wrapper, "CvCity", NUM_YIELD_TYPES, m_abBaseYieldRankValid);
	WRAPPER_WRITE_ARRAY(wrapper, "CvCity", NUM_YIELD_TYPES, m_aiYieldRank);
	WRAPPER_WRITE_ARRAY(wrapper, "CvCity", NUM_YIELD_TYPES, m_abYieldRankValid);
	WRAPPER_WRITE_ARRAY(wrapper, "CvCity", NUM_COMMERCE_TYPES, m_aiCommerceRank);
	WRAPPER_WRITE_ARRAY(wrapper, "CvCity", NUM_COMMERCE_TYPES, m_abCommerceRankValid);

	m_Properties.writeWrapper(pStream);

	WRAPPER_WRITE_DECORATED(wrapper, "CvCity", m_aEventsOccured.size(), "iNumElts");
	foreach_(const EventTypes eEvent, m_aEventsOccured)
	{
		WRAPPER_WRITE_CLASS_ENUM_DECORATED(wrapper, "CvCity", REMAPPED_CLASS_TYPE_EVENTS, eEvent, "eEvent");
	}

	WRAPPER_WRITE_DECORATED(wrapper, "CvCity", m_aBuildingYieldChange.size(), "iNumElts");
	foreach_(BuildingYieldChange& pChange, m_aBuildingYieldChange)
	{
		pChange.write(pStream);
	}

	WRAPPER_WRITE_DECORATED(wrapper, "CvCity", m_aBuildingCommerceChange.size(), "iNumElts");
	foreach_(BuildingCommerceChange& pChange, m_aBuildingCommerceChange)
	{
		pChange.write(pStream);
	}

	WRAPPER_WRITE_DECORATED(wrapper, "CvCity", m_aBuildingHappyChange.size(), "iNumElts");
	for (BuildingChangeArray::iterator it = m_aBuildingHappyChange.begin(); it != m_aBuildingHappyChange.end(); ++it)
	{
		WRAPPER_WRITE_CLASS_ENUM_DECORATED(wrapper, "CvCity", REMAPPED_CLASS_TYPE_BUILDINGS, (*it).first, "iBuilding");
		WRAPPER_WRITE_DECORATED(wrapper, "CvCity", (*it).second, "iChange");
	}

	WRAPPER_WRITE_DECORATED(wrapper, "CvCity", m_aBuildingHealthChange.size(), "iNumElts");
	for (BuildingChangeArray::iterator it = m_aBuildingHealthChange.begin(); it != m_aBuildingHealthChange.end(); ++it)
	{
		WRAPPER_WRITE_CLASS_ENUM_DECORATED(wrapper, "CvCity", REMAPPED_CLASS_TYPE_BUILDINGS, (*it).first, "iBuilding");
		WRAPPER_WRITE_DECORATED(wrapper, "CvCity", (*it).second, "iChange");
	}

	//TB Combat Mod (Buildings) begin

	for (int iI = 0; iI < GC.getNumSpecialistInfos(); iI++)
	{
		WRAPPER_WRITE_ARRAY(wrapper, "CvCity", NUM_YIELD_TYPES, m_ppaaiLocalSpecialistExtraYield[iI]);
	}
	WRAPPER_WRITE(wrapper, "CvCity", m_iPrioritySpecialist);
	WRAPPER_WRITE_CLASS_ARRAY(wrapper, "CvCity", REMAPPED_CLASS_TYPE_SPECIALISTS, GC.getNumSpecialistInfos(), m_paiSpecialistBannedCount);
	WRAPPER_WRITE(wrapper, "CvCity", m_iModifiedBuildingDefenseRecoverySpeedCap);
	WRAPPER_WRITE_CLASS_ARRAY(wrapper, "CvCity", REMAPPED_CLASS_TYPE_COMBATINFOS, GC.getNumUnitCombatInfos(), m_paiUnitCombatDefenseAgainstModifier);
	//TB Combat Mod (Buildings) end

	WRAPPER_WRITE(wrapper, "CvCity", m_iZoCCount);

	for (int iI = 0; iI < GC.getNumSpecialistInfos(); iI++)
	{
	}
	WRAPPER_WRITE(wrapper, "CvCity", m_bVisibilitySetup);

	for (int iI = 0; iI < GC.getNumBonusInfos(); iI++)
	{
		WRAPPER_WRITE_CLASS_ARRAY(wrapper, "CvCity", REMAPPED_CLASS_TYPE_PROPERTIES, GC.getNumPropertyInfos(), m_ppaaiExtraBonusAidModifier[iI]);
	}

	WRAPPER_WRITE_CLASS_ARRAY(wrapper, "CvCity", REMAPPED_CLASS_TYPE_PROPERTIES, GC.getNumPropertyInfos(), m_paiAidRate);
	WRAPPER_WRITE(wrapper, "CvCity", m_iQuarantinedCount);
	WRAPPER_WRITE_CLASS_ARRAY(wrapper, "CvCity", REMAPPED_CLASS_TYPE_BONUSES, GC.getNumBonusInfos(), m_paiFreeBonusEvents);
	WRAPPER_WRITE(wrapper, "CvCity", m_bPropertyControlBuildingQueued);


	WRAPPER_WRITE(wrapper, "CvCity", m_iRevIndexDistanceMod);

	// Toffer - Write vectors
	{
		{
			uint16_t iCityOutputHistorySize = CityOutputHistory::getCityOutputHistorySize();
			WRAPPER_WRITE_DECORATED(wrapper, "CvCity", iCityOutputHistorySize, "CityOutputHistorySize");
			for (uint16_t iI = 0; iI < iCityOutputHistorySize; iI++)
			{
				WRAPPER_WRITE_DECORATED(wrapper, "CvCity", m_outputHistory.getRecentOutputTurn(iI), "RecentOutputTurn");
				uint16_t iCityOutputHistoryNumEntries = m_outputHistory.getCityOutputHistoryNumEntries(iI);
				WRAPPER_WRITE_DECORATED(wrapper, "CvCity", iCityOutputHistoryNumEntries, "CityOutputHistoryNumEntries");
				for (uint16_t iJ = 0; iJ < iCityOutputHistoryNumEntries; iJ++)
				{
					WRAPPER_WRITE_DECORATED(wrapper, "CvCity", m_outputHistory.getCityOutputHistoryEntry(iI, iJ, true), "OrderType");
					WRAPPER_WRITE_DECORATED(wrapper, "CvCity", m_outputHistory.getCityOutputHistoryEntry(iI, iJ, false), "Type");
				}
			}
		}

		// Buildings
		WRAPPER_WRITE_DECORATED(wrapper, "CvCity", (short)m_progressOnBuilding.size(), "BuildingProgressSize");
		for (std::vector< std::pair<BuildingTypes, int> >::iterator it = m_progressOnBuilding.begin(); it != m_progressOnBuilding.end(); ++it)
		{
			WRAPPER_WRITE_DECORATED(wrapper, "CvCity", static_cast<short>((*it).first), "BuildingProgressType");
			WRAPPER_WRITE_DECORATED(wrapper, "CvCity", (*it).second, "BuildingProgressValue");
		}

		WRAPPER_WRITE_DECORATED(wrapper, "CvCity", (short)m_delayOnBuilding.size(), "DelayOnBuildingSize");
		for (std::vector< std::pair<BuildingTypes, int> >::iterator it = m_delayOnBuilding.begin(); it != m_delayOnBuilding.end(); ++it)
		{
			WRAPPER_WRITE_DECORATED(wrapper, "CvCity", static_cast<short>((*it).first), "DelayOnBuildingType");
			WRAPPER_WRITE_DECORATED(wrapper, "CvCity", (*it).second, "DelayOnBuildingValue");
		}

		// Units
		WRAPPER_WRITE_DECORATED(wrapper, "CvCity", (short)m_progressOnUnit.size(), "UnitProgressSize");
		for (std::vector< std::pair<UnitTypes, int> >::iterator it = m_progressOnUnit.begin(); it != m_progressOnUnit.end(); ++it)
		{
			WRAPPER_WRITE_DECORATED(wrapper, "CvCity", static_cast<short>((*it).first), "UnitProgressType");
			WRAPPER_WRITE_DECORATED(wrapper, "CvCity", (*it).second, "UnitProgressValue");
		}

		WRAPPER_WRITE_DECORATED(wrapper, "CvCity", (short)m_workers.size(), "WorkersSize");
		foreach_(const int iUnitID, m_workers)
		{
			WRAPPER_WRITE_DECORATED(wrapper, "CvCity", iUnitID, "WorkerUnitID");
		}

		WRAPPER_WRITE_DECORATED(wrapper, "CvCity", (short)m_delayOnUnit.size(), "DelayOnUnitSize");
		for (std::vector< std::pair<UnitTypes, int> >::iterator it = m_delayOnUnit.begin(); it != m_delayOnUnit.end(); ++it)
		{
			WRAPPER_WRITE_DECORATED(wrapper, "CvCity", static_cast<short>((*it).first), "DelayOnUnitType");
			WRAPPER_WRITE_DECORATED(wrapper, "CvCity", (*it).second, "DelayOnUnitValue");
		}

		// Bonuses
		WRAPPER_WRITE_DECORATED(wrapper, "CvCity", (short)m_corpBonusProduction.size(), "CorpBonusProductionSize");
		for (std::vector< std::pair<BonusTypes, int> >::iterator it = m_corpBonusProduction.begin(); it != m_corpBonusProduction.end(); ++it)
		{
			WRAPPER_WRITE_DECORATED(wrapper, "CvCity", static_cast<short>((*it).first), "CorpBonusProductionType");
			WRAPPER_WRITE_DECORATED(wrapper, "CvCity", (*it).second, "CorpBonusProductionValue");
		}
	}
	// Toffer - Write Maps
	{
		WRAPPER_WRITE_DECORATED(wrapper, "CvCity", (short)m_buildingLedger.size(), "BuildingLedgerSize");
		for (std::map<BuildingTypes, BuiltBuildingData>::const_iterator it = m_buildingLedger.begin(), itEnd = m_buildingLedger.end(); it != itEnd; ++it)
		{
			WRAPPER_WRITE_DECORATED(wrapper, "CvCity", static_cast<short>(it->first), "BuildingLedgerType");
			WRAPPER_WRITE_DECORATED(wrapper, "CvCity", it->second.eBuiltBy, "BuildingLedgerBuiltBy");
			WRAPPER_WRITE_DECORATED(wrapper, "CvCity", it->second.iTimeBuilt, "BuildingLedgerTimeBuilt");
		}

	}
	WRAPPER_WRITE_OBJECT_END(wrapper);
}


//------------------------------------------------------------------------------------------------
class VisibleBuildingComparator
{
public:
	bool operator() (BuildingTypes e1, BuildingTypes e2)
	{
		if (GC.getBuildingInfo(e1).getVisibilityPriority() > GC.getBuildingInfo(e2).getVisibilityPriority())
			return true;
		else if (GC.getBuildingInfo(e1).getVisibilityPriority() == GC.getBuildingInfo(e2).getVisibilityPriority())
		{
			//break ties by building type higher building type
			if (e1 > e2)
				return true;
		}

		return false;
	}
};

//	Flags to determine which building types are displayed
#define	SHOW_BUILDINGS_WONDERS	1
#define	SHOW_BUILDINGS_DEFENSES	2
#define	SHOW_BUILDINGS_OTHER	128

void CvCity::getVisibleBuildings(std::list<BuildingTypes>& kChosenVisible, int& iChosenNumGenerics)
{
	PROFILE_EXTRA_FUNC();
	if (!plot()->isGraphicsVisible(ECvPlotGraphics::CITY))
	{
		iChosenNumGenerics = 0;
		return;
	}

	const int iShowFlags = GC.getSHOW_BUILDINGS_LEVEL();
	std::vector<BuildingTypes> kVisible;

	foreach_(const BuildingTypes eType, getHasBuildings())
	{
		bool bValid = false;
		const CvBuildingInfo& kBuilding = GC.getBuildingInfo(eType);

		//	A building with no model to place is not offered at all -- 90% of them are scaled to nothing.
		if (kBuilding.isNotShownInCity())
		{
			continue;
		}

		const bool bIsWonder = isLimitedWonder(eType);
		const bool bIsDefense = (kBuilding.getDefense(DEFENSE_AMOUNT, CASC_SCOPE_CITY) > 0);

		if ((iShowFlags & SHOW_BUILDINGS_WONDERS) != 0)
		{
			//	Wonders
			bValid |= bIsWonder;
		}
		if ((iShowFlags & SHOW_BUILDINGS_DEFENSES) != 0)
		{
			//	Wonders
			bValid |= bIsDefense;
		}
		if ((iShowFlags & SHOW_BUILDINGS_OTHER) != 0)
		{
			//	Wonders
			bValid |= (!bIsWonder && !bIsDefense);
		}

		if (bValid)
		{
			kVisible.push_back(eType);
		}
	}

	// sort the visible ones by decreasing priority
	algo::sort(kVisible, VisibleBuildingComparator());

	// how big is this city, in terms of buildings?
	// general rule: no more than fPercentUnique percent of a city can be uniques
	int iTotalVisibleBuildings = 0;
	if (stricmp(GC.getDefineSTRING("GAME_CITY_SIZE_METHOD"), "METHOD_EXPONENTIAL") == 0)
	{
		int iCityScaleMod = ((int)(pow((float)getPopulation(), GC.getDefineFLOAT("GAME_CITY_SIZE_EXP_MODIFIER")))) * 2;
		iTotalVisibleBuildings = (10 + iCityScaleMod);
	}
	else
	{
		float fLo = GC.getDefineFLOAT("GAME_CITY_SIZE_LINMAP_AT_0");
		float fHi = GC.getDefineFLOAT("GAME_CITY_SIZE_LINMAP_AT_50");
		float fCurSize = (float)getPopulation();
		iTotalVisibleBuildings = int(((fHi - fLo) / 50.0f) * fCurSize + fLo);
	}
	float fMaxUniquePercent = GC.getDefineFLOAT("GAME_CITY_SIZE_MAX_PERCENT_UNIQUE");
	int iMaxNumUniques = (int)(fMaxUniquePercent * iTotalVisibleBuildings);

	// compute how many buildings are generics vs. unique Civ buildings?
	int iNumGenerics;
	int iNumUniques;
	if ((int)kVisible.size() > iMaxNumUniques)
	{
		iNumUniques = iMaxNumUniques;
	}
	else
	{
		iNumUniques = kVisible.size();
	}
	iNumGenerics = iTotalVisibleBuildings - iNumUniques + getCitySizeBoost();

	// return
	iChosenNumGenerics = iNumGenerics;
	for (int i = 0; i < iNumUniques; i++)
	{
		kChosenVisible.push_back(kVisible[i]);
	}
}

static int natGetDeterministicRandom(int iMin, int iMax, int iSeedX, int iSeedY)
{
	srand(7297 * iSeedX + 2909 * iSeedY);
	return (rand() % (iMax - iMin)) + iMin;
}

void CvCity::getVisibleEffects(ZoomLevelTypes eCurZoom, std::vector<const char*>& kEffectNames)
{
	PROFILE_EXTRA_FUNC();
	if (isOccupation() && isVisible(getTeam(), false) == true)
	{
		if (eCurZoom == ZOOM_DETAIL)
		{
			kEffectNames.push_back("EFFECT_CITY_BIG_BURNING_SMOKE");
			kEffectNames.push_back("EFFECT_CITY_FIRE");
		}
		else
		{
			kEffectNames.push_back("EFFECT_CITY_BIG_BURNING_SMOKE");
		}
		return;
	}

	if ((getTeam() == GC.getGame().getActiveTeam()) || GC.getGame().isDebugMode())
	{

		if (angryPopulation() > 0)
		{
			kEffectNames.push_back("EFFECT_CITY_BURNING_SMOKE");
		}

		if (healthRate() < 0)
		{
			kEffectNames.push_back("EFFECT_CITY_DISEASED");
		}


		if (isWeLoveTheKingDay())
		{
			int iSeed = natGetDeterministicRandom(0, 32767, getX(), getY());
			CvRandom kRand;
			kRand.init(iSeed);

			// fireworks
			const char* szFireworkEffects[] =
			{ "FIREWORKS_RED_LARGE_SLOW",
				"FIREWORKS_RED_SMALL_FAST",
				"FIREWORKS_GREEN_LARGE_SLOW",
				"FIREWORKS_GREEN_SMALL_FAST",
				"FIREWORKS_PURPLE_LARGE_SLOW",
				"FIREWORKS_PURPLE_SMALL_FAST",
				"FIREWORKS_YELLOW_LARGE_SLOW",
				"FIREWORKS_YELLOW_SMALL_FAST",
				"FIREWORKS_BLUE_LARGE_SLOW",
				"FIREWORKS_BLUE_SMALL_FAST" };

			int iNumEffects = sizeof(szFireworkEffects) / sizeof(char*);
			for (int i = 0; i < (iNumEffects < 3 ? iNumEffects : 3); i++)
			{
				kEffectNames.push_back(szFireworkEffects[kRand.get(iNumEffects)]);
			}
		}
	}
}

void CvCity::getCityBillboardSizeIconColors(NiColorA& kDotColor, NiColorA& kTextColor) const
{
	const NiColorA kPlayerColor = GC.getColorInfo((ColorTypes)GC.getPlayerColorInfo(GET_PLAYER(getOwner()).getPlayerColor()).getColorTypePrimary()).getColor();
	static const NiColorA kGrowing(0.73f, 1, 0.73f, 1);
	static const NiColorA kShrinking(1, 0.73f, 0.73f, 1);
	static const NiColorA kStagnant(0.83f, 0.83f, 0.83f, 1);
	static const NiColorA kUnknown(.5f, .5f, .5f, 1);
	static const NiColorA kWhite(1, 1, 1, 1);
	static const NiColorA kBlack(0, 0, 0, 1);

	if ((getTeam() == GC.getGame().getActiveTeam()))
	{
		// A UI EDGE: reduced ONCE here, because the test below compares against a whole-food literal and the
		// stored food beside it is the whole-unit bar ([DEC-fixedpoint-x100]).
		const int iFoodDifference = foodDifference() / 100;
		if (iFoodDifference < 0)
		{
			if ((iFoodDifference == -1) && (getFood() >= ((75 * growthThreshold()) / 100)))
			{
				kDotColor = kStagnant;
				kTextColor = kBlack;
			}
			else
			{
				kDotColor = kShrinking;
				kTextColor = kBlack;
			}
		}
		else if (iFoodDifference > 0)
		{
			kDotColor = kGrowing;
			kTextColor = kBlack;
		}
		else if (iFoodDifference == 0)
		{
			kDotColor = kStagnant;
			kTextColor = kBlack;
		}
	}
	else
	{
		kDotColor = kPlayerColor;
		const NiColorA kPlayerSecondaryColor = GC.getColorInfo((ColorTypes)GC.getPlayerColorInfo(GET_PLAYER(getOwner()).getPlayerColor()).getColorTypeSecondary()).getColor();
		kTextColor = kPlayerSecondaryColor;
	}
}

const char* CvCity::getCityBillboardProductionIcon() const
{
	if (canBeSelected() && isProduction())
	{
		bst::optional<OrderData> nextOrder = getHeadOrder();
		FAssert(nextOrder);

		const char* szIcon = NULL;
		switch (nextOrder->eOrderType)
		{
		case ORDER_TRAIN:
		{
			UnitTypes eType = getProductionUnit();
			FAssert(eType != NO_UNIT);
			szIcon = GET_PLAYER(getOwner()).getUnitButton(eType);
			break;
		}
		case ORDER_CONSTRUCT:
		{
			BuildingTypes eType = getProductionBuilding();
			FAssert(eType != NO_BUILDING);
			szIcon = GC.getBuildingInfo(eType).getButton();
			break;
		}
		case ORDER_CREATE:
		{
			ProjectTypes eType = getProductionProject();
			FAssert(eType != NO_PROJECT);
			szIcon = GC.getProjectInfo(eType).getButton();
			break;
		}
		case ORDER_MAINTAIN:
		{
			ProcessTypes eType = getProductionProcess();
			FAssert(eType != NO_PROCESS);
			szIcon = GC.getProcessInfo(eType).getButton();
			break;
		}
		case ORDER_LIST:
		{
			// Should not happen
			break;
		}
		default:
		{
			FErrorMsg("error");
		}
		}
		return szIcon;
	}
	return ARTFILEMGR.getInterfaceArtInfo("INTERFACE_BUTTONS_NOPRODUCTION")->getPath();
}

bool CvCity::getFoodBarPercentages(std::vector<float>& afPercentages) const
{
	if (!canBeSelected())
	{
		return false;
	}
	afPercentages.resize(NUM_INFOBAR_TYPES, 0.0f);
	// A UI EDGE: every other term here is the whole-unit food BAR (stored food, the growth threshold), so the
	// x100 surplus reduces ONCE into this local ([DEC-fixedpoint-x100]).
	const int iFoodDifference = foodDifference() / 100;
	if (iFoodDifference < 0)
	{
		afPercentages[INFOBAR_STORED] = std::max(0, (getFood() + iFoodDifference)) / (float)growthThreshold();
		afPercentages[INFOBAR_RATE_EXTRA] = std::min(-iFoodDifference, getFood()) / (float)growthThreshold();
	}
	else
	{
		afPercentages[INFOBAR_STORED] = getFood() / (float)growthThreshold();
		afPercentages[INFOBAR_RATE] = iFoodDifference / (float)growthThreshold();
	}
	return true;
}

bool CvCity::getProductionBarPercentages(std::vector<float>& afPercentages) const
{
	if (!canBeSelected())
	{
		return false;
	}

	if (!isProductionProcess())
	{
		afPercentages.resize(NUM_INFOBAR_TYPES, 0.0f);
		const int iProductionDiffNoFood = getCurrentProductionDifference(ProductionCalc::Overflow);
		const int iProductionDiffJustFood = getCurrentProductionDifference(ProductionCalc::FoodProduction | ProductionCalc::Overflow) - iProductionDiffNoFood;
		afPercentages[INFOBAR_STORED] = getProductionProgress() / (float)getProductionNeeded();
		afPercentages[INFOBAR_RATE] = iProductionDiffNoFood / (float)getProductionNeeded();
		afPercentages[INFOBAR_RATE_EXTRA] = iProductionDiffJustFood / (float)getProductionNeeded();
	}

	return true;
}

NiColorA CvCity::getBarBackgroundColor() const
{
	if (atWar(getTeam(), GC.getGame().getActiveTeam()))
	{
		return NiColorA(0.5f, 0, 0, 0.5f); // red
	}
	return NiColorA(0, 0, 0, 0.5f);
}

bool CvCity::isStarCity() const
{
	return isCapital();
}


bool CvCity::isEventTriggerPossible(EventTriggerTypes eTrigger) const
{
	PROFILE_EXTRA_FUNC();
	FASSERT_BOUNDS(0, GC.getNumEventTriggerInfos(), eTrigger);

	const CvEventTriggerInfo& kTrigger = GC.getEventTriggerInfo(eTrigger);

	if (!CvString(kTrigger.getPythonCanDoCity()).empty())
	{
		if (!Cy::call<bool>(PYRandomEventModule, kTrigger.getPythonCanDoCity(), Cy::Args()
			<< eTrigger
			<< getOwner()
			<< getID()
			))
		{
			return false;
		}
	}

	if (kTrigger.getNumBuildings() > 0 && kTrigger.getNumBuildingsRequired() > 0)
	{
		bool bFoundValid = false;

		for (int i = 0; i < kTrigger.getNumBuildingsRequired(); ++i)
		{
			if (kTrigger.getBuildingRequired(i) != NO_BUILDING && isActiveBuilding((BuildingTypes)kTrigger.getBuildingRequired(i)))
			{
				bFoundValid = true;
			}
		}

		if (!bFoundValid)
		{
			return false;
		}
	}


	if (getReligionCount() < kTrigger.getNumReligions())
	{
		return false;
	}

	if (kTrigger.getNumReligions() > 0 && kTrigger.getNumReligionsRequired() > 0)
	{
		bool bFoundValid = false;

		for (int i = 0; i < kTrigger.getNumReligionsRequired(); ++i)
		{
			if (!kTrigger.isStateReligion() || kTrigger.getReligionRequired(i) == GET_PLAYER(getOwner()).getStateReligion())
			{
				if (isHasReligion((ReligionTypes)kTrigger.getReligionRequired(i)))
				{
					if (!kTrigger.isHolyCity() || isHolyCity((ReligionTypes)kTrigger.getReligionRequired(i)))
					{
						bFoundValid = true;
					}
				}
			}
		}

		if (!bFoundValid)
		{
			return false;
		}
	}

	if (getCorporationCount() < kTrigger.getNumCorporations())
	{
		return false;
	}

	if (kTrigger.getNumCorporations() > 0 && kTrigger.getNumCorporationsRequired() > 0)
	{
		bool bFoundValid = false;

		for (int i = 0; i < kTrigger.getNumCorporationsRequired(); ++i)
		{
			if (isHasCorporation((CorporationTypes)kTrigger.getCorporationRequired(i)))
			{
				if (!kTrigger.isHeadquarters() || isHeadquarters((CorporationTypes)kTrigger.getCorporationRequired(i)))
				{
					bFoundValid = true;
				}
			}
		}

		if (!bFoundValid)
		{
			return false;
		}
	}

	if (kTrigger.getMinPopulation() > 0)
	{
		if (getPopulation() < kTrigger.getMinPopulation())
		{
			return false;
		}
	}


	if (kTrigger.getMaxPopulation() > 0)
	{
		if (getPopulation() > kTrigger.getMaxPopulation())
		{
			return false;
		}
	}

	if (kTrigger.getAngry() > 0)
	{
		// The threshold is a whole-citizen count, which is what the net already answers in.
		if (-netHappiness() < kTrigger.getAngry())
		{
			return false;
		}
	}
	else if (kTrigger.getAngry() < 0)
	{
		if (netHappiness() < -kTrigger.getAngry())
		{
			return false;
		}
	}

	if (kTrigger.getUnhealthy() > 0)
	{
		// The threshold is a whole health-point count.
		if (-netHealth() < kTrigger.getUnhealthy())
		{
			return false;
		}
	}
	else if (kTrigger.getUnhealthy() < 0)
	{
		if (netHealth() < -kTrigger.getUnhealthy())
		{
			return false;
		}
	}

	if (kTrigger.isPrereqEventCity() && kTrigger.getNumPrereqEvents() > 0)
	{
		bool bFoundValid = true;

		for (int iI = 0; iI < kTrigger.getNumPrereqEvents(); ++iI)
		{
			if (!isEventOccured((EventTypes)kTrigger.getPrereqEvent(iI)))
			{
				bFoundValid = false;
				break;
			}
		}

		if (!bFoundValid)
		{
			return false;
		}
	}

	if (!((*getPropertiesConst()) >= *kTrigger.getPrereqMinProperties()))
		return false;

	if (!((*getPropertiesConst()) <= *kTrigger.getPrereqMaxProperties()))
		return false;

	if (0 == getFood() && kTrigger.getCityFoodWeight() > 0)
	{
		return false;
	}
	return true;
}

int CvCity::getTriggerValue(EventTriggerTypes eTrigger) const
{
	FASSERT_BOUNDS(0, GC.getNumEventTriggerInfos(), eTrigger);

	if (!isEventTriggerPossible(eTrigger))
	{
		return MIN_INT;
	}

	return getFood() * GC.getEventTriggerInfo(eTrigger).getCityFoodWeight();
}

bool CvCity::canApplyEvent(EventTypes eEvent, const EventTriggeredData& kTriggeredData) const
{
	PROFILE_EXTRA_FUNC();
	const CvEventInfo& kEvent = GC.getEventInfo(eEvent);

	if (!kEvent.isCityEffect() && !kEvent.isOtherPlayerCityEffect())
	{
		return true;
	}

	if (-1 == kTriggeredData.m_iCityId && kEvent.isCityEffect())
	{
		return false;
	}

	if (-1 == kTriggeredData.m_iOtherPlayerCityId && kEvent.isOtherPlayerCityEffect())
	{
		return false;
	}

	if (kEvent.getFood() + ((100 + kEvent.getFoodPercent()) * getFood()) / 100 < 0)
	{
		return false;
	}

	if (kEvent.getPopulationChange() + getPopulation() <= 0)
	{
		return false;
	}

	if (100 * kEvent.getCulture() + getCultureTimes100(getOwner()) < 0)
	{
		return false;
	}

	if (kEvent.getBuilding() != NO_BUILDING && kEvent.getBuildingChange() != 0)
	{
		if (kEvent.getBuildingChange() > 0)
		{
			if (hasBuilding((BuildingTypes)kEvent.getBuilding()))
			{
				return false;
			}
		}
		else if (!hasBuilding((BuildingTypes)kEvent.getBuilding()))
		{
			return false;
		}
	}

	if (-1 != kEvent.getMaxNumReligions() && getReligionCount() > kEvent.getMaxNumReligions())
	{
		return false;
	}

	if (kEvent.getMinPillage() > 0)
	{
		int iNumImprovements = 0;

		foreach_(const CvPlot* pPlot, plots(true))
		{
			if (pPlot->getOwner() == getOwner() && pPlot->isImprovementDestructible())
			{
				++iNumImprovements;
			}
		}
		if (iNumImprovements < kEvent.getMinPillage())
		{
			return false;
		}
	}
	return true;
}

void CvCity::applyEvent(EventTypes eEvent, const EventTriggeredData* pTriggeredData)
{
	PROFILE_EXTRA_FUNC();
	//	NULL pTriggeredData implies a replay after a reset of modifiers and only modifier effects
	//	should be applied
	bool	adjustModifiersOnly = (pTriggeredData == NULL);
	const EventTriggeredData& kTriggeredData = *pTriggeredData;

	if (!adjustModifiersOnly)
	{
		if (!canApplyEvent(eEvent, kTriggeredData))
		{
			return;
		}

		setEventOccured(eEvent, true);
	}

	const CvEventInfo& kEvent = GC.getEventInfo(eEvent);

	if (kEvent.isCityEffect() || kEvent.isOtherPlayerCityEffect())
	{
		if (kEvent.getHappy() != 0)
		{
			changeExtraHappiness(kEvent.getHappy());
		}

		if (kEvent.getHealth() != 0)
		{
			changeExtraHealth(kEvent.getHealth());
		}

		if (kEvent.getHurryAnger() != 0 && !adjustModifiersOnly)
		{
			changeHurryAngerTimer(kEvent.getHurryAnger() * flatHurryAngerLength());
		}

		if (kEvent.getHappyTurns() != 0 && !adjustModifiersOnly)
		{
			changeHappinessTimer(kEvent.getHappyTurns());
		}

		if (kEvent.getFood() != 0 || kEvent.getFoodPercent() != 0)
		{
			changeFood(kEvent.getFood() + (kEvent.getFoodPercent() * getFood()) / 100);
		}

		if (kEvent.getPopulationChange() != 0 && !adjustModifiersOnly)
		{
			changePopulation(kEvent.getPopulationChange());
		}

		if (kEvent.getRevoltTurns() > 0 && !adjustModifiersOnly)
		{
			changeCultureUpdateTimer(kEvent.getRevoltTurns());
			changeOccupationTimer(kEvent.getRevoltTurns());
		}

		if (kEvent.getMaxPillage() > 0 && !adjustModifiersOnly)
		{
			FAssert(kEvent.getMaxPillage() >= kEvent.getMinPillage());
			int iNumPillage = kEvent.getMinPillage() + GC.getGame().getSorenRandNum(kEvent.getMaxPillage() - kEvent.getMinPillage(), "Pick number of event pillaged plots");

			int iNumPillaged = 0;
			for (int i = 0; i < iNumPillage; ++i)
			{
				int iRandOffset = GC.getGame().getSorenRandNum(NUM_CITY_PLOTS, "Pick event pillage plot");
				for (int j = 0; j < NUM_CITY_PLOTS; ++j)
				{
					int iPlot = (j + iRandOffset) % NUM_CITY_PLOTS;
					if (CITY_HOME_PLOT != iPlot)
					{
						CvPlot* pPlot = getCityIndexPlot(iPlot);

						if (NULL != pPlot && pPlot->getOwner() == getOwner() && pPlot->isImprovementDestructible())
						{
							AddDLLMessage(
								getOwner(), false, GC.getEVENT_MESSAGE_TIME(),
								gDLL->getText("TXT_KEY_EVENT_CITY_IMPROVEMENT_DESTROYED", GC.getImprovementInfo(pPlot->getImprovementType()).getTextKeyWide()),
								"AS2D_PILLAGED", MESSAGE_TYPE_INFO, GC.getImprovementInfo(pPlot->getImprovementType()).getButton(), GC.getCOLOR_RED(), pPlot->getX(), pPlot->getY(), true, true
							);
							pPlot->setImprovementType(NO_IMPROVEMENT);
							++iNumPillaged;
							break;
						}
					}
				}
			}

			PlayerTypes eOtherPlayer = kTriggeredData.m_eOtherPlayer;
			if (!kEvent.isCityEffect() && kEvent.isOtherPlayerCityEffect())
			{
				eOtherPlayer = kTriggeredData.m_ePlayer;
			}

			if (NO_PLAYER != eOtherPlayer)
			{

				CvWString szBuffer = gDLL->getText("TXT_KEY_EVENT_NUM_CITY_IMPROVEMENTS_DESTROYED", iNumPillaged, GET_PLAYER(getOwner()).getCivilizationAdjectiveKey());
				AddDLLMessage(eOtherPlayer, false, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_PILLAGED", MESSAGE_TYPE_INFO);
			}
		}

		for (int i = 0; i < NUM_COMMERCE_TYPES; ++i)
		{
			if (kEvent.getCommerceModifier(i) != 0)
			{
				recordCommerceRateModifierGrant(eEvent, (CommerceTypes)i, kEvent.getCommerceModifier(i));
			}
		}

		for (int i = 0; i < NUM_YIELD_TYPES; ++i)
		{
			if (kEvent.getYieldModifier(i) != 0)
			{
				changeYieldRateModifier((YieldTypes)i, kEvent.getYieldModifier(i));
			}
		}

		for (int i = 0; i < GC.getNumSpecialistInfos(); ++i)
		{
			if (kEvent.getFreeSpecialistCount(i) > 0)
			{
				changeFreeSpecialistCount((SpecialistTypes)i, kEvent.getFreeSpecialistCount(i));
			}
		}

		if (kEvent.getCulture() != 0 && !adjustModifiersOnly)
		{
			changeCulture(getOwner(), kEvent.getCulture(), true, true);
		}

		if (kEvent.getRevolutionIndexChange() > 0 && !adjustModifiersOnly)
		{
			changeLocalRevIndex(kEvent.getRevolutionIndexChange());
		}
		else if (kEvent.getRevolutionIndexChange() < 0 && !adjustModifiersOnly)
		{
			changeLocalRevIndex(std::max(-getLocalRevIndex(), kEvent.getRevolutionIndexChange()));
		}
	}


	if (kEvent.getFreeUnit() != NO_UNIT && !adjustModifiersOnly)
	{
		UnitTypes eUnit = (UnitTypes) kEvent.getFreeUnit();
		for (int i = 0; i < kEvent.getNumUnits(); ++i)
		{
			GET_PLAYER(getOwner()).initUnit(eUnit, getX(), getY(), NO_UNITAI, NO_DIRECTION, GC.getGame().getSorenRandNum(10000, "AI Unit Birthmark"));
		}
	}

	const BuildingTypes eventBuilding = static_cast<BuildingTypes>(kEvent.getBuilding());
	if (eventBuilding != NO_BUILDING && !adjustModifiersOnly)
	{
		if (0 != kEvent.getBuildingChange())
		{
			changeHasBuilding(eventBuilding, kEvent.getBuildingChange() > 0);
		}
	}

	if (kEvent.getNumBuildingYieldChanges() > 0)
	{
		for (int iBuilding = 0; iBuilding < GC.getNumBuildingInfos(); ++iBuilding)
		{
			for (int iYield = 0; iYield < NUM_YIELD_TYPES; ++iYield)
			{
				setBuildingYieldChange((BuildingTypes)iBuilding, (YieldTypes)iYield, getBuildingYieldChange((BuildingTypes)iBuilding, (YieldTypes)iYield) + kEvent.getBuildingYieldChange(iBuilding, iYield));
			}
		}
	}

	foreach_(const BuildingCommerceChange& cc, kEvent.getBuildingCommerceChanges())
	{
		recordBuildingCommerceGrant(eEvent, cc.eBuilding, cc.eCommerce, cc.iChange);
	}

	if (kEvent.getNumBuildingHappyChanges() > 0)
	{
		for (int iBuilding = 0; iBuilding < GC.getNumBuildingInfos(); ++iBuilding)
		{
			setBuildingHappyChange((BuildingTypes)iBuilding, kEvent.getBuildingHappyChange(iBuilding));
		}
	}

	if (kEvent.getNumBuildingHealthChanges() > 0)
	{
		for (int iBuilding = 0; iBuilding < GC.getNumBuildingInfos(); ++iBuilding)
		{
			setBuildingHealthChange((BuildingTypes)iBuilding, kEvent.getBuildingHealthChange(iBuilding));
		}
	}

	getProperties()->addProperties(kEvent.getProperties());
	//GET_PLAYER(getOwner()).getProperties()->addProperties(kEvent.getProperties());
}

bool CvCity::isEventOccured(EventTypes eEvent) const
{
	return algo::any_of_equal(m_aEventsOccured, eEvent);
}

void CvCity::setEventOccured(EventTypes eEvent, bool bOccured)
{
	std::vector<EventTypes>::iterator itr = find(m_aEventsOccured.begin(), m_aEventsOccured.end(), eEvent);

	if (itr == m_aEventsOccured.end())
	{
		if (bOccured)
		{
			m_aEventsOccured.push_back(eEvent);
		}
	}
	else if (!bOccured)
	{
		m_aEventsOccured.erase(itr);
	}
}

// CACHE: cache frequently used values
///////////////////////////////////////
bool CvCity::hasShrine(ReligionTypes eReligion) const
{
	PROFILE_EXTRA_FUNC();
	// note, for normal XML, this count will be one, there is only one shrine of each religion
	foreach_(const BuildingTypes eBuilding, GC.getReligionInfo(eReligion).getShrineBuildings())
	{
		if (isActiveBuilding(eBuilding))
		{
			return true;
		}
	}
	return false;
}

bool CvCity::hasOrbitalInfrastructure() const
{
	PROFILE_EXTRA_FUNC();
	//ls612: To check if a city gets full benefits from Orbital Buildings
	for (int iI = 0; iI < GC.getNumBuildingInfos(); iI++)
	{
		if (GC.getBuildingInfo((BuildingTypes)iI).hasAttribute(CLS_ATTRIBUTE_ORBITAL_INFRASTRUCTURE) && isActiveBuilding((BuildingTypes)iI))
		{
			return true;
		}
	}
	return false;
}

void CvCity::invalidatePopulationRankCache()
{
	m_bPopulationRankValid = false;
}

void CvCity::invalidateYieldRankCache(YieldTypes eYield)
{
	PROFILE_EXTRA_FUNC();
	FASSERT_BOUNDS(NO_YIELD, NUM_YIELD_TYPES, eYield);

	if (eYield == NO_YIELD)
	{
		for (int iI = 0; iI < NUM_YIELD_TYPES; iI++)
		{
			m_abBaseYieldRankValid[iI] = false;
			m_abYieldRankValid[iI] = false;
		}
	}
	else
	{
		m_abBaseYieldRankValid[eYield] = false;
		m_abYieldRankValid[eYield] = false;
	}
}

void CvCity::invalidateCommerceRankCache(CommerceTypes eCommerce)
{
	PROFILE_EXTRA_FUNC();
	FASSERT_BOUNDS(NO_COMMERCE, NUM_COMMERCE_TYPES, eCommerce);

	if (eCommerce == NO_COMMERCE)
	{
		for (int iI = 0; iI < NUM_COMMERCE_TYPES; iI++)
		{
			m_abCommerceRankValid[iI] = false;
		}
	}
	else
	{
		m_abCommerceRankValid[eCommerce] = false;
	}
}

int CvCity::getBuildingYieldChange(BuildingTypes eBuilding, YieldTypes eYield) const
{
	PROFILE_EXTRA_FUNC();
	foreach_(const BuildingYieldChange& it, m_aBuildingYieldChange)
	{
		if (it.eBuilding == eBuilding && it.eYield == eYield)
		{
			return it.iChange;
		}
	}
	return 0;
}

void CvCity::setBuildingYieldChange(BuildingTypes eBuilding, YieldTypes eYield, int iChange)
{
	PROFILE_EXTRA_FUNC();
	int iCount = 0;
	bool bFound = false;
	const bool bErase = iChange == 0;
	foreach_(BuildingYieldChange& yieldChange, m_aBuildingYieldChange)
	{
		if (yieldChange.eBuilding == eBuilding && yieldChange.eYield == eYield)
		{
			bFound = true;
			const int iOldChange = yieldChange.iChange;
			if (iOldChange != iChange && !bErase)
			{
				yieldChange.iChange = iChange;
			}
			break;
		}
		iCount++;
	}

	if (bFound)
	{
		if (bErase)
		{
			m_aBuildingYieldChange.erase(m_aBuildingYieldChange.begin()+iCount);
		}
		return;
	}
	else if (bErase) return;

	// Cache new vector entry.
	BuildingYieldChange kChange;
	kChange.eBuilding = eBuilding;
	kChange.eYield = eYield;
	kChange.iChange = iChange;
	m_aBuildingYieldChange.push_back(kChange);
}

void CvCity::changeBuildingYieldChange(BuildingTypes eBuilding, YieldTypes eYield, int iChange)
{
	setBuildingYieldChange(eBuilding, eYield, getBuildingYieldChange(eBuilding, eYield) + iChange);
}

int CvCity::getBuildingCommerceChange(BuildingTypes eBuilding, CommerceTypes eCommerce) const
{
	PROFILE_EXTRA_FUNC();
	// The derivable half, plus what EVENTS granted -- the two are stored apart because only the first can be
	// recomputed from live sources; a rebuild would wipe the second if they shared a home.
	int iChange = m_eventGrants.sum(EVENTGRANT_BUILDING_COMMERCE, eCommerce, eBuilding);
	foreach_(const BuildingCommerceChange& it, m_aBuildingCommerceChange)
	{
		if (it.eBuilding == eBuilding && it.eCommerce == eCommerce)
		{
			iChange += it.iChange;
			break;
		}
	}
	return iChange;
}

void CvCity::setBuildingCommerceChange(BuildingTypes eBuilding, CommerceTypes eCommerce, int iChange)
{
	PROFILE_EXTRA_FUNC();
	int iCount = 0;
	bool bFound = false;
	const bool bErase = iChange == 0;
	foreach_(BuildingCommerceChange& commerceChange, m_aBuildingCommerceChange)
	{
		if (commerceChange.eBuilding == eBuilding && commerceChange.eCommerce == eCommerce)
		{
			bFound = true;
			if (commerceChange.iChange != iChange)
			{
				if (!bErase)
				{
					commerceChange.iChange = iChange;
				}
			}
			break;
		}
		iCount++;
	}

	if (bFound)
	{
		if (bErase)
		{
			m_aBuildingCommerceChange.erase(m_aBuildingCommerceChange.begin()+iCount);
		}
		return;
	}
	else if (bErase) return;

	// Cache new vector entry.
	BuildingCommerceChange kChange;
	kChange.eBuilding = eBuilding;
	kChange.eCommerce = eCommerce;
	kChange.iChange = iChange;
	m_aBuildingCommerceChange.push_back(kChange);

}

void CvCity::changeBuildingCommerceChange(BuildingTypes eBuilding, CommerceTypes eCommerce, int iChange)
{
	setBuildingCommerceChange(eBuilding, eCommerce, getBuildingCommerceChange(eBuilding, eCommerce) + iChange);
}


void CvCity::recordBuildingCommerceGrant(EventTypes eEvent, BuildingTypes eBuilding, CommerceTypes eCommerce, int iChange)
{
	if (iChange != 0)
	{
		m_eventGrants.add(EVENTGRANT_BUILDING_COMMERCE, eEvent, eCommerce, eBuilding, iChange);
	}
}


void CvCity::setBuildingHappyChange(BuildingTypes eBuilding, int iChange)
{
	PROFILE_EXTRA_FUNC();
	for (BuildingChangeArray::iterator it = m_aBuildingHappyChange.begin(); it != m_aBuildingHappyChange.end(); ++it)
	{
		if ((*it).first == eBuilding)
		{
			if ((*it).second != iChange)
			{
				const int iOldChange = (*it).second;

				m_aBuildingHappyChange.erase(it);

				if (iChange != 0 && hasFullyActiveBuilding(eBuilding))
				{
					m_aBuildingHappyChange.push_back(std::make_pair(eBuilding, iChange));
				}
			}
			return;
		}
	}

	if (0 != iChange && hasFullyActiveBuilding(eBuilding))
	{
		m_aBuildingHappyChange.push_back(std::make_pair(eBuilding, iChange));
	}
}


int CvCity::getBuildingHappyChange(BuildingTypes eBuilding) const
{
	PROFILE_EXTRA_FUNC();
	for (BuildingChangeArray::const_iterator it = m_aBuildingHappyChange.begin(); it != m_aBuildingHappyChange.end(); ++it)
	{
		if ((*it).first == eBuilding)
		{
			return (*it).second;
		}
	}
	return 0;
}


void CvCity::setBuildingHealthChange(BuildingTypes eBuilding, int iChange)
{
	PROFILE_EXTRA_FUNC();
	for (BuildingChangeArray::iterator it = m_aBuildingHealthChange.begin(); it != m_aBuildingHealthChange.end(); ++it)
	{
		if ((*it).first == eBuilding)
		{
			if ((*it).second != iChange)
			{
				const int iOldChange = (*it).second;

				m_aBuildingHealthChange.erase(it);

				if (iChange != 0 && hasFullyActiveBuilding(eBuilding))
				{
					m_aBuildingHealthChange.push_back(std::make_pair(eBuilding, iChange));
				}
			}
			return;
		}
	}

	if (0 != iChange && hasFullyActiveBuilding(eBuilding))
	{
		m_aBuildingHealthChange.push_back(std::make_pair(eBuilding, iChange));
	}
}


int CvCity::getBuildingHealthChange(BuildingTypes eBuilding) const
{
	PROFILE_EXTRA_FUNC();
	for (BuildingChangeArray::const_iterator it = m_aBuildingHealthChange.begin(); it != m_aBuildingHealthChange.end(); ++it)
	{
		if ((*it).first == eBuilding)
		{
			return (*it).second;
		}
	}

	return 0;
}

void CvCity::liberate(bool bConquest)
{
	PROFILE_EXTRA_FUNC();
	const PlayerTypes ePlayer = getLiberationPlayer(bConquest);

	if (NO_PLAYER == ePlayer) return;

	const PlayerTypes eOwner = getOwner();

	int iOldMasterLand = 0;
	int iOldVassalLand = 0;
	if (GET_TEAM(GET_PLAYER(ePlayer).getTeam()).isVassal(GET_PLAYER(eOwner).getTeam()))
	{
		iOldMasterLand = GET_TEAM(GET_PLAYER(eOwner).getTeam()).getTotalLand();
		iOldVassalLand = GET_TEAM(GET_PLAYER(ePlayer).getTeam()).getTotalLand(false);
	}

	const CvWString szBuffer = gDLL->getText("TXT_KEY_MISC_CITY_LIBERATED", getNameKey(), GET_PLAYER(eOwner).getNameKey(), GET_PLAYER(ePlayer).getCivilizationAdjectiveKey());
	for (int iI = 0; iI < MAX_PC_PLAYERS; iI++)
	{
		if (GET_PLAYER((PlayerTypes)iI).isAlive() && GET_PLAYER((PlayerTypes)iI).isHumanPlayer() && isRevealed(GET_PLAYER((PlayerTypes)iI).getTeam(), false))
		{
			AddDLLMessage(
				(PlayerTypes)iI, false, GC.getEVENT_MESSAGE_TIME(),
				szBuffer, "AS2D_REVOLTEND", MESSAGE_TYPE_MAJOR_EVENT,
				ARTFILEMGR.getInterfaceArtInfo("WORLDBUILDER_CITY_EDIT")->getPath(),
				GC.getCOLOR_HIGHLIGHT_TEXT(), getX(), getY(), true, true
			);
		}
	}
	GC.getGame().addReplayMessage(REPLAY_MESSAGE_MAJOR_EVENT, eOwner, szBuffer, getX(), getY(), GC.getCOLOR_HIGHLIGHT_TEXT());

	const int64_t iCulture = getCultureTimes100(eOwner);
	const CvPlot* cityPlot = plot();
	GET_PLAYER(ePlayer).acquireCity(this, false, true, true); // Invalidates this city object. this::kill() is called.
	GET_PLAYER(ePlayer).AI_changeMemoryCount(eOwner, MEMORY_LIBERATED_CITIES, 1);

	if (GET_TEAM(GET_PLAYER(ePlayer).getTeam()).isVassal(GET_PLAYER(eOwner).getTeam()))
	{
		int iNewMasterLand = GET_TEAM(GET_PLAYER(eOwner).getTeam()).getTotalLand();
		int iNewVassalLand = GET_TEAM(GET_PLAYER(ePlayer).getTeam()).getTotalLand(false);

		GET_TEAM(GET_PLAYER(ePlayer).getTeam()).setMasterPower(GET_TEAM(GET_PLAYER(ePlayer).getTeam()).getMasterPower() + iNewMasterLand - iOldMasterLand);
		GET_TEAM(GET_PLAYER(ePlayer).getTeam()).setVassalPower(GET_TEAM(GET_PLAYER(ePlayer).getTeam()).getVassalPower() + iNewVassalLand - iOldVassalLand);
	}
	CvCity* pCity = cityPlot->getPlotCity();

	FAssertMsg(NULL != pCity, "Not expected!")

	pCity->setCultureTimes100(ePlayer, pCity->getCultureTimes100(ePlayer) + iCulture / 2, true, true);

	if (GET_TEAM(GET_PLAYER(ePlayer).getTeam()).isAVassal())
	{
		for (int i = 0; i < GC.getCOLONY_NUM_FREE_DEFENDERS(); ++i)
		{
			pCity->initConscriptedUnit();
		}
	}
}

PlayerTypes CvCity::getLiberationPlayer(bool bConquest) const
{
	PROFILE_EXTRA_FUNC();
	if (isCapital())
	{
		return NO_PLAYER;
	}

	for (int iI = 0; iI < MAX_PC_PLAYERS; ++iI)
	{
		const CvPlayer& kLoopPlayer = GET_PLAYER((PlayerTypes)iI);
		if (kLoopPlayer.isAlive() && kLoopPlayer.getParent() == getOwner())
		{
			const CvCity* pLoopCapital = kLoopPlayer.getCapitalCity();
			if (NULL != pLoopCapital && pLoopCapital->area() == area())
			{
				return (PlayerTypes)iI;
			}
		}
	}

	const CvPlayer& kOwner = GET_PLAYER(getOwner());
	if (kOwner.canSplitEmpire() && kOwner.canSplitArea(area()->getID()))
	{
		const PlayerTypes ePlayer = kOwner.getSplitEmpirePlayer(area()->getID());

		if (NO_PLAYER != ePlayer && GET_PLAYER(ePlayer).isAlive())
		{
			return ePlayer;
		}
	}

	const int64_t iTotalCultureTimes100 = countTotalCultureTimes100();
	PlayerTypes eBestPlayer = NO_PLAYER;
	int64_t iBestValue = 0;

	for (int iI = 0; iI < MAX_PC_PLAYERS; ++iI)
	{
		const CvPlayer& playerX = GET_PLAYER((PlayerTypes)iI);

		if (playerX.isAlive())
		{
			const CvCity* pCapital = playerX.getCapitalCity();
			if (NULL != pCapital)
			{
				int iCapitalDistance = ::plotDistance(getX(), getY(), pCapital->getX(), pCapital->getY());
				if (area() != pCapital->area())
				{
					iCapitalDistance *= 2;
				}

				int64_t iCultureTimes100 = getCultureTimes100((PlayerTypes)iI);

				if (bConquest && iI == getOriginalOwner())
				{
					iCultureTimes100 *= 3;
					iCultureTimes100 /= 2;
				}

				if (playerX.getTeam() == getTeam()
				|| GET_TEAM(playerX.getTeam()).isVassal(getTeam())
				|| GET_TEAM(getTeam()).isVassal(playerX.getTeam()))
				{
					iCultureTimes100 *= 2;
					iCultureTimes100 = (iCultureTimes100 + iTotalCultureTimes100) / 2;
				}

				const int64_t iValue = std::max<int64_t>(100, iCultureTimes100) / std::max(1, iCapitalDistance);

				if (iValue > iBestValue)
				{
					iBestValue = iValue;
					eBestPlayer = (PlayerTypes)iI;
				}
			}
		}
	}

	if (NO_PLAYER != eBestPlayer)
	{
		if (getOwner() == eBestPlayer)
		{
			return NO_PLAYER;
		}
		for (int iPlot = 0; iPlot < getNumCityPlots(); ++iPlot)
		{
			CvPlot* pLoopPlot = ::plotCity(getX(), getY(), iPlot);

			if (NULL != pLoopPlot && pLoopPlot->isVisibleEnemyUnit(eBestPlayer))
			{
				return NO_PLAYER;
			}
		}
	}
	return eBestPlayer;
}

int CvCity::getBestYieldAvailable(YieldTypes eYield) const
{
	PROFILE_EXTRA_FUNC();
	int iBestYieldAvailable = 0;

	for (int iJ = SKIP_CITY_HOME_PLOT; iJ < NUM_CITY_PLOTS; ++iJ)
	{
		if (!isWorkingPlot(iJ))
		{
			const CvPlot* pPlot = getCityIndexPlot(iJ);

			if (NULL != pPlot && canWork(pPlot))
			{
				int aiPlotYields100[NUM_YIELD_TYPES];
				pPlot->getYields(aiPlotYields100);   // ×100 group read (getYield is the EXE edge)
				if (aiPlotYields100[eYield] > iBestYieldAvailable)
				{
					iBestYieldAvailable = aiPlotYields100[eYield];
				}
			}
		}
	}

	for (int iJ = 0; iJ < GC.getNumSpecialistInfos(); ++iJ)
	{
		if (isSpecialistValid((SpecialistTypes)iJ, 1))
		{
			const int iYield = GC.getSpecialistInfo((SpecialistTypes)iJ).getFlatYield(eYield, CASC_SCOPE_CITY);
			if (iYield > iBestYieldAvailable)
			{
				iBestYieldAvailable = iYield;
			}
		}
	}

	return iBestYieldAvailable;
}

int CvCity::getMusicScriptId() const
{
	bool bIsHappy = true;
	if (GC.getGame().getActiveTeam() == getTeam())
	{
		if (angryPopulation() > 0)
		{
			bIsHappy = false;
		}
	}
	else
	{
		if (GET_TEAM(GC.getGame().getActiveTeam()).isAtWar(getTeam()))
		{
			bIsHappy = false;
		}
	}

	const CvLeaderHeadInfo& kLeaderInfo = GC.getLeaderHeadInfo(GET_PLAYER(getOwner()).getLeaderType());
	const EraTypes eCurEra = GET_PLAYER(getOwner()).getCurrentEra();
	if (bIsHappy)
	{
		return (kLeaderInfo.getDiploMusicScriptId(DIPLO_MUSIC_PEACE, eCurEra));
	}
	else
	{
		return (kLeaderInfo.getDiploMusicScriptId(DIPLO_MUSIC_WAR, eCurEra));
	}
}

int CvCity::getSoundscapeScriptId() const
{
	return GC.getEraInfo(GET_PLAYER(getOwner()).getCurrentEra()).getCitySoundscapeScriptId(getCitySizeType());
}

void CvCity::cheat(bool bCtrl, bool bAlt, bool bShift)
{
	if (gDLL->getChtLvl() > 0 || GC.getGame().isDebugMode())
	{
		if (bCtrl)
		{
			changeCulture(getOwner(), CvGameSpeedScale::speedPercent(), true, true);
		}
		else if (bShift)
		{
			changePopulation(1);
		}
		else
		{
			popOrder(0, true);
		}
	}
}

void CvCity::getBuildQueue(std::vector<std::string>& astrQueue) const
{
	PROFILE_EXTRA_FUNC();
	foreach_(const OrderData& order, m_orderQueue)
	{
		switch (order.eOrderType)
		{
		case ORDER_TRAIN:
			astrQueue.push_back(GC.getUnitInfo(order.getUnitType()).getType());
			break;

		case ORDER_CONSTRUCT:
			astrQueue.push_back(GC.getBuildingInfo(order.getBuildingType()).getType());
			break;

		case ORDER_CREATE:
			astrQueue.push_back(GC.getProjectInfo(order.getProjectType()).getType());
			break;

		case ORDER_MAINTAIN:
			astrQueue.push_back(GC.getProcessInfo(order.getProcessType()).getType());
			break;

		case ORDER_LIST:
			astrQueue.push_back("List");
			break;

		default:
			FErrorMsg(CvString::format("Unexpected eOrderType %d", order.eOrderType).c_str());
			break;
		}
	}
}

/************************************************************************************************/
/* INFLUENCE_DRIVEN_WAR                   04/16/09                                johnysmith    */
/*                                                                                              */
/* Original Author Moctezuma              Start                                                 */
/************************************************************************************************/
// ------ BEGIN InfluenceDrivenWar -------------------------------
void CvCity::emergencyConscript()
{
	if (getConscriptUnit() == NO_UNIT)
	{
		return;
	}
	if (getConscriptAngerTimer() > 3 * flatConscriptAngerLength() * GC.getIDW_EMERGENCY_DRAFT_ANGER_MULTIPLIER() / 100)
	{
		return;
	}
	changeConscriptAngerTimer(flatConscriptAngerLength() * GC.getIDW_EMERGENCY_DRAFT_ANGER_MULTIPLIER() / 100);
	changePopulation(-1);

	const UnitTypes eConscriptUnit = getConscriptUnit();
	UnitAITypes eCityAI;

	if (GET_PLAYER(getOwner()).AI_unitValue(eConscriptUnit, UNITAI_CITY_DEFENSE, area()) > 0)
	{
		eCityAI = UNITAI_CITY_DEFENSE;
	}
	else if (GET_PLAYER(getOwner()).AI_unitValue(eConscriptUnit, UNITAI_CITY_COUNTER, area()) > 0)
	{
		eCityAI = UNITAI_CITY_COUNTER;
	}
	else if (GET_PLAYER(getOwner()).AI_unitValue(eConscriptUnit, UNITAI_CITY_SPECIAL, area()) > 0)
	{
		eCityAI = UNITAI_CITY_SPECIAL;
	}
	else
	{
		eCityAI = NO_UNITAI;
	}

	CvUnit* pUnit = GET_PLAYER(getOwner()).initUnit(eConscriptUnit, getX(), getY(), eCityAI, NO_DIRECTION, GC.getGame().getSorenRandNum(10000, "AI Unit Birthmark"));
	if (pUnit == NULL)
	{
		FErrorMsg("pUnit is expected to be assigned a valid unit object");
		return;
	}
	addProductionExperience(pUnit, true);
	pUnit->setMoves(0);
	pUnit->setDamage((100 - GC.getIDW_EMERGENCY_DRAFT_STRENGTH()) * pUnit->getMaxHP() / 100, getOwner());
}
// ------ END InfluenceDrivenWar ---------------------------------


int CvCity::getRevTrend() const
{
	if (!GC.getGame().isOption(GAMEOPTION_UNSUPPORTED_REVOLUTION))
		return 0;

	//This is the value from python
	int iRevInsigatorThreshold = 1000;
	int iRevIndex = std::min(getRevolutionIndex(), iRevInsigatorThreshold);
	int iDeltaTrend = iRevIndex - getRevIndexAverage();
	if (iDeltaTrend != 0)
		iDeltaTrend *= std::max(abs(iDeltaTrend), 1 + iRevInsigatorThreshold / 100);
	return iDeltaTrend;
}

bool CvCity::isInquisitionConditions() const
{
	PROFILE_EXTRA_FUNC();
	const ReligionTypes eStateReligion = GET_PLAYER(getOwner()).getStateReligion();

	if (eStateReligion == NO_RELIGION)
	{
		return false;
	}
	if (isHasReligion(eStateReligion))
	{
		for (int iI = 0; iI < GC.getNumReligionInfos(); iI++)
		{
			if (isHasReligion(ReligionTypes(iI)) && ReligionTypes(iI) != eStateReligion
			&& (!isHolyCity(ReligionTypes(iI)) || isHolyCity(ReligionTypes(iI)) && GC.isOC_RESPAWN_HOLY_CITIES()))
			{
				return true;
			}
		}
	}
	return false;
}

/*
Checks the cities culture level and if it meets the criteria specified in the CultureLevelsInfo.xml, the city radius expands.
NUM_CITY_PLOTS is the largest city size, a radius of 3, NUM_CITY_PLOTS_2 is the standard BTS city size ( a radius of 2), and NUM_CITY_PLOTS_3
is a city size of 1.
*/
int CvCity::getNumCityPlots() const
{
	return getNumCityPlotsAtCultureLevel((int)getCultureLevel());
}

// THE WHOLE MAINTENANCE SURFACE of the plots' `workableBy` membership. The city-plot addressing is a FIXED
// ring-ordered table, so a radius is a PREFIX of it and a change is exactly the index range between the two
// counts -- nothing geometric is rebuilt, and the direction falls out of which count is larger.
void CvCity::changeWorkableArea(int iOldNumCityPlots, int iNewNumCityPlots) const
{
	const bool bGrowing = (iNewNumCityPlots > iOldNumCityPlots);
	const int iFrom = bGrowing ? iOldNumCityPlots : iNewNumCityPlots;
	const int iTo = bGrowing ? iNewNumCityPlots : iOldNumCityPlots;
	for (int iRingIndex = iFrom; iRingIndex < iTo; ++iRingIndex)
	{
		CvPlot* pPlot = getCityIndexPlot(iRingIndex);
		if (pPlot != NULL)
		{
			pPlot->setWorkableBy(this, bGrowing);
		}
	}
}

// The SAME derivation, parameterized by the culture level rather than reading the city's current one -- so the
// work area a level the city NO LONGER HOLDS defined is still answerable. That is what lets a level change apply
// as a RING DELTA (the plots between the two counts) instead of a re-derivation: the city-plot addressing is a
// FIXED ring-ordered table (index 0 = the city, 1-8 = ring 1, 9-20 = ring 2, 21-36 = ring 3), so a radius IS a
// prefix of it and a growth is exactly the indices [oldCount, newCount).
// ⛔ ONE implementation ([DEC-single-implementation]) -- getNumCityPlots() delegates here rather than the two
// carrying a copy of the override/level ladder each.
int CvCity::getNumCityPlotsAtCultureLevel(int iCultureLevel) const
{
	if (getWorkableRadiusOverride() == 0 && !GC.getGame().isOption(GAMEOPTION_EXP_LARGER_CITIES))
	{
		return NUM_CITY_PLOTS_2;
	}
	if (iCultureLevel == -1)
	{
		return NUM_CITY_PLOTS_1;
	}
	const int iRadius =
	(
		getWorkableRadiusOverride() > 0
		?
		getWorkableRadiusOverride()
		:
		GC.getCultureLevelInfo((CultureLevelTypes)iCultureLevel).getCityRadius()
	);
	switch (iRadius)
	{
		case 3:
		{
			return NUM_CITY_PLOTS;
		}
		case 2:
		{
			return NUM_CITY_PLOTS_2;
		}
		case 1:
		{
			return NUM_CITY_PLOTS_1;
		}
		default:
		{
			return NUM_CITY_PLOTS_2;
		}
	}
}

/*
 updateYieldRate(...) checks to see if the building given is in the city, and if it is, updates the yield by the iChange amount.
*/
void CvCity::updateYieldRate(BuildingTypes eBuilding, YieldTypes eYield, int iChange)
{
	setBuildingYieldChange(eBuilding, eYield, iChange);
}


/*
	Checks to see if the city is producing a wonder
*/
bool CvCity::isProductionWonder() const
{
	bst::optional<OrderData> headOrder = getHeadOrder();

	if (headOrder && headOrder->eOrderType == ORDER_CONSTRUCT)
	{
		return isLimitedWonder(headOrder->getBuildingType());
	}
	return false;
}

void CvCity::clearLostProduction()
{
	m_iLostProductionModified = 0;
	m_iGoldFromLostProduction = 0;
}

int CvCity::getLandmarkAngerTimer() const
{
	return m_iLandmarkAngerTimer;
}

void CvCity::changeLandmarkAngerTimer(int iChange)
{
	if (iChange != 0)
	{
		m_iLandmarkAngerTimer += iChange;
		AI_setAssignWorkDirty(true);

		if (getTeam() == GC.getGame().getActiveTeam())
		{
			setInfoDirty(true);
		}
	}
}

int CvCity::getLandmarkAnger() const
{
	int iAnger = 0;
	if (getLandmarkAngerTimer() > 0)
	{
		iAnger++;
	}
	int iDivisor = std::max(1, GC.getLANDMARK_ANGER_DIVISOR());
	iDivisor *= CvGameSpeedScale::speedPercent();
	iDivisor /= 100;

	iAnger += getLandmarkAngerTimer() / std::max(1, iDivisor);

	return iAnger;
}


bool CvCity::isBuiltFoodProducedUnit() const
{
	return m_bBuiltFoodProducedUnit;
}

void CvCity::setBuiltFoodProducedUnit(bool bNewValue)
{
	m_bBuiltFoodProducedUnit = bNewValue;
}






// The city's population-growth-rate percent, read as the ordinary cascade channel it is
// (populationGrowthRate.city -- the SCALAR_POPULATION_GROWTH_RATE straggler slot). What this replaces was a
// FLOAT log-space accumulator rebuilt by a blanket walk over every building in the game each turn: a float on a
// deterministic-lockstep engine (an OOS hazard in its own right), a hand-named scalar that no derived mask could
// address ([DEC-uniform-cache-shape]), and a self-heal ([DEC-no-self-heal]) all at once.
// ⚠ BEHAVIOUR CHANGE, stated rather than hidden: the legacy form composed the sources MULTIPLICATIVELY through
// the log. Percents are ADDITIVE deltas that sum and apply once ([modifier.md §2]), so the sources now add. A
// percent is not ×100 scaled ([DEC-fixedpoint-x100]), so this needs no reduction.
int CvCity::getPopulationgrowthratepercentage() const
{
	ModifierFamily eFamily = MODFAM_NONE;
	int iKind = -1;
	infoScalarSlot(SCALAR_POPULATION_GROWTH_RATE, eFamily, iKind);
	return InfoValuation::realizedAtCity(*this, CascadeChannelRegistry::channelLookup(eFamily, iKind, -1));
}




// The provider-BUILDING-fed access verdict, read straight off the amenity refcount that holds it -- the same
// shape as isPowered/isGovernmentCenter. ⚠ DISTINCT from the plot's adjacency HAS_FRESHWATER fact: a building can
// grant a city access on a plot with no water at all.
bool CvCity::hasFreshWater() const
{
	return m_cityContext.amenityCount(CLS_AMENITY_PROVIDES_FRESH_WATER) > 0;
}

// The derived state the access verdict feeds. Called from the crossing announcement, so it runs once per genuine
// 0 <-> non-zero move and never on a second provider arriving.
void CvCity::refreshFreshWaterDerived()
{
	algo::for_each(plots(), bind(CvPlot::updateIrrigated, _1));

	updateFreshWaterHealth();
}


bool CvCity::canUpgradeUnit(UnitTypes eUnit) const
{
	PROFILE_FUNC();

	foreach_(const int iUpgrade, GC.getUnitInfo(eUnit).getUpgradesTo())
	{
		const UnitTypes eUpgradeUnit = (UnitTypes)iUpgrade;

		if (GC.getGame().isUnitMaxedOut(eUpgradeUnit) || GET_PLAYER(getOwner()).isUnitMaxedOut(eUpgradeUnit))
		{ // if the upgrade unit is maxed out, I assume you can construct them, and already have constructed the max
			return true;
		}
	}
	return false;
}

void CvCity::setCivilizationType(int iCiv)
{
	m_iCiv = iCiv;
}

int CvCity::getAdditionalDefenseByBuilding(BuildingTypes eBuilding) const
{
	PROFILE_EXTRA_FUNC();
	FASSERT_BOUNDS(0, GC.getNumBuildingInfos(), eBuilding);

	int iExtraRate = 0;
	int iExtraBuildingRate = 0;

	const CvBuildingInfo& kBuilding = GC.getBuildingInfo(eBuilding);

	if (kBuilding.getDefense(DEFENSE_AMOUNT, CASC_SCOPE_CITY) != 0)
	{
		//iExtraRate += std::max(0, kBuilding.getDefense(DEFENSE_AMOUNT, CASC_SCOPE_CITY) - std::max(0, iCultureDefense));
		iExtraBuildingRate += kBuilding.getDefense(DEFENSE_AMOUNT, CASC_SCOPE_CITY);
	}
	// The bonus-keyed defense rows, read as the entity's OWN authored entries rather than by asking every bonus
	// in the registry whether this building deposits onto it -- the own-data inversion ([modifier.md §5]).
	{
		std::vector<std::pair<int, int> > bonusDefense;
		InfoValuation::collectKeyedTarget(kBuilding.getModifiers(), MODFAM_DEFENSE, DEFENSE_AMOUNT,
			InfoValuation::keyedTargetSegment("bonuses"), bonusDefense);
		for (std::vector<std::pair<int, int> >::const_iterator it = bonusDefense.begin(); it != bonusDefense.end(); ++it)
		{
			if (hasBonus(static_cast<BonusTypes>(it->first)))
			{
				iExtraRate += it->second;
			}
		}
	}
	if (kBuilding.getDefense(DEFENSE_AMOUNT, CASC_SCOPE_EMPIRE) != 0)
	{
		iExtraRate += kBuilding.getDefense(DEFENSE_AMOUNT, CASC_SCOPE_EMPIRE);
	}

	// If this new building replaces an old one, subtract the old defense rate from the new one.
	std::vector<int> supersededBuildings;
	EnablerKernel::supersededBy(EDGEB_BUILDINGS, (int)eBuilding, supersededBuildings);
	for (size_t iI = 0; iI < supersededBuildings.size(); iI++)
	{
		const BuildingTypes eBuildingX = static_cast<BuildingTypes>(supersededBuildings[iI]);

		if (isActiveBuilding(eBuildingX))
		{
			const CvBuildingInfo& info = GC.getBuildingInfo(eBuildingX);

			iExtraBuildingRate -= info.getDefense(DEFENSE_AMOUNT, CASC_SCOPE_CITY);
			std::vector<std::pair<int, int> > replacedBonusDefense;
			InfoValuation::collectKeyedTarget(info.getModifiers(), MODFAM_DEFENSE, DEFENSE_AMOUNT,
				InfoValuation::keyedTargetSegment("bonuses"), replacedBonusDefense);
			for (std::vector<std::pair<int, int> >::const_iterator it = replacedBonusDefense.begin(); it != replacedBonusDefense.end(); ++it)
			{
				if (hasBonus(static_cast<BonusTypes>(it->first)))
				{
					iExtraRate -= it->second;
				}
			}
			iExtraRate -= info.getDefense(DEFENSE_AMOUNT, CASC_SCOPE_EMPIRE);
		}
	}

	// With ONE additive defense stack there is no building-vs-natural max to straddle, so the effective delta of
	// adding iExtraBuildingRate IS iExtraBuildingRate -- the old form computed
	// max(b+extra, natural) - max(b, natural) purely to model that vanished max.
	return iExtraRate + iExtraBuildingRate;
}





int CvCity::sight() const
{
	// A city is an OBSERVER like any other (vision.md): base sight STRENGTH plus ELEVATION -- a settlement
	// stands tall by construction (CITY_BASE_ELEVATION), and its buildings raise it further (the cascade
	// channel: a tree platform puts the lookout a storey up). Both defines are authored in PLOTS and lifted to
	// the vision scale here (the MAX_UNIT_VISIBILITY_RANGE shape); the ring a city keeps eyes on is bought by
	// this budget, never by a bypass of the walk (owner: no guaranteed ring -- model it as strength + elevation).
	int aVisions[NUM_VISION_KINDS];
	getVisionKinds(aVisions);
	return aVisions[VISION_STRENGTH] + aVisions[VISION_ELEVATION]
		+ (GC.getCITY_VISIBILITY_RANGE() + GC.getCITY_BASE_ELEVATION()) * VISION_OPEN_GROUND_COST;
}

void CvCity::getVisionKinds(int (&visions)[NUM_VISION_KINDS]) const
{
	// The city's sight (vision.md), read like every other group: its buildings raise VISION_ELEVATION, and the
	// packages those deposits feed are built by the spine -- the load reseed once, then each BUILDING_CHANGED
	// fact -- exactly as every other channel is. No accumulator, no serialization, nothing to drift.
	for (int iKind = 0; iKind < NUM_VISION_KINDS; ++iKind)
	{
		const int iChannel = CascadeChannelRegistry::channelLookup(MODFAM_VISION, iKind, -1);
		visions[iKind] = InfoValuation::realizedAtCity(*this, iChannel);
	}
}


BuildTypes CvCity::findChopBuild(FeatureTypes eFeature) const
{
	PROFILE_EXTRA_FUNC();
	for (int iI = 0; iI < GC.getNumBuildInfos(); iI++)
	{
		const CvBuildInfo& kBuild = GC.getBuildInfo((BuildTypes)iI);
		if (kBuild.getImprovement() == NO_IMPROVEMENT)
		{
			if (kBuild.isFeatureRemove(eFeature) &&
				kBuild.getFeatureProduction(eFeature) != 0 &&
				GET_TEAM(getTeam()).isHasTech(kBuild.getTechPrereq()))
			{
				return (BuildTypes)iI;
			}
		}
	}

	return NO_BUILD;
}


static bool bonusAvailableFromBuildings(BonusTypes eBonus)
{
	PROFILE_EXTRA_FUNC();
	static bool* bBonusAvailability = NULL;

	FASSERT_BOUNDS(0, GC.getNumBonusInfos(), eBonus);

	if (bBonusAvailability == NULL)
	{
		CvXMLLoadUtility::InitList<bool>(&bBonusAvailability, GC.getNumBonusInfos(), false);

		for (int iI = 0; iI < GC.getNumBonusInfos(); iI++)
		{
			foreach_(const CvBuildingInfo* pBuilding, GC.getBuildingInfos())
			{
				if (pBuilding->getProvides()->has(iI))
				{
					bBonusAvailability[iI] = true;
					break;
				}
			}
		}
	}

	return bBonusAvailability[eBonus];
}



bool CvCity::isDevelopingCity() const
{
	return
	(
		getPopulation() < 3 && !isCapital()
		|| // Pop is less than half your average city pop value.
		getPopulation() < GET_PLAYER(getOwner()).getTotalPopulation() / (2*GET_PLAYER(getOwner()).getNumCities())
	);
}


int CvCity::getUnitCombatExtraStrength(UnitCombatTypes eIndex) const
{
	FASSERT_BOUNDS(0, GC.getNumUnitCombatInfos(), eIndex);
	return m_paiUnitCombatExtraStrength[eIndex];
}


void CvCity::changeUnitCombatExtraStrength(UnitCombatTypes eIndex, int iChange)
{
	FASSERT_BOUNDS(0, GC.getNumUnitCombatInfos(), eIndex);
	m_paiUnitCombatExtraStrength[eIndex] += iChange;
}

bool CvCity::isZoneOfControl() const
{
	return (m_iZoCCount > 0);
}

void CvCity::changeZoCCount(short iChange)
{
	if (iChange != 0)
	{
		m_iZoCCount = std::max(0, (m_iZoCCount + iChange));
	}
}

int CvCity::getAdjacentDamagePercent() const { return cascadeDefense(DEFENSE_ADJACENT_DAMAGE); }

int CvCity::getWorkableRadiusOverride() const
{
	return m_iWorkableRadiusOverride;
}

void CvCity::setWorkableRadiusOverride(int iNewVal)
{
	m_iWorkableRadiusOverride = iNewVal;
}

bool CvCity::isProtectedCulture() const
{
	return getProtectedCultureCount() > 0;
}

int CvCity::getProtectedCultureCount() const
{
	return m_iProtectedCultureCount;
}

void CvCity::changeProtectedCultureCount(int iChange)
{
	m_iProtectedCultureCount += iChange;
}

void CvCity::doAttack()
{
	PROFILE_FUNC();

	if (getAdjacentDamagePercent() > 0)
	{
		if (GET_TEAM(getTeam()).isAtWar(true))
		{
			bool abInformPlayer[MAX_PLAYERS];
			for (int iI = 0; iI < MAX_PLAYERS; iI++)
			{
				abInformPlayer[iI] = false;
			}
			foreach_(const CvPlot* pAdjacentPlot, plot()->adjacent())
			{
				foreach_(CvUnit* pLoopUnit, pAdjacentPlot->units() | filtered(CvUnit::fn::getTeam() != getTeam()))
				{
					if (GET_TEAM(getTeam()).isAtWar(pLoopUnit->getTeam()))
					{
						//	Koshling - changed city defenses to have a 1-in-4 chance of damaging each unit each turn
						if (pLoopUnit->baseCombatStr() && GC.getGame().getSorenRandNum(4, "City adjacent damage") == 0)
						{
							int iDamage = pLoopUnit->getHP();
							iDamage *= getAdjacentDamagePercent();
							iDamage /= 100;

							pLoopUnit->changeDamage(iDamage, getOwner());
							if (!abInformPlayer[pLoopUnit->getOwner()])
							{
								abInformPlayer[pLoopUnit->getOwner()] = true;
								CvWString szBuffer = gDLL->getText("TXT_KEY_MISC_UNITS_DAMAGED", getNameKey());
								AddDLLMessage(pLoopUnit->getOwner(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, NULL, MESSAGE_TYPE_INFO, pLoopUnit->getButton(), GC.getCOLOR_RED(), pLoopUnit->getX(), pLoopUnit->getY(), true, true);
							}
						}
					}
				}
			}
		}
	}
}


void CvCity::doCorporation()
{
	PROFILE_FUNC();

	if (!GC.getGame().isOption(GAMEOPTION_ADVANCED_REALISTIC_CORPORATIONS))
	{
		return;
	}
	const PlayerTypes ePlayer = getOwner();
	CvPlayer& kOwner = GET_PLAYER(ePlayer);

	for (int iI = 0; iI < GC.getNumCorporationInfos(); iI++)
	{
		if (GC.getGame().getHeadquarters((CorporationTypes)iI) == NULL)
		{
			continue;
		}
		if (!isHasCorporation((CorporationTypes)iI) && EnablerKernel::everAvailable(EDGEB_CORPORATIONS, iI))
		{
			if (kOwner.isNoCorporations() || kOwner.isNoForeignCorporations() && GC.getGame().getHeadquarters((CorporationTypes)iI)->getOwner() != ePlayer)
			{
				continue;
			}
			int iRandThreshold = 0;
			for (int iJ = 0; iJ < MAX_PLAYERS; iJ++)
			{
				const CvPlayer& kPlayer = GET_PLAYER((PlayerTypes)iJ);
				if (kPlayer.isAlive())
				{
					foreach_(const CvCity* pLoopCity, kPlayer.cities())
					{
						if (pLoopCity->isConnectedTo(this))
						{
							int iSpread = pLoopCity->getCorporationInfluence((CorporationTypes)iI);

							iSpread *= GC.getCorporationInfo((CorporationTypes)iI).getSpreadFactor();

							iSpread /= 100;

							if (iJ != ePlayer && GET_TEAM(kPlayer.getTeam()).isFreeTradeAgreement(kOwner.getTeam()))
							{
								iSpread *= (100 + GC.getFREE_TRADE_CORPORATION_SPREAD_MOD());
								iSpread /= 100;
							}

							if (iSpread > 0)
							{
								iSpread /= std::max(1, (((GC.getCORPORATION_SPREAD_DISTANCE_DIVISOR() * plotDistance(getX(), getY(), pLoopCity->getX(), pLoopCity->getY())) / GC.getMap().maxPlotDistance()) - 5));

								iRandThreshold = std::max(iRandThreshold, iSpread);
							}
						}
					}
				}
			}
			iRandThreshold *= kOwner.getCorporationSpreadModifier() + 100;
			iRandThreshold /= 100;
			iRandThreshold *= kOwner.getCorporationInfluence((CorporationTypes)iI);
			iRandThreshold /= 100;
			iRandThreshold /= 1 + getCorporationCount() / 2;

			int iRand = GC.getCORPORATION_SPREAD_RAND();
			iRand *= CvGameSpeedScale::speedPercent();
			iRand /= 100;
			iRand = GC.getGame().getSorenRandNum(iRand, "Corporation Spread");
			if (iRand < iRandThreshold)
			{
				//Remove Hostile Corporations
				for (int iJ = 0; iJ < GC.getNumCorporationInfos(); iJ++)
				{
					if (iI != iJ
					&& GC.getGame().isCompetingCorporation((CorporationTypes)iJ, (CorporationTypes)iI)
					&& isActiveCorporation((CorporationTypes)iJ))
					{
						setHasCorporation((CorporationTypes)iJ, false, false, false);

						AddDLLMessage(
							ePlayer, false, GC.getEVENT_MESSAGE_TIME(),
							gDLL->getText(
								"TXT_KEY_MISC_CORPORATION_HOSTILE_TAKEOVER",
								GC.getCorporationInfo((CorporationTypes)iJ).getTextKeyWide(),
								GC.getCorporationInfo((CorporationTypes)iI).getTextKeyWide(),
								getNameKey()
							),
							GC.getCorporationInfo((CorporationTypes)iJ).getSound(),
							MESSAGE_TYPE_MAJOR_EVENT,
							GC.getCorporationInfo((CorporationTypes)iJ).getButton(),
							GC.getCOLOR_WHITE(), getX(), getY(), false, false
						);
					}
				}
				setHasCorporation((CorporationTypes)iI, true, true, false);
				break;
			}
		}
		// Decay
		else if (this != GC.getGame().getHeadquarters((CorporationTypes)iI))
		{
			// TODO: Should HQ ever relocate?
			const int iDiff =
			(
				GC.getGame().getAverageCorporationInfluence(this, (CorporationTypes)iI)
				-
				GC.getCorporationInfo((CorporationTypes)iI).getSpreadFactor()
				* getCorporationInfluence((CorporationTypes)iI)
				* kOwner.getCorporationInfluence((CorporationTypes)iI)
				/
				10000
			);
			// Our influence is lower than average
			if (iDiff > 0)
			{
				const int iRand =
				(
					GC.getCORPORATION_SPREAD_RAND()
					* CvGameSpeedScale::speedPercent()
					/ 100
				);
				if (GC.getGame().getSorenRandNum(iRand, "Corporation Decay") < iDiff)
				{
					setHasCorporation((CorporationTypes)iI, false, true, false);
					break;
				}
			}
		}
	}
}

// TODO: cache this variable
int CvCity::getCorporationInfluence(CorporationTypes eCorporation) const
{
	PROFILE_EXTRA_FUNC();
	int iInfluence = 100;

	int iBonusesConsumed = 0;
	int iNumAvailBonuses = 0;
	//Influence scales based on the number of resources a corporation consumes
	foreach_(const int iConsumedBonus, GC.getCorporationInfo(eCorporation).getConsumedBonuses())
	{
		const BonusTypes eBonus = static_cast<BonusTypes>(iConsumedBonus);
		iBonusesConsumed++;
		iNumAvailBonuses += getNumBonuses(eBonus);
	}

	if (iNumAvailBonuses > 0)
	{
		iInfluence += iNumAvailBonuses;
	}
	else
	{
		if (iNumAvailBonuses == 0 && iBonusesConsumed > 0)
		{
			return 0;
		}
	}

	if (iBonusesConsumed > 0)
	{
		foreach_(const int iConsumedBonus, GC.getCorporationInfo(eCorporation).getConsumedBonuses())
		{
			const BonusTypes eBonus = static_cast<BonusTypes>(iConsumedBonus);
			if (hasBonus(eBonus))
			{
				iInfluence += (GC.getCORPORATION_RESOURCE_BASE_INFLUENCE() / iBonusesConsumed);
			}
		}
	}

	for (int iI = 0; iI < GC.getNumCorporationInfos(); iI++)
	{
		if (iI != eCorporation)
		{
			if (GC.getGame().isCompetingCorporation(eCorporation, (CorporationTypes)iI))
			{
				if (isActiveCorporation((CorporationTypes)iI))
				{
					iInfluence /= 10;
				}
			}
		}
	}
	for (int iI = 0; iI < GC.getNumCorporationInfos(); iI++)
	{
		if (iI != eCorporation)
		{
			if (GC.getGame().isCompetingCorporation(eCorporation, (CorporationTypes)iI))
			{
				if (isActiveCorporation((CorporationTypes)iI))
				{
					if (GC.getGame().getHeadquarters((CorporationTypes)iI) == this)
					{
						return 0;
					}
				}
			}
		}
	}
	int iAveragePopulation = GC.getGame().getTotalPopulation();
	iAveragePopulation /= std::max(1, GC.getGame().getNumCivCities());
	if (iAveragePopulation > 0)
	{
		iInfluence *= getPopulation();
		iInfluence /= iAveragePopulation;
	}
	return iInfluence;
}

int64_t CvCity::calcCorporateMaintenance() const
{
	PROFILE_FUNC();

	int64_t iTaxes = 0;

	for (int iI = 0; iI < GC.getNumCorporationInfos(); iI++)
	{
		if (isActiveCorporation((CorporationTypes)iI) && GET_PLAYER(getOwner()).isActiveCorporation((CorporationTypes)iI))
		{
			const CvCorporationInfo& kCorporation = GC.getCorporationInfo((CorporationTypes)iI);

			int64_t iCorpTaxes = 0;

			for (int iCommerce = 0; iCommerce < NUM_COMMERCE_TYPES; ++iCommerce)
			{
				iCorpTaxes += kCorporation.getHeadquartersCommerce((CommerceTypes)iCommerce);
			}

			// The per-bonus rate and this city's owned count of the consumed bonuses are ONE authored deposit
			// (the `per:{anyOf}` scaler) -- the corp upkeep deposit's own fold.
			iCorpTaxes +=
			(
				kCorporation.expectedModifier(MODFAM_MAINTENANCE, MAINTENANCE_CORPORATION, CASC_UNIT_FLAT,
					getCityContext(), GET_PLAYER(getOwner()).getEmpireContext(), plotGroup(getOwner()))
				* GC.getWorldInfo(GC.getMap().getWorldSize()).getCorporationMaintenancePercent()
				/ 100
			);
			const int iAveragePopulation = GC.getGame().getTotalPopulation() / std::max(1, GC.getGame().getNumCivCities());

			if (iAveragePopulation > 0)
			{
				iCorpTaxes *= getPopulation();
				iCorpTaxes /= iAveragePopulation;
			}
			// The additive-linear §2 combine at 64-bit width. realizedChannel is the ONE implementation of the
			// shape, but it takes `long` -- 32-bit on this toolchain -- and a corporate-tax total does not fit
			// that, so the same arithmetic is spelled here rather than narrowing the value to reach it.
			const int iCorpStack = maintenancePercentStack((int)MAINTENANCE_CORPORATION);
			iCorpTaxes = iCorpTaxes * std::max(0, 100 + iCorpStack) / 100;

			iCorpTaxes *= GC.getHandicapInfo(getHandicapType()).getMaintenanceModifier(MAINTENANCE_CORPORATION, CASC_SCOPE_EMPIRE);
			iCorpTaxes /= 100;

			iTaxes += iCorpTaxes;
		}
	}
	FASSERT_NOT_NEGATIVE(iTaxes);

	return iTaxes / 100;
}

int CvCity::getWarWearinessTimer() const
{
	return m_iWarWearinessTimer;
}

void CvCity::changeWarWearinessTimer(int iChange)
{
	m_iWarWearinessTimer += iChange;
}

void CvCity::doWarWeariness()
{
	if (getWarWearinessTimer() > 0)
	{
		changeWarWearinessTimer(-20);
	}
	if (getEventAnger() > 0)
	{
		int iTurnCheck = 10;
		iTurnCheck *= CvGameSpeedScale::speedPercent();
		iTurnCheck /= 100;
		if (GC.getGame().getElapsedGameTurns() % iTurnCheck == 0)
		{
			changeEventAnger(-1);
		}
	}
}

int CvCity::getEventAnger() const
{
	return m_iEventAnger;
}

void CvCity::changeEventAnger(int iChange)
{
	if (iChange != 0)
	{
		m_iEventAnger += iChange;
		FASSERT_NOT_NEGATIVE(getEventAnger());

		AI_setAssignWorkDirty(true);

		if (getTeam() == GC.getGame().getActiveTeam())
		{
			setInfoDirty(true);
		}
	}
}

int CvCity::getNonHolyReligionCount() const
{
	PROFILE_EXTRA_FUNC();
	int iCount = 0;

	for (int iI = 0; iI < GC.getNumReligionInfos(); iI++)
	{
		if (isHasReligion((ReligionTypes)iI) && !isHolyCity((ReligionTypes)iI))
		{
			iCount++;
		}
	}
	return iCount;
}

int CvCity::getMinimumDefenseLevel() const
{
	if (!GC.getGame().isOption(GAMEOPTION_COMBAT_REALISTIC_SIEGE))
	{
		return 0;
	}

	if (m_iMinimumDefenseLevel == 0)
	{
		return m_iMinimumDefenseLevel;
	}

	if (getExtraMinDefense() > m_iMinimumDefenseLevel)
	{
		return getExtraMinDefense();
	}

	return m_iMinimumDefenseLevel;
}

void CvCity::setMinimumDefenseLevel(int iNewValue)
{
	m_iMinimumDefenseLevel = iNewValue;
}

void CvCity::removeWorstCitizenActualEffects(int iNumCitizens, int& iGreatPeopleRate, int& iHappiness, int& iHealthiness, int*& aiYields, int*& aiCommerces) const
{
	PROFILE_FUNC();

	std::vector<SpecialistTypes> paeRemovedSpecailists(iNumCitizens, NO_SPECIALIST);
	std::vector<bool> abRemovedPlots(NUM_CITY_PLOTS, false);

	iGreatPeopleRate = 0;
	iHappiness = 0;
	iHealthiness = 0;
	for (int iI = 0; iI < NUM_YIELD_TYPES; iI++)
	{
		aiYields[iI] = 0;
	}
	for (int iI = 0; iI < NUM_COMMERCE_TYPES; iI++)
	{
		aiCommerces[iI] = 0;
	}
	int iGenericSpecialist = GC.getDEFAULT_SPECIALIST();
	int iNumRemoved = 0;
	int iNumSpecialistsRemoved = 0;

	// A generic-citizen fast-removal loop used to live here; its guard
	// (getAssignedSpecialistCount() < iNumRemoved, with iNumRemoved starting at 0)
	// was always false, so it never executed. Removed as dead code (#64) - the
	// valuation loop below already performs all removals. Reviving the optimisation
	// would need the available generic-citizen count to decrement per iteration,
	// otherwise the corrected condition loops forever.
	bool bAvoidGrowth = false;
	bool bIgnoreGrowth = false;

	while (iNumRemoved < iNumCitizens)
	{
		int iWorstValue = MAX_INT;
		SpecialistTypes eWorstSpecialist = NO_SPECIALIST;

		// if we are using more specialists than the free ones we get
		if (getAssignedSpecialistCount() < iNumSpecialistsRemoved)
		{
			for (int iI = 0; iI < GC.getNumSpecialistInfos(); iI++)
			{
				if (getSpecialistCount((SpecialistTypes)iI) > getForceSpecialistCount((SpecialistTypes)iI))
				{
					const int iValue = AI_specialistValue(((SpecialistTypes)iI), bAvoidGrowth, /*bRemove*/ true);

					if (iValue < iWorstValue)
					{
						iWorstValue = iValue;
						eWorstSpecialist = (SpecialistTypes)iI;
					}
				}
			}
		}

		// check all the plots we working
		int iWorstPlot = -1;
		for (int plotIdx = SKIP_CITY_HOME_PLOT; plotIdx < NUM_CITY_PLOTS; plotIdx++)
		{
			if (isWorkingPlot(plotIdx) && !abRemovedPlots[plotIdx])
			{
				const CvPlot* pLoopPlot = getCityIndexPlot(plotIdx);

				if (pLoopPlot != NULL)
				{
					const int iValue = AI_plotValue(pLoopPlot, bAvoidGrowth, /*bRemove*/ true, /*bIgnoreFood*/ false, bIgnoreGrowth);

					if (iValue < iWorstValue)
					{
						iWorstValue = iValue;
						eWorstSpecialist = NO_SPECIALIST;
						iWorstPlot = plotIdx;
					}
				}
			}
		}

		if (eWorstSpecialist != NO_SPECIALIST)
		{
			paeRemovedSpecailists[iNumRemoved] = eWorstSpecialist;
			iNumRemoved++;
			iNumSpecialistsRemoved++;
		}
		else if (iWorstPlot != -1)
		{
			abRemovedPlots[iWorstPlot] = true;
			iNumRemoved++;
		}
		else break;
	}

	for (int iI = 0; iI < iNumCitizens; iI++)
	{
		if (paeRemovedSpecailists[iI] != NO_SPECIALIST)
		{
			const CvSpecialistInfo& kSpecialist = GC.getSpecialistInfo(paeRemovedSpecailists[iI]);
			iHappiness -= kSpecialist.getFlatWellbeing(WELLBEING_HAPPINESS, CASC_SCOPE_CITY);
			iHealthiness -= kSpecialist.getFlatWellbeing(WELLBEING_HEALTH, CASC_SCOPE_CITY);
			iGreatPeopleRate -= kSpecialist.getScalar(SCALAR_GREAT_PEOPLE_RATE, CASC_SCOPE_CITY, CASC_UNIT_FLAT);
			for (int iJ = 0; iJ < NUM_YIELD_TYPES; iJ++)
			{
				//Team Project (1)
				aiYields[iJ] -= specialistYield(paeRemovedSpecailists[iI], (YieldTypes)iJ);
			}
			for (int iJ = 0; iJ < NUM_COMMERCE_TYPES; iJ++)
			{
				aiCommerces[iJ] -= GET_PLAYER(getOwner()).specialistCommerce(paeRemovedSpecailists[iI], (CommerceTypes)iJ);
			}
		}
	}
	iHealthiness /= 100;
	iHappiness /= 100;
	iGreatPeopleRate /= 100;   // reduced ONCE here with its siblings, never inline
	for (int plotIdx = 0; plotIdx < NUM_CITY_PLOTS; plotIdx++)
	{
		if (abRemovedPlots[plotIdx])
		{
			const CvPlot* pLoopPlot = getCityIndexPlot(plotIdx);
			FAssertMsg(pLoopPlot != NULL, CvString::format("pLoopPlot was null for iIndex %d", plotIdx).c_str());
			if (pLoopPlot != NULL)
			{
				int aiPlotYields100[NUM_YIELD_TYPES];
				pLoopPlot->getYields(aiPlotYields100);   // ×100 group read (getYield is the EXE edge)
				for (int yieldIdx = 0; yieldIdx < NUM_YIELD_TYPES; yieldIdx++)
				{
					aiYields[yieldIdx] -= aiPlotYields100[yieldIdx];
				}
			}
		}
	}
}

int CvCity::calculatePopulationHealth() const
{
	return m_iHealthPercentPerPopulation * getPopulation() / 100;
}

void CvCity::changeHealthPercentPerPopulation(int iChange)
{
	if (iChange != 0)
	{
		m_iHealthPercentPerPopulation += iChange;
		AI_setAssignWorkDirty(true);
	}
}

int CvCity::getAssignedSpecialistCount() const
{
	PROFILE_EXTRA_FUNC();
	int iCount = 0;
	for (int iI = 0; iI < GC.getNumSpecialistInfos(); iI++)
	{
		iCount += getSpecialistCount((SpecialistTypes)iI);
	}
	return iCount;
}

bool CvCity::isAutomatedCanBuild(BuildTypes eBuild) const
{
	return m_pabAutomatedCanBuild[eBuild];
}

void CvCity::setAutomatedCanBuild(BuildTypes eBuild, bool bNewValue)
{
	m_pabAutomatedCanBuild[eBuild] = bNewValue;
}

int CvCity::getMintedCommerce() const
{
	PROFILE_EXTRA_FUNC();
	int iCommerceTimes100 = 0;
	for (int iI = 0; iI < GC.getNumBonusInfos(); iI++)
	{
		int iBonusCount = getNumBonuses((BonusTypes)iI);
		if (iBonusCount != 0)
		{
			iCommerceTimes100 += iBonusCount * GET_PLAYER(getOwner()).getBonusMintedPercent((BonusTypes)iI);
		}
	}
	return iCommerceTimes100;
}

void CvCity::setBuildingListInvalid()
{
	m_BuildingList.setInvalid();
}

bool CvCity::getBuildingListFilterActive(BuildingFilterTypes eFilter) const
{
	return m_BuildingList.getFilterActive(eFilter);
}

void CvCity::setBuildingListFilterActive(BuildingFilterTypes eFilter, bool bActive)
{
	m_BuildingList.setFilterActive(eFilter, bActive);
}

BuildingGroupingTypes CvCity::getBuildingListGrouping()
{
	return m_BuildingList.getGroupingActive();
}

void CvCity::setBuildingListGrouping(BuildingGroupingTypes eGrouping)
{
	m_BuildingList.setGroupingActive(eGrouping);
}

BuildingSortTypes CvCity::getBuildingListSorting()
{
	return m_BuildingList.getSortingActive();
}

void CvCity::setBuildingListSorting(BuildingSortTypes eSorting)
{
	m_BuildingList.setSortingActive(eSorting);
}

int CvCity::getBuildingListGroupNum()
{
	return m_BuildingList.getGroupNum();
}

int CvCity::getBuildingListNumInGroup(int iGroup)
{
	return m_BuildingList.getNumInGroup(iGroup);
}

BuildingTypes CvCity::getBuildingListType(int iGroup, int iPos)
{
	return m_BuildingList.getBuildingType(iGroup, iPos);
}

int CvCity::getBuildingListSelectedBuildingRow()
{
	return m_BuildingList.getBuildingSelectionRow();
}

int CvCity::getBuildingListSelectedWonderRow()
{
	return m_BuildingList.getWonderSelectionRow();
}

BuildingTypes CvCity::getBuildingListSelectedBuilding()
{
	return m_BuildingList.getSelectedBuilding();
}

BuildingTypes CvCity::getBuildingListSelectedWonder()
{
	return m_BuildingList.getSelectedWonder();
}

void CvCity::setBuildingListSelectedBuilding(BuildingTypes eBuilding)
{
	m_BuildingList.setSelectedBuilding(eBuilding);
}

void CvCity::setBuildingListSelectedWonder(BuildingTypes eWonder)
{
	m_BuildingList.setSelectedWonder(eWonder);
}

void CvCity::setUnitListInvalid()
{
	m_UnitList.setInvalid();
}

bool CvCity::getUnitListFilterActive(UnitFilterTypes eFilter) const
{
	return m_UnitList.getFilterActive(eFilter);
}

void CvCity::setUnitListFilterActive(UnitFilterTypes eFilter, bool bActive)
{
	m_UnitList.setFilterActive(eFilter, bActive);
}

UnitGroupingTypes CvCity::getUnitListGrouping()
{
	return m_UnitList.getGroupingActive();
}

void CvCity::setUnitListGrouping(UnitGroupingTypes eGrouping)
{
	m_UnitList.setGroupingActive(eGrouping);
}

UnitSortTypes CvCity::getUnitListSorting()
{
	return m_UnitList.getSortingActive();
}

void CvCity::setUnitListSorting(UnitSortTypes eSorting)
{
	m_UnitList.setSortingActive(eSorting);
}

int CvCity::getUnitListGroupNum()
{
	return m_UnitList.getGroupNum();
}

int CvCity::getUnitListNumInGroup(int iGroup)
{
	return m_UnitList.getNumInGroup(iGroup);
}

UnitTypes CvCity::getUnitListType(int iGroup, int iPos)
{
	return m_UnitList.getUnitListType(iGroup, iPos);
}

int CvCity::getUnitListSelectedRow()
{
	return m_UnitList.getSelectionRow();
}

UnitTypes CvCity::getUnitListSelected()
{
	return m_UnitList.getSelectedUnit();
}

void CvCity::setUnitListSelected(UnitTypes eUnit)
{
	m_UnitList.setSelectedUnit(eUnit);
}

int CvCity::getTotalBuildingSourcedProperty(PropertyTypes eProperty) const
{
	PROFILE_EXTRA_FUNC();
	std::map<int, int>::const_iterator itr = m_buildingSourcedPropertyCache.find(eProperty);

	if (itr != m_buildingSourcedPropertyCache.end())
	{
		return itr->second;
	}
	int	iValue = 0;

	foreach_(const BuildingTypes eTypeX, getHasBuildings())
	{
		if (hasFullyActiveBuilding(eTypeX))
		{
			foreach_(const CvPropertySource* pSource, GC.getBuildingInfo(eTypeX).getPropertyManipulators()->getSources())
			{
				//	For now we're only interested in constant sources
				//	TODO - expand this as buildings add other types
				if (pSource->getType() == PROPERTYSOURCE_CONSTANT && pSource->getProperty() == eProperty)
				{
					iValue += static_cast<const CvPropertySourceConstant*>(pSource)->getAmountPerTurn(getGameObject());
				}
			}
		}
	}
	m_buildingSourcedPropertyCache[(int)eProperty] = iValue;

	return iValue;
}

void unitSources(const CvPropertyManipulators* pMani, PropertyTypes eProperty, const CvCity* pCity, int* iValue)
{
	PROFILE_EXTRA_FUNC();
	foreach_(const CvPropertySource* pSource, pMani->getSources())
	{
		//	Sources that deliver to the city or the plot are both considered since the city plot diffuses
		//	to the city for most properties anyway
		if (pSource->getProperty() == eProperty &&
			(pSource->getObjectType() == GAMEOBJECT_CITY || pSource->getObjectType() == GAMEOBJECT_PLOT) &&
			pSource->getType() == PROPERTYSOURCE_CONSTANT)
		{
			*iValue += static_cast<const CvPropertySourceConstant*>(pSource)->getAmountPerTurn(pCity->getGameObject());
		}
	}
}

int CvCity::getTotalUnitSourcedProperty(PropertyTypes eProperty) const
{
	PROFILE_EXTRA_FUNC();
	std::map<int, int>::const_iterator itr = m_unitSourcedPropertyCache.find(eProperty);

	if (itr != m_unitSourcedPropertyCache.end())
	{
		return itr->second;
	}
	int	iValue = 0;

	foreach_ (const CvUnit* unit, plot()->units())
	{
		unit->getGameObject()->foreachManipulator(bind(unitSources, _1, eProperty, this, &iValue));
	}
	m_unitSourcedPropertyCache[(int)eProperty] = iValue;

	return iValue;
}

void unitHasSources(const CvPropertyManipulators* pMani, bool* bHasSources)
{
	PROFILE_EXTRA_FUNC();
	foreach_(const CvPropertySource* pSource, pMani->getSources())
	{
		//	Sources that deliver to the city or the plot are both considered since the city plot diffuses
		//	to the city for most properties anyway
		if ((pSource->getObjectType() == GAMEOBJECT_CITY || pSource->getObjectType() == GAMEOBJECT_PLOT) &&
			pSource->getType() == PROPERTYSOURCE_CONSTANT)
		{
			*bHasSources = true;
			break;
		}
	}
}

//	Helper function to determine if a unit has any city/plot property sources
static bool unitHasCityOrPlotPropertySources(const CvUnit* pUnit)
{
	PROFILE_EXTRA_FUNC();
	bool bHasSources = false;

	pUnit->getGameObject()->foreachManipulator(bind(unitHasSources, _1, &bHasSources));

	return bHasSources;
}

void CvCity::noteUnitMoved(const CvUnit* pUnit) const
{
	if (unitHasCityOrPlotPropertySources(pUnit))
	{
		m_unitSourcedPropertyCache.clear();
	}
}

void sumCitySources(const CvPropertyManipulators* pMani, const CvCity* pCity, int* iSum, PropertyTypes eProperty)
{
	PROFILE_EXTRA_FUNC();
	foreach_(const CvPropertySource* pSource, pMani->getSources())
	{
		if (pSource->getProperty() == eProperty)
		{
			if (pSource->isActive(const_cast<CvGameObjectCity*>(pCity->getGameObject())))
			{
				*iSum += pSource->getSourcePredict(pCity->getGameObject(), pCity->getPropertiesConst()->getValueByProperty(eProperty));
			}
		}
	}
}

int CvCity::getGlobalSourcedProperty(PropertyTypes eProperty) const
{
	PROFILE_EXTRA_FUNC();
	int iSum = 0;
	foreach_(const CvPropertySource* pSource, GC.getPropertyInfo(eProperty).getPropertyManipulators()->getSources())
	{
		if (pSource->isActive(getGameObject()))
		{
			iSum += pSource->getSourcePredict(getGameObject(), getPropertiesConst()->getValueByProperty(eProperty));
		}
	}
	// Add sources from the player object that have an effect on cities
	GET_PLAYER(getOwner()).getGameObject()->foreachManipulator(bind(sumCitySources, _1, this, &iSum, eProperty));

	return iSum;
}




int CvCity::getUnitCombatDefenseAgainstModifierTotal(UnitCombatTypes eIndex) const
{
	FASSERT_BOUNDS(0, GC.getNumUnitCombatInfos(), eIndex);
	return m_paiUnitCombatDefenseAgainstModifier[eIndex];
}

void CvCity::changeUnitCombatDefenseAgainstModifierTotal(UnitCombatTypes eIndex, int iChange)
{
	FASSERT_BOUNDS(0, GC.getNumUnitCombatInfos(), eIndex);
	m_paiUnitCombatDefenseAgainstModifier[eIndex] += iChange;
}







int CvCity::localCitizenCaptureResistance() const
{
	int iTotal = 0;
	int aiCapture[NUM_CAPTURE_KINDS];
	GET_PLAYER(getOwner()).getCaptureKinds(aiCapture);
	iTotal += aiCapture[CAPTURE_RESISTANCE];
	return iTotal;
}

int CvCity::getExtraLocalDynamicDefense() const { return cascadeDefense(DEFENSE_DYNAMIC); }
int CvCity::getExtraRiverDefensePenalty() const { return cascadeDefense(DEFENSE_RIVER_PENALTY); }
// The FLOOR on the defense stack, and therefore the SAME unit as the stack it floors: `amount` sums like a flat
// but is measured in defense points and applied as a percentage, so `min` is a percent too and is never scaled.
// The realized read maxes the two against each other, which only means anything while they share a unit.
int CvCity::getExtraMinDefense() const { return cascadeDefense(DEFENSE_MIN); }

int CvCity::getExtraBuildingDefenseRecoverySpeedModifier() const { return cascadeDefense(DEFENSE_BUILDING_RECOVERY); }
int CvCity::getModifiedBuildingDefenseRecoverySpeedCap() const
{
	return m_iModifiedBuildingDefenseRecoverySpeedCap;
}
void CvCity::setModifiedBuildingDefenseRecoverySpeedCap(int iValue)
{
	m_iModifiedBuildingDefenseRecoverySpeedCap = iValue;
}
void CvCity::changeModifiedBuildingDefenseRecoverySpeedCap(int iChange)
{
	m_iModifiedBuildingDefenseRecoverySpeedCap += iChange;
}

int CvCity::getExtraCityDefenseRecoverySpeedModifier() const { return cascadeDefense(DEFENSE_CITY_RECOVERY); }
int CvCity::cityDefenseRecoveryRate() const
{
	int iValue = GC.getCITY_DEFENSE_DAMAGE_HEAL_RATE();

	int iRecoveryModifier = getExtraCityDefenseRecoverySpeedModifier();

	if (getDefenseModifier(false) < getModifiedBuildingDefenseRecoverySpeedCap())
	{
		iRecoveryModifier += getExtraBuildingDefenseRecoverySpeedModifier();
	}

	iValue *= (100 + iRecoveryModifier);
	iValue /= 100;

	return iValue;
}








int CvCity::getLocalSpecialistExtraYield(SpecialistTypes eSpecialist, YieldTypes eYield) const
{
	FASSERT_BOUNDS(0, GC.getNumSpecialistInfos(), eSpecialist);
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, eYield);

	return m_ppaaiLocalSpecialistExtraYield[eSpecialist][eYield];
}

void CvCity::changeLocalSpecialistExtraYield(SpecialistTypes eSpecialist, YieldTypes eYield, int iChange)
{
	FASSERT_BOUNDS(0, GC.getNumSpecialistInfos(), eSpecialist);
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, eYield);

	if (iChange != 0)
	{
		m_ppaaiLocalSpecialistExtraYield[eSpecialist][eYield] += iChange;
	}
	AI_setAssignWorkDirty(true);
}


int CvCity::specialistCount(SpecialistTypes eSpecialist) const
{
	FASSERT_BOUNDS(0, GC.getNumSpecialistInfos(), eSpecialist);

	return getSpecialistCount(eSpecialist) + getFreeSpecialistCount(eSpecialist);
}

int CvCity::specialistYield(SpecialistTypes eSpecialist, YieldTypes eYield) const
{
	FASSERT_BOUNDS(0, GC.getNumSpecialistInfos(), eSpecialist);
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, eYield);

	return GET_PLAYER(getOwner()).specialistYield(eSpecialist, eYield) + getLocalSpecialistExtraYield(eSpecialist, eYield);
}

int CvCity::specialistCommerce(SpecialistTypes eSpecialist, CommerceTypes eCommerce) const
{
	FASSERT_BOUNDS(0, GC.getNumSpecialistInfos(), eSpecialist);
	FASSERT_BOUNDS(0, NUM_COMMERCE_TYPES, eCommerce);

	return GET_PLAYER(getOwner()).specialistCommerce(eSpecialist, eCommerce);
}

int CvCity::specialistYieldTotal(SpecialistTypes eSpecialist, YieldTypes eYield) const
{
	FASSERT_BOUNDS(0, GC.getNumSpecialistInfos(), eSpecialist);
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, eYield);

	return (specialistCount(eSpecialist) * specialistYield(eSpecialist, eYield));
}

int CvCity::getPrioritorizedSpecialist() const
{
	return m_iPrioritySpecialist;
}

void CvCity::setPrioritorizedSpecialist(SpecialistTypes eSpecialist)
{
	FASSERT_BOUNDS(0, GC.getNumSpecialistInfos(), eSpecialist);

	m_iPrioritySpecialist = (int)eSpecialist;
	if (isSpecialistBanned(eSpecialist))
	{
		removeSpecialistBan(eSpecialist);
	}
}

bool CvCity::isSpecialistBanned(SpecialistTypes eSpecialist) const
{
	FASSERT_BOUNDS(0, GC.getNumSpecialistInfos(), eSpecialist);

	return m_paiSpecialistBannedCount[eSpecialist] > 0;
}

void CvCity::banSpecialist(SpecialistTypes eSpecialist)
{
	FASSERT_BOUNDS(0, GC.getNumSpecialistInfos(), eSpecialist);

	m_paiSpecialistBannedCount[eSpecialist] = 1;
	if (m_iPrioritySpecialist == (int)eSpecialist)
	{
		m_iPrioritySpecialist = NO_SPECIALIST;
	}
}

void CvCity::removeSpecialistBan(SpecialistTypes eSpecialist)
{
	FASSERT_BOUNDS(0, GC.getNumSpecialistInfos(), eSpecialist);

	m_paiSpecialistBannedCount[eSpecialist] = 0;
}

bool CvCity::isDirectAttackable() const
{
	int iMinimumDefenseLevel = getMinimumDefenseLevel();
	if (iMinimumDefenseLevel == 0)
	{
		iMinimumDefenseLevel = MAX_INT;
	}

	//ls612 Quickfix: Cities can be attacked if they have no defenders, regardless of if there is a minimun defense level
	if (getDefenseModifier(false) > iMinimumDefenseLevel&& plot()->getNumDefenders(getOwner()) > 0)
	{
		return false;
	}

	return true;
}

int CvCity::getInvestigationTotal(bool bActual) const
{
	PROFILE_EXTRA_FUNC();
	int iBestUnitInvestigation = 0;
	int iAssistance = 0;
	int iFivePercentAssistance = 0;

	const CvPlot* pPlot = plot();
	if (pPlot != NULL)
	{
		CvUnit* pBestUnit = NULL;
		foreach_(CvUnit* pLoopUnit, pPlot->units())
		{
			if (pLoopUnit->getOwner() == getOwner())
			{
				const int iUnitInvestigation = pLoopUnit->getInvestigationTotal();

				if (iUnitInvestigation > iBestUnitInvestigation)
				{
					iBestUnitInvestigation = iUnitInvestigation;
					pBestUnit = pLoopUnit;
				}
				if (iUnitInvestigation > 0)
				{
					iAssistance++;
					iFivePercentAssistance += iUnitInvestigation;
				}
			}
		}
		if (bActual && pBestUnit != NULL)
		{
			iAssistance--;//To remove the bonus one would give itself.
			iFivePercentAssistance -= iBestUnitInvestigation;
			pBestUnit->changeExperience100(5);
		}
	}
	iFivePercentAssistance /= 20;
	int iTotal = iAssistance;
	iTotal += iFivePercentAssistance;
	iTotal += iBestUnitInvestigation;
	iTotal += getExtraInvestigation();

	return iTotal;
}

// UNDERWORLD, not espionage: the in-city criminal contest (json.md §6). The same words exist on the unit plane
// as espionage spy stats -- a different mechanic, kept separate by family.
// ⚠ Both are FLAT slots (×100) feeding a human contest rolled against a plain random, and the unit-plane twin
// already reduces -- so they reduce here too ([DEC-fixedpoint-x100]). Raw, one building decides the contest.
int CvCity::getExtraInsidiousness() const { return cascadeValue(MODFAM_UNDERWORLD, UNDERWORLD_INSIDIOUSNESS) / 100; }
int CvCity::getExtraInvestigation() const { return cascadeValue(MODFAM_UNDERWORLD, UNDERWORLD_INVESTIGATION) / 100; }

int CvCity::getPropertyNeed(PropertyTypes eProperty) const
{
	PROFILE_EXTRA_FUNC();
	if (m_icachedPropertyNeedsTurn != GC.getGame().getGameTurn() || NULL == m_cachedPropertyNeeds)
	{
		const EraTypes eEra = GET_PLAYER(getOwner()).getCurrentEra();

		if (NULL == m_cachedPropertyNeeds)
		{
			m_cachedPropertyNeeds = new int[GC.getNumPropertyInfos()];
			for (int iI = 0; iI < GC.getNumPropertyInfos(); iI++)
			{
				m_cachedPropertyNeeds[iI] = 0;
			}
		}

		for (int iI = 0; iI < GC.getNumPropertyInfos(); iI++)
		{
			const PropertyTypes pProperty = (PropertyTypes)iI;
			if (GC.getPropertyInfo(pProperty).getAIWeight() != 0)
			{
				int iCurrentValue = getPropertiesConst()->getValueByProperty(pProperty);
				int iCurrentChange = getPropertiesConst()->getChangeByProperty(pProperty);
				//TB attempt to allow some modification to need based on existing drift value
				int iCurrentSourceSize = iCurrentChange;//Calvitix remove getTotalBuildingSourcedProperty(eProperty) + getTotalUnitSourcedProperty(eProperty);
				iCurrentSourceSize *= 10; //Evolution for the next 10 turns
				iCurrentValue += iCurrentSourceSize;
				//
				int iTarget = 0;
				if (GC.getPropertyInfo(pProperty).isTargetLevelbyEraType((int)eEra))
				{
					iTarget = GC.getPropertyInfo(pProperty).getTargetLevelbyEraType((int)eEra);
				}
				else
				{
					iTarget = GC.getPropertyInfo(pProperty).getTargetLevel();
				}
				int iNeed = iTarget - iCurrentValue;
				int iAIPropertyWeight = GC.getPropertyInfo(pProperty).getAIWeight() / 50;
				if (iAIPropertyWeight > 0) //Properties that are positive (EDU,TOURISM)
				{ 
					if (iTarget > iCurrentValue) //Still need to be better
					{
						iNeed = (iTarget - iCurrentValue) * iAIPropertyWeight;
					}
					else
					{
						iNeed = -1;
					}		
				}
				else
				{
					if (iTarget < iCurrentValue) //Still need to be better
					{
						iNeed = (iTarget - iCurrentValue) * iAIPropertyWeight;
					}
					else
					{
						iNeed = -1;
					}
				}
				m_cachedPropertyNeeds[iI] = iNeed;//(iNeed * (GC.getPropertyInfo(pProperty).getAIWeight() / 50));
			}
		}
		m_icachedPropertyNeedsTurn = GC.getGame().getGameTurn();
	}
	int iIndex = (int)eProperty;

	return (m_cachedPropertyNeeds[iIndex]);
}


bool CvCity::isQuarantined() const
{
	return (getQuarantinedCount() > 0);
}

int CvCity::getQuarantinedCount() const
{
	return m_iQuarantinedCount;
}

void CvCity::changeQuarantinedCount(int iChange)
{
	m_iQuarantinedCount += iChange;
}

// ---- THE CITIZEN-JUGGLE BRACKET -------------------------------------------------------------------------
//
//	The governor probes citizen assignments by MUTATING for real and measuring, many times per run. Each probe
//	mutation is semantically correct but drags a side-effect layer behind it -- three whole-set specialist
//	recomputes, and a DOMAIN fact per specialist and per worked plot, each of which marks the city's packages
//	and forces a rebuild. Across a run that is the measured churn.
//
//	So the bracket DEFERS the layer and replays the run's NET once at the close. The probes keep their exact
//	semantics; what is withheld is only the announcing. ⚑ The net is the point: probes that converge back to
//	where they started cancel to NOTHING, so the common case announces nothing at all.
//
//	⛔ The close does NOT hand-dirty a cache. It EMITS the net facts and lets the modifier consumer derive the
//	marks, because that is the one mark derivation ([DEC-uniform-cache-shape]; the city package is "marked ONLY
//	by the modifier consumer's derived masks"). An earlier version dirtied a per-scope accumulator directly --
//	the archived substrate ([superseded-ideas] #14) -- and that is exactly what must not come back.
//
//	REFCOUNTED so a nested bracket cannot close the outer one early. Purely transient: never serialized, and
//	cleared by reset() like any other run state.
void CvCity::startCitizenJuggling()
{
	if (m_iCitizenJugglingCount++ == 0)
	{
		m_bJuggleDeferredSpec = false;
		m_bJuggleDeferredWork = false;

		const int iNumSpecialists = GC.getNumSpecialistInfos();
		m_juggleSpecialistStart.resize(iNumSpecialists);
		for (int iSpecialist = 0; iSpecialist < iNumSpecialists; ++iSpecialist)
		{
			m_juggleSpecialistStart[iSpecialist] = getSpecialistCount((SpecialistTypes)iSpecialist);
		}
		m_juggleWorkedStart.resize(NUM_CITY_PLOTS);
		for (int iPlot = 0; iPlot < NUM_CITY_PLOTS; ++iPlot)
		{
			m_juggleWorkedStart[iPlot] = isWorkingPlot(iPlot);
		}
	}
}

void CvCity::endCitizenJuggling()
{
	if (m_iCitizenJugglingCount <= 0)
	{
		FErrorMsg("Unbalanced citizen-juggle bracket");
		m_iCitizenJugglingCount = 0;
		return;
	}
	if (--m_iCitizenJugglingCount > 0)
	{
		return;   // an inner bracket closing; only the outermost replays
	}

	if (m_bJuggleDeferredSpec)
	{
		// the three whole-set recomputes every probe skipped, run ONCE for the run

		const int iNumSpecialists = std::min((int)m_juggleSpecialistStart.size(), GC.getNumSpecialistInfos());
		for (int iSpecialist = 0; iSpecialist < iNumSpecialists; ++iSpecialist)
		{
			const int iNet = getSpecialistCount((SpecialistTypes)iSpecialist) - m_juggleSpecialistStart[iSpecialist];
			if (iNet != 0)
			{
				if (iNet > 0)
	{
		emitCitySpecialistAdded(getID(), getOwner(), iSpecialist, iNet);
	}
	else
	{
		emitCitySpecialistRemoved(getID(), getOwner(), iSpecialist, -iNet);
	}
			}
		}
	}

	if (m_bJuggleDeferredWork)
	{
		const int iNumPlots = std::min((int)m_juggleWorkedStart.size(), (int)NUM_CITY_PLOTS);
		const bool bShowSymbols = getTeam() == GC.getGame().getActiveTeam() || GC.getGame().isDebugMode();
		for (int iPlot = 0; iPlot < iNumPlots; ++iPlot)
		{
			if (m_juggleWorkedStart[iPlot] == isWorkingPlot(iPlot))
			{
				continue;   // this plot ended where it started -- the probes cancelled
			}
			CvPlot* pPlot = getCityIndexPlot(iPlot);
			if (pPlot == NULL)
			{
				continue;   // a radius index off the map edge: no plot, so no fact and nothing to refresh
			}
			if (isWorkingPlot(iPlot))
	{
		emitPlotWorkedAdded(GC.getMap().plotNum(pPlot->getX(), pPlot->getY()), (int)getOwner(), getID());
	}
	else
	{
		emitPlotWorkedRemoved(GC.getMap().plotNum(pPlot->getX(), pPlot->getY()), (int)getOwner(), getID());
	}
			// the display half, once per genuinely-changed plot rather than once per probe
			pPlot->updatePlotBuilder();
			if (bShowSymbols)
			{
				pPlot->updateSymbolDisplay();
			}
		}
	}

	if ((m_bJuggleDeferredSpec || m_bJuggleDeferredWork) && isCitySelected())
	{
		gDLL->getInterfaceIFace()->setDirty(CitizenButtons_DIRTY_BIT, true);
	}
	m_bJuggleDeferredSpec = false;
	m_bJuggleDeferredWork = false;
}


void CvCity::resetQuarantinedCount()
{
	m_iQuarantinedCount = 0;
}


void CvCity::AI_setPropertyControlBuildingQueued(bool bSet)
{
	m_bPropertyControlBuildingQueued = bSet;
}

bool CvCity::AI_isPropertyControlBuildingQueued() const
{
	return m_bPropertyControlBuildingQueued;
}


void CvCity::setWorkerHave(const int iUnitID, const bool bNewValue)
{
	std::vector<int>::iterator itr = find(m_workers.begin(), m_workers.end(), iUnitID);

	if (bNewValue)
	{
		UnitCompWorker* workerComp = GET_PLAYER(getOwner()).getUnit(iUnitID)->getWorkerComponent();
		if (workerComp)
		{
			if (itr == m_workers.end())
			{
				m_workers.push_back(iUnitID);
			}
			else FErrorMsg("Tried to add a duplicate vector element!");

			workerComp->setCityAssignment(getID());
		}
		else FErrorMsg("UnitCompWorker unexpectedly not initialized");
	}
	else if (itr != m_workers.end())
	{
		CvUnit* unitX = GET_PLAYER(getOwner()).getUnit(iUnitID);
		if (unitX)
		{
			UnitCompWorker* workerComp = unitX->getWorkerComponent();
			if (workerComp)
			{
				workerComp->setCityAssignment(-1);
			}
			else FErrorMsg("UnitCompWorker unexpectedly not initialized!");
		}
		else FErrorMsg("m_workers contained an invalid unitID!");

		m_workers.erase(itr);
	}
	else
	{
		FErrorMsg("Vector element to remove was missing!");
		UnitCompWorker* workerComp = GET_PLAYER(getOwner()).getUnit(iUnitID)->getWorkerComponent();
		if (workerComp)
		{
			workerComp->setCityAssignment(-1);
		}
	}
}



const CityOutputHistory* CvCity::getCityOutputHistory() const
{
	return &m_outputHistory;
}


void CvCity::alterBuildingLedger(const BuildingTypes eType, const bool bAdd, const PlayerTypes eOwner, const int iTime)
{
	PROFILE_EXTRA_FUNC();
	FASSERT_BOUNDS(0, GC.getNumBuildingInfos(), eType);

	std::map<BuildingTypes, BuiltBuildingData>::const_iterator itr = m_buildingLedger.find(eType);

	if (itr == m_buildingLedger.end())
	{
		if (bAdd)
		{
			m_bHasBuildings[eType] = true;
			m_hasBuildings.push_back(eType);
			BuiltBuildingData data; 
			data.eBuiltBy = eOwner;
			data.iTimeBuilt = iTime;
			m_buildingLedger.insert(std::make_pair(eType, data));
		}
		else
		{
			FErrorMsg("Trying to remove entry that does not exist!");
		}
	}
	else if (bAdd)
	{
		FErrorMsg("Trying to add a duplicate entry!");
	}
	else 
	{
		m_bHasBuildings[eType] = false;
		m_hasBuildings.erase(find(m_hasBuildings.begin(), m_hasBuildings.end(), eType));
		m_buildingLedger.erase(itr->first);
	}
}

bool CvCity::hasBuilding(const BuildingTypes eType) const
{
	FASSERT_BOUNDS(0, GC.getNumBuildingInfos(), eType);
	return m_bHasBuildings[eType];
}

BuiltBuildingData CvCity::getBuildingData(const BuildingTypes eType) const
{
	FASSERT_BOUNDS(0, GC.getNumBuildingInfos(), eType);
	FAssert(m_bHasBuildings[eType]);
	if (m_bHasBuildings[eType])
	{
		return m_buildingLedger.find(eType)->second;
	}
	BuiltBuildingData data; 
	data.eBuiltBy = NO_PLAYER;
	data.iTimeBuilt = MIN_INT;
	return data;
}

