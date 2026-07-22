//
//	CvJsonParse -- see the header. The shared, composable JSON parse primitives the JsonInfo layer reuses.
//	Behaviour is a faithful RELOCATION of the retired Cascade-side parse helpers (themselves lifted from the spec/StoneBase-proven
//	reader): the ×100 rule, the FK resolution, the {name:true}->set and {channel:value} shapes are verbatim so the
//	relocation is behaviour-preserving. New here: the CJK_GATE class (entity-level enabled/disabled, the loadPrune
//	replacement) and the authored-but-unconsumed section diagnostic (the anti-"didn't pan out" census).
//

#include "CvGameCoreDLL.h"          // PCH umbrella -- picojson, GC
#include "CvJsonParse.h"
#include "Defines/CvGlobals.h"      // GC.getInfoTypeForString -- the kept type registry (FK resolution)

// The single human -> ×100 fixed-point conversion (round half away from zero). 7 -> 700, 1.5 -> 150, -10 -> -1000.
int jsonX100(double h)
{
	return (int)(h >= 0 ? h * 100.0 + 0.5 : h * 100.0 - 0.5);
}

// --- load-time diagnostics (Orwell bar): the accumulators every miss lands in, surfaced by the reader ---
static std::set<std::string> s_unresolved;
static std::set<std::string> s_unconsumed;

void jsonResetDiag()                                    { s_unresolved.clear(); s_unconsumed.clear(); }
const std::set<std::string>& jsonUnresolvedIds()        { return s_unresolved; }
const std::set<std::string>& jsonUnconsumedSections()   { return s_unconsumed; }

void jsonNoteUnconsumed(const std::string& szType, const std::string& szSection)
{
	if (s_unconsumed.size() < 64) s_unconsumed.insert(szType + ":" + szSection);   // bounded -- the distinct misses
}

int jsonResolveId(const std::string& id)
{
	const int rid = GC.getInfoTypeForString(id.c_str(), true);
	if (rid < 0 && s_unresolved.size() < 64) s_unresolved.insert(id);   // bounded -- surface the distinct misses
	return rid;
}

void jsonBoolSet(const picojson::value& v, std::set<std::string>& out)
{
	if (!v.is<picojson::object>()) return;
	const picojson::object& o = v.get<picojson::object>();
	for (picojson::object::const_iterator it = o.begin(); it != o.end(); ++it)
		if (it->second.is<bool>() && it->second.get<bool>()) out.insert(it->first);
}

void jsonCommerceMap(const picojson::value& v, std::map<std::string, int>& out)
{
	if (!v.is<picojson::object>()) return;
	const picojson::object& o = v.get<picojson::object>();
	for (picojson::object::const_iterator it = o.begin(); it != o.end(); ++it)
		if (it->second.is<double>()) out[it->first] = (int)it->second.get<double>();
}

// ===================== the shared JSON walkers -- the ONE canonical copy (see the header) =====================

const picojson::object* jsonChildObj(const picojson::object& o, const char* key)
{
	picojson::object::const_iterator it = o.find(key);
	return (it != o.end() && it->second.is<picojson::object>()) ? &it->second.get<picojson::object>() : NULL;
}

const picojson::object* jsonWorldArt(const picojson::object& o)
{
	const picojson::object* w = jsonChildObj(o, "world");
	return w ? jsonChildObj(*w, "art") : NULL;
}

double jsonFamDbl(const picojson::object& o, const char* family, const char* scope, const char* unit)
{
	const picojson::object* fo = jsonChildObj(o, family);  if (!fo) return 0.0;
	const picojson::object* so = jsonChildObj(*fo, scope); if (!so) return 0.0;
	picojson::object::const_iterator u = so->find(unit);
	return (u != so->end() && u->second.is<double>()) ? u->second.get<double>() : 0.0;
}

int jsonFamVal(const picojson::object& o, const char* family, const char* scope, const char* unit)
{ return (int)jsonFamDbl(o, family, scope, unit); }

int jsonFamMemberVal(const picojson::object& o, const char* family, const char* scope, const char* member, const char* unit)
{
	const picojson::object* fo = jsonChildObj(o, family);   if (!fo) return 0;
	const picojson::object* so = jsonChildObj(*fo, scope);  if (!so) return 0;
	const picojson::object* mo = jsonChildObj(*so, member); if (!mo) return 0;
	picojson::object::const_iterator u = mo->find(unit);
	return (u != mo->end() && u->second.is<double>()) ? (int)u->second.get<double>() : 0;
}

int jsonIdInt(const picojson::object& io, const char* key, int iDefault)
{
	picojson::object::const_iterator it = io.find(key);
	return (it != io.end() && it->second.is<double>()) ? (int)it->second.get<double>() : iDefault;
}

bool jsonIdBool(const picojson::object& io, const char* key)
{
	picojson::object::const_iterator it = io.find(key);
	return (it != io.end() && it->second.is<bool>()) ? it->second.get<bool>() : false;
}

int jsonIdFk(const picojson::object& io, const char* key)
{
	picojson::object::const_iterator it = io.find(key);
	return (it != io.end() && it->second.is<std::string>()) ? jsonResolveId(it->second.get<std::string>()) : -1;
}

bool jsonIdStr(const picojson::object& io, const char* key, std::string& out)
{
	picojson::object::const_iterator it = io.find(key);
	if (it == io.end() || !it->second.is<std::string>()) return false;
	out = it->second.get<std::string>(); return true;
}

void jsonReadFkMap(const picojson::object& parent, const char* key, std::map<int, int>& out)
{
	picojson::object::const_iterator it = parent.find(key);
	if (it == parent.end() || !it->second.is<picojson::object>()) return;
	const picojson::object& m = it->second.get<picojson::object>();
	for (picojson::object::const_iterator e = m.begin(); e != m.end(); ++e)
		if (e->second.is<double>()) { const int id = jsonResolveId(e->first); if (id >= 0) out[id] = (int)e->second.get<double>(); }
}

void jsonReadFlavours(const picojson::object& aiObj, std::map<int, int>& out)
{
	picojson::object::const_iterator fv = aiObj.find("flavours");
	if (fv == aiObj.end() || !fv->second.is<picojson::array>()) return;
	const picojson::array& a = fv->second.get<picojson::array>();
	for (size_t i = 0; i < a.size(); ++i)
		if (a[i].is<picojson::object>())
		{
			const picojson::object& fo = a[i].get<picojson::object>();
			for (picojson::object::const_iterator e = fo.begin(); e != fo.end(); ++e)
				if (e->second.is<double>()) { const int id = jsonResolveId(e->first); if (id >= 0) out[id] = (int)e->second.get<double>(); }
		}
}

CvCascScope jsonParseScope(const std::string& s, CvCascScope defaultScope)
{
	if (s == "world") return CASC_SCOPE_WORLD;   if (s == "team") return CASC_SCOPE_TEAM;
	if (s == "empire") return CASC_SCOPE_EMPIRE; if (s == "area") return CASC_SCOPE_AREA;
	if (s == "city") return CASC_SCOPE_CITY;     if (s == "plot") return CASC_SCOPE_PLOT;
	if (s == "improvement") return CASC_SCOPE_IMPROVEMENT; if (s == "feature") return CASC_SCOPE_FEATURE;
	if (s == "terrain") return CASC_SCOPE_TERRAIN; if (s == "route") return CASC_SCOPE_ROUTE;
	if (s == "building") return CASC_SCOPE_BUILDING; if (s == "specialist") return CASC_SCOPE_SPECIALIST;
	if (s == "unit") return CASC_SCOPE_UNIT;     if (s == "self") return CASC_SCOPE_SELF;
	return defaultScope;
}

// ===================== top-level key classification (json.md §1) -- the ONE vocabulary home =====================
// The enables-family source + target-side edges (§4.1/§4.2). `provides` (§5a) is a sibling edge dispatched separately.
static const char* CJK_EDGES[] = { "enables", "obsoletes", "replaces", "disables", "obsoletedBy", 0 };
// Intrinsic (§7) + auxiliary/bespoke (§9) + the §8 classification blocks -- everything the base's DISPATCH does not
// route to a section unit (a subclass / another system owns it). `policies`/`capabilities`/`skills`/`tags`/`state`/
// `attributes` sit here because their FLAT-BOOL halves are dispatched to the subclass's CvJsonBoolBlock units by the
// subclass itself (keyed skill extras stay typed subclass members); the bespoke `shrine`/`headquarters` FK sections +
// era `worldGen` likewise.
static const char* CJK_INTRINSIC_KEYS[] = {
	"type", "text", "description", "help", "civilopedia", "message", "quote", "strategy", "adjective", "shortDescription",
	"cost", "ui", "world", "sound", "identity", "ai",
	"policies", "succession", "excludes", "produces", "condition", "effect",
	"vision", "outcomes", "mapGeneration", "replacedBy", "capabilities", "skills", "tags", "state", "attributes", "builds",
	"promotionLine", "buildUp", "shrine", "headquarters", "worldGen", "properties", "voteSource", "threshold", "role", "victory",
	"targetLevel", "conversion", "cityFounding", "unitCapability",
	"canTrade", "canTradeOn", "canWorkOn",   // tech bespoke blocks (owner 2026-07-02, json.md §2 / capabilities.md)
	"spread",   // UNIT spread strength block: spread.religion/spread.corporation keyed maps (owner 2026-07-11 -- clearer than burying under timed `grants`)
	"groupSpawn",   // UNIT group-spawn config: struct rows {unitCombat, chance, title} (owner 2026-07-11 -- config, not a grant)
	0
};
// Purged vocabulary -- keys that must NEVER be parsed again; a straggler in the data surfaces in the unconsumed
// census (superseded-ideas.md). "loadPrune" was a curator-era invention (owner ruling 2026-07-08): its payload
// re-homed to the entity-level `enabled`/`disabled` gate.
static const char* CJK_RETIRED_KEYS[] = { "loadPrune", 0 };

bool jsonInList(const char** list, const std::string& key)
{
	for (int i = 0; list[i]; ++i) if (key == list[i]) return true;
	return false;
}

JsonKeyClass jsonClassifyKey(const std::string& key, bool valueIsObject)
{
	if (jsonInList(CJK_EDGES, key))        return CJK_EDGE;
	if (key == "provides")                 return CJK_PROVIDES;
	if (key == "allowed")                  return CJK_ALLOWED;
	if (key == "grants")                   return CJK_GRANTS;
	if (key == "requires")                 return CJK_REQUIRES;
	if (key == "whenObsolete")             return CJK_WHEN_OBSOLETE;   // §4.2 the obsolete-state modifier tree
	if (key == "enabled" || key == "disabled") return CJK_GATE;        // entity-level applicability (§3.9 at entity level)
	if (jsonInList(CJK_RETIRED_KEYS, key)) return CJK_RETIRED;
	if (jsonInList(CJK_INTRINSIC_KEYS, key)) return CJK_INTRINSIC;
	return valueIsObject ? CJK_FAMILY : CJK_FLAG;   // unknown object = modifier family; unknown scalar = flag/text
}

const char* jsonKeyClassName(JsonKeyClass c)
{
	switch (c)
	{
	case CJK_EDGE:      return "edge";
	case CJK_PROVIDES:  return "provides";
	case CJK_ALLOWED:   return "allowed";
	case CJK_GRANTS:    return "grants";
	case CJK_REQUIRES:  return "requires";
	case CJK_WHEN_OBSOLETE: return "whenObsolete";
	case CJK_GATE:      return "gate";
	case CJK_INTRINSIC: return "intrinsic";
	case CJK_FAMILY:    return "family";
	case CJK_FLAG:      return "flag";
	case CJK_RETIRED:   return "RETIRED";
	default:            return "UNCLASSIFIED";
	}
}
