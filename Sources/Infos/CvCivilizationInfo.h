#pragma once
#ifndef CV_CIVILIZATION_INFO_H
#define CV_CIVILIZATION_INFO_H

//
//	CvCivilizationInfo -- the CIVILIZATION poco rebuilt to the exemplar surface (patterns.md § THE GETTER SETUP).
//	Styled for the JSON anatomy (json.md §2): the game-start provisions (grants.buildings/techs/civics) ride the
//	composed grants payload and are served as typed FK views materialized at mapFrom ([DEC-materialize-at-mapfrom] --
//	bare member reads, never per-call bucket-string walks); the NPC research ban (disables.techs) rides the
//	composed edges; the spawnRate straggler compiles into the composed modifiers (base getScalar read).
//	Selectability (identity.playable/aiPlayable) is load-only identity metadata (json.md §7); art / sound /
//	text / leaders / cityNames are intrinsic self-description. No legacy getter name survives
//	([DEC-new-getter-surface]).
//
//	This ONE engine class is both the typed getCivilizationInfo(...) consumer surface AND the JSON payload
//	(InfoRepo aliases GC.m_paCivilizationInfo); there is no separate poco. The DllExport reads are EXE-bound
//	(civ-selection UI) and keep their exact signatures.
//

#include "CvInfo.h"
#include "Defines/CvEnums.h"   // BuildingTypes / TechTypes / CivicTypes / CivicOptionTypes / LeaderHeadTypes
#include <string>
#include <vector>

class CvArtInfoCivilization;

class CvCivilizationInfo : public CvInfo
{
public:
	CvCivilizationInfo();
	virtual void reset();

	virtual void mapFrom(const picojson::value& entity);

	// ======================= 1. SECTIONS -- whole typed objects =======================
	virtual const CvEdges*     getEdges()     const { return &m_edges; }
	virtual const CvModifiers* getModifiers() const { return &m_modifiers; }

	// ======================= 2. CLASSIFICATION -- none (civilizations author no §8 block) ====================

	// ======================= 3. MODIFIER GROUPS -- the spawnRate straggler ===================================
	// (spawnRate.empire.npcPeace.percent -- authored by the 6 NPC civs -- is a 1-kind straggler riding the base
	// getScalar(SCALAR_SPAWN_RATE_NPC_PEACE, CASC_SCOPE_EMPIRE, CASC_UNIT_PERCENT) over the composed
	// modifiers; no per-family point read lives here.)

	// ======================= 4. GAME-START GRANT VIEWS -- typed FK vectors off the composed units ============
	// Pure VIEWS of the same data the grants machine resolves (ONE representation, materialized at mapFrom off
	// the composed grants/edges units, never a second raw-JSON walk). They exist only because the apply still
	// runs at the scattered legacy sites; when the grants machine owns the apply, they and those sites delete
	// together with nothing else reading a private copy.
	const std::vector<BuildingTypes>& getFreeBuildings() const { return m_freeBuildings; }   // grants.buildings
	const std::vector<TechTypes>& getFreeTechs() const { return m_freeTechs; }               // grants.techs
	bool isFreeTech(TechTypes eTech) const;
	// grants.civics keyed into CivicOption slots -- one civic per option; NO_CIVIC when the slot is unfilled
	CivicTypes getInitialCivic(CivicOptionTypes eCivicOption) const;
	// load-window writer -- cvInternalGlobals::checkInitialCivics defaults an unfilled option slot at load
	void setInitialCivic(CivicOptionTypes eCivicOption, CivicTypes eCivic);
	// disables.techs -- the per-civ research ban (canEverResearch false while the civ is active)
	const std::vector<TechTypes>& getDisabledTechs() const { return m_disabledTechs; }
	bool isTechDisabled(TechTypes eTech) const;

	// ======================= 5. INTRINSIC -- bare typed reads (the census identity set) ======================
	// selectability -- load-only identity metadata (json.md §7); EXE-bound (civ-selection screen)
	DllExport bool isPlayable() const { return m_bPlayable; }       // identity.playable
	DllExport bool isAIPlayable() const { return m_bAIPlayable; }   // identity.aiPlayable
	// the NPC build-lockdown (paired with the unit/building EnabledCivilization gate)
	bool isStronglyRestricted() const { return m_bStronglyRestricted; }   // identity.stronglyRestricted
	CivilizationTypes getDerivativeCiv() const { return m_eDerivativeCiv; }   // identity.derivativeCiv (FK)
	// leaders -- identity.leaders (FK list); the count feeds random-leader generation
	const std::vector<LeaderHeadTypes>& getLeaders() const { return m_leaders; }
	int getNumLeaders() const { return (int)m_leaders.size(); }
	DllExport bool isLeaders(int i) const;   // EXE-bound membership read (a LEADER_ engine id)
	// city-name pool -- identity.cityNames (TXT keys, kept as strings)
	int getNumCityNames() const { return (int)m_cityNames.size(); }
	std::string getCityName(int iName) const;
	// TEXT -- the per-form reads + their keys
	std::wstring pyGetShortDescription(uint uiForm) { return getShortDescription(uiForm); }
	DllExport const wchar_t* getShortDescription(uint uiForm = 0);
	const wchar_t* getShortDescriptionKey() const { return m_szShortDescriptionKey; }
	const std::wstring pyGetShortDescriptionKey() const { return getShortDescriptionKey(); }
	std::wstring pyGetAdjective(uint uiForm) { return getAdjective(uiForm); }
	DllExport const wchar_t* getAdjective(uint uiForm = 0);
	const wchar_t* getAdjectiveKey() const { return m_szAdjectiveKey; }
	const std::wstring pyGetAdjectiveKey() const { return getAdjectiveKey(); }
	// world.art -- player color / art styles (FK-resolved ids) + the ART_DEF_* tag
	DllExport int getDefaultPlayerColor() const { return m_iDefaultPlayerColor; }   // world.art.playerColor
	int getArtStyleType() const { return m_iArtStyleType; }           // world.art.style
	int getUnitArtStyleType() const { return m_iUnitArtStyleType; }   // world.art.unitStyle
	const char* getArtDefineTag() const { return m_szArtDefineTag; }  // world.art.define (ART_DEF_* tag)
	const CvArtInfoCivilization* getArtInfo() const;
	const char* getButton() const;
	DllExport const char* getFlagTexture() const;
	// sound.selection / sound.action -> runtime audio-tag indices (AUDIOTAG_3DSCRIPT; -1 = none)
	int getSelectionSoundScriptId() const { return m_iSelectionSoundScriptId; }
	int getActionSoundScriptId() const { return m_iActionSoundScriptId; }

	virtual const CvTriggers*  getTriggers()  const { return &m_triggers; }   // §5 -- triggers + the folded grants

protected:
	virtual CvTriggers*  mutTriggers()  { return &m_triggers; }
	virtual CvEdges*     mutEdges()     { return &m_edges; }
	virtual CvModifiers* mutModifiers() { return &m_modifiers; }

private:
	// --- the composed section units ---
	CvTriggers  m_triggers;    // §5 -- the game-start provisions, the null-condition trigger entry
	CvEdges     m_edges;       // §4.1/4.2 -- the enables-family edges (the NPC disables.techs research ban)
	CvModifiers m_modifiers;   // §6 families: spawnRate.empire.npcPeace.percent

	// --- the typed game-start grant views (materialized at mapFrom off the composed units) ---
	std::vector<BuildingTypes> m_freeBuildings;   // grants.buildings
	std::vector<TechTypes> m_freeTechs;           // grants.techs
	std::vector<CivicTypes> m_initialCivics;      // grants.civics, one slot per CivicOptionTypes; NO_CIVIC unfilled
	std::vector<TechTypes> m_disabledTechs;       // disables.techs (the composed-edges view)

	// --- the intrinsic identity members (materialized once at mapFrom; getters are bare reads) ---
	bool m_bPlayable;
	bool m_bAIPlayable;
	bool m_bStronglyRestricted;
	CivilizationTypes m_eDerivativeCiv;
	std::vector<LeaderHeadTypes> m_leaders;
	std::vector<CvString> m_cityNames;
	CvWString m_szShortDescriptionKey;
	CvWString m_szAdjectiveKey;
	int m_iDefaultPlayerColor;
	int m_iArtStyleType;
	int m_iUnitArtStyleType;
	CvString m_szArtDefineTag;
	int m_iSelectionSoundScriptId;
	int m_iActionSoundScriptId;

	// per-form localized-text render caches (the CvInfoBase text idiom; cleared by reset/mapFrom)
	mutable std::vector<CvWString> m_aszShortDescription;
	mutable std::vector<CvWString> m_aszAdjective;
};

#endif // CV_CIVILIZATION_INFO_H
