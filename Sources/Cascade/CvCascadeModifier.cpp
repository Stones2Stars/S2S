//
//	CvCascadeModifier -- the magnitude combine core (see CvCascadeModifier.h).
//

#include "CvGameCoreDLL.h"
#include "CvCascadeModifier.h"

void CvModifierSlot::deposit(ModifierUnit eUnit, int iValue)
{
	switch (eUnit)
	{
	case MODUNIT_FLAT:       iFlat += iValue;                              break;
	case MODUNIT_PERCENT:    iPercent += iValue;                          break;
	case MODUNIT_MULTIPLIER: iMultiplierX100 = iMultiplierX100 * iValue / 100; break; // compose by product
	}
}

int CvModifierSlot::effective(int iBase) const
{
	// (base + Σflat) × (100 + Σpercent)/100 × Π(multiplier/100). Int math, matching the legacy accumulators;
	// the precise ordering / overflow / combine-mode handling is the #430 arithmetic pass.
	int iValue = iBase + iFlat;
	iValue = iValue * (100 + iPercent) / 100;
	iValue = iValue * iMultiplierX100 / 100;
	return iValue;
}
