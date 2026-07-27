#pragma once
#ifndef CV_JSON_HERITAGE_INFO_H
#define CV_JSON_HERITAGE_INFO_H

//
//	CvHeritageInfo -- the HERITAGE poco rebuilt to the exemplar surface (patterns.md § THE GETTER SETUP).
//	Empire-scope acquired legacies: the era-banded empire commerce is authored as ERA-threshold CONDITIONED
//	entries ({value, enabled:{type:ERA, min:N}} -- json.md §6 "era-dependent values use the ERA counter"), so it
//	lives on the compiled conditioned list (base modifierConditioned()/expected* surface), never as a mirrored
//	band table ([DEC-new-getter-surface]: the legacy era-commerce mirror died with this rebuild). No legacy
//	getter name returns.
//
//	Acquisition prereqs are store-inverted onto the FORWARD enables edges (curate_heritage.py: PrereqTech ->
//	tech.enables.heritages; PrereqOrHeritage -> the predecessor heritage's enables.heritages) and read back here
//	from THIS info's own load-populated reverse view (EDGEF_RELATED -- [DEC-one-reverse-view]: every info
//	already carries its reverse lookups after load; the exact enables-predicate is confirmed against each
//	related source's forward edge, so no consumer-side repo scan exists).
//

#include "CvInfo.h"
#include "Defines/CvEnums.h"   // HeritageTypes / NO_TECH
#include <vector>

class CvHeritageInfo : public CvInfo
{
public:
	CvHeritageInfo();

	virtual void mapFrom(const picojson::value& entity);

	// The post-map derivation the reverse pass drives (rp_deriveHeritagePrereqs), matching the unit plane's
	// deriveAtRegistryComplete: runnable only once EVERY entity is mapped, because it reads OTHER infos' edges.
	void deriveAtRegistryComplete();

	// ======================= 1. SECTIONS -- whole typed objects =======================
	virtual const CvEdges*     getEdges()     const { return &m_edges; }
	virtual const CvModifiers* getModifiers() const { return &m_modifiers; }

	// ======================= 3. MODIFIER GROUPS -- conditioned-only (the ERA-banded empire commerce) =========
	// No point getters: every commerce entry is ERA-conditioned, so every unconditioned sum is 0 by
	// construction. Readers walk the base modifierConditioned()/modifierConditionedRange() or ask the
	// expected* endpoints (the what-if legitimately means "the empire commerce at the asking player's era").
	// The PROPERTY_* families (the folklore education) feed the property engine through the bridge below.

	// ======================= 4. INTRINSIC -- bare typed reads (the census identity set) ======================
	bool needsLanguage() const { return m_bNeedsLanguage; }   // identity.needsLanguage (the canAddHeritage language gate)

	// Fed from the PROPERTY_* families in mapFrom (player gather -> every owner city, RELATION_ASSOCIATED).
	const CvPropertyManipulators* getPropertyManipulators() const { return &m_PropertyManipulators; }

	// --- the acquisition prereqs, read from THIS info's own reverse view (see the header note): ---
	//   getPrereqTech       = the tech whose enables.heritages lists THIS heritage (legacy single PrereqTech).
	//   getPrereqOrHeritage = every heritage whose enables.heritages lists THIS heritage (the folklore->taxon
	//                         predecessors; empty for a folklore heritage, which is tech-gated only).
	// Both are materialized ONCE by deriveAtRegistryComplete(), so these are BARE MEMBER READS.
	int getPrereqTech() const { return m_iPrereqTech; }
	const std::vector<HeritageTypes>& getPrereqOrHeritage() const { return m_prereqOrHeritage; }

	// --- RUNTIME member (assigned post-load, NOT JSON) ---
	int getMissionType() const { return m_iMissionType; }
	void setMissionType(int iMission) { m_iMissionType = iMission; }

protected:
	virtual CvEdges*     mutEdges()     { return &m_edges; }
	virtual CvModifiers* mutModifiers() { return &m_modifiers; }

private:
	// --- the composed section units ---
	CvEdges     m_edges;
	CvModifiers m_modifiers;

	// --- the intrinsic identity members ---
	bool m_bNeedsLanguage;
	int m_iMissionType;   // runtime
	CvPropertyManipulators m_PropertyManipulators;   // fed from the PROPERTY_* families (CascadePropertyBridge)

	// --- the acquisition prereqs, materialized from the load-populated reverse view (see the .cpp) ---
	int m_iPrereqTech;
	std::vector<HeritageTypes> m_prereqOrHeritage;
};

#endif // CV_JSON_HERITAGE_INFO_H
