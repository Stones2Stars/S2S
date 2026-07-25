#pragma once
#ifndef CV_JSON_INFO_H
#define CV_JSON_INFO_H

//
//	CvInfo -- the JSON-INFO BASE CLASS (owner rulings 2026-07-07/08). It distinguishes the JSON-populated info
//	pocos from the XML-era CvInfo classes (different data structure + reader: mapFrom(JSON) vs read(XML)).
//
//	⛔ THE BASE CARRIES ZERO SECTION DATA (owner ruling 2026-07-08: "the composable objects are not to land on
//	CvInfo directly, they are supposed to be used as needed on the different derived infos"). The json.md
//	sections are COMPOSABLE UNITS (CvRequires / CvEdges / CvAllowed / CvGrants / CvProvides /
//	CvGate / CvModifiers / CvClassificationBlock), composed BY VALUE on the derived infos that actually author
//	them (the data-grounded per-type table). The base is only:
//	  - the CvInfoBase bridge (type / text keys / button) mapFrom reads for every type;
//	  - the ONE reusable section DISPATCH (each unit's parse is written once, reused by every composing type);
//	  - DATA-FREE virtual accessors (NULL = "this type does not compose the section") so generic consumers
//	    (the enabler's gate sets, the DepositIndex build) read uniformly without the base holding anything.
//
//	WRITE-ONCE AT LOAD, IMMUTABLE AFTER (owner ruling 2026-07-08: "we are only ever writing to these objects at
//	load, they are not changed after, that is the very design"): mapFrom is the sole mutation path (readJson,
//	during load); every public reader is const. A section authored in an entity's JSON that its type composes no
//	unit for is RECORDED (jsonNoteUnconsumed -- the [READJSON] census), never silently dropped -- the exact
//	"didn't pan out" representation-gap class that bred the old-Info fallbacks.
//

#include "Infos/CvInfoBase.h"     // the shared base of ALL infos -- getType()/getDescription()/...
#include "CvRequires.h"       // the composable section units (small headers; the base names them, holds none)
#include "CvEdges.h"
#include "CvAllowed.h"
#include "CvGrants.h"
#include "CvTriggers.h"
#include "CvProvides.h"
#include "CvGate.h"
#include "CvModifiers.h"
#include "CvClassificationBlock.h"
#include <string>
#include <vector>

namespace picojson { class value; }   // mapFrom's input -- full definition via the PCH umbrella in the .cpp

// Base = CvHotkeyInfo (owner ruling 2026-07-08: "A is fine, we dont care if everything can have a hotkey"). CvHotkeyInfo
// : CvInfoBase, so every poco is still a CvInfoBase (getType/getDescription/...); the 8 action types (Building/Corporation/
// Heritage/Promotion/Religion/Specialist/Unit/Build -- the classes that inherited CvHotkeyInfo in the XML era) use the
// hotkey/action surface (read from XML, operational side deferred), the other 15 carry the unused fields harmlessly. This
// puts the shared CvHotkeyInfo base ON CvInfo (single inheritance chain, no CvInfoBase diamond -- the per-type
// alternative would need risky virtual-CvInfoBase inheritance across CvHotkeyInfo's many EXE-adjacent descendants).
class CvInfo : public CvHotkeyInfo
{
public:
	CvInfo();
	virtual ~CvInfo();

	// #430: the ONE load hook -- IDEMPOTENT BY CONTRACT (owner constraint: FK links register AFTER all JSONs are
	// loaded). LoadGlobalClassInfoJson (CvXMLLoadUtilitySet) creates the poco and calls mapFrom on this entity's
	// curated JSON -- NO XML read (DEC-no-xml-into-game). A per-type subclass overrides mapFrom, calls
	// CvInfo::mapFrom(entity) FIRST (clears + re-parses the composed sections), then parses its own typed members
	// (keyed skills, FKs, flags). Because an ALIASED poco is mapFrom'd by its category's loader MID-registry (any
	// FK naming a later-loading category silently drops), loadJson RE-RUNS the full mapFrom once the
	// registry is complete -- so every mapFrom (base AND subclass) must FULLY DEFINE its output each call:
	// sections clear via clearSections() here; a subclass CLEARS its accumulating typed containers (vectors,
	// += maps) at the top of its own parse. Scalar assigns are naturally idempotent.
	virtual void mapFrom(const picojson::value& entity);

protected:
	void mapSections(const picojson::value& entity);   // the ONE base section dispatch (mapFrom's body)
	void clearSections();                              // clearParsed() on every composed unit the type carries

public:

	// --- the composable section units -- DATA-FREE here; a derived info that composes one overrides its pair ---
	virtual const CvRequires*  getRequires()     const { return NULL; }   // §4.3
	virtual const CvEdges*     getEdges()        const { return NULL; }   // §4.1/§4.2
	virtual const CvAllowed*   getAllowed()      const { return NULL; }   // §4.4
	virtual const CvGrants*    getGrants()       const { return NULL; }   // §5
	virtual const CvTriggers*      getTriggers()     const { return NULL; }   // §5 trigger -> chance -> action
	virtual const CvProvides*  getProvides()     const { return NULL; }   // §5a
	virtual const CvGate*      getGate()         const { return NULL; }   // entity-level enabled/disabled
	virtual const CvModifiers* getModifiers()    const { return NULL; }   // §6 families
	virtual const CvModifiers* getWhenObsolete() const { return NULL; }   // §4.2 obsolete-state tree (buildings)
	virtual const CvClassificationBlock* getSkills()       const { return NULL; }   // §8 (flat bools; keyed extras = subclass)
	virtual const CvClassificationBlock* getTags()         const { return NULL; }   // §8
	virtual const CvClassificationBlock* getAttributes()   const { return NULL; }   // §8 (buildings)
	virtual const CvClassificationBlock* getCapabilities() const { return NULL; }   // §8 (grantors)
	virtual const CvClassificationBlock* getPolicies()     const { return NULL; }   // §9 (civics/traits)

	// §8/§9 classification id-plane resolve -- fills each carried block's by-id bitsets from the generated
	// ClassificationRegistry (SKILL_/TAG_/ATTRIBUTE_/CAPABILITY_/POLICY_). Called by the registry's
	// buildAndResolve after minting (LOAD-ONLY; the info touches its own protected mut* blocks).
	void resolveClassificationIds();

	// --- terse read-throughs (the cascade's hot query surface; safe when the unit is absent) ---
	const CvCondition* requiresBuild() const   { const CvRequires* r = getRequires(); return r ? r->build : NULL; }
	const CvCondition* requiresOperate() const { const CvRequires* r = getRequires(); return r ? r->operate : NULL; }
	const std::vector<int>& dormantTriggers() const;                          // empty static when absent
	const std::vector<int>* edge(EnEdgeFamily eFamily, EnEdgeBucket eBucket) const   // NULL when absent; int-keyed, no strings
	{ const CvEdges* e = getEdges(); return e ? e->find(eFamily, eBucket) : NULL; }
	// The reverse-view writer -- LOAD-ONLY (the readJson reverse pass fills EDGEF_RELATED/EDGEF_REQUIRED_BY onto
	// the REFERENCED info, so every info already carries its reverse lookups after load). Part of the
	// write-once-at-load window; never called post-load.
	void addReverseEdge(EnEdgeFamily eFamily, EnEdgeBucket eBucket, int iId)
	{ CvEdges* e = mutEdges(); if (e != NULL) e->add(eFamily, eBucket, iId); }
	void sortUniqueEdges() { CvEdges* e = mutEdges(); if (e != NULL) e->sortUnique(); }
	// The own-output reverse LANDING writer -- LOAD-ONLY (the readJson general reverse pass, [DEC-one-reverse-view],
	// modifier.md §4): a source's target-keyed own-output deposit lands HERE, on the target, as a compiled
	// conditioned entry ("+X while the source is present"). Takes ownership; a type composing no CvModifiers
	// frees the entry (nothing to land on). Part of the write-once-at-load window; never called post-load.
	void landOwnOutputEntry(CvModEntry* pEntry)
	{
		CvModifiers* pModifiers = mutModifiers();
		if (pModifiers != NULL)
		{
			pModifiers->landReverseEntry(pEntry);
		}
		else
		{
			delete pEntry;
		}
	}
	int allowedCap(const std::string& szKind) const                           // -1 = uncapped/absent
	{ const CvAllowed* a = getAllowed(); return a ? a->cap(szKind) : -1; }
	const std::vector<int>* grantList(const std::string& szBucket) const      // NULL when absent
	{ const CvGrants* g = getGrants(); return g ? g->list(szBucket) : NULL; }
	int grantPulse100(const std::string& szChannel) const { const CvGrants* g = getGrants(); return g ? g->pulse100(szChannel) : 0; }
	bool grantFlag(const std::string& szName) const       { const CvGrants* g = getGrants(); return g ? g->flag(szName) : false; }

	// --- the compiled modifier point reads (patterns.md § THE GETTER SETUP; kind and scope are separate
	// arguments per [DEC-scope-is-an-axis]; the unit picks the Σflat vs Σpercent slot, modifier.md §2) ---
	int modifier100(ModifierFamily eFamily, int iKind, CvCascScope eScope, CvCascUnit eUnit) const
	{ const CvModifiers* pMods = getModifiers(); return pMods ? pMods->sum100(eFamily, iKind, eScope, eUnit) : 0; }
	// The straggler scalars (patterns.md getScalar) -- the same compiled-sum fetch through the InfoScalar table.
	int getScalar100(InfoScalar eScalar, CvCascScope eScope, CvCascUnit eUnit) const
	{
		ModifierFamily eFamily = MODFAM_NONE;
		int iKind = -1;
		infoScalarSlot(eScalar, eFamily, iKind);
		return modifier100(eFamily, iKind, eScope, eUnit);
	}

protected:
	// --- the load-time WRITE targets (mapFrom-only; the composition declaration). A derived info composes a unit
	// by holding it BY VALUE and overriding the mut* + get* pair (one line each). Default NULL = the dispatch
	// records an authored-but-unconsumed diagnostic for the section instead of parsing it.
	virtual CvRequires*  mutRequires()     { return NULL; }
	virtual CvEdges*     mutEdges()        { return NULL; }
	virtual CvAllowed*   mutAllowed()      { return NULL; }
	virtual CvGrants*    mutGrants()       { return NULL; }
	virtual CvTriggers*      mutTriggers()     { return NULL; }
	virtual CvProvides*  mutProvides()     { return NULL; }
	virtual CvGate*      mutGate()         { return NULL; }
	virtual CvModifiers* mutModifiers()    { return NULL; }
	virtual CvModifiers* mutWhenObsolete() { return NULL; }
	virtual CvClassificationBlock* mutSkills()       { return NULL; }
	virtual CvClassificationBlock* mutTags()         { return NULL; }
	virtual CvClassificationBlock* mutAttributes()   { return NULL; }
	virtual CvClassificationBlock* mutCapabilities() { return NULL; }
	virtual CvClassificationBlock* mutPolicies()     { return NULL; }

private:
	CvInfo(const CvInfo&);            // noncopyable (the composed units own conditions)
	CvInfo& operator=(const CvInfo&);
};

#endif // CV_JSON_INFO_H
