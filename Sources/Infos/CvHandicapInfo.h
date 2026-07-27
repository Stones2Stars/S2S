#pragma once

#ifndef CV_HANDICAP_INFO_H
#define CV_HANDICAP_INFO_H

//
//	CvHandicapInfo -- the HANDICAP poco rebuilt to the exemplar surface (patterns.md § THE GETTER SETUP;
//	wave D, the config-heavy cut). A difficulty level is a CONFIG entity (state-repositories.md § WORLD is
//	CONFIG): it enables nothing and is read from its source, never cached behind a dirty protocol. JSON-fed
//	(Assets/Data/handicaps/*.json via mapFrom); no XML read (DEC-no-xml-into-game).
//
//	The pervasive HUMAN/AI DUAL-LEAF duality (curate_handicap.py; engine.md § Handicaps): a leaf's bare unit
//	is the BASE every player reads; the sibling `ai` object is the AI-AUDIENCE leaf the engine applies as its
//	own (usually multiplicative) stage for AI players. The point getters therefore carry an explicit
//	bAiAudience parameter: false = the base leaf, true = the AI-ONLY sibling leaf -- never a silently summed
//	union (which record a player reads is the engine's job, not encoded here).
//
//	THE GAME-START BASE READ (ruling 24 / the Sources-pass hc_leafBase semantics, PRESERVED): a batch-3a
//	leaf is an entry LIST -- bare base terms + a `{value, per:"ERA"}` deposit + its flat companion. The
//	scalar-mirror semantics every consumer formula was re-pointed against is the leaf's value AT GAME START
//	(ERA counter = 1, json §3.1): bare terms + the per-ERA terms once. The consumers' own
//	`getUnitUpkeepEraModifier() × getCurrentEra()` formulas carry the era ramp, so the per-ERA deposits stay
//	the cascade-side twin, never double-applied. Every point getter reads
//	compiled-sum + the per-ERA-at-one tail (materialized ONCE at mapFrom from the compiled entry list -- the
//	one sanctioned load-time scan source; no raw-JSON walker survives).
//

#include "CvInfo.h"                       // JSON-info base (mapFrom); on /I -> bare include
#include "Engine/CvPropertyManipulators.h"
#include <map>
#include <vector>

namespace picojson { class value; }

class CvHandicapInfo : public CvInfo
{
public:

	CvHandicapInfo();

	virtual void mapFrom(const picojson::value& entity);

	// ======================= 1. SECTIONS -- whole typed objects =======================
	virtual const CvModifiers* getModifiers() const { return &m_modifiers; }
	// `grants` (§5 numeric pulses + the `ai` scoped overrides) is COMPOSED: the section parse and the view
	// scalars below read ONE representation, so the grants machine and the engine views cannot drift.

	// ======================= 3. MODIFIER GROUPS -- game-start-base point reads ========================
	// (Conditioned-list access + the expected* what-if valuations are the base CvInfo surface. Kind and
	// scope are separate arguments -- [DEC-scope-is-an-axis]; every magnitude is ×100 -- [DEC-fixedpoint-x100];
	// bAiAudience selects the dual-leaf audience where the census authors an ai plane.)
	int getMaintenanceModifier(MaintenanceKind eKind, CvCascScope eScope) const;
	int getUpkeepModifier(UpkeepKind eKind, CvCascScope eScope, bool bAiAudience) const;
	int getCostsModifier(CostsKind eKind, CvCascScope eScope, bool bAiAudience) const;
	int getCombat(CombatKind eKind, CvCascScope eScope, bool bAiAudience) const;
	int getDiplomacy(DiplomacyKind eKind, CvCascScope eScope, bool bAiAudience) const;
	int getBarbarians(BarbariansKind eKind, CvCascScope eScope) const;
	// revolution.empire.percent -- the Revolution-index % (the AMOUNT kind's PERCENT slot; the flat-vs-percent
	// split lives in the getter NAME because REVOLUTION_AMOUNT carries flat index deltas elsewhere).
	int getRevolutionIndexModifier(CvCascScope eScope) const;
	// The authored wellbeing families' signed sums (happiness/health empire flat); ANGER/UNHEALTH hold no
	// slot and read 0 here (modifier.md §2b: the four-channel sign ROUTING is a fill/valuation rule).
	int getFlatWellbeing(WellbeingChannel eChannel, CvCascScope eScope) const;
	// The straggler scalars carrying the handicap's ai plane (growth / workRate -- both percent): the same
	// game-start-base read through the InfoScalar slot table.
	int getScalarModifier(InfoScalar eScalar, CvCascScope eScope, bool bAiAudience) const;
	// The colony-maintenance hard CAP (maintenance.empire.colony.cap -- ×100): the straight point read over
	// the (MAINTENANCE, CAP, EMPIRE, COUNT) compiled slot -- the dotted "colony.cap" member row; the walk's
	// bare-number leaf under `colony` synthesizes the COUNT unit.
	int getColonyMaintenanceCap() const;

	// ======================= 4. INTRINSIC -- bare typed reads (config; human values) ==================
	// Ruling 24: the AI unit-upkeep era stage is a PLAIN CONFIG VALUE on the §7 ai metadata plane
	// (ai.unitUpkeepEraModifier -- the legacy iAIPerEraModifier), feeding the clamped multiplicative stage at
	// CvPlayer.cpp:10354 and the other mapped `× getCurrentEra()` consumer formulas.
	int getUnitUpkeepEraModifier() const { return m_iUnitUpkeepEraModifier; }
	// identity.advancedStart.* -- the parked advanced-start points budget config.
	int getAdvancedStartPointsMod() const { return m_iAdvancedStartPointsMod; }
	int getAdvancedStartAiPercent() const { return m_iAdvancedStartAiPercent; }
	// grants views -- one-shot GAME-START provisioning, materialized from the COMPOSED §5 unit (base pulses +
	// the `ai` scoped overrides; humans and AIs split entirely, own vs game handicap).
	int getStartingGold() const { return m_iStartingGold; }
	int getStartingDefenseUnits() const { return m_iStartingDefenseUnits; }
	int getStartingWorkerUnits() const { return m_iStartingWorkerUnits; }
	int getStartingExploreUnits() const { return m_iStartingExploreUnits; }
	int getAIStartingDefenseUnits() const { return m_iAIStartingDefenseUnits; }
	int getAIStartingWorkerUnits() const { return m_iAIStartingWorkerUnits; }
	int getAIStartingExploreUnits() const { return m_iAIStartingExploreUnits; }
	// identity.goodies -- the goody-hut roster (GOODY_* FKs, resolved at load).
	int getNumGoodies() const { return (int)m_aiGoodies.size(); }
	int getGoodies(int iIndex) const
	{
		if (iIndex >= 0 && iIndex < (int)m_aiGoodies.size())
		{
			return m_aiGoodies[iIndex];
		}
		return -1;
	}

	// PROPERTY_* per-turn sources (per-handicap crime/education), fed from the compiled PROPERTY_* families
	// via the ONE shared bridge (CascadePropertyBridge::bridgeFamilies).
	const CvPropertyManipulators* getPropertyManipulators() const { return &m_PropertyManipulators; }

	virtual const CvTriggers*  getTriggers()  const { return &m_triggers; }   // §5 -- triggers + the folded grants

protected:
	virtual CvModifiers* mutModifiers() { return &m_modifiers; }
	virtual CvTriggers*  mutTriggers()  { return &m_triggers; }

private:
	// The ONE game-start-base read every point getter delegates to: the compiled (family, kind, scope, unit)
	// slot sum for the requested audience leaf, plus the materialized per-ERA-at-one tail.
	int gameStartBase(ModifierFamily eFamily, int iKind, CvCascScope eScope, CvCascUnit eUnit, bool bAiAudience) const;

	// --- the composed section units ---
	CvModifiers m_modifiers;                        // §6 families (the base dispatch fills this; feeds the PROPERTY_* bridge)
	CvTriggers  m_triggers;    // §5 game-start pulses (startingGold/units + the `ai` overrides)
	CvPropertyManipulators m_PropertyManipulators;  // fed from the PROPERTY_* families (CascadePropertyBridge::bridgeFamilies)

	// --- the materialized load-derived reads (mapFrom-only; the compiled entry list is the one scan source) ---
	std::map<int, int> m_perEraStart;   // packed (audience, family, kind, scope, unit) -> Σ per-ERA value at ERA 1

	// --- the intrinsic identity/config members ---
	int m_iUnitUpkeepEraModifier;
	int m_iAdvancedStartPointsMod;
	int m_iAdvancedStartAiPercent;
	int m_iStartingGold;
	int m_iStartingDefenseUnits;
	int m_iStartingWorkerUnits;
	int m_iStartingExploreUnits;
	int m_iAIStartingDefenseUnits;
	int m_iAIStartingWorkerUnits;
	int m_iAIStartingExploreUnits;
	std::vector<int> m_aiGoodies;   // identity.goodies -> resolved GOODY_* ids
};

#endif // CV_HANDICAP_INFO_H
