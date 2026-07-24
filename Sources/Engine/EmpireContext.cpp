#include "CvGameCoreDLL.h"
#include "EmpireContext.h"
#include "CvPlayer.h"

// forwarding accessor: the empire's state religion is already O(1) on CvPlayer -- read it through, no stored copy.
int EmpireContext::stateReligion() const { return m_player != NULL ? (int)m_player->getStateReligion() : -1; }
