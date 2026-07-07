#pragma once
#ifndef CV_JSON_INFO_H
#define CV_JSON_INFO_H

//
//	CvJsonInfo -- the JSON-INFO BASE CLASS (owner rulings 2026-07-07/08). It distinguishes the JSON-populated info
//	pocos from the XML-era CvInfo classes (different data structure + reader: mapFrom(JSON) vs read(XML)).
//
//	⛔ THE BASE CARRIES ZERO SECTION DATA (owner ruling 2026-07-08: "the composable objects are not to land on
//	CvJsonInfo directly, they are supposed to be used as needed on the different derived infos"). The json.md
//	sections are COMPOSABLE UNITS (CvJsonRequires / CvJsonEdges / CvJsonAllowed / CvJsonGrants / CvJsonProvides /
//	CvJsonGate / CvJsonModifiers / CvJsonBoolBlock), composed BY VALUE on the derived infos that actually author
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
#include "CvJsonRequires.h"       // the composable section units (small headers; the base names them, holds none)
#include "CvJsonEdges.h"
#include "CvJsonAllowed.h"
#include "CvJsonGrants.h"
#include "CvJsonProvides.h"
#include "CvJsonGate.h"
#include "CvJsonModifiers.h"
#include "CvJsonBoolBlock.h"
#include <string>
#include <vector>

namespace picojson { class value; }   // mapFrom's input -- full definition via the PCH umbrella in the .cpp

class CvJsonInfo : public CvInfoBase
{
public:
	CvJsonInfo();
	virtual ~CvJsonInfo();

	// Core JSON reading: the shared CvInfoBase fields + the ONE section dispatch. A per-type subclass overrides
	// this, calls CvJsonInfo::mapFrom(entity) FIRST, then parses its own typed members (keyed skills, FKs, flags).
	virtual void mapFrom(const picojson::value& entity);

	// --- the composable section units -- DATA-FREE here; a derived info that composes one overrides its pair ---
	virtual const CvJsonRequires*  getRequires()     const { return NULL; }   // §4.3
	virtual const CvJsonEdges*     getEdges()        const { return NULL; }   // §4.1/§4.2
	virtual const CvJsonAllowed*   getAllowed()      const { return NULL; }   // §4.4
	virtual const CvJsonGrants*    getGrants()       const { return NULL; }   // §5
	virtual const CvJsonProvides*  getProvides()     const { return NULL; }   // §5a
	virtual const CvJsonGate*      getGate()         const { return NULL; }   // entity-level enabled/disabled
	virtual const CvJsonModifiers* getModifiers()    const { return NULL; }   // §6 families
	virtual const CvJsonModifiers* getWhenObsolete() const { return NULL; }   // §4.2 obsolete-state tree (buildings)
	virtual const CvJsonBoolBlock* getSkills()       const { return NULL; }   // §8 (flat bools; keyed extras = subclass)
	virtual const CvJsonBoolBlock* getTags()         const { return NULL; }   // §8
	virtual const CvJsonBoolBlock* getAttributes()   const { return NULL; }   // §8 (buildings)
	virtual const CvJsonBoolBlock* getCapabilities() const { return NULL; }   // §8 (grantors)
	virtual const CvJsonBoolBlock* getPolicies()     const { return NULL; }   // §9 (civics/traits)

	// --- terse read-throughs (the cascade's hot query surface; safe when the unit is absent) ---
	const CvJsonCondition* requiresBuild() const   { const CvJsonRequires* r = getRequires(); return r ? r->build : NULL; }
	const CvJsonCondition* requiresOperate() const { const CvJsonRequires* r = getRequires(); return r ? r->operate : NULL; }
	const std::vector<int>& dormantTriggers() const;                          // empty static when absent
	const std::vector<int>* edge(const std::string& szEdgeDotBucket) const    // NULL when absent
	{ const CvJsonEdges* e = getEdges(); return e ? e->find(szEdgeDotBucket) : NULL; }
	int allowedCap(const std::string& szKind) const                           // -1 = uncapped/absent
	{ const CvJsonAllowed* a = getAllowed(); return a ? a->cap(szKind) : -1; }
	const std::vector<int>* grantList(const std::string& szBucket) const      // NULL when absent
	{ const CvJsonGrants* g = getGrants(); return g ? g->list(szBucket) : NULL; }
	int grantPulse100(const std::string& szChannel) const { const CvJsonGrants* g = getGrants(); return g ? g->pulse100(szChannel) : 0; }
	bool grantFlag(const std::string& szName) const       { const CvJsonGrants* g = getGrants(); return g ? g->flag(szName) : false; }

protected:
	// --- the load-time WRITE targets (mapFrom-only; the composition declaration). A derived info composes a unit
	// by holding it BY VALUE and overriding the mut* + get* pair (one line each). Default NULL = the dispatch
	// records an authored-but-unconsumed diagnostic for the section instead of parsing it.
	virtual CvJsonRequires*  mutRequires()     { return NULL; }
	virtual CvJsonEdges*     mutEdges()        { return NULL; }
	virtual CvJsonAllowed*   mutAllowed()      { return NULL; }
	virtual CvJsonGrants*    mutGrants()       { return NULL; }
	virtual CvJsonProvides*  mutProvides()     { return NULL; }
	virtual CvJsonGate*      mutGate()         { return NULL; }
	virtual CvJsonModifiers* mutModifiers()    { return NULL; }
	virtual CvJsonModifiers* mutWhenObsolete() { return NULL; }
	virtual CvJsonBoolBlock* mutSkills()       { return NULL; }
	virtual CvJsonBoolBlock* mutTags()         { return NULL; }
	virtual CvJsonBoolBlock* mutAttributes()   { return NULL; }
	virtual CvJsonBoolBlock* mutCapabilities() { return NULL; }
	virtual CvJsonBoolBlock* mutPolicies()     { return NULL; }

private:
	CvJsonInfo(const CvJsonInfo&);            // noncopyable (the composed units own conditions)
	CvJsonInfo& operator=(const CvJsonInfo&);
};

#endif // CV_JSON_INFO_H
