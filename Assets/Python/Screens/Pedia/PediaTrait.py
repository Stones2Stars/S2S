# Pedia overhaul by Toffer for Caveman2Cosmos.

from CvPythonExtensions import *
INFO = CyInfo()

class Page:

	def __init__(self, parent, H_BOT_ROW):
		self.main = parent

		self.H_PEDIA_PAGE = H_PEDIA_PAGE = parent.H_PEDIA_PAGE
		self.W_PEDIA_PAGE = parent.W_PEDIA_PAGE

		self.Y_ROW_1 = parent.Y_PEDIA_PAGE

		self.H_TOP_ROW = H_TOP_ROW = (H_PEDIA_PAGE - H_BOT_ROW * 3) / 4
		self.H_BOT_ROW = H_BOT_ROW

		self.X_COL_1 = parent.X_PEDIA_PAGE

		self.S_ICON = H_TOP_ROW - 10


	def interfaceScreen(self, iTheTrait):
		aName = self.main.getNextWidgetName

		eWidGen		= WidgetTypes.WIDGET_GENERAL
		ePnlBlue50	= PanelStyles.PANEL_STYLE_BLUE50

		szfontEdge, szfont4b, szfont4, szfont3b, szfont3, szfont2b, szfont2 = self.main.aFontList

		H_ROW_1 = self.H_TOP_ROW
		H_ROW_2 = self.H_BOT_ROW
		S_ICON = self.S_ICON
		X_COL_1 = self.X_COL_1
		Y_ROW_1 = self.Y_ROW_1
		Y_ROW_3 = Y_ROW_2 = Y_ROW_1 + H_ROW_1
		H_ROW_3 = self.H_PEDIA_PAGE - H_ROW_1
		W_PEDIA_PAGE = self.W_PEDIA_PAGE

		screen = self.main.screen()

		# Main Panel
		szTxt1 = szfontEdge + INFO.getDescription("TRAIT_", iTheTrait)
		screen.setText(aName(), "", szTxt1, 1<<0, X_COL_1, 0, 0, FontTypes.TITLE_FONT, eWidGen, 1, 1)
		Pnl = aName()
		screen.addPanel(Pnl, "", "", False, False, X_COL_1 - 3, Y_ROW_1 + 2, H_ROW_1 + 2, H_ROW_1 + 2, PanelStyles.PANEL_STYLE_MAIN)
		Img = "ToolTip|TRAIT%d" % iTheTrait
		screen.setImageButtonAt(Img, Pnl, INFO.getButton("TRAIT_", iTheTrait), 4, 6, S_ICON, S_ICON, eWidGen, 1, 1)
		# Strategy & help text.
		szTxt1 = INFO.getStrategy("TRAIT_", iTheTrait)
		szTxt2 = INFO.getHelp("TRAIT_", iTheTrait)
		if szTxt2:
			if szTxt1:
				szTxt1 += "\n"
			szTxt1 += szTxt2
		if szTxt1:
			x = X_COL_1 + H_ROW_1 + 4
			w = W_PEDIA_PAGE - H_ROW_1 - 4
			screen.addPanel(aName(), "", "", True, True, x, Y_ROW_1, w, H_ROW_1, ePnlBlue50)
			screen.addMultilineText(aName(), szfont2 + szTxt1, x + 4, Y_ROW_1 + 8, w - 8, H_ROW_1 - 16, eWidGen, 1, 1, 1<<0)
		# Leaders
		#  A leader holds its traits as a plain FK list and a trait never names a leader, so this reads the
		#  inverse the load builds. It is EMPTY until the assignments are authored, which is expected.
		aList = []
		for iLeader in INFO.getEdgeIds("TRAIT_", iTheTrait, EdgeFamily.EDGEF_RELATED, EdgeBucket.EDGEB_LEADERS):
			aList.append([iLeader, INFO.getButton("LEADER_", iLeader)])
		if aList:
			screen.addPanel(aName(), "", "", False, True, X_COL_1, Y_ROW_2, W_PEDIA_PAGE, H_ROW_2, ePnlBlue50)
			iSize = H_ROW_2*4/5
			Pnl = aName()
			screen.addScrollPanel(Pnl, "", X_COL_1 - 4, Y_ROW_2, W_PEDIA_PAGE + 6, H_ROW_2 - 26, ePnlBlue50)
			screen.setStyle(Pnl, "ScrollPanel_Alt_Style")
			x = 2
			y = (H_ROW_2 - iSize)/2 - 6
			for iLeader, BTN in aList:
				screen.setImageButtonAt("ToolTip|JumpTo|LEADER%d" % iLeader, Pnl, BTN, x, y, iSize, iSize, eWidGen, 1, 1)
				x += iSize + 6
			Y_ROW_3 += H_ROW_2
			H_ROW_3 -= H_ROW_2
		# Pedia Text & Effects
		szTxt1 = CyGameTextMgr().parseTraits(iTheTrait, False, True)[1:]
		szTxt2 = INFO.getCivilopedia("TRAIT_", iTheTrait)
		if szTxt1 or szTxt2:
			if szTxt1 and szTxt2:
				W_ROW_3 = W_PEDIA_PAGE/2 - 4
				X_COL_3 = X_COL_1 + W_ROW_3 + 8
			else:
				W_ROW_3 = W_PEDIA_PAGE
				X_COL_3 = X_COL_1
			if szTxt1:
				screen.addPanel(aName(), "", "", True, False, X_COL_1, Y_ROW_3, W_ROW_3, H_ROW_3, ePnlBlue50)
				screen.addMultilineText(aName(), szfont3 + szTxt1, X_COL_1 + 4, Y_ROW_3 + 8, W_ROW_3 - 8, H_ROW_3 - 16, eWidGen, 1, 1, 1<<0)
			if szTxt2:
				screen.addPanel(aName(), "", "", True, False, X_COL_3, Y_ROW_3, W_ROW_3, H_ROW_3, ePnlBlue50)
				screen.addMultilineText(aName(), szfont2 + szTxt2, X_COL_3 + 4, Y_ROW_3 + 8, W_ROW_3 - 8, H_ROW_3 - 16, eWidGen, 1, 1, 1<<0)
