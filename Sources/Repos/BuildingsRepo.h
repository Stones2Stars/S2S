#pragma once

#ifndef BUILDINGS_REPO_H
#define BUILDINGS_REPO_H

// Named lookups over the global CvBuildingInfo array.
//
// Replaces inline `for (i < getNumBuildingInfos()) if (kBuilding.isX())` loops
// with named methods. Each method returns a sorted std::vector<BuildingTypes>
// of buildings matching the named predicate. Rebuilt once after XML load via
// rebuild(); see CvXMLLoadUtilitySet::LoadPostMenuInfos.

class BuildingsRepo
	: private bst::noncopyable
{
public:
	static BuildingsRepo& get();

	// Rebuild every index from GC.m_paBuildingInfo. Idempotent.
	// Call after all Info arrays are loaded (or reloaded).
	void rebuild();

	// Buildings whose CvBuildingInfo::getReligionType() == eReligion.
	// Returns an empty vector for NO_RELIGION or out-of-range eReligion.
	const std::vector<BuildingTypes>& byReligion(ReligionTypes eReligion) const;

	// Buildings with getAllowed()->cap(ALLOWEDCAP_WORLD) != -1 (i.e. isWorldWonder).
	const std::vector<BuildingTypes>& worldWonders() const;

	// Buildings with getFreeStartEra() != NO_ERA.
	const std::vector<BuildingTypes>& withFreeStartEra() const;

	// The QUEUE-EXCLUDED class -- every `identity.notConstructible` building (bands, autobuilds, spawn-placed).
	// These are placed in every city unconditionally and gated purely by dormancy (CvCity::placeSystemBuildings).
	const std::vector<BuildingTypes>& systemPlacedBuildings() const;

private:
	BuildingsRepo();
	~BuildingsRepo();

	// m_byReligion[r] = sorted ascending list of buildings with getReligionType() == r.
	std::vector<std::vector<BuildingTypes> > m_byReligion;
	std::vector<BuildingTypes> m_worldWonders;
	std::vector<BuildingTypes> m_withFreeStartEra;
	std::vector<BuildingTypes> m_systemPlaced;
	std::vector<BuildingTypes> m_emptyBucket;
};

#endif // BUILDINGS_REPO_H
