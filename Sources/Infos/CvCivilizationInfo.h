#pragma once

#ifndef CV_CIVILIZATION_INFO_H
#define CV_CIVILIZATION_INFO_H

#include "CvInfo.h"   // JSON-info base (mapFrom); on /I -> bare include

namespace picojson { class value; }

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  class : CvCivilizationInfo
//
//  DESC:   A civilization (game-start grants + per-civ art/identity). #430: JSON-fed
//          (Assets/Data/civilizations/*.json via mapFrom); no XML read. This ONE engine
//          class is both the typed getCivilizationInfo(...) consumer surface AND the JSON
//          payload (InfoRepo aliases GC.m_paCivilizationInfo); there is no separate poco.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
class CvArtInfoCivilization;
class CvCivilizationInfo : public CvInfo
{
	//---------------------------PUBLIC INTERFACE---------------------------------
public:

	CvCivilizationInfo();
	virtual ~CvCivilizationInfo();
	virtual void reset();

	DllExport int getDefaultPlayerColor() const;
	int getArtStyleType() const;
	int getUnitArtStyleType() const;
	int getNumCityNames() const;
	// the number of leaders the Civ has, this is needed so that random leaders can be generated easily
	int getNumLeaders() const;
	int getSelectionSoundScriptId() const;
	int getActionSoundScriptId() const;

	DllExport bool isAIPlayable() const;
	DllExport bool isPlayable() const;

	std::wstring pyGetShortDescription(uint uiForm) { return getShortDescription(uiForm); }
	DllExport const wchar_t* getShortDescription(uint uiForm = 0);
	const wchar_t* getShortDescriptionKey() const;
	const std::wstring pyGetShortDescriptionKey() const { return getShortDescriptionKey(); }

	std::wstring pyGetAdjective(uint uiForm) { return getAdjective(uiForm); }
	DllExport const wchar_t* getAdjective(uint uiForm = 0);
	const wchar_t* getAdjectiveKey() const;
	const std::wstring pyGetAdjectiveKey() const { return getAdjectiveKey(); }

	DllExport const char* getFlagTexture() const;
	const char* getArtDefineTag() const;

	int getCivilizationInitialCivics(int i) const;
	void setCivilizationInitialCivics(int iCivicOption, int iCivic);

	DllExport bool isLeaders(int i) const;

	bool isCivilizationFreeTechs(int i) const;
	bool isCivilizationDisableTechs(int i) const;

	int getNumCivilizationBuildings() const;
	int getCivilizationBuilding(int i) const;
	bool isCivilizationBuilding(int i) const;

	std::string getCityNames(int i) const;

	const CvArtInfoCivilization* getArtInfo() const;
	const char* getButton() const;

	CivilizationTypes getDerivativeCiv() const { return m_iDerivativeCiv; }

	//TB Tags
	//int
	int getSpawnRateModifier() const;
	int getSpawnRateNPCPeaceModifier() const;
	// Means that the civilization cannot build or train anything which is not specified
	// as allowed in Unit or Building Info by the EnabledCivilization tag.  Generally used for NPC players.
	bool isStronglyRestricted() const;

	// The COMPOSED section units. Without them the base dispatch parses neither block: `grants` reached only this
	// class's own typed members (so the grants machine's game-start resolution counted 0 and silently no-op'd), and
	// the `enables`-family edges reached nothing at all (the NPC research ban). Composing them puts the civ on the
	// generic surface every cross-cutting reader uses (the grants machine, the enabler, /state/info).
	virtual const CvGrants* getGrants() const { return &m_grants; }
	virtual CvGrants*       mutGrants()       { return &m_grants; }
	virtual const CvEdges*  getEdges()  const { return &m_edges; }
	virtual CvEdges*        mutEdges()        { return &m_edges; }

	virtual void mapFrom(const picojson::value& entity);

	//----------------------PROTECTED MEMBER VARIABLES----------------------------
protected:
	CvGrants m_grants;   // §5 -- the game-start provisions (civics/techs/buildings)
	CvEdges  m_edges;    // §4.1/4.2 -- the enables-family edges (the NPC `disables` research ban)

	int m_iDefaultPlayerColor;
	int m_iArtStyleType;
	int m_iUnitArtStyleType;
	int m_iNumCityNames;
	int m_iNumLeaders;
	int m_iSelectionSoundScriptId;
	int m_iActionSoundScriptId;
	CivilizationTypes m_iDerivativeCiv;
	// TB Tags
	int m_iSpawnRateModifier;
	int m_iSpawnRateNPCPeaceModifier;

	bool m_bStronglyRestricted;
	// ! TB Tags
	bool m_bAIPlayable;
	bool m_bPlayable;

	CvString m_szArtDefineTag;
	CvWString m_szShortDescriptionKey;
	CvWString m_szAdjectiveKey;

	int* m_piCivilizationInitialCivics;

	std::vector<LeaderHeadTypes> m_aeLeaders;
	std::vector<TechTypes> m_aeCivilizationFreeTechs;
	std::vector<TechTypes> m_aeCivilizationDisableTechs;

	CvString* m_paszCityNames;

	std::vector<int> m_aiCivilizationBuildings;

	mutable std::vector<CvWString> m_aszShortDescription;
	mutable std::vector<CvWString> m_aszAdjective;
};

#endif // CV_CIVILIZATION_INFO_H
