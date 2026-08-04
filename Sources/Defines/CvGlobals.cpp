//
// globals.cpp
//
#include "CvGameCoreDLL.h"
#include "Infrastructure/BoolExpr.h"
#include "CvBuildingInfo.h"
#include "AI/CvGameAI.h"
#include "CvGlobals.h"
#include "Tools/CvHttpServer.h"
#include "Spine/CvEventSpine.h"   // emitGlobalDefineChanged -- the live-option bridge announces
#include "CvImprovementInfo.h"
#include "CvBonusInfo.h"
#include "CvInfos.h"
#include "CvInfoUtil.h"
#include "CvDiplomacyClasses.h"
#include "CvUnitCombatInfo.h"
#include "CvTechInfo.h"           // cascadeStartNode -- the synthetic TECH_GAME_START root every player holds
#include "CvPlayerOptionInfo.h"
#include "CvInfoWater.h"
#include "Infrastructure/CvInitCore.h"
#include "Infrastructure/CvLogWriter.h"   // shutdown() at uninit -- the off-thread log writer
#include "Engine/CvMap.h"
#include "UI/CvMapExternal.h"
#include "UI/CvMessageControl.h"
#include "AI/CvPlayerAI.h"
#include "Engine/CvPlot.h"
#include "Infrastructure/CvPython.h"
#include "Tools/CvRandom.h"
#include "AI/CvTeamAI.h"
#include "UI/CvViewport.h"
#include "Infrastructure/CvXMLLoadUtility.h"
#include "Infrastructure/CvDLLEngineIFaceBase.h"
#include "Infrastructure/CvDLLFAStarIFaceBase.h"
#include "Infrastructure/CvDLLUtilityIFaceBase.h"
#include "Python/CyGlobalContext.h"
#include "Tools/FVariableSystem.h"
#include "UI/CityOutputHistory.h"
#include "Repos/BuildingsRepo.h"
#include "Repos/BuildsRepo.h"
#include "Repos/InfoRepo.h"   // #430: the 5 EXE-bound getters return the JSON-mapped shim from the per-type InfoRepo
#include "Data/CvReadJson.h"   // #430: map the curated JSON -> InfoRepo at load
#include <time.h>
#include <sstream>

static char gVersionString[1024] = { 0 };

// Use macro override when available. Version string might not be loaded in time for
// applying it to the mini-dump so we will use macro version string for releases
#ifndef C2C_VERSION
#	define C2C_VERSION gVersionString
#endif

/*
#define COPY(dst, src, typeName) \
	{ \
		int iNum = sizeof(src)/sizeof(typeName); \
		dst = new typeName[iNum]; \
		for (int i =0;i<iNum;i++) \
			dst[i] = src[i]; \
	}
*/

void deleteInfoArray(std::vector<CvInfoBase*>* array)
{
	PROFILE_EXTRA_FUNC();
	foreach_(const CvInfoBase* info, *array)
	{
		delete info;
	}

	array->clear();
}

//////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////

CvGlobals gGlobalsProxy;	// for debugging
cvInternalGlobals* gGlobals = NULL;
CvDLLUtilityIFaceBase* gDLL = NULL;
bool gMiscLogging = false;

#ifdef _DEBUG
int inDLL = 0;
const char* fnName = NULL;

//	Wrapper for debugging so as to be able to always tell last method entered
ProxyTracker::ProxyTracker(const char* name)
{
	inDLL++;
	fnName = name;
	FAssertMsg(gGlobals != NULL, "Method called prior to global instantiation");
}

ProxyTracker::~ProxyTracker()
{
	inDLL--;
	fnName = NULL;
}
#endif

//
// CONSTRUCTOR
//
cvInternalGlobals::cvInternalGlobals()
	: m_paszAnimationOperatorTypes(NULL)
	, m_paszFunctionTypes(NULL)
	, m_paszFlavorTypes(NULL)
	, m_paszArtStyleTypes(NULL)
	, m_paszCitySizeTypes(NULL)
	, m_paszContactTypes(NULL)
	, m_paszDiplomacyPowerTypes(NULL)
	, m_paszAutomateTypes(NULL)
	, m_paszDirectionTypes(NULL)
	, m_paszFootstepAudioTypes(NULL)
	, m_paszFootstepAudioTags(NULL)
	, m_bGraphicsInitialized(false)
	, m_bLogging(false)
	, m_bRandLogging(false)
	, m_bOverwriteLogs(false)
	, m_bSynchLogging(false)
	, m_bDLLProfiler(false)
	, m_pFMPMgr(NULL)
	, m_asyncRand(NULL)
	, m_interface(NULL)
	, m_game(NULL)
	, m_messageQueue(NULL)
	, m_hotJoinMsgQueue(NULL)
	, m_messageControl(NULL)
	, m_messageCodes(NULL)
	, m_dropMgr(NULL)
	, m_portal(NULL)
	, m_setupData(NULL)
	, m_initCore(NULL)
	, m_statsReporter(NULL)
	, m_diplomacyScreen(NULL)
	, m_mpDiplomacyScreen(NULL)
	, m_aiPlotDirectionX(NULL)
	, m_aiPlotDirectionY(NULL)
	, m_aiPlotCardinalDirectionX(NULL)
	, m_aiPlotCardinalDirectionY(NULL)
	, m_aiCityPlotX(NULL)
	, m_aiCityPlotY(NULL)
	, m_aiCityPlotPriority(NULL)
	, m_aeTurnLeftDirection(NULL)
	, m_aeTurnRightDirection(NULL)
	, m_Profiler(NULL)
	, m_VarSystem(NULL)
	, m_fPLOT_SIZE(0)
	, m_iViewportCenterOnSelectionCenterBorder(5)
	, m_szAlternateProfilSampleName("")
	// BBAI Options
	, m_bBBAI_AIR_COMBAT(false)
	, m_bBBAI_HUMAN_VASSAL_WAR_BUILD(false)

	// Tech Diffusion
	, m_bTECH_DIFFUSION_ENABLE(false)

	, m_bIsInPedia(false)
	, m_iLastTypeID(-1)
	, m_iActiveLandscapeID(0)
	// uninitialized variables bugfix
	, m_iNumPlayableCivilizationInfos(0)
	, m_iNumAIPlayableCivilizationInfos(0)
	, m_iTotalNumModules(0) // Modular loading control
	, iStuckUnitID(0)
	, iStuckUnitCount(0)
	, m_iniInitCore(NULL)
	, m_loadedInitCore (NULL)
	, m_bResourceLayerOn(false)
	, m_iNumAnimationOperatorTypes(0)
	, m_iNumFlavorTypes(0)
	, m_iNumArtStyleTypes(0)
	, m_iNumCitySizeTypes(0)
	, m_iNumFootstepAudioTypes(0)
	, m_bSignsCleared(false)

#define ADD_TO_CONSTRUCTOR(dataType, VAR) \
	, m_##VAR((dataType)0)

	DO_FOR_EACH_GLOBAL_DEFINE(ADD_TO_CONSTRUCTOR)

#define ADD_INFO_TYPE_TO_CONSTRUCTOR(dataType, VAR) \
	, m_##VAR((dataType)-1)

	DO_FOR_EACH_INFO_TYPE(ADD_INFO_TYPE_TO_CONSTRUCTOR)
{
}

cvInternalGlobals::~cvInternalGlobals()
{
}

/************************************************************************************************/
/* MINIDUMP_MOD                           04/10/11                                terkhen       */
/*                                                                                              */
/* See http://www.debuginfo.com/articles/effminidumps.html                                      */
/************************************************************************************************/
#define MINIDUMP
#ifdef MINIDUMP

#include <dbghelp.h>
#pragma comment (lib, "dbghelp.lib")

// delayimp.h is ABSENT from the vendored VC7.1 SDK, so declare the minimal delay-load surface by hand (layout
// verified against a live dump: szDll @ +0x0c, dlp.fImportByName @ +0x10, dlp.szProcName @ +0x14). Lets the crash
// filter name the missing DLL/proc on a delay-load exception (c06d007e/f) without the header.
#ifndef FACILITY_VISUALCPP
#define FACILITY_VISUALCPP  ((DWORD)0x6d)
#endif
#define VcppException(sev, err)  ((sev) | (FACILITY_VISUALCPP << 16) | (err))
#ifndef ERROR_MOD_NOT_FOUND
#define ERROR_MOD_NOT_FOUND  126L
#endif
#ifndef ERROR_PROC_NOT_FOUND
#define ERROR_PROC_NOT_FOUND 127L
#endif
struct S2SDelayLoadProc { BOOL fImportByName; union { LPCSTR szProcName; DWORD dwOrdinal; }; };
struct S2SDelayLoadInfo { DWORD cb; const void* pidd; void** ppfn; LPCSTR szDll; S2SDelayLoadProc dlp;
                          HMODULE hmodCur; void* pfnCur; DWORD dwLastError; };


std::string getPyTrace()
{
	PROFILE_EXTRA_FUNC();
	std::vector<Cy::StackFrame> trace = Cy::get_stack_trace();

	std::stringstream buffer;

	for (std::vector<Cy::StackFrame>::const_iterator itr = trace.begin(); itr != trace.end(); ++itr)
	{
		if (itr != trace.begin()) buffer << "\r\n";
		buffer << CvString::format("%s.py (%d): %s", itr->filename.c_str(), itr->line, itr->code.c_str());
	}

	return buffer.str();
}

//	⚑ THE LAST PYTHON-FACING READ TO START. Set at the top of each read on the Cy* surface and never cleared, so
//	a crash names the read that was live when it happened. The crash we are chasing faults INSIDE python24 with
//	our DLL absent from the live frames, which makes the stack useless for attribution -- this is the attribution.
const char* g_szLastCyRead = "(none)";
// Render an SEH exception record as ONE string-parseable line -- `[EXCEPTION] key=value ...` -- so a crash NAMES its
// own cause in the log (grep-able), instead of needing offline dump analysis. The delay-load cases (PROC/MOD not found,
// c06d007e/f) decode the DelayLoadInfo and DEMANGLE the proc via UnDecorateSymbolName (dbghelp, already linked) -- this
// is exactly what would have printed `CvArtInfoImprovement::getShaderNIF` straight into the log. Safe in the crash
// filter: pure formatting, no allocation through the event spine (the process is unstable here -- file log only).
std::string describeException(EXCEPTION_POINTERS* pep)
{
	if (pep == NULL || pep->ExceptionRecord == NULL) return "";
	const EXCEPTION_RECORD* er = pep->ExceptionRecord;
	const DWORD code = er->ExceptionCode;
	const char* kind = "UNKNOWN";
	switch (code)
	{
	case EXCEPTION_ACCESS_VIOLATION:    kind = "ACCESS_VIOLATION"; break;
	case EXCEPTION_INT_DIVIDE_BY_ZERO:  kind = "INT_DIVIDE_BY_ZERO"; break;
	case EXCEPTION_FLT_DIVIDE_BY_ZERO:  kind = "FLT_DIVIDE_BY_ZERO"; break;
	case EXCEPTION_STACK_OVERFLOW:      kind = "STACK_OVERFLOW"; break;
	case EXCEPTION_ILLEGAL_INSTRUCTION: kind = "ILLEGAL_INSTRUCTION"; break;
	case EXCEPTION_PRIV_INSTRUCTION:    kind = "PRIV_INSTRUCTION"; break;
	case 0xE06D7363:                    kind = "CPP_EXCEPTION"; break;
	case VcppException(ERROR_SEVERITY_ERROR, ERROR_MOD_NOT_FOUND):  kind = "DELAY_LOAD_MOD_NOT_FOUND"; break;
	case VcppException(ERROR_SEVERITY_ERROR, ERROR_PROC_NOT_FOUND): kind = "DELAY_LOAD_PROC_NOT_FOUND"; break;
	}
	std::stringstream ss;
	ss << CvString::format("[EXCEPTION] code=0x%08X kind=%s addr=0x%08X", code, kind, (unsigned)(size_t)er->ExceptionAddress);
	if (code == EXCEPTION_ACCESS_VIOLATION && er->NumberParameters >= 2)
		ss << CvString::format(" access=%s faultAddr=0x%08X",
			er->ExceptionInformation[0] == 1 ? "write" : (er->ExceptionInformation[0] == 8 ? "execute" : "read"),
			(unsigned)er->ExceptionInformation[1]);
	if ((code == VcppException(ERROR_SEVERITY_ERROR, ERROR_MOD_NOT_FOUND) ||
	     code == VcppException(ERROR_SEVERITY_ERROR, ERROR_PROC_NOT_FOUND)) && er->NumberParameters >= 1)
	{
		const S2SDelayLoadInfo* dli = (const S2SDelayLoadInfo*)er->ExceptionInformation[0];
		if (dli != NULL)
		{
			ss << CvString::format(" dll=%s", dli->szDll ? dli->szDll : "?");
			if (dli->dlp.fImportByName && dli->dlp.szProcName != NULL)
			{
				char undec[512];
				if (UnDecorateSymbolName(dli->dlp.szProcName, undec, sizeof(undec), UNDNAME_COMPLETE) != 0)
					ss << CvString::format(" proc=%s", undec);
				else
					ss << CvString::format(" proc=%s", dli->dlp.szProcName);
			}
			else ss << CvString::format(" proc=ordinal#%u", dli->dlp.dwOrdinal);
		}
	}
	return ss.str();
}

void CreateMiniDump(EXCEPTION_POINTERS *pep)
{
	_TCHAR filename[256];

	time_t rawtime;
	struct tm* timeinfo;
	time (&rawtime);
	timeinfo = localtime (&rawtime);

	// tm_mon is 0-based (January == 0), so add 1 to print the calendar month. tm_year is
	// already adjusted via +1900. (Fixes #324 — dumps were stamped one month early.)
	_stprintf(filename, _T("MiniDump-%s-%d%02d%02d-%02d%02d%02d.dmp"), C2C_VERSION, 1900 + timeinfo->tm_year, timeinfo->tm_mon + 1, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);

	/* Open a file to store the minidump. */
	HANDLE hFile = CreateFile(filename,
							  GENERIC_READ | GENERIC_WRITE,
							  0,
							  NULL,
							  CREATE_ALWAYS,
							  FILE_ATTRIBUTE_NORMAL,
							  NULL);

	if((hFile == NULL) || (hFile == INVALID_HANDLE_VALUE))
	{
		return;
	}

	/* Create the minidump. */
	MINIDUMP_EXCEPTION_INFORMATION mdei;

	mdei.ThreadId           = GetCurrentThreadId();
	mdei.ExceptionPointers  = pep;
	mdei.ClientPointers     = FALSE;

	MINIDUMP_TYPE mdt       = MiniDumpNormal;

	BOOL result = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),
									hFile,
									mdt,
									(pep != NULL) ? &mdei : NULL,
									NULL,
									NULL);

	// ⛔ A FAILED WRITE MUST NOT LEAVE A FILE BEHIND. CreateFile has already made the .dmp, so discarding `result`
	// leaves a ZERO-BYTE dump that looks exactly like a real one -- the crash workflow then reads as "we have a
	// dump" right up until cdb refuses it, and the reason the write failed is gone for good. Name the failure and
	// remove the husk, so "no dump" is distinguishable from "a dump nobody can open".
	const DWORD dumpError = result ? 0 : GetLastError();

	/* Close the file. */
	CloseHandle(hFile);

	if (!result)
	{
		DeleteFile(filename);
	}

	// String-parseable exception log: one `[EXCEPTION] ...` headline naming the cause (grep it to find the issue),
	// with the dump filename + the Python callstack under it. Co-located in Exceptions.log so a crash is diagnosable
	// straight from the logs (the delay-load proc name, the AV read/write addr, ...) without offline dump analysis.
	std::string exc = describeException(pep);
	exc += CvString::format(" lastCyRead=%s", g_szLastCyRead);
	if (!exc.empty())
	{
		// `dump=` names a file that EXISTS; a failed write reports the HRESULT instead, which is the difference
		// between "go symbolize this" and "the dump writer itself is broken, fix that first".
		gDLL->logMsg("Exceptions.log",
			result ? CvString::format("%s dump=%s", exc.c_str(), filename).c_str()
			       : CvString::format("%s dump=FAILED err=0x%08X", exc.c_str(), (unsigned)dumpError).c_str(),
			true, false);
	}
	std::string pyTrace = getPyTrace();
	if(!pyTrace.empty())
	{
		gDLL->logMsg("Exceptions.log", CvString::format("[EXCEPTION.pyTrace]\r\n%s", pyTrace.c_str()).c_str(), true, false);
		gDLL->logMsg("PythonCallstack.log", pyTrace.c_str(), true, false);
	}
}

LONG WINAPI CustomFilter(EXCEPTION_POINTERS *ExceptionInfo)
{
	CreateMiniDump(ExceptionInfo);
	return EXCEPTION_EXECUTE_HANDLER;
}

// A VECTORED handler fires for EVERY exception BEFORE the SEH/C++ search -- so it logs even the ones that get CAUGHT
// (e.g. a delay-load PROC_NOT_FOUND or an access violation that boost.python turns into "unidentifiable C++ exception").
// That is exactly the class the unhandled-filter above misses. We only log the two interesting kinds (missing-export
// delay-load + access violation), name them via describeException, then CONTINUE_SEARCH so normal handling is untouched.
// Capped so a per-frame throw can't flood the log; dedup on the identical line so one culprit prints once per burst.
LONG WINAPI S2SVectoredHandler(EXCEPTION_POINTERS* pep)
{
	static int s_logged = 0;
	static std::string s_last;
	if (pep != NULL && pep->ExceptionRecord != NULL && gDLL != NULL && s_logged < 200)
	{
		const DWORD code = pep->ExceptionRecord->ExceptionCode;
		if (code == VcppException(ERROR_SEVERITY_ERROR, ERROR_PROC_NOT_FOUND) ||
			code == VcppException(ERROR_SEVERITY_ERROR, ERROR_MOD_NOT_FOUND) ||
			code == EXCEPTION_ACCESS_VIOLATION)
		{
			const std::string exc = describeException(pep);
			if (!exc.empty() && exc != s_last)
			{
				s_last = exc;
				++s_logged;
				gDLL->logMsg("Exceptions.log", CvString::format("[EXCEPTION.caught] %s", exc.c_str()).c_str(), true, false);
			}
		}
	}
	return EXCEPTION_CONTINUE_SEARCH;
}

#endif
/************************************************************************************************/
/* MINIDUMP_MOD                                END                                              */
/************************************************************************************************/

//
// allocate
//
void cvInternalGlobals::init()
{
	PROFILE_EXTRA_FUNC();
	OutputDebugString("Initializing Internal Globals: Start\n");

#ifdef MINIDUMP
	/* Enable our custom exception that will write the minidump for us. */
	SetUnhandledExceptionFilter(CustomFilter);
	/* Also name CAUGHT delay-load/AV exceptions in Exceptions.log (the boost.python "unidentifiable C++ exception"). */
	AddVectoredExceptionHandler(1, S2SVectoredHandler);
#endif

	//
	// These vars are used to initialize the globals.
	//

	int aiPlotDirectionX[NUM_DIRECTION_TYPES] =
	{
		0,	// DIRECTION_NORTH
		1,	// DIRECTION_NORTHEAST
		1,	// DIRECTION_EAST
		1,	// DIRECTION_SOUTHEAST
		0,	// DIRECTION_SOUTH
		-1,	// DIRECTION_SOUTHWEST
		-1,	// DIRECTION_WEST
		-1,	// DIRECTION_NORTHWEST
	};

	int aiPlotDirectionY[NUM_DIRECTION_TYPES] =
	{
		1,	// DIRECTION_NORTH
		1,	// DIRECTION_NORTHEAST
		0,	// DIRECTION_EAST
		-1,	// DIRECTION_SOUTHEAST
		-1,	// DIRECTION_SOUTH
		-1,	// DIRECTION_SOUTHWEST
		0,	// DIRECTION_WEST
		1,	// DIRECTION_NORTHWEST
	};

	int aiPlotCardinalDirectionX[NUM_CARDINALDIRECTION_TYPES] =
	{
		0,	// CARDINALDIRECTION_NORTH
		1,	// CARDINALDIRECTION_EAST
		0,	// CARDINALDIRECTION_SOUTH
		-1,	// CARDINALDIRECTION_WEST
	};

	int aiPlotCardinalDirectionY[NUM_CARDINALDIRECTION_TYPES] =
	{
		1,	// CARDINALDIRECTION_NORTH
		0,	// CARDINALDIRECTION_EAST
		-1,	// CARDINALDIRECTION_SOUTH
		0,	// CARDINALDIRECTION_WEST
	};

	int aiCityPlotX[NUM_CITY_PLOTS] =
	{
		0,
		0, 1, 1, 1, 0,-1,-1,-1,
		0, 1, 2, 2, 2, 1, 0,-1,-2,-2,-2,-1,
		0, 1, 2, 3, 3, 3, 2, 1, 0, -1, -2, -3, -3, -3, -2, -1,
	};

	int aiCityPlotY[NUM_CITY_PLOTS] =
	{
		0,
		1, 1, 0,-1,-1,-1, 0, 1,
		2, 2, 1, 0,-1,-2,-2,-2,-1, 0, 1, 2,
		3, 3, 2, 1, 0, -1, -2, -3, -3, -3, -2, -1, 0, 1, 2, 3,
	};

	int aiCityPlotPriority[NUM_CITY_PLOTS] =
	{
		0,
		1, 2, 1, 2, 1, 2, 1, 2,
		3, 4, 4, 3, 4, 4, 3, 4, 4, 3, 4, 4,
		5, 6, 7, 6, 5, 6, 7, 6, 5, 6, 7, 6, 5, 6, 7, 6,
	};

	int aaiXYCityPlot[CITY_PLOTS_DIAMETER][CITY_PLOTS_DIAMETER] =
	{
		{-1, -1, 32, 33, 34, -1, -1},
		{-1, 31, 17, 18, 19, 35, -1},
		{30, 16, 6,   7,  8, 20, 36},
		{29, 15, 5,   0,  1,  9, 21},
		{28, 14, 4,   3,  2, 10, 22},
		{-1, 27, 13, 12, 11, 23, -1},
		{-1, -1, 26, 25, 24, -1, -1},
	};

	DirectionTypes aeTurnRightDirection[NUM_DIRECTION_TYPES] =
	{
		DIRECTION_NORTHEAST,	// DIRECTION_NORTH
		DIRECTION_EAST,				// DIRECTION_NORTHEAST
		DIRECTION_SOUTHEAST,	// DIRECTION_EAST
		DIRECTION_SOUTH,			// DIRECTION_SOUTHEAST
		DIRECTION_SOUTHWEST,	// DIRECTION_SOUTH
		DIRECTION_WEST,				// DIRECTION_SOUTHWEST
		DIRECTION_NORTHWEST,	// DIRECTION_WEST
		DIRECTION_NORTH,			// DIRECTION_NORTHWEST
	};

	DirectionTypes aeTurnLeftDirection[NUM_DIRECTION_TYPES] =
	{
		DIRECTION_NORTHWEST,	// DIRECTION_NORTH
		DIRECTION_NORTH,			// DIRECTION_NORTHEAST
		DIRECTION_NORTHEAST,	// DIRECTION_EAST
		DIRECTION_EAST,				// DIRECTION_SOUTHEAST
		DIRECTION_SOUTHEAST,	// DIRECTION_SOUTH
		DIRECTION_SOUTH,			// DIRECTION_SOUTHWEST
		DIRECTION_SOUTHWEST,	// DIRECTION_WEST
		DIRECTION_WEST,				// DIRECTION_NORTHWEST
	};

	DirectionTypes aaeXYDirection[DIRECTION_DIAMETER][DIRECTION_DIAMETER] =
	{
		DIRECTION_SOUTHWEST, DIRECTION_WEST,	DIRECTION_NORTHWEST,
		DIRECTION_SOUTH,     NO_DIRECTION,    DIRECTION_NORTH,
		DIRECTION_SOUTHEAST, DIRECTION_EAST,	DIRECTION_NORTHEAST,
	};

	FAssertMsg(gDLL != NULL, "Civ app needs to set gDLL");

	m_VarSystem = new FVariableSystem;
	m_asyncRand = new CvRandom;
	// Toffer - Strange that there's three instances of CvInitCore...
	//	Maybe when a save is loaded from within a game the new one has to be built before the old one is destroyed.
	//	I guess the exe does some juggling magic with the three, we only ever use m_initCore internaly in the dll.
	m_initCore = new CvInitCore;
	m_loadedInitCore = new CvInitCore;
	m_iniInitCore = new CvInitCore;

	gDLL->initGlobals();	// some globals need to be allocated outside the dll

	for (int i = 1; i < NUM_MAPS; i++)
	{
		m_pathFinders[i]			= gDLL->getFAStarIFace()->create();
		m_interfacePathFinders[i]	= gDLL->getFAStarIFace()->create();
		m_stepFinders[i]			= gDLL->getFAStarIFace()->create();
		m_routeFinders[i]			= gDLL->getFAStarIFace()->create();
		m_borderFinders[i]			= gDLL->getFAStarIFace()->create();
		m_areaFinders[i]			= gDLL->getFAStarIFace()->create();
		m_plotGroupFinders[i]		= gDLL->getFAStarIFace()->create();
	}

	m_game = new CvGameAI;

	for (int i = 0; i < NUM_MAPS; i++)
	{
		m_maps[i] = new CvMap((MapTypes)i);
	}

	CvPlayerAI::initStatics();
	CvTeamAI::initStatics();
	CyGlobalContext::initStatics();

	COPY(m_aiPlotDirectionX, aiPlotDirectionX, int);
	COPY(m_aiPlotDirectionY, aiPlotDirectionY, int);
	COPY(m_aiPlotCardinalDirectionX, aiPlotCardinalDirectionX, int);
	COPY(m_aiPlotCardinalDirectionY, aiPlotCardinalDirectionY, int);
	COPY(m_aiCityPlotX, aiCityPlotX, int);
	COPY(m_aiCityPlotY, aiCityPlotY, int);
	COPY(m_aiCityPlotPriority, aiCityPlotPriority, int);
	COPY(m_aeTurnLeftDirection, aeTurnLeftDirection, DirectionTypes);
	COPY(m_aeTurnRightDirection, aeTurnRightDirection, DirectionTypes);
	memcpy(m_aaiXYCityPlot, aaiXYCityPlot, sizeof(m_aaiXYCityPlot));
	memcpy(m_aaeXYDirection, aaeXYDirection,sizeof(m_aaeXYDirection));

	m_bSignsCleared = false;
	m_bResourceLayerOn = false;

	OutputDebugString("Initializing Internal Globals: End\n");
}

namespace
{
	void deleteFAStar(FAStar* ptr)
	{
		gDLL->getFAStarIFace()->destroy(ptr);
		ptr = NULL;
	}
}

//
// free
//
void cvInternalGlobals::uninit()
{
	PROFILE_EXTRA_FUNC();
	//
	// See also CvXMLLoadUtilityInit.cpp::CleanUpGlobalVariables()
	//
	CvLogWriter::shutdown();   // drain + flush + join the off-thread log writer before globals tear down
	SAFE_DELETE_ARRAY(m_aiPlotDirectionX);
	SAFE_DELETE_ARRAY(m_aiPlotDirectionY);
	SAFE_DELETE_ARRAY(m_aiPlotCardinalDirectionX);
	SAFE_DELETE_ARRAY(m_aiPlotCardinalDirectionY);
	SAFE_DELETE_ARRAY(m_aiCityPlotX);
	SAFE_DELETE_ARRAY(m_aiCityPlotY);
	SAFE_DELETE_ARRAY(m_aiCityPlotPriority);
	SAFE_DELETE_ARRAY(m_aeTurnLeftDirection);
	SAFE_DELETE_ARRAY(m_aeTurnRightDirection);

	SAFE_DELETE(m_game);

	foreach_(const CvMap* map, m_maps)
	{
		SAFE_DELETE(map);
	}

	CvPlayerAI::freeStatics();
	CvTeamAI::freeStatics();

	SAFE_DELETE(m_asyncRand);
	SAFE_DELETE(m_initCore);
	SAFE_DELETE(m_loadedInitCore);
	SAFE_DELETE(m_iniInitCore);
	gDLL->uninitGlobals();	// free globals allocated outside the dll
	SAFE_DELETE(m_VarSystem);

	// already deleted outside of the dll, set to null for safety
	m_messageQueue = NULL;
	m_hotJoinMsgQueue = NULL;
	m_messageControl = NULL;
	m_setupData = NULL;
	m_messageCodes = NULL;
	m_dropMgr = NULL;
	m_portal = NULL;
	m_statsReporter = NULL;
	m_interface = NULL;
	m_diplomacyScreen = NULL;
	m_mpDiplomacyScreen = NULL;

	algo::for_each(m_pathFinders, bind(deleteFAStar, _1));
	algo::for_each(m_interfacePathFinders, bind(deleteFAStar, _1));
	algo::for_each(m_stepFinders, bind(deleteFAStar, _1));
	algo::for_each(m_routeFinders, bind(deleteFAStar, _1));
	algo::for_each(m_borderFinders, bind(deleteFAStar, _1));
	algo::for_each(m_areaFinders, bind(deleteFAStar, _1));
	algo::for_each(m_plotGroupFinders, bind(deleteFAStar, _1));

	m_aInfoVectors.clear();
}

void cvInternalGlobals::clearTypesMap()
{
	if (m_VarSystem)
	{
		m_VarSystem->UnInit();
	}
}

std::vector<CvInterfaceModeInfo*>& cvInternalGlobals::getInterfaceModeInfos()
{
	return m_paInterfaceModeInfo;
}

CvInterfaceModeInfo& cvInternalGlobals::getInterfaceModeInfo(InterfaceModeTypes e) const
{
	FASSERT_BOUNDS(0, NUM_INTERFACEMODE_TYPES, e);
	return infoArrayAt(m_paInterfaceModeInfo, e, "m_paInterfaceModeInfo");
}

int* cvInternalGlobals::getPlotDirectionX() const
{
	return m_aiPlotDirectionX;
}

int* cvInternalGlobals::getPlotDirectionY() const
{
	return m_aiPlotDirectionY;
}

int* cvInternalGlobals::getPlotCardinalDirectionX() const
{
	return m_aiPlotCardinalDirectionX;
}

int* cvInternalGlobals::getPlotCardinalDirectionY() const
{
	return m_aiPlotCardinalDirectionY;
}

int* cvInternalGlobals::getCityPlotX() const
{
	return m_aiCityPlotX;
}

int* cvInternalGlobals::getCityPlotY() const
{
	return m_aiCityPlotY;
}

int* cvInternalGlobals::getCityPlotPriority() const
{
	return m_aiCityPlotPriority;
}

int cvInternalGlobals::getXYCityPlot(int i, int j) const
{
	FASSERT_BOUNDS(0, CITY_PLOTS_DIAMETER, i);
	FASSERT_BOUNDS(0, CITY_PLOTS_DIAMETER, j);
	return m_aaiXYCityPlot[i][j];
}

DirectionTypes* cvInternalGlobals::getTurnLeftDirection() const
{
	return m_aeTurnLeftDirection;
}

DirectionTypes cvInternalGlobals::getTurnLeftDirection(int i) const
{
	FASSERT_BOUNDS(0, DIRECTION_DIAMETER, i);
	return m_aeTurnLeftDirection[i];
}

DirectionTypes* cvInternalGlobals::getTurnRightDirection() const
{
	return m_aeTurnRightDirection;
}

DirectionTypes cvInternalGlobals::getTurnRightDirection(int i) const
{
	FASSERT_BOUNDS(0, DIRECTION_DIAMETER, i);
	return m_aeTurnRightDirection[i];
}

DirectionTypes cvInternalGlobals::getXYDirection(int i, int j) const
{
	FASSERT_BOUNDS(0, DIRECTION_DIAMETER, i);
	FASSERT_BOUNDS(0, DIRECTION_DIAMETER, j);
	return m_aaeXYDirection[i][j];
}

/*********************************/
/***** Parallel Maps - Begin *****/
/*********************************/
bool cvInternalGlobals::viewportsEnabled() const
{
	return m_ENABLE_VIEWPORTS;
}

int cvInternalGlobals::getNumMapInfos() const
{
	return m_paMapInfo.size();
}

CvMapInfo& cvInternalGlobals::getMapInfo(MapTypes eMap) const
{
	FASSERT_BOUNDS(0, NUM_MAPS, eMap);
	return infoArrayAt(m_paMapInfo, eMap, "m_paMapInfo");
}

void cvInternalGlobals::setResourceLayer(bool bOn)
{
	m_bResourceLayerOn = bOn;

	gDLL->getEngineIFace()->setResourceLayer(bOn);
}

bool cvInternalGlobals::getResourceLayer() const
{
	return m_bResourceLayerOn;
}

/*******************************/
/***** Parallel Maps - End *****/
/*******************************/

int cvInternalGlobals::getNumWorldInfos() const
{
	return (int)m_paWorldInfo.size();
}

CvWorldInfo& cvInternalGlobals::getWorldInfo(WorldSizeTypes e) const
{
	FASSERT_BOUNDS(0, GC.getNumWorldInfos(), e);
	return infoArrayAt(m_paWorldInfo, e, "m_paWorldInfo");
}

int cvInternalGlobals::getNumClimateInfos() const
{
	return (int)m_paClimateInfo.size();
}

CvClimateInfo& cvInternalGlobals::getClimateInfo(ClimateTypes e) const
{
	FASSERT_BOUNDS(0, GC.getNumClimateInfos(), e);
	return infoArrayAt(m_paClimateInfo, e, "m_paClimateInfo");
}

int cvInternalGlobals::getNumSeaLevelInfos() const
{
	return (int)m_paSeaLevelInfo.size();
}

CvSeaLevelInfo& cvInternalGlobals::getSeaLevelInfo(SeaLevelTypes e) const
{
	FASSERT_BOUNDS(0, GC.getNumSeaLevelInfos(), e);
	return infoArrayAt(m_paSeaLevelInfo, e, "m_paSeaLevelInfo");
}

int cvInternalGlobals::getNumHints() const
{
	return (int)m_paHints.size();
}

CvInfoBase& cvInternalGlobals::getHints(int i) const
{
	return infoArrayAt(m_paHints, i, "m_paHints");
}

int cvInternalGlobals::getNumMainMenus() const
{
	return (int)m_paMainMenus.size();
}

CvMainMenuInfo& cvInternalGlobals::getMainMenus(int i) const
{
	if (i >= getNumMainMenus())
	{
		return infoArrayAt(m_paMainMenus, 0, "m_paMainMenus");
	}

	return infoArrayAt(m_paMainMenus, i, "m_paMainMenus");
}
/************************************************************************************************/
/* MODULAR_LOADING_CONTROL                 10/30/07                            MRGENIE          */
/*                                                                                              */
/*                                                                                              */
/************************************************************************************************/
// MLF loading
void cvInternalGlobals::resetModLoadControlVector()
{
	m_paModLoadControlVector.clear();
}

int cvInternalGlobals::getModLoadControlVectorSize() const
{
	return (int)m_paModLoadControlVector.size();
}

void cvInternalGlobals::setModLoadControlVector(const char* szModule)
{
	m_paModLoadControlVector.push_back(szModule);
}

CvString cvInternalGlobals::getModLoadControlVector(int i) const
{
	return (CvString)m_paModLoadControlVector.at(i);
}

int cvInternalGlobals::getTotalNumModules() const
{
	return m_iTotalNumModules;
}

void cvInternalGlobals::setTotalNumModules()
{
	m_iTotalNumModules++;
}

int cvInternalGlobals::getNumModLoadControlInfos() const
{
	return (int)m_paModLoadControls.size();
}

CvModLoadControlInfo& cvInternalGlobals::getModLoadControlInfos(int iIndex) const
{
	FASSERT_BOUNDS(0, GC.getNumModLoadControlInfos(), iIndex);
	return infoArrayAt(m_paModLoadControls, iIndex, "m_paModLoadControls");
}


/************************************************************************************************/
/* MODULAR_LOADING_CONTROL                 END                                                  */
/************************************************************************************************/

int cvInternalGlobals::getNumColorInfos() const
{
	return (int)m_paColorInfo.size();
}

CvColorInfo& cvInternalGlobals::getColorInfo(ColorTypes e) const
{
	FASSERT_BOUNDS(0, GC.getNumColorInfos(), e);
	return infoArrayAt(m_paColorInfo, e, "m_paColorInfo");
}


int cvInternalGlobals::getNumPlayerColorInfos() const
{
	return (int)m_paPlayerColorInfo.size();
}

CvPlayerColorInfo& cvInternalGlobals::getPlayerColorInfo(PlayerColorTypes e) const
{
	FASSERT_BOUNDS(0, GC.getNumPlayerColorInfos(), e);
	return infoArrayAt(m_paPlayerColorInfo, e, "m_paPlayerColorInfo");
}

int cvInternalGlobals::getNumAdvisorInfos() const
{
	return (int)m_paAdvisorInfo.size();
}

CvAdvisorInfo& cvInternalGlobals::getAdvisorInfo(AdvisorTypes e) const
{
	FASSERT_BOUNDS(0, GC.getNumAdvisorInfos(), e);
	return infoArrayAt(m_paAdvisorInfo, e, "m_paAdvisorInfo");
}

int cvInternalGlobals::getNumRouteModelInfos() const
{
	return (int)m_paRouteModelInfo.size();
}

CvRouteModelInfo& cvInternalGlobals::getRouteModelInfo(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumRouteModelInfos(), i);
	return infoArrayAt(m_paRouteModelInfo, i, "m_paRouteModelInfo");
}

int cvInternalGlobals::getNumRiverModelInfos() const
{
	return (int)m_paRiverModelInfo.size();
}

CvRiverModelInfo& cvInternalGlobals::getRiverModelInfo(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumRiverModelInfos(), i);
	return infoArrayAt(m_paRiverModelInfo, i, "m_paRiverModelInfo");
}

int cvInternalGlobals::getNumWaterPlaneInfos() const
{
	return (int)m_paWaterPlaneInfo.size();
}

CvWaterPlaneInfo& cvInternalGlobals::getWaterPlaneInfo(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumWaterPlaneInfos(), i);
	return infoArrayAt(m_paWaterPlaneInfo, i, "m_paWaterPlaneInfo");
}

int cvInternalGlobals::getNumTerrainPlaneInfos() const
{
	return (int)m_paTerrainPlaneInfo.size();
}

CvTerrainPlaneInfo& cvInternalGlobals::getTerrainPlaneInfo(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumTerrainPlaneInfos(), i);
	return infoArrayAt(m_paTerrainPlaneInfo, i, "m_paTerrainPlaneInfo");
}

int cvInternalGlobals::getNumCameraOverlayInfos() const
{
	return (int)m_paCameraOverlayInfo.size();
}

CvCameraOverlayInfo& cvInternalGlobals::getCameraOverlayInfo(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumCameraOverlayInfos(), i);
	return infoArrayAt(m_paCameraOverlayInfo, i, "m_paCameraOverlayInfo");
}

int cvInternalGlobals::getNumAnimationPathInfos() const
{
	return (int)m_paAnimationPathInfo.size();
}

CvAnimationPathInfo& cvInternalGlobals::getAnimationPathInfo(AnimationPathTypes e) const
{
	FASSERT_BOUNDS(0, GC.getNumAnimationPathInfos(), e);
	return infoArrayAt(m_paAnimationPathInfo, e, "m_paAnimationPathInfo");
}

int cvInternalGlobals::getNumAnimationCategoryInfos() const
{
	return (int)m_paAnimationCategoryInfo.size();
}

CvAnimationCategoryInfo& cvInternalGlobals::getAnimationCategoryInfo(AnimationCategoryTypes e) const
{
	FASSERT_BOUNDS(0, GC.getNumAnimationCategoryInfos(), e);
	return infoArrayAt(m_paAnimationCategoryInfo, e, "m_paAnimationCategoryInfo");
}

int cvInternalGlobals::getNumEntityEventInfos() const
{
	return (int)m_paEntityEventInfo.size();
}

CvEntityEventInfo& cvInternalGlobals::getEntityEventInfo(EntityEventTypes e) const
{
	FASSERT_BOUNDS(0, GC.getNumEntityEventInfos(), e);
	return infoArrayAt(m_paEntityEventInfo, e, "m_paEntityEventInfo");
}

int cvInternalGlobals::getNumEffectInfos() const
{
	return (int)m_paEffectInfo.size();
}

CvEffectInfo& cvInternalGlobals::getEffectInfo(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumEffectInfos(), i);
	return infoArrayAt(m_paEffectInfo, i, "m_paEffectInfo");
}


int cvInternalGlobals::getNumAttachableInfos() const
{
	return (int)m_paAttachableInfo.size();
}

CvAttachableInfo& cvInternalGlobals::getAttachableInfo(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumAttachableInfos(), i);
	return infoArrayAt(m_paAttachableInfo, i, "m_paAttachableInfo");
}

int cvInternalGlobals::getNumUnitFormationInfos() const
{
	return (int)m_paUnitFormationInfo.size();
}

CvUnitFormationInfo& cvInternalGlobals::getUnitFormationInfo(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumUnitFormationInfos(), i);
	return infoArrayAt(m_paUnitFormationInfo, i, "m_paUnitFormationInfo");
}


int cvInternalGlobals::getNumLandscapeInfos() const
{
	return (int)m_paLandscapeInfo.size();
}

CvLandscapeInfo& cvInternalGlobals::getLandscapeInfo(int iIndex) const
{
	FASSERT_BOUNDS(0, GC.getNumLandscapeInfos(), iIndex);
	return infoArrayAt(m_paLandscapeInfo, iIndex, "m_paLandscapeInfo");
}

int cvInternalGlobals::getActiveLandscapeID() const
{
	return m_iActiveLandscapeID;
}

void cvInternalGlobals::setActiveLandscapeID(int iLandscapeID)
{
	m_iActiveLandscapeID = iLandscapeID;
}


int cvInternalGlobals::getNumTerrainInfos() const
{
	return (int)m_paTerrainInfo.size();
}

CvTerrainInfo& cvInternalGlobals::getTerrainInfo(TerrainTypes eTerrainNum) const
{
	FASSERT_BOUNDS(0, GC.getNumTerrainInfos(), eTerrainNum);
	// #430: the JSON-mapped shim leaf (the payload IS a CvTerrainInfo); the XML m_paTerrainInfo load is demolition
	// fodder cut at the atomic cutover (cascade-engine-430.md §3).
	return *static_cast<CvTerrainInfo*>(InfoRepo<CvTerrainInfo>::get().atPtr(eTerrainNum, "CvTerrainInfo"));
}

int cvInternalGlobals::getNumBonusClassInfos() const
{
	return (int)m_paBonusClassInfo.size();
}

CvBonusClassInfo& cvInternalGlobals::getBonusClassInfo(BonusClassTypes eBonusNum) const
{
	FASSERT_BOUNDS(0, GC.getNumBonusClassInfos(), eBonusNum);
	return infoArrayAt(m_paBonusClassInfo, eBonusNum, "m_paBonusClassInfo");
}


int cvInternalGlobals::getNumBonusInfos() const
{
	return (int)m_paBonusInfo.size();
}

const std::vector<CvBonusInfo*>& cvInternalGlobals::getBonusInfos() const
{
	return m_paBonusInfo;
}

CvBonusInfo& cvInternalGlobals::getBonusInfo(BonusTypes eBonusNum) const
{
	FASSERT_BOUNDS(0, GC.getNumBonusInfos(), eBonusNum);
	return *static_cast<CvBonusInfo*>(InfoRepo<CvBonusInfo>::get().atPtr(eBonusNum, "CvBonusInfo"));   // #430: JSON shim leaf (see getTerrainInfo)
}

int cvInternalGlobals::getNumMapBonuses() const
{
	return (int)m_mapBonuses.size();
}

BonusTypes cvInternalGlobals::getMapBonus(const int i) const
{
	FASSERT_BOUNDS(0, (int)m_mapBonuses.size(), i);
	return m_mapBonuses[i];
}

PromotionTypes cvInternalGlobals::getStatusPromotion(int i) const
{
	return m_aiStatusPromotions[i];
}

int cvInternalGlobals::getNumStatusPromotions() const
{
	return (int)m_aiStatusPromotions.size();
}

int cvInternalGlobals::getNumFeatureInfos() const
{
	return (int)m_paFeatureInfo.size();
}

CvFeatureInfo& cvInternalGlobals::getFeatureInfo(FeatureTypes eFeatureNum) const
{
	FASSERT_BOUNDS(0, GC.getNumFeatureInfos(), eFeatureNum);
	return *static_cast<CvFeatureInfo*>(InfoRepo<CvFeatureInfo>::get().atPtr(eFeatureNum, "CvFeatureInfo"));   // #430: JSON shim leaf (see getTerrainInfo)
}

int& cvInternalGlobals::getNumPlayableCivilizationInfos()
{
	return m_iNumPlayableCivilizationInfos;
}

int& cvInternalGlobals::getNumAIPlayableCivilizationInfos()
{
	return m_iNumAIPlayableCivilizationInfos;
}

int cvInternalGlobals::getNumCivilizationInfos() const
{
	return (int)m_paCivilizationInfo.size();
}

CvCivilizationInfo& cvInternalGlobals::getCivilizationInfo(CivilizationTypes eCivilizationNum) const
{
	FASSERT_BOUNDS(0, GC.getNumCivilizationInfos(), eCivilizationNum);
	return infoArrayAt(m_paCivilizationInfo, eCivilizationNum, "m_paCivilizationInfo");
}


int cvInternalGlobals::getNumLeaderHeadInfos() const
{
	return (int)m_paLeaderHeadInfo.size();
}

CvLeaderHeadInfo& cvInternalGlobals::getLeaderHeadInfo(LeaderHeadTypes eLeaderHeadNum) const
{
	FASSERT_BOUNDS(0, GC.getNumLeaderHeadInfos(), eLeaderHeadNum);
	return infoArrayAt(m_paLeaderHeadInfo, eLeaderHeadNum, "m_paLeaderHeadInfo");
}


int cvInternalGlobals::getNumTraitInfos() const
{
	return (int)m_paTraitInfo.size();
}

CvTraitInfo& cvInternalGlobals::getTraitInfo(TraitTypes eTraitNum) const
{
	FASSERT_BOUNDS(0, GC.getNumTraitInfos(), eTraitNum);
	// The ACTIVE trait set is chosen PURELY by GAMEOPTION_LEADER_COMPLEX_TRAITS (the two sets share the engine id, so
	// they live in separate repos -- cascade-engine-430.md §6 / modifier.md §4). complex/ is SELF-COMPLETE (a SUPERSET
	// of simple/), so under the complex option EVERY id MUST resolve in the complex repo -- a miss is a CURATION defect
	// (a trait absent from complex/), asserted LOUD and NEVER silently served from the simple set (owner ruling
	// 2026-07-21: a simple trait must never reach a complex game). The FASSERT fires in dev; the self-complete data
	// means it cannot fire on shipped content, and the Release fall-through is a crash-avoidance floor, not a fallback.
	if (getGame().isOption(GAMEOPTION_LEADER_COMPLEX_TRAITS))
	{
		const bool bHasComplex = InfoRepo<CvComplexTraitTag>::get().get(eTraitNum) != NULL;
		FAssertMsg(bHasComplex, CvString::format("TRAIT %d absent from the self-complete complex set (curation gap)", (int)eTraitNum).c_str());
		if (bHasComplex)
			return *static_cast<CvComplexTraitInfo*>(InfoRepo<CvComplexTraitTag>::get().atPtr(eTraitNum, "CvComplexTraitTag"));
	}
	return *static_cast<CvSimpleTraitInfo*>(InfoRepo<CvTraitInfo>::get().atPtr(eTraitNum, "CvTraitInfo"));
}


int cvInternalGlobals::getNumCursorInfos() const
{
	return (int)m_paCursorInfo.size();
}

CvCursorInfo& cvInternalGlobals::getCursorInfo(CursorTypes eCursorNum) const
{
	FASSERT_BOUNDS(0, GC.getNumCursorInfos(), eCursorNum);
	return infoArrayAt(m_paCursorInfo, eCursorNum, "m_paCursorInfo");
}

int cvInternalGlobals::getNumThroneRoomCameras() const
{
	return (int)m_paThroneRoomCamera.size();
}

CvThroneRoomCamera& cvInternalGlobals::getThroneRoomCamera(int iIndex) const
{
	FASSERT_BOUNDS(0, GC.getNumThroneRoomCameras(), iIndex);
	return infoArrayAt(m_paThroneRoomCamera, iIndex, "m_paThroneRoomCamera");
}

int cvInternalGlobals::getNumThroneRoomInfos() const
{
	return (int)m_paThroneRoomInfo.size();
}

CvThroneRoomInfo& cvInternalGlobals::getThroneRoomInfo(int iIndex) const
{
	FASSERT_BOUNDS(0, GC.getNumThroneRoomInfos(), iIndex);
	return infoArrayAt(m_paThroneRoomInfo, iIndex, "m_paThroneRoomInfo");
}

int cvInternalGlobals::getNumThroneRoomStyleInfos() const
{
	return (int)m_paThroneRoomStyleInfo.size();
}

CvThroneRoomStyleInfo& cvInternalGlobals::getThroneRoomStyleInfo(int iIndex) const
{
	FASSERT_BOUNDS(0, GC.getNumThroneRoomStyleInfos(), iIndex);
	return infoArrayAt(m_paThroneRoomStyleInfo, iIndex, "m_paThroneRoomStyleInfo");
}

int cvInternalGlobals::getNumSlideShowInfos() const
{
	return (int)m_paSlideShowInfo.size();
}

CvSlideShowInfo& cvInternalGlobals::getSlideShowInfo(int iIndex) const
{
	FASSERT_BOUNDS(0, GC.getNumSlideShowInfos(), iIndex);
	return infoArrayAt(m_paSlideShowInfo, iIndex, "m_paSlideShowInfo");
}

int cvInternalGlobals::getNumSlideShowRandomInfos() const
{
	return (int)m_paSlideShowRandomInfo.size();
}

CvSlideShowRandomInfo& cvInternalGlobals::getSlideShowRandomInfo(int iIndex) const
{
	FASSERT_BOUNDS(0, GC.getNumSlideShowRandomInfos(), iIndex);
	return infoArrayAt(m_paSlideShowRandomInfo, iIndex, "m_paSlideShowRandomInfo");
}

int cvInternalGlobals::getNumWorldPickerInfos() const
{
	return (int)m_paWorldPickerInfo.size();
}

CvWorldPickerInfo& cvInternalGlobals::getWorldPickerInfo(int iIndex) const
{
	FASSERT_BOUNDS(0, GC.getNumWorldPickerInfos(), iIndex);
	return infoArrayAt(m_paWorldPickerInfo, iIndex, "m_paWorldPickerInfo");
}

int cvInternalGlobals::getNumSpaceShipInfos() const
{
	return (int)m_paSpaceShipInfo.size();
}

CvSpaceShipInfo& cvInternalGlobals::getSpaceShipInfo(int iIndex) const
{
	FASSERT_BOUNDS(0, GC.getNumSpaceShipInfos(), iIndex);
	return infoArrayAt(m_paSpaceShipInfo, iIndex, "m_paSpaceShipInfo");
}

int cvInternalGlobals::getNumUnitInfos() const
{
	return (int)m_paUnitInfo.size();
}

CvUnitInfo& cvInternalGlobals::getUnitInfo(UnitTypes eUnitNum) const
{
	FASSERT_BOUNDS(0, GC.getNumUnitInfos(), eUnitNum);
	return *static_cast<CvUnitInfo*>(InfoRepo<CvUnitInfo>::get().atPtr(eUnitNum, "CvUnitInfo"));
}

int cvInternalGlobals::getNumSpawnInfos() const
{
	return (int)m_paSpawnInfo.size();
}

CvSpawnInfo& cvInternalGlobals::getSpawnInfo(SpawnTypes eSpawnNum) const
{
	FASSERT_BOUNDS(0, GC.getNumSpawnInfos(), eSpawnNum);
	return infoArrayAt(m_paSpawnInfo, eSpawnNum, "m_paSpawnInfo");
}

int cvInternalGlobals::getNumSpecialUnitInfos() const
{
	return (int)m_paSpecialUnitInfo.size();
}

CvSpecialUnitInfo& cvInternalGlobals::getSpecialUnitInfo(SpecialUnitTypes eSpecialUnitNum) const
{
	FASSERT_BOUNDS(0, GC.getNumSpecialUnitInfos(), eSpecialUnitNum);
	return infoArrayAt(m_paSpecialUnitInfo, eSpecialUnitNum, "m_paSpecialUnitInfo");
}


int cvInternalGlobals::getNumConceptInfos() const
{
	return (int)m_paConceptInfo.size();
}

CvInfoBase& cvInternalGlobals::getConceptInfo(ConceptTypes e) const
{
	FASSERT_BOUNDS(0, GC.getNumConceptInfos(), e);
	return infoArrayAt(m_paConceptInfo, e, "m_paConceptInfo");
}


int cvInternalGlobals::getNumNewConceptInfos() const
{
	return (int)m_paNewConceptInfo.size();
}

CvInfoBase& cvInternalGlobals::getNewConceptInfo(NewConceptTypes e) const
{
	FASSERT_BOUNDS(0, GC.getNumNewConceptInfos(), e);
	return infoArrayAt(m_paNewConceptInfo, e, "m_paNewConceptInfo");
}


int cvInternalGlobals::getNumCityTabInfos() const
{
	return (int)m_paCityTabInfo.size();
}

CvInfoBase& cvInternalGlobals::getCityTabInfo(CityTabTypes e) const
{
	FASSERT_BOUNDS(0, GC.getNumCityTabInfos(), e);
	return infoArrayAt(m_paCityTabInfo, e, "m_paCityTabInfo");
}


int cvInternalGlobals::getNumCalendarInfos() const
{
	return (int)m_paCalendarInfo.size();
}

CvInfoBase& cvInternalGlobals::getCalendarInfo(CalendarTypes e) const
{
	FASSERT_BOUNDS(0, GC.getNumCalendarInfos(), e);
	return infoArrayAt(m_paCalendarInfo, e, "m_paCalendarInfo");
}


int cvInternalGlobals::getNumSeasonInfos() const
{
	return (int)m_paSeasonInfo.size();
}

CvInfoBase& cvInternalGlobals::getSeasonInfo(SeasonTypes e) const
{
	FASSERT_BOUNDS(0, GC.getNumSeasonInfos(), e);
	return infoArrayAt(m_paSeasonInfo, e, "m_paSeasonInfo");
}


int cvInternalGlobals::getNumMonthInfos() const
{
	return (int)m_paMonthInfo.size();
}

CvInfoBase& cvInternalGlobals::getMonthInfo(MonthTypes e) const
{
	FASSERT_BOUNDS(0, GC.getNumMonthInfos(), e);
	return infoArrayAt(m_paMonthInfo, e, "m_paMonthInfo");
}


int cvInternalGlobals::getNumDenialInfos() const
{
	return (int)m_paDenialInfo.size();
}

CvInfoBase& cvInternalGlobals::getDenialInfo(DenialTypes e) const
{
	FASSERT_BOUNDS(0, GC.getNumDenialInfos(), e);
	return infoArrayAt(m_paDenialInfo, e, "m_paDenialInfo");
}


int cvInternalGlobals::getNumInvisibleInfos() const
{
	return (int)m_paInvisibleInfo.size();
}

CvInvisibleInfo& cvInternalGlobals::getInvisibleInfo(InvisibleTypes e) const
{
	FASSERT_BOUNDS(0, GC.getNumInvisibleInfos(), e);
	return infoArrayAt(m_paInvisibleInfo, e, "m_paInvisibleInfo");
}


int cvInternalGlobals::getNumCategoryInfos() const
{
	return (int)m_paCategoryInfo.size();
}

CvCategoryInfo& cvInternalGlobals::getCategoryInfo(CategoryTypes e) const
{
	FASSERT_BOUNDS(0, GC.getNumCategoryInfos(), e);
	return infoArrayAt(m_paCategoryInfo, e, "m_paCategoryInfo");
}

int cvInternalGlobals::getNumHeritageInfos() const
{
	return (int)m_heritageInfo.size();
}

CvHeritageInfo& cvInternalGlobals::getHeritageInfo(HeritageTypes e) const
{
	FASSERT_BOUNDS(0, GC.getNumHeritageInfos(), e);
	return *static_cast<CvHeritageInfo*>(InfoRepo<CvHeritageInfo>::get().atPtr(e, "CvHeritageInfo"));
}


int cvInternalGlobals::getNumVoteSourceInfos() const
{
	return (int)m_paVoteSourceInfo.size();
}

CvVoteSourceInfo& cvInternalGlobals::getVoteSourceInfo(VoteSourceTypes e) const
{
	FASSERT_BOUNDS(0, GC.getNumVoteSourceInfos(), e);
	return infoArrayAt(m_paVoteSourceInfo, e, "m_paVoteSourceInfo");
}


int cvInternalGlobals::getNumUnitCombatInfos() const
{
	return (int)m_paUnitCombatInfo.size();
}

CvUnitCombatInfo& cvInternalGlobals::getUnitCombatInfo(UnitCombatTypes e) const
{
	FASSERT_BOUNDS(0, GC.getNumUnitCombatInfos(), e);
	return *static_cast<CvUnitCombatInfo*>(InfoRepo<CvUnitCombatInfo>::get().atPtr(e, "CvUnitCombatInfo"));
}


CvInfoBase& cvInternalGlobals::getDomainInfo(DomainTypes e) const
{
	FASSERT_BOUNDS(0, NUM_DOMAIN_TYPES, e)
	return infoArrayAt(m_paDomainInfo, e, "m_paDomainInfo");
}

int cvInternalGlobals::getNumPromotionLineInfos() const
{
	return (int)m_paPromotionLineInfo.size();
}

CvPromotionLineInfo& cvInternalGlobals::getPromotionLineInfo(PromotionLineTypes e) const
{
	FASSERT_BOUNDS(0, GC.getNumPromotionLineInfos(), e);
	return *static_cast<CvPromotionLineInfo*>(InfoRepo<CvPromotionLineInfo>::get().atPtr(e, "CvPromotionLineInfo"));
}

int cvInternalGlobals::getNumMapCategoryInfos() const
{
	return (int)m_paMapCategoryInfo.size();
}

CvMapCategoryInfo& cvInternalGlobals::getMapCategoryInfo(MapCategoryTypes e) const
{
	FASSERT_BOUNDS(0, GC.getNumMapCategoryInfos(), e);
	return infoArrayAt(m_paMapCategoryInfo, e, "m_paMapCategoryInfo");
}

int cvInternalGlobals::getNumIdeaClassInfos() const
{
	return (int)m_paIdeaClassInfo.size();
}

CvIdeaClassInfo& cvInternalGlobals::getIdeaClassInfo(IdeaClassTypes e) const
{
	FASSERT_BOUNDS(0, GC.getNumIdeaClassInfos(), e);
	return infoArrayAt(m_paIdeaClassInfo, e, "m_paIdeaClassInfo");
}

int cvInternalGlobals::getNumIdeaInfos() const
{
	return (int)m_paIdeaInfo.size();
}

CvIdeaInfo& cvInternalGlobals::getIdeaInfo(IdeaTypes e) const
{
	FASSERT_BOUNDS(0, GC.getNumIdeaInfos(), e);
	return infoArrayAt(m_paIdeaInfo, e, "m_paIdeaInfo");
}
//int cvInternalGlobals::getNumTraitOptionEditsInfos() const
//{
//	return (int)m_paTraitOptionEditsInfo.size();
//}
//
//CvTraitOptionEditsInfo& cvInternalGlobals::getTraitOptionEditsInfo(TraitOptionEditsTypes e) const
//{
//	FASSERT_BOUNDS(0, GC.getNumTraitOptionEditsInfos(), e);
//	return infoArrayAt(m_paTraitOptionEditsInfo, e, "m_paTraitOptionEditsInfo");
//}


//	Toffer - Added internal registration of plot types
#define	REGISTER_PLOT_TYPE(x)	setInfoTypeFromString(#x,x)

void cvInternalGlobals::registerPlotTypes()
{
	REGISTER_PLOT_TYPE(NO_PLOT);
	REGISTER_PLOT_TYPE(PLOT_PEAK);
	REGISTER_PLOT_TYPE(PLOT_HILLS);
	REGISTER_PLOT_TYPE(PLOT_LAND);
	REGISTER_PLOT_TYPE(PLOT_OCEAN);
}
// ! Toffer

//	Koshling - added internal registration of supported UnitAI types, not reliant
//	on external definition in XML
CvInfoBase& cvInternalGlobals::getUnitAIInfo(UnitAITypes eUnitAINum) const
{
	FASSERT_BOUNDS(0, NUM_UNITAI_TYPES, eUnitAINum);
	return infoArrayAt(m_paUnitAIInfos, eUnitAINum, "m_paUnitAIInfos");
}


void cvInternalGlobals::registerUnitAI(const char* szType, int enumVal)
{
	FAssertMsg(m_paUnitAIInfos.size() == enumVal, "enumVal not expected value");

	CvInfoBase* entry = new	CvInfoBase(szType);

	m_paUnitAIInfos.push_back(entry);
	setInfoTypeFromString(szType, enumVal);
}




#define	REGISTER_UNITAI(x)	registerUnitAI(#x,x)

void cvInternalGlobals::registerUnitAIs()
{
	//	Sadly C++ doesn't have any reflection capability so need to do this explicitly
	REGISTER_UNITAI(UNITAI_UNKNOWN);
	REGISTER_UNITAI(UNITAI_ANIMAL);
	REGISTER_UNITAI(UNITAI_SETTLE);
	REGISTER_UNITAI(UNITAI_WORKER);
	REGISTER_UNITAI(UNITAI_ATTACK);
	REGISTER_UNITAI(UNITAI_ATTACK_CITY);
	REGISTER_UNITAI(UNITAI_COLLATERAL);
	REGISTER_UNITAI(UNITAI_PILLAGE);
	REGISTER_UNITAI(UNITAI_RESERVE);
	REGISTER_UNITAI(UNITAI_COUNTER);
	REGISTER_UNITAI(UNITAI_CITY_DEFENSE);
	REGISTER_UNITAI(UNITAI_CITY_COUNTER);
	REGISTER_UNITAI(UNITAI_CITY_SPECIAL);
	REGISTER_UNITAI(UNITAI_EXPLORE);
	REGISTER_UNITAI(UNITAI_MISSIONARY);
	REGISTER_UNITAI(UNITAI_PROPHET);
	REGISTER_UNITAI(UNITAI_ARTIST);
	REGISTER_UNITAI(UNITAI_SCIENTIST);
	REGISTER_UNITAI(UNITAI_GENERAL);
	REGISTER_UNITAI(UNITAI_MERCHANT);
	REGISTER_UNITAI(UNITAI_ENGINEER);
	REGISTER_UNITAI(UNITAI_SPY);
	REGISTER_UNITAI(UNITAI_ICBM);
	REGISTER_UNITAI(UNITAI_WORKER_SEA);
	REGISTER_UNITAI(UNITAI_ATTACK_SEA);
	REGISTER_UNITAI(UNITAI_RESERVE_SEA);
	REGISTER_UNITAI(UNITAI_ESCORT_SEA);
	REGISTER_UNITAI(UNITAI_EXPLORE_SEA);
	REGISTER_UNITAI(UNITAI_ASSAULT_SEA);
	REGISTER_UNITAI(UNITAI_SETTLER_SEA);
	REGISTER_UNITAI(UNITAI_MISSIONARY_SEA);
	REGISTER_UNITAI(UNITAI_SPY_SEA);
	REGISTER_UNITAI(UNITAI_CARRIER_SEA);
	REGISTER_UNITAI(UNITAI_MISSILE_CARRIER_SEA);
	REGISTER_UNITAI(UNITAI_PIRATE_SEA);
	REGISTER_UNITAI(UNITAI_ATTACK_AIR);
	REGISTER_UNITAI(UNITAI_DEFENSE_AIR);
	REGISTER_UNITAI(UNITAI_CARRIER_AIR);
	REGISTER_UNITAI(UNITAI_MISSILE_AIR);
	REGISTER_UNITAI(UNITAI_PARADROP);
	REGISTER_UNITAI(UNITAI_ATTACK_CITY_LEMMING);
	REGISTER_UNITAI(UNITAI_PILLAGE_COUNTER);
	REGISTER_UNITAI(UNITAI_SUBDUED_ANIMAL);
	REGISTER_UNITAI(UNITAI_HUNTER);
	REGISTER_UNITAI(UNITAI_GREAT_HUNTER);
	REGISTER_UNITAI(UNITAI_GREAT_ADMIRAL);
	REGISTER_UNITAI(UNITAI_PROPERTY_CONTROL);
	REGISTER_UNITAI(UNITAI_HEALER);
	REGISTER_UNITAI(UNITAI_PROPERTY_CONTROL_SEA);
	REGISTER_UNITAI(UNITAI_HEALER_SEA);
	REGISTER_UNITAI(UNITAI_HUNTER_ESCORT);
	REGISTER_UNITAI(UNITAI_BARB_CRIMINAL);
	REGISTER_UNITAI(UNITAI_INVESTIGATOR);
	REGISTER_UNITAI(UNITAI_INFILTRATOR);
	REGISTER_UNITAI(UNITAI_SEE_INVISIBLE);
	REGISTER_UNITAI(UNITAI_SEE_INVISIBLE_SEA);
	REGISTER_UNITAI(UNITAI_ESCORT);
}

//	AIAndy - added internal registration of supported AIScale types similar to UnitAIs but without info class
#define	REGISTER_AISCALE(x)	setInfoTypeFromString(#x,x)

void cvInternalGlobals::registerAIScales()
{
	//	Sadly C++ doesn't have any reflection capability so need to do this explicitly
	REGISTER_AISCALE(AISCALE_NONE);
	REGISTER_AISCALE(AISCALE_CITY);
	REGISTER_AISCALE(AISCALE_AREA);
	REGISTER_AISCALE(AISCALE_PLAYER);
	REGISTER_AISCALE(AISCALE_TEAM);
}

//	AIAndy: Register game object types
#define	REGISTER_GAMEOBJECT(x)	setInfoTypeFromString(#x,x)

void cvInternalGlobals::registerGameObjects()
{
	//	Sadly C++ doesn't have any reflection capability so need to do this explicitly
	REGISTER_GAMEOBJECT(NO_GAMEOBJECT);
	REGISTER_GAMEOBJECT(GAMEOBJECT_GAME);
	REGISTER_GAMEOBJECT(GAMEOBJECT_TEAM);
	REGISTER_GAMEOBJECT(GAMEOBJECT_PLAYER);
	REGISTER_GAMEOBJECT(GAMEOBJECT_CITY);
	REGISTER_GAMEOBJECT(GAMEOBJECT_UNIT);
	REGISTER_GAMEOBJECT(GAMEOBJECT_PLOT);
}

//	AIAndy: Register game object types
#define	REGISTER_GOM(x)	setInfoTypeFromString(#x,x)

void cvInternalGlobals::registerGOMs()
{
	//	Sadly C++ doesn't have any reflection capability so need to do this explicitly
	REGISTER_GOM(NO_GOM);
	REGISTER_GOM(GOM_HERITAGE);
	REGISTER_GOM(GOM_BUILDING);
	REGISTER_GOM(GOM_PROMOTION);
	REGISTER_GOM(GOM_TRAIT);
	REGISTER_GOM(GOM_FEATURE);
	REGISTER_GOM(GOM_OPTION);
	REGISTER_GOM(GOM_TERRAIN);
	REGISTER_GOM(GOM_GAMESPEED);
	REGISTER_GOM(GOM_ROUTE);
	REGISTER_GOM(GOM_BONUS);
	REGISTER_GOM(GOM_UNITTYPE);
	REGISTER_GOM(GOM_TECH);
	REGISTER_GOM(GOM_CIVIC);
	REGISTER_GOM(GOM_RELIGION);
	REGISTER_GOM(GOM_CORPORATION);
	REGISTER_GOM(GOM_IMPROVEMENT);
	REGISTER_GOM(GOM_UNITCOMBAT);
	REGISTER_GOM(GOM_HANDICAP);
}

//	AIAndy: Register game object relation types
#define	REGISTER_RELATION(x)	setInfoTypeFromString(#x,x)

void cvInternalGlobals::registerRelations()
{
	//	Sadly C++ doesn't have any reflection capability so need to do this explicitly
	REGISTER_RELATION(NO_RELATION);
	REGISTER_RELATION(RELATION_ASSOCIATED);
	REGISTER_RELATION(RELATION_TRADE);
	REGISTER_RELATION(RELATION_NEAR);
	REGISTER_RELATION(RELATION_SAME_PLOT);
	REGISTER_RELATION(RELATION_WORKING);
}

//	AIAndy: Register game object attribute types
#define	REGISTER_ATTRIBUTE(x)	setInfoTypeFromString(#x,x)

void cvInternalGlobals::registerAttributes()
{
	//	Sadly C++ doesn't have any reflection capability so need to do this explicitly
	REGISTER_ATTRIBUTE(NO_ATTRIBUTE);
	REGISTER_ATTRIBUTE(ATTRIBUTE_POPULATION);
	REGISTER_ATTRIBUTE(ATTRIBUTE_HEALTH);
	REGISTER_ATTRIBUTE(ATTRIBUTE_HAPPINESS);
	REGISTER_ATTRIBUTE(ATTRIBUTE_PLAYERS);
	REGISTER_ATTRIBUTE(ATTRIBUTE_TEAMS);
}

//	AIAndy: Register game object tag types
#define	REGISTER_TAG(x)	setInfoTypeFromString(#x,x)

void cvInternalGlobals::registerTags()
{
	//	Sadly C++ doesn't have any reflection capability so need to do this explicitly
	REGISTER_TAG(NO_TAG);
	REGISTER_TAG(TAG_ONLY_DEFENSIVE);
	REGISTER_TAG(TAG_SPY);
	REGISTER_TAG(TAG_FIRST_STRIKE_IMMUNE);
	REGISTER_TAG(TAG_NO_DEFENSIVE_BONUS);
	REGISTER_TAG(TAG_CAN_MOVE_IMPASSABLE);
	REGISTER_TAG(TAG_HIDDEN_NATIONALITY);
	REGISTER_TAG(TAG_BLITZ);
	REGISTER_TAG(TAG_ALWAYS_HEAL);
	REGISTER_TAG(TAG_ENEMY_ROUTE);
	REGISTER_TAG(TAG_WATER);
	REGISTER_TAG(TAG_FRESH_WATER);
	REGISTER_TAG(TAG_PEAK);
	REGISTER_TAG(TAG_HILL);
	REGISTER_TAG(TAG_FLATLAND);
	REGISTER_TAG(TAG_OWNED);
	REGISTER_TAG(TAG_CITY);
	REGISTER_TAG(TAG_ANARCHY);
	REGISTER_TAG(TAG_COASTAL);
}

//	AIAndy: Register property source types
#define	REGISTER_PROPERTYSOURCE(x)	setInfoTypeFromString(#x,x)

void cvInternalGlobals::registerPropertySources()
{
	//	Sadly C++ doesn't have any reflection capability so need to do this explicitly
	REGISTER_PROPERTYSOURCE(NO_PROPERTYSOURCE);
	REGISTER_PROPERTYSOURCE(PROPERTYSOURCE_CONSTANT);
	REGISTER_PROPERTYSOURCE(PROPERTYSOURCE_CONSTANT_LIMITED);
	REGISTER_PROPERTYSOURCE(PROPERTYSOURCE_DECAY);
	REGISTER_PROPERTYSOURCE(PROPERTYSOURCE_ATTRIBUTE_CONSTANT);
}

//	AIAndy: Register property interaction types
#define	REGISTER_PROPERTYINTERACTION(x)	setInfoTypeFromString(#x,x)

void cvInternalGlobals::registerPropertyInteractions()
{
	//	Sadly C++ doesn't have any reflection capability so need to do this explicitly
	REGISTER_PROPERTYINTERACTION(NO_PROPERTYINTERACTION);
	REGISTER_PROPERTYINTERACTION(PROPERTYINTERACTION_CONVERT_CONSTANT);
	REGISTER_PROPERTYINTERACTION(PROPERTYINTERACTION_INHIBITED_GROWTH);
	REGISTER_PROPERTYINTERACTION(PROPERTYINTERACTION_CONVERT_PERCENT);
}

//	AIAndy: Register property propagator types
#define	REGISTER_PROPERTYPROPAGATOR(x)	setInfoTypeFromString(#x,x)

void cvInternalGlobals::registerPropertyPropagators()
{
	//	Sadly C++ doesn't have any reflection capability so need to do this explicitly
	REGISTER_PROPERTYPROPAGATOR(NO_PROPERTYPROPAGATOR);
	REGISTER_PROPERTYPROPAGATOR(PROPERTYPROPAGATOR_SPREAD);
	REGISTER_PROPERTYPROPAGATOR(PROPERTYPROPAGATOR_GATHER);
	REGISTER_PROPERTYPROPAGATOR(PROPERTYPROPAGATOR_DIFFUSE);
}

//	AlbertS2: Register mission types
void cvInternalGlobals::registerMission(const char* szType, int enumVal)
{
	FAssert(m_paMissionInfo.size() == enumVal);

	CvMissionInfo* entry = new CvMissionInfo(szType);

	m_paMissionInfo.push_back(entry);
	setInfoTypeFromString(szType, enumVal);
}

#define	REGISTER_MISSION(x)	registerMission(#x,x)

void cvInternalGlobals::registerMissions()
{
	//	Sadly C++ doesn't have any reflection capability so need to do this explicitly
	REGISTER_MISSION(MISSION_MOVE_TO);
	REGISTER_MISSION(MISSION_ROUTE_TO);
	REGISTER_MISSION(MISSION_MOVE_TO_UNIT);
	REGISTER_MISSION(MISSION_SKIP);
	REGISTER_MISSION(MISSION_SLEEP);
	REGISTER_MISSION(MISSION_FORTIFY);
	REGISTER_MISSION(MISSION_PLUNDER);
	REGISTER_MISSION(MISSION_AIRPATROL);
	REGISTER_MISSION(MISSION_SEAPATROL);
	REGISTER_MISSION(MISSION_HEAL);
	REGISTER_MISSION(MISSION_SENTRY);
	REGISTER_MISSION(MISSION_AIRLIFT);
	REGISTER_MISSION(MISSION_NUKE);
	REGISTER_MISSION(MISSION_RECON);
	REGISTER_MISSION(MISSION_PARADROP);
	REGISTER_MISSION(MISSION_AIRBOMB);
	REGISTER_MISSION(MISSION_RANGE_ATTACK);
	REGISTER_MISSION(MISSION_BOMBARD);
	REGISTER_MISSION(MISSION_PILLAGE);
	REGISTER_MISSION(MISSION_SABOTAGE);
	REGISTER_MISSION(MISSION_DESTROY);
	REGISTER_MISSION(MISSION_STEAL_PLANS);
	REGISTER_MISSION(MISSION_FOUND);
	REGISTER_MISSION(MISSION_SPREAD);
	REGISTER_MISSION(MISSION_SPREAD_CORPORATION);
	REGISTER_MISSION(MISSION_JOIN);
	REGISTER_MISSION(MISSION_CONSTRUCT);
	REGISTER_MISSION(MISSION_DISCOVER);
	REGISTER_MISSION(MISSION_HURRY);
	REGISTER_MISSION(MISSION_TRADE);
	REGISTER_MISSION(MISSION_GREAT_WORK);
	REGISTER_MISSION(MISSION_INFILTRATE);
	REGISTER_MISSION(MISSION_GOLDEN_AGE);
	REGISTER_MISSION(MISSION_BUILD);
	REGISTER_MISSION(MISSION_LEAD);
	REGISTER_MISSION(MISSION_ESPIONAGE);
	REGISTER_MISSION(MISSION_DIE_ANIMATION);
	REGISTER_MISSION(MISSION_BEGIN_COMBAT);
	REGISTER_MISSION(MISSION_END_COMBAT);
	REGISTER_MISSION(MISSION_AIRSTRIKE);
	REGISTER_MISSION(MISSION_SURRENDER);
	REGISTER_MISSION(MISSION_CAPTURED);
	REGISTER_MISSION(MISSION_IDLE);
	REGISTER_MISSION(MISSION_DIE);
	REGISTER_MISSION(MISSION_DAMAGE);
	REGISTER_MISSION(MISSION_MULTI_SELECT);
	REGISTER_MISSION(MISSION_MULTI_DESELECT);
	REGISTER_MISSION(MISSION_FENGAGE);

	REGISTER_MISSION(MISSION_INQUISITION);
	REGISTER_MISSION(MISSION_CLAIM_TERRITORY);
	REGISTER_MISSION(MISSION_HURRY_FOOD);
	REGISTER_MISSION(MISSION_ESPIONAGE_SLEEP);
	REGISTER_MISSION(MISSION_GREAT_COMMANDER);
	REGISTER_MISSION(MISSION_GREAT_COMMODORE);
	REGISTER_MISSION(MISSION_SHADOW);
	REGISTER_MISSION(MISSION_GOTO);
	REGISTER_MISSION(MISSION_BUTCHER);
	REGISTER_MISSION(MISSION_DIPLOMAT_ASSIMULATE_IND_PEOPLE);
	REGISTER_MISSION(MISSION_DIPLOMAT_PRAISE_IND_PEOPLE);
	REGISTER_MISSION(MISSION_DIPLOMAT_SPEAK_TO_BARBARIAN_LEADERS);
	REGISTER_MISSION(MISSION_DIPLOMAT_SPREAD_RELIGION);
	REGISTER_MISSION(MISSION_LAWYER_REMOVE_CORPORATIONS);
	REGISTER_MISSION(MISSION_JOIN_CITY_POPULATION);
	REGISTER_MISSION(MISSION_CURE);
	REGISTER_MISSION(MISSION_BUILDUP);
	REGISTER_MISSION(MISSION_AUTO_BUILDUP);
	REGISTER_MISSION(MISSION_HEAL_BUILDUP);
	REGISTER_MISSION(MISSION_AMBUSH);
	REGISTER_MISSION(MISSION_ASSASSINATE);
	REGISTER_MISSION(MISSION_ENTERTAIN_CITY);
	REGISTER_MISSION(MISSION_HURRY_PRODUCTION_CARAVAN);
	REGISTER_MISSION(MISSION_CAPTIVE_UPGRADE_TO_SETTLER);
	REGISTER_MISSION(MISSION_CAPTIVE_UPGRADE_TO_GATHERER);
	REGISTER_MISSION(MISSION_CAPTIVE_UPGRADE_TO_WORKER);
	REGISTER_MISSION(MISSION_CAPTIVE_UPGRADE_TO_IMMIGRANT);
	REGISTER_MISSION(MISSION_CAPTIVE_UPGRADE_TO_STONE_THROWER);
	REGISTER_MISSION(MISSION_CAPTIVE_UPGRADE_TO_ARCHER);
	REGISTER_MISSION(MISSION_CAPTIVE_UPGRADE_TO_AXEMAN);
	REGISTER_MISSION(MISSION_CAPTIVE_UPGRADE_TO_SPEARMAN);
	REGISTER_MISSION(MISSION_CAPTIVE_UPGRADE_TO_NEANDERTHAL_WARRIOR);
	REGISTER_MISSION(MISSION_SELL_CAPTIVE);
	REGISTER_MISSION(MISSION_FREE_CAPTIVE);
	REGISTER_MISSION(MISSION_SLAY_ANIMAL);
	REGISTER_MISSION(MISSION_JOIN_CITY_FREED_SLAVE);
	REGISTER_MISSION(MISSION_RECORD_TALE);
	REGISTER_MISSION(MISSION_RECORD_TALE_ORAL);
	REGISTER_MISSION(MISSION_RECORD_TALE_WRITTEN);
	REGISTER_MISSION(MISSION_ANIMAL_COMBAT);
	REGISTER_MISSION(MISSION_ANIMAL_STUDY);
	REGISTER_MISSION(MISSION_ANIMAL_SACRIFICE);
	REGISTER_MISSION(MISSION_BUILD_DOMESTICATED_HERD);
	REGISTER_MISSION(MISSION_CAPTIVE_UPGRADE_TO_NEANDERTHAL_GATHERER);
	REGISTER_MISSION(MISSION_CAPTIVE_UPGRADE_TO_NEANDERTHAL_TRACKER);
	REGISTER_MISSION(MISSION_HERITAGE);

#ifdef _MOD_SENTRY
	REGISTER_MISSION(MISSION_MOVE_TO_SENTRY);
	REGISTER_MISSION(MISSION_SENTRY_WHILE_HEAL);
	REGISTER_MISSION(MISSION_SENTRY_NAVAL_UNITS);
	REGISTER_MISSION(MISSION_SENTRY_LAND_UNITS);
#endif
}

#define	REGISTER_NPC(x)	setInfoTypeFromString(#x,x)

void cvInternalGlobals::registerNPCPlayers()
{
	REGISTER_NPC(BEAST_PLAYER);
	REGISTER_NPC(PREDATOR_PLAYER);
	REGISTER_NPC(PREY_PLAYER);
	REGISTER_NPC(INSECT_PLAYER);
	REGISTER_NPC(NPC4_PLAYER);
	REGISTER_NPC(NPC3_PLAYER);
	REGISTER_NPC(NPC2_PLAYER);
	REGISTER_NPC(NPC1_PLAYER);
	REGISTER_NPC(NPC0_PLAYER);
	REGISTER_NPC(NEANDERTHAL_PLAYER);
	REGISTER_NPC(BARBARIAN_PLAYER);
}

#define	REGISTER_CLIMATE_ZONE(x) setInfoTypeFromString(#x,x)

void cvInternalGlobals::registerClimateZones()
{
	//	Sadly C++ doesn't have any reflection capability so need to do this explicitly
	REGISTER_CLIMATE_ZONE(NO_CLIMATE_ZONE);
	REGISTER_CLIMATE_ZONE(CLIMATE_ZONE_TROPICAL);
	REGISTER_CLIMATE_ZONE(CLIMATE_ZONE_TEMPERATE);
	REGISTER_CLIMATE_ZONE(CLIMATE_ZONE_POLAR);
}


CvInfoBase& cvInternalGlobals::getAttitudeInfo(AttitudeTypes eAttitudeNum) const
{
	FASSERT_BOUNDS(0, NUM_ATTITUDE_TYPES, eAttitudeNum);
	return infoArrayAt(m_paAttitudeInfos, eAttitudeNum, "m_paAttitudeInfos");
}


CvInfoBase& cvInternalGlobals::getMemoryInfo(MemoryTypes eMemoryNum) const
{
	FASSERT_BOUNDS(0, NUM_MEMORY_TYPES, eMemoryNum);
	return infoArrayAt(m_paMemoryInfos, eMemoryNum, "m_paMemoryInfos");
}


int cvInternalGlobals::getNumGameOptionInfos() const
{
	return (int)m_paGameOptionInfos.size();
}

CvGameOptionInfo& cvInternalGlobals::getGameOptionInfo(GameOptionTypes eGameOptionNum) const
{
	FASSERT_BOUNDS(0, getNumGameOptionInfos(), eGameOptionNum);
	return infoArrayAt(m_paGameOptionInfos, eGameOptionNum, "m_paGameOptionInfos");
}

int cvInternalGlobals::getNumMPOptionInfos() const
{
	return (int)m_paMPOptionInfos.size();
}

CvMPOptionInfo& cvInternalGlobals::getMPOptionInfo(MultiplayerOptionTypes eMPOptionNum) const
{
	FASSERT_BOUNDS(0, GC.getNumMPOptionInfos(), eMPOptionNum);
	return infoArrayAt(m_paMPOptionInfos, eMPOptionNum, "m_paMPOptionInfos");
}

int cvInternalGlobals::getNumForceControlInfos() const
{
	return (int)m_paForceControlInfos.size();
}

CvForceControlInfo& cvInternalGlobals::getForceControlInfo(ForceControlTypes eForceControlNum) const
{
	FASSERT_BOUNDS(0, GC.getNumForceControlInfos(), eForceControlNum);
	return infoArrayAt(m_paForceControlInfos, eForceControlNum, "m_paForceControlInfos");
}

CvPlayerOptionInfo& cvInternalGlobals::getPlayerOptionInfo(PlayerOptionTypes ePlayerOptionNum) const
{
	FASSERT_BOUNDS(0, NUM_PLAYEROPTION_TYPES, ePlayerOptionNum);
	return infoArrayAt(m_paPlayerOptionInfos, ePlayerOptionNum, "m_paPlayerOptionInfos");
}

CvGraphicOptionInfo& cvInternalGlobals::getGraphicOptionInfo(GraphicOptionTypes eGraphicOptionNum) const
{
	FASSERT_BOUNDS(0, NUM_GRAPHICOPTION_TYPES, eGraphicOptionNum);
	return infoArrayAt(m_paGraphicOptionInfos, eGraphicOptionNum, "m_paGraphicOptionInfos");
}

CvYieldInfo& cvInternalGlobals::getYieldInfo(YieldTypes eYieldNum) const
{
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, eYieldNum);
	return infoArrayAt(m_paYieldInfo, eYieldNum, "m_paYieldInfo");
}

CvCommerceInfo& cvInternalGlobals::getCommerceInfo(CommerceTypes eCommerceNum) const
{
	FASSERT_BOUNDS(0, NUM_COMMERCE_TYPES, eCommerceNum);
	return infoArrayAt(m_paCommerceInfo, eCommerceNum, "m_paCommerceInfo");
}

int cvInternalGlobals::getNumRouteInfos() const
{
	return (int)m_paRouteInfo.size();
}

CvRouteInfo& cvInternalGlobals::getRouteInfo(RouteTypes eRouteNum) const
{
	FASSERT_BOUNDS(0, GC.getNumRouteInfos(), eRouteNum);
	// #430: the JSON poco from the per-type InfoRepo (the payload IS a CvRouteInfo); the XML m_paRouteInfo
	// load is demolition fodder cut at the atomic cutover (cascade-engine-430.md §3).
	return *static_cast<CvRouteInfo*>(InfoRepo<CvRouteInfo>::get().atPtr(eRouteNum, "CvRouteInfo"));
}

int cvInternalGlobals::getNumImprovementInfos() const
{
	return (int)m_paImprovementInfo.size();
}

CvImprovementInfo& cvInternalGlobals::getImprovementInfo(ImprovementTypes eImprovementNum) const
{
	FASSERT_BOUNDS(0, GC.getNumImprovementInfos(), eImprovementNum);
	return *static_cast<CvImprovementInfo*>(InfoRepo<CvImprovementInfo>::get().atPtr(eImprovementNum, "CvImprovementInfo"));   // #430: JSON shim leaf (see getTerrainInfo)
}

int cvInternalGlobals::getNumGoodyInfos() const
{
	return (int)m_paGoodyInfo.size();
}

CvGoodyInfo& cvInternalGlobals::getGoodyInfo(GoodyTypes eGoodyNum) const
{
	FASSERT_BOUNDS(0, GC.getNumGoodyInfos(), eGoodyNum);
	return infoArrayAt(m_paGoodyInfo, eGoodyNum, "m_paGoodyInfo");
}

int cvInternalGlobals::getNumBuildInfos() const
{
	return m_buildTable.getNum();
}

CvBuildInfo& cvInternalGlobals::getBuildInfo(BuildTypes eBuildNum) const
{
	FASSERT_BOUNDS(0, GC.getNumBuildInfos(), eBuildNum);
	return *static_cast<CvBuildInfo*>(InfoRepo<CvBuildInfo>::get().atPtr(eBuildNum, "CvBuildInfo"));   // #430: JSON shim leaf (see getTerrainInfo); m_buildTable is demolition fodder
}

void cvInternalGlobals::linkAllInfos()
{
	PROFILE_EXTRA_FUNC();
	// Uniform parse-then-link over every registered info vector. CvInfoUtil's virtual getDataMembers
	// dispatches to each row's concrete declarative fields; rows with no declared FKs (or only
	// already-resolved immediate ones) are a no-op. Coverage grows as classes are migrated.
	for (uint32_t v = 0, nv = m_aInfoVectors.size(); v < nv; v++)
	{
		std::vector<CvInfoBase*>& vec = *m_aInfoVectors[v];
		for (uint32_t i = 0, n = vec.size(); i < n; i++)
		{
			CvInfoUtil(vec[i]).link();
		}
	}
}

int cvInternalGlobals::getNumHandicapInfos() const
{
	return (int)m_paHandicapInfo.size();
}

CvHandicapInfo& cvInternalGlobals::getHandicapInfo(HandicapTypes eHandicapNum) const
{
	FASSERT_BOUNDS(0, GC.getNumHandicapInfos(), eHandicapNum);
	return infoArrayAt(m_paHandicapInfo, eHandicapNum, "m_paHandicapInfo");
}

int cvInternalGlobals::getNumGameSpeedInfos() const
{
	return (int)m_paGameSpeedInfo.size();
}

CvGameSpeedInfo& cvInternalGlobals::getGameSpeedInfo(GameSpeedTypes eGameSpeedNum) const
{
	FASSERT_BOUNDS(0, GC.getNumGameSpeedInfos(), eGameSpeedNum);
	return infoArrayAt(m_paGameSpeedInfo, eGameSpeedNum, "m_paGameSpeedInfo");
}

int cvInternalGlobals::getNumTurnTimerInfos() const
{
	return (int)m_paTurnTimerInfo.size();
}

CvTurnTimerInfo& cvInternalGlobals::getTurnTimerInfo(TurnTimerTypes eTurnTimerNum) const
{
	FASSERT_BOUNDS(0, GC.getNumTurnTimerInfos(), eTurnTimerNum);
	return infoArrayAt(m_paTurnTimerInfo, eTurnTimerNum, "m_paTurnTimerInfo");
}

int cvInternalGlobals::getNumProcessInfos() const
{
	return (int)m_paProcessInfo.size();
}

CvProcessInfo& cvInternalGlobals::getProcessInfo(ProcessTypes e) const
{
	FASSERT_BOUNDS(0, GC.getNumProcessInfos(), e);
	return *static_cast<CvProcessInfo*>(InfoRepo<CvProcessInfo>::get().atPtr(e, "CvProcessInfo"));
}

int cvInternalGlobals::getNumVoteInfos() const
{
	return (int)m_paVoteInfo.size();
}

CvVoteInfo& cvInternalGlobals::getVoteInfo(VoteTypes e) const
{
	FASSERT_BOUNDS(0, GC.getNumVoteInfos(), e);
	return infoArrayAt(m_paVoteInfo, e, "m_paVoteInfo");
}

int cvInternalGlobals::getNumProjectInfos() const
{
	return (int)m_paProjectInfo.size();
}

CvProjectInfo& cvInternalGlobals::getProjectInfo(ProjectTypes e) const
{
	FASSERT_BOUNDS(0, GC.getNumProjectInfos(), e);
	return *static_cast<CvProjectInfo*>(InfoRepo<CvProjectInfo>::get().atPtr(e, "CvProjectInfo"));
}

int cvInternalGlobals::getNumBuildingInfos() const
{
	return (int)m_paBuildingInfo.size();
}

CvBuildingInfo& cvInternalGlobals::getBuildingInfo(BuildingTypes eBuildingNum) const
{
	FASSERT_BOUNDS(0, GC.getNumBuildingInfos(), eBuildingNum);
	return *static_cast<CvBuildingInfo*>(InfoRepo<CvBuildingInfo>::get().atPtr(eBuildingNum, "CvBuildingInfo"));
}

int cvInternalGlobals::getNumSpecialBuildingInfos() const
{
	return (int)m_paSpecialBuildingInfo.size();
}

CvSpecialBuildingInfo& cvInternalGlobals::getSpecialBuildingInfo(SpecialBuildingTypes eSpecialBuildingNum) const
{
	FASSERT_BOUNDS(0, GC.getNumSpecialBuildingInfos(), eSpecialBuildingNum);
	return infoArrayAt(m_paSpecialBuildingInfo, eSpecialBuildingNum, "m_paSpecialBuildingInfo");
}

int cvInternalGlobals::getNumActionInfos() const
{
	return (int)m_paActionInfo.size();
}

CvActionInfo& cvInternalGlobals::getActionInfo(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumActionInfos(), i);
	return infoArrayAt(m_paActionInfo, i, "m_paActionInfo");
}

CvMissionInfo& cvInternalGlobals::getMissionInfo(MissionTypes eMissionNum) const
{
	FASSERT_BOUNDS(0, GC.getNumMissionInfos(), eMissionNum);
	return infoArrayAt(m_paMissionInfo, eMissionNum, "m_paMissionInfo");
}

CvControlInfo& cvInternalGlobals::getControlInfo(ControlTypes eControlNum) const
{
	FASSERT_BOUNDS(0, NUM_CONTROL_TYPES, eControlNum);
	FAssert(!m_paControlInfo.empty());
	return infoArrayAt(m_paControlInfo, eControlNum, "m_paControlInfo");
}

CvCommandInfo& cvInternalGlobals::getCommandInfo(CommandTypes eCommandNum) const
{
	FASSERT_BOUNDS(0, NUM_COMMAND_TYPES, eCommandNum);
	return infoArrayAt(m_paCommandInfo, eCommandNum, "m_paCommandInfo");
}

int cvInternalGlobals::getNumAutomateInfos() const
{
	return (int)m_paAutomateInfo.size();
}

CvAutomateInfo& cvInternalGlobals::getAutomateInfo(int iAutomateNum) const
{
	FASSERT_BOUNDS(0, getNumAutomateInfos(), iAutomateNum);
	return infoArrayAt(m_paAutomateInfo, iAutomateNum, "m_paAutomateInfo");
}

int cvInternalGlobals::getNumPromotionInfos() const
{
	return (int)m_paPromotionInfo.size();
}

CvPromotionInfo& cvInternalGlobals::getPromotionInfo(PromotionTypes ePromotionNum) const
{
	FASSERT_BOUNDS(0, GC.getNumPromotionInfos(), ePromotionNum);
	return *static_cast<CvPromotionInfo*>(InfoRepo<CvPromotionInfo>::get().atPtr(ePromotionNum, "CvPromotionInfo"));
}

PromotionTypes cvInternalGlobals::findPromotion(PromotionPredicateFn predicateFn) const
{
	PROFILE_EXTRA_FUNC();
	for (int idx = 0; idx < static_cast<int>(m_paPromotionInfo.size()); ++idx)
	{
		if (predicateFn(m_paPromotionInfo[idx], static_cast<PromotionTypes>(idx)))
		{
			return static_cast<PromotionTypes>(idx);
		}
	}
	return static_cast<PromotionTypes>(-1);
}

int cvInternalGlobals::getNumTechInfos() const
{
	return (int)m_paTechInfo.size();
}

CvTechInfo& cvInternalGlobals::getTechInfo(TechTypes eTechNum) const
{
	FASSERT_BOUNDS(0, GC.getNumTechInfos(), eTechNum);
	return *static_cast<CvTechInfo*>(InfoRepo<CvTechInfo>::get().atPtr(eTechNum, "CvTechInfo"));
}

int cvInternalGlobals::getNumReligionInfos() const
{
	return (int)m_paReligionInfo.size();
}

CvReligionInfo& cvInternalGlobals::getReligionInfo(ReligionTypes eReligionNum) const
{
	FASSERT_BOUNDS(0, GC.getNumReligionInfos(), eReligionNum);
	return *static_cast<CvReligionInfo*>(InfoRepo<CvReligionInfo>::get().atPtr(eReligionNum, "CvReligionInfo"));
}

int cvInternalGlobals::getNumCorporationInfos() const
{
	return (int)m_paCorporationInfo.size();
}

CvCorporationInfo& cvInternalGlobals::getCorporationInfo(CorporationTypes eCorporationNum) const
{
	FASSERT_BOUNDS(0, GC.getNumCorporationInfos(), eCorporationNum);
	return *static_cast<CvCorporationInfo*>(InfoRepo<CvCorporationInfo>::get().atPtr(eCorporationNum, "CvCorporationInfo"));
}

int cvInternalGlobals::getNumSpecialistInfos() const
{
	return (int)m_paSpecialistInfo.size();
}

CvSpecialistInfo& cvInternalGlobals::getSpecialistInfo(SpecialistTypes eSpecialistNum) const
{
	FASSERT_BOUNDS(0, GC.getNumSpecialistInfos(), eSpecialistNum);
	return *static_cast<CvSpecialistInfo*>(InfoRepo<CvSpecialistInfo>::get().atPtr(eSpecialistNum, "CvSpecialistInfo"));
}

int cvInternalGlobals::getNumCivicOptionInfos() const
{
	return (int)m_paCivicOptionInfo.size();
}

CvCivicOptionInfo& cvInternalGlobals::getCivicOptionInfo(CivicOptionTypes eCivicOptionNum) const
{
	FASSERT_BOUNDS(0, GC.getNumCivicOptionInfos(), eCivicOptionNum);
	return *static_cast<CvCivicOptionInfo*>(InfoRepo<CvCivicOptionInfo>::get().atPtr(eCivicOptionNum, "CvCivicOptionInfo"));
}

int cvInternalGlobals::getNumCivicInfos() const
{
	return (int)m_paCivicInfo.size();
}

CvCivicInfo& cvInternalGlobals::getCivicInfo(CivicTypes eCivicNum) const
{
	FASSERT_BOUNDS(0, GC.getNumCivicInfos(), eCivicNum);
	return *static_cast<CvCivicInfo*>(InfoRepo<CvCivicInfo>::get().atPtr(eCivicNum, "CvCivicInfo"));
}

int cvInternalGlobals::getNumDiplomacyInfos() const
{
	return (int)m_paDiplomacyInfo.size();
}

CvDiplomacyInfo& cvInternalGlobals::getDiplomacyInfo(int iDiplomacyNum) const
{
	FASSERT_BOUNDS(0, GC.getNumDiplomacyInfos(), iDiplomacyNum);
	return infoArrayAt(m_paDiplomacyInfo, iDiplomacyNum, "m_paDiplomacyInfo");
}

int cvInternalGlobals::getNumEraInfos() const
{
	return (int)m_aEraInfo.size();
}

CvEraInfo& cvInternalGlobals::getEraInfo(EraTypes eEraNum) const
{
	FASSERT_BOUNDS(0, GC.getNumEraInfos(), eEraNum);
	return infoArrayAt(m_aEraInfo, eEraNum, "m_aEraInfo");
}

int cvInternalGlobals::getNumHurryInfos() const
{
	return (int)m_paHurryInfo.size();
}

CvHurryInfo& cvInternalGlobals::getHurryInfo(HurryTypes eHurryNum) const
{
	FASSERT_BOUNDS(0, GC.getNumHurryInfos(), eHurryNum);
	return infoArrayAt(m_paHurryInfo, eHurryNum, "m_paHurryInfo");
}

int cvInternalGlobals::getNumEmphasizeInfos() const
{
	return (int)m_paEmphasizeInfo.size();
}

CvEmphasizeInfo& cvInternalGlobals::getEmphasizeInfo(EmphasizeTypes eEmphasizeNum) const
{
	FASSERT_BOUNDS(0, GC.getNumEmphasizeInfos(), eEmphasizeNum);
	return infoArrayAt(m_paEmphasizeInfo, eEmphasizeNum, "m_paEmphasizeInfo");
}

int cvInternalGlobals::getNumUpkeepInfos() const
{
	return (int)m_paUpkeepInfo.size();
}

CvUpkeepInfo& cvInternalGlobals::getUpkeepInfo(UpkeepTypes eUpkeepNum) const
{
	FASSERT_BOUNDS(0, GC.getNumUpkeepInfos(), eUpkeepNum);
	return infoArrayAt(m_paUpkeepInfo, eUpkeepNum, "m_paUpkeepInfo");
}

int cvInternalGlobals::getNumCultureLevelInfos() const
{
	return (int)m_paCultureLevelInfo.size();
}

CvCultureLevelInfo& cvInternalGlobals::getCultureLevelInfo(CultureLevelTypes eCultureLevelNum) const
{
	FASSERT_BOUNDS(0, GC.getNumCultureLevelInfos(), eCultureLevelNum);
	return *static_cast<CvCultureLevelInfo*>(InfoRepo<CvCultureLevelInfo>::get().atPtr(eCultureLevelNum, "CvCultureLevelInfo"));
}

int cvInternalGlobals::getNumVictoryInfos() const
{
	return (int)m_paVictoryInfo.size();
}

CvVictoryInfo& cvInternalGlobals::getVictoryInfo(VictoryTypes eVictoryNum) const
{
	FASSERT_BOUNDS(0, GC.getNumVictoryInfos(), eVictoryNum);
	return infoArrayAt(m_paVictoryInfo, eVictoryNum, "m_paVictoryInfo");
}

int cvInternalGlobals::getNumEventTriggerInfos() const
{
	return (int)m_paEventTriggerInfo.size();
}

CvEventTriggerInfo& cvInternalGlobals::getEventTriggerInfo(EventTriggerTypes eEventTrigger) const
{
	FASSERT_BOUNDS(0, GC.getNumEventTriggerInfos(), eEventTrigger);
	return infoArrayAt(m_paEventTriggerInfo, eEventTrigger, "m_paEventTriggerInfo");
}

int cvInternalGlobals::getNumEventInfos() const
{
	return (int)m_paEventInfo.size();
}

CvEventInfo& cvInternalGlobals::getEventInfo(EventTypes eEvent) const
{
	FASSERT_BOUNDS(0, GC.getNumEventInfos(), eEvent);
	return infoArrayAt(m_paEventInfo, eEvent, "m_paEventInfo");
}

int cvInternalGlobals::getNumEspionageMissionInfos() const
{
	return (int)m_paEspionageMissionInfo.size();
}

CvEspionageMissionInfo& cvInternalGlobals::getEspionageMissionInfo(EspionageMissionTypes eEspionageMissionNum) const
{
	FASSERT_BOUNDS(0, GC.getNumEspionageMissionInfos(), eEspionageMissionNum);
	return infoArrayAt(m_paEspionageMissionInfo, eEspionageMissionNum, "m_paEspionageMissionInfo");
}

int& cvInternalGlobals::getNumAnimationOperatorTypes()
{
	return m_iNumAnimationOperatorTypes;
}

CvString*& cvInternalGlobals::getAnimationOperatorTypes()
{
	return m_paszAnimationOperatorTypes;
}

CvString& cvInternalGlobals::getAnimationOperatorTypes(AnimationOperatorTypes e)
{
	FASSERT_BOUNDS(0, GC.getNumAnimationOperatorTypes(), e);
	return m_paszAnimationOperatorTypes[e];
}

CvString*& cvInternalGlobals::getFunctionTypes()
{
	return m_paszFunctionTypes;
}

CvString& cvInternalGlobals::getFunctionTypes(FunctionTypes e)
{
	FASSERT_BOUNDS(0, NUM_FUNC_TYPES, e);
	return m_paszFunctionTypes[e];
}

int& cvInternalGlobals::getNumFlavorTypes()
{
	return m_iNumFlavorTypes;
}

CvString*& cvInternalGlobals::getFlavorTypes()
{
	return m_paszFlavorTypes;
}

CvString& cvInternalGlobals::getFlavorTypes(FlavorTypes e)
{
	FASSERT_BOUNDS(0, GC.getNumFlavorTypes(), e);
	return m_paszFlavorTypes[e];
}

int& cvInternalGlobals::getNumArtStyleTypes()
{
	return m_iNumArtStyleTypes;
}

CvString*& cvInternalGlobals::getArtStyleTypes()
{
	return m_paszArtStyleTypes;
}

CvString& cvInternalGlobals::getArtStyleTypes(ArtStyleTypes e)
{
	FASSERT_BOUNDS(0, GC.getNumArtStyleTypes(), e);
	return m_paszArtStyleTypes[e];
}

int cvInternalGlobals::getNumUnitArtStyleTypeInfos() const
{
	return (int)m_paUnitArtStyleTypeInfo.size();
}

CvUnitArtStyleTypeInfo& cvInternalGlobals::getUnitArtStyleTypeInfo(UnitArtStyleTypes eUnitArtStyleTypeNum) const
{
	FASSERT_BOUNDS(0, GC.getNumUnitArtStyleTypeInfos(), eUnitArtStyleTypeNum);
	return infoArrayAt(m_paUnitArtStyleTypeInfo, eUnitArtStyleTypeNum, "m_paUnitArtStyleTypeInfo");
}

int& cvInternalGlobals::getNumCitySizeTypes()
{
	return m_iNumCitySizeTypes;
}

CvString*& cvInternalGlobals::getCitySizeTypes()
{
	return m_paszCitySizeTypes;
}

CvString& cvInternalGlobals::getCitySizeTypes(int i)
{
	FASSERT_BOUNDS(0, GC.getNumCitySizeTypes(), i);
	return m_paszCitySizeTypes[i];
}

CvString*& cvInternalGlobals::getContactTypes()
{
	return m_paszContactTypes;
}

CvString& cvInternalGlobals::getContactTypes(ContactTypes e)
{
	FASSERT_BOUNDS(0, NUM_CONTACT_TYPES, e);
	return m_paszContactTypes[e];
}

CvString*& cvInternalGlobals::getDiplomacyPowerTypes()
{
	return m_paszDiplomacyPowerTypes;
}

CvString& cvInternalGlobals::getDiplomacyPowerTypes(DiplomacyPowerTypes e)
{
	FASSERT_BOUNDS(0, NUM_DIPLOMACYPOWER_TYPES, e);
	return m_paszDiplomacyPowerTypes[e];
}

CvString*& cvInternalGlobals::getAutomateTypes()
{
	return m_paszAutomateTypes;
}

CvString& cvInternalGlobals::getAutomateTypes(AutomateTypes e)
{
	FASSERT_BOUNDS(0, NUM_AUTOMATE_TYPES, e);
	return m_paszAutomateTypes[e];
}

CvString*& cvInternalGlobals::getDirectionTypes()
{
	return m_paszDirectionTypes;
}

CvString& cvInternalGlobals::getDirectionTypes(AutomateTypes e)
{
	FASSERT_BOUNDS(0, NUM_AUTOMATE_TYPES, e);
	return m_paszDirectionTypes[e];
}

int cvInternalGlobals::getNumPropertyInfos() const
{
	return (int)m_paPropertyInfo.size();
}

CvPropertyInfo& cvInternalGlobals::getPropertyInfo(PropertyTypes ePropertyNum) const
{
	FASSERT_BOUNDS(0, GC.getNumPropertyInfos(), ePropertyNum);
	return *static_cast<CvPropertyInfo*>(InfoRepo<CvPropertyInfo>::get().atPtr(ePropertyNum, "CvPropertyInfo"));
}

int cvInternalGlobals::getNumOutcomeInfos() const
{
	return (int)m_paOutcomeInfo.size();
}

CvOutcomeInfo& cvInternalGlobals::getOutcomeInfo(OutcomeTypes eOutcomeNum) const
{
	FASSERT_BOUNDS(0, GC.getNumOutcomeInfos(), eOutcomeNum);
	return infoArrayAt(m_paOutcomeInfo, eOutcomeNum, "m_paOutcomeInfo");
}

int& cvInternalGlobals::getNumFootstepAudioTypes()
{
	return m_iNumFootstepAudioTypes;
}

CvString*& cvInternalGlobals::getFootstepAudioTypes()
{
	return m_paszFootstepAudioTypes;
}

CvString& cvInternalGlobals::getFootstepAudioTypes(int i)
{
	FASSERT_BOUNDS(0, GC.getNumFootstepAudioTypes(), i);
	return m_paszFootstepAudioTypes[i];
}

int cvInternalGlobals::getFootstepAudioTypeByTag(const CvString strTag) const
{
	PROFILE_EXTRA_FUNC();
	if (strTag.GetLength() > 0)
	{
		for (int i = 0; i < m_iNumFootstepAudioTypes; i++)
		{
			if (strTag.CompareNoCase(m_paszFootstepAudioTypes[i]) == 0)
			{
				return i;
			}
		}
	}
	return -1;
}

CvString*& cvInternalGlobals::getFootstepAudioTags()
{
	return m_paszFootstepAudioTags;
}

CvString& cvInternalGlobals::getFootstepAudioTags(int i) const
{
	static CvString* emptyString = NULL;

	if ( emptyString == NULL )
	{
		emptyString = new CvString("");
	}
	FASSERT_BOUNDS(0, GC.getNumFootstepAudioTypes(), i);
	return m_paszFootstepAudioTags ? m_paszFootstepAudioTags[i] : *emptyString;
}

void cvInternalGlobals::setCurrentXMLFile(const char* szFileName)
{
	m_szCurrentXMLFile = szFileName;
}

const CvString& cvInternalGlobals::getCurrentXMLFile() const
{
	return m_szCurrentXMLFile;
}

FVariableSystem* cvInternalGlobals::getDefinesVarSystem() const
{
	return m_VarSystem;
}

void cvInternalGlobals::cacheEnumGlobals()
{
#define CACHE_ENUM_GLOBAL_DEFINE(dataType, VAR) \
	m_##VAR = getDefineINT(#VAR);
	DO_FOR_EACH_ENUM_GLOBAL_DEFINE(CACHE_ENUM_GLOBAL_DEFINE)
}

void cvInternalGlobals::cacheGlobals()
{
#ifdef _DEBUG
	OutputDebugString("Caching Globals: Start\n");
#endif
	strcpy(gVersionString, getDefineSTRING("C2C_VERSION"));

#define CACHE_INT_GLOBAL_DEFINE(dataType, VAR) \
	m_##VAR = getDefineINT(#VAR);
	DO_FOR_EACH_INT_GLOBAL_DEFINE(CACHE_INT_GLOBAL_DEFINE)
	DO_FOR_EACH_BOOL_GLOBAL_DEFINE(CACHE_INT_GLOBAL_DEFINE)

#define CACHE_FLOAT_GLOBAL_DEFINE(dataType, VAR) \
	m_##VAR = getDefineFLOAT(#VAR);
	DO_FOR_EACH_FLOAT_GLOBAL_DEFINE(CACHE_FLOAT_GLOBAL_DEFINE)

	m_fPLOT_SIZE = getDefineFLOAT("PLOT_SIZE");

	m_bBBAI_AIR_COMBAT = !(getDefineINT("BBAI_AIR_COMBAT") == 0);
	m_bBBAI_HUMAN_VASSAL_WAR_BUILD = !(getDefineINT("BBAI_HUMAN_VASSAL_WAR_BUILD") == 0);

	m_bTECH_DIFFUSION_ENABLE = !(getDefineINT("TECH_DIFFUSION_ENABLE") == 0);

	m_szAlternateProfilSampleName = getDefineSTRING("PROFILER_ALTERNATE_SAMPLE_SET_SOURCE");
	if (m_szAlternateProfilSampleName == NULL)
	{
		m_szAlternateProfilSampleName = "";
	}
#ifdef _DEBUG
	OutputDebugString("Caching Globals: End\n");
#endif
}


bool cvInternalGlobals::getDefineBOOL(const char* szName, bool bDefault) const
{
	const bool success = m_VarSystem->GetValue(szName, bDefault);
	//FAssertMsg(success, szName);
	return bDefault;
}

int cvInternalGlobals::getDefineINT(const char* szName, int iDefault) const
{
	const bool success = m_VarSystem->GetValue(szName, iDefault);
	//FAssertMsg(success, szName);
	return iDefault;
}

float cvInternalGlobals::getDefineFLOAT(const char* szName, float fDefault) const
{
	const bool success = m_VarSystem->GetValue(szName, fDefault);
	//FAssertMsg(success, szName);
	return fDefault;
}

const char* cvInternalGlobals::getDefineSTRING(const char* szName, const char* szDefault) const
{
	const bool success = m_VarSystem->GetValue(szName, szDefault);
	//FAssertMsg(success, szName);
	return szDefault;
}

// ⚠ The three setters announce ONLY on the genuine LOCAL set (the `else` branch). The `bUpdate` path sends a net
// message instead of writing, and CvGlobalDefineUpdate::Execute calls straight back in with bUpdate=false -- so
// announcing on both paths would double-emit one change on the initiating machine (the spine's ONE bar is
// duplicates). The emit follows cacheGlobals() so a consumer reading a cached accessor sees the NEW value.
void cvInternalGlobals::setDefineINT(const char* szName, int iValue, bool bUpdate)
{
	if (getDefineINT(szName) != iValue)
	{
		if (bUpdate)
		{
			CvMessageControl::getInstance().sendGlobalDefineUpdate(szName, iValue, -1.0f, "");
		}
		else m_VarSystem->SetValue(szName, iValue);

		cacheEnumGlobals();
		cacheGlobals();

		if (!bUpdate)
		{
			emitGameGlobalDefineAdded(szName, GLOBALDEFINE_INT, iValue, 0.0f, NULL);
		}
	}
}

void cvInternalGlobals::setDefineFLOAT(const char* szName, float fValue, bool bUpdate)
{
	if (getDefineFLOAT(szName) != fValue)
	{
		if (bUpdate)
		{
			CvMessageControl::getInstance().sendGlobalDefineUpdate(szName, -1, fValue, "");
		}
		else m_VarSystem->SetValue(szName, fValue);

		cacheGlobals();

		if (!bUpdate)
		{
			emitGameGlobalDefineAdded(szName, GLOBALDEFINE_FLOAT, 0, fValue, NULL);
		}
	}
}

void cvInternalGlobals::setDefineSTRING(const char* szName, const char* szValue, bool bUpdate)
{
	if (getDefineSTRING(szName) != szValue)
	{
		if (bUpdate)
		{
			CvMessageControl::getInstance().sendGlobalDefineUpdate(szName, -1, -1.0f, szValue);
		}
		else m_VarSystem->SetValue(szName, szValue);

		cacheGlobals(); // TO DO : we should not cache all globals at each single set

		if (!bUpdate)
		{
			emitGameGlobalDefineAdded(szName, GLOBALDEFINE_STRING, 0, 0.0f, szValue);
		}
	}
}


float cvInternalGlobals::getPLOT_SIZE() const
{
	return m_fPLOT_SIZE;
}

void cvInternalGlobals::setDLLProfiler(FProfiler* prof)
{
	m_Profiler = prof;
}

FProfiler* cvInternalGlobals::getDLLProfiler() const
{
	return m_Profiler;
}

void cvInternalGlobals::enableDLLProfiler(bool bEnable)
{
	m_bDLLProfiler = bEnable;

#ifdef USE_INTERNAL_PROFILER
	if (bEnable)
	{
		g_bTraceBackgroundThreads = getDefineBOOL("ENABLE_BACKGROUND_PROFILING");
	}
#endif
}

bool cvInternalGlobals::isDLLProfilerEnabled() const
{
	return m_bDLLProfiler;
}

const char* cvInternalGlobals::alternateProfileSampleName() const
{
	return m_szAlternateProfilSampleName;
}

void cvInternalGlobals::deleteInfoArrays()
{
	algo::for_each(m_aInfoVectors, bind(deleteInfoArray, _1));

	m_paModLoadControlVector.clear();

	SAFE_DELETE_ARRAY(GC.getAnimationOperatorTypes());
	SAFE_DELETE_ARRAY(GC.getFunctionTypes());
	SAFE_DELETE_ARRAY(GC.getFlavorTypes());
	SAFE_DELETE_ARRAY(GC.getArtStyleTypes());
	SAFE_DELETE_ARRAY(GC.getCitySizeTypes());
	SAFE_DELETE_ARRAY(GC.getContactTypes());
	SAFE_DELETE_ARRAY(GC.getDiplomacyPowerTypes());
	SAFE_DELETE_ARRAY(GC.getAutomateTypes());
	SAFE_DELETE_ARRAY(GC.getDirectionTypes());
	SAFE_DELETE_ARRAY(GC.getFootstepAudioTypes());
	SAFE_DELETE_ARRAY(GC.getFootstepAudioTags());

	clearTypesMap();
	m_aInfoVectors.clear();
}

//
// Global Infos Hash Map
//
int cvInternalGlobals::getInfoTypeForString(const char* szType, bool hideAssert) const
{
	FAssertMsg(szType, "null info type string");

	InfosMap::const_iterator it = m_infosMap.find(szType);

	if (it != m_infosMap.end())
	{
		return it->second;
	}
	if (stricmp(szType, "NONE") != 0 && strcmp(szType, "") != 0 && !hideAssert && !getDefineINT(szType))
	{
		CvString szError;
		szError.Format("info type '%s' not found, Current XML file is: %s", szType, GC.getCurrentXMLFile().GetCString());
		FAssertMsg(stricmp(szType, "NONE") == 0 || strcmp(szType, "") == 0, szError.c_str());

		logging::logMsg("Xml_MissingTypes.log", szError.c_str());
	}
	return -1;
}

/************************************************************************************************/
/* SORT_ALPHABET                           11/19/07                                MRGENIE      */
/*                                                                                              */
/* Rearranging the infos map                                                                    */
/************************************************************************************************/
void cvInternalGlobals::setInfoTypeFromString(const char* szType, int idx)
{
	FAssertMsg(szType, "null info type string");
#ifdef _DEBUG
	OutputDebugString(CvString::format("%s -> %d\n", szType, idx).c_str());
#endif
	char* strCpy = new char[strlen(szType)+1];

	m_infosMap[strcpy(strCpy, szType)] = idx;
}

// returns the ID if it exists, otherwise assigns a new ID
int cvInternalGlobals::getOrCreateInfoTypeForString(const char* szType)
{
	int iID = getInfoTypeForString(szType, true);
	if (iID < 0)
	{
		m_iLastTypeID++;
		iID = m_iLastTypeID;
		setInfoTypeFromString(szType, iID);
	}
	return iID;
}

void cvInternalGlobals::logInfoTypeMap(const char* tagMsg)
{
	PROFILE_EXTRA_FUNC();
	logging::logMsg("cvInternalGlobals_logInfoTypeMap.log", " === Info Type Map Dump BEGIN: %s ===", tagMsg);

	int iCnt = 0;
	std::vector<std::string> vInfoMapKeys;
	for (InfosMap::const_iterator it = m_infosMap.begin(); it != m_infosMap.end(); ++it)
	{
		std::string sKey = it->first;
		vInfoMapKeys.push_back(sKey);
	}

	algo::sort(vInfoMapKeys);

	foreach_(const std::string& sKey, vInfoMapKeys)
	{
		logging::logMsg("cvInternalGlobals_logInfoTypeMap.log", " * %i --  %s: %i", iCnt, sKey.c_str(), m_infosMap[sKey.c_str()]);
		iCnt++;
	}

	logging::logMsg("cvInternalGlobals_logInfoTypeMap.log", "Entries in total: %i", iCnt);
	logging::logMsg("cvInternalGlobals_logInfoTypeMap.log", " === Info Type Map Dump END: %s ===", tagMsg);
}
/************************************************************************************************/
/* SORT_ALPHABET                           END                                                  */
/************************************************************************************************/

void cvInternalGlobals::infoTypeFromStringReset()
{
	PROFILE_EXTRA_FUNC();
	for (InfosMap::const_iterator it = m_infosMap.begin(); it != m_infosMap.end(); ++it)
	{
		delete[] it->first;
	}

	m_infosMap.clear();
}

void cvInternalGlobals::addToInfosVectors(void* infoVector, InfoClassTypes eInfoClass)
{
	m_aInfoVectors.push_back(static_cast<std::vector<CvInfoBase*>*>(infoVector));

	if (eInfoClass > NO_INFO_CLASS)
	{
		static uint16_t numClassesLoaded = 0;
		m_infoClassXmlLoadOrder[eInfoClass] = ++numClassesLoaded;
	}
}

void cvInternalGlobals::infosReset()
{
	PROFILE_EXTRA_FUNC();
	foreach_(const std::vector<CvInfoBase*>* infoVector, m_aInfoVectors)
	{
		foreach_(CvInfoBase* info, *infoVector)
			info->reset();
	}
}

void cvInternalGlobals::cacheInfoTypes()
{
#define CACHE_INFO_TYPE(type, VAR) \
	m_##VAR = (type)getInfoTypeForString(#VAR);

	DO_FOR_EACH_INFO_TYPE(CACHE_INFO_TYPE)
}

/*********************************/
/***** Parallel Maps - Begin *****/
/*********************************/

void cvInternalGlobals::switchMap(MapTypes eMap)
{
	FASSERT_BOUNDS(0, NUM_MAPS, eMap);

	// Multi-map is soft-disabled pending the single-map collapse (see
	// docs/dev/plans/multimap-zone-rework.md): never initialise / page in a map that
	// isn't already live. New games only have MAP_EARTH loaded, so no other map is
	// reachable; an existing save's already-loaded maps stay switchable so it can't be
	// trapped. The single-map collapse + zone paging replaces this wholesale.
	if (eMap != CURRENT_MAP && !getMapByIndex(eMap).plotsInitialized())
	{
		return;
	}

	if (eMap != CURRENT_MAP)
	{
		getMap().beforeSwitch();
		getGame().setCurrentMap(eMap);
		// The CyMap wrapper is no longer synced here: the global context hands out no Cy* HANDLES any more, so
		// there is nothing on the far side to keep in step. The map switch itself is untouched.
		getMap().afterSwitch();
	}
}

CvViewport* cvInternalGlobals::getCurrentViewport() const
{
	return m_maps[CURRENT_MAP]->getCurrentViewport();
}

int	cvInternalGlobals::getViewportCenteringBorder() const
{
	return m_iViewportCenterOnSelectionCenterBorder;
}


CvMapExternal& cvInternalGlobals::getMapExternal() const
{
	const CvViewport* currentViewport = getCurrentViewport();

	FAssert(currentViewport != NULL);

	return currentViewport->getProxy();
}

CvMap& cvInternalGlobals::getMapByIndex(MapTypes eIndex) const
{
	FASSERT_BOUNDS(0, NUM_MAPS, eIndex);
	return *m_maps[eIndex];
}

void cvInternalGlobals::clearSigns()
{
	gDLL->getEngineIFace()->clearSigns();

	m_bSignsCleared = true;
}

void cvInternalGlobals::reprocessSigns()
{
	if (m_bSignsCleared)
	{
		Cy::call(PYCivModule, "AddSign", Cy::Args() << (CvPlot*)NULL << NO_PLAYER << "");
		m_bSignsCleared = false;
	}
}
/*******************************/
/***** Parallel Maps - End *****/
/*******************************/

bool cvInternalGlobals::isDelayedResolutionRequired(InfoClassTypes eLoadingClass, InfoClassTypes eRefClass) const
{
	// Non-info-class callers (NO_INFO_CLASS enums, link-time CvInfoUtil reconstruction over
	// CvInfoBase*) have no load-order entry — indexing the array with -1 is out of bounds.
	// Immediate is the only resolution the link phase can offer them.
	if (eLoadingClass <= NO_INFO_CLASS || eLoadingClass >= NUM_INFO_CLASSES
	|| eRefClass <= NO_INFO_CLASS || eRefClass >= NUM_INFO_CLASSES)
	{
		return false;
	}
	// SPIKE (parse-then-link): force CvBuildInfo onto the deferred path regardless of load order, so
	// its top-level FK columns resolve in InfoTable<CvBuildInfo>::link() instead of inline at read.
	// This demonstrates that with a link phase the catalog no longer depends on XML load order.
	// (Nested struct-element FKs still resolve immediately — see CvInfoUtil m_bForceImmediate.)
	if (eLoadingClass == BUILD_INFO)
		return true;
	// Load-order stamps are 1-based (set when a category STARTS loading); 0 means the ref class
	// has not begun loading, so none of its types exist yet and resolution MUST be deferred.
	// Without this, an immediate read of a not-yet-loaded ref through IDValueMap::read would
	// phantom-register the type string via getOrCreateInfoTypeForString, and the ref class's
	// loader would later mistake its real definition for a duplicate and overwrite whatever
	// info sits at the phantom index (observed: SPECIALIST_SLAVES' TechHappinessTypes registered
	// TECH_HUMANISM/TECH_EMANCIPATION as ids 0/1, destroying the first two techs).
	if (m_infoClassXmlLoadOrder[eRefClass] == 0)
		return true;
	return m_infoClassXmlLoadOrder[eLoadingClass] <= m_infoClassXmlLoadOrder[eRefClass];
}

void cvInternalGlobals::addDelayedResolution(int *pType, CvString szString)
{
	m_delayedResolutionMap[pType] = std::make_pair(szString,  GC.getCurrentXMLFile());
	//m_delayedResolutionMap.insert(DelayedResolutionMap::value_type(pType, szString));
}

CvString* cvInternalGlobals::getDelayedResolution(int *pType)
{
	DelayedResolutionMap::iterator it = m_delayedResolutionMap.find(pType);
	if (it == m_delayedResolutionMap.end())
	{
		return NULL;
	}
	return &(it->second.first);
}

void cvInternalGlobals::removeDelayedResolution(int *pType)
{
	m_delayedResolutionMap.erase(pType);
}

void cvInternalGlobals::copyNonDefaultDelayedResolution(int* pTypeSelf, int* pTypeOther)
{
	if (getDelayedResolution(pTypeSelf) == NULL)
	{
		CvString* pszOther = getDelayedResolution(pTypeOther);
		if (pszOther != NULL)
		{
			addDelayedResolution(pTypeSelf, *pszOther);
		}
	}
}

void cvInternalGlobals::resolveDelayedResolution()
{
	PROFILE_EXTRA_FUNC();
	for (DelayedResolutionMap::iterator it = m_delayedResolutionMap.begin(); it != m_delayedResolutionMap.end(); ++it)
	{
		GC.setCurrentXMLFile(it->second.second);
		*(it->first) = getInfoTypeForString(it->second.first);
	}
	m_delayedResolutionMap.clear();
}

int cvInternalGlobals::getNumMissionInfos() const
{
	return (int) m_paMissionInfo.size();
}

CvMap& cvInternalGlobals::getMap() const
{
	return *m_maps[CURRENT_MAP];
}

FAStar& cvInternalGlobals::getPathFinder(MapTypes map) const
{
	return *m_pathFinders[map == NO_MAP ? (m_game ? CURRENT_MAP : MAP_EARTH) : map];
}

FAStar& cvInternalGlobals::getInterfacePathFinder(MapTypes map) const
{
	return *m_interfacePathFinders[map == NO_MAP ? (m_game ? CURRENT_MAP : MAP_EARTH) : map];
}

FAStar& cvInternalGlobals::getStepFinder(MapTypes map) const
{
	return *m_stepFinders[map == NO_MAP ? (m_game ? CURRENT_MAP : MAP_EARTH) : map];
}

FAStar& cvInternalGlobals::getRouteFinder(MapTypes map) const
{
	return *m_routeFinders[map == NO_MAP ? (m_game ? CURRENT_MAP : MAP_EARTH) : map];
}

FAStar& cvInternalGlobals::getBorderFinder(MapTypes map) const
{
	return *m_borderFinders[map == NO_MAP ? (m_game ? CURRENT_MAP : MAP_EARTH) : map];
}

FAStar& cvInternalGlobals::getAreaFinder(MapTypes map) const
{
	return *m_areaFinders[map == NO_MAP ? (m_game ? CURRENT_MAP : MAP_EARTH) : map];
}

FAStar& cvInternalGlobals::getPlotGroupFinder(MapTypes map) const
{
	return *m_plotGroupFinders[map == NO_MAP ? (m_game ? CURRENT_MAP : MAP_EARTH) : map];
}

CvGameAI* cvInternalGlobals::getGamePointer() { return m_game; }

bool cvInternalGlobals::IsGraphicsInitialized() const { return m_bGraphicsInitialized; }
void cvInternalGlobals::SetGraphicsInitialized(bool bVal) { m_bGraphicsInitialized = bVal; }
void cvInternalGlobals::setInterface(CvInterface* pVal) { m_interface = pVal; }
void cvInternalGlobals::setDiplomacyScreen(CvDiplomacyScreen* pVal) { m_diplomacyScreen = pVal; }
void cvInternalGlobals::setMPDiplomacyScreen(CMPDiplomacyScreen* pVal) { m_mpDiplomacyScreen = pVal; }
void cvInternalGlobals::setMessageQueue(CMessageQueue* pVal) { m_messageQueue = pVal; }
void cvInternalGlobals::setHotJoinMessageQueue(CMessageQueue* pVal) { m_hotJoinMsgQueue = pVal; }
void cvInternalGlobals::setMessageControl(CMessageControl* pVal) { m_messageControl = pVal; }
void cvInternalGlobals::setSetupData(CvSetupData* pVal) { m_setupData = pVal; }
void cvInternalGlobals::setMessageCodeTranslator(CvMessageCodeTranslator* pVal) { m_messageCodes = pVal; }
void cvInternalGlobals::setDropMgr(CvDropMgr* pVal) { m_dropMgr = pVal; }
void cvInternalGlobals::setPortal(CvPortal* pVal) { m_portal = pVal; }
void cvInternalGlobals::setStatsReport(CvStatsReporter* pVal) { m_statsReporter = pVal; }

void cvInternalGlobals::setPathFinder(FAStar* pVal)
{
	m_pathFinders[MAP_EARTH] = pVal;
}

void cvInternalGlobals::setInterfacePathFinder(FAStar* pVal)
{
	m_interfacePathFinders[MAP_EARTH] = pVal;
}

void cvInternalGlobals::setStepFinder(FAStar* pVal)
{
	m_stepFinders[MAP_EARTH] = pVal;
}

void cvInternalGlobals::setRouteFinder(FAStar* pVal)
{
	m_routeFinders[MAP_EARTH] = pVal;
}

void cvInternalGlobals::setBorderFinder(FAStar* pVal)
{
	m_borderFinders[MAP_EARTH] = pVal;
}

void cvInternalGlobals::setAreaFinder(FAStar* pVal)
{
	m_areaFinders[MAP_EARTH] = pVal;
}

void cvInternalGlobals::setPlotGroupFinder(FAStar* pVal)
{
	m_plotGroupFinders[MAP_EARTH] = pVal;
}

static bool bBugInitCalled = false;

bool cvInternalGlobals::bugInitCalled() const
{
	return bBugInitCalled;
}

// Toffer - Only ever called once, happens the first time one start a new game, or loads a save.
void cvInternalGlobals::setIsBug()
{
	PROFILE_EXTRA_FUNC();
	bBugInitCalled = true;

	::setIsBug();
	refreshOptionsBUG();

	// If viewports are truned on in BUG the settinsg there override those in the global defines
	if (getBugOptionBOOL("MainInterface__EnableViewports", false))
	{
		m_ENABLE_VIEWPORTS = true;

		// Push them back inot the globals so that a reload of the globals cache preserves these values
		setDefineINT("ENABLE_VIEWPORTS", 1, false);
		setDefineINT("VIEWPORT_SIZE_X", getBugOptionINT("MainInterface__ViewportX", 40), false);
		setDefineINT("VIEWPORT_SIZE_Y", getBugOptionINT("MainInterface__ViewportY", 40), false);
		setDefineINT("VIEWPORT_FOCUS_BORDER", getBugOptionINT("MainInterface__ViewportAutoSwitchBorder", 2), false);
		m_iViewportCenterOnSelectionCenterBorder = getBugOptionINT("MainInterface__ViewportAutoCenterBorder", 5);

		// This happens after the maps load on first load, so resize existing viewports
		foreach_(const CvMap* map, m_maps)
		{
			foreach_(CvViewport* viewport, map->getViewports())
			{
				viewport->resizeForMap();
			}
		}
	}
}

void cvInternalGlobals::refreshOptionsBUG()
{
	m_bGraphicalPaging = getBugOptionBOOL("MainInterface__EnableGraphicalPaging", true);
	// One unified AI-log verbosity knob for now: every subsystem log (player/team/city/unit)
	// follows the single Player level. The per-scope Team/City/Unit BUG options are left in the
	// UI but currently ignored; we may re-split these globals later if scope-specific gating is
	// wanted again. Driving them all from the known-good Player option removes any doubt about a
	// given scope's option being mis-wired (e.g. WarAI.log staying empty at gTeamLogLevel).
	const int iAILogLevel = getBugOptionINT("Autolog__LogLevelPlayerBBAI", 0);
	gPlayerLogLevel = iAILogLevel;
	gTeamLogLevel = iAILogLevel;
	gCityLogLevel = iAILogLevel;
	gUnitLogLevel = iAILogLevel;
	gMiscLogging = getBugOptionBOOL("Autolog__MiscLogging", false);

	// Turn-timing has its own knob so wall-clock timing can run independently of the verbose
	// AI logs. Deliberately NOT forced to 4 in _DEBUG -- timing a debug build is meaningless.
	gPerfLogLevel = getBugOptionINT("Autolog__LogLevelPerf", 0);

	// Live log stream over /events (#419): headline lines (level <= this) are teed raw
	// onto the SSE pipe. Default 1 -- inert anyway unless the HTTP server is on.
	gStreamLogLevel = getBugOptionINT("Autolog__LogLevelStream", 1);

	// Dev live-state HTTP endpoint PoC (#387): GET-only hello-world server on
	// 127.0.0.1:7227, own Win32 thread, zero game-state access. Starts/stops live
	// when the option is toggled (this refresh runs on closing the BUG screen).
	// OR'd with the global define: the server may already be up from the MAIN MENU (HTTP_SERVER_FROM_MENU, read at
	// SetGlobalDefines -- long before BUG exists). This refresh runs at first game start and on closing the BUG
	// screen, so a plain assignment here would TEAR DOWN the menu-started server the moment a game loaded, killing
	// the stream mid-load. The BUG option still enables it independently.
	CvHttpServer::setEnabled(getBugOptionBOOL("Autolog__HttpServer", false)
		|| getDefineINT("HTTP_SERVER_FROM_MENU") != 0);

#ifdef _DEBUG
	gPlayerLogLevel = 4;
	gTeamLogLevel = 4;
	gCityLogLevel = 4;
	gUnitLogLevel = 4;
#endif // DEBUG



	OutputRatios::setBaseOutputWeights(
		getBugOptionINT("CityScreen__BaseWeightFood", 10),
		getBugOptionINT("CityScreen__BaseWeightHammer", 8),
		getBugOptionINT("CityScreen__BaseWeightCommerce", 6)
	);
}


bool cvInternalGlobals::getBBAI_AIR_COMBAT() const
{
	return m_bBBAI_AIR_COMBAT;
}

bool cvInternalGlobals::getBBAI_HUMAN_VASSAL_WAR_BUILD() const
{
	return m_bBBAI_HUMAN_VASSAL_WAR_BUILD;
}

bool cvInternalGlobals::getTECH_DIFFUSION_ENABLE() const
{
	return m_bTECH_DIFFUSION_ENABLE;
}


// calculate asset checksum
uint32_t cvInternalGlobals::getAssetCheckSum() const
{
	PROFILE_EXTRA_FUNC();
	uint32_t iSum = 0;
	foreach_(const std::vector<CvInfoBase*>* infoVector, m_aInfoVectors)
	{
		foreach_(const CvInfoBase* info, *infoVector)
		{
			info->getCheckSum(iSum);
			logging::logMsg("Checksum.log", "%s : %u", info->getType(), iSum);
		}
	}
	return iSum;
}

// The ENGINE-side load-time indexes over info data -- GC-owned lists and the two enabler reverse-indexes, none
// of them an info MEMBER (a member derived from another info's data belongs to the general reverse pass's
// post-map derivation step, Data/CvReversePass.cpp). Every list is rebuilt whole.
// ⛔ It runs AFTER loadJson(JSON_LOAD_POSTMENU), never before: the postmenu pass is what completes the registry
// and re-runs mapFrom on every entity, so anything built ahead of it reads incomplete data and is then
// overwritten by the re-map.
void cvInternalGlobals::buildLoadTimeIndexes()
{
	PROFILE_EXTRA_FUNC();
	checkInitialCivics();

	// Index the buildings carrying EMPIRE-scope per-turn property sources (the converted <PropertiesAllCities>
	// one-shots -- property-audit.md one-shot ruling), so the per-city gather walks this short list instead of
	// every building info each solver turn. Clear-then-rebuild keeps the pass idempotent.
	m_allCitiesManipBuildings.clear();
	for (int iI = 0; iI < getNumBuildingInfos(); iI++)
	{
		if (getBuildingInfo(static_cast<BuildingTypes>(iI)).getPropertyManipulatorsAllCities()->getNumSources() > 0)
		{
			m_allCitiesManipBuildings.push_back(static_cast<BuildingTypes>(iI));
		}
	}

	// The map-generation bonus list (placement-ordered), rebuilt whole.
	{
		const int iNumBonusInfos = getNumBonusInfos();
		m_mapBonuses.clear();
		for (int iBonus = 0; iBonus < iNumBonusInfos; iBonus++)
		{
			if (getBonusInfo(static_cast<BonusTypes>(iBonus)).getPlacementOrder() > -1)
			{
				m_mapBonuses.push_back(static_cast<BonusTypes>(iBonus));
			}
		}
	}

	{
		//TB: Set Statuses and starsigns
		m_aiStatusPromotions.clear();
		m_starsigns.clear();
		for (int iI = 0; iI < GC.getNumPromotionInfos(); iI++)
		{
			const PromotionTypes ePromoX = static_cast<PromotionTypes>(iI);

			if (GC.getPromotionInfo(ePromoX).isStatus())
			{
				m_aiStatusPromotions.push_back(ePromoX);
			}
			else if (GC.getPromotionInfo(ePromoX).isStarsign())
			{
				m_starsigns.push_back(ePromoX);
			}
		}
	}

	CityOutputHistory::setCityOutputHistorySize((uint16_t)GC.getCITY_OUTPUT_HISTORY_SIZE());


}

// The hiding METHOD has two id spaces and this is the ONE map between them (vision.md §4). The METHOD KEY is
// the INVISIBLE_* registry -- small, fixed, and what the per-plot per-method registration is dimensioned by --
// while the CARRIER on a unit is a SKILL, because a promotion can grant a method (optical camouflage) and a
// registry entry cannot be granted. The names are a pure prefix swap (INVISIBLE_NAVAL_DISGUISE ->
// SKILL_NAVAL_DISGUISE), so neither side carries a table and the two cannot drift.
// Built lazily: the SKILL_* infotypes mint from the authored `skills` blocks, i.e. AFTER the info registry.
int cvInternalGlobals::getMethodSkill(InvisibleTypes eInvisible) const
{
	FASSERT_BOUNDS(0, getNumInvisibleInfos(), eInvisible);
	if ((int)m_invisibleMethodSkill.size() != getNumInvisibleInfos())
	{
		m_invisibleMethodSkill.assign(getNumInvisibleInfos(), -2);   // -2 = not yet resolved
	}
	int& iCached = m_invisibleMethodSkill[eInvisible];
	if (iCached == -2)
	{
		std::string szSkill = "SKILL_";
		szSkill += getInvisibleInfo(eInvisible).getType() + strlen("INVISIBLE_");
		iCached = getInfoTypeForString(szSkill.c_str(), /*bHideAssert*/true);
	}
	return iCached;
}

// "Invisibility method -> the trainable units that can see it". A pure function of info data, so it is
// identical on every client (lockstep/OOS safe) and never rebuilt during play. Units that can never be trained
// (ProductionCost -1, e.g. spawn-only creatures) are excluded: a civ "unlocking" one of those is no counter.
// Only units offering UNITAI_SEE_INVISIBLE count as seers -- recon units technically detect camouflage but roam
// the map instead of countering infiltrators, and counting them opened the NPC gate at TECH_TRAILS.
// ⚑ Built LAZILY on first ask, not at load: it resolves method SKILLS, and those mint from the authored
// `skills` blocks, so an eager build could run before the registry exists and cache -1 forever.
const std::vector<UnitTypes>& cvInternalGlobals::getUnitsSeeingInvisible(InvisibleTypes eInvisible) const
{
	FASSERT_BOUNDS(0, getNumInvisibleInfos(), eInvisible);
	if ((int)m_invisibleSeerUnits.size() != getNumInvisibleInfos())
	{
		m_invisibleSeerUnits.assign(getNumInvisibleInfos(), std::vector<UnitTypes>());

		for (int iMethod = 0; iMethod < getNumInvisibleInfos(); ++iMethod)
		{
			const int iMethodSkill = getMethodSkill((InvisibleTypes)iMethod);
			if (iMethodSkill < 0)
			{
				continue;
			}
			for (int iUnit = 0; iUnit < getNumUnitInfos(); ++iUnit)
			{
				const CvUnitInfo& kUnit = getUnitInfo((UnitTypes)iUnit);
				if (kUnit.getProductionCost() < 0 || !kUnit.hasUnitAI(UNITAI_SEE_INVISIBLE))
				{
					continue;
				}
				if (kUnit.getHideAndSeek().detectionAgainst(iMethodSkill) > 0)
				{
					m_invisibleSeerUnits[iMethod].push_back((UnitTypes)iUnit);
				}
			}
		}
	}
	return m_invisibleSeerUnits[eInvisible];
}

// Which method a UNIT TYPE hides by -- the info-level twin of CvUnit::getInvisibleType, for the callers that
// hold a type rather than an instance (the build list, the spawn gates).
InvisibleTypes cvInternalGlobals::getUnitMethod(UnitTypes eUnit) const
{
	const CvUnitInfo& kUnit = getUnitInfo(eUnit);
	for (int iI = 0; iI < getNumInvisibleInfos(); ++iI)
	{
		const int iMethodSkill = getMethodSkill((InvisibleTypes)iI);
		if (iMethodSkill >= 0 && kUnit.hasSkill(iMethodSkill))
		{
			return (InvisibleTypes)iI;
		}
	}
	return NO_INVISIBLE;
}

// Build, once at load, the map "enabler building B -> buildings whose constructibility
// B (or a free bonus B grants) can flip true". This is a pure function of info data, so
// it is identical on every client (lockstep/OOS safe) and never needs rebuilding during
// play. It is a *superset* of the true become-constructible set: getInvolvedGOMs returns
// every building/bonus a construct-condition references, which is a superset of those
// that flip it true. The CABV PreLoop confirms each candidate against the gate,
// so the resulting constructible set is identical to the old O(buildings^2) inner scan.
// #195 Phase 2: one-shot verification that the unified requirement model reproduces the
// typed prereq fields the enabler index relies on. Logged via the [PERF] channel (not
// asserted) so it surfaces in any DLL -- including FinalRelease -- when
// Autolog__LogLevelPerf >= 1. The index itself is built at load (buildLoadTimeIndexes) before
// gPerfLogLevel is read at game init, so this is called once from the CABV PreLoop, by which
// point the log level is set. mismatches=0 == the model faithfully backs the index.
void cvInternalGlobals::checkInitialCivics()
{
	PROFILE_EXTRA_FUNC();
	for (int iCiv = getNumCivilizationInfos() - 1; iCiv > -1; iCiv--)
	{
		CvCivilizationInfo& civ = getCivilizationInfo(static_cast<CivilizationTypes>(iCiv));

		for (int iJ = getNumCivicOptionInfos() - 1; iJ > -1; iJ--)
		{
			//No Initial Civic Found
			const CivicTypes eCivic = civ.getInitialCivic((CivicOptionTypes)iJ);

			if (eCivic == NO_CIVIC || getCivicInfo(eCivic).getCivicOption() != iJ)
			{
				//	The substitute only has to be VALID ([save.md] par.7, the FALLBACK class), and the
				//	start-available civics are exactly the ones the synthetic TECH_GAME_START root enables --
				//	the node every player always holds ([enabler.md] par.2). A civic carries NO tech in its own
				//	`requires`: tech drives MEMBERSHIP through `enables`, never the gate, so the old
				//	"first civic with no tech prereq" test has no forward reading and this asks the root instead.
				const std::vector<int>* pStartCivics = cascadeStartNode().edge(EDGEF_ENABLES, EDGEB_CIVICS);
				bool bFound = false;
				for (int iK = 0; pStartCivics != NULL && iK < getNumCivicInfos(); iK++)
				{
					if (getCivicInfo((CivicTypes)iK).getCivicOption() != iJ)
					{
						continue;
					}
					if (std::find(pStartCivics->begin(), pStartCivics->end(), iK) != pStartCivics->end())
					{
						bFound = true;
						civ.setInitialCivic((CivicOptionTypes)iJ, (CivicTypes)iK);
						break;
					}
				}
				if (!bFound)
				{
					// Should not get here, having no initial civic is very bad.
					FErrorMsg("Error, No Valid Civic Was Found!");
				}
			}
		}
	}
}

void cvInternalGlobals::cacheGameSpecificValues()
{
	PROFILE_EXTRA_FUNC();
	int iLevel = 0;

	foreach_(CvCultureLevelInfo* info, m_paCultureLevelInfo)
	{
		if (info->getPrereqGameOption() == NO_GAMEOPTION || getGame().isOption((GameOptionTypes)info->getPrereqGameOption()))
		{
			info->setLevel(iLevel++);
		}
		else info->setLevel(-1);
	}
}