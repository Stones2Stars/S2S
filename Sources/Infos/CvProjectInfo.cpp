//
//	CvProjectInfo -- the project poco's own typed reading on top of the base section dispatch (see the header).
//	mapFrom materializes the census identity set + the bespoke `victory` block ONCE
//	(docs/architecture/patterns.md §Materialize at mapFrom); every modifier magnitude is a compiled point read (header), never a mirrored
//	scalar. Idempotent by contract (unconditional assigns, clear-first containers).
//

#include "CvGameCoreDLL.h"
#include "CvProjectInfo.h"
#include "CvJsonParse.h"   // jsonChildObj / jsonIdInt / jsonIdBool / jsonIdFk / jsonIdStr / jsonReadFkMap / jsonResolveId

CvProjectInfo::CvProjectInfo()
	: m_iProductionCost(0)
	, m_bSpaceship(false)
	, m_eLaunchesVictory(-1)
	, m_iVictoryDelayPercent(0)
	, m_iSuccessRate(0)
	, m_eTechPrereq(NO_TECH)
{
}

void CvProjectInfo::mapFrom(const picojson::value& entity)
{
	CvInfo::mapFrom(entity);   // core reading + the section dispatch (compiles m_modifiers, fills edges/allowed)

	// idempotency (CvInfo.h): the full-registry re-run fully redefines every materialized member
	// (the reverse-pass-fed tech FK + needed-projects set are NOT reset -- written after the last mapFrom)
	m_aeMapCategories.clear();
	m_victoryThreshold.clear();
	m_victoryMinThreshold.clear();
	m_iProductionCost = 0;
	m_bSpaceship = false;
	m_eLaunchesVictory = -1;
	m_iVictoryDelayPercent = 0;
	m_iSuccessRate = 0;
	m_szMovieDefineTag.clear();

	if (!entity.is<picojson::object>())
	{
		return;
	}
	const picojson::object& entityObj = entity.get<picojson::object>();

	if (const picojson::object* pCost = jsonChildObj(entityObj, "cost"))
	{
		m_iProductionCost = jsonIdInt(*pCost, "create");
	}

	// bespoke `victory` launch params (json §9)
	if (const picojson::object* pVictory = jsonChildObj(entityObj, "victory"))
	{
		jsonReadFkMap(*pVictory, "thresholds", m_victoryThreshold);
		jsonReadFkMap(*pVictory, "minThresholds", m_victoryMinThreshold);
		m_iVictoryDelayPercent = jsonIdInt(*pVictory, "delayPercent");
		m_iSuccessRate = jsonIdInt(*pVictory, "successRate");
	}

	if (const picojson::object* pIdentity = jsonChildObj(entityObj, "identity"))
	{
		m_bSpaceship = jsonIdBool(*pIdentity, "spaceship");
		m_eLaunchesVictory = jsonIdFk(*pIdentity, "launchesVictory");
		picojson::object::const_iterator categoriesIt = pIdentity->find("mapCategories");
		if (categoriesIt != pIdentity->end() && categoriesIt->second.is<picojson::array>())
		{
			const picojson::array& categories = categoriesIt->second.get<picojson::array>();
			for (size_t iCategory = 0; iCategory < categories.size(); ++iCategory)
			{
				if (categories[iCategory].is<std::string>())
				{
					const int iCategoryId = jsonResolveId(categories[iCategory].get<std::string>());
					if (iCategoryId >= 0)
					{
						m_aeMapCategories.push_back((MapCategoryTypes)iCategoryId);
					}
				}
			}
		}
	}

	// ui.art.movie.defineTag -- the completion movie art tag
	if (const picojson::object* pUi = jsonChildObj(entityObj, "ui"))
	{
		if (const picojson::object* pArt = jsonChildObj(*pUi, "art"))
		{
			if (const picojson::object* pMovie = jsonChildObj(*pArt, "movie"))
			{
				jsonIdStr(*pMovie, "defineTag", m_szMovieDefineTag);
			}
		}
	}
}
