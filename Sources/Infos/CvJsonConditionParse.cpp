//
//	cascadeParseCondition -- see the header. A faithful port of StoneBase ConditionParser.cs: the curated JSON ->
//	the typed CvCondition tree, FK-resolving each type/param to its engine id. Mirrors the C# control flow
//	method-for-method (ParseBareString / ParseObject / TryMembership / the string->typed maps).
//

#include "CvGameCoreDLL.h"
#include "CvJsonConditionParse.h"
#include "CvCondition.h"
#include "CvJsonParse.h"        // jsonResolveId (FK diagnostics) + jsonParseScope (the ONE scope-token vocabulary)
#include "Defines/CvEnums.h"    // YieldTypes -- the natureYield atom's channel axis
#include <string>

// ---- the closed string->typed maps (the ONE place a name/scope/connection is recognized) -----------------------

static bool cp_starts(const std::string& s, const char* p) { return s.compare(0, strlen(p), p) == 0; }

static CvCascPredKind cp_predKind(const std::string& s)
{
	if (s == "IS_WATER") return CASC_PRED_IS_WATER;       if (s == "IS_LAND") return CASC_PRED_IS_LAND;
	if (s == "IS_FLATLANDS") return CASC_PRED_IS_FLATLANDS; if (s == "IS_AIR") return CASC_PRED_IS_AIR;
	if (s == "IS_SPACE") return CASC_PRED_IS_SPACE;       if (s == "IS_LUNAR") return CASC_PRED_IS_LUNAR;
	if (s == "IS_MARS") return CASC_PRED_IS_MARS;
	if (s == "HAS_PEAK") return CASC_PRED_HAS_PEAK;       if (s == "HAS_HILLS") return CASC_PRED_HAS_HILLS;
	if (s == "HAS_COAST") return CASC_PRED_HAS_COAST;     if (s == "HAS_RIVER") return CASC_PRED_HAS_RIVER;
	if (s == "HAS_FRESHWATER") return CASC_PRED_HAS_FRESHWATER; if (s == "HAS_IRRIGATION") return CASC_PRED_HAS_IRRIGATION;
	if (s == "HAS_FEATURE") return CASC_PRED_HAS_FEATURE; if (s == "HAS_LANDMARK") return CASC_PRED_HAS_LANDMARK;
	if (s == "VICINITY") return CASC_PRED_VICINITY;       if (s == "WORKABLE") return CASC_PRED_WORKABLE;
	if (s == "IS_WORKED") return CASC_PRED_IS_WORKED;     if (s == "NO_NUKES") return CASC_PRED_NO_NUKES;
	if (s == "IS_CAPITAL") return CASC_PRED_IS_CAPITAL;   if (s == "IS_GOVERNMENT_CENTER") return CASC_PRED_IS_GOVERNMENT_CENTER;
	if (s == "HAS_POWER") return CASC_PRED_HAS_POWER;     if (s == "HAS_STATE_RELIGION") return CASC_PRED_HAS_STATE_RELIGION;
	if (s == "STATE_RELIGION_IN_CITY") return CASC_PRED_STATE_RELIGION_IN_CITY; if (s == "IS_GOLDEN_AGE") return CASC_PRED_IS_GOLDEN_AGE;
	if (s == "IS_REBEL") return CASC_PRED_IS_REBEL;
	if (s == "IS_ANARCHY") return CASC_PRED_IS_ANARCHY;   if (s == "IS_OWNED") return CASC_PRED_IS_OWNED;
	if (s == "IS_HOLY_CITY") return CASC_PRED_IS_HOLY_CITY; if (s == "IS_STATE_RELIGION_HOLY_CITY") return CASC_PRED_IS_STATE_RELIGION_HOLY_CITY;
	if (s == "IS_STATE_RELIGION") return CASC_PRED_IS_STATE_RELIGION;   // the counted-religion test (ruling 23)
	if (s == "IS_HEADQUARTERS") return CASC_PRED_IS_HEADQUARTERS;   // corp HQ city (ruling 10; bare = any corp's HQ)
	return CASC_PRED_UNKNOWN;
}

const char* cascadeSpellPredKind(CvCascPredKind ePredKind)
{
	// The reverse of cp_predKind above -- one row per recognized spelling, same order, so a vocabulary edit
	// touches the two tables side by side. The parameterized-only kinds spell their key form.
	switch (ePredKind)
	{
	case CASC_PRED_IS_WATER:                   return "IS_WATER";
	case CASC_PRED_IS_LAND:                    return "IS_LAND";
	case CASC_PRED_IS_FLATLANDS:               return "IS_FLATLANDS";
	case CASC_PRED_IS_AIR:                     return "IS_AIR";
	case CASC_PRED_IS_SPACE:                   return "IS_SPACE";
	case CASC_PRED_IS_LUNAR:                   return "IS_LUNAR";
	case CASC_PRED_IS_MARS:                    return "IS_MARS";
	case CASC_PRED_HAS_PEAK:                   return "HAS_PEAK";
	case CASC_PRED_HAS_HILLS:                  return "HAS_HILLS";
	case CASC_PRED_HAS_COAST:                  return "HAS_COAST";
	case CASC_PRED_HAS_RIVER:                  return "HAS_RIVER";
	case CASC_PRED_HAS_FRESHWATER:             return "HAS_FRESHWATER";
	case CASC_PRED_HAS_IRRIGATION:             return "HAS_IRRIGATION";
	case CASC_PRED_HAS_FEATURE:                return "HAS_FEATURE";
	case CASC_PRED_HAS_LANDMARK:               return "HAS_LANDMARK";
	case CASC_PRED_VICINITY:                   return "VICINITY";
	case CASC_PRED_WORKABLE:                   return "WORKABLE";
	case CASC_PRED_IS_WORKED:                  return "IS_WORKED";
	case CASC_PRED_NO_NUKES:                   return "NO_NUKES";
	case CASC_PRED_IS_CAPITAL:                 return "IS_CAPITAL";
	case CASC_PRED_IS_GOVERNMENT_CENTER:       return "IS_GOVERNMENT_CENTER";
	case CASC_PRED_HAS_POWER:                  return "HAS_POWER";
	case CASC_PRED_HAS_STATE_RELIGION:         return "HAS_STATE_RELIGION";
	case CASC_PRED_STATE_RELIGION_IN_CITY:     return "STATE_RELIGION_IN_CITY";
	case CASC_PRED_IS_GOLDEN_AGE:              return "IS_GOLDEN_AGE";
	case CASC_PRED_IS_REBEL:                   return "IS_REBEL";
	case CASC_PRED_IS_ANARCHY:                 return "IS_ANARCHY";
	case CASC_PRED_IS_OWNED:                   return "IS_OWNED";
	case CASC_PRED_IS_HOLY_CITY:               return "IS_HOLY_CITY";
	case CASC_PRED_IS_STATE_RELIGION_HOLY_CITY: return "IS_STATE_RELIGION_HOLY_CITY";
	case CASC_PRED_IS_STATE_RELIGION:          return "IS_STATE_RELIGION";
	case CASC_PRED_HAS_TERRAIN:                return "HAS_TERRAIN";
	case CASC_PRED_HAS_IMPROVEMENT:            return "HAS_IMPROVEMENT";
	case CASC_PRED_HAS_BONUS:                  return "HAS_BONUS";
	case CASC_PRED_HAS_RELIGION:               return "HAS_RELIGION";
	case CASC_PRED_STATE_RELIGION:             return "STATE_RELIGION";
	case CASC_PRED_HAS_CORPORATION:            return "HAS_CORPORATION";
	case CASC_PRED_IS_HEADQUARTERS:            return "IS_HEADQUARTERS";
	case CASC_PRED_LATITUDE:                   return "latitude";
	case CASC_PRED_EXISTED_FOR:                return "existedFor";
	case CASC_PRED_NATURE_YIELD:               return "natureYield";
	case CASC_PRED_IS_TAG:                     return "IS_TAG";
	default:                                   return "";
	}
}

// Scope IMPLIED from the type's domain (json §3.4): TECH->team, civic/heritage->empire, else->city.
static CvCascScope cp_impliedScope(const std::string& t)
{
	if (cp_starts(t, "TECH_")) return CASC_SCOPE_TEAM;
	if (cp_starts(t, "CIVIC_") || cp_starts(t, "HERITAGE_")) return CASC_SCOPE_EMPIRE;
	return CASC_SCOPE_CITY;
}

static CvCascConnection cp_parseConnection(const std::string& c)
{
	if (c == "trade") return CASC_CONN_TRADE;
	if (c == "vicinity") return CASC_CONN_VICINITY;
	if (c.find("trade") != std::string::npos && c.find("vicinity") != std::string::npos) return CASC_CONN_TRADE_OR_VICINITY;
	return CASC_CONN_NONE;
}

static CvCascVicinity cp_parseVicinity(const std::string& v)
{
	if (v == "owned") return CASC_VIC_OWNED;   if (v == "worked") return CASC_VIC_WORKED;
	if (v == "connected") return CASC_VIC_CONNECTED; if (v == "crossBorder") return CASC_VIC_CROSSBORDER;
	return CASC_VIC_NONE;
}

static bool cp_isTypeRef(const std::string& n)
{
	return cp_starts(n, "TECH_") || cp_starts(n, "CIVIC_") || cp_starts(n, "TRAIT_") || cp_starts(n, "RELIGION_")
	    || cp_starts(n, "HERITAGE_") || cp_starts(n, "PROJECT_") || cp_starts(n, "BUILDING_") || cp_starts(n, "CORPORATION_")
	    || cp_starts(n, "BONUS_") || cp_starts(n, "MAPCATEGORY_") || cp_starts(n, "FEATURE_") || cp_starts(n, "TERRAIN_")
	    || cp_starts(n, "IMPROVEMENT_") || cp_starts(n, "ROUTE_") || cp_starts(n, "VICTORY_") || cp_starts(n, "UNIT_")
	    || cp_starts(n, "GAMEOPTION_") || cp_starts(n, "PROPERTY_") || cp_starts(n, "PROMOTION_");
}

// ---- node builders (FK resolution via jsonResolveId -- unresolved ids land in the load-time diagnostics) --------

static CvCondition* cp_presence(const std::string& type, CvCascScope scope, int min, int max,
                                       CvCascConnection conn, CvCascVicinity vic)
{
	CvCondition* c = new CvCondition();
	c->kind = CASC_COND_PRESENCE; c->type = type; c->scope = scope; c->min = min; c->max = max;
	c->connection = conn; c->vicinity = vic; c->id = jsonResolveId(type);
	return c;
}
static CvCondition* cp_predicate(CvCascPredKind k, const std::string& param, int min, int max)
{
	CvCondition* c = new CvCondition();
	c->kind = CASC_COND_PREDICATE; c->predKind = k; c->param = param; c->min = min; c->max = max;
	if (!param.empty()) c->id = jsonResolveId(param);
	return c;
}
static CvCondition* cp_group() { CvCondition* c = new CvCondition(); c->kind = CASC_COND_GROUP; return c; }

// ---- picojson helpers ------------------------------------------------------------------------------------------

static bool po_has(const picojson::object& o, const char* k) { return o.find(k) != o.end(); }
static const picojson::value* po_get(const picojson::object& o, const char* k)
{ picojson::object::const_iterator it = o.find(k); return it == o.end() ? NULL : &it->second; }
static std::string po_str(const picojson::object& o, const char* k)
{ const picojson::value* v = po_get(o, k); return (v && v->is<std::string>()) ? v->get<std::string>() : std::string(); }
static int po_int(const picojson::object& o, const char* k, int dflt)
{ const picojson::value* v = po_get(o, k); return (v && v->is<double>()) ? (int)v->get<double>() : dflt; }

// ---- the parse (ConditionParser.cs) ----------------------------------------------------------------------------

static CvCondition* cp_parseBareString(const std::string& s);

static CvCondition* cp_parseObject(const picojson::object& o)
{
	// 1) combinator node
	if (po_has(o, "all") || po_has(o, "any") || po_has(o, "noneOf") || po_has(o, "enabled") || po_has(o, "disabled"))
	{
		CvCondition* g = cp_group();
		const char* lists[3] = { "all", "any", "noneOf" };
		std::vector<CvCondition*>* dst[3] = { &g->all, &g->anyOf, &g->noneOf };
		for (int i = 0; i < 3; ++i)
		{
			const picojson::value* v = po_get(o, lists[i]);
			if (v && v->is<picojson::array>())
			{
				const picojson::array& a = v->get<picojson::array>();
				for (size_t j = 0; j < a.size(); ++j) { CvCondition* c = cascadeParseCondition(a[j]); if (c) dst[i]->push_back(c); }
			}
		}
		const picojson::value* en = po_get(o, "enabled");  if (en) g->enabled = cascadeParseCondition(*en);
		const picojson::value* di = po_get(o, "disabled"); if (di) g->disabled = cascadeParseCondition(*di);
		return g;
	}

	// 2) presence / count atom: {type, scope?, min?, max?, connection?, vicinity?}
	{
		const picojson::value* ty = po_get(o, "type");
		if (ty && ty->is<std::string>())
		{
			const std::string type = ty->get<std::string>();
			const std::string sc = po_str(o, "scope");
			CvCondition* pAtom = cp_presence(type, sc.empty() ? cp_impliedScope(type) : jsonParseScope(sc, CASC_SCOPE_EMPIRE),
			                   po_int(o, "min", -1), po_int(o, "max", -1),
			                   cp_parseConnection(po_str(o, "connection")), cp_parseVicinity(po_str(o, "vicinity")));
			// A bound is AUTHORED iff its key is present. ⛔ Never inferred from the value's sign: a PROPERTY_
			// band bound is legitimately negative (CvCondition.h), so a sign test drops it and the clause
			// collapses to always-true.
			pAtom->hasMin = (po_get(o, "min") != NULL);
			pAtom->hasMax = (po_get(o, "max") != NULL);
			return pAtom;
		}
	}

	// 3) membership sugar: {terrain|feature|improvement|bonus: [TYPE,…]} == an Any of the matching predicate (§3.5)
	{
		const char* memKeys[4] = { "bonus", "terrain", "feature", "improvement" };
		for (int m = 0; m < 4; ++m)
		{
			const picojson::value* arr = po_get(o, memKeys[m]);
			if (arr && arr->is<picojson::array>())
			{
				const picojson::array& a = arr->get<picojson::array>();
				CvCondition* g = cp_group();
				for (size_t j = 0; j < a.size(); ++j)
				{
					if (!a[j].is<std::string>()) continue;
					const std::string t = a[j].get<std::string>();
					if (m == 0) g->anyOf.push_back(cp_presence(t, CASC_SCOPE_CITY, 1, -1, cp_parseConnection(po_str(o, "connection")), cp_parseVicinity(po_str(o, "vicinity"))));
					else if (m == 1) g->anyOf.push_back(cp_predicate(CASC_PRED_HAS_TERRAIN, t, -1, -1));
					else if (m == 2) g->anyOf.push_back(cp_predicate(CASC_PRED_HAS_FEATURE, t, -1, -1));
					else g->anyOf.push_back(cp_predicate(CASC_PRED_HAS_IMPROVEMENT, t, -1, -1));
				}
				return g;
			}
		}
	}

	// 4) numeric-parameterized predicates
	{
		const picojson::value* lat = po_get(o, "latitude");
		if (lat && lat->is<picojson::object>()) { const picojson::object& l = lat->get<picojson::object>(); return cp_predicate(CASC_PRED_LATITUDE, "", po_int(l, "min", -1), po_int(l, "max", -1)); }
		const picojson::value* ef = po_get(o, "existedFor");
		if (ef && ef->is<picojson::object>()) { const picojson::object& l = ef->get<picojson::object>(); return cp_predicate(CASC_PRED_EXISTED_FOR, "", po_int(l, "min", -1), -1); }
		const picojson::value* hc = po_get(o, "HAS_COAST");
		if (hc && hc->is<picojson::object>()) { const picojson::object& l = hc->get<picojson::object>(); return cp_predicate(CASC_PRED_HAS_COAST, "", po_int(l, "minArea", -1), -1); }
		// {natureYield:{food:N,...}} -- the improvement PLACEMENT threshold atom (requires.build.all): the plot's
		// PRE-improvement nature yield per channel must be >= N (the CvPlot::canHaveImprovement gate). One
		// predicate node per authored channel (a multi-channel object ANDs as a group); the YieldTypes channel
		// rides `id` (param stays empty -- no Type to FK-route), the threshold rides `min`.
		const picojson::value* ny = po_get(o, "natureYield");
		if (ny && ny->is<picojson::object>())
		{
			const picojson::object& l = ny->get<picojson::object>();
			const char* aszChannels[3] = { "food", "production", "commerce" };
			const int aiYields[3] = { (int)YIELD_FOOD, (int)YIELD_PRODUCTION, (int)YIELD_COMMERCE };
			CvCondition* pGroup = 0;
			CvCondition* pSingle = 0;
			for (int iChannel = 0; iChannel < 3; ++iChannel)
			{
				const int iThreshold = po_int(l, aszChannels[iChannel], -1);
				if (iThreshold < 0)
				{
					continue;
				}
				CvCondition* pNode = cp_predicate(CASC_PRED_NATURE_YIELD, "", iThreshold, -1);
				pNode->id = aiYields[iChannel];
				if (pSingle == 0 && pGroup == 0)
				{
					pSingle = pNode;
					continue;
				}
				if (pGroup == 0)
				{
					pGroup = cp_group();
					pGroup->all.push_back(pSingle);
					pSingle = 0;
				}
				pGroup->all.push_back(pNode);
			}
			if (pGroup != 0) { return pGroup; }
			if (pSingle != 0) { return pSingle; }
			return cp_predicate(CASC_PRED_UNKNOWN, "", -1, -1);   // an empty/malformed natureYield object
		}
	}

	// 5) single-key type-parameterized predicate: {HAS_BONUS:X} / {HAS_TERRAIN:X} / {IS_HOLY_CITY:RELIGION_X} / …
	for (picojson::object::const_iterator it = o.begin(); it != o.end(); ++it)
	{
		const std::string& k = it->first;
		if (k == "scope" || k == "per" || k == "value" || k == "ai" || k == "connection" || k == "vicinity"
		 || k == "each" || k == "min" || k == "max") continue;   // entry modifiers, not a predicate key
		const std::string pv = it->second.is<std::string>() ? it->second.get<std::string>() : std::string();
		if (k == "HAS_BONUS")
			return cp_presence(pv, CASC_SCOPE_CITY, 1, -1, cp_parseConnection(po_str(o, "connection")), cp_parseVicinity(po_str(o, "vicinity")));
		if (k == "HAS_TERRAIN")     return cp_predicate(CASC_PRED_HAS_TERRAIN, pv, -1, -1);
		if (k == "HAS_FEATURE")     return cp_predicate(CASC_PRED_HAS_FEATURE, pv, -1, -1);
		if (k == "HAS_IMPROVEMENT") return cp_predicate(CASC_PRED_HAS_IMPROVEMENT, pv, -1, -1);
		if (k == "HAS_RELIGION")    return cp_predicate(CASC_PRED_HAS_RELIGION, pv, -1, -1);
		if (k == "HAS_CORPORATION") return cp_predicate(CASC_PRED_HAS_CORPORATION, pv, -1, -1);
		if (k == "IS_HOLY_CITY")    return cp_predicate(CASC_PRED_IS_HOLY_CITY, pv, -1, -1);
		if (k == "IS_HEADQUARTERS") return cp_predicate(CASC_PRED_IS_HEADQUARTERS, pv, -1, -1);
		if (k == "STATE_RELIGION")  return cp_predicate(CASC_PRED_STATE_RELIGION, pv, -1, -1);
		const CvCascPredKind bk = cp_predKind(k);
		if (bk != CASC_PRED_UNKNOWN) return cp_predicate(bk, "", -1, -1);   // bare-kind keyed object
	}
	return cp_predicate(CASC_PRED_UNKNOWN, "", -1, -1);
}

static CvCondition* cp_parseBareString(const std::string& s)
{
	if (s.size() > 1 && s[0] == '!')
	{
		std::string inner = s.substr(1);
		while (!inner.empty() && (inner[0] == ' ' || inner[0] == '\t')) inner.erase(0, 1);
		CvCondition* g = cp_group();
		g->noneOf.push_back(cp_parseBareString(inner));
		return g;
	}
	const CvCascPredKind k = cp_predKind(s);
	if (k != CASC_PRED_UNKNOWN) return cp_predicate(k, "", -1, -1);
	if (cp_isTypeRef(s)) return cp_presence(s, cp_impliedScope(s), 1, -1, CASC_CONN_NONE, CASC_VIC_NONE);
	// an unrecognized IS_<SUFFIX> is a classification-TAG test against a unit target (json §8): IS_MILITARY ->
	// the `military` tag. Store the full TAG_<SUFFIX> type name in `param`; eval resolves the id lazily (the TAG_*
	// infotypes are minted AFTER condition parse, so it cannot FK-resolve here).
	if (cp_starts(s, "IS_")) return cp_predicate(CASC_PRED_IS_TAG, "TAG_" + s.substr(3), -1, -1);
	return cp_predicate(CASC_PRED_UNKNOWN, "", -1, -1);   // unknown -> IGNORED at eval (§3.5)
}

CvCondition* cascadeParseCondition(const picojson::value& v)
{
	if (v.is<std::string>()) return cp_parseBareString(v.get<std::string>());
	if (v.is<picojson::object>()) return cp_parseObject(v.get<picojson::object>());
	return cp_predicate(CASC_PRED_UNKNOWN, "", -1, -1);
}
