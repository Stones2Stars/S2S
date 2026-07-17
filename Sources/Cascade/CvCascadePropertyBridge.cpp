//
//	CascadePropertyBridge -- the JSON->legacy-expression translator (property-audit.md increment 4).
//	See the header for the contract; everything here fails CLOSED (NULL) so a caller can never over-apply a
//	condition it cannot represent.
//

#include "CvGameCoreDLL.h"
#include "CvCascadePropertyBridge.h"
#include "CvJsonModEntry.h"                 // CvJsonModEntry + CvJsonCondition (JsonInfo, on /I)
#include "CvJsonModifiers.h"                // the per-poco family map bridgeFamilies walks
#include "CvJsonGrants.h"                   // the repeatable PROPERTY pulses bridgePulses walks
#include "CvJsonParse.h"                    // jsonResolveId -- the PROPERTY_X family-key FK
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
	const BoolExpr* foldChildren(const std::vector<CvJsonCondition*>& children)
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

const BoolExpr* CascadePropertyBridge::condToBoolExpr(const CvJsonCondition* pCond)
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

const BoolExpr* CascadePropertyBridge::entryActiveExpr(const CvJsonModEntry* pEntry, bool* pbUntranslatable)
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

void CascadePropertyBridge::bridgeFamilies(const CvJsonModifiers* pMods, CvPropertyManipulators& kTarget,
	RelationTypes eRelation, int iRelationData, CvPropertyManipulators* pEmpireTarget)
{
	kTarget.clear();
	if (pEmpireTarget != NULL) pEmpireTarget->clear();
	if (pMods == NULL) return;

	const std::map<std::string, CvJsonModFamily*>& all = pMods->all();
	for (std::map<std::string, CvJsonModFamily*>::const_iterator mit = all.begin(); mit != all.end(); ++mit)
	{
		const std::string& addr = mit->first;
		if (addr.compare(0, 9, "PROPERTY_") != 0) continue;
		const size_t dot = addr.find('.');
		if (dot == std::string::npos) continue;
		const std::string scope = addr.substr(dot + 1);
		if (scope != "city" && scope != "plot" && scope != "empire") continue;
		if (scope == "empire" && pEmpireTarget == NULL) continue;   // no empire entries authored off buildings
		const int eProp = jsonResolveId(addr.substr(0, dot));
		if (eProp < 0) continue;
		const GameObjectTypes eObj = (scope == "plot") ? GAMEOBJECT_PLOT : GAMEOBJECT_CITY;
		CvPropertyManipulators& manip = (scope == "empire") ? *pEmpireTarget : kTarget;
		const CvJsonModFamily* f = mit->second;
		for (int i = 0; i < f->size(); ++i)
		{
			const CvJsonModEntry* e = f->entries[i];
			if (e->unit != CASC_UNIT_FLAT) continue;
			bool bUntranslatable = false;
			const BoolExpr* pActive = entryActiveExpr(e, &bUntranslatable);
			if (bUntranslatable) continue;
			if (e->hasPer)
			{
				if (e->perType != "POPULATION") { delete pActive; continue; }   // no non-POPULATION per authored
				if (e->perEach <= 1 && pActive == NULL)
					manip.addAttributeConstantSource((PropertyTypes)eProp, ATTRIBUTE_POPULATION, e->value100 / 100, eObj);
				else
					manip.addConstantSource((PropertyTypes)eProp,
						perPopulationAmount(e->value100 / 100, e->perEach), eObj, eRelation, iRelationData, pActive);
			}
			else manip.addConstantSource((PropertyTypes)eProp, e->value100 / 100, eObj, eRelation, iRelationData, pActive);
		}
	}
}

void CascadePropertyBridge::bridgePulses(const CvJsonGrants* pGrants, CvPropertyManipulators& kTarget)
{
	kTarget.clear();
	if (pGrants == NULL) return;

	const std::vector<CvJsonGrantRepeatable*>& reps = pGrants->repeatables();
	for (size_t i = 0; i < reps.size(); ++i)
	{
		const CvJsonGrantRepeatable* r = reps[i];
		if (r->propertyId < 0) continue;
		// plain per-turn pulses only -- a chance-rolled or interval>1 pulse is not a constant source (none
		// authored on features/improvements); fail closed.
		if (r->intervalPerTurn != 1 || r->chanceValue100 != 0 || r->chancePerId >= 0 || !r->chancePerToken.empty()) continue;
		const BoolExpr* pActive = NULL;
		if (r->enabled != NULL)
		{
			pActive = condToBoolExpr(r->enabled);
			if (pActive == NULL) continue;
		}
		const GameObjectTypes eObj = (r->on == "plot") ? GAMEOBJECT_PLOT : GAMEOBJECT_CITY;
		const RelationTypes eRel = (r->relation == "near") ? RELATION_NEAR
			: (r->relation == "same" || r->relation == "samePlot") ? RELATION_SAME_PLOT : NO_RELATION;
		kTarget.addConstantSource((PropertyTypes)r->propertyId, r->amount100 / 100, eObj, eRel, r->distance, pActive);
	}
}
