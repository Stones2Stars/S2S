#pragma once
#ifndef CV_CASCADE_READJSON_H
#define CV_CASCADE_READJSON_H

//
//	CvCascadeReadJson -- the #430 cascade's FRESH picojson reader of the curated Assets/Data JSON (build plan:
//	docs/plans/structural-cleanup/readjson.md). Built FROM SCRATCH, interface-bounded; it NEVER threads through
//	CvInfoUtil / the XML read() path (cascade-engine-430.md §2b/§3). Conditionals parse through the StoneBase-ported
//	typed condition tree (cascadeParseCondition -> CvJsonCondition), NOT BoolExpr. It maps each entity's JSON to a
//	CvJsonInfo held in the per-info-type InfoRepo (the cascade machines -- modifier/enabler -- read it from there).
//

//	Map the whole Assets/Data set into the per-type InfoRepo, ONCE per process, at the SAME load point as the XML
//	infos -- called from cvInternalGlobals::doPostLoadCaching (after every info + the type registry is populated, with
//	the mod asset path available). The MAP is UNCONDITIONAL: it must NOT depend on gPlayerLogLevel (which can be cold
//	this early in the load) -- the [READJSON/*] survey instead rides the event spine (SD_READJSON, gated in the
//	consumer). So loading the game with the XML always populates the cascade's static data-feed for the machines.
void cascadeLoadJson();

#include <string>
//	The probe-stat stash (set=true stores at map time; set=false reads): what the DARK load-time [READJSON] burst
//	saw -- file count found under dataDir, entities parsed, and the dataDir string (the return). gPlayerLogLevel is 0
//	during doPostLoadCaching so the load-time census never reaches the log; a per-turn emitter (the [MODIFIER/repo]
//	census) re-surfaces these where logging is live. iFiles/iEntities are -1 if the probe never ran.
const std::string& cascadeReadJsonStats(bool bSet, int& iFiles, int& iEntities, const std::string& sDir);

//	Sibling stash for the KEEP-legacy heal/spawn/free-promo SHADOW-DIFF summary (set=true stores at map time; set=false
//	reads): checked = fields compared engine-vs-JSON, diverging = fields that mismatched. The load-time
//	[READJSON/healdiff-summary] burst is dark (gPlayerLogLevel 0 during doPostLoadCaching), so the per-turn [MODIFIER/repo]
//	census re-surfaces these where logging is live -- `diverging` is the headline the owner drives to 0. -1 if never run.
void cascadeReadJsonHealDiffStats(bool bSet, int& iChecked, int& iDiverging);

#endif // CV_CASCADE_READJSON_H
