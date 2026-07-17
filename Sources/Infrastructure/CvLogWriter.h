#pragma once
#ifndef CV_LOG_WRITER_H
#define CV_LOG_WRITER_H

//
//	CvLogWriter -- the OFF-THREAD log file writer (owner 2026-07-16: "can we have this writer on a separate
//	thread?"). The game thread renders a line and ENQUEUES it here (a critical-section push -- microseconds);
//	a dedicated Win32 writer thread drains the queue and does ALL disk I/O, replacing the synchronous
//	gDLL->logMsg appends that stalled the game thread under heavy logging. Two clients: the event-spine FILE
//	consumer and the BetterBTSAI log helpers (each log FILE has exactly ONE writer -- the census verified the
//	remaining gDLL->logMsg call sites target disjoint files).
//
//	Behaviour notes:
//	- The line is TIMESTAMPED AT ENQUEUE ([sec.mmm] since process start) -- a line's time is when it happened,
//	  not when the writer got to it.
//	- Files open once per session ("w" -- fresh logs per launch, matching the EXE logger) and stay open, but the
//	  writer fflushes every batch and opens with shared read, so logs are READABLE WHILE THE GAME RUNS (killing
//	  the "game holds its .log files open" pain, AGENTS.md).
//	- Path: %USERPROFILE%\Documents\My Games\Beyond The Sword\Logs\<name> (the PlotSnapshot resolution; same
//	  OneDrive-redirection caveat).
//	- shutdown() drains + flushes + joins; called from cvInternalGlobals::uninit.
//

class CvLogWriter
{
public:
	static void write(const char* szLogFileName, const char* szLine);   // any thread; lazy-starts the writer
	static void shutdown();                                             // drain + flush + join (DLL teardown)

private:
	CvLogWriter();   // purely-static component -- never instantiated
};

#endif // CV_LOG_WRITER_H
