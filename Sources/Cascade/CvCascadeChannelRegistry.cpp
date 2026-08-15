//
//	CascadeChannelRegistry -- the minted channel vocabulary + per-scope channel sets (see the header).
//	Everything here is filled at load by the DepositIndex push and read by the package plane; append-only
//	across a readJson re-map (same keys re-register to the same ids).
//

#include "CvGameCoreDLL.h"
#include "CvCascadeChannelRegistry.h"
#include "CvModEntry.h"        // modSegmentSpell -- the kind member's authored spelling (naming only)
#include "CvPropertyInfo.h"    // the property plane's channel name (the PROPERTY_* type string)
#include "Spine/CvEventSpine.h"   // the [MODIFIER] domain -- the channel-set census render path
#include "Defines/CvGlobals.h"
#include "Engine/CvMap.h"      // cascadePlotCityFloor100 -- the plot fetch behind the live city-centre floor
#include "Engine/CvPlot.h"     // cascadePlotCityFloor100 -- isCity, the plot's own state
#include "CvYieldInfo.h"       // cascadePlotCityFloor100 -- the authored MinCity minimums
#include "CvInfoKinds.h"       // infoYieldFamily -- the yield -> channel family mapping
#include <map>
#include <string>
#include <vector>

namespace
{
	// One minted channel's identity + census flags. The name is resolved lazily (a property's type string is
	// not readable until its info is registered).
	struct CascadeChannelRecord
	{
		ModifierFamily family;
		int kind;
		int propertyFk;
		bool twin;            // a wellbeing sign twin (anger/unhealth) -- census-excluded
		int twinOf;           // the authored channel a twin shadows; -1 otherwise
		std::string name;
		CascadeChannelRecord() : family(MODFAM_NONE), kind(-1), propertyFk(-1), twin(false), twinOf(-1) {}
	};

	// The channel key -> id map (family, kind, propertyFk, twin) and the id -> record table. Append-only.
	struct CascadeChannelKey
	{
		int family;
		int kind;
		int propertyFk;
		bool twin;
		bool operator<(const CascadeChannelKey& other) const
		{
			if (family != other.family) return family < other.family;
			if (kind != other.kind) return kind < other.kind;
			if (propertyFk != other.propertyFk) return propertyFk < other.propertyFk;
			return twin < other.twin;
		}
	};
	std::map<CascadeChannelKey, int> s_channelIds;
	std::vector<CascadeChannelRecord> s_channels;

	// One scope's layout: the channel set (local slot order = first-sight order) + the receiver slots.
	struct CascadeScopeLayout
	{
		std::vector<int> channels;              // channel id per local slot
		std::map<int, int> channelSlots;        // channel id -> local slot (BUILD-time; never a read path)
		std::vector<int> receiverChannels;      // channel id per receiver slot
		std::map<int, int> receiverSlots;       // channel id -> receiver slot (BUILD-time; never a read path)

		// ⛔ THE READ PATH -- DENSE channel id -> slot, -1 absent. A package read must be a BARE FETCH
		// ([state-repositories.md]: "ONE read surface, and it is a bare fetch"), and the maps above are a
		// red-black tree WALK per lookup. They were on every readFlat/readPercent/readSum AND every apply --
		// the hottest reads in the engine -- which is a per-call calculation on a path specified to have none.
		// ⚑ Dense is exact here because channel ids are minted CONSECUTIVELY from 0, and it does NOT reopen
		// KEYS ONLY WHERE NEEDED: that ruling governs per-object STORAGE (the packages stay sparse), while this
		// is one shared per-SCOPE index -- 5 scopes x the channel count, not per city/plot.
		std::vector<int> slotByChannel;
		std::vector<int> receiverByChannel;
	};
	CascadeScopeLayout s_layouts[CASCADE_PACKAGE_SCOPES];

	bool cr_isPackageScope(CvCascScope eScope)
	{
		return (int)eScope >= (int)CASC_SCOPE_WORLD && (int)eScope < CASCADE_PACKAGE_SCOPES;
	}

	// Mint (or find) the channel id of a key; fills the record on first sight.
	int cr_mint(ModifierFamily eFamily, int iKind, int iPropertyFk, bool bTwin)
	{
		CascadeChannelKey key;
		key.family = (int)eFamily;
		key.kind = iKind;
		key.propertyFk = iPropertyFk;
		key.twin = bTwin;
		const std::map<CascadeChannelKey, int>::const_iterator found = s_channelIds.find(key);
		if (found != s_channelIds.end())
		{
			return found->second;
		}
		const int iChannel = (int)s_channels.size();
		s_channelIds.insert(std::make_pair(key, iChannel));
		s_channels.push_back(CascadeChannelRecord());
		CascadeChannelRecord& record = s_channels.back();
		record.family = eFamily;
		record.kind = iKind;
		record.propertyFk = iPropertyFk;
		record.twin = bTwin;
		return iChannel;
	}

	// Add a channel to a scope's set (idempotent; first sight assigns the local slot).
	void cr_addToScope(CvCascScope eScope, int iChannel)
	{
		if (!cr_isPackageScope(eScope) || iChannel < 0)
		{
			return;
		}
		CascadeScopeLayout& layout = s_layouts[(int)eScope];
		if (layout.channelSlots.find(iChannel) != layout.channelSlots.end())
		{
			return;
		}
		const int iSlot = (int)layout.channels.size();
		layout.channels.push_back(iChannel);
		layout.channelSlots.insert(std::make_pair(iChannel, iSlot));

		// Keep the dense read index in step at BUILD time, so the read never has to.
		if ((int)layout.slotByChannel.size() <= iChannel)
		{
			layout.slotByChannel.resize(iChannel + 1, -1);
		}
		layout.slotByChannel[iChannel] = iSlot;
	}

	// The spec'd receiver table (state-repositories.md: one consuming scope per channel -- food/production ->
	// city; gold/research/espionage/MAINTENANCE -> empire; CULTURE the lone dual-consumer; the commerce YIELD is
	// the city's slider input, consumed city-side with the other rates). Minted once, lazily -- the receiver
	// channels are the engine channels and exist regardless of which scopes author them.
	// ⚑ MAINTENANCE is a receiver like any other, and the reason is the RULING rather than the family: a
	// cross-scope receiver total is the Σ of its members' REALIZED values ([state-repositories.md]), and the
	// empire's maintenance is exactly that Σ over its cities. It is therefore the same cache holding a different
	// slot -- never a hand-named scalar beside the package, which is the shape
	// [DEC-uniform-cache-shape] calls a DEFECT precisely because it forces its own bespoke invalidation path.
	// ⚠ It is the FIRST non-commerce receiver, so it is also the first to exercise the gather's plain-rate
	// branch instead of the commerce split.
	void cr_addReceivers(CascadeScopeLayout& kLayout, const ModifierFamily* aFamilies, int iCount)
	{
		for (int iIndex = 0; iIndex < iCount; ++iIndex)
		{
			const int iChannel = cr_mint(aFamilies[iIndex], (int)CHANNEL_AMOUNT, -1, false);
			const int iReceiverSlot = (int)kLayout.receiverChannels.size();
			kLayout.receiverSlots.insert(std::make_pair(iChannel, iReceiverSlot));
			kLayout.receiverChannels.push_back(iChannel);

			// The dense read index, kept in step at BUILD time (see slotByChannel).
			if ((int)kLayout.receiverByChannel.size() <= iChannel)
			{
				kLayout.receiverByChannel.resize(iChannel + 1, -1);
			}
			kLayout.receiverByChannel[iChannel] = iReceiverSlot;
		}
	}

	bool s_bReceiversInitialized = false;
	void cr_initReceivers()
	{
		if (s_bReceiversInitialized)
		{
			return;
		}
		s_bReceiversInitialized = true;
		const ModifierFamily CITY_RECEIVES[] = { MODFAM_FOOD, MODFAM_PRODUCTION, MODFAM_COMMERCE, MODFAM_CULTURE };
		const ModifierFamily EMPIRE_RECEIVES[] = {
			MODFAM_GOLD, MODFAM_RESEARCH, MODFAM_CULTURE, MODFAM_ESPIONAGE, MODFAM_MAINTENANCE
		};
		cr_addReceivers(s_layouts[(int)CASC_SCOPE_CITY], CITY_RECEIVES, 4);
		cr_addReceivers(s_layouts[(int)CASC_SCOPE_EMPIRE], EMPIRE_RECEIVES, 5);
	}

	// Lazy name resolution (the property type string needs its registered info).
	const char* cr_resolveName(int iChannel)
	{
		CascadeChannelRecord& record = s_channels[iChannel];
		if (!record.name.empty())
		{
			return record.name.c_str();
		}
		if (record.twin)
		{
			record.name = (record.family == MODFAM_HAPPINESS) ? "anger" : "unhealth";
			return record.name.c_str();
		}
		if (record.family == MODFAM_PROPERTY)
		{
			if (record.propertyFk >= 0 && record.propertyFk < GC.getNumPropertyInfos())
			{
				record.name = GC.getPropertyInfo((PropertyTypes)record.propertyFk).getType();
			}
			else
			{
				record.name = "PROPERTY_?";
			}
			return record.name.c_str();
		}
		const char* szFamilyKey = infoFamilyKey(record.family);
		record.name = (szFamilyKey != NULL) ? szFamilyKey : "?";
		if (record.kind > 0)
		{
			// The kind's authored member spelling is not reverse-mapped by the vocabulary; the kind id is the
			// stable rendering ("defense#1" = the family's kind slot 1). Observability-only, never a read key.
			char szKind[16];
			_snprintf(szKind, sizeof(szKind), "#%d", record.kind);
			szKind[sizeof(szKind) - 1] = '\0';
			record.name += szKind;
		}
		return record.name.c_str();
	}
}

int CascadeChannelRegistry::registerDeposit(CvCascScope eScope, ModifierFamily eFamily, int iKind, int iPropertyFk)
{
	if (eFamily == MODFAM_NONE)
	{
		return -1;
	}
	if (eFamily != MODFAM_PROPERTY && iKind < 0)
	{
		return -1;   // an unkinded (batch-pending) member never slots; it stays visible in the kind-coverage census
	}
	cr_initReceivers();
	const int iChannel = cr_mint(eFamily, (eFamily == MODFAM_PROPERTY) ? 0 : iKind, iPropertyFk, false);
	cr_addToScope(eScope, iChannel);
	// The wellbeing sign twin rides its authored channel's scope set (modifier.md #2b: the split is a routing
	// rule at fill; the four channels are ordinary channels of the same package).
	if ((eFamily == MODFAM_HAPPINESS || eFamily == MODFAM_HEALTH) && iKind == (int)CHANNEL_AMOUNT)
	{
		const int iTwin = cr_mint(eFamily, iKind, iPropertyFk, true);
		s_channels[iTwin].twinOf = iChannel;
		cr_addToScope(eScope, iTwin);
	}
	return iChannel;
}

int CascadeChannelRegistry::channelLookup(ModifierFamily eFamily, int iKind, int iPropertyFk)
{
	CascadeChannelKey key;
	key.family = (int)eFamily;
	key.kind = (eFamily == MODFAM_PROPERTY) ? 0 : iKind;
	key.propertyFk = iPropertyFk;
	key.twin = false;
	const std::map<CascadeChannelKey, int>::const_iterator found = s_channelIds.find(key);
	return found == s_channelIds.end() ? -1 : found->second;
}

int CascadeChannelRegistry::wellbeingTwin(int iChannel)
{
	if (iChannel < 0 || iChannel >= (int)s_channels.size())
	{
		return -1;
	}
	const CascadeChannelRecord& record = s_channels[iChannel];
	if (record.twin || (record.family != MODFAM_HAPPINESS && record.family != MODFAM_HEALTH))
	{
		return -1;
	}
	CascadeChannelKey key;
	key.family = (int)record.family;
	key.kind = record.kind;
	key.propertyFk = record.propertyFk;
	key.twin = true;
	const std::map<CascadeChannelKey, int>::const_iterator found = s_channelIds.find(key);
	return found == s_channelIds.end() ? -1 : found->second;
}

bool CascadeChannelRegistry::isTwin(int iChannel)
{
	return iChannel >= 0 && iChannel < (int)s_channels.size() && s_channels[iChannel].twin;
}

const char* CascadeChannelRegistry::channelName(int iChannel)
{
	if (iChannel < 0 || iChannel >= (int)s_channels.size())
	{
		return "?";
	}
	return cr_resolveName(iChannel);
}

int CascadeChannelRegistry::channelCount()
{
	return (int)s_channels.size();
}

ModifierFamily CascadeChannelRegistry::channelFamily(int iChannel)
{
	return (iChannel >= 0 && iChannel < (int)s_channels.size()) ? s_channels[iChannel].family : MODFAM_NONE;
}

int CascadeChannelRegistry::channelKind(int iChannel)
{
	return (iChannel >= 0 && iChannel < (int)s_channels.size()) ? s_channels[iChannel].kind : -1;
}

int CascadeChannelRegistry::channelPropertyFk(int iChannel)
{
	return (iChannel >= 0 && iChannel < (int)s_channels.size()) ? s_channels[iChannel].propertyFk : -1;
}

int CascadeChannelRegistry::scopeChannelCount(CvCascScope eScope)
{
	return cr_isPackageScope(eScope) ? (int)s_layouts[(int)eScope].channels.size() : 0;
}

int CascadeChannelRegistry::scopeAuthoredChannelCount(CvCascScope eScope)
{
	if (!cr_isPackageScope(eScope))
	{
		return 0;
	}
	const CascadeScopeLayout& layout = s_layouts[(int)eScope];
	int iAuthored = 0;
	for (size_t iSlot = 0; iSlot < layout.channels.size(); ++iSlot)
	{
		if (!s_channels[layout.channels[iSlot]].twin)
		{
			++iAuthored;
		}
	}
	return iAuthored;
}

int CascadeChannelRegistry::scopeSlotIndex(CvCascScope eScope, int iChannel)
{
	if (!cr_isPackageScope(eScope))
	{
		return -1;
	}
	const CascadeScopeLayout& layout = s_layouts[(int)eScope];
	if (iChannel < 0 || iChannel >= (int)layout.slotByChannel.size())
	{
		return -1;
	}
	return layout.slotByChannel[iChannel];
}

int CascadeChannelRegistry::scopeSlotChannel(CvCascScope eScope, int iSlotIndex)
{
	if (!cr_isPackageScope(eScope))
	{
		return -1;
	}
	const CascadeScopeLayout& layout = s_layouts[(int)eScope];
	if (iSlotIndex < 0 || iSlotIndex >= (int)layout.channels.size())
	{
		return -1;
	}
	return layout.channels[iSlotIndex];
}

int CascadeChannelRegistry::scopeReceiverCount(CvCascScope eScope)
{
	if (!cr_isPackageScope(eScope))
	{
		return 0;
	}
	cr_initReceivers();
	return (int)s_layouts[(int)eScope].receiverChannels.size();
}

int CascadeChannelRegistry::scopeReceiverIndex(CvCascScope eScope, int iChannel)
{
	if (!cr_isPackageScope(eScope))
	{
		return -1;
	}
	cr_initReceivers();
	const CascadeScopeLayout& layout = s_layouts[(int)eScope];
	if (iChannel < 0 || iChannel >= (int)layout.receiverByChannel.size())
	{
		return -1;
	}
	return layout.receiverByChannel[iChannel];
}

int CascadeChannelRegistry::scopeReceiverChannel(CvCascScope eScope, int iReceiverIndex)
{
	if (!cr_isPackageScope(eScope))
	{
		return -1;
	}
	cr_initReceivers();
	const CascadeScopeLayout& layout = s_layouts[(int)eScope];
	if (iReceiverIndex < 0 || iReceiverIndex >= (int)layout.receiverChannels.size())
	{
		return -1;
	}
	return layout.receiverChannels[iReceiverIndex];
}

namespace
{
	// ---- the [MODIFIER] domain registration (event ids + field tags are DOMAIN-LOCAL) ----
	enum ModifierDomainEvent
	{
		MODEVT_CHANNEL_CENSUS = 1,  // the load-end per-scope channel-set census (KEYS ONLY WHERE NEEDED)
		MODEVT_PLOTS_FAN,           // one `plots`-target fan applied (the plot plane's only readback)
		MODEVT_GROWTH_READ,         // one city's growth threshold + consumption, term by term
		MODEVT_DEPOSIT_APPLY,       // ONE deposit landing in ONE slot: who, which channel, which package, how much
		MODEVT_RATE_READ,           // one city's §2a yield RATE, term by term (the six quantities behind one int)
		MODEVT_BONUS_STORES,        // one city's bonus stores, by size -- empty store vs refusing route
		MODEVT_ATOM_ROUTE,          // ONE atom crossing's outcome: found / noSource / refused / applied
		MODEVT_SPECIALIST_READ,     // ONE specialist TYPE's share of a rate's `specialists` term (the Σ decomposed)
		MODEVT_TRAIT_IMPROVEMENT_READ   // ONE (trait x improvement) keyed deposit's share of a rate's BASE
	};
	enum ModifierDomainField
	{
		MODF_SCOPE = 0,
		MODF_AUTHORED,
		MODF_SLOTS,
		MODF_RECEIVERS,
		MODF_SOURCE,
		MODF_ENTRIES,
		MODF_CITIES,
		MODF_PLOTS,
		MODF_RESOLVED,
		MODF_MULT,
		MODF_PLAYER, MODF_HUMAN, MODF_CITY, MODF_POP, MODF_FOOD, MODF_THRESHOLD, MODF_SPEEDPCT,
		MODF_ERAPCT, MODF_BASETHRESH, MODF_CONSUMPTION, MODF_PERPOP, MODF_FOODDIFF,
		MODF_DEFBASE, MODF_DEFMULT, MODF_NORMALAI, MODF_GOLDENAGE,
		MODF_CHANNEL, MODF_UNIT, MODF_VALUE, MODF_EVENT,
		MODF_PLOTBASE, MODF_TRADEYIELD, MODF_GOLDENYIELD, MODF_UPPERFLAT, MODF_SPECIALISTS,
		MODF_CITYFLAT, MODF_PERCENTSUM, MODF_WORKEDPLOTS, MODF_RATE,
		MODF_PLOTNATURE, MODF_PLOTIMPROVEMENT, MODF_PLOTREST,
		MODF_ONSITE, MODF_VICALL, MODF_VICOWNED, MODF_VICWORKED, MODF_TRADED, MODF_NETLIST,
		MODF_ATOM, MODF_LISTSIZE, MODF_FOUND, MODF_NOSOURCE, MODF_REFUSED, MODF_APPLIED,
		MODF_SPECIALIST, MODF_ASSIGNED, MODF_FREETYPED, MODF_PERUNIT, MODF_CONTRIB,
		MODF_TRAIT, MODF_IMPROVEMENT, MODF_TILES, MODF_PERTILE
	};

	const char* cr_modifierDomainPrefix(int iEventId)
	{
		switch (iEventId)
		{
		case MODEVT_CHANNEL_CENSUS: return "[MODIFIER] channels";
		case MODEVT_PLOTS_FAN:      return "[MODIFIER] plotsFan";
		case MODEVT_GROWTH_READ:    return "[MODIFIER] growthRead";
		case MODEVT_DEPOSIT_APPLY:  return "[MODIFIER] depositApply";
		case MODEVT_RATE_READ:      return "[MODIFIER] rateRead";
		case MODEVT_BONUS_STORES:   return "[MODIFIER] bonusStores";
		case MODEVT_ATOM_ROUTE:     return "[MODIFIER] atomRoute";
		case MODEVT_SPECIALIST_READ: return "[MODIFIER] specialistRead";
		case MODEVT_TRAIT_IMPROVEMENT_READ: return "[MODIFIER] traitImprovementRead";
		default:                    return "[MODIFIER] ?";
		}
	}

	const char* cr_modifierDomainFieldInfo(int iFieldTag, SpineFieldType* peType)
	{
		switch (iFieldTag)
		{
		case MODF_SCOPE:     *peType = SFT_STR; return "scope";
		case MODF_AUTHORED:  *peType = SFT_INT; return "authored";
		case MODF_SLOTS:     *peType = SFT_INT; return "slots";
		case MODF_RECEIVERS: *peType = SFT_INT; return "receivers";
		case MODF_SOURCE:    *peType = SFT_STR; return "source";
		case MODF_ENTRIES:   *peType = SFT_INT; return "entries";
		case MODF_CITIES:    *peType = SFT_INT; return "cities";
		case MODF_PLOTS:     *peType = SFT_INT; return "plots";
		case MODF_RESOLVED:  *peType = SFT_INT; return "resolved";
		case MODF_MULT:      *peType = SFT_INT; return "mult";
		case MODF_PLAYER:      *peType = SFT_INT; return "player";
		case MODF_HUMAN:       *peType = SFT_INT; return "human";
		case MODF_CITY:        *peType = SFT_INT; return "city";
		case MODF_POP:         *peType = SFT_INT; return "pop";
		case MODF_FOOD:        *peType = SFT_INT; return "food";
		case MODF_THRESHOLD:   *peType = SFT_INT; return "threshold";
		case MODF_SPEEDPCT:    *peType = SFT_INT; return "speedPct";
		case MODF_ERAPCT:      *peType = SFT_INT; return "eraPct";
		case MODF_BASETHRESH:  *peType = SFT_INT; return "baseThresh";
		case MODF_CONSUMPTION: *peType = SFT_INT; return "consumption";
		case MODF_PERPOP:      *peType = SFT_INT; return "perPop";
		case MODF_FOODDIFF:    *peType = SFT_INT; return "foodDiff";
		case MODF_DEFBASE:     *peType = SFT_INT; return "defBase";
		case MODF_DEFMULT:     *peType = SFT_INT; return "defMult";
		case MODF_NORMALAI:    *peType = SFT_INT; return "normalAI";
		case MODF_GOLDENAGE:   *peType = SFT_INT; return "goldenAge";
		case MODF_PLOTBASE:    *peType = SFT_INT; return "plotBase";
		case MODF_PLOTNATURE:      *peType = SFT_INT; return "plotNature";
		case MODF_PLOTIMPROVEMENT: *peType = SFT_INT; return "plotImprovement";
		case MODF_PLOTREST:        *peType = SFT_INT; return "plotRest";
		case MODF_ONSITE:          *peType = SFT_INT; return "onSite";
		case MODF_VICALL:          *peType = SFT_INT; return "vicinityAll";
		case MODF_VICOWNED:        *peType = SFT_INT; return "vicinityOwned";
		case MODF_VICWORKED:       *peType = SFT_INT; return "vicinityWorked";
		case MODF_TRADED:          *peType = SFT_INT; return "traded";
		case MODF_NETLIST:         *peType = SFT_INT; return "networkList";
		case MODF_ATOM:            *peType = SFT_STR; return "atom";
		case MODF_LISTSIZE:        *peType = SFT_INT; return "listSize";
		case MODF_FOUND:           *peType = SFT_INT; return "found";
		case MODF_NOSOURCE:        *peType = SFT_INT; return "noSource";
		case MODF_REFUSED:         *peType = SFT_INT; return "refused";
		case MODF_APPLIED:         *peType = SFT_INT; return "applied";
		case MODF_TRADEYIELD:  *peType = SFT_INT; return "trade";
		case MODF_GOLDENYIELD: *peType = SFT_INT; return "goldenAgeYield";
		case MODF_UPPERFLAT:   *peType = SFT_INT; return "upperFlat";
		case MODF_SPECIALISTS: *peType = SFT_INT; return "specialists";
		case MODF_CITYFLAT:    *peType = SFT_INT; return "cityFlatExtra";
		case MODF_PERCENTSUM:  *peType = SFT_INT; return "percentSum";
		case MODF_WORKEDPLOTS: *peType = SFT_INT; return "workedPlots";
		case MODF_RATE:        *peType = SFT_INT; return "rate";
		// SFT_SPECIALIST -- a TYPED INDEX, so the id travels raw and the RENDERER names it behind the gate
		// ([event-spine.md]: a payload carries typed fields and never a pre-resolved string).
		case MODF_SPECIALIST:  *peType = SFT_SPECIALIST; return "specialist";
		case MODF_ASSIGNED:    *peType = SFT_INT; return "assigned";
		case MODF_FREETYPED:   *peType = SFT_INT; return "freeTyped";
		case MODF_PERUNIT:     *peType = SFT_INT; return "perUnit";
		case MODF_CONTRIB:     *peType = SFT_INT; return "contribution";
		case MODF_TRAIT:       *peType = SFT_TRAIT; return "trait";
		case MODF_IMPROVEMENT: *peType = SFT_IMPROVEMENT; return "improvement";
		case MODF_TILES:       *peType = SFT_INT; return "workedTiles";
		case MODF_PERTILE:     *peType = SFT_INT; return "perTile";
		case MODF_CHANNEL:     *peType = SFT_STR; return "channel";
		case MODF_UNIT:        *peType = SFT_STR; return "unit";
		case MODF_VALUE:       *peType = SFT_INT; return "value";
		case MODF_EVENT:       *peType = SFT_STR; return "onFact";
		default:             *peType = SFT_INT; return NULL;
		}
	}

	bool s_bModifierDomainRegistered = false;
	void cr_ensureModifierDomain()
	{
		if (s_bModifierDomainRegistered)
		{
			return;
		}
		s_bModifierDomainRegistered = true;
		spineRegisterDomain(SD_MODIFIER, cr_modifierDomainPrefix, NULL, cr_modifierDomainFieldInfo);   // NULL => Cascade.log
	}

	const char* cr_scopeName(int iScope)
	{
		switch (iScope)
		{
		case CASC_SCOPE_WORLD:  return "world";
		case CASC_SCOPE_TEAM:   return "team";
		case CASC_SCOPE_EMPIRE: return "empire";
		case CASC_SCOPE_CITY:   return "city";
		case CASC_SCOPE_PLOT:   return "plot";
		default:                return "?";
		}
	}
}

void CascadeChannelRegistry::reportChannelCensus()
{
	static bool s_bReported = false;
	if (s_bReported)
	{
		return;
	}
	s_bReported = true;
	cr_ensureModifierDomain();
	// One line per package scope: the AUTHORED channel count (comparable to the state-repositories.md measured
	// expectation), the SLOTTED count (sign twins included), and the receiver slots. World reports too -- its
	// authored count is the mis-scoped-data census (world is CONFIG, no package).
	for (int iScope = 0; iScope < CASCADE_PACKAGE_SCOPES; ++iScope)
	{
		CvSpineEvent census(EVENTKIND_DIAGNOSTIC, SD_MODIFIER, MODEVT_CHANNEL_CENSUS, 1);
		census.addStr(MODF_SCOPE, cr_scopeName(iScope));
		census.addI(MODF_AUTHORED, scopeAuthoredChannelCount((CvCascScope)iScope));
		census.addI(MODF_SLOTS, scopeChannelCount((CvCascScope)iScope));
		census.addI(MODF_RECEIVERS, scopeReceiverCount((CvCascScope)iScope));
		eventSpine().emit(census);
	}
}

void CascadeChannelRegistry::reportPlotsFan(const char* szSource, CvCascScope eEntryScope,
	int iEntriesSelected, int iCitiesWalked, int iPlotsApplied,
	int iEntriesResolved, int iMultiplicity)
{
	cr_ensureModifierDomain();
	CvSpineEvent fan(EVENTKIND_DIAGNOSTIC, SD_MODIFIER, MODEVT_PLOTS_FAN, 2);
	fan.addStr(MODF_SOURCE, (szSource != NULL) ? szSource : "?");
	fan.addStr(MODF_SCOPE, cr_scopeName(eEntryScope));
	fan.addI(MODF_ENTRIES, iEntriesSelected);
	fan.addI(MODF_CITIES, iCitiesWalked);
	fan.addI(MODF_PLOTS, iPlotsApplied);
	fan.addI(MODF_RESOLVED, iEntriesResolved);
	fan.addI(MODF_MULT, iMultiplicity);
	eventSpine().emit(fan);
}

// ONE deposit landing in ONE slot. This is the ATTRIBUTION the totals cannot give: a package read says a channel
// holds N, and nothing anywhere says WHO put it there or how many times -- which is why a wrong total has only ever
// been answerable by hypothesis. Emitted per apply, so a grep over one load answers "who contributes to empire
// happiness, and how often" directly ([DEC-no-guessing]: at a gap, EMIT the decomposition rather than infer it).
// ⚠ DIAGNOSTIC by kind, so it rides gStreamLogLevel and NO consumer may build state from it
// ([event-spine.md] § THE RECEIVED LINE -- a line that says code ran is never a fact anything folds on).
// ⚑ Level 3 (the per-candidate tier): this is per-deposit and would drown a level-1 read.
// ⛔ THE PER-APPLY LINE IS A FIRE HAZARD AND IS NOT WHAT ANSWERS THE QUESTION. An apply happens per source per
// deposit per owner, which on a late-game load is millions of lines -- a log that large is not an instrument, and
// the question ("WHO contributes to this channel, and how many times") is an aggregate anyway.
// ⇒ So this ACCUMULATES per (source, channel, scope) and the census below emits ONE line per surviving entry.
// ⚑ That is the `[MODIFIER] channels` shape already in this file, and it keeps the output bounded by the number of
// SOURCES that actually deposited rather than by how often they did.
// ⚠ It is a DIAGNOSTIC accumulator for a census, NOT state: nothing reads it, nothing folds on it, and it is
// cleared each time it is reported ([event-spine.md] § THE RECEIVED LINE -- no consumer may build state from a
// line that says code ran).
namespace
{
	struct DepositTally
	{
		int iApplies;
		int64_t iTotal;
		DepositTally() : iApplies(0), iTotal(0) {}
	};
	// key: source|channel|scope|unit -- interned as a string because the census is cold-path by construction.
	std::map<std::string, DepositTally> s_depositTally;
}

void CascadeChannelRegistry::reportDepositApply(const char* szSource, int iChannel, CvCascScope eScope,
	bool bPercentSide, int64_t iValue, int iPlayer, int iCity, const char* szOnFact)
{
	std::string szKey((szSource != NULL) ? szSource : "?");
	szKey += "|";
	szKey += channelName(iChannel);
	szKey += "|";
	szKey += cr_scopeName(eScope);
	szKey += "|";
	szKey += (bPercentSide ? "percent" : "flat");
	szKey += "|";
	szKey += ((szOnFact != NULL) ? szOnFact : "?");
	DepositTally& kTally = s_depositTally[szKey];
	kTally.iApplies += 1;
	kTally.iTotal += iValue;
	(void)iPlayer;
	(void)iCity;
}

// The bounded decomposition: one line per (source, channel, scope, unit, driving fact) that actually deposited,
// carrying HOW MANY applies and their SUMMED value. A channel whose total is impossible against the authored data
// is then attributable to a NAMED source without reading any code -- which is the whole point ([DEC-no-guessing]:
// at a gap, EMIT the decomposition; a bare total supports neither VERIFY nor ASK).
void CascadeChannelRegistry::reportDepositCensus()
{
	cr_ensureModifierDomain();
	for (std::map<std::string, DepositTally>::const_iterator it = s_depositTally.begin();
		it != s_depositTally.end(); ++it)
	{
		const std::string& szKey = it->first;
		std::string::size_type a = szKey.find('|');
		std::string::size_type b = szKey.find('|', a + 1);
		std::string::size_type c = szKey.find('|', b + 1);
		std::string::size_type d = szKey.find('|', c + 1);
		if (a == std::string::npos || b == std::string::npos || c == std::string::npos || d == std::string::npos)
		{
			continue;
		}
		// ⛔ The segments MUST be named locals: addStr BORROWS the char* and the event is rendered at emit, so a
		// substr() temporary would be destroyed at the end of its own full-expression and the field would render
		// from freed memory. (It does not crash -- it renders EMPTY, which reads as "the census found nothing".)
		const std::string szSourceSeg  = szKey.substr(0, a);
		const std::string szChannelSeg = szKey.substr(a + 1, b - a - 1);
		const std::string szScopeSeg   = szKey.substr(b + 1, c - b - 1);
		const std::string szUnitSeg    = szKey.substr(c + 1, d - c - 1);
		const std::string szFactSeg    = szKey.substr(d + 1);
		CvSpineEvent dep(EVENTKIND_DIAGNOSTIC, SD_MODIFIER, MODEVT_DEPOSIT_APPLY, 2);
		dep.addStr(MODF_SOURCE, szSourceSeg.c_str());
		dep.addStr(MODF_CHANNEL, szChannelSeg.c_str());
		dep.addStr(MODF_SCOPE, szScopeSeg.c_str());
		dep.addStr(MODF_UNIT, szUnitSeg.c_str());
		dep.addStr(MODF_EVENT, szFactSeg.c_str());
		dep.addI(MODF_ENTRIES, it->second.iApplies);
		dep.addI(MODF_VALUE, (int)it->second.iTotal);
		eventSpine().emit(dep);
	}
	s_depositTally.clear();
}

void CascadeChannelRegistry::reportGrowthRead(int iPlayer, int iHuman, int iCity, int iPop, int iFood, int iThreshold,
	int iSpeedPercent, int iEraPercent, int iBaseThreshold,
	int iConsumption, int iPerPop, int iFoodDifference,
	int iDefineBase, int iDefineMult, int iNormalAI, int iGoldenAge)
{
	cr_ensureModifierDomain();
	CvSpineEvent growth(EVENTKIND_DIAGNOSTIC, SD_MODIFIER, MODEVT_GROWTH_READ, 2);
	growth.addI(MODF_PLAYER, iPlayer);
	growth.addI(MODF_HUMAN, iHuman);
	growth.addI(MODF_CITY, iCity);
	growth.addI(MODF_POP, iPop);
	growth.addI(MODF_FOOD, iFood);
	growth.addI(MODF_THRESHOLD, iThreshold);
	growth.addI(MODF_SPEEDPCT, iSpeedPercent);
	growth.addI(MODF_ERAPCT, iEraPercent);
	growth.addI(MODF_BASETHRESH, iBaseThreshold);
	growth.addI(MODF_CONSUMPTION, iConsumption);
	growth.addI(MODF_PERPOP, iPerPop);
	growth.addI(MODF_FOODDIFF, iFoodDifference);
	growth.addI(MODF_DEFBASE, iDefineBase);
	growth.addI(MODF_DEFMULT, iDefineMult);
	growth.addI(MODF_NORMALAI, iNormalAI);
	growth.addI(MODF_GOLDENAGE, iGoldenAge);
	eventSpine().emit(growth);
}

void CascadeChannelRegistry::reportAtomRoute(const char* szAtom, int iListSize, int iFound,
	int iNoSource, int iRefused, int iApplied, int iPlayer, int iCity)
{
	if (iListSize <= 0)
	{
		return;   // nothing anywhere is conditioned on this atom -- silence is the honest report
	}
	cr_ensureModifierDomain();
	CvSpineEvent route(EVENTKIND_DIAGNOSTIC, SD_MODIFIER, MODEVT_ATOM_ROUTE, 3);
	route.addStr(MODF_ATOM, (szAtom != NULL) ? szAtom : "?");
	route.addI(MODF_LISTSIZE, iListSize);
	route.addI(MODF_FOUND, iFound);
	route.addI(MODF_NOSOURCE, iNoSource);
	route.addI(MODF_REFUSED, iRefused);
	route.addI(MODF_APPLIED, iApplied);
	route.addI(MODF_PLAYER, iPlayer);
	route.addI(MODF_CITY, iCity);
	eventSpine().emit(route);
}

void CascadeChannelRegistry::reportBonusStores(int iPlayer, int iCity, int iOnSite, int iVicinityAll,
	int iVicinityOwned, int iVicinityWorked, int iTraded, int iNetworkList)
{
	cr_ensureModifierDomain();
	CvSpineEvent stores(EVENTKIND_DIAGNOSTIC, SD_MODIFIER, MODEVT_BONUS_STORES, 1);
	stores.addI(MODF_PLAYER, iPlayer);
	stores.addI(MODF_CITY, iCity);
	stores.addI(MODF_ONSITE, iOnSite);
	stores.addI(MODF_VICALL, iVicinityAll);
	stores.addI(MODF_VICOWNED, iVicinityOwned);
	stores.addI(MODF_VICWORKED, iVicinityWorked);
	stores.addI(MODF_TRADED, iTraded);
	stores.addI(MODF_NETLIST, iNetworkList);
	eventSpine().emit(stores);
}

void CascadeChannelRegistry::reportRateRead(int iPlayer, int iHuman, int iCity, int iChannelId,
	int iPlotBase, int iPlotNature, int iPlotImprovement, int iPlotRest,
	int iTradeYield, int iGoldenAgeYield, int iUpperFlat, int iSpecialists,
	int iCityFlatExtra, int iPercentSum, int iWorkedPlots, int iRate)
{
	cr_ensureModifierDomain();
	CvSpineEvent rate(EVENTKIND_DIAGNOSTIC, SD_MODIFIER, MODEVT_RATE_READ, 2);
	rate.addI(MODF_PLAYER, iPlayer);
	rate.addI(MODF_HUMAN, iHuman);
	rate.addI(MODF_CITY, iCity);
	// The channel id is carried RAW, exactly as the deposit census carries it -- a data-minted id is the cache's
	// internal key and is never resolved to a name at emit ([event-spine.md]: a payload carries typed fields and
	// never a pre-resolved string; the resolution is the RENDERER's, deferred behind the gate).
	rate.addI(MODF_VALUE, iChannelId);
	rate.addI(MODF_PLOTBASE, iPlotBase);
	rate.addI(MODF_PLOTNATURE, iPlotNature);
	rate.addI(MODF_PLOTIMPROVEMENT, iPlotImprovement);
	rate.addI(MODF_PLOTREST, iPlotRest);
	rate.addI(MODF_TRADEYIELD, iTradeYield);
	rate.addI(MODF_GOLDENYIELD, iGoldenAgeYield);
	rate.addI(MODF_UPPERFLAT, iUpperFlat);
	rate.addI(MODF_SPECIALISTS, iSpecialists);
	rate.addI(MODF_CITYFLAT, iCityFlatExtra);
	rate.addI(MODF_PERCENTSUM, iPercentSum);
	rate.addI(MODF_WORKEDPLOTS, iWorkedPlots);
	rate.addI(MODF_RATE, iRate);
	eventSpine().emit(rate);
}

// ⚠ rateRead above stands at EXACTLY SPINE_MAX_FIELDS, and `addI` past the cap DROPS THE FIELD SILENTLY -- so the
// specialist decomposition could not have ridden that line even if the per-TYPE axis had allowed it. It is its own
// line for the same reason `plotBase`'s segments are not: one row per type is a different shape from one term.
// ⚑ LEVEL 3 (the per-candidate tier, [observability.md]) -- rateRead is the per-decision line at 2, and this is
// its per-candidate detail, so it costs nothing until someone asks the question it answers.
void CascadeChannelRegistry::reportSpecialistRead(int iPlayer, int iCity, int iChannelId, int iSpecialist,
	int iAssigned, int iFreeTyped, int iPerUnit, int iContribution)
{
	cr_ensureModifierDomain();
	CvSpineEvent row(EVENTKIND_DIAGNOSTIC, SD_MODIFIER, MODEVT_SPECIALIST_READ, 3);
	row.addI(MODF_PLAYER, iPlayer);
	row.addI(MODF_CITY, iCity);
	// the channel id RAW, exactly as rateRead and the deposit census carry it
	row.addI(MODF_VALUE, iChannelId);
	row.addI(MODF_SPECIALIST, iSpecialist);
	row.addI(MODF_ASSIGNED, iAssigned);
	row.addI(MODF_FREETYPED, iFreeTyped);
	row.addI(MODF_PERUNIT, iPerUnit);
	row.addI(MODF_CONTRIB, iContribution);
	eventSpine().emit(row);
}

// ONE (trait x improvement) keyed deposit's share of a rate's BASE. Its OWN line rather than a rateRead field:
// the Σ carries axes the term cannot (WHICH trait, WHICH improvement), and rateRead is at the 16-field cap
// ([event-spine.md] -- a full line is answered by a SECOND event, never by swapping a term out).
// ⛔ WITHOUT THIS THE LEG IS INVISIBLE. It joins BASE after `plotBase` is captured, so it shows in no reported
// term and the terms silently stop summing to the rate -- a residual then has to be recovered by reconciling
// the whole combine by hand, which is exactly what a census exists to make unnecessary.
void CascadeChannelRegistry::reportTraitImprovementRead(int iPlayer, int iCity, int iChannelId, int iTrait,
	int iImprovement, int iWorkedTiles, int iPerTile, int iContribution)
{
	cr_ensureModifierDomain();
	CvSpineEvent row(EVENTKIND_DIAGNOSTIC, SD_MODIFIER, MODEVT_TRAIT_IMPROVEMENT_READ, 3);
	row.addI(MODF_PLAYER, iPlayer);
	row.addI(MODF_CITY, iCity);
	// the channel id RAW, exactly as rateRead and specialistRead carry it
	row.addI(MODF_VALUE, iChannelId);
	row.addI(MODF_TRAIT, iTrait);
	row.addI(MODF_IMPROVEMENT, iImprovement);
	row.addI(MODF_TILES, iWorkedTiles);
	row.addI(MODF_PERTILE, iPerTile);
	row.addI(MODF_CONTRIB, iContribution);
	eventSpine().emit(row);
}

// The channel -> engine yield resolve behind the two city-plot legs below. NO_YIELD for every non-yield channel.
static YieldTypes cr_yieldForChannel(int iChannel)
{
	for (int iYield = 0; iYield < NUM_YIELD_TYPES; ++iYield)
	{
		if (CascadeChannelRegistry::channelLookup(infoYieldFamily(iYield), (int)CHANNEL_AMOUNT, -1) == iChannel)
		{
			return (YieldTypes)iYield;
		}
	}
	return NO_YIELD;
}

// ⚖ THE CITY-PLOT ADD, read LIVE off the plot itself (owner: the city leg is the PLOT'S -- a plot knows it
// carries a city -- never an operand mirrored onto the package): the YieldInfo CityChange constant (the
// legacy calculateYield city block: food -1 / production +1 / commerce +1) plus population/divisor (food /5,
// production /2, commerce /4 -- integer division, the legacy step). Applied BEFORE the plot scaling, so the
// threshold plane tests the total the legacy engine tested. Answers 0 for every non-city plot.
// ⚠ The population half resolves the city OBJECT, which mid-map-read has not streamed yet -- it answers 0
// there, and the city's own in-read SEVT_CITY_POPULATION fact re-resolves this plot with the object in hand
// (the consumer's route), so the term lands exactly once the amount is knowable.
int64_t cascadePlotCityAdd100(int iX, int iY, int iChannel)
{
	const CvPlot* pPlot = GC.getMap().plot(iX, iY);
	if (pPlot == NULL || !pPlot->isCityDesignated())
	{
		return 0;
	}
	const YieldTypes eYield = cr_yieldForChannel(iChannel);
	if (eYield == NO_YIELD)
	{
		return 0;
	}
	const CvYieldInfo& kYield = GC.getYieldInfo(eYield);
	int iAdd = kYield.getCityChange();
	const CvCity* pPlotCity = pPlot->getPlotCity();
	if (pPlotCity != NULL && kYield.getPopulationChangeDivisor() != 0)
	{
		iAdd += pPlotCity->getPopulation() / kYield.getPopulationChangeDivisor();
	}
	return (int64_t)iAdd * 100;
}

// ⚖ THE CITY-CENTRE FLOOR input, read LIVE off the plot itself (the same ruling as the add above): a city
// plot's yield channels floor at the YieldInfo MinCity values (authored HUMAN; the package plane is x100).
// Applied LAST, after the scaling -- the legacy max(iYield, MinCity) position.
int64_t cascadePlotCityFloor100(int iX, int iY, int iChannel)
{
	// isCityDesignated, never isCity: the reseed's plot-city fact resolves this mid-map-read, when the city
	// OBJECT has not streamed yet -- the raw designation is the committed state the emit itself rode.
	const CvPlot* pPlot = GC.getMap().plot(iX, iY);
	if (pPlot == NULL || !pPlot->isCityDesignated())
	{
		return 0;
	}
	const YieldTypes eYield = cr_yieldForChannel(iChannel);
	if (eYield == NO_YIELD)
	{
		return 0;
	}
	return (int64_t)GC.getYieldInfo(eYield).getMinCity() * 100;
}
