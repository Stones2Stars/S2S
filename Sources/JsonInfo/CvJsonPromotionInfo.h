#pragma once
#ifndef CV_JSON_PROMOTION_INFO_H
#define CV_JSON_PROMOTION_INFO_H

//
//	CvJsonPromotionInfo -- the per-type cascade info for PROMOTIONS. Composes the section units a promotion authors
//	(modifier families / the section-8 `skills` bool block / the entity-level gate / `grants`); the remaining
//	consumer surface below MIRRORS the archived `CvPromotionInfo` (SourceArchive/Infos/CvPromotionInfo.h)
//	getter-for-getter. A promotion is a grantor of unit skills; a unit's ACTIVE skill set is its type's base skills +
//	the skills of its held promotions, resolved on the unit INSTANCE later -- this static info defines what THIS
//	promotion contributes.
//
//	GROUND TRUTH for the family/member vocabulary is curate_promotion.py: `strength` is THE combat family (its
//	`percent` = general combat %, plus named members cityAttack/cityDefense/hills*/quality/group/attack/defense/
//	vsBarbs/... and vs-keyed maps terrain/feature/unitCombat/domain/flanking); `withdrawal`/`firstStrike`/`bombard`/
//	`collateral`/`air`/`heal`/`movement`/`experience`/`workRate`/`cargo`/`upkeep`/`capture`/`poison`/`espionage`/
//	`revoltProtection`/`pillage`/`survivor`/`vision` are their own families; the section-8 boolean SKILLS (blitz/
//	amphib/..., the grant/revoke pairs, the count-abilities, the double-move keyed lists) live in the `skills` block;
//	`identity` carries the parked availability gates (unitCombats/prereq*/notOn*/negatesInvisibility/cargo FKs/era/
//	command/...) and flags (leader/status/quick/starsign/...); `grants` carries builds/freeToUnitCombats/specialUnit;
//	`vision` carries the LOS resolver (intensity pair-maps + the invisible/visible struct rows); `promotionLine` is
//	the top-level {LINE: rank} object; `ai.unitCombatWeights` the AI weights; the entity-level `enabled`/`disabled`
//	gate carries the OnGameOptions/NotOnGameOptions applicability.
//
//	NAMED GAPS (fields the curator genuinely does NOT emit onto the promotion, verified against curate_promotion.py +
//	Assets/Data/promotions/*.json -- NOT silent placeholders):
//	  - getTechPrereq / getObsoleteTech : store-inverted onto the TECH (tech.enables.promotions /
//	    tech.obsoletes.promotions). No promotion JSON carries a tech-prereq/obsoletes block; the reverse lookup over
//	    every tech is not reconstructable from this poco. Deferred to the enabling reconstruction.
//	  - getPrereqPromotion / getPrereqOrPromotion1 / getPrereqOrPromotion2 : curator DROP (owner ruling: the chain is
//	    carried by PromotionLine + iLinePriority + the tech, so the promotion-on-promotion prereq is dropped).
//	  - getCelebrityHappy : the numeric amount is DROPPED (owner: "not a random field on a unit"); only the boolean
//	    skills.celebrity survives, so the int amount does not exist in the JSON.
//	  - getZobristValue : a runtime state-hash the archived class seeds from GC.getGame().getSorenRand() in its
//	    constructor -- not JSON, not curated. Left 0 to avoid a GC.getGame() dependency at load-time poco construction.
//	  - trap subsystem / Categories / the BATTLEWORN trio (getDamageperTurn/getStrAdjperTurn/getWeakenperTurn) :
//	    curator DROP (dead systems -- traps removed, categories dead, battleworn nuked by the owner).
//	HOTKEY getters are inherited from CvHotkeyInfo and NOT redeclared here.
//

#include "CvJsonInfo.h"
#include "Defines/CvEnums.h"     // Tech/Promotion/PromotionLine/UnitCombat/Domain/SpecialUnit/MapCategory Types + NO_*
#include "Defines/CvStructs.h"   // UnitCombatModifier / HealUnitCombat / Invisible{Terrain,Feature,Improvement}Changes
#include <map>
#include <set>
#include <string>
#include <vector>

class CvJsonPromotionInfo : public CvJsonInfo
{
public:
	CvJsonPromotionInfo();
	virtual void mapFrom(const picojson::value& entity);

	// --- the composed section units (by value; the base's mapFrom dispatch writes them via mut*) ---
	virtual const CvJsonModifiers* getModifiers() const { return &m_modifiers; }
	virtual const CvJsonBoolBlock* getSkills()    const { return &m_skills; }
	virtual const CvJsonGate*      getGate()      const { return &m_gate; }
	virtual const CvJsonGrants*    getGrants()    const { return &m_grants; }

	// =========================================================================================================
	//  strength family (general combat % + named members)
	// =========================================================================================================
	int getCombatPercent() const { return m_iCombatPercent; }
	int getStrengthChange() const { return m_iStrengthChange; }
	int getStrengthModifier() const { return m_iStrengthModifier; }
	int getAttackCombatModifierChange() const { return m_iAttackCombatModifierChange; }
	int getDefenseCombatModifierChange() const { return m_iDefenseCombatModifierChange; }
	int getVSBarbsChange() const { return m_iVSBarbsChange; }
	int getReligiousCombatModifierChange() const { return m_iReligiousCombatModifierChange; }
	int getStealthCombatModifierChange() const { return m_iStealthCombatModifierChange; }  // value real; legacy also runtime-gates on GAMEOPTION_COMBAT_WITHOUT_WARNING (consumer/deferred)
	int getDamageModifierChange() const { return m_iDamageModifierChange; }
	int getMaxHPChange() const { return m_iMaxHPChange; }
	int getEnduranceChange() const { return m_iEnduranceChange; }
	int getTauntChange() const { return m_iTauntChange; }
	int getBreakdownChanceChange() const { return m_iBreakdownChanceChange; }
	int getBreakdownDamageChange() const { return m_iBreakdownDamageChange; }
	int getUnnerveChange() const { return m_iUnnerveChange; }                 // value real; legacy runtime-gates on GAMEOPTION_COMBAT_SURROUND_DESTROY (consumer/deferred)
	int getEncloseChange() const { return m_iEncloseChange; }                 // value real; S&D runtime gate deferred
	int getLungeChange() const { return m_iLungeChange; }                     // value real; S&D runtime gate deferred
	int getDynamicDefenseChange() const { return m_iDynamicDefenseChange; }   // value real; S&D runtime gate deferred
	int getCombatModifierPerSizeMoreChange() const { return m_iCombatModifierPerSizeMoreChange; }     // value real; SIZE_MATTERS runtime gate deferred
	int getCombatModifierPerSizeLessChange() const { return m_iCombatModifierPerSizeLessChange; }     // value real; SIZE_MATTERS runtime gate deferred
	int getCombatModifierPerVolumeMoreChange() const { return m_iCombatModifierPerVolumeMoreChange; } // value real; SIZE_MATTERS runtime gate deferred
	int getCombatModifierPerVolumeLessChange() const { return m_iCombatModifierPerVolumeLessChange; } // value real; SIZE_MATTERS runtime gate deferred
	int getCityAttackPercent() const { return m_iCityAttackPercent; }
	int getCityDefensePercent() const { return m_iCityDefensePercent; }
	int getHillsAttackPercent() const { return m_iHillsAttackPercent; }
	int getHillsDefensePercent() const { return m_iHillsDefensePercent; }
	int getKamikazePercent() const { return m_iKamikazePercent; }
	int getCombatLimitChange() const { return m_iCombatLimitChange; }
	int getStealthStrikesChange() const { return m_iStealthStrikesChange; }   // value real; COMBAT_WITHOUT_WARNING runtime gate deferred
	int getQualityChange() const { return m_iQualityChange; }
	int getGroupChange() const { return m_iGroupChange; }

	// vs-keyed strength maps (percent by target id)
	int getTerrainAttackPercent(int i) const { return mapGet(m_aiTerrainAttackPercent, i); }
	bool isAnyTerrainAttackPercent() const { return !m_aiTerrainAttackPercent.empty(); }
	int getTerrainDefensePercent(int i) const { return mapGet(m_aiTerrainDefensePercent, i); }
	bool isAnyTerrainDefensePercent() const { return !m_aiTerrainDefensePercent.empty(); }
	int getFeatureAttackPercent(int i) const { return mapGet(m_aiFeatureAttackPercent, i); }
	bool isAnyFeatureAttackPercent() const { return !m_aiFeatureAttackPercent.empty(); }
	int getFeatureDefensePercent(int i) const { return mapGet(m_aiFeatureDefensePercent, i); }
	bool isAnyFeatureDefensePercent() const { return !m_aiFeatureDefensePercent.empty(); }
	int getUnitCombatModifierPercent(int i) const { return mapGet(m_aiUnitCombatModifierPercent, i); }
	bool isAnyUnitCombatModifierPercent() const { return !m_aiUnitCombatModifierPercent.empty(); }
	int getDomainModifierPercent(int i) const { return mapGet(m_aiDomainModifierPercent, i); }
	bool isAnyDomainModifierPercent() const { return !m_aiDomainModifierPercent.empty(); }
	int getNumFlankingStrikesbyUnitCombatTypesChange() const { return (int)m_aiFlanking.size(); }
	int getFlankingStrengthbyUnitCombatTypeChange(int iUnitCombat) const { return mapGet(m_aiFlanking, iUnitCombat); }
	bool isFlankingStrikebyUnitCombatTypeChange(int iUnitCombat) const { return m_aiFlanking.count(iUnitCombat) != 0; }

	// =========================================================================================================
	//  other own-families (withdrawal/firstStrike/bombard/collateral/air/heal/movement/...)
	// =========================================================================================================
	int getWithdrawalChange() const { return m_iWithdrawalChange; }
	int getFirstStrikesChange() const { return m_iFirstStrikesChange; }
	int getChanceFirstStrikesChange() const { return m_iChanceFirstStrikesChange; }
	int getBombardRateChange() const { return m_iBombardRateChange; }
	int getRBombardDamageChange() const { return m_iRBombardDamageChange; }
	int getRBombardDamageLimitChange() const { return m_iRBombardDamageLimitChange; }
	int getRBombardDamageMaxUnitsChange() const { return m_iRBombardDamageMaxUnitsChange; }
	int getDCMBombRangeChange() const { return m_iDCMBombRangeChange; }
	int getDCMBombAccuracyChange() const { return m_iDCMBombAccuracyChange; }
	int getCollateralDamageChange() const { return m_iCollateralDamageChange; }
	int getCollateralDamageLimitChange() const { return m_iCollateralDamageLimitChange; }
	int getCollateralDamageMaxUnitsChange() const { return m_iCollateralDamageMaxUnitsChange; }
	int getCollateralDamageProtection() const { return m_iCollateralDamageProtection; }
	int getAirRangeChange() const { return m_iAirRangeChange; }
	int getInterceptChange() const { return m_iInterceptChange; }
	int getEvasionChange() const { return m_iEvasionChange; }
	int getAirCombatLimitChange() const { return m_iAirCombatLimitChange; }
	int getEnemyHealChange() const { return m_iEnemyHealChange; }
	int getNeutralHealChange() const { return m_iNeutralHealChange; }
	int getFriendlyHealChange() const { return m_iFriendlyHealChange; }
	int getSameTileHealChange() const { return m_iSameTileHealChange; }
	int getAdjacentTileHealChange() const { return m_iAdjacentTileHealChange; }
	int getSelfHealModifier() const { return m_iSelfHealModifier; }
	int getNumHealSupport() const { return m_iNumHealSupport; }
	int getVictoryHeal() const { return m_iVictoryHeal; }
	int getVictoryAdjacentHeal() const { return m_iVictoryAdjacentHeal; }
	int getVictoryStackHeal() const { return m_iVictoryStackHeal; }
	int getMovesChange() const { return m_iMovesChange; }
	int getMoveDiscountChange() const { return m_iMoveDiscountChange; }
	int getExtraDropRange() const { return m_iExtraDropRange; }
	int getExperiencePercent() const { return m_iExperiencePercent; }
	int getWorkRatePercent() const { return m_iWorkRatePercent; }
	int getHillsWorkPercent() const { return m_iHillsWorkPercent; }
	int getPeaksWorkPercent() const { return m_iPeaksWorkPercent; }
	int getCargoChange() const { return m_iCargoChange; }
	int getSMCargoChange() const { return m_iSMCargoChange; }
	int getSMCargoVolumeChange() const { return m_iSMCargoVolumeChange; }
	int getSMCargoVolumeModifierChange() const { return m_iSMCargoVolumeModifierChange; }
	int getUpkeepModifier() const { return m_iUpkeepModifier; }
	int getExtraUpkeep100() const { return m_iExtraUpkeep100; }               // upkeep.unit.extra.flat, re-scaled x100 (legacy accessor is x100)
	int getUpgradeDiscount() const { return m_iUpgradeDiscount; }
	int getVisibilityChange() const { return m_iVisibilityChange; }          // vision.range.flat
	int getCaptureProbabilityModifierChange() const { return m_iCaptureProbabilityModifierChange; }
	int getCaptureResistanceModifierChange() const { return m_iCaptureResistanceModifierChange; }
	int getPoisonProbabilityModifierChange() const { return m_iPoisonProbabilityModifierChange; }
	int getInsidiousnessChange() const { return m_iInsidiousnessChange; }
	int getInvestigationChange() const { return m_iInvestigationChange; }
	int getRevoltProtection() const { return m_iRevoltProtection; }
	int getPillageChange() const { return m_iPillageChange; }
	int getSurvivorChance() const { return m_iSurvivorChance; }

	// workRate vs-keyed maps
	int getTerrainWorkPercent(int i) const { return mapGet(m_aiTerrainWorkPercent, i); }
	int getFeatureWorkPercent(int i) const { return mapGet(m_aiFeatureWorkPercent, i); }
	int getNumBuildWorkRateModifierChangeTypes() const { return (int)m_aiBuildWorkRate.size(); }
	int getBuildWorkRateModifierChangeType(int iBuild) const { return mapGet(m_aiBuildWorkRate, iBuild); }
	bool isBuildWorkRateModifierChangeType(int iBuild) const { return m_aiBuildWorkRate.count(iBuild) != 0; }

	// heal.unit.unitCombat struct rows
	int getNumHealUnitCombatChangeTypes() const { return (int)m_aHealUnitCombat.size(); }
	const HealUnitCombat& getHealUnitCombatChangeType(int iIndex) const { return m_aHealUnitCombat[iIndex]; }

	// ai.unitCombatWeights struct rows
	int getNumAIWeightbyUnitCombatTypes() const { return (int)m_aAIWeight.size(); }
	const UnitCombatModifier& getAIWeightbyUnitCombatType(int iIndex) const { return m_aAIWeight[iIndex]; }

	// =========================================================================================================
	//  section-8 boolean SKILLS block
	// =========================================================================================================
	bool isBlitz() const { return m_skills.has("blitz"); }
	bool isAmphib() const { return m_skills.has("amphib"); }
	bool isRiver() const { return m_skills.has("river"); }
	bool isEnemyRoute() const { return m_skills.has("enemyRoute"); }
	bool isAlwaysHeal() const { return m_skills.has("alwaysHeal"); }
	bool isHillsDoubleMove() const { return m_skills.has("hillsDoubleMove"); }
	bool isImmuneToFirstStrikes() const { return m_skills.has("immuneToFirstStrikes"); }
	bool isDefensiveVictoryMove() const { return m_skills.has("defensiveVictoryMove"); }
	bool isFreeDrop() const { return m_skills.has("freeDrop"); }
	bool isOffensiveVictoryMove() const { return m_skills.has("offensiveVictoryMove"); }
	bool isOneUp() const { return m_skills.has("oneUp"); }
	bool isPillageEspionage() const { return m_skills.has("pillageEspionage"); }
	bool isPillageMarauder() const { return m_skills.has("pillageMarauder"); }
	bool isPillageOnMove() const { return m_skills.has("pillageOnMove"); }
	bool isPillageOnVictory() const { return m_skills.has("pillageOnVictory"); }
	bool isPillageResearch() const { return m_skills.has("pillageResearch"); }
	bool isCanMovePeaks() const { return m_skills.has("canPassPeaks"); }        // dual-plane rename (capabilities.md)
	bool isCanLeadThroughPeaks() const { return m_skills.has("canLeadThroughPeaks"); }
	bool isZoneOfControl() const { return m_skills.has("zoneOfControl"); }
	bool isOnslaughtChange() const { return m_skills.has("onslaught"); }
	bool isParalyze() const { return m_skills.has("paralyze"); }
	bool isNoSelfHeal() const { return m_skills.has("noSelfHeal"); }

	// grant/revoke skill PAIRS -- tri-state (the section-8 bool block drops false, so these read the raw skills object)
	bool isStampedeChange() const { return skillTrue("stampede"); }
	bool isRemoveStampede() const { return skillFalse("stampede"); }
	bool isAttackOnlyCitiesAdd() const { return skillTrue("attackOnlyCities"); }
	bool isAttackOnlyCitiesSubtract() const { return skillFalse("attackOnlyCities"); }
	bool isIgnoreNoEntryLevelAdd() const { return skillTrue("ignoreNoEntryLevel"); }
	bool isIgnoreNoEntryLevelSubtract() const { return skillFalse("ignoreNoEntryLevel"); }
	bool isIgnoreZoneofControlAdd() const { return skillTrue("ignoreZoneofControl"); }
	bool isIgnoreZoneofControlSubtract() const { return skillFalse("ignoreZoneofControl"); }
	bool isFliesToMoveAdd() const { return skillTrue("fliesToMove"); }
	bool isFliesToMoveSubtract() const { return skillFalse("fliesToMove"); }

	// count-ability skills -- legacy returns int; curator collapsed the magnitude to a SIGN-aware bool, so these
	// reconstruct +1 (grant) / -1 (revoke) / 0 (absent). Consumers test >0 / !=0 (semantics preserved).
	int getExcileChange() const { return skillCount("excile"); }
	int getPassageChange() const { return skillCount("passage"); }
	int getNoNonOwnedCityEntryChange() const { return skillCount("noNonOwnedCityEntry"); }
	int getBarbCoExistChange() const { return skillCount("barbCoExist"); }
	int getBlendIntoCityChange() const { return skillCount("blendIntoCity"); }
	int getUpgradeAnywhereChange() const { return skillCount("upgradeAnywhere"); }
	int getHiddenNationalityChange() const { return skillCount("hiddenNationality"); }
	int getAssassinChange() const { return skillCount("assassin"); }
	int getStealthDefenseChange() const { return skillCount("stealthDefense"); }
	int getDefenseOnlyChange() const { return skillCount("defenseOnly"); }
	int getNoInvisibilityChange() const { return skillCount("noInvisibility"); }
	int getNoDefensiveBonusChange() const { return skillCount("noDefensiveBonus"); }
	int getGatherHerdChange() const { return skillCount("gatherHerd"); }
	int getAnimalIgnoresBordersChange() const { return skillCount("animalIgnoresBorders"); }

	// double-move keyed skills
	bool getTerrainDoubleMove(int i) const { return m_aiTerrainDoubleMove.count(i) != 0; }
	bool getFeatureDoubleMove(int i) const { return m_aiFeatureDoubleMove.count(i) != 0; }

	// unitcombat membership the promotion mutates (skills lists)
	int getSubCombatChangeType(int i) const { return vecGet(m_aiSubCombat, i); }
	int getNumSubCombatChangeTypes() const { return (int)m_aiSubCombat.size(); }
	bool isSubCombatChangeType(int i) const { return vecHas(m_aiSubCombat, i); }
	int getRemovesUnitCombatType(int i) const { return vecGet(m_aiRemoves, i); }
	int getNumRemovesUnitCombatTypes() const { return (int)m_aiRemoves.size(); }
	bool isRemovesUnitCombatType(int i) const { return vecHas(m_aiRemoves, i); }

	bool changesMoveThroughPlots() const
	{
		return isAmphib() || isCanMovePeaks() || isCanLeadThroughPeaks() || isHillsDoubleMove()
			|| !m_aiTerrainDoubleMove.empty() || !m_aiFeatureDoubleMove.empty();
	}

	// =========================================================================================================
	//  grants
	// =========================================================================================================
	int getAddsBuildType(int i) const { const std::vector<int>* l = grantList("builds"); return (l && i >= 0 && i < (int)l->size()) ? (*l)[i] : -1; }
	int getNumAddsBuildTypes() const { const std::vector<int>* l = grantList("builds"); return l ? (int)l->size() : 0; }
	bool isAddsBuildType(int i) const { const std::vector<int>* l = grantList("builds"); return l && vecHas(*l, i); }
	int getFreetoUnitCombat(int i) const { const std::vector<int>* l = grantList("freeToUnitCombats"); return (l && i >= 0 && i < (int)l->size()) ? (*l)[i] : -1; }
	int getNumFreetoUnitCombats() const { const std::vector<int>* l = grantList("freeToUnitCombats"); return l ? (int)l->size() : 0; }
	bool isFreetoUnitCombat(int i) const { const std::vector<int>* l = grantList("freeToUnitCombats"); return l && vecHas(*l, i); }
	SpecialUnitTypes setSpecialUnit() const { return static_cast<SpecialUnitTypes>(m_grants.firstListId("specialUnit")); }

	// =========================================================================================================
	//  identity (parked availability gates + flags + text/FK references)
	// =========================================================================================================
	int getLayerAnimationPath() const { return m_iLayerAnimationPath; }
	int getMinEraType() const { return m_iMinEraType; }
	int getMaxEraType() const { return m_iMaxEraType; }
	int getStateReligionPrereq() const { return m_iStateReligionPrereq; }
	int getControlPoints() const { return m_iControlPoints; }
	int getCommandRange() const { return m_iCommandRange; }
	int getLevelPrereq() const { return m_iLevelPrereq; }
	int getLinePriority() const { return m_iLinePriority; }
	int getCommandType() const { return m_iCommandType; }                     // identity.commandType (default NO_COMMAND; runtime setter may override)
	void setCommandType(int iNewType) { m_iCommandType = iNewType; }
	PromotionLineTypes getPromotionLine() const { return m_ePromotionLine; }
	const wchar_t* getRenamesUnitTo() const { return m_szRenamesUnitTo.c_str(); }
	UnitCombatTypes getReplacesUnitCombat() const { return static_cast<UnitCombatTypes>(m_iReplacesUnitCombat); }
	DomainTypes getDomainCargoChange() const { return static_cast<DomainTypes>(m_iDomainCargoChange); }
	SpecialUnitTypes getSpecialCargoChange() const { return static_cast<SpecialUnitTypes>(m_iSpecialCargoChange); }
	SpecialUnitTypes getSpecialCargoPrereq() const { return static_cast<SpecialUnitTypes>(m_iSpecialCargoPrereq); }
	SpecialUnitTypes getSMNotSpecialCargoChange() const { return static_cast<SpecialUnitTypes>(m_iSMNotSpecialCargoChange); }
	SpecialUnitTypes getSMNotSpecialCargoPrereq() const { return static_cast<SpecialUnitTypes>(m_iSMNotSpecialCargoPrereq); }

	bool isLeader() const { return m_bLeader; }
	bool isStatus() const { return m_bStatus; }
	bool isQuick() const { return m_bQuick; }
	bool isStarsign() const { return m_bStarsign; }
	bool isZeroesXP() const { return m_bZeroesXP; }
	bool isForOffset() const { return m_bForOffset; }
	bool isCargoPrereq() const { return m_bCargoPrereq; }
	bool isRBombardPrereq() const { return m_bRBombardPrereq; }
	bool isSetOnHNCapture() const { return m_bSetOnHNCapture; }
	bool isSetOnInvestigated() const { return m_bSetOnInvestigated; }
	bool isPrereqNormInvisible() const { return m_bPrereqNormInvisible; }
	bool isPlotPrereqsKeepAfter() const { return m_bPlotPrereqsKeepAfter; }
	bool isRemoveAfterSet() const { return m_bRemoveAfterSet; }

	bool getUnitCombat(int i) const { return vecHas(m_aeUnitCombat, i); }   // identity.unitCombats membership

	// pedia-derived, computed post-load exactly as the archived doPostLoadCaching did: from this promotion's own
	// unitCombats / notOnUnitCombats PLUS the promotion line's unitcombat prereqs / notOnUnitCombats
	// (curate_promotionline.py -> identity.unitCombats / identity.notOnUnitCombats, read via
	// CvJsonPromotionLineInfo). The engine calls the setters post-load; the getters then read the computed vectors.
	int getQualifiedUnitCombatType(int i) const { return vecGet(m_aiQualifiedUnitCombat, i); }
	int getNumQualifiedUnitCombatTypes() const { return (int)m_aiQualifiedUnitCombat.size(); }
	bool isQualifiedUnitCombatType(int i) const { return vecHas(m_aiQualifiedUnitCombat, i); }
	void setQualifiedUnitCombatTypes();
	int getDisqualifiedUnitCombatType(int i) const { return vecGet(m_aiDisqualifiedUnitCombat, i); }
	int getNumDisqualifiedUnitCombatTypes() const { return (int)m_aiDisqualifiedUnitCombat.size(); }
	void setDisqualifiedUnitCombatTypes();

	int getNotOnUnitCombatType(int i) const { return vecGet(m_aiNotOnUnitCombats, i); }
	int getNumNotOnUnitCombatTypes() const { return (int)m_aiNotOnUnitCombats.size(); }
	bool isNotOnUnitCombatType(int i) const { return vecHas(m_aiNotOnUnitCombats, i); }
	int getNotOnDomainType(int i) const { return vecGet(m_aiNotOnDomains, i); }
	int getNumNotOnDomainTypes() const { return (int)m_aiNotOnDomains.size(); }
	bool isNotOnDomainType(int i) const { return vecHas(m_aiNotOnDomains, i); }
	int getPrereqTerrainType(int i) const { return vecGet(m_aiPrereqTerrains, i); }
	int getNumPrereqTerrainTypes() const { return (int)m_aiPrereqTerrains.size(); }
	bool isPrereqTerrainType(int i) const { return vecHas(m_aiPrereqTerrains, i); }
	int getPrereqFeatureType(int i) const { return vecGet(m_aiPrereqFeatures, i); }
	int getNumPrereqFeatureTypes() const { return (int)m_aiPrereqFeatures.size(); }
	bool isPrereqFeatureType(int i) const { return vecHas(m_aiPrereqFeatures, i); }
	int getPrereqImprovementType(int i) const { return vecGet(m_aiPrereqImprovements, i); }
	int getNumPrereqImprovementTypes() const { return (int)m_aiPrereqImprovements.size(); }
	bool isPrereqImprovementType(int i) const { return vecHas(m_aiPrereqImprovements, i); }
	int getPrereqPlotBonusType(int i) const { return vecGet(m_aiPrereqPlotBonuses, i); }
	int getNumPrereqPlotBonusTypes() const { return (int)m_aiPrereqPlotBonuses.size(); }
	bool isPrereqPlotBonusType(int i) const { return vecHas(m_aiPrereqPlotBonuses, i); }
	int getPrereqLocalBuildingType(int i) const { return vecGet(m_aiPrereqLocalBuildings, i); }
	int getNumPrereqLocalBuildingTypes() const { return (int)m_aiPrereqLocalBuildings.size(); }
	bool isPrereqLocalBuildingType(int i) const { return vecHas(m_aiPrereqLocalBuildings, i); }
	const std::vector<BonusTypes>& getPrereqBonuses() const { return reinterpret_cast<const std::vector<BonusTypes>&>(m_aiPrereqBonuses); }
	const std::vector<MapCategoryTypes>& getMapCategories() const { return m_aeMapCategories; }  // empty -- promotions author no mapCategories (no curator table); returned as an empty view

	// vision LOS resolver -- negates + intensity pair-maps + the invisible/visible struct rows
	int getNegatesInvisibilityType(int i) const { return vecGet(m_aiNegatesInvisibility, i); }
	int getNumNegatesInvisibilityTypes() const { return (int)m_aiNegatesInvisibility.size(); }
	bool isNegatesInvisibilityType(int i) const { return vecHas(m_aiNegatesInvisibility, i); }
	int getNumVisibilityIntensityChangeTypes() const { return (int)m_aiVisibilityIntensity.size(); }
	int getVisibilityIntensityChangeType(int iInvisibility) const { return mapGet(m_aiVisibilityIntensity, iInvisibility); }
	bool isVisibilityIntensityChangeType(int iInvisibility) const { return m_aiVisibilityIntensity.count(iInvisibility) != 0; }
	int getNumInvisibilityIntensityChangeTypes() const { return (int)m_aiInvisibilityIntensity.size(); }
	int getInvisibilityIntensityChangeType(int iInvisibility) const { return mapGet(m_aiInvisibilityIntensity, iInvisibility); }
	bool isInvisibilityIntensityChangeType(int iInvisibility) const { return m_aiInvisibilityIntensity.count(iInvisibility) != 0; }
	int getNumVisibilityIntensityRangeChangeTypes() const { return (int)m_aiVisibilityIntensityRange.size(); }
	int getVisibilityIntensityRangeChangeType(int iInvisibility) const { return mapGet(m_aiVisibilityIntensityRange, iInvisibility); }
	bool isVisibilityIntensityRangeChangeType(int iInvisibility) const { return m_aiVisibilityIntensityRange.count(iInvisibility) != 0; }

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

	const char* getSound() const { return m_szSound.c_str(); }
	const CvPropertyManipulators* getPropertyManipulators() const { return &m_PropertyManipulators; }  // empty -- per-property modifier deposits carry the real data; the XML-era manipulator struct is deferred

	bool hasNegativeEffects() const
	{
		return getLungeChange() < 0 || getEnduranceChange() < 0 || getFirstStrikesChange() < 0
			|| getChanceFirstStrikesChange() < 0 || getVSBarbsChange() < 0 || getStrengthChange() < 0
			|| getAttackCombatModifierChange() < 0 || getCombatPercent() < 0
			|| getDefenseOnlyChange() > 0 || getNoInvisibilityChange() > 0 || getHiddenNationalityChange() != 0;
	}

	// entity-level gate -> the OnGameOptions/NotOnGameOptions flat lists (walked out of the composed CvJsonGate
	// condition tree in mapFrom: enabled = required-ON options, disabled = suppress-if-ON options).
	int getOnGameOption(int i) const { return vecGet(m_aiOnGameOptions, i); }
	int getNumOnGameOptions() const { return (int)m_aiOnGameOptions.size(); }
	bool isOnGameOption(int i) const { return vecHas(m_aiOnGameOptions, i); }
	int getNotOnGameOption(int i) const { return vecGet(m_aiNotOnGameOptions, i); }
	int getNumNotOnGameOptions() const { return (int)m_aiNotOnGameOptions.size(); }
	bool isNotOnGameOption(int i) const { return vecHas(m_aiNotOnGameOptions, i); }

	// =========================================================================================================
	//  NAMED GAPS -- see the class-header note. Genuinely NOT emitted by curate_promotion.py; NOT silent stubs.
	// =========================================================================================================
	// store-inverted onto tech.enables.promotions / tech.obsoletes.promotions; reconstructed at LOAD by the
	// cascadeLoadJson tech-FK reverse-index pass (the Route<-bonus pattern), which calls the setters below.
	TechTypes getTechPrereq() const { return m_eTechPrereq; }
	TechTypes getObsoleteTech() const { return m_eObsoleteTech; }
	void setTechPrereq(TechTypes e) { m_eTechPrereq = e; }       // load-time reverse-index writers (cascadeLoadJson)
	void setObsoleteTech(TechTypes e) { m_eObsoleteTech = e; }
	PromotionTypes getPrereqPromotion() const { return NO_PROMOTION; }        // curator DROP: line + priority + tech carry the chain
	PromotionTypes getPrereqOrPromotion1() const { return NO_PROMOTION; }     // curator DROP (as above)
	PromotionTypes getPrereqOrPromotion2() const { return NO_PROMOTION; }     // curator DROP (as above)
	int getCelebrityHappy() const { return 0; }                // curator drops the amount (owner); only skills.celebrity boolean survives

	int getZobristValue() const { return m_iZobristValue; }    // non-XML runtime map-hash, drawn from the synced RNG in the ctor (mirrors the archive)

	// dead: Categories (curator DROP)
	int getCategory(int /*i*/) const { return -1; }
	int getNumCategories() const { return 0; }
	bool isCategory(int /*i*/) const { return false; }

	// dead: BATTLEWORN trio (curator DROP -- not applied in processPromotion, pedia-only, nuked by owner)
	int getDamageperTurn() const { return 0; }
	int getStrAdjperTurn() const { return 0; }
	int getWeakenperTurn() const { return 0; }

	// dead: trap subsystem (curator DROP -- traps removed from the game)
	int getTrapDamageMax() const { return 0; }
	int getTrapDamageMin() const { return 0; }
	int getTrapComplexity() const { return 0; }
	int getNumTriggers() const { return 0; }
	int getTriggerBeforeAttackChange() const { return 0; }
	int getTrapSetWithPromotionType(int /*i*/) const { return -1; }
	int getNumTrapSetWithPromotionTypes() const { return 0; }
	bool isTrapSetWithPromotionType(int /*i*/) const { return false; }
	int getTrapImmunityUnitCombatType(int /*i*/) const { return -1; }
	int getNumTrapImmunityUnitCombatTypes() const { return 0; }
	bool isTrapImmunityUnitCombatType(int /*i*/) const { return false; }
	int getTargetUnitCombatType(int /*i*/) const { return -1; }
	int getNumTargetUnitCombatTypes() const { return 0; }
	bool isTargetUnitCombatType(int /*i*/) const { return false; }
	int getNumTrapDisableUnitCombatTypes() const { return 0; }
	int getTrapDisableUnitCombatType(int /*iUnitCombat*/) const { return 0; }
	bool isTrapDisableUnitCombatType(int /*iUnitCombat*/) const { return false; }
	int getNumTrapAvoidanceUnitCombatTypes() const { return 0; }
	int getTrapAvoidanceUnitCombatType(int /*iUnitCombat*/) const { return 0; }
	bool isTrapAvoidanceUnitCombatType(int /*iUnitCombat*/) const { return false; }
	int getNumTrapTriggerUnitCombatTypes() const { return 0; }
	int getTrapTriggerUnitCombatType(int /*iUnitCombat*/) const { return 0; }
	bool isTrapTriggerUnitCombatType(int /*iUnitCombat*/) const { return false; }

protected:
	virtual CvJsonModifiers* mutModifiers() { return &m_modifiers; }
	virtual CvJsonBoolBlock* mutSkills()    { return &m_skills; }
	virtual CvJsonGate*      mutGate()      { return &m_gate; }
	virtual CvJsonGrants*    mutGrants()    { return &m_grants; }

private:
	static int mapGet(const std::map<int, int>& m, int k) { std::map<int, int>::const_iterator it = m.find(k); return it != m.end() ? it->second : 0; }
	static int vecGet(const std::vector<int>& v, int i) { return (i >= 0 && i < (int)v.size()) ? v[i] : -1; }
	static bool vecHas(const std::vector<int>& v, int id) { for (size_t j = 0; j < v.size(); ++j) if (v[j] == id) return true; return false; }
	// skills tri-state (raw skills object: present true / present false / absent)
	bool skillTrue(const char* n) const { std::map<std::string, bool>::const_iterator it = m_skillTri.find(n); return it != m_skillTri.end() && it->second; }
	bool skillFalse(const char* n) const { std::map<std::string, bool>::const_iterator it = m_skillTri.find(n); return it != m_skillTri.end() && !it->second; }
	int skillCount(const char* n) const { std::map<std::string, bool>::const_iterator it = m_skillTri.find(n); return it == m_skillTri.end() ? 0 : (it->second ? 1 : -1); }

	CvJsonModifiers m_modifiers;
	CvJsonBoolBlock m_skills;
	CvJsonGate      m_gate;
	CvJsonGrants    m_grants;

	// strength family scalars
	int m_iCombatPercent, m_iStrengthChange, m_iStrengthModifier, m_iAttackCombatModifierChange, m_iDefenseCombatModifierChange;
	int m_iVSBarbsChange, m_iReligiousCombatModifierChange, m_iStealthCombatModifierChange, m_iDamageModifierChange, m_iMaxHPChange;
	int m_iEnduranceChange, m_iTauntChange, m_iBreakdownChanceChange, m_iBreakdownDamageChange;
	int m_iUnnerveChange, m_iEncloseChange, m_iLungeChange, m_iDynamicDefenseChange;
	int m_iCombatModifierPerSizeMoreChange, m_iCombatModifierPerSizeLessChange, m_iCombatModifierPerVolumeMoreChange, m_iCombatModifierPerVolumeLessChange;
	int m_iCityAttackPercent, m_iCityDefensePercent, m_iHillsAttackPercent, m_iHillsDefensePercent;
	int m_iKamikazePercent, m_iCombatLimitChange, m_iStealthStrikesChange, m_iQualityChange, m_iGroupChange;
	// other families
	int m_iWithdrawalChange, m_iFirstStrikesChange, m_iChanceFirstStrikesChange;
	int m_iBombardRateChange, m_iRBombardDamageChange, m_iRBombardDamageLimitChange, m_iRBombardDamageMaxUnitsChange, m_iDCMBombRangeChange, m_iDCMBombAccuracyChange;
	int m_iCollateralDamageChange, m_iCollateralDamageLimitChange, m_iCollateralDamageMaxUnitsChange, m_iCollateralDamageProtection;
	int m_iAirRangeChange, m_iInterceptChange, m_iEvasionChange, m_iAirCombatLimitChange;
	int m_iEnemyHealChange, m_iNeutralHealChange, m_iFriendlyHealChange, m_iSameTileHealChange, m_iAdjacentTileHealChange;
	int m_iSelfHealModifier, m_iNumHealSupport, m_iVictoryHeal, m_iVictoryAdjacentHeal, m_iVictoryStackHeal;
	int m_iMovesChange, m_iMoveDiscountChange, m_iExtraDropRange, m_iExperiencePercent;
	int m_iWorkRatePercent, m_iHillsWorkPercent, m_iPeaksWorkPercent;
	int m_iCargoChange, m_iSMCargoChange, m_iSMCargoVolumeChange, m_iSMCargoVolumeModifierChange;
	int m_iUpkeepModifier, m_iExtraUpkeep100, m_iUpgradeDiscount, m_iVisibilityChange;
	int m_iCaptureProbabilityModifierChange, m_iCaptureResistanceModifierChange, m_iPoisonProbabilityModifierChange;
	int m_iInsidiousnessChange, m_iInvestigationChange, m_iRevoltProtection, m_iPillageChange, m_iSurvivorChance;
	// vs-keyed percent maps
	std::map<int, int> m_aiTerrainAttackPercent, m_aiTerrainDefensePercent, m_aiFeatureAttackPercent, m_aiFeatureDefensePercent;
	std::map<int, int> m_aiUnitCombatModifierPercent, m_aiDomainModifierPercent, m_aiFlanking;
	std::map<int, int> m_aiTerrainWorkPercent, m_aiFeatureWorkPercent, m_aiBuildWorkRate;
	// struct rows
	std::vector<HealUnitCombat> m_aHealUnitCombat;
	std::vector<UnitCombatModifier> m_aAIWeight;
	// skills
	std::map<std::string, bool> m_skillTri;     // raw skills bool object (tri-state pairs + count-abilities)
	std::set<int> m_aiTerrainDoubleMove, m_aiFeatureDoubleMove;
	std::vector<int> m_aiSubCombat, m_aiRemoves;
	// identity
	int m_iLayerAnimationPath, m_iMinEraType, m_iMaxEraType, m_iStateReligionPrereq, m_iControlPoints, m_iCommandRange, m_iLevelPrereq, m_iLinePriority, m_iCommandType;
	PromotionLineTypes m_ePromotionLine;
	CvWString m_szRenamesUnitTo;
	int m_iReplacesUnitCombat, m_iDomainCargoChange, m_iSpecialCargoChange, m_iSpecialCargoPrereq, m_iSMNotSpecialCargoChange, m_iSMNotSpecialCargoPrereq;
	bool m_bLeader, m_bStatus, m_bQuick, m_bStarsign, m_bZeroesXP, m_bForOffset, m_bCargoPrereq, m_bRBombardPrereq;
	bool m_bSetOnHNCapture, m_bSetOnInvestigated, m_bPrereqNormInvisible, m_bPlotPrereqsKeepAfter, m_bRemoveAfterSet;
	std::vector<int> m_aeUnitCombat, m_aiNotOnUnitCombats, m_aiNotOnDomains;
	std::vector<int> m_aiQualifiedUnitCombat, m_aiDisqualifiedUnitCombat;   // computed post-load (setQualified/DisqualifiedUnitCombatTypes)
	std::vector<int> m_aiPrereqTerrains, m_aiPrereqFeatures, m_aiPrereqImprovements, m_aiPrereqPlotBonuses, m_aiPrereqLocalBuildings, m_aiPrereqBonuses;
	std::vector<MapCategoryTypes> m_aeMapCategories;   // empty -- no curator table
	// gate flat-lists (walked from m_gate in mapFrom)
	std::vector<int> m_aiOnGameOptions, m_aiNotOnGameOptions;
	TechTypes m_eTechPrereq, m_eObsoleteTech;   // store-inverted tech FKs, reconstructed at load (cascadeLoadJson)
	int m_iZobristValue;               // non-XML runtime map-hash (drawn from the synced RNG in the ctor, mirrors the archive)
	// vision
	std::vector<int> m_aiNegatesInvisibility;
	std::map<int, int> m_aiVisibilityIntensity, m_aiInvisibilityIntensity, m_aiVisibilityIntensityRange;
	std::vector<InvisibleTerrainChanges> m_aInvisibleTerrainChanges, m_aVisibleTerrainChanges, m_aVisibleTerrainRangeChanges;
	std::vector<InvisibleFeatureChanges> m_aInvisibleFeatureChanges, m_aVisibleFeatureChanges, m_aVisibleFeatureRangeChanges;
	std::vector<InvisibleImprovementChanges> m_aInvisibleImprovementChanges, m_aVisibleImprovementChanges, m_aVisibleImprovementRangeChanges;
	// misc
	std::string m_szSound;
	CvPropertyManipulators m_PropertyManipulators;   // empty -- deferred
};

#endif // CV_JSON_PROMOTION_INFO_H
