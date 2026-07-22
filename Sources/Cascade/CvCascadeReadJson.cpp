//
//	CvCascadeReadJson -- the fresh reader of the curated Assets/Data JSON. See the header for the build plan + scope.
//
//	AUDIT 2026-07-01 (owner): the reader is now a THIN ORCHESTRATOR, not a god-function that knows every type. It:
//	  (1) finds + parses each Assets/Data/*.json,
//	  (2) resolves the entity's type id + selects its per-type InfoRepo (prefix dispatch, RJ_REPO_TYPES),
//	  (3) calls `data->mapFrom(v)` -- VIRTUAL dispatch: the info "loads itself" (the base parses the common cascade
//	      sections; each per-type CvJson*Info subclass adds its ONE extension block -- skills/tags/capabilities/policies/
//	      identity), drawing shared primitives from JsonInfo/CvJsonParse,
//	  (4) runs a SEPARATE generic CENSUS that classifies every top-level key (jsonClassifyKey) to prove 0
//	      UNCLASSIFIED, surfaces every unresolved FK id (jsonUnresolvedIds), and reads the mapped data back for
//	      the survey counts. The per-type parsing that used to live here (rj_walk{Capabilities,Policies,Identity,Shrine,
//	      Mod,EnableEdge,...} + s_rjData) has MOVED onto the types -- the reader no longer re-hand-rolls it.
//

#include "CvGameCoreDLL.h"             // PCH umbrella -- picojson, windows.h, gDLL, GC
#include "CvCascadeReadJson.h"
#include "Defines/CvGlobals.h"         // GC.getInfoTypeForString -- the type registry (entity id resolution)
#include "CvEventSpine.h"              // the #430 dispatch spine -- the [READJSON] census rides it as a CONSUMER
#include "CvJsonParse.h"               // jsonClassifyKey / jsonUnresolvedIds -- shared vocabulary + FK diag
#include "CvInfo.h"                // CvInfo (+ cascadeStartNode) -- the mapped info data + the TECH_GAME_START root
#include "CvCascadeDepositIndex.h"     // DepositIndex::pushInfo/clearCompiled -- the compiled deposit index (push-time interning)
#include "CvClassificationRegistry.h"  // the §8/§9 generated classification categories -- minted + resolved post-map
#include "CvTechInfo.h"            // CvTechInfo -- for the capabilities read-back survey
#include "CvImprovementInfo.h"     // CvImprovementInfo -- the reverse-view improvement relations
#include "CvBuildingInfo.h"        // the REVERSE-VIEW build pass (rj_buildReverseView) -- the tech-referencing relations
#include "CvUnitInfo.h"
#include "CvBonusInfo.h"
#include "CvCivicInfo.h"
#include "CvHeritageInfo.h"
#include "CvBuildInfo.h"
#include "CvPromotionInfo.h"
#include "CvSpecialistInfo.h"      // /state/info typed-member dispatch (rjInfoForType)
#include "CvUnitCombatInfo.h"      // /state/info typed-member dispatch (rjInfoForType)
#include "CvInfos.h"               // legacy CvSpecialBuildingInfo (XML-loaded uniformity set) -- scanned by the reverse view
#include "CvJsonCondition.h"       // the typed requires tree -- walked by the REQUIRED_BY inversion
#include "CvEnablerKernel.h"       // EnablerKernel::jsonFor -- the one per-bucket InfoRepo dispatch (single-source)
#include "Repos/InfoRepo.h"            // the per-info-type home (InfoRepo<CvXInfo>) -- readJson edit()s, mapFrom populates;
                                       // the CvXInfo tag types for the RJ_REPO_TYPES prefix dispatch are forward-declared there
#include <fstream>
#include <sstream>
#include <vector>
#include <set>
#include <map>
#include <string>

// ==================== THE REVERSE VIEW (built at JSON read -- modifier.md par.1) ====================
// The info objects ALREADY CARRY their reverse lookups after load: this pass inverts, onto each referenced
// TECH's own edges (EDGEF_RELATED, per kind), every tech-referencing relation the consumers check --
// prereqs, obsoletes, tech-keyed value tables, secondary gates -- exactly the getters setTechHelp et al.
// evaluate, so a consumer keeps its exact predicate and iterates the (tiny) candidate list instead of the
// whole database. ⛔ EDGEF_RELATED is DISPLAY-ONLY (a candidate SUPERSET): the enabler's GENERATE/GATE
// never reads it; the gate's own axis is EDGEF_REQUIRED_BY, populated here when the requires-gate stage
// lands -- NEVER as a bespoke index inside an enabler.

// The reverse-build observability counters + timing (emitted as RJE_REVERSE_DONE at the call site -- the
// SD_READJSON enums are declared below this block).
static int s_rvRelated = 0, s_rvRequiredBy = 0;
static DWORD s_rvMs = 0;

// Add iId under the referenced tech's RELATED bucket (skips NO_TECH).
static void rvAddTech(TechTypes eTech, EnEdgeBucket eBucket, int iId)
{
	if (eTech <= NO_TECH || (int)eTech >= GC.getNumTechInfos()) return;
	CvInfo* jt = InfoRepo<CvTechInfo>::get().editPtr((int)eTech);
	if (jt != NULL) { jt->addReverseEdge(EDGEF_RELATED, eBucket, iId); ++s_rvRelated; }
}

// --- the requires -> EDGEF_REQUIRED_BY inversion (the enabler's requires-reverse-index; enabler.md par.7.1
// step 2, DEC-one-reverse-view): every HAVE-axis atom a dependent's requires references gains that dependent
// under the dependent's kind bucket, ON THE REFERENCED INFO -- the gate stage re-gates exactly these
// dependents when the atom's HAVE-event fires, never a database sweep. ---

// The referenced HAVE-axis info by its INFOTYPE prefix (naming.md routes by prefix). NULL = not a HAVE-axis
// re-gate target: engine tokens (TURN/POPULATION/ERA), the plot substrate (terrain/feature/improvement --
// the dynamic plot axis with its own event routing), and PROPERTY_ bands (the property engine's axis).
static CvInfo* rvRefInfo(const std::string& t, int id)
{
	// the synthetic root's cascade data lives OFF the InfoRepo (cascadeStartNode) -- route it there, never the
	// GC poco (the split-brain the /state/info read exposed)
	if (t == "TECH_GAME_START")            return &cascadeStartNode();
	if (!t.compare(0, 5, "TECH_"))         return InfoRepo<CvTechInfo>::get().editPtr(id);
	if (!t.compare(0, 9, "BUILDING_"))     return InfoRepo<CvBuildingInfo>::get().editPtr(id);
	if (!t.compare(0, 6, "BONUS_"))        return InfoRepo<CvBonusInfo>::get().editPtr(id);
	if (!t.compare(0, 6, "CIVIC_"))        return InfoRepo<CvCivicInfo>::get().editPtr(id);
	if (!t.compare(0, 5, "UNIT_"))         return InfoRepo<CvUnitInfo>::get().editPtr(id);
	if (!t.compare(0, 9, "RELIGION_"))     return InfoRepo<CvReligionInfo>::get().editPtr(id);
	if (!t.compare(0, 12, "CORPORATION_")) return InfoRepo<CvCorporationInfo>::get().editPtr(id);
	if (!t.compare(0, 9, "HERITAGE_"))     return InfoRepo<CvHeritageInfo>::get().editPtr(id);
	if (!t.compare(0, 8, "PROJECT_"))      return InfoRepo<CvProjectInfo>::get().editPtr(id);
	return NULL;
}

// Recurse one requires tree: every FK-resolved PRESENCE atom + parameterized PREDICATE lands the dependent on
// the referenced info's REQUIRED_BY bucket.
static void rvRequiresWalk(const CvJsonCondition* c, EnEdgeBucket eDepKind, int iDepId)
{
	if (c == NULL) return;
	for (size_t i = 0; i < c->all.size(); ++i)    rvRequiresWalk(c->all[i], eDepKind, iDepId);
	for (size_t i = 0; i < c->anyOf.size(); ++i)  rvRequiresWalk(c->anyOf[i], eDepKind, iDepId);
	for (size_t i = 0; i < c->noneOf.size(); ++i) rvRequiresWalk(c->noneOf[i], eDepKind, iDepId);
	rvRequiresWalk(c->enabled, eDepKind, iDepId);
	rvRequiresWalk(c->disabled, eDepKind, iDepId);
	if (c->id < 0) return;
	CvInfo* jr = NULL;
	if (c->kind == CASC_COND_PRESENCE) jr = rvRefInfo(c->type, c->id);
	else if (c->kind == CASC_COND_PREDICATE && !c->param.empty()) jr = rvRefInfo(c->param, c->id);
	if (jr != NULL) { jr->addReverseEdge(EDGEF_REQUIRED_BY, eDepKind, iDepId); ++s_rvRequiredBy; }
}

// The ONE INFOTYPE-prefix -> InfoRepo dispatch (exported; header decl). Broader than rvRefInfo above, which
// deliberately keeps only the HAVE-axis re-gate kinds.
CvInfo* rjInfoForType(const std::string& t, int iId)
{
	if (CvInfo* j = rvRefInfo(t, iId)) return j;
	if (!t.compare(0, 10, "PROMOTION_"))    return InfoRepo<CvPromotionInfo>::get().editPtr(iId);
	if (!t.compare(0, 8, "PROCESS_"))       return InfoRepo<CvProcessInfo>::get().editPtr(iId);
	if (!t.compare(0, 6, "BUILD_"))         return InfoRepo<CvBuildInfo>::get().editPtr(iId);
	if (!t.compare(0, 12, "IMPROVEMENT_"))  return InfoRepo<CvImprovementInfo>::get().editPtr(iId);
	if (!t.compare(0, 14, "PROMOTIONLINE_")) return InfoRepo<CvPromotionLineInfo>::get().editPtr(iId);
	if (!t.compare(0, 11, "SPECIALIST_"))   return InfoRepo<CvSpecialistInfo>::get().editPtr(iId);
	if (!t.compare(0, 11, "UNITCOMBAT_"))   return InfoRepo<CvUnitCombatInfo>::get().editPtr(iId);
	// the runtime-GENERATED classification categories (SKILL_/TAG_/ATTRIBUTE_/CAPABILITY_/POLICY_) -- referenceable
	// like any authored info ([DEC-classification-infos]); cold-path const view, cast for the shared return type.
	return const_cast<CvInfo*>(ClassificationRegistry::infoForType(t));
}

static int rvNumFor(EnEdgeBucket b)
{
	switch (b)
	{
	case EDGEB_BUILDINGS:  return GC.getNumBuildingInfos();
	case EDGEB_UNITS:      return GC.getNumUnitInfos();
	case EDGEB_TECHS:      return GC.getNumTechInfos();
	case EDGEB_CIVICS:     return GC.getNumCivicInfos();
	case EDGEB_PROJECTS:   return GC.getNumProjectInfos();
	case EDGEB_PROCESSES:  return GC.getNumProcessInfos();
	case EDGEB_PROMOTIONS: return GC.getNumPromotionInfos();
	case EDGEB_BUILDS:     return GC.getNumBuildInfos();
	default:               return 0;
	}
}

static void rj_buildReverseView()
{
	const DWORD rvT0 = GetTickCount();
	s_rvRelated = 0; s_rvRequiredBy = 0;
	const int nTech = GC.getNumTechInfos();

	for (int i = 0; i < GC.getNumBuildingInfos(); ++i)
	{
		const CvBuildingInfo& k = GC.getBuildingInfo((BuildingTypes)i);
		rvAddTech(k.getObsoleteTech(), EDGEB_BUILDINGS, i);
		rvAddTech((TechTypes)k.getPrereqAndTech(), EDGEB_BUILDINGS, i);
		// The tech-side FORWARD obsoletion view (building obsoletion is authored TARGET-side, obsoletedBy.techs;
		// nothing authors tech.obsoletes.buildings) -- reconstructed here so the enabler's O(delta) tech
		// application can find "which buildings does this tech obsolete" without a candidate scan
		// (the leadsTo/getPrereqBonus reconstruction class, readjson.md).
		{
			const CvInfo* jb = InfoRepo<CvBuildingInfo>::get().get(i);
			const std::vector<int>* obs = jb ? jb->edge(EDGEF_OBSOLETED_BY, EDGEB_TECHS) : NULL;
			if (obs != NULL)
				for (size_t j = 0; j < obs->size(); ++j)
				{
					CvInfo* jt = InfoRepo<CvTechInfo>::get().editPtr((*obs)[j]);
					if (jt != NULL) jt->addReverseEdge(EDGEF_OBSOLETES, EDGEB_BUILDINGS, i);
				}
		}
		for (size_t j = 0; j < k.getPrereqAndTechs().size(); ++j)
			rvAddTech(k.getPrereqAndTechs()[j], EDGEB_BUILDINGS, i);
		foreach_(const TechArray& p, k.getTechYieldChanges100())    rvAddTech(p.first, EDGEB_BUILDINGS, i);
		foreach_(const TechArray& p, k.getTechYieldModifiers())     rvAddTech(p.first, EDGEB_BUILDINGS, i);
		foreach_(const TechCommerceArray& p, k.getTechCommerceChanges100()) rvAddTech(p.first, EDGEB_BUILDINGS, i);
		foreach_(const TechCommerceArray& p, k.getTechCommerceModifiers())  rvAddTech(p.first, EDGEB_BUILDINGS, i);
		for (int t = 0; t < nTech; ++t)
		{
			const TechTypes eT = (TechTypes)t;
			if (k.getTechHappiness(eT) != 0 || k.getTechHealth(eT) != 0) { rvAddTech(eT, EDGEB_BUILDINGS, i); continue; }
			if (k.isAnyTechSpecialistChanges())
				for (int s = 0; s < GC.getNumSpecialistInfos(); ++s)
					if (k.getTechSpecialistChange(t, s) != 0) { rvAddTech(eT, EDGEB_BUILDINGS, i); break; }
		}
	}
	for (int i = 0; i < GC.getNumUnitInfos(); ++i)
	{
		const CvUnitInfo& k = GC.getUnitInfo((UnitTypes)i);
		rvAddTech((TechTypes)k.getPrereqAndTech(), EDGEB_UNITS, i);
		for (size_t j = 0; j < k.getPrereqAndTechs().size(); ++j)
			rvAddTech((TechTypes)k.getPrereqAndTechs()[j], EDGEB_UNITS, i);
	}
	// The tech-side FORWARD obsoletion view for PROCESSES (authored TARGET-side, obsoletedBy.techs -- the
	// lesser/meager process tiers; "processes is pure enabler gate, with replace": the obsoleting tech drops
	// the tier from the TREE) -- reconstructed exactly like the buildings one above, so the processes domain's
	// O(delta) tech application feeds its REMOVE plane.
	for (int i = 0; i < GC.getNumProcessInfos(); ++i)
	{
		const CvInfo* jp = InfoRepo<CvProcessInfo>::get().get(i);
		const std::vector<int>* obs = jp ? jp->edge(EDGEF_OBSOLETED_BY, EDGEB_TECHS) : NULL;
		if (obs != NULL)
			for (size_t j = 0; j < obs->size(); ++j)
			{
				CvInfo* jt = InfoRepo<CvTechInfo>::get().editPtr((*obs)[j]);
				if (jt != NULL) jt->addReverseEdge(EDGEF_OBSOLETES, EDGEB_PROCESSES, i);
			}
	}
	for (int i = 0; i < GC.getNumBonusInfos(); ++i)
	{
		const CvBonusInfo& k = GC.getBonusInfo((BonusTypes)i);
		rvAddTech((TechTypes)k.getTechReveal(), EDGEB_BONUSES, i);
		rvAddTech((TechTypes)k.getTechObsolete(), EDGEB_BONUSES, i);
	}
	// #430: reconstruct SpecialBuildingInfo::getTechPrereq from the tech-side inversion. The curator stores the
	// special-building tech-gate as tech.enables.specialBuildings (store.py:181); the special-building JSON carries
	// none. The legacy building-group gate reads GC.getSpecialBuildingInfo(x).getTechPrereq() (CvPlayer.cpp:6533),
	// so un-invert it HERE, before the reverse-view below reads it. (getTechPrereqAnyone is unused across all
	// groups; no tech obsoletes a special building -- both correctly stay NO_TECH.)
	for (int t = 0; t < nTech; ++t)
	{
		const CvInfo* jt = InfoRepo<CvTechInfo>::get().get(t);
		const std::vector<int>* sbs = jt ? jt->edge(EDGEF_ENABLES, EDGEB_SPECIAL_BUILDINGS) : NULL;
		if (sbs != NULL)
			for (size_t j = 0; j < sbs->size(); ++j)
			{
				CvSpecialBuildingInfo* sb = static_cast<CvSpecialBuildingInfo*>(
					InfoRepo<CvSpecialBuildingInfo>::get().editPtr((*sbs)[j]));
				if (sb != NULL) sb->setTechPrereq((TechTypes)t);
			}
	}
	for (int i = 0; i < GC.getNumSpecialBuildingInfos(); ++i)
	{
		const CvSpecialBuildingInfo& k = GC.getSpecialBuildingInfo((SpecialBuildingTypes)i);
		rvAddTech((TechTypes)k.getTechPrereq(), EDGEB_SPECIAL_BUILDINGS, i);
		rvAddTech((TechTypes)k.getTechPrereqAnyone(), EDGEB_SPECIAL_BUILDINGS, i);
		rvAddTech((TechTypes)k.getObsoleteTech(), EDGEB_SPECIAL_BUILDINGS, i);
	}
	for (int i = 0; i < GC.getNumImprovementInfos(); ++i)
	{
		const CvImprovementInfo& k = GC.getImprovementInfo((ImprovementTypes)i);
		rvAddTech(k.getPrereqTech(), EDGEB_IMPROVEMENTS, i);
		for (int t = 0; t < nTech; ++t)
			if (k.getTechYieldChangesArray(t) != NULL) rvAddTech((TechTypes)t, EDGEB_IMPROVEMENTS, i);
	}
	for (int i = 0; i < GC.getNumBuildInfos(); ++i)
	{
		const CvBuildInfo& k = GC.getBuildInfo((BuildTypes)i);
		rvAddTech(k.getTechPrereq(), EDGEB_BUILDS, i);
		foreach_(const TerrainStructs& ts, k.getTerrainStructs())
			rvAddTech(ts.ePrereqTech, EDGEB_BUILDS, i);
		for (int f = 0; f < GC.getNumFeatureInfos(); ++f)
			rvAddTech(k.getFeatureTech((FeatureTypes)f), EDGEB_BUILDS, i);
	}
	for (int i = 0; i < GC.getNumCivicInfos(); ++i)
		rvAddTech((TechTypes)GC.getCivicInfo((CivicTypes)i).getTechPrereq(), EDGEB_CIVICS, i);
	for (int i = 0; i < GC.getNumHeritageInfos(); ++i)
		rvAddTech((TechTypes)GC.getHeritageInfo((HeritageTypes)i).getPrereqTech(), EDGEB_HERITAGES, i);
	for (int i = 0; i < GC.getNumProjectInfos(); ++i)
		rvAddTech((TechTypes)GC.getProjectInfo((ProjectTypes)i).getTechPrereq(), EDGEB_PROJECTS, i);
	for (int i = 0; i < GC.getNumProcessInfos(); ++i)
		rvAddTech((TechTypes)GC.getProcessInfo((ProcessTypes)i).getTechPrereq(), EDGEB_PROCESSES, i);
	for (int i = 0; i < GC.getNumReligionInfos(); ++i)
		rvAddTech((TechTypes)GC.getReligionInfo((ReligionTypes)i).getTechPrereq(), EDGEB_RELIGIONS, i);
	for (int i = 0; i < GC.getNumCorporationInfos(); ++i)
		rvAddTech((TechTypes)GC.getCorporationInfo((CorporationTypes)i).getTechPrereq(), EDGEB_CORPORATIONS, i);
	for (int i = 0; i < GC.getNumPromotionInfos(); ++i)
	{
		const CvPromotionInfo& k = GC.getPromotionInfo((PromotionTypes)i);
		rvAddTech((TechTypes)k.getTechPrereq(), EDGEB_PROMOTIONS, i);
		rvAddTech((TechTypes)k.getObsoleteTech(), EDGEB_PROMOTIONS, i);
	}
	// requires -> EDGEF_REQUIRED_BY (the enabler's requires-reverse-index): every dependent kind's
	// requires.build + requires.operate trees + its dormant triggers, inverted onto the referenced infos.
	static const EnEdgeBucket DEP_KINDS[] =
	{
		EDGEB_BUILDINGS, EDGEB_UNITS, EDGEB_TECHS, EDGEB_CIVICS,
		EDGEB_PROJECTS, EDGEB_PROCESSES, EDGEB_PROMOTIONS, EDGEB_BUILDS, NO_EDGEB
	};
	for (int k = 0; DEP_KINDS[k] != NO_EDGEB; ++k)
	{
		const EnEdgeBucket eKind = DEP_KINDS[k];
		const int n = rvNumFor(eKind);
		for (int i = 0; i < n; ++i)
		{
			const CvInfo* j = EnablerKernel::jsonFor(eKind, i);
			if (j == NULL) continue;
			rvRequiresWalk(j->requiresBuild(), eKind, i);
			rvRequiresWalk(j->requiresOperate(), eKind, i);
			// dormant triggers reference same-kind successors (building ReplacementBuildings; unit upgrades)
			const std::vector<int>& dorm = j->dormantTriggers();
			for (size_t d = 0; d < dorm.size(); ++d)
			{
				CvInfo* jr = (CvInfo*)EnablerKernel::jsonFor(eKind, dorm[d]);
				if (jr != NULL) { jr->addReverseEdge(EDGEF_REQUIRED_BY, eKind, i); ++s_rvRequiredBy; }
			}
		}
	}
	// improvements + heritages carry requires too (no jsonFor bucket dispatch -- repo-direct)
	for (int i = 0; i < GC.getNumImprovementInfos(); ++i)
	{
		const CvInfo* j = InfoRepo<CvImprovementInfo>::get().get(i);
		if (j == NULL) continue;
		rvRequiresWalk(j->requiresBuild(), EDGEB_IMPROVEMENTS, i);
		rvRequiresWalk(j->requiresOperate(), EDGEB_IMPROVEMENTS, i);
	}
	for (int i = 0; i < GC.getNumHeritageInfos(); ++i)
	{
		const CvInfo* j = InfoRepo<CvHeritageInfo>::get().get(i);
		if (j == NULL) continue;
		rvRequiresWalk(j->requiresBuild(), EDGEB_HERITAGES, i);
		rvRequiresWalk(j->requiresOperate(), EDGEB_HERITAGES, i);
	}

	// Building improvement-KEYED yields ride DOWN the improvement upgrade chain (the mechanic the model
	// carries; legacy doPostLoadCaching did the same expansion): a yield keyed to LUMBERMILL also lands on a
	// worked TREEFARM/HYBRID_FOREST -- the authored row keys the ANCESTOR only. Expand each building's city +
	// global improvement-yield rows onto every keyed improvement's TRANSITIVE upgrade descendants, so every
	// consumer (the plot fresh-sum, the keyed-ledger recomputes, the endpoints) reads the propagated rows.
	// Building rows ONLY -- civic/trait/tech keyed rows stay direct (the player/team accumulators never
	// propagated). Runs HERE, after the full-registry mapFrom re-run (which clears + re-populates the maps).
	{
		const int iNumImps = GC.getNumImprovementInfos();
		std::vector< std::vector<int> > aDescendants(iNumImps);
		for (int iI = 0; iI < iNumImps; iI++)
		{
			std::set<int> seen;   // cycle guard
			int iCur = (int)GC.getImprovementInfo((ImprovementTypes)iI).getImprovementUpgrade();
			while (iCur >= 0 && seen.insert(iCur).second)
			{
				aDescendants[iI].push_back(iCur);
				iCur = (int)GC.getImprovementInfo((ImprovementTypes)iCur).getImprovementUpgrade();
			}
		}
		for (int iB = 0; iB < GC.getNumBuildingInfos(); iB++)
		{
			CvBuildingInfo& kBuilding = GC.getBuildingInfo((BuildingTypes)iB);
			// snapshot the AUTHORED rows first -- the expansion must never chain off its own additions
			const std::vector<ImprovementArray> aCityRows(kBuilding.getImprovementYieldChanges().begin(), kBuilding.getImprovementYieldChanges().end());
			for (size_t iR = 0; iR < aCityRows.size(); ++iR)
			{
				const int iImp = (int)aCityRows[iR].first;
				if (iImp < 0 || iImp >= iNumImps) continue;
				for (size_t iD = 0; iD < aDescendants[iImp].size(); ++iD)
					kBuilding.addImprovementYieldRow((ImprovementTypes)aDescendants[iImp][iD], aCityRows[iR].second);
			}
			const std::vector<ImprovementArray> aGlobalRows(kBuilding.getGlobalImprovementYieldChanges().begin(), kBuilding.getGlobalImprovementYieldChanges().end());
			for (size_t iR = 0; iR < aGlobalRows.size(); ++iR)
			{
				const int iImp = (int)aGlobalRows[iR].first;
				if (iImp < 0 || iImp >= iNumImps) continue;
				for (size_t iD = 0; iD < aDescendants[iImp].size(); ++iD)
					kBuilding.addGlobalImprovementYieldRow((ImprovementTypes)aDescendants[iImp][iD], aGlobalRows[iR].second);
			}
		}
	}

	// dedup the derived lists (sortUnique touches ONLY the RELATED/REQUIRED_BY families) -- every kind that
	// can carry reverse edges: techs (RELATED + REQUIRED_BY) + the HAVE-axis referenced kinds (REQUIRED_BY)
	for (int t = 0; t < nTech; ++t)
	{
		CvInfo* jt = InfoRepo<CvTechInfo>::get().editPtr(t);
		if (jt != NULL) jt->sortUniqueEdges();
	}
	for (int i = 0; i < GC.getNumBuildingInfos(); ++i)    { CvInfo* j = InfoRepo<CvBuildingInfo>::get().editPtr(i);    if (j) j->sortUniqueEdges(); }
	for (int i = 0; i < GC.getNumUnitInfos(); ++i)        { CvInfo* j = InfoRepo<CvUnitInfo>::get().editPtr(i);        if (j) j->sortUniqueEdges(); }
	for (int i = 0; i < GC.getNumBonusInfos(); ++i)       { CvInfo* j = InfoRepo<CvBonusInfo>::get().editPtr(i);       if (j) j->sortUniqueEdges(); }
	for (int i = 0; i < GC.getNumCivicInfos(); ++i)       { CvInfo* j = InfoRepo<CvCivicInfo>::get().editPtr(i);       if (j) j->sortUniqueEdges(); }
	for (int i = 0; i < GC.getNumReligionInfos(); ++i)    { CvInfo* j = InfoRepo<CvReligionInfo>::get().editPtr(i);    if (j) j->sortUniqueEdges(); }
	for (int i = 0; i < GC.getNumCorporationInfos(); ++i) { CvInfo* j = InfoRepo<CvCorporationInfo>::get().editPtr(i); if (j) j->sortUniqueEdges(); }
	for (int i = 0; i < GC.getNumHeritageInfos(); ++i)    { CvInfo* j = InfoRepo<CvHeritageInfo>::get().editPtr(i);    if (j) j->sortUniqueEdges(); }
	for (int i = 0; i < GC.getNumProjectInfos(); ++i)     { CvInfo* j = InfoRepo<CvProjectInfo>::get().editPtr(i);     if (j) j->sortUniqueEdges(); }

	s_rvMs = GetTickCount() - rvT0;   // announced as RJE_REVERSE_DONE at the call site (the enums live below)
}

// ===================== [READJSON] spine domain (logging.md §4: logging is a spine CONSUMER) =====================
enum RjEvt
{
	RJE_UNRESOLVED = 1, RJE_MOD, RJE_EDGE, RJE_GRANT, RJE_DIR, RJE_PROBE, RJE_COND_SURVEY, RJE_MOD_SURVEY,
	RJE_EDGE_SURVEY, RJE_EDGE_UNRES, RJE_GRANT_SURVEY, RJE_GRANT_UNRES, RJE_KEY, RJE_MAP, RJE_CAP_SURVEY,
	RJE_CAP, RJE_MAP_SUMMARY,
	RJE_REMAPPED,      // one aliased entity's full-registry section re-map: what it maps (type + edge/family counts)
	RJE_MAP_DONE,      // the initial JSON map (PASS 1+2) is DONE -- entities/resolved/remapped/ms
	RJE_REVERSE_DONE   // the reverse-view build (RELATED + REQUIRED_BY) is DONE -- add counts/ms
};

// DOMAIN-LOCAL field tags, shared by name across lines where a field recurs.
enum RjFld
{
	RJF_TYPE = 1, RJF_ADDR, RJF_UNIT, RJF_VAL, RJF_COND, RJF_PER, RJF_EDGE, RJF_BUCKET, RJF_ID, RJF_STATUS, RJF_DIR,
	RJF_FILES, RJF_PARSED, RJF_FAILED, RJF_ENTITIES, RJF_RESOLVED, RJF_UNRESOLVED, RJF_FAMILYKINDS, RJF_FLAGKINDS,
	RJF_REQCLAUSES, RJF_FAMILIES, RJF_MAGNITUDES, RJF_FLAT, RJF_PERCENT, RJF_MULT, RJF_OTHER, RJF_CONDITIONED,
	RJF_PERSCALED, RJF_BAREVALUES, RJF_EDGES, RJF_BUCKETENTRIES, RJF_BUCKETKINDS, RJF_ALLOWEDCLAUSES, RJF_CAPKINDS,
	RJF_LISTENTRIES, RJF_LISTKINDS, RJF_PULSES, RJF_PULSECHANNELS, RJF_FLAGS, RJF_ENTRYARRAYS, RJF_OBJECTS,
	RJF_KEY, RJF_COUNT, RJF_CLASS, RJF_DEPOSITS, RJF_REQBUILD, RJF_REQOPERATE, RJF_ALLOWED, RJF_GRANTLISTS,
	RJF_GRANTPULSES, RJF_GRANTING, RJF_CAPGRANTS, RJF_DISTINCTNAMES, RJF_NAME, RJF_WITHDATA,
	RJF_MS, RJF_REMAPPED, RJF_RELATED, RJF_REQUIREDBY
};

static const char* rj_prefix(int evt)
{
	switch (evt)
	{
	case RJE_UNRESOLVED:   return "[READJSON/unresolved]";
	case RJE_MOD:          return "[READJSON/mod]";
	case RJE_EDGE:         return "[READJSON/edge]";
	case RJE_GRANT:        return "[READJSON/grant]";
	case RJE_DIR:          return "[READJSON/dir]";
	case RJE_PROBE:        return "[READJSON/probe]";
	case RJE_COND_SURVEY:  return "[READJSON/cond-survey]";
	case RJE_MOD_SURVEY:   return "[READJSON/mod-survey]";
	case RJE_EDGE_SURVEY:  return "[READJSON/edge-survey]";
	case RJE_EDGE_UNRES:   return "[READJSON/unresolved-fk]";
	case RJE_GRANT_SURVEY: return "[READJSON/grant-survey]";
	case RJE_GRANT_UNRES:  return "[READJSON/grant-unresolved]";
	case RJE_KEY:          return "[READJSON/key]";
	case RJE_MAP:          return "[READJSON/map]";
	case RJE_CAP_SURVEY:   return "[READJSON/cap-survey]";
	case RJE_CAP:          return "[READJSON/cap]";
	case RJE_MAP_SUMMARY:  return "[READJSON/map-summary]";
	case RJE_REMAPPED:     return "[READJSON/remapped]";
	case RJE_MAP_DONE:     return "[READJSON/map-done]";
	case RJE_REVERSE_DONE: return "[READJSON/reverse-done]";
	default:               return "[READJSON]";
	}
}

static const char* rj_field(int tag, SpineFieldType* peType)
{
	*peType = SFT_INT;
	switch (tag)
	{
	case RJF_TYPE:          *peType = SFT_STR; return "type";
	case RJF_ADDR:          *peType = SFT_STR; return "addr";
	case RJF_UNIT:          *peType = SFT_STR; return "unit";
	case RJF_VAL:           return "val";
	case RJF_COND:          return "conditioned";
	case RJF_PER:           return "per";
	case RJF_EDGE:          *peType = SFT_STR; return "edge";
	case RJF_BUCKET:        *peType = SFT_STR; return "bucket";
	case RJF_ID:            *peType = SFT_STR; return "id";
	case RJF_STATUS:        *peType = SFT_STR; return "status";
	case RJF_DIR:           *peType = SFT_STR; return "dir";
	case RJF_FILES:         return "files";
	case RJF_PARSED:        return "parsed";
	case RJF_FAILED:        return "failed";
	case RJF_ENTITIES:      return "entities";
	case RJF_RESOLVED:      return "resolved";
	case RJF_UNRESOLVED:    return "unresolved";
	case RJF_FAMILYKINDS:   return "familyKinds";
	case RJF_FLAGKINDS:     return "flagKinds";
	case RJF_REQCLAUSES:    return "requiresClauses";
	case RJF_FAMILIES:      return "families";
	case RJF_MAGNITUDES:    return "magnitudes";
	case RJF_FLAT:          return "flat";
	case RJF_PERCENT:       return "percent";
	case RJF_MULT:          return "mult";
	case RJF_OTHER:         return "other";
	case RJF_CONDITIONED:   return "conditioned";
	case RJF_PERSCALED:     return "perScaled";
	case RJF_BAREVALUES:    return "bareValues";
	case RJF_EDGES:         return "edges";
	case RJF_BUCKETENTRIES: return "bucketEntries";
	case RJF_BUCKETKINDS:   return "bucketKinds";
	case RJF_ALLOWEDCLAUSES:return "allowedClauses";
	case RJF_CAPKINDS:      return "capKinds";
	case RJF_LISTENTRIES:   return "listEntries";
	case RJF_LISTKINDS:     return "listKinds";
	case RJF_PULSES:        return "pulses";
	case RJF_PULSECHANNELS: return "pulseChannels";
	case RJF_FLAGS:         return "flags";
	case RJF_ENTRYARRAYS:   return "entryArrays";
	case RJF_OBJECTS:       return "objects";
	case RJF_KEY:           *peType = SFT_STR; return "key";
	case RJF_COUNT:         return "count";
	case RJF_CLASS:         *peType = SFT_STR; return "class";
	case RJF_DEPOSITS:      return "modFamilies";   // was "deposits" -- the retired generic vector; now the §6 family count
	case RJF_REQBUILD:      return "reqBuild";
	case RJF_REQOPERATE:    return "reqOperate";
	case RJF_ALLOWED:       return "allowed";
	case RJF_GRANTLISTS:    return "grantLists";
	case RJF_GRANTPULSES:   return "grantPulses";
	case RJF_GRANTING:      return "grantingEntities";
	case RJF_CAPGRANTS:     return "capGrants";
	case RJF_DISTINCTNAMES: return "distinctNames";
	case RJF_NAME:          *peType = SFT_STR; return "name";
	case RJF_WITHDATA:      return "entitiesWithCascadeData";
	case RJF_MS:            return "ms";
	case RJF_REMAPPED:      return "remapped";
	case RJF_RELATED:       return "relatedIds";
	case RJF_REQUIREDBY:    return "requiredByIds";
	default:                return NULL;
	}
}

// Self-register the SD_READJSON domain once (idempotent) -- so the spine stays domain-agnostic (it never names readJson).
static void rj_registerDomain()
{
	static bool s_reg = false;
	if (!s_reg) { spineRegisterDomain(SD_READJSON, rj_prefix, "Cascade.log", rj_field); s_reg = true; }
}

static bool rj_starts(const std::string& s, const char* p)
{
	const std::string ps(p);
	return s.size() >= ps.size() && s.compare(0, ps.size(), ps) == 0;
}

static bool rj_readFile(const std::string& path, std::string& out)
{
	std::ifstream f(path.c_str(), std::ios::binary);
	if (!f.is_open()) return false;
	std::ostringstream ss; ss << f.rdbuf(); out = ss.str();
	return true;
}

// Recursive *.json walk under a dir (Assets/Data is per-type subdirs). Win32, C++03.
static void rj_find(const std::string& dir, std::vector<std::string>& out)
{
	const std::string pattern = dir + "\\*";
	WIN32_FIND_DATAA fd;
	HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
	if (h == INVALID_HANDLE_VALUE) return;
	do
	{
		const std::string name = fd.cFileName;
		if (name == "." || name == "..") continue;
		const std::string full = dir + "\\" + name;
		if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) rj_find(full, out);
		else if (name.size() > 5 && name.substr(name.size() - 5) == ".json") out.push_back(full);
	} while (FindNextFileA(h, &fd) != 0);
	FindClose(h);
}

// (cascadeJsonForType removed -- the #430 load no longer rides SetGlobalClassInfo->read(); LoadGlobalClassInfoJson
//  scans Assets/Data per category and mapFrom's each entity directly. No XML read on the replaced-info path.)

// The cascade info-type table (X-macro): (type-prefix, CvXInfo class) -- ONE source of truth for the per-type InfoRepo
// selection (edit + clear-all), so a new cascade info type is added in exactly ONE place (cascade-engine-430.md §3
// care-point (d)). Order MATTERS: longer/more-specific prefixes FIRST (UNITCOMBAT_ before UNIT_, CIVICOPTION_ before
// CIVIC_, PROMOTIONLINE_ before PROMOTION_; TRAIT_ covers TRAIT_COMPLEX_). Unlisted types -> NULL (no cascade home).
#define RJ_REPO_TYPES(X)                     \
	X("BUILDING_",      CvBuildingInfo)       \
	X("UNITCOMBAT_",    CvUnitCombatInfo)     \
	X("UNIT_",          CvUnitInfo)           \
	X("TECH_",          CvTechInfo)           \
	X("CIVICOPTION_",   CvCivicOptionInfo)    \
	X("CIVIC_",         CvCivicInfo)          \
	X("CIVILIZATION_",  CvCivilizationInfo)   \
	X("TRAIT_",         CvTraitInfo)          \
	X("SPECIALBUILDING_", CvSpecialBuildingInfo) \
	X("SPECIALIST_",    CvSpecialistInfo)     \
	X("BONUS_",         CvBonusInfo)          \
	X("RELIGION_",      CvReligionInfo)       \
	X("CORPORATION_",   CvCorporationInfo)    \
	X("PROMOTIONLINE_", CvPromotionLineInfo)  \
	X("PROMOTION_",     CvPromotionInfo)      \
	X("IMPROVEMENT_",   CvImprovementInfo)    \
	X("FEATURE_",       CvFeatureInfo)        \
	X("TERRAIN_",       CvTerrainInfo)        \
	X("ROUTE_",         CvRouteInfo)          \
	X("PROJECT_",       CvProjectInfo)        \
	X("PROCESS_",       CvProcessInfo)        \
	X("HERITAGE_",      CvHeritageInfo)       \
	X("CULTURELEVEL_",  CvCultureLevelInfo)   \
	X("BUILD_",         CvBuildInfo)          \
	X("PROPERTY_",      CvPropertyInfo)

// get-or-create the entity's CvInfo (the reader calls mapFrom on it); NULL for non-cascade types.
static CvInfo* rj_jsonEdit(const std::string& t, int id)
{
	if (id < 0) return NULL;
#define X(PFX, T) if (rj_starts(t, PFX)) return InfoRepo<T>::get().editPtr(id);
	RJ_REPO_TYPES(X)
#undef X
	return NULL;
}

// #430 collapse: is this type's repo ALIASED over GC.m_pa<X>Info? An aliased type's JSON is loaded by CvInfo::read()
// (per entity, at its SetGlobalClassInfo moment) straight into the GC object this repo views -- so cascadeLoadJson must
// NOT mapFrom it a second time (mapFrom accumulates edges/deposits; a re-map would double-count). The JSON-only OWNED
// types (Heritage/Build/complex) have no XML shell / no read(), so they stay cascadeLoadJson's to map.
static bool rj_isAliased(const std::string& t)
{
#define X(PFX, T) if (rj_starts(t, PFX)) return InfoRepo<T>::get().isAliased();
	RJ_REPO_TYPES(X)
#undef X
	return false;
}

// PASS-1 id lookup -- REUSE-ONLY (owner ruling 2026-07-08). readJson mirrors the XML's premenu/postmenu load PHASING:
// a type is mapped only AFTER its XML shell has registered its id (the 63 premenu types at the LoadPreMenuGlobals
// map; the 18 postmenu process/vote/espionage/spawn types at the LoadPostMenuGlobals re-map). readJson must NOT mint
// an id for a type whose XML shell has not loaded yet -- that pre-registered a postmenu type before its XML array
// existed, so SetGlobalClassInfo saw it as "already loaded" and deref'd the empty array (aInfos[id], the load crash).
// So: return the XML-registered id if present, else -1 = DEFER (skipped this pass, mapped on the re-run once its XML
// phase has loaded). Complex traits reuse the ENGINE id (their own separate repo prevents collision with the simple set).
static int rj_registerId(const std::string& t)
{
	return GC.getInfoTypeForString(t.c_str(), true);   // >=0 reuse the XML shell's id; -1 defer to the phase that loads it
}

// Clear every cascade InfoRepo (free all CvInfo) BEFORE (re)mapping, so a re-run can't DOUBLE the deposit vectors
// (cascade-engine-430.md §3 care-point (a)). No-op on the one-shot first run; makes the map re-run-safe at cutover.
static void rj_clearAllRepos()
{
	DepositIndex::clearCompiled();                // the compiled registry keys the about-to-be-freed infos -- drop it
	                                              // FIRST (the interner stays, append-only: ids survive the re-map)
#define X(PFX, T) InfoRepo<T>::get().clear();
	RJ_REPO_TYPES(X)
#undef X
	InfoRepo<CvComplexTraitTag>::get().clear();   // the complex trait set's own repo (off the RJ_REPO_TYPES dispatch)
	cascadeStartNodeReset();                      // the synthetic TECH_GAME_START root lives off the InfoRepo --
	                                              // reset-RECREATE (write-once discipline; a re-map gets a fresh node)
}

// One walked entity: its type string, engine id, and the CvInfo it mapped into (a stable pointer -- the InfoRepo /
// start node / complex repo own it and outlive this call -- so the census reads it straight back).
struct RjEntity { std::string type; int typeId; CvInfo* data; picojson::value value; std::string path; };   // value+path stashed in PASS 1 for the PASS-2 map

// Probe-stat stash (set=true stores; set=false reads). Lets a post-load emitter surface what the DARK load-time
// burst saw: how many files the dataDir scan found + how many entities parsed, and the dataDir string itself.
const std::string& cascadeReadJsonStats(bool bSet, int& iFiles, int& iEntities, const std::string& sDir)
{
	static int s_files = -1, s_entities = -1;   // -1 = the probe never ran
	static std::string s_dir;
	if (bSet) { s_files = iFiles; s_entities = iEntities; s_dir = sDir; }
	else { iFiles = s_files; iEntities = s_entities; }
	return s_dir;
}

void cascadeLoadJson()
{
	// TWO-PHASE, mirroring the XML's premenu/postmenu load phasing + DELAYED READ (owner ruling 2026-07-08). readJson
	// runs at the END of BOTH LoadPreMenuGlobals and LoadPostMenuGlobals. rj_registerId is REUSE-ONLY, so each pass maps
	// exactly the types whose XML shell has registered by then: the premenu pass maps the premenu-XML set (all the
	// terrain/plot/mapscript infos + the other premenu types); the postmenu pass -- once processes/votes/espionage/
	// spawns are XML-registered too -- rj_clearAllRepos-frees the premenu pass and re-maps EVERYTHING with FULL FK
	// resolution (readJson's equivalent of the XML delayed read: FKs resolve only when every target is loaded; a
	// premenu-only map drops the edges to not-yet-loaded types -- the canMaintain empty-frontier bug). NOT one-shot: the
	// postmenu re-run is what completes the FK edges. UNCONDITIONAL: no gPlayerLogLevel dependency (cold this early) --
	// the [READJSON/*] census rides the event spine (SD_READJSON; the log consumer gates per level).
	spineRegisterConsumers();   // register the spine's logging CONSUMER (idempotent) before the census emits
	rj_registerDomain();
	rj_clearAllRepos();           // care-point (a): re-map-safe (no-op first run)
	jsonResetDiag();              // reset the FK-unresolved accumulator (surfaced below)

	// Always-on load timing (the spine census above is DARK at load -- gPlayerLogLevel 0). Grep `[READJSON]` in
	// Loading.log to SEE the JSON read progress + per-phase ms, so a slow/stuck load is diagnosable from the log.
	const DWORD s2sT0 = GetTickCount();
	gDLL->logMsg("Loading.log", "[READJSON] BEGIN cascadeLoadJson", true, false);

	std::string base = gDLL->getModName(true);
	if (!base.empty() && base[base.size() - 1] != '\\' && base[base.size() - 1] != '/') base += "\\";
	const std::string dataDir = base + "Assets\\Data";

	std::vector<std::string> files;
	rj_find(dataDir, files);
	gDLL->logMsg("Loading.log", CvString::format("[READJSON] scan dir=%s files=%u ms=%u", dataDir.c_str(), (unsigned)files.size(), (unsigned)(GetTickCount() - s2sT0)).c_str(), true, false);

	int iFailed = 0, iEntities = 0, iResolved = 0, iUnresolved = 0, iShownUnres = 0, iRemapped = 0;
	std::set<std::string> familyKinds, flagKinds;
	std::map<std::string, int> topKeys;                       // FULL-COVERAGE census: every top-level key kind -> count
	std::map<std::string, JsonKeyClass> keyClass;             // key -> its class (for the RJE_KEY completeness line)
	std::vector<RjEntity> store;

	// ===== PASS 1 -- REGISTER: readJson OWNS the enum now (XML archived). Assign each entity a PER-CATEGORY id in
	// LOAD ORDER + register type->id; the registry must be COMPLETE before ANY mapFrom, else a FORWARD FK reference
	// would drop. Parse ONCE -- the parsed value + path are stashed on the RjEntity for PASS 2. TECH_GAME_START is the
	// synthetic no-engine-id root (id -1, off the InfoRepo).
	for (size_t i = 0; i < files.size(); ++i)
	{
		std::string text;
		if (!rj_readFile(files[i], text)) { ++iFailed; continue; }
		picojson::value v;
		const std::string err = picojson::parse(v, text);
		if (!err.empty() || !v.is<picojson::object>()) { ++iFailed; continue; }
		const picojson::object& o = v.get<picojson::object>();
		picojson::object::const_iterator t = o.find("type");
		if (t == o.end() || !t->second.is<std::string>()) continue;
		++iEntities;
		const std::string type = t->second.get<std::string>();
		// COMPLEX traits (the `\complex\` folder) are keyed by the SAME ENGINE id as their type string. The simple and
		// complex sets live in SEPARATE repos (CvTraitInfo vs CvComplexTraitTag), so they never collide even sharing an id.
		// The active-set consumers (getTraitInfo / MMKernel::traitData) index the complex repo BY THE ENGINE id, so it MUST
		// be keyed that way. LoadGlobalClassInfoJson registers EVERY trait type (simple / complex-only developing levels /
		// shared) in m_paTraitInfo, so rj_registerId resolves the engine id for complex traits too. (PASS-2 still routes the
		// \complex\ files into the complex repo -- only the id changed from a private counter to the engine id.)
		const int typeId = (type == "TECH_GAME_START") ? -1 : rj_registerId(type);   // engine id (or -1 = DEFER to its load phase)
		if (typeId >= 0) ++iResolved;
		else { ++iUnresolved; if (iShownUnres < 16) { eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_READJSON, RJE_UNRESOLVED, 1).addStr(RJF_TYPE, type.c_str())); ++iShownUnres; } }
		RjEntity rec; rec.type = type; rec.typeId = typeId; rec.data = NULL; rec.value = v; rec.path = files[i];
		store.push_back(rec);
	}
	gDLL->logMsg("Loading.log", CvString::format("[READJSON] PASS1-register parsed=%d entities=%d resolved=%d deferred=%d failed=%d ms=%u",
		(int)files.size() - iFailed, iEntities, iResolved, iUnresolved, iFailed, (unsigned)(GetTickCount() - s2sT0)).c_str(), true, false);

	// ===== PASS 2 -- MAP: registry complete -> each entity loads itself (mapFrom resolves its FKs against the FULL id
	// space). Complex traits collide on the engine id with the simple set -> their OWN repo (`\complex\` path is the
	// discriminator). The 0-UNCLASSIFIED census rides here. (The XML shadow-diff is GONE -- the legacy poco is archived;
	// readJson no longer proves against it.)
	for (size_t s = 0; s < store.size(); ++s)
	{
		RjEntity& rec = store[s];
		if (!rec.value.is<picojson::object>()) continue;
		const picojson::object& o = rec.value.get<picojson::object>();
		const bool bComplexTrait = rec.typeId >= 0 && rj_starts(rec.type, "TRAIT_") && rec.path.find("\\complex\\") != std::string::npos;
		const bool bStartNode = (rec.type == "TECH_GAME_START");
		CvInfo* data = bStartNode ? &cascadeStartNode()
			: bComplexTrait ? InfoRepo<CvComplexTraitTag>::get().editPtr(rec.typeId)
			: rj_jsonEdit(rec.type, rec.typeId);
		rec.data = data;
		if (data != NULL)
		{
			// THE FULL-REGISTRY LINK RE-REGISTRATION (owner constraint: FK links register AFTER all JSONs are
			// loaded). An ALIASED poco (rj_jsonEdit -> GC.m_pa<X>Info) was mapFrom'd by its category's loader
			// MID-registry -- any FK naming a later-loading category silently dropped (the cross-category drop
			// defect, readjson.md: section edges AND subclass typed members alike). The registry is complete HERE,
			// so the FULL virtual mapFrom re-runs -- mapFrom is IDEMPOTENT BY CONTRACT (CvInfo.h: sections clear via
			// clearSections, subclass typed containers clear at their parse top), so every link (composed sections +
			// typed FK members) resolves against the full id space, no mismatch, no double-accumulation. The OWNED
			// objects -- the synthetic start node (cascadeStartNode) and the complex-trait set (CvComplexTraitTag),
			// both matching an aliased type PREFIX (TECH_/TRAIT_) without being the aliased GC objects -- get their
			// FIRST (and only) map here, on the same call.
			const bool bAliased = !bStartNode && !bComplexTrait && rj_isAliased(rec.type);
			data->mapFrom(rec.value);
			if (bAliased)
			{
				++iRemapped;
				eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_READJSON, RJE_REMAPPED, 1)
					.addStr(RJF_TYPE, rec.type.c_str())
					.addI(RJF_EDGES, data->getEdges() ? data->getEdges()->count() : 0)
					.addI(RJF_DEPOSITS, data->getModifiers() ? (int)data->getModifiers()->all().size() : 0));
			}
			DepositIndex::pushInfo(data);   // the compiled deposit index PUSH: the info's §6 families (+ whenObsolete)
			                                // intern + compile HERE, at readJson push-time (modifier-substrate.md)
		}
		for (picojson::object::const_iterator it = o.begin(); it != o.end(); ++it)
		{
			++topKeys[it->first];
			const JsonKeyClass c = jsonClassifyKey(it->first, it->second.is<picojson::object>());
			keyClass[it->first] = c;
			if (c == CJK_FAMILY) familyKinds.insert(it->first);
			else if (c == CJK_FLAG) flagKinds.insert(it->first);
		}
	}
	gDLL->logMsg("Loading.log", CvString::format("[READJSON] PASS2-map (mapFrom+DepositIndex) remapped=%d ms=%u", iRemapped, (unsigned)(GetTickCount() - s2sT0)).c_str(), true, false);

	// ===== the §8/§9 CLASSIFICATION registries -- generated infos (SKILL_/TAG_/ATTRIBUTE_/CAPABILITY_/POLICY_)
	// minted from the union of authored block keys (append-only ids, stable across both load passes), then every
	// entity's blocks resolved to the by-id bitsets the O(1) getter surface reads (ClassificationRegistry).
	{
		std::vector<CvInfo*> mapped;
		mapped.reserve(store.size());
		for (size_t s = 0; s < store.size(); ++s)
			if (store[s].data != NULL) mapped.push_back(store[s].data);
		ClassificationRegistry::buildAndResolve(mapped);
		gDLL->logMsg("Loading.log", CvString::format("[READJSON] classification minted skills=%d tags=%d attributes=%d capabilities=%d policies=%d ms=%u",
			ClassificationRegistry::count(CLSD_SKILL), ClassificationRegistry::count(CLSD_TAG),
			ClassificationRegistry::count(CLSD_ATTRIBUTE), ClassificationRegistry::count(CLSD_CAPABILITY),
			ClassificationRegistry::count(CLSD_POLICY), (unsigned)(GetTickCount() - s2sT0)).c_str(), true, false);
	}
	// The initial JSON map is DONE -- the spine announcement (the owner's load-lifecycle observability):
	eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_READJSON, RJE_MAP_DONE, 1)
		.addI(RJF_ENTITIES, iEntities).addI(RJF_RESOLVED, iResolved).addI(RJF_REMAPPED, iRemapped)
		.addI(RJF_MS, (int)(GetTickCount() - s2sT0)));

	// STASH the probe stats for post-load re-emission (the load-time burst is dark: gPlayerLogLevel is 0 here, so
	// the log consumer drops these lines -- the [MODIFIER/repo] census re-emits them per turn where logging is live).
	int iStashFiles = (int)files.size(), iStashEntities = iEntities;
	cascadeReadJsonStats(true, iStashFiles, iStashEntities, dataDir);

	eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_READJSON, RJE_DIR, 1).addStr(RJF_DIR, dataDir.c_str()));
	eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_READJSON, RJE_PROBE, 1)
		.addI(RJF_FILES, (int)files.size()).addI(RJF_PARSED, (int)files.size() - iFailed).addI(RJF_FAILED, iFailed)
		.addI(RJF_ENTITIES, iEntities).addI(RJF_RESOLVED, iResolved).addI(RJF_UNRESOLVED, iUnresolved)
		.addI(RJF_FAMILYKINDS, (int)familyKinds.size()).addI(RJF_FLAGKINDS, (int)flagKinds.size()));

	// FK diagnostics (Orwell bar): every distinct unresolved REFERENCED id (edges/grants/atoms/dormant) collected by
	// jsonResolveId during the maps -- surfaced so a data typo never hides.
	const std::set<std::string>& unres = jsonUnresolvedIds();
	for (std::set<std::string>::const_iterator it = unres.begin(); it != unres.end(); ++it)
		eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_READJSON, RJE_EDGE_UNRES, 1).addStr(RJF_ID, it->c_str()));

	// READ-BACK survey: reconstruct the modifier stats + per-entity structure counts from the MAPPED data (the
	// home) -- the §6 families on getModifiers(), the spec model ([DEC-json-not-cascade]; the retired generic
	// vector is gone) -- proving the map round-trips (values ×100'd, requires/edges/allowed/grants populated).
	int iAttached = 0, iMapSample = 0, iModSample = 0;
	int mMag = 0, mFlat = 0, mPercent = 0, mMult = 0, mOther = 0, mCond = 0, mPer = 0;
	for (size_t s = 0; s < store.size(); ++s)
	{
		const CvInfo* cd = store[s].data;
		if (cd == NULL) continue;
		++iAttached;
		const CvJsonModifiers* mods = cd->getModifiers();
		if (mods != NULL)
		{
			const std::map<std::string, CvJsonModFamily*>& fams = mods->all();
			for (std::map<std::string, CvJsonModFamily*>::const_iterator fit = fams.begin(); fit != fams.end(); ++fit)
			{
				if (fit->second == NULL) continue;
				const std::vector<CvJsonModEntry*>& entries = fit->second->entries;
				for (size_t e = 0; e < entries.size(); ++e)
				{
					const CvJsonModEntry* en = entries[e];
					if (en == NULL) continue;
					++mMag;
					if (en->unit == CASC_UNIT_FLAT) ++mFlat; else if (en->unit == CASC_UNIT_PERCENT) ++mPercent;
					else if (en->unit == CASC_UNIT_MULTIPLIER) ++mMult; else ++mOther;
					if (en->enabled != NULL || en->disabled != NULL) ++mCond;
					if (en->hasPer) ++mPer;   // the §3.7 per count-scaler (represented since 2026-07-08)
					if (iModSample < 10)   // concrete value samples -- proves the single human->×100 conversion at the leaf
					{
						eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_READJSON, RJE_MOD, 1)
							.addStr(RJF_TYPE, store[s].type.c_str()).addStr(RJF_ADDR, fit->first.c_str())
							.addStr(RJF_UNIT, DepositIndex::unitSegment(en->unit)).addI(RJF_VAL, en->value100));
						++iModSample;
					}
				}
			}
		}
		if (iMapSample < 8)
		{
			eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_READJSON, RJE_MAP, 1)
				.addStr(RJF_TYPE, store[s].type.c_str()).addI(RJF_DEPOSITS, cd->getModifiers() ? (int)cd->getModifiers()->all().size() : 0)
				.addI(RJF_REQBUILD, cd->requiresBuild() ? 1 : 0).addI(RJF_REQOPERATE, cd->requiresOperate() ? 1 : 0)
				.addI(RJF_EDGES, cd->getEdges() ? cd->getEdges()->count() : 0)
				.addI(RJF_ALLOWED, cd->getAllowed() ? (int)cd->getAllowed()->all().size() : 0)
				.addI(RJF_GRANTLISTS, cd->getGrants() ? (int)cd->getGrants()->lists().size() : 0)
				.addI(RJF_GRANTPULSES, cd->getGrants() ? cd->getGrants()->pulseCount() : 0));
			++iMapSample;
		}
	}
	eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_READJSON, RJE_MOD_SURVEY, 1)
		.addI(RJF_MAGNITUDES, mMag).addI(RJF_FLAT, mFlat).addI(RJF_PERCENT, mPercent).addI(RJF_MULT, mMult)
		.addI(RJF_OTHER, mOther).addI(RJF_CONDITIONED, mCond).addI(RJF_PERSCALED, mPer).addI(RJF_FAMILYKINDS, (int)familyKinds.size()));

	// §8 capabilities read-back survey (now on CvTechInfo -- techs are the only grantor). Verifies the block maps.
	int capEntities = 0, capGrants = 0;
	std::set<std::string> capNames;
	for (size_t s = 0; s < store.size(); ++s)
	{
		if (store[s].data == NULL || !rj_starts(store[s].type, "TECH_")) continue;
		const CvTechInfo* tech = static_cast<const CvTechInfo*>(store[s].data);
		const CvJsonBoolBlock* caps = tech->getCapabilities();
		if (caps == NULL || caps->isEmpty()) continue;
		++capEntities;
		for (std::set<std::string>::const_iterator it = caps->all().begin(); it != caps->all().end(); ++it)
		{ ++capGrants; capNames.insert(*it); }
	}
	eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_READJSON, RJE_CAP_SURVEY, 1)
		.addI(RJF_GRANTING, capEntities).addI(RJF_CAPGRANTS, capGrants).addI(RJF_DISTINCTNAMES, (int)capNames.size()));
	for (std::set<std::string>::const_iterator it = capNames.begin(); it != capNames.end(); ++it)
		eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_READJSON, RJE_CAP, 1).addStr(RJF_NAME, it->c_str()));

	// FULL-COVERAGE census line: every top-level key kind + its class -- UNCLASSIFIED (impossible: classify always
	// returns family/flag for an unknown) is the thing to investigate.
	for (std::map<std::string, int>::const_iterator it = topKeys.begin(); it != topKeys.end(); ++it)
		eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_READJSON, RJE_KEY, 1)
			.addStr(RJF_KEY, it->first.c_str()).addI(RJF_COUNT, it->second)
			.addStr(RJF_CLASS, jsonKeyClassName(keyClass[it->first])));

	// Route<-bonus prereq REVERSE INDEX. curate_route.py inverts a route's PrereqOrBonuses to the bonus's
	// `enables.routes` (the enabler GENERATE edge), so no route JSON carries the relationship. The getRouteInfo(...)
	// callers (CvPlot route validity, CvPlayerAI, CvDLLWidgetData) still ask the route "which bonuses do I need?",
	// so reconstruct each route's OR-list here, once, after every entity is mapped. (getPrereqBonus -- the legacy
	// single AND-prereq -- is authored by NO route, so it stays a NO_BONUS constant on the poco.)
	{
		const int nBonus = GC.getNumBonusInfos();
		for (int b = 0; b < nBonus; ++b)
		{
			const CvInfo* jb = InfoRepo<CvBonusInfo>::get().get(b);
			if (jb == NULL || jb->getEdges() == NULL) continue;
			const std::vector<int>* routes = jb->getEdges()->find(EDGEF_ENABLES, EDGEB_ROUTES);
			if (routes != NULL)
				for (size_t r = 0; r < routes->size(); ++r)
					static_cast<CvRouteInfo*>(InfoRepo<CvRouteInfo>::get().editPtr((*routes)[r]))->addPrereqOrBonus((BonusTypes)b);
			// the single AND-prereq bonus rides a DISTINCT bucket (store.py routesAnd) so it stays out of the OR-list --
			// getPrereqBonus (the CvPlot build gate), not getPrereqOrBonuses. A route has at most one single AND bonus.
			const std::vector<int>* routesAnd = jb->getEdges()->find(EDGEF_ENABLES, EDGEB_ROUTES_AND);
			if (routesAnd != NULL)
				for (size_t r = 0; r < routesAnd->size(); ++r)
					static_cast<CvRouteInfo*>(InfoRepo<CvRouteInfo>::get().editPtr((*routesAnd)[r]))->setPrereqBonus((BonusTypes)b);
		}
	}

	// Improvement<-route YIELD REVERSE INDEX. RouteYieldChanges live ROUTE-side (deliveryguy, modifier.md §4:
	// "a route upgrading improvements -> on the route, keyed by improvement" -- curate_route.py authors
	// {food|production|commerce}.plot.improvements.{IMP}.flat), but the legacy improvement-side readers
	// (CvPlot::calculateImprovementYieldChange:8354/8370/12357 + the CvDLLWidgetData help) still ask the
	// IMPROVEMENT "what does route R add on me?" -- a stub 0 under-yielded every improved+routed plot and fed
	// the AI wrong tile values. Reconstruct each improvement's route rows here, once, after every entity is
	// mapped (flat unconditioned entries only -- the data authors nothing else on this address).
	{
		static const struct { const char* szFam; int iYield; } YFAMS[] = {
			{ "food.plot.improvements.",       YIELD_FOOD },
			{ "production.plot.improvements.", YIELD_PRODUCTION },
			{ "commerce.plot.improvements.",   YIELD_COMMERCE } };
		const int nRoute = GC.getNumRouteInfos();
		for (int r = 0; r < nRoute; ++r)
		{
			const CvInfo* jr = InfoRepo<CvRouteInfo>::get().get(r);
			if (jr == NULL || jr->getModifiers() == NULL) continue;
			const std::map<std::string, CvJsonModFamily*>& fams = jr->getModifiers()->all();
			for (std::map<std::string, CvJsonModFamily*>::const_iterator it = fams.begin(); it != fams.end(); ++it)
			{
				for (int f = 0; f < 3; ++f)
				{
					const size_t iLen = strlen(YFAMS[f].szFam);
					if (it->first.compare(0, iLen, YFAMS[f].szFam) != 0) continue;
					const int iImp = jsonResolveId(it->first.substr(iLen));
					if (iImp < 0) break;
					const CvJsonModFamily* fam = it->second;
					for (int e = 0; e < fam->size(); ++e)
					{
						const CvJsonModEntry* en = fam->entries[e];
						if (en->unit != CASC_UNIT_FLAT || en->hasPer || en->enabled != NULL || en->disabled != NULL) continue;
						static_cast<CvImprovementInfo*>(InfoRepo<CvImprovementInfo>::get().editPtr(iImp))
							->addRouteYieldChange(r, YFAMS[f].iYield, en->value100 / 100);
					}
					break;
				}
			}
		}
	}

	// STORE-INVERTED TECH-FK REVERSE INDEX -- the Route<-bonus pattern above, generalized. curate_*.py DROP each
	// entity's tech prereq/obsolete FK and store-invert it onto the TECH's enables/obsoletes buckets (bonus reveal +
	// cityTrade both -> enables.bonuses, deliberately merged/indistinguishable; corp/project/religion/process/promotion
	// -> enables.<bucket>; bonus/build/corp/promotion obsolete -> obsoletes.<bucket>). The getXInfo(...) compat getters
	// still ask "which tech do I need?" -- a REVERSE lookup -- so reconstruct each target's FK here, once, after every
	// entity is mapped. Forward reads only on the hot path (enabler.md §2); this cold reverse view is derived once at
	// load (modifier.md §1). getProjectsNeeded is the project<-project variant (off the prereq project's enables.projects).
	{
		const int nTech = GC.getNumTechInfos();
		for (int t = 0; t < nTech; ++t)
		{
			const CvInfo* jt = InfoRepo<CvTechInfo>::get().get(t);
			if (jt == NULL || jt->getEdges() == NULL) continue;
			const CvJsonEdges* e = jt->getEdges();
			const TechTypes eTech = (TechTypes)t;
			if (const std::vector<int>* v = e->find(EDGEF_ENABLES, EDGEB_BONUSES))
				for (size_t k = 0; k < v->size(); ++k)
				{
					CvBonusInfo* p = static_cast<CvBonusInfo*>(InfoRepo<CvBonusInfo>::get().editPtr((*v)[k]));
					if (p->getTechReveal() == NO_TECH) { p->setTechReveal(eTech); p->setTechCityTrade(eTech); }   // first (lowest-id) tech wins -- merged/indistinguishable
				}
			if (const std::vector<int>* v = e->find(EDGEF_OBSOLETES, EDGEB_BONUSES))
				for (size_t k = 0; k < v->size(); ++k)
					static_cast<CvBonusInfo*>(InfoRepo<CvBonusInfo>::get().editPtr((*v)[k]))->setTechObsolete(eTech);
			if (const std::vector<int>* v = e->find(EDGEF_OBSOLETES, EDGEB_BUILDS))
				for (size_t k = 0; k < v->size(); ++k)
					static_cast<CvBuildInfo*>(InfoRepo<CvBuildInfo>::get().editPtr((*v)[k]))->setObsoleteTech(eTech);
			if (const std::vector<int>* v = e->find(EDGEF_ENABLES, EDGEB_PROJECTS))
				for (size_t k = 0; k < v->size(); ++k)
					static_cast<CvProjectInfo*>(InfoRepo<CvProjectInfo>::get().editPtr((*v)[k]))->setTechPrereq(eTech);
			if (const std::vector<int>* v = e->find(EDGEF_ENABLES, EDGEB_CORPORATIONS))
				for (size_t k = 0; k < v->size(); ++k)
					static_cast<CvCorporationInfo*>(InfoRepo<CvCorporationInfo>::get().editPtr((*v)[k]))->setTechPrereq(eTech);
			if (const std::vector<int>* v = e->find(EDGEF_OBSOLETES, EDGEB_CORPORATIONS))
				for (size_t k = 0; k < v->size(); ++k)
					static_cast<CvCorporationInfo*>(InfoRepo<CvCorporationInfo>::get().editPtr((*v)[k]))->setObsoleteTech(eTech);
			if (const std::vector<int>* v = e->find(EDGEF_ENABLES, EDGEB_RELIGIONS))
				for (size_t k = 0; k < v->size(); ++k)
					static_cast<CvReligionInfo*>(InfoRepo<CvReligionInfo>::get().editPtr((*v)[k]))->setTechPrereq(eTech);
			if (const std::vector<int>* v = e->find(EDGEF_ENABLES, EDGEB_PROCESSES))
				for (size_t k = 0; k < v->size(); ++k)
					static_cast<CvProcessInfo*>(InfoRepo<CvProcessInfo>::get().editPtr((*v)[k]))->setTechPrereq(eTech);
			if (const std::vector<int>* v = e->find(EDGEF_ENABLES, EDGEB_PROMOTIONS))
				for (size_t k = 0; k < v->size(); ++k)
					static_cast<CvPromotionInfo*>(InfoRepo<CvPromotionInfo>::get().editPtr((*v)[k]))->setTechPrereq(eTech);
			if (const std::vector<int>* v = e->find(EDGEF_OBSOLETES, EDGEB_PROMOTIONS))
				for (size_t k = 0; k < v->size(); ++k)
					static_cast<CvPromotionInfo*>(InfoRepo<CvPromotionInfo>::get().editPtr((*v)[k]))->setObsoleteTech(eTech);
			if (const std::vector<int>* v = e->find(EDGEF_ENABLES, EDGEB_PROMOTION_LINES))
				for (size_t k = 0; k < v->size(); ++k)
					static_cast<CvPromotionLineInfo*>(InfoRepo<CvPromotionLineInfo>::get().editPtr((*v)[k]))->setTechPrereq(eTech);
			if (const std::vector<int>* v = e->find(EDGEF_OBSOLETES, EDGEB_PROMOTION_LINES))
				for (size_t k = 0; k < v->size(); ++k)
					static_cast<CvPromotionLineInfo*>(InfoRepo<CvPromotionLineInfo>::get().editPtr((*v)[k]))->setObsoleteTech(eTech);
		}
		// project <- project: PrereqProjects store-inverted onto the prerequisite project's enables.projects.
		const int nProj = GC.getNumProjectInfos();
		for (int pr = 0; pr < nProj; ++pr)
		{
			const CvInfo* jp = InfoRepo<CvProjectInfo>::get().get(pr);
			if (jp == NULL || jp->getEdges() == NULL) continue;
			if (const std::vector<int>* v = jp->getEdges()->find(EDGEF_ENABLES, EDGEB_PROJECTS))
				for (size_t k = 0; k < v->size(); ++k)
					static_cast<CvProjectInfo*>(InfoRepo<CvProjectInfo>::get().editPtr((*v)[k]))->addProjectNeeded(pr);
		}
	}

	// THE REVERSE VIEW -- inverted onto the referenced infos' own edges, so every consumer reads its info
	// directly (the pedia/tooltip candidate lists; modifier.md par.1). Runs after every FK/compat view above
	// so it inverts the final reconstructed getters.
	rj_buildReverseView();
	// The reverse-view build is DONE -- the spine announcement (counts pre-dedup: the raw inversion volume).
	eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_READJSON, RJE_REVERSE_DONE, 1)
		.addI(RJF_RELATED, s_rvRelated).addI(RJF_REQUIREDBY, s_rvRequiredBy).addI(RJF_MS, (int)s_rvMs));
	gDLL->logMsg("Loading.log", CvString::format("[READJSON] reverse-view related=%d requiredBy=%d ms=%u",
		s_rvRelated, s_rvRequiredBy, (unsigned)s_rvMs).c_str(), true, false);

	eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_READJSON, RJE_MAP_SUMMARY, 1).addI(RJF_WITHDATA, iAttached));
	gDLL->logMsg("Loading.log", CvString::format("[READJSON] END withData=%d reverseIndex+survey done totalMs=%u", iAttached, (unsigned)(GetTickCount() - s2sT0)).c_str(), true, false);
}
