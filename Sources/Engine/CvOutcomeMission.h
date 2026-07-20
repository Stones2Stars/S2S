//------------------------------------------------------------------------------------------------
//
//  FILE:    CvOutcomeMission.h
//
//  PURPOSE: A mission that has a result depending on an outcome list
//
//------------------------------------------------------------------------------------------------
#pragma once

#ifndef CV_OUTCOME_MISSION_H
#define CV_OUTCOME_MISSION_H

#include "UI/CvOutcomeList.h"
#include "CvProperties.h"

class CvUnit;
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
	GameObjectTypes getPayerType() const;

	bool isPossible(const CvUnit* pUnit, bool bTestVisible = false) const;
	void buildDisplayString(CvWStringBuffer& szBuffer, const CvUnit* pUnit) const;
	void execute(CvUnit* pUnit) const;

	void mapFrom(const picojson::value& v);   // #430: build from an outcomes.actions[] entry {mission, consumes?, <outcome(s)>}

	void getCheckSum(uint32_t& iSum) const;

protected:
	MissionTypes m_eMission;
	CvOutcomeList m_OutcomeList;
	CvProperties m_PropertyCost;
	GameObjectTypes m_ePayerType;
	bool m_bKill;
};

#endif
