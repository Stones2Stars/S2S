//
//	UnitEnabler -- StoneBase CalculateTrainableUnits.cs (see the header): the unit frontier generate/gate (cap,
//	reachable-upgrade closure, trainable), a declared surface over the ONE EnablerKernel primitive (the
//	single-source law, patterns.md).
//

#include "CvGameCoreDLL.h"
#include "CvUnitEnabler.h"
#include "CvEnablerKernel.h"     // EnablerKernel::obsoletedByHeldTech
#include "CvBuildingEnabler.h"   // BuildingEnabler::augmentWaived (shared AugmentState waiver)
#include "CvInfo.h"
#include "CvUnitInfo.h"           // spawnOnly (the enabler's own never-trained flag; self-containment)
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
#include "CvCascadeAccumulator.h"     // CascadeAccumulator::CascadeHaveKind (recheckHave dispatch) + CPK_FRONT_U
#include "CvUnitInfo.h"
#include "CvBuildingInfo.h"     // InfoRepo<CvBuildingInfo> (a changed building's provides.bonuses / enables.units)
#include "Engine/CvGame.h"

// Unit instance cap (StoneBase UnitEnabler.Capped): WORLD = lifetime-created (getUnitCreatedCount) + making >=
// allowed.world; EMPIRE = live count (tally) + making >= ERA-SCALED (base-5 => +5/era) allowed.empire, waived by
// NO_NATIONAL_UNIT_LIMIT unless the unit is unlimitedException. (Units have no team cap.)
bool UnitEnabler::capped(const CvInfo* j, int eU, const CvPlayer& kPlayer, bool noNationalLimit)
{
	if (j == NULL) return false;
	const int making = kPlayer.getUnitMaking((UnitTypes)eU);
	const int wcap = j->allowedCap("world");
	if (wcap >= 0 && GC.getGame().getUnitCreatedCount((UnitTypes)eU) + making >= wcap) return true;
	const int ecap = j->allowedCap("empire");
	if (ecap >= 0 && !(noNationalLimit && !GC.getUnitInfo((UnitTypes)eU).isUnlimitedException()))
	{
		const int era = (int)kPlayer.getCurrentEra();
		const int cap = (ecap == 5 && era > 0) ? ecap + era * 5 : ecap;   // era-scaled base-5 national cap
		if (cascadeTally().unitCount((int)kPlayer.getID(), eU, CASCADE_COUNT_EMPIRE) + making >= cap) return true;
	}
	return false;
}

// ===========================================================================================================
// The isolated-box REVERSE INDEX for UNITS (enabler-frontier-perf.md Part A) -- the UNIT analogue of the proven
// s_bc* building index (CvBuildingEnabler.cpp). At LOAD we invert every unit's requires.build tree into
// "HAVE-atom kind -> {unit ids that reference it}", so a HAVE-change (pop / religion / corp / power / a specific
// tech or bonus, or a building the unit requires) re-checks ONLY its bucket via the shared uc_isTrainable,
// instead of the ~3340-unit full re-walk. trainable() gates ONLY on requires.build (NOT operate), so the shared
// scanner (EnablerKernel::scanCondDeps, with the UNIT_ leg on) runs over requires.build alone. SAFE-BY-DESIGN:
// over-inclusion only costs a few extra re-checks; the slice-boundary full rebuild remains the net.
// ===========================================================================================================
namespace {

// The buckets (unit-id lists), built once. Over-inclusive is fine; a missed class self-heals at the boundary.
static bool s_ucIdxBuilt = false;
static std::vector<int> s_ucPop, s_ucReligion, s_ucCorp, s_ucPower, s_ucGolden, s_ucStateRel, s_ucCivic;
static std::map<int, std::vector<int> > s_ucTech, s_ucBonus, s_ucBuilding;
static std::map<int, std::vector<int> > s_ucUnitDeps;      // referencedUnitId -> {units whose requires references it}
static std::map<int, std::vector<int> > s_ucUpgradePred;   // upgradeUnitId  -> {units that name it as a dormant-trigger}

static void uc_buildIndices()
{
	if (s_ucIdxBuilt) return;
	s_ucIdxBuilt = true;
	const int nU = GC.getNumUnitInfos();
	for (int u = 0; u < nU; ++u)
	{
		const CvInfo* j = InfoRepo<CvUnitInfo>::get().get(u);
		if (j == NULL) continue;
		CascadeCondDeps d;
		// The ONE shared HAVE-atom scanner: requires.build ONLY (trainable() does not gate on operate), UNIT_ leg on
		// (a unit's requires may reference another unit's count), no DYNAMIC marking (plot/domain predicates are
		// read LIVE at eval; they don't bucket a HAVE-change re-check).
		EnablerKernel::scanCondDeps(j->requiresBuild(), d, /*bTrackUnits*/ true, /*bMarkDynamic*/ false);
		// a unit's dormant-triggers are its DIRECT UPGRADES: this unit's dormancy depends on their reachability,
		// so a count change to an upgrade that flips its availability must re-check this predecessor.
		const std::vector<int>& dorm = j->dormantTriggers();
		for (size_t i = 0; i < dorm.size(); ++i) s_ucUpgradePred[dorm[i]].push_back(u);
		if (d.pop)           s_ucPop.push_back(u);
		if (d.religion)      s_ucReligion.push_back(u);
		if (d.corp)          s_ucCorp.push_back(u);
		if (d.power)         s_ucPower.push_back(u);
		if (d.goldenAge)     s_ucGolden.push_back(u);
		if (d.stateReligion) s_ucStateRel.push_back(u);
		if (d.civicAny)      s_ucCivic.push_back(u);
		for (std::set<int>::const_iterator it = d.techs.begin(); it != d.techs.end(); ++it) s_ucTech[*it].push_back(u);
		for (std::set<int>::const_iterator it = d.bonuses.begin(); it != d.bonuses.end(); ++it) s_ucBonus[*it].push_back(u);
		for (std::set<int>::const_iterator it = d.buildings.begin(); it != d.buildings.end(); ++it) s_ucBuilding[*it].push_back(u);
		for (std::set<int>::const_iterator it = d.units.begin(); it != d.units.end(); ++it) s_ucUnitDeps[*it].push_back(u);
	}
}

// The per-city recheck context: the ONE per-city setup (waived / ec / operating buildings / flags) computed once, plus the
// memo caches so a targeted subset shares availability/reachability lookups (file scope: VC7.1 forbids local
// types as arguments to the helper functions below).
struct UcRecheckCtx
{
	const CvCity* city;
	const CvPlayer* player;
	const CvTeam* team;
	CvCascadeEvalCtx* ec;
	const CvCascadeEvalFlags* flags;
	bool noNationalLimit;
	std::map<int, bool> availCache;   // u -> uc_isAvailable(u)
	std::map<int, bool> reachCache;   // v -> uc_reachable(v)
	std::set<int> inProgress;         // reachable() cycle guard (always fully unwound between roots)
};

// The ONE availability gate (trainable()'s (1) leg AND the targeted re-checks' -- the bc_isBuildable idiom, one
// primitive, N consumers): spawnOnly / tech-obsolete / instance-cap / entity gate / requires.build STRICT.
// Self-contained (tally + ctx reads); no dependency on other units' verdicts.
static bool uc_isAvailable(int u, UcRecheckCtx& x)
{
	const CvInfo* j = InfoRepo<CvUnitInfo>::get().get(u);
	if (j != NULL && ((const CvUnitInfo*)j)->spawnOnly) return false;
	if (EnablerKernel::obsoletedByHeldTech(j, *x.team)) return false;
	if (UnitEnabler::capped(j, u, *x.player, x.noNationalLimit)) return false;
	if (j != NULL && !cascadeGateOk(j->getGate(), *x.ec, *x.flags)) return false;   // entity-level enabled/disabled
	if (j != NULL && j->requiresBuild() != NULL && !cascadeEvalCondition(j->requiresBuild(), *x.ec, *x.flags)) return false;
	return true;
}

static bool uc_availMemo(int u, UcRecheckCtx& x)
{
	std::map<int, bool>::const_iterator it = x.availCache.find(u);
	if (it != x.availCache.end()) return it->second;
	const bool r = uc_isAvailable(u, x);
	x.availCache[u] = r;
	return r;
}

// reachable(v) (StoneBase UnitEnabler.Reachable) -- the ONE upgrade-reachability closure, driven by the memoized
// availability PREDICATE (uc_availMemo, the more general form: a precomputed set is just this predicate frozen),
// so the full fill and a targeted recheck share one implementation: v is itself available OR some DIRECT upgrade
// of v (its dormant triggers = requires.build.dormant.all) is reachable. Cycle-guarded (a cycle -> self-available).
static bool uc_reachable(int v, UcRecheckCtx& x)
{
	std::map<int, bool>::const_iterator c = x.reachCache.find(v);
	if (c != x.reachCache.end()) return c->second;
	if (!x.inProgress.insert(v).second) return uc_availMemo(v, x);   // cycle -> self-available terminal
	bool r = uc_availMemo(v, x);
	if (!r)
	{
		const CvInfo* j = InfoRepo<CvUnitInfo>::get().get(v);
		if (j != NULL)
		{
			const std::vector<int>& dorm = j->dormantTriggers();
			for (size_t i = 0; i < dorm.size() && !r; ++i)
				r = uc_reachable(dorm[i], x);
		}
	}
	x.inProgress.erase(v);
	x.reachCache[v] = r;
	return r;
}

// The ONE per-unit TRAINABLE verdict (trainable()'s (3) leg AND the targeted re-checks': LISTED = available ∧
// ¬dormant, dormant = EVERY direct upgrade reachable-trainable). NB the (2) GENERATE `replaces.units` removal is inert on current
// data ("no target-side replacedBy is curated" -- trainable()'s own note); the incremental path treats it inert
// and the slice-net covers any future curation of that edge.
static bool uc_isTrainable(int u, UcRecheckCtx& x)
{
	if (!uc_availMemo(u, x)) return false;
	const CvInfo* j = InfoRepo<CvUnitInfo>::get().get(u);
	if (j != NULL && !j->dormantTriggers().empty())
	{
		const std::vector<int>& dorm = j->dormantTriggers();
		bool dormant = true;
		for (size_t i = 0; i < dorm.size() && dormant; ++i)
			if (!uc_reachable(dorm[i], x)) dormant = false;
		if (dormant) return false;
	}
	return true;
}

// Re-check ONLY the units in `affected` in pCity's LIVE trainable box (a TARGETED update, no full rebuild) and
// insert/erase each. Per-city setup mirrors trainable() (waived / operating buildings / flags -- no condEvals) so only the
// affected units pay the requires eval. NB trainable() has NO queued-exclusion (that is the building enabler's);
// the per-turn "don't re-pick what was queued" erase lives at CvCity::pushOrder, so this path matches the full
// fill by NOT excluding queued.
static void uc_recheckUnits(const CvCity* pCity, const CvPlayer& kPlayer, const CvTeam& kTeam,
	const std::vector<int>& affected, std::set<int>& box)
{
	if (affected.empty()) return;
	std::set<int> waived; BuildingEnabler::augmentWaived(kPlayer, kTeam, waived);
	CvCascadeEvalCtx ec; ec.city = pCity; ec.plot = pCity->plot(); ec.player = &kPlayer; ec.team = &kTeam; ec.waivedPrereqBuildings = &waived;
	EnablerKernel::wireOperatingBuildings(pCity, ec);
	CvCascadeEvalFlags flags; flags.strictStateReligionForBuild = true;
	UcRecheckCtx x;
	x.city = pCity; x.player = &kPlayer; x.team = &kTeam; x.ec = &ec; x.flags = &flags;
	x.noNationalLimit = GC.getGame().isOption(GAMEOPTION_NO_NATIONAL_UNIT_LIMIT);
	for (size_t i = 0; i < affected.size(); ++i)
	{
		const int u = affected[i];
		if (uc_isTrainable(u, x)) box.insert(u);
		else                      box.erase(u);
	}
}

// Append every id in `src` into `dst` (bucket concat for the affected-set gather).
static void uc_appendBucket(const std::vector<int>& src, std::vector<int>& dst)
{
	dst.insert(dst.end(), src.begin(), src.end());
}
static void uc_appendMapBucket(const std::map<int, std::vector<int> >& m, int key, std::vector<int>& dst)
{
	std::map<int, std::vector<int> >::const_iterator it = m.find(key);
	if (it != m.end()) dst.insert(dst.end(), it->second.begin(), it->second.end());
}

} // namespace

// --- UnitEnabler.cs: the city's TRAINABLE set (the engine canTrain TRUE-set), GENERATE-then-GATE. Units REUSE the
// building machinery -- only the inputs differ. Built on the ONE primitive pair (the bc_isBuildable idiom: one
// primitive, two consumers -- the full fill here and the targeted box re-checks can never diverge in per-unit logic):
// (1) GATE availability: uc_isAvailable, memoized -- all units minus spawnOnly (identity.spawnOnly, the enabler's OWN
// flag -- never player-trained; self-containment, StoneBase u.SpawnOnly) / tech-obsoleted / instance-capped /
// entity-gate-failed, then requires.build (STRICT). (2) GENERATE frontier: all units
// minus spawnOnly/obsoleted/replaced-when-the-replacer-is-available (the `replaces` edge -- source-side, inverted;
// inert today, enabler.md §2). (3) GATE the frontier: uc_isTrainable -- LISTED = available AND not dormant (a
// non-available frontier member is GREYED, not LISTED; requires.build.dormant.all = the direct-upgrade closure:
// a unit hides only when EVERY direct upgrade is reachable-trainable; one dead branch keeps it buildable).
// AugmentState vicinity/gov-center operating buildings are read LIVE by the evaluator.
void UnitEnabler::trainable(const CvCity* pCity, const CvPlayer& kPlayer, const CvTeam& kTeam, std::set<int>& result, bool bVisible)
{
	std::set<int> waived;
	BuildingEnabler::augmentWaived(kPlayer, kTeam, waived);   // SAME AugmentState waiver the building enabler uses (shared evaluator)
	CvCascadeEvalCtx ec; ec.city = pCity; ec.plot = pCity->plot(); ec.player = &kPlayer; ec.team = &kTeam; ec.waivedPrereqBuildings = &waived;
	// The two per-city operating buildings (active set + in-vicinity `provides` supply, json §5a): a herd/tamed-animal
	// building that provides e.g. HORSE ⇒ HORSE in-vicinity, so a horse unit's `requires` {BONUS, connection:vicinity}
	// trains. Computed from the enabler, NOT the engine's hasVicinityBonus (DEC-calc-zero-ride-in).
	EnablerKernel::wireOperatingBuildings(pCity, ec);
	CvCascadeEvalFlags flags; flags.strictStateReligionForBuild = true;
	flags.testVisible = bVisible;   // VISIBLE frontier: relax greyable clauses (connectable resource / unadopted civic -> GREYED, enabler.md §6)
	UcRecheckCtx x;
	x.city = pCity; x.player = &kPlayer; x.team = &kTeam; x.ec = &ec; x.flags = &flags;
	x.noNationalLimit = GC.getGame().isOption(GAMEOPTION_NO_NATIONAL_UNIT_LIMIT);
	const int nU = GC.getNumUnitInfos();

	// (1) GATE availability (uc_isAvailable, memoized -- the memo also feeds the (3) dormancy reachability walk).
	std::set<int> available;
	for (int u = 0; u < nU; ++u)
		if (uc_availMemo(u, x)) available.insert(u);

	// The replaced set (StoneBase UnitEnabler: `u.ReplacedBy["units"].Any(available.Contains)`): a unit is HIDDEN the
	// moment any of its SUPERSEDERS is AVAILABLE. The curator emits SupersedingUnits TARGET-side as the predecessor's
	// `replacedBy.units`, which CvUnitInfo.mapFrom parses into m_superseding (getSupersedingUnit) -- NOT the CvJsonEdges
	// bucket, so it is read from the poco, NOT j->edge("replacedBy.units") (that returns NULL -- the inert-read trap).
	// A GENERATE removal weighed BEFORE the unit's own requires; mirrors isSupersedingUnitAvailable. (Upgrade-tree
	// hiding is the SEPARATE uc_reachable dormancy gate, §3 below.)
	std::set<int> replacedUnits;
	for (int u = 0; u < nU; ++u)
	{
		const CvInfo* j = InfoRepo<CvUnitInfo>::get().get(u);
		if (j == NULL) continue;
		const CvUnitInfo* ju = (const CvUnitInfo*)j;   // SupersedingUnits live in m_superseding (getSupersedingUnit), NOT the edges
		for (int i = 0; i < ju->getNumSupersedingUnits(); ++i)
		{
			const int sup = ju->getSupersedingUnit(i);
			if (sup >= 0 && available.count(sup) != 0) { replacedUnits.insert(u); break; }
		}
	}

	// (2) GENERATE frontier: all units minus spawnOnly / obsoleted / replaced.
	std::set<int> frontier;
	for (int u = 0; u < nU; ++u)
	{
		const CvInfo* j = InfoRepo<CvUnitInfo>::get().get(u);
		if (j != NULL && ((const CvUnitInfo*)j)->spawnOnly) continue;   // spawnOnly (the enabler's own flag; self-containment)
		if (EnablerKernel::obsoletedByHeldTech(j, kTeam)) continue;
		if (replacedUnits.count(u) != 0) continue;
		frontier.insert(u);
	}

	// (3) GATE the frontier: LISTED = in CAN GET ∧ available ∧ not dormant (uc_isTrainable).
	for (std::set<int>::const_iterator it = frontier.begin(); it != frontier.end(); ++it)
		if (uc_isTrainable(*it, x)) result.insert(*it);
}

void UnitEnabler::buildIndices()
{
	uc_buildIndices();
}

void UnitEnabler::recheckHave(const CvCity* pCity, const CvPlayer& kPlayer, const CvTeam& kTeam, int eHaveKind)
{
	if (pCity == NULL) return;
	// Only touch a currently-BUILT box: a dirty CPK_FRONT_U means a full rebuild is already pending on next read,
	// so a targeted update would be wasted (overwritten) -- and correctness holds (the rebuild happens on read).
	if (pCity->m_cascadeCityPackages.set.isDirty(CPK_FRONT_U)) return;
	uc_buildIndices();
	const std::vector<int>* bucket = NULL;
	switch (eHaveKind)
	{
	case CascadeAccumulator::CASC_HAVE_POP:      bucket = &s_ucPop; break;
	case CascadeAccumulator::CASC_HAVE_RELIGION: bucket = &s_ucReligion; break;
	case CascadeAccumulator::CASC_HAVE_CORP:     bucket = &s_ucCorp; break;
	case CascadeAccumulator::CASC_HAVE_POWER:    bucket = &s_ucPower; break;
	default: return;   // tech/civic/golden-age stay the broad markPlayerScopeAndCities (not wired here)
	}
	uc_recheckUnits(pCity, kPlayer, kTeam, *bucket, pCity->m_cascadeCityPackages.enTrainable);
}

void UnitEnabler::onUnitChanged(const CvPlayer& kPlayer, int eUnit)
{
	if (eUnit < 0) return;
	uc_buildIndices();
	// GUARD: a unit's empire COUNT is an availability input ONLY through its instance cap (allowed). An uncapped
	// unit's count change cannot flip ANY unit's trainable verdict (requires atoms don't read counts; dormancy
	// reads availability, which for an uncapped unit never moves on count) -- UNLESS some other unit's requires
	// references this unit's count (s_ucUnitDeps). So skip entirely in the common (uncapped, unreferenced) case;
	// this keeps combat unit births/deaths free.
	const CvInfo* jU = InfoRepo<CvUnitInfo>::get().get(eUnit);
	const bool bCapped = (jU != NULL && jU->getAllowed() != NULL && !jU->getAllowed()->isEmpty());
	const bool bReferenced = (s_ucUnitDeps.find(eUnit) != s_ucUnitDeps.end());
	if (!bCapped && !bReferenced) return;
	// The affected subset: the changed unit + units whose requires references it + units it is a direct upgrade of
	// (their dormancy depends on this unit's availability). Unit caps are EMPIRE-scoped, so this re-check applies to
	// every one of the player's cities.
	std::vector<int> affected;
	affected.push_back(eUnit);
	uc_appendMapBucket(s_ucUnitDeps, eUnit, affected);
	uc_appendMapBucket(s_ucUpgradePred, eUnit, affected);
	const CvTeam& kTeam = GET_TEAM(kPlayer.getTeam());
	int iLoop;
	for (const CvCity* pc = kPlayer.firstCity(&iLoop); pc != NULL; pc = kPlayer.nextCity(&iLoop))
	{
		if (pc->m_cascadeCityPackages.set.isDirty(CPK_FRONT_U)) continue;   // rebuild pending -> skip (see recheckHave)
		uc_recheckUnits(pc, kPlayer, kTeam, affected, pc->m_cascadeCityPackages.enTrainable);
	}
}

void UnitEnabler::onBuildingChangedUnits(const CvCity* pCity, int eBuilding)
{
	if (pCity == NULL || eBuilding < 0) return;
	if (pCity->m_cascadeCityPackages.set.isDirty(CPK_FRONT_U)) return;   // rebuild pending -> skip
	uc_buildIndices();
	// A building change touches unit trainability ONLY via: (a) a unit requiring the building's PRESENCE, (b) a
	// unit requiring a BONUS this building supplies IN-VICINITY (provides.bonuses -> the operating buildings vicinity supply,
	// re-read fresh below), or (c) a unit the building ENABLES (over-inclusive; trainable() does not gate on
	// enables, but including these is harmless). The operating buildings were re-marked dirty by buildingProcessed, so the
	// recheck reads the FRESH vicinity supply. (Secondary active-flips of OTHER buildings' provides -- the operate
	// fixpoint -- are NOT chased here; that class self-heals at the slice, matching the building path's own scope.)
	std::vector<int> affected;
	uc_appendMapBucket(s_ucBuilding, eBuilding, affected);
	const CvInfo* jb = InfoRepo<CvBuildingInfo>::get().get(eBuilding);
	if (jb != NULL)
	{
		const CvJsonProvides* pv = jb->getProvides();
		if (pv != NULL)
			for (size_t i = 0; i < pv->bonuses.size(); ++i) uc_appendMapBucket(s_ucBonus, pv->bonuses[i], affected);
		const std::vector<int>* eu = jb->edge("enables.units");
		if (eu != NULL) uc_appendBucket(*eu, affected);
	}
	if (affected.empty()) return;
	const CvPlayer& kPlayer = GET_PLAYER(pCity->getOwner());
	const CvTeam& kTeam = GET_TEAM(kPlayer.getTeam());
	uc_recheckUnits(pCity, kPlayer, kTeam, affected, pCity->m_cascadeCityPackages.enTrainable);
}
