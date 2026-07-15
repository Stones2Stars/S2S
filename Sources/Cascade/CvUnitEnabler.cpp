//
//	UnitEnabler -- the UNITS domain on the standardized enabler component (see the header): the per-city
//	trainable vector's seed + O(delta) event appliers. The domain arrays are the ONLY mutable state; every
//	HAVE-event applies its source's unit edges DIRECTLY through the ONE kernel applier.
//

#include "CvGameCoreDLL.h"
#include "CvUnitEnabler.h"
#include "CvEnabler.h"            // EnablerDomain/CityEnabler -- the standardized per-city domain (CvCity::m_enabler)
#include "CvEnablerKernel.h"      // EnablerKernel::applyEdges / obsoletedByHeldTech / obsoletedByOtherHeldTech
#include "CvInfo.h"
#include "CvUnitInfo.h"           // spawnOnly -- the static never-trainable exclusion
#include "CvBuildingInfo.h"
#include "CvTechInfo.h"           // cascadeStartNode -- the TECH_GAME_START root redirect (the tech HAVE axis)
#include "CvCivicInfo.h"
#include "CvReligionInfo.h"
#include "CvBonusInfo.h"
#include "Repos/InfoRepo.h"
#include "AI/CvPlayerAI.h"        // GET_PLAYER
#include "AI/CvTeamAI.h"          // GET_TEAM
#include "Defines/CvGlobals.h"
#include "Engine/CvCity.h"
#include "Engine/CvPlayer.h"
#include "Engine/CvTeam.h"

// the source's cascade info per axis (the tech axis redirects the TECH_GAME_START root to cascadeStartNode).
// Units' HAVE axes per the store's enables.units inversion sources: techs, buildings, bonuses, religions, civics.
static const CvInfo* ud_techJson(int iTech)
{
	if (iTech == GC.getInfoTypeForString("TECH_GAME_START", true)) return &cascadeStartNode();
	return InfoRepo<CvTechInfo>::get().get(iTech);
}

// The CITY-CREATED applier (founding init + the load read's start, BEFORE the city's own in-read emits): init
// the domain (size + the spawnOnly static exclusions) and fold ONLY the cross-scope HAVE that predates the
// city -- team techs + player civics. The city's OWN facts (buildings/religions/bonuses) arrive as DOMAIN
// events -- at load from the in-read reseed emits, at founding from the real grant/build emits -- one
// mechanism (DEC-spine-reseed).
void UnitEnabler::onCityCreated(const CvCity& kCity)
{
	const CvPlayer& kPlayer = GET_PLAYER(kCity.getOwner());
	const CvTeam& kTeam = GET_TEAM(kPlayer.getTeam());
	EnablerDomain& d = kCity.m_enabler.units;
	d.init(GC.getNumUnitInfos());
	for (int u = 0; u < GC.getNumUnitInfos(); ++u)
	{
		const CvUnitInfo* ju = (const CvUnitInfo*)InfoRepo<CvUnitInfo>::get().get(u);
		if (ju != NULL && ju->spawnOnly) d.setStaticExcluded(u, true);   // never trainable (placed by systems)
	}
	for (int t = 0; t < GC.getNumTechInfos(); ++t)   // the root IS a held tech (the load backfill guarantees it)
		if (kTeam.isHasTech((TechTypes)t)) EnablerKernel::applyEdges(d, ud_techJson(t), EDGEB_UNITS, +1);
	for (int co = 0; co < GC.getNumCivicOptionInfos(); ++co)
	{
		const CivicTypes c = kPlayer.getCivics((CivicOptionTypes)co);
		if (c != NO_CIVIC) EnablerKernel::applyEdges(d, InfoRepo<CvCivicInfo>::get().get((int)c), EDGEB_UNITS, +1);
	}
}

// The tech delta. Broad emit -> the PLAYER tech domain's held flag is the flip guard; the invalidation route
// runs this BEFORE TechEnabler::onTechChanged flips it (the ordering contract, shared with the buildings domain).
void UnitEnabler::onCityTechChanged(TeamTypes eTeam, TechTypes eTech, bool bHas)
{
	if (eTeam == NO_TEAM || eTech == NO_TECH) return;
	const CvInfo* jt = ud_techJson((int)eTech);
	const std::vector<int>* obsB = jt ? jt->edge(EDGEF_OBSOLETES, EDGEB_BUILDINGS) : NULL;
	const CvTeam& kTeam = GET_TEAM(eTeam);
	for (int iP = 0; iP < MAX_PLAYERS; iP++)
	{
		CvPlayer& kPlayer = GET_PLAYER((PlayerTypes)iP);
		if (kPlayer.getTeam() != eTeam) continue;
		if (!kPlayer.m_enabler.techs.isSeeded() || kPlayer.m_enabler.techs.isHeld((int)eTech) == bHas) continue;
		const int iDelta = bHas ? +1 : -1;
		foreach_(CvCity* pCity, kPlayer.cities())
		{
			EnablerDomain& d = pCity->m_enabler.units;
			if (!d.isSeeded()) continue;
			EnablerKernel::applyEdges(d, jt, EDGEB_UNITS, iDelta);
			// the obsolete-present ripple: a PRESENT building this tech obsoletes stops/resumes enabling its units
			if (obsB != NULL)
				for (size_t i = 0; i < obsB->size(); ++i)
				{
					const int b = (*obsB)[i];
					if (!pCity->hasBuilding((BuildingTypes)b)) continue;
					const CvInfo* jb = InfoRepo<CvBuildingInfo>::get().get(b);
					if (!EnablerKernel::obsoletedByOtherHeldTech(jb, kTeam, eTech)) EnablerKernel::applyEdges(d, jb, EDGEB_UNITS, -iDelta);
				}
		}
	}
}

void UnitEnabler::onCityBuildingChanged(const CvCity& kCity, int iBuilding, bool bPresent)
{
	EnablerDomain& d = kCity.m_enabler.units;
	if (!d.isSeeded() || iBuilding < 0) return;
	// the flip guard is the BUILDINGS domain's held flag (pre-flip: this runs BEFORE BuildingEnabler's handler)
	if (kCity.m_enabler.buildings.isSeeded() && kCity.m_enabler.buildings.isHeld(iBuilding) == bPresent) return;
	const CvPlayer& kPlayer = GET_PLAYER(kCity.getOwner());
	const CvInfo* jb = InfoRepo<CvBuildingInfo>::get().get(iBuilding);
	if (!EnablerKernel::obsoletedByHeldTech(jb, GET_TEAM(kPlayer.getTeam())))
		EnablerKernel::applyEdges(d, jb, EDGEB_UNITS, bPresent ? +1 : -1);
}

void UnitEnabler::onCityReligionChanged(const CvCity& kCity, int iReligion, bool bHas)
{
	EnablerDomain& d = kCity.m_enabler.units;
	if (!d.isSeeded()) return;   // flip-guarded emit (setHasReligion)
	EnablerKernel::applyEdges(d, InfoRepo<CvReligionInfo>::get().get(iReligion), EDGEB_UNITS, bHas ? +1 : -1);
}

void UnitEnabler::onCityBonusChanged(const CvCity& kCity, int iBonus, int iChange)
{
	EnablerDomain& d = kCity.m_enabler.units;
	if (!d.isSeeded() || iBonus < 0 || iChange == 0) return;
	const int iNew = kCity.getNumBonuses((BonusTypes)iBonus);
	const int iOld = iNew - iChange;
	if ((iOld > 0) == (iNew > 0)) return;   // HAVE = count > 0: apply only on a crossing
	EnablerKernel::applyEdges(d, InfoRepo<CvBonusInfo>::get().get(iBonus), EDGEB_UNITS, iNew > 0 ? +1 : -1);
}

void UnitEnabler::onPlayerCivicsChanged(PlayerTypes ePlayer, int iOldCivic, int iNewCivic)
{
	if (ePlayer == NO_PLAYER || iOldCivic == iNewCivic) return;
	CvPlayer& kPlayer = GET_PLAYER(ePlayer);
	foreach_(CvCity* pCity, kPlayer.cities())
	{
		EnablerDomain& d = pCity->m_enabler.units;
		if (!d.isSeeded()) continue;
		if (iOldCivic >= 0) EnablerKernel::applyEdges(d, InfoRepo<CvCivicInfo>::get().get(iOldCivic), EDGEB_UNITS, -1);
		if (iNewCivic >= 0) EnablerKernel::applyEdges(d, InfoRepo<CvCivicInfo>::get().get(iNewCivic), EDGEB_UNITS, +1);
	}
}
