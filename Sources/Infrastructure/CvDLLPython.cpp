#include "CvGameCoreDLL.h"
#include "Python/CyIterator.h"
#include "CvPython.h"
#include "Python/CyCity.h"
#include "Python/CyEnabler.h"
#include "Python/CyEnums.h"
#include "Python/CyGlobalContext.h"
#include "Python/CyState.h"
#include "Python/CyPlayer.h"
#include "Python/CyPlot.h"
#include "Python/CyUnit.h"
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





	Win32::pythonPublish();

	OutputDebugString("Publishing to Python: End\n");
}
