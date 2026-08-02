//
//	CvTraitSelection -- see the header. The ONE place the GAMEOPTION_LEADER_* trait rules are composed.
//

#include "CvGameCoreDLL.h"
#include "CvTraitSelection.h"
#include "CvGame.h"
#include "Infos/CvTraitInfo.h"
#include "Infos/CvLeaderHeadInfo.h"

const std::vector<int>& CvTraitSelection::leaderTraits(const CvLeaderHeadInfo& kLeader)
{
	// The COMPLEX set answers only while its option is live AND the leader actually authored one; a leader
	// with just `traits` keeps working under either option rather than coming up traitless (header).
	if (GC.getGame().isOption(GAMEOPTION_LEADER_COMPLEX_TRAITS) && !kLeader.getComplexTraits().empty())
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
