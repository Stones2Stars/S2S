//
//	BuildingEnabler -- StoneBase CalculateBuildableBuildings.cs (see the header): the building frontier
//	generate/gate (waiver augment, cap, scaled prereq, buildable), a declared surface over the ONE EnablerKernel
//	primitive (the single-source law, patterns.md).
//

#include "CvGameCoreDLL.h"
#include "Enabler/CvBuildingEnabler.h"
#include "Enabler/CvEnablerKernel.h"    // EnablerKernel::obsoletedByHeldTech / requiresMet / allowedOk / scanCondDeps / wireOperatingBuildings
#include "Conditions/CvConditionEval.h"   // CvCascadeEvalCtx -- the gate evaluation's city context
#include "Spine/CvEventSpine.h"       // spineGameLoadInProgress -- load gates once at GAME_LOAD_FINISHED, never mid-read
#include "Tally/CvTally.h"     // cascadeTally -- the group-cap member counts (the buildings count domain)
#include "CvInfo.h"
#include "CvBuildingInfo.h"       // notConstructible (the enabler's own never-buildable flag; self-containment)
#include "Repos/InfoRepo.h"
#include "AI/CvPlayerAI.h"            // GET_PLAYER
#include "AI/CvTeamAI.h"             // GET_TEAM
#include "Defines/CvGlobals.h"
#include "Engine/CvCity.h"
#include "Engine/CvPlayer.h"
#include "Engine/CvTeam.h"
#include "CvBuildingInfo.h"
#include "CvCivicInfo.h"
#include "Enabler/CvEnabler.h"            // EnablerDomain/CityEnabler -- the standardized per-city domain (CvCity::m_enabler)
#include "CvTechInfo.h"           // cascadeStartNode -- the TECH_GAME_START root redirect (the tech HAVE axis)
#include "CvReligionInfo.h"       // the religion HAVE axis repo
#include "CvCorporationInfo.h"    // the corporation HAVE axis repo
#include "CvBonusInfo.h"          // the bonus HAVE axis repo
#include "CvCultureLevelInfo.h"   // the culture-level HAVE axis repo

// AugmentState's prereq-WAIVER set (StoneBase BuildingEnabler.AugmentState: ObsoleteBuildings ∪ PrereqWaivedBuildings):
// a BUILDING is a waived prereq iff its obsoleting tech is held by the team (the JSON `obsoletedBy.techs` edge --
// EnablerKernel::obsoletedByHeldTech, the ONE tech-obsolescence authority), OR its SpecialBuilding group is made
// not-required by an adopted civic (enables.specialBuildingsWaived). Shared by the building + unit enablers (both gate
// requires.build through the SAME evaluator). The vicinity-supply + gov-center AugmentState operating buildings are read LIVE by the
// evaluator (hasVicinityBonus / isGovernmentCenter), so only the waived set is materialized here.
void BuildingEnabler::augmentWaived(const CvPlayer& kPlayer, const CvTeam& kTeam, std::set<int>& waived)
{
	const int nB = GC.getNumBuildingInfos();
	for (int b = 0; b < nB; ++b)   // obsolete-by-held-tech (obsoletedBy.techs)
	{
		if (EnablerKernel::obsoletedByHeldTech(InfoRepo<CvBuildingInfo>::get().get(b), kTeam)) waived.insert(b);
	}
	std::set<int> waivedSpecials;   // the SpecialBuilding groups the player's adopted civics make not-required
	for (int co = 0; co < GC.getNumCivicOptionInfos(); ++co)
	{
		const CivicTypes c = kPlayer.getCivics((CivicOptionTypes)co);
		if (c == NO_CIVIC) continue;
		const CvInfo* j = InfoRepo<CvCivicInfo>::get().get((int)c);
		if (j == NULL) continue;
		const std::vector<int>* p = j->edge(EDGEF_ENABLES, EDGEB_SPECIAL_BUILDINGS_WAIVED);
		if (p != NULL)
			for (size_t i = 0; i < p->size(); ++i) waivedSpecials.insert((*p)[i]);
	}
	if (!waivedSpecials.empty())
		for (int b = 0; b < nB; ++b)
		{
			const SpecialBuildingTypes sb = (SpecialBuildingTypes)GC.getBuildingInfo((BuildingTypes)b).getSpecialBuildingType();
			if (sb != NO_SPECIALBUILDING && waivedSpecials.count((int)sb) != 0) waived.insert(b);
		}
}

// ==================== the STANDARDIZED per-city BUILDING domain (CvCity::m_enabler) ====================
// enabler.md par.7/7.1. Membership is the refcount FORMULA on EnablerDomain; the domain arrays are the ONLY
// mutable state: every HAVE-event applies its source's edge deltas DIRECTLY (the event carries the delta --
// flip-guarded emits, or a live-derivable crossing), and the load seed replays the same appliers over the
// object-owned has-lists. Order-independence is the formula's (removal wins); idempotency is the emit surface's
// (flip-guarded) plus the held-flag guards where an emit is documented broad (tech).

// the source's cascade info per axis (the tech axis redirects the TECH_GAME_START root to cascadeStartNode)
static const CvInfo* bd_sourceInfo(int eAxis, int iId)
{
	switch (eAxis)
	{
	case BuildingEnabler::AX_TECH:
		if (iId == GC.getInfoTypeForString("TECH_GAME_START", true)) return &cascadeStartNode();
		return InfoRepo<CvTechInfo>::get().get(iId);
	case BuildingEnabler::AX_CIVIC:    return InfoRepo<CvCivicInfo>::get().get(iId);
	case BuildingEnabler::AX_BUILDING: return InfoRepo<CvBuildingInfo>::get().get(iId);
	case BuildingEnabler::AX_RELIGION: return InfoRepo<CvReligionInfo>::get().get(iId);
	case BuildingEnabler::AX_CORP:     return InfoRepo<CvCorporationInfo>::get().get(iId);
	case BuildingEnabler::AX_BONUS:    return InfoRepo<CvBonusInfo>::get().get(iId);
	case BuildingEnabler::AX_CULTURE:  return InfoRepo<CvCultureLevelInfo>::get().get(iId);
	}
	return NULL;
}

static void bd_applyAxis(EnablerDomain& d, int eAxis, int iId, int iDelta)
{
	if (iId >= 0) EnablerKernel::applyEdges(d, bd_sourceInfo(eAxis, iId), EDGEB_BUILDINGS, iDelta);
}

// The CITY-CREATED applier (founding init + the load read's start, BEFORE the city's own in-read emits): init
// the domain (size + the offerability static exclusions) and fold ONLY the cross-scope HAVE that predates
// the city -- team techs + player civics (no events can carry 400 pre-existing techs to a new city). The
// city's OWN facts (buildings/religions/corps/bonuses/culture) arrive as DOMAIN events -- at load from the
// in-read reseed emits, at founding from the real grant/build emits -- one mechanism (docs/spine.md §5 (the load reseed)).
// ⛔ `identity.enabledCivilizations` is NOT a bar here: legacy applied it only inside the strongly-restricted
// NPC lockdown (a whitelist for WHICH restricted NPCs may build this -- inert for every normal civ), and that
// lockdown is a requires.build civ-membership gate when civilizations wire (json.md par.9 policies note).
// Reading it as a universal whitelist statically barred 21 empire-level constructibles for every human player.
void BuildingEnabler::onCityCreated(const CvCity& kCity)
{
	const CvPlayer& kPlayer = GET_PLAYER(kCity.getOwner());
	const CvTeam& kTeam = GET_TEAM(kPlayer.getTeam());
	EnablerDomain& d = kCity.m_enabler.buildings;
	d.init(GC.getNumBuildingInfos());
	for (int b = 0; b < GC.getNumBuildingInfos(); ++b)
	{
		const CvBuildingInfo* jb = (const CvBuildingInfo*)InfoRepo<CvBuildingInfo>::get().get(b);
		if (jb != NULL && !jb->isOfferable()) d.setStaticExcluded(b, true);
	}
	// the TECH_GAME_START root IS a held engine tech (the load backfill guarantees it) -- covered by this loop
	for (int t = 0; t < GC.getNumTechInfos(); ++t)
		if (kTeam.isHasTech((TechTypes)t)) bd_applyAxis(d, AX_TECH, t, +1);
	for (int co = 0; co < GC.getNumCivicOptionInfos(); ++co)
	{
		const CivicTypes c = kPlayer.getCivics((CivicOptionTypes)co);
		if (c != NO_CIVIC) bd_applyAxis(d, AX_CIVIC, (int)c, +1);
	}
	// docs/specs/enabler.md §2 (empire-level buildings): the owner's held EMPIRE-LEVEL buildings are HAVE this city starts under -- the
	// member reads held here (never offerable) and its edges contribute, the civic fold's two-leg shape: the
	// grantor fact fans the standing cities, and a city that starts existing folds what the owner already holds.
	const std::vector<BuildingTypes>& heldBuildings = kPlayer.getHasBuildings();
	for (size_t iHeld = 0; iHeld < heldBuildings.size(); ++iHeld)
	{
		const int iHeldBuilding = (int)heldBuildings[iHeld];
		const CvBuildingInfo* jHeld = (const CvBuildingInfo*)InfoRepo<CvBuildingInfo>::get().get(iHeldBuilding);
		if (jHeld != NULL && jHeld->isEmpireLevel())
		{
			d.setHeld(iHeldBuilding, true);
			if (!EnablerKernel::obsoletedByHeldTech(jHeld, kTeam))
				EnablerKernel::applyEdges(d, jHeld, EDGEB_BUILDINGS, +1);
		}
	}
	// gate the fold's entrants (no events carried them, so nothing else gates them). Inside the load bracket
	// the GAME_LOAD_FINISHED pass gates instead (a mid-read evaluation is the hazard, see the header).
	if (!spineGameLoadInProgress()) gateCity(kCity);
}

// ==================== THE REQUIRES GATE (enabler.md par.7.1 steps 2+3) ====================

// The SpecialBuilding GROUP cap (json §4.4): the member authors identity.specialBuildingType (the engine
// getSpecialBuilding FK); the GROUP entity holds the cap (allowed:{empire:N} -- the grouped wonders); the
// count = Σ member buildings at the cap scope through the tally's buildings domain. Group->members is DERIVED
// (json §4.4 "member->group is authored, group->members derived") -- load-compiled once, game-thread static.
static std::map<int, std::vector<int> > s_specialBuildingMembers;
static bool s_specialBuildingMembersBuilt = false;
static const std::vector<int>& bd_sbMembers(int iSb)
{
	if (!s_specialBuildingMembersBuilt)
	{
		s_specialBuildingMembersBuilt = true;
		for (int b = 0; b < GC.getNumBuildingInfos(); ++b)
		{
			const SpecialBuildingTypes sb = (SpecialBuildingTypes)GC.getBuildingInfo((BuildingTypes)b).getSpecialBuildingType();
			if (sb != NO_SPECIALBUILDING) s_specialBuildingMembers[(int)sb].push_back(b);
		}
	}
	return s_specialBuildingMembers[iSb];
}

// The per-CITY wonder-CATEGORY cap (json.md §4.4, enabler.md §8): a CultureLevel caps how many of a CATEGORY one
// city may hold -- distinct from the building's own self-cap, which limits how many of THAT building exist at a
// scope. The two read different data and both must hold.
//
// The building's CATEGORY is not a separate flag: json.md §4.4 says the self-cap's SCOPE is what makes it a world
// / team / national wonder, so it is derived from which cap the building authors -- no `isWorldWonder` mirror.
// The counts are the city's own raw category counts (ordinary state reads, not a derived verdict: the engine's
// own isWorldWondersMaxed() answer is exactly the kind of computed output a gate must not ride in on,
// docs/specs/validation.md §pollution guardrail (zero legacy ride-in)).
// Every building carrying a self-cap, i.e. every building the per-city CATEGORY cap can gate. Built once from
// static data (the bd_sbMembers precedent). This is the re-gate SET for the two facts that move a category
// verdict without referencing the building at all -- the city's culture level, and another wonder of the same
// category appearing here.
static std::vector<int> s_cappedBuildings;
static bool s_cappedBuildingsBuilt = false;
static const std::vector<int>& bd_cappedBuildings()
{
	if (!s_cappedBuildingsBuilt)
	{
		s_cappedBuildingsBuilt = true;
		for (int b = 0; b < GC.getNumBuildingInfos(); ++b)
		{
			const CvInfo* j = InfoRepo<CvBuildingInfo>::get().get(b);
			const CvAllowed* a = (j != NULL) ? j->getAllowed() : NULL;
			if (a == NULL) continue;
			if (a->cap(ALLOWEDCAP_WORLD) >= 0 || a->cap(ALLOWEDCAP_TEAM) >= 0 || a->cap(ALLOWEDCAP_EMPIRE) >= 0)
			{
				s_cappedBuildings.push_back(b);
			}
		}
	}
	return s_cappedBuildings;
}

static bool bd_categoryCapOk(int iB, const CvCity& kCity)
{
	// Two game options REMOVE the per-city category limit outright, and both gate HERE, at the consuming system,
	// while the info keeps serving ungated data (json.md §9). Neither RESCALES the limit, so there is
	// deliberately no curated cap variant to read for either.
	//   NO_WONDER_LIMIT      -- the player asked for no limit; it is the whole point of the option.
	//   CHALLENGE_ONE_CITY   -- no wonder limits at all (owner).
	if (GC.getGame().isOption(GAMEOPTION_NO_WONDER_LIMIT)
	||  GC.getGame().isOption(GAMEOPTION_CHALLENGE_ONE_CITY))
	{
		return true;
	}
	const CvInfo* j = InfoRepo<CvBuildingInfo>::get().get(iB);
	const CvAllowed* a = (j != NULL) ? j->getAllowed() : NULL;
	if (a == NULL)
	{
		return true;
	}
	const CvCultureLevelInfo& kLevel = GC.getCultureLevelInfo(kCity.getCultureLevel());
	// world / team / empire self-cap -> world / team / NATIONAL wonder, in that order.
	if (a->cap(ALLOWEDCAP_WORLD) >= 0)
	{
		return kCity.getNumWorldWonders() < kLevel.getMaxWorldWonders();
	}
	if (a->cap(ALLOWEDCAP_TEAM) >= 0)
	{
		return kCity.getNumTeamWonders() < kLevel.getMaxTeamWonders();
	}
	if (a->cap(ALLOWEDCAP_EMPIRE) >= 0)
	{
		return kCity.getNumNationalWonders() < kLevel.getMaxNationalWonders();
	}
	return true;   // uncapped building -- no category, so no category cap
}

static bool bd_groupCapOk(int iB, const CvPlayer& kPlayer)
{
	const SpecialBuildingTypes sb = (SpecialBuildingTypes)GC.getBuildingInfo((BuildingTypes)iB).getSpecialBuildingType();
	if (sb == NO_SPECIALBUILDING) return true;
	const CvInfo* jg = InfoRepo<CvSpecialBuildingInfo>::get().get((int)sb);
	const CvAllowed* a = (jg != NULL) ? jg->getAllowed() : NULL;
	if (a == NULL) return true;
	const std::vector<int>& mem = bd_sbMembers((int)sb);
	for (int iKind = 0; iKind < NUM_ALLOWEDCAP; ++iKind)
	{
		const int iCap = a->cap((EnAllowedCap)iKind);
		if (iCap < 0) continue;
		CascadeCountScope eScope;
		int iEntity;
		if (iKind == ALLOWEDCAP_WORLD)
		{
			eScope = CASCADE_COUNT_WORLD;
			iEntity = 0;
		}
		else if (iKind == ALLOWEDCAP_TEAM)
		{
			eScope = CASCADE_COUNT_TEAM;
			iEntity = (int)kPlayer.getTeam();
		}
		else if (iKind == ALLOWEDCAP_EMPIRE)
		{
			eScope = CASCADE_COUNT_EMPIRE;
			iEntity = (int)kPlayer.getID();
		}
		else
		{
			continue;
		}
		int iCount = 0;
		for (size_t iMember = 0; iMember < mem.size(); ++iMember)
		{
			iCount += CvCascadeTally::buildingCount(iEntity, mem[iMember], eScope);
		}
		if (iCount >= iCap) return false;
	}
	return true;
}

// The gate verdict for ONE candidate in ONE city: requiresMet (build ∧ operate, the ONE evaluator, the full
// city context incl. the waived-prereq set + the standing operating-buildings wiring), the DORMANT-TRIGGER
// check (building "replacement" is DORMANCY, enabler.md §2/§3 -- a candidate whose dormantTriggers successor
// is PRESENT in the city would be born dormant, so it gates out; the successor's EDGEF_REQUIRED_BY reverse
// edge re-gates it on the successor's flips), the allowed self-caps (world/team/empire, the tally's real
// buildings domain), the per-CITY wonder-CATEGORY cap from the city's CultureLevel, AND the
// SpecialBuilding GROUP cap above.
static void bd_gate(const CvCity& kCity, const CvPlayer& kPlayer, const std::set<int>& waived, EnablerDomain& d, int iB)
{
	const CvInfo* j = InfoRepo<CvBuildingInfo>::get().get(iB);
	bool bDormant = false;
	bool bReplacedHidden = false;
	if (j != NULL)
	{
		const std::vector<int>& dorm = j->dormantTriggers();
		for (size_t i = 0; i < dorm.size(); ++i)
			if (kCity.getCityContext().hasBuilding(dorm[i])) { bDormant = true; break; }   // the §7 has-list, through the context (contexts.md HAVE axis)
		// the HIDE-REPLACED interface option (the legacy canConstruct replacement leg, post-flip): with the
		// option on, a candidate whose dormancy successor is REACHABLE hides from the offer -- inTree is the
		// two-mode read the legacy recursive canConstruct(replacement, bTestVisible=true) maps onto. Freshness:
		// a source flipping the successor's membership re-gates this predecessor via the one-level REQUIRED_BY
		// expansion in bd_touched; the option toggle itself re-gates every city (CvPlayer::setModderOption).
		if (!bDormant && kPlayer.isModderOption(MODDEROPTION_HIDE_REPLACED_BUILDINGS))
		{
			for (size_t i = 0; i < dorm.size(); ++i)
				if (d.inTree(dorm[i])) { bReplacedHidden = true; break; }
		}
	}
	CvCascadeEvalCtx ec;
	kCity.getCityContext().fillEvalCtx(ec);       // city+plot -- the contexts fill the eval state (contexts.md)
	kPlayer.getEmpireContext().fillEvalCtx(ec);   // player+team
	ec.waivedPrereqBuildings = &waived;
	EnablerKernel::wireOperatingBuildings(&kCity, ec);
	CvCascadeEvalFlags gateFlags;
	gateFlags.strictStateReligionForBuild = true;
	//	WHICH clause refuses is what gets stored ([enabler.md] par.6) -- a bare bool cannot tell a capped-out
	//	wonder (nothing to be done: HIDE) from a missing resource (go connect it: GREY), so it leaves the player
	//	and the AI to guess which they are looking at.
	//	⚑ The building-only clauses are tested here and the shared trio defers to the ONE kernel implementation.
	//	Order decides only WHICH reason a multiply-refused candidate reports: the clauses nothing can be done
	//	about lead, so a wonder that is both capped and short a resource says the cap.
	unsigned char eReason = EnablerDomain::GATEREASON_NONE;
	if (bDormant)                         eReason = EnablerDomain::GATEREASON_DORMANT;
	else if (bReplacedHidden)             eReason = EnablerDomain::GATEREASON_REPLACED;
	else if (!bd_groupCapOk(iB, kPlayer)) eReason = EnablerDomain::GATEREASON_CAP_GROUP;
	else if (!bd_categoryCapOk(iB, kCity)) eReason = EnablerDomain::GATEREASON_CAP_CATEGORY;
	else eReason = EnablerKernel::standardGateReason(j, iB, kPlayer, ec, gateFlags, /*bUnit*/ false);
	d.setGateReason(iB, eReason);
	// QUEUED is the FRESH-OFFER exclusion (par.8, the legacy "!bContinue getFirstBuildingOrder"), NOT a gate reason:
	// a SEPARATE read-time overlay so canConstruct(bContinue=true)/canContinueProduction can see past it. Folding it
	// into the gate (as before) flipped a queued building to GREYED -> !listed -> canContinueProduction=false ->
	// doCheckProduction cancelled every in-progress build EACH TURN (progress lost). Live object-owned read;
	// re-run on SEVT_CITY_ORDER_CHANGED (onCityOrderChanged) + the load-end gate pass.
	d.setQueued(iB, kCity.getFirstBuildingOrder((BuildingTypes)iB) != -1);
}

// Gate a SET of candidate ids in one city (the touched set of one event / a class list): the waived-prereq
// set computes ONCE per call, non-members skip (a later entry event gates them then).
static void bd_gateSet(const CvCity& kCity, const std::set<int>& ids)
{
	EnablerDomain& d = kCity.m_enabler.buildings;
	if (!d.isSeeded() || ids.empty()) return;
	const CvPlayer& kPlayer = GET_PLAYER(kCity.getOwner());
	std::set<int> waived;
	BuildingEnabler::augmentWaived(kPlayer, GET_TEAM(kPlayer.getTeam()), waived);
	for (std::set<int>::const_iterator it = ids.begin(); it != ids.end(); ++it)
		if (d.inTree(*it)) bd_gate(kCity, kPlayer, waived, d, *it);
}

// The TOUCHED candidate set of one HAVE-event source (all read off the source's OWN info, O(delta)): its
// enables proposals, its removal-family targets, and its EDGEF_REQUIRED_BY building dependents (the readJson
// requires-reverse-index, docs/cascade.md §1 (reverse lookups are populated once, at load)).
static void bd_touched(const CvInfo* j, std::set<int>& touched)
{
	if (j == NULL) return;
	static const EnEdgeFamily FAMS[] = { EDGEF_ENABLES, EDGEF_OBSOLETES, EDGEF_REPLACES, EDGEF_DISABLES, EDGEF_REQUIRED_BY, NUM_EDGEF };
	for (int f = 0; FAMS[f] != NUM_EDGEF; ++f)
	{
		const std::vector<int>* p = j->edge(FAMS[f], EDGEB_BUILDINGS);
		if (p != NULL) touched.insert(p->begin(), p->end());
	}
	// ONE-LEVEL expansion: whoever REQUIRES a touched candidate re-gates too. The hide-replaced gate leg reads
	// the SUCCESSOR's tree membership, so a source that (un)reaches the successor must re-gate its
	// predecessors -- carried by the successor's own EDGEF_REQUIRED_BY (the dormant triggers invert there,
	// docs/cascade.md §1 (reverse lookups are populated once, at load); other requires-dependents re-gate idempotently).
	const std::set<int> firstOrder = touched;
	for (std::set<int>::const_iterator it = firstOrder.begin(); it != firstOrder.end(); ++it)
	{
		const CvInfo* jt = InfoRepo<CvBuildingInfo>::get().get(*it);
		if (jt == NULL) continue;
		const std::vector<int>* p = jt->edge(EDGEF_REQUIRED_BY, EDGEB_BUILDINGS);
		if (p != NULL) touched.insert(p->begin(), p->end());
	}
}

// The CLASS candidate lists (load-compiled once, game-thread static): for each event class with no FK reverse
// edge, the buildings whose requires (build ∧ operate) reference it -- EnablerKernel::scanCondDeps, the ONE
// dependency-signature scanner. GATE_DYNAMIC = the live non-HAVE atoms (the bounded per-turn re-check set).
static std::set<int> s_gateClass[BuildingEnabler::NUM_GATE_CLASSES];
// The PLOT-ATOM candidate index: (atom kind, atom id) -> the buildings whose requires names it. This is
// enabler.md par.8's "a coarse list matches a coarse event" -- the plot substrate is deliberately absent from
// EDGEF_REQUIRED_BY (CvReversePass routes none of its prefixes), so the reverse view it needs is compiled here.
static std::map<std::pair<int, int>, std::vector<int> > s_gatePlotAtomConsumers;
static bool s_gateClassBuilt = false;
static void bd_recordPlotAtoms(int eKind, const std::set<int>& ids, int b)
{
	for (std::set<int>::const_iterator it = ids.begin(); it != ids.end(); ++it)
	{
		s_gatePlotAtomConsumers[std::make_pair(eKind, *it)].push_back(b);
	}
}
void BuildingEnabler::clearCompiledIndexes()
{
	s_gateClassBuilt = false;
	for (int i = 0; i < BuildingEnabler::NUM_GATE_CLASSES; ++i) s_gateClass[i].clear();
	s_gatePlotAtomConsumers.clear();
	s_specialBuildingMembersBuilt = false;
	s_specialBuildingMembers.clear();
	s_cappedBuildingsBuilt = false;
	s_cappedBuildings.clear();
}

static void bd_buildGateClasses()
{
	if (s_gateClassBuilt) return;
	s_gateClassBuilt = true;
	for (int b = 0; b < GC.getNumBuildingInfos(); ++b)
	{
		const CvInfo* j = InfoRepo<CvBuildingInfo>::get().get(b);
		if (j == NULL) continue;
		CascadeCondDeps deps;
		EnablerKernel::scanCondDeps(j->requiresBuild(), deps, /*bTrackUnits*/ false, /*bMarkDynamic*/ true);
		EnablerKernel::scanCondDeps(j->requiresOperate(), deps, /*bTrackUnits*/ false, /*bMarkDynamic*/ true);
		if (deps.pop)           s_gateClass[BuildingEnabler::GATE_POP].insert(b);
		if (deps.power)         s_gateClass[BuildingEnabler::GATE_POWER].insert(b);
		if (deps.goldenAge)     s_gateClass[BuildingEnabler::GATE_GOLDEN_AGE].insert(b);
		if (deps.stateReligion) s_gateClass[BuildingEnabler::GATE_STATE_RELIGION].insert(b);
		if (deps.dynamic)       s_gateClass[BuildingEnabler::GATE_DYNAMIC].insert(b);
		bd_recordPlotAtoms(PLOTATOM_TERRAIN,     deps.terrains,       b);
		bd_recordPlotAtoms(PLOTATOM_FEATURE,     deps.features,       b);
		bd_recordPlotAtoms(PLOTATOM_IMPROVEMENT, deps.improvements,   b);
		bd_recordPlotAtoms(PLOTATOM_ROUTE,       deps.routes,         b);
		bd_recordPlotAtoms(PLOTATOM_MAPCATEGORY, deps.mapCategories,  b);
		bd_recordPlotAtoms(PLOTATOM_PREDICATE,   deps.plotPredicates, b);
	}
}

void BuildingEnabler::gateCity(const CvCity& kCity)
{
	EnablerDomain& d = kCity.m_enabler.buildings;
	if (!d.isSeeded()) return;
	const CvPlayer& kPlayer = GET_PLAYER(kCity.getOwner());
	std::set<int> waived;
	augmentWaived(kPlayer, GET_TEAM(kPlayer.getTeam()), waived);
	for (int b = 0; b < GC.getNumBuildingInfos(); ++b)
		if (d.inTree(b)) bd_gate(kCity, kPlayer, waived, d, b);
}

void BuildingEnabler::gateAllCities()
{
	for (int iP = 0; iP < MAX_PLAYERS; iP++)
	{
		const CvPlayer& kPlayer = GET_PLAYER((PlayerTypes)iP);
		foreach_(const CvCity* pCity, kPlayer.cities())
			gateCity(*pCity);
	}
}

void BuildingEnabler::onLoadFinished()
{
	// â THE OPERATING SET IS ALREADY BUILT BY THE TIME THIS RUNS, AND NOTHING SEEDS IT HERE.
	// It is filled by the in-read facts: each building announces its presence as it deserializes and resolves
	// its own dormancy, and every have axis (bonus / vicinity bonus / religion / corp / pop / power) re-checks
	// the consumers of what it supplies, so a manufactured chain lights tier by tier as the stream runs.
	// â A full recompute stood here and was deleted. It rebuilt a set the facts had already converged and
	// announced the whole thing at once -- which is why the in-read emit had to be suppressed to stop the
	// deposits double-applying, and why the CASCADE saw no operating verdict until after the load bracket while
	// the ENABLER had been event-built all along. The two must build on the SAME SEEDS (owner).
	// â Do not reintroduce one: the objects and their contexts exist before the facts flow (the game could not
	// load otherwise), so there is nothing for a rebuild to discover that the stream did not already carry
	// (docs/spine.md §5 (the load reseed), docs/cascade.md §A SELF-HEAL IS THE FOSSIL OF A MISSING EMIT).
	// the par.7.1 order rule's "gate once after the stream ends" option: every city's full gate pass, exactly
	// once, against the fully-loaded state (fires from GAME_LOAD_FINISHED at the end of onFinalInitialized).
	gateAllCities();
}

void BuildingEnabler::onCityGateClass(const CvCity& kCity, int eClass)
{
	if (spineGameLoadInProgress()) return;   // the load-end pass gates
	bd_buildGateClasses();
	bd_gateSet(kCity, s_gateClass[eClass]);
}

void BuildingEnabler::onPlayerGateClass(PlayerTypes ePlayer, int eClass)
{
	if (spineGameLoadInProgress() || ePlayer == NO_PLAYER) return;
	bd_buildGateClasses();
	CvPlayer& kPlayer = GET_PLAYER(ePlayer);
	foreach_(const CvCity* pCity, kPlayer.cities())
		bd_gateSet(*pCity, s_gateClass[eClass]);
}

// The tech delta. SEVT_TECH_CHANGED is documented BROAD (fires on any set), so the flip guard is the PLAYER tech
// domain's held flag -- which means this MUST run BEFORE TechEnabler::onTechChanged flips it (the invalidation
// route owns that ordering).
void BuildingEnabler::onCityTechChanged(TeamTypes eTeam, TechTypes eTech, bool bHas)
{
	if (eTeam == NO_TEAM || eTech == NO_TECH) return;
	const CvInfo* jt = bd_sourceInfo(AX_TECH, (int)eTech);
	const std::vector<int>* obsB = jt ? jt->edge(EDGEF_OBSOLETES, EDGEB_BUILDINGS) : NULL;
	const CvTeam& kTeam = GET_TEAM(eTeam);
	const bool bGate = !spineGameLoadInProgress();   // load gates once at GAME_LOAD_FINISHED
	std::set<int> touched;
	if (bGate) bd_touched(jt, touched);
	for (int iP = 0; iP < MAX_PLAYERS; iP++)
	{
		CvPlayer& kPlayer = GET_PLAYER((PlayerTypes)iP);
		if (kPlayer.getTeam() != eTeam) continue;   // seeded-but-dead players update too (a revival reads fresh)
		if (!kPlayer.m_enabler.techs.isSeeded() || kPlayer.m_enabler.techs.isHeld((int)eTech) == bHas) continue;   // the broad-emit flip guard (pre-TechEnabler)
		const int iDelta = bHas ? +1 : -1;
		foreach_(CvCity* pCity, kPlayer.cities())
		{
			EnablerDomain& d = pCity->m_enabler.buildings;
			if (!d.isSeeded()) continue;   // pre-init window: the object's own read/init emits replay its facts
			EnablerKernel::applyEdges(d, jt, EDGEB_BUILDINGS, iDelta);
			// the obsolete-present ripple: a PRESENT building this tech obsoletes stops (acquire) / resumes
			// (loss) enabling -- unless another held obsoleting tech already pins it obsolete
			if (obsB != NULL)
				for (size_t i = 0; i < obsB->size(); ++i)
				{
					const int b = (*obsB)[i];
					if (!pCity->hasBuilding((BuildingTypes)b)) continue;
					const CvInfo* jb = InfoRepo<CvBuildingInfo>::get().get(b);
					if (EnablerKernel::obsoletedByOtherHeldTech(jb, kTeam, eTech)) continue;
					EnablerKernel::applyEdges(d, jb, EDGEB_BUILDINGS, -iDelta);
					// ⚖ THE INSTANCE'S FATE, decided here because a TECH is the only thing that can obsolete
					// (owner): an EMPTY `whenObsolete` is a HARD REMOVAL, while a tree carrying modifiers leaves
					// the building standing for that tree to take over ([json.md §4.2], [enabler.md §3.2] -- the
					// enabler's own obsolete set is exactly the tree-carrying population). ⛔ No successor is
					// placed: what the retired culture-shell chain used to put here is what the curator now reads
					// to emit the tree on the building itself.
					if (bHas)
					{
						const CvModifiers* pWhenObsolete = jb->getWhenObsolete();
						if (pWhenObsolete == NULL || pWhenObsolete->empty())
						{
							pCity->changeHasBuilding((BuildingTypes)b, false);
						}
					}
				}
			if (bGate) bd_gateSet(*pCity, touched);   // gate-on-entry + the par.7.1 step-2 re-gates
		}
	}
}

void BuildingEnabler::onCityBuildingChanged(const CvCity& kCity, int iBuilding, bool bPresent)
{
	EnablerDomain& d = kCity.m_enabler.buildings;
	if (!d.isSeeded() || iBuilding < 0) return;
	if (d.isHeld(iBuilding) == bPresent) return;   // idempotency guard (the tech-domain idiom)
	d.setHeld(iBuilding, bPresent);
	const CvPlayer& kPlayer = GET_PLAYER(kCity.getOwner());
	const CvInfo* jb = InfoRepo<CvBuildingInfo>::get().get(iBuilding);
	if (!EnablerKernel::obsoletedByHeldTech(jb, GET_TEAM(kPlayer.getTeam())))
		EnablerKernel::applyEdges(d, jb, EDGEB_BUILDINGS, bPresent ? +1 : -1);
	if (!spineGameLoadInProgress())
	{
		// gate-on-entry + the step-2 re-gates in THIS city (building prereq atoms reference iBuilding)
		std::set<int> touched;
		bd_touched(jb, touched);
		bd_gateSet(kCity, touched);
		// the CAP CROSSING (par.7.1 step 3): a capped building's count changed -- re-gate IT on every seeded
		// city its cap scope reaches (a completed world wonder vanishes from every rival's list; the tally's
		// buildings domain carries the counts). A GROUP-capped member's crossing re-gates ALL its group
		// siblings (the grouped wonders: one member built consumes the shared slot). world/team caps reach ALL
		// players; empire reaches the owner's -- the widest reach is used (idempotent gate evals).
		const SpecialBuildingTypes eSb = (SpecialBuildingTypes)GC.getBuildingInfo((BuildingTypes)iBuilding).getSpecialBuildingType();
		const CvInfo* jg = (eSb != NO_SPECIALBUILDING) ? InfoRepo<CvSpecialBuildingInfo>::get().get((int)eSb) : NULL;
		const bool bGroupCapped = (jg != NULL && jg->getAllowed() != NULL);
		if ((jb != NULL && jb->getAllowed() != NULL) || bGroupCapped)
		{
			std::set<int> capSet;
			capSet.insert(iBuilding);
			if (bGroupCapped)
			{
				const std::vector<int>& mem = bd_sbMembers((int)eSb);
				capSet.insert(mem.begin(), mem.end());
			}
			for (int iP = 0; iP < MAX_PLAYERS; iP++)
			{
				CvPlayer& kP = GET_PLAYER((PlayerTypes)iP);
				foreach_(const CvCity* pCity, kP.cities())
					bd_gateSet(*pCity, capSet);
			}
			// THIS CITY additionally re-gates every capped building: the per-city CATEGORY cap counts the city's
			// wonders of a category, so one arriving can cap out every OTHER candidate of that category here --
			// candidates the cap-scope fan above never names, because it re-gates the building whose own COUNT
			// moved, not its category siblings.
			const std::vector<int>& categoryCapped = bd_cappedBuildings();
			std::set<int> categorySet(categoryCapped.begin(), categoryCapped.end());
			bd_gateSet(kCity, categorySet);
		}
	}
}

// One per-city axis flip + the play-time gate follow-up (gate-on-entry + the step-2 re-gates of the source's
// touched set). Inside the load bracket the GAME_LOAD_FINISHED pass gates instead.
static void bd_applyAxisGated(const CvCity& kCity, EnablerDomain& d, int eAxis, int iId, int iDelta)
{
	bd_applyAxis(d, eAxis, iId, iDelta);
	if (spineGameLoadInProgress()) return;
	std::set<int> touched;
	bd_touched(bd_sourceInfo(eAxis, iId), touched);
	bd_gateSet(kCity, touched);
}

// The empire's per-type building COUNT moved -> re-check the `allowed` SELF-CAP for that one type
// (par.7.1 step 3), across exactly the cities the cap's SCOPE reaches. The twin of
// UnitEnabler::onUnitCountChanged, and the reason the count fact exists at all.
// ⚠ DISTINCT from onCityBuildingChanged, which re-gates the DEPENDENTS of a building present in ONE city. A cap
// is cross-city by construction: a world wonder completed in city A must leave the buildable set of every OTHER
// city -- of every player, for a world cap -- and no per-city presence fact reaches them, so without this the
// wonder stays offered everywhere until something else happens to re-gate.
void BuildingEnabler::onBuildingCountChanged(PlayerTypes ePlayer, int eBuilding)
{
	if (spineGameLoadInProgress() || ePlayer == NO_PLAYER || eBuilding < 0) return;
	const CvInfo* j = InfoRepo<CvBuildingInfo>::get().get(eBuilding);
	const CvAllowed* a = (j != NULL) ? j->getAllowed() : NULL;
	if (a == NULL) return;
	const bool bWorldCap  = (a->cap(ALLOWEDCAP_WORLD)  >= 0);
	const bool bTeamCap   = (a->cap(ALLOWEDCAP_TEAM)   >= 0);
	const bool bEmpireCap = (a->cap(ALLOWEDCAP_EMPIRE) >= 0);
	if (!bWorldCap && !bTeamCap && !bEmpireCap) return;   // uncapped: its count moves no verdict anywhere
	std::set<int> one;
	one.insert(eBuilding);
	const TeamTypes eOwnerTeam = GET_PLAYER(ePlayer).getTeam();
	for (int iP = 0; iP < MAX_PLAYERS; iP++)
	{
		const PlayerTypes eP = (PlayerTypes)iP;
		// The cap's own scope decides the reach: a WORLD cap re-gates everyone, a TEAM cap the team, an EMPIRE
		// cap only the owner. Widening any of them would re-gate cities whose verdict cannot have moved.
		if (!bWorldCap)
		{
			if (bTeamCap)
			{
				if (GET_PLAYER(eP).getTeam() != eOwnerTeam) continue;
			}
			else if (eP != ePlayer)
			{
				continue;
			}
		}
		foreach_(const CvCity* pCity, GET_PLAYER(eP).cities())
		{
			bd_gateSet(*pCity, one);
		}
	}
}

// A PLOT SUBSTRATE entity arrived on / left a plot this city can work -- re-gate exactly the candidates whose
// `requires` REFERENCES that entity (par.7.1 step 2, the EDGEF_REQUIRED_BY re-gate), and nothing else.
// ⚑ THIS IS THE ROUTE THE PLOT PLANE NEVER HAD. The substrate atoms are the single largest gate axis in the
// authored data -- MAPCATEGORY_ / TERRAIN_ / FEATURE_ / HAS_COAST / HAS_RIVER across thousands of entities -- and
// none of them re-gated at all: a terraform, a chop, an improvement built or pillaged moved no verdict, and the
// tri-state is a BARE FETCH that nothing recomputes (docs/cascade.md §A SELF-HEAL IS THE FOSSIL OF A MISSING EMIT), so the stale verdict simply stood.
// ⛔ It must NOT ride the GATE_DYNAMIC class the rare player/city facts use: that class is effectively the whole
// registry (anything the deps scanner does not recognise falls through to it), and plot facts are HIGH frequency
// -- a worked-plot flip would re-gate every building in the city. The reverse edge is what makes this affordable.
// The plot-atom index's own census -- distinct atom keys, and the total candidate entries behind them. Reported
// at load so an index that compiled EMPTY says so, which is the one failure this route cannot show any other way.
void BuildingEnabler::plotAtomCensus(int& iKeysOut, int& iEntriesOut)
{
	bd_buildGateClasses();
	iKeysOut = (int)s_gatePlotAtomConsumers.size();
	iEntriesOut = 0;
	for (std::map<std::pair<int, int>, std::vector<int> >::const_iterator it = s_gatePlotAtomConsumers.begin();
	     it != s_gatePlotAtomConsumers.end(); ++it)
	{
		iEntriesOut += (int)it->second.size();
	}
}

// The GATE-CLASS census: how many candidates each coarse class holds, and the registry it is drawn from.
// ⚑ It exists because the class SIZE decides whether routing a fact through one is affordable at all -- a class
// holding most of the registry makes every fact that names it a whole-registry re-gate, which is the shape
// par.7.1's "small load-compiled set" rules out. Without this the size is invisible, so a class quietly widening
// (an axis that gained a precise route but kept marking the catch-all) is unobservable -- the plot-atom census
// beside it exists for the same reason, one plane over.
void BuildingEnabler::gateClassCensus(int (&aiCountsOut)[NUM_GATE_CLASSES], int& iTotalOut)
{
	bd_buildGateClasses();
	for (int iClass = 0; iClass < NUM_GATE_CLASSES; ++iClass)
	{
		aiCountsOut[iClass] = (int)s_gateClass[iClass].size();
	}
	iTotalOut = GC.getNumBuildingInfos();
}

void BuildingEnabler::onPlotAtomChanged(const CvCity& kCity, int eKind, int iId)
{
	if (iId < 0) return;
	bd_buildGateClasses();
	std::vector<std::pair<int, int> > atoms;
	EnablerKernel::plotAtomSeeds(eKind, iId, atoms);   // a TERRAIN fact also seeds its map categories
	std::set<int> touched;
	for (size_t iAtom = 0; iAtom < atoms.size(); ++iAtom)
	{
		std::map<std::pair<int, int>, std::vector<int> >::const_iterator it = s_gatePlotAtomConsumers.find(atoms[iAtom]);
		if (it != s_gatePlotAtomConsumers.end()) touched.insert(it->second.begin(), it->second.end());
	}
	bd_gateSet(kCity, touched);   // no-ops on an empty set, so an atom nothing requires costs one lookup
}

void BuildingEnabler::onCityOrderChanged(const CvCity& kCity, int iBuilding)
{
	// queue push/pop of iBuilding (par.7.1 step 3): membership untouched -- ONE id re-gates, its verdict
	// reading the live queue (bd_gate's getFirstBuildingOrder conjunct). Load-window emits never occur
	// (the queue deserializes without pushOrder; the load-end gate pass reads it).
	if (iBuilding < 0) return;
	std::set<int> one;
	one.insert(iBuilding);
	bd_gateSet(kCity, one);
}

void BuildingEnabler::onCityReligionChanged(const CvCity& kCity, int iReligion, bool bHas)
{
	EnablerDomain& d = kCity.m_enabler.buildings;
	if (!d.isSeeded()) return;   // the emit is flip-guarded (setHasReligion) -- the delta applies directly
	bd_applyAxisGated(kCity, d, AX_RELIGION, iReligion, bHas ? +1 : -1);
}

void BuildingEnabler::onCityCorporationChanged(const CvCity& kCity, int iCorporation, bool bHas)
{
	EnablerDomain& d = kCity.m_enabler.buildings;
	if (!d.isSeeded()) return;   // flip-guarded emit (setHasCorporation)
	bd_applyAxisGated(kCity, d, AX_CORP, iCorporation, bHas ? +1 : -1);
}

void BuildingEnabler::onCityBonusChanged(const CvCity& kCity, int iBonus, int iChange)
{
	EnablerDomain& d = kCity.m_enabler.buildings;
	if (!d.isSeeded() || iBonus < 0 || iChange == 0) return;
	// GATE-ONLY (owner ruling 2026-07-15): a plot-group-carried bonus NEVER drives tree membership -- its
	// enables edges are the reverse-mapped gate view, and the requires atom (retained target-side) is the one
	// authority. A network crossing therefore re-gates the bonus's dependents; membership rides the
	// tech/building/civic edges + the root (bonus-only entities root, curator-derived).
	const int iNew = kCity.getNumBonuses((BonusTypes)iBonus);
	const int iOld = iNew - iChange;
	if ((iOld > 0) == (iNew > 0)) return;   // HAVE = count > 0: re-gate only on a presence crossing
	if (spineGameLoadInProgress()) return;  // load: the one GAME_LOAD_FINISHED gate pass covers it
	std::set<int> touched;
	bd_touched(bd_sourceInfo(AX_BONUS, iBonus), touched);
	bd_gateSet(kCity, touched);
}

// The LOCAL-presence twin: vicinity is a plain "we have it here" fact (never an owned count -- that lives on
// the plot group), so a flip is a pure gate re-check of the bonus's dependents, same gate-only shape.
void BuildingEnabler::onCityVicinityBonusChanged(const CvCity& kCity, int iBonus)
{
	EnablerDomain& d = kCity.m_enabler.buildings;
	if (!d.isSeeded() || iBonus < 0) return;
	if (spineGameLoadInProgress()) return;  // load: the one GAME_LOAD_FINISHED gate pass covers it
	std::set<int> touched;
	bd_touched(bd_sourceInfo(AX_BONUS, iBonus), touched);
	bd_gateSet(kCity, touched);
}

// ONE LEVEL PER CALL, signed -- a level MOVE is the REMOVED of the old beside the ADDED of the new, so this no
// longer takes both ends. The capped set re-gates on either end because the per-city wonder-CATEGORY cap reads
// the level's max, and re-gating it twice across a move is idempotent.
void BuildingEnabler::onCityCultureLevelChanged(const CvCity& kCity, int iLevel, int iCrossing)
{
	EnablerDomain& d = kCity.m_enabler.buildings;
	if (!d.isSeeded() || iLevel < 0 || iCrossing == 0) return;
	if (iCrossing > 0)
	{
		bd_applyAxisGated(kCity, d, AX_CULTURE, iLevel, +1);
	}
	else
	{
		bd_applyAxis(d, AX_CULTURE, iLevel, -1);
	}
	if (!spineGameLoadInProgress())
	{
		std::set<int> touched;
		bd_touched(bd_sourceInfo(AX_CULTURE, iLevel), touched);
		// ...plus every CAPPED building here: the per-city wonder-CATEGORY cap reads the culture level's max, so a
		// level change moves that verdict for candidates referencing the level NOWHERE -- the touched set above
		// cannot reach them, and an unrouted gate input is a permanently stale verdict (docs/cascade.md §A SELF-HEAL IS THE FOSSIL OF A MISSING EMIT).
		const std::vector<int>& capped = bd_cappedBuildings();
		touched.insert(capped.begin(), capped.end());
		bd_gateSet(kCity, touched);
	}
}

void BuildingEnabler::onPlayerCivicsChanged(PlayerTypes ePlayer, int iOldCivic, int iNewCivic)
{
	if (ePlayer == NO_PLAYER || iOldCivic == iNewCivic) return;
	CvPlayer& kPlayer = GET_PLAYER(ePlayer);
	const bool bGate = !spineGameLoadInProgress();
	std::set<int> touched;
	if (bGate)
	{
		bd_touched(bd_sourceInfo(AX_CIVIC, iOldCivic), touched);   // both swap halves' dependents re-gate
		bd_touched(bd_sourceInfo(AX_CIVIC, iNewCivic), touched);
	}
	foreach_(CvCity* pCity, kPlayer.cities())
	{
		EnablerDomain& d = pCity->m_enabler.buildings;
		if (!d.isSeeded()) continue;
		bd_applyAxis(d, AX_CIVIC, iOldCivic, -1);   // the emit now carries the swapped-out civic (emit-surface fix)
		bd_applyAxis(d, AX_CIVIC, iNewCivic, +1);
		if (bGate) bd_gateSet(*pCity, touched);
	}
}

