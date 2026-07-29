#pragma once
#ifndef CV_JSON_SPECIALIST_INFO_H
#define CV_JSON_SPECIALIST_INFO_H

//
//	CvSpecialistInfo -- the SPECIALIST poco rebuilt to the exemplar surface (patterns.md § THE GETTER SETUP:
//	the four read categories, nothing else). The specialist's own output is CITY-scope own-output
//	(modifier.md §2a: the specialist carries its OWN percent layer -- the civic-conditioned percent entries --
//	applied to its intrinsic output BEFORE it joins the city BASE; the empire-scope wonder-conditioned entries
//	are the own-output inversions of the legacy building SpecialistYieldChanges). Every magnitude read is a
//	load-compiled fetch ([DEC-materialize-at-mapfrom]); kind and scope are separate parameters
//	([DEC-scope-is-an-axis]); every magnitude getter IS ×100 ([DEC-fixedpoint-x100] -- NB the wellbeing values
//	are the curator's ÷100 de-scale of the legacy latent-×100, so the compiled ×100 sum IS the legacy-scale
//	number); no legacy getter name returns ([DEC-new-getter-surface]).
//
//	The expected* what-if on this type means the SPECIALIST'S OWN OUTPUT LAYER per §2a (what one assigned
//	specialist of this type yields under the passed contexts) -- its own percent stack resolves inside that
//	layer, distinct from the city percent stack the output later takes.
//

#include "CvInfo.h"
#include <map>
#include <vector>

class CvSpecialistInfo : public CvInfo
{
public:
	CvSpecialistInfo();

	virtual void mapFrom(const picojson::value& entity);

	// ======================= 1. SECTIONS -- whole typed objects =======================
	virtual const CvModifiers* getModifiers() const { return &m_modifiers; }

	// ======================= 3. MODIFIER GROUPS -- point reads over the compiled sums ========================
	// (Conditioned-list access + the expected* what-if valuations are the base CvInfo surface. The census
	// participation: unconditioned CITY flats = the intrinsic own output (the point reads below); the civic-
	// conditioned percents (the §2a own percent layer), the wonder-conditioned empire flats, and the tech
	// keep-on-self wellbeing entries are ALL conditioned -- conditioned-list/valuation reads by design.)
	int getFlatYield(YieldTypes eYield, CvCascScope eScope) const
	{ return m_modifiers.sum(infoYieldFamily(eYield), CHANNEL_AMOUNT, eScope, CASC_UNIT_FLAT); }
	int getFlatCommerce(CommerceTypes eCommerce, CvCascScope eScope) const
	{ return m_modifiers.sum(infoCommerceFamily(eCommerce), CHANNEL_AMOUNT, eScope, CASC_UNIT_FLAT); }
	// The authored wellbeing families' SIGNED sums. ⛔ ANGER/UNHEALTH read 0 here BY CONSTRUCTION and that is
	// never a gap to chase: an INFO keeps a negative in its POSITIVE family (happiness -1, not anger +1) --
	// the sign ROUTING to the opposing channel happens at FILL, on the city PACKAGE, not on authored data
	// (modifier.md §2b). So this read already carries the negatives; there is nothing to verify in the JSON.
	int getFlatWellbeing(WellbeingChannel eChannel, CvCascScope eScope) const
	{
		if (eChannel == WELLBEING_ANGER || eChannel == WELLBEING_UNHEALTH)
		{
			return 0;
		}
		return m_modifiers.sum(infoWellbeingFamily(eChannel), CHANNEL_AMOUNT, eScope, CASC_UNIT_FLAT);
	}
	int getExperience(ExperienceKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_EXPERIENCE, eKind, eScope, infoKindUnit(MODFAM_EXPERIENCE, eKind, eScope)); }
	// experience.city.unitCombats.{UNITCOMBAT_*}.flat -- the XP this specialist gives units of ONE combat
	// class. A KEYED deposit, so it is read per target and NEVER folded scope-wide (modifier.md §5: that
	// fold hands every unit the one class's XP). Materialized at mapFrom; 0 = none.
	int getExperienceForUnitCombat(int iUnitCombat) const
	{
		std::map<int, int>::const_iterator it = m_unitCombatExperience.find(iUnitCombat);
		return it != m_unitCombatExperience.end() ? it->second : 0;
	}
	int getUnderworld(UnderworldKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_UNDERWORLD, eKind, eScope, CASC_UNIT_FLAT); }
	// (greatPeopleRate is a 1-kind straggler: the base getScalar(SCALAR_GREAT_PEOPLE_RATE) covers it. The
	// experience.city.unitCombats.{UNITCOMBAT_*} keyed entries are entry-list reads by design -- and their
	// GAMEOPTION_UNIT_XP_FROM_SPECIALISTS gate belongs to the CONSUMING system, never inside an info read,
	// json §9: an info serves ungated data.)

	// ======================= 4. INTRINSIC -- bare typed reads (the census identity set) ======================
	int getGreatPeopleUnitType() const { return m_iGreatPeopleUnitType; }   // identity.greatPeopleUnit (UNIT_* FK)
	bool isSlave() const { return m_bSlave; }                               // identity.slave
	bool isVisible() const { return m_bVisible; }                           // identity.visible (assignable on the city screen)
	const std::vector<int>& getCategories() const { return m_aiCategories; }   // identity.categories (CATEGORY_* FKs)
	const char* getTexture() const { return m_szTexture.c_str(); }          // ui.art.texture (the city-screen glyph)
	int getFlavorValue(int iFlavor) const;                                  // ai.flavours [{FLAVOR_X: n}]

	// Fed from the PROPERTY_* families in mapFrom (city gather, per assigned specialist, RELATION_SAME_PLOT).
	const CvPropertyManipulators* getPropertyManipulators() const { return &m_PropertyManipulators; }

	// --- RUNTIME member (set post-load, NOT JSON) ---
	int getMissionType() const { return m_iMissionType; }
	void setMissionType(int iMission) { m_iMissionType = iMission; }

protected:
	virtual CvModifiers* mutModifiers() { return &m_modifiers; }

private:
	std::map<int, int> m_unitCombatExperience;   // keyed experience, materialized at mapFrom
	// --- the composed section unit ---
	CvModifiers m_modifiers;

	// --- the intrinsic identity members (materialized once at mapFrom) ---
	int m_iGreatPeopleUnitType;
	bool m_bSlave;
	bool m_bVisible;
	std::vector<int> m_aiCategories;
	std::string m_szTexture;
	std::map<int, int> m_flavours;
	CvPropertyManipulators m_PropertyManipulators;
	int m_iMissionType;   // runtime
};

#endif // CV_JSON_SPECIALIST_INFO_H
