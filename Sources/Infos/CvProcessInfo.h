#pragma once
#ifndef CV_JSON_PROCESS_INFO_H
#define CV_JSON_PROCESS_INFO_H

//
//	CvProcessInfo -- the PROCESS poco rebuilt to the exemplar surface (patterns.md § THE GETTER SETUP). A
//	process converts hammers into a commerce channel (Wealth->gold, Research->research, ...); its ONE live value
//	set is the per-commerce production->commerce CONVERSION rate, authored as the json.md §9 `conversion`
//	bespoke block (hurry's home -- keys per commerce channel, human values; OWNER-CONFIRMED item 18: a process
//	is a hammers->commerce CONVERSION, the idle-production fallback, never a commerce-modifier deposit) and
//	materialized ONCE at mapFrom into the typed per-channel plane below ([DEC-materialize-at-mapfrom]). No
//	legacy getter name returns ([DEC-new-getter-surface]).
//
//	The tech prereq + the only-latest-in-chain supersession are store-inverted onto the TECH
//	(tech.enables.processes / this process's obsoletedBy.techs) -- base availability, reconstructed at LOAD by
//	the readJson reverse pass (the setter below).
//

#include "CvInfo.h"

class CvProcessInfo : public CvInfo
{
public:
	CvProcessInfo();

	virtual void mapFrom(const picojson::value& entity);

	// ======================= 1. SECTIONS -- whole typed objects =======================
	virtual const CvEdges*     getEdges()     const { return &m_edges; }
	virtual const CvModifiers* getModifiers() const { return &m_modifiers; }

	// ======================= 3. THE CONVERSION PLANE -- one materialized §9 block ============================
	// The production->commerce conversion rate (×100: authored 50 reads 5000), per channel -- a bare member
	// read over the plane mapFrom materializes from the `conversion` block. The conversion is the CITY's
	// hammers-fold (the §2a EXTRA leg `production × prodToCommerce`), so only the city scope answers; the
	// scope parameter stays spelled out ([DEC-scope-is-an-axis] -- the signature the consumers already ask).
	int getProductionToCommerce(CommerceTypes eCommerce, CvCascScope eScope) const
	{
		if (eScope != CASC_SCOPE_CITY || eCommerce < 0 || eCommerce >= NUM_COMMERCE_TYPES)
		{
			return 0;
		}
		return m_aiProductionToCommerce[(int)eCommerce];
	}

	// ======================= 4. INTRINSIC -- the reverse-pass-fed tech FK ====================================
	// store-inverted onto the tech (tech.enables.processes); reconstructed at LOAD by the readJson reverse
	// pass (CvReversePass), which calls the setter below. LOAD-ONLY writer.
	TechTypes getTechPrereq() const { return m_eTechPrereq; }
	void setTechPrereq(TechTypes eTech) { m_eTechPrereq = eTech; }

protected:
	virtual CvEdges*     mutEdges()     { return &m_edges; }
	virtual CvModifiers* mutModifiers() { return &m_modifiers; }

private:
	CvEdges     m_edges;
	CvModifiers m_modifiers;
	TechTypes m_eTechPrereq;
	// the materialized §9 `conversion` plane (per-channel ×100; fully redefined on every (re-)map)
	int m_aiProductionToCommerce[NUM_COMMERCE_TYPES];
};

#endif // CV_JSON_PROCESS_INFO_H
