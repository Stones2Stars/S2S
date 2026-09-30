#include "CvGameCoreDLL.h"
#include "CvDecisionAI.h"

#include "BetterBTSAI.h"
#include "CvGameAI.h"
#include "Defines/CvGlobals.h"
#include "CvPlayerAI.h"
#include "CvTeamAI.h"
#include "Spine/CvEventSpine.h" // #430 logging consolidation: route [DAI] through the event spine (shadow)

// [DAI] decisions -> event spine (CvDecisionAI). Self-registers its prefixes + DecisionAI.log; the spine never names
// DAI. [DAI/begin] and [DAI/flavors] carry a runtime %S / %s arg and stay on the legacy path.
namespace
{
	// [DAI/begin]: runtime %S (getCivilizationDescription) blocks wiring -- legacy only this pass.
	// [DAI/flavors]: runtime %s (getFlavorTypes name) blocks wiring -- legacy only this pass.
	enum DaiEvent
	{
		DAI_BEGIN = 0,   // [DAI/begin]  -- blocked: runtime civ-description string; no emit wired
		DAI_FLAVORS,     // [DAI/flavors] -- blocked: runtime flavor-name string; no emit wired
		DAI_TECH_STATE   // [DAI/tech/state] -- the research state a player enters its turn with
	};
	const char* daiLinePrefix(int iEventId)
	{
		switch (iEventId)
		{
		case DAI_BEGIN:      return "[DAI/begin]";
		case DAI_FLAVORS:    return "[DAI/flavors]";
		case DAI_TECH_STATE: return "[DAI/tech/state]";
		default:             return NULL;
		}
	}
	enum DaiField
	{
		DAIF_player = 0, DAIF_turn, DAIF_era, DAIF_value,
		DAIF_current, DAIF_progress, DAIF_cost, DAIF_beakers, DAIF_researchPercent, DAIF_goldPercent,
		DAIF_gold, DAIF_financialTrouble, DAIF_listed, DAIF_target, DAIF_targetHeld
	};
	const char* daiFieldInfo(int iFieldTag, SpineFieldType* peType)
	{
		*peType = SFT_INT;
		switch (iFieldTag)
		{
		case DAIF_player:           *peType = SFT_PLAYER; return "player";
		case DAIF_turn:             return "turn";
		case DAIF_era:              return "era";
		case DAIF_value:            return "value";
		case DAIF_current:          *peType = SFT_TECH; return "current";
		case DAIF_progress:         return "progress";
		case DAIF_cost:             return "cost";
		case DAIF_beakers:          return "beakers";
		case DAIF_researchPercent:  return "researchPercent";
		case DAIF_goldPercent:      return "goldPercent";
		case DAIF_gold:             return "gold";
		case DAIF_financialTrouble: return "financialTrouble";
		case DAIF_listed:           return "listed";
		case DAIF_target:           *peType = SFT_TECH; return "target";
		case DAIF_targetHeld:       return "targetHeld";
		default:                    return NULL;
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

	if (m_owner == NO_PLAYER)
	{
		return;
	}

	const CvPlayerAI& kPlayer = GET_PLAYER(m_owner);

	if (!kPlayer.isNPC() && kPlayer.isAlive())
	{
		std::vector<int> listedTechs;
		kPlayer.m_enabler.techs.listedIds(listedTechs);
		const TechTypes eCurrentResearch = kPlayer.getCurrentResearch();
		const CvTeamAI& kTeam = GET_TEAM(kPlayer.getTeam());
		eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_DECISION, DAI_TECH_STATE, 4)
			.addI(DAIF_player, (int)m_owner)
			.addI(DAIF_current, (int)eCurrentResearch)
			.addI(DAIF_progress, eCurrentResearch != NO_TECH ? kTeam.getResearchProgress(eCurrentResearch) : -1)
			.addI(DAIF_cost, eCurrentResearch != NO_TECH ? kTeam.getResearchCost(eCurrentResearch) : -1)
			.addI(DAIF_beakers, kPlayer.calculateResearchRate())
			.addI(DAIF_researchPercent, kPlayer.getCommercePercent(COMMERCE_RESEARCH))
			.addI(DAIF_goldPercent, kPlayer.getCommercePercent(COMMERCE_GOLD))
			.addI(DAIF_gold, static_cast<int>(std::min<int64_t>(kPlayer.getGold(), MAX_INT)))
			.addI(DAIF_financialTrouble, kPlayer.AI_isFinancialTrouble() ? 1 : 0)
			.addI(DAIF_listed, (int)listedTechs.size())
			.addI(DAIF_target, (int)kPlayer.AI_getBestResearchTarget())
			.addI(DAIF_targetHeld, kPlayer.AI_getBestResearchTarget() != NO_TECH && kTeam.isHasTech(kPlayer.AI_getBestResearchTarget()) ? 1 : 0));
	}

	if (gPlayerLogLevel < 1)
	{
		return;
	}

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
