//
//	BuildingCascade -- StoneBase CalculateBuildableBuildings.cs (see the header). Ported VERBATIM from
//	CvCascadeEnabler.cpp's file-static en_augmentWaived / en_buildingCapped / en_scaledPrereq / en_buildingBuildable;
//	promoted to a declared surface (the single-source law, patterns.md). LOGIC unchanged: only the signatures + the
//	EnablerKernel-qualified call sites were rewritten.
//

#include "CvGameCoreDLL.h"
#include "CvCascadeBuildingCascade.h"
#include "CvCascadeEnablerKernel.h"    // EnablerKernel::obsoletedByHeldTech
#include "CvJsonInfo.h"
#include "CvJsonBuildingInfo.h"       // notConstructible (the cascade's own never-buildable flag; self-containment)
#include "Repos/InfoRepo.h"
#include "CvCascadeTally.h"
#include "AI/CvPlayerAI.h"            // GET_PLAYER
#include "AI/CvTeamAI.h"             // GET_TEAM
#include "Defines/CvGlobals.h"
#include "Engine/CvCity.h"
#include "Engine/CvPlayer.h"
#include "Engine/CvTeam.h"
#include "CvCascadeConditionEval.h"   // cascadeEvalCondition
#include "CvJsonCondition.h"       // the condition tree (reverse-index scan)
#include "CvCascadeAccumulator.h"     // CascadeAccumulator::CascadeHaveKind (recheckHave dispatch)
#include "Infos/CvBuildingInfo.h"
#include "Infos/CvSpecialBuildingInfo.h"   // getMaxPlayerInstances (special-building group cap)
#include "Infos/CvWorldInfo.h"             // getBuildingPrereqModifier (the raw world scalar for ScaledPrereq)
#include "Infos/CvCivicInfo.h"
#include "Engine/CvMap.h"
#include "Engine/CvGame.h"

// AugmentState's prereq-WAIVER set (StoneBase BuildingCascade.AugmentState: ObsoleteBuildings ∪ PrereqWaivedBuildings):
// a BUILDING is a waived prereq iff its OBSOLETE tech is held by the team, OR its SpecialBuilding group is made
// not-required by an adopted civic (enables.specialBuildingsWaived). Shared by the building + unit cascades (both gate
// requires.build through the SAME evaluator). The vicinity-supply + gov-center AugmentState operating buildings are read LIVE by the
// evaluator (hasVicinityBonus / isGovernmentCenter), so only the waived set is materialized here.
void BuildingCascade::augmentWaived(const CvPlayer& kPlayer, const CvTeam& kTeam, std::set<int>& waived)
{
	const int nB = GC.getNumBuildingInfos();
	for (int b = 0; b < nB; ++b)   // obsolete-by-held-tech
	{
		const TechTypes obs = GC.getBuildingInfo((BuildingTypes)b).getObsoleteTech();
		if (obs != NO_TECH && kTeam.isHasTech(obs)) waived.insert(b);
	}
	std::set<int> waivedSpecials;   // the SpecialBuilding groups the player's adopted civics make not-required
	for (int co = 0; co < GC.getNumCivicOptionInfos(); ++co)
	{
		const CivicTypes c = kPlayer.getCivics((CivicOptionTypes)co);
		if (c == NO_CIVIC) continue;
		const CvJsonInfo* j = InfoRepo<CvCivicInfo>::get().get((int)c);
		if (j == NULL) continue;
		const std::vector<int>* p = j->edge("enables.specialBuildingsWaived");
		if (p != NULL)
			for (size_t i = 0; i < p->size(); ++i) waivedSpecials.insert((*p)[i]);
	}
	if (!waivedSpecials.empty())
		for (int b = 0; b < nB; ++b)
		{
			const SpecialBuildingTypes sb = GC.getBuildingInfo((BuildingTypes)b).getSpecialBuilding();
			if (sb != NO_SPECIALBUILDING && waivedSpecials.count((int)sb) != 0) waived.insert(b);
		}
}

// Instance cap (StoneBase Capped): the entity is maxed at some scope -- current tally count + in-production making >=
// allowed. Reads the cascade's own allowed (CvJsonInfo) + tally + the live making, NOT the engine's isBuildingMaxedOut
// (that would be tautological vs canConstruct -- the shadow must validate the cascade's OWN count).
bool BuildingCascade::capped(const CvJsonInfo* j, int eB, const CvPlayer& kPlayer)
{
	if (j == NULL) return false;
	const CvJsonAllowed* a = j->getAllowed();
	if (a == NULL) return false;
	const int making = kPlayer.getBuildingMaking((BuildingTypes)eB);   // the player's in-production count
	for (std::map<std::string, int>::const_iterator it = a->all().begin(); it != a->all().end(); ++it)
	{
		CascadeCountScope sc; int ent;
		if (it->first == "world")       { sc = CASCADE_COUNT_WORLD;  ent = 0; }
		else if (it->first == "team")   { sc = CASCADE_COUNT_TEAM;   ent = (int)kPlayer.getTeam(); }
		else if (it->first == "empire") { sc = CASCADE_COUNT_EMPIRE; ent = (int)kPlayer.getID(); }
		else continue;   // category caps (worldWonders/...) live on CultureLevel -- a follow-on
		if (cascadeTally().buildingCount(ent, eB, sc) + making >= it->second) return true;
	}
	return false;
}

// ScaledPrereq (StoneBase BuildingCascade.ScaledPrereq, VERBATIM): the required count of a PrereqNumOfBuildings prereq --
// world-size-scaled (getModifiedIntValue: wsMod>0 -> *(100+m)/100; wsMod<0 -> *100/(100-m)), then *(1+selfCount) unless
// SELF is a limited wonder; bypassed (= base) if SELF is forceNoPrereqScaling OR the PREREQ is a limited wonder. This is
// a faithful TRANSCRIPTION of the legacy CvPlayer::getBuildingPrereqBuilding math -- ported, NOT called (the legacy
// method does not understand the cascade). CHALLENGE_ONE_CITY is omitted, as StoneBase omits it.
int BuildingCascade::scaledPrereq(int baseN, int wsMod, bool selfLimited, bool prereqLimited, bool selfNoScale, int selfCount)
{
	if (baseN < 1) return 0;
	if (selfNoScale || prereqLimited) return baseN;
	int req = wsMod > 0 ? baseN * (100 + wsMod) / 100 : (wsMod < 0 ? baseN * 100 / (100 - wsMod) : baseN);
	if (!selfLimited) req *= (1 + selfCount);
	return std::max(1, req);
}

// ===========================================================================================================
// The isolated-box REVERSE INDICES (owner 2026-07-05, "the biggest perf win"). At LOAD we invert every building's
// requires tree into "HAVE-atom kind -> {building ids that depend on it}", so a HAVE-change (pop / religion / corp
// / power / golden-age / a specific tech or bonus) re-checks ONLY its bucket via the shared bc_isBuildable, instead
// of a full-frontier re-walk. This is the DEPENDENCY MAP; the targeted-update that consumes it is the next piece.
// SAFE-BY-DESIGN: over-inclusion only costs a few extra re-checks; the slice-boundary full rebuild remains the net.
// ===========================================================================================================
namespace {

// The whole-building dependency signature (which HAVE-classes gate this building at all).
struct BcDeps
{
	bool pop, religion, corp, power, goldenAge, stateReligion, civicAny;
	std::set<int> techs;      // specific TECH_ ids referenced
	std::set<int> bonuses;    // specific BONUS_ ids referenced
	std::set<int> buildings;  // specific BUILDING_ ids referenced in requires (prereqs) -- for the completion re-check
	BcDeps() : pop(false), religion(false), corp(false), power(false), goldenAge(false), stateReligion(false), civicAny(false) {}
};

// Recursively collect the HAVE-atoms a condition (build or operate) references into `d`. Walks the GROUP children
// (all/anyOf/noneOf) + enabled/disabled; classifies PRESENCE by type prefix/token and PREDICATE by predKind.
static void bc_scanCond(const CvJsonCondition* c, BcDeps& d)
{
	if (c == NULL) return;
	if (c->kind == CASC_COND_GROUP)
	{
		for (size_t i = 0; i < c->all.size(); ++i)    bc_scanCond(c->all[i], d);
		for (size_t i = 0; i < c->anyOf.size(); ++i)  bc_scanCond(c->anyOf[i], d);
		for (size_t i = 0; i < c->noneOf.size(); ++i) bc_scanCond(c->noneOf[i], d);
		bc_scanCond(c->enabled, d);
		bc_scanCond(c->disabled, d);
		return;
	}
	if (c->kind == CASC_COND_PRESENCE)
	{
		const std::string& t = c->type;
		if (t == "POPULATION") d.pop = true;
		else if (t.compare(0, 5, "TECH_") == 0)  { if (c->id >= 0) d.techs.insert(c->id); }
		else if (t.compare(0, 6, "BONUS_") == 0) { if (c->id >= 0) d.bonuses.insert(c->id); }
		else if (t.compare(0, 6, "CIVIC_") == 0) d.civicAny = true;
		else if (t.compare(0, 9, "RELIGION_") == 0) d.religion = true;
		else if (t.compare(0, 12, "CORPORATION_") == 0) d.corp = true;
		else if (t.compare(0, 9, "BUILDING_") == 0) { if (c->id >= 0) d.buildings.insert(c->id); }
		return;
	}
	if (c->kind == CASC_COND_PREDICATE)
	{
		switch (c->predKind)
		{
		case CASC_PRED_HAS_POWER:               d.power = true; break;
		case CASC_PRED_IS_GOLDEN_AGE:           d.goldenAge = true; break;
		case CASC_PRED_HAS_CORPORATION:         d.corp = true; break;
		case CASC_PRED_HAS_RELIGION:
		case CASC_PRED_IS_HOLY_CITY:            d.religion = true; break;
		case CASC_PRED_HAS_STATE_RELIGION:
		case CASC_PRED_STATE_RELIGION_IN_CITY:
		case CASC_PRED_STATE_RELIGION:
		case CASC_PRED_IS_STATE_RELIGION_HOLY_CITY: d.stateReligion = true; break;
		case CASC_PRED_HAS_BONUS:               if (c->id >= 0) d.bonuses.insert(c->id); break;
		default: break;   // plot/domain predicates don't gate a building's city-scope buildability re-check here
		}
		return;
	}
}

// The buckets (building-id lists), built once. Over-inclusive is fine; a missed class self-heals at the boundary.
static bool s_bcIdxBuilt = false;
static std::vector<int> s_bcPop, s_bcReligion, s_bcCorp, s_bcPower, s_bcGolden, s_bcStateRel, s_bcCivic;
static std::map<int, std::vector<int> > s_bcTech, s_bcBonus;
static std::map<int, std::vector<int> > s_bcBuildingDeps;   // completedBuildingId -> {buildings that require it}

static void bc_buildIndices()
{
	if (s_bcIdxBuilt) return;
	s_bcIdxBuilt = true;
	const int nB = GC.getNumBuildingInfos();
	for (int b = 0; b < nB; ++b)
	{
		const CvJsonInfo* j = InfoRepo<CvBuildingInfo>::get().get(b);
		if (j == NULL) continue;
		BcDeps d;
		bc_scanCond(j->requiresBuild(), d);
		bc_scanCond(j->requiresOperate(), d);
		// the amount-prereq buildings (getPrereqNumOfBuildings) also make `b` depend on those buildings' counts
		const IDValueMap<BuildingTypes, int>& amtPrereqs = GC.getBuildingInfo((BuildingTypes)b).getPrereqNumOfBuildings();
		for (IDValueMap<BuildingTypes, int>::const_iterator it = amtPrereqs.begin(); it != amtPrereqs.end(); ++it)
			d.buildings.insert((int)it->first);
		for (std::set<int>::const_iterator it = d.buildings.begin(); it != d.buildings.end(); ++it) s_bcBuildingDeps[*it].push_back(b);
		if (d.pop)           s_bcPop.push_back(b);
		if (d.religion)      s_bcReligion.push_back(b);
		if (d.corp)          s_bcCorp.push_back(b);
		if (d.power)         s_bcPower.push_back(b);
		if (d.goldenAge)     s_bcGolden.push_back(b);
		if (d.stateReligion) s_bcStateRel.push_back(b);
		if (d.civicAny)      s_bcCivic.push_back(b);
		for (std::set<int>::const_iterator it = d.techs.begin(); it != d.techs.end(); ++it) s_bcTech[*it].push_back(b);
		for (std::set<int>::const_iterator it = d.bonuses.begin(); it != d.bonuses.end(); ++it) s_bcBonus[*it].push_back(b);
	}
}

} // namespace

// The isolated-box PRIMITIVE (owner 2026-07-05): is ONE building buildable in `pCity` RIGHT NOW? This is the EXACT
// per-building prune+gate chain the full buildable() fill applies -- extracted so the incremental targeted
// subset-recheck (the box's cheap update) and the full rebuild share ONE source of truth and can never diverge in
// per-building logic. The per-city setup (waived/ec/wireOperatingBuildings/queued/specialCount/wsMod) is computed ONCE by the
// caller and passed in. Any change to a prune/gate here is inherited by BOTH the full rebuild and the box update.
static bool bc_isBuildable(int b, const CvCity* pCity, const CvPlayer& kPlayer, const CvTeam& kTeam,
	CvCascadeEvalCtx& ec, const CvCascadeEvalFlags& buildFlags, const CvCascadeEvalFlags& operFlags,
	const std::set<int>& queued, const std::map<int, int>& specialCount, int wsMod)
{
	const BuildingTypes eB = (BuildingTypes)b;
	const CvBuildingInfo& bi = GC.getBuildingInfo(eB);
	const CvJsonInfo* j = InfoRepo<CvBuildingInfo>::get().get(b);
	if (EnablerKernel::obsoletedByHeldTech(j, kTeam)) return false;      // PRUNE: tech-obsolescence (obsoletedBy.techs)
	if (pCity->hasBuilding(eB)) return false;                           // EXCLUDE: already built in this city
	if (queued.count(b) != 0) return false;                            // EXCLUDE: already in this city's production queue
	{
		const CvJsonBuildingInfo* jb = (const CvJsonBuildingInfo*)j;
		if (jb != NULL && jb->notConstructible) return false;          // EXCLUDE never-buildable (identity.notConstructible)
	}
	if (BuildingCascade::capped(j, b, kPlayer)) return false;          // INSTANCE CAP (created + making >= allowed)
	const SpecialBuildingTypes sb = bi.getSpecialBuilding();
	if (sb != NO_SPECIALBUILDING)
	{
		const int cap = GC.getSpecialBuildingInfo(sb).getMaxPlayerInstances();
		std::map<int, int>::const_iterator scIt = specialCount.find((int)sb);
		const int cnt = (scIt != specialCount.end()) ? scIt->second : 0;
		if (cap != -1 && cnt >= cap) return false;                     // SPECIALBUILDING GROUP CAP (player scope)
	}
	bool bornDormant = false;
	if (j != NULL)
	{
		const std::vector<int>& dorm = j->dormantTriggers();
		for (size_t i = 0; i < dorm.size() && !bornDormant; ++i)
			if (pCity->hasBuilding((BuildingTypes)dorm[i])) bornDormant = true;
	}
	if (bornDormant) return false;                                     // DORMANT-ON-BUILD (successor present)
	const bool selfLimited = (j != NULL && j->getAllowed() != NULL && !j->getAllowed()->isEmpty());
	const bool selfNoScale = bi.isForceNoPrereqScaling();
	const int selfCount = cascadeTally().buildingCount((int)kPlayer.getID(), b, CASCADE_COUNT_EMPIRE);
	const IDValueMap<BuildingTypes, int>& prereqs = bi.getPrereqNumOfBuildings();
	for (IDValueMap<BuildingTypes, int>::const_iterator it = prereqs.begin(); it != prereqs.end(); ++it)
	{
		const CvJsonInfo* pj = InfoRepo<CvBuildingInfo>::get().get((int)it->first);
		const bool prereqLimited = (pj != NULL && pj->getAllowed() != NULL && !pj->getAllowed()->isEmpty());
		const int required = BuildingCascade::scaledPrereq(it->second, wsMod, selfLimited, prereqLimited, selfNoScale, selfCount);
		if (cascadeTally().buildingCount((int)kPlayer.getID(), (int)it->first, CASCADE_COUNT_EMPIRE) < required) return false;  // PREREQ-AMOUNT
	}
	if (j != NULL)
	{
		if (!cascadeGateOk(j->getGate(), ec, buildFlags)) return false;   // entity-level enabled/disabled (owner 2026-07-08)
		if (j->requiresBuild() != NULL && !cascadeEvalCondition(j->requiresBuild(), ec, buildFlags)) return false;    // GATE requires.build (strict)
		if (j->requiresOperate() != NULL && !cascadeEvalCondition(j->requiresOperate(), ec, operFlags)) return false; // GATE requires.operate (positive)
	}
	return true;
}

// --- BuildingCascade.cs: the city's BUILDABLE set (the engine canConstruct TRUE-set), computed IN ISOLATION.
// FRONTIER = ALL buildings (the engine has NO enables-frontier; an enables-frontier under-offers no-enabler buildings
// like PALACE). Prune in StoneBase's order: tech-obsolete, already-built, in-queue, never-buildable (notConstructible),
// instance-capped, special-building GROUP-capped, dormant-on-build, prereq-AMOUNT unmet. Then GATE requires.build
// (STRICT state religion) + requires.operate (IgnoreDisabled -- its dormancy `disabled` must not remove the building
// from buildable; POSITIVE prereqs still gate, with obsolete/civic-waived prereqs skipped via the AugmentState set).
void BuildingCascade::buildable(const CvCity* pCity, const CvPlayer& kPlayer, const CvTeam& kTeam, std::set<int>& avail)
{
	std::set<int> waived;
	augmentWaived(kPlayer, kTeam, waived);
	CvCascadeEvalCtx ec; ec.city = pCity; ec.plot = pCity->plot(); ec.player = &kPlayer; ec.team = &kTeam; ec.waivedPrereqBuildings = &waived;
	// The two per-city operating buildings (active set + in-vicinity `provides` supply, json §5a) so a requires with an
	// ACTIVE-building or vicinity-provided BONUS predicate resolves from the cascade, not the engine (DEC-calc-zero-ride-in).
	EnablerKernel::wireOperatingBuildings(pCity, ec);
	CvCascadeEvalFlags buildFlags; buildFlags.strictStateReligionForBuild = true;   // requires.build = strict
	CvCascadeEvalFlags operFlags;  operFlags.ignoreDisabled = true;                  // requires.operate = positive prereqs only
	const int nB = GC.getNumBuildingInfos();

	// QueuedBuildings (StoneBase exclude): buildings already in THIS city's production order queue.
	std::set<int> queued;
	for (int iq = 0; iq < pCity->getOrderQueueLength(); ++iq)
	{
		const OrderData od = pCity->getOrderData(iq);
		if (od.eOrderType == ORDER_CONSTRUCT) queued.insert(od.iData1);
	}

	// SpecialBuilding GROUP COUNT (StoneBase GroupCount): the player's summed count of every member of each group.
	std::map<int, int> specialCount;
	for (int b = 0; b < nB; ++b)
	{
		const SpecialBuildingTypes sb = GC.getBuildingInfo((BuildingTypes)b).getSpecialBuilding();
		if (sb != NO_SPECIALBUILDING)
			specialCount[(int)sb] += cascadeTally().buildingCount((int)kPlayer.getID(), b, CASCADE_COUNT_EMPIRE);
	}

	const int wsMod = GC.getWorldInfo(GC.getMap().getWorldSize()).getBuildingPrereqModifier();

	for (int b = 0; b < nB; ++b)
	{
		// #430 the isolated box: ONE shared primitive decides buildability (bc_isBuildable above). The full rebuild
		// runs it over every building; the incremental box update runs it over only the affected subset -- they can
		// never diverge because it is the SAME code. (Was ~60 lines of inline prune+gate here; now the single source.)
		if (bc_isBuildable(b, pCity, kPlayer, kTeam, ec, buildFlags, operFlags, queued, specialCount, wsMod))
			avail.insert(b);
	}
}

// Re-check ONLY the buildings in `affected` (a TARGETED box update, no full rebuild) and insert/erase each in
// `avail`. Per-city setup mirrors buildable() (all tally reads -- no condEvals); only the affected buildings pay
// the requires eval. `avail` is the city's LIVE buildable box, so this incrementally maintains it in place.
static void bc_recheckBuildings(const CvCity* pCity, const CvPlayer& kPlayer, const CvTeam& kTeam,
	const std::vector<int>& affected, std::set<int>& avail)
{
	if (affected.empty()) return;
	std::set<int> waived; BuildingCascade::augmentWaived(kPlayer, kTeam, waived);
	CvCascadeEvalCtx ec; ec.city = pCity; ec.plot = pCity->plot(); ec.player = &kPlayer; ec.team = &kTeam; ec.waivedPrereqBuildings = &waived;
	EnablerKernel::wireOperatingBuildings(pCity, ec);
	CvCascadeEvalFlags buildFlags; buildFlags.strictStateReligionForBuild = true;
	CvCascadeEvalFlags operFlags;  operFlags.ignoreDisabled = true;
	std::set<int> queued;
	for (int iq = 0; iq < pCity->getOrderQueueLength(); ++iq)
	{ const OrderData od = pCity->getOrderData(iq); if (od.eOrderType == ORDER_CONSTRUCT) queued.insert(od.iData1); }
	const int nB = GC.getNumBuildingInfos();
	// LAZY specialCount (targeted update must NOT be O(nB) per event): only the group counts the affected
	// buildings actually need (only 7 group special-buildings exist, so this inner scan almost never runs).
	std::map<int, int> specialCount;
	for (size_t i = 0; i < affected.size(); ++i)
	{
		const SpecialBuildingTypes sb = GC.getBuildingInfo((BuildingTypes)affected[i]).getSpecialBuilding();
		if (sb != NO_SPECIALBUILDING && specialCount.find((int)sb) == specialCount.end())
		{
			int cnt = 0;
			for (int b2 = 0; b2 < nB; ++b2)
				if (GC.getBuildingInfo((BuildingTypes)b2).getSpecialBuilding() == sb)
					cnt += cascadeTally().buildingCount((int)kPlayer.getID(), b2, CASCADE_COUNT_EMPIRE);
			specialCount[(int)sb] = cnt;
		}
	}
	const int wsMod = GC.getWorldInfo(GC.getMap().getWorldSize()).getBuildingPrereqModifier();
	for (size_t i = 0; i < affected.size(); ++i)
	{
		const int b = affected[i];
		if (bc_isBuildable(b, pCity, kPlayer, kTeam, ec, buildFlags, operFlags, queued, specialCount, wsMod))
			avail.insert(b);
		else
			avail.erase(b);
	}
}

void BuildingCascade::onBuildingChanged(const CvCity* pCity, int eBuilding)
{
	if (pCity == NULL || eBuilding < 0) return;
	// Only touch a currently-BUILT box: if CPK_FRONT_B is dirty a full rebuild is already pending on next read,
	// so a targeted update here would be wasted (overwritten). This is the "never rebuilt on building-completed"
	// contract -- completion does NOT dirty FRONT_B (see buildingProcessed); tech/civic growth DOES (-> rebuild).
	if (pCity->m_cascadeCityPackages.set.isDirty(CPK_FRONT_B)) return;
	bc_buildIndices();
	// The affected subset: the changed building itself (built -> leaves buildable; lost -> may re-enter) + every
	// building whose requires references it (its prereq state just changed). No other building can be affected.
	std::vector<int> affected;
	affected.push_back(eBuilding);
	std::map<int, std::vector<int> >::const_iterator it = s_bcBuildingDeps.find(eBuilding);
	if (it != s_bcBuildingDeps.end())
		affected.insert(affected.end(), it->second.begin(), it->second.end());
	const CvPlayer& kPlayer = GET_PLAYER(pCity->getOwner());
	const CvTeam& kTeam = GET_TEAM(kPlayer.getTeam());
	bc_recheckBuildings(pCity, kPlayer, kTeam, affected, pCity->m_cascadeCityPackages.enBuildable);
}

// Part A: promote the lazy first-onBuildingChanged trigger to the load-end warm-up (idempotent -- s_bcIdxBuilt).
void BuildingCascade::buildIndices()
{
	bc_buildIndices();
}

// Part C: a city-local HAVE atom flipped -> re-check ONLY the buildings that reference it (the matching s_bc*
// bucket) in this city's buildable box, in place. Skips a box with a full rebuild already pending (correctness
// held by that rebuild). Consumes the "built but dead" HAVE-atom buckets (bc_buildIndices).
void BuildingCascade::recheckHave(const CvCity* pCity, const CvPlayer& kPlayer, const CvTeam& kTeam, int eHaveKind)
{
	if (pCity == NULL) return;
	if (pCity->m_cascadeCityPackages.set.isDirty(CPK_FRONT_B)) return;
	bc_buildIndices();
	const std::vector<int>* bucket = NULL;
	switch (eHaveKind)
	{
	case CascadeAccumulator::CASC_HAVE_POP:      bucket = &s_bcPop; break;
	case CascadeAccumulator::CASC_HAVE_RELIGION: bucket = &s_bcReligion; break;
	case CascadeAccumulator::CASC_HAVE_CORP:     bucket = &s_bcCorp; break;
	case CascadeAccumulator::CASC_HAVE_POWER:    bucket = &s_bcPower; break;
	default: return;   // tech/civic/golden-age stay the broad markPlayerScopeAndCities (not wired here)
	}
	bc_recheckBuildings(pCity, kPlayer, kTeam, *bucket, pCity->m_cascadeCityPackages.enBuildable);
}
