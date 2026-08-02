//
//	CvConditionQuery -- the static structural reads over a parsed condition tree (see the header for why this
//	is a shared surface rather than a recursion inside each consumer).
//

#include "CvGameCoreDLL.h"
#include "Conditions/CvConditionQuery.h"

namespace
{
	// ⚑ The prefix test is a STARTS-WITH, so every entry must be long enough to be unambiguous on its own.
	// The one real collision in the vocabulary is TRAIT_ against TRAIT_COMPLEX_ ([naming.md]) -- both are the
	// TRAITS axis, so matching the shorter one first is correct rather than merely harmless.
	struct cq_PrefixRow
	{
		const char*   szPrefix;
		EnEdgeBucket  eBucket;
	};

	const cq_PrefixRow CQ_PREFIX_ROWS[] =
	{
		{ "BUILDING_",        EDGEB_BUILDINGS },
		{ "UNIT_",            EDGEB_UNITS },
		{ "BUILD_",           EDGEB_BUILDS },
		{ "TECH_",            EDGEB_TECHS },
		{ "CIVIC_",           EDGEB_CIVICS },
		{ "RELIGION_",        EDGEB_RELIGIONS },
		{ "CORPORATION_",     EDGEB_CORPORATIONS },
		{ "PROJECT_",         EDGEB_PROJECTS },
		{ "PROCESS_",         EDGEB_PROCESSES },
		{ "PROMOTIONLINE_",   EDGEB_PROMOTION_LINES },   // before PROMOTION_ -- it is the longer prefix
		{ "PROMOTION_",       EDGEB_PROMOTIONS },
		{ "HERITAGE_",        EDGEB_HERITAGES },
		{ "SPECIALBUILDING_", EDGEB_SPECIAL_BUILDINGS },
		{ "IMPROVEMENT_",     EDGEB_IMPROVEMENTS },
		{ "BONUS_",           EDGEB_BONUSES },
		{ "ROUTE_",           EDGEB_ROUTES },
		{ "VOTE_",            EDGEB_VOTES },
		{ "HURRY_",           EDGEB_HURRIES },
		{ "TRAIT_",           EDGEB_TRAITS },
		{ "SPECIALIST_",      EDGEB_SPECIALISTS },
		{ 0,                  NO_EDGEB }
	};

	bool cq_startsWith(const std::string& szType, const char* szPrefix)
	{
		const size_t iPrefixLength = strlen(szPrefix);
		return szType.size() >= iPrefixLength && szType.compare(0, iPrefixLength, szPrefix) == 0;
	}

	// The ONE recursion. Every public read is this walk with a different leaf test, so a tree is traversed the
	// same way whichever question is asked. `enabled`/`disabled` are walked with the combinators deliberately:
	// a consumer asking what a tree MENTIONS wants the gated atoms too (the header states the limit that comes
	// with that -- mention is not requirement).
	template <class LeafVisitor>
	void cq_walk(const CvCondition* pCondition, LeafVisitor& visitor)
	{
		if (pCondition == NULL)
		{
			return;
		}
		for (size_t iChild = 0; iChild < pCondition->all.size(); ++iChild)
		{
			cq_walk(pCondition->all[iChild], visitor);
		}
		for (size_t iChild = 0; iChild < pCondition->anyOf.size(); ++iChild)
		{
			cq_walk(pCondition->anyOf[iChild], visitor);
		}
		for (size_t iChild = 0; iChild < pCondition->noneOf.size(); ++iChild)
		{
			cq_walk(pCondition->noneOf[iChild], visitor);
		}
		cq_walk(pCondition->enabled, visitor);
		cq_walk(pCondition->disabled, visitor);
		visitor.leaf(*pCondition);
	}

	// --- the leaf visitors, one per public read ---

	class cq_IdCollector
	{
	public:
		cq_IdCollector(EnEdgeBucket eBucket, std::vector<int>& aIdsOut) : m_eBucket(eBucket), m_pIds(&aIdsOut) {}
		void leaf(const CvCondition& condition)
		{
			if (condition.kind != CASC_COND_PRESENCE || condition.id < 0)
			{
				return;
			}
			if (CvConditionQuery::bucketForType(condition.type) == m_eBucket)
			{
				m_pIds->push_back(condition.id);
			}
		}
	private:
		EnEdgeBucket      m_eBucket;
		std::vector<int>* m_pIds;
	};

	class cq_IdFinder
	{
	public:
		cq_IdFinder(EnEdgeBucket eBucket, int iId) : m_eBucket(eBucket), m_iId(iId), m_bFound(false) {}
		void leaf(const CvCondition& condition)
		{
			if (m_bFound || condition.kind != CASC_COND_PRESENCE || condition.id != m_iId)
			{
				return;
			}
			if (CvConditionQuery::bucketForType(condition.type) == m_eBucket)
			{
				m_bFound = true;
			}
		}
		bool found() const { return m_bFound; }
	private:
		EnEdgeBucket m_eBucket;
		int          m_iId;
		bool         m_bFound;
	};

	class cq_PredicateFinder
	{
	public:
		explicit cq_PredicateFinder(CvCascPredKind ePredicate) : m_ePredicate(ePredicate), m_bFound(false) {}
		void leaf(const CvCondition& condition)
		{
			if (condition.kind == CASC_COND_PREDICATE && condition.predKind == m_ePredicate)
			{
				m_bFound = true;
			}
		}
		bool found() const { return m_bFound; }
	private:
		CvCascPredKind m_ePredicate;
		bool           m_bFound;
	};

	class cq_PredicateIdCollector
	{
	public:
		cq_PredicateIdCollector(CvCascPredKind ePredicate, std::vector<int>& aIdsOut)
			: m_ePredicate(ePredicate), m_pIds(&aIdsOut) {}
		void leaf(const CvCondition& condition)
		{
			if (condition.kind == CASC_COND_PREDICATE && condition.predKind == m_ePredicate && condition.id >= 0)
			{
				m_pIds->push_back(condition.id);
			}
		}
	private:
		CvCascPredKind    m_ePredicate;
		std::vector<int>* m_pIds;
	};
}


EnEdgeBucket CvConditionQuery::bucketForType(const std::string& szType)
{
	for (int iRow = 0; CQ_PREFIX_ROWS[iRow].szPrefix != 0; ++iRow)
	{
		if (cq_startsWith(szType, CQ_PREFIX_ROWS[iRow].szPrefix))
		{
			return CQ_PREFIX_ROWS[iRow].eBucket;
		}
	}
	return NO_EDGEB;
}


void CvConditionQuery::collectIds(const CvCondition* pRoot, EnEdgeBucket eBucket, std::vector<int>& aIdsOut)
{
	if (eBucket == NO_EDGEB)
	{
		return;
	}
	cq_IdCollector collector(eBucket, aIdsOut);
	cq_walk(pRoot, collector);
}


bool CvConditionQuery::namesId(const CvCondition* pRoot, EnEdgeBucket eBucket, int iId)
{
	if (eBucket == NO_EDGEB || iId < 0)
	{
		return false;
	}
	cq_IdFinder finder(eBucket, iId);
	cq_walk(pRoot, finder);
	return finder.found();
}


bool CvConditionQuery::hasPredicate(const CvCondition* pRoot, CvCascPredKind ePredicate)
{
	if (ePredicate == CASC_PRED_UNKNOWN)
	{
		return false;
	}
	cq_PredicateFinder finder(ePredicate);
	cq_walk(pRoot, finder);
	return finder.found();
}


void CvConditionQuery::collectPredicateIds(const CvCondition* pRoot, CvCascPredKind ePredicate,
	std::vector<int>& aIdsOut)
{
	if (ePredicate == CASC_PRED_UNKNOWN)
	{
		return;
	}
	cq_PredicateIdCollector collector(ePredicate, aIdsOut);
	cq_walk(pRoot, collector);
}
