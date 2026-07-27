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
	m_tradedBonuses.clear();
	m_areaId = -1;
	m_areaTileCount = 0;
	m_maxAdjacentWaterTiles = 0;
	m_holyCityCount = 0;
}

// ---- the ONE derivation per stored block ------------------------------------------------------------------------

// Re-derive the MAP-provider vicinity presence over the city's CURRENT workable radius (which grows with culture, so
// the plot count is read live rather than assumed). A plot carries at most ONE bonus, so this is one cheap pass.
// The two reveal semantics below are the ones the two existing read paths already used, preserved exactly:
// the loose ownership tiers read the bonus as revealed to THIS city's team, while the CONNECTED tier reads the
// plot's own revealed bonus -- the engine's obtained-vicinity form.
void CityContext::refreshVicinityBonuses() const
{
	m_vicinityOwned.clear();
	m_vicinityNeutral.clear();
	m_vicinityForeign.clear();
	m_vicinityWorked.clear();
	m_vicinityConnected.clear();
	if (m_city == NULL)
	{
		return;
	}
	const int iCityOwner = (int)m_city->getOwner();
	const TeamTypes eCityTeam = m_city->getTeam();
	const CvPlot* pCentrePlot = m_city->plot();
	const int iNumCityPlots = m_city->getNumCityPlots();

	for (int iRingIndex = 0; iRingIndex < iNumCityPlots; ++iRingIndex)
	{
		const CvPlot* pRadiusPlot = m_city->getCityIndexPlot(iRingIndex);
		if (pRadiusPlot == NULL)
		{
			continue;
		}
		const bool bCentre = (pRadiusPlot == pCentrePlot);
		const int iPlotOwner = (int)pRadiusPlot->getOwner();

		const int eRevealedBonus = (int)pRadiusPlot->getBonusType(eCityTeam);
		if (eRevealedBonus != (int)NO_BONUS)
		{
			if (bCentre || iPlotOwner == iCityOwner)
			{
				m_vicinityOwned.add(eRevealedBonus, 1);
			}
			else if (iPlotOwner == (int)NO_PLAYER)
			{
				m_vicinityNeutral.add(eRevealedBonus, 1);
			}
			else
			{
				m_vicinityForeign.add(eRevealedBonus, 1);
			}
			if (bCentre || pRadiusPlot->isBeingWorked())
			{
				m_vicinityWorked.add(eRevealedBonus, 1);
			}
		}

		// The OBTAINED tier: owned + a valid (improved, revealed, networked) bonus + connected to this city.
		const int eOwnBonus = (int)pRadiusPlot->getBonusType();
		if (eOwnBonus == (int)NO_BONUS)
		{
			continue;
		}
		if (bCentre)
		{
			m_vicinityConnected.add(eOwnBonus, 1);
		}
		else if (iPlotOwner == iCityOwner && pRadiusPlot->isHasValidBonus() && pRadiusPlot->isConnectedTo(m_city))
		{
			m_vicinityConnected.add(eOwnBonus, 1);
		}
	}
}

// Re-derive the gated network count. The city's own m_paiNumBonuses is already an O(1) maintained number; what was
// DERIVED per read is the pair of gates over it -- the bonus's TechCityTrade tech and the player's minted-percent
// suppression -- so the gated result is what is stored. Re-read from source on every fact rather than accumulated as
// a delta, so a fact whose payload cannot express the change (a presence-only transition) still lands the true value.
void CityContext::refreshTradedBonuses() const
{
	m_tradedBonuses.clear();
	if (m_city == NULL)
	{
		return;
	}
	const int iNumBonuses = GC.getNumBonusInfos();
	for (int eBonus = 0; eBonus < iNumBonuses; ++eBonus)
	{
		const int iCount = m_city->getNumBonuses((BonusTypes)eBonus);
		if (iCount != 0)
		{
			m_tradedBonuses.set(eBonus, iCount);
		}
	}
}

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
	return eBonus >= 0 && m_tradedBonuses.has(eBonus) && hasVicinityBonusAt(eBonus, CASC_VIC_CONNECTED);
}

int  CityContext::tradedBonusCount(int eBonus) const { return eBonus >= 0 ? m_tradedBonuses.count(eBonus) : 0; }
int  CityContext::areaId() const                     { return m_areaId; }
int  CityContext::areaSize() const                   { return m_areaTileCount; }
// A water body always has at least one tile, so the > 0 test is what distinguishes "no adjacent water at all" from a
// zero threshold -- without it a landlocked city would read coastal at iMinWaterSize 0.
bool CityContext::isCoastal(int iMinWaterSize) const { return m_maxAdjacentWaterTiles > 0 && m_maxAdjacentWaterTiles >= iMinWaterSize; }
bool CityContext::isHolyCityAny() const              { return m_holyCityCount > 0; }

// --- forwarding accessors: read the bound CvCity / its owner; no stored copy (owner: don't duplicate available state) ---
int  CityContext::population() const           { return m_city != NULL ? m_city->getPopulation() : 0; }
int  CityContext::power() const                { return m_city != NULL ? m_city->getPowerCount() : 0; }
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
int  CityContext::stateReligion() const        { return m_city != NULL ? (int)GET_PLAYER(m_city->getOwner()).getStateReligion() : -1; }
bool CityContext::hasPolicy(int ePolicy) const { return m_city != NULL && GET_PLAYER(m_city->getOwner()).getEmpireContext().policies.has(ePolicy); }

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
	ec.city = m_city;
	ec.plot = (m_city != NULL) ? m_city->plot() : NULL;
}
