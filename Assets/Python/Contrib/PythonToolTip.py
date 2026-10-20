from CvPythonExtensions import *

import ScreenResolution as SR

CyIF = CyInterface()

# The one hover's font: one size above the game's own tooltips (Font_Size1_Normal), so the text holds against
# the box's translucent background. Every width the box is sized by is measured in it.
TOOLTIP_FONT = "<font=2>"


class PythonToolTip:
	def __init__(self):
		self.szTextTT = None
		self.iOffsetTT = [0, 0]
		self.bLockedTT = False
		# The screen's area on the display (setScreenArea); None for a screen that covers the display.
		self.aArea = None

	def handle(self, screen, szText = None, xPos = -1, yPos = -1):
		if not szText:
			# None means something, while sending in an empty string is a fluke.
			if szText is None:
				followCursor(screen, "Tooltip", self.iOffsetTT, self.aArea)
			return

		if szText != self.szTextTT:
			self.szTextTT = szText
			iX, iY = makeTooltip(screen, xPos, yPos, szText, "Tooltip", self.aArea)
			xCursor, yCursor = cursorOnScreen(self.aArea)
			self.iOffsetTT = [iX - xCursor, iY - yCursor]

		else: # Already cached, move tooltip to cursor and make it visible
			if xPos == yPos == -1:
				followCursor(screen, "Tooltip", self.iOffsetTT, self.aArea)
			screen.moveToFront("Tooltip")
			screen.show("Tooltip")

		self.bLockedTT = xPos == yPos == -1


	def reset(self, screen):
		screen.hide("Tooltip")
		self.bLockedTT = False
		# The next hover draws afresh: by then the screen may have been rebuilt, or be another screen of the same object.
		self.szTextTT = None

# The one hover (docs/reference/tooltip-look.md § THE ONE HOVER). The game asks the DLL for a widget's hover text
# once, when the mouse arrives on it; the DLL hands that text to showWidgetHover, and the screen whose widget the
# mouse is in draws it. Every screen's input passes noteHoverInput (CvScreensInterface.handleInput), so that
# screen is known, and the box goes when the mouse leaves. A screen answers getScreen(); one that draws its own
# hovers answers showHover(szText) / hideHover() instead.
g_hoverScreen = None
# The silent-area screens (§ THE ONE HOVER, Hover state), the one last opened at the end.
g_silentAreaScreens = []
# The silent-area screen whose box is shown, the cursor on that screen when the box was drawn, and the size of one
# widget in the rectangle it was drawn for.
g_silentHoverScreen = None
g_silentHoverAnchor = None
g_silentHoverCell = None
SILENT_AREA_MARGIN = 4
# The Build List screen's two dropdown rows are this far apart.
DROPDOWN_HEIGHT = 30
# True while a screen builds its button lists (buildButtonLists).
g_bBuildingButtonLists = False
g_hoverTooltips = {}
# The size makeTooltip last drew each box at, so a box that follows the cursor stays inside its screen.
g_boxSizes = {}

def screenArea(aArea):
	# The area a box keeps inside, as (left, top, width, height) on the display: a registered screen's, or the display.
	if aArea is None:
		return 0, 0, SR.x, SR.y
	return aArea

def cursorOnScreen(aArea):
	# The cursor in the coordinates of the screen whose area is aArea.
	iLeft, iTop = screenArea(aArea)[:2]
	POINT = Win32.getCursorPos()
	return POINT.x - iLeft, POINT.y - iTop

def cursorOnScreenOf(screenObject):
	# The cursor in the coordinates of a screen that draws in its PythonToolTip (hoverTooltip).
	return cursorOnScreen(hoverTooltip(screenObject).aArea)

def followCursor(screen, listBox, aOffset, aArea=None):
	# Moves the box to the cursor at the offset it was drawn with, kept inside its screen on every side.
	iWidth, iHeight = screenArea(aArea)[2:]
	xCursor, yCursor = cursorOnScreen(aArea)
	dx, dy = g_boxSizes.get(listBox, (0, 0))
	iX = min(max(xCursor + aOffset[0], 0), max(iWidth - dx, 0))
	iY = min(max(yCursor + aOffset[1], 0), max(iHeight - dy, 0))
	screen.moveItem(listBox, iX, iY, 0)

def setScreenArea(screenObject, iX, iY, iWidth, iHeight):
	# The area a screen was placed at with setDimensions (§ THE ONE HOVER, On screen).
	hoverTooltip(screenObject).aArea = (iX, iY, iWidth, iHeight)

def buildButtonLists(fBuild, *args):
	# Runs fBuild(*args), which builds a screen's button lists; the text requests it causes are not hovers
	# (§ THE ONE HOVER, Button lists).
	global g_bBuildingButtonLists
	bOuterBuild = g_bBuildingButtonLists
	g_bBuildingButtonLists = True
	try:
		return fBuild(*args)
	finally:
		g_bBuildingButtonLists = bOuterBuild

def noteSilentArea(screenObject):
	# A screen notes itself as it opens; one entry per screen class, so a replaced screen object drops its old one.
	for otherScreen in g_silentAreaScreens[:]:
		if otherScreen.__class__ is screenObject.__class__:
			g_silentAreaScreens.remove(otherScreen)
	g_silentAreaScreens.append(screenObject)

def silentRectUnderCursor(screenObject):
	# The rectangle of screenObject.silentAreaRects() the cursor is over, or None.
	x, y = cursorOnScreenOf(screenObject)
	for aRect in screenObject.silentAreaRects():
		xRect, yRect, iWidth, iHeight = aRect[:4]
		if xRect - SILENT_AREA_MARGIN <= x < xRect + iWidth + SILENT_AREA_MARGIN and yRect - SILENT_AREA_MARGIN <= y < yRect + iHeight + SILENT_AREA_MARGIN:
			return aRect
	return None

def silentAreaUnderCursor():
	# The screen and rectangle under the cursor, or (None, None).
	for screenObject in reversed(g_silentAreaScreens):
		aRect = silentRectUnderCursor(screenObject)
		if aRect is not None:
			return screenObject, aRect
	return None, None

def pollSilentArea():
	# Called from every silent-area screen's update (§ THE ONE HOVER, Hover state).
	global g_silentHoverScreen
	if g_silentHoverScreen is None:
		return
	x, y = cursorOnScreenOf(g_silentHoverScreen)
	xAnchor, yAnchor = g_silentHoverAnchor
	iCellWidth, iCellHeight = g_silentHoverCell
	if silentRectUnderCursor(g_silentHoverScreen) is None or abs(x - xAnchor) > iCellWidth or abs(y - yAnchor) > iCellHeight:
		hideHover(g_silentHoverScreen)
		g_silentHoverScreen = None

def noteHoverInput(screenObject, inputClass):
	global g_hoverScreen, g_silentHoverScreen
	iCode = inputClass.eNotifyCode
	if iCode == NotifyCode.NOTIFY_CURSOR_MOVE_ON:
		g_hoverScreen = screenObject
		g_silentHoverScreen = None
	elif iCode == NotifyCode.NOTIFY_CURSOR_MOVE_OFF and g_hoverScreen is screenObject:
		hideHover(screenObject)
		g_hoverScreen = None

def noteScreenClosed(screenObject):
	# A screen closed under the cursor sends no MOVE_OFF, and its getScreen() need not survive the close.
	global g_hoverScreen
	if g_hoverScreen is screenObject:
		g_hoverScreen = None

def leaveAll():
	# Ending the turn freezes the screen, so no arrive or leave survives it: the mouse has left whatever it was on.
	global g_hoverScreen, g_silentHoverScreen
	if g_hoverScreen is not None:
		hideHover(g_hoverScreen)
	if g_silentHoverScreen is not None:
		hideHover(g_silentHoverScreen)
	g_hoverScreen = None
	g_silentHoverScreen = None

def showWidgetHover(szText):
	# True when a screen drew the text; the DLL then leaves the game's own box empty. The game also asks for every
	# action button's text when the action bar is rebuilt, wherever the mouse is, so a request never grants the
	# hover: the screen the mouse arrived on draws it, or a silent area when the cursor is over it now. A request
	# while button lists are built leaves the text with the game (§ THE ONE HOVER, Button lists).
	global g_hoverScreen, g_silentHoverScreen, g_silentHoverAnchor, g_silentHoverCell
	if g_bBuildingButtonLists:
		return False
	screenObject = g_hoverScreen
	if screenObject is None:
		screenObject, aRect = silentAreaUnderCursor()
		if screenObject is not None:
			if g_silentHoverScreen not in (None, screenObject):
				hideHover(g_silentHoverScreen)
			g_silentHoverScreen = screenObject
			g_silentHoverAnchor = cursorOnScreenOf(screenObject)
			g_silentHoverCell = aRect[4:]
	if screenObject is None:
		return False
	if hasattr(screenObject, "showHover"):
		screenObject.showHover(szText)
		return True
	if not hasattr(screenObject, "getScreen"):
		return False
	screen = screenObject.getScreen()
	# A screen closed under the cursor never sent its MOVE_OFF; the game's own box shows the text then.
	if not screen.isActive():
		g_hoverScreen = None
		return False
	hoverTooltip(screenObject).handle(screen, szText)
	return True

def hideHover(screenObject):
	if hasattr(screenObject, "hideHover"):
		screenObject.hideHover()
	elif hasattr(screenObject, "getScreen"):
		hoverTooltip(screenObject).reset(screenObject.getScreen())

def hoverTooltip(screenObject):
	# The screen's own PythonToolTip when it has one, so one box object serves all of that screen's hovers.
	tooltip = getattr(screenObject, "tooltip", None)
	if isinstance(tooltip, PythonToolTip):
		return tooltip
	key = id(screenObject)
	if key not in g_hoverTooltips:
		g_hoverTooltips[key] = PythonToolTip()
	return g_hoverTooltips[key]


def fitLines(aLines, aRuns, szMoreLine):
	# The lines the one hover shows of an engine breakdown on this screen: all of them when the columns fit them;
	# otherwise the largest runs of sources each become the one summary line the engine made for it, as few as it
	# takes, and szMoreLine closes the list. aRuns holds (firstLine, lineCount, summary) per run.
	iMaxRows = (SR.y - 48) / 22
	dxLongest = 1
	for aLine in aLines:
		dxLongest = max(dxLongest, CyIF.determineWidth(TOOLTIP_FONT + aLine))
	iMaxColumns = max(1, min(8, (SR.x - 16) / (dxLongest + 28)))
	iCapacity = iMaxRows * iMaxColumns
	aBySize = list(aRuns)
	aBySize.sort(lambda aLeft, aRight: cmp(aRight[1], aLeft[1]))
	aCollapsed = {}
	iShown = len(aLines)
	for iFirst, iCount, szSummary in aBySize:
		if iShown <= iCapacity:
			break
		if not aCollapsed:
			iShown += 1 # szMoreLine
		aCollapsed[iFirst] = (iCount, szSummary)
		iShown -= iCount - 1
	if not aCollapsed:
		return list(aLines)
	aShown = []
	i = 0
	while i < len(aLines):
		if i in aCollapsed:
			iCount, szSummary = aCollapsed[i]
			aShown.append(szSummary)
			i += iCount
		else:
			aShown.append(aLines[i])
			i += 1
	aShown.append(szMoreLine)
	return aShown


def wrapLines(aWidthList, dx, uFont):
	# Breaks every line wider than dx into lines that fit, carrying an open colour onto the next line.
	iMaxX = dx - 24
	# Wrap long lines
	aList = []
	COLOR = ""
	for iWidth, aLine in aWidthList:
		iMoreLines = float(iWidth) / dx
		if iMoreLines > 1.0:
			# The line is too long, break it into a word list.
			idxImage = aLine.find("<img=")
			if idxImage == -1:
				aWordList = aLine.split(" ")
			else: # We got image codes.
				aWordList = []
				while True:
					i = aLine.find(" ")
					if i != -1 and i < idxImage:
						aWordList.append(aLine[:i])
						aLine = aLine[i+1:]
						idxImage -= i+1
						continue
					else:
						if idxImage > 0:
							aWordList.append(aLine[:idxImage])
							aLine = aLine[idxImage:]
							idxImage = 0
						idx1 = aLine.find("/img>") + 5
						idx2 = aLine.find("/>") + 2
						if idx1 > 4 or idx2 > 1:
							if idx1 > 4 and idx2 > 1:
								if idx1 < idx2:
									idx0 = idx1
								else:
									idx0 = idx2
							else:
								if idx1 > 4:
									idx0 = idx1
								else:
									idx0 = idx2
								if len(aLine) == idx0:
									aWordList.append(aLine)
									break
					aWordList.append(aLine[:idx0])
					aLine = aLine[idx0:]
					idxImage = aLine.find("<img=")
					if idxImage == -1:
						aWordList += aLine.split(" ")
						break
			# Assemble lines from the word list.
			szLine = ""
			while True:
				if szLine:
					# Store the finished line.
					aList.append(COLOR + szLine)
					COLOR = evalColorCodes(szLine, COLOR)

				if not aWordList:
					break # Wrapping complete

				# A line must start with the first word.
				szLine = aWordList.pop(0)
				# Is it already too long?
				if CyIF.determineWidth(uFont + szLine) >= iMaxX:
					continue # Oh well... Wrap a new line

				# A line should have more than just one word.
				while aWordList:

					if CyIF.determineWidth(uFont + szLine + " " + aWordList[0]) >= iMaxX:
						break # Wrap a new line

					# Add another word to the line.
					szLine += " " + aWordList.pop(0)

		else: # No wrapping was necessary.
			aList.append(COLOR + aLine)
			COLOR = evalColorCodes(aLine, COLOR)
	return aList


def makeTooltip(screen, xPos, yPos, szTxt, listBox, aArea=None):
	# Lays the box out inside the screen whose area is aArea, at (xPos, yPos) in the screen's coordinates, the
	# cursor when -1. Returns the box's position on the screen.
	uFont = TOOLTIP_FONT
	if xPos == -1 or yPos == -1:
		xCursor, yCursor = cursorOnScreen(aArea)
		if xPos == -1:
			xPos = xCursor
		if yPos == -1:
			yPos = yCursor
	iScreenWidth, iScreenHeight = screenArea(aArea)[2:]

	# Enforce certain structure
	szTxt = szTxt.replace("\n</color>", "</color>\n")
	# Remove whitespace at the very end.
	while szTxt[-1:] in ("\n", "\r", "\t", " "):
		szTxt = szTxt[:-1]

	# Remove font literals
	i1 = szTxt.find("<font=")
	if i1 > -1:
		while i1 > -1:
			i2 = szTxt.find(">", i1+6)
			szTxt = szTxt[:i1] + szTxt[i2+1:]
			i1 = szTxt.find("<font=")
		szTxt = szTxt.replace("</font>", "")
	# Remove link literals
	i1 = szTxt.find("<link=")
	if i1 > -1:
		while i1 > -1:
			i2 = szTxt.find(">", i1+6)
			szTxt = szTxt[:i1] + szTxt[i2+1:]
			i1 = szTxt.find("<link=")
		szTxt = szTxt.replace("</link>", "")
	# Find longest text line in pixels
	aList = szTxt.split('\n')
	aWidthList = []
	dx = 0
	for aLine in aList:
		iWidth = CyIF.determineWidth(uFont + aLine)
		aWidthList.append((iWidth, aLine))
		if iWidth > dx:
			dx = iWidth
	dxLongest = dx
	if not dx:
		print "CvMainInterface: This will probably never happen, but hey it did."
		dx = 1
	else:
		# A third of the screen unless the wrapped text would run tall; then widen in steps.
		for dxMax in (iScreenWidth/3, iScreenWidth/2, iScreenWidth*2/3):
			dx = min(dxLongest, dxMax)
			iLines = 0
			for iWidth, aLine in aWidthList:
				iLines += 1 + iWidth / dx
			if iLines * 22 + 16 <= iScreenHeight*2/3:
				break
	aList = wrapLines(aWidthList, dx, uFont)
	# Taller than the screen: the lines go into as many side-by-side columns as it takes to fit.
	iMaxRows = (iScreenHeight - 48) / 22
	iColumns = 1
	while (len(aList) + iColumns - 1) / iColumns > iMaxRows and iColumns < 8:
		iColumns += 1
		dx = min(dxLongest, (iScreenWidth - 16) / iColumns - 28)
		aList = wrapLines(aWidthList, dx, uFont)
	iRows = (len(aList) + iColumns - 1) / iColumns
	dxColumn = dx + 24
	dxTable = iColumns * dxColumn
	# Find text height in pixels
	dy = iRows * 22 + 16
	# Find best position.
	if xPos > iScreenWidth / 2:
		x = xPos - 8 - dxTable
	else: # Left side of screen.
		x = xPos + 24
	# A widened tooltip can overrun either edge; keep it on screen.
	if x + dxTable + 4 > iScreenWidth:
		x = iScreenWidth - dxTable - 4
	if x < 4:
		x = 4
	if yPos > iScreenHeight / 2:
		if yPos - dy - 12 > 4:
			yOffset = -dy - 12
		else:
			yOffset = 4 - yPos
	else: # Top half of the screen.
		if yPos + dy + 24 < iScreenHeight - 4:
			yOffset = 24
		elif iScreenHeight - dy - 4 > 4:
			yOffset = iScreenHeight - yPos - dy - 4
		else:
			yOffset = 4 - yPos
	y = yPos + yOffset
	g_boxSizes[listBox] = (dxTable, dy)
	# draw help hover
	screen.addTableControlGFC(listBox, iColumns, x, y, dxTable, dy, False, False, 0, 0, TableStyles.TABLE_STYLE_STANDARD)
	screen.setStyle(listBox, "Table_Help_Style")
	for iColumn in xrange(iColumns):
		screen.setTableColumnHeader(listBox, iColumn, "", dxColumn)
	for iRow in xrange(iRows):
		screen.appendTableRow(listBox)
	# Column by column, top to bottom, so the lines read in their order.
	for i, aLine in enumerate(aList):
		screen.setTableText(listBox, i / iRows, i % iRows, uFont + aLine, "", WidgetTypes.WIDGET_GENERAL, 0, 0, 1<<0)
	return x, y


# Evaluates if there's a color that should wrap to next line.
def evalColorCodes(szText, COLOR):
	i = szText.find("<color=")
	if i == -1: # No color code found
		if not COLOR or szText.find("/color>") != -1:
			return ""
		return COLOR

	if i: szText = szText[i:]

	# Only interested in the last color
	while True:
		i = szText[1:].find("<color=")
		if i == -1:
			# Is color resolved?
			if szText.find("/color>") != -1:
				return "" # Resolved
			break # Not Resolved
		szText = szText[i+1:]

	idxEnd = szText.find(">") + 1
	return szText[:idxEnd]