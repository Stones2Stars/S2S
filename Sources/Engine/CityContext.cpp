//
//	CityContext -- the ONE derivation of the city's stored blocks (see the header for what each block is and the
//	vicinity split against the enabler), plus the forwards for the raw data CvCity already holds O(1).
//
//	DEC-single-implementation: each stored fact is derived by calling the SAME engine accessor a read used to call --
//	once, at maintenance time, instead of once per read. No predicate's logic is re-implemented here.
//
//	CONSTRAINT: refreshAreaFacts reads the CENTRE plot's neighbours' CvArea, so it is valid only once the map is
//	settled -- CvPlot::area() is NULL for a plot whose area has not been assigned. ⚑ The guarantee is STRUCTURAL
//	rather than a guard: every route that reaches it needs a CITY to exist, and a city cannot exist before the map
//	has settled. World generation and the save read announce their plot facts while no city stands, so the radius
//	sweep finds nobody and the derivation is never entered; the area-recalculated fact re-runs it after a
//	wholesale reassignment. There is deliberately NO staleness check on the read side: a fact that fails to fire leaves the
//	value visibly wrong rather than being silently rebuilt (DEC-no-self-heal).
//

#include "CvGameCoreDLL.h"
#include "CityContext.h"
#include "CvPlot.h"
#include "CvCity.h"
#include "CvArea.h"             // refreshAreaFacts -- the area tile count the AREA_SIZE read is served from
#include "CvProperties.h"       // propertyValue -- the PROPERTY_ band read forward
#include "CvGameCoreUtils.h"    // plotDirection / plotCity -- the centre plot's neighbours + the radius-city inverse
#include "CvMap.h"              // plotByIndex -- a fact's iSrcLoc resolution
#include <vector>
#include "AI/CvPlayerAI.h"      // GET_PLAYER (the owner forward: state religion / policies)
#include "EmpireContext.h"      // the owner's empire aggregate (policies)
#include "CvCondition.h"    // CASC_PRED_* -- the shared HAS_/IS_ plot predicate ids plotAttrs keys on
#include "Defines/CvGlobals.h"            // GC -- the bonus / religion domain sizes the derivations walk
#include "Conditions/CvConditionEval.h"   // CvCascadeEvalCtx -- fillEvalCtx
#include "Infos/CvClassificationIds.h"    // CLS_AMENITY_* -- the generated amenity ids
#include "Spine/CvEventSpine.h"           // the amenity CROSSING facts consumers route on
#include "Infos/CvBuildingInfo.h"         // the grantor's `amenities` block (the city-local grantor)
#include "Infos/CvCivicInfo.h"            // the EMPIRE-scope grantor's `amenities` block
#include "Infos/CvClassificationRegistry.h"   // cachedKeyId -- the by-key amenity read's memoized id resolve

void CityContext::onPlotChanged(const CvPlot* plot, int sign)
{
	if (plot == NULL)
		return;
	// Fold the plot's STORED CASC_PRED_* verdict bitset into plotAttrs (+1 on enter, -1 on leave). COUNTS only --
	// the plot itself is never stored. The bitset IS the fold's only source, so plotAttrs is literally the sum of
	// the member plots' bits: one vocabulary at two granularities, which therefore cannot drift and needs no second
	// derivation here (PlotContext keeps the bits current off its own consumer).
	const unsigned int attributeBits = plot->getPlotContext().attributeBits();
	for (int predicateId = 0; predicateId < 32; ++predicateId)
	{
		if ((attributeBits & (1u << predicateId)) != 0)
		{
			plotAttrs.add(predicateId, sign);
		}
	}
}

// A member plot's OWN bit crossed. One ±1, on the bit the fact named -- nothing is re-derived and nothing is
// re-read from the plot ([contexts.md]: the fact SETS the count it names).
void CityContext::onPlotPredicateChanged(int iPredicateId, int iSign) const
{
	plotAttrs.add(iPredicateId, iSign);
}

void CityContext::clear() const
{
	plotAttrs.clear();
	m_vicinityAll.clear();
	m_vicinityOwned.clear();
	m_vicinityForeign.clear();
	m_vicinityWorked.clear();
	m_areaId = -1;
	m_areaTileCount = 0;
	m_governmentCenterDistance = 0;
	m_maxAdjacentWaterTiles = 0;
	m_holyCityCount = 0;
	m_headquartersCount = 0;
	// ⚠ The AMENITY state is NOT cleared here -- it is not this context's to clear. `CvCity::reset` zeroes its own
	// amenity context, the same way it resets its enabler and its operating set, because a city is recycled out of
	// an FFreeListTrashArray and a delta store is correct only from a known zero.
}

// ---- the ONE derivation per stored block ------------------------------------------------------------------------


// Re-derive the area facts: the city's area ID, that area's tile count, and the largest ADJACENT water body. The
// area id replaces a per-read plot()->area() chase; the water-body size replaces CvPlot::isCoastalLand's 8-neighbour
// scan, which dereferenced an adjacent water plot's CvArea on EVERY coastal check.
void CityContext::refreshAreaFacts() const
{
	m_areaId = -1;
	m_areaTileCount = 0;
	m_maxAdjacentWaterTiles = 0;
	if (m_city == NULL)
	{
		return;
	}
	const CvPlot* pCentrePlot = m_city->plot();
	if (pCentrePlot == NULL)
	{
		return;
	}
	m_areaId = pCentrePlot->getArea();
	const CvArea* pArea = pCentrePlot->area();
	if (pArea != NULL)
	{
		m_areaTileCount = pArea->getNumTiles();
	}
	// A water city plot is never coastal LAND, matching CvPlot::isCoastalLand's own first test.
	if (pCentrePlot->isWater())
	{
		return;
	}
	for (int iDirection = 0; iDirection < NUM_DIRECTION_TYPES; ++iDirection)
	{
		const CvPlot* pAdjacentPlot = plotDirection(pCentrePlot->getX(), pCentrePlot->getY(), (DirectionTypes)iDirection);
		if (pAdjacentPlot == NULL || !pAdjacentPlot->isWater())
		{
			continue;
		}
		const CvArea* pAdjacentArea = pAdjacentPlot->area();
		if (pAdjacentArea == NULL)
		{
			continue;   // the map is unsettled; the maintainer re-runs this once it is
		}
		if (pAdjacentArea->getNumTiles() > m_maxAdjacentWaterTiles)
		{
			m_maxAdjacentWaterTiles = pAdjacentArea->getNumTiles();
		}
	}
}

// Re-derive the holy-city count. The bare IS_HOLY_CITY read walked every religion info per call asking the game
// registry; the count answers it with one comparison.
// The DISTANCE_TO_GOVERNMENT_CENTER store. It re-derives WHOLE on any fact that can move it (a government
// centre appearing or going, a city gained or lost) rather than per-event deltas -- the one-derivation shape
// every other block here uses. The min over the owner's government centres is what the legacy distance
// formula computed PER READ; holding it is the whole point ([contexts.md]: cost tracks EVENT volume, never
// read volume).
void CityContext::refreshGovernmentCenterDistance() const
{
	m_governmentCenterDistance = 0;
	if (m_city == NULL)
	{
		return;
	}
	if (m_city->isGovernmentCenter())
	{
		return;
	}
	int iNearest = -1;
	foreach_(const CvCity* pLoopCity, GET_PLAYER(m_city->getOwner()).cities())
	{
		if (pLoopCity == m_city || !pLoopCity->isGovernmentCenter())
		{
			continue;
		}
		const int iDistance = plotDistance(m_city->getX(), m_city->getY(), pLoopCity->getX(), pLoopCity->getY());
		if (iNearest < 0 || iDistance < iNearest)
		{
			iNearest = iDistance;
		}
	}
	m_governmentCenterDistance = (iNearest > 0) ? iNearest : 0;
}

// ±1 from the fact that names the designation. ⛔ Never a recount: the store is zeroed at owner reset and every
// designation announces both ends, so the count IS the number of live designations at every instant.
void CityContext::changeHolyCityCount(int iChange) const
{
	m_holyCityCount += iChange;
	FASSERT_NOT_NEGATIVE(m_holyCityCount);
}

void CityContext::changeHeadquartersCount(int iChange) const
{
	m_headquartersCount += iChange;
	FASSERT_NOT_NEGATIVE(m_headquartersCount);
}


// The AMENITY reads are FORWARDS: the state is owned by CvCity's AmenityContext, which owns its storage, its
// maintenance and the facts that drive it in one place (Engine/AmenityContext.h). This context stores none of
// it and no caller reaches the amenity context THROUGH here.
bool CityContext::hasAmenity(int iAmenityId) const
{
	return m_city != NULL && m_city->amenity().has(iAmenityId);
}

int CityContext::amenityCount(int iAmenityId) const
{
	return m_city != NULL ? m_city->amenity().count(iAmenityId) : 0;
}

bool CityContext::hasAmenityKey(int& iIdCache, const char* szKey) const
{
	return m_city != NULL && m_city->amenity().hasKey(iIdCache, szKey);
}

// ---- the STORED reads: O(1) fetches over the blocks above -------------------------------------------------------

const ContextDict& CityContext::vicinityTier(CvCascVicinity eTier) const
{
	switch (eTier)
	{
	case CASC_VIC_OWNED:  return m_vicinityOwned;
	case CASC_VIC_WORKED: return m_vicinityWorked;
	default:              return m_vicinityAll;   // the composed bands are assembled by the caller below
	}
}

// ⚖ THE BANDS ARE CARVED OUT OF `all`, because NEUTRAL IS THE DEFAULT -- no owner means neutral (owner). So the
// residual `all − owned − foreign` IS the neutral count and nothing has to store or announce it.
bool CityContext::hasVicinityBonusAt(int eBonus, CvCascVicinity eTier) const
{
	if (eBonus < 0)
	{
		return false;
	}
	switch (eTier)
	{
	// crossBorder = ANY ownership, which is the total itself.
	case CASC_VIC_CROSSBORDER:
		return m_vicinityAll.has(eBonus);
	// The DEFAULT band: owned + neutral, NOT foreign -- i.e. everything except the foreign-owned tiles.
	// ⚠ Counted, not OR'd: two radius tiles can carry the same bonus under different ownership, so the test is
	// "is there a non-foreign one", which only the arithmetic answers.
	case CASC_VIC_NONE:
		return (m_vicinityAll.count(eBonus) - m_vicinityForeign.count(eBonus)) > 0;
	// The OBTAINED tier: on an owned radius tile AND actually reaching this city through the network. The second
	// half is the plot group's, forwarded -- never a second store here ([enabler.md] §8 RESIDENCY).
	case CASC_VIC_CONNECTED:
		return m_vicinityOwned.has(eBonus) && tradedBonusCount(eBonus) > 0;
	default:
		return vicinityTier(eTier).has(eBonus);
	}
}

// A radius tile's bonus moved, or its ownership moved it between partitions. THE ONE WRITE POINT.
void CityContext::applyVicinityBonus(int iBonus, CityVicinityPartition ePartition, int iSign) const
{
	if (iBonus < 0)
	{
		return;
	}
	switch (ePartition)
	{
	case CITYVIC_ALL:     m_vicinityAll.add(iBonus, iSign);     break;
	case CITYVIC_OWNED:   m_vicinityOwned.add(iBonus, iSign);   break;
	case CITYVIC_FOREIGN: m_vicinityForeign.add(iBonus, iSign); break;
	case CITYVIC_WORKED:  m_vicinityWorked.add(iBonus, iSign);  break;
	}
}

// The OBTAINED read composes the two stores, which is exactly the engine's own shape: its vicinity check is gated on
// the city HAVING the bonus at all before it looks at any tile, so a resource whose TechCityTrade gate is shut reads
// false even with the tile sitting in the radius. Composing at the read keeps the two derivations independent of
// each other's ordering.
bool CityContext::hasVicinityBonus(int eBonus) const
{
	// The OBTAINED semantic, composed once in hasVicinityBonusAt -- not restated here, or the two would be free
	// to drift ([DEC-single-implementation](../../docs/architecture/decisions.md)).
	return hasVicinityBonusAt(eBonus, CASC_VIC_CONNECTED);
}

// FORWARDED, never stored: the count belongs to the plot group, which owns it O(1), and the city relays it through
// its group pointer ([state-repositories.md]). A third copy here bought nothing and cost a sweep of every bonus on
// every fact that could move one, so what is forwarded is the city's own gated read.
int  CityContext::tradedBonusCount(int eBonus) const
{
	if (eBonus < 0 || m_city == NULL)
	{
		return 0;
	}
	return m_city->getNumBonuses((BonusTypes)eBonus);
}
int  CityContext::areaId() const                     { return m_areaId; }
int  CityContext::areaSize() const                   { return m_areaTileCount; }
int  CityContext::governmentCenterDistance() const   { return m_governmentCenterDistance; }
// A water body always has at least one tile, so the > 0 test is what distinguishes "no adjacent water at all" from a
// zero threshold -- without it a landlocked city would read coastal at iMinWaterSize 0.
bool CityContext::isCoastal(int iMinWaterSize) const { return m_maxAdjacentWaterTiles > 0 && m_maxAdjacentWaterTiles >= iMinWaterSize; }
bool CityContext::isHolyCityAny() const              { return m_holyCityCount > 0; }

// --- forwarding accessors: read the bound CvCity / its owner; no stored copy (owner: don't duplicate available state) ---
int  CityContext::population() const           { return m_city != NULL ? m_city->getPopulation() : 0; }
const OperatingBuildings* CityContext::operatingBuildings() const
{ return m_city != NULL ? &m_city->m_operatingBuildings : NULL; }
//  Power is an AMENITY (owner), so this reads the fold it lives in rather than forwarding to a counter.
int  CityContext::power() const                { return amenityCount(CLS_AMENITY_PROVIDES_POWER); }
bool CityContext::isPowered() const            { return m_city != NULL && m_city->isPower(); }
bool CityContext::hasReligion(int eReligion) const   { return m_city != NULL && m_city->isHasReligion((ReligionTypes)eReligion); }
bool CityContext::isHolyCityOf(int eReligion) const  { return m_city != NULL && m_city->isHolyCity((ReligionTypes)eReligion); }
bool CityContext::hasCorporation(int eCorp) const    { return m_city != NULL && m_city->isHasCorporation((CorporationTypes)eCorp); }
bool CityContext::hasActiveCorporation(int eCorp) const { return m_city != NULL && m_city->isActiveCorporation((CorporationTypes)eCorp); }
bool CityContext::isHeadquartersOf(int eCorp) const  { return m_city != NULL && m_city->isHeadquarters((CorporationTypes)eCorp); }
bool CityContext::isHeadquartersAny() const          { return m_headquartersCount > 0; }
bool CityContext::isCapital() const            { return m_city != NULL && m_city->isCapital(); }
bool CityContext::isGovernmentCenter() const   { return m_city != NULL && m_city->isGovernmentCenter(); }
bool CityContext::hasFreshWaterAccess() const  { return m_city != NULL && m_city->hasFreshWater(); }
int  CityContext::propertyValue(int eProperty) const
{
	if (m_city == NULL)
		return 0;
	return m_city->getPropertiesConst()->getValueByProperty((PropertyTypes)eProperty);
}
// The §3.7 `per` count domains. FORWARDS, not stores: the city already maintains its specialist
// population O(1), and the improved-plot count is asked of the city that owns the radius.
// ⚠ specialistCount answers the ASSIGNED population; the typed-free ledger is not folded in because
// the city no longer carries that member -- when it is restored, it is added HERE.
// ⚠ improvedPlotCount walks the city's radius per call. It is evaluated at package-rebuild and
// per-decision cadence, never on a read path, but the standing target is the id-keyed radius
// dictionary on this context ([contexts.md]) -- this is the read that wants it.
int  CityContext::specialistCount() const
{
	if (m_city == NULL)
		return 0;
	return m_city->getSpecialistPopulation();
}

int  CityContext::improvedPlotCount(int eImprovement) const
{
	if (m_city == NULL || eImprovement < 0)
		return 0;
	return m_city->countNumImprovedPlots((ImprovementTypes)eImprovement, false);
}

int  CityContext::ownCulturePercent() const
{
	if (m_city == NULL || m_city->plot() == NULL)
		return 0;
	return m_city->plot()->calculateCulturePercent(m_city->getOwner());
}
int  CityContext::owner() const                { return m_city != NULL ? (int)m_city->getOwner() : (int)NO_PLAYER; }
int  CityContext::team() const                 { return m_city != NULL ? (int)m_city->getTeam() : (int)NO_TEAM; }
const CvPlot* CityContext::cityPlot() const    { return m_city != NULL ? m_city->plot() : NULL; }
const CvPlot* CityContext::radiusPlot(int iRingIndex) const { return m_city != NULL ? m_city->getCityIndexPlot(iRingIndex) : NULL; }
bool CityContext::hasBuilding(int eBuilding) const   { return m_city != NULL && eBuilding >= 0 && m_city->hasBuilding((BuildingTypes)eBuilding); }
// MIN_INT when the city does not hold it -- getBuildingData answers that sentinel for an absent building, and a
// build YEAR is legitimately negative, so absence cannot be signalled by sign.
int  CityContext::buildingBuildYear(int eBuilding) const
{
	if (m_city == NULL || eBuilding < 0 || !m_city->hasBuilding((BuildingTypes)eBuilding))
	{
		return MIN_INT;
	}
	return m_city->getBuildingData((BuildingTypes)eBuilding).iTimeBuilt;
}
int  CityContext::stateReligion() const        { return m_city != NULL ? (int)GET_PLAYER(m_city->getOwner()).getStateReligion() : -1; }
bool CityContext::hasPolicy(int ePolicy) const { return m_city != NULL && GET_PLAYER(m_city->getOwner()).policies().has(ePolicy); }

// The realized-yield group forward: the bound city's own group read, handed on unchanged -- no store, no mirror, no
// second derivation (contexts.md STORES vs FORWARDS: forwarding raw data the object already holds O(1) is not
// duplication; storing a second copy of it would be). The out-array is FULLY DEFINED on every path, so an unbound
// context zero-fills rather than leaving caller memory untouched.
void CityContext::yields(int (&realizedYields)[NUM_YIELD_TYPES]) const
{
	if (m_city == NULL)
	{
		for (int iYield = 0; iYield < NUM_YIELD_TYPES; ++iYield)
		{
			realizedYields[iYield] = 0;
		}
		return;
	}
	m_city->getYields(realizedYields);
}

// The realized-commerce group forward: the bound city's own group read, handed on unchanged -- the split itself
// lives on the calc surface, so nothing about it is repeated here. The out-array is FULLY DEFINED on every path,
// so an unbound context zero-fills rather than leaving caller memory untouched.
void CityContext::commerces(int (&realizedCommerces)[NUM_COMMERCE_TYPES]) const
{
	if (m_city == NULL)
	{
		for (int iCommerce = 0; iCommerce < NUM_COMMERCE_TYPES; ++iCommerce)
		{
			realizedCommerces[iCommerce] = 0;
		}
		return;
	}
	m_city->getCommerces(realizedCommerces);
}

// Fill the CITY half of the eval ctx from the bound city (the context IS the eval state -- no raw pointer leaks to
// the caller; the ctx it fills is what the ONE evaluator reads). EmpireContext::fillEvalCtx fills player/team.
void CityContext::fillEvalCtx(CvCascadeEvalCtx& ec) const
{
	//	The ctx is handed the SILOS, never the objects holding them ([contexts.md]). A city binds its own plot's
	//	context alongside its own, because a city-scope evaluation reads plot facts AT the city centre.
	ec.cityContext = this;
	ec.plotContext = (m_city != NULL) ? &m_city->plot()->getPlotContext() : NULL;
	//	The ENABLER's derived output, fed in rather than re-derived: the ctx's active / obsolete / provided sets
	//	point at the city's own standing set (enabler.md §3.2), so a building-presence or vicinity-provides
	//	predicate reads the cascade-computed verdict and never the engine's ([DEC-calc-zero-ride-in]).
	const OperatingBuildings* pOperating = operatingBuildings();
	if (pOperating != NULL)
	{
		ec.activeBuildings = &pOperating->active;
		ec.obsoleteBuildings = &pOperating->obsolete;
		ec.vicinityProvidedBonuses = &pOperating->provided;
	}
}

// ============================================================================================================
//	THE STORE'S OWN SPINE CONSUMER -- storage, maintenance and the declared facts in ONE place (owner).
// ============================================================================================================

namespace
{
	// One buffered in-read membership fact: plot index + the assigned city's (owner, id).
	// ⚠ THE BUFFER IS AN ORDERING FACT, NOT A STALENESS MECHANISM. At load the map streams BEFORE the players, so
	// CvPlot::read announces a plot's working city while that city does not yet exist to fold into. The facts are
	// therefore held and applied ONCE after the stream ends -- the "apply once after the stream ends" option of
	// the enabler.md par.7.1 order rule, never the mixed form.
	// ⚑ The plot BITS need no such treatment: by then every plot has announced its own substrate facts and the
	// per-bit table has settled them, so the drain folds final values.
	struct CityContextMembershipFact
	{
		int iPlotIndex;
		int iOwner;
		int iCityId;
	};

	std::vector<CityContextMembershipFact> s_bufferedMembership;

	CvCity* cc_cityFor(int iOwner, int iCityId)
	{
		if (iOwner < 0 || iOwner >= MAX_PLAYERS || iCityId < 0)
		{
			return NULL;
		}
		return GET_PLAYER((PlayerTypes)iOwner).getCity(iCityId);
	}

	// A member plot's bit moved: it lands on the city that WORKS the plot, which is the city whose plotAttrs the
	// plot is a member of.
	void cc_applyPredicate(int iPlotIndex, int iPredicateId, int iSign)
	{
		if (iPlotIndex < 0)
		{
			return;
		}
		const CvPlot* pPlot = GC.getMap().plotByIndex(iPlotIndex);
		if (pPlot == NULL)
		{
			return;
		}
		const CvCity* pCity = pPlot->getWorkingCity();
		if (pCity != NULL)
		{
			pCity->getCityContext().onPlotPredicateChanged(iPredicateId, iSign);
		}
	}

	// ⚖ WHICH OWNERSHIP BAND a radius tile falls in FOR A GIVEN CITY. NEUTRAL is not a case: no owner means the
	// tile contributes to `all` alone, and the neutral count is the residual (owner).
	// ⚠ Returns whether an ownership band applies at all -- an unowned tile has none.
	bool cc_ownershipBand(int iPlotOwner, int iCityOwner, CityVicinityPartition& eBand)
	{
		if (iPlotOwner < 0)
		{
			return false;   // unowned == neutral == the residual
		}
		eBand = (iPlotOwner == iCityOwner) ? CITYVIC_OWNED : CITYVIC_FOREIGN;
		return true;
	}

	// ⚖ THE PLOT KNOWS WHICH CITIES MAY WORK IT -- read its own `workableBy` membership, never a radius inverse.
	// The CITY defines its potential work area (it GROWS with culture), and it hands that definition to the plots
	// as an ordinary membership; so this is an EXACT list, and a growing radius reaches the store as a fact rather
	// than as something a walk has to rediscover.
	template <class TAction>
	void cc_forEachWorkableCity(const CvPlot* pPlot, TAction action)
	{
		if (pPlot == NULL)
		{
			return;
		}
		const std::vector<IDInfo>& kCities = pPlot->workableByCities();
		for (std::vector<IDInfo>::const_iterator it = kCities.begin(); it != kCities.end(); ++it)
		{
			const CvCity* pCity = cc_cityFor((int)it->eOwner, it->iID);
			if (pCity != NULL)
			{
				action(pCity);
			}
		}
	}

	// A radius tile's BONUS arrived or left: it moves `all` for every city that can see it, plus whichever
	// ownership band that tile currently falls in for that city, plus the worked band if that city works it.
	struct ApplyVicinityBonusFromPlot
	{
		const CvPlot* pPlot;
		int iBonus;
		int iSign;
		void operator()(const CvCity* pCity) const
		{
			const CityContext& kContext = pCity->getCityContext();
			kContext.applyVicinityBonus(iBonus, CITYVIC_ALL, iSign);
			CityVicinityPartition eBand;
			if (cc_ownershipBand((int)pPlot->getOwner(), (int)pCity->getOwner(), eBand))
			{
				kContext.applyVicinityBonus(iBonus, eBand, iSign);
			}
			if (pPlot->getWorkingCity() == pCity && pPlot->isBeingWorked())
			{
				kContext.applyVicinityBonus(iBonus, CITYVIC_WORKED, iSign);
			}
		}
	};

	// A radius tile's OWNERSHIP moved: only the ownership BAND changes -- `all` does not, because the tile and its
	// bonus are still there. The fact NAMES the owner each half is about, which is what makes the withdrawal exact
	// (the plot's own `m_eOwner` has already moved by emit time).
	struct ApplyVicinityOwnerBand
	{
		int iBonus;
		int iPlotOwner;
		int iSign;
		void operator()(const CvCity* pCity) const
		{
			CityVicinityPartition eBand;
			if (cc_ownershipBand(iPlotOwner, (int)pCity->getOwner(), eBand))
			{
				pCity->getCityContext().applyVicinityBonus(iBonus, eBand, iSign);
			}
		}
	};

	// The plot's bonus, unfiltered by reveal: the vicinity store is per-CITY live state, not a per-team view.
	int cc_plotBonus(const CvPlot* pPlot)
	{
		return (pPlot != NULL) ? (int)pPlot->getBonusType(NO_TEAM) : (int)NO_BONUS;
	}

	// A plot ENTERED / LEFT ONE city's work area -- the membership fact's whole job. Folds that plot's bonus into
	// that city alone: no radius walk, no re-derivation, and the direction comes from the fact's own id.
	// ⚡ THIS IS ALSO THE SEEDING PATH. A city establishing its work area (born, or rebuilt at load) announces one
	// membership fact per plot, so the store fills through the SAME route that maintains it -- there is no separate
	// build pass, and no second mechanism beside the event stream ([DEC-spine-reseed]).
	void cc_applyVicinityMembership(const CvPlot* pPlot, const CvCity* pCity, int iSign)
	{
		if (pPlot == NULL || pCity == NULL)
		{
			return;
		}
		const int iBonus = cc_plotBonus(pPlot);
		if (iBonus < 0)
		{
			return;
		}
		const CityContext& kContext = pCity->getCityContext();
		kContext.applyVicinityBonus(iBonus, CITYVIC_ALL, iSign);
		CityVicinityPartition eBand;
		if (cc_ownershipBand((int)pPlot->getOwner(), (int)pCity->getOwner(), eBand))
		{
			kContext.applyVicinityBonus(iBonus, eBand, iSign);
		}
		if (pPlot->getWorkingCity() == pCity && pPlot->isBeingWorked())
		{
			kContext.applyVicinityBonus(iBonus, CITYVIC_WORKED, iSign);
		}
	}

	void cc_changeHolyCityCount(int iOwner, int iCityId, int iChange)
	{
		const CvCity* pCity = cc_cityFor(iOwner, iCityId);
		if (pCity != NULL)
		{
			pCity->getCityContext().changeHolyCityCount(iChange);
		}
	}

	void cc_changeHeadquartersCount(int iOwner, int iCityId, int iChange)
	{
		const CvCity* pCity = cc_cityFor(iOwner, iCityId);
		if (pCity != NULL)
		{
			pCity->getCityContext().changeHeadquartersCount(iChange);
		}
	}

	// Every city of ONE player re-measures. A city gaining or losing government-centre status moves the answer for
	// all of its siblings, never only for itself.
	void cc_refreshGovernmentCenterDistanceForPlayer(int iOwner)
	{
		if (iOwner < 0 || iOwner >= MAX_PLAYERS)
		{
			return;
		}
		CvPlayer& kPlayer = GET_PLAYER((PlayerTypes)iOwner);
		if (!kPlayer.isAlive())
		{
			return;
		}
		int iLoop = 0;
		for (const CvCity* pCity = kPlayer.firstCity(&iLoop); pCity != NULL; pCity = kPlayer.nextCity(&iLoop))
		{
			pCity->getCityContext().refreshGovernmentCenterDistance();
		}
	}

	// The RADIUS-CITY INVERSE: which cities can see this plot. The workable fat cross is symmetric, so the cities
	// that may hold this plot in radius sit at exactly the same offsets around it. Deliberately over-inclusive --
	// a candidate whose radius has since shrunk simply re-derives its own facts and finds nothing changed.
	void cc_refreshAreaFactsAroundPlot(int iPlotIndex)
	{
		if (iPlotIndex < 0)
		{
			return;
		}
		const CvPlot* pPlot = GC.getMap().plotByIndex(iPlotIndex);
		if (pPlot == NULL)
		{
			return;
		}
		for (int iRingIndex = 0; iRingIndex < NUM_CITY_PLOTS; ++iRingIndex)
		{
			const CvPlot* pCandidatePlot = plotCity(pPlot->getX(), pPlot->getY(), iRingIndex);
			if (pCandidatePlot == NULL)
			{
				continue;
			}
			const CvCity* pCity = pCandidatePlot->getPlotCity();
			if (pCity != NULL)
			{
				pCity->getCityContext().refreshAreaFacts();
			}
		}
	}

	void cc_refreshAreaFactsForAllCities()
	{
		for (int iPlayer = 0; iPlayer < MAX_PLAYERS; ++iPlayer)
		{
			CvPlayer& kPlayer = GET_PLAYER((PlayerTypes)iPlayer);
			if (!kPlayer.isAlive())
			{
				continue;
			}
			int iLoop = 0;
			for (const CvCity* pCity = kPlayer.firstCity(&iLoop); pCity != NULL; pCity = kPlayer.nextCity(&iLoop))
			{
				pCity->getCityContext().refreshAreaFacts();
			}
		}
	}

	class CityContextSpineConsumer : public IEventConsumer
	{
	public:
		int wantedKinds() const { return (1 << EVENTKIND_DOMAIN); }
		void onEvent(const CvSpineEvent& kEvent) { CityContext::onSpineEvent(kEvent); }
	};

	CityContextSpineConsumer s_cityContextConsumer;
	bool s_bCityContextRegistered = false;
}

// ⚖ THE DECLARED INTEREST SET. Everything that maintains this store is named here, at the store.
bool CityContext::wantsEvent(int iEventId)
{
	switch (iEventId)
	{
	// A MEMBER PLOT'S OWN BIT crossed -- the ±1 into plotAttrs. This is the route the per-bit fact exists for.
	case SEVT_PLOT_PREDICATE_ADDED:
	case SEVT_PLOT_PREDICATE_REMOVED:
	// MEMBERSHIP: a plot entered or left this city's workable set -- fold its whole current block.
	case SEVT_PLOT_WORKING_CITY_ADDED:
	case SEVT_PLOT_WORKING_CITY_REMOVED:
	// THE VICINITY STORE -- the MAP half of the json §5a supply, fed ±1 per fact and never by a radius walk.
	// (The BUILDING half stays the enabler's operate/provides fixpoint; the reader unions the two.)
	case SEVT_PLOT_BONUS_ADDED:
	case SEVT_PLOT_BONUS_REMOVED:
	case SEVT_PLOT_OWNER_ADDED:
	case SEVT_PLOT_OWNER_REMOVED:
	case SEVT_PLOT_WORKED_ADDED:
	case SEVT_PLOT_WORKED_REMOVED:
	// A land/ocean flip moves the water-body size the neighbourhood's cities read as their coastal facts.
	case SEVT_PLOT_TYPE_ADDED:
	case SEVT_PLOT_TYPE_REMOVED:
	// The designation counts, applied ±1 by the fact that NAMES the designation -- never re-derived.
	case SEVT_CITY_HOLY_CITY_ADDED:
	case SEVT_CITY_HOLY_CITY_REMOVED:
	case SEVT_CITY_HEADQUARTERS_ADDED:
	case SEVT_CITY_HEADQUARTERS_REMOVED:
	// A government centre appeared or went: every city of that player re-measures its distance to the nearest one.
	// ⚑ BOTH directions drive the SAME re-measure, and that is not a discriminator the fact has lost: the answer
	// is a MINIMUM over the player's centres, so gaining one and losing one both move it and neither direction
	// tells a city what its new distance is. The fact says WHICH PLAYER's centre set moved; the re-measure says
	// what that means per city.
	case SEVT_CITY_GOVERNMENT_CENTER_ADDED:
	case SEVT_CITY_GOVERNMENT_CENTER_REMOVED:
	// Every area id was reassigned, so every city re-reads its area facts. Rare by construction and not
	// addressable per-source, which is why it is announced wholesale rather than being a self-heal.
	case SEVT_AREAS_RECALCULATED:
	// A plot ENTERED / LEFT a city's potential work area -- the membership the city defines, and both the
	// maintenance and the seeding path for the vicinity store.
	case SEVT_PLOT_WORKABLE_BY_ADDED:
	case SEVT_PLOT_WORKABLE_BY_REMOVED:
	// A new city has no derived blocks yet.
	case SEVT_CITY_FOUNDED:
	// The load bracket: buffer the membership facts, drain them once the stream has ended.
	case SEVT_GAME_LOAD_STARTED:
	case SEVT_GAME_LOAD_FINISHED:
		return true;
	default:
		return false;
	}
}

void CityContext::onSpineEvent(const CvSpineEvent& kEvent)
{
	if (!wantsEvent(kEvent.iEventId))
	{
		return;
	}
	switch (kEvent.iEventId)
	{
	// ⛔ Dropped INSIDE the load bracket, deliberately: the cities do not exist yet while the map streams, and the
	// membership drain below folds each plot's FINAL block once they do. Applying here would fold against a city
	// that is not there and then fold the same bits again at the drain.
	case SEVT_PLOT_PREDICATE_ADDED:
		if (!spineGameLoadInProgress())
		{
			cc_applyPredicate(kEvent.iSrcLoc, kEvent.iType, +1);
		}
		break;
	case SEVT_PLOT_PREDICATE_REMOVED:
		if (!spineGameLoadInProgress())
		{
			cc_applyPredicate(kEvent.iSrcLoc, kEvent.iType, -1);
		}
		break;

	case SEVT_PLOT_WORKING_CITY_ADDED:
	case SEVT_PLOT_WORKING_CITY_REMOVED:
	{
		const int iSign = (kEvent.iEventId == SEVT_PLOT_WORKING_CITY_ADDED) ? +1 : -1;
		if (spineGameLoadInProgress())
		{
			// Only the ASSIGNMENT is buffered: a deserializing plot has no prior working city to unfold.
			if (iSign > 0 && kEvent.iA >= 0 && kEvent.iC >= 0 && kEvent.iC < MAX_PLAYERS && kEvent.iSrcLoc >= 0)
			{
				CityContextMembershipFact kFact;
				kFact.iPlotIndex = kEvent.iSrcLoc;
				kFact.iOwner = kEvent.iC;
				kFact.iCityId = kEvent.iA;
				s_bufferedMembership.push_back(kFact);
			}
			break;
		}
		CvCity* pCity = cc_cityFor(kEvent.iC, kEvent.iA);
		const CvPlot* pPlot = (kEvent.iSrcLoc >= 0) ? GC.getMap().plotByIndex(kEvent.iSrcLoc) : NULL;
		if (pCity != NULL && pPlot != NULL)
		{
			pCity->onCityPlotChanged(pPlot, iSign);
		}
		break;
	}

	case SEVT_PLOT_TYPE_ADDED:
	case SEVT_PLOT_TYPE_REMOVED:
		cc_refreshAreaFactsAroundPlot(kEvent.iSrcLoc);
		break;

	// The bonus itself arrived / left. ⚠ The REMOVED fact is emitted while the OLD state still holds, so the
	// tile's ownership and worked state are exactly what the contribution was booked against
	// ([state-repositories.md] § THE INVARIANT) -- the withdrawal resolves against what it deposited.
	case SEVT_PLOT_BONUS_ADDED:
	case SEVT_PLOT_BONUS_REMOVED:
	{
		const CvPlot* pPlot = (kEvent.iSrcLoc >= 0) ? GC.getMap().plotByIndex(kEvent.iSrcLoc) : NULL;
		if (pPlot != NULL && kEvent.iType >= 0)
		{
			ApplyVicinityBonusFromPlot kApply;
			kApply.pPlot = pPlot;
			kApply.iBonus = kEvent.iType;
			kApply.iSign = (kEvent.iEventId == SEVT_PLOT_BONUS_ADDED) ? +1 : -1;
			cc_forEachWorkableCity(pPlot, kApply);
		}
		break;
	}

	// Ownership moved the tile between BANDS. `all` does not move -- the tile and its bonus are still there.
	case SEVT_PLOT_OWNER_ADDED:
	case SEVT_PLOT_OWNER_REMOVED:
	{
		const CvPlot* pPlot = (kEvent.iSrcLoc >= 0) ? GC.getMap().plotByIndex(kEvent.iSrcLoc) : NULL;
		const int iBonus = cc_plotBonus(pPlot);
		if (pPlot != NULL && iBonus >= 0)
		{
			ApplyVicinityOwnerBand kApply;
			kApply.iBonus = iBonus;
			kApply.iPlotOwner = kEvent.iC;   // the owner THIS half of the pair is about
			kApply.iSign = (kEvent.iEventId == SEVT_PLOT_OWNER_ADDED) ? +1 : -1;
			cc_forEachWorkableCity(pPlot, kApply);
		}
		break;
	}

	// A citizen took or left the tile: only the WORKED band moves, and only for the city that works it -- a tile
	// is worked by exactly one city, so this hottest of the plot facts costs one city, never a neighbourhood.
	case SEVT_PLOT_WORKED_ADDED:
	case SEVT_PLOT_WORKED_REMOVED:
	{
		const CvPlot* pPlot = (kEvent.iSrcLoc >= 0) ? GC.getMap().plotByIndex(kEvent.iSrcLoc) : NULL;
		const int iBonus = cc_plotBonus(pPlot);
		const CvCity* pCity = cc_cityFor(kEvent.iC, kEvent.iB);
		if (pCity != NULL && iBonus >= 0)
		{
			pCity->getCityContext().applyVicinityBonus(iBonus, CITYVIC_WORKED,
				(kEvent.iEventId == SEVT_PLOT_WORKED_ADDED) ? +1 : -1);
		}
		break;
	}

	case SEVT_CITY_HOLY_CITY_ADDED:      cc_changeHolyCityCount(kEvent.iC, kEvent.iSrcLoc, +1); break;
	case SEVT_CITY_HOLY_CITY_REMOVED:    cc_changeHolyCityCount(kEvent.iC, kEvent.iSrcLoc, -1); break;
	case SEVT_CITY_HEADQUARTERS_ADDED:   cc_changeHeadquartersCount(kEvent.iC, kEvent.iSrcLoc, +1); break;
	case SEVT_CITY_HEADQUARTERS_REMOVED: cc_changeHeadquartersCount(kEvent.iC, kEvent.iSrcLoc, -1); break;

	case SEVT_CITY_GOVERNMENT_CENTER_ADDED:
	case SEVT_CITY_GOVERNMENT_CENTER_REMOVED:
		cc_refreshGovernmentCenterDistanceForPlayer(kEvent.iC);
		break;

	case SEVT_AREAS_RECALCULATED:
		cc_refreshAreaFactsForAllCities();
		break;

	case SEVT_CITY_FOUNDED:
	{
		const CvCity* pCity = cc_cityFor(kEvent.iC, kEvent.iSrcLoc);
		if (pCity != NULL)
		{
			// ⛔ The holy-city and headquarters counts are NOT built here, and adding them would be a bug: both
			// are delta stores fed ±1 by their own facts, which a founding city has none of yet. A build pass
			// beside a delta store double-counts.
			pCity->getCityContext().refreshAreaFacts();
			pCity->getCityContext().refreshGovernmentCenterDistance();
		}
		break;
	}

	// ⚑ ONE ROUTE, both jobs. A city establishing its work area announces one of these per plot, so the store is
	// SEEDED through exactly the path that maintains it -- at birth, on a culture level-up, and at load. There is
	// no separate build pass to keep in step, and no ordering window to get right.
	case SEVT_PLOT_WORKABLE_BY_ADDED:
	case SEVT_PLOT_WORKABLE_BY_REMOVED:
		cc_applyVicinityMembership(
			(kEvent.iSrcLoc >= 0) ? GC.getMap().plotByIndex(kEvent.iSrcLoc) : NULL,
			cc_cityFor(kEvent.iC, kEvent.iA),
			(kEvent.iEventId == SEVT_PLOT_WORKABLE_BY_ADDED) ? +1 : -1);
		break;

	case SEVT_GAME_LOAD_STARTED:
		s_bufferedMembership.clear();   // a fresh load: no stale facts from a previous stream
		break;

	case SEVT_GAME_LOAD_FINISHED:
	{
		// THE DRAIN. Every buffered membership fact folds through the ONE applier against the fully-read map and
		// cities, and against plot blocks the per-bit table has already settled. Unresolvable facts (a city razed
		// mid-read) drop -- the same not-present convention the enabler's cityForEvent uses.
		for (size_t iFact = 0; iFact < s_bufferedMembership.size(); ++iFact)
		{
			const CityContextMembershipFact& kFact = s_bufferedMembership[iFact];
			CvCity* pCity = cc_cityFor(kFact.iOwner, kFact.iCityId);
			const CvPlot* pPlot = GC.getMap().plotByIndex(kFact.iPlotIndex);
			if (pCity != NULL && pPlot != NULL)
			{
				pCity->onCityPlotChanged(pPlot, +1);
			}
		}
		s_bufferedMembership.clear();

		// The load build of the blocks that read state only complete once the WHOLE stream has ended: the areas
		// deserialize after the plots. LOAD is the only full build there is (state-repositories.md CAPSTONE) --
		// after this the facts alone maintain them and nothing sweeps them again.
		for (int iPlayer = 0; iPlayer < MAX_PLAYERS; ++iPlayer)
		{
			CvPlayer& kPlayer = GET_PLAYER((PlayerTypes)iPlayer);
			if (!kPlayer.isAlive())
			{
				continue;
			}
			int iLoop = 0;
			for (const CvCity* pCity = kPlayer.firstCity(&iLoop); pCity != NULL; pCity = kPlayer.nextCity(&iLoop))
			{
				pCity->getCityContext().refreshAreaFacts();
				pCity->getCityContext().refreshGovernmentCenterDistance();
				// The map streamed BEFORE the cities, so no plot could announce a membership to a city that did
				// not exist. Establishing the work area now fires those facts, and the vicinity store fills
				// through the ordinary route -- not a second build mechanism beside the event stream.
				pCity->changeWorkableArea(0, pCity->getNumCityPlots());
			}
		}
		break;
	}
	default:
		break;
	}
}

void cityContextRegisterConsumer()
{
	if (s_bCityContextRegistered)
	{
		return;
	}
	s_bCityContextRegistered = true;
	eventSpine().registerConsumer(&s_cityContextConsumer);
}
