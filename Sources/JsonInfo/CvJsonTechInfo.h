#pragma once
#ifndef CV_JSON_TECH_INFO_H
#define CV_JSON_TECH_INFO_H

//
//	CvJsonTechInfo -- the per-type cascade info for TECHS. TWO parts:
//	(1) the empire-ability blocks this tech PROVIDES when held (json.md §8; capabilities.md): the flat `capabilities`
//	    (the composed CvJsonBoolBlock unit) + the siblings `canTrade` (trade-table items/agreements), `canTradeOn`
//	    (tradable TERRAIN_ FK refs), `canWorkOn`.
//	(2) the tech's OWN typed values (research cost, era, the empire modifiers it deposits, flags, AI, art/sound/quote).
//	A tech's UNLOCK surface (units/buildings/…/other techs) is store-inverted onto those entities' `enables.*` / this
//	tech's `requires`/`obsoletes`/`grants` -- base availability, NOT poco getters. HUMAN-native (assetValue/powerValue
//	re-apply the latent ×100, like specialist health). No cascade here.
//

#include "CvJsonInfo.h"
#include <set>
#include <map>

class CvJsonTechInfo : public CvJsonInfo
{
public:
	CvJsonTechInfo();

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
	const CvWString& getQuote() const { return m_szQuoteKey; }                       // identity.quote (CvInfoBase has no quote key)

	// ⏳ DATA-GAP (DEC-data-first): DomainExtraMoves is authored by 12 techs but curate_tech DROPS it (its channel lacks
	//    valueKeys/targetType), so it emits nothing -- getter reads 0 until the curator emits domainMoves.player.flat keyed by domain.
	int getDomainExtraMoves(int /*iDomain*/) const { return 0; }
	// ⏳ ALWAYS-0 leftovers -- live callers exist but ZERO techs author these + no channel maps them (return 0; owner call
	//    whether to keep the getter or cut the caller at cutover).
	int getMaintenanceModifier() const { return 0; }
	int getDistanceMaintenanceModifier() const { return 0; }
	int getNumCitiesMaintenanceModifier() const { return 0; }
	int getCoastalDistanceMaintenanceModifier() const { return 0; }
	int getCommerceModifier(int /*i*/) const { return 0; }

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
	int m_iHealth, m_iHappiness, m_iGlobalTradeModifier, m_iGlobalForeignTradeModifier, m_iTradeMissionModifier;
	int m_iCorporationRevenueModifier, m_iCorporationMaintenanceModifier, m_iAssetValue, m_iPowerValue, m_iGridX, m_iGridY;
	int m_iAIWeight, m_iAITradeModifier;
	bool m_bRepeat, m_bTrade, m_bDisable, m_bGoodyTech;
	std::map<int, int> m_flavours;        // FlavorTypes -> weight
	std::map<int, int> m_freeSpecialists; // SpecialistTypes -> count (inert today)
	std::string m_szSoundMP;
	CvWString m_szQuoteKey;
};

// The synthetic TECH_GAME_START root (no engine id -- lives OFF the InfoRepo; readjson.md §5.1): the universal
// start node whose enables/capabilities seed every player's HAVE. Created on first use; cascadeStartNodeReset()
// destroys it before a re-map (write-once discipline: a re-map gets a FRESH object, never re-parses into a stale one).
CvJsonTechInfo& cascadeStartNode();
void cascadeStartNodeReset();

#endif // CV_JSON_TECH_INFO_H
