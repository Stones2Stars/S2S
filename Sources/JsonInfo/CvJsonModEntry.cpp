//
//	CvJsonModFamily::parseLeaf -- parse a §3.9 modifier-family leaf into owned CvJsonModEntry list, ×100'ing values
//	and reusing the spec-defined condition parser for enabled/disabled. See the header.
//

#include "CvGameCoreDLL.h"          // PCH umbrella -- picojson
#include "CvJsonModEntry.h"
#include "CvJsonConditionParse.h"   // cascadeParseCondition -- the ONE human->condition boundary (spec-defined predicates)
#include "CvJsonParse.h"            // jsonX100 + jsonResolveId + jsonParseScope -- the ×100 boundary, the per
                                    // count-scaler type FK, and the shared §3.2 scope-token vocabulary

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

static CvJsonModEntry* mk(int value100, CvCascUnit unit, CvCascScope scope)
{
	CvJsonModEntry* e = new CvJsonModEntry();
	e->value100 = value100; e->unit = unit; e->scope = scope;
	return e;
}

// The §3.7 `per` count-scaler on a conditioned entry: {type|anyOf, each?} or a bare type string. Shared (declared
// in the header): the grants repeatable `chance.per` parses through this same function -- ONE per parser.
void jsonParsePer(CvJsonModEntry* e, const picojson::value& v)
{
	e->hasPer = true;
	if (v.is<std::string>()) { e->perType = v.get<std::string>(); e->perTypeId = jsonResolveId(e->perType); return; }
	if (!v.is<picojson::object>()) return;
	const picojson::object& o = v.get<picojson::object>();
	picojson::object::const_iterator it;
	if ((it = o.find("type")) != o.end() && it->second.is<std::string>())
	{
		e->perType = it->second.get<std::string>();
		e->perTypeId = jsonResolveId(e->perType);
	}
	if ((it = o.find("each")) != o.end() && it->second.is<double>()) e->perEach = (int)it->second.get<double>();
	// the optional §3.7 `scope` -- where the count is taken (json §3.7: defaults to the deposit's own scope, so an
	// unknown token falls back to exactly that); absent stays -1 (= the deposit's own scope, today's behavior).
	if ((it = o.find("scope")) != o.end() && it->second.is<std::string>())
		e->perScope = (int)jsonParseScope(it->second.get<std::string>(), e->scope);
	if ((it = o.find("anyOf")) != o.end() && it->second.is<picojson::array>())
	{
		const picojson::array& a = it->second.get<picojson::array>();
		for (size_t i = 0; i < a.size(); ++i)
			if (a[i].is<std::string>())
			{
				const int id = jsonResolveId(a[i].get<std::string>());
				// keep the TYPE STRING parallel to the id -- the count resolver routes by prefix (an id alone
				// is ambiguous across info kinds); resolved entries only, so the two vectors stay in sync.
				if (id >= 0) { e->perAnyOf.push_back(id); e->perAnyOfTypes.push_back(a[i].get<std::string>()); }
			}
	}
}

// One `{value, unit?, per?, enabled?, disabled?}` entry object (§3.9; `unit:` = the §3.7 predicate qualifier).
static CvJsonModEntry* parse_entry(const picojson::object& o, CvCascUnit unit, CvCascScope scope)
{
	picojson::object::const_iterator ve = o.find("value");
	if (ve == o.end() || !ve->second.is<double>()) return NULL;   // not an entry object
	CvJsonModEntry* e = mk(jsonX100(ve->second.get<double>()), unit, scope);
	picojson::object::const_iterator it;
	if ((it = o.find("unit")) != o.end())      e->unitQual = cascadeParseCondition(it->second);
	if ((it = o.find("per")) != o.end())       jsonParsePer(e, it->second);
	if ((it = o.find("enabled")) != o.end())   e->enabled  = cascadeParseCondition(it->second);
	if ((it = o.find("disabled")) != o.end())  e->disabled = cascadeParseCondition(it->second);
	return e;
}

void CvJsonModFamily::parseLeaf(const picojson::value& leaf, CvCascUnit unit, CvCascScope scope, const picojson::value* nodeQual)
{
	const size_t iFirst = entries.size();
	if (leaf.is<double>())   // a bare, always-on value
		entries.push_back(mk(jsonX100(leaf.get<double>()), unit, scope));
	else if (leaf.is<picojson::object>())   // a single `{value, unit?, per?, enabled?, disabled?}` entry (§3.9)
	{
		CvJsonModEntry* e = parse_entry(leaf.get<picojson::object>(), unit, scope);
		if (e != NULL) entries.push_back(e);
	}
	else if (leaf.is<picojson::array>())
	{
		const picojson::array& a = leaf.get<picojson::array>();
		for (size_t i = 0; i < a.size(); ++i)
		{
			if (a[i].is<double>())   // a bare number in a list = an always-on entry
			{
				entries.push_back(mk(jsonX100(a[i].get<double>()), unit, scope));
				continue;
			}
			if (!a[i].is<picojson::object>()) continue;
			CvJsonModEntry* e = parse_entry(a[i].get<picojson::object>(), unit, scope);
			if (e != NULL) entries.push_back(e);
		}
	}
	// the NODE-form `unit:` qualifier (json §3.7 -- a sibling of the magnitude leaves, e.g. cargo.space.{unit:
	// IS_AIR, flat: N}): applies to every entry this leaf produced that carries no entry-form qualifier of its own.
	if (nodeQual != NULL)
		for (size_t i = iFirst; i < entries.size(); ++i)
			if (entries[i]->unitQual == NULL) entries[i]->unitQual = cascadeParseCondition(*nodeQual);
}
