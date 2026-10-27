## A New Dawn Mod Option Code
from CvPythonExtensions import *
import BugCore
import CvUtil

# The one data-fetching library: INFO = what an entity CARRIES, ENABLER = can I?, ENUMS = the engine
# enum vocabulary + name->id resolution. A game object's own data is asked OF THAT OBJECT --
# GC.getPlayer(i).getCity(id).getYields(), never a flat class keyed by (owner, id).
GC = CyGlobalContext()
INFO = CyInfo()
GAME = CyGame()
ENABLER = CyEnabler()
ENUMS = CyEnums()
ANewDawnOpt = BugCore.game.RoMSettings

MODDEROPTION_EVENT_ID = CvUtil.getNewEventID()
MODDERGAMEOPTION_EVENT_ID = CvUtil.getNewEventID()
DIFFICULTY_EVENT_ID = CvUtil.getNewEventID()
COLOR_EVENT_ID = CvUtil.getNewEventID()
CANBUILD_EVENT_ID = CvUtil.getNewEventID()

# A game-wide rule is the host's to decide (docs/reference/engine.md, synced code reads nothing that differs per
# machine): only the host sends it, when it starts a game or changes a rule, and never while a load re-sends
# settings -- the save already carries the rules.
bLoadingSettings = False

def sendModderGameOption(eOption, value):
	if bLoadingSettings or not GAME.isRulesHost():
		return
	CyMessageControl().sendModNetMessage(MODDERGAMEOPTION_EVENT_ID, GAME.getActivePlayer(), int(eOption), int(value), 0)

class ANewDawnSettings:
	def __init__(self, eventManager):
		eventManager.addEventHandler("OnLoad", self.onLoadGame)
		eventManager.addEventHandler("GameStart", self.onGameStart)
		eventManager.addEventHandler("ModNetMessage", self.onModNetMessage)

	def onLoadGame(self,argsList):
		# The save carries the game-wide rules; only the per-player settings are re-sent.
		global bLoadingSettings
		bLoadingSettings = True
		self.optionUpdate()
		bLoadingSettings = False

	def onGameStart(self,argsList):
		self.optionUpdate()

	def optionUpdate(self):
		if ANewDawnOpt.isRoMReset():
			resetOptions()
		else:
			setXMLOptionsfromIniFile()

	def onModNetMessage(self, argsList):
		protocol, data1, data2, data3, data4 = argsList
		#ModderOptions
		if protocol == MODDEROPTION_EVENT_ID:
			pPlayer = GC.getPlayer(data1)
			pPlayer.setModderOption(data2, data3)
		#ModderGameOptions: the host's rule, applied on every client as the message executes
		elif protocol == MODDERGAMEOPTION_EVENT_ID:
			GAME.setModderGameOption(data2, data3)
		#Change Difficulty
		elif protocol == DIFFICULTY_EVENT_ID:
			pPlayer = GC.getPlayer(data1)
			pPlayer.setHandicap(data2, True)
		#Change Color
		elif protocol == COLOR_EVENT_ID:
			pPlayer = GC.getPlayer(data1)
			pPlayer.setColor(data2)
			GC.getMap().updateMinimapColor()
		#disabled/enable worker actions
		elif protocol == CANBUILD_EVENT_ID:
			GAME.setBuildDisabled(data2, data3)

#####################################################
# Module level functions defined in RoMSettings.xml #
#####################################################

def changedRoMReset(option, value):
	resetOptions()
	return True

#####################################################
def changedDefenderWithdraw(option, value):
	sendModderGameOption(ModderGameOptionTypes.MODDERGAMEOPTION_DEFENDER_WITHDRAW, value)

def changedMaxUnitsPerTile(option, value):
	sendModderGameOption(ModderGameOptionTypes.MODDERGAMEOPTION_MAX_UNITS_PER_TILES, value)

def changedFlexibleDifficultyTurnIncrements(option, value):
	CyMessageControl().sendModNetMessage(MODDEROPTION_EVENT_ID, GC.getGame().getActivePlayer(), int(ModderOptionTypes.MODDEROPTION_FLEXIBLE_DIFFICULTY_TURN_INCREMENTS), int(value), 0)

def changedFlexibleDifficultyAITurnIncrements(option, value):
	sendModderGameOption(ModderGameOptionTypes.MODDERGAMEOPTION_FLEXIBLE_DIFFICULTY_AI_TURN_INCREMENTS, value)

def changedMaxBombardDefense(option, value):
	sendModderGameOption(ModderGameOptionTypes.MODDERGAMEOPTION_MAX_BOMBARD_DEFENSE, value)

def changedMinCityDistance(option, value):
	sendModderGameOption(ModderGameOptionTypes.MODDERGAMEOPTION_MIN_CITY_DISTANCE, value)

def changedCanNotClaimOcean(option, value):
	sendModderGameOption(ModderGameOptionTypes.MODDERGAMEOPTION_CANNOT_CLAIM_OCEAN, value)

def changedNoFriendlyPillaging(option, value):
	CyMessageControl().sendModNetMessage(MODDEROPTION_EVENT_ID, GC.getGame().getActivePlayer(), int(ModderOptionTypes.MODDEROPTION_NO_FRIENDLY_PILLAGING), int(value), 0)

def changedEnableFlexibleDifficulty(option, value):
	CyMessageControl().sendModNetMessage(MODDEROPTION_EVENT_ID, GC.getGame().getActivePlayer(), int(ModderOptionTypes.MODDEROPTION_FLEXIBLE_DIFFICULTY), int(value), 0)

def changedFlexibleDifficultyMinimumDiff(option, value):
	CyMessageControl().sendModNetMessage(MODDEROPTION_EVENT_ID, GC.getGame().getActivePlayer(), int(ModderOptionTypes.MODDEROPTION_FLEXIBLE_DIFFICULTY_MIN_DIFFICULTY), int(value - 1), 0)

def changedFlexibleDifficultyMaximumDiff(option, value):
	CyMessageControl().sendModNetMessage(MODDEROPTION_EVENT_ID, GC.getGame().getActivePlayer(), int(ModderOptionTypes.MODDEROPTION_FLEXIBLE_DIFFICULTY_MAX_DIFFICULTY), int(value - 1), 0)

def changedFlexibleDifficultyAIMinimumDiff(option, value):
	sendModderGameOption(ModderGameOptionTypes.MODDERGAMEOPTION_FLEXIBLE_DIFFICULTY_AI_MIN_DIFFICULTY, value - 1)

def changedFlexibleDifficultyAIMaximumDiff(option, value):
	sendModderGameOption(ModderGameOptionTypes.MODDERGAMEOPTION_FLEXIBLE_DIFFICULTY_AI_MAX_DIFFICULTY, value - 1)

def changedFlexibleDifficultyMinRank(option, value):
	CyMessageControl().sendModNetMessage(MODDEROPTION_EVENT_ID, GC.getGame().getActivePlayer(), int(ModderOptionTypes.MODDEROPTION_FLEXIBLE_DIFFICULTY_MIN_RANK), int(value), 0)

def changedFlexibleDifficultyMaxRank(option, value):
	CyMessageControl().sendModNetMessage(MODDEROPTION_EVENT_ID, GC.getGame().getActivePlayer(), int(ModderOptionTypes.MODDEROPTION_FLEXIBLE_DIFFICULTY_MAX_RANK), int(value), 0)

def changedDepletionMod(option, value):
	sendModderGameOption(ModderGameOptionTypes.MODDERGAMEOPTION_RESOURCE_DEPLETION, value)

def changedGreaterGreatFarmer(option, value):
	sendModderGameOption(ModderGameOptionTypes.MODDERGAMEOPTION_GREATER_GREAT_FARMER, value)

def changedBetterAirInterception(option, value):
	sendModderGameOption(ModderGameOptionTypes.MODDERGAMEOPTION_BETTER_INTERCETION, value)

def changedMaxRebaseRange(option, value):
	sendModderGameOption(ModderGameOptionTypes.MODDERGAMEOPTION_AIRLIFT_RANGE, value)

def changedMercyRule(option, value):
	sendModderGameOption(ModderGameOptionTypes.MODDERGAMEOPTION_MERCY_RULE, value)

def changedRealisiticDiplomacy(option, value):
	sendModderGameOption(ModderGameOptionTypes.MODDERGAMEOPTION_REALISTIC_DIPLOMACY, value)

def changedShowRevCivics(option, value):
	CyMessageControl().sendModNetMessage(MODDEROPTION_EVENT_ID, GC.getGame().getActivePlayer(), int(ModderOptionTypes.MODDEROPTION_SHOW_REV_EFFECTS), int(value), 0)

def changedBattlefieldPromotions(option, value):
	sendModderGameOption(ModderGameOptionTypes.MODDERGAMEOPTION_BATTLEFIELD_PROMOTIONS, value)

def changedStarsigns(option, value):
	sendModderGameOption(ModderGameOptionTypes.MODDERGAMEOPTION_STARSIGNS, value)

def changedHideReplacedBuildings(option, value):
	CyMessageControl().sendModNetMessage(MODDEROPTION_EVENT_ID, GC.getGame().getActivePlayer(), int(ModderOptionTypes.MODDEROPTION_HIDE_REPLACED_BUILDINGS), int(value), 0)

def changedImprovedXP(option, value):
	sendModderGameOption(ModderGameOptionTypes.MODDERGAMEOPTION_IMPROVED_XP, value)

def changedPlayerColor(option, value):
	iColor = value - 1
	if iColor >= 0:
		CyMessageControl().sendModNetMessage(COLOR_EVENT_ID, GC.getGame().getActivePlayer(), iColor, 0, 0)

def updateAliveCivsOption():
	import BugGame
	aliveCivsOption = BugGame.ANewDawnSettings.AliveCivilization
	descs = []
	for iPlayer in range(GC.getMAX_PC_PLAYERS()):
		CyPlayer = GC.getPlayer(iPlayer)
		if CyPlayer.isHuman() and CyPlayer.isAlive():
			descs.append(INFO.getShortDescription("CIVILIZATION_", CyPlayer.getCivilizationType(), 0))
	descs.sort()
	aliveCivsOption.setValues(descs)

def changedCurrentDifficulty(option, value):
	if (value > 0):
		CyMessageControl().sendModNetMessage(DIFFICULTY_EVENT_ID, GC.getGame().getActivePlayer(), value - 1, 0, 0)

def changedUseLandmarkNames(option, value):
	CyMessageControl().sendModNetMessage(MODDEROPTION_EVENT_ID, GC.getGame().getActivePlayer(), int(ModderOptionTypes.MODDEROPTION_USE_LANDMARK_NAMES), int(value), 0)

def changedHideUnavailableBuilds(option, value):
	CyMessageControl().sendModNetMessage(MODDEROPTION_EVENT_ID, GC.getGame().getActivePlayer(), int(ModderOptionTypes.MODDEROPTION_HIDE_UNAVAILBLE_BUILDS), int(value), 0)


def changedIgnoreDisabledBuildingAlerts(option, value):
	CyMessageControl().sendModNetMessage(MODDEROPTION_EVENT_ID, GC.getGame().getActivePlayer(), int(ModderOptionTypes.MODDEROPTION_IGNORE_DISABLED_ALERTS), int(value), 0)

def changedReligionDecay(option, value):
	sendModderGameOption(ModderGameOptionTypes.MODDERGAMEOPTION_RELIGION_DECAY, value)

def changedMultipleReligionSpread(option, value):
	sendModderGameOption(ModderGameOptionTypes.MODDERGAMEOPTION_MULTIPLE_RELIGION_SPREAD, value)

def changedAllowTerraforming(option, value):
	CyMessageControl().sendModNetMessage(CANBUILD_EVENT_ID, GC.getGame().getActivePlayer(), GC.getInfoTypeForString("BUILD_TERRAFORM_GRASS"), not value, 0)
	CyMessageControl().sendModNetMessage(CANBUILD_EVENT_ID, GC.getGame().getActivePlayer(), GC.getInfoTypeForString("BUILD_TERRAFORM_PLAINS"), not value, 0)
	CyMessageControl().sendModNetMessage(CANBUILD_EVENT_ID, GC.getGame().getActivePlayer(), GC.getInfoTypeForString("BUILD_TERRAFORM_TUNDRA"), not value, 0)

def changedReforestation(option, value):
	CyMessageControl().sendModNetMessage(CANBUILD_EVENT_ID, GC.getGame().getActivePlayer(), GC.getInfoTypeForString("BUILD_YOUNG_FOREST"), not value, 0)
	CyMessageControl().sendModNetMessage(CANBUILD_EVENT_ID, GC.getGame().getActivePlayer(), GC.getInfoTypeForString("BUILD_PLANT_JUNGLE"), not value, 0)

def changedSeaTunnels(option, value):
	CyMessageControl().sendModNetMessage(CANBUILD_EVENT_ID, GC.getGame().getActivePlayer(), GC.getInfoTypeForString("BUILD_TUNNEL"), not value, 0)

def changedNoAutoCorporationFounding(option, value):
	sendModderGameOption(ModderGameOptionTypes.MODDERGAMEOPTION_NO_AUTO_CORPORATION_FOUNDING, value)

def changedWarPrizes(option, value):
	GC.setDefineINT("WAR_PRIZES", int(value))

def changedNextTraitCultureReqPercent(option, value):
	sendModderGameOption(ModderGameOptionTypes.MODDERGAMEOPTION_NEXT_TRAIT_CULTURE_REQ_PERCENT, value)

def changedFlexibleDifficultyAI(option, value):
	sendModderGameOption(ModderGameOptionTypes.MODDERGAMEOPTION_AI_USE_FLEXIBLE_DIFFICULTY, value)

def changedHistoricalAccurateCalendar(option, value):
	sendModderGameOption(ModderGameOptionTypes.MODDERGAMEOPTION_USE_HISTORICAL_ACCURATE_CALENDAR, value)

def changedInfrastructureIgnoresImprovements(option, value):
	CyMessageControl().sendModNetMessage(MODDEROPTION_EVENT_ID, GC.getGame().getActivePlayer(), int(ModderOptionTypes.MODDEROPTION_INFRASTRUCTURE_IGNORES_IMPROVEMENTS), int(value), 0)

def changedFogWarDecay(option, value):
	sendModderGameOption(ModderGameOptionTypes.MODDERGAMEOPTION_FOGWAR_DECAY, value)

def changedFogWarNbTurns(option, value):
	sendModderGameOption(ModderGameOptionTypes.MODDERGAMEOPTION_FOGWAR_NBTURNS, value)

def changedMinorCivTech(option, value):
	sendModderGameOption(ModderGameOptionTypes.MODDERGAMEOPTION_MINORCIV_TECH, value)

def changedAssassinateMinimal(option, value):
	sendModderGameOption(ModderGameOptionTypes.MODDERGAMEOPTION_ASSASSINATE_MINIMAL, value)

def changedAssassinateChoice(option, value):
	sendModderGameOption(ModderGameOptionTypes.MODDERGAMEOPTION_ASSASSINATE_CHOICE, value)



def setXMLOptionsfromIniFile():
	print "Initializing A New Dawn Settings"
	changedDefenderWithdraw(ANewDawnOpt, ANewDawnOpt.isDefenderWithdraw())
	changedMaxUnitsPerTile(ANewDawnOpt, ANewDawnOpt.getMaxUnitsPerTile())
	changedFlexibleDifficultyTurnIncrements(ANewDawnOpt, ANewDawnOpt.getFlexibleDifficultyTurnIncrements())
	changedFlexibleDifficultyAITurnIncrements(ANewDawnOpt, ANewDawnOpt.getFlexibleDifficultyAITurnIncrements())
	changedMaxBombardDefense(ANewDawnOpt, ANewDawnOpt.getMaxBombardDefense())
	changedMinCityDistance(ANewDawnOpt, ANewDawnOpt.getMinCityDistance())
	changedCanNotClaimOcean(ANewDawnOpt, ANewDawnOpt.isCanNotClaimOcean())
	changedNoFriendlyPillaging(ANewDawnOpt, ANewDawnOpt.isNoFriendlyPillaging())
	changedEnableFlexibleDifficulty(ANewDawnOpt, ANewDawnOpt.isEnableFlexibleDifficulty())
	changedFlexibleDifficultyMinimumDiff(ANewDawnOpt, ANewDawnOpt.getFlexibleDifficultyMinimumDiff())
	changedFlexibleDifficultyMaximumDiff(ANewDawnOpt, ANewDawnOpt.getFlexibleDifficultyMaximumDiff())
	changedFlexibleDifficultyAIMinimumDiff(ANewDawnOpt, ANewDawnOpt.getFlexibleDifficultyAIMinimumDiff())
	changedFlexibleDifficultyAIMaximumDiff(ANewDawnOpt, ANewDawnOpt.getFlexibleDifficultyAIMaximumDiff())
	changedFlexibleDifficultyMinRank(ANewDawnOpt, ANewDawnOpt.getFlexibleDifficultyMinRank())
	changedFlexibleDifficultyMaxRank(ANewDawnOpt, ANewDawnOpt.getFlexibleDifficultyMaxRank())
	changedDepletionMod(ANewDawnOpt, ANewDawnOpt.isDepletionMod())
	changedGreaterGreatFarmer(ANewDawnOpt, ANewDawnOpt.isGreaterGreatFarmer())
	changedBetterAirInterception(ANewDawnOpt, ANewDawnOpt.isBetterAirInterception())
	changedMaxRebaseRange(ANewDawnOpt, ANewDawnOpt.getMaxRebaseRange())
	changedMercyRule(ANewDawnOpt, ANewDawnOpt.isMercyRule())
	changedRealisiticDiplomacy(ANewDawnOpt, ANewDawnOpt.isRealisiticDiplomacy())
	changedShowRevCivics(ANewDawnOpt, ANewDawnOpt.isShowRevCivics())
	changedBattlefieldPromotions(ANewDawnOpt, ANewDawnOpt.isBattlefieldPromotions())
	changedHideReplacedBuildings(ANewDawnOpt, ANewDawnOpt.isHideReplacedBuildings())
	changedImprovedXP(ANewDawnOpt, ANewDawnOpt.isImprovedXP())
	changedUseLandmarkNames(ANewDawnOpt, ANewDawnOpt.isUseLandmarkNames())
	changedHideUnavailableBuilds(ANewDawnOpt, ANewDawnOpt.isHideUnavailableBuilds())
	changedIgnoreDisabledBuildingAlerts(ANewDawnOpt, ANewDawnOpt.isIgnoreDisabledBuildingAlerts())
	changedReligionDecay(ANewDawnOpt, ANewDawnOpt.isReligionDecay())
	changedMultipleReligionSpread(ANewDawnOpt, ANewDawnOpt.isMultipleReligionSpread())
	changedAllowTerraforming(ANewDawnOpt, ANewDawnOpt.isAllowTerraforming())
	changedReforestation(ANewDawnOpt, ANewDawnOpt.isReforestation())
	changedSeaTunnels(ANewDawnOpt, ANewDawnOpt.isSeaTunnels())
	changedWarPrizes(ANewDawnOpt, ANewDawnOpt.isWarPrizes())
	changedNextTraitCultureReqPercent(ANewDawnOpt, ANewDawnOpt.getNextTraitCultureReqPercent())
	changedFlexibleDifficultyAI(ANewDawnOpt, ANewDawnOpt.isFlexibleDifficultyAI())
	changedHistoricalAccurateCalendar(ANewDawnOpt, ANewDawnOpt.isHistoricalAccurateCalendar())
	changedInfrastructureIgnoresImprovements(ANewDawnOpt, ANewDawnOpt.isInfrastructureIgnoresImprovements())
	changedFogWarDecay(ANewDawnOpt, ANewDawnOpt.isFogWarDecay())
	changedFogWarNbTurns(ANewDawnOpt, ANewDawnOpt.getFogWarNbTurns())
	changedMinorCivTech(ANewDawnOpt, ANewDawnOpt.getMinorCivTech())
	changedAssassinateMinimal(ANewDawnOpt, ANewDawnOpt.isAssassinateMinimal())
	changedAssassinateChoice(ANewDawnOpt, ANewDawnOpt.isAssassinateChoice())
		
	ANewDawnOpt.setPlayerColor(0)
	ANewDawnOpt.setCurrentDifficulty(0)

def resetOptions():
	import BugOptions
	ANewDawnOptions = BugOptions.getOptions("RoMSettings").options
	for i in range(len(ANewDawnOptions)):
		ANewDawnOptions[i].resetValue()
	setXMLOptionsfromIniFile()
	ANewDawnOpt.setRoMReset(False)