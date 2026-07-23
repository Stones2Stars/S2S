//
//	Derived-data repositories -- out-of-line parts of CvDerivedData.h (everything that needs the
//	current game turn or the registry's element type). See the header for the architecture notes.
//

#include "CvGameCoreDLL.h"
#include "CvDerivedData.h"
#include "CvGameAI.h"
#include "CvGlobals.h"

bool TLazyBase::s_bReadOnlyPhase = false;

void TLazyBase::setReadOnlyPhase(bool bReadOnly)
{
	s_bReadOnlyPhase = bReadOnly;
}

bool TLazyBase::isReadOnlyPhase()
{
	return s_bReadOnlyPhase;
}

bool TLazyBase::dirty() const
{
	return m_bDirty
		//	Bounded staleness: a datum with a max age counts as dirty once it is older than that
		//	many turns (its inputs move too often for event-exact invalidation).
		|| (m_iMaxAgeTurns >= 0 && GC.getGame().getGameTurn() - m_iComputedOnTurn > m_iMaxAgeTurns);
}

void TLazyBase::invalidate()
{
	FAssertMsg(!s_bReadOnlyPhase, "derived-data invalidate() during the read-only phase");
	m_bDirty = true;
}

void TLazyBase::markFresh(bool bValueChanged)
{
	FAssertMsg(!s_bReadOnlyPhase, "derived-data recompute during the read-only phase");
	m_bDirty = false;
	m_iComputedOnTurn = GC.getGame().getGameTurn();
	if (bValueChanged)
	{
		m_iVersion++;
	}
}

void CvDataRepositoryBase::invalidateAll()
{
	for (size_t i = 0; i < m_data.size(); i++)
	{
		m_data[i]->invalidate();
	}
}
