#pragma once
#ifndef CV_CLASSIFICATION_REGISTRY_H
#define CV_CLASSIFICATION_REGISTRY_H

//
//	ClassificationRegistry -- the §8/§9 classification categories as RUNTIME-GENERATED INFOS (owner ruling
//	2026-07-16: "capabilities, skills, and policies ... expressed as runtime generated enum entries, the same way
//	any Info is -- so we have clear data to refer to, even if they are only in essence a boolean switch"; the two
//	sibling §8 blocks -- attributes, tags -- ride the same mechanism for uniformity).
//
//	At the end of each cascadeLoadJson pass, one generated info is MINTED per distinct authored block key across
//	all entities: the camelCase key ("setScienceRate") becomes an INFOTYPE_NAME id ("CAPABILITY_SET_SCIENCE_RATE",
//	naming.md convention), registered in the global infotype map (GC.setInfoTypeFromString) and created as a CvInfo
//	in its domain's InfoRepo -- referenceable exactly like any authored info. Minting is APPEND-ONLY process-wide
//	(the premenu pass mints its keys, the postmenu pass adds the rest; ids never shift, so getter-side memoized ids
//	stay valid). Nothing serializes -- ids are derived, re-minted deterministically per load.
//
//	After minting, every entity's blocks resolve their names to by-id bitsets (CvInfo::resolveClassificationIds ->
//	CvJsonBoolBlock::resolveIds), making the whole classification getter surface an O(1) bit test.
//

#include "CvJsonBoolBlock.h"   // ClsDomain
#include <string>
#include <vector>

class CvInfo;

class ClassificationRegistry
{
public:
	// Mint every authored block key across `infos` (append-only), re-register the GC infotype entries, then
	// resolve every info's blocks to the id plane. Called from cascadeLoadJson after PASS 2 (both load passes).
	static void buildAndResolve(const std::vector<CvInfo*>& infos);

	static int count(int eDomain);                                  // minted keys in the domain
	static int keyId(int eDomain, const std::string& szKey);        // camelCase key -> generated id (-1 unknown)
	static const std::string& keyOf(int eDomain, int iId);          // reverse: id -> camelCase key ("" if oob)
	static const char* prefix(int eDomain);                         // "SKILL_" / "TAG_" / ...
	static std::string typeName(int eDomain, const std::string& szKey);   // PREFIX + UPPER_SNAKE(key)

	// The getter-side memo behind CvJsonBoolBlock::hasKey: returns the cached id, resolving on first call.
	// A miss is memoized (-2 -> -1) only once BOTH load passes have run, so a not-yet-minted key keeps retrying
	// during the load window but a genuinely-absent key costs nothing at runtime.
	static int cachedKeyId(int& iCache, int eDomain, const char* szKey);

	// The generated info behind a full type name ("SKILL_BLITZ") -- NULL when the prefix matches no domain or the
	// id is unminted. Cold-path (the /state/info dispatch); referenceable like any authored info.
	static const CvInfo* infoForType(const std::string& szType);

private:
	ClassificationRegistry();   // static-methods class -- never instantiated
	static int mint(int eDomain, const std::string& szKey);
};

#endif // CV_CLASSIFICATION_REGISTRY_H
