//
//	CvLogWriter -- see the header. Producer/consumer over a critical-section queue + one Win32 writer thread
//	(the CvHttpServer thread pattern; C++03/Win32 only -- the toolchain has no std::thread).
//

#include "CvGameCoreDLL.h"
#include "CvLogWriter.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace
{
	struct LogWriterState
	{
		CRITICAL_SECTION lock;
		std::vector<std::pair<std::string, std::string> > queue;   // (log file name, stamped line) -- guarded by lock
		HANDLE hThread;
		HANDLE hWake;              // auto-reset: signaled on enqueue + on stop
		volatile LONG bStop;
		DWORD dwStartTicks;        // the [sec.mmm] stamp epoch (process-start-ish: first static init)
		FILETIME ftSessionStart;   // same epoch as a FILETIME -- the stale-log cutoff (see sweepStaleLogs)
		std::map<std::string, FILE*> files;   // writer thread only

		LogWriterState() : hThread(NULL), hWake(NULL), bStop(0), dwStartTicks(GetTickCount())
		{
			GetSystemTimeAsFileTime(&ftSessionStart);   // DLL load == before anything logs this session
			InitializeCriticalSection(&lock);   // static init runs single-threaded at DLL load
		}
	};
	LogWriterState g_state;

	FILE* fileFor(const std::string& szName)
	{
		const std::map<std::string, FILE*>::const_iterator it = g_state.files.find(szName);
		if (it != g_state.files.end()) return it->second;

		FILE* fp = NULL;
		const char* szUserProfile = std::getenv("USERPROFILE");
		if (szUserProfile != NULL)
		{
			char szPath[MAX_PATH];
			const int n = _snprintf(szPath, sizeof(szPath), "%s\\Documents\\My Games\\Beyond The Sword\\Logs\\%s",
				szUserProfile, szName.c_str());
			// "w" = fresh log per session (the EXE logger's behaviour); CRT fopen shares reads, and the
			// per-batch fflush below keeps the file readable while the game runs.
			if (n > 0 && n < (int)sizeof(szPath)) fp = fopen(szPath, "w");
		}
		g_state.files[szName] = fp;   // NULL caches too -- a failing path fails once, not per line
		return fp;
	}

	// Clear STALE logs once per session. A log file is only truncated by whoever writes it (this writer opens
	// "w"; the legacy gDLL->logMsg sinks truncate on their first append-false write), so a file that NOTHING
	// writes this session survives untouched -- and reads as if it were current. That is a live misdiagnosis
	// hazard, not clutter: a days-old Asserts.log / Xml_MissingTypes.log is indistinguishable from a fresh one.
	// Only files last written BEFORE this session are cleared, so a log already written this run (XmlLoad.log
	// during the XML pass, an early assert) is never truncated out from under its writer.
	void sweepStaleLogs()
	{
		const char* szUserProfile = std::getenv("USERPROFILE");
		if (szUserProfile == NULL) return;
		char szDir[MAX_PATH];
		int n = _snprintf(szDir, sizeof(szDir), "%s\\Documents\\My Games\\Beyond The Sword\\Logs", szUserProfile);
		if (n <= 0 || n >= (int)sizeof(szDir)) return;

		char szGlob[MAX_PATH];
		n = _snprintf(szGlob, sizeof(szGlob), "%s\\*.log", szDir);
		if (n <= 0 || n >= (int)sizeof(szGlob)) return;

		WIN32_FIND_DATAA fd;
		const HANDLE hFind = FindFirstFileA(szGlob, &fd);
		if (hFind == INVALID_HANDLE_VALUE) return;
		do
		{
			if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) continue;
			if (CompareFileTime(&fd.ftLastWriteTime, &g_state.ftSessionStart) >= 0) continue;   // written THIS session
			char szPath[MAX_PATH];
			const int k = _snprintf(szPath, sizeof(szPath), "%s\\%s", szDir, fd.cFileName);
			if (k <= 0 || k >= (int)sizeof(szPath)) continue;
			// truncate, never delete: gDLL keeps handles open, which blocks remove() (the PlotSnapshot caveat).
			// A file another handle holds exclusively just fails to open here -- harmless, it stays as it was.
			FILE* fp = fopen(szPath, "w");
			if (fp != NULL) fclose(fp);
		}
		while (FindNextFileA(hFind, &fd) != 0);
		FindClose(hFind);
	}

	DWORD WINAPI writerThread(LPVOID)
	{
		sweepStaleLogs();   // once, before any file opens -- the writer thread starts on the first logged line
		std::vector<std::pair<std::string, std::string> > batch;
		for (;;)
		{
			WaitForSingleObject(g_state.hWake, 500);
			batch.clear();
			EnterCriticalSection(&g_state.lock);
			batch.swap(g_state.queue);
			const bool bStop = (g_state.bStop != 0);
			LeaveCriticalSection(&g_state.lock);

			for (size_t i = 0; i < batch.size(); ++i)
			{
				FILE* fp = fileFor(batch[i].first);
				if (fp != NULL) fputs(batch[i].second.c_str(), fp);
			}
			if (!batch.empty())
			{
				for (std::map<std::string, FILE*>::const_iterator it = g_state.files.begin(); it != g_state.files.end(); ++it)
					if (it->second != NULL) fflush(it->second);
			}
			if (bStop && batch.empty()) break;   // drained after the stop flag -> done
		}
		for (std::map<std::string, FILE*>::const_iterator it = g_state.files.begin(); it != g_state.files.end(); ++it)
			if (it->second != NULL) fclose(it->second);
		g_state.files.clear();
		return 0;
	}
}

void CvLogWriter::write(const char* szLogFileName, const char* szLine)
{
	if (szLogFileName == NULL || szLine == NULL) return;

	// Stamp at enqueue: [sec.mmm] since process start (the EXE logger's stamp shape; our epoch).
	const DWORD dwMs = GetTickCount() - g_state.dwStartTicks;
	char szStamped[600];
	_snprintf(szStamped, sizeof(szStamped) - 1, "[%u.%03u] %s\n", dwMs / 1000, dwMs % 1000, szLine);
	szStamped[sizeof(szStamped) - 1] = '\0';

	EnterCriticalSection(&g_state.lock);
	if (g_state.bStop == 0)
	{
		if (g_state.hThread == NULL)
		{
			g_state.hWake = CreateEvent(NULL, FALSE, FALSE, NULL);
			g_state.hThread = CreateThread(NULL, 0, writerThread, NULL, 0, NULL);
		}
		g_state.queue.push_back(std::make_pair(std::string(szLogFileName), std::string(szStamped)));
	}
	LeaveCriticalSection(&g_state.lock);
	if (g_state.hWake != NULL) SetEvent(g_state.hWake);
}

void CvLogWriter::shutdown()
{
	EnterCriticalSection(&g_state.lock);
	const HANDLE hThread = g_state.hThread;
	g_state.bStop = 1;
	LeaveCriticalSection(&g_state.lock);
	if (hThread == NULL) return;

	SetEvent(g_state.hWake);
	WaitForSingleObject(hThread, 5000);   // the writer exits after its post-stop drain
	CloseHandle(hThread);
	CloseHandle(g_state.hWake);
	EnterCriticalSection(&g_state.lock);
	g_state.hThread = NULL;
	g_state.hWake = NULL;
	LeaveCriticalSection(&g_state.lock);
}
