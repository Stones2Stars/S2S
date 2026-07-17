#include "CvGameCoreDLL.h"
#include "CvExeTrace.h"
#include "Cascade/CvEventSpine.h"
#include "Infrastructure/CvDLLInterfaceIFaceBase.h"

//
//	[EXE] -- the DLL->EXE graphics/dirty call trace. See CvExeTrace.h for the model.
//

namespace
{
	long g_exeCalls[NUM_EXE_TRACE_KINDS] = { 0 };

	const char* exeLinePrefix(int iEventId)
	{
		switch (iEventId)
		{
		case EXEK_UI_SETDIRTY:      return "[EXE/uiDirty]";
		case EXEK_ENG_SETDIRTY:     return "[EXE/engDirty]";
		case EXEK_COLORED_PLOT:     return "[EXE/coloredPlot]";
		case EXEK_CLEAR_COLORED:    return "[EXE/clearColored]";
		case EXEK_AREA_BORDER:      return "[EXE/areaBorder]";
		case EXEK_CLEAR_BORDER:     return "[EXE/clearBorder]";
		case EXEK_REBUILD_ALL:      return "[EXE/rebuildAllPlots]";
		case EXEK_REBUILD_PLOT:     return "[EXE/rebuildPlot]";
		case EXEK_REBUILD_TILE_ART: return "[EXE/rebuildTileArt]";
		case EXEK_REBUILD_RIVER:    return "[EXE/rebuildRiver]";
		case EXEK_TEXTURE_DIRTY:    return "[EXE/textureDirty]";
		case EXEK_BRIDGES_DIRTY:    return "[EXE/bridgesDirty]";
		case EXEK_RESOURCE_LAYER:   return "[EXE/resourceLayer]";
		case EXEK_EFFECT:           return "[EXE/effect]";
		case EXEK_SIGNS:            return "[EXE/signs]";
		case EXEK_VISIBILITY:       return "[EXE/visibility]";
		case EXEK_FOUNDING_BORDER:  return "[EXE/foundingBorder]";
		case EXEK_TREE_OFFSETS:     return "[EXE/treeOffsets]";
		case EXEK_GREAT_WALL:       return "[EXE/greatWall]";
		case EXEK_LAUNCH:           return "[EXE/launch]";
		case EXEK_SYMBOL_DISPLAY:   return "[EXE/symbolDisplay]";
		case EXEK_SYMBOLS:          return "[EXE/symbols]";
		case EXEK_MINIMAP_COLOR:    return "[EXE/minimapColor]";
		case EXEK_FLAG_DIRTY:       return "[EXE/flagDirty]";
		case EXEK_CENTER_UNIT:      return "[EXE/centerUnit]";
		case EXEK_PLOT_LAYOUT:      return "[EXE/plotLayout]";
		case EXEK_CITY_LAYOUT:      return "[EXE/cityLayout]";
		default:                    return NULL;
		}
	}

	const char* exeFieldInfo(int iFieldTag, SpineFieldType* peType)
	{
		*peType = SFT_INT;
		switch (iFieldTag)
		{
		case EXEF_bit: return "bit";
		case EXEF_val: return "val";
		case EXEF_rva: return "rva";
		default:       return NULL;
		}
	}

	struct ExeLogRegistrar { ExeLogRegistrar() { spineRegisterDomain(SD_EXE, &exeLinePrefix, "ExeTrace.log", &exeFieldInfo); } };
	ExeLogRegistrar s_exeLogRegistrar; // static-init registration

	void exeEmit(int eKind, int iA, int iB, void* pCallerRet)
	{
		if (eKind < 0 || eKind >= NUM_EXE_TRACE_KINDS)
		{
			return;
		}
		++g_exeCalls[eKind];
		static const char* s_pModuleBase = (const char*)GetModuleHandle("CvGameCoreDLL.dll");
		CvSpineEvent e(EVENTKIND_DIAGNOSTIC, SD_EXE, eKind, 1);
		e.addI(EXEF_bit, iA).addI(EXEF_val, iB)
		 .addI(EXEF_rva, (int)((const char*)pCallerRet - s_pModuleBase));
		eventSpine().emit(e);
	}
}

void exeSetUIDirty(int eDirtyBit, bool bNewValue)
{
	exeEmit(EXEK_UI_SETDIRTY, eDirtyBit, bNewValue ? 1 : 0, _ReturnAddress());
	gDLL->getInterfaceIFace()->setDirty((InterfaceDirtyBits)eDirtyBit, bNewValue);
}

void exeEng(int eKind, int iA, int iB)
{
	exeEmit(eKind, iA, iB, _ReturnAddress());
}

void exeEngFrom(int eKind, int iA, int iB, void* pCallerRet)
{
	exeEmit(eKind, iA, iB, pCallerRet);
}

volatile long gExeCyCityBarReads = 0;
volatile long gExePlotGetYieldCalls = 0;
volatile long gExePlotYieldRecomputes = 0;
volatile long gExePlotCalcYieldCalls = 0;

namespace
{
	const int EXE_RVA_SLOTS = 32;
	ExeRvaCount g_yieldCallers[EXE_RVA_SLOTS] = { { 0, 0 } };
	volatile long g_exeIn[NUM_EXE_IN_KINDS] = { 0 };
}

void exeYieldCallerSample(void* pCallerRet)
{
	static const char* s_pModuleBase = (const char*)GetModuleHandle("CvGameCoreDLL.dll");
	const long iRva = (long)((const char*)pCallerRet - s_pModuleBase);
	// Racy-tolerant linear probe: exactness is not the point, the census is (a lost increment is noise).
	for (int i = 0; i < EXE_RVA_SLOTS; i++)
	{
		if (g_yieldCallers[i].iRva == iRva)
		{
			++g_yieldCallers[i].iCount;
			return;
		}
		if (g_yieldCallers[i].iRva == 0)
		{
			g_yieldCallers[i].iRva = iRva;
			g_yieldCallers[i].iCount = 1;
			return;
		}
	}
	// table full: dropped (32 distinct sampled callers is far beyond the expected loop count)
}

const ExeRvaCount* exeYieldCallerTable(int* piSlots)
{
	*piSlots = EXE_RVA_SLOTS;
	return g_yieldCallers;
}

void exeIn(int eKind)
{
	if (eKind >= 0 && eKind < NUM_EXE_IN_KINDS)
	{
		InterlockedIncrement((volatile LONG*)&g_exeIn[eKind]);
	}
}

const long* exeInCounters()
{
	return (const long*)g_exeIn;
}

// ---------------------------------------------------------------------------
//	The EIP sampler (see CvExeTrace.h). Fixed hash table; the sampler thread is the only writer, readers snapshot
//	racy-tolerantly (a diagnostic census). NOTHING here allocates or takes a lock while a target thread is
//	suspended -- the suspend window is Suspend -> GetThreadContext -> Resume only.
// ---------------------------------------------------------------------------
#include <tlhelp32.h>

namespace
{
	const int SAMPLER_SLOTS = 32768;   // 256KB static -- power of two for the mask
	ExeEipCount g_eipBuckets[SAMPLER_SLOTS] = { { 0, 0 } };
	volatile long g_iSamplerTotal = 0;
	volatile long g_iSamplerRunning = 0;
	volatile long g_iSamplerStopAtTick = 0;
	DWORD g_iSamplerThreadId = 0;

	void samplerBucket(unsigned int uiEip)
	{
		if (uiEip >= 0x77000000u)
		{
			return;   // ntdll/kernel wait EIPs: idle threads, not workload -- keep the buckets for ACTIVE code
		}
		unsigned int uiSlot = (uiEip >> 2) * 2654435761u;   // Knuth multiplicative; EIPs share low-bit alignment
		for (int iProbe = 0; iProbe < 32; iProbe++)
		{
			ExeEipCount& kSlot = g_eipBuckets[(uiSlot + iProbe) & (SAMPLER_SLOTS - 1)];
			if (kSlot.uiEip == uiEip) { ++kSlot.iCount; return; }
			if (kSlot.uiEip == 0)     { kSlot.uiEip = uiEip; kSlot.iCount = 1; return; }
		}
		// 32 probes exhausted: dropped (table far larger than any real hot set)
	}

	DWORD WINAPI samplerThreadProc(LPVOID)
	{
		const DWORD iSelf = GetCurrentThreadId();
		const DWORD iPid = GetCurrentProcessId();
		std::vector<DWORD> threadIds;
		DWORD iLastEnum = 0;

		while (g_iSamplerRunning != 0 && (long)GetTickCount() < g_iSamplerStopAtTick)
		{
			const DWORD iNow = GetTickCount();
			if (threadIds.empty() || iNow - iLastEnum > 2000)
			{
				// re-enumerate the process's threads every ~2s (allocation happens HERE, nothing suspended)
				threadIds.clear();
				const HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
				if (hSnap != INVALID_HANDLE_VALUE)
				{
					THREADENTRY32 te; te.dwSize = sizeof(te);
					if (Thread32First(hSnap, &te))
					{
						do
						{
							if (te.th32OwnerProcessID == iPid && te.th32ThreadID != iSelf)
							{
								threadIds.push_back(te.th32ThreadID);
							}
						} while (Thread32Next(hSnap, &te));
					}
					CloseHandle(hSnap);
				}
				iLastEnum = iNow;
			}

			for (size_t i = 0; i < threadIds.size() && g_iSamplerRunning != 0; ++i)
			{
				const HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT, FALSE, threadIds[i]);
				if (hThread == NULL) continue;
				CONTEXT ctx; ctx.ContextFlags = CONTEXT_CONTROL;
				if (SuspendThread(hThread) != (DWORD)-1)
				{
					const BOOL bOk = GetThreadContext(hThread, &ctx);
					ResumeThread(hThread);
					if (bOk)
					{
						samplerBucket((unsigned int)ctx.Eip);
						InterlockedIncrement((volatile LONG*)&g_iSamplerTotal);
					}
				}
				CloseHandle(hThread);
			}
			Sleep(1);   // ~1kHz total pacing
		}
		g_iSamplerRunning = 0;
		return 0;
	}
}

void exeSamplerArm(int iSeconds)
{
	if (iSeconds <= 0) iSeconds = 30;
	if (iSeconds > 600) iSeconds = 600;
	g_iSamplerStopAtTick = (long)GetTickCount() + iSeconds * 1000;
	if (InterlockedCompareExchange((volatile LONG*)&g_iSamplerRunning, 1, 0) == 0)
	{
		// fresh window: zero the table (sampler not running yet -- safe)
		memset((void*)g_eipBuckets, 0, sizeof(g_eipBuckets));
		g_iSamplerTotal = 0;
		const HANDLE hThread = CreateThread(NULL, 0, samplerThreadProc, NULL, 0, &g_iSamplerThreadId);
		if (hThread != NULL) CloseHandle(hThread);
		else g_iSamplerRunning = 0;
	}
	// already running: the extended stop tick is the only change (the window stretches)
}

void exeSamplerStop()
{
	g_iSamplerRunning = 0;
}

bool exeSamplerRunning()
{
	return g_iSamplerRunning != 0;
}

int exeSamplerTop(ExeEipCount* aOut, int iMax, long* piTotal)
{
	*piTotal = g_iSamplerTotal;
	int iCount = 0;
	for (int i = 0; i < SAMPLER_SLOTS; i++)
	{
		const ExeEipCount kEntry = g_eipBuckets[i];   // racy-tolerant copy
		if (kEntry.uiEip == 0) continue;
		if (iCount < iMax)
		{
			aOut[iCount++] = kEntry;
		}
		else if (kEntry.iCount > aOut[iMax - 1].iCount)
		{
			aOut[iMax - 1] = kEntry;
		}
		else
		{
			continue;
		}
		for (int j = (iCount < iMax ? iCount : iMax) - 1; j > 0 && aOut[j].iCount > aOut[j - 1].iCount; --j)
		{
			const ExeEipCount kTmp = aOut[j]; aOut[j] = aOut[j - 1]; aOut[j - 1] = kTmp;
		}
	}
	return iCount;
}

namespace
{
	// one slot table per kind: [objId, lastAnswer] keyed by objId & MASK, collision-checked via the stored id
	const int FLIP_SLOTS = 16384;   // power of two
	struct FlipSlot { long iObjId; long iAnswer; };
	FlipSlot g_flipTable[NUM_EXE_IN_KINDS][FLIP_SLOTS];   // zero-init static; objId 0 uses id+1 keying below
	volatile long g_flipPolls[NUM_EXE_IN_KINDS] = { 0 };
	volatile long g_flipFlips[NUM_EXE_IN_KINDS] = { 0 };
}

void exeInAnswer(int eKind, int iObjId, int iAnswer)
{
	if (eKind < 0 || eKind >= NUM_EXE_IN_KINDS) return;
	InterlockedIncrement((volatile LONG*)&g_flipPolls[eKind]);
	const long iKey = (long)iObjId + 1;   // shift so a real id 0 never matches the zero-init empty slot
	FlipSlot& kSlot = g_flipTable[eKind][((unsigned int)iKey * 2654435761u) & (FLIP_SLOTS - 1)];
	if (kSlot.iObjId == iKey)
	{
		if (kSlot.iAnswer != (long)iAnswer)
		{
			kSlot.iAnswer = (long)iAnswer;
			InterlockedIncrement((volatile LONG*)&g_flipFlips[eKind]);
		}
	}
	else
	{
		// first sight or collision: adopt without counting a flip
		kSlot.iObjId = iKey;
		kSlot.iAnswer = (long)iAnswer;
	}
}

const long* exeFlipPolls() { return (const long*)g_flipPolls; }
const long* exeFlipFlips() { return (const long*)g_flipFlips; }

// ---------------------------------------------------------------------------
//	The EXE-order coalescing bracket (see CvExeTrace.h). Game-thread only.
// ---------------------------------------------------------------------------
namespace
{
	int g_iCoalesceDepth = 0;
	std::vector<unsigned char> g_aCoalesceMarks;   // per plot index: bit0=flag, bit1=centerUnit, bit2=minimap
}

void exeCoalesceBegin()
{
	if (g_iCoalesceDepth++ == 0)
	{
		g_aCoalesceMarks.assign(GC.getMap().numPlots(), 0);
	}
}

bool exeCoalesceMark(int iChannel, int iPlotIdx)
{
	if (g_iCoalesceDepth <= 0)
	{
		return false;
	}
	if (iPlotIdx >= 0 && iPlotIdx < (int)g_aCoalesceMarks.size())
	{
		g_aCoalesceMarks[iPlotIdx] |= (unsigned char)(1 << iChannel);
	}
	return true;
}

void exeCoalesceEnd()
{
	if (g_iCoalesceDepth <= 0)
	{
		g_iCoalesceDepth = 0;
		return;
	}
	if (--g_iCoalesceDepth == 0)
	{
		// flush OUTSIDE the bracket (depth 0): the real calls execute directly now
		int aiFlushed[3] = { 0, 0, 0 };
		for (int i = 0; i < (int)g_aCoalesceMarks.size(); i++)
		{
			const unsigned char ucMark = g_aCoalesceMarks[i];
			if (ucMark == 0) continue;
			CvPlot* pPlot = GC.getMap().plotByIndex(i);
			if (pPlot == NULL) continue;
			if (ucMark & 2) { pPlot->updateCenterUnit(); ++aiFlushed[1]; }   // first: it re-derives flag/minimap itself
			if (ucMark & 1) { pPlot->setFlagDirty(true); ++aiFlushed[0]; }
			if (ucMark & 4) { pPlot->updateMinimapColor(); ++aiFlushed[2]; }
		}
		exeEng(EXEK_ENG_SETDIRTY, aiFlushed[0] + aiFlushed[1], aiFlushed[2]);   // observability: the flush size rides the trace
		g_aCoalesceMarks.clear();
	}
}

const char* exeInKindName(int eKind)
{
	switch (eKind)
	{
	case EXIN_PLOT_CENTER_UNIT:        return "plot.getCenterUnit";
	case EXIN_PLOT_DEBUG_CENTER_UNIT:  return "plot.getDebugCenterUnit";
	case EXIN_PLOT_FLAG_SYMBOL:        return "plot.getFlagSymbol";
	case EXIN_PLOT_UPDATE_FLAG_SYMBOL: return "plot.updateFlagSymbol";
	case EXIN_PLOT_ROUTE_SYMBOL:       return "plot.getRouteSymbol";
	case EXIN_PLOT_RIVER_SYMBOL:       return "plot.getRiverSymbol";
	case EXIN_PLOT_ACTIVE_VISIBLE:     return "plot.isActiveVisible";
	case EXIN_PLOT_NUM_VISIBLE_UNITS:  return "plot.getNumVisibleUnits";
	case EXIN_PLOT_VISIBLE_ENEMY_UNIT: return "plot.isVisibleEnemyUnit";
	case EXIN_PLOT_VISIBLE_IMPROVEMENT:return "plot.getVisibleImprovementState";
	case EXIN_PLOT_VISIBLE_BONUS:      return "plot.getVisibleBonusState";
	case EXIN_PLOT_SYMBOL_OFFSET:      return "plot.getSymbolOffset";
	case EXIN_MAPX_UPDATE_FLAG_SYMBOLS:return "mapx.updateFlagSymbols";
	case EXIN_MAPX_UPDATE_FOG:         return "mapx.updateFog";
	case EXIN_MAPX_UPDATE_SYMBOL_VIS:  return "mapx.updateSymbolVisibility";
	case EXIN_MAPX_UPDATE_MINIMAP:     return "mapx.updateMinimapColor";
	case EXIN_MAPX_UPDATE_CENTER_UNIT: return "mapx.updateCenterUnit";
	case EXIN_MAPX_PLOT_BY_INDEX:      return "mapx.plotByIndex";
	case EXIN_MAPX_PLOT_XY:            return "mapx.plot";
	case EXIN_MAPX_POINT_TO_PLOT:      return "mapx.pointToPlot";
	case EXIN_MAPX_PLOT_TO_POINT:      return "mapx.plotToPoint";
	case EXIN_UNIT_IS_FIGHTING:        return "unit.isFighting";
	case EXIN_UNIT_IS_DEAD:            return "unit.isDead";
	case EXIN_UNIT_GET_DAMAGE:         return "unit.getDamage";
	case EXIN_UNIT_IS_INVISIBLE:       return "unit.isInvisible";
	case EXIN_UNIT_PLOT_EXTERNAL:      return "unit.plotExternal";
	case EXIN_UNIT_VIEWPORT_XY:        return "unit.getViewportXY";
	case EXIN_UNIT_AT_PLOT:            return "unit.atPlot";
	case EXIN_UNIT_CAN_MOVE:           return "unit.canMove";
	case EXIN_UNIT_HAS_MOVED:          return "unit.hasMoved";
	case EXIN_UNIT_IS_WAITING:         return "unit.isWaiting";
	case EXIN_UNIT_NOTIFY_ENTITY:      return "unit.NotifyEntity";
	default:                           return "?";
	}
}

const long* exeTraceCounters()
{
	return g_exeCalls;
}

const char* exeTraceKindName(int eKind)
{
	const char* szPrefix = exeLinePrefix(eKind);
	// "[EXE/name]" -> "name" would need allocation; the raw prefix is fine for the JSON key consumer.
	return szPrefix != NULL ? szPrefix : "?";
}
