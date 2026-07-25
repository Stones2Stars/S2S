//
//	CvTriggers -- see the header. The parse is grounded in the authored data (the `triggers` arrays across
//	Assets/Data; every shape below is authored, verified against the live corpus):
//	  { "trigger": "onTurnEnd", "action": { "promote": { "promotions": ["PROMOTION_X"], "units": "present" } } }
//	  { "trigger": "onTurn",    "action": { "heal": 5, "unitCombat": "UNITCOMBAT_X" } }
//	  { "trigger": "onTurn",    "action": { "heal": "full", "count": 1 } }
//	  { "trigger": "onTurn",    "action": { "PROPERTY_X": -3, "on": "plot", "relation": "near", "distance": 1 } }
//	  { "trigger": "onTurn",    "chance": { "per": "PROPERTY_CRIME" }, "action": { "grant": { "units": [...] } } }
//	plus the spec'd trigger forms not yet authored (json.md §5): the {"on<Happening>": N} every-N-turns token
//	and a §3 state-condition trigger (the fire-band exemplar). Chance is a number or {value?, per?} -- the
//	authored per-only form carries no `value`. An unrecognized entry or action key surfaces via the unconsumed
//	census, never silently drops.
//

#include "CvGameCoreDLL.h"          // PCH umbrella -- picojson
#include "CvTriggers.h"
#include "CvGrants.h"           // the nested action.grant payload (the §5 vocabulary whole)
#include "CvJsonConditionParse.h"   // cascadeParseCondition -- the ONE condition boundary
#include "CvModEntry.h"         // jsonParsePer -- the ONE §3.7 `per` parser (chance.per)
#include "CvJsonParse.h"            // jsonResolveId / jsonX100 / jsonNoteUnconsumed
#include <ctype.h>

CvTriggerEntry::CvTriggerEntry()
	: happeningInterval(1), condition(NULL),
	  chanceValue100(0), chancePerTypeId(-1), chancePerEach(1), chancePerScope(-1),
	  grant(NULL),
	  heal100(0), healFull(false), healUnitCombatId(-1), healCount(0),
	  propertyId(-1), propertyAmount100(0), spatialDistance(0)
{
}

CvTriggerEntry::~CvTriggerEntry()
{
	delete condition;
	delete grant;
}

CvTriggers::~CvTriggers()
{
	clearParsed();
}

void CvTriggers::clearParsed()
{
	for (size_t i = 0; i < m_entries.size(); ++i)
	{
		delete m_entries[i];
	}
	m_entries.clear();
}

// An on-token is a spine happening in authoring form: "on" + a capitalized happening name (json.md §5).
static bool triggersIsOnToken(const std::string& szKey)
{
	return szKey.size() > 2 && szKey.compare(0, 2, "on") == 0 && isupper((unsigned char)szKey[2]) != 0;
}

// `trigger` -- the WHEN/WHY: a bare on-token string, an {"on<Happening>": N} interval token, or a §3 state
// condition (a bare predicate string / a condition object), per json.md §5.
static void triggersParseTrigger(CvTriggerEntry* pEntry, const picojson::value& v)
{
	if (v.is<std::string>())
	{
		const std::string szToken = v.get<std::string>();
		if (triggersIsOnToken(szToken))
		{
			pEntry->happening = szToken;
			return;
		}
		pEntry->condition = cascadeParseCondition(v);   // a bare §3 predicate string
		return;
	}
	if (v.is<picojson::object>())
	{
		const picojson::object& o = v.get<picojson::object>();
		if (o.size() == 1 && triggersIsOnToken(o.begin()->first) && o.begin()->second.is<double>())
		{
			pEntry->happening = o.begin()->first;
			const int iInterval = (int)o.begin()->second.get<double>();
			pEntry->happeningInterval = (iInterval > 1) ? iInterval : 1;
			return;
		}
		pEntry->condition = cascadeParseCondition(v);   // a §3 state-condition tree (atoms/predicates/combinators)
	}
}

// `chance` -- the odds, always on the trigger: a bare number, or {value?, per?} (the authored per-only form
// carries no value). The per parses through the ONE §3.7 parser and its fields copy onto the entry.
static void triggersParseChance(CvTriggerEntry* pEntry, const picojson::value& v)
{
	if (v.is<double>())
	{
		pEntry->chanceValue100 = jsonX100(v.get<double>());
		return;
	}
	if (!v.is<picojson::object>())
	{
		return;
	}
	const picojson::object& o = v.get<picojson::object>();
	picojson::object::const_iterator valueIt = o.find("value");
	if (valueIt != o.end() && valueIt->second.is<double>())
	{
		pEntry->chanceValue100 = jsonX100(valueIt->second.get<double>());
	}
	picojson::object::const_iterator perIt = o.find("per");
	if (perIt != o.end())
	{
		CvModEntry perEntry;
		jsonParsePer(&perEntry, perIt->second);
		pEntry->chancePerTypeId = perEntry.perTypeId;
		if (perEntry.perTypeId < 0)
		{
			pEntry->chancePerToken = perEntry.perType;   // a catch-all token survives (carry-only)
		}
		pEntry->chancePerEach = perEntry.perEach;
		pEntry->chancePerScope = perEntry.perScope;
		pEntry->chancePerAnyOf = perEntry.perAnyOf;
	}
}

// `action` -- the verb object (an OPEN registry; json.md §5). The authored verbs parse to typed fields; an
// unrecognized verb key is recorded, never silently dropped.
static void triggersParseAction(CvTriggerEntry* pEntry, const picojson::value& v)
{
	if (!v.is<picojson::object>())
	{
		return;
	}
	const picojson::object& o = v.get<picojson::object>();
	for (picojson::object::const_iterator it = o.begin(); it != o.end(); ++it)
	{
		const std::string& szKey = it->first;
		const picojson::value& val = it->second;
		if (szKey == "promote" && val.is<picojson::object>())
		{
			const picojson::object& promoteObj = val.get<picojson::object>();
			picojson::object::const_iterator promosIt = promoteObj.find("promotions");
			if (promosIt != promoteObj.end() && promosIt->second.is<picojson::array>())
			{
				const picojson::array& promos = promosIt->second.get<picojson::array>();
				for (size_t i = 0; i < promos.size(); ++i)
				{
					if (!promos[i].is<std::string>())
					{
						continue;
					}
					const int iPromotion = jsonResolveId(promos[i].get<std::string>());
					if (iPromotion >= 0)
					{
						pEntry->promotePromotions.push_back(iPromotion);
					}
				}
			}
			picojson::object::const_iterator unitsIt = promoteObj.find("units");
			if (unitsIt != promoteObj.end() && unitsIt->second.is<std::string>())
			{
				pEntry->promoteUnits = unitsIt->second.get<std::string>();
			}
		}
		else if (szKey == "grant")
		{
			delete pEntry->grant;
			pEntry->grant = new CvGrants();
			pEntry->grant->parse(val);   // the §5 payload vocabulary nested whole
		}
		else if (szKey == "heal")
		{
			if (val.is<std::string>() && val.get<std::string>() == "full")
			{
				pEntry->healFull = true;
			}
			else if (val.is<double>())
			{
				pEntry->heal100 = jsonX100(val.get<double>());
			}
		}
		else if (szKey == "unitCombat" && val.is<std::string>())
		{
			pEntry->healUnitCombatId = jsonResolveId(val.get<std::string>());
		}
		else if (szKey == "count" && val.is<double>())
		{
			pEntry->healCount = (int)val.get<double>();
		}
		else if (szKey == "on" && val.is<std::string>())
		{
			pEntry->spatialOn = val.get<std::string>();
		}
		else if (szKey == "relation" && val.is<std::string>())
		{
			pEntry->spatialRelation = val.get<std::string>();
		}
		else if (szKey == "distance" && val.is<double>())
		{
			pEntry->spatialDistance = (int)val.get<double>();
		}
		else if (szKey.compare(0, 9, "PROPERTY_") == 0 && val.is<double>())
		{
			pEntry->propertyId = jsonResolveId(szKey);
			pEntry->propertyAmount100 = jsonX100(val.get<double>());
		}
		else
		{
			jsonNoteUnconsumed("triggers.action", szKey);   // an unknown verb -> the census, never silent
		}
	}
}

void CvTriggers::parse(const picojson::value& v)
{
	if (!v.is<picojson::array>())
	{
		return;
	}
	const picojson::array& entries = v.get<picojson::array>();
	for (size_t i = 0; i < entries.size(); ++i)
	{
		if (!entries[i].is<picojson::object>())
		{
			continue;
		}
		const picojson::object& entryObj = entries[i].get<picojson::object>();
		CvTriggerEntry* pEntry = new CvTriggerEntry();
		for (picojson::object::const_iterator it = entryObj.begin(); it != entryObj.end(); ++it)
		{
			if (it->first == "trigger")
			{
				triggersParseTrigger(pEntry, it->second);
			}
			else if (it->first == "chance")
			{
				triggersParseChance(pEntry, it->second);
			}
			else if (it->first == "action")
			{
				triggersParseAction(pEntry, it->second);
			}
			else
			{
				jsonNoteUnconsumed("triggers", it->first);   // an unknown entry key -> the census, never silent
			}
		}
		m_entries.push_back(pEntry);
	}
}
