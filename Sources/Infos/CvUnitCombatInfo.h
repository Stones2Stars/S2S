#pragma once
#ifndef CV_JSON_UNITCOMBAT_INFO_H
#define CV_JSON_UNITCOMBAT_INFO_H

//
//	CvUnitCombatInfo -- the UNITCOMBAT poco rebuilt to the full exemplar surface (patterns.md par. THE GETTER
//	SETUP: the four read categories, nothing else). A unitcombat is a GRANTOR on the unit plane: it PROVIDES
//	skills (the par.8 grant/revoke planes) and deposits par.6 unit-scope self-accumulator values (modifier.md
//	par.6) that the unit's ACTIVE set folds in on the instance. Styled for the JSON anatomy (json.md par.2):
//	every magnitude read is a load-compiled fetch ([DEC-materialize-at-mapfrom]); kind and scope are separate
//	parameters ([DEC-scope-is-an-axis]); every magnitude getter IS x100 ([DEC-fixedpoint-x100]); the type-keyed
//	vs-entries (terrain/feature/unitCombat/build targets) stay compiled ENTRY-LIST reads by design; no legacy
//	getter name returns ([DEC-new-getter-surface]).
//

#include "CvInfo.h"
#include "CvVisionSection.h"        // par.9 `vision` typed section (shared unit-plane LOS block)
#include "CvSizeMattersSection.h"   // par.9 `sizeMatters` typed section (the SM base ranks live here)
#include "CvOutcomesSection.h"      // par.8 `outcomes` typed section (shared unit-plane CvOutcome intake)
#include "Defines/CvEnums.h"        // ReligionTypes / EraTypes / MissionTypes + NO_*
#include <vector>

class CvUnitCombatInfo : public CvInfo
{
public:
	CvUnitCombatInfo();
	virtual void mapFrom(const picojson::value& entity);

	// ======================= 1. SECTIONS -- whole typed objects =======================
	virtual const CvModifiers* getModifiers() const { return &m_modifiers; }
	virtual const CvClassificationBlock* getSkills() const { return &m_skills; }
	virtual const CvGate* getGate() const { return &m_gate; }
	const CvVisionSection& getVision() const { return m_vision; }
	const CvSizeMattersSection& getSizeMatters() const { return m_sizeMatters; }

	// ======================= 2. CLASSIFICATION -- O(1) bitset tests, hold-vs-provide in the NAME (json par.8) ==
	// A unitcombat PROVIDES skills to its member units (the grantor direction); the FALSE plane REVOKES
	// (skills.md par.4 grant/revoke). The unit instance folds the active set.
	bool providesSkill(int iSkillId) const { return m_skills.hasId(iSkillId); }
	bool revokesSkill(int iSkillId) const { return m_skills.hasFalseId(iSkillId); }
	bool providesSkills() const { return !m_skills.isEmpty(); }

	// ======================= 3. MODIFIER GROUPS -- point reads over the compiled sums =======================
	// (Conditioned-list access + the expected* what-if valuations are the base CvInfo surface. The census
	// stragglers -- withdrawal / revoltProtection -- read through the base getScalar. The mixed-unit groups
	// keep the flat-vs-modifier split in the NAME, the getFlatYield/getYieldModifier convention --
	// [DEC-fixedpoint-x100]: the name says the VALUE, never a scale suffix.)
	int getFlatCombat(CombatKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_COMBAT, eKind, eScope, CASC_UNIT_FLAT); }
	int getCombatModifier(CombatKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_COMBAT, eKind, eScope, CASC_UNIT_PERCENT); }
	// capture probability/resistance: the unit plane authors FLAT percentage-point chances (census verdict;
	// the infoKindUnit CAPTURE row carries the empire-scope percent authoring -- reported).
	int getCapture(CaptureKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_CAPTURE, eKind, eScope, CASC_UNIT_FLAT); }
	int getFlatHeal(HealKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_HEAL, eKind, eScope, CASC_UNIT_FLAT); }
	int getHealModifier(HealKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_HEAL, eKind, eScope, CASC_UNIT_PERCENT); }
	int getEspionage(EspionageKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_ESPIONAGE, eKind, eScope, CASC_UNIT_FLAT); }
	// experience.unit.percent is the XP-GAIN modifier (the city-scope flat free-XP plane rides the same kind at
	// its own scope+unit slot; the name-split separates them).
	int getExperienceModifier(ExperienceKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_EXPERIENCE, eKind, eScope, CASC_UNIT_PERCENT); }
	int getUpkeepModifier(UpkeepKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_UPKEEP, eKind, eScope, CASC_UNIT_PERCENT); }
	// upkeep.unit.extra is a FLAT gold magnitude: the compiled x100 IS the legacy x100 accessor scale.
	int getFlatUpkeep(UpkeepKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_UPKEEP, eKind, eScope, CASC_UNIT_FLAT); }

	// ======================= 4. INTRINSIC -- bare typed reads (the census identity set) =======================
	int getReligion() const { return m_iReligion; }   // identity.religion: RELIGION_* FK (religious-combat class)
	int getEra() const { return m_iEra; }             // identity.era: ERA_* FK (era classification)
	bool isForMilitary() const { return m_bForMilitary; }           // identity.forMilitary (AI tag)
	bool isForNavalMilitary() const { return m_bForNavalMilitary; } // identity.forNavalMilitary (AI tag)
	const std::vector<int>& getGGPointsForUnits() const { return m_aiGGPointsForUnits; }   // identity.ggPointsForUnits (UNIT_* FKs)
	const std::vector<int>& getDefaultStatuses() const { return m_aiDefaultStatuses; }     // identity.defaultStatuses (PROMOTION_* FKs)
	// par.8 keyed-skill FK lists (skills.<name>.{TYPE}:true -- typed members per the CvClassificationBlock
	// header: keyed extras are NOT the flat bool block).
	const std::vector<int>& getTerrainDoubleMoves() const { return m_aiTerrainDoubleMove; }
	const std::vector<int>& getFeatureDoubleMoves() const { return m_aiFeatureDoubleMove; }
	// Derived at mapFrom over the type's own skill/doubleMove data (the hasCityOverLimitAnger materialization
	// precedent): does holding this class change which plots the unit can move through?
	bool changesMoveThroughPlots() const { return m_bChangesMoveThroughPlots; }
	// Non-XML runtime map-hash contribution, drawn from the synced RNG at construction (archive mirror).
	int getZobristValue() const { return m_iZobristValue; }
	// The property engine's uniform walk surface (unitcombats author no PROPERTY_* families today -- empty).
	const CvPropertyManipulators* getPropertyManipulators() const { return &m_PropertyManipulators; }

	// ======================= the CvOutcome kill/action-mission system (par.8 outcomes) =======================
	// Fed from the unitcombat's `outcomes` JSON (the composed section parses it); the runtime merges these
	// with the unit's lists. Forwards keep the outward surface unchanged (CvOutcomeListMerged consumers).
	const CvOutcomeList* getKillOutcomeList() const { return m_outcomes.getKillOutcomeList(); }
	int getNumActionOutcomes() const { return m_outcomes.getNumActionOutcomes(); }
	const CvOutcomeList* getActionOutcomeList(int iIndex) const { return m_outcomes.getActionOutcomeList(iIndex); }
	MissionTypes getActionOutcomeMission(int iIndex) const { return m_outcomes.getActionOutcomeMission(iIndex); }
	const CvOutcomeList* getActionOutcomeListByMission(MissionTypes eMission) const { return m_outcomes.getActionOutcomeListByMission(eMission); }
	const CvOutcomeMission* getOutcomeMission(int iIndex) const { return m_outcomes.getOutcomeMission(iIndex); }
	const CvOutcomeMission* getOutcomeMissionByMission(MissionTypes eMission) const { return m_outcomes.getOutcomeMissionByMission(eMission); }

protected:
	virtual CvModifiers* mutModifiers() { return &m_modifiers; }
	virtual CvClassificationBlock* mutSkills() { return &m_skills; }
	virtual CvGate* mutGate() { return &m_gate; }

private:
	// --- the composed section units ---
	CvModifiers m_modifiers;
	CvClassificationBlock m_skills;
	CvGate m_gate;
	CvVisionSection m_vision;
	CvSizeMattersSection m_sizeMatters;
	CvOutcomesSection m_outcomes;

	// --- the intrinsic identity members (materialized once at mapFrom) ---
	int m_iReligion;
	int m_iEra;
	bool m_bForMilitary;
	bool m_bForNavalMilitary;
	std::vector<int> m_aiGGPointsForUnits;
	std::vector<int> m_aiDefaultStatuses;
	std::vector<int> m_aiTerrainDoubleMove;
	std::vector<int> m_aiFeatureDoubleMove;
	bool m_bChangesMoveThroughPlots;
	int m_iZobristValue;   // runtime random (drawn once in the ctor -- a re-map must not redraw the synced RNG)

	CvPropertyManipulators m_PropertyManipulators;   // property engine (no unitcombat PROPERTY_* data today)
};

#endif // CV_JSON_UNITCOMBAT_INFO_H
