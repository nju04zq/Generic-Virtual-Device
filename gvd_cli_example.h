#ifndef __GVD_CLI_EXAMPLE_H__
#define __GVD_CLI_EXAMPLE_H__

#include "gvd_cli_base.h"
#include "gvd_cli.h"

/* gvd show */

END(node_gvd_show_cmd_end, exec_gvd_show);

KEYWORD(node_gvd_show_cmd,
        node_gvd_show_cmd_end,
        NO_ALT,
        "show", "show GVD stuff");

KEYWORD(node_gvd_show,
        node_gvd_show_cmd,
        NO_ALT,
        "gvd", "GVD exec command");

// Link to Exec mode
LINK_ROOT(node_gvd_show, CLI_MODE_EXEC);

/* gvd-config */

END_SUBMODE(node_gvd_config_mode_end, exec_config_gvd, CLI_MODE_CONFIG_GVD);

KEYWORD(node_gvd_config_mode,
        node_gvd_config_mode_end,
        NO_ALT,
        "gvd-config", "GVD config mode");

/* gvd global */

END(node_gvd_global_end, exec_gvd_global);

KEYWORD(node_gvd_global,
        node_gvd_global_end,
        node_gvd_config_mode,
        "gvd-global", "GVD global config");

// Link to Exec mode
LINK_ROOT(node_gvd_global, CLI_MODE_CONFIG);

/* gvd local */

END(node_gvd_local_end, exec_gvd_local);

KEYWORD(node_gvd_local,
        node_gvd_local_end,
        NO_ALT,
        "gvd-local", "GVD local config");

// Link to Exec mode
LINK_ROOT(node_gvd_local, CLI_MODE_CONFIG_GVD);

#endif //__GVD_CLI_EXAMPLE_H__
