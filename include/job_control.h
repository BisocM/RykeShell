#ifndef JOB_CONTROL_H
#define JOB_CONTROL_H

#include <vector>
#include <string>
#include <sys/types.h>

struct Job {
    int id;
    pid_t pgid;
    std::string command;
    bool isRunning;
    bool isStopped;
};

extern std::vector<Job> jobList;
extern int nextJobID;

void addJob(pid_t pgid, const std::string& command, bool isRunning, bool isStopped);
void removeJob(pid_t pgid);
void updateJobStatus(pid_t pgid, bool isRunning, bool isStopped);
Job* findJob(pid_t pgid);
Job* findJobByID(int id);
void listJobs();
void bringJobToForeground(Job* job, bool continueJob);
void continueJobInBackground(Job* job, bool continueJob);
void waitForJob(Job* job);

#endif //JOB_CONTROL_H