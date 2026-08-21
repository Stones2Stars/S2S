## GPUtil
##
## Utilities for dealing with Great People.
##
## MODDERS
##
##   There are four places where you must add information about your new great people.
##   This is also necessary if you assign GP points to buildings that don't normally get them,
##   for example GG points to Heroic Epic.
##
##     1. Unit Type
##     2. Named constant
##     3. Color
##     4. Icon (font glyph or string)
##
## Notes
##   - Must be initialized externally by calling init()
##
## Copyright (c) 2007-2009 The BUG Mod.
##
## Author: EmperorFool

from CvPythonExtensions import *

# The one data-fetching library ([DEC-cy-not-fixed]): STATE = live state, ENABLER = availability,
# ENUMS = the engine enum vocabulary + name->id resolution.
GC = CyGlobalContext()
INFO = CyInfo()
STATE = CyState()
GAME = CyGame()
ENABLER = CyEnabler()
ENUMS = CyEnums()

# Map unit to font symbol
g_gpUnitTypes = [
	["UNIT_GREAT_HUNTER",		unichr(8483)],
	["UNIT_GREAT_ENGINEER",		unichr(8484)],
	["UNIT_MERCHANT",			unichr(8500)],
	["UNIT_SCIENTIST",			unichr(8501)],
	["UNIT_ARTIST",				unichr(8502)],
	["UNIT_GREAT_SPY",			unichr(8503)],
	["UNIT_GREAT_GENERAL",		unichr(8528)],
	["UNIT_GREAT_ADMIRAL",		unichr(8530)],
#	["UNIT_GREAT_AVIATOR",		unichr(8531)],
	["UNIT_GREAT_DETECTIVE",	unichr(8532)],
	["UNIT_DOCTOR",				unichr(8852)],
	["UNIT_PROPHET",			unichr(8857)],
	["UNIT_GREAT_STATESMAN",	unichr(8869)]
]

def init():
	# Translate KEY to unitType
	global g_gpUnitTypes
	aList = []
	for KEY, char in g_gpUnitTypes:
		aList.append([GC.getInfoTypeForString(KEY), char])
	g_gpUnitTypes = aList


def getUnitIcon(iUnit):
	for iUnitX, char in g_gpUnitTypes:
		if iUnit == iUnitX:
			return char
	print "[WARN] GPUtil.getUnitIcon\n\tNo GP icon for " + INFO.getType("UNIT_", iUnit)
	return unichr(8862) # Generic great person symbol

def getDisplayCity():
	# Returns (owner, cityId, turns) for the progress bar. The selection is asked of the library rather than of
	# the EXE's CyInterface, which hands back a handle carrying zero defs -- see CyState::getHeadSelectedCityId.
	aSelected = STATE.getHeadSelectedCityId()
	iPlayer = aSelected[0]
	if aSelected[1] >= 0 and GC.getPlayer(iPlayer).getTeam() == GAME.getActiveTeam():
		iCityId = aSelected[1]
		return (iPlayer, iCityId, getCityTurns(iPlayer, iCityId))

	iPlayer = GAME.getActivePlayer()
	iCityId, iTurns = findNextCity()
	if iCityId < 0:
		iCityId, iGPP = findMaxCity()
		iTurns = None
	return (iPlayer, iCityId, iTurns)

def findNextCity():
	# Cities are walked by ID: the handle a CyCity would be carries zero defs, so it could answer neither its
	# rate nor its progress. getCityIds is ONE crossing for the whole set.
	iCityId = -1
	iTurns = 0
	iPlayer = GAME.getActivePlayer()
	iThreshold = STATE.getGreatPeopleThresholdNonMilitary(iPlayer)

	for iCityX in GC.getPlayer(iPlayer).getCityIds():
		iRate = GC.getPlayer(iPlayer).getCity(iCityX).getGreatPeopleRate()
		if iRate > 0:
			iProgress = GC.getPlayer(iPlayer).getCity(iCityX).getGreatPeopleProgress()
			iTurnsX = (iThreshold - iProgress + iRate - 1) / iRate
			if not iTurns or iTurnsX < iTurns:
				iTurns = iTurnsX
				iCityId = iCityX

	return [iCityId, iTurns]

def findMaxCity():
	iCityId = -1
	iGPP = 0
	iPlayer = GAME.getActivePlayer()

	for iCityX in GC.getPlayer(iPlayer).getCityIds():
		iGPPX = GC.getPlayer(iPlayer).getCity(iCityX).getGreatPeopleProgress()
		if iGPPX > iGPP:
			iGPP = iGPPX
			iCityId = iCityX

	return [iCityId, iGPP]

def getCityTurns(iPlayer, iCityId):
	if iCityId >= 0:
		iThreshold = STATE.getGreatPeopleThresholdNonMilitary(iPlayer)
		iRate = GC.getPlayer(iPlayer).getCity(iCityId).getGreatPeopleRate()
		if iRate > 0:
			iProgress = GC.getPlayer(iPlayer).getCity(iCityId).getGreatPeopleProgress()
			iTurns = (iThreshold - iProgress + iRate - 1) / iRate
			return iTurns
	return None

def calcPercentages(iPlayer, iCityId):
	# Calc total rate
	iTotal = 0
	for iUnit, _ in g_gpUnitTypes:
		iTotal += GC.getPlayer(iPlayer).getCity(iCityId).getGreatPeopleUnitProgress(iUnit)
	# Calc individual percentages based on rates and total
	percents = []
	if iTotal > 0:
		iLeftover = 100
		for iUnit in range(GC.getNumUnitInfos()):
			iProgress = GC.getPlayer(iPlayer).getCity(iCityId).getGreatPeopleUnitProgress(iUnit)
			if iProgress > 0:
				iPercent = 100 * iProgress / iTotal
				iLeftover -= iPercent
				percents.append((iPercent, iUnit))
		# Add remaining from 100 to first in list to match Civ4
		if iLeftover > 0:
			percents[0] = (percents[0][0] + iLeftover, percents[0][1])
	return percents


# Displaying Progress
def getGreatPeopleText(iPlayer, iCityId, iGPTurns, iGPBarWidth, bGPBarTypesNone, bGPBarTypesOne, bIncludeCityName, uFont):

	if iCityId < 0:
		szText = CyTranslator().getText("INTERFACE_GREAT_PERSON_NONE", (unichr(8862),))

	elif bGPBarTypesNone:

		if iGPTurns:

			if bIncludeCityName:
				szText = CyTranslator().getText("INTERFACE_GREAT_PERSON_CITY_TURNS", (unichr(8862), GC.getPlayer(iPlayer).getCity(iCityId).getName(), iGPTurns))
			else:
				szText = CyTranslator().getText("INTERFACE_GREAT_PERSON_TURNS", (unichr(8862), iGPTurns))
		else:
			if bIncludeCityName:
				szText = CyTranslator().getText("INTERFACE_GREAT_PERSON_CITY", (unichr(8862), GC.getPlayer(iPlayer).getCity(iCityId).getName()))
			else:
				szText = unichr(8862)
	else:
		lPercents = calcPercentages(iPlayer, iCityId)
		iLength = len(lPercents)
		if not iLength:

			if iGPTurns:

				if bIncludeCityName:
					szText = CyTranslator().getText("INTERFACE_GREAT_PERSON_CITY_TURNS", (unichr(8862), GC.getPlayer(iPlayer).getCity(iCityId).getName(), iGPTurns))
				else:
					szText = CyTranslator().getText("INTERFACE_GREAT_PERSON_TURNS", (unichr(8862), iGPTurns))
			else:
				if bIncludeCityName:
					szText = CyTranslator().getText("INTERFACE_GREAT_PERSON_CITY", (unichr(8862), GC.getPlayer(iPlayer).getCity(iCityId).getName()))
				else:
					szText = unichr(8862)
		else:
			lPercents.sort()
			lPercents.reverse()
			if bGPBarTypesOne or iLength == 1:

				iPercent, iUnit = lPercents[0]
				name = INFO.getDescription("UNIT_", iUnit)
				if iGPTurns:

					if bIncludeCityName:
						szText = CyTranslator().getText("INTERFACE_GREAT_PERSON_CITY_TURNS", (name, GC.getPlayer(iPlayer).getCity(iCityId).getName(), iGPTurns))
					else:
						szText = CyTranslator().getText("INTERFACE_GREAT_PERSON_TURNS", (name, iGPTurns))
				else:
					if bIncludeCityName:
						szText = CyTranslator().getText("INTERFACE_GREAT_PERSON_CITY", (name, GC.getPlayer(iPlayer).getCity(iCityId).getName()))
					else:
						szText = unicode(name)
			else:
				if iGPTurns:
					szText = CyTranslator().getText("INTERFACE_GREAT_PERSON_TURNS", (unichr(8862), iGPTurns))
				else:
					szText = unichr(8862) + ":"

				szTypes = " -"
				for iPercent, iUnit in lPercents:
					szNewTypes = szTypes + " %s%d%%" % (getUnitIcon(iUnit), iPercent)
					szNewText = szText + " -" + szTypes
					if CyInterface().determineWidth(uFont + szNewText) > iGPBarWidth:
						break
					szTypes = szNewTypes
				if iLength > 0:
					szText += szTypes
	return uFont + szText
