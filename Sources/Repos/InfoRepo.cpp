//
//	InfoRepo.cpp -- the ONE definition site of every InfoRepo<TTag>::get() singleton (fix 2026-07-02).
//
//	WHY THIS FILE EXISTS: get() used to be header-inline with a function-local static. Under the vendored VC7.1
//	toolchain + FastBuild unity batching, a local static in an INLINED template member is DUPLICATED per translation
//	unit (the classic VC7.x defect) -- CvCascadeReadJson.cpp mapped 13,444 JSON entities into ITS copy of each
//	singleton while the calc/shadow TUs (a different unity batch) read a DIFFERENT, EMPTY copy: the live signature was
//	[MODIFIER/repo] probeFiles=13444 probeEntities=13444 mapped=0, which silently zeroed the whole modifier building
//	tier and the dormancy derivation. Explicit per-tag specializations in this single TU guarantee ONE instance per
//	tag process-wide; the header only DECLARES get(), so a NEW tag use without a row here fails at LINK -- add its
//	CASCADE_INFOREPO_DEFINE line below (and, if it carries type-specific data, a JsonPayload specialization in the header).
//

#include "CvGameCoreDLL.h"
#include "InfoRepo.h"

// Phantom tag types not already declared by InfoRepo.h (never dereferenced -- per-type discriminators only).
class CvJsonCivicOptionInfo; class CvJsonPromotionLineInfo; class CvJsonCultureLevelInfo; class CvJsonSpecialistInfo;
class CvBonusInfo; class CvImprovementInfo; class CvFeatureInfo; class CvTerrainInfo; class CvJsonRouteInfo;
class CvJsonProjectInfo; class CvJsonProcessInfo; class CvJsonHeritageInfo; class CvBuildInfo; class CvCivilizationInfo;
class CvEraInfo; class CvHandicapInfo; class CvSpecialBuildingInfo; class CvJsonPropertyInfo;

// #430 option B: ALIAS the tag's singleton to the engine's GC.m_pa<X>Info array -- the repo becomes a VIEW over the
// read()+mapFrom'd objects getXInfo returns (one object, no separate store). The reinterpret is layout-safe: both are
// std::vector<pointer>, and CvJson<X>Info / the Cv<X>Info shims derive from CvJsonInfo (element upcast is trivial).
#define CASCADE_INFOREPO_ALIAS(TAG, ARR) \
	template <> InfoRepo<TAG>& InfoRepo<TAG>::get() \
	{ \
		static InfoRepo<TAG> s_instance; \
		if (!s_instance.isAliased()) s_instance.bind(reinterpret_cast<std::vector<CvJsonInfo*>*>(&GC.ARR)); \
		return s_instance; \
	}
// OWNED: JSON-only tags with no XML shell array (Heritage/Build/complex-traits) + the uniformity set (consumed via
// their legacy arrays, not this repo). The repo owns its m_data; cascadeLoadJson maps the JSON-only ones into it.
#define CASCADE_INFOREPO_OWNED(TAG) \
	template <> InfoRepo<TAG>& InfoRepo<TAG>::get() \
	{ \
		static InfoRepo<TAG> s_instance; \
		return s_instance; \
	}

CASCADE_INFOREPO_ALIAS(CvJsonBuildingInfo,      m_paBuildingInfo)
CASCADE_INFOREPO_ALIAS(CvJsonUnitCombatInfo,    m_paUnitCombatInfo)
CASCADE_INFOREPO_ALIAS(CvJsonUnitInfo,          m_paUnitInfo)
CASCADE_INFOREPO_ALIAS(CvJsonTechInfo,          m_paTechInfo)
CASCADE_INFOREPO_ALIAS(CvJsonCivicOptionInfo,   m_paCivicOptionInfo)
CASCADE_INFOREPO_ALIAS(CvJsonCivicInfo,         m_paCivicInfo)
CASCADE_INFOREPO_ALIAS(CvJsonTraitInfo,         m_paTraitInfo)
CASCADE_INFOREPO_ALIAS(CvJsonSpecialistInfo,    m_paSpecialistInfo)
CASCADE_INFOREPO_ALIAS(CvBonusInfo,             m_paBonusInfo)
CASCADE_INFOREPO_ALIAS(CvJsonReligionInfo,      m_paReligionInfo)
CASCADE_INFOREPO_ALIAS(CvJsonCorporationInfo,   m_paCorporationInfo)
CASCADE_INFOREPO_ALIAS(CvJsonPromotionLineInfo, m_paPromotionLineInfo)
CASCADE_INFOREPO_ALIAS(CvJsonPromotionInfo,     m_paPromotionInfo)
CASCADE_INFOREPO_ALIAS(CvImprovementInfo,       m_paImprovementInfo)
CASCADE_INFOREPO_ALIAS(CvFeatureInfo,           m_paFeatureInfo)
CASCADE_INFOREPO_ALIAS(CvTerrainInfo,           m_paTerrainInfo)
CASCADE_INFOREPO_ALIAS(CvJsonRouteInfo,         m_paRouteInfo)
CASCADE_INFOREPO_ALIAS(CvJsonProjectInfo,       m_paProjectInfo)
CASCADE_INFOREPO_ALIAS(CvJsonProcessInfo,       m_paProcessInfo)
CASCADE_INFOREPO_ALIAS(CvJsonCultureLevelInfo,  m_paCultureLevelInfo)
CASCADE_INFOREPO_ALIAS(CvJsonPropertyInfo,      m_paPropertyInfo)
CASCADE_INFOREPO_OWNED(CvJsonHeritageInfo)   // no m_paHeritageInfo -- JSON-only (cascadeLoadJson -> m_data)
CASCADE_INFOREPO_OWNED(CvBuildInfo)          // no m_paBuildInfo    -- JSON-only
CASCADE_INFOREPO_OWNED(CvComplexTraitTag)    // the complex-trait set -- JSON-only, no XML shell
CASCADE_INFOREPO_OWNED(CvCivilizationInfo)
CASCADE_INFOREPO_OWNED(CvEraInfo)
CASCADE_INFOREPO_OWNED(CvHandicapInfo)
CASCADE_INFOREPO_OWNED(CvSpecialBuildingInfo)
CASCADE_INFOREPO_OWNED(CvGameSpeedInfo)
CASCADE_INFOREPO_OWNED(CvLeaderHeadInfo)
CASCADE_INFOREPO_OWNED(CvSpecialUnitInfo)
CASCADE_INFOREPO_OWNED(CvVictoryInfo)
CASCADE_INFOREPO_OWNED(CvVoteInfo)
CASCADE_INFOREPO_OWNED(CvHurryInfo)
CASCADE_INFOREPO_OWNED(CvBonusClassInfo)

#undef CASCADE_INFOREPO_ALIAS
#undef CASCADE_INFOREPO_OWNED
