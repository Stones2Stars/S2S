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

	// Emit ONE `plots`-target fan ([MODIFIER] plotsFan source=... scope=... entries=... cities=... plots=...).
	// A plural-target deposit lands in each PLOT's own package (modifier.md §5), and NO served surface carries a
	// plot package -- the cache documents stop at city scope -- so the fan is the one apply path whose result
	// cannot be read back at all. That makes it unverifiable rather than merely unlogged
	// ([validation.md]: a value not on the surface is not verifiable, and emitting it is step one of its fix).
	// DIAGNOSTIC by kind: it says an apply RAN, never what the state IS, so nothing may build state from it
	// ([event-spine.md] § THE RECEIVED LINE).
	static void reportPlotsFan(const char* szSource, CvCascScope eEntryScope,
		int iEntriesSelected, int iCitiesWalked, int iPlotsApplied,
		int iEntriesResolved, int iMultiplicity);

	// ONE city's growth arithmetic, decomposed into every term of both quantities. The threshold and the
	// consumption are the two numbers a city grows on, and NEITHER is on any served surface -- so a report of
	// "cities need less food than they used to" has no way to be attributed to a term. DIAGNOSTIC: it says what
	// a calculation produced, never what the state IS.
	// ONE deposit landing in ONE slot -- the per-source ATTRIBUTION a package total cannot give. DIAGNOSTIC,
	// level 3, so it costs nothing until asked for. szOnFact names the spine fact that drove the apply, which is
	// what turns "this value is too high" into "this source applied N times, on this fact".
	static void reportDepositApply(const char* szSource, int iChannel, CvCascScope eScope,
		bool bPercentSide, int64_t iValue, int iPlayer, int iCity, const char* szOnFact);
	// The bounded per-source decomposition of everything reportDepositApply accumulated: ONE line per
	// (source, channel, scope, unit, driving fact) with its APPLY COUNT and SUMMED value. Reported at load end.
	static void reportDepositCensus();
	static void reportGrowthRead(int iPlayer, int iHuman, int iCity, int iPop, int iFood, int iThreshold,
		int iSpeedPercent, int iEraPercent, int iBaseThreshold,
		int iConsumption, int iPerPop, int iFoodDifference,
		int iDefineBase, int iDefineMult, int iNormalAI, int iGoldenAge);
	// ONE city's §2a yield RATE, decomposed into every term the combine actually used. The rate is SIX
	// independent quantities collapsed into one int, so a report that a city produces too little is unanswerable
	// against the total -- this is what turns it into "the plot base is short" or "the stack is missing".
	// DIAGNOSTIC: it says what a calculation produced, never what the state IS.
	// ⚑ plotBase carries its THREE SEGMENTS beside it (nature / improvement / rest, raw and unfloored). A short
	// plot Σ is the one term the total cannot attribute on its own -- a dead improvement leg and a dead nature
	// leg are the same number in it -- and the segments are read from the SAME walk, never re-derived.
	// ONE city's BONUS STORES, by size. The vicinity/onSite dictionaries and the network's own holdings decide
	// every bonus-gated deposit in the game and appear on NO served surface, so an empty store and a working
	// store that everything legitimately refuses look identical from the outside -- a short yield either way.
	// This is the line that tells them apart. DIAGNOSTIC: what the stores HOLD, never what they should hold.
	// ONE atom crossing's OUTCOME. listSize/found say the index answered; noSource/refused/applied say what
	// happened to each deposit. ⛔ A route that fires and moves nothing is indistinguishable from one that never
	// fired, and that ambiguity is what makes a coverage claim unfalsifiable -- this is the line that makes
	// "does the cascade cover this atom" a number instead of an assertion.
	static void reportAtomRoute(const char* szAtom, int iListSize, int iFound, int iNoSource,
		int iRefused, int iApplied, int iPlayer, int iCity);
	static void reportBonusStores(int iPlayer, int iCity, int iOnSite, int iVicinityAll, int iVicinityOwned,
		int iVicinityWorked, int iTraded, int iNetworkList);
	static void reportRateRead(int iPlayer, int iHuman, int iCity, int iChannelId,
		int iPlotBase, int iPlotNature, int iPlotImprovement, int iPlotRest,
		int iTradeYield, int iGoldenAgeYield, int iUpperFlat, int iSpecialists,
		int iCityFlatExtra, int iPercentSum, int iWorkedPlots, int iRate);
};

#endif // CV_CASCADE_CHANNEL_REGISTRY_H
