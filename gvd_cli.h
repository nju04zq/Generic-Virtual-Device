#ifndef __GVD_CLI_CFG_DEFAULT_H__
#define __GVD_CLI_CFG_DEFAULT_H__

#include "gvd_cli_parser.h"

int
exec_show_time(struct cli_parser_info_s *cpi_p);

int
exec_show_version(struct cli_parser_info_s *cpi_p);

int
exec_logfile_flush(struct cli_parser_info_s *cpi_p);

int
exec_logfile_clear(struct cli_parser_info_s *cpi_p);

int
exec_config_term(struct cli_parser_info_s *cpi_p);

int
exec_quit(struct cli_parser_info_s *cpi_p);

int
exec_exit(struct cli_parser_info_s *cpi_p);

int
exec_end(struct cli_parser_info_s *cpi_p);

int
exec_shell_mode(struct cli_parser_info_s *cpi_p);

int
exec_shell_cmd(struct cli_parser_info_s *cpi_p);

int
exec_gvd_show(struct cli_parser_info_s *cpi_p);

int
exec_config_gvd(struct cli_parser_info_s *cpi_p);

int
exec_gvd_global(struct cli_parser_info_s *cpi_p);

int
exec_gvd_local(struct cli_parser_info_s *cpi_p);

#endif//__GVD_CLI_CFG_DEFAULT_H__
