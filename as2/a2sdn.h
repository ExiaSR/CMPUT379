/*
 * Copyright (C) 2018 Every-fucking-one, except the Author
 *
 * This source code is licensed under the GLWTS Public License found in the
 * LICENSE file in the root directory of this source tree.
 */

#ifndef A2SDN_H
#define A2SDN_H

#define MAX_NSW 7
#define MAXIP 1000
#define MAXPRI 0
#define MINPRI 4
#define MAXBUF 1024

#include <string>
#include <map>
#include <vector>

typedef enum {
  CMD_LIST,
  CMD_EXIT,
  CMD_NOTFOUND
} CMDType;

typedef enum {
  ACTION_DELIVER,
  ACTION_FORWARD,
  ACTTION_DROP
} ActionType;

typedef enum {
  PKT_OPEN,
  PKT_ACK,
  PKT_QUERY,
  PKT_ADD,
  PKT_RELAY,
  PKT_ADMIT
} PktType;

struct IPRange {
  int low;
  int high;
};

struct Session {
  std::string in_fifo_name;
  std::string out_fifo_name;
  int in_fifo_fd;
  int out_fifo_fd;
  int port;
};

struct Packet {
  PktType type;
  std::string meesage;
};

struct PacketStats {
  std::map<PktType, int> received;
  std::map<PktType, int> transmitted;
};

struct Switch {
  int switch_num;
  int port1;
  int port2;
  IPRange port3;
};

std::vector<std::string> parse_message(std::string);
CMDType get_cmd_type(std::string);
IPRange split_ip_range_string(std::string);
std::string get_fifo_name(int, int);
void send_message(int, PktType, std::string);
std::string action_type_tostring(ActionType);


#endif
