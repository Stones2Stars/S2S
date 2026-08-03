#include "CvGameCoreDLL.h"
#include "Python/CyIterator.h"
#include "CvPython.h"
#include "Python/CyCity.h"
#include "Python/CyEnabler.h"
#include "Python/CyArea.h"
#include "Python/CyArtFileMgr.h"
#include "Python/CyEnums.h"
#include "Python/CyInfo.h"
#include "CvPythonPlayerLoader.h"
#include "CvPythonPlotLoader.h"
#include "Python/CyGame.h"
#include "Python/CyTeam.h"
#include "Python/CyGameTextMgr.h"
#include "Python/CyGlobalContext.h"
#include "Python/CyMap.h"
#include "Python/CyState.h"
#include "Python/CyPlayer.h"
#include "Python/CyPlot.h"
#include "Python/CyUnit.h"
#include "Python/CySelectionGroup.h"
#include "Tools/SCyDebug.h"
#include "IDValueMap.h"
#include "Tools/Win32.h"





DllExport void DLLPublishToPython()
{
	OutputDebugString("Publishing to Python: Start\n");

	using namespace Cy::call_policy;

	registerAllowPyIntAsType<TechTypes>();
	registerAllowPyIntAsType<BuildingTypes>();
	registerAllowPyIntAsType<DirectionTypes>();
	registerAllowPyIntAsType<MultiplayerOptionTypes>();
	registerAllowPyIntAsType<CorporationTypes>();
	registerAllowPyIntAsType<GameOptionTypes>();
	registerAllowPyIntAsType<PlayerTypes>();
	registerAllowPyIntAsType<ReligionTypes>();
	registerAllowPyIntAsType<ImprovementTypes>();
	registerAllowPyIntAsType<CivilizationTypes>();
	registerAllowPyIntAsType<TeamTypes>();
	registerAllowPyIntAsType<ProjectTypes>();
	registerAllowPyIntAsType<SpecialUnitTypes>();
	registerAllowPyIntAsType<CivicOptionTypes>();
	registerAllowPyIntAsType<CivicTypes>();
	registerAllowPyIntAsType<SpecialBuildingTypes>();
	registerAllowPyIntAsType<ControlTypes>();
	registerAllowPyIntAsType<ForceControlTypes>();
	registerAllowPyIntAsType<EventTriggerTypes>();
	registerAllowPyIntAsType<LeaderHeadTypes>();
	registerAllowPyIntAsType<CultureLevelTypes>();
	registerAllowPyIntAsType<ReplayMessageTypes>();
	registerAllowPyIntAsType<ModderGameOptionTypes>();
	registerAllowPyIntAsType<YieldTypes>();
	registerAllowPyIntAsType<CultureLevelTypes>();
	registerAllowPyIntAsType<CommerceTypes>();
	registerAllowPyIntAsType<ColorTypes>();
	registerAllowPyIntAsType<EraTypes>();
	registerAllowPyIntAsType<ForceControlTypes>();
	registerAllowPyIntAsType<BonusTypes>();
	registerAllowPyIntAsType<HeritageTypes>();
	registerAllowPyIntAsType<HurryTypes>();
	registerAllowPyIntAsType<MapTypes>();
	registerAllowPyIntAsType<MapCategoryTypes>();
	registerAllowPyIntAsType<UnitAITypes>();
	registerAllowPyIntAsType<DomainTypes>();
	registerAllowPyIntAsType<PropertyTypes>();
	registerAllowPyIntAsType<ProcessTypes>();
	registerAllowPyIntAsType<UnitCombatTypes>();
	registerAllowPyIntAsType<UnitTypes>();
	registerAllowPyIntAsType<VictoryTypes>();
	registerAllowPyIntAsType<VoteTypes>();
	registerAllowPyIntAsType<VoteSourceTypes>();
	registerAllowPyIntAsType<FeatureTypes>();
	registerAllowPyIntAsType<TerrainTypes>();
	registerAllowPyIntAsType<PromotionTypes>();
	registerAllowPyIntAsType<FlavorTypes>();

	publishPythonVectorInterface<std::vector<BonusTypes>, CovertToInteger>();
	publishPythonVectorInterface<std::vector<HeritageTypes>, CovertToInteger>();
	publishPythonVectorInterface<std::vector<ImprovementTypes>, CovertToInteger>();
	publishPythonVectorInterface<std::vector<MapCategoryTypes>, CovertToInteger>();
	publishPythonVectorInterface<std::vector<TechTypes>, CovertToInteger>();

	publishIDValueMapPythonInterface<IDValueMap<BonusTypes, int> >();
	publishIDValueMapPythonInterface<IDValueMap<BuildingTypes, int> >();
	publishIDValueMapPythonInterface<IDValueMap<ImprovementTypes, int> >();
	publishIDValueMapPythonInterface<IDValueMap<TechTypes, int> >();
	publishIDValueMapPythonInterface<IDValueMap<TerrainTypes, int> >();
	publishIDValueMapPythonInterface<IDValueMap<UnitCombatTypes, int> >();
	publishIDValueMapPythonInterface<IDValueMap<UnitTypes, int> >();

	SCyDebug::installInPython();

	//
	// The NEW uniform read surface. Not a widened Cy* binding -- an id-based surface with no dependency on the
	// legacy wrappers, so they can be cut away without touching it ([DEC-cy-not-fixed]).
	//
	// The VOCABULARY goes first: the group reads below are specified as `getYields()[YieldTypes.YIELD_FOOD]`,
	// so the enum types have to exist before anything can consume a result.
	CyEnums::pythonPublish();     // the engine enum constants + name->id resolution
	CyEnabler::pythonPublish();   // "can I, right now?"      -- the availability half
	CyState::pythonPublish();     // "what do I HAVE, now?"   -- the live-state half
	CyInfo::pythonPublish();      // "what do I CARRY?"       -- the info half (the ONLY home for infos)

	// NOT the library, and not the banned surface: TXT is an UNMIGRATED SYSTEM BOUNDARY that stays, and Python
	// screen chrome calls it directly (patterns.md § THE PYTHON READ BOUNDARY). It was collateral in the Cy
	// BINDING purge, which took a kept boundary out along with the read surface it was aimed at.
	CyGameTextMgr::pythonPublish();
	CyArtFileMgr::pythonPublish();   // ART: out of scope, kept

	// The CONFIG half of the old global context, reintroduced deliberately (owner) WITHOUT the infos:
	// counts, defines, constants and the BUG bridge are configuration a great deal of Python needs, and are
	// not the read surface the library replaces. The get<X>Info accessors and Cy* handles stay gone.
	CyGlobalContext::pythonPublish();

	// The MAP-SCRIPT boundary: handles, not infos. Map scripts are their own boundary (patterns.md) and were
	// never meant to be affected by the Cy* cut.
	// The game-object HANDLES. Not infos -- these are the wrappers the engine hands to Python callbacks, and
	// GC.getPlayer/getTeam/getMap/getGame are the most-called names in the tree.
	CyGame::pythonPublish();
	CyTeam::pythonPublish();
	{
		// CyPlayer and CyPlot publish through their loaders (their def sets are split across translation units
		// to keep any single one inside the VC7.1 compiler's limits).
		python::class_<CyPlayer> player("CyPlayer", python::no_init);
		CvPythonPlayerLoader::CyPlayerPythonInterface1(player);
		CvPythonPlayerLoader::CyPlayerPythonInterface2(player);
		CvPythonPlayerLoader::CyPlayerPythonInterface3(player);
	}
	CyMap::pythonPublish();
	CyPlot::pythonPublish();
	CyArea::pythonPublish();





	// ⛔ TYPE REGISTRATION ONLY -- NOT a read surface, and NOT a revival of the cut Cy* bindings.
	//
	// `DECLARE_PY_WRAPPER` exists for exactly four types (CyCity / CyUnit / CySelectionGroup / CyPlot). When the
	// engine calls into Python with a CvCity*/CvUnit*/CvSelectionGroup*, ArgTraits wraps it BY VALUE and marshals
	// it through makePythonObject -> python::object(obj), which THROWS unless boost::python has a registered
	// class_<> converter for that exact type. So without these three lines every engine event carrying a city, a
	// unit or a selection group raises on the way out -- and CvDllPythonEvents fires those constantly, which is
	// the Python-error popup storm rather than any fault in the Python tree.
	//
	// ⚑ REGISTRATION IS NOT BINDING (patterns.md § THE PYTHON READ BOUNDARY). These carry `no_init` and ZERO
	// `.def`s: Python can RECEIVE and pass one back, and can call NOTHING on it. That is the correct end state
	// for a wrapper whose read surface is deliberately gone ([DEC-cy-not-fixed]) -- the legacy getters stay cut.
	// The Cy* BINDING purge took these registrations out along with the read surfaces it was aimed at; only the
	// second half was ever the target.
	python::class_<CyCity>("CyCity", python::no_init);
	python::class_<CyUnit>("CyUnit", python::no_init);
	python::class_<CySelectionGroup>("CySelectionGroup", python::no_init);

	// ⛔ THE PLAIN-STRUCT MARSHALLING VOCABULARY -- the SAME registration-is-not-binding rule as the wrappers
	// above, one level down. These are VALUE structs, not handles: their fields ARE the value, so they answer
	// no question about game state and constitute no read surface ([DEC-cy-not-fixed] bans the info/state
	// GETTER contract, which a bare coordinate pair is not).
	//
	// ⚑ Both directions of the boundary need them, which is why the absence bites twice over:
	//   NiColorA -- Python CONSTRUCTS one and hands it to the engine (the strategy/dot-map overlay's plot
	//               colours), so without the ctor the module raises NameError at import.
	//   POINT    -- the engine RETURNS one (`Win32::getCursorPos`, published just below), so without the
	//               registration that def resolves and then throws at CONVERSION -- a TypeError where a reader
	//               expects an AttributeError, which is exactly why this class reads as a mystery rather than
	//               as a missing binding (patterns.md § THE PYTHON READ BOUNDARY).
	//
	// Registered on DEMAND, not wholesale: the rest of the cut struct set has no live consumer, and a type that
	// turns out to be needed comes back the same way this one did -- named by the call site that wanted it.
	python::class_<NiColorA>("NiColorA")
		.def(python::init<float, float, float, float>())
		.def_readwrite("r", &NiColorA::r)
		.def_readwrite("g", &NiColorA::g)
		.def_readwrite("b", &NiColorA::b)
		.def_readwrite("a", &NiColorA::a)
		;

	python::class_<POINT>("POINT")
		.def_readwrite("x", &POINT::x)
		.def_readwrite("y", &POINT::y)
		;

	Win32::pythonPublish();

	OutputDebugString("Publishing to Python: End\n");
}
