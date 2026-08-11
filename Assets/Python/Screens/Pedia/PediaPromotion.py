# Pedia overhaul by Toffer for Caveman2Cosmos.

from CvPythonExtensions import *
INFO = CyInfo()
TRNSLTR = CyTranslator()

class PediaPromotion:

	def __init__(self, parent, H_BOT_ROW):
		self.main = parent

		H_PEDIA_PAGE = parent.H_PEDIA_PAGE

		self.Y_TOP_ROW = Y_TOP_ROW = parent.Y_PEDIA_PAGE
		self.Y_BOT_ROW = Y_TOP_ROW + H_PEDIA_PAGE - H_BOT_ROW

		self.H_TOP_ROW = H_TOP_ROW = (H_PEDIA_PAGE - H_BOT_ROW * 3) / 4
		self.H_BOT_ROW = H_BOT_ROW

		self.S_ICON = H_TOP_ROW - 10

		self.Y_MID_ROW = Y_TOP_ROW + H_TOP_ROW
		self.H_MID_LEFT = H_PEDIA_PAGE - H_BOT_ROW - H_TOP_ROW
		self.H_MID_RIGHT = H_PEDIA_PAGE - H_BOT_ROW

		self.W_PEDIA_PAGE = W_PEDIA_PAGE = parent.W_PEDIA_PAGE

		self.W_HALF_PP = W_HALF_PP = W_PEDIA_PAGE / 2 - 4
		self.W_3RD_PP = W_PEDIA_PAGE / 3 - 4

		self.X_COL_1 = X_COL_1 = parent.X_PEDIA_PAGE
		self.X_COL_2 = X_COL_1 + W_HALF_PP + 8

	def interfaceScreen(self, iThePromotion):
		TRNSLTR = CyTranslator()
		screen = self.main.screen()

		iWidGen			= WidgetTypes.WIDGET_GENERAL
		iPanelBlue50	= PanelStyles.PANEL_STYLE_BLUE50
		iFontGame		= FontTypes.GAME_FONT

		enumGBS = self.main.enumGBS
		uFontEdge, uFont4b, uFont4, uFont3b, uFont3, uFont2b, uFont2 = self.main.aFontList

		X_COL_1 = self.X_COL_1
		X_COL_2 = self.X_COL_2
		Y_BOT_ROW = self.Y_BOT_ROW
		Y_MID_ROW = self.Y_MID_ROW
		Y_TOP_ROW = self.Y_TOP_ROW
		H_BOT_ROW = self.H_BOT_ROW
		H_TOP_ROW = self.H_TOP_ROW
		H_MID_LEFT = self.H_MID_LEFT
		H_MID_RIGHT = self.H_MID_RIGHT
		W_PEDIA_PAGE = self.W_PEDIA_PAGE
		W_HALF_PP = self.W_HALF_PP
		S_ICON = self.S_ICON

		aName = self.main.getNextWidgetName

		# Main Panel
		screen.setText(aName(), "", uFontEdge + INFO.getDescription("PROMOTION_", iThePromotion), 1<<0, X_COL_1, 0, 0, FontTypes.TITLE_FONT, iWidGen, 0, 0)
		Pnl = aName()
		screen.addPanel(Pnl, "", "", False, False, X_COL_1 - 3, Y_TOP_ROW + 2, W_HALF_PP + 8, H_TOP_ROW + 2, PanelStyles.PANEL_STYLE_MAIN)
		Img = "ToolTip|PROMO" + str(iThePromotion)
		screen.setImageButtonAt(Img, Pnl, INFO.getButton("PROMOTION_", iThePromotion), 4, 6, S_ICON, S_ICON, iWidGen, 1, 1)
		# Leads To -- the promotion's OWN forward edge. A ladder rung `enables` the rung above it
		# ([json.md] par.9: a ladder is an enables edge), so this is a straight list fetch and nothing sweeps
		# the promotion registry asking each rung whether it names this one (DEC-one-reverse-view).
		#
		# The REQUIRES icon strip is GONE. It decorated its icons with &/||/brackets, i.e. it reconstructed the
		# condition tree's STRUCTURE -- and CvConditionQuery refuses to report how a tree combines, deliberately:
		# its walk reaches noneOf and disabled too, so an id-list strip would advertise a FORBIDDEN entity as
		# required. The requires are rendered instead by the promotion help below, DLL-side, where the and/or
		# and the noneOf are all honoured ([pedia-read-map.md] finding 3: no boolean-expression API belongs on
		# this surface).
		PF = "ToolTip|JumpTo|"
		aList1 = []
		aList2 = [] # Leads To
		for iType in INFO.getEdgeIds("PROMOTION_", iThePromotion, EdgeFamily.EDGEF_ENABLES, EdgeBucket.EDGEB_PROMOTIONS):
			aList2.append((INFO.getButton("PROMOTION_", iType), "PROMO" + str(iType)))
		if aList2:
			LeadsToPanel = aName()
			screen.addPanel(LeadsToPanel, TRNSLTR.getText("TXT_KEY_PEDIA_LEADS_TO", ()), "", False, True, X_COL_1, Y_BOT_ROW, W_PEDIA_PAGE, H_BOT_ROW, iPanelBlue50)
			for BTN, szChild in aList2:
				screen.attachImageButton(LeadsToPanel, PF + szChild, BTN, enumGBS, iWidGen, 1, 1, False)
			aList2 = []
		else:
			H_MID_LEFT += H_BOT_ROW
			H_MID_RIGHT += H_BOT_ROW
		# Promotion Help
		szText = CyGameTextMgr().getPromotionHelp(iThePromotion, True)[1:]
		if szText:
			szText = uFont3 + szText
			screen.addPanel(aName(), "", "", True, False, X_COL_1, Y_MID_ROW, W_HALF_PP, H_MID_LEFT, iPanelBlue50)
			screen.addMultilineText(aName(), szText, X_COL_1 + 4, Y_MID_ROW + 12, W_HALF_PP - 8, H_MID_LEFT - 20, iWidGen, 0, 0, 1<<0)

		# Unit Combats -- the info's own post-load caches, already folded over this rung AND its line, so the
		# page reads a computed answer instead of re-folding a ladder per render.
		aList1 = INFO.getIdList("PROMOTION_", iThePromotion, IdListSlot.PYLIST_QUALIFIED_UNITCOMBATS)
		aList2 = INFO.getIdList("PROMOTION_", iThePromotion, IdListSlot.PYLIST_DISQUALIFIED_UNITCOMBATS)

		if aList1 or aList2:
			szChild = PF + "COMBAT"
			if uFont3b == "<font=3b>":
				sIcon = 32
			elif uFont3b == "<font=2b>":
				sIcon = 28
			else:
				sIcon = 24
			dy = sIcon + 2

			n = 0
			screen.addPanel(aName(), "", "", True, True, X_COL_2, Y_TOP_ROW + 2, W_HALF_PP, H_MID_RIGHT - 2, iPanelBlue50)
			ScrlPnl = aName()
			screen.addScrollPanel(ScrlPnl, "", X_COL_2, Y_TOP_ROW + 8, W_HALF_PP, H_MID_RIGHT - 38, iPanelBlue50)
			screen.setStyle(ScrlPnl, "ScrollPanel_Alt_Style")
			y = -4
			if aList1:
				screen.setTextAt(aName(), ScrlPnl, uFont3b + TRNSLTR.getText("TXT_KEY_VALID_FOR", ()), 1<<0, 0, y, 0, iFontGame, iWidGen, 1, 2)
				y += 4 + dy
				for iType in aList1:
					screen.addDDSGFCAt(szChild + str(iType) + "|" + str(n), ScrlPnl, INFO.getButton("UNITCOMBAT_", iType), 0, y, sIcon, sIcon, iWidGen, 1, 2, False)
					n += 1
					szText = "<color=230,230,0,255>" + uFont2b + INFO.getDescription("UNITCOMBAT_", iType)
					screen.setTextAt(szChild + str(iType) + "|" + str(n), ScrlPnl, szText, 1<<0, sIcon + 4, y+6, 0, iFontGame, iWidGen, 1, 2)
					n += 1
					y += dy
				y += dy

			if aList2:
				screen.setTextAt(aName(), ScrlPnl, uFont3b + TRNSLTR.getText("TXT_KEY_VALID_FOR_NOT", ()), 1<<0, 0, y, 0, iFontGame, iWidGen, 1, 2)
				y += 4 + dy
				for iType in aList2:
					screen.addDDSGFCAt(szChild + str(iType) + "|" + str(n), ScrlPnl, INFO.getButton("UNITCOMBAT_", iType), 0, y, sIcon, sIcon, iWidGen, 1, 2, False)
					n += 1
					szText = "<color=255,80,80,255>" + uFont2b + INFO.getDescription("UNITCOMBAT_", iType)
					screen.setTextAt(szChild + str(iType) + "|" + str(n), ScrlPnl, szText, 1<<0, sIcon + 4, y+6, 0, iFontGame, iWidGen, 1, 2)
					n += 1
					y += dy
