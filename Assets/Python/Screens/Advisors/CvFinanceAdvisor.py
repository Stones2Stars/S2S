from CvPythonExtensions import *

# globals
# The one data-fetching library: INFO = what an entity CARRIES, ENABLER = can I?, ENUMS = the engine
# enum vocabulary + name->id resolution. A game object's own data is asked OF THAT OBJECT --
# GC.getPlayer(i).getCity(id).getYields(), never a flat class keyed by (owner, id).
GC = CyGlobalContext()
INFO = CyInfo()
BUILDING = CyBuildingInfo()   # the per-info BUILDING accessor
GAME = GC.getGame()
ENABLER = CyEnabler()
ENUMS = CyEnums()
TRNSLTR = CyTranslator()
TEXT = CyGameTextMgr()

class CvFinanceAdvisor:

	def __init__(self, screenId):
		self.screenId = screenId

	def getScreen(self):
		return CyGInterfaceScreen("FinanceAdvisor", self.screenId)

	def interfaceScreen(self):
		screen = self.getScreen()
		if screen.isActive():
			return

		import PythonToolTip
		self.tooltip = PythonToolTip.PythonToolTip()

		GAME = GC.getGame()
		# Get screen resolution.
		import ScreenResolution as SR
		self.xRes = xRes = SR.x
		self.yRes = yRes = SR.y
		# Calibrate variables.
		if yRes > 1000:
			H_EDGE_PANEL = 38
			uFontEdge = "<font=4b>"
		elif yRes > 800:
			H_EDGE_PANEL = 32
			uFontEdge = "<font=3b>"
		else:
			H_EDGE_PANEL = 29
			uFontEdge = "<font=2b>"

		self.aFontList = aFontList = [uFontEdge] + SR.aFontList

		self.Y_TOP_PAGE = Y_TOP_PAGE = H_EDGE_PANEL - 6
		self.H_PAGE = yRes - Y_TOP_PAGE - H_EDGE_PANEL + 8
		self.Y_BOT_TEXT = yRes - H_EDGE_PANEL + 10

		self.CyPlayer = CyPlayer = GC.getActivePlayer()
		self.iPlayer = iPlayer = GAME.getActivePlayer()
		self.nWidgetCount = 0

		self.aRowNames = []
		# The hover each part of the panels has; a part not listed has none of its own.
		self.TERM_WIDGETS = {
			CityBreakdownTerm.CITY_BREAKDOWN_TERM_TRADE_ROUTES		: WidgetTypes.WIDGET_HELP_FINANCE_DOMESTIC_TRADE,
			CityBreakdownTerm.CITY_BREAKDOWN_TERM_UNIT_SUPPLY		: WidgetTypes.WIDGET_HELP_FINANCE_AWAY_SUPPLY,
			CityBreakdownTerm.CITY_BREAKDOWN_TERM_CITY_MAINTENANCE	: WidgetTypes.WIDGET_HELP_FINANCE_CITY_MAINT,
			CityBreakdownTerm.CITY_BREAKDOWN_TERM_CIVIC_UPKEEP		: WidgetTypes.WIDGET_HELP_FINANCE_CIVIC_UPKEEP,
			CityBreakdownTerm.CITY_BREAKDOWN_TERM_FOREIGN_INCOME	: WidgetTypes.WIDGET_HELP_FINANCE_FOREIGN_INCOME,
			CityBreakdownTerm.CITY_BREAKDOWN_TERM_FOREIGN_PAYMENTS	: WidgetTypes.WIDGET_HELP_FINANCE_FOREIGN_INCOME,
			CityBreakdownTerm.CITY_BREAKDOWN_TERM_INFLATION		: WidgetTypes.WIDGET_HELP_FINANCE_INFLATED_COSTS,
		}
		self.szTreasury = TRNSLTR.getText("TXT_KEY_FINANCIAL_ADVISOR_TREASURY", (0,))[:-3]

		self.bStrike = bStrike = CyPlayer.isStrike()
		if bStrike:
			self.szStrike = TRNSLTR.getText("TXT_KEY_MISC_STRIKE", ())
		# Commerce icons
		aList = []
		for i in xrange(CommerceTypes.NUM_COMMERCE_TYPES):
			aList.append(u'%c' % TEXT.getSymbolChar("COMMERCE_", i))
		self.iconCommerceList = list(aList)
		# Enumerators
		eWidGen = WidgetTypes.WIDGET_GENERAL
		eFontTitle = FontTypes.TITLE_FONT
		ePanelHudHelp = PanelStyles.PANEL_STYLE_HUD_HELP
		# Build screen
		screen.setRenderInterfaceOnly(True)
		screen.showScreen(PopupStates.POPUPSTATE_IMMEDIATE, False)

		screen.addDDSGFC("FinAdv_BG", CyArtFileMgr().getInterfaceArtInfo("MAINMENU_SLIDESHOW_LOAD").getPath(), 0, 0, xRes, yRes, eWidGen, 0, 0)
		screen.addPanel("FinAdv_TopPanel", "", "", True, False, 0, 0, xRes, H_EDGE_PANEL, PanelStyles.PANEL_STYLE_TOPBAR)
		screen.addPanel("FinAdv_BotPanel", "", "", True, False, 0, yRes - H_EDGE_PANEL, xRes, H_EDGE_PANEL, PanelStyles.PANEL_STYLE_BOTTOMBAR)
		screen.setLabel("FinAdv_Header", "", uFontEdge + TRNSLTR.getText("TXT_KEY_FINANCIAL_ADVISOR_TITLE",()), 1<<2, xRes / 2, 2, 0, eFontTitle, eWidGen, 0, 0)
		screen.setText("FinAdv_Exit", "", uFontEdge + TRNSLTR.getText("TXT_WORD_EXIT",()), 1<<1, xRes - 16, 0, 0, eFontTitle, WidgetTypes.WIDGET_CLOSE_SCREEN, -1, -1)

		import DebugUtils
		if DebugUtils.isAnyDebugMode():
			DD = "FinAdv_DebugDD"
			screen.addDropDownBoxGFC(DD, 8, 0, 300, eWidGen, -1, -1, FontTypes.GAME_FONT)
			for iPlayerX in range(GC.getMAX_PLAYERS()):
				CyPlayerX = GC.getPlayer(iPlayerX)
				if CyPlayerX.isAlive():
					screen.addPullDownString(DD, CyPlayerX.getName(), iPlayerX, iPlayerX, iPlayer == iPlayerX)

		uFont3b = aFontList[3]
		a3rdX = xRes / 3
		dx = a3rdX / 16
		x0 = dx
		y0 = self.Y_TOP_PAGE + 16
		dx = a3rdX - 2 * dx
		dy = self.H_PAGE - 32
		screen.addPanel("FinAdv_Panel_1", "", "", True, True, x0, y0, dx, dy, ePanelHudHelp)
		szText = TRNSLTR.getText("TXT_KEY_CONCEPT_COMMERCE", ())
		screen.setLabelAt("", "FinAdv_Panel_1", uFont3b + szText, 1<<2, dx/2, 8, 0, eFontTitle, eWidGen, 1, 1)

		x0 += a3rdX
		screen.addPanel("FinAdv_Panel_2", "", "", True, True, x0, y0, dx, dy, ePanelHudHelp)
		szText = TRNSLTR.getText("TXT_KEY_FINANCIAL_ADVISOR_INCOME_HEADER", ())
		screen.setLabelAt("", "FinAdv_Panel_2", uFont3b + szText, 1<<2, dx/2, 8, 0, eFontTitle, eWidGen, 1, 1)

		x0 += a3rdX
		screen.addPanel("FinAdv_Panel_3", "", "", True, True, x0, y0, dx, dy, ePanelHudHelp)
		szText = TRNSLTR.getText("TXT_KEY_FINANCIAL_ADVISOR_EXPENSES_HEADER", ())
		screen.setLabelAt("", "FinAdv_Panel_3", uFont3b + szText, 1<<2, dx/2, 8, 0, eFontTitle, eWidGen, 1, 1)

		self.drawBase()
		self.updateContents()

	def drawBase(self):
		screen = self.getScreen()
		ePanelHudHelp = PanelStyles.PANEL_STYLE_HUD_HELP
		xRes = self.xRes
		a3rdX = xRes / 3
		dx = a3rdX / 16
		x0 = dx
		y0 = self.Y_TOP_PAGE + 16
		dx = a3rdX - 2 * dx
		dy = self.H_PAGE - 32
		self.xValues = dx - 24
		for i in (1, 2, 3):
			Pnl = "FinAdv_Scroll_%d" % i
			screen.addScrollPanel(Pnl, "", x0, y0 + 32, dx, dy - 64, ePanelHudHelp)
			screen.setStyle(Pnl, "ScrollPanel_Alt_Style")
			x0 += a3rdX
		# The empire's commerce as the engine sums its cities' commerce; the sliders follow it.
		self.aBaseRowNames = []
		self.yCommerceSlider = self.drawRows(screen, "FinAdv_Scroll_1", FinancePart.FINANCE_PART_COMMERCE, self.aBaseRowNames) + 32

	def drawRows(self, screen, Pnl, iPart, aNames):
		# One line per line of the engine's breakdown of the panel: the label left, the value right, subtotals
		# and the total in bold, and each line with the hover its part has. Returns the y below the last line.
		uFont2b = self.aFontList[6]
		uFont2 = self.aFontList[7]
		eGameFont = FontTypes.GAME_FONT
		y = 0
		for iRole, iTerm, szLabel, szValue in self.CyPlayer.getFinanceRows(iPart):
			uFont = uFont2
			if iRole in (CityBreakdownRole.CITY_BREAKDOWN_SUBTOTAL, CityBreakdownRole.CITY_BREAKDOWN_TOTAL):
				uFont = uFont2b
				y += 8
			eWidget = self.TERM_WIDGETS.get(iTerm, WidgetTypes.WIDGET_GENERAL)
			iData1 = self.iPlayer
			iData2 = 1
			szBase = "FinAdv_Row"
			if iTerm == CityBreakdownTerm.CITY_BREAKDOWN_TERM_UNIT_UPKEEP:
				szBase = "FinAdv_UnitUpkeep"
			for szText, iJustify, x in ((szLabel, 1<<0, 8), (szValue, 1<<1, self.xValues)):
				# Each panel numbers its rows in its own thousand, so no two panels share a widget name.
				szName = "%s%d" % (szBase, iPart * 1000 + len(aNames))
				aNames.append(szName)
				screen.setTextAt(szName, Pnl, uFont + szText, iJustify, x, y, 0, eGameFont, eWidget, iData1, iData2)
			y += 20
		return y

	def clearRows(self, screen, aNames):
		for szName in aNames:
			screen.deleteWidget(szName)
		del aNames[:]

	def updateContents(self):
		screen = self.getScreen()
		aName = self.getNextWidgetName
		self.deleteAllWidgets(screen, aName)
		self.clearRows(screen, self.aRowNames)
		# Enumerators
		eWidGen = WidgetTypes.WIDGET_GENERAL
		eGameFont = FontTypes.GAME_FONT
		eComGold = CommerceTypes.COMMERCE_GOLD
		# Variables
		xRes = self.xRes
		uFontEdge, uFont4b, uFont4, uFont3b, uFont3, uFont2b, uFont2, uFont1b, uFont1 = self.aFontList
		iconCommerceList = self.iconCommerceList
		CyPlayer = self.CyPlayer
		iIncome = GC.getPlayer(CyPlayer.getID()).getCommerces()[eComGold]

		# Treasury footer
		szTxt = self.szTreasury
		iGold = CyPlayer.getGold()
		iGoldRate = CyPlayer.calculateGoldRate()
		if iGold < 0:
			szTxt += "<color=255,0,0,255>"
		szTxt += "%d%s" %(iGold, iconCommerceList[eComGold])
		if iGoldRate:
			szTxt += " <color="
			if iGoldRate > 0 and iGold + iGoldRate >= 0:
				szTxt += "127,255,0,255>"
			elif iGoldRate < 0 and iGold + iGoldRate < 0:
				szTxt += "255,124,0,255>"
			else:
				szTxt += "255,227,0,255>"
			if iGoldRate > 0:
				szTxt += "+ "
			else:
				szTxt += "- "
				iGoldRate = -iGoldRate
			szTxt += str(iGoldRate)
		if self.bStrike:
			szTxt += self.szStrike

		screen.setLabel(aName(), "", uFontEdge + szTxt, 1<<2, xRes/2, self.Y_BOT_TEXT, 0, FontTypes.TITLE_FONT, eWidGen, 1, 1)

		# Content
		x = xRes / 3 - xRes / 24 - 24
		y = self.yCommerceSlider
		Btnsize = 20
		xBtnOffset = 0
		yBtnOffset = 0
		if (xRes > 2500) :
			Btnsize = 30
			xBtnOffset = 60
			yBtnOffset = -2
		Pnl = "FinAdv_Scroll_1"
		# Slider percentages
		iInc = GC.getDefineINT("COMMERCE_PERCENT_CHANGE_INCREMENTS")
		iRange = CommerceTypes.NUM_COMMERCE_TYPES
		for i in range(iRange):
			iType = (i + 1) % iRange # 1, 2, 3, 0 (Research, Culture, Espionage, Gold)

			if CyPlayer.isCommerceFlexible(iType):

				szText = uFont2b + iconCommerceList[iType]
				screen.setLabelAt(aName(), Pnl, szText, 1<<0, 8, y + 2, 0, eGameFont, eWidGen, 1, 1)

				Btn = aName()
				screen.setImageButtonAt(Btn, Pnl, "", 26, y+yBtnOffset, Btnsize, Btnsize, WidgetTypes.WIDGET_CHANGE_PERCENT, iType, iInc)
				screen.setStyle(Btn, "Button_Plus_Style")
				Btn = aName()
				screen.setImageButtonAt(Btn, Pnl, "", 28+Btnsize, y+yBtnOffset, Btnsize, Btnsize, WidgetTypes.WIDGET_CHANGE_PERCENT, iType, -iInc)
				screen.setStyle(Btn, "Button_Minus_Style")

				szText = uFont2b + str(CyPlayer.getCommercePercent(iType)) + "%"
				screen.setLabelAt(aName(), Pnl, szText, 1<<1, 112+xBtnOffset, y+yBtnOffset, 0, eGameFont, eWidGen, 1, 1)

				if CommerceTypes(iType) == CommerceTypes.COMMERCE_RESEARCH:
					# Research is subject to modifiers.
					iRate = CyPlayer.calculateResearchRate(CyPlayer.getCurrentResearch())
				else:
					iRate = CyPlayer.getCommerces()[iType]
				szRate = uFont2b + str(iRate) + iconCommerceList[iType]
				screen.setLabelAt(aName(), Pnl, szRate, 1<<1, x, y, 0, eGameFont, eWidGen, 1, 1)
				y += Btnsize
			elif iType == eComGold:
				y += Btnsize - 12
				szText = uFont2b + iconCommerceList[iType]
				screen.setLabelAt(aName(), Pnl, szText, 1<<0, 8, y + 2, 0, eGameFont, eWidGen, 1, 1)

				szText = uFont2b + INFO.getDescription("COMMERCE_", iType)
				screen.setLabelAt(aName(), Pnl, szText, 1<<0, 26, y+yBtnOffset, 0, eGameFont, eWidGen, 1, 1)

				szText = uFont2b + str(CyPlayer.getCommercePercent(iType)) + "%"
				screen.setLabelAt(aName(), Pnl, szText, 1<<1, 108+xBtnOffset, y+yBtnOffset, 0, eGameFont, eWidGen, 1, 1)

				szCommerce = uFont2b + str(iIncome) + iconCommerceList[iType]
				screen.setLabelAt(aName(), Pnl, szCommerce, 1<<1, x, y+yBtnOffset, 0, eGameFont, eWidGen, 1, 1)

		# Income and expenses, as the engine's gold calculation makes them (CvPlayer::calculateBaseNetGold).
		self.drawRows(screen, "FinAdv_Scroll_2", FinancePart.FINANCE_PART_INCOME, self.aRowNames)
		self.drawRows(screen, "FinAdv_Scroll_3", FinancePart.FINANCE_PART_EXPENSES, self.aRowNames)



	# returns a unique ID for a widget in this screen
	def getNextWidgetName(self):
		szName = "FinAdv_Widget" + str(self.nWidgetCount)
		self.nWidgetCount += 1
		return szName

	def deleteAllWidgets(self, screen, aName):
		i = self.nWidgetCount - 1
		while i >= 0:
			self.nWidgetCount = i
			screen.deleteWidget(aName())
			i -= 1
		self.nWidgetCount = 0


	#--------------------------#
	# Base operation functions #
	#||||||||||||||||||||||||||#
	def update(self, fDelta):
		if CyInterface().isDirty(InterfaceDirtyBits.Financial_Screen_DIRTY_BIT):
			CyInterface().setDirty(InterfaceDirtyBits.Financial_Screen_DIRTY_BIT, False)
			self.updateContents()
		if self.tooltip.bLockedTT:
			self.tooltip.handle(self.getScreen())

	# Will handle the input for this screen...
	def handleInput(self, inputClass):

		iCode	= inputClass.eNotifyCode
		NAME	= inputClass.szFunctionName

		screen = self.getScreen()
		# Not on arrival: a widget's own hover may already be drawn by then (PythonToolTip.showWidgetHover).
		if iCode != 4:
			self.tooltip.reset(screen)

		if iCode == 4: # Mouse Enter

			if NAME == "FinAdv_UnitUpkeep":
				self.tooltip.handle(screen, CyGameTextMgr().getFinanceUnitUpkeepString(self.iPlayer))

		elif iCode == 11: # List Select
			self.iPlayer = screen.getPullDownData("FinAdv_DebugDD", inputClass.iData)
			self.CyPlayer = GC.getPlayer(self.iPlayer)
			screen.deleteWidget("FinAdv_Scroll_1")
			screen.deleteWidget("FinAdv_Scroll_2")
			screen.deleteWidget("FinAdv_Scroll_3")
			del self.aRowNames[:]
			self.drawBase()
			self.updateContents()
			return 1
		return 0

	def onClose(self):
		screen = self.getScreen()
		screen.setDying(True)
		del self.nWidgetCount, self.CyPlayer, self.iPlayer, self.bStrike, self.aFontList, self.iconCommerceList, \
			self.szTreasury, self.yCommerceSlider, self.aRowNames, self.aBaseRowNames, self.TERM_WIDGETS, self.xValues
