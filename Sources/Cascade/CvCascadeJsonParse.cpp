//
//	CvCascadeJsonParse -- see the header. The shared, composable JSON parse primitives the CvJson*Info loaders reuse.
//	Behaviour is a faithful RELOCATION of the former one-function reader's spec/StoneBase-proven logic (json.md), not a
//	rewrite: the ×100 rule, the FK resolution, the {name:true}->set and {channel:value} shapes are lifted verbatim so
//	the "infos load themselves" refactor is behaviour-preserving.
//

#include "CvGameCoreDLL.h"          // PCH umbrella -- picojson, GC
#include "CvCascadeJsonParse.h"
#include "Defines/CvGlobals.h"      // GC.getInfoTypeForString -- the kept type registry (FK resolution)

// The single human -> ×100 fixed-point conversion (round half away from zero). 7 -> 700, 1.5 -> 150, -10 -> -1000.
int cascadeJsonX100(double h)
{
	return (int)(h >= 0 ? h * 100.0 + 0.5 : h * 100.0 - 0.5);
}

// --- load-time FK diagnostics (Orwell bar): the accumulator every resolve failure lands in, surfaced by the reader ---
static std::set<std::string> s_unresolved;

void cascadeJsonResetDiag()                                  { s_unresolved.clear(); }
const std::set<std::string>& cascadeJsonUnresolvedIds()     { return s_unresolved; }

int cascadeJsonResolveId(const std::string& id)
{
	const int rid = GC.getInfoTypeForString(id.c_str(), true);
	if (rid < 0 && s_unresolved.size() < 64) s_unresolved.insert(id);   // bounded -- surface the distinct misses
	return rid;
}

void cascadeJsonBoolSet(const picojson::value& v, std::set<std::string>& out)
{
	if (!v.is<picojson::object>()) return;
	const picojson::object& o = v.get<picojson::object>();
	for (picojson::object::const_iterator it = o.begin(); it != o.end(); ++it)
		if (it->second.is<bool>() && it->second.get<bool>()) out.insert(it->first);
}

void cascadeJsonCommerceMap(const picojson::value& v, std::map<std::string, int>& out)
{
	if (!v.is<picojson::object>()) return;
	const picojson::object& o = v.get<picojson::object>();
	for (picojson::object::const_iterator it = o.begin(); it != o.end(); ++it)
		if (it->second.is<double>()) out[it->first] = (int)it->second.get<double>();
}

// ===================== top-level key classification (json.md §1) -- the ONE vocabulary home =====================
// The enables-family source + target-side edges (§4.1/§4.2). `provides` (§5a) is a sibling edge dispatched separately.
static const char* CJK_EDGES[] = { "enables", "obsoletes", "replaces", "disables", "obsoletedBy", 0 };
// Intrinsic (§7) + auxiliary/bespoke (§9) + the §8 classification blocks -- everything the base SKIPS (a subclass /
// another system owns it). `policies`/`capabilities`/`skills`/`tags`/`state`/`attributes` (building §8) sit here so
// the base leaves them alone; the bespoke `shrine`/`headquarters` FK sections + era `worldGen` likewise.
static const char* CJK_INTRINSIC_KEYS[] = {
	"type", "text", "description", "help", "civilopedia", "message", "quote", "strategy", "adjective", "shortDescription",
	"cost", "ui", "world", "sound", "identity", "ai",
	"loadPrune", "policies", "succession", "excludes", "produces", "condition", "effect",
	"vision", "outcomes", "mapGeneration", "replacedBy", "capabilities", "skills", "tags", "state", "attributes", "builds",
	"promotionLine", "buildUp", "shrine", "headquarters", "worldGen", "properties", "voteSource", "threshold", "role", "victory",
	"targetLevel", "conversion", "cityFounding", "unitCapability", 0
};

bool cascadeJsonInList(const char** list, const std::string& key)
{
	for (int i = 0; list[i]; ++i) if (key == list[i]) return true;
	return false;
}

CascJsonKeyClass cascadeJsonClassifyKey(const std::string& key, bool valueIsObject)
{
	if (cascadeJsonInList(CJK_EDGES, key)) return CJK_EDGE;
	if (key == "provides")                 return CJK_PROVIDES;
	if (key == "allowed")                  return CJK_ALLOWED;
	if (key == "grants")                   return CJK_GRANTS;
	if (key == "requires")                 return CJK_REQUIRES;
	if (cascadeJsonInList(CJK_INTRINSIC_KEYS, key)) return CJK_INTRINSIC;
	return valueIsObject ? CJK_FAMILY : CJK_FLAG;   // unknown object = modifier family; unknown scalar = flag/text
}

const char* cascadeJsonKeyClassName(CascJsonKeyClass c)
{
	switch (c)
	{
	case CJK_EDGE:      return "edge";
	case CJK_PROVIDES:  return "provides";
	case CJK_ALLOWED:   return "allowed";
	case CJK_GRANTS:    return "grants";
	case CJK_REQUIRES:  return "requires";
	case CJK_INTRINSIC: return "intrinsic";
	case CJK_FAMILY:    return "family";
	case CJK_FLAG:      return "flag";
	default:            return "UNCLASSIFIED";
	}
}
