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
class CityContext;    // the per-scope live-state contexts the expected* what-if endpoints take (contexts.md)
class EmpireContext;
class CvPlotGroup;    // the trade-network object -- the reserved explicit traded-bonus source (json §3.4)
struct CvCascadeHypothetical;   // the AS-IF-HELD gate twin the expected* endpoints optionally evaluate under

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
	virtual const CvTriggers*      getTriggers()     const { return NULL; }   // §5 trigger -> chance -> action
	virtual const CvProvides*  getProvides()     const { return NULL; }   // §5a
	virtual const CvGate*      getGate()         const { return NULL; }   // entity-level enabled/disabled
	virtual const CvModifiers* getModifiers()    const { return NULL; }   // §6 families
	virtual const CvModifiers* getWhenObsolete() const { return NULL; }   // §4.2 obsolete-state tree (buildings)
	virtual const CvClassificationBlock* getSkills()       const { return NULL; }   // §8 (flat bools; keyed extras = subclass)
	virtual const CvClassificationBlock* getTags()         const { return NULL; }   // §8
	virtual const CvClassificationBlock* getAttributes()   const { return NULL; }   // §8 (buildings)
	// §8: CITY-held, grantor-PROVIDED -- a building/civic/trait/tech carries it to mean "I hand this to a city".
	virtual const CvClassificationBlock* getAmenities()    const { return NULL; }
	virtual const CvClassificationBlock* getCharacteristics() const { return NULL; } // §8 (plot substrate)
	virtual const CvClassificationBlock* getCapabilities() const { return NULL; }   // §8 (grantors)
	virtual const CvClassificationBlock* getPolicies()     const { return NULL; }   // §9 (civics/traits)

	// ⚖ THE PARAMETERIZED CLASSIFICATION READ -- ONE per domain, for every info ([patterns.md] § THE GETTER
	// SETUP: "all these individual getters should be replaced with one parameterized read"). The id argument is
	// a compile-time constant from the GENERATED CvClassificationIds.h, which the ClassificationRegistry seeds
	// from, so `kUnitInfo.hasSkill(CLS_SKILL_BLITZ)` is an O(1) bit test with no per-key surface anywhere.
	// ⛔ This is what replaced the per-key read classes (a static method per key, ~60 of them each). Do NOT
	// re-introduce one: a new key is a regenerated table entry, never a new function.
	// ⚠ A block-less info answers FALSE -- the accessors above default to NULL, so the guard is the read's.
	// The name encodes HOLD-vs-PROVIDE (json.md §8): what the entity HAS vs what it hands onward.
	bool hasSkill(int iSkillId) const                  { return clsHasId(getSkills(), iSkillId); }
	bool hasTag(int iTagId) const                      { return clsHasId(getTags(), iTagId); }
	bool hasAttribute(int iAttributeId) const          { return clsHasId(getAttributes(), iAttributeId); }
	bool providesAmenity(int iAmenityId) const         { return clsHasId(getAmenities(), iAmenityId); }
	bool hasCharacteristic(int iCharacteristicId) const{ return clsHasId(getCharacteristics(), iCharacteristicId); }
	bool providesCapability(int iCapabilityId) const   { return clsHasId(getCapabilities(), iCapabilityId); }
	bool providesPolicy(int iPolicyId) const           { return clsHasId(getPolicies(), iPolicyId); }
	// The REVOKE plane (skills.md §4 grant/revoke: a promotion authoring `stampede: false` takes the ability away).
	bool revokesSkill(int iSkillId) const              { return clsHasFalseId(getSkills(), iSkillId); }

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
	// THE UNLOCKING TECH -- declared on the BASE so every rebuilt info serves the same read, exactly as the
	// expected* valuations are. `enables` is authored SOURCE-side only (a tech names what it unlocks), so this
	// reverse family is the only direction a target can ask from; EDGEF_RELATED would answer with obsoleting
	// and merely-depositing techs mixed in. The obsoletion twin does NOT belong beside it: a target authors its
	// own `obsoletedBy`, so that read is the target's own data and stays on the types that carry it.
	// ⚠ FIRST of the list -- an entity unlocked by several techs has several, and the enabler's membership
	// formula (ANY held source enables it) is what actually decides availability. This read is for a consumer
	// that wants ONE representative tech (an era stamp, a "when does this arrive" estimate), never a gate.
	TechTypes getEnablingTech() const
	{
		const std::vector<int>* pTechs = edge(EDGEF_ENABLED_BY, EDGEB_TECHS);
		return (TechTypes)((pTechs != NULL && !pTechs->empty()) ? (*pTechs)[0] : NO_TECH);
	}
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
	int allowedCap(EnAllowedCap eKind) const                                  // -1 = uncapped/absent
	{ const CvAllowed* a = getAllowed(); return a ? a->cap(eKind) : -1; }
	// The payload this entity hands over on its OWN CONSIDERED ACTION -- the `grants` authoring shape, compiled
	// into the ONE entry list as the null-condition trigger (json.md §5). NULL when the entity authors none.
	const CvGrants* consideredGrants() const
	{ const CvTriggers* t = getTriggers(); return t ? t->consideredGrant() : NULL; }
	const std::vector<int>* grantList(int iBucketKey) const                   // NULL when absent; CvGrants::key handle
	{ const CvGrants* g = consideredGrants(); return g ? g->list(iBucketKey) : NULL; }
	int grantPulse(int iChannelKey) const { const CvGrants* g = consideredGrants(); return g ? g->pulse(iChannelKey) : 0; }
	// The SCOPED twin: a channel authored per scope (`grants.population` at city vs empire). Values are ×100 at
	// parse like every pulse, so a reader takes /100 for the human count.
	int grantScopedPulse(int iChannelKey, int iScopeKey) const
	{ const CvGrants* g = consideredGrants(); return g ? g->scopedPulse(iChannelKey, iScopeKey) : 0; }
	bool grantFlag(int iFlagKey) const       { const CvGrants* g = consideredGrants(); return g ? g->flag(iFlagKey) : false; }

	// The FREE-PROMOTION payload, read off the `triggers` onTurnEnd promote entries -- for consumers that DISPLAY
	// or SCORE it. (The applier walks the entries itself, because it must evaluate each entry's condition against
	// the unit being promoted; this read cannot, and does not pretend to.)
	// Split by whether the entry carries a CONDITION: an unconditional promotion always lands, a conditional one
	// may not, and the AI has always weighted the two differently. Both vectors are cleared and filled.
	void triggerPromotions(std::vector<int>& outAlways, std::vector<int>& outConditional) const;
	bool hasTriggerPromotions() const;

	// The FULL-HEAL payload -- the herbalist shape: a trigger that sets N units to 100% HP. The compiled entry
	// already carries both halves (healFull + healCount), so this is a bare presence read for a consumer that
	// SCORES or DISPLAYS the effect, exactly as hasTriggerPromotions is.
	// ⚠ The heal MECHANIC is a KEEP-legacy carve-out (roadmap § Scope decisions) and nothing here changes it --
	// the arithmetic that consumes the payload is untouched. What moved is the FEED: the legacy point getter this
	// replaces read a member the rebuilt info does not carry, because the effect is authored as a `triggers`
	// entry now ([json.md §5] -- a recurring handout is a trigger, never a grant).
	// ⚑ IT TOUCHES NO CACHE AND NO CASCADE (owner) -- setting N units to full HP moves no deposit and no derived
	// value, so the applier needs NO mark and this read needs no eval context. ⛔ Do not wire an invalidation for
	// it: there is nothing downstream to re-derive, and a mark would be pure per-turn cost for no state change.
	bool hasTriggerFullHeal() const;

	// --- the compiled modifier point reads (patterns.md § THE GETTER SETUP; kind and scope are separate
	// arguments per [DEC-scope-is-an-axis]; the unit picks the Σflat vs Σpercent slot, modifier.md §2).
	// AUDIENCE (json §3.9 `ai`): the default read is HUMAN; bIncludeAiOnly=true adds the ai-sibling sums
	// (the value an AI player experiences) -- always an explicit ask, never a silent default. ---
	int modifier(ModifierFamily eFamily, int iKind, CvCascScope eScope, CvCascUnit eUnit, bool bIncludeAiOnly = false) const
	{ const CvModifiers* pMods = getModifiers(); return pMods ? pMods->sum(eFamily, iKind, eScope, eUnit, bIncludeAiOnly) : 0; }
	// The straggler scalars (patterns.md getScalar) -- the same compiled-sum fetch through the InfoScalar table.
	int getScalar(InfoScalar eScalar, CvCascScope eScope, CvCascUnit eUnit, bool bIncludeAiOnly = false) const
	{
		ModifierFamily eFamily = MODFAM_NONE;
		int iKind = -1;
		infoScalarSlot(eScalar, eFamily, iKind);
		return modifier(eFamily, iKind, eScope, eUnit, bIncludeAiOnly);
	}
	// The wellbeing point read exposes the AUTHORED families' signed compiled sums (happiness/health); the
	// four-channel sign ROUTING is a fill/valuation rule (modifier.md §2b -- expectedWellbeing), so the two
	// unauthored channels (ANGER/UNHEALTH) hold no slot and read 0 here. On the BASE so every registry serves
	// it -- a bonus carries wellbeing exactly as a building does.
	int getFlatWellbeing(WellbeingChannel eChannel, CvCascScope eScope) const
	{
		if (eChannel == WELLBEING_ANGER || eChannel == WELLBEING_UNHEALTH)
		{
			return 0;
		}
		return modifier(infoWellbeingFamily(eChannel), CHANNEL_AMOUNT, eScope, CASC_UNIT_FLAT);
	}
	// The compiled conditioned list + its per-family range (patterns.md § THE GETTER SETUP read 2: the typed
	// entries with prebuilt trees -- what the package rebuild, the pedia, and the valuation walk). Empty/0-range
	// when the type composes no modifiers.
	const std::vector<const CvModEntry*>& modifierConditioned() const;
	void modifierConditionedRange(ModifierFamily eFamily, size_t& iBeginOut, size_t& iEndOut) const;

	// --- THE PER-GROUP WHAT-IF VALUATION (patterns.md § THE GETTER SETUP read 3; contexts.md § The read):
	// pass the live contexts in, get the group's expected ×100 values out -- the compiled unconditioned sums
	// fetched straight + the conditioned tail through the ONE evaluator over the ctx the CONTEXTS fill, scopes
	// folded into the experienced-here answer, plots-targets scaled by cityContext.plotAttrs, the audience
	// resolved from the asking player, the entity active/dormant verdict FED IN via the enabler's operating set
	// (a what-if NEVER evaluates requires). One-line delegations onto the ONE calc unit (InfoValuation,
	// [DEC-single-implementation]) -- declared on the base so EVERY rebuilt info serves the same read. ---
	void expectedFlatYields(const CityContext& cityContext, const EmpireContext& empireContext,
		const CvPlotGroup* plotGroup, int (&flatYields)[NUM_YIELD_TYPES],
		const CvCascadeHypothetical* pHypothetical = NULL) const;
	void expectedYieldModifiers(const CityContext& cityContext, const EmpireContext& empireContext,
		const CvPlotGroup* plotGroup, int (&yieldModifiers)[NUM_YIELD_TYPES],
		const CvCascadeHypothetical* pHypothetical = NULL) const;
	void expectedPlotYields(const CityContext& cityContext, const EmpireContext& empireContext,
		const CvPlotGroup* plotGroup, int (&plotYields)[NUM_YIELD_TYPES],
		const CvCascadeHypothetical* pHypothetical = NULL) const;
	void expectedFlatCommerce(const CityContext& cityContext, const EmpireContext& empireContext,
		const CvPlotGroup* plotGroup, int (&flatCommerce)[NUM_COMMERCE_TYPES],
		const CvCascadeHypothetical* pHypothetical = NULL) const;
	void expectedWellbeing(const CityContext& cityContext, const EmpireContext& empireContext,
		const CvPlotGroup* plotGroup, int (&wellbeing)[NUM_WELLBEING_CHANNELS],
		const CvCascadeHypothetical* pHypothetical = NULL) const;
	// The grouped/scalar-family analogues -- the same walk for one vocabulary slot (a defense kind, a
	// maintenance modifier, an InfoScalar straggler), axes spelled out exactly as the point reads'.
	//
	// pHypothetical (optional) is the AS-IF-HELD gate twin (CvConditionEval.h): the CONDITIONED tail evaluates as
	// though the caller also held / no longer held the named ids, so "what would this be worth if I had X" is the
	// DELTA between two calls rather than a second implementation of which entries X gates.
	// ⚠ It moves the conditioned entries ONLY -- an unconditioned compiled sum is unconditional by definition.
	int expectedModifier(ModifierFamily eFamily, int iKind, CvCascUnit eUnit,
		const CityContext& cityContext, const EmpireContext& empireContext, const CvPlotGroup* plotGroup,
		const CvCascadeHypothetical* pHypothetical = NULL) const;
	int expectedScalar(InfoScalar eScalar, CvCascUnit eUnit,
		const CityContext& cityContext, const EmpireContext& empireContext, const CvPlotGroup* plotGroup,
		const CvCascadeHypothetical* pHypothetical = NULL) const;

protected:
	// --- the load-time WRITE targets (mapFrom-only; the composition declaration). A derived info composes a unit
	// by holding it BY VALUE and overriding the mut* + get* pair (one line each). Default NULL = the dispatch
	// records an authored-but-unconsumed diagnostic for the section instead of parsing it.
	virtual CvRequires*  mutRequires()     { return NULL; }
	virtual CvEdges*     mutEdges()        { return NULL; }
	virtual CvAllowed*   mutAllowed()      { return NULL; }
	virtual CvTriggers*      mutTriggers()     { return NULL; }
	virtual CvProvides*  mutProvides()     { return NULL; }
	virtual CvGate*      mutGate()         { return NULL; }
	virtual CvModifiers* mutModifiers()    { return NULL; }
	virtual CvModifiers* mutWhenObsolete() { return NULL; }
	virtual CvClassificationBlock* mutSkills()       { return NULL; }
	virtual CvClassificationBlock* mutTags()         { return NULL; }
	virtual CvClassificationBlock* mutAttributes()   { return NULL; }
	virtual CvClassificationBlock* mutAmenities()    { return NULL; }
	virtual CvClassificationBlock* mutCharacteristics() { return NULL; }
	virtual CvClassificationBlock* mutCapabilities() { return NULL; }
	virtual CvClassificationBlock* mutPolicies()     { return NULL; }

private:
	CvInfo(const CvInfo&);            // noncopyable (the composed units own conditions)
	CvInfo& operator=(const CvInfo&);
};

#endif // CV_JSON_INFO_H
