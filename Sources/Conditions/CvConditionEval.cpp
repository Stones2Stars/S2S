//
//	cascadeEvalCondition -- the PORT of StoneBase CascadingEnabler/ConditionEvaluator.cs (see the header). The walk
//	+ every predicate's semantics is a faithful transcription of the parity-proven C#; only the state reads differ
//	(the live engine here, EvalState/PlotContext there). Section headers below name the C# method ported.
//

#include "CvGameCoreDLL.h"
#include "AI/BetterBTSAI.h"          // PerfAccumTimer
#include "Conditions/CvConditionEval.h"
#include "CvGate.h"              // cascadeGateOk -- the entity-level enabled/disabled pair
#include "CvFoldTargetInfo.h"    // FoldTargets -- what a generalized plot predicate MEANS (json.md §3.5)
#include "Tally/CvTally.h"
#include "AI/CvPlayerAI.h"          // GET_PLAYER
#include "AI/CvTeamAI.h"           // GET_TEAM
#include "Defines/CvGlobals.h"
#include "Engine/CvCity.h"
#include "Engine/CvPlayer.h"
#include "Engine/CvPlot.h"
#include "Engine/CvTeam.h"
#include "Engine/CvGame.h"
#include "Engine/CvUnit.h"           // ctx.unit->getUnitInfo() (the IS_<TAG> predicate)
#include "CvUnitInfo.h"              // getTags() (the unit tag bitset)
#include "CvClassificationBlock.h"         // hasId (the classification bitset O(1) test)
#include "CvClassificationRegistry.h"   // cachedKeyId(CLSD_POLICY) -- resolve a policy key to its minted id
#include "Engine/CityContext.h"         // the city-scope HAVE read surface (contexts.md -- the evaluator's atoms ask it)
#include "Engine/EmpireContext.h"       // the empire/team-scope HAVE read surface (incl. the prebuilt policy union)
#include "Engine/PlotContext.h"         // the plot-fact read surface (every HAS_/IS_ plot predicate)
#include "Engine/CvPlotGroup.h"         // the reserved TRADED-bonus source (the ctx.plotGroup connection:"trade" leg)
#include <string>

static bool ev_hasPolicy(const EmpireContext* empireContext, const char* szKey);   // defined below (the L1 policy read)

static bool en_starts(const std::string& s, const char* pfx) { return s.compare(0, strlen(pfx), pfx) == 0; }

// ---- the context reads (contexts.md HAVE axis): the ctx HOLDS each scope's isolated live-state silo, so a
// predicate reads it directly -- city state through CityContext, empire/team state through EmpireContext, plot
// facts through PlotContext. A NULL context is the not-present verdict, unchanged. ---------------------------


// A BUILDING prereq must be ACTIVE -- present AND not dormant. Dormancy is CASCADE-COMPUTED (governed 100% by operate
// enablers, DEC-calc-zero-ride-in), never read from the engine; cascadeIsBuildingActive reads that precomputed fact.
static bool ev_hasActiveBuilding(const CvCascadeEvalCtx& ctx, int eBuilding)
{
	return cascadeIsBuildingActive(eBuilding, ctx);
}

// VICINITY scan helper: walk the city's workable plots (through the city context's radius forward) and test
// `pred` against each plot's PlotContext. (Mirrors StoneBase PlotHas over s.Plots.)
typedef bool (*EvPlotPred)(const PlotContext&, const CvCondition*, const CityContext&);
static bool ev_cityPlotHas(const CityContext* cityContext, EvPlotPred pred, const CvCondition* a)
{
	if (cityContext == NULL) return false;
	for (int i = 0; i < NUM_CITY_PLOTS; ++i)
	{
		const CvPlot* radiusPlot = cityContext->radiusPlot(i);
		if (radiusPlot != NULL && pred(radiusPlot->getPlotContext(), a, *cityContext)) return true;
	}
	return false;
}

// the workable-plot predicates used by Present (terrain/feature/improvement/route prereqs -- OWNED vicinity; a FEATURE
// also accepts a NEUTRAL tile unless EXP_STRICT_VICINITY).
static bool evp_feature(const PlotContext& plotContext, const CvCondition* a, const CityContext& cityContext)
{
	if (!plotContext.hasFeature(a->id)) return false;
	const bool bOwned = plotContext.owner() == cityContext.owner();
	const bool bNeutral = plotContext.owner() == (int)NO_PLAYER;
	return bOwned || (bNeutral && !GC.getGame().isOption((GameOptionTypes)GC.getInfoTypeForString("GAMEOPTION_EXP_STRICT_VICINITY")));
}
static bool evp_peak(const PlotContext& plotContext, const CvCondition*, const CityContext& cityContext)
{ return plotContext.owner() == cityContext.owner() && plotContext.hasPeak(); }
static bool evp_hill(const PlotContext& plotContext, const CvCondition*, const CityContext& cityContext)
{ return plotContext.owner() == cityContext.owner() && plotContext.hasHills(); }
static bool evp_terrain(const PlotContext& plotContext, const CvCondition* a, const CityContext& cityContext)
{ return plotContext.owner() == cityContext.owner() && plotContext.hasTerrain(a->id); }
static bool evp_improvement(const PlotContext& plotContext, const CvCondition* a, const CityContext& cityContext)
{ return plotContext.owner() == cityContext.owner() && plotContext.hasImprovement(a->id); }
static bool evp_route(const PlotContext& plotContext, const CvCondition* a, const CityContext& cityContext)
{ return plotContext.owner() == cityContext.owner() && plotContext.hasRoute(a->id); }
// worked-tile GOM predicates ({HAS_TERRAIN|FEATURE|IMPROVEMENT:X})
static bool evp_workedTerrain(const PlotContext& plotContext, const CvCondition* a, const CityContext&)
{ return plotContext.isWorked() && plotContext.hasTerrain(a->id); }
static bool evp_workedFeatureAny(const PlotContext& plotContext, const CvCondition*, const CityContext&)
{ return plotContext.isWorked() && plotContext.hasFeatureAny(); }
static bool evp_workedFeature(const PlotContext& plotContext, const CvCondition* a, const CityContext&)
{ return plotContext.isWorked() && plotContext.hasFeature(a->id); }
static bool evp_workedImprovement(const PlotContext& plotContext, const CvCondition* a, const CityContext&)
{ return plotContext.isWorked() && plotContext.hasImprovement(a->id); }
// ⚠ NO_TEAM -- the tile's bonus UNFILTERED BY REVEAL. A plot resolves in ISOLATION ([modifier.md] par.2: its
// substrate carries ONE value), so what a tile yields is not a per-observer question; the vicinity store beside
// this reads the same way and for the same reason.
static bool evp_workedBonus(const PlotContext& plotContext, const CvCondition* a, const CityContext&)
{ return plotContext.isWorked() && plotContext.hasBonus(a->id, (int)NO_TEAM); }

// ---- BonusPresent / VicinityHas (StoneBase) ---------------------------------------------------------------------

static bool ev_vicinityHas(const CvCascadeEvalCtx& ctx, int eBonus, CvCascVicinity disc)
{
	const CityContext* cityContext = ctx.cityContext;
	if (cityContext == NULL) return false;
	// An ACTIVE building in this city that `provides` eBonus supplies it IN-VICINITY (json §5a) -- computed from JSON
	// (the enabler's vicinityProvidedBonuses set), NEVER read from the engine's hasVicinityBonus (DEC-calc-zero-ride-in).
	if (ctx.vicinityProvidedBonuses != NULL && ctx.vicinityProvidedBonuses->count(eBonus) != 0) return true;
	// CONNECTED = the engine's OBTAINED-in-vicinity (json §3.4: owned+valid+connected). CvCity::hasVicinityBonus
	// (read through the city context) encodes EXACTLY that -- hasBonus-gated, then centre OR an
	// owned+valid+`isConnectedTo(this)` radius plot OR a building-provided supply -- so defer to it wholesale
	// (StoneBase's per-plot `BonusConnected` scan + VicinityBonuses fallback collapses to this one read;
	// `isConnectedToCapital` was the WRONG read).
	// Every tier is a stored, event-maintained set on the city context -- one O(1) read, never a radius scan
	// (contexts.md: a predicate that walks plots per call is the efficiency defect this design removes).
	// The building-provided supply is handled up-front from vicinityProvidedBonuses (json par.5a) -- the enabler
	// owns that half of the union, the context owns the MAP half.
	return cityContext->hasVicinityBonusAt(eBonus, disc);
}

// The TRADED leg (contexts.md: CvPlotGroup is the reserved explicit traded-bonus source; traded state is NEVER
// mirrored into CityContext). A city-bound ctx answers through the city's own plot-group-backed MAINTAINED count
// (CityContext::tradedBonusCount -- the tech-gate/minted/corp relay over the network count); a city-less ctx
// with an explicit plotGroup pass-in (the valuation's third context) reads the network object directly.
static bool ev_tradedBonus(const CvCascadeEvalCtx& ctx, int eBonus)
{
	const CityContext* cityContext = ctx.cityContext;
	if (cityContext != NULL) return cityContext->tradedBonusCount(eBonus) > 0;
	return ctx.plotGroup != NULL && eBonus >= 0 && ctx.plotGroup->hasBonus((BonusTypes)eBonus);
}

static bool ev_bonusPresent(const CvCascadeEvalCtx& ctx, int eBonus, CvCascConnection conn, CvCascVicinity vic)
{
	switch (conn)
	{
	case CASC_CONN_VICINITY:          return ev_vicinityHas(ctx, eBonus, vic);
	case CASC_CONN_TRADE:             return ev_tradedBonus(ctx, eBonus);
	// "trade|vicinity" = trade-network OR in-vicinity (json §3.4). The vicinity leg is REQUIRED here: a
	// MANUFACTURED bonus is supplied by an active building's `provides.bonuses` (json §5a) into
	// vicinityProvidedBonuses, NOT the trade network -- so a trade-only check misses every building-supplied
	// bonus (the "manufactured bonus buildings can't be built" bug). Matches StoneBase's TradeOrVicinity.
	case CASC_CONN_TRADE_OR_VICINITY: return ev_tradedBonus(ctx, eBonus) || ev_vicinityHas(ctx, eBonus, vic);
	default:
		if (ctx.plotContext != NULL) return ctx.plotContext->hasBonus(eBonus, ctx.empireContext != NULL ? ctx.empireContext->teamId() : (int)NO_TEAM);
		return ev_tradedBonus(ctx, eBonus);
	}
}

// The AS-IF-HELD hypothetical applied to ONE have-atom's live answer (CvConditionEval.h): absent wins, then
// present, then the live scope. With no hypothetical bound this is a single null test on the ordinary path.
static bool ev_hypothetical(const CvCascadeEvalCtx& ctx, EnEdgeBucket eBucket, int iId, bool bLive)
{
	return ctx.hypothetical == NULL ? bLive : ctx.hypothetical->has((int)eBucket, iId, bLive);
}

// ---- Present (StoneBase) -- bare presence by type prefix --------------------------------------------------------

static bool ev_present(const CvCascadeEvalCtx& ctx, const CvCondition* a)
{
	const std::string& t = a->type;
	const int id = a->id;
	const CityContext* cityContext = ctx.cityContext;
	const EmpireContext* empireContext = ctx.empireContext;
	// team-held facts read through the player's EmpireContext (team is deliberately not a context, contexts.md)
	// ⚖ EVERY HELD-KIND ROUTES ITS LIVE ANSWER THROUGH THE AS-IF-HELD HYPOTHETICAL, and this is no longer just
	// the what-if's business. The ATOM ROUTE (plane C, mc_applyTypeAtom) withdraws a deposit by resolving it
	// against the verdict it was BOOKED at, and a DOMAIN fact is PAST TENSE -- it announces the crossing once the
	// state has already moved. So the pin is what makes the withdrawal exact, and a kind whose branch is NOT
	// wrapped cannot be routed at all: its removal would resolve false and withdraw nothing, leaving a deposit
	// that compounds on every re-acquisition ([state-repositories.md] § THE INVARIANT).
	// ⛔ The old rule here -- "a kind nobody asks a what-if about is deliberately NOT wrapped" -- is retired: the
	// injection path is now exercised by every crossing, not only by a hypothetical question.
	if (en_starts(t, "TECH_"))     return ev_hypothetical(ctx, EDGEB_TECHS, id,
		empireContext != NULL && empireContext->teamHasTech(id));
	if (en_starts(t, "CIVIC_"))    return ev_hypothetical(ctx, EDGEB_CIVICS, id,
		empireContext != NULL && empireContext->hasCivic(id));
	if (en_starts(t, "TRAIT_"))    return ev_hypothetical(ctx, EDGEB_TRAITS, id,
		empireContext != NULL && empireContext->hasTrait(id));
	if (en_starts(t, "RELIGION_")) return ev_hypothetical(ctx, EDGEB_RELIGIONS, id,
		cityContext != NULL && id >= 0 && cityContext->hasReligion(id));
	if (en_starts(t, "HERITAGE_")) return ev_hypothetical(ctx, EDGEB_HERITAGES, id,
		empireContext != NULL && empireContext->hasHeritage(id));
	if (en_starts(t, "PROJECT_"))  return ev_hypothetical(ctx, EDGEB_PROJECTS, id,
		empireContext != NULL && empireContext->teamProjectCount(id) > 0);
	// the promotion-chain requires.build atoms (unit context -- the enPromotionValid level-up gate): held check.
	// Units are the deliberate FUTURE context scope (contexts.md), so this stays a raw unit read until it exists.
	if (en_starts(t, "PROMOTION_")) return ev_hypothetical(ctx, EDGEB_PROMOTIONS, id,
		ctx.unit != NULL && id >= 0 && ctx.unit->isHasPromotion((PromotionTypes)id));
	// the enabler GATE reads raw PRESENCE (ctx.buildingAtomsPresence -- the §7 has-list / engine PrereqInCity
	// mirror, exclusions included); deposits + the operate fixpoint read the cascade-computed ACTIVE set
	if (en_starts(t, "BUILDING_")) return ev_hypothetical(ctx, EDGEB_BUILDINGS, id, ctx.buildingAtomsPresence
		? (cityContext != NULL && cityContext->hasBuilding(id))
		: ev_hasActiveBuilding(ctx, id));
	if (en_starts(t, "CORPORATION_")) return ev_hypothetical(ctx, EDGEB_CORPORATIONS, id,
		cityContext != NULL && id >= 0 && cityContext->hasCorporation(id));
	// game/world-scope facts have no context by design (the scope set is plot/city/player, contexts.md)
	if (en_starts(t, "VICTORY_"))  return id >= 0 && GC.getGame().isVictoryValid((VictoryTypes)id);
	if (en_starts(t, "GAMEOPTION_")) return id >= 0 && GC.getGame().isOption((GameOptionTypes)id);
	if (en_starts(t, "BONUS_"))    return ev_hypothetical(ctx, EDGEB_BONUSES, id,
		ev_bonusPresent(ctx, id, a->connection, a->vicinity));
	// ⚖ THE MAP-CATEGORY GATE -- WHERE ON (or off) THE WORLD this may be built. It is a CITY-LOCAL plot check
	// ([enabler.md] par.7.1: the one city-local project fact stays a live check at the gate), so it asks the CITY'S OWN
	// plot, never the radius: a building is built IN the city, and a category is a property of the ground it
	// stands on rather than of a tile it can work.
	// ⛔ It answered TRUE unconditionally while unmodelled, which is the enable-side OVER-OFFER: the off-world
	// content gates on this and nothing else keeps it out of an Earth city's build list, so a Martian or lunar
	// building became offerable the moment its tech landed.
	// ⚑ The verdict is CvPlot::isMapCategoryType and nothing is re-expressed here
	// ([DEC-single-implementation]) -- including its own permissive leg, where a plot whose terrain names NO
	// category is allowed everything. ⚠ A plot's categories are DERIVED from its terrain and have no fact of
	// their own, which is exactly why the terrain fact seeds the map-category atom in the plot-atom index
	// ([enabler.md] par.8) -- the re-gate route was already wired and waiting for this body.
	if (en_starts(t, "MAPCATEGORY_"))
	{
		// Unresolved id, or no city to stand in: the atom cannot be ANSWERED, and an unanswerable one is IGNORED
		// rather than refused (json par.3.5). ⚠ Deliberately not the fail-closed shape its plot-substrate siblings
		// take: they ask about a RADIUS tile, which a city-less caller genuinely lacks, while this asks about the
		// asker's own ground -- so refusing here would HIDE every Earth-gated building (the overwhelming majority
		// of the registry) from any evaluation without a city, which is a far quieter failure than the over-offer.
		if (id < 0 || cityContext == NULL) return true;
		const CvPlot* pCityPlot = cityContext->cityPlot();
		return pCityPlot == NULL || pCityPlot->isMapCategoryType((MapCategoryTypes)id);
	}
	// plot-substrate vicinity scans (owned, culture-grown radius)
	if (en_starts(t, "FEATURE_"))     return ev_cityPlotHas(cityContext, evp_feature, a);
	if (t == "TERRAIN_PEAK")          return ev_cityPlotHas(cityContext, evp_peak, a);
	if (t == "TERRAIN_HILL")          return ev_cityPlotHas(cityContext, evp_hill, a);
	if (en_starts(t, "TERRAIN_"))     return ev_cityPlotHas(cityContext, evp_terrain, a);
	if (en_starts(t, "IMPROVEMENT_")) return ev_cityPlotHas(cityContext, evp_improvement, a);
	if (en_starts(t, "ROUTE_"))       return ev_cityPlotHas(cityContext, evp_route, a);
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
	const CityContext* cityContext = ctx.cityContext;
	const EmpireContext* empireContext = ctx.empireContext;
	if (en_starts(t, "PROPERTY_"))
	{
		iOut = (cityContext != NULL && id >= 0) ? cityContext->propertyValue(id) : 0;
		return true;
	}
	if (t == "POPULATION") { iOut = cityContext != NULL ? cityContext->population() : 0; return true; }
	// the §3.7 SPECIALIST count token -- json.md's own first worked example of a `per` scaler. Buildings,
	// civics and traits all author "+N per specialist"; without this the deposits fell to the presence
	// fallback and multiplied by 0/1 instead of a count.
	// ⚑ The SCOPE decides which count is meant, exactly as it does for BUILDING_/UNIT_ below: a CITY-scope
	// scaler is this city's own specialists (a local count reads the live object -- tally.md §2), and a
	// cross-city one is the tally's roll-up. A `per` with no scope takes the DEPOSIT's scope (json §3.7),
	// so an empire-scope deposit asks the empire.
	if (t == "SPECIALIST" && eScope == CASC_SCOPE_CITY)
	{
		iOut = cityContext != NULL ? cityContext->specialistCount() : 0;
		return true;
	}
	// the IMPROVEMENT_ count at CITY scope: how many of the city's plots carry it (the per-improvement
	// free-specialist scaler's multiplier). A presence fallback answered 1 where the count is wanted.
	// ⚠ CITY scope only -- elsewhere an improvement reference stays the ordinary presence atom.
	if (en_starts(t, "IMPROVEMENT_") && id >= 0 && eScope == CASC_SCOPE_CITY && cityContext != NULL)
	{
		iOut = cityContext->improvedPlotCount(id);
		return true;
	}
	// BONUS_ counts are VOLUMETRIC at city scope (json.md par.3.4: presence = min:1 of the same count; the network
	// count lives on the plot group, read via the city relay -- CityContext::tradedBonusCount). Without this branch
	// every `per`-scaled bonus deposit (the legacy per-instance BonusYieldChanges class) fell to the 0/1 presence
	// fallback and undercounted by the whole network count. Presence-shaped atoms (min<=1 with a connection) never
	// reach here -- the evalPresence order routes them to ev_bonusPresent first.
	if (en_starts(t, "BONUS_") && id >= 0 && eScope == CASC_SCOPE_CITY && cityContext != NULL)
	{
		iOut = cityContext->tradedBonusCount(id);
		return true;
	}
	// the §3.1 CORPORATION_LEVEL counter (rulings 4+10 -- the corp HQ-revenue per-scaler): the game-wide corp
	// level count. `id` is the SOURCE corp, SELF-collapsed onto the entry at mapFrom
	// (CvModifiers::resolvePerToken from CvCorporationInfo); an unresolved id counts 0 (fail-visible).
	if (t == "CORPORATION_LEVEL")
	{
		iOut = id >= 0 ? GC.getGame().countCorporationLevels((CorporationTypes)id) : 0;
		return true;
	}
	if (t == "CITY")       { iOut = empireContext != NULL ? empireContext->numCities() : 0; return true; }
	if (t == "TEAM")       { iOut = empireContext != NULL ? empireContext->teamMemberCount() : 0; return true; }
	if (t == "AREA_SIZE")  { iOut = cityContext != NULL ? cityContext->areaSize() : 0; return true; }
	// the §3.1 DISTANCE_TO_GOVERNMENT_CENTER city counter: plot distance to the owner's NEAREST government
	// centre, 0 in one. Served from the maintained CityContext store -- the min-over-cities walk the legacy
	// distance-maintenance formula did per read is now an event-maintained int ([contexts.md]).
	if (t == "DISTANCE_TO_GOVERNMENT_CENTER")
	{
		iOut = cityContext != NULL ? cityContext->governmentCenterDistance() : 0;
		return true;
	}
	if (t == "ERA")        { iOut = empireContext != NULL ? empireContext->currentEra() + 1 : 0; return true; }   // 1..X counter
	// the §3.1 commerce SLIDER-RATE counters (ruling 20: the player's current slider percents as plain counters
	// -- "happiness per 10% culture rate" / "anger per gold rate" author as ordinary per-scaled deposits)
	if (t == "GOLD_RATE")      { iOut = empireContext != NULL ? empireContext->commerceRate((int)COMMERCE_GOLD) : 0; return true; }
	if (t == "RESEARCH_RATE")  { iOut = empireContext != NULL ? empireContext->commerceRate((int)COMMERCE_RESEARCH) : 0; return true; }
	if (t == "CULTURE_RATE")   { iOut = empireContext != NULL ? empireContext->commerceRate((int)COMMERCE_CULTURE) : 0; return true; }
	if (t == "ESPIONAGE_RATE") { iOut = empireContext != NULL ? empireContext->commerceRate((int)COMMERCE_ESPIONAGE) : 0; return true; }
	// the §3.1 CULTURE_PERCENTAGE city counter (ruling 22: the city's OWN-culture percent of its plot -- the
	// engine formula CvCity.cpp:5650-5654, forwarded by CityContext::ownCulturePercent; foreign share is the
	// authored `100 −` telescoping pair, never an inverse unit here)
	if (t == "CULTURE_PERCENTAGE")
	{
		iOut = cityContext != NULL ? cityContext->ownCulturePercent() : 0;
		return true;
	}
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
		// the count-scope ENTITY id -- both sides asked of the player: a team is the tech bridge and holds no
		// live-state surface, so its id is forwarded through EmpireContext like every other team fact.
		const int ent = (ctx.empireContext == NULL) ? 0
		              : (eScope == CASC_SCOPE_TEAM) ? ctx.empireContext->teamId()
		                                           : ctx.empireContext->playerId();
		// the SPECIALIST count's AGGREGATE half: a local city count stays local (above), and rolling it up
		// across cities is the tally's job -- one place holding the count of all specialists in scope.
		if (t == "SPECIALIST") { iOut = CvCascadeTally::specialistCount(ent, sc); return true; }
		if (en_starts(t, "BUILDING_") && id >= 0) { iOut = CvCascadeTally::buildingCount(ent, id, sc); return true; }
		// TECH_X as a COUNT: how many teams hold it (world) / does this side hold it (team, empire). The count
		// domain a `techShare`-style threshold reads -- "known by N other teams" is `{TECH_X, world, min: N}`.
		if (en_starts(t, "TECH_") && id >= 0) { iOut = CvCascadeTally::techCount(ent, id, sc); return true; }
		if (en_starts(t, "UNIT_") && id >= 0)
		{
			iOut = (sc == CASCADE_COUNT_WORLD) ? GC.getGame().getUnitCreatedCount((UnitTypes)id)
			                                   : CvCascadeTally::unitCount(ent, id, sc);
			return true;
		}
		// TAG_X as a COUNT: how many in-scope units carry the classification tag -- the iterate-on-read tally
		// domain (tally.md: no O(1) object aggregate exists for a tag). Without this branch an authored
		// {TAG_X, min:N} atom fell through to presence and answered 0/1.
		if (en_starts(t, "TAG_") && id >= 0)
		{
			iOut = CvCascadeTally::countUnitsWithTag(ent, id, sc);
			return true;
		}
	}
	return false;   // not a countable domain -> the caller's presence fallback
}

static int ev_countOf(const CvCascadeEvalCtx& ctx, const CvCondition* a)
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
	CvCondition a;   // stack presence atom -- no children, trivially freed
	a.kind = CASC_COND_PRESENCE;
	a.type = sType;
	a.scope = eScope;
	a.id = iTypeId;
	return ev_present(ec, &a) ? 1 : 0;
}

// ---- EvalPresence (StoneBase) ----------------------------------------------------------------------------------

static bool ev_evalPresence(const CvCascadeEvalCtx& ctx, const CvCascadeEvalFlags& f, const CvCondition* a)
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
		// A PROPERTY value is legitimately NEGATIVE, so a bound's PRESENCE decides, never its sign.
		return (!a->hasMin || val >= a->min) && (!a->hasMax || val <= a->max);
	}
	// A typed BONUS_ presence whose reference did NOT resolve (a->id < 0 -- a curator typo like BONUS_ELEPHANT for
	// BONUS_ELEPHANTS, or a renamed/removed bonus) names an entity that does not exist, so it is NOT present. Guard
	// BEFORE any id-indexed engine read: hasBonus((BonusTypes)-1) walks the plot-group bonus array at [-1] (an out-of-bounds read ->
	// the load-warm-up AV), and a plot's NO_BONUS (== -1) would SPURIOUSLY match a->id == -1. An unknown reference must
	// degrade to false, never crash/mis-satisfy (json §3.5); the miss is surfaced by the load-time FK census, not here.
	if (a->id < 0 && en_starts(t, "BONUS_")) return false;
	if (a->min >= 0 || a->max >= 0)
	{
		if (en_starts(t, "BONUS_") && (a->min < 0 || a->min <= 1) && a->max < 0)
		{
			if (a->connection != CASC_CONN_NONE) return ev_bonusPresent(ctx, a->id, a->connection, a->vicinity);
			if (f.bonusFromPlot && ctx.plotContext != NULL) return ctx.plotContext->hasBonus(a->id, ctx.empireContext != NULL ? ctx.empireContext->teamId() : (int)NO_TEAM);
			if (a->scope == CASC_SCOPE_PLOT) return ctx.plotContext != NULL && ctx.plotContext->hasBonus(a->id, ctx.empireContext != NULL ? ctx.empireContext->teamId() : (int)NO_TEAM);
			// ⛔ AN UNQUALIFIED CITY-SCOPE BONUS ATOM IS **TRADED, ONLY** (owner): `{type, scope:"city", min:1}`
			// reads the plot group and nothing else. The trade list and the onSite list are populated by the
			// SAME acquisition events, which is exactly what makes them look interchangeable -- and they are
			// ⛔ **NEVER TO BE LOOKED AT TOGETHER, AT ANY TIME**. A union here would silently satisfy a network
			// question with a locally-produced resource, which is the conflation the onSite/connected split was
			// made to end. onSite is asked EXPLICITLY, via connection:"vicinity" + vicinity:"onSite".
			return ev_tradedBonus(ctx, a->id);
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

static bool ev_evalPredicate(const CvCascadeEvalCtx& ctx, const CvCascadeEvalFlags& f, const CvCondition* pr)
{
	const PlotContext* plotContext = ctx.plotContext;
	const CityContext* cityContext = ctx.cityContext;
	const EmpireContext* empireContext = ctx.empireContext;
	switch (pr->predKind)
	{
	case CASC_PRED_HAS_RIVER:       return plotContext != NULL && plotContext->hasRiver();
	case CASC_PRED_HAS_IRRIGATION:  return plotContext != NULL && plotContext->hasIrrigation();
	case CASC_PRED_HAS_LANDMARK:    return plotContext != NULL && plotContext->hasLandmark();
	case CASC_PRED_HAS_HILLS:       return plotContext != NULL && plotContext->hasHills();
	// ⛔ THE GENERALIZED PLOT PREDICATES RESOLVE THROUGH THEIR FOLD-TARGET SET, NOT THE PLOT-TYPE BIT
	// (json.md §3.5 -- we never fold onto a boolean, we need a target to fold onto). The plot-TYPE axis these
	// read before is not a carrier: its fact announces for a FRACTION of the map and NEVER for water, so
	// IS_WATER was false on every water plot in the game and every `plots {IS_WATER}` deposit -- Lighthouse,
	// Pier, Seawalls, Fisherman's Hut, the Seafaring achievement -- delivered NOTHING, silently. TERRAIN
	// announces for every plot, so the fold set is what makes these answerable at all.
	// ⚑ A predicate no foldtargets file defines keeps its own read: the registry is open (json.md §8), so this
	// is "no set authored yet", never a missing case.
	case CASC_PRED_HAS_PEAK:        return FoldTargets::defines(CASC_PRED_HAS_PEAK)
	                                     ? (plotContext != NULL && FoldTargets::terrainMatches(CASC_PRED_HAS_PEAK, plotContext->terrainId()))
	                                     : (plotContext != NULL && plotContext->hasPeak());
	case CASC_PRED_IS_FLATLANDS:    return FoldTargets::defines(CASC_PRED_IS_FLATLANDS)
	                                     ? (plotContext != NULL && FoldTargets::terrainMatches(CASC_PRED_IS_FLATLANDS, plotContext->terrainId()))
	                                     : (plotContext == NULL || plotContext->isFlatlands());
	case CASC_PRED_IS_WATER:        return FoldTargets::defines(CASC_PRED_IS_WATER)
	                                     ? (plotContext != NULL && FoldTargets::terrainMatches(CASC_PRED_IS_WATER, plotContext->terrainId()))
	                                     : (plotContext != NULL && plotContext->isWater());
	case CASC_PRED_IS_LAND:         return FoldTargets::defines(CASC_PRED_IS_LAND)
	                                     ? (plotContext != NULL && FoldTargets::terrainMatches(CASC_PRED_IS_LAND, plotContext->terrainId()))
	                                     : (plotContext == NULL || plotContext->isLand());
	case CASC_PRED_HAS_COAST:       return pr->min >= 0 ? (cityContext != NULL && cityContext->isCoastal(pr->min))
	                                                     : (plotContext != NULL && plotContext->hasCoast());
	// Fresh water is target-relative (json §3.5): on a PLOT target the tile's own access; with a CITY in
	// context ALSO the city's fresh-water ACCESS verdict (the `providesFreshWater` amenity refcount) -- the
	// engine's own dormancy leg is plot()->isFreshWater() || hasFreshWater(), so a plot-only read wrongly dorms
	// every city fed by a provider building (the AQUEDUCT/WATER_TOWER chain -- the worked-plot yield collapse).
	case CASC_PRED_HAS_FRESHWATER:  return (plotContext != NULL && plotContext->hasFreshWater())
	                                     || (cityContext != NULL && cityContext->hasFreshWaterAccess());
	case CASC_PRED_HAS_TERRAIN:     return ev_cityPlotHas(cityContext, evp_workedTerrain, pr);
	case CASC_PRED_HAS_FEATURE:     return pr->id < 0 ? ev_cityPlotHas(cityContext, evp_workedFeatureAny, pr)
	                                                   : ev_cityPlotHas(cityContext, evp_workedFeature, pr);
	case CASC_PRED_HAS_IMPROVEMENT: return ev_cityPlotHas(cityContext, evp_workedImprovement, pr);
	// ⚖ TARGET-RELATIVE, like HAS_COAST and HAS_FRESHWATER above -- a predicate is evaluated against the DEPOSIT'S
	// TARGET (json par.3.5), and this one is authored overwhelmingly on PLOT-scope improvement yields ("+N food, but
	// only on a tile carrying salt"), which is the deliveryguy shape: the improvement owns its own output and the
	// bonus is the condition ([modifier.md] par.4). So a bound PLOT answers about ITSELF; with only a CITY in
	// context it takes the worked-radius reading its HAS_TERRAIN / HAS_IMPROVEMENT siblings take.
	// ⛔ It used to fall to `default: return true`, so every one of those deposits applied on EVERY tile the
	// improvement stood on -- a silent, plausible over-yield rather than a visible failure, which is why it
	// survived: the predicate was parsed and even indexed for re-gating, and only the EVALUATION was missing.
	case CASC_PRED_HAS_BONUS:       return plotContext != NULL
	                                     ? (pr->id >= 0 && plotContext->hasBonus(pr->id, (int)NO_TEAM))
	                                     : ev_cityPlotHas(cityContext, evp_workedBonus, pr);
	case CASC_PRED_IS_CAPITAL:            return cityContext != NULL && cityContext->isCapital();
	// Both predicates below read the CITY's own verdict through the context's forwards, and neither is a
	// self-containment violation to re-flag.
	// ⚖ isPowered resolves CvCity::isPowered -- the ONE definition of "powered" (a live grantor supplies it AND no
	// blackout gates delivery), whose crossing the amenity fold announces. A STATUS is middleware gating delivery,
	// so it never reaches this evaluator ([state.md] § A STATUS IS MIDDLEWARE).
	// ⚠ isGovernmentCenter still rides its own hand-named counter and migrates onto the amenity crossing as it
	// converts ([contexts.md]).
	case CASC_PRED_IS_GOVERNMENT_CENTER:  return cityContext != NULL && cityContext->isGovernmentCenter();
	case CASC_PRED_HAS_POWER:             return cityContext != NULL && cityContext->isPowered();
	case CASC_PRED_IS_GOLDEN_AGE:         return empireContext != NULL && empireContext->isGoldenAge();
	case CASC_PRED_IS_REBEL:              return empireContext != NULL && empireContext->isRebel();
	case CASC_PRED_IS_ANARCHY:            return empireContext != NULL && empireContext->isAnarchy();   // #430 outcome gate
	case CASC_PRED_IS_OWNED:              return plotContext != NULL && plotContext->isOwned();         // #430 outcome gate (plot in owned territory)
	case CASC_PRED_NO_NUKES:              return GC.getGame().isNoNukes();
	case CASC_PRED_HAS_STATE_RELIGION:    return empireContext != NULL && empireContext->stateReligion() >= 0;
	case CASC_PRED_STATE_RELIGION_IN_CITY:
		return empireContext != NULL && empireContext->stateReligion() >= 0
		    && cityContext != NULL && cityContext->hasReligion(empireContext->stateReligion());
	case CASC_PRED_HAS_RELIGION:          return cityContext != NULL && pr->id >= 0 && cityContext->hasReligion(pr->id);
	// {HAS_CORPORATION:X} = corp ACTIVE (json §3.5 / enabler §3), distinct from a bare CORPORATION_ presence atom.
	// isActiveCorporation is a SANCTIONED engine-owned input (engine-driven spread state like religion), NOT a ride-in.
	case CASC_PRED_HAS_CORPORATION:       return cityContext != NULL && pr->id >= 0 && cityContext->hasActiveCorporation(pr->id);
	// {IS_HEADQUARTERS: CORPORATION_X} -- the corp HQ-revenue gate (ruling 10): this city IS the corp's HQ city.
	// Bare form = HQ of any corporation.
	case CASC_PRED_IS_HEADQUARTERS:
		return cityContext != NULL && (pr->id < 0 ? cityContext->isHeadquartersAny() : cityContext->isHeadquartersOf(pr->id));
	case CASC_PRED_IS_HOLY_CITY:
		return cityContext != NULL && (pr->id < 0 ? cityContext->isHolyCityAny() : cityContext->isHolyCityOf(pr->id));
	case CASC_PRED_IS_STATE_RELIGION_HOLY_CITY:
		return empireContext != NULL && empireContext->stateReligion() >= 0
		    && cityContext != NULL && cityContext->isHolyCityOf(empireContext->stateReligion());
	// the counted-religion test (ruling 23; the §3.7 `religion:` filter): true iff the religion under test
	// (ctx.religion, set by cascadeCountCityReligions) IS the owner's state religion. No religion in context
	// -> not-present (false), the NULL-object convention of this evaluator.
	case CASC_PRED_IS_STATE_RELIGION:
		return empireContext != NULL && ctx.religion >= 0 && empireContext->stateReligion() == ctx.religion;
	// {CIVIC_CATEGORY: CIVICOPTION_X}: the CIVIC whose value is being resolved sits in that category -- the
	// authored shape for "this trait waives the upkeep of religion civics" (a WHICH is a PREDICATE, never a
	// keyed member: [DEC-conditions-are-predicates]). A SOURCE-SLOT predicate, so no civic in hand answers
	// FALSE rather than resolving against whichever civic a walk happened to reach last
	// (contexts.md § THE SOURCE SLOTS). ⚠ The civic-upkeep consumer must set ctx.civic per civic it charges;
	// until it does, the deposit is inert -- visibly unapplied, never silently applied to every civic.
	case CASC_PRED_CIVIC_CATEGORY:
		return ctx.civic >= 0 && ctx.civic < GC.getNumCivicInfos()
		    && GC.getCivicInfo((CivicTypes)ctx.civic).getCivicOption() == pr->id;
	case CASC_PRED_STATE_RELIGION:
	{
		const int iStateReligion = empireContext != NULL ? empireContext->stateReligion() : -1;
		if (f.strictStateReligionForBuild) return iStateReligion == pr->id;
		// lenient (modifier) + the L1 POLICY read (2026-07-05, the ruled Free-Church shape): a present
		// religion's SR-gated commerce pays under the nonStateReligionCommerce POLICY -- the legacy
		// getReligionCommerceByReligion OR-gate, derived from the civic/trait grantors' §9 policies
		// blocks, NEVER the legacy m_iNonStateReligionCommerceCount counter
		return iStateReligion == pr->id || iStateReligion < 0
		    || ev_hasPolicy(ctx.empireContext, "nonStateReligionCommerce");
	}
	case CASC_PRED_LATITUDE:
	{
		int lat = 0;
		if (plotContext != NULL)
		{
			lat = plotContext->latitude();
		}
		else if (cityContext != NULL && cityContext->cityPlot() != NULL)
		{
			lat = cityContext->cityPlot()->getPlotContext().latitude();
		}
		return (pr->min < 0 || lat >= pr->min) && (pr->max < 0 || lat <= pr->max);
	}
	// {existedFor:{min:N}} -- the AGE gate (json §3.5): N GAME YEARS since the SOURCE building was built. The
	// unit is years, not turns: the city ledger stores getGameTurnYear() at build time and the tooltip has
	// always promised "doubles in 1000 years", so nothing converts. The live authorings are the commerce
	// doublings (the legacy CommerceChangeDoubleTimes), which author as a SECOND deposit on the same slot
	// gated here rather than a post-sum multiply (modifier.md §3).
	case CASC_PRED_EXISTED_FOR:
	{
		// Needs the carrier: a deposit resolved with no source building in hand cannot be aged. Answering TRUE
		// would apply every age-gated deposit from turn 0 -- which is precisely the hole this closes -- and
		// answering against some other building would be worse than either.
		if (cityContext == NULL || ctx.sourceBuilding < 0)
		{
			return false;
		}
		const int iBuiltYear = cityContext->buildingBuildYear(ctx.sourceBuilding);
		// ⛔ Test the absence sentinel BEFORE subtracting: a build year is legitimately NEGATIVE (BC), so
		// absence cannot be read off the sign, and GC.getGame().getGameTurnYear() - MIN_INT overflows.
		if (iBuiltYear == MIN_INT)
		{
			return false;
		}
		return pr->min < 0 || (GC.getGame().getGameTurnYear() - iBuiltYear) >= pr->min;
	}
	// {natureYield:{<channel>:N}} -- the improvement PLACEMENT threshold (json §3.5): the target plot's
	// PRE-improvement nature yield of the channel (`id` = YieldTypes) must be >= `min`. Reads the plot
	// package's SUBSTRATE segment, so no improvement is applied and nothing is walked per call.
	// A plot predicate: no plot in context -> not-present (false).
	case CASC_PRED_NATURE_YIELD:
		return plotContext != NULL && pr->id >= 0 && pr->min >= 0
		    && plotContext->natureYield(pr->id) >= pr->min;
	case CASC_PRED_VICINITY:   return plotContext != NULL;
	case CASC_PRED_WORKABLE:   return plotContext != NULL && cityContext != NULL && plotContext->owner() == cityContext->owner();
	case CASC_PRED_IS_WORKED:  return plotContext != NULL && plotContext->isWorked();
	// IS_<TAG> -- classification-tag membership against the UNIT target (json §8). The tag id resolves lazily: the
	// TAG_* infotypes are minted after condition parse, so `param` carries the TAG_<SUFFIX> type name. An UNMINTED
	// tag (undefined/retired) is an unknown predicate -> IGNORED (true, json §3.5); a minted tag the unit lacks -> false.
	case CASC_PRED_IS_TAG:
	{
		if (ctx.unit == NULL) return false;   // a unit predicate with no unit target cannot match (the PROMOTION_ precedent)
		const int iTagId = pr->id >= 0 ? pr->id : GC.getInfoTypeForString(pr->param.c_str(), /*bHideAssert*/true);
		return iTagId < 0 ? true : ctx.unit->getUnitInfo().getTags()->hasId(iTagId);
	}
	default:                   return true;   // UNKNOWN / unmodelled domain -> IGNORED, never false (json §3.5)
	}
}

// ---- IsWaivedPrereq + EvalGroup (StoneBase) --------------------------------------------------------------------

// A BUILDING prereq is WAIVED iff it is in the cascade's precomputed waived set (StoneBase IsWaivedPrereq reading
// EvalState.ObsoleteBuildings ∪ PrereqWaivedBuildings: a prereq obsoleted by a held tech, OR one whose SpecialBuilding
// group is civic-not-required). The set is built by AugmentState (en_augmentWaived) and pointed-to in the ctx; the
// evaluator just reads it (decoupled -- no InfoRepo / civic walk here). NULL set -> no waivers.
static bool ev_isWaivedPrereq(const CvCascadeEvalCtx& ctx, const CvCondition* c)
{
	if (c == NULL || c->kind != CASC_COND_PRESENCE || !en_starts(c->type, "BUILDING_") || c->id < 0) return false;
	return ctx.waivedPrereqBuildings != NULL && ctx.waivedPrereqBuildings->count(c->id) != 0;
}

// The §9 POLICY read (json §9): resolve the policy key to its minted POLICY_* id and read the player's PREBUILT
// enacted-policy DICTIONARY the player owns (delta-maintained by the civic/trait/player-init facts, PolicyContext),
// so the per-leaf read is O(1) with NO walk. The union IS the single source (DEC-single-implementation): it reads the
// CvJson*Info policies sets, never a legacy counter (DEC-calc-zero-ride-in). Sole data grantors today: complex traits
// (bigot/progressive/spiritual); the civic half is model headroom the union carries for free. (The naive per-leaf walk
// -- ~1200 traits × a string+set lookup EACH, evaluated on millions of {STATE_RELIGION:X} leaves per turn -- is what
// the prebuilt union eliminates; it also retired a per-player version memo whose invalidation trigger had been orphaned.)
static bool ev_hasPolicy(const EmpireContext* empireContext, const char* szKey)
{
	if (empireContext == NULL) return false;
	// ONE call site, one literal key -> a per-call-site memoized id (the CLS_HAS idiom; a second key would need its own).
	static int s_pid = -1;
	const int iPolicy = ClassificationRegistry::cachedKeyId(s_pid, CLSD_POLICY, szKey);
	return iPolicy >= 0 && empireContext->hasPolicy(iPolicy);
}

// Reads the cascade-computed ACTIVE set, or -- absent it -- falls back to raw PRESENCE (hasBuilding, a raw
// input; NOT the engine active-building read, the ride-in dormancy we replaced). See the header + DEC-calc-zero-ride-in.
bool cascadeIsBuildingActive(int eBuilding, const CvCascadeEvalCtx& ec)
{
	return ec.activeBuildings != NULL ? (ec.activeBuildings->count(eBuilding) != 0)
	                                  : (ec.cityContext != NULL && ec.cityContext->hasBuilding(eBuilding));
}

// The obsolete set the SAME obsoletion process maintains (present ∧ obsoleted-by-held-tech, json §4.2): an obsolete
// building deposits its `whenObsolete` tree in place of its normal families. NULL set = none (no raw-presence fallback:
// obsolescence needs the team's held techs, computed cascade-side, never read from the engine).
bool cascadeIsBuildingObsolete(int eBuilding, const CvCascadeEvalCtx& ec)
{
	return ec.obsoleteBuildings != NULL && eBuilding >= 0 && ec.obsoleteBuildings->count(eBuilding) != 0;
}

// The §3.7 counted-kind RELIGION filter's count leg (see the header): Σ over the city's PRESENT religions of
// the filter verdict, each religion evaluated with ctx.religion set (the IS_STATE_RELIGION predicate's input).
// The lenient default flags -- a deposit-side count, never a build gate.
int cascadeCountCityReligions(const CvCondition* filter, const CvCascadeEvalCtx& ec)
{
	if (ec.cityContext == NULL)
	{
		return 0;
	}
	static const CvCascadeEvalFlags kFlags;
	CvCascadeEvalCtx perReligionCtx = ec;
	int iCount = 0;
	const CityContext& cityContext = *ec.cityContext;
	for (int iReligion = 0; iReligion < GC.getNumReligionInfos(); ++iReligion)
	{
		if (!cityContext.hasReligion(iReligion))
		{
			continue;
		}
		perReligionCtx.religion = iReligion;
		if (cascadeEvalCondition(filter, perReligionCtx, kFlags))
		{
			++iCount;
		}
	}
	return iCount;
}

bool cascadeEvalCondition(const CvCondition* c, const CvCascadeEvalCtx& ctx, const CvCascadeEvalFlags& flags)
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

// Walk toward the leaf a FALSE verdict turns on. `bWantTrue` says which way the CALLER needed this node to go,
// and it inverts under `noneOf` -- a noneOf fails on a child that HOLDS, so the offending atom there is a true
// one. Each arm mirrors cascadeEvalCondition's own, waiver included; nothing here decides truth.
static const CvCondition* ev_offendingAtom(const CvCondition* c, const CvCascadeEvalCtx& ctx,
	const CvCascadeEvalFlags& flags, bool bWantTrue)
{
	if (c == NULL)
	{
		return NULL;
	}
	if (c->kind != CASC_COND_GROUP)
	{
		return c;                                              // a leaf IS the answer
	}

	if (bWantTrue)
	{
		// The group had to HOLD and did not, so the first clause that fails is the cause.
		for (size_t iAll = 0; iAll < c->all.size(); ++iAll)
		{
			if (!ev_isWaivedPrereq(ctx, c->all[iAll]) && !cascadeEvalCondition(c->all[iAll], ctx, flags))
			{
				return ev_offendingAtom(c->all[iAll], ctx, flags, true);
			}
		}
		if (!c->anyOf.empty())
		{
			const CvCondition* pFirstCandidate = NULL;
			bool bAnyHit = false;
			for (size_t iAny = 0; iAny < c->anyOf.size(); ++iAny)
			{
				if (ev_isWaivedPrereq(ctx, c->anyOf[iAny]))
				{
					continue;
				}
				if (pFirstCandidate == NULL)
				{
					pFirstCandidate = c->anyOf[iAny];
				}
				if (cascadeEvalCondition(c->anyOf[iAny], ctx, flags))
				{
					bAnyHit = true;
					break;
				}
			}
			// EVERY non-waived member failed, so each one is a cause; the first names the kind. An all-waived
			// OR imposes no requirement at all, exactly as the evaluator reads it.
			if (pFirstCandidate != NULL && !bAnyHit)
			{
				return ev_offendingAtom(pFirstCandidate, ctx, flags, true);
			}
		}
		for (size_t iNone = 0; iNone < c->noneOf.size(); ++iNone)
		{
			if (cascadeEvalCondition(c->noneOf[iNone], ctx, flags))
			{
				return ev_offendingAtom(c->noneOf[iNone], ctx, flags, false);
			}
		}
		if (c->enabled != NULL && !cascadeEvalCondition(c->enabled, ctx, flags))
		{
			return ev_offendingAtom(c->enabled, ctx, flags, true);
		}
		if (!flags.ignoreDisabled && c->disabled != NULL && cascadeEvalCondition(c->disabled, ctx, flags))
		{
			return ev_offendingAtom(c->disabled, ctx, flags, false);
		}
		return NULL;                                           // it holds after all -- nothing refused
	}

	// The group had to be FALSE and it HELD, so every clause that had to pass for it to hold is a cause.
	for (size_t iAll = 0; iAll < c->all.size(); ++iAll)
	{
		if (!ev_isWaivedPrereq(ctx, c->all[iAll]))
		{
			return ev_offendingAtom(c->all[iAll], ctx, flags, false);
		}
	}
	for (size_t iAny = 0; iAny < c->anyOf.size(); ++iAny)
	{
		if (!ev_isWaivedPrereq(ctx, c->anyOf[iAny]) && cascadeEvalCondition(c->anyOf[iAny], ctx, flags))
		{
			return ev_offendingAtom(c->anyOf[iAny], ctx, flags, false);
		}
	}
	if (!c->noneOf.empty())
	{
		return ev_offendingAtom(c->noneOf[0], ctx, flags, true);
	}
	if (c->enabled != NULL)
	{
		return ev_offendingAtom(c->enabled, ctx, flags, false);
	}
	return NULL;
}

const CvCondition* cascadeFailingAtom(const CvCondition* c, const CvCascadeEvalCtx& ctx,
	const CvCascadeEvalFlags& flags)
{
	if (cascadeEvalCondition(c, ctx, flags))
	{
		return NULL;                                           // it holds -- there is nothing to explain
	}
	return ev_offendingAtom(c, ctx, flags, true);
}

void cascadeTopLevelClauses(const CvCondition* pRoot, std::vector<const CvCondition*>& kClausesOut)
{
	if (pRoot == NULL)
	{
		return;
	}
	if (pRoot->kind == CASC_COND_GROUP && !pRoot->all.empty())
	{
		for (size_t iClause = 0; iClause < pRoot->all.size(); ++iClause)
		{
			kClausesOut.push_back(pRoot->all[iClause]);
		}
		return;
	}
	kClausesOut.push_back(pRoot);
}

// The ENTITY-LEVEL applicability gate (json.md §2 Applicability; owner 2026-07-08 -- the loadPrune replacement):
// the entity applies only while `enabled` holds (NULL = always-on) and `disabled` does not (§3.9 order: enabled
// first, disabled overrides). Same evaluator as every other condition -- a GAMEOPTION_X leaf reads the live options.
bool cascadeGateOk(const CvGate* pGate, const CvCascadeEvalCtx& ec, const CvCascadeEvalFlags& flags)
{
	if (pGate == NULL) return true;
	if (pGate->enabled != NULL && !cascadeEvalCondition(pGate->enabled, ec, flags)) return false;
	if (pGate->disabled != NULL && cascadeEvalCondition(pGate->disabled, ec, flags)) return false;
	return true;
}
