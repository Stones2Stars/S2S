#include "CvGameCoreDLL.h"
#include "PlotContext.h"
#include "CvPlot.h"

// forwarding accessors: every plot fact is already O(1) on CvPlot -- read it through, no stored copy (owner: don't
// duplicate available state). Each delegates to the SAME accessor the one condition evaluator reads
// (CvConditionEval.cpp CASC_PRED_*), so a HAS_/IS_ plot fact has ONE source.
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
bool PlotContext::hasBonus(int eBonus, int eTeam) const  { return m_plot != NULL && (int)m_plot->getBonusType((TeamTypes)eTeam) == eBonus; }
bool PlotContext::isWorked() const       { return m_plot != NULL && m_plot->isBeingWorked(); }
bool PlotContext::isCity() const         { return m_plot != NULL && m_plot->isCity(); }
