//
//	CvScopedAccumulator -- out-of-line parts of the #430 cascade substrate primitive.
//	See CvScopedAccumulator.h for the architecture + the "NOT the derived-data repository" boundary.
//

#include "CvGameCoreDLL.h"
#include "CvScopedAccumulator.h"

void CvScopedAccumulator::deposit(int iKey, int iDelta)
{
	if (iDelta == 0)
	{
		return;
	}
	std::map<int, int>::iterator it = m_sums.find(iKey);
	if (it == m_sums.end())
	{
		m_sums[iKey] = iDelta;
	}
	else
	{
		it->second += iDelta;
		if (it->second == 0)
		{
			m_sums.erase(it); // a sum back to zero == absence; keep the map sparse
		}
	}
}

int CvScopedAccumulator::get(int iKey) const
{
	std::map<int, int>::const_iterator it = m_sums.find(iKey);
	return (it == m_sums.end()) ? 0 : it->second;
}

void CvScopedAccumulator::clear()
{
	m_sums.clear();
}
