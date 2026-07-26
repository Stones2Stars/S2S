//
//	CascadeCalcCount -- the (scope, channel) calc counters + the per-turn [MODIFIER] report (see the header).
//	The SD_MODIFIER domain self-registers here (per-domain isolation: zero shared edits in the spine).
//

#include "CvGameCoreDLL.h"
#include "CvCascadeCalcCount.h"
#include "CvCascadeChannelRegistry.h"
#include "Spine/CvEventSpine.h"
#include <map>
#include <utility>

namespace
{
	// (scope, channel) -> per-turn recompute count. Sparse -- a quiet turn allocates nothing.
	std::map<std::pair<int, int>, int> s_slotCounts;
	std::map<std::pair<int, int>, int> s_sumCounts;

	// ---- the [MODIFIER] domain registration (event ids + field tags are DOMAIN-LOCAL) ----
	enum ModifierDomainEvent
	{
		MODEVT_CALC_TURN = 1,       // the per-scope headline: t=, scope=, calcs=, sums=
		MODEVT_CALC_CHANNEL = 2,    // the per-(scope, channel) detail line (level 2)
		MODEVT_CHANNEL_CENSUS = 3   // the load-end per-scope channel-set census (KEYS ONLY WHERE NEEDED)
	};
	enum ModifierDomainField
	{
		MODF_SCOPE = 0,
		MODF_CHANNEL,
		MODF_CALCS,
		MODF_SUMS,
		MODF_AUTHORED,
		MODF_SLOTS,
		MODF_RECEIVERS
	};

	const char* modifierDomainPrefix(int iEventId)
	{
		switch (iEventId)
		{
		case MODEVT_CALC_TURN:      return "[MODIFIER] calcCount";
		case MODEVT_CALC_CHANNEL:   return "[MODIFIER] calcChannel";
		case MODEVT_CHANNEL_CENSUS: return "[MODIFIER] channels";
		default:                    return "[MODIFIER] ?";
		}
	}

	const char* modifierDomainFieldInfo(int iFieldTag, SpineFieldType* peType)
	{
		switch (iFieldTag)
		{
		case MODF_SCOPE:     *peType = SFT_STR; return "scope";
		case MODF_CHANNEL:   *peType = SFT_STR; return "channel";
		case MODF_CALCS:     *peType = SFT_INT; return "calcs";
		case MODF_SUMS:      *peType = SFT_INT; return "sums";
		case MODF_AUTHORED:  *peType = SFT_INT; return "authored";
		case MODF_SLOTS:     *peType = SFT_INT; return "slots";
		case MODF_RECEIVERS: *peType = SFT_INT; return "receivers";
		default:             *peType = SFT_INT; return NULL;
		}
	}

	bool s_bDomainRegistered = false;
	void cc_ensureDomain()
	{
		if (s_bDomainRegistered)
		{
			return;
		}
		s_bDomainRegistered = true;
		spineRegisterDomain(SD_MODIFIER, modifierDomainPrefix, NULL, modifierDomainFieldInfo);   // NULL => Cascade.log
	}

	const char* cc_scopeName(int iScope)
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

void CascadeCalcCount::count(CvCascScope eScope, int iChannel)
{
	++s_slotCounts[std::make_pair((int)eScope, iChannel)];
}

void CascadeCalcCount::countSum(CvCascScope eScope, int iChannel)
{
	++s_sumCounts[std::make_pair((int)eScope, iChannel)];
}

void CascadeCalcCount::reportAndReset()
{
	cc_ensureDomain();
	// per-scope headline totals (level 1) + per-channel detail (level 2)
	int aScopeCalcs[CASCADE_PACKAGE_SCOPES] = { 0, 0, 0, 0, 0, 0 };
	int aScopeSums[CASCADE_PACKAGE_SCOPES] = { 0, 0, 0, 0, 0, 0 };
	for (std::map<std::pair<int, int>, int>::const_iterator it = s_slotCounts.begin(); it != s_slotCounts.end(); ++it)
	{
		const int iScope = it->first.first;
		if (iScope >= 0 && iScope < CASCADE_PACKAGE_SCOPES)
		{
			aScopeCalcs[iScope] += it->second;
		}
		CvSpineEvent detail(EVENTKIND_DIAGNOSTIC, SD_MODIFIER, MODEVT_CALC_CHANNEL, 2);
		detail.addStr(MODF_SCOPE, cc_scopeName(iScope));
		detail.addStr(MODF_CHANNEL, CascadeChannelRegistry::channelName(it->first.second));
		detail.addI(MODF_CALCS, it->second);
		eventSpine().emit(detail);
	}
	for (std::map<std::pair<int, int>, int>::const_iterator it = s_sumCounts.begin(); it != s_sumCounts.end(); ++it)
	{
		const int iScope = it->first.first;
		if (iScope >= 0 && iScope < CASCADE_PACKAGE_SCOPES)
		{
			aScopeSums[iScope] += it->second;
		}
	}
	for (int iScope = 0; iScope < CASCADE_PACKAGE_SCOPES; ++iScope)
	{
		if (aScopeCalcs[iScope] == 0 && aScopeSums[iScope] == 0)
		{
			continue;   // a quiet scope reports nothing -- the quiet turn approaches zero lines
		}
		CvSpineEvent headline(EVENTKIND_DIAGNOSTIC, SD_MODIFIER, MODEVT_CALC_TURN, 1);
		headline.addStr(MODF_SCOPE, cc_scopeName(iScope));
		headline.addI(MODF_CALCS, aScopeCalcs[iScope]);
		headline.addI(MODF_SUMS, aScopeSums[iScope]);
		eventSpine().emit(headline);
	}
	s_slotCounts.clear();
	s_sumCounts.clear();
}

void CascadeCalcCount::reportChannelCensus()
{
	static bool s_bReported = false;
	if (s_bReported)
	{
		return;
	}
	s_bReported = true;
	cc_ensureDomain();
	// One line per package scope: the AUTHORED channel count (comparable to the state-repositories.md measured
	// expectation), the SLOTTED count (sign twins included), and the receiver slots. World reports too -- its
	// authored count is the mis-scoped-data census (world is CONFIG, no package).
	for (int iScope = 0; iScope < CASCADE_PACKAGE_SCOPES; ++iScope)
	{
		CvSpineEvent census(EVENTKIND_DIAGNOSTIC, SD_MODIFIER, MODEVT_CHANNEL_CENSUS, 1);
		census.addStr(MODF_SCOPE, cc_scopeName(iScope));
		census.addI(MODF_AUTHORED, CascadeChannelRegistry::scopeAuthoredChannelCount((CvCascScope)iScope));
		census.addI(MODF_SLOTS, CascadeChannelRegistry::scopeChannelCount((CvCascScope)iScope));
		census.addI(MODF_RECEIVERS, CascadeChannelRegistry::scopeReceiverCount((CvCascScope)iScope));
		eventSpine().emit(census);
	}
}
