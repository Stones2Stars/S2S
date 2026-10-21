#pragma once

#ifndef CyGameCoreUtils_h
#define CyGameCoreUtils_h

//
// Python wrapper functions for DLL
//

class CvRandom;
class CyPlot;

int cyPlotDistance(int iX1, int iY1, int iX2, int iY2);
int cyStepDistance(int iX1, int iY1, int iX2, int iY2);
CyPlot* cyPlotDirection(int iX, int iY, DirectionTypes eDirection);
CyPlot* cyPlotCardinalDirection(int iX, int iY, CardinalDirectionTypes eCardDirection);
CyPlot* cyPlotXY(int iX, int iY, int iDX, int iDY);
CyPlot* cyPlotCity(int iX, int iY, int iIndex);

int cyGetEspionageModifier(int /*TeamTypes*/ iOurTeam, int /*TeamTypes*/ iTargetTeam);

int64_t cyIntSqrt64(int64_t iValue);

void cyShufflePyList(python::list& pyList, CvRandom& rand);

#endif	// CyGameCoreUtils_h
