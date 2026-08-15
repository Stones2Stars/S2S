#pragma once
#ifndef CV_JSON_BUILDING_INFO_H
#define CV_JSON_BUILDING_INFO_H

//
//	CvBuildingInfo -- the BUILDING poco rebuilt to the full exemplar surface (patterns.md § THE GETTER SETUP:
//	the four read categories, nothing else). Styled for the JSON anatomy (json.md §2), members grouped by the
//	entity anatomy; every magnitude read is a load-compiled fetch ([DEC-materialize-at-mapfrom]); kind and scope
//	are separate parameters everywhere ([DEC-scope-is-an-axis]); every magnitude getter IS ×100
//	([DEC-fixedpoint-x100]); no legacy getter name returns ([DEC-new-getter-surface]).
//

#include "CvInfo.h"
#include "CvClassificationBlock.h"   // the §8 held-boolean block + CLS_HAS
#include <map>

class CvArtInfoBuilding;

class CvBuildingInfo : public CvInfo
{
public:
	CvBuildingInfo();

	virtual void mapFrom(const picojson::value& entity);

	// ======================= 1. SECTIONS -- whole typed objects =======================
	virtual const CvRequires*  getRequires()     const { return &m_requires; }
	virtual const CvEdges*     getEdges()        const { return &m_edges; }
	virtual const CvAllowed*   getAllowed()      const { return &m_allowed; }
	virtual const CvTriggers*  getTriggers()     const { return &m_triggers; }
	virtual const CvProvides*  getProvides()     const { return &m_provides; }
	virtual const CvModifiers* getModifiers()    const { return &m_modifiers; }
	virtual const CvModifiers* getWhenObsolete() const { return &m_whenObsolete; }
	// obsoletedBy.techs -- authored TARGET-side (enabler.md §2: nothing authors tech.obsoletes.buildings), so
	// the obsoleting tech IS this edge rather than a member. Reads [0]: an AUTHORED edge list keeps its data
	// order, which `CvEdges::sortUnique` deliberately leaves alone for exactly these first-element getters.
	// Same shape as the sibling `CvSpecialBuildingInfo::getObsoleteTech`, which names this read as its twin.
	TechTypes getObsoleteTech() const
	{
		const std::vector<int>* pTechs = edge(EDGEF_OBSOLETED_BY, EDGEB_TECHS);
		return (TechTypes)((pTechs != NULL && !pTechs->empty()) ? (*pTechs)[0] : NO_TECH);
	}
	// The one-shot POPULATION and FREE-TECH payloads this building hands over on its CONSIDERED ACTION --
	// `grants.population` at city vs empire scope, and `grants.freeTechs` ([json.md §5] numeric pulses).
	// Materialized at mapFrom as HUMAN counts ([DEC-materialize-at-mapfrom]): the pulses are ×100 at parse and
	// the key handles are interned once, so neither a scale nor a string lookup ever reaches a read.
	int getPopulationChange() const { return m_iPopulationChange; }
	int getGlobalPopulationChange() const { return m_iGlobalPopulationChange; }
	int getFreeTechs() const { return m_iFreeTechs; }
	virtual const CvClassificationBlock* getAttributes()   const { return &m_attributes; }
	virtual const CvClassificationBlock* getAmenities()    const { return &m_amenities; }
	virtual const CvClassificationBlock* getCapabilities() const { return &m_capabilities; }
	// The entity-level applicability gate ([DEC-entity-gate], json.md §2). A building composing NONE meant the
	// base returned NULL and cascadeGateOk answered "never barred" (enabler.md §8), so the 23 corporate-HQ
	// buildings that author `disabled` were silently ungated -- the authored clause reported unconsumed on every
	// load and refused nothing.
	virtual const CvGate* getGate() const { return &m_gate; }

	// ======================= 2. CLASSIFICATION -- O(1) bitset tests, hold-vs-provide in the NAME (json §8) ====
	// What the BUILDING ITSELF is/does (teamShare, destroyedOnCapture, orbital) -- never what it confers.
	bool hasAttributes() const                { return !m_attributes.isEmpty(); }
	// What it CONFERS on its city -- the grantor side of the city's amenity fold.
	bool providesAmenities() const             { return !m_amenities.isEmpty(); }
	// The named reads, each over the block that actually OWNS it. Named for the AUTHORED KEY's meaning, never the
	// legacy getter replaced: `borderObstacle` carries no "area" (a landmass is not a scope), and
	// `destroyedOnCapture` says what happens rather than what does not (the legacy spelling was the negative,
	// isNeverCapture).
	// ⚠ TRANSITIONAL: these per-key reads are the same getter-per-channel shape patterns.md calls the disease.
	// They collapse onto the parameterized block read AFTER GREEN (owner); do not grow the set as a habit.

	// What the building CONFERS ON ITS CITY (json §8 `amenities`) -- city-HELD, grantor-PROVIDED. ⛔ These are
	// the GRANTOR side only: a consumer asks the CITY (whose fold refcounts every grantor), never each building.
	// The legacy iWorkableRadius carried no information -- every authoring was the same 3, and the radius itself
	// is culture-driven state -- so what a building says is "this CITY reaches its third ring early".
	// The WHOLESALE DISABLES (owner): a hard off-switch, not a modifier -- while such a grantor is present the
	// city's whole anger / unhealthy-population / non-building-health side ceases to exist (modifier.md §2b).
	// ⚑ `abolishedAnger` names the MECHANIC, never the WHERE: the legacy pair spelled it `bNoUnhappiness` on a
	// building and `bNoCapitalUnhappiness` on a civic -- two names for ONE gate, the second baking its condition
	// into the key ([DEC-conditions-are-predicates]). Both carriers now confer the SAME amenity.
	// No shipped BUILDING authors it -- deliberately, the mechanic being wildly overpowered -- so this read is
	// false today; the chain is wired and lights up the moment data authors one.
	// What the building PROVIDES to the empire (grantor-provided capabilities: setCultureRate, ...).
	bool providesCapabilities() const                { return !m_capabilities.isEmpty(); }

	// ======================= 3. MODIFIER GROUPS -- point reads over the compiled sums ========================
	// (The conditioned-list access + the expected* what-if valuations are the base surface: CvInfo's
	// modifierConditioned()/modifierConditionedRange() + expectedFlatYields()/... -- one declaration for every
	// rebuilt info, delegating to the ONE calc unit.)
	// The engine-enum channel groups (ruling 1: the engine enum IS the kind axis; the flat-vs-modifier split
	// lives in the NAME, never a scale suffix -- [DEC-fixedpoint-x100]).
	int getFlatYield(YieldTypes eYield, CvCascScope eScope) const
	{ return flatWithFans(infoYieldFamily(eYield), CHANNEL_AMOUNT, eScope); }
	int getYieldModifier(YieldTypes eYield, CvCascScope eScope) const
	{ return m_modifiers.sum(infoYieldFamily(eYield), CHANNEL_AMOUNT, eScope, CASC_UNIT_PERCENT); }
	int getFlatCommerce(CommerceTypes eCommerce, CvCascScope eScope) const
	{ return flatWithFans(infoCommerceFamily(eCommerce), CHANNEL_AMOUNT, eScope); }
	int getCommerceModifier(CommerceTypes eCommerce, CvCascScope eScope) const
	{ return m_modifiers.sum(infoCommerceFamily(eCommerce), CHANNEL_AMOUNT, eScope, CASC_UNIT_PERCENT); }
	// The PER-POPULATION plane of the same two channel groups -- the §3.7 count-scaled deposit the yield/commerce
	// rate adds as `perPopulation × population` (modifier.md §2a's EXTRA tier). It is a THIRD unit on the family,
	// not a third family: the split lives in the NAME exactly as flat-vs-modifier does, never a scale suffix.
	// ⚠ perPopulation is FLAT-SIDE, so it is ×100 like a flat and the reader reduces at its point of use
	// ([DEC-fixedpoint-x100]) -- NOT the legacy "latent /100" that made the old member look human.
	int getYieldPerPopulation(YieldTypes eYield, CvCascScope eScope) const
	{ return m_modifiers.sum(infoYieldFamily(eYield), CHANNEL_AMOUNT, eScope, CASC_UNIT_PER_POPULATION); }
	int getCommercePerPopulation(CommerceTypes eCommerce, CvCascScope eScope) const
	{ return m_modifiers.sum(infoCommerceFamily(eCommerce), CHANNEL_AMOUNT, eScope, CASC_UNIT_PER_POPULATION); }
	// The grouped families this type's census participation covers -- one getter per group, parameterized over
	// the group's kind enum; the canonical authored unit resolves in the vocabulary (infoKindUnit), never here.
	int getDefense(DefenseKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_DEFENSE, eKind, eScope, infoDefenseUnit(eKind)); }
	// Maintenance is authored on BOTH units at city scope, so it takes the flat-vs-modifier PAIR the yields and
	// commerce take -- the split lives in the NAME, never a scale suffix. The flat plane is the dominant one: a
	// building whose upkeep is a gold AMOUNT authors `maintenance.city.flat` (the negative-gold fold,
	// economy.md), which is what the vocabulary's own scope-split verdict reports (infoKindUnit answers FLAT for
	// MAINTENANCE_AMOUNT at city). A percent-only surface leaves that plane unreadable.
	int getFlatMaintenance(MaintenanceKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_MAINTENANCE, eKind, eScope, CASC_UNIT_FLAT); }
	int getMaintenanceModifier(MaintenanceKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_MAINTENANCE, eKind, eScope, CASC_UNIT_PERCENT); }
	int getTradeRoute(TradeRouteKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_TRADE_ROUTES, eKind, eScope, infoKindUnit(MODFAM_TRADE_ROUTES, eKind)); }
	// The ruling-27 per-channel route-yield modifier (the §2a tradeYield input fold's per-source term): the
	// channel kinds sit contiguous in YieldTypes order, so the parameterized read is FOOD + eYield.
	int getTradeRouteYieldModifier(YieldTypes eYield, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_TRADE_ROUTES, TRADE_ROUTE_MODIFIER_FOOD + (int)eYield, eScope, CASC_UNIT_PERCENT); }
	int getExperience(ExperienceKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_EXPERIENCE, eKind, eScope, infoKindUnit(MODFAM_EXPERIENCE, eKind)); }
	int getRevolution(RevolutionKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_REVOLUTION, eKind, eScope, infoKindUnit(MODFAM_REVOLUTION, eKind)); }
	int getUnderworld(UnderworldKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_UNDERWORLD, eKind, eScope, CASC_UNIT_FLAT); }
	int getHeal(HealKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_HEAL, eKind, eScope, CASC_UNIT_FLAT); }
	int getCombatModifier(CombatKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_COMBAT, eKind, eScope, CASC_UNIT_PERCENT); }
	int getBuildRateModifier(BuildRateKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_BUILD_RATE, eKind, eScope, CASC_UNIT_PERCENT); }
	int getCostsModifier(CostsKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_COSTS, eKind, eScope, CASC_UNIT_PERCENT); }
	// (The remaining census families -- anarchy/foodKept/hurryAnger/occupationTime/populationGrowthRate/
	// espionageDefense/greatGeneralRate/greatPeopleRate/goldenAge/warWeariness/workRate -- are the 1-2-kind
	// stragglers: the base getScalar(SCALAR_*) covers them. Keyed/targeted groups -- allowedSpecialists /
	// freeSpecialists / religion.{RELIGION_*} / the keyed buildRate targets -- are entry-list reads by design.)

	// ======================= 4. INTRINSIC -- bare typed reads (the census identity set) ======================
	// Counts/configs/FKs/flags are plain typed values (the identity.cityLimit convention: a config is human, a
	// MAGNITUDE is ×100 -- the two ×100 members here are the commerce magnitudes).
	// --- the `cost` section (json.md §7): WHAT IT COSTS TO MAKE. ⛔ One of the THREE cost planes and not to be
	// merged with the others: this is the entity's own authored cost; what CHANGES a cost is the `costs` MODIFIER
	// family (getCostsModifier above); the derived PRICE (upgrade gold, hurry gold/pop) is engine-computed from
	// the two and is nobody's getter.
	// ⚑ HUMAN, never ×100 -- a build cost is a plain hammer count, the identity.cityLimit convention (a config is
	// human, a MAGNITUDE is ×100). The gamespeed/era/handicap scaling is the CONSUMING system's
	// (CvPlayer::getProductionNeeded), which is why nothing is pre-scaled here.
	// ⛔ It carries NO -1 SENTINEL. The legacy `iCost == -1` meaning "not player-constructible" is translated by
	// the curator into the explicit identity.notConstructible flag, and buildability gates on isNotConstructible()
	// -- never on a raw cost value. An absent cost block is simply 0.
	int getCost() const                     { return m_iCost; }                     // cost.production
	int getCostSizeModifier() const         { return m_iCostSizeModifier; }         // cost.sizeModifier
	int getCostMaterialsModifier() const    { return m_iCostMaterialsModifier; }    // cost.materialsModifier
	int getCostComplexityModifier() const   { return m_iCostComplexityModifier; }   // cost.complexityModifier
	int getCostCountModifier() const        { return m_iCostCountModifier; }        // cost.countModifier
	int getCostHurryModifier() const        { return m_iCostHurryModifier; }        // cost.hurryModifier ("hurrying ME")

	int getWorth() const                    { return m_iWorth; }                    // AI trade/conquest valuation config
	// ai.flavours -- FLAVOR_* id -> weight (sparse; absent = 0). json §7 `ai` METADATA: it never affects rules,
	// only how the AI weights this building. Same shape + shared reader as CvLeaderHeadInfo's.
	int getFlavorValue(FlavorTypes eFlavor) const;
	// freeSpecialists.<scope>.any -- the GENERIC free-specialist slots this building grants ([modifier.md §6]:
	// the count-by-type leaf, where the key IS the type or `any`). The AMOUNT is the cascade's half of the
	// two-part seam; PLACEMENT stays the engine's. The data authors `any` at CITY and at EMPIRE, so scope is
	// the caller's argument ([DEC-scope-is-an-axis]) -- the empire leaf is what the legacy global/area pair
	// authored, a landmass being no scope of its own. ×100 like every other getter; the reader reduces.
	int getFreeSpecialistsAny(CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_FREE_SPECIALISTS, CHANNEL_AMOUNT, eScope, CASC_UNIT_COUNT); }
	int getMilitaryWorth() const            { return m_iMilitaryWorth; }
	int getConquestProbability() const      { return m_iConquestProbability; }      // survive-conquest percent config
	int getVisibilityPriority() const       { return m_iVisibilityPriority; }
	int getAirlift() const                  { return m_iAirlift; }
	int getAirUnitCapacity() const          { return m_iAirUnitCapacity; }
	int getWorkableRadius() const           { return m_iWorkableRadius; }
	int getMaxPlayerInstancesExtra() const  { return m_iMaxPlayerInstancesExtra; }
	bool isCenterInCity() const             { return m_bCenterInCity; }
	bool isNotConstructible() const         { return m_bNotConstructible; }         // json §7: excluded from the queue
	bool isAutoBuild() const                { return m_bAutoBuild; }                // json §7: autoBuild ⊂ notConstructible
	/// <summary>The info's OWN offerability verdict: may this building ever appear on a canConstruct offer
	/// (owner ruling)? The enabler's static exclusion folds THIS getter -- never per-flag logic assembled at the
	/// consumer. Asker-dependent bars (the strongly-restricted NPC lockdown) are gate concerns and can never
	/// live here: an info does not know who is asking.</summary>
	bool isOfferable() const                { return !m_bNotConstructible; }
	// identity.empireLevel -- the building is held by the PLAYER, once, empire-wide, and is never present in any
	// city (DEC-empire-level-buildings, enabler-spec §2). The ONE placement choke point routes an empire-level id
	// to the owner; an atom naming one implies EMPIRE scope (json §3.4 -- the tag IS the type's domain).
	bool isEmpireLevel() const              { return m_bEmpireLevel; }
	bool isNoInstanceLimit() const          { return m_bNoInstanceLimit; }
	bool isAllowsNukes() const              { return m_bAllowsNukes; }
	bool isForceNoPrereqScaling() const     { return m_bForceNoPrereqScaling; }
	int getGreatPeopleUnitType() const      { return m_iGreatPeopleUnitType; }      // UNIT_* FK
	int getAdvisor() const                  { return m_iAdvisor; }                  // ADVISOR_* FK
	// world.art.define -- the ART_DEF_* tag this resolves through ArtFileMgr. Art is an UNMIGRATED system
	// BOUNDARY (json.md par.7): the JSON carries only the tag id and the definitions stay in the ART XML,
	// so this is a shim onto ARTFILEMGR rather than data the info holds.
	const CvArtInfoBuilding* getArtInfo() const;
	// world.art.notShownInCity -- TRUE when the building has NO model to place in the city, which is ~90% of them:
	// they are game entities whose art define is scaled to nothing (while still carrying a real <Button> for the
	// pedia and the build list). The curator DERIVES the verdict from the art define once, offline, so the engine
	// reads a member here instead of interrogating the art plane per layout rebuild.
	bool isNotShownInCity() const { return m_bNotShownInCity; }
	// A building's BUTTON lives on its art define, not on the building -- the JSON carries the ART_DEF_* tag and
	// the <Button> path stays in the ART XML with the rest of the definition (the same unmigrated boundary as
	// getArtInfo above). Without this override the inherited CvInfoBase::getButton reads m_szButton, which no
	// building ever authors, so every building answered "" and the interface asked the resource manager to load
	// an empty texture path.
	virtual const char* getButton() const;
	// ui.art.movie.defineTag -- the ART_DEF_MOVIE_* tag of the one-off movie a wonder plays on completion, the
	// same unmigrated ART boundary as getArtInfo: the JSON carries the tag id and the definition stays in the
	// ART XML. 805 buildings author one, so the datum is live authored data rather than a legacy leftover.
	// ⚠ hasMovie asks only WHETHER one is authored, which is the whole of what the completion popup gates on --
	// resolving the tag is ARTFILEMGR's job at the moment the movie screen opens.
	bool hasMovie() const                   { return !m_szMovieDefineTag.empty(); }
	const char* getMovieDefineTag() const   { return m_szMovieDefineTag.c_str(); }
	int getSpecialBuildingType() const      { return m_iSpecialBuildingType; }      // SPECIALBUILDING_* FK (group cap, json §4.4)
	int getFreeStartEra() const             { return m_iFreeStartEra; }             // ERA FK
	int getDiploVoteType() const            { return m_iDiploVoteType; }            // DIPLOVOTE_* FK
	int getReligion() const                 { return m_iReligion; }                 // RELIGION_* FK (the religious-building association)
	int getShrineReligion() const           { return m_iShrineReligion; }           // json §9 `shrine`: the religion this is the SHRINE of
	int getHeadquartersCorporation() const  { return m_iHeadquartersCorporation; }  // json §9 `headquarters`: the corp-HQ FK
	// The audio played when this building COMPLETES. Audio is an unmigrated system the JSON only REFERENCES
	// (json §7), so the info carries the TAG and the audio manager resolves it -- never a migrated asset.
	const char* getConstructSound() const   { return m_szConstructSound.c_str(); } // json §7 `sound.construct`

	// The KEEP-legacy property engine's per-turn SOURCES, bridged from this building's PROPERTY_* families
	// (property-audit.md). Buildings gather NO_RELATION -- a building's deposit lands in its OWN city -- while
	// the `empire`-scope authorings ride the all-cities container the city gather walks for the owning player.
	const CvPropertyManipulators* getPropertyManipulators() const { return &m_PropertyManipulators; }
	const CvPropertyManipulators* getPropertyManipulatorsAllCities() const { return &m_PropertyManipulatorsAllCities; }
	const std::vector<MapCategoryTypes>& getMapCategories() const { return m_aeMapCategories; }   // MAPCATEGORY_* FKs
	const std::vector<int>& getEnabledCivilizations() const { return m_aiEnabledCivilizations; }  // CIVILIZATION_* FKs
	const std::map<int, int>& getVictoryThresholds() const  { return m_victoryThresholds; }       // VICTORY_* FK -> threshold
	// The commerce-magnitude identity config (×100 -- a magnitude, not a count).
	int getStateReligionCommerce(CommerceTypes eCommerce) const { return m_aiStateReligionCommerce[(int)eCommerce]; }

protected:
	virtual CvRequires*  mutRequires()     { return &m_requires; }
	virtual CvEdges*     mutEdges()        { return &m_edges; }
	virtual CvAllowed*   mutAllowed()      { return &m_allowed; }
	virtual CvTriggers*  mutTriggers()     { return &m_triggers; }
	virtual CvProvides*  mutProvides()     { return &m_provides; }
	virtual CvModifiers* mutModifiers()    { return &m_modifiers; }
	virtual CvModifiers* mutWhenObsolete() { return &m_whenObsolete; }
	virtual CvClassificationBlock* mutAttributes()   { return &m_attributes; }
	virtual CvClassificationBlock* mutAmenities()    { return &m_amenities; }
	virtual CvClassificationBlock* mutCapabilities() { return &m_capabilities; }
	virtual CvGate* mutGate()                        { return &m_gate; }

private:
	// --- the composed section units (availability · provisions · effects · classification) ---
	CvRequires  m_requires;
	CvEdges     m_edges;
	CvAllowed   m_allowed;
	CvTriggers  m_triggers;
	CvProvides  m_provides;
	CvModifiers m_modifiers;
	CvModifiers m_whenObsolete;
	CvClassificationBlock m_attributes;
	CvClassificationBlock m_amenities;
	CvClassificationBlock m_capabilities;
	CvGate m_gate;

	// --- the intrinsic identity members (materialized once at mapFrom; getters are bare reads) ---
	int m_iCost;
	int m_iCostSizeModifier;
	int m_iCostMaterialsModifier;
	int m_iCostComplexityModifier;
	int m_iCostCountModifier;
	int m_iCostHurryModifier;
	int m_iWorth;
	int m_iMilitaryWorth;
	int m_iConquestProbability;
	int m_iVisibilityPriority;
	int m_iAirlift;
	int m_iAirUnitCapacity;
	int m_iWorkableRadius;
	int m_iMaxPlayerInstancesExtra;
	bool m_bCenterInCity;
	bool m_bNotConstructible;
	bool m_bAutoBuild;
	bool m_bEmpireLevel;
	bool m_bNoInstanceLimit;
	bool m_bAllowsNukes;
	bool m_bForceNoPrereqScaling;
	int m_iGreatPeopleUnitType;
	int m_iAdvisor;
	std::string m_szArtDefineTag;
	bool m_bNotShownInCity;   // world.art.notShownInCity (curator-derived from the art define)
	std::string m_szMovieDefineTag;
	int m_iSpecialBuildingType;
	int m_iFreeStartEra;
	int m_iDiploVoteType;
	int m_iReligion;
	int m_iShrineReligion;
	int m_iHeadquartersCorporation;
	std::string m_szConstructSound;
	std::vector<MapCategoryTypes> m_aeMapCategories;
	std::vector<int> m_aiEnabledCivilizations;
	std::map<int, int> m_victoryThresholds;
	std::map<int, int> m_flavours;                  // ai.flavours: FLAVOR_* id -> weight
	int m_iPopulationChange;
	int m_iGlobalPopulationChange;
	int m_iFreeTechs;
	int m_aiStateReligionCommerce[NUM_COMMERCE_TYPES];
	// Fed from the PROPERTY_* families (CascadePropertyBridge::bridgeFamilies -- property-audit.md).
	CvPropertyManipulators m_PropertyManipulators;            // PROPERTY_X.city.flat -- this building's own city
	CvPropertyManipulators m_PropertyManipulatorsAllCities;   // PROPERTY_X.empire.flat -- every city of the owner
};

#endif // CV_JSON_BUILDING_INFO_H
