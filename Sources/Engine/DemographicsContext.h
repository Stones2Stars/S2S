#pragma once
#ifndef CV_DEMOGRAPHICS_CONTEXT_H
#define CV_DEMOGRAPHICS_CONTEXT_H

//
//	DemographicsContext -- the empire's POWER and ASSETS ledgers: storage, maintenance and the declared interest
//	set in ONE place. What is stored, what is forwarded and why: docs/cascade/11-context-stores-vs-forwards.md
//	§ THE DEMOGRAPHICS LEDGERS.
//
//	⚑ ONE CONSUMER, THREE LEDGERS -- the player's (CvPlayer::demographics), each city's (CvCity::buildingWorth) and
//	each area's per-player military worth (CvArea::changeMilitaryWorth) -- because one fact moves several of them
//	at once. Each object owns its ledger; this consumer is the one thing that writes them.
//
//	⛔ IT ONLY CONSUMES. Facts in, state out, nothing back.
//
//	⚠ REGISTRATION ORDER: inside the CONTEXTS band of contexts -> enabler -> modifier -> triggers.
//

#include "WorthLedger.h"

struct CvSpineEvent;

///<summary>The power a number of citizens adds: 1 each.</summary>
inline int citizenPower(int iCitizens) { return 100 * iCitizens; }
///<summary>The assets a number of citizens or scored land plots adds: 10 each.</summary>
inline int countAssets(int iCount) { return 1000 * iCount; }

///<summary>The empire's power and assets over what it holds -- its units, its held techs and its operating
/// buildings. Population and scored land are forwarded by CvPlayer::getPower / getAssets.</summary>
class DemographicsContext
{
public:
	///<summary>The military worth of the empire's counted units.</summary>
	int unitPower() const { return m_units.militaryWorth; }
	///<summary>The military worth of the empire's held techs.</summary>
	int techPower() const { return m_techs.militaryWorth; }
	///<summary>The military worth of everything the empire holds.</summary>
	int heldPower() const { return m_units.militaryWorth + m_techs.militaryWorth + m_buildings.militaryWorth; }
	///<summary>The worth of everything the empire holds.</summary>
	int heldAssets() const { return m_units.worth + m_techs.worth + m_buildings.worth; }

	///<summary>The facts that maintain the ledgers -- the declared interest set, stated at the store.</summary>
	static bool wantsEvent(int iEventId);
	///<summary>Applies one fact to the ledgers it moves.</summary>
	static void onSpineEvent(const CvSpineEvent& kEvent);

	// ⛔ CONSTRAINT: written by the consumer above and nothing else. Public only because the consumer reaches the
	// ledger through the owning player; a direct call beside the emit would be a second surface maintaining one fact.
	///<summary>The units' ledger.</summary>
	WorthLedger& units() { return m_units; }
	///<summary>The held techs' ledger.</summary>
	WorthLedger& techs() { return m_techs; }
	///<summary>The operating buildings' ledger.</summary>
	WorthLedger& buildings() { return m_buildings; }
	///<summary>Zeroes every ledger, for a player reset.</summary>
	void clear();

private:
	WorthLedger m_units;
	WorthLedger m_techs;
	WorthLedger m_buildings;
};

///<summary>Registers the consumer on the event spine (from spineRegisterConsumers; idempotent).</summary>
void demographicsContextRegisterConsumer();

#endif // CV_DEMOGRAPHICS_CONTEXT_H
