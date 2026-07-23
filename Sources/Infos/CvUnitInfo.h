#pragma once
#ifndef CV_JSON_UNIT_INFO_H
#define CV_JSON_UNIT_INFO_H

//
//	CvUnitInfo -- the per-type cascade info for UNITS. Composes the section units a unit authors (requires /
//	edges / allowed / grants / modifier families / the section-8 `skills` + `tags` bool blocks / the entity-level
//	gate) AND holds the unit's own typed values (identity.base + identity scalars/lists + cost + the section-5
//	unit-scope combat families + vision) read from the curated JSON in mapFrom. The getters mirror the archived
//	CvUnitInfo consumer surface; curator field addresses (Tools/Migration/curate_unit.py) are noted per member.
//

#include "CvInfo.h"
#include "Defines/CvEnums.h"    // EraTypes/UnitArtStyleTypes/MissionTypes/UnitCombatTypes/UnitAITypes/DomainTypes/...
#include "Defines/CvStructs.h"  // GroupSpawnUnitCombat / HealUnitCombat / Invisible*Changes / EnabledCivilizations / *ModifierArray
#include "Engine/ConstructRequirement.h"  // getTrainRequirements ref type
#include "UI/CvOutcomeList.h"              // m_KillOutcomeList value member (CvOutcome fed from the unit's `outcomes` JSON, #430)
#include <vector>
#include <map>
#include <set>
#include <string>

class CvArtInfoUnit;
class CvOutcomeMission;   // the <Actions>/actions[] outcome-missions (heap-owned pointer members)

class CvUnitInfo : public CvInfo
{
public:
	CvUnitInfo();
	virtual ~CvUnitInfo();   // frees the heap-owned m_aOutcomeMissions
	bool spawnOnly, unlimitedException;
	std::vector<int> builds;        // top-level `builds`: the unit type's build REPERTOIRE (resolved BUILD_* ids)
	virtual void mapFrom(const picojson::value& entity);

	// ============================================================================================================
	//  Mirrored legacy CvUnitInfo surface (consumer-called). Hotkey/action getters INHERIT from CvHotkeyInfo;
	//  getButton/getExtraHoverText/getType inherit from CvInfoBase. The XML lifecycle (read/readPass3/copyNonDefaults/
	//  getCheckSum/getDataMembers/doPostLoadCaching) is replaced by mapFrom and deliberately omitted.
	// ============================================================================================================

	// --- REAL DATA: art + builds repertoire + the skills.unlimitedException flag ---
	const CvArtInfoUnit* getArtInfo(int i, EraTypes eEra, UnitArtStyleTypes eStyle) const;   // world.art.icon
	const char* getButton() const;   // art-define button (else CvInfoBase's empty m_szButton -> missing unit icon)
	bool isUnlimitedException() const { return unlimitedException; }              // skills.unlimitedException
	const std::vector<BuildTypes>& getBuilds() const { return reinterpret_cast<const std::vector<BuildTypes>&>(builds); }
	BuildTypes getBuild(int i) const { return (i >= 0 && i < (int)builds.size()) ? (BuildTypes)builds[i] : NO_BUILD; }
	short getNumBuilds() const { return (short)builds.size(); }
	bool hasBuild(BuildTypes e) const { for (size_t i = 0; i < builds.size(); ++i) if (builds[i] == (int)e) return true; return false; }

	// --- identity.base (create-unit foundation) ---
	int getCombat() const { return m_iCombat; }                                  // strength.unit.flat (0 when absent)
	int getMoves() const { return m_iMoves; }                                    // movement.unit.flat
	int getWorkRate() const { return m_iWorkRate; }                              // identity.base.workRate
	int getAirCombat() const { return m_iAirCombat; }                            // identity.base.airCombat
	int getCombatLimit() const { return m_iCombatLimit; }                        // identity.base.combatLimit
	int getAirCombatLimit() const { return m_iAirCombatLimit; }                  // identity.base.airCombatLimit
	int getAirUnitCap() const { return m_iAirUnitCap; }                          // identity.base.airUnitCap
	int getUnitCombatType() const { return m_iUnitCombatType; }                  // root `combatClass` (primary FK)

	// --- identity scalars ---
	int getAssetValue() const { return m_iAssetValue; }                          // identity.worth
	int getPowerValue() const { return m_iPowerValue; }                          // identity.militaryWorth
	int getXPValueAttack() const { return m_iXPValueAttack; }                    // identity.xpValueAttack
	int getXPValueDefense() const { return m_iXPValueDefense; }                  // identity.xpValueDefense
	int getConscriptionValue() const { return m_iConscriptionValue; }            // identity.conscription
	int getAggression() const { return m_iAggression; }                          // identity.aggression
	int getAnimalCombatModifier() const { return m_iAnimalCombatModifier; }      // identity.animalCombat
	int getCommandRange() const { return m_iCommandRange; }                      // identity.commandRange
	int getControlPoints() const { return m_iControlPoints; }                    // identity.controlPoints
	int getLeaderExperience() const { return m_iLeaderExperience; }              // identity.leaderExperience
	int getMinAreaSize() const { return m_iMinAreaSize; }                        // identity.minAreaSize
	int getEspionagePoints() const { return m_iEspionagePoints; }                // identity.espionagePoints
	DomainTypes getDomainType() const { return (DomainTypes)m_iDomainType; }     // identity.domain
	UnitAITypes getDefaultUnitAIType() const { return (UnitAITypes)m_iDefaultUnitAIType; }  // identity.defaultUnitAI
	int getSpecialUnitType() const { return m_iSpecialUnitType; }                // identity.special
	int getAdvisorType() const { return m_iAdvisorType; }                        // identity.advisor
	int getLeaderPromotion() const { return m_iLeaderPromotion; }                // identity.leaderPromotion
	int getReligionType() const { return m_iReligionType; }                      // identity.religion
	UnitTypes getUnitCaptureType() const { return (UnitTypes)m_iUnitCaptureType; }  // identity.captures
	const char* getFormationType() const { return m_szFormationType.c_str(); }   // identity.formationType

	// --- cost ---
	int getProductionCost() const { return m_iProductionCost; }                  // cost.production
	int getBaseUpkeep() const { return m_iBaseUpkeep; }                          // cost.upkeep
	int getHurryCostModifier() const { return m_iHurryCostModifier; }            // cost.hurryCostModifier
	int getInstanceCostModifier() const { return m_iInstanceCostModifier; }      // costs.empire.perInstance.percent
	int getAdvancedStartCost() const { return m_iAdvancedStartCost; }            // identity.advancedStart.cost

	// --- cargo ---
	int getCargoSpace() const { return m_iCargoSpace; }                          // cargo.unit.space.flat (iCargo)
	int getSpecialCargo() const { return m_iSpecialCargo; }                      // identity.cargo.special
	int getSMNotSpecialCargo() const { return m_iSMNotSpecialCargo; }            // identity.cargo.smNotSpecial

	// --- section 5 unit-scope combat-trait families (raw human values) ---
	int getCityAttackModifier() const { return m_iCityAttackModifier; }          // strength.unit.cityAttack.percent
	int getCityDefenseModifier() const { return m_iCityDefenseModifier; }        // strength.unit.cityDefense.percent
	int getHillsAttackModifier() const { return m_iHillsAttackModifier; }        // strength.unit.hillsAttack.percent
	int getHillsDefenseModifier() const { return m_iHillsDefenseModifier; }      // strength.unit.hillsDefense.percent
	int getVSBarbs() const { return m_iVSBarbs; }                                // strength.unit.vsBarbs.percent
	int getAttackCombatModifier() const { return m_iAttackCombatModifier; }      // strength.unit.attack.percent
	int getDefenseCombatModifier() const { return m_iDefenseCombatModifier; }    // strength.unit.defense.percent
	int getCombatModifierPerSizeMore() const;      // GAMEOPTION_COMBAT_SIZE_MATTERS-gated (archive mirror; .cpp)
	int getCombatModifierPerSizeLess() const;      // SIZE_MATTERS-gated
	int getCombatModifierPerVolumeMore() const;    // SIZE_MATTERS-gated
	int getCombatModifierPerVolumeLess() const;    // SIZE_MATTERS-gated
	int getLunge() const;                          // GAMEOPTION_COMBAT_SURROUND_DESTROY-gated (archive mirror; .cpp)
	int getEnclose() const;                        // S&D-gated
	int getUnnerve() const;                        // S&D-gated
	int getDynamicDefense() const;                 // S&D-gated
	int getStealthStrikes() const;                 // GAMEOPTION_COMBAT_WITHOUT_WARNING-gated (archive mirror; .cpp)
	int getStealthCombatModifier() const;          // WITHOUT_WARNING-gated
	int getBreakdownChance() const { return m_iBreakdownChance; }                // strength.unit.breakdownChance.flat
	int getBreakdownDamage() const { return m_iBreakdownDamage; }                // strength.unit.breakdownDamage.flat
	int getWithdrawalProbability() const { return m_iWithdrawalProbability; }     // withdrawal.unit.percent
	int getFirstStrikes() const { return m_iFirstStrikes; }                      // firstStrike.unit.strikes.flat
	int getChanceFirstStrikes() const { return m_iChanceFirstStrikes; }          // firstStrike.unit.chance.flat
	int getBombardRate() const { return m_iBombardRate; }                        // bombard.unit.rate.percent
	int getRBombardDamage() const { return m_iRBombardDamage; }                  // bombard.unit.rangedDamage.flat
	int getRBombardDamageLimit() const { return m_iRBombardDamageLimit; }        // bombard.unit.rangedDamageLimit.flat
	int getDCMBombRange() const { return m_iDCMBombRange; }                       // bombard.unit.dcmRange.flat
	int getDCMBombAccuracy() const { return m_iDCMBombAccuracy; }                 // bombard.unit.dcmAccuracy.flat
	int getBombRate() const { return m_iBombRate; }                              // bombard.unit.airBombRate.flat
	int getCollateralDamage() const { return m_iCollateralDamage; }              // collateral.unit.damage.percent
	int getCollateralDamageLimit() const { return m_iCollateralDamageLimit; }    // collateral.unit.limit.flat
	int getCollateralDamageMaxUnits() const { return m_iCollateralDamageMaxUnits; }  // collateral.unit.maxUnits.flat
	int getAirRange() const { return m_iAirRange; }                              // range.unit.flat
	int getInterceptionProbability() const { return m_iInterceptionProbability; }  // air.unit.intercept.percent
	int getEvasionProbability() const { return m_iEvasionProbability; }          // air.unit.evasion.percent
	int getNukeRange() const { return m_iNukeRange; }                            // air.unit.nukeRange.flat (-1 = not a nuke)
	int getCaptureProbabilityModifier() const { return m_iCaptureProbabilityModifier; }  // capture.unit.probability.flat
	int getCaptureResistanceModifier() const { return m_iCaptureResistanceModifier; }    // capture.unit.resistance.flat
	int getInsidiousness() const { return m_iInsidiousness; }                    // espionage.unit.insidiousness.flat
	int getInvestigation() const { return m_iInvestigation; }                    // espionage.unit.investigation.flat
	int getNumHealSupport() const { return m_iNumHealSupport; }                  // heal.unit.support.flat
	int getSelfHealModifier() const { return m_iSelfHealModifier; }              // heal.unit.selfModifier.percent
	int getDropRange() const { return m_iDropRange; }                            // movement.unit.dropRange.flat
	int getCultureGarrisonValue() const { return m_iCultureGarrisonValue; }      // culture.unit.garrison.flat

	// --- keyed combat families -> id->value maps (magnitude 0 for an unlisted key) ---
	int getTerrainAttackModifier(int i) const { return mapGet(m_terrainAttack, i); }        // strength.unit.terrain.{T}.attack.percent
	int getTerrainDefenseModifier(int i) const { return mapGet(m_terrainDefense, i); }      // strength.unit.terrain.{T}.defense.percent
	int getFeatureAttackModifier(int i) const { return mapGet(m_featureAttack, i); }        // strength.unit.feature.{F}.attack.percent
	int getFeatureDefenseModifier(int i) const { return mapGet(m_featureDefense, i); }      // strength.unit.feature.{F}.defense.percent
	int getUnitCombatModifier(int i) const { return mapGet(m_unitCombatModifier, i); }      // strength.unit.unitCombat.{UC}.percent
	int getDomainModifier(int i) const { return mapGet(m_domainModifier, i); }              // strength.unit.domain.{D}.percent
	int getBonusProductionModifier(int i) const { return mapGet(m_bonusProductionModifier, i); }  // buildRate.self.percent[enabled BONUS]
	int getNumFlankingStrikesbyUnitCombatTypes() const { return (int)m_flankingByUnitCombat.size(); }  // strength.unit.flanking.{UC}.percent
	int getFlankingStrengthbyUnitCombatType(int i) const { return mapGet(m_flankingByUnitCombat, i); }
	bool isFlankingStrikebyUnitCombatType(int i) const { return m_flankingByUnitCombat.count(i) != 0; }

	// --- section 8 keyed skill-extras (targeting / immunity) ---
	bool getTargetUnitCombat(int i) const { return m_targetUnitCombat.count(i) != 0; }      // strength.unit.targets.{UC}
	bool getDefenderUnitCombat(int i) const { return m_defenderUnitCombat.count(i) != 0; }  // strength.unit.defenders.{UC}
	// collateralImmune is now a BLANKET `skills` bit (curator reclassification), not a per-unitCombat keyed set: the
	// unit is collateral-immune against ALL unitCombats or none, so the arg is ignored (behaviour change vs legacy per-UC).
	int getUnitCombatCollateralImmune(int /*i*/) const { return skill("collateralImmune") ? 1 : 0; }  // skills[collateralImmune]
	int getTargetUnit(int i) const { return (i >= 0 && i < (int)m_targetUnits.size()) ? m_targetUnits[i] : -1; }  // strength.unit.unitTargets
	int getNumTargetUnits() const { return (int)m_targetUnits.size(); }
	bool isTargetUnit(int i) const { return contains(m_targetUnits, i); }

	// --- GP-action magnitudes (grants.greatPersonAction.<act>.base/multiplier) ---
	int getBaseDiscover() const { return m_iBaseDiscover; }
	int getDiscoverMultiplier() const { return m_iDiscoverMultiplier; }
	int getBaseHurry() const { return m_iBaseHurry; }
	int getHurryMultiplier() const { return m_iHurryMultiplier; }
	int getBaseTrade() const { return m_iBaseTrade; }
	int getTradeMultiplier() const { return m_iTradeMultiplier; }
	int getGreatWorkCulture() const { return m_iGreatWorkCulture; }
	int getBaseFoodChange() const { return m_iBaseFoodChange; }

	// --- ai ---
	int getAIWeight() const { return m_iAIWeight; }                              // ai.behaviour.weight
	int getFlavorValue(int i) const { return mapGet(m_flavours, i); }            // ai.flavours

	// --- skills (section-8 flat bools) + the goldenAge grant flag + the spy tag ---
	// O(1) generated-id bit tests (CLS_HAS -> CvJsonBoolBlock::hasKey; SKILL_* ids from the ClassificationRegistry).
	// These sit on the render/AI hot paths (the EXE polls unit.isInvisible ~98M calls/turn-window; the pathfinder
	// reads the movement gates per step) -- never a per-call string lookup.
	bool isAlwaysHostile() const CLS_HAS(m_skills, CLSD_SKILL, "alwaysHostile")
	bool isAssassin() const CLS_HAS(m_skills, CLSD_SKILL, "assassin")
	bool isHiddenNationality() const CLS_HAS(m_skills, CLSD_SKILL, "hiddenNationality")
	bool isNoCapture() const CLS_HAS(m_skills, CLSD_SKILL, "noCapture")
	bool isNoDefensiveBonus() const CLS_HAS(m_skills, CLSD_SKILL, "noDefensiveBonus")
	bool isPillage() const CLS_HAS(m_skills, CLSD_SKILL, "pillage")
	bool isStampede() const CLS_HAS(m_skills, CLSD_SKILL, "stampede")
	bool isBlendIntoCity() const CLS_HAS(m_skills, CLSD_SKILL, "blendIntoCity")
	bool isBarbCoExist() const CLS_HAS(m_skills, CLSD_SKILL, "barbCoExist")
	bool isCanMoveAllTerrain() const CLS_HAS(m_skills, CLSD_SKILL, "canMoveAllTerrain")
	bool isCanMoveImpassable() const CLS_HAS(m_skills, CLSD_SKILL, "canMoveImpassable")
	bool isCounterSpy() const CLS_HAS(m_skills, CLSD_SKILL, "counterSpy")
	bool isDestroy() const CLS_HAS(m_skills, CLSD_SKILL, "destroy")
	bool isFirstStrikeImmune() const CLS_HAS(m_skills, CLSD_SKILL, "firstStrikeImmune")
	bool isFlatMovementCost() const CLS_HAS(m_skills, CLSD_SKILL, "flatMovementCost")
	bool isFoodProduction() const CLS_HAS(m_skills, CLSD_SKILL, "food")
	bool isFound() const CLS_HAS(m_skills, CLSD_SKILL, "found")
	bool isGreatGeneral() const CLS_HAS(m_skills, CLSD_SKILL, "greatGeneral")
	bool isIgnoreBuildingDefense() const CLS_HAS(m_skills, CLSD_SKILL, "ignoreBuildingDefense")
	bool isIgnoreTerrainCost() const CLS_HAS(m_skills, CLSD_SKILL, "ignoreTerrainCost")
	bool isIgnoreZoneofControl() const CLS_HAS(m_skills, CLSD_SKILL, "ignoreZoneOfControl")
	bool isInquisitor() const CLS_HAS(m_skills, CLSD_SKILL, "inquisitor")
	bool isInvestigate() const CLS_HAS(m_skills, CLSD_SKILL, "investigate")
	bool isInvisible() const CLS_HAS(m_skills, CLSD_SKILL, "alwaysInvisible")
	bool isMechUnit() const CLS_HAS(m_skills, CLSD_SKILL, "mechanized")
	bool isNoBadGoodies() const CLS_HAS(m_skills, CLSD_SKILL, "noBadGoodies")
	bool isNoNonOwnedCityEntry() const CLS_HAS(m_skills, CLSD_SKILL, "noNonOwnedCityEntry")
	bool isNoNonTypeProdMods() const CLS_HAS(m_skills, CLSD_SKILL, "noNonTypeProdMods")
	bool isNukeImmune() const CLS_HAS(m_skills, CLSD_SKILL, "nukeImmune")
	bool isOnlyDefensive() const CLS_HAS(m_skills, CLSD_SKILL, "onlyDefensive")
	bool isPassage() const CLS_HAS(m_skills, CLSD_SKILL, "passage")
	bool isRBombardForceAbility() const CLS_HAS(m_skills, CLSD_SKILL, "rBombardForceAbility")
	bool isRivalTerritory() const CLS_HAS(m_skills, CLSD_SKILL, "rivalTerritory")
	bool isSabotage() const CLS_HAS(m_skills, CLSD_SKILL, "sabotage")
	bool isStateReligion() const CLS_HAS(m_skills, CLSD_SKILL, "stateReligion")
	bool isStealPlans() const CLS_HAS(m_skills, CLSD_SKILL, "stealPlans")
	bool isStealthDefense() const;   // GAMEOPTION_COMBAT_WITHOUT_WARNING-gated skill read (archive mirror; .cpp)
	bool isSuicide() const CLS_HAS(m_skills, CLSD_SKILL, "suicide")
	bool isUpgradeAnywhere() const CLS_HAS(m_skills, CLSD_SKILL, "upgradeAnywhere")
	bool isWorkerTrade() const CLS_HAS(m_skills, CLSD_SKILL, "workerTrade")
	bool isAttackOnlyCities() const CLS_HAS(m_skills, CLSD_SKILL, "attackOnlyCities")
	bool isIgnoreNoEntryLevel() const CLS_HAS(m_skills, CLSD_SKILL, "ignoreNoEntryLevel")
	bool isFliesToMove() const CLS_HAS(m_skills, CLSD_SKILL, "fliesToMove")
	bool isFreeDrop() const CLS_HAS(m_skills, CLSD_SKILL, "freeDrop")
	bool getDCMFighterEngage() const CLS_HAS(m_skills, CLSD_SKILL, "dcmFighterEngage")
	bool isRenderBelowWater() const CLS_HAS(m_skills, CLSD_SKILL, "renderBelowWater")
	bool isMilitaryTrade() const CLS_HAS(m_skills, CLSD_SKILL, "militaryTrade")
	bool isNoSelfHeal() const CLS_HAS(m_skills, CLSD_SKILL, "noSelfHeal")
	bool canAnimalIgnoresBorders() const { return false; }  // DEAD (owner 2026-07-11): animal border-ignoring is PURE game-option runtime (CvUnit::canAnimalIgnoresBorders), not curated unit data
	bool isGoldenAge() const { const CvJsonGrants* g = getGrants(); return g && g->flag("goldenAge"); }  // grants.goldenAge
	bool isSpy() const { const CvJsonBoolBlock* t = getTags(); return t && t->has("spy"); }               // tags.spy

	// --- DCM air-bomb tier (skills.dcmAirBomb = count of set levels) ---
	bool getDCMAirBomb1() const { return m_iDcmAirBombTier >= 1; }
	bool getDCMAirBomb2() const { return m_iDcmAirBombTier >= 2; }
	bool getDCMAirBomb3() const { return m_iDcmAirBombTier >= 3; }
	bool getDCMAirBomb4() const { return m_iDcmAirBombTier >= 4; }
	bool getDCMAirBomb5() const { return m_iDcmAirBombTier >= 5; }

	// --- grants lists (free promotions / great people / free buildings) ---
	bool getFreePromotions(int i) const { return contains(m_freePromotions, i); }   // grants.promotions
	bool getGreatPeoples(int i) const { return contains(m_greatPeoples, i); }       // grants.greatPeople
	int getBuildings(int i) const { return (i >= 0 && i < (int)m_buildings.size()) ? m_buildings[i] : -1; }  // grants.buildings
	bool getHasBuilding(int i) const { return contains(m_buildings, i); }
	int getNumBuildings() const { return (int)m_buildings.size(); }

	// --- identity lists ---
	bool getUnitAIType(int i) const { return contains(m_unitAIs, i); }              // identity.unitAIs
	bool getNotUnitAIType(int i) const { return contains(m_notUnitAIs, i); }        // identity.notUnitAIs
	UnitCombatTypes getSubCombatType(int i) const { return (i >= 0 && i < (int)m_subCombatTypes.size()) ? (UnitCombatTypes)m_subCombatTypes[i] : NO_UNITCOMBAT; }  // root `combatClasses`
	int getNumSubCombatTypes() const { return (int)m_subCombatTypes.size(); }
	bool isSubCombatType(UnitCombatTypes e) const { return contains(m_subCombatTypes, (int)e); }
	const std::vector<UnitCombatTypes>& getSubCombatTypes() const { return reinterpret_cast<const std::vector<UnitCombatTypes>&>(m_subCombatTypes); }
	const std::vector<MapCategoryTypes>& getMapCategories() const { return reinterpret_cast<const std::vector<MapCategoryTypes>&>(m_mapCategories); }  // identity.mapCategories
	bool isTerrainImpassableType(TerrainTypes e) const { return contains(m_impassableTerrains, (int)e); }        // identity.terrainImpassable
	const std::vector<TerrainTypes>& getImpassableTerrains() const { return reinterpret_cast<const std::vector<TerrainTypes>&>(m_impassableTerrains); }
	bool isFeatureImpassableType(FeatureTypes e) const { return contains(m_impassableFeatures, (int)e); }        // identity.featureImpassable
	const std::vector<FeatureTypes>& getImpassableFeatures() const { return reinterpret_cast<const std::vector<FeatureTypes>&>(m_impassableFeatures); }
	int getDefendAgainstUnit(int i) const { return (i >= 0 && i < (int)m_defendAgainstUnits.size()) ? m_defendAgainstUnits[i] : -1; }  // identity.defendAgainstUnit
	int getNumDefendAgainstUnits() const { return (int)m_defendAgainstUnits.size(); }
	bool isDefendAgainstUnit(int i) const { return contains(m_defendAgainstUnits, i); }
	int getHeritage(int i) const { return (i >= 0 && i < (int)m_heritage.size()) ? m_heritage[i] : -1; }         // identity.heritage
	bool getHasHeritage(int i) const { return contains(m_heritage, i); }
	int getNumHeritage() const { return (int)m_heritage.size(); }
	int getNumEnabledCivilizationTypes() const { return (int)m_enabledCivs.size(); }                             // identity.enabledCivilizations
	const EnabledCivilizations& getEnabledCivilizationType(int i) const { return (i >= 0 && i < (int)m_enabledCivs.size()) ? m_enabledCivs[i] : m_emptyEnabledCivilization; }
	const char* getUnitNames(int i) const { return (i >= 0 && i < (int)m_unitNames.size()) ? m_unitNames[i].c_str() : ""; }  // identity.uniqueNames
	int getNumUnitNames() const { return (int)m_unitNames.size(); }

	// --- succession / replacedBy ---
	int getUnitUpgrade(int i) const { return (i >= 0 && i < (int)m_upgrades.size()) ? m_upgrades[i] : -1; }      // succession.upgradesTo
	int getNumUnitUpgrades() const { return (int)m_upgrades.size(); }
	bool isUnitUpgrade(int i) const { return contains(m_upgrades, i); }
	int getSupersedingUnit(int i) const { return (i >= 0 && i < (int)m_superseding.size()) ? m_superseding[i] : -1; }  // replacedBy.units
	short getNumSupersedingUnits() const { return (short)m_superseding.size(); }
	bool isSupersedingUnit(int i) const { return contains(m_superseding, i); }
	std::vector<int> getUnitUpgradeChain() const { return m_upgradeChain; }         // runtime-built via addUnitToUpgradeChain
	void addUnitToUpgradeChain(int i) { m_upgradeChain.push_back(i); }

	// --- allowed (instance caps) ---
	int getMaxGlobalInstances() const { const CvJsonAllowed* a = getAllowed(); return a ? a->cap("world") : -1; }   // allowed.world
	int getMaxPlayerInstances() const { const CvJsonAllowed* a = getAllowed(); return a ? a->cap("empire") : -1; }  // allowed.empire

	// --- edges (target-side obsolete) ---
	int getObsoleteTech() const { const CvJsonEdges* e = getEdges(); const std::vector<int>* l = e ? e->find(EDGEF_OBSOLETED_BY, EDGEB_TECHS) : NULL; return (l && !l->empty()) ? (*l)[0] : -1; }  // obsoletedBy.techs

	// --- requires-tree reconstructed prereqs ---
	int getPrereqAndTech() const { return m_iPrereqAndTech; }                     // requires.build.all TECH_ (first)
	const std::vector<TechTypes>& getPrereqAndTechs() const { return reinterpret_cast<const std::vector<TechTypes>&>(m_prereqAndTechs); }
	int getPrereqAndBonus() const { return m_iPrereqAndBonus; }                   // requires.build.all BONUS_ (trade|vicinity)
	int getPrereqVicinityBonus() const { return m_iPrereqVicinityBonus; }         // requires.build.all BONUS_ (vicinity connected)
	const std::vector<BonusTypes>& getPrereqOrBonuses() const { return reinterpret_cast<const std::vector<BonusTypes>&>(m_prereqOrBonuses); }
	const std::vector<BonusTypes>& getPrereqOrVicinityBonuses() const { return reinterpret_cast<const std::vector<BonusTypes>&>(m_prereqOrVicinityBonuses); }
	int getPrereqReligion() const { return m_iPrereqReligion; }                   // requires.build.all RELIGION_
	int getPrereqCorporation() const { return m_iPrereqCorporation; }             // requires.build HAS_CORPORATION
	int getStateReligion() const { return m_iStateReligion; }                     // requires.build STATE_RELIGION
	int getHolyCity() const { return m_iHolyCity; }                               // requires.build IS_HOLY_CITY
	bool isRequiresStateReligionInCity() const { return m_bRequiresStateReligionInCity; }  // requires.build STATE_RELIGION_IN_CITY
	int getPrereqAndBuilding(int i) const { return (i >= 0 && i < (int)m_prereqAndBuildings.size()) ? m_prereqAndBuildings[i] : -1; }  // requires.build.all BUILDING_
	int getNumPrereqAndBuildings() const { return (int)m_prereqAndBuildings.size(); }
	bool isPrereqAndBuilding(int i) const { return contains(m_prereqAndBuildings, i); }
	BuildingTypes getPrereqOrBuilding(int i) const { return (i >= 0 && i < (int)m_prereqOrBuildings.size()) ? (BuildingTypes)m_prereqOrBuildings[i] : NO_BUILDING; }  // requires.build.all {any} BUILDING_
	int getPrereqOrBuildingsNum() const { return (int)m_prereqOrBuildings.size(); }
	bool isPrereqOrBuilding(int i) const { return contains(m_prereqOrBuildings, i); }
	const std::vector<HeritageTypes>& getPrereqAndHeritage() const { return reinterpret_cast<const std::vector<HeritageTypes>&>(m_prereqAndHeritage); }  // requires.build.all HERITAGE_
	const std::vector<HeritageTypes>& getPrereqOrHeritage() const { return reinterpret_cast<const std::vector<HeritageTypes>&>(m_prereqOrHeritage); }
	bool isPrereqOrCivics(int i) const { return contains(m_prereqOrCivics, i); }   // requires.build.all {any} CIVIC_

	// --- vision ---
	int getInvisibleType() const { return m_iInvisibleType; }                     // vision.invisible
	int getSeeInvisibleType(int i) const { return (i >= 0 && i < (int)m_seeInvisibleTypes.size()) ? m_seeInvisibleTypes[i] : -1; }  // vision.seeInvisible
	int getNumSeeInvisibleTypes() const { return (int)m_seeInvisibleTypes.size(); }
	int getNumVisibilityIntensityTypes() const { return (int)m_visibilityIntensity.size(); }                    // vision.visibilityIntensity
	int getVisibilityIntensityType(int i) const { return (i >= 0 && i < (int)m_visibilityIntensity.size()) ? (int)m_visibilityIntensity[i].first : -1; }
	bool isVisibilityIntensityType(int i) const { for (size_t k = 0; k < m_visibilityIntensity.size(); ++k) if ((int)m_visibilityIntensity[k].first == i) return true; return false; }
	const InvisibilityArray& getVisibilityIntensityTypes() const { return m_visibilityIntensity; }
	int getNumInvisibilityIntensityTypes() const { return (int)m_invisibilityIntensity.size(); }                // vision.invisibilityIntensity
	int getInvisibilityIntensityType(int i) const { return (i >= 0 && i < (int)m_invisibilityIntensity.size()) ? (int)m_invisibilityIntensity[i].first : -1; }
	bool isInvisibilityIntensityType(int i) const { for (size_t k = 0; k < m_invisibilityIntensity.size(); ++k) if ((int)m_invisibilityIntensity[k].first == i) return true; return false; }
	const InvisibilityArray& getInvisibilityIntensityTypes() const { return m_invisibilityIntensity; }

	// --- heal-unit-combat records (heal.unit.unitCombat.{UC} = {heal, adjacentHeal}) ---
	int getNumHealUnitCombatTypes() const { return (int)m_healUnitCombat.size(); }
	const HealUnitCombat& getHealUnitCombatType(int i) const { return (i >= 0 && i < (int)m_healUnitCombat.size()) ? m_healUnitCombat[i] : m_emptyHealUnitCombat; }

	// --- property engine (self-contained; XML-era manip data deferred with the CvOutcome system) ---
	const CvPropertyManipulators* getPropertyManipulators() const { return &m_PropertyManipulators; }

	// --- combat-class membership (primary or sub-combat) ---
	bool hasUnitCombat(UnitCombatTypes e) const { return getUnitCombatType() == (int)e || contains(m_subCombatTypes, (int)e); }

	// ============================================================================================================
	//  DEFERRED SYSTEMS + CURATOR-GAPS -- documented, not silently stubbed. (See the report for the full list.)
	// ============================================================================================================
	// CvOutcome kill/action-mission system -- fed from the unit's `outcomes` JSON block in mapFrom (#430; the CvOutcome
	// engine itself is unchanged, just JSON-loaded). Getters mirror the archived surface (SourceArchive:1230-1270).
	int getNumActionOutcomes() const { return (int)m_aOutcomeMissions.size(); }
	const CvOutcomeList* getActionOutcomeList(int index) const;                       // CvOutcomeMission deref -> .cpp
	const CvOutcomeList* getActionOutcomeListByMission(MissionTypes eMission) const;
	MissionTypes getActionOutcomeMission(int index) const;                           // CvOutcomeMission deref -> .cpp
	const CvOutcomeMission* getOutcomeMission(int index) const { return m_aOutcomeMissions[index]; }
	const CvOutcomeMission* getOutcomeMissionByMission(MissionTypes eMission) const;
	const CvOutcomeList* getKillOutcomeList() const { return &m_KillOutcomeList; }

	// strength.unit.vsUnit.{U}.attack/defense.percent -> IDValueMap (populated via setValue in mapFrom):
	const IDValueMap<UnitTypes, int>& getUnitAttackModifiers() const { return m_unitAttackModifiers; }
	const IDValueMap<UnitTypes, int>& getUnitDefenseModifiers() const { return m_unitDefenseModifiers; }
	const IDValueMap<UnitTypes, int, -1>& getFlankingStrikeUnits() const { return m_flankingStrikeUnits; }  // strength.unit.flankingUnit.{UNIT}.percent (by-unit flanking)

	int getDomainCargo() const { return m_iDomainCargo; }        // cargo.unit.space.unit qualifier IS_<DOMAIN> -> DOMAIN_<X>
	int getFeaturePassableTech(int i) const { std::map<int,int>::const_iterator it = m_featurePassableTech.find(i); return it != m_featurePassableTech.end() ? it->second : -1; }  // identity.featurePassableTechs
	int getTerrainPassableTech(int i) const { std::map<int,int>::const_iterator it = m_terrainPassableTech.find(i); return it != m_terrainPassableTech.end() ? it->second : -1; }  // identity.terrainPassableTechs
	int getPrereqGameOption() const { return m_iPrereqGameOption; }  // entity gate `enabled: GAMEOPTION_X`
	int getNotGameOption() const { return m_iNotGameOption; }        // entity gate `disabled: GAMEOPTION_X`
	int getNumInvisibleTerrainChanges() const { return (int)m_invisibleTerrainChanges.size(); }               // vision.invisibleTerrain
	const InvisibleTerrainChanges& getInvisibleTerrainChange(int i) const { return (i >= 0 && i < (int)m_invisibleTerrainChanges.size()) ? m_invisibleTerrainChanges[i] : m_emptyInvisibleTerrainChanges; }
	int getNumInvisibleFeatureChanges() const { return (int)m_invisibleFeatureChanges.size(); }               // vision.invisibleFeature
	const InvisibleFeatureChanges& getInvisibleFeatureChange(int i) const { return (i >= 0 && i < (int)m_invisibleFeatureChanges.size()) ? m_invisibleFeatureChanges[i] : m_emptyInvisibleFeatureChanges; }

	int getReligionSpreads(int i) const { return mapGet(m_religionSpreads, i); }        // spread.religion[RELIGION] -- per-religion spread strength
	int getCorporationSpreads(int i) const { return mapGet(m_corporationSpreads, i); }  // spread.corporation[CORPORATION] -- per-corp spread strength
	int getBaseCargoVolume() const;                              // DERIVED SM cargo volume (archive post-load pass); see ensureSMBase
	int getSMCargoSpace() const { return 0; }                    // GAP: size-matters cargo not curated
	int getSMCargoVolume() const { return 0; }                   // GAP: size-matters cargo not curated
	int getMaxStartEra() const { return -1; }                    // GAP: MaxStartEra not curated
	int getUnitGroupRequired(int /*i*/) const { return 0; }      // GAP: UnitGroupRequired not curated
	bool getPassableRouteNeeded(int /*i*/) const { return false; }  // GAP: PassableRouteNeeded not curated
	int getHealAsType(int /*i*/) const { return -1; }            // GAP: HealAsTypes not curated
	int getNumHealAsTypes() const { return 0; }
	bool isHealAsType(int /*i*/) const { return false; }
	int getTrapSetWithPromotionType(int /*i*/) const { return -1; }  // GAP: trap fields not curated
	int getNumTrapSetWithPromotionTypes() const { return 0; }
	bool isTrapSetWithPromotionType(int /*i*/) const { return false; }
	int getTrapImmunityUnitCombatType(int /*i*/) const { return -1; }
	int getNumTrapImmunityUnitCombatTypes() const { return 0; }
	bool isTrapImmunityUnitCombatType(int /*i*/) const { return false; }
	int getCategory(int /*i*/) const { return -1; }              // GAP: Categories not curated
	int getNumCategories() const { return 0; }
	bool isCategory(int /*i*/) const { return false; }
	int getNumTrapDisableUnitCombatTypes() const { return 0; }
	int getTrapDisableUnitCombatType(int /*i*/) const { return -1; }
	bool isTrapDisableUnitCombatType(int /*i*/) const { return false; }
	const UnitCombatModifierArray& getTrapDisableUnitCombatTypes() const { return m_emptyUnitCombatModifierArray; }
	int getNumTrapAvoidanceUnitCombatTypes() const { return 0; }
	int getTrapAvoidanceUnitCombatType(int /*i*/) const { return -1; }
	bool isTrapAvoidanceUnitCombatType(int /*i*/) const { return false; }
	const UnitCombatModifierArray& getTrapAvoidanceUnitCombatTypes() const { return m_emptyUnitCombatModifierArray; }
	int getNumTrapTriggerUnitCombatTypes() const { return 0; }
	int getTrapTriggerUnitCombatType(int /*i*/) const { return -1; }
	bool isTrapTriggerUnitCombatType(int /*i*/) const { return false; }
	const UnitCombatModifierArray& getTrapTriggerUnitCombatTypes() const { return m_emptyUnitCombatModifierArray; }
	int getNumTerrainWorkRateModifierTypes() const { return 0; }   // GAP: work-rate modifiers not curated
	int getTerrainWorkRateModifierType(int /*i*/) const { return 0; }
	bool isTerrainWorkRateModifierType(int /*i*/) const { return false; }
	int getNumFeatureWorkRateModifierTypes() const { return 0; }
	int getFeatureWorkRateModifierType(int /*i*/) const { return 0; }
	bool isFeatureWorkRateModifierType(int /*i*/) const { return false; }
	int getNumBuildWorkRateModifierTypes() const { return 0; }
	int getBuildWorkRateModifierType(int /*i*/) const { return 0; }
	bool isBuildWorkRateModifierType(int /*i*/) const { return false; }
	int getNumVisibilityIntensityRangeTypes() const { return 0; }  // GAP: VisibilityIntensityRange not curated
	int getVisibilityIntensityRangeType(int /*i*/) const { return -1; }
	bool isVisibilityIntensityRangeType(int /*i*/) const { return false; }
	const InvisibilityArray& getVisibilityIntensityRangeTypes() const { return m_emptyInvisibilityArray; }
	int getNumInvisibleImprovementChanges() const { return 0; }   // UNITS author no InvisibleImprovementChanges (XML census 0) -- the authored rows live on
	                                                              // unitcombats/promotions, whose pocos map them fully; this empty stub IS the faithful value
	const InvisibleImprovementChanges& getInvisibleImprovementChange(int /*i*/) const { return m_emptyInvisibleImprovementChanges; }
	int getNumVisibleTerrainChanges() const { return 0; }         // GAP: visible* changes not curated
	const InvisibleTerrainChanges& getVisibleTerrainChange(int /*i*/) const { return m_emptyInvisibleTerrainChanges; }
	int getNumVisibleFeatureChanges() const { return 0; }
	const InvisibleFeatureChanges& getVisibleFeatureChange(int /*i*/) const { return m_emptyInvisibleFeatureChanges; }
	int getNumVisibleImprovementChanges() const { return 0; }
	const InvisibleImprovementChanges& getVisibleImprovementChange(int /*i*/) const { return m_emptyInvisibleImprovementChanges; }
	int getNumVisibleTerrainRangeChanges() const { return 0; }
	const InvisibleTerrainChanges& getVisibleTerrainRangeChange(int /*i*/) const { return m_emptyInvisibleTerrainChanges; }
	int getNumVisibleFeatureRangeChanges() const { return 0; }
	const InvisibleFeatureChanges& getVisibleFeatureRangeChange(int /*i*/) const { return m_emptyInvisibleFeatureChanges; }
	const InvisibleImprovementChanges& getVisibleImprovementRangeChange(int /*i*/) const { return m_emptyInvisibleImprovementChanges; }
	int getNumVisibleImprovementRangeChanges() const { return 0; }
	int getNumGroupSpawnUnitCombatTypes() const { return (int)m_groupSpawn.size(); }   // groupSpawn struct rows {unitCombat, chance, title}
	const GroupSpawnUnitCombat& getGroupSpawnUnitCombatType(int i) const { return (i >= 0 && i < (int)m_groupSpawn.size()) ? m_groupSpawn[i] : m_emptyGroupSpawnUnitCombat; }
	CvWString getCivilizationName(int /*i*/) const { return CvWString(); }         // GAP: per-civ naming not curated
	int getCivilizationNamesVectorSize() const { return 0; }
	CvWString getCivilizationNamesNamesVectorElement(int /*i*/) const { return CvWString(); }
	CvWString getCivilizationNamesValuesVectorElement(int /*i*/) const { return CvWString(); }

	// runtime-derived / mesh / SM values the curator does not emit (kept at safe legacy defaults):
	int getZobristValue() const { return m_iZobristValue; }      // non-XML runtime map-hash, drawn from the synced RNG in the ctor (mirrors the archive)
	int getGroupSize() const { return 1; }                       // UnitMeshGroups not curated (single-individual fallback)
	int getGroupDefinitions() const { return 1; }                // UnitMeshGroups not curated (single mesh-group fallback)
	int getMeleeWaveSize() const { return 0; }                   // UnitMeshGroups not curated
	int getRangedWaveSize() const { return 0; }                  // UnitMeshGroups not curated
	int getEraInfo() const;                                      // DERIVED from the prereq tech's era (archive CvUnitInfo::getEraInfo); see .cpp
	int getMaxHP(bool /*bForLoad*/ = false) const { return 100; } // legacy default (m_iMaxHP==0 / non-SM -> 100, archive
	                                                              // CvUnitInfo.cpp:1724); SM per-unit maxHP not curated yet
	int getDamageModifier() const { return 0; }                  // GAP: DamageModifier not curated
	int getTaunt() const { return 0; }                           // GAP: Taunt not curated
	int getPoisonProbabilityModifier() const { return 0; }       // GAP: not curated
	int getNumTriggers() const { return 0; }                     // GAP: triggers not curated
	int getAnimalIgnoresBorders() const { return 0; }  // DEAD: game-option-driven at runtime (see canAnimalIgnoresBorders)
	int getReligiousCombatModifier() const { return 0; }         // GAP: ReligiousCombatModifier not curated
	int getHillsWorkModifier() const { return 0; }               // GAP: not curated
	int getPeaksWorkModifier() const { return 0; }               // GAP: not curated
	int getEndurance() const { return 0; }                       // GAP: not curated
	int getBaseGroupRank() const;                                // DERIVED at load: Σ combat classes' getGroupBase (>-10)
	int getRBombardDamageMaxUnits() const { return 0; }          // GAP: iRBombardDamageMaxUnits not curated
	int getTrapComplexity() const { return 0; }                  // GAP: trap fields not curated
	int getTrapDamageMax() const { return 0; }                   // GAP: trap fields not curated
	int getTrapDamageMin() const { return 0; }                   // GAP: trap fields not curated
	bool getTerrainNative(int /*i*/) const { return false; }     // GAP: TerrainNative not curated
	bool getFeatureNative(int /*i*/) const { return false; }     // GAP: FeatureNative not curated
	bool isExcile() const { return false; }                      // GAP: bExcile not curated
	bool isOnslaught() const { return false; }                   // GAP: bOnslaught not curated
	int getTotalModifiedCombatStrength100(bool bSizeMatters) const;   // DERIVED (archive CvUnitInfo::…): 100×(combat + SM change base), SM-scaled; see .cpp
	bool isQualifiedPromotionType(int /*i*/) const { return false; }   // pediahelp: computed at post-load (not curated)
	bool setQualifiedPromotionType(const int /*iPromo*/, std::vector<int>& /*checklist*/) { return false; }
	void setQualifiedPromotionTypes() {}
	void setCanAnimalIgnores() {}                                // skills.animalIgnoresBorders already read at load
	const BoolExpr* getTrainCondition() const { return NULL; }   // requires.build carries the folded condition instead
	const std::vector<ConstructRequirement>& getTrainRequirements() const { return m_trainRequirements; }  // #195: derived post-load (empty)

	// runtime / no-JSON-source flags kept at legacy defaults (fields the curator does not emit):
	// TWO semantics on one overload, which is how this came to be stubbed `false` -- and a stub here is not a
	// harmless default: `addStartUnitAI` reads the player form, so a blanket false skips EVERY candidate and a new
	// game starts with NO UNITS (instant defeat). Backed by identity.enabledCivilizations, a WHITELIST (empty =
	// available to all), the same data CvCity::canTrain gates NPC `stronglyRestricted` civs on.
	//   NO_PLAYER -> is this unit RESTRICTED to specific civilizations? (CvGame's "invalidates Neanderthal units")
	//   a player  -> is it available to THAT player's civilization?
	bool isCivilizationUnit(const PlayerTypes ePlayer = NO_PLAYER) const;
	bool isWildAnimal() const { return false; }                  // GAP: bWildAnimal not curated
	bool canAnimalIgnoresCities() const { return false; }        // GAP: only animalIgnoresBorders emitted
	bool canAnimalIgnoresImprovements() const { return false; }  // GAP: only animalIgnoresBorders emitted
	bool isNoNonOwnedEntry() const { return false; }             // GAP: bNoNonOwnedEntry (ls612) not curated
	bool isRenderAlways() const { return false; }                // GAP: bRenderAlways not curated
	bool isLineOfSight() const { return false; }                 // GAP: bLineOfSight not curated
	bool isNoInvisibility() const { return false; }              // GAP: bNoInvisibility not curated
	bool isTriggerBeforeAttack() const { return false; }         // GAP: bTriggerBeforeAttack not curated
	bool isAnimal() const { return false; }                      // GAP: bAnimal not curated
	bool isGatherHerd() const { return false; }                  // GAP: bGatherHerd not curated
	bool isSlave() const { return false; }                       // GAP: bSlave not curated
	bool isForceUpgrade() const { return false; }                // GAP: bForceUpgrade not curated
	bool isNoRevealMap() const CLS_HAS(m_skills, CLSD_SKILL, "noRevealMap")  // skills.noRevealMap (bNoRevealMap, goody-hut gate)
	bool canMergeSplit() const { return false; }                 // GAP: bCanMergeSplit not curated
	// The three legacy military* flags UNIFY onto the `military` tag / IS_MILITARY (skills.md §3; owner-confirmed
	// 2026-07-11 -- a deliberate behaviour change, the differing legacy corpora collapse to one verdict).
	bool isMilitaryProduction() const { const CvJsonBoolBlock* t = getTags(); return t && t->has("military"); }
	bool isMilitaryHappiness() const  { const CvJsonBoolBlock* t = getTags(); return t && t->has("military"); }
	bool isMilitarySupport() const    { const CvJsonBoolBlock* t = getTags(); return t && t->has("military"); }
	// RUNTIME command-type (not curated; CvHotkeyInfo has no command-type member) -- SetGlobalActionInfo assigns
	// COMMAND_UPGRADE via setCommandType at load and reads it back (getCommandInfo(getCommandType())), so it MUST be
	// stored, not a -1 stub (a -1 stub -> getCommandInfo(-1) -> garbage-string crash in SetGlobalActionInfo).
	int getCommandType() const { return m_iCommandType; }
	void setCommandType(int iNewType) { m_iCommandType = iNewType; }
	void setInvisible(bool /*bEnable*/) {}                        // legacy runtime toggle (no-op on the immutable poco)
	void setPowerValue(int iNewValue) { m_iPowerValue = iNewValue; }   // MUST store: CvGame.cpp:317 halves it for great-generals under !GAMEOPTION_UNIT_GREAT_COMMANDERS (a no-op discarded that)
	float getUnitMaxSpeed() const { return 0.0f; }               // UnitMeshGroups not curated
	float getUnitPadTime() const { return 0.0f; }                // UnitMeshGroups not curated
	bool canAcquireExperience() const;                           // DERIVED: true iff some promotion targets this unit's combat class (archive CvUnitInfo::canAcquireExperience); see .cpp
	// era/style art-define-tag grid: legacy per-era art; the poco holds one world.art.icon tag (grid not curated):
	const char* getClassicalArtDefineTag(int /*i*/, UnitArtStyleTypes /*eStyle*/) const { return ""; }
	void setClassicalArtDefineTag(int /*i*/, const char* /*szVal*/) {}
	const char* getRennArtDefineTag(int /*i*/, UnitArtStyleTypes /*eStyle*/) const { return ""; }
	void setRennArtDefineTag(int /*i*/, const char* /*szVal*/) {}
	const char* getIndustrialArtDefineTag(int /*i*/, UnitArtStyleTypes /*eStyle*/) const { return ""; }
	void setIndustrialArtDefineTag(int /*i*/, const char* /*szVal*/) {}
	const char* getFutureArtDefineTag(int /*i*/, UnitArtStyleTypes /*eStyle*/) const { return ""; }
	void setFutureArtDefineTag(int /*i*/, const char* /*szVal*/) {}
	const char* getEarlyArtDefineTag(int /*i*/, UnitArtStyleTypes /*eStyle*/) const { return ""; }
	void setEarlyArtDefineTag(int /*i*/, const char* /*szVal*/) {}
	const char* getLateArtDefineTag(int /*i*/, UnitArtStyleTypes /*eStyle*/) const { return ""; }
	void setLateArtDefineTag(int /*i*/, const char* /*szVal*/) {}
	const char* getMiddleArtDefineTag(int /*i*/, UnitArtStyleTypes /*eStyle*/) const { return ""; }
	void setMiddleArtDefineTag(int /*i*/, const char* /*szVal*/) {}
	void updateArtDefineButton() {}

	// --- the composed section units (by value; the base's mapFrom dispatch writes them via mut*) ---
	virtual const CvJsonRequires*  getRequires()  const { return &m_requires; }
	virtual const CvJsonEdges*     getEdges()     const { return &m_edges; }
	virtual const CvJsonAllowed*   getAllowed()   const { return &m_allowed; }
	virtual const CvJsonGrants*    getGrants()    const { return &m_grants; }
	virtual const CvJsonModifiers* getModifiers() const { return &m_modifiers; }
	virtual const CvJsonBoolBlock* getSkills()    const { return &m_skills; }
	virtual const CvJsonBoolBlock* getTags()      const { return &m_tags; }
	virtual const CvJsonGate*      getGate()      const { return &m_gate; }

protected:
	virtual CvJsonRequires*  mutRequires()  { return &m_requires; }
	virtual CvJsonEdges*     mutEdges()     { return &m_edges; }
	virtual CvJsonAllowed*   mutAllowed()   { return &m_allowed; }
	virtual CvJsonGrants*    mutGrants()    { return &m_grants; }
	virtual CvJsonModifiers* mutModifiers() { return &m_modifiers; }
	virtual CvJsonBoolBlock* mutSkills()    { return &m_skills; }
	virtual CvJsonBoolBlock* mutTags()      { return &m_tags; }
	virtual CvJsonGate*      mutGate()      { return &m_gate; }

private:
	void reconstructPrereqs();   // walk the composed requires.build into the typed prereq members (mapFrom-time)
	void mapOutcomes(const picojson::object& o);   // #430: parse the `outcomes` block into the CvOutcome engine objects

	bool skill(const char* szName) const { const CvJsonBoolBlock* s = getSkills(); return s && s->has(szName); }   // cold/oracle read only -- hot getters use CLS_HAS

	static bool contains(const std::vector<int>& v, int id) { for (size_t i = 0; i < v.size(); ++i) if (v[i] == id) return true; return false; }
	static int mapGet(const std::map<int, int>& m, int k) { std::map<int, int>::const_iterator it = m.find(k); return it != m.end() ? it->second : 0; }

	// composed section units
	CvJsonRequires  m_requires;
	CvJsonEdges     m_edges;
	CvJsonAllowed   m_allowed;
	CvJsonGrants    m_grants;
	CvJsonModifiers m_modifiers;
	CvJsonBoolBlock m_skills;
	CvJsonBoolBlock m_tags;
	CvJsonGate      m_gate;

	std::string m_szArtDefineTag;      // world.art.icon
	std::string m_szFormationType;     // identity.formationType
	int m_iZobristValue;               // non-XML runtime map-hash (drawn from the synced RNG in the ctor, mirrors the archive)
	int m_iCommandType;                // non-XML runtime command-type (assigned by SetGlobalActionInfo via setCommandType)
	mutable int m_iBaseGroupRankCache; // lazy: Σ combat classes' getGroupBase (derived; -1 = not yet computed)
	// Lazy SM strength/cargo base derivation (archive post-load pass CvUnitInfo.cpp:4345-4400), computed together:
	mutable bool m_bSMBaseDone;
	mutable int  m_iSMChangeBase;         // Σ combat classes' getStrengthChange()
	mutable int  m_iSMModifierBase;       // Σ (quality/size/group Base - 5) where > -10, over combat classes
	mutable int  m_iBaseCargoVolumeCache; // derived SM cargo volume (>= 1)
	void ensureSMBase() const;            // fill the three above in one combat-class pass
	CvPropertyManipulators m_PropertyManipulators;   // empty (property engine; XML-era manip data deferred)

	// CvOutcome system -- parsed from the unit's `outcomes` JSON block in mapFrom (#430).
	CvOutcomeList m_KillOutcomeList;                    // outcomes.kill[]
	std::vector<CvOutcomeMission*> m_aOutcomeMissions;  // outcomes.actions[] (heap-owned; freed in the dtor)

	// identity.base + identity scalars + cost + cargo
	int m_iCombat, m_iMoves, m_iWorkRate, m_iAirCombat, m_iCombatLimit, m_iAirCombatLimit, m_iAirUnitCap;
	int m_iUnitCombatType, m_iDomainType, m_iDefaultUnitAIType, m_iSpecialUnitType, m_iAdvisorType, m_iLeaderPromotion;
	int m_iReligionType, m_iUnitCaptureType, m_iInvisibleType;
	int m_iAssetValue, m_iPowerValue, m_iXPValueAttack, m_iXPValueDefense, m_iConscriptionValue, m_iAggression;
	int m_iAnimalCombatModifier, m_iCommandRange, m_iControlPoints, m_iLeaderExperience, m_iMinAreaSize, m_iEspionagePoints;
	int m_iProductionCost, m_iBaseUpkeep, m_iHurryCostModifier, m_iInstanceCostModifier, m_iAdvancedStartCost;
	int m_iCargoSpace, m_iSpecialCargo, m_iSMNotSpecialCargo, m_iDomainCargo;
	// section-5 family scalars
	int m_iCityAttackModifier, m_iCityDefenseModifier, m_iHillsAttackModifier, m_iHillsDefenseModifier, m_iVSBarbs;
	int m_iAttackCombatModifier, m_iDefenseCombatModifier, m_iCombatModifierPerSizeMore, m_iCombatModifierPerSizeLess;
	int m_iCombatModifierPerVolumeMore, m_iCombatModifierPerVolumeLess, m_iLunge, m_iEnclose, m_iUnnerve, m_iDynamicDefense;
	int m_iStealthStrikes, m_iStealthCombatModifier, m_iBreakdownChance, m_iBreakdownDamage;
	int m_iWithdrawalProbability, m_iFirstStrikes, m_iChanceFirstStrikes;
	int m_iBombardRate, m_iRBombardDamage, m_iRBombardDamageLimit, m_iDCMBombRange, m_iDCMBombAccuracy, m_iBombRate;
	int m_iCollateralDamage, m_iCollateralDamageLimit, m_iCollateralDamageMaxUnits;
	int m_iAirRange, m_iInterceptionProbability, m_iEvasionProbability, m_iNukeRange;
	int m_iCaptureProbabilityModifier, m_iCaptureResistanceModifier, m_iInsidiousness, m_iInvestigation;
	int m_iNumHealSupport, m_iSelfHealModifier, m_iDropRange, m_iCultureGarrisonValue;
	// GP-action magnitudes + ai + dcm tier
	int m_iBaseDiscover, m_iDiscoverMultiplier, m_iBaseHurry, m_iHurryMultiplier, m_iBaseTrade, m_iTradeMultiplier;
	int m_iGreatWorkCulture, m_iBaseFoodChange, m_iAIWeight, m_iDcmAirBombTier;
	// requires-tree prereqs
	int m_iPrereqAndTech, m_iPrereqAndBonus, m_iPrereqVicinityBonus, m_iPrereqReligion, m_iPrereqCorporation;
	int m_iHolyCity, m_iStateReligion, m_iPrereqGameOption, m_iNotGameOption;
	bool m_bRequiresStateReligionInCity;

	// FK-id lists (typed getters reinterpret_cast where the archived signature returns a typed vector)
	std::vector<int> m_prereqAndTechs, m_prereqOrBonuses, m_prereqOrVicinityBonuses, m_prereqAndBuildings, m_prereqOrBuildings;
	std::vector<int> m_prereqAndHeritage, m_prereqOrHeritage, m_prereqOrCivics;
	std::vector<int> m_subCombatTypes, m_impassableTerrains, m_impassableFeatures, m_mapCategories, m_defendAgainstUnits, m_heritage;
	std::vector<int> m_unitAIs, m_notUnitAIs, m_upgrades, m_superseding, m_upgradeChain;
	std::vector<int> m_freePromotions, m_greatPeoples, m_buildings, m_seeInvisibleTypes, m_targetUnits;
	std::vector<std::string> m_unitNames;   // identity.uniqueNames (raw name/text-key strings, NOT infotypes)

	// keyed id->value maps + flavours + passable-tech maps
	std::map<int, int> m_terrainAttack, m_terrainDefense, m_featureAttack, m_featureDefense;
	std::map<int, int> m_unitCombatModifier, m_domainModifier, m_flankingByUnitCombat, m_bonusProductionModifier, m_flavours;
	std::map<int, int> m_featurePassableTech, m_terrainPassableTech;   // identity.{feature|terrain}PassableTechs -> typeId->techId
	std::map<int, int> m_religionSpreads, m_corporationSpreads;   // spread.religion / spread.corporation -- per-type spread strength
	// keyed skill-extra sets
	std::set<int> m_targetUnitCombat, m_defenderUnitCombat;
	// struct-bearing populated data
	std::vector<HealUnitCombat> m_healUnitCombat;
	std::vector<EnabledCivilizations> m_enabledCivs;
	std::vector<InvisibleTerrainChanges> m_invisibleTerrainChanges;   // vision.invisibleTerrain rows
	std::vector<InvisibleFeatureChanges> m_invisibleFeatureChanges;   // vision.invisibleFeature rows
	InvisibilityArray m_visibilityIntensity, m_invisibilityIntensity;

	// empty fallbacks for deferred/gap struct- and IDValueMap-returning getters (never a temporary)
	IDValueMap<UnitTypes, int> m_unitAttackModifiers, m_unitDefenseModifiers;
	IDValueMap<UnitTypes, int, -1> m_flankingStrikeUnits;
	UnitCombatModifierArray m_emptyUnitCombatModifierArray;
	InvisibilityArray m_emptyInvisibilityArray;
	std::vector<ConstructRequirement> m_trainRequirements;
	HealUnitCombat m_emptyHealUnitCombat;
	GroupSpawnUnitCombat m_emptyGroupSpawnUnitCombat;
	std::vector<GroupSpawnUnitCombat> m_groupSpawn;   // groupSpawn block: {unitCombat, chance, title} rows
	InvisibleTerrainChanges m_emptyInvisibleTerrainChanges;
	InvisibleFeatureChanges m_emptyInvisibleFeatureChanges;
	InvisibleImprovementChanges m_emptyInvisibleImprovementChanges;
	EnabledCivilizations m_emptyEnabledCivilization;
};

#endif // CV_JSON_UNIT_INFO_H
