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
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>
#include <chrono>

using namespace std;

int switch_num;
// int left_switch;
// int right_switch;
vector<Session> switch_sessions;
vector<FlowTableEntry> flow_table;
TCPSession tcp_session;

PacketStats switch_stats;
Mode switch_mode = MODE_DISCONNECTED;

string server_address;
int server_port;
struct hostent *host_entity;
sockaddr_in client_sockaddr_in;
FILE *sfp;
pthread_t tid;
pthread_t sleep_tid;
mutex delay_mutex;

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
void switch_handler(int num, int left_switch, int right_switch, string traffic_file, IPRange ip_range, string addr, int port) {
  switch_num = num;
  server_address = addr;
  server_port = port;
  switch_sessions = init_session(switch_num, left_switch, right_switch);
  pollfd pfd[switch_sessions.size() + 2];
  flow_table = init_flow_table(ip_range);
  switch_stats = init_switch_stats();
  vector<string> traffic_file_cache = cache_traffic_file(traffic_file);

  // non-blocking I/O polling from STDIN
  pfd[0] = { STDIN_FILENO, POLLIN, 0 };

  for (int i = 0; i < switch_sessions.size(); i++) {
    pfd[i+2].fd = switch_sessions[i].in_fifo_fd;
    pfd[i+2].events = POLLIN;
  }

  host_entity = gethostbyname(server_address.c_str());
  memset((char*) &client_sockaddr_in, 0, sizeof(client_sockaddr_in));
  memcpy((char*) &client_sockaddr_in.sin_addr.s_addr, host_entity->h_addr, host_entity->h_length);
  client_sockaddr_in.sin_family = host_entity->h_addrtype;
  client_sockaddr_in.sin_port = htons(server_port);

  char tmp_server_addr[128];
  strcpy(tmp_server_addr, server_address.c_str());
  if (inet_ntop(AF_INET, &client_sockaddr_in.sin_addr.s_addr, tmp_server_addr, sizeof(tmp_server_addr)) == NULL) {
    exit(EXIT_FAILURE);
  }

  send_open_to_controller(switch_num, left_switch, right_switch);

  // create new thread to handle heartbeat mechanism
  if (pthread_create(&tid, NULL, switch_keep_alive, (void *) &tcp_session.socket_fd) != 0) {
    perror("Failed to create thread");
    exit(EXIT_FAILURE);
  }

  pfd[1] = { tcp_session.socket_fd, POLLIN, 0 };

  int rule_cnt = 0;
  while (true) {
    // IO multiplexing
    int ret = poll(pfd, switch_sessions.size() + 2, POLL_TIMEOUT);

    if (ret < 0) {
      cout << "Failed to poll" << endl;
      exit(EXIT_FAILURE);
    }

    // read traffic file
    if (rule_cnt < traffic_file_cache.size() && switch_mode == MODE_CONNECTED && tcp_session.status == TCP_CONNECTED) {
      TrafficRule rule = parse_traffic_rule(traffic_file_cache[rule_cnt]);
      // cout << traffic_file_cache[rule_cnt] << endl;
      if (rule.switch_num == switch_num && rule.type == TRAFFIC_PKT && ip_range.low <= rule.dest_ip && rule.dest_ip <= ip_range.high) {
        // found match in flow table, admit packet
        flow_table[0].pkt_count += 1;
        switch_stats.received[PKT_ADMIT] += 1;
      } else if (rule.switch_num == switch_num && rule.type == TRAFFIC_DELAY) {
        // Stop sending/process traffic file for a while
        delay_args *args = (struct delay_args*) malloc(sizeof(struct delay_args));
        args->delay = rule.delay;
        tcp_session.status = TCP_DISCONNECTED;
        // create new process to simulate the delay
        if (pthread_create(&sleep_tid, NULL, switch_delay_handler, (void *) args) != 0) {
          perror("Failed to create new thread");
          exit(EXIT_FAILURE);
        }
        pthread_detach(sleep_tid);
      } else if (rule.switch_num == switch_num && rule.type == TRAFFIC_PKT) {
        switch_stats.received[PKT_ADMIT] += 1;
        int rule_index = find_matching_rule(rule.dest_ip);
        if (rule_index > 0) {
          // find matched rule send relay message
          int session_idx = find_matching_session(flow_table[rule_index].action_value);
          stringstream ss;
          ss << "RELAY " << switch_num << " " << rule.src_ip << " " << rule.dest_ip;
          flow_table[rule_index].pkt_count += 1;
          switch_stats.transmitted[PKT_RELAY] += 1;
          send_message(switch_sessions[session_idx].out_fifo_fd, PKT_RELAY, ss.str());
          cout << "Transmitted (src= sw" << switch_num << ", dest= sw" << switch_sessions[session_idx].switch_num << ") [RELAY]" << endl;
          cout << "    header= (srcIP= " << rule.src_ip << ", destIP= " << rule.dest_ip << ")" << endl;
        } else {
          // send QUERY to controller
          stringstream ss;
          ss << "QUERY " << switch_num << " " << rule.src_ip << " " << rule.dest_ip;
          switch_stats.transmitted[PKT_QUERY] += 1;
          switch_mode = MODE_WAITINGADD;
          send_message_socket(tcp_session.socket_fd, PKT_OPEN, ss.str());
          cout << "Transmitted: (src= sw" << switch_num << ", dest=cont [QUERY]" << endl;
          cout << "    header= (srcIP= " << rule.src_ip << ", destIP= " << rule.dest_ip << ")" << endl;
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
          case CMD_DEBUG:
            for (auto p : pfd) {
              cout << "fd: " << p.fd << endl;
            }
            switch_debug_handler();
            break;
          case CMD_NOTFOUND:
            cout << "Invalid command" << endl;
            break;
        }
      }
    }

    // Socket
    if (pfd[1].revents & POLLIN && tcp_session.status == TCP_CONNECTED) {
      char buffer[MAXBUF];
      read(tcp_session.socket_fd, buffer, MAXBUF);
      if (DEBUG) cout << "\nReceived (socket): " << string(buffer) << "\n" << endl;
      switch_incoming_message_handler(parse_message(string(buffer)));
    }

    // FIFO
    for (int i = 0; i < switch_sessions.size(); i++) {
      if (pfd[i+2].revents & POLLIN) {
        char buffer[MAXBUF];
        read(switch_sessions[i].in_fifo_fd, buffer, MAXBUF);
        switch_incoming_message_handler(parse_message(string(buffer)));
      }
    }
  }
}

void *switch_delay_handler(void *arg) {
  int delay = ((struct delay_args*)arg)->delay;
  cout << "Entering a delay period for " << delay << " millisec\n" << endl;
  usleep(delay * 1000);
  delay_mutex.lock();
  tcp_session.status = TCP_CONNECTED;
  delay_mutex.unlock();
  pthread_exit(NULL);
}

void *switch_keep_alive(void *arg) {
  while (true) {
    if (tcp_session.status == TCP_CONNECTED) {
      stringstream ss;
      ss << "HEARTBEAT " << switch_num;
      send_message(tcp_session.socket_fd, PKT_HEARTBEAT, ss.str());
    }
    // else {
    //   cout << "Detached mode" << endl;
    // }
    sleep(HEARTBEAT_INTERVAL);
  }
}

void switch_incoming_message_handler(vector<string> message) {
  if (message.size() > 1) {
    PktType type = name_pkt_map[message[0]];
    switch_stats.received[type] += 1;

    switch (type) {
      case PKT_ADD:
        cout << "Received: (src= cont, dest= sw" << switch_num << ") [ADD]" << endl;
        if (message.size() == 5) {
          // not found
          cout << "    (srcIP= 0-1000, destIP= " << stoi(message[4]) << ", action= DROP:0, pri=4, pktCount= 0)" << endl;
          add_drop_rule(stoi(message[2]));
        } else if (message.size() == 8) {
          int forwarding_port = stoi(message[2]) == switch_num ? 2 : 1;
          cout << "    srcIP= 0-1000, destIP= " << stoi(message[4]) << "-" << stoi(message[5]) << ", action= FORWARD:" << forwarding_port << ", pri=4, pktCount= 0)" << endl;
          // update flow table and relay packet
          int flow_table_idx = add_forward_rule(forwarding_port, { stoi(message[4]), stoi(message[5]) });
          int session_idx = find_matching_session(forwarding_port);

          stringstream ss;
          ss << "RELAY " << switch_num << " " << stoi(message[6]) << " " << stoi(message[7]);
          if (session_idx > -1) {
            send_message(switch_sessions[session_idx].out_fifo_fd, PKT_RELAY, ss.str());
            cout << "Transmitted (src= sw" << switch_num << ", dest= sw" << switch_sessions[session_idx].switch_num << ") [RELAY]" << endl;
            cout << "    header= (srcIP= " << stoi(message[6]) << ", destIP= " << stoi(message[7]) << ")" << endl;
          }
          switch_stats.transmitted[PKT_RELAY] += 1;
          flow_table[flow_table_idx].pkt_count += 1;
        }
        switch_mode = MODE_CONNECTED;
        break;
      case PKT_RELAY:
        cout << "Received: (src= sw" << message[1] << ", dest= sw" << switch_num << ") [RELAY]" << endl;
        cout << "    header= (srcIP= " << message[2] << " destIP= " << message[3] << ")" << endl;
        flow_table[0].pkt_count += 1;
        break;
      case PKT_ACK:
        cout << "Received: (src= cont, dest= sw" << switch_num << ") [ACK]" << endl;
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
  int socket_fd = socket(host_entity->h_addrtype, SOCK_STREAM, 0);
  if (socket_fd < 0) {
    cout << "Failed to open socket" << endl;
    exit(EXIT_FAILURE);
  }

  tcp_session.socket_fd = socket_fd;

  int on = 1;
  if (setsockopt(tcp_session.socket_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0) {
    perror("client setsockopt() failed");
    close(tcp_session.socket_fd);
    exit(EXIT_FAILURE);
  }

  if (connect(socket_fd, (struct sockaddr*) &client_sockaddr_in, sizeof(client_sockaddr_in)) < 0) {
    cout << "Failed to connect to the socket" << endl;
    exit(EXIT_FAILURE);
  }
  tcp_session.status = TCP_CONNECTED;
  // tcp_session.file_read_fd = fdopen(socket_fd, "r");
  // tcp_session.file_write_fd = fdopen(socket_fd, "w");
  // if (tcp_session.file_read_fd < 0 || tcp_session.file_write_fd < 0) {
  //   cout << "Failed to establish connection" << endl;
  //   exit(EXIT_FAILURE);
  // }

  stringstream ss;
  ss << "OPEN" << " " << num << " " << left_switch << " " << right_switch << " " <<
  flow_table[0].dest_range.low << " " << flow_table[0].dest_range.high;
  send_message_socket(socket_fd, PKT_OPEN, ss.str());
  cout << "Transmitted (src= sw" << switch_num << " ,dest= cont) [OPEN]" << endl;
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
  vector<Session> sessions;

  if (left > 0) {
    int in_fifo_fd = open(get_fifo_name(left, switch_num).c_str(), O_RDWR | O_NONBLOCK);
    int out_fifo_fd = open(get_fifo_name(switch_num, left).c_str(), O_RDWR | O_NONBLOCK);
    if (in_fifo_fd < 0 || out_fifo_fd < 0) {
      cout << "Failed to open fifo" << endl;
      exit(EXIT_FAILURE);
    }
    sessions.push_back({
      get_fifo_name(left, switch_num), // in
      get_fifo_name(switch_num, left), // out
      in_fifo_fd,
      out_fifo_fd,
      1,
      left
    });
  }

  if (right > 0) {
    int in_fifo_fd = open(get_fifo_name(right, switch_num).c_str(), O_RDWR | O_NONBLOCK);
    int out_fifo_fd = open(get_fifo_name(switch_num, right).c_str(), O_RDWR | O_NONBLOCK);
    if (in_fifo_fd < 0 || out_fifo_fd < 0) {
      cout << "Failed to open fifo" << endl;
      exit(EXIT_FAILURE);
    }
    sessions.push_back({
      get_fifo_name(right, switch_num), // in
      get_fifo_name(switch_num, right), // out
      in_fifo_fd,
      out_fifo_fd,
      2,
      right
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
  cout << "Flow table:" << endl;
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
  stringstream ss;
  ss << "EXIT " << switch_num;
  send_message(tcp_session.socket_fd, PKT_EXIT, ss.str());
  shutdown(tcp_session.socket_fd, SHUT_RDWR);
  close(tcp_session.socket_fd);
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
    if (placeholder[1] == "delay") {
      rule.switch_num = stoi(placeholder[0].substr(2));
      rule.delay = stoi(placeholder[2]);
      rule.type = TRAFFIC_DELAY;
    } else {
      rule.switch_num = stoi(placeholder[0].substr(2));
      rule.src_ip = stoi(placeholder[1]);
      rule.dest_ip = stoi(placeholder[2]);
      rule.type = TRAFFIC_PKT;
    }
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

void switch_debug_handler() {
  for (auto s : switch_sessions) {
    cout << "Switch out fifo: " << s.out_fifo_name << " Switch in fifo: " << s.in_fifo_name << " Switch num: " << s.switch_num << " Connected port: " << s.port << endl;
  }
}
