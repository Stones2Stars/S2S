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
//	  - DIAGNOSTIC : code RAN (a function entered, a decision re-evaluated N times). UNSYNCED execution trace ->
//	                 logging only; NEVER gates.
//	  - TRACE      : fine-grained "show me every step" -> logging only.
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
	SFT_UNITCOMBAT
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
	// #430 cascade diagnostic domains (per-emitter, one file -- all tee to Cascade.log). Each self-registers in its
	// own .cpp (spineRegisterDomain); the [TAG] sub-area lives in the per-domain prefix fn ([READJSON/*],
	// [MODIFIER/*]). Diagnostic lines (EVENTKIND_DIAGNOSTIC) -- census/diagnostic traces, logging only.
	SD_READJSON,   // [READJSON] the JSON->InfoRepo load census (CvReadJson)
	SD_ENABLER,    // [ENABLER] reserved (historical tag; no live registrant)
	SD_MODIFIER,   // [MODIFIER] the per-scope channel-set census (CvCascadeChannelRegistry)
	SD_TRIGGERS,     // [TRIGGERS] the payload plane: a `triggers` entry is the general form, and a `grants` block
	                 // is its degenerate case (implicit happening, no condition, no roll) -- json.md §5
	SD_SPINE,      // [SPINE] spine lifecycle signals (game-load started/finished) -- rendered via the registered prefix, not inline
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
	SEVT_BUILDING_COUNT = 1,  // iType = BuildingTypes, iA = new empire count, iB = delta, iC = PlayerTypes -- a counted-domain event
	SEVT_UNIT_COUNT     = 2,  // iType = UnitTypes,     iA = new empire count, iB = delta, iC = PlayerTypes
	SEVT_NAME_CHANGE    = 3,  // iType = NameChangeKind, iA = owner player, iB = entity id (= owner for PLAYER/CIV), iC = 0
	SEVT_TECH_ACQUIRED  = 4,  // iType = TechTypes, iA = 1 (first-discoverer), iC = discovering player -- the tech-grant trigger
	SEVT_RELIGION_FOUNDED = 5,// iType = ReligionTypes, iC = founding player -- the religion-founder grant trigger
	SEVT_CIVIC_ADOPTED  = 6,  // iType = CivicTypes, iC = adopting player -- the civic grant trigger (revolution pulse)
	SEVT_PLAYER_INIT    = 7,  // iType = iC = player -- game start: resolve the player's civ/era/handicap grants

	// The per-source STATE-CHANGE events -- the COMPLETE emit surface (event-spine.md: a DOMAIN event on EVERY state
	// change a package can depend on). iType = source type index; iC = owner / triggering player; iSrcLoc = cityId
	// (per-city) | plotId (per-plot) | -1 (empire/team/world); iA/iB carry has/new-value/delta where meaningful.
	SEVT_BUILDING_CHANGED       = 8,  // CvCity::setHasBuilding: iType=Building, iC=owner, iSrcLoc=cityId, iB=+1/-1,
	                                  // iA=bFirst (1 = genuine first acquisition, 0 = conquest transfer / load restore)
	SEVT_RELIGION_CHANGED       = 9,  // CvCity::setHasReligion: iType=Religion, iC=owner, iSrcLoc=cityId, iA=has
	SEVT_CORPORATION_CHANGED    = 10, // CvCity::setHasCorporation: iType=Corp, iC=owner, iSrcLoc=cityId, iA=has
	// ⚖ The city's obtained-bonus PRESENCE CROSSING ONLY (0 <-> non-zero), by owner ruling -- NOT every count move.
	// processNumBonusChange calls processBonus solely when the has-verdict crosses zero, so a count going 2 -> 3
	// announces nothing, deliberately: a per-count fact would drag in attribution edge cases (WHICH city or source
	// added this copy) that the crossing sidesteps entirely. ⛔ Do NOT "fix" the missing count fact -- it is a scope
	// boundary, not a gap. A count-threshold reader (a `min: 3` atom) is knowingly outside it for now.
	SEVT_BONUS_CHANGED          = 11, // CvCity::processBonus/doVicinityBonus: iType=Bonus, iC=owner, iSrcLoc=cityId, iB=change
	SEVT_POPULATION_CHANGED     = 12, // CvCity::setPopulation: iC=owner, iSrcLoc=cityId, iA=newPop
	SEVT_SPECIALIST_CHANGED     = 13, // CvCity::setSpecialistCount: iType=Specialist, iC=owner, iSrcLoc=cityId, iB=delta
	SEVT_POWER_CHANGED          = 14, // CvCity::changePowerCount: iC=owner, iSrcLoc=cityId, iB=delta
	// plot SUBSTRATE changes -- the ACTUAL state changes. Yield is a COMPUTED RESULT of these, never itself an event
	// (an improvement/terrain/feature/route changed; the yield recomputes downstream). These emit for observability
	// + consumers, but they are NOT the yield-cache gate: plot yield is pull-computed (updateYield self-dirties).
	SEVT_IMPROVEMENT_CHANGED    = 15, // CvPlot::setImprovementType: iType=Improvement, iC=owner, iSrcLoc=plotId
	SEVT_TERRAIN_CHANGED        = 16, // CvPlot::setTerrainType: iType=Terrain, iC=owner, iSrcLoc=plotId
	SEVT_FEATURE_CHANGED        = 17, // CvPlot::setFeatureType: iType=Feature, iC=owner, iSrcLoc=plotId
	SEVT_ROUTE_CHANGED          = 18, // CvPlot::setRouteType: iType=Route, iC=owner, iSrcLoc=plotId
	SEVT_TECH_CHANGED           = 19, // CvTeam::setHasTech (BROAD -- any set): iType=Tech, iC=triggering player, iA=has
	SEVT_TRAIT_CHANGED          = 20, // CvPlayer::processTrait/setHasTrait: iType=Trait, iC=player, iA=add
	SEVT_PROJECT_CHANGED        = 21, // CvTeam::changeProjectCount (+ the load reseed): iType=Project, iC=member player (PER-MEMBER, one emit per alive team member), iB=count delta
	SEVT_GOLDEN_AGE_CHANGED     = 22, // CvPlayer::changeGoldenAgeTurns (flip): iC=player, iA=on
	SEVT_STATE_RELIGION_CHANGED = 23, // CvPlayer::setLastStateReligion: iType=Religion, iC=player
	// ownership changes -- a city/plot changed OWNER (conquest / culture flip / gift): the entity's packages move
	// scope and BOTH owners' empire aggregates change. iSrcLoc = cityId | plotId; iA = OLD owner; iC = NEW owner.
	SEVT_CITY_OWNER_CHANGED     = 24, // a city changed owner (acquire / dispose)
	SEVT_PLOT_OWNER_CHANGED     = 25, // CvPlot::setOwner
	// a plot's WORKING CITY was reassigned -- the plot's yield moves from the old city to the new (both cities'
	// yield packages change). iSrcLoc = plotId; iA = old working cityId; iB = new working cityId; iC = owner.
	SEVT_WORKING_CITY_CHANGED   = 26, // CvPlot::setWorkingCity
	// load LIFECYCLE -- a synced DOMAIN signal (NOT a state change): the save read is beginning / complete.
	// Result-producers (grants) rely PURELY on the spine, so they gate on THESE -- LOAD_STARTED -> suppress,
	// LOAD_FINISHED -> resume (a grant is a RESULT of a genuine in-play acquisition, and a load is not one). The
	// cache-build consumer stays load-active. (event-spine.md the load-RESEED.)
	SEVT_GAME_LOAD_STARTED      = 27,
	SEVT_GAME_LOAD_FINISHED     = 28,
	// plot SUBSTRATE (sibling of terrain/feature/improvement/route, §15-18): a plot's RESOURCE (BONUS_*) was placed /
	// discovered / removed. All play-time paths route through CvPlot::setBonusType (a Great-Farmer build, a discovery
	// event, removal); the reseed fires the SAME event. iType=Bonus, iC=owner, iSrcLoc=plotId, iB=+1 placed / -1 removed.
	SEVT_PLOT_BONUS_CHANGED     = 29,
	// DIAGNOSTIC (not a state-change): the cache invalidation OBSERVABILITY -- a package was marked dirty. Carries the
	// scope + owning-object id + the package-bit names + the source event. The Orwell bar for the invalidation flow.
	SEVT_CACHE_INVALIDATE       = 30,
	// DIAGNOSTIC: the complement -- a package was REBUILT (recomputed from dirty). invalidate = max work queued;
	// rebuilt = work actually done. Carries scope + owner + id + the package-bit names (no src).
	SEVT_CACHE_REBUILT          = 31,
	// a player HERITAGE was acquired/removed (CvPlayer::setHeritage) -- feeds the empire package's commerce
	// flats (era-stacked). Acquired mid-game via MISSION_HERITAGE. iType=Heritage, iC=player, iA=add. DOMAIN.
	SEVT_HERITAGE_CHANGED       = 32,
	// a plot-group (the connectivity / trade-NETWORK identity) gained/lost access to a resource --
	// CvPlotGroup::changeNumBonuses on a presence transition. A traded resource enters at the capital's group and
	// reaches every connected city; the consumer re-evals connection:trade deposits for the group's member cities.
	// iType=Bonus, iC=owner, iSrcLoc=plotGroupId, iB=+1 gained / -1 lost. DOMAIN.
	SEVT_PLOTGROUP_BONUS_CHANGED = 33,
	// a CITY's own center plot moved to a different plot-group (merge/split -- CvPlot::setPlotGroup, owner-gated), so
	// its whole NETWORK resource set changed. The membership twin of SEVT_PLOTGROUP_BONUS_CHANGED (which is the
	// resource-set twin). iC=owner, iSrcLoc=cityId. DOMAIN.
	SEVT_CITY_NETWORK_CHANGED   = 34,
	// a player's ERA advanced (CvPlayer::setCurrentEra) -- a BROAD player-scope cascade input: heritage era-stacked
	// commerce (the empire package), every ERA-counter-threshold deposit, and ERA requires atoms (frontier). iC=player,
	// iA=newEra. DOMAIN. (Cache-masked today via setHasTech's tech mark, but the state change is its own fact.)
	SEVT_ERA_CHANGED            = 35,
	// a player's NUKE STATE changed -- one of THREE: 0 DISABLED (no nuke-enabling building) / 1 ENABLED (built +
	// available) / 2 BANNED (the world AP-UN no-nukes vote or option). "available" is PER-PLAYER (`m_bNukesValid`,
	// `makeNukesValid` at CvCity processBuilding); "banned" is WORLD (`isNoNukes`) and flips EVERY player at once.
	// iC=player, iA=state(0/1/2). DOMAIN. (The cascade NO_NUKES predicate reads only the world BAN half.)
	SEVT_NUKES_CHANGED          = 36,
	// a city's CULTURE LEVEL changed (CvCity::setCultureLevel). TWO things ride this one fact: the culture-level
	// cascade input (wonder caps, defense) AND the city's workable RADIUS growth -- so it is ALSO the vicinity
	// MEMBERSHIP signal (the city gains/loses plots into its vicinity). iC=owner, iSrcLoc=cityId, iA=newLevel. DOMAIN.
	SEVT_CITY_CULTURE_LEVEL_CHANGED = 37,
	// a religion's HOLY CITY designation moved (CvGame::setHolyCity) -- the IS_HOLY_CITY / IS_STATE_RELIGION_HOLY_CITY
	// predicates flip for the OLD city (loses) and the NEW city (gains); gates conditioned commerce/yields. Emitted
	// per affected city. iType=religion, iC=cityOwner, iSrcLoc=cityId, iA=1 now holy / 0 no longer. DOMAIN.
	SEVT_HOLY_CITY_CHANGED      = 38,
	// a city's LOCAL (vicinity) supply of a bonus changed -- an active providing building appeared/vanished
	// (CvCity::processBuilding at construction/destruction). Vicinity supply never adds an owned COUNT (that lives on
	// the plot group); the consumer re-gates the bonus's connection:vicinity dependents. iType=Bonus, iC=owner,
	// iSrcLoc=cityId, iB=the applied local delta (a city can hold several of a bonus locally). DOMAIN.
	// ⚠ The BUILDING half is the only half this fact carries. The MAP half -- a bonus appearing/vanishing on a radius
	// tile, or that tile changing hands -- is announced by the PLOT facts (SEVT_PLOT_BONUS_CHANGED /
	// SEVT_PLOT_OWNER_CHANGED / SEVT_PLOT_WORKED_CHANGED) and by the radius growth on SEVT_CITY_CULTURE_LEVEL_CHANGED;
	// a consumer holding a city-scope vicinity store folds both halves from those. There is deliberately no per-turn
	// sweep behind this fact -- a missed emit must stay visibly wrong (DEC-no-self-heal).
	SEVT_VICINITY_BONUS_CHANGED = 39,
	// a present building's PROCESSED (operating-contribution) state flipped -- construction/destruction's
	// processing leg AND a dormancy active<->dormant flip (both reach processBuilding). DISTINCT from
	// SEVT_BUILDING_CHANGED (the PRESENCE fact, emitted at CvCity::setHasBuilding): events are facts, and a
	// process flip is NOT a presence change -- conflating them fed the enabler domains fake has-flips (the
	// 62-phantom-in-tree over-offer). Consumed by the modifier invalidation (package dirties + the operating-set
	// mark); the enabler domains consume only the presence fact. iType=Building, iC=owner, iSrcLoc=cityId,
	// iB=+1 processed-in / -1 processed-out. DOMAIN.
	SEVT_BUILDING_PROCESSED     = 40,
	// the load-end pipeline diagnostic (DIAGNOSTIC -- logging/observability only): the stage timings + the
	// dormancy-fixpoint depth, announced once per load through the registered SD_SPINE render path.
	SEVT_LOAD_PIPELINE          = 41,
	// a city's production QUEUE gained/lost an order (CvCity::pushOrder / popOrder). The enabler's queue leg
	// (enabler.md par.7.1 step 3 -- "queueing/completion is the targeted single-id erase"): a QUEUED building
	// leaves the fresh offer (a queued candidate is excluded from a FRESH offer by definition), and
	// a dequeue restores it -- the event triggers the one-id re-gate; the gate itself reads the live queue
	// (object-owned state, the resolved-fork rule). iType = the ordered item id, iA = OrderTypes,
	// iB = +1 push / -1 pop, iC = owner, iSrcLoc = cityId. DOMAIN.
	SEVT_CITY_ORDER_CHANGED     = 42,
	// TURN BOUNDARIES. The turn counter advancing is a genuine synced state change, so both are DOMAIN. They
	// bracket the real boundary: the GAME pair straddles CvGame::incrementGameTurn (ended = the closing turn,
	// started = the incremented one); the PLAYER pair rides CvPlayer::setTurnActive. iType = the game turn,
	// iC = the player (-1 = the GAME-scope boundary). Consumers filter on iC; the emit surface stays complete.
	SEVT_TURN_STARTED           = 43,
	SEVT_TURN_ENDED             = 44,
	// A unit ENTERED a friendly city's plot (CvUnit::setXY's new-city branch). The targeted trigger for anything
	// a city hands to units PRESENT in it -- above all building free promotions, which are otherwise a per-turn
	// rescan of (promo buildings x every unit on the plot). NOT emitted on every move: only on a city entry, so
	// the stream stays proportional to a rare fact rather than to unit traffic.
	// ⚠ This must never grow into cache invalidation -- unit movement invalidating cache is a standing owner
	// "full stop" ([DEC-unit-modifiers-on-top]; the per-move clear caused an automation storm). A GRANT is
	// one-shot state, not a recompute, which is why it may ride here.
	// iType = unit TYPE, iA = unit id, iC = owner, iSrcLoc = city id. DOMAIN.
	SEVT_UNIT_ENTERED_CITY      = 45,
	// A unit INSTANCE was created (CvUnit::init). Distinct from SEVT_UNIT_COUNT, which is the player's per-TYPE
	// tally and carries no instance -- the grants machine needs the actual unit to hand its `grants.promotions` to.
	// iType = unit TYPE, iA = unit id, iC = owner. DOMAIN.
	SEVT_UNIT_CREATED           = 46,
	// A city was FOUNDED (CvPlayer::found, once the city object exists and before the settle-time provisions).
	// ⛔ Distinct from SEVT_CITY_OWNER_CHANGED, which fires on ACQUIRE (conquest/trade) and on the load restore --
	// founding produced NO identifiable fact before this, only a constellation of side-effects (populationChanged,
	// plotOwnerChanged, cityNetworkChanged), which is why the settle-time provisions had no trigger to hang on.
	// iType = the FOUNDING unit's type (-1 if none), iA = that unit's id, iC = owner, iSrcLoc = the new city.
	SEVT_CITY_FOUNDED           = 47,
	// The empire's CAPITAL changed -- relocation after the old capital was lost (CvPlayer::findNewCapital, once it
	// has PICKED the replacement), or a capital being established. The spine carried no capital fact at all, so
	// nothing could react to a capital moving. iC = owner, iSrcLoc = the new capital city (-1 = none left).
	SEVT_CAPITAL_CHANGED        = 48,
	// plot SUBSTRATE, the remaining five (siblings of terrain/feature/improvement/route/bonus, par.15-18 + 29). Each
	// is a genuine plot state change that carried no DOMAIN fact, so its consumers had to be wired from the setter.
	// The plot's TYPE (CvPlot::setPlotType): flat / hills / peak / OCEAN. Load-bearing well beyond relief, because
	// CvPlot::isWater() IS getPlotType() == PLOT_OCEAN -- the whole water/land axis (and every neighbour's coast
	// verdict) hangs off this fact, not off terrain. iType = the NEW PlotTypes, iA = the OLD one (a consumer acting
	// on the delta needs both, as with plotOwnerChanged), iC = owner, iSrcLoc = plotId. DOMAIN.
	SEVT_PLOT_TYPE_CHANGED      = 49,
	// The plot's RIVER presence flipped (CvPlot::changeRiverCrossingCount crossing zero) -- the count is a running
	// tally of river-carrying edges, and only the PRESENCE transition is a fact worth announcing. iA = 1 has river /
	// 0 no longer, iB = the new crossing count, iC = owner, iSrcLoc = plotId. DOMAIN.
	SEVT_PLOT_RIVER_CHANGED     = 50,
	// The plot's IRRIGATION flipped (CvPlot::setIrrigated) -- the spread of irrigation water, distinct from the
	// improvement that carries it. iA = 1 irrigated / 0 not, iC = owner, iSrcLoc = plotId. DOMAIN.
	SEVT_PLOT_IRRIGATION_CHANGED = 51,
	// The plot's LANDMARK designation changed (CvPlot::setLandmarkType) -- the named natural feature (peak range,
	// bay, lake, ...) the map generator and the landmark events assign. iType = the NEW LandmarkTypes, iA = the OLD
	// one, iC = owner, iSrcLoc = plotId. DOMAIN.
	SEVT_PLOT_LANDMARK_CHANGED  = 52,
	// A citizen started / stopped WORKING the plot (CvCity::setWorkingPlot). CITY-driven, so it carries the city as
	// well as the plot: the fact belongs to the plot (its IS_WORKED verdict flips) but only the city can attribute
	// it. DISTINCT from SEVT_WORKING_CITY_CHANGED, which is the RADIUS-MEMBERSHIP fact (which city may work the
	// plot) -- membership is the superset, working is the citizen actually assigned to it.
	// iA = 1 worked / 0 no longer, iB = the working cityId, iC = owner, iSrcLoc = plotId. DOMAIN.
	SEVT_PLOT_WORKED_CHANGED    = 53,
	// EVERY area identity was reassigned (CvMap::recalculateAreas: every plot's area is cleared, the area list is
	// emptied, and the areas are recalculated from scratch). It is the ONE wholesale-reassignment fact, so it carries
	// no id: after it, EVERY holder of an area id must re-read, rather than each inventing its own staleness test.
	// Areas are virtually never recalculated -- terrain levelled to sea level (the WMD mechanic) plus map generation --
	// so announcing the whole reassignment costs nothing at its real frequency. This is NOT the banned self-heal: a
	// wholesale identity reassignment is not addressable per-source, so there is no finer route to derive
	// (DEC-no-self-heal bans papering over a MISSED invalidation, not announcing a genuine wholesale one).
	// No payload -- the fact IS "all of them". DOMAIN.
	SEVT_AREAS_RECALCULATED     = 54,
	// A commerce SLIDER moved (CvPlayer::setCommercePercent) -- the empire's split of its cities' COMMERCE yield
	// across gold / research / culture / espionage. Synced player state, deterministic and OOS-relevant, so
	// DOMAIN and never DIAGNOSTIC: every city's realized rate of the moved channel is built on it (modifier.md
	// §2a, the commerce paragraph). ⚠ ONE slider move emits SEVERAL of these -- the setter REBALANCES the other
	// channels in place to keep the total at 100, and each channel it moves is its own state change.
	// iType = CommerceTypes, iA = the new percent, iB = the old percent, iC = player, iSrcLoc = -1. DOMAIN.
	SEVT_COMMERCE_PERCENT_CHANGED = 55,
	// A game object's PROPERTY VALUE changed (CvProperties -- the generic (PropertyTypes,int) bag on
	// game/team/player/city/unit/plot). DOMAIN: a property value is synced, deterministic, save-carried state that
	// folds into the OOS checksum, and PROPERTY_* is one cascade channel per property info
	// (state-repositories.md), read by CityContext::propertyValue, every requires.operate property BAND
	// (enabler.md par.3) and every deposit conditioned on a property threshold.
	// iType = PropertyTypes, iA = the NEW value, iB = the object KIND (GameObjectTypes -- what iSrcLoc identifies;
	// a city id and a plot id are otherwise the same int), iC = owner player (-1 for the game, a team, an unowned
	// plot), iSrcLoc = the object's OWN id (cityId | unitId | plotId | playerId | teamId; -1 = the game). The OLD
	// value rides as a render field (the ints are full).
	// ⚠ The solver's change PROPAGATION (CvProperties::propagateChange -- FLAMMABILITY's city->player rollup) fans
	// one change onto OTHER objects, each of which re-enters the mutation path and emits its own fact. Those are
	// DISTINCT objects' facts, not duplicates -- every one of them emits.
	SEVT_PROPERTY_CHANGED = 56,
	// The city's power-DISABLED state flipped (CvCity::changeDisabledPowerTimer). CvCity::isPower() ORs THREE legs
	// -- the power COUNT (SEVT_POWER_CHANGED), this timer, and the area clean-power flag below -- so the HAS_POWER
	// verdict is stale unless all three announce. ⚠ The timer TICKS DOWN every turn (CvCity::doDisabledPower), so
	// only the derived 0-CROSSING is a fact; emitting per decrement would fire every turn for no state change.
	// iA = 1 disabled / 0 restored, iB = the new timer value, iC = owner, iSrcLoc = cityId. DOMAIN.
	SEVT_CITY_POWER_DISABLED_CHANGED = 57,
	// An AREA's CLEAN-POWER flag flipped for a TEAM (CvArea::changeCleanPowerCount, at its existing count crossing)
	// -- the third leg of CvCity::isPower(), reached through CvCity::isAreaCleanPower(). The fact is scoped to
	// (area x team), which has no owning player, so iC stays -1 and the team rides iB.
	// iA = 1 clean-powered / 0 no longer, iB = teamId, iC = -1, iSrcLoc = areaId. DOMAIN.
	SEVT_AREA_CLEAN_POWER_CHANGED = 58,
	// A corporation's HEADQUARTERS designation moved (CvGame::setHeadquarters) -- the IS_HEADQUARTERS predicate
	// flips for the OLD city (loses) and the NEW city (gains). Emitted per affected city, the setHolyCity shape.
	// ⛔ NOT a duplicate of the changeHasBuilding / setHasCorporation calls the same setter makes: those announce
	// building PRESENCE and corporation PRESENCE, and an HQ designation is neither.
	// iType = Corporation, iA = 1 now HQ / 0 no longer, iC = city owner, iSrcLoc = cityId. DOMAIN.
	SEVT_HEADQUARTERS_CHANGED = 59,
	// The city SITTING ON a plot changed (CvPlot::setPlotCity) -- a city was founded/acquired onto the plot or
	// removed from it. DISTINCT from SEVT_WORKING_CITY_CHANGED (which city may WORK the plot) and from
	// SEVT_CITY_FOUNDED (the founding act, which does not fire on razing or on a plot losing its city).
	// ⚠ CvPlot::changeCityRadiusCount / changePlayerCityRadiusCount are PASS-THROUGHS of this setter -- this one
	// fact covers them; a second emit there would announce the same change per radius plot.
	// iA = old cityId (-1 = none), iB = new cityId (-1 = none), iC = owner, iSrcLoc = plotId. DOMAIN.
	SEVT_PLOT_CITY_CHANGED = 60,
	// The city's GOVERNMENT-CENTRE verdict flipped -- the palace/counterpart buildings that make a city a
	// maintenance origin. The verdict is the city's AMENITY FOLD, so the crossing is announced by the contexts'
	// consumer around the fold that moved it (ContextConsumer::announceAmenityCrossings). DISTINCT from
	// SEVT_CAPITAL_CHANGED: a capital is always a government centre, but a government centre need not be capital.
	// iA = 1 now a government centre / 0 no longer, iC = owner, iSrcLoc = cityId. DOMAIN.
	SEVT_GOVERNMENT_CENTER_CHANGED = 61,
	// The player entered / left ANARCHY (CvPlayer::changeAnarchyTurns, at its existing 0-crossing). Anarchy zeroes
	// the empire's commerce and suspends civic/corporation effects, so IS_ANARCHY is a live cascade input.
	// iA = 1 in anarchy / 0 no longer, iC = player, iSrcLoc = -1. DOMAIN.
	SEVT_ANARCHY_CHANGED = 62,
	// The city's FRESH-WATER ACCESS flipped (CvCity::changeFreshWater, at its existing count crossing) -- the
	// PROVIDER-BUILDING-fed access counter. ⚠ DISTINCT from the plot-adjacency HAS_FRESHWATER verdict the plot
	// substrate maintains (CvPlot::isFreshWater): a building can grant a city access on a dry plot.
	// iA = 1 has access / 0 no longer, iB = the new counter, iC = owner, iSrcLoc = cityId. DOMAIN.
	SEVT_CITY_FRESH_WATER_CHANGED = 63,
	// ===== the UNIT plane. state-repositories.md: a unit's resolved values dirty "ONLY when a promotion or a
	// combat class changes" -- neither trigger existed, so the plane had no fact to rebuild from. =====
	// A unit gained / lost a PROMOTION (CvUnit::processPromotion -- the single funnel BOTH setHasPromotion
	// overloads reach; the PromotionApply::flags overload delegates to the bool overload, which calls it).
	// iType = Promotion, iA = unit id, iB = +1 gained / -1 lost, iC = owner, iSrcLoc = -1. DOMAIN.
	SEVT_UNIT_PROMOTION_CHANGED = 64,
	// A unit gained / lost a COMBAT CLASS (CvUnit::processUnitCombat -- the single funnel setHasUnitCombat reaches
	// once past BOTH its change guard and its game-option/spy validity gate) -- the unit plane's second dirty
	// trigger. A promotion's subCombat grants route through the same setter, so the fact is emitted once per
	// genuine class change regardless of what caused it.
	// iType = UnitCombat, iA = unit id, iB = +1 gained / -1 lost, iC = owner, iSrcLoc = -1. DOMAIN.
	SEVT_UNIT_COMBAT_CHANGED = 65,
	// A unit INSTANCE died -- the DEATH TWIN of SEVT_UNIT_CREATED, without which grants and the out-of-process
	// replay see units born and never die. Emitted on the FIRST line of CvUnit::die, the one function that ends
	// a unit's life: die() carries no early return and no conditional deletion, so the fact is true BY
	// CONSTRUCTION rather than by sitting past a run of survival branches. The outcomes that leave the unit
	// alive (evacuate-to-capital, last-stand survival) are decided BEFORE die() is entered and never reach it.
	// iType = unit TYPE, iA = unit id, iC = owner, iSrcLoc = the plot it died on (-1 = the unit held none).
	// DOMAIN.
	SEVT_UNIT_KILLED = 66,
	// A unit LEFT a city's plot (CvUnit::setXY's old-city branch) -- the leave twin of SEVT_UNIT_ENTERED_CITY.
	// ⚠ The leave is announced for EVERY city plot a unit vacates, while the entry's conquest branch resolves
	// into an acquisition instead of an entry -- so the two are not a balanced pair, and a consumer that counts
	// occupancy must read the unit's live plot rather than net the facts.
	// iType = unit TYPE, iA = unit id, iC = owner, iSrcLoc = the city id it left. DOMAIN.
	SEVT_UNIT_LEFT_CITY = 67,
	// The WORLD's cumulative created-count of a unit type advanced (CvGame::incrementUnitCreatedCount) -- read
	// live by the UnitEnabler's world-instance cap. The counter only ever grows, so every increment IS a distinct
	// state change (there is no verdict to cross). DISTINCT from SEVT_UNIT_COUNT (a player's LIVE per-type tally)
	// and from SEVT_UNIT_CREATED (the instance); all three fire at one unit's birth and none duplicates another.
	// iType = unit TYPE, iA = the new world count, iB = +1, iC = -1 (world scope), iSrcLoc = -1. DOMAIN.
	SEVT_UNIT_CREATED_COUNT_CHANGED = 68,
	// A TEAM's member count changed (CvTeam::changeNumMembers) -- the `TEAM` counter token
	// (EmpireContext::teamMemberCount). The COUNT itself is what the token reads, so every nonzero change is one
	// state change; the setter carries no guard of its own, so the emit supplies it.
	// iA = the new member count, iB = the change, iC = -1 (a team has no owning player), iSrcLoc = teamId. DOMAIN.
	SEVT_TEAM_MEMBERS_CHANGED = 69,
	// An AREA's tile count changed (CvArea::changeNumTiles) -- feeds CityContext's AREA_SIZE and its
	// max-adjacent-water store (the isCoastal(minArea) form). An area has no owning player, so iC stays -1.
	// iA = the new tile count, iB = the change, iC = -1, iSrcLoc = areaId. DOMAIN.
	SEVT_AREA_TILES_CHANGED = 70,
	// A unit's DEATH SCHEDULE flipped (CvUnit::m_bDeathDelay) -- the state a delayed kill leaves behind so the
	// object outlives combat resolution, save-carried and read by isDead()/isDelayedDeath() across the engine.
	// NOT a duplicate of SEVT_UNIT_KILLED: a scheduled death is an INTENTION whose outcome can still flip to
	// survival (evacuate-to-capital, last stand), and a consumer that treated the schedule as a death would
	// bury units that walk away. Both transitions announce, so a consumer never keeps a survivor marked dying.
	// iType = unit TYPE, iA = unit id, iB = 1 scheduled / 0 cleared, iC = owner, iSrcLoc = the plot it stands
	// on (-1 = none). DOMAIN.
	SEVT_UNIT_DEATH_SCHEDULED = 71
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
// (iSrcLoc), so a consumer can route it -- but the endpoints themselves only emit (no consumer/routing here). =====
void emitBuildingChanged(int iCity, int iOwner, int iBuilding, int iDelta, bool bFirst);
void emitBuildingProcessed(int iCity, int iOwner, int iBuilding, int iDelta);
void emitLoadPipeline(int iRebuildMs, int iFixpointMs, int iFixEnsureMs, int iFixProcessMs, int iPasses, int iFlips, int iConverged, int iVerifyCatches, int iPlotWarmMs, int iPackageWarmMs);
void emitReligionChanged(int iCity, int iOwner, int iReligion, bool bHas);
void emitCorporationChanged(int iCity, int iOwner, int iCorporation, bool bHas);
void emitBonusChanged(int iCity, int iOwner, int iBonus, int iChange);
void emitPopulationChanged(int iCity, int iOwner, int iNewPop);
void emitSpecialistChanged(int iCity, int iOwner, int iSpecialist, int iDelta);
void emitPowerChanged(int iCity, int iOwner, int iDelta);
// The plot-SUBSTRATE type facts carry the OLD value alongside the new (the plotOwnerChanged shape below): a
// consumer must be able to re-mark what LEFT, not only what arrived.
void emitImprovementChanged(int iPlot, int iOwner, int iOldImprovement, int iImprovement);
void emitPlotBonusChanged(int iPlot, int iOwner, int iBonus, int iChange);   // plot RESOURCE placed(+1)/removed(-1)
void emitTerrainChanged(int iPlot, int iOwner, int iOldTerrain, int iTerrain);
void emitFeatureChanged(int iPlot, int iOwner, int iOldFeature, int iFeature);
void emitRouteChanged(int iPlot, int iOwner, int iOldRoute, int iRoute);
void emitTechChanged(int iPlayer, int iTech, bool bHas);
void emitTraitChanged(int iPlayer, int iTrait, bool bAdd);
// A civic was adopted (revolution pulse). Mirrors the inline SEVT_CIVIC_ADOPTED emit in CvPlayer::setCivics so the
// full-state replay + any future callers share one clean endpoint. iType = CivicTypes, iC = adopting player.
void emitCivicAdopted(int iPlayer, int iCivic, int iOldCivic);   // the swap fact: adopted + swapped-out (iB)
void emitProjectChanged(int iPlayer, int iProject, int iDelta);
void emitGoldenAgeChanged(int iPlayer, bool bOn);
void emitStateReligionChanged(int iPlayer, int iReligion);
void emitHeritageChanged(int iPlayer, int iHeritage, bool bAdd);
void emitPlotGroupBonusChanged(int iOwner, int iPlotGroupId, int iBonus, int iDelta);   // network: a plot-group gained(+1)/lost(-1) a resource
void emitVicinityBonusChanged(int iCity, int iOwner, int iBonus, int iDelta);           // vicinity: a city's local presence of a bonus flipped (+1/-1)
void emitCityNetworkChanged(int iOwner, int iCity);   // network membership: a city's center plot moved to a different plot-group
void emitEraChanged(int iPlayer, int iEra);   // a player's era advanced (broad player-scope cascade input)
//	A commerce slider moved. Call AFTER the percent field is updated, at EVERY choke point that moves it --
//	including the setter's own rebalance of the channels the caller did not name (each is its own fact).
void emitCommercePercentChanged(int iPlayer, int iCommerce, int iNewPercent, int iOldPercent);
//	A game object's property value changed. Call AFTER the value is written, from the CvProperties mutation choke
//	points -- never from CvGameObject::eventPropertyChanged (the unit override does not chain to the base).
//	iObjectKind = GameObjectTypes (what iObjectId identifies); iOwner = NO_PLAYER (-1) where the object has none.
void emitPropertyChanged(int iObjectKind, int iObjectId, int iOwner, int iProperty, int iNewValue, int iOldValue);
//	The two silent legs of CvCity::isPower(), beside the power COUNT's emitPowerChanged. Call at the derived
//	CROSSING only -- the disabled-power timer ticks down every turn, and a per-decrement emit would announce a
//	fact that did not change.
void emitCityPowerDisabledChanged(int iCity, int iOwner, bool bDisabled, int iTimer);
void emitAreaCleanPowerChanged(int iArea, int iTeam, bool bCleanPower);
//	A corporation headquarters designation moved. Call per AFFECTED city -- the old one loses, the new one gains
//	(the emitHolyCityChanged shape). Never a substitute for the presence facts the same setter also drives.
void emitHeadquartersChanged(int iCity, int iOwner, int iCorporation, bool bIsHeadquarters);
//	The city sitting ON a plot changed. ONE emit at CvPlot::setPlotCity covers the radius-count pass-throughs.
void emitPlotCityChanged(int iPlot, int iOwner, int iOldCity, int iNewCity);
void emitGovernmentCenterChanged(int iCity, int iOwner, bool bIsGovernmentCenter);
void emitAnarchyChanged(int iPlayer, bool bAnarchy);
//	The city's PROVIDER-BUILDING-fed fresh-water ACCESS -- not the plot-adjacency fresh-water verdict.
void emitCityFreshWaterChanged(int iCity, int iOwner, bool bHasFreshWater, int iCount);
//	The unit plane's two dirty triggers. Call from the ONE funnel each (CvUnit::processPromotion /
//	CvUnit::processUnitCombat), never from the setter overloads that pass through them.
void emitUnitPromotionChanged(int iUnitId, int iOwner, int iPromotion, int iDelta);
void emitUnitCombatChanged(int iUnitId, int iOwner, int iUnitCombat, int iDelta);
//	A unit instance died -- the twin of emitUnitCreated. Call from CvUnit::die and nowhere else: that function is
//	the only unconditional end of a unit's life. iPlot = -1 where the unit held no plot.
void emitUnitKilled(int iUnitType, int iUnitId, int iOwner, int iPlot);
//	A unit's death SCHEDULE flipped. Call AFTER m_bDeathDelay is written, at both transitions: bScheduled = true
//	where a delayed kill deferred the death, false where an outcome brought the unit back.
void emitUnitDeathScheduled(int iUnitType, int iUnitId, int iOwner, int iPlot, bool bScheduled);
//	A unit left a city's plot -- the twin of emitUnitEnteredCity. Call from the old-city branch, before the move.
void emitUnitLeftCity(int iUnitType, int iUnitId, int iOwner, int iCity);
//	The world's cumulative created-count of a unit type advanced (the world-instance cap's input).
void emitUnitCreatedCountChanged(int iUnitType, int iNewCount, int iDelta);
//	A team's member count changed (the `TEAM` counter token's source).
void emitTeamMembersChanged(int iTeam, int iNewCount, int iDelta);
//	An area's tile count changed (AREA_SIZE + the city max-adjacent-water store).
void emitAreaTilesChanged(int iArea, int iNewCount, int iDelta);
void emitNukesChanged(int iPlayer, int iState);   // a player's nuke state: 0 disabled / 1 enabled / 2 banned
void emitCultureLevelChanged(int iCity, int iOwner, int iNewLevel, int iOldLevel);   // culture level old->new (+ the radius/vicinity growth it drives)
void emitHolyCityChanged(int iCity, int iOwner, int iReligion, bool bIsHoly);   // a city gained(true)/lost(false) a religion's holy-city designation
void emitCityOrderChanged(int iCity, int iOwner, int iOrderType, int iItem, int iDelta);   // production queue push(+1)/pop(-1) of an order (iOrderType = OrderTypes)
void emitCityOwnerChanged(int iCity, int iOldOwner, int iNewOwner);
//	Turn boundaries. iPlayer = -1 for the GAME-scope boundary, else the player whose turn opened/closed. These
//	REPLACE the bespoke CvHttpServer::publishEvent("turnStart"/"turnEnd"/"playerTurnStart"/"playerTurnEnd")
//	side-channel: a happening lives on the spine ONCE, and the file + /events stream consumers carry it for free.
void emitTurnStarted(int iTurn, int iPlayer);
void emitTurnEnded(int iTurn, int iPlayer);
//	A unit entered a friendly city's plot. Call from the new-city branch, AFTER the move is committed.
void emitUnitEnteredCity(int iUnitType, int iUnitId, int iOwner, int iCity);
//	A unit INSTANCE was created. Call from CvUnit::init once the unit is constructed enough to take a promotion.
void emitUnitCreated(int iUnitType, int iUnitId, int iOwner);
//	A city was founded. Call from CvPlayer::found once the city exists, BEFORE the settle-time provisions run.
//	The founding unit is passed so its `grants.buildings` (json §5) can resolve against the new city.
void emitCityFounded(int iOwner, int iCity, int iFounderType, int iFounderId);
//	The empire's capital changed. Call AFTER the replacement city has been chosen -- consumers need somewhere to
//	put what a capital carries (above all the palace, which is what MAKES a city the capital).
void emitCapitalChanged(int iOwner, int iCity);
void emitPlotOwnerChanged(int iPlot, int iOldOwner, int iNewOwner);
void emitWorkingCityChanged(int iPlot, int iOwner, int iOldCity, int iNewCity);
// The remaining plot-substrate facts. Each carries what a consumer needs to act on the DELTA, so a type/landmark
// change carries the OLD value alongside the new (the plotOwnerChanged shape) and a flip carries the new state.
void emitPlotTypeChanged(int iPlot, int iOwner, int iOldPlotType, int iNewPlotType);
void emitPlotRiverChanged(int iPlot, int iOwner, bool bHasRiver, int iCrossingCount);
void emitPlotIrrigationChanged(int iPlot, int iOwner, bool bIrrigated);
void emitPlotLandmarkChanged(int iPlot, int iOwner, int iOldLandmark, int iNewLandmark);
// City-driven: the plot is WHERE it happened (iSrcLoc), the city is WHO assigned the citizen.
void emitPlotWorkedChanged(int iPlot, int iOwner, int iCity, bool bWorked);
// Every area identity was reassigned (CvMap::recalculateAreas). Carries no payload: the fact IS "all of them", so
// every holder of an area id re-reads on it.
void emitAreasRecalculated();

// The empire-count observability events + the grant-trigger events -- distinct from the per-source state-change
// endpoints above (these carry the whole-empire count / a game-start or first-discover trigger, iSrcLoc = -1). One
// clean endpoint each so the emit sites in CvPlayer / CvTeam never build a CvSpineEvent inline (single-source; every
// DOMAIN emit is tagged for the logging render path). grants reads iType/iA/iB/iC off these (CvTriggerEngine).
void emitBuildingCount(int iPlayer, int iBuilding, int iNewCount, int iDelta);
void emitUnitCount(int iPlayer, int iUnit, int iNewCount, int iDelta);
void emitTechAcquired(int iPlayer, int iTech);
//	A religion was FOUNDED. Carries what the founder-grant apply needs: the CHOSEN religion (iReligion -- sets the
//	free-unit TYPE) and the SLOT being claimed (iSlotReligion -- sets the free-unit COUNT; the two are deliberately
//	different, see CvPlayer::foundReligion), plus the award flag and the holy city the units spawn in.
void emitReligionFounded(int iPlayer, int iReligion, int iSlotReligion, int iCity, bool bAward);
void emitPlayerInit(int iPlayer);

// The load-lifecycle bracket (event-spine.md the load-RESEED): emit STARTED before the save read begins, FINISHED
// after it completes. Result-producers (grants) suppress between them; the cache-build consumer stays load-active.
void emitGameLoadStarted();
void emitGameLoadFinished();
// True between GAME_LOAD_STARTED and GAME_LOAD_FINISHED -- the load-active window (the reseed). Consumers that must
// behave differently during the reseed (e.g. skip play-time targeted ripples) read this.
bool spineGameLoadInProgress();

// The cache-invalidation OBSERVABILITY (SEVT_CACHE_INVALIDATE): announce a package dirty-mark so the invalidation
// flow is verifiable in Cascade.log ("[CASCADE] invalidate scope=<team|empire|area|city|plot> id=<n> pkg=<NAMES>
// src=<why>"). iScope = the package's CvCascScope; iMask = the scope's 64-bit dirty mask (channel + receiver-sum
// bits, decoded to channel names via the CascadeChannelRegistry); szSource = the DOMAIN event that derived the
// mark (spineEventName). DIAGNOSTIC kind.
void emitCacheInvalidate(int iScope, int iOwner, int iId, int64_t iMask, const char* szSource);   // iOwner: the empire (city ids are unique only within a player); -1 = none
void emitCacheRebuilt(int iScope, int iOwner, int iId, int64_t iMask);   // the complement: a package was recomputed

// The short human name of a spine event id (e.g. "religionChanged") -- the invalidate observability's `src`.
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

#endif // CV_EVENT_SPINE_H
