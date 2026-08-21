//
//	CvHideAndSeekSection -- the shared load-time parse of the json.md par.9 `hideAndSeek` block (see the header).
//	Idempotent (clear-first). Values are lifted onto the ×100 native scale here, at the readJson IN boundary.
//

#include "CvGameCoreDLL.h"   // PCH umbrella -- picojson
#include "CvHideAndSeekSection.h"
#include "CvClassificationRegistry.h"   // typeName(CLSD_SKILL, ...) -- the ONE camelCase->SKILL_* spelling
#include "CvInfoKinds.h"     // VISION_PLOT / VISION_OPEN_GROUND_COST -- the ONE vision scale
#include "CvJsonParse.h"     // jsonChildObj / jsonIdInt / jsonIdStr
#include "Defines/CvGlobals.h"

namespace
{
	// The authored `{unit: HAS_<SKILL>}` qualifier -> the SKILL_* infotype the classification registry mints.
	// The prefix swap is the whole of it: the qualifier says HAS_ because a skill is something a unit HAS
	// (json.md §3.5), and the minted id says SKILL_ because that is its category.
	bool hs_methodTypeFromQualifier(const std::string& szQualifier, std::string& szTypeOut)
	{
		static const char* HAS_PREFIX = "HAS_";
		const size_t iPrefix = 4;
		if (szQualifier.size() <= iPrefix || szQualifier.compare(0, iPrefix, HAS_PREFIX) != 0)
		{
			return false;
		}
		szTypeOut = "SKILL_" + szQualifier.substr(iPrefix);
		return true;
	}

	// One `{value, unit}` seeker row. A row whose qualifier is missing or malformed is DROPPED rather than
	// silently made universal: an unqualified detection would answer every method at once, which is the one
	// wrong answer the pairing exists to prevent.
	void hs_readDetectionRow(const picojson::object& rowObj, std::vector<CvDetectionRow>& out)
	{
		std::string szQualifier;
		if (!jsonIdStr(rowObj, "unit", szQualifier))
		{
			return;
		}
		CvDetectionRow row;
		if (!hs_methodTypeFromQualifier(szQualifier, row.methodSkillType))
		{
			return;
		}
		row.value = jsonIdInt(rowObj, "value") * VISION_PLOT;
		out.push_back(row);
	}
}

CvHideAndSeekSection::CvHideAndSeekSection()
	: concealment(0), classicMethod(-1)
{
}

void CvHideAndSeekSection::clearParsed()
{
	concealment = 0;
	detection.clear();
	classicMethodType.clear();
	classicMethod = -1;
}

void CvHideAndSeekSection::parse(const picojson::value& entity)
{
	clearParsed();
	if (!entity.is<picojson::object>())
	{
		return;
	}
	const picojson::object& entityObj = entity.get<picojson::object>();
	const picojson::object* pBlock = jsonChildObj(entityObj, "hideAndSeek");
	if (pBlock == NULL)
	{
		return;
	}

	// the CLASSIC method: `method: "camouflage"` -- the legacy single-tag system's own datum, a skill name
	// resolved to its minted SKILL_* id on first ask (the rows' lazy shape). Absent = classically never-invisible.
	std::string szClassicMethod;
	if (jsonIdStr(*pBlock, "method", szClassicMethod) && !szClassicMethod.empty())
	{
		classicMethodType = ClassificationRegistry::typeName(CLSD_SKILL, szClassicMethod);
	}

	// the hider: `concealment: { flat: N }` -- one memberless magnitude, signed
	const picojson::object* pConcealment = jsonChildObj(*pBlock, "concealment");
	if (pConcealment != NULL)
	{
		concealment = jsonIdInt(*pConcealment, "flat") * VISION_PLOT;
	}

	// the seeker: `detection` is ONE row or a LIST of them (the §3.9 leaf-or-list shape)
	const picojson::object::const_iterator itDetection = pBlock->find("detection");
	if (itDetection != pBlock->end())
	{
		const picojson::value& kDetection = itDetection->second;
		if (kDetection.is<picojson::array>())
		{
			const picojson::array& kRows = kDetection.get<picojson::array>();
			for (size_t iRow = 0; iRow < kRows.size(); ++iRow)
			{
				if (kRows[iRow].is<picojson::object>())
				{
					hs_readDetectionRow(kRows[iRow].get<picojson::object>(), detection);
				}
			}
		}
		else if (kDetection.is<picojson::object>())
		{
			hs_readDetectionRow(kDetection.get<picojson::object>(), detection);
		}
	}
}

int CvHideAndSeekSection::resolveMethodSkill(const CvDetectionRow& kRow) const
{
	if (kRow.methodSkill < 0 && !kRow.methodSkillType.empty())
	{
		// lazy, like the IS_TAG predicate: the SKILL_* infotypes mint after the entities parse
		kRow.methodSkill = GC.getInfoTypeForString(kRow.methodSkillType.c_str(), /*bHideAssert*/true);
	}
	return kRow.methodSkill;
}

int CvHideAndSeekSection::classicMethodSkill() const
{
	if (classicMethod < 0 && !classicMethodType.empty())
	{
		// the same lazy resolve as the rows; a re-lookup while negative is the standing registry convention
		classicMethod = GC.getInfoTypeForString(classicMethodType.c_str(), /*bHideAssert*/true);
	}
	return classicMethod;
}

int CvHideAndSeekSection::detectionAgainst(int iMethodSkillId) const
{
	if (iMethodSkillId < 0)
	{
		return 0;
	}
	int iTotal = 0;
	for (size_t iRow = 0; iRow < detection.size(); ++iRow)
	{
		if (resolveMethodSkill(detection[iRow]) == iMethodSkillId)
		{
			iTotal += detection[iRow].value;
		}
	}
	return iTotal;
}

void CvHideAndSeekSection::collectDetectionInto(std::vector<std::pair<int, int> >& aResolvedRowsOut) const
{
	for (size_t iRow = 0; iRow < detection.size(); ++iRow)
	{
		const int iMethodSkill = resolveMethodSkill(detection[iRow]);
		if (iMethodSkill >= 0)
		{
			aResolvedRowsOut.push_back(std::make_pair(iMethodSkill, detection[iRow].value));
		}
	}
}
