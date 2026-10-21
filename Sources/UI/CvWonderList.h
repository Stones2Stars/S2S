#pragma once
#ifndef CV_WONDER_LIST_H
#define CV_WONDER_LIST_H

//
//	CvWonderList -- the Info screen's wonders: what each list holds, and which of it a player may see
//	(docs/reference/info-screen.md § The wonders).
//

class CvCity;

///<summary>One row of a wonders list: a world or national wonder, or a project, built or under construction.
/// A built wonder carries the year it was built; a project or a wonder under construction carries none.</summary>
struct WonderRow
{
	///<summary>The building, or for the projects list the project.</summary>
	int iType;
	///<summary>Whether it is being built rather than built.</summary>
	bool bUnderConstruction;
	///<summary>Whether iYear holds the year it was built.</summary>
	bool bHasYear;
	///<summary>The year it was built, when bHasYear.</summary>
	int iYear;
	///<summary>Who built it; NO_PLAYER when the viewer does not know.</summary>
	PlayerTypes eBuilder;
	///<summary>The owner of the city it stands in, or is being built in.</summary>
	PlayerTypes eCityOwner;
	///<summary>That city's id; -1 when no city is shown.</summary>
	int iCityId;
	///<summary>Whether the viewer can zoom to the city: it is shown and the builder is named.</summary>
	bool bZoomable;
};

///<summary>Whether eBuilding belongs on the list eKind: the world wonders, or the national and team wonders.</summary>
bool isOnWonderList(BuildingTypes eBuilding, WonderListKind eKind);

///<summary>The buildings of the list eKind that city has, in building order.</summary>
void getCityWonders(const CvCity& city, WonderListKind eKind, std::vector<BuildingTypes>& wonders);

///<summary>The rows of the list eKind that eViewer may see: what is under construction first, then what is built,
/// the most recent first.</summary>
void getWonderRows(PlayerTypes eViewer, WonderListKind eKind, std::vector<WonderRow>& rows);

#endif // CV_WONDER_LIST_H
