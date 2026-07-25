//
//	CvTechInfo::mapFrom -- base common sections + the empire-ability blocks (§8: capabilities / canTrade /
//	canTradeOn / canWorkOn) + the tech's OWN typed values (empire modifiers, flags, AI, art/sound/quote). The whole
//	forward-unlock surface is store-inverted onto other entities' `enables.*` (base) -- never a getter here.
//	HUMAN-native (assetValue/powerValue re-apply the latent ×100). See the header.
//

#include "CvGameCoreDLL.h"          // PCH umbrella -- picojson
#include "CvTechInfo.h"
#include "CvJsonParse.h"            // jsonBoolSet + jsonResolveId + the shared walkers (jsonChildObj/jsonFamVal/...)

CvTechInfo::CvTechInfo()
	: m_iResearchCost(0), m_iEra(-1), m_iAdvisorType(-1), m_iTradeRoutes(0),
	  m_iFeatureProductionModifier(0), m_iWorkerSpeedModifier(0), m_iHealth(0), m_iHappiness(0),
	  m_iGlobalTradeModifier(0), m_iGlobalForeignTradeModifier(0), m_iTradeMissionModifier(0),
	  m_iCorporationRevenueModifier(0), m_iCorporationMaintenanceModifier(0), m_iInflationModifier(0), m_iAssetValue(0), m_iPowerValue(0),
	  m_iGridX(0), m_iGridY(0), m_iAIWeight(0), m_iAITradeModifier(0),
	  m_bRepeat(false), m_bTrade(false), m_bDisable(false), m_bGoodyTech(false),
	  m_bTradeTechs(false), m_bTradeGold(false), m_bTradeMaps(false), m_bTradeOpenBorders(false),
	  m_bTradeDefensivePact(false), m_bTradePermanentAlliance(false), m_bTradeVassals(false), m_bTradeEmbassy(false),
	  m_bWorkWater(false), m_bGlobal(false),
	  m_iFirstFreeProphet(-1), m_iFirstFreeUnit(-1), m_iFirstFreeTechs(0)
{}

// identity.quote -- RESOLVE the loaded TXT_KEY to display text, mirroring the archived CvTechInfo::getQuote. Returning
// the raw m_szQuoteKey showed the literal "TXT_KEY_..._QUOTE" in the tech splash. std::wstring: Boost.Python 1.32 has a
// std::wstring to_python converter, none for CvWString (the TechWindow.getQuote() TypeError).
std::wstring CvTechInfo::getQuote() const
{
	return gDLL->getText(m_szQuoteKey);
}

// identity double (the ×100-re-applied worth fields read a fractional human value).
static double id_dbl(const picojson::object& io, const char* k)
{ picojson::object::const_iterator it = io.find(k); return (it != io.end() && it->second.is<double>()) ? it->second.get<double>() : 0.0; }

void CvTechInfo::mapFrom(const picojson::value& entity)
{
	// remap-idempotency (CvInfo.h): the full-registry pass re-runs mapFrom -- fully define the accumulating members
	// (the ability SETS re-insert identical elements and need no clear; the prereq vectors would double).
	m_aePrereqAndTechs.clear(); m_aePrereqOrTechs.clear(); m_aPrereqOrBuildings.clear();
	CvInfo::mapFrom(entity);    // the common cascade sections (text + availability) first
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();

	// (1) empire-ability grantor blocks (§8): the flat `capabilities` block rides the base dispatch into the
	// composed m_capabilities unit; the bespoke siblings stay subclass-parsed here.
	picojson::object::const_iterator it;
	if ((it = o.find("canTrade")) != o.end()) jsonBoolSet(it->second, canTrade);
	if ((it = o.find("canWorkOn")) != o.end()) jsonBoolSet(it->second, canWorkOn);
	// materialized getter members (the per-call set/grants/allowed string reads retire to bare member reads)
	m_bTradeTechs             = canTrade.count("techs") != 0;
	m_bTradeGold              = canTrade.count("gold") != 0;
	m_bTradeMaps              = canTrade.count("maps") != 0;
	m_bTradeOpenBorders       = canTrade.count("openBorders") != 0;
	m_bTradeDefensivePact     = canTrade.count("defensivePact") != 0;
	m_bTradePermanentAlliance = canTrade.count("permanentAlliance") != 0;
	m_bTradeVassals           = canTrade.count("vassals") != 0;
	m_bTradeEmbassy           = canTrade.count("embassy") != 0;
	m_bWorkWater              = canWorkOn.count("water") != 0;
	m_bGlobal                 = m_allowed.cap("world") == 1;
	m_iFirstFreeProphet       = m_grants.firstListId("firstFreeProphet");
	m_iFirstFreeUnit          = m_grants.firstListId("firstFreeUnit");
	m_iFirstFreeTechs         = m_grants.pulse100("freeTechs") / 100;
	if ((it = o.find("canTradeOn")) != o.end() && it->second.is<picojson::object>())
	{
		picojson::object::const_iterator tt = it->second.get<picojson::object>().find("terrains");
		if (tt != it->second.get<picojson::object>().end() && tt->second.is<picojson::array>())
		{
			const picojson::array& a = tt->second.get<picojson::array>();
			for (size_t i = 0; i < a.size(); ++i)
				if (a[i].is<std::string>()) { const int id = jsonResolveId(a[i].get<std::string>()); if (id >= 0) canTradeOnTerrains.insert(id); }
		}
	}

	// (2) the tech's own typed values -- empire-scope modifier families (×1 human)
	if (const picojson::object* co = jsonChildObj(o, "cost")) m_iResearchCost = jsonIdInt(*co, "research");
	// domainMoves.empire.domains.{DOMAIN}.flat -> m_domainExtraMoves (DomainExtraMoves; getDomainExtraMoves)
	if (const picojson::object* dm = jsonChildObj(o, "domainMoves"))
		if (const picojson::object* emp = jsonChildObj(*dm, "empire"))
			if (const picojson::object* dom = jsonChildObj(*emp, "domains"))
				for (picojson::object::const_iterator it = dom->begin(); it != dom->end(); ++it)
				{
					const int id = jsonResolveId(it->first);
					if (id < 0 || !it->second.is<picojson::object>()) continue;
					const picojson::object& to = it->second.get<picojson::object>();
					picojson::object::const_iterator fl = to.find("flat");
					if (fl != to.end() && fl->second.is<double>()) m_domainExtraMoves[id] = (int)fl->second.get<double>();
				}
	m_iTradeRoutes                     = jsonFamVal(o, "tradeRoutes", "city", "flat");
	m_iFeatureProductionModifier       = jsonFamVal(o, "featureProduction", "empire", "percent");
	m_iWorkerSpeedModifier             = jsonFamVal(o, "workRate", "empire", "percent");
	m_iHealth                          = jsonFamVal(o, "health", "empire", "flat");
	m_iHappiness                       = jsonFamVal(o, "happiness", "empire", "flat");
	m_iGlobalTradeModifier             = jsonFamVal(o, "tradeRouteYield", "empire", "percent");
	m_iGlobalForeignTradeModifier      = jsonFamVal(o, "foreignTradeRouteYield", "empire", "percent");
	m_iTradeMissionModifier            = jsonFamVal(o, "tradeMission", "empire", "percent");
	m_iCorporationRevenueModifier      = jsonFamVal(o, "corporationRevenue", "empire", "percent");
	m_iCorporationMaintenanceModifier  = jsonFamVal(o, "corporationMaintenance", "empire", "percent");
	m_iInflationModifier               = jsonFamVal(o, "inflation", "empire", "percent");   // legacy iInflationModifier -> CvPlayer::changeTechInflation (getInflationMod10000). Inflation is its OWN family (DEC-maintenance-bookkeeping), matching CvBuildingInfo

	// identity: scalars / flags / FK / the ×100-re-applied worth fields / the quote key
	if (const picojson::object* io = jsonChildObj(o, "identity"))
	{
		m_iEra        = jsonIdFk(*io, "era");
		m_iGridX      = jsonIdInt(*io, "gridX");
		m_iGridY      = jsonIdInt(*io, "gridY");
		m_iAssetValue = jsonX100(id_dbl(*io, "worth"));           // legacy getAssetValue returns ×100; JSON worth is human
		m_iPowerValue = jsonX100(id_dbl(*io, "militaryWorth"));   // legacy getPowerValue returns ×100
		m_bRepeat     = jsonIdBool(*io, "repeat");
		m_bTrade      = jsonIdBool(*io, "tradeable");
		m_bDisable    = jsonIdBool(*io, "disable");
		m_bGoodyTech  = jsonIdBool(*io, "goodyTech");
		std::string q; jsonIdStr(*io, "quote", q); if (!q.empty()) m_szQuoteKey = CvWString(q.c_str());
	}

	// ui.art.advisor (FK) -- NB under ui.art, not identity
	if (const picojson::object* ui = jsonChildObj(o, "ui"))
		if (const picojson::object* art = jsonChildObj(*ui, "art"))
			m_iAdvisorType = jsonIdFk(*art, "advisor");

	// sound.{sound,soundMP} -- sound is the tech-completed jingle; soundMP the MP variant (distinct keys)
	if (const picojson::object* so = jsonChildObj(o, "sound"))
	{
		jsonIdStr(*so, "sound", m_szSound);
		jsonIdStr(*so, "soundMP", m_szSoundMP);
	}

	// ai.behaviour.{weight,tradeModifier} + ai.flavours {FLAVOR:int}
	if (const picojson::object* ai = jsonChildObj(o, "ai"))
	{
		if (const picojson::object* be = jsonChildObj(*ai, "behaviour"))
		{
			m_iAIWeight        = jsonIdInt(*be, "weight");
			m_iAITradeModifier = jsonIdInt(*be, "tradeModifier");
		}
		// ai.flavours -- an ARRAY of single-key { FLAVOR_X: n } objects (NOT a map, unlike freeSpecialists.team)
		jsonReadFlavours(*ai, m_flavours);
	}

	// freeSpecialists.team.{SPECIALIST} (inert -- no tech write-path today; carried faithfully)
	if (const picojson::object* fs = jsonChildObj(o, "freeSpecialists"))
		jsonReadFkMap(*fs, "team", m_freeSpecialists);

	// --- requires-tree prereqs: walk the composed requires.build the base just parsed (mirrors CvUnitInfo /
	// CvBuildInfo). curate_tech.requires_fn builds ONE `all` list holding: team-scope TECH_ presence atoms
	// (AND prereqs -- AndPreReqs + folded 1-member OrPreReqs), empire-scope BUILDING_ AND atoms (no tech data today),
	// and nested {any} OR-groups (each homogeneous: a multi-member tech OR-group, or the building OR-group with min).
	const CvRequires* r = getRequires();
	if (r != NULL && r->build != NULL)
	{
		const CvCondition* b = r->build;
		for (size_t i = 0; i < b->all.size(); ++i)
		{
			const CvCondition* c = b->all[i];
			if (c == NULL) continue;
			if (c->kind == CASC_COND_PRESENCE)
			{
				if (c->id >= 0 && c->type.compare(0, 5, "TECH_") == 0)
					m_aePrereqAndTechs.push_back((TechTypes)c->id);   // AND tech prereq (incl. folded 1-member OR)
			}
			else if (c->kind == CASC_COND_GROUP && !c->anyOf.empty())
			{
				const CvCondition* first = c->anyOf[0];
				if (first == NULL || first->kind != CASC_COND_PRESENCE) continue;
				const bool bTech     = first->type.compare(0, 5, "TECH_") == 0;
				const bool bBuilding = first->type.compare(0, 9, "BUILDING_") == 0;
				if (c->anyOf.size() == 1)   // a single-member OR is logically a hard AND (defensive: curator pre-folds these)
				{
					if (bTech && first->id >= 0) m_aePrereqAndTechs.push_back((TechTypes)first->id);
					continue;
				}
				if (bTech && m_aePrereqOrTechs.empty())        // FIRST multi-member TECH OR-group (mirrors legacy single Or-list)
				{
					for (size_t j = 0; j < c->anyOf.size(); ++j)
					{
						const CvCondition* m = c->anyOf[j];
						if (m != NULL && m->kind == CASC_COND_PRESENCE && m->id >= 0 && m->type.compare(0, 5, "TECH_") == 0)
							m_aePrereqOrTechs.push_back((TechTypes)m->id);
					}
				}
				else if (bBuilding && m_aPrereqOrBuildings.empty())   // the BUILDING_ OR-group ((building, min) count atoms)
				{
					for (size_t j = 0; j < c->anyOf.size(); ++j)
					{
						const CvCondition* m = c->anyOf[j];
						if (m != NULL && m->kind == CASC_COND_PRESENCE && m->id >= 0 && m->type.compare(0, 9, "BUILDING_") == 0)
						{
							PrereqBuilding pb;
							pb.eBuilding = (BuildingTypes)m->id;
							pb.iMinimumRequired = (m->min > 0) ? m->min : 1;
							m_aPrereqOrBuildings.push_back(pb);
						}
					}
				}
			}
		}
	}
}

// --- the synthetic TECH_GAME_START root (readjson.md §5.1) ---
// No engine id -> lives OFF the InfoRepo as this owned singleton. Reset-recreate (never re-parse into a stale
// object) keeps the write-once-at-load discipline across a re-map.
static CvTechInfo* s_pStartNode = NULL;

CvTechInfo& cascadeStartNode()
{
	if (s_pStartNode == NULL) s_pStartNode = new CvTechInfo();
	return *s_pStartNode;
}

void cascadeStartNodeReset()
{
	delete s_pStartNode;
	s_pStartNode = NULL;
}
