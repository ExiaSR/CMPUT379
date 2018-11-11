#ifndef CONTROLLER_H
#define CONTROLLER_H

#include "a2sdn.h"
#include <vector>

void controller_handler(int);
void controller_exit_handler(std::vector<Session>);
void controller_list_handler();
void send_open_to_controller();
void print_switch_info();
void print_controller_stats();
PacketStats init_controller_stats();
void controller_incoming_message_handler(std::vector<std::string>);
Switch find_switch_by_ip(int);

#endif
