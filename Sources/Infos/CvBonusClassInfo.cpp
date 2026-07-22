//------------------------------------------------------------------------------------------------
//  FILE:    CvBonusClassInfo.cpp
//------------------------------------------------------------------------------------------------
#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson
#include "CvInfos.h"              // umbrella: keeps the unity batch's info-type defs whole (leakage guard)
#include "AI/CvGameAI.h"
#include "CvBonusClassInfo.h"
#include "CvJsonParse.h"          // jsonChildObj / jsonIdInt


CvBonusClassInfo::CvBonusClassInfo()
	: m_iUniqueRange(0)
{
}


// #430: mapGeneration.uniqueRange -> iUniqueRange (raw int; absent -> 0). Load-bearing in map gen
// (CvMapGenerator: min-spacing that prevents same-class bonus stacking).
void CvBonusClassInfo::mapFrom(const picojson::value& entity)
{
	CvInfo::mapFrom(entity);   // core reading (type / text keys) + availability
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();

	if (const picojson::object* mg = jsonChildObj(o, "mapGeneration"))
		m_iUniqueRange = jsonIdInt(*mg, "uniqueRange");
}
