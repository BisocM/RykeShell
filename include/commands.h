#ifndef COMMANDS_H
#define COMMANDS_H

#include "ryke_shell.h"

//Built-in Command Handlers
void cmdExit(const Command& command);
void cmdCd(const Command& command);
void cmdPwd(const Command& command);
void cmdHistory(const Command& command);
void cmdAlias(const Command& command);
void cmdTheme(const Command& command);

//Command Handler Map
extern std::map<std::string, CommandFunction> commandHandlers;

#endif