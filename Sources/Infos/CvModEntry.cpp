//
//	CvModEntry -- the compiled §3.9 entry's shared machinery: the unit-key vocabulary, the ONE §3.7 `per`
//	parser (shared with the trigger entries' chance.per), the address-segment interner (append-only,
//	spell-back capable), and the diagnostics-only address render. See the header.
//

#include "CvGameCoreDLL.h"          // PCH umbrella -- picojson
#include "CvModEntry.h"
#include "CvJsonParse.h"            // jsonResolveId + jsonParseScope -- the per count-scaler type FK and the
                                    // shared §3.2 scope-token vocabulary
#include <map>

CvCascUnit cascadeUnitFromString(const std::string& s)
{
	if (s == "flat")                return CASC_UNIT_FLAT;
	if (s == "percent")             return CASC_UNIT_PERCENT;
	if (s == "multiplier")          return CASC_UNIT_MULTIPLIER;
	if (s == "postMultiplier")      return CASC_UNIT_POST_MULTIPLIER;
	if (s == "rawPercent")          return CASC_UNIT_RAW_PERCENT;
	if (s == "perPopulation")       return CASC_UNIT_PER_POPULATION;
	if (s == "perSpecialist")       return CASC_UNIT_PER_SPECIALIST;
	if (s == "perCorporationLevel") return CASC_UNIT_PER_CORPORATION_LEVEL;
	return CASC_UNIT_UNKNOWN;
}

// --- the address-segment interner (append-only; ids stay valid across a readJson re-map) ---
static std::map<std::string, int> s_modSegments;
static std::vector<std::string> s_modSegmentSpellings;

int modSegmentIntern(const std::string& szSegment)
{
	const std::map<std::string, int>::const_iterator it = s_modSegments.find(szSegment);
	if (it != s_modSegments.end())
	{
		return it->second;
	}
	const int iId = (int)s_modSegmentSpellings.size();
	s_modSegments.insert(std::make_pair(szSegment, iId));
	s_modSegmentSpellings.push_back(szSegment);
	return iId;
}

int modSegmentLookup(const std::string& szSegment)
{
	const std::map<std::string, int>::const_iterator it = s_modSegments.find(szSegment);
	return it == s_modSegments.end() ? -1 : it->second;
}

const char* modSegmentSpell(int iSegmentId)
{
	if (iSegmentId < 0 || iSegmentId >= (int)s_modSegmentSpellings.size())
	{
		return "";
	}
	return s_modSegmentSpellings[iSegmentId].c_str();
}

std::string CvModEntry::address() const
{
	std::string szAddress;
	for (int i = 0; i < nSeg && i < MOD_ENTRY_SEGS; ++i)
	{
		if (i > 0)
		{
			szAddress += ".";
		}
		szAddress += modSegmentSpell(seg[i]);
	}
	return szAddress;
}

// The §3.7 `per` count-scaler on an entry: {type|anyOf, each?, scope?} or a bare type string. Shared (declared
// in the header): a trigger entry's `chance.per` parses through this same function -- ONE per parser.
void jsonParsePer(CvModEntry* entry, const picojson::value& v)
{
	entry->hasPer = true;
	if (v.is<std::string>())
	{
		entry->perType = v.get<std::string>();
		entry->perTypeId = jsonResolveId(entry->perType);
		return;
	}
	if (!v.is<picojson::object>())
	{
		return;
	}
	const picojson::object& o = v.get<picojson::object>();
	picojson::object::const_iterator it;
	if ((it = o.find("type")) != o.end() && it->second.is<std::string>())
	{
		entry->perType = it->second.get<std::string>();
		entry->perTypeId = jsonResolveId(entry->perType);
	}
	if ((it = o.find("each")) != o.end() && it->second.is<double>())
	{
		entry->perEach = (int)it->second.get<double>();
	}
	// the §3.7 `above:` over-threshold scaler (ruling 26): a LITERAL threshold, or a TOKEN kept as its spelling
	// (CITY_LIMIT resolves source-side at load -- CvModifiers::resolveAboveToken -- and scales at eval)
	if ((it = o.find("above")) != o.end())
	{
		if (it->second.is<double>())
		{
			entry->hasAbove = true;
			entry->perAbove = (int)it->second.get<double>();
		}
		else if (it->second.is<std::string>())
		{
			entry->hasAbove = true;
			entry->perAboveToken = it->second.get<std::string>();
		}
	}
	// the optional §3.7 `scope` -- where the count is taken (json §3.7: defaults to the deposit's own scope, so an
	// unknown token falls back to exactly that); absent stays -1 (= the deposit's own scope).
	if ((it = o.find("scope")) != o.end() && it->second.is<std::string>())
	{
		entry->perScope = (int)jsonParseScope(it->second.get<std::string>(), entry->scope);
	}
	if ((it = o.find("anyOf")) != o.end() && it->second.is<picojson::array>())
	{
		const picojson::array& a = it->second.get<picojson::array>();
		for (size_t i = 0; i < a.size(); ++i)
		{
			if (a[i].is<std::string>())
			{
				const int iId = jsonResolveId(a[i].get<std::string>());
				// keep the TYPE STRING parallel to the id -- the count resolver routes by prefix (an id alone
				// is ambiguous across info kinds); resolved entries only, so the two vectors stay in sync.
				if (iId >= 0)
				{
					entry->perAnyOf.push_back(iId);
					entry->perAnyOfTypes.push_back(a[i].get<std::string>());
				}
			}
		}
	}
}
