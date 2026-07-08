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
class CvCivicOptionInfo; class CvPromotionLineInfo; class CvCultureLevelInfo; class CvSpecialistInfo;
class CvBonusInfo; class CvImprovementInfo; class CvFeatureInfo; class CvTerrainInfo; class CvJsonRouteInfo;
class CvProjectInfo; class CvProcessInfo; class CvHeritageInfo; class CvBuildInfo; class CvCivilizationInfo;
class CvEraInfo; class CvHandicapInfo; class CvSpecialBuildingInfo; class CvPropertyInfo;

#define CASCADE_INFOREPO_DEFINE(TAG) \
	template <> InfoRepo<TAG>& InfoRepo<TAG>::get() \
	{ \
		static InfoRepo<TAG> s_instance; \
		return s_instance; \
	}

// The RJ_REPO_TYPES set (CvCascadeReadJson.cpp) ...
CASCADE_INFOREPO_DEFINE(CvBuildingInfo)
CASCADE_INFOREPO_DEFINE(CvUnitCombatInfo)
CASCADE_INFOREPO_DEFINE(CvUnitInfo)
CASCADE_INFOREPO_DEFINE(CvTechInfo)
CASCADE_INFOREPO_DEFINE(CvCivicOptionInfo)
CASCADE_INFOREPO_DEFINE(CvCivicInfo)
CASCADE_INFOREPO_DEFINE(CvTraitInfo)
CASCADE_INFOREPO_DEFINE(CvSpecialistInfo)
CASCADE_INFOREPO_DEFINE(CvBonusInfo)
CASCADE_INFOREPO_DEFINE(CvReligionInfo)
CASCADE_INFOREPO_DEFINE(CvCorporationInfo)
CASCADE_INFOREPO_DEFINE(CvPromotionLineInfo)
CASCADE_INFOREPO_DEFINE(CvPromotionInfo)
CASCADE_INFOREPO_DEFINE(CvImprovementInfo)
CASCADE_INFOREPO_DEFINE(CvFeatureInfo)
CASCADE_INFOREPO_DEFINE(CvTerrainInfo)
CASCADE_INFOREPO_DEFINE(CvJsonRouteInfo)
CASCADE_INFOREPO_DEFINE(CvProjectInfo)
CASCADE_INFOREPO_DEFINE(CvProcessInfo)
CASCADE_INFOREPO_DEFINE(CvHeritageInfo)
CASCADE_INFOREPO_DEFINE(CvCultureLevelInfo)
CASCADE_INFOREPO_DEFINE(CvBuildInfo)
CASCADE_INFOREPO_DEFINE(CvPropertyInfo)
// ... + the off-table repos (complex traits + the grep-found extras).
CASCADE_INFOREPO_DEFINE(CvComplexTraitTag)
CASCADE_INFOREPO_DEFINE(CvCivilizationInfo)
CASCADE_INFOREPO_DEFINE(CvEraInfo)
CASCADE_INFOREPO_DEFINE(CvHandicapInfo)
CASCADE_INFOREPO_DEFINE(CvSpecialBuildingInfo)
// ... + the uniformity set's remaining tags (owner ruling: every type gets its own CvJson<X>Info subclass).
CASCADE_INFOREPO_DEFINE(CvGameSpeedInfo)
CASCADE_INFOREPO_DEFINE(CvLeaderHeadInfo)
CASCADE_INFOREPO_DEFINE(CvSpecialUnitInfo)
CASCADE_INFOREPO_DEFINE(CvVictoryInfo)
CASCADE_INFOREPO_DEFINE(CvVoteInfo)
CASCADE_INFOREPO_DEFINE(CvHurryInfo)
CASCADE_INFOREPO_DEFINE(CvBonusClassInfo)

#undef CASCADE_INFOREPO_DEFINE
