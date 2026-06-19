#include "CvGameCoreDLL.h"
#include "CvDecisionAI.h"

#include "BetterBTSAI.h"
#include "CvGameAI.h"
#include "Defines/CvGlobals.h"
#include "CvPlayerAI.h"
#include "Cascade/CvEventSpine.h" // #430 logging consolidation: route [DAI] through the event spine (shadow)

// #430 logging: [DAI] flavour decisions -> event spine (CvDecisionAI). Self-registers its prefixes + DecisionAI.log;
// the spine never names DAI. Shadow: emits run ALONGSIDE the legacy logDecisionAI calls (diff on /events, then cut).
// NOTE: Both call sites carry a runtime %S / %s arg (civilization description and flavor name) that cannot be
// field-ized under the raw-field model (no string slots). Both lines are therefore LEFT on the legacy path only; no
// spine emits are wired in this pass (see flagged list). The domain infrastructure (registrar + event/field tables)
// is committed so the registration slot is claimed and future wiring has zero shared-file edits.
namespace
{
	// [DAI/begin]: runtime %S (getCivilizationDescription) blocks wiring -- legacy only this pass.
	// [DAI/flavors]: runtime %s (getFlavorTypes name) blocks wiring -- legacy only this pass.
	enum DaiEvent
	{
		DAI_BEGIN = 0,   // [DAI/begin]  -- blocked: runtime civ-description string; no emit wired
		DAI_FLAVORS      // [DAI/flavors] -- blocked: runtime flavor-name string; no emit wired
	};
	const char* daiLinePrefix(int iEventId)
	{
		switch (iEventId)
		{
		case DAI_BEGIN:   return "[DAI/begin]";
		case DAI_FLAVORS: return "[DAI/flavors]";
		default:          return NULL;
		}
	}
	// DAI's LOCAL field tags (plain ints). Fields listed here are those a future wiring would add once the
	// runtime strings are resolved (either dropped, or split into per-value event ids).
	enum DaiField { DAIF_player = 0, DAIF_turn, DAIF_era, DAIF_value };
	const char* daiFieldInfo(int iFieldTag, SpineFieldType* peType)
	{
		*peType = SFT_INT;
		switch (iFieldTag)
		{
		case DAIF_player: return "player";
		case DAIF_turn:   return "turn";
		case DAIF_era:    return "era";
		case DAIF_value:  return "value";
		default:        return NULL;
		}
	}
	struct DecisionLogRegistrar { DecisionLogRegistrar() { spineRegisterDomain(SD_DECISION, &daiLinePrefix, "DecisionAI.log", &daiFieldInfo); } };
	DecisionLogRegistrar s_daiLogRegistrar; // static-init registration; safe (g_domains zero-init before this runs)
}

// ---------------------------------------------------------------------------
// Lifecycle.
// ---------------------------------------------------------------------------
CvDecisionAI::CvDecisionAI(PlayerTypes owner)
	: m_owner(owner)
	, m_lastTurn(-1)
{
}

void CvDecisionAI::onTurnBegin(int gameTurn)
{
	m_lastTurn = gameTurn;

	if (gPlayerLogLevel < 1 || m_owner == NO_PLAYER)
	{
		return;
	}

	const CvPlayerAI& kPlayer = GET_PLAYER(m_owner);

	// Baseline is only meaningful for AI players -- the decision functions all
	// early-out for humans / NPCs, so logging their flavours would be noise.
	if (kPlayer.isHumanPlayer() || kPlayer.isNPC() || !kPlayer.isAlive())
	{
		return;
	}

	logDecisionAI(1, "[DAI/begin] player=%d (%S) turn=%d era=%d",
		(int)m_owner, kPlayer.getCivilizationDescription(0), gameTurn, (int)kPlayer.getCurrentEra());

	for (int iI = 0; iI < GC.getNumFlavorTypes(); iI++)
	{
		logDecisionAI(1, "[DAI/flavors] player=%d flavor=%s value=%d",
			(int)m_owner, GC.getFlavorTypes((FlavorTypes)iI).c_str(),
			kPlayer.AI_getFlavorValue((FlavorTypes)iI));
	}
}
