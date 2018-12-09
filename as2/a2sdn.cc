/*
 * Copyright (C) 2018 Every-fucking-one, except the Author
 *
 * This source code is licensed under the GLWTS Public License found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "a2sdn.h"
#include "controller.h"
#include "switch.h"
#include <sstream>
#include <iostream>
#include <unistd.h>
#include <string.h>

using namespace std;

int main(int argc, char *argv[]) {
  if (argc < 2) {
    cout << "Invalid argument" << endl;
    cout << "a2sdn cont <nSwitch>" << endl;
    cout << "a2sdn swi trafficFile [null|swj] [null|swk] IPLow-IPHigh" << endl;
    exit(EXIT_FAILURE);
  }

  if (argv[1] == string("cont") && argc == 3 && stoi(argv[2]) <= MAX_NSW) {
    controller_handler(stoi(argv[2]));
  } else if (string(argv[1]).find("sw") != string::npos && argc == 6) {
    IPRange ip_range = split_ip_range_string(argv[5]);
    int switch_num = stoi(string(argv[1]).substr(2));
    int left_switch_num = -1, right_switch_num = -1;
    if (argv[3] != string("null")) left_switch_num = stoi(string(argv[3]).substr(2));
    if (argv[4] != string("null")) right_switch_num = stoi(string(argv[4]).substr(2));
    switch_handler(switch_num, left_switch_num, right_switch_num, argv[2], ip_range);
  } else {
    cout << "Invalid argument" << endl;
    exit(EXIT_FAILURE);
  }
  return 0;
}

string get_fifo_name(int from, int to) {
  stringstream ss;
  ss << "fifo-" << from << "-" << to;
  return ss.str();
}

CMDType get_cmd_type(string command) {
  if (command == "list") {
    return CMD_LIST;
  } else if (command == "exit") {
    return CMD_EXIT;
  } else {
    return CMD_NOTFOUND;
  }
}

IPRange split_ip_range_string(string input) {
  stringstream ss(input);
  string item;
  vector<int> placeholder;
  IPRange ip_range;

  while (getline(ss, item, '-')) {
    placeholder.push_back(stoi(item));
  }

  ip_range.low = placeholder[0];
  ip_range.high = placeholder[1];

  return ip_range;
}

string action_type_tostring(ActionType type) {
  if (type == ACTION_FORWARD) return "FORWARD";
  else if (type == ACTTION_DROP) return "DROP";
  else if (type == ACTION_DELIVER) return "DELIVER";
}

void send_message(int fd, PktType type, string message) {
  char buffer[message.length() + 1];
  strcpy(buffer, message.c_str());
  write(fd, buffer, sizeof(buffer));
}

vector<string> parse_message(string message) {
  stringstream ss(message);
  vector<string> placeholder;
  string buffer;

  while (getline(ss, buffer, ' ')) {
    placeholder.push_back(buffer);
  }
  return placeholder;
}
