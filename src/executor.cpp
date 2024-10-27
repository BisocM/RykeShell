#include "ryke_shell.h"
#include <unistd.h>
#include <sys/wait.h>
#include <glob.h>
#include <fcntl.h>
#include <iostream>
#include <cstring>
#include <cstdlib>

#include "job_control.h"

void executeCommands(const std::vector<Command>& commands) {
    pid_t pid;
    pid_t pgid = 0;
    int prevPipeFd[2] = {-1, -1};

    for (size_t cmdIndex = 0; cmdIndex < commands.size(); ++cmdIndex) {
        int pipeFd[2];
        if (cmdIndex < commands.size() - 1) {
            if (pipe(pipeFd) == -1) {
                perror("pipe");
                return;
            }
        }

        pid = fork();
        if (pid < 0) {
            perror("fork");
            return;
        } else if (pid == 0) {
            //Child process

            //Set the process group ID for the job
            if (pgid == 0) {
                pgid = getpid();
            }
            setpgid(0, pgid);

            //If not running in background, take control of the terminal
            if (!commands[0].background) {
                tcsetpgrp(STDIN_FILENO, pgid);
            }

            //Restore default signal handlers
            signal(SIGINT, SIG_DFL);
            signal(SIGQUIT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            signal(SIGTTIN, SIG_DFL);
            signal(SIGTTOU, SIG_DFL);
            signal(SIGCHLD, SIG_DFL);

            //[Input/output redirection code here]

            //Handling pipes
            if (prevPipeFd[0] != -1) {
                dup2(prevPipeFd[0], STDIN_FILENO);
                close(prevPipeFd[0]);
                close(prevPipeFd[1]);
            }
            if (cmdIndex < commands.size() - 1) {
                close(pipeFd[0]);
                dup2(pipeFd[1], STDOUT_FILENO);
                close(pipeFd[1]);
            }

            //Expand arguments (globbing)
            std::vector<char*> expandedArgs;
            for (const auto& arg : commands[cmdIndex].args) {
                glob_t glob_results{};
                if (glob(arg.c_str(), GLOB_NOCHECK | GLOB_TILDE, nullptr, &glob_results) == 0) {
                    for (size_t j = 0; j < glob_results.gl_pathc; ++j) {
                        expandedArgs.push_back(strdup(glob_results.gl_pathv[j]));
                    }
                } else {
                    expandedArgs.push_back(strdup(arg.c_str()));
                }
                globfree(&glob_results);
            }
            expandedArgs.push_back(nullptr);

            if (expandedArgs.empty()) {
                exit(EXIT_FAILURE);
            }

            //Execute command
            execvp(expandedArgs[0], expandedArgs.data());
            std::cerr << "\033[1;31mError: Command not found: " << expandedArgs[0] << "\033[0m\n";
            for (char* arg : expandedArgs) {
                free(arg);
            }
            exit(EXIT_FAILURE);
        } else {
            //Parent process

            //Set the process group ID for the child
            if (pgid == 0) {
                pgid = pid;
            }
            setpgid(pid, pgid);

            if (prevPipeFd[0] != -1) {
                close(prevPipeFd[0]);
                close(prevPipeFd[1]);
            }
            if (cmdIndex < commands.size() - 1) {
                prevPipeFd[0] = pipeFd[0];
                prevPipeFd[1] = pipeFd[1];
                close(pipeFd[1]);
            }
        }
    }

    //Parent process continues here
    if (!commands[0].background) {
        //Foreground job: Wait for job to finish
        addJob(pgid, commands[0].args[0], true, false);
        Job* job = findJob(pgid);

        //Give terminal control to the job
        tcsetpgrp(STDIN_FILENO, pgid);

        //Wait for the job to complete
        waitForJob(job);

        //Restore terminal control to the shell
        tcsetpgrp(STDIN_FILENO, shellPGID);
    } else {
        //Background job
        addJob(pgid, commands[0].args[0], true, false);
        std::cout << "[" << nextJobID - 1 << "] " << pgid << " running in background\n";
    }
}