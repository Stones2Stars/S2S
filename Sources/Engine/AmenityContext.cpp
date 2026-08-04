//
//	AmenityContext -- the city's amenity state: storage, maintenance and its declared interest set, in ONE place
//	(see the header for the rulings).
//

#include "CvGameCoreDLL.h"
#include "AmenityContext.h"
#include "CityContext.h"
#include "Spine/CvEventSpine.h"
#include "CvCity.h"
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
	case SEVT_BUILDING_ACTIVATED:
	case SEVT_BUILDING_DORMANTED:
	// The EMPIRE leg -- a civic swap moves what the empire confers on every city; a capital move changes a
	// CONDITIONED grant's gate for two cities at once while the grantor set does not move at all.
	case SEVT_CIVIC_ADOPTED:
	case SEVT_CAPITAL_CHANGED:
	// A city CHANGED HANDS: its empire-conferred amenities belong to a different set of civics now, and the
	// grantor facts will never restate themselves for it.
	case SEVT_CITY_OWNER_CHANGED:
	// A city STARTED EXISTING: it folds what its owner already holds. The building half needs no pass (it is a
	// delta off the per-building crossings), but the civic facts fired before this city existed to fan to.
	case SEVT_CITY_FOUNDED:
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
	case SEVT_BUILDING_ACTIVATED:
	case SEVT_BUILDING_DORMANTED:
	{
		const CvCity* pCity = amenityCityFor(kEvent.iC, kEvent.iSrcLoc);
		if (pCity != NULL)
		{
			pCity->amenity().onGrantorCrossing(kEvent.iType,
				(kEvent.iEventId == SEVT_BUILDING_ACTIVATED) ? 1 : -1);
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
	// THE CAPITAL MOVED -- a conditioned grant's GATE flipped for two NAMED cities while the grantor set did not
	// move at all. iSrcLoc = the new capital, iA = the old. Exactly two cities are touched, never the empire.
	case SEVT_CAPITAL_CHANGED:
		if (!spineGameLoadInProgress() && kEvent.iC >= 0 && kEvent.iC < MAX_PLAYERS)
		{
			const CvCity* pOld = amenityCityFor(kEvent.iC, kEvent.iA);
			const CvCity* pNew = amenityCityFor(kEvent.iC, kEvent.iSrcLoc);
			if (pOld != NULL) pOld->amenity().foldGateFlip(kEvent.iC, CASC_PRED_IS_CAPITAL, -1);
			if (pNew != NULL) pNew->amenity().foldGateFlip(kEvent.iC, CASC_PRED_IS_CAPITAL, +1);
		}
		break;
	// A city CHANGED HANDS: its empire-conferred amenities belong to a different set of civics now. iA = the OLD
	// owner, iC = the new -- so the swap is derivable from the fact alone.
	case SEVT_CITY_OWNER_CHANGED:
		if (!spineGameLoadInProgress() && kEvent.iC >= 0 && kEvent.iC < MAX_PLAYERS)
		{
			const CvCity* pCity = amenityCityFor(kEvent.iC, kEvent.iSrcLoc);
			if (pCity != NULL)
			{
				pCity->amenity().foldAllCivicsOf(kEvent.iA, -1);
				pCity->amenity().foldAllCivicsOf(kEvent.iC, +1);
			}
		}
		break;
	// A city STARTED EXISTING: fold its owner's standing civics in, from zero.
	case SEVT_CITY_FOUNDED:
		if (!spineGameLoadInProgress() && kEvent.iC >= 0 && kEvent.iC < MAX_PLAYERS)
		{
			const CvCity* pCity = amenityCityFor(kEvent.iC, kEvent.iSrcLoc);
			if (pCity != NULL) pCity->amenity().foldAllCivicsOf(kEvent.iC, +1);
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
	// A crossing (0 <-> non-zero) is a genuine state change, so it ANNOUNCES -- a consumer routing on an amenity
	// must not have to re-derive which key moved. Only the crossing: a second grantor of an amenity the city
	// already holds changes no verdict, exactly as the counters this replaces behaved.
	const bool bHadBefore = has(iAmenityId);
	add(iAmenityId, iSign);
	if (m_city != NULL && bHadBefore != has(iAmenityId))
	{
		// Power is the one amenity whose fact is wired today; the other per-attribute facts (government centre,
		// fresh water) still ride their own counters and migrate onto this crossing as they convert.
		if (iAmenityId == CLS_AMENITY_PROVIDES_POWER)
		{
			if (iSign > 0)
	{
		emitCityPowerAdded(m_city->getID(), m_city->getOwner());
	}
	else
	{
		emitCityPowerRemoved(m_city->getID(), m_city->getOwner());
	}
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
