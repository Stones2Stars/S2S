//  $Header:
//------------------------------------------------------------------------------------------------
//
//  FILE:	CvOutcome.cpp
//
//  PURPOSE: A single outcome from an outcome list
//
//------------------------------------------------------------------------------------------------

#include "Tools/FProfiler.h"

#include "CvGameCoreDLL.h"
#include "CvCity.h"
#include "AI/CvCityAI.h"
#include "Defines/CvGlobals.h"
#include "CvBonusInfo.h"
#include "CvInfos.h"
#include "CvMap.h"
#include "CvOutcome.h"
#include "CvProperties.h"
#include "AI/CvPlayerAI.h"
#include "CvPlot.h"
#include "Infrastructure/CvPython.h"
#include "AI/CvTeamAI.h"
#include "CvUnit.h"
#include "Infrastructure/CvXMLLoadUtility.h"
#include "Python/CyUnit.h"
#include "Python/CyPlot.h"
#include "Tools/CheckSum.h"
#include "AI/CvGameAI.h"
#include "Infrastructure/IntExpr.h"
#include "CvJsonCondition.h"            // #430: cascade condition tree (replaces the BoolExpr gates)
#include "CvJsonConditionParse.h"       // cascadeParseCondition -- JSON -> CvJsonCondition
#include "CvJsonParse.h"                 // jsonResolveId / jsonChildObj / picojson helpers
#include "Cascade/CvCascadeConditionEval.h"       // cascadeEvalCondition + CvCascadeEvalCtx

namespace
{
	// A clean outcome IntExpr value (json: `int` | `{base, random}`) -> the legacy IntExpr the getters evaluate.
	// `Adapt*` is GONE from the data (pure-engine gamespeed scaling applied downstream); only Constant / Plus(Constant,Random) remain.
	const IntExpr* jsonToIntExpr(const picojson::value& v)
	{
		if (v.is<double>())
			return new IntExprConstant((int)v.get<double>());
		if (v.is<picojson::object>())
		{
			const picojson::object& o = v.get<picojson::object>();
			picojson::object::const_iterator b = o.find("base"), r = o.find("random");
			const int iBase   = (b != o.end() && b->second.is<double>()) ? (int)b->second.get<double>() : 0;
			const int iRandom = (r != o.end() && r->second.is<double>()) ? (int)r->second.get<double>() : 0;
			if (iRandom != 0)
				return new IntExprPlus(new IntExprConstant(iBase), new IntExprRandom(new IntExprConstant(iRandom)));
			return new IntExprConstant(iBase);
		}
		return NULL;
	}

	// The eval context for an outcome condition: the acting unit + (optionally) the TARGET plot, with the derived
	// city/player/team. Mirrors the C# (EvalState, PlotContext?) the ConditionEvaluator reads.
	CvCascadeEvalCtx outcomeEvalCtx(const CvUnit& kUnit, const CvPlot* pPlot)
	{
		CvCascadeEvalCtx ec;
		ec.unit   = &kUnit;
		ec.player = &GET_PLAYER(kUnit.getOwner());
		ec.team   = &GET_TEAM(kUnit.getTeam());
		ec.plot   = pPlot;
		ec.city   = pPlot ? pPlot->getPlotCity() : NULL;
		return ec;
	}

	bool evalCond(const CvJsonCondition* pCond, const CvUnit& kUnit, const CvPlot* pPlot)
	{
		if (pCond == NULL) return true;   // vacuous
		const CvCascadeEvalCtx ec = outcomeEvalCtx(kUnit, pPlot);
		const CvCascadeEvalFlags flags;
		return cascadeEvalCondition(pCond, ec, flags);
	}

	// A Python-authoritative gate (`requires.plot/unit: {python: fn}`, from the legacy `Greater(Python(fn),0)` plot
	// BoolExpr). Python-authoritative gameplay stays Python (owner): call fn(unit, plot) via the outcome interface,
	// hold iff result > 0. Empty = no gate. (~11 domesticate/captive units; module mirrors the m_szPythonCallback path.)
	bool evalPyGate(const CvString& szFunc, const CvUnit& kUnit)
	{
		if (szFunc.empty()) return true;
		return Cy::call<int>("CvOutcomeInterface", szFunc,
			Cy::Args() << const_cast<CvUnit*>(&kUnit) << const_cast<CvPlot*>(kUnit.plot())) > 0;
	}

	// A requires.plot / requires.unit gate: `{python: fn}` -> the Python-gate string; anything else -> a parsed
	// cascade CvJsonCondition (caller owns it). Only one of the two outputs is set.
	void parseGate(const picojson::object& req, const char* key, const CvJsonCondition** ppCond, CvString& szPy)
	{
		picojson::object::const_iterator g = req.find(key);
		if (g == req.end()) return;
		if (g->second.is<picojson::object>())
		{
			const picojson::object& go = g->second.get<picojson::object>();
			picojson::object::const_iterator py = go.find("python");
			if (py != go.end() && py->second.is<std::string>()) { szPy = py->second.get<std::string>().c_str(); return; }
		}
		*ppCond = cascadeParseCondition(g->second);
	}

	// o[key] as an FK id (-1 if absent / not a string).
	int fkOf(const picojson::object& o, const char* key)
	{
		picojson::object::const_iterator it = o.find(key);
		return (it != o.end() && it->second.is<std::string>()) ? jsonResolveId(it->second.get<std::string>()) : -1;
	}
	int intOf(const picojson::object& o, const char* key, int fallback)
	{
		picojson::object::const_iterator it = o.find(key);
		return (it != o.end() && it->second.is<double>()) ? (int)it->second.get<double>() : fallback;
	}
}

CvOutcome::CvOutcome(): m_eUnitType(NO_UNIT),
						m_iChance(NULL),
						m_eType(NO_OUTCOME),
						m_ePromotionType(NO_PROMOTION),
						m_iGPP(0),
						m_eGPUnitType(NO_UNIT),
						m_eBonusType(NO_BONUS),
						m_bToCity(false),
						m_pToCityCond(NULL),
						m_eEventTrigger(NO_EVENTTRIGGER),
						m_pPlotCondition(NULL),
						m_pUnitCondition(NULL),
						m_bKill(false),
						m_iChancePerPop(0),
						m_iHappinessTimer(0),
						m_iPopulationBoost(0),
						m_iReduceAnarchyLength(NULL),
						m_pPythonAIFunc(NULL),
						m_pPythonDisplayFunc(NULL),
						m_pPythonExecFunc(NULL),
						m_pPythonPossibleFunc(NULL)
{
	PROFILE_EXTRA_FUNC();
	for (int i=0; i<NUM_YIELD_TYPES; i++)
	{
		m_aiYield[i] = NULL;
	}

	for (int i=0; i<NUM_COMMERCE_TYPES; i++)
	{
		m_aiCommerce[i] = NULL;
	}
}

CvOutcome::~CvOutcome()
{
	PROFILE_EXTRA_FUNC();
	GC.removeDelayedResolution((int*)&m_eType);
	GC.removeDelayedResolution((int*)&m_eUnitType);
	GC.removeDelayedResolution((int*)&m_ePromotionType);
	GC.removeDelayedResolution((int*)&m_eBonusType);
	GC.removeDelayedResolution((int*)&m_eGPUnitType);
	GC.removeDelayedResolution((int*)&m_eEventTrigger);
	SAFE_DELETE(m_iChance);
	SAFE_DELETE(m_pToCityCond);
	SAFE_DELETE(m_iReduceAnarchyLength);
	SAFE_DELETE(m_pPlotCondition);
	SAFE_DELETE(m_pUnitCondition);

	for (int i=0; i<NUM_YIELD_TYPES; i++)
	{
		SAFE_DELETE(m_aiYield[i]);
	}
	for (int i=0; i<NUM_COMMERCE_TYPES; i++)
	{
		SAFE_DELETE(m_aiCommerce[i]);
	}

	Py_XDECREF(m_pPythonAIFunc);
	Py_XDECREF(m_pPythonDisplayFunc);
	Py_XDECREF(m_pPythonExecFunc);
	Py_XDECREF(m_pPythonPossibleFunc);
}

int CvOutcome::getYield(YieldTypes eYield, const CvUnit& kUnit) const
{
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, eYield);

	if (m_aiYield[eYield])
	{
		return m_aiYield[eYield]->evaluate(kUnit.getGameObject());
	}
	else
	{
		return 0;
	}
}

int CvOutcome::getCommerce(CommerceTypes eCommerce, const CvUnit& kUnit) const
{
	FASSERT_BOUNDS(0, NUM_COMMERCE_TYPES, eCommerce);

	if (m_aiCommerce[eCommerce])
	{
		return m_aiCommerce[eCommerce]->evaluate(kUnit.getGameObject());
	}
	else
	{
		return 0;
	}
}

OutcomeTypes CvOutcome::getType() const
{
	return m_eType;
}

UnitTypes CvOutcome::getUnitType() const
{
	return m_eUnitType;
}

bool CvOutcome::getUnitToCity(const CvUnit& kUnit) const
{
	// spawns.toCity: send the spawned unit to a city iff toCity was authored AND its (optional) gate holds.
	return m_bToCity && evalCond(m_pToCityCond, kUnit, kUnit.plot());
}

PromotionTypes CvOutcome::getPromotionType() const
{
	return m_ePromotionType;
}

BonusTypes CvOutcome::getBonusType() const
{
	return m_eBonusType;
}

UnitTypes CvOutcome::getGPUnitType() const
{
	return m_eGPUnitType;
}

int CvOutcome::getGPP() const
{
	return m_iGPP;
}

const CvProperties* CvOutcome::getProperties() const
{
	return &m_Properties;
}

int CvOutcome::getHappinessTimer() const
{
	return m_iHappinessTimer;
}

int CvOutcome::getPopulationBoost() const
{
	return m_iPopulationBoost;
}

int CvOutcome::getReduceAnarchyLength(const CvUnit &kUnit) const
{
	if (m_iReduceAnarchyLength)
	{
		return m_iReduceAnarchyLength->evaluate(kUnit.getGameObject());
	}
	return 0;
}

EventTriggerTypes CvOutcome::getEventTrigger() const
{
	return m_eEventTrigger;
}

int CvOutcome::getChancePerPop() const
{
	return m_iChancePerPop;
}

bool CvOutcome::isKill() const
{
	return m_bKill;
}

void preparePython(CvString& szCode)
{
	PROFILE_EXTRA_FUNC();
	//bst::replace_all(szCode, "\r\n", "\n");
	//bst::replace_all(szCode, "\n", "\r\n");
	//bst::replace_all(szCode, "\t", "  ");
	/*size_t firstNL = szCode.find_first_of('\n');
	if (firstNL != CvString::npos)
	{
		szCode.erase(0, firstNL + 1);
	}*/

	// remove the last line so the closing tag can be separate without caring for whitespace
	size_t lastNL = szCode.find_last_of('\n');
	if (lastNL != CvString::npos)
	{
		szCode.erase(lastNL, szCode.length() - lastNL);
	}

	// we want to remove the amount of space at the start of first code line from all code lines as there might be arbitrary indentation from the XML
	CvString szXMLSpace;
	bool bFinished = false;

	// read the string line by line
	while (!bFinished)
	{
		int iPos = 0;
		bool bComment = false;
		bool bLineFinished = false;
		szXMLSpace.clear();
		while (!bFinished && !bLineFinished)
		{
			if (iPos >= (int)szCode.length())
			{
				// this only happens if there is only whitespace and no code
				bFinished = true;
				szXMLSpace.clear();
				break;
			}
			char c = szCode[iPos];
			switch (c)
			{
				case ' ':
				case '\t':
					if (!bComment)
					{
						szXMLSpace.append(1, c);
					}
					break;

				case '\r':
					break;

				case '\n':
					// end of line, erase it from the string
					szCode.erase(0, iPos + 1);
					bLineFinished = true;
					break;

				case '#':
					// comment
					bComment = true;
					break;

				default:
					// something else, assume actual code, remove line until this char without itself
					szCode.erase(0, iPos);
					bFinished = true;
					break;
			}
			iPos++;
		}
	}

	// now remove this amount of white space from every line
	bst::replace_all(szCode, CvString("\n"+szXMLSpace), CvString("\n"));
}

void CvOutcome::compilePython()
{
	if (m_szPythonCode.empty())
	{
		return;
	}

	// compile the code and add it as a new module
	PyObject* pCode = Py_CompileString(m_szPythonCode.c_str(), m_szPythonModuleName.c_str(), Py_file_input);
	if (!pCode)
	{
		return;
	}

	PyObject* pModule = PyImport_ExecCodeModule((char*)m_szPythonModuleName.c_str(), pCode);
	Py_XDECREF(pCode);
	if (!pModule)
	{
		return;
	}

	PyObject* pDictionary = PyModule_GetDict(pModule);   // borrowed reference

	PyObject* pFunc = PyDict_GetItemString(pDictionary, "isPossible");     // borrowed reference
	if (pFunc)
	{
		Py_INCREF(pFunc);
		m_pPythonPossibleFunc = pFunc;
	}

	pFunc = PyDict_GetItemString(pDictionary, "doOutcome");     // borrowed reference
	if (pFunc)
	{
		Py_INCREF(pFunc);
		m_pPythonExecFunc = pFunc;
	}

	pFunc = PyDict_GetItemString(pDictionary, "getDisplay");     // borrowed reference
	if (pFunc)
	{
		Py_INCREF(pFunc);
		m_pPythonDisplayFunc = pFunc;
	}

	pFunc = PyDict_GetItemString(pDictionary, "getAIValue");     // borrowed reference
	if (pFunc)
	{
		Py_INCREF(pFunc);
		m_pPythonAIFunc = pFunc;
	}

	Py_XDECREF(pModule);

}

int CvOutcome::getChance(const CvUnit &kUnit) const
{
	PROFILE_EXTRA_FUNC();
	int iChance = m_iChance->evaluate(kUnit.getGameObject());
	const CvOutcomeInfo& kInfo = GC.getOutcomeInfo(m_eType);

	const CvCity* pCity = kUnit.plot()->getPlotCity();

	if (pCity)
	{
		iChance += getChancePerPop() * pCity->getPopulation();
	}

	if (kInfo.isCapture() && !kUnit.isHuman())
	{
		iChance += GC.getHandicapInfo(GC.getGame().getHandicapType()).getSubdueAnimalBonusAI();
	}

	for (int i = 0; i < kInfo.getNumExtraChancePromotions(); i++)
	{
		if (kUnit.isHasPromotion(kInfo.getExtraChancePromotion(i)))
		{
			iChance += kInfo.getExtraChancePromotionChance(i);
		}
	}
	return iChance > 0 ? iChance : 0;
}

bool CvOutcome::isPossible(const CvUnit& kUnit) const
{
	PROFILE_EXTRA_FUNC();
	const CvTeam& kTeam = GET_TEAM(kUnit.getTeam());
	const CvOutcomeInfo& kInfo = GC.getOutcomeInfo(m_eType);

	if (!kTeam.isHasTech(kInfo.getPrereqTech()))
	{
		return false;
	}

	if (kInfo.getObsoleteTech() != NO_TECH)
	{
		if (kTeam.isHasTech(kInfo.getObsoleteTech()))
		{
			return false;
		}
	}

	if (kInfo.getPrereqCivic() != NO_CIVIC)
	{
		if (!GET_PLAYER(kUnit.getOwner()).isCivic(kInfo.getPrereqCivic()))
		{
			return false;
		}
	}

	if (kInfo.getCity())
	{
		if (!kUnit.plot()->isCity())
		{
			return false;
		}
	}

	if (kInfo.getNotCity())
	{
		if (kUnit.plot()->isCity())
		{
			return false;
		}
	}

	if (kInfo.isCapture())
	{
		if (kUnit.isNoCapture())
		{
			return false;
		}
	}

	const TeamTypes eOwnerTeam = GET_PLAYER(kUnit.getOwner()).getTeam();
	const CvTeam& kOwnerTeam = GET_TEAM(eOwnerTeam);
	const PlayerTypes ePlotOwner = kUnit.plot()->getOwner();
	if (ePlotOwner == NO_PLAYER)
	{
		if (!kInfo.getNeutralTerritory())
		{
			return false;
		}
	}
	else if (GET_PLAYER(ePlotOwner).isNPC())
	{
		if (!kInfo.getBarbarianTerritory())
		{
			return false;
		}
	}
	else
	{
		const TeamTypes ePlotOwnerTeam = GET_PLAYER(ePlotOwner).getTeam();
		const CvTeam& kPlotOwnerTeam = GET_TEAM(ePlotOwnerTeam);
		if (kOwnerTeam.isAtWar(ePlotOwnerTeam))
		{
			if (!kInfo.getHostileTerritory())
			{
				return false;
			}
		}
		else if ((eOwnerTeam == ePlotOwnerTeam) || (kPlotOwnerTeam.isVassal(eOwnerTeam)))
		{
			if (!kInfo.getFriendlyTerritory())
			{
				return false;
			}
		}
		else if (!kInfo.getNeutralTerritory())
		{
			return false;
		}
	}

	if (!kInfo.getPrereqBuildings().empty())
	{
		const CvCity* pCity = kUnit.plot()->getPlotCity();
		if (!pCity)
		{
			return false;
		}

		foreach_(const BuildingTypes ePrereq, kInfo.getPrereqBuildings())
		{
			if (!pCity->isActiveBuilding(ePrereq))
			{
				return false;
			}
		}
	}

	// Removed because outcome has its own prereq and obsolete tech
	/*if (m_ePromotionType != NO_PROMOTION)
	{
		CvPromotionInfo& kPromotion = GC.getPromotionInfo(m_ePromotionType);
		if (!kTeam.isHasTech(kPromotion.getTechPrereq()))
		{
			return false;
		}
		if (kPromotion.getObsoleteTech() != NO_TECH)
		{
			if (kTeam.isHasTech(kPromotion.getObsoleteTech()))
			{
				return false;
			}
		}
	}*/

	if (m_eBonusType != NO_BONUS)
	{
		const CvBonusInfo& kBonus = GC.getBonusInfo(m_eBonusType);
		if (!kTeam.isHasTech((TechTypes)kBonus.getTechReveal()))
		{
			return false;
		}
		if ((TechTypes)kBonus.getTechObsolete() != NO_TECH)
		{
			if (kTeam.isHasTech((TechTypes)kBonus.getTechObsolete()))
			{
				return false;
			}
		}
		if (kUnit.plot()->getBonusType() != NO_BONUS)
		{
			return false;
		}
		if (kUnit.plot()->getFeatureType() == NO_FEATURE)
		{
			if (!kBonus.isTerrain(kUnit.plot()->getTerrainType()))
			{
				return false;
			}
		}
		else
		{
			if (!kBonus.isFeature(kUnit.plot()->getFeatureType()))
			{
				return false;
			}
			if (!kBonus.isFeatureTerrain(kUnit.plot()->getTerrainType()))
			{
				return false;
			}
		}

		const int iCount = algo::count_if(kUnit.plot()->adjacent(), CvPlot::fn::getBonusType(NO_TEAM) == m_eBonusType);

		if (!(iCount == 0 || (iCount == 1 && kUnit.plot()->isWater())))
		{
			return false;
		}
	}

	if (m_eEventTrigger != NO_EVENTTRIGGER)
	{
		const CvPlayer& kOwner = GET_PLAYER(kUnit.getOwner());
		const CvEventTriggerInfo& kTriggerInfo = GC.getEventTriggerInfo(m_eEventTrigger);
		if (!kOwner.isEventTriggerPossible(m_eEventTrigger, true))
		{
			return false;
		}

		if (kTriggerInfo.isPickCity())
		{
			const CvCity* pCity = kUnit.plot()->getPlotCity();
			if (pCity)
			{
				if (!pCity->isEventTriggerPossible(m_eEventTrigger))
				{
					return false;
				}
			}
		}

		if (!kUnit.plot()->canTrigger(m_eEventTrigger, kUnit.getOwner()))
		{
			return false;
		}
	}

	if (!evalCond(m_pPlotCondition, kUnit, kUnit.plot()) || !evalPyGate(m_szPlotPythonGate, kUnit))
		return false;
	if (!evalCond(m_pUnitCondition, kUnit, kUnit.plot()) || !evalPyGate(m_szUnitPythonGate, kUnit))
		return false;

	if (m_pPythonPossibleFunc)
	{
		CyUnit cyUnit(const_cast<CvUnit*>(&kUnit));
		PyObject* pyUnit = gDLL->getPythonIFace()->makePythonObject(&cyUnit);
		CyPlot cyPlot(const_cast<CvPlot*>(kUnit.plot()));
		PyObject* pyPlot = gDLL->getPythonIFace()->makePythonObject(&cyPlot);

		PyObject* pyResult = PyObject_CallFunctionObjArgs(m_pPythonPossibleFunc, pyUnit, pyPlot, NULL);
		bool bResult = boost::python::extract<bool>(pyResult);

		Py_XDECREF(pyResult);
		Py_DECREF(pyUnit);
		Py_DECREF(pyPlot);

		if (!bResult)
		{
			return false;
		}
	}

	return getChance(kUnit) > 0;
}

// Can return a false positive if an outcome requires a building combination
bool CvOutcome::isPossibleSomewhere(const CvUnit& kUnit) const
{
	PROFILE_EXTRA_FUNC();
	const CvTeam& kTeam = GET_TEAM(kUnit.getTeam());
	const CvOutcomeInfo& kInfo = GC.getOutcomeInfo(m_eType);

	if (!kTeam.isHasTech(kInfo.getPrereqTech()))
	{
		return false;
	}

	if (kInfo.getObsoleteTech() != NO_TECH)
	{
		if (kTeam.isHasTech(kInfo.getObsoleteTech()))
		{
			return false;
		}
	}

	if (kInfo.getPrereqCivic() != NO_CIVIC)
	{
		if (!GET_PLAYER(kUnit.getOwner()).isCivic(kInfo.getPrereqCivic()))
		{
			return false;
		}
	}

	//TeamTypes eOwnerTeam = GET_PLAYER(kUnit.getOwner()).getTeam();
	//CvTeam& kOwnerTeam = GET_TEAM(eOwnerTeam);

	foreach_(const BuildingTypes ePrereq, kInfo.getPrereqBuildings())
	{
		if (GET_PLAYER(kUnit.getOwner()).getBuildingCount(ePrereq) <= 0)
		{
			return false;
		}
	}

	// Removed because outcome has its own prereq and obsolete tech
	/*if (m_ePromotionType != NO_PROMOTION)
	{
		CvPromotionInfo& kPromotion = GC.getPromotionInfo(m_ePromotionType);
		if (!kTeam.isHasTech(kPromotion.getTechPrereq()))
		{
			return false;
		}
		if (kPromotion.getObsoleteTech() != NO_TECH)
		{
			if (kTeam.isHasTech(kPromotion.getObsoleteTech()))
			{
				return false;
			}
		}
	}*/

	if (m_eBonusType != NO_BONUS)
	{
		const CvBonusInfo& kBonus = GC.getBonusInfo(m_eBonusType);
		if (!kTeam.isHasTech((TechTypes)kBonus.getTechReveal()))
		{
			return false;
		}
		if ((TechTypes)kBonus.getTechObsolete() != NO_TECH)
		{
			if (kTeam.isHasTech((TechTypes)kBonus.getTechObsolete()))
			{
				return false;
			}
		}
	}

	if (m_eEventTrigger != NO_EVENTTRIGGER)
	{
		if (!GET_PLAYER(kUnit.getOwner()).isEventTriggerPossible(m_eEventTrigger, true))
		{
			return false;
		}
	}

	if (!evalCond(m_pUnitCondition, kUnit, kUnit.plot()) || !evalPyGate(m_szUnitPythonGate, kUnit))
		return false;

	return getChance(kUnit) > 0;
}

bool CvOutcome::isPossibleInPlot(const CvUnit& kUnit, const CvPlot& kPlot, bool bForTrade) const
{
	PROFILE_EXTRA_FUNC();
	const CvTeam& kTeam = GET_TEAM(kUnit.getTeam());
	const CvOutcomeInfo& kInfo = GC.getOutcomeInfo(m_eType);

	if (!kTeam.isHasTech(kInfo.getPrereqTech()))
	{
		return false;
	}

	if (kInfo.getObsoleteTech() != NO_TECH)
	{
		if (kTeam.isHasTech(kInfo.getObsoleteTech()))
		{
			return false;
		}
	}

	if (kInfo.getPrereqCivic() != NO_CIVIC)
	{
		if (!GET_PLAYER(kUnit.getOwner()).isCivic(kInfo.getPrereqCivic()))
		{
			return false;
		}
	}

	if (kInfo.getCity())
	{
		if (!kPlot.isCity())
		{
			return false;
		}
	}

	if (kInfo.getNotCity())
	{
		if (kPlot.isCity())
		{
			return false;
		}
	}

	const TeamTypes eOwnerTeam = GET_PLAYER(kUnit.getOwner()).getTeam();
	const CvTeam& kOwnerTeam = GET_TEAM(eOwnerTeam);
	const PlayerTypes ePlotOwner = kPlot.getOwner();
	if (ePlotOwner == NO_PLAYER)
	{
		if (!kInfo.getNeutralTerritory())
		{
			return false;
		}
	}
	else if (GET_PLAYER(ePlotOwner).isNPC())
	{
		if (!kInfo.getBarbarianTerritory())
		{
			return false;
		}
	}
	else
	{
		const TeamTypes ePlotOwnerTeam = GET_PLAYER(ePlotOwner).getTeam();
		const CvTeam& kPlotOwnerTeam = GET_TEAM(ePlotOwnerTeam);
		if (kOwnerTeam.isAtWar(ePlotOwnerTeam))
		{
			if (!kInfo.getHostileTerritory())
			{
				return false;
			}
		}
		else if ((eOwnerTeam == ePlotOwnerTeam) || (kPlotOwnerTeam.isVassal(eOwnerTeam)))
		{
			if (!kInfo.getFriendlyTerritory())
			{
				return false;
			}
		}
		else
		{
			if (!kInfo.getNeutralTerritory())
			{
				return false;
			}
		}
	}

	if (!kInfo.getPrereqBuildings().empty())
	{
		const CvCity* pCity = kPlot.getPlotCity();
		if (!pCity)
		{
			return false;
		}

		foreach_(const BuildingTypes ePrereq, kInfo.getPrereqBuildings())
		{
			if (!pCity->isActiveBuilding(ePrereq))
			{
				return false;
			}
		}
	}

	// Removed because outcome has its own prereq and obsolete tech
	/*if (m_ePromotionType != NO_PROMOTION)
	{
		CvPromotionInfo& kPromotion = GC.getPromotionInfo(m_ePromotionType);
		if (!kTeam.isHasTech(kPromotion.getTechPrereq()))
		{
			return false;
		}
		if (kPromotion.getObsoleteTech() != NO_TECH)
		{
			if (kTeam.isHasTech(kPromotion.getObsoleteTech()))
			{
				return false;
			}
		}
	}*/

	if (m_eBonusType != NO_BONUS)
	{
		const CvBonusInfo& kBonus = GC.getBonusInfo(m_eBonusType);
		if (!kTeam.isHasTech((TechTypes)kBonus.getTechReveal()))
		{
			return false;
		}
		if ((TechTypes)kBonus.getTechObsolete() != NO_TECH)
		{
			if (kTeam.isHasTech((TechTypes)kBonus.getTechObsolete()))
			{
				return false;
			}
		}
		if (kPlot.getBonusType() != NO_BONUS)
		{
			return false;
		}
		if (kPlot.getFeatureType() == NO_FEATURE)
		{
			if (!kBonus.isTerrain(kPlot.getTerrainType()))
			{
				return false;
			}
		}
		else
		{
			if (!kBonus.isFeature(kPlot.getFeatureType()))
			{
				return false;
			}
			if (!kBonus.isFeatureTerrain(kPlot.getTerrainType()))
			{
				return false;
			}
		}

		const int iCount = algo::count_if(kPlot.adjacent(), CvPlot::fn::getBonusType(NO_TEAM) == m_eBonusType);

		if (!(iCount == 0 || (iCount == 1 && kPlot.isWater())))
		{
			return false;
		}
	}

	if (m_eEventTrigger != NO_EVENTTRIGGER)
	{
		const CvPlayer& kOwner = GET_PLAYER(kUnit.getOwner());
		const CvEventTriggerInfo& kTriggerInfo = GC.getEventTriggerInfo(m_eEventTrigger);
		if (!kOwner.isEventTriggerPossible(m_eEventTrigger, true))
		{
			return false;
		}

		if (kTriggerInfo.isPickCity())
		{
			const CvCity* pCity = kPlot.getPlotCity();
			if (pCity && !pCity->isEventTriggerPossible(m_eEventTrigger))
			{
				return false;
			}
		}

		if (!kPlot.canTrigger(m_eEventTrigger, kUnit.getOwner()))
		{
			return false;
		}
	}

	if (!evalCond(m_pPlotCondition, kUnit, &kPlot) || !evalPyGate(m_szPlotPythonGate, kUnit))
		return false;
	if (!evalCond(m_pUnitCondition, kUnit, &kPlot) || !evalPyGate(m_szUnitPythonGate, kUnit))
		return false;

	if (m_pPythonPossibleFunc)
	{
		CyUnit cyUnit(const_cast<CvUnit*>(&kUnit));
		PyObject* pyUnit = gDLL->getPythonIFace()->makePythonObject(&cyUnit);
		CyPlot cyPlot(const_cast<CvPlot*>(&kPlot));
		PyObject* pyPlot = gDLL->getPythonIFace()->makePythonObject(&cyPlot);

		PyObject* pyResult = PyObject_CallFunctionObjArgs(m_pPythonPossibleFunc, pyUnit, pyPlot, NULL);
		bool bResult = boost::python::extract<bool>(pyResult);

		Py_XDECREF(pyResult);
		Py_DECREF(pyUnit);
		Py_DECREF(pyPlot);

		if (!bResult)
		{
			return false;
		}
	}

	return getChance(kUnit) > 0;
}

bool CvOutcome::isPossible(const CvPlayerAI& kPlayer) const
{
	const CvTeam& kTeam = GET_TEAM(kPlayer.getTeam());
	const CvOutcomeInfo& kInfo = GC.getOutcomeInfo(m_eType);

	if (!kTeam.isHasTech(kInfo.getPrereqTech()))
	{
		return false;
	}

	if (kInfo.getObsoleteTech() != NO_TECH)
	{
		if (kTeam.isHasTech(kInfo.getObsoleteTech()))
		{
			return false;
		}
	}

	if (kInfo.getPrereqCivic() != NO_CIVIC)
	{
		if (!kPlayer.isCivic(kInfo.getPrereqCivic()))
		{
			return false;
		}
	}

	// Removed because outcome has its own prereq and obsolete tech
	/*if (m_ePromotionType != NO_PROMOTION)
	{
		CvPromotionInfo& kPromotion = GC.getPromotionInfo(m_ePromotionType);
		if (!kTeam.isHasTech(kPromotion.getTechPrereq()))
		{
			return false;
		}
		if (kPromotion.getObsoleteTech() != NO_TECH)
		{
			if (kTeam.isHasTech(kPromotion.getObsoleteTech()))
			{
				return false;
			}
		}
	}*/

	if (m_eBonusType != NO_BONUS)
	{
		const CvBonusInfo& kBonus = GC.getBonusInfo(m_eBonusType);
		if (!kTeam.isHasTech((TechTypes)kBonus.getTechReveal()))
		{
			return false;
		}
		if ((TechTypes)kBonus.getTechObsolete() != NO_TECH)
		{
			if (kTeam.isHasTech((TechTypes)kBonus.getTechObsolete()))
			{
				return false;
			}
		}
	}

	if (m_eEventTrigger != NO_EVENTTRIGGER)
	{
		if (!kPlayer.isEventTriggerPossible(m_eEventTrigger, true))
		{
			return false;
		}
	}

	return true;
}

bool CvOutcome::execute(CvUnit &kUnit, PlayerTypes eDefeatedUnitPlayer, UnitTypes eDefeatedUnitType) const
{
	PROFILE_EXTRA_FUNC();
	if (!isPossible(kUnit))
	{
		return false;
	}
	CvWStringBuffer szBuffer;

	CvPlayer& kPlayer = GET_PLAYER(kUnit.getOwner());

	const bool bToCoastalCity = GC.getOutcomeInfo(getType()).getToCoastalCity();

	const CvUnitInfo* pUnitInfo =
	(
		eDefeatedUnitType > NO_UNIT
		?
		pUnitInfo = &GC.getUnitInfo(eDefeatedUnitType)
		:
		pUnitInfo = &kUnit.getUnitInfo()
	);

	CvWString& szMessage = GC.getOutcomeInfo(getType()).getMessageText();
	bool bNothing = true;
	if (!szMessage.empty())
	{
		szBuffer.append(gDLL->getText(szMessage, kUnit.getNameKey(), pUnitInfo->getTextKeyWide()));
		szBuffer.append(L" ( ");
		bNothing = false;
	}

	bool bFirst = true;

	if (m_ePromotionType > NO_PROMOTION)
	{
		kUnit.setHasPromotion(m_ePromotionType, true);
		bFirst = false;
		szBuffer.append(GC.getPromotionInfo(m_ePromotionType).getDescription());
	}

	const bool bUnitToCity =
	(
		getUnitToCity(kUnit)
		||
		m_eUnitType > NO_UNIT
		&&
		GC.getGame().isOption(GAMEOPTION_ANIMAL_TELEPORT_AWARDS)
		&& (
			GC.getUnitInfo(m_eUnitType).hasUnitCombat(GC.getUNITCOMBAT_SUBDUED())
			||
			GC.getUnitInfo(m_eUnitType).hasUnitCombat(GC.getUNITCOMBAT_IDEA())
		)
	);

	if (m_eUnitType > NO_UNIT && !bUnitToCity)
	{
		CvUnit* pUnit = kPlayer.initUnit(m_eUnitType, kUnit.getX(), kUnit.getY(), GC.getUnitInfo(m_eUnitType).getDefaultUnitAIType(), NO_DIRECTION, GC.getGame().getSorenRandNum(10000, "AI Unit Birthmark"));

		if (pUnit)
		{
			if (pUnit->AI_getUnitAIType() == UNITAI_SUBDUED_ANIMAL && (kUnit.AI_getUnitAIType() == UNITAI_HUNTER || kUnit.getGroup()->getAutomateType() == AUTOMATE_HUNT))
			{
				pUnit->joinGroup(kUnit.getGroup());
			}
			pUnit->finishMoves();
		}
		else FErrorMsg("pUnit is expected to be assigned a valid unit object");

		if (!bFirst)
		{
			szBuffer.append(L", ");
		}
		else bFirst = false;

		szBuffer.append(GC.getUnitInfo(m_eUnitType).getDescription());
	}

	// Calculate the actual yields and commerces
	int aiYield[NUM_YIELD_TYPES];
	int aiCommerce[NUM_COMMERCE_TYPES];

	for (int i=0; i<NUM_YIELD_TYPES; i++)
	{
		aiYield[i] = getYield((YieldTypes)i, kUnit);
	}

	for (int i=0; i<NUM_COMMERCE_TYPES; i++)
	{
		aiCommerce[i] = getCommerce((CommerceTypes)i, kUnit);
	}

	if (aiYield[YIELD_COMMERCE])
	{
		aiCommerce[COMMERCE_CULTURE] += aiYield[YIELD_COMMERCE] * kPlayer.getCommercePercent(COMMERCE_CULTURE);
	}

	if (aiYield[YIELD_PRODUCTION] || aiYield[YIELD_FOOD] || aiCommerce[COMMERCE_CULTURE] || m_iGPP || (bUnitToCity && m_eUnitType > NO_UNIT) || m_iHappinessTimer || m_iPopulationBoost || m_iReduceAnarchyLength)
	{
		CvCity* pCity = GC.getMap().findCity(kUnit.getX(), kUnit.getY(), kUnit.getOwner(), NO_TEAM, true, bToCoastalCity);
		if (!pCity)
			pCity = GC.getMap().findCity(kUnit.getX(), kUnit.getY(), kUnit.getOwner(), NO_TEAM, false, bToCoastalCity);

		if (pCity)
		{
			if (!bFirst)
			{
				szBuffer.append(L", ");
			}
			else bFirst = false;

			if (aiYield[YIELD_PRODUCTION])
			{
				pCity->changeProduction(aiYield[YIELD_PRODUCTION]);
				CvWString szTemp;
				szTemp.Format(L" %d%c", aiYield[YIELD_PRODUCTION], GC.getYieldInfo(YIELD_PRODUCTION).getChar());
				szBuffer.append(szTemp);
			}
			if (aiYield[YIELD_FOOD])
			{
				pCity->changeFood(aiYield[YIELD_FOOD], true);
				CvWString szTemp;
				szTemp.Format(L" %d%c", aiYield[YIELD_FOOD], GC.getYieldInfo(YIELD_FOOD).getChar());
				szBuffer.append(szTemp);
			}

			if (aiCommerce[COMMERCE_CULTURE])
			{
				pCity->changeCulture(kUnit.getOwner(), aiCommerce[COMMERCE_CULTURE], true, true);
				CvWString szTemp;
				szTemp.Format(L" %d%c", aiCommerce[COMMERCE_CULTURE], GC.getCommerceInfo(COMMERCE_CULTURE).getChar());
				szBuffer.append(szTemp);
			}

			if (m_iGPP)
			{
				pCity->changeGreatPeopleProgress(m_iGPP);
				if (m_eGPUnitType > NO_UNIT)
				{
					pCity->changeGreatPeopleUnitProgress(m_eGPUnitType, m_iGPP);
				}
				CvWString szTemp;
				szTemp.Format(L" %d%c", m_iGPP, gDLL->getSymbolID(GREAT_PEOPLE_CHAR));
				szBuffer.append(szTemp);
			}

			if (!m_Properties.isEmpty())
			{
				pCity->getProperties()->addProperties(&m_Properties);
				m_Properties.buildCompactChangesString(szBuffer);
			}

			if (m_iHappinessTimer)
			{
				pCity->changeHappinessTimer(m_iHappinessTimer);
				szBuffer.append(L" ");
				szBuffer.append(gDLL->getText("TXT_KEY_OUTCOME_TEMP_HAPPY", GC.getTEMP_HAPPY(), m_iHappinessTimer));
			}

			if (m_iPopulationBoost)
			{
				pCity->changePopulation(m_iPopulationBoost);
				szBuffer.append(L" ");
				szBuffer.append(gDLL->getText("TXT_KEY_OUTCOME_TEMP_POPULATION_BOOST", m_iPopulationBoost));
			}

			int iReduce = getReduceAnarchyLength(kUnit);
			if (iReduce)
			{
				iReduce = std::min(iReduce, pCity->getOccupationTimer());
				if (iReduce)
				{
					pCity->changeOccupationTimer(-iReduce);
					szBuffer.append(L" ");
					szBuffer.append(gDLL->getText("TXT_KEY_OUTCOME_LESS_ANARCHY", iReduce));
				}
			}

			if (bUnitToCity && m_eUnitType > NO_UNIT)
			{
				CvUnit* pUnit = kPlayer.initUnit(m_eUnitType, pCity->getX(), pCity->getY(), GC.getUnitInfo(m_eUnitType).getDefaultUnitAIType(), NO_DIRECTION, GC.getGame().getSorenRandNum(10000, "AI Unit Birthmark"));

				if (pUnit != NULL)
				{
					pUnit->finishMoves();
				}
				else FErrorMsg("pUnit is expected to be assigned a valid unit object");

				szBuffer.append(L" ");
				szBuffer.append(GC.getUnitInfo(m_eUnitType).getDescription());
			}

			szBuffer.append(L" ");
			szBuffer.append(gDLL->getText("TXT_KEY_OUTCOME_TO"));
			szBuffer.append(" ");
			szBuffer.append(pCity->getName());
		}
	}

	int iGoldTimes100 = 0;
	int iResearchTimes100 = 0;
	int iEspionageTimes100 = 0;

	if (aiYield[YIELD_COMMERCE])
	{
		iGoldTimes100 = aiYield[YIELD_COMMERCE] * kPlayer.getCommercePercent(COMMERCE_GOLD);
		iResearchTimes100 = aiYield[YIELD_COMMERCE] * kPlayer.getCommercePercent(COMMERCE_RESEARCH);
		iEspionageTimes100 = aiYield[YIELD_COMMERCE] * kPlayer.getCommercePercent(COMMERCE_ESPIONAGE);
	}

	iGoldTimes100 += aiCommerce[COMMERCE_GOLD] * 100;
	iResearchTimes100 += aiCommerce[COMMERCE_RESEARCH] * 100;
	iEspionageTimes100 += aiCommerce[COMMERCE_ESPIONAGE] * 100;

	if (iGoldTimes100)
	{
		kPlayer.changeGold(iGoldTimes100 / 100);
		if (!bFirst)
		{
			szBuffer.append(L", ");
		}
		else bFirst = false;

		CvWString szTemp;
		szTemp.Format(L" %d%c", iGoldTimes100 / 100, GC.getCommerceInfo(COMMERCE_GOLD).getChar());
		szBuffer.append(szTemp);
	}
	CvTeam& kTeam = GET_TEAM(kUnit.getTeam());
	if (iResearchTimes100)
	{
		const TechTypes eCurrentTech = kPlayer.getCurrentResearch();
		if (eCurrentTech != NO_TECH)
		{
			kTeam.changeResearchProgress(eCurrentTech, iResearchTimes100 / 100, kUnit.getOwner());
			if (!bFirst)
			{
				szBuffer.append(L", ");
			}
			else bFirst = false;

			CvWString szTemp;
			szTemp.Format(L" %d%c", iResearchTimes100 / 100, GC.getCommerceInfo(COMMERCE_RESEARCH).getChar());
			szBuffer.append(szTemp);
		}
	}
	if (iEspionageTimes100 && (eDefeatedUnitPlayer != NO_PLAYER))
	{
		kTeam.changeEspionagePointsEver(iEspionageTimes100 / 100);
		kTeam.changeEspionagePointsAgainstTeam(GET_PLAYER(eDefeatedUnitPlayer).getTeam(), iEspionageTimes100 / 100);
		if (!bFirst)
		{
			szBuffer.append(L", ");
		}
		else bFirst = false;

		CvWString szTemp;
		szTemp.Format(L" %d%c", iEspionageTimes100 / 100, GC.getCommerceInfo(COMMERCE_ESPIONAGE).getChar());
		szBuffer.append(szTemp);
	}

	if (m_eBonusType != NO_BONUS)
	{
		kUnit.plot()->setBonusType(m_eBonusType);
		if (!bFirst)
		{
			szBuffer.append(L", ");
		}
		else bFirst = false;

		szBuffer.append(GC.getBonusInfo(m_eBonusType).getDescription());
	}

	if (m_bKill)
	{
		if (!bFirst)
		{
			szBuffer.append(L", ");
		}
		else bFirst = false;

		szBuffer.append(gDLL->getText("TXT_KEY_OUTCOME_KILLS_UNIT"));
	}

	if (m_eEventTrigger != NO_EVENTTRIGGER)
	{
		if (!bFirst)
		{
			szBuffer.append(L", ");
		}
		else bFirst = false;

		szBuffer.append(GC.getEventTriggerInfo(m_eEventTrigger).getDescription());
	}

	szBuffer.append(L" )");

	if (!bNothing)
	{

		AddDLLMessage(kUnit.getOwner(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer.getCString(), NULL, MESSAGE_TYPE_INFO, pUnitInfo->getButton(), NO_COLOR, kUnit.getX(), kUnit.getY(), true, true);
	}

	if (m_eEventTrigger != NO_EVENTTRIGGER)
	{
		const CvEventTriggerInfo& kTriggerInfo = GC.getEventTriggerInfo(m_eEventTrigger);
		if (kTriggerInfo.isPickCity() && (kUnit.plot()->getPlotCity() != NULL))
		{
			kPlayer.initTriggeredData(m_eEventTrigger, true, kUnit.plot()->getPlotCity()->getID());
		}
		else
		{
			kPlayer.initTriggeredData(m_eEventTrigger, true, -1, kUnit.getX(), kUnit.getY());
		}
	}

	if (!m_szPythonCallback.empty())
	{
		Cy::call("CvOutcomeInterface", m_szPythonCallback, Cy::Args() << &kUnit << eDefeatedUnitPlayer << eDefeatedUnitType);
	}

	if (m_pPythonExecFunc)
	{
		CyUnit cyUnit(&kUnit);
		PyObject* pyUnit = gDLL->getPythonIFace()->makePythonObject(&cyUnit);
		CyPlot cyPlot(kUnit.plot());
		PyObject* pyPlot = gDLL->getPythonIFace()->makePythonObject(&cyPlot);
		PyObject* pyDefeatedPlayer = gDLL->getPythonIFace()->makePythonObject(&eDefeatedUnitPlayer);
		PyObject* pyDefeatedUnitType = gDLL->getPythonIFace()->makePythonObject(&eDefeatedUnitType);

		PyObject* pyResult = PyObject_CallFunctionObjArgs(m_pPythonExecFunc, pyUnit, pyPlot, pyDefeatedPlayer, pyDefeatedUnitType, NULL);
		//bool bResult = boost::python::extract<bool>(pyResult);

		Py_XDECREF(pyResult);
		Py_DECREF(pyUnit);
		Py_DECREF(pyPlot);
		Py_DECREF(pyDefeatedPlayer);
		Py_DECREF(pyDefeatedUnitType);
	}

	if (m_bKill)
	{
		kUnit.kill(true);
	}
	return true;
}

int CvOutcome::AI_getValueInPlot(const CvUnit &kUnit, const CvPlot &kPlot, bool bForTrade) const
{
	PROFILE_EXTRA_FUNC();
	if (!isPossibleInPlot(kUnit, kPlot, bForTrade))
	{
		return 0;
	}

	int iValue = 0;

	CvPlayerAI& kPlayer = GET_PLAYER(kUnit.getOwner());
	const bool bToCoastalCity = GC.getOutcomeInfo(getType()).getToCoastalCity();
	//CvUnitInfo* pUnitInfo = &kUnit.getUnitInfo();

	if (m_ePromotionType > NO_PROMOTION)
	{
		iValue += kPlayer.AI_promotionValue(m_ePromotionType, kUnit.getUnitType(), &kUnit, kUnit.AI_getUnitAIType());
	}

	if ( m_eEventTrigger != NO_EVENTTRIGGER )
	{
		int iTempValue;
		EventTriggeredData* pTriggerData;

		const CvEventTriggerInfo& kTriggerInfo = GC.getEventTriggerInfo(m_eEventTrigger);
		if (kTriggerInfo.isPickCity() && (kPlot.getPlotCity() != NULL))
		{
			pTriggerData = kPlayer.initTriggeredData(m_eEventTrigger, false, kPlot.getPlotCity()->getID());
		}
		else
		{
			pTriggerData = kPlayer.initTriggeredData(m_eEventTrigger, false, -1, kPlot.getX(), kPlot.getY());
		}

		if ( NO_EVENT != kPlayer.AI_chooseEvent(pTriggerData->getID(), &iTempValue) )
		{
			iValue += iTempValue;
		}

		kPlayer.deleteEventTriggered(pTriggerData->getID());
	}

	if (m_eUnitType > NO_UNIT)
	{
		iValue += kPlayer.AI_unitValue(m_eUnitType, GC.getUnitInfo(m_eUnitType).getDefaultUnitAIType(), kPlot.area());
	}

	// Calculate the actual yields and commerces, if the expression tries to include the plot the result will be incorrect
	int aiYield[NUM_YIELD_TYPES];
	int aiCommerce[NUM_COMMERCE_TYPES];

	for (int i=0; i<NUM_YIELD_TYPES; i++)
	{
		aiYield[i] = getYield((YieldTypes)i, kUnit);
	}

	for (int i=0; i<NUM_COMMERCE_TYPES; i++)
	{
		aiCommerce[i] = getCommerce((CommerceTypes)i, kUnit);
	}

	// We go the easy way and use AI_yieldValue for all the yields and commerces despite that city multipliers do not apply for some
	if (aiYield[YIELD_PRODUCTION] || aiYield[YIELD_FOOD] || aiYield[YIELD_COMMERCE] || aiCommerce[COMMERCE_GOLD] || aiCommerce[COMMERCE_RESEARCH] || aiCommerce[COMMERCE_CULTURE] || aiCommerce[COMMERCE_ESPIONAGE] || m_iGPP)
	{
		// short circuit plot city as this method will be called for city plots most of the time
		CvCityAI* pCity = static_cast<CvCityAI*>(kPlot.getPlotCity());
		if (!pCity || (bToCoastalCity && (!pCity->isCoastal(GC.getWorldInfo(GC.getMap().getWorldSize()).getOceanMinAreaSize()))))
			pCity = (CvCityAI*) GC.getMap().findCity(kPlot.getX(), kPlot.getY(), kUnit.getOwner(), NO_TEAM, true, bToCoastalCity);
		if (!pCity)
			pCity = (CvCityAI*) GC.getMap().findCity(kPlot.getX(), kPlot.getY(), kUnit.getOwner(), NO_TEAM, false, bToCoastalCity);

		if (pCity)
		{
			if (aiYield[YIELD_PRODUCTION] || aiYield[YIELD_FOOD] || aiYield[YIELD_COMMERCE] || aiCommerce[COMMERCE_GOLD] || aiCommerce[COMMERCE_RESEARCH] || aiCommerce[COMMERCE_CULTURE] || aiCommerce[COMMERCE_ESPIONAGE])
			{
				// We need shorts and not ints
				short aiYields[NUM_YIELD_TYPES];
				short aiCommerces[NUM_COMMERCE_TYPES];
				for (int i=0; i<NUM_YIELD_TYPES; i++)
					aiYields[i] = (short)aiYield[i];
				for (int i=0; i<NUM_COMMERCE_TYPES; i++)
					aiCommerces[i] = (short)aiCommerce[i];
				iValue += pCity->AI_yieldValue(aiYields, aiCommerces, false, false);
			}

			if (m_iGPP)
			{
				// Currently there is no use of that feature so go the easy way. If it is used more, code similar to AI_specialistValue should be used
				iValue += m_iGPP * 4;
			}

			if (m_iHappinessTimer)
			{
				if (pCity->happyLevel() / 100 - pCity->unhappyLevel(1) / 100 < 0)   // ÷100: verdicts ×100
				{
					iValue += m_iHappinessTimer * 10;
				}
			}

			if (m_iPopulationBoost)
			{
				iValue += m_iPopulationBoost * 50;
			}

			int iReduce = getReduceAnarchyLength(kUnit);
			if (iReduce)
			{
				const int iOccupation = pCity->getOccupationTimer();
				iReduce = std::min(iReduce, iOccupation);
				iValue += iReduce * 50; // pure guess, might be more or less valuable or some more complex code might be needed
			}
		}

	}

	if (m_pPythonAIFunc)
	{
		CyUnit cyUnit(const_cast<CvUnit*>(&kUnit));
		PyObject* pyUnit = gDLL->getPythonIFace()->makePythonObject(&cyUnit);
		CyPlot cyPlot(const_cast<CvPlot*>(&kPlot));
		PyObject* pyPlot = gDLL->getPythonIFace()->makePythonObject(&cyPlot);

		PyObject* pyResult = PyObject_CallFunctionObjArgs(m_pPythonAIFunc, pyUnit, pyPlot, NULL);
		iValue += boost::python::extract<int>(pyResult);

		Py_XDECREF(pyResult);
		Py_DECREF(pyUnit);
		Py_DECREF(pyPlot);
	}

	return iValue;
}

// #430: JSON intake of ONE outcome entry (a member of outcomes.kill[] or an action's payload). Mirrors read()
// field-for-field over the CLEAN curated JSON: numerics -> IntExpr (jsonToIntExpr), FKs resolved directly,
// conditions -> cascade CvJsonCondition (or a {python} gate). The XML read() path above is dead.
void CvOutcome::mapFrom(const picojson::value& v)
{
	if (!v.is<picojson::object>()) return;
	const picojson::object& o = v.get<picojson::object>();
	picojson::object::const_iterator it;

	// requires: the OUTCOME_* gate FK + plot/unit conditions (cascade, or a {python} gate)
	if (const picojson::object* req = jsonChildObj(o, "requires"))
	{
		const int t = fkOf(*req, "outcome");
		if (t >= 0) m_eType = (OutcomeTypes)t;
		parseGate(*req, "plot", &m_pPlotCondition, m_szPlotPythonGate);
		parseGate(*req, "unit", &m_pUnitCondition, m_szUnitPythonGate);
	}

	if ((it = o.find("chance")) != o.end())        m_iChance = jsonToIntExpr(it->second);
	m_iChancePerPop = intOf(o, "chancePerPop", 0);
	if ((it = o.find("consumes")) != o.end() && it->second.is<bool>()) m_bKill = it->second.get<bool>();

	// spawns { unit, toCity? }
	if (const picojson::object* sp = jsonChildObj(o, "spawns"))
	{
		const int u = fkOf(*sp, "unit");
		if (u >= 0) m_eUnitType = (UnitTypes)u;
		it = sp->find("toCity");
		if (it != sp->end())
		{
			m_bToCity = true;
			if (!it->second.is<bool>()) m_pToCityCond = cascadeParseCondition(it->second);   // conditional to-city (e.g. a tech)
		}
	}

	{ const int p = fkOf(o, "promotes"); if (p >= 0) m_ePromotionType = (PromotionTypes)p; }
	{ const int b = fkOf(o, "places");   if (b >= 0) m_eBonusType     = (BonusTypes)b; }
	{ const int e = fkOf(o, "triggers"); if (e >= 0) m_eEventTrigger  = (EventTriggerTypes)e; }

	if (const picojson::object* gp = jsonChildObj(o, "greatPeople"))
	{
		m_iGPP = intOf(*gp, "points", 0);
		const int gu = fkOf(*gp, "unit"); if (gu >= 0) m_eGPUnitType = (UnitTypes)gu;
	}
	m_iPopulationBoost = intOf(o, "population", 0);
	if ((it = o.find("revolution")) != o.end()) m_iReduceAnarchyLength = jsonToIntExpr(it->second);
	if (const picojson::object* hp = jsonChildObj(o, "happiness")) m_iHappinessTimer = intOf(*hp, "duration", 0);

	static const char* const YKEY[NUM_YIELD_TYPES]    = { "food", "production", "commerce" };
	for (int y = 0; y < NUM_YIELD_TYPES; ++y)
		if ((it = o.find(YKEY[y])) != o.end()) m_aiYield[y] = jsonToIntExpr(it->second);
	static const char* const CKEY[NUM_COMMERCE_TYPES] = { "gold", "research", "culture", "espionage" };
	for (int c = 0; c < NUM_COMMERCE_TYPES; ++c)
		if ((it = o.find(CKEY[c])) != o.end()) m_aiCommerce[c] = jsonToIntExpr(it->second);

	// properties: no unit outcome authors a CvProperties delta today (curator-verified) -> not parsed; add when data appears.

	if (const picojson::object* py = jsonChildObj(o, "python"))
	{
		if ((it = py->find("callback")) != py->end() && it->second.is<std::string>()) m_szPythonCallback   = it->second.get<std::string>().c_str();
		if ((it = py->find("module"))   != py->end() && it->second.is<std::string>()) m_szPythonModuleName = it->second.get<std::string>().c_str();
		if ((it = py->find("code"))     != py->end() && it->second.is<std::string>()) m_szPythonCode       = it->second.get<std::string>().c_str();
	}
	// The curator already dedents the inline code to clean, column-0 Python (curate_unit._python_code) -- so compile
	// it DIRECTLY; preparePython (which strips the first-line prefix AND drops the last line for the XML `</Python>`)
	// would corrupt already-clean code by removing a real final line.
	if (!m_szPythonCode.empty()) compilePython();
}

void CvOutcome::buildDisplayString(CvWStringBuffer &szBuffer, const CvUnit& kUnit) const
{
	//CvPlayer& kPlayer = GET_PLAYER(kUnit.getOwner());
	const bool bToCoastalCity = GC.getOutcomeInfo(getType()).getToCoastalCity();
	//CvUnitInfo* pUnitInfo = &kUnit.getUnitInfo();

	szBuffer.append(GC.getOutcomeInfo(getType()).getText());
	szBuffer.append(L" ( ");

	bool bFirst = true;

	if (m_ePromotionType > NO_PROMOTION)
	{
		szBuffer.append(GC.getPromotionInfo(m_ePromotionType).getDescription());
		bFirst = false;
	}

	bool bUnitToCity = getUnitToCity(kUnit);
	if (GC.getGame().isOption(GAMEOPTION_ANIMAL_TELEPORT_AWARDS) &&
		m_eUnitType > NO_UNIT &&
		(GC.getUnitInfo(m_eUnitType).hasUnitCombat(GC.getUNITCOMBAT_SUBDUED()) ||
		GC.getUnitInfo(m_eUnitType).hasUnitCombat(GC.getUNITCOMBAT_IDEA())))
	{
		bUnitToCity = true;
	}
	if (m_eUnitType > NO_UNIT && !bUnitToCity)
	{
		if (!bFirst)
		{
			szBuffer.append(L", ");
		}
		else
		{
			bFirst = false;
		}
		szBuffer.append(GC.getUnitInfo(m_eUnitType).getDescription());
	}

	if ((m_aiYield[YIELD_PRODUCTION] && !m_aiYield[YIELD_PRODUCTION]->isConstantZero()) || (m_aiYield[YIELD_FOOD] && !m_aiYield[YIELD_FOOD]->isConstantZero()) ||
		(m_aiYield[YIELD_COMMERCE] && !m_aiYield[YIELD_COMMERCE]->isConstantZero()) || m_aiCommerce[COMMERCE_CULTURE] || m_iGPP || (bUnitToCity && m_eUnitType > NO_UNIT))
	{
		if (!bFirst)
		{
			szBuffer.append(L", ");
		}
		else
		{
			bFirst = false;
		}
		if (m_aiYield[YIELD_PRODUCTION])
		{
			if (!m_aiYield[YIELD_PRODUCTION]->isConstantZero())
			{
				CvWString szTemp;
				szBuffer.append(L" ");
				m_aiYield[YIELD_PRODUCTION]->buildDisplayString(szBuffer);
				szTemp.Format(L"%c", GC.getYieldInfo(YIELD_PRODUCTION).getChar());
				szBuffer.append(szTemp);
			}
		}
		if (m_aiYield[YIELD_FOOD])
		{
			if (!m_aiYield[YIELD_FOOD]->isConstantZero())
			{
				CvWString szTemp;
				szBuffer.append(L" ");
				m_aiYield[YIELD_FOOD]->buildDisplayString(szBuffer);
				szTemp.Format(L"%c", GC.getYieldInfo(YIELD_FOOD).getChar());
				szBuffer.append(szTemp);
			}
		}

		if (m_aiYield[YIELD_COMMERCE])
		{
			if (!m_aiYield[YIELD_COMMERCE]->isConstantZero())
			{
				CvWString szTemp;
				szBuffer.append(L" ");
				m_aiYield[YIELD_COMMERCE]->buildDisplayString(szBuffer);
				szTemp.Format(L"%c", GC.getYieldInfo(YIELD_COMMERCE).getChar());
				szBuffer.append(szTemp);
			}
		}

		if (m_aiCommerce[COMMERCE_CULTURE])
		{
			if (!m_aiCommerce[COMMERCE_CULTURE]->isConstantZero())
			{
				CvWString szTemp;
				szBuffer.append(L" ");
				m_aiCommerce[COMMERCE_CULTURE]->buildDisplayString(szBuffer);
				szTemp.Format(L"%c", GC.getCommerceInfo(COMMERCE_CULTURE).getChar());
				szBuffer.append(szTemp);
			}
		}

		if (m_iGPP)
		{
			CvWString szTemp;
			szTemp.Format(L" %d%c", m_iGPP, gDLL->getSymbolID(GREAT_PEOPLE_CHAR));
			szBuffer.append(szTemp);
		}

		if (!m_Properties.isEmpty())
		{
			m_Properties.buildCompactChangesString(szBuffer);
		}

		if (m_iHappinessTimer)
		{
			const int iHappy = GC.getTEMP_HAPPY();
			szBuffer.append(L" ");
			szBuffer.append(gDLL->getText("TXT_KEY_OUTCOME_TEMP_HAPPY", iHappy, m_iHappinessTimer));
		}

		if (m_iPopulationBoost)
		{
			szBuffer.append(L" ");
			szBuffer.append(gDLL->getText("TXT_KEY_OUTCOME_TEMP_POPULATION_BOOST", m_iPopulationBoost));
		}

		if (m_iReduceAnarchyLength)
		{
			szBuffer.append(L" -");
			m_iReduceAnarchyLength->buildDisplayString(szBuffer);
			szBuffer.append(gDLL->getText("TXT_KEY_OUTCOME_LESS_ANARCHY_DISPLAY"));
		}

		if (bUnitToCity && m_eUnitType > NO_UNIT)
		{
			szBuffer.append(L" ");
			szBuffer.append(GC.getUnitInfo(m_eUnitType).getDescription());
		}

		if (bToCoastalCity)
		{
			szBuffer.append(L" ");
			szBuffer.append(gDLL->getText("TXT_KEY_OUTCOME_NEAREST_COASTAL"));
		}
		else
		{
			szBuffer.append(L" ");
			szBuffer.append(gDLL->getText("TXT_KEY_OUTCOME_NEAREST_CITY"));
		}
	}

	//int iGoldTimes100 = 0;
	//int iResearchTimes100 = 0;

	//if (m_aiYield[YIELD_COMMERCE])
	//{
	//	iGoldTimes100 = m_aiYield[YIELD_COMMERCE] * kPlayer.getCommercePercent(COMMERCE_GOLD);
	//	iResearchTimes100 = m_aiYield[YIELD_COMMERCE] * kPlayer.getCommercePercent(COMMERCE_RESEARCH);
	//}

	//iGoldTimes100 += m_aiCommerce[COMMERCE_GOLD] * 100;
	//iResearchTimes100 += m_aiCommerce[COMMERCE_RESEARCH] * 100;

	//if (iGoldTimes100)
	if (m_aiCommerce[COMMERCE_GOLD])
	{
		if (!m_aiCommerce[COMMERCE_GOLD]->isConstantZero())
		{
			if (!bFirst)
			{
				szBuffer.append(L", ");
			}
			else
			{
				bFirst = false;
			}
			CvWString szTemp;
			szBuffer.append(L" ");
			m_aiCommerce[COMMERCE_GOLD]->buildDisplayString(szBuffer);
			szTemp.Format(L"%c", GC.getCommerceInfo(COMMERCE_GOLD).getChar());
			szBuffer.append(szTemp);
		}
	}
	//CvTeam& kTeam = GET_TEAM(kUnit.getTeam());
	//if (iResearchTimes100)
	if (m_aiCommerce[COMMERCE_RESEARCH])
	{
		if (!m_aiCommerce[COMMERCE_RESEARCH]->isConstantZero())
		{
			if (!bFirst)
			{
				szBuffer.append(L", ");
			}
			else
			{
				bFirst = false;
			}
			CvWString szTemp;
			szBuffer.append(L" ");
			m_aiCommerce[COMMERCE_RESEARCH]->buildDisplayString(szBuffer);
			szTemp.Format(L"%c", GC.getCommerceInfo(COMMERCE_RESEARCH).getChar());
			szBuffer.append(szTemp);
		}
	}

	if (m_eBonusType != NO_BONUS)
	{
		if (!bFirst)
		{
			szBuffer.append(L", ");
		}
		else
		{
			bFirst = false;
		}
		szBuffer.append(GC.getBonusInfo(m_eBonusType).getDescription());
	}

	if (m_eEventTrigger != NO_EVENTTRIGGER)
	{
		if (!bFirst)
		{
			szBuffer.append(L", ");
		}
		else
		{
			bFirst = false;
		}
		szBuffer.append(GC.getEventTriggerInfo(m_eEventTrigger).getDescription());
	}

	if (m_bKill)
	{
		if (!bFirst)
		{
			szBuffer.append(L", ");
		}
		else
		{
			bFirst = false;
		}
		szBuffer.append(gDLL->getText("TXT_KEY_OUTCOME_KILLS_UNIT"));
	}

	if (m_pPythonDisplayFunc)
	{
		if (!bFirst)
		{
			szBuffer.append(L", ");
		}
		else
		{
			bFirst = false;
		}

		CyUnit cyUnit(const_cast<CvUnit*>(&kUnit));
		PyObject* pyUnit = gDLL->getPythonIFace()->makePythonObject(&cyUnit);
		CyPlot cyPlot(const_cast<CvPlot*>(kUnit.plot()));
		PyObject* pyPlot = gDLL->getPythonIFace()->makePythonObject(&cyPlot);

		PyObject* pyResult = PyObject_CallFunctionObjArgs(m_pPythonDisplayFunc, pyUnit, pyPlot, NULL);
		szBuffer.append(CvWString(boost::python::extract<std::wstring>(pyResult)));

		Py_XDECREF(pyResult);
		Py_DECREF(pyUnit);
		Py_DECREF(pyPlot);
	}

	szBuffer.append(L" )");
}

void CvOutcome::getCheckSum(uint32_t& iSum) const
{
	PROFILE_EXTRA_FUNC();
	CheckSum(iSum, m_eType);
	m_iChance->getCheckSum(iSum);
	CheckSum(iSum, m_eUnitType);
	CheckSum(iSum, m_bToCity);
	CheckSum(iSum, m_ePromotionType);
	CheckSum(iSum, m_eBonusType);
	CheckSum(iSum, m_iGPP);
	CheckSum(iSum, m_eGPUnitType);
	for (int i=0; i<NUM_YIELD_TYPES; i++)
	{
		if (m_aiYield[i])
			m_aiYield[i]->getCheckSum(iSum);
	}
	for (int i=0; i<NUM_COMMERCE_TYPES; i++)
	{
		if (m_aiCommerce[i])
			m_aiCommerce[i]->getCheckSum(iSum);
	}
	CheckSum(iSum, m_iHappinessTimer);
	CheckSum(iSum, m_iPopulationBoost);
	if (m_iReduceAnarchyLength)
		m_iReduceAnarchyLength->getCheckSum(iSum);
	m_Properties.getCheckSum(iSum);
	// (plot/unit conditions are CvJsonCondition trees now -- not summed here; the outcome id + payload above cover it)
	CheckSumC(iSum, m_szPlotPythonGate);
	CheckSumC(iSum, m_szUnitPythonGate);
	CheckSumC(iSum, m_szPythonCallback);
	CheckSum(iSum, m_bKill);
	CheckSum(iSum, m_szPythonCode);
	CheckSum(iSum, m_szPythonModuleName);
}
