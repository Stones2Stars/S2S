#pragma once
#ifndef CV_SPECIAL_UNIT_INFO_H
#define CV_SPECIAL_UNIT_INFO_H

//
//	CvSpecialUnitInfo -- the SPECIAL-UNIT class poco (captive / people / missile / fighter / ...) rebuilt to the
//	exemplar surface (patterns.md par. THE GETTER SETUP). JSON-fed (Assets/Data/specialunits/*.json via mapFrom);
//	no XML read. A special-unit class authors the combat/withdrawal unit-plane families (one entity today) --
//	those ride the composed compiled CvModifiers, read through the point getters; the identity flags are bare
//	typed intrinsics. No legacy getter name returns (docs/architecture/patterns.md §THE TWO READ ROLES (new getter surface, never widen legacy)).
//

#include "CvInfo.h"   // JSON-info base (mapFrom); on /I -> bare include

namespace picojson { class value; }

class CvSpecialUnitInfo : public CvInfo
{
public:
	CvSpecialUnitInfo();

	virtual void mapFrom(const picojson::value& entity);

	// ======================= 1. SECTIONS -- whole typed objects =======================
	virtual const CvModifiers* getModifiers() const { return &m_modifiers; }

	// ======================= 3. MODIFIER GROUPS -- point reads over the compiled sums =======================
	// (Conditioned-list access + the expected* what-if valuations are the base CvInfo surface. The lone
	// withdrawal straggler reads through the base getScalar(SCALAR_WITHDRAWAL, CASC_SCOPE_UNIT,
	// CASC_UNIT_PERCENT) -- patterns.md getScalar.)
	int getCombatModifier(CombatKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_COMBAT, eKind, eScope, CASC_UNIT_PERCENT); }

	// ======================= 4. INTRINSIC -- bare typed reads (the census identity set) =======================
	bool isValid() const { return m_bValid; }         // identity.valid (default TRUE; curator elides valid:true)
	bool isCityLoad() const { return m_bCityLoad; }   // identity.cityLoad
	bool isSMLoadSame() const { return m_bSMLoadSame; }   // identity.smLoadSame

protected:
	virtual CvModifiers* mutModifiers() { return &m_modifiers; }

private:
	// --- the composed section units ---
	CvModifiers m_modifiers;

	// --- the intrinsic identity members (materialized once at mapFrom) ---
	bool m_bValid;
	bool m_bCityLoad;
	bool m_bSMLoadSame;
};

#endif // CV_SPECIAL_UNIT_INFO_H
