//
//	TechEnabler -- StoneBase CalculateAvailableTechs.cs (see the header): the tech availability generate/gate,
//	a declared surface over the ONE EnablerKernel primitive (the single-source law, patterns.md).
//

#include "CvGameCoreDLL.h"
#include "CvTechEnabler.h"
#include "CvInfo.h"
#include "Repos/InfoRepo.h"
#include "AI/CvTeamAI.h"             // GET_TEAM
#include "Defines/CvGlobals.h"
#include "Engine/CvPlayer.h"
#include "Engine/CvTeam.h"
#include "CvEnablerKernel.h"           // EnablerKernel::generate + EnBucketSets -- the enable-side GENERATE
#include "CvTechInfo.h"

// --- STAGE 1: the ENABLE-SIDE CAN GET only (enabler.md §1-2). CAN GET = union(enables.techs) - (disables/obsoletes/
// replaces) over HAVE, minus already-held. This is the GENERATE pass; the requires.build GATE + the allowed cap are
// LATER stages, so this set is DELIBERATELY OVER-INCLUSIVE -- a multi-prereq tech appears once ANY ONE prereq is held
// (the `enables` OR; `requires.build.all` will confirm the AND in the gate stage). Event-driven: recomputed when a
// tech-HAVE event marks the frontier (enabler.md §7 recompute-on-HAVE-change) -- no poll, no self-heal.
void TechEnabler::available(const CvPlayer& kPlayer, const CvTeam& kTeam, std::set<int>& avail)
{
	EnBucketSets cand;
	EnablerKernel::generate(kPlayer, NULL, cand);   // union(enables) - removals over HAVE (start node + held techs + civics)
	const std::set<int>& techs = cand["techs"];
	for (std::set<int>::const_iterator it = techs.begin(); it != techs.end(); ++it)
	{
		const int t = *it;
		if (kTeam.isHasTech((TechTypes)t)) continue;              // already held -> not a CAN GET candidate
		if (GC.getTechInfo((TechTypes)t).isDisable()) continue;   // the reversible law-ban (the Neanderthal research ban)
		avail.insert(t);
	}
}
