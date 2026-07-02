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
#include "Infos/CvBuildingInfo.h"
#include "Infos/CvSpecialBuildingInfo.h"   // getMaxPlayerInstances (special-building group cap)
#include "Infos/CvWorldInfo.h"             // getBuildingPrereqModifier (the raw world scalar for ScaledPrereq)
#include "Infos/CvCivicInfo.h"
#include "Engine/CvMap.h"
#include "Engine/CvGame.h"

// AugmentState's prereq-WAIVER set (StoneBase BuildingCascade.AugmentState: ObsoleteBuildings ∪ PrereqWaivedBuildings):
// a BUILDING is a waived prereq iff its OBSOLETE tech is held by the team, OR its SpecialBuilding group is made
// not-required by an adopted civic (enables.specialBuildingsWaived). Shared by the building + unit cascades (both gate
// requires.build through the SAME evaluator). The vicinity-supply + gov-center AugmentState facts are read LIVE by the
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
		std::map<std::string, std::vector<int> >::const_iterator it = j->edges.find("enables.specialBuildingsWaived");
		if (it != j->edges.end())
			for (size_t i = 0; i < it->second.size(); ++i) waivedSpecials.insert(it->second[i]);
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
	const int making = kPlayer.getBuildingMaking((BuildingTypes)eB);   // the player's in-production count
	for (std::map<std::string, int>::const_iterator it = j->allowed.begin(); it != j->allowed.end(); ++it)
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
	// The two per-city building facts (active set + in-vicinity `provides` supply, json §5a) so a requires with an
	// ACTIVE-building or vicinity-provided BONUS predicate resolves from the cascade, not the engine (DEC-calc-zero-ride-in).
	std::set<int> activeB, provB; EnablerKernel::computeCityBuildingFacts(pCity, ec, activeB, provB);
	ec.activeBuildings = &activeB; ec.vicinityProvidedBonuses = &provB;
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
		const BuildingTypes eB = (BuildingTypes)b;
		const CvBuildingInfo& bi = GC.getBuildingInfo(eB);
		const CvJsonInfo* j = InfoRepo<CvBuildingInfo>::get().get(b);
		if (EnablerKernel::obsoletedByHeldTech(j, kTeam)) continue;      // PRUNE: tech-obsolescence (obsoletedBy.techs)
		if (pCity->hasBuilding(eB)) continue;                           // EXCLUDE: already built in this city
		if (queued.count(b) != 0) continue;                             // EXCLUDE: already in this city's production queue
		// EXCLUDE never-buildable: the cascade's OWN identity.notConstructible flag (StoneBase reads exactly this JSON
		// flag -- IReadOnlySet NotConstructible), NOT the engine's productionCost<0/isAutoBuild markers (self-containment,
		// DEC-calc-zero-ride-in: the cascade must stand after legacy is cut). VALUE-EQUIVALENT on current data: the
		// curator sets identity.notConstructible when iCost==-1 (== productionCost<0), and every autoBuild:true building
		// (181/181) also carries notConstructible:true, so `productionCost<0 || isAutoBuild()` == notConstructible here
		// (json §7 autoBuild ⊂ notConstructible). NULL j (no cascade info) => not-notConstructible (matches engine default).
		{
			const CvJsonBuildingInfo* jb = (const CvJsonBuildingInfo*)j;
			if (jb != NULL && jb->notConstructible) continue;
		}
		if (capped(j, b, kPlayer)) continue;                            // INSTANCE CAP (created + making >= allowed)
		// SPECIALBUILDING GROUP CAP (PLAYER scope only): a member leaves buildable once its group count >=
		// getMaxPlayerInstances. StoneBase's GroupCount gates player/team/world from the group's `allowed.{empire,team,
		// world}`; here only the PLAYER scope is checked, against the engine getMaxPlayerInstances (value-equivalent to
		// the group's allowed.empire -- the curator maps iMaxPlayerInstances -> allowed.empire).
		// TODO(port): SpecialBuilding team/world group cap needs a group->members index + group `allowed` available to
		// the cascade (StoneBase SpecialBuildingGroup / its team+world caps). BLOCKED today: SPECIALBUILDING_ is not in
		// readJson's RJ_REPO_TYPES, so no InfoRepo<CvSpecialBuildingInfo> (group allowed) nor a group->members index is
		// parsed. DATA-BENIGN: all 7 group special-buildings are allowed.empire:1 only -- no team/world group cap exists,
		// so the player-scope check is complete for current data. Wire the group InfoRepo + members index to lift this.
		const SpecialBuildingTypes sb = bi.getSpecialBuilding();
		if (sb != NO_SPECIALBUILDING)
		{
			const int cap = GC.getSpecialBuildingInfo(sb).getMaxPlayerInstances();
			if (cap != -1 && specialCount[(int)sb] >= cap) continue;
		}
		// DORMANT-ON-BUILD: a building whose successor (operate.dormant trigger) is already present is born dormant.
		bool bornDormant = false;
		if (j != NULL)
			for (size_t i = 0; i < j->dormantTriggers.size() && !bornDormant; ++i)
				if (pCity->hasBuilding((BuildingTypes)j->dormantTriggers[i])) bornDormant = true;
		if (bornDormant) continue;
		// PREREQ-AMOUNT scaling (StoneBase): each PrereqNumOfBuildings prereq needs a SCALED count of that building.
		const bool selfLimited = (j != NULL && !j->allowed.empty());
		const bool selfNoScale = bi.isForceNoPrereqScaling();
		const int selfCount = cascadeTally().buildingCount((int)kPlayer.getID(), b, CASCADE_COUNT_EMPIRE);
		bool amountFailed = false;
		const IDValueMap<BuildingTypes, int>& prereqs = bi.getPrereqNumOfBuildings();
		for (IDValueMap<BuildingTypes, int>::const_iterator it = prereqs.begin(); it != prereqs.end() && !amountFailed; ++it)
		{
			const CvJsonInfo* pj = InfoRepo<CvBuildingInfo>::get().get((int)it->first);
			const bool prereqLimited = (pj != NULL && !pj->allowed.empty());
			const int required = scaledPrereq(it->second, wsMod, selfLimited, prereqLimited, selfNoScale, selfCount);
			if (cascadeTally().buildingCount((int)kPlayer.getID(), (int)it->first, CASCADE_COUNT_EMPIRE) < required) amountFailed = true;
		}
		if (amountFailed) continue;
		// GATE: requires.build (strict) + requires.operate (ignoreDisabled -- positive prereqs only).
		if (j != NULL)
		{
			if (j->requiresBuild != NULL && !cascadeEvalCondition(j->requiresBuild, ec, buildFlags)) continue;
			if (j->requiresOperate != NULL && !cascadeEvalCondition(j->requiresOperate, ec, operFlags)) continue;
		}
		avail.insert(b);
	}
}
