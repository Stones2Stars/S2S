//
//	IntExprCascadeCount -- see the header. The evaluate() is the ONLY instance-facing part: it converts the eval target
//	(a CvGameObject) to its owning player and reads that player's live tally count. It lives HERE, on the
//	live-game-state side, so readJson stays static-data-only (owner ruling 2026-06-30). Lifted verbatim from the inline
//	class that used to sit in CvCascadeReadJson.cpp.
//

#include "CvGameCoreDLL.h"
#include "CvCascadeCountExpr.h"
#include "Engine/CvGameObject.h"   // CvGameObjectPlayer -- the eval target's owner
#include "Engine/CvPlayer.h"       // CvPlayer::getID -- the tally key
#include "CvCascadeTally.h"        // cascadeTally() -- the live cross-city count source

int IntExprCascadeCount::evaluate(const CvGameObject* pObject) const
{
	if (pObject == NULL) return 0;
	CvGameObjectPlayer* pOwner = pObject->getOwner();
	if (pOwner == NULL || pOwner->getPlayer() == NULL) return 0;
	const int iPlayer = pOwner->getPlayer()->getID();
	if (m_eGOM == GOM_BUILDING) return cascadeTally().buildingCount(iPlayer, m_iID);
	if (m_eGOM == GOM_UNITTYPE) return cascadeTally().unitCount(iPlayer, m_iID);
	return 0; // domain not in the tally yet
}

void IntExprCascadeCount::getCheckSum(uint32_t& iSum) const
{
	iSum = iSum * 31u + (uint32_t)m_eGOM;
	iSum = iSum * 31u + (uint32_t)m_iID;
}

void IntExprCascadeCount::buildDisplayString(CvWStringBuffer& szBuffer) const
{
	szBuffer.append(CvWString(L"tallyCount"));
}

int IntExprCascadeCount::getBindingStrength() const
{
	return 100;
}
