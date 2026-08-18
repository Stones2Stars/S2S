//
//	CvFoldTargetInfo -- the generalized-plot-predicate fold set (see the header). mapFrom reads the authored
//	predicate spelling and FK-resolves its terrain list; idempotent by contract (the list is cleared first).
//

#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson
#include "CvInfos.h"              // umbrella: keeps the unity batch's info-type defs whole (leakage guard)
#include "AI/CvGameAI.h"
#include "CvFoldTargetInfo.h"
#include "CvJsonParse.h"          // jsonIdStr / jsonReadIdList
#include "CvJsonConditionParse.h" // cascadeSpellPredKind -- the ONE predicate speller (docs/architecture/patterns.md §DRY (single implementation))
#include "Defines/CvGlobals.h"    // GC.getFoldTargetInfo / getNumFoldTargetInfos
#include <map>
#include <set>

CvFoldTargetInfo::CvFoldTargetInfo()
{
}

// predicate -> the authored spelling; terrains -> the FK-resolved substrate ids the predicate MEANS.
// Both live at the entity's TOP level: this info IS the mapping, so neither is a sub-block.
void CvFoldTargetInfo::mapFrom(const picojson::value& entity)
{
	CvInfo::mapFrom(entity);   // core reading (type / text keys) + availability
	if (!entity.is<picojson::object>())
	{
		return;
	}
	const picojson::object& entityObj = entity.get<picojson::object>();

	m_szPredicate.clear();
	jsonIdStr(entityObj, "predicate", m_szPredicate);

	// cleared first: mapFrom is idempotent by contract and runs again on the full-pass re-map.
	m_aiTerrains.clear();
	jsonReadIdList(entityObj, "terrains", m_aiTerrains);
}

// ---- FoldTargets: the load-derived predicate -> terrain-set index (see the header) --------------------------

namespace
{
	// predicate kind -> the terrain ids it MEANS. Built ONCE from the loaded fold-target infos, which are
	// write-once-at-load, so there is nothing here that can go stale (docs/architecture/patterns.md §Materialize at mapFrom one plane
	// out: the authored strings resolve once and every later read is a lookup).
	std::map<int, std::set<int> > s_foldSets;
	bool s_bFoldSetsBuilt = false;

	// Resolve an authored spelling to its kind through the ONE public speller rather than a second
	// string->enum map: a private copy would drift the moment a predicate is added.
	CvCascPredKind ft_kindOfSpelling(const std::string& szSpelling)
	{
		if (szSpelling.empty())
		{
			return CASC_PRED_UNKNOWN;
		}
		// UNKNOWN is 0 (the enum's FIRST value), so the walk runs from the first real kind to the last one.
		for (int iKind = (int)CASC_PRED_UNKNOWN + 1; iKind <= (int)CASC_PRED_IS_TAG; ++iKind)
		{
			const char* szKind = cascadeSpellPredKind((CvCascPredKind)iKind);
			if (szKind != NULL && szSpelling == szKind)
			{
				return (CvCascPredKind)iKind;
			}
		}
		return CASC_PRED_UNKNOWN;
	}

	void ft_ensureBuilt()
	{
		if (s_bFoldSetsBuilt)
		{
			return;
		}
		s_bFoldSetsBuilt = true;
		const int iNumFoldTargets = GC.getNumFoldTargetInfos();
		for (int iIndex = 0; iIndex < iNumFoldTargets; ++iIndex)
		{
			const CvFoldTargetInfo& kFoldTarget = GC.getFoldTargetInfo(iIndex);
			const CvCascPredKind ePredicate = ft_kindOfSpelling(kFoldTarget.getPredicate());
			if (ePredicate == CASC_PRED_UNKNOWN)
			{
				continue;   // an unrecognized spelling: reported by the readJson census, never silently folded
			}
			const std::vector<int>& kTerrains = kFoldTarget.getTerrains();
			std::set<int>& kSet = s_foldSets[(int)ePredicate];
			for (size_t iTerrain = 0; iTerrain < kTerrains.size(); ++iTerrain)
			{
				kSet.insert(kTerrains[iTerrain]);
			}
		}
	}
}

bool FoldTargets::defines(CvCascPredKind ePredicate)
{
	ft_ensureBuilt();
	return s_foldSets.find((int)ePredicate) != s_foldSets.end();
}

bool FoldTargets::terrainMatches(CvCascPredKind ePredicate, int iTerrain)
{
	ft_ensureBuilt();
	if (iTerrain < 0)
	{
		return false;
	}
	const std::map<int, std::set<int> >::const_iterator found = s_foldSets.find((int)ePredicate);
	if (found == s_foldSets.end())
	{
		return false;
	}
	return found->second.find(iTerrain) != found->second.end();
}
