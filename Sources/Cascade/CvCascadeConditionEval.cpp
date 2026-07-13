//
//	CvCascadeConditionEval -- the PORT of StoneBase CascadingEnabler/ConditionEvaluator.cs (see the header). The walk
//	+ every predicate's semantics is a faithful transcription of the parity-proven C#; only the state reads differ
//	(the live engine here, EvalState/PlotContext there). Section headers below name the C# method ported.
//

#include "CvGameCoreDLL.h"
#include "CvCascadePerfCount.h"   // per-turn call counters + stopwatches (owner 2026-07-02: repeat-calc hunt)
#include "AI/BetterBTSAI.h"          // PerfAccumTimer
#include "CvCascadeConditionEval.h"
#include "CvJsonGate.h"              // cascadeGateOk -- the entity-level enabled/disabled pair
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
#include "CvCascadeMMKernel.h"        // traitData -- the active-set trait resolver (the L1 policy read)
#include "CvCivicInfo.h"          // the civic §9 policies block (the L1 policy read)
#include "CvTraitInfo.h"          // the trait §9 policies block
#include "Repos/InfoRepo.h"
#include "CvCivicInfo.h"
#include <string>

static bool ev_playerHasPolicy(const CvPlayer* pPlayer, const char* szKey);   // defined below (the L1 policy read)

static bool en_starts(const std::string& s, const char* pfx) { return s.compare(0, strlen(pfx), pfx) == 0; }

// ---- small live-engine helpers (the EvalState set-membership reads) ----------------------------------------------

static bool ev_hasCivic(const CvPlayer* pl, int eCivic)
{
	if (pl == NULL || eCivic < 0) return false;
	for (int iCO = 0; iCO < GC.getNumCivicOptionInfos(); ++iCO)
		if ((int)pl->getCivics((CivicOptionTypes)iCO) == eCivic) return true;
	return false;
}

// A BUILDING prereq must be ACTIVE -- present AND not dormant. Dormancy is CASCADE-COMPUTED (governed 100% by operate
// enablers, DEC-calc-zero-ride-in), never read from the engine; cascadeIsBuildingActive reads that precomputed fact.
static bool ev_hasActiveBuilding(const CvCascadeEvalCtx& ctx, int eBuilding)
{
	return cascadeIsBuildingActive(eBuilding, ctx);
}

// VICINITY scan helper: walk the city's workable plots and test `pred`. (Mirrors StoneBase PlotHas over s.Plots.)
typedef bool (*EvPlotPred)(const CvPlot*, const CvJsonCondition*, const CvCity*);
static bool ev_cityPlotHas(const CvCity* c, EvPlotPred pred, const CvJsonCondition* a)
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
static bool evp_feature(const CvPlot* pl, const CvJsonCondition* a, const CvCity* c)
{
	if ((int)pl->getFeatureType() != a->id) return false;
	const bool bOwned = pl->getOwner() == c->getOwner();
	const bool bNeutral = pl->getOwner() == NO_PLAYER;
	return bOwned || (bNeutral && !GC.getGame().isOption((GameOptionTypes)GC.getInfoTypeForString("GAMEOPTION_EXP_STRICT_VICINITY")));
}
static bool evp_peak(const CvPlot* pl, const CvJsonCondition*, const CvCity* c)
{ return pl->getOwner() == c->getOwner() && pl->isPeak(); }
static bool evp_hill(const CvPlot* pl, const CvJsonCondition*, const CvCity* c)
{ return pl->getOwner() == c->getOwner() && pl->isHills(); }
static bool evp_terrain(const CvPlot* pl, const CvJsonCondition* a, const CvCity* c)
{ return pl->getOwner() == c->getOwner() && (int)pl->getTerrainType() == a->id; }
static bool evp_improvement(const CvPlot* pl, const CvJsonCondition* a, const CvCity* c)
{ return pl->getOwner() == c->getOwner() && (int)pl->getImprovementType() == a->id; }
static bool evp_route(const CvPlot* pl, const CvJsonCondition* a, const CvCity* c)
{ return pl->getOwner() == c->getOwner() && (int)pl->getRouteType() == a->id; }
// worked-tile GOM predicates ({HAS_TERRAIN|FEATURE|IMPROVEMENT:X})
static bool evp_workedTerrain(const CvPlot* pl, const CvJsonCondition* a, const CvCity*)
{ return pl->isBeingWorked() && (int)pl->getTerrainType() == a->id; }
static bool evp_workedFeatureAny(const CvPlot* pl, const CvJsonCondition*, const CvCity*)
{ return pl->isBeingWorked() && pl->getFeatureType() != NO_FEATURE; }
static bool evp_workedFeature(const CvPlot* pl, const CvJsonCondition* a, const CvCity*)
{ return pl->isBeingWorked() && (int)pl->getFeatureType() == a->id; }
static bool evp_workedImprovement(const CvPlot* pl, const CvJsonCondition* a, const CvCity*)
{ return pl->isBeingWorked() && (int)pl->getImprovementType() == a->id; }

// ---- BonusPresent / VicinityHas (StoneBase) ---------------------------------------------------------------------

static bool ev_vicinityHas(const CvCascadeEvalCtx& ctx, int eBonus, CvCascVicinity disc)
{
	const CvCity* c = ctx.city;
	if (c == NULL) return false;
	// An ACTIVE building in this city that `provides` eBonus supplies it IN-VICINITY (json §5a) -- computed from JSON
	// (the enabler's vicinityProvidedBonuses set), NEVER read from the engine's hasVicinityBonus (DEC-calc-zero-ride-in).
	if (ctx.vicinityProvidedBonuses != NULL && ctx.vicinityProvidedBonuses->count(eBonus) != 0) return true;
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
	return false;   // the building-provided supply is handled up-front from vicinityProvidedBonuses (json §5a), NOT the engine
}

static bool ev_bonusPresent(const CvCascadeEvalCtx& ctx, int eBonus, CvCascConnection conn, CvCascVicinity vic)
{
	const CvCity* c = ctx.city;
	switch (conn)
	{
	case CASC_CONN_VICINITY:          return ev_vicinityHas(ctx, eBonus, vic);
	case CASC_CONN_TRADE:             return c != NULL && c->hasBonus((BonusTypes)eBonus);
	// "trade|vicinity" = trade-network OR in-vicinity (json §3.4). The vicinity leg is REQUIRED here: a
	// MANUFACTURED bonus is supplied by an active building's `provides.bonuses` (json §5a) into
	// vicinityProvidedBonuses, NOT the trade network -- so a hasBonus-only check misses every building-supplied
	// bonus (the "manufactured bonus buildings can't be built" bug). Matches StoneBase's TradeOrVicinity.
	case CASC_CONN_TRADE_OR_VICINITY: return (c != NULL && c->hasBonus((BonusTypes)eBonus)) || ev_vicinityHas(ctx, eBonus, vic);
	default:
		if (ctx.plot != NULL) return (int)ctx.plot->getBonusType(ctx.team ? ctx.team->getID() : NO_TEAM) == eBonus;
		return c != NULL && c->hasBonus((BonusTypes)eBonus);
	}
}

// ---- Present (StoneBase) -- bare presence by type prefix --------------------------------------------------------

static bool ev_present(const CvCascadeEvalCtx& ctx, const CvJsonCondition* a)
{
	const std::string& t = a->type;
	const int id = a->id;
	if (en_starts(t, "TECH_"))     return ctx.team != NULL && id >= 0 && ctx.team->isHasTech((TechTypes)id);
	if (en_starts(t, "CIVIC_"))    return ev_hasCivic(ctx.player, id);
	if (en_starts(t, "TRAIT_"))    return ctx.player != NULL && id >= 0 && ctx.player->hasTrait((TraitTypes)id);
	if (en_starts(t, "RELIGION_")) return ctx.city != NULL && id >= 0 && ctx.city->isHasReligion((ReligionTypes)id);
	if (en_starts(t, "HERITAGE_")) return ctx.player != NULL && id >= 0 && ctx.player->hasHeritage((HeritageTypes)id);
	if (en_starts(t, "PROJECT_"))  return ctx.team != NULL && id >= 0 && ctx.team->getProjectCount((ProjectTypes)id) > 0;
	if (en_starts(t, "BUILDING_")) return ev_hasActiveBuilding(ctx, id);
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

// THE countable core -- ONE implementation (DEC-single-implementation), shared by the condition count-atoms
// (ev_countOf below) and the §3.7 `per` resolver (cascadeCountOf -> MMKernel::perScale). Fills iOut with the
// count of TYPE/token at SCOPE and returns true; false = the type names no countable domain (the callers fall
// back to presence 0/1). A bool-protocol, NOT a sentinel: a PROPERTY_* "count" is the city's property VALUE and
// can be legitimately NEGATIVE. Cross-city scopes resolve via the tally (tally.md §2); WORLD rides the same
// tally roll-up, EXCEPT a unit's world count = the engine's LIFETIME-CREATED counter (json §4.4 / tally.md §4 --
// the UnitEnabler world-cap read: "born once, still consumes its slot").
static bool ev_countCore(const CvCascadeEvalCtx& ctx, const std::string& t, int id, CvCascScope eScope, int& iOut)
{
	if (en_starts(t, "PROPERTY_"))
	{
		iOut = (ctx.city != NULL && id >= 0) ? ctx.city->getPropertiesConst()->getValueByProperty((PropertyTypes)id) : 0;
		return true;
	}
	if (t == "POPULATION") { iOut = ctx.city != NULL ? ctx.city->getPopulation() : 0; return true; }
	if (t == "CITY")       { iOut = ctx.player != NULL ? ctx.player->getNumCities() : 0; return true; }
	if (t == "TEAM")       { iOut = ctx.team != NULL ? ctx.team->getNumMembers() : 0; return true; }
	if (t == "AREA_SIZE")  { iOut = (ctx.city != NULL && ctx.city->area() != NULL) ? ctx.city->area()->getNumTiles() : 0; return true; }
	if (t == "ERA")        { iOut = ctx.player != NULL ? (int)ctx.player->getCurrentEra() + 1 : 0; return true; }   // 1..X counter
	// cross-scope RELIGION_X reads the world religion-level count; a CITY-scope RELIGION_X is a PRESENCE check (Present).
	if (en_starts(t, "RELIGION_") && eScope != CASC_SCOPE_CITY && id >= 0)
	{
		iOut = GC.getGame().countReligionLevels((ReligionTypes)id);
		return true;
	}
	if (eScope == CASC_SCOPE_EMPIRE || eScope == CASC_SCOPE_TEAM || eScope == CASC_SCOPE_WORLD)
	{
		const CascadeCountScope sc = (eScope == CASC_SCOPE_TEAM) ? CASCADE_COUNT_TEAM
		                           : (eScope == CASC_SCOPE_WORLD) ? CASCADE_COUNT_WORLD : CASCADE_COUNT_EMPIRE;
		const int ent = (eScope == CASC_SCOPE_TEAM) ? (ctx.team ? (int)ctx.team->getID() : 0)
		                                            : (ctx.player ? (int)ctx.player->getID() : 0);
		if (en_starts(t, "BUILDING_") && id >= 0) { iOut = cascadeTally().buildingCount(ent, id, sc); return true; }
		if (en_starts(t, "UNIT_") && id >= 0)
		{
			iOut = (sc == CASCADE_COUNT_WORLD) ? GC.getGame().getUnitCreatedCount((UnitTypes)id)
			                                   : cascadeTally().unitCount(ent, id, sc);
			return true;
		}
	}
	return false;   // not a countable domain -> the caller's presence fallback
}

static int ev_countOf(const CvCascadeEvalCtx& ctx, const CvJsonCondition* a)
{
	int n = 0;
	if (ev_countCore(ctx, a->type, a->id, a->scope, n)) return n;
	return ev_present(ctx, a) ? 1 : 0;   // full-atom presence (connection/vicinity intact)
}

// The exposed count surface for the §3.7 `per` resolver -- a thin caller of the SAME core as ev_countOf (never a
// parallel count path). The presence fallback builds a bare default atom (the per grammar carries no
// connection/min/max), so e.g. a BONUS_X per counts the city's presence 0/1 exactly as a count-atom would.
int cascadeCountOf(int iTypeId, const std::string& sType, CvCascScope eScope, const CvCascadeEvalCtx& ec)
{
	int n = 0;
	if (ev_countCore(ec, sType, iTypeId, eScope, n)) return n;
	CvJsonCondition a;   // stack presence atom -- no children, trivially freed
	a.kind = CASC_COND_PRESENCE;
	a.type = sType;
	a.scope = eScope;
	a.id = iTypeId;
	return ev_present(ec, &a) ? 1 : 0;
}

// ---- EvalPresence (StoneBase) ----------------------------------------------------------------------------------

static bool ev_evalPresence(const CvCascadeEvalCtx& ctx, const CvCascadeEvalFlags& f, const CvJsonCondition* a)
{
	if (f.ignorePlotScope && a->scope == CASC_SCOPE_PLOT) return true;
	const std::string& t = a->type;
	// VISIBLE-frontier GREYABLE relaxation (enabler.md §6): for the build-list question, a connectable BONUS_ resource
	// and an unadopted CIVIC_ are treated as PRESENT so the entity shows GREYED rather than HIDDEN ("go get copper" /
	// "adopt this civic"). Every other clause (tech, building prereq, terrain/placement, religion, ...) stays a HARD
	// hide. Model: "pretend the resource/civic is present" -- a positive requirement is satisfied. STRICT gates
	// (bTestVisible=false) never set testVisible, so the buildable-now set is unchanged.
	if (f.testVisible && (en_starts(t, "BONUS_") || en_starts(t, "CIVIC_"))) return true;
	if (en_starts(t, "PROPERTY_"))
	{
		const int val = ev_countOf(ctx, a);
		return (a->min < 0 || val >= a->min) && (a->max < 0 || val <= a->max);
	}
	// A typed BONUS_ presence whose reference did NOT resolve (a->id < 0 -- a curator typo like BONUS_ELEPHANT for
	// BONUS_ELEPHANTS, or a renamed/removed bonus) names an entity that does not exist, so it is NOT present. Guard
	// BEFORE any id-indexed engine read: hasBonus((BonusTypes)-1) walks m_paiNumBonuses[-1] (an out-of-bounds read ->
	// the load-warm-up AV), and a plot's NO_BONUS (== -1) would SPURIOUSLY match a->id == -1. An unknown reference must
	// degrade to false, never crash/mis-satisfy (json §3.5); the miss is surfaced by the load-time FK census, not here.
	if (a->id < 0 && en_starts(t, "BONUS_")) return false;
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

static bool ev_evalPredicate(const CvCascadeEvalCtx& ctx, const CvCascadeEvalFlags& f, const CvJsonCondition* pr)
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
	// The two engine-counter reads below are OWNER-RULED SANCTIONED (2026-07-05, cutover.md Rulings #4):
	// isGovernmentCenter -> the counter is KEEP until the Gate-3 building-attributes lane wires (then this
	// predicate derives from the cascade operating buildings); isPower -> the power machinery is KEEP wholesale ("a city
	// either has power or does not"), revisited at the later power pass. Neither is a self-containment
	// violation to re-flag.
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
		// lenient (modifier) + the L1 POLICY read (2026-07-05, the ruled Free-Church shape): a present
		// religion's SR-gated commerce pays under the nonStateReligionCommerce POLICY -- the legacy
		// getReligionCommerceByReligion OR-gate, derived from the civic/trait grantors' §9 policies
		// blocks, NEVER the legacy m_iNonStateReligionCommerceCount counter
		return (int)sr == pr->id || sr == NO_RELIGION
		    || ev_playerHasPolicy(ctx.player, "nonStateReligionCommerce");
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
static bool ev_isWaivedPrereq(const CvCascadeEvalCtx& ctx, const CvJsonCondition* c)
{
	if (c == NULL || c->kind != CASC_COND_PRESENCE || !en_starts(c->type, "BUILDING_") || c->id < 0) return false;
	return ctx.waivedPrereqBuildings != NULL && ctx.waivedPrereqBuildings->count(c->id) != 0;
}

// The §9 POLICY read (derived-on-query over the LIVE grantors -- adopted civics + active traits; the L1
// ruling: the SR-commerce waiver "is in essence a civic-instated POLICY"). Reads the CvJson*Info policies
// sets, never a legacy counter (DEC-calc-zero-ride-in). Sole data grantors today: complex traits
// (bigot/progressive/spiritual); the civic half is model headroom the walk carries for free.
// ⛔ MEMOIZED PER PLAYER (the 2026-07-05 grind fix, mapped by the condEval caller split): the naive walk
// (~1200 traits × a std::string construction + set lookup EACH) ran per {STATE_RELIGION:X} LEAF -- the
// operating buildings fixpoint + frontier fills evaluate those lenient leaves millions of times per turn (ceFacts 2.58M +
// ceFrontB 2.15M), which ground the first verification turn to a crawl. The verdict changes only on
// civic/trait events, so it memoizes on a version bumped by cascadePolicyStateChanged (wired into
// markPlayerScopeAndCities, the civic/trait/tech event fan-in; a tech bump is a harmless extra recompute).
// Game-thread statics (the established census/memo idiom).
static int s_policyVer[MAX_PLAYERS];        // bumped on civic/trait/tech events (0 = pristine)
static int s_policyMemoVer[MAX_PLAYERS];    // the memoized verdict's version (-1 = never computed)
static bool s_policyMemoNSRC[MAX_PLAYERS];  // the nonStateReligionCommerce verdict
static bool s_policyInit = false;

void cascadePolicyStateChanged(int ePlayer)
{
	if (!s_policyInit) return;   // pristine arrays -- the first read initializes
	if (ePlayer >= 0 && ePlayer < MAX_PLAYERS) ++s_policyVer[ePlayer];
}

static bool ev_playerHasPolicy(const CvPlayer* pPlayer, const char* szKey)
{
	if (pPlayer == NULL) return false;
	if (!s_policyInit)
	{
		for (int i = 0; i < MAX_PLAYERS; ++i) { s_policyVer[i] = 0; s_policyMemoVer[i] = -1; s_policyMemoNSRC[i] = false; }
		s_policyInit = true;
	}
	const int p = (int)pPlayer->getID();
	if (p < 0 || p >= MAX_PLAYERS) return false;
	if (s_policyMemoVer[p] == s_policyVer[p]) return s_policyMemoNSRC[p];   // the O(1) hot path

	bool bHas = false;
	for (int i = 0; i < GC.getNumCivicOptionInfos() && !bHas; ++i)
	{
		const CivicTypes eCivic = pPlayer->getCivics((CivicOptionTypes)i);
		if (eCivic == NO_CIVIC) continue;
		const CvCivicInfo* d = static_cast<const CvCivicInfo*>(InfoRepo<CvCivicInfo>::get().get(eCivic));
		if (d != NULL && d->getPolicies() != NULL && d->getPolicies()->has(szKey)) bHas = true;
	}
	for (int t = 0; t < GC.getNumTraitInfos() && !bHas; ++t)
	{
		if (!pPlayer->hasTrait((TraitTypes)t)) continue;
		const CvTraitInfo* d = MMKernel::traitData(t);
		if (d != NULL && d->getPolicies() != NULL && d->getPolicies()->has(szKey)) bHas = true;
	}
	s_policyMemoNSRC[p] = bHas;
	s_policyMemoVer[p] = s_policyVer[p];
	return bHas;
}

// Reads the cascade-computed ACTIVE set, or -- absent it -- falls back to raw PRESENCE (hasBuilding, a raw
// input; NOT the engine active-building read, the ride-in dormancy we replaced). See the header + DEC-calc-zero-ride-in.
bool cascadeIsBuildingActive(int eBuilding, const CvCascadeEvalCtx& ec)
{
	return ec.activeBuildings != NULL ? (ec.activeBuildings->count(eBuilding) != 0)
	                                  : (ec.city != NULL && eBuilding >= 0 && ec.city->hasBuilding((BuildingTypes)eBuilding));
}

// The obsolete set the SAME obsoletion process maintains (present ∧ obsoleted-by-held-tech, json §4.2): an obsolete
// building deposits its `whenObsolete` tree in place of its normal families. NULL set = none (no raw-presence fallback:
// obsolescence needs the team's held techs, computed cascade-side, never read from the engine).
bool cascadeIsBuildingObsolete(int eBuilding, const CvCascadeEvalCtx& ec)
{
	return ec.obsoleteBuildings != NULL && eBuilding >= 0 && ec.obsoleteBuildings->count(eBuilding) != 0;
}

bool cascadeEvalCondition(const CvJsonCondition* c, const CvCascadeEvalCtx& ctx, const CvCascadeEvalFlags& flags)
{
	++CascadePerf::condEval;
	++CascadePerf::condEvalBy[CascadePerf::condCaller];   // the caller-domain split (CascadeCondScope sets the tag)
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

// The ENTITY-LEVEL applicability gate (json.md §2 Applicability; owner 2026-07-08 -- the loadPrune replacement):
// the entity applies only while `enabled` holds (NULL = always-on) and `disabled` does not (§3.9 order: enabled
// first, disabled overrides). Same evaluator as every other condition -- a GAMEOPTION_X leaf reads the live options.
bool cascadeGateOk(const CvJsonGate* pGate, const CvCascadeEvalCtx& ec, const CvCascadeEvalFlags& flags)
{
	if (pGate == NULL) return true;
	if (pGate->enabled != NULL && !cascadeEvalCondition(pGate->enabled, ec, flags)) return false;
	if (pGate->disabled != NULL && cascadeEvalCondition(pGate->disabled, ec, flags)) return false;
	return true;
}
