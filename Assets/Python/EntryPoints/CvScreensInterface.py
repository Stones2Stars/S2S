## Sid Meier's Civilization 4
## Copyright Firaxis Games 2005

from CvPythonExtensions import *
from CvScreenEnums import *
import types
import CvMainInterface
import CvOptionsScreen
import CvReplayScreen
import ScreenInput as PyScreenInput

import BugCore
GC = CyGlobalContext()
GAME = GC.getGame()
AdvisorOpt = BugCore.game.Advisors

g_iScreenActive = -2

def toggleSetNoScreens():
	global g_iScreenActive
	print "SCREEN %s TURNED OFF" %(g_iScreenActive)
	toggleSetScreenOn((MAIN_INTERFACE,))

def toggleSetScreenOn(argsList):
	global g_iScreenActive
	if g_iScreenActive == -2:
		import ScreenResolution as SR
		if not SR.x:
			print "Fetch resolution setting from profileName.pfl"
			UserProfile = CyUserProfile()
			print "\nAll User Profiles:"
			for i in xrange(UserProfile.getNumProfileFiles()):
				print UserProfile.getProfileFileName(i)
			print "Current profile: " + UserProfile.getProfileName()
			szRes = UserProfile.getResolutionString(UserProfile.getResolution())
			szRes = szRes.split(" x ")
			SR.x = int(szRes[0])
			SR.y = int(szRes[1])
			print "Resolution: %dx%d" %(SR.x, SR.y)
			SR.calibrate()
	print "SCREEN %s TURNED ON" %(argsList[0])
	g_iScreenActive = argsList[0]

#diplomacyScreen = CvDiplomacy.CvDiplomacy()

mainInterface = CvMainInterface.CvMainInterface()
def showMainInterface():
	print "showMainInterface"
	mainInterface.interfaceScreen()

def reinitMainInterface():
	print "reinitMainInterface"
	global mainInterface
	mainInterface = CvMainInterface.CvMainInterface()
	mainInterface.interfaceScreen()

def initMinimap():
	mainInterface.initMinimap()

def numPlotListButtons(): return 0 # Called from exe

def showTechChooser():
	if CyGame().getActivePlayer() != -1:
		getScreen(TECH_CHOOSER).interfaceScreen(TECH_CHOOSER)

def showHallOfFame(argsList):
	getScreen(HALL_OF_FAME).interfaceScreen(argsList[0])

def showCivicsScreen():
	if CyGame().getActivePlayer() != -1:
		getScreen(CIVICS_SCREEN).interfaceScreen()

def showHeritageScreen():
	if CyGame().getActivePlayer() != -1:
		getScreen(HERITAGE_SCREEN).interfaceScreen()

def showReligionScreen():
	if CyGame().getActivePlayer() != -1:
		getScreen(RELIGION_SCREEN).interfaceScreen()

def showCorporationScreen():
	if CyGame().getActivePlayer() != -1:
		getScreen(CORPORATION_SCREEN).interfaceScreen()

optionsScreen = CvOptionsScreen.CvOptionsScreen()
def showOptionsScreen():
	optionsScreen.interfaceScreen()

def showForeignAdvisorScreen(argsList):
	if CyGame().getActivePlayer() != -1:
		getScreen(FOREIGN_ADVISOR).interfaceScreen()

def showFinanceAdvisor():
	if CyGame().getActivePlayer() != -1:
		getScreen(FINANCE_ADVISOR).interfaceScreen()

def showDomesticAdvisor(argsList):
	if CyGame().getActivePlayer() != -1:
		getScreen(DOMESTIC_ADVISOR).interfaceScreen()

def showMilitaryAdvisor():
	if CyGame().getActivePlayer() != -1:
		getScreen(MILITARY_ADVISOR).interfaceScreen()

def showEspionageAdvisor():
	if CyGame().getActivePlayer() != -1:
		getScreen(ESPIONAGE_ADVISOR).interfaceScreen()

def showDawnOfMan(argsList):
	getScreen(DAWN_OF_MAN).interfaceScreen(DAWN_OF_MAN)

def showIntroMovie(argsList):
	getScreen(INTRO_MOVIE_SCREEN).interfaceScreen()

def showVictoryMovie(argsList):
	getScreen(VICTORY_MOVIE_SCREEN).interfaceScreen(argsList[0])

def showWonderMovie(argsList):
	getScreen(WONDER_MOVIE_SCREEN).interfaceScreen(argsList[0], argsList[1], argsList[2], WONDER_MOVIE_SCREEN)

def showEraMovie(argsList):
	getScreen(ERA_MOVIE_SCREEN).interfaceScreen(argsList[0])

def showSpaceShip(argsList):
	if CyGame().getActivePlayer() != -1:
		getScreen(SPACE_SHIP_SCREEN).interfaceScreen(argsList[0])

replayScreen = CvReplayScreen.CvReplayScreen(REPLAY_SCREEN)
def showReplay(argsList):
	if argsList[0] > -1:
		CyGame().saveReplay(argsList[0])
	replayScreen.showScreen(argsList[4])

def showDanQuayleScreen(argsList):
	getScreen(DAN_QUAYLE_SCREEN).interfaceScreen()

unVictoryScreen = None
def getUnVictoryScreen():
	"""Built on first use, like the screenMap screens."""
	global unVictoryScreen
	if unVictoryScreen is None:
		import CvUnVictoryScreen
		unVictoryScreen = CvUnVictoryScreen.CvUnVictoryScreen()
	return unVictoryScreen

def showUnVictoryScreen(argsList):
	getUnVictoryScreen().interfaceScreen()

def showTopCivs():
	getScreen(TOP_CIVS).showScreen()

def showInfoScreen(argsList):
	if CyGame().getActivePlayer() != -1:
		getScreen(INFO_SCREEN).interfaceScreen(argsList[0], argsList[1])

def showDebugInfoScreen():
	getScreen(DEBUG_INFO_SCREEN).interfaceScreen()

def configTechSplash(option=None, value=None):
	if value is None:
		TechWindowOpt = BugCore.game.TechWindow
		if TechWindowOpt.isWideView():
			value = True

	if value:
		import TechWindowWide
		screen = TechWindowWide.CvTechSplashScreen(TECH_SPLASH)
	else:
		import TechWindow
		screen = TechWindow.CvTechSplashScreen(TECH_SPLASH)
	screenMap[TECH_SPLASH] = screen

def showTechSplash(argsList):
	if TECH_SPLASH not in screenMap:
		configTechSplash()
	getScreen(TECH_SPLASH).interfaceScreen(argsList[0])

def showVictoryScreen():
	if CyGame().getActivePlayer() != -1:
		getScreen(VICTORY_SCREEN).interfaceScreen()


def cityScreenRedraw():
	mainInterface.updateCityScreen()

def showBuildListScreen():
	if CyGame().getActivePlayer() != -1:
		getScreen(BUILD_LIST_SCREEN).interfaceScreen()

def showForgetfulScreen():
	getScreen(FORGETFUL_SCREEN).interfaceScreen(FORGETFUL_SCREEN)

#################################################
## Pedia
#################################################

def linkToPedia(argsList):
	getScreen(PEDIA).link(argsList)

def pediaShow():
	getScreen(PEDIA).pediaShow()

def pediaBack():
	getScreen(PEDIA).back()

def pediaForward():
	getScreen(PEDIA).forward()

def pediaJumpToBuilding(argsList):
	getScreen(PEDIA).pediaJump(-3, "", argsList[0])

def pediaJumpToUnit(argsList):
	if argsList[0] > -1:
		getScreen(PEDIA).pediaJump(-2, "", argsList[0])
	else:
		getScreen(PEDIA).pediaJump(10, "UnitCombat", argsList[0] + 100000)

def pediaMain(argsList):
	getScreen(PEDIA).pediaJump(-1, "", argsList[0])

def pediaShowHistorical(argsList):
	if argsList[0] == CivilopediaPageTypes.CIVILOPEDIA_PAGE_CONCEPT_NEW:
		getScreen(PEDIA).pediaJump(0, "NEW", argsList[1])
	else:
		getScreen(PEDIA).pediaJump(0, "", argsList[1])

def pediaJumpToTech(argsList):
	getScreen(PEDIA).pediaJump(1, "", argsList[0])

def pediaJumpToPromotion(argsList):
	getScreen(PEDIA).pediaJump(4, "", argsList[0])

def pediaJumpToBonus(argsList):
	getScreen(PEDIA).pediaJump(7, "", argsList[0])

def pediaJumpToTerrain(argsList):
	getScreen(PEDIA).pediaJump(8, "Terrain", argsList[0])

def pediaJumpToFeature(argsList):
	getScreen(PEDIA).pediaJump(8, "Feature", argsList[0])

def pediaJumpToImprovement(argsList):
	getScreen(PEDIA).pediaJump(8, "Improvement", argsList[0])

def pediaJumpToTrait(argsList):
	getScreen(PEDIA).pediaJump(9, "Trait", argsList[0])

def pediaJumpToCiv(argsList):
	getScreen(PEDIA).pediaJump(9, "Civ", argsList[0])

def pediaJumpToLeader(argsList):
	getScreen(PEDIA).pediaJump(9, "Leader", argsList[0])

def pediaJumpToCivic(argsList):
	getScreen(PEDIA).pediaJump(9, "Civic", argsList[0])

def pediaJumpToReligion(argsList):
	getScreen(PEDIA).pediaJump(9, "Religion", argsList[0])

def pediaJumpToHeritage(argsList):
	getScreen(PEDIA).pediaJump(9, "Heritage", argsList[0])

def pediaJumpToProject(argsList):
	getScreen(PEDIA).pediaJump(10, "Project", argsList[0])

def pediaJumpToSpecialist(argsList):
	getScreen(PEDIA).pediaJump(10, "Specialist", argsList[0])

def pediaJumpToCorporation(argsList):
	getScreen(PEDIA).pediaJump(10, "Corporation", argsList[0])

def pediaJumpToRoute(argsList):
	if argsList[0] > -1:
		getScreen(PEDIA).pediaJump(8, "Route", argsList[0])
	else:
		getScreen(PEDIA).pediaJump(10, "Build", argsList[0] + 100000)

def pediaJumpToEra(iEra):
	getScreen(PEDIA).pediaJump(0, "Eras", iEra)

#################################################
## Worldbuilder
#################################################
def showWorldBuilderScreen():
	print "showWorldBuilderScreen"
	if CyInterface().isInAdvancedStart():
		advancedStartScreen.interfaceScreen(ADVANCED_START_SCREEN)
	else: worldBuilderScreen.interfaceScreen()

def WorldBuilderExitCB():
	print "WorldBuilderExitCB"
	if CyInterface().isInAdvancedStart():
		CyInterface().setWorldBuilder(False)
	else: CyGame().exitWorldBuilder()

def hideWorldBuilderScreen():
	print "hideWorldBuilderScreen"
	if CyInterface().isInAdvancedStart():
		advancedStartScreen.killScreen()
	else:
		worldBuilderScreen.killScreen()
		toggleSetNoScreens()

def WorldBuilderToggleUnitEditCB():
	print "WorldBuilderToggleUnitEditCB"
	worldBuilderScreen.toggleUnitEditCB()

def WorldBuilderEraseCB():
	print "WorldBuilderEraseCB"
	worldBuilderScreen.eraseCB()

def WorldBuilderLandmarkCB():
	print "WorldBuilderLandmarkCB"
	worldBuilderScreen.landmarkModeCB()

def WorldBuilderToggleCityEditCB():
	print "WorldBuilderToggleCityEditCB"
	worldBuilderScreen.toggleCityEditCB()

def WorldBuilderNormalMapTabModeCB():
	print "WorldBuilderNormalMapTabModeCB"
	worldBuilderScreen.normalMapTabModeCB()

def WorldBuilderRevealTabModeCB():
	print "WorldBuilderRevealTabModeCB"
	worldBuilderScreen.revealTabModeCB()

def WorldBuilderDiplomacyModeCB():
	print "WorldBuilderDiplomacyModeCB"
	getScreen(WB_DIPLOMACY).interfaceScreen(CyGame().getActivePlayer(), False)

def WorldBuilderRevealAllCB():
	print "WorldBuilderRevealAllCB"
	worldBuilderScreen.revealAll(True)

def WorldBuilderUnRevealAllCB():
	print "WorldBuilderUnRevealAllCB"
	worldBuilderScreen.revealAll(False)

def WorldBuilderGetHighlightPlot(argsList):
	print "WorldBuilderGetHighlightPlot"
	if CyInterface().isInAdvancedStart():
		return advancedStartScreen.getHighlightPlot(argsList)
	else: return worldBuilderScreen.getHighlightPlot(argsList)

#----------------------------------------------------------------------------#
# Functions called by the exe in this order at Advanced start initialization #
#----------------------------------------------------------------------------#
def WorldBuilderGetASCityTabID():
	print "WorldBuilderGetASCityTabID"
	return advancedStartScreen.getCityTab()

def WorldBuilderGetASCityListID():
	print "WorldBuilderGetASCityListID"
	return advancedStartScreen.getCityRow()

def WorldBuilderGetASBuildingsListID():
	print "WorldBuilderGetASBuildingsListID"
	return advancedStartScreen.getBuildingsRow()

def WorldBuilderGetASAutomateListID():
	print "WorldBuilderGetASAutomateListID"
	return advancedStartScreen.getAutomationRow()

def WorldBuilderGetASUnitTabID():
	print "WorldBuilderGetASUnitTabID"
	return advancedStartScreen.getUnitTab()

def WorldBuilderGetASImprovementsTabID():
	print "WorldBuilderGetASImprovementsTabID"
	return advancedStartScreen.getImprovementTab()

def WorldBuilderGetASRoutesListID():
	print "WorldBuilderGetASRoutesListID"
	return advancedStartScreen.getRoutesRow()

def WorldBuilderGetASImprovementsListID():
	print "WorldBuilderGetASImprovementsListID"
	return advancedStartScreen.getImprovementsRow()

def WorldBuilderGetASVisibilityTabID():
	print "WorldBuilderGetASVisibilityTabID"
	return advancedStartScreen.getVisibilityTab()

def WorldBuilderGetASTechTabID():
	print "WorldBuilderGetASTechTabID"
	return advancedStartScreen.getTechTab()
#------------------------------------------------#
# Called by the exe for WB and AS initialization #
#------------------------------------------------#
def WorldBuilderNormalPlayerTabModeCB():
	print "WorldBuilderNormalPlayerTabModeCB"
	if CyInterface().isInAdvancedStart():
		getWBToolNormalMapTabCtrl().enable(False)
	else:
		worldBuilderScreen.normalPlayerTabModeCB()
#---------------------------------#
# Called by the exe for WB and AS #
#---------------------------------#
def WorldBuilderOnAdvancedStartBrushSelected(argsList):
	iList, iIndex, iTab = argsList
	print "WorldBuilderOnAdvancedStartBrushSelected, iList=%d, iIndex=%d, type=%d" %(iList, iIndex, iTab)
	if iTab == advancedStartScreen.getTechTab():
		showTechChooser()
	elif iTab == advancedStartScreen.getCityTab() and iList == advancedStartScreen.getAutomationRow():
		CyMessageControl().sendAdvancedStartAction(AdvancedStartActionTypes.ADVANCEDSTARTACTION_AUTOMATE, advancedStartScreen.iPlayer, -1, -1, -1, True)

	advancedStartScreen.setCurrentSelection(iTab, iIndex, iList)


######################
## Strategy Overlay ##
######################
import CvDotMapOverlayScreen
overlayScreen = CvDotMapOverlayScreen.CvDotMapOverlayScreen(STRATEGY_OVERLAY_SCREEN)

def showOverlayScreen():
	overlayScreen.interfaceScreen()

def hideOverlayScreen():
	overlayScreen.hideScreen()

######################
## Utility Functions #
######################

def movieDone(argsList):
	if argsList[0] == INTRO_MOVIE_SCREEN:
		getScreen(INTRO_MOVIE_SCREEN).hideScreen()
	elif argsList[0] == VICTORY_MOVIE_SCREEN:
		getScreen(VICTORY_MOVIE_SCREEN).hideScreen()

def leftMouseDown(argsList):

	if argsList[0] == WORLDBUILDER_SCREEN:
		worldBuilderScreen.leftMouseDown(argsList[1:])
		return 1
	elif argsList[0] == ADVANCED_START_SCREEN:
		advancedStartScreen.leftMouseDown(argsList[1:])
		return 1
	return 0

def rightMouseDown(argsList):

	if argsList[0] == WORLDBUILDER_SCREEN:
		worldBuilderScreen.rightMouseDown()
		return 1
	elif argsList[0] == ADVANCED_START_SCREEN:
		advancedStartScreen.rightMouseDown()
		return 1
	return 0

def mouseOverPlot(argsList):

	if argsList[0] == STRATEGY_OVERLAY_SCREEN:
		overlayScreen.onMouseOverPlot()

	elif argsList[0] == WORLDBUILDER_SCREEN:
		worldBuilderScreen.mouseOverPlot()

	elif argsList[0] == ADVANCED_START_SCREEN:
		advancedStartScreen.mouseOverPlot()

def handleInput(argsList):
	inputClass = PyScreenInput.ScreenInput(argsList)
	iPythonFile = inputClass.ePythonFileEnum
	# get the screen that is active from the screenMap Dictionary
	if iPythonFile in screenMap:
		# call handle input on that screen
		return screenMap[iPythonFile].handleInput(inputClass)
	return 0

# Entry point for dll requests to display messages.
import CvUtil
def sendMessage(args):
	CvUtil.sendMessage(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8], args[9], args[10], args[11])

def update(argsList):
	if argsList[0] == STRATEGY_OVERLAY_SCREEN:
		overlayScreen.update(argsList)

	elif argsList[0] in screenMap:
		screen = screenMap[argsList[0]]
		screen.update(argsList[1])

def onClose(argsList):
	if argsList[0] in screenMap:
		screen = screenMap[argsList[0]]
		if hasattr(screen, "onClose") and isinstance(screen.onClose, types.MethodType):
			screen.onClose()

# Forced screen update (250 ms)
def forceScreenUpdate(argsList):
	if argsList[0] == TECH_CHOOSER:
		getScreen(TECH_CHOOSER).updateTechRecords(False)

	elif argsList[0] == MAIN_INTERFACE:
		mainInterface.updateScreen()

	elif argsList[0] == WORLDBUILDER_SCREEN:
		worldBuilderScreen.updateScreen()

	elif argsList[0] == ADVANCED_START_SCREEN:
		advancedStartScreen.updateScreen()

# Called by dll every time a players turn is set to active when the active player has finished his turn.
# Only used to update what player one is waiting for while using "minimize AI turns" BUG option.
def updateWaitingForPlayer(argsList):
	if g_iScreenActive == MAIN_INTERFACE:
		mainInterface.updateWaitingForPlayer(argsList[0])


# Forced redraw (~7 ms)
def forceScreenRedraw(argsList):
	if argsList[0] == MAIN_INTERFACE:
		mainInterface.redraw()

	elif argsList[0] == TECH_CHOOSER:
		getScreen(TECH_CHOOSER).updateTechRecords(True)

	elif argsList[0] == ESPIONAGE_ADVISOR:
		getScreen(ESPIONAGE_ADVISOR).redraw(CyGInterfaceScreen("EspionageAdvisor", ESPIONAGE_ADVISOR))

def minimapClicked (argsList):
	if MILITARY_ADVISOR == argsList[0]:
		getScreen(MILITARY_ADVISOR).minimapClicked()


############################################################################
## Misc Functions
############################################################################

def handleBack(screens):
	if screens:
		for iScreen in screens:
			screen = screenMap.get(iScreen)
			if hasattr(screen, "back") and isinstance(screen.back, types.MethodType):
				screen.back()
	else:
		mainInterface.back()

def handleForward(screens):
	if screens:
		for iScreen in screens:
			screen = screenMap.get(iScreen)
			if hasattr(screen, "forward") and isinstance(screen.forward, types.MethodType):
				screen.forward()
	else:
		mainInterface.forward()

def refreshMilitaryAdvisor(argsList):
	if 1 == argsList[0]:
		getScreen(MILITARY_ADVISOR).refreshSelectedGroup(argsList[1])
	elif argsList[0] <= 0:
		getScreen(MILITARY_ADVISOR).refreshSelectedUnit(-argsList[0], argsList[1])

def updateMusicPath(argsList):
	szPathName = argsList[0]
	optionsScreen.updateMusicPath(szPathName)

def refreshOptionsScreen():
	optionsScreen.refreshScreen()

def cityWarningOnClickedCallback(argsList):
	iButtonId = argsList[0]
	iData1 = argsList[1]
	iData2 = argsList[2]
	iData3 = argsList[3]
	iData4 = argsList[4]
	szText = argsList[5]
	bOption1 = argsList[6]
	bOption2 = argsList[7]
	city = CyGlobalContext().getPlayer(CyGlobalContext().getGame().getActivePlayer()).getCity(iData1)
	if city:
		if (iButtonId == 0):
			if (city.isProductionProcess()):
				CyMessageControl().sendPushOrder(iData1, iData2, iData3, False, False, False)
			else:
				CyMessageControl().sendPushOrder(iData1, iData2, iData3, False, True, False)
		elif (iButtonId == 2):
			CyInterface().selectCity(city, False)

def cityWarningOnFocusCallback(argsList):
	CyInterface().playGeneralSound("AS2D_ADVISOR_SUGGEST")
	CyInterface().lookAtCityOffset(argsList[0])
	return 0

def liberateOnClickedCallback(argsList):
	iButtonId = argsList[0]
	iData1 = argsList[1]
	iData2 = argsList[2]
	iData3 = argsList[3]
	iData4 = argsList[4]
	szText = argsList[5]
	bOption1 = argsList[6]
	bOption2 = argsList[7]
	city = CyGlobalContext().getPlayer(CyGlobalContext().getGame().getActivePlayer()).getCity(iData1)
	if city:
		if iButtonId == 0:
			CyMessageControl().sendDoTask(iData1, TaskTypes.TASK_LIBERATE, 0, -1, False, False, False, False)
		elif iButtonId == 2:
			CyInterface().selectCity(city, False)

def colonyOnClickedCallback(argsList):
	iButtonId = argsList[0]
	iData1 = argsList[1]
	iData2 = argsList[2]
	iData3 = argsList[3]
	iData4 = argsList[4]
	szText = argsList[5]
	bOption1 = argsList[6]
	bOption2 = argsList[7]
	city = CyGlobalContext().getPlayer(CyGlobalContext().getGame().getActivePlayer()).getCity(iData1)
	if city:
		if iButtonId == 0:
			CyMessageControl().sendEmpireSplit(CyGlobalContext().getGame().getActivePlayer(), city.area().getID())
		elif iButtonId == 2:
			CyInterface().selectCity(city, False)

def featAccomplishedOnClickedCallback(argsList):
	iButtonId = argsList[0]
	iData1 = argsList[1]
	iData2 = argsList[2]
	iData3 = argsList[3]
	iData4 = argsList[4]
	szText = argsList[5]
	bOption1 = argsList[6]
	bOption2 = argsList[7]

	if iButtonId == 1:
		if iData1 == FeatTypes.FEAT_TRADE_ROUTE:
			showDomesticAdvisor(())
		elif (iData1 >= FeatTypes.FEAT_UNITCOMBAT_ARCHER) and (iData1 <= FeatTypes.FEAT_UNIT_SPY):
			CyGlobalContext().getGame().doControl(ControlTypes.CONTROL_MILITARY_SCREEN)
		elif (iData1 >= FeatTypes.FEAT_COPPER_CONNECTED) and (iData1 <= FeatTypes.FEAT_FOOD_CONNECTED):
			showForeignAdvisorScreen([0])
		elif iData1 == FeatTypes.FEAT_NATIONAL_WONDER:
			# 2 is for the wonder tab...
			showInfoScreen([2, 0])
		elif (iData1 >= FeatTypes.FEAT_POPULATION_HALF_MILLION) and (iData1 <= FeatTypes.FEAT_POPULATION_2_BILLION):
			# 1 is for the demographics tab...
			showInfoScreen([1, 0])
		elif iData1 == FeatTypes.FEAT_CORPORATION_ENABLED:
			showCorporationScreen()

def featAccomplishedOnFocusCallback(argsList):
	iData1 = argsList[0]
	iData2 = argsList[1]
	iData3 = argsList[2]
	iData4 = argsList[3]
	szText = argsList[4]
	bOption1 = argsList[5]
	bOption2 = argsList[6]

	CyInterface().playGeneralSound("AS2D_FEAT_ACCOMPLISHED")
	if iData1 >= FeatTypes.FEAT_UNITCOMBAT_ARCHER and iData1 <= FeatTypes.FEAT_FOOD_CONNECTED:
		CyInterface().lookAtCityOffset(iData2)

####################
# Handle Input Map #
####################
class _LazyScreenMap(dict):
	"""screenMap, with construction deferred to FIRST USE.

	Reading screenMap[X] is unchanged for every caller; what changed is that a screen nobody opens is never
	built. That matters because a screen constructor READS THE GAME, so eagerly building the tree put every
	read it performs on the startup path.
	"""
	def __missing__(self, key):
		moduleName, className, args = _screenFactories[key]
		module = __import__(moduleName)
		screen = getattr(module, className)(*args)
		dict.__setitem__(self, key, screen)
		return screen

screenMap = _LazyScreenMap({
	MAIN_INTERFACE			: mainInterface,
	OPTIONS_SCREEN			: optionsScreen,
	REPLAY_SCREEN			: replayScreen,
	STRATEGY_OVERLAY_SCREEN		: overlayScreen
})
##############
# Initialize #
##############
def lateInit():
	"""Registers the LATE screens and does the remaining late-init work.

	The screens are registered as FACTORIES, not constructed: building one reads the game, and constructing
	twenty at startup put the whole advisor/screen tree on the path before anything could be shown. screenMap[X]
	still reaches them -- they are simply built the first time they are asked for, exactly like earlyInit's.
	"""
	# WorldBuilder is built FIRST: its sub-screens take it as a constructor argument, so it has to exist before
	# the factory rows below are evaluated.
	import WorldBuilder, CvAdvancedStartScreen
	global worldBuilderScreen, advancedStartScreen
	advancedStartScreen = CvAdvancedStartScreen.CvAdvancedStartScreen()
	worldBuilderScreen = WorldBuilder.WorldBuilder(WORLDBUILDER_SCREEN)

	_screenFactories.update({
		CORPORATION_SCREEN    : ('CvCorporationScreen', 'CvCorporationScreen', ()),
		ESPIONAGE_ADVISOR     : ('CvEspionageAdvisor', 'CvEspionageAdvisor', ()),
		MILITARY_ADVISOR      : ('CvMilitaryAdvisor', 'CvMilitaryAdvisor', (MILITARY_ADVISOR,)),
		DOMESTIC_ADVISOR      : ('CvDomesticAdvisor', 'CvDomesticAdvisor', (DOMESTIC_ADVISOR,)),
		FOREIGN_ADVISOR       : ('CvForeignAdvisor', 'CvForeignAdvisor', (FOREIGN_ADVISOR,)),
		FINANCE_ADVISOR       : ('CvFinanceAdvisor', 'CvFinanceAdvisor', (FINANCE_ADVISOR,)),
		RELIGION_SCREEN       : ('CvReligionScreen', 'CvReligionScreen', ()),
		ERA_MOVIE_SCREEN      : ('CvEraMovieScreen', 'CvEraMovieScreen', ()),
		VICTORY_SCREEN        : ('CvVictoryScreen', 'CvVictoryScreen', (VICTORY_SCREEN,)),
		CIVICS_SCREEN         : ('CvCivicsScreen', 'CvCivicsScreen', (CIVICS_SCREEN,)),
		HERITAGE_SCREEN       : ('HeritageScreen', 'HeritageScreen', (HERITAGE_SCREEN,)),
		INFO_SCREEN           : ('CvInfoScreen', 'CvInfoScreen', (INFO_SCREEN,)),
		DAWN_OF_MAN           : ('CvDawnOfMan', 'CvDawnOfMan', ()),
		TOP_CIVS              : ('CvTopCivs', 'CvTopCivs', (TOP_CIVS,)),
		FORGETFUL_SCREEN      : ('Forgetful', 'Forgetful', ()),
		TECH_CHOOSER          : ('CvTechChooser', 'CvTechChooser', ()),
		BUILD_LIST_SCREEN     : ('BuildListScreen', 'BuildListScreen', ()),
		DEBUG_INFO_SCREEN     : ('CvDebugInfoScreen', 'CvDebugInfoScreen', ()),
		WB_PLOT               : ('WBPlotScreen', 'WBPlotScreen', (worldBuilderScreen,)),
		WB_EVENT              : ('WBEventScreen', 'WBEventScreen', (worldBuilderScreen,)),
		WB_BUILDING           : ('WBBuildingScreen', 'WBBuildingScreen', (worldBuilderScreen,)),
		WB_CITYDATA           : ('WBCityDataScreen', 'WBCityDataScreen', (worldBuilderScreen,)),
		WB_CITYEDIT           : ('WBCityEditScreen', 'WBCityEditScreen', (worldBuilderScreen,)),
		WB_PROJECT            : ('WBProjectScreen', 'WBProjectScreen', (worldBuilderScreen,)),
		WB_TEAM               : ('WBTeamScreen', 'WBTeamScreen', (worldBuilderScreen,)),
		WB_PLAYER             : ('WBPlayerScreen', 'WBPlayerScreen', (worldBuilderScreen,)),
		WB_PROMOTION          : ('WBPromotionScreen', 'WBPromotionScreen', (worldBuilderScreen,)),
		WB_DIPLOMACY          : ('WBDiplomacyScreen', 'WBDiplomacyScreen', (worldBuilderScreen,)),
		WB_UNITLIST           : ('WBPlayerUnits', 'WBPlayerUnits', (worldBuilderScreen,)),
		WB_RELIGION           : ('WBReligionScreen', 'WBReligionScreen', (worldBuilderScreen,)),
		WB_CORPORATION        : ('WBCorporationScreen', 'WBCorporationScreen', (worldBuilderScreen,)),
		WB_INFO               : ('WBInfoScreen', 'WBInfoScreen', (worldBuilderScreen,)),
		WB_TRADE              : ('WBTradeScreen', 'WBTradeScreen', (worldBuilderScreen,)),
	})

	import CivicData
	CivicData.initCivicData()

# ⛔ THE SCREENS ARE BUILT EAGERLY, AT earlyInit, AND THAT IS THE POINT.
# Building them on first use let the load reach the main interface without the info plane being able to
# answer what the screens ask -- the reads simply happened later, deep inside interfaceScreen(), where a
# missing one is no longer a named Python AttributeError but a NULL handed to the EXE and dereferenced
# there. Deferring the read moved the failure somewhere it cannot be read; it initialized nothing.
# Constructing here puts every screen's reads back on the engine's entry path, so an info plane that is
# not fully stood up fails AT THE MENU, naming the read it could not answer.
_screenFactories = {
	INTRO_MOVIE_SCREEN   : ('CvIntroMovieScreen',   'CvIntroMovieScreen',   ()),
	WONDER_MOVIE_SCREEN  : ('CvWonderMovieScreen',  'CvWonderMovieScreen',  ()),
	VICTORY_MOVIE_SCREEN : ('CvVictoryMovieScreen', 'CvVictoryMovieScreen', ()),
	HALL_OF_FAME         : ('CvHallOfFameScreen',   'CvHallOfFameScreen',   (HALL_OF_FAME,)),
	DAN_QUAYLE_SCREEN    : ('CvDanQuayle',          'CvDanQuayle',          ()),
	SPACE_SHIP_SCREEN    : ('CvSpaceShipScreen',    'CvSpaceShipScreen',    ()),
	PEDIA                : ('Pedia',                'Pedia',                (PEDIA,)),
}

def getScreen(screenId):
	"""The screen for this id.

	⛔ EVERY screen access goes through here, never screenMap[id] directly -- these are ENGINE entry points
	(the intro / wonder / victory movies, the hall of fame, the spaceship, Dan Quayle), so a direct index
	that raises leaves the engine holding nothing, and the failure lands on its side of the call.
	earlyInit builds the factory-owned screens up front; this stays total so an id registered elsewhere is
	still returned, and an unknown one says what is wrong rather than raising a bare KeyError.
	"""
	screen = screenMap.get(screenId)
	if screen is None:
		if screenId not in _screenFactories:
			raise KeyError("screen %s is neither registered nor buildable" % (screenId,))
		moduleName, className, args = _screenFactories[screenId]
		module = __import__(moduleName)
		screen = getattr(module, className)(*args)
		screenMap[screenId] = screen
	return screen

def earlyInit():
	"""Build every factory-owned screen NOW, before the menu.

	⛔ This is deliberately eager. A screen constructor reads the game, so constructing here is what puts
	those reads on the engine's entry path -- which is exactly where we want a not-yet-initialized info
	plane to fail: at the menu, as a named Python error naming the read, instead of surviving to the main
	interface and dying as an access violation inside the EXE holding a NULL we returned.
	"""
	for screenId in _screenFactories:
		getScreen(screenId)
	getUnVictoryScreen()

earlyInit()