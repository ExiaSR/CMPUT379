/*
 * Copyright (C) 2018 Every-fucking-one, except the Author
 *
 * This source code is licensed under the GLWTS Public License found in the
 * LICENSE file in the root directory of this source tree.
 */

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
