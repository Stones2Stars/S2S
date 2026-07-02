//
//	CascadeRates -- the #430 flipped-getter rate service (see the header). The compute mirrors the getter
//	instrument's context build (ctx + the cascade-computed city building facts), then the §1/§2 assemblers.
//

#include "CvGameCoreDLL.h"
#include "CvCascadeRateService.h"
#include "CvCascadeYieldRate.h"        // YieldRate::yieldRate100
#include "CvCascadeCommerceCalc.h"     // CommerceCalc::commerceRate100 + channel(eC)
#include "CvCascadeConditionEval.h"    // CvCascadeEvalCtx
#include "CvCascadeEnablerKernel.h"    // computeCityBuildingFacts + factsMemoEvict/Clear
#include "Defines/CvGlobals.h"
#include "AI/CvGameAI.h"               // GC.getGame()
#include "Engine/CvCity.h"
#include "Engine/CvPlayer.h"
#include "AI/CvPlayerAI.h"             // GET_PLAYER
#include "AI/CvTeamAI.h"               // GET_TEAM
#include <map>
#include <set>

// entry validity = (city version, global epoch, commerce epoch for plane 1, turn) all current
struct CrEntry { long val; int ver; int epoch; int cepoch; int turn; };
static std::map<int, CrEntry> s_rateMemo;   // key: cid*8 + plane*4 + channel
static std::map<int, int> s_cityVer;        // cid -> version (absent = 0)
static int s_epoch = 0;
static int s_commerceEpoch = 0;

static int cr_cid(const CvCity* pCity) { return ((int)pCity->getOwner()) * 100000 + pCity->getID(); }

void CascadeRates::invalidateCity(const CvCity* pCity)
{
	if (pCity == NULL) return;
	++s_cityVer[cr_cid(pCity)];
	EnablerKernel::factsMemoEvict((int)pCity->getOwner(), pCity->getID());
}

void CascadeRates::invalidateCityRates(const CvCity* pCity)
{
	if (pCity == NULL) return;
	++s_cityVer[cr_cid(pCity)];
}

void CascadeRates::invalidateAll()
{
	++s_epoch;
	EnablerKernel::factsMemoClear();
}

void CascadeRates::invalidateCommerce()
{
	++s_commerceEpoch;
}

static long cr_compute(const CvCity* pCity, int iPlane, int iChannel);

static long cr_get(const CvCity* pCity, int iPlane, int iChannel)
{
	if (pCity == NULL) return 0;
	const int cid = cr_cid(pCity);
	const int iKey = cid * 8 + iPlane * 4 + iChannel;
	std::map<int, int>::const_iterator vit = s_cityVer.find(cid);
	const int iVer = (vit != s_cityVer.end()) ? vit->second : 0;
	const int iTurn = GC.getGame().getGameTurn();
	std::map<int, CrEntry>::const_iterator it = s_rateMemo.find(iKey);
	if (it != s_rateMemo.end() && it->second.ver == iVer && it->second.epoch == s_epoch && it->second.turn == iTurn
	&& (iPlane == 0 || it->second.cepoch == s_commerceEpoch))
	{
		return it->second.val;
	}
	const long lVal = cr_compute(pCity, iPlane, iChannel);
	CrEntry e; e.val = lVal; e.ver = iVer; e.epoch = s_epoch; e.cepoch = s_commerceEpoch; e.turn = iTurn;
	s_rateMemo[iKey] = e;
	return lVal;
}

static long cr_compute(const CvCity* pCity, int iPlane, int iChannel)
{
	const CvPlayer& player = GET_PLAYER(pCity->getOwner());
	CvCascadeEvalCtx ctx;
	ctx.city = pCity; ctx.plot = pCity->plot(); ctx.player = &player; ctx.team = &GET_TEAM(player.getTeam());
	std::set<int> activeB, provB;   // cascade-COMPUTED active buildings + vicinity provides (never the engine's dormancy verdict)
	EnablerKernel::computeCityBuildingFacts(pCity, ctx, activeB, provB);
	ctx.activeBuildings = &activeB; ctx.vicinityProvidedBonuses = &provB;

	static const char* aszYield[NUM_YIELD_TYPES] = { "food", "production", "commerce" };
	if (iPlane == 0)
	{
		return YieldRate::yieldRate100(aszYield[iChannel], (YieldTypes)iChannel, pCity, ctx);
	}
	// commerce derives from the CASCADE commerce/production rates -- routed back through the memo so the four
	// commerce channels reuse the two yield computes instead of re-deriving them per channel
	const long yc100 = cr_get(pCity, 0, (int)YIELD_COMMERCE);
	const long prate = cr_get(pCity, 0, (int)YIELD_PRODUCTION) / 100;
	return CommerceCalc::commerceRate100(CommerceCalc::channel(iChannel), (CommerceTypes)iChannel, pCity, ctx, yc100, prate);
}

long CascadeRates::yieldRate100(const CvCity* pCity, YieldTypes eY)
{
	if (eY < 0 || eY >= NUM_YIELD_TYPES) return 0;
	return cr_get(pCity, 0, (int)eY);
}

long CascadeRates::commerceRate100(const CvCity* pCity, CommerceTypes eC)
{
	if (eC < 0 || eC >= NUM_COMMERCE_TYPES) return 0;
	return cr_get(pCity, 1, (int)eC);
}
