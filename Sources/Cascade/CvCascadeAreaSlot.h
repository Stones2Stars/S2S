#pragma once
#ifndef CV_CASCADE_AREA_SLOT_H
#define CV_CASCADE_AREA_SLOT_H

//
//	CvCascadeAreaSlot -- the (area x player) owner of the AREA-scope package. Area effects realize per player
//	(an area yield modifier applies to ONE player's cities in the area -- the legacy per-player CvArea arrays'
//	semantic), so the scoped item carrying the area package is the (area, player) pair: CvArea grafts one slot
//	per player, each holding the SAME uniform package type as every other scope ([DEC-uniform-cache-shape] --
//	the sameness is the type + the protocol; the owner axis is this slot). Three channels measured at area
//	scope -- as a bespoke struct each scope was a project, as keyed slots it is trivial
//	(state-repositories.md).
//
//	The refresh delegate lives here (the CvDerivedCacheSet member-function contract needs the owner type);
//	its body is the one-line delegation to CascadeGather::refreshArea (CvCascadeGather.cpp).
//

#include "CvCascadePackage.h"

class CvArea;

struct CvCascadeAreaSlot
{
	const CvArea* area;
	PlayerTypes player;
	CvCascadePackage<CvCascadeAreaSlot> package;

	CvCascadeAreaSlot() : area(NULL), player(NO_PLAYER) {}

	// iAreaId is passed IN, never read off pArea: CvArea is only forward-declared here (CvArea.h includes this
	// header, so completing the type would be circular). With the player it is the SERVED identity of this
	// (area x player) slot -- a served value naming neither would not say which slot it came from.
	void bind(const CvArea* pArea, PlayerTypes eSlotPlayer, int iAreaId)
	{
		area = pArea;
		player = eSlotPlayer;
		package.bind(CASC_SCOPE_AREA, this, &CvCascadeAreaSlot::refreshCascadePackage, (int)eSlotPlayer, iAreaId);
	}

	// The refresh delegate -- defined beside the gather (CvCascadeGather.cpp).
	void refreshCascadePackage(int64_t iMask) const;

private:
	CvCascadeAreaSlot(const CvCascadeAreaSlot&);              // noncopyable (the package binds this slot)
	CvCascadeAreaSlot& operator=(const CvCascadeAreaSlot&);
};

#endif // CV_CASCADE_AREA_SLOT_H
