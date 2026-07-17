#pragma once
#ifndef CV_EXE_TRACE_H
#define CV_EXE_TRACE_H

//
//	[EXE] -- the DLL->EXE graphics/dirty call trace (#430 observability).
//
//	Every DLL call that can make the closed EXE re-render -- a UI dirty bit, a landscape/scene mutator, a
//	plot/city layout invalidation -- emits a spine DIAGNOSTIC event carrying WHICH api (the kind), its arguments,
//	and the CALLER's RVA (resolve via the linker map / PDB). The EXE side of the render pipeline is a black box;
//	this trace is the complete record of what we TELL it to do, so an EXE-side churn (an FPS drop with an idle
//	DLL) is attributable to the exact call and call site instead of hypothesized. Per-kind counters are exposed
//	on /computed/perf (exeCalls) so rates are readable by double-sampling even with every log gate off.
//
//	The wrappers forward verbatim -- zero behavior change; the trace is observation only.
//

// The call kinds. One per DLL->EXE mutation api (the eventId of the [EXE/*] line).
enum ExeTraceKind
{
	EXEK_UI_SETDIRTY = 0,     // CvDLLInterfaceIFaceBase::setDirty (the 263-site UI dirty-bit surface)
	EXEK_ENG_SETDIRTY,        // CvDLLEngineIFaceBase::SetDirty
	EXEK_COLORED_PLOT,        // addColoredPlot
	EXEK_CLEAR_COLORED,       // clearColoredPlots
	EXEK_AREA_BORDER,         // fillAreaBorderPlot
	EXEK_CLEAR_BORDER,        // clearAreaBorderPlots
	EXEK_REBUILD_ALL,         // RebuildAllPlots
	EXEK_REBUILD_PLOT,        // RebuildPlot
	EXEK_REBUILD_TILE_ART,    // RebuildTileArt
	EXEK_REBUILD_RIVER,       // RebuildRiverPlotTile
	EXEK_TEXTURE_DIRTY,       // MarkPlotTextureAsDirty
	EXEK_BRIDGES_DIRTY,       // MarkBridgesDirty
	EXEK_RESOURCE_LAYER,      // setResourceLayer
	EXEK_EFFECT,              // TriggerEffect
	EXEK_SIGNS,               // clearSigns
	EXEK_VISIBILITY,          // Blacken/Darken/LightenVisibility (iA: 0=blacken 1=darken 2=lighten)
	EXEK_FOUNDING_BORDER,     // updateFoundingBorder
	EXEK_TREE_OFFSETS,        // ForceTreeOffsets
	EXEK_GREAT_WALL,          // Add/RemoveGreatWall (iA: 1=add 0=remove)
	EXEK_LAUNCH,              // AddLaunch
	EXEK_SYMBOL_DISPLAY,      // CvPlot::updateSymbolDisplay
	EXEK_SYMBOLS,             // CvPlot::updateSymbols
	EXEK_MINIMAP_COLOR,       // CvPlot::updateMinimapColor
	EXEK_FLAG_DIRTY,          // CvPlot::setFlagDirty
	EXEK_CENTER_UNIT,         // CvPlot::updateCenterUnit
	EXEK_PLOT_LAYOUT,         // CvPlot::setLayoutDirty(true)
	EXEK_CITY_LAYOUT,         // CvCity::setLayoutDirty(true)
	NUM_EXE_TRACE_KINDS
};

// [EXE] LOCAL field tags.
enum ExeTraceField
{
	EXEF_bit = 0,   // the dirty-bit / api-specific primary argument
	EXEF_val,       // the bool/secondary argument
	EXEF_rva        // the caller's module-relative return address (resolve via the linker map)
};

//	Ring A -- the UI dirty-bit choke point: emits, then forwards to gDLL->getInterfaceIFace()->setDirty.
//	(int param so call sites need no extra includes; the InterfaceDirtyBits enum converts implicitly.)
void exeSetUIDirty(int eDirtyBit, bool bNewValue);

//	Ring B/C -- the bare emit for the engine-iface mutators + the plot/city wrappers. Usable in a comma
//	expression before the forwarded call: exeEng(EXEK_REBUILD_PLOT), gDLL->getEngineIFace()->RebuildPlot(...);
void exeEng(int eKind, int iA = 0, int iB = 0);

//	Ring C variant for the CvPlot/CvCity graphics wrappers: the wrapper passes ITS caller's return address
//	(_ReturnAddress() at wrapper entry) so the rva names who invalidated, not the wrapper itself.
void exeEngFrom(int eKind, int iA, int iB, void* pCallerRet);

extern "C" void* _ReturnAddress(void);
#pragma intrinsic(_ReturnAddress)

//	The /computed/perf export: per-kind cumulative call counters (+ the kind names, indexed by ExeTraceKind).
const long* exeTraceCounters();
const char* exeTraceKindName(int eKind);

//	The EXE->DLL plot-yield read surface (the serialized-cache-death hypothesis instrument): CvPlot::getYield is a
//	DllExport the EXE render path may call per visible plot, and the plot-yield cache recompute is NOT attributed in
//	the (scope,channel) calc counters -- these three make that surface visible. Interlocked: render-thread callable.
//	The PYTHON boundary: every CyCity bar-data getter (getFoodTurnsLeft/foodDifference/getFood/growthThreshold/
//	getProductionNeeded/getProductionTurnsLeft/isProduction/isFoodProduction) ticks this -- the measured answer
//	to "does Python fetch city bar data per frame", replacing assertion with a rate.
extern volatile long gExeCyCityBarReads;

extern volatile long gExePlotGetYieldCalls;     // CvPlot::getYield(eIndex) entries (the exported read)
extern volatile long gExePlotYieldRecomputes;   // CvPlot::recomputeYieldInto entries (the cache was DIRTY at read)
extern volatile long gExePlotCalcYieldCalls;    // CvPlot::calculateYield entries (the raw full sum, any caller)

//	getYield caller SAMPLER: every 65536th getYield call records its caller's RVA into a small racy-tolerant
//	count table (a diagnostic census, not exact accounting) -- at ~917M calls/turn that is ~14k samples/turn for
//	near-zero cost, enough to name the loops that drive the volume. Read back via exeYieldCallerTable.
void exeYieldCallerSample(void* pCallerRet);
struct ExeRvaCount { long iRva; long iCount; };
const ExeRvaCount* exeYieldCallerTable(int* piSlots);

//	The EXE->DLL ENTRY counters (the mirror of the [EXE] DLL->EXE trace): one per instrumented DllExport the EXE
//	render/UI path can call. Counts only (InterlockedIncrement; render-thread callable, no spine emit -- consumers
//	assume the game thread). Exposed on /computed/perf as exeIn; double-sample for per-frame rates.
enum ExeInKind
{
	// CvPlot render exports
	EXIN_PLOT_CENTER_UNIT = 0, EXIN_PLOT_DEBUG_CENTER_UNIT, EXIN_PLOT_FLAG_SYMBOL, EXIN_PLOT_UPDATE_FLAG_SYMBOL,
	EXIN_PLOT_ROUTE_SYMBOL, EXIN_PLOT_RIVER_SYMBOL, EXIN_PLOT_ACTIVE_VISIBLE, EXIN_PLOT_NUM_VISIBLE_UNITS,
	EXIN_PLOT_VISIBLE_ENEMY_UNIT, EXIN_PLOT_VISIBLE_IMPROVEMENT, EXIN_PLOT_VISIBLE_BONUS, EXIN_PLOT_SYMBOL_OFFSET,
	// CvMapExternal proxies (the EXE-facing map surface)
	EXIN_MAPX_UPDATE_FLAG_SYMBOLS, EXIN_MAPX_UPDATE_FOG, EXIN_MAPX_UPDATE_SYMBOL_VIS, EXIN_MAPX_UPDATE_MINIMAP,
	EXIN_MAPX_UPDATE_CENTER_UNIT, EXIN_MAPX_PLOT_BY_INDEX, EXIN_MAPX_PLOT_XY, EXIN_MAPX_POINT_TO_PLOT,
	EXIN_MAPX_PLOT_TO_POINT,
	// CvUnit render exports
	EXIN_UNIT_IS_FIGHTING, EXIN_UNIT_IS_DEAD, EXIN_UNIT_GET_DAMAGE, EXIN_UNIT_IS_INVISIBLE, EXIN_UNIT_PLOT_EXTERNAL,
	EXIN_UNIT_VIEWPORT_XY, EXIN_UNIT_AT_PLOT, EXIN_UNIT_CAN_MOVE, EXIN_UNIT_HAS_MOVED, EXIN_UNIT_IS_WAITING,
	EXIN_UNIT_NOTIFY_ENTITY,
	NUM_EXE_IN_KINDS
};
void exeIn(int eKind);
const long* exeInCounters();
const char* exeInKindName(int eKind);

//	The ANSWER-STABILITY detector (the drop hunt's final layer): the EXE re-renders when a polled answer CHANGES,
//	so an evaluation that should be an event-maintained stable value but recomputes-per-poll into a DIFFERENT
//	answer drives EXE-side churn that samples as EXE/driver time. Per instrumented getter: record (objId, answer)
//	against the last poll; count flips. Read exeFlipPolls/exeFlipFlips per kind; a flip RATE that explodes
//	post-turn names the unstable feed. Collision-checked table (a colliding objId skips, never a false flip).
void exeInAnswer(int eKind, int iObjId, int iAnswer);
const long* exeFlipPolls();
const long* exeFlipFlips();

//	The EXE-ORDER COALESCING BRACKET (the drop experiment): during a player's doTurn, the per-unit-move scene
//	orders (flag dirty / center unit / minimap color -- ~210k per turn from setMoves/setXY churn) dedupe into
//	per-plot channel marks instead of firing; the bracket end flushes each DISTINCT plot once. Cuts the flood
//	~20-50x at the source. Channels: 0=flag, 1=centerUnit, 2=minimap.
void exeCoalesceBegin();
void exeCoalesceEnd();                       // flushes distinct plots, then deactivates
bool exeCoalesceMark(int iChannel, int iPlotIdx);   // true = coalescing active, order absorbed (caller returns)
// RAII form -- covers every return path of the bracketed scope (the PERF_SCOPE idiom).
struct ExeCoalesceBracket
{
	ExeCoalesceBracket() { exeCoalesceBegin(); }
	~ExeCoalesceBracket() { exeCoalesceEnd(); }
};

//	The EIP SAMPLER -- the poor-man's sampling profiler (the Profile configs are broken/banned; this is the ONE
//	CPU-attribution instrument, gated + on-demand). A dedicated Win32 thread enumerates the process's threads and
//	samples each EIP round-robin (SuspendThread -> GetThreadContext -> ResumeThread) at ~1kHz total, bucketing raw
//	EIPs in a fixed table (no allocation while any target is suspended). Armed from ANY thread (the HTTP server --
//	works mid-wall while the game thread is buried); read back as an EIP->count histogram, resolved offline via the
//	linker map (DLL base 0x10000000) / module bases (EXE 0x00400000).
void exeSamplerArm(int iSeconds);      // start (or extend) a sampling window; no-op if already running
void exeSamplerStop();
bool exeSamplerRunning();
struct ExeEipCount { unsigned int uiEip; long iCount; };
//	Snapshot the top-N buckets by count into aOut (caller-sized); returns the number filled + total samples via piTotal.
int exeSamplerTop(ExeEipCount* aOut, int iMax, long* piTotal);

#endif // CV_EXE_TRACE_H
