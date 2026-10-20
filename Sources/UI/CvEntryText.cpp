//
//	CvEntryText -- the ONE per-entry text renderer (see the header). Rendering rules:
//	  - the magnitude is /100 here (the single OUT boundary division -- docs/specs/curators/fixed-point-and-scales.md §1 (the x100 fixed-point model));
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
#include "Conditions/CvConditionQuery.h"   // bucketForType / predicateForBucket -- which entity a leaf names
#include "CvYieldInfo.h"
#include "CvCommerceInfo.h"
#include "Data/CvReadJson.h"        // rjInfoForTypeConst -- FK id -> the referenced info (name resolution; a READ)
#include "Data/CvDepositRead.h"     // MMKernel::unitIsPercentSide
#include "Data/CvInfoValuation.h"   // resolvedCityLimit -- the limit a CITY_LIMIT scaler counts against
#include "CvClassificationRegistry.h"   // keyOf -- a hiding method's skill key, for its name
#include "CvPropertyInfo.h"             // getChar -- a property's font glyph
#include "Defines/CvGlobals.h"
#include "AI/CvGameAI.h"            // the live game options a condition's GAMEOPTION_ atoms resolve against

namespace
{
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

	///<summary>An authored member path ("dynamicDefense", "counterDamage.chance") as lower-case words ("dynamic
	/// defense", "counter damage chance"). Presentation-casing only, like etx_prettyToken; a run of capitals stays
	/// one word.</summary>
	CvWString etx_memberWords(const char* szMember)
	{
		CvWString szWords;
		bool bPrevUpper = false;
		for (const char* pChar = szMember; *pChar != '\0'; ++pChar)
		{
			const char cChar = *pChar;
			if (cChar == '.')
			{
				szWords += L' ';
				bPrevUpper = false;
				continue;
			}
			const bool bUpper = (cChar >= 'A' && cChar <= 'Z');
			if (bUpper && !bPrevUpper && !szWords.empty() && szWords[szWords.size() - 1] != L' ')
			{
				szWords += L' ';
			}
			szWords += bUpper ? (wchar_t)(cChar - 'A' + 'a') : (wchar_t)(unsigned char)cChar;
			bPrevUpper = bUpper;
		}
		return szWords;
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

	// The experienced-where phrase of a scope. City and unit are the default containing scope of the entity
	// that carries the entry, so they are unrendered.
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
		case CASC_SCOPE_UNIT:        return L"";
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
	//	A yield/commerce channel renders as its GAMEFONT GLYPH -- what legacy showed and what a player reads at a
	//	glance, rather than the channel spelled out in words.
	//	⚠ The glyph is a RUNTIME slot the CvGameTextMgr symbol pass assigns, not authored data ([patterns.md]:
	//	it is text-plane) -- and this renderer ALSO runs inside the load window (the [READJSON] samples, per the
	//	file header), where that pass has not run yet. The slot is 0 until it does, so a 0 falls back to the NAME
	//	instead of rendering a null character into a log line.
	CvWString etx_channelLabel(int iChar, const CvInfoBase& info, const std::string& szFallback)
	{
		if (iChar > 0)
		{
			return CvWString::format(L"%c", iChar);
		}
		return etx_infoBaseName(info, szFallback);
	}

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
				const CvYieldInfo& kYield = GC.getYieldInfo((YieldTypes)iYield);
				szName = etx_channelLabel(kYield.getChar(), kYield, szFamilyFallback);
			}
			else if (iCommerce >= 0)
			{
				const CvCommerceInfo& kCommerce = GC.getCommerceInfo((CommerceTypes)iCommerce);
				szName = etx_channelLabel(kCommerce.getChar(), kCommerce, szFamilyFallback);
			}
			else
			{
				szName = etx_memberWords(szFamilyFallback.c_str());   // the authored family key, as words
			}
		}
		// a member named like its own family (`heal.unit.….heal`) would read "heal heal", so it is not repeated
		const char* szOwnFamilyKey = (entry.family == MODFAM_PROPERTY) ? NULL : infoFamilyKey(entry.family);
		if (entry.memberSeg >= 0
		&& (szOwnFamilyKey == NULL || strcmp(modSegmentSpell(entry.memberSeg), szOwnFamilyKey) != 0))
		{
			szName += CvWString(L" ") + etx_memberWords(modSegmentSpell(entry.memberSeg));
		}
		return szName;
	}

	///<summary>The name of the entity an entry targets by key ("UNITCOMBAT_CRIMINAL" -> "Criminals").</summary>
	CvWString etx_targetName(const CvModEntry& entry)
	{
		const char* szTypeSegment = etx_targetTypeSegment(entry);
		const std::string szTargetType = (szTypeSegment != NULL) ? szTypeSegment : "";
		return etx_infoNameForType(szTargetType, entry.targetFk);
	}

	// Is this entry's `per` quantum of 100 folded into the magnitude, so "30 per 100 Population" reads
	// "0.3 per Population"? Only where the fold is exact, and never on a percent, which carries no decimals.
	bool etx_perNormalised(const CvModEntry& entry)
	{
		return entry.hasPer
			&& entry.perEach == 100
			&& entry.value % 100 == 0
			&& !MMKernel::unitIsPercentSide(entry.unit);
	}

	///<summary>The magnitude of iValue as the entry renders it: absolute, with a normalised `per` quantum folded
	/// in. A vision amount reads in plots of open ground (vision.md §1a: VISION_PLOT to the plot), so "+100" sight is
	/// "+1". iValue is the entry's authored value (bAuthored), or the value the engine resolved it to at one object;
	/// only the authored value carries a `per` quantum to fold in, since a resolved value has its scaling applied.</summary>
	int64_t etx_absMagnitudeOf(const CvModEntry& entry, int64_t iValue, bool bAuthored)
	{
		int64_t iAbsValue = (iValue < 0) ? -iValue : iValue;
		if (entry.family == MODFAM_VISION && !MMKernel::unitIsPercentSide(entry.unit))
		{
			iAbsValue /= VISION_PLOT;
		}
		return (bAuthored && etx_perNormalised(entry)) ? iAbsValue / 100 : iAbsValue;
	}

	///<summary>The entry's own authored magnitude as it renders (etx_absMagnitudeOf).</summary>
	int etx_absMagnitude(const CvModEntry& entry)
	{
		return (int)etx_absMagnitudeOf(entry, entry.value, true);
	}

	///<summary>The signed magnitude of iValue with the entry's unit marker ("+2", "-25%", "x1.5"); iValue and
	/// bAuthored as etx_absMagnitudeOf.</summary>
	CvWString etx_signedMagnitudeOf(const CvModEntry& entry, int64_t iValue, bool bAuthored)
	{
		const int64_t iAbsValue = etx_absMagnitudeOf(entry, iValue, bAuthored);
		const wchar_t* szSign = (iValue < 0) ? L"-" : L"+";
		const CvWString szNumber = MMKernel::unitIsPercentSide(entry.unit)
			? CvWString::format(L"%I64d", iAbsValue)
			: amountText(iAbsValue);
		switch (entry.unit)
		{
		case CASC_UNIT_PERCENT:
			return CvWString(szSign) + szNumber + L"%";
		case CASC_UNIT_RAW_PERCENT:
			return CvWString(szSign) + szNumber + L"% (raw)";
		case CASC_UNIT_MULTIPLIER:
			return CvWString(L"x") + ((iValue < 0) ? CvWString(L"-") : CvWString()) + szNumber;
		case CASC_UNIT_POST_MULTIPLIER:
			return CvWString(L"x") + ((iValue < 0) ? CvWString(L"-") : CvWString()) + szNumber + L" (post)";
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

	///<summary>The entry's own authored magnitude, signed, with its unit marker (etx_signedMagnitudeOf).</summary>
	CvWString etx_signedMagnitude(const CvModEntry& entry)
	{
		return etx_signedMagnitudeOf(entry, entry.value, true);
	}

	// The §3.7 per-scaler phrase ("per Specialist", "per 5 Population", "per City over the city limit").
	CvWString etx_perPhrase(const CvModEntry& entry)
	{
		CvWString szPhrase = L"per ";
		if (entry.perEach > 1 && !etx_perNormalised(entry))
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
			// the §3.7 over-threshold scaler (ruling 26): the city limit is named with the number the engine
			// counts against -- the base scaled for the world size -- so the cost and the limit read as one line;
			// the literal spelled directly
			if (entry.perAboveToken == "CITY_LIMIT")
			{
				const int iCityLimit = InfoValuation::resolvedCityLimit(entry.perAbove);
				szPhrase += (iCityLimit > 0)
					? CvWString::format(L" over the city limit of %d", iCityLimit)
					: CvWString(L" over the city limit");
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

	///<summary>True when the condition is one unit-tag test and nothing else.</summary>
	bool etx_isUnitTagOnly(const CvCondition* condition)
	{
		return condition != NULL && condition->kind == CASC_COND_PREDICATE && condition->predKind == CASC_PRED_IS_TAG;
	}

	///<summary>The reach phrase of an entry above the city whose whole `enabled` condition is one plain city check
	/// or its plain negation: it applies in exactly the cities that pass it, so it reads as where it applies ("+2%
	/// research in your capital") rather than as a reach and a gate that seem to contradict each other ("empire-wide
	/// -- while in your capital"). NULL when the condition is anything else.</summary>
	const char* etx_cityReachKey(const CvModEntry& entry)
	{
		if ((entry.scope != CASC_SCOPE_EMPIRE && entry.scope != CASC_SCOPE_TEAM) || entry.enabled == NULL || entry.disabled != NULL)
		{
			return NULL;
		}
		const CvCondition* pCondition = entry.enabled;
		bool bNegated = false;
		if (pCondition->kind == CASC_COND_GROUP && pCondition->all.empty() && pCondition->anyOf.empty()
			&& pCondition->noneOf.size() == 1 && pCondition->enabled == NULL && pCondition->disabled == NULL)
		{
			pCondition = pCondition->noneOf[0];
			bNegated = true;
		}
		if (pCondition == NULL || pCondition->kind != CASC_COND_PREDICATE || !pCondition->param.empty() || pCondition->min >= 0)
		{
			return NULL;
		}
		switch (pCondition->predKind)
		{
		case CASC_PRED_IS_CAPITAL:
			return bNegated ? "TXT_KEY_CONDITION_REACH_NOT_IS_CAPITAL" : "TXT_KEY_CONDITION_REACH_IS_CAPITAL";
		case CASC_PRED_IS_HOME_AREA:
			return bNegated ? "TXT_KEY_CONDITION_REACH_NOT_IS_HOME_AREA" : "TXT_KEY_CONDITION_REACH_IS_HOME_AREA";
		case CASC_PRED_STATE_RELIGION_IN_CITY:
			return bNegated ? "TXT_KEY_CONDITION_REACH_NOT_STATE_RELIGION_IN_CITY" : "TXT_KEY_CONDITION_REACH_STATE_RELIGION_IN_CITY";
		case CASC_PRED_IS_GOVERNMENT_CENTER:        return bNegated ? NULL : "TXT_KEY_CONDITION_REACH_IS_GOVERNMENT_CENTER";
		case CASC_PRED_HAS_POWER:                   return bNegated ? NULL : "TXT_KEY_CONDITION_REACH_HAS_POWER";
		case CASC_PRED_HAS_COAST:                   return bNegated ? NULL : "TXT_KEY_CONDITION_REACH_HAS_COAST";
		case CASC_PRED_HAS_RIVER:                   return bNegated ? NULL : "TXT_KEY_CONDITION_REACH_HAS_RIVER";
		case CASC_PRED_HAS_FRESHWATER:              return bNegated ? NULL : "TXT_KEY_CONDITION_REACH_HAS_FRESHWATER";
		case CASC_PRED_IS_HOLY_CITY:                return bNegated ? NULL : "TXT_KEY_CONDITION_REACH_IS_HOLY_CITY";
		case CASC_PRED_IS_STATE_RELIGION_HOLY_CITY: return bNegated ? NULL : "TXT_KEY_CONDITION_REACH_IS_STATE_RELIGION_HOLY_CITY";
		case CASC_PRED_IS_HEADQUARTERS:             return bNegated ? NULL : "TXT_KEY_CONDITION_REACH_IS_HEADQUARTERS";
		default:                                    return NULL;
		}
	}

	///<summary>The player-facing phrase of a bare predicate; NULL for one with no phrase of its own.</summary>
	const char* etx_predicateKey(CvCascPredKind ePredKind)
	{
		switch (ePredKind)
		{
		case CASC_PRED_IS_WATER:                    return "TXT_KEY_CONDITION_IS_WATER";
		case CASC_PRED_IS_LAND:                     return "TXT_KEY_CONDITION_IS_LAND";
		case CASC_PRED_IS_FLATLANDS:                return "TXT_KEY_CONDITION_IS_FLATLANDS";
		case CASC_PRED_IS_AIR:                      return "TXT_KEY_CONDITION_IS_AIR";
		case CASC_PRED_HAS_PEAK:                    return "TXT_KEY_CONDITION_HAS_PEAK";
		case CASC_PRED_HAS_HILLS:                   return "TXT_KEY_CONDITION_HAS_HILLS";
		case CASC_PRED_HAS_COAST:                   return "TXT_KEY_CONDITION_HAS_COAST";
		case CASC_PRED_HAS_RIVER:                   return "TXT_KEY_CONDITION_HAS_RIVER";
		case CASC_PRED_HAS_FRESHWATER:              return "TXT_KEY_CONDITION_HAS_FRESHWATER";
		case CASC_PRED_HAS_IRRIGATION:              return "TXT_KEY_CONDITION_HAS_IRRIGATION";
		case CASC_PRED_HAS_FEATURE:                 return "TXT_KEY_CONDITION_HAS_FEATURE";
		case CASC_PRED_HAS_LANDMARK:                return "TXT_KEY_CONDITION_HAS_LANDMARK";
		case CASC_PRED_VICINITY:                    return "TXT_KEY_CONDITION_VICINITY";
		case CASC_PRED_WORKABLE:                    return "TXT_KEY_CONDITION_WORKABLE";
		case CASC_PRED_IS_WORKED:                   return "TXT_KEY_CONDITION_IS_WORKED";
		case CASC_PRED_NO_NUKES:                    return "TXT_KEY_CONDITION_NO_NUKES";
		case CASC_PRED_IS_CAPITAL:                  return "TXT_KEY_CONDITION_IS_CAPITAL";
		case CASC_PRED_IS_GOVERNMENT_CENTER:        return "TXT_KEY_CONDITION_IS_GOVERNMENT_CENTER";
		case CASC_PRED_IS_HOME_AREA:                return "TXT_KEY_CONDITION_IS_HOME_AREA";
		case CASC_PRED_HAS_POWER:                   return "TXT_KEY_CONDITION_HAS_POWER";
		case CASC_PRED_HAS_STATE_RELIGION:          return "TXT_KEY_CONDITION_HAS_STATE_RELIGION";
		case CASC_PRED_STATE_RELIGION_IN_CITY:      return "TXT_KEY_CONDITION_STATE_RELIGION_IN_CITY";
		case CASC_PRED_IS_GOLDEN_AGE:               return "TXT_KEY_CONDITION_IS_GOLDEN_AGE";
		case CASC_PRED_IS_STATE_RELIGION_HOLY_CITY: return "TXT_KEY_CONDITION_IS_STATE_RELIGION_HOLY_CITY";
		case CASC_PRED_IS_ANARCHY:                  return "TXT_KEY_CONDITION_IS_ANARCHY";
		case CASC_PRED_IS_OWNED:                    return "TXT_KEY_CONDITION_IS_OWNED";
		case CASC_PRED_IS_REBEL:                    return "TXT_KEY_CONDITION_IS_REBEL";
		case CASC_PRED_IS_STATE_RELIGION:           return "TXT_KEY_CONDITION_IS_STATE_RELIGION";
		case CASC_PRED_IS_HOLY_CITY:                return "TXT_KEY_CONDITION_IS_HOLY_CITY";
		case CASC_PRED_IS_HEADQUARTERS:             return "TXT_KEY_CONDITION_IS_HEADQUARTERS";
		case CASC_PRED_CAN_FOUND:                   return "TXT_KEY_CONDITION_CAN_FOUND";
		case CASC_PRED_IS_FOREIGN:                  return "TXT_KEY_CONDITION_IS_FOREIGN";
		case CASC_PRED_SHARES_CIVIC:                return "TXT_KEY_CONDITION_SHARES_CIVIC";
		default:                                    return NULL;
		}
	}

	///<summary>The player-facing phrase of a predicate that names a type ("with Coal on the tile"); NULL for one
	/// with no phrase of its own.</summary>
	const char* etx_predicateTypeKey(CvCascPredKind ePredKind)
	{
		switch (ePredKind)
		{
		case CASC_PRED_HAS_FEATURE:     return "TXT_KEY_CONDITION_HAS_FEATURE_TYPE";
		case CASC_PRED_HAS_TERRAIN:     return "TXT_KEY_CONDITION_HAS_TERRAIN_TYPE";
		case CASC_PRED_HAS_IMPROVEMENT: return "TXT_KEY_CONDITION_HAS_IMPROVEMENT_TYPE";
		case CASC_PRED_HAS_BONUS:       return "TXT_KEY_CONDITION_HAS_BONUS_TYPE";
		case CASC_PRED_HAS_RELIGION:    return "TXT_KEY_CONDITION_HAS_RELIGION_TYPE";
		case CASC_PRED_STATE_RELIGION:  return "TXT_KEY_CONDITION_STATE_RELIGION_TYPE";
		case CASC_PRED_IS_HOLY_CITY:    return "TXT_KEY_CONDITION_IS_HOLY_CITY_TYPE";
		case CASC_PRED_HAS_CORPORATION: return "TXT_KEY_CONDITION_HAS_CORPORATION_TYPE";
		case CASC_PRED_IS_HEADQUARTERS: return "TXT_KEY_CONDITION_IS_HEADQUARTERS_TYPE";
		case CASC_PRED_CIVIC_CATEGORY:  return "TXT_KEY_CONDITION_CIVIC_CATEGORY_TYPE";
		default:                        return NULL;
		}
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
			return gDLL->getText("TXT_KEY_CONDITION_UNIT_TAG", etx_prettyToken(szSuffix).GetCString());
		}
		case CASC_PRED_HAS_COAST:
			if (condition.min >= 0)
			{
				return gDLL->getText("TXT_KEY_CONDITION_HAS_COAST_MIN_AREA", condition.min);
			}
			break;
		case CASC_PRED_NATURE_YIELD:
			if (condition.id >= 0 && condition.id < NUM_YIELD_TYPES)
			{
				const CvWString szYieldIcon = CvWString::format(L"%c", GC.getYieldInfo((YieldTypes)condition.id).getChar());
				return gDLL->getText("TXT_KEY_CONDITION_NATURE_YIELD", condition.min, szYieldIcon.GetCString());
			}
			break;
		default:
			break;
		}
		// A predicate reads as the phrase a player would say ("in your capital"), never as its JSON token; a
		// parameterized one names its type.
		const char* szKey = condition.param.empty() ? etx_predicateKey(condition.predKind) : etx_predicateTypeKey(condition.predKind);
		if (szKey == NULL)
		{
			const char* szSpelling = cascadeSpellPredKind(condition.predKind);
			return (szSpelling[0] != '\0') ? CvWString(szSpelling) : CvWString(L"?");
		}
		if (condition.param.empty())
		{
			return gDLL->getText(szKey);
		}
		return gDLL->getText(szKey, etx_infoNameForType(condition.param, condition.id).GetCString());
	}

	bool etx_isGameOptionAtom(const CvCondition& condition)
	{
		return condition.kind == CASC_COND_PRESENCE && condition.type.compare(0, 11, "GAMEOPTION_") == 0;
	}

	enum GameOptionVerdict
	{
		OPTION_VERDICT_FALSE,
		OPTION_VERDICT_TRUE,
		OPTION_VERDICT_UNDECIDED   // depends on something other than a game option
	};

	// What the game's options ALONE decide about a condition. A game option is fixed for the whole game, so a
	// tooltip resolves it instead of printing it; every other atom stays undecided here and is rendered.
	GameOptionVerdict etx_gameOptionVerdict(const CvCondition* condition)
	{
		if (condition == NULL)
		{
			return OPTION_VERDICT_TRUE;
		}
		if (condition->kind != CASC_COND_GROUP)
		{
			if (!etx_isGameOptionAtom(*condition) || condition->id < 0)
			{
				return OPTION_VERDICT_UNDECIDED;
			}
			const bool bWantsAbsent = condition->max == 0 && condition->min <= 0;
			const bool bOptionOn = GC.getGame().isOption((GameOptionTypes)condition->id);
			return (bOptionOn != bWantsAbsent) ? OPTION_VERDICT_TRUE : OPTION_VERDICT_FALSE;
		}

		bool bUndecided = false;
		for (size_t iChild = 0; iChild < condition->all.size(); ++iChild)
		{
			const GameOptionVerdict eChild = etx_gameOptionVerdict(condition->all[iChild]);
			if (eChild == OPTION_VERDICT_FALSE)
			{
				return OPTION_VERDICT_FALSE;
			}
			bUndecided = bUndecided || eChild == OPTION_VERDICT_UNDECIDED;
		}
		if (!condition->anyOf.empty())
		{
			bool bAnyHolds = false;
			bool bAnyUndecided = false;
			for (size_t iChild = 0; iChild < condition->anyOf.size(); ++iChild)
			{
				const GameOptionVerdict eChild = etx_gameOptionVerdict(condition->anyOf[iChild]);
				bAnyHolds = bAnyHolds || eChild == OPTION_VERDICT_TRUE;
				bAnyUndecided = bAnyUndecided || eChild == OPTION_VERDICT_UNDECIDED;
			}
			if (!bAnyHolds && !bAnyUndecided)
			{
				return OPTION_VERDICT_FALSE;
			}
			bUndecided = bUndecided || !bAnyHolds;
		}
		for (size_t iChild = 0; iChild < condition->noneOf.size(); ++iChild)
		{
			const GameOptionVerdict eChild = etx_gameOptionVerdict(condition->noneOf[iChild]);
			if (eChild == OPTION_VERDICT_TRUE)
			{
				return OPTION_VERDICT_FALSE;
			}
			bUndecided = bUndecided || eChild == OPTION_VERDICT_UNDECIDED;
		}
		if (condition->enabled != NULL)
		{
			const GameOptionVerdict eEnabled = etx_gameOptionVerdict(condition->enabled);
			if (eEnabled == OPTION_VERDICT_FALSE)
			{
				return OPTION_VERDICT_FALSE;
			}
			bUndecided = bUndecided || eEnabled == OPTION_VERDICT_UNDECIDED;
		}
		if (condition->disabled != NULL)
		{
			const GameOptionVerdict eDisabled = etx_gameOptionVerdict(condition->disabled);
			if (eDisabled == OPTION_VERDICT_TRUE)
			{
				return OPTION_VERDICT_FALSE;
			}
			bUndecided = bUndecided || eDisabled == OPTION_VERDICT_UNDECIDED;
		}
		return bUndecided ? OPTION_VERDICT_UNDECIDED : OPTION_VERDICT_TRUE;
	}

	CvWString etx_presenceText(const CvCondition& condition)
	{
		if (etx_isGameOptionAtom(condition))
		{
			return CvWString();
		}
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
		case CASC_CONN_ONSITE:
			szText += L" on site";
			break;
		case CASC_CONN_NONE:
		default:
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
				const CvWString szChild = entryConditionText(condition.anyOf[iChild]);
				if (szChild.empty())
				{
					continue;
				}
				if (!szAny.empty())
				{
					szAny += L" or ";
				}
				szAny += szChild;
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
			const CvWString szChild = entryConditionText(condition.noneOf[iChild]);
			if (!szChild.empty())
			{
				parts.push_back(CvWString(L"not ") + szChild);
			}
		}
		if (condition.enabled != NULL)
		{
			parts.push_back(entryConditionText(condition.enabled));
		}
		if (condition.disabled != NULL)
		{
			const CvWString szDisabled = entryConditionText(condition.disabled);
			if (!szDisabled.empty())
			{
				parts.push_back(CvWString(L"not ") + szDisabled);
			}
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

CvWString entryClassificationName(const std::string& szKey)
{
	//	camelCase -> spaced words, first letter capitalised. A leading run of capitals is kept together so an
	//	authored acronym does not explode into single letters.
	CvWString szName;
	for (size_t iChar = 0; iChar < szKey.size(); ++iChar)
	{
		const char cChar = szKey[iChar];
		if (iChar == 0)
		{
			szName += (wchar_t)toupper(cChar);
			continue;
		}
		const bool bUpper = (cChar >= 'A' && cChar <= 'Z');
		const bool bPrevUpper = (szKey[iChar - 1] >= 'A' && szKey[iChar - 1] <= 'Z');
		if (bUpper && !bPrevUpper)
		{
			szName += L' ';
		}
		szName += (wchar_t)cChar;
	}
	return szName;
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

bool entryHiddenByGameOptions(const CvModEntry& entry)
{
	if (entry.enabled != NULL && etx_gameOptionVerdict(entry.enabled) == OPTION_VERDICT_FALSE)
	{
		return true;
	}
	return entry.disabled != NULL && etx_gameOptionVerdict(entry.disabled) == OPTION_VERDICT_TRUE;
}

bool entryIsPlainFlatChannel(const CvModEntry& entry)
{
	if (infoFamilyYield(entry.family) < 0 && infoFamilyCommerce(entry.family) < 0)
	{
		return false;
	}
	if (entry.unit != CASC_UNIT_FLAT || entry.kind != 0)
	{
		return false;
	}
	//	Anything the entry CARRIES disqualifies it: the carried half is the part worth reading, and a grouped
	//	line has nowhere to put it.
	if (entry.enabled != NULL || entry.disabled != NULL)
	{
		return false;
	}
	if (entry.unitQual != NULL || entry.religionQual != NULL)
	{
		return false;
	}
	if (entry.hasPer || entry.hasRankQual || entry.aiOnly)
	{
		return false;
	}
	return entry.targetFk < 0 && entry.targetSeg < 0 && entry.memberSeg < 0;
}

CvWString entryFlatSummary(const CvModEntry& entry)
{
	return etx_signedMagnitude(entry) + etx_entryName(entry);
}

CvWString yieldAmountSummary(YieldTypes eYield, int64_t iAmount)
{
	const CvYieldInfo& kYield = GC.getYieldInfo(eYield);
	return amountText(iAmount) + etx_channelLabel(kYield.getChar(), kYield, kYield.getType());
}

CvWString visionPlotsText(int iVisionAmount)
{
	return amountText(iVisionAmount / VISION_PLOT);
}

bool infoHasEntryConditionedOn(const CvInfo& info, CvCascPredKind ePredicate)
{
	const CvModifiers* pModifiers = info.getModifiers();
	if (pModifiers == NULL)
	{
		return false;
	}
	foreach_(const CvModEntry* pEntry, pModifiers->entries())
	{
		if (pEntry != NULL && CvConditionQuery::hasPredicate(pEntry->enabled, ePredicate))
		{
			return true;
		}
	}
	return false;
}

CvWString amountText(int64_t iAmount)
{
	const int64_t iMagnitude = (iAmount < 0) ? -iAmount : iAmount;
	const int64_t iWhole = iMagnitude / 100;
	const int64_t iFraction = iMagnitude % 100;
	const wchar_t* szSign = (iAmount < 0) ? L"-" : L"";
	if (iFraction == 0)
	{
		return CvWString::format(L"%s%I64d", szSign, iWhole);
	}
	if (iFraction % 10 == 0)
	{
		return CvWString::format(L"%s%I64d.%I64d", szSign, iWhole, iFraction / 10);
	}
	return CvWString::format(L"%s%I64d.%02I64d", szSign, iWhole, iFraction);
}

CvWString ratioText(int64_t iNumerator, int64_t iDenominator, int iDecimals)
{
	if (iDenominator <= 0)
	{
		return CvWString();
	}
	int64_t iScale = 1;
	for (int iDigit = 0; iDigit < iDecimals; iDigit++)
	{
		iScale *= 10;
	}
	const int64_t iMagnitude = (iNumerator < 0) ? -iNumerator : iNumerator;
	const int64_t iScaled = (2 * iMagnitude * iScale + iDenominator) / (2 * iDenominator);
	const wchar_t* szSign = (iNumerator < 0 && iScaled > 0) ? L"-" : L"";
	if (iDecimals <= 0)
	{
		return CvWString::format(L"%s%I64d", szSign, iScaled);
	}
	return CvWString::format(L"%s%I64d.%0*I64d", szSign, iScaled / iScale, iDecimals, iScaled % iScale);
}

CvWString unitStrengthText(int iCurrentStrength, int iFullStrength, bool bCurrentUnknown)
{
	const CvWString szGlyph = CvWString::format(L"%c", gDLL->getSymbolID(STRENGTH_CHAR));
	if (bCurrentUnknown)
	{
		return CvWString(L"?/") + amountText(iFullStrength) + szGlyph;
	}
	if (iCurrentStrength < iFullStrength)
	{
		return amountText(iCurrentStrength) + L"/" + amountText(iFullStrength) + szGlyph;
	}
	return amountText(iFullStrength) + szGlyph;
}

CvWString propertySourceLine(PropertyTypes eProperty, int iAmountPerTurn, bool bOnPlot)
{
	const CvPropertyInfo& kProperty = GC.getPropertyInfo(eProperty);
	CvWString szLine = CvWString::format(L"%+d", iAmountPerTurn);
	if (kProperty.getChar() > 0)
	{
		szLine += CvWString::format(L"%c", kProperty.getChar());
	}
	else
	{
		szLine += CvWString(L" ") + etx_infoBaseName(kProperty, kProperty.getType());
	}
	if (bOnPlot)
	{
		szLine += CvWString(L", ") + etx_scopePhrase(CASC_SCOPE_PLOT);
	}
	return szLine;
}

CvWString upkeepAmountText(int iUpkeep100)
{
	return amountText(iUpkeep100);
}

CvWString visionAmountText(int iSight)
{
	return visionPlotsText(iSight) + L" vision";
}

bool entryIsUnitBaseStat(const CvModEntry& entry)
{
	return (entry.family == MODFAM_STRENGTH || entry.family == MODFAM_MOVEMENT || entry.family == MODFAM_VISION)
		&& entry.scope == CASC_SCOPE_UNIT
		&& entry.unit == CASC_UNIT_FLAT
		&& entry.kind == 0
		&& !entry.isConditioned()
		&& !entry.aiOnly
		&& entry.targetFk < 0
		&& entry.targetSeg < 0;
}

CvWString entryUnitBaseStatText(const CvModEntry& entry)
{
	if (entry.family == MODFAM_VISION)
	{
		return visionAmountText(entry.value);
	}
	if (entry.family == MODFAM_STRENGTH)
	{
		return unitStrengthText(entry.value, entry.value, false);
	}
	const CvWString szAmount = amountText((entry.value < 0) ? -etx_absMagnitude(entry) : etx_absMagnitude(entry));
	const int iSymbol = gDLL->getSymbolID((entry.family == MODFAM_STRENGTH) ? STRENGTH_CHAR : MOVES_CHAR);
	return szAmount + CvWString::format(L"%c", iSymbol);
}

bool entryIsMembershipFlag(const CvModEntry& entry)
{
	//	The shape CvModifiers::parseLeaf compiles a `{TYPE: true}` keyed-container flag into, and the same test
	//	CvUnitInfo materializes its targeting sets from.
	return entry.family == MODFAM_COMBAT
		&& entry.scope == CASC_SCOPE_UNIT
		&& entry.unit == CASC_UNIT_COUNT
		&& entry.targetSeg >= 0
		&& entry.targetFk >= 0;
}

CvWString entryMembershipHeading(const CvModEntry& entry)
{
	static int iTargetsSeg = -1;
	static int iUnitTargetsSeg = -1;
	static int iDefendersSeg = -1;
	if (entry.targetSeg == modSegmentCached("targets", iTargetsSeg)
	|| entry.targetSeg == modSegmentCached("unitTargets", iUnitTargetsSeg))
	{
		return L"Targets first in combat";
	}
	if (entry.targetSeg == modSegmentCached("defenders", iDefendersSeg))
	{
		return L"Defends first against";
	}
	return entryClassificationName(modSegmentSpell(entry.targetSeg));
}

CvWString entryMembershipTarget(const CvModEntry& entry)
{
	return etx_targetName(entry);
}

CvWString hideAndSeekConcealmentLine(int iConcealment)
{
	return CvWString((iConcealment < 0) ? L"" : L"+") + visionPlotsText(iConcealment) + L" concealment";
}

CvWString hideAndSeekDetectionLine(int iMethodSkill, int iDetection)
{
	return CvWString((iDetection < 0) ? L"" : L"+") + visionPlotsText(iDetection)
		+ L" detection vs " + entryClassificationName(ClassificationRegistry::keyOf(CLSD_SKILL, iMethodSkill));
}

bool entryIsPlainCombatMember(const CvModEntry& entry)
{
	return entry.family == MODFAM_COMBAT
		&& entry.scope == CASC_SCOPE_UNIT
		&& !entry.isConditioned()
		&& !entry.aiOnly
		&& entry.targetFk < 0
		&& entry.targetSeg < 0;
}

CvWString entryCombatSummary(const CvModEntry& entry)
{
	//	The memberless combat percent modifies the unit's whole strength (the slot table's URS_STRENGTH_PERCENT).
	const CvWString szWhat = (entry.memberSeg >= 0) ? etx_memberWords(modSegmentSpell(entry.memberSeg)) : CvWString(L"strength");
	return etx_signedMagnitude(entry) + L" " + szWhat;
}

//	Is this condition NOTHING BUT a plain mention of the subject? Anything it carries beyond that -- a second
//	clause, a count, an upper bound, a connection requirement -- is information the line must keep, so it
//	answers false. A group wrapping exactly one `all` child is seen through.
static bool etx_isBareMentionOf(const CvCondition* pCondition, EnEdgeBucket eSubjectBucket, int iSubjectId)
{
	if (pCondition == NULL || eSubjectBucket == NO_EDGEB || iSubjectId < 0)
	{
		return false;
	}
	if (pCondition->kind == CASC_COND_PRESENCE)
	{
		return pCondition->id == iSubjectId
			&& CvConditionQuery::bucketForType(pCondition->type) == eSubjectBucket
			&& pCondition->min <= 1
			&& !pCondition->hasMax
			&& pCondition->connection == CASC_CONN_NONE;
	}
	if (pCondition->kind == CASC_COND_PREDICATE)
	{
		return pCondition->id == iSubjectId
			&& pCondition->predKind != CASC_PRED_UNKNOWN
			&& pCondition->predKind == CvConditionQuery::predicateForBucket(eSubjectBucket);
	}
	if (pCondition->all.size() != 1 || !pCondition->anyOf.empty() || !pCondition->noneOf.empty())
	{
		return false;
	}
	if (pCondition->enabled != NULL || pCondition->disabled != NULL)
	{
		return false;
	}
	return etx_isBareMentionOf(pCondition->all[0], eSubjectBucket, iSubjectId);
}

namespace
{
	///<summary>The one line body behind entryDetailLine and entryResolvedLine: bAuthored says iValue is the
	/// entry's own authored value (so its `per` clause still applies) rather than what the engine resolved it
	/// to.</summary>
	CvWString etx_detailLine(const CvModEntry& entry, EnEdgeBucket eSubjectBucket, int iSubjectId, bool bAuthored, int64_t iValue);
}

CvWString entryDetailLine(const CvModEntry& entry, EnEdgeBucket eSubjectBucket, int iSubjectId)
{
	return etx_detailLine(entry, eSubjectBucket, iSubjectId, true, entry.value);
}

CvWString entryResolvedLine(const CvModEntry& entry, int64_t iResolvedValue)
{
	return etx_detailLine(entry, NO_EDGEB, -1, false, iResolvedValue);
}

namespace
{
CvWString etx_detailLine(const CvModEntry& entry, EnEdgeBucket eSubjectBucket, int iSubjectId, bool bAuthored, int64_t iValue)
{
	CvWString szLine;

	// The wellbeing pair renders in the game's own icon idiom ("+2<happy>"): abs magnitude + the sign-routed
	// channel icon (modifier.md §2b -- the sign routes the channel; the icon carries the polarity).
	const bool bWellbeingIcon = (entry.family == MODFAM_HAPPINESS || entry.family == MODFAM_HEALTH)
	                         && entry.unit == CASC_UNIT_FLAT && entry.kind == 0;
	if (bWellbeingIcon)
	{
		const bool bGood = iValue >= 0;
		int iSymbol;
		if (entry.family == MODFAM_HAPPINESS)
		{
			iSymbol = gDLL->getSymbolID(bGood ? HAPPY_CHAR : UNHAPPY_CHAR);
		}
		else
		{
			iSymbol = gDLL->getSymbolID(bGood ? HEALTHY_CHAR : UNHEALTHY_CHAR);
		}
		szLine = CvWString(L"+") + amountText(etx_absMagnitudeOf(entry, iValue, bAuthored)) + CvWString::format(L"%c", iSymbol);
	}
	else if (entryIsMembershipFlag(entry))
	{
		// a membership flag carries no magnitude: the container says what the listed entity is to the unit
		szLine = entryMembershipHeading(entry) + L": " + etx_targetName(entry);
	}
	else if ((entry.family == MODFAM_STRENGTH || entry.family == MODFAM_MOVEMENT)
	      && entry.scope == CASC_SCOPE_UNIT && entry.unit == CASC_UNIT_FLAT && entry.kind == 0)
	{
		// a unit's own strength and moves render as the stat glyphs, the way the unit bar shows them ("+3<strength>")
		const int iSymbol = gDLL->getSymbolID((entry.family == MODFAM_STRENGTH) ? STRENGTH_CHAR : MOVES_CHAR);
		szLine = etx_signedMagnitudeOf(entry, iValue, bAuthored) + CvWString::format(L"%c", iSymbol);
	}
	else if (entry.family == MODFAM_PROPERTY && entry.propertyFk >= 0 && entry.unit == CASC_UNIT_FLAT
	      && GC.getPropertyInfo((PropertyTypes)entry.propertyFk).getChar() > 0)
	{
		// a property with a font glyph renders as it, the way the city bar shows crime or disease ("-4<crime>");
		// the glyph slot is 0 inside the load window, where the name stays (see etx_channelLabel)
		szLine = etx_signedMagnitudeOf(entry, iValue, bAuthored)
			+ CvWString::format(L"%c", GC.getPropertyInfo((PropertyTypes)entry.propertyFk).getChar());
	}
	else
	{
		szLine = etx_signedMagnitudeOf(entry, iValue, bAuthored) + L" " + etx_entryName(entry);
	}

	// the deposit's target: a named-entity key renders its resolved name; a plural object-target its token.
	// A family-scoped keyed container ([CvInfoKinds] INFO_FAMILY_TARGET_TOKENS) is part of what the entry means,
	// so it is named before the target ("+100% combat flanking for Siege"); a plain container like `buildings`
	// only says the key is a building, which the name already does.
	bool bPluralTarget = false;
	const char* szReachKey = etx_cityReachKey(entry);
	if (entryIsMembershipFlag(entry))
	{
		// the heading above already named the container and the target
	}
	else if (entry.targetFk >= 0)
	{
		if (entry.targetSeg >= 0 && infoIsFamilyTargetToken(entry.family, modSegmentSpell(entry.targetSeg)))
		{
			szLine += CvWString(L" ") + etx_memberWords(modSegmentSpell(entry.targetSeg));
		}
		szLine += CvWString(L" for ") + etx_targetName(entry);
	}
	else if (entry.targetSeg >= 0)
	{
		bPluralTarget = true;
		// "on cities" is the reach the city-check phrase below already states ("in your capital")
		if (szReachKey == NULL || entry.targetSeg != modSegmentLookup("cities"))
		{
			szLine += CvWString(L" on ") + CvWString(modSegmentSpell(entry.targetSeg));
		}
	}

	if (szReachKey != NULL)
	{
		szLine += CvWString(L" ") + gDLL->getText(szReachKey);
	}
	// The scope, where non-obvious (city is the default containing scope).
	// ⛔ A PLURAL TARGET ALREADY STATES THE REACH, so the bare scope word is NOT appended beside it (json.md
	// §3.3: the scope is the REACH and the plural target is "all objects of that kind IN that scope" -- the two
	// are one statement, never two). Printing both turns a per-object deposit into a claim about the whole
	// scope: a specialist's `{y}.empire.cities.flat` read "on cities, empire-wide", which describes an
	// empire-wide lump when the deposit actually lands in the ONE city holding that specialist.
	else if (!bPluralTarget)
	{
		const wchar_t* szScopePhrase = etx_scopePhrase(entry.scope);
		if (szScopePhrase[0] != L'\0')
		{
			szLine += CvWString(L", ") + szScopePhrase;
		}
	}

	if (entry.hasPer && bAuthored)
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
	// A condition that only names the kind of unit reads "for Military units" on its own; "while" belongs to a
	// state that holds or does not ("while in your capital").
	const CvWString szEnabled = (szReachKey != NULL) ? CvWString() : entryConditionText(entry.enabled);
	if (!szEnabled.empty() && !etx_isBareMentionOf(entry.enabled, eSubjectBucket, iSubjectId))
	{
		szLine += (etx_isUnitTagOnly(entry.enabled) ? CvWString(L" -- ") : CvWString(L" -- while ")) + szEnabled;
	}
	const CvWString szDisabled = entryConditionText(entry.disabled);
	if (!szDisabled.empty())
	{
		szLine += (etx_isUnitTagOnly(entry.disabled) ? CvWString(L" -- except ") : CvWString(L" -- unless ")) + szDisabled;
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
}
