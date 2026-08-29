#------------------------------------------------------------------------
# Can't abandon last city without the "Require Complete Kill" gameoption.
#------------------------------------------------------------------------
from CvPythonExtensions import *
from operator import itemgetter
import CvScreensInterface
GC = CyGlobalContext()
GAME = GC.getGame()
TRNSLTR = CyTranslator()
# The one data-fetching library: INFO = what an entity CARRIES, ENABLER = can I?, ENUMS = the engine
# enum vocabulary + name->id resolution. A game object's own data is asked OF THAT OBJECT --
# GC.getPlayer(i).getCity(id).getYields(), never a flat class keyed by (owner, id).
INFO = CyInfo()
BUILDING = CyBuildingInfo()   # the per-info BUILDING accessor
ENABLER = CyEnabler()

TEXT = CyGameTextMgr()
# globals
CD = None

# Entry point
def onHotKeyStart(argsList):
	import CvScreenEnums
	screen = CyGInterfaceScreen("MainInterface", CvScreenEnums.MAIN_INTERFACE)
	xRes = screen.getXResolution()
	yRes = screen.getYResolution()
	startCityDemolish(screen, xRes, yRes)

def startCityDemolish(screen, xRes, yRes):
	if CyInterface().isCityScreenUp():
		GC = CyGlobalContext()
		GAME = GC.getGame()
		# Get the player details and game options.
		CyCity = CyInterface().getHeadSelectedCity()
		iPlayer = CyCity.getOwner()
		if iPlayer == GAME.getActivePlayer():
			global CD
			CD = CityDemolish()
			CD.iPlayer = iPlayer
			CD.CyPlayer = CyPlayer = GC.getPlayer(iPlayer)
			CD.CyCity = CyCity
			if GAME.isOption(GameOptionTypes.GAMEOPTION_NO_CITY_RAZING):
				CD.bAbandonCity = False
			elif not GAME.isOption(GameOptionTypes.GAMEOPTION_EXP_COMPLETE_KILLS) and CyPlayer.getNumCities() < 2:
				CD.bAbandonCity = False
			else:
				CD.bAbandonCity = True
			CD.iconUnhappy = u'%c' % GAME.getSymbolID(FontSymbols.UNHAPPY_CHAR)
			CD.createPopup(screen, xRes, yRes)

class CityDemolish:
	def __init__(self):
		GC = CyGlobalContext()
		self.iSelected = None
		self.bListHidden = False
		self.updateTooltip = CvScreensInterface.mainInterface.updateTooltip
		self.iconGold = u'%c' %TEXT.getSymbolChar("COMMERCE_", CommerceTypes.COMMERCE_GOLD)
		self.iAbandonTrigger = GC.getNumBuildingInfos()

	def createPopup(self, screen, xRes, yRes):
		GC = CyGlobalContext()
		# Get player and city details. Set up headings etc.
		# Display appropriate dialogue.
		if xRes > 1700:
			aFontList = aFontList = ["<font=4b>", "<font=3b>", "<font=2b>"]
			uFont = "<font=3b>"
			iconSize = 28
		elif xRes > 1400:
			aFontList = aFontList = ["<font=4>", "<font=3>", "<font=2>"]
			uFont = "<font=2b>"
			iconSize = 24
		else:
			aFontList = aFontList = ["<font=3b>", "<font=2b>", "<font=1b>"]
			uFont = "<font=1b>"
			iconSize = 20
		TRNSLTR = CyTranslator()
		szText  = aFontList[0] + TRNSLTR.getText("TXT_KEY_ABANDON_CITY_HEADER1", ()) + '</font>\n\n'
		szText += aFontList[1] + TRNSLTR.getText("TXT_KEY_ABANDON_CITY_HEADER2", ()) + '</font>\n\n'
		szText += aFontList[2] + TRNSLTR.getText("TXT_KEY_ABANDON_CITY_HEADER3", ())
		iWidGen		= WidgetTypes.WIDGET_GENERAL
		iPanelMain	= PanelStyles.PANEL_STYLE_MAIN
		PRE = "CityDemolish|"
		name = PRE + "Bkgr"
		screen.addPanel(name, "", "", False, False, -8, -8, xRes + 16, yRes + 16, PanelStyles.PANEL_STYLE_MAIN_BLACK50)
		screen.setImageButton(PRE + "Exit0", "", 0, 0, xRes, yRes, iWidGen, 0, 0)
		dx = xRes / 3
		dy = 300
		xStart = xRes - dx * 2
		self.xListTooltip = xStart - 30
		y = (yRes - dy)/2
		screen.addPanel(PRE + "Main", "", "", False, False, xStart, y, dx, dy, iPanelMain)
		x = xStart + 8
		y += 8
		screen.addMultilineText(PRE + "Text", szText, x, y, dx - 12, 194, iWidGen, 0, 0, 1<<0)
		xMid = xRes / 2
		iBtnStd = ButtonStyles.BUTTON_STYLE_STANDARD
		y += 202
		dx -= 16
		self.xywBtn0 = [x, y, dx]
		screen.setButtonGFC(PRE + "Btn0", "****", "", x, y, dx, 32, iWidGen, 0, 0, iBtnStd)
		y += 40
		szText = TRNSLTR.getText("TXT_KEY_MAIN_MENU_OK", ())
		name = PRE + "Btn1"
		screen.setButtonGFC(name, szText, "", x, y, dx, 32, iWidGen, 0, 0, iBtnStd)
		screen.hide(name)
		szText = TRNSLTR.getText("TXT_KEY_POPUP_CANCEL", ())
		screen.setButtonGFC(PRE + "Exit1", szText, "", x, y, dx, 32, iWidGen, 0, 0, iBtnStd)
		screen.addPanel(PRE + "ListBkgr", "", "", False, False, 40, 24, xStart - 60, yRes - 48, iPanelMain)
		name = PRE + "List"
		screen.addScrollPanel(name, "", 40, 32, xStart - 60, yRes - 100, iPanelMain)
		screen.setStyle(name, "ScrollPanel_Alt_Style")
		# Set the cost modifier scaled by era and gamespeed.
		iconGold = self.iconGold
		fGoldMod = 0.09
		fGoldMod *= GC.getDefineINT("BUILDING_PRODUCTION_PERCENT") / 100.0
		fFactorGS = GAME.getHammerCostPercent() / 100.0
		fGoldMod *= fFactorGS
		self.fGoldMod = fGoldMod
		# Build List
		CyCity = self.CyCity
		aList = []
		iSum = 0
		for iType in xrange(GC.getNumBuildingInfos()):
			if CyCity.hasBuilding(iType) and not CyCity.isFreeBuilding(iType):
				# A wonder's CATEGORY is WHICH self-cap scope it authors ([json.md] 4.4): WORLD and TEAM are the
				# world and team wonders. There is no isWorldWonder mirror to ask, and never was one.
				eScope = BUILDING.getWonderScope(iType)
				if eScope in (AllowedCap.ALLOWEDCAP_WORLD, AllowedCap.ALLOWEDCAP_TEAM):
					continue
				# Unique buildings are protected. Each is asked of the BUILDING because the question is what
				# this candidate IS, not what the city holds -- classify by the QUESTION, not the domain
				# ([patterns.md]: gate -> the city; valuation / apply / display -> the grantor).
				if (INFO.providesNukeImmunity(iType) or INFO.isAutoBuild(iType)
				or INFO.providesCapitalStatus(iType) or INFO.isShrine(iType)):
					continue
				iGold = INFO.getIntrinsic("BUILDING_", iType, IntrinsicSlot.PYINT_COST)
				if iGold < 0:
					iGold = 0
				elif iGold != 0:
					iGold = int(iGold * fGoldMod)
				aList.append((INFO.getDescription("BUILDING_", iType), INFO.getButton("BUILDING_", iType), iType, iGold))
				iSum += iGold
		self.iSum = iSum
		# Abandon City cost.
		iFontGame = FontTypes.GAME_FONT
		x = y = 8
		dy = iconSize + 2
		screen.setTextAt(name + "Top0", name, "****", 1<<0, x, y, 0, iFontGame, iWidGen, 0, 0)
		if self.bAbandonCity:
			y += dy
			self.iAbandonGold = iGold = int(iSum - CyCity.getPopulation() * 13.37 * fFactorGS * (self.CyPlayer.getCurrentEra() + 1)**2)
			szAbandon = uFont + TRNSLTR.getText("TXT_KEY_ABANDON_CITY", ())
			if iGold:
				if iGold < 0:
					szAbandon += " (<color=197,0,0>" + str(iGold) + "</color> " + iconGold + ")"
				else:
					szAbandon += " (" + str(iGold) + " " + iconGold + ")"
			self.szAbandon = szAbandon
			screen.setTextAt(name + "Top1", name, szAbandon, 1<<0, x, y, 0, iFontGame, iWidGen, 0, 0)
		# Populate list box with valid buildings
		if aList:
			aList.sort(key=itemgetter(0))
			iconUnhappy = self.iconUnhappy
			aNameList = []
			for i, entry in enumerate(aList):
				szText, szBtn, iType, iGold = entry
				szBtn = '<img=%s size=%d></img>' %(szBtn, iconSize)
				# Build up text to display in the list box
				if iGold:
					if iGold < 0:
						szText += " (<color=197,0,0>" + str(iGold) + "</color> " + iconGold
					else:
						szText += " (" + str(iGold) + " " + iconGold
				# A religious building costs a happy face to sell -- the RELIGION_* association, not the shrine
				# relationship (213 buildings carry the first, 29 the second).
				if INFO.isReligiousBuilding(iType):
					if iGold:
						szText += ","
					else:
						szText += " ("
					szText += " 1" + iconUnhappy + ")"
				elif iGold:
					szText += ")"
				aNameList.append(szText)
				y += dy
				screen.setTextAt(name + "Btn" + str(iType), name, szBtn, 1<<0, x, y, 0, iFontGame, iWidGen, i, 0)
				screen.setTextAt(name + str(iType), name, uFont + szText, 1<<0, x + iconSize + 2, y+2, 0, iFontGame, iWidGen, i, 0)
			self.aList = aNameList


	def doIt(self, iSelected):
		GC = CyGlobalContext()
		iPlayer = self.iPlayer
		CyPlayer = self.CyPlayer
		CyCity = self.CyCity
		iCity = CyCity.getID()
		if iSelected == -1:
			X = CyCity.getX()
			Y = CyCity.getY()

			iCulturePercent = GC.getMap().plot(CyCity.getX(), CyCity.getY()).calculateCulturePercent(iPlayer)
			iPopulation = CyCity.getPopulation()
			iOwnCulturePop = iPopulation * iCulturePercent / 100
			iForeignPop = iPopulation - iOwnCulturePop

			# Judge
			if CyCity.isActiveBuilding(GC.getInfoTypeForString("BUILDING_COURTHOUSE")):
				CyMessageControl().sendModNetMessage(905, iPlayer, iCity, -1, GC.getInfoTypeForString("UNIT_JUDGE"))

			# Tribal Guardian
			iExp = -1
			UNIT = GC.getInfoTypeForString("UNIT_TRIBAL_GUARDIAN")
			if UNIT > -1:
				# Remove Tribal Guardian
				CyPlot = CyCity.plot()
				for i in xrange(CyPlot.getNumUnits() - 1, -1, -1):
					CyUnit = CyPlot.getUnit(i)
					if CyUnit.getRead()[UnitReadKind.UNIT_READ_TYPE] == UNIT:
						iExp = CyUnit.getRead()[UnitReadKind.UNIT_READ_EXPERIENCE] / 100
						CyMessageControl().sendModNetMessage(902, iPlayer, CyUnit.getID(), 0, 0)
						break

			# Settler
			if iOwnCulturePop > 0 or iForeignPop > 2:
				aSettlerList = [
					GC.getInfoTypeForString("UNIT_AIRSETTLER"),
					GC.getInfoTypeForString("UNIT_PIONEER"),
					GC.getInfoTypeForString("UNIT_COLONIST"),
					GC.getInfoTypeForString("UNIT_SETTLER"),
					GC.getInfoTypeForString("UNIT_TRIBE"),
					GC.getInfoTypeForString("UNIT_BAND")
				]
				for iUnit in aSettlerList:
					if iUnit < 0: continue
					# "Can this city train it?" is the ENABLER's, and its verdict is a bare O(1) fetch of the
					# maintained tri-state ([enabler.md] par.8). What stood here re-derived the tech, building
					# and bonus prereqs by hand -- the scattered canTrain check the enabler exists to replace,
					# and it read `isActiveBuilding`, a computed dormancy verdict a gate must not ride in on
					# ([DEC-calc-zero-ride-in]).
					if ENABLER.getUnitAvailability(iPlayer, iCity, iUnit) != EnablerState.ENABLER_LISTED:
						continue
					# Found Valid Settler
					CyMessageControl().sendModNetMessage(906, iPlayer, iCity, iExp, iUnit)
					if iOwnCulturePop:
						iOwnCulturePop -= 1
					else: iForeignPop -= 2
					break

			# Captives
			if iForeignPop > 0:
				if iPopulation > 1 or GC.getGame().getSorenRandNum(2, "50%"):
					iCaptives = (iForeignPop + 1) / 2
				else: iCaptives = iForeignPop / 2

				if iCaptives > 0:
					UNIT = GC.getInfoTypeForString("UNIT_CAPTIVE_CIVILIAN")
					for i in xrange(iCaptives):
						CyMessageControl().sendModNetMessage(905, iPlayer, iCity, -1, UNIT)
					# Attitude Penalty if the dominant city culture is of another player
					iCulturalOwner = CyCity.findHighestCulture()
					if iCulturalOwner not in (-1, iPlayer):
						CyMessageControl().sendModNetMessage(901, iPlayer, iCulturalOwner, 1, 1)

			# Immigrants
			if iOwnCulturePop > 0:
				UNIT = GC.getInfoTypeForString("UNIT_IMMIGRANT")
				for i in xrange(iOwnCulturePop):
					CyMessageControl().sendModNetMessage(905, iPlayer, iCity, -1, UNIT)

			# Merchants
			fModifierGS = GAME.getHammerCostPercent() / 100.0
			aMerchantList = [
				GC.getInfoTypeForString("UNIT_FREIGHT"),
				GC.getInfoTypeForString("UNIT_SUPPLY_TRAIN"),
				GC.getInfoTypeForString("UNIT_TRADE_CARAVAN"),
				GC.getInfoTypeForString("UNIT_EARLY_MERCHANT_C2C")
			]
			for iUnit in aMerchantList:
				if iUnit < 0: continue
				# The enabler's maintained verdict, never a re-derived prereq walk ([enabler.md] par.8).
				if ENABLER.getUnitAvailability(iPlayer, iCity, iUnit) != EnablerState.ENABLER_LISTED:
					continue
				# Found Valid Merchant
				fCost = INFO.getIntrinsic("UNIT_", iUnit, IntrinsicSlot.PYINT_COST) * fModifierGS
				if fCost < 1: break
				iNum = int(self.iSum / fCost)
				for i in xrange(iNum):
					CyMessageControl().sendModNetMessage(905, iPlayer, iCity, -1, iUnit)
				break

			aMerchantList = [
				GC.getInfoTypeForString("UNIT_FOOD_FREIGHT"),
				GC.getInfoTypeForString("UNIT_FOOD_SUPPLY_TRAIN"),
				GC.getInfoTypeForString("UNIT_FOOD_CARAVAN"),
				GC.getInfoTypeForString("UNIT_EARLY_FOOD_MERCHANT_C2C")
			]
			for iUnit in aMerchantList:
				if iUnit < 0: continue
				# The enabler's maintained verdict, never a re-derived prereq walk ([enabler.md] par.8).
				if ENABLER.getUnitAvailability(iPlayer, iCity, iUnit) != EnablerState.ENABLER_LISTED:
					continue
				# Found Valid Merchant
				fCost = INFO.getIntrinsic("UNIT_", iUnit, IntrinsicSlot.PYINT_COST) * fModifierGS
				iNum = int(CyCity.getFood() / fCost)
				for i in xrange(iNum):
					CyMessageControl().sendModNetMessage(905, iPlayer, iCity, -1, iUnit)
				break

			# Attitude Penalty for destroying holy city of someones state religion
			if CyCity.isHolyCity():
				for iReligion in xrange(GC.getNumReligionInfos()):
					if CyCity.isHolyCityByType(iReligion):
						for iOtherPlayer in xrange(GC.getMAX_PC_PLAYERS()):
							if iOtherPlayer == iPlayer: continue
							CyPlayer = GC.getPlayer(iOtherPlayer)
							if CyPlayer.isAlive() and CyPlayer.getStateReligion() == iReligion:
								CyMessageControl().sendModNetMessage(901, iPlayer, iOtherPlayer, 0, 1)

			# Abandon the City
			CyMessageControl().sendModNetMessage(904, iPlayer, iCity, 0, self.iAbandonGold)
			CyAudioGame().Play2DSound("AS2D_DISCOVERBONUS")

		else: # Sell a building
			iGold = int(INFO.getIntrinsic("BUILDING_", iSelected, IntrinsicSlot.PYINT_COST) * self.fGoldMod)
			CyMessageControl().sendModNetMessage(903, iPlayer, iCity, iSelected, iGold)
			CyAudioGame().Play2DSound("AS2D_DISCOVERBONUS")


	def handleInput(self, screen, szSplit, iNotifyCode, szFlag, ID, iData1):
		print "ACEM - handleInput"
		if iNotifyCode == NotifyCode.NOTIFY_CURSOR_MOVE_ON:
			if szSplit[0] in ("List", "ListBtn"):
				szText = CyGameTextMgr().getBuildingHelp(ID, True, self.CyCity, False, False, False)
				self.updateTooltip(screen, szText, self.xListTooltip)
		elif iNotifyCode == NotifyCode.NOTIFY_CLICKED:
			if szSplit[0] == "Exit":
				exitCityDemolish(screen)
				return
			if szSplit[0] == "Btn":
				if not ID:
					if szFlag == "MOUSE_RBUTTONUP" and iData1:
						x, y, w = self.xywBtn0
						screen.setButtonGFC("CityDemolish|Btn0", "****", "", x, y, w, 30, WidgetTypes.WIDGET_GENERAL, 0, 0, ButtonStyles.BUTTON_STYLE_STANDARD)
						self.iSelected = None
					elif self.bListHidden:
						screen.show("CityDemolish|List")
						screen.show("CityDemolish|ListBkgr")
						self.bListHidden = False
					else:
						screen.hide("CityDemolish|List")
						screen.hide("CityDemolish|ListBkgr")
						self.bListHidden = True
				elif ID == 1 and szFlag == "MOUSE_LBUTTONUP":
					iSelected = self.iSelected
					if iSelected != None:
						self.doIt(iSelected)
					exitCityDemolish(screen)
			elif szSplit[0] == "ListTop":
				x, y, w = self.xywBtn0
				if ID:
					screen.setButtonGFC("CityDemolish|Btn0", self.szAbandon, "", x, y, w, 30, WidgetTypes.WIDGET_GENERAL, 1, 0, ButtonStyles.BUTTON_STYLE_STANDARD)
					self.iSelected = -1
				else:
					screen.setButtonGFC("CityDemolish|Btn0", "****", "", x, y, w, 30, WidgetTypes.WIDGET_GENERAL, 0, 0, ButtonStyles.BUTTON_STYLE_STANDARD)
					self.iSelected = None
			elif szSplit[0] in ("List", "ListBtn"):
				if szFlag == "MOUSE_RBUTTONUP":
					CvScreensInterface.pediaJumpToBuilding([ID])
				else:
					x, y, w = self.xywBtn0
					screen.setButtonGFC("CityDemolish|Btn0", self.aList[iData1], "", x, y, w, 30, WidgetTypes.WIDGET_GENERAL, 1, 0, ButtonStyles.BUTTON_STYLE_STANDARD)
					self.iSelected = ID
			if self.iSelected == None:
				screen.hide("CityDemolish|Btn1")
				screen.show("CityDemolish|Exit1")
			else:
				screen.hide("CityDemolish|Exit1")
				screen.show("CityDemolish|Btn1")

def exitCityDemolish(screen):
	PRE = "CityDemolish|"
	screen.deleteWidget(PRE + "Bkgr")
	screen.deleteWidget(PRE + "Exit0")
	screen.deleteWidget(PRE + "Exit1")
	screen.deleteWidget(PRE + "Main")
	screen.deleteWidget(PRE + "Text")
	screen.deleteWidget(PRE + "Btn0")
	screen.deleteWidget(PRE + "Btn1")
	screen.deleteWidget(PRE + "List")
	screen.deleteWidget(PRE + "ListBkgr")
	global CD
	CD = None