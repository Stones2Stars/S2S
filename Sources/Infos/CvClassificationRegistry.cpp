//
//	ClassificationRegistry -- see the header. Runtime-generated classification infos (SKILL_/TAG_/ATTRIBUTE_/
//	AMENITY_/CAPABILITY_/POLICY_) minted from the union of authored §8/§9 block keys, + the per-entity id-plane resolve.
//

#include "CvGameCoreDLL.h"
#include "CvClassificationRegistry.h"
#include "CvClassificationIds.h"   // the GENERATED seed table (curate_classification_ids.py)
#include "CvInfo.h"
#include "Repos/InfoRepo.h"

// The five generated-domain repo tags (phantom discriminators; payload = the RepoPayload default, plain CvInfo).
// Their InfoRepo singletons are OWNED rows in InfoRepo.cpp.
class CvSkillClsTag; class CvTagClsTag; class CvAttributeClsTag; class CvCapabilityClsTag; class CvPolicyClsTag;
class CvCharacteristicClsTag; class CvAmenityClsTag;
class CvCanTradeClsTag; class CvCanWorkOnClsTag;

namespace
{
	const char* CLS_PREFIX[NUM_CLS_DOMAINS] = { "SKILL_", "TAG_", "ATTRIBUTE_", "AMENITY_", "CHARACTERISTIC_", "CAPABILITY_", "POLICY_",
		"CANTRADE_", "CANWORKON_" };

	// per-domain minted state -- APPEND-ONLY process-wide (ids never shift across the premenu/postmenu passes)
	std::vector<std::string> s_keys[NUM_CLS_DOMAINS];              // [id] -> camelCase key
	std::map<std::string, int> s_keyToId[NUM_CLS_DOMAINS];         // camelCase key -> id
	int s_buildCount = 0;                                          // completed buildAndResolve passes

	// camelCase / snake_case key -> UPPER_SNAKE: '_' before an upper that starts a word (prev lower, or next
	// lower after an acronym run) and before a digit run following a letter. "setScienceRate" -> SET_SCIENCE_RATE,
	// "maxHP" -> MAX_HP, "dcmAirBomb" -> DCM_AIR_BOMB, "is_cargo_vessel" -> IS_CARGO_VESSEL.
	std::string clsUpperSnake(const std::string& key)
	{
		std::string out;
		out.reserve(key.size() + 8);
		for (size_t i = 0; i < key.size(); ++i)
		{
			const char c = key[i];
			const bool bUp = c >= 'A' && c <= 'Z';
			const bool bDigit = c >= '0' && c <= '9';
			if (i > 0)
			{
				const char p = key[i - 1];
				const bool bPrevLower = p >= 'a' && p <= 'z';
				const bool bPrevUpper = p >= 'A' && p <= 'Z';
				const bool bPrevDigit = p >= '0' && p <= '9';
				const bool bNextLower = i + 1 < key.size() && key[i + 1] >= 'a' && key[i + 1] <= 'z';
				if ((bUp && (bPrevLower || bPrevDigit || (bPrevUpper && bNextLower)))
				 || (bDigit && (bPrevLower || bPrevUpper)))
					out += '_';
			}
			out += (char)toupper((unsigned char)c);
		}
		return out;
	}

	// domain -> its generated-info repo's editPtr (get-or-create at id)
	CvInfo* clsRepoEdit(int eDomain, int iId)
	{
		switch (eDomain)
		{
		case CLSD_SKILL:      return InfoRepo<CvSkillClsTag>::get().editPtr(iId);
		case CLSD_TAG:        return InfoRepo<CvTagClsTag>::get().editPtr(iId);
		case CLSD_ATTRIBUTE:  return InfoRepo<CvAttributeClsTag>::get().editPtr(iId);
		case CLSD_AMENITY:    return InfoRepo<CvAmenityClsTag>::get().editPtr(iId);
		case CLSD_CHARACTERISTIC: return InfoRepo<CvCharacteristicClsTag>::get().editPtr(iId);
		case CLSD_CAPABILITY: return InfoRepo<CvCapabilityClsTag>::get().editPtr(iId);
		case CLSD_POLICY:     return InfoRepo<CvPolicyClsTag>::get().editPtr(iId);
		case CLSD_CANTRADE:   return InfoRepo<CvCanTradeClsTag>::get().editPtr(iId);
		case CLSD_CANWORKON:  return InfoRepo<CvCanWorkOnClsTag>::get().editPtr(iId);
		}
		return NULL;
	}

	// domain -> the entity's authored block (const view; NULL = the type does not carry the section)
	const CvClassificationBlock* clsBlockOf(const CvInfo* d, int eDomain)
	{
		switch (eDomain)
		{
		case CLSD_SKILL:      return d->getSkills();
		case CLSD_TAG:        return d->getTags();
		case CLSD_ATTRIBUTE:  return d->getAttributes();
		case CLSD_AMENITY:    return d->getAmenities();
		case CLSD_CHARACTERISTIC: return d->getCharacteristics();
		case CLSD_CAPABILITY: return d->getCapabilities();
		case CLSD_POLICY:     return d->getPolicies();
		case CLSD_CANTRADE:   return d->getCanTrade();
		case CLSD_CANWORKON:  return d->getCanWorkOn();
		}
		return NULL;
	}
}

int ClassificationRegistry::count(int eDomain)
{
	return (eDomain >= 0 && eDomain < NUM_CLS_DOMAINS) ? (int)s_keys[eDomain].size() : 0;
}

const char* ClassificationRegistry::prefix(int eDomain)
{
	return (eDomain >= 0 && eDomain < NUM_CLS_DOMAINS) ? CLS_PREFIX[eDomain] : "";
}

std::string ClassificationRegistry::typeName(int eDomain, const std::string& szKey)
{
	return std::string(prefix(eDomain)) + clsUpperSnake(szKey);
}

int ClassificationRegistry::keyId(int eDomain, const std::string& szKey)
{
	if (eDomain < 0 || eDomain >= NUM_CLS_DOMAINS) return -1;
	std::map<std::string, int>::const_iterator it = s_keyToId[eDomain].find(szKey);
	return it != s_keyToId[eDomain].end() ? it->second : -1;
}

const std::string& ClassificationRegistry::keyOf(int eDomain, int iId)
{
	static const std::string s_empty;
	if (eDomain < 0 || eDomain >= NUM_CLS_DOMAINS) return s_empty;
	if (iId < 0 || iId >= (int)s_keys[eDomain].size()) return s_empty;
	return s_keys[eDomain][iId];
}

int ClassificationRegistry::cachedKeyId(int& iCache, int eDomain, const char* szKey)
{
	if (iCache != -1) return iCache == -2 ? -1 : iCache;
	const int id = keyId(eDomain, szKey);
	if (id >= 0) { iCache = id; return id; }
	if (s_buildCount >= 2) iCache = -2;   // both load passes ran -> genuinely absent; memoize the miss
	return -1;
}

const CvInfo* ClassificationRegistry::infoForType(const std::string& szType)
{
	for (int d = 0; d < NUM_CLS_DOMAINS; ++d)
	{
		const size_t n = strlen(CLS_PREFIX[d]);
		if (szType.compare(0, n, CLS_PREFIX[d]) != 0) continue;
		const int id = GC.getInfoTypeForString(szType.c_str(), true);
		return id >= 0 ? clsRepoEdit(d, id) : NULL;
	}
	return NULL;
}

int ClassificationRegistry::mint(int eDomain, const std::string& szKey)
{
	const int existing = keyId(eDomain, szKey);
	if (existing >= 0) return existing;
	const int id = (int)s_keys[eDomain].size();
	s_keys[eDomain].push_back(szKey);
	s_keyToId[eDomain][szKey] = id;
	const std::string type = typeName(eDomain, szKey);
	GC.setInfoTypeFromString(type.c_str(), id);
	// the generated info object -- referenceable like any authored info (minimal synthesized entity: just its type)
	if (CvInfo* d = clsRepoEdit(eDomain, id))
	{
		picojson::object o;
		o["type"] = picojson::value(type);
		d->mapFrom(picojson::value(o));
	}
	return id;
}

void ClassificationRegistry::buildAndResolve(const std::vector<CvInfo*>& infos)
{
	// SEED from the GENERATED compile-time table FIRST, in table order. Minting is append-only from empty, so a
	// seeded key takes exactly its table index -- which is what makes CvClassificationIds.h's constants BE the
	// runtime ids, and therefore what lets every consumer read `info.hasSkill(CLS_SKILL_BLITZ)` instead of carrying a
	// memoized id per key ([patterns.md] § THE GETTER SETUP).
	// ⚑ The categories stay OPEN (docs/specs/json.md §8 + docs/architecture/patterns.md §The coherent surface (THE GETTER SETUP)): the mint loop below still adds any authored key
	// absent from the table, appended after the seeded block, so authoring a new key needs no engine change --
	// regenerating the table merely promotes it to a compile-time constant.
	for (int d = 0; d < NUM_CLS_DOMAINS; ++d)
		for (const char* const* pKey = CLS_SEED_KEYS[d]; *pKey != NULL; ++pKey)
			mint(d, *pKey);

	// re-assert the GC infotype entries for everything already minted (a mod re-load can rebuild the infos map
	// while this registry's append-only state survives the process)
	for (int d = 0; d < NUM_CLS_DOMAINS; ++d)
		for (size_t i = 0; i < s_keys[d].size(); ++i)
			GC.setInfoTypeFromString(typeName(d, s_keys[d][i]).c_str(), (int)i);

	// MINT: the union of authored keys, both planes (a revoke-only key is still a real ability id)
	for (size_t s = 0; s < infos.size(); ++s)
	{
		for (int d = 0; d < NUM_CLS_DOMAINS; ++d)
		{
			const CvClassificationBlock* b = clsBlockOf(infos[s], d);
			if (b == NULL) continue;
			for (std::set<std::string>::const_iterator it = b->all().begin(); it != b->all().end(); ++it)
				mint(d, *it);
			for (std::set<std::string>::const_iterator it = b->allFalse().begin(); it != b->allFalse().end(); ++it)
				mint(d, *it);
		}
	}

	// RESOLVE: every entity's blocks -> the id plane (the info touches its own protected mut* blocks)
	for (size_t s = 0; s < infos.size(); ++s)
		infos[s]->resolveClassificationIds();

	++s_buildCount;
}
