# CvVictoryScreen
from CvPythonExtensions import *
import AttitudeUtil
import PythonToolTip

# globals
# The one data-fetching library: INFO = what an entity CARRIES, ENABLER = can I? A game object's own data is
# asked OF THAT OBJECT --
# GC.getPlayer(i).getCity(id).getYields(), never a flat class keyed by (owner, id).
GC = CyGlobalContext()
INFO = CyInfo()
VICTORY = CyVictoryInfo()   # the per-info accessor: the `condition` block and the buildings and projects needed
GAME = GC.getGame()
ENABLER = CyEnabler()
TRNSLTR = CyTranslator()


class CvVictoryScreen:
	"Keeps track of victory conditions"

	def __init__(self, screenId):
		self.screenId = screenId
		# The members table's attitude columns, a silent area (§ THE ONE HOVER, Hover state).
		self.membersRects = []

		self.VoteType = 1  # 1 = Pope or GenSec, 2 = Diplomatic Victory
		self.VoteBody = 1  # 1 = AP, 2 = UN

		self.nWidgetCount = 0

		self.iTab = 0

	def hasAllTechs(self, CyTeamX, lTechs):
		"""Does this team hold EVERY tech in the list?

		A project's tech gate is a list of mandatory requirements now rather than one prereq id, so "has the
		gate" means holding all of them. An empty list is ungated and answers True.
		"""
		for iTech in lTechs:
			if not CyTeamX.isHasTech(iTech):
				return False
		return True

	def canResearchAll(self, iPlayer, lTechs):
		"""Is every tech in the list either held or on this player's research frontier?"""
		for iTech in lTechs:
			if ENABLER.getTechAvailability(iPlayer, iTech) != EnablerState.ENABLER_LISTED:
				return False
		return True

	def getScreen(self):
		return CyGInterfaceScreen("VictoryScreen", self.screenId)

	def interfaceScreen(self):
		PythonToolTip.noteSilentArea(self)

		# Create a new screen
		screen = self.getScreen()
		if screen.isActive():
			return
		# Get screen resolution.
		self.xRes = xRes = screen.getXResolution()
		self.yRes = yRes = screen.getYResolution()
		# Calibrate variables.
		if yRes > 1000:
			H_BOT_ROW = 110
			self.H_EDGE_PANEL = H_EDGE_PANEL = 38
			szfontEdge = "<font=4b>"
		elif yRes > 800:
			H_BOT_ROW = 92
			self.H_EDGE_PANEL = H_EDGE_PANEL = 32
			szfontEdge = "<font=3b>"
		else:
			H_BOT_ROW = 78
			self.H_EDGE_PANEL = H_EDGE_PANEL = 29
			szfontEdge = "<font=2b>"

		if xRes > 1700:
			self.aFontList = aFontList = [szfontEdge, "<font=4b>", "<font=4>", "<font=3b>", "<font=3>", "<font=2b>", "<font=2>", "<font=1>"]
		elif xRes > 1400:
			self.aFontList = aFontList = [szfontEdge, "<font=3b>", "<font=3>", "<font=2b>", "<font=2>", "<font=1b>", "<font=1>", "<font=0>"]
		else:
			self.aFontList = aFontList = [szfontEdge, "<font=2b>", "<font=2>", "<font=1b>", "<font=1>", "<font=0b>", "<font=0>", "<font=0>"]

		self.Y_TOP_PAGE = Y_TOP_PAGE = H_EDGE_PANEL - 6
		self.H_PAGE = H_PAGE = yRes - Y_TOP_PAGE - H_EDGE_PANEL + 8
		self.Y_BOT_TEXT = Y_BOT_TEXT = yRes - H_EDGE_PANEL + 8
		# Declare misc variables
		self.iActivePlayer = GAME.getActivePlayer()
		eWidGen = WidgetTypes.WIDGET_GENERAL
		eFontTitle = FontTypes.TITLE_FONT

		# Build Victory screen.
		screen.setRenderInterfaceOnly(True)
		screen.showScreen(PopupStates.POPUPSTATE_IMMEDIATE, False)

		screen.addDDSGFC("VS_Background", CyArtFileMgr().getInterfaceArtInfo("MAINMENU_SLIDESHOW_LOAD").getPath(), 0, 0, xRes, yRes, eWidGen, 1, 2)
		screen.addPanel("VS_TopPanel", "", "", True, False, 0, 0, xRes, H_EDGE_PANEL, PanelStyles.PANEL_STYLE_TOPBAR)
		screen.addPanel("VS_BotPanel", "", "", True, False, 0, yRes - H_EDGE_PANEL, xRes, H_EDGE_PANEL, PanelStyles.PANEL_STYLE_BOTTOMBAR)
		screen.setLabel("VS_Header", "", szfontEdge + TRNSLTR.getText("TXT_WORD_GAME",()), 1<<2, xRes / 2, 2, 0, eFontTitle, eWidGen, 1, 2)
		screen.setText("VS_Btn_Exit", "", szfontEdge + TRNSLTR.getText("TXT_WORD_EXIT",()), 1<<1, xRes - 16, 0, 0, eFontTitle, WidgetTypes.WIDGET_CLOSE_SCREEN, -1, -1)

		self.COLOR_GREEN = GC.getInfoTypeForString("COLOR_GREEN")
		self.COLOR_YELLOW = iCol = GC.getInfoTypeForString("COLOR_YELLOW")
		szTxt = szfontEdge + TRNSLTR.getText("TXT_KEY_MAIN_MENU_VICTORIES", ())
		szTxtCol = TRNSLTR.changeTextColor(szTxt, iCol)
		dX = xRes / 4
		x = dX / 2
		screen.setText("VS_Tab0", "", szTxt, 1<<2, x, Y_BOT_TEXT, 0, eFontTitle, eWidGen, 1, 2)
		screen.setText("VS_Col_Tab0", "", szTxtCol, 1<<2, x, Y_BOT_TEXT, 0, eFontTitle, eWidGen, 1, 2)
		screen.hide("VS_Col_Tab0")

		szTxt = szfontEdge + TRNSLTR.getText("TXT_KEY_MAIN_MENU_SETTINGS", ())
		szTxtCol = TRNSLTR.changeTextColor(szTxt, iCol)
		x += dX
		screen.setText("VS_Tab1", "", szTxt, 1<<2, x, Y_BOT_TEXT, 0, eFontTitle, eWidGen, 1, 2)
		screen.setText("VS_Col_Tab1", "", szTxtCol, 1<<2, x, Y_BOT_TEXT, 0, eFontTitle, eWidGen, 1, 2)
		screen.hide("VS_Col_Tab1")

		szTxt = szfontEdge + TRNSLTR.getText("TXT_KEY_VOTING_TITLE", ())
		szTxtCol = TRNSLTR.changeTextColor(szTxt, iCol)
		x += dX
		screen.setText("VS_Tab2", "", szTxt, 1<<2, x, Y_BOT_TEXT, 0, eFontTitle, eWidGen, 1, 2)
		screen.setText("VS_Col_Tab2", "", szTxtCol, 1<<2, x, Y_BOT_TEXT, 0, eFontTitle, eWidGen, 1, 2)
		screen.hide("VS_Col_Tab2")

		szTxt = szfontEdge + TRNSLTR.getText("TXT_KEY_MEMBERS_TITLE", ())
		szTxtCol = TRNSLTR.changeTextColor(szTxt, iCol)
		x += dX
		screen.setText("VS_Tab3", "", szTxt, 1<<2, x, Y_BOT_TEXT, 0, eFontTitle, eWidGen, 1, 2)
		screen.setText("VS_Col_Tab3", "", szTxtCol, 1<<2, x, Y_BOT_TEXT, 0, eFontTitle, eWidGen, 1, 2)
		screen.hide("VS_Col_Tab3")

		if self.iTab == 0:
			screen.hide("VS_Tab0")
			screen.show("VS_Col_Tab0")
			self.showVictoryConditionScreen(screen)
		elif self.iTab == 1:
			screen.hide("VS_Tab1")
			screen.show("VS_Col_Tab1")
			self.showGameSettingsScreen(screen)
		elif self.iTab == 2:
			screen.hide("VS_Tab2")
			screen.show("VS_Col_Tab2")
			self.showVotingScreen(screen)
		elif self.iTab == 3:
			screen.hide("VS_Tab3")
			screen.show("VS_Col_Tab3")
			self.showMembersScreen(screen)


	def showVictoryConditionScreen(self, screen):
		ufontEdge, ufont4b, ufont4, ufont3b, ufont3, ufont2b, ufont2, ufont1 = self.aFontList
		iActivePlayer = self.iActivePlayer
		CyPlayer = GC.getPlayer(iActivePlayer)
		iTeamAct = CyPlayer.getTeam()
		CyTeam = GC.getTeam(iTeamAct)

		self.deleteAllWidgets()
		eWidGen = WidgetTypes.WIDGET_GENERAL
		xRes = self.xRes
		screen.addPanel(self.getNextWidgetName(), "", "", False, False, 0, self.Y_TOP_PAGE - 6, xRes, self.H_PAGE + 6, PanelStyles.PANEL_STYLE_BLUE50)

		# civ picker dropdown
		import DebugUtils
		if DebugUtils.isAnyDebugMode():
			screen.addDropDownBoxGFC("VictoryScreenDropdownWidget", 22, 0, 300, eWidGen, 1, 2, FontTypes.GAME_FONT)
			for j in xrange(GC.getMAX_PLAYERS()):
				if GC.getPlayer(j).isAlive():
					screen.addPullDownString("VictoryScreenDropdownWidget", GC.getPlayer(j).getName(), j, j, False)

		iTurn = GAME.getElapsedGameTurns()
		iMaxTurns = GAME.getMaxTurns()
		dx = xRes - 24
		a18thX = dx / 18
		szTable = self.getNextWidgetName()
		# Mastery
		for iLoopVC in xrange(GC.getNumVictoryInfos()):
			if VICTORY.conditionFlag(iLoopVC, VictoryConditionFlag.VICTORY_CONDITION_TOTAL_VICTORY):
				if GAME.isVictoryValid(iLoopVC):

					iBestTeam = CyTeam.getBestVictoryScoreRival()
					if iBestTeam != -1 and GC.getTeam(iBestTeam).isKnownTo(iActivePlayer):
						CyTeamBest = GC.getTeam(iBestTeam)
						aRivalParts = CyTeamBest.getVictoryScoreParts()
						szUnknown = ""
					else:
						CyTeamBest = None
						szUnknown = TRNSLTR.getText("TXT_KEY_VICTORY_SCREEN_UNKNOWN", ())
					aOurParts = CyTeam.getVictoryScoreParts()
					# Start filling in the table below
					screen.addTableControlGFC(szTable, 4, 12, self.Y_TOP_PAGE + 12, dx, self.H_PAGE - 24, False, True, 32,32, TableStyles.TABLE_STYLE_STANDARD)
					iTemp1 = 6 * a18thX
					iTemp2 = 3 * a18thX
					iTemp3 = (dx - 2 * iTemp1 - 2 * iTemp2) / 4
					iTemp1 += iTemp3
					iTemp2 += iTemp3
					screen.setTableColumnHeader(szTable, 0, "", iTemp1)
					screen.setTableColumnHeader(szTable, 1, "", iTemp2)
					screen.setTableColumnHeader(szTable, 2, "", iTemp2)
					screen.setTableColumnHeader(szTable, 3, "", iTemp1)
					del xRes, dx, a18thX, iTemp1, iTemp2, iTemp3

					# Headings
					szVictoryType = ufont3b + INFO.getDescription("VICTORY_", iLoopVC)
					if iMaxTurns > iTurn:
						szVictoryType += ufont2 + "\t\t(" + TRNSLTR.getText("TXT_KEY_MISC_TURNS_LEFT", (iMaxTurns - iTurn,)) + ")"

					iRow = screen.appendTableRow(szTable)
					screen.setTableText(szTable, 0, iRow, szVictoryType, "", eWidGen, 1, 2, 1<<0)
					screen.setTableText(szTable, 1, iRow, ufont3b + CyPlayer.getName(), "", eWidGen, 1, 2, 1<<2)
					if szUnknown:
						screen.setTableText(szTable, 3, iRow, ufont3b + szUnknown, "", eWidGen, 1, 2, 1<<0)
					else:
						screen.setTableText(szTable, 3, iRow, ufont3b + CyTeamBest.getName(), "", eWidGen, 1, 2, 1<<0)

					iRow = screen.appendTableRow(szTable)
					screen.setTableText(szTable, 0, iRow, ufont2b + TRNSLTR.getText("TXT_KEY_VICTORY_SCREEN_CONDITION", ()), "", eWidGen, 1, 2, 1<<0)
					screen.setTableText(szTable, 1, iRow, ufont2b + TRNSLTR.getText("TXT_KEY_VICTORY_SCREEN_PERCENTAGE", ()), "", eWidGen, 1, 2, 1<<0)
					screen.setTableText(szTable, 2, iRow, ufont2b + TRNSLTR.getText("TXT_KEY_VICTORY_SCREEN_POINTS", ()), "", eWidGen, 1, 2, 1<<0)
					screen.setTableText(szTable, 3, iRow, ufont2b + "(" + TRNSLTR.getText("TXT_KEY_VICTORY_SCREEN_RIVAL_CIV", ()) + ")", "", eWidGen, 1, 2, 1<<0)

					screen.appendTableRow(szTable)

					# Population, land, culture
					for iPart, szLabel in (
						(VictoryScorePartKind.VICTORY_SCORE_POPULATION, "TXT_KEY_POPULATION"),
						(VictoryScorePartKind.VICTORY_SCORE_LAND, "TXT_KEY_LAND"),
						(VictoryScorePartKind.VICTORY_SCORE_CULTURE, "TXT_WORD_CULTURE")
					):
						iRow = screen.appendTableRow(szTable)
						screen.setTableText(szTable, 0, iRow, ufont2 + TRNSLTR.getText(szLabel, ()), "", eWidGen, 1, 2, 1<<0)
						screen.setTableText(szTable, 1, iRow, ufont2 + (u"%i%%" % aOurParts[iPart][3]), "", eWidGen, 1, 2, 1<<0)
						screen.setTableText(szTable, 2, iRow, ufont2 + (u"%i" % aOurParts[iPart][3]), "", eWidGen, 1, 2, 1<<0)
						if szUnknown:
							screen.setTableText(szTable, 3, iRow, ufont2 + szUnknown, "", eWidGen, 1, 2, 1<<0)
						else:
							screen.setTableText(szTable, 3, iRow, ufont2 + (u"%i%%" % aRivalParts[iPart][3]), "", eWidGen, 1, 2, 1<<0)

					# Power
					iPart = VictoryScorePartKind.VICTORY_SCORE_POWER
					iRow = screen.appendTableRow(szTable)
					screen.setTableText(szTable, 0, iRow, ufont2 + TRNSLTR.getText("TXT_KEY_POWER", ()), "", eWidGen, 1, 2, 1<<0)
					screen.setTableText(szTable, 1, iRow, ufont2 + u"%i" % aOurParts[iPart][1], "", eWidGen, 1, 2, 1<<0)
					screen.setTableText(szTable, 2, iRow, ufont2 + u"%i" % aOurParts[iPart][3], "", eWidGen, 1, 2, 1<<0)
					if szUnknown:
						screen.setTableText(szTable, 3, iRow, ufont2 + szUnknown, "", eWidGen, 1, 2, 1<<0)
					else:
						screen.setTableText(szTable, 3, iRow, ufont2 + u"%i" % aRivalParts[iPart][1], "", eWidGen, 1, 2, 1<<0)

					# Wonders
					iPart = VictoryScorePartKind.VICTORY_SCORE_WONDERS
					iRow = screen.appendTableRow(szTable)
					screen.setTableText(szTable, 0, iRow, ufont2 + TRNSLTR.getText("TXT_KEY_CONCEPT_WONDERS", ()), "", eWidGen, 1, 2, 1<<0)
					screen.setTableText(szTable, 1, iRow, ufont2 + u"%i  (%i of %i built)" %(aOurParts[iPart][1], CyPlayer.getSevoWondersScore(1), aOurParts[iPart][2]), "", eWidGen, 1, 2, 1<<0)
					screen.setTableText(szTable, 2, iRow, ufont2 + u"%i" % aOurParts[iPart][3], "", eWidGen, 1, 2, 1<<0)
					if szUnknown:
						screen.setTableText(szTable, 3, iRow, ufont2 + szUnknown, "", eWidGen, 1, 2, 1<<0)
					else:
						screen.setTableText(szTable, 3, iRow, ufont2 + u"%i" % aRivalParts[iPart][1], "", eWidGen, 1, 2, 1<<0)

					# Religion: the holy cities the team holds
					iRow = screen.appendTableRow(szTable)
					iRefRow = iRow
					screen.setTableText(szTable, 0, iRow, ufont2 + TRNSLTR.getText("TXT_KEY_CONCEPT_RELIGION",()), "", eWidGen, 1, 2, 1<<0)

					aReligions = CyTeam.getHolyCityReligions()
					if aReligions:
						for iReligion, iPercent, bScoring in aReligions:
							iRow = screen.appendTableRow(szTable)
							screen.setTableText(szTable, 0, iRow, ufont2 + INFO.getDescription("RELIGION_", iReligion), "", eWidGen, 1, 2, 1<<1)
							screen.setTableText(szTable, 1, iRow, ufont2 + (u"%d%%" % iPercent), "", eWidGen, 1, 2, 1<<0)
							if bScoring:
								screen.setTableText(szTable, 2, iRow, ufont2 + (u"%d" % aOurParts[VictoryScorePartKind.VICTORY_SCORE_RELIGION][3]), "", eWidGen, 1, 2, 1<<0)
					else:
						iRow = screen.appendTableRow(szTable)
						screen.setTableText(szTable, 1, iRow, ufont2 + TRNSLTR.getText("TXT_KEY_VICTORY_SCREEN_NO_HOLY", ()), "", eWidGen, 1, 2, 1<<0)

					if szUnknown:
						screen.setTableText(szTable, 3, iRow, ufont2 + szUnknown, "", eWidGen, 1, 2, 1<<0)
					else:
						aRivalReligions = CyTeamBest.getHolyCityReligions()
						for iReligion, iPercent, bScoring in aRivalReligions:
							iRefRow += 1
							if iRefRow > iRow:
								iRow = screen.appendTableRow(szTable)
							screen.setTableText(szTable, 3, iRefRow, ufont2 + (u"%s: %i%%" %(INFO.getDescription("RELIGION_", iReligion), iPercent)), "", eWidGen, 1, 2, 1<<0)
						if not aRivalReligions:
							iRefRow += 1
							if iRefRow > iRow:
								iRow = screen.appendTableRow(szTable)
							screen.setTableText(szTable, 3, iRefRow, ufont2 + TRNSLTR.getText("TXT_KEY_VICTORY_SCREEN_NO_HOLY", ()), "", eWidGen, 1, 2, 1<<0)

					# Monumental cities: the team's cities with the most culture, its monumental ones (green) and three more
					iPart = VictoryScorePartKind.VICTORY_SCORE_MONUMENTAL_CITIES
					iRow = screen.appendTableRow(szTable)
					iRefRow = iRow
					screen.setTableText(szTable, 0, iRow, ufont2 + TRNSLTR.getText("TXT_KEY_VICTORY_SCREEN_CITIES", ()), "", eWidGen, 1, 2, 1<<0)
					screen.setTableText(szTable, 2, iRow, ufont2 + u"%i" % aOurParts[iPart][3], "", eWidGen, 1, 2, 1<<0)

					for iOwner, iCity, iCulture, bMonumental in CyTeam.getCitiesByCulture(aOurParts[iPart][1] + 3):
						iRow = screen.appendTableRow(szTable)
						szCulture = str(iCulture)
						if bMonumental:
							szCulture = TRNSLTR.changeTextColor(szCulture, self.COLOR_GREEN)
						screen.setTableText(szTable, 0, iRow, ufont2 + GC.getPlayer(iOwner).getCity(iCity).getName() + ":", "", eWidGen, 1, 2, 1<<1)
						screen.setTableText(szTable, 1, iRow, ufont2 + szCulture, "", eWidGen, 1, 2, 1<<0)

					if szUnknown:
						screen.setTableText(szTable, 3, iRow, ufont2 + szUnknown, "", eWidGen, 1, 2, 1<<0)
					else:
						for iOwner, iCity, iCulture, bMonumental in CyTeamBest.getCitiesByCulture(aRivalParts[iPart][1] + 3):
							iRefRow += 1
							if iRefRow > iRow:
								iRow = screen.appendTableRow(szTable)
							screen.setTableText(szTable, 3, iRefRow, ufont2 + u"%s: %i" %(GC.getPlayer(iOwner).getCity(iCity).getName(), iCulture), "", eWidGen, 1, 2, 1<<0)

					screen.appendTableRow(szTable)

					# Starship Launch Data
					iPart = VictoryScorePartKind.VICTORY_SCORE_STARSHIP
					iRow = screen.appendTableRow(szTable)
					screen.setTableText(szTable, 0, iRow, ufont2 + TRNSLTR.getText("TXT_KEY_CONCEPT_SPACESHIP", ()), "", eWidGen, 1, 2, 1<<0)

					if aOurParts[iPart][1]:
						screen.setTableText(szTable, 1, iRow, ufont2 + TRNSLTR.getText("TXT_KEY_VICTORY_SCREEN_LAUNCHED", ()), "", eWidGen, 1, 2, 1<<0)
						screen.setTableText(szTable, 2, iRow, ufont2 + str(aOurParts[iPart][3]), "", eWidGen, 1, 2, 1<<0)
					else:
						screen.setTableText(szTable, 1, iRow, ufont2 + TRNSLTR.getText("TXT_KEY_VICTORY_SCREEN_NOTLAUNCHED", ()), "", eWidGen, 1, 2, 1<<0)

					if CyTeamBest and aRivalParts[iPart][1]:
						screen.setTableText(szTable, 3, iRow, ufont2 + u"%s" %(TRNSLTR.getText("TXT_KEY_VICTORY_SCREEN_LAUNCHED", ())), "", eWidGen, 1, 2, 1<<0)
					elif CyTeamBest:
						screen.setTableText(szTable, 3, iRow, ufont2 + TRNSLTR.getText("TXT_KEY_VICTORY_SCREEN_NOTLAUNCHED", ()), "", eWidGen, 1, 2, 1<<0)

					# View spaceship button
					iRow = screen.appendTableRow(szTable)
					screen.setButtonGFC("SpaceShipButton" + str(iLoopVC), TRNSLTR.getText("TXT_KEY_GLOBELAYER_STRATEGY_VIEW", ()), "", 0, 0, 15, 10, eWidGen, 1234, -1, ButtonStyles.BUTTON_STYLE_STANDARD )
					screen.attachControlToTableCell("SpaceShipButton" + str(iLoopVC), szTable, iRow, 1)

					if not aOurParts[iPart][1]:
						for iVictory in xrange(GC.getNumVictoryInfos()):
							if not CyTeam.hasVictoryNeededProjects(iVictory):
								continue
							for iProject in VICTORY.getProjects(iVictory):
								if INFO.isProjectSpaceship(iProject):
									iRow = screen.appendTableRow(szTable)
									screen.setTableText(szTable, 0, iRow, ufont2 + TRNSLTR.getText("TXT_KEY_VICTORY_SCREEN_BUILDING", (INFO.getProjectVictoryThreshold(iProject, iVictory), INFO.getTextKey("PROJECT_", iProject))), "", eWidGen, 1, 2, 1<<1)
									screen.setTableText(szTable, 1, iRow, ufont2 + str(CyTeam.getProjectCount(iProject)), "", eWidGen, 1, 2, 1<<0)

					screen.appendTableRow(szTable)

					# Score Totals
					iRow = screen.appendTableRow(szTable)
					screen.setTableText(szTable, 0, iRow, ufont3b + TRNSLTR.getText("TXT_KEY_VICTORY_SCREEN_TOTALS", ()) + "</font>", "", eWidGen, 1, 2, 1<<0)
					screen.setTableText(szTable, 2, iRow, ufont3b + "%i" % CyTeam.getTotalVictoryScore(), "", eWidGen, 1, 2, 1<<0)


					if CyTeamBest:
						screen.setTableText(szTable, 3, iRow, ufont3b + "%i" % CyTeamBest.getTotalVictoryScore(), "", eWidGen, 1, 2, 1<<0)

					screen.appendTableRow(szTable)
					screen.appendTableRow(szTable)

					# The rest of the scores
					iRow = screen.appendTableRow(szTable)
					screen.setTableText(szTable, 0, iRow, ufont3b + TRNSLTR.getText("TXT_KEY_VICTORY_SCREEN_OTHER_CIV_SCORE",()), "", eWidGen, 1, 2, 1<<0)
					iRow = screen.appendTableRow(szTable)
					for iTeamX in xrange(GC.getMAX_PC_TEAMS()):
						if iTeamX == iTeamAct or iTeamX == iBestTeam: continue
						CyTeamX = GC.getTeam(iTeamX)
						if CyTeamX.isAlive() and not CyTeamX.isMinorCiv():
							if CyTeamX.isKnownTo(iActivePlayer):
								iRow = screen.appendTableRow(szTable)
								screen.setTableText(szTable, 0, iRow, ufont2b + CyTeamX.getName() + ":", "", eWidGen, 1, 2, 1<<0)
								screen.setTableText(szTable, 1, iRow, ufont2b + str(CyTeamX.getTotalVictoryScore()), "", eWidGen, 1, 2, 1<<0)

					return

		# Non-mastery victory conditions
		iTemp1 = 6 * a18thX
		iTemp2 = 3 * a18thX
		iTemp3 = (dx - 2 * iTemp1 - 2 * iTemp2) / 4
		iTemp1 += iTemp3
		iTemp2 += iTemp3
		screen.addTableControlGFC(szTable, 4, 12, self.Y_TOP_PAGE + 12, dx, self.H_PAGE - 24, False, True, 32,32, TableStyles.TABLE_STYLE_STANDARD)
		screen.setTableColumnHeader(szTable, 0, "", iTemp1)
		screen.setTableColumnHeader(szTable, 1, "", iTemp1)
		screen.setTableColumnHeader(szTable, 2, "", iTemp2)
		screen.setTableColumnHeader(szTable, 3, "", iTemp2)
		iRow = screen.appendTableRow(szTable)
		del xRes, dx, a18thX, iTemp1, iTemp2, iTemp3

		szText = CyTeam.getName() + " - " + TRNSLTR.getText("TXT_WORD_TEAM", ()) + " %i" % iTeamAct
		screen.setTableText(szTable, 1, iRow, ufont3b + szText, "", eWidGen, 1, 2, 1<<2)
		szText = TRNSLTR.getText("TXT_KEY_VICTORY_SCREEN_RIVAL_CIV", ())
		screen.setTableText(szTable, 2, iRow, ufont3b + szText, "", eWidGen, 1, 2, 1<<2)

		szUnknown = TRNSLTR.getText("TXT_KEY_VICTORY_SCREEN_UNKNOWN", ())
		szTeamName = CyTeam.getName()
		for iLoopVC in xrange(GC.getNumVictoryInfos()):
			if VICTORY.conditionFlag(iLoopVC, VictoryConditionFlag.VICTORY_CONDITION_TARGET_SCORE): continue
			if GAME.isVictoryValid(iLoopVC):
				if iRow:
					screen.appendTableRow(szTable)
				screen.appendTableRow(szTable)

				iRow = screen.appendTableRow(szTable)

				szText = ufont3b + INFO.getDescription("VICTORY_", iLoopVC)

				iCategoryRow = iRow
				screen.setTableText(szTable, 0, iRow, szText, "", eWidGen, 1, 2, 1<<1)

				if VICTORY.conditionFlag(iLoopVC, VictoryConditionFlag.VICTORY_CONDITION_END_SCORE):
					if iMaxTurns > iTurn:
						szText = ufont2 + TRNSLTR.getText("TXT_KEY_MISC_TURNS_LEFT", (iMaxTurns - iTurn,))
						screen.setTableText(szTable, 1, iRow, szText, "", eWidGen, 1, 2, 1<<2)

					iRow = screen.appendTableRow(szTable)
					szText = TRNSLTR.getText("TXT_KEY_VICTORY_SCREEN_HIGHEST_SCORE", (CyGameTextMgr().getTimeStr(GAME.getStartTurn() + iMaxTurns, False), ))
					screen.setTableText(szTable, 0, iRow, ufont2 + szText, "", eWidGen, 1, 2, 1<<1)
					screen.setTableText(szTable, 1, iRow, ufont2 + (u"%d" % GAME.getTeamScore(iTeamAct)), "", eWidGen, 1, 2, 1<<2)

					iRival = CyTeam.getBestKnownVictoryRival(VictoryMeasureKind.VICTORY_MEASURE_SCORE, iLoopVC, iActivePlayer)
					if iRival != -1:
						screen.setTableText(szTable, 2, iRow, ufont2 + GC.getTeam(iRival).getName() + ":", "", eWidGen, 1, 2, 1<<0)
						screen.setTableText(szTable, 3, iRow, ufont2 + (u"%d" % GAME.getTeamScore(iRival)), "", eWidGen, 1, 2, 1<<0)

				if VICTORY.conditionFlag(iLoopVC, VictoryConditionFlag.VICTORY_CONDITION_CONQUEST):
					iKnownRivals = CyTeam.countConquestRivals(iActivePlayer, True)
					iUnknownRivals = CyTeam.countConquestRivals(iActivePlayer, False)
					iVassals = CyTeam.getVassalCount()
					iRow = screen.appendTableRow(szTable)
					screen.setTableText(szTable, 0, iRow, ufont2 + TRNSLTR.getText("TXT_KEY_VICTORY_SCREEN_ELIMINATE_ALL", ()), "", eWidGen, 1, 2, 1<<1)
					szText = TRNSLTR.getText("TXT_KEY_VICTORY_SCREEN_RIVALS_LEFT", ()) + " "
					if iKnownRivals:
						szText += str(iKnownRivals)
					elif iUnknownRivals:
						szText += szUnknown
					else:
						szText += TRNSLTR.getText("TXT_KEY_NONE", ())
					screen.setTableText(szTable, 1, iRow, ufont2 + szText, "", eWidGen, 1, 2, 1<<2)

					if iVassals:
						szText = TRNSLTR.getText("TXT_KEY_BUG_VICTORY_VASSALED", (iVassals,))
						screen.setTableText(szTable, 2, iRow, ufont2 + szText, "", eWidGen, 1, 2, 1<<0)
					if iUnknownRivals:
						szText = TRNSLTR.getText("TXT_KEY_BUG_VICTORY_UNKNOWN", (iUnknownRivals,))
						screen.setTableText(szTable, 3, iRow, ufont2 + szText, "", eWidGen, 1, 2, 1<<0)

				if GAME.getAdjustedPopulationPercent(iLoopVC) > 0:
					iRow = screen.appendTableRow(szTable)
					szText = TRNSLTR.getText("TXT_KEY_VICTORY_SCREEN_PERCENT_POP", (GAME.getAdjustedPopulationPercent(iLoopVC), ))
					screen.setTableText(szTable, 0, iRow, ufont2 + szText, "", eWidGen, 1, 2, 1<<1)
					self.setShareRow(screen, szTable, iRow, CyTeam, VictoryMeasureKind.VICTORY_MEASURE_POPULATION, iLoopVC)

				if GAME.getAdjustedLandPercent(iLoopVC) > 0:
					iRow = screen.appendTableRow(szTable)
					szText = TRNSLTR.getText("TXT_KEY_VICTORY_SCREEN_PERCENT_LAND", (GAME.getAdjustedLandPercent(iLoopVC), ))
					screen.setTableText(szTable, 0, iRow, ufont2 + szText, "", eWidGen, 1, 2, 1<<1)
					self.setShareRow(screen, szTable, iRow, CyTeam, VictoryMeasureKind.VICTORY_MEASURE_LAND, iLoopVC)

				if VICTORY.conditionValue(iLoopVC, VictoryConditionValue.VICTORY_CONDITION_RELIGION_PERCENT) > 0:
					iRow = screen.appendTableRow(szTable)
					szText = TRNSLTR.getText("TXT_KEY_VICTORY_SCREEN_PERCENT_RELIGION", (VICTORY.conditionValue(iLoopVC, VictoryConditionValue.VICTORY_CONDITION_RELIGION_PERCENT),))
					screen.setTableText(szTable, 0, iRow, ufont2 + szText, "", eWidGen, 1, 2, 1<<1)
					screen.setTableText(szTable, 1, iRow, ufont2 + self.getScoringReligionText(CyTeam), "", eWidGen, 1, 2, 1<<2)
					iRival = CyTeam.getBestKnownVictoryRival(VictoryMeasureKind.VICTORY_MEASURE_RELIGION, iLoopVC, iActivePlayer)
					if iRival != -1:
						CyTeamRival = GC.getTeam(iRival)
						screen.setTableText(szTable, 2, iRow, ufont2 + CyTeamRival.getName() + ":", "", eWidGen, 1, 2, 1<<0)
						screen.setTableText(szTable, 3, iRow, ufont2 + self.getScoringReligionText(CyTeamRival), "", eWidGen, 1, 2, 1<<0)

				lBuildings = VICTORY.getBuildings(iLoopVC)
				if lBuildings:
					iRival = CyTeam.getBestKnownVictoryRival(VictoryMeasureKind.VICTORY_MEASURE_BUILDINGS, iLoopVC, iActivePlayer)
					for iBuilding in lBuildings:
						iRow = screen.appendTableRow(szTable)
						szNumber = unicode(INFO.getBuildingVictoryThreshold(iBuilding, iLoopVC))
						szText = TRNSLTR.getText("TXT_KEY_VICTORY_SCREEN_BUILDING", (szNumber, INFO.getTextKey("BUILDING_", iBuilding)))
						screen.setTableText(szTable, 0, iRow, ufont2 + szText, "", eWidGen, 1, 2, 1<<1)
						screen.setTableText(szTable, 1, iRow, ufont2 + str(CyTeam.getBuildingCount(iBuilding)), "", eWidGen, 1, 2, 1<<2)
						if iRival != -1:
							screen.setTableText(szTable, 2, iRow, ufont2 + GC.getTeam(iRival).getName() + ":", "", eWidGen, 1, 2, 1<<0)
							screen.setTableText(szTable, 3, iRow, ufont2 + str(GC.getTeam(iRival).getBuildingCount(iBuilding)), "", eWidGen, 1, 2, 1<<0)

				lProjects = VICTORY.getProjects(iLoopVC)
				if lProjects:
					iRival = CyTeam.getBestKnownVictoryRival(VictoryMeasureKind.VICTORY_MEASURE_PROJECTS, iLoopVC, iActivePlayer)
					bOurNeededBuilt = CyTeam.hasVictoryNeededProjects(iLoopVC)

					# The projects the victory's projects need: who built them
					for iNeeded in VICTORY.getNeededProjects(iLoopVC):
						iRow = screen.appendTableRow(szTable)
						screen.setTableText(szTable, 0, iRow, ufont2 + INFO.getDescription("PROJECT_", iNeeded), "", eWidGen, 1, 2, 1<<1)
						if CyTeam.getProjectCount(iNeeded) > 0:
							screen.setTableText(szTable, 1, iRow, ufont2 + TRNSLTR.getText("TXT_KEY_VICTORY_SCREEN_BUILT", (szTeamName, )), "", eWidGen, 1, 2, 1<<2)
						else:
							screen.setTableText(szTable, 1, iRow, ufont2 + TRNSLTR.getText("TXT_KEY_VICTORY_SCREEN_NOT_BUILT", ()), "", eWidGen, 1, 2, 1<<2)
						if iRival != -1 and GC.getTeam(iRival).getProjectCount(iNeeded) > 0:
							screen.setTableText(szTable, 2, iRow, ufont2 + TRNSLTR.getText("TXT_KEY_VICTORY_SCREEN_BUILT", (GC.getTeam(iRival).getName(), )), "", eWidGen, 1, 2, 1<<0)

					# The victory's projects, once someone known has built what they need
					bRivalNeededBuilt = iRival != -1 and GC.getTeam(iRival).hasVictoryNeededProjects(iLoopVC)
					if bOurNeededBuilt or bRivalNeededBuilt:
						for iProject in lProjects:
							iThreshold = INFO.getProjectVictoryThreshold(iProject, iLoopVC)
							iMinThreshold = INFO.getProjectVictoryMinThreshold(iProject, iLoopVC)
							lReqTechs = INFO.getRequiresIdsInClause("PROJECT_", iProject, EdgeBucket.EDGEB_TECHS, RequiresClause.REQCLAUSE_ALL)
							iRow = screen.appendTableRow(szTable)

							if iMinThreshold == iThreshold:
								szNumber = unicode(iThreshold)
							else:
								szNumber = unicode(iMinThreshold) + u"-" + unicode(iThreshold)
							szText = TRNSLTR.getText("TXT_KEY_VICTORY_SCREEN_BUILDING", (szNumber, INFO.getTextKey("PROJECT_", iProject)))
							screen.setTableText(szTable, 0, iRow, ufont2 + szText, "", eWidGen, 1, 2, 1<<1)

							if bOurNeededBuilt:
								iBuilt = CyTeam.getProjectCount(iProject)
								szPlayer = szTeamName + ":"
								szCount = "%i [+%i]" % (iBuilt, CyTeam.getProjectMaking(iProject))

								iColor = 0
								if iBuilt == iThreshold:
									szCount = "%i" % iBuilt
									iColor = self.COLOR_GREEN
								elif iBuilt >= iMinThreshold:
									iColor = self.COLOR_YELLOW

								if iColor > 0:
									szPlayer = TRNSLTR.changeTextColor(szPlayer, iColor)
									szCount = TRNSLTR.changeTextColor(szCount, iColor)

								szText = szPlayer
								if self.hasAllTechs(CyTeam, lReqTechs):
									szText += " - " + szCount
								screen.setTableText(szTable, 1, iRow, ufont2 + szText, "", eWidGen, 1, 2, 1<<2)

								if INFO.isProjectSpaceship(iProject):
									# Add spaceship button
									screen.setButtonGFC("SpaceShipButton" + str(iLoopVC), TRNSLTR.getText("TXT_KEY_GLOBELAYER_STRATEGY_VIEW", ()), "", 0, 0, 15, 10, eWidGen, 1234, -1, ButtonStyles.BUTTON_STYLE_STANDARD)
									screen.attachControlToTableCell("SpaceShipButton" + str(iLoopVC), szTable, iCategoryRow, 1)

									victoryDelay = GC.getTeam(iTeamAct).getVictoryCountdown(iLoopVC)
									if victoryDelay > 0 and GAME.getGameState() != GameStateTypes.GAMESTATE_EXTENDED:
										szDate = CyGameTextMgr().getTimeStr(GAME.getGameTurn() + victoryDelay, False)
										szText = TRNSLTR.getText("TXT_KEY_SPACE_SHIP_SCREEN_ARRIVAL", ()) + ": " + szDate
										screen.setTableText(szTable, 1, iCategoryRow, ufont2 + szText, "", eWidGen, 1, 2, 1<<2)
										szText = TRNSLTR.getText("TXT_KEY_TURNS", ()) + ":"
										screen.setTableText(szTable, 2, iCategoryRow, ufont2 + szText, "", eWidGen, 1, 2, 1<<0)
										screen.setTableText(szTable, 3, iCategoryRow, ufont2 + str(victoryDelay), "", eWidGen, 1, 2, 1<<0)

							if bRivalNeededBuilt:
								CyTeamRival = GC.getTeam(iRival)
								iBuilt = CyTeamRival.getProjectCount(iProject)
								szPlayer = CyTeamRival.getName() + ":"
								szCount = "%i" % iBuilt

								iColor = 0
								if iBuilt == iThreshold:
									iColor = self.COLOR_GREEN
								elif iBuilt >= iMinThreshold:
									iColor = self.COLOR_YELLOW
								elif self.hasAllTechs(CyTeamRival, lReqTechs) and (self.hasAllTechs(CyTeam, lReqTechs) or self.canResearchAll(iActivePlayer, lReqTechs)):
									iColor = GC.getInfoTypeForString("COLOR_PLAYER_ORANGE")

								if iColor > 0:
									szPlayer = TRNSLTR.changeTextColor(szPlayer, iColor)
									szCount = TRNSLTR.changeTextColor(szCount, iColor)

								screen.setTableText(szTable, 2, iRow, ufont2 + szPlayer, "", eWidGen, 1, 2, 1<<0)
								screen.setTableText(szTable, 3, iRow, ufont2 + szCount, "", eWidGen, 1, 2, 1<<0)

				if VICTORY.conditionFlag(iLoopVC, VictoryConditionFlag.VICTORY_CONDITION_DIPLO_VOTE) and not GAME.isOption(GameOptionTypes.GAMEOPTION_ENABLE_UNITED_NATIONS):
					for iVoteBuilding in self.getVoteBuildings():
						iRow = screen.appendTableRow(szTable)
						szText = TRNSLTR.getText("TXT_KEY_VICTORY_SCREEN_ELECTION", (INFO.getTextKey("BUILDING_", iVoteBuilding),))
						screen.setTableText(szTable, 0, iRow, ufont2 + szText, "", eWidGen, 1, 2, 1<<1)
						screen.setTableText(szTable, 1, iRow, ufont2 + self.getBuildingOwnerText(iVoteBuilding), "", eWidGen, 1, 2, 1<<2)

				eVictoryCulture = VICTORY.getCityCulture(iLoopVC)
				iNumCultureCities = VICTORY.conditionValue(iLoopVC, VictoryConditionValue.VICTORY_CONDITION_NUM_CULTURE_CITIES)
				if eVictoryCulture != CultureLevelTypes.NO_CULTURELEVEL and iNumCultureCities > 0:
					aOurCities = CyTeam.getCitiesByCulture(iNumCultureCities)
					iRival = CyTeam.getBestKnownVictoryRival(VictoryMeasureKind.VICTORY_MEASURE_CULTURE, iLoopVC, iActivePlayer)
					if iRival != -1:
						aRivalCities = GC.getTeam(iRival).getCitiesByCulture(iNumCultureCities)
					else:
						aRivalCities = []

					iRow = screen.appendTableRow(szTable)
					szText = TRNSLTR.getText("TXT_KEY_VICTORY_SCREEN_CITY_CULTURE", (iNumCultureCities, INFO.getTextKey("CULTURELEVEL_", eVictoryCulture)))
					screen.setTableText(szTable, 0, iRow, ufont2 + szText, "", eWidGen, 1, 2, 1<<1)

					for i in xrange(max(len(aOurCities), len(aRivalCities))):
						if i:
							iRow = screen.appendTableRow(szTable)
						if i < len(aOurCities):
							screen.setTableText(szTable, 1, iRow, ufont2 + self.getCultureCityText(aOurCities[i], eVictoryCulture), "", eWidGen, 1, 2, 1<<2)
						if i < len(aRivalCities):
							screen.setTableText(szTable, 2, iRow, ufont2 + self.getCultureCityText(aRivalCities[i], eVictoryCulture), "", eWidGen, 1, 2, 1<<0)


	def showGameSettingsScreen(self, screen):
		ufontEdge, ufont4b, ufont4, ufont3b, ufont3, ufont2b, ufont2, ufont1 = self.aFontList
		a3rdX = self.xRes / 3
		dx = a3rdX / 16
		x = dx
		y = self.Y_TOP_PAGE + 16
		dx = a3rdX - 2 * dx
		dy = self.H_PAGE - 32

		iPlayerAct = self.iActivePlayer
		CyPlayer = GC.getPlayer(iPlayerAct)

		self.deleteAllWidgets()
		eWidGen = WidgetTypes.WIDGET_GENERAL
		iPanelMain = PanelStyles.PANEL_STYLE_MAIN
		iTableEmpty = TableStyles.TABLE_STYLE_EMPTY
		iCol = self.COLOR_YELLOW
		BULLET = u'%c' %(GAME.getSymbolID(FontSymbols.BULLET_CHAR))

		screen.addPanel(self.getNextWidgetName(), "", "", True, True, x, y, dx, dy, iPanelMain)
		szSettingsTable = self.getNextWidgetName()
		screen.addListBoxGFC(szSettingsTable, "", x + 16, y + 16, dx - 32, dy - 32, iTableEmpty)
		screen.enableSelect(szSettingsTable, False)

		szTxt = ufont3b + TRNSLTR.getColorText("TXT_KEY_MAIN_MENU_SETTINGS", (), iCol) + "\n"
		screen.appendListBoxStringNoUpdate(szSettingsTable, szTxt, eWidGen, 1, 2, 1<<2)

		szTxt = BULLET + ufont2 + TRNSLTR.getText("TXT_KEY_LEADER_CIV_DESCRIPTION", (CyPlayer.getNameKey(), CyPlayer.getCivilizationShortDescriptionKey()))
		screen.appendListBoxStringNoUpdate(szSettingsTable, szTxt, eWidGen, 1, 2, 1<<0)

		szTxt = CyGameTextMgr().parseLeaderTraits(CyPlayer.getLeaderType(), True, False)
		if szTxt:
			szTxt = ufont1 + "\t<color=200,200,200>(" + szTxt + ")\n"
			screen.appendListBoxStringNoUpdate(szSettingsTable, szTxt, eWidGen, 1, 2, 1<<0)

		szTxt = BULLET + ufont2 + TRNSLTR.getText("TXT_KEY_SETTINGS_DIFFICULTY_GAME", (INFO.getTextKey("HANDICAP_", GAME.getHandicapType()),))
		screen.appendListBoxStringNoUpdate(szSettingsTable, szTxt, eWidGen, 1, 2, 1<<0)

		szTxt = BULLET + ufont2 + TRNSLTR.getText("TXT_KEY_SETTINGS_DIFFICULTY_PLAYER", (INFO.getTextKey("HANDICAP_", CyPlayer.getHandicapType()),))
		screen.appendListBoxStringNoUpdate(szSettingsTable, szTxt, eWidGen, 1, 2, 1<<0)

		szTxt = BULLET + ufont2 + TRNSLTR.getText("TXT_KEY_SETTINGS_GAME_SPEED", (INFO.getTextKey("GAMESPEED_", GAME.getGameSpeedType()),))
		screen.appendListBoxStringNoUpdate(szSettingsTable, szTxt, eWidGen, 1, 2, 1<<0)

		CyMap = GC.getMap()
		szTxt = BULLET + ufont2 + CyMap.getMapScriptName()
		screen.appendListBoxStringNoUpdate(szSettingsTable, szTxt, eWidGen, 1, 2, 1<<0)

		if CyMap.isWrapY():
			szTxt = "\t" + ufont1 + TRNSLTR.getText("TXT_KEY_MAP_WRAP_TOROID", ())
		elif CyMap.isWrapX():
			szTxt = "\t" + ufont1 + TRNSLTR.getText("TXT_KEY_MAP_WRAP_CYLINDER", ())
		else:
			szTxt = "\t" + ufont1 + TRNSLTR.getText("TXT_KEY_MAP_WRAP_FLAT", ())
		screen.appendListBoxStringNoUpdate(szSettingsTable, szTxt, eWidGen, 1, 2, 1<<0)

		szTxt = "\t" + ufont1 + TRNSLTR.getText("TXT_KEY_SETTINGS_MAP_SIZE", (INFO.getTextKey("WORLDSIZE_", CyMap.getWorldSize()),))
		szTxt += u"  (%dx%d)" %(CyMap.getGridWidth(), CyMap.getGridHeight())
		screen.appendListBoxStringNoUpdate(szSettingsTable, szTxt, eWidGen, 1, 2, 1<<0)

		szTxt = "\t" + ufont1 + TRNSLTR.getText("TXT_KEY_SETTINGS_CLIMATE", (INFO.getTextKey("CLIMATE_", CyMap.getClimate()),))
		screen.appendListBoxStringNoUpdate(szSettingsTable, szTxt, eWidGen, 1, 2, 1<<0)

		szTxt = "\t" + ufont1 + TRNSLTR.getText("TXT_KEY_SETTINGS_SEA_LEVEL", (INFO.getTextKey("SEALEVEL_", CyMap.getSeaLevel()),)) + "\n"
		screen.appendListBoxStringNoUpdate(szSettingsTable, szTxt, eWidGen, 1, 2, 1<<0)

		szTxt = BULLET + TRNSLTR.getText("TXT_KEY_SETTINGS_STARTING_ERA", (INFO.getTextKey("C2C_ERA_", GAME.getStartEra()),))
		screen.appendListBoxStringNoUpdate(szSettingsTable, szTxt, eWidGen, 1, 2, 1<<0)

		screen.updateListBox(szSettingsTable)

		x += a3rdX
		screen.addPanel(self.getNextWidgetName(), "", "", True, True, x, y, dx, dy, iPanelMain)
		szOptionsTable = self.getNextWidgetName()
		screen.addListBoxGFC(szOptionsTable, "", x + 16, y + 16, dx - 32, dy - 32, iTableEmpty)
		screen.enableSelect(szOptionsTable, False)

		szTxt = ufont3b + TRNSLTR.getColorText("TXT_KEY_MAIN_MENU_CUSTOM_SETUP_OPTIONS", (), iCol) + "\n"
		screen.appendListBoxStringNoUpdate(szOptionsTable, szTxt, eWidGen, 1, 2, 1<<2)

		if GAME.isGameMultiPlayer():
			szTxt = ""
			for i in xrange(GC.getNumMPOptionInfos()):
				if GAME.isMPOption(i):
					szTxt += BULLET + ufont2 + INFO.getDescription("MPOPTION_", i) + "\n"

			if GAME.getMaxTurns() > 0:
				szTxt += ufont1 + u"\t%s %d" % (TRNSLTR.getText("TXT_KEY_TURN_LIMIT_TAG", ()), GAME.getMaxTurns()) + "\n"

			if GAME.getMaxCityElimination() > 0:
				szTxt += ufont1 + u"\t%s %d" % (TRNSLTR.getText("TXT_KEY_CITY_ELIM_TAG", ()), GAME.getMaxCityElimination()) + "\n"

			screen.appendListBoxStringNoUpdate(szOptionsTable, szTxt, eWidGen, 1, 2, 1<<0)

		if GAME.isOption(GameOptionTypes.GAMEOPTION_CORE_CUSTOM_START):
			szTxt = BULLET + ufont2 + u"%s %d" %(TRNSLTR.getText("TXT_KEY_ADVANCED_START_POINTS", ()), GAME.getNumAdvancedStartPoints()) + "\n"
			screen.appendListBoxStringNoUpdate(szOptionsTable, szTxt, eWidGen, 1, 2, 1<<0)

		for i in xrange(GC.getNumGameOptionInfos()):
			if GAME.isOption(i):
				screen.appendListBoxStringNoUpdate(szOptionsTable, BULLET + ufont2 + INFO.getDescription("GAMEOPTION_", i), eWidGen, 1, 2, 1<<0)

		if GAME.hasSkippedSaveChecksum():
			szTxt = ufont2 + TRNSLTR.getText("TXT_KEY_BUFFYWARNING_CHECKSUM_SKIPPED", ())
			screen.appendListBoxStringNoUpdate(szOptionsTable, BULLET + szTxt, eWidGen, 1, 2, 1<<0)

		screen.updateListBox(szOptionsTable)

		x += a3rdX
		screen.addPanel(self.getNextWidgetName(), "", "", True, True, x, y, dx, dy, iPanelMain)
		szCivsTable = self.getNextWidgetName()
		screen.addListBoxGFC(szCivsTable, "", x + 16, y + 16, dx - 32, dy - 32, iTableEmpty)
		screen.enableSelect(szCivsTable, False)

		szTxt = ufont3b + TRNSLTR.getColorText("TXT_KEY_RIVALS_MET", (), iCol) + "\n"
		screen.appendListBoxStringNoUpdate(szCivsTable, szTxt, eWidGen, 1, 2, 1<<2)

		iTeam = CyPlayer.getTeam()
		for iPlayerX in xrange(GC.getMAX_PC_PLAYERS()):
			if iPlayerX == iPlayerAct:
				continue
			CyPlayerX = GC.getPlayer(iPlayerX)
			if not CyPlayerX.isEverAlive() or CyPlayerX.isMinorCiv():
				continue
			if GC.getTeam(CyPlayerX.getTeam()).isHasMet(iTeam) or GAME.isDebugMode():
				szTxt = BULLET + ufont2 + TRNSLTR.getText("TXT_KEY_LEADER_CIV_DESCRIPTION", (CyPlayerX.getNameKey(), CyPlayerX.getCivilizationShortDescriptionKey()))
				screen.appendListBoxStringNoUpdate(szCivsTable, szTxt, eWidGen, 1, 2, 1<<0)
				szTxt = CyGameTextMgr().parseLeaderTraits(CyPlayerX.getLeaderType(), True, False)
				if szTxt:
					szTxt = ufont1 + "\t<color=200,200,200>(" + szTxt + ")"
					screen.appendListBoxStringNoUpdate(szCivsTable, szTxt, eWidGen, 1, 2, 1<<0)

		screen.updateListBox(szCivsTable)


	def showVotingScreen(self, screen):
		uFont = self.aFontList[6]
		self.deleteAllWidgets()

		lVoteBuildings = self.getVoteBuildings()
		if not lVoteBuildings:
			return

		eWidGen = WidgetTypes.WIDGET_GENERAL
		xRes = self.xRes - 4
		szTable = self.getNextWidgetName()
		screen.addTableControlGFC(szTable, 2, 2, self.Y_TOP_PAGE, self.xRes, self.H_PAGE, False, True, 32,32, TableStyles.TABLE_STYLE_STANDARD)
		screen.enableSelect(szTable, False)
		screen.setTableColumnHeader(szTable, 0, "", xRes/4)
		screen.setTableColumnHeader(szTable, 1, "", 3*xRes/4)

		for iVoteBuilding in lVoteBuildings:
			iRow = screen.appendTableRow(szTable)
			screen.setTableText(szTable, 0, iRow, uFont + self.getBuildingOwnerText(iVoteBuilding), "", eWidGen, 1, 2, 1<<2)
			screen.setTableText(szTable, 1, iRow, uFont + TRNSLTR.getText("TXT_KEY_VICTORY_SCREEN_ELECTION", (INFO.getDescription("BUILDING_", iVoteBuilding),)), "", eWidGen, 1, 2, 1<<0)

		szPassed = uFont + TRNSLTR.getText("TXT_KEY_POPUP_PASSED", ())
		for i in xrange(GC.getNumVoteSourceInfos()):
			screen.appendTableRow(szTable)
			if not GAME.canHaveSecretaryGeneral(i): continue
			iTeamSG = GAME.getSecretaryGeneral(i)
			if -1 != iTeamSG:
				iRow = screen.appendTableRow(szTable)
				screen.setTableText(szTable, 0, iRow, uFont + GC.getTeam(iTeamSG).getName(), "", eWidGen, 1, 2, 1<<2)
				screen.setTableText(szTable, 1, iRow, uFont + INFO.getVoteSourceSecretaryGeneralText(i), "", eWidGen, 1, 2, 1<<0)

			for iLoop in INFO.getVoteSourceVotes(i):
				if GAME.isChooseElection(iLoop):
					iCount = GAME.countPossibleVote(iLoop, i)
					if iCount > 0:
						iRow = screen.appendTableRow(szTable)
						if GAME.isVotePassed(iLoop):
							screen.setTableText(szTable, 0, iRow, szPassed, "", eWidGen, 1, 2, 1<<2)
						else:
							szTxt = TRNSLTR.getText("TXT_KEY_POPUP_ELECTION_OPTION", ("", GAME.getVoteRequired(iLoop, i), iCount))
							screen.setTableText(szTable, 0, iRow, uFont + szTxt, "", eWidGen, 1, 2, 1<<2)

						screen.setTableText(szTable, 1, iRow, uFont + INFO.getDescription("VOTE_", iLoop), "", eWidGen, 1, 2, 1<<0)


	def showMembersScreen(self, screen):
		# The members table is a silent area (§ THE ONE HOVER, Button lists).
		PythonToolTip.buildButtonLists(self.buildMembersScreen, screen)

	def buildMembersScreen(self, screen):
		self.deleteAllWidgets()

		iRelVote = -1
		iUNVote = -1
		for i in xrange(GC.getNumVoteSourceInfos()):
			if GAME.isDiploVote(i):
				if GAME.getVoteSourceReligion(i) != -1:
					iRelVote = i
				else:
					iUNVote = i
		if iRelVote == -1 and iUNVote == -1: return  # AP nor UN are active

		if  iRelVote == -1: self.VoteBody = 2 # AP not active
		elif iUNVote == -1: self.VoteBody = 1 # UN not active

		if self.VoteBody == 1:
			iVoteBody = iRelVote
		else:
			iVoteBody = iUNVote

		# The election shown: the body's Secretary General election, or its vote for a victory when it holds one
		iVictoryVote = INFO.getVoteSourceVictoryVote(iVoteBody)
		if iVictoryVote == -1:
			self.VoteType = 1
		if self.VoteType == 1:
			iVoteIdx = INFO.getVoteSourceSecretaryGeneralVote(iVoteBody)
		else:
			iVoteIdx = iVictoryVote
		if iVoteIdx == -1: return

		iPlayerAct = self.iActivePlayer

		screen.addPanel(self.getNextWidgetName(), "", "", False, False, 0, 45, 1030, 680, PanelStyles.PANEL_STYLE_BLUE50)

		# set up the header row
		szHeading = self.getNextWidgetName()
		screen.addTableControlGFC(szHeading, 3, 10, 60, 1010, 30, False, False, 32,32, TableStyles.TABLE_STYLE_STANDARD)
		screen.setTableColumnHeader(szHeading, 0, "", 450)
		screen.setTableColumnHeader(szHeading, 1, "", 180)
		screen.setTableColumnHeader(szHeading, 2, "", 180)
		iRow = screen.appendTableRow(szHeading)

		# heading
		sTableHeader = "<font=4b>" + INFO.getVoteSourceSecretaryGeneralText(iVoteBody).upper() + "</font>"
		if GAME.getVoteSourceReligion(iVoteBody) != -1:
			sTableHeader += " (" + INFO.getDescription("RELIGION_", GAME.getVoteSourceReligion(iVoteBody)) + ")"

		screen.setTableText(szHeading, 0, iRow, sTableHeader, "", WidgetTypes.WIDGET_GENERAL, -1, -1, 1<<0)

		# the candidates, with the votes each is forecast to receive
		aForecast = GAME.getTeamVoteForecast(iVoteIdx, iVoteBody)
		iCandTeam1 = -1
		iCandTeam2 = -1
		iVoteTotal = [0] * 2
		if len(aForecast) > 0:
			iCandTeam1, iVoteTotal[0] = aForecast[0]
		if len(aForecast) > 1:
			iCandTeam2, iVoteTotal[1] = aForecast[-1]

		# each candidate team's leader, whose attitude columns the members' hovers show
		iCandPlayer1 = -1
		iCandPlayer2 = -1
		if iCandTeam1 != -1:
			iCandPlayer1 = GC.getTeam(iCandTeam1).getLeaderID()
		if iCandTeam2 != -1:
			iCandPlayer2 = GC.getTeam(iCandTeam2).getLeaderID()

		# candidate known returns -1 if there is no candidate, 0 if not known or 1 if known
		iCand1Known, sCand1Name = self.getCandStatusName(iCandTeam1)
		iCand2Known, sCand2Name = self.getCandStatusName(iCandTeam2)

		screen.setTableText(szHeading, 1, iRow, sCand1Name, "", WidgetTypes.WIDGET_GENERAL, -1, -1, 1<<2)
		screen.setTableText(szHeading, 2, iRow, sCand2Name, "", WidgetTypes.WIDGET_GENERAL, -1, -1, 1<<2)

		# set up the member table
		szTable = self.getNextWidgetName()
		x = 10
		y = 90
		h = 600
		iRowHeight = 32
		wName = 450
		wColumn = 90
		screen.addTableControlGFC(szTable, 6, x, y, 1010, h, False, False, iRowHeight, iRowHeight, TableStyles.TABLE_STYLE_STANDARD)
		screen.enableSelect(szTable, False)
		screen.setTableColumnHeader(szTable, 0, "", wName)
		screen.setTableColumnHeader(szTable, 1, "", wColumn)
		screen.setTableColumnHeader(szTable, 2, "", wColumn)
		screen.setTableColumnHeader(szTable, 3, "", wColumn)
		# The attitude columns (1 and 3) carry the leaders' hovers.
		self.membersRects = [(x + wName, y, wColumn, h, wColumn, iRowHeight), (x + wName + 2 * wColumn, y, wColumn, h, wColumn, iRowHeight)]
		screen.setTableColumnHeader(szTable, 4, "", wColumn)
		screen.setTableColumnHeader(szTable, 5, "", 200)
		iRow = screen.appendTableRow(szTable)

		# set up the vote selection texts
		iYellow = self.COLOR_YELLOW
		iX = 994
		szTxt = INFO.getVoteSourceSecretaryGeneralText(iVoteBody)
		if self.VoteType == 1:
			szTxt = TRNSLTR.changeTextColor(szTxt, iYellow)

		screen.setText("BUGVotePope_Widget", "", szTxt, 1<<1, iX, 688, 0, FontTypes.TITLE_FONT, WidgetTypes.WIDGET_GENERAL, -1, -1)

		if iVictoryVote != -1:
			iX -= 10 + CyInterface().determineWidth(szTxt)
			szTxt = TRNSLTR.getText("TXT_KEY_BUG_VICTORY_DIPLOMATIC", ())
			if self.VoteType == 2:
				szTxt = TRNSLTR.changeTextColor(szTxt, iYellow)

			screen.setText("BUGVoteDiplomaticVictory_Widget", "", szTxt, 1<<1, iX, 688, 0, FontTypes.TITLE_FONT, WidgetTypes.WIDGET_GENERAL, -1, -1)

		if iRelVote != -1 and iUNVote != -1:  # both AP and UN are active
			iX = 20
			szTxt = INFO.getDescription("VOTESOURCE_", iRelVote)
			if iVoteBody == iRelVote:
				szTxt = TRNSLTR.changeTextColor(szTxt, iYellow)

			screen.setText("VoteAP", "", szTxt, 1<<0, iX, 688, 0, FontTypes.TITLE_FONT, WidgetTypes.WIDGET_GENERAL, -1, -1)

			iX += 10 + CyInterface().determineWidth(szTxt)
			szTxt = INFO.getDescription("VOTESOURCE_", iUNVote)
			if iVoteBody == iUNVote:
				szTxt = TRNSLTR.changeTextColor(szTxt, iYellow)

			screen.setText("VoteUN", "", szTxt, 1<<0, iX, 688, 0, FontTypes.TITLE_FONT, WidgetTypes.WIDGET_GENERAL, -1, -1)
		else:
			screen.hide("VoteAP")
			screen.hide("VoteUN")

		# the team holding the body's building, labelled with the leader's title while no leader is elected
		iBuildingOwnerTeam = -1
		for iVoteBuilding in INFO.getVoteSourceBuildings(iVoteBody):
			iBuildingOwnerTeam = GAME.getBuildingOwnerTeam(iVoteBuilding)
			if iBuildingOwnerTeam != -1:
				break

		aList = []
		for iPlayerX in xrange(GC.getMAX_PC_PLAYERS()):
			if GC.getPlayer(iPlayerX).isAlive():
				aList.append((GC.getPlayer(iPlayerX).getVotes(iVoteIdx, iVoteBody), iPlayerX))
		aList.sort(reverse=True)

		for iVotes, iPlayerX in aList:
			CyPlayerX = GC.getPlayer(iPlayerX)
			iTeamX = CyPlayerX.getTeam()
			# player name
			bKnown = GC.getTeam(iTeamX).isKnownTo(iPlayerAct)
			if bKnown:
				szTxt = CyPlayerX.getName()
			else:
				szTxt = TRNSLTR.getText("TXT_KEY_TOPCIVS_UNKNOWN", ())

			if iVotes > 0 and bKnown:
				szTxt += TRNSLTR.getText("TXT_KEY_VICTORY_SCREEN_PLAYER_VOTES", (iVotes, iVoteBody),)

			iRow = screen.appendTableRow(szTable)
			screen.setTableText(szTable, 0, iRow, szTxt, "", WidgetTypes.WIDGET_GENERAL, -1, -1, 1<<0)

			if iPlayerX != iPlayerAct and bKnown:
				# player attitude to candidate #1
				szText = AttitudeUtil.getAttitudeText(iPlayerX, iCandPlayer1, True, True, False, False)
				if szText and iCand1Known == 1:
					screen.setTableText(szTable, 1, iRow, szText, "", WidgetTypes.WIDGET_LEADERHEAD, iPlayerX, iCandPlayer1, 1<<2)

				# player attitude to candidate #2
				szText = AttitudeUtil.getAttitudeText(iPlayerX, iCandPlayer2, True, True, False, False)
				if szText and iCand2Known == 1:
					screen.setTableText(szTable, 3, iRow, szText, "", WidgetTypes.WIDGET_LEADERHEAD, iPlayerX, iCandPlayer2, 1<<2)

			# the votes for the candidate this player is forecast to vote for
			iForecastTeam = CyPlayerX.getForecastTeamVote(iVoteIdx, iVoteBody)
			if iVotes > 0 and iForecastTeam != -1:
				if iForecastTeam == iCandTeam1 and iCand1Known == 1:
					screen.setTableText(szTable, 2, iRow, str(iVotes), "", WidgetTypes.WIDGET_GENERAL, -1, -1, 1<<2)
				elif iForecastTeam == iCandTeam2 and iCand2Known == 1:
					screen.setTableText(szTable, 4, iRow, str(iVotes), "", WidgetTypes.WIDGET_GENERAL, -1, -1, 1<<2)

			# player status
			if bKnown:
				if GAME.canHaveSecretaryGeneral(iVoteBody) and iBuildingOwnerTeam == iTeamX and GAME.getSecretaryGeneral(iVoteBody) == -1:
					szLabel = INFO.getVoteSourceSecretaryGeneralText(iVoteBody)

				elif GAME.canHaveSecretaryGeneral(iVoteBody) and GAME.getSecretaryGeneral(iVoteBody) == iTeamX:
					szLabel = INFO.getVoteSourceSecretaryGeneralText(iVoteBody)

				elif CyPlayerX.isFullMember(iVoteBody):
					szLabel = TRNSLTR.getText("TXT_KEY_VOTESOURCE_FULL_MEMBER", ())

				elif CyPlayerX.isVotingMember(iVoteBody):
					szLabel = TRNSLTR.getText("TXT_KEY_VOTESOURCE_VOTING_MEMBER", ())
				else:
					szLabel = TRNSLTR.getText("TXT_KEY_VOTESOURCE_NON_VOTING_MEMBER", ())

				screen.setTableText(szTable, 5, iRow, szLabel, "", WidgetTypes.WIDGET_GENERAL, -1, -1, 1<<1)

		iRow = screen.appendTableRow(szTable)
		iVoteReq = GAME.getVoteRequired(iVoteIdx, iVoteBody)
		sString = "<font=3b>" + TRNSLTR.getText("TXT_KEY_CONCEPT_TOTAL", ()) + "</font> "
		if iCand1Known and iCand2Known:
			sString +=  TRNSLTR.getText("TXT_KEY_BUG_VICTORY_VOTES_REQUIRED", ("%i" % iVoteReq,))
		screen.setTableText(szTable, 0, iRow, sString, "", WidgetTypes.WIDGET_GENERAL, 1, 1, 1<<0)

		iWinnerTeam, szWinnerShare, szWinnerMargin = GAME.getTeamVoteForecastWinner(iVoteIdx, iVoteBody)
		aCandidates = [(iCandTeam1, iCand1Known, 0, 2), (iCandTeam2, iCand2Known, 1, 4)]
		for iCandTeam, iCandKnown, iCand, iColumn in aCandidates:
			if iCandKnown == 1:
				# color code the totals
				sVoteTotal = str(iVoteTotal[iCand])
				iColor = self.getVoteTotalColor(iVoteReq, iVoteTotal[iCand], GC.getTeam(iCandTeam).getVotes(iVoteIdx, iVoteBody), iWinnerTeam == iCandTeam, self.VoteType == 2)
				if iColor != -1:
					sVoteTotal = TRNSLTR.changeTextColor(sVoteTotal, iColor)
				screen.setTableText(szTable, iColumn, iRow, sVoteTotal, "", WidgetTypes.WIDGET_GENERAL, 1, 1, 1<<2)

		# add a blank row
		iRow = screen.appendTableRow(szTable)

		if self.VoteType == 1:
			sSecGen = INFO.getVoteSourceSecretaryGeneralText(iVoteBody)
		else:
			sSecGen = TRNSLTR.getText("TXT_KEY_BUG_VICTORY_DIPLOMATIC", ())

		# display the forecast winner
		if iCand1Known != 0 and iCand2Known != 0 and iWinnerTeam != -1:
			sString = sSecGen + ":"
			iRow = screen.appendTableRow(szTable)
			screen.setTableText(szTable, 0, iRow, sString, "", WidgetTypes.WIDGET_GENERAL, -1, -1, 1<<0)

			sWin = self.getCandStatusName(iWinnerTeam)[1]
			sString = "     " + TRNSLTR.getText("TXT_KEY_BUG_VICTORY_BUG_POLL_RESULT", (sWin, szWinnerShare + u"%", szWinnerMargin + u"%"))
			iRow = screen.appendTableRow(szTable)
			screen.setTableText(szTable, 0, iRow, sString, "", WidgetTypes.WIDGET_GENERAL, -1, -1, 1<<0)

		iRow = screen.appendTableRow(szTable)

		# add info about vote timing
		iRow = screen.appendTableRow(szTable)
		iVoteTimer = GAME.getVoteTimer(iVoteBody)
		sString = "<font=2>" + TRNSLTR.getText("TXT_KEY_BUG_VICTORY_TURNS_NEXT_VOTE", (iVoteTimer,))
		screen.setTableText(szTable, 0, iRow, sString, "", WidgetTypes.WIDGET_GENERAL, -1, -1, 1<<0)

		iRow = screen.appendTableRow(szTable)
		iSecGenTimer = GAME.getSecretaryGeneralTimer(iVoteBody)
		sString = "<font=2>" + TRNSLTR.getText("TXT_KEY_BUG_VICTORY_VOTES_NEXT_ELECTION", (iSecGenTimer,))
		screen.setTableText(szTable, 0, iRow, sString, "", WidgetTypes.WIDGET_GENERAL, -1, -1, 1<<0)


	def getCandStatusName(self, iTeam):
		if iTeam == -1: # there is no candidate
			return -1, "-"

		if GC.getTeam(iTeam).isKnownTo(self.iActivePlayer):
			return 1, GC.getTeam(iTeam).getName()
		else:
			return 0, TRNSLTR.getText("TXT_KEY_TOPCIVS_UNKNOWN", ())

	def getVoteBuildings(self):
		"""The buildings that found a vote source, source by source."""
		lBuildings = []
		for iVoteSource in xrange(GC.getNumVoteSourceInfos()):
			lBuildings.extend(INFO.getVoteSourceBuildings(iVoteSource))
		return lBuildings

	def getBuildingOwnerText(self, iBuilding):
		"""Who built iBuilding: its owner's team, unknown when the viewer does not know it, or not built."""
		iTeam = GAME.getBuildingOwnerTeam(iBuilding)
		if iTeam == -1:
			return TRNSLTR.getText("TXT_KEY_VICTORY_SCREEN_NOT_BUILT", ())
		if GC.getTeam(iTeam).isKnownTo(self.iActivePlayer):
			szName = GC.getTeam(iTeam).getName()
		else:
			szName = TRNSLTR.getText("TXT_KEY_TOPCIVS_UNKNOWN", ())
		return TRNSLTR.getText("TXT_KEY_VICTORY_SCREEN_BUILT", (szName,))


	def getVoteTotalColor(self, iVoteReq, iVoteTotal, iVoteCand, bWinner, bVictoryVote):
		if not bWinner:
			return -1
		if iVoteCand > iVoteReq and bVictoryVote:
			return GC.getInfoTypeForString("COLOR_RED")
		if iVoteTotal > iVoteReq:
			return self.COLOR_GREEN
		return -1


	def getCultureCityText(self, aCityRow, eCultureLevel):
		"""A culture victory city, from a CyTeam.getCitiesByCulture row: its name and the turns until it reaches
		eCultureLevel, or its culture level once there or with no culture coming in."""
		CyCity = GC.getPlayer(aCityRow[0]).getCity(aCityRow[1])
		iTurns = CyCity.getCultureTurnsTo(eCultureLevel)
		if 0 < iTurns < 1000:
			szState = TRNSLTR.getText("TXT_KEY_MISC_TURNS_LEFT", (iTurns,))
		else:
			szState = INFO.getDescription("CULTURELEVEL_", CyCity.getCultureLevel())
		return CyCity.getName() + ": " + szState

	def setShareRow(self, screen, szTable, iRow, CyTeam, iMeasure, iVictory):
		"""Fill a domination row: the team's share of the world's measure, and the best known rival's."""
		eWidGen = WidgetTypes.WIDGET_GENERAL
		ufont2 = self.aFontList[6]
		screen.setTableText(szTable, 1, iRow, ufont2 + CyTeam.getVictoryShareText(iMeasure) + u"%", "", eWidGen, 1, 2, 1<<2)
		iRival = CyTeam.getBestKnownVictoryRival(iMeasure, iVictory, self.iActivePlayer)
		if iRival != -1:
			CyTeamRival = GC.getTeam(iRival)
			screen.setTableText(szTable, 2, iRow, ufont2 + CyTeamRival.getName() + ":", "", eWidGen, 1, 2, 1<<0)
			screen.setTableText(szTable, 3, iRow, ufont2 + CyTeamRival.getVictoryShareText(iMeasure) + u"%", "", eWidGen, 1, 2, 1<<0)

	def getScoringReligionText(self, CyTeam):
		"""The team's scoring religion and the percent of the world following it, or that it holds no holy city."""
		for iReligion, iPercent, bScoring in CyTeam.getHolyCityReligions():
			if bScoring:
				return INFO.getDescription("RELIGION_", iReligion) + u": %d%%" % iPercent
		return TRNSLTR.getText("TXT_KEY_VICTORY_SCREEN_NO_HOLY", ())

	# returns a unique ID for a widget in this screen
	def getNextWidgetName(self):
		szName = "VictoryScreenWidget" + str(self.nWidgetCount)
		self.nWidgetCount += 1
		return szName

	def deleteAllWidgets(self):
		self.membersRects = []
		screen = self.getScreen()
		i = self.nWidgetCount - 1
		while (i >= 0):
			self.nWidgetCount = i
			screen.deleteWidget(self.getNextWidgetName())
			i -= 1

		self.nWidgetCount = 0

		screen.deleteWidget("BUGVotePope_Widget")
		screen.deleteWidget("BUGVoteDiplomaticVictory_Widget")
		screen.deleteWidget("VoteAP")
		screen.deleteWidget("VoteUN")

	# handle the input for this screen...
	def handleInput(self, inputClass):
		screen = self.getScreen()

		iCode	= inputClass.eNotifyCode
		ID		= inputClass.iItemID
		NAME	= inputClass.szFunctionName

		if iCode == NotifyCode.NOTIFY_LISTBOX_ITEM_SELECTED:
			if NAME == "VictoryScreenDropdownWidget":
				self.iActivePlayer = screen.getPullDownData(NAME, screen.getSelectedPullDownID(NAME))
				self.iTab = 0
				self.showVictoryConditionScreen(screen)
		elif iCode == NotifyCode.NOTIFY_CLICKED:
			#	The engine splits a widget name's numeric tail off: a click on VS_Tab2 arrives as NAME "VS_Tab" with
			#	the 2 in iItemID (CvInfoScreen reads its identically built tabs the same way).
			if NAME == "VS_Tab":
				screen.hide("VS_Col_Tab" + str(self.iTab))
				screen.show("VS_Tab" + str(self.iTab))
				screen.hide("VS_Tab" + str(ID))
				screen.show("VS_Col_Tab" + str(ID))
				self.iTab = ID
				if ID == 0:
					self.showVictoryConditionScreen(screen)
				elif ID == 1:
					self.showGameSettingsScreen(screen)
				elif ID == 2:
					self.showVotingScreen(screen)
				elif ID == 3:
					self.showMembersScreen(screen)

			elif NAME == "BUGVotePope_Widget" and self.VoteType == 2:
				self.VoteType = 1
				self.iTab = 3
				self.showMembersScreen(screen)

			elif NAME == "BUGVoteDiplomaticVictory_Widget" and self.VoteType == 1:
				self.VoteType = 2
				self.iTab = 3
				self.showMembersScreen(screen)

			elif NAME == "VoteAP" and self.VoteBody == 2:
				self.VoteBody = 1
				self.iTab = 3
				self.showMembersScreen(screen)

			elif NAME == "VoteUN" and self.VoteBody == 1:
				self.VoteBody = 2
				self.iTab = 3
				self.showMembersScreen(screen)

			elif inputClass.getData1() == 1234:
				#close screen
				screen.setDying(True)
				CyInterface().clearSelectedCities()

				#popup spaceship screen
				popupInfo = CyPopupInfo()
				popupInfo.setButtonPopupType(ButtonPopupTypes.BUTTONPOPUP_PYTHON_SCREEN)
				popupInfo.setData1(-1)
				popupInfo.setText(u"showSpaceShip")
				popupInfo.addPopup(self.iActivePlayer)

	def update(self, fDelta):
		PythonToolTip.pollSilentArea()

	def silentAreaRects(self):
		if not self.getScreen().isActive():
			return []
		return self.membersRects