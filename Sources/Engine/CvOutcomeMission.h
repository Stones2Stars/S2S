//------------------------------------------------------------------------------------------------
//
//  FILE:    CvOutcomeMission.h
//
//  PURPOSE: A mission that has a cost and a result depending on an outcome list
//
//------------------------------------------------------------------------------------------------
#pragma once

#ifndef CV_OUTCOME_MISSION_H
#define CV_OUTCOME_MISSION_H

#include "UI/CvOutcomeList.h"
#include "CvProperties.h"

class CvUnit;
class CvXMLLoadUtility;
class BoolExpr;
class IntExpr;
namespace picojson { class value; }

class CvOutcomeMission
{
public:
	CvOutcomeMission();
	~CvOutcomeMission();
	MissionTypes getMission() const;
	const CvOutcomeList* getOutcomeList() const;
	const CvProperties* getPropertyCost() const;
	bool isKill() const;
//	const IntExpr* getCost() const;
	GameObjectTypes getPayerType() const;

	/// <summary>Which of isPossible's independent gates refused. Diagnostic only -- the gates have completely
	/// different causes, so a bare false cannot be acted on.</summary>
	enum OutcomeMissionLeg
	{
		OUTCOMEMISSION_LEG_NONE = 0,        // passed
		OUTCOMEMISSION_LEG_GOLD,            // the owner cannot afford m_iCost
		OUTCOMEMISSION_LEG_PLOT_CONDITION,  // m_pPlotCondition refused the unit's plot
		OUTCOMEMISSION_LEG_UNIT_CONDITION,  // m_pUnitCondition refused the unit
		OUTCOMEMISSION_LEG_OUTCOME_LIST,    // no outcome in the list is possible (CvOutcome's own gates)
		OUTCOMEMISSION_LEG_PROPERTY_COST    // the payer cannot meet m_PropertyCost
	};

	/// <summary>May this unit take the mission now? `piRefusalLeg` is OPTIONAL and diagnostic: when non-NULL it
	/// receives the OutcomeMissionLeg that refused. Every gameplay caller passes NULL and is unaffected.</summary>
	bool isPossible(const CvUnit* pUnit, bool bTestVisible = false, int* piRefusalLeg = NULL) const;
	void buildDisplayString(CvWStringBuffer& szBuffer, const CvUnit* pUnit) const;
	void execute(CvUnit* pUnit) const;


	// The JSON intake -- ONE outcomes.actions[] entry (mission-outcome-system.md).
	void mapFrom(const picojson::value& v);

	void getCheckSum(uint32_t& iSum) const;

protected:
	MissionTypes m_eMission;
	CvOutcomeList m_OutcomeList;
	CvProperties m_PropertyCost;
	GameObjectTypes m_ePayerType;
	bool m_bKill;
	const IntExpr* m_iCost;
	const BoolExpr* m_pPlotCondition;
	const BoolExpr* m_pUnitCondition;
};

#endif