//
//	AmenityContext -- the city's amenity state: storage, maintenance and its declared interest set, in ONE place
//	(see the header for the rulings).
//

#include "CvGameCoreDLL.h"
#include "AmenityContext.h"
#include "CityContext.h"
#include "Spine/CvEventSpine.h"
#include "CvCity.h"
#include "CvStatus.h"                          // CITYSTATUS_POWER_DISABLED -- the status this store's verdict is gated by
#include "AI/CvPlayerAI.h"                     // GET_PLAYER
#include "Infos/CvClassificationIds.h"         // CLS_AMENITY_* -- the generated ids the crossing tests
#include "Infos/CvClassificationRegistry.h"    // the load-minted amenity id registry (cachedKeyId)
#include "Infos/CvBuildingInfo.h"
#include "Infos/CvCivicInfo.h"
#include "Conditions/CvConditionEval.h"        // the ONE evaluator
#include "Conditions/CvConditionQuery.h"       // hasPredicate -- which entries an atom gates        // the ONE evaluator, for a CONDITIONED grant

namespace
{
	// Resolve a per-city fact's (owner, cityId) to the live city. Works during the load reseed too: the two-phase
	// city stream read registers a city in its owner's list off readIdentity, BEFORE its body streams its emits.
	const CvCity* amenityCityFor(int iOwner, int iCityId)
	{
		if (iOwner < 0 || iOwner >= MAX_PLAYERS || iCityId < 0)
		{
			return NULL;
		}
		return GET_PLAYER((PlayerTypes)iOwner).getCity(iCityId);
	}

	//	The AMENITY context's spine consumer. It owns no state -- the state is on the contexts; this only carries
	//	the dispatch, so "one place responsible" is not broken by the spine's need for a registerable object.
	class AmenitySpineConsumer : public IEventConsumer
	{
	public:
		int wantedKinds() const { return (1 << EVENTKIND_DOMAIN); }
		void onEvent(const CvSpineEvent& kEvent) { AmenityContext::onSpineEvent(kEvent); }
	};

	AmenitySpineConsumer s_amenityConsumer;
	bool s_bAmenityRegistered = false;
}

// ⚖ THE DECLARED INTEREST SET. Everything that maintains amenity state is named here, at the store.
bool AmenityContext::wantsEvent(int iEventId)
{
	switch (iEventId)
	{
	// The BUILDING leg -- the enabler's OPERATE crossing (never presence, never the processed notice).
	case SEVT_CITY_BUILDING_ACTIVATED:
	case SEVT_CITY_BUILDING_DORMANTED:
	// The EMPIRE leg -- a civic swap moves what the empire confers on every city; a capital move changes a
	// CONDITIONED grant's gate, one NAMED city per fact, while the grantor set does not move at all.
	case SEVT_CIVIC_ADOPTED:
	case SEVT_EMPIRE_CAPITAL_ADDED:
	case SEVT_EMPIRE_CAPITAL_REMOVED:
	// A city STARTED EXISTING UNDER AN OWNER -- founded or acquired. It folds what that owner already holds,
	// from zero; the grantor facts fired before this city existed to fan to and will never restate themselves.
	// ⛔ The city-FOUNDED fact is deliberately NOT here: `CvPlayer::found` announces ownership FIRST and then the
	// founding, so folding on both counted every civic-granted amenity twice -- which `has()` hides, and which
	// then leaves the amenity standing after its last grantor is gone.
	// ⛔ The owner-REMOVED half is deliberately NOT here either -- see the apply.
	case SEVT_CITY_OWNER_ADDED:
	// The EMPIRE-LEVEL building grantor leg (DEC-empire-level-buildings): a player-held member's amenities reach
	// every city of the owner, so its ACTIVE crossing fans exactly as a civic swap does (239 members author
	// amenities -- the power markers among them).
	case SEVT_EMPIRE_BUILDING_ACTIVATED:
	case SEVT_EMPIRE_BUILDING_DORMANTED:
	// ⚖ THE STATUS LEG -- a status is MIDDLEWARE that gates DELIVERY, so it moves no amenity count and is never
	// folded into the store. It reaches this context for ONE reason: the powered verdict it gates is what this
	// store announces, so a blackout starting or ending crosses that verdict with the grantor set untouched.
	// ⛔ It goes no further -- the status is never a cascade input, and nothing downstream routes on it.
	case SEVT_CITY_STATUS_ADDED:
	case SEVT_CITY_STATUS_REMOVED:
	// THE LOAD BUILD. The building half needs no pass -- it is a delta off the per-building crossings the save
	// read already emitted. The EMPIRE half does: the civic facts fired from CvPlayer::read, BEFORE the cities
	// deserialized, so there was no city to fan to. Every city folds its owner's standing civics once, here.
	case SEVT_GAME_LOAD_FINISHED:
		return true;
	default:
		return false;
	}
}

void AmenityContext::onSpineEvent(const CvSpineEvent& kEvent)
{
	if (!wantsEvent(kEvent.iEventId))
	{
		return;
	}
	switch (kEvent.iEventId)
	{
	case SEVT_CITY_BUILDING_ACTIVATED:
	case SEVT_CITY_BUILDING_DORMANTED:
	{
		const CvCity* pCity = amenityCityFor(kEvent.iC, kEvent.iSrcLoc);
		if (pCity != NULL)
		{
			pCity->amenity().onGrantorCrossing(kEvent.iType,
				(kEvent.iEventId == SEVT_CITY_BUILDING_ACTIVATED) ? 1 : -1);
		}
		break;
	}
	// A civic SWAP: -old / +new over every city of the empire. iType = adopted, iB = swapped out.
	// The fact carries BOTH ends, so the withdrawal needs no record of what was given.
	// Play-time ONLY: at load the civic facts fire from CvPlayer::read BEFORE the cities exist, so there is
	// nothing to fan to; the load build below folds them once the stream has ended.
	case SEVT_CIVIC_ADOPTED:
		if (!spineGameLoadInProgress() && kEvent.iC >= 0 && kEvent.iC < MAX_PLAYERS)
		{
			CvPlayer& kPlayer = GET_PLAYER((PlayerTypes)kEvent.iC);
			int iLoop = 0;
			for (const CvCity* pCity = kPlayer.firstCity(&iLoop); pCity != NULL; pCity = kPlayer.nextCity(&iLoop))
			{
				pCity->amenity().foldCivic(kEvent.iB, -1);      // the displaced civic
				pCity->amenity().foldCivic(kEvent.iType, +1);   // the adopted one
			}
		}
		break;
	// The EMPIRE-LEVEL building leg, play-time: the member's amenities fan over every city of the owner on its
	// ACTIVE crossing, the civic-swap shape. Load-time the player emits before the cities deserialize, so the
	// load build (and the city-starts-existing fold) carries that half.
	case SEVT_EMPIRE_BUILDING_ACTIVATED:
	case SEVT_EMPIRE_BUILDING_DORMANTED:
		if (!spineGameLoadInProgress() && kEvent.iC >= 0 && kEvent.iC < MAX_PLAYERS)
		{
			CvPlayer& kPlayer = GET_PLAYER((PlayerTypes)kEvent.iC);
			int iLoop = 0;
			for (const CvCity* pCity = kPlayer.firstCity(&iLoop); pCity != NULL; pCity = kPlayer.nextCity(&iLoop))
			{
				pCity->amenity().onGrantorCrossing(kEvent.iType,
					(kEvent.iEventId == SEVT_EMPIRE_BUILDING_ACTIVATED) ? 1 : -1);
			}
		}
		break;
	// THE CAPITAL MOVED -- a conditioned grant's GATE flipped while the grantor set did not move at all. Each
	// fact names ONE city (iSrcLoc) and its owner (iC), so a move is the REMOVED of the old beside the ADDED of
	// the new and neither side has to be recovered from a payload.
	case SEVT_EMPIRE_CAPITAL_ADDED:
	case SEVT_EMPIRE_CAPITAL_REMOVED:
		if (!spineGameLoadInProgress() && kEvent.iC >= 0 && kEvent.iC < MAX_PLAYERS)
		{
			const CvCity* pCity = amenityCityFor(kEvent.iC, kEvent.iSrcLoc);
			if (pCity != NULL)
			{
				pCity->amenity().foldGateFlip(kEvent.iC, CASC_PRED_IS_CAPITAL,
					(kEvent.iEventId == SEVT_EMPIRE_CAPITAL_ADDED) ? +1 : -1);
			}
		}
		break;
	// ⚖ A CITY STARTING TO EXIST UNDER AN OWNER IS THE *ONLY* CIVIC FOLD-IN, AND OWNER_ADDED IS THAT MOMENT.
	// It is announced for BOTH paths that produce one -- founding (`CvPlayer::found`) and acquisition
	// (`CvPlayer::acquireCity`) -- and in both the store has just been zeroed, so the fold is a delta from a
	// known zero ([DEC-keyed-accumulator]).
	// ⛔ THE WITHDRAWAL HALF IS DELIBERATELY ABSENT, and that is not an unrouted pair. `acquireCity` announces
	// the removal against the *NEW* city id (its own comment: "the surviving entity now under the new owner"),
	// so a `-1` there withdraws the OLD owner's civics from a store that never held them -- driving the count
	// negative and leaving an amenity BOTH empires grant reading false forever. The conquered city's real store
	// died with the old object. A withdrawal is only ever exact against the store that took the addition.
	case SEVT_CITY_OWNER_ADDED:
		if (!spineGameLoadInProgress() && kEvent.iC >= 0 && kEvent.iC < MAX_PLAYERS)
		{
			const CvCity* pCity = amenityCityFor(kEvent.iC, kEvent.iSrcLoc);
			if (pCity != NULL)
			{
				pCity->amenity().foldAllCivicsOf(kEvent.iC, +1);
				// ...and the owner's ACTIVE empire-level members (DEC-empire-level-buildings) -- the same
				// city-starts-existing leg, one grantor kind over.
				pCity->amenity().foldAllEmpireBuildingsOf(kEvent.iC, +1);
			}
		}
		break;
	// A STATUS GATED OR UNGATED DELIVERY. The store does not move -- the grantors are exactly as they were -- but
	// the verdict they feed does, so the crossing is announced here like any other.
	// ⚠ The status has ALREADY moved by the time its fact lands (CvCity::setStatus commits, then emits), so the
	// PRIOR verdict is reconstructed from the ungated source rather than re-read: a blackout STARTING was preceded
	// by whatever the source supplied, and a blackout ENDING was preceded by nothing being delivered at all.
	case SEVT_CITY_STATUS_ADDED:
	case SEVT_CITY_STATUS_REMOVED:
		if (kEvent.iType == (int)CITYSTATUS_POWER_DISABLED)
		{
			const CvCity* pCity = amenityCityFor(kEvent.iC, kEvent.iSrcLoc);
			if (pCity != NULL)
			{
				const bool bPoweredBefore = (kEvent.iEventId == SEVT_CITY_STATUS_ADDED)
					&& pCity->hasPowerSource();
				pCity->amenity().announcePowerCrossing(bPoweredBefore);
			}
		}
		break;
	// THE LOAD BUILD. The building half needs no pass -- it is a delta off the per-building crossings the save
	// read already emitted. The EMPIRE half does: the civic facts fired before the cities deserialized.
	case SEVT_GAME_LOAD_FINISHED:
		for (int iPlayer = 0; iPlayer < MAX_PLAYERS; ++iPlayer)
		{
			CvPlayer& kPlayer = GET_PLAYER((PlayerTypes)iPlayer);
			if (!kPlayer.isAlive()) continue;
			int iLoop = 0;
			for (const CvCity* pCity = kPlayer.firstCity(&iLoop); pCity != NULL; pCity = kPlayer.nextCity(&iLoop))
			{
				pCity->amenity().foldAllCivicsOf(iPlayer, +1);
				pCity->amenity().foldAllEmpireBuildingsOf(iPlayer, +1);   // the empire-building half of the same pass
			}
		}
		break;
	default:
		break;
	}
}

bool AmenityContext::hasKey(int& iIdCache, const char* szKey) const
{
	return has(ClassificationRegistry::cachedKeyId(iIdCache, CLSD_AMENITY, szKey));
}

// COUNTS, not bits: several grantors can confer the SAME amenity, so each contributes and a departure
// decrements -- losing one power plant must not darken a city that has two.
void AmenityContext::onGrantorCrossing(int iBuilding, int iSign)
{
	if (iBuilding < 0)
	{
		return;
	}
	foldBlock(GC.getBuildingInfo((BuildingTypes)iBuilding).getAmenities(), iSign);
}

void AmenityContext::foldCivic(int iCivic, int iSign)
{
	if (iCivic < 0)
	{
		return;
	}
	foldBlock(GC.getCivicInfo((CivicTypes)iCivic).getAmenities(), iSign);
}

// Reading the player's OWN adopted civics is a FORWARD of raw, object-owned O(1) state -- the HAVE axis every
// context forwards -- not the banned re-derivation, which is a store reading ANOTHER SYSTEM's built state.
void AmenityContext::foldAllCivicsOf(int iPlayer, int iSign)
{
	if (iPlayer < 0 || iPlayer >= MAX_PLAYERS)
	{
		return;
	}
	const CvPlayer& kPlayer = GET_PLAYER((PlayerTypes)iPlayer);
	const int iNumCivicOptions = GC.getNumCivicOptionInfos();
	for (int iCivicOption = 0; iCivicOption < iNumCivicOptions; ++iCivicOption)
	{
		foldCivic((int)kPlayer.getCivics((CivicOptionTypes)iCivicOption), iSign);
	}
}

// The empire-building fold-in (DEC-empire-level-buildings): the owner's ACTIVE empire-level members, folded like
// its civics -- reading the player's own held set is the ordinary HAVE forward, and only an ACTIVE member
// confers (the dormant firewall marker darkens nothing, exactly as a dormant building would).
void AmenityContext::foldAllEmpireBuildingsOf(int iPlayer, int iSign)
{
	if (iPlayer < 0 || iPlayer >= MAX_PLAYERS)
	{
		return;
	}
	const CvPlayer& kPlayer = GET_PLAYER((PlayerTypes)iPlayer);
	const std::vector<BuildingTypes>& kHeld = kPlayer.getHasBuildings();
	for (size_t iHeld = 0; iHeld < kHeld.size(); ++iHeld)
	{
		const BuildingTypes eBuilding = kHeld[iHeld];
		if (GC.getBuildingInfo(eBuilding).isEmpireLevel() && kPlayer.isEmpireBuildingActive(eBuilding))
		{
			onGrantorCrossing((int)eBuilding, iSign);
		}
	}
}

// The conditioned tail as a DELTA. The fact already decided the verdict and its direction, so the entries gated
// on that atom are applied WITHOUT evaluating it -- the only way the withdrawal can be right, since the atom has
// ALREADY moved by the time the fact arrives.
void AmenityContext::foldGateFlip(int iPlayer, CvCascPredKind ePredicate, int iSign)
{
	if (iPlayer < 0 || iPlayer >= MAX_PLAYERS)
	{
		return;
	}
	const CvPlayer& kPlayer = GET_PLAYER((PlayerTypes)iPlayer);
	const int iNumCivicOptions = GC.getNumCivicOptionInfos();
	for (int iCivicOption = 0; iCivicOption < iNumCivicOptions; ++iCivicOption)
	{
		const CivicTypes eCivic = kPlayer.getCivics((CivicOptionTypes)iCivicOption);
		if (eCivic == NO_CIVIC)
		{
			continue;
		}
		const CvClassificationBlock* pBlock = GC.getCivicInfo(eCivic).getAmenities();
		if (pBlock == NULL)
		{
			continue;
		}
		const std::vector<char>& kGranted = pBlock->grantedById();
		for (int iAmenityId = 0; iAmenityId < (int)kGranted.size(); ++iAmenityId)
		{
			if (kGranted[iAmenityId] == 0)
			{
				continue;
			}
			// ONLY the entries this atom actually gates. An unconditional grant does not move when a gate does,
			// and a grant gated on something ELSE is not this fact's business.
			const CvCondition* pGate = pBlock->conditionForId(iAmenityId);
			if (pGate == NULL || !CvConditionQuery::hasPredicate(pGate, ePredicate))
			{
				continue;
			}
			applyKey(iAmenityId, iSign);
		}
	}
}

// The ONE fold over any grantor's block. Evaluates a CONDITIONED grant against CURRENT state, which is exact
// because every gate is itself kept current by its own fact (the gate-flip route above).
void AmenityContext::foldBlock(const CvClassificationBlock* pBlock, int iSign)
{
	if (pBlock == NULL || iSign == 0)
	{
		return;
	}
	// Walk what the GRANTOR carries (the index IS the generated id), never every minted amenity id.
	const std::vector<char>& kGranted = pBlock->grantedById();
	CvCascadeEvalCtx ec;
	bool bCtxFilled = false;
	for (int iAmenityId = 0; iAmenityId < (int)kGranted.size(); ++iAmenityId)
	{
		if (kGranted[iAmenityId] == 0)
		{
			continue;
		}
		// Filling the ctx READS the sibling city context -- the shared evaluator seam, not a second home for this
		// state. Amenity storage and its maintenance both live here.
		const CvCondition* pGate = pBlock->conditionForId(iAmenityId);
		if (pGate != NULL)
		{
			if (!bCtxFilled)
			{
				if (m_city == NULL)
				{
					return;
				}
				m_city->getCityContext().fillEvalCtx(ec);
				bCtxFilled = true;
			}
			if (!cascadeEvalCondition(pGate, ec, CvCascadeEvalFlags()))
			{
				continue;
			}
		}
		applyKey(iAmenityId, iSign);
	}
}

// The single write point, so the CROSSING announcement exists exactly once.
void AmenityContext::applyKey(int iAmenityId, int iSign)
{
	// A crossing is a genuine state change, so it ANNOUNCES -- a consumer routing on an amenity must not have to
	// re-derive which key moved. Only the crossing: a second grantor of an amenity the city already holds changes
	// no verdict, exactly as the counters this replaces behaved.
	// ⛔ POWER announces the crossing of the GATED verdict (CvCity::isPowered), NEVER of the raw refcount. A status
	// is middleware between a source and its targets, so the two values genuinely differ: a plant completed during
	// a blackout moves the store while delivering nothing, and a blackout lifting delivers power while the store
	// stands still. Announcing the refcount would put the fact and every consumer's read on two different values,
	// leaving plane C holding deposits nothing withdraws ([DEC-maintained-sum]).
	const bool bIsPowerKey = (iAmenityId == CLS_AMENITY_PROVIDES_POWER);
	const bool bPoweredBefore = bIsPowerKey && m_city != NULL && m_city->isPowered();
	// ⚖ THE GOVERNMENT CENTRE is the same crossing one key over, and it is SIMPLER than power's: no status gates
	// its delivery, so the store's own 0 <-> non-zero verdict IS the fact and there is nothing to compose it with.
	// It is read exactly that way -- CvCity::isGovernmentCenter is a bare `has` on this key.
	const bool bIsGovernmentCenterKey = (iAmenityId == CLS_AMENITY_GOVERNMENT_CENTER);
	const bool bGovernmentCenterBefore = bIsGovernmentCenterKey && has(iAmenityId);
	// ⚖ FRESH WATER is the government centre's shape: nothing gates delivery, so this store's own 0 <-> non-zero
	// verdict IS the fact. It rode a serialized CvCity counter whose changer had lost every caller, so a provider
	// building granted its city nothing at all -- the grantor's own amenity is the feeder that was missing.
	const bool bIsFreshWaterKey = (iAmenityId == CLS_AMENITY_PROVIDES_FRESH_WATER);
	const bool bFreshWaterBefore = bIsFreshWaterKey && has(iAmenityId);
	add(iAmenityId, iSign);
	if (m_city != NULL)
	{
		if (bIsPowerKey)
		{
			announcePowerCrossing(bPoweredBefore);
		}
		else if (bIsGovernmentCenterKey)
		{
			announceGovernmentCenterCrossing(bGovernmentCenterBefore);
		}
		else if (bIsFreshWaterKey)
		{
			announceFreshWaterCrossing(bFreshWaterBefore);
		}
	}
}


// ⚖ THE ONE ANNOUNCEMENT POINT for the powered verdict. BOTH inputs that can move it -- a grantor starting or
// stopping, and the blackout status gating delivery -- resolve through CvCity::isPowered and compare against what
// held before, so neither leg has to know the other's, and no second expression of "powered" exists to drift.
void AmenityContext::announcePowerCrossing(bool bPoweredBefore)
{
	const bool bPoweredNow = m_city->isPowered();
	if (bPoweredBefore == bPoweredNow)
	{
		return;
	}
	if (bPoweredNow)
	{
		emitCityPowerAdded(m_city->getID(), m_city->getOwner());
	}
	else
	{
		emitCityPowerRemoved(m_city->getID(), m_city->getOwner());
	}
}


// ⚖ THE ONE ANNOUNCEMENT POINT for the government-centre verdict. The designation used to ride a hand-named
// CvCity counter whose changer announced this fact; the counter is gone and the verdict is now this store's
// `has` on one key, so the fact has to leave from HERE or it does not leave at all -- which is what left three
// consumers waiting on an event nothing emitted (the enabler's per-city gate re-check, the DISTANCE_TO_
// GOVERNMENT_CENTER refresh, and CityContext's interest set).
// ⛔ Unlike power there is NO gated read to compare against: no status sits between this store and its targets,
// so the refcount crossing IS the verdict crossing and composing one would invent a second definition.
void AmenityContext::announceGovernmentCenterCrossing(bool bWasGovernmentCenter)
{
	const bool bIsGovernmentCenter = has(CLS_AMENITY_GOVERNMENT_CENTER);
	if (bWasGovernmentCenter == bIsGovernmentCenter)
	{
		return;
	}
	if (bIsGovernmentCenter)
	{
		emitCityGovernmentCenterAdded(m_city->getID(), m_city->getOwner());
	}
	else
	{
		emitCityGovernmentCenterRemoved(m_city->getID(), m_city->getOwner());
	}
}


// ⚖ THE ONE ANNOUNCEMENT POINT for the fresh-water access verdict. The verdict used to live in a serialized
// CvCity counter fed by changeFreshWater -- which had no caller left, so `providesFreshWater` buildings conferred
// nothing and the crossing this store now announces could never happen. The refcount IS the verdict, exactly as
// the government centre's is: no status sits between a provider and the city.
// ⛔ Unlike the other two crossings this one owes DERIVED state -- plot irrigation and the city's fresh-water
// health both read the verdict -- so they refresh HERE, off the crossing, rather than at whatever moved it.
void AmenityContext::announceFreshWaterCrossing(bool bHadFreshWater)
{
	const bool bHasFreshWater = has(CLS_AMENITY_PROVIDES_FRESH_WATER);
	if (bHadFreshWater == bHasFreshWater)
	{
		return;
	}
	const int iCityId = m_city->getID();
	const PlayerTypes eOwner = m_city->getOwner();
	if (bHasFreshWater)
	{
		emitCityFreshWaterAdded(iCityId, eOwner, count(CLS_AMENITY_PROVIDES_FRESH_WATER));
	}
	else
	{
		emitCityFreshWaterRemoved(iCityId, eOwner, count(CLS_AMENITY_PROVIDES_FRESH_WATER));
	}
	if (eOwner != NO_PLAYER)
	{
		CvCity* pMutableCity = GET_PLAYER(eOwner).getCity(iCityId);
		if (pMutableCity != NULL)
		{
			pMutableCity->refreshFreshWaterDerived();
		}
	}
}

void amenityContextRegisterConsumer()
{
	if (s_bAmenityRegistered)
	{
		return;
	}
	s_bAmenityRegistered = true;
	eventSpine().registerConsumer(&s_amenityConsumer);
}
