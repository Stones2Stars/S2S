
#include "Tools/FProfiler.h"

#include "CvGameCoreDLL.h"
#include "Engine/CvGameCoreUtils.h"
#include "Tools/CvRandom.h"
#include "CyGameCoreUtils.h"
#include "CyPlot.h"

int cyPlotDistance(int iX, int iY, int iX2, int iY2)
{
	return plotDistance(iX, iY, iX2, iY2);
}

int cyStepDistance(int iX1, int iY1, int iX2, int iY2)
{
	return stepDistance(iX1, iY1, iX2, iY2);
}

CyPlot* cyPlotDirection(int iX, int iY, DirectionTypes eDirection)
{
	if (plotDirection(iX, iY, eDirection))
	{
		return new CyPlot(plotDirection(iX, iY, eDirection));
	}
	return NULL;
}

CyPlot* cyPlotCardinalDirection(int iX, int iY, CardinalDirectionTypes eCardDirection)
{
	if (plotCardinalDirection(iX, iY, eCardDirection))
	{
		return new CyPlot(plotCardinalDirection(iX, iY, eCardDirection));
	}
	return NULL;
}

CyPlot* cyPlotXY(int iX, int iY, int iDX, int iDY)
{
	if (plotXY(iX, iY, iDX, iDY))
	{
		return new CyPlot(plotXY(iX, iY, iDX, iDY));
	}
	return NULL;
}

CyPlot* cyPlotCity(int iX, int iY, int iIndex)
{
	if (plotCity(iX, iY, iIndex))
	{
		return new CyPlot(plotCity(iX, iY, iIndex));
	}
	return NULL;
}

int cyGetEspionageModifier(int iOurTeam, int iTargetTeam)
{
	return getEspionageModifier((TeamTypes)iOurTeam, (TeamTypes)iTargetTeam);
}

int64_t cyIntSqrt64(int64_t iValue)
{
	// Signed at the boundary on purpose: a caller legitimately holds a negative here (a treasury in debt is the
	// reachable one), and the engine's unsigned signature would read that as an enormous positive rather than
	// rejecting it. A root below zero has no answer, so the edge floors it instead of inventing one.
	return iValue > 0 ? intSqrt64((uint64_t)iValue) : 0;
}


void cyShufflePyList(python::list& pyList, CvRandom& rand)
{
	PROFILE_EXTRA_FUNC();
	PyObject* pyObj = pyList.ptr();

	for (int i = 0, size = PySequence_Length(pyObj); i < size; i++)
	{
		const int j = i + rand.get(size - i);
		if (i != j)
		{
			PyObject* temp = PySequence_GetItem(pyObj, i);
			PySequence_SetItem(pyObj, i, PySequence_GetItem(pyObj, j));
			PySequence_SetItem(pyObj, j, temp);
		}
	}
}
