//
//	CvCascadeGetterShadow -- the #430 GETTER-CONTRACT instrumentation (see the header). The [GETTER] spine domain:
//	the in-getter cascade-vs-legacy diff at the real call moment, once per (city, plane, channel) per turn.
//

#include "CvGameCoreDLL.h"
#include "CvCascadeGetterShadow.h"
#include "CvCascadeYieldRate.h"        // YieldRate::yieldRate100
#include "CvCascadeCommerceCalc.h"     // CommerceCalc::commerceRate100 + channel(eC)
#include "CvCascadeConditionEval.h"    // CvCascadeEvalCtx
#include "CvCascadeEnablerKernel.h"    // EnablerKernel::computeCityBuildingFacts -- the cascade-computed active set
#include "CvEventSpine.h"
#include "AI/BetterBTSAI.h"            // gPlayerLogLevel
#include "Defines/CvGlobals.h"
#include "Engine/CvGame.h"
#include "Engine/CvCity.h"
#include "Engine/CvPlayer.h"
#include "AI/CvPlayerAI.h"             // GET_PLAYER
#include "AI/CvTeamAI.h"               // GET_TEAM
#include <set>
#include <utility>

// ===================== [GETTER] spine domain (logging.md §4: logging is a spine CONSUMER) =====================
enum GsEvt { GSE_DIFF = 1, GSE_SHADOW };
enum GsFld { GSF_WHO = 1, GSF_CHANNEL, GSF_CASC, GSF_LEG, GSF_CHECKED, GSF_DIVERGING, GSF_TURN };

static const char* gs_prefix(int evt)
{
	switch (evt)
	{
	case GSE_DIFF:   return "[GETTER/diff]";
	case GSE_SHADOW: return "[GETTER/shadow]";
	default:         return "[GETTER]";
	}
}
static const char* gs_field(int tag, SpineFieldType* peType)
{
	*peType = SFT_INT;
	switch (tag)
	{
	case GSF_WHO:       *peType = SFT_WSTR; return "who";
	case GSF_CHANNEL:   *peType = SFT_STR;  return "channel";
	case GSF_CASC:      return "casc100";
	case GSF_LEG:       return "leg100";
	case GSF_CHECKED:   return "checked";
	case GSF_DIVERGING: return "diverging";
	case GSF_TURN:      return "turn";
	default:            return NULL;
	}
}
static void gs_registerDomain()
{
	static bool s_reg = false;
	if (!s_reg) { spineRegisterDomain(SD_GETTER, gs_prefix, "Cascade.log", gs_field); s_reg = true; }
}

// ===================== per-turn state =====================
// ONE compare per (city, plane, channel) per turn: the memo key is ((owner,cityId), plane*8+channel). Counters roll
// up into the [GETTER/shadow] summary, flushed lazily when the first call of the NEXT turn arrives (no doTurn hook).
typedef std::pair<std::pair<int, int>, int> GsKey;
static std::set<GsKey> s_done;
static int s_iTurn = -1;
static int s_iChecked = 0, s_iDiverging = 0, s_iShown = 0;
static bool s_bInShadow = false;   // reentrancy guard: a cascade-internal read of an instrumented getter must not recurse

// Per-turn compute cap: the cascade rate is an on-demand full recompute (no accumulator substrate yet), so an
// uncapped late-game sweep (hundreds of cities x 7 channels) would drag a logged turn. First N call moments win.
static const int GS_MAX_COMPUTES_PER_TURN = 256;   // 1024 -> 256 (2026-07-02): real repos made each compute heavy

static void gs_rollTurn()
{
	const int iTurn = GC.getGame().getGameTurn();
	if (iTurn == s_iTurn) return;
	if (s_iTurn >= 0 && s_iChecked > 0)
	{
		eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_GETTER, GSE_SHADOW, 1)
			.addI(GSF_TURN, s_iTurn).addI(GSF_CHECKED, s_iChecked).addI(GSF_DIVERGING, s_iDiverging));
	}
	s_done.clear();
	s_iChecked = 0; s_iDiverging = 0; s_iShown = 0;
	s_iTurn = iTurn;
}

static void gs_check(const CvCity* pCity, int iPlane, int iChannel, int iLegacy100)
{
	if (gPlayerLogLevel < 1 || pCity == NULL || s_bInShadow) return;
	// LOAD GATE (2026-07-02): the load path recomputes every city's yields/commerce repeatedly, and with the repos
	// populated each instrumented compute does real condition evaluation -- that dragged map loading hard. The
	// getter contract shadow is about REAL consumer calls in a RUNNING game (validation.md end-turn discipline),
	// so it stays silent until the game is fully initialized.
	if (!GC.getGame().isFinalInitialized()) return;
	gs_registerDomain();
	gs_rollTurn();
	if (s_iChecked >= GS_MAX_COMPUTES_PER_TURN) return;

	const GsKey key(std::make_pair((int)pCity->getOwner(), pCity->getID()), iPlane * 8 + iChannel);
	if (!s_done.insert(key).second) return;   // already compared at an earlier real call this turn
	++s_iChecked;

	s_bInShadow = true;
	const CvPlayer& player = GET_PLAYER(pCity->getOwner());
	CvCascadeEvalCtx ctx;
	ctx.city = pCity; ctx.plot = pCity->plot(); ctx.player = &player; ctx.team = &GET_TEAM(player.getTeam());
	std::set<int> activeB, provB;   // cascade-COMPUTED active buildings + vicinity provides (never the engine's dormancy verdict)
	EnablerKernel::computeCityBuildingFacts(pCity, ctx, activeB, provB);
	ctx.activeBuildings = &activeB; ctx.vicinityProvidedBonuses = &provB;

	static const char* aszYield[NUM_YIELD_TYPES] = { "food", "production", "commerce" };
	const char* szChannel;
	long lCascade;
	if (iPlane == 0)
	{
		szChannel = aszYield[iChannel];
		lCascade = YieldRate::yieldRate100(szChannel, (YieldTypes)iChannel, pCity, ctx);
	}
	else
	{
		szChannel = CommerceCalc::channel(iChannel);
		const long yc100 = YieldRate::yieldRate100("commerce", YIELD_COMMERCE, pCity, ctx);
		const long prate = YieldRate::yieldRate100("production", YIELD_PRODUCTION, pCity, ctx) / 100;
		lCascade = CommerceCalc::commerceRate100(szChannel, (CommerceTypes)iChannel, pCity, ctx, yc100, prate);
	}
	s_bInShadow = false;

	if (lCascade != (long)iLegacy100)
	{
		++s_iDiverging;
		if (s_iShown < 40)
		{
			eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_GETTER, GSE_DIFF, 1)
				.addWStr(GSF_WHO, pCity->getName().GetCString()).addStr(GSF_CHANNEL, szChannel)
				.addI(GSF_CASC, (int)lCascade).addI(GSF_LEG, iLegacy100));
			++s_iShown;
		}
	}
}

void cascadeGetterShadowYield(const CvCity* pCity, int iYield, int iLegacy100)
{
	if (iYield < 0 || iYield >= NUM_YIELD_TYPES) return;
	gs_check(pCity, 0, iYield, iLegacy100);
}

void cascadeGetterShadowCommerce(const CvCity* pCity, int iCommerce, int iLegacy100)
{
	if (iCommerce < 0 || iCommerce >= NUM_COMMERCE_TYPES) return;
	gs_check(pCity, 1, iCommerce, iLegacy100);
}
