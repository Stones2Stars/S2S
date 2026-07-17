//
//	JsonModScan -- see the header. The ONE load-time scan over a poco's composed modifier families, feeding the
//	mapFrom materialization pass. Lifted from the per-file civSum*/sumUnconditioned duplicates ([DEC-single-implementation]).
//

#include "CvJsonModScan.h"
#include "CvJsonCondition.h"

int JsonModScan::familyUnconditioned100(const CvJsonModFamily* f, CvCascUnit unit)
{
	if (!f) return 0;
	int total100 = 0;
	for (int i = 0; i < f->size(); ++i)
	{
		const CvJsonModEntry* e = f->entries[i];
		if (e->unit == unit && e->enabled == NULL && e->disabled == NULL && !e->hasPer && e->unitQual == NULL)
			total100 += e->value100;
	}
	return total100;
}

int JsonModScan::sum100(const CvJsonModifiers* mods, const std::string& address, CvCascUnit unit)
{
	return familyUnconditioned100(mods ? mods->find(address) : NULL, unit);
}

int JsonModScan::sumKeyed(const CvJsonModifiers* mods, const std::string& baseAddr, const char* typeStr, CvCascUnit unit)
{
	if (!mods || typeStr == NULL || *typeStr == '\0') return 0;
	return sum(mods, baseAddr + "." + typeStr, unit);
}

bool JsonModScan::hasPrefixedFamily(const CvJsonModifiers* mods, const std::string& prefix)
{
	if (!mods) return false;
	const std::map<std::string, CvJsonModFamily*>& all = mods->all();
	for (std::map<std::string, CvJsonModFamily*>::const_iterator it = all.begin(); it != all.end(); ++it)
		if (it->first.compare(0, prefix.size(), prefix) == 0) return true;
	return false;
}

int JsonModScan::sumAll(const CvJsonModifiers* mods, const std::string& address, CvCascUnit unit)
{
	const CvJsonModFamily* f = mods ? mods->find(address) : NULL;
	if (!f) return 0;
	int total100 = 0;
	for (int i = 0; i < f->size(); ++i)
		if (f->entries[i]->unit == unit) total100 += f->entries[i]->value100;
	return total100 / 100;
}

int JsonModScan::sumUnitQualified(const CvJsonModifiers* mods, const std::string& address, CvCascUnit unit)
{
	const CvJsonModFamily* f = mods ? mods->find(address) : NULL;
	if (!f) return 0;
	int total100 = 0;
	for (int i = 0; i < f->size(); ++i)
	{
		const CvJsonModEntry* e = f->entries[i];
		if (e->unit == unit && e->unitQual != NULL && e->enabled == NULL && e->disabled == NULL && !e->hasPer)
			total100 += e->value100;
	}
	return total100 / 100;
}

int JsonModScan::sumEnabledPred(const CvJsonModifiers* mods, const std::string& address, CvCascUnit unit, CvCascPredKind k)
{
	const CvJsonModFamily* f = mods ? mods->find(address) : NULL;
	if (!f) return 0;
	int total100 = 0;
	for (int i = 0; i < f->size(); ++i)
	{
		const CvJsonModEntry* e = f->entries[i];
		const CvJsonCondition* c = e->enabled;
		if (e->unit == unit && e->disabled == NULL && !e->hasPer
		 && c != NULL && c->kind == CASC_COND_PREDICATE && c->predKind == k)
			total100 += e->value100;
	}
	return total100 / 100;
}

int JsonModScan::sumEnabledPresence(const CvJsonModifiers* mods, const std::string& address, CvCascUnit unit, const char* typeStr)
{
	const CvJsonModFamily* f = mods ? mods->find(address) : NULL;
	if (!f) return 0;
	int total100 = 0;
	for (int i = 0; i < f->size(); ++i)
	{
		const CvJsonModEntry* e = f->entries[i];
		const CvJsonCondition* c = e->enabled;
		if (e->unit == unit && e->disabled == NULL && !e->hasPer
		 && c != NULL && c->kind == CASC_COND_PRESENCE && c->type == typeStr)
			total100 += e->value100;
	}
	return total100 / 100;
}
