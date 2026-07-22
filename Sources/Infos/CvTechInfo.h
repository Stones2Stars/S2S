#pragma once
#ifndef CV_JSON_TECH_INFO_H
#define CV_JSON_TECH_INFO_H

//
//	CvTechInfo -- the per-type cascade info for TECHS. TWO parts:
//	(1) the empire-ability blocks this tech PROVIDES when held (json.md §8; capabilities.md): the flat `capabilities`
//	    (the composed CvJsonBoolBlock unit) + the siblings `canTrade` (trade-table items/agreements), `canTradeOn`
//	    (tradable TERRAIN_ FK refs), `canWorkOn`.
//	(2) the tech's OWN typed values (research cost, era, the empire modifiers it deposits, flags, AI, art/sound/quote).
//	A tech's UNLOCK surface (units/buildings/…/other techs) is store-inverted onto those entities' `enables.*` / this
//	tech's `requires`/`obsoletes`/`grants` -- base availability, NOT poco getters. HUMAN-native (assetValue/powerValue
//	re-apply the latent ×100, like specialist health). No cascade here.
//

#include "CvInfo.h"
#include "Defines/CvEnums.h"   // COMMERCE_* / TechTypes
#include "Defines/CvStructs.h" // PrereqBuilding (the STUB empty prereq-building struct)
#include <set>
#include <map>
#include <vector>

class CvTechInfo : public CvInfo
{
public:
	CvTechInfo();

	// (1) empire-ability grantor blocks (§8) -- derived-on-query by CascadeCapabilities. The flat `capabilities`
	// bool block is the composed m_capabilities unit (getCapabilities below); the bespoke siblings stay typed here.
	std::set<std::string> canTrade;       // canTrade:{item:true}
	std::set<int> canTradeOnTerrains;     // canTradeOn:{terrains:[TERRAIN_..]} -- FK ids
	std::set<std::string> canWorkOn;      // canWorkOn:{class:true}

	// (2) the tech's own typed values
	int getResearchCost() const { return m_iResearchCost; }                          // cost.research
	int getEra() const { return m_iEra; }                                            // identity.era (FK)
	int getAdvisorType() const { return m_iAdvisorType; }                            // ui.art.advisor (FK)
	int getTradeRoutes() const { return m_iTradeRoutes; }                            // tradeRoutes.city.flat
	int getFeatureProductionModifier() const { return m_iFeatureProductionModifier; } // featureProduction.empire.percent
	int getWorkerSpeedModifier() const { return m_iWorkerSpeedModifier; }            // workRate.empire.percent
	int getHealth() const { return m_iHealth; }                                      // health.empire.flat
	int getHappiness() const { return m_iHappiness; }                                // happiness.empire.flat
	int getGlobalTradeModifier() const { return m_iGlobalTradeModifier; }            // tradeRouteYield.empire.percent
	int getGlobalForeignTradeModifier() const { return m_iGlobalForeignTradeModifier; } // foreignTradeRouteYield.empire.percent
	int getTradeMissionModifier() const { return m_iTradeMissionModifier; }          // tradeMission.empire.percent
	int getCorporationRevenueModifier() const { return m_iCorporationRevenueModifier; } // corporationRevenue.empire.percent
	int getCorporationMaintenanceModifier() const { return m_iCorporationMaintenanceModifier; } // corporationMaintenance.empire.percent
	int getAssetValue() const { return m_iAssetValue; }                              // identity.worth (×100 re-applied)
	int getPowerValue() const { return m_iPowerValue; }                              // identity.militaryWorth (×100 re-applied)
	int getGridX() const { return m_iGridX; }                                        // identity.gridX
	int getGridY() const { return m_iGridY; }                                        // identity.gridY
	bool isRepeat() const { return m_bRepeat; }                                      // identity.repeat
	bool isTrade() const { return m_bTrade; }                                        // identity.tradeable
	bool isDisable() const { return m_bDisable; }                                    // identity.disable
	bool isGoodyTech() const { return m_bGoodyTech; }                                // identity.goodyTech
	int getAIWeight() const { return m_iAIWeight; }                                  // ai.behaviour.weight
	int getAITradeModifier() const { return m_iAITradeModifier; }                    // ai.behaviour.tradeModifier
	int getFlavorValue(int i) const { return mapGet(m_flavours, i); }                // ai.flavours {FLAVOR:int}
	int getFreeSpecialistCount(int i) const { return mapGet(m_freeSpecialists, i); } // freeSpecialists.team.{SPECIALIST} (INERT -- no write-path today)
	const char* getSoundMP() const { return m_szSoundMP.c_str(); }                   // sound.soundMP
	std::wstring getQuote() const;                                                   // identity.quote -- RESOLVES m_szQuoteKey via gDLL->getText (matches archived CvTechInfo::getQuote); returning the raw key showed "TXT_KEY_..._QUOTE" in the tech splash. std::wstring return: Boost.Python 1.32 has a std::wstring converter, none for CvWString (the getQuote() TypeError).

	int getDomainExtraMoves(int iDomain) const { return mapGet(m_domainExtraMoves, iDomain); }  // domainMoves.empire.domains.{DOMAIN}.flat
	// STUB ALWAYS-0 leftovers -- live callers exist but ZERO techs author these + no channel maps them (return 0; owner call
	//    whether to keep the getter or cut the caller at cutover).
	int getMaintenanceModifier() const { return 0; }
	int getDistanceMaintenanceModifier() const { return 0; }
	int getNumCitiesMaintenanceModifier() const { return 0; }
	int getCoastalDistanceMaintenanceModifier() const { return 0; }
	int getCommerceModifier(int /*i*/) const { return 0; }

	// --- mirrored legacy CvTechInfo getters (consumer surface; operational side deferred, owner ruling 2026-07-08) ---
	// Capability/trading flags: mirror the composed capability blocks (capabilities.md key mapping) -- real data.
	// materialized at mapFrom from the canTrade/canWorkOn blocks (bare member reads; the sets stay for cold renders)
	bool isTechTrading() const              { return m_bTradeTechs; }
	bool isGoldTrading() const              { return m_bTradeGold; }
	bool isMapTrading() const               { return m_bTradeMaps; }
	bool isOpenBordersTrading() const       { return m_bTradeOpenBorders; }
	bool isDefensivePactTrading() const     { return m_bTradeDefensivePact; }
	bool isPermanentAllianceTrading() const { return m_bTradePermanentAlliance; }
	bool isVassalStateTrading() const       { return m_bTradeVassals; }
	bool isWaterWork() const                { return m_bWorkWater; }
	// O(1) generated-id bit tests (CLS_HAS; CAPABILITY_* ids from the ClassificationRegistry)
	bool isBridgeBuilding() const           CLS_HAS(m_capabilities, CLSD_CAPABILITY, "canBuildBridges")
	bool isIrrigation() const               CLS_HAS(m_capabilities, CLSD_CAPABILITY, "canSpreadIrrigation")
	bool isIgnoreIrrigation() const         CLS_HAS(m_capabilities, CLSD_CAPABILITY, "canIgnoreIrrigation")
	bool isRiverTrade() const               CLS_HAS(m_capabilities, CLSD_CAPABILITY, "hasRiverTrade")
	bool isExtraWaterSeeFrom() const        CLS_HAS(m_capabilities, CLSD_CAPABILITY, "canSeeFurtherFromWater")
	bool isMapCentering() const             CLS_HAS(m_capabilities, CLSD_CAPABILITY, "hasCenteredMap")
	bool isMapVisible() const               CLS_HAS(m_capabilities, CLSD_CAPABILITY, "hasWholeMapRevealed")
	bool isCanPassPeaks() const             CLS_HAS(m_capabilities, CLSD_CAPABILITY, "canPassPeaks")
	bool isMoveFastPeaks() const            CLS_HAS(m_capabilities, CLSD_CAPABILITY, "canMoveFastOnPeaks")
	bool isCanFoundOnPeaks() const          CLS_HAS(m_capabilities, CLSD_CAPABILITY, "canFoundOnPeaks")
	bool isRebaseAnywhere() const           CLS_HAS(m_capabilities, CLSD_CAPABILITY, "canRebaseAnywhere")
	bool isEnablesDesertFarming() const     CLS_HAS(m_capabilities, CLSD_CAPABILITY, "canFarmDesert")
	bool isLanguage() const                 CLS_HAS(m_capabilities, CLSD_CAPABILITY, "hasLanguage")
	bool isEmbassyTrading() const           { return m_bTradeEmbassy; }
	bool getDCMAirBombTech1() const CLS_HAS(m_capabilities, CLSD_CAPABILITY, "dcmAirBomb1")   // capabilities.dcmAirBomb1 (tech_radio); interim-correct until the DCM system's planned removal (capabilities.md)
	bool getDCMAirBombTech2() const CLS_HAS(m_capabilities, CLSD_CAPABILITY, "dcmAirBomb2")   // capabilities.dcmAirBomb2 (tech_guided_weapons)
	int getFirstFreeProphet() const { return m_iFirstFreeProphet; }   // grants.firstFreeProphet (UNIT_ FK; materialized at mapFrom)
	int getInflationModifier() const { return m_iInflationModifier; }   // inflation.empire.percent (legacy iInflationModifier; feeds CvPlayer::getTechInflation, summed in getInflationMod10000)
	int* getCommerceModifierArray() const { static int s[NUM_COMMERCE_TYPES] = {0}; return s; }   // STUB zero array (paired with getCommerceModifierSTUB)
	const PrereqBuilding& getPrereqBuilding(int /*i*/) const { return m_emptyPrereqBuilding; }    // STUB empty (getNumPrereqBuildings()==0 guards it)
	const PrereqBuilding& getPrereqOrBuilding(int i) const { return m_aPrereqOrBuildings[i]; }    // requires.build.all[].any[] BUILDING_ OR-group (empire, min)
	bool isTerrainTrade(int iTerrain) const { return canTradeOnTerrains.count(iTerrain) != 0; }
	bool isCommerceFlexible(int i) const    // canSet{Science|Culture|Espionage}Rate; gold has no slider (capabilities.md)
	{ static int s_r = -1, s_c = -1, s_e = -1;
	  return (i == COMMERCE_RESEARCH  && m_capabilities.hasKey(s_r, CLSD_CAPABILITY, "canSetScienceRate"))
	      || (i == COMMERCE_CULTURE   && m_capabilities.hasKey(s_c, CLSD_CAPABILITY, "canSetCultureRate"))
	      || (i == COMMERCE_ESPIONAGE && m_capabilities.hasKey(s_e, CLSD_CAPABILITY, "canSetEspionageRate")); }

	// Relationship getters reconstructed from the composed availability units (requires/grants/allowed). The
	// multi-parent prereqs are walked out of requires.build.all/.any in mapFrom; firstFree*/global read grants/allowed.
	// leadsTo remains a STUB reverse index (operational deferred).
	bool isGlobal() const { return m_bGlobal; }                       // legacy bGlobal -> allowed.world:1 (materialized at mapFrom)
	int getFirstFreeUnit() const { return m_iFirstFreeUnit; }         // grants.firstFreeUnit (UNIT_ FK; materialized at mapFrom)
	int getFirstFreeTechs() const { return m_iFirstFreeTechs; }       // grants.freeTechs (int; materialized at mapFrom)
	int getNumLeadsToTechs() const { return (int)m_leadsTo.size(); }
	int getLeadsToTech(int iCount) const   // the iCount-th tech of the (ordered) reverse index
	{
		if (iCount < 0 || iCount >= (int)m_leadsTo.size()) return -1;
		std::set<TechTypes>::const_iterator it = m_leadsTo.begin();
		for (int k = 0; k < iCount; ++k) ++it;
		return (int)*it;
	}
	const std::set<TechTypes>& getLeadsToTechs() const { return m_leadsTo; }   // reverse index: techs that list THIS tech as a prereq (built at load in doPostLoadCaching)
	void addLeadsToTech(TechTypes eTech) { m_leadsTo.insert(eTech); }          // load-time reverse-index writer
	int getPrereqGameOption() const { return NO_GAMEOPTION; }                  // STUB entity-level game-option gate (DEC-entity-gate)
	int getNumPrereqBuildings() const { return 0; }
	int getPrereqBuildingType(int /*iIndex*/) const { return -1; }
	int getPrereqBuildingMinimumRequired(int /*iIndex*/) const { return 0; }
	int getNumPrereqOrBuildings() const { return (int)m_aPrereqOrBuildings.size(); }
	int getPrereqOrBuildingType(int iIndex) const { return (int)m_aPrereqOrBuildings[iIndex].eBuilding; }
	int getPrereqOrBuildingMinimumRequired(int iIndex) const { return m_aPrereqOrBuildings[iIndex].iMinimumRequired; }
	const char* getSound() const { return m_szSound.c_str(); }                 // sound.sound (distinct from soundMP)
	const std::vector<TechTypes>& getPrereqOrTechs() const { return m_aePrereqOrTechs; }
	const std::vector<TechTypes>& getPrereqAndTechs() const { return m_aePrereqAndTechs; }

	virtual void mapFrom(const picojson::value& entity);

	// --- the composed section units (by value; the base's mapFrom dispatch writes them via mut*) ---
	virtual const CvJsonRequires*  getRequires()     const { return &m_requires; }
	virtual const CvJsonEdges*     getEdges()        const { return &m_edges; }
	virtual const CvJsonAllowed*   getAllowed()      const { return &m_allowed; }
	virtual const CvJsonGrants*    getGrants()       const { return &m_grants; }
	virtual const CvJsonModifiers* getModifiers()    const { return &m_modifiers; }
	virtual const CvJsonBoolBlock* getCapabilities() const { return &m_capabilities; }

protected:
	virtual CvJsonRequires*  mutRequires()     { return &m_requires; }
	virtual CvJsonEdges*     mutEdges()        { return &m_edges; }
	virtual CvJsonAllowed*   mutAllowed()      { return &m_allowed; }
	virtual CvJsonGrants*    mutGrants()       { return &m_grants; }
	virtual CvJsonModifiers* mutModifiers()    { return &m_modifiers; }
	virtual CvJsonBoolBlock* mutCapabilities() { return &m_capabilities; }

private:
	static int mapGet(const std::map<int, int>& m, int k) { std::map<int, int>::const_iterator it = m.find(k); return it != m.end() ? it->second : 0; }

	CvJsonRequires  m_requires;
	CvJsonEdges     m_edges;
	CvJsonAllowed   m_allowed;
	CvJsonGrants    m_grants;
	CvJsonModifiers m_modifiers;
	CvJsonBoolBlock m_capabilities;

	int m_iResearchCost, m_iEra, m_iAdvisorType, m_iTradeRoutes, m_iFeatureProductionModifier, m_iWorkerSpeedModifier;
	int m_iInflationModifier;
	int m_iHealth, m_iHappiness, m_iGlobalTradeModifier, m_iGlobalForeignTradeModifier, m_iTradeMissionModifier;
	int m_iCorporationRevenueModifier, m_iCorporationMaintenanceModifier, m_iAssetValue, m_iPowerValue, m_iGridX, m_iGridY;
	int m_iAIWeight, m_iAITradeModifier;
	bool m_bRepeat, m_bTrade, m_bDisable, m_bGoodyTech;
	// materialized getter members (string-address reads are load-time only)
	bool m_bTradeTechs, m_bTradeGold, m_bTradeMaps, m_bTradeOpenBorders, m_bTradeDefensivePact;
	bool m_bTradePermanentAlliance, m_bTradeVassals, m_bTradeEmbassy, m_bWorkWater, m_bGlobal;
	int m_iFirstFreeProphet, m_iFirstFreeUnit, m_iFirstFreeTechs;
	std::map<int, int> m_flavours;        // FlavorTypes -> weight
	std::map<int, int> m_domainExtraMoves;  // DomainTypes -> extra moves (domainMoves.empire.domains.{DOMAIN}.flat)
	std::map<int, int> m_freeSpecialists; // SpecialistTypes -> count (inert today)
	std::string m_szSoundMP;
	std::string m_szSound;                      // sound.sound (the tech-completed jingle; distinct from soundMP)
	CvWString m_szQuoteKey;
	std::vector<TechTypes> m_aePrereqOrTechs;   // FIRST multi-member TECH any-group under requires.build.all (mirrors legacy single Or-list)
	std::vector<TechTypes> m_aePrereqAndTechs;  // team-scope TECH_ atoms in requires.build.all (AND prereqs, incl. folded 1-member ORs)
	std::vector<PrereqBuilding> m_aPrereqOrBuildings;  // the requires.build.all[].any[] BUILDING_ OR-group ((building, min) pairs)
	std::set<TechTypes> m_leadsTo;              // STUB empty reverse index (which techs this leads to)
	PrereqBuilding m_emptyPrereqBuilding;       // STUB empty -- returned by getPrereqBuilding (AND-building count is 0: no data today)
};

// The synthetic TECH_GAME_START root (no engine id -- lives OFF the InfoRepo; readjson.md §5.1): the universal
// start node whose enables/capabilities seed every player's HAVE. Created on first use; cascadeStartNodeReset()
// destroys it before a re-map (write-once discipline: a re-map gets a FRESH object, never re-parses into a stale one).
CvTechInfo& cascadeStartNode();
void cascadeStartNodeReset();

#endif // CV_JSON_TECH_INFO_H
