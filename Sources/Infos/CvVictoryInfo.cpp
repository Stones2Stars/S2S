//------------------------------------------------------------------------------------------------
//  FILE:    CvVictoryInfo.cpp
//------------------------------------------------------------------------------------------------
#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson
#include "CvInfos.h"              // umbrella: keeps the unity batch's info-type defs whole (leakage guard)
#include "AI/CvGameAI.h"
#include "CvVictoryInfo.h"
#include "CvJsonParse.h"          // jsonChildObj / jsonIdInt / jsonIdBool / jsonIdStr / jsonResolveId


CvVictoryInfo::CvVictoryInfo()
	: m_iPopulationPercentLead(0)
	, m_iLandPercent(0)
	, m_iMinLandPercent(0)
	, m_iReligionPercent(0)
	, m_iCityCulture(-1)          // NO_CULTURELEVEL -- only the cultural victory sets a real level
	, m_iNumCultureCities(0)
	, m_iTotalCultureRatio(0)     // dormant (no JSON authors it) -- stays 0
	, m_iVictoryDelayTurns(0)
	, m_bTargetScore(false)
	, m_bEndScore(false)
	, m_bConquest(false)
	, m_bDiploVote(false)
	, m_bPermanent(false)
	, m_bTotalVictory(false)
{
}


bool CvVictoryInfo::isPermanent() const
{
	return m_bPermanent;
}


// #430: condition.* -> the scoring thresholds + bool flags; condition.cityCulture is a CULTURELEVEL_ FK string
// (resolve to id; absent -> NO_CULTURELEVEL); the victory movie is ui.art.movie.file.
void CvVictoryInfo::mapFrom(const picojson::value& entity)
{
	CvInfo::mapFrom(entity);   // core reading (type / text keys) + availability
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();

	if (const picojson::object* c = jsonChildObj(o, "condition"))
	{
		m_iPopulationPercentLead = jsonIdInt(*c, "populationPercentLead");
		m_iLandPercent           = jsonIdInt(*c, "landPercent");
		m_iMinLandPercent        = jsonIdInt(*c, "minLandPercent");
		m_iReligionPercent       = jsonIdInt(*c, "religionPercent");
		m_iNumCultureCities      = jsonIdInt(*c, "numCultureCities");
		m_iVictoryDelayTurns     = jsonIdInt(*c, "delayTurns");
		m_bTargetScore  = jsonIdBool(*c, "targetScore");
		m_bEndScore     = jsonIdBool(*c, "endScore");
		m_bConquest     = jsonIdBool(*c, "conquest");
		m_bDiploVote    = jsonIdBool(*c, "diploVote");
		m_bPermanent    = jsonIdBool(*c, "permanent");
		m_bTotalVictory = jsonIdBool(*c, "totalVictory");
		std::string cc;
		if (jsonIdStr(*c, "cityCulture", cc)) m_iCityCulture = jsonResolveId(cc);
	}

	if (const picojson::object* ui = jsonChildObj(o, "ui"))
		if (const picojson::object* art = jsonChildObj(*ui, "art"))
			if (const picojson::object* mv = jsonChildObj(*art, "movie"))
			{
				std::string f;
				if (jsonIdStr(*mv, "file", f)) m_szMovie = f.c_str();
			}
}
