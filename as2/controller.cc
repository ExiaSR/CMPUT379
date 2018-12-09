/*
 * Copyright (C) 2018 Every-fucking-one, except the Author
 *
 * This source code is licensed under the GLWTS Public License found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "controller.h"
#include <iostream>
#include <stdio.h>
#include <sstream>
#include <algorithm>
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>

using namespace std;

PacketStats controller_stats;
vector<Switch> switches;
vector<Session> sessions;

/**
 * pfd[0] -> STDIN
 * pfd[i+1] -> fifo
*/
void controller_handler(int num_of_switch) {
  pollfd pfd[num_of_switch + 1];
  controller_stats = init_controller_stats();

  // non-blocking I/O polling from STDIN
  pfd[0].fd = STDIN_FILENO;
  pfd[0].events = POLLIN;
  pfd[0].revents = 0;

  // init fifo
  for (int i = 0; i < num_of_switch; i++) {
    string in_fifo_name = get_fifo_name(i+1, 0);
    string out_fifo_name = get_fifo_name(0, i+1);

    int in_fifo_fd = open(in_fifo_name.c_str(), O_RDONLY | O_NONBLOCK);   // incoming pkt fifo
    int out_fifo_fd = open(out_fifo_name.c_str(), O_RDWR | O_NONBLOCK); // outgoing pkt fifo

    if (in_fifo_fd < 0 || out_fifo_fd < 0) {
      exit(EXIT_FAILURE);
    }

    Session session = {in_fifo_name, out_fifo_name, in_fifo_fd, out_fifo_fd, 0};
    sessions.push_back(session);

    pfd[i+1].fd = in_fifo_fd;
    pfd[i+1].events = POLLIN;
  }

  while(true) {
    // IO multiplexing
    int ret = poll(pfd, num_of_switch + 1, 1);
    if (ret < 0) {
      cout << "Failed to poll" << endl;
      exit(EXIT_FAILURE);
    }

    // handling STDIN
    if (pfd[0].revents & POLLIN) {
      char buffer[128];
      int len = read(STDIN_FILENO, buffer, sizeof(buffer));
      if (len > 0) {
        string command(buffer, len);
        // remove newline character from command
        // https://stackoverflow.com/a/1488815
        command.erase(remove(command.begin(), command.end(), '\n'), command.end());
        CMDType cmd_type = get_cmd_type(command);

        switch(cmd_type) {
          case CMD_LIST:
            controller_list_handler();
            break;
          case CMD_EXIT:
            controller_exit_handler(sessions);
            break;
          case CMD_NOTFOUND:
            cout << "Invalid command" << endl;
            break;
        }
      }
    }

    for (int i = 0; i < num_of_switch; i++) {
      if (pfd[i+1].revents & POLLIN) {
        char buffer[MAXBUF];
        // cout << "[cont] Reading from " << sessions[i].in_fifo_name << endl;
        read(sessions[i].in_fifo_fd, buffer, MAXBUF);
        vector<string> message = parse_message(string(buffer));
        controller_incoming_message_handler(message);
      }
    }
  }
}

void controller_incoming_message_handler(vector<string> message) {
  string type = message[0];
  cout << "Received: (src= sw" << message[1] << ", dest= cont) [" << message[0] << "]" << endl;
  if (type == "OPEN") {
    controller_stats.received[PKT_OPEN] += 1;
    int switch_num = stoi(message[1]);
    int port1 = stoi(message[2]) <= 0 ? -1 : stoi(message[2]);
    int port2 = stoi(message[3]) <= 0 ? -1 : stoi(message[3]);
    int port3_lo = stoi(message[4]);
    int port3_hi = stoi(message[5]);
    switches.push_back({ switch_num, port1, port2, { port3_lo, port3_hi } });
    send_message(sessions[switch_num-1].out_fifo_fd, PKT_ACK, "ACK 0");
    controller_stats.transmitted[PKT_ACK] += 1;
  } else if (type == "QUERY") {
    int switch_num = stoi(message[1]);
    controller_stats.received[PKT_QUERY] += 1;
    stringstream ss;
    ss << "ADD " << message[1] << " ";
    bool if_found = false;
    for (auto sw : switches) {
      if (sw.port3.low <= stoi(message[2]) && stoi(message[2]) <= sw.port3.high) {
        ss << sw.port1 << " " << sw.port2 << " " << sw.port3.low << " " << sw.port3.high;
        if_found = true;
        break;
      }
    }
    if (!if_found) ss << message[2];
    controller_stats.transmitted[PKT_ADD] += 1;
    send_message(sessions[switch_num-1].out_fifo_fd, PKT_ADD, ss.str());
  }
}

void controller_exit_handler(vector<Session> sessions) {
  for (auto session : sessions) {
    close(session.in_fifo_fd);
    close(session.out_fifo_fd);
  }
  exit(EXIT_SUCCESS);
}

PacketStats init_controller_stats() {
  PacketStats stats = {
    { {PKT_OPEN, 0}, {PKT_QUERY, 0} },
    { {PKT_ACK, 0}, {PKT_ADD, 0} }
  };
  return stats;
}

void controller_list_handler() {
  print_switch_info();
  print_controller_stats();
}

void print_switch_info() {
  cout << "Switch information: " << endl;
  for (auto s: switches) {
    cout << "[sw" << s.switch_num << "] port1= " << s.port1 << ", port2= " << s.port2 <<
    ", port3= " << s.port3.low << "-" << s.port3.high << endl;
  }
}

void add_switch() {

}

void print_controller_stats() {
  cout << "Packet Stats:" << endl;
  string received_stats_str = "    Received: ";
  string transmitted_stats_str = "    Transmitted: ";

  int counter = 0;
  for (auto stat : controller_stats.received) {
    stringstream ss;
    if (stat.first == PKT_OPEN) ss << "OPEN:" << stat.second;
    else if (stat.first == PKT_QUERY) ss << "QUERY:" << stat.second;

    if (counter != controller_stats.received.size() - 1) ss << ", ";

    received_stats_str += ss.str();
    counter += 1;
  }

  counter = 0;
  for (auto stat : controller_stats.transmitted) {
    stringstream ss;
    if (stat.first == PKT_ACK) ss << "ACK:" << stat.second;
    else if (stat.first == PKT_ADD) ss << "ADD:" << stat.second;

    if (counter != controller_stats.received.size() - 1) ss << ", ";

    transmitted_stats_str += ss.str();
    counter += 1;
  }

  // cout << received_stats_str << endl;
  // cout << transmitted_stats_str << endl;
  printf("%s\n", received_stats_str.c_str());
  printf("%s\n", transmitted_stats_str.c_str());
}
