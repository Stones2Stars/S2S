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
