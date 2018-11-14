#ifndef CONTROLLER_H
#define CONTROLLER_H

#include "a3sdn.h"
#include <vector>

void controller_handler(int, int);
void controller_exit_handler();
void controller_list_handler();
void send_open_to_controller();
void print_switch_info();
void print_controller_stats();
PacketStats init_controller_stats();
void controller_incoming_message_handler(std::vector<std::string>);
Switch find_switch_by_ip(int);
void controller_debug_handler();
void *controller_keep_alive(void *arg);

#endif
