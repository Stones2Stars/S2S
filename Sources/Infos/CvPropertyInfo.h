#pragma once
#ifndef CV_JSON_PROPERTY_INFO_H
#define CV_JSON_PROPERTY_INFO_H

//
//	CvPropertyInfo -- the JSON poco for PROPERTIES (crime/pollution/…-class plot+city scalars; uniformity
//	ruling: every info type has its own CvJson<X>Info home). Composes the section units the property data authors:
//	`grants` (the granted effect-building LIST) + its modifier families (the decay/constant per-turn change), both
//	served by the CvInfo base dispatch. mapFrom adds the TYPED scalars that live outside those sections: the AI
//	value-normalization band (ai.weight / ai.operationalRange / ai.trainReluctance), targetLevel (+ its per-era
//	byEra overrides), and the identity.sourceDrain flag. getChar is the one non-authored member: a runtime GameFont
//	glyph the CvGameTextMgr symbol pass assigns through setChar.
//

#include "CvInfo.h"
#include "Engine/CvPropertyManipulators.h"    // CvPropertyManipulators
#include <map>

class CvPropertyInfo : public CvInfo
{
public:
	CvPropertyInfo();

	// --- mirrored legacy CvPropertyInfo getters (live consumer surface across Sources/) ---
	int getAIWeight() const { return m_iAIWeight; }                   // ai.weight (raw, AI-native; not x100)
	AIScaleTypes getAIScaleType() const { return m_eAIScaleType; }    // ai.scale ("city"/"area"/"player"/"team")
	CvWString getPrereqMinDisplayText() const { return m_szPrereqMinDisplayText; }   // identity.text.prereqMin (TXT_KEY)
	CvWString getPrereqMaxDisplayText() const { return m_szPrereqMaxDisplayText; }   // identity.text.prereqMax (TXT_KEY)
	// The change-propagation table (properties.changePropagation[]): a VALUE change on `eFrom` propagates
	// percent-scaled onto every related `eTo` object (CvProperties::propagateChange). FLAMMABILITY's
	// City->Player 100% rollup is the one authored row.
	int getChangePropagator(GameObjectTypes eFrom, GameObjectTypes eTo) const
	{
		const std::map<int,int>::const_iterator it = m_changePropagation.find((int)eFrom * NUM_GAMEOBJECTS + (int)eTo);
		return it != m_changePropagation.end() ? it->second : 0;
	}
	int getFontButtonIndex() const { return m_iFontButtonIndex; }    // identity.fontButtonIndex (raw int)
	void setChar(int i) { m_iChar = i; }             // stores the glyph the CvGameTextMgr symbol pass assigns (getPropertyInfo(i).setChar, CvGameTextMgr.cpp:29147)
	int getOperationalRangeMin() const { return m_iOperationalRangeMin; }   // ai.operationalRange.min
	int getOperationalRangeMax() const { return m_iOperationalRangeMax; }   // ai.operationalRange.max
	int getTargetLevel() const { return m_iTargetLevel; }                   // targetLevel (flat, or targetLevel.base)
	int getTrainReluctance() const { return m_iTrainReluctance; }           // ai.trainReluctance
	bool isSourceDrain() const { return m_bSourceDrain; }                   // identity.sourceDrain

	// getChar is a RUNTIME GameFont glyph (non-XML): the CvGameTextMgr symbol pass assigns it via setChar (stored in
	// m_iChar), exactly as the archived class. Not curated; reproduced via the live setter, never a defaulted 0.
	int getChar() const { return m_iChar; }

	int getTargetLevelbyEraType(int iIndex) const   // targetLevel.byEra { ERA_x: n }
	{ std::map<int,int>::const_iterator it = m_aTargetLevelbyEraTypes.find(iIndex); return it != m_aTargetLevelbyEraTypes.end() ? it->second : 0; }
	bool isTargetLevelbyEraType(int iIndex) const   // targetLevel.byEra membership
	{ return m_aTargetLevelbyEraTypes.find(iIndex) != m_aTargetLevelbyEraTypes.end(); }

	const CvPropertyManipulators* getPropertyManipulators() const { return &m_PropertyManipulators; }       // DEFERRED SYSTEM (property engine; XML-era manip data, owner: post-migration rework)

	virtual void mapFrom(const picojson::value& entity);

	// --- the composed section units (by value; the base's mapFrom dispatch writes them via mut*) ---
	virtual const CvModifiers* getModifiers() const { return &m_modifiers; }

	virtual const CvTriggers*  getTriggers()  const { return &m_triggers; }   // §5 -- triggers + the folded grants

protected:
	virtual CvTriggers*  mutTriggers()  { return &m_triggers; }
	virtual CvModifiers* mutModifiers() { return &m_modifiers; }

private:
	CvTriggers  m_triggers;
	CvModifiers m_modifiers;
	int  m_iAIWeight;                                       // ai.weight
	AIScaleTypes m_eAIScaleType;                            // ai.scale
	int  m_iFontButtonIndex;                               // identity.fontButtonIndex
	int  m_iOperationalRangeMin, m_iOperationalRangeMax;    // ai.operationalRange.min/max
	int  m_iTargetLevel, m_iTrainReluctance;               // targetLevel(.base) / ai.trainReluctance
	bool m_bSourceDrain;                                    // identity.sourceDrain
	int  m_iChar;                                          // runtime GameFont glyph (assigned by the symbol pass via setChar)
	std::map<int,int> m_aTargetLevelbyEraTypes;            // eraId -> level (targetLevel.byEra)
	CvWString m_szPrereqMinDisplayText;                    // identity.text.prereqMin
	CvWString m_szPrereqMaxDisplayText;                    // identity.text.prereqMax
	CvPropertyManipulators m_PropertyManipulators;          // the property's own sources/propagators (fed from JSON in mapFrom)
	std::map<int,int> m_changePropagation;                  // (from x NUM_GAMEOBJECTS + to) -> percent (properties.changePropagation[])
};

#endif // CV_JSON_PROPERTY_INFO_H
