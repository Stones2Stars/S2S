#pragma once

#ifndef CV_WORKORDER_BROKER
#define CV_WORKORDER_BROKER

#include "../CvGameCoreDLL.h"
#include "../CvEnums.h"

struct WorkerProject {
    int id;
    int x;
    int y;
    int workNeeded;
    ImprovementTypes improvement;
    int priority;
};

class CvWorkOrderBroker {
public:
    void UnitKilled(int unitId);
    void TravellingToProject(int unitId, int projectId);
    void TravelAborted(int unitId);
    void ProjectStarted(int projectId);
    void WorkStarted(int unitId);
    void ProjectComplete(int projectId);
    bool AnnounceAvailability(CvUnit* unit);

private:
    std::vector<WorkerProject> projects;
    std::map<int, int> assignedWorkers;


};


#endif