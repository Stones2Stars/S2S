#include "CvGameCoreDLL.h"
#include "WorthLedger.h"

void WorthLedger::change(int64_t iMilitaryWorth, int64_t iWorth)
{
	militaryWorth += iMilitaryWorth;
	worth += iWorth;
	FASSERT_NOT_NEGATIVE(militaryWorth);
	FASSERT_NOT_NEGATIVE(worth);
}

void WorthLedger::clear()
{
	militaryWorth = 0;
	worth = 0;
}
