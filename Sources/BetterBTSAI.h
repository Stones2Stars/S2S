#pragma once

#ifndef BETTERBTSAI_H
#define BETTERBTSAI_H

#include "Stopwatch.h"

// Log-verbosity globals, shared by the per-subsystem tagged logs
// (CvWorkerAI [WAI/*] -> BuildEvaluation.log, CvHunterAI [HAI/*] -> HunterAI.log,
// CvDecisionAI [DAI/*] -> DecisionAI.log, ContractBroker -> ContractBroker.log).
// Set from BUG options in CvGlobals; see logDecisionAI et al. below.
extern int gPlayerLogLevel;
extern int gTeamLogLevel;
extern int gCityLogLevel;
extern int gUnitLogLevel;

// Turn-timing verbosity, driven by its OWN BUG option (Autolog__LogLevelPerf) so wall-clock
// timing can run always-on in normal play without enabling the verbose AI logs. 0 = off.
// Feeds logPerf() -> Performance.log. See PERF_SCOPE below.
extern int gPerfLogLevel;

// Live log stream gate (#419, Autolog__LogLevelStream): lines at this level or below are
// ALSO teed raw onto the /events SSE pipe ("the counter-strike way" -- out-of-process
// parsing against the tag taxonomy in docs/reference/ai-logging-reference.md). A line
// only streams if its file gate passed too; headline default is 1.
extern int gStreamLogLevel;

void logCB(CvString message);
void logToFile(CvString message, const char* filename);
void logContractBroker(int level, const char* format, ...);
void logBuildEvaluation(int level, const char* format, ...);
void logHunterAI(int level, const char* format, ...);
void logDecisionAI(int level, const char* format, ...);
void logDiploAI(int level, const char* format, ...);
void logWarAI(int level, const char* format, ...);
void logUnitAI(int level, const char* format, ...);
void logCityAI(int level, const char* format, ...);
void logGroupAI(int level, const char* format, ...);
void logEspionageAI(int level, const char* format, ...);
void logFoundAI(int level, const char* format, ...);
void logGameInfo(const char* format, ...);
void logCombatAI(int level, const char* format, ...);

// Engine-integrity warnings -- [ENG/*] tags -> Engine.log, gated by gTeamLogLevel.
// Home for "should never happen" sanity checks demoted from FAssert/FErrorMsg so they
// still surface in FinalRelease (where FASSERT compiles out) without popping dialogs or
// flooding Asserts.log with stack traces. One key=value line per occurrence; the
// repeated-line count is itself the signal (don't dedup).
//   [ENG/viscap] (lvl 2, CvPlot::changeVisibilityCount) = a team's plot visibility
//   count went negative and was capped to zero (fires en masse during
//   recalculateModifiers' remove/re-add sight passes).
void logEngine(int level, const char* format, ...);

// Wall-clock turn-phase timing -> Performance.log, gated by gPerfLogLevel.
// Works with ANY DLL config (Assert/Release); no special Profile build needed.
// Emits one grep-friendly line per phase:
//   [PERF/phase] turn=312 owner=1 phase=CvPlayer::doTurn ms=84.213
// (owner=-1 for game/map-scope phases that have no single player).
void logPerf(int level, const char* format, ...);

// RAII scope timer: starts a high-res stopwatch on construction and logs the elapsed
// milliseconds via logPerf() when it goes out of scope. Cost when gPerfLogLevel==0 is a
// single integer compare, so it is safe to leave compiled into shipping DLLs.
// Use the PERF_SCOPE macro rather than constructing directly.
class ScopedPerfTimer
{
public:
	ScopedPerfTimer(const char* szPhase, int iOwner);
	~ScopedPerfTimer();

private:
	win32::Stopwatch m_stopwatch;
	const char* m_szPhase;
	int m_iOwner;
	bool m_bActive;

	ScopedPerfTimer(const ScopedPerfTimer&);
	ScopedPerfTimer& operator=(const ScopedPerfTimer&);
};

#define PERF_SCOPE_CONCAT_(a, b) a##b
#define PERF_SCOPE_CONCAT(a, b) PERF_SCOPE_CONCAT_(a, b)
// Time the enclosing scope. szPhase is a stable label; iOwner is a player id (or -1).
#define PERF_SCOPE(szPhase, iOwner) ScopedPerfTimer PERF_SCOPE_CONCAT(perfTimer_, __LINE__)((szPhase), (iOwner))

// Accumulating scope timer: instead of logging, ADDS its elapsed ms to a caller-owned
// accumulator on scope exit. Use to measure interleaved sub-sections inside a hot loop --
// accumulate across iterations, then logPerf() the totals ONCE (avoids per-iteration log spam).
// Active only when gPerfLogLevel >= 1, so it is free when perf logging is off.
class PerfAccumTimer
{
public:
	explicit PerfAccumTimer(double& dAccumMs)
		: m_dAccumMs(dAccumMs)
		, m_bActive(gPerfLogLevel >= 1)
	{
		if (m_bActive) m_stopwatch.Start();
	}
	~PerfAccumTimer()
	{
		if (m_bActive)
		{
			m_stopwatch.Stop();
			m_dAccumMs += m_stopwatch.ElapsedMilliseconds();
		}
	}

private:
	win32::Stopwatch m_stopwatch;
	double& m_dAccumMs;
	bool m_bActive;

	PerfAccumTimer(const PerfAccumTimer&);
	PerfAccumTimer& operator=(const PerfAccumTimer&);
};
// Accumulate the enclosing scope's ms into dAccumMs (a double the caller declares + logs).
#define PERF_ACCUM(dAccumMs) PerfAccumTimer PERF_SCOPE_CONCAT(perfAccum_, __LINE__)(dAccumMs)

//	Whole-turn accumulators for the frame-driven span the [PERF/phase] doTurn tree does NOT
//	cover: AI unit movement and all other per-frame DLL work run from CvGame::update between
//	turn boundaries (the EXE calls update per rendered frame). Accumulated here across the
//	turn and logged + reset at CvGame::doTurn, so wall-clock turn time can be split into
//	doTurn phases / frame-driven compute / engine+render residual. Defined in BetterBTSAI.cpp.
extern double gPerfGameUpdateAccumMs;    // total time inside CvGame::update
extern double gPerfUpdateMovesAccumMs;   // inside CvGame::updateMoves (unit movement driver)
extern double gPerfAutoMissionAccumMs;   // ...of which: selection-group autoMission sweeps
extern double gPerfUnitUpdateAccumMs;    // ...of which: CvPlayerAI::AI_unitUpdate decisions
extern double gPerfBrokerPPAccumMs;      // ...of which: contract-broker post-processing

//	Second-tier attribution inside the frame span (same log+reset cycle):
extern double gPerfPathGenAccumMs;       // CvPathGenerator::generatePath total
extern int    gPerfPathGenN;             // ...call count
extern double gPerfReachableAccumMs;     // CvReachablePlotSet construction total
extern int    gPerfReachableN;           // ...call count
extern double gPerfPlotPagingAccumMs;    // CvPlotPaging::UpdatePaging per slice
extern double gPerfPyGameUpdateAccumMs;  // Python "gameUpdate" generic event per slice
extern double gPerfUpdateScoreAccumMs;   // CvGame::updateScore per slice
extern double gPerfUpdateTimersAccumMs;  // updateTimers + updateTurnTimer per slice
extern double gPerfAssignWorkAccumMs;    // CvGame::AI_updateAssignWork per slice
extern double gPerfTestAliveAccumMs;     // CvGame::testAlive per slice
//	Per-UNITAI-type decision time (CvSelectionGroupAI::AI_update attributed to the head
//	unit's AI type) -- logged as a [PERF/unitai] table at the turn boundary.
extern double gPerfUnitAITypeAccumMs[];
extern int    gPerfUnitAITypeAccumN[];
//	Churn discriminators per UNITAI type: how AI_update calls arrive (force-armed vs
//	awake-ready) and how many exit with the group STILL ready to move (a non-terminating
//	decision cascade -- the re-decide spin signature).
extern int    gPerfUnitAITypeForceN[];
extern int    gPerfUnitAITypeAwakeN[];
extern int    gPerfUnitAITypeExitReadyN[];

#endif