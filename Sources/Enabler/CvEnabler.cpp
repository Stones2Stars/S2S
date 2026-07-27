//
//	The standardized enabler component (see the header): the tri-state + refcount storage and the membership
//	FORMULA (enabler.md par.7.1 step 1) -- removal wins, whatever order the deltas arrive in.
//

#include "CvGameCoreDLL.h"
#include "Enabler/CvEnabler.h"

void EnablerDomain::init(int iCount)
{
	m_aState.assign(iCount, (unsigned char)STATE_HIDDEN);
	m_aiEnable.assign(iCount, (short)0);
	m_aiRemove.assign(iCount, (short)0);
	m_aFlags.assign(iCount, (unsigned char)0);
	m_bSeeded = true;
}

void EnablerDomain::reset()
{
	m_aState.clear();
	m_aiEnable.clear();
	m_aiRemove.clear();
	m_aFlags.clear();
	m_bSeeded = false;
}

// THE MEMBERSHIP FORMULA -- never an operation sequence. Removal wins regardless of arrival order (the
// sequenced add/erase delta is banned: a late enables-add must not resurrect a removed candidate). The
// requires-gate verdict (FLAG_GATE_FAILED) only splits a member's state LISTED/GREYED -- never membership.
void EnablerDomain::refresh(int iId)
{
	const bool bIn = m_aiEnable[iId] > 0 && m_aiRemove[iId] == 0
		&& (m_aFlags[iId] & (unsigned char)(FLAG_HELD | FLAG_STATIC_EXCLUDED)) == 0;
	m_aState[iId] = !bIn ? (unsigned char)STATE_HIDDEN
		: ((m_aFlags[iId] & (unsigned char)FLAG_GATE_FAILED) != 0 ? (unsigned char)STATE_GREYED
		                                                          : (unsigned char)STATE_LISTED);
}

void EnablerDomain::addEnable(int iId, int iDelta)
{
	if (!inRange(iId)) return;
	m_aiEnable[iId] = (short)(m_aiEnable[iId] + iDelta);
	FAssertMsg(m_aiEnable[iId] >= 0, "EnablerDomain enable-refcount went negative -- a lost-source delta without its acquire");
	refresh(iId);
}

void EnablerDomain::addRemove(int iId, int iDelta)
{
	if (!inRange(iId)) return;
	m_aiRemove[iId] = (short)(m_aiRemove[iId] + iDelta);
	FAssertMsg(m_aiRemove[iId] >= 0, "EnablerDomain remove-refcount went negative -- a lost-source delta without its acquire");
	refresh(iId);
}

void EnablerDomain::setHeld(int iId, bool bHeld)
{
	if (!inRange(iId)) return;
	if (bHeld) m_aFlags[iId] |= (unsigned char)FLAG_HELD;
	else       m_aFlags[iId] &= (unsigned char)~FLAG_HELD;
	refresh(iId);
}

void EnablerDomain::setStaticExcluded(int iId, bool bExcluded)
{
	if (!inRange(iId)) return;
	if (bExcluded) m_aFlags[iId] |= (unsigned char)FLAG_STATIC_EXCLUDED;
	else           m_aFlags[iId] &= (unsigned char)~FLAG_STATIC_EXCLUDED;
	refresh(iId);
}

void EnablerDomain::setGateFailed(int iId, bool bFailed)
{
	if (!inRange(iId)) return;
	if (bFailed) m_aFlags[iId] |= (unsigned char)FLAG_GATE_FAILED;
	else         m_aFlags[iId] &= (unsigned char)~FLAG_GATE_FAILED;
	refresh(iId);
}

void EnablerDomain::setQueued(int iId, bool bQueued)
{
	if (!inRange(iId)) return;
	// A read-time OVERLAY only -- it filters listed()/listedIds() (the fresh offer), NOT state/membership. So NO
	// refresh(): a queued building stays LISTED in state (in-tree, continuable via listedForContinue) and is
	// removed from the offer solely by the listed() read applying FLAG_QUEUED. (Stale FLAG_QUEUED on a
	// non-LISTED id is harmless -- listed() checks STATE_LISTED first.)
	if (bQueued) m_aFlags[iId] |= (unsigned char)FLAG_QUEUED;
	else         m_aFlags[iId] &= (unsigned char)~FLAG_QUEUED;
}

bool EnablerDomain::isHeld(int iId) const
{
	return inRange(iId) && (m_aFlags[iId] & (unsigned char)FLAG_HELD) != 0;
}

unsigned char EnablerDomain::state(int iId) const
{
	return inRange(iId) ? m_aState[iId] : (unsigned char)STATE_HIDDEN;
}

int EnablerDomain::enableCount(int iId) const
{
	return inRange(iId) ? (int)m_aiEnable[iId] : 0;
}

int EnablerDomain::removeCount(int iId) const
{
	return inRange(iId) ? (int)m_aiRemove[iId] : 0;
}

bool EnablerDomain::isStaticExcluded(int iId) const
{
	return inRange(iId) && (m_aFlags[iId] & (unsigned char)FLAG_STATIC_EXCLUDED) != 0;
}

void EnablerDomain::listedIds(std::vector<int>& out) const
{
	out.clear();
	const int iN = (int)m_aState.size();
	for (int iId = 0; iId < iN; ++iId)
	{
		// the FRESH OFFER: LISTED and not currently queued (FLAG_QUEUED overlay) -- the AI's production loops
		// iterate only what they can newly START, exactly as canConstruct(bContinue=false) answers per id.
		if (m_aState[iId] == (unsigned char)STATE_LISTED
			&& (m_aFlags[iId] & (unsigned char)FLAG_QUEUED) == 0) out.push_back(iId);
	}
}

void EnablerDomain::inTreeIds(std::vector<int>& out) const
{
	out.clear();
	const int iN = (int)m_aState.size();
	for (int iId = 0; iId < iN; ++iId)
	{
		if (m_aState[iId] >= (unsigned char)STATE_GREYED) out.push_back(iId);
	}
}
