#include "gvd_util.h"
#include "gvd_common.h"
#include "gvd_cli_tree.h"
#include "gvd_cli_example.h"

static cli_tree_root_link_t *tree_root_links[] =
{
    &link_name(node_gvd_show, CLI_MODE_EXEC),
    &link_name(node_gvd_global, CLI_MODE_CONFIG),
    &link_name(node_gvd_local, CLI_MODE_CONFIG_GVD),
};

int
gvd_init_example_cli_tree (void)
{
    int rc;

    rc = cli_link_root_nodes(tree_root_links, ARRAY_LEN(tree_root_links));
    if (rc == -1) {
        return -1;
    }

    return 0;
}


