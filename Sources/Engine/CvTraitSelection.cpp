//
//	CvTraitSelection -- see the header. The ONE place the GAMEOPTION_LEADER_* trait rules are composed.
//

#include "CvGameCoreDLL.h"
#include "CvTraitSelection.h"
#include "CvGame.h"
#include "Infos/CvTraitInfo.h"

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

	// 3. The DEVELOPING gate -- the two option states admit disjoint sets, so this is not a filter that can
	// be skipped when the option is off: with it off, a line member is NOT selectable.
	if (kGame.isOption(GAMEOPTION_LEADER_DEVELOPING))
	{
		return kTrait.getSuccessionPriority() != 0;
	}
	return kTrait.getSuccessionPriority() == 0;
}
