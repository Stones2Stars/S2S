//
//	CvBonusClassInfo -- pure intrinsic self-description (see the header). mapFrom reads the one
//	mapGeneration config value; idempotent by contract (a plain scalar assign).
//

#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson
#include "CvInfos.h"              // umbrella: keeps the unity batch's info-type defs whole (leakage guard)
#include "AI/CvGameAI.h"
#include "CvBonusClassInfo.h"
#include "CvJsonParse.h"          // jsonChildObj / jsonIdInt

CvBonusClassInfo::CvBonusClassInfo()
	: m_iUniqueRange(0)
{
}

// mapGeneration.uniqueRange -> the min-spacing config (raw int; absent -> 0). Load-bearing in map gen
// (CvMapGenerator: prevents same-class bonus stacking).
void CvBonusClassInfo::mapFrom(const picojson::value& entity)
{
	CvInfo::mapFrom(entity);   // core reading (type / text keys) + availability
	if (!entity.is<picojson::object>())
	{
		return;
	}
	const picojson::object& entityObj = entity.get<picojson::object>();

	if (const picojson::object* pMapGen = jsonChildObj(entityObj, "mapGeneration"))
	{
		m_iUniqueRange = jsonIdInt(*pMapGen, "uniqueRange");
	}
}
