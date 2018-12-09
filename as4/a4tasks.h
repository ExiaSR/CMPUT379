/*
 * Copyright (C) 2018 Michael (Zichun) Lin
 * All rights reserved - Do Not Redistribute
*/

#ifndef A4TASKS_H
#define A4TASKS_H

#define NRES_TYPES 10 // max resources typ
#define NTASKS 25 // max tasks
#define DEBUG false

#include <string>
#include <vector>
#include <tuple>
#include <thread>
#include <chrono>
#include <map>

typedef enum {
  STATE_WAIT,
  STATE_RUN,
  STATE_IDLE
} TaskState;

typedef enum {
  STATE_FREE,
  STATE_BUSY
} ResourceState;

struct Resource {
  std::string name;
  int units; // how many instances are avaliable
  int held; // how many instance are currently being held
  ResourceState state;
};

struct Task {
  std::string name;
  int busy_time;
  int idle_time;
  std::thread::id tid;
  std::vector<Resource> required_resources;
  TaskState state;
  int time_wait; // amout of time spent on waiting for resources
  int cnt; // number of iteration
};

void start_simulator(std::string, int, int);
void task_runner(std::string, int);
void monitor_runner(int);
void print_running_task(std::string, std::thread::id, int, int);
void print_resources_info();
void print_tasks_info();
void debug_before_release_resource(std::string, std::string);
void debug_after_release_resource(std::string, std::string);
void debug_before_acquire_resource(std::string, std::string);
void debug_after_acquire_resource(std::string, std::string);
void debug_fail_acquire_resouce(std::string, std::string);
std::tuple<std::map<std::string, Resource>, std::map<std::string, Task>> parse_input_file(std::string);

#endif
