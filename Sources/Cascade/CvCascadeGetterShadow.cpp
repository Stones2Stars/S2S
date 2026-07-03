//
//	CvCascadeGetterShadow -- the #430 getter-contract NET (see the header). The [GETTER] spine domain:
//	the flipped getter's slot-vs-legacy diff, counted once per (city, plane, channel) per turn.
//

#include "CvGameCoreDLL.h"
#include "CvCascadeGetterShadow.h"
#include "CvCascadeCommerceCalc.h"     // CommerceCalc::channel(eC) -- the diff-sample channel name
#include "CvEventSpine.h"
#include "AI/BetterBTSAI.h"            // gPlayerLogLevel
#include "Defines/CvGlobals.h"
#include "Engine/CvGame.h"
#include "Engine/CvCity.h"
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
	case GSF_CASC:      return "slot100";
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
// ONE count per (city, plane, channel) per turn: the memo key is ((owner,cityId), plane*8+channel). Counters roll
// up into the [GETTER/shadow] summary, flushed lazily when the first call of the NEXT turn arrives (no doTurn hook).
typedef std::pair<std::pair<int, int>, int> GsKey;
static std::set<GsKey> s_done;
static int s_iTurn = -1;
static int s_iChecked = 0, s_iDiverging = 0, s_iShown = 0;

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

static void gs_net(const CvCity* pCity, int iPlane, int iChannel, const char* szChannel, int iLegacy100, int iSlot100)
{
	if (gPlayerLogLevel < 1 || pCity == NULL) return;
	gs_registerDomain();
	gs_rollTurn();

	const GsKey key(std::make_pair((int)pCity->getOwner(), pCity->getID()), iPlane * 8 + iChannel);
	if (!s_done.insert(key).second) return;   // already counted at an earlier real call this turn
	++s_iChecked;

	if (iSlot100 != iLegacy100)
	{
		++s_iDiverging;
		if (s_iShown < 40)
		{
			eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_GETTER, GSE_DIFF, 1)
				.addWStr(GSF_WHO, pCity->getName().GetCString()).addStr(GSF_CHANNEL, szChannel)
				.addI(GSF_CASC, iSlot100).addI(GSF_LEG, iLegacy100));
			++s_iShown;
		}
	}
}

void cascadeGetterShadowYield(const CvCity* pCity, int iYield, int iLegacy100, int iSlot100)
{
	if (iYield < 0 || iYield >= NUM_YIELD_TYPES) return;
	static const char* aszYield[NUM_YIELD_TYPES] = { "food", "production", "commerce" };
	gs_net(pCity, 0, iYield, aszYield[iYield], iLegacy100, iSlot100);
}

void cascadeGetterShadowCommerce(const CvCity* pCity, int iCommerce, int iLegacy100, int iSlot100)
{
	if (iCommerce < 0 || iCommerce >= NUM_COMMERCE_TYPES) return;
	gs_net(pCity, 1, iCommerce, CommerceCalc::channel(iCommerce), iLegacy100, iSlot100);
}
