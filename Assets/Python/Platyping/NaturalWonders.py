## Sid Meier's Civilization 4
## Copyright Firaxis Games 2007

from CvPythonExtensions import *
TRNSLTR = CyTranslator()

# The one data-fetching library ([DEC-cy-not-fixed]): STATE = live state, ENABLER = availability,
# ENUMS = the engine enum vocabulary + name->id resolution.
GC = CyGlobalContext()
INFO = CyInfo()
GAME = GC.getGame()
MAP = GC.getMap()
STATE = CyState()
ACT = CyAct()
ENABLER = CyEnabler()
ENUMS = CyEnums()

class NaturalWonders:
    def __init__(self,):
        self.iFirstCulture = 50					## Culture Granted to First Team to Discover
        self.lBigWonder = ["FEATURE_PLATY_GREAT_BARRIER"]	## List of Natural Wonders that occupy 2 Tiles
        self.lLatitude = [("FEATURE_PLATY_AURORA", 70, 90)]	## Min Latitude, Max Latitude
        self.discoveredWonders = {}
        self.pendingCulture = []

    def placeNaturalWonders(self):
        MAP = GC.getMap()
        GAME = GC.getGame()
        for iFeature in xrange(GC.getNumFeatureInfos()):
            sType = INFO.getType("FEATURE_", iFeature)
            if sType.find("FEATURE_PLATY_") == -1:
                continue
            WonderPlot = []
            for pPlot in MAP.plots():
                if pPlot.getBonusType(-1) > -1:
                    continue
                ## Nearby Plot Check ##
                bUnsuitable = False
                iRadius = 2
                bAdjacentPlot = True

                ## Big Wonders ##
                if sType in self.lBigWonder:
                    iRadius += 1
                    bAdjacentPlot = False

                for x in xrange(pPlot.getX() - iRadius, pPlot.getX() + iRadius + 1):
                    for y in xrange(pPlot.getY() - iRadius, pPlot.getY() + iRadius + 1):
                        pAdjacentPlot = MAP.plot(x, y)
                        if not pAdjacentPlot: continue

                        if pAdjacentPlot.getFeatureType() > -1 and INFO.getType("FEATURE_", pAdjacentPlot.getFeatureType()).find("FEATURE_PLATY_") > -1:
                            bUnsuitable = True
                            break
                        ## Big Wonders ##
                        if (not bAdjacentPlot and pAdjacentPlot.canHaveFeature(iFeature)
                                and abs(pAdjacentPlot.getX() - pPlot.getX()) < 2
                                and abs(pAdjacentPlot.getY() - pPlot.getY()) < 2
                                and pAdjacentPlot.getBonusType(-1) == -1
                        ): bAdjacentPlot = True

                    if bUnsuitable: break
                if bUnsuitable or not bAdjacentPlot or not pPlot.canHaveFeature(iFeature):
                    continue

                ## Latitude Check ##
                for i in self.lLatitude:
                    if sType == i[0] and (pPlot.getLatitude() < i[1] or pPlot.getLatitude() > i[2]):
                        break
                ## Suitable Plot ##
                else: WonderPlot.append(pPlot)

            while WonderPlot:
                pPlot = WonderPlot.pop(GAME.getSorenRandNum(len(WonderPlot), "Random Plot"))

                ## Big Wonders ##
                if sType in self.lBigWonder:
                    AdjacentPlot = []
                    for x in xrange(pPlot.getX() - 1, pPlot.getX() + 2):
                        for y in xrange(pPlot.getY() - 1, pPlot.getY() + 2):
                            if x == pPlot.getX() and y == pPlot.getY():
                                continue
                            pAdjacentPlot = MAP.plot(x, y)
                            if not pAdjacentPlot:
                                continue
                            if pAdjacentPlot.canHaveFeature(iFeature):
                                if pAdjacentPlot.getBonusType(-1) > -1:
                                    continue
                                AdjacentPlot.append(pAdjacentPlot)
                    if not AdjacentPlot:
                        continue
                    AdjacentPlot[GAME.getSorenRandNum(len(AdjacentPlot), "Random Plot")].setFeatureType(iFeature, 0)

                ## Standard Wonders ##
                pPlot.setFeatureType(iFeature, 0)
                break


    def checkReveal(self, pPlot, iTeam):
        ## A NULL plot crosses the boundary as None -- the one guard the handler needs.
        if pPlot is None:
            return

        iFeature = pPlot.getFeatureType()
        if iFeature == -1:
            return

        sType = INFO.getType("FEATURE_", iFeature)
        if sType.find("FEATURE_PLATY_") == -1:
            return

        if iTeam < 0 or iTeam >= GC.getMAX_TEAMS():
            return
        CyTeam = GC.getTeam(iTeam)
        if CyTeam is None or CyTeam.isNPC():
            return

        GAME = GC.getGame()
        if GAME.GetWorldBuilderMode():
            return

        ## Flush any pending culture grants from wonders discovered before the
        ## first city was founded. Only runs once capital exists.
        remaining = []
        for iP, c in self.pendingCulture:
            player = GC.getPlayer(iP)
            if player is None:
                continue
            pCapital = player.getCapitalCity()
            if pCapital:
                ACT.changeCityCulture(pCapital.getOwner(), pCapital.getID(), iP, c, True)
            else:
                remaining.append((iP, c))
        self.pendingCulture = remaining

        pWonderPlot = None
        if sType in self.lBigWonder:
            for x in xrange(pPlot.getX() - 1, pPlot.getX() + 2):
                for y in xrange(pPlot.getY() - 1, pPlot.getY() + 2):
                    if x == pPlot.getX() and y == pPlot.getY():
                        continue
                    pAdjacentPlot = GC.getMap().plot(x, y)
                    if not pAdjacentPlot:
                        continue
                    if pAdjacentPlot.getFeatureType() == iFeature:
                        pWonderPlot = pAdjacentPlot
                        break
                if pWonderPlot:
                    break
            if pWonderPlot is None:
                return
            if pWonderPlot.isRevealed(iTeam, False):
                return

        if (iFeature, iTeam) in self.discoveredWonders:
            return

        bFirst = True
        for iTeamX in xrange(GC.getMAX_PC_TEAMS()):
            if iTeamX == iTeam:
                continue
            if pPlot.isRevealed(iTeamX, False):
                bFirst = False
                break
            if pWonderPlot and pWonderPlot.isRevealed(iTeamX, False):
                bFirst = False
                break

        self.discoveredWonders[(iFeature, iTeam)] = True

        iCulture = self.iFirstCulture * GAME.getSpeedPercent() / 100

        import CvUtil
        TRNSLTR = CyTranslator()
        iPlayerAct = GAME.getActivePlayer()

        for iPlayerX in xrange(GC.getMAX_PC_PLAYERS()):
            CyPlayerX = GC.getPlayer(iPlayerX)
            if CyPlayerX is None:
                continue
            iTeamX = CyPlayerX.getTeam()
            if iTeamX != iTeam:
                if bFirst and iPlayerX == iPlayerAct:
                    if CyTeam.isHasMet(iTeamX):
                        CvUtil.sendMessage(TRNSLTR.getText("TXT_KEY_MET_FIRST_WONDER",(GC.getTeam(iTeam).getName(), INFO.getDescription("FEATURE_", iFeature))), iPlayerX, 12, bForce=False)
                    else:
                        CvUtil.sendMessage(TRNSLTR.getText("TXT_KEY_NOT_MET_FIRST_WONDER",(INFO.getDescription("FEATURE_", iFeature),)), iPlayerX, 12, bForce=False)
                continue
            if iPlayerX == iPlayerAct:
                popupInfo = CyPopupInfo()
                popupInfo.setButtonPopupType(ButtonPopupTypes.BUTTONPOPUP_PYTHON_SCREEN)
                popupInfo.setData1(iFeature)
                popupInfo.setData2(-1)
                popupInfo.setData3(3)
                popupInfo.setText("showWonderMovie")
                popupInfo.addPopup(iPlayerX)
                CvUtil.sendMessage(TRNSLTR.getText("TXT_KEY_WONDERDISCOVERED_YOU",(INFO.getDescription("FEATURE_", iFeature),)), iPlayerX, 12, INFO.getButton("FEATURE_", iFeature), ColorTypes(44), pPlot.getX(), pPlot.getY(), True, True, bForce=False)
            if bFirst:
                pCapital = CyPlayerX.getCapitalCity()
                if pCapital:
                    ACT.changeCityCulture(pCapital.getOwner(), pCapital.getID(), iPlayerX, iCulture, True)
                else:
                    self.pendingCulture.append((iPlayerX, iCulture))
                if iPlayerX == iPlayerAct:
                    CvUtil.sendMessage(TRNSLTR.getText("TXT_KEY_FIRST_FOUND_WONDER",(iCulture,)), iPlayerX, 12, None, ColorTypes(44), bForce=False)