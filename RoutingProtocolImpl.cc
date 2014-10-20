#include "RoutingProtocolImpl.h"
#include <arpa/inet.h>
#include "Node.h"

const char RoutingProtocolImpl::PING_ALARM = 8;
const char RoutingProtocolImpl::CHECK_ALARM = 9;

RoutingProtocolImpl::RoutingProtocolImpl(Node *n) : RoutingProtocol(n) {
  sys = n;
}

RoutingProtocolImpl::~RoutingProtocolImpl() {}

void RoutingProtocolImpl::init(unsigned short num_ports, unsigned short router_id, eProtocolType protocol_type) {
  this->num_ports = num_ports;
  this->router_id = router_id;
  this->protocol_type = protocol_type;

  /* fist ping message sent at 0 second */
  sys->set_alarm(this, 0, (void*)&PING_ALARM);
  sys->set_alarm(this, CHECK_DURATION, (void*)&CHECK_ALARM);
}

void RoutingProtocolImpl::handle_alarm(void *data) {
  cout << "port table: " << ports.size() << endl;
  char alarm_type = *(char*)data;
  switch (alarm_type) {
  case PING_ALARM:
    handle_ping_alarm();
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
  switch (packet_type) {
  case DATA_PACKET:
    handle_data_packet();
    break;
  case PING_PACKET:
    handle_ping_packet(port, packet, size);
    break;
  case PONG_PACKET:
    handle_pong_packet(port, packet);
    break;
  case LS_PACKET:
    handle_ls_packet();
    break;
  case DV_PACKET:
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
    *packet = PING_PACKET;
    *(unsigned short*)(packet + 2) = (unsigned short)htons(packet_size);
    *(unsigned short*)(packet + 4) = (unsigned short)htons(router_id);
    *(unsigned int*)(packet + 8) = (unsigned int)htonl(sys->time());

    sys->send(i, packet, packet_size);
  }

  sys->set_alarm(this, PING_DURATION, (void*)&PING_ALARM);
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
    cout << "ports size: " << ports.size() << endl;
    if (sys->time() > port->time_to_expire) {
      ports.erase(iter++);
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

  *(char*)packet = PONG_PACKET;
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

  Port port;
  port.cost = sys->time() - timestamp;
  port.time_to_expire = sys->time() + PONG_TIMEOUT;
  port.neighbor_id = neighbor_id;
  ports[port_id] = &port;

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
