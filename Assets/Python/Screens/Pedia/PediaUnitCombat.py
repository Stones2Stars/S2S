# Sid Meier's Civilization 4
# Copyright Firaxis Games 2005

#
# Sevopedia 2.3
#   sevotastic.blogspot.com
#   sevotastic@yahoo.com
#
# additional work by Gaurav, Progor, Ket, Vovan, Fitchn, LunarMongoose
# see ReadMe for details
#

from CvPythonExtensions import *
INFO = CyInfo()
TRNSLTR = CyTranslator()

TEXT = CyGameTextMgr()
class PediaUnitCombat:

	def __init__(self, parent, H_BOT_ROW):
		self.main = parent

		self.H_PEDIA_PAGE = H_PEDIA_PAGE = parent.H_PEDIA_PAGE
		self.Y_TOP_ROW = parent.Y_PEDIA_PAGE

		self.H_TOP_ROW = H_TOP_ROW = (H_PEDIA_PAGE - H_BOT_ROW * 3) / 4

		self.W_PEDIA_PAGE = W_PEDIA_PAGE = parent.W_PEDIA_PAGE

		W_BASE = 64 + H_TOP_ROW + W_PEDIA_PAGE / 8

		self.X_COL_1 = X_COL_1 = parent.X_PEDIA_PAGE
		self.X_COL_2 = X_COL_1 + W_BASE + 4
		self.W_COL_1 = W_BASE - 4
		self.W_COL_2 = W_PEDIA_PAGE - W_BASE - 4

		self.S_ICON = H_TOP_ROW - 10


	def interfaceScreen(self, iTheUnitCombat):
		TRNSLTR = CyTranslator()
		screen = self.main.screen()

		aName = self.main.getNextWidgetName

		eWidGen		= WidgetTypes.WIDGET_GENERAL
		ePnlBlue50	= PanelStyles.PANEL_STYLE_BLUE50
		eFontTitle	= FontTypes.TITLE_FONT

		szfontEdge, szfont4b, szfont4, szfont3b, szfont3, szfont2b, szfont2 = self.main.aFontList

		H_PEDIA_PAGE = self.H_PEDIA_PAGE
		X_COL_1 = self.X_COL_1
		X_COL_2 = self.X_COL_2
		H_TOP_ROW = self.H_TOP_ROW
		Y_TOP_ROW_1 = self.Y_TOP_ROW
		Y_TOP_ROW_2 = Y_TOP_ROW_1 + H_TOP_ROW
		W_COL_1 = self.W_COL_1
		W_COL_2 = self.W_COL_2
		H_SPECIAL = H_PEDIA_PAGE - H_TOP_ROW
		S_ICON = self.S_ICON

		# Main Panel
		screen.setText(aName(), "", szfontEdge + INFO.getDescription("UNITCOMBAT_", iTheUnitCombat), 1<<0, X_COL_1, 0, 0, eFontTitle, eWidGen, 0, 0)
		Pnl = aName()
		screen.addPanel(Pnl, "", "", False, False, X_COL_1 - 3, Y_TOP_ROW_1 + 2, W_COL_1 + 8, H_TOP_ROW + 2, PanelStyles.PANEL_STYLE_MAIN)
		Img = "COMBAT|ToolTip" + str(iTheUnitCombat)
		self.main.aWidgetBucket.append(Img)
		screen.setImageButtonAt(Img, Pnl, INFO.getButton("UNITCOMBAT_", iTheUnitCombat), 4, 6, S_ICON, S_ICON, eWidGen, 1, 1)

		# Special
		szSpecial = CyGameTextMgr().getUnitCombatHelp(iTheUnitCombat, True)[1:]

		if szSpecial:
			screen.addPanel(aName(), "", "", True, False, X_COL_1, Y_TOP_ROW_2, W_COL_1, H_SPECIAL, ePnlBlue50)
			screen.addMultilineText(aName(), szfont3 + szSpecial, X_COL_1 + 4, Y_TOP_ROW_2 + 8, W_COL_1 - 8, H_SPECIAL - 16, eWidGen, 0, 0, 1<<0)

		# Unit Table -- the class's MEMBERS, read off its own reverse edge family. Membership is landed once at
		# load, so nothing sweeps the unit registry asking each id whether it belongs (DEC-one-reverse-view).
		aList = []
		for iUnit in INFO.getEdgeIds("UNITCOMBAT_", iTheUnitCombat, EdgeFamily.EDGEF_MEMBERS, EdgeBucket.EDGEB_UNITS):

			szName = INFO.getDescription("UNIT_", iUnit)

			# An air unit fights with its air strength and reaches with its RANGE; everything else with strength
			# and moves. The two are exclusive, so the column shows one or the other and never a sum.
			# airCombat is a human intrinsic; the other three are x100 cascade amounts, so only they reduce.
			iAirCombat = INFO.getUnitAirCombat(iUnit)
			iStrength = INFO.getScalar("UNIT_", iUnit, InfoScalar.SCALAR_STRENGTH,
				CascScope.CASC_SCOPE_UNIT, CascUnit.CASC_UNIT_FLAT)
			if iAirCombat > 0 and iStrength == 0:
				szStrength = str(iAirCombat)
				szMoves = str(INFO.getScalar("UNIT_", iUnit, InfoScalar.SCALAR_RANGE,
					CascScope.CASC_SCOPE_UNIT, CascUnit.CASC_UNIT_FLAT) / 100)
			else:
				szStrength = str(iStrength / 100)
				aMovement = INFO.getMovementKinds("UNIT_", iUnit, CascScope.CASC_SCOPE_UNIT)
				szMoves = str(aMovement[MovementKind.MOVEMENT_MOVES] / 100)

			iCost = INFO.getIntrinsic("UNIT_", iUnit, IntrinsicSlot.PYINT_COST)
			if iCost < 0:
				szCost = TRNSLTR.getText("TXT_KEY_NON_APPLICABLE", ())
			else:
				szCost = str(iCost)

			aList.append((iUnit, szName, szStrength, szMoves, szCost))

		if aList:
			screen.addPanel(aName(), "", "", True, True, X_COL_2, Y_TOP_ROW_1, W_COL_2, H_PEDIA_PAGE, ePnlBlue50)
			Pnl = aName()
			screen.addPanel(Pnl, "", "", True, True, X_COL_2 + 5, Y_TOP_ROW_1 + 6, W_COL_2 - 16, H_PEDIA_PAGE - 12, ePnlBlue50)
			screen.setStyle(Pnl, "Panel_Black50_Style")
			Tbl = aName()
			# FOUR columns: the tech-grid one is GONE with the condition walker it read through. Its helper took
			# an info OBJECT and recursed the retired BoolExpr tree; that display returns with the `requires`
			# renderer, never by restoring a Python-side tree walk (DEC-no-legacy-masking).
			screen.addTableControlGFC(Tbl, 4, X_COL_2 + 6, Y_TOP_ROW_1 + 13, W_COL_2 - 6, H_PEDIA_PAGE - 26, True, False, 32, 32, TableStyles.TABLE_STYLE_EMPTY)
			screen.enableSort(Tbl)
			iTableWidth = W_COL_2 - 16
			iColWidth = iTableWidth / 2
			screen.setTableColumnHeader(Tbl, 0, "", iColWidth)
			iColWidth = iTableWidth - iColWidth
			screen.setTableColumnHeader(Tbl, 1, u"%c" % CyGame().getSymbolID(FontSymbols.STRENGTH_CHAR), iColWidth * 3 / 10)
			screen.setTableColumnHeader(Tbl, 2, u"%c" % CyGame().getSymbolID(FontSymbols.MOVES_CHAR), iColWidth * 3 / 10)
			screen.setTableColumnHeader(Tbl, 3, u"%c" % TEXT.getSymbolChar("YIELD_", YieldTypes.YIELD_PRODUCTION), iColWidth * 4 / 10)

			for iUnit, szName, szStrength, szMoves, szCost in aList:
				iRow = screen.appendTableRow(Tbl)
				screen.setTableText(Tbl, 0, iRow, szfont2 + szName, "", WidgetTypes.WIDGET_PEDIA_JUMP_TO_UNIT, iUnit, 1, 1<<0)
				screen.setTableInt(Tbl, 1, iRow, szfont2 + szStrength, "", eWidGen, 1, 2, 1<<0)
				screen.setTableInt(Tbl, 2, iRow, szfont2 + szMoves, "", eWidGen, -1, -1, 1<<0)
				screen.setTableInt(Tbl, 3, iRow, szfont2 + szCost, "", eWidGen, -1, -1, 1<<0)

