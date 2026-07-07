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
#include "Defines/CvEnums.h"   // NUM_COMMERCE_TYPES
#include <vector>

class CvJsonHeritageInfo : public CvJsonInfo
{
public:
	CvJsonHeritageInfo() : m_bNeedsLanguage(false), m_iMissionType(-1) {}

	// era-THRESHOLD-gated empire commerce (×1 human): the value at era E = Σ bands whose eraMin <= E, per commerce.
	int getEraCommerceChange(int iCommerce, int iEra) const;

	bool needLanguage() const { return m_bNeedsLanguage; }   // identity.needsLanguage

	int getMissionType() const { return m_iMissionType; }    // RUNTIME (assigned post-load), NOT JSON
	void setMissionType(int i) { m_iMissionType = i; }

	// ⏳ getPropertyManipulators() -- the empire PROPERTY_* source deposits (CvPropertyManipulators). NOT modeled on the
	//    poco: the property-source/manipulator system is its own subsystem (#429); parsing it here is deferred to that
	//    system's pass. The one live caller (CvGameObject.cpp) reads the legacy Info meanwhile.

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
};

#endif // CV_JSON_HERITAGE_INFO_H
