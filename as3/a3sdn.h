/*
 * Copyright (C) 2018 Michael (Zichun) Lin
 * All rights reserved
*/

#ifndef A3SDN_H
#define A3SDN_H

#define MAX_NSW 7
#define MAXIP 1000
#define MAXPRI 0
#define MINPRI 4
#define MAXBUF 128
#define POLL_TIMEOUT 0
#define DEBUG false
#define HEARTBEAT_INTERVAL 1
#define HEARTBEAT_THRESHOLD 3 // how many conseticue times that there is no heartbeat that the server will consider switch as detached.

#include <string>
#include <map>
#include <vector>
#include <pthread.h>
#include <thread>
#include <mutex>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>

typedef enum {
  CMD_LIST,
  CMD_EXIT,
  CMD_NOTFOUND,
  CMD_DEBUG
} CMDType;

typedef enum {
  ACTION_DELIVER,
  ACTION_FORWARD,
  ACTTION_DROP
} ActionType;

typedef enum {
  TCP_CONNECTED,
  TCP_DISCONNECTED
} ConnectionStatus;

typedef enum {
  PKT_OPEN,
  PKT_ACK,
  PKT_QUERY,
  PKT_ADD,
  PKT_RELAY,
  PKT_ADMIT,
  PKT_EXIT,
  PKT_HEARTBEAT
} PktType;

struct IPRange {
  int low;
  int high;
};

struct TCPSession {
  int socket_fd;
  ConnectionStatus status;
  sockaddr_in socket_addr;
  socklen_t socket_addr_len;
  FILE *file_read_fd;
  FILE *file_write_fd;
  int heartbeat_cnt;
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
void send_message_socket(int, PktType, std::string);
std::string action_type_tostring(ActionType);

#endif
