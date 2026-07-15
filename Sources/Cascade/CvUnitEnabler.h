#pragma once
#ifndef CV_UNIT_ENABLER_H
#define CV_UNIT_ENABLER_H

//
//	UnitEnabler -- the UNITS domain's PURE CALCULATORS on the standardized enabler component (enabler.md
//	par.7/7.1; CvEnabler.h): onCityCreated (the lifecycle init + cross-scope fold) + the onCity* event-delta
//	appliers that maintain CvCity::m_enabler.units. The content is built PURELY from DOMAIN events -- the load
//	reseed's in-read emits and the play-time emits are one mechanism (DEC-spine-reseed); only the cross-scope
//	HAVE that predates the city (team techs + player civics) folds at city creation. A static is a calculator
//	ONLY -- reads are the owner's bare member lookups (canTrain reads m_enabler.units.listed directly).
//	Enable-side stage: no requires gate, no allowed caps, no superseder/dormant-upgrade gates (those are the
//	requires stage); a unit never leaves the frontier on being trained (no held flag) -- spawnOnly is the
//	static never-trainable exclusion.
//
//	Purely-organizational static-methods class: NO data members, never instantiated, no per-instance state.
//

class CvCity;

class UnitEnabler
{
public:
	// The city-created applier: init (size + static exclusions) + fold the cross-scope HAVE that predates the
	// city (team techs + player civics). Called at founding (CvCity::init) and at the start of the city's save
	// read, BEFORE its own in-read emits stream. The city's own facts arrive as events.
	static void onCityCreated(const CvCity& kCity);
	// MUST run BEFORE TechEnabler::onTechChanged (the player tech domain's held flag is the broad-emit flip guard)
	static void onCityTechChanged(TeamTypes eTeam, TechTypes eTech, bool bHas);
	// MUST run BEFORE BuildingEnabler::onCityBuildingChanged (the buildings domain's held flag is the flip guard)
	static void onCityBuildingChanged(const CvCity& kCity, int iBuilding, bool bPresent);
	static void onCityReligionChanged(const CvCity& kCity, int iReligion, bool bHas);
	static void onCityBonusChanged(const CvCity& kCity, int iBonus, int iChange);   // count delta; applies on a 0-crossing
	static void onPlayerCivicsChanged(PlayerTypes ePlayer, int iOldCivic, int iNewCivic);
};

#endif // CV_UNIT_ENABLER_H
