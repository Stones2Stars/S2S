#include "CvGameCoreDLL.h"
#include "CityContext.h"
#include "CvPlot.h"
#include "CvJsonCondition.h"   // CASC_PRED_* -- the shared HAS_/IS_ plot predicate ids the plotAttrs dictionary keys on

void CityContext::onPlotChanged(const CvPlot* plot, int sign)
{
	if (plot == NULL)
		return;
	// Fold each stable HAS_/IS_ attribute the plot carries into the plotAttrs dictionary (+1 on enter, -1 on leave).
	// COUNTS only -- the plot itself is never stored. These are the terrain-level attributes that change only when a
	// plot enters/leaves the city; mutable ones (feature / irrigation / worked) join once their change-events land.
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
