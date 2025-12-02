#include "ryke_shell.h"
#include "utils.h"

#include <algorithm>
#include <csignal>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <fcntl.h>
#include <glob.h>
#include <iostream>
#include <ranges>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace ryke {

namespace {

std::vector<char*> buildArgv(const Command& command, bool enableGlob) {
    std::vector<char*> args;
    for (const auto& arg : command.args) {
        if (enableGlob) {
            glob_t globResults{};
            if (const int globRet = glob(arg.c_str(), GLOB_NOCHECK | GLOB_TILDE, nullptr, &globResults); globRet == 0) {
                for (std::size_t i = 0; i < globResults.gl_pathc; ++i) {
                    args.push_back(strdup(globResults.gl_pathv[i]));
                }
            } else {
                args.push_back(strdup(arg.c_str()));
            }
            globfree(&globResults);
        } else {
            args.push_back(strdup(arg.c_str()));
        }
    }
    args.push_back(nullptr);
    return args;
}

void closePipe(int pipeFd[2]) {
    if (pipeFd[0] != -1) {
        close(pipeFd[0]);
    }
    if (pipeFd[1] != -1) {
        close(pipeFd[1]);
    }
}

} // namespace

CommandExecutor::CommandExecutor(pid_t shellPgid, int terminalFd, const ShellOptions* options,
                                 std::function<void(const std::string&)> notifier)
    : shellPgid_(shellPgid), terminalFd_(terminalFd), options_(options), notify_(std::move(notifier)) {}

int CommandExecutor::execute(const std::vector<Pipeline>& pipelines, const std::string& commandLine) {
    int lastStatus = 0;
    bool hasPrevious = false;

    if (options_ && options_->xtrace) {
        std::cerr << "+ " << commandLine << '\n';
    }

    for (const auto& pipeline : pipelines) {
        if (pipeline.condition == ChainCondition::And && hasPrevious && lastStatus != 0) {
            continue;
        }
        if (pipeline.condition == ChainCondition::Or && hasPrevious && lastStatus == 0) {
            continue;
        }

        lastStatus = executePipeline(pipeline, commandLine);
        hasPrevious = true;
    }

    return lastStatus;
}

void CommandExecutor::reapBackground() {
    int status = 0;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        const pid_t pgid = getpgid(pid);
        if (pgid == -1) {
            continue;
        }
        for (auto& job : jobs_) {
            if (job.pgid != pgid) {
                continue;
            }
            if (WIFEXITED(status)) {
                job.status = Job::Status::Done;
                job.exitCode = WEXITSTATUS(status);
            } else if (WIFSIGNALED(status)) {
                job.status = Job::Status::Done;
                job.exitCode = 128 + WTERMSIG(status);
            } else if (WIFSTOPPED(status)) {
                job.status = Job::Status::Stopped;
            } else if (WIFCONTINUED(status)) {
                job.status = Job::Status::Running;
            }
            if (job.status == Job::Status::Done && options_ && options_->notify && notify_) {
                notify_("job [" + std::to_string(job.id) + "] done");
            }
        }
    }
}

void CommandExecutor::listJobs(std::ostream& os, bool verbose) {
    pruneDone();
    for (const auto& job : jobs_) {
        std::string status;
        switch (job.status) {
            case Job::Status::Running: status = "Running"; break;
            case Job::Status::Stopped: status = "Stopped"; break;
            case Job::Status::Done: status = "Done"; break;
        }
        if (verbose) {
            os << '[' << job.id << "] " << job.pgid << ' ' << status << " " << job.command << '\n';
        } else {
            os << '[' << job.id << "] " << status << " " << job.command << '\n';
        }
    }
}

bool CommandExecutor::foregroundJob(int jobId) {
    if (options_ && !options_->monitor) {
        return false;
    }
    Job* job = jobId == -1 ? lastJob() : findJob(jobId);
    if (!job) {
        return false;
    }

    currentFgPgid_ = job->pgid;
    adoptTerminal(job->pgid);
    if (job->status == Job::Status::Stopped) {
        kill(-job->pgid, SIGCONT);
    }

    int status = 0;
    waitpid(-job->pgid, &status, WUNTRACED);
    restoreTerminal();
    currentFgPgid_ = 0;

    if (WIFSTOPPED(status)) {
        job->status = Job::Status::Stopped;
        return true;
    }

    job->status = Job::Status::Done;
    job->exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : (WIFSIGNALED(status) ? 128 + WTERMSIG(status) : status);
    pruneDone();
    return true;
}

bool CommandExecutor::backgroundJob(int jobId) {
    if (options_ && !options_->monitor) {
        return false;
    }
    Job* job = jobId == -1 ? lastJob() : findJob(jobId);
    if (!job) {
        return false;
    }
    if (job->status == Job::Status::Stopped) {
        kill(-job->pgid, SIGCONT);
        job->status = Job::Status::Running;
    }
    return true;
}

void CommandExecutor::stopForeground() {
    if (currentFgPgid_ > 0) {
        kill(-currentFgPgid_, SIGTSTP);
    }
}

int CommandExecutor::executePipeline(const Pipeline& pipeline, const std::string& commandLine) {
    if (pipeline.stages.empty()) {
        return 0;
    }

    int prevPipe[2] = {-1, -1};
    std::vector<pid_t> childPids;
    pid_t pgid = 0;

    for (std::size_t index = 0; index < pipeline.stages.size(); ++index) {
        int pipeFd[2] = {-1, -1};
        const bool createPipe = index + 1 < pipeline.stages.size();
        if (createPipe) {
            if (pipe(pipeFd) == -1) {
                perror("pipe");
                return 1;
            }
        }

        const Command& command = pipeline.stages[index];
        int heredocPipe[2] = {-1, -1};
        if (command.heredocDelimiter || command.hereString || command.heredocData) {
            if (pipe(heredocPipe) == -1) {
                perror("pipe");
                return 1;
            }
        }

        const pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            return 1;
        }

        if (pid == 0) {
            if (pgid == 0) {
                pgid = getpid();
            }
            setpgid(0, pgid);

            if (!pipeline.background && (!options_ || options_->monitor)) {
                tcsetpgrp(terminalFd_, pgid);
            }

            if (prevPipe[0] != -1) {
                dup2(prevPipe[0], STDIN_FILENO);
            }
            if (createPipe) {
                dup2(pipeFd[1], STDOUT_FILENO);
            }

            if (prevPipe[0] != -1) {
                closePipe(prevPipe);
            }
            if (createPipe) {
                closePipe(pipeFd);
            }

            if (command.inputFile) {
                const int fd = open(command.inputFile->c_str(), O_RDONLY);
                if (fd == -1) {
                    perror("open");
                    _exit(EXIT_FAILURE);
                }
                dup2(fd, STDIN_FILENO);
                close(fd);
            }

            std::vector<Command::FdRedirection> redirs = command.fdRedirections;
            if (command.outputFile) {
                redirs.push_back(Command::FdRedirection{1, Command::FdRedirection::Type::Truncate, *command.outputFile, 1});
            } else if (command.appendFile) {
                redirs.push_back(Command::FdRedirection{1, Command::FdRedirection::Type::Append, *command.appendFile, 1});
            }
            if (command.stderrFile) {
                redirs.push_back(Command::FdRedirection{2, Command::FdRedirection::Type::Truncate, *command.stderrFile, 2});
            } else if (command.stderrAppendFile) {
                redirs.push_back(Command::FdRedirection{2, Command::FdRedirection::Type::Append, *command.stderrAppendFile, 2});
            } else if (command.mergeStderr) {
                redirs.push_back(Command::FdRedirection{2, Command::FdRedirection::Type::Dup, "", 1});
            }

            // Apply file redirections first, then descriptor dups so duplication targets updated fds.
            for (const auto& r : redirs) {
                if (r.type == Command::FdRedirection::Type::Dup) continue;
                int flags = O_WRONLY | O_CREAT;
                if (r.type == Command::FdRedirection::Type::Append) {
                    flags |= O_APPEND;
                } else {
                    if (options_ && options_->noclobber) {
                        flags |= O_EXCL;
                    } else {
                        flags |= O_TRUNC;
                    }
                }
                const int fd = open(r.target.c_str(), flags, 0644);
                if (fd == -1) {
                    perror("open");
                    _exit(EXIT_FAILURE);
                }
                dup2(fd, r.fd);
                close(fd);
            }
            for (const auto& r : redirs) {
                if (r.type == Command::FdRedirection::Type::Dup) {
                    dup2(r.dupFd, r.fd);
                }
            }

            if (heredocPipe[0] != -1) {
                dup2(heredocPipe[0], STDIN_FILENO);
            }

            std::vector<char*> argv = buildArgv(command, !(options_ && options_->noglob));
            if (argv.empty() || argv.front() == nullptr) {
                _exit(EXIT_FAILURE);
            }

            execvp(argv.front(), argv.data());
            std::cerr << "\033[1;31mError: Command not found: " << argv.front() << "\033[0m\n";
            for (char* arg : argv) {
                free(arg);
            }
            _exit(EXIT_FAILURE);
        }

        if (pgid == 0) {
            pgid = pid;
        }
        setpgid(pid, pgid);
        childPids.push_back(pid);

        if (heredocPipe[0] != -1) {
            if (command.heredocDelimiter || command.hereString || command.heredocData) {
                close(heredocPipe[0]);
                std::string data;
                if (command.hereString) {
                    data = *command.hereString;
                } else if (command.heredocData) {
                    data = *command.heredocData;
                } else {
                    std::string line;
                    while (true) {
                        std::cout << "> ";
                        std::cout.flush();
                        if (!std::getline(std::cin, line)) {
                            break;
                        }
                        if (line == *command.heredocDelimiter) {
                            break;
                        }
                        if (command.heredocStripTabs) {
                            while (!line.empty() && line.front() == '\t') {
                                line.erase(line.begin());
                            }
                        }
                        data += line + '\n';
                    }
                }
                if (command.heredocExpand) {
                    try {
                        data = expandVariables(data, options_);
                    } catch (...) {
                        // ignore expansion errors in heredoc
                    }
                }
                write(heredocPipe[1], data.c_str(), data.size());
                close(heredocPipe[1]);
            }
        }

        if (prevPipe[0] != -1) {
            closePipe(prevPipe);
        }
        if (createPipe) {
            close(pipeFd[1]);
            prevPipe[0] = pipeFd[0];
            prevPipe[1] = -1;
        } else {
            prevPipe[0] = prevPipe[1] = -1;
        }
    }

    int status = 0;
    if (pipeline.background) {
        const int jobId = nextJobId_++;
        jobs_.push_back(Job{jobId, pgid, commandLine, Job::Status::Running, 0});
        std::cout << '[' << jobId << "] " << pgid << "\n";
        return 0;
    }

    if (!options_ || options_->monitor) {
        currentFgPgid_ = pgid;
        adoptTerminal(pgid);
    }
    for (const pid_t pid : childPids) {
        int childStatus = 0;
        waitpid(pid, &childStatus, WUNTRACED);
        if (pid == childPids.back()) {
            status = childStatus;
        }
        if (WIFSTOPPED(childStatus)) {
            jobs_.push_back(Job{nextJobId_++, pgid, commandLine, Job::Status::Stopped, 0});
            if (!options_ || options_->monitor) {
                restoreTerminal();
            }
            return 128 + WSTOPSIG(childStatus);
        }
    }
    if (!options_ || options_->monitor) {
        restoreTerminal();
    }
    currentFgPgid_ = 0;

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }
    return status;
}

void CommandExecutor::adoptTerminal(pid_t pgid) {
    if (tcsetpgrp(terminalFd_, pgid) == -1 && errno != ENOTTY) {
        perror("tcsetpgrp");
    }
}

void CommandExecutor::restoreTerminal() {
    if (tcsetpgrp(terminalFd_, shellPgid_) == -1 && errno != ENOTTY) {
        perror("tcsetpgrp");
    }
}

Job* CommandExecutor::findJob(int jobId) {
    const auto it = std::ranges::find_if(jobs_, [&](const Job& job) { return job.id == jobId; });
    return it == jobs_.end() ? nullptr : &(*it);
}

Job* CommandExecutor::lastJob() {
    pruneDone();
    if (jobs_.empty()) {
        return nullptr;
    }
    return &jobs_.back();
}

void CommandExecutor::pruneDone() {
    jobs_.erase(std::remove_if(jobs_.begin(), jobs_.end(),
                               [](const Job& job) { return job.status == Job::Status::Done; }),
                jobs_.end());
}

} // namespace ryke
