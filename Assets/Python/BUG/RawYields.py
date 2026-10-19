## RawYields
##
## Displays the raw yields of food, production and commerce for a city in the trade table when enabled.
##
## Copyright (c) 2008 The BUG Mod.
##
## Author: EmperorFool

from CvPythonExtensions import *

# The one data-fetching library: a game object's own data is asked OF THAT OBJECT --
# GC.getPlayer(i).getCity(id).getYieldBreakdownRows(), never a flat class keyed by (owner, id).
GC = CyGlobalContext()

# Yields
YIELDS = (YieldTypes.YIELD_FOOD, YieldTypes.YIELD_PRODUCTION, YieldTypes.YIELD_COMMERCE)

TOTAL_COLOR = u"<color=205,180,55,255>%s</color>"

def getViewAndType(iView):
	# Returns the view boolean and YieldTypes enum given the give number 0-3.
	if iView == 0:
		return (False, YieldTypes.YIELD_FOOD)
	elif iView in (1, 2, 3):
		return (True, YIELDS[iView - 1])
	else:
		print "RawYields - invalid view-type: %d" %iView
		return (False, YieldTypes.YIELD_FOOD)

class Tracker:

	def fillTable(self, screen, table, iPlayer, iCityID, eYield):
		# The rows are the engine's yield breakdown, one per line, labelled and formatted by the city hover's
		# renderer; the subtotals and the total are coloured. The table scrolls, so every line has its row.
		iRow = 0
		for iRole, szLabel, szValue in GC.getPlayer(iPlayer).getCity(iCityID).getYieldBreakdownRows(eYield):
			if iRole in (CityBreakdownRole.CITY_BREAKDOWN_SUBTOTAL, CityBreakdownRole.CITY_BREAKDOWN_TOTAL):
				szLabel = TOTAL_COLOR % szLabel
				szValue = TOTAL_COLOR % szValue
			screen.appendTableRow(table)
			screen.setTableText(table, 0, iRow, u"<font=1>%s</font>" % szLabel, "", WidgetTypes.WIDGET_GENERAL, -1, -1, 1<<0)
			screen.setTableText(table, 1, iRow, u"<font=1>%s</font>" % szValue, "", WidgetTypes.WIDGET_GENERAL, -1, -1, 1<<1)
			iRow += 1
