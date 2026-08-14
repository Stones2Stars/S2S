//
//	CascadePropertyBridge -- the JSON->legacy-expression translator (property-audit.md increment 4).
//	See the header for the contract; everything here fails CLOSED (NULL) so a caller can never over-apply a
//	condition it cannot represent.
//

#include "CvGameCoreDLL.h"
#include "Property/CvPropertyBridge.h"
#include "CvModEntry.h"                 // CvModEntry + CvCondition (JsonInfo, on /I)
#include "CvModifiers.h"                // the per-poco family map bridgeFamilies walks
#include "CvTriggers.h"                     // the `triggers` PROPERTY pulses bridgePulses walks
#include "CvJsonParse.h"                    // jsonResolveId + jsonNoteUnconsumed -- the FK + the skip census
#include "CvPropertyInfo.h"                 // the skipped deposit's PROPERTY_X type for the census attribution
#include "Engine/CvPropertyManipulators.h"
#include "Infrastructure/BoolExpr.h"
#include "Infrastructure/IntExpr.h"

namespace
{
	// The presence-atom prefix -> GOM table. Only kinds whose city/plot hasGOM semantic matches the json implied
	// scope (tech -> team, civic -> owner, bonus/religion/corporation -> city, building -> active-in-city).
	GOMTypes gomFromType(const std::string& szType)
	{
		if (szType.compare(0, 5, "TECH_") == 0)         return GOM_TECH;
		if (szType.compare(0, 6, "BONUS_") == 0)        return GOM_BONUS;
		if (szType.compare(0, 9, "BUILDING_") == 0)     return GOM_BUILDING;
		if (szType.compare(0, 6, "CIVIC_") == 0)        return GOM_CIVIC;
		if (szType.compare(0, 9, "RELIGION_") == 0)     return GOM_RELIGION;
		if (szType.compare(0, 12, "CORPORATION_") == 0) return GOM_CORPORATION;
		return NO_GOM;
	}

	// Fold a child list with a binary combiner. Returns NULL (and cleans up) if ANY child fails to translate --
	// a partially-represented AND/OR would apply the source under the WRONG condition, so the whole group fails.
	template <class COMBINE>
	const BoolExpr* foldChildren(const std::vector<CvCondition*>& children)
	{
		const BoolExpr* pAcc = NULL;
		for (size_t i = 0; i < children.size(); ++i)
		{
			const BoolExpr* pChild = CascadePropertyBridge::condToBoolExpr(children[i]);
			if (pChild == NULL)
			{
				delete pAcc;   // composite dtors cascade
				return NULL;
			}
			pAcc = (pAcc == NULL) ? pChild : new COMBINE(pAcc, pChild);
		}
		return pAcc;
	}
}

const BoolExpr* CascadePropertyBridge::condToBoolExpr(const CvCondition* pCond)
{
	if (pCond == NULL) return NULL;

	switch (pCond->kind)
	{
	case CASC_COND_PRESENCE:
	{
		// PROPERTY_* band atom (json §3.4 -- its "count" is the property VALUE): min -> value >= min,
		// max -> NOT(value > max), both -> AND. Maps onto the legacy comparison nodes over IntExprProperty
		// (the trait crime-band gates ride this).
		if (pCond->type.compare(0, 9, "PROPERTY_") == 0)
		{
			if (pCond->id < 0 || (pCond->min == -1 && pCond->max == -1)) return NULL;
			const BoolExpr* pAcc = NULL;
			if (pCond->min != -1)
				pAcc = new BoolExprGreaterEqual(new IntExprProperty((PropertyTypes)pCond->id), new IntExprConstant(pCond->min));
			if (pCond->max != -1)
			{
				const BoolExpr* pNotAbove = new BoolExprNot(
					new BoolExprGreater(new IntExprProperty((PropertyTypes)pCond->id), new IntExprConstant(pCond->max)));
				pAcc = (pAcc == NULL) ? pNotAbove : (const BoolExpr*)new BoolExprAnd(pAcc, pNotAbove);
			}
			return pAcc;
		}
		// Plain presence only: a count threshold (min>1 / max) or a connection qualifier has no BoolExprHas
		// equivalent -- fail closed.
		if (pCond->max != -1 || pCond->min > 1 || pCond->connection != CASC_CONN_NONE || pCond->id < 0)
			return NULL;
		const GOMTypes eGOM = gomFromType(pCond->type);
		if (eGOM == NO_GOM) return NULL;
		return new BoolExprHas(eGOM, pCond->id);
	}
	case CASC_COND_GROUP:
	{
		// A group node's own enabled/disabled sub-conditions do not occur on deposit conditions; fail closed.
		if (pCond->enabled != NULL || pCond->disabled != NULL) return NULL;
		if (pCond->all.empty() && pCond->anyOf.empty() && pCond->noneOf.empty()) return NULL;

		const BoolExpr* pAll = NULL;
		const BoolExpr* pAny = NULL;
		if (!pCond->all.empty())
		{
			pAll = foldChildren<BoolExprAnd>(pCond->all);
			if (pAll == NULL) return NULL;
		}
		if (!pCond->anyOf.empty())
		{
			pAny = foldChildren<BoolExprOr>(pCond->anyOf);
			if (pAny == NULL) { delete pAll; return NULL; }
		}
		const BoolExpr* pAcc = pAll;
		if (pAny != NULL) pAcc = (pAcc == NULL) ? pAny : new BoolExprAnd(pAcc, pAny);
		if (!pCond->noneOf.empty())
		{
			const BoolExpr* pNone = foldChildren<BoolExprOr>(pCond->noneOf);
			if (pNone == NULL) { delete pAcc; return NULL; }
			const BoolExpr* pNot = new BoolExprNot(pNone);
			pAcc = (pAcc == NULL) ? pNot : new BoolExprAnd(pAcc, pNot);
		}
		return pAcc;
	}
	case CASC_COND_PREDICATE:
		// No building/unit property deposit gates on a predicate today; the diffuse tag-gates are raw strings
		// (predStringToBoolExpr). Fail closed until a real case names its mapping.
		return NULL;
	}
	return NULL;
}

const BoolExpr* CascadePropertyBridge::entryActiveExpr(const CvModEntry* pEntry, bool* pbUntranslatable)
{
	*pbUntranslatable = false;
	const BoolExpr* pActive = NULL;
	if (pEntry->enabled != NULL)
	{
		pActive = condToBoolExpr(pEntry->enabled);
		if (pActive == NULL) { *pbUntranslatable = true; return NULL; }
	}
	if (pEntry->disabled != NULL)
	{
		const BoolExpr* pDis = condToBoolExpr(pEntry->disabled);
		if (pDis == NULL) { delete pActive; *pbUntranslatable = true; return NULL; }
		const BoolExpr* pNot = new BoolExprNot(pDis);
		pActive = (pActive == NULL) ? pNot : (const BoolExpr*)new BoolExprAnd(pActive, pNot);
	}
	return pActive;
}

const BoolExpr* CascadePropertyBridge::predStringToBoolExpr(const std::string& szPred)
{
	if (szPred == "IS_OWNED") return new BoolExprIs(TAG_OWNED);
	if (szPred == "HAS_PEAK") return new BoolExprIs(TAG_PEAK);
	if (szPred == "IS_WATER") return new BoolExprIs(TAG_WATER);
	if (szPred == "IS_CITY")  return new BoolExprIs(TAG_CITY);
	return NULL;
}

void CascadePropertyBridge::discard(const BoolExpr* pExpr)
{
	delete pExpr;
}

const IntExpr* CascadePropertyBridge::perPopulationAmount(int iValue, int iEach)
{
	const IntExpr* pAmount = new IntExprMult(new IntExprConstant(iValue), new IntExprAttribute(ATTRIBUTE_POPULATION));
	if (iEach > 1) pAmount = new IntExprDiv(pAmount, new IntExprConstant(iEach));
	return pAmount;
}

// The census attribution for a skipped deposit: "<PROPERTY_X>.<reason>" against the SOURCE's own type, so the
// [READJSON] unconsumed-section line is a worklist entry rather than a count. The property registry is complete
// before any carrier maps (same-pass registration precedes every mapFrom), so the type read is total.
static std::string pb_skipSection(int iPropertyFk, const char* szReason)
{
	const char* szProp = GC.getPropertyInfo((PropertyTypes)iPropertyFk).getType();
	return std::string(szProp != NULL ? szProp : "PROPERTY_?") + "." + szReason;
}

void CascadePropertyBridge::bridgeFamilies(const CvModifiers* pMods, CvPropertyManipulators& kTarget,
	RelationTypes eRelation, int iRelationData, CvPropertyManipulators* pEmpireTarget, const char* szSourceType)
{
	kTarget.clear();
	if (pEmpireTarget != NULL) pEmpireTarget->clear();
	if (pMods == NULL) return;

	const std::string szSource = (szSourceType != NULL) ? szSourceType : "propertyBridge";
	const std::vector<CvModEntry*>& entries = pMods->entries();
	for (size_t i = 0; i < entries.size(); ++i)
	{
		const CvModEntry* e = entries[i];
		if (e->family != MODFAM_PROPERTY || e->propertyFk < 0) continue;
		if (e->nSeg != 2) continue;   // PROPERTY_X.<scope> exactly -- a deeper/targeted address is not an own-source
		if (e->scope != CASC_SCOPE_CITY && e->scope != CASC_SCOPE_PLOT && e->scope != CASC_SCOPE_EMPIRE) continue;
		if (e->scope == CASC_SCOPE_EMPIRE && pEmpireTarget == NULL)
		{
			// an empire-scope authoring on a carrier with no all-cities container -- authored data with no delivery
			jsonNoteUnconsumed(szSource, pb_skipSection(e->propertyFk, "empireScopeNoContainer"));
			continue;
		}
		if (e->unit != CASC_UNIT_FLAT) continue;
		const GameObjectTypes eObj = (e->scope == CASC_SCOPE_PLOT) ? GAMEOBJECT_PLOT : GAMEOBJECT_CITY;
		CvPropertyManipulators& manip = (e->scope == CASC_SCOPE_EMPIRE) ? *pEmpireTarget : kTarget;
		bool bUntranslatable = false;
		const BoolExpr* pActive = entryActiveExpr(e, &bUntranslatable);
		if (bUntranslatable)
		{
			// the gate refuses what it cannot faithfully translate -- correct; loading and never applying with
			// NO report is not (triggers.md: fail-closed AND silent is invisible on both axes at once)
			jsonNoteUnconsumed(szSource, pb_skipSection(e->propertyFk, "conditionUntranslatable"));
			continue;
		}
		if (e->hasPer)
		{
			if (e->perType != "POPULATION")
			{
				jsonNoteUnconsumed(szSource, pb_skipSection(e->propertyFk, "perNotPopulation"));
				delete pActive;
				continue;
			}
			if (e->perEach <= 1 && pActive == NULL)
				manip.addAttributeConstantSource((PropertyTypes)e->propertyFk, ATTRIBUTE_POPULATION, e->value / 100, eObj);
			else
				manip.addConstantSource((PropertyTypes)e->propertyFk,
					perPopulationAmount(e->value / 100, e->perEach), eObj, eRelation, iRelationData, pActive);
		}
		else manip.addConstantSource((PropertyTypes)e->propertyFk, e->value / 100, eObj, eRelation, iRelationData, pActive);
	}
}

void CascadePropertyBridge::bridgePulses(const CvTriggers* pTriggers, CvPropertyManipulators& kTarget,
	const char* szSourceType)
{
	kTarget.clear();
	if (pTriggers == NULL) return;

	const std::string szSource = (szSourceType != NULL) ? szSourceType : "propertyBridge";
	const std::vector<CvTriggerEntry*>& entries = pTriggers->entries();
	for (size_t i = 0; i < entries.size(); ++i)
	{
		const CvTriggerEntry* pEntry = entries[i];
		if (pEntry->propertyId < 0) continue;
		// plain per-turn pulses only -- a chance-rolled, interval>1, or non-turn entry is not a constant
		// source (none authored on features/improvements); fail closed.
		if (pEntry->happening != "onTurn" || pEntry->happeningInterval != 1) { jsonNoteUnconsumed(szSource, pb_skipSection(pEntry->propertyId, "pulseNotPerTurnConstant")); continue; }
		if (pEntry->chanceValue != 0 || pEntry->chancePerTypeId >= 0 || !pEntry->chancePerToken.empty()) { jsonNoteUnconsumed(szSource, pb_skipSection(pEntry->propertyId, "pulseChanceRolled")); continue; }
		const BoolExpr* pActive = NULL;
		if (pEntry->condition != NULL)
		{
			pActive = condToBoolExpr(pEntry->condition);
			if (pActive == NULL) { jsonNoteUnconsumed(szSource, pb_skipSection(pEntry->propertyId, "pulseConditionUntranslatable")); continue; }
		}
		const GameObjectTypes eObj = (pEntry->spatialOn == "plot") ? GAMEOBJECT_PLOT : GAMEOBJECT_CITY;
		const RelationTypes eRel = (pEntry->spatialRelation == "near") ? RELATION_NEAR
			: (pEntry->spatialRelation == "same" || pEntry->spatialRelation == "samePlot") ? RELATION_SAME_PLOT : NO_RELATION;
		kTarget.addConstantSource((PropertyTypes)pEntry->propertyId, pEntry->propertyAmount / 100, eObj, eRel, pEntry->spatialDistance, pActive);
	}
}
