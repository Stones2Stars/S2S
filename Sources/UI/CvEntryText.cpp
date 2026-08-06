//
//	CvEntryText -- the ONE per-entry text renderer (see the header). Rendering rules:
//	  - the magnitude is /100 here (the single OUT boundary division -- [DEC-fixedpoint-x100]);
//	  - names resolve through the UNCACHED gDLL->getObjectText read of the referenced info's text key (never
//	    the caching getDescription -- this renderer also runs inside the load window ([READJSON] samples), and
//	    a pre-text-load call must not poison the per-info description cache);
//	  - what the TXT infrastructure cannot reach (the family/kind/member vocabulary, predicate spellings,
//	    count tokens) spells back its authored segment -- the honest fallback, never a guessed prose mapping.
//

#include "CvGameCoreDLL.h"          // PCH umbrella -- gDLL / GC / CvWString / FontSymbols
#include "CvEntryText.h"
#include "CvModEntry.h"             // the compiled §3.9 entry + modSegmentSpell (Infos on /I)
#include "CvCondition.h"            // the prebuilt condition tree
#include "CvInfoKinds.h"            // family key spell-back + the ruling-1 channel reverse lookups
#include "CvInfo.h"                 // the JSON-info base (getTextKeyWide via CvInfoBase)
#include "CvJsonConditionParse.h"   // cascadeSpellPredKind -- the predicate vocabulary's spell-back
#include "CvYieldInfo.h"
#include "CvCommerceInfo.h"
#include "Data/CvReadJson.h"        // rjInfoForTypeConst -- FK id -> the referenced info (name resolution; a READ)
#include "Data/CvDepositRead.h"     // MMKernel::unitIsPercentSide -- the ONE "is this a percent" test
#include "Defines/CvGlobals.h"

namespace
{
	// /100 at the out boundary: "2", "0.5", "12.75" -- integer math only (no float in the engine).
	CvWString etx_number100(int iAbsValue100)
	{
		const int iWhole = iAbsValue100 / 100;
		const int iFraction = iAbsValue100 % 100;
		if (iFraction == 0)
		{
			return CvWString::format(L"%d", iWhole);
		}
		if (iFraction % 10 == 0)
		{
			return CvWString::format(L"%d.%d", iWhole, iFraction / 10);
		}
		return CvWString::format(L"%d.%02d", iWhole, iFraction);
	}

	// A vocabulary token ("POPULATION", "TARGET_NUM_CITIES") -> a readable word ("Population",
	// "Target Num Cities"). Presentation-casing only -- the spelling stays the authored token's.
	CvWString etx_prettyToken(const std::string& szToken)
	{
		CvWString szPretty;
		bool bWordStart = true;
		for (size_t iChar = 0; iChar < szToken.size(); ++iChar)
		{
			const char cChar = szToken[iChar];
			if (cChar == '_')
			{
				szPretty += L' ';
				bWordStart = true;
				continue;
			}
			wchar_t wideChar = (wchar_t)(unsigned char)cChar;
			if (bWordStart)
			{
				wideChar = towupper(wideChar);
			}
			else
			{
				wideChar = towlower(wideChar);
			}
			szPretty += wideChar;
			bWordStart = false;
		}
		return szPretty;
	}

	// The uncached localized name of an info base (see the file header). Empty/unreachable -> the fallback.
	CvWString etx_infoBaseName(const CvInfoBase& info, const std::string& szFallback)
	{
		const wchar_t* szKeyWide = info.getTextKeyWide();
		if (szKeyWide != NULL && szKeyWide[0] != L'\0')
		{
			const CvWString szText = gDLL->getObjectText(CvWString(szKeyWide), 0);
			if (!szText.empty())
			{
				return szText;
			}
		}
		return etx_prettyToken(szFallback);
	}

	// An FK-referenced info's name: TYPE string + resolved id -> the localized description, else the
	// prettified type token (the honest spell-back for unresolved/count tokens: ERA, POPULATION, ...).
	CvWString etx_infoNameForType(const std::string& szType, int iId)
	{
		if (!szType.empty() && iId >= 0)
		{
			// the CONST twin -- name resolution is a READ, and the get-or-create dispatch would grow the
			// registry for an FK that resolved to an id the registry never mapped.
			const CvInfo* pInfo = rjInfoForTypeConst(szType, iId);
			if (pInfo != NULL)
			{
				return etx_infoBaseName(*pInfo, szType);
			}
		}
		return etx_prettyToken(szType);
	}

	// The experienced-where phrase of a non-city scope (city is the default containing scope -- unrendered).
	const wchar_t* etx_scopePhrase(CvCascScope eScope)
	{
		switch (eScope)
		{
		case CASC_SCOPE_WORLD:       return L"world-wide";
		case CASC_SCOPE_TEAM:        return L"team-wide";
		case CASC_SCOPE_EMPIRE:      return L"empire-wide";
		case CASC_SCOPE_CITY:        return L"";
		case CASC_SCOPE_PLOT:        return L"on this plot";
		case CASC_SCOPE_IMPROVEMENT: return L"on the improvement";
		case CASC_SCOPE_FEATURE:     return L"on the feature";
		case CASC_SCOPE_TERRAIN:     return L"on the terrain";
		case CASC_SCOPE_ROUTE:       return L"on the route";
		case CASC_SCOPE_BUILDING:    return L"on the building";
		case CASC_SCOPE_SPECIALIST:  return L"per specialist";
		case CASC_SCOPE_UNIT:        return L"for this unit";
		case CASC_SCOPE_SELF:        return L"for itself";
		default:                     return L"";
		}
	}

	// The bare scope word (the per-scaler's cross-scope annotation) -- the reverse of jsonParseScope.
	const wchar_t* etx_scopeWord(CvCascScope eScope)
	{
		switch (eScope)
		{
		case CASC_SCOPE_WORLD:       return L"world";
		case CASC_SCOPE_TEAM:        return L"team";
		case CASC_SCOPE_EMPIRE:      return L"empire";
		case CASC_SCOPE_CITY:        return L"city";
		case CASC_SCOPE_PLOT:        return L"plot";
		case CASC_SCOPE_IMPROVEMENT: return L"improvement";
		case CASC_SCOPE_FEATURE:     return L"feature";
		case CASC_SCOPE_TERRAIN:     return L"terrain";
		case CASC_SCOPE_ROUTE:       return L"route";
		case CASC_SCOPE_BUILDING:    return L"building";
		case CASC_SCOPE_SPECIALIST:  return L"specialist";
		case CASC_SCOPE_UNIT:        return L"unit";
		case CASC_SCOPE_SELF:        return L"self";
		default:                     return L"";
		}
	}

	// The FK target's TYPE segment: the underscored non-target-token segment of the authored address (the
	// CvModifiers decode's own rule -- member spellings are camelCase, never underscored). The property
	// family's key segment (seg[0] = PROPERTY_*) is excluded -- it names the family, not the target.
	const char* etx_targetTypeSegment(const CvModEntry& entry)
	{
		const int iFirstSeg = (entry.family == MODFAM_PROPERTY) ? 1 : 0;
		for (int iSegIdx = iFirstSeg; iSegIdx < entry.nSeg && iSegIdx < CvModEntry::MOD_ENTRY_SEGS; ++iSegIdx)
		{
			const char* szSegment = modSegmentSpell(entry.seg[iSegIdx]);
			if (strchr(szSegment, '_') == NULL)
			{
				continue;
			}
			if (infoIsTargetToken(szSegment))
			{
				continue;
			}
			return szSegment;
		}
		return NULL;
	}

	// The family/kind name: TXT-reachable where the vocabulary keys an engine info (the ruling-1 channel
	// families -> Yield/CommerceInfo; the open property plane -> the PropertyInfo); the closed vocabulary's
	// own families spell back their authored key + member path (no TXT keys exist for them -- honest).
	CvWString etx_entryName(const CvModEntry& entry)
	{
		CvWString szName;
		if (entry.family == MODFAM_PROPERTY)
		{
			const std::string szFamilySeg = (entry.nSeg > 0) ? modSegmentSpell(entry.seg[0]) : "";
			szName = etx_infoNameForType(szFamilySeg, entry.propertyFk);
		}
		else
		{
			const int iYield = infoFamilyYield(entry.family);
			const int iCommerce = infoFamilyCommerce(entry.family);
			const char* szFamilyKey = infoFamilyKey(entry.family);
			const std::string szFamilyFallback = (szFamilyKey != NULL) ? szFamilyKey : "?";
			if (iYield >= 0)
			{
				szName = etx_infoBaseName(GC.getYieldInfo((YieldTypes)iYield), szFamilyFallback);
			}
			else if (iCommerce >= 0)
			{
				szName = etx_infoBaseName(GC.getCommerceInfo((CommerceTypes)iCommerce), szFamilyFallback);
			}
			else
			{
				szName = CvWString(szFamilyFallback);   // the authored family key, verbatim
			}
		}
		if (entry.memberSeg >= 0)
		{
			szName += CvWString(L" ") + CvWString(modSegmentSpell(entry.memberSeg));
		}
		return szName;
	}

	// The signed magnitude + unit marker ("+2", "-25%", "x1.5"). /100 happens here.
	CvWString etx_signedMagnitude(const CvModEntry& entry)
	{
		const int iAbsValue = (entry.value < 0) ? -entry.value : entry.value;
		const wchar_t* szSign = (entry.value < 0) ? L"-" : L"+";
		//	⛔ THE REDUCE IS PER UNIT, NEVER BLANKET ([fixed-point-and-scales] §4d). A percent is stored PLAIN --
		//	`mod_valueForUnit` scales every unit EXCEPT the percent side, because a percent carries no decimals --
		//	so reducing one here renders a +3% civic as "+0.03%", and a 1% entry as "+0%". Flats and multipliers
		//	genuinely are x100 and still reduce.
		//	⚑ The test is the ONE predicate the parse, the deposit index and the gather already share
		//	([DEC-single-implementation]); a second copy of "is this a percent" is what would drift.
		const CvWString szNumber = MMKernel::unitIsPercentSide(entry.unit)
			? CvWString::format(L"%d", iAbsValue)
			: etx_number100(iAbsValue);
		switch (entry.unit)
		{
		case CASC_UNIT_PERCENT:
			return CvWString(szSign) + szNumber + L"%";
		case CASC_UNIT_RAW_PERCENT:
			return CvWString(szSign) + szNumber + L"% (raw)";
		case CASC_UNIT_MULTIPLIER:
			return CvWString(L"x") + ((entry.value < 0) ? CvWString(L"-") : CvWString()) + szNumber;
		case CASC_UNIT_POST_MULTIPLIER:
			return CvWString(L"x") + ((entry.value < 0) ? CvWString(L"-") : CvWString()) + szNumber + L" (post)";
		case CASC_UNIT_PER_POPULATION:
			return CvWString(szSign) + szNumber + L" per population";
		case CASC_UNIT_PER_SPECIALIST:
			return CvWString(szSign) + szNumber + L" per specialist";
		case CASC_UNIT_PER_CORPORATION_LEVEL:
			return CvWString(szSign) + szNumber + L" per corporation level";
		case CASC_UNIT_FLAT:
		case CASC_UNIT_COUNT:
		default:
			return CvWString(szSign) + szNumber;
		}
	}

	// The §3.7 per-scaler phrase ("per Specialist", "per 5 Population", "per City over the city limit").
	CvWString etx_perPhrase(const CvModEntry& entry)
	{
		CvWString szPhrase = L"per ";
		if (entry.perEach > 1)
		{
			szPhrase += CvWString::format(L"%d ", entry.perEach);
		}
		if (!entry.perAnyOf.empty())
		{
			for (size_t iAnyIdx = 0; iAnyIdx < entry.perAnyOf.size() && iAnyIdx < entry.perAnyOfTypes.size(); ++iAnyIdx)
			{
				if (iAnyIdx > 0)
				{
					szPhrase += L" / ";
				}
				szPhrase += etx_infoNameForType(entry.perAnyOfTypes[iAnyIdx], entry.perAnyOf[iAnyIdx]);
			}
		}
		else
		{
			szPhrase += etx_infoNameForType(entry.perType, entry.perTypeId);
		}
		if (entry.hasAbove)
		{
			// the §3.7 over-threshold scaler (ruling 26): "over the city limit" for the token form (the
			// world-size scaling makes the resolved base alone misleading), the literal spelled directly
			if (entry.perAboveToken == "CITY_LIMIT")
			{
				szPhrase += L" over the city limit";
			}
			else if (!entry.perAboveToken.empty())
			{
				szPhrase += CvWString(L" over ") + etx_prettyToken(entry.perAboveToken);
			}
			else if (entry.perAbove >= 0)
			{
				szPhrase += CvWString::format(L" over %d", entry.perAbove);
			}
		}
		if (entry.perScope >= 0 && entry.perScope != (int)entry.scope)
		{
			szPhrase += CvWString(L" (") + etx_scopeWord((CvCascScope)entry.perScope) + L")";
		}
		return szPhrase;
	}

	// The §3.3 ranked-selection phrase ("top 3 by highest citySize") -- parse-carried quals (ruling 25).
	CvWString etx_rankPhrase(const CvModEntry& entry)
	{
		CvWString szPhrase;
		if (entry.rankMax >= 0)
		{
			szPhrase = CvWString::format(L"top %d", entry.rankMax);
		}
		else if (!entry.rankMaxToken.empty())
		{
			szPhrase = CvWString(L"top ") + etx_prettyToken(entry.rankMaxToken);
		}
		if (entry.orderedBySeg >= 0)
		{
			if (!szPhrase.empty())
			{
				szPhrase += L" ";
			}
			szPhrase += CvWString(entry.orderedDescending ? L"by highest " : L"by lowest ")
			          + CvWString(modSegmentSpell(entry.orderedBySeg));
		}
		return szPhrase;
	}

	CvWString etx_predicateText(const CvCondition& condition)
	{
		switch (condition.predKind)
		{
		case CASC_PRED_LATITUDE:
			if (condition.min >= 0 && condition.max >= 0)
			{
				return CvWString::format(L"latitude %d-%d", condition.min, condition.max);
			}
			if (condition.min >= 0)
			{
				return CvWString::format(L"latitude %d+", condition.min);
			}
			return CvWString::format(L"latitude up to %d", condition.max);
		case CASC_PRED_EXISTED_FOR:
			return CvWString::format(L"existed %d+ years", condition.min);
		case CASC_PRED_IS_TAG:
		{
			// param carries the full TAG_<SUFFIX> name (json §8) -- render the suffix
			std::string szSuffix = condition.param;
			if (szSuffix.compare(0, 4, "TAG_") == 0)
			{
				szSuffix = szSuffix.substr(4);
			}
			return CvWString(L"is ") + etx_prettyToken(szSuffix);
		}
		default:
			break;
		}
		const char* szSpelling = cascadeSpellPredKind(condition.predKind);
		CvWString szText = (szSpelling[0] != '\0') ? CvWString(szSpelling) : CvWString(L"?");
		if (!condition.param.empty())
		{
			szText += CvWString(L" ") + etx_infoNameForType(condition.param, condition.id);
		}
		if (condition.predKind == CASC_PRED_HAS_COAST && condition.min >= 0)
		{
			szText += CvWString::format(L" (area %d+)", condition.min);
		}
		return szText;
	}

	CvWString etx_presenceText(const CvCondition& condition)
	{
		const CvWString szName = etx_infoNameForType(condition.type, condition.id);
		CvWString szText;
		if (condition.max == 0 && condition.min <= 0)
		{
			szText = CvWString(L"no ") + szName;
		}
		else if (condition.min > 1 && condition.max >= 0)
		{
			szText = CvWString::format(L"%d-%d ", condition.min, condition.max) + szName;
		}
		else if (condition.min > 1)
		{
			szText = CvWString::format(L"%d+ ", condition.min) + szName;
		}
		else if (condition.id < 0 && condition.min > 0)
		{
			// a count TOKEN threshold (ERA/POPULATION/TURN...): the count comparison reads better spelled out
			szText = szName + CvWString::format(L" >= %d", condition.min);
		}
		else
		{
			szText = szName;   // plain presence (min 1)
		}
		switch (condition.connection)
		{
		case CASC_CONN_TRADE:
			szText += L" connected";
			break;
		case CASC_CONN_VICINITY:
			szText += L" in vicinity";
			break;
		case CASC_CONN_TRADE_OR_VICINITY:
			szText += L" connected or in vicinity";
			break;
		case CASC_CONN_NONE:
		default:
			if (condition.vicinity != CASC_VIC_NONE)
			{
				szText += L" in vicinity";
			}
			break;
		}
		return szText;
	}

	CvWString etx_groupText(const CvCondition& condition)
	{
		std::vector<CvWString> parts;
		for (size_t iChild = 0; iChild < condition.all.size(); ++iChild)
		{
			parts.push_back(entryConditionText(condition.all[iChild]));
		}
		if (!condition.anyOf.empty())
		{
			CvWString szAny;
			for (size_t iChild = 0; iChild < condition.anyOf.size(); ++iChild)
			{
				if (iChild > 0)
				{
					szAny += L" or ";
				}
				szAny += entryConditionText(condition.anyOf[iChild]);
			}
			const bool bMixed = !condition.all.empty() || !condition.noneOf.empty();
			if (bMixed && condition.anyOf.size() > 1)
			{
				szAny = CvWString(L"(") + szAny + L")";
			}
			parts.push_back(szAny);
		}
		for (size_t iChild = 0; iChild < condition.noneOf.size(); ++iChild)
		{
			parts.push_back(CvWString(L"not ") + entryConditionText(condition.noneOf[iChild]));
		}
		if (condition.enabled != NULL)
		{
			parts.push_back(entryConditionText(condition.enabled));
		}
		if (condition.disabled != NULL)
		{
			parts.push_back(CvWString(L"not ") + entryConditionText(condition.disabled));
		}
		CvWString szText;
		for (size_t iPart = 0; iPart < parts.size(); ++iPart)
		{
			if (parts[iPart].empty())
			{
				continue;
			}
			if (!szText.empty())
			{
				szText += L" and ";
			}
			szText += parts[iPart];
		}
		return szText;
	}
}

CvWString entryConditionText(const CvCondition* condition)
{
	if (condition == NULL)
	{
		return CvWString();
	}
	switch (condition->kind)
	{
	case CASC_COND_PREDICATE:
		return etx_predicateText(*condition);
	case CASC_COND_PRESENCE:
		return etx_presenceText(*condition);
	case CASC_COND_GROUP:
	default:
		return etx_groupText(*condition);
	}
}

CvWString entryDetailLine(const CvModEntry& entry)
{
	CvWString szLine;

	// The wellbeing pair renders in the game's own icon idiom ("+2<happy>"): abs magnitude + the sign-routed
	// channel icon (modifier.md §2b -- the sign routes the channel; the icon carries the polarity).
	const bool bWellbeingIcon = (entry.family == MODFAM_HAPPINESS || entry.family == MODFAM_HEALTH)
	                         && entry.unit == CASC_UNIT_FLAT && entry.kind == 0;
	if (bWellbeingIcon)
	{
		const bool bGood = entry.value >= 0;
		int iSymbol;
		if (entry.family == MODFAM_HAPPINESS)
		{
			iSymbol = gDLL->getSymbolID(bGood ? HAPPY_CHAR : UNHAPPY_CHAR);
		}
		else
		{
			iSymbol = gDLL->getSymbolID(bGood ? HEALTHY_CHAR : UNHEALTHY_CHAR);
		}
		const int iAbsValue100 = (entry.value < 0) ? -entry.value : entry.value;
		szLine = CvWString(L"+") + etx_number100(iAbsValue100) + CvWString::format(L"%c", iSymbol);
	}
	else
	{
		szLine = etx_signedMagnitude(entry) + L" " + etx_entryName(entry);
	}

	// the deposit's target: a named-entity key renders its resolved name; a plural object-target its token
	if (entry.targetFk >= 0)
	{
		const char* szTypeSegment = etx_targetTypeSegment(entry);
		const std::string szTargetType = (szTypeSegment != NULL) ? szTypeSegment : "";
		szLine += CvWString(L" for ") + etx_infoNameForType(szTargetType, entry.targetFk);
	}
	else if (entry.targetSeg >= 0)
	{
		szLine += CvWString(L" on ") + CvWString(modSegmentSpell(entry.targetSeg));
	}

	// the scope, where non-obvious (city is the default containing scope)
	const wchar_t* szScopePhrase = etx_scopePhrase(entry.scope);
	if (szScopePhrase[0] != L'\0')
	{
		szLine += CvWString(L", ") + szScopePhrase;
	}

	if (entry.hasPer)
	{
		szLine += CvWString(L" ") + etx_perPhrase(entry);
	}
	if (entry.hasRankQual)
	{
		const CvWString szRank = etx_rankPhrase(entry);
		if (!szRank.empty())
		{
			szLine += CvWString(L" -- ") + szRank;
		}
	}
	if (entry.enabled != NULL)
	{
		szLine += CvWString(L" -- while ") + entryConditionText(entry.enabled);
	}
	if (entry.disabled != NULL)
	{
		szLine += CvWString(L" -- unless ") + entryConditionText(entry.disabled);
	}
	if (entry.unitQual != NULL)
	{
		szLine += CvWString(L" -- units matching ") + entryConditionText(entry.unitQual);
	}
	if (entry.religionQual != NULL)
	{
		szLine += CvWString(L" -- per city religion matching ") + entryConditionText(entry.religionQual);
	}
	if (entry.aiOnly)
	{
		szLine += L" [AI only]";
	}
	return szLine;
}
