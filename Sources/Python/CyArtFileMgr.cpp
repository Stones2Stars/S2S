//
// Python wrapper class for CvArtFileMgr
//
#include "CvGameCoreDLL.h"
#include "UI/CvArtFileMgr.h"
#include "CyArtFileMgr.h"
// boost needs COMPLETE art types for reference_existing_object -- the SPECIFIC headers, never the
// CvInfos.h umbrella the old interface leaned on (AGENTS.md Conventions).
#include "Infos/CvArtInfoInterface.h"
#include "Infos/CvArtInfoMovie.h"
#include "Infos/CvArtInfoMisc.h"
#include "Infos/CvArtInfoUnit.h"
#include "Infos/CvArtInfoBuilding.h"
#include "Infos/CvArtInfoCivilization.h"
#include "Infos/CvArtInfoLeaderhead.h"
#include "Infos/CvArtInfoBonus.h"
#include "Infos/CvArtInfoImprovement.h"
#include "Infos/CvArtInfoTerrain.h"
#include "Infos/CvArtInfoFeature.h"


CyArtFileMgr::CyArtFileMgr() : m_pArtFileMgr(ARTFILEMGR) {}

CyArtFileMgr::CyArtFileMgr(const CvArtFileMgr& pArtFileMgr) : m_pArtFileMgr(pArtFileMgr) {}


CvArtInfoInterface* CyArtFileMgr::getInterfaceArtInfo(const char* szArtDefineTag) const
{
	return m_pArtFileMgr.getInterfaceArtInfo(szArtDefineTag);
}

CvArtInfoMovie* CyArtFileMgr::getMovieArtInfo(const char* szArtDefineTag) const
{
	return m_pArtFileMgr.getMovieArtInfo(szArtDefineTag);
}

CvArtInfoMisc* CyArtFileMgr::getMiscArtInfo(const char* szArtDefineTag) const
{
	return m_pArtFileMgr.getMiscArtInfo(szArtDefineTag);
}

//
//	THE ART BOUNDARY, republished. ART IS EXPLICITLY OUT OF SCOPE for this rework (roadmap Scope decisions:
//	the art defines stay in the ART XML and ARTFILEMGR keeps resolving them), so this is a KEPT boundary like
//	TXT -- not the library, and not the banned read surface. It was collateral in the Cy BINDING purge.
//
void CyArtFileMgr::pythonPublish()
{
	// ⛔ REGISTER WHAT THE ACCESSORS RETURN, or every one of them raises at CONVERSION rather than answering.
	// Publishing a def whose return type has no registered class_ yields a TypeError where a reader expects an
	// AttributeError -- which is why this class reads as a mystery rather than as a missing binding
	// ([patterns.md] THE PYTHON READ BOUNDARY). The binding purge took these registrations along with the read
	// surfaces it was aimed at; only the second half was ever the target.
	//
	// ⚑ ONE method, on the common base: across the whole Python tree the art infos are asked for `getPath` and
	// nothing else (334 call sites, all of them getPath), so the leaves carry no def of their own -- they exist
	// only so a returned pointer has an identity to cross the boundary with.
	//
	// This is the KEPT art boundary, not the banned read surface: ART is explicitly out of scope for this
	// rework (roadmap § Scope decisions) and ARTFILEMGR keeps resolving the tags, exactly as TXT stays.
	// ⛔ Registered STANDALONE -- deliberately NOT `bases<CvInfoBase>`. boost::python resolves a declared base
	// at MODULE-INIT time, so naming an unregistered one does not fail at the call site: it aborts the whole
	// registration and the game dies with "Failed Initializing Python". CvInfoBase is not published (it is the
	// legacy info read surface, which stays cut), and getPath is this class's own method, so there is nothing
	// to inherit here anyway.
	python::class_<CvAssetInfoBase, boost::noncopyable>("CvAssetInfoBase", python::no_init)
		.def("getPath", &CvAssetInfoBase::getPath)
		;
	// ⚠ The noncopyable tag must be BOOST 1.32's, and that is not the alias this tree usually reaches for:
	// `bst` is boost155 while `python` is boost::python (1.32) -- so `bst::noncopyable` is a DIFFERENT type and
	// boost::python would not recognise it as the tag. Qualified deliberately; the banned shape is a
	// `using namespace boost*`, which is what lets a bare name silently resolve through the PCH (engine.md).
	python::class_<CvArtInfoAsset,        python::bases<CvAssetInfoBase>, boost::noncopyable>("CvArtInfoAsset", python::no_init);
	python::class_<CvArtInfoInterface,    python::bases<CvArtInfoAsset>, boost::noncopyable>("CvArtInfoInterface", python::no_init);
	python::class_<CvArtInfoMovie,        python::bases<CvArtInfoAsset>, boost::noncopyable>("CvArtInfoMovie", python::no_init);
	python::class_<CvArtInfoMisc,         python::bases<CvArtInfoAsset>, boost::noncopyable>("CvArtInfoMisc", python::no_init);
	python::class_<CvArtInfoUnit,         python::bases<CvArtInfoAsset>, boost::noncopyable>("CvArtInfoUnit", python::no_init);
	python::class_<CvArtInfoBuilding,     python::bases<CvArtInfoAsset>, boost::noncopyable>("CvArtInfoBuilding", python::no_init);
	python::class_<CvArtInfoCivilization, python::bases<CvArtInfoAsset>, boost::noncopyable>("CvArtInfoCivilization", python::no_init);
	python::class_<CvArtInfoBonus,        python::bases<CvArtInfoAsset>, boost::noncopyable>("CvArtInfoBonus", python::no_init);
	python::class_<CvArtInfoImprovement,  python::bases<CvArtInfoAsset>, boost::noncopyable>("CvArtInfoImprovement", python::no_init);

	python::class_<CyArtFileMgr>("CyArtFileMgr")
		.def("getInterfaceArtInfo", &CyArtFileMgr::getInterfaceArtInfo,  python::return_value_policy<python::reference_existing_object>())
		.def("getMovieArtInfo", &CyArtFileMgr::getMovieArtInfo,  python::return_value_policy<python::reference_existing_object>())
		.def("getMiscArtInfo", &CyArtFileMgr::getMiscArtInfo, python::return_value_policy<python::reference_existing_object>())
	;
}
