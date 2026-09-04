#pragma once
#ifndef CV_EVENT_SPINE_H
#define CV_EVENT_SPINE_H

#include <vector>

//
//	CvEventSpine -- the GENERAL state-change event dispatch primitive (design: docs/specs/event-spine.md).
//
//	These are GAME domain state-change events (building built, unit created, tech acquired, ...) dispatched by a
//	general spine. The spine is NOT cascade-owned: the cascade is just ONE consumer that READS these events as
//	inputs, alongside LOGGING, `grants`, and the out-of-process OOS-replay. Callers emit(KIND, type, payload) to
//	the spine; each consumer reads the event KINDS it cares about. The registered consumers are LOGGING and
//	`grants`. The TALLY is NOT a spine consumer -- it READS the object-owned counts on demand (tally.md); the
//	DOMAIN count events serve observability + the out-of-process replay, never a tally feed.
//
//	KIND is the OOS FIREWALL axis, declared at the call site (never inferred):
//	  - DOMAIN     : game STATE changed (building built, unit created). SYNCED/deterministic -> gate-eligible;
//	                 drives the grants consumer.
//	  - SAVELOAD   : a fact was READ OFF THE SAVE STREAM. A LOG OF LOADING, never what sets state -> logging only.
//	  - DIAGNOSTIC : code RAN (a function entered, a decision re-evaluated N times). UNSYNCED execution trace ->
//	                 logging only; NEVER gates.
//	  - TRACE      : fine-grained "show me every step" -> logging only.
//
//	⛔ WHY SAVELOAD IS ITS OWN KIND AND NOT A DIAGNOSTIC (owner). DIAGNOSTIC means CODE RAN; a save-load fact is a
//	record of what the STREAM CONTAINED, which is a different statement. Filing it under DIAGNOSTIC would put "the
//	save says this plot is TERRAIN_GRASS" in the same bucket as "this function was entered", after which the two
//	are separable only by convention. Its own kind makes the rule STRUCTURAL instead: a state-building consumer
//	registers for DOMAIN, so "nothing derives held state from the load log" is enforced by the interest mask rather
//	than by reviewer memory -- the contract-not-prohibition shape (patterns.md). It sits on the UNSYNCED side of
//	the firewall with DIAGNOSTIC: never counted, never gates.
//	⚑ It needs no gate knob of its own -- volume rides the event's OWN iLevel through the existing file
//	(gPlayerLogLevel) and stream (gStreamLogLevel) gates, so a load record costs nothing until it is turned on.
//	⛔ The load's DOMAIN facts are NOT these: the save read populates base state through the objects' internal
//	setters, and THOSE setters emit the ordinary DOMAIN facts that build the cascade, the enabler and the contexts
//	-- one mechanism for load and play. A SAVELOAD line is testimony ABOUT the read, beside them.
//
//	Two consumer appetites, one front door: LOGGING is BROAD (sees everything, outputs per the existing log gates);
//	GRANTS is SELECTIVE (takes only the DOMAIN kinds it resolves). Payload is RAW (never a pre-formatted string) so
//	the costly index->text formatting defers to the gated logging consumer.
//
//	C++03 / VC7.1: virtual interface, no lambdas, no Boost (event-spine.md §6). This spine is the dispatch primitive.
//
enum EventKind
{
	EVENTKIND_DOMAIN = 0,   // synced state change -> grants + logging; gate-eligible
	EVENTKIND_SAVELOAD,     // a fact READ OFF THE SAVE STREAM -> logging only; NEVER builds state
	EVENTKIND_DIAGNOSTIC,   // unsynced execution trace -> logging only
	EVENTKIND_TRACE,        // fine-grained step trace -> logging only
	NUM_EVENT_KINDS
};

//	======================= the RAW field payload (event-spine.md §3, RESOLVED 2026-06-18) =======================
//	A logging-only (DIAGNOSTIC/TRACE) event carries its line as RAW FIELDS, never a formatted string -- the gated logging
//	consumer renders them. Chosen shape (from the Stage-0 field catalog, logging-field-catalog.md): a generic typed-slot
//	array (median 5-6 fields, <=16 operational). The line = a constant PREFIX (the [TAG] + any constant text, keyed by
//	iDomainTag+iEventId) followed by each field rendered "name=value" -- so constant labels live in the prefix, only the
//	VARIABLE numeric fields are slots (no string fields, no per-line format registry; lines that don't fit pure
//	[TAG] key=value get redone during migration -- owner "drop/redo" ruling). PERF is the exception (its own struct, later).

//	How a field slot's int payload renders. The TAG implies the type (one table), so the slot stays 8 bytes -- the int
//	payload is reinterpreted per the type (typeIndex kinds resolve to a name via GC.getXInfo in the consumer).
enum SpineFieldType
{
	SFT_INT = 0, SFT_FLOAT, SFT_BOOL,
	// String POINTER kinds: the field carries a POINTER to an EXISTING string (no copy, no call-site concat) -- the
	// consumer renders it. For genuinely free-text data (a szReason already built for other logic) that has no id/type to
	// resolve. NOT call-site composition (owner 2026-06-19, event-spine.md §3): the line is still assembled in the
	// consumer; the call site just hands over the pointer. Lifetime: the pointee must outlive the SYNCHRONOUS emit (render
	// happens then, on the game thread) -- a literal, a member, or a still-in-scope local. SFT_STR = narrow, SFT_WSTR = wide.
	SFT_STR, SFT_WSTR,
	// typeIndex kinds: the int is a Types index, rendered to its type string via GC.getXInfo (resolution in the consumer).
	// This is how a former "%s = GC.getXInfo(i).getType()" line becomes a clean raw field (the index travels, not the string).
	SFT_BUILDING, SFT_UNIT, SFT_TECH, SFT_PLAYER,
	SFT_BONUS, SFT_IMPROVEMENT, SFT_PROMOTION, SFT_RELIGION, SFT_CORPORATION, SFT_FEATURE, SFT_TERRAIN, SFT_CIVIC, SFT_PROJECT, SFT_SPECIALIST,
	SFT_TRAIT, SFT_ROUTE, SFT_COMMERCE, SFT_PROPERTY,
	// The unit plane's combat CLASS (UnitCombatTypes) -- resolved via GC.getUnitCombatInfo, bounded by
	// GC.getNumUnitCombatInfos(). Distinct from SFT_UNIT (the unit TYPE) and from SFT_PROMOTION.
	SFT_UNITCOMBAT,
	// A DIPLOMATIC REFUSAL (DenialTypes -> GC.getDenialInfo). It is what the AI answers when asked whether it
	// would trade a given item, so it IS the reason an existing deal stops being acceptable -- rendered by name
	// because "denial=7" is unreadable and the whole point of the fact is the reason.
	// ⚠ NO_DENIAL is -1, i.e. "it did not refuse", which renders as "none" rather than as an out-of-range "?".
	SFT_DENIAL
};

//	Field identities are DOMAIN-LOCAL (owner 2026-06-18): each migrated domain defines its OWN field-tag enum + a
//	resolver mapping tag -> (name, type), registered with the spine (below). The spine holds NO global field registry --
//	this isolates each domain (Clean-Architecture), kills the fragile central enum-plus-two-parallel-tables sync, and lets
//	domains migrate in PARALLEL with zero shared edits. (Constant labels like "action=safety" are NOT fields -- they live
//	in the event prefix.) A field slot's `eTag` is interpreted only together with the event's `iDomainTag`.
struct CvSpineEventField
{
	int eTag; // a DOMAIN-LOCAL field tag (resolved to name+type by the domain's registered SpineFieldInfoFn)
	union { int i; float f; const char* s; const wchar_t* w; } v; // pointers are 4B on x86 -> the slot stays POD/8B
};

//	Domain discriminator -- selects the [TAG] family and (with iEventId) the constant line prefix. Grows per migrated domain.
enum SpineDomainTag
{
	SD_NONE = 0,
	SD_HUNTER,     // [HAI] roaming-attacker AI (CvHunterAI) -- the pilot
	SD_WAR,        // [WAR] team-war (CvTeamAI)
	// Pre-allocated so each domain migrates with ZERO shared-file edits (parallel-safe); a domain is "live" once it
	// self-registers (spineRegisterDomain) in its own .cpp. Unregistered tags simply never emit.
	SD_WORKER,     // [WAI] worker build evaluation (CvWorkerAI)
	SD_CITY,       // [CIT] city production (CvCityAI / CvCity)
	SD_UNIT,       // [UNT] unit AI dispatch (CvUnitAI / CvSelectionGroupAI)
	SD_COMBAT,     // [COM] combat (CvUnitAI / CvSelectionGroupAI)
	SD_GROUP,      // [GRP] group & army (CvSelectionGroupAI / CvArmy)
	SD_FOUND,      // [FND] founding/settle (CvUnitAI::AI_found)
	SD_DECISION,   // [DAI] flavour decisions (CvDecisionAI)
	SD_DIPLO,      // [DIP] diplomacy/deals (CvPlayerAI / CvDeal)
	SD_ESPIONAGE,  // [ESP] espionage (CvPlayerAI)
	SD_CONTRACT,   // [CTB] contract broker (CvContractBroker)
	SD_ENGINE,     // [ENG] engine integrity (CvPlot)
	SD_GRAPHICS,   // [GFX] what the SCENE is asked to hold: the plot's centre-unit verdict, and real-vs-dummy unit
	               // entities with their running counts (CvGraphicsTrace). The entity counts existed only as an
	               // every-100 OutputDebugString, i.e. on no surface at all -- observability.md's "a fact that is
	               // not on either surface is EMITTED".
	// #430 cascade diagnostic domains (per-emitter, one file -- all tee to Cascade.log). Each self-registers in its
	// own .cpp (spineRegisterDomain); the [TAG] sub-area lives in the per-domain prefix fn ([READJSON/*],
	// [MODIFIER/*]). Diagnostic lines (EVENTKIND_DIAGNOSTIC) -- census/diagnostic traces, logging only.
	SD_READJSON,   // [READJSON] the JSON->InfoRepo load census (CvReadJson)
	SD_ENABLER,    // [ENABLER] reserved (historical tag; no live registrant)
	SD_MODIFIER,   // [MODIFIER] the per-scope channel-set census (CvCascadeChannelRegistry)
	SD_TRIGGERS,     // [TRIGGERS] the payload plane: a `triggers` entry is the general form, and a `grants` block
	                 // is its degenerate case (implicit happening, no condition, no roll) -- json.md §5
	SD_SPINE,      // [SPINE] spine lifecycle signals (game-load started/finished) -- rendered via the registered prefix, not inline
	SD_EXPERIENCE, // [XP] every unit experience grant, with the CALLER that asked for it (CvUnit::changeExperience100).
	               // XP had NO surface at all -- a unit's `level` was the only thing on the wire -- so a grant could not
	               // be attributed to its source without reading code (observability.md: a fact that is on neither
	               // surface is EMITTED, never given a side-counter behind a route).
	NUM_SPINE_DOMAINS
};

//	A domain supplies its eventId -> constant line-prefix mapping through this contract, registered at startup -- so the
//	spine stays domain-AGNOSTIC (it never names a domain; the domain self-registers, Clean-Architecture isolation). The
//	prefix is the [TAG] + any constant labels (e.g. "[HAI/begin] phase=hunterMove"); variable fields follow as name=value.
typedef const char* (*SpineLinePrefixFn)(int iEventId);
// A domain resolves one of its LOCAL field tags to a name + render-type: returns the field name (NULL if the tag is
// unknown) and sets *peType. The renderer calls this per field slot, so field name/type knowledge lives in the domain,
// never in the spine. C++03 free-function pointer (no captures); SFT_INT is a safe default for an unknown tag.
typedef const char* (*SpineFieldInfoFn)(int iFieldTag, SpineFieldType* peType);
// A domain self-registers (at startup) its prefix provider, its destination .log file, AND its field-info resolver --
// so the spine stays fully domain-agnostic AND per-domain files are kept (R-2). szLogFile NULL => Cascade.log;
// fieldFn NULL => fields render as "fN=value" by index (a safe fallback). (e.g. [HAI] -> "HunterAI.log".)
void spineRegisterDomain(int iDomainTag, SpineLinePrefixFn prefixFn, const char* szLogFile, SpineFieldInfoFn fieldFn);

//	The cap on slots per event -- 16 covers every operational AI line (97th pct; widest operational = [WAI/score] @ 16).
static const int SPINE_MAX_FIELDS = 16;

//	A spine event: KIND (firewall axis) + a RAW, self-describing payload (NEVER a formatted string). TWO payloads,
//	carried TOGETHER (not exclusive modes): the DOMAIN state ints iType + iA/iB/iC (+ iSrcLoc) that grants / the
//	cache-invalidation consumer read; AND the render payload iDomainTag + iEventId (-> the constant line prefix) +
//	aFields[iFieldCount] (the variable "name=value" fields) that the ONE logging path formats. A DOMAIN event carries
//	BOTH (its emit endpoint tags SD_SPINE + adds fields); a DIAGNOSTIC/TRACE event carries only the render payload.
struct CvSpineEvent
{
	EventKind eKind;
	int iEventId;
	int iType;
	int iA;
	int iB;
	int iC;
	int iSrcLoc;      // DOMAIN source LOCATION id (cityId | plotId, per the event kind; -1 = empire/world scope) --
	                  // lets a source-carrying event name WHERE it happened so a consumer can route it to the
	                  // affected packages (state-repositories.md: "a DOMAIN event carries its SOURCE"). Non-count
	                  // events set it; the legacy count events leave it -1 (their footprint is the whole empire).

	int iDomainTag;   // SpineDomainTag -- the line's [TAG] family (every emitted event tags one; SD_NONE only if a
	                  // caller forgot -- spineRenderEventLine still renders it via the generic prefix/field fallback)
	int iLevel;       // the surveillance level this line emits at (1 Telescreen .. 4 Thought Police); consumer gates on it
	int iFieldCount;  // number of valid aFields (the DOMAIN state ints iType/iA/iB/iC ride alongside for the machine consumers)
	CvSpineEventField aFields[SPINE_MAX_FIELDS];

	// DOMAIN constructor: the state ints + iSrcLoc (-1 = empire/world scope; a per-city / per-plot event passes its
	// cityId / plotId so the event names WHERE it happened). Defaults iDomainTag = SD_NONE / iFieldCount = 0; the emit
	// endpoint then sets iDomainTag = SD_SPINE and appends render fields via addI (so the event renders like any other).
	CvSpineEvent(EventKind eKind_, int iEventId_, int iType_ = -1, int iA_ = 0, int iB_ = 0, int iC_ = 0, int iSrcLoc_ = -1)
		: eKind(eKind_), iEventId(iEventId_), iType(iType_), iA(iA_), iB(iB_), iC(iC_)
		, iSrcLoc(iSrcLoc_), iDomainTag(SD_NONE), iLevel(1), iFieldCount(0) {}

	// Logging-event constructor (DIAGNOSTIC/TRACE): domain + event id + the level it emits at, then add fields. The
	// domain param is the SpineDomainTag ENUM (not int) -- this disambiguates from the legacy int-eventId ctor above.
	CvSpineEvent(EventKind eKind_, SpineDomainTag eDomainTag_, int iEventId_, int iLevel_)
		: eKind(eKind_), iEventId(iEventId_), iType(-1), iA(0), iB(0), iC(0)
		, iSrcLoc(-1), iDomainTag((int)eDomainTag_), iLevel(iLevel_), iFieldCount(0) {}

	// Append a raw field by its DOMAIN-LOCAL tag (int or typeIndex). No-op past the cap (a backstop; lines <=16 by the catalog).
	CvSpineEvent& addI(int iFieldTag, int iValue)
	{
		if (iFieldCount < SPINE_MAX_FIELDS) { aFields[iFieldCount].eTag = iFieldTag; aFields[iFieldCount].v.i = iValue; ++iFieldCount; }
		return *this;
	}
	CvSpineEvent& addF(int iFieldTag, float fValue)
	{
		if (iFieldCount < SPINE_MAX_FIELDS) { aFields[iFieldCount].eTag = iFieldTag; aFields[iFieldCount].v.f = fValue; ++iFieldCount; }
		return *this;
	}
	// Carry a POINTER to an EXISTING string (no copy, no concat). The pointee must outlive the synchronous emit (the
	// consumer renders then). The domain's field resolver must return SFT_STR / SFT_WSTR for this tag. See SpineFieldType.
	CvSpineEvent& addStr(int iFieldTag, const char* szValue)
	{
		if (iFieldCount < SPINE_MAX_FIELDS) { aFields[iFieldCount].eTag = iFieldTag; aFields[iFieldCount].v.s = szValue; ++iFieldCount; }
		return *this;
	}
	CvSpineEvent& addWStr(int iFieldTag, const wchar_t* szValue)
	{
		if (iFieldCount < SPINE_MAX_FIELDS) { aFields[iFieldCount].eTag = iFieldTag; aFields[iFieldCount].v.w = szValue; ++iFieldCount; }
		return *this;
	}
};

//	Render a logging event's RAW fields into szBuf as "<prefix> name=value name=value ..." (the consumer's job).
//	prefix from the domain's registered prefix provider; field names/types from the domain's registered SpineFieldInfoFn. Declared here,
//	defined in the .cpp beside the logging consumer.
void spineRenderEventLine(char* szBuf, int iBufSize, const CvSpineEvent& kEvent);

//	Real (non-test) DOMAIN event ids -- WHAT changed in synced game state. Distinct namespace from the temporary
//	DIAGNOSTIC ids in CvCascadeSelfTest (the KIND prefix in the log disambiguates). This is the production side: real
//	gameplay state-changes emit these, so the spine's consumers are driven by genuine input, not a recompute.
enum SpineDomainEvent
{
	// ⛔ A FACT NAMES THE HAPPENING. `*_CHANGED` IS NOT A VALID EVENT NAME -- there is no exempt category
	// (event-spine.md § A FACT NAMES THE HAPPENING; docs/spine.md §A FACT NAMES THE HAPPENING). Every fact below is
	// `<SCOPE>_<THING>_ADDED` / `_REMOVED`: the EVENT is the OPERATOR, and the payload is ONLY EVER A MAGNITUDE.
	// A fact may carry HOW MANY (an ADDED of 3 adds three times over); it must never carry WHICH WAY -- a ±1, a
	// presence bool or an old value beside a new one are all the same defect, a discriminator the consumer must
	// branch on, which is the calculation relocated into a `switch`.
	// ⚑ A SLOT REPLACEMENT announces BOTH ends, REMOVED first: the withdrawal is emitted while the OLD STATE STILL
	// HOLDS, which is what makes it exact ([state-repositories.md] § THE INVARIANT). emit() dispatches
	// SYNCHRONOUSLY, so no two operands are ever in flight together.
	// ⚑ Ids are grouped by SCOPE and sequential. They are an internal dispatch axis with no external contract --
	// the rendered NAME is the wire form -- so a new fact is appended to its scope's band, never bolted on the end.

	// ===== GAME / lifecycle =====
	SEVT_GAME_LOAD_STARTED          = 1,
	SEVT_GAME_LOAD_FINISHED         = 2,
	// TURN BOUNDARIES. The turn counter advancing is a genuine synced state change, so both are DOMAIN. They bracket
	// the real boundary: the GAME pair straddles CvGame::incrementGameTurn (ended = the closing turn, started = the
	// incremented one); the PLAYER pair rides CvPlayer::setTurnActive. iType = the game turn, iC = the player
	// (-1 = the GAME-scope boundary). Consumers filter on iC; the emit surface stays complete.
	SEVT_TURN_STARTED               = 3,
	SEVT_TURN_ENDED                 = 4,
	// A GAME OPTION was turned ON / OFF (CvGame::setOption / setModderGameOption). An option is the ONE axis an
	// entity-level gate reads (docs/specs/json.md §2 (Applicability row) + docs/specs/enabler.md §WHAT THE ENABLER IS NOT), so a flip can change ANY entity's applicability at once, and
	// WorldBuilder can toggle one at will -- without this fact every maintained gate verdict silently keeps the
	// pre-flip answer and NOTHING re-derives it (docs/cascade.md §A SELF-HEAL IS THE FOSSIL OF A MISSING EMIT).
	// ⚠ TWO ID SPACES ride these, so iB DISAMBIGUATES (GameOptionSpace) -- a game-option id and a modder-option id
	// are otherwise the same int. iType = the option id, iA = the value's magnitude. DOMAIN.
	SEVT_GAME_OPTION_ADDED          = 5,
	SEVT_GAME_OPTION_REMOVED        = 6,
	// The GAME handicap moved (CvGame::setHandicapType) -- the integer average over alive HUMAN players, which every
	// getAI* advantage reads (engine.md: AI advantages scale with the HUMAN's difficulty, never the AI's). A SLOT
	// REPLACEMENT: REMOVED names the outgoing handicap, ADDED the incoming. Derived, never saved, so no in-read half.
	// iType = the handicap. DOMAIN.
	SEVT_GAME_HANDICAP_ADDED        = 7,
	SEVT_GAME_HANDICAP_REMOVED      = 8,
	// A GLOBAL DEFINE took a new value (cvInternalGlobals::setDefine*). A define is MP-SYNCED state, and this is the
	// LIVE-OPTION bridge: a BUG option fires a Python callback -> GC.setDefineINT -> cacheGlobals(), so a user can
	// flip an engine tunable AT ANY TIME mid-game. A SLOT REPLACEMENT, so the old value is REMOVED and the new one
	// ADDED. ⚠ Emitted ONLY on the genuine LOCAL set: the `bUpdate` path sends a net message and
	// CvGlobalDefineUpdate::Execute calls back with bUpdate=false, so emitting on both would double-announce.
	// ⚠ A define is STRING-KEYED with no id space, so the NAME rides as a render field and a machine consumer keys
	// on that, not the ints. iType = -1, iA = the INT magnitude, iB = GlobalDefineKind. DOMAIN.
	SEVT_GAME_GLOBAL_DEFINE_ADDED   = 9,
	SEVT_GAME_GLOBAL_DEFINE_REMOVED = 10,

	// ===== WORLD =====
	// The world's NUKE BAN was imposed / lifted -- the AP/UN no-nukes vote or option, which flips EVERY player at
	// once. ⛔ DISTINCT from the per-player availability pair below, and deliberately a SEPARATE FACT rather than a
	// tri-state on one id: "the world banned nukes" and "this empire can build one" are two happenings reaching one
	// choke point, and a consumer branching on a state int to tell them apart is the calculation in a `switch`.
	// The cascade's NO_NUKES predicate reads only this world half. iC = -1 (world scope). DOMAIN.
	SEVT_WORLD_NUKES_BANNED_ADDED   = 11,
	SEVT_WORLD_NUKES_BANNED_REMOVED = 12,
	// The world's cumulative created-count of a unit type advanced (CvGame::incrementUnitCreatedCount) -- read live
	// by the UnitEnabler's world-instance cap. ⛔ MONOTONIC, so there is no REMOVED half and that is not an omission:
	// the counter only ever grows, so every increment IS a distinct state change with no verdict to cross. DISTINCT
	// from SEVT_EMPIRE_UNIT_COUNT_* (a player's LIVE per-type tally) and from SEVT_UNIT_CREATED (the instance); all
	// three fire at one birth and none duplicates another. iType = unit TYPE, iA = HOW MANY. DOMAIN.
	SEVT_WORLD_UNIT_CREATED_COUNT_ADDED = 13,
	// EVERY area identity was reassigned (CvMap::recalculateAreas). The ONE wholesale-reassignment fact, so it
	// carries no id: after it, EVERY holder of an area id must re-read. Areas are virtually never recalculated
	// (terrain levelled to sea level -- the WMD mechanic -- plus map generation), so the blanket costs nothing at
	// its real frequency, and it is NOT the banned self-heal: a wholesale identity reassignment is not addressable
	// per-source, so no finer route exists to derive. No payload -- the fact IS "all of them". DOMAIN.
	SEVT_AREAS_RECALCULATED         = 14,

	// ===== TEAM =====
	// A team GAINED / LOST a member (CvTeam::changeNumMembers) -- the `TEAM` counter token
	// (EmpireContext::teamMemberCount). iA = HOW MANY, unsigned. iC = -1 (a team has no owning player),
	// iSrcLoc = teamId. DOMAIN.
	SEVT_TEAM_MEMBER_ADDED          = 20,
	SEVT_TEAM_MEMBER_REMOVED        = 21,

	// ===== EMPIRE (player) =====
	// The team GAINED / LOST a tech (CvTeam::setHasTech). Tech is TEAM-held but the fact is emitted per-self from
	// each member's read, so it reads at empire scope like every other HAVE axis ([contexts.md]: team is the TECH
	// BRIDGE, the player holds the context). iType = Tech, iC = triggering player. DOMAIN.
	SEVT_EMPIRE_TECH_ADDED          = 30,
	SEVT_EMPIRE_TECH_REMOVED        = 31,
	// The player GAINED / LOST a trait (CvPlayer::processTrait / setHasTrait). iType = Trait, iC = player. DOMAIN.
	SEVT_EMPIRE_TRAIT_ADDED         = 32,
	SEVT_EMPIRE_TRAIT_REMOVED       = 33,
	// A project was COMPLETED / lost (CvTeam::changeProjectCount + the load reseed). PER-MEMBER -- one emit per
	// alive team member. iA = HOW MANY, unsigned. iType = Project, iC = member player. DOMAIN.
	SEVT_EMPIRE_PROJECT_ADDED       = 34,
	SEVT_EMPIRE_PROJECT_REMOVED     = 35,
	// A player HERITAGE was acquired / lost (CvPlayer::setHeritage) -- feeds the empire package's era-stacked
	// commerce flats. Acquired mid-game via MISSION_HERITAGE. iType = Heritage, iC = player. DOMAIN.
	SEVT_EMPIRE_HERITAGE_ADDED      = 36,
	SEVT_EMPIRE_HERITAGE_REMOVED    = 37,
	// The player ADOPTED / RENOUNCED a state religion (CvPlayer::setLastStateReligion). A SLOT REPLACEMENT, so a
	// swap announces both ends -- REMOVED for the outgoing religion first, then ADDED for the incoming.
	// iType = Religion, iC = player. DOMAIN.
	SEVT_EMPIRE_STATE_RELIGION_ADDED   = 38,
	SEVT_EMPIRE_STATE_RELIGION_REMOVED = 39,
	// The player ENTERED / LEFT a golden age (CvPlayer::changeGoldenAgeTurns, at its 0-crossing). iC = player. DOMAIN.
	SEVT_EMPIRE_GOLDEN_AGE_ADDED    = 40,
	SEVT_EMPIRE_GOLDEN_AGE_REMOVED  = 41,
	// The player ENTERED / LEFT anarchy (CvPlayer::changeAnarchyTurns, at its 0-crossing). Anarchy zeroes the
	// empire's commerce and suspends civic/corporation effects, so IS_ANARCHY is a live cascade input.
	// iC = player. DOMAIN.
	SEVT_EMPIRE_ANARCHY_ADDED       = 42,
	SEVT_EMPIRE_ANARCHY_REMOVED     = 43,
	// The player's ERA advanced (CvPlayer::setCurrentEra) -- a BROAD player-scope cascade input: heritage era-stacked
	// commerce, every ERA-counter-threshold deposit, and ERA requires atoms. A SLOT REPLACEMENT: the outgoing era is
	// REMOVED, the incoming ADDED. iType = the era, iC = player. DOMAIN.
	SEVT_EMPIRE_ERA_ADDED           = 44,
	SEVT_EMPIRE_ERA_REMOVED         = 45,
	// The player's own SAVED handicap moved (CvPlayer::setHandicap), as FLEXIBLE DIFFICULTY does in play. A genuine
	// cascade input rather than observability: the gather folds the handicap's OWN modifier families into that
	// player's packages, so without this fact every handicap-derived deposit keeps the OLD difficulty permanently.
	// ⚠ DISTINCT from the GAME handicap above and not a duplicate of it. A SLOT REPLACEMENT.
	// iType = the handicap, iC = player. DOMAIN.
	SEVT_EMPIRE_HANDICAP_ADDED      = 46,
	SEVT_EMPIRE_HANDICAP_REMOVED    = 47,
	// The empire's NUKE AVAILABILITY -- it built a nuke-enabling building (`m_bNukesValid`, `makeNukesValid` at
	// CvCity processBuilding) or lost it. PER-PLAYER; the world BAN is its own fact above. iC = player. DOMAIN.
	SEVT_EMPIRE_NUKES_ENABLED_ADDED   = 48,
	SEVT_EMPIRE_NUKES_ENABLED_REMOVED = 49,
	// A commerce SLIDER moved (CvPlayer::setCommercePercent) -- the empire's split of its cities' COMMERCE yield
	// across gold / research / culture / espionage. Synced player state, deterministic and OOS-relevant, so DOMAIN:
	// every city's realized rate of the moved channel is built on it (modifier.md §2a).
	// ⚠ ONE slider move emits SEVERAL facts -- the setter REBALANCES the other channels in place to hold the total
	// at 100, writing them directly rather than recursing, so each channel it moves emits its own. A consumer
	// reading only the caller's channel sees a 100-total that does not add up.
	// iType = CommerceTypes, iA = HOW MANY percent points moved (unsigned), iC = player. DOMAIN.
	SEVT_EMPIRE_COMMERCE_PERCENT_ADDED   = 50,
	SEVT_EMPIRE_COMMERCE_PERCENT_REMOVED = 51,
	// The empire's CAPITAL slot moved -- relocation after the old capital was lost (CvPlayer::findNewCapital, once
	// it has PICKED the replacement), or a capital being established. A SLOT REPLACEMENT, and the REMOVED half is
	// load-bearing: an IS_CAPITAL-gated grant cannot be withdrawn by re-evaluating its gate after the flip.
	// iC = owner, iSrcLoc = the city. DOMAIN.
	SEVT_EMPIRE_CAPITAL_ADDED       = 52,
	SEVT_EMPIRE_CAPITAL_REMOVED     = 53,
	// The player's per-TYPE empire tally of a building / unit moved -- the whole-empire count (iSrcLoc = -1),
	// distinct from the per-city presence facts. iType = the type, iA = HOW MANY, iC = player. DOMAIN.
	SEVT_EMPIRE_BUILDING_COUNT_ADDED   = 54,
	SEVT_EMPIRE_BUILDING_COUNT_REMOVED = 55,
	SEVT_EMPIRE_UNIT_COUNT_ADDED       = 56,
	SEVT_EMPIRE_UNIT_COUNT_REMOVED     = 57,

	// ===== CITY =====
	// The city GAINED / LOST a building. TWO facts, because they are two HAPPENINGS -- an add owes its first-build
	// payload, joins the operate fixpoint and confers its amenities; a remove withdraws deposits, repeals those
	// amenities and can ripple dormancy.
	// ⛔ There is deliberately NO constructed-vs-granted split: "the only difference between a building granted and
	// a building constructed is that we didn't use production if granted", and NOTHING downstream may branch on it
	// (triggers.md § A GRANTED ENTITY IS AN ORDINARY ENTITY). Both arrivals are this one fact.
	// iA = bFirst on the ADD (1 = a genuine first acquisition in this city, 0 = conquest transfer / load restore) --
	// NOT how it arrived, but whether the first-build payload is OWED: CvCity::setupBuilding runs that block only
	// when bFirst, and CvPlayer::acquireCity re-adds every captured building with bFirst=false precisely so conquest
	// does not re-fire the grants. The reseed passes 0 for the same reason -- a load is a restore, not a build.
	// iType = Building, iC = owner, iSrcLoc = cityId. DOMAIN.
	SEVT_CITY_BUILDING_ADDED        = 60,
	SEVT_CITY_BUILDING_REMOVED      = 61,
	// ⚖ THE OPERATE CROSSING -- a present building STARTED or STOPPED contributing. DOMAIN because the verdict is
	// genuine synced state: it is the enabler's operate fixpoint (enabler.md §3.2), so the enabler announces it at
	// the one place it changes.
	// ⛔ This is what the deposit / amenity / free-promotion consumers actually want, and it is NOT a duplicate of
	// the PRESENCE pair: presence cannot tell dormant from operating, which is exactly why a consumer given only
	// presence had to go and CALCULATE the difference. A dormant building confers nothing and deposits nothing.
	// iType = Building, iC = owner, iSrcLoc = cityId. DOMAIN.
	SEVT_CITY_BUILDING_ACTIVATED    = 62,
	SEVT_CITY_BUILDING_DORMANTED    = 63,
	// A present building crossed into / out of OBSOLESCENCE in this city -- the enabler's own obsolete-set verdict.
	// ⛔ OBSERVABILITY ONLY -- logging and the player NOTIFICATION (owner). It drives NOTHING: a tech is the only
	// thing that can obsolete, so the FATE (an empty `whenObsolete` removes the instance, a tree-carrying one leaves
	// it standing for that tree to take over) is applied on the TECH fact where the check already runs. Routing the
	// apply through this fact would make a UI concern a condition of the state change.
	// iType = Building, iC = owner, iSrcLoc = cityId. DOMAIN.
	SEVT_CITY_BUILDING_OBSOLETED_ADDED   = 64,
	SEVT_CITY_BUILDING_OBSOLETED_REMOVED = 65,
	// The city GAINED / LOST a religion (CvCity::setHasReligion). iType = Religion, iC = owner, iSrcLoc = cityId.
	SEVT_CITY_RELIGION_ADDED        = 66,
	SEVT_CITY_RELIGION_REMOVED      = 67,
	// The city GAINED / LOST a corporation (CvCity::setHasCorporation). iType = Corp, iC = owner, iSrcLoc = cityId.
	SEVT_CITY_CORPORATION_ADDED     = 68,
	SEVT_CITY_CORPORATION_REMOVED   = 69,
	// The city OBTAINED / LOST a bonus -- the NETWORK (trade) supply presence crossing, from CvCity::processBonus.
	// ⚖ THE PRESENCE CROSSING ONLY (0 <-> non-zero), by owner ruling -- NOT every count move. processNumBonusChange
	// calls processBonus solely when the has-verdict crosses zero, so a count going 2 -> 3 announces nothing,
	// deliberately: a per-count fact would drag in attribution edge cases (WHICH city or source added this copy)
	// that the crossing sidesteps entirely. ⛔ Do NOT "fix" the missing count fact -- it is a scope boundary that
	// somebody DECIDED, not a gap. A count-threshold reader (a `min: 3` atom) is knowingly outside it for now, and
	// it reopens only when a resource becomes a QUANTITY a city draws against (the volumetric trigger).
	// ⚠ DISTINCT from the VICINITY pair below: this is what the PLOT GROUP supplies, so a network membership move
	// fires these and never those (a locally-provided bonus survives a group change).
	// iType = Bonus, iC = owner, iSrcLoc = cityId. DOMAIN.
	SEVT_CITY_BONUS_ADDED           = 70,
	SEVT_CITY_BONUS_REMOVED         = 71,
	// The city's LOCAL (vicinity) supply of a bonus moved -- an active providing building appeared / vanished (the
	// operate/provides fixpoint, and CvCity::processBuilding). Vicinity supply never adds an owned COUNT (that lives
	// on the plot group); the consumer re-gates the bonus's connection:vicinity dependents.
	// ⚑ A city can hold SEVERAL of a bonus locally, so these carry a MAGNITUDE -- iA = HOW MANY, unsigned. That is
	// the payload doing its job; the DIRECTION is still the id, never a sign. Splitting loses nothing: an ADDED of 2
	// and a REMOVED of 1 say exactly what a signed delta said, without handing the consumer a branch.
	// ⚠ The BUILDING half is the only half these facts carry. The MAP half -- a bonus appearing/vanishing on a
	// radius tile, or that tile changing hands -- is announced by the PLOT facts and by the radius growth on the
	// culture-level fact; a consumer holding a city-scope vicinity store folds both halves from those. There is
	// deliberately no per-turn sweep behind this fact -- a missed emit must stay visibly wrong (docs/cascade.md §A SELF-HEAL IS THE FOSSIL OF A MISSING EMIT).
	// iType = Bonus, iC = owner, iSrcLoc = cityId. DOMAIN.
	SEVT_CITY_VICINITY_BONUS_ADDED   = 72,
	SEVT_CITY_VICINITY_BONUS_REMOVED = 73,
	// The city GREW / SHRANK (CvCity::setPopulation). iA = HOW MANY population moved, as an unsigned MAGNITUDE --
	// never the new total: a consumer maintaining a sum needs how much MOVED, and a total would force it to subtract
	// against a remembered previous value, which is the derivation this split exists to remove. The one consumer
	// that wants the total reads the city, which owns it. ⚑ The SAVE READ emits ADDED with the stored amount -- the
	// ordinary fact with its magnitude, not a bespoke load verb -- so a loaded city's `per:{POPULATION}` deposits
	// build from zero by applying, like every other (docs/spine.md §5 (the load reseed)).
	// iC = owner, iSrcLoc = cityId. DOMAIN.
	SEVT_CITY_POPULATION_ADDED      = 74,
	SEVT_CITY_POPULATION_REMOVED    = 75,
	// Specialists were ASSIGNED to / REMOVED from the city (CvCity::setSpecialistCount). iA = HOW MANY, unsigned --
	// an ADDED of 3 adds three times over. iType = Specialist, iC = owner, iSrcLoc = cityId. DOMAIN.
	SEVT_CITY_SPECIALIST_ADDED      = 76,
	SEVT_CITY_SPECIALIST_REMOVED    = 77,
	// The `providesPower` amenity CROSSING -- the city started / stopped being powered. iSrcLoc = cityId. DOMAIN.
	SEVT_CITY_POWER_ADDED           = 78,
	SEVT_CITY_POWER_REMOVED         = 79,
	// A CITY STATUS was APPLIED / EXPIRED (CvCity::setStatus, at the 0-crossing). A status is an applied counter
	// that ticks down and is over at zero (Engine/CvStatus.h), so only the CROSSING is a fact -- emitting per
	// decrement would fire every turn for no state change, the general rule for every timer-backed fact.
	// ⚑ The fact is GENERIC OVER THE STATUS, and deliberately: the CityStatus enum is hand-maintained and GROWS,
	// so a fact per status would mean an engine change per status -- exactly what an open registry exists to
	// avoid. iType names WHICH status, the same standing a religion or property id has; it is not a direction
	// discriminator, which is what docs/spine.md §A FACT NAMES THE HAPPENING actually bans. The direction is the event.
	// ⚖ `CITYSTATUS_POWER_DISABLED` is MIDDLEWARE, not a leg: it gates what the power amenity DELIVERS while the
	// store keeps its grantors (state.md § A STATUS IS MIDDLEWARE). So this fact reaches the AMENITY FOLD and
	// stops -- the fold re-checks CvCity::isPowered and announces SEVT_CITY_POWER_* if that verdict crossed.
	// ⛔ It is never a cascade input, and nothing downstream routes on it: the verdict is what consumers ride.
	// iType = the CityStatus, iB = turns remaining, iC = owner, iSrcLoc = cityId. DOMAIN.
	SEVT_CITY_STATUS_ADDED           = 80,
	SEVT_CITY_STATUS_REMOVED         = 81,
	// The UNIT-scope twin, same model one scope down (Engine/CvStatus.h -- a status is a SCOPE concept, so each
	// scope carries its own enum and the identical store). iType = the UnitStatus, iB = turns remaining,
	// iC = owner, iSrcLoc = plotNum. DOMAIN.
	SEVT_UNIT_STATUS_ADDED           = 192,
	SEVT_UNIT_STATUS_REMOVED         = 193,
	// The player BECAME / STOPPED BEING a rebel (CvPlayer::setIsRebel, the one write path). IS_REBEL gates the
	// authored rebel maintenance discount on TECH_GAME_START, which every player holds forever -- so without this
	// fact that deposit resolves ONCE, at game start when nobody is a rebel, and the discount can never apply.
	// iC = player. DOMAIN.
	SEVT_EMPIRE_REBEL_ADDED          = 194,
	SEVT_EMPIRE_REBEL_REMOVED        = 195,
	// A city's PROPERTY value crossed one of the band BOUNDARIES the authored `requires.operate` clauses declare
	// -- the value ROSE across one (_ADDED) or FELL across one (_REMOVED). iType = the PROPERTY_, iC = owner,
	// iSrcLoc = cityId. DOMAIN.
	// ⚖ THE HOLDER ANNOUNCES THE THRESHOLD CROSSING, exactly as the amenity fold announces power's 0 <-> non-zero
	// verdict and stays silent from 1 -> 2 (owner). SEVT_PROPERTY_ADDED / _REMOVED beside it is the VALUE moving,
	// which happens to nearly every property of every city every turn as the solver runs; this is the far rarer
	// happening a band consumer actually cares about, so that consumer routes on it with ZERO arithmetic.
	// ⛔ The crossing is NOT re-derived per consumer. The boundaries live in ONE registry
	// (EnablerKernel::propertyBandThresholds) and ONE place tests them, or every future consumer -- a trigger, a
	// player alert -- re-implements the same sweep (docs/architecture/patterns.md §DRY (single implementation)).
	SEVT_CITY_PROPERTY_BAND_ADDED    = 196,
	SEVT_CITY_PROPERTY_BAND_REMOVED  = 197,
	// The EMPIRE-LEVEL BUILDING facts (docs/specs/enabler.md §2 (empire-level buildings), enabler-spec §2): the PLAYER gained / lost an
	// identity.empireLevel building -- held once, never present in any city. The held store is the player's own
	// building count at 0/1, announced here at its crossing; iType = the building, iA = bFirst (a load RESTORES,
	// so the reseed emits 0), iC = owner. DOMAIN.
	SEVT_EMPIRE_BUILDING_ADDED       = 198,
	SEVT_EMPIRE_BUILDING_REMOVED     = 199,
	// The player-side OPERATE verdict crossing for an empire-level building -- the empire twin of
	// SEVT_CITY_BUILDING_ACTIVATED / _DORMANTED, and the same split: the building's OWN deposits ride THIS
	// crossing (a held-but-dormant member deposits nothing -- the civic-gated markers genuinely toggle), while
	// plane-C atom re-resolution rides the presence pair above. iType = the building, iC = owner. DOMAIN.
	SEVT_EMPIRE_BUILDING_ACTIVATED   = 200,
	SEVT_EMPIRE_BUILDING_DORMANTED   = 201,
	// ⛔ THE CITY GAINED / LOST AN AMENITY -- the generic 0 <-> non-zero crossing of the city's amenity fold, for
	// EVERY key. iType = the AMENITY_* classification id, iC = owner, iSrcLoc = cityId. DOMAIN.
	// ⚑ The id is NOT the discriminator docs/spine.md §A FACT NAMES THE HAPPENING bans -- it names WHICH member of an OPEN
	// registry moved, exactly as a religion, property or status id does, and the DIRECTION is in the event name.
	// A fact per amenity would mean an engine change per authored key, which is precisely what the open
	// classification registry exists to avoid (the SEVT_CITY_STATUS pair settles the identical question).
	// ⚠ POWER keeps its OWN pair below and that is NOT a duplicate of this: this announces the STORE's refcount
	// crossing, while power announces the GATED verdict (CvCity::isPowered -- the refcount AND no blackout). The
	// two genuinely differ, and both are real happenings ([state.md] § A STATUS IS MIDDLEWARE): a plant finished
	// during a blackout moves the store and delivers nothing; a blackout lifting delivers power with the store
	// untouched. Government centre and fresh water have no such gate, so THIS fact is their crossing and they
	// carry no pair of their own.
	SEVT_CITY_AMENITY_ADDED         = 206,
	SEVT_CITY_AMENITY_REMOVED       = 207,
	// The city GAINED / LOST a religion's HOLY-CITY designation (CvGame::setHolyCity) -- the IS_HOLY_CITY /
	// IS_STATE_RELIGION_HOLY_CITY predicates flip for the OLD city (loses) and the NEW city (gains); gates
	// conditioned commerce/yields. Emitted per affected city. iType = Religion, iC = owner, iSrcLoc = cityId. DOMAIN.
	SEVT_CITY_HOLY_CITY_ADDED       = 86,
	SEVT_CITY_HOLY_CITY_REMOVED     = 87,
	// The city GAINED / LOST a corporation's HEADQUARTERS designation (CvGame::setHeadquarters), the holy-city
	// shape. ⛔ NOT a duplicate of the building-presence / corporation-presence facts the same setter drives: an HQ
	// designation is neither. iType = Corporation, iC = owner, iSrcLoc = cityId. DOMAIN.
	SEVT_CITY_HEADQUARTERS_ADDED    = 88,
	SEVT_CITY_HEADQUARTERS_REMOVED  = 89,
	// The city's CULTURE LEVEL moved (CvCity::setCultureLevel). TWO things ride this fact: the culture-level cascade
	// input (wonder caps, defense) AND the city's workable RADIUS growth -- so it is ALSO the vicinity MEMBERSHIP
	// signal. A SLOT REPLACEMENT: the outgoing level is REMOVED, the incoming ADDED.
	// iA = the level, iC = owner, iSrcLoc = cityId. DOMAIN.
	SEVT_CITY_CULTURE_LEVEL_ADDED   = 90,
	SEVT_CITY_CULTURE_LEVEL_REMOVED = 91,
	// The city's OWNER slot moved (conquest / culture flip / gift / dispose): the entity's packages move scope and
	// BOTH owners' empire aggregates change. A SLOT REPLACEMENT -- REMOVED names the losing owner, ADDED the
	// gaining one. iC = the owner this fact is about, iSrcLoc = cityId. DOMAIN.
	SEVT_CITY_OWNER_ADDED           = 92,
	SEVT_CITY_OWNER_REMOVED         = 93,
	// The city's own centre plot JOINED / LEFT a plot-group (merge/split -- CvPlot::setPlotGroup, owner-gated). The
	// MEMBERSHIP twin of the PLOTGROUP_BONUS pair (which is the resource-set twin).
	// ⛔ The RESOURCE consequences are NOT these facts' to carry and must not be re-derived from them: the same
	// choke point calls CvCity::onNetworkSupplyChanged FIRST, which walks the two groups' holdings and fires a
	// per-bonus CITY_BONUS_ADDED / _REMOVED for every genuine presence crossing (in the deferred path too --
	// endDeferredBonusProcessing replays them against the entry snapshot). Those specific facts already say what
	// moved; a consumer re-gating everything on these is doing their work again.
	// iA = the plotGroupId, iC = owner, iSrcLoc = cityId. DOMAIN.
	SEVT_CITY_NETWORK_ADDED         = 94,
	SEVT_CITY_NETWORK_REMOVED       = 95,
	// The city's production QUEUE gained / lost an order (CvCity::pushOrder / popOrder). The enabler's queue leg
	// (enabler.md §7.1 step 3): a QUEUED building leaves the fresh offer, and a dequeue restores it -- the event
	// triggers the one-id re-gate; the gate itself reads the live queue (object-owned state).
	// iType = the ordered item id, iA = OrderTypes, iC = owner, iSrcLoc = cityId. DOMAIN.
	SEVT_CITY_ORDER_ADDED           = 96,
	SEVT_CITY_ORDER_REMOVED         = 97,
	// A city was FOUNDED (CvPlayer::found, once the city object exists and before the settle-time provisions).
	// ⛔ Distinct from the CITY_OWNER pair, which fires on ACQUIRE (conquest/trade) and on the load restore --
	// founding produced NO identifiable fact before this, only a constellation of side-effects, which is why the
	// settle-time provisions had no trigger to hang on. That constellation is exactly what a `*_CHANGED` surface
	// looks like from a consumer's side, and why this fact exists.
	// iType = the FOUNDING unit's type (-1 if none), iA = that unit's id, iC = owner, iSrcLoc = the new city.
	SEVT_CITY_FOUNDED               = 98,
	// The city's CORPORATION-ACTIVE verdict crossed ({HAS_CORPORATION: X} = ACTIVE, json §3.5). The verdict is a
	// four-leg engine composition (CvCity::isActiveCorporation -- presence, the player-level state, the obsoleting
	// tech, a consumed bonus held), so no single leg's fact is it: the stored verdict's holder (CityContext)
	// re-reads the ONE engine implementation on each leg's fact and announces here only when the verdict moved --
	// the threshold-crossing shape (event-spine.md: the holder of the value announces the crossing).
	// ⛔ NOT a duplicate of the CORPORATION presence pair above: a present-but-dormant corporation is the case
	// that separates them, and it is what plane C routes on ({HAS_CORPORATION}-gated deposits).
	// iType = Corporation, iC = owner, iSrcLoc = cityId. DOMAIN.
	SEVT_CITY_CORPORATION_ACTIVE_ADDED   = 99,
	SEVT_CITY_CORPORATION_ACTIVE_REMOVED = 100,

	// ===== PLOT =====
	// ⛔ THE SUBSTRATE SLOTS. Each (terrain / feature / improvement / route / bonus / type / landmark) holds ONE
	// source at a time, so a replacement is TWO happenings and announces as two facts: the old source LEAVING, then
	// the new one ARRIVING. THE ORDER IS THE MECHANISM: the REMOVED fact is emitted while the OLD STATE STILL HOLDS,
	// so a consumer withdrawing that source's deposits resolves them against exactly the state they were computed
	// against ([state-repositories.md] § THE INVARIANT). Carrying the old id on one CHANGED fact was the earlier
	// shape and it is what left the gap -- a single "the slot moved" fact makes every consumer DERIVE the removal,
	// and that derivation is impossible once the state has moved.
	// ⚑ The PLOT announces its own bits and sends them UP the chain; a city never reaches down to re-read them
	// ([contexts.md]) -- a city-side maintainer that "unfolds the old bits and refolds the new" cannot work,
	// because by the time any consumer runs the plot already holds the NEW value.
	// iType = the source, iC = owner, iSrcLoc = plotId. DOMAIN.
	SEVT_PLOT_TERRAIN_ADDED         = 110,
	SEVT_PLOT_TERRAIN_REMOVED       = 111,
	SEVT_PLOT_FEATURE_ADDED         = 112,
	SEVT_PLOT_FEATURE_REMOVED       = 113,
	SEVT_PLOT_IMPROVEMENT_ADDED     = 114,
	SEVT_PLOT_IMPROVEMENT_REMOVED   = 115,
	SEVT_PLOT_ROUTE_ADDED           = 116,
	SEVT_PLOT_ROUTE_REMOVED         = 117,
	// The plot's RESOURCE was placed / discovered / removed. All play-time paths route through
	// CvPlot::setBonusType (a Great-Farmer build, a discovery event, removal); the reseed fires the SAME facts.
	SEVT_PLOT_BONUS_ADDED           = 118,
	SEVT_PLOT_BONUS_REMOVED         = 119,
	// The plot's TYPE (CvPlot::setPlotType): flat / hills / peak / OCEAN. Load-bearing well beyond relief, because
	// CvPlot::isWater() IS getPlotType() == PLOT_OCEAN -- the whole water/land axis (and every neighbour's coast
	// verdict) hangs off this fact, not off terrain.
	SEVT_PLOT_TYPE_ADDED            = 120,
	SEVT_PLOT_TYPE_REMOVED          = 121,
	// The plot's LANDMARK designation (CvPlot::setLandmarkType) -- the named natural feature (peak range, bay,
	// lake, ...) the map generator and the landmark events assign.
	SEVT_PLOT_LANDMARK_ADDED        = 122,
	SEVT_PLOT_LANDMARK_REMOVED      = 123,
	// The plot GAINED / LOST a river (CvPlot::changeRiverCrossingCount crossing zero) -- the count is a running
	// tally of river-carrying edges, and only the PRESENCE transition is a fact worth announcing.
	// iB = the new crossing count, iC = owner, iSrcLoc = plotId. DOMAIN.
	SEVT_PLOT_RIVER_ADDED           = 124,
	SEVT_PLOT_RIVER_REMOVED         = 125,
	// The plot GAINED / LOST irrigation (CvPlot::setIrrigated) -- the spread of irrigation water, distinct from the
	// improvement that carries it. iC = owner, iSrcLoc = plotId. DOMAIN.
	SEVT_PLOT_IRRIGATION_ADDED      = 126,
	SEVT_PLOT_IRRIGATION_REMOVED    = 127,
	// The plot's OWNER slot moved (CvPlot::setOwner). ⚑ OWNERSHIP IS A MEMBERSHIP FACT: a plot gaining or losing a
	// city's ownership adds or removes that plot's CASC_PRED_* bits from that city's dictionary through the ONE
	// applier, exactly as entering or leaving the worked radius does (docs/cascade.md §Maintained EVENT-DRIVEN (a context is never marked)).
	// iC = the owner this fact is about, iSrcLoc = plotId. DOMAIN.
	SEVT_PLOT_OWNER_ADDED           = 128,
	SEVT_PLOT_OWNER_REMOVED         = 129,
	// The plot ENTERED / LEFT a city's workable RADIUS (CvPlot::updateWorkingCity) -- the MEMBERSHIP fact: which city
	// may work the plot. The plot's yield moves from the old city to the new, so both cities' packages change.
	// DISTINCT from the WORKED pair below -- membership is the superset, working is the citizen actually assigned.
	// iA = the cityId, iC = owner, iSrcLoc = plotId. DOMAIN.
	SEVT_PLOT_WORKING_CITY_ADDED    = 130,
	SEVT_PLOT_WORKING_CITY_REMOVED  = 131,
	// A citizen STARTED / STOPPED working the plot (CvCity::setWorkingPlot). CITY-driven, so it carries the city as
	// well as the plot: the fact belongs to the plot (its IS_WORKED verdict flips) but only the city can attribute
	// it. iB = the working cityId, iC = owner, iSrcLoc = plotId. DOMAIN.
	SEVT_PLOT_WORKED_ADDED          = 132,
	SEVT_PLOT_WORKED_REMOVED        = 133,
	// A city SAT DOWN on / was REMOVED from the plot (CvPlot::setPlotCity). DISTINCT from the WORKING_CITY pair
	// (which city may WORK the plot) and from CITY_FOUNDED (the founding act, which does not fire on razing).
	// ⚠ CvPlot::changeCityRadiusCount / changePlayerCityRadiusCount are PASS-THROUGHS of this setter -- these facts
	// cover them; a second emit there would announce the same change per radius plot.
	// iA = the cityId, iC = owner, iSrcLoc = plotId. DOMAIN.
	SEVT_PLOT_CITY_ADDED            = 134,
	SEVT_PLOT_CITY_REMOVED          = 135,
	// ⚖ THE PLOT'S OWN DERIVED VERDICT CROSSED -- one CASC_PRED_* bit of its stored block moved. Announced by
	// PlotContext, which OWNS the verdict, at the crossing and nowhere else ([contexts.md]: the crossing is
	// emitted by the FOLD, because the fold IS the maintenance path).
	// ⚑ IT IS WHAT LETS A MEMBER PLOT'S BIT REACH THE CITY as `plotAttrs.add(bit, ±1)`. Without it a city could
	// only learn that "something about this plot moved" and would have to unfold and refold the plot's whole
	// block -- which cannot work, because by the time any consumer runs the plot already holds the NEW value.
	// The PLOT sends its bit UP; the city never reaches down for it.
	// ⛔ It is NOT a substrate fact and never replaces one: the substrate facts say what the TILE now carries,
	// this says what that MEANS for the one predicate that moved. A consumer routing on a substrate id is asking
	// about the source; a consumer routing on this is asking about the verdict.
	// iType = the CASC_PRED_* id, iC = owner, iSrcLoc = plotId. DOMAIN.
	SEVT_PLOT_PREDICATE_ADDED       = 136,
	SEVT_PLOT_PREDICATE_REMOVED     = 137,
	// ⚖ THE PLOT ENTERED / LEFT A CITY'S POTENTIAL WORK AREA -- the membership the CITY defines and the PLOT
	// holds (`CvPlot::setWorkableBy`). ⛔ DISTINCT from the WORKING_CITY pair: that names the ONE city actually
	// assigned the tile, this names every city that MAY work it, which is the set a radius-keyed store folds on.
	// ⚑ It is what makes a GROWING radius an ordinary fact: the city-plot addressing is a fixed ring-ordered
	// table, so a culture level-up admits exactly the indices between the old and new counts, and each announces.
	// iA = the cityId, iC = owner, iSrcLoc = plotId. DOMAIN.
	SEVT_PLOT_WORKABLE_BY_ADDED     = 138,
	SEVT_PLOT_WORKABLE_BY_REMOVED   = 139,

	// ===== PLOT GROUP / AREA =====
	// A plot-group (the connectivity / trade-NETWORK identity) GAINED / LOST access to a resource --
	// CvPlotGroup::changeNumBonuses on a presence transition. A traded resource enters at the capital's group and
	// reaches every connected city; the consumer re-evals connection:trade deposits for the group's member cities.
	// iType = Bonus, iA = HOW MANY, iC = owner, iSrcLoc = plotGroupId. DOMAIN.
	SEVT_PLOTGROUP_BONUS_ADDED      = 140,
	SEVT_PLOTGROUP_BONUS_REMOVED    = 141,
	// An AREA GAINED / LOST tiles (CvArea::changeNumTiles) -- feeds CityContext's AREA_SIZE and its
	// max-adjacent-water store. An area has no owning player, so iC stays -1.
	// iA = HOW MANY, unsigned. iSrcLoc = areaId. DOMAIN.
	SEVT_AREA_TILE_ADDED            = 142,
	SEVT_AREA_TILE_REMOVED          = 143,

	// ⚖ THE PLOT SERVES A RESOURCE ON SITE -- the plot's own derived verdict that this tile makes bonus B AVAILABLE
	// to a city that can work it: the tile CARRIES B *and* its improvement TRADES it
	// (CvImprovementInfo::isImprovementBonusTrade). ⛔ NOT a duplicate of the PLOT_BONUS pair and it never replaces
	// one -- that says what the tile CARRIES, this says what that MEANS, the same substrate-vs-verdict split
	// SEVT_PLOT_PREDICATE_* draws. A consumer cannot derive one from the other, because the verdict reads TWO axes
	// (bonus AND improvement) and either can move without the other.
	// ⚑ It carries an ID rather than riding the CASC_PRED_* bitset because the answer is a BONUS, not a boolean: a
	// plot holds at most one bonus, so "which resource does this tile serve" is a single id whose crossing is the
	// ordinary ADDED / REMOVED pair. ⚠ The id sits here rather than in the plot block above only because that
	// block's numbers are used up; it is a PLOT fact in every other respect.
	// iType = Bonus, iC = owner, iSrcLoc = plotId. DOMAIN.
	SEVT_PLOT_SERVED_BONUS_ADDED    = 146,
	SEVT_PLOT_SERVED_BONUS_REMOVED  = 147,

	// ===== UNIT =====
	// A unit INSTANCE was created (CvUnit::init) / died (CvUnit::die). ⛔ KILLED's correctness is STRUCTURAL, not
	// positional: it is emitted on the FIRST line of die(), the one function that ends a unit's life, which carries
	// no early return and no conditional deletion and always ends in deleteUnit. The outcomes that leave a unit
	// ALIVE (evacuate-to-capital, last-stand survival) are decided BEFORE die() is entered and never reach it.
	// An OFF-MAP death is a real outcome of that function, not a skipped one: iSrcLoc is -1 and the unit is deleted
	// exactly as an on-map one is. iType = unit TYPE, iA = unit id, iC = owner. DOMAIN.
	SEVT_UNIT_CREATED               = 150,
	SEVT_UNIT_KILLED                = 151,
	// A unit's DEATH SCHEDULE was set / cleared (CvUnit::m_bDeathDelay) -- the save-carried state a delayed kill
	// leaves behind so the object outlives combat resolution, read across the engine through isDelayedDeath().
	// ⛔ NOT a duplicate of KILLED: a scheduled death is an INTENTION whose outcome can still flip to survival, so
	// a consumer treating it as a death would bury units that walk away. BOTH transitions announce -- a one-way
	// fact would leave a survivor permanently marked dying -- and CvUnit::read carries the in-read half for a save
	// taken mid-schedule. iType = unit TYPE, iA = unit id, iC = owner, iSrcLoc = the plot. DOMAIN.
	SEVT_UNIT_DEATH_SCHEDULE_ADDED   = 152,
	SEVT_UNIT_DEATH_SCHEDULE_REMOVED = 153,
	// A unit ENTERED / LEFT a friendly city's plot (CvUnit::setXY's city branches). The targeted trigger for
	// anything a city hands to units PRESENT in it -- above all building free promotions, which are otherwise a
	// per-turn rescan of (promo buildings x every unit on the plot). NOT emitted on every move: only on a city
	// entry/leave, so the stream stays proportional to a rare fact rather than to unit traffic.
	// ⚠ These must never grow into cache invalidation -- unit movement invalidating cache is a standing owner
	// "full stop" (docs/cascade.md §2b (unit-carried modifiers apply on top, live)). A GRANT is one-shot state, not a recompute, which is why it rides.
	// ⚠ The LEAVE is announced for EVERY city plot a unit vacates while the ENTRY's conquest branch resolves into an
	// acquisition instead of an entry, so the two do NOT net to occupancy -- a consumer needing occupancy reads the
	// unit's live plot. iType = unit TYPE, iA = unit id, iC = owner, iSrcLoc = city id. DOMAIN.
	SEVT_UNIT_ENTERED_CITY          = 154,
	SEVT_UNIT_LEFT_CITY             = 155,
	// A unit GAINED / LOST a promotion (CvUnit::processPromotion -- the ONE funnel both setHasPromotion overloads
	// reach) and a COMBAT CLASS (CvUnit::setHasUnitCombatInternal -- the commit + hash + fact, which the public
	// setter reaches past its change guard and its game-option/spy validity gate, and which the LOAD calls on its
	// own; processUnitCombat beside it applies only the STATS, which a load must not re-apply).
	// state-repositories.md: a unit's resolved values move ONLY when a promotion
	// or a combat class changes, so these two are that plane's whole maintenance surface.
	// iType = Promotion | UnitCombat, iA = unit id, iC = owner. DOMAIN.
	SEVT_UNIT_PROMOTION_ADDED       = 156,
	SEVT_UNIT_PROMOTION_REMOVED     = 157,
	SEVT_UNIT_COMBAT_ADDED          = 158,
	SEVT_UNIT_COMBAT_REMOVED        = 159,
	// A COMBAT RESOLVED, with a winner and a loser (CvEventReporter::combatResult). Emitted BESIDE the Python
	// callback the reporter already makes, never instead of it -- the reporter is untouched behaviourally.
	// ⛔ NOT a duplicate of SEVT_UNIT_KILLED: a combat resolves whether or not anybody dies (a withdrawal, a
	// retreat, a non-fatal exchange all resolve), and KILLED names one unit's death while this names the
	// EXCHANGE and its two parties. Both fire on a fatal combat and neither substitutes for the other.
	// ⚑ It exists because the happening reached PYTHON ONLY, so nothing on the C++ side could see a combat at
	// all -- which is also why a captured unit was untraceable: SEVT_UNIT_CREATED said a unit appeared and no
	// fact said where it came from.
	// ⚠ The payload is RAW -- the reporter's own arguments, nothing modelled. Defining what a combat event MEANS
	// waits for the event system to move over and be formalized in JSON; anything designed here is undone then.
	// iType = winner unit TYPE, iA = winner unit id, iB = loser unit id, iC = winner owner, iSrcLoc = the plot.
	// The loser's TYPE and OWNER do not fit the five DOMAIN ints and ride as render fields. DOMAIN.
	SEVT_COMBAT_RESULT              = 160,

	// ===== PROPERTY (any owner scope) =====
	// A game object's PROPERTY VALUE moved (CvProperties -- the generic (PropertyTypes,int) bag on
	// game/team/player/city/unit/plot). DOMAIN: a property value is synced, deterministic, save-carried state that
	// folds into the OOS checksum, and PROPERTY_* is one cascade channel per property info, read by
	// CityContext::propertyValue, every requires.operate property BAND and every threshold-conditioned deposit.
	// Emitted at the three CvProperties mutation choke points, which every owner scope funnels through.
	// ⚠ The solver's change PROPAGATION fans one change onto OTHER objects, each of which re-enters the mutation
	// path -- distinct objects' facts, so each emits.
	// iType = PropertyTypes, iA = HOW MANY the value moved (unsigned), iB = the object KIND (GameObjectTypes -- a
	// city id and a plot id are otherwise the same int), iC = owner (-1 where the object has none), iSrcLoc = the
	// object's own id. DOMAIN.
	SEVT_PROPERTY_ADDED             = 170,
	SEVT_PROPERTY_REMOVED           = 171,

	// ===== NAMED HAPPENINGS the trigger plane dispatches on =====
	// These already name what happened and take no ADDED/REMOVED split: each is a single, directionless occurrence.
	SEVT_TECH_ACQUIRED              = 180,  // iType = Tech, iA = 1 (first-discoverer), iC = discovering player
	SEVT_RELIGION_FOUNDED           = 181,  // iType = Religion, iC = founding player
	SEVT_CIVIC_ADOPTED              = 182,  // iType = Civic, iC = adopting player (the revolution pulse)
	SEVT_PLAYER_INIT                = 183,  // iType = iC = player -- game start
	SEVT_NAME_CHANGE                = 184,  // iType = NameChangeKind, iA = owner, iB = entity id

	// ⛔ ONE TRADED ITEM TAKING EFFECT BETWEEN TWO EMPIRES -- iType = TradeableItems, iA = the item's data (a
	// BONUS for TRADE_RESOURCES, a tech/city/unit id otherwise), iB = from-player, iC = to-player. The deal id
	// rides as a render field only: a deal is an id, two players and two lists, so the fact is about the
	// MOVEMENT, not its container ([bonuses.md](../../docs/reference/bonuses.md)). DOMAIN.
	// ⚑ It NEEDS NO DISCRIMINATOR, which is why it is per ITEM: every deal kind -- trade, demand, tribute,
	// peace, vassal -- decomposes into the same primitive, so nothing branches on what wrapped it. A demand is
	// simply items that all flow one way. And multiplicity is FREE: TradeData carries no quantity, so three
	// iron is three nodes and three emits.
	// ⚠ ADDED fires for every item; REMOVED only for the ones with a DURATION (the set startTrade stores). A
	// one-shot -- a tech handed over, gold paid -- correctly gets an ADDED and no REMOVED.
	SEVT_EMPIRE_TRADE_ADDED         = 202,
	SEVT_EMPIRE_TRADE_REMOVED       = 203,
	// ⛔ THE AGREEMENT ITSELF -- iType = the deal id, iB = first player, iC = second player. Distinct from the
	// per-ITEM pair above and NOT a duplicate of it: an item fact says a resource moved, this says two empires
	// are (or stop being) bound. They fire together and neither substitutes for the other -- several distinct
	// happenings reaching one choke point are several facts (docs/spine.md §A FACT NAMES THE HAPPENING). DOMAIN.
	// ⚑ It exists because the DEAL carries what no item does: WHO it favoured, which is what the diplomacy side
	// rides. A demand is a deal with one empty side, so the fact shape covers it with no discriminator.
	SEVT_EMPIRE_DEAL_ADDED          = 204,
	SEVT_EMPIRE_DEAL_REMOVED        = 205,

	// ===== DIAGNOSTIC -- code RAN, never what the state IS =====
	// ⛔ NO CONSUMER MAY BUILD STATE FROM THESE. "I have completed my job" -- the test that decides the kind is
	// whether the fact says WHAT THE STATE IS or WHAT SOME CODE DID (event-spine.md § THE RECEIVED LINE). Deriving
	// held state from an announcement that an apply ran is the failure this kind exists to make unsayable; the
	// STATE these sit beside is the ACTIVATED / DORMANTED crossing above.
	SEVT_CITY_BUILDING_PROCESSED    = 190,
	SEVT_LOAD_PIPELINE              = 191,

	// ===== SAVELOAD -- what the STREAM CONTAINED, never what set the state =====
	// ⛔ NO CONSUMER MAY BUILD STATE FROM THIS. It is testimony ABOUT the read, sitting beside the DOMAIN facts
	// the internal setters emit ([event-spine.md]: the save stream is authoritative for base state, the fact
	// stream for derived -- and neither is authoritative for the other's half).
	// ⚑ It answers the one question a DOMAIN fact structurally cannot: whether a serialized BLOCK was EMPTY. A
	// fact that never fires is indistinguishable from a fact that is not wired, so the block's own count is the
	// only thing that separates "the save held none" from "the emit is dead".
	SEVT_SAVELOAD_BLOCK             = 208,

	// The load-END census of a serialized block, emitted from onFinalInitialized where the BUG log level is
	// unambiguously up. DIAGNOSTIC, because it states that the load reached this point carrying N -- it is not
	// testimony about the stream (that is SEVT_SAVELOAD_BLOCK above) and nothing may fold on it.
	// ⚑ It exists because CvGame::read's OWN body runs before the log level is pushed, so a line emitted there is
	// invisible while every later one logs -- and a fact that cannot be seen is indistinguishable from a fact that
	// never fired. Reporting the same count from a live point is what makes the in-read half verifiable at all.
	SEVT_SAVELOAD_CENSUS            = 209,

	// WHY a deal was cancelled by CvGame::verifyDeals. DIAGNOSTIC: it says which leg of verify()'s OR fired, not
	// what the state is -- the REMOVAL itself is SEVT_EMPIRE_DEAL_REMOVED, and nothing may fold on this.
	// ⚑ Four independent conditions (untradeable resource / no trade network / either team obsoleting the bonus)
	// converge on one kill(), so the deal-removed fact alone can never attribute a mass cancellation.
	SEVT_DEAL_VERIFY_CANCEL         = 210,

	// The AI's per-turn verdict on an EXISTING deal it is free to cancel: does it CONTINUE, and if not, why.
	// DIAGNOSTIC -- a decision was re-evaluated; the cancellation it may cause is SEVT_EMPIRE_DEAL_REMOVED.
	// ⚑ It is emitted on the KEEP as well as the drop, deliberately: "why did it continue" is the owner's
	// question as much as "why did it stop", and a fact that fires only on cancellation cannot answer the first.
	// ⚠ This is the AI's own judgement (a refusal, or the offer no longer valuing out) and is a DIFFERENT question
	// from SEVT_DEAL_VERIFY_CANCEL, which is the mechanical resource check. Both can kill the same deal.
	SEVT_DEAL_AI_EVALUATED          = 211
};

//	WHICH typed setter produced a SEVT_GAME_GLOBAL_DEFINE_ADDED / _REMOVED fact (its iB) -- the value's kind decides which render
//	field carries it, since only the INT form fits the DOMAIN ints.
enum GlobalDefineKind
{
	GLOBALDEFINE_INT = 0,
	GLOBALDEFINE_FLOAT = 1,
	GLOBALDEFINE_STRING = 2
};

//	WHICH option id space a SEVT_GAME_OPTION_ADDED / _REMOVED fact speaks (its iB). The two are separate registries with
//	overlapping int ranges: GAMEOPTION_* is what an entity gate reads, MODDERGAMEOPTION_* is engine-side tuning.
enum GameOptionSpace
{
	GAMEOPTSPACE_GAME = 0,     // GameOptionTypes   -- GAMEOPTION_*
	GAMEOPTSPACE_MODDER = 1    // ModderGameOptionTypes -- MODDERGAMEOPTION_*
};

//	Which entity's display name changed (the iType of a SEVT_NAME_CHANGE event). The logging consumer resolves the
//	NEW name LIVE (synchronous game-thread render -> exact), so the payload stays string-free; an out-of-process consumer
//	(StoneBase / an agent) rebuilds its id->name table from the emitted lines -- REQUIRED for the total-observability
//	("Orwell") bar (event-spine.md §8). CIV = the empire name (the civic-name-on-civic-change bug lever).
enum NameChangeKind
{
	NAMECHANGE_PLAYER = 0,   // leader/player name (CvPlayer::getName)
	NAMECHANGE_CIV,          // empire / civ short name (CvPlayer::getCivilizationShortDescription)
	NAMECHANGE_CITY,         // city name (CvCity::getName)
	NAMECHANGE_UNIT          // unit name (CvUnit::getName)
};

//	Emit a name-change DOMAIN event. Call AFTER the name field is updated (the consumer resolves the NEW name live).
//	iOwner = owning player; iEntityId = city/unit id (pass iOwner for PLAYER/CIV). String-free payload by design.
void emitNameChange(int iKind, int iOwner, int iEntityId);

// ===== the DOMAIN emit ENDPOINTS -- the ONE API every state-change choke point calls (event-spine.md: "a DOMAIN
// event on every state change"). Each builds a source-carrying DOMAIN event and hands it to eventSpine().emit().
// Call AFTER the state field is updated. Source-carrying: the event names WHAT (iType) + WHO (iOwner) + WHERE
// (iSrcLoc), so a consumer can route it -- but the endpoints themselves only emit (no consumer/routing here).
//
// ⛔ THERE IS NO DIRECTION PARAMETER ANYWHERE ON THIS SURFACE, and its absence is the design. A `bool bHas` / an
// `int iDelta` whose SIGN carries the direction is exactly the discriminator docs/spine.md §A FACT NAMES THE HAPPENING bans:
// it makes the caller state what happened in a payload the consumer must then branch on. The CALLER picks the
// endpoint that names what it just did; a magnitude argument is always HOW MANY, never which way.
//
// ⛔ A SLOT REPLACEMENT CALLS BOTH, REMOVED FIRST. The withdrawal must be announced while the OLD STATE STILL
// HOLDS ([state-repositories.md] § THE INVARIANT) -- emit() dispatches synchronously, so ordering the two calls
// IS the guarantee. Calling ADDED first leaves the removal resolving against state that has already moved, which
// is the exact gap the old `*_CHANGED` shape left. =====
void emitCityBuildingAdded(int iCity, int iOwner, int iBuilding, bool bFirst);
void emitCityBuildingRemoved(int iCity, int iOwner, int iBuilding);
void emitCityBuildingProcessed(int iCity, int iOwner, int iBuilding, int iCount);   // DIAGNOSTIC -- "the apply ran"
void emitCityBuildingActivated(int iCity, int iOwner, int iBuilding);
// The EMPIRE-LEVEL building facts (docs/specs/enabler.md §2 (empire-level buildings)): the PLAYER's held crossing, and the player-side
// operate verdict crossing the member's own deposits ride. iSrcLoc is -1 -- there is no city.
void emitEmpireBuildingAdded(int iOwner, int iBuilding, bool bFirst);
void emitEmpireBuildingRemoved(int iOwner, int iBuilding);
void emitEmpireBuildingActivated(int iOwner, int iBuilding);
void emitEmpireBuildingDormanted(int iOwner, int iBuilding);
void emitCityBuildingDormanted(int iCity, int iOwner, int iBuilding);
// Observability only (logging + the player notification) -- drives no apply.
void emitCityBuildingObsoletedAdded(int iCity, int iOwner, int iBuilding);
void emitCityBuildingObsoletedRemoved(int iCity, int iOwner, int iBuilding);
void emitLoadPipeline(int iRebuildMs, int iFixpointMs, int iFixEnsureMs, int iFixProcessMs, int iPasses, int iFlips, int iConverged, int iVerifyCatches, int iPlotWarmMs, int iPackageWarmMs);
void emitCityReligionAdded(int iCity, int iOwner, int iReligion);
void emitCityReligionRemoved(int iCity, int iOwner, int iReligion);
void emitCityCorporationAdded(int iCity, int iOwner, int iCorporation);
void emitCityCorporationRemoved(int iCity, int iOwner, int iCorporation);
// The ACTIVE-verdict crossing (the presence pair's four-leg composed twin) -- emitted ONLY by CityContext's
// verdict store, which is the one place that remembers what held before.
void emitCityCorporationActiveAdded(int iCity, int iOwner, int iCorporation);
void emitCityCorporationActiveRemoved(int iCity, int iOwner, int iCorporation);
// The NETWORK supply presence crossing (0 <-> non-zero), not a count move.
void emitCityBonusAdded(int iCity, int iOwner, int iBonus);
void emitCityBonusRemoved(int iCity, int iOwner, int iBonus);
// The city's LOCAL (vicinity) supply. iCount = HOW MANY, unsigned -- a city can hold several of a bonus locally,
// so the magnitude is real; the direction is the endpoint.
void emitCityVicinityBonusAdded(int iCity, int iOwner, int iBonus, int iCount);
void emitCityVicinityBonusRemoved(int iCity, int iOwner, int iBonus, int iCount);
// The city grew / shrank. iCount = HOW MANY population moved, unsigned -- NEVER the new total. The save read calls
// ADDED with the stored amount, which is why the reseed needs no bespoke load verb.
void emitCityPopulationAdded(int iCity, int iOwner, int iCount);
void emitCityPopulationRemoved(int iCity, int iOwner, int iCount);
void emitCitySpecialistAdded(int iCity, int iOwner, int iSpecialist, int iCount);
void emitCitySpecialistRemoved(int iCity, int iOwner, int iSpecialist, int iCount);
void emitCityPowerAdded(int iCity, int iOwner);
void emitCityPowerRemoved(int iCity, int iOwner);
// A STATUS crossing. Call at the derived CROSSING only -- a status ticks down every turn, and a per-decrement emit
// would announce a fact that did not change.
void emitCityStatusAdded(int iCity, int iOwner, int iStatus, int iTurns);
void emitCityStatusRemoved(int iCity, int iOwner, int iStatus, int iTurns);
void emitUnitStatusAdded(int iUnit, int iOwner, int iStatus, int iTurns, int iPlot);
void emitUnitStatusRemoved(int iUnit, int iOwner, int iStatus, int iTurns, int iPlot);
// The city's amenity fold crossing 0 <-> non-zero on ONE key (SEVT_CITY_AMENITY_ADDED / _REMOVED above).
// Government centre and fresh water ride this; POWER does not -- it announces the GATED verdict instead.
void emitCityAmenityAdded(int iCity, int iOwner, int iAmenity, int iCount);
void emitCityAmenityRemoved(int iCity, int iOwner, int iAmenity, int iCount);
// A holy-city / headquarters designation moved. Call per AFFECTED city -- the old one REMOVED, the new one ADDED.
void emitCityHolyCityAdded(int iCity, int iOwner, int iReligion);
void emitCityHolyCityRemoved(int iCity, int iOwner, int iReligion);
void emitCityHeadquartersAdded(int iCity, int iOwner, int iCorporation);
void emitCityHeadquartersRemoved(int iCity, int iOwner, int iCorporation);
// The culture level slot moved (+ the workable-radius / vicinity growth it drives). REMOVED the old, ADDED the new.
void emitCityCultureLevelAdded(int iCity, int iOwner, int iLevel);
void emitCityCultureLevelRemoved(int iCity, int iOwner, int iLevel);
// The city's owner slot moved. REMOVED names the losing owner, ADDED the gaining one.
void emitCityOwnerAdded(int iCity, int iOwner, int iPlot);
void emitCityOwnerRemoved(int iCity, int iOwner);
// Network MEMBERSHIP: the city's centre plot left one plot-group and joined another.
void emitCityNetworkAdded(int iOwner, int iCity, int iPlotGroup);
void emitCityNetworkRemoved(int iOwner, int iCity, int iPlotGroup);
// The production queue gained / lost an order (iOrderType = OrderTypes).
void emitCityOrderAdded(int iCity, int iOwner, int iOrderType, int iItem);
void emitCityOrderRemoved(int iCity, int iOwner, int iOrderType, int iItem);
// ===== PLOT SUBSTRATE. A replacement calls REMOVED(old) then ADDED(new) -- see the block header above. =====
void emitPlotTerrainAdded(int iPlot, int iOwner, int iTerrain);
void emitPlotTerrainRemoved(int iPlot, int iOwner, int iTerrain);
void emitPlotFeatureAdded(int iPlot, int iOwner, int iFeature);
void emitPlotFeatureRemoved(int iPlot, int iOwner, int iFeature);
void emitPlotImprovementAdded(int iPlot, int iOwner, int iImprovement);
void emitPlotImprovementRemoved(int iPlot, int iOwner, int iImprovement);
void emitPlotRouteAdded(int iPlot, int iOwner, int iRoute);
void emitPlotRouteRemoved(int iPlot, int iOwner, int iRoute);
void emitPlotBonusAdded(int iPlot, int iOwner, int iBonus);
void emitPlotBonusRemoved(int iPlot, int iOwner, int iBonus);
void emitPlotTypeAdded(int iPlot, int iOwner, int iPlotType);
void emitPlotTypeRemoved(int iPlot, int iOwner, int iPlotType);
void emitPlotLandmarkAdded(int iPlot, int iOwner, int iLandmark);
void emitPlotLandmarkRemoved(int iPlot, int iOwner, int iLandmark);
void emitPlotRiverAdded(int iPlot, int iOwner, int iCrossingCount);
void emitPlotRiverRemoved(int iPlot, int iOwner, int iCrossingCount);
void emitPlotIrrigationAdded(int iPlot, int iOwner);
void emitPlotIrrigationRemoved(int iPlot, int iOwner);
void emitPlotOwnerAdded(int iPlot, int iOwner);
void emitPlotOwnerRemoved(int iPlot, int iOwner);
// Radius MEMBERSHIP (which city may work the plot), distinct from the citizen assignment below.
void emitPlotWorkingCityAdded(int iPlot, int iOwner, int iCity);
void emitPlotWorkingCityRemoved(int iPlot, int iOwner, int iCity);
// City-driven: the plot is WHERE it happened (iSrcLoc), the city is WHO assigned the citizen.
void emitPlotWorkedAdded(int iPlot, int iOwner, int iCity);
void emitPlotWorkedRemoved(int iPlot, int iOwner, int iCity);
// A city sat down on / was removed from the plot. ONE pair at CvPlot::setPlotCity covers the radius-count
// pass-throughs (changeCityRadiusCount / changePlayerCityRadiusCount).
void emitPlotCityAdded(int iPlot, int iOwner, int iCity);
void emitPlotCityRemoved(int iPlot, int iOwner, int iCity);
// The plot's own derived verdict crossed. Emitted by PlotContext at the crossing ONLY -- never from a mutation
// site, which owns the SOURCE and not the verdict.
void emitPlotPredicateAdded(int iPlot, int iOwner, int iPredicate);
void emitPlotPredicateRemoved(int iPlot, int iOwner, int iPredicate);
void emitPlotServedBonusAdded(int iPlot, int iOwner, int iBonus);
void emitPlotServedBonusRemoved(int iPlot, int iOwner, int iBonus);
// The plot entered / left a city's potential work area. Emitted by CvPlot::setWorkableBy only.
void emitPlotWorkableByAdded(int iPlot, int iOwner, int iCity);
void emitPlotWorkableByRemoved(int iPlot, int iOwner, int iCity);
// ===== EMPIRE / player =====
void emitEmpireTechAdded(int iPlayer, int iTech);
void emitEmpireTechRemoved(int iPlayer, int iTech);
void emitEmpireTraitAdded(int iPlayer, int iTrait);
void emitEmpireTraitRemoved(int iPlayer, int iTrait);
void emitEmpireProjectAdded(int iPlayer, int iProject, int iCount);
void emitEmpireProjectRemoved(int iPlayer, int iProject, int iCount);
void emitEmpireHeritageAdded(int iPlayer, int iHeritage);
void emitEmpireHeritageRemoved(int iPlayer, int iHeritage);
// A state-religion SWAP calls REMOVED(outgoing) then ADDED(incoming).
void emitEmpireStateReligionAdded(int iPlayer, int iReligion);
void emitEmpireStateReligionRemoved(int iPlayer, int iReligion);
void emitEmpireGoldenAgeAdded(int iPlayer);
void emitEmpireGoldenAgeRemoved(int iPlayer);
void emitEmpireAnarchyAdded(int iPlayer);
void emitEmpireAnarchyRemoved(int iPlayer);
void emitEmpireRebelAdded(int iPlayer);
void emitEmpireRebelRemoved(int iPlayer);
// The era advanced: REMOVED(old era) then ADDED(new era).
void emitEmpireEraAdded(int iPlayer, int iEra);
void emitEmpireEraRemoved(int iPlayer, int iEra);
// The player's own SAVED handicap moved (flexible difficulty). A slot replacement.
void emitEmpireHandicapAdded(int iPlayer, int iHandicap);
void emitEmpireHandicapRemoved(int iPlayer, int iHandicap);
// The empire's nuke AVAILABILITY (it built / lost a nuke-enabling building). The world BAN is its own pair below.
void emitEmpireNukesEnabledAdded(int iPlayer);
void emitEmpireNukesEnabledRemoved(int iPlayer);
// A commerce slider moved. Call AFTER the percent field is updated, at EVERY choke point that moves it -- including
// the setter's own rebalance of the channels the caller did not name (each is its own fact). iPoints = HOW MANY
// percent points moved, unsigned.
void emitEmpireCommercePercentAdded(int iPlayer, int iCommerce, int iPoints);
void emitEmpireCommercePercentRemoved(int iPlayer, int iCommerce, int iPoints);
// The capital slot moved. Call AFTER the replacement city has been chosen -- consumers need somewhere to put what
// a capital carries (above all the palace, which is what MAKES a city the capital).
void emitEmpireCapitalAdded(int iOwner, int iCity);
void emitEmpireCapitalRemoved(int iOwner, int iCity);
// The player's per-TYPE empire tally moved. iCount = HOW MANY, unsigned.
void emitEmpireBuildingCountAdded(int iPlayer, int iBuilding, int iCount);
void emitEmpireBuildingCountRemoved(int iPlayer, int iBuilding, int iCount);
void emitEmpireUnitCountAdded(int iPlayer, int iUnit, int iCount);
void emitEmpireUnitCountRemoved(int iPlayer, int iUnit, int iCount);
// ===== PLOT GROUP / AREA =====
void emitPlotGroupBonusAdded(int iOwner, int iPlotGroupId, int iBonus, int iCount);
void emitPlotGroupBonusRemoved(int iOwner, int iPlotGroupId, int iBonus, int iCount);
void emitAreaTileAdded(int iArea, int iCount);
void emitAreaTileRemoved(int iArea, int iCount);
// ===== TEAM =====
void emitTeamMemberAdded(int iTeam, int iCount);
void emitTeamMemberRemoved(int iTeam, int iCount);
// ===== UNIT =====
// A unit INSTANCE was created / died. Call KILLED from CvUnit::die and nowhere else: that function is the only
// unconditional end of a unit's life. iPlot = -1 where the unit held none.
void emitUnitCreated(int iUnitType, int iUnitId, int iOwner, int iPlot);
void emitUnitKilled(int iUnitType, int iUnitId, int iOwner, int iPlot);
// A resolved combat. Called from CvEventReporter::combatResult, BESIDE its Python callback -- the reporter's
// arguments verbatim, so the spine sees exactly what Python sees and a later migration is a swap.
// The three winner-health arguments are RENDER-ONLY diagnostics -- BEFORE is the pre-combat snapshot, AFTER the
// damage the winner carries now, so a win where they are equal says the winner took nothing.
void emitCombatResult(int iWinnerUnitType, int iWinnerUnitId, int iWinnerOwner,
					  int iLoserUnitType, int iLoserUnitId, int iLoserOwner, int iPlot,
					  int iWinnerDamageBefore, int iWinnerDamageAfter, int iWinnerMaxHP);
// A unit's death SCHEDULE. Call AFTER m_bDeathDelay is written, at BOTH transitions: ADDED where a delayed kill
// deferred the death, REMOVED where an outcome brought the unit back.
void emitUnitDeathScheduleAdded(int iUnitType, int iUnitId, int iOwner, int iPlot);
void emitUnitDeathScheduleRemoved(int iUnitType, int iUnitId, int iOwner, int iPlot);
void emitUnitEnteredCity(int iUnitType, int iUnitId, int iOwner, int iCity);
void emitUnitLeftCity(int iUnitType, int iUnitId, int iOwner, int iCity);
// The unit plane's two mark triggers. Call from the ONE body each (CvUnit::processPromotion /
// CvUnit::setHasUnitCombatInternal), never from the setter overloads that pass through them.
void emitUnitPromotionAdded(int iUnitId, int iOwner, int iPromotion);
void emitUnitPromotionRemoved(int iUnitId, int iOwner, int iPromotion);
void emitUnitCombatAdded(int iUnitId, int iOwner, int iUnitCombat);
void emitUnitCombatRemoved(int iUnitId, int iOwner, int iUnitCombat);
// ===== WORLD / GAME =====
void emitWorldNukesBannedAdded();
void emitWorldNukesBannedRemoved();
// MONOTONIC -- the world's cumulative created-count only ever grows, so there is no REMOVED half.
void emitWorldUnitCreatedCountAdded(int iUnitType, int iCount);
// Every area identity was reassigned (CvMap::recalculateAreas). Carries no payload: the fact IS "all of them", so
// every holder of an area id re-reads on it.
void emitAreasRecalculated();
// A game option was turned on / off. eSpace = GameOptionSpace -- WHICH id space iOption speaks.
void emitGameOptionAdded(int iOption, int iValue, int eSpace);
void emitGameOptionRemoved(int iOption, int iValue, int eSpace);
// The derived human-average handicap behind every getAI* advantage. A slot replacement.
void emitGameHandicapAdded(int iHandicap);
void emitGameHandicapRemoved(int iHandicap);
// A global define took a new value -- the LIVE-OPTION bridge. ONE pair for the three typed setters; eKind =
// GlobalDefineKind says which of the value arguments is the real one. szName/szValue are borrowed for the
// SYNCHRONOUS emit only and are never copied.
void emitGameGlobalDefineAdded(const char* szName, int eKind, int iValue, float fValue, const char* szValue);
void emitGameGlobalDefineRemoved(const char* szName, int eKind, int iValue, float fValue, const char* szValue);
// ===== PROPERTY (any owner scope) =====
// A game object's property value moved. Call AFTER the value is written, from the CvProperties mutation choke
// points.
// iObjectKind = GameObjectTypes (what iObjectId identifies); iOwner = NO_PLAYER (-1) where the object has none.
// iAmount = HOW MUCH the value moved, unsigned.
void emitPropertyAdded(int iObjectKind, int iObjectId, int iOwner, int iProperty, int iAmount);
void emitPropertyRemoved(int iObjectKind, int iObjectId, int iOwner, int iProperty, int iAmount);
void emitCityPropertyBandAdded(int iCityId, int iOwner, int iProperty);
void emitCityPropertyBandRemoved(int iCityId, int iOwner, int iProperty);
// ===== NAMED HAPPENINGS + lifecycle =====
// Turn boundaries. iPlayer = -1 for the GAME-scope boundary, else the player whose turn opened/closed. These
// REPLACE the bespoke CvHttpServer::publishEvent("turnStart"/...) side-channel: a happening lives on the spine
// ONCE, and the file + /events stream consumers carry it for free.
void emitTurnStarted(int iTurn, int iPlayer);
void emitTurnEnded(int iTurn, int iPlayer);
// A city was FOUNDED. Call from CvPlayer::found once the city exists, BEFORE the settle-time provisions run.
// The founding unit is passed so its `grants.buildings` (json §5) can resolve against the new city.
void emitCityFounded(int iOwner, int iCity, int iFounderType, int iFounderId);
void emitTechAcquired(int iPlayer, int iTech);
// A religion was FOUNDED. Carries what the founder-grant apply needs: the CHOSEN religion (sets the free-unit
// TYPE) and the SLOT being claimed (sets the free-unit COUNT; the two are deliberately different), plus the award
// flag and the holy city the units spawn in.
void emitReligionFounded(int iPlayer, int iReligion, int iSlotReligion, int iCity, bool bAward);
// A civic was adopted (the revolution pulse). iOldCivic = the one it displaced (-1 = an empty slot).
void emitCivicAdopted(int iPlayer, int iCivic, int iOldCivic);
void emitPlayerInit(int iPlayer);

// The load-lifecycle bracket (event-spine.md the load-RESEED): emit STARTED before the save read begins, FINISHED
// after it completes. Result-producers (grants) suppress between them; the cache-build consumer stays load-active.
void emitGameLoadStarted();
void emitGameLoadFinished();

// One traded item taking effect / ceasing between two empires (SEVT_EMPIRE_TRADE_ADDED / _REMOVED above).
void emitEmpireTradeAdded(int iItem, int iData, int iFromPlayer, int iToPlayer, int iDeal);
void emitEmpireTradeRemoved(int iItem, int iData, int iFromPlayer, int iToPlayer, int iDeal);

// A serialized BLOCK's contents, announced from the read itself (SEVT_SAVELOAD_BLOCK above). SAVELOAD kind, so
// logging alone consumes it. szBlock must outlive the emit -- pass a literal (the render is synchronous on the
// game thread, the SEVT_NAME_CHANGE precedent).
void emitSaveLoadBlock(const char* szBlock, int iCount);
void emitSaveLoadCensus(const char* szBlock, int iCount);
void emitDealVerifyCancel(int iDeal, int iBonus, int iTradeable, const char* szReason);
void emitDealAiEvaluated(int iDeal, int iPlayer, int iOtherPlayer, int iDenial, const char* szVerdict);

// The agreement between two empires beginning / ending (SEVT_EMPIRE_DEAL_ADDED / _REMOVED above).
void emitEmpireDealAdded(int iDeal, int iFirstPlayer, int iSecondPlayer);
void emitEmpireDealRemoved(int iDeal, int iFirstPlayer, int iSecondPlayer);
// True between GAME_LOAD_STARTED and GAME_LOAD_FINISHED -- the load-active window (the reseed). Consumers that must
// behave differently during the reseed (e.g. skip play-time targeted ripples) read this.
bool spineGameLoadInProgress();


// The short human name of a spine event id (e.g. "cityReligionAdded").
const char* spineEventName(int iEventId);

//	A consumer of spine events (tally / grants / logging). C++03 virtual interface -- the consumer's state lives in the
//	consumer (no captures, no Boost). wantedKinds() returns a bitmask of (1 << EventKind); the spine uses it to skip
//	dispatch when nobody wants a kind AND to filter per consumer.
class IEventConsumer
{
public:
	virtual ~IEventConsumer() {}
	virtual int wantedKinds() const = 0;
	virtual void onEvent(const CvSpineEvent& kEvent) = 0;
};

//	The front door. Consumers register once at startup; emit() dispatches to interested consumers and SKIPS ENTIRELY
//	when no registered consumer wants the event's kind (the cheap interest-guard -- so a dormant DIAGNOSTIC/TRACE
//	firehose costs ~nothing, while DOMAIN always flows because the grants consumer is listening). One instance: eventSpine().
class CvEventSpine
{
public:
	CvEventSpine() : m_iInterestMask(0) {}

	void registerConsumer(IEventConsumer* pConsumer);
	void emit(const CvSpineEvent& kEvent);

	bool anyInterest(EventKind eKind) const { return (m_iInterestMask & (1 << eKind)) != 0; }
	const std::vector<IEventConsumer*>& consumers() const { return m_consumers; }

private:
	std::vector<IEventConsumer*> m_consumers;
	int m_iInterestMask; // OR of all registered consumers' wantedKinds() -- the interest-guard
};

//	The single engine-wide spine.
CvEventSpine& eventSpine();

//	Register the built-in cascade consumers -- the broad logging consumer AND the selective grants consumer. The tally
//	is NOT a consumer (it reads the object-owned counts on demand, tally.md). Idempotent; call once
//	(CvGame::doTurn guards it).
void spineRegisterConsumers();

// [PERF] per-consumer dispatch time, accumulated across every emit between a reset and a log. The log line names
// each consumer by its type and its summed milliseconds, so a slow sweep is attributed to the consumer that
// spent it rather than to the emit as a whole.
void spinePerfConsumerAccumReset();
void spinePerfConsumerAccumLog(const char* szPhase, int iOwner);

#endif // CV_EVENT_SPINE_H
