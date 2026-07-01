//
//	CvCascadeConditionEval -- the PORT of StoneBase CascadingEnabler/ConditionEvaluator.cs (see the header). The walk
//	+ every predicate's semantics is a faithful transcription of the parity-proven C#; only the state reads differ
//	(the live engine here, EvalState/PlotContext there). Section headers below name the C# method ported.
//

#include "CvGameCoreDLL.h"
#include "CvCascadeConditionEval.h"
#include "CvCascadeTally.h"
#include "AI/CvPlayerAI.h"          // GET_PLAYER
#include "AI/CvTeamAI.h"           // GET_TEAM
#include "Defines/CvGlobals.h"
#include "Engine/CvCity.h"
#include "Engine/CvPlayer.h"
#include "Engine/CvPlot.h"
#include "Engine/CvTeam.h"
#include "Engine/CvGame.h"
#include "Engine/CvArea.h"
#include "Engine/CvProperties.h"
#include <string>

static bool en_starts(const std::string& s, const char* pfx) { return s.compare(0, strlen(pfx), pfx) == 0; }

// ---- small live-engine helpers (the EvalState set-membership reads) ----------------------------------------------

static bool ev_hasCivic(const CvPlayer* pl, int eCivic)
{
	if (pl == NULL || eCivic < 0) return false;
	for (int iCO = 0; iCO < GC.getNumCivicOptionInfos(); ++iCO)
		if ((int)pl->getCivics((CivicOptionTypes)iCO) == eCivic) return true;
	return false;
}

// A BUILDING prereq must be ACTIVE (engine PrereqInCity/OrBuilding use isActiveBuilding) -- present AND not dormant.
static bool ev_hasActiveBuilding(const CvCity* c, int eBuilding)
{
	return c != NULL && eBuilding >= 0 && c->isActiveBuilding((BuildingTypes)eBuilding);
}

// VICINITY scan helper: walk the city's workable plots and test `pred`. (Mirrors StoneBase PlotHas over s.Plots.)
typedef bool (*EvPlotPred)(const CvPlot*, const CvCascadeCondition*, const CvCity*);
static bool ev_cityPlotHas(const CvCity* c, EvPlotPred pred, const CvCascadeCondition* a)
{
	if (c == NULL) return false;
	for (int i = 0; i < NUM_CITY_PLOTS; ++i)
	{
		const CvPlot* pl = c->getCityIndexPlot(i);
		if (pl != NULL && pred(pl, a, c)) return true;
	}
	return false;
}

// the workable-plot predicates used by Present (terrain/feature/improvement/route prereqs -- OWNED vicinity; a FEATURE
// also accepts a NEUTRAL tile unless EXP_STRICT_VICINITY).
static bool evp_feature(const CvPlot* pl, const CvCascadeCondition* a, const CvCity* c)
{
	if ((int)pl->getFeatureType() != a->id) return false;
	const bool bOwned = pl->getOwner() == c->getOwner();
	const bool bNeutral = pl->getOwner() == NO_PLAYER;
	return bOwned || (bNeutral && !GC.getGame().isOption((GameOptionTypes)GC.getInfoTypeForString("GAMEOPTION_EXP_STRICT_VICINITY")));
}
static bool evp_peak(const CvPlot* pl, const CvCascadeCondition*, const CvCity* c)
{ return pl->getOwner() == c->getOwner() && pl->isPeak(); }
static bool evp_hill(const CvPlot* pl, const CvCascadeCondition*, const CvCity* c)
{ return pl->getOwner() == c->getOwner() && pl->isHills(); }
static bool evp_terrain(const CvPlot* pl, const CvCascadeCondition* a, const CvCity* c)
{ return pl->getOwner() == c->getOwner() && (int)pl->getTerrainType() == a->id; }
static bool evp_improvement(const CvPlot* pl, const CvCascadeCondition* a, const CvCity* c)
{ return pl->getOwner() == c->getOwner() && (int)pl->getImprovementType() == a->id; }
static bool evp_route(const CvPlot* pl, const CvCascadeCondition* a, const CvCity* c)
{ return pl->getOwner() == c->getOwner() && (int)pl->getRouteType() == a->id; }
// worked-tile GOM predicates ({HAS_TERRAIN|FEATURE|IMPROVEMENT:X})
static bool evp_workedTerrain(const CvPlot* pl, const CvCascadeCondition* a, const CvCity*)
{ return pl->isBeingWorked() && (int)pl->getTerrainType() == a->id; }
static bool evp_workedFeatureAny(const CvPlot* pl, const CvCascadeCondition*, const CvCity*)
{ return pl->isBeingWorked() && pl->getFeatureType() != NO_FEATURE; }
static bool evp_workedFeature(const CvPlot* pl, const CvCascadeCondition* a, const CvCity*)
{ return pl->isBeingWorked() && (int)pl->getFeatureType() == a->id; }
static bool evp_workedImprovement(const CvPlot* pl, const CvCascadeCondition* a, const CvCity*)
{ return pl->isBeingWorked() && (int)pl->getImprovementType() == a->id; }

// ---- BonusPresent / VicinityHas (StoneBase) ---------------------------------------------------------------------

static bool ev_vicinityHas(const CvCascadeEvalCtx& ctx, int eBonus, CvCascVicinity disc)
{
	const CvCity* c = ctx.city;
	if (c == NULL) return false;
	// CONNECTED = the engine's OBTAINED-in-vicinity (json §3.4: owned+valid+connected). CvCity::hasVicinityBonus
	// (CvCity.cpp:21353) encodes EXACTLY that -- hasBonus-gated, then centre OR an owned+valid+`isConnectedTo(this)`
	// radius plot OR a building-provided supply -- so defer to it wholesale (StoneBase's per-plot `BonusConnected`
	// scan + VicinityBonuses fallback collapses to this one engine method; `isConnectedToCapital` was the WRONG read).
	if (disc == CASC_VIC_CONNECTED) return c->hasVicinityBonus((BonusTypes)eBonus);
	// The looser discriminators scan the workable radius directly; the centre tile always counts.
	for (int i = 0; i < NUM_CITY_PLOTS; ++i)
	{
		const CvPlot* pl = c->getCityIndexPlot(i);
		if (pl == NULL || (int)pl->getBonusType(c->getTeam()) != eBonus) continue;
		const bool bCenter  = (pl == c->plot());
		const bool bOwned   = pl->getOwner() == c->getOwner();
		const bool bNeutral = pl->getOwner() == NO_PLAYER;
		switch (disc)
		{
		case CASC_VIC_WORKED:    if (bCenter || pl->isBeingWorked()) return true; break;
		case CASC_VIC_OWNED:     if (bCenter || bOwned) return true; break;
		case CASC_VIC_CROSSBORDER: return true;
		default:                 if (bCenter || bOwned || bNeutral) return true; break;   // owned+neutral (the DEFAULT)
		}
	}
	return c->hasVicinityBonus((BonusTypes)eBonus);   // a building-provided in-vicinity supply (provides.bonuses) fallback
}

static bool ev_bonusPresent(const CvCascadeEvalCtx& ctx, int eBonus, CvCascConnection conn, CvCascVicinity vic)
{
	const CvCity* c = ctx.city;
	switch (conn)
	{
	case CASC_CONN_VICINITY:          return ev_vicinityHas(ctx, eBonus, vic);
	case CASC_CONN_TRADE:             return c != NULL && c->hasBonus((BonusTypes)eBonus);
	case CASC_CONN_TRADE_OR_VICINITY: return c != NULL && c->hasBonus((BonusTypes)eBonus);
	default:
		if (ctx.plot != NULL) return (int)ctx.plot->getBonusType(ctx.team ? ctx.team->getID() : NO_TEAM) == eBonus;
		return c != NULL && c->hasBonus((BonusTypes)eBonus);
	}
}

// ---- Present (StoneBase) -- bare presence by type prefix --------------------------------------------------------

static bool ev_present(const CvCascadeEvalCtx& ctx, const CvCascadeCondition* a)
{
	const std::string& t = a->type;
	const int id = a->id;
	if (en_starts(t, "TECH_"))     return ctx.team != NULL && id >= 0 && ctx.team->isHasTech((TechTypes)id);
	if (en_starts(t, "CIVIC_"))    return ev_hasCivic(ctx.player, id);
	if (en_starts(t, "TRAIT_"))    return ctx.player != NULL && id >= 0 && ctx.player->hasTrait((TraitTypes)id);
	if (en_starts(t, "RELIGION_")) return ctx.city != NULL && id >= 0 && ctx.city->isHasReligion((ReligionTypes)id);
	if (en_starts(t, "HERITAGE_")) return ctx.player != NULL && id >= 0 && ctx.player->hasHeritage((HeritageTypes)id);
	if (en_starts(t, "PROJECT_"))  return ctx.team != NULL && id >= 0 && ctx.team->getProjectCount((ProjectTypes)id) > 0;
	if (en_starts(t, "BUILDING_")) return ev_hasActiveBuilding(ctx.city, id);
	if (en_starts(t, "CORPORATION_")) return ctx.city != NULL && id >= 0 && ctx.city->isHasCorporation((CorporationTypes)id);
	if (en_starts(t, "VICTORY_"))  return id >= 0 && GC.getGame().isVictoryValid((VictoryTypes)id);
	if (en_starts(t, "GAMEOPTION_")) return id >= 0 && GC.getGame().isOption((GameOptionTypes)id);
	if (en_starts(t, "BONUS_"))    return ev_bonusPresent(ctx, id, a->connection, a->vicinity);
	if (en_starts(t, "MAPCATEGORY_")) return true;   // map-category gate: not modelled (json §3.5 in-flight) -> ignored
	// plot-substrate vicinity scans (owned, culture-grown radius)
	if (en_starts(t, "FEATURE_"))     return ev_cityPlotHas(ctx.city, evp_feature, a);
	if (t == "TERRAIN_PEAK")          return ev_cityPlotHas(ctx.city, evp_peak, a);
	if (t == "TERRAIN_HILL")          return ev_cityPlotHas(ctx.city, evp_hill, a);
	if (en_starts(t, "TERRAIN_"))     return ev_cityPlotHas(ctx.city, evp_terrain, a);
	if (en_starts(t, "IMPROVEMENT_")) return ev_cityPlotHas(ctx.city, evp_improvement, a);
	if (en_starts(t, "ROUTE_"))       return ev_cityPlotHas(ctx.city, evp_route, a);
	return false;
}

// ---- CountOf (StoneBase) ---------------------------------------------------------------------------------------

static int ev_countOf(const CvCascadeEvalCtx& ctx, const CvCascadeCondition* a)
{
	const std::string& t = a->type;
	if (en_starts(t, "PROPERTY_"))
		return (ctx.city != NULL && a->id >= 0) ? ctx.city->getPropertiesConst()->getValueByProperty((PropertyTypes)a->id) : 0;
	if (t == "POPULATION") return ctx.city != NULL ? ctx.city->getPopulation() : 0;
	if (t == "CITY")       return ctx.player != NULL ? ctx.player->getNumCities() : 0;
	if (t == "TEAM")       return ctx.team != NULL ? ctx.team->getNumMembers() : 0;
	if (t == "AREA_SIZE")  return (ctx.city != NULL && ctx.city->area() != NULL) ? ctx.city->area()->getNumTiles() : 0;
	if (t == "ERA")        return ctx.player != NULL ? (int)ctx.player->getCurrentEra() + 1 : 0;   // 1..X counter
	// cross-scope RELIGION_X reads the world religion-level count; a CITY-scope RELIGION_X is a PRESENCE check (Present).
	if (en_starts(t, "RELIGION_") && a->scope != CASC_SCOPE_CITY && a->id >= 0)
		return GC.getGame().countReligionLevels((ReligionTypes)a->id);
	if (a->scope == CASC_SCOPE_EMPIRE || a->scope == CASC_SCOPE_TEAM)
	{
		CascadeCountScope sc = (a->scope == CASC_SCOPE_TEAM) ? CASCADE_COUNT_TEAM : CASCADE_COUNT_EMPIRE;
		const int ent = (a->scope == CASC_SCOPE_TEAM) ? (ctx.team ? (int)ctx.team->getID() : 0)
		                                              : (ctx.player ? (int)ctx.player->getID() : 0);
		if (en_starts(t, "BUILDING_") && a->id >= 0) return cascadeTally().buildingCount(ent, a->id, sc);
		if (en_starts(t, "UNIT_") && a->id >= 0)     return cascadeTally().unitCount(ent, a->id, sc);
	}
	return ev_present(ctx, a) ? 1 : 0;
}

// ---- EvalPresence (StoneBase) ----------------------------------------------------------------------------------

static bool ev_evalPresence(const CvCascadeEvalCtx& ctx, const CvCascadeEvalFlags& f, const CvCascadeCondition* a)
{
	if (f.ignorePlotScope && a->scope == CASC_SCOPE_PLOT) return true;
	const std::string& t = a->type;
	if (en_starts(t, "PROPERTY_"))
	{
		const int val = ev_countOf(ctx, a);
		return (a->min < 0 || val >= a->min) && (a->max < 0 || val <= a->max);
	}
	if (a->min >= 0 || a->max >= 0)
	{
		if (en_starts(t, "BONUS_") && (a->min < 0 || a->min <= 1) && a->max < 0)
		{
			if (a->connection != CASC_CONN_NONE) return ev_bonusPresent(ctx, a->id, a->connection, a->vicinity);
			if (f.bonusFromPlot && ctx.plot != NULL) return (int)ctx.plot->getBonusType(ctx.team ? ctx.team->getID() : NO_TEAM) == a->id;
			if (a->scope == CASC_SCOPE_PLOT) return ctx.plot != NULL && (int)ctx.plot->getBonusType(ctx.team ? ctx.team->getID() : NO_TEAM) == a->id;
			return ctx.city != NULL && ctx.city->hasBonus((BonusTypes)a->id);
		}
		const int n = ev_countOf(ctx, a);
		return (a->min < 0 || n >= a->min) && (a->max < 0 || n <= a->max);
	}
	// bare presence -- an EXPLICIT empire/team BUILDING scope means "the player/team HAS it anywhere" (the tally).
	if ((a->scope == CASC_SCOPE_EMPIRE || a->scope == CASC_SCOPE_TEAM) && en_starts(t, "BUILDING_"))
		return ev_countOf(ctx, a) >= 1;
	return ev_present(ctx, a);
}

// ---- EvalPredicate (StoneBase) ---------------------------------------------------------------------------------

static bool ev_evalPredicate(const CvCascadeEvalCtx& ctx, const CvCascadeEvalFlags& f, const CvCascadeCondition* pr)
{
	const CvPlot* p = ctx.plot;
	switch (pr->predKind)
	{
	case CASC_PRED_HAS_RIVER:       return p != NULL && p->isRiver();
	case CASC_PRED_HAS_IRRIGATION:  return p != NULL && p->isIrrigated();
	case CASC_PRED_HAS_LANDMARK:    return p != NULL && p->getLandmarkType() != NO_LANDMARK;
	case CASC_PRED_HAS_HILLS:       return p != NULL && p->isHills();
	case CASC_PRED_HAS_PEAK:        return p != NULL && p->isPeak();
	case CASC_PRED_IS_FLATLANDS:    return !(p != NULL && p->isHills()) && !(p != NULL && p->isPeak());
	case CASC_PRED_IS_WATER:        return p != NULL && p->isWater();
	case CASC_PRED_IS_LAND:         return !(p != NULL && p->isWater());
	case CASC_PRED_HAS_COAST:       return pr->min >= 0 ? (ctx.city != NULL && ctx.city->isCoastal(pr->min))
	                                                     : (p != NULL && p->isCoastalLand());
	case CASC_PRED_HAS_FRESHWATER:  return (p != NULL && p->isFreshWater()) || (p != NULL && p->isRiver());
	case CASC_PRED_HAS_TERRAIN:     return ev_cityPlotHas(ctx.city, evp_workedTerrain, pr);
	case CASC_PRED_HAS_FEATURE:     return pr->id < 0 ? ev_cityPlotHas(ctx.city, evp_workedFeatureAny, pr)
	                                                   : ev_cityPlotHas(ctx.city, evp_workedFeature, pr);
	case CASC_PRED_HAS_IMPROVEMENT: return ev_cityPlotHas(ctx.city, evp_workedImprovement, pr);
	case CASC_PRED_IS_CAPITAL:            return ctx.city != NULL && ctx.city->isCapital();
	case CASC_PRED_IS_GOVERNMENT_CENTER:  return ctx.city != NULL && ctx.city->isGovernmentCenter();
	case CASC_PRED_HAS_POWER:             return ctx.city != NULL && ctx.city->isPower();
	case CASC_PRED_IS_GOLDEN_AGE:         return ctx.player != NULL && ctx.player->isGoldenAge();
	case CASC_PRED_NO_NUKES:              return GC.getGame().isNoNukes();
	case CASC_PRED_HAS_STATE_RELIGION:    return ctx.player != NULL && ctx.player->getStateReligion() != NO_RELIGION;
	case CASC_PRED_STATE_RELIGION_IN_CITY:
		return ctx.player != NULL && ctx.player->getStateReligion() != NO_RELIGION
		    && ctx.city != NULL && ctx.city->isHasReligion(ctx.player->getStateReligion());
	case CASC_PRED_HAS_RELIGION:          return ctx.city != NULL && pr->id >= 0 && ctx.city->isHasReligion((ReligionTypes)pr->id);
	case CASC_PRED_HAS_CORPORATION:       return ctx.city != NULL && pr->id >= 0 && ctx.city->isActiveCorporation((CorporationTypes)pr->id);
	case CASC_PRED_IS_HOLY_CITY:
		return ctx.city != NULL && (pr->id < 0 ? ctx.city->isHolyCity() : ctx.city->isHolyCity((ReligionTypes)pr->id));
	case CASC_PRED_IS_STATE_RELIGION_HOLY_CITY:
		return ctx.player != NULL && ctx.player->getStateReligion() != NO_RELIGION
		    && ctx.city != NULL && ctx.city->isHolyCity(ctx.player->getStateReligion());
	case CASC_PRED_STATE_RELIGION:
	{
		const ReligionTypes sr = ctx.player != NULL ? ctx.player->getStateReligion() : NO_RELIGION;
		if (f.strictStateReligionForBuild) return (int)sr == pr->id;
		return (int)sr == pr->id || sr == NO_RELIGION;   // lenient (modifier); NonStateReligionCommerce deferred (modifier-side)
	}
	case CASC_PRED_LATITUDE:
	{
		int lat = ctx.plot != NULL ? ctx.plot->getLatitude()
		        : (ctx.city != NULL && ctx.city->plot() != NULL ? ctx.city->plot()->getLatitude() : 0);
		return (pr->min < 0 || lat >= pr->min) && (pr->max < 0 || lat <= pr->max);
	}
	case CASC_PRED_VICINITY:   return p != NULL;
	case CASC_PRED_WORKABLE:   return p != NULL && ctx.city != NULL && p->getOwner() == ctx.city->getOwner();
	case CASC_PRED_IS_WORKED:  return p != NULL && p->isBeingWorked();
	default:                   return true;   // UNKNOWN / ExistedFor / unmodelled domain -> IGNORED (json §3.5)
	}
}

// ---- IsWaivedPrereq + EvalGroup (StoneBase) --------------------------------------------------------------------

// A BUILDING prereq is WAIVED iff it is in the cascade's precomputed waived set (StoneBase IsWaivedPrereq reading
// EvalState.ObsoleteBuildings ∪ PrereqWaivedBuildings: a prereq obsoleted by a held tech, OR one whose SpecialBuilding
// group is civic-not-required). The set is built by AugmentState (en_augmentWaived) and pointed-to in the ctx; the
// evaluator just reads it (decoupled -- no InfoRepo / civic walk here). NULL set -> no waivers.
static bool ev_isWaivedPrereq(const CvCascadeEvalCtx& ctx, const CvCascadeCondition* c)
{
	if (c == NULL || c->kind != CASC_COND_PRESENCE || !en_starts(c->type, "BUILDING_") || c->id < 0) return false;
	return ctx.waivedPrereqBuildings != NULL && ctx.waivedPrereqBuildings->count(c->id) != 0;
}

bool cascadeEvalCondition(const CvCascadeCondition* c, const CvCascadeEvalCtx& ctx, const CvCascadeEvalFlags& flags)
{
	if (c == NULL) return true;                                  // vacuously true
	if (c->kind == CASC_COND_PRESENCE) return ev_evalPresence(ctx, flags, c);
	if (c->kind == CASC_COND_PREDICATE) return ev_evalPredicate(ctx, flags, c);

	// ConditionGroup (json §3.4): AND(all, non-waived) ∧ OR(any, non-waived) ∧ NONE(noneOf) ∧ enabled ∧ ¬disabled.
	for (size_t i = 0; i < c->all.size(); ++i)
		if (!ev_isWaivedPrereq(ctx, c->all[i]) && !cascadeEvalCondition(c->all[i], ctx, flags)) return false;
	if (!c->anyOf.empty())
	{
		bool bAnyCand = false, bAnyHit = false;
		for (size_t i = 0; i < c->anyOf.size(); ++i)
		{
			if (ev_isWaivedPrereq(ctx, c->anyOf[i])) continue;
			bAnyCand = true;
			if (cascadeEvalCondition(c->anyOf[i], ctx, flags)) { bAnyHit = true; break; }
		}
		if (bAnyCand && !bAnyHit) return false;                 // OR over non-waived members (all-waived => no requirement)
	}
	for (size_t i = 0; i < c->noneOf.size(); ++i)
		if (cascadeEvalCondition(c->noneOf[i], ctx, flags)) return false;
	if (c->enabled != NULL && !cascadeEvalCondition(c->enabled, ctx, flags)) return false;
	if (!flags.ignoreDisabled && c->disabled != NULL && cascadeEvalCondition(c->disabled, ctx, flags)) return false;
	return true;
}
