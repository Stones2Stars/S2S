//
//	CvPromotionAccrual -- see the header. Both reads walk the load-materialized accrual list, which always holds
//	at least the promotion itself, so neither needs an is-there-a-line branch.
//

#include "CvGameCoreDLL.h"   // PCH umbrella -- GC
#include "CvPromotionAccrual.h"
#include "CvPromotionInfo.h"
#include "CvModifiers.h"

int CvPromotionAccrual::sum(const CvPromotionInfo& kPromotion, ModifierFamily eFamily, int iKind,
                            CvCascScope eScope, CvCascUnit eUnit)
{
	const std::vector<int>& accrual = kPromotion.getLineAccrual();
	int iTotal = 0;
	for (size_t iRung = 0; iRung < accrual.size(); ++iRung)
	{
		const CvPromotionInfo& kRung = GC.getPromotionInfo((PromotionTypes)accrual[iRung]);
		iTotal += kRung.getModifiers()->sum(eFamily, iKind, eScope, eUnit);
	}
	return iTotal;
}

bool CvPromotionAccrual::skill(const CvPromotionInfo& kPromotion, SkillRead fnRead)
{
	const std::vector<int>& accrual = kPromotion.getLineAccrual();
	for (size_t iRung = 0; iRung < accrual.size(); ++iRung)
	{
		const CvPromotionInfo& kRung = GC.getPromotionInfo((PromotionTypes)accrual[iRung]);
		if (fnRead(kRung.getSkills()))
		{
			return true;
		}
	}
	return false;
}
