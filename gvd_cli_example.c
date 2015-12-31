#include "gvd_util.h"
#include "gvd_cli_parser.h"
#include "gvd_line_buffer.h"

int
exec_gvd_show (struct cli_parser_info_s *cpi_p)
{
    print_buffer_t *output_p = &cpi_p->cli_output;

    printb(output_p, "Dummy cmd, gvd show\n");
    return PROCESS_CONTINUE;
}

int
exec_config_gvd (struct cli_parser_info_s *cpi_p)
{
    return PROCESS_CONTINUE;
}

int
exec_gvd_global (struct cli_parser_info_s *cpi_p)
{
    print_buffer_t *output_p = &cpi_p->cli_output;
    char *prefix = "";

    if (cpi_p->set_no) {
        prefix = "no ";
    } else if (cpi_p->set_default) {
        prefix = "default ";
    }

    printb(output_p, "Dummy cmd, %sgvd globally config\n", prefix);
    return PROCESS_CONTINUE;
}

int
exec_gvd_local (struct cli_parser_info_s *cpi_p)
{
    print_buffer_t *output_p = &cpi_p->cli_output;
    char *prefix = "";

    if (cpi_p->set_no) {
        prefix = "no ";
    } else if (cpi_p->set_default) {
        prefix = "default ";
    }

    printb(output_p, "Dummy cmd, %sgvd locally config\n", prefix);
    return PROCESS_CONTINUE;
}

