//
//	UnitCascade -- StoneBase CalculateTrainableUnits.cs (see the header). Ported VERBATIM from CvCascadeEnabler.cpp's
//	file-static en_unitCapped / en_unitReachable / en_unitTrainable; promoted to a declared surface (the single-source
//	law, patterns.md). LOGIC unchanged: only the signatures + the EnablerKernel/BuildingCascade-qualified call sites
//	were rewritten.
//

#include "CvGameCoreDLL.h"
#include "CvCascadeUnitCascade.h"
#include "CvCascadeEnablerKernel.h"     // EnablerKernel::obsoletedByHeldTech
#include "CvCascadeBuildingCascade.h"   // BuildingCascade::augmentWaived (shared AugmentState waiver)
#include "CvJsonInfo.h"
#include "CvJsonUnitInfo.h"           // spawnOnly (the cascade's own never-trained flag; self-containment)
#include "Repos/InfoRepo.h"
#include "CvCascadeTally.h"
#include "AI/CvPlayerAI.h"            // GET_PLAYER
#include "AI/CvTeamAI.h"             // GET_TEAM
#include "Defines/CvGlobals.h"
#include "Engine/CvCity.h"
#include "Engine/CvPlayer.h"
#include "Engine/CvTeam.h"
#include "CvCascadeConditionEval.h"   // cascadeEvalCondition
#include "Infos/CvUnitInfo.h"
#include "Engine/CvGame.h"

// Unit instance cap (StoneBase UnitCascade.Capped): WORLD = lifetime-created (getUnitCreatedCount) + making >=
// allowed.world; EMPIRE = live count (tally) + making >= ERA-SCALED (base-5 => +5/era) allowed.empire, waived by
// NO_NATIONAL_UNIT_LIMIT unless the unit is unlimitedException. (Units have no team cap.)
bool UnitCascade::capped(const CvJsonInfo* j, int eU, const CvPlayer& kPlayer, bool noNationalLimit)
{
	if (j == NULL) return false;
	const int making = kPlayer.getUnitMaking((UnitTypes)eU);
	std::map<std::string, int>::const_iterator w = j->allowed.find("world");
	if (w != j->allowed.end() && GC.getGame().getUnitCreatedCount((UnitTypes)eU) + making >= w->second) return true;
	std::map<std::string, int>::const_iterator e = j->allowed.find("empire");
	if (e != j->allowed.end() && !(noNationalLimit && !GC.getUnitInfo((UnitTypes)eU).isUnlimitedException()))
	{
		const int era = (int)kPlayer.getCurrentEra();
		const int cap = (e->second == 5 && era > 0) ? e->second + era * 5 : e->second;   // era-scaled base-5 national cap
		if (cascadeTally().unitCount((int)kPlayer.getID(), eU, CASCADE_COUNT_EMPIRE) + making >= cap) return true;
	}
	return false;
}

// reachable(v) (StoneBase UnitCascade.Reachable): v is itself available OR some DIRECT upgrade of v (its dormant
// triggers = requires.build.dormant.all) is reachable. Cycle-guarded (a cycle resolves to the self-available terminal).
bool UnitCascade::reachable(int v, const std::set<int>& available, std::map<int, bool>& cache, std::set<int>& inProgress)
{
	std::map<int, bool>::const_iterator c = cache.find(v);
	if (c != cache.end()) return c->second;
	if (!inProgress.insert(v).second) return available.count(v) != 0;   // cycle -> self-available terminal
	bool r = available.count(v) != 0;
	if (!r)
	{
		const CvJsonInfo* j = InfoRepo<CvUnitInfo>::get().get(v);
		if (j != NULL)
			for (size_t i = 0; i < j->dormantTriggers.size() && !r; ++i)
				r = reachable(j->dormantTriggers[i], available, cache, inProgress);
	}
	inProgress.erase(v);
	cache[v] = r;
	return r;
}

// --- UnitCascade.cs: the city's TRAINABLE set (the engine canTrain TRUE-set), GENERATE-then-GATE. Units REUSE the
// building machinery -- only the inputs differ. (1) GATE availability: all units minus spawnOnly (identity.spawnOnly, the
// cascade's OWN flag -- never player-trained; self-containment, StoneBase u.SpawnOnly) / tech-obsoleted / instance-capped,
// then requires.build (STRICT). (2) GENERATE frontier: all units
// minus spawnOnly/obsoleted/replaced-when-the-replacer-is-available (the `replaces` edge -- source-side, inverted;
// inert today, enabler.md §2). (3) GATE the frontier: LISTED = available AND not dormant (requires.build.dormant.all =
// the direct-upgrade closure: a unit hides only when EVERY direct upgrade is reachable-trainable; one dead branch keeps
// it buildable). AugmentState vicinity/gov-center facts are read LIVE by the evaluator.
void UnitCascade::trainable(const CvCity* pCity, const CvPlayer& kPlayer, const CvTeam& kTeam, std::set<int>& result)
{
	std::set<int> waived;
	BuildingCascade::augmentWaived(kPlayer, kTeam, waived);   // SAME AugmentState waiver the building cascade uses (shared evaluator)
	CvCascadeEvalCtx ec; ec.city = pCity; ec.player = &kPlayer; ec.team = &kTeam; ec.waivedPrereqBuildings = &waived;
	// The two per-city building facts (active set + in-vicinity `provides` supply, json §5a): a herd/tamed-animal
	// building that provides e.g. HORSE ⇒ HORSE in-vicinity, so a horse unit's `requires` {BONUS, connection:vicinity}
	// trains. Computed from the cascade, NOT the engine's hasVicinityBonus (DEC-calc-zero-ride-in).
	std::set<int> activeB, provB; EnablerKernel::computeCityBuildingFacts(pCity, ec, activeB, provB);
	ec.activeBuildings = &activeB; ec.vicinityProvidedBonuses = &provB;
	CvCascadeEvalFlags flags; flags.strictStateReligionForBuild = true;
	const bool noNationalLimit = GC.getGame().isOption(GAMEOPTION_NO_NATIONAL_UNIT_LIMIT);
	const int nU = GC.getNumUnitInfos();

	// (1) GATE availability.
	std::set<int> available;
	for (int u = 0; u < nU; ++u)
	{
		const CvJsonInfo* j = InfoRepo<CvUnitInfo>::get().get(u);
		// spawnOnly: never player-trained (StoneBase u.SpawnOnly = identity.spawnOnly). Read the cascade's OWN flag, NOT
		// the engine productionCost<0 marker (self-containment, DEC-calc-zero-ride-in). Value-equivalent on current data.
		if (j != NULL && ((const CvJsonUnitInfo*)j)->spawnOnly) continue;
		if (EnablerKernel::obsoletedByHeldTech(j, kTeam)) continue;
		if (capped(j, u, kPlayer, noNationalLimit)) continue;
		if (j != NULL && j->requiresBuild != NULL && !cascadeEvalCondition(j->requiresBuild, ec, flags)) continue;
		available.insert(u);
	}

	// The replaced set: a unit is HIDDEN if any AVAILABLE unit's `replaces.units` names it (source-side edge inverted;
	// no target-side replacedBy is curated -- inert today). Computed BEFORE its own requires is weighed (a GENERATE removal).
	std::set<int> replacedUnits;
	for (std::set<int>::const_iterator a = available.begin(); a != available.end(); ++a)
	{
		const CvJsonInfo* ja = InfoRepo<CvUnitInfo>::get().get(*a);
		if (ja == NULL) continue;
		std::map<std::string, std::vector<int> >::const_iterator re = ja->edges.find("replaces.units");
		if (re != ja->edges.end())
			for (size_t i = 0; i < re->second.size(); ++i) replacedUnits.insert(re->second[i]);
	}

	// (2) GENERATE frontier: all units minus spawnOnly / obsoleted / replaced.
	std::set<int> frontier;
	for (int u = 0; u < nU; ++u)
	{
		const CvJsonInfo* j = InfoRepo<CvUnitInfo>::get().get(u);
		if (j != NULL && ((const CvJsonUnitInfo*)j)->spawnOnly) continue;   // spawnOnly (cascade's own flag; self-containment)
		if (EnablerKernel::obsoletedByHeldTech(j, kTeam)) continue;
		if (replacedUnits.count(u) != 0) continue;
		frontier.insert(u);
	}

	// (3) GATE the frontier: LISTED = in CAN GET ∧ available ∧ not dormant.
	std::map<int, bool> cache; std::set<int> inProgress;
	for (std::set<int>::const_iterator it = frontier.begin(); it != frontier.end(); ++it)
	{
		const int u = *it;
		if (available.count(u) == 0) continue;   // in CAN GET but requires.build unmet => GREYED, not LISTED
		const CvJsonInfo* j = InfoRepo<CvUnitInfo>::get().get(u);
		bool dormant = false;
		if (j != NULL && !j->dormantTriggers.empty())
		{
			dormant = true;
			for (size_t i = 0; i < j->dormantTriggers.size() && dormant; ++i)
				if (!reachable(j->dormantTriggers[i], available, cache, inProgress)) dormant = false;
		}
		if (!dormant) result.insert(u);
	}
}
