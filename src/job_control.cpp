#include "job_control.h"
#include "ryke_shell.h"
#include <iostream>
#include <algorithm>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

std::vector<Job> jobList;
int nextJobID = 1;

void addJob(pid_t pgid, const std::string& command, bool isRunning, bool isStopped) {
    Job job;
    job.id = nextJobID++;
    job.pgid = pgid;
    job.command = command;
    job.isRunning = isRunning;
    job.isStopped = isStopped;
    jobList.push_back(job);
}

void removeJob(pid_t pgid) {
    jobList.erase(std::remove_if(jobList.begin(), jobList.end(),
        [pgid](const Job& job) { return job.pgid == pgid; }), jobList.end());
}

void updateJobStatus(pid_t pgid, bool isRunning, bool isStopped) {
    for (auto& job : jobList) {
        if (job.pgid == pgid) {
            job.isRunning = isRunning;
            job.isStopped = isStopped;
            break;
        }
    }
}

Job* findJob(pid_t pgid) {
    for (auto& job : jobList) {
        if (job.pgid == pgid) {
            return &job;
        }
    }
    return nullptr;
}

Job* findJobByID(int id) {
    for (auto& job : jobList) {
        if (job.id == id) {
            return &job;
        }
    }
    return nullptr;
}

void listJobs() {
    for (const auto& job : jobList) {
        std::cout << "[" << job.id << "] ";
        if (job.isRunning) {
            std::cout << "Running    ";
        } else if (job.isStopped) {
            std::cout << "Stopped    ";
        } else {
            std::cout << "Completed  ";
        }
        std::cout << job.command << "\n";
    }
}

void bringJobToForeground(Job* job, bool continueJob) {
    if (!job) return;

    //Put job's process group into the foreground
    tcsetpgrp(STDIN_FILENO, job->pgid);

    if (continueJob) {
        if (kill(-job->pgid, SIGCONT) < 0) {
            perror("kill (SIGCONT)");
        }
    }

    waitForJob(job);

    //Restore shell to foreground
    tcsetpgrp(STDIN_FILENO, shellPGID);
}

void continueJobInBackground(Job* job, bool continueJob) {
    if (!job) return;

    if (continueJob) {
        if (kill(-job->pgid, SIGCONT) < 0) {
            perror("kill (SIGCONT)");
        }
    }

    job->isRunning = true;
    job->isStopped = false;
}

void waitForJob(Job* job) {
    int status;
    pid_t pid;

    do {
        pid = waitpid(-job->pgid, &status, WUNTRACED);
        if (pid == -1) {
            if (errno == ECHILD) {
                //No child processes
                break;
            } else {
                perror("waitpid");
                break;
            }
        }
        if (WIFSTOPPED(status)) {
            job->isStopped = true;
            job->isRunning = false;
            break;
        }
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));

    if (WIFEXITED(status) || WIFSIGNALED(status)) {
        removeJob(job->pgid);
    }
}