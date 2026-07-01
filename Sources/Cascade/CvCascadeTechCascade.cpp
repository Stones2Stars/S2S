//
//	TechCascade -- StoneBase CalculateAvailableTechs.cs (see the header). Ported VERBATIM from CvCascadeEnabler.cpp's
//	file-static en_techAvailable; promoted to a declared surface (the single-source law, patterns.md). LOGIC unchanged:
//	only the signature was rewritten.
//

#include "CvGameCoreDLL.h"
#include "CvCascadeTechCascade.h"
#include "CvJsonInfo.h"
#include "Repos/InfoRepo.h"
#include "AI/CvTeamAI.h"             // GET_TEAM
#include "Defines/CvGlobals.h"
#include "Engine/CvPlayer.h"
#include "Engine/CvTeam.h"
#include "CvCascadeConditionEval.h"   // cascadeEvalCondition
#include "Infos/CvTechInfo.h"

// --- TechCascade.cs: a tech is available iff not disabled, not held, under allowed.world, requires.build holds.
// (Default flags -- TechCascade uses `new ConditionEvaluator()`.) The all-techs+requires set is "researchable now".
void TechCascade::available(const CvPlayer& kPlayer, const CvTeam& kTeam, std::set<int>& avail)
{
	CvCascadeEvalCtx ec; ec.player = &kPlayer; ec.team = &kTeam;
	CvCascadeEvalFlags flags;   // default (NOT strict) -- mirrors StoneBase TechCascade's plain evaluator
	const int nT = GC.getNumTechInfos();
	for (int t = 0; t < nT; ++t)
	{
		if (GC.getTechInfo((TechTypes)t).isDisable()) continue;        // IsDisabled
		if (kTeam.isHasTech((TechTypes)t)) continue;                   // held
		const CvJsonInfo* j = InfoRepo<CvTechInfo>::get().get(t);
		if (j != NULL)
		{
			std::map<std::string, int>::const_iterator w = j->allowed.find("world");
			if (w != j->allowed.end())                                 // world cap (rare: a globally-unique tech)
			{
				int held = 0;
				for (int tm = 0; tm < MAX_TEAMS; ++tm)
					if (GET_TEAM((TeamTypes)tm).isAlive() && GET_TEAM((TeamTypes)tm).isHasTech((TechTypes)t)) ++held;
				if (held >= w->second) continue;
			}
			if (j->requiresBuild != NULL && !cascadeEvalCondition(j->requiresBuild, ec, flags)) continue;
		}
		avail.insert(t);
	}
}
