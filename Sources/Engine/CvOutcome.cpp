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
#include "CvTerrainInfo.h"
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
#include "Tools/CheckSum.h"
#include "AI/CvGameAI.h"
#include "Infrastructure/IntExpr.h"
#include "Conditions/CvConditionEval.h"       // the ONE evaluator + CvCascadeEvalCtx
#include "Infos/CvJsonConditionParse.h"       // cascadeParseCondition -- the ONE human->data condition boundary
#include "Infos/CvJsonParse.h"                // jsonChildObj / jsonIdInt / jsonIdFk / jsonIdBool / jsonIdStr
#include "Data/CvInfoValuation.h"             // fillEvalCtxAtPlot

namespace
{
	//	The ONE eval-ctx assembly for an outcome's gates. Plot-anchored, because every gate an outcome carries is
	//	asked about a place ([contexts.md]: the ctx carries the per-scope CONTEXT SILOS, never the game objects);
	//	the unit rides the one acknowledged raw slot until the unit context lands.
	void oc_fillCtx(const CvUnit& kUnit, const CvPlot* pPlot, CvCascadeEvalCtx& ctx)
	{
		if (pPlot != NULL)
		{
			InfoValuation::fillEvalCtxAtPlot(*pPlot, ctx);
		}
		//	The ACTOR is the unit, so an empire atom (a tech, anarchy) asks about the unit's owner -- never the
		//	plot's, which is nobody on an unowned tile and a rival on a foreign one.
		if (kUnit.getOwner() != NO_PLAYER)
		{
			ctx.empireContext = &GET_PLAYER(kUnit.getOwner()).getEmpireContext();
		}
		ctx.unit = &kUnit;
	}

	bool oc_gateHolds(const CvCondition* pCondition, const CvUnit& kUnit, const CvPlot* pPlot)
	{
		if (pCondition == NULL)
		{
			return true;
		}
		CvCascadeEvalCtx ctx;
		oc_fillCtx(kUnit, pPlot, ctx);
		static const CvCascadeEvalFlags kFlags;
		return cascadeEvalCondition(pCondition, ctx, kFlags);
	}

	///<summary>
	/// A uniformly random map plot on which the condition holds for this unit, or NULL when none does. One draw on
	/// the synchronized stream, taken only when there is a choice to make.
	///</summary>
	const CvPlot* oc_randomPlotWhere(const CvCondition* pCondition, const CvUnit& kUnit)
	{
		const CvMap& kMap = GC.getMap();
		int iMatching = 0;
		for (int iPlot = 0; iPlot < kMap.numPlots(); iPlot++)
		{
			if (oc_gateHolds(pCondition, kUnit, kMap.plotByIndex(iPlot)))
			{
				iMatching++;
			}
		}
		if (iMatching == 0)
		{
			return NULL;
		}
		int iChosen = iMatching > 1 ? GC.getGame().getSorenRandNum(iMatching, "Outcome spawn anywhere") : 0;
		for (int iPlot = 0; iPlot < kMap.numPlots(); iPlot++)
		{
			const CvPlot* pPlot = kMap.plotByIndex(iPlot);
			if (oc_gateHolds(pCondition, kUnit, pPlot))
			{
				if (iChosen == 0)
				{
					return pPlot;
				}
				iChosen--;
			}
		}
		return NULL;
	}

	//	A numeric outcome payload: a plain int, or the {base, random} pair the curator emits for a rolled
	//	amount -- Plus(Constant, Random), which is exactly what the legacy expression tree held.
	const IntExpr* oc_intExpr(const picojson::object& o, const char* szKey)
	{
		picojson::object::const_iterator it = o.find(szKey);
		if (it == o.end())
		{
			return NULL;
		}
		if (it->second.is<double>())
		{
			return new IntExprConstant((int)it->second.get<double>());
		}
		if (it->second.is<picojson::object>())
		{
			const picojson::object& kNum = it->second.get<picojson::object>();
			const int iBase = jsonIdInt(kNum, "base");
			const int iRandom = jsonIdInt(kNum, "random");
			if (iRandom != 0)
			{
				return new IntExprPlus(new IntExprConstant(iBase), new IntExprRandom(new IntExprConstant(iRandom)));
			}
			return new IntExprConstant(iBase);
		}
		return NULL;
	}
}

CvOutcome::CvOutcome(): m_eUnitType(NO_UNIT),
						m_iChance(NULL),
						m_eType(NO_OUTCOME),
						m_ePromotionType(NO_PROMOTION),
						m_iGPP(0),
						m_eGPUnitType(NO_UNIT),
						m_eBonusType(NO_BONUS),
						m_bUnitToCity(NULL),
						m_eEventTrigger(NO_EVENTTRIGGER),
						m_pPlotCondition(NULL),
						m_pUnitCondition(NULL),
						m_bKill(false),
						m_iChancePerPop(0),
						m_iHappinessTimer(0),
						m_iPopulationBoost(0),
						m_iReduceAnarchyLength(NULL),
						m_pSpawnAnywhere(NULL),
						m_eTerraformTerrain(NO_TERRAIN),
						m_bUnitToCapital(false),
						m_bFound(false)
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
	SAFE_DELETE(m_bUnitToCity);
	SAFE_DELETE(m_iReduceAnarchyLength);
	SAFE_DELETE(m_pPlotCondition);
	SAFE_DELETE(m_pUnitCondition);
	SAFE_DELETE(m_pSpawnAnywhere);

	for (int i=0; i<NUM_YIELD_TYPES; i++)
	{
		SAFE_DELETE(m_aiYield[i]);
	}
	for (int i=0; i<NUM_COMMERCE_TYPES; i++)
	{
		SAFE_DELETE(m_aiCommerce[i]);
	}
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
	if (m_bUnitToCity)
	{
		return oc_gateHolds(m_bUnitToCity, kUnit, kUnit.plot());
	}
	return false;
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

int CvOutcome::getChance(const CvUnit &kUnit, int iDefeatedCaptureResistance) const
{
	PROFILE_EXTRA_FUNC();
	int iChance = m_iChance->evaluate(kUnit.getGameObject());
	const CvOutcomeInfo& kInfo = GC.getOutcomeInfo(m_eType);

	const CvCity* pCity = kUnit.plot()->getPlotCity();

	if (pCity)
	{
		iChance += getChancePerPop() * pCity->getPopulation();
	}

	// The AI's subdue-animal handicap bonus re-homes onto the TRIGGER's own `chance` ([triggers.md]: a threshold
	// on a happening is a trigger, not a magnitude), so it carries no kind and no handicap getter; it reaches the
	// odds again when the curator attaches it there.

	const std::map<int, int>& kPromotionOdds = kInfo.getPromotionOdds();
	for (std::map<int, int>::const_iterator itOdds = kPromotionOdds.begin(); itOdds != kPromotionOdds.end(); ++itOdds)
	{
		if (kUnit.isHasPromotion((PromotionTypes)itOdds->first))
		{
			iChance += itOdds->second;
		}
	}

	if (kInfo.isCaptureContest())
	{
		iChance += kUnit.captureProbabilityTotal() - iDefeatedCaptureResistance;
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

	if (kInfo.hasPlacement(OUTCOME_PLACEMENT_CITY))
	{
		if (!kUnit.plot()->isCity())
		{
			return false;
		}
	}

	if (kInfo.hasPlacement(OUTCOME_PLACEMENT_NOT_CITY))
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

	//	A contested capture is taken by a land unit that is not an animal.
	if (kInfo.isCaptureContest() && (kUnit.isAnimal() || kUnit.getDomainType() != DOMAIN_LAND))
	{
		return false;
	}

	const TeamTypes eOwnerTeam = GET_PLAYER(kUnit.getOwner()).getTeam();
	const CvTeam& kOwnerTeam = GET_TEAM(eOwnerTeam);
	const PlayerTypes ePlotOwner = kUnit.plot()->getOwner();
	if (ePlotOwner == NO_PLAYER)
	{
		if (!kInfo.hasTerritory(OUTCOME_TERRITORY_NEUTRAL))
		{
			return false;
		}
	}
	else if (GET_PLAYER(ePlotOwner).isNPC())
	{
		if (!kInfo.hasTerritory(OUTCOME_TERRITORY_BARBARIAN))
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
			if (!kInfo.hasTerritory(OUTCOME_TERRITORY_HOSTILE))
			{
				return false;
			}
		}
		else if ((eOwnerTeam == ePlotOwnerTeam) || (kPlotOwnerTeam.isVassal(eOwnerTeam)))
		{
			if (!kInfo.hasTerritory(OUTCOME_TERRITORY_FRIENDLY))
			{
				return false;
			}
		}
		else if (!kInfo.hasTerritory(OUTCOME_TERRITORY_NEUTRAL))
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

	if (!oc_gateHolds(m_pPlotCondition, kUnit, kUnit.plot()))
	{
		return false;
	}

	if (!oc_gateHolds(m_pUnitCondition, kUnit, kUnit.plot()))
	{
		return false;
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

	if (!oc_gateHolds(m_pUnitCondition, kUnit, kUnit.plot()))
	{
		return false;
	}

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

	if (kInfo.hasPlacement(OUTCOME_PLACEMENT_CITY))
	{
		if (!kPlot.isCity())
		{
			return false;
		}
	}

	if (kInfo.hasPlacement(OUTCOME_PLACEMENT_NOT_CITY))
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
		if (!kInfo.hasTerritory(OUTCOME_TERRITORY_NEUTRAL))
		{
			return false;
		}
	}
	else if (GET_PLAYER(ePlotOwner).isNPC())
	{
		if (!kInfo.hasTerritory(OUTCOME_TERRITORY_BARBARIAN))
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
			if (!kInfo.hasTerritory(OUTCOME_TERRITORY_HOSTILE))
			{
				return false;
			}
		}
		else if ((eOwnerTeam == ePlotOwnerTeam) || (kPlotOwnerTeam.isVassal(eOwnerTeam)))
		{
			if (!kInfo.hasTerritory(OUTCOME_TERRITORY_FRIENDLY))
			{
				return false;
			}
		}
		else
		{
			if (!kInfo.hasTerritory(OUTCOME_TERRITORY_NEUTRAL))
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

	if (!oc_gateHolds(m_pPlotCondition, kUnit, &kPlot))
	{
		return false;
	}

	if (!oc_gateHolds(m_pUnitCondition, kUnit, &kPlot))
	{
		return false;
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

	const bool bToCoastalCity = GC.getOutcomeInfo(getType()).hasPlacement(OUTCOME_PLACEMENT_COASTAL_CITY);

	const CvUnitInfo* pUnitInfo =
	(
		eDefeatedUnitType > NO_UNIT
		?
		pUnitInfo = &GC.getUnitInfo(eDefeatedUnitType)
		:
		pUnitInfo = &kUnit.getUnitInfo()
	);

	const CvWString& szMessage = GC.getOutcomeInfo(getType()).getMessageKey();
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
			GC.getUnitInfo(m_eUnitType).hasCombatClass(GC.getUNITCOMBAT_SUBDUED())
			||
			GC.getUnitInfo(m_eUnitType).hasCombatClass(GC.getUNITCOMBAT_IDEA())
		)
	);

	const CvPlot* pSpawnPlot = kUnit.plot();
	if (m_bUnitToCapital && kPlayer.getCapitalCity() != NULL)
	{
		pSpawnPlot = kPlayer.getCapitalCity()->plot();
	}
	else if (m_pSpawnAnywhere != NULL)
	{
		pSpawnPlot = oc_randomPlotWhere(m_pSpawnAnywhere, kUnit);
	}
	if (m_eUnitType > NO_UNIT && !bUnitToCity && pSpawnPlot != NULL)
	{
		CvUnit* pUnit = kPlayer.createUnit(m_eUnitType, pSpawnPlot->getX(), pSpawnPlot->getY(), GC.getUnitInfo(m_eUnitType).getDefaultUnitAI());

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
				CvUnit* pUnit = kPlayer.createUnit(m_eUnitType, pCity->getX(), pCity->getY(), GC.getUnitInfo(m_eUnitType).getDefaultUnitAI());

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

	if (m_eTerraformTerrain != NO_TERRAIN)
	{
		kUnit.plot()->setTerrainType(m_eTerraformTerrain, true, true);
	}

	if (m_bFound)
	{
		kPlayer.found(kUnit.getX(), kUnit.getY(), &kUnit);
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
	const bool bToCoastalCity = GC.getOutcomeInfo(getType()).hasPlacement(OUTCOME_PLACEMENT_COASTAL_CITY);
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
		iValue += kPlayer.AI_unitValue(m_eUnitType, GC.getUnitInfo(m_eUnitType).getDefaultUnitAI(), kPlot.area());
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
				if (pCity->netHappiness(1) < 0)
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

	return iValue;
}


//	ONE outcomes.kill[] / outcomes.actions[] entry -> this outcome. The verb-per-payload vocabulary of
//	[json.md] par.8; the engine below is untouched, only the read path is JSON (mission-outcome-system.md).
//	Idempotent by contract: every member is fully redefined, owned expressions freed first.
void CvOutcome::mapFrom(const picojson::value& v)
{
	SAFE_DELETE(m_iChance);
	SAFE_DELETE(m_bUnitToCity);
	SAFE_DELETE(m_iReduceAnarchyLength);
	SAFE_DELETE(m_pPlotCondition);
	SAFE_DELETE(m_pUnitCondition);
	SAFE_DELETE(m_pSpawnAnywhere);
	for (int i = 0; i < NUM_YIELD_TYPES; i++)
	{
		SAFE_DELETE(m_aiYield[i]);
	}
	for (int i = 0; i < NUM_COMMERCE_TYPES; i++)
	{
		SAFE_DELETE(m_aiCommerce[i]);
	}
	m_eType = NO_OUTCOME;
	m_eTerraformTerrain = NO_TERRAIN;
	m_bUnitToCapital = false;
	m_bFound = false;
	m_eUnitType = NO_UNIT;
	m_ePromotionType = NO_PROMOTION;
	m_eBonusType = NO_BONUS;
	m_eGPUnitType = NO_UNIT;
	m_eEventTrigger = NO_EVENTTRIGGER;
	m_iGPP = 0;
	m_iChancePerPop = 0;
	m_iHappinessTimer = 0;
	m_iPopulationBoost = 0;
	m_bKill = false;
	m_szPythonCallback.clear();

	if (!v.is<picojson::object>())
	{
		return;
	}
	const picojson::object& o = v.get<picojson::object>();

	//	`requires` is the GATE: the OUTCOME_* id (identity + replace tier) plus the plot/unit condition trees,
	//	which parse through the ONE human->data boundary into typed nodes.
	const picojson::object* pRequires = jsonChildObj(o, "requires");
	if (pRequires != NULL)
	{
		m_eType = static_cast<OutcomeTypes>(jsonIdFk(*pRequires, "outcome"));

		picojson::object::const_iterator itGate = pRequires->find("plot");
		if (itGate != pRequires->end())
		{
			m_pPlotCondition = cascadeParseCondition(itGate->second);
		}
		itGate = pRequires->find("unit");
		if (itGate != pRequires->end())
		{
			m_pUnitCondition = cascadeParseCondition(itGate->second);
		}
	}

	m_iChance = oc_intExpr(o, "chance");
	m_iChancePerPop = jsonIdInt(o, "chancePerPop");
	m_bKill = jsonIdBool(o, "consumes");
	m_iPopulationBoost = jsonIdInt(o, "population");
	m_iReduceAnarchyLength = oc_intExpr(o, "revolution");
	m_ePromotionType = static_cast<PromotionTypes>(jsonIdFk(o, "promotes"));
	m_eBonusType = static_cast<BonusTypes>(jsonIdFk(o, "places"));
	m_eEventTrigger = static_cast<EventTriggerTypes>(jsonIdFk(o, "triggers"));
	m_bFound = jsonIdBool(o, "found");

	const picojson::object* pTerraform = jsonChildObj(o, "terraform");
	if (pTerraform != NULL)
	{
		m_eTerraformTerrain = static_cast<TerrainTypes>(jsonIdFk(*pTerraform, "terrain"));
	}

	//	`spawns` -- the unit, and WHERE it arrives: on the acting unit's plot by default; in a CITY (`toCity`,
	//	a bare true or a condition such as a tech requirement); in the CAPITAL (`toCapital`); or on a random
	//	map plot where a condition holds (`anywhere`).
	const picojson::object* pSpawns = jsonChildObj(o, "spawns");
	if (pSpawns != NULL)
	{
		m_eUnitType = static_cast<UnitTypes>(jsonIdFk(*pSpawns, "unit"));
		m_bUnitToCapital = jsonIdBool(*pSpawns, "toCapital");

		picojson::object::const_iterator itAnywhere = pSpawns->find("anywhere");
		if (itAnywhere != pSpawns->end())
		{
			m_pSpawnAnywhere = cascadeParseCondition(itAnywhere->second);
		}

		picojson::object::const_iterator itToCity = pSpawns->find("toCity");
		if (itToCity != pSpawns->end())
		{
			if (itToCity->second.is<bool>())
			{
				// an UNCONDITIONAL to-city: the empty AND group, which holds by construction
				m_bUnitToCity = itToCity->second.get<bool>() ? new CvCondition() : NULL;
			}
			else
			{
				m_bUnitToCity = cascadeParseCondition(itToCity->second);
			}
		}
	}

	const picojson::object* pGreatPeople = jsonChildObj(o, "greatPeople");
	if (pGreatPeople != NULL)
	{
		m_iGPP = jsonIdInt(*pGreatPeople, "points");
		m_eGPUnitType = static_cast<UnitTypes>(jsonIdFk(*pGreatPeople, "unit"));
	}

	//	a TIMED happiness pulse -- `duration` is the turns it lasts, not a magnitude.
	const picojson::object* pHappiness = jsonChildObj(o, "happiness");
	if (pHappiness != NULL)
	{
		m_iHappinessTimer = jsonIdInt(*pHappiness, "duration");
	}

	//	The one-shot YIELDS and COMMERCES reuse the ordinary family words ([json.md] par.8), so each maps
	//	straight onto its engine channel; a rolled amount arrives as the {base, random} pair.
	static const char* const kYieldKeys[NUM_YIELD_TYPES] = { "food", "production", "commerce" };
	for (int iYield = 0; iYield < NUM_YIELD_TYPES; iYield++)
	{
		m_aiYield[iYield] = oc_intExpr(o, kYieldKeys[iYield]);
	}
	static const char* const kCommerceKeys[NUM_COMMERCE_TYPES] = { "gold", "research", "culture", "espionage" };
	for (int iCommerce = 0; iCommerce < NUM_COMMERCE_TYPES; iCommerce++)
	{
		m_aiCommerce[iCommerce] = oc_intExpr(o, kCommerceKeys[iCommerce]);
	}

	//	A named callback into CvOutcomeInterface. JSON carries no logic, so an inline `code` body is refused
	//	(mission-outcome-system.md) and surfaces on the load census rather than running.
	const picojson::object* pPython = jsonChildObj(o, "python");
	if (pPython != NULL)
	{
		std::string szValue;
		if (jsonIdStr(*pPython, "callback", szValue))
		{
			m_szPythonCallback = szValue.c_str();
		}
		if (pPython->find("code") != pPython->end())
		{
			std::string szOutcome;
			if (pRequires == NULL || !jsonIdStr(*pRequires, "outcome", szOutcome))
			{
				szOutcome = "outcome";
			}
			jsonNoteUnconsumed(szOutcome, "python.code");
		}
	}
}

void CvOutcome::copyNonDefaults(CvOutcome* pOutcome)
{
	PROFILE_EXTRA_FUNC();
	GC.copyNonDefaultDelayedResolution((int*)&m_eType, (int*)&(pOutcome->m_eType));
	//if (m_eType == NO_OUTCOME)
	//{
	//	m_eType = pOutcome->getType();
	//}
	if (!m_iChance)
	{
		m_iChance = pOutcome->m_iChance;
		pOutcome->m_iChance = NULL;
	}
	GC.copyNonDefaultDelayedResolution((int*)&m_eUnitType, (int*)&(pOutcome->m_eUnitType));
	//if (m_eUnitType == NO_UNIT)
	//{
	//	m_eUnitType = pOutcome->getUnitType();
	//}
	if (!m_bUnitToCity)
	{
		m_bUnitToCity = pOutcome->m_bUnitToCity;
		pOutcome->m_bUnitToCity = NULL;
	}
	GC.copyNonDefaultDelayedResolution((int*)&m_ePromotionType, (int*)&(pOutcome->m_ePromotionType));
	//if (m_ePromotionType == NO_PROMOTION)
	//{
	//	m_ePromotionType = pOutcome->getPromotionType();
	//}
	GC.copyNonDefaultDelayedResolution((int*)&m_eBonusType, (int*)&(pOutcome->m_eBonusType));
	//if (m_eBonusType == NO_BONUS)
	//{
	//	m_eBonusType = pOutcome->getBonusType();
	//}
	GC.copyNonDefaultDelayedResolution((int*)&m_eGPUnitType, (int*)&(pOutcome->m_eGPUnitType));
	if (m_iGPP == 0)
	{
		m_iGPP = pOutcome->getGPP();
		//m_eGPUnitType = pOutcome->getGPUnitType();
	}
	if (m_iHappinessTimer == 0)
	{
		m_iHappinessTimer = pOutcome->getHappinessTimer();
	}

	if (m_iPopulationBoost == 0)
	{
		m_iPopulationBoost = pOutcome->getPopulationBoost();
	}

	if (!m_iReduceAnarchyLength)
	{
		m_iReduceAnarchyLength = pOutcome->m_iReduceAnarchyLength;
		pOutcome->m_iReduceAnarchyLength = NULL;
	}

	GC.copyNonDefaultDelayedResolution((int*)&m_eEventTrigger,(int*)&(pOutcome->m_eEventTrigger));

	bool bDefault = true;
	for (int i=0; i<NUM_YIELD_TYPES; i++)
	{
		bDefault = bDefault && !m_aiYield[i];
	}
	if (bDefault)
	{
		for (int i=0; i<NUM_YIELD_TYPES; i++)
		{
			m_aiYield[i] = pOutcome->m_aiYield[i];
			pOutcome->m_aiYield[i] = NULL;
		}
	}
	bDefault = true;
	for (int i=0; i<NUM_COMMERCE_TYPES; i++)
	{
		bDefault = bDefault && !m_aiCommerce[i];
	}
	if (bDefault)
	{
		for (int i=0; i<NUM_COMMERCE_TYPES; i++)
		{
			m_aiCommerce[i] = pOutcome->m_aiCommerce[i];
			pOutcome->m_aiCommerce[i] = NULL;
		}
	}

	m_Properties.copyNonDefaults(pOutcome->getProperties());

	if (!m_pPlotCondition)
	{
		m_pPlotCondition = pOutcome->m_pPlotCondition;
		pOutcome->m_pPlotCondition = NULL;
	}

	if (!m_pUnitCondition)
	{
		m_pUnitCondition = pOutcome->m_pUnitCondition;
		pOutcome->m_pUnitCondition = NULL;
	}

	if (!m_pSpawnAnywhere)
	{
		m_pSpawnAnywhere = pOutcome->m_pSpawnAnywhere;
		pOutcome->m_pSpawnAnywhere = NULL;
	}

	if (m_szPythonCallback.empty()) m_szPythonCallback = pOutcome->m_szPythonCallback;
	if (!m_bKill) m_bKill = pOutcome->m_bKill;
	if (!m_bFound) m_bFound = pOutcome->m_bFound;
	if (!m_bUnitToCapital) m_bUnitToCapital = pOutcome->m_bUnitToCapital;
	if (m_eTerraformTerrain == NO_TERRAIN) m_eTerraformTerrain = pOutcome->m_eTerraformTerrain;
}

void CvOutcome::buildDisplayString(CvWStringBuffer &szBuffer, const CvUnit& kUnit) const
{
	//CvPlayer& kPlayer = GET_PLAYER(kUnit.getOwner());
	const bool bToCoastalCity = GC.getOutcomeInfo(getType()).hasPlacement(OUTCOME_PLACEMENT_COASTAL_CITY);
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
		(GC.getUnitInfo(m_eUnitType).hasCombatClass(GC.getUNITCOMBAT_SUBDUED()) ||
		GC.getUnitInfo(m_eUnitType).hasCombatClass(GC.getUNITCOMBAT_IDEA())))
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

	if (m_eTerraformTerrain != NO_TERRAIN)
	{
		if (!bFirst)
		{
			szBuffer.append(L", ");
		}
		else
		{
			bFirst = false;
		}
		szBuffer.append(GC.getTerrainInfo(m_eTerraformTerrain).getDescription());
	}

	if (m_bFound)
	{
		if (!bFirst)
		{
			szBuffer.append(L", ");
		}
		else
		{
			bFirst = false;
		}
		szBuffer.append(gDLL->getText("TXT_KEY_MISSION_BUILD_CITY"));
	}

	szBuffer.append(L" )");
}

void CvOutcome::getCheckSum(uint32_t& iSum) const
{
	PROFILE_EXTRA_FUNC();
	CheckSum(iSum, m_eType);
	m_iChance->getCheckSum(iSum);
	CheckSum(iSum, m_eUnitType);
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
	CheckSumC(iSum, m_szPythonCallback);
	CheckSum(iSum, m_bKill);
	CheckSum(iSum, m_bFound);
	CheckSum(iSum, m_bUnitToCapital);
	CheckSum(iSum, m_eTerraformTerrain);
}
