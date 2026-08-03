## Civ4lerts
## This class extends the built in event manager and overrides various
## event handlers to display alerts about important game situations.
##
## [*] = Already implemented in the Civ4lerts mod
## [o] = Partially implemented in the Civ4lerts mod
## [x] = Already implemented in CivIV
## [?] = Not sure if this applies in CivIV
##
## Golden Age turns left
## At Year 1000 B.C. (QSC Save Submission)
## Within 10 tiles of domination limit
## There is new technology for sale
## There is a new luxury resource for sale
## There is a new strategic resource for sale
## There is a new bonus resource for sale
## We can sell a technology
## We can sell a luxury resource
## We can sell a strategic resource
## We can sell a bonus resource
## [*] Rival has lots of cash
## [*] Rival has lots of cash per turn
## [x] Rival has changed civics
## Rival has entered a new Era
## Trade deal expires next turn
## [o] Enemy at war is willing to negotiate
## [x] There are foreign units in our territory
## City is about to riot or rioting
## [*] City has grown or shrunk
## City has shrunk
## [*] City is unhealthy
## [*] City is angry
## City specialists reassigned
## [*] City is about to grow
## City is about to starve
## [*] City is about to grow into unhealthiness
## [*] City is about to grow into anger
## City is in resistance
## [?] City is wasting food
## City is working unimproved tiles
## Disconnected resources in our territory
## City is about to produce a great person
##
## Other:
## City is under cultural pressure


from CvPythonExtensions import *
import CvUtil
import AttitudeUtil
import BugCore
import BugUtil
import CityUtil
import TradeUtil

# Must set alerts to "not immediate" to have icons show up
# Need a healthy person icon
HEALTHY_ICON = "Art/Interface/Buttons/General/unhealthy_person.dds"
UNHEALTHY_ICON = "Art/Interface/Buttons/General/unhealthy_person.dds"

HAPPY_ICON = "Art/Interface/Buttons/General/happy_person.dds"
UNHAPPY_ICON = "Art/Interface/mainscreen/cityscreen/angry_citizen.dds"

### Globals

# The one data-fetching library ([DEC-cy-not-fixed]): STATE = live state, ENABLER = availability,
# ENUMS = the engine enum vocabulary + name->id resolution.
GC = CyGlobalContext()
GAME = GC.getGame()
STATE = CyState()
INFO = CyInfo()
ENABLER = CyEnabler()
ENUMS = CyEnums()
TRNSLTR = CyTranslator()

# An order names its subject on the info plane, which is addressed by PREFIX + id.
ORDER_PREFIX = {
	int(OrderTypes.ORDER_TRAIN):     "UNIT_",
	int(OrderTypes.ORDER_CONSTRUCT): "BUILDING_",
	int(OrderTypes.ORDER_CREATE):    "PROJECT_",
	int(OrderTypes.ORDER_MAINTAIN):  "PROCESS_",
}

EVENT_MESSAGE_TIME_LONG = GC.getDefineINT("EVENT_MESSAGE_TIME_LONG")
Civ4lertsOpt = BugCore.game.Civ4lerts


## Initialization

class Civ4lerts:

	def __init__(self, eventManager):
		cityEvent = BeginActivePlayerTurnCityAlertManager(eventManager)
		cityEvent.add(CityOccupation(eventManager))
		cityEvent.add(CityGrowth(eventManager))
		cityEvent.add(CityHealthiness(eventManager))
		cityEvent.add(CityHappiness(eventManager))
		cityEvent.add(CanHurryPopulation(eventManager))
		cityEvent.add(CanHurryGold(eventManager))

		cityEvent = EndTurnReadyCityAlertManager(eventManager)
		cityEvent.add(CityPendingGrowth(eventManager))

		GoldTrade(eventManager)
		GoldPerTurnTrade(eventManager)
		RefusesToTalk(eventManager)
		WorstEnemy(eventManager)


## Displaying Alert Messages
def addMessage(iPlayer, szTxt, icon=None, iX=-1, iY=-1, bOffArrow=False, bOnArrow=False):
	"Displays an on-screen message."
	"""
	Make these alerts optionally show a delayable popup with various options.
	a) show:

	Happy: Zoom to City, Turn OFF avoid growth, Whip (maybe?), Ignore
	Unhappy:  Zoom to City, Turn on Avoid Growth, Suggest cheapest military unit (with right civic), Open Resources screen in FA, Ignore. (for future = suggest building)

	Healthy: Zoom to City, Turn OFF avoid growth, Ignore
	Unhealthy:  Zoom to City, Turn on Avoid Growth, Whip population, Open Resources screen in FA, Ignore. (for future = suggest building)

	Growth: Zoom to City, Turn on avoid Growth, Whip, Ignore
	Starvation: Zoom to City, Turn on avoid Growth, Ignore

	Culture:  Zoom to City, Ignore
	"""
	CvUtil.sendMessage(szTxt, iPlayer, EVENT_MESSAGE_TIME_LONG, icon, -1, iX, iY, bOffArrow, bOnArrow)

## Base Alert Class
class AbstractStatefulAlert:
	"""
	Provides a base class and several convenience functions for
	implementing an alert that retains state between turns.
	"""
	def __init__(self, eventManager):
		eventManager.addEventHandler("GameStart", self.onGameStart)
		eventManager.addEventHandler("OnLoad", self.onLoadGame)

	def onGameStart(self, argsList):
		self._init()
		self._reset()

	def onLoadGame(self, argsList):
		self._init()
		self._reset()
		return 0

	def _init(self):
		"Initializes globals that could not be done in __init__."
		pass

	def _reset(self):
		"Resets the state for this alert."
		pass


## City Alert Managers
#
#	⚑ A city is an (iOwner, iCityId) PAIR everywhere below, never a handle. That is what an engine callback
#	hands over now (Cy::PyIdentity) and what every CyState read is addressed by, so the alert framework's
#	tracking key and its subject are the SAME value -- the old getCityId(city) round-trip has nothing left to do.

class AbstractCityAlertManager(AbstractStatefulAlert):
	"""
	Triggered when cities are acquired or lost, this event manager passes
	each off to a set of alert checkers.

	All of the alerts are reset when the game is loaded or started.
	"""
	def __init__(self, eventManager):
		AbstractStatefulAlert.__init__(self, eventManager)
		eventManager.addEventHandler("cityAcquiredAndKept", self.onCityAcquiredAndKept)
		eventManager.addEventHandler("cityLost", self.onCityLost)
		self.alerts = []

	def add(self, alert):
		self.alerts.append(alert)
		alert.init()

	def onCityAcquiredAndKept(self, argsList):
		#iOwnerOld, iOwnerNew, cityId, bConquest, bTrade = argsList
		if argsList[1] == STATE.getActivePlayer():
			self._resetCity(argsList[2])

	def onCityLost(self, argsList):
		cityId = argsList[0]
		if cityId[0] == STATE.getActivePlayer():
			self._discardCity(cityId)

	def checkAllActivePlayerCities(self):
		"Loops over active player's cities, telling each alert to perform its check."
		ePlayer = STATE.getActivePlayer()
		for iCity in STATE.getCityIds(ePlayer):
			for alert in self.alerts:
				alert.checkCity((ePlayer, iCity), ePlayer)

	def _init(self):
		"Initializes each alert."
		for alert in self.alerts:
			alert.init()

	def _reset(self):
		"Resets each alert."
		for alert in self.alerts:
			alert.reset()

	def _resetCity(self, cityId):
		"tells each alert to check the state of the given city -- no alerts are displayed."
		for alert in self.alerts:
			alert.resetCity(cityId)

	def _discardCity(self, cityId):
		"tells each alert to discard the state of the given city."
		for alert in self.alerts:
			alert.discardCity(cityId)

class BeginActivePlayerTurnCityAlertManager(AbstractCityAlertManager):
	"""
	Extends AbstractCityAlertManager to loop over all of the active player's
	cities at the start of their turn.
	"""
	def __init__(self, eventManager):
		AbstractCityAlertManager.__init__(self, eventManager)
		eventManager.addEventHandler("BeginActivePlayerTurn", self.onBeginActivePlayerTurn)

	def onBeginActivePlayerTurn(self, argsList):
		"Loops over active player's cities, telling each to perform its check."
		self.checkAllActivePlayerCities()

class EndTurnReadyCityAlertManager(AbstractCityAlertManager):
	"""
	Extends AbstractCityAlertManager to loop over all of the active player's
	cities at the end of their turn (the moment the End Turn button turns red).
	"""
	def __init__(self, eventManager):
		AbstractCityAlertManager.__init__(self, eventManager)
		eventManager.addEventHandler("endTurnReady", self.onEndTurnReady)

	def onEndTurnReady(self, argsList):
		"Loops over active player's cities, telling each to perform its check."
		self.checkAllActivePlayerCities()


## City Alerts

class AbstractCityAlert:
	"""
	Tracks cities from turn-to-turn and checks each at the end of every game turn
	to see if the alert should be displayed.
	"""
	def __init__(self, eventManager):
		"Performs static initialization that doesn't require game data."
		pass

	def checkCity(self, cityId, iPlayer):
		"Checks the city, updates its tracked state and possibly displays an alert."
		pass

	def init(self):
		"Initializes globals that could not be done in __init__ and resets the data."
		self._beforeReset()

	def reset(self):
		"Clears state kept for each city."
		self._beforeReset()
		ePlayer = STATE.getActivePlayer()
		for iCity in STATE.getCityIds(ePlayer):
			self.resetCity((ePlayer, iCity))

	def _beforeReset(self):
		"Performs clearing of state before looping over cities."
		pass

	def resetCity(self, cityId):
		"Checks the city and updates its tracked state."
		pass

	def discardCity(self, cityId):
		"Discards the tracked state of the city."
		pass

class AbstractCityTestAlert(AbstractCityAlert):
	"""
	Extends the basic city alert by applying a boolean test to each city, tracking the results,
	and displaying an alert whenever a city switches or will switch state on the following turn.

	State: set of city IDs that pass the test.
	"""
	def __init__(self, eventManager):
		AbstractCityAlert.__init__(self, eventManager)

	def checkCity(self, cityId, iPlayer):
		message = None
		passes = self._passesTest(cityId)
		passed = cityId in self.cities
		if passes != passed:
			# City switched this turn, save new state and display an alert
			if passes:
				self.cities.add(cityId)
				if self._isShowAlert(passes):
					message, icon = self._getAlertMessageIcon(cityId, passes)
			else:
				self.cities.discard(cityId)
				if self._isShowAlert(passes):
					message, icon = self._getAlertMessageIcon(cityId, passes)
		elif self._isShowPendingAlert(passes):
			# See if city will switch next turn
			willPass = self._willPassTest(cityId)
			if passed != willPass:
				message, icon = self._getPendingAlertMessageIcon(cityId, willPass)
		if message:
			aPos = STATE.getCityPosition(cityId[0], cityId[1])
			addMessage(iPlayer, message, icon, aPos[0], aPos[1], True, True)

	def _passedTest(self, cityId):
		"Returns True if the city passed the test last turn."
		return cityId in self.cities

	def _passesTest(self, cityId):
		"Returns True if the city passes the test."
		return False

	def _willPassTest(self, cityId):
		"Returns True if the city will pass the test next turn based on current conditions."
		return False

	def _beforeReset(self):
		self.cities = set()

	def resetCity(self, cityId):
		if self._passesTest(cityId):
			self.cities.add(cityId)

	def discardCity(self, cityId):
		self.cities.discard(cityId)

	def _isShowAlert(self, passes):
		"Returns True if the alert is enabled."
		return False

	def _getAlertMessageIcon(self, cityId, passes):
		"Returns a tuple of the message and icon to use for the alert."
		return (None, None)

	def _isShowPendingAlert(self, passes):
		"Returns True if the alert is enabled."
		return False

	def _getPendingAlertMessageIcon(self, cityId, passes):
		"Returns a tuple of the message and icon to use for the pending alert."
		return (None, None)

# Population

class CityPendingGrowth(AbstractCityAlert):
	"""
	Displays an alert when a city's population will change next turn.
	State: None.
	"""
	def __init__(self, eventManager):
		AbstractCityAlert.__init__(self, eventManager)

	def checkCity(self, cityId, iPlayer):
		if Civ4lertsOpt.isShowCityPendingGrowthAlert():
			szName = STATE.getCityName(cityId[0], cityId[1])
			iPop = STATE.getCityPopulation(cityId[0], cityId[1])
			aPos = STATE.getCityPosition(cityId[0], cityId[1])
			if CityUtil.willGrowThisTurn(cityId):
				addMessage(
					iPlayer, TRNSLTR.getText("TXT_KEY_CIV4LERTS_ON_CITY_PENDING_GROWTH", (szName, iPop + 1)),
					"Art/Interface/Symbols/Food/food05.dds", aPos[0], aPos[1], True, True
				)
			elif CityUtil.willShrinkThisTurn(cityId):
				addMessage(
					iPlayer, TRNSLTR.getText("TXT_KEY_CIV4LERTS_ON_CITY_PENDING_SHRINKAGE", (szName, iPop - 1)),
					"Art/Interface/Symbols/Food/food05.dds", aPos[0], aPos[1], True, True
				)

class CityGrowth(AbstractCityAlert):
	"""
	Displays an alert when a city's population changes.
	State: map of populations by city ID.
	"""
	def __init__(self, eventManager):
		AbstractCityAlert.__init__(self, eventManager)

	def checkCity(self, cityId, iPlayer):
		if cityId not in self.populations:
			self.resetCity(cityId)
		else:
			iPop = STATE.getCityPopulation(cityId[0], cityId[1])
			iOldPop = self.populations[cityId]
			aCountdowns = STATE.getCountdowns(cityId[0], cityId[1])
			iWhipCounter = aCountdowns[CityCountdownKind.COUNTDOWN_HURRY_ANGER]
			iConscriptCounter = aCountdowns[CityCountdownKind.COUNTDOWN_CONSCRIPT_ANGER]

			bWhipOrDraft = False
			if iWhipCounter > self.CityWhipCounter[cityId] or iConscriptCounter > self.CityConscriptCounter[cityId]:
				bWhipOrDraft = True

			if Civ4lertsOpt.isShowCityGrowthAlert():
				szName = STATE.getCityName(cityId[0], cityId[1])
				aPos = STATE.getCityPosition(cityId[0], cityId[1])
				if iPop > iOldPop:
					addMessage(
						iPlayer, TRNSLTR.getText("TXT_KEY_CIV4LERTS_ON_CITY_GROWTH", (szName, iPop)),
						"Art/Interface/Symbols/Food/food05.dds", aPos[0], aPos[1], True, True
					)
				elif iPop < iOldPop and not bWhipOrDraft:
					addMessage(
						iPlayer, TRNSLTR.getText("TXT_KEY_CIV4LERTS_ON_CITY_SHRINKAGE", (szName, iPop)),
						"Art/Interface/Symbols/Food/food05.dds", aPos[0], aPos[1], True, True
					)

			self.populations[cityId] = iPop
			self.CityWhipCounter[cityId] = iWhipCounter
			self.CityConscriptCounter[cityId] = iConscriptCounter

	def _beforeReset(self):
		self.populations = dict()
		self.CityWhipCounter = dict()
		self.CityConscriptCounter = dict()

	def resetCity(self, cityId):
		aCountdowns = STATE.getCountdowns(cityId[0], cityId[1])
		self.populations[cityId] = STATE.getCityPopulation(cityId[0], cityId[1])
		self.CityWhipCounter[cityId] = aCountdowns[CityCountdownKind.COUNTDOWN_HURRY_ANGER]
		self.CityConscriptCounter[cityId] = aCountdowns[CityCountdownKind.COUNTDOWN_CONSCRIPT_ANGER]

	def discardCity(self, cityId):
		if cityId in self.populations:
			del self.populations[cityId], self.CityWhipCounter[cityId], self.CityConscriptCounter[cityId]

# Happiness and Healthiness

class CityHappiness(AbstractCityTestAlert):
	"""
	Displays an event when a city goes from happy to angry or vice versa.

	Test: True if the city is unhappy.
	"""
	def __init__(self, eventManager):
		AbstractCityTestAlert.__init__(self, eventManager)

	def init(self):
		AbstractCityAlert.init(self)
		self.kiTempHappy = GC.getDefineINT("TEMP_HAPPY")

	def _passesTest(self, cityId):
		# angryPopulation is a FINAL-STATE calculation over the channels, not a channel of its own
		# ([patterns.md] THE TWO READ ROLES, rule 6): clamp(anger - happiness, 0, pop).
		aWellbeing = STATE.getRealizedWellbeing(cityId[0], cityId[1], 0)
		iDeficit = aWellbeing[WellbeingChannel.WELLBEING_ANGER] - aWellbeing[WellbeingChannel.WELLBEING_HAPPINESS]
		return iDeficit > 0

	def _willPassTest(self, cityId):
		if CityUtil.willGrowThisTurn(cityId):
			iExtra = 1
		elif CityUtil.willShrinkThisTurn(cityId):
			iExtra = -1
		else:
			iExtra = 0
		# The channels are x100 native; this comparison is in whole citizens, so it reduces at the point of use
		# ([DEC-fixedpoint-x100]: no getter reduces).
		aWellbeing = STATE.getRealizedWellbeing(cityId[0], cityId[1], iExtra)
		iHappy = aWellbeing[WellbeingChannel.WELLBEING_HAPPINESS] / 100
		iUnhappy = aWellbeing[WellbeingChannel.WELLBEING_ANGER] / 100
		aCountdowns = STATE.getCountdowns(cityId[0], cityId[1])
		aDecaying = (
			(CityCountdownKind.COUNTDOWN_HURRY_ANGER, CityCountdownKind.COUNTDOWN_HURRY_ANGER_PERIOD),
			(CityCountdownKind.COUNTDOWN_CONSCRIPT_ANGER, CityCountdownKind.COUNTDOWN_CONSCRIPT_ANGER_PERIOD),
			(CityCountdownKind.COUNTDOWN_DEFY_RESOLUTION_ANGER, CityCountdownKind.COUNTDOWN_DEFY_RESOLUTION_ANGER_PERIOD),
		)
		for eTimer, ePeriod in aDecaying:
			iTimer = aCountdowns[eTimer]
			iPeriod = aCountdowns[ePeriod]
			if iUnhappy > 0 and iTimer > 0 and iPeriod > 0 and not iTimer % iPeriod:
				iUnhappy -= 1
		if iUnhappy > 0 and aCountdowns[CityCountdownKind.COUNTDOWN_ESPIONAGE_HAPPINESS] > 0:
			iUnhappy -= 1
		if iHappy > 0 and aCountdowns[CityCountdownKind.COUNTDOWN_HAPPINESS] == 1:
			iHappy -= self.kiTempHappy
		if iHappy < 0:
			iHappy = 0
		if iUnhappy < 0:
			iUnhappy = 0
		return iHappy < iUnhappy

	def _isShowAlert(self, passes):
		return Civ4lertsOpt.isShowCityHappinessAlert()

	def _getAlertMessageIcon(self, cityId, passes):
		if passes:
			return (TRNSLTR.getText("TXT_KEY_CIV4LERTS_ON_CITY_UNHAPPY", (STATE.getCityName(cityId[0], cityId[1]), )), UNHAPPY_ICON)
		return (TRNSLTR.getText("TXT_KEY_CIV4LERTS_ON_CITY_HAPPY", (STATE.getCityName(cityId[0], cityId[1]), )), HAPPY_ICON)

	def _isShowPendingAlert(self, passes):
		return Civ4lertsOpt.isShowCityPendingHappinessAlert()

	def _getPendingAlertMessageIcon(self, cityId, passes):
		if passes:
			return (TRNSLTR.getText("TXT_KEY_CIV4LERTS_ON_CITY_PENDING_UNHAPPY", (STATE.getCityName(cityId[0], cityId[1]), )), UNHAPPY_ICON)
		return (TRNSLTR.getText("TXT_KEY_CIV4LERTS_ON_CITY_PENDING_HAPPY", (STATE.getCityName(cityId[0], cityId[1]), )), HAPPY_ICON)

class CityHealthiness(AbstractCityTestAlert):
	"""
	Displays an event when a city goes from healthy to sick or vice versa.

	Test: True if the city is unhealthy.
	"""
	def __init__(self, eventManager):
		AbstractCityTestAlert.__init__(self, eventManager)

	def _passesTest(self, cityId):
		# healthRate is health summed AGAINST unhealth -- a final-state calculation over the channels, not a
		# channel of its own ([patterns.md] THE TWO READ ROLES, rule 6).
		aWellbeing = STATE.getRealizedWellbeing(cityId[0], cityId[1], 0)
		return aWellbeing[WellbeingChannel.WELLBEING_HEALTH] < aWellbeing[WellbeingChannel.WELLBEING_UNHEALTH]

	def _willPassTest(self, cityId):
		if CityUtil.willGrowThisTurn(cityId):
			iExtra = 1
		elif CityUtil.willShrinkThisTurn(cityId):
			iExtra = -1
		else:
			iExtra = 0
		aWellbeing = STATE.getRealizedWellbeing(cityId[0], cityId[1], iExtra)
		iHealthRate = (aWellbeing[WellbeingChannel.WELLBEING_HEALTH]
		               - aWellbeing[WellbeingChannel.WELLBEING_UNHEALTH]) / 100
		aCountdowns = STATE.getCountdowns(cityId[0], cityId[1])
		if aCountdowns[CityCountdownKind.COUNTDOWN_ESPIONAGE_HEALTH] > 0:
			iHealthRate += 1
		return iHealthRate < 0

	def _isShowAlert(self, passes):
		return Civ4lertsOpt.isShowCityHealthinessAlert()

	def _getAlertMessageIcon(self, cityId, passes):
		if passes:
			return (TRNSLTR.getText("TXT_KEY_CIV4LERTS_ON_CITY_UNHEALTHY", (STATE.getCityName(cityId[0], cityId[1]), )), UNHEALTHY_ICON)
		return (TRNSLTR.getText("TXT_KEY_CIV4LERTS_ON_CITY_HEALTHY", (STATE.getCityName(cityId[0], cityId[1]), )), HEALTHY_ICON)

	def _isShowPendingAlert(self, passes):
		return Civ4lertsOpt.isShowCityPendingHealthinessAlert()

	def _getPendingAlertMessageIcon(self, cityId, passes):
		if passes:
			return (TRNSLTR.getText("TXT_KEY_CIV4LERTS_ON_CITY_PENDING_UNHEALTHY", (STATE.getCityName(cityId[0], cityId[1]), )), UNHEALTHY_ICON)
		return (TRNSLTR.getText("TXT_KEY_CIV4LERTS_ON_CITY_PENDING_HEALTHY", (STATE.getCityName(cityId[0], cityId[1]), )), HEALTHY_ICON)

# Occupation

class CityOccupation(AbstractCityTestAlert):
	"""
	Displays an alert when a city switches to/from occupation.

	Test: True if the city is under occupation.
	"""
	def __init__(self, eventManager):
		AbstractCityTestAlert.__init__(self, eventManager)

	def _passesTest(self, cityId):
		return STATE.isOccupation(cityId[0], cityId[1])

	def _willPassTest(self, cityId):
		if not STATE.isOccupation(cityId[0], cityId[1]):
			return False
		return STATE.getCountdowns(cityId[0], cityId[1])[CityCountdownKind.COUNTDOWN_OCCUPATION] > 1

	def _isShowAlert(self, passes):
		return Civ4lertsOpt.isShowCityOccupationAlert()

	def _getAlertMessageIcon(self, cityId, passes):
		if passes:
			print "%s passed occupation test, ignoring" % STATE.getCityName(cityId[0], cityId[1])
			return (None, None)
		return (TRNSLTR.getText("TXT_KEY_CIV4LERTS_ON_CITY_PACIFIED", (STATE.getCityName(cityId[0], cityId[1]), )), HAPPY_ICON)

	def _isShowPendingAlert(self, passes):
		return Civ4lertsOpt.isShowCityPendingOccupationAlert()

	def _getPendingAlertMessageIcon(self, cityId, passes):
		if passes:
			print "[WARN] %s passed pending occupation test, ignoring" % STATE.getCityName(cityId[0], cityId[1])
			return (None, None)
		return (TRNSLTR.getText("TXT_KEY_CIV4LERTS_ON_CITY_PENDING_PACIFIED", (STATE.getCityName(cityId[0], cityId[1]), )), HAPPY_ICON)

# Hurrying Production

class AbstractCanHurry(AbstractCityTestAlert):
	"""
	Displays an alert when a city can hurry the current production item.

	Test: True if the city can hurry.
	"""
	def __init__(self, eventManager):
		AbstractCityTestAlert.__init__(self, eventManager)
		eventManager.addEventHandler("cityBuildingUnit", self.onCityBuildingUnit)
		eventManager.addEventHandler("cityBuildingBuilding", self.onCityBuildingBuilding)
		eventManager.addEventHandler("cityBuildingProject", self.onCityBuildingProject)
		eventManager.addEventHandler("cityBuildingProcess", self.onCityBuildingProcess)

	def init(self, szHurryType):
		AbstractCityAlert.init(self)
		self.keHurryType = ENUMS.getInfoType(szHurryType)

	def onCityBuildingUnit(self, argsList):
		#cityId, iUnit = argsList
		self._onItemStarted(argsList[0])

	def onCityBuildingBuilding(self, argsList):
		#cityId, iBuilding = argsList
		self._onItemStarted(argsList[0])

	def onCityBuildingProject(self, argsList):
		#cityId, iProject = argsList
		self._onItemStarted(argsList[0])

	def onCityBuildingProcess(self, argsList):
		#cityId, iProcess = argsList
		self._onItemStarted(argsList[0])

	def _onItemStarted(self, cityId):
		if cityId[0] == STATE.getActivePlayer():
			self.discardCity(cityId)

	def _passesTest(self, cityId):
		aQuote = STATE.getHurryQuote(cityId[0], cityId[1], self.keHurryType)
		return aQuote[CityHurryQuote.HURRY_QUOTE_ALLOWED] != 0

	def _getAlertMessageIcon(self, cityId, passes):
		if passes:
			# The order read names WHAT is being built in one fetch; the info plane is addressed by prefix + id.
			aOrder = STATE.getOrder(cityId[0], cityId[1])
			szPrefix = ORDER_PREFIX.get(aOrder[CityOrderRead.ORDER_READ_TYPE])
			iType = aOrder[CityOrderRead.ORDER_READ_ID]
			if szPrefix is not None and iType >= 0:
				return (self._getAlertMessage(cityId, szPrefix, iType), INFO.getButton(szPrefix, iType))
		return (None, None)

class CanHurryPopulation(AbstractCanHurry):
	"""
	Displays an alert when a city can hurry using population.
	"""
	def __init__(self, eventManager):
		AbstractCanHurry.__init__(self, eventManager)

	def init(self):
		AbstractCanHurry.init(self, "HURRY_POPULATION")

	def _isShowAlert(self, passes):
		return passes and Civ4lertsOpt.isShowCityCanHurryPopAlert()

	def _getAlertMessage(self, cityId, szPrefix, iType):
		aQuote = STATE.getHurryQuote(cityId[0], cityId[1], self.keHurryType)
		aOrder = STATE.getOrder(cityId[0], cityId[1])
		aCountdowns = STATE.getCountdowns(cityId[0], cityId[1])
		iPop = aQuote[CityHurryQuote.HURRY_QUOTE_POPULATION_COST]
		iOverflow = (aQuote[CityHurryQuote.HURRY_QUOTE_PRODUCTION_GAINED]
		             - aOrder[CityOrderRead.ORDER_READ_PRODUCTION_LEFT])
		if Civ4lertsOpt.isWhipAssistOverflowCountCurrentProduction():
			iOverflow = iOverflow + aOrder[CityOrderRead.ORDER_READ_PRODUCTION_PER_TURN]
		iAnger = (aCountdowns[CityCountdownKind.COUNTDOWN_HURRY_ANGER]
		          + aCountdowns[CityCountdownKind.COUNTDOWN_HURRY_ANGER_PERIOD])
		iMaxOverflow = aOrder[CityOrderRead.ORDER_READ_MAX_OVERFLOW]
		iOverflowGold = max(0, iOverflow - iMaxOverflow) * STATE.getDefineINT("MAXED_UNIT_GOLD_PERCENT") / 100
		# The kept overflow is expressed in POST-modifier hammers; dividing by the production modifier reports
		# it in the pre-modifier hammers the player actually sees in the build box.
		iProductionModifier = STATE.getYieldModifiers(cityId[0], cityId[1])[YieldTypes.YIELD_PRODUCTION]
		if iProductionModifier > 0:
			iOverflow = 100 * iMaxOverflow / iProductionModifier
		szName = STATE.getCityName(cityId[0], cityId[1])
		szItem = INFO.getDescription(szPrefix, iType)
		if iOverflowGold > 0:
			return TRNSLTR.getText("TXT_KEY_CIV4LERTS_ON_CITY_CAN_HURRY_POP_PLUS_GOLD", (szName, szItem, iPop, iOverflow, iAnger, iOverflowGold))

		return TRNSLTR.getText("TXT_KEY_CIV4LERTS_ON_CITY_CAN_HURRY_POP", (szName, szItem, iPop, iOverflow, iAnger))

class CanHurryGold(AbstractCanHurry):
	"""
	Displays an alert when a city can hurry using gold.
	"""
	def __init__(self, eventManager):
		AbstractCanHurry.__init__(self, eventManager)

	def init(self):
		AbstractCanHurry.init(self, "HURRY_GOLD")

	def _isShowAlert(self, passes):
		return passes and Civ4lertsOpt.isShowCityCanHurryGoldAlert()

	def _getAlertMessage(self, cityId, szPrefix, iType):
		iGold = STATE.getHurryQuote(cityId[0], cityId[1], self.keHurryType)[CityHurryQuote.HURRY_QUOTE_GOLD_COST]
		szName = STATE.getCityName(cityId[0], cityId[1])
		return TRNSLTR.getText("TXT_KEY_CIV4LERTS_ON_CITY_CAN_HURRY_GOLD", (szName, INFO.getDescription(szPrefix, iType), iGold))


## Trading Gold

class GoldTrade(AbstractStatefulAlert):
	"""
	Displays an alert when a civilization has a significant increase
	in gold available for trade since the last alert.
	"""
	def __init__(self, eventManager):
		AbstractStatefulAlert.__init__(self, eventManager)
		eventManager.addEventHandler("BeginActivePlayerTurn", self.onBeginActivePlayerTurn)

	def onBeginActivePlayerTurn(self, argsList):
		if (not Civ4lertsOpt.isShowGoldTradeAlert()):
			return
		playerID = GAME.getActivePlayer()
		for rival in TradeUtil.getGoldTradePartners(playerID):
			rivalID = rival.getID()
			oldMaxGoldTrade = self._getMaxGoldTrade(playerID, rivalID)
			newMaxGoldTrade = rival.AI_maxGoldTrade(playerID)
			deltaMaxGoldTrade = newMaxGoldTrade - oldMaxGoldTrade
			if deltaMaxGoldTrade >= Civ4lertsOpt.getGoldTradeThreshold():
				message = TRNSLTR.getText("TXT_KEY_CIV4LERTS_ON_GOLD_TRADE", (rival.getName(), newMaxGoldTrade))
				addMessage(playerID, message)
				self._setMaxGoldTrade(playerID, rivalID, newMaxGoldTrade)
			elif newMaxGoldTrade < oldMaxGoldTrade:
				self._setMaxGoldTrade(playerID, rivalID, newMaxGoldTrade)

	def _reset(self):
		self.maxGoldTrade = {}
		for player in range(GC.getMAX_PLAYERS()):
			self.maxGoldTrade[player] = {}
			for rival in range(GC.getMAX_PLAYERS()):
				self._setMaxGoldTrade(player, rival, 0)

	def _getMaxGoldTrade(self, player, rival):
		return self.maxGoldTrade[player][rival]

	def _setMaxGoldTrade(self, player, rival, value):
		self.maxGoldTrade[player][rival] = value

class GoldPerTurnTrade(AbstractStatefulAlert):
	"""
	Displays an alert when a civilization has a significant increase
	in gold per turn available for trade since the last alert.
	"""
	def __init__(self, eventManager):
		AbstractStatefulAlert.__init__(self, eventManager)
		eventManager.addEventHandler("BeginActivePlayerTurn", self.onBeginActivePlayerTurn)

	def onBeginActivePlayerTurn(self, argsList):
		if (not Civ4lertsOpt.isShowGoldPerTurnTradeAlert()):
			return
		playerID = GAME.getActivePlayer()
		for rival in TradeUtil.getGoldTradePartners(playerID):
			rivalID = rival.getID()
			oldMaxGoldPerTurnTrade = self._getMaxGoldPerTurnTrade(playerID, rivalID)
			newMaxGoldPerTurnTrade = rival.AI_maxGoldPerTurnTrade(playerID)
			deltaMaxGoldPerTurnTrade = newMaxGoldPerTurnTrade - oldMaxGoldPerTurnTrade
			if (deltaMaxGoldPerTurnTrade >= Civ4lertsOpt.getGoldPerTurnTradeThreshold()):
				message = TRNSLTR.getText("TXT_KEY_CIV4LERTS_ON_GOLD_PER_TURN_TRADE", (rival.getName(), newMaxGoldPerTurnTrade))
				addMessage(playerID, message)
				self._setMaxGoldPerTurnTrade(playerID, rivalID, newMaxGoldPerTurnTrade)
			else:
				maxGoldPerTurnTrade = min(oldMaxGoldPerTurnTrade, newMaxGoldPerTurnTrade)
				self._setMaxGoldPerTurnTrade(playerID, rivalID, maxGoldPerTurnTrade)

	def _reset(self):
		self.maxGoldPerTurnTrade = {}
		for player in range(GC.getMAX_PC_PLAYERS()):
			self.maxGoldPerTurnTrade[player] = {}
			for rival in range(GC.getMAX_PC_PLAYERS()):
				self._setMaxGoldPerTurnTrade(player, rival, 0)

	def _getMaxGoldPerTurnTrade(self, player, rival):
		return self.maxGoldPerTurnTrade[player][rival]

	def _setMaxGoldPerTurnTrade(self, player, rival, value):
		self.maxGoldPerTurnTrade[player][rival] = value


## Diplomacy

class RefusesToTalk(AbstractStatefulAlert):
	"""
	Displays an alert when a civilization cuts off or reestablishes communication.
	"""
	def __init__(self, eventManager):
		AbstractStatefulAlert.__init__(self, eventManager)
		#RevolutionDCM start - start as minors fix
		self._reset()
		#RevolutionDCM end - start as minors fix
		eventManager.addEventHandler("BeginActivePlayerTurn", self.onBeginActivePlayerTurn)
		eventManager.addEventHandler("changeWar", self.onChangeWar)
		eventManager.addEventHandler("cityRazed", self.onCityRazed)
		eventManager.addEventHandler("DealCanceled", self.onDealCanceled)
		eventManager.addEventHandler("EmbargoAccepted", self.onEmbargoAccepted)

	def onBeginActivePlayerTurn(self, argsList):
		self.check()

	def onChangeWar(self, argsList):
		bIsWar, eTeam, eRivalTeam = argsList
		self.checkIfIsAnyOrHasMetAllTeams(eTeam, eRivalTeam)

	def onCityRazed(self, argsList):
		cityId, iPlayer = argsList
		self.checkIfIsAnyOrHasMetAllTeams(STATE.getPlayerTeam(cityId[0]), STATE.getPlayerTeam(iPlayer))

	def onDealCanceled(self, argsList):
		eOfferPlayer, eTargetPlayer, pTrade = argsList
		if eOfferPlayer != -1 and eTargetPlayer != -1:
			self.checkIfIsAnyOrHasMetAllTeams(GC.getPlayer(eOfferPlayer).getTeam(), GC.getPlayer(eTargetPlayer).getTeam())

	def onEmbargoAccepted(self, argsList):
		eOfferPlayer, eTargetPlayer, pTrade = argsList
		self.checkIfIsAnyOrHasMetAllTeams(GC.getPlayer(eOfferPlayer).getTeam(), GC.getPlayer(eTargetPlayer).getTeam())

	def checkIfIsAnyOrHasMetAllTeams(self, *eTeams):
		"""
		Calls check() only if the active team is any or has met all of the given teams.
		"""
		iActiveTeam = GC.getGame().getActiveTeam()
		activeTeam = GC.getTeam(iActiveTeam)
		for eTeam in eTeams:
			if iActiveTeam != eTeam and eTeam >= 0 and not activeTeam.isHasMet(eTeam):
				return
		self.check()

	def check(self):
		if not Civ4lertsOpt.isShowRefusesToTalkAlert():
			return
		iPlayer = GAME.getActivePlayer()
		CyPlayer = GC.getActivePlayer()
		iTeam = CyPlayer.getTeam()
		CyTeam = GC.getTeam(iTeam)
		refusals = self.refusals[iPlayer]
		aSet = set()
		for iPlayerX in range(GC.getMAX_PC_PLAYERS()):
			if iPlayerX == iPlayer: continue
			CyPlayerX = GC.getPlayer(iPlayerX)
			if not CyPlayerX.isAlive() or CyPlayerX.isHuman() or CyPlayerX.isMinorCiv(): continue

			iTeamX = CyPlayerX.getTeam()
			if iTeamX == iTeam or not CyTeam.isHasMet(iTeamX) or CyTeam.isAtWarWith(iTeamX): continue

			if not CyPlayerX.AI_isWillingToTalk(iPlayer):
				aSet.add(CyPlayerX.getID())

		self.display(iPlayer, "TXT_KEY_CIV4LERTS_ON_WILLING_TO_TALK", refusals.difference(aSet))
		self.display(iPlayer, "TXT_KEY_CIV4LERTS_ON_REFUSES_TO_TALK", aSet.difference(refusals))
		self.refusals[iPlayer] = aSet

	def display(self, eActivePlayer, key, players):
		if GAME.getElapsedGameTurns() > 0:
			for ePlayer in players:
				player = GC.getPlayer(ePlayer)
				if player.isAlive():
					message = BugUtil.getText(key, player.getName())
					addMessage(eActivePlayer, message)

	def _reset(self):
		self.refusals = {}
		for i in range(0,GC.getMAX_PC_PLAYERS()):
			self.refusals[i] = set()

class WorstEnemy(AbstractStatefulAlert):
	"""
	Displays an alert when a civilization's worst enemy changes.
	"""
	def __init__(self, eventManager):
		AbstractStatefulAlert.__init__(self, eventManager)
		eventManager.addEventHandler("BeginActivePlayerTurn", self.onBeginActivePlayerTurn)

	def onBeginActivePlayerTurn(self, argsList):
		self.check()

	def onFirstContact(self, argsList):
		eTeam, eRivalTeam = argsList
		self.checkIfIsAnyOrHasMetAllTeams(eTeam, eRivalTeam)

	def onChangeWar(self, argsList):
		#bIsWar = argsList[0]
		eTeam = argsList[1]
		eRivalTeam = argsList[2]
		self.checkIfIsAnyOrHasMetAllTeams(eTeam, eRivalTeam)

	def onCityRazed(self, argsList):
		cityId, ePlayer = argsList
		self.checkIfIsAnyOrHasMetAllTeams(STATE.getPlayerTeam(cityId[0]), STATE.getPlayerTeam(ePlayer))

	def onVassalState(self, argsList):
		eMaster = argsList[0]
		eVassal = argsList[1]
		#bVassal = argsList[2]
		self.checkIfIsAnyOrHasMetAllTeams(eMaster, eVassal)

	def checkIfIsAnyOrHasMetAllTeams(self, *eTeams):
		"""
		Calls check() only if the active team is any or has met all of the given teams.
		"""
		iActiveTeam = GC.getGame().getActiveTeam()
		activeTeam = GC.getTeam(iActiveTeam)
		for eTeam in eTeams:
			if eTeam != -1 and iActiveTeam != eTeam and not activeTeam.isHasMet(eTeam):
				return
		self.check()

	def check(self):
		if (not Civ4lertsOpt.isShowWorstEnemyAlert()):
			return
		eActivePlayer = GAME.getActivePlayer()
		iActiveTeam = GC.getGame().getActiveTeam()
		activeTeam = GC.getTeam(iActiveTeam)
		enemies = self.enemies[eActivePlayer]
		newEnemies = AttitudeUtil.getWorstEnemyTeams()
		delayedMessages = {}
		for eTeam, eNewEnemy in newEnemies.iteritems():
			#RevolutionDCM fix
			if eTeam != -1 and activeTeam.isHasMet(eTeam):
				eOldEnemy = enemies[eTeam]
				if eOldEnemy != -1 and not GC.getTeam(eOldEnemy).isAlive():
					eOldEnemy = -1
					enemies[eTeam] = -1
				#RevolutionDCM fix
				if eNewEnemy != -1 and iActiveTeam != eNewEnemy and not activeTeam.isHasMet(eNewEnemy):
					eNewEnemy = -1
				if eOldEnemy != eNewEnemy:
					enemies[eTeam] = eNewEnemy
					if eNewEnemy == -1:
						if eOldEnemy == iActiveTeam:
							message = BugUtil.getText("TXT_KEY_CIV4LERTS_ON_YOU_NO_WORST_ENEMY", GC.getTeam(eTeam).getName())
						else:
							message = BugUtil.getText("TXT_KEY_CIV4LERTS_ON_NO_WORST_ENEMY",
									(GC.getTeam(eTeam).getName(), GC.getTeam(eOldEnemy).getName()))
					elif eOldEnemy == -1:
						message = None # handled below
						if eNewEnemy not in delayedMessages:
							delayedMessages[eNewEnemy] = GC.getTeam(eTeam).getName()
						else:
							delayedMessages[eNewEnemy] += u", " + GC.getTeam(eTeam).getName()
					else:
						if eOldEnemy == iActiveTeam:
							message = BugUtil.getText("TXT_KEY_CIV4LERTS_ON_SWITCH_WORST_ENEMY_FROM_YOU",
									(GC.getTeam(eTeam).getName(), GC.getTeam(eNewEnemy).getName()))
						elif eNewEnemy == iActiveTeam:
							message = BugUtil.getText("TXT_KEY_CIV4LERTS_ON_SWITCH_WORST_ENEMY_TO_YOU",
									(GC.getTeam(eTeam).getName(), GC.getTeam(eOldEnemy).getName()))
						else:
							message = BugUtil.getText("TXT_KEY_CIV4LERTS_ON_SWITCH_WORST_ENEMY",
									(GC.getTeam(eTeam).getName(), GC.getTeam(eNewEnemy).getName(), GC.getTeam(eOldEnemy).getName()))
					if message:
						addMessage(eActivePlayer, message)
		for eEnemy, haters in delayedMessages.iteritems():
			if iActiveTeam == eEnemy:
				message = BugUtil.getText("TXT_KEY_CIV4LERTS_ON_YOU_WORST_ENEMY", haters)
			else:
				message = BugUtil.getText("TXT_KEY_CIV4LERTS_ON_WORST_ENEMY", (haters, GC.getTeam(eEnemy).getName()))
			addMessage(eActivePlayer, message)

	def _reset(self):
		"""
		The enemies dictionary maps all teams to their worst enemy.
		It will hold -1 for any team or enemy the active team hasn't met.
		"""
		self.enemies = {}
		for i in range(0,GC.getMAX_PC_PLAYERS()):
			self.enemies[i] = [-1] * GC.getMAX_TEAMS()
