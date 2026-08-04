//
//	CityContext -- the ONE derivation of the city's stored blocks (see the header for what each block is and the
//	vicinity split against the enabler), plus the forwards for the raw data CvCity already holds O(1).
//
//	DEC-single-implementation: each stored fact is derived by calling the SAME engine accessor a read used to call --
//	once, at maintenance time, instead of once per read. No predicate's logic is re-implemented here.
//
//	CONSTRAINT: refreshAreaFacts reads the CENTRE plot's neighbours' CvArea, so it is valid only once the map is
//	settled -- CvPlot::area() is NULL for a plot whose area has not been assigned. The maintainer
//	(Engine/ContextConsumer) holds that guarantee, and the area-recalculated fact re-runs it after a wholesale
//	reassignment. There is deliberately NO staleness check on the read side: a fact that fails to fire leaves the
//	value visibly wrong rather than being silently rebuilt (DEC-no-self-heal).
//

#include "CvGameCoreDLL.h"
#include "CityContext.h"
#include "CvPlot.h"
#include "CvCity.h"
#include "CvArea.h"             // refreshAreaFacts -- the area tile count the AREA_SIZE read is served from
#include "CvProperties.h"       // propertyValue -- the PROPERTY_ band read forward
#include "CvGameCoreUtils.h"    // plotDirection -- the centre plot's 8 neighbours (the coastal water-body scan)
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
	// derivation here (the plot's own maintainer keeps the bits current, Engine/ContextConsumer).
	const unsigned int attributeBits = plot->getPlotContext().attributeBits();
	for (int predicateId = 0; predicateId < 32; ++predicateId)
	{
		if ((attributeBits & (1u << predicateId)) != 0)
		{
			plotAttrs.add(predicateId, sign);
		}
	}
}

void CityContext::clear() const
{
	plotAttrs.clear();
	m_vicinityOwned.clear();
	m_vicinityNeutral.clear();
	m_vicinityForeign.clear();
	m_vicinityWorked.clear();
	m_vicinityConnected.clear();
	m_areaId = -1;
	m_areaTileCount = 0;
	m_governmentCenterDistance = 0;
	m_maxAdjacentWaterTiles = 0;
	m_holyCityCount = 0;
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

void CityContext::refreshHolyCity() const
{
	m_holyCityCount = 0;
	if (m_city == NULL)
	{
		return;
	}
	const int iNumReligions = GC.getNumReligionInfos();
	for (int eReligion = 0; eReligion < iNumReligions; ++eReligion)
	{
		if (m_city->isHolyCity((ReligionTypes)eReligion))
		{
			++m_holyCityCount;
		}
	}
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
	case CASC_VIC_OWNED:     return m_vicinityOwned;
	case CASC_VIC_WORKED:    return m_vicinityWorked;
	case CASC_VIC_CONNECTED: return m_vicinityConnected;
	default:                 return m_vicinityOwned;   // the composed tiers are assembled by the caller below
	}
}

bool CityContext::hasVicinityBonusAt(int eBonus, CvCascVicinity eTier) const
{
	if (eBonus < 0)
	{
		return false;
	}
	switch (eTier)
	{
	// The nesting owned ⊂ owned+neutral ⊂ crossBorder is composed at the read, so no tier stores a redundant copy.
	case CASC_VIC_CROSSBORDER:
		return m_vicinityOwned.has(eBonus) || m_vicinityNeutral.has(eBonus) || m_vicinityForeign.has(eBonus);
	case CASC_VIC_NONE:
		return m_vicinityOwned.has(eBonus) || m_vicinityNeutral.has(eBonus);   // the DEFAULT: owned + neutral, NOT foreign
	default:
		return vicinityTier(eTier).has(eBonus);
	}
}

// The OBTAINED read composes the two stores, which is exactly the engine's own shape: its vicinity check is gated on
// the city HAVING the bonus at all before it looks at any tile, so a resource whose TechCityTrade gate is shut reads
// false even with the tile sitting in the radius. Composing at the read keeps the two derivations independent of
// each other's ordering.
bool CityContext::hasVicinityBonus(int eBonus) const
{
	return eBonus >= 0 && tradedBonusCount(eBonus) > 0 && hasVicinityBonusAt(eBonus, CASC_VIC_CONNECTED);
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
bool CityContext::isHeadquartersAny() const          { return m_city != NULL && m_city->isHeadquarters(); }
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
