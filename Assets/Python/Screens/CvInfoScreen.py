#
# Info Screen - Contains the Demographics, Wonders / Top Cities and Statistics Screens
#
# Thanks to "Ulf 'ulfn' Norell" from Apolyton for his additions relating to the graph section of this screen
#
from CvPythonExtensions import *
import math
import PythonToolTip

# globals
GC = CyGlobalContext()
INFO = CyInfo()
GAME = GC.getGame()
TRNSLTR = CyTranslator()

TEXT = CyGameTextMgr()
class CvInfoScreen:

	def __init__(self, screenId):

		self.screenId = screenId
		# The wonders table's hover columns, silent areas (§ THE ONE HOVER, Hover state).
		self.wondersRects = []

		self.szWonderDisplayMode = "WorldWonders"

		self.iGraphEnd		= None
		self.iGraphZoom		= None
		self.nLineCount		= 0

		self.iTab = 0

		self.iGraph1 = 0
		self.iGraph2 = 1
		self.iGraph_Smoothing = 10
		self.bDoubleGraph = False

		# Placement of top 5 Cities
		self.Y_ROWS_CITIES = [90, 208, 326, 444, 562] # delta = 118


	# Screen construction function
	def interfaceScreen(self, iTabID):
		PythonToolTip.noteSilentArea(self)

		if GAME.getReplayMessageTurn(GAME.getNumReplayMessages() - 1) < -1:
			return
		screen = CyGInterfaceScreen("InfoScreen", self.screenId)
		if screen.isActive():
			return
		self.nWidgetCount	= 0

		if iTabID > -1:
			self.iTab = iTabID

		self.iTurn = GAME.getGameTurn()
		self.iStartTurn = GAME.getStartTurn()

		self.aWidgetBucket = []

		import BugCore
		AdvisorOpt = BugCore.game.Advisors
		self.bGraphLogScale = AdvisorOpt.isGraphsLogScale()
		self.bWonderPlayerColor = AdvisorOpt.isWonderListUsePlayerColor()
		self.bShowImprovement = AdvisorOpt.isShowImprovements()

		# Get screen resolution.
		import ScreenResolution as SR
		self.xRes = xRes = SR.x
		self.yRes = yRes = SR.y
		# Calibrate variables.
		if yRes > 1000:
			H_BOT_PANEL = 38
			uFontEdge = "<font=4b>"
		elif yRes > 800:
			H_BOT_PANEL = 32
			uFontEdge = "<font=3b>"
		else:
			H_BOT_PANEL = 29
			uFontEdge = "<font=2b>"

		self.aFontList = aFontList = [uFontEdge] + SR.aFontList
		# uFontEdge, uFont4b, uFont4, uFont3b, uFont3, uFont2b, uFont2, uFont1b, uFont1 = self.aFontList
		uFont2b = aFontList[5]

		self.H_PAGE = yRes - 36 - H_BOT_PANEL + 8
		self.Y_BOT_TEXT = Y_BOT_TEXT = yRes - H_BOT_PANEL + 8

		# Initialize text
		self.TEXT_ENTIRE_HISTORY = TRNSLTR.getText("TXT_KEY_INFO_ENTIRE_HISTORY", ())
		self.TEXT_TURNS = TRNSLTR.getText("TXT_KEY_TURNS", ())

		self.TEXT_TIME_PLAYED = TRNSLTR.getText("TXT_KEY_INFO_SCREEN_TIME_PLAYED", ())
		self.TEXT_CITIES_BUILT = TRNSLTR.getText("TXT_KEY_INFO_SCREEN_CITIES_BUILT", ())
		self.TEXT_CITIES_RAZED = TRNSLTR.getText("TXT_KEY_INFO_SCREEN_CITIES_RAZED", ())
		self.TEXT_NUM_RELIGIONS_FOUNDED = TRNSLTR.getText("TXT_KEY_INFO_SCREEN_RELIGIONS_FOUNDED", ())

		self.TEXT_CURRENT = TRNSLTR.getText("TXT_KEY_CURRENT", ())
		self.TEXT_UNITS = TRNSLTR.getText("TXT_KEY_CONCEPT_UNITS", ())
		self.TEXT_BUILDINGS = TRNSLTR.getText("TXT_KEY_WB_BUILDINGS", ())
		self.TEXT_KILLED = TRNSLTR.getText("TXT_KEY_INFO_SCREEN_KILLED", ())
		self.TEXT_LOST = TRNSLTR.getText("TXT_KEY_INFO_SCREEN_LOST", ())
		self.TEXT_BUILT = TRNSLTR.getText("TXT_KEY_INFO_SCREEN_BUILT", ())
		self.TEXT_IMPROVEMENTS = TRNSLTR.getText("TXT_KEY_CONCEPT_IMPROVEMENTS", ())

		self.SHOW_ALL = uFont2b + TRNSLTR.getText("TXT_KEY_SHOW_ALL", ())
		self.SHOW_NONE = uFont2b + TRNSLTR.getText("TXT_KEY_SHOW_NONE", ())
		self.LOG_SCALE = uFont2b + TRNSLTR.getText("TXT_KEY_LOGSCALE", ())

		self.szName = TRNSLTR.getText("TXT_WORD_NAME", ())
		self.szWonderDate = TRNSLTR.getText("TXT_KEY_WONDER_DATE", ())
		self.szWonderOwner = TRNSLTR.getText("TXT_KEY_WONDER_OWNER", ())
		self.szWonderCity = TRNSLTR.getText("TXT_WORD_CITY", ())

		sTemp1 = [
			TRNSLTR.getText("TXT_KEY_GAME_SCORE", ()),
			TRNSLTR.getText("TXT_KEY_DEMO_SCREEN_COMMERCE", ()),
			TRNSLTR.getText("TXT_KEY_DEMO_SCREEN_PRODUCTION", ()),
			TRNSLTR.getText("TXT_KEY_DEMO_SCREEN_FOOD", ()),
			TRNSLTR.getText("TXT_KEY_POWER", ()),
			TRNSLTR.getText("TXT_WORD_CULTURE", ()),
			TRNSLTR.getText("TXT_WORD_ESPIONAGE", ()),
		]
		if GAME.isOption(GameOptionTypes.GAMEOPTION_UNSUPPORTED_REVOLUTION):
			sTemp1.append(TRNSLTR.getText("TXT_KEY_REVOLUTION_GRAPH_TITLE_SMALL", ()))
			self.iNumGraphs = 8
		else: self.iNumGraphs = 7

		self.iBlack = GC.getInfoTypeForString("COLOR_BLACK")
		iYellow = GC.getInfoTypeForString("COLOR_YELLOW")
		sTemp2 = []
		for txt in sTemp1:
			sTemp2.append(TRNSLTR.changeTextColor(txt, iYellow))

		self.sGraphText = [sTemp1, sTemp2]

		# determine the big graph text spacing
		self.X_GRAPH_TEXT = []
		x = xRes - 16
		for i in xrange(self.iNumGraphs):
			self.X_GRAPH_TEXT.append(x)
			x -= 32 + CyInterface().determineWidth(aFontList[3] + sTemp1[i])

		self.BUG_LEGEND_DEAD = TRNSLTR.getText("TXT_KEY_BUG_DEAD_CIV", ())

		AFM = CyArtFileMgr()
		# world wonder / national wonder / projects icons / buttons
		self.BUGWorldWonder_On = AFM.getInterfaceArtInfo("BUG_WORLDWONDER_ON").getPath()
		self.BUGWorldWonder_Off = AFM.getInterfaceArtInfo("BUG_WORLDWONDER_OFF").getPath()
		self.BUGNatWonder_On = AFM.getInterfaceArtInfo("BUG_NATWONDER_ON").getPath()
		self.BUGNatWonder_Off = AFM.getInterfaceArtInfo("BUG_NATWONDER_OFF").getPath()
		self.BUGProject_On = AFM.getInterfaceArtInfo("BUG_PROJECT_ON").getPath()
		self.BUGProject_Off = AFM.getInterfaceArtInfo("BUG_PROJECT_OFF").getPath()

		self.pathNullBtn = AFM.getInterfaceArtInfo("INTERFACE_BUTTON_NULL").getPath()
		self.zoomArt = AFM.getInterfaceArtInfo("INTERFACE_BUTTONS_CITYSELECTION").getPath()

		# Cache player specifics
		self.iPlayer = iPlayer = GAME.getActivePlayer()
		self.determineKnownPlayers()

		# Build screen
		eWidGen = WidgetTypes.WIDGET_GENERAL
		eFontTitle = FontTypes.TITLE_FONT

		screen.setRenderInterfaceOnly(True)
		screen.showScreen(PopupStates.POPUPSTATE_IMMEDIATE, False)

		screen.addDDSGFC("Info_BG", AFM.getInterfaceArtInfo("MAINMENU_SLIDESHOW_LOAD").getPath(), 0, 0, xRes, yRes, eWidGen, 1, 2)
		screen.addDDSGFC("Graph_BG", AFM.getInterfaceArtInfo("SCREEN_BG").getPath(), 0, 0, self.xRes, self.yRes, eWidGen, 1, 2)
		screen.hide("Graph_BG")
		screen.addPanel("Info_TopPanel", "", "", True, False, 0, -2, xRes, 40, PanelStyles.PANEL_STYLE_TOPBAR)
		screen.addPanel("Info_BotPanel", "", "", True, False, 0, yRes - H_BOT_PANEL, xRes, H_BOT_PANEL, PanelStyles.PANEL_STYLE_BOTTOMBAR)
		screen.setLabel("Info_Header", "", uFontEdge + TRNSLTR.getText("TXT_KEY_INFO_SCREEN",()), 1<<2, xRes / 2, 2, 0, eFontTitle, eWidGen, 0, 0)
		screen.setText("Info_Exit", "", uFontEdge + TRNSLTR.getText("TXT_WORD_EXIT",()), 1<<1, xRes - 16, 0, 0, eFontTitle, WidgetTypes.WIDGET_CLOSE_SCREEN, -1, -1)

		# Add tab control
		szTxt = uFontEdge + TRNSLTR.getText("TXT_KEY_DEMO_SCREEN_TITLE", ())
		szTxtCol = TRNSLTR.changeTextColor(szTxt, iYellow)
		dX = xRes / 4
		x = dX / 2
		screen.setText("VS_Tab0", "", szTxt, 1<<2, x, Y_BOT_TEXT, 0, eFontTitle, eWidGen, 0, 0)
		screen.setText("VS_Tab_Act0", "", szTxtCol, 1<<2, x, Y_BOT_TEXT, 0, eFontTitle, eWidGen, 0, 0)
		screen.hide("VS_Tab_Act0")

		szTxt = uFontEdge + TRNSLTR.getText("TXT_KEY_INFO_GRAPH", ())
		szTxtCol = TRNSLTR.changeTextColor(szTxt, iYellow)
		x += dX
		screen.setText("VS_Tab1", "", szTxt, 1<<2, x, Y_BOT_TEXT, 0, eFontTitle, eWidGen, 0, 0)
		screen.setText("VS_Tab_Act1", "", szTxtCol, 1<<2, x, Y_BOT_TEXT, 0, eFontTitle, eWidGen, 0, 0)
		screen.hide("VS_Tab_Act1")

		szTxt = uFontEdge + TRNSLTR.getText("TXT_KEY_WONDERS_SCREEN_TOP_CITIES", ())
		szTxtCol = TRNSLTR.changeTextColor(szTxt, iYellow)
		x += dX
		screen.setText("VS_Tab2", "", szTxt, 1<<2, x, Y_BOT_TEXT, 0, eFontTitle, eWidGen, 0, 0)
		screen.setText("VS_Tab_Act2", "", szTxtCol, 1<<2, x, Y_BOT_TEXT, 0, eFontTitle, eWidGen, 0, 0)
		screen.hide("VS_Tab_Act2")

		szTxt = uFontEdge + TRNSLTR.getText("TXT_KEY_INFO_SCREEN_STATISTICS_TITLE", ())
		szTxtCol = TRNSLTR.changeTextColor(szTxt, iYellow)
		x += dX
		screen.setText("VS_Tab3", "", szTxt, 1<<2, x, Y_BOT_TEXT, 0, eFontTitle, eWidGen, 0, 0)
		screen.setText("VS_Tab_Act3", "", szTxtCol, 1<<2, x, Y_BOT_TEXT, 0, eFontTitle, eWidGen, 0, 0)
		screen.hide("VS_Tab_Act3")

		# Debug DropDown
		import DebugUtils
		if DebugUtils.isAnyDebugMode():
			DD = "Info_DebugDD"
			screen.addDropDownBoxGFC(DD, 8, 0, 300, eWidGen, 1, 2, FontTypes.GAME_FONT)
			for iPlayerX in xrange(GC.getMAX_PLAYERS()):
				if GC.getPlayer(iPlayerX).isAlive():
					screen.addPullDownString(DD, GC.getPlayer(iPlayerX).getName(), iPlayerX, iPlayerX, iPlayer == iPlayerX)

		# Reset variables
		iLastTurn = self.iTurn - 1
		if self.iGraphEnd != iLastTurn:
			self.iGraphEnd = iLastTurn
			self.iGraphZoom = self.iGraphEnd - self.iStartTurn
			if self.iGraphZoom > 1000:
				self.iGraphZoom = 1000

		self.redrawContents()


	def redrawContents(self):
		# The wonders table is a silent area (§ THE ONE HOVER, Button lists).
		PythonToolTip.buildButtonLists(self.drawContents)

	def drawContents(self):
		screen = CyGInterfaceScreen("InfoScreen", self.screenId)
		self.deleteAllWidgets(screen)
		self.wondersRects = []
		# Specific widgets
		for widget in self.aWidgetBucket:
			screen.deleteWidget(widget)
		self.aWidgetBucket = []

		# Draw Tab buttons and tabs
		if not self.iTab:
			screen.hide("VS_Tab0")
			screen.show("VS_Tab_Act0")
			self.drawDemographicsTab(screen)

		elif self.iTab == 1:
			screen.show("Graph_BG")
			screen.hide("VS_Tab1")
			screen.show("VS_Tab_Act1")
			self.drawGraphTab(screen)

		elif self.iTab == 2:
			screen.hide("VS_Tab2")
			screen.show("VS_Tab_Act2")
			self.drawTopCitiesTab(screen)

		elif self.iTab == 3:
			screen.hide("VS_Tab3")
			screen.show("VS_Tab_Act3")
			self.drawStatsTab(screen)

#############################################################################################################
#################################################### GRAPH ##################################################
#############################################################################################################

	def drawGraphTab(self, screen):

		self.scoreCache = [None] * self.iNumGraphs
		self.bPlayerInclude = [True] * GC.getMAX_PC_PLAYERS()

		xRes = self.xRes
		self.wLegend = wLegend = 128 + xRes / 20
		self.xGraph = xGraph = 32 + wLegend
		self.wGraph = wGraph = xRes - xGraph - 16
		self.iNumReference = 2 + xRes / 420
		self.iReferenceDelta = wGraph / (self.iNumReference + 1)

		self.sGraphTextHeadingWidget = []
		self.sGraphTextBannerWidget = []
		self.sGraphPanelWidget = []
		for i in xrange(self.iNumGraphs):
			self.sGraphTextHeadingWidget.append(self.getNextWidgetName())
			self.sGraphTextBannerWidget.append(self.getNextWidgetName())
			self.sGraphPanelWidget.append(self.getNextWidgetName())

		self.sPlayerTextWidget = [0] * GC.getMAX_PC_PLAYERS()
		for i in xrange(GC.getMAX_PC_PLAYERS()):
			self.sPlayerTextWidget[i] = self.getNextWidgetName()

		self.sShowAllWidget = self.getNextWidgetName()
		self.sShowNoneWidget = self.getNextWidgetName()

		eWidGen = WidgetTypes.WIDGET_GENERAL
		eFontGame = FontTypes.GAME_FONT

		y = self.Y_BOT_TEXT - 30
		screen.setButtonGFC("GraphShiftL", "", "", xGraph - 12, y, 20, 20, eWidGen, 1, 2, ButtonStyles.BUTTON_STYLE_ARROW_LEFT)
		screen.setButtonGFC("GraphShiftR", "", "", xRes - 24, y, 20, 20, eWidGen, 1, 2, ButtonStyles.BUTTON_STYLE_ARROW_RIGHT)
		self.aWidgetBucket.append("GraphShiftL")
		self.aWidgetBucket.append("GraphShiftR")


		# graph smoothing dropdown
		self.szGraphSmoothingDropdownWidget = self.getNextWidgetName()

		screen.addDropDownBoxGFC(self.szGraphSmoothingDropdownWidget, xRes - 316, 0, 200, eWidGen, 1, 2, eFontGame)
		for i in xrange(6):
			iV = 2*i
			screen.addPullDownString(self.szGraphSmoothingDropdownWidget, TRNSLTR.getText("TXT_KEY_GRAPH_SMOOTHING", (iV,)), i, iV, self.iGraph_Smoothing == iV)

		# graph range dropdown
		start = self.iStartTurn
		now   = self.iTurn
		bEntireHistory = self.iGraphZoom == self.iGraphEnd - start
		self.dropDownTurns = []
		self.szTurnsDropdownWidget = self.getNextWidgetName()
		screen.addDropDownBoxGFC(self.szTurnsDropdownWidget, 16, self.H_PAGE, wLegend, eWidGen, 1, 2, eFontGame)
		nTurns = now - start - 1
		screen.addPullDownString(self.szTurnsDropdownWidget, self.TEXT_ENTIRE_HISTORY, 0, 0, bEntireHistory)
		self.dropDownTurns.append(nTurns)
		if 50 < nTurns:
			screen.addPullDownString(self.szTurnsDropdownWidget, self.TEXT_TURNS + ": " + str(50), 1, 1, not bEntireHistory and self.iGraphZoom == 50)
			self.dropDownTurns.append(50)
			iCounter = 2
			last = 100
			while last < nTurns and last < 1001:
				screen.addPullDownString(self.szTurnsDropdownWidget, self.TEXT_TURNS + ": " + str(last), iCounter, iCounter, not bEntireHistory and self.iGraphZoom == last)
				self.dropDownTurns.append(last)
				iCounter += 1
				last += 100

		self.iNumPreDemoChartWidgets = self.nWidgetCount

		self.drawGraphs()

	def checkGraphBounds(self):
		if self.iGraphEnd - self.iGraphZoom < self.iStartTurn:
			self.iGraphEnd = self.iStartTurn + self.iGraphZoom
		if self.iGraphEnd > self.iTurn - 1:
			self.iGraphEnd = self.iTurn - 1

	def slideGraph(self, right):
		self.iGraphEnd += right
		self.checkGraphBounds()


	# The values graph iGraph draws for iPlayer, one per turn from the start turn to this turn.
	def computeHistory(self, iGraph, iPlayer):
		# The history each graph draws, in the order of the graph titles.
		aHistory = (
			PlayerHistoryKind.PLAYER_HISTORY_SCORE,
			PlayerHistoryKind.PLAYER_HISTORY_ECONOMY,
			PlayerHistoryKind.PLAYER_HISTORY_INDUSTRY,
			PlayerHistoryKind.PLAYER_HISTORY_AGRICULTURE,
			PlayerHistoryKind.PLAYER_HISTORY_POWER,
			PlayerHistoryKind.PLAYER_HISTORY_CULTURE,
			PlayerHistoryKind.PLAYER_HISTORY_ESPIONAGE,
			PlayerHistoryKind.PLAYER_HISTORY_REVOLUTION_STABILITY
		)
		aValues = GC.getPlayer(iPlayer).getHistory(aHistory[iGraph], self.iStartTurn, self.iTurn)
		if aHistory[iGraph] == PlayerHistoryKind.PLAYER_HISTORY_REVOLUTION_STABILITY:
			return [1000 - iValue for iValue in aValues]
		return aValues


	def getDate(self, turn):
		year = GAME.getTurnYear(turn)
		if year < 0:
			return self.aFontList[4] + TRNSLTR.getText("TXT_KEY_TIME_BC", (-year,))
		return self.aFontList[4] + TRNSLTR.getText("TXT_KEY_TIME_AD", (year,))


	def drawGraphs(self):
		startTurn = self.iStartTurn
		thisTurn = self.iTurn
		xGraph = self.xGraph
		wGraph = self.wGraph

		eWidGen = WidgetTypes.WIDGET_GENERAL
		eFont = FontTypes.TITLE_FONT

		screen = CyGInterfaceScreen("InfoScreen", self.screenId)

		# Delete graph lines
		i = 0
		while i < self.nLineCount:
			screen.deleteWidget("DemoLine" + str(i))
			i += 1
		self.nLineCount = 0

		self.deleteAllWidgets(screen, self.iNumPreDemoChartWidgets)

		screen.enable("GraphShiftL", self.iGraphEnd - self.iGraphZoom > startTurn)
		screen.enable("GraphShiftR", self.iGraphEnd < thisTurn - 1)

		for i in xrange(self.iNumGraphs):
			screen.hide(self.sGraphTextHeadingWidget[i])
			screen.hide(self.sGraphPanelWidget[i])

		if self.bDoubleGraph:
			for i in xrange(self.iNumGraphs):
				screen.hide(self.sGraphTextBannerWidget[i])
			# Double graph selections
			DD = "GraphDD0"
			x = xGraph + wGraph / 2 - 208
			screen.addDropDownBoxGFC(DD, x, 40, 200, eWidGen, 1, 2, eFont)
			for j, txt in enumerate(self.sGraphText[0]):
				screen.addPullDownString(DD, txt, j, j, self.iGraph1 == j)
			self.aWidgetBucket.append(DD)

			DD = "GraphDD1"
			x += 216
			screen.addDropDownBoxGFC(DD, x, 40, 200, eWidGen, 1, 2, eFont)
			for j, txt in enumerate(self.sGraphText[0]):
				screen.addPullDownString(DD, txt, j, j, self.iGraph2 == j)
			self.aWidgetBucket.append(DD)
		else:
			screen.hide("GraphDD0")
			screen.hide("GraphDD1")

			uFont3b = self.aFontList[3]

			for i, x in enumerate(self.X_GRAPH_TEXT):
				screen.hide(self.sGraphTextBannerWidget[i])
				if i == self.iGraph1:
					screen.setText(self.sGraphTextBannerWidget[i], "", uFont3b + self.sGraphText[1][i], 1<<1, x, 41, 0, eFont, eWidGen, 1, 2)
				else:
					screen.setText(self.sGraphTextBannerWidget[i], "", uFont3b + self.sGraphText[0][i], 1<<1, x, 36, 0, eFont, eWidGen, 1, 2)

		# Draw graph(s)
		for i in xrange(1 + self.bDoubleGraph):

			if self.bDoubleGraph and i:
				iGraphID = self.iGraph2
			else: iGraphID = self.iGraph1

			# compute graph x, y, w, h
			iH_GRAPH = self.yRes - 134
			if self.bDoubleGraph:
				iH_GRAPH = iH_GRAPH / 2 - 4
			iY_GRAPH = 72
			if self.bDoubleGraph and i:
				iY_GRAPH += iH_GRAPH + 8

			# Draw the graph widget
			zsGRAPH_CANVAS_ID = self.getNextWidgetName()
			screen.addDrawControl(zsGRAPH_CANVAS_ID, None, xGraph, iY_GRAPH, wGraph, iH_GRAPH, eWidGen, 1, 2)
			screen.setStyle(zsGRAPH_CANVAS_ID, "Panel_HudHelp_Style")

			x = self.iReferenceDelta
			for i in xrange(self.iNumReference):
				screen.addLineGFC(zsGRAPH_CANVAS_ID, self.getNextLineName(), x, iH_GRAPH, x, 0, self.iBlack)
				x += self.iReferenceDelta

			screen.addCheckBoxGFC("GraphArea" + str(iGraphID), self.pathNullBtn, self.pathNullBtn, xGraph, iY_GRAPH, wGraph, iH_GRAPH, eWidGen, 1, 2, ButtonStyles.BUTTON_STYLE_LABEL)

			# Compute max score, check if the scores have already been computed
			if self.scoreCache[iGraphID] is None:

				# Get the player with the highest ID
				maxPlayer = 0
				for p in self.aiPlayersMet:
					if maxPlayer < p:
						maxPlayer = p

				# Compute the scores
				self.scoreCache[iGraphID] = []
				for p in xrange(maxPlayer + 1):

					if p in self.aiPlayersMet:
						self.scoreCache[iGraphID].append(self.computeHistory(iGraphID, p))
					else: # Don't compute score for people we haven't met
						self.scoreCache[iGraphID].append(None)


			if not self.iGraphZoom or not self.iGraphEnd:
				firstTurn = startTurn
			else: firstTurn = self.iGraphEnd - self.iGraphZoom

			if self.iGraphEnd:
				lastTurn = self.iGraphEnd
			else: lastTurn = thisTurn - 1 # all civs haven't necessarily got a score for the current turn

			# Draw x-labels
			screen.setLabel(self.getNextWidgetName(), "", self.getDate(firstTurn), 1<<0, xGraph + 14, self.Y_BOT_TEXT - 30, 0, eFont, eWidGen, 1, 2)
			screen.setLabel(self.getNextWidgetName(), "", self.getDate(lastTurn), 1<<1, self.xRes - 30, self.Y_BOT_TEXT - 30, 0, eFont, eWidGen, 1, 2)

			# Don't draw anything the first turn
			if firstTurn >= lastTurn:
				return

			# Compute max and min
			if self.bGraphLogScale:
				max = 2
				min = 1
			else:
				max = 1
				min = 0

			for p in self.aiPlayersMet:
				for turn in xrange(firstTurn, lastTurn + 1):
					score = self.scoreCache[iGraphID][p][turn - startTurn]
					if max < score:
						max = score
					if min > score:
						min = score

			if self.bGraphLogScale:
				yFactor = 1.0 * iH_GRAPH / (self.getLog10(max) - self.getLog10(min))
			else:
				yFactor = 1.0 * iH_GRAPH / (max - min)

			xFactor = 1.0 * wGraph / (lastTurn - firstTurn)

			x = self.iReferenceDelta
			turnDelta = (lastTurn - firstTurn) / (self.iNumReference + 1)
			iTurn = firstTurn + turnDelta
			for i in xrange(self.iNumReference):
				screen.setLabel(self.getNextWidgetName(), "", self.getDate(iTurn), 1<<2, xGraph + x, self.Y_BOT_TEXT - 30, 0, eFont, eWidGen, 1, 2)
				x += self.iReferenceDelta
				iTurn += turnDelta

			# Draw the lines
			for p in self.aiPlayersMet:

				if not self.bPlayerInclude[p]:
					continue

				color = GC.getPlayer(p).getColorPrimary()
				oldX = -1
				oldY = iH_GRAPH
				turn = lastTurn

				iSmooth = self.iGraph_Smoothing

				while turn >= firstTurn:

					score = self.scoreCache[iGraphID][p][turn - startTurn]

					if self.bGraphLogScale:
						y = iH_GRAPH - int(yFactor * (self.getLog10(score) - self.getLog10(min)))
					else:
						y = iH_GRAPH - int(yFactor * (score - min))
					x = int(xFactor * (turn - firstTurn))

					if x < oldX - iSmooth:
						if y != iH_GRAPH or oldY != iH_GRAPH:
							screen.addLineGFC(zsGRAPH_CANVAS_ID, self.getNextLineName(), oldX, oldY + 1, x, y + 1, color)
							screen.addLineGFC(zsGRAPH_CANVAS_ID, self.getNextLineName(), oldX - 1, oldY, x - 1, y, color)
							screen.addLineGFC(zsGRAPH_CANVAS_ID, self.getNextLineName(), oldX, oldY, x, y, color)
						oldX = x
						oldY = y
					elif oldX == -1:
						oldX = x
						oldY = y

					turn -= 1

		# Draw Legend
		uFont2b = self.aFontList[5]

		pnl = self.getNextWidgetName()
		screen.addPanel(pnl, "", "", True, True, 16, 74, self.wLegend, self.H_PAGE - 74, PanelStyles.PANEL_STYLE_IN)
		screen.setStyle(pnl, "ScrollPanel_Alt_Style")

		for iPlayerX in self.aiPlayersMet:

			playerX = GC.getPlayer(iPlayerX)
			name = playerX.getName()

			if self.bPlayerInclude[iPlayerX]:
				textColorR = playerX.getPlayerTextColorR()
				textColorG = playerX.getPlayerTextColorG()
				textColorB = playerX.getPlayerTextColorB()
			else:
				textColorR = 175
				textColorG = 175
				textColorB = 175

			txt = "<color=%d,%d,%d>%s</color>" %(textColorR, textColorG, textColorB, name)

			screen.attachTextGFC(pnl, self.sPlayerTextWidget[iPlayerX], uFont2b + txt, eFont, eWidGen, 1, 2)

		screen.attachLabel(pnl, self.getNextWidgetName(), "")
		screen.attachTextGFC(pnl, self.sShowAllWidget, self.SHOW_ALL, eFont, eWidGen, 1, 2)
		screen.attachTextGFC(pnl, self.sShowNoneWidget, self.SHOW_NONE, eFont, eWidGen, 1, 2)
		screen.attachLabel(pnl, self.getNextWidgetName(), "")

		# ADD players where you don't have enough espionage points
		for iPlayerX in self.aiPlayersMetNAEspionage:

			playerX = GC.getPlayer(iPlayerX)
			name = playerX.getName()

			if not playerX.isAlive(): # player is dead!
				textColorR = 175
				textColorG = 175
				textColorB = 175
				name += " [" + self.BUG_LEGEND_DEAD + "]"
			else:
				textColorR = playerX.getPlayerTextColorR()
				textColorG = playerX.getPlayerTextColorG()
				textColorB = playerX.getPlayerTextColorB()

			txt = "<color=%d,%d,%d>%s</color>" %(textColorR, textColorG, textColorB, name)

			screen.attachLabel(pnl, self.sPlayerTextWidget[iPlayerX], uFont2b + txt)

		if self.bGraphLogScale:
			screen.attachLabel(pnl, self.getNextWidgetName(), "")
			screen.attachLabel(pnl, self.getNextWidgetName(), self.LOG_SCALE)


#############################################################################################################
################################################# DEMOGRAPHICS ##############################################
#############################################################################################################

	def drawDemographicsTab(self, screen):
		if self.demographicsTable:
			screen.show(self.demographicsTable)
			return
		eWidGen = WidgetTypes.WIDGET_GENERAL

		# Create Table
		self.demographicsTable = table = "DemographicsTable"
		screen.addTableControlGFC(table, 6, 45, 80, 934, 600, True, True, 32,32, TableStyles.TABLE_STYLE_STANDARD)
		screen.setTableColumnHeader(table, 0, TRNSLTR.getText("TXT_KEY_DEMO_SCREEN_TITLE", ()), 224) # Total graph width is 430
		screen.setTableColumnHeader(table, 1, TRNSLTR.getText("TXT_KEY_DEMO_SCREEN_VALUE", ()), 155)
		screen.setTableColumnHeader(table, 2, TRNSLTR.getText("TXT_KEY_INFO_RIVAL_BEST", ()), 155)
		screen.setTableColumnHeader(table, 3, TRNSLTR.getText("TXT_KEY_DEMOGRAPHICS_SCREEN_RIVAL_AVERAGE", ()), 155)
		screen.setTableColumnHeader(table, 4, TRNSLTR.getText("TXT_KEY_INFO_RIVAL_WORST", ()), 155)
		screen.setTableColumnHeader(table, 5, TRNSLTR.getText("TXT_KEY_DEMO_SCREEN_RANK", ()), 90)

		# Each measure's label; a blank row separates one measure from the next.
		labelOfKind = {
			DemographicKind.DEMOGRAPHIC_ECONOMY:		"TXT_KEY_DEMO_SCREEN_COMMERCE",
			DemographicKind.DEMOGRAPHIC_INDUSTRY:		"TXT_KEY_DEMO_SCREEN_PRODUCTION",
			DemographicKind.DEMOGRAPHIC_AGRICULTURE:	"TXT_KEY_DEMO_SCREEN_FOOD",
			DemographicKind.DEMOGRAPHIC_MILITARY:		"TXT_KEY_DEMO_SCREEN_POWER",
			DemographicKind.DEMOGRAPHIC_LAND_AREA:		"TXT_KEY_DEMO_SCREEN_LAND_PLOTS",
			DemographicKind.DEMOGRAPHIC_POPULATION:		"TXT_KEY_DEMO_SCREEN_POPULATION",
			DemographicKind.DEMOGRAPHIC_HAPPINESS:		"TXT_KEY_DEMO_SCREEN_HAPPINESS",
			DemographicKind.DEMOGRAPHIC_HEALTH:			"TXT_KEY_DEMO_SCREEN_HEALTH",
			DemographicKind.DEMOGRAPHIC_NET_TRADE:		"TXT_KEY_DEMO_SCREEN_NET_TRADE"
		}
		for i in xrange(2 * DemographicKind.NUM_DEMOGRAPHIC_KINDS - 1):
			screen.appendTableRow(table)

		for iKind, szValue, szBestRival, szRivalAverage, szWorstRival, iRank in GC.getPlayer(self.iPlayer).getDemographicsRows():
			iRow = 2 * iKind
			screen.setTableText(table, 0, iRow, TRNSLTR.getText(labelOfKind[iKind], ()), "", eWidGen, 1, 2, 1<<0)
			screen.setTableText(table, 1, iRow, szValue, "", eWidGen, 1, 2, 1<<0)
			screen.setTableText(table, 2, iRow, szBestRival, "", eWidGen, 1, 2, 1<<0)
			screen.setTableText(table, 3, iRow, szRivalAverage, "", eWidGen, 1, 2, 1<<0)
			screen.setTableText(table, 4, iRow, szWorstRival, "", eWidGen, 1, 2, 1<<0)
			screen.setTableText(table, 5, iRow, str(iRank), "", eWidGen, 1, 2, 1<<0)


#############################################################################################################
################################################## TOP CITIES ###############################################
#############################################################################################################

	def drawTopCitiesTab(self, screen):

		# Background Panes
		screen.addPanel(self.getNextWidgetName(), "", "", True, True, 45, 70, 470, 620, PanelStyles.PANEL_STYLE_MAIN)

		for iRank, (iOwnerX, iCityX) in enumerate(GAME.getTopCities(len(self.Y_ROWS_CITIES))):
			CyCityX = GC.getPlayer(iOwnerX).getCity(iCityX)
			y = self.Y_ROWS_CITIES[iRank]

			iYear = CyCityX.getYearFounded()
			if iYear < 0:
				szFounded = TRNSLTR.getText("TXT_KEY_MISC_FOUNDED_IN", (TRNSLTR.getText("TXT_KEY_TIME_BC", (-iYear,)),))
			else:
				szFounded = TRNSLTR.getText("TXT_KEY_MISC_FOUNDED_IN", (TRNSLTR.getText("TXT_KEY_TIME_AD", (iYear,)),))

			bRevealed = CyCityX.isRevealedTo(self.iTeam)
			if bRevealed:
				szCityName = CyCityX.getName()
				szCityDesc = "%s, %s" % (GC.getPlayer(iOwnerX).getCivilizationAdjective(0), szFounded)
			else:
				szCityName = TRNSLTR.getText("TXT_KEY_UNKNOWN", ())
				szCityDesc = szFounded

			iPopulation = CyCityX.getPopulation()
			screen.addPanel(self.getNextWidgetName(), "", "", False, True, 225, y - 4, 275, 60, PanelStyles.PANEL_STYLE_DAWNTOP)
			szText = u"<font=4b>" + str(iPopulation) + u"</font>" + " - " + u"<font=3b>" + szCityName + u"</font>" + "\n" + szCityDesc
			screen.addMultilineText(self.getNextWidgetName(), szText, 231, y - 1, 269, 54, WidgetTypes.WIDGET_GENERAL, -1, -1, 1<<0)

			if bRevealed:
				iDistance = 200 + iPopulation * 5
				if iDistance > 400:
					iDistance = 400
				aPosition = CyCityX.getPosition()
				screen.addPlotGraphicGFC(self.getNextWidgetName(), 65, y, 150, 110, CyMap().plot(aPosition[0], aPosition[1]), iDistance, False, WidgetTypes.WIDGET_GENERAL, -1, -1)

			# The city's world wonders, under it.
			szIconPanel = self.getNextWidgetName()
			screen.addPanel(szIconPanel, "", "", False, True, 225, y + 53, 275, 60, PanelStyles.PANEL_STYLE_DAWNTOP)
			for iBuilding in CyCityX.getWonders(WonderListKind.WONDER_LIST_WORLD):
				screen.attachImageButton(szIconPanel, self.getNextWidgetName(), INFO.getButton("BUILDING_", iBuilding), GenericButtonSizes.BUTTON_SIZE_46, WidgetTypes.WIDGET_PEDIA_JUMP_TO_BUILDING, iBuilding, -1, False)


		xDD = 540

		screen.addPanel(self.getNextWidgetName(), "", "", True, True, 520, 70, 460, 620, PanelStyles.PANEL_STYLE_MAIN)

		# Draw the Wonders Dropdown Box"

		if self.szWonderDisplayMode == "WorldWonders":
			sWW = self.BUGWorldWonder_On
			sNW = self.BUGNatWonder_Off
			sPj = self.BUGProject_Off
			sDesc = TRNSLTR.getText("TXT_KEY_TOP_CITIES_SCREEN_WORLD_WONDERS", ())
		elif self.szWonderDisplayMode == "NationalWonders":
			sWW = self.BUGWorldWonder_Off
			sNW = self.BUGNatWonder_On
			sPj = self.BUGProject_Off
			sDesc = TRNSLTR.getText("TXT_KEY_TOP_CITIES_SCREEN_NATIONAL_WONDERS", ())
		else:
			sWW = self.BUGWorldWonder_Off
			sNW = self.BUGNatWonder_Off
			sPj = self.BUGProject_On
			sDesc = TRNSLTR.getText("TXT_KEY_PEDIA_CATEGORY_PROJECT", ())

		screen.setImageButton("WorldWondersWidget", sWW,  xDD +  0, 90, 24, 24, WidgetTypes.WIDGET_INFO_WORLD_WONDERS, -1, -1)
		screen.setImageButton("NationalWondersWidget", sNW,  xDD + 30, 90, 24, 24, WidgetTypes.WIDGET_INFO_NATIONAL_WONDERS, -1, -1)
		screen.setImageButton("ProjectsWidget", sPj,  xDD + 60, 90, 24, 24, WidgetTypes.WIDGET_INFO_PROJECTS, -1, -1)
		self.aWidgetBucket.append("WorldWondersWidget")
		self.aWidgetBucket.append("NationalWondersWidget")
		self.aWidgetBucket.append("ProjectsWidget")

		screen.setLabel(self.getNextWidgetName(), "", "<font=4>" + sDesc, 1<<0, xDD + 100, 93, -6.3, FontTypes.TITLE_FONT, WidgetTypes.WIDGET_GENERAL, 1, 2)

		if self.szWonderDisplayMode == "WorldWonders":
			iList = WonderListKind.WONDER_LIST_WORLD
		elif self.szWonderDisplayMode == "NationalWonders":
			iList = WonderListKind.WONDER_LIST_NATIONAL
		else:
			iList = WonderListKind.WONDER_LIST_PROJECTS

		# Draw Wonders List
		self.szWondersTable = self.getNextWidgetName()

		x = 540
		y = 130
		h = 540
		iRow = 24
		wZoom = 30
		wWonder = 115
		screen.addTableControlGFC(self.szWondersTable, 5, x, y, 420, h, True, True, iRow, iRow, TableStyles.TABLE_STYLE_STANDARD)
		screen.enableSort(self.szWondersTable)
		screen.setTableColumnHeader(self.szWondersTable, 0, "", wZoom)
		screen.setTableColumnHeader(self.szWondersTable, 1, self.szName, wWonder)
		# The zoom column and the wonder column carry hovers.
		self.wondersRects = [(x, y, wZoom, h, wZoom, iRow), (x + wZoom, y, wWonder, h, wWonder, iRow)]
		screen.setTableColumnHeader(self.szWondersTable, 2, self.szWonderDate, 70)
		screen.setTableColumnHeader(self.szWondersTable, 3, self.szWonderOwner, 100)
		screen.setTableColumnHeader(self.szWondersTable, 4, self.szWonderCity, 100)

		if iList == WonderListKind.WONDER_LIST_PROJECTS:
			szPrefix = "PROJECT_"
			iWidget = WidgetTypes.WIDGET_PEDIA_JUMP_TO_PROJECT
		else:
			szPrefix = "BUILDING_"
			iWidget = WidgetTypes.WIDGET_PEDIA_JUMP_TO_BUILDING

		for i, (iType, bUnderConstruction, bHasYear, iYear, iBuilder, iCityOwner, iCityId, bZoomable) in enumerate(GC.getPlayer(self.iPlayer).getWonderRows(iList)):

			if bUnderConstruction:
				szYear = u"<font=2>%c</font>" % TEXT.getSymbolChar("YIELD_", YieldTypes.YIELD_PRODUCTION)
				iYearJustify = 1<<2
			else:
				iYearJustify = 1<<1
				if not bHasYear:
					szYear = ""
				elif iYear < 0:
					szYear = TRNSLTR.getText("TXT_KEY_TIME_BC", (-iYear,))
				else:
					szYear = TRNSLTR.getText("TXT_KEY_TIME_AD", (iYear,))

			if iBuilder > -1:
				szBuilder = GC.getPlayer(iBuilder).getCivilizationShortDescription(0)
			else:
				szBuilder = TRNSLTR.getText("TXT_KEY_UNKNOWN", ())

			if iCityId > -1:
				szCityName = GC.getPlayer(iCityOwner).getCity(iCityId).getName()
			else:
				szCityName = ""

			if self.bWonderPlayerColor and iBuilder > -1:
				iColor = GC.getPlayer(iBuilder).getColorPrimary()
				szBuilder = TRNSLTR.changeTextColor(szBuilder, iColor)
				szCityName = TRNSLTR.changeTextColor(szCityName, iColor)

			screen.appendTableRow(self.szWondersTable)
			if bZoomable:
				screen.setTableText(self.szWondersTable, 0, i, "", self.zoomArt, WidgetTypes.WIDGET_ZOOM_CITY, iCityOwner, iCityId, 1<<0)
			screen.setTableText(self.szWondersTable, 1, i, INFO.getDescription(szPrefix, iType), "", iWidget, iType, -1, 1<<0)
			screen.setTableInt (self.szWondersTable, 2, i, szYear, "", WidgetTypes.WIDGET_GENERAL, -1, -1, iYearJustify)
			screen.setTableText(self.szWondersTable, 3, i, szBuilder, "", WidgetTypes.WIDGET_GENERAL, -1, -1, 1<<0)
			screen.setTableText(self.szWondersTable, 4, i, szCityName, "", WidgetTypes.WIDGET_GENERAL, -1, -1, 1<<0)

#############################################################################################################
################################################## STATISTICS ###############################################
#############################################################################################################

	def drawStatsTab(self, screen):

		# Bottom Chart
		if self.bShowImprovement:
			self.W_STATS_BOTTOM_CHART_UNITS = 455
			self.W_STATS_BOTTOM_CHART_BUILDINGS = 260
		else:
			self.W_STATS_BOTTOM_CHART_UNITS = 545
			self.W_STATS_BOTTOM_CHART_BUILDINGS = 390

################################################### CALCULATE STATS ###################################################

		iMinutesPlayed = GAME.getMinutesPlayed()
		iHoursPlayed = iMinutesPlayed / 60
		iMinutesPlayed = iMinutesPlayed - iHoursPlayed * 60

		szMinutesString = str(iMinutesPlayed)
		if iMinutesPlayed < 10:
			szMinutesString = "0" + szMinutesString
		szHoursString = str(iHoursPlayed)
		if iHoursPlayed < 10:
			szHoursString = "0" + szHoursString

		szTimeString = szHoursString + ":" + szMinutesString

		iNumCitiesBuilt = CyStatistics().getPlayerNumCitiesBuilt(self.iPlayer)

		iNumCitiesRazed = CyStatistics().getPlayerNumCitiesRazed(self.iPlayer)

		player = GC.getPlayer(self.iPlayer)
		iNumReligionsFounded = player.getNumReligionsFounded()
		aUnitRows = player.getUnitStatistics()
		aBuildingRows = player.getBuildingStatistics()
		aImprovementRows = player.getImprovementCounts()

################################################### TOP PANEL ###################################################

		eWidGen = WidgetTypes.WIDGET_GENERAL

		# Add Panel
		screen.addPanel(self.getNextWidgetName(), "", "", True, False, 45, 75, 935, 180, PanelStyles.PANEL_STYLE_DAWNTOP)

		# Leaderhead graphic
		screen.addLeaderheadGFC(self.getNextWidgetName(), player.getLeaderType(), AttitudeTypes.ATTITUDE_PLEASED, 250, 95, 110, 140, eWidGen, 1, 2)

		# Leader Name
		screen.setText(self.getNextWidgetName(), "", "<font=4b>" + player.getName(), 1<<0, 400, 90, 0, FontTypes.TITLE_FONT, eWidGen, 1, 2)

		# Create Table
		szTopChart = self.getNextWidgetName()
		screen.addTableControlGFC(szTopChart, 2, 400, 130, 380, 102, False, True, 32, 32, TableStyles.TABLE_STYLE_STANDARD)

		# Add Columns
		screen.setTableColumnHeader(szTopChart, 0, "", 304)
		screen.setTableColumnHeader(szTopChart, 1, "", 76)

		# Add Rows
		for i in xrange(4):
			screen.appendTableRow(szTopChart)

		# Graph itself
		screen.setTableText(szTopChart, 0, 0, self.TEXT_TIME_PLAYED, "", eWidGen, 1, 2, 1<<0)
		screen.setTableText(szTopChart, 1, 0, szTimeString, "", eWidGen, 1, 2, 1<<0)
		screen.setTableText(szTopChart, 0, 1, self.TEXT_CITIES_BUILT, "", eWidGen, 1, 2, 1<<0)
		screen.setTableText(szTopChart, 1, 1, str(iNumCitiesBuilt), "", eWidGen, 1, 2, 1<<0)
		screen.setTableText(szTopChart, 0, 2, self.TEXT_CITIES_RAZED, "", eWidGen, 1, 2, 1<<0)
		screen.setTableText(szTopChart, 1, 2, str(iNumCitiesRazed), "", eWidGen, 1, 2, 1<<0)
		screen.setTableText(szTopChart, 0, 3, self.TEXT_NUM_RELIGIONS_FOUNDED, "", eWidGen, 1, 2, 1<<0)
		screen.setTableText(szTopChart, 1, 3, str(iNumReligionsFounded), "", eWidGen, 1, 2, 1<<0)

################################################### BOTTOM PANEL ###################################################

		# Create Tables
		szUnitsTable = self.getNextWidgetName()
		screen.addTableControlGFC(szUnitsTable, 5, 45, 280, self.W_STATS_BOTTOM_CHART_UNITS, 410, True, True, 32, 32, TableStyles.TABLE_STYLE_STANDARD)
		screen.enableSort(szUnitsTable)

		szBuildingsTable = self.getNextWidgetName()
		screen.addTableControlGFC(szBuildingsTable, 2, 45 + self.W_STATS_BOTTOM_CHART_UNITS, 280, self.W_STATS_BOTTOM_CHART_BUILDINGS, 410, True, True, 32, 32, TableStyles.TABLE_STYLE_STANDARD)
		screen.enableSort(szBuildingsTable)

		# Reducing the width a bit to leave room for the vertical scrollbar, preventing a horizontal scrollbar from also being created
		if self.bShowImprovement:
			szImprovementsTable = self.getNextWidgetName()
			screen.addTableControlGFC(szImprovementsTable, 2, 45 + self.W_STATS_BOTTOM_CHART_UNITS + self.W_STATS_BOTTOM_CHART_BUILDINGS, 280, 220, 410, True, True, 32,32, TableStyles.TABLE_STYLE_STANDARD)
			screen.enableSort(szImprovementsTable)

			iChartWidth = self.W_STATS_BOTTOM_CHART_UNITS + self.W_STATS_BOTTOM_CHART_BUILDINGS + 196

			# Add Columns
			screen.setTableColumnHeader(szUnitsTable, 0, self.TEXT_UNITS, iChartWidth * 3 / 16)
			iColWidth = iChartWidth / 14
			screen.setTableColumnHeader(szUnitsTable, 1, self.TEXT_CURRENT, iColWidth)
			screen.setTableColumnHeader(szUnitsTable, 2, self.TEXT_BUILT, iColWidth)
			screen.setTableColumnHeader(szUnitsTable, 3, self.TEXT_KILLED, iColWidth)
			screen.setTableColumnHeader(szUnitsTable, 4, self.TEXT_LOST, iColWidth)
			screen.setTableColumnHeader(szBuildingsTable, 0, self.TEXT_BUILDINGS, iChartWidth * 3 / 16)
			screen.setTableColumnHeader(szBuildingsTable, 1, self.TEXT_BUILT, iColWidth)
			screen.setTableColumnHeader(szImprovementsTable, 0, self.TEXT_IMPROVEMENTS, iChartWidth / 7)
			screen.setTableColumnHeader(szImprovementsTable, 1, self.TEXT_CURRENT, iColWidth)
		else:
			iChartWidth = self.W_STATS_BOTTOM_CHART_UNITS + self.W_STATS_BOTTOM_CHART_BUILDINGS - 24

			# Add Columns
			screen.setTableColumnHeader(szUnitsTable, 0, self.TEXT_UNITS, iChartWidth / 4)
			iColWidth = iChartWidth / 12
			screen.setTableColumnHeader(szUnitsTable, 1, self.TEXT_CURRENT, iColWidth)
			screen.setTableColumnHeader(szUnitsTable, 2, self.TEXT_BUILT, iColWidth)
			screen.setTableColumnHeader(szUnitsTable, 3, self.TEXT_KILLED, iColWidth)
			screen.setTableColumnHeader(szUnitsTable, 4, self.TEXT_LOST, iColWidth)
			screen.setTableColumnHeader(szBuildingsTable, 0, self.TEXT_BUILDINGS, iChartWidth / 3)
			screen.setTableColumnHeader(szBuildingsTable, 1, self.TEXT_BUILT, iColWidth)


		for iRow, (iUnit, iCurrent, iBuilt, iKilled, iLost) in enumerate(aUnitRows):
			screen.appendTableRow(szUnitsTable)
			screen.setTableText(szUnitsTable, 0, iRow, INFO.getDescription("UNIT_", iUnit), "", eWidGen, 1, 2, 1<<0)
			screen.setTableInt(szUnitsTable, 1, iRow, str(iCurrent), "", eWidGen, 1, 2, 1<<0)
			screen.setTableInt(szUnitsTable, 2, iRow, str(iBuilt), "", eWidGen, 1, 2, 1<<0)
			screen.setTableInt(szUnitsTable, 3, iRow, str(iKilled), "", eWidGen, 1, 2, 1<<0)
			screen.setTableInt(szUnitsTable, 4, iRow, str(iLost), "", eWidGen, 1, 2, 1<<0)

		for iRow, (iBuilding, iBuilt) in enumerate(aBuildingRows):
			screen.appendTableRow(szBuildingsTable)
			screen.setTableText(szBuildingsTable, 0, iRow, INFO.getDescription("BUILDING_", iBuilding), "", eWidGen, 1, 2, 1<<0)
			screen.setTableInt(szBuildingsTable, 1, iRow, str(iBuilt), "", eWidGen, 1, 2, 1<<0)

		if self.bShowImprovement:
			for iRow, (iImprovement, iCount) in enumerate(aImprovementRows):
				screen.appendTableRow(szImprovementsTable)
				screen.setTableText(szImprovementsTable, 0, iRow, INFO.getDescription("IMPROVEMENT_", iImprovement), "", eWidGen, 1, 2, 1<<0)
				screen.setTableInt(szImprovementsTable, 1, iRow, str(iCount), "", eWidGen, 1, 2, 1<<0)


#############################################################################################################
##################################################### OTHER #################################################
#############################################################################################################


	def determineKnownPlayers(self):

		self.demographicsTable = ""
		self.iTeam = GC.getPlayer(self.iPlayer).getTeam()
		# The civilizations the viewed player knows of, split by whether it may see their statistics.
		self.aiPlayersMet = []
		self.aiPlayersMetNAEspionage = []
		for iPlayerX in xrange(GC.getMAX_PC_PLAYERS()):
			playerX = GC.getPlayer(iPlayerX)
			if not playerX.isKnownTo(self.iPlayer):
				continue
			if playerX.isDemographicsVisibleTo(self.iPlayer):
				self.aiPlayersMet.append(iPlayerX)
			else:
				self.aiPlayersMetNAEspionage.append(iPlayerX)


	def getLog10(self, x):
		return math.log10(max(1, x))

	def getNextLineName(self):
		name = "InfoGraphLine" + str(self.nLineCount)
		self.nLineCount += 1
		return name

	# returns a unique ID for a widget in this screen
	def getNextWidgetName(self):
		szName = "InfoScreenWidget" + str(self.nWidgetCount)
		self.nWidgetCount += 1
		return szName

	def deleteAllWidgets(self, screen, iNumPermanentWidgets = 0):
		i = self.nWidgetCount - 1
		while i >= iNumPermanentWidgets:
			self.nWidgetCount = i
			screen.deleteWidget(self.getNextWidgetName())
			i -= 1

		self.nWidgetCount = iNumPermanentWidgets
		self.yMessage = 5


	def getScreen(self):
		return CyGInterfaceScreen("InfoScreen", self.screenId)

	# handle the input for this screen...
	def handleInput(self, inputClass):

		screen = CyGInterfaceScreen("InfoScreen", self.screenId)

		iCode	= inputClass.eNotifyCode
		ID		= inputClass.iItemID
		NAME	= inputClass.szFunctionName

		szWidgetName = NAME + str(ID)

		if iCode == 11: # List Select

			# Debug dropdown
			if NAME == "Info_DebugDD":
				iIndex = screen.getSelectedPullDownID("Info_DebugDD")
				self.iPlayer = screen.getPullDownData("Info_DebugDD", iIndex)
				# The demographics table is kept between draws; it belongs to the player viewed before.
				if self.demographicsTable:
					screen.deleteWidget(self.demographicsTable)
				self.determineKnownPlayers()
				self.redrawContents()

			iSelected = inputClass.getData()

			if self.iTab == 1:

				# Graph dropdown to select what values are being graphed
				if szWidgetName == self.szTurnsDropdownWidget:

					self.iGraphZoom = self.dropDownTurns[iSelected]
					self.checkGraphBounds()

					self.drawGraphs()

				elif szWidgetName == self.szGraphSmoothingDropdownWidget:
					self.iGraph_Smoothing = iSelected * 2
					self.drawGraphs()


				if NAME == "GraphDD":
					if ID:
						self.iGraph2 = iSelected
					else: self.iGraph1 = iSelected
					self.drawGraphs()

			elif self.iTab == 2:

				if szWidgetName == self.szWondersTable and not inputClass.getMouseX():
					screen.hideScreen()
					#	The widget carries the (owner, id) pair the table row was built with.
					aCityPos = GC.getPlayer(inputClass.getData1()).getCity(inputClass.getData2()).getPosition()
					CyCamera().JustLookAtPlot(CyMap().plot(aCityPos[0], aCityPos[1]))

		# Something Clicked
		elif iCode == NotifyCode.NOTIFY_CLICKED:

			if NAME == "VS_Tab":
				screen.hide("VS_Tab_Act" + str(self.iTab))
				screen.show("VS_Tab" + str(self.iTab))
				if not self.iTab:
					screen.hide(self.demographicsTable)
				elif self.iTab == 1:
					screen.hide("Graph_BG")

				self.iTab = ID
				self.redrawContents()

			elif NAME == "GraphShiftL":
				self.slideGraph(- 2 * self.iGraphZoom / 5)
				self.drawGraphs()

			elif NAME == "GraphShiftR":
				self.slideGraph(2 * self.iGraphZoom / 5)
				self.drawGraphs()

			elif NAME in ("WorldWondersWidget", "NationalWondersWidget", "ProjectsWidget"):

				if NAME == "WorldWondersWidget":
					self.szWonderDisplayMode = "WorldWonders"

				elif NAME == "NationalWondersWidget":
					self.szWonderDisplayMode = "NationalWonders"

				elif NAME == "ProjectsWidget":
					self.szWonderDisplayMode = "Projects"

				self.redrawContents()

			if self.iTab == 1:
				for i in xrange(self.iNumGraphs):
					if szWidgetName == self.sGraphTextHeadingWidget[i] or NAME == "GraphArea" and ID == i:
						self.bDoubleGraph = not self.bDoubleGraph
						if self.iGraph1 != i:
							self.iGraph2 = self.iGraph1
							self.iGraph1 = i
						self.drawGraphs()
						break

					elif szWidgetName == self.sGraphTextBannerWidget[i]:
						if self.iGraph1 != i:
							self.iGraph1 = i
							self.drawGraphs()
							break

				for i in xrange(GC.getMAX_PC_PLAYERS()):
					if szWidgetName == self.sPlayerTextWidget[i]:
						self.bPlayerInclude[i] = not self.bPlayerInclude[i]
						self.drawGraphs()
						break

				if szWidgetName == self.sShowAllWidget:
					for i in xrange(GC.getMAX_PC_PLAYERS()):
						self.bPlayerInclude[i] = True
					self.drawGraphs()

				if szWidgetName == self.sShowNoneWidget:
					for i in xrange(GC.getMAX_PC_PLAYERS()):
						self.bPlayerInclude[i] = False
					self.drawGraphs()

		return 0


	def update(self, fDelta):
		PythonToolTip.pollSilentArea()

	def silentAreaRects(self):
		if not self.getScreen().isActive():
			return []
		return self.wondersRects

	def onClose(self):
		CyGInterfaceScreen("InfoScreen", self.screenId).setDying(True)
		del self.nWidgetCount, self.aWidgetBucket, self.iTurn, self.iStartTurn, \
			self.xRes, self.yRes, self.aFontList, self.H_PAGE, \
			self.demographicsTable, self.iNumGraphs
