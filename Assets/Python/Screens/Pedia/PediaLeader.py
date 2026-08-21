# Pedia overhaul by Toffer for Caveman2Cosmos.

from CvPythonExtensions import *
INFO = CyInfo()
TRNSLTR = CyTranslator()

class PediaLeader:

	def __init__(self, parent, H_BOT_ROW):
		self.main = parent

		H_PEDIA_PAGE = parent.H_PEDIA_PAGE

		self.Y_TOP_ROW = Y_TOP_ROW = parent.Y_PEDIA_PAGE
		self.H_BOT_ROW = H_BOT_ROW

		Y_BOT_ROW = Y_TOP_ROW + H_PEDIA_PAGE - H_BOT_ROW

		self.H_ROW_1 = H_ROW_1 = 3 * (H_PEDIA_PAGE - H_BOT_ROW * 3) / 4

		self.Y_ROW_2 = Y_ROW_2 = Y_TOP_ROW + H_ROW_1
		self.Y_ROW_3 = Y_ROW_3 = Y_ROW_2 + H_BOT_ROW
		self.Y_ROW_4 = Y_ROW_4 = Y_ROW_3 + H_BOT_ROW

		self.H_ROW_2 = Y_BOT_ROW + H_BOT_ROW - Y_ROW_2
		self.H_ROW_4 = Y_BOT_ROW + H_BOT_ROW - Y_ROW_4

		self.W_COL_1 = W_COL_1 = int(H_ROW_1 * 0.78)
		self.W_COL_2 = parent.W_PEDIA_PAGE - W_COL_1 - 8

		self.X_COL_1 = X_COL_1 = parent.X_PEDIA_PAGE
		self.X_COL_2 = X_COL_1 + W_COL_1 + 8


	def interfaceScreen(self, iLeader):
		TRNSLTR = CyTranslator()
		screen = self.main.screen()
		aName = self.main.getNextWidgetName

		eWidGen			= WidgetTypes.WIDGET_GENERAL
		ePanelBlue50	= PanelStyles.PANEL_STYLE_BLUE50

		enumGBS	= self.main.enumGBS
		szfontEdge, szfont4b, szfont4, szfont3b, szfont3, szfont2b, szfont2 = self.main.aFontList

		H_BOT_ROW = self.H_BOT_ROW
		X_COL_1 = self.X_COL_1
		X_COL_2 = self.X_COL_2
		Y_ROW_1 = self.Y_TOP_ROW
		Y_ROW_2 = self.Y_ROW_2
		Y_ROW_3 = self.Y_ROW_3
		W_COL_1 = self.W_COL_1
		W_COL_2 = self.W_COL_2
		H_ROW_1 = self.H_ROW_1
		H_ROW_2 = self.H_ROW_2

		# Main Panel
		szName = szfontEdge + INFO.getDescription("LEADER_", iLeader)
		if INFO.isNPCLeader(iLeader):
			szName += " (NPC)"
		screen.setText(aName(), "", szName, 1<<0, X_COL_1, 0, 0, FontTypes.TITLE_FONT, eWidGen, 0, 0)
		screen.addPanel(aName(), "", "", False, False, X_COL_1, Y_ROW_1, W_COL_1, H_ROW_1, PanelStyles.PANEL_STYLE_MAIN)
		screen.addDDSGFC(aName(), INFO.getLeaderHeadArt(iLeader), X_COL_1+12, Y_ROW_1+12, W_COL_1-24, H_ROW_1-24, eWidGen, -1, -1)

		# Civilization
		panelName = aName()
		screen.addPanel(panelName, "", "", False, True, X_COL_1, Y_ROW_2, W_COL_1, H_BOT_ROW, ePanelBlue50)
		screen.attachLabel(panelName, "", "  ")
		screen.attachImageButton(panelName, "", INFO.getButton("LEADER_", iLeader), enumGBS, eWidGen, iLeader, 0, False)
		# The civs this leader may lead -- the leader's OWN inverse of the civilization roster, landed at load.
		# A civ names its leaders and a leaderhead names no civ, so sweeping every civilization asking
		# `isLeaders(me)` was the own-data inversion (DEC-one-reverse-view).
		iWidget = WidgetTypes.WIDGET_PEDIA_JUMP_TO_CIV
		for iCiv in INFO.getEdgeIds("LEADER_", iLeader, EdgeFamily.EDGEF_RELATED, EdgeBucket.EDGEB_CIVILIZATIONS):
			screen.attachImageButton(panelName, "", INFO.getButton("CIVILIZATION_", iCiv), enumGBS, iWidget, iCiv, 1, False)

		# Favourite Civic & Religion
		panelName = aName()
		screen.addPanel(panelName, TRNSLTR.getText("TXT_KEY_PEDIA_FAV_CIVIC_AND_RELIGION", ()), "", False, True, X_COL_1, Y_ROW_3, W_COL_1, H_BOT_ROW, ePanelBlue50)
		screen.attachLabel(panelName, "", "  ")

		iCivic = INFO.getIntrinsic("LEADER_", iLeader, IntrinsicSlot.PYINT_FAVORITE_CIVIC)
		if iCivic != -1:
			screen.attachImageButton(panelName, "", INFO.getButton("CIVIC_", iCivic), enumGBS, WidgetTypes.WIDGET_PEDIA_JUMP_TO_CIVIC, iCivic, 1, False)

		iReligion = INFO.getIntrinsic("LEADER_", iLeader, IntrinsicSlot.PYINT_FAVORITE_RELIGION)
		if iReligion != -1:
			screen.attachImageButton(panelName, "", INFO.getButton("RELIGION_", iReligion), enumGBS, WidgetTypes.WIDGET_PEDIA_JUMP_TO_RELIGION, iReligion, 1, False)

		# History
		screen.addPanel(aName(), "", "", True, True, X_COL_2, Y_ROW_1, W_COL_2, H_ROW_1, ePanelBlue50)
		szText = szfont3 + INFO.getCivilopedia("LEADER_", iLeader)
		screen.addMultilineText(aName(), szText, X_COL_2+4, Y_ROW_1+16, W_COL_2-8, H_ROW_1-32, eWidGen, 0, 0, 1<<0)

		# Traits
		screen.addPanel(aName(), "", "", True, False, X_COL_2, Y_ROW_2, W_COL_2, H_ROW_2, ePanelBlue50)
		szSpecialText = szfont3 + CyGameTextMgr().parseLeaderTraits(iLeader, False, True)
		screen.addMultilineText(aName(), szSpecialText, X_COL_2+4, Y_ROW_2+16, W_COL_2-8, H_ROW_2-32, eWidGen, -1, -1, 1<<0)

		# The PERSONALITY dump is deliberately absent (S2S#455). It printed eleven raw leaderhead integers with
		# no units and no scale stated, which tells a player nothing they can act on -- and a personality
		# display cannot be written before the FLAVOUR system it reports on is described. Re-serving the legacy
		# getters to render it again would preserve exactly the shape that issue exists to replace.
