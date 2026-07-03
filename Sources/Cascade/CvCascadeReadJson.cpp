//
//	CvCascadeReadJson -- the fresh reader of the curated Assets/Data JSON. See the header for the build plan + scope.
//
//	AUDIT 2026-07-01 (owner): the reader is now a THIN ORCHESTRATOR, not a god-function that knows every type. It:
//	  (1) finds + parses each Assets/Data/*.json,
//	  (2) resolves the entity's type id + selects its per-type InfoRepo (prefix dispatch, RJ_REPO_TYPES),
//	  (3) calls `data->mapFrom(v)` -- VIRTUAL dispatch: the info "loads itself" (the base parses the common cascade
//	      sections; each per-type CvJson*Info subclass adds its ONE extension block -- skills/tags/capabilities/policies/
//	      identity), drawing shared primitives from CvCascadeJsonParse,
//	  (4) runs a SEPARATE generic CENSUS that classifies every top-level key (cascadeJsonClassifyKey) to prove 0
//	      UNCLASSIFIED, surfaces every unresolved FK id (cascadeJsonUnresolvedIds), and reads the mapped data back for
//	      the survey counts. The per-type parsing that used to live here (rj_walk{Capabilities,Policies,Identity,Shrine,
//	      Mod,EnableEdge,...} + s_rjData) has MOVED onto the types -- the reader no longer re-hand-rolls it.
//

#include "CvGameCoreDLL.h"             // PCH umbrella -- picojson, windows.h, gDLL, GC
#include "CvCascadeReadJson.h"
#include "Defines/CvGlobals.h"         // GC.getInfoTypeForString -- the type registry (entity id resolution)
#include "CvEventSpine.h"              // the #430 dispatch spine -- the [READJSON] census rides it as a CONSUMER
#include "CvCascadeJsonParse.h"        // cascadeJsonClassifyKey / cascadeJsonUnresolvedIds -- shared vocabulary + FK diag
#include "CvJsonInfo.h"                // CvJsonInfo (+ cascadeStartNode) -- the mapped info data + the TECH_GAME_START root
#include "CvJsonTechInfo.h"            // CvJsonTechInfo -- for the capabilities read-back survey
#include "Repos/InfoRepo.h"            // the per-info-type home (InfoRepo<CvXInfo>) -- readJson edit()s, mapFrom populates
// The cascade info classes = the InfoRepo<CvXInfo> tags for the RJ_REPO_TYPES prefix dispatch (specific headers, not the umbrella).
#include "Infos/CvBuildingInfo.h"
#include "Infos/CvUnitInfo.h"
#include "Infos/CvTechInfo.h"
#include "Infos/CvCivicInfo.h"
#include "Infos/CvCivicOptionInfo.h"
#include "Infos/CvTraitInfo.h"
#include "Infos/CvSpecialistInfo.h"
#include "Infos/CvBonusInfo.h"
#include "Infos/CvReligionInfo.h"
#include "Infos/CvCorporationInfo.h"
#include "Infos/CvPromotionInfo.h"
#include "Infos/CvPromotionLineInfo.h"
#include "Infos/CvImprovementInfo.h"
#include "Infos/CvFeatureInfo.h"
#include "Infos/CvTerrainInfo.h"
#include "Infos/CvRouteInfo.h"
#include "Infos/CvProjectInfo.h"
#include "Infos/CvProcessInfo.h"
#include "Infos/CvHeritageInfo.h"
#include "Infos/CvCultureLevelInfo.h"
#include "Infos/CvUnitCombatInfo.h"
#include "Infos/CvBuildInfo.h"
#include "Infos/CvPropertyInfo.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <set>
#include <map>
#include <string>

// ===================== [READJSON] spine domain (logging.md §4: logging is a spine CONSUMER) =====================
enum RjEvt
{
	RJE_UNRESOLVED = 1, RJE_MOD, RJE_EDGE, RJE_GRANT, RJE_DIR, RJE_PROBE, RJE_COND_SURVEY, RJE_MOD_SURVEY,
	RJE_EDGE_SURVEY, RJE_EDGE_UNRES, RJE_GRANT_SURVEY, RJE_GRANT_UNRES, RJE_KEY, RJE_MAP, RJE_CAP_SURVEY,
	RJE_CAP, RJE_MAP_SUMMARY
};

// DOMAIN-LOCAL field tags, shared by name across lines where a field recurs.
enum RjFld
{
	RJF_TYPE = 1, RJF_ADDR, RJF_UNIT, RJF_VAL, RJF_COND, RJF_PER, RJF_EDGE, RJF_BUCKET, RJF_ID, RJF_STATUS, RJF_DIR,
	RJF_FILES, RJF_PARSED, RJF_FAILED, RJF_ENTITIES, RJF_RESOLVED, RJF_UNRESOLVED, RJF_FAMILYKINDS, RJF_FLAGKINDS,
	RJF_REQCLAUSES, RJF_FAMILIES, RJF_MAGNITUDES, RJF_FLAT, RJF_PERCENT, RJF_MULT, RJF_OTHER, RJF_CONDITIONED,
	RJF_PERSCALED, RJF_BAREVALUES, RJF_EDGES, RJF_BUCKETENTRIES, RJF_BUCKETKINDS, RJF_ALLOWEDCLAUSES, RJF_CAPKINDS,
	RJF_LISTENTRIES, RJF_LISTKINDS, RJF_PULSES, RJF_PULSECHANNELS, RJF_FLAGS, RJF_ENTRYARRAYS, RJF_OBJECTS,
	RJF_KEY, RJF_COUNT, RJF_CLASS, RJF_DEPOSITS, RJF_REQBUILD, RJF_REQOPERATE, RJF_ALLOWED, RJF_GRANTLISTS,
	RJF_GRANTPULSES, RJF_GRANTING, RJF_CAPGRANTS, RJF_DISTINCTNAMES, RJF_NAME, RJF_WITHDATA
};

static const char* rj_prefix(int evt)
{
	switch (evt)
	{
	case RJE_UNRESOLVED:   return "[READJSON/unresolved]";
	case RJE_MOD:          return "[READJSON/mod]";
	case RJE_EDGE:         return "[READJSON/edge]";
	case RJE_GRANT:        return "[READJSON/grant]";
	case RJE_DIR:          return "[READJSON/dir]";
	case RJE_PROBE:        return "[READJSON/probe]";
	case RJE_COND_SURVEY:  return "[READJSON/cond-survey]";
	case RJE_MOD_SURVEY:   return "[READJSON/mod-survey]";
	case RJE_EDGE_SURVEY:  return "[READJSON/edge-survey]";
	case RJE_EDGE_UNRES:   return "[READJSON/unresolved-fk]";
	case RJE_GRANT_SURVEY: return "[READJSON/grant-survey]";
	case RJE_GRANT_UNRES:  return "[READJSON/grant-unresolved]";
	case RJE_KEY:          return "[READJSON/key]";
	case RJE_MAP:          return "[READJSON/map]";
	case RJE_CAP_SURVEY:   return "[READJSON/cap-survey]";
	case RJE_CAP:          return "[READJSON/cap]";
	case RJE_MAP_SUMMARY:  return "[READJSON/map-summary]";
	default:               return "[READJSON]";
	}
}

static const char* rj_field(int tag, SpineFieldType* peType)
{
	*peType = SFT_INT;
	switch (tag)
	{
	case RJF_TYPE:          *peType = SFT_STR; return "type";
	case RJF_ADDR:          *peType = SFT_STR; return "addr";
	case RJF_UNIT:          *peType = SFT_STR; return "unit";
	case RJF_VAL:           return "val";
	case RJF_COND:          return "conditioned";
	case RJF_PER:           return "per";
	case RJF_EDGE:          *peType = SFT_STR; return "edge";
	case RJF_BUCKET:        *peType = SFT_STR; return "bucket";
	case RJF_ID:            *peType = SFT_STR; return "id";
	case RJF_STATUS:        *peType = SFT_STR; return "status";
	case RJF_DIR:           *peType = SFT_STR; return "dir";
	case RJF_FILES:         return "files";
	case RJF_PARSED:        return "parsed";
	case RJF_FAILED:        return "failed";
	case RJF_ENTITIES:      return "entities";
	case RJF_RESOLVED:      return "resolved";
	case RJF_UNRESOLVED:    return "unresolved";
	case RJF_FAMILYKINDS:   return "familyKinds";
	case RJF_FLAGKINDS:     return "flagKinds";
	case RJF_REQCLAUSES:    return "requiresClauses";
	case RJF_FAMILIES:      return "families";
	case RJF_MAGNITUDES:    return "magnitudes";
	case RJF_FLAT:          return "flat";
	case RJF_PERCENT:       return "percent";
	case RJF_MULT:          return "mult";
	case RJF_OTHER:         return "other";
	case RJF_CONDITIONED:   return "conditioned";
	case RJF_PERSCALED:     return "perScaled";
	case RJF_BAREVALUES:    return "bareValues";
	case RJF_EDGES:         return "edges";
	case RJF_BUCKETENTRIES: return "bucketEntries";
	case RJF_BUCKETKINDS:   return "bucketKinds";
	case RJF_ALLOWEDCLAUSES:return "allowedClauses";
	case RJF_CAPKINDS:      return "capKinds";
	case RJF_LISTENTRIES:   return "listEntries";
	case RJF_LISTKINDS:     return "listKinds";
	case RJF_PULSES:        return "pulses";
	case RJF_PULSECHANNELS: return "pulseChannels";
	case RJF_FLAGS:         return "flags";
	case RJF_ENTRYARRAYS:   return "entryArrays";
	case RJF_OBJECTS:       return "objects";
	case RJF_KEY:           *peType = SFT_STR; return "key";
	case RJF_COUNT:         return "count";
	case RJF_CLASS:         *peType = SFT_STR; return "class";
	case RJF_DEPOSITS:      return "deposits";
	case RJF_REQBUILD:      return "reqBuild";
	case RJF_REQOPERATE:    return "reqOperate";
	case RJF_ALLOWED:       return "allowed";
	case RJF_GRANTLISTS:    return "grantLists";
	case RJF_GRANTPULSES:   return "grantPulses";
	case RJF_GRANTING:      return "grantingEntities";
	case RJF_CAPGRANTS:     return "capGrants";
	case RJF_DISTINCTNAMES: return "distinctNames";
	case RJF_NAME:          *peType = SFT_STR; return "name";
	case RJF_WITHDATA:      return "entitiesWithCascadeData";
	default:                return NULL;
	}
}

// Self-register the SD_READJSON domain once (idempotent) -- so the spine stays domain-agnostic (it never names readJson).
static void rj_registerDomain()
{
	static bool s_reg = false;
	if (!s_reg) { spineRegisterDomain(SD_READJSON, rj_prefix, "Cascade.log", rj_field); s_reg = true; }
}

static bool rj_starts(const std::string& s, const char* p)
{
	const std::string ps(p);
	return s.size() >= ps.size() && s.compare(0, ps.size(), ps) == 0;
}

static bool rj_readFile(const std::string& path, std::string& out)
{
	std::ifstream f(path.c_str(), std::ios::binary);
	if (!f.is_open()) return false;
	std::ostringstream ss; ss << f.rdbuf(); out = ss.str();
	return true;
}

// Recursive *.json walk under a dir (Assets/Data is per-type subdirs). Win32, C++03.
static void rj_find(const std::string& dir, std::vector<std::string>& out)
{
	const std::string pattern = dir + "\\*";
	WIN32_FIND_DATAA fd;
	HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
	if (h == INVALID_HANDLE_VALUE) return;
	do
	{
		const std::string name = fd.cFileName;
		if (name == "." || name == "..") continue;
		const std::string full = dir + "\\" + name;
		if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) rj_find(full, out);
		else if (name.size() > 5 && name.substr(name.size() - 5) == ".json") out.push_back(full);
	} while (FindNextFileA(h, &fd) != 0);
	FindClose(h);
}

// The cascade info-type table (X-macro): (type-prefix, CvXInfo class) -- ONE source of truth for the per-type InfoRepo
// selection (edit + clear-all), so a new cascade info type is added in exactly ONE place (cascade-engine-430.md §3
// care-point (d)). Order MATTERS: longer/more-specific prefixes FIRST (UNITCOMBAT_ before UNIT_, CIVICOPTION_ before
// CIVIC_, PROMOTIONLINE_ before PROMOTION_; TRAIT_ covers TRAIT_COMPLEX_). Unlisted types -> NULL (no cascade home).
#define RJ_REPO_TYPES(X)                     \
	X("BUILDING_",      CvBuildingInfo)       \
	X("UNITCOMBAT_",    CvUnitCombatInfo)     \
	X("UNIT_",          CvUnitInfo)           \
	X("TECH_",          CvTechInfo)           \
	X("CIVICOPTION_",   CvCivicOptionInfo)    \
	X("CIVIC_",         CvCivicInfo)          \
	X("TRAIT_",         CvTraitInfo)          \
	X("SPECIALIST_",    CvSpecialistInfo)     \
	X("BONUS_",         CvBonusInfo)          \
	X("RELIGION_",      CvReligionInfo)       \
	X("CORPORATION_",   CvCorporationInfo)    \
	X("PROMOTIONLINE_", CvPromotionLineInfo)  \
	X("PROMOTION_",     CvPromotionInfo)      \
	X("IMPROVEMENT_",   CvImprovementInfo)    \
	X("FEATURE_",       CvFeatureInfo)        \
	X("TERRAIN_",       CvTerrainInfo)        \
	X("ROUTE_",         CvRouteInfo)          \
	X("PROJECT_",       CvProjectInfo)        \
	X("PROCESS_",       CvProcessInfo)        \
	X("HERITAGE_",      CvHeritageInfo)       \
	X("CULTURELEVEL_",  CvCultureLevelInfo)   \
	X("BUILD_",         CvBuildInfo)          \
	X("PROPERTY_",      CvPropertyInfo)

// get-or-create the entity's CvJsonInfo (the reader calls mapFrom on it); NULL for non-cascade types.
static CvJsonInfo* rj_jsonEdit(const std::string& t, int id)
{
	if (id < 0) return NULL;
#define X(PFX, T) if (rj_starts(t, PFX)) return InfoRepo<T>::get().editPtr(id);
	RJ_REPO_TYPES(X)
#undef X
	return NULL;
}

// Clear every cascade InfoRepo (free all CvJsonInfo) BEFORE (re)mapping, so a re-run can't DOUBLE the deposit vectors
// (cascade-engine-430.md §3 care-point (a)). No-op on the one-shot first run; makes the map re-run-safe at cutover.
static void rj_clearAllRepos()
{
#define X(PFX, T) InfoRepo<T>::get().clear();
	RJ_REPO_TYPES(X)
#undef X
	InfoRepo<CvComplexTraitTag>::get().clear();   // the complex trait set's own repo (off the RJ_REPO_TYPES dispatch)
	cascadeStartNode().clear();                   // the synthetic TECH_GAME_START root lives off the InfoRepo
	// The start node IS a CvJsonTechInfo (crash fix 2026-07-02) and base clear() is non-virtual -- clear the
	// tech-side extension too so a re-probe never carries stale capabilities.
	static_cast<CvJsonTechInfo&>(cascadeStartNode()).capabilities.clear();
}

// One walked entity: its type string, engine id, and the CvJsonInfo it mapped into (a stable pointer -- the InfoRepo /
// start node / complex repo own it and outlive this call -- so the census reads it straight back).
struct RjEntity { std::string type; int typeId; CvJsonInfo* data; };

// Probe-stat stash (set=true stores; set=false reads). Lets a post-load emitter surface what the DARK load-time
// burst saw: how many files the dataDir scan found + how many entities parsed, and the dataDir string itself.
const std::string& cascadeReadJsonStats(bool bSet, int& iFiles, int& iEntities, const std::string& sDir)
{
	static int s_files = -1, s_entities = -1;   // -1 = the probe never ran
	static std::string s_dir;
	if (bSet) { s_files = iFiles; s_entities = iEntities; s_dir = sDir; }
	else { iFiles = s_files; iEntities = s_entities; }
	return s_dir;
}

void cascadeLoadJson()
{
	// ONE-SHOT per process: the static JSON->InfoRepo map is built ONCE, at the END of LoadPostMenuGlobals -- the
	// LAST XML load stage, so EVERY info type is in the registry. ⛔ NOT doPostLoadCaching (the previous home): that
	// fires pre-menu, BEFORE processes/votes/espionage-missions/spawns register, so their FK edges silently dropped
	// (the canMaintain empty-frontier bug, 2026-07-02). UNCONDITIONAL: it must NOT depend on gPlayerLogLevel (cold
	// this early) -- the [READJSON/*] census rides the event spine (SD_READJSON; the log consumer gates per level).
	static bool s_done = false;
	if (s_done) return;
	s_done = true;
	cascadeRegisterConsumers();   // register the spine's logging CONSUMER (idempotent) before the census emits
	rj_registerDomain();
	rj_clearAllRepos();           // care-point (a): re-map-safe (no-op first run)
	cascadeJsonResetDiag();       // reset the FK-unresolved accumulator (surfaced below)

	std::string base = gDLL->getModName(true);
	if (!base.empty() && base[base.size() - 1] != '\\' && base[base.size() - 1] != '/') base += "\\";
	const std::string dataDir = base + "Assets\\Data";

	std::vector<std::string> files;
	rj_find(dataDir, files);

	int iFailed = 0, iEntities = 0, iResolved = 0, iUnresolved = 0, iShownUnres = 0;
	std::set<std::string> familyKinds, flagKinds;
	std::map<std::string, int> topKeys;                       // FULL-COVERAGE census: every top-level key kind -> count
	std::map<std::string, CascJsonKeyClass> keyClass;         // key -> its class (for the RJE_KEY completeness line)
	std::vector<RjEntity> store;

	for (size_t i = 0; i < files.size(); ++i)
	{
		std::string text;
		if (!rj_readFile(files[i], text)) { ++iFailed; continue; }
		picojson::value v;
		const std::string err = picojson::parse(v, text);
		if (!err.empty() || !v.is<picojson::object>()) { ++iFailed; continue; }
		const picojson::object& o = v.get<picojson::object>();

		picojson::object::const_iterator t = o.find("type");
		if (t == o.end() || !t->second.is<std::string>()) continue;
		++iEntities;
		const std::string type = t->second.get<std::string>();
		const int typeId = GC.getInfoTypeForString(type.c_str(), true);   // entity id (separate from the FK diag)
		if (typeId >= 0) ++iResolved;
		else { ++iUnresolved; if (iShownUnres < 16) { eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_READJSON, RJE_UNRESOLVED, 1).addStr(RJF_TYPE, type.c_str())); ++iShownUnres; } }

		// SELECT the per-type InfoRepo + MAP: the info loads itself (virtual mapFrom). TECH_GAME_START is the synthetic
		// no-prereq root (XML-less -> id -1) homed off the InfoRepo in cascadeStartNode(); complex traits collide on the
		// engine id with the simple set -> their OWN repo (folder path `\complex\` is the discriminator).
		const bool bComplexTrait = typeId >= 0 && rj_starts(type, "TRAIT_") && files[i].find("\\complex\\") != std::string::npos;
		CvJsonInfo* data = (type == "TECH_GAME_START") ? &cascadeStartNode()
			: bComplexTrait ? InfoRepo<CvComplexTraitTag>::get().editPtr(typeId)
			: rj_jsonEdit(type, typeId);
		if (data != NULL) data->mapFrom(v);

		RjEntity rec; rec.type = type; rec.typeId = typeId; rec.data = data;
		store.push_back(rec);

		// CENSUS: classify every top-level key (the SAME cascadeJsonClassifyKey the base mapFrom dispatches on) -> the
		// 0-UNCLASSIFIED completeness proof, independent of the per-type parsing.
		for (picojson::object::const_iterator it = o.begin(); it != o.end(); ++it)
		{
			++topKeys[it->first];
			const CascJsonKeyClass c = cascadeJsonClassifyKey(it->first, it->second.is<picojson::object>());
			keyClass[it->first] = c;
			if (c == CJK_FAMILY) familyKinds.insert(it->first);
			else if (c == CJK_FLAG) flagKinds.insert(it->first);
		}
	}

	// STASH the probe stats for post-load re-emission (the load-time burst is dark: gPlayerLogLevel is 0 here, so
	// the log consumer drops these lines -- the [MODIFIER/repo] census re-emits them per turn where logging is live).
	int iStashFiles = (int)files.size(), iStashEntities = iEntities;
	cascadeReadJsonStats(true, iStashFiles, iStashEntities, dataDir);

	eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_READJSON, RJE_DIR, 1).addStr(RJF_DIR, dataDir.c_str()));
	eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_READJSON, RJE_PROBE, 1)
		.addI(RJF_FILES, (int)files.size()).addI(RJF_PARSED, (int)files.size() - iFailed).addI(RJF_FAILED, iFailed)
		.addI(RJF_ENTITIES, iEntities).addI(RJF_RESOLVED, iResolved).addI(RJF_UNRESOLVED, iUnresolved)
		.addI(RJF_FAMILYKINDS, (int)familyKinds.size()).addI(RJF_FLAGKINDS, (int)flagKinds.size()));

	// FK diagnostics (Orwell bar): every distinct unresolved REFERENCED id (edges/grants/atoms/dormant) collected by
	// cascadeJsonResolveId during the maps -- surfaced so a data typo never hides.
	const std::set<std::string>& unres = cascadeJsonUnresolvedIds();
	for (std::set<std::string>::const_iterator it = unres.begin(); it != unres.end(); ++it)
		eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_READJSON, RJE_EDGE_UNRES, 1).addStr(RJF_ID, it->c_str()));

	// READ-BACK survey: reconstruct the modifier-deposit stats + per-entity structure counts from the MAPPED data (the
	// home), proving the map round-trips (deposits ×100'd, requires/edges/allowed/grants populated).
	int iAttached = 0, iMapSample = 0, iModSample = 0;
	int mMag = 0, mFlat = 0, mPercent = 0, mMult = 0, mOther = 0, mCond = 0, mPer = 0;
	for (size_t s = 0; s < store.size(); ++s)
	{
		const CvJsonInfo* cd = store[s].data;
		if (cd == NULL) continue;
		++iAttached;
		for (size_t d = 0; d < cd->deposits.size(); ++d)
		{
			const CvCascadeDeposit& dep = cd->deposits[d];
			++mMag;
			if (dep.unit == "flat") ++mFlat; else if (dep.unit == "percent") ++mPercent;
			else if (dep.unit == "multiplier") ++mMult; else ++mOther;
			if (dep.enabled || dep.disabled) ++mCond;
			if (dep.hasPer) ++mPer;
			if (iModSample < 10)   // concrete value samples -- proves the single human->×100 conversion at the leaf
			{
				eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_READJSON, RJE_MOD, 1)
					.addStr(RJF_TYPE, store[s].type.c_str()).addStr(RJF_ADDR, dep.address.c_str())
					.addStr(RJF_UNIT, dep.unit.c_str()).addI(RJF_VAL, dep.value100));
				++iModSample;
			}
		}
		if (iMapSample < 8)
		{
			eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_READJSON, RJE_MAP, 1)
				.addStr(RJF_TYPE, store[s].type.c_str()).addI(RJF_DEPOSITS, (int)cd->deposits.size())
				.addI(RJF_REQBUILD, cd->requiresBuild ? 1 : 0).addI(RJF_REQOPERATE, cd->requiresOperate ? 1 : 0)
				.addI(RJF_EDGES, (int)cd->edges.size()).addI(RJF_ALLOWED, (int)cd->allowed.size())
				.addI(RJF_GRANTLISTS, (int)cd->grantLists.size()).addI(RJF_GRANTPULSES, (int)cd->grantPulses.size()));
			++iMapSample;
		}
	}
	eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_READJSON, RJE_MOD_SURVEY, 1)
		.addI(RJF_MAGNITUDES, mMag).addI(RJF_FLAT, mFlat).addI(RJF_PERCENT, mPercent).addI(RJF_MULT, mMult)
		.addI(RJF_OTHER, mOther).addI(RJF_CONDITIONED, mCond).addI(RJF_PERSCALED, mPer).addI(RJF_FAMILYKINDS, (int)familyKinds.size()));

	// §8 capabilities read-back survey (now on CvJsonTechInfo -- techs are the only grantor). Verifies the block maps.
	int capEntities = 0, capGrants = 0;
	std::set<std::string> capNames;
	for (size_t s = 0; s < store.size(); ++s)
	{
		if (store[s].data == NULL || !rj_starts(store[s].type, "TECH_")) continue;
		const CvJsonTechInfo* tech = static_cast<const CvJsonTechInfo*>(store[s].data);
		if (tech->capabilities.empty()) continue;
		++capEntities;
		for (std::set<std::string>::const_iterator it = tech->capabilities.begin(); it != tech->capabilities.end(); ++it)
		{ ++capGrants; capNames.insert(*it); }
	}
	eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_READJSON, RJE_CAP_SURVEY, 1)
		.addI(RJF_GRANTING, capEntities).addI(RJF_CAPGRANTS, capGrants).addI(RJF_DISTINCTNAMES, (int)capNames.size()));
	for (std::set<std::string>::const_iterator it = capNames.begin(); it != capNames.end(); ++it)
		eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_READJSON, RJE_CAP, 1).addStr(RJF_NAME, it->c_str()));

	// FULL-COVERAGE census line: every top-level key kind + its class -- UNCLASSIFIED (impossible: classify always
	// returns family/flag for an unknown) is the thing to investigate.
	for (std::map<std::string, int>::const_iterator it = topKeys.begin(); it != topKeys.end(); ++it)
		eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_READJSON, RJE_KEY, 1)
			.addStr(RJF_KEY, it->first.c_str()).addI(RJF_COUNT, it->second)
			.addStr(RJF_CLASS, cascadeJsonKeyClassName(keyClass[it->first])));

	eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_READJSON, RJE_MAP_SUMMARY, 1).addI(RJF_WITHDATA, iAttached));
}
