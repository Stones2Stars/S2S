#pragma once
#ifndef CV_CASCADE_CHANNEL_REGISTRY_H
#define CV_CASCADE_CHANNEL_REGISTRY_H

//
//	CascadeChannelRegistry -- the DATA-DERIVED channel vocabulary + the per-scope channel sets of the modifier
//	cascade's value-cache plane (state-repositories.md, the per-scope package model).
//
//	A CHANNEL is one modifiable number: a (ModifierFamily, kind) slot of the closed CvInfoKinds vocabulary, or
//	one PROPERTY_* info on the open property plane. Channel ids are MINTED AT LOAD from the compiled deposits
//	(the ClassificationRegistry precedent -- KEYS ONLY WHERE NEEDED, never a hand-listed dense index): the
//	DepositIndex push registers every compiled entry's (scope, family, kind, propertyFk, unit) here, so after
//	load each scope's channel set is exactly what the data authors AT that scope and nothing else.
//
//	THE PER-SCOPE LAYOUT is the storage/bit contract of the ONE uniform package (CvCascadePackage): a scope's
//	channels get LOCAL slot indices in first-sight order; the package's dirty bit for a channel IS its local
//	index; RECEIVER sum slots (the realized totals the scope CONSUMES -- [DEC-uniform-cache-shape]: a receiver
//	is the same cache holding a different slot) take a FIXED region at the TOP of the 64-bit space (bits
//	58..62; bit 63 is the over-budget tripwire). The contract is ORDER-INDEPENDENT BY CONSTRUCTION: channel
//	slots are append-only (an assigned index never moves) and receiver bits are position constants, so a mask
//	computed or applied at ANY point of the load stays valid across later minting -- a receiver bit can never
//	come to denote a channel slot. City and empire exceed 32 channels, so every bit space is 64-bit (int64_t).
//
//	The WELLBEING SIGN TWINS: happiness/health are the only AUTHORED wellbeing families; anger/unhealth are
//	minted BESIDE them as sign twins (modifier.md #2b -- a negative deposit routes to the opposing channel at
//	fill; four ordinary channels, no polarity storage). Twins are flagged so the authored-channel census stays
//	comparable to the measured per-scope counts (plot 13 / city 40 / empire 50 / team 3).
//
//	APPEND-ONLY, like the DepositIndex interner: channel ids and local slot indices survive a readJson re-map
//	(the re-push re-registers the same keys to the same ids). Purely-organizational static-methods class
//	(patterns.md static-class law): no data members, never instantiated. Game-thread only.
//

// The windef.h min/max function-macros are live until Engine/CvGameCoreUtils.h's #undef (boost includes
// windows.h BEFORE the PCH's NOMINMAX). This header is pulled into that window through CvMap.h -> CvArea.h,
// and CvCondition.h carries `min`/`max` MEMBERS -- kill the macros here exactly as CvGameCoreUtils.h does
// (they are dead in engine code by standing rule).
#undef min
#undef max

#include "CvInfoKinds.h"   // ModifierFamily / CvCascUnit
#include "CvCondition.h"   // CvCascScope

// The containment-spine scopes that carry a package (state-repositories.md: world = CONFIG, no package --
// tracked for the mis-scoped-data census only; unit = resolved values, no package; self = off-spine).
// Indexed by CvCascScope directly (WORLD..PLOT are the first six values).
enum { CASCADE_PACKAGE_SCOPES = (int)CASC_SCOPE_PLOT + 1 };

class CascadeChannelRegistry
{
public:
	// ---- the load-time REGISTRATION side (called by the DepositIndex push, once per compiled record) ----

	// Mint (assign-on-first-sight) the channel of a (family, kind[, property]) slot and add it to eScope's
	// channel set. Returns the channel id; -1 when the address is not a package channel (kind outside the
	// vocabulary and not the property plane -- the batch-pending unkinded members flow through unslotted).
	// Wellbeing families additionally mint their sign twin into the same scope set.
	static int registerDeposit(CvCascScope eScope, ModifierFamily eFamily, int iKind, int iPropertyFk);

	// ---- the channel identity ----

	// Lookup WITHOUT minting. -1 = never authored anywhere.
	static int channelLookup(ModifierFamily eFamily, int iKind, int iPropertyFk);
	// The wellbeing sign twin of a channel (happiness->anger, health->unhealth); -1 for every other channel.
	static int wellbeingTwin(int iChannel);
	// Is this channel a minted sign twin (excluded from the authored-channel census)?
	static bool isTwin(int iChannel);
	// The channel's spell-back name ("food", "defense.bombardDefense", "PROPERTY_CRIME", "anger") -- the
	// [CASCADE]/[MODIFIER] observability rendering; never a runtime read.
	static const char* channelName(int iChannel);
	static int channelCount();
	// The channel's identity axes (for a gather that maps a channel back to its (family, kind) slot).
	static ModifierFamily channelFamily(int iChannel);
	static int channelKind(int iChannel);
	static int channelPropertyFk(int iChannel);

	// ---- the per-scope layout (the package storage/bit contract) ----

	// The scope's channel-set size (package slots; sign twins included).
	static int scopeChannelCount(CvCascScope eScope);
	// The scope's AUTHORED channel count (twins excluded) -- the census figure comparable to the measured
	// plot 13 / city 40 / empire 50 / team 3.
	static int scopeAuthoredChannelCount(CvCascScope eScope);
	// A channel's local slot index at a scope; -1 = not authored at that scope (no storage anywhere).
	static int scopeSlotIndex(CvCascScope eScope, int iChannel);
	// The channel id occupying a local slot; -1 = out of range.
	static int scopeSlotChannel(CvCascScope eScope, int iSlotIndex);
	// The package dirty bit of a channel at a scope (1 << local index); 0 = not authored there.
	static int64_t scopeChannelBit(CvCascScope eScope, int iChannel);
	// The whole scope's package-channel mask (every authored channel bit; receiver bits excluded).
	static int64_t scopeAllChannelsMask(CvCascScope eScope);
	// The OR of ONE FAMILY's channel bits at a scope -- so a consumer can ask "did this rebuild touch my
	// family here?" against the SAME index-derived mask the rebuild was routed by, never a hand-kept list
	// ([state-repositories.md]: the dirty flags fall out of the deposit addresses).
	static int64_t scopeFamilyMask(CvCascScope eScope, ModifierFamily eFamily);

	// ---- the RECEIVER slots (one consuming scope per channel; culture the lone dual-consumer) ----

	// How many receiver sum slots eScope carries (city: food/production/commerce/culture; empire:
	// gold/research/culture/espionage/maintenance; every other scope: 0). MAINTENANCE is the one
	// NON-commerce receiver: the empire's total is the Σ of its cities' realized maintenance, which is what a
	// cross-scope receiver total IS ([state-repositories.md]) -- so it is a slot here rather than a hand-named
	// cache beside the package ([DEC-uniform-cache-shape]).
	static int scopeReceiverCount(CvCascScope eScope);
	// The receiver slot of a channel at its consuming scope; -1 = eScope does not consume iChannel.
	static int scopeReceiverIndex(CvCascScope eScope, int iChannel);
	// The channel a receiver slot realizes; -1 = out of range.
	static int scopeReceiverChannel(CvCascScope eScope, int iReceiverIndex);
	// The receiver sum's dirty bit at its consuming scope (a FIXED top-region position, 1 << (58 +
	// receiverIndex) -- independent of the scope's channel count); 0 = none.
	static int64_t scopeReceiverBit(CvCascScope eScope, int iChannel);
	// The whole scope's receiver-bit mask.
	static int64_t scopeAllReceiversMask(CvCascScope eScope);
	// The receiver bits at eReceiverScope fed by the channels AUTHORED at eSourceScope -- DERIVED from the
	// minted per-scope channel sets (the data), never hand-listed: the mark mask for an event that re-bases
	// a whole source scope (a plot substrate/owner change, a plot moving between cities) names exactly the
	// realized sums the source scope's authored channels feed.
	static int64_t scopeReceiversFedBy(CvCascScope eReceiverScope, CvCascScope eSourceScope);

	// Decode a scope's dirty mask to a "|"-joined channel-name string (receiver bits render as "sum:<name>")
	// -- the [CASCADE] invalidate/rebuilt observability's package-name decode.
	static void decodeMask(CvCascScope eScope, int64_t iMask, char* szOut, int iOutSize);

	// ---- the minted layout's observability ----

	// Emit the per-scope CHANNEL-SET census ([MODIFIER] channels scope=... authored=... slots=... receivers=...)
	// -- the KEYS-ONLY-WHERE-NEEDED derivation made observable (the measured expectation: plot 13 / city 40 /
	// empire 50 / team 3 authored channels). Reports what THIS registry minted, so it can only run once
	// the load has pushed every compiled deposit: fired at GAME_LOAD_FINISHED, guarded to once per load.
	static void reportChannelCensus();
};

#endif // CV_CASCADE_CHANNEL_REGISTRY_H
