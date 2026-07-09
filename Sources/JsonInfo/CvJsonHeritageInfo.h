#pragma once
#ifndef CV_JSON_HERITAGE_INFO_H
#define CV_JSON_HERITAGE_INFO_H

//
//	CvJsonHeritageInfo -- the JSON real poco for HERITAGES (empire-scope acquired legacies). Its live values are the
//	era-gated empire commerce + the language gate. Its tech/heritage prereqs ride the base (tech.enables.heritages /
//	this heritage's enables.heritages succession). The era commerce is HUMAN (the curator ÷100-descaled the legacy ×100
//	EraCommerceChanges -- the ONE ×100 field in the small/mid set; NEVER emit a ×100 value). No cascade here.
//
//	Live callers (verified 2026-07-07): getEraCommerceChange -> CvPlayer::processHeritage (the empire commerce apply --
//	a MODIFIER apply-loop the cascade replaces, so this is cascade-read data); needLanguage -> canAddHeritage gate.
//

#include "CvJsonInfo.h"
#include "Defines/CvEnums.h"      // NUM_COMMERCE_TYPES / EraTypes / HeritageTypes / NO_TECH
#include "Defines/CvStructs.h"    // CommerceArray
#include "Infrastructure/IDValueMap.h"   // IDValueMap<EraTypes,CommerceArray> / EraCommerceArray -- the archived getEraCommerceChanges100 shape
#include <vector>

class CvJsonHeritageInfo : public CvJsonInfo
{
public:
	CvJsonHeritageInfo() : m_bNeedsLanguage(false), m_iMissionType(-1), m_bPrereqsResolved(false), m_iPrereqTech(NO_TECH) {}

	// era-THRESHOLD-gated empire commerce (×1 human): the value at era E = Σ bands whose eraMin <= E, per commerce.
	int getEraCommerceChange(int iCommerce, int iEra) const;

	bool needLanguage() const { return m_bNeedsLanguage; }   // identity.needsLanguage

	int getMissionType() const { return m_iMissionType; }    // RUNTIME (assigned post-load), NOT JSON
	void setMissionType(int i) { m_iMissionType = i; }

	// property engine (self-contained, #429); the XML-era manipulator data is deferred -- empty for now. The archived
	// Info the one live caller (CvGameObject.cpp) used to read is gone, so the poco serves the surface.
	const CvPropertyManipulators* getPropertyManipulators() const { return &m_PropertyManipulators; }

	// ============================ #430 mirrored legacy CvHeritageInfo getters (remainder of the archived surface) ============================

	// --- archived getEraCommerceChanges100 -- REAL (live callers: CvPlayer::processHeritage + CvPlayer::
	// getHeritageCommerceEraChange, both still reading this exact reference-returning shape). Reconstructed from the
	// SAME era-threshold bands mapFrom already parses for getEraCommerceChange, ×100 (CentiCommerce, matching the
	// archived/legacy scale), keyed by the actual EraTypes -- the JSON only carries the curator's 1-based era ORDINAL
	// (enabled.min; curate_heritage.py::_era_ordinal), and CvCascadeConditionEval.cpp's own ERA gate evaluator
	// ("ctx.player->getCurrentEra() + 1" compared against that ordinal) grounds ordinal-1 == the EraTypes index.
	const IDValueMap<EraTypes, CommerceArray>& getEraCommerceChanges100() const { return m_eraCommerceChanges100; }

	// --- archived acquisition-prereq getters -- REAL, reconstructed from the FORWARD edges the curator store-inverts
	// this data onto (curate_heritage.py: "PrereqTech -> DROP: the store inverts it to tech.enables.heritages" /
	// "PrereqOrHeritage -> DROP: the store now derives the heritage->heritage succession edge (folklore enables taxon)
	// into enables.heritages"). Both are LIVE consumer gates (CvPlayer::canAddHeritage tech + predecessor checks;
	// CvGameTextMgr pedia/help), so they must return the real values -- recovered by the inverse edge scan (see .cpp):
	//   getPrereqTech       = the tech whose enables.heritages lists THIS heritage (legacy single PrereqTech).
	//   getPrereqOrHeritage = every heritage whose enables.heritages lists THIS heritage (the folklore->taxon
	//                         predecessors; empty for a folklore heritage, which is tech-gated only).
	// Resolved lazily on first read (reverse scan needs every tech/heritage poco loaded -- true at runtime, never at
	// load time when a poco's own mapFrom runs), cached in mutable members (a pure derived memo over immutable data).
	int getPrereqTech() const;
	const std::vector<HeritageTypes>& getPrereqOrHeritage() const;

	virtual void mapFrom(const picojson::value& entity);

	// --- the composed section units (by value; the base's mapFrom dispatch writes them via mut*) ---
	virtual const CvJsonEdges*     getEdges()     const { return &m_edges; }
	virtual const CvJsonModifiers* getModifiers() const { return &m_modifiers; }

protected:
	virtual CvJsonEdges*     mutEdges()     { return &m_edges; }
	virtual CvJsonModifiers* mutModifiers() { return &m_modifiers; }

private:
	CvJsonEdges     m_edges;
	CvJsonModifiers m_modifiers;
	struct EraBand { int eraMin; int value; };
	std::vector<EraBand> m_aEraCommerce[NUM_COMMERCE_TYPES];   // {gold/research/culture/espionage}.empire.flat, era-gated
	bool m_bNeedsLanguage;
	int m_iMissionType;   // runtime
	CvPropertyManipulators m_PropertyManipulators;   // STUB empty -- property engine, XML-era manipulator data deferred

	// REAL -- the archived getEraCommerceChanges100 shape, built from m_aEraCommerce in mapFrom (see .cpp).
	IDValueMap<EraTypes, CommerceArray> m_eraCommerceChanges100;

	// REAL -- the archived getPrereqTech / getPrereqOrHeritage values, lazily reverse-scanned from the FORWARD
	// enables.heritages edges the curator inverts them onto (see resolvePrereqs() in the .cpp). Mutable: a derived
	// memo over immutable loaded data, populated on first read (all pocos present by then), never a data mutation.
	void resolvePrereqs() const;
	mutable bool m_bPrereqsResolved;
	mutable int m_iPrereqTech;                        // reverse of tech.enables.heritages (NO_TECH when none)
	mutable std::vector<HeritageTypes> m_prereqOrHeritage;   // reverse of predecessor heritages' enables.heritages
};

#endif // CV_JSON_HERITAGE_INFO_H
