//  $Header:
//------------------------------------------------------------------------------------------------
//
//  FILE:	CvOutcomeMission.cpp
//
//  PURPOSE: A mission that has a result depending on an outcome list
//
//------------------------------------------------------------------------------------------------

#include "Tools/FProfiler.h"

#include "CvGameCoreDLL.h"
#include "CvGameObject.h"
#include "Defines/CvGlobals.h"
#include "CvOutcomeMission.h"
#include "AI/CvPlayerAI.h"
#include "CvUnit.h"
#include "Tools/CheckSum.h"
#include "CvJsonParse.h"   // jsonResolveId -- the MISSION_ FK

CvOutcomeMission::CvOutcomeMission() :
m_eMission(NO_MISSION),
m_bKill(true),
m_ePayerType(NO_GAMEOBJECT)
{
}

CvOutcomeMission::~CvOutcomeMission()
{
	GC.removeDelayedResolution((int*)&m_eMission);
}

MissionTypes CvOutcomeMission::getMission() const
{
	return m_eMission;
}

const CvOutcomeList* CvOutcomeMission::getOutcomeList() const
{
	return &m_OutcomeList;
}

const CvProperties* CvOutcomeMission::getPropertyCost() const
{
	return &m_PropertyCost;
}

bool CvOutcomeMission::isKill() const
{
	return m_bKill;
}

GameObjectTypes CvOutcomeMission::getPayerType() const
{
	return m_ePayerType;
}

void callSetPayer(const CvGameObject* pObject, const CvGameObject** ppPayer)
{
	*ppPayer = pObject;
}

bool CvOutcomeMission::isPossible(const CvUnit* pUnit, bool bTestVisible) const
{
	PROFILE_EXTRA_FUNC();
	if (!getOutcomeList()->isPossible(*pUnit))
	{
		return false;
	}

	if (!getPropertyCost()->isEmpty())
	{
		const CvGameObject* pPayer = NULL;
		if ((m_ePayerType == NO_GAMEOBJECT) || (m_ePayerType == GAMEOBJECT_UNIT))
		{
			pPayer = pUnit->getGameObject();
		}
		else
		{
			pUnit->getGameObject()->foreach(m_ePayerType, bind(callSetPayer, _1, &pPayer));
		}

		if (!pPayer)
		{
			return false;
		}

		if (! (*(pPayer->getProperties()) > m_PropertyCost ))
		{
			return false;
		}
	}

	return true;
}

void CvOutcomeMission::buildDisplayString(CvWStringBuffer& szBuffer, const CvUnit* pUnit) const
{
	if (!m_PropertyCost.isEmpty())
	{
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_COST"));
		m_PropertyCost.buildCompactChangesString(szBuffer);
	}

	if (m_bKill)
	{
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_ACTION_CONSUME_UNIT"));
	}

	m_OutcomeList.buildDisplayString(szBuffer, *pUnit);
}

void CvOutcomeMission::execute(CvUnit* pUnit) const
{
	PROFILE_EXTRA_FUNC();
	m_OutcomeList.execute(*pUnit);

	if (!getPropertyCost()->isEmpty())
	{
		const CvGameObject* pPayer = NULL;
		if ((m_ePayerType == NO_GAMEOBJECT) || (m_ePayerType == GAMEOBJECT_UNIT))
		{
			pPayer = pUnit->getGameObject();
		}
		else
		{
			pUnit->getGameObject()->foreach(m_ePayerType, bind(callSetPayer, _1, &pPayer));
		}

		if (pPayer)
		{
			pPayer->getProperties()->subtractProperties(&m_PropertyCost);
		}
	}

	pUnit->finishMoves();
}

// #430: JSON intake of one action `{ mission, consumes?, requires/chance/<rewards> | outcomes:[...] }`. The lone-
// outcome case is inlined onto the action (curator), so the whole object maps as the single outcome; multiple ride
// an `outcomes` array. `consumes` defaults TRUE (the legacy CvOutcomeMission bKill default). The XML read() is dead.
void CvOutcomeMission::mapFrom(const picojson::value& v)
{
	if (!v.is<picojson::object>()) return;
	const picojson::object& o = v.get<picojson::object>();
	picojson::object::const_iterator it = o.find("mission");
	if (it != o.end() && it->second.is<std::string>())
		m_eMission = (MissionTypes)jsonResolveId(it->second.get<std::string>());
	it = o.find("consumes");
	m_bKill = (it != o.end() && it->second.is<bool>()) ? it->second.get<bool>() : true;
	it = o.find("outcomes");
	if (it != o.end())
		m_OutcomeList.mapFrom(it->second);   // several weighted outcomes
	else
		m_OutcomeList.mapFrom(v);            // lone outcome inlined on the action (mission/consumes keys ignored by CvOutcome::mapFrom)
}

void CvOutcomeMission::getCheckSum(uint32_t& iSum) const
{
	CheckSum(iSum, m_eMission);
	CheckSum(iSum, m_bKill);
	CheckSum(iSum, m_ePayerType);
	m_PropertyCost.getCheckSum(iSum);
	m_OutcomeList.getCheckSum(iSum);
}
