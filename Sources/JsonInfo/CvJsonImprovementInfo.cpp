//
//	CvJsonImprovementInfo::mapFrom -- base core reading + availability (the terrain/feature/irrigation VALIDITY
//	prereqs ride requires.build, store-inverted by the curator), then the improvement's real members from the
//	curator's family/identity/mapGeneration shapes. HUMAN-native values. FK resolution via the kept type registry.
//	Shapes nailed against curate_improvement.py (2026-07-07). See header.
//

#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson
#include "CvJsonImprovementInfo.h"
#include "CvJsonParse.h"          // jsonResolveId + the shared walkers (jsonChildObj/jsonFamVal/...)

CvJsonImprovementInfo::CvJsonImprovementInfo()
	: m_iDefenseModifier(0), m_iAirBombDefense(0), m_iHealthPercent(0), m_iHappiness(0), m_iCulture(0),
	  m_iPillageGold(0), m_iUniqueRange(0), m_iCultureRange(0), m_iFeatureGrowthProbability(0), m_iUpgradeTime(0),
	  m_eImprovementUpgrade(NO_IMPROVEMENT), m_eImprovementPillage(NO_IMPROVEMENT), m_eBonusChange(NO_BONUS),
	  m_bActsAsCity(false), m_bMilitaryStructure(false), m_bCarriesIrrigation(false),
	  m_bOutsideBorders(false), m_bBombardable(false), m_bZOCSource(false), m_bExtraterrestrial(false),
	  m_bUniversalBonusTrade(false), m_bGoody(false), m_bRequiresRiverSide(false)
{
	for (int i = 0; i < NUM_YIELD_TYPES; ++i) m_aiYieldChange[i] = 0;
}

void CvJsonImprovementInfo::mapFrom(const picojson::value& entity)
{
	CvJsonInfo::mapFrom(entity);   // core reading + availability (requires.build carries the placement/validity prereqs)
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();

	// plot yield + modifier families (the improvement's own tile output)
	m_aiYieldChange[YIELD_FOOD]       = jsonFamVal(o, "food", "plot", "flat");
	m_aiYieldChange[YIELD_PRODUCTION] = jsonFamVal(o, "production", "plot", "flat");
	m_aiYieldChange[YIELD_COMMERCE]   = jsonFamVal(o, "commerce", "plot", "flat");
	m_iDefenseModifier = jsonFamMemberVal(o, "defense", "plot", "amount", "percent");   // iDefenseModifier -> defense.plot.amount.percent
	m_iAirBombDefense  = jsonFamMemberVal(o, "defense", "plot", "air", "flat");         // iAirBombDefense -> defense.plot.air.flat
	m_iCulture         = jsonFamVal(o, "culture", "plot", "flat");                      // iCulture -> culture.plot.flat (Super Forts)
	// m_iHealthPercent stays 0: iHealthPercent is a BALANCE-CUT source from improvements (curate_improvement.py
	//   EXTRA_DROP). The getter survives (live UI/CvCity callers) reading the cut-to-0 member.

	// mapGeneration
	if (const picojson::object* mg = jsonChildObj(o, "mapGeneration"))
		m_iUniqueRange = jsonIdInt(*mg, "uniqueRange");   // iUniqueRange -> mapGeneration.uniqueRange

	// identity: scalars, FKs, held-capability flags (the moved placement-domain flags are NOT here -- see below)
	if (const picojson::object* io = jsonChildObj(o, "identity"))
	{
		m_iHappiness                = jsonIdInt(*io, "happiness");            // iHappiness -> identity (default path)
		m_iPillageGold              = jsonIdInt(*io, "pillageGold");          // iPillageGold -> pillageGold (id_rename)
		m_iCultureRange             = jsonIdInt(*io, "cultureRange");         // iCultureRange -> identity (owner: leave)
		m_iFeatureGrowthProbability = jsonIdInt(*io, "featureGrowth");        // iFeatureGrowth -> identity (owner: leave)
		m_iUpgradeTime              = jsonIdInt(*io, "upgradeTime");          // iUpgradeTime -> identity
		m_eImprovementUpgrade       = (ImprovementTypes)jsonIdFk(*io, "upgradesTo");   // ImprovementUpgrade -> upgradesTo (id_rename)
		m_eImprovementPillage       = (ImprovementTypes)jsonIdFk(*io, "pillageTo");    // ImprovementPillage -> pillageTo (id_rename)
		m_eBonusChange              = (BonusTypes)jsonIdFk(*io, "bonusChange");         // BonusChange -> identity.bonusChange

		m_bActsAsCity            = jsonIdBool(*io, "actsAsCity");
		m_bMilitaryStructure     = jsonIdBool(*io, "militaryStructure");
		m_bCarriesIrrigation     = jsonIdBool(*io, "carriesIrrigation");     // KEPT in identity (propagation is live code)
		m_bOutsideBorders        = jsonIdBool(*io, "outsideBorders");
		m_bBombardable           = jsonIdBool(*io, "bombardable");
		m_bZOCSource             = jsonIdBool(*io, "zoneOfControl");         // bIsZOCSource -> zoneOfControl (id_rename)
		m_bExtraterrestrial      = jsonIdBool(*io, "extraterrestrial");      // bExtraterresial -> extraterrestrial (id_rename)
		m_bUniversalBonusTrade   = jsonIdBool(*io, "universalBonusTrade");   // bIsUniversalTradeBonusProvider (LIVE lynchpin)
		// NB waterImprovement / requiresIrrigation / peakImprovement are NOT read here: they are PLACEMENT-DOMAIN
		//    prereqs the curator store-inverts into requires.build (IS_WATER / HAS_IRRIGATION / HAS_PEAK), read by
		//    the cascade GENERATE gate -- not poco getters.

		picojson::object::const_iterator mc = io->find("mapCategories");
		if (mc != io->end() && mc->second.is<picojson::array>())
		{
			const picojson::array& a = mc->second.get<picojson::array>();
			for (size_t i = 0; i < a.size(); ++i)
				if (a[i].is<std::string>()) { const int id = jsonResolveId(a[i].get<std::string>()); if (id >= 0) m_aeMapCategories.push_back((MapCategoryTypes)id); }
		}
	}

	// world.art.icon -- the ART_DEF_* tag (EXE map-gen art lookup, via the CvImprovementInfo shim's getArtInfo)
	if (const picojson::object* art = jsonWorldArt(o)) jsonIdStr(*art, "icon", m_szArtDefineTag);
	// mapGeneration.goody / .requiresRiverSide -- the EXE-bound isGoody() / isRequiresRiverSide()
	if (const picojson::object* mg = jsonChildObj(o, "mapGeneration"))
	{
		m_bGoody            = jsonIdBool(*mg, "goody");
		m_bRequiresRiverSide = jsonIdBool(*mg, "requiresRiverSide");
	}
}
