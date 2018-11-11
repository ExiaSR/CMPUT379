/*
 * Copyright (C) 2018 Michael (Zichun) Lin
 * All rights reserved
*/

#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <map>
#include <list>
#include <vector>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/times.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include "a1jobs.h"

map<string, CMDType> cmd_type_map = {
  { "run", CMD_RUN },
  { "suspend", CMD_SUSPEND },
  { "resume", CMD_RESUME },
  { "terminate", CMD_TERMINATE },
  { "list", CMD_LIST },
  { "exit", CMD_EXIT },
  { "quit", CMD_QUIT }
};

struct rlimit cpu_time_limit = {
  600, // soft limit
  600  // hard limit
};

clock_t start_time, end_time;
struct tms start_cpu_time, end_cpu_time;
int current_pid = getpid();
vector<map<string, string>> jobs_list;


int main(int argc, char *argv[]) {
  // Set CPU time limit
  setrlimit(RLIMIT_CPU, &cpu_time_limit);
  start_clock();

  start_shell();

  end_clock();
  print_cpu_time();

  return 0;
}

void start_shell() {
  bool terminal = false;
  string cmd;

  while (!terminal) {
    cout << "a1jobs[" << current_pid << "]: ";
    getline(cin, cmd);
    CMDType type = check_cmd_type(cmd);
    try {
      switch (type) {
        case CMD_RUN:
          cmd_run_handler(cmd.substr(4));
          break;
        case CMD_SUSPEND:
          cmd_suspend_handler(stoi(cmd.substr(8)));
          break;
        case CMD_RESUME:
          cmd_resume_handler(stoi(cmd.substr(7)));
          break;
        case CMD_TERMINATE:
          cmd_terminate_handler(stoi(cmd.substr(10)));
          break;
        case CMD_LIST:
          cmd_list_handler();
          break;
        case CMD_EXIT:
          cmd_exit_handler();
          terminal = true; // break the loop
          break;
        case CMD_QUIT:
          break;
        case CMD_NOTFOUND:
          cout << "Unsupported command, please try again" << endl;
      }
    } catch (const out_of_range& e) {
      cout << "Unexpected argument" << endl;
      exit(EXIT_FAILURE);
    } catch (const exception& e) {
      cout << e.what() << endl;
    }
  }
}

void cmd_run_handler(string cmd) {
  vector<string> argument_list;
  istringstream cmd_stream(cmd);
  for (string argument; cmd_stream >> argument; ) {
    argument_list.push_back(argument);
  }

  char **argument_array = new char*[argument_list.size()];
  for(size_t i = 0; i < argument_list.size(); i++){
      argument_array[i] = new char[argument_list[i].size() + 1];
      strcpy(argument_array[i], argument_list[i].c_str());
  }

  // Fork and run command
  pid_t child_pid = spawn(argument_array);
  if (child_pid == -1) {
    string error_message = "Unable to spawn child process";
    throw runtime_error(error_message.c_str());
  } else {
    add_new_job(child_pid, cmd);
  }
}

void cmd_list_handler() {
  int job_index = 0;
  for (map<string, string> item: jobs_list) {
    if (!item["pid"].empty()) {
      cout << job_index << ": (pid= " << item["pid"] << ", " <<
              "cmd= " << item["cmd"] << ")" << endl;
    }
    job_index += 1;
  }
}

void cmd_suspend_handler(int job_id) {
  check_job_exist(job_id);
  int pid = stoi(jobs_list[job_id]["pid"]);
  kill((pid_t) pid, SIGSTOP);
}

void cmd_resume_handler(int job_id) {
  check_job_exist(job_id);
  int pid = stoi(jobs_list[job_id]["pid"]);
  kill((pid_t) pid, SIGCONT);
}

void cmd_terminate_handler(int job_id) {
  check_job_exist(job_id);
  int pid = stoi(jobs_list[job_id]["pid"]);
  kill((pid_t) pid, SIGKILL);
}

void cmd_exit_handler() {
  int job_index = 0;
  for (map<string, string> item: jobs_list) {
    if (!item["pid"].empty()) {
      pid_t pid = (pid_t) stoi(item["pid"]);
      kill(pid, SIGKILL);
      cout << "    " << "job " << pid << " terminated" << endl;
    }
    job_index += 1;
  }
}

// Spawn a new child process and return PID
int spawn(char **argument_array) {
	pid_t child_pid;

	child_pid = fork();

  if (child_pid == 0) {
    execvp(argument_array[0], argument_array);
    exit(0);
	} else {
    return child_pid;
  }
}

// Add new job to job list
void add_new_job(pid_t pid, string cmd) {
  map<string, string> job = {
    { "pid", to_string(pid) },
    { "cmd", cmd }
  };
  jobs_list.push_back(job);
}

// Check command type
CMDType check_cmd_type(string cmd) {
  for (auto const& x : cmd_type_map) {
    if (cmd.find(x.first, 0) == 0) {
      return x.second;
    }
  }
  return CMD_NOTFOUND;
}

void check_job_exist(int job_id) {
  if (job_id < 0 || job_id > jobs_list.size() || jobs_list[job_id].empty()) {
    string error_message = "Job does not exist";
    throw runtime_error(error_message.c_str());
  }
}

void start_clock() {
  start_time = times(&start_cpu_time);
}

void end_clock() {
  end_time = times(&end_cpu_time);
}

void print_cpu_time() {
  static long clktct = 0;

  // APUE Section 8.17
  if (clktct == 0) {
    clktct = sysconf(_SC_CLK_TCK);
  }

  // https://stackoverflow.com/a/5907076/4557739
  cout << fixed;
  cout << setprecision(2);

  cout << "real: " << (end_time - start_time) / (double) clktct << " sec." << endl;
  cout << "user: " << (end_cpu_time.tms_utime - start_cpu_time.tms_utime) / (double) clktct << " sec." << endl;
  cout << "system: " << (end_cpu_time.tms_stime - start_cpu_time.tms_stime) / (double) clktct << " sec." << endl;
  cout << "child user: " << (end_cpu_time.tms_cutime - start_cpu_time.tms_cutime) / (double) clktct << " sec." << endl;
  cout << "child sys: " << (end_cpu_time.tms_cstime - start_cpu_time.tms_cstime) / (double) clktct << " sec." << endl;
}
