/*
 * Copyright (C) 2018 Michael (Zichun) Lin
 * All rights reserved
*/

#include <string>
#include <vector>
#include <map>
#include <unordered_map>

using namespace std;

void start_monitor(void);
vector<map<string, string>> sort_ps_output(string);
vector<vector<string>> filter_monitored_processes(vector<map<string, string>>);
string pipe_to_string(string);
vector<string> tokenize_line_by_space(string);
void print_monitored_processes(vector<vector<string>>);
bool check_target_terminated(vector<vector<string>>);
void exit_monitor();
