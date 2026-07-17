#pragma once
#ifndef CV_JSON_UNITCOMBAT_INFO_H
#define CV_JSON_UNITCOMBAT_INFO_H

//
//	CvUnitCombatInfo -- the per-type cascade info for UNITCOMBATS. Composes the section units a unitcombat
//	authors (the §6 modifier families / the §8 `skills` bool block / the entity-level gate) AND parses the values
//	the surviving legacy CvUnitCombatInfo consumers still read as named getters. Like a promotion, a unitcombat is
//	a grantor of unit skills (healsAs / rBombardDirect / ...); the unit's ACTIVE set folds them in on the instance.
//
//	GETTER SURFACE (mapped in mapFrom; every address is the curate_unitcombat.py output, confirmed against
//	Assets/Data/unitcombats/*.json). The read helpers (jsonFamVal/jsonFamMemberVal) return the raw human JSON value
//	with NO x100 -- and the curator writes the raw XML value (never descaled EXCEPT the one `*100` field) -- so the
//	round-trip reproduces the archived getter exactly. `getExtraUpkeep100` is the sole x100 re-apply (its JSON is the
//	curator-descaled human).
//	  - identity/base + refs + AI tags -> real.
//	  - the §8 `skills` flat bool block -> real; the CAP_COUNT abilities the curator collapsed to a boolean skill
//	    (magnitude intentionally dropped, owner) -> 1 when set.
//	  - every §6 modifier family SCALAR (strength/heal/withdrawal/experience/...) + the vs-keyed struct-vectors
//	    (terrain/feature/build/unitCombat/flanking/trapAvoidance) + the domain array -> real (0/empty where the
//	    family is unauthored across the data, which is the curator's actual output, not a stub).
//	  - DEFERRED, STATED (NOT curator-gaps -- the curator DOES emit these, under a different spec section that needs
//	    its own typed structured member): the §7 vision/LOS-resolver pair-vectors, the §8 keyed-skill lists
//	    (terrainDoubleMove/...), the entity-gate game-option int-lists (the gate is a CONDITION TREE, not an int
//	    list), and the parked identity lists (ggPointsForUnits/defaultStatuses). See the grouped comments below.
//	  - the CvOutcome kill/action-mission machinery is a genuinely-deferred SYSTEM (stays XML) -> NULL/0.
//

#include "CvInfo.h"
#include "Defines/CvEnums.h"     // MissionTypes/NO_MISSION, ReligionTypes/BonusTypes/EraTypes + NO_*, NUM_DOMAIN_TYPES
#include "Defines/CvStructs.h"   // TerrainModifier/FeatureModifier/BuildModifier/UnitCombatModifier/Invisible*Changes
#include <map>
#include <vector>

class CvOutcomeList;      // Sources/UI/CvOutcomeList.h -- CvOutcome system deferred (stays XML); pointer-only use
class CvOutcomeMission;   // Sources/Engine/CvOutcomeMission.h -- ditto

class CvUnitCombatInfo : public CvInfo
{
public:
	CvUnitCombatInfo();
	virtual void mapFrom(const picojson::value& entity);

	// --- textual refs (identity.{religion,culture,era} FKs; NO_* when unauthored) ---
	ReligionTypes getReligion() const { return m_eReligion; }
	BonusTypes    getCulture()  const { return m_eCulture; }
	EraTypes      getEra()      const { return m_eEra; }

	// --- create-unit `*Base` ranks (identity.base.*; quality/group/size carry the legacy -10 "unset" sentinel) ---
	int getQualityBase() const { return m_iQualityBase; }                                // identity.base.quality
	int getGroupBase()   const { return m_iGroupBase; }                                  // identity.base.group
	int getSizeBase()    const { return m_iSizeBase; }                                   // identity.base.size
	int getRBombardDamageBase()          const { return m_iRBombardDamageBase; }         // identity.base.rangedBombardDamage
	int getRBombardDamageLimitBase()     const { return m_iRBombardDamageLimitBase; }    // identity.base.rangedBombardLimit
	int getRBombardDamageMaxUnitsBase()  const { return m_iRBombardDamageMaxUnitsBase; } // identity.base.rangedBombardMaxUnits
	int getDCMBombRangeBase()            const { return m_iDCMBombRangeBase; }            // identity.base.dcmRange
	int getDCMBombAccuracyBase()         const { return m_iDCMBombAccuracyBase; }         // identity.base.dcmAccuracy

	// --- AI tags (identity.{forMilitary,forNavalMilitary} bools) ---
	bool isForMilitary()      const { return m_bForMilitary; }
	bool isForNavalMilitary() const { return m_bForNavalMilitary; }

	// --- §8 `skills` flat bool block (real; each name per curate_promotion CAP_BOOL/CAP_BOOL_X) ---
	// O(1) generated-id bit tests (CLS_HAS; SKILL_* ids from the ClassificationRegistry -- no per-call string lookups)
	bool isDefensiveVictoryMove() const CLS_HAS(m_skills, CLSD_SKILL, "defensiveVictoryMove")
	bool isFreeDrop()             const CLS_HAS(m_skills, CLSD_SKILL, "freeDrop")
	bool isOffensiveVictoryMove() const CLS_HAS(m_skills, CLSD_SKILL, "offensiveVictoryMove")
	bool isOneUp()                const CLS_HAS(m_skills, CLSD_SKILL, "oneUp")
	bool isPillageEspionage()     const CLS_HAS(m_skills, CLSD_SKILL, "pillageEspionage")
	bool isPillageMarauder()      const CLS_HAS(m_skills, CLSD_SKILL, "pillageMarauder")
	bool isPillageOnMove()        const CLS_HAS(m_skills, CLSD_SKILL, "pillageOnMove")
	bool isPillageOnVictory()     const CLS_HAS(m_skills, CLSD_SKILL, "pillageOnVictory")
	bool isPillageResearch()      const CLS_HAS(m_skills, CLSD_SKILL, "pillageResearch")
	bool isBlitz()                const CLS_HAS(m_skills, CLSD_SKILL, "blitz")
	bool isAmphib()               const CLS_HAS(m_skills, CLSD_SKILL, "amphib")
	bool isRiver()                const CLS_HAS(m_skills, CLSD_SKILL, "river")
	bool isEnemyRoute()           const CLS_HAS(m_skills, CLSD_SKILL, "enemyRoute")
	bool isAlwaysHeal()           const CLS_HAS(m_skills, CLSD_SKILL, "alwaysHeal")
	bool isHillsDoubleMove()      const CLS_HAS(m_skills, CLSD_SKILL, "hillsDoubleMove")
	bool isImmuneToFirstStrikes() const CLS_HAS(m_skills, CLSD_SKILL, "immuneToFirstStrikes")
	bool isOnslaughtChange()      const CLS_HAS(m_skills, CLSD_SKILL, "onslaught")
	bool isCanMovePeaks()         const CLS_HAS(m_skills, CLSD_SKILL, "canPassPeaks")        // dual-plane rename (owner 2026-07-02)
	bool isCanLeadThroughPeaks()  const CLS_HAS(m_skills, CLSD_SKILL, "canLeadThroughPeaks")
	bool isZoneOfControl()        const CLS_HAS(m_skills, CLSD_SKILL, "zoneOfControl")
	bool isCannotMergeSplit()     const CLS_HAS(m_skills, CLSD_SKILL, "cannotMergeSplit")
	bool isRBombardDirect()       const CLS_HAS(m_skills, CLSD_SKILL, "rBombardDirect")
	bool isRBombardForceAbility() const CLS_HAS(m_skills, CLSD_SKILL, "rBombardForceAbility")
	bool isAlwaysInvisible()      const CLS_HAS(m_skills, CLSD_SKILL, "alwaysInvisible")
	bool isHealsAs()              const CLS_HAS(m_skills, CLSD_SKILL, "healsAs")
	bool isNoSelfHeal()           const CLS_HAS(m_skills, CLSD_SKILL, "noSelfHeal")
	// grant side of the grant/revoke pairs -> the skill is present when granted (real). The REVOKE (*Subtract/
	// Remove) side sets the flag false, which the flat bool block cannot carry -> false below.
	bool isStampedeChange()         const CLS_HAS(m_skills, CLSD_SKILL, "stampede")
	bool isAttackOnlyCitiesAdd()    const CLS_HAS(m_skills, CLSD_SKILL, "attackOnlyCities")
	bool isIgnoreNoEntryLevelAdd()  const CLS_HAS(m_skills, CLSD_SKILL, "ignoreNoEntryLevel")
	bool isIgnoreZoneofControlAdd() const CLS_HAS(m_skills, CLSD_SKILL, "ignoreZoneofControl")
	bool isFliesToMoveAdd()         const CLS_HAS(m_skills, CLSD_SKILL, "fliesToMove")

	// --- CAP_COUNT abilities: curator collapsed the int amount to a boolean skill (>0 => has); magnitude dropped
	// (owner). Return 1 when the skill is set so the surviving `> 0` consumer tests read true. ---
	int getExcileChange()               const { static int s_clsId = -1; return m_skills.hasKey(s_clsId, CLSD_SKILL, "excile") ? 1 : 0; }
	int getPassageChange()              const { static int s_clsId = -1; return m_skills.hasKey(s_clsId, CLSD_SKILL, "passage") ? 1 : 0; }
	int getNoNonOwnedCityEntryChange()  const { static int s_clsId = -1; return m_skills.hasKey(s_clsId, CLSD_SKILL, "noNonOwnedCityEntry") ? 1 : 0; }
	int getBarbCoExistChange()          const { static int s_clsId = -1; return m_skills.hasKey(s_clsId, CLSD_SKILL, "barbCoExist") ? 1 : 0; }
	int getBlendIntoCityChange()        const { static int s_clsId = -1; return m_skills.hasKey(s_clsId, CLSD_SKILL, "blendIntoCity") ? 1 : 0; }
	int getStealthDefenseChange()       const;   // GAMEOPTION_COMBAT_WITHOUT_WARNING-gated (archive mirror; .cpp)
	int getDefenseOnlyChange()          const { static int s_clsId = -1; return m_skills.hasKey(s_clsId, CLSD_SKILL, "defenseOnly") ? 1 : 0; }
	int getNoInvisibilityChange()       const { static int s_clsId = -1; return m_skills.hasKey(s_clsId, CLSD_SKILL, "noInvisibility") ? 1 : 0; }
	int getNoCaptureChange()            const { static int s_clsId = -1; return m_skills.hasKey(s_clsId, CLSD_SKILL, "noCapture") ? 1 : 0; }
	int getAnimalIgnoresBordersChange() const { static int s_clsId = -1; return m_skills.hasKey(s_clsId, CLSD_SKILL, "animalIgnoresBorders") ? 1 : 0; }
	int getNoDefensiveBonusChange()     const { static int s_clsId = -1; return m_skills.hasKey(s_clsId, CLSD_SKILL, "noDefensiveBonus") ? 1 : 0; }
	int getGatherHerdChange()           const { static int s_clsId = -1; return m_skills.hasKey(s_clsId, CLSD_SKILL, "gatherHerd") ? 1 : 0; }

	// --- computed move-through-plots gate (mirrors the archived predicate: the flat move skills + the
	// terrain/featureDoubleMove keyed lists) ---
	bool changesMoveThroughPlots() const
	{
		return isAmphib() || isCanMovePeaks() || isCanLeadThroughPeaks() || isHillsDoubleMove()
			|| !m_aiTerrainDoubleMove.empty() || !m_aiFeatureDoubleMove.empty();
	}

	// --- runtime zobrist (non-XML; a per-type random set at load, mirroring the archived ctor) ---
	inline int getZobristValue() const { return m_zobristValue; }

	// --- the property engine (self-contained; XML-era manipulator data deferred, like Corporation) ---
	const CvPropertyManipulators* getPropertyManipulators() const { return &m_PropertyManipulators; }

	// ===================== §6 MODIFIER-FAMILY SCALARS (real; address = curate_unitcombat/promotion table) =====================
	// strength.unit.* (member-less percent/flat + the named sub-members)
	int getCombatPercent() const { return m_iCombatPercent; }                             // strength.unit.percent
	int getStrengthChange() const { return m_iStrengthChange; }                           // strength.unit.flat
	int getStrengthModifier() const { return m_iStrengthModifier; }                       // strength.unit.sizeModifier.percent
	int getAttackCombatModifierChange() const { return m_iAttackCombatModifierChange; }   // strength.unit.attack.percent
	int getDefenseCombatModifierChange() const { return m_iDefenseCombatModifierChange; } // strength.unit.defense.percent
	int getVSBarbsChange() const { return m_iVSBarbsChange; }                             // strength.unit.vsBarbs.percent
	int getReligiousCombatModifierChange() const { return m_iReligiousCombatModifierChange; } // strength.unit.religious.percent
	int getStealthCombatModifierChange() const;   // GAMEOPTION_COMBAT_WITHOUT_WARNING-gated (archive mirror; .cpp)
	int getDamageModifierChange() const { return m_iDamageModifierChange; }               // strength.unit.damageModifier.percent
	int getMaxHPChange() const;                   // GAMEOPTION_COMBAT_SIZE_MATTERS-gated (archive mirror; .cpp)
	int getEnduranceChange() const { return m_iEnduranceChange; }                         // strength.unit.endurance.flat
	int getTauntChange() const { return m_iTauntChange; }                                 // strength.unit.taunt.flat
	int getBreakdownChanceChange() const { return m_iBreakdownChanceChange; }             // strength.unit.breakdownChance.flat
	int getBreakdownDamageChange() const { return m_iBreakdownDamageChange; }             // strength.unit.breakdownDamage.flat
	int getUnnerveChange() const;                 // GAMEOPTION_COMBAT_SURROUND_DESTROY-gated (archive mirror; .cpp)
	int getEncloseChange() const;                 // S&D-gated
	int getLungeChange() const;                   // S&D-gated
	int getDynamicDefenseChange() const;          // S&D-gated
	int getCombatModifierPerSizeMoreChange() const;     // GAMEOPTION_COMBAT_SIZE_MATTERS-gated
	int getCombatModifierPerSizeLessChange() const;     // SIZE_MATTERS-gated
	int getCombatModifierPerVolumeMoreChange() const;   // SIZE_MATTERS-gated
	int getCombatModifierPerVolumeLessChange() const;   // SIZE_MATTERS-gated
	int getCityAttackPercent() const { return m_iCityAttackPercent; }                     // strength.unit.cityAttack.percent
	int getCityDefensePercent() const { return m_iCityDefensePercent; }                   // strength.unit.cityDefense.percent
	int getHillsAttackPercent() const { return m_iHillsAttackPercent; }                   // strength.unit.hillsAttack.percent
	int getHillsDefensePercent() const { return m_iHillsDefensePercent; }                 // strength.unit.hillsDefense.percent
	int getKamikazePercent() const { return m_iKamikazePercent; }                         // strength.unit.kamikaze.percent
	int getCombatLimitChange() const { return m_iCombatLimitChange; }                     // strength.unit.combatLimit.flat
	int getStealthStrikesChange() const;   // GAMEOPTION_COMBAT_WITHOUT_WARNING-gated (archive mirror; .cpp)
	// withdrawal / firstStrike / bombard / collateral / air
	int getWithdrawalChange() const { return m_iWithdrawalChange; }                       // withdrawal.unit.percent
	int getFirstStrikesChange() const { return m_iFirstStrikesChange; }                   // firstStrike.unit.strikes.flat
	int getChanceFirstStrikesChange() const { return m_iChanceFirstStrikesChange; }       // firstStrike.unit.chance.flat
	int getBombardRateChange() const { return m_iBombardRateChange; }                     // bombard.unit.rate.percent
	int getCollateralDamageChange() const { return m_iCollateralDamageChange; }           // collateral.unit.damage.percent
	int getCollateralDamageLimitChange() const { return m_iCollateralDamageLimitChange; } // collateral.unit.limit.flat
	int getCollateralDamageMaxUnitsChange() const { return m_iCollateralDamageMaxUnitsChange; } // collateral.unit.maxUnits.flat
	int getCollateralDamageProtection() const { return m_iCollateralDamageProtection; }   // collateral.unit.protection.percent
	int getAirRangeChange() const { return m_iAirRangeChange; }                           // air.unit.range.flat
	int getInterceptChange() const { return m_iInterceptChange; }                         // air.unit.intercept.percent
	int getEvasionChange() const { return m_iEvasionChange; }                             // air.unit.evasion.percent
	int getAirCombatLimitChange() const { return m_iAirCombatLimitChange; }               // air.unit.combatLimit.flat
	// heal
	int getEnemyHealChange() const { return m_iEnemyHealChange; }                         // heal.unit.enemy.flat
	int getNeutralHealChange() const { return m_iNeutralHealChange; }                     // heal.unit.neutral.flat
	int getFriendlyHealChange() const { return m_iFriendlyHealChange; }                   // heal.unit.friendly.flat
	int getSameTileHealChange() const { return m_iSameTileHealChange; }                   // heal.unit.sameTile.flat
	int getAdjacentTileHealChange() const { return m_iAdjacentTileHealChange; }           // heal.unit.adjacentTile.flat
	int getSelfHealModifier() const { return m_iSelfHealModifier; }                       // heal.unit.selfModifier.percent
	int getNumHealSupport() const { return m_iNumHealSupport; }                           // heal.unit.support.flat
	int getVictoryHeal() const { return m_iVictoryHeal; }                                 // heal.unit.victory.flat
	int getVictoryAdjacentHeal() const { return m_iVictoryAdjacentHeal; }                 // heal.unit.victoryAdjacent.flat
	int getVictoryStackHeal() const { return m_iVictoryStackHeal; }                       // heal.unit.victoryStack.flat
	// movement / experience / workRate / cargo
	int getMovesChange() const { return m_iMovesChange; }                                 // movement.unit.moves.flat
	int getMoveDiscountChange() const { return m_iMoveDiscountChange; }                   // movement.unit.moveDiscount.flat
	int getExtraDropRange() const { return m_iExtraDropRange; }                           // movement.unit.dropRange.flat
	int getExperiencePercent() const { return m_iExperiencePercent; }                     // experience.unit.percent
	int getWorkRatePercent() const { return m_iWorkRatePercent; }                         // workRate.unit.rate.percent
	int getHillsWorkPercent() const { return m_iHillsWorkPercent; }                       // workRate.unit.hills.percent
	int getPeaksWorkPercent() const { return m_iPeaksWorkPercent; }                       // workRate.unit.peaks.percent
	int getCargoChange() const { return m_iCargoChange; }                                 // cargo.unit.space.flat
	int getSMCargoChange() const { return m_iSMCargoChange; }                             // cargo.unit.smSpace.flat
	int getSMCargoVolumeChange() const { return m_iSMCargoVolumeChange; }                 // cargo.unit.volume.flat
	int getSMCargoVolumeModifierChange() const { return m_iSMCargoVolumeModifierChange; } // cargo.unit.volumeModifier.percent
	// upkeep (getExtraUpkeep100 is the sole x100 re-apply: JSON holds the curator-descaled human)
	int getUpkeepModifier() const { return m_iUpkeepModifier; }                           // upkeep.unit.modifier.percent
	int getExtraUpkeep100() const { return m_iExtraUpkeep100; }                           // upkeep.unit.extra.flat (x100 re-applied)
	int getUpgradeDiscount() const { return m_iUpgradeDiscount; }                         // upkeep.unit.upgradeDiscount.percent
	// vision(range) / capture / poison / espionage / revoltProtection / pillage / survivor
	int getVisibilityChange() const { return m_iVisibilityChange; }                       // vision.range.flat
	int getCaptureProbabilityModifierChange() const { return m_iCaptureProbabilityModifierChange; } // capture.unit.probability.flat
	int getCaptureResistanceModifierChange() const { return m_iCaptureResistanceModifierChange; }   // capture.unit.resistance.flat
	int getPoisonProbabilityModifierChange() const { return m_iPoisonProbabilityModifierChange; }   // poison.unit.probability.flat
	int getInsidiousnessChange() const { return m_iInsidiousnessChange; }                 // espionage.unit.insidiousness.flat
	int getInvestigationChange() const { return m_iInvestigationChange; }                 // espionage.unit.investigation.flat
	int getRevoltProtection() const { return m_iRevoltProtection; }                       // revoltProtection.unit.percent
	int getPillageChange() const { return m_iPillageChange; }                             // pillage.unit.flat
	int getSurvivorChance() const { return m_iSurvivorChance; }                           // survivor.unit.percent
	// celebrity: the amount is DROPPED by the curator (owner) -> a boolean skills.celebrity; no scalar survives.
	int getCelebrityHappy() const { return 0; }   // curator-gap (by design): iCelebrityHappy amount dropped -> skills.celebrity

	// ===================== §6 vs-keyed struct-vector modifiers (real vectors; addresses per VS_KEYED) =====================
	int getNumTerrainAttackChangeModifiers() const { return (int)m_terrainAttack.size(); }
	const TerrainModifier& getTerrainAttackChangeModifier(int i) const { return m_terrainAttack[i]; }   // strength.unit.terrain.{T}.attack.percent
	int getNumTerrainDefenseChangeModifiers() const { return (int)m_terrainDefense.size(); }
	const TerrainModifier& getTerrainDefenseChangeModifier(int i) const { return m_terrainDefense[i]; } // strength.unit.terrain.{T}.defense.percent
	int getNumTerrainWorkChangeModifiers() const { return (int)m_terrainWork.size(); }
	const TerrainModifier& getTerrainWorkChangeModifier(int i) const { return m_terrainWork[i]; }       // workRate.unit.terrain.{T}.percent
	int getNumBuildWorkChangeModifiers() const { return (int)m_buildWork.size(); }
	const BuildModifier& getBuildWorkChangeModifier(int i) const { return m_buildWork[i]; }             // workRate.unit.build.{B}.percent
	int getNumFeatureAttackChangeModifiers() const { return (int)m_featureAttack.size(); }
	const FeatureModifier& getFeatureAttackChangeModifier(int i) const { return m_featureAttack[i]; }   // strength.unit.feature.{F}.attack.percent
	int getNumFeatureDefenseChangeModifiers() const { return (int)m_featureDefense.size(); }
	const FeatureModifier& getFeatureDefenseChangeModifier(int i) const { return m_featureDefense[i]; } // strength.unit.feature.{F}.defense.percent
	int getNumFeatureWorkChangeModifiers() const { return (int)m_featureWork.size(); }
	const FeatureModifier& getFeatureWorkChangeModifier(int i) const { return m_featureWork[i]; }       // workRate.unit.feature.{F}.percent
	int getNumUnitCombatChangeModifiers() const { return (int)m_unitCombatMod.size(); }
	const UnitCombatModifier& getUnitCombatChangeModifier(int i) const { return m_unitCombatMod[i]; }   // strength.unit.unitCombat.{UC}.percent
	int getNumFlankingStrengthbyUnitCombatTypesChange() const { return (int)m_flanking.size(); }
	const UnitCombatModifier& getFlankingStrengthbyUnitCombatTypeChange(int i) const { return m_flanking[i]; } // strength.unit.flanking.{UC}.percent
	int getNumTrapAvoidanceUnitCombatTypes() const { return (int)m_trapAvoidance.size(); }
	const UnitCombatModifier& getTrapAvoidanceUnitCombatType(int i) const { return m_trapAvoidance[i]; }         // trap.unit.avoidance.{UC}.flat

	// ===================== §6 domain modifier array (strength.unit.domain.{DOMAIN}.percent) =====================
	int getDomainModifierPercent(int i) const { return (i >= 0 && i < NUM_DOMAIN_TYPES) ? m_aiDomainModifierPercent[i] : 0; }
	bool isAnyDomainModifierPercent() const { return m_bAnyDomainModifierPercent; }

	// ===================== CvOutcome kill/action-mission system -- genuinely-deferred SYSTEM (stays XML) =====================
	const CvOutcomeList* getKillOutcomeList() const { return NULL; }
	int getNumActionOutcomes() const { return 0; }
	const CvOutcomeList* getActionOutcomeList(int /*index*/) const { return NULL; }
	MissionTypes getActionOutcomeMission(int /*index*/) const { return NO_MISSION; }
	const CvOutcomeList* getActionOutcomeListByMission(MissionTypes /*eMission*/) const { return NULL; }
	const CvOutcomeMission* getOutcomeMission(int /*index*/) const { return NULL; }
	const CvOutcomeMission* getOutcomeMissionByMission(MissionTypes /*eMission*/) const { return NULL; }

	// ===================== revoke side of grant/revoke pairs (flat bool block cannot carry a false) =====================
	bool isRemoveStampede() const { return false; }
	bool isAttackOnlyCitiesSubtract() const { return false; }
	bool isIgnoreNoEntryLevelSubtract() const { return false; }
	bool isIgnoreZoneofControlSubtract() const { return false; }
	bool isFliesToMoveSubtract() const { return false; }
	bool isSpy() const { return false; }   // curator-gap (by design): bSpy DROPPED (spy is a UNIT-level tag, owner 2026-06-23)

	// ===================== §8 keyed-skill lists (real vector<int>; curator emits `skills.<name>.{TYPE}:true`) =====================
	int getTerrainDoubleMoveChangeType(int i) const { return vecGet(m_aiTerrainDoubleMove, i); }   // skills.terrainDoubleMove.{TERRAIN}
	int getNumTerrainDoubleMoveChangeTypes() const { return (int)m_aiTerrainDoubleMove.size(); }
	bool isTerrainDoubleMoveChangeType(int i) const { return vecHas(m_aiTerrainDoubleMove, i); }
	int getFeatureDoubleMoveChangeType(int i) const { return vecGet(m_aiFeatureDoubleMove, i); }   // skills.featureDoubleMove.{FEATURE}
	int getNumFeatureDoubleMoveChangeTypes() const { return (int)m_aiFeatureDoubleMove.size(); }
	bool isFeatureDoubleMoveChangeType(int i) const { return vecHas(m_aiFeatureDoubleMove, i); }
	int getTrapImmunityUnitCombatType(int i) const { return vecGet(m_aiTrapImmunity, i); }         // skills.trapImmunity.{UC} (0 authored today)
	int getNumTrapImmunityUnitCombatTypes() const { return (int)m_aiTrapImmunity.size(); }
	bool isTrapImmunityUnitCombatType(int i) const { return vecHas(m_aiTrapImmunity, i); }

	// ===================== entity-gate game-option int-lists (real; walked out of the composed getGate() condition
	// tree in mapFrom -- enabled = required-ON options, disabled = suppress-if-ON options) =====================
	int getOnGameOption(int i) const { return vecGet(m_aiOnGameOptions, i); }
	int getNumOnGameOptions() const { return (int)m_aiOnGameOptions.size(); }
	bool isOnGameOption(int i) const { return vecHas(m_aiOnGameOptions, i); }
	int getNotOnGameOption(int i) const { return vecGet(m_aiNotOnGameOptions, i); }
	int getNumNotOnGameOptions() const { return (int)m_aiNotOnGameOptions.size(); }
	bool isNotOnGameOption(int i) const { return vecHas(m_aiNotOnGameOptions, i); }

	// ===================== parked identity lists (real vector<int>; curator emits identity.ggPointsForUnits /
	// identity.defaultStatuses as FK string arrays). Categories are DROPPED (dead) -> curator-gap. =====================
	int getGGptsforUnitType(int i) const { return vecGet(m_aiGGptsforUnitTypes, i); }              // identity.ggPointsForUnits.[UNIT]
	int getNumGGptsforUnitTypes() const { return (int)m_aiGGptsforUnitTypes.size(); }
	bool isGGptsforUnitType(int i) const { return vecHas(m_aiGGptsforUnitTypes, i); }
	int getDefaultStatusType(int i) const { return vecGet(m_aiDefaultStatusTypes, i); }            // identity.defaultStatuses.[PROMOTION]
	int getNumDefaultStatusTypes() const { return (int)m_aiDefaultStatusTypes.size(); }
	bool isDefaultStatusType(int i) const { return vecHas(m_aiDefaultStatusTypes, i); }
	int getCategory(int /*i*/) const { return -1; }       // curator-gap (by design): Categories DROPPED (dead)
	int getNumCategories() const { return 0; }
	bool isCategory(int /*i*/) const { return false; }

	// ===================== §7 vision/LOS-resolver intensity pair-maps (real std::map keyed by INVISIBLE id;
	// curator emits `vision.<name>.{INVISIBLE}: intensity`) =====================
	int getNumVisibilityIntensityChangeTypes() const { return (int)m_aiVisibilityIntensity.size(); }
	int getVisibilityIntensityChangeType(int iInvisibility) const { return mapGet(m_aiVisibilityIntensity, iInvisibility); }
	bool isVisibilityIntensityChangeType(int iInvisibility) const { return m_aiVisibilityIntensity.count(iInvisibility) != 0; }
	int getNumInvisibilityIntensityChangeTypes() const { return (int)m_aiInvisibilityIntensity.size(); }
	int getInvisibilityIntensityChangeType(int iInvisibility) const { return mapGet(m_aiInvisibilityIntensity, iInvisibility); }
	bool isInvisibilityIntensityChangeType(int iInvisibility) const { return m_aiInvisibilityIntensity.count(iInvisibility) != 0; }
	int getNumVisibilityIntensityRangeChangeTypes() const { return (int)m_aiVisibilityIntensityRange.size(); }
	int getVisibilityIntensityRangeChangeType(int iInvisibility) const { return mapGet(m_aiVisibilityIntensityRange, iInvisibility); }
	bool isVisibilityIntensityRangeChangeType(int iInvisibility) const { return m_aiVisibilityIntensityRange.count(iInvisibility) != 0; }
	int getNumVisibilityIntensitySameTileChangeTypes() const { return (int)m_aiVisibilityIntensitySameTile.size(); }
	int getVisibilityIntensitySameTileChangeType(int iInvisibility) const { return mapGet(m_aiVisibilityIntensitySameTile, iInvisibility); }
	bool isVisibilityIntensitySameTileChangeType(int iInvisibility) const { return m_aiVisibilityIntensitySameTile.count(iInvisibility) != 0; }

	// ===================== §7 vision Invisible/Visible struct-row vectors (real; curator emits `vision.<name>` row
	// lists of {invisible, terrain|feature|improvement, intensity}) =====================
	int getNumInvisibleTerrainChanges() const { return (int)m_aInvisibleTerrainChanges.size(); }
	const InvisibleTerrainChanges& getInvisibleTerrainChange(int i) const { return m_aInvisibleTerrainChanges[i]; }
	int getNumInvisibleFeatureChanges() const { return (int)m_aInvisibleFeatureChanges.size(); }
	const InvisibleFeatureChanges& getInvisibleFeatureChange(int i) const { return m_aInvisibleFeatureChanges[i]; }
	int getNumInvisibleImprovementChanges() const { return (int)m_aInvisibleImprovementChanges.size(); }
	const InvisibleImprovementChanges& getInvisibleImprovementChange(int i) const { return m_aInvisibleImprovementChanges[i]; }
	int getNumVisibleTerrainChanges() const { return (int)m_aVisibleTerrainChanges.size(); }
	const InvisibleTerrainChanges& getVisibleTerrainChange(int i) const { return m_aVisibleTerrainChanges[i]; }
	int getNumVisibleFeatureChanges() const { return (int)m_aVisibleFeatureChanges.size(); }
	const InvisibleFeatureChanges& getVisibleFeatureChange(int i) const { return m_aVisibleFeatureChanges[i]; }
	int getNumVisibleImprovementChanges() const { return (int)m_aVisibleImprovementChanges.size(); }
	const InvisibleImprovementChanges& getVisibleImprovementChange(int i) const { return m_aVisibleImprovementChanges[i]; }
	int getNumVisibleTerrainRangeChanges() const { return (int)m_aVisibleTerrainRangeChanges.size(); }
	const InvisibleTerrainChanges& getVisibleTerrainRangeChange(int i) const { return m_aVisibleTerrainRangeChanges[i]; }
	int getNumVisibleFeatureRangeChanges() const { return (int)m_aVisibleFeatureRangeChanges.size(); }
	const InvisibleFeatureChanges& getVisibleFeatureRangeChange(int i) const { return m_aVisibleFeatureRangeChanges[i]; }
	int getNumVisibleImprovementRangeChanges() const { return (int)m_aVisibleImprovementRangeChanges.size(); }
	const InvisibleImprovementChanges& getVisibleImprovementRangeChange(int i) const { return m_aVisibleImprovementRangeChanges[i]; }

	// --- the composed section units (by value; the base's mapFrom dispatch writes them via mut*) ---
	virtual const CvJsonModifiers* getModifiers() const { return &m_modifiers; }
	virtual const CvJsonBoolBlock* getSkills()    const { return &m_skills; }
	virtual const CvJsonGate*      getGate()      const { return &m_gate; }

protected:
	virtual CvJsonModifiers* mutModifiers() { return &m_modifiers; }
	virtual CvJsonBoolBlock* mutSkills()    { return &m_skills; }
	virtual CvJsonGate*      mutGate()      { return &m_gate; }

private:
	// small readers for the FK-list / intensity-map getters (mirrors the Promotion poco)
	static int mapGet(const std::map<int, int>& m, int k) { std::map<int, int>::const_iterator it = m.find(k); return it != m.end() ? it->second : 0; }
	static int vecGet(const std::vector<int>& v, int i) { return (i >= 0 && i < (int)v.size()) ? v[i] : -1; }
	static bool vecHas(const std::vector<int>& v, int id) { for (size_t j = 0; j < v.size(); ++j) if (v[j] == id) return true; return false; }

	CvJsonModifiers m_modifiers;
	CvJsonBoolBlock m_skills;
	CvJsonGate      m_gate;

	// identity scalars
	ReligionTypes m_eReligion;
	BonusTypes    m_eCulture;
	EraTypes      m_eEra;
	int m_iQualityBase, m_iGroupBase, m_iSizeBase;   // identity.base.* (-10 sentinel default)
	int m_iRBombardDamageBase, m_iRBombardDamageLimitBase, m_iRBombardDamageMaxUnitsBase;
	int m_iDCMBombRangeBase, m_iDCMBombAccuracyBase;  // identity.base.* (0 default)
	bool m_bForMilitary, m_bForNavalMilitary;
	int m_zobristValue;                              // runtime random (set in mapFrom)

	// §6 modifier-family scalars (all 0 default; set in mapFrom from the curator address)
	int m_iCombatPercent, m_iStrengthChange, m_iStrengthModifier, m_iAttackCombatModifierChange, m_iDefenseCombatModifierChange;
	int m_iVSBarbsChange, m_iReligiousCombatModifierChange, m_iStealthCombatModifierChange, m_iDamageModifierChange;
	int m_iMaxHPChange, m_iEnduranceChange, m_iTauntChange, m_iBreakdownChanceChange, m_iBreakdownDamageChange;
	int m_iUnnerveChange, m_iEncloseChange, m_iLungeChange, m_iDynamicDefenseChange;
	int m_iCombatModifierPerSizeMoreChange, m_iCombatModifierPerSizeLessChange, m_iCombatModifierPerVolumeMoreChange, m_iCombatModifierPerVolumeLessChange;
	int m_iCityAttackPercent, m_iCityDefensePercent, m_iHillsAttackPercent, m_iHillsDefensePercent, m_iKamikazePercent, m_iCombatLimitChange, m_iStealthStrikesChange;
	int m_iWithdrawalChange, m_iFirstStrikesChange, m_iChanceFirstStrikesChange, m_iBombardRateChange;
	int m_iCollateralDamageChange, m_iCollateralDamageLimitChange, m_iCollateralDamageMaxUnitsChange, m_iCollateralDamageProtection;
	int m_iAirRangeChange, m_iInterceptChange, m_iEvasionChange, m_iAirCombatLimitChange;
	int m_iEnemyHealChange, m_iNeutralHealChange, m_iFriendlyHealChange, m_iSameTileHealChange, m_iAdjacentTileHealChange;
	int m_iSelfHealModifier, m_iNumHealSupport, m_iVictoryHeal, m_iVictoryAdjacentHeal, m_iVictoryStackHeal;
	int m_iMovesChange, m_iMoveDiscountChange, m_iExtraDropRange, m_iExperiencePercent;
	int m_iWorkRatePercent, m_iHillsWorkPercent, m_iPeaksWorkPercent;
	int m_iCargoChange, m_iSMCargoChange, m_iSMCargoVolumeChange, m_iSMCargoVolumeModifierChange;
	int m_iUpkeepModifier, m_iExtraUpkeep100, m_iUpgradeDiscount;
	int m_iVisibilityChange, m_iCaptureProbabilityModifierChange, m_iCaptureResistanceModifierChange, m_iPoisonProbabilityModifierChange;
	int m_iInsidiousnessChange, m_iInvestigationChange, m_iRevoltProtection, m_iPillageChange, m_iSurvivorChance;

	// §6 vs-keyed struct-vectors + domain array
	std::vector<TerrainModifier>    m_terrainAttack, m_terrainDefense, m_terrainWork;
	std::vector<FeatureModifier>    m_featureAttack, m_featureDefense, m_featureWork;
	std::vector<BuildModifier>      m_buildWork;
	std::vector<UnitCombatModifier> m_unitCombatMod, m_flanking, m_trapAvoidance;
	int m_aiDomainModifierPercent[NUM_DOMAIN_TYPES];
	bool m_bAnyDomainModifierPercent;

	// §8 keyed-skill FK lists / entity-gate game-option lists / parked identity FK lists
	std::vector<int> m_aiTerrainDoubleMove, m_aiFeatureDoubleMove, m_aiTrapImmunity;
	std::vector<int> m_aiOnGameOptions, m_aiNotOnGameOptions;
	std::vector<int> m_aiGGptsforUnitTypes, m_aiDefaultStatusTypes;

	// §7 vision/LOS resolver: intensity pair-maps (keyed by INVISIBLE id) + the invisible/visible struct-row vectors
	std::map<int, int> m_aiVisibilityIntensity, m_aiInvisibilityIntensity, m_aiVisibilityIntensityRange, m_aiVisibilityIntensitySameTile;
	std::vector<InvisibleTerrainChanges>     m_aInvisibleTerrainChanges, m_aVisibleTerrainChanges, m_aVisibleTerrainRangeChanges;
	std::vector<InvisibleFeatureChanges>     m_aInvisibleFeatureChanges, m_aVisibleFeatureChanges, m_aVisibleFeatureRangeChanges;
	std::vector<InvisibleImprovementChanges> m_aInvisibleImprovementChanges, m_aVisibleImprovementChanges, m_aVisibleImprovementRangeChanges;

	CvPropertyManipulators m_PropertyManipulators;   // property engine, XML-era manipulator data deferred
};

#endif // CV_JSON_UNITCOMBAT_INFO_H
