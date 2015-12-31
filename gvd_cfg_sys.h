#ifndef __GVD_CFG_DEFAULT_H__
#define __GVD_CFG_DEFAULT_H__

#include "gvd_cli_base.h"
#include "gvd_cli.h"

// template node for "no" "default" "exit"
END(node_exit_end, exec_exit);
END(node_end_end, exec_end);

DEF_DEFAULT(node_default,
            NO_ALT,
            NO_ALT,
            "default", "Set a command to its defaults");

DEF_NO(node_no,
       NO_ALT,
       node_default,
       "no", "Negate a command or set its defaults");

KEYWORD(node_end,
        node_end_end,
        node_no,
        "end", "Exit to Exec mode");

KEYWORD(node_exit,
        node_exit_end,
        node_end,
        "exit", "Exit from %s mode");

/* exec <cmd> */

END(node_shell_exec_end, exec_shell_cmd);

STRING(node_shell_exec_cmd,
       node_shell_exec_end,
       NO_ALT,
       OBJ(P_STRING, P0),
       "Linux shell command, embraced with quotes if contain space");

KEYWORD(node_shell_exec,
        node_shell_exec_cmd,
        NO_ALT,
        "exec", "Execute a linux shell command");

// Link to Shell mode
LINK_ROOT(node_shell_exec, CLI_MODE_SHELL);

/* shell */

END_SUBMODE(node_shell_end, exec_shell_mode, CLI_MODE_SHELL);

KEYWORD(node_shell,
        node_shell_end,
        NO_ALT,
        "shell", "Run linux shell commands");

/* show time */

END(node_show_time_end, exec_show_time);

KEYWORD(node_show_time,
        node_show_time_end,
        NO_ALT,
        "time", "System time");

/* show version */

END(node_show_ver_end, exec_show_version);

KEYWORD(node_show_ver,
        node_show_ver_end,
        node_show_time,
        "version", "System hardware and software status");

KEYWORD(node_show,
        node_show_ver,
        node_shell,
        "show", "Show running system information");


/* logfile {flush | clear} */

END(node_config_flush_end, exec_logfile_flush);
END(node_config_clear_end, exec_logfile_clear);

KEYWORD(node_logfile_clear,
        node_config_clear_end,
        NO_ALT,
        "clear", "Clear console log file");

KEYWORD(node_logfile_flush,
        node_config_flush_end,
        node_logfile_clear,
        "flush", "Flush all console content to log file");

KEYWORD(node_logfile,
        node_logfile_flush,
        node_show,
        "logfile", "Do action on console log file");

/* configure terminal */

END_SUBMODE(node_config_term_end, exec_config_term, CLI_MODE_CONFIG);

KEYWORD(node_config_term,
        node_config_term_end,
        NO_ALT,
        "terminal", "Configure from the terminal");

KEYWORD(node_config,
        node_config_term,
        node_logfile,
        "configure", "Enter configuration mode");

/* quit */

END(node_quit_end, exec_quit);

KEYWORD(node_quit,
        node_quit_end,
        node_config,
        "quit", "Quit GVD");

// Link to Exec mode
LINK_ROOT(node_quit, CLI_MODE_EXEC);

#endif //__GVD_CFG_DEFAULT_H__
