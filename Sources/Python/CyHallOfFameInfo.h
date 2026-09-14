#pragma once

#ifndef __CY_HallOfFame_H__
#define __CY_HallOfFame_H__

#include "CvHallOfFameInfo.h"

class CyReplayInfo;

class CyHallOfFameInfo
{
public:
	CyHallOfFameInfo();

	void loadReplays();
	int getNumGames() const;
	CyReplayInfo* getReplayInfo(int i);

	/// <summary>Publishes the hall-of-fame handle to Python. Python CONSTRUCTS this type
	/// (`CyHallOfFameInfo()` in the hall-of-fame screen), so without a registration the name is absent from
	/// CvPythonExtensions and the screen dies with a NameError after it is already shown -- a blank screen.
	/// The replays it hands out are CyReplayInfo handles, registered by CyReplayInfo::pythonPublish.</summary>
	static void pythonPublish();

private:
	CvHallOfFameInfo m_hallOfFame;
};

#endif __CY_HallOfFame_H__
