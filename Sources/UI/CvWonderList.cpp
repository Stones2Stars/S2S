#include "CvGameCoreDLL.h"
#include "CvWonderList.h"
#include "Engine/CvCity.h"
#include "Engine/CvGameCoreUtils.h"
#include "Defines/CvGlobals.h"
#include "AI/CvPlayerAI.h"
#include "AI/CvTeamAI.h"

namespace
{
	///<summary>Built rows the most recent first; an undated row (a project) after every dated one; the higher id
	/// first between equals.</summary>
	struct wl_MostRecentFirst
	{
		bool operator()(const WonderRow& left, const WonderRow& right) const
		{
			if (left.bHasYear != right.bHasYear)
			{
				return left.bHasYear;
			}
			if (left.bHasYear && left.iYear != right.iYear)
			{
				return left.iYear > right.iYear;
			}
			return left.iType > right.iType;
		}
	};

	///<summary>A row with no year: the builder (NO_PLAYER when unknown to the viewer) and the city it names (NULL
	/// for none).</summary>
	WonderRow wl_row(int iType, bool bUnderConstruction, PlayerTypes eBuilder, const CvCity* pShownCity)
	{
		WonderRow row;
		row.iType = iType;
		row.bUnderConstruction = bUnderConstruction;
		row.bHasYear = false;
		row.iYear = 0;
		row.eBuilder = eBuilder;
		row.eCityOwner = pShownCity != NULL ? pShownCity->getOwner() : NO_PLAYER;
		row.iCityId = pShownCity != NULL ? pShownCity->getID() : -1;
		row.bZoomable = pShownCity != NULL && eBuilder != NO_PLAYER;
		return row;
	}

	///<summary>What eViewer may see of city's production: its own team's, or a rival city revealed to it and open
	/// to it (CvCity::isOpenTo).</summary>
	bool wl_isProductionVisibleTo(const CvCity& city, PlayerTypes eViewer)
	{
		const TeamTypes eViewerTeam = GET_PLAYER(eViewer).getTeam();
		if (city.getTeam() == eViewerTeam)
		{
			return true;
		}
		return city.isRevealed(eViewerTeam, false) && city.isOpenTo(eViewer);
	}

	///<summary>One row per project each living team has completed; the builder is the team's leader, named for the
	/// viewer's own team and the teams it has met.</summary>
	void wl_addProjectRows(PlayerTypes eViewer, std::vector<WonderRow>& built)
	{
		const TeamTypes eViewerTeam = GET_PLAYER(eViewer).getTeam();
		const CvTeam& viewerTeam = GET_TEAM(eViewerTeam);
		for (int iTeam = 0; iTeam < MAX_PC_TEAMS; iTeam++)
		{
			const CvTeam& team = GET_TEAM((TeamTypes)iTeam);
			if (!team.isAlive())
			{
				continue;
			}
			const bool bKnown = iTeam == eViewerTeam || viewerTeam.isHasMet((TeamTypes)iTeam);
			const PlayerTypes eBuilder = bKnown ? team.getLeaderID() : NO_PLAYER;
			for (int iProject = 0; iProject < GC.getNumProjectInfos(); iProject++)
			{
				const int iCount = team.getProjectCount((ProjectTypes)iProject);
				for (int iCopy = 0; iCopy < iCount; iCopy++)
				{
					built.push_back(wl_row(iProject, false, eBuilder, NULL));
				}
			}
		}
	}
}

bool isOnWonderList(BuildingTypes eBuilding, WonderListKind eKind)
{
	const int iScope = wonderScope(eBuilding);
	switch (eKind)
	{
	case WONDER_LIST_WORLD:
		return iScope == ALLOWEDCAP_WORLD;
	case WONDER_LIST_NATIONAL:
		return iScope == ALLOWEDCAP_TEAM || iScope == ALLOWEDCAP_EMPIRE;
	default:
		return false;
	}
}

void getCityWonders(const CvCity& city, WonderListKind eKind, std::vector<BuildingTypes>& wonders)
{
	foreach_(const BuildingTypes eBuilding, city.getHasBuildings())
	{
		if (isOnWonderList(eBuilding, eKind))
		{
			wonders.push_back(eBuilding);
		}
	}
	std::sort(wonders.begin(), wonders.end());
}

void getWonderRows(PlayerTypes eViewer, WonderListKind eKind, std::vector<WonderRow>& rows)
{
	const TeamTypes eViewerTeam = GET_PLAYER(eViewer).getTeam();
	const CvTeam& viewerTeam = GET_TEAM(eViewerTeam);
	std::vector<WonderRow> built;

	for (int iPlayer = 0; iPlayer < MAX_PC_PLAYERS; iPlayer++)
	{
		const CvPlayer& player = GET_PLAYER((PlayerTypes)iPlayer);
		if (!player.isAlive())
		{
			continue;
		}
		const bool bOwnTeam = player.getTeam() == eViewerTeam;
		const bool bMet = viewerTeam.isHasMet(player.getTeam());

		foreach_(const CvCity* pCity, player.cities())
		{
			const bool bRevealed = pCity->isRevealed(eViewerTeam, false);
			const CvCity* pShownCity = bRevealed ? pCity : NULL;

			if (wl_isProductionVisibleTo(*pCity, eViewer))
			{
				if (eKind == WONDER_LIST_PROJECTS && pCity->isProductionProject())
				{
					rows.push_back(wl_row(pCity->getProductionProject(), true, (PlayerTypes)iPlayer, pCity));
				}
				else if (eKind != WONDER_LIST_PROJECTS && pCity->isProductionBuilding()
					&& isOnWonderList(pCity->getProductionBuilding(), eKind))
				{
					rows.push_back(wl_row(pCity->getProductionBuilding(), true, (PlayerTypes)iPlayer, pCity));
				}
			}

			if (eKind == WONDER_LIST_PROJECTS)
			{
				continue;
			}
			std::vector<BuildingTypes> wonders;
			getCityWonders(*pCity, eKind, wonders);
			foreach_(const BuildingTypes eBuilding, wonders)
			{
				if (eKind == WONDER_LIST_NATIONAL && !bOwnTeam && !(bMet && bRevealed))
				{
					continue;
				}
				const PlayerTypes eBuilder = bOwnTeam || bMet ? (PlayerTypes)iPlayer : NO_PLAYER;
				WonderRow row = wl_row(eBuilding, false, eBuilder, pShownCity);
				row.bHasYear = true;
				row.iYear = pCity->getBuildingData(eBuilding).iTimeBuilt;
				built.push_back(row);
			}
		}
	}

	if (eKind == WONDER_LIST_PROJECTS)
	{
		wl_addProjectRows(eViewer, built);
	}
	std::stable_sort(built.begin(), built.end(), wl_MostRecentFirst());
	rows.insert(rows.end(), built.begin(), built.end());
}
