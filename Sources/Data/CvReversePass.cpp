//
//	reversePass -- see the header. ONE general pass over the COMPILED info surfaces (edges / requires trees /
//	deposit entries / grants / provides / triggers), replacing the retired per-relationship inversions that read
//	the legacy-mirror getters. Every write here is inside the write-once-at-load window ([DEC-one-reverse-view]).
//

#include "CvGameCoreDLL.h"             // PCH umbrella
#include "Data/CvReversePass.h"
#include "Data/CvReadJson.h"           // rjInfoForType -- the ONE INFOTYPE-prefix -> InfoRepo dispatch
#include "Defines/CvGlobals.h"         // GC.getNum<X>Infos -- the per-kind id spaces
#include "Repos/InfoRepo.h"            // the per-info-type homes
#include "Enabler/CvEnablerKernel.h"   // EnablerKernel::jsonFor -- the gate-axis per-bucket dispatch (single-source)
#include "CvInfo.h"                    // the base surface: edges/requires/deposits/grants/provides/triggers
#include "CvCondition.h"               // the typed requires tree the walks recurse
#include "CvInfoKinds.h"               // infoFamilyYield / infoFamilyKey / infoIsTargetToken
// the per-type repo homes the pass walks / writes (direct imports -- never the CvInfos.h umbrella)
#include "CvBuildingInfo.h"
#include "CvUnitInfo.h"
#include "CvBuildInfo.h"
#include "CvTechInfo.h"                // + cascadeStartNode -- the synthetic TECH_GAME_START root
#include "CvCivicInfo.h"
#include "CvReligionInfo.h"
#include "CvCorporationInfo.h"
#include "CvProjectInfo.h"
#include "CvProcessInfo.h"
#include "CvPromotionInfo.h"
#include "CvPromotionLineInfo.h"
#include "CvHeritageInfo.h"
#include "CvSpecialBuildingInfo.h"
#include "CvImprovementInfo.h"
#include "CvBonusInfo.h"
#include "CvRouteInfo.h"
#include "CvVoteInfo.h"
#include "CvHurryInfo.h"
#include "CvTraitInfo.h"
#include "CvSpecialistInfo.h"
#include "CvTerrainInfo.h"
#include "CvFeatureInfo.h"
#include <cstring>
#include <set>
#include <string>
#include <vector>

namespace
{
	ReversePassCounts s_counts;

	// ==================== the per-kind tables ====================

	// Every source KIND the general RELATED walk iterates: (repo tag, GC count getter, the EnEdgeBucket the kind
	// lands under on a referenced info). Kinds with no EnEdgeBucket in the spec vocabulary (features / terrains /
	// unitcombats / properties / the config kinds) cannot be landed as RELATED entries and are not walked as
	// sources; they still RECEIVE nothing (they compose no CvEdges), so the receiver set is unchanged from the
	// retired bespoke pass. Complex traits are walked beside the TRAIT_ row (same ids, same bucket, own repo).
	// Cross-reference: RJ_REPO_TYPES (CvReadJson.cpp) is the reader's registration axis; this table is the
	// reverse pass's kind->bucket axis -- a new bucket-carrying kind is added to both.
	#define RP_RELATED_SOURCE_KINDS(X) \
		X(CvBuildingInfo,        getNumBuildingInfos,        EDGEB_BUILDINGS) \
		X(CvUnitInfo,            getNumUnitInfos,            EDGEB_UNITS) \
		X(CvBuildInfo,           getNumBuildInfos,           EDGEB_BUILDS) \
		X(CvTechInfo,            getNumTechInfos,            EDGEB_TECHS) \
		X(CvCivicInfo,           getNumCivicInfos,           EDGEB_CIVICS) \
		X(CvReligionInfo,        getNumReligionInfos,        EDGEB_RELIGIONS) \
		X(CvCorporationInfo,     getNumCorporationInfos,     EDGEB_CORPORATIONS) \
		X(CvProjectInfo,         getNumProjectInfos,         EDGEB_PROJECTS) \
		X(CvProcessInfo,         getNumProcessInfos,         EDGEB_PROCESSES) \
		X(CvPromotionInfo,       getNumPromotionInfos,       EDGEB_PROMOTIONS) \
		X(CvPromotionLineInfo,   getNumPromotionLineInfos,   EDGEB_PROMOTION_LINES) \
		X(CvHeritageInfo,        getNumHeritageInfos,        EDGEB_HERITAGES) \
		X(CvSpecialBuildingInfo, getNumSpecialBuildingInfos, EDGEB_SPECIAL_BUILDINGS) \
		X(CvImprovementInfo,     getNumImprovementInfos,     EDGEB_IMPROVEMENTS) \
		X(CvBonusInfo,           getNumBonusInfos,           EDGEB_BONUSES) \
		X(CvRouteInfo,           getNumRouteInfos,           EDGEB_ROUTES) \
		X(CvVoteInfo,            getNumVoteInfos,            EDGEB_VOTES) \
		X(CvHurryInfo,           getNumHurryInfos,           EDGEB_HURRIES) \
		X(CvTraitInfo,           getNumTraitInfos,           EDGEB_TRAITS) \
		X(CvSpecialistInfo,      getNumSpecialistInfos,      EDGEB_SPECIALISTS)

	// The referenced info of an edge BUCKET (the display axis -- every bucket the spec vocabulary carries; the
	// _AND/_OR/_WAIVED variants resolve to their kind's repo). Distinct from EnablerKernel::jsonFor, which is
	// the GATE axis and deliberately serves only the enabler's domain kinds. Reads never create: get() +
	// const_cast (the load window's sanctioned mutation -- addReverseEdge is the load-only writer).
	CvInfo* rp_infoForBucket(EnEdgeBucket eBucket, int iId)
	{
		if (iId < 0)
		{
			return NULL;
		}
		switch (eBucket)
		{
		case EDGEB_BUILDINGS:               return const_cast<CvInfo*>(InfoRepo<CvBuildingInfo>::get().get(iId));
		case EDGEB_UNITS:                   return const_cast<CvInfo*>(InfoRepo<CvUnitInfo>::get().get(iId));
		case EDGEB_BUILDS:                  return const_cast<CvInfo*>(InfoRepo<CvBuildInfo>::get().get(iId));
		case EDGEB_TECHS:                   return const_cast<CvInfo*>(InfoRepo<CvTechInfo>::get().get(iId));
		case EDGEB_CIVICS:                  return const_cast<CvInfo*>(InfoRepo<CvCivicInfo>::get().get(iId));
		case EDGEB_RELIGIONS:               return const_cast<CvInfo*>(InfoRepo<CvReligionInfo>::get().get(iId));
		case EDGEB_CORPORATIONS:            return const_cast<CvInfo*>(InfoRepo<CvCorporationInfo>::get().get(iId));
		case EDGEB_PROJECTS:                return const_cast<CvInfo*>(InfoRepo<CvProjectInfo>::get().get(iId));
		case EDGEB_PROCESSES:               return const_cast<CvInfo*>(InfoRepo<CvProcessInfo>::get().get(iId));
		case EDGEB_PROMOTIONS:              return const_cast<CvInfo*>(InfoRepo<CvPromotionInfo>::get().get(iId));
		case EDGEB_PROMOTION_LINES:         return const_cast<CvInfo*>(InfoRepo<CvPromotionLineInfo>::get().get(iId));
		case EDGEB_HERITAGES:               return const_cast<CvInfo*>(InfoRepo<CvHeritageInfo>::get().get(iId));
		case EDGEB_SPECIAL_BUILDINGS:       return const_cast<CvInfo*>(InfoRepo<CvSpecialBuildingInfo>::get().get(iId));
		case EDGEB_SPECIAL_BUILDINGS_WAIVED:return const_cast<CvInfo*>(InfoRepo<CvSpecialBuildingInfo>::get().get(iId));
		case EDGEB_IMPROVEMENTS:            return const_cast<CvInfo*>(InfoRepo<CvImprovementInfo>::get().get(iId));
		case EDGEB_BONUSES:                 return const_cast<CvInfo*>(InfoRepo<CvBonusInfo>::get().get(iId));
		case EDGEB_ROUTES:                  return const_cast<CvInfo*>(InfoRepo<CvRouteInfo>::get().get(iId));
		case EDGEB_ROUTES_AND:              return const_cast<CvInfo*>(InfoRepo<CvRouteInfo>::get().get(iId));
		case EDGEB_VOTES:                   return const_cast<CvInfo*>(InfoRepo<CvVoteInfo>::get().get(iId));
		case EDGEB_HURRIES:                 return const_cast<CvInfo*>(InfoRepo<CvHurryInfo>::get().get(iId));
		case EDGEB_TRAITS:                  return const_cast<CvInfo*>(InfoRepo<CvTraitInfo>::get().get(iId));
		case EDGEB_TRAITS_AND:              return const_cast<CvInfo*>(InfoRepo<CvTraitInfo>::get().get(iId));
		case EDGEB_TRAITS_OR:               return const_cast<CvInfo*>(InfoRepo<CvTraitInfo>::get().get(iId));
		case EDGEB_SPECIALISTS:             return const_cast<CvInfo*>(InfoRepo<CvSpecialistInfo>::get().get(iId));
		default:                            return NULL;
		}
	}

	// ==================== sub-pass (2): EDGEF_RELATED -- the general display inversion ====================

	// Land one RELATED reference: the referencing info (eSourceBucket, iSourceId) onto the referenced info.
	void rp_landRelated(CvInfo* pReferencedInfo, EnEdgeBucket eSourceBucket, int iSourceId)
	{
		if (pReferencedInfo == NULL)
		{
			return;
		}
		if (eSourceBucket == NO_EDGEB)
		{
			return;
		}
		pReferencedInfo->addReverseEdge(EDGEF_RELATED, eSourceBucket, iSourceId);
		++s_counts.relatedAdds;
	}

	// Recurse one condition tree: every FK-resolved PRESENCE atom + parameterized PREDICATE lands the owning
	// info on the referenced info's RELATED bucket (the broad rjInfoForType routing -- any repo-homed kind).
	void rp_relatedConditionWalk(const CvCondition* pCondition, EnEdgeBucket eSourceBucket, int iSourceId)
	{
		if (pCondition == NULL)
		{
			return;
		}
		for (size_t iChild = 0; iChild < pCondition->all.size(); ++iChild)
		{
			rp_relatedConditionWalk(pCondition->all[iChild], eSourceBucket, iSourceId);
		}
		for (size_t iChild = 0; iChild < pCondition->anyOf.size(); ++iChild)
		{
			rp_relatedConditionWalk(pCondition->anyOf[iChild], eSourceBucket, iSourceId);
		}
		for (size_t iChild = 0; iChild < pCondition->noneOf.size(); ++iChild)
		{
			rp_relatedConditionWalk(pCondition->noneOf[iChild], eSourceBucket, iSourceId);
		}
		rp_relatedConditionWalk(pCondition->enabled, eSourceBucket, iSourceId);
		rp_relatedConditionWalk(pCondition->disabled, eSourceBucket, iSourceId);
		if (pCondition->id < 0)
		{
			return;
		}
		if (pCondition->kind == CASC_COND_PRESENCE)
		{
			rp_landRelated(rjInfoForType(pCondition->type, pCondition->id), eSourceBucket, iSourceId);
		}
		else if (pCondition->kind == CASC_COND_PREDICATE && !pCondition->param.empty())
		{
			rp_landRelated(rjInfoForType(pCondition->param, pCondition->id), eSourceBucket, iSourceId);
		}
	}

	// One grants payload's FK lists + their per-entry conditions (also reached nested, as a trigger action's grant).
	void rp_relatedFromGrants(const CvGrants* pGrants, EnEdgeBucket eSourceBucket, int iSourceId)
	{
		if (pGrants == NULL)
		{
			return;
		}
		const std::map<int, std::vector<int> >& grantLists = pGrants->lists();
		for (std::map<int, std::vector<int> >::const_iterator it = grantLists.begin(); it != grantLists.end(); ++it)
		{
			// Load-window bucket classification: the grants key table is local to CvGrants, so the bucket is
			// recovered by matching the minted key against each edge-bucket spelling (strings are sanctioned
			// here -- this pass IS the load window; a non-bucket list key classifies NO_EDGEB and only its
			// per-entry conditions walk).
			EnEdgeBucket eGrantBucket = NO_EDGEB;
			for (int iBucket = 0; iBucket < NUM_EDGEB; ++iBucket)
			{
				if (CvGrants::findKey(CvEdges::bucketName((EnEdgeBucket)iBucket)) == it->first)
				{
					eGrantBucket = (EnEdgeBucket)iBucket;
					break;
				}
			}
			for (size_t iEntry = 0; iEntry < it->second.size(); ++iEntry)
			{
				if (eGrantBucket != NO_EDGEB)
				{
					rp_landRelated(rp_infoForBucket(eGrantBucket, it->second[iEntry]), eSourceBucket, iSourceId);
				}
				rp_relatedConditionWalk(pGrants->listCond(it->first, iEntry), eSourceBucket, iSourceId);
			}
		}
	}

	// One compiled deposit list (the entity's §6 families, or its whenObsolete tree): the FK-resolved target key
	// + the entry's condition trees + the per-scaler FKs.
	void rp_relatedFromModifiers(const CvModifiers* pModifiers, EnEdgeBucket eSourceBucket, int iSourceId)
	{
		if (pModifiers == NULL)
		{
			return;
		}
		const std::vector<CvModEntry*>& compiledEntries = pModifiers->entries();
		for (size_t iEntry = 0; iEntry < compiledEntries.size(); ++iEntry)
		{
			const CvModEntry* pEntry = compiledEntries[iEntry];
			if (pEntry == NULL)
			{
				continue;
			}
			if (pEntry->targetFk >= 0)
			{
				// the FK key segment is the one underscored non-target-token segment of the authored address
				// (member spellings are camelCase, never underscored -- the CvModifiers decode's own rule)
				for (int iSeg = 0; iSeg < pEntry->nSeg && iSeg < CvModEntry::MOD_ENTRY_SEGS; ++iSeg)
				{
					const char* szSegment = modSegmentSpell(pEntry->seg[iSeg]);
					if (strchr(szSegment, '_') == NULL)
					{
						continue;
					}
					if (infoIsTargetToken(szSegment))
					{
						continue;
					}
					rp_landRelated(rjInfoForType(szSegment, pEntry->targetFk), eSourceBucket, iSourceId);
					break;
				}
			}
			rp_relatedConditionWalk(pEntry->enabled, eSourceBucket, iSourceId);
			rp_relatedConditionWalk(pEntry->disabled, eSourceBucket, iSourceId);
			rp_relatedConditionWalk(pEntry->unitQual, eSourceBucket, iSourceId);
			rp_relatedConditionWalk(pEntry->religionQual, eSourceBucket, iSourceId);
			if (pEntry->perTypeId >= 0 && !pEntry->perType.empty())
			{
				rp_landRelated(rjInfoForType(pEntry->perType, pEntry->perTypeId), eSourceBucket, iSourceId);
			}
			for (size_t iPer = 0; iPer < pEntry->perAnyOf.size() && iPer < pEntry->perAnyOfTypes.size(); ++iPer)
			{
				if (pEntry->perAnyOf[iPer] >= 0)
				{
					rp_landRelated(rjInfoForType(pEntry->perAnyOfTypes[iPer], pEntry->perAnyOf[iPer]), eSourceBucket, iSourceId);
				}
			}
		}
	}

	// ONE info's complete compiled surface -> RELATED. Edge references land BOTH directions (the referencing
	// info under the referenced info's kind bucket, AND the referenced id under the edge's own bucket on the
	// referencing info) -- the store-inversion means either side may carry the authored edge, and the symmetric
	// landing is what keeps every legacy display read servable from the info it asks (the bonus<-tech reveal
	// class). Requires/deposits/grants land ONE direction (owner -> referenced): their forward side already
	// lives on the owning info's own sections.
	void rp_relatedFromInfo(CvInfo* pSourceInfo, EnEdgeBucket eSourceBucket, int iSourceId)
	{
		if (pSourceInfo == NULL)
		{
			return;
		}
		const CvEdges* pEdges = pSourceInfo->getEdges();
		if (pEdges != NULL)
		{
			// AUTHORED families only (ENABLES..OBSOLETED_BY) -- the derived RELATED/REQUIRED_BY families are
			// this pass's own output, never its input.
			for (int iFamily = 0; iFamily < (int)EDGEF_RELATED; ++iFamily)
			{
				for (int iBucket = 0; iBucket < NUM_EDGEB; ++iBucket)
				{
					const std::vector<int>* pReferencedIds = pEdges->find((EnEdgeFamily)iFamily, (EnEdgeBucket)iBucket);
					if (pReferencedIds == NULL)
					{
						continue;
					}
					for (size_t iRef = 0; iRef < pReferencedIds->size(); ++iRef)
					{
						const int iReferencedId = (*pReferencedIds)[iRef];
						rp_landRelated(rp_infoForBucket((EnEdgeBucket)iBucket, iReferencedId), eSourceBucket, iSourceId);
						pSourceInfo->addReverseEdge(EDGEF_RELATED, (EnEdgeBucket)iBucket, iReferencedId);
						++s_counts.relatedAdds;
					}
				}
			}
		}
		rp_relatedConditionWalk(pSourceInfo->requiresBuild(), eSourceBucket, iSourceId);
		rp_relatedConditionWalk(pSourceInfo->requiresOperate(), eSourceBucket, iSourceId);
		const std::vector<int>& dormantSuccessors = pSourceInfo->dormantTriggers();
		for (size_t iDormant = 0; iDormant < dormantSuccessors.size(); ++iDormant)
		{
			// dormant triggers reference same-kind successors (building ReplacementBuildings; unit upgrades)
			rp_landRelated(rp_infoForBucket(eSourceBucket, dormantSuccessors[iDormant]), eSourceBucket, iSourceId);
		}
		rp_relatedFromModifiers(pSourceInfo->getModifiers(), eSourceBucket, iSourceId);
		rp_relatedFromModifiers(pSourceInfo->getWhenObsolete(), eSourceBucket, iSourceId);
		rp_relatedFromGrants(pSourceInfo->getGrants(), eSourceBucket, iSourceId);
		const CvProvides* pProvides = pSourceInfo->getProvides();
		if (pProvides != NULL)
		{
			for (size_t iBonus = 0; iBonus < pProvides->bonuses.size(); ++iBonus)
			{
				rp_landRelated(rp_infoForBucket(EDGEB_BONUSES, pProvides->bonuses[iBonus]), eSourceBucket, iSourceId);
			}
		}
		const CvTriggers* pTriggers = pSourceInfo->getTriggers();
		if (pTriggers != NULL)
		{
			const std::vector<CvTriggerEntry*>& triggerEntries = pTriggers->entries();
			for (size_t iTrigger = 0; iTrigger < triggerEntries.size(); ++iTrigger)
			{
				const CvTriggerEntry* pTrigger = triggerEntries[iTrigger];
				if (pTrigger == NULL)
				{
					continue;
				}
				rp_relatedConditionWalk(pTrigger->condition, eSourceBucket, iSourceId);
				for (size_t iPromotion = 0; iPromotion < pTrigger->promotePromotions.size(); ++iPromotion)
				{
					rp_landRelated(rp_infoForBucket(EDGEB_PROMOTIONS, pTrigger->promotePromotions[iPromotion]), eSourceBucket, iSourceId);
				}
				rp_relatedFromGrants(pTrigger->grant, eSourceBucket, iSourceId);
				// healUnitCombatId / propertyId reference kinds outside the bucket vocabulary that also compose
				// no CvEdges -- nothing to land, by construction.
			}
		}
	}

	// One source kind's whole repo. Reads never create: get() + const_cast (an OWNED repo -- heritage / build /
	// complex traits -- must not sprout empty pocos for ids its phase has not mapped).
	template <class RepoTag>
	void rp_relatedWalkKind(int iNumInfos, EnEdgeBucket eSourceBucket)
	{
		for (int iInfo = 0; iInfo < iNumInfos; ++iInfo)
		{
			CvInfo* pSourceInfo = const_cast<CvInfo*>(InfoRepo<RepoTag>::get().get(iInfo));
			rp_relatedFromInfo(pSourceInfo, eSourceBucket, iInfo);
		}
	}

	// The BUILD kind's one typed-section contributor: the per-terrain / per-feature tech gates live in the
	// build's `produces` section (typed subclass members, not the shared compiled surface), so the general walk
	// cannot see them -- the one place a per-type read remains in the RELATED build.
	void rp_relatedFromBuildProduces()
	{
		const int iNumBuilds = GC.getNumBuildInfos();
		const int iNumFeatures = GC.getNumFeatureInfos();
		for (int iBuild = 0; iBuild < iNumBuilds; ++iBuild)
		{
			const CvBuildInfo* pBuild = static_cast<const CvBuildInfo*>(InfoRepo<CvBuildInfo>::get().get(iBuild));
			if (pBuild == NULL)
			{
				continue;
			}
			const std::vector<TerrainStructs>& terraformRows = pBuild->getTerrainStructs();
			for (size_t iRow = 0; iRow < terraformRows.size(); ++iRow)
			{
				rp_landRelated(rp_infoForBucket(EDGEB_TECHS, (int)terraformRows[iRow].ePrereqTech), EDGEB_BUILDS, iBuild);
			}
			for (int iFeature = 0; iFeature < iNumFeatures; ++iFeature)
			{
				rp_landRelated(rp_infoForBucket(EDGEB_TECHS, (int)pBuild->getFeatureTech((FeatureTypes)iFeature)), EDGEB_BUILDS, iBuild);
			}
		}
	}

	void rp_buildRelated()
	{
		#define X(REPO_TAG, COUNT_GETTER, SOURCE_BUCKET) rp_relatedWalkKind<REPO_TAG>(GC.COUNT_GETTER(), SOURCE_BUCKET);
		RP_RELATED_SOURCE_KINDS(X)
		#undef X
		// the complex trait set rides beside the TRAIT_ row: same engine ids, same bucket, its own repo (the
		// RELATED lists dedup, so a shared reference lands once)
		rp_relatedWalkKind<CvComplexTraitTag>(GC.getNumTraitInfos(), EDGEB_TRAITS);
		rp_relatedFromBuildProduces();
	}

	// ==================== sub-pass (3): EDGEF_REQUIRED_BY -- the enabler's re-gate index ====================
	// The exact tree-walk semantics of the retired rvRequiresWalk, over the same dependent-kind set
	// (enabler.md §7.1 step 2): every HAVE-axis atom a dependent's requires references gains that dependent
	// under the dependent's kind bucket ON THE REFERENCED INFO -- the gate stage re-gates exactly these
	// dependents on the atom's HAVE-event, never a database sweep.

	// The referenced HAVE-axis info by its INFOTYPE prefix (naming.md routes by prefix). NULL = not a HAVE-axis
	// re-gate target: engine tokens (TURN/POPULATION/ERA), the plot substrate (terrain/feature/improvement --
	// the dynamic plot axis with its own event routing), and PROPERTY_ bands (the property engine's axis).
	CvInfo* rp_requiredByRefInfo(const std::string& szType, int iId)
	{
		// the synthetic root's cascade data lives OFF the InfoRepo (cascadeStartNode) -- route it there, never
		// the GC poco (the split-brain the /state/info read exposed)
		if (szType == "TECH_GAME_START")
		{
			return &cascadeStartNode();
		}
		if (!szType.compare(0, 5, "TECH_"))
		{
			return InfoRepo<CvTechInfo>::get().editPtr(iId);
		}
		if (!szType.compare(0, 9, "BUILDING_"))
		{
			return InfoRepo<CvBuildingInfo>::get().editPtr(iId);
		}
		if (!szType.compare(0, 6, "BONUS_"))
		{
			return InfoRepo<CvBonusInfo>::get().editPtr(iId);
		}
		if (!szType.compare(0, 6, "CIVIC_"))
		{
			return InfoRepo<CvCivicInfo>::get().editPtr(iId);
		}
		if (!szType.compare(0, 5, "UNIT_"))
		{
			return InfoRepo<CvUnitInfo>::get().editPtr(iId);
		}
		if (!szType.compare(0, 9, "RELIGION_"))
		{
			return InfoRepo<CvReligionInfo>::get().editPtr(iId);
		}
		if (!szType.compare(0, 12, "CORPORATION_"))
		{
			return InfoRepo<CvCorporationInfo>::get().editPtr(iId);
		}
		if (!szType.compare(0, 9, "HERITAGE_"))
		{
			return InfoRepo<CvHeritageInfo>::get().editPtr(iId);
		}
		if (!szType.compare(0, 8, "PROJECT_"))
		{
			return InfoRepo<CvProjectInfo>::get().editPtr(iId);
		}
		return NULL;
	}

	// Recurse one requires tree: every FK-resolved PRESENCE atom + parameterized PREDICATE lands the dependent
	// on the referenced info's REQUIRED_BY bucket.
	void rp_requiredByWalk(const CvCondition* pCondition, EnEdgeBucket eDependentKind, int iDependentId)
	{
		if (pCondition == NULL)
		{
			return;
		}
		for (size_t iChild = 0; iChild < pCondition->all.size(); ++iChild)
		{
			rp_requiredByWalk(pCondition->all[iChild], eDependentKind, iDependentId);
		}
		for (size_t iChild = 0; iChild < pCondition->anyOf.size(); ++iChild)
		{
			rp_requiredByWalk(pCondition->anyOf[iChild], eDependentKind, iDependentId);
		}
		for (size_t iChild = 0; iChild < pCondition->noneOf.size(); ++iChild)
		{
			rp_requiredByWalk(pCondition->noneOf[iChild], eDependentKind, iDependentId);
		}
		rp_requiredByWalk(pCondition->enabled, eDependentKind, iDependentId);
		rp_requiredByWalk(pCondition->disabled, eDependentKind, iDependentId);
		if (pCondition->id < 0)
		{
			return;
		}
		CvInfo* pReferencedInfo = NULL;
		if (pCondition->kind == CASC_COND_PRESENCE)
		{
			pReferencedInfo = rp_requiredByRefInfo(pCondition->type, pCondition->id);
		}
		else if (pCondition->kind == CASC_COND_PREDICATE && !pCondition->param.empty())
		{
			pReferencedInfo = rp_requiredByRefInfo(pCondition->param, pCondition->id);
		}
		if (pReferencedInfo != NULL)
		{
			pReferencedInfo->addReverseEdge(EDGEF_REQUIRED_BY, eDependentKind, iDependentId);
			++s_counts.requiredByAdds;
		}
	}

	int rp_gateKindCount(EnEdgeBucket eBucket)
	{
		switch (eBucket)
		{
		case EDGEB_BUILDINGS:
			return GC.getNumBuildingInfos();
		case EDGEB_UNITS:
			return GC.getNumUnitInfos();
		case EDGEB_TECHS:
			return GC.getNumTechInfos();
		case EDGEB_CIVICS:
			return GC.getNumCivicInfos();
		case EDGEB_PROJECTS:
			return GC.getNumProjectInfos();
		case EDGEB_PROCESSES:
			return GC.getNumProcessInfos();
		case EDGEB_PROMOTIONS:
			return GC.getNumPromotionInfos();
		case EDGEB_BUILDS:
			return GC.getNumBuildInfos();
		default:
			return 0;
		}
	}

	void rp_buildRequiredBy()
	{
		// every dependent kind's requires.build + requires.operate trees + its dormant triggers, inverted onto
		// the referenced infos
		static const EnEdgeBucket DEPENDENT_KINDS[] =
		{
			EDGEB_BUILDINGS, EDGEB_UNITS, EDGEB_TECHS, EDGEB_CIVICS,
			EDGEB_PROJECTS, EDGEB_PROCESSES, EDGEB_PROMOTIONS, EDGEB_BUILDS, NO_EDGEB
		};
		for (int iKind = 0; DEPENDENT_KINDS[iKind] != NO_EDGEB; ++iKind)
		{
			const EnEdgeBucket eKind = DEPENDENT_KINDS[iKind];
			const int iNumInfos = rp_gateKindCount(eKind);
			for (int iInfo = 0; iInfo < iNumInfos; ++iInfo)
			{
				const CvInfo* pDependent = EnablerKernel::jsonFor(eKind, iInfo);
				if (pDependent == NULL)
				{
					continue;
				}
				rp_requiredByWalk(pDependent->requiresBuild(), eKind, iInfo);
				rp_requiredByWalk(pDependent->requiresOperate(), eKind, iInfo);
				// dormant triggers reference same-kind successors (building ReplacementBuildings; unit upgrades)
				const std::vector<int>& dormantSuccessors = pDependent->dormantTriggers();
				for (size_t iDormant = 0; iDormant < dormantSuccessors.size(); ++iDormant)
				{
					CvInfo* pSuccessor = const_cast<CvInfo*>(EnablerKernel::jsonFor(eKind, dormantSuccessors[iDormant]));
					if (pSuccessor != NULL)
					{
						pSuccessor->addReverseEdge(EDGEF_REQUIRED_BY, eKind, iInfo);
						++s_counts.requiredByAdds;
					}
				}
			}
		}
		// improvements + heritages carry requires too (no jsonFor bucket dispatch -- repo-direct)
		for (int iImprovement = 0; iImprovement < GC.getNumImprovementInfos(); ++iImprovement)
		{
			const CvInfo* pDependent = InfoRepo<CvImprovementInfo>::get().get(iImprovement);
			if (pDependent == NULL)
			{
				continue;
			}
			rp_requiredByWalk(pDependent->requiresBuild(), EDGEB_IMPROVEMENTS, iImprovement);
			rp_requiredByWalk(pDependent->requiresOperate(), EDGEB_IMPROVEMENTS, iImprovement);
		}
		for (int iHeritage = 0; iHeritage < GC.getNumHeritageInfos(); ++iHeritage)
		{
			const CvInfo* pDependent = InfoRepo<CvHeritageInfo>::get().get(iHeritage);
			if (pDependent == NULL)
			{
				continue;
			}
			rp_requiredByWalk(pDependent->requiresBuild(), EDGEB_HERITAGES, iHeritage);
			rp_requiredByWalk(pDependent->requiresOperate(), EDGEB_HERITAGES, iHeritage);
		}
	}

	// ==================== sub-pass (4): the own-output reverse LANDING (modifier.md §4) ====================
	// The (family × targetKind) classification, derived from modifier.md §4 over the compiled data's keyed
	// tuples (the full census -- every pair present in Assets/Data):
	//   LANDED (own-output -- the target's own produced output, the source a presence condition):
	//     - building/civic/tech × {food,production,commerce} × {improvements,terrains,features,routes} FLAT
	//       (tile output -- lands PLOT-scope, presence at the source kind's HAVE scope);
	//     - building/civic/tech × {gold,culture,research,espionage,commerce,food,production,happiness,health}
	//       × buildings keyed (info-rebuild.md ruling 19: the wonder/civic/tech -> building-TYPE boosts are the
	//       TARGET building's own output -- land CITY-scope per §2a/§2b, same family/value/unit, kind 0,
	//       presence at the AUTHORED deposit's scope axis, an authored condition composed in, never dropped).
	//   STAYS SOURCE-SIDE (governing-deliverer, or the §4 carve-outs):
	//     - buildRate × every keyed target (the §4 named governing-deliverer exemplar -- incl. its buildings
	//       keyed targets: a build-cost discount is delivered, not the target's output);
	//     - EVERY TRAIT-sourced keyed deposit (the §4 per-set carve-out: simple/complex values differ per set
	//       and the target is one shared file -- the cascade reads the ACTIVE set source-side);
	//     - route × improvements yields (the §4 governing-deliverer exemplar -- read off the ROUTE's compiled
	//       keyed entries; the improvement-side consumers rewire onto them at the stage-4 cut);
	//     - source × units/domains/unitCombats/specialists/techs/builds keyed values, + non-output-channel
	//       families keyed to buildings (delivered effects: XP, research-rate, work-rate, great-people,
	//       diplomacy -- the deliverer brings them);
	//     - civic × features happiness (modifier.md §2b reads the civic-side keyed member in the wellbeing
	//       terms -- the legacy one-term feature+improvement bundling; left source-side, reported).
	//   A plot-substrate landing-shaped entry that is conditioned / non-flat / member-kinded is left
	//   source-side and COUNTED (ownOutputSkipped) -- never guessed at (the census carries zero today). A
	//   buildings-keyed entry composes conditions instead; only a member-kinded one (it cannot land as the
	//   kind-0 city output slot) is left source-side and COUNTED (census: zero today).

	// The plot-substrate landing target of a keyed deposit: the entry's target-token segment names the kind.
	CvInfo* rp_landingTarget(const CvModEntry* pEntry, int iImprovementsToken, int iTerrainsToken, int iFeaturesToken, int iRoutesToken)
	{
		if (pEntry->targetSeg < 0)
		{
			return NULL;
		}
		if (pEntry->targetSeg == iImprovementsToken)
		{
			return const_cast<CvInfo*>(InfoRepo<CvImprovementInfo>::get().get(pEntry->targetFk));
		}
		if (pEntry->targetSeg == iTerrainsToken)
		{
			return const_cast<CvInfo*>(InfoRepo<CvTerrainInfo>::get().get(pEntry->targetFk));
		}
		if (pEntry->targetSeg == iFeaturesToken)
		{
			return const_cast<CvInfo*>(InfoRepo<CvFeatureInfo>::get().get(pEntry->targetFk));
		}
		if (pEntry->targetSeg == iRoutesToken)
		{
			return const_cast<CvInfo*>(InfoRepo<CvRouteInfo>::get().get(pEntry->targetFk));
		}
		return NULL;
	}

	// The nine output-channel families of the buildings-keyed landing (info-rebuild.md ruling 19): the yield
	// channels (food/production/commerce), the commerce channels (gold/research/culture/espionage), and the
	// wellbeing channels (happiness/health). Everything else keyed to a building is a delivered effect and
	// stays source-side by classification.
	bool rp_isOutputChannelFamily(ModifierFamily eFamily)
	{
		if (infoFamilyYield(eFamily) >= 0)
		{
			return true;
		}
		if (infoFamilyCommerce(eFamily) >= 0)
		{
			return true;
		}
		return eFamily == MODFAM_HAPPINESS || eFamily == MODFAM_HEALTH;
	}

	// Deep clone of a compiled condition tree. Needed because a landed entry OWNS its trees while the authored
	// source-side entry keeps its own (CvCondition is noncopyable by design -- ownership, not immutability).
	CvCondition* rp_cloneCondition(const CvCondition* pCondition)
	{
		if (pCondition == NULL)
		{
			return NULL;
		}
		CvCondition* pClone = new CvCondition();
		pClone->kind = pCondition->kind;
		for (size_t iChild = 0; iChild < pCondition->all.size(); ++iChild)
		{
			pClone->all.push_back(rp_cloneCondition(pCondition->all[iChild]));
		}
		for (size_t iChild = 0; iChild < pCondition->anyOf.size(); ++iChild)
		{
			pClone->anyOf.push_back(rp_cloneCondition(pCondition->anyOf[iChild]));
		}
		for (size_t iChild = 0; iChild < pCondition->noneOf.size(); ++iChild)
		{
			pClone->noneOf.push_back(rp_cloneCondition(pCondition->noneOf[iChild]));
		}
		pClone->enabled = rp_cloneCondition(pCondition->enabled);
		pClone->disabled = rp_cloneCondition(pCondition->disabled);
		pClone->type = pCondition->type;
		pClone->scope = pCondition->scope;
		pClone->min = pCondition->min;
		pClone->max = pCondition->max;
		pClone->connection = pCondition->connection;
		pClone->vicinity = pCondition->vicinity;
		pClone->predKind = pCondition->predKind;
		pClone->param = pCondition->param;
		pClone->id = pCondition->id;
		return pClone;
	}

	// Land ONE buildings-keyed output-channel deposit on its TARGET building (info-rebuild.md ruling 19).
	// The landed entry is the target building's own CITY-scope output (§2a building output / §2b wellbeing):
	// same family/value/unit, kind 0, gated on the SOURCE's presence at the AUTHORED deposit's scope --
	// derived from the compiled entry's scope axis (an empire-authored keyed deposit gates on empire-scope
	// presence; city-authored on city; team-authored on team), never hardcoded per source kind. An authored
	// condition/per-scaler is PRESERVED by composing: the presence atom ANDs with a deep clone of the authored
	// `enabled` (a GROUP-all node); `disabled` / the `unit:` qualifier clone across whole; a `per` copies its
	// fields, an absent per-scope pinned to the AUTHORED deposit scope so the count does not silently re-scope
	// to the landed city scope.
	void rp_landOnTargetBuilding(const CvInfo* pSourceInfo, int iSourceId, const CvModEntry* pEntry)
	{
		CvInfo* pTargetInfo = const_cast<CvInfo*>(static_cast<const CvInfo*>(InfoRepo<CvBuildingInfo>::get().get(pEntry->targetFk)));
		if (pTargetInfo == NULL)
		{
			return;   // unresolved target FK -- the reader's fail-loud coverage summary owns the report
		}
		CvModEntry* pLanded = new CvModEntry();
		pLanded->family = pEntry->family;
		pLanded->scope = CASC_SCOPE_CITY;
		pLanded->kind = 0;
		pLanded->unit = pEntry->unit;
		pLanded->value = pEntry->value;
		pLanded->aiOnly = pEntry->aiOnly;   // the §3.9 audience flag rides the landing whole
		pLanded->seg[0] = modSegmentIntern(infoFamilyKey(pEntry->family));
		pLanded->seg[1] = modSegmentIntern("city");
		pLanded->nSeg = 2;
		CvCondition* pPresence = new CvCondition();
		pPresence->kind = CASC_COND_PRESENCE;
		pPresence->type = pSourceInfo->getType();
		pPresence->scope = pEntry->scope;   // the AUTHORED deposit's scope axis, never the source kind's
		pPresence->min = 1;
		pPresence->id = iSourceId;
		if (pEntry->enabled != NULL)
		{
			CvCondition* pComposed = new CvCondition();
			pComposed->kind = CASC_COND_GROUP;
			pComposed->all.push_back(pPresence);
			pComposed->all.push_back(rp_cloneCondition(pEntry->enabled));
			pLanded->enabled = pComposed;
		}
		else
		{
			pLanded->enabled = pPresence;
		}
		pLanded->disabled = rp_cloneCondition(pEntry->disabled);
		pLanded->unitQual = rp_cloneCondition(pEntry->unitQual);
		pLanded->religionQual = rp_cloneCondition(pEntry->religionQual);
		// the ranked-selection carry (ruling 25) rides the landing whole -- parse-carried, evaluation parked
		pLanded->hasRankQual = pEntry->hasRankQual;
		pLanded->rankMax = pEntry->rankMax;
		pLanded->rankMaxToken = pEntry->rankMaxToken;
		pLanded->orderedBySeg = pEntry->orderedBySeg;
		pLanded->orderedDescending = pEntry->orderedDescending;
		if (pEntry->hasPer)
		{
			pLanded->hasPer = true;
			pLanded->perType = pEntry->perType;
			pLanded->perAnyOfTypes = pEntry->perAnyOfTypes;
			pLanded->perTypeId = pEntry->perTypeId;
			pLanded->perEach = pEntry->perEach;
			pLanded->perScope = (pEntry->perScope >= 0) ? pEntry->perScope : (int)pEntry->scope;
			pLanded->perAnyOf = pEntry->perAnyOf;
			// per.above (ruling 26): the source-resolved base + token travel with the per whole
			pLanded->hasAbove = pEntry->hasAbove;
			pLanded->perAbove = pEntry->perAbove;
			pLanded->perAboveToken = pEntry->perAboveToken;
		}
		pTargetInfo->landOwnOutputEntry(pLanded);
		++s_counts.ownOutputLanded;
	}

	// Land ONE source kind's own-output keyed deposits on their targets -- two landing classes:
	//   (a) plot-substrate keyed (improvements/terrains/features/routes): the landed entry is the target's own
	//       plot-scope flat ("+X while the source is present"): same family/value, scope PLOT (the plot package
	//       is where every component-specific buff resolves -- modifier.md §2/§2a), the source's presence as
	//       the prebuilt `enabled` condition at the source kind's HAVE scope (building -> city, civic ->
	//       empire, tech -> team; the condition parser's own implied-scope rule);
	//   (b) buildings keyed (info-rebuild.md ruling 19): the nine output channels land on the TARGET building
	//       at CITY scope, presence at the AUTHORED deposit's scope axis (rp_landOnTargetBuilding).
	template <class SourceRepoTag>
	void rp_landOwnOutputKind(int iNumInfos, CvCascScope ePresenceScope)
	{
		const int iImprovementsToken = modSegmentLookup("improvements");
		const int iTerrainsToken = modSegmentLookup("terrains");
		const int iFeaturesToken = modSegmentLookup("features");
		const int iRoutesToken = modSegmentLookup("routes");
		const int iBuildingsToken = modSegmentLookup("buildings");
		for (int iSource = 0; iSource < iNumInfos; ++iSource)
		{
			const CvInfo* pSourceInfo = InfoRepo<SourceRepoTag>::get().get(iSource);
			if (pSourceInfo == NULL)
			{
				continue;
			}
			const CvModifiers* pModifiers = pSourceInfo->getModifiers();
			if (pModifiers == NULL)
			{
				continue;
			}
			// index loop over the live vector: a self-keyed landing appends to this same entry list, which is
			// safe (pointer elements; the appended landed entries fail the targetFk gate on their own turn)
			const std::vector<CvModEntry*>& compiledEntries = pModifiers->entries();
			for (size_t iEntry = 0; iEntry < compiledEntries.size(); ++iEntry)
			{
				const CvModEntry* pEntry = compiledEntries[iEntry];
				if (pEntry == NULL)
				{
					continue;
				}
				if (pEntry->targetFk < 0)
				{
					continue;
				}
				// landing class (b): the buildings-keyed output-channel flip. Non-output families keyed to
				// buildings (buildRate discounts, ...) stay source-side by classification -- silent, not a skip.
				if (pEntry->targetSeg == iBuildingsToken)
				{
					if (!rp_isOutputChannelFamily(pEntry->family))
					{
						continue;
					}
					if (pEntry->kind != 0)
					{
						// a member-kinded buildings-keyed slot cannot land as the kind-0 city output entry --
						// left source-side and surfaced, never guessed at (the census carries zero today)
						++s_counts.ownOutputSkipped;
						continue;
					}
					rp_landOnTargetBuilding(pSourceInfo, iSource, pEntry);
					continue;
				}
				// landing class (a): the plot-substrate tile-output landing.
				if (infoFamilyYield(pEntry->family) < 0)
				{
					continue;   // own tile output is a YIELD channel; every other keyed family is delivered
				}
				CvInfo* pTargetInfo = rp_landingTarget(pEntry, iImprovementsToken, iTerrainsToken, iFeaturesToken, iRoutesToken);
				if (pTargetInfo == NULL)
				{
					continue;   // not a plot-substrate target kind -- stays source-side by classification
				}
				if (pEntry->unit != CASC_UNIT_FLAT || pEntry->isConditioned() || pEntry->kind != 0)
				{
					// a per-component percent / an already-conditioned keyed value / a member-kinded slot has no
					// precedent in the data (census: zero) -- left source-side and surfaced, never guessed at
					++s_counts.ownOutputSkipped;
					continue;
				}
				CvModEntry* pLanded = new CvModEntry();
				pLanded->family = pEntry->family;
				pLanded->scope = CASC_SCOPE_PLOT;
				pLanded->kind = 0;
				pLanded->unit = CASC_UNIT_FLAT;
				pLanded->value = pEntry->value;
				pLanded->aiOnly = pEntry->aiOnly;   // the §3.9 audience flag rides the landing whole
				pLanded->seg[0] = modSegmentIntern(infoFamilyKey(pEntry->family));
				pLanded->seg[1] = modSegmentIntern("plot");
				pLanded->nSeg = 2;
				CvCondition* pPresence = new CvCondition();
				pPresence->kind = CASC_COND_PRESENCE;
				pPresence->type = pSourceInfo->getType();
				pPresence->scope = ePresenceScope;
				pPresence->min = 1;
				pPresence->id = iSource;
				pLanded->enabled = pPresence;
				pTargetInfo->landOwnOutputEntry(pLanded);
				++s_counts.ownOutputLanded;
			}
		}
	}

	void rp_landOwnOutput()
	{
		rp_landOwnOutputKind<CvBuildingInfo>(GC.getNumBuildingInfos(), CASC_SCOPE_CITY);
		rp_landOwnOutputKind<CvCivicInfo>(GC.getNumCivicInfos(), CASC_SCOPE_EMPIRE);
		rp_landOwnOutputKind<CvTechInfo>(GC.getNumTechInfos(), CASC_SCOPE_TEAM);
		// traits: NEVER landed (the §4 per-set carve-out); routes: governing-deliverer (§4 exemplar) -- the
		// route-keyed improvement yields stay on the ROUTE's compiled entries (source-side).
	}

	// ==================== sub-pass (1): the forward compat reconstructions ====================
	// The store-inverted authored views un-inverted back onto the forward getters the consumers still read.
	// All of these read COMPILED edges/deposits -- never a legacy-mirror getter.

	// Route<-bonus prereq REVERSE INDEX. curate_route.py inverts a route's PrereqOrBonuses to the bonus's
	// `enables.routes` (the enabler GENERATE edge), so no route JSON carries the relationship. The
	// getRouteInfo(...) callers (CvPlot route validity, CvPlayerAI, CvDLLWidgetData) still ask the route "which
	// bonuses do I need?", so reconstruct each route's OR-list here, once, after every entity is mapped.
	void rp_reconstructRouteBonusPrereqs()
	{
		const int iNumBonuses = GC.getNumBonusInfos();
		for (int iBonus = 0; iBonus < iNumBonuses; ++iBonus)
		{
			const CvInfo* pBonus = InfoRepo<CvBonusInfo>::get().get(iBonus);
			if (pBonus == NULL || pBonus->getEdges() == NULL)
			{
				continue;
			}
			const std::vector<int>* pRoutes = pBonus->getEdges()->find(EDGEF_ENABLES, EDGEB_ROUTES);
			if (pRoutes != NULL)
			{
				for (size_t iRoute = 0; iRoute < pRoutes->size(); ++iRoute)
				{
					static_cast<CvRouteInfo*>(InfoRepo<CvRouteInfo>::get().editPtr((*pRoutes)[iRoute]))->addPrereqOrBonus((BonusTypes)iBonus);
				}
			}
			// the single AND-prereq bonus rides a DISTINCT bucket (store.py routesAnd) so it stays out of the
			// OR-list -- getPrereqBonus (the CvPlot build gate), not getPrereqOrBonuses. A route has at most one
			// single AND bonus.
			const std::vector<int>* pRoutesAnd = pBonus->getEdges()->find(EDGEF_ENABLES, EDGEB_ROUTES_AND);
			if (pRoutesAnd != NULL)
			{
				for (size_t iRoute = 0; iRoute < pRoutesAnd->size(); ++iRoute)
				{
					static_cast<CvRouteInfo*>(InfoRepo<CvRouteInfo>::get().editPtr((*pRoutesAnd)[iRoute]))->setPrereqBonus((BonusTypes)iBonus);
				}
			}
		}
	}

	// (The former improvement<-route YIELD reconstruction is GONE with the wave-B improvement rebuild: it wrote
	// the deleted improvement route-yield mirror (addRouteYieldChange). RouteYieldChanges are governing-deliverer
	// SOURCE-SIDE data (modifier.md §4: "a route upgrading improvements -> on the route, keyed by improvement" --
	// curate_route.py authors {food|production|commerce}.plot.improvements.{IMP}.flat); the improvement-side
	// consumers (CvPlot::calculateImprovementYieldChange + the CvDLLWidgetData help) rewire onto the ROUTE's
	// compiled keyed entries with the stage-4 consumer cut.)

	// STORE-INVERTED TECH-FK REVERSE INDEX -- the Route<-bonus pattern, generalized. curate_*.py DROP each
	// entity's tech prereq/obsolete FK and store-invert it onto the TECH's enables/obsoletes buckets (bonus
	// reveal + cityTrade both -> enables.bonuses, deliberately merged/indistinguishable; corp/project/religion/
	// process/promotion -> enables.<bucket>; bonus/build/corp/promotion obsolete -> obsoletes.<bucket>). The
	// getXInfo(...) compat getters still ask "which tech do I need?" -- a REVERSE lookup -- so reconstruct each
	// target's FK here. getProjectsNeeded is the project<-project variant (off the prereq project's
	// enables.projects). The special-building tech-gate is the same class: the curator stores it as
	// tech.enables.specialBuildings (store.py:181) and the legacy building-group gate reads
	// getSpecialBuildingInfo(x).getTechPrereq() (CvPlayer.cpp:6533). (getTechPrereqAnyone is unused across all
	// groups; no tech obsoletes a special building -- both correctly stay NO_TECH.)
	void rp_reconstructTechForeignKeys()
	{
		const int iNumTechs = GC.getNumTechInfos();
		for (int iTech = 0; iTech < iNumTechs; ++iTech)
		{
			const CvInfo* pTech = InfoRepo<CvTechInfo>::get().get(iTech);
			if (pTech == NULL || pTech->getEdges() == NULL)
			{
				continue;
			}
			const CvEdges* pEdges = pTech->getEdges();
			const TechTypes eTech = (TechTypes)iTech;
			if (const std::vector<int>* pLinkedIds = pEdges->find(EDGEF_ENABLES, EDGEB_BONUSES))
			{
				for (size_t iLinked = 0; iLinked < pLinkedIds->size(); ++iLinked)
				{
					CvBonusInfo* pBonus = static_cast<CvBonusInfo*>(InfoRepo<CvBonusInfo>::get().editPtr((*pLinkedIds)[iLinked]));
					if (pBonus->getTechReveal() == NO_TECH)
					{
						// first (lowest-id) tech wins -- merged/indistinguishable
						pBonus->setTechReveal(eTech);
						pBonus->setTechCityTrade(eTech);
					}
				}
			}
			if (const std::vector<int>* pLinkedIds = pEdges->find(EDGEF_OBSOLETES, EDGEB_BONUSES))
			{
				for (size_t iLinked = 0; iLinked < pLinkedIds->size(); ++iLinked)
				{
					static_cast<CvBonusInfo*>(InfoRepo<CvBonusInfo>::get().editPtr((*pLinkedIds)[iLinked]))->setTechObsolete(eTech);
				}
			}
			if (const std::vector<int>* pLinkedIds = pEdges->find(EDGEF_OBSOLETES, EDGEB_BUILDS))
			{
				for (size_t iLinked = 0; iLinked < pLinkedIds->size(); ++iLinked)
				{
					static_cast<CvBuildInfo*>(InfoRepo<CvBuildInfo>::get().editPtr((*pLinkedIds)[iLinked]))->setObsoleteTech(eTech);
				}
			}
			if (const std::vector<int>* pLinkedIds = pEdges->find(EDGEF_ENABLES, EDGEB_PROJECTS))
			{
				for (size_t iLinked = 0; iLinked < pLinkedIds->size(); ++iLinked)
				{
					static_cast<CvProjectInfo*>(InfoRepo<CvProjectInfo>::get().editPtr((*pLinkedIds)[iLinked]))->setTechPrereq(eTech);
				}
			}
			if (const std::vector<int>* pLinkedIds = pEdges->find(EDGEF_ENABLES, EDGEB_CORPORATIONS))
			{
				for (size_t iLinked = 0; iLinked < pLinkedIds->size(); ++iLinked)
				{
					static_cast<CvCorporationInfo*>(InfoRepo<CvCorporationInfo>::get().editPtr((*pLinkedIds)[iLinked]))->setTechPrereq(eTech);
				}
			}
			if (const std::vector<int>* pLinkedIds = pEdges->find(EDGEF_OBSOLETES, EDGEB_CORPORATIONS))
			{
				for (size_t iLinked = 0; iLinked < pLinkedIds->size(); ++iLinked)
				{
					static_cast<CvCorporationInfo*>(InfoRepo<CvCorporationInfo>::get().editPtr((*pLinkedIds)[iLinked]))->setObsoleteTech(eTech);
				}
			}
			if (const std::vector<int>* pLinkedIds = pEdges->find(EDGEF_ENABLES, EDGEB_RELIGIONS))
			{
				for (size_t iLinked = 0; iLinked < pLinkedIds->size(); ++iLinked)
				{
					static_cast<CvReligionInfo*>(InfoRepo<CvReligionInfo>::get().editPtr((*pLinkedIds)[iLinked]))->setTechPrereq(eTech);
				}
			}
			if (const std::vector<int>* pLinkedIds = pEdges->find(EDGEF_ENABLES, EDGEB_PROCESSES))
			{
				for (size_t iLinked = 0; iLinked < pLinkedIds->size(); ++iLinked)
				{
					static_cast<CvProcessInfo*>(InfoRepo<CvProcessInfo>::get().editPtr((*pLinkedIds)[iLinked]))->setTechPrereq(eTech);
				}
			}
			if (const std::vector<int>* pLinkedIds = pEdges->find(EDGEF_ENABLES, EDGEB_PROMOTIONS))
			{
				for (size_t iLinked = 0; iLinked < pLinkedIds->size(); ++iLinked)
				{
					static_cast<CvPromotionInfo*>(InfoRepo<CvPromotionInfo>::get().editPtr((*pLinkedIds)[iLinked]))->setTechPrereq(eTech);
				}
			}
			if (const std::vector<int>* pLinkedIds = pEdges->find(EDGEF_OBSOLETES, EDGEB_PROMOTIONS))
			{
				for (size_t iLinked = 0; iLinked < pLinkedIds->size(); ++iLinked)
				{
					static_cast<CvPromotionInfo*>(InfoRepo<CvPromotionInfo>::get().editPtr((*pLinkedIds)[iLinked]))->setObsoleteTech(eTech);
				}
			}
			if (const std::vector<int>* pLinkedIds = pEdges->find(EDGEF_ENABLES, EDGEB_PROMOTION_LINES))
			{
				for (size_t iLinked = 0; iLinked < pLinkedIds->size(); ++iLinked)
				{
					static_cast<CvPromotionLineInfo*>(InfoRepo<CvPromotionLineInfo>::get().editPtr((*pLinkedIds)[iLinked]))->setTechPrereq(eTech);
				}
			}
			if (const std::vector<int>* pLinkedIds = pEdges->find(EDGEF_OBSOLETES, EDGEB_PROMOTION_LINES))
			{
				for (size_t iLinked = 0; iLinked < pLinkedIds->size(); ++iLinked)
				{
					static_cast<CvPromotionLineInfo*>(InfoRepo<CvPromotionLineInfo>::get().editPtr((*pLinkedIds)[iLinked]))->setObsoleteTech(eTech);
				}
			}
			if (const std::vector<int>* pLinkedIds = pEdges->find(EDGEF_ENABLES, EDGEB_SPECIAL_BUILDINGS))
			{
				for (size_t iLinked = 0; iLinked < pLinkedIds->size(); ++iLinked)
				{
					static_cast<CvSpecialBuildingInfo*>(InfoRepo<CvSpecialBuildingInfo>::get().editPtr((*pLinkedIds)[iLinked]))->setTechPrereq(eTech);
				}
			}
		}
		// project <- project: PrereqProjects store-inverted onto the prerequisite project's enables.projects.
		const int iNumProjects = GC.getNumProjectInfos();
		for (int iProject = 0; iProject < iNumProjects; ++iProject)
		{
			const CvInfo* pProject = InfoRepo<CvProjectInfo>::get().get(iProject);
			if (pProject == NULL || pProject->getEdges() == NULL)
			{
				continue;
			}
			if (const std::vector<int>* pLinkedIds = pProject->getEdges()->find(EDGEF_ENABLES, EDGEB_PROJECTS))
			{
				for (size_t iLinked = 0; iLinked < pLinkedIds->size(); ++iLinked)
				{
					static_cast<CvProjectInfo*>(InfoRepo<CvProjectInfo>::get().editPtr((*pLinkedIds)[iLinked]))->addProjectNeeded(iProject);
				}
			}
		}
	}

	// SHRINE-BUILDING REGISTRY FEED. The building authors the relationship as its §9 `shrine` FK
	// (CvBuildingInfo::getShrineReligion -- json.md §9: the building declares only the FK; the per-commerce
	// values live on the RELIGION); the consumers (CvCityAI:958 / CvCity:19016 foreach getShrineBuildings)
	// ask the RELIGION for its shrine buildings. The legacy buildings-self-register path died in the cutover
	// (addShrineBuilding had ZERO callers), so feed the registry here from the compiled FK. Idempotent like
	// the sibling sub-passes: the repos are cleared and re-mapped before each full pass, so the vector starts
	// empty. (No corporation analog exists: no HQ-building registry lives on CvCorporationInfo and no consumer
	// asks for one -- the corp HQ CITY is CvGame state; verified, nothing to feed.)
	void rp_feedShrineBuildings()
	{
		const int iNumBuildings = GC.getNumBuildingInfos();
		for (int iBuilding = 0; iBuilding < iNumBuildings; ++iBuilding)
		{
			const CvBuildingInfo* pBuilding = static_cast<const CvBuildingInfo*>(InfoRepo<CvBuildingInfo>::get().get(iBuilding));
			if (pBuilding == NULL)
			{
				continue;
			}
			const int iReligion = pBuilding->getShrineReligion();
			if (iReligion < 0)
			{
				continue;
			}
			CvReligionInfo* pReligion = static_cast<CvReligionInfo*>(InfoRepo<CvReligionInfo>::get().editPtr(iReligion));
			if (pReligion != NULL)
			{
				pReligion->addShrineBuilding(iBuilding);
			}
		}
	}

	// The tech-side FORWARD obsoletion views (building/process obsoletion is authored TARGET-side,
	// obsoletedBy.techs; nothing authors tech.obsoletes.buildings) -- reconstructed so the enabler's O(delta)
	// tech application can find "which buildings/processes does this tech obsolete" without a candidate scan.
	void rp_reconstructTechObsoletionViews()
	{
		for (int iBuilding = 0; iBuilding < GC.getNumBuildingInfos(); ++iBuilding)
		{
			const CvInfo* pBuilding = InfoRepo<CvBuildingInfo>::get().get(iBuilding);
			const std::vector<int>* pObsoletingTechs = pBuilding ? pBuilding->edge(EDGEF_OBSOLETED_BY, EDGEB_TECHS) : NULL;
			if (pObsoletingTechs == NULL)
			{
				continue;
			}
			for (size_t iEdge = 0; iEdge < pObsoletingTechs->size(); ++iEdge)
			{
				CvInfo* pTech = InfoRepo<CvTechInfo>::get().editPtr((*pObsoletingTechs)[iEdge]);
				if (pTech != NULL)
				{
					pTech->addReverseEdge(EDGEF_OBSOLETES, EDGEB_BUILDINGS, iBuilding);
				}
			}
		}
		// PROCESSES: the lesser/meager process tiers ("processes is pure enabler gate, with replace": the
		// obsoleting tech drops the tier from the TREE) -- reconstructed exactly like the buildings one above.
		for (int iProcess = 0; iProcess < GC.getNumProcessInfos(); ++iProcess)
		{
			const CvInfo* pProcess = InfoRepo<CvProcessInfo>::get().get(iProcess);
			const std::vector<int>* pObsoletingTechs = pProcess ? pProcess->edge(EDGEF_OBSOLETED_BY, EDGEB_TECHS) : NULL;
			if (pObsoletingTechs == NULL)
			{
				continue;
			}
			for (size_t iEdge = 0; iEdge < pObsoletingTechs->size(); ++iEdge)
			{
				CvInfo* pTech = InfoRepo<CvTechInfo>::get().editPtr((*pObsoletingTechs)[iEdge]);
				if (pTech != NULL)
				{
					pTech->addReverseEdge(EDGEF_OBSOLETES, EDGEB_PROCESSES, iProcess);
				}
			}
		}
	}

	// ==================== the derived-list dedup ====================
	// sortUnique touches ONLY the load-DERIVED families (RELATED/REQUIRED_BY) -- every walked kind may have
	// received entries, so dedup them all (+ the complex trait set).
	template <class RepoTag>
	void rp_sortUniqueKind(int iNumInfos)
	{
		for (int iInfo = 0; iInfo < iNumInfos; ++iInfo)
		{
			CvInfo* pInfo = const_cast<CvInfo*>(InfoRepo<RepoTag>::get().get(iInfo));
			if (pInfo != NULL)
			{
				pInfo->sortUniqueEdges();
			}
		}
	}

	void rp_sortUniqueAll()
	{
		#define X(REPO_TAG, COUNT_GETTER, SOURCE_BUCKET) rp_sortUniqueKind<REPO_TAG>(GC.COUNT_GETTER());
		RP_RELATED_SOURCE_KINDS(X)
		#undef X
		rp_sortUniqueKind<CvComplexTraitTag>(GC.getNumTraitInfos());
		cascadeStartNode().sortUniqueEdges();
	}

	// ==================== sub-pass (5): the unit-plane post-map derivation (json.md §9) ====================

	// The unit's era = its FIRST prereq TECH atom's era (the top-level requires.build AND list, in authored
	// order). The atom's kind is classified by its RESOLVED id through the ONE type dispatch: the routed info
	// is a tech iff it IS the tech repo's object at that id (repos are disjoint object sets, so pointer
	// identity is exact; TECH_GAME_START resolves id -1 and never qualifies). NO_ERA when tech-free.
	int rp_firstPrereqTechEra(const CvInfo* pUnitData)
	{
		const CvCondition* pBuild = pUnitData->requiresBuild();
		if (pBuild == NULL)
		{
			return NO_ERA;
		}
		for (size_t iChild = 0; iChild < pBuild->all.size(); ++iChild)
		{
			const CvCondition* pAtom = pBuild->all[iChild];
			if (pAtom == NULL || pAtom->kind != CASC_COND_PRESENCE || pAtom->id < 0)
			{
				continue;
			}
			const CvInfo* pReferenced = rjInfoForType(pAtom->type, pAtom->id);
			if (pReferenced != NULL && pReferenced == InfoRepo<CvTechInfo>::get().get(pAtom->id))
			{
				return static_cast<const CvTechInfo*>(pReferenced)->getEra();
			}
		}
		return NO_ERA;
	}

	// Walk the unit repo and recompute-assign every CvUnitInfo's load-derived members (the SM base sums, the
	// derived era, the can-acquire-experience verdict, the upgrade chain) -- runnable only HERE, after every
	// mapFrom, because the sums span OTHER registries (unitcombats / techs / promotions / units). Idempotent
	// like the sibling sub-passes. The promotion-applicability union is the one cross-registry precompute,
	// built once and shared by every unit's verdict.
	void rp_deriveUnitPlane()
	{
		std::set<int> combatClassesWithPromotions;
		const int iNumPromotions = GC.getNumPromotionInfos();
		for (int iPromotion = 0; iPromotion < iNumPromotions; ++iPromotion)
		{
			const CvInfo* pPromotionData = InfoRepo<CvPromotionInfo>::get().get(iPromotion);
			if (pPromotionData == NULL)
			{
				continue;
			}
			const CvPromotionInfo* pPromotion = static_cast<const CvPromotionInfo*>(pPromotionData);
			const std::vector<int>& applicableClasses = pPromotion->getUnitCombats();
			for (size_t iEntry = 0; iEntry < applicableClasses.size(); ++iEntry)
			{
				combatClassesWithPromotions.insert(applicableClasses[iEntry]);
			}
		}
		const int iNumUnits = GC.getNumUnitInfos();
		for (int iUnit = 0; iUnit < iNumUnits; ++iUnit)
		{
			CvInfo* pUnitData = const_cast<CvInfo*>(InfoRepo<CvUnitInfo>::get().get(iUnit));
			if (pUnitData == NULL)
			{
				continue;
			}
			CvUnitInfo* pUnit = static_cast<CvUnitInfo*>(pUnitData);
			pUnit->deriveAtRegistryComplete(rp_firstPrereqTechEra(pUnitData), combatClassesWithPromotions);
		}
	}
}

void reversePassRun()
{
	const DWORD iStartTick = GetTickCount();
	s_counts = ReversePassCounts();
	rp_reconstructRouteBonusPrereqs();
	rp_feedShrineBuildings();
	rp_reconstructTechForeignKeys();
	rp_reconstructTechObsoletionViews();
	// the own-output landing runs BEFORE the RELATED walk, so a landed entry's source-presence condition feeds
	// the display inversion too (the improvement lands on the building's RELATED[improvements] -- the one
	// direction that can carry it, since the plot-substrate infos compose no CvEdges)
	rp_landOwnOutput();
	rp_buildRelated();
	rp_buildRequiredBy();
	rp_sortUniqueAll();
	rp_deriveUnitPlane();
	s_counts.milliseconds = (unsigned int)(GetTickCount() - iStartTick);
}

const ReversePassCounts& reversePassCounts()
{
	return s_counts;
}
