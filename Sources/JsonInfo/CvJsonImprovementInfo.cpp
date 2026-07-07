//
//	CvJsonImprovementInfo::mapFrom -- base core reading + availability (the terrain/feature/irrigation VALIDITY
//	prereqs ride requires.build, store-inverted by the curator), then the improvement's real members from the
//	curator's family/identity/mapGeneration shapes. HUMAN-native values. FK resolution via the kept type registry.
//	Shapes nailed against curate_improvement.py (2026-07-07). See header.
//

#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson, GC
#include "CvJsonImprovementInfo.h"
#include "Defines/CvGlobals.h"    // GC.getInfoTypeForString -- improvement/bonus FKs

CvJsonImprovementInfo::CvJsonImprovementInfo()
	: m_iDefenseModifier(0), m_iAirBombDefense(0), m_iHealthPercent(0), m_iHappiness(0), m_iCulture(0),
	  m_iPillageGold(0), m_iUniqueRange(0), m_iCultureRange(0), m_iFeatureGrowthProbability(0), m_iUpgradeTime(0),
	  m_eImprovementUpgrade(NO_IMPROVEMENT), m_eImprovementPillage(NO_IMPROVEMENT), m_eBonusChange(NO_BONUS),
	  m_bActsAsCity(false), m_bMilitaryStructure(false), m_bCarriesIrrigation(false),
	  m_bOutsideBorders(false), m_bBombardable(false), m_bZOCSource(false), m_bExtraterrestrial(false),
	  m_bUniversalBonusTrade(false)
{
	for (int i = 0; i < NUM_YIELD_TYPES; ++i) m_aiYieldChange[i] = 0;
}

static const picojson::object* child_obj(const picojson::object& o, const char* key)
{
	picojson::object::const_iterator it = o.find(key);
	return (it != o.end() && it->second.is<picojson::object>()) ? &it->second.get<picojson::object>() : NULL;
}
static int fam_val(const picojson::object& o, const char* family, const char* scope, const char* unit)
{
	const picojson::object* fo = child_obj(o, family);  if (!fo) return 0;
	const picojson::object* so = child_obj(*fo, scope); if (!so) return 0;
	picojson::object::const_iterator u = so->find(unit);
	return (u != so->end() && u->second.is<double>()) ? (int)u->second.get<double>() : 0;
}
static int fam_member_val(const picojson::object& o, const char* family, const char* scope, const char* member, const char* unit)
{
	const picojson::object* fo = child_obj(o, family);   if (!fo) return 0;
	const picojson::object* so = child_obj(*fo, scope);  if (!so) return 0;
	const picojson::object* mo = child_obj(*so, member); if (!mo) return 0;
	picojson::object::const_iterator u = mo->find(unit);
	return (u != mo->end() && u->second.is<double>()) ? (int)u->second.get<double>() : 0;
}
static int  id_int (const picojson::object& io, const char* key)
{ picojson::object::const_iterator it = io.find(key); return (it != io.end() && it->second.is<double>()) ? (int)it->second.get<double>() : 0; }
static bool id_bool(const picojson::object& io, const char* key)
{ picojson::object::const_iterator it = io.find(key); return (it != io.end() && it->second.is<bool>()) ? it->second.get<bool>() : false; }
static int id_fk(const picojson::object& io, const char* key)
{ picojson::object::const_iterator it = io.find(key); return (it != io.end() && it->second.is<std::string>()) ? GC.getInfoTypeForString(it->second.get<std::string>().c_str(), true) : -1; }

void CvJsonImprovementInfo::mapFrom(const picojson::value& entity)
{
	CvJsonInfo::mapFrom(entity);   // core reading + availability (requires.build carries the placement/validity prereqs)
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();

	// plot yield + modifier families (the improvement's own tile output)
	m_aiYieldChange[YIELD_FOOD]       = fam_val(o, "food", "plot", "flat");
	m_aiYieldChange[YIELD_PRODUCTION] = fam_val(o, "production", "plot", "flat");
	m_aiYieldChange[YIELD_COMMERCE]   = fam_val(o, "commerce", "plot", "flat");
	m_iDefenseModifier = fam_member_val(o, "defense", "plot", "amount", "percent");   // iDefenseModifier -> defense.plot.amount.percent
	m_iAirBombDefense  = fam_member_val(o, "defense", "plot", "air", "flat");         // iAirBombDefense -> defense.plot.air.flat
	m_iCulture         = fam_val(o, "culture", "plot", "flat");                       // iCulture -> culture.plot.flat (Super Forts)
	// m_iHealthPercent stays 0: iHealthPercent is a BALANCE-CUT source from improvements (curate_improvement.py
	//   EXTRA_DROP). The getter survives (live UI/CvCity callers) reading the cut-to-0 member.

	// mapGeneration
	if (const picojson::object* mg = child_obj(o, "mapGeneration"))
		m_iUniqueRange = id_int(*mg, "uniqueRange");   // iUniqueRange -> mapGeneration.uniqueRange

	// identity: scalars, FKs, held-capability flags (the moved placement-domain flags are NOT here -- see below)
	if (const picojson::object* io = child_obj(o, "identity"))
	{
		m_iHappiness                = id_int(*io, "happiness");            // iHappiness -> identity (default path)
		m_iPillageGold              = id_int(*io, "pillageGold");          // iPillageGold -> pillageGold (id_rename)
		m_iCultureRange             = id_int(*io, "cultureRange");         // iCultureRange -> identity (owner: leave)
		m_iFeatureGrowthProbability = id_int(*io, "featureGrowth");        // iFeatureGrowth -> identity (owner: leave)
		m_iUpgradeTime              = id_int(*io, "upgradeTime");          // iUpgradeTime -> identity
		m_eImprovementUpgrade       = (ImprovementTypes)id_fk(*io, "upgradesTo");   // ImprovementUpgrade -> upgradesTo (id_rename)
		m_eImprovementPillage       = (ImprovementTypes)id_fk(*io, "pillageTo");    // ImprovementPillage -> pillageTo (id_rename)
		m_eBonusChange              = (BonusTypes)id_fk(*io, "bonusChange");         // BonusChange -> identity.bonusChange

		m_bActsAsCity            = id_bool(*io, "actsAsCity");
		m_bMilitaryStructure     = id_bool(*io, "militaryStructure");
		m_bCarriesIrrigation     = id_bool(*io, "carriesIrrigation");     // KEPT in identity (propagation is live code)
		m_bOutsideBorders        = id_bool(*io, "outsideBorders");
		m_bBombardable           = id_bool(*io, "bombardable");
		m_bZOCSource             = id_bool(*io, "zoneOfControl");         // bIsZOCSource -> zoneOfControl (id_rename)
		m_bExtraterrestrial      = id_bool(*io, "extraterrestrial");      // bExtraterresial -> extraterrestrial (id_rename)
		m_bUniversalBonusTrade   = id_bool(*io, "universalBonusTrade");   // bIsUniversalTradeBonusProvider (LIVE lynchpin)
		// NB waterImprovement / requiresIrrigation / peakImprovement are NOT read here: they are PLACEMENT-DOMAIN
		//    prereqs the curator store-inverts into requires.build (IS_WATER / HAS_IRRIGATION / HAS_PEAK), read by
		//    the cascade GENERATE gate -- not poco getters.

		picojson::object::const_iterator mc = io->find("mapCategories");
		if (mc != io->end() && mc->second.is<picojson::array>())
		{
			const picojson::array& a = mc->second.get<picojson::array>();
			for (size_t i = 0; i < a.size(); ++i)
				if (a[i].is<std::string>()) { const int id = GC.getInfoTypeForString(a[i].get<std::string>().c_str(), true); if (id >= 0) m_aeMapCategories.push_back((MapCategoryTypes)id); }
		}
	}
}
