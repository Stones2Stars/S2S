//  $Header:
//------------------------------------------------------------------------------------------------
//
//  FILE:	CvOutcomeMission.cpp
//
//  PURPOSE: A mission that has a cost and a result depending on an outcome list
//
//------------------------------------------------------------------------------------------------

#include "Tools/FProfiler.h"

#include "CvGameCoreDLL.h"
#include "CvGameObject.h"
#include "Defines/CvGlobals.h"
#include "CvOutcomeMission.h"
#include "AI/CvPlayerAI.h"
#include "CvUnit.h"
#include "Infrastructure/CvXMLLoadUtility.h"
#include "Tools/CheckSum.h"
#include "Infrastructure/IntExpr.h"
#include "Infrastructure/BoolExpr.h"
#include "Infos/CvJsonParse.h"   // jsonIdStr

CvOutcomeMission::CvOutcomeMission() :
m_eMission(NO_MISSION),
m_iCost(NULL),
m_bKill(true),
m_ePayerType(NO_GAMEOBJECT),
m_pPlotCondition(NULL),
m_pUnitCondition(NULL)
{
}

//	ONE outcomes.actions[] entry -> this outcome-mission. The entry names its MISSION_*, and its outcome payload
//	arrives one of two ways ([json.md] par.8): an explicit `outcomes` LIST when the action can resolve several
//	ways, or the entry ITSELF as a single inline outcome. CvOutcomeList::mapFrom accepts both shapes, so the
//	choice is a pass-through rather than a branch in the payload reading.
//	⚠ `constructs` is deliberately NOT read here: it is the MISSION_CONSTRUCT hardcoded ability's building, and
//	CvOutcome carries no building payload -- that ability reads its own per-unit has-building surface.
void CvOutcomeMission::mapFrom(const picojson::value& v)
{
	if (!v.is<picojson::object>())
	{
		return;
	}
	const picojson::object& o = v.get<picojson::object>();

	std::string szMission;
	if (jsonIdStr(o, "mission", szMission))
	{
		m_eMission = static_cast<MissionTypes>(GC.getInfoTypeForString(szMission.c_str(), true));
	}

	picojson::object::const_iterator itOutcomes = o.find("outcomes");
	if (itOutcomes != o.end())
	{
		m_OutcomeList.mapFrom(itOutcomes->second);
	}
	else if (o.find("requires") != o.end())
	{
		m_OutcomeList.mapFrom(v);
	}

	// the mission's own consume flag defaults TRUE (an outcome mission spends its unit unless it says otherwise)
	picojson::object::const_iterator itConsumes = o.find("consumes");
	if (itConsumes != o.end() && itConsumes->second.is<bool>())
	{
		m_bKill = itConsumes->second.get<bool>();
	}
}

CvOutcomeMission::~CvOutcomeMission()
{
	GC.removeDelayedResolution((int*)&m_eMission);
	SAFE_DELETE(m_iCost);
	SAFE_DELETE(m_pPlotCondition);
	SAFE_DELETE(m_pUnitCondition);
}

//const IntExpr* CvOutcomeMission::getCost() const
//{
//	return m_iCost;
//}

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
	//if (!bTestVisible)
	//{
		if (m_iCost)
		{
			if (GET_PLAYER(pUnit->getOwner()).getGold() < m_iCost->evaluate(pUnit->getGameObject()))
			{
				return false;
			}
		}

		if (m_pPlotCondition)
			if (!m_pPlotCondition->evaluate(pUnit->plot()->getGameObject()))
				return false;
	//}

	if (m_pUnitCondition)
		if (!m_pUnitCondition->evaluate(pUnit->getGameObject()))
			return false;

	if (!getOutcomeList()->isPossible(*pUnit))
	{
		return false;
	}

	//if (!bTestVisible)
	//{
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
	//}

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

	if (m_iCost)
	{
		if (m_iCost->evaluate(pUnit->getGameObject()) != 0)
		{	/*GC.getGame().getGameObject()->adaptValueToGame(m_iID, m_pExpr->evaluate(GC.getGame().getGameObject())*/
			CvWString szTempBuffer;

			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_COST"));
			m_iCost->buildDisplayString(szBuffer);
		}
	}

	if (m_pPlotCondition)
	{
		if (!m_pPlotCondition->evaluate(pUnit->plot()->getGameObject()))
		{
			szBuffer.append(gDLL->getText("TXT_KEY_REQUIRES"));
			m_pPlotCondition->buildDisplayString(szBuffer);
		}
	}

	if (m_pUnitCondition)
	{
		if (!m_pUnitCondition->evaluate(pUnit->getGameObject()))
		{
			szBuffer.append(gDLL->getText("TXT_KEY_REQUIRES"));
			m_pUnitCondition->buildDisplayString(szBuffer);
		}
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
	if (m_iCost)
	{
		GET_PLAYER(pUnit->getOwner()).changeGold(-(m_iCost->evaluate(pUnit->getGameObject())));
	}

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

void CvOutcomeMission::getCheckSum(uint32_t& iSum) const
{
	CheckSum(iSum, m_eMission);
	CheckSum(iSum, m_bKill);
	CheckSum(iSum, m_ePayerType);
	m_PropertyCost.getCheckSum(iSum);
	m_OutcomeList.getCheckSum(iSum);
	if (m_iCost)
	{
		m_iCost->getCheckSum(iSum);
	}
}
