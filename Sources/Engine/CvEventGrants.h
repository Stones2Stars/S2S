#pragma once
#ifndef CV_EVENT_GRANTS_H
#define CV_EVENT_GRANTS_H

//
//	EVENT GRANT -- a value a PYTHON EVENT handed to an object, kept apart from everything derived.
//
//	⚖ THE MODEL (owner): an event grant is genuine ONE-SHOT state. Nothing derives it and nothing can recompute
//	it, so it lives in its OWN serialized store ON THE OBJECT THE EVENT GRANTED TO -- never rolled into a
//	derived accumulator. Rolling it in is exactly what this rebuild is undoing: a recompute-from-source cache
//	rebuilds from live sources, so whatever an event put there is silently wiped on the next rebuild
//	("having events just be stored in the cache is lunacy"). The split is the same one the building-commerce
//	and free-bonus stores already make -- the reader sums CASCADE (derivable) + THIS (persisted).
//
//	⚖ ONE STANDARDIZED OBJECT, AND IT REFERENCES THE EVENT (owner). The record carries the granting event id
//	rather than an anonymous number, so the coming Python-events rework can attribute, inspect and re-issue what
//	an event gave. A store that held only amounts would force that rework to guess.
//
//	⚑ EXTENSIBLE BY DATA, not by members: a newly-granted channel is a new kind id inside an existing record,
//	never a new field and never a second store (docs/cascade.md §EVERY DERIVED STORE IS ONE SHAPE -- a hand-named per-channel field cannot
//	be addressed uniformly, which is how the legacy accumulated one per flavour).
//
//	⚠ PRE-SPLIT GRANTS IN AN EXISTING SAVE ARE DEEMED LOST (owner). They were rolled directly into the dirtied
//	cache instead of living here, so there is nothing to migrate out and no reader should look for one. New
//	grants land here. (Its tags are absent from old saves, which is soft -- save.md §2.)
//
//	⛔ This is NOT the trigger/grants machine. That plane APPLIES a provision on a genuine acquisition; this is
//	only the STORAGE for the residue an event leaves behind on an object, for values that would otherwise have
//	no home once their derived twin recomputes.
//

//	The vocabulary a granted value is addressed in. A domain says WHICH id space eKind and eTarget speak.
enum EventGrantDomain
{
	//	CITY: a percent on one commerce channel (eKind = CommerceTypes, no target).
	EVENTGRANT_COMMERCE_RATE_MODIFIER = 0,
	//	CITY: flat commerce on a keyed BUILDING (eKind = CommerceTypes, eTarget = BuildingTypes).
	EVENTGRANT_BUILDING_COMMERCE,
	//	TEAM/PLAYER: a yield on a keyed IMPROVEMENT (eKind = YieldTypes, eTarget = ImprovementTypes).
	EVENTGRANT_IMPROVEMENT_YIELD,

	NUM_EVENTGRANT_DOMAINS
};

//	ONE granted value. Kept flat and POD so the store serializes as plain parallel columns.
struct CvEventGrant
{
	int eEvent;    // EventTypes -- WHICH event granted this; -1 when unattributed
	int eDomain;   // EventGrantDomain -- the id space the two below speak
	int eKind;     // the granted channel within that domain
	int eTarget;   // the keyed target, or -1 when the grant is scope-wide
	int iValue;    // the granted amount
};

class CvTaggedSaveFormatWrapper;

//
//	The store. ONE implementation, held by every scope owner that can be granted to
//	(docs/architecture/patterns.md §DRY (single implementation)) -- the same move CvStatus.h makes for applied counters.
//
class CvEventGrantStore
{
public:
	//	Record a grant. Entries ACCUMULATE rather than replace: two events granting the same channel both
	//	happened, and the rework needs to see both.
	void add(EventGrantDomain eDomain, int eEvent, int eKind, int eTarget, int iValue);

	//	The read: the summed granted amount for one address. eTarget -1 means "scope-wide" and matches only
	//	scope-wide records, so a keyed grant never leaks into the scope-wide answer.
	int sum(EventGrantDomain eDomain, int eKind, int eTarget) const;

	//	Whole-store access, for the events rework and for display/attribution.
	int size() const;
	const CvEventGrant& record(int iIndex) const;

	void clear();

	//	Serialization support. The store cannot own its own read/write bodies: the wrapper macros build the tag
	//	by concatenating STRING LITERALS (className "::" saveName), so the owning class name has to be present as
	//	a literal at the call site. The macros below are therefore the ONE implementation
	//	(docs/architecture/patterns.md §DRY (single implementation)) and each owner invokes them from its own read()/write() with its own name.
	void resize(int iCount);
	CvEventGrant& mutableRecord(int iIndex);

private:
	std::vector<CvEventGrant> m_grants;
};

//	⛔ Uniquely-named sub-tags, deliberately NOT the shared "iNumElts" count tag the legacy variable-length
//	blocks reuse: that one shared name is exactly what makes those blocks un-listable in savemigration.txt
//	(save.md §3 -- a bracket-free …Size/…Value sub-tag is the sanctioned form).
#define EVENT_GRANTS_READ(wrapper, className, store)                                                        \
	{                                                                                                       \
		int iEventGrantCount = 0;                                                                           \
		WRAPPER_READ_DECORATED(wrapper, className, &iEventGrantCount, "EventGrantsSize");                   \
		(store).resize(iEventGrantCount);                                                                   \
		for (int iEventGrantIndex = 0; iEventGrantIndex < iEventGrantCount; iEventGrantIndex++)             \
		{                                                                                                   \
			CvEventGrant& kGrant = (store).mutableRecord(iEventGrantIndex);                                 \
			WRAPPER_READ_DECORATED(wrapper, className, &kGrant.eEvent,  "EventGrantsEvent");                \
			WRAPPER_READ_DECORATED(wrapper, className, &kGrant.eDomain, "EventGrantsDomain");               \
			WRAPPER_READ_DECORATED(wrapper, className, &kGrant.eKind,   "EventGrantsKind");                 \
			WRAPPER_READ_DECORATED(wrapper, className, &kGrant.eTarget, "EventGrantsTarget");               \
			WRAPPER_READ_DECORATED(wrapper, className, &kGrant.iValue,  "EventGrantsValue");                \
		}                                                                                                   \
	}

#define EVENT_GRANTS_WRITE(wrapper, className, store)                                                       \
	{                                                                                                       \
		const int iEventGrantCount = (store).size();                                                        \
		WRAPPER_WRITE_DECORATED(wrapper, className, iEventGrantCount, "EventGrantsSize");                   \
		for (int iEventGrantIndex = 0; iEventGrantIndex < iEventGrantCount; iEventGrantIndex++)             \
		{                                                                                                   \
			const CvEventGrant& kGrant = (store).record(iEventGrantIndex);                                  \
			WRAPPER_WRITE_DECORATED(wrapper, className, kGrant.eEvent,  "EventGrantsEvent");                \
			WRAPPER_WRITE_DECORATED(wrapper, className, kGrant.eDomain, "EventGrantsDomain");               \
			WRAPPER_WRITE_DECORATED(wrapper, className, kGrant.eKind,   "EventGrantsKind");                 \
			WRAPPER_WRITE_DECORATED(wrapper, className, kGrant.eTarget, "EventGrantsTarget");               \
			WRAPPER_WRITE_DECORATED(wrapper, className, kGrant.iValue,  "EventGrantsValue");                \
		}                                                                                                   \
	}

#endif // CV_EVENT_GRANTS_H
