from CvPythonExtensions import *
GC = CyGlobalContext()
INFO = CyInfo()
#	The era pacing this screen tabulates belongs to ONE registry, so it is read from the per-info GAMESPEED
#	accessor rather than a generic slot ([patterns.md] THE PYTHON READ BOUNDARY -- an explicit binding at module
#	scope IS the module's dependency list).
SPEED = CyGameSpeedInfo()
TRNSLTR = CyTranslator()

def TimeKeeper():
	import CvScreenEnums
	import ScreenResolution
	xRes = ScreenResolution.x
	yRes = ScreenResolution.y
	GC = CyGlobalContext()
	TRNSLTR = CyTranslator()
	# Cache Era data
	iEras = GC.getNumEraInfos()
	szColorEra = "<font=3>" + TRNSLTR.getText("[COLOR_UNIT_TEXT]", ())
	#	Keyed by the entry's OWN id, never by position: getIndex walks to the registry's real count and skips a
	#	hole, so a positional list would run short and the rows below index by era id.
	aListEra = {}
	for kEra in INFO.getIndex("C2C_ERA_"):
		aListEra[kEra["id"]] = szColorEra + kEra["description"]
	# Create table
	screen = CyGInterfaceScreen("TimeKeeperScreen", CvScreenEnums.TIMEKEEPER)
	screen.addPanel("", "", "", True, False, -10, -10, xRes + 20, yRes + 20, PanelStyles.PANEL_STYLE_MAIN)
	screen.showScreen(PopupStates.POPUPSTATE_IMMEDIATE, False)

	iNumColumns = GC.getNumGameSpeedInfos()
	TABLE = "TimeKeeperTable"
	screen.addTableControlGFC(TABLE, iNumColumns + 1, 0, 0, xRes, yRes, True, False, 24, 24, TableStyles.TABLE_STYLE_STANDARD )
	screen.setTableColumnHeader(TABLE, 0, "", 120)
	for i in xrange(iNumColumns):
		screen.setTableColumnHeader(TABLE, i + 1, INFO.getDescription("GAMESPEED_", i), (xRes - 120)/iNumColumns)

	for _ in xrange(5 * iEras + 3):
		screen.appendTableRow(TABLE)

	# Cache misc content
	szColorSel = TRNSLTR.getText("[COLOR_SELECTED_TEXT]", ())
	szStartYear = "<font=3>" + TRNSLTR.getText("TXT_KEY_WB_START_YEAR", ())
	szTurns = "<font=3>" + TRNSLTR.getText("TXT_KEY_TURNS", ())
	szTurnsCol = szColorSel + szTurns
	szColorSel += "<font=3>"
	szIncrement = "<font=3>" + TRNSLTR.getText("TXT_KEY_INCREMENT", ())
	szDuration = "<font=3>" + TRNSLTR.getText("TXT_KEY_DURATION", ())
	szEndYear = szColorSel + TRNSLTR.getText("TXT_KEY_END_YEAR", ())
	eCalNoSeasons = CalendarTypes.CALENDAR_NO_SEASONS
	eWidGen = WidgetTypes.WIDGET_GENERAL
	iStartYear = GC.getGame().getStartYear()
	# Fill table: one block of rows per era, one column per game speed
	iCol = 0
	while iCol < iNumColumns:
		iRow = 0
		for iEra in xrange(iEras):
			iStartTurn = SPEED.getEraStartTurn(iCol, iEra)
			iTurns = SPEED.getTurnsInEra(iCol, iEra)
			iIncrement = SPEED.getTicksPerTurnInEra(iCol, iEra)
			screen.setTableText(TABLE, 0, iRow, aListEra.get(iEra, ""), "", eWidGen, 1, 2, 1<<0)
			iRow += 1
			screen.setTableText(TABLE, 0, iRow, szStartYear, "", eWidGen, 1, 2, 1<<0)
			screen.setTableText(TABLE, iCol+1, iRow, "<font=3>" + CyGameTextMgr().getDateStr(iStartTurn, False, eCalNoSeasons, iStartYear, iCol), "", eWidGen, 1, 2, 1<<0)
			iRow += 1
			screen.setTableText(TABLE, 0, iRow, szTurns, "", eWidGen, 1, 2, 1<<0)
			screen.setTableText(TABLE, iCol+1, iRow, "<font=3>" + str(iTurns), "", eWidGen, 1, 2, 1<<0)
			iRow += 1
			screen.setTableText(TABLE, 0, iRow, szIncrement, "", eWidGen, 1, 2, 1<<0)
			screen.setTableText(TABLE, iCol+1, iRow, "<font=3>" + separateYearMonthDay(iIncrement), "", eWidGen, 1, 2, 1<<0)
			iRow += 1
			screen.setTableText(TABLE, 0, iRow, szDuration, "", eWidGen, 1, 2, 1<<0)
			screen.setTableText(TABLE, iCol+1, iRow, "<font=3>" + separateYearMonthDay(iTurns * iIncrement), "", eWidGen, 1, 2, 1<<0)
			iRow += 1

		iTotalTurns = SPEED.getTotalTurns(iCol)
		iRow += 1
		screen.setTableText(TABLE, 0, iRow, szEndYear, "", eWidGen, 1, 2, 1<<0)
		screen.setTableText(TABLE, iCol+1, iRow, szColorSel + CyGameTextMgr().getDateStr(iTotalTurns, False, eCalNoSeasons, iStartYear, iCol), "", eWidGen, 1, 2, 1<<0)
		iRow += 1
		screen.setTableText(TABLE, 0, iRow, szTurnsCol, "", eWidGen, 1, 2, 1<<0)
		screen.setTableText(TABLE, iCol+1, iRow, szColorSel + str(iTotalTurns), "", eWidGen, 1, 2, 1<<0)
		iCol += 1

def separateYearMonthDay(iValue):
	iValueYear = iValue / 360
	iValueMonth = (iValue % 360)/30
	iValueDay = (iValue % 360) % 30
	sValue = ""
	if iValueYear > 0:
		sValue += str(iValueYear) + "y "
	if iValueMonth > 0:
		sValue += str(iValueMonth) + "m "
	if iValueDay > 0:
		sValue += str(iValueDay) + "d"
	return sValue
