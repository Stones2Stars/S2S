#pragma once

#ifndef CyArtFileMgr_h
#define CyArtFileMgr_h

//
// Python wrapper class for CvArtFileMgr
//

class CvArtFileMgr;
class CvArtInfoInterface;
class CvArtInfoMovie;
class CvArtInfoMisc;
class CvArtInfoUnit;
class CvArtInfoGenericBuilding;
class CvArtInfoGenericCity;
class CvArtInfoBuilding;
class CvArtInfoLeaderhead;
class CvArtInfoBonus;
class CvArtInfoImprovement;
class CvArtInfoTerrain;
class CvArtInfoFeature;
class CvArtInfoCivilization;

class CyArtFileMgr
{
public:
	CyArtFileMgr();

	// Publishes the ART boundary -- kept, not migrated (roadmap: art is out of scope).
	static void pythonPublish();
	explicit CyArtFileMgr(const CvArtFileMgr& pArtFileMgr);			// Call from C++

	CvArtInfoInterface* getInterfaceArtInfo(const char* szArtDefineTag) const;
	CvArtInfoMovie* getMovieArtInfo(const char* szArtDefineTag) const;
	CvArtInfoMisc* getMiscArtInfo(const char* szArtDefineTag) const;
	CvArtInfoGenericBuilding* getGenericBuildingArtInfo(const char* szArtDefineTag) const;
	CvArtInfoGenericCity* getGenericCityArtInfo(const char* szArtDefineTag) const;

protected:
	const CvArtFileMgr& m_pArtFileMgr;
};

#endif	// #ifndef CyArtFileMgr
