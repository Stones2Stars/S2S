//
//	CvTraitSelection -- see the header. The ONE place the GAMEOPTION_LEADER_* trait rules are composed.
//

#include "CvGameCoreDLL.h"
#include "CvTraitSelection.h"
#include "CvGame.h"
#include "AI/CvGameAI.h"   // GC.getGame() is a CvGameAI& -- the complete type isOption() needs
#include "Infos/CvTraitInfo.h"
#include "Infos/CvLeaderHeadInfo.h"

const std::vector<int>& CvTraitSelection::leaderTraits(const CvLeaderHeadInfo& kLeader)
{
	// ⛔ THE ACTIVE SET ANSWERS ALONE -- there is NO fall-through, in EITHER direction (owner). A complex game may
	// not fill a simple trait because the complex one is absent, and a simple game may not reach the other way.
	// ⚠ An EMPTY active list is not a defect and is not reported: that leader misses its traits, which is how the
	// shipped data stands until the community authors the assignments.
	if (GC.getGame().isOption(GAMEOPTION_LEADER_COMPLEX_TRAITS))
	{
		return kLeader.getComplexTraits();
	}
	return kLeader.getTraits();
}

bool CvTraitSelection::isSelectable(const CvTraitInfo& kTrait, bool bGameStart)
{
	const CvGame& kGame = GC.getGame();

	// 1. The barbarian-selection carve-out: valid at START, and nowhere else.
	if (bGameStart && kTrait.isBarbarianSelectionOnly())
	{
		return true;
	}

	// 2. ALIGNMENT against the option pair.
	if (kTrait.isNegativeTrait())
	{
		if (kGame.isOption(GAMEOPTION_LEADER_NO_NEGATIVE_TRAITS)
		|| (bGameStart
			&& kGame.isOption(GAMEOPTION_LEADER_START_NO_POSITIVE_TRAITS)
			&& kGame.isOption(GAMEOPTION_LEADER_DEVELOPING)))
		{
			return false;
		}
	}
	else if (bGameStart && kGame.isOption(GAMEOPTION_LEADER_START_NO_POSITIVE_TRAITS))
	{
		return false;
	}

	// There is deliberately no third stage. Whether a trait is a base rung or a developed one is not asked here
	// and is not a property this calc can see: it falls out of the ENABLER's completed tree. A base rung roots on
	// TECH_GAME_START's `enables.traits`, so it is LISTED from turn 1; a rung above one is reached only by its
	// predecessor's ladder edge (and, from rank 2, the line's tech), so it is LISTED only once that predecessor is
	// held. The caller gates on that verdict (CvPlayer::canLearnTrait) and this composes only the option rules the
	// availability machine has no business knowing.
	return true;
}
