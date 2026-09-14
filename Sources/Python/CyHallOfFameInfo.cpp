#include "CvGameCoreDLL.h"
#include "CyHallOfFameInfo.h"
#include "CyReplayInfo.h"

CyHallOfFameInfo::CyHallOfFameInfo() {}


void CyHallOfFameInfo::loadReplays()
{
	m_hallOfFame.loadReplays();
}

int CyHallOfFameInfo::getNumGames() const
{
	return m_hallOfFame.getNumGames();
}

CyReplayInfo* CyHallOfFameInfo::getReplayInfo(int i)
{
	// An index from script past the end answers a none-handle (CyReplayInfo::isNone), never a read past the vector.
	if (i < 0 || i >= m_hallOfFame.getNumGames())
	{
		return new CyReplayInfo(NULL);
	}
	return new CyReplayInfo(m_hallOfFame.getReplayInfo(i));
}

void CyHallOfFameInfo::pythonPublish()
{
	// noncopyable: the wrapped CvHallOfFameInfo owns and deletes its replays, so a copy would free them twice.
	python::class_<CyHallOfFameInfo, boost::noncopyable>("CyHallOfFameInfo")
		.def("loadReplays",   &CyHallOfFameInfo::loadReplays)
		.def("getNumGames",   &CyHallOfFameInfo::getNumGames)
		.def("getReplayInfo", &CyHallOfFameInfo::getReplayInfo, python::return_value_policy<python::manage_new_object>())
		;
}
