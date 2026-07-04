//
//	CvCascadePerfCount -- the ONE definition site of the cascade per-turn call counters + stopwatch accumulators.
//

#include "CvGameCoreDLL.h"
#include "CvCascadePerfCount.h"

int CascadePerf::facts = 0;
int CascadePerf::factsMemoHit = 0;
int CascadePerf::yieldRate = 0;
int CascadePerf::pctStack = 0;
int CascadePerf::commerceRate = 0;
int CascadePerf::condEval = 0;
int CascadePerf::accRefresh = 0;
int CascadePerf::wbCompute = 0;

double CascadePerf::factsMs = 0.0;
double CascadePerf::yieldRateMs = 0.0;
double CascadePerf::pctStackMs = 0.0;
double CascadePerf::commerceRateMs = 0.0;
double CascadePerf::wbComputeMs = 0.0;
double CascadePerf::legacyRateMs = 0.0;
double CascadePerf::legacyWbMs = 0.0;

void CascadePerf::reset()
{
	facts = 0; factsMemoHit = 0; yieldRate = 0; pctStack = 0; commerceRate = 0; condEval = 0; accRefresh = 0;
	wbCompute = 0;   // was MISSING (found 2026-07-04): wbN/wbMsX10 accumulated across turns instead of per turn
	factsMs = 0.0; yieldRateMs = 0.0; pctStackMs = 0.0; commerceRateMs = 0.0;
	wbComputeMs = 0.0;
	legacyRateMs = 0.0; legacyWbMs = 0.0;
}
