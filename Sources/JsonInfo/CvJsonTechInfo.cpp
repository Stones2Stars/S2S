//
//	CvJsonTechInfo::mapFrom -- base common sections + the empire-ability blocks (§8: capabilities / canTrade /
//	canTradeOn / canWorkOn) + the tech's OWN typed values (empire modifiers, flags, AI, art/sound/quote). The whole
//	forward-unlock surface is store-inverted onto other entities' `enables.*` (base) -- never a getter here.
//	HUMAN-native (assetValue/powerValue re-apply the latent ×100). See the header.
//

#include "CvGameCoreDLL.h"          // PCH umbrella -- picojson
#include "CvJsonTechInfo.h"
#include "CvJsonParse.h"            // jsonBoolSet + jsonResolveId
#include "Defines/CvGlobals.h"      // GC.getInfoTypeForString (FK)

CvJsonTechInfo::CvJsonTechInfo()
	: m_iResearchCost(0), m_iEra(-1), m_iAdvisorType(-1), m_iTradeRoutes(0),
	  m_iFeatureProductionModifier(0), m_iWorkerSpeedModifier(0), m_iHealth(0), m_iHappiness(0),
	  m_iGlobalTradeModifier(0), m_iGlobalForeignTradeModifier(0), m_iTradeMissionModifier(0),
	  m_iCorporationRevenueModifier(0), m_iCorporationMaintenanceModifier(0), m_iAssetValue(0), m_iPowerValue(0),
	  m_iGridX(0), m_iGridY(0), m_iAIWeight(0), m_iAITradeModifier(0),
	  m_bRepeat(false), m_bTrade(false), m_bDisable(false), m_bGoodyTech(false)
{}

static const picojson::object* child_obj(const picojson::object& o, const char* key)
{ picojson::object::const_iterator it = o.find(key); return (it != o.end() && it->second.is<picojson::object>()) ? &it->second.get<picojson::object>() : NULL; }
static int fam_val(const picojson::object& o, const char* f, const char* s, const char* u)
{ const picojson::object* fo = child_obj(o, f); if (!fo) return 0; const picojson::object* so = child_obj(*fo, s); if (!so) return 0;
  picojson::object::const_iterator it = so->find(u); return (it != so->end() && it->second.is<double>()) ? (int)it->second.get<double>() : 0; }
static int id_int(const picojson::object& io, const char* k)
{ picojson::object::const_iterator it = io.find(k); return (it != io.end() && it->second.is<double>()) ? (int)it->second.get<double>() : 0; }
static double id_dbl(const picojson::object& io, const char* k)
{ picojson::object::const_iterator it = io.find(k); return (it != io.end() && it->second.is<double>()) ? it->second.get<double>() : 0.0; }
static bool id_bool(const picojson::object& io, const char* k)
{ picojson::object::const_iterator it = io.find(k); return (it != io.end() && it->second.is<bool>()) ? it->second.get<bool>() : false; }
static int id_fk(const picojson::object& io, const char* k)
{ picojson::object::const_iterator it = io.find(k); return (it != io.end() && it->second.is<std::string>()) ? GC.getInfoTypeForString(it->second.get<std::string>().c_str(), true) : -1; }
static void id_str(const picojson::object& io, const char* k, std::string& out)
{ picojson::object::const_iterator it = io.find(k); if (it != io.end() && it->second.is<std::string>()) out = it->second.get<std::string>(); }
// re-apply the latent ×100 (round half away from zero): JSON carries the ÷100 human value, the getter returns ×100.
static int x100(double v) { return (int)(v >= 0 ? v * 100.0 + 0.5 : v * 100.0 - 0.5); }
// FK-keyed int map: {"FLAVOR_X"/"SPECIALIST_X": n} -> map[id] = n.
static void read_fk_int_map(const picojson::object* parent, const char* key, std::map<int, int>& out)
{
	if (!parent) return;
	picojson::object::const_iterator it = parent->find(key);
	if (it == parent->end() || !it->second.is<picojson::object>()) return;
	const picojson::object& m = it->second.get<picojson::object>();
	for (picojson::object::const_iterator e = m.begin(); e != m.end(); ++e)
		if (e->second.is<double>()) { const int id = GC.getInfoTypeForString(e->first.c_str(), true); if (id >= 0) out[id] = (int)e->second.get<double>(); }
}

void CvJsonTechInfo::mapFrom(const picojson::value& entity)
{
	CvJsonInfo::mapFrom(entity);    // the common cascade sections (text + availability) first
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();

	// (1) empire-ability grantor blocks (§8): the flat `capabilities` block rides the base dispatch into the
	// composed m_capabilities unit; the bespoke siblings stay subclass-parsed here.
	picojson::object::const_iterator it;
	if ((it = o.find("canTrade")) != o.end()) jsonBoolSet(it->second, canTrade);
	if ((it = o.find("canWorkOn")) != o.end()) jsonBoolSet(it->second, canWorkOn);
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
	if (const picojson::object* co = child_obj(o, "cost")) m_iResearchCost = id_int(*co, "research");
	m_iTradeRoutes                     = fam_val(o, "tradeRoutes", "city", "flat");
	m_iFeatureProductionModifier       = fam_val(o, "featureProduction", "empire", "percent");
	m_iWorkerSpeedModifier             = fam_val(o, "workRate", "empire", "percent");
	m_iHealth                          = fam_val(o, "health", "empire", "flat");
	m_iHappiness                       = fam_val(o, "happiness", "empire", "flat");
	m_iGlobalTradeModifier             = fam_val(o, "tradeRouteYield", "empire", "percent");
	m_iGlobalForeignTradeModifier      = fam_val(o, "foreignTradeRouteYield", "empire", "percent");
	m_iTradeMissionModifier            = fam_val(o, "tradeMission", "empire", "percent");
	m_iCorporationRevenueModifier      = fam_val(o, "corporationRevenue", "empire", "percent");
	m_iCorporationMaintenanceModifier  = fam_val(o, "corporationMaintenance", "empire", "percent");

	// identity: scalars / flags / FK / the ×100-re-applied worth fields / the quote key
	if (const picojson::object* io = child_obj(o, "identity"))
	{
		m_iEra        = id_fk(*io, "era");
		m_iGridX      = id_int(*io, "gridX");
		m_iGridY      = id_int(*io, "gridY");
		m_iAssetValue = x100(id_dbl(*io, "worth"));           // legacy getAssetValue returns ×100; JSON worth is human
		m_iPowerValue = x100(id_dbl(*io, "militaryWorth"));   // legacy getPowerValue returns ×100
		m_bRepeat     = id_bool(*io, "repeat");
		m_bTrade      = id_bool(*io, "tradeable");
		m_bDisable    = id_bool(*io, "disable");
		m_bGoodyTech  = id_bool(*io, "goodyTech");
		std::string q; id_str(*io, "quote", q); if (!q.empty()) m_szQuoteKey = CvWString(q.c_str());
	}

	// ui.art.advisor (FK) -- NB under ui.art, not identity
	if (const picojson::object* ui = child_obj(o, "ui"))
		if (const picojson::object* art = child_obj(*ui, "art"))
			m_iAdvisorType = id_fk(*art, "advisor");

	// sound.soundMP
	if (const picojson::object* so = child_obj(o, "sound")) id_str(*so, "soundMP", m_szSoundMP);

	// ai.behaviour.{weight,tradeModifier} + ai.flavours {FLAVOR:int}
	if (const picojson::object* ai = child_obj(o, "ai"))
	{
		if (const picojson::object* be = child_obj(*ai, "behaviour"))
		{
			m_iAIWeight        = id_int(*be, "weight");
			m_iAITradeModifier = id_int(*be, "tradeModifier");
		}
		// ai.flavours -- an ARRAY of single-key { FLAVOR_X: n } objects (NOT a map, unlike freeSpecialists.team)
		picojson::object::const_iterator fv = ai->find("flavours");
		if (fv != ai->end() && fv->second.is<picojson::array>())
		{
			const picojson::array& fa = fv->second.get<picojson::array>();
			for (size_t i = 0; i < fa.size(); ++i)
				if (fa[i].is<picojson::object>())
				{
					const picojson::object& fo2 = fa[i].get<picojson::object>();
					for (picojson::object::const_iterator e = fo2.begin(); e != fo2.end(); ++e)
						if (e->second.is<double>()) { const int id = GC.getInfoTypeForString(e->first.c_str(), true); if (id >= 0) m_flavours[id] = (int)e->second.get<double>(); }
				}
		}
	}

	// freeSpecialists.team.{SPECIALIST} (inert -- no tech write-path today; carried faithfully)
	if (const picojson::object* fs = child_obj(o, "freeSpecialists"))
		read_fk_int_map(fs, "team", m_freeSpecialists);
}

// --- the synthetic TECH_GAME_START root (readjson.md §5.1) ---
// No engine id -> lives OFF the InfoRepo as this owned singleton. Reset-recreate (never re-parse into a stale
// object) keeps the write-once-at-load discipline across a re-map.
static CvJsonTechInfo* s_pStartNode = NULL;

CvJsonTechInfo& cascadeStartNode()
{
	if (s_pStartNode == NULL) s_pStartNode = new CvJsonTechInfo();
	return *s_pStartNode;
}

void cascadeStartNodeReset()
{
	delete s_pStartNode;
	s_pStartNode = NULL;
}
