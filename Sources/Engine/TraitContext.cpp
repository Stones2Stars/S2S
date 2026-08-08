//
//	TraitContext -- the empire's held-trait set (see the header for the rulings).
//

#include "CvGameCoreDLL.h"
#include "TraitContext.h"
#include "Spine/CvEventSpine.h"
#include "AI/CvPlayerAI.h"                     // GET_PLAYER
#include "Infos/CvTraitInfo.h"
#include "Data/CvDepositRead.h"                // MMKernel::traitData -- the option-selected active trait set
#include "Defines/CvGlobals.h"                 // GC -- the trait count, for the player-init fold only

namespace
{
	TraitContext* traitsFor(int iPlayer)
	{
		if (iPlayer < 0 || iPlayer >= MAX_PLAYERS)
		{
			return NULL;
		}
		return &GET_PLAYER((PlayerTypes)iPlayer).traits();
	}

	//	The dispatch object the spine registers. It owns NO state -- the state is on the players.
	class TraitSpineConsumer : public IEventConsumer
	{
	public:
		int wantedKinds() const { return (1 << EVENTKIND_DOMAIN); }
		void onEvent(const CvSpineEvent& kEvent) { TraitContext::onSpineEvent(kEvent); }
	};

	TraitSpineConsumer s_traitConsumer;
	bool s_bTraitRegistered = false;
}

// ⚖ THE DECLARED INTEREST SET. Everything that maintains held-trait state is named here, at the store.
bool TraitContext::wantsEvent(int iEventId)
{
	switch (iEventId)
	{
	case SEVT_EMPIRE_TRAIT_ADDED:     // iType trait; the direction is the fact's identity, never a re-read
	case SEVT_EMPIRE_TRAIT_REMOVED:
	case SEVT_PLAYER_INIT:            // the initial traits, which no setter announces
		return true;
	default:
		return false;
	}
}

void TraitContext::onSpineEvent(const CvSpineEvent& kEvent)
{
	if (!wantsEvent(kEvent.iEventId))
	{
		return;
	}
	TraitContext* pTraits = traitsFor(kEvent.iC);
	if (pTraits == NULL)
	{
		return;
	}
	switch (kEvent.iEventId)
	{
	// ⛔ The sign comes from WHICH fact arrived. `setHasTraitInternal` has already written the has-array by the
	// time this runs, so asking `hasTrait` here would read the NEW value on both ends and never withdraw.
	case SEVT_EMPIRE_TRAIT_ADDED:
		pTraits->foldTrait(kEvent.iType, +1);
		break;
	case SEVT_EMPIRE_TRAIT_REMOVED:
		pTraits->foldTrait(kEvent.iType, -1);
		break;
	case SEVT_PLAYER_INIT:
		pTraits->foldHeldTraits(+1);
		break;
	default:
		break;
	}
}

void TraitContext::foldTrait(int iTrait, int iSign)
{
	if (iTrait < 0 || iSign == 0)
	{
		return;
	}
	add(iTrait, iSign);
}

// The NEW-GAME build, from a known zero. ⚑ This is the ONE place the registry is walked, and it is a
// lifecycle-start fold rather than a read -- which is the whole distinction this store exists to draw.
void TraitContext::foldHeldTraits(int iSign)
{
	if (m_player == NULL)
	{
		return;
	}
	const int iNumTraits = GC.getNumTraitInfos();
	for (int iTrait = 0; iTrait < iNumTraits; ++iTrait)
	{
		if (m_player->hasTrait((TraitTypes)iTrait))
		{
			foldTrait(iTrait, iSign);
		}
	}
}

// The ACTIVE set's record per held id -- a complex trait carries different values from its simple twin, so the
// raw registry entry may be the wrong set's ([modifier.md] §4).
// ⚠ `it->second > 0` IS the `has(id)` contract spelled out over the whole key space, not the refcount leaking:
// what is asked is presence, and the number is never carried past this line.
void TraitContext::heldTraits(std::vector<HeldTrait>& heldTraits) const
{
	for (std::map<int, int>::const_iterator it = m.begin(); it != m.end(); ++it)
	{
		if (it->second > 0)
		{
			const CvTraitInfo* pTrait = MMKernel::traitData(it->first);
			if (pTrait != NULL)
			{
				HeldTrait kHeld;
				kHeld.id = it->first;
				kHeld.info = pTrait;
				heldTraits.push_back(kHeld);
			}
		}
	}
}

void traitContextRegisterConsumer()
{
	if (s_bTraitRegistered)
	{
		return;
	}
	s_bTraitRegistered = true;
	eventSpine().registerConsumer(&s_traitConsumer);
}
