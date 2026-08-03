#pragma once

#ifndef CyInfo_h__
#define CyInfo_h__

#include <string>
#include "Infrastructure/IDValueMap.h"   // the bulk id->value index handed to Python in one crossing

//
//	CyInfo -- the Python INFO surface: the "what do I CARRY?" read role (patterns.md § THE TWO READ ROLES),
//	exposed to script. Sibling of CyEnabler ("can I?"), CyState ("what do I HAVE?") and CyEnums (the vocabulary).
//
//	⛔ THIS IS WHERE INFOS LIVE NOW, AND THE ONLY PLACE. The global context deliberately hands out none
//	([DEC-cy-not-fixed]): its `get<X>Info(i)` accessors returned an object carrying the whole legacy getter set,
//	which is the escape hatch the rebuild closes. A script wanting a SETTING asks the global context; a script
//	wanting ENTITY DATA asks here.
//
//	⚑ ADDRESSED BY INFOTYPE PREFIX, not by a getter per registry. `getDescription("UNIT_", id)` routes through the
//	ONE infotype-prefix -> InfoRepo dispatch the load pipeline already owns
//	([DEC-single-implementation]), so a new category is served the moment it is registered there -- no method is
//	added here, and the two cannot drift. That is the same "extensible by DATA" rule the rest of the surface obeys.
//
//	⚠ DELIBERATELY SMALL. This serves the per-type INDEX shape (pedia-read-map § shape 2: id -> name/type), which
//	is what every enumeration in script actually asks for. The per-entity PAYLOAD, the edge lists and the rendered
//	entry lines are the rest of the shape and are NOT here yet; a consumer needing one gets a loud AttributeError
//	rather than a quiet wrong answer.
//
//	BOOST: only the `python::` alias, never a bare `boost::` -- two Boosts coexist (engine.md).
//
//
//	The INTRINSIC STRAGGLERS -- genuinely lone, unconditioned values that belong to no group
//	([patterns.md] THE GETTER SETUP category 4: "bare typed reads ... plus getScalar for the 1-2-entry
//	stragglers"). Addressed by SLOT so the surface grows by an enum entry rather than by a method per value,
//	which is the same "extensible by DATA, not by new getters" rule the rest of the read surface obeys.
//
//	⛔ NOT a general escape hatch. A value that belongs to a GROUP is read through its group (yields, commerce,
//	wellbeing, ...); a CLASSIFICATION is an O(1) bitset test; an EDGE is getEdgeIds. This is only for the
//	leftovers those three do not cover, and a new entry earns its place by being one of them.
//
enum PyIntrinsicSlot
{
	PYINT_COST = 0,            // the entity's own authored make-cost (`cost.production`) -- HUMAN, never x100
	PYINT_BONUS_CLASS,         // BONUSCLASS_* FK  (identity.bonusClassType)
	PYINT_IS_MAP_BONUS,        // does this bonus appear on the map at all
	PYINT_IS_VISIBLE,          // is this specialist assignable on the city screen (identity.visible)
	PYINT_COLOR_TYPE,          // COLOR_* FK for a yield
	PYINT_ACTION_INFO_INDEX,   // the hotkey/action index a control maps to
	PYINT_IS_LIMITED_WONDER,   // does this building carry a SELF-cap (world/team/empire) -- i.e. is it a wonder
	PYINT_IS_PERMANENT,        // is this victory PERMANENT (never written into a scenario's victory list)
	PYINT_IS_REPEAT,           // is this tech REPEATABLE (researchable more than once)
	PYINT_DEFAULT_PLAYERS,     // a world size's default player count (the map-setup straggler)
	PYINT_HEADQUARTERS_CORPORATION, // CORPORATION_* FK -- the corp this building is the HEADQUARTERS of (json §9)
	PYINT_IS_SEE_DEMOGRAPHICS, // does this espionage mission reveal demographics (the scoreboard's power column)
	PYINT_IS_SEE_RESEARCH,     // does this espionage mission reveal research (the scoreboard's research column)
	PYINT_IS_SPACESHIP,
	PYINT_WONDER_SCOPE,         // WHICH scope a building's self-cap sits at -- the wonder CATEGORY
	PYINT_IS_NO_INSTANCE_LIMIT, // is this building RELOCATABLE -- it waives the empire (national-wonder)
	                            // cap, so a palace / culture building can be rebuilt elsewhere (json identity)        // is this project a spaceship part (the build-progress readout)
	NUM_PYINT
};

//
//	The LIST-valued intrinsics -- an info's own id LIST, addressed by SLOT exactly as the scalar intrinsics are,
//	so this surface grows by an enum entry rather than by a method per relationship.
//
//	⚑ Each one exists because the alternative is a WHOLE-REGISTRY SCAN in script. The headquarters list is the
//	worked case: the readJson reverse pass feeds it onto the corporation for exactly this reason ("every
//	consumer asks the CORPORATION which building is its HQ; feeding the registry here is what stops each of them
//	scanning the whole building registry" -- CvReversePass), and asking it the other way round is the shape
//	[DEC-one-reverse-view] exists to delete.
//
enum PyIdListSlot
{
	PYLIST_HEADQUARTERS_BUILDINGS = 0, // CORPORATION_ -> the buildings that are its headquarters
	PYLIST_CONSUMED_BONUSES,           // CORPORATION_ -> the bonuses it consumes
	NUM_PYLIST
};

class CyInfo
{
public:
	CyInfo() {}

	// The entity's display NAME, already localized (the info's own resolved text).
	std::wstring getDescription(const std::string& szTypePrefix, int iId) const;
	// The entity's TXT_KEY -- the UNRESOLVED key, for a caller that composes it into a larger string the text
	// system then resolves. ⛔ The `*Key` suffix is the contract ([patterns.md]): this returns a KEY, while
	// getDescription above returns RESOLVED TEXT. A name that hides which one you hold is how a raw key ends up
	// rendered to a player, or a resolved string ends up fed back into getText.
	std::wstring getTextKey(const std::string& szTypePrefix, int iId) const;
	// The entity's stable TYPE KEY ("UNIT_AXEMAN") -- what a scenario serializer and a config string need.
	std::string getType(const std::string& szTypePrefix, int iId) const;
	// The entity's BUTTON/icon art reference (the `ui` block, json.md §7). ART is an unmigrated system boundary
	// that stays ([roadmap] Scope decisions), so what crosses here is the TAG the art manager resolves -- never
	// pixels, and never an art OBJECT. This is the read every enumeration screen makes beside the description.
	std::string getButton(const std::string& szTypePrefix, int iId) const;
	// Whether the registry actually holds an entity at this id, so a caller can skip a hole without
	// inferring it from an empty name.
	bool exists(const std::string& szTypePrefix, int iId) const;

	// The entity's EDGE ids for one (family, bucket) -- "what do I unlock", "what unlocks me", "what needs me".
	// ⚑ This is a SERVED answer to a question script currently asks by SCANNING A WHOLE REGISTRY and testing a
	// per-id predicate. The readJson reverse pass already lands every reverse family on the info
	// ([DEC-one-reverse-view]), so the scan is not merely slow, it is re-deriving what the info already carries.
	// ⛔ Parameterized over the family/bucket ENUMS, never a getter per relation: the axes are the spec's fixed
	// vocabulary, so a new bucket is data and this surface does not grow.
	python::list getEdgeIds(const std::string& szTypePrefix, int iId, int iFamily, int iBucket) const;

	// THE PER-TYPE INDEX PAYLOAD (pedia-read-map §5 shape 2) -- the WHOLE registry in ONE crossing.
	//
	// ⚑ This is what every enumeration screen actually renders: the pedia hub's item lists, the A-Z index, the
	// WorldBuilder drop-downs, Forgetful's whole-registry sweep. They ask (id, name, button) per entity and today
	// pay one boundary crossing PER ENTITY -- a boost::python call costs far more than the lookup inside it, so
	// the read that scales crosses ONCE and is cached Python-side (the CivicData precedent).
	//
	// Each entry is a dict: {"id", "type", "description", "textKey", "button"} -- the identity block, carrying
	// the resolved TEXT and the TXT_KEY both, each named for which it is ([patterns.md]: a `*Key` returns a key,
	// the bare form returns resolved text; a name that hides which you hold is how a raw key reaches a player).
	// ⛔ CATEGORY TAGS are deliberately ABSENT: the pedia's taxonomy has no home yet (pedia-read-map finding 4),
	// and minting one here would be exactly the bespoke shape that gap is waiting on a decision for.
	python::list getIndex(const std::string& szTypePrefix) const;

	// A STRAGGLER SCALAR off the compiled sums -- patterns.md's getScalar, the category-4 read for the genuinely
	// lone unconditioned values that belong to no group. Keyed by the shared InfoScalar vocabulary, with SCOPE
	// and UNIT as spelled-out arguments ([DEC-scope-is-an-axis]: scope is an axis, never a name fragment).
	// ⚑ This is how a value that looks like it needs a bespoke accessor is actually reached -- a game speed's
	// percent is getScalar("GAMESPEED_", id, SCALAR_SPEED, CASC_SCOPE_WORLD, CASC_UNIT_PERCENT), not a
	// getSpeedPercent revival. The surface grows by an ENUM ENTRY, never by a method per value.
	int getScalar(const std::string& szTypePrefix, int iId, int iScalar, int iScope, int iUnit) const;

	// One lone intrinsic value, by SLOT (see PyIntrinsicSlot). Bools answer 0/1 and FKs answer their id, so the
	// whole straggler plane is one int-returning read. Answers -1 when the (prefix, slot) pair names nothing,
	// so a caller can tell "not served here" from a real 0.
	int getIntrinsic(const std::string& szTypePrefix, int iId, int iSlot) const;
	// An info's own id LIST, by SLOT (see PyIdListSlot). Empty when the (prefix, slot) pair names nothing.
	python::list getIdList(const std::string& szTypePrefix, int iId, int iSlot) const;

	// ⚑ THE BULK INDEX SHAPE -- one boundary crossing for a WHOLE id->value column, not one per entity.
	// A boost::python call costs far more than the lookup inside it, so the read that scales is the one that
	// crosses ONCE and is cached on the Python side (CivicData does exactly that). Typed end to end: no string
	// prefix, no string member name -- the id type IS the addressing.
	// The map is engine-owned and built once at first use; Python receives a reference and may only iterate it,
	// test membership, and getValue() -- it can mutate nothing.
	const IDValueMap<CivicTypes, int>& civicOptions() const;

	static void pythonPublish();
};

#endif // CyInfo_h__
