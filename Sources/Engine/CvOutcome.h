#pragma once

//  $Header:
//------------------------------------------------------------------------------------------------
//
//  FILE:    CvOutcome.h
//
//  PURPOSE: A single outcome from an outcome list
//
//------------------------------------------------------------------------------------------------
#ifndef CV_OUTCOME_H
#define CV_OUTCOME_H

class CvPlayerAI;
class CvPlot;
class CvProperties;
class CvUnit;
class CvXMLLoadUtility;
class IntExpr;
class CvJsonCondition;                 // #430: conditions are cascade CvJsonCondition trees (eval via cascadeEvalCondition), not BoolExpr
namespace picojson { class value; }    // mapFrom -- the JSON intake (replaces the XML read path)

class CvOutcome
{
public:
	CvOutcome();
	virtual ~CvOutcome();
	int getYield(YieldTypes eYield, const CvUnit& kUnit) const;
	int getCommerce(CommerceTypes eCommerce, const CvUnit& kUnit) const;
	int getChance(const CvUnit& kUnit) const;
	OutcomeTypes getType() const;
	UnitTypes getUnitType() const;
	bool getUnitToCity(const CvUnit& kUnit) const;
	PromotionTypes getPromotionType() const;
	int getGPP() const;
	UnitTypes getGPUnitType() const;
	BonusTypes getBonusType() const;
	const CvProperties* getProperties() const;
	int getHappinessTimer() const;
	int getPopulationBoost() const;
	int getReduceAnarchyLength(const CvUnit& kUnit) const;
	EventTriggerTypes getEventTrigger() const;
	int getChancePerPop() const;
	bool isKill() const;
	void compilePython();

	bool isPossible(const CvUnit& kUnit) const;
	bool isPossibleSomewhere(const CvUnit& kUnit) const;
	bool isPossibleInPlot(const CvUnit& kUnit, const CvPlot& kPlot, bool bForTrade = false) const;
	bool isPossible(const CvPlayerAI& kPlayer) const;
	bool execute(CvUnit& kUnit, PlayerTypes eDefeatedUnitPlayer = NO_PLAYER, UnitTypes eDefeatedUnitType = NO_UNIT) const;

	int AI_getValueInPlot(const CvUnit& kUnit, const CvPlot& kPlot, bool bForTrade = false) const;

	void buildDisplayString(CvWStringBuffer& szBuffer, const CvUnit& kUnit) const;

	void mapFrom(const picojson::value& v);   // #430: JSON intake of ONE outcome entry (outcomes.kill[]/actions[]); XML read path is dead
	void getCheckSum(uint32_t& iSum) const;

protected:
	OutcomeTypes m_eType;
	const IntExpr* m_iChance;
	int m_iChancePerPop;
	const IntExpr* m_aiYield[NUM_YIELD_TYPES];
	const IntExpr* m_aiCommerce[NUM_COMMERCE_TYPES];
	UnitTypes m_eUnitType;
	bool m_bToCity;                          // spawns.toCity present (send the spawned unit to a city)
	const CvJsonCondition* m_pToCityCond;    // spawns.toCity gate (NULL = unconditional to-city; e.g. a tech requirement)
	PromotionTypes m_ePromotionType;
	BonusTypes m_eBonusType;
	int m_iGPP;
	UnitTypes m_eGPUnitType;
	CvProperties m_Properties;
	int m_iHappinessTimer;
	int m_iPopulationBoost;
	const IntExpr* m_iReduceAnarchyLength;
	EventTriggerTypes m_eEventTrigger;
	const CvJsonCondition* m_pPlotCondition;   // requires.plot -- cascade condition, eval via cascadeEvalCondition
	const CvJsonCondition* m_pUnitCondition;   // requires.unit
	CvString m_szPlotPythonGate;               // requires.plot {python:fn} -- a Python-authoritative plot gate (called, not cascade-eval'd)
	CvString m_szUnitPythonGate;               // requires.unit {python:fn}
	CvString m_szPythonCallback;
	bool m_bKill;
	CvString m_szPythonCode;
	CvString m_szPythonModuleName;
	PyObject* m_pPythonPossibleFunc;
	PyObject* m_pPythonExecFunc;
	PyObject* m_pPythonDisplayFunc;
	PyObject* m_pPythonAIFunc;
};

#endif