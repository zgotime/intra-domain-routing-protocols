#include "RoutingProtocolImpl.h"
#include <arpa/inet.h>
#include "Node.h"

const char RoutingProtocolImpl::PING_ALARM = 8;
const char RoutingProtocolImpl::LS_ALARM = 9;
const char RoutingProtocolImpl::DV_ALARM = 10;
const char RoutingProtocolImpl::CHECK_ALARM = 11;

RoutingProtocolImpl::RoutingProtocolImpl(Node *n) : RoutingProtocol(n) {
  sys = n;
}

RoutingProtocolImpl::~RoutingProtocolImpl() {
  /* release memory for ports */
  hash_map<unsigned short, Port*>::iterator port_iter = ports.begin();

  while (port_iter != ports.end()) {
    Port* port = port_iter->second;
    ports.erase(port_iter++);
    free(port);
  }

  /* release memory for dv_table */
  hash_map<unsigned short, DV_Entry*>::iterator dv_iter = dv_table.begin();

  while (dv_iter != dv_table.end()) {
    DV_Entry* dv_entry = dv_iter->second;
    dv_table.erase(dv_iter++);
    free(dv_entry);
  }

  /* release memory for ls_table */
  /*hash_map<unsigned short, LS_Entry*>::iterator ls_iter = ls_table.begin();

  while (ls_iter != ls_table.end()) {
    LS_Entry* ls_entry = ls_iter->second;
    ls_table.erase(ls_iter++);
    free(ls_entry);
  }*/
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
  if (!check_packet_size(packet, size)) {
    cerr << "[ERROR] The router " << router_id << " received a packet with a wrong packet size at time "
         << sys->time() / 1000.0 << endl;
    free(packet);
    return;
  }

  char packet_type = *(char*)packet;
  cout << "size: " << sizeof(ePacketType) << endl;
  cout << "type: " << (unsigned short)packet_type << endl;
  switch (packet_type) {
  case DATA:
    handle_data_packet();
    break;
  case PING:
    handle_ping_packet(port, packet, size);
    break;
  case PONG:
    handle_pong_packet(port, packet);
    break;
  case LS:
    handle_ls_packet();
    break;
  case DV:
    handle_dv_packet();
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
    *(ePacketType*)packet = PING;
    *(unsigned short*)(packet + 2) = (unsigned short)htons(packet_size);
    *(unsigned short*)(packet + 4) = (unsigned short)htons(router_id);
    *(unsigned int*)(packet + 8) = (unsigned int)htonl(sys->time());

    sys->send(i, packet, packet_size);
  }

  sys->set_alarm(this, PING_DURATION, (void*)&PING_ALARM);
}

void RoutingProtocolImpl::handle_ls_alarm() {
  unsigned short packet_size = 12 + 4 * ports.size();

  /* flood */
  for (hash_map<unsigned short, Port*>::iterator iter_i = ports.begin(); iter_i != ports.end(); ++iter_i) {
    char* packet = (char*)malloc(packet_size);
    *(ePacketType*)packet = LS;
    *(unsigned short*)(packet + 2) = (unsigned short)htons(packet_size);
    *(unsigned short*)(packet + 4) = (unsigned short)htons(router_id);
    *(unsigned int*)(packet + 8) = (unsigned int)htonl(sequence_num);

    /* get neighbor ID and cost from ports */
    int count = 0;
    for (hash_map<unsigned short, Port*>::iterator iter_j = ports.begin(); iter_j != ports.end(); ++iter_j) {
      int offset = 12 + 4 * count;
      Port* port = iter_j->second;
      *(unsigned short*)(packet + offset) = (unsigned short)htons(port->neighbor_id);
      *(unsigned short*)(packet + offset + 4) = (unsigned short)htons(port->cost);
    }

    sys->send(iter_i->first, packet, packet_size);
  }

  sys->set_alarm(this, LS_DURATION, (void*)&LS_ALARM);
}

void RoutingProtocolImpl::handle_dv_alarm() {
  unsigned short packet_size = 8 + 4 * dv_table.size();

  /* send dv to neighbors */
  for (hash_map<unsigned short, Port*>::iterator iter = ports.begin(); iter != ports.end(); ++iter) {
    char* packet = (char*)malloc(packet_size);
    *(ePacketType*)packet = DV;
    *(unsigned short*)(packet + 2) = (unsigned short)htons(packet_size);
    *(unsigned short*)(packet + 4) = (unsigned short)htons(router_id);
    *(unsigned short*)(packet + 6) = (unsigned short)htons(iter->second->neighbor_id);

    /* get router ID and cost from dv_table */
    int count = 0;
    for (hash_map<unsigned short, DV_Entry*>::iterator dv_iter = dv_table.begin(); dv_iter != dv_table.end(); ++dv_iter) {
      int offset = 8 + 4 * count;
      DV_Entry* entry = dv_iter->second;

      /* poison reverse */
      int cost = (iter->second->neighbor_id == entry->next_hop) ? INFINITY_COST : entry->cost;
      *(unsigned short*)(packet + offset) = (unsigned short)htons(dv_iter->first);
      *(unsigned short*)(packet + offset + 4) = (unsigned short)htons(cost);
    }

    sys->send(iter->first, packet, packet_size);
  }

  sys->set_alarm(this, DV_DURATION, (void*)&DV_ALARM);
}

void RoutingProtocolImpl::handle_check_alarm() {
  update_port_stat();

  switch (protocol_type) {
  case P_LS:
    update_ls_stat();
    break;
  case P_DV:
    update_ls_stat();
    break;
  default:
    break;
  }

  sys->set_alarm(this, CHECK_DURATION, (void*)&CHECK_ALARM);
}

void RoutingProtocolImpl::update_port_stat() {
  hash_map<unsigned short, Port*>::iterator iter = ports.begin();

  while (iter != ports.end()) {
    Port* port = iter->second;
    if (sys->time() > port->time_to_expire) {
      ports.erase(iter++);
      free(port);
    } else {
      ++iter;
    }
  }
}

void RoutingProtocolImpl::update_ls_stat() {

}

void RoutingProtocolImpl::update_dv_stat() {

}


void RoutingProtocolImpl::handle_data_packet() {

}

void RoutingProtocolImpl::handle_ping_packet(unsigned short port_id, void* packet, unsigned short size) {
  unsigned short dest_id = (unsigned short)ntohs(*(unsigned short*)((char*)packet + 4));

  *(char*)packet = (char)PONG;
  *(unsigned short*)((char*)packet + 4) = (unsigned short)htons(router_id);
  *(unsigned short*)((char*)packet + 6) = (unsigned short)htons(dest_id);

  sys->send(port_id, packet, size);
}

void RoutingProtocolImpl::handle_pong_packet(unsigned short port_id, void* packet) {
  if (!check_dest_id(packet)) {
    cerr << "[ERROR] The router " << router_id << " received a PONG packet with a wrong destination ID at time "
         << sys->time() / 1000.0 << endl;
    free(packet);
    return;
  }

  unsigned int timestamp = (unsigned int)ntohl(*(unsigned int*)((char*)packet + 8));
  unsigned short neighbor_id = (unsigned short)ntohl(*(unsigned short*)((char*)packet + 4));

  Port* port = (Port*)malloc(sizeof(Port));
  /* impossible to overflow because PONG_TIMEOUT is only 15 seconds */
  port->cost = (short)(sys->time() - timestamp);
  port->time_to_expire = sys->time() + PONG_TIMEOUT;
  port->neighbor_id = neighbor_id;
  ports[port_id] = port;

  free(packet);
}

void RoutingProtocolImpl::handle_ls_packet() {

}
void RoutingProtocolImpl::handle_dv_packet() {

}

bool RoutingProtocolImpl::check_packet_size(void* packet, unsigned short size) {
  unsigned short packet_size = (unsigned short)ntohs(*(unsigned short*)((char*)packet + 2));
  return (size == packet_size) ? true : false;
}

bool RoutingProtocolImpl::check_dest_id(void* packet) {
  unsigned short dest_id = (unsigned short)ntohs(*(unsigned short*)((char*)packet + 6));
  return (router_id == dest_id) ? true : false;
}
