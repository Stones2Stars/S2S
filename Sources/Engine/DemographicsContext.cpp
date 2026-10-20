//
//	DemographicsContext -- the empire's power and assets ledgers (see the header for the rulings).
//

#include "CvGameCoreDLL.h"
#include "DemographicsContext.h"
#include "Spine/CvEventSpine.h"
#include "AI/CvPlayerAI.h"                     // GET_PLAYER
#include "Defines/CvGlobals.h"                 // GC -- the tech and building infos, the map
#include "Engine/CvArea.h"
#include "Engine/CvCity.h"
#include "Engine/CvMap.h"
#include "Engine/CvPlot.h"
#include "Engine/CvUnit.h"
#include "Infos/CvBuildingInfo.h"
#include "Infos/CvTechInfo.h"

void DemographicsContext::clear()
{
	m_units.clear();
	m_techs.clear();
	m_buildings.clear();
}

namespace
{
	CvPlayer* dc_player(int iPlayer)
	{
		if (iPlayer < 0 || iPlayer >= MAX_PLAYERS)
		{
			return NULL;
		}
		return &GET_PLAYER((PlayerTypes)iPlayer);
	}

	//	The area a plot stands in, or NULL off the map.
	CvArea* dc_areaOfPlot(int iPlot)
	{
		if (iPlot < 0 || iPlot >= GC.getMap().numPlots())
		{
			return NULL;
		}
		return GC.getMap().plotByIndex(iPlot)->area();
	}

	void dc_changeAreaWorth(CvArea* pArea, int iPlayer, int iChange)
	{
		if (pArea != NULL && iPlayer >= 0 && iPlayer < MAX_PLAYERS && iChange != 0)
		{
			pArea->changeMilitaryWorth((PlayerTypes)iPlayer, iChange);
		}
	}

	//	A PLOT moved between areas: its units and its city's operating buildings move with it. The sign is the
	//	fact's -- REMOVED fires while the plot still stands in the old area, ADDED once it stands in the new one.
	void dc_movePlotWorth(int iArea, int iPlot, int iSign)
	{
		if (iPlot < 0 || iPlot >= GC.getMap().numPlots())
		{
			return;
		}
		CvArea* pArea = GC.getMap().getArea(iArea);
		const CvPlot* pPlot = GC.getMap().plotByIndex(iPlot);
		foreach_(const CvUnit* pUnit, pPlot->units())
		{
			dc_changeAreaWorth(pArea, (int)pUnit->getOwner(), iSign * pUnit->getPowerValueTotal());
		}
		const CvCity* pCity = pPlot->getPlotCity();
		if (pCity != NULL)
		{
			dc_changeAreaWorth(pArea, (int)pCity->getOwner(), iSign * pCity->buildingWorth().militaryWorth);
		}
	}

	//	The WHOLESALE area fact: every area was re-created empty, so every area ledger is booked again from what
	//	stands on the map.
	void dc_rebookAreas()
	{
		for (int iPlayer = 0; iPlayer < MAX_PLAYERS; iPlayer++)
		{
			const CvPlayer& kPlayer = GET_PLAYER((PlayerTypes)iPlayer);
			foreach_(const CvUnit* pUnit, kPlayer.units())
			{
				if (pUnit->plot() != NULL)
				{
					dc_changeAreaWorth(pUnit->plot()->area(), iPlayer, pUnit->getPowerValueTotal());
				}
			}
			foreach_(const CvCity* pCity, kPlayer.cities())
			{
				dc_changeAreaWorth(pCity->area(), iPlayer, pCity->buildingWorth().militaryWorth);
			}
		}
	}

	//	The dispatch object the spine registers. It owns NO state -- the state is on the players, cities and areas.
	class DemographicsSpineConsumer : public IEventConsumer
	{
	public:
		int wantedKinds() const { return (1 << EVENTKIND_DOMAIN); }
		void onEvent(const CvSpineEvent& kEvent) { DemographicsContext::onSpineEvent(kEvent); }
	};

	DemographicsSpineConsumer s_demographicsConsumer;
	bool s_bDemographicsRegistered = false;
}

// ⚖ THE DECLARED INTEREST SET. Everything that moves a power or assets ledger is named here, at the store.
bool DemographicsContext::wantsEvent(int iEventId)
{
	switch (iEventId)
	{
	// A unit's counted worth: birth, Size Matters re-rank, death, and the end of its read.
	case SEVT_UNIT_MILITARY_WORTH_ADDED:
	case SEVT_UNIT_MILITARY_WORTH_REMOVED:
	case SEVT_UNIT_WORTH_ADDED:
	case SEVT_UNIT_WORTH_REMOVED:
	// A unit carrying its worth from one area into another.
	case SEVT_UNIT_AREA_ADDED:
	case SEVT_UNIT_AREA_REMOVED:
	// A held tech, once per held count (CvPlayer::read announces the save's).
	case SEVT_EMPIRE_TECH_ADDED:
	case SEVT_EMPIRE_TECH_REMOVED:
	// The OPERATE crossing -- a dormant building is worth nothing, and a city's death dorms every building
	// under its old owner before the population leaves, so a capture moves the worth through these alone.
	case SEVT_CITY_BUILDING_ACTIVATED:
	case SEVT_CITY_BUILDING_DORMANTED:
	// A plot moving between areas one at a time, and the wholesale reassignment.
	case SEVT_AREA_TILE_ADDED:
	case SEVT_AREA_TILE_REMOVED:
	case SEVT_AREAS_RECALCULATED:
		return true;
	default:
		return false;
	}
}

void DemographicsContext::onSpineEvent(const CvSpineEvent& kEvent)
{
	if (!wantsEvent(kEvent.iEventId))
	{
		return;
	}
	switch (kEvent.iEventId)
	{
	case SEVT_UNIT_MILITARY_WORTH_ADDED:
	case SEVT_UNIT_MILITARY_WORTH_REMOVED:
	{
		const int iChange = (kEvent.iEventId == SEVT_UNIT_MILITARY_WORTH_ADDED) ? kEvent.iA : -kEvent.iA;
		CvPlayer* pPlayer = dc_player(kEvent.iC);
		if (pPlayer != NULL)
		{
			pPlayer->demographics().units().change(iChange, 0);
		}
		dc_changeAreaWorth(dc_areaOfPlot(kEvent.iSrcLoc), kEvent.iC, iChange);
		break;
	}
	case SEVT_UNIT_WORTH_ADDED:
	case SEVT_UNIT_WORTH_REMOVED:
	{
		CvPlayer* pPlayer = dc_player(kEvent.iC);
		if (pPlayer != NULL)
		{
			pPlayer->demographics().units().change(0, (kEvent.iEventId == SEVT_UNIT_WORTH_ADDED) ? kEvent.iA : -kEvent.iA);
		}
		break;
	}
	case SEVT_UNIT_AREA_ADDED:
	case SEVT_UNIT_AREA_REMOVED:
		dc_changeAreaWorth(GC.getMap().getArea(kEvent.iB), kEvent.iC,
			(kEvent.iEventId == SEVT_UNIT_AREA_ADDED) ? kEvent.iA : -kEvent.iA);
		break;
	case SEVT_EMPIRE_TECH_ADDED:
	case SEVT_EMPIRE_TECH_REMOVED:
	{
		CvPlayer* pPlayer = dc_player(kEvent.iC);
		if (pPlayer != NULL && kEvent.iType >= 0 && kEvent.iType < GC.getNumTechInfos())
		{
			const CvTechInfo& kTech = GC.getTechInfo((TechTypes)kEvent.iType);
			const int iSign = (kEvent.iEventId == SEVT_EMPIRE_TECH_ADDED) ? 1 : -1;
			pPlayer->demographics().techs().change(iSign * kTech.getMilitaryWorth(), iSign * kTech.getWorth());
		}
		break;
	}
	case SEVT_CITY_BUILDING_ACTIVATED:
	case SEVT_CITY_BUILDING_DORMANTED:
	{
		CvPlayer* pPlayer = dc_player(kEvent.iC);
		CvCity* pCity = (pPlayer != NULL) ? pPlayer->getCity(kEvent.iSrcLoc) : NULL;
		if (pCity != NULL && kEvent.iType >= 0 && kEvent.iType < GC.getNumBuildingInfos())
		{
			const CvBuildingInfo& kBuilding = GC.getBuildingInfo((BuildingTypes)kEvent.iType);
			const int iSign = (kEvent.iEventId == SEVT_CITY_BUILDING_ACTIVATED) ? 1 : -1;
			const int iMilitaryWorth = iSign * kBuilding.getMilitaryWorth();
			const int iWorth = iSign * kBuilding.getWorth();
			pCity->buildingWorth().change(iMilitaryWorth, iWorth);
			pPlayer->demographics().buildings().change(iMilitaryWorth, iWorth);
			dc_changeAreaWorth(pCity->area(), kEvent.iC, iMilitaryWorth);
		}
		break;
	}
	// ⚠ Inside the recalculation bracket every plot leaves and rejoins by the thousand while the map does not yet
	// exist; the wholesale fact that closes the bracket is what this store acts on instead.
	case SEVT_AREA_TILE_ADDED:
	case SEVT_AREA_TILE_REMOVED:
		if (!GC.getMap().isRecalculatingAreas())
		{
			dc_movePlotWorth(kEvent.iSrcLoc, kEvent.iB, (kEvent.iEventId == SEVT_AREA_TILE_ADDED) ? 1 : -1);
		}
		break;
	case SEVT_AREAS_RECALCULATED:
		dc_rebookAreas();
		break;
	default:
		break;
	}
}

void demographicsContextRegisterConsumer()
{
	if (s_bDemographicsRegistered)
	{
		return;
	}
	s_bDemographicsRegistered = true;
	eventSpine().registerConsumer(&s_demographicsConsumer);
}
