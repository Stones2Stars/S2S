//
//	CvGameSpeedInfo -- the gamespeed poco's exemplar reads (see the header). The legacy scalar MIRRORS are
//	DEAD (wave D): the base dispatch compiles the two authored world percents into m_modifiers and every read
//	is a compiled-slot fetch through the base getScalar (docs/architecture/patterns.md §Materialize at mapFrom -- no raw-JSON family
//	walker survives). The derived era-pacing reads below consume ONLY info data (this speed percent +
//	CvEraInfo's year span / Normal-speed turn count).
//

#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson
#include "CvInfos.h"              // umbrella: keeps the unity batch's info-type defs whole (leakage guard)
#include "AI/CvGameAI.h"
#include "Defines/CvGlobals.h"
#include "CvEraInfo.h"
#include "CvGameSpeedInfo.h"


CvGameSpeedInfo::CvGameSpeedInfo()
{
}


namespace
{
	// This speed's pace percent (normal = 100). A PERCENT IS NOT SCALED (docs/specs/curators/fixed-point-and-scales.md §1 (the x100 fixed-point model)), so the read is
	// already the human percent and NOTHING is reduced here.
	// ⚠ Takes the info explicitly, so it answers "THIS speed's percent" -- CvGameSpeedScale::speedPercent()
	// answers "the RUNNING game's", and is what a live-state caller must use instead of re-reading the scalar.
	int gs_speedPercent(const CvGameSpeedInfo& kSpeed)
	{
		return kSpeed.getScalar(SCALAR_SPEED, CASC_SCOPE_WORLD, CASC_UNIT_PERCENT);
	}
}


int CvGameSpeedInfo::getTurnsInEra(int iEra) const
{
	FASSERT_BOUNDS(0, GC.getNumEraInfos(), iEra);
	return std::max(1, (GC.getEraInfo((EraTypes)iEra).getNormalSpeedTurns() * gs_speedPercent(*this) + 50) / 100);
}


int CvGameSpeedInfo::getEraStartTurn(int iEra) const
{
	FASSERT_BOUNDS(0, GC.getNumEraInfos(), iEra);
	int iTurn = 0;
	for (int iPriorEra = 0; iPriorEra < iEra; iPriorEra++)
	{
		iTurn += getTurnsInEra(iPriorEra);
	}
	return iTurn;
}


int CvGameSpeedInfo::getTotalTurns() const
{
	int iTurns = 0;
	for (int iEra = 0; iEra < GC.getNumEraInfos(); iEra++)
	{
		iTurns += getTurnsInEra(iEra);
	}
	return iTurns;
}


// Calendar ticks (days; 30/month, 360/year) that one turn advances within the era.
int CvGameSpeedInfo::getTicksPerTurnInEra(int iEra) const
{
	FASSERT_BOUNDS(0, GC.getNumEraInfos(), iEra);
	const CvEraInfo& kEra = GC.getEraInfo((EraTypes)iEra);
	const int iSpanTicks = (kEra.getHistoricalEndYear() - kEra.getHistoricalStartYear()) * 360;
	const int iTurns = getTurnsInEra(iEra);
	return std::max(1, (iSpanTicks + iTurns / 2) / iTurns);
}


// #430: the JSON load hook. The §6 families (speed.world.percent + missionYieldMultiplier.world.percent)
// compile into m_modifiers via the base section dispatch -- nothing type-specific to materialize here.
void CvGameSpeedInfo::mapFrom(const picojson::value& entity)
{
	CvInfo::mapFrom(entity);   // core reading (type / text keys / button) + the section dispatch (compiles m_modifiers)
}
