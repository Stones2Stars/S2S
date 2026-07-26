#include "CvGameCoreDLL.h"
#include "PlotContext.h"
#include "CvPlot.h"

// forwarding accessors: every plot fact is already O(1) on CvPlot -- read it through, no stored copy (owner: don't
// duplicate available state). The one condition evaluator (CvConditionEval.cpp CASC_PRED_*) reads each HAS_/IS_
// plot fact THROUGH these forwards, so every fact has ONE home (the contexts.md HAVE axis).
bool PlotContext::isWater() const        { return m_plot != NULL && m_plot->isWater(); }
bool PlotContext::isLand() const         { return m_plot != NULL && !m_plot->isWater(); }
bool PlotContext::isFlatlands() const    { return m_plot != NULL && !m_plot->isHills() && !m_plot->isPeak(); }
bool PlotContext::hasHills() const       { return m_plot != NULL && m_plot->isHills(); }
bool PlotContext::hasPeak() const        { return m_plot != NULL && m_plot->isPeak(); }
bool PlotContext::hasCoast() const       { return m_plot != NULL && m_plot->isCoastalLand(); }
bool PlotContext::hasRiver() const       { return m_plot != NULL && m_plot->isRiver(); }
bool PlotContext::hasFreshWater() const  { return m_plot != NULL && (m_plot->isFreshWater() || m_plot->isRiver()); }
bool PlotContext::hasIrrigation() const  { return m_plot != NULL && m_plot->isIrrigated(); }
bool PlotContext::hasLandmark() const    { return m_plot != NULL && m_plot->getLandmarkType() != NO_LANDMARK; }
bool PlotContext::hasFeatureAny() const  { return m_plot != NULL && m_plot->getFeatureType() != NO_FEATURE; }
bool PlotContext::hasFeature(int eFeature) const         { return m_plot != NULL && (int)m_plot->getFeatureType() == eFeature; }
bool PlotContext::hasTerrain(int eTerrain) const         { return m_plot != NULL && (int)m_plot->getTerrainType() == eTerrain; }
bool PlotContext::hasImprovement(int eImprovement) const { return m_plot != NULL && (int)m_plot->getImprovementType() == eImprovement; }
bool PlotContext::hasRoute(int eRoute) const             { return m_plot != NULL && (int)m_plot->getRouteType() == eRoute; }
bool PlotContext::hasBonus(int eBonus, int eTeam) const  { return m_plot != NULL && (int)m_plot->getBonusType((TeamTypes)eTeam) == eBonus; }
bool PlotContext::isWorked() const       { return m_plot != NULL && m_plot->isBeingWorked(); }
bool PlotContext::isCity() const         { return m_plot != NULL && m_plot->isCity(); }
bool PlotContext::isOwned() const        { return m_plot != NULL && m_plot->isOwned(); }
int  PlotContext::owner() const          { return m_plot != NULL ? (int)m_plot->getOwner() : (int)NO_PLAYER; }
int  PlotContext::latitude() const       { return m_plot != NULL ? m_plot->getLatitude() : 0; }
int  PlotContext::natureYield(int eYield, int eTeam) const
{
	if (m_plot == NULL || eYield < 0)
		return 0;
	return m_plot->calculateNatureYield((YieldTypes)eYield, (TeamTypes)eTeam);
}
