/*
 * Copyright (C) 2018 Every-fucking-one, except the Author
 *
 * This source code is licensed under the GLWTS Public License found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <stack>
#include <set>
#include <iterator>
#include <numeric>
#include <thread>
#include <chrono>
#include <unordered_map>
#include <sys/resource.h>
#include <unistd.h>
#include <stdio.h>
#include "a1mon.h"

using namespace std;

struct rlimit cpu_time_limit = {
  600, // soft limit
  600  // hard limit
};

int target_pid;
int current_pid = getpid();
int interval = 3;
vector<vector<string>> monitored_processes;

int main(int argc, char *argv[]) {
  if (argc < 2) {
    cout << "error: missing target PID" << endl;
    cout << "Usage: ./a1mon <target_pid> [interval]" << endl;
    exit(EXIT_FAILURE);
  }

  // Set CPU time limit
  setrlimit(RLIMIT_CPU, &cpu_time_limit);

  try {
    target_pid = stoi(argv[1]);
    if (argc == 3) {
      interval = stoi(argv[2]);
    }
  } catch (invalid_argument const &e) {
    cerr << "Invalid input" << endl;
  }

  start_monitor();
  exit_monitor();
  exit(EXIT_SUCCESS);
}

void start_monitor() {
  bool terminal = false;
  int counter = 0;
  string command = "ps -u $USER -o user,pid,ppid,state,start,cmd --sort start";

  while (!terminal) {
    cout << "a1mon [counter= " << counter++ << ", pid= " << current_pid << ", target_pid= " <<
            target_pid << ", interval= " << interval << " sec]:" << endl;

    string ps_output_str = pipe_to_string(command);
    auto processes_list = sort_ps_output(ps_output_str);
    auto filtered_processes_list = filter_monitored_processes(processes_list);

    cout << ps_output_str << endl; // TODO uncomment when done
    cout << "-------------------------" << endl;
    cout << "List of monitored processes:" << endl;
    print_monitored_processes(filtered_processes_list);

    if (check_target_terminated(filtered_processes_list)) {
      terminal = true;
    } else {
      monitored_processes = filtered_processes_list;
    }

    // Sleep for [interval] secs.
    this_thread::sleep_for(chrono::seconds(interval));
  }
}

void exit_monitor() {
  cout << "a1mon: target appears to have terminated; cleaning up" << endl;

  for (auto process: monitored_processes) {
    cout << "terminating [" << process[0] << ", " << process[1] << "]" << endl;
  }

  cout << "exiting a1mon" << endl;
}

bool check_target_terminated(vector<vector<string>> processes) {
  string t_pid = to_string(target_pid);

  for (auto process: processes) {
    if (process[0] == t_pid) {
      return false;
    }
  }
  return true;
}

vector<vector<string>> filter_monitored_processes(vector<map<string, string>> processes_list) {
  vector<vector<string>> filtered_list;
  unordered_map<string, string> filtered_map;
  string t_pid = to_string(target_pid);

  // Find target process
  for (auto process: processes_list) {
    if (process["pid"] == t_pid) {
      filtered_map.insert({{ process["pid"], process["cmd"] }});
    } else if (process["ppid"] == t_pid) {
      filtered_map.insert({{ process["pid"], process["cmd"] }});
    }
  }

  for (vector<int>::size_type i = 0; i != filtered_list.size(); i++) {
    for (auto process: processes_list) {
      if (filtered_list[i][0] == process["ppid"]) {
        filtered_map.insert({{ process["pid"], process["cmd"] }});
      }
    }
  }

  for (auto process: filtered_map) {
    filtered_list.push_back(vector<string> { process.first, process.second });
  }

  return filtered_list;
}

void print_monitored_processes(vector<vector<string>> processes) {
  string monitor_processes_str = "[";
  int counter = 0;
  for (vector<int>::size_type i = 0; i != processes.size(); i++) {
    stringstream ss;
    if (i == (processes.size() -1)) {
      ss << counter << ":[" << processes[i][0] << "," << processes[i][1] << "]]";
    } else {
      ss << counter << ":[" << processes[i][0] << "," << processes[i][1] << "], ";
    }
    monitor_processes_str += ss.str();
    counter += 1;
  }

  cout << monitor_processes_str << endl;
}

vector<map<string, string>> sort_ps_output(string output_str) {
  vector<map<string, string>> processes_list;

  stringstream ss(output_str);
  string line;

  while(getline(ss, line, '\n')) {
    // lines.push_back(line);
    auto tokens = tokenize_line_by_space(line);
    if (tokens.size() > 0 && tokens[0] != "USER") {
      string pid = tokens[1];
      string ppid = tokens[2];
      string cmd;

      // Join tokenzie CMD back together
      for (vector<int>::size_type i = 5; i != tokens.size(); i++) {
        cmd += tokens[i];
        if (!(i == tokens.size() - 1)) {
          cmd += " ";
        }
      }

      processes_list.push_back(map<string, string> {
        { "pid", pid },
        { "ppid", ppid },
        { "cmd", cmd }
      });
    }
  }
  return processes_list;
}

vector<string> tokenize_line_by_space(string str) {
  istringstream iss(str);
  vector<string> tokenize_str;
  copy(istream_iterator<string>(iss),
        istream_iterator<string>(),
        back_inserter(tokenize_str));
  return tokenize_str;
}

// http://www.cplusplus.com/forum/beginner/117874/#msg642881
string pipe_to_string(string command) {
  FILE* file = popen(command.c_str(), "r" );

  if (file) {
    std::ostringstream stm;

    constexpr std::size_t MAX_LINE_SZ = 1024;
    char line[MAX_LINE_SZ];

    while(fgets(line, MAX_LINE_SZ, file)) stm << line << '\n';

    pclose(file);
    return stm.str();
  }
  return "";
}
