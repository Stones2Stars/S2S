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
#include "Python/CyAct.h"
#include "Python/CyGame.h"
#include "Python/CyTeam.h"
#include "Python/CyGameTextMgr.h"
#include "Python/CyGlobalContext.h"
#include "Python/CyMap.h"
#include "Python/CyMessageControl.h"
#include "Python/CyState.h"
#include "Python/CyWorldInfo.h"
#include "Python/CyImprovementInfo.h"
#include "Python/CyGameSpeedInfo.h"
#include "Python/CyEspionageMissionInfo.h"
#include "Python/CyVictoryInfo.h"
#include "Python/CyCultureLevelInfo.h"
#include "Python/CyPlayer.h"
#include "Python/CyPlot.h"
#include "Python/CyUnit.h"
#include "Python/CySelectionGroup.h"
#include "Python/CyGameCoreUtils.h"   // the shared calc helpers published as free functions
#include "Tools/CvRandom.h"           // registered (zero defs) so getMapRand's handle can cross
#include "Tools/SCyDebug.h"
#include "IDValueMap.h"
#include "Tools/Win32.h"
#include "Engine/CvUnit.h"           // CombatDetails -- the combat-log events' payload struct





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
	CyInfo::pythonPublish();      // "what do I CARRY?"       -- the GENERIC info half: identity text,
	                              //                            classification, edges -- what every registry shares
	// The PER-INFO accessors, for what belongs to ONE type. A script binds these BY NAME, so its bindings list is
	// its dependency list ([patterns.md]: explicit imports, always -- you see what is used).
	CyWorldInfo::pythonPublish();
	CyImprovementInfo::pythonPublish();
	CyGameSpeedInfo::pythonPublish();
	CyEspionageMissionInfo::pythonPublish();
	CyVictoryInfo::pythonPublish();
	CyCultureLevelInfo::pythonPublish();

	// NOT the library, and not the banned surface: TXT is an UNMIGRATED SYSTEM BOUNDARY that stays, and Python
	// screen chrome calls it directly (patterns.md § THE PYTHON READ BOUNDARY). It was collateral in the Cy
	// BINDING purge, which took a kept boundary out along with the read surface it was aimed at.
	CyGameTextMgr::pythonPublish();
	CyArtFileMgr::pythonPublish();   // ART: out of scope, kept

	// The COMMAND boundary: Python-authoritative UI telling the engine to ACT, through the net layer so
	// multiplayer stays in lockstep. The cut is DIRECTIONAL -- only the READ surface dies -- so this is a kept
	// boundary, not a revived getter contract ([DEC-cy-not-fixed]).
	CyMessageControl::pythonPublish();

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
	CyAct::pythonPublish();
	CyMap::pythonPublish();
	CyPlot::pythonPublish();
	CyArea::pythonPublish();





	// ⛔ TYPE REGISTRATION ONLY -- NOT a read surface, and NOT a revival of the cut Cy* bindings.
	//
	// A type needs a `class_<>` iff some engine call site hands it ACROSS, and there are TWO routes that do:
	//   `DECLARE_PY_WRAPPER` (CyPlot, CySelectionGroup) -- ArgTraits wraps the object BY VALUE and marshals it
	//        through makePythonObject -> python::object(obj);
	//   `CvGameObject::createPythonWrapper` -- builds a CyGame / CyTeam / CyPlayer / CyCity / CyUnit / CyPlot on
	//        its own path, for the property and outcome systems.
	// Either way the conversion THROWS unless boost::python holds a registered converter for that exact type, so
	// a missing line here is an unhandled C++ exception on the way OUT of the engine -- never a fault in the
	// Python tree, and never something a traceback names.
	//
	// ⚠ CyCity and CyUnit take NEITHER route through ArgTraits: they are `DECLARE_PY_IDENTITY`, crossing as an
	// (owner, id) tuple that needs no converter. Their registration below is owed by createPythonWrapper ALONE.
	// ⛔ So "it crosses as an identity, therefore it needs no class_<>" is false for them, and reading it that way
	// eliminates the right suspect.
	//
	// ⚑ REGISTRATION IS NOT BINDING (patterns.md § THE PYTHON READ BOUNDARY). These carry `no_init` and ZERO
	// `.def`s: Python can RECEIVE and pass one back, and can call NOTHING on it. That is the correct end state
	// for a wrapper whose read surface is deliberately gone ([DEC-cy-not-fixed]) -- the legacy getters stay cut.
	// The Cy* BINDING purge took these registrations out along with the read surfaces it was aimed at; only the
	// second half was ever the target.
	// ⚖ THE HANDLES CARRY AN IDENTITY SET (owner) -- owner + id + position, and nothing else. A legacy consumer
	// holding a handle must be able to say WHICH object it holds, and re-pointing every such site onto the read
	// planes is refactoring we are deliberately not doing: *"I only want to refactor the python I have to,
	// otherwise we never will be done."* ⛔ The registration-is-not-binding rule still governs everything ELSE --
	// the info/state getter contract stays cut ([DEC-cy-not-fixed]); a consumer wanting DATA asks CyInfo /
	// CyState / CyEnabler by that address. Each publish lives in the file named for its type, never here.
	CyCity::pythonPublish();
	CyUnit::pythonPublish();
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

	//   CvRandom -- the engine HANDS ONE ACROSS (CyGame::getMapRand / getSorenRand, both published with
	//               reference_existing_object) and Python both passes it on to the EXE's own shuffleList and
	//               DRAWS from it. So it needs the registration (or the def throws at CONVERSION: "No Python
	//               class registered for C++ class class CvRandom") AND the draw.
	//               ⚑ `get` is what a random is FOR: the map scripts draw through the handle at dozens of sites
	//               (CvMapGeneratorUtil's mapRand), and those are an OPEN EXTENSION POINT whose contract is the
	//               named Python callbacks -- a third-party script cannot be re-pointed, so the draw stays ON
	//               the handle. [DEC-cy-not-fixed] bans the info/state GETTER contract; a draw is neither, the
	//               same reading that keeps def_readwrite on the plain value structs above.
	//               ⛔ Do NOT add a second, named draw beside it. One job, one spelling: near-synonyms are the
	//               duplication the boundary ruling actually warns about ([patterns.md]).
	//               ⚠ It reaches the SYNCHRONIZED stream too, since getSorenRand hands that one across -- but
	//               that capability already exists as the published getSorenRandNum, so this adds a spelling and
	//               not a power. The draw COUNT on that stream is shared save state
	//               ([DEC-synced-rng-is-shared-state]): a cosmetic pick belongs on getASyncRand and must stay
	//               there.
	//               ⚠ shuffleList is the EXE's, not ours -- it has never existed in Sources/.
	python::class_<CvRandom>("CvRandom", python::no_init)
		.def("get", &CvRandom::get, "int (int iNum, str szLog) - a draw from 0 to iNum-1 inclusive")
		;

	//   TradeData -- fails in BOTH directions, which is why it surfaced twice over. The engine RETURNS one
	//               (CyDeal::getFirstTrade / getSecondTrade), so the scoreboard's deal walk resolved the def and
	//               then threw at CONVERSION -- "No Python class registered for C++ class struct TradeData",
	//               a TypeError arriving inside forceScreenRedraw, once per frame. Python also CONSTRUCTS one
	//               (TradeUtil / MoreCiv4lerts / CvRandomEventInterface / CvForeignAdvisor), which needs the
	//               default ctor. It is the marshalling VOCABULARY, not a getter contract, so the
	//               [DEC-cy-not-fixed] ban does not reach it.
	//	⚠ The published FIELD NAMES are not the member names: script reads `trade.ItemType` and `trade.iData`.
	//	Renaming them here to match the C++ members would silently break every reader.
	python::class_<TradeData>("TradeData")
		.def_readwrite("ItemType",  &TradeData::m_eItemType)
		.def_readwrite("iData",     &TradeData::m_iData)
		.def_readwrite("bOffering", &TradeData::m_bOffering)
		.def_readwrite("bHidden",   &TradeData::m_bHidden)
		;

	//   CombatDetails -- the engine PUSHES one (six sites in CvUnit::updateCombat, feeding the `combatLogCalc`
	//               and `combatLogHit` generic events), so without the registration the conversion throws
	//               BEFORE any handler is reached: an unhandled C++ exception (0xE06D7363) out of
	//               boost::python's to_python, i.e. a HARD CRASH rather than a Python traceback.
	//	⚑ Every push is guarded `isHuman() || pDefender->isHuman()`, so it fires whenever a human is on either
	//	side of a fight -- the player attacking, and the AI attacking the player during its end-turn. That is the
	//	whole of the repro, and it is why the same defect reads as two separate bugs.
	//	⚠ It is a decomposition of ONE combat's modifiers, computed and handed over by value -- a snapshot, not a
	//	handle to game state -- so its fields ARE the value and no getter contract is being revived here.
	//	The published names match the members; `CvUtil.combatMessageBuilder` and both `CvEventManager` handlers
	//	read `eOwner` / `eVisualOwner` / `iCurrCombatStr` / `sUnitName` by those spellings.
	python::class_<CombatDetails>("CombatDetails")
		.def_readwrite("iExtraCombatPercent",            &CombatDetails::iExtraCombatPercent)
		.def_readwrite("iAnimalCombatModifierTA",        &CombatDetails::iAnimalCombatModifierTA)
		.def_readwrite("iAIAnimalCombatModifierTA",      &CombatDetails::iAIAnimalCombatModifierTA)
		.def_readwrite("iAnimalCombatModifierAA",        &CombatDetails::iAnimalCombatModifierAA)
		.def_readwrite("iAIAnimalCombatModifierAA",      &CombatDetails::iAIAnimalCombatModifierAA)
		.def_readwrite("iBarbarianCombatModifierTB",     &CombatDetails::iBarbarianCombatModifierTB)
		.def_readwrite("iAIBarbarianCombatModifierTB",   &CombatDetails::iAIBarbarianCombatModifierTB)
		.def_readwrite("iBarbarianCombatModifierAB",     &CombatDetails::iBarbarianCombatModifierAB)
		.def_readwrite("iAIBarbarianCombatModifierAB",   &CombatDetails::iAIBarbarianCombatModifierAB)
		.def_readwrite("iPlotDefenseModifier",           &CombatDetails::iPlotDefenseModifier)
		.def_readwrite("iFortifyModifier",               &CombatDetails::iFortifyModifier)
		.def_readwrite("iCityDefenseModifier",           &CombatDetails::iCityDefenseModifier)
		.def_readwrite("iHillsAttackModifier",           &CombatDetails::iHillsAttackModifier)
		.def_readwrite("iHillsDefenseModifier",          &CombatDetails::iHillsDefenseModifier)
		.def_readwrite("iFeatureAttackModifier",         &CombatDetails::iFeatureAttackModifier)
		.def_readwrite("iFeatureDefenseModifier",        &CombatDetails::iFeatureDefenseModifier)
		.def_readwrite("iTerrainAttackModifier",         &CombatDetails::iTerrainAttackModifier)
		.def_readwrite("iTerrainDefenseModifier",        &CombatDetails::iTerrainDefenseModifier)
		.def_readwrite("iCityAttackModifier",            &CombatDetails::iCityAttackModifier)
		.def_readwrite("iDomainDefenseModifier",         &CombatDetails::iDomainDefenseModifier)
		.def_readwrite("iCityBarbarianDefenseModifier",  &CombatDetails::iCityBarbarianDefenseModifier)
		.def_readwrite("iDefenseModifier",               &CombatDetails::iDefenseModifier)
		.def_readwrite("iAttackModifier",                &CombatDetails::iAttackModifier)
		.def_readwrite("iCombatModifierT",               &CombatDetails::iCombatModifierT)
		.def_readwrite("iCombatModifierA",               &CombatDetails::iCombatModifierA)
		.def_readwrite("iDomainModifierA",               &CombatDetails::iDomainModifierA)
		.def_readwrite("iDomainModifierT",               &CombatDetails::iDomainModifierT)
		.def_readwrite("iAnimalCombatModifierA",         &CombatDetails::iAnimalCombatModifierA)
		.def_readwrite("iAnimalCombatModifierT",         &CombatDetails::iAnimalCombatModifierT)
		.def_readwrite("iRiverAttackModifier",           &CombatDetails::iRiverAttackModifier)
		.def_readwrite("iAmphibAttackModifier",          &CombatDetails::iAmphibAttackModifier)
		.def_readwrite("iKamikazeModifier",              &CombatDetails::iKamikazeModifier)
		.def_readwrite("iModifierTotal",                 &CombatDetails::iModifierTotal)
		.def_readwrite("iBaseCombatStr",                 &CombatDetails::iBaseCombatStr)
		.def_readwrite("iCombat",                        &CombatDetails::iCombat)
		.def_readwrite("iMaxCombatStr",                  &CombatDetails::iMaxCombatStr)
		.def_readwrite("iCurrHitPoints",                 &CombatDetails::iCurrHitPoints)
		.def_readwrite("iMaxHitPoints",                  &CombatDetails::iMaxHitPoints)
		.def_readwrite("iCurrCombatStr",                 &CombatDetails::iCurrCombatStr)
		.def_readwrite("eOwner",                         &CombatDetails::eOwner)
		.def_readwrite("eVisualOwner",                   &CombatDetails::eVisualOwner)
		.def_readwrite("sUnitName",                      &CombatDetails::sUnitName)
		;

	//   EventTriggeredData -- the RANDOM-EVENT payload. Five sites in CvPlayer push it
	//               (`Cy::Args() << &kTriggeredData`) into the random-event module: a trigger's
	//               `<PythonCallback>`, an event's `<PythonCanDo>` / `<PythonCallback>` /
	//               `<PythonExpireCheck>`. Those are XML-declared callbacks, so nothing in the Python tree
	//               names them and no grep of it finds the dependency.
	//	⛔ Without the registration the conversion throws before the callback is entered, and `CvPlayer::doEvents`
	//	runs PER PLAYER PER TURN -- so it is an end-of-turn crash, not an occasional one.
	//	⚠ The published names drop the `m_` prefix, which is what the handlers already read (`ePlayer` alone at
	//	~400 sites); renaming them to the member spellings would break every one.
	//	⚑ The two CvWString members are deliberately NOT published: `CvWString` derives from `std::wstring` and
	//	boost's built-in converter does not cover the derived type, so a `def_readwrite` on one would compile and
	//	then throw the first time a script touched it. No handler reads either, so there is no demand to serve --
	//	when one appears it wants an accessor returning `std::wstring`, never this shortcut.
	python::class_<EventTriggeredData>("EventTriggeredData")
		.def_readwrite("iId",                  &EventTriggeredData::m_iId)
		.def_readwrite("eTrigger",             &EventTriggeredData::m_eTrigger)
		.def_readwrite("iTurn",                &EventTriggeredData::m_iTurn)
		.def_readwrite("ePlayer",              &EventTriggeredData::m_ePlayer)
		.def_readwrite("iCityId",              &EventTriggeredData::m_iCityId)
		.def_readwrite("iPlotX",               &EventTriggeredData::m_iPlotX)
		.def_readwrite("iPlotY",               &EventTriggeredData::m_iPlotY)
		.def_readwrite("iUnitId",              &EventTriggeredData::m_iUnitId)
		.def_readwrite("eOtherPlayer",         &EventTriggeredData::m_eOtherPlayer)
		.def_readwrite("iOtherPlayerCityId",   &EventTriggeredData::m_iOtherPlayerCityId)
		.def_readwrite("eReligion",            &EventTriggeredData::m_eReligion)
		.def_readwrite("eCorporation",         &EventTriggeredData::m_eCorporation)
		.def_readwrite("eBuilding",            &EventTriggeredData::m_eBuilding)
		.def_readwrite("bExpired",             &EventTriggeredData::m_bExpired)
		;

	//
	//	THE SHARED CALC HELPERS -- free functions, not a surface of their own.
	//
	//	⛔ Published rather than reimplemented in script ([DEC-single-implementation]: every calculation exists
	//	EXACTLY ONCE). getModifiedIntValue is the engine's own percentage application, used throughout the AI, and
	//	the interface applies the SAME formula when it shows what a modifier did -- so a Python copy would be a
	//	second implementation of a game formula that could silently drift from the one the game actually runs.
	//	⚠ This is NOT the banned read surface: it takes plain ints and answers a plain int. It reaches no info, no
	//	game object and no per-owner state, so there is nothing here for a consumer to reach legacy THROUGH.
	//
	python::def("getModifiedIntValue", getModifiedIntValue);
	//	The espionage point-cost multiplier between two teams. Same justification as above and the same shape --
	//	two ints in, one int out -- and the espionage advisor SHOWS the number the engine actually charges, so a
	//	script-side copy would be a second implementation of a live game formula.
	python::def("getEspionageModifier", cyGetEspionageModifier);
	//	Integer square root, same shape and same justification. The war-prize, espionage-theft and great-people
	//	handlers feed its result straight into a synced gameplay outcome, and the engine takes the root with
	//	integer math -- so a script-side float sqrt would round differently from the one the game runs.
	python::def("intSqrt", cyIntSqrt64);

	Win32::pythonPublish();

	OutputDebugString("Publishing to Python: End\n");
}
