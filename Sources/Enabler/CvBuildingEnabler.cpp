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

// THE ORACLE CORE (never a lifecycle path): the pure full build from current state -- replay the +1 appliers
// over the object-owned has-lists into the TARGET struct. verifyCity fresh-seeds a local oracle domain and
// diffs it against the event-maintained vector; that diff is the missed-emit tripwire (the enabler consumes
// ONLY events precisely so a missed emit surfaces as a visibly wrong enabler).
static void bd_seedInto(CityEnabler& en, const CvCity& kCity)
{
	const CvPlayer& kPlayer = GET_PLAYER(kCity.getOwner());
	const CvTeam& kTeam = GET_TEAM(kPlayer.getTeam());
	EnablerDomain& d = en.buildings;
	d.init(GC.getNumBuildingInfos());
	for (int b = 0; b < GC.getNumBuildingInfos(); ++b)
	{
		const CvBuildingInfo* jb = (const CvBuildingInfo*)InfoRepo<CvBuildingInfo>::get().get(b);
		if (jb != NULL && jb->isNotConstructible()) d.setStaticExcluded(b, true);
		if (kCity.hasBuilding((BuildingTypes)b))
		{
			d.setHeld(b, true);
			// a tech-obsoleted PRESENT building stays held but does not enable (the obsoletion flip)
			if (!EnablerKernel::obsoletedByHeldTech(jb, kTeam)) bd_applyAxis(d, BuildingEnabler::AX_BUILDING, b, +1);
		}
	}
	// the TECH_GAME_START root IS a held engine tech (the load backfill guarantees it) -- covered by this loop
	for (int t = 0; t < GC.getNumTechInfos(); ++t)
		if (kTeam.isHasTech((TechTypes)t)) bd_applyAxis(d, BuildingEnabler::AX_TECH, t, +1);
	for (int co = 0; co < GC.getNumCivicOptionInfos(); ++co)
	{
		const CivicTypes c = kPlayer.getCivics((CivicOptionTypes)co);
		if (c != NO_CIVIC) bd_applyAxis(d, BuildingEnabler::AX_CIVIC, (int)c, +1);
	}
	for (int r = 0; r < GC.getNumReligionInfos(); ++r)
		if (kCity.isHasReligion((ReligionTypes)r)) bd_applyAxis(d, BuildingEnabler::AX_RELIGION, r, +1);
	for (int c = 0; c < GC.getNumCorporationInfos(); ++c)
		if (kCity.isHasCorporation((CorporationTypes)c)) bd_applyAxis(d, BuildingEnabler::AX_CORP, c, +1);
	// no bonus axis: a plot-group-carried bonus is GATE-ONLY, never membership (owner ruling 2026-07-15)
	if (kCity.getCultureLevel() != NO_CULTURELEVEL)
		bd_applyAxis(d, BuildingEnabler::AX_CULTURE, (int)kCity.getCultureLevel(), +1);
}

// The CITY-CREATED applier (founding init + the load read's start, BEFORE the city's own in-read emits): init
// the domain (size + the notConstructible static exclusions) and fold ONLY the cross-scope HAVE that predates
// the city -- team techs + player civics (no events can carry 400 pre-existing techs to a new city). The
// city's OWN facts (buildings/religions/corps/bonuses/culture) arrive as DOMAIN events -- at load from the
// in-read reseed emits, at founding from the real grant/build emits -- one mechanism (DEC-spine-reseed).
void BuildingEnabler::onCityCreated(const CvCity& kCity)
{
	const CvPlayer& kPlayer = GET_PLAYER(kCity.getOwner());
	const CvTeam& kTeam = GET_TEAM(kPlayer.getTeam());
	EnablerDomain& d = kCity.m_enabler.buildings;
	d.init(GC.getNumBuildingInfos());
	for (int b = 0; b < GC.getNumBuildingInfos(); ++b)
	{
		const CvBuildingInfo* jb = (const CvBuildingInfo*)InfoRepo<CvBuildingInfo>::get().get(b);
		if (jb != NULL && jb->isNotConstructible()) d.setStaticExcluded(b, true);
	}
	// the TECH_GAME_START root IS a held engine tech (the load backfill guarantees it) -- covered by this loop
	for (int t = 0; t < GC.getNumTechInfos(); ++t)
		if (kTeam.isHasTech((TechTypes)t)) bd_applyAxis(d, AX_TECH, t, +1);
	for (int co = 0; co < GC.getNumCivicOptionInfos(); ++co)
	{
		const CivicTypes c = kPlayer.getCivics((CivicOptionTypes)co);
		if (c != NO_CIVIC) bd_applyAxis(d, AX_CIVIC, (int)c, +1);
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
// [DEC-calc-zero-ride-in]).
// Every building carrying a self-cap, i.e. every building the per-city CATEGORY cap can gate. Built once from
// static data (the bd_sbMembers precedent). This is the re-gate SET for the two facts that move a category
// verdict without referencing the building at all -- the city's culture level, and another wonder of the same
// category appearing here.
static const std::vector<int>& bd_cappedBuildings()
{
	static std::vector<int> s_capped;
	static bool s_built = false;
	if (!s_built)
	{
		s_built = true;
		for (int b = 0; b < GC.getNumBuildingInfos(); ++b)
		{
			const CvInfo* j = InfoRepo<CvBuildingInfo>::get().get(b);
			const CvAllowed* a = (j != NULL) ? j->getAllowed() : NULL;
			if (a == NULL) continue;
			if (a->cap(ALLOWEDCAP_WORLD) >= 0 || a->cap(ALLOWEDCAP_TEAM) >= 0 || a->cap(ALLOWEDCAP_EMPIRE) >= 0)
			{
				s_capped.push_back(b);
			}
		}
	}
	return s_capped;
}

static bool bd_categoryCapOk(int iB, const CvCity& kCity)
{
	// ONE CITY CHALLENGE: no wonder limits at all (owner). It is an ordinary game option like any other, so it
	// gates HERE, at the consuming system, and the info keeps serving ungated data (json.md §9). There is
	// deliberately NO curated OCC cap variant to read -- the option does not RESCALE the limit, it removes it.
	if (GC.getGame().isOption(GAMEOPTION_CHALLENGE_ONE_CITY))
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
			iCount += cascadeTally().buildingCount(iEntity, mem[iMember], eScope);
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
	d.setGateFailed(iB, bDormant
	                 || bReplacedHidden
	                 || (j != NULL && !cascadeGateOk(j->getGate(), ec, gateFlags))   // entity-level enabled/disabled (DEC-entity-gate)
	                 || !EnablerKernel::requiresMet(j, ec)
	                 || !EnablerKernel::allowedOk(j, iB, kPlayer, /*bUnit*/ false)
	                 || !bd_groupCapOk(iB, kPlayer)
	                 || !bd_categoryCapOk(iB, kCity));
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
// requires-reverse-index, DEC-one-reverse-view).
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
	// DEC-one-reverse-view; other requires-dependents re-gate idempotently).
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
static bool s_gateClassBuilt = false;
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

void BuildingEnabler::onCityTurn(const CvCity& kCity)
{
	if (spineGameLoadInProgress()) return;
	bd_buildGateClasses();
	bd_gateSet(kCity, s_gateClass[GATE_DYNAMIC]);
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

void BuildingEnabler::onCityCultureLevelChanged(const CvCity& kCity, int iOldLevel, int iNewLevel)
{
	EnablerDomain& d = kCity.m_enabler.buildings;
	if (!d.isSeeded() || iOldLevel == iNewLevel) return;
	bd_applyAxis(d, AX_CULTURE, iOldLevel, -1);
	bd_applyAxisGated(kCity, d, AX_CULTURE, iNewLevel, +1);   // one gate pass over both levels' touched... the new level's; the old level's dependents ride its REQUIRED_BY below
	if (!spineGameLoadInProgress())
	{
		std::set<int> touched;
		bd_touched(bd_sourceInfo(AX_CULTURE, iOldLevel), touched);
		// ...plus every CAPPED building here: the per-city wonder-CATEGORY cap reads the culture level's max, so a
		// level change moves that verdict for candidates referencing the level NOWHERE -- the touched set above
		// cannot reach them, and an unrouted gate input is a permanently stale verdict ([DEC-no-self-heal]).
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

int BuildingEnabler::verifyCity(const CvCity& kCity, std::string& sDiff)
{
	CityEnabler& en = kCity.m_enabler;
	if (!en.buildings.isSeeded())   // an uninitialized domain is itself the finding -- never quietly built here
	{
		sDiff += "domain not initialized; ";
		return GC.getNumBuildingInfos();
	}
	CityEnabler fresh;
	bd_seedInto(fresh, kCity);
	int iMismatch = 0;
	for (int b = 0; b < GC.getNumBuildingInfos(); ++b)
	{
		// MEMBERSHIP compare (inTree), not state: the fresh oracle is enable-side only (no gate flags), so the
		// maintained GREYED/LISTED split is the gate's -- the diff verifies the event maintenance, the tripwire.
		if (en.buildings.inTree(b) == fresh.buildings.inTree(b)) continue;
		++iMismatch;
		if (iMismatch <= 8)
		{
			sDiff += CvString::format("%s maintainedInTree=%d freshInTree=%d; ", GC.getBuildingInfo((BuildingTypes)b).getType(),
				(int)en.buildings.inTree(b), (int)fresh.buildings.inTree(b)).GetCString();
		}
	}
	return iMismatch;
}
