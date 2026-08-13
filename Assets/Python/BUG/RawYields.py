## RawYields
##
## Calculates the raw yields of food, production and commerce for a city
## and displays them in the trade table when enabled.
##
## Copyright (c) 2008 The BUG Mod.
##
## Author: EmperorFool

from CvPythonExtensions import *
TRNSLTR = CyTranslator()

# The one data-fetching library ([DEC-cy-not-fixed]): ENABLER = availability,
# ENUMS = the engine enum vocabulary + name->id resolution.
GC = CyGlobalContext()
ENABLER = CyEnabler()
ENUMS = CyEnums()

TEXT = CyGameTextMgr()
# Types
NUM_TYPES = 10
(
	WORKED_TILES,
	CITY_TILES,
	OWNED_TILES,
	ALL_TILES,

	DOMESTIC_TRADE,
	FOREIGN_TRADE, # excludes overseas trade

	BUILDINGS,
	CORPORATIONS,
	SPECIALISTS,

	# Hold the percents, not the actual yield values
	BASE_MODIFIER,
) = xrange(NUM_TYPES)

# Leave these for later when we have icons for each
#DOMAIN_MODIFIER
#MILITARY_MODIFIER
#TRAIT_MODIFIER
#CIVIC_MODIFIER
#RELIGION_MODIFIER
#BONUS_MODIFIER
#WONDER_MODIFIER
#...

# Labels
LABEL_KEYS = (
	"TXT_KEY_CONCEPT_WORKED_TILES",
	"TXT_KEY_CONCEPT_CITY_TILES",
	"TXT_KEY_CONCEPT_OWNED_TILES",
	"TXT_KEY_CONCEPT_ALL_TILES",
	"TXT_KEY_CONCEPT_DOMESTIC_TRADE",
	"TXT_KEY_CONCEPT_FOREIGN_TRADE",
	"TXT_KEY_WB_BUILDINGS",
	"TXT_KEY_CONCEPT_CORPORATIONS",
	"TXT_KEY_CONCEPT_SPECIALISTS",
	"TXT_KEY_CONCEPT_BASE_MODIFIER"
)
# The census term ORDER, as CyState::getCityYieldTerms returns it. One list, one order, both sides.
NUM_TERMS = 12
(
	T_PLOT_BASE,
	T_PLOT_NATURE,
	T_PLOT_IMPROVEMENT,
	T_PLOT_REST,
	T_TRADE_YIELD,
	T_GOLDEN_AGE,
	T_UPPER_FLAT,
	T_SPECIALISTS,
	T_CITY_FLAT,
	T_PERCENT_SUM,
	T_WORKED_PLOTS,
	T_RATE,
) = xrange(NUM_TERMS)

# Yields
YIELDS = (YieldTypes.YIELD_FOOD, YieldTypes.YIELD_PRODUCTION, YieldTypes.YIELD_COMMERCE)
# Tiles
TILES = (WORKED_TILES, CITY_TILES, OWNED_TILES, ALL_TILES)

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

	def __init__(self):
		# Creates a table to hold all of the tracked values for each yield type.
		self.values = {}
		for eYield in xrange(YieldTypes.NUM_YIELD_TYPES):
			self.values[eYield] = {}
			for eType in xrange(NUM_TYPES):
				self.values[eYield][eType] = 0
		self.tileCounts = [0, 0, 0, 0]
		self.terms = {}

	def addBuilding(self, eYield, iValue):
		self.values[eYield][BUILDINGS] += iValue

	def addDomesticTrade(self, eYield, iValue):
		self.values[eYield][DOMESTIC_TRADE] += iValue

	def addForeignTrade(self, eYield, iValue):
		self.values[eYield][FOREIGN_TRADE] += iValue

	def processCity(self, iPlayer, iCityID):
		"""
		Reads the city's yield CENSUS -- the same decomposition InfoValuation::cityReceiverRate produces and the
		/computed census renders. Nothing is calculated here.

		â A tooltip IS a census (owner), so this panel and the served document must be the SAME answer or they
		are two answers to one question. This used to hand-roll its own walk over plots, specialists and
		corporations through the CyCity API -- an API that no longer exists, which is why it raised.
		"""
		self.terms = {}
		for eYield in YIELDS:
			self.terms[eYield] = GC.getPlayer(iPlayer).getCity(iCityID).getYieldTerms(eYield)

	def term(self, eYield, iTerm):
		# x100 amounts come back as whole game numbers here; percentSum / workedPlots are already whole
		# ([DEC-fixedpoint-x100]: the reader divides at the point of use).
		terms = self.terms.get(eYield)
		if not terms or iTerm >= len(terms):
			return 0
		return int(terms[iTerm])

	def fillTable(self, screen, table, eYield, eType):
		# Renders the CENSUS terms. â The rows are what the cascade actually computes, not the old hand-rolled
		# buckets: the census walks the city's WORKED plots, so "city / owned / all tiles" have no term and are
		# not shown. Altered visible text is never a reason to hesitate here ([patterns.md]) -- what matters is
		# that the panel and the served census agree.
		TRNSLTR = CyTranslator()
		self.iRow = 0

		# The worked-plot base, with its three segments -- a short plot yield cannot be attributed from the total
		# alone (a dead improvement leg and a dead nature leg look identical in it).
		iPlotBase = self.term(eYield, T_PLOT_BASE) / 100
		self.appendTable(screen, table, False,
			TRNSLTR.getText(LABEL_KEYS[WORKED_TILES], (self.term(eYield, T_WORKED_PLOTS),)), eYield, iPlotBase)
		for iTerm, szKey in ((T_PLOT_NATURE, "TXT_KEY_CONCEPT_NATURE"),
		                     (T_PLOT_IMPROVEMENT, "TXT_KEY_CONCEPT_IMPROVEMENT"),
		                     (T_PLOT_REST, "TXT_KEY_CONCEPT_OTHER")):
			iValue = self.term(eYield, iTerm) / 100
			if iValue:
				self.appendTable(screen, table, False, u"    " + TRNSLTR.getText(szKey, ()), eYield, iValue)
		iTotal = iPlotBase

		# Trade -- the SCREEN supplies the domestic/foreign split, which the census does not carry; the census's
		# own tradeYield is the same money and is deliberately not added beside it.
		for eTradeType in (DOMESTIC_TRADE, FOREIGN_TRADE):
			iValue = self.values[eYield][eTradeType]
			if iValue:
				self.appendTable(screen, table, False, TRNSLTR.getText(LABEL_KEYS[eTradeType], ()), eYield, iValue, True)
		iTotal += (self.values[eYield][DOMESTIC_TRADE] + self.values[eYield][FOREIGN_TRADE]) / 100

		# The remaining TIER-1 flats the percent stack multiplies, then the city's own TIER-2 flats.
		for iTerm, szKey in ((T_SPECIALISTS, "TXT_KEY_CONCEPT_SPECIALISTS"),
		                     (T_UPPER_FLAT, "TXT_KEY_WB_BUILDINGS"),
		                     (T_GOLDEN_AGE, "TXT_KEY_CONCEPT_GOLDEN_AGE"),
		                     (T_CITY_FLAT, "TXT_KEY_WB_BUILDINGS")):
			iValue = self.term(eYield, iTerm) / 100
			if iValue:
				iTotal += iValue
				self.appendTable(screen, table, False, TRNSLTR.getText(szKey, ()), eYield, iValue)

		# The single additive percent stack.
		iModifier = self.term(eYield, T_PERCENT_SUM)
		if iModifier:
			self.appendTableTotal(screen, table, eYield, iTotal)
			iValue = (iTotal * (iModifier + 100) // 100) - iTotal
			self.appendTable(screen, table, False,
				TRNSLTR.getText("TXT_KEY_CONCEPT_BASE_MODIFIER", (iModifier,)), eYield, iValue)

		# â The TOTAL is the census's OWN rate, never this function's running sum. The combine is the
		# authority; a total re-added here would be a second answer, and the two would drift the moment a term
		# is added to the cascade and not to this loop.
		self.appendTableTotal(screen, table, eYield, self.term(eYield, T_RATE) / 100)

	def appendTable(self, screen, table, bTotal, heading, eYield, iValue, bFraction=False):
		"""
		Appends the given yield value to the table control.
		If bTotal is True, the heading is colored yellow and there's no + sign on the value.
		"""
		cYield = TEXT.getSymbolChar("YIELD_", eYield)
		screen.appendTableRow(table)
		if bTotal:
			heading = u"<color=205,180,55,255>%s</color>" % heading
			value = u"<color=205,180,55,255>%d</color>" % iValue
			if bFraction:
				# showing fraction doesn't fit in column
				value = u"<color=205,180,55,255>%d</color>" % (iValue / 100)
			else:
				value = u"<color=205,180,55,255>%d</color>" % iValue
		else:
			if bFraction:
				# showing fraction doesn't fit in column
				value = u"%+d" % (iValue / 100)
			else:
				value = u"%+d" % iValue
		screen.setTableText(table, 0, self.iRow, u"<font=1>%s</font>" % (heading), "", WidgetTypes.WIDGET_GENERAL, -1, -1, 1<<0)
		screen.setTableText(table, 1, self.iRow, u"<font=1>%s%c</font>" % (value, cYield), "", WidgetTypes.WIDGET_GENERAL, -1, -1, 1<<1)
		self.iRow += 1

	def appendTableTotal(self, screen, table, eYield, iValue):
		"""
		Appends the given yield total to the table control's 3rd running total column.
		"""
		if self.iRow > 0:
			cYield = TEXT.getSymbolChar("YIELD_", eYield)
			value = u"<color=205,180,55,255>%d</color>" % iValue
			screen.setTableText(table, 2, self.iRow - 1, u"<font=1>%s%c</font>" % (value, cYield), "", WidgetTypes.WIDGET_GENERAL, -1, -1, 1<<1)