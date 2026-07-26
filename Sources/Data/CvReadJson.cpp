//
//	readJson -- the ONE JSON reader of the curated Assets/Data set. See the header for the pipeline contract.
//
//	The reader is a THIN ORCHESTRATOR, not a god-function that knows every type. It:
//	  (1) enumerates + parses each Assets/Data/*.json ONCE into the retained store (loadJsonEnsureStore),
//	  (2) serves each category's entities to the per-category registration (loadJsonCategory -- manifest-ordered
//	      id assignment at that category's load point in CvXMLLoadUtility::LoadGlobalClassInfoJson),
//	  (3) at each phase end (loadJson), resolves every entity's type id (prefix dispatch, RJ_REPO_TYPES) and
//	      calls `data->mapFrom(v)` -- VIRTUAL dispatch: the info "loads itself" (the base parses the common
//	      sections; each per-type subclass adds its own typed members), drawing shared primitives from CvJsonParse,
//	  (4) runs a SEPARATE generic CENSUS that classifies every top-level key (jsonClassifyKey), surfaces every
//	      unresolved FK id (jsonUnresolvedIds), every unconsumed section, and every unknown key -- the three
//	      failure counts print UNCONDITIONALLY in the Loading.log coverage summary.
//

#include "CvGameCoreDLL.h"             // PCH umbrella -- picojson, windows.h, gDLL, GC
#include "Data/CvReadJson.h"
#include "Defines/CvGlobals.h"         // GC.getInfoTypeForString -- the type registry (entity id resolution)
#include "Spine/CvEventSpine.h"              // the #430 dispatch spine -- the [READJSON] census rides it as a CONSUMER
#include "CvJsonParse.h"               // jsonClassifyKey / jsonUnresolvedIds -- shared vocabulary + FK diag
#include "CvInfo.h"                // CvInfo (+ cascadeStartNode) -- the mapped info data + the TECH_GAME_START root
#include "Data/CvDepositIndex.h"     // DepositIndex::pushInfo/clearCompiled -- the compiled deposit index (push-time interning)
#include "UI/CvEntryText.h"          // entryDetailLine -- the ruling-29 per-entry renderer (the [READJSON/mod] sample proof)
#include "CvClassificationRegistry.h"  // the §8/§9 generated classification categories -- minted + resolved post-map
#include "CvTechInfo.h"            // CvTechInfo -- for the capabilities read-back survey (+ cascadeStartNode)
#include "CvImprovementInfo.h"     // complete type for the RJ_REPO_TYPES dispatch
#include "CvBuildingInfo.h"        // complete type for the RJ_REPO_TYPES dispatch
#include "CvUnitInfo.h"
#include "CvBonusInfo.h"
#include "CvCivicInfo.h"
#include "CvHeritageInfo.h"
#include "CvBuildInfo.h"
#include "CvPromotionInfo.h"
#include "CvSpecialistInfo.h"      // /state/info typed-member dispatch (rjInfoForType)
#include "CvUnitCombatInfo.h"      // /state/info typed-member dispatch (rjInfoForType)
// the JSON-fed uniformity set -- complete types for the RJ_REPO_TYPES dispatch (re-map + DepositIndex + /state/info)
#include "CvSpecialBuildingInfo.h"
#include "CvSpecialUnitInfo.h"
#include "CvEraInfo.h"
#include "CvHandicapInfo.h"
#include "CvGameSpeedInfo.h"
#include "CvLeaderHeadInfo.h"
#include "CvVictoryInfo.h"
#include "CvVoteInfo.h"
#include "CvHurryInfo.h"
#include "CvBonusClassInfo.h"
#include "CvInfos.h"               // the umbrella -- the remaining RJ_REPO_TYPES complete types with no own header
#include "Data/CvReversePass.h"        // reversePassRun/reversePassCounts -- the ONE general reverse pass ([DEC-one-reverse-view])
#include "Repos/InfoRepo.h"            // the per-info-type home (InfoRepo<CvXInfo>) -- readJson edit()s, mapFrom populates;
                                       // the CvXInfo tag types for the RJ_REPO_TYPES prefix dispatch are forward-declared there
#include <fstream>
#include <sstream>
#include <vector>
#include <set>
#include <map>
#include <string>

// The REVERSE VIEW (RELATED / REQUIRED_BY / the own-output landing / the forward compat reconstructions) is
// the ONE general pass in Data/CvReversePass.cpp ([DEC-one-reverse-view]) -- called from loadJson below.

// ===================== [READJSON] spine domain (logging.md §4: logging is a spine CONSUMER) =====================
enum RjEvt
{
	RJE_UNRESOLVED = 1, RJE_MOD, RJE_EDGE, RJE_GRANT, RJE_DIR, RJE_PROBE, RJE_COND_SURVEY, RJE_MOD_SURVEY,
	RJE_EDGE_SURVEY, RJE_EDGE_UNRES, RJE_GRANT_SURVEY, RJE_GRANT_UNRES, RJE_KEY, RJE_MAP, RJE_CAP_SURVEY,
	RJE_CAP, RJE_MAP_SUMMARY,
	RJE_SECTION_UNCONSUMED,  // an AUTHORED section the type composes no unit for -- the data never reaches a getter
	RJE_KEY_UNKNOWN,   // a non-reserved object key outside the closed family vocabulary ("<type>:<key>")
	RJE_REMAPPED,      // one aliased entity's full-registry section re-map: what it maps (type + edge/family counts)
	RJE_MAP_DONE,      // the initial JSON map (PASS 1+2) is DONE -- entities/resolved/remapped/ms
	RJE_REVERSE_DONE   // the general reverse pass (RELATED + REQUIRED_BY + own-output landing) is DONE -- add counts/ms
};

// DOMAIN-LOCAL field tags, shared by name across lines where a field recurs.
enum RjFld
{
	RJF_TYPE = 1, RJF_ADDR, RJF_UNIT, RJF_VAL, RJF_COND, RJF_PER, RJF_EDGE, RJF_BUCKET, RJF_ID, RJF_STATUS, RJF_DIR,
	RJF_FILES, RJF_PARSED, RJF_FAILED, RJF_ENTITIES, RJF_RESOLVED, RJF_UNRESOLVED, RJF_FAMILYKINDS, RJF_FLAGKINDS,
	RJF_REQCLAUSES, RJF_FAMILIES, RJF_MAGNITUDES, RJF_FLAT, RJF_PERCENT, RJF_MULT, RJF_OTHER, RJF_CONDITIONED,
	RJF_PERSCALED, RJF_AIONLY, RJF_BAREVALUES, RJF_EDGES, RJF_BUCKETENTRIES, RJF_BUCKETKINDS, RJF_ALLOWEDCLAUSES, RJF_CAPKINDS,
	RJF_LISTENTRIES, RJF_LISTKINDS, RJF_PULSES, RJF_PULSECHANNELS, RJF_FLAGS, RJF_ENTRYARRAYS, RJF_OBJECTS,
	RJF_KEY, RJF_COUNT, RJF_CLASS, RJF_DEPOSITS, RJF_REQBUILD, RJF_REQOPERATE, RJF_ALLOWED, RJF_GRANTLISTS,
	RJF_GRANTPULSES, RJF_GRANTING, RJF_CAPGRANTS, RJF_DISTINCTNAMES, RJF_NAME, RJF_WITHDATA,
	RJF_MS, RJF_REMAPPED, RJF_RELATED, RJF_REQUIREDBY, RJF_OWNOUTPUT,
	RJF_RENDERED   // the ruling-29 per-entry detail line (UI/CvEntryText) -- the renderer's observability proof
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
	case RJE_SECTION_UNCONSUMED: return "[READJSON/unconsumed-section]";
	case RJE_KEY_UNKNOWN:  return "[READJSON/unknown-key]";
	case RJE_GRANT_SURVEY: return "[READJSON/grant-survey]";
	case RJE_GRANT_UNRES:  return "[READJSON/grant-unresolved]";
	case RJE_KEY:          return "[READJSON/key]";
	case RJE_MAP:          return "[READJSON/map]";
	case RJE_CAP_SURVEY:   return "[READJSON/cap-survey]";
	case RJE_CAP:          return "[READJSON/cap]";
	case RJE_MAP_SUMMARY:  return "[READJSON/map-summary]";
	case RJE_REMAPPED:     return "[READJSON/remapped]";
	case RJE_MAP_DONE:     return "[READJSON/map-done]";
	case RJE_REVERSE_DONE: return "[READJSON/reverse-done]";
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
	case RJF_AIONLY:        return "aiOnly";
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
	case RJF_DEPOSITS:      return "modFamilies";   // was "deposits" -- the retired generic vector; now the §6 family count
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
	case RJF_MS:            return "ms";
	case RJF_REMAPPED:      return "remapped";
	case RJF_RELATED:       return "relatedIds";
	case RJF_REQUIREDBY:    return "requiredByIds";
	case RJF_OWNOUTPUT:     return "ownOutputLanded";
	case RJF_RENDERED:      *peType = SFT_WSTR; return "rendered";
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

// The JSON info-type table (X-macro): (type-prefix, CvXInfo class) -- ONE source of truth for the per-type InfoRepo
// selection (edit + clear-all), so a new JSON-fed info type is added in exactly ONE place. Order MATTERS:
// longer/more-specific prefixes FIRST (UNITCOMBAT_ before UNIT_, CIVICOPTION_ before
// CIVIC_, PROMOTIONLINE_ before PROMOTION_, BONUSCLASS_ before BONUS_; TRAIT_ covers TRAIT_COMPLEX_). Unlisted types
// -> NULL (no repo home): no full-registry re-map (so any FK naming a later-loading category stays dropped), no
// DepositIndex push, and no /state/info home -- listing a JSON-loaded category here is what makes it verifiable.
#define RJ_REPO_TYPES(X)                     \
	X("BUILDING_",      CvBuildingInfo)       \
	X("UNITCOMBAT_",    CvUnitCombatInfo)     \
	X("UNIT_",          CvUnitInfo)           \
	X("TECH_",          CvTechInfo)           \
	X("CIVICOPTION_",   CvCivicOptionInfo)    \
	X("CIVIC_",         CvCivicInfo)          \
	X("CIVILIZATION_",  CvCivilizationInfo)   \
	X("TRAIT_",         CvTraitInfo)          \
	X("SPECIALBUILDING_", CvSpecialBuildingInfo) \
	X("SPECIALUNIT_",   CvSpecialUnitInfo)    \
	X("SPECIALIST_",    CvSpecialistInfo)     \
	X("C2C_ERA_",       CvEraInfo)            \
	X("HANDICAP_",      CvHandicapInfo)       \
	X("GAMESPEED_",     CvGameSpeedInfo)      \
	X("LEADER_",        CvLeaderHeadInfo)     \
	X("VICTORY_",       CvVictoryInfo)        \
	X("VOTE_",          CvVoteInfo)           \
	X("HURRY_",         CvHurryInfo)          \
	X("BONUSCLASS_",    CvBonusClassInfo)     \
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
	X("WORLDSIZE_",     CvWorldInfo)          \
	X("PROPERTY_",      CvPropertyInfo)

// get-or-create the entity's CvInfo (the reader calls mapFrom on it); NULL for types with no repo home.
static CvInfo* rj_jsonEdit(const std::string& t, int id)
{
	if (id < 0) return NULL;
#define X(PFX, T) if (rj_starts(t, PFX)) return InfoRepo<T>::get().editPtr(id);
	RJ_REPO_TYPES(X)
#undef X
	return NULL;
}

// Is this type's repo ALIASED over GC.m_pa<X>Info? An aliased type's poco is created + mapFrom'd by its
// category's registration (LoadGlobalClassInfoJson) straight into the GC array this repo views; the full pass
// re-runs the idempotent mapFrom against the complete registry. The JSON-only OWNED types (Heritage/Build/
// complex traits) have no GC array home, so the full pass gives them their only map.
static bool rj_isAliased(const std::string& t)
{
#define X(PFX, T) if (rj_starts(t, PFX)) return InfoRepo<T>::get().isAliased();
	RJ_REPO_TYPES(X)
#undef X
	return false;
}

// The ONE INFOTYPE-prefix -> InfoRepo dispatch (exported; header decl) -- the /state/info observability read's
// resolver. It routes through RJ_REPO_TYPES so a category is registered in exactly ONE place
// ([DEC-single-implementation]): a second hand-maintained prefix list here is what silently drifted from the table
// and left a whole cutover wave unresolvable by the standing /state/info verification. Broader than the reverse
// pass's REQUIRED_BY router (HAVE-axis re-gate kinds only), and than the table itself, which carries no generated infos.
CvInfo* rjInfoForType(const std::string& t, int iId)
{
	// the synthetic root's cascade data lives OFF the InfoRepo (cascadeStartNode) -- route it there, never the GC poco
	if (t == "TECH_GAME_START") return &cascadeStartNode();
#define X(PFX, T) if (rj_starts(t, PFX)) return InfoRepo<T>::get().editPtr(iId);
	RJ_REPO_TYPES(X)
#undef X
	// the runtime-GENERATED classification categories (SKILL_/TAG_/ATTRIBUTE_/CAPABILITY_/POLICY_) -- referenceable
	// like any authored info ([DEC-classification-infos]); cold-path const view, cast for the shared return type.
	return const_cast<CvInfo*>(ClassificationRegistry::infoForType(t));
}

// PASS-1 id lookup -- REUSE-ONLY (owner ruling 2026-07-08). readJson mirrors the XML's premenu/postmenu load PHASING:
// a type is mapped only AFTER its XML shell has registered its id (the 63 premenu types at the LoadPreMenuGlobals
// map; the 18 postmenu process/vote/espionage/spawn types at the LoadPostMenuGlobals re-map). readJson must NOT mint
// an id for a type whose XML shell has not loaded yet -- that pre-registered a postmenu type before its XML array
// existed, so SetGlobalClassInfo saw it as "already loaded" and deref'd the empty array (aInfos[id], the load crash).
// So: return the XML-registered id if present, else -1 = DEFER (skipped this pass, mapped on the re-run once its XML
// phase has loaded). Complex traits reuse the ENGINE id (their own separate repo prevents collision with the simple set).
static int rj_registerId(const std::string& t)
{
	return GC.getInfoTypeForString(t.c_str(), true);   // >=0 reuse the XML shell's id; -1 defer to the phase that loads it
}

// Clear every InfoRepo (free all CvInfo) BEFORE (re)mapping, so a re-run can't DOUBLE the deposit vectors.
// No-op on the first run; the postmenu full pass relies on it to re-map cleanly.
static void rj_clearAllRepos()
{
	DepositIndex::clearCompiled();                // the compiled registry keys the about-to-be-freed infos -- drop it
	                                              // FIRST (the interner stays, append-only: ids survive the re-map)
#define X(PFX, T) InfoRepo<T>::get().clear();
	RJ_REPO_TYPES(X)
#undef X
	InfoRepo<CvComplexTraitTag>::get().clear();   // the complex trait set's own repo (off the RJ_REPO_TYPES dispatch)
	cascadeStartNodeReset();                      // the synthetic TECH_GAME_START root lives off the InfoRepo --
	                                              // reset-RECREATE (write-once discipline; a re-map gets a fresh node)
}

// ==================== THE RETAINED PARSE STORE (parse ONCE -- [DEC-one-json-reader]) ====================
// One record per *.json file under Assets/Data, built on FIRST use (the one disk read + one picojson parse of
// the whole set) and RETAINED across the premenu->postmenu load window: the per-category registrations and
// both full passes read from here, never from disk. Retention budget: ~21 MB of JSON text parses to an
// estimated ~70 MB of picojson structures on the 32-bit heap (node-count model over the full corpus) -- well
// inside the load-window budget; the store frees COMPLETELY when the postmenu pass ends, so no JSON-shaped
// object survives the load.
struct JsonFileRecord
{
	std::string path;          // absolute file path (diagnostics + the trait \complex\ discriminator)
	std::string relDir;        // directory relative to Assets\Data ("buildings\ancient", "traits\simple", ...)
	std::string type;          // the entity's "type" id ("" = not an entity)
	bool isOrderManifest;      // an _order.json category manifest (value = the ordered type array)
	picojson::value value;     // the ONE parse of this file
	int typeId;                // engine id resolved by the CURRENT full pass (-1 = deferred to a later phase)
	CvInfo* data;              // the info the CURRENT full pass mapped into (NULL = none)
	JsonFileRecord() : isOrderManifest(false), typeId(-1), data(NULL) {}
};

static std::vector<JsonFileRecord>* s_jsonStore = NULL;
static int s_jsonFilesFound = 0;      // every *.json the one walk saw (entities + manifests + failures)
static int s_jsonParseFailed = 0;     // unreadable / unparseable files (never stored)
static std::string s_jsonDataDir;

// Build the store on first use: the ONE Assets/Data walk, the ONE read + parse per file.
static void loadJsonEnsureStore()
{
	if (s_jsonStore != NULL) return;
	const DWORD scanT0 = GetTickCount();
	std::string base = gDLL->getModName(true);
	if (!base.empty() && base[base.size() - 1] != '\\' && base[base.size() - 1] != '/') base += "\\";
	s_jsonDataDir = base + "Assets\\Data";

	std::vector<std::string> files;
	rj_find(s_jsonDataDir, files);
	s_jsonFilesFound = (int)files.size();

	s_jsonStore = new std::vector<JsonFileRecord>();
	s_jsonStore->reserve(files.size());   // exact reserve: records are filled in place, values swapped in
	s_jsonParseFailed = 0;
	int iEntities = 0;
	int iManifests = 0;
	for (size_t i = 0; i < files.size(); ++i)
	{
		std::string text;
		if (!rj_readFile(files[i], text)) { ++s_jsonParseFailed; continue; }
		picojson::value parsed;
		const std::string err = picojson::parse(parsed, text);
		if (!err.empty()) { ++s_jsonParseFailed; continue; }
		s_jsonStore->push_back(JsonFileRecord());
		JsonFileRecord& rec = s_jsonStore->back();
		rec.path = files[i];
		const std::string rel = (files[i].size() > s_jsonDataDir.size() + 1) ? files[i].substr(s_jsonDataDir.size() + 1) : std::string();
		const std::string::size_type lastSlash = rel.find_last_of('\\');
		rec.relDir = (lastSlash != std::string::npos) ? rel.substr(0, lastSlash) : std::string();
		rec.isOrderManifest = files[i].size() >= 11 && files[i].substr(files[i].size() - 11) == "_order.json";
		rec.value.swap(parsed);
		if (rec.isOrderManifest) { ++iManifests; continue; }
		if (rec.value.is<picojson::object>())
		{
			const picojson::object& o = rec.value.get<picojson::object>();
			picojson::object::const_iterator t = o.find("type");
			if (t != o.end() && t->second.is<std::string>()) { rec.type = t->second.get<std::string>(); ++iEntities; }
		}
	}
	gDLL->logMsg("Loading.log", CvString::format(
		"[READJSON] store scan dir=%s files=%d entities=%d manifests=%d failed=%d ms=%u",
		s_jsonDataDir.c_str(), s_jsonFilesFound, iEntities, iManifests, s_jsonParseFailed,
		(unsigned)(GetTickCount() - scanT0)).c_str(), true, false);
}

// Free the store COMPLETELY (the postmenu pass end -- patterns.md: every JSON-shaped object is freed before
// load ends).
static void loadJsonFreeStore()
{
	delete s_jsonStore;
	s_jsonStore = NULL;
}

// Does the record's directory sit under the category folder (equal, or a subdirectory of it)?
static bool rj_inCategory(const std::string& szRelDir, const std::string& szFolder)
{
	if (szRelDir.size() < szFolder.size()) return false;
	if (szRelDir.compare(0, szFolder.size(), szFolder) != 0) return false;
	return szRelDir.size() == szFolder.size() || szRelDir[szFolder.size()] == '\\';
}

// The category ORDER-MANIFEST sort (`_order.json`, curator-derived -- Tools/Migration/curate_order.py): entities
// sort by their legacy XML document position, so the registered engine ids reproduce the LEGACY id order and every
// id-ordered UI surface keeps its familiar layout (the unit level-up promotion popup grouped each line's tiers
// adjacently because the XML did). A type ABSENT from the manifest (the synthetic TECH_GAME_START, future
// additions, a category with no manifest) sorts AFTER every listed one, alphabetically -- the legacy
// new-stuff-appends-last behaviour.
struct JsonCategoryEntityOrder
{
	const std::map<std::string, int>* pOrder;
	explicit JsonCategoryEntityOrder(const std::map<std::string, int>* p) : pOrder(p) {}
	int idx(const std::string& t) const
	{
		std::map<std::string, int>::const_iterator it = pOrder->find(t);
		return it != pOrder->end() ? it->second : MAX_INT;
	}
	bool operator()(const std::pair<std::string, const picojson::value*>& a, const std::pair<std::string, const picojson::value*>& b) const
	{
		const int ia = idx(a.first);
		const int ib = idx(b.first);
		if (ia != ib) return ia < ib;
		return a.first < b.first;
	}
};

// The per-category registration feed (header contract): the folder's parsed entities in manifest order,
// straight from the retained store.
void loadJsonCategory(const char* szDataFolder,
	std::vector<std::pair<std::string, const picojson::value*> >& aOutEntities)
{
	loadJsonEnsureStore();
	aOutEntities.clear();
	const std::string folder(szDataFolder);
	std::map<std::string, int> order;
	for (size_t i = 0; i < s_jsonStore->size(); ++i)
	{
		const JsonFileRecord& rec = (*s_jsonStore)[i];
		if (!rj_inCategory(rec.relDir, folder)) continue;
		if (rec.isOrderManifest)
		{
			if (rec.value.is<picojson::array>())
			{
				const picojson::array& orderedTypes = rec.value.get<picojson::array>();
				for (size_t j = 0; j < orderedTypes.size(); ++j)
				{
					if (!orderedTypes[j].is<std::string>()) continue;
					const int iPosition = (int)order.size();
					order[orderedTypes[j].get<std::string>()] = iPosition;
				}
			}
			continue;
		}
		if (rec.type.empty()) continue;
		aOutEntities.push_back(std::make_pair(rec.type, (const picojson::value*)&rec.value));
	}
	std::sort(aOutEntities.begin(), aOutEntities.end(), JsonCategoryEntityOrder(&order));
}

void loadJson(JsonLoadPhase eLoadPhase)
{
	// TWO-PHASE, mirroring the XML's premenu/postmenu load phasing + DELAYED READ. loadJson runs at the END of
	// BOTH LoadPreMenuGlobals and LoadPostMenuGlobals. rj_registerId is REUSE-ONLY, so each pass maps exactly
	// the types whose registration has landed by then: the premenu pass maps the premenu set (all the
	// terrain/plot/mapscript infos + the other premenu types); the postmenu pass -- once processes/votes/
	// espionage/spawns are registered too -- rj_clearAllRepos-frees the premenu pass and re-maps EVERYTHING
	// from the RETAINED store with FULL FK resolution (readJson's equivalent of the XML delayed read: FKs
	// resolve only when every target is loaded; a premenu-only map drops the edges to not-yet-loaded types --
	// the canMaintain empty-frontier bug). NOT one-shot: the postmenu re-run is what completes the FK edges,
	// and it ends by FREEING the store. UNCONDITIONAL: no gPlayerLogLevel dependency (cold this early) -- the
	// [READJSON/*] census rides the event spine (SD_READJSON; the log consumer gates per level).
	spineRegisterConsumers();   // register the spine's logging CONSUMER (idempotent) before the census emits
	rj_registerDomain();
	rj_clearAllRepos();           // re-map-safe (no-op first run)
	jsonResetDiag();              // reset the FK-unresolved accumulator (surfaced below)
	infoResetKindDiag();          // reset the kind-coverage accumulator (CvInfoKinds; surfaced below)

	// Always-on load timing (the spine census above is DARK at load -- gPlayerLogLevel 0). Grep `[READJSON]` in
	// Loading.log to SEE the JSON read progress + per-phase ms, so a slow/stuck load is diagnosable from the log.
	const DWORD s2sT0 = GetTickCount();
	gDLL->logMsg("Loading.log", CvString::format("[READJSON] BEGIN loadJson phase=%s",
		eLoadPhase == JSON_LOAD_PREMENU ? "premenu" : "postmenu").c_str(), true, false);

	loadJsonEnsureStore();   // normally already built by the first category registration -- a no-op then
	std::vector<JsonFileRecord>& store = *s_jsonStore;

	int iEntities = 0, iResolved = 0, iUnresolved = 0, iShownUnres = 0, iRemapped = 0;
	std::set<std::string> familyKinds, flagKinds;
	std::map<std::string, int> topKeys;                       // FULL-COVERAGE census: every top-level key kind -> count
	std::map<std::string, JsonKeyClass> keyClass;             // key -> its class (for the RJE_KEY completeness line)

	// ===== PASS 1 -- REGISTER: resolve every store entity's engine id BEFORE any mapFrom (the registry must be
	// COMPLETE first, else a FORWARD FK reference would drop). The parse already happened ONCE, at store build.
	// TECH_GAME_START is the synthetic no-engine-id root (id -1, off the InfoRepo).
	// COMPLEX traits (the `\complex\` folder) are keyed by the SAME ENGINE id as their type string. The simple and
	// complex sets live in SEPARATE repos (CvTraitInfo vs CvComplexTraitTag), so they never collide even sharing an
	// id. The active-set consumers (getTraitInfo / MMKernel::traitData) index the complex repo BY THE ENGINE id, so
	// it MUST be keyed that way; the category registration registers EVERY trait type (simple / complex-only
	// developing levels / shared) in m_paTraitInfo, so rj_registerId resolves the engine id for complex traits too.
	for (size_t i = 0; i < store.size(); ++i)
	{
		JsonFileRecord& rec = store[i];
		rec.typeId = -1;
		rec.data = NULL;
		if (rec.isOrderManifest || rec.type.empty()) continue;
		++iEntities;
		rec.typeId = (rec.type == "TECH_GAME_START") ? -1 : rj_registerId(rec.type);   // engine id (or -1 = DEFER to its load phase)
		if (rec.typeId >= 0) ++iResolved;
		else { ++iUnresolved; if (iShownUnres < 16) { eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_READJSON, RJE_UNRESOLVED, 1).addStr(RJF_TYPE, rec.type.c_str())); ++iShownUnres; } }
	}
	gDLL->logMsg("Loading.log", CvString::format("[READJSON] PASS1-register entities=%d resolved=%d deferred=%d ms=%u",
		iEntities, iResolved, iUnresolved, (unsigned)(GetTickCount() - s2sT0)).c_str(), true, false);

	// ===== PASS 2 -- MAP: registry complete -> each entity loads itself (mapFrom resolves its FKs against the FULL id
	// space). Complex traits collide on the engine id with the simple set -> their OWN repo (`\complex\` path is the
	// discriminator). The key-coverage census rides here.
	for (size_t s = 0; s < store.size(); ++s)
	{
		JsonFileRecord& rec = store[s];
		if (rec.isOrderManifest || rec.type.empty()) continue;
		if (!rec.value.is<picojson::object>()) continue;
		const picojson::object& o = rec.value.get<picojson::object>();
		const bool bComplexTrait = rec.typeId >= 0 && rj_starts(rec.type, "TRAIT_") && rec.path.find("\\complex\\") != std::string::npos;
		const bool bStartNode = (rec.type == "TECH_GAME_START");
		CvInfo* data = bStartNode ? &cascadeStartNode()
			: bComplexTrait ? InfoRepo<CvComplexTraitTag>::get().editPtr(rec.typeId)
			: rj_jsonEdit(rec.type, rec.typeId);
		rec.data = data;
		if (data != NULL)
		{
			// THE FULL-REGISTRY LINK RE-REGISTRATION (owner constraint: FK links register AFTER all JSONs are
			// loaded). An ALIASED poco (rj_jsonEdit -> GC.m_pa<X>Info) was mapFrom'd by its category's loader
			// MID-registry -- any FK naming a later-loading category silently dropped (the cross-category drop
			// defect: section edges AND subclass typed members alike). The registry is complete HERE,
			// so the FULL virtual mapFrom re-runs -- mapFrom is IDEMPOTENT BY CONTRACT (CvInfo.h: sections clear via
			// clearSections, subclass typed containers clear at their parse top), so every link (composed sections +
			// typed FK members) resolves against the full id space, no mismatch, no double-accumulation. The OWNED
			// objects -- the synthetic start node (cascadeStartNode) and the complex-trait set (CvComplexTraitTag),
			// both matching an aliased type PREFIX (TECH_/TRAIT_) without being the aliased GC objects -- get their
			// FIRST (and only) map here, on the same call.
			const bool bAliased = !bStartNode && !bComplexTrait && rj_isAliased(rec.type);
			data->mapFrom(rec.value);
			if (bAliased)
			{
				++iRemapped;
				eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_READJSON, RJE_REMAPPED, 1)
					.addStr(RJF_TYPE, rec.type.c_str())
					.addI(RJF_EDGES, data->getEdges() ? data->getEdges()->count() : 0)
					.addI(RJF_DEPOSITS, data->getModifiers() ? (int)data->getModifiers()->entries().size() : 0));
			}
			// the compiled deposit index PUSH runs AFTER the general reverse pass (below), so the reverse-landed
			// own-output entries compile into the index exactly like authored ones
		}
		for (picojson::object::const_iterator it = o.begin(); it != o.end(); ++it)
		{
			++topKeys[it->first];
			const JsonKeyClass c = jsonClassifyKey(it->first, it->second.is<picojson::object>());
			keyClass[it->first] = c;
			if (c == CJK_FAMILY) familyKinds.insert(it->first);
			else if (c == CJK_FLAG) flagKinds.insert(it->first);
		}
	}
	gDLL->logMsg("Loading.log", CvString::format("[READJSON] PASS2-map (mapFrom) remapped=%d ms=%u", iRemapped, (unsigned)(GetTickCount() - s2sT0)).c_str(), true, false);

	// ===== the §8/§9 CLASSIFICATION registries -- generated infos (SKILL_/TAG_/ATTRIBUTE_/CAPABILITY_/POLICY_)
	// minted from the union of authored block keys (append-only ids, stable across both load passes), then every
	// entity's blocks resolved to the by-id bitsets the O(1) getter surface reads (ClassificationRegistry).
	{
		std::vector<CvInfo*> mapped;
		mapped.reserve(store.size());
		for (size_t s = 0; s < store.size(); ++s)
			if (store[s].data != NULL) mapped.push_back(store[s].data);
		ClassificationRegistry::buildAndResolve(mapped);
		gDLL->logMsg("Loading.log", CvString::format("[READJSON] classification minted skills=%d tags=%d attributes=%d capabilities=%d policies=%d ms=%u",
			ClassificationRegistry::count(CLSD_SKILL), ClassificationRegistry::count(CLSD_TAG),
			ClassificationRegistry::count(CLSD_ATTRIBUTE), ClassificationRegistry::count(CLSD_CAPABILITY),
			ClassificationRegistry::count(CLSD_POLICY), (unsigned)(GetTickCount() - s2sT0)).c_str(), true, false);
	}
	// The initial JSON map is DONE -- the spine announcement (the owner's load-lifecycle observability):
	eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_READJSON, RJE_MAP_DONE, 1)
		.addI(RJF_ENTITIES, iEntities).addI(RJF_RESOLVED, iResolved).addI(RJF_REMAPPED, iRemapped)
		.addI(RJF_MS, (int)(GetTickCount() - s2sT0)));

	eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_READJSON, RJE_DIR, 1).addStr(RJF_DIR, s_jsonDataDir.c_str()));
	eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_READJSON, RJE_PROBE, 1)
		.addI(RJF_FILES, s_jsonFilesFound).addI(RJF_PARSED, s_jsonFilesFound - s_jsonParseFailed).addI(RJF_FAILED, s_jsonParseFailed)
		.addI(RJF_ENTITIES, iEntities).addI(RJF_RESOLVED, iResolved).addI(RJF_UNRESOLVED, iUnresolved)
		.addI(RJF_FAMILYKINDS, (int)familyKinds.size()).addI(RJF_FLAGKINDS, (int)flagKinds.size()));

	// FK diagnostics (Orwell bar): every distinct unresolved REFERENCED id (edges/grants/atoms/dormant) collected by
	// jsonResolveId during the maps -- surfaced so a data typo never hides.
	const std::set<std::string>& unres = jsonUnresolvedIds();
	for (std::set<std::string>::const_iterator it = unres.begin(); it != unres.end(); ++it)
		eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_READJSON, RJE_EDGE_UNRES, 1).addStr(RJF_ID, it->c_str()));

	// AUTHORED-BUT-UNCONSUMED sections (the same Orwell bar, the other half): a section the type composes NO unit
	// for is recorded by jsonNoteUnconsumed -- "never silently dropped" (CvInfo.h) only holds if someone READS the
	// census, and nothing did, so the misses sat dark. They are REAL data losses: the authored value never reaches
	// any getter (the SpecialBuilding group cap -- `allowed` authored, no CvAllowed composed -- was invisible
	// this way, leaving every group member offered at once). Surfaced beside the FK misses so the next one is loud.
	const std::set<std::string>& unc = jsonUnconsumedSections();
	for (std::set<std::string>::const_iterator it = unc.begin(); it != unc.end(); ++it)
		eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_READJSON, RJE_SECTION_UNCONSUMED, 1).addStr(RJF_ID, it->c_str()));

	// UNKNOWN top-level keys, per authoring entity ("<type>:<key>", recorded by the mapFrom dispatch): the spine
	// detail beside the unconditional Loading.log ERROR lines below -- WHO authored the stray key, not just which.
	const std::set<std::string>& unknownKeys = jsonUnknownKeys();
	for (std::set<std::string>::const_iterator it = unknownKeys.begin(); it != unknownKeys.end(); ++it)
		eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_READJSON, RJE_KEY_UNKNOWN, 1).addStr(RJF_ID, it->c_str()));

	// READ-BACK survey: reconstruct the modifier stats + per-entity structure counts from the MAPPED data (the
	// home) -- the compiled §6 entries on getModifiers(), the spec model ([DEC-json-not-cascade]) -- proving the
	// compile round-trips (values ×100'd, addresses interned, requires/edges/allowed/grants populated).
	int iAttached = 0, iMapSample = 0, iModSample = 0;
	int mMag = 0, mFlat = 0, mPercent = 0, mMult = 0, mOther = 0, mCond = 0, mPer = 0, mAiOnly = 0;
	for (size_t s = 0; s < store.size(); ++s)
	{
		const CvInfo* cd = store[s].data;
		if (cd == NULL) continue;
		++iAttached;
		const CvModifiers* mods = cd->getModifiers();
		if (mods != NULL)
		{
			const std::vector<CvModEntry*>& modEntries = mods->entries();
			for (size_t e = 0; e < modEntries.size(); ++e)
			{
				const CvModEntry* en = modEntries[e];
				if (en == NULL) continue;
				++mMag;
				if (en->unit == CASC_UNIT_FLAT) ++mFlat; else if (en->unit == CASC_UNIT_PERCENT) ++mPercent;
				else if (en->unit == CASC_UNIT_MULTIPLIER) ++mMult; else ++mOther;
				if (en->enabled != NULL || en->disabled != NULL) ++mCond;
				if (en->hasPer) ++mPer;   // the §3.7 per count-scaler
				if (en->aiOnly) ++mAiOnly;   // the §3.9 `ai` audience flag (the Orwell census keeps the axis visible)
				if (iModSample < 10)   // concrete value samples -- proves the single human->×100 conversion at the leaf
				{
					// the ruling-29 renderer's observability proof (the ONE demonstration consumer): each sample
					// carries its per-entry detail line through UI/CvEntryText, end to end through the spine
					const CvWString szRendered = entryDetailLine(*en);
					eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_READJSON, RJE_MOD, 1)
						.addStr(RJF_TYPE, store[s].type.c_str()).addStr(RJF_ADDR, en->address().c_str())
						.addStr(RJF_UNIT, DepositIndex::unitSegment(en->unit)).addI(RJF_VAL, en->value)
							.addI(RJF_AIONLY, en->aiOnly ? 1 : 0)   // the audience flag (never an address segment)
							.addWStr(RJF_RENDERED, szRendered.c_str()));
					++iModSample;
				}
			}
		}
		if (iMapSample < 8)
		{
			eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_READJSON, RJE_MAP, 1)
				.addStr(RJF_TYPE, store[s].type.c_str()).addI(RJF_DEPOSITS, cd->getModifiers() ? (int)cd->getModifiers()->entries().size() : 0)
				.addI(RJF_REQBUILD, cd->requiresBuild() ? 1 : 0).addI(RJF_REQOPERATE, cd->requiresOperate() ? 1 : 0)
				.addI(RJF_EDGES, cd->getEdges() ? cd->getEdges()->count() : 0)
				.addI(RJF_ALLOWED, cd->getAllowed() ? cd->getAllowed()->authoredCount() : 0)
				.addI(RJF_GRANTLISTS, cd->getGrants() ? (int)cd->getGrants()->lists().size() : 0)
				.addI(RJF_GRANTPULSES, cd->getGrants() ? cd->getGrants()->pulseCount() : 0));
			++iMapSample;
		}
	}
	eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_READJSON, RJE_MOD_SURVEY, 1)
		.addI(RJF_MAGNITUDES, mMag).addI(RJF_FLAT, mFlat).addI(RJF_PERCENT, mPercent).addI(RJF_MULT, mMult)
		.addI(RJF_OTHER, mOther).addI(RJF_CONDITIONED, mCond).addI(RJF_PERSCALED, mPer)
		.addI(RJF_AIONLY, mAiOnly).addI(RJF_FAMILYKINDS, (int)familyKinds.size()));

	// §8 capabilities read-back survey (now on CvTechInfo -- techs are the only grantor). Verifies the block maps.
	int capEntities = 0, capGrants = 0;
	std::set<std::string> capNames;
	for (size_t s = 0; s < store.size(); ++s)
	{
		if (store[s].data == NULL || !rj_starts(store[s].type, "TECH_")) continue;
		const CvTechInfo* tech = static_cast<const CvTechInfo*>(store[s].data);
		const CvClassificationBlock* caps = tech->getCapabilities();
		if (caps == NULL || caps->isEmpty()) continue;
		++capEntities;
		for (std::set<std::string>::const_iterator it = caps->all().begin(); it != caps->all().end(); ++it)
		{ ++capGrants; capNames.insert(*it); }
	}
	eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_READJSON, RJE_CAP_SURVEY, 1)
		.addI(RJF_GRANTING, capEntities).addI(RJF_CAPGRANTS, capGrants).addI(RJF_DISTINCTNAMES, (int)capNames.size()));
	for (std::set<std::string>::const_iterator it = capNames.begin(); it != capNames.end(); ++it)
		eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_READJSON, RJE_CAP, 1).addStr(RJF_NAME, it->c_str()));

	// FULL-COVERAGE census line: every top-level key kind + its class -- UNCLASSIFIED (impossible: classify always
	// returns family/flag for an unknown) is the thing to investigate.
	for (std::map<std::string, int>::const_iterator it = topKeys.begin(); it != topKeys.end(); ++it)
		eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_READJSON, RJE_KEY, 1)
			.addStr(RJF_KEY, it->first.c_str()).addI(RJF_COUNT, it->second)
			.addStr(RJF_CLASS, jsonKeyClassName(keyClass[it->first])));

	// THE GENERAL REVERSE PASS ([DEC-one-reverse-view]) -- the forward compat reconstructions, the
	// EDGEF_RELATED display inversion, the EDGEF_REQUIRED_BY re-gate index, and the own-output reverse
	// landing, in ONE pass over the compiled surfaces (Data/CvReversePass.cpp). Runs after every entity is
	// mapped so it inverts the final compiled state.
	reversePassRun();
	const ReversePassCounts& reverseCounts = reversePassCounts();

	// The compiled deposit index PUSH: every mapped info's §6 families (+ whenObsolete) intern + compile here,
	// AFTER the reverse pass, so the reverse-landed own-output entries enter the index like authored ones.
	for (size_t s = 0; s < store.size(); ++s)
	{
		if (store[s].data != NULL)
		{
			DepositIndex::pushInfo(store[s].data);
		}
	}
	gDLL->logMsg("Loading.log", CvString::format("[READJSON] deposit-index push done ms=%u",
		(unsigned)(GetTickCount() - s2sT0)).c_str(), true, false);

	// The general reverse pass is DONE -- the spine announcement (counts pre-dedup: the raw inversion volume).
	eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_READJSON, RJE_REVERSE_DONE, 1)
		.addI(RJF_RELATED, reverseCounts.relatedAdds).addI(RJF_REQUIREDBY, reverseCounts.requiredByAdds)
		.addI(RJF_OWNOUTPUT, reverseCounts.ownOutputLanded).addI(RJF_MS, (int)reverseCounts.milliseconds));
	gDLL->logMsg("Loading.log", CvString::format(
		"[READJSON] reverse-view related=%d requiredBy=%d ownOutputLanded=%d ownOutputSkipped=%d ms=%u",
		reverseCounts.relatedAdds, reverseCounts.requiredByAdds, reverseCounts.ownOutputLanded,
		reverseCounts.ownOutputSkipped, (unsigned)reverseCounts.milliseconds).c_str(), true, false);

	eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_READJSON, RJE_MAP_SUMMARY, 1).addI(RJF_WITHDATA, iAttached));

	// ===== THE FAIL-LOUD COVERAGE SUMMARY -- UNCONDITIONAL (patterns.md § The ONE reader). The three failure
	// counts print to Loading.log on EVERY load, no log-level gate: unresolved FK ids, authored-but-unconsumed
	// sections, and unknown top-level keys (non-reserved object keys outside the closed family vocabulary).
	// Every unknown key additionally gets its own LOUD ERROR line -- a typo'd section name must never load as
	// silent garbage family data. The per-item detail for the first two rides the SD_READJSON spine events above.
	int iUnknownKeyKinds = 0;
	for (std::map<std::string, int>::const_iterator it = topKeys.begin(); it != topKeys.end(); ++it)
	{
		if (keyClass[it->first] != CJK_UNKNOWN) continue;
		++iUnknownKeyKinds;
		gDLL->logMsg("Loading.log", CvString::format("[READJSON] ERROR unknown-key key=%s entities=%d",
			it->first.c_str(), it->second).c_str(), true, false);
	}
	gDLL->logMsg("Loading.log", CvString::format("[READJSON] coverage unresolvedFk=%d unconsumedSections=%d unknownKeys=%d",
		(int)jsonUnresolvedIds().size(), (int)jsonUnconsumedSections().size(), iUnknownKeyKinds).c_str(), true, false);

	// The kind-coverage half of the same bar (CvInfoKinds): every authored member the vocabulary does not carry
	// flowed through the compile pass as an interned segment -- surfaced per distinct family.member so a
	// batch-pending re-authoring (or a data typo) is visible on every load, never silently unkinded.
	const std::set<std::string>& unkindedMembers = infoUnkindedMembers();
	for (std::set<std::string>::const_iterator it = unkindedMembers.begin(); it != unkindedMembers.end(); ++it)
		gDLL->logMsg("Loading.log", CvString::format("[READJSON] unkinded-member %s", it->c_str()).c_str(), true, false);
	gDLL->logMsg("Loading.log", CvString::format("[READJSON] kind-coverage unkindedMembers=%d",
		(int)unkindedMembers.size()).c_str(), true, false);

	gDLL->logMsg("Loading.log", CvString::format("[READJSON] END withData=%d reverseIndex+survey done totalMs=%u", iAttached, (unsigned)(GetTickCount() - s2sT0)).c_str(), true, false);

	// The postmenu pass is the LAST reader of the retained parse: free the store completely -- after load, no
	// JSON-shaped object survives ([DEC-one-json-reader]).
	if (eLoadPhase == JSON_LOAD_POSTMENU)
	{
		loadJsonFreeStore();
		gDLL->logMsg("Loading.log", "[READJSON] retained parse store freed", true, false);
	}
}
