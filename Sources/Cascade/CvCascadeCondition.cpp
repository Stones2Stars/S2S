//
//	CvCascadeCondition -- the NEW uniform condition/count/predicate evaluator (see CvCascadeCondition.h).
//	#430 shadow surface: builds the verdict from the JSON `requires` tree, reading ground-truth STATE primitives
//	at the leaves (the tally for cross-city building/unit counts; direct city/plot/team/player reads otherwise).
//	It NEVER calls a legacy gate (canConstruct/...) -- those are only the shadow's comparison oracle.
//
//	Reuse note (owner ruling -- adversarial about existing impls): every leaf primitive here is a rock-solid
//	atomic state accessor (hasBuilding / isRiver / isHasTech / isCivic / getPopulation / ...). The ONE flagged
//	primitive is BONUS connection -- hasBonus / hasVicinityBonus -- which the spec calls fuzzy/suspect; reused
//	here for the shadow, a candidate for the cascade's own trade-network model later.
//

#include "CvGameCoreDLL.h"
#include "CvCascadeCondition.h"
#include "CvGlobals.h"
#include "CvMap.h"
#include "CvInfos.h"      // CvWorldInfo::getOceanMinAreaSize
#include "CvBuildingInfo.h" // getSpecialBuilding (special-building-group waiver on a prereq building)
#include "CvPlayerAI.h"   // GET_PLAYER
#include "CvTeamAI.h"     // GET_TEAM
#include "CvCity.h"
#include "CvPlot.h"
#include "CvProperties.h" // getValueByProperty (the PROPERTY band atom)

namespace
{
	// The context's CvCity (for city/plot-scope atoms + predicates), or NULL when no specific city is set.
	const CvCity* ccCtxCity(const CvCascadeContext& kCtx)
	{
		if (kCtx.iPlayer < 0 || kCtx.iPlayer >= MAX_PLAYERS || kCtx.iCity < 0)
		{
			return NULL;
		}
		return GET_PLAYER((PlayerTypes)kCtx.iPlayer).getCity(kCtx.iCity);
	}

	// VICINITY (enabler-spec §8): does ANY plot in the city's CURRENT workable radius (culture-grown, getNumCityPlots)
	// satisfy the plot predicate? No ownership/worked filter -- the broadest clean barrier, deliberately more permissive
	// than legacy's owned (isValidTerrainForBuildings) / worked (GOM_TERRAIN/FEATURE) scans, which it subsumes.
	bool ccVicinityMatch(const CvCity* c, const CvPredicate& pr)
	{
		if (c == NULL) return false;
		const int iNumPlots = c->getNumCityPlots();
		for (int iI = 0; iI < iNumPlots; iI++)
		{
			const CvPlot* plotX = c->getCityIndexPlot(iI);
			if (plotX == NULL) continue;
			switch (pr.eKind)
			{
			case PRED_HAS_TERRAIN:     if (plotX->getTerrainType() == (TerrainTypes)pr.iParam) return true; break;
			case PRED_HAS_FEATURE:     if (plotX->getFeatureType() == (FeatureTypes)pr.iParam) return true; break;
			case PRED_HAS_FEATURE_ANY: if (plotX->getFeatureType() != NO_FEATURE) return true; break;
			case PRED_HAS_IMPROVEMENT: if (plotX->getImprovementType() == (ImprovementTypes)pr.iParam) return true; break;
			default: break;
			}
		}
		return false;
	}
}

int CvCascadeContext::contextFor(CountScope eScope) const
{
	switch (eScope)
	{
	case COUNTSCOPE_EMPIRE: return iPlayer;
	case COUNTSCOPE_TEAM:   return (iPlayer >= 0 && iPlayer < MAX_PLAYERS) ? (int)GET_PLAYER((PlayerTypes)iPlayer).getTeam() : -1;
	case COUNTSCOPE_WORLD:  return -1;
	default:                return -1; // CITY/PLOT are direct reads, not tally
	}
}

int cascadeAtomCount(const CvCountAtom& a, const CvCascadeContext& kCtx)
{
	const int p = kCtx.iPlayer;
	switch (a.eDomain)
	{
	case ATOMDOMAIN_BUILDING:
		if (a.eScope == COUNTSCOPE_CITY || a.eScope == COUNTSCOPE_PLOT)
		{
			const CvCity* c = ccCtxCity(kCtx);
			if (c == NULL) return 0;
			if (c->hasBuilding((BuildingTypes)a.iType)) return 1;
			// A prereq building is ALSO satisfied when you don't NEED it: its special-building GROUP is waived
			// (a civic -- State Church / Free Church waive SPECIALBUILDING_MONASTERY, so missionaries need no
			// monastery) or it is OBSOLETE (teched past). Legacy isPlotTrainable (CvCity.cpp:1933-1941) waives on
			// exactly these two. Engine-side resolution of "what satisfies a building requirement" -- the same
			// pattern as the BONUS connection logic below; the DATA just names the building, no per-unit OR.
			const SpecialBuildingTypes eSB =
				(SpecialBuildingTypes)GC.getBuildingInfo((BuildingTypes)a.iType).getSpecialBuilding();
			if (eSB != NO_SPECIALBUILDING && p >= 0 && GET_PLAYER((PlayerTypes)p).isSpecialBuildingNotRequired(eSB))
			{
				return 1;
			}
			if (GET_TEAM(c->getTeam()).isObsoleteBuilding((BuildingTypes)a.iType)) return 1;
			return 0;
		}
		return cascadeTally().count(COUNTDOMAIN_BUILDING, a.iType, a.eScope, kCtx.contextFor(a.eScope));

	case ATOMDOMAIN_UNIT:
		return cascadeTally().count(COUNTDOMAIN_UNIT, a.iType, a.eScope, kCtx.contextFor(a.eScope));

	case ATOMDOMAIN_TECH:
	{
		if (p < 0) return 0;
		const TeamTypes eTeam = GET_PLAYER((PlayerTypes)p).getTeam();
		return GET_TEAM(eTeam).isHasTech((TechTypes)a.iType) ? 1 : 0; // tech presence is team-scope
	}

	case ATOMDOMAIN_BONUS:
	{
		const CvCity* c = ccCtxCity(kCtx);
		if (c == NULL) return 0; // bonus presence is city-relative; no city -> can't answer (pending)
		bool bHas;
		if (a.eConn == CONN_VICINITY)   bHas = c->hasVicinityBonus((BonusTypes)a.iType);
		else if (a.eConn == CONN_TRADE) bHas = c->hasBonus((BonusTypes)a.iType);
		else                            bHas = c->hasBonus((BonusTypes)a.iType) || c->hasVicinityBonus((BonusTypes)a.iType);
		if (!bHas) return 0;
		const int n = c->getNumBonuses((BonusTypes)a.iType);
		return n > 0 ? n : 1; // volumetric where the count is available, else presence
	}

	case ATOMDOMAIN_CIVIC:
		return (p >= 0 && GET_PLAYER((PlayerTypes)p).isCivic((CivicTypes)a.iType)) ? 1 : 0;

	case ATOMDOMAIN_HERITAGE: // per-player acquired heritage (folklore/myth/taxon tiers); empire scope
		return (p >= 0 && GET_PLAYER((PlayerTypes)p).hasHeritage((HeritageTypes)a.iType)) ? 1 : 0;

	case ATOMDOMAIN_RELIGION:
	{
		const CvCity* c = ccCtxCity(kCtx);
		return (c != NULL && c->isHasReligion((ReligionTypes)a.iType)) ? 1 : 0;
	}

	case ATOMDOMAIN_CORPORATION:
	{
		const CvCity* c = ccCtxCity(kCtx);
		return (c != NULL && c->isHasCorporation((CorporationTypes)a.iType)) ? 1 : 0;
	}

	case ATOMDOMAIN_POPULATION:
	{
		const CvCity* c = ccCtxCity(kCtx);
		return c != NULL ? c->getPopulation() : 0;
	}

	case ATOMDOMAIN_CITYCOUNT:
		if (p < 0) return 0;
		if (a.eScope == COUNTSCOPE_TEAM) return GET_TEAM(GET_PLAYER((PlayerTypes)p).getTeam()).getNumCities();
		return GET_PLAYER((PlayerTypes)p).getNumCities();

	case ATOMDOMAIN_AREASIZE: // AREA_SIZE = the context city's LANDMASS tile count (land buildings; water uses IS_COASTAL)
	{
		const CvCity* c = ccCtxCity(kCtx);
		return (c != NULL && c->area() != NULL) ? c->area()->getNumTiles() : 0;
	}

	case ATOMDOMAIN_PROPERTY: // the context city's current value for property iType (the band's count; min/max gate it)
	{
		const CvCity* c = ccCtxCity(kCtx);
		return (c != NULL) ? c->getPropertiesConst()->getValueByProperty((PropertyTypes)a.iType) : 0;
	}

	default:
		return 0;
	}
}

bool cascadeEvalAtom(const CvCountAtom& a, const CvCascadeContext& kCtx)
{
	const int iCount = cascadeAtomCount(a, kCtx);
	if (iCount < a.iMin) return false;
	if (a.iMax >= 0 && iCount > a.iMax) return false;
	return true;
}

bool cascadeEvalPredicate(const CvPredicate& pr, const CvCascadeContext& kCtx)
{
	const CvCity* c = ccCtxCity(kCtx);
	const CvPlot* pl = (c != NULL) ? c->plot() : NULL;
	switch (pr.eKind)
	{
	case PRED_IS_CAPITAL:         return c != NULL && c->isCapital();
	case PRED_HAS_POWER:          return c != NULL && c->isPower();
	case PRED_HAS_STATE_RELIGION: return kCtx.iPlayer >= 0 && GET_PLAYER((PlayerTypes)kCtx.iPlayer).getStateReligion() != NO_RELIGION;
	case PRED_STATE_RELIGION_IN_CITY:
	{
		// Legacy needStateReligionInCity (CvCity.cpp:2594): the player's state religion is present in THIS city.
		if (c == NULL || kCtx.iPlayer < 0) return false;
		const ReligionTypes eSR = GET_PLAYER((PlayerTypes)kCtx.iPlayer).getStateReligion();
		return eSR != NO_RELIGION && c->isHasReligion(eSR);
	}
	case PRED_IS_COASTAL:         return c != NULL && c->isCoastal(GC.getWorldInfo(GC.getMap().getWorldSize()).getOceanMinAreaSize());
	case PRED_HAS_RIVER:          return pl != NULL && pl->isRiver();
	case PRED_IS_WATER:           return pl != NULL && pl->isWater();
	case PRED_IS_HILLS:           return pl != NULL && pl->isHills();
	case PRED_IS_PEAK:            return pl != NULL && pl->isPeak();
	case PRED_IS_FLATLANDS:       return pl != NULL && pl->isFlatlands();
	case PRED_IS_FRESHWATER:      return pl != NULL && pl->isFreshWater();
	case PRED_HAS_IRRIGATION:     return pl != NULL && pl->isIrrigated();
	case PRED_HAS_FEATURE_ANY:    return ccVicinityMatch(c, pr);  // VICINITY: any feature in the workable radius
	case PRED_HAS_FEATURE:        return ccVicinityMatch(c, pr);  // VICINITY: this feature in the workable radius
	case PRED_HAS_IMPROVEMENT:    return ccVicinityMatch(c, pr);  // VICINITY: this improvement in the workable radius
	case PRED_HAS_TERRAIN:
	{
		// VICINITY (enabler-spec §8): terrain present anywhere in the city's CURRENT workable radius (culture-grown,
		// getNumCityPlots). Per-city, overlapping, NO ownership/canWork filter -- deliberately more permissive than
		// legacy isValidTerrainForBuildings (which excludes contested plots). Building PrereqOrTerrain is vicinity;
		// improvement center-plot terrain is the flagged Phase-F divergence (migration-renames), not a consumer yet.
		return ccVicinityMatch(c, pr);
	}
	case PRED_HAS_BONUS:          return pl != NULL && pl->getBonusType() == (BonusTypes)pr.iParam;
	case PRED_LATITUDE:           return pl != NULL && pl->getLatitude() >= pr.iMin && pl->getLatitude() <= pr.iMax;
	// CENTER-plot map-category (legacy isMapCategory, CvGameCoreUtils.h:382): an UNCATEGORIZED plot is always valid.
	case PRED_HAS_MAP_CATEGORY:   return pl != NULL && (pl->getMapCategories().empty() || pl->isMapCategoryType((MapCategoryTypes)pr.iParam));
	case PRED_HAS_RELIGION:       return c != NULL && c->isHasReligion((ReligionTypes)pr.iParam);
	case PRED_STATE_RELIGION:     return kCtx.iPlayer >= 0 && GET_PLAYER((PlayerTypes)kCtx.iPlayer).getStateReligion() == (ReligionTypes)pr.iParam;
	case PRED_HOLY_CITY:          return c != NULL && c->isHolyCity((ReligionTypes)pr.iParam);
	case PRED_HAS_CORPORATION:    return c != NULL && c->isHasCorporation((CorporationTypes)pr.iParam);
	default:                      return true; // unknown -> ignored (dropped at parse; defensive)
	}
}

bool cascadeEvalLeaf(const CvCascadeLeaf& kLeaf, const CvCascadeContext& kCtx)
{
	return kLeaf.bPredicate ? cascadeEvalPredicate(kLeaf.pred, kCtx) : cascadeEvalAtom(kLeaf.atom, kCtx);
}

bool cascadeEvalCondition(const CvCascadeCondition& kCond, const CvCascadeContext& kCtx)
{
	// all: every leaf must hold (AND)
	for (std::vector<CvCascadeLeaf>::const_iterator it = kCond.all.begin(); it != kCond.all.end(); ++it)
	{
		if (!cascadeEvalLeaf(*it, kCtx)) return false;
	}
	// any: each OR-group must contain at least one true leaf (AND of ORs)
	for (std::vector<std::vector<CvCascadeLeaf> >::const_iterator g = kCond.any.begin(); g != kCond.any.end(); ++g)
	{
		bool bGroupHolds = false;
		for (std::vector<CvCascadeLeaf>::const_iterator it = g->begin(); it != g->end(); ++it)
		{
			if (cascadeEvalLeaf(*it, kCtx)) { bGroupHolds = true; break; }
		}
		if (!bGroupHolds) return false;
	}
	// noneOf: none may hold
	for (std::vector<CvCascadeLeaf>::const_iterator it = kCond.noneOf.begin(); it != kCond.noneOf.end(); ++it)
	{
		if (cascadeEvalLeaf(*it, kCtx)) return false;
	}
	return true;
}

int cascadePerValue(CountDomain eDomain, int iType, CountScope eScope, int iEach, const CvCascadeContext& kCtx)
{
	const int iCount = cascadeTally().count(eDomain, iType, eScope, kCtx.contextFor(eScope));
	return iCount / (iEach > 0 ? iEach : 1);
}

bool cascadeWithinAllowed(CountDomain eDomain, int iSelfType, CountScope eScope, int iCap, const CvCascadeContext& kCtx)
{
	if (iCap < 0) return true; // uncapped
	// countForCap, not count: a UNIT world-cap reads LIFETIME-CREATED (a born-then-poof hero keeps its slot),
	// while buildings/other scopes use the live count -- the tally owns that distinction in one place.
	const int iCount = cascadeTally().countForCap(eDomain, iSelfType, eScope, kCtx.contextFor(eScope));
	return iCount < iCap;
}
