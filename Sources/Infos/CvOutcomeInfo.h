#pragma once

#ifndef CV_OUTCOME_INFO_H
#define CV_OUTCOME_INFO_H

//
//	CvOutcomeInfo -- the OUTCOME poco on the exemplar surface (patterns.md § THE GETTER SETUP). An outcome is
//	ONLY a GATE + IDENTITY + REPLACE-TIER tag: it carries NO effect payload (the payload lives on the per-carrier
//	CvOutcome instances -- mission-outcome-system.md), so the type composes no modifier unit and no enabler
//	section. The authored `requires.all` is a flat list of bare type ids, materialized ONCE at mapFrom into the
//	typed prereq members the gate reads (docs/architecture/patterns.md §Materialize at mapFrom) -- no per-call string read, and no section
//	unit standing in front of a scalar nothing else consumes.
//
//	JSON-fed via mapFrom from Assets/Data/outcomes/*.json (curate_outcome.py); the XML read path is GONE
//	(AGENTS.md §Build And Test (no XML-into-game for replaced infos)) -- no getDataMembers, no CvInfoUtil.
//
//	The grouped flag axes each hold ONE typed member read by ONE getter parameterized over the group's natural
//	index, never a bool-per-key hand getter. An outcome gate carries no magnitude entering cascade math, so
//	nothing here is ×100 -- these are raw config values.
//

#include "CvInfo.h"   // the JSON-info base (mapFrom); on /I -> bare include
#include <map>
#include <vector>

namespace picojson { class value; }

// `territory` -- the plot-ownership classes an outcome may fire in (authored as a string array).
enum OutcomeTerritory
{
	OUTCOME_TERRITORY_FRIENDLY,
	OUTCOME_TERRITORY_NEUTRAL,
	OUTCOME_TERRITORY_HOSTILE,
	OUTCOME_TERRITORY_BARBARIAN,
	NUM_OUTCOME_TERRITORIES
};

// `in` + `coastalCity` -- where, relative to a city, the outcome may fire.
enum OutcomePlacement
{
	OUTCOME_PLACEMENT_CITY,
	OUTCOME_PLACEMENT_NOT_CITY,
	OUTCOME_PLACEMENT_COASTAL_CITY,
	NUM_OUTCOME_PLACEMENTS
};

class CvOutcomeInfo : public CvInfo
{
public:

	CvOutcomeInfo();

	virtual void mapFrom(const picojson::value& entity);

	// ======================= 1. SECTIONS -- none: an outcome is a gate, not an effect source =================

	// ======================= 2. GROUPED FLAG AXES -- one getter per group, parameterized =====================
	bool hasTerritory(OutcomeTerritory eTerritory) const { return m_abTerritory[(int)eTerritory]; }
	bool hasPlacement(OutcomePlacement ePlacement) const { return m_abPlacement[(int)ePlacement]; }

	// ======================= 3. THE ODDS TABLE -- one materialized map, one read =============================
	// PROMOTION_* id -> the extra-chance percentage a holder of it contributes (may be negative).
	const std::map<int, int>& getPromotionOdds() const { return m_promotionOdds; }

	// ======================= 4. INTRINSIC -- bare typed reads (identity / FKs / the tier list) ===============
	const CvWString& getMessageKey() const { return m_szMessageKey; }
	bool isCapture() const             { return m_bCapture; }
	TechTypes  getPrereqTech() const   { return m_ePrereqTech; }    // TECH_* FK
	TechTypes  getObsoleteTech() const { return m_eObsoleteTech; }  // TECH_* FK
	CivicTypes getPrereqCivic() const  { return m_ePrereqCivic; }   // CIVIC_* FK
	const std::vector<BuildingTypes>& getPrereqBuildings() const { return m_aePrereqBuildings; }
	// The replace-tier list: a SURVIVING outcome prunes every outcome named here (CvOutcomeList's recursive
	// higher-tier-wins pass, mission-outcome-system.md).
	const std::vector<OutcomeTypes>& getReplaceOutcomes() const { return m_aeReplaceOutcomes; }

	void getCheckSum(uint32_t& iSum) const;

private:

	bool m_abTerritory[NUM_OUTCOME_TERRITORIES];
	bool m_abPlacement[NUM_OUTCOME_PLACEMENTS];
	std::map<int, int> m_promotionOdds;
	CvWString m_szMessageKey;
	bool m_bCapture;
	TechTypes m_ePrereqTech;
	TechTypes m_eObsoleteTech;
	CivicTypes m_ePrereqCivic;
	std::vector<BuildingTypes> m_aePrereqBuildings;
	std::vector<OutcomeTypes> m_aeReplaceOutcomes;
};

#endif // CV_OUTCOME_INFO_H
