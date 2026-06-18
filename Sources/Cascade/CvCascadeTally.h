#pragma once
#ifndef CV_CASCADE_TALLY_H
#define CV_CASCADE_TALLY_H

#include "CvEventSpine.h"
#include "CvScopedAccumulator.h"

//
//	CvCascadeTally -- the #428/#430 additive COUNT machine (tally-cascade-spec.md), and the SELECTIVE consumer of
//	the event spine (wants ONLY DOMAIN events; the OOS firewall keeps DIAGNOSTIC/TRACE out).
//
//	It is the engine-side count read-surface the JSON gates land on. A count is keyed by (DOMAIN, type-index),
//	tracked per PLAYER (the leaf the change*Count events report at), and rolled up to team/world on read. Each
//	domain is the engine resolution of the JSON type PREFIX (BUILDING_* -> COUNTDOMAIN_BUILDING, UNIT_* -> _UNIT,
//	...). Maintained INCREMENTALLY by DOMAIN events; SEEDED by rebuild() (a scan of the authoritative objects);
//	serializes NOTHING (tally-spec §9). shadowVerify() diffs every per-player count vs getXCount (ground truth).
//
//	The JSON count vocabulary maps onto count() thus (the atom evaluator, step 2, applies the comparisons):
//	  presence == (count >= 1) · min:N == (count >= N) · max:N == (count <= N) ·
//	  allowed cap == (count < cap) · per:{each} == (count / each) · SELF == count(ownDomain, ownType, scope).
//

// The counted domains -- the engine side of the JSON type prefix. Extensible: add a value, then wire its emit
// site + seededTruth() + domainNumTypes() + onEvent() mapping + a shadow id. (tech/civic/religion/bonus/project
// follow buildings + units.)
enum CountDomain
{
	COUNTDOMAIN_BUILDING = 0,
	COUNTDOMAIN_UNIT,
	NUM_COUNT_DOMAINS
};

// The scopes a count clause can name. The TALLY itself serves only the cross-player roll-up (EMPIRE/TEAM/WORLD);
// CITY/PLOT are read directly off the live CvCity/CvPlot by the condition evaluator (cascadeAtomCount), never
// through the tally -- so the tally's count() ignores them (returns 0). They live in one enum so an atom carries
// a single scope field across both paths.
enum CountScope
{
	COUNTSCOPE_EMPIRE = 0, // iContext = player id          (tally)
	COUNTSCOPE_TEAM,       // iContext = team id            (tally)
	COUNTSCOPE_WORLD,      // iContext ignored              (tally)
	COUNTSCOPE_CITY,       // the context city              (direct read)
	COUNTSCOPE_PLOT        // the context city's plot       (direct read)
};

class CvCascadeTally : public IEventConsumer
{
public:
	int wantedKinds() const { return (1 << EVENTKIND_DOMAIN); } // SELECTIVE: domain only
	void onEvent(const CvCascadeEvent& kEvent);

	void rebuild(); // clear + seed every wired domain from the authoritative objects (the §9 rebuild path)

	// THE count read-surface the gates call. eDomain picks the bucket; eScope + iContext select the roll-up.
	int count(CountDomain eDomain, int iType, CountScope eScope, int iContext) const;

	// The count the ALLOWED CAP must read -- which differs from the live count for UNITS: a world-unique hero is
	// "created once, does one thing, then poofs", so the cap is on LIFETIME-CREATED, not currently-alive (legacy
	// CvGame::isUnitMaxedOut reads getUnitCreatedCount). The historic counter is the ENGINE's (getUnitCreatedCount,
	// persisted, one increment site, never decremented) -- this is the ONE located seam that reads it, so the
	// tally owns the JOB without duplicating the STORAGE. UNIT@world -> getUnitCreatedCount; everything else (incl.
	// persistent buildings, where alive == created) -> the normal count(). (UNITS have NO team cap -- owner ruling
	// 2026-06-17, folded to empire; a UNIT@empire cap reads the live count for now -- flag to revisit as
	// lifetime-created if a per-player "one ever" unique ever needs it.)
	int countForCap(CountDomain eDomain, int iType, CountScope eScope, int iContext) const;

	void shadowVerify() const; // per-domain, per-player diff vs getXCount; emit DIAGNOSTIC results

private:
	int  seededTruth(CountDomain eDomain, int iPlayer, int iType) const; // authoritative getXCount for a domain
	void rebuildDomain(CountDomain eDomain);
	void shadowDomain(CountDomain eDomain, int iShadowEventId) const;

	CvScopedAccumulator m_counts[NUM_COUNT_DOMAINS][MAX_PLAYERS]; // [domain][player] -> (type index -> count)
};

CvCascadeTally& cascadeTally();

#endif // CV_CASCADE_TALLY_H
