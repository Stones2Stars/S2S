//
//	CvCascadePerfCount -- the ONE definition site of the cascade per-turn call counters + stopwatch accumulators.
//

#include "CvGameCoreDLL.h"
#include "CvCascadePerfCount.h"

int CascadePerf::operatingBuildingsRecomputed = 0;
int CascadePerf::operatingBuildingsCacheHits = 0;
int CascadePerf::yieldRate = 0;
int CascadePerf::pctStack = 0;
int CascadePerf::commerceRate = 0;
int CascadePerf::condEval = 0;
int CascadePerf::condEvalBy[CC_COUNT] = { 0 };
int CascadePerf::condCaller = CC_OTHER;   // the live scope tag -- NOT reset() (a flush never runs mid-scope)
int CascadePerf::accRefresh = 0;
int CascadePerf::wbCompute = 0;
int CascadePerf::frontBFills = 0;
int CascadePerf::frontUFills = 0;
int CascadePerf::frontPPFills = 0;
int CascadePerf::frontPFills = 0;
int CascadePerf::promoFills = 0;

double CascadePerf::operatingBuildingsRecomputeMs = 0.0;
double CascadePerf::yieldRateMs = 0.0;
double CascadePerf::pctStackMs = 0.0;
double CascadePerf::commerceRateMs = 0.0;
double CascadePerf::wbComputeMs = 0.0;
double CascadePerf::legacyRateMs = 0.0;
double CascadePerf::legacyWbMs = 0.0;
double CascadePerf::scRefreshMs = 0.0;
double CascadePerf::frontBMs = 0.0;
double CascadePerf::frontUMs = 0.0;
double CascadePerf::frontPPMs = 0.0;
double CascadePerf::frontPMs = 0.0;
double CascadePerf::promoMs = 0.0;

int CascadePerf::scGpBaseReads = 0;
int CascadePerf::scGpModReads = 0;
int CascadePerf::scDefReads = 0;
int CascadePerf::scMaintReads = 0;
int CascadePerf::scRefresh = 0;
int CascadePerf::scSpecRefresh = 0;
int CascadePerf::autoMissions = 0;
double CascadePerf::autoMissionMs = 0.0;

void CascadePerf::reset()
{
	operatingBuildingsRecomputed = 0; operatingBuildingsCacheHits = 0; yieldRate = 0; pctStack = 0; commerceRate = 0; condEval = 0; accRefresh = 0;
	wbCompute = 0;   // was MISSING (found 2026-07-04): wbN/wbMsX10 accumulated across turns instead of per turn
	for (int i = 0; i < CC_COUNT; ++i) condEvalBy[i] = 0;   // condCaller itself is a live scope tag -- never reset
	frontBFills = 0; frontUFills = 0; frontPPFills = 0; frontPFills = 0; promoFills = 0;
	scGpBaseReads = 0; scGpModReads = 0; scDefReads = 0; scMaintReads = 0; scRefresh = 0; scSpecRefresh = 0;
	operatingBuildingsRecomputeMs = 0.0; yieldRateMs = 0.0; pctStackMs = 0.0; commerceRateMs = 0.0;
	wbComputeMs = 0.0;
	legacyRateMs = 0.0; legacyWbMs = 0.0;
	scRefreshMs = 0.0;
	frontBMs = 0.0; frontUMs = 0.0; frontPPMs = 0.0; frontPMs = 0.0; promoMs = 0.0;
	autoMissions = 0; autoMissionMs = 0.0;
}
