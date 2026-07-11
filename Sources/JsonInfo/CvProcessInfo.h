#pragma once
#ifndef CV_JSON_PROCESS_INFO_H
#define CV_JSON_PROCESS_INFO_H

//
//	CvProcessInfo -- the JSON real poco for PROCESSES (convert hammers into a commerce type: Wealth->gold,
//	Research->research, Culture->culture, …). A process's ONE live value is its per-commerce production->commerce
//	CONVERSION rate. Its tech prereq + the only-latest-in-chain supersession are store-inverted onto the TECH
//	(tech.enables.processes / this process's obsoletedBy.techs) -- base availability, NOT poco getters. HUMAN-native
//	percents (natural 30/40/50, NOT ×100 -- CvCity::changeProduction reads them straight). No cascade here.
//
//	Live callers (verified 2026-07-07): getProductionToCommerceModifier -> CvCity::changeProduction (the hammer->
//	commerce conversion) + CvPlayerAI/CvCityAI process valuation + pedia. ⚠ DISTINCT from the same-named
//	CvCity::getProductionToCommerceModifier (a city accumulator) -- same name, different class.
//

#include "CvInfo.h"
#include "Defines/CvEnums.h"   // NUM_COMMERCE_TYPES / COMMERCE_*

class CvProcessInfo : public CvInfo
{
public:
	CvProcessInfo();

	int getProductionToCommerceModifier(int i) const
	{ return (i >= 0 && i < NUM_COMMERCE_TYPES) ? m_aiProductionToCommerce[i] : 0; }

	// store-inverted onto the tech (tech.enables.processes); reconstructed at LOAD by the cascadeLoadJson tech-FK
	// reverse-index pass (the Route<-bonus pattern), which calls setTechPrereq.
	TechTypes getTechPrereq() const { return m_eTechPrereq; }
	void setTechPrereq(TechTypes e) { m_eTechPrereq = e; }   // load-time reverse-index writer (cascadeLoadJson)

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
	int m_aiProductionToCommerce[NUM_COMMERCE_TYPES];   // gold/research/culture/espionage .city.percent (natural %, ×1)
	TechTypes m_eTechPrereq;   // store-inverted tech.enables.processes, reconstructed at load (cascadeLoadJson)
};

#endif // CV_JSON_PROCESS_INFO_H
