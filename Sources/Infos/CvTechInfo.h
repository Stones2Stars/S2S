#pragma once
#ifndef CV_JSON_TECH_INFO_H
#define CV_JSON_TECH_INFO_H

//
//	CvTechInfo -- the TECH poco rebuilt to the full exemplar surface (patterns.md § THE GETTER SETUP: the four
//	read categories, nothing else). Styled for the JSON anatomy (json.md §2); every magnitude read is a
//	load-compiled fetch ([DEC-materialize-at-mapfrom]); kind and scope are separate parameters
//	([DEC-scope-is-an-axis]); no legacy getter name returns ([DEC-new-getter-surface]).
//
//	A tech's UNLOCK surface (units/buildings/.../other techs) is store-inverted onto those entities'
//	`enables.*` -- base availability data, never poco getters. The tech-side FORWARD views that survive are
//	the enabler.md §2 reconstructions: the multi-parent prereqs walked out of the composed requires.build tree
//	(the child retains them -- curate_tech keeps AndPreReqs/OrPreReqs on the child), and the leadsTo reverse
//	index built at load by the general reverse pass from exactly those retained prereqs.
//

#include "CvInfo.h"
#include "CvJsonParse.h"       // mapValueOrDefault -- the ONE sparse id-map point read
#include "Defines/CvEnums.h"   // TechTypes / NO_GAMEOPTION consumers' enums
#include "Defines/CvStructs.h" // PrereqBuilding -- the requires.build BUILDING_ OR-group's (building, min) pair
#include <set>
#include <map>
#include <vector>

class CvTechInfo : public CvInfo
{
public:
	CvTechInfo();

	virtual void mapFrom(const picojson::value& entity);

	// ======================= 1. SECTIONS -- whole typed objects =======================
	virtual const CvRequires*  getRequires()  const { return &m_requires; }
	virtual const CvEdges*     getEdges()     const { return &m_edges; }
	virtual const CvAllowed*   getAllowed()   const { return &m_allowed; }
	virtual const CvModifiers* getModifiers() const { return &m_modifiers; }
	virtual const CvClassificationBlock* getCapabilities() const { return &m_capabilities; }

	// ======================= 2. CLASSIFICATION -- O(1) bitset tests, hold-vs-provide in the NAME (json §8) ====
	// A tech PROVIDES a capability to the empire while held (the grantor direction; capabilities.md -- the
	// empire's active set is the derived-on-query union CascadeCapabilities maintains).
	bool providesCapability(int iCapabilityId) const { return m_capabilities.hasId(iCapabilityId); }
	bool providesCapabilities() const                { return !m_capabilities.isEmpty(); }
	// The bespoke capability-plane SIBLING blocks (capabilities.md; json §2 auxiliary): typed sections the
	// trade-table / trade-route / canWork systems union over live sources. Open string registries by design
	// (canTrade/canWorkOn keys are data, not classification-registry ids); canTradeOn carries real TERRAIN_ FKs.
	const std::set<std::string>& getCanTrade() const  { return m_canTrade; }
	const std::set<int>& getCanTradeOnTerrains() const { return m_canTradeOnTerrains; }
	const std::set<std::string>& getCanWorkOn() const { return m_canWorkOn; }
	bool canTradeItem(const std::string& szItem) const   { return m_canTrade.count(szItem) != 0; }
	bool canTradeOnTerrain(int iTerrain) const           { return m_canTradeOnTerrains.count(iTerrain) != 0; }
	bool canWorkOnClass(const std::string& szClass) const { return m_canWorkOn.count(szClass) != 0; }

	// ======================= 3. MODIFIER GROUPS -- point reads over the compiled sums ========================
	// (Conditioned-list access + the expected* what-if valuations are the base CvInfo surface. Census
	// participation: health/happiness empire flats; tradeRoutes city routes-count + empire route-profit
	// modifier incl. the IS_FOREIGN conditioned extra; commerce.empire.corporation + maintenance.empire.
	// corporation, ruling 15; workRate / featureProduction / tradeMission / inflation are getScalar
	// stragglers; domainMoves.empire.domains.{DOMAIN} and freeSpecialists.team.{SPECIALIST} are keyed shapes --
	// entry-list reads by design.)
	int getFlatWellbeing(WellbeingChannel eChannel, CvCascScope eScope) const
	{
		if (eChannel == WELLBEING_ANGER || eChannel == WELLBEING_UNHEALTH)
		{
			return 0;
		}
		return m_modifiers.sum(infoWellbeingFamily(eChannel), CHANNEL_AMOUNT, eScope, CASC_UNIT_FLAT);
	}
	int getTradeRoute(TradeRouteKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_TRADE_ROUTES, eKind, eScope, infoKindUnit(MODFAM_TRADE_ROUTES, eKind)); }
	int getMaintenanceModifier(MaintenanceKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_MAINTENANCE, eKind, eScope, CASC_UNIT_PERCENT); }
	// Ruling 15: legacy corporationRevenue = the commerce channel family's `corporation` source-component kind.
	int getCorporationCommerceModifier(CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_COMMERCE, CHANNEL_CORPORATION, eScope, CASC_UNIT_PERCENT); }

	// ======================= 4. INTRINSIC -- bare typed reads (the census identity set) ======================
	int getResearchCost() const { return m_iResearchCost; }   // cost.research (plane-1 actual cost, ruling 18)
	int getEra() const          { return m_iEra; }            // identity.era (ERA_* FK)
	int getAdvisor() const      { return m_iAdvisor; }        // ui.art.advisor (ADVISOR_* FK)
	int getGridX() const        { return m_iGridX; }          // identity.gridX (tech-tree layout)
	int getGridY() const        { return m_iGridY; }
	// The two AI valuation magnitudes are ×100 (identity.worth/militaryWorth author fractional human values;
	// [DEC-fixedpoint-x100]: the name says the VALUE, the scale is always ×100).
	int getWorth() const         { return m_iWorth; }
	int getMilitaryWorth() const { return m_iMilitaryWorth; }
	bool isRepeat() const    { return m_bRepeat; }     // identity.repeat (repeatable research)
	bool isTradeable() const { return m_bTradeable; }  // identity.tradeable (may appear in tech trades)
	bool isDisable() const   { return m_bDisable; }    // identity.disable
	bool isGoodyTech() const { return m_bGoodyTech; }  // identity.goodyTech (tribal-hut eligible)
	int getAIWeight() const        { return m_iAIWeight; }        // ai.behaviour.weight
	int getAITradeModifier() const { return m_iAITradeModifier; } // ai.behaviour.tradeModifier
	int getFlavorValue(int iFlavor) const                         // ai.flavours {FLAVOR: weight}
	{
		return mapValueOrDefault(m_flavours, iFlavor);
	}
	const char* getSound() const   { return m_szSound.c_str(); }     // sound.sound (tech-completed jingle)
	const char* getSoundMP() const { return m_szSoundMP.c_str(); }   // sound.soundMP
	// identity.quote -- RESOLVES the TXT_KEY via gDLL->getText (the raw key showed "TXT_KEY_..._QUOTE" in the
	// tech splash). std::wstring return: Boost.Python 1.32 has a std::wstring converter, none for CvWString.
	std::wstring getQuote() const;

	// --- the FORWARD prereq views walked from the composed requires.build tree (enabler.md §2: the tech case
	// reconstructs from the child's RETAINED requires.build.all/.any) ---
	const std::vector<TechTypes>& getPrereqAndTechs() const { return m_aePrereqAndTechs; }
	const std::vector<TechTypes>& getPrereqOrTechs() const  { return m_aePrereqOrTechs; }
	const std::vector<PrereqBuilding>& getPrereqOrBuildings() const { return m_aPrereqOrBuildings; }
	// --- the leadsTo REVERSE index (which techs list THIS tech as a prereq) -- built at load by
	// the general reverse pass from the retained prereq views above; the writer is load-window-only ---
	const std::set<TechTypes>& getLeadsToTechs() const { return m_leadsTo; }
	void addLeadsToTech(TechTypes eTech) { m_leadsTo.insert(eTech); }
	void clearLeadsTo() { m_leadsTo.clear(); }   // clear-first: the reverse pass runs in BOTH load phases

	virtual const CvTriggers*  getTriggers()  const { return &m_triggers; }   // §5 -- triggers + the folded grants

protected:
	virtual CvRequires*  mutRequires()  { return &m_requires; }
	virtual CvEdges*     mutEdges()     { return &m_edges; }
	virtual CvAllowed*   mutAllowed()   { return &m_allowed; }
	virtual CvTriggers*  mutTriggers()  { return &m_triggers; }
	virtual CvModifiers* mutModifiers() { return &m_modifiers; }
	virtual CvClassificationBlock* mutCapabilities() { return &m_capabilities; }

private:
	// --- the composed section units ---
	CvRequires  m_requires;
	CvEdges     m_edges;
	CvAllowed   m_allowed;
	CvTriggers  m_triggers;
	CvModifiers m_modifiers;
	CvClassificationBlock m_capabilities;

	// --- the bespoke capability-plane sibling blocks (typed sections; capabilities.md) ---
	std::set<std::string> m_canTrade;        // canTrade:{item:true} -- trade-table items/agreements
	std::set<int> m_canTradeOnTerrains;      // canTradeOn:{terrains:[TERRAIN_..]} -- FK ids
	std::set<std::string> m_canWorkOn;       // canWorkOn:{class:true} -- workable plot classes

	// --- the intrinsic identity members (materialized once at mapFrom; getters are bare reads) ---
	int m_iResearchCost;
	int m_iEra;
	int m_iAdvisor;
	int m_iGridX;
	int m_iGridY;
	int m_iWorth;
	int m_iMilitaryWorth;
	bool m_bRepeat;
	bool m_bTradeable;
	bool m_bDisable;
	bool m_bGoodyTech;
	int m_iAIWeight;
	int m_iAITradeModifier;
	std::map<int, int> m_flavours;           // FlavorTypes -> weight (ai.flavours)
	std::string m_szSound;
	std::string m_szSoundMP;
	CvWString m_szQuoteKey;

	// --- the forward prereq views + the leadsTo reverse index (load-window writes only) ---
	std::vector<TechTypes> m_aePrereqAndTechs;         // team-scope TECH_ atoms in requires.build.all (incl. folded 1-member ORs)
	std::vector<TechTypes> m_aePrereqOrTechs;          // FIRST multi-member TECH any-group under requires.build.all
	std::vector<PrereqBuilding> m_aPrereqOrBuildings;  // the requires.build.all[].any[] BUILDING_ OR-group ((building, min) pairs)
	std::set<TechTypes> m_leadsTo;                     // filled by the reverse pass (rp_deriveTechLeadsTo)
};

// The synthetic TECH_GAME_START root (no engine id -- lives OFF the InfoRepo; readjson.md §5.1): the universal
// start node whose enables/capabilities seed every player's HAVE. Created on first use; cascadeStartNodeReset()
// destroys it before a re-map (write-once discipline: a re-map gets a FRESH object, never re-parses into a stale one).
CvTechInfo& cascadeStartNode();
void cascadeStartNodeReset();

#endif // CV_JSON_TECH_INFO_H
