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

//	A cascade event: KIND (firewall axis) + a RAW, self-describing payload (NEVER a formatted string -- formatting is
//	the gated logging consumer's job). Payload shape is PROVISIONAL (event-spine-spec section 3/9, flagged): eventId =
//	WHAT happened (a per-subsystem id), iType = the primary data Type index involved (-1 if none), iA/iB/iC = raw
//	slots (e.g. the building-count event carries iType=building, iA=new count, iB=delta, iC=player).
struct CvCascadeEvent
{
	EventKind eKind;
	int iEventId;
	int iType;
	int iA;
	int iB;
	int iC;

	CvCascadeEvent(EventKind eKind_, int iEventId_, int iType_ = -1, int iA_ = 0, int iB_ = 0, int iC_ = 0)
		: eKind(eKind_), iEventId(iEventId_), iType(iType_), iA(iA_), iB(iB_), iC(iC_) {}
};

//	Real (non-test) DOMAIN event ids -- WHAT changed in synced game state. Distinct namespace from the temporary
//	DIAGNOSTIC ids in CvCascadeSelfTest (the KIND prefix in the log disambiguates). This is the production side: real
//	gameplay state-changes emit these, so the spine (and next the tally) is driven by genuine input, not a recompute.
enum CascadeDomainEvent
{
	CASCADE_EVT_BUILDING_COUNT = 1,  // iType = BuildingTypes, iA = new empire count, iB = delta, iC = PlayerTypes -- a counted-domain event
	CASCADE_EVT_UNIT_COUNT     = 2   // iType = UnitTypes,     iA = new empire count, iB = delta, iC = PlayerTypes
};

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
