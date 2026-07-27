#pragma once

#ifndef CV_ERA_INFO_H
#define CV_ERA_INFO_H

//
//	CvEraInfo -- the ERA poco rebuilt to the exemplar surface (patterns.md § THE GETTER SETUP; wave D, the
//	config-heavy cut). An era is a CONFIG entity (state-repositories.md § WORLD is CONFIG): world-scope cost
//	multipliers read from their source (the compiled sums), never cached behind a dirty protocol; pacing
//	identity + one-shot starting grants + era audio are plain config. JSON-fed (Assets/Data/eras/*.json via
//	mapFrom); no XML read (DEC-no-xml-into-game).
//
//	The legacy modifier-family scalar MIRRORS are DEAD: the costs.world percents and the durations anger
//	multiplier read via the parameterized point getters below; the singleton world families
//	(growth / greatPeopleRate / eventChance) are stragglers riding the base
//	getScalar(SCALAR_GROWTH / SCALAR_GREAT_PEOPLE_RATE / SCALAR_EVENT_CHANCE, CASC_SCOPE_WORLD, ...).
//
//	Calendar pacing: the era's real-history year span and its Normal-speed turn count are identity config;
//	other speeds scale the turn count by the gamespeed's speed percent and CvDate interpolates dates from the
//	year span. Eras must be contiguous (start == previous era's end).
//

#include "CvInfo.h"   // JSON-info base (mapFrom); on /I -> bare include

namespace picojson { class value; }

class CvEraInfo : public CvInfo
{
public:

	CvEraInfo();
	virtual ~CvEraInfo();

	virtual void mapFrom(const picojson::value& entity);

	// ======================= 1. SECTIONS -- whole typed objects =======================
	virtual const CvModifiers* getModifiers() const { return &m_modifiers; }
	// `grants.<channel>: N` is a §5 numeric PULSE parsed by the base dispatch; the view scalars below read
	// the COMPOSED unit, so the grants machine and the engine views serve one representation.

	// ======================= 3. MODIFIER GROUPS -- point reads over the compiled sums =================
	// (Conditioned-list access + the expected* valuations are the base CvInfo surface; kind and scope are
	// separate arguments -- [DEC-scope-is-an-axis]; every magnitude is ×100 -- [DEC-fixedpoint-x100].)
	int getCostsModifier(CostsKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_COSTS, (int)eKind, eScope, CASC_UNIT_PERCENT); }
	int getDurationsModifier(DurationsKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_DURATIONS, (int)eKind, eScope, infoKindUnit(MODFAM_DURATIONS, (int)eKind)); }
	// (growth / greatPeopleRate / eventChance are 1-kind stragglers: the base getScalar(SCALAR_*) covers them.)

	// ======================= 4. INTRINSIC -- bare typed reads (config; human values) ==================
	// grants views (materialized from the composed §5 unit).
	int getStartingUnitMultiplier() const { return m_iStartingUnitMultiplier; }
	int getStartingDefenseUnits() const { return m_iStartingDefenseUnits; }
	int getStartingWorkerUnits() const { return m_iStartingWorkerUnits; }
	int getStartingExploreUnits() const { return m_iStartingExploreUnits; }
	int getStartingGold() const { return m_iStartingGold; }
	int getFreePopulation() const { return m_iFreePopulation; }
	// identity: the era's 1-based sequence position (the §3.1 ERA counter value -- eras are ORDERED data)
	// + the advanced-start budget + the calendar pacing inputs.
	int getOrder() const { return m_iOrder; }
	int getAdvancedStartPoints() const { return m_iAdvancedStartPoints; }
	int getHistoricalStartYear() const { return m_iHistoricalStartYear; }
	int getHistoricalEndYear() const { return m_iHistoricalEndYear; }
	int getNormalSpeedTurns() const { return m_iNormalSpeedTurns; }
	// sound: the era audio config (§7 sound -- scripts, soundtrack config, the two resolved audio-tag arrays).
	int getSoundtrackSpace() const { return m_iSoundtrackSpace; }
	int getNumSoundtracks() const { return m_iNumSoundtracks; }
	bool isFirstSoundtrackFirst() const { return m_bFirstSoundtrackFirst; }
	const char* getAudioUnitVictoryScript() const { return m_szAudioUnitVictoryScript; }
	const char* getAudioUnitDefeatScript() const { return m_szAudioUnitDefeatScript; }
	int getSoundtracks(int iIndex) const;
	int getCitySoundscapeScriptId(int iCitySize) const;

	virtual const CvTriggers*  getTriggers()  const { return &m_triggers; }   // §5 -- triggers + the folded grants

protected:
	virtual CvModifiers* mutModifiers() { return &m_modifiers; }
	virtual CvTriggers*  mutTriggers()  { return &m_triggers; }

private:
	// --- the composed section units ---
	CvModifiers m_modifiers;   // §6 families: costs.world.* / growth / greatPeopleRate / durations / eventChance
	CvTriggers  m_triggers;    // §5 -- the game-start starting-gold/units pulses

	// --- the intrinsic identity members (materialized once at mapFrom) ---
	int m_iStartingUnitMultiplier;
	int m_iStartingDefenseUnits;
	int m_iStartingWorkerUnits;
	int m_iStartingExploreUnits;
	int m_iStartingGold;
	int m_iFreePopulation;
	int m_iOrder;
	int m_iAdvancedStartPoints;
	int m_iHistoricalStartYear;
	int m_iHistoricalEndYear;
	int m_iNormalSpeedTurns;
	int m_iSoundtrackSpace;
	int m_iNumSoundtracks;
	bool m_bFirstSoundtrackFirst;
	CvString m_szAudioUnitVictoryScript;
	CvString m_szAudioUnitDefeatScript;
	int* m_paiSoundtracks;
	int* m_paiCitySoundscapeScriptIds;
};

#endif // CV_ERA_INFO_H
