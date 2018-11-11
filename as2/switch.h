#ifndef SWITCH_H
#define SWITCH_H

#include "a2sdn.h"
#include <string>
#include <vector>

struct TrafficRule {
  int switch_num;
  int src_ip;
  int dest_ip;
};

struct FlowTableEntry {
  IPRange src_range;
  IPRange dest_range;
  ActionType action_type;
  int action_value; // the switch port to which the packet should be forwarded
  int pri; // priority value
  int pkt_count; // packet count
};

typedef enum {
  MODE_CONNECTED,
  MODE_DISCONNECTED,
  MODE_WAITINGADD
} Mode;

extern int switch_num;

void switch_handler(int, int, int, std::string, IPRange);
std::vector<FlowTableEntry> init_flow_table(IPRange);
std::vector<Session> init_session(int, int, int);
void switch_exit_handler();
void switch_list_handler();
void print_flow_table();
void send_open_to_controller(int, int, int);
PacketStats init_switch_stats();
void print_switch_stats();
std::vector<TrafficRule> parse_traffic_file(std::string);
void switch_incoming_message_handler(std::vector<std::string>);
std::vector<std::string> cache_traffic_file(std::string);
TrafficRule parse_traffic_rule(std::string);
void add_drop_rule(int);
int add_forward_rule(int, IPRange);
int find_matching_rule(int);
int find_matching_session(int);

#endif
