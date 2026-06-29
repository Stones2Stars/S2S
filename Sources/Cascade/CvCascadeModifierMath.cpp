//
//	CvCascadeModifierMath -- the #430 modifier machine, INCREMENT 1: the percent stack. See the header + the build plan.
//	Ports StoneBase's PercentStack (Application/Features/Calc/PercentStack.cs over ModifierMath.SumUnitAtScope): the
//	single additive `modifier = max(0, 100 + Σ {channel}.<scope>.percent)` over every active source. Shadowed vs the
//	legacy getBaseYieldRateModifier (CvCity.cpp:11174 = 100 + building/bonus/power/player/area/capital mods).
//
//	⚠ Scale: a `percent` deposit is stored ×100 (readJson's blanket ×100), but the modifier stack is HUMAN percent (the
//	legacy modifier is 100 + Σ whole percents), so each deposit contributes value100/100.
//	⏳ Increment-1 scope: city/area/empire-scope percents from active buildings, empire buildings, civics, traits. The
//	PURE_TRAITS filter, civic building-keyed percents, and projects are follow-ons (modifier-machine.md). Divergences
//	from not-yet-modelled sources (events) and deferred predicates (HAS_POWER/HAS_BONUS, currently ignored→always-true)
//	are EXPECTED and surfaced by the shadow (validation.md: a divergence is a gap mapped to a named source).
//

#include "CvGameCoreDLL.h"
#include "CvCascadeModifierMath.h"
#include "CvCascadeData.h"             // CvCascadeData + cascadeForInfo
#include "AI/BetterBTSAI.h"            // gPlayerLogLevel + streamLogTee
#include "Defines/CvGlobals.h"
#include "Engine/CvCity.h"
#include "Engine/CvPlayer.h"
#include "Engine/CvGameObject.h"       // CvGameObjectCity -> CvGameObject (BoolExpr eval target)
#include "Infrastructure/BoolExpr.h"
#include "Infos/CvBuildingInfo.h"
#include "Infos/CvCivicInfo.h"
#include "Infos/CvTraitInfo.h"
#include <string>

// A deposit applies iff enabled holds (or is absent) AND disabled does NOT hold (json.md §3.9), evaluated against the
// city's game object.
static bool mm_applies(const BoolExpr* enabled, const BoolExpr* disabled, const CvGameObject* ctx)
{
	if (enabled != NULL && !enabled->evaluate(ctx)) return false;
	if (disabled != NULL && disabled->evaluate(ctx)) return false;
	return true;
}

// Sum a channel's SCOPE-WIDE percent deposits (address == "<family>.<scope>", unit "percent"), gated, as HUMAN percent.
static int mm_sumPercent(const CvCascadeData* d, const std::string& wantAddress, const CvGameObject* ctx)
{
	int sum = 0;
	for (size_t i = 0; i < d->deposits.size(); ++i)
	{
		const CvCascadeDeposit& dep = d->deposits[i];
		if (dep.unit != "percent" || dep.address != wantAddress) continue;
		if (!mm_applies(dep.enabled, dep.disabled, ctx)) continue;
		sum += dep.value100 / 100;
	}
	return sum;
}

// The percent stack for one channel at one city: max(0, 100 + Σ percent) over active city buildings (city+area),
// empire buildings (empire), adopted civics (empire), and the player's active traits (empire).
static int mm_percentStack(const std::string& channel, const CvCity* pCity)
{
	const CvGameObject* ctx = pCity->getGameObject();
	const CvPlayer& player = GET_PLAYER(pCity->getOwner());
	const std::string wantCity = channel + ".city";
	const std::string wantArea = channel + ".area";
	const std::string wantEmpire = channel + ".empire";
	int sum = 0;

	const int nB = GC.getNumBuildingInfos();
	for (int b = 0; b < nB; ++b)
	{
		const BuildingTypes eB = (BuildingTypes)b;
		const bool active = pCity->isActiveBuilding(eB);          // non-dormant, in this city
		const bool owned = player.getBuildingCount(eB) > 0;       // anywhere in the empire
		if (!active && !owned) continue;
		const CvCascadeData* d = cascadeForInfo(&GC.getBuildingInfo(eB));
		if (d == NULL) continue;
		if (active) { sum += mm_sumPercent(d, wantCity, ctx); sum += mm_sumPercent(d, wantArea, ctx); }
		if (owned) sum += mm_sumPercent(d, wantEmpire, ctx);
	}
	for (int co = 0; co < GC.getNumCivicOptionInfos(); ++co)
	{
		const CivicTypes c = player.getCivics((CivicOptionTypes)co);
		if (c == NO_CIVIC) continue;
		const CvCascadeData* d = cascadeForInfo(&GC.getCivicInfo(c));
		if (d != NULL) sum += mm_sumPercent(d, wantEmpire, ctx);
	}
	for (int t = 0; t < GC.getNumTraitInfos(); ++t)
	{
		if (!player.hasTrait((TraitTypes)t)) continue;
		const CvCascadeData* d = cascadeForInfo(&GC.getTraitInfo((TraitTypes)t));
		if (d != NULL) sum += mm_sumPercent(d, wantEmpire, ctx);
	}
	return std::max(0, 100 + sum);
}

void cvCascadeModifierShadow()
{
	static bool s_done = false;
	if (s_done || gPlayerLogLevel < 1) return;
	s_done = true;

	const YieldTypes aeYield[3] = { YIELD_FOOD, YIELD_PRODUCTION, YIELD_COMMERCE };
	const char* aszChannel[3] = { "food", "production", "commerce" };
	int iChecked = 0, iDiverging = 0, iShown = 0;
	char szBuf[1024];

	for (int p = 0; p < MAX_PLAYERS && iChecked < 90; ++p)
	{
		const CvPlayer& player = GET_PLAYER((PlayerTypes)p);
		if (!player.isAlive()) continue;
		int iLoop;
		for (const CvCity* pCity = player.firstCity(&iLoop); pCity != NULL && iChecked < 90; pCity = player.nextCity(&iLoop))
		{
			for (int y = 0; y < 3; ++y)
			{
				++iChecked;
				const int iCascade = mm_percentStack(aszChannel[y], pCity);
				const int iLegacy = pCity->getBaseYieldRateModifier(aeYield[y]);
				if (iCascade != iLegacy)
				{
					++iDiverging;
					if (iShown < 16)
					{
						sprintf(szBuf, "[MODIFIER/diff] %S %s cascade=%d legacy=%d", pCity->getName().GetCString(), aszChannel[y], iCascade, iLegacy);
						gDLL->logMsg("Cascade.log", szBuf); streamLogTee(1, szBuf); ++iShown;
					}
				}
			}
		}
	}
	sprintf(szBuf, "[MODIFIER/shadow] percentStack checked=%d diverging=%d", iChecked, iDiverging);
	gDLL->logMsg("Cascade.log", szBuf); streamLogTee(1, szBuf);
}
