/*
 * Copyright (C) 2018 Michael (Zichun) Lin
 * All rights reserved
*/

#include "controller.h"
#include <iostream>
#include <stdio.h>
#include <sstream>
#include <algorithm>
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>

using namespace std;

PacketStats controller_stats;
vector<Switch> switches;
// vector<Session> sessions;
vector<TCPSession> tcp_sessions(MAX_NSW + 1);
struct sockaddr_in addr_in;
int master_socket_fd;
pollfd controller_pfd[MAX_NSW + 2];
int current_socket_num;
pthread_t controller_tid;

/**
 * controller_pfd[0] -> STDIN
 * controller_pfd[i+1] -> fifo
*/
void controller_handler(int num_of_switch, int port) {
  // pollfd controller_pfd[num_of_switch + 2];
  controller_stats = init_controller_stats();

  // non-blocking I/O polling from STDIN
  controller_pfd[0].fd = STDIN_FILENO;
  controller_pfd[0].events = POLLIN;
  controller_pfd[0].revents = 0;

  master_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (master_socket_fd< 0) {
    cout << "Failed to open socket" << endl;
    exit(EXIT_FAILURE);
  }

  int on = 1;
  if(setsockopt(master_socket_fd, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on)) < 0) {
    perror("Server setsockopt() failed");
    close(master_socket_fd);
    exit(EXIT_FAILURE);
  }

  if(ioctl(master_socket_fd, FIONBIO, (char*)&on) < 0) {
    perror("server ioctl() failed");
    close(master_socket_fd);
    exit(EXIT_FAILURE);
  }

  memset((char*) &addr_in, 0, sizeof(addr_in));
  addr_in.sin_family = AF_INET;
  addr_in.sin_addr.s_addr = htonl(INADDR_ANY);
  addr_in.sin_port = htons(port);

  if (bind(master_socket_fd, (struct sockaddr*) &addr_in, sizeof(addr_in)) < 0) {
    cout << "Failed to bind address" << endl;
    close(master_socket_fd);
    exit(EXIT_FAILURE);
  }

  if (listen(master_socket_fd, num_of_switch) < 0) {
    cout << "Failed to start server" << endl;
    close(master_socket_fd);
    exit(EXIT_FAILURE);
  }

  tcp_sessions[0].socket_fd = master_socket_fd;
  controller_pfd[1].fd = master_socket_fd;
  controller_pfd[1].events = POLLIN;
  controller_pfd[1].revents = 0;

  if (pthread_create(&controller_tid, NULL, controller_keep_alive, NULL) != 0) {
    perror("Failed to create new thread");
    exit(EXIT_FAILURE);
  }

  current_socket_num = 1;
  while(true) {
    // IO multiplexing
    int ret = poll(controller_pfd, num_of_switch + 2, POLL_TIMEOUT);
    if (ret < 0) {
      cout << "Failed to poll" << endl;
      exit(EXIT_FAILURE);
    }

    // handling STDIN
    if (controller_pfd[0].revents & POLLIN) {
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
            controller_exit_handler();
            break;
          case CMD_DEBUG:
            for (auto f : controller_pfd) {
              cout << "controller_pfd fd: " << f.fd << endl;
            }
            controller_debug_handler();
            break;
          case CMD_NOTFOUND:
            cout << "Invalid command" << endl;
            break;
        }
      }
    }

    if ((current_socket_num < num_of_switch + 1) && (controller_pfd[1].revents & POLLIN)) {
      int idx;
      for (idx = 2; idx < num_of_switch + 2; idx++) {
        if (controller_pfd[idx].fd <= 0) break;
      }

      int s_idx = idx - 1;
      tcp_sessions[s_idx].socket_addr_len = sizeof(tcp_sessions[s_idx].socket_addr_len);
      tcp_sessions[s_idx].socket_fd = accept(master_socket_fd, (struct sockaddr*)&tcp_sessions[s_idx].socket_addr, &tcp_sessions[s_idx].socket_addr_len);
      tcp_sessions[s_idx].status = TCP_CONNECTED;
      tcp_sessions[s_idx].heartbeat_cnt = 0;
      // tcp_sessions[s_idx].file_read_fd = fdopen(tcp_sessions[s_idx].socket_fd, "r");
      // tcp_sessions[s_idx].file_write_fd = fdopen(tcp_sessions[s_idx].socket_fd, "w");

      controller_pfd[idx] = { tcp_sessions[s_idx].socket_fd, POLLIN, 0 };

      current_socket_num += 1;
    }

    for (int i = 2; i < num_of_switch + 2; i++) {
      if (controller_pfd[i].revents & POLLIN) {
        char buffer[MAXBUF];
        recv(tcp_sessions[i-1].socket_fd, buffer, sizeof(buffer), 0);
        // read(tcp_sessions[i-1].socket_fd, buffer, MAXBUF);
        if (DEBUG) cout << "\nReceived (socket): " << string(buffer) << "\n" << endl;
        vector<string> message = parse_message(string(buffer));
        controller_incoming_message_handler(message);
      }
    }
  }
}

void *controller_keep_alive(void *arg) {
  while (true) {
    for (int i = 1; i < MAX_NSW + 1; i++) {
      if (tcp_sessions[i].socket_fd <= 0) continue;

      if (tcp_sessions[i].heartbeat_cnt == HEARTBEAT_THRESHOLD) {
        tcp_sessions[i].status = TCP_DISCONNECTED;
        cout << "Lost connection to sw" << i+1 << endl;
      } else if (tcp_sessions[i].heartbeat_cnt < 5) {
        tcp_sessions[i].heartbeat_cnt += 1;
      }
    }
    sleep(HEARTBEAT_INTERVAL);
  }
}

void controller_incoming_message_handler(vector<string> message) {
  string type = message[0];
  if (type != "HEARTBEAT" && type != "EXIT") cout << "Received: (src= sw" << message[1] << ", dest= cont) [" << message[0] << "]" << endl;
  if (type == "OPEN") {
    controller_stats.received[PKT_OPEN] += 1;
    int switch_num = stoi(message[1]);
    int port1 = stoi(message[2]) <= 0 ? -1 : stoi(message[2]);
    int port2 = stoi(message[3]) <= 0 ? -1 : stoi(message[3]);
    int port3_lo = stoi(message[4]);
    int port3_hi = stoi(message[5]);
    switches.push_back({ switch_num, port1, port2, { port3_lo, port3_hi } });
    send_message_socket(tcp_sessions[switch_num].socket_fd, PKT_ACK, "ACK 0");
    stringstream ss;
    ss << "    " << "(port0= cont, port1= ";
    port1 == -1 ? ss << "null" : ss << "sw" << port1;
    ss << ", port2= ";
    port2 == -1 ? ss << "null" : ss << "sw" << port2;
    ss << ", port3= " << port3_lo << "-" << port3_hi << ")";
    cout << ss.str() << endl;
    controller_stats.transmitted[PKT_ACK] += 1;
    cout << "Transmitted (src= cont, dest= sw" << switch_num << ") [ACK]" << endl;
  } else if (type == "QUERY") {
    int switch_num = stoi(message[1]);
    controller_stats.received[PKT_QUERY] += 1;
    int src_ip = stoi(message[2]);
    int dest_ip = stoi(message[3]);
    cout << "    header= (srcIP= " << src_ip << " destIP= " << dest_ip << ")" << endl;
    stringstream ss;
    stringstream debug_msg;
    ss << "ADD " << message[1] << " ";
    debug_msg << "    (srcIP= 0-1000, destIP= ";
    bool if_found = false;
    for (auto sw : switches) {
      if (sw.port3.low <= dest_ip && dest_ip <= sw.port3.high) {
        ss << sw.port1 << " " << sw.port2 << " " << sw.port3.low << " " << sw.port3.high;
        debug_msg << sw.port3.low << "-" << sw.port3.high << " action= FORWARD:" ;
        sw.port1 == switch_num ? debug_msg << "1" : debug_msg << "2";
        if_found = true;
        break;
      }
    }
    if (!if_found) ss << message[2];
    ss << " " << src_ip << " " << dest_ip;
    send_message_socket(tcp_sessions[switch_num].socket_fd, PKT_ADD, ss.str());
    controller_stats.transmitted[PKT_ADD] += 1;
    cout << "Transmitted (src= cont, dest= sw" << switch_num << ") [ADD]" << endl;
    if (!if_found) debug_msg << dest_ip << "-" << dest_ip << " action= DROP:0";
    debug_msg << ", pri= 4, pktCount= 0)";
    cout << debug_msg.str() << endl;
  } else if (type == "EXIT") {
    int switch_num = stoi(message[1]);
    shutdown(tcp_sessions[switch_num].socket_fd, SHUT_RDWR);
    close(tcp_sessions[switch_num].socket_fd);
    controller_pfd[switch_num+1].fd = -1;
    tcp_sessions[switch_num].socket_fd = -1;
    tcp_sessions[switch_num].heartbeat_cnt = 0;
    current_socket_num -= 1;
  }
  if (type == "HEARTBEAT") {
    int switch_num = stoi(message[1]);
    // cout << "beat " << tcp_sessions[switch_num]
    tcp_sessions[switch_num].heartbeat_cnt = 0;
    tcp_sessions[switch_num].status = TCP_CONNECTED;
  }
}

void controller_exit_handler() {
  for (auto session : tcp_sessions) {
    close(session.socket_fd);
  }
  close(master_socket_fd);
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

void controller_debug_handler() {
  for (auto s : tcp_sessions) {
    cout << "FD: " << s.socket_fd << endl;
  }
}
