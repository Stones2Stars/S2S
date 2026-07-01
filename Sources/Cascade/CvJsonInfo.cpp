//
//	CvJsonInfo -- see the header. Holds the base's out-of-line bodies: the dtor/clear (frees the owned condition trees)
//	AND CvJsonInfo::mapFrom -- the COMMON-section parse every entity shares ("the info loads itself"; audit 2026-07-01).
//	The per-type subclasses override mapFrom to call this base first, then parse their ONE extension block. The generic
//	section walkers below are file-static (the base's private implementation); the shared leaf primitives (×100, FK
//	resolve, bool-set, commerce-map) come from CvCascadeJsonParse so no walker is re-hand-rolled per type.
//

#include "CvGameCoreDLL.h"              // PCH umbrella -- picojson
#include "CvJsonInfo.h"
#include "CvCascadeJsonParse.h"         // the shared leaf primitives (cascadeJsonX100 / ...ResolveId)
#include "CvCascadeConditionParse.h"    // cascadeParseCondition -- curated JSON -> typed CvCascadeCondition tree

CvJsonInfo::~CvJsonInfo()
{
	clear();
}

void CvJsonInfo::clear()
{
	for (size_t i = 0; i < deposits.size(); ++i)
	{
		delete deposits[i].enabled;
		delete deposits[i].disabled;
	}
	deposits.clear();
	delete requiresBuild;   requiresBuild = NULL;
	delete requiresOperate; requiresOperate = NULL;
	edges.clear();
	allowed.clear();
	dormantTriggers.clear();
	grantLists.clear();
	grantPulses.clear();
	grantFlags.clear();
	grantScopedPulses.clear();
	grantRepeatables.clear();
}

// The synthetic TECH_GAME_START root (see the header): a single process-static CvJsonInfo, off the InfoRepo (it has no
// engine id). readJson maps TECH_GAME_START's enables into it; the enabler seeds GENERATE from it for every player.
CvJsonInfo& cascadeStartNode()
{
	static CvJsonInfo s_startNode;
	return s_startNode;
}

// ===================== the base's COMMON-section walkers (file-static; json.md) =====================
// The top-level key classification (which key is an edge / family / intrinsic / …) lives ONCE in CvCascadeJsonParse
// (cascadeJsonClassifyKey) -- shared with the reader's census, so there is no duplicate vocabulary table to collide
// under unity batching. Only the magnitude-UNITS table is local here (nothing else uses it).
// The magnitude UNITS (json.md §3.6) -- only these become deposit leaves; every other child key is a deeper address segment.
static const char* RJ_MOD_UNITS[] = { "flat", "percent", "multiplier", "postMultiplier", "rawPercent",
	"perPopulation", "perSpecialist", "perCorporationLevel", 0 };

// A magnitude leaf: a bare number, an entry object {value, scope?, enabled?, disabled?, per?, ai?}, or a LIST of either
// (json.md §3.9). Each leaf does the single human->×100 conversion; enabled/disabled parse to the typed condition tree.
static void rj_parseMag(CvJsonInfo* pData, const std::string& unit, const picojson::value& v, const std::string& path)
{
	if (v.is<picojson::array>())
	{
		const picojson::array& a = v.get<picojson::array>();
		for (size_t i = 0; i < a.size(); ++i) rj_parseMag(pData, unit, a[i], path);
		return;
	}
	int value100 = 0;
	bool perScaled = false;
	std::vector<int> perAnyOf;   // per:{anyOf:[...]} resolved type ids (corp CommercesProduced prereq-bonus scaler, etc.)
	CvCascadeCondition* en = NULL;
	CvCascadeCondition* dis = NULL;
	if (v.is<double>())
	{
		value100 = cascadeJsonX100(v.get<double>());
	}
	else if (v.is<picojson::object>())
	{
		const picojson::object& o = v.get<picojson::object>();
		picojson::object::const_iterator it;
		if ((it = o.find("value")) != o.end() && it->second.is<double>()) value100 = cascadeJsonX100(it->second.get<double>());
		if ((it = o.find("enabled")) != o.end())  en = cascadeParseCondition(it->second);
		if ((it = o.find("disabled")) != o.end()) dis = cascadeParseCondition(it->second);
		picojson::object::const_iterator pit = o.find("per");
		if (pit != o.end())
		{
			perScaled = true;
			if (pit->second.is<picojson::object>())   // per:{anyOf:[...], scope?} -- resolve the anyOf type ids
			{
				const picojson::object& po = pit->second.get<picojson::object>();
				picojson::object::const_iterator ao = po.find("anyOf");
				if (ao != po.end() && ao->second.is<picojson::array>())
				{
					const picojson::array& aa = ao->second.get<picojson::array>();
					for (size_t ai = 0; ai < aa.size(); ++ai)
						if (aa[ai].is<std::string>())
						{
							const int bid = cascadeJsonResolveId(aa[ai].get<std::string>());
							if (bid >= 0) perAnyOf.push_back(bid);
						}
				}
			}
		}
	}
	else return;

	CvCascadeDeposit d;
	d.address = path; d.unit = unit; d.value100 = value100;
	d.enabled = en; d.disabled = dis; d.hasPer = perScaled; d.perAnyOf = perAnyOf;
	pData->deposits.push_back(d);
}

// Walk a modifier-family node: a unit-keyed child is a magnitude leaf; every other child is a deeper address segment; a
// bare number is a count leaf (allowedSpecialists.<scope>.SPECIALIST_X: N); an array is a conditioned count-leaf list.
static void rj_walkModNode(CvJsonInfo* pData, const std::string& path, const picojson::value& v)
{
	if (v.is<double>())   // a bare count leaf
	{
		CvCascadeDeposit d; d.address = path; d.unit = "count"; d.value100 = cascadeJsonX100(v.get<double>());
		pData->deposits.push_back(d);
		return;
	}
	if (v.is<picojson::array>()) { rj_parseMag(pData, "count", v, path); return; }   // a conditioned count-leaf list
	if (!v.is<picojson::object>()) return;
	const picojson::object& o = v.get<picojson::object>();
	for (picojson::object::const_iterator it = o.begin(); it != o.end(); ++it)
	{
		if (cascadeJsonInList(RJ_MOD_UNITS, it->first)) rj_parseMag(pData, it->first, it->second, path);   // unit leaf: address = parent path
		else rj_walkModNode(pData, path + "." + it->first, it->second);                                    // descend, extend the address
	}
}

// An enables-family edge (enables/obsoletes/replaces/disables/obsoletedBy/provides): per-kind id buckets (json.md §4.1).
static void rj_walkEnableEdge(CvJsonInfo* pData, const std::string& edge, const picojson::value& v)
{
	if (!v.is<picojson::object>()) return;
	const picojson::object& o = v.get<picojson::object>();
	for (picojson::object::const_iterator it = o.begin(); it != o.end(); ++it)
	{
		if (!it->second.is<picojson::array>()) continue;
		const picojson::array& a = it->second.get<picojson::array>();
		for (size_t i = 0; i < a.size(); ++i)
		{
			if (!a[i].is<std::string>()) continue;
			const int rid = cascadeJsonResolveId(a[i].get<std::string>());
			if (rid >= 0) pData->edges[edge + "." + it->first].push_back(rid);
		}
	}
}

// `allowed` (json.md §4.4): a scope key (world/team/empire self-cap) or a wonder-category key -> a number.
static void rj_walkAllowed(CvJsonInfo* pData, const picojson::value& v)
{
	if (!v.is<picojson::object>()) return;
	const picojson::object& o = v.get<picojson::object>();
	for (picojson::object::const_iterator it = o.begin(); it != o.end(); ++it)
		if (it->second.is<double>()) pData->allowed[it->first] = (int)it->second.get<double>();
}

// `interval`: "perTurn" (or unrecognized) -> 1 turn; { perTurn: N } -> N turns.
static int rj_interval(const picojson::value& v)
{
	if (v.is<picojson::object>())
	{
		const picojson::object& o = v.get<picojson::object>();
		picojson::object::const_iterator it = o.find("perTurn");
		if (it != o.end() && it->second.is<double>()) return (int)it->second.get<double>();
	}
	return 1;
}
// `chance`: { per: <type-string> } -> the scaler type id (e.g. PROPERTY_CRIME); -1 = unconditional / unrecognized.
static int rj_chancePer(const picojson::value& v)
{
	const picojson::object& o = v.get<picojson::object>();
	picojson::object::const_iterator it = o.find("per");
	if (it != o.end() && it->second.is<std::string>()) return cascadeJsonResolveId(it->second.get<std::string>());
	return -1;
}
// A structured `grants.repeatable` array (json.md §5): parse each entry into a CvCascadeGrantRepeatable -- the payload
// (unit spawn / unitCombat heal / PROPERTY_* pulse), the interval, the chance-per scaler, and the #429 spatial fields.
static void rj_parseRepeatable(CvJsonInfo* pData, const picojson::array& a)
{
	for (size_t i = 0; i < a.size(); ++i)
	{
		if (!a[i].is<picojson::object>()) continue;
		const picojson::object& eo = a[i].get<picojson::object>();
		CvCascadeGrantRepeatable r;
		for (picojson::object::const_iterator it = eo.begin(); it != eo.end(); ++it)
		{
			const std::string& key = it->first;
			const picojson::value& v = it->second;
			if (key == "unit"       && v.is<std::string>()) r.unitId = cascadeJsonResolveId(v.get<std::string>());
			else if (key == "unitCombat" && v.is<std::string>()) r.unitCombatId = cascadeJsonResolveId(v.get<std::string>());
			else if (key == "heal") { if (v.is<std::string>()) r.healFull = true; else if (v.is<double>()) r.heal = cascadeJsonX100(v.get<double>()); }
			else if (key == "count"    && v.is<double>())   r.count = (int)v.get<double>();
			else if (key == "interval")                     r.intervalTurns = rj_interval(v);
			else if (key == "chance"   && v.is<picojson::object>()) r.chancePerId = rj_chancePer(v);
			else if (key == "on"       && v.is<std::string>()) r.on = v.get<std::string>();
			else if (key == "relation" && v.is<std::string>()) r.relation = v.get<std::string>();
			else if (key == "distance" && v.is<double>())   r.distance = (int)v.get<double>();
			else if (v.is<double>())   // a PROPERTY_* : amount leaf -- the property-pulse payload (key IS the property type)
			{
				const int pid = cascadeJsonResolveId(key);
				if (pid >= 0) { r.propertyId = pid; r.propertyAmount = cascadeJsonX100(v.get<double>()); }
			}
		}
		pData->grantRepeatables.push_back(r);
	}
}

// `grants` (json.md §5): GENERIC by value-shape -- array-of-strings = an id LIST; array-of-objects = entry list
// (`repeatable` -> the structured parse above; foundBuildings/... -> resolve its single id field); number = a numeric
// pulse; bare string = a single-id grant; bool = a flag; scoped-object = a scoped pulse. NOTHING is "unknown."
static void rj_walkGrants(CvJsonInfo* pData, const picojson::value& v)
{
	if (!v.is<picojson::object>()) return;
	const picojson::object& o = v.get<picojson::object>();
	static const char* ID_FIELDS[] = { "building", "unit", "type", "bonus", "tech", "promotion", "specialist", 0 };
	for (picojson::object::const_iterator it = o.begin(); it != o.end(); ++it)
	{
		const std::string& k = it->first;
		const picojson::value& val = it->second;
		if (val.is<picojson::array>())
		{
			const picojson::array& a = val.get<picojson::array>();
			bool allStr = true;
			for (size_t i = 0; i < a.size(); ++i) if (!a[i].is<std::string>()) { allStr = false; break; }
			if (allStr)
			{
				for (size_t i = 0; i < a.size(); ++i)
				{
					const int rid = cascadeJsonResolveId(a[i].get<std::string>());
					if (rid >= 0) pData->grantLists[k].push_back(rid);
				}
			}
			else if (k == "repeatable") rj_parseRepeatable(pData, a);   // structured capture (increment 2b): spawn/heal/property-pulse
			else                                            // other array of entry-objects (foundBuildings/...): resolve the single id field
			{
				for (size_t i = 0; i < a.size(); ++i)
				{
					if (!a[i].is<picojson::object>()) continue;
					const picojson::object& eo = a[i].get<picojson::object>();
					for (int f = 0; ID_FIELDS[f]; ++f)      // resolve the entry's single id field, if any
					{
						picojson::object::const_iterator fi = eo.find(ID_FIELDS[f]);
						if (fi != eo.end() && fi->second.is<std::string>())
						{
							const int rid = cascadeJsonResolveId(fi->second.get<std::string>());
							if (rid >= 0) pData->grantLists[k].push_back(rid);
							break;
						}
					}
				}
			}
		}
		else if (val.is<double>()) pData->grantPulses[k] = cascadeJsonX100(val.get<double>());   // numeric pulse grants.<channel>: value
		else if (val.is<std::string>())                                                          // single-id grant
		{
			const int rid = cascadeJsonResolveId(val.get<std::string>());
			if (rid >= 0) pData->grantLists[k].push_back(rid);
		}
		else if (val.is<bool>()) { if (val.get<bool>()) pData->grantFlags.insert(k); }            // flag grant (goldenAge: true)
		else if (val.is<picojson::object>())   // scoped-pulse grant (population {scope:N}). Object-VALUED dicts (the deferred
		{                                       // mission-key greatPersonAction {trade:{base,mult}}) have no number leaves -> skipped.
			const picojson::object& po = val.get<picojson::object>();
			for (picojson::object::const_iterator pi = po.begin(); pi != po.end(); ++pi)
				if (pi->second.is<double>()) pData->grantScopedPulses[k][pi->first] = cascadeJsonX100(pi->second.get<double>());
		}
		// (the repeatable interval/chance + property-pulse on/relation/distance structure is still dropped -- increment 2b.)
	}
}

// requires.build / requires.operate -> the typed condition tree; + the `dormant` triggers extracted SEPARATELY (the
// condition parser drops the structural `dormant` key -- json.md §4.3 / StoneBase DormantTriggers).
static void rj_walkRequires(CvJsonInfo* pData, const picojson::value& v)
{
	if (!v.is<picojson::object>()) return;
	const picojson::object& ro = v.get<picojson::object>();
	for (picojson::object::const_iterator sub = ro.begin(); sub != ro.end(); ++sub)
	{
		CvCascadeCondition* e = cascadeParseCondition(sub->second);
		if (sub->first == "build") { delete pData->requiresBuild; pData->requiresBuild = e; }
		else if (sub->first == "operate") { delete pData->requiresOperate; pData->requiresOperate = e; }
		else delete e;
		// the `dormant` triggers: building operate.dormant = [BUILDING_…]; unit build.dormant = {all:[UNIT_…]}.
		if (sub->second.is<picojson::object>())
		{
			const picojson::object& clause = sub->second.get<picojson::object>();
			picojson::object::const_iterator dm = clause.find("dormant");
			if (dm != clause.end())
			{
				const picojson::value* arr = NULL;
				if (dm->second.is<picojson::array>()) arr = &dm->second;
				else if (dm->second.is<picojson::object>())
				{
					const picojson::object& do_ = dm->second.get<picojson::object>();
					picojson::object::const_iterator al = do_.find("all");
					if (al != do_.end() && al->second.is<picojson::array>()) arr = &al->second;
				}
				if (arr != NULL)
				{
					const picojson::array& a = arr->get<picojson::array>();
					for (size_t i = 0; i < a.size(); ++i)
						if (a[i].is<std::string>())
						{
							const int id = cascadeJsonResolveId(a[i].get<std::string>());
							if (id >= 0) pData->dormantTriggers.push_back(id);
						}
				}
			}
		}
	}
}

void CvJsonInfo::mapFrom(const picojson::value& entity)
{
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();
	for (picojson::object::const_iterator it = o.begin(); it != o.end(); ++it)
	{
		const std::string& k = it->first;
		// classify via the ONE shared vocabulary (cascadeJsonClassifyKey) -- same call the reader census uses.
		switch (cascadeJsonClassifyKey(k, it->second.is<picojson::object>()))
		{
		case CJK_EDGE:     rj_walkEnableEdge(this, k, it->second); break;            // §4.1/§4.2 GENERATE buckets
		case CJK_PROVIDES: rj_walkEnableEdge(this, "provides", it->second); break;   // §5a continuous supply
		case CJK_ALLOWED:  rj_walkAllowed(this, it->second); break;                  // §4.4 the cap
		case CJK_GRANTS:   rj_walkGrants(this, it->second); break;                   // §5 the grants grammar
		case CJK_REQUIRES: rj_walkRequires(this, it->second); break;                 // §4.3 build/operate + dormant
		case CJK_FAMILY:   rj_walkModNode(this, k, it->second); break;               // §6 modifier-family deposits
		// CJK_INTRINSIC = intrinsic/auxiliary/classification (a SUBCLASS or another system owns it) -- the base skips it.
		// CJK_FLAG = a scalar unknown (a flag/text field) -- no common home; skipped here.
		case CJK_INTRINSIC:
		case CJK_FLAG:     break;
		}
	}
}
