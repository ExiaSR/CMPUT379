/*
 * Copyright (C) 2018 Michael (Zichun) Lin
 * All rights reserved - Do Not Redistribute
*/

#include "a4tasks.h"
#include "utils.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <tuple>
#include <regex>
#include <thread>
#include <mutex>
#include <chrono>
#include <map>

using namespace std;

map<string, Resource> resources_map;
map<string, Task> tasks_map;
vector<thread> workers;
bool worker_terminal;
chrono::time_point<chrono::high_resolution_clock> start_time;

mutex resource_mutex;
mutex task_mutex;

map<TaskState, string> state_name_map = {
  { STATE_IDLE, "IDLE" },
  { STATE_WAIT, "WAIT" },
  { STATE_RUN, "RUN" }
};

int main(int argc, char *argv[]) {
  start_time = chrono::high_resolution_clock::now();

  if (argc < 4) {
    cout << "Invalid argument" << endl;
    cout << "./a4tasks inputFile monitorTime NITER" << endl;
  }

  string input_file = argv[1];
  int monitoring_interval = stoi(argv[2]);
  int num_iteration = stoi(argv[3]);

  start_simulator(input_file, num_iteration, monitoring_interval);

  return 0;
}

void start_simulator(string input_file, int num_iteration, int monitoring_interval) {
  tie(resources_map, tasks_map) = parse_input_file(input_file);

  worker_terminal = false; // indicator for monitor thread to exit

  // create workers thread
  for (auto ele : tasks_map) {
    workers.push_back(thread(task_runner, ele.first, num_iteration));
  }

  // create monitor thread
  thread monitor_thread(monitor_runner, monitoring_interval);

  for (auto& worker : workers) {
    worker.join();
  }

  // terminate monitor thread
  worker_terminal = true;
  monitor_thread.join();

  // wrap up and terminate
  print_resources_info();
  print_tasks_info();

  auto current_time = chrono::high_resolution_clock::now();
  cout << "Running time= " << chrono::duration_cast<chrono::milliseconds>(current_time - start_time).count() << " msec" << endl;
}

void task_runner(string task_name, int num_iteration) {
  Task current_task = tasks_map[task_name];
  tasks_map[task_name].tid = this_thread::get_id();
  tasks_map[task_name].cnt = 0;
  current_task = tasks_map[task_name];

  for (int run = 0; run < num_iteration; run++) {
    auto start_acquire_resources = chrono::high_resolution_clock::now();

    // update state to WAIT
    task_mutex.lock();
    tasks_map[task_name].state = STATE_WAIT;
    task_mutex.unlock();

    while (true) {
      if (resource_mutex.try_lock()) {
        bool flag = false; // true -> lack of resource, false -> all good and acquire resources
        for (auto r : current_task.required_resources) {
          if (!((resources_map[r.name].units - resources_map[r.name].held) >= r.units)) {
            debug_fail_acquire_resouce(current_task.name, r.name);
            flag = true;
            break;
          }
        }
        if (flag) {
          resource_mutex.unlock();
          this_thread::sleep_for(chrono::milliseconds(100));
        } else {
          for (auto r : current_task.required_resources) {
            debug_acquire_resource(current_task.name, r.name);
            resources_map[r.name].held = r.units; // all resoucres are avalibale, hold resouce.
          }
          resource_mutex.unlock();
          break;
        }
      } else {
        this_thread::sleep_for(chrono::milliseconds(100));
      }
    }
    auto finish_acquire_resources = chrono::high_resolution_clock::now();
    tasks_map[task_name].time_wait += chrono::duration_cast<chrono::milliseconds>(finish_acquire_resources - start_acquire_resources).count();

    // update state to RUN
    task_mutex.lock();
    tasks_map[task_name].state = STATE_RUN;
    task_mutex.unlock();

    // simulate busy time
    this_thread::sleep_for(chrono::milliseconds(current_task.busy_time));

    for (auto r : current_task.required_resources) {
      lock_guard<mutex> lock(resource_mutex);
      resources_map[r.name].held -= r.units;
      debug_release_resource(current_task.name, r.name);
    }

    // update state to IDLE
    task_mutex.lock();
    tasks_map[task_name].state = STATE_IDLE;
    task_mutex.unlock();

    // simulate idle time
    this_thread::sleep_for(chrono::milliseconds(current_task.idle_time));

    // wrap up current iteration
    auto current_time = chrono::high_resolution_clock::now();
    auto time_spent = chrono::duration_cast<chrono::milliseconds>(current_time - start_time).count();
    print_running_task(current_task.name, current_task.tid, run+1, time_spent);
    tasks_map[task_name].cnt += 1;
  }
}

void monitor_runner(int interval) {
  while (!worker_terminal) {
    lock_guard<mutex> lock(task_mutex); // make sure no task state change when printing
    stringstream wait_buf;
    stringstream run_buf;
    stringstream idle_buf;
    wait_buf << "    [WAIT] ";
    run_buf << "    [RUN] ";
    idle_buf << "    [IDLE] ";
    for (auto ele : tasks_map ) {
      Task t = ele.second;
      switch (t.state) {
        case STATE_WAIT:
          wait_buf << t.name << " ";
          break;
        case STATE_RUN:
          run_buf << t.name << " ";
          break;
        case STATE_IDLE:
          idle_buf << t.name << " ";
          break;
      }
    }
    cout << "monitor:" << "\n" << wait_buf.str() << "\n" << run_buf.str() << "\n" << idle_buf.str() << endl;
    this_thread::sleep_for(chrono::milliseconds(interval));
  }
}

void print_running_task(string name, thread::id tid, int iteration, int time) {
  cout << "task: " << name << " (tid= " << tid << ", iter= " << iteration << ", time= " << time << " msec)" << endl;
}

void print_resources_info() {
  stringstream ss;
  ss << "System Resources:\n";
  for (auto ele : resources_map) {
    Resource r = ele.second;
    ss << "    " << ele.first << ": (maxAvail= " << r.units << " , held= " << r.held << ")" << endl;
  }
  cout << ss.str();
}

void print_tasks_info() {
  stringstream ss;
  ss << "System Tasks:" << endl;
  int cnt = 0;

  for (auto ele : tasks_map) {
    Task t = ele.second;
    ss << "[" << cnt << "] " << t.name << endl;
    ss << "    (" << state_name_map[t.state] << " runTime: " << t.busy_time << " msec, idleTime= " << t.idle_time << " msec):" << endl;
    ss << "    (tid= " << t.tid << ")" << endl;
    for (auto r : t.required_resources) {
      ss << "    " << r.name << ": (needed= " << r.units << ", held= " << r.held << ")" << endl;
    }
    ss << "    (RUN: " << t.cnt << " times, WAIT: " << t.time_wait << " msec)" << endl;
    cnt += 1;
  }

  cout << ss.str();
}

// parse input file
tuple<map<string, Resource>, map<string, Task>> parse_input_file(string input_file) {
  vector<string> lines;
  ifstream file(input_file);
  string buffer;

  // seperate line by space
  while (getline(file, buffer)) {
    lines.push_back(buffer);
  }

  map<string, Resource> r_map;
  map<string, Task> t_map;
  for (auto line : lines) {
    // skip comment or empty line
    if (line.find("#") == string::npos && !line.empty()) {
      regex ws_re("\\s+");
      vector<string> placeholder {
        sregex_token_iterator(line.begin(), line.end(), ws_re, -1), {}
      };

      if (placeholder[0] == "resources") {
        for (int i = 1; i < placeholder.size(); i++) {
          auto tokens = split(placeholder[i], ':');
          int units = stoi(tokens[1]);
          r_map.insert({ tokens[0], { tokens[0], units, 0, STATE_FREE } });
        }
      } else if (placeholder[0] == "task") {
        Task task = { placeholder[1], stoi(placeholder[2]), stoi(placeholder[3]) };
        vector<Resource> task_required_resource;
        for (int i = 4; i < placeholder.size(); i++) {
          auto tokens = split(placeholder[i], ':');
          task.required_resources.push_back({ tokens[0], stoi(tokens[1]), 0 }); // TODO
          task.state = STATE_WAIT;
        }
        t_map.insert({ task.name, task });
      }
    }
  }

  return make_tuple(r_map, t_map);
}

void debug_release_resource(string task, string resource) {
  if (DEBUG) {
    cout << task << " released " << resource << endl;
    cout << resource << ": maxAvaliable: " << resources_map[resource].units << " held: " << resources_map[resource].held << endl;
    cout << endl;
  }
}

void debug_acquire_resource(string task, string resource) {
  if (DEBUG) {
    cout << task << " acquiring " << resource << endl;
    cout << resource << ": maxAvaliable: " << resources_map[resource].units << " held: " << resources_map[resource].held << endl;
    cout << endl;
  }
}

void debug_fail_acquire_resouce(string task, string resource) {
  if (DEBUG) {
    cout << task << " fail to acquire " << resource << endl;
    cout << resource << ": maxAvaliable: " << resources_map[resource].units << " held: " << resources_map[resource].held << endl;
    cout << endl;
  }
}
