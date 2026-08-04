from CvPythonExtensions import *
GC = CyGlobalContext()
INFO = CyInfo()
TEXT = CyGameTextMgr()
GAME = GC.getGame()

def GameFontScreen():
	import CvScreenEnums
	import ScreenResolution
	xRes = ScreenResolution.x
	yRes = ScreenResolution.y
	GC = CyGlobalContext()
	GAME = GC.getGame()

	screen = CyGInterfaceScreen("GameFontScreen", CvScreenEnums.GAMEFONT_SCREEN)
	screen.addPanel("", "", "", True, False, -10, -10, xRes + 20, yRes + 20, PanelStyles.PANEL_STYLE_MAIN)

	TABLE = "GameFontTable"
	screen.addTableControlGFC(TABLE, 5, (xRes-772)/2, 0, 772, yRes, True, False, 24, 24, TableStyles.TABLE_STYLE_STANDARD)
	screen.setTableColumnHeader(TABLE, 0, "ID", 64)
	screen.setTableColumnHeader(TABLE, 1, "Small", 64)
	screen.setTableColumnHeader(TABLE, 2, "Big", 64)
	screen.setTableColumnHeader(TABLE, 3, "Button", 64)
	screen.setTableColumnHeader(TABLE, 4, "Type", 500)

	eWidGen = WidgetTypes.WIDGET_GENERAL
	iRandom = GAME.getSymbolID(FontSymbols.RANDOM_CHAR)
	iHappy = GAME.getSymbolID(FontSymbols.HAPPY_CHAR)
	aList0 = [
		"HAPPY_CHAR",
		"UNHAPPY_CHAR",
		"HEALTHY_CHAR",
		"UNHEALTHY_CHAR",
		"BULLET_CHAR",
		"STRENGTH_CHAR",
		"MOVES_CHAR",
		"RELIGION_CHAR",
		"STAR_CHAR",
		"SILVER_STAR_CHAR",
		"TRADE_CHAR",
		"DEFENSE_CHAR",
		"GREAT_PEOPLE_CHAR",
		"BAD_GOLD_CHAR",
		"BAD_FOOD_CHAR",
		"EATEN_FOOD_CHAR",
		"GOLDEN_AGE_CHAR",
		"ANGRY_POP_CHAR",
		"OPEN_BORDERS_CHAR",
		"DEFENSIVE_PACT_CHAR",
		"MAP_CHAR",
		"OCCUPATION_CHAR",
		"POWER_CHAR",
		"CITIZEN_CHAR",
		"GREAT_GENERAL_CHAR",
		"AIRPORT_CHAR",
		"ANGRY_CIV_CHAR",
		"UNHAPPY_CIV_CHAR",
		"NORMAL_CIV_CHAR",
		"HAPPY_CIV_CHAR",
		"VERYHAPPY_CIV_CHAR",
		"CROSSED_CHAR",
		"RANDOM_CHAR"
	]
	#	Each row is (glyph, prefix, id) rather than an info OBJECT: the glyph is TEXT-plane (this manager's own
	#	symbol pass), while the button and type key are read back off the info surface by id.
	aList1 = []
	for i in range(GC.getNumReligionInfos()):
		aList1.append((TEXT.getSymbolChar("RELIGION_", i), "RELIGION_", i))

	for i in range(GC.getNumCorporationInfos()):
		aList1.append((TEXT.getSymbolChar("CORPORATION_", i), "CORPORATION_", i))

	szBonusClass = "BONUSCLASS_CULTURE"
	BONUSCLASS_CULTURE = GC.getInfoTypeForString(szBonusClass)
	bOnce = True
	for i in range(GC.getNumBonusInfos()):
		cGlyph = TEXT.getSymbolChar("BONUS_", i)
		if INFO.getIntrinsic("BONUS_", i, IntrinsicSlot.PYINT_BONUS_CLASS) == BONUSCLASS_CULTURE:
			#	The culture bonuses all share one glyph, so the atlas lists the CLASS once instead of every member.
			if bOnce:
				aList1.append((cGlyph, None, szBonusClass))
				bOnce = False
		else:
			aList1.append((cGlyph, "BONUS_", i))
	iMax = len(aList1)

	iRow = -1
	iID = 8482
	for _ in xrange(650): # Increase this range when needed, when it no longer displays all icons in GameFont.tga due to a content expansion in that atlas texture.
		iRow += 1
		iID += 1
		screen.appendTableRow(TABLE)
		screen.setTableText(TABLE, 0, iRow , str(iID), "", eWidGen, 1, 1, 1<<0)
		screen.setTableText(TABLE, 1, iRow , unichr(iID), "", eWidGen, 1, 1, 1<<0)
		screen.setTableText(TABLE, 2, iRow , u"<font=4>%c</font>" % iID, "", eWidGen, 1, 1, 1<<0)

		if iID >= iHappy and iID <= iRandom:
			screen.setTableText(TABLE, 4, iRow , aList0[iID - iHappy], "", eWidGen, 1, 2, 1<<0)
			continue
		bFound = False
		i = 0
		while i < iMax:
			if aList1[i][0] == iID:
				entry = aList1.pop(i)
				iMax -= 1
				if bFound:
					screen.appendTableRow(TABLE)
					iRow += 1
					screen.setTableText(TABLE, 2, iRow, u"<font=4>%c</font>" % iID, "", eWidGen, 1, 1, 1<<0)
				bFound = True
				if entry[1] is None:
					screen.setTableText(TABLE, 4, iRow, entry[2], "", eWidGen, 1, 2, 1<<0)
				else:
					screen.setTableText(TABLE, 3, iRow, "", INFO.getButton(entry[1], entry[2]), eWidGen, 1, 2, 1<<0)
					screen.setTableText(TABLE, 4, iRow, INFO.getType(entry[1], entry[2]), "", eWidGen, 1, 2, 1<<0)
			else:
				i += 1

	screen.showScreen(PopupStates.POPUPSTATE_IMMEDIATE, False)