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

	void bind(const CvArea* pArea, PlayerTypes eSlotPlayer)
	{
		area = pArea;
		player = eSlotPlayer;
		package.bind(CASC_SCOPE_AREA, this, &CvCascadeAreaSlot::refreshCascadePackage);
	}

	// The refresh delegate -- defined beside the gather (CvCascadeGather.cpp).
	void refreshCascadePackage(int64_t iMask) const;

private:
	CvCascadeAreaSlot(const CvCascadeAreaSlot&);              // noncopyable (the package binds this slot)
	CvCascadeAreaSlot& operator=(const CvCascadeAreaSlot&);
};

#endif // CV_CASCADE_AREA_SLOT_H
