#include "CvGameCoreDLL.h"
#include "CityContext.h"
#include "CvPlot.h"
#include "CvCity.h"
#include "AI/CvPlayerAI.h"      // GET_PLAYER (the owner forward: state religion / policies)
#include "EmpireContext.h"      // the owner's empire aggregate (policies)
#include "CvCondition.h"    // CASC_PRED_* -- the shared HAS_/IS_ plot predicate ids plotAttrs keys on
#include "Conditions/CvConditionEval.h"   // CvCascadeEvalCtx -- fillEvalCtx

void CityContext::onPlotChanged(const CvPlot* plot, int sign)
{
	if (plot == NULL)
		return;
	// Fold each stable HAS_/IS_ attribute the plot carries into plotAttrs (+1 on enter, -1 on leave). COUNTS only --
	// the plot itself is never stored. Terrain-level attributes that change only on plot enter/leave; mutable ones
	// (feature / irrigation / worked) join once their own change-events are wired.
	const bool bWater = plot->isWater();
	const bool bHills = plot->isHills();
	const bool bPeak  = plot->isPeak();
	if (plot->isRiver())      plotAttrs.add(CASC_PRED_HAS_RIVER,      sign);
	if (bHills)               plotAttrs.add(CASC_PRED_HAS_HILLS,      sign);
	if (bPeak)                plotAttrs.add(CASC_PRED_HAS_PEAK,       sign);
	if (plot->isFreshWater()) plotAttrs.add(CASC_PRED_HAS_FRESHWATER, sign);
	if (bWater)               plotAttrs.add(CASC_PRED_IS_WATER,       sign);
	else                      plotAttrs.add(CASC_PRED_IS_LAND,        sign);
	if (!bHills && !bPeak)    plotAttrs.add(CASC_PRED_IS_FLATLANDS,   sign);   // relief-free (water is relief-free too, json §3.5)
}

// --- forwarding accessors: read the bound CvCity / its owner; no stored copy (owner: don't duplicate available state) ---
int  CityContext::population() const           { return m_city != NULL ? m_city->getPopulation() : 0; }
int  CityContext::power() const                { return m_city != NULL ? m_city->getPowerCount() : 0; }
bool CityContext::hasReligion(int eReligion) const   { return m_city != NULL && m_city->isHasReligion((ReligionTypes)eReligion); }
bool CityContext::isHolyCityOf(int eReligion) const  { return m_city != NULL && m_city->isHolyCity((ReligionTypes)eReligion); }
bool CityContext::isHolyCityAny() const              { return m_city != NULL && m_city->isHolyCity(); }
bool CityContext::hasCorporation(int eCorp) const    { return m_city != NULL && m_city->isHasCorporation((CorporationTypes)eCorp); }
bool CityContext::hasVicinityBonus(int eBonus) const { return m_city != NULL && m_city->hasVicinityBonus((BonusTypes)eBonus); }
int  CityContext::stateReligion() const        { return m_city != NULL ? (int)GET_PLAYER(m_city->getOwner()).getStateReligion() : -1; }
bool CityContext::hasPolicy(int ePolicy) const { return m_city != NULL && GET_PLAYER(m_city->getOwner()).getEmpireContext().policies.has(ePolicy); }

// Fill the CITY half of the eval ctx from the bound city (the context IS the eval state -- no raw pointer leaks to
// the caller; the ctx it fills is what the ONE evaluator reads). EmpireContext::fillEvalCtx fills player/team.
void CityContext::fillEvalCtx(CvCascadeEvalCtx& ec) const
{
	ec.city = m_city;
	ec.plot = (m_city != NULL) ? m_city->plot() : NULL;
}
