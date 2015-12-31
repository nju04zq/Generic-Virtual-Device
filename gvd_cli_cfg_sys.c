#include <pwd.h>
#include <time.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include "gvd_tty.h"
#include "gvd_util.h"
#include "gvd_common.h"
#include "gvd_cli_tree.h"
#include "gvd_cli_parser.h"
#include "gvd_line_buffer.h"

#define COMPILE_TIME_MAX_LEN 31
#define USER_NAME_MAX_LEN 31
#define EXE_LOCATION_MAX_LEN 255
#define UPTIME_MAX_LEN 127
#define MEM_INFO_MAX_LEN 31

#define SECS_PER_MIN  (60)
#define SECS_PER_HOUR (60*SECS_PER_MIN)
#define SECS_PER_DAY  (24*SECS_PER_HOUR)
#define SECS_PER_WEEK (7*SECS_PER_DAY)

int
exec_show_time (struct cli_parser_info_s *cpi_p)
{
    time_t time_value;
    struct tm tm; 
    char time_str[64];
    print_buffer_t *output_p = &cpi_p->cli_output;

    time(&time_value);
    localtime_r(&time_value, &tm);

    strftime(time_str, sizeof(time_str), "%a %b %d %T %Z %Y", &tm);
    printb(output_p, "%s\n", time_str);

    return PROCESS_CONTINUE;
}

static void
get_exe_file_info (char *compile_time, char *user_name, char *exe_location)
{
    struct stat stat_info;
    struct tm tm;
    struct passwd *pwd;
    int rc;

    memset(compile_time, 0, COMPILE_TIME_MAX_LEN+1);
    memset(user_name, 0, USER_NAME_MAX_LEN+1);
    memset(exe_location, 0, EXE_LOCATION_MAX_LEN+1);
    
    rc = readlink("/proc/self/exe", exe_location, EXE_LOCATION_MAX_LEN);
    if (rc == -1) {
        return;
    }
    
    rc = stat(exe_location, &stat_info);
    if (rc == -1) {
        return;
    }

    localtime_r(&stat_info.st_mtime, &tm);
    strftime(compile_time, COMPILE_TIME_MAX_LEN+1, "%a %d-%b-%y %H:%M", &tm);

    pwd = getpwuid(stat_info.st_uid);
    if (!pwd) {
        return;
    }
    safe_strncpy(user_name, pwd->pw_name, USER_NAME_MAX_LEN);

    return;
}

static void
get_uptime (long up_sec, char *uptime)
{
    long weeks, days, hours, minutes;
    char *week, *day, *hour, *minute;

    weeks = up_sec/SECS_PER_WEEK;
    up_sec %= SECS_PER_WEEK;
    days = up_sec/SECS_PER_DAY;
    up_sec %= SECS_PER_DAY;
    hours = up_sec/SECS_PER_HOUR;
    up_sec %= SECS_PER_HOUR;
    minutes = up_sec/SECS_PER_MIN;

    week = (weeks<=1)?"week":"weeks";
    day = (days<=1)?"day":"days";
    hour = (hours<=1)?"hour":"hours";
    minute = (minutes<=1)?"minute":"minutes";

    if (weeks == 0 && days == 0 && hours == 0) {
        snprintf(uptime, UPTIME_MAX_LEN+1, "%lu %s",
                 minutes, minute);
    } else if (weeks == 0 && days == 0) {
        snprintf(uptime, UPTIME_MAX_LEN+1, "%lu %s, %lu %s",
                 hours, hour, minutes, minute);
    } else if (weeks == 0) {
        snprintf(uptime, UPTIME_MAX_LEN+1, "%lu %s, %lu %s, %lu %s",
                 days, day, hours, hour, minutes, minute);
    } else {
        snprintf(uptime, UPTIME_MAX_LEN+1, "%lu %s, %lu %s, %lu %s, %lu %s",
                 weeks, week, days, day, hours, hour, minutes, minute);
    }

    return;
}

static void
get_mem_info (struct sysinfo *sys_info_p, char *total_mem)
{
    long long unsigned int total_mem_num;

    total_mem_num = sys_info_p->totalram * sys_info_p->mem_unit;
    snprintf(total_mem, MEM_INFO_MAX_LEN+1, "%llu KB", total_mem_num/1024);
    return;
}

int
exec_show_version (struct cli_parser_info_s *cpi_p)
{
    print_buffer_t *output_p = &cpi_p->cli_output;
    char compile_time[COMPILE_TIME_MAX_LEN+1];
    char user_name[USER_NAME_MAX_LEN+1];
    char exe_location[EXE_LOCATION_MAX_LEN+1];
    char uptime[UPTIME_MAX_LEN+1];
    char total_mem[MEM_INFO_MAX_LEN+1];
    struct utsname u_name;
    struct sysinfo sys_info;

    get_exe_file_info(compile_time, user_name, exe_location);
    uname(&u_name);
    sysinfo(&sys_info);
    get_uptime(sys_info.uptime, uptime);
    get_mem_info(&sys_info, total_mem);

    printb(output_p, "GenericCallHome Vritual Device(GVD)\n");
    printb(output_p, "Compiled %s by %s\n", compile_time, user_name);
    printb(output_p, "\n");
    printb(output_p, "%s uptime is %s\n", u_name.nodename, uptime);
    printb(output_p, "System version: %s\n", u_name.version);
    printb(output_p, "\n");
    printb(output_p, "%s release version: %s\n", u_name.sysname,u_name.release);
    printb(output_p, "Platform %s, total memory %s, total process %d\n",
           u_name.machine, total_mem, sys_info.procs);
    printb(output_p, "GVD run as %s\n\n", exe_location);

    return PROCESS_CONTINUE;
}

int
exec_logfile_flush (struct cli_parser_info_s *cpi_p)
{
    printv("All console content dumped.\n");
    dump_all_lines_to_disk();
    return PROCESS_CONTINUE;
}

int
exec_logfile_clear (struct cli_parser_info_s *cpi_p)
{
    clear_disk_logfile();
    return PROCESS_CONTINUE;
}

int
exec_quit (struct cli_parser_info_s *cpi_p)
{
    return PROCESS_EXIT;
}

int
exec_exit (struct cli_parser_info_s *cpi_p)
{
    gvd_return_upper_cli_mode(cpi_p->tty_p);
    return PROCESS_CONTINUE;
}

int
exec_end (struct cli_parser_info_s *cpi_p)
{
    gvd_return_exec_cli_mode(cpi_p->tty_p);
    return PROCESS_CONTINUE;
}

int
exec_shell_mode (struct cli_parser_info_s *cpi_p)
{
    return PROCESS_CONTINUE;
}

int
exec_shell_cmd (struct cli_parser_info_s *cpi_p)
{
    char *cmd;
    print_buffer_t *output_p = &cpi_p->cli_output;

    cmd = GET_OBJ(P_STRING, 0);
    run_shell_cmd(cmd, output_p);
    printb(output_p, "\n\n");
    return PROCESS_CONTINUE;
}

