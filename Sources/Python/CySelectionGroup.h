#pragma once

#ifndef CySelectionGroup_h__
#define CySelectionGroup_h__

//
// Python wrapper class for CvSelectionGroup
//

class CvSelectionGroup;
class CyPlot;
class CyArea;
class CyUnit;

class CySelectionGroup
{
public:
	explicit CySelectionGroup(CvSelectionGroup* pSelectionGroup);		// Call from C++

	//CvSelectionGroup* getSelectionGroup() const { return m_pSelectionGroup; }	// Call from C++


	bool isHuman() const;
	bool isFull() const;
	bool canFight() const;

	CyPlot* plot() const;
	CyArea* area() const;


	int getID() const;
	int /*PlayerTypes*/ getOwner() const;
	int /*TeamTypes*/ getTeam() const;
	int /*ActivityTypes*/ getActivityType() const;
	void setActivityType(int /*ActivityTypes*/ eNewValue);
	int /*AutomateTypes*/ getAutomateType() const;

	int getNumUnits() const;
	int getLengthMissionQueue() const;
	int getMissionType(int iNode) const;
	int getMissionData1(int iNode) const;
	CyUnit* getHeadUnit() const;

protected:
	CvSelectionGroup* m_pSelectionGroup;
};

DECLARE_PY_WRAPPER(CySelectionGroup, CvSelectionGroup*);

#endif // CySelectionGroup_h__