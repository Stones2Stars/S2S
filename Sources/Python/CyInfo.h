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
//	⛔ THE PREFIX NAMES THE REGISTRY, AND THAT IS USUALLY -- BUT NOT ALWAYS -- THE AUTHORED INFOTYPE PREFIX
//	([naming.md]). The coincidence breaks in BOTH directions, and each break is silent rather than loud:
//	  · ONE registry, SEVERAL authored prefixes -- Diplomacy holds AI_DIPLOCOMMENT_* beside USER_DIPLOCOMMENT_*,
//	    so no authored prefix addresses the registry at all.
//	  · TWO registries, ONE authored prefix -- NewConcept authors CONCEPT_*, exactly as the separate Concept
//	    registry does, so routing on the authored token answers from whichever registry the dispatch reached
//	    first: a wrong-registry read that returns a plausible string for an id that means something else.
//	⇒ When adding a registry, take the routing token from the REGISTRY, then check no other registry already
//	claims it. A collision here cannot be caught by the compiler and does not fail loud -- it answers.
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
	PYINT_IS_DISABLED,         // TECH_ is this tech switched OFF in the data (identity.disable) -- it exists
	                           // in the registry and is never offered, so a LISTING must skip it
	PYINT_DEFAULT_PLAYERS,     // a world size's default player count (the map-setup straggler)
	PYINT_HEADQUARTERS_CORPORATION, // CORPORATION_* FK -- the corp this building is the HEADQUARTERS of (json §9)
	PYINT_IS_SPACESHIP,
	PYINT_WONDER_SCOPE,         // WHICH scope a building's self-cap sits at -- the wonder CATEGORY
	PYINT_ERA,                  // TECH_ era FK (identity.era) -- the tech tree's era banding
	PYINT_ADVISOR,              // TECH_ ADVISOR_* FK (ui.art.advisor) -- which advisor the tech is filed under
	PYINT_TOTAL_TURNS,          // GAMESPEED_ the speed's total game length in turns (the end-turn estimate)
	PYINT_GRID_X,               // TECH_ tech-tree layout column (identity.gridX)
	PYINT_GRID_Y,               // TECH_ tech-tree layout row
	PYINT_TRADE_ROUTE_AMOUNT,   // the scope-wide flat trade-ROUTE COUNT this entity deposits (kind 0 IS the
	                            // count -- TRADE_ROUTE_AMOUNT, CvInfoKinds.h)
	PYINT_IS_NO_INSTANCE_LIMIT, // is this building RELOCATABLE -- it waives the empire (national-wonder)
	                            // cap, so a palace / culture building can be rebuilt elsewhere (json identity)        // is this project a spaceship part (the build-progress readout)
	PYINT_VOTE_SOURCE,          // BUILDING_ VOTESOURCE_* FK -- the diplomatic body this building CONVENES
	                            // (json par.9 `voteSource`); -1 for the overwhelming majority
	PYINT_IS_BUILD,             // MISSION_ -- is this mission a worker BUILD (the order carries a BUILD_ id)
	PYINT_UNIT_COMBAT,          // UNIT_ UNITCOMBAT_* FK, NO_UNITCOMBAT when the unit has none
	PYINT_HURRY_GOLD_PER_PRODUCTION,   // HURRY_ -- gold paid per hammer
	PYINT_HURRY_PRODUCTION_PER_POPULATION,
	PYINT_HURRY_IS_ANGER,       // HURRY_ -- does using it anger the population
	PYINT_ESPIONAGE_COST,       // ESPIONAGEMISSION_ -- its base cost, -1 when the mission is unavailable
	PYINT_ESPIONAGE_TARGETS_CITY,
	PYINT_ESPIONAGE_IS_PASSIVE,
	PYINT_ESPIONAGE_TECH_PREREQ,             // MISSION_ -- is this mission a worker BUILD (the order carries a BUILD_ id)
	PYINT_SPECIAL_BUILDING,     // BUILDING_ SPECIALBUILDING_* FK -- the shared-cap group, -1 when ungrouped
	PYINT_DOMAIN,               // UNIT_ DOMAIN_* FK (identity.domain) -- WHERE the unit operates.
	                            // ⛔ It is a genuine INTRINSIC, never a tag read ([json.md] par.7, [tags.md]): a
	                            // tag says what a unit IS, a domain says where it OPERATES, and answering the
	                            // second from the tag set means FILTERING EVERY TAG for what one field holds.
	                            // The domain tags (landUnit/seaUnit/airUnit) exist and are inert by ruling --
	                            // there is deliberately no composition over them for this, so a consumer asking
	                            // "which domain" asks HERE.
	PYINT_PILLAGE_GOLD,         // IMPROVEMENT_ -- the gold a pillage of this improvement rolls against
	                            // (identity.pillageGold). ⚠ NOT the building field of the same name, which is
	                            // orphaned and unwired ([legacy-value-calc-map.md] par.10.3); this one is live and
	                            // is the improvement's own value.
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
	PYLIST_GRANTED_BUILDINGS,          // UNIT_ -> the buildings its `grants.buildings` hands over (the
	                                   // MISSION_CONSTRUCT repertoire the construct gate reads). The EXACT
	                                   // relation, so a consumer can filter the merged EDGEF_RELATED candidate
	                                   // set down to it instead of trusting that superset.
	PYLIST_PREREQ_AND_TECHS,           // TECH_ -> the techs its requires.build.all names (ALL must be held)
	PYLIST_PREREQ_OR_TECHS,            // TECH_ -> the techs of its requires.build any-group (ONE must be held)
	                                   // ⚑ The AND/OR split is why these are two slots rather than the merged
	                                   // EDGEF_ENABLED_BY family: that bucket mixes enabling techs with
	                                   // OBSOLETING ones and drops the distinction ([enabler.md] §2), so a
	                                   // consumer with ALL semantics -- a tech tree drawing prereq arrows --
	                                   // cannot read it. These are the load-reconstructed forward views.
	PYLIST_QUALIFIED_UNITCOMBATS,      // PROMOTION_ -> the combat classes it may be taken by
	PYLIST_DISQUALIFIED_UNITCOMBATS,   // PROMOTION_ -> the classes it is barred from
	                                   // ⚑ Both are the info's own POST-LOAD caches, folded once from this
	                                   // promotion's own lists plus its LINE's ([CvPromotionInfo.h] names them
	                                   // the pedia caches). So this publishes a computed answer rather than
	                                   // asking Python to re-fold a rung against its ladder per render.
	PYLIST_PREREQ_EVENTS,              // EVENTTRIGGER_ -> the events a trigger requires to have already fired.
	                                   // ⚠ EVENT_ / EVENTTRIGGER_ are the #425 PERMANENT carve-out (events stay
	                                   // Python and are due a ground-up rework), so this is a KEEP-WORKING read
	                                   // and nothing more -- it rides the existing generic slot plane beside its
	                                   // prereq siblings rather than earning the type its own accessor.
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
	// The other GENERIC CvInfoBase texts, on the same prefix dispatch: every registry carries them, so they
	// belong on the generic plane rather than a per-type accessor. RESOLVED TEXT, like getDescription.
	std::wstring getCivilopedia(const std::string& szTypePrefix, int iId) const;
	std::wstring getStrategy(const std::string& szTypePrefix, int iId) const;
	std::wstring getHelp(const std::string& szTypePrefix, int iId) const;
	// The other two AUTHORED IDENTITY TEXTS ([json.md] §7: identity carries description, help, civilopedia,
	// message, quote, strategy, ADJECTIVE and SHORT DESCRIPTION). They are genuinely distinct content, not a
	// legacy per-field getter to collapse: a civilization carries a NAME, a SHORT name and an ADJECTIVE
	// ("Rome" / "Rome" / "Roman"), and the dynamic-naming code composes from exactly these.
	// ⛔ uiForm is LOAD-BEARING and is passed through, never defaulted away -- it selects the grammatical form
	// localization needs, so dropping it silently collapses every declined variant onto the nominative.
	std::wstring getAdjective(const std::string& szTypePrefix, int iId, int iForm) const;
	std::wstring getShortDescription(const std::string& szTypePrefix, int iId, int iForm) const;
	// The entity's stable TYPE KEY ("UNIT_AXEMAN") -- what a scenario serializer and a config string need.
	std::string getType(const std::string& szTypePrefix, int iId) const;
	// The entity's BUTTON/icon art reference (the `ui` block, json.md §7). ART is an unmigrated system boundary
	// that stays ([roadmap] Scope decisions), so what crosses here is the TAG the art manager resolves -- never
	// pixels, and never an art OBJECT. This is the read every enumeration screen makes beside the description.
	std::string getButton(const std::string& szTypePrefix, int iId) const;
	//	The pedia BUCKET (identity.pediaCategory) for ONE entity -- the per-id twin of the same field on
	//	getIndex. Both exist because the pedia asks both questions: which entities are in a bucket (the list),
	//	and which bucket is THIS entity in (the jump-to-page). Empty = the ordinary bucket.
	std::string getPediaCategory(const std::string& szTypePrefix, int iId) const;
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

	// The WELLBEING group -- one read hands back the WHOLE group, indexed by the published WellbeingChannel
	// ([patterns.md] THE TWO READ ROLES rule 1: the surface grows by GROUPS, never by channels, so there is no
	// per-channel getter to add). SCOPE is a spelled-out argument ([DEC-scope-is-an-axis]).
	// ⚑ This is what lets a consumer ask "which bonuses are a luxury / a health resource" WITHOUT sweeping the
	// registry asking a per-id predicate -- it reads the entity's own compiled sum.
	// Values are x100 ([DEC-fixedpoint-x100]); a reader divides at its point of use.
	python::list getWellbeing(const std::string& szTypePrefix, int iId, int iScope) const;

	// The REVOLUTION group -- the whole kind-indexed group in one read, exactly as getWellbeing hands back its
	// channels ([patterns.md] THE TWO READ ROLES rule 1). Revolutions is Python-authoritative and due its own
	// rework, so this carries the authored data faithfully and models nothing.
	python::list getRevolution(const std::string& szTypePrefix, int iId, int iScope) const;

	// The PROCESS conversion group -- how much of the city's production this process turns into each commerce,
	// the whole group in one read indexed by CommerceTypes ([patterns.md] THE TWO READ ROLES rule 1).
	python::list getProductionToCommerce(const std::string& szTypePrefix, int iId, int iScope) const;

	// The MOVEMENT group -- the whole kind-indexed group in one read, the getWellbeing shape ([patterns.md] THE
	// TWO READ ROLES rule 1: the surface grows by GROUPS, never by channels). It is what an enumeration screen
	// renders a unit's moves column from, and the family carries no straggler scalar, so nothing else reaches it.
	// ⛔ Each kind is read at ITS OWN unit (`infoKindUnit`), never one pinned for the whole family
	// ([fixed-point-and-scales.md] par.4d: ask the KIND's unit, never the family's).
	python::list getMovementKinds(const std::string& szTypePrefix, int iId, int iScope) const;

	// A CLASSIFICATION test -- O(1) bitset, the parameterized read.
	//
	// ⛔ THIS IS NOT THE PYTHON CONSUMER SURFACE -- A NAMED ENDPOINT IS (owner): "you can easily make a Cy wrapper
	// for a specific skill such as hidden nationality; I want the Cy endpoints to be understandable -- minimal
	// amount of endpoints is not the target here, properly organized is." A consumer therefore calls
	// `INFO.isHiddenNationality(unitId)`, never a test carrying an id ([patterns.md] THE PYTHON READ BOUNDARY).
	// ⚠ The C++ surface is the OPPOSITE and correctly so ([patterns.md] THE GETTER SETUP category 2): there the id
	// IS a compile-time constant, so `hasSkill(CLS_SKILL_BLITZ)` already names the thing at the call site. Python
	// has no such constant, which is why the two planes diverge -- do not "align" them.
	// ⇒ These parameterized reads stay as the SHARED IMPLEMENTATION the named endpoints below delegate to, so a
	// named endpoint costs one line and never a second bitset walk ([DEC-single-implementation]).
	//
	// ⚑ ALL SEVEN DOMAINS, because the plane is only usable if it is COMPLETE. The name encodes HOLD vs PROVIDE
	// exactly as the C++ surface does ([patterns.md] category 2, [json.md] §8): what the entity IS or HAS is
	// `has*`, what it hands to another scope is `provides*` -- a building's `attributes` are its own, its
	// `amenities` go to its CITY, its `capabilities` go to the EMPIRE. Publishing only two of the seven left
	// script with no way to ask the other five and no way to tell that it could not.
	//
	// ⛔ Whose question it is decides the RECEIVER, not this surface: a GATE ("does this city have power?") is
	// the city's FOLD (CyState), while a VALUATION or a DISPLAY ("what would this building give me?") asks the
	// GRANTOR, which is here ([contexts.md] `CityContext.amenities`). Re-pointing a gate at `providesAmenity`
	// would leave it doing exactly what it did before while reading as migrated.
	bool hasSkill(const std::string& szTypePrefix, int iId, int iSkillId) const;
	bool hasTag(const std::string& szTypePrefix, int iId, int iTagId) const;
	bool hasAttribute(const std::string& szTypePrefix, int iId, int iAttributeId) const;
	bool hasCharacteristic(const std::string& szTypePrefix, int iId, int iCharacteristicId) const;
	bool providesAmenity(const std::string& szTypePrefix, int iId, int iAmenityId) const;
	bool providesCapability(const std::string& szTypePrefix, int iId, int iCapabilityId) const;
	// The empire-STATE sibling ([json.md] §9 `policies`) -- same parameterized bitset read, by generated id.
	bool providesPolicy(const std::string& szTypePrefix, int iId, int iPolicyId) const;
	// The REVOKE plane ([skills.md] §4): a PROMOTION authoring `false` takes a skill away. A separate read
	// because absent and revoked are different answers, and `hasSkill` cannot express the second.
	bool revokesSkill(const std::string& szTypePrefix, int iId, int iSkillId) const;

	// ---- The NAMED classification tests -- the Python CONSUMER surface (the owner ruling above). ----
	// One endpoint per key, spelled for what it ASKS, so a reader of the call site knows what is fetched without
	// resolving an id. ⛔ Added ON DEMAND, for the call site that wants it -- never pre-emptively across a
	// registry (the [patterns.md] THE IDENTITY SET rule: restore on demand, named by the call site that wanted it).
	// ⚑ The named form is also what lets the CALLER stay ignorant of which PLANE the answer lives on: hidden
	// nationality is a unit SKILL while spy is a unit TAG ([skills.md] §1, [tags.md]), and no consumer should have
	// to know that to ask a question about a unit.
	bool isHiddenNationality(int iUnitId) const;
	bool isSpy(int iUnitId) const;
	// ⚠ Reads the FOLDED tag set, so a unit whose own block lists no tag still answers true through its combat
	// classes (UNITCOMBAT_ANIMAL / SEA_ANIMAL) -- which is the whole point of the fold ([tags.md]).
	bool isAnimal(int iUnitId) const;
	// Does this unit spread a religion at all (the MISSIONARY test)?
	// ⚖ It replaces a legacy `getPrereqReligion() > -1`, and the change of QUESTION is the point: that asked a
	// BUILD GATE in order to infer a CAPABILITY. The gate is now an ordinary `requires.build` RELIGION_ atom the
	// enabler owns, while what the caller actually wants is authored directly as `spread.religion`
	// ([json.md] §9 -- its own block precisely so a reader is not left inferring it from a prereq).
	bool canSpreadReligion(int iUnitId) const;
	// The SPECIFIC twin -- does it spread THIS religion? The general read above is the missionary test; a
	// recommender choosing a missionary for the empire's own faith needs this one, or it offers a unit that
	// spreads somebody else's.
	bool spreadsReligion(int iUnitId, int iReligion) const;
	// Is this unit WORLD-UNIQUE -- does it author a `world` self-cap ([json.md] §4.4)?
	// ⚑ It is the UNIT twin of the building's PYINT_WONDER_SCOPE, and it is NAMED rather than given a slot for
	// the reason [patterns.md] states: a slot read that falls through an unrouted prefix answers a shared -1 that
	// is indistinguishable from a real verdict, while an unwired NAMED read does not compile.
	// ⚠ The test is `>= 0` (a cap is AUTHORED), never `> 0` -- the enabler's own cap gate reads it the same way,
	// so there is ONE meaning of "capped" rather than two that can drift.
	bool isWorldUnit(int iUnitId) const;
	// Is this TECH world-unique -- inventable once in the whole game (the religion founder techs). Same
	// `allowed.world` cap as isWorldUnit reads, on the tech plane; it is what a trade screen must skip,
	// because a global tech is nobody's to hand over.
	bool isGlobalTech(int iTechId) const;
	// Does this UNIT carry ANY self-cap (world or empire) -- i.e. is it a limited instance rather than
	// something an empire may field freely. ⚠ Read `>= 0` (a cap is AUTHORED), the same test isWorldUnit
	// uses, so "capped" has ONE meaning across the surface.
	bool hasUnitInstanceCap(int iUnitId) const;
	// An AIR unit's base air strength. ⚠ It is NOT the `air` family -- that family authors no AMOUNT at all; the
	// value sits at `identity.base.airCombat`, which is why it is a NAMED read rather than a kind of getAirKinds.
	// ⛔ So a screen picks BETWEEN this and SCALAR_STRENGTH by what the unit actually fights with; summing them
	// would double a plane the unit does not have.
	int getUnitAirCombat(int iUnitId) const;
	// Does this entity SUPPLY that bonus in its city while active (json §5a `provides.bonuses`)?
	// ⛔ Not the merged EDGEF_RELATED bucket: that lands every reference together, so a building which
	// merely REQUIRES the bonus would answer yes to a question about who PRODUCES it.
	bool providesBonus(const std::string& szTypePrefix, int iId, int iBonusId) const;
	// Does this BUILDING play a completion movie? The wonder-movie popup gates on the verdict alone, and the
	// screen behind it resolves the ART_DEF_MOVIE_* tag itself -- so no tag string crosses the boundary.
	bool hasMovie(int iBuildingId) const;

	// ---- PROMOTION facts a consumer classifies the promotion registry by. ----
	// Is this a STATUS pseudo-promotion -- a condition PUT ON a unit for N turns, not an ability it HAS
	// ([state.md]). The two are listed apart because they are different mechanics wearing one carrier.
	bool isStatusPromotion(int iPromotionId) const;
	// Does this promotion belong to a BUILD-UP line? ⚑ The flag lives on the LINE, not on the rung, so this
	// folds the promotion -> line hop rather than making script fetch a line id it has nothing else to do with.
	bool isBuildUpPromotion(int iPromotionId) const;

	// ---- BUILDING facts a consumer classifies a city's buildings by. ----
	// Each is NAMED because Python cannot name a classification id: the parameterized `providesAmenity` carries
	// a CLS_* the caller has no vocabulary for, which is the opaque-slot shape the owner ruling above retires.
	//
	// Is this building the SHRINE of a religion (json §9 `shrine`)? ⚑ It replaces a legacy
	// `getGlobalReligionCommerce() > 0`, and the change of QUESTION is the point: that read a MAGNITUDE in order
	// to infer an identity. The shrine relationship IS the data, and its per-commerce values live on the
	// RELIGION, scaled per city holding it ([json.md] §9) -- so the building's own number never was the fact.
	bool isShrine(int iBuildingId) const;
	// Is this building ASSOCIATED with a religion (its RELIGION_* FK -- a temple, a monastery)?
	// ⚠ Distinct from isShrine: 213 buildings carry the association, 29 are shrines.
	bool isReligiousBuilding(int iBuildingId) const;
	// Is this building placed by the engine's own auto-placement (json §7 `autoBuild` ⊂ `notConstructible`)?
	bool isAutoBuild(int iBuildingId) const;
	// Does this building confer NUKE IMMUNITY on its city (the `nukeImmune` AMENITY)?
	// ⚠ The BUILDING is not the thing protected -- the CITY is ([json.md] §8), which is why this is an amenity
	// the building PROVIDES and not an attribute it HAS. The plot-substrate key of the same name is a
	// CHARACTERISTIC and a different mechanic; the blocks are distinct precisely so they cannot merge.
	bool providesNukeImmunity(int iBuildingId) const;
	// Does this building confer CAPITAL status on its city (the `capital` AMENITY -- the palace)?
	bool providesCapitalStatus(int iBuildingId) const;
	// A HANDICAP's civic-upkeep percentage (`upkeep.empire.civic.percent`) -- the difficulty scaler the combat
	// theft handlers weigh a winner's difficulty against a loser's by.
	// ⚑ NAMED rather than slotted for the same reason as isWorldUnit above, and because the value belongs to ONE
	// registry ([patterns.md]: a value that belongs to one type is named on that type's accessor).
	int getHandicapCivicUpkeepPercent(int iHandicapId) const;
	// A CIVIC's upkeep CLASS -- the `UPKEEP_` id, which the caller then names through the identity plane. The
	// civic carries the FK and the upkeep entity carries the text, so handing back the id keeps the two apart
	// instead of resolving a string here.
	int getCivicUpkeep(int iCivicId) const;
	// The AUTHORED `allowed` cap at one scope, or -1 where the entity authors none.
	// ⚑ Parameterized over the SCOPE axis rather than split into a read per question, because scope is an axis
	// and not part of a name ([DEC-scope-is-an-axis]) -- so one read serves buildings, units, techs and projects,
	// and a wonder CATEGORY cap is the same read with a category cap id.
	// ⚠ -1 means UNCAPPED, so a caller tests `>= 0` for "is capped" and prints the value only above it; reading
	// it as a count would report every uncapped entity as capped at minus one.
	int getAllowedCap(const std::string& szTypePrefix, int iId, int iCap) const;
	// A FEATURE's per-turn spread and vanish odds (`identity.growth` / `identity.disappearance`). Both are plain
	// authored numbers the caller scales by gamespeed itself -- the engine hands over what the data says and
	// converts nothing for display.
	// ⚑ NAMED, because they belong to ONE registry: a slot read would leave the call site naming a slot rather
	// than the thing ([patterns.md]).
	int getFeatureGrowthProbability(int iFeatureId) const;
	int getFeatureDisappearanceProbability(int iFeatureId) const;
	// A CIVILIZATION's OWN authored lists -- the leaderheads that may lead it, and its city-name pool.
	// ⚑ These are the civ's own data, so the read hands the list over. Asking every leaderhead whether it
	// belongs to this civ is the own-data inversion the reverse-view rule names, and it is what these replace.
	// ⚠ The city names are TXT KEYS, not resolved text: the caller resolves them, because TXT is not this
	// surface's to own ([patterns.md] -- the library serves the raw key reference).
	python::list getCivilizationLeaders(int iCivilizationId) const;
	python::list getCivilizationCityNames(int iCivilizationId) const;

	// The `canTrade` block -- what this entity puts on the trade table (capabilities.md). Deliberately
	// STRING-keyed: canTrade keys are open DATA, not classification-registry ids, so the key IS the vocabulary.
	// ⚠ Cold display/AI path only; it is not on any turn read.
	bool canTradeItem(const std::string& szTypePrefix, int iId, const std::string& szItem) const;

	// The DIPLOMACY_ RESPONSE plane -- a comment's candidate responses, addressed by (comment, response).
	// ⚑ NAMED on the type rather than routed through the generic prefix plane: a response belongs to ONE
	// registry, so the call site says what it fetches ([patterns.md] THE PYTHON READ BOUNDARY).
	// ⛔ These serve the FILTER, never the choice: which response a leader gives is picked in Python
	// (CvDiplomacy.filterUserResponse weighs attitude / civ / leader / power and rolls the ASYNC stream, which
	// is correct -- a cosmetic line must not touch the synchronized RNG, [DEC-synced-rng-is-shared-state]).
	// Porting that selection into C++ belongs to the events move, not here.
	// ⚠ Every read is total: an out-of-range comment or response answers 0/false/"" rather than raising, because
	// the caller walks these indices to DISCOVER which responses apply.
	int getDiplomacyNumResponses(int iComment) const;
	bool getDiplomacyResponseAttitude(int iComment, int iResponse, int iAttitude) const;
	bool getDiplomacyResponseCivilization(int iComment, int iResponse, int iCivilization) const;
	bool getDiplomacyResponseLeaderHead(int iComment, int iResponse, int iLeaderHead) const;
	bool getDiplomacyResponsePower(int iComment, int iResponse, int iPower) const;
	int getDiplomacyNumText(int iComment, int iResponse) const;
	std::string getDiplomacyText(int iComment, int iResponse, int iText) const;

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

	// The entity's OWN authored flat deposits for a scope, one entry per channel -- the cascade-shaped
	// replacement for the per-type `getYieldProduced` / `getCommerceProduced` accessors that no longer exist.
	// x100 like every amount ([DEC-fixedpoint-x100]); the reader divides at the point of use.
	python::list getFlatYields(const std::string& szTypePrefix, int iId, int iScope) const;
	python::list getFlatCommerces(const std::string& szTypePrefix, int iId, int iScope) const;
	// ---- THE WHAT-IF: what this CANDIDATE would ADD to that city, resolved against its live contexts ----
	// ⚖ The valuation protocol ([patterns.md] § THE VALUATION PROTOCOL): the contexts go IN and the resolved
	// DELTA comes out -- never the raw authored deposit and never the new total. The context IS the current
	// value, so a percent resolves against the base it multiplies and the conditioned tail is evaluated
	// through the ONE evaluator; a caller passes no amounts of its own.
	// ⛔ These are NOT the `get*` reads above wearing a city argument. Those answer *what do I CARRY* off the
	// authored data ([patterns.md] § THE TWO READ ROLES); these answer *what would I GAIN HERE*, and the two
	// must never look interchangeable. A recommender asks THIS one.
	// ⚑ ONE call answers a whole group, and the SAME call answers the AI's weighting and the build-list hover
	// tooltip -- which is what keeps the number a player is shown and the number the AI plans against the same
	// number, structurally ([DEC-single-implementation]).
	// x100 like every amount; percents are unscaled ([DEC-fixedpoint-x100]).
	// ⚠ Answers an empty list when the (player, city) pair does not resolve -- a caller tests it rather than
	// reading a zero as "this candidate is worth nothing".
	python::list expectedWellbeing(const std::string& szTypePrefix, int iId, int iPlayer, int iCity) const;
	python::list expectedFlatYields(const std::string& szTypePrefix, int iId, int iPlayer, int iCity) const;
	// The PLOTS-TARGET contribution in one read -- each plots-target deposit scaled by the city context's
	// STORED `plotAttrs` count, so a "+1 food per water tile" answers against the tiles this city actually
	// has. It is the read that retires the legacy per-PlotType table: `PLOT_OCEAN` is not a key any more
	// ([json.md] §6.1 -- a water plot is `plots {IS_WATER}`), and the two shapes the data now authors (a
	// terrain-keyed flat and a predicate-gated plots entry) are BOTH folded here.
	python::list expectedPlotYields(const std::string& szTypePrefix, int iId, int iPlayer, int iCity) const;
	python::list expectedFlatCommerces(const std::string& szTypePrefix, int iId, int iPlayer, int iCity) const;
	// The grouped-family what-ifs, over the family's own kind enum.
	python::list expectedCommerceModifiers(const std::string& szTypePrefix, int iId, int iPlayer, int iCity) const;
	python::list expectedDefenseKinds(const std::string& szTypePrefix, int iId, int iPlayer, int iCity) const;

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
