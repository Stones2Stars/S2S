## A New Dawn Mod Code
##

from CvPythonExtensions import *
import BugOptions
import BugCore
import CvUtil
# The one data-fetching library: INFO = what an entity CARRIES, ENABLER = can I?, ENUMS = the engine
# enum vocabulary + name->id resolution. A game object's own data is asked OF THAT OBJECT --
# GC.getPlayer(i).getCity(id).getYields(), never a flat class keyed by (owner, id).
GC = CyGlobalContext()
ENABLER = CyEnabler()
ENUMS = CyEnums()
AutomatedOpts = BugCore.game.AutomatedSettings

MODDEROPTION_EVENT_ID = CvUtil.getNewEventID()
CANAUTOBUILD_EVENT_ID = CvUtil.getNewEventID()
CANPLAYERAUTOBUILD_EVENT_ID = CvUtil.getNewEventID()

class AutomatedSettings:
	def __init__(self, eventManager):
		eventManager.addEventHandler("OnLoad", self.onLoadGame)
		eventManager.addEventHandler("GameStart", self.onGameStart)
		eventManager.addEventHandler("ModNetMessage", self.onModNetMessage)

	def onLoadGame(self, argsList):
		self.optionUpdate()

	def onGameStart(self, argsList):
		self.optionUpdate()

	def optionUpdate(self):
		if AutomatedOpts.isReset():
			resetOptions()
		else:
			setXMLOptionsfromIniFile()
		sendNetworkedBugOptions()

	def onModNetMessage(self, argsList):
		protocol, data1, data2, data3, data4 = argsList
		#ModderOptions
		if (protocol == MODDEROPTION_EVENT_ID):
			pPlayer = GC.getPlayer(data1)
			pPlayer.setModderOption(data2, data3)
		elif (protocol == CANAUTOBUILD_EVENT_ID):
			pPlayer = GC.getPlayer(data1)
			pCity = pPlayer.getCity(data2)
			pCity.setAutomatedCanBuild(data3, data4)
		elif (protocol == CANPLAYERAUTOBUILD_EVENT_ID):
			pPlayer = GC.getPlayer(data1)
			pPlayer.setAutomatedCanBuild(data3, data4)

#####################################################
# Module level functions defined in RoMSettings.xml #
#####################################################

def getCanAutoBuildEventID():
	return CANAUTOBUILD_EVENT_ID

def getCanPlayerAutoBuildEventID():
	return CANPLAYERAUTOBUILD_EVENT_ID

def changedReset(option, value):
	resetOptions()
	return True

# The player's own BUG settings that change synchronized state. Each one reaches every client as a message and is
# applied there, the sender included, so synced code reads the owner's value (docs/reference/engine.md, synced code
# reads nothing that differs per machine). Member NAMES, resolved when sent, so this module never binds a member
# the running DLL does not publish.
NETWORKED_BUG_OPTIONS = {
	"CityScreen__ProductionOverflowLimit": "MODDEROPTION_PRODUCTION_OVERFLOW_LIMIT",
	"CityScreen__BaseWeightFood": "MODDEROPTION_BASE_WEIGHT_FOOD",
	"CityScreen__BaseWeightHammer": "MODDEROPTION_BASE_WEIGHT_HAMMER",
	"CityScreen__BaseWeightCommerce": "MODDEROPTION_BASE_WEIGHT_COMMERCE",
	"MainInterface__UseAIPathing": "MODDEROPTION_USE_AI_PATHING",
	"Actions__SentryHealing": "MODDEROPTION_SENTRY_HEALING",
	"Actions__SentryHealingOnlyNeutral": "MODDEROPTION_SENTRY_HEALING_ONLY_NEUTRAL",
	"Actions__PreChopForests": "MODDEROPTION_PRE_CHOP_FORESTS",
	"Actions__PreChopImprovements": "MODDEROPTION_PRE_CHOP_IMPROVEMENTS",
	"MiscHover__UpdateUnitNameOnUpgrade": "MODDEROPTION_UPDATE_UNIT_NAME_ON_UPGRADE",
}

def sendNetworkedBugOption(optionId, value):
	iPlayer = GC.getGame().getActivePlayer()
	if iPlayer < 0:
		return
	eOption = getattr(ModderOptionTypes, NETWORKED_BUG_OPTIONS[optionId])
	CyMessageControl().sendModNetMessage(MODDEROPTION_EVENT_ID, iPlayer, int(eOption), int(value), 0)

def sendNetworkedBugOptions():
	for optionId in NETWORKED_BUG_OPTIONS.keys():
		sendNetworkedBugOption(optionId, BugOptions.getOption(optionId).getValue())

def changedNetworkedBugOption(option, value):
	sendNetworkedBugOption(option.getFullID(), value)

#####################################################
def changedAvoidEnemyUnits(option, value):
	CyMessageControl().sendModNetMessage(MODDEROPTION_EVENT_ID, GC.getGame().getActivePlayer(), int(ModderOptionTypes.MODDEROPTION_AUTO_PILLAGE_AVOID_ENEMY_UNITS), int(value), 0)

def changedAvoidBarbarianCities(option, value):
	CyMessageControl().sendModNetMessage(MODDEROPTION_EVENT_ID, GC.getGame().getActivePlayer(), int(ModderOptionTypes.MODDEROPTION_AUTO_PILLAGE_AVOID_BARBARIAN_CITIES), int(value), 0)

def changedHideAutomatePillage(option, value):
	CyMessageControl().sendModNetMessage(MODDEROPTION_EVENT_ID, GC.getGame().getActivePlayer(), int(ModderOptionTypes.MODDEROPTION_HIDE_AUTO_PILLAGE), int(value), 0)

def changedNoCapturingCities(option, value):
	CyMessageControl().sendModNetMessage(MODDEROPTION_EVENT_ID, GC.getGame().getActivePlayer(), int(ModderOptionTypes.MODDEROPTION_AUTO_HUNT_NO_CITY_CAPTURING), int(value), 0)

def changedAllowUnitSuiciding(option, value):
	CyMessageControl().sendModNetMessage(MODDEROPTION_EVENT_ID, GC.getGame().getActivePlayer(), int(ModderOptionTypes.MODDEROPTION_AUTO_HUNT_ALLOW_UNIT_SUICIDING), int(value), 0)

def changedAutoHuntReturnForUpgrades(option, value):
	CyMessageControl().sendModNetMessage(MODDEROPTION_EVENT_ID, GC.getGame().getActivePlayer(), int(ModderOptionTypes.MODDEROPTION_AUTO_HUNT_RETURN_FOR_UPGRADES), int(value), 0)

def changedHideAutomateHunt(option, value):
	CyMessageControl().sendModNetMessage(MODDEROPTION_EVENT_ID, GC.getGame().getActivePlayer(), int(ModderOptionTypes.MODDEROPTION_HIDE_AUTO_HUNT), int(value), 0)

def changedAutoHuntMinimumAttackOdds(option, value):
	CyMessageControl().sendModNetMessage(MODDEROPTION_EVENT_ID, GC.getGame().getActivePlayer(), int(ModderOptionTypes.MODDEROPTION_AUTO_HUNT_MIN_COMBAT_ODDS), int(value), 0)

def changedCanLeaveBorders(option, value):
	CyMessageControl().sendModNetMessage(MODDEROPTION_EVENT_ID, GC.getGame().getActivePlayer(), int(ModderOptionTypes.MODDEROPTION_AUTO_PATROL_CAN_LEAVE_BORDERS), int(value), 0)

def changedPatrolAllowUnitSuiciding(option, value):
	CyMessageControl().sendModNetMessage(MODDEROPTION_EVENT_ID, GC.getGame().getActivePlayer(), int(ModderOptionTypes.MODDEROPTION_AUTO_PATROL_ALLOW_UNIT_SUICIDING), int(value), 0)

def changedNoPatrolCapturingCities(option, value):
	CyMessageControl().sendModNetMessage(MODDEROPTION_EVENT_ID, GC.getGame().getActivePlayer(), int(ModderOptionTypes.MODDEROPTION_AUTO_PATROL_NO_CITY_CAPTURING), int(value), 0)

def changedHideAutomatePatrol(option, value):
	CyMessageControl().sendModNetMessage(MODDEROPTION_EVENT_ID, GC.getGame().getActivePlayer(), int(ModderOptionTypes.MODDEROPTION_HIDE_AUTO_PATROL), int(value), 0)

def changedAutoPatrolMinimumAttackOdds(option, value):
	CyMessageControl().sendModNetMessage(MODDEROPTION_EVENT_ID, GC.getGame().getActivePlayer(), int(ModderOptionTypes.MODDEROPTION_AUTO_PATROL_MIN_COMBAT_ODDS), int(value), 0)

def changedCanLeaveCity(option, value):
	CyMessageControl().sendModNetMessage(MODDEROPTION_EVENT_ID, GC.getGame().getActivePlayer(), int(ModderOptionTypes.MODDEROPTION_AUTO_DEFENSE_CAN_LEAVE_CITY), int(value), 0)

def changedHideAutomateDefense(option, value):
	CyMessageControl().sendModNetMessage(MODDEROPTION_EVENT_ID, GC.getGame().getActivePlayer(), int(ModderOptionTypes.MODDEROPTION_HIDE_AUTO_DEFENSE), int(value), 0)

def changedAutoDefenseMinimumAttackOdds(option, value):
	CyMessageControl().sendModNetMessage(MODDEROPTION_EVENT_ID, GC.getGame().getActivePlayer(), int(ModderOptionTypes.MODDEROPTION_AUTO_DEFENSE_MIN_COMBAT_ODDS), int(value), 0)

def changedAirUnitCanDefend(option, value):
	CyMessageControl().sendModNetMessage(MODDEROPTION_EVENT_ID, GC.getGame().getActivePlayer(), int(ModderOptionTypes.MODDEROPTION_AUTO_AIR_CAN_DEFEND), int(value), 0)

def changedAirUnitCanRebase(option, value):
	CyMessageControl().sendModNetMessage(MODDEROPTION_EVENT_ID, GC.getGame().getActivePlayer(), int(ModderOptionTypes.MODDEROPTION_AUTO_AIR_CAN_REBASE), int(value), 0)

def changedHideAirAutomations(option, value):
	CyMessageControl().sendModNetMessage(MODDEROPTION_EVENT_ID, GC.getGame().getActivePlayer(), int(ModderOptionTypes.MODDEROPTION_HIDE_AUTO_AIR), int(value), 0)

def changedHideAutoExplore(option, value):
	CyMessageControl().sendModNetMessage(MODDEROPTION_EVENT_ID, GC.getGame().getActivePlayer(), int(ModderOptionTypes.MODDEROPTION_HIDE_AUTO_EXPLORE), int(value), 0)

def changedHideAutoSpread(option, value):
	CyMessageControl().sendModNetMessage(MODDEROPTION_EVENT_ID, GC.getGame().getActivePlayer(), int(ModderOptionTypes.MODDEROPTION_HIDE_AUTO_SPREAD), int(value), 0)

def changedHideAutoCaravan(option, value):
	CyMessageControl().sendModNetMessage(MODDEROPTION_EVENT_ID, GC.getGame().getActivePlayer(), int(ModderOptionTypes.MODDEROPTION_HIDE_AUTO_CARAVAN), int(value), 0)

def changedHideAutoPirate(option, value):
	CyMessageControl().sendModNetMessage(MODDEROPTION_EVENT_ID, GC.getGame().getActivePlayer(), int(ModderOptionTypes.MODDEROPTION_HIDE_AUTO_PIRATE), int(value), 0)

def changedAutoPirateMinimumAttackOdds(option, value):
	CyMessageControl().sendModNetMessage(MODDEROPTION_EVENT_ID, GC.getGame().getActivePlayer(), int(ModderOptionTypes.MODDEROPTION_AUTO_PIRATE_MIN_COMBAT_ODDS), int(value), 0)

def changedHideAutoProtect(option, value):
	CyMessageControl().sendModNetMessage(MODDEROPTION_EVENT_ID, GC.getGame().getActivePlayer(), int(ModderOptionTypes.MODDEROPTION_HIDE_AUTO_PROTECT), int(value), 0)

def changedHideAutoUpgrade(option, value):
	CyMessageControl().sendModNetMessage(MODDEROPTION_EVENT_ID, GC.getGame().getActivePlayer(), int(ModderOptionTypes.MODDEROPTION_HIDE_AUTO_UPGRADE), int(value), 0)

def changedMostExpensive(option, value):
	CyMessageControl().sendModNetMessage(MODDEROPTION_EVENT_ID, GC.getGame().getActivePlayer(), int(ModderOptionTypes.MODDEROPTION_UPGRADE_MOST_EXPENSIVE), int(value), 0)

def changedMostExperienced(option, value):
	CyMessageControl().sendModNetMessage(MODDEROPTION_EVENT_ID, GC.getGame().getActivePlayer(), int(ModderOptionTypes.MODDEROPTION_UPGRADE_MOST_EXPERIENCED), int(value), 0)

def changedMinimumUpgradeGold(option, value):
	CyMessageControl().sendModNetMessage(MODDEROPTION_EVENT_ID, GC.getGame().getActivePlayer(), int(ModderOptionTypes.MODDEROPTION_UPGRADE_MIN_GOLD), int(value), 0)

def changedHideAutoPromote(option, value):
	CyMessageControl().sendModNetMessage(MODDEROPTION_EVENT_ID, GC.getGame().getActivePlayer(), int(ModderOptionTypes.MODDEROPTION_HIDE_AUTO_PROMOTE), int(value), 0)


def setXMLOptionsfromIniFile():
	print "Initializing Automated Settings"
	AutomatedOptions = BugOptions.getOptions("AutomatedSettings").options
	for i in range(len(AutomatedOptions)):
		if AutomatedOptions[i].getKey() != "Reset":
			AutomatedOptions[i].doDirties()

def resetOptions():
	AutomatedOptions = BugOptions.getOptions("AutomatedSettings").options
	for i in range(len(AutomatedOptions)):
		AutomatedOptions[i].resetValue()
	setXMLOptionsfromIniFile()
	AutomatedOptions.setReset(False)