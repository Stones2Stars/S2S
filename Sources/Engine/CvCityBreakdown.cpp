//
//	CvCityBreakdown -- see the header.
//

#include "CvGameCoreDLL.h"
#include "CvCityBreakdown.h"
#include "CvCity.h"
#include "AI/CvPlayerAI.h"                    // GET_PLAYER
#include "Data/CvInfoValuation.h"             // the per-source deposit walk
#include "Infos/CvBuildingInfo.h"
#include "Infos/CvHeritageInfo.h"
#include "Infos/CvSpecialistInfo.h"
#include "Infos/CvCivicInfo.h"
#include "Infos/CvTechInfo.h"
#include "Infos/CvTraitInfo.h"
#include "Infos/CvProjectInfo.h"
#include "Infos/CvBonusInfo.h"
#include "Infos/CvReligionInfo.h"
#include "Infos/CvCorporationInfo.h"
#include "Infos/CvProcessInfo.h"

namespace
{
	///<summary>The kinds of source a run is made of, in the order the lines list them; NONE is a line about no
	/// source, UNKNOWN a source of no kind below (never in a run).</summary>
	enum SourceKind
	{
		SOURCE_KIND_NONE,
		SOURCE_KIND_BUILDING,
		SOURCE_KIND_HERITAGE,
		SOURCE_KIND_SPECIALIST,
		SOURCE_KIND_CIVIC,
		SOURCE_KIND_TECH,
		SOURCE_KIND_TRAIT,
		SOURCE_KIND_PROJECT,
		SOURCE_KIND_BONUS,
		SOURCE_KIND_RELIGION,
		SOURCE_KIND_CORPORATION,
		SOURCE_KIND_PROCESS,
		SOURCE_KIND_UNKNOWN
	};

	///<summary>The kind of info a line's source is.</summary>
	SourceKind cb_sourceKind(const CvInfo* pSource)
	{
		if (pSource == NULL)
		{
			return SOURCE_KIND_NONE;
		}
		if (dynamic_cast<const CvBuildingInfo*>(pSource) != NULL)
		{
			return SOURCE_KIND_BUILDING;
		}
		if (dynamic_cast<const CvHeritageInfo*>(pSource) != NULL)
		{
			return SOURCE_KIND_HERITAGE;
		}
		if (dynamic_cast<const CvSpecialistInfo*>(pSource) != NULL)
		{
			return SOURCE_KIND_SPECIALIST;
		}
		if (dynamic_cast<const CvCivicInfo*>(pSource) != NULL)
		{
			return SOURCE_KIND_CIVIC;
		}
		if (dynamic_cast<const CvTechInfo*>(pSource) != NULL)
		{
			return SOURCE_KIND_TECH;
		}
		if (dynamic_cast<const CvTraitInfo*>(pSource) != NULL)
		{
			return SOURCE_KIND_TRAIT;
		}
		if (dynamic_cast<const CvProjectInfo*>(pSource) != NULL)
		{
			return SOURCE_KIND_PROJECT;
		}
		if (dynamic_cast<const CvBonusInfo*>(pSource) != NULL)
		{
			return SOURCE_KIND_BONUS;
		}
		if (dynamic_cast<const CvReligionInfo*>(pSource) != NULL)
		{
			return SOURCE_KIND_RELIGION;
		}
		if (dynamic_cast<const CvCorporationInfo*>(pSource) != NULL)
		{
			return SOURCE_KIND_CORPORATION;
		}
		if (dynamic_cast<const CvProcessInfo*>(pSource) != NULL)
		{
			return SOURCE_KIND_PROCESS;
		}
		return SOURCE_KIND_UNKNOWN;
	}

	///<summary>The term of one line standing for several sources of eKind.</summary>
	CityBreakdownTerm cb_groupTerm(SourceKind eKind)
	{
		switch (eKind)
		{
		case SOURCE_KIND_BUILDING:    return CITY_BREAKDOWN_TERM_BUILDINGS;
		case SOURCE_KIND_HERITAGE:    return CITY_BREAKDOWN_TERM_HERITAGES;
		case SOURCE_KIND_SPECIALIST:  return CITY_BREAKDOWN_TERM_SPECIALISTS;
		case SOURCE_KIND_CIVIC:       return CITY_BREAKDOWN_TERM_CIVICS;
		case SOURCE_KIND_TECH:        return CITY_BREAKDOWN_TERM_TECHS;
		case SOURCE_KIND_TRAIT:       return CITY_BREAKDOWN_TERM_TRAITS;
		case SOURCE_KIND_PROJECT:     return CITY_BREAKDOWN_TERM_PROJECTS;
		case SOURCE_KIND_BONUS:       return CITY_BREAKDOWN_TERM_BONUSES;
		case SOURCE_KIND_RELIGION:    return CITY_BREAKDOWN_TERM_RELIGIONS;
		case SOURCE_KIND_CORPORATION: return CITY_BREAKDOWN_TERM_CORPORATIONS;
		default:                      return CITY_BREAKDOWN_TERM_PROCESSES;
		}
	}

	///<summary>Buildings, then heritages, then every other kind; largest first within a kind.</summary>
	bool cb_kindThenValue(const std::pair<int64_t, const CvInfo*>& kLeft, const std::pair<int64_t, const CvInfo*>& kRight)
	{
		const SourceKind eLeftKind = cb_sourceKind(kLeft.second);
		const SourceKind eRightKind = cb_sourceKind(kRight.second);
		if (eLeftKind != eRightKind)
		{
			return eLeftKind < eRightKind;
		}
		return kLeft.first > kRight.first;
	}
}

CvCityBreakdown::CvCityBreakdown()
	: m_eSubject(NUM_CITY_BREAKDOWN_SUBJECTS)
{
}

void CvCityBreakdown::reset(CityBreakdownSubject eSubject)
{
	m_eSubject = eSubject;
	m_lines.clear();
}

int64_t CvCityBreakdown::getTotal() const
{
	for (size_t iLine = 0; iLine < m_lines.size(); ++iLine)
	{
		if (m_lines[iLine].role == CITY_BREAKDOWN_TOTAL)
		{
			return m_lines[iLine].value;
		}
	}
	return 0;
}

int64_t CvCityBreakdown::getTermValue(CityBreakdownTerm eTerm) const
{
	for (size_t iLine = 0; iLine < m_lines.size(); ++iLine)
	{
		if (m_lines[iLine].term == eTerm)
		{
			return m_lines[iLine].value;
		}
	}
	return 0;
}

void CvCityBreakdown::addLine(CityBreakdownRole eRole, CityBreakdownTerm eTerm, const CvInfo* pSource, int64_t iValue)
{
	CityBreakdownLine kLine;
	kLine.role = eRole;
	kLine.term = eTerm;
	kLine.source = pSource;
	kLine.count = 0;
	kLine.hasOwnStack = false;
	kLine.stackBase = 0;
	kLine.stackPercent = 0;
	kLine.value = iValue;
	m_lines.push_back(kLine);
}

void CvCityBreakdown::addAmount(CityBreakdownTerm eTerm, int64_t iValue, int iCount)
{
	if (iValue != 0)
	{
		addLine(CITY_BREAKDOWN_AMOUNT, eTerm, NULL, iValue);
		m_lines.back().count = iCount;
	}
}

void CvCityBreakdown::addStackedAmount(CityBreakdownTerm eTerm, int64_t iBase, int iPercent, int64_t iValue)
{
	if (iValue == 0)
	{
		return;
	}
	addLine(CITY_BREAKDOWN_AMOUNT, eTerm, NULL, iValue);
	m_lines.back().hasOwnStack = (iPercent != 0);
	m_lines.back().stackBase = iBase;
	m_lines.back().stackPercent = iPercent;
}

void CvCityBreakdown::addSourceAmount(const CvInfo* pSource, int64_t iValue, int iCount)
{
	if (iValue != 0)
	{
		addLine(CITY_BREAKDOWN_AMOUNT, CITY_BREAKDOWN_TERM_SOURCE, pSource, iValue);
		m_lines.back().count = iCount;
	}
}

void CvCityBreakdown::addPercent(CityBreakdownTerm eTerm, int iPercent)
{
	if (iPercent != 0)
	{
		addLine(CITY_BREAKDOWN_PERCENT, eTerm, NULL, iPercent);
	}
}

void CvCityBreakdown::addSourcePercent(const CvInfo* pSource, int iPercent)
{
	if (iPercent != 0)
	{
		addLine(CITY_BREAKDOWN_PERCENT, CITY_BREAKDOWN_TERM_SOURCE, pSource, iPercent);
	}
}

void CvCityBreakdown::addSubtotal(int64_t iValue, CityBreakdownTerm eTerm)
{
	addLine(CITY_BREAKDOWN_SUBTOTAL, eTerm, NULL, iValue);
}

void CvCityBreakdown::setTotal(int64_t iValue)
{
	addLine(CITY_BREAKDOWN_TOTAL, CITY_BREAKDOWN_TERM_TOTAL, NULL, iValue);
}

void CvCityBreakdown::addNote(CityBreakdownTerm eTerm)
{
	addLine(CITY_BREAKDOWN_NOTE, eTerm, NULL, 0);
}

void CvCityBreakdown::getSourceRuns(std::vector<CityBreakdownRun>& kRuns) const
{
	size_t iLine = 0;
	while (iLine < m_lines.size())
	{
		const CityBreakdownLine& kFirst = m_lines[iLine];
		const SourceKind eKind = cb_sourceKind(kFirst.source);
		if (kFirst.term != CITY_BREAKDOWN_TERM_SOURCE || eKind == SOURCE_KIND_NONE || eKind == SOURCE_KIND_UNKNOWN)
		{
			++iLine;
			continue;
		}
		size_t iRunEnd = iLine;
		int64_t iTotal = 0;
		while (iRunEnd < m_lines.size() && m_lines[iRunEnd].role == kFirst.role
			&& m_lines[iRunEnd].term == CITY_BREAKDOWN_TERM_SOURCE && cb_sourceKind(m_lines[iRunEnd].source) == eKind)
		{
			iTotal += m_lines[iRunEnd].value;
			++iRunEnd;
		}
		if (iRunEnd - iLine > 1)
		{
			CityBreakdownRun kRun;
			kRun.firstLine = iLine;
			kRun.lineCount = iRunEnd - iLine;
			kRun.role = kFirst.role;
			kRun.term = cb_groupTerm(eKind);
			kRun.total = iTotal;
			kRuns.push_back(kRun);
		}
		iLine = iRunEnd;
	}
}

void CvCityBreakdown::addSourceLines(const std::map<const CvInfo*, int64_t>& kPerSource, CityBreakdownRole eRole,
	int64_t iTotal, const std::map<const CvInfo*, int>* pCounts)
{
	std::vector<std::pair<int64_t, const CvInfo*> > kSources;
	int64_t iNamed = 0;
	for (std::map<const CvInfo*, int64_t>::const_iterator itSource = kPerSource.begin(); itSource != kPerSource.end(); ++itSource)
	{
		if (itSource->second == 0)
		{
			continue;
		}
		iNamed += itSource->second;
		kSources.push_back(std::make_pair(itSource->second, itSource->first));
	}
	std::sort(kSources.begin(), kSources.end(), cb_kindThenValue);
	const bool bPercent = (eRole == CITY_BREAKDOWN_PERCENT);
	for (size_t iSource = 0; iSource < kSources.size(); ++iSource)
	{
		if (bPercent)
		{
			addSourcePercent(kSources[iSource].second, (int)kSources[iSource].first);
		}
		else
		{
			int iCount = 0;
			if (pCounts != NULL)
			{
				const std::map<const CvInfo*, int>::const_iterator itCount = pCounts->find(kSources[iSource].second);
				iCount = (itCount != pCounts->end()) ? itCount->second : 0;
			}
			addSourceAmount(kSources[iSource].second, kSources[iSource].first, iCount);
		}
	}
	if (bPercent)
	{
		addPercent(CITY_BREAKDOWN_TERM_OTHER, (int)(iTotal - iNamed));
	}
	else
	{
		addAmount(CITY_BREAKDOWN_TERM_OTHER, iTotal - iNamed);
	}
}

void CvCityBreakdown::addPercentSources(const CvCity& city, int iChannel, int iPercentSum)
{
	if (iChannel < 0 || city.getOwner() == NO_PLAYER)
	{
		return;
	}
	std::vector<InfoValuation::RefusedDeposit> kDeposits;
	std::vector<InfoValuation::RefusedDeposit> kUpperDeposits;
	InfoValuation::cityRefusedDeposits(city, iChannel, kDeposits);
	InfoValuation::upperScopeDeposits(GET_PLAYER(city.getOwner()), iChannel, kUpperDeposits);
	kDeposits.insert(kDeposits.end(), kUpperDeposits.begin(), kUpperDeposits.end());

	// One line per source, summed over every scope it reached (docs/reference/tooltip-look.md, the wellbeing hover).
	std::map<const CvInfo*, int64_t> kPerSource;
	for (size_t iDeposit = 0; iDeposit < kDeposits.size(); ++iDeposit)
	{
		const InfoValuation::RefusedDeposit& kDeposit = kDeposits[iDeposit];
		if (kDeposit.bApplied && kDeposit.bPercentSide && kDeposit.iChannel == iChannel && kDeposit.pSource != NULL)
		{
			kPerSource[kDeposit.pSource] += kDeposit.iValue;
		}
	}
	addSourceLines(kPerSource, CITY_BREAKDOWN_PERCENT, iPercentSum);
}
