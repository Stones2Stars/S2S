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
		std::map<int, int> channelSlots;        // channel id -> local slot
		std::vector<int> receiverChannels;      // channel id per receiver slot
		std::map<int, int> receiverSlots;       // channel id -> receiver slot
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
	}

	// The spec'd receiver table (state-repositories.md: one consuming scope per channel -- food/production ->
	// city; gold/research/espionage -> empire; CULTURE the lone dual-consumer; the commerce YIELD is the city's
	// slider input, consumed city-side with the other rates). Minted once, lazily -- the receiver channels are
	// the engine channels and exist regardless of which scopes author them.
	bool s_bReceiversInitialized = false;
	void cr_initReceivers()
	{
		if (s_bReceiversInitialized)
		{
			return;
		}
		s_bReceiversInitialized = true;
		const ModifierFamily CITY_RECEIVES[] = { MODFAM_FOOD, MODFAM_PRODUCTION, MODFAM_COMMERCE, MODFAM_CULTURE };
		const ModifierFamily EMPIRE_RECEIVES[] = { MODFAM_GOLD, MODFAM_RESEARCH, MODFAM_CULTURE, MODFAM_ESPIONAGE };
		CascadeScopeLayout& cityLayout = s_layouts[(int)CASC_SCOPE_CITY];
		CascadeScopeLayout& empireLayout = s_layouts[(int)CASC_SCOPE_EMPIRE];
		for (int iIndex = 0; iIndex < 4; ++iIndex)
		{
			const int iCityChannel = cr_mint(CITY_RECEIVES[iIndex], (int)CHANNEL_AMOUNT, -1, false);
			cityLayout.receiverChannels.push_back(iCityChannel);
			cityLayout.receiverSlots.insert(std::make_pair(iCityChannel, iIndex));
			const int iEmpireChannel = cr_mint(EMPIRE_RECEIVES[iIndex], (int)CHANNEL_AMOUNT, -1, false);
			empireLayout.receiverChannels.push_back(iEmpireChannel);
			empireLayout.receiverSlots.insert(std::make_pair(iEmpireChannel, iIndex));
		}
	}

	// THE BIT CONTRACT IS ORDER-INDEPENDENT BY CONSTRUCTION: receiver bits live in a FIXED region at the TOP
	// of the 64-bit budget, so a receiver's bit never depends on how many channels have been minted when it
	// is computed. Channel slots are append-only (a local slot never moves once assigned), so EVERY bit's
	// meaning is stable for the process lifetime -- a mask computed or applied at any point of the load
	// (a cached SourceRoute, a package's already-set dirty bits) stays valid across later minting. Budget:
	// the measured channel sets (city 40 / empire 50 authored + the wellbeing sign twins) fit under the
	// 59-slot channel region; the spec'd receiver tables are 4 slots per consuming scope (bits 59..62).
	// Bit 63 stays the over-budget tripwire.
	enum
	{
		CASCADE_RECEIVER_BIT_FIRST = 59,
		CASCADE_TRIPWIRE_BIT = 63
	};

	// A CHANNEL slot's bit, clamped below the fixed receiver region: an over-budget slot shares the region's
	// last channel bit (coarse-safe -- marks a sibling too rather than never marking, and never collides with
	// a receiver bit; the measured sets fit, so this is a tripwire, not a plan).
	int64_t cr_channelBitOf(int iSlotIndex)
	{
		if (iSlotIndex < 0)
		{
			return 0;
		}
		if (iSlotIndex >= CASCADE_RECEIVER_BIT_FIRST)
		{
			FAssertMsg(false, "CascadeChannelRegistry: a scope's channel count exceeded the fixed channel-bit region");
			return (int64_t)1 << (CASCADE_RECEIVER_BIT_FIRST - 1);
		}
		return (int64_t)1 << iSlotIndex;
	}

	// A RECEIVER slot's bit at its FIXED position (the spec'd receiver tables carry 4 entries per consuming
	// scope; the region holds exactly 4 -- a 5th spec'd receiver trips the assert, never a silent alias).
	int64_t cr_receiverBitOf(int iReceiverIndex)
	{
		if (iReceiverIndex < 0)
		{
			return 0;
		}
		if (CASCADE_RECEIVER_BIT_FIRST + iReceiverIndex >= CASCADE_TRIPWIRE_BIT)
		{
			FAssertMsg(false, "CascadeChannelRegistry: a scope's receiver count exceeded the fixed receiver-bit region");
			return (int64_t)1 << (CASCADE_TRIPWIRE_BIT - 1);
		}
		return (int64_t)1 << (CASCADE_RECEIVER_BIT_FIRST + iReceiverIndex);
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
	const std::map<int, int>::const_iterator found = layout.channelSlots.find(iChannel);
	return found == layout.channelSlots.end() ? -1 : found->second;
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

int64_t CascadeChannelRegistry::scopeChannelBit(CvCascScope eScope, int iChannel)
{
	const int iSlot = scopeSlotIndex(eScope, iChannel);
	return iSlot < 0 ? 0 : cr_channelBitOf(iSlot);
}

int64_t CascadeChannelRegistry::scopeAllChannelsMask(CvCascScope eScope)
{
	const int iCount = scopeChannelCount(eScope);
	int64_t iMask = 0;
	for (int iSlot = 0; iSlot < iCount; ++iSlot)
	{
		iMask |= cr_channelBitOf(iSlot);
	}
	return iMask;
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
	const std::map<int, int>::const_iterator found = layout.receiverSlots.find(iChannel);
	return found == layout.receiverSlots.end() ? -1 : found->second;
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

int64_t CascadeChannelRegistry::scopeReceiverBit(CvCascScope eScope, int iChannel)
{
	const int iReceiver = scopeReceiverIndex(eScope, iChannel);
	if (iReceiver < 0)
	{
		return 0;
	}
	return cr_receiverBitOf(iReceiver);
}

int64_t CascadeChannelRegistry::scopeAllReceiversMask(CvCascScope eScope)
{
	const int iReceivers = scopeReceiverCount(eScope);
	int64_t iMask = 0;
	for (int iReceiver = 0; iReceiver < iReceivers; ++iReceiver)
	{
		iMask |= cr_receiverBitOf(iReceiver);
	}
	return iMask;
}

int64_t CascadeChannelRegistry::scopeReceiversFedBy(CvCascScope eReceiverScope, CvCascScope eSourceScope)
{
	if (!cr_isPackageScope(eSourceScope))
	{
		return 0;
	}
	const CascadeScopeLayout& sourceLayout = s_layouts[(int)eSourceScope];
	int64_t iMask = 0;
	for (size_t iSlot = 0; iSlot < sourceLayout.channels.size(); ++iSlot)
	{
		iMask |= scopeReceiverBit(eReceiverScope, sourceLayout.channels[iSlot]);
	}
	return iMask;
}

namespace
{
	// The decode's shared name append (the "|"-joined rendering; receivers prefix "sum:").
	void cr_appendDecodedName(char* szOut, int iOutSize, bool& bWroteAny, bool bReceiver, const char* szName)
	{
		int iRemaining = iOutSize - (int)strlen(szOut) - 1;
		if (iRemaining <= 0)
		{
			return;
		}
		if (bWroteAny)
		{
			strncat(szOut, "|", iRemaining);
			iRemaining = iOutSize - (int)strlen(szOut) - 1;
		}
		if (iRemaining > 0 && bReceiver)
		{
			strncat(szOut, "sum:", iRemaining);
			iRemaining = iOutSize - (int)strlen(szOut) - 1;
		}
		if (iRemaining > 0)
		{
			strncat(szOut, szName, iRemaining);
		}
		bWroteAny = true;
	}
}

void CascadeChannelRegistry::decodeMask(CvCascScope eScope, int64_t iMask, char* szOut, int iOutSize)
{
	szOut[0] = '\0';
	if (!cr_isPackageScope(eScope) || iOutSize <= 0)
	{
		return;
	}
	const int iChannels = scopeChannelCount(eScope);
	const int iReceivers = scopeReceiverCount(eScope);
	bool bWroteAny = false;
	for (int iSlot = 0; iSlot < iChannels && iSlot < (int)CASCADE_RECEIVER_BIT_FIRST; ++iSlot)
	{
		if ((iMask & cr_channelBitOf(iSlot)) == 0)
		{
			continue;
		}
		cr_appendDecodedName(szOut, iOutSize, bWroteAny, false, channelName(scopeSlotChannel(eScope, iSlot)));
	}
	for (int iReceiver = 0; iReceiver < iReceivers; ++iReceiver)
	{
		if ((iMask & cr_receiverBitOf(iReceiver)) == 0)
		{
			continue;
		}
		cr_appendDecodedName(szOut, iOutSize, bWroteAny, true, channelName(scopeReceiverChannel(eScope, iReceiver)));
	}
	if (!bWroteAny)
	{
		strncat(szOut, "none", iOutSize - (int)strlen(szOut) - 1);
	}
}

namespace
{
	// ---- the [MODIFIER] domain registration (event ids + field tags are DOMAIN-LOCAL) ----
	enum ModifierDomainEvent
	{
		MODEVT_CHANNEL_CENSUS = 1   // the load-end per-scope channel-set census (KEYS ONLY WHERE NEEDED)
	};
	enum ModifierDomainField
	{
		MODF_SCOPE = 0,
		MODF_AUTHORED,
		MODF_SLOTS,
		MODF_RECEIVERS
	};

	const char* cr_modifierDomainPrefix(int iEventId)
	{
		switch (iEventId)
		{
		case MODEVT_CHANNEL_CENSUS: return "[MODIFIER] channels";
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
		case CASC_SCOPE_AREA:   return "area";
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
