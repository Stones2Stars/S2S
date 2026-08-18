//
//	CvVictoryInfo -- the victory poco's own typed reading on top of the base section dispatch (see the header).
//	mapFrom materializes the bespoke §9 `condition` unit + the movie intrinsic ONCE
//	(docs/architecture/patterns.md §Materialize at mapFrom). Idempotent by contract (reset-first unit, unconditional assigns).
//

#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson
#include "CvInfos.h"              // umbrella: keeps the unity batch's info-type defs whole (leakage guard)
#include "AI/CvGameAI.h"
#include "CvVictoryInfo.h"
#include "CvJsonParse.h"          // jsonChildObj / jsonIdInt / jsonIdBool / jsonIdFk / jsonIdStr


CvVictoryInfo::Condition::Condition()
{
	reset();
}


// The unit's full redefinition (the mapFrom idempotency contract, CvInfo.h).
void CvVictoryInfo::Condition::reset()
{
	for (int iFlag = 0; iFlag < NUM_VICTORY_CONDITION_FLAGS; iFlag++)
	{
		flags[iFlag] = false;
	}
	for (int iValue = 0; iValue < NUM_VICTORY_CONDITION_VALUES; iValue++)
	{
		values[iValue] = 0;
	}
	cityCultureLevel = -1;   // NO_CULTURELEVEL -- only the cultural victory authors a level
}


CvVictoryInfo::CvVictoryInfo()
{
}


// EXE-bound (DllExport) -- the closed .exe imports this symbol, so the read stays out-of-line.
bool CvVictoryInfo::isPermanent() const
{
	return m_condition.flags[VICTORY_CONDITION_PERMANENT];
}


// condition.* -> the typed unit (bool rules + numeric thresholds; cityCulture is a CULTURELEVEL_* FK,
// absent -> -1); the victory movie is ui.art.movie.file (the base consumes only ui.art.icon).
void CvVictoryInfo::mapFrom(const picojson::value& entity)
{
	CvInfo::mapFrom(entity);   // core reading (type / text keys / button) + the section dispatch

	// idempotency (CvInfo.h): the full-registry re-run fully redefines every materialized member
	m_condition.reset();
	m_szMovie.clear();

	if (!entity.is<picojson::object>())
	{
		return;
	}
	const picojson::object& entityObj = entity.get<picojson::object>();

	if (const picojson::object* pCondition = jsonChildObj(entityObj, "condition"))
	{
		m_condition.flags[VICTORY_CONDITION_CONQUEST]      = jsonIdBool(*pCondition, "conquest");
		m_condition.flags[VICTORY_CONDITION_DIPLO_VOTE]    = jsonIdBool(*pCondition, "diploVote");
		m_condition.flags[VICTORY_CONDITION_TARGET_SCORE]  = jsonIdBool(*pCondition, "targetScore");
		m_condition.flags[VICTORY_CONDITION_END_SCORE]     = jsonIdBool(*pCondition, "endScore");
		m_condition.flags[VICTORY_CONDITION_PERMANENT]     = jsonIdBool(*pCondition, "permanent");
		m_condition.flags[VICTORY_CONDITION_TOTAL_VICTORY] = jsonIdBool(*pCondition, "totalVictory");
		m_condition.values[VICTORY_CONDITION_LAND_PERCENT]            = jsonIdInt(*pCondition, "landPercent");
		m_condition.values[VICTORY_CONDITION_MIN_LAND_PERCENT]        = jsonIdInt(*pCondition, "minLandPercent");
		m_condition.values[VICTORY_CONDITION_POPULATION_PERCENT_LEAD] = jsonIdInt(*pCondition, "populationPercentLead");
		m_condition.values[VICTORY_CONDITION_RELIGION_PERCENT]        = jsonIdInt(*pCondition, "religionPercent");
		m_condition.values[VICTORY_CONDITION_NUM_CULTURE_CITIES]      = jsonIdInt(*pCondition, "numCultureCities");
		m_condition.values[VICTORY_CONDITION_DELAY_TURNS]             = jsonIdInt(*pCondition, "delayTurns");
		m_condition.cityCultureLevel = jsonIdFk(*pCondition, "cityCulture");
	}

	// ui.art.movie.file -- the victory movie art
	if (const picojson::object* pUi = jsonChildObj(entityObj, "ui"))
	{
		if (const picojson::object* pArt = jsonChildObj(*pUi, "art"))
		{
			if (const picojson::object* pMovie = jsonChildObj(*pArt, "movie"))
			{
				jsonIdStr(*pMovie, "file", m_szMovie);
			}
		}
	}
}
