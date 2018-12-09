/*
 * Copyright (C) 2018 Every-fucking-one, except the Author
 *
 * This source code is licensed under the GLWTS Public License found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "switch.h"
#include <stdio.h>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <fstream>
#include <regex>
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>

using namespace std;

int switch_num;
// int left_switch;
// int right_switch;
vector<Session> switch_sessions;
vector<FlowTableEntry> flow_table;

PacketStats switch_stats;
Mode switch_mode = MODE_DISCONNECTED;

map<string, PktType> name_pkt_map = {
  { "OPEN", PKT_OPEN },
  { "ACK", PKT_ACK },
  { "QUERY", PKT_QUERY },
  { "ADD", PKT_ADD },
  { "RELAY", PKT_RELAY },
  { "ADMIT", PKT_ADMIT }
};

/*
 * 0 -> controller
 * 1 -> left_switch
 * 2 -> right_switch
 * 3 -> ip_range
*/
void switch_handler(int num, int left_switch, int right_switch, string traffic_file, IPRange ip_range) {
  switch_num = num;
  // left_switch = left_switch;
  // right_switch = right_switch;
  switch_sessions = init_session(switch_num, left_switch, right_switch);
  pollfd pfd[switch_sessions.size() + 1];
  flow_table = init_flow_table(ip_range);
  switch_stats = init_switch_stats();
  // vector<TrafficRule> traffic_rule = parse_traffic_file(traffic_file);
  vector<string> traffic_file_cache = cache_traffic_file(traffic_file);

  // non-blocking I/O polling from STDIN
  pfd[0].fd = STDIN_FILENO;
  pfd[0].events = POLLIN;
  pfd[0].revents = 0;

  for (int i = 0; i < switch_sessions.size(); i++) {
    pfd[i+1].fd = switch_sessions[i].in_fifo_fd;
    pfd[i+1].events = POLLIN;
  }

  send_open_to_controller(switch_num, left_switch, right_switch);

  int rule_cnt = 0;
  while (true) {
    // fflush(stdout);
    // IO multiplexing
    int ret = poll(pfd, switch_sessions.size() + 1, 1);

    if (ret < 0) {
      cout << "Failed to poll" << endl;
      exit(EXIT_FAILURE);
    }

    // read traffic file
    if (rule_cnt < traffic_file_cache.size() && switch_mode == MODE_CONNECTED) {
      TrafficRule rule = parse_traffic_rule(traffic_file_cache[rule_cnt]);
      if (rule.switch_num == switch_num && ip_range.low <= rule.dest_ip && rule.dest_ip <= ip_range.high) {
        // found match in flow table, admit packet
        flow_table[0].pkt_count += 1;
        switch_stats.received[PKT_ADMIT] += 1;
      } else if (rule.switch_num == switch_num) {
        switch_stats.received[PKT_ADMIT] += 1;
        int rule_index = find_matching_rule(rule.dest_ip);
        if (rule_index > 0) {
          // find matched rule send relay message
          int session_idx = find_matching_session(flow_table[rule_index].action_value);
          stringstream ss;
          ss << "RELAY " << switch_num;
          flow_table[rule_index].pkt_count += 1;
          switch_stats.transmitted[PKT_RELAY] += 1;
          send_message(switch_sessions[session_idx].out_fifo_fd, PKT_RELAY, ss.str());
        } else {
          // send QUERY to controller
          stringstream ss;
          ss << "QUERY " << switch_num << " " << rule.dest_ip;
          switch_stats.transmitted[PKT_QUERY] += 1;
          switch_mode = MODE_WAITINGADD;
          send_message(switch_sessions[0].out_fifo_fd, PKT_OPEN, ss.str());
        }
      }
      rule_cnt += 1;
    }

    // STDIN
    if (pfd[0].revents & POLLIN) {
      char buffer[128];
      int len = read(STDIN_FILENO, buffer, sizeof(buffer));
      if (len > 0) {
        string command(buffer, len);
        command.erase(remove(command.begin(), command.end(), '\n'), command.end());
        CMDType cmd_type = get_cmd_type(command);

        switch(cmd_type) {
          case CMD_LIST:
            switch_list_handler();
            break;
          case CMD_EXIT:
            switch_exit_handler();
            break;
          case CMD_NOTFOUND:
            cout << "Invalid command" << endl;
            break;
        }
      }
    }

    // FIFO
    for (int i = 0; i < switch_sessions.size(); i++) {
      if (pfd[i+1].revents & POLLIN) {
        char buffer[MAXBUF];
        read(switch_sessions[i].in_fifo_fd, buffer, MAXBUF);
        switch_incoming_message_handler(parse_message(string(buffer)));
      }
    }
  }
}

void switch_incoming_message_handler(vector<string> message) {
  if (message.size() > 1) {
    PktType type = name_pkt_map[message[0]];
    switch_stats.received[type] += 1;

    switch (type) {
      case PKT_ADD:
        cout << "Received: (src= cont) [ADD]" << endl;
        if (message.size() == 3) {
          // not found
          add_drop_rule(stoi(message[2]));
        } else if (message.size() == 6) {
          // update flow table and relay packet
          int forwarding_port = stoi(message[2]) == switch_num ? 2 : 1;
          int flow_table_idx = add_forward_rule(forwarding_port, { stoi(message[4]), stoi(message[5]) });
          int session_idx = find_matching_session(forwarding_port);
          stringstream ss;
          ss << "RELAY " << switch_num;
          if (session_idx > 0) send_message(switch_sessions[session_idx].out_fifo_fd, PKT_RELAY, ss.str());
          switch_stats.transmitted[PKT_RELAY] += 1;
          flow_table[flow_table_idx].pkt_count += 1;
        }
        switch_mode = MODE_CONNECTED;
        break;
      case PKT_RELAY:
        cout << "Received: (src= sw" << message[1] << ") [RELAY]" << endl;
        flow_table[0].pkt_count += 1;
        break;
      case PKT_ACK:
        cout << "Received: (src= cont) [ACK]" << endl;
        switch_mode = MODE_CONNECTED;
        break;
    }
  }
}

int add_forward_rule(int port, IPRange dest) {
  flow_table.push_back({
    { 0, MAXIP },
    { dest.low, dest.high },
    ACTION_FORWARD,
    port,
    MINPRI,
    0
  });
  return flow_table.size() - 1;
}

void add_drop_rule(int ip) {
  flow_table.push_back({
    { 0, MAXIP },
    { ip, ip },
    ACTTION_DROP,
    0,
    MINPRI,
    1
  });
}

int find_matching_rule(int dest_ip) {
  int i = 0;
  int result_index = -1;
  for (auto entry : flow_table) {
    if (entry.dest_range.low <= dest_ip && dest_ip <= entry.dest_range.high) {
      result_index = i;
      break;
    }
    i += 1;
  }
  return result_index;
}

int find_matching_session(int port) {
  int cnt = 0;
  int result = -1;
  for (auto s : switch_sessions) {
    if (s.port == port) {
      result = cnt;
      break;
    }
    cnt += 1;
  }
  return result;
}

void send_open_to_controller(int num, int left_switch, int right_switch) {
  stringstream ss;
  ss << "OPEN" << " " << num << " " << left_switch << " " << right_switch << " " <<
  flow_table[0].dest_range.low << " " << flow_table[0].dest_range.high;
  send_message(switch_sessions[0].out_fifo_fd, PKT_OPEN, ss.str());
  switch_stats.transmitted[PKT_OPEN] += 1;
}

vector<FlowTableEntry> init_flow_table(IPRange ip_range) {
  vector<FlowTableEntry> flow_table = {{
    { 0, MAXIP },
    { ip_range.low, ip_range.high },
    ACTION_DELIVER,
    3,
    MINPRI,
    0
  }};

  return flow_table;
}

vector<Session> init_session(int switch_num, int left, int right) {
  string in_fifo_name = get_fifo_name(0, switch_num);
  string out_fifo_name = get_fifo_name(switch_num, 0);
  int in_fifo_fd = open(in_fifo_name.c_str(), O_RDONLY | O_NONBLOCK);
  int out_fifo_fd = open(out_fifo_name.c_str(), O_RDWR | O_NONBLOCK);
  if (out_fifo_fd < 0) {
    fprintf(stderr, "Failed to open fifo");
    exit(EXIT_FAILURE);
  }
  vector<Session> sessions = {
    {
      in_fifo_name, out_fifo_name, in_fifo_fd, out_fifo_fd, 0
    }
  };

  if (left > 0) {
    sessions.push_back({
      get_fifo_name(left, switch_num), // in
      get_fifo_name(switch_num, left), // out
      open(get_fifo_name(left, switch_num).c_str(), O_RDWR | O_NONBLOCK),
      open(get_fifo_name(switch_num, left).c_str(), O_RDWR | O_NONBLOCK),
      1
    });
  }

  if (right > 0) {
    sessions.push_back({
      get_fifo_name(right, switch_num), // in
      get_fifo_name(switch_num, right), // out
      open(get_fifo_name(right, switch_num).c_str(), O_RDWR | O_NONBLOCK),
      open(get_fifo_name(switch_num, right).c_str(), O_RDWR | O_NONBLOCK),
      2
    });
  }

  return sessions;
}

void switch_list_handler() {
  cout << "" << endl;
  print_flow_table();
  cout << "" << endl;
  print_switch_stats();
}

void print_flow_table() {
  cout << "Switch information:" << endl;
  int i = 0;
  for (auto entry : flow_table) {
    cout << "[" << i << "] (srcIP= " <<
    entry.src_range.low << "-" << entry.src_range.high << ", "
    "destIP=" << entry.dest_range.low << "-" << entry.dest_range.high << ", " <<
    "action= " << action_type_tostring(entry.action_type) << ":" << entry.action_value << ", " <<
    "pri= " << entry.pri << ", " <<
    "pktCount= " << entry.pkt_count << ")" << endl;
    i++;
  }
}

void print_switch_stats() {
  cout << "Packet Stats:" << endl;
  string received_stats_str = "Received: ";
  string transmitted_stats_str = "Transmitted: ";

  int counter = 0;
  for (auto stat : switch_stats.received) {
    stringstream ss;
    if (stat.first == PKT_ADMIT) ss << "ADMIT:" << stat.second;
    else if (stat.first == PKT_ACK) ss << "ACK:" << stat.second;
    else if (stat.first == PKT_ADD) ss << "ADDRULE:" << stat.second;
    else if (stat.first == PKT_RELAY) ss << "RELAYIN:" << stat.second;

    if (counter != switch_stats.received.size() - 1) ss << ", ";

    received_stats_str += ss.str();
    counter += 1;
  }

  counter = 0;
  for (auto stat : switch_stats.transmitted) {
    stringstream ss;
    if (stat.first == PKT_OPEN) ss << "OPEN:" << stat.second;
    else if (stat.first == PKT_QUERY) ss << "QUERY:" << stat.second;
    else if (stat.first == PKT_RELAY) ss << "RELAYOUT:" << stat.second;

    if (counter != switch_stats.received.size() - 1) ss << ", ";

    transmitted_stats_str += ss.str();
    counter += 1;
  }

  cout << received_stats_str << endl;
  cout << transmitted_stats_str << endl;
}

void switch_exit_handler() {
  for (auto session : switch_sessions) {
    close(session.in_fifo_fd);
    close(session.out_fifo_fd);
  }
  exit(EXIT_SUCCESS);
}

PacketStats init_switch_stats() {
  PacketStats stats = {
    { {PKT_ADMIT, 0}, {PKT_ACK, 0}, {PKT_ADD, 0}, {PKT_RELAY, 0} },
    { {PKT_OPEN, 0}, {PKT_QUERY, 0}, {PKT_RELAY, 0} }
  };
  return stats;
}

vector<string> cache_traffic_file(string traffic_file) {
  vector<string> lines;
  ifstream file(traffic_file);
  string buffer;
  while (getline(file, buffer)) {
    lines.push_back(buffer);
  }
  return lines;
}

TrafficRule parse_traffic_rule(string line) {
  TrafficRule rule;
  rule.switch_num = -1;
  if (line.find("#") == string::npos && !line.empty()) {
    regex ws_re("\\s+");
    vector<string> placeholder {
      sregex_token_iterator(line.begin(), line.end(), ws_re, -1), {}
    };
    rule.switch_num = stoi(placeholder[0].substr(2));
    rule.src_ip = stoi(placeholder[1]);
    rule.dest_ip = stoi(placeholder[2]);
  }
  return rule;
}

vector<TrafficRule> parse_traffic_file(string traffic_file) {
  vector<TrafficRule> rules;
  ifstream file(traffic_file);
  string buffer;
  while (getline(file, buffer)) {
    if (buffer.find("#") == string::npos && !buffer.empty()) {
      regex ws_re("\\s+");
      vector<string> placeholder {
        sregex_token_iterator(buffer.begin(), buffer.end(), ws_re, -1), {}
      };
      rules.push_back({
        stoi(placeholder[0].substr(2)),
        stoi(placeholder[1]),
        stoi(placeholder[2])
      });
    }
  }
  return rules;
}
