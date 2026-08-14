//
//	The AS-IF-HELD overlay (see the header): hypothetical HAVE folded over the maintained membership planes,
//	resolved through the ONE par.7.1 formula (EnablerDomain::isMember).
//

#include "CvGameCoreDLL.h"
#include "Enabler/CvEnablerOverlay.h"
#include "Enabler/CvEnabler.h"

namespace
{
	// The overlay's own membership resolution: the maintained planes PLUS this overlay's contributions, through
	// the shared formula. A set hit contributes "at least one", which is all `>0` / `==0` can observe (header).
	bool ov_member(const EnablerDomain& kDomain, int iId,
		const std::set<int>& kEnables, const std::set<int>& kRemoves, const std::set<int>& kHeld)
	{
		const int iEnable = kDomain.enableCount(iId) + (kEnables.count(iId) != 0 ? 1 : 0);
		const int iRemove = kDomain.removeCount(iId) + (kRemoves.count(iId) != 0 ? 1 : 0);
		const bool bHeld = kDomain.isHeld(iId) || kHeld.count(iId) != 0;

		return EnablerDomain::isMember(iEnable, iRemove, bHeld, kDomain.isStaticExcluded(iId));
	}
}

void EnablerOverlay::addHave(const CvInfo* pSource, EnEdgeBucket eSourceBucket, int iSourceId)
{
	if (pSource == NULL)
	{
		return;
	}
	// THE BONUS REFUSAL (header): the data carries bonus `enables` edges that the runtime deliberately does not
	// count, so folding them would manufacture unlocks the real frontier never grants.
	if (eSourceBucket == EDGEB_BONUSES)
	{
		FAssertMsg(false, "EnablerOverlay: a BONUS carries no membership meaning (the bonus axis is GATE-ONLY) -- "
			"asking whether a bonus unlocks something is a requires-gate question, not an overlay one");
		return;
	}
	// The SAME source-side edge walk the maintained appliers use -- enables into the enable plane, the three
	// removal families into the remove plane.
	EnablerKernel::accumHave(pSource, m_enables, m_removes);

	// A source in its own domain leaves that domain's frontier the moment it is held.
	setHeld(eSourceBucket, iSourceId);
}

void EnablerOverlay::setHeld(EnEdgeBucket eBucket, int iId)
{
	if (eBucket == NO_EDGEB || iId < 0)
	{
		return;
	}
	m_held[eBucket].insert(iId);
}

void EnablerOverlay::clear()
{
	for (int iBucket = 0; iBucket < NUM_EDGEB; ++iBucket)
	{
		m_enables.a[iBucket].clear();
		m_removes.a[iBucket].clear();
		m_held.a[iBucket].clear();
	}
}

bool EnablerOverlay::isEmpty() const
{
	for (int iBucket = 0; iBucket < NUM_EDGEB; ++iBucket)
	{
		if (!m_enables.a[iBucket].empty() || !m_removes.a[iBucket].empty() || !m_held.a[iBucket].empty())
		{
			return false;
		}
	}
	return true;
}

bool EnablerOverlay::inTree(const EnablerDomain& kDomain, EnEdgeBucket eBucket, int iId) const
{
	if (eBucket == NO_EDGEB || !kDomain.isSeeded())
	{
		return false;
	}
	return ov_member(kDomain, iId, m_enables[eBucket], m_removes[eBucket], m_held[eBucket]);
}

