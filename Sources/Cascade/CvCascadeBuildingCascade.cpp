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
	CvCascadeEvalCtx ec; ec.city = pCity; ec.player = &kPlayer; ec.team = &kTeam; ec.waivedPrereqBuildings = &waived;
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
		// EXCLUDE never-buildable: productionCost < 0 = the engine's "can never be built" marker (notConstructible,
		// OUTSIDE canConstruct -- owner ruling 2026-06-30), as is an auto-placed building.
		if (bi.getProductionCost() < 0 || bi.isAutoBuild()) continue;
		if (capped(j, b, kPlayer)) continue;                            // INSTANCE CAP (created + making >= allowed)
		// SPECIALBUILDING GROUP CAP: a member leaves buildable once its group count >= getMaxPlayerInstances (engine
		// special buildings have only a player cap; -1 = uncapped).
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
