## Sid Meier's Civilization 4
## Copyright Firaxis Games 2005
from CvPythonExtensions import *
import string

# globals
# The one data-fetching library: INFO = what an entity CARRIES, ENABLER = can I?, ENUMS = the engine
# enum vocabulary + name->id resolution. A game object's own data is asked OF THAT OBJECT --
# GC.getPlayer(i).getCity(id).getYields(), never a flat class keyed by (owner, id).
GC = CyGlobalContext()
INFO = CyInfo()
BUILDINFO = CyBuildInfo()
gc = GC   # this module spells it lowercase
ENABLER = CyEnabler()
ENUMS = CyEnums()
ArtFileMgr = CyArtFileMgr()
localText = CyTranslator()

TEXT = CyGameTextMgr()
class SevoPediaRoute:
	"Civilopedia Screen for tile Routes"

	def __init__(self, main):
		self.iRoute = -1
		self.top = main


		self.X_ROUTE_PANE = self.top.X_PEDIA_PAGE
		self.Y_ROUTE_PANE = self.top.Y_PEDIA_PAGE
		self.W_ROUTE_PANE = 300
		self.H_ROUTE_PANE = 116

		self.W_ICON = 100
		self.H_ICON = 100
		self.X_ICON = self.X_ROUTE_PANE + (self.H_ROUTE_PANE - self.H_ICON) / 2
		self.Y_ICON = self.Y_ROUTE_PANE + (self.H_ROUTE_PANE - self.H_ICON) / 2
		self.ICON_SIZE = 64
		self.BUTTON_SIZE = 64

		self.X_STATS_PANE = self.X_ROUTE_PANE + 125
		self.Y_STATS_PANE = self.Y_ROUTE_PANE + 10
		self.W_STATS_PANE = 290
		self.H_STATS_PANE = 200

		self.X_BONUS_YIELDS_PANE = self.X_ROUTE_PANE + self.W_ROUTE_PANE +5
		self.Y_BONUS_YIELDS_PANE = self.Y_ROUTE_PANE + 10
		self.W_BONUS_YIELDS_PANE = 275
		self.H_BONUS_YIELDS_PANE = 226

		self.X_REQUIRES_PANE = self.X_ROUTE_PANE
		self.Y_REQUIRES_PANE = self.Y_ROUTE_PANE + self.H_ROUTE_PANE + 10
		self.W_REQUIRES_PANE = 300
		self.H_REQUIRES_PANE = 110

		self.X_SPECIAL_PANE = self.X_REQUIRES_PANE
		self.Y_SPECIAL_PANE = self.Y_REQUIRES_PANE + self.H_REQUIRES_PANE + 10
		self.W_SPECIAL_PANE = 600
		self.H_SPECIAL_PANE = 195

		self.X_HISTORY_PANE = self.X_SPECIAL_PANE
		self.Y_HISTORY_PANE = self.Y_SPECIAL_PANE + self.H_SPECIAL_PANE + 10
		self.W_HISTORY_PANE = 600
		self.H_HISTORY_PANE = 195

	# Screen construction function
	def interfaceScreen(self, iRoute):
		self.iRoute = iRoute
		screen = self.top.getScreen()

		screen.addPanel(self.top.getNextWidgetName(), "", "", False, False, self.X_ROUTE_PANE, self.Y_ROUTE_PANE, self.W_ROUTE_PANE, self.H_ROUTE_PANE, PanelStyles.PANEL_STYLE_BLUE50)
		screen.addPanel(self.top.getNextWidgetName(), "", "", False, False, self.X_ICON, self.Y_ICON, self.W_ICON, self.H_ICON, PanelStyles.PANEL_STYLE_MAIN)
		screen.addDDSGFC(self.top.getNextWidgetName(), INFO.getButton("ROUTE_", self.iRoute), self.X_ICON + self.W_ICON/2 - self.ICON_SIZE/2, self.Y_ICON + self.H_ICON/2 - self.ICON_SIZE/2, self.ICON_SIZE, self.ICON_SIZE, WidgetTypes.WIDGET_GENERAL, -1, -1 )

		self.placeStats()
		self.placeRequires()
		self.placeImprovementYield()
		self.placeSpecial()
		self.placeHistory()

	def placeStats(self):
		screen = self.top.getScreen()
		panelName = self.top.getNextWidgetName()

		screen.addListBoxGFC(panelName, "", self.X_STATS_PANE, self.Y_STATS_PANE, self.W_STATS_PANE, self.H_STATS_PANE, TableStyles.TABLE_STYLE_EMPTY)
		screen.enableSelect(panelName, False)
		iFontSize = 3

		# The route's OWN plot-scope output -- a plot-substrate entity owns its plot yield (modifier.md par.4).
		# x100 like every amount, so the reduce happens here, at the read edge.
		aYields = INFO.getFlatYields("ROUTE_", self.iRoute, CascScope.CASC_SCOPE_PLOT)
		for k in range(YieldTypes.NUM_YIELD_TYPES):
			iYieldChange = aYields[k] / 100
			if (iYieldChange != 0):
				if (iYieldChange > 0):
					sign = "+"
				else:
					sign = ""
				szYield = (u"%s: %s%i " % (INFO.getDescription("YIELD_", k).upper(), sign, iYieldChange))
				screen.appendListBoxStringNoUpdate(panelName, u"<font=%d>" % iFontSize + szYield + (u"%c" % TEXT.getSymbolChar("YIELD_", k)) + u"</font>", WidgetTypes.WIDGET_GENERAL, 0, 0, 1<<0)

		screen.updateListBox(panelName)

	def placeImprovementYield(self):
		screen = self.top.getScreen()

		panelName = self.top.getNextWidgetName()
		screen.addPanel( panelName, localText.getText("TXT_KEY_PEDIA_ROUTE_IMPROVEMENT_YIELD_CHANGE", ()), "", True, True,
				 self.X_BONUS_YIELDS_PANE, self.Y_BONUS_YIELDS_PANE, self.W_BONUS_YIELDS_PANE, self.H_BONUS_YIELDS_PANE, PanelStyles.PANEL_STYLE_BLUE50 )

		szYield = u""
		# ⛔ THIS PANEL IS DELIBERATELY EMPTY, and the emptiness is TRUTHFUL rather than a missing read.
		# A route's per-improvement yield is governing-deliverer data authored ON THE ROUTE keyed by
		# improvement (modifier.md par.4). A KEYED entry never folds scope-wide (par.5), and the plot's route leg
		# reads the route's UNTARGETED output -- so that authored data currently reaches no plot yield at all.
		# Rendering it here would promise the player a yield the game does not apply.
		# ⚠ The legacy form asked every improvement in the registry what this route gave it, which was both the
		# own-data inversion [DEC-one-reverse-view] bans AND a read of the improvement side of a relationship
		# the route owns. Serving it needs the improvement's FK passed beside its modifiers so the route's keyed
		# entry can resolve -- the tracked todo item, not something to invent at a pedia call site
		# ([DEC-no-legacy-masking]: the hole shows rather than being papered over).
		bImprovementYieldChange = False

		if bImprovementYieldChange == False:
			szYield += localText.getText("TXT_KEY_PEDIA_NO_PLOT_YIELD_CHANGE", ())

			listName = self.top.getNextWidgetName()
			screen.addMultilineText(listName, szYield, self.X_BONUS_YIELDS_PANE+5, self.Y_BONUS_YIELDS_PANE+30, self.W_BONUS_YIELDS_PANE-10, self.H_BONUS_YIELDS_PANE-35, WidgetTypes.WIDGET_GENERAL, -1, -1, 1<<0)

		return

	def placeRequires(self):

		screen = self.top.getScreen()

		panelName = self.top.getNextWidgetName()
		screen.addPanel( panelName, localText.getText("TXT_KEY_PEDIA_REQUIRES", ()), "", False, True,
				 self.X_REQUIRES_PANE, self.Y_REQUIRES_PANE, self.W_REQUIRES_PANE, self.H_REQUIRES_PANE, PanelStyles.PANEL_STYLE_BLUE50 )

		screen.attachLabel(panelName, "", "  ")

		# The techs behind the builds that LAY this route. The builds are the route's own reverse edge family
		# (a build's `produces.route` is landed onto the route at load), so this no longer sweeps every build
		# asking what it produces ([DEC-one-reverse-view]).
		aTechList = []
		for iBuild in INFO.getEdgeIds("ROUTE_", self.iRoute, EdgeFamily.EDGEF_RELATED, EdgeBucket.EDGEB_BUILDS):
			iTech = BUILDINFO.getTechPrereq(iBuild)
			if (iTech > -1):
				if not iTech in aTechList:
					aTechList.append(iTech)
		for i in aTechList:
			screen.attachImageButton( panelName, "", INFO.getButton("TECH_", i), GenericButtonSizes.BUTTON_SIZE_46, WidgetTypes.WIDGET_PEDIA_JUMP_TO_TECH, i, 2, False )

		# The route's bonus prereqs, read PER CLAUSE so the mandatory one and the one-of group stay apart --
		# which is exactly the AND/OR this panel draws. REQCLAUSE_NONE is never asked for: a `noneOf` names
		# what BARS the route, and listing it under "Requires" would invert its meaning.
		aReqAll = INFO.getRequiresIdsInClause("ROUTE_", self.iRoute, EdgeBucket.EDGEB_BONUSES, RequiresClause.REQCLAUSE_ALL)
		aReqAny = INFO.getRequiresIdsInClause("ROUTE_", self.iRoute, EdgeBucket.EDGEB_BONUSES, RequiresClause.REQCLAUSE_ANY)
		bFirst = True
		for iPrereq in aReqAll:
			bFirst = False
			screen.attachImageButton(panelName, "", INFO.getButton("BONUS_", iPrereq), GenericButtonSizes.BUTTON_SIZE_46, WidgetTypes.WIDGET_PEDIA_JUMP_TO_BONUS, iPrereq, 2, False)
		nOr = len(aReqAny)
		szLeftDelimeter = ""
		szRightDelimeter = ""
		if (not bFirst):
			if (nOr > 1):
				szLeftDelimeter = localText.getText("TXT_KEY_AND", ()) + "("
				szRightDelimeter = ") "
			elif (nOr > 0):
				szLeftDelimeter = localText.getText("TXT_KEY_AND", ())
		if len(szLeftDelimeter) > 0:
			screen.attachLabel(panelName, "", szLeftDelimeter)
		bFirst = True
		for eBonus in aReqAny:
			if not bFirst:
				screen.attachLabel(panelName, "", localText.getText("TXT_KEY_OR", ()))
			else:
				bFirst = False
			screen.attachImageButton(panelName, "", INFO.getButton("BONUS_", eBonus), GenericButtonSizes.BUTTON_SIZE_46, WidgetTypes.WIDGET_PEDIA_JUMP_TO_BONUS, eBonus, -1, False)
		if len(szRightDelimeter) > 0:
			screen.attachLabel(panelName, "", szRightDelimeter)

	def placeSpecial(self):

		screen = self.top.getScreen()

		panelName = self.top.getNextWidgetName()
		screen.addPanel( panelName, localText.getText("TXT_KEY_PEDIA_SPECIAL_ABILITIES", ()), "", True, False, self.X_SPECIAL_PANE, self.Y_SPECIAL_PANE, self.W_SPECIAL_PANE, self.H_SPECIAL_PANE, PanelStyles.PANEL_STYLE_BLUE50 )
		listName = self.top.getNextWidgetName()
		screen.attachListBoxGFC( panelName, listName, "", TableStyles.TABLE_STYLE_EMPTY )
		screen.enableSelect(listName, False)
		szSpecialText = CyGameTextMgr().getRouteHelp(self.iRoute, True)
		splitText = string.split( szSpecialText, "\n" )
		for special in splitText:
			if len( special ) != 0:
				screen.appendListBoxString( listName, special, WidgetTypes.WIDGET_GENERAL, -1, -1, 1<<0 )

	def placeHistory(self):

		screen = self.top.getScreen()

		panelName = self.top.getNextWidgetName()
		screen.addPanel(panelName, CyTranslator().getText("TXT_KEY_PEDIA_CATEGORY_CONCEPT", ()), "", True, False, self.X_HISTORY_PANE, self.Y_HISTORY_PANE, self.W_HISTORY_PANE, self.H_HISTORY_PANE, PanelStyles.PANEL_STYLE_BLUE50 )
		screen.addMultilineText(panelName, CyTranslator().getText("TXT_KEY_CONCEPT_MOVEMENT_PEDIA", ()), self.X_HISTORY_PANE+10, self.Y_HISTORY_PANE + 30, self.W_HISTORY_PANE -20, self.H_HISTORY_PANE- 55, WidgetTypes.WIDGET_GENERAL, -1, -1, 1<<0)

	def placeLinks(self, bRedraw):

		screen = self.top.getScreen()

		if bRedraw:
			screen.clearListBoxGFC(self.top.LIST_ID)

		# The whole route registry in ONE crossing -- the per-type INDEX read carries the description and the
		# art-only marker together, so the list no longer pays a boundary call per entity to ask each one.
		# ⚑ Enumerating a registry to LIST every entity is the pedia's own job and is the sanctioned full scan
		# (patterns.md); what changed is the cost of the crossing, not the shape.
		rowListName = []
		for kRoute in INFO.getIndex("ROUTE_"):
			if not kRoute["graphicalOnly"]:
				rowListName.append((kRoute["description"], kRoute["id"]))
		rowListName.sort()

		iSelected = 0
		i = 0
		for szName, iRouteId in rowListName:
			if bRedraw:
				screen.appendListBoxString(self.top.LIST_ID, szName, WidgetTypes.WIDGET_HELP_MOVE_BONUS, iRouteId, 0, 1<<0)
			if iRouteId == self.iRoute:
				iSelected = i
			i += 1

		screen.setSelectedListBoxStringGFC(self.top.LIST_ID, iSelected)


	# Will handle the input for this screen...
	def handleInput (self, inputClass):
		return 0


