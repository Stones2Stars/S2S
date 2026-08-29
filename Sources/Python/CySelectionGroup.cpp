#include "CvGameCoreDLL.h"
#include "Engine/CvSelectionGroup.h"
#include "CyArea.h"
#include "CyPlot.h"
#include "CySelectionGroup.h"
#include "CyUnit.h"

//
// Python wrapper class for CvSelectionGroup
//

CySelectionGroup::CySelectionGroup(CvSelectionGroup* pSelectionGroup) : m_pSelectionGroup(pSelectionGroup)
{
	FAssert(m_pSelectionGroup != NULL);
}

bool CySelectionGroup::isHuman() const
{
	return m_pSelectionGroup->isHuman();
}

bool CySelectionGroup::isFull() const
{
	return m_pSelectionGroup->isFull();
}

bool CySelectionGroup::canFight() const
{
	return m_pSelectionGroup->canFight();
}

CyPlot* CySelectionGroup::plot() const
{
	if (m_pSelectionGroup->plot())
	{
		return new CyPlot(m_pSelectionGroup->plot());
	}
	return NULL;
}

int CySelectionGroup::getID() const
{
	return m_pSelectionGroup->getID();
}

int /*PlayerTypes*/ CySelectionGroup::getOwner() const
{
	return m_pSelectionGroup->getOwner();
}

int /*TeamTypes*/ CySelectionGroup::getTeam() const
{
	return m_pSelectionGroup->getTeam();
}

int /*ActivityTypes*/ CySelectionGroup::getActivityType() const
{
	return m_pSelectionGroup->getActivityType();
}

void CySelectionGroup::setActivityType(int /*ActivityTypes*/ eNewValue)
{
	m_pSelectionGroup->setActivityType((ActivityTypes) eNewValue);
}

int /*AutomateTypes*/ CySelectionGroup::getAutomateType() const
{
	return m_pSelectionGroup->getAutomateType();
}

int CySelectionGroup::getNumUnits() const
{
	return m_pSelectionGroup->getNumUnits();
}

int CySelectionGroup::getLengthMissionQueue() const
{
	return m_pSelectionGroup->getLengthMissionQueue();
}

CyUnit* CySelectionGroup::getHeadUnit() const
{
	return new CyUnit(m_pSelectionGroup->getHeadUnit());
}

int CySelectionGroup::getMissionType(int iNode) const
{
	return m_pSelectionGroup->getMissionType(iNode);
}

int CySelectionGroup::getMissionData1(int iNode) const
{
	return m_pSelectionGroup->getMissionData1(iNode);
}
