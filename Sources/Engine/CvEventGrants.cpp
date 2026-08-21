#include "CvGameCoreDLL.h"
#include "Engine/CvEventGrants.h"

void CvEventGrantStore::add(EventGrantDomain eDomain, int eEvent, int eKind, int eTarget, int iValue)
{
	if (iValue == 0)
	{
		return;
	}
	CvEventGrant kGrant;
	kGrant.eEvent = eEvent;
	kGrant.eDomain = eDomain;
	kGrant.eKind = eKind;
	kGrant.eTarget = eTarget;
	kGrant.iValue = iValue;
	m_grants.push_back(kGrant);
}

int CvEventGrantStore::sum(EventGrantDomain eDomain, int eKind, int eTarget) const
{
	int iTotal = 0;
	for (std::vector<CvEventGrant>::const_iterator it = m_grants.begin(); it != m_grants.end(); ++it)
	{
		if ((*it).eDomain == eDomain && (*it).eKind == eKind && (*it).eTarget == eTarget)
		{
			iTotal += (*it).iValue;
		}
	}
	return iTotal;
}

int CvEventGrantStore::size() const
{
	return (int)m_grants.size();
}

const CvEventGrant& CvEventGrantStore::record(int iIndex) const
{
	return m_grants[iIndex];
}

void CvEventGrantStore::clear()
{
	m_grants.clear();
}

void CvEventGrantStore::resize(int iCount)
{
	m_grants.clear();
	m_grants.resize(iCount);
}

CvEventGrant& CvEventGrantStore::mutableRecord(int iIndex)
{
	return m_grants[iIndex];
}
