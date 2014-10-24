#include "RoutingProtocolImpl.h"
#include <arpa/inet.h>
#include "Node.h"
#include "DVTable.h"

const char RoutingProtocolImpl::PING_ALARM = 8;
const char RoutingProtocolImpl::LS_ALARM = 9;
const char RoutingProtocolImpl::DV_ALARM = 10;
const char RoutingProtocolImpl::CHECK_ALARM = 11;

RoutingProtocolImpl::RoutingProtocolImpl(Node *n) : RoutingProtocol(n) {
  sys = n;
}

RoutingProtocolImpl::~RoutingProtocolImpl() {
  /* release memory for ports */
  hash_map<unsigned short, Port*>::iterator iter = ports.begin();

  while (iter != ports.end()) {
    Port* port = iter->second;
    ports.erase(iter++);
    free(port);
  }
}

void RoutingProtocolImpl::init(unsigned short num_ports, unsigned short router_id, eProtocolType protocol_type) {
  this->num_ports = num_ports;
  this->router_id = router_id;
  this->protocol_type = protocol_type;

  /* first ping message sent at 0 second */
  sys->set_alarm(this, 0, (void*)&PING_ALARM);
  sys->set_alarm(this, CHECK_DURATION, (void*)&CHECK_ALARM);

  if (protocol_type == P_LS) {
    sequence_num = 0;
    sys->set_alarm(this, LS_DURATION, (void*)&LS_ALARM);
  } else {
    sys->set_alarm(this, DV_DURATION, (void*)&DV_ALARM);
  }
}

void RoutingProtocolImpl::handle_alarm(void *data) {
  char alarm_type = *(char*)data;
  switch (alarm_type) {
  case PING_ALARM:
    handle_ping_alarm();
    break;
  case LS_ALARM:
    handle_ls_alarm();
    break;
  case DV_ALARM:
    handle_dv_alarm();
    break;
  case CHECK_ALARM:
    handle_check_alarm();
    break;
  default:
    break;
  }
}

void RoutingProtocolImpl::recv(unsigned short port, void *packet, unsigned short size) {
  if (!check_packet_size((char*)packet, size)) {
    cerr << "[ERROR] The router " << router_id << " received a packet with a wrong packet size at time "
         << sys->time() / 1000.0 << endl;
    free(packet);
    return;
  }

  char packet_type = *(char*)packet;
  switch (packet_type) {
  case DATA:
    recv_data_packet((char*)packet, size);
    break;
  case PING:
    recv_ping_packet(port, (char*)packet, size);
    break;
  case PONG:
    recv_pong_packet(port, (char*)packet);
    break;
  case LS:
    recv_ls_packet();
    break;
  case DV:
    recv_dv_packet((char*)packet, size);
    break;
  default:
    break;
  }
}

void RoutingProtocolImpl::handle_ping_alarm() {
  unsigned short packet_size = 12;

  /* send ping packet to all ports */
  for (int i = 0; i < num_ports; ++i) {
    char* packet = (char*)malloc(packet_size);
    *packet = (char)PING;
    *(unsigned short*)(packet + 2) = (unsigned short)htons(packet_size);
    *(unsigned short*)(packet + 4) = (unsigned short)htons(router_id);
    *(unsigned int*)(packet + 8) = (unsigned int)htonl(sys->time());

    sys->send(i, packet, packet_size);
  }

  sys->set_alarm(this, PING_DURATION, (void*)&PING_ALARM);
}

void RoutingProtocolImpl::handle_ls_alarm() {
  send_ls_packet();
  sys->set_alarm(this, LS_DURATION, (void*)&LS_ALARM);
}

void RoutingProtocolImpl::handle_dv_alarm() {
  send_dv_packet();
  sys->set_alarm(this, DV_DURATION, (void*)&DV_ALARM);
}

void RoutingProtocolImpl::handle_check_alarm() {
  bool port_update = check_port_state();

  if (protocol_type == P_LS) {
    bool ls_upate = false; // to do!

    if (port_update || ls_upate) {

    }
  } else {
    bool dv_update = dv_table.check_dv_state(sys->time(), routing_table);

    if (port_update || dv_update) {
      send_dv_packet();
    }
  }

  sys->set_alarm(this, CHECK_DURATION, (void*)&CHECK_ALARM);
}

bool RoutingProtocolImpl::check_port_state() {
  bool update = false;
  hash_map<unsigned short, Port*>::iterator iter = ports.begin();
  vector<unsigned short> deleted_dst_ids;

  while (iter != ports.end()) {
    Port* port = iter->second;
    if (sys->time() > port->time_to_expire) {
      update = true;
      deleted_dst_ids.push_back(iter->first);
      ports.erase(iter++);
      free(port);
    } else {
      ++iter;
    }
  }

  if (update) {
    if (protocol_type == P_LS) {

    } else {
      dv_table.delete_dv(deleted_dst_ids, routing_table);
    }
  }

  return update;
}

void RoutingProtocolImpl::recv_data_packet(char* packet, unsigned short size) {
  unsigned short dst_id = (unsigned short)ntohs(*(unsigned short*)(packet + 6));

  if (dst_id == router_id) {
    free(packet);
    return;
  }

  hash_map<unsigned short, unsigned short>::iterator iter = routing_table.find(dst_id);

  if (iter != routing_table.end()) {
    unsigned short next_hop = iter->second;
    hash_map<unsigned short, Port*>::iterator port_iter = ports.find(next_hop);

    if (port_iter != ports.end()) {
      Port* port = port_iter->second;
      sys->send(port->port_id, packet, size);
    } else {
      free(packet);
    }
  }
}

void RoutingProtocolImpl::recv_ping_packet(unsigned short port_id, char* packet, unsigned short size) {
  unsigned short dest_id = (unsigned short)ntohs(*(unsigned short*)((char*)packet + 4));

  *packet = (char)PONG;
  *(unsigned short*)(packet + 4) = (unsigned short)htons(router_id);
  *(unsigned short*)(packet + 6) = (unsigned short)htons(dest_id);

  sys->send(port_id, packet, size);
}

void RoutingProtocolImpl::recv_pong_packet(unsigned short port_id, char* packet) {
  if (!check_dst_id(packet)) {
    cerr << "[ERROR] The router " << router_id << " received a PONG packet with a wrong destination ID at time "
         << sys->time() / 1000.0 << endl;
    free(packet);
    return;
  }

  unsigned int timestamp = (unsigned int)ntohl(*(unsigned int*)(packet + 8));
  unsigned short src_id = (unsigned short)ntohs(*(unsigned short*)(packet + 4));
  unsigned short cost = (short)(sys->time() - timestamp);
  unsigned int time_to_expire = sys->time() + PONG_TIMEOUT;
  free(packet);

  hash_map<unsigned short, Port*>::iterator iter = ports.find(port_id);

  if (iter != ports.end()) {
    Port* port = iter->second;
    port->time_to_expire = time_to_expire;
  } else {
    Port* port = (Port*)malloc(sizeof(Port));
    port->time_to_expire = time_to_expire;
    port->port_id = port_id;
    ports[src_id] = port;
  }

  if (protocol_type == P_LS) {

  } else {
    if (dv_table.update_by_pong(src_id, cost, sys->time(), routing_table)) {
      send_dv_packet();
    }
  }
}

void RoutingProtocolImpl::recv_ls_packet() {

}

void RoutingProtocolImpl::recv_dv_packet(char* packet, unsigned short size) {
  if (!check_dst_id(packet)) {
    cerr << "[ERROR] The router " << router_id << " received a DV packet with a wrong destination ID at time "
         << sys->time() / 1000.0 << endl;
    free(packet);
    return;
  }

  if (dv_table.update_by_dv(packet, size, router_id, sys->time(), routing_table)) {
    send_dv_packet();
  }

  free(packet);
}

bool RoutingProtocolImpl::check_packet_size(char* packet, unsigned short size) {
  unsigned short packet_size = (unsigned short)ntohs(*(unsigned short*)(packet + 2));
  return (size == packet_size) ? true : false;
}

bool RoutingProtocolImpl::check_dst_id(char* packet) {
  unsigned short dst_id = (unsigned short)ntohs(*(unsigned short*)(packet + 6));
  return (router_id == dst_id) ? true : false;
}


void RoutingProtocolImpl::send_ls_packet() {

}

void RoutingProtocolImpl::send_dv_packet() {
  unsigned short packet_size = 8 + (dv_table.dv_length() << 2);

  for (hash_map<unsigned short, Port*>::iterator iter = ports.begin(); iter != ports.end(); ++iter) {
    char* packet = (char*)malloc(packet_size);
    Port* port = iter->second;
    dv_table.set_dv_packet(packet, router_id, iter->first, routing_table);

    sys->send(port->port_id, packet, packet_size);
  }
}
