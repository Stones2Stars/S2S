#pragma once
#ifndef CV_JSON_UNIT_INFO_H
#define CV_JSON_UNIT_INFO_H

//
//	CvUnitInfo -- the UNIT poco rebuilt to the full exemplar surface (patterns.md par. THE GETTER SETUP: the
//	four read categories, nothing else). The unit plane is deliberately its own RESOLVED-VALUES shape
//	(state-repositories.md par. UNIT): the type's par.6 self-accumulator deposits compile into the composed
//	CvModifiers and are read as point sums here; the unit INSTANCE later sums type + unitcombats + promotions
//	into its resolved stats on the promotion/combat-class-change trigger. Styled for the JSON anatomy (json.md
//	par.2): identity/cost are bare typed intrinsics; skills/tags are the par.8 held-classification bitsets
//	(hold-vs-provide: a unit HAS its skills/tags); builds / spread / groupSpawn / vision / sizeMatters /
//	outcomes are par.8-par.9 bespoke typed sections; every magnitude read is a load-compiled fetch
//	([DEC-materialize-at-mapfrom]); kind and scope are separate parameters ([DEC-scope-is-an-axis]); every
//	magnitude getter IS x100 ([DEC-fixedpoint-x100]); the type-keyed vs-entries (terrain/feature/unitCombat/
//	domain/flanking/vsUnit targets) stay compiled ENTRY-LIST reads by design; no legacy getter name returns
//	([DEC-new-getter-surface]).
//

#include "CvInfo.h"
#include "CvJsonParse.h"            // vectorHas / mapValueOrDefault -- the shared runtime point reads the getters delegate to
#include "CvHideAndSeekSection.h"   // par.9 `hideAndSeek` typed section (the shared concealment/detection contest)
#include "CvSizeMattersSection.h"   // par.9 `sizeMatters` typed section
#include "CvOutcomesSection.h"      // par.8 `outcomes` typed section (shared unit-plane CvOutcome intake)
#include "Defines/CvEnums.h"        // EraTypes/UnitArtStyleTypes/MissionTypes/DomainTypes/UnitAITypes/...
#include "Defines/CvStructs.h"      // GroupSpawnUnitCombat
#include <map>
#include <set>
#include <string>
#include <vector>

class CvArtInfoUnit;

//	The ERA BANDS a unit's mesh-group art is authored over (world.art.meshGroups.groups[].define, json.md par.7).
//	A band is not an era: it is the era from which that art applies, and the resolution walks DOWN from the
//	highest band the observer has reached to the first one the unit actually authors.
enum UnitArtEraBand
{
	UNIT_ART_ERA_EARLY = 0,
	UNIT_ART_ERA_CLASSICAL,
	UNIT_ART_ERA_MIDDLE,
	UNIT_ART_ERA_RENAISSANCE,
	UNIT_ART_ERA_INDUSTRIAL,
	UNIT_ART_ERA_LATE,
	UNIT_ART_ERA_FUTURE,

	NUM_UNIT_ART_ERA_BANDS
};

//	One <UnitMeshGroup>: how many members the formation requires of it, and the art it wears per era band.
//	An unauthored band is an empty tag -- the resolution falls through it to the next band down.
struct CvUnitMeshGroup
{
	CvUnitMeshGroup() : iRequired(0) {}

	int iRequired;
	std::string aszEraDefine[NUM_UNIT_ART_ERA_BANDS];
};

class CvUnitInfo : public CvInfo
{
public:
	CvUnitInfo();
	virtual void mapFrom(const picojson::value& entity);

	// ======================= 1. SECTIONS -- whole typed objects =======================
	virtual const CvRequires* getRequires() const { return &m_requires; }
	virtual const CvEdges* getEdges() const { return &m_edges; }
	virtual const CvAllowed* getAllowed() const { return &m_allowed; }
	virtual const CvTriggers* getTriggers() const { return &m_triggers; }
	virtual const CvModifiers* getModifiers() const { return &m_modifiers; }
	virtual const CvClassificationBlock* getSkills() const { return &m_skills; }
	virtual const CvClassificationBlock* getTags() const { return &m_tags; }
	virtual const CvGate* getGate() const { return &m_gate; }
	const CvHideAndSeekSection& getHideAndSeek() const { return m_hideAndSeek; }
	const CvSizeMattersSection& getSizeMatters() const { return m_sizeMatters; }

	// ======================= 2. CLASSIFICATION -- O(1) bitset tests, hold-vs-provide in the NAME (json par.8) ==
	// What the unit HAS: its mutable abilities (skills) and its immutable type membership (tags). The ids are
	// the ClassificationRegistry's runtime-minted SKILL_*/TAG_* infos.
	bool hasSkills() const { return !m_skills.isEmpty(); }
	bool hasTags() const { return !m_tags.isEmpty(); }

	// ======================= 3. MODIFIER GROUPS -- point reads over the compiled sums =======================
	// (Conditioned-list access + the expected* what-if valuations are the base CvInfo surface. The census
	// stragglers -- strength [ruling 5: the BASE value only, strength.unit.flat] / withdrawal / firstStrike
	// strikes + chances / range -- read through the base getScalar. The batch-pending unkinded member
	// culture.unit.garrison mints NO getter until its vocabulary/curator call lands (reported). The mixed-unit groups keep the flat-vs-modifier split in the NAME, the
	// getFlatYield/getYieldModifier convention.)
	int getFlatCombat(CombatKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_COMBAT, eKind, eScope, CASC_UNIT_FLAT); }
	int getCombatModifier(CombatKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_COMBAT, eKind, eScope, CASC_UNIT_PERCENT); }
	int getMovement(MovementKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_MOVEMENT, eKind, eScope, infoKindUnit(MODFAM_MOVEMENT, eKind, eScope)); }
	int getFlatCollateral(CollateralKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_COLLATERAL, eKind, eScope, CASC_UNIT_FLAT); }
	int getCollateralModifier(CollateralKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_COLLATERAL, eKind, eScope, CASC_UNIT_PERCENT); }
	int getFlatBombard(BombardKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_BOMBARD, eKind, eScope, CASC_UNIT_FLAT); }
	int getBombardModifier(BombardKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_BOMBARD, eKind, eScope, CASC_UNIT_PERCENT); }
	int getAir(AirKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_AIR, eKind, eScope, infoKindUnit(MODFAM_AIR, eKind, eScope)); }
	// cargo.space entries may carry the par.3.7 `unit:` predicate qualifier (a carrier restricted to a cargo
	// class); the qualifier rides the compiled entries (CvModEntry::unitQual), evaluated at the CONSUMER
	// against each cargo candidate -- the point sum here is the unqualified capacity plane.
	int getCargo(CargoKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_CARGO, eKind, eScope, CASC_UNIT_FLAT); }
	int getCapture(CaptureKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_CAPTURE, eKind, eScope, CASC_UNIT_FLAT); }
	int getFlatHeal(HealKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_HEAL, eKind, eScope, CASC_UNIT_FLAT); }
	int getHealModifier(HealKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_HEAL, eKind, eScope, CASC_UNIT_PERCENT); }
	int getEspionage(EspionageKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_ESPIONAGE, eKind, eScope, CASC_UNIT_FLAT); }
	// `underworld` is authored at UNIT scope as well as city ([json.md] §6: the in-city criminal contest --
	// the city is the arena, the unit carries the stat). Both kinds are flats.
	int getUnderworld(UnderworldKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_UNDERWORLD, eKind, eScope, CASC_UNIT_FLAT); }
	// buildRate.self entries are bonus-conditioned (the legacy BonusProductionModifiers) -- the conditioned
	// list carries them; this point read serves any unconditioned residue.
	int getBuildRateModifier(BuildRateKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_BUILD_RATE, eKind, eScope, CASC_UNIT_PERCENT); }

	// ======================= 4. INTRINSIC -- bare typed reads (the census identity set) =======================
	// --- the root combat-class FKs (json par.8: combatClass/combatClasses are ROOT keys, not identity) ---
	int getCombatClass() const { return m_iCombatClass; }                          // root `combatClass` (primary UNITCOMBAT_* FK)
	const std::vector<int>& getCombatClasses() const { return m_aiCombatClasses; } // root `combatClasses` (sub classes)
	bool hasCombatClass(int iUnitCombat) const
	{ return m_iCombatClass == iUnitCombat || vectorHas(m_aiCombatClasses, iUnitCombat); }

	// --- identity scalars/flags ---
	bool isSpawnOnly() const { return m_bSpawnOnly; }             // identity.spawnOnly (never-buildable flag, json par.4.3)
	/// <summary>The info's OWN offerability verdict: may this unit ever appear on a canTrain offer (owner
	/// ruling)? The enabler's static exclusion folds THIS getter -- never per-flag logic assembled at the
	/// consumer. Asker-dependent bars (the strongly-restricted NPC lockdown) are gate concerns and can never
	/// live here: an info does not know who is asking.</summary>
	bool isOfferable() const { return !m_bSpawnOnly; }
	int getWorth() const { return m_iWorth; }                     // identity.worth (AI asset valuation config)
	int getMilitaryWorth() const { return m_iMilitaryWorth; }     // identity.militaryWorth (power valuation config)
	int getXpValueAttack() const { return m_iXpValueAttack; }     // identity.xpValueAttack
	int getXpValueDefense() const { return m_iXpValueDefense; }   // identity.xpValueDefense
	int getConscription() const { return m_iConscription; }       // identity.conscription
	int getAggression() const { return m_iAggression; }           // identity.aggression (legacy load default 5)
	int getAnimalCombat() const { return m_iAnimalCombat; }       // identity.animalCombat
	int getCommandRange() const { return m_iCommandRange; }       // identity.commandRange (commander plane)
	int getControlPoints() const { return m_iControlPoints; }     // identity.controlPoints (commander plane)
	int getLeaderExperience() const { return m_iLeaderExperience; }   // identity.leaderExperience
	int getMinAreaSize() const { return m_iMinAreaSize; }         // identity.minAreaSize
	int getEspionagePoints() const { return m_iEspionagePoints; } // identity.espionagePoints
	DomainTypes getDomain() const { return (DomainTypes)m_iDomain; }              // identity.domain
	UnitAITypes getDefaultUnitAI() const { return (UnitAITypes)m_iDefaultUnitAI; }   // identity.defaultUnitAI
	int getSpecialUnitType() const { return m_iSpecialUnitType; } // identity.special (SPECIALUNIT_* FK)
	int getAdvisor() const { return m_iAdvisor; }                 // identity.advisor (ADVISOR_* FK)
	int getLeaderPromotion() const { return m_iLeaderPromotion; } // identity.leaderPromotion (PROMOTION_* FK)
	int getReligion() const { return m_iReligion; }               // identity.religion (RELIGION_* FK)
	int getCaptures() const { return m_iCaptures; }               // identity.captures (UNIT_* FK -- what capturing yields)
	const char* getFormationType() const { return m_szFormationType.c_str(); }    // identity.formationType

	// --- identity.base (the create-unit foundation; strength/moves are FAMILIES, ruling 5 / json par.8) ---
	int getWorkRate() const { return m_iWorkRate; }               // identity.base.workRate
	int getAirCombat() const { return m_iAirCombat; }             // identity.base.airCombat
	int getCombatLimit() const { return m_iCombatLimit; }         // identity.base.combatLimit (legacy default 100)
	int getAirCombatLimit() const { return m_iAirCombatLimit; }   // identity.base.airCombatLimit
	int getAirUnitCap() const { return m_iAirUnitCap; }           // identity.base.airUnitCap

	// --- cost (json par.7) ---
	int getProductionCost() const { return m_iProductionCost; }   // cost.production
	int getUpkeepCost() const { return m_iUpkeepCost; }           // cost.upkeep
	int getHurryCostModifier() const { return m_iHurryCostModifier; }   // cost.hurryCostModifier (own-cost data, ruling 18)
	int getAdvancedStartCost() const { return m_iAdvancedStartCost; }   // identity.advancedStart.cost (legacy default 100)

	// --- identity.cargo (the special-unit cargo RESTRICTIONS -- identity, not the cargo family) ---
	int getSpecialCargo() const { return m_iSpecialCargo; }             // identity.cargo.special (SPECIALUNIT_* FK)
	int getSMNotSpecialCargo() const { return m_iSMNotSpecialCargo; }   // identity.cargo.smNotSpecial (SPECIALUNIT_* FK)

	// --- identity lists (whole typed containers) ---
	const std::vector<int>& getUnitAIs() const { return m_aiUnitAIs; }         // identity.unitAIs
	bool hasUnitAI(int iUnitAI) const { return vectorHas(m_aiUnitAIs, iUnitAI); }
	const std::vector<int>& getNotUnitAIs() const { return m_aiNotUnitAIs; }   // identity.notUnitAIs
	bool hasNotUnitAI(int iUnitAI) const { return vectorHas(m_aiNotUnitAIs, iUnitAI); }
	const std::vector<MapCategoryTypes>& getMapCategories() const { return m_aeMapCategories; }   // identity.mapCategories
	const std::vector<int>& getTerrainImpassable() const { return m_aiTerrainImpassable; }   // identity.terrainImpassable
	bool isTerrainImpassable(int iTerrain) const { return vectorHas(m_aiTerrainImpassable, iTerrain); }
	const std::vector<int>& getFeatureImpassable() const { return m_aiFeatureImpassable; }   // identity.featureImpassable
	bool isFeatureImpassable(int iFeature) const { return vectorHas(m_aiFeatureImpassable, iFeature); }
	const std::vector<int>& getDefendAgainstUnits() const { return m_aiDefendAgainstUnits; } // identity.defendAgainstUnit
	bool isDefendAgainstUnit(int iUnit) const { return vectorHas(m_aiDefendAgainstUnits, iUnit); }
	const std::vector<int>& getHeritage() const { return m_aiHeritage; }       // identity.heritage
	const std::vector<std::string>& getUniqueNames() const { return m_aszUniqueNames; }   // identity.uniqueNames (raw name/TXT strings)
	// identity.{feature|terrain}PassableTechs -- substrate id -> the tech that opens passage (-1 = none).
	const std::map<int, int>& getFeaturePassableTechs() const { return m_featurePassableTechs; }
	const std::map<int, int>& getTerrainPassableTechs() const { return m_terrainPassableTechs; }
	// The POINT reads over those two maps -- the keyed-group `value(id)` shape ([contexts.md] COUNTS, not
	// objects), the same form getReligionSpreadStrength takes. A mover asks about the ONE substrate it is
	// standing on; it never walks the terrain/feature database asking which entries this unit has a tech for.
	// ABSENT answers NO_TECH (-1), which is what the passability gate tests for -- never 0, a real tech id.
	int getFeaturePassableTech(int iFeature) const { return mapValueOrDefault(m_featurePassableTechs, iFeature, -1); }
	int getTerrainPassableTech(int iTerrain) const { return mapValueOrDefault(m_terrainPassableTechs, iTerrain, -1); }

	// --- the par.8 combat targeting/immunity keyed sets (json par.8: value-carrying keyed targeting is the
	// combat family's, not a skill). The family-scoped target tokens compile the maps as targeted COUNT
	// entries; these typed keyed members are MATERIALIZED at mapFrom from the compiled entry list. ---
	const std::set<int>& getTargetUnitCombats() const { return m_targetUnitCombats; }      // combat.unit.targets.{UC}
	bool hasTargetUnitCombat(int iUnitCombat) const { return m_targetUnitCombats.count(iUnitCombat) != 0; }
	const std::set<int>& getDefenderUnitCombats() const { return m_defenderUnitCombats; }  // combat.unit.defenders.{UC}
	bool hasDefenderUnitCombat(int iUnitCombat) const { return m_defenderUnitCombats.count(iUnitCombat) != 0; }
	const std::vector<int>& getTargetUnits() const { return m_aiTargetUnits; }             // combat.unit.unitTargets
	bool hasTargetUnit(int iUnit) const { return vectorHas(m_aiTargetUnits, iUnit); }

	// --- par.8 `builds` -- the unit type's worker-build repertoire (BUILD_* FKs) ---
	const std::vector<int>& getBuilds() const { return m_aiBuilds; }
	bool hasBuild(int iBuild) const { return vectorHas(m_aiBuilds, iBuild); }

	// --- par.9 bespoke sections ---
	const std::map<int, int>& getReligionSpread() const { return m_religionSpread; }         // spread.religion.{RELIGION}: strength
	const std::map<int, int>& getCorporationSpread() const { return m_corporationSpread; }   // spread.corporation.{CORP}: strength
	// The POINT reads over those two maps -- the keyed-group `count(id)` shape ([contexts.md] COUNTS, not
	// objects). A consumer asking about ONE religion/corporation indexes the group here; it never sweeps the
	// religion/corporation database asking each id whether this unit spreads it.
	int getReligionSpreadStrength(int iReligion) const
	{ const std::map<int, int>::const_iterator it = m_religionSpread.find(iReligion); return it != m_religionSpread.end() ? it->second : 0; }
	int getCorporationSpreadStrength(int iCorporation) const
	{ const std::map<int, int>::const_iterator it = m_corporationSpread.find(iCorporation); return it != m_corporationSpread.end() ? it->second : 0; }
	const std::vector<GroupSpawnUnitCombat>& getGroupSpawn() const { return m_groupSpawn; }  // groupSpawn rows {unitCombat, chance, title}
	// A unit's DIRECT upgrades are its par.4.3 `requires.build.dormant.all` ids -- the upgrades MINUS any that
	// also supersede it, which is precisely the set the dormancy gate recurses (enabler.md par.3). CvRequires
	// already materializes them, so this is a bare read of that one store, never a second copy of the list.
	// The superseders are the sibling `replacedBy.units` below; a consumer wanting both unions the two.
	const std::vector<int>& getUpgradesTo() const { return dormantTriggers(); }
	bool isUpgradeTo(int iUnit) const { return vectorHas(dormantTriggers(), iUnit); }
	const std::vector<int>& getReplacedByUnits() const { return m_aiReplacedByUnits; }   // replacedBy.units (the par.4.2 superseders)
	// the upgrade CHAIN -- every unit transitively reachable over the direct upgrades above (the whole upgrade
	// TREE: chains, obsolete intermediates, cycles); derived by deriveAtRegistryComplete, not JSON-mapped.
	const std::vector<int>& getUpgradeChain() const { return m_aiUpgradeChain; }

	// --- grants (par.5 payload) -- the lists materialized at mapFrom (bucket-string reads are load-time only) ---
	const std::vector<int>& getGrantedPromotions() const { return m_aiGrantedPromotions; }   // grants.promotions
	bool grantsPromotion(int iPromotion) const { return vectorHas(m_aiGrantedPromotions, iPromotion); }
	const std::vector<int>& getGrantedGreatPeople() const { return m_aiGrantedGreatPeople; } // grants.greatPeople
	bool grantsGreatPerson(int iSpecialist) const { return vectorHas(m_aiGrantedGreatPeople, iSpecialist); }
	const std::vector<int>& getGrantedBuildings() const { return m_aiGrantedBuildings; }     // grants.buildings
	bool grantsBuilding(int iBuilding) const { return vectorHas(m_aiGrantedBuildings, iBuilding); }
	// grants.greatPersonAction.<act>.{base,multiplier} -- the GP mission-payload magnitudes (the par.8
	// `missions` carve-out's data, parked under grants today).
	int getDiscoverBase() const { return m_iDiscoverBase; }
	int getDiscoverMultiplier() const { return m_iDiscoverMultiplier; }
	int getHurryBase() const { return m_iHurryBase; }
	int getHurryMultiplier() const { return m_iHurryMultiplier; }
	int getTradeBase() const { return m_iTradeBase; }
	int getTradeMultiplier() const { return m_iTradeMultiplier; }
	int getGreatWorkBase() const { return m_iGreatWorkBase; }
	int getFoodBase() const { return m_iFoodBase; }

	// --- ai metadata ---
	int getAIWeight() const { return m_iAIWeight; }               // ai.behaviour.weight
	int getFlavour(int iFlavour) const { return mapValueOrDefault(m_flavours, iFlavour); }   // ai.flavours

	// --- ui/world art (the ART_DEF_* tag resolved through ArtFileMgr -- the CvBonusInfo shim leaf) ---
	// The MESH GROUPS the EXE lays the unit out and animates it through: the formation's own numbers, and the
	// per-group art keyed by era band. Answering these with absent values does not degrade to "no art" -- a max
	// animation speed of 0 plays the walk cycle without translating, and 0 group definitions leaves the models
	// with no per-member offsets (json.md par.7).
	const CvArtInfoUnit* getArtInfo(int iIndex, EraTypes eEra, UnitArtStyleTypes eStyle) const;
	const char* getButton() const;   // art-define button (units carry no ui.art.icon)
	int getGroupSize() const { return m_iMeshGroupSize; }
	int getGroupDefinitions() const { return (int)m_meshGroups.size(); }
	int getUnitGroupRequired(int iGroup) const;
	int getMeleeWaveSize() const { return m_iMeleeWaveSize; }
	int getRangedWaveSize() const { return m_iRangedWaveSize; }
	// Art, in the EXE's own animation units -- no fixed-point scale, and never a cascade input.
	float getAnimationMaxSpeed() const { return m_fAnimationMaxSpeed; }
	float getAnimationPadTime() const { return m_fAnimationPadTime; }

	// --- runtime members (documented write-once seams) ---
	// Non-XML runtime map-hash, drawn from the synced RNG in the ctor (archive mirror; seeds CvUnit's
	// movement-characteristics hash).
	int getZobristValue() const { return m_iZobristValue; }
	// RUNTIME command-type: SetGlobalActionInfo assigns COMMAND_UPGRADE at load and reads it back -- must be
	// stored (a -1 stub crashes SetGlobalActionInfo).
	int getCommandType() const { return m_iCommandType; }
	void setCommandType(int iNewType) { m_iCommandType = iNewType; }

	// --- load-derived reads (json par.9 sizeMatters: the ranks are DERIVED at load, never stored on the data) ---
	// The members are materialized by deriveAtRegistryComplete (the reversePassRun post-map derivation step,
	// runnable only once the FULL registry is mapped); the getters are bare member reads.
	// Sigma over the unit's combat classes (primary + subs) of each class's sizeMatters *Base where > -10 (the
	// "unset" sentinel). The group rank feeds getUnitCountSM (count / 3^(rank-1)) -- a stubbed 0 integer-divides
	// by zero there.
	int getBaseQualityRank() const { return m_iBaseQualityRank; }
	int getBaseGroupRank() const { return m_iBaseGroupRank; }
	int getBaseSizeRank() const { return m_iBaseSizeRank; }
	int getBaseCargoVolume() const { return m_iBaseCargoVolume; }   // the derived SM cargo volume (>= 1)
	// 100 x (strength base [air: identity.base.airCombat] + the combat-class flat-combat sum), SM-scaled by
	// the derived modifier base when bSizeMatters.
	int getTotalModifiedCombatStrength(bool bSizeMatters) const;
	// Derived: the unit's era = its first prereq tech's era (NO_ERA when tech-free).
	int getEra() const { return m_iEra; }
	// Derived: a unit can gain XP iff SOME promotion applies to its primary combat class.
	bool canAcquireExperience() const { return m_bCanAcquireExperience; }

	// The post-map LOAD-WINDOW derivation seam (write-once-at-load, the mapFrom sibling): reversePassRun calls
	// this for every unit AFTER the full registry is mapped. Recompute-assigns EVERY derived member each run
	// (clear-first, idempotent). The two inputs the unit cannot resolve itself are computed by the pass: the
	// first prereq TECH atom's era (atom-kind routing lives in the load pipeline) and the union of combat
	// classes some promotion applies to (one cross-registry precompute, shared by all units).
	void deriveAtRegistryComplete(int iFirstPrereqTechEra, const std::set<int>& combatClassesWithPromotions);

	// ======================= the CvOutcome kill/action-mission system (par.8 outcomes) =======================
	// Fed from the unit's `outcomes` JSON (the composed section parses it). Forwards keep the outward surface
	// unchanged (CvOutcomeListMerged consumers).
	const CvOutcomeList* getKillOutcomeList() const { return m_outcomes.getKillOutcomeList(); }
	int getNumActionOutcomes() const { return m_outcomes.getNumActionOutcomes(); }
	const CvOutcomeList* getActionOutcomeList(int iIndex) const { return m_outcomes.getActionOutcomeList(iIndex); }
	MissionTypes getActionOutcomeMission(int iIndex) const { return m_outcomes.getActionOutcomeMission(iIndex); }
	const CvOutcomeList* getActionOutcomeListByMission(MissionTypes eMission) const { return m_outcomes.getActionOutcomeListByMission(eMission); }
	const CvOutcomeMission* getOutcomeMission(int iIndex) const { return m_outcomes.getOutcomeMission(iIndex); }
	const CvOutcomeMission* getOutcomeMissionByMission(MissionTypes eMission) const { return m_outcomes.getOutcomeMissionByMission(eMission); }

	// --- the property engine (fed from the unit's PROPERTY_* families in mapFrom) ---
	const CvPropertyManipulators* getPropertyManipulators() const { return &m_PropertyManipulators; }

protected:
	virtual CvRequires* mutRequires() { return &m_requires; }
	virtual CvEdges* mutEdges() { return &m_edges; }
	virtual CvAllowed* mutAllowed() { return &m_allowed; }
	virtual CvTriggers* mutTriggers() { return &m_triggers; }
	virtual CvModifiers* mutModifiers() { return &m_modifiers; }
	virtual CvClassificationBlock* mutSkills() { return &m_skills; }
	virtual CvClassificationBlock* mutTags() { return &m_tags; }
	virtual CvGate* mutGate() { return &m_gate; }

private:
	// --- the composed section units ---
	CvRequires m_requires;
	CvEdges m_edges;
	CvAllowed m_allowed;
	CvTriggers m_triggers;
	CvModifiers m_modifiers;
	CvClassificationBlock m_skills;
	CvClassificationBlock m_tags;
	CvGate m_gate;
	CvHideAndSeekSection m_hideAndSeek;
	CvSizeMattersSection m_sizeMatters;
	CvOutcomesSection m_outcomes;

	// --- the intrinsic identity members (materialized once at mapFrom) ---
	int m_iCombatClass;
	std::vector<int> m_aiCombatClasses;
	bool m_bSpawnOnly;
	int m_iWorth;
	int m_iMilitaryWorth;
	int m_iXpValueAttack;
	int m_iXpValueDefense;
	int m_iConscription;
	int m_iAggression;
	int m_iAnimalCombat;
	int m_iCommandRange;
	int m_iControlPoints;
	int m_iLeaderExperience;
	int m_iMinAreaSize;
	int m_iEspionagePoints;
	int m_iDomain;
	int m_iDefaultUnitAI;
	int m_iSpecialUnitType;
	int m_iAdvisor;
	int m_iLeaderPromotion;
	int m_iReligion;
	int m_iCaptures;
	std::string m_szFormationType;
	int m_iWorkRate;
	int m_iAirCombat;
	int m_iCombatLimit;
	int m_iAirCombatLimit;
	int m_iAirUnitCap;
	int m_iProductionCost;
	int m_iUpkeepCost;
	int m_iHurryCostModifier;
	int m_iAdvancedStartCost;
	int m_iSpecialCargo;
	int m_iSMNotSpecialCargo;
	std::vector<int> m_aiUnitAIs;
	std::vector<int> m_aiNotUnitAIs;
	std::vector<MapCategoryTypes> m_aeMapCategories;
	std::vector<int> m_aiTerrainImpassable;
	std::vector<int> m_aiFeatureImpassable;
	std::vector<int> m_aiDefendAgainstUnits;
	std::vector<int> m_aiHeritage;
	std::vector<std::string> m_aszUniqueNames;
	std::map<int, int> m_featurePassableTechs;
	std::map<int, int> m_terrainPassableTechs;

	// --- the par.8 keyed targeting/immunity containers (materialized from the compiled targeted entries) ---
	std::set<int> m_targetUnitCombats;
	std::set<int> m_defenderUnitCombats;
	std::vector<int> m_aiTargetUnits;

	// --- par.8/par.9 bespoke section data ---
	std::vector<int> m_aiBuilds;
	std::map<int, int> m_religionSpread;
	std::map<int, int> m_corporationSpread;
	std::vector<GroupSpawnUnitCombat> m_groupSpawn;
	std::vector<int> m_aiReplacedByUnits;
	std::vector<int> m_aiUpgradeChain;   // derived: the direct-upgrade transitive closure (deriveAtRegistryComplete)

	// --- grants materialization + GP-action magnitudes ---
	std::vector<int> m_aiGrantedPromotions;
	std::vector<int> m_aiGrantedGreatPeople;
	std::vector<int> m_aiGrantedBuildings;
	int m_iDiscoverBase;
	int m_iDiscoverMultiplier;
	int m_iHurryBase;
	int m_iHurryMultiplier;
	int m_iTradeBase;
	int m_iTradeMultiplier;
	int m_iGreatWorkBase;
	int m_iFoodBase;

	// --- ai metadata ---
	int m_iAIWeight;
	std::map<int, int> m_flavours;

	// --- art / runtime ---
	std::string m_szArtDefineTag;      // world.art.define
	std::vector<CvUnitMeshGroup> m_meshGroups;   // world.art.meshGroups.groups
	int m_iMeshGroupSize;              // world.art.meshGroups.groupSize
	int m_iMeleeWaveSize;              // world.art.meshGroups.meleeWaveSize
	int m_iRangedWaveSize;             // world.art.meshGroups.rangedWaveSize
	float m_fAnimationMaxSpeed;        // world.art.meshGroups.maxSpeed
	float m_fAnimationPadTime;         // world.art.meshGroups.padTime
	int m_iZobristValue;               // runtime map-hash (ctor draw; a re-map must not redraw the synced RNG)
	int m_iCommandType;                // runtime command-type (SetGlobalActionInfo)

	// --- load-derived members (written ONLY by deriveAtRegistryComplete inside the load window; an info holds
	// no cache or dirty flag -- patterns.md INFO DATA-OUT) ---
	int m_iBaseQualityRank;    // Sigma combat classes' sizeMatters qualityBase where > -10
	int m_iBaseGroupRank;      // Sigma combat classes' sizeMatters groupBase where > -10
	int m_iBaseSizeRank;       // Sigma combat classes' sizeMatters sizeBase where > -10
	int m_iSMChangeBase;    // Sigma combat classes' flat-combat sum (x100)
	int m_iSMModifierBase;     // Sigma (quality/size/group base - 5) where > -10 (human ranks)
	int m_iBaseCargoVolume;    // derived SM cargo volume (>= 1)
	int m_iEra;                // first prereq TECH atom's era (NO_ERA when tech-free)
	bool m_bCanAcquireExperience;   // SOME promotion applies to the primary combat class

	CvPropertyManipulators m_PropertyManipulators;   // fed from the PROPERTY_* families (CascadePropertyBridge)
};

#endif // CV_JSON_UNIT_INFO_H
