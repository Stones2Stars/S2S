#pragma once
#ifndef CV_CASCADE_SLOT_VALUES_H
#define CV_CASCADE_SLOT_VALUES_H

//
//	CvCascadeSlotValues -- ONE scoped object's cascade slot values as a PLAIN DOCUMENT, the shape BOTH
//	endpoint-facing reads answer in: the STORED values the events built
//	(CvCascadePackage::readValuesInto), DECOMPOSED term by term so a reader can attribute a value to a NAMED
//	(scope, channel, owner). Nothing here compares, reports or repairs anything -- a divergence has no in-DLL
//	representation at all.
//
//	⛔ NOT a cache and NOT storage: a caller-owned buffer, copyable by value, holding no owner pointer. Anything
//	that computes into one of these instead of into the stored package structurally cannot repair the package
//	([DEC-no-self-heal]).
//
//	THE IDENTITY is TWO ints INTERPRETED PER SCOPE, exactly as the spine's DOMAIN ints are interpreted per
//	event (the scope owners share no common id accessor, so identity is passed IN at bind):
//	   city   -> (owner player, city id)      empire -> (player id, -1)        team -> (-1, team id)
//	A divergence naming only its scope and channel says a missed emit exists somewhere among every city on the
//	map without naming which one, which is not actionable.
//

#include "CvCascadeChannelRegistry.h"
#include <vector>

struct CvCascadeSlotValues
{
	CvCascScope scope;
	int identityFirst;                 // per scope: city owner | player id | -1 | plot x
	int identitySecond;                // per scope: city id | -1 | team id | plot y
	// Mirrors the package's per-unit widths exactly (CvCascadePackage.h): amounts 64, percents 32. The two
	// served documents are diffed field by field, so a width that differed here would silently truncate one
	// side of the comparison.
	std::vector<int64_t> flat;         // dictionary 1, by the scope's LOCAL slot index: the x100 flat sums
	std::vector<int> percent;          // dictionary 2, same indexing: the percent sums
	std::vector<int64_t> sum;          // the receiver slots: the realized x100 totals this scope consumes
	// The SUBSTRATE segment of `flat` (CvCascadePackage.h) -- carried here for the same reason as every other
	// dictionary: a segment the stored side serves but the document does not would be invisible to the diff.
	std::vector<int64_t> substrateFlat;

	CvCascadeSlotValues() : scope(CASC_SCOPE_CITY), identityFirst(-1), identitySecond(-1) {}

	// Read one CHANNEL's value out of this document (the local slot index is the scope's own; a channel this
	// scope never carries answers 0, exactly as a package read would).
	int64_t flatForChannel(int iChannel) const
	{
		const int iSlot = CascadeChannelRegistry::scopeSlotIndex(scope, iChannel);
		if (iSlot < 0 || iSlot >= (int)flat.size())
		{
			return 0;
		}
		return flat[iSlot];
	}

	int percentForChannel(int iChannel) const
	{
		const int iSlot = CascadeChannelRegistry::scopeSlotIndex(scope, iChannel);
		if (iSlot < 0 || iSlot >= (int)percent.size())
		{
			return 0;
		}
		return percent[iSlot];
	}

	// Size to the scope's CURRENT registry layout and zero-fill, so a slot no source fills still answers (as 0)
	// on both sides and the two documents stay index-aligned.
	void reset(CvCascScope eScope, int iIdentityFirst, int iIdentitySecond)
	{
		scope = eScope;
		identityFirst = iIdentityFirst;
		identitySecond = iIdentitySecond;
		const size_t iChannels = (size_t)CascadeChannelRegistry::scopeChannelCount(eScope);
		const size_t iReceivers = (size_t)CascadeChannelRegistry::scopeReceiverCount(eScope);
		flat.assign(iChannels, 0);
		percent.assign(iChannels, 0);
		sum.assign(iReceivers, 0);
		substrateFlat.assign(eScope == CASC_SCOPE_PLOT ? iChannels : 0, 0);
	}
};

#endif // CV_CASCADE_SLOT_VALUES_H
