#pragma once
#ifndef CV_CASCADE_COUNT_EXPR_H
#define CV_CASCADE_COUNT_EXPR_H

//
//	IntExprCascadeCount -- a cascade IntExpr LEAF that resolves the cross-city (empire) COUNT of a building|unit type
//	for the EVALUATED game object's owner, read from the live cascade TALLY (the count machine, tally.md).
//
//	⛔ HOME = the LIVE-GAME-STATE side, NOT readJson (owner ruling 2026-06-30: "gameobject cares only about live game
//	state -- cascades/tally/eventspine; readJson cares about the static data"). readJson produces only STATIC info data,
//	so it CONSTRUCTS this leaf as a data node (a count-band condition) but must never DEFINE evaluation that walks a
//	CvGameObject. The info -> instance -> owner -> tally conversion is the engine's job, done here in evaluate() at eval
//	time -- exactly like the engine's own IntExprProperty / IntExprAttribute value leaves. This is why readJson no
//	longer includes CvGameObject.h: building a BoolExpr/IntExpr tree (the deliberate reuse) is fine; OWNING an
//	instance-walking leaf is what leaked live state into the static reader.
//
//	EMPIRE scope (the tally's domain). Team/world rollup, city-local counts, and non-building/unit domains are
//	follow-ons (return 0 until their tally domain is added). The enabler that evaluates this in a live gate is a later
//	increment; today only the readJson probe builds + renders it.
//

#include "Infrastructure/IntExpr.h"   // the IntExpr base (forward-declares CvGameObject) + CvWStringBuffer/uint32_t

class IntExprCascadeCount : public IntExpr
{
public:
	IntExprCascadeCount(GOMTypes eGOM, int iID) : m_eGOM(eGOM), m_iID(iID) {}
	virtual int evaluate(const CvGameObject* pObject) const;
	virtual void getCheckSum(uint32_t& iSum) const;
	virtual void buildDisplayString(CvWStringBuffer& szBuffer) const;
	virtual int getBindingStrength() const;
private:
	GOMTypes m_eGOM;
	int m_iID;
};

#endif // CV_CASCADE_COUNT_EXPR_H
