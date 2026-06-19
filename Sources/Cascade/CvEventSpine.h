#pragma once
#ifndef CV_EVENT_SPINE_H
#define CV_EVENT_SPINE_H

#include <vector>

//
//	CvEventSpine -- the #430 cascade's FRONT DOOR (design: docs/dev/plans/event-spine-spec.md).
//
//	Callers emit(KIND, type, payload) to the spine; consumers read the event KINDS they care about. The TALLY
//	(authoritative state counts), `grants`, and LOGGING are all consumers of this one dispatch spine. The spine
//	sits IN FRONT OF the tally -- the tally never reaches into game state, the events come to it.
//
//	KIND is the OOS FIREWALL axis, declared at the call site (never inferred):
//	  - DOMAIN     : game STATE changed (building built, unit created). SYNCED/deterministic -> the tally counts it,
//	                 gate-eligible. The only kind that may feed the authoritative tally.
//	  - DIAGNOSTIC : code RAN (a function entered, a decision re-evaluated N times). UNSYNCED execution trace ->
//	                 logging only; NEVER gates, NEVER counted into the authoritative tally.
//	  - TRACE      : fine-grained "show me every step" -> logging only; the tally ignores it entirely.
//
//	Two consumer appetites, one front door: LOGGING is BROAD (sees everything, outputs per the existing log gates);
//	the TALLY is SELECTIVE (takes only the DOMAIN kinds it counts). Payload is RAW (never a pre-formatted string) so
//	the costly index->text formatting defers to the gated logging consumer.
//
//	C++03 / VC7.1: virtual interface, no lambdas, no Boost (event-spine-spec section 6). The substrate accumulator
//	(CvScopedAccumulator) is the count primitive the tally instantiates; this spine is the dispatch primitive.
//
enum EventKind
{
	EVENTKIND_DOMAIN = 0,   // synced state change -> tally + grants + logging; gate-eligible
	EVENTKIND_DIAGNOSTIC,   // unsynced execution trace -> logging only
	EVENTKIND_TRACE,        // fine-grained step trace -> logging only; tally ignores
	NUM_EVENT_KINDS
};

//	======================= the RAW field payload (event-spine-spec section 3, RESOLVED 2026-06-18) =======================
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
	// typeIndex kinds: the int is a Types index, rendered to its type string via GC.getXInfo (resolution in the consumer).
	// This is how a former "%s = GC.getXInfo(i).getType()" line becomes a clean raw field (the index travels, not the string).
	SFT_BUILDING, SFT_UNIT, SFT_TECH, SFT_PLAYER,
	SFT_BONUS, SFT_IMPROVEMENT, SFT_PROMOTION, SFT_RELIGION, SFT_CORPORATION, SFT_FEATURE, SFT_TERRAIN, SFT_CIVIC, SFT_PROJECT, SFT_SPECIALIST
};

//	Field identities are DOMAIN-LOCAL (owner 2026-06-18): each migrated domain defines its OWN field-tag enum + a
//	resolver mapping tag -> (name, type), registered with the spine (below). The spine holds NO global field registry --
//	this isolates each domain (Clean-Architecture), kills the fragile central enum-plus-two-parallel-tables sync, and lets
//	domains migrate in PARALLEL with zero shared edits. (Constant labels like "action=safety" are NOT fields -- they live
//	in the event prefix.) A field slot's `eTag` is interpreted only together with the event's `iDomainTag`.
struct CvCascadeEventField
{
	int eTag; // a DOMAIN-LOCAL field tag (resolved to name+type by the domain's registered SpineFieldInfoFn)
	union { int i; float f; } v;
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

//	A cascade event: KIND (firewall axis) + a RAW, self-describing payload (NEVER a formatted string). Two payload modes,
//	both raw: DOMAIN count events use iType + iA/iB/iC (the tally reads these); logging events (DIAGNOSTIC/TRACE) use
//	iDomainTag + iEventId (-> the constant line prefix) + aFields[iFieldCount] (the variable "name=value" fields).
struct CvCascadeEvent
{
	EventKind eKind;
	int iEventId;
	int iType;
	int iA;
	int iB;
	int iC;

	int iDomainTag;   // SpineDomainTag -- the line's [TAG] family (logging events); SD_NONE for the legacy count path
	int iLevel;       // the surveillance level this line emits at (1 Telescreen .. 4 Thought Police); consumer gates on it
	int iFieldCount;  // number of valid aFields (0 = a legacy count event, uses iType/iA/iB/iC)
	CvCascadeEventField aFields[SPINE_MAX_FIELDS];

	// Legacy DOMAIN-count constructor (iFieldCount stays 0; aFields unused).
	CvCascadeEvent(EventKind eKind_, int iEventId_, int iType_ = -1, int iA_ = 0, int iB_ = 0, int iC_ = 0)
		: eKind(eKind_), iEventId(iEventId_), iType(iType_), iA(iA_), iB(iB_), iC(iC_)
		, iDomainTag(SD_NONE), iLevel(1), iFieldCount(0) {}

	// Logging-event constructor (DIAGNOSTIC/TRACE): domain + event id + the level it emits at, then add fields. The
	// domain param is the SpineDomainTag ENUM (not int) -- this disambiguates from the legacy int-eventId ctor above.
	CvCascadeEvent(EventKind eKind_, SpineDomainTag eDomainTag_, int iEventId_, int iLevel_)
		: eKind(eKind_), iEventId(iEventId_), iType(-1), iA(0), iB(0), iC(0)
		, iDomainTag((int)eDomainTag_), iLevel(iLevel_), iFieldCount(0) {}

	// Append a raw field by its DOMAIN-LOCAL tag (int or typeIndex). No-op past the cap (a backstop; lines <=16 by the catalog).
	CvCascadeEvent& addI(int iFieldTag, int iValue)
	{
		if (iFieldCount < SPINE_MAX_FIELDS) { aFields[iFieldCount].eTag = iFieldTag; aFields[iFieldCount].v.i = iValue; ++iFieldCount; }
		return *this;
	}
	CvCascadeEvent& addF(int iFieldTag, float fValue)
	{
		if (iFieldCount < SPINE_MAX_FIELDS) { aFields[iFieldCount].eTag = iFieldTag; aFields[iFieldCount].v.f = fValue; ++iFieldCount; }
		return *this;
	}
};

//	Render a logging event's RAW fields into szBuf as "<prefix> name=value name=value ..." (the consumer's job).
//	prefix from the domain's registered prefix provider; field names/types from the domain's registered SpineFieldInfoFn. Declared here,
//	defined in the .cpp beside the logging consumer.
void cascadeRenderEventLine(char* szBuf, int iBufSize, const CvCascadeEvent& kEvent);

//	Real (non-test) DOMAIN event ids -- WHAT changed in synced game state. Distinct namespace from the temporary
//	DIAGNOSTIC ids in CvCascadeSelfTest (the KIND prefix in the log disambiguates). This is the production side: real
//	gameplay state-changes emit these, so the spine (and next the tally) is driven by genuine input, not a recompute.
enum CascadeDomainEvent
{
	CASCADE_EVT_BUILDING_COUNT = 1,  // iType = BuildingTypes, iA = new empire count, iB = delta, iC = PlayerTypes -- a counted-domain event
	CASCADE_EVT_UNIT_COUNT     = 2,  // iType = UnitTypes,     iA = new empire count, iB = delta, iC = PlayerTypes
	CASCADE_EVT_NAME_CHANGE    = 3   // iType = NameChangeKind, iA = owner player, iB = entity id (= owner for PLAYER/CIV), iC = 0
};

//	Which entity's display name changed (the iType of a CASCADE_EVT_NAME_CHANGE event). The logging consumer resolves the
//	NEW name LIVE (synchronous game-thread render -> exact), so the payload stays string-free; an out-of-process consumer
//	(GameTracker / an agent) rebuilds its id->name table from the emitted lines -- REQUIRED for the total-observability
//	("Orwell") bar (event-spine-spec.md section 8). CIV = the empire name (the civic-name-on-civic-change bug lever).
enum NameChangeKind
{
	NAMECHANGE_PLAYER = 0,   // leader/player name (CvPlayer::getName)
	NAMECHANGE_CIV,          // empire / civ short name (CvPlayer::getCivilizationShortDescription)
	NAMECHANGE_CITY,         // city name (CvCity::getName)
	NAMECHANGE_UNIT          // unit name (CvUnit::getName)
};

//	Emit a name-change DOMAIN event. Call AFTER the name field is updated (the consumer resolves the NEW name live).
//	iOwner = owning player; iEntityId = city/unit id (pass iOwner for PLAYER/CIV). String-free payload by design.
void cascadeEmitNameChange(int iKind, int iOwner, int iEntityId);

//	A consumer of spine events (tally / grants / logging). C++03 virtual interface -- the consumer's state lives in the
//	consumer (no captures, no Boost). wantedKinds() returns a bitmask of (1 << EventKind); the spine uses it to skip
//	dispatch when nobody wants a kind AND to filter per consumer.
class IEventConsumer
{
public:
	virtual ~IEventConsumer() {}
	virtual int wantedKinds() const = 0;
	virtual void onEvent(const CvCascadeEvent& kEvent) = 0;
};

//	The front door. Consumers register once at startup; emit() dispatches to interested consumers and SKIPS ENTIRELY
//	when no registered consumer wants the event's kind (the cheap interest-guard -- so a dormant DIAGNOSTIC/TRACE
//	firehose costs ~nothing, while DOMAIN always flows because the tally is listening). One instance: eventSpine().
class CvEventSpine
{
public:
	CvEventSpine() : m_iInterestMask(0) {}

	void registerConsumer(IEventConsumer* pConsumer);
	void emit(const CvCascadeEvent& kEvent);

	bool anyInterest(EventKind eKind) const { return (m_iInterestMask & (1 << eKind)) != 0; }

private:
	std::vector<IEventConsumer*> m_consumers;
	int m_iInterestMask; // OR of all registered consumers' wantedKinds() -- the interest-guard
};

//	The single engine-wide spine.
CvEventSpine& eventSpine();

//	Register the built-in cascade consumers -- the broad logging consumer AND the selective tally (registered + seeded
//	from current state via rebuild(); maintained incrementally by DOMAIN events thereafter, §9). Idempotent; call once
//	(CvGame::doTurn guards it).
void cascadeRegisterConsumers();

#endif // CV_EVENT_SPINE_H
