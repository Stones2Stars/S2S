//
//	CvOutcomesSection -- the shared load-time parse of the json.md par.8 `outcomes` block (see the header):
//	kill[] feeds the CvOutcomeList value member; actions[] news one CvOutcomeMission per row. The CvOutcome
//	engine objects consume the JSON through their own mapFrom (mission-outcome-system.md); this section owns
//	only the intake + the query surface.
//

#include "CvGameCoreDLL.h"   // PCH umbrella -- picojson + SAFE_DELETE
#include "CvOutcomesSection.h"
#include "CvJsonParse.h"     // jsonChildObj
#include "Engine/CvOutcomeMission.h"

CvOutcomesSection::CvOutcomesSection()
{
}

CvOutcomesSection::~CvOutcomesSection()
{
	for (size_t i = 0; i < m_actionMissions.size(); ++i)
	{
		SAFE_DELETE(m_actionMissions[i]);
	}
}

void CvOutcomesSection::clearParsed()
{
	m_killOutcomes.clear();
	for (size_t i = 0; i < m_actionMissions.size(); ++i)
	{
		SAFE_DELETE(m_actionMissions[i]);
	}
	m_actionMissions.clear();
}

void CvOutcomesSection::parse(const picojson::value& entity)
{
	clearParsed();
	if (!entity.is<picojson::object>())
	{
		return;
	}
	const picojson::object& entityObj = entity.get<picojson::object>();
	const picojson::object* pOutcomes = jsonChildObj(entityObj, "outcomes");
	if (pOutcomes == NULL)
	{
		return;
	}
	picojson::object::const_iterator iter = pOutcomes->find("kill");
	if (iter != pOutcomes->end())
	{
		m_killOutcomes.mapFrom(iter->second);
	}
	iter = pOutcomes->find("actions");
	if (iter != pOutcomes->end() && iter->second.is<picojson::array>())
	{
		const picojson::array& actions = iter->second.get<picojson::array>();
		for (size_t i = 0; i < actions.size(); ++i)
		{
			CvOutcomeMission* pMission = new CvOutcomeMission();
			pMission->mapFrom(actions[i]);
			m_actionMissions.push_back(pMission);
		}
	}
}

const CvOutcomeList* CvOutcomesSection::getActionOutcomeList(int iIndex) const
{
	return m_actionMissions[iIndex]->getOutcomeList();
}

MissionTypes CvOutcomesSection::getActionOutcomeMission(int iIndex) const
{
	return m_actionMissions[iIndex]->getMission();
}

const CvOutcomeList* CvOutcomesSection::getActionOutcomeListByMission(MissionTypes eMission) const
{
	for (size_t i = 0; i < m_actionMissions.size(); ++i)
	{
		if (m_actionMissions[i]->getMission() == eMission)
		{
			return m_actionMissions[i]->getOutcomeList();
		}
	}
	return NULL;
}

const CvOutcomeMission* CvOutcomesSection::getOutcomeMissionByMission(MissionTypes eMission) const
{
	for (size_t i = 0; i < m_actionMissions.size(); ++i)
	{
		if (m_actionMissions[i]->getMission() == eMission)
		{
			return m_actionMissions[i];
		}
	}
	return NULL;
}
