//
//	InfoRepo.cpp -- the ONE definition site of every InfoRepo<TTag>::get() singleton (fix 2026-07-02).
//
//	WHY THIS FILE EXISTS: get() used to be header-inline with a function-local static. Under the vendored VC7.1
//	toolchain + FastBuild unity batching, a local static in an INLINED template member is DUPLICATED per translation
//	unit (the classic VC7.x defect) -- CvReadJson.cpp mapped 13,444 JSON entities into ITS copy of each
//	singleton while the calc/shadow TUs (a different unity batch) read a DIFFERENT, EMPTY copy: the live signature was
//	[MODIFIER/repo] probeFiles=13444 probeEntities=13444 mapped=0, which silently zeroed the whole modifier building
//	tier and the dormancy derivation. Explicit per-tag specializations in this single TU guarantee ONE instance per
//	tag process-wide; the header only DECLARES get(), so a NEW tag use without a row here fails at LINK -- add its
//	CASCADE_INFOREPO_DEFINE line below (and, if it carries type-specific data, a RepoPayload specialization in the header).
//

#include "CvGameCoreDLL.h"
#include "InfoRepo.h"

// Phantom tag types not already declared by InfoRepo.h (never dereferenced -- per-type discriminators only).
class CvCivicOptionInfo; class CvPromotionLineInfo; class CvCultureLevelInfo; class CvSpecialistInfo;
class CvBonusInfo; class CvImprovementInfo; class CvFeatureInfo; class CvTerrainInfo; class CvRouteInfo;
class CvProjectInfo; class CvProcessInfo; class CvHeritageInfo; class CvBuildInfo; class CvCivilizationInfo;
class CvEraInfo; class CvHandicapInfo; class CvSpecialBuildingInfo; class CvPropertyInfo;
// The five GENERATED classification categories (ClassificationRegistry mints SKILL_/TAG_/ATTRIBUTE_/CAPABILITY_/
// POLICY_ infos from the union of authored §8/§9 block keys) -- JSON-derived, no XML shell, plain-CvInfo payload.
class CvSkillClsTag; class CvTagClsTag; class CvAttributeClsTag; class CvCapabilityClsTag; class CvPolicyClsTag;

// #430 option B: ALIAS the tag's singleton to the engine's GC.m_pa<X>Info array -- the repo becomes a VIEW over the
// read()+mapFrom'd objects getXInfo returns (one object, no separate store). The reinterpret is layout-safe: both are
// std::vector<pointer>, and CvJson<X>Info / the Cv<X>Info shims derive from CvInfo (element upcast is trivial).
#define CASCADE_INFOREPO_ALIAS(TAG, ARR) \
	template <> InfoRepo<TAG>& InfoRepo<TAG>::get() \
	{ \
		static InfoRepo<TAG> s_instance; \
		if (!s_instance.isAliased()) s_instance.bind(reinterpret_cast<std::vector<CvInfo*>*>(&GC.ARR)); \
		return s_instance; \
	}
// OWNED: JSON-only tags with no XML shell array (Heritage/Build/complex-traits) + the uniformity set (consumed via
// their legacy arrays, not this repo). The repo owns its m_data; loadJson maps the JSON-only ones into it.
#define CASCADE_INFOREPO_OWNED(TAG) \
	template <> InfoRepo<TAG>& InfoRepo<TAG>::get() \
	{ \
		static InfoRepo<TAG> s_instance; \
		return s_instance; \
	}

CASCADE_INFOREPO_ALIAS(CvBuildingInfo,      m_paBuildingInfo)
CASCADE_INFOREPO_ALIAS(CvUnitCombatInfo,    m_paUnitCombatInfo)
CASCADE_INFOREPO_ALIAS(CvUnitInfo,          m_paUnitInfo)
CASCADE_INFOREPO_ALIAS(CvTechInfo,          m_paTechInfo)
CASCADE_INFOREPO_ALIAS(CvCivicOptionInfo,   m_paCivicOptionInfo)
CASCADE_INFOREPO_ALIAS(CvCivicInfo,         m_paCivicInfo)
CASCADE_INFOREPO_ALIAS(CvTraitInfo,         m_paTraitInfo)
CASCADE_INFOREPO_ALIAS(CvSpecialistInfo,    m_paSpecialistInfo)
CASCADE_INFOREPO_ALIAS(CvBonusInfo,             m_paBonusInfo)
CASCADE_INFOREPO_ALIAS(CvReligionInfo,      m_paReligionInfo)
CASCADE_INFOREPO_ALIAS(CvCorporationInfo,   m_paCorporationInfo)
CASCADE_INFOREPO_ALIAS(CvPromotionLineInfo, m_paPromotionLineInfo)
CASCADE_INFOREPO_ALIAS(CvPromotionInfo,     m_paPromotionInfo)
CASCADE_INFOREPO_ALIAS(CvImprovementInfo,       m_paImprovementInfo)
CASCADE_INFOREPO_ALIAS(CvFeatureInfo,           m_paFeatureInfo)
CASCADE_INFOREPO_ALIAS(CvTerrainInfo,           m_paTerrainInfo)
CASCADE_INFOREPO_ALIAS(CvRouteInfo,         m_paRouteInfo)
CASCADE_INFOREPO_ALIAS(CvProjectInfo,       m_paProjectInfo)
CASCADE_INFOREPO_ALIAS(CvProcessInfo,       m_paProcessInfo)
CASCADE_INFOREPO_ALIAS(CvCultureLevelInfo,  m_paCultureLevelInfo)
CASCADE_INFOREPO_ALIAS(CvPropertyInfo,      m_paPropertyInfo)
CASCADE_INFOREPO_ALIAS(CvHeritageInfo, m_heritageInfo)       // #430: loaded into the GC array by LoadGlobalClassInfoJson
CASCADE_INFOREPO_ALIAS(CvBuildInfo,        m_buildTable.rows())  // #430: the BuildInfo catalog's row vector
CASCADE_INFOREPO_OWNED(CvComplexTraitTag)    // the complex-trait set -- JSON-only, no XML shell
CASCADE_INFOREPO_ALIAS(CvCivilizationInfo,  m_paCivilizationInfo) // #430: consolidated onto the JSON-fed engine class (re-mapped for post-sort derivativeCiv)
CASCADE_INFOREPO_ALIAS(CvEraInfo,           m_aEraInfo)            // #430: consolidated onto the JSON-fed engine class (note: m_aEraInfo)
CASCADE_INFOREPO_ALIAS(CvHandicapInfo,      m_paHandicapInfo)     // #430: consolidated onto the JSON-fed engine class
CASCADE_INFOREPO_ALIAS(CvSpecialBuildingInfo, m_paSpecialBuildingInfo) // #430: consolidated onto the JSON-fed engine class
CASCADE_INFOREPO_ALIAS(CvGameSpeedInfo,     m_paGameSpeedInfo)   // #430: consolidated onto the JSON-fed engine class
CASCADE_INFOREPO_ALIAS(CvLeaderHeadInfo,    m_paLeaderHeadInfo)   // #430: consolidated onto the JSON-fed engine class
CASCADE_INFOREPO_ALIAS(CvSpecialUnitInfo,   m_paSpecialUnitInfo)   // #430: consolidated onto the JSON-fed engine class
CASCADE_INFOREPO_ALIAS(CvVictoryInfo,       m_paVictoryInfo)       // #430: consolidated onto the JSON-fed engine class
CASCADE_INFOREPO_ALIAS(CvVoteInfo,          m_paVoteInfo)         // #430: consolidated onto the JSON-fed engine class
CASCADE_INFOREPO_ALIAS(CvHurryInfo,         m_paHurryInfo)        // #430: consolidated onto the JSON-fed engine class
CASCADE_INFOREPO_ALIAS(CvBonusClassInfo,    m_paBonusClassInfo)   // #430: consolidated onto the JSON-fed engine class
CASCADE_INFOREPO_ALIAS(CvWorldInfo,         m_paWorldInfo)        // #430 item 15: consolidated onto the JSON-fed engine class
CASCADE_INFOREPO_OWNED(CvSkillClsTag)        // the five generated classification categories (ClassificationRegistry)
CASCADE_INFOREPO_OWNED(CvTagClsTag)
CASCADE_INFOREPO_OWNED(CvAttributeClsTag)
CASCADE_INFOREPO_OWNED(CvCapabilityClsTag)
CASCADE_INFOREPO_OWNED(CvPolicyClsTag)

#undef CASCADE_INFOREPO_ALIAS
#undef CASCADE_INFOREPO_OWNED
