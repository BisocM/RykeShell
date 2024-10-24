#include "ryke_shell.h"
#include <unistd.h>
#include <sys/wait.h>
#include <glob.h>
#include <fcntl.h>
#include <iostream>
#include <cstring>
#include <cstdlib>

void executeCommands(const std::vector<Command>& commands) {
    const size_t numCommands = commands.size();
    int prevPipeFd[2] = {-1, -1};

    for (size_t cmdIndex = 0; cmdIndex < numCommands; ++cmdIndex) {
        int pipeFd[2];
        if (cmdIndex < numCommands - 1) {
            if (pipe(pipeFd) == -1) {
                perror("pipe");
                return;
            }
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            return;
        } else if (pid == 0) {
            //Child process

            //Input redirection
            if (!commands[cmdIndex].inputFile.empty()) {
                const int fd = open(commands[cmdIndex].inputFile.c_str(), O_RDONLY);
                if (fd == -1) {
                    perror("open");
                    exit(EXIT_FAILURE);
                }
                dup2(fd, STDIN_FILENO);
                close(fd);
            }

            //Output redirection
            if (!commands[cmdIndex].outputFile.empty()) {
                const int fd = open(commands[cmdIndex].outputFile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd == -1) {
                    perror("open");
                    exit(EXIT_FAILURE);
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            } else if (!commands[cmdIndex].appendFile.empty()) {
                const int fd = open(commands[cmdIndex].appendFile.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
                if (fd == -1) {
                    perror("open");
                    exit(EXIT_FAILURE);
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }

            //Set up pipes
            if (prevPipeFd[0] != -1) {
                dup2(prevPipeFd[0], STDIN_FILENO);
                close(prevPipeFd[0]);
                close(prevPipeFd[1]);
            }
            if (cmdIndex < numCommands - 1) {
                close(pipeFd[0]);
                dup2(pipeFd[1], STDOUT_FILENO);
                close(pipeFd[1]);
            }

            //Expand wildcards in arguments
            std::vector<char*> expandedArgs;
            for (const auto& arg : commands[cmdIndex].args) {
                glob_t glob_results = {};
                int glob_ret = glob(arg.c_str(), GLOB_NOCHECK | GLOB_TILDE, nullptr, &glob_results);
                if (glob_ret == 0) {
                    for (size_t j = 0; j < glob_results.gl_pathc; ++j) {
                        expandedArgs.push_back(strdup(glob_results.gl_pathv[j]));
                    }
                } else {
                    //Keep the original argument if glob fails
                    expandedArgs.push_back(strdup(arg.c_str()));
                }
                globfree(&glob_results);
            }
            expandedArgs.push_back(nullptr);

            //Execute command
            if (expandedArgs.empty()) {
                exit(EXIT_FAILURE);
            }
            execvp(expandedArgs[0], expandedArgs.data());
            //If execvp returns, it must have failed
            std::cerr << "\033[1;31mError: Command not found: " << expandedArgs[0] << "\033[0m\n";
            //Free memory
            for (char* arg : expandedArgs) {
                free(arg);
            }
            exit(EXIT_FAILURE);
        } else {
            //Parent process
            if (!commands[cmdIndex].background) {
                int status;
                waitpid(pid, &status, 0);
            } else {
                //Background process
                std::cout << "[Background pid: " << pid << "]" << std::endl;
            }

            //Close previous pipes
            if (prevPipeFd[0] != -1) {
                close(prevPipeFd[0]);
                close(prevPipeFd[1]);
            }

            if (cmdIndex < numCommands - 1) {
                prevPipeFd[0] = pipeFd[0];
                prevPipeFd[1] = pipeFd[1];
                close(pipeFd[1]);
            }
        }
    }
}