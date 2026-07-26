#include "CvGameCoreDLL.h"
#include "CityContext.h"
#include "CvPlot.h"
#include "CvCity.h"
#include "CvArea.h"             // areaSize -- the AREA_SIZE counter forward
#include "CvProperties.h"       // propertyValue -- the PROPERTY_ band read forward
#include "AI/CvPlayerAI.h"      // GET_PLAYER (the owner forward: state religion / policies)
#include "EmpireContext.h"      // the owner's empire aggregate (policies)
#include "CvCondition.h"    // CASC_PRED_* -- the shared HAS_/IS_ plot predicate ids plotAttrs keys on
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

// --- forwarding accessors: read the bound CvCity / its owner; no stored copy (owner: don't duplicate available state) ---
int  CityContext::population() const           { return m_city != NULL ? m_city->getPopulation() : 0; }
int  CityContext::power() const                { return m_city != NULL ? m_city->getPowerCount() : 0; }
bool CityContext::isPowered() const            { return m_city != NULL && m_city->isPower(); }
bool CityContext::hasReligion(int eReligion) const   { return m_city != NULL && m_city->isHasReligion((ReligionTypes)eReligion); }
bool CityContext::isHolyCityOf(int eReligion) const  { return m_city != NULL && m_city->isHolyCity((ReligionTypes)eReligion); }
bool CityContext::isHolyCityAny() const              { return m_city != NULL && m_city->isHolyCity(); }
bool CityContext::hasCorporation(int eCorp) const    { return m_city != NULL && m_city->isHasCorporation((CorporationTypes)eCorp); }
bool CityContext::hasActiveCorporation(int eCorp) const { return m_city != NULL && m_city->isActiveCorporation((CorporationTypes)eCorp); }
bool CityContext::isHeadquartersOf(int eCorp) const  { return m_city != NULL && m_city->isHeadquarters((CorporationTypes)eCorp); }
bool CityContext::isHeadquartersAny() const          { return m_city != NULL && m_city->isHeadquarters(); }
bool CityContext::hasVicinityBonus(int eBonus) const { return m_city != NULL && m_city->hasVicinityBonus((BonusTypes)eBonus); }
int  CityContext::tradedBonusCount(int eBonus) const { return m_city != NULL ? m_city->getNumBonuses((BonusTypes)eBonus) : 0; }
bool CityContext::isCapital() const            { return m_city != NULL && m_city->isCapital(); }
bool CityContext::isGovernmentCenter() const   { return m_city != NULL && m_city->isGovernmentCenter(); }
bool CityContext::hasFreshWaterAccess() const  { return m_city != NULL && m_city->hasFreshWater(); }
bool CityContext::isCoastal(int iMinWaterSize) const { return m_city != NULL && m_city->isCoastal(iMinWaterSize); }
int  CityContext::propertyValue(int eProperty) const
{
	if (m_city == NULL)
		return 0;
	return m_city->getPropertiesConst()->getValueByProperty((PropertyTypes)eProperty);
}
int  CityContext::areaSize() const
{
	if (m_city == NULL || m_city->area() == NULL)
		return 0;
	return m_city->area()->getNumTiles();
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

// Fill the CITY half of the eval ctx from the bound city (the context IS the eval state -- no raw pointer leaks to
// the caller; the ctx it fills is what the ONE evaluator reads). EmpireContext::fillEvalCtx fills player/team.
void CityContext::fillEvalCtx(CvCascadeEvalCtx& ec) const
{
	ec.city = m_city;
	ec.plot = (m_city != NULL) ? m_city->plot() : NULL;
}
