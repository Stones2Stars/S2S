#pragma once
#ifndef CV_JSON_PROMOTION_INFO_H
#define CV_JSON_PROMOTION_INFO_H

//
//	CvPromotionInfo -- the PROMOTION poco rebuilt to the full exemplar surface (patterns.md par. THE GETTER
//	SETUP: the four read categories, nothing else). A promotion is the unit plane's runtime GRANTOR: it PROVIDES
//	skills (the par.8 grant/revoke planes) and deposits par.6 unit-scope self-accumulator values (modifier.md
//	par.6 -- the additive promotion stack) the unit folds in as it is gained. Styled for the JSON anatomy
//	(json.md par.2): every magnitude read is a load-compiled fetch (docs/architecture/patterns.md §Materialize at mapFrom); kind and
//	scope are separate parameters (docs/architecture/patterns.md §The coherent surface (scope is a separate axis)); every magnitude getter IS x100
//	(docs/specs/curators/fixed-point-and-scales.md §1 (the x100 fixed-point model)); the type-keyed vs-entries (terrain/feature/unitCombat/domain/flanking/build
//	targets) stay compiled ENTRY-LIST reads by design; no legacy getter name returns (docs/architecture/patterns.md §THE TWO READ ROLES (new getter surface, never widen legacy)).
//
//	The chain edges: enables.promotions rides the composed CvEdges (store-inverted onto the tech is
//	reconstructed at load via the setters below -- the loadJson tech-FK reverse-index pass); the chain's
//	AND-half is the composed requires.build tree (the enabler's PROMOTION_ atom reads ctx.unit).
//

#include "CvInfo.h"
#include "CvJsonParse.h"            // vectorHas -- the shared id-vector membership scan the has*/is* getters read
#include "CvHideAndSeekSection.h"   // par.9 `hideAndSeek` typed section (the shared concealment/detection contest)
#include "CvSizeMattersSection.h"   // par.9 `sizeMatters` typed section (the SM deltas live here)
#include "Defines/CvEnums.h"        // TechTypes / PromotionLineTypes / NO_* + NO_COMMAND
#include "Defines/CvStructs.h"      // UnitCombatModifier (the ai.unitCombatWeights rows)
#include <vector>

class CvPromotionInfo : public CvInfo
{
public:
	CvPromotionInfo();
	virtual void mapFrom(const picojson::value& entity);

	// ======================= 1. SECTIONS -- whole typed objects =======================
	virtual const CvModifiers* getModifiers() const { return &m_modifiers; }
	virtual const CvClassificationBlock* getSkills() const { return &m_skills; }
	virtual const CvGate* getGate() const { return &m_gate; }
	virtual const CvEdges* getEdges() const { return &m_edges; }         // enables.promotions chain edges
	virtual const CvRequires* getRequires() const { return &m_requires; }   // the chain's requires.build AND-half
	const CvHideAndSeekSection& getHideAndSeek() const { return m_hideAndSeek; }
	const CvSizeMattersSection& getSizeMatters() const { return m_sizeMatters; }

	// ======================= 2. CLASSIFICATION -- O(1) bitset tests, hold-vs-provide in the NAME (json par.8) ==
	// A promotion PROVIDES skills to the holding unit; the FALSE plane REVOKES (skills.md par.4 -- a promotion
	// authoring `stampede:false` removes the ability). The unit instance folds the active set.
	bool providesSkill(int iSkillId) const { return m_skills.hasId(iSkillId); }
	bool revokesSkill(int iSkillId) const { return m_skills.hasFalseId(iSkillId); }
	bool providesSkills() const { return !m_skills.isEmpty(); }
	// The combat-class MEMBERSHIP the promotion mutates (skills.unitCombats / skills.removesUnitCombats --
	// keyed extras, typed members per the CvClassificationBlock header).
	const std::vector<int>& providesUnitCombats() const { return m_aiProvidesUnitCombats; }
	const std::vector<int>& removesUnitCombats() const { return m_aiRemovesUnitCombats; }

	// ======================= 3. MODIFIER GROUPS -- point reads over the compiled sums =======================
	// (Conditioned-list access + the expected* what-if valuations are the base CvInfo surface. The census
	// stragglers -- withdrawal / firstStrike strikes / revoltProtection / pillage / workRate(+hills) -- read
	// through the base getScalar. survivor is TRIGGER-PLANE chance data (info-rebuild ruling 16): no getter
	// is minted. The mixed-unit groups keep the flat-vs-modifier split in the NAME.)
	int getFlatCombat(CombatKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_COMBAT, eKind, eScope, CASC_UNIT_FLAT); }
	int getCombatModifier(CombatKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_COMBAT, eKind, eScope, CASC_UNIT_PERCENT); }
	int getCapture(CaptureKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_CAPTURE, eKind, eScope, CASC_UNIT_FLAT); }
	int getFlatHeal(HealKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_HEAL, eKind, eScope, CASC_UNIT_FLAT); }
	int getHealModifier(HealKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_HEAL, eKind, eScope, CASC_UNIT_PERCENT); }
	int getAir(AirKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_AIR, eKind, eScope, infoKindUnit(MODFAM_AIR, eKind, eScope)); }
	int getMovement(MovementKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_MOVEMENT, eKind, eScope, CASC_UNIT_FLAT); }
	// How much SIGHT this promotion sharpens ([vision.md] §1: a unit's strength is its base stat plus its
	// promotions). Engine-native, so a reader wanting PLOTS divides by VISION_OPEN_GROUND_COST at its use.
	int getFlatVision(VisionKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_VISION, eKind, eScope, CASC_UNIT_FLAT); }
	int getFlatBombard(BombardKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_BOMBARD, eKind, eScope, CASC_UNIT_FLAT); }
	int getBombardModifier(BombardKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_BOMBARD, eKind, eScope, CASC_UNIT_PERCENT); }
	int getFlatCollateral(CollateralKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_COLLATERAL, eKind, eScope, CASC_UNIT_FLAT); }
	int getCollateralModifier(CollateralKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_COLLATERAL, eKind, eScope, CASC_UNIT_PERCENT); }
	int getCargo(CargoKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_CARGO, eKind, eScope, CASC_UNIT_FLAT); }
	int getEspionage(EspionageKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_ESPIONAGE, eKind, eScope, CASC_UNIT_FLAT); }
	// `underworld` is authored at UNIT scope as well as city ([json.md] §6: the in-city criminal contest --
	// the city is the arena, the unit carries the stat). Both kinds are flats.
	int getUnderworld(UnderworldKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_UNDERWORLD, eKind, eScope, CASC_UNIT_FLAT); }
	int getExperienceModifier(ExperienceKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_EXPERIENCE, eKind, eScope, CASC_UNIT_PERCENT); }
	int getUpkeepModifier(UpkeepKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_UPKEEP, eKind, eScope, CASC_UNIT_PERCENT); }
	int getFlatUpkeep(UpkeepKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_UPKEEP, eKind, eScope, CASC_UNIT_FLAT); }
	// costs.unit.upgrade.percent -- the ruling-18 upgrade-cost kind (the legacy upgrade discount).
	int getCostsModifier(CostsKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_COSTS, eKind, eScope, CASC_UNIT_PERCENT); }

	// ======================= 4. INTRINSIC -- bare typed reads (the census identity set) =======================
	bool isLeader() const { return m_bLeader; }                       // identity.leader (great-commander promotion)
	bool isStatus() const { return m_bStatus; }                       // identity.status (status pseudo-promotion)
	bool isQuick() const { return m_bQuick; }                         // identity.quick
	bool isStarsign() const { return m_bStarsign; }                   // identity.starsign
	bool isZeroesXP() const { return m_bZeroesXP; }                   // identity.zeroesXP
	bool isForOffset() const { return m_bForOffset; }                 // identity.forOffset
	bool isCargoPrereq() const { return m_bCargoPrereq; }             // identity.cargoPrereq
	bool isSetOnInvestigated() const { return m_bSetOnInvestigated; } // identity.setOnInvestigated
	bool isPrereqNormInvisible() const { return m_bPrereqNormInvisible; }   // identity.prereqNormInvisible
	bool isRemoveAfterSet() const { return m_bRemoveAfterSet; }       // identity.removeAfterSet
	int getStateReligionPrereq() const { return m_iStateReligionPrereq; }   // identity.stateReligionPrereq (RELIGION_* FK; parked availability gate)
	int getControlPoints() const { return m_iControlPoints; }         // identity.controlPoints (commander plane)
	int getCommandRange() const { return m_iCommandRange; }           // identity.commandRange (commander plane)
	int getLevelPrereq() const { return m_iLevelPrereq; }             // identity.levelPrereq (unit-level gate)
	int getMinEra() const { return m_iMinEra; }                       // identity.minEra (ERA FK band; NO_ERA = unbanded)
	int getReplacesUnitCombat() const { return m_iReplacesUnitCombat; }     // identity.replacesUnitCombat (UNITCOMBAT_* FK)
	int getDomainCargoChange() const { return m_iDomainCargoChange; }       // identity.domainCargoChange (DOMAIN_* FK)
	int getSpecialCargoChange() const { return m_iSpecialCargoChange; }     // identity.specialCargoChange (SPECIALUNIT_* FK)
	int getSMNotSpecialCargoChange() const { return m_iSMNotSpecialCargoChange; }   // identity.smNotSpecialCargoChange (SPECIALUNIT_* FK)
	// identity.unitCombats -- the combat classes this promotion APPLIES to (the availability membership; the
	// qualified-set caches below derive from it + the line's lists).
	const std::vector<int>& getUnitCombats() const { return m_aiUnitCombats; }
	bool appliesToUnitCombat(int iUnitCombat) const { return vectorHas(m_aiUnitCombats, iUnitCombat); }
	const std::vector<int>& getNotOnUnitCombats() const { return m_aiNotOnUnitCombats; }
	bool isNotOnUnitCombat(int iUnitCombat) const { return vectorHas(m_aiNotOnUnitCombats, iUnitCombat); }
	const std::vector<int>& getNotOnDomains() const { return m_aiNotOnDomains; }
	bool isNotOnDomain(int iDomain) const { return vectorHas(m_aiNotOnDomains, iDomain); }
	// par.8 keyed-skill FK lists (skills.<name>.{TYPE}:true).
	const std::vector<int>& getTerrainDoubleMoves() const { return m_aiTerrainDoubleMove; }
	const std::vector<int>& getFeatureDoubleMoves() const { return m_aiFeatureDoubleMove; }
	// par.9 promotionLine link: the top-level {LINE: rank} object.
	PromotionLineTypes getPromotionLine() const { return m_ePromotionLine; }
	int getLinePriority() const { return m_iLinePriority; }

	// --- THE LINE ACCRUAL: this promotion PLUS every lower-priority promotion in its own line, this one first. ---
	//
	// A promotion line is a LADDER, and holding a rung IMPLIES the rungs beneath it -- each level's
	// `requires.build` names the level below (ACCURACY3 -> ACCURACY2 -> ACCURACY), so a unit carrying the top
	// of a line carries the whole chain and its EFFECTIVE value is the chain's SUM.
	//
	// ⚑ THAT IS WHY THERE ARE TWO READS, and they answer different questions: the promotion's OWN getters say
	// what THIS rung contributes (what the pedia shows about the promotion itself), while the accrual says what
	// a UNIT HOLDING IT ACTUALLY HAS (what the unit's tooltip shows). Neither is the other's approximation.
	//
	// ⛔ A STATUS promotion accrues only ITSELF: status / affliction / equipment lines are parallel states, not
	// a ladder, so summing them would invent a compounding that does not exist. A promotion with no line
	// likewise accrues only itself, which is why every promotion has a NON-EMPTY accrual and no reader needs an
	// is-there-a-line branch.
	//
	// Derived by deriveAtRegistryComplete: it reads OTHER promotions, so it cannot be a mapFrom read (the twin
	// of CvUnitInfo::m_aiUpgradeChain). The getter is a bare member read; the SUM over it is
	// CvPromotionAccrual (docs/architecture/patterns.md §DRY (single implementation)), never open-coded at a consumer.
	const std::vector<int>& getLineAccrual() const { return m_aiLineAccrual; }
	// Fed the finished ordered list by the reverse pass, which groups the lines ONCE rather than having every
	// promotion re-scan the registry for its siblings.
	void deriveAtRegistryComplete(const std::vector<int>& aiLineAccrual);
	// ai.unitCombatWeights {UC:int} -- AI metadata rows.
	int getNumAIWeightsByUnitCombat() const { return (int)m_aAIWeights.size(); }
	const UnitCombatModifier& getAIWeightByUnitCombat(int iIndex) const { return m_aAIWeights[iIndex]; }
	// sound.sound -- the promotion-gained audio asset.
	const char* getSound() const { return m_szSound.c_str(); }
	// Derived at mapFrom over the promotion's own data (the hasCityOverLimitAnger materialization precedent).
	bool changesMoveThroughPlots() const { return m_bChangesMoveThroughPlots; }
	bool hasNegativeEffects() const { return m_bNegativeEffects; }
	// Non-XML runtime map-hash contribution (archived ctor mirror; CvUnit XORs it into its movement hash).
	int getZobristValue() const { return m_iZobristValue; }
	// RUNTIME command-type (SetGlobalActionInfo assigns it at load and reads it back -- must be stored).
	int getCommandType() const { return m_iCommandType; }
	void setCommandType(int iNewType) { m_iCommandType = iNewType; }
	// The property engine's uniform walk surface (fed from the PROPERTY_* families in mapFrom).
	const CvPropertyManipulators* getPropertyManipulators() const { return &m_PropertyManipulators; }

	// --- the store-inverted tech FKs (tech.enables.promotions / tech.obsoletes.promotions), reconstructed at
	// LOAD by the loadJson tech-FK reverse-index pass via the setters (the write-once-at-load window). ---
	TechTypes getTechPrereq() const { return m_eTechPrereq; }
	TechTypes getObsoleteTech() const { return m_eObsoleteTech; }
	void setTechPrereq(TechTypes eTech) { m_eTechPrereq = eTech; }
	void setObsoleteTech(TechTypes eTech) { m_eObsoleteTech = eTech; }

	// --- the pedia qualified/disqualified caches, computed POST-LOAD (the load window) from this promotion's
	// own unitCombats/notOnUnitCombats plus the promotion line's lists (CvPromotionLineInfo). ---
	const std::vector<int>& getQualifiedUnitCombats() const { return m_aiQualifiedUnitCombats; }
	bool isQualifiedUnitCombat(int iUnitCombat) const { return vectorHas(m_aiQualifiedUnitCombats, iUnitCombat); }
	const std::vector<int>& getDisqualifiedUnitCombats() const { return m_aiDisqualifiedUnitCombats; }
	void setQualifiedUnitCombatTypes();
	void setDisqualifiedUnitCombatTypes();

protected:
	virtual CvModifiers* mutModifiers() { return &m_modifiers; }
	virtual CvClassificationBlock* mutSkills() { return &m_skills; }
	virtual CvGate* mutGate() { return &m_gate; }
	virtual CvEdges* mutEdges() { return &m_edges; }
	virtual CvRequires* mutRequires() { return &m_requires; }

private:
	// --- the composed section units ---
	CvModifiers m_modifiers;
	CvClassificationBlock m_skills;
	CvGate m_gate;
	CvEdges m_edges;
	CvRequires m_requires;
	CvHideAndSeekSection m_hideAndSeek;
	CvSizeMattersSection m_sizeMatters;

	// --- the intrinsic identity members (materialized once at mapFrom) ---
	bool m_bLeader;
	bool m_bStatus;
	bool m_bQuick;
	bool m_bStarsign;
	bool m_bZeroesXP;
	bool m_bForOffset;
	bool m_bCargoPrereq;
	bool m_bSetOnInvestigated;
	bool m_bPrereqNormInvisible;
	bool m_bRemoveAfterSet;
	int m_iStateReligionPrereq;
	int m_iControlPoints;
	int m_iCommandRange;
	int m_iLevelPrereq;
	int m_iMinEra;
	int m_iReplacesUnitCombat;
	int m_iDomainCargoChange;
	int m_iSpecialCargoChange;
	int m_iSMNotSpecialCargoChange;
	std::vector<int> m_aiUnitCombats;
	std::vector<int> m_aiNotOnUnitCombats;
	std::vector<int> m_aiNotOnDomains;
	std::vector<int> m_aiProvidesUnitCombats;
	std::vector<int> m_aiRemovesUnitCombats;
	std::vector<int> m_aiTerrainDoubleMove;
	std::vector<int> m_aiFeatureDoubleMove;
	PromotionLineTypes m_ePromotionLine;
	int m_iLinePriority;
	// Load-derived (deriveAtRegistryComplete), never JSON-mapped: this promotion followed by every lower-priority
	// promotion in its line, descending. Always holds at least this promotion.
	std::vector<int> m_aiLineAccrual;
	std::vector<UnitCombatModifier> m_aAIWeights;
	std::string m_szSound;
	bool m_bChangesMoveThroughPlots;
	bool m_bNegativeEffects;
	int m_iZobristValue;
	int m_iCommandType;

	// --- load-window runtime members ---
	TechTypes m_eTechPrereq;
	TechTypes m_eObsoleteTech;
	std::vector<int> m_aiQualifiedUnitCombats;
	std::vector<int> m_aiDisqualifiedUnitCombats;

	CvPropertyManipulators m_PropertyManipulators;   // fed from the PROPERTY_* families (CascadePropertyBridge)
};

#endif // CV_JSON_PROMOTION_INFO_H
