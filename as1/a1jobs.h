/*
 * Copyright (C) 2018 Every-fucking-one, except the Author
 *
 * This source code is licensed under the GLWTS Public License found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <string>

#define MAX_JOBS 32

using namespace std;

typedef enum {
  CMD_RUN,
  CMD_SUSPEND,
  CMD_RESUME,
  CMD_TERMINATE,
  CMD_LIST,
  CMD_EXIT,
  CMD_QUIT,
  CMD_NOTFOUND
} CMDType;

void start_clock(void);
void end_clock(void);
void print_cpu_time(void);
void start_shell(void);
CMDType check_cmd_type(string);
void cmd_run_handler(string);
int spawn(char**);
void add_new_job(pid_t, string);
void cmd_list_handler(void);
void cmd_suspend_handler(int);
void cmd_resume_handler(int);
void cmd_terminate_handler(int);
void cmd_exit_handler();
void check_job_exist(int);
