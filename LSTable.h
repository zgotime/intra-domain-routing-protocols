#ifndef LSTABLE_H
#define LSTABLE_H

#include "global.h"

struct LS_Entry {
  unsigned int time_to_expire;
  unsigned short neighbor_id;
  unsigned short cost;
};

struct LS_Info{
  unsigned short destinatin_id;
  unsigned short cost;
  unsigned short next_hop_id;
};


class LSTable{

public:
  LSTable();
  ~LSTable();
  void init(unsigned short router_id);
  void insert(unsigned short router_id, vector<LS_Entry*>* vec);

  hash_map<unsigned short, vector<LS_Entry*>*>::iterator find(unsigned short router_id);
  void delete_ls(unsigned int current_time);
  void update_ls_package(unsigned short port_id, char* packet, unsigned short size);
  bool check_lsp_sequence_num(char* packet);

  void compute_ls_routing_table(hash_map<unsigned short, unsigned short>& routing_table);

  void set_ls_package(char* packet, unsigned short packet_size);

  void increase_seq();
  bool update_by_pong(unsigned short src_id, unsigned short cost, unsigned short current_time);

private:

  static const unsigned int LS_TIMEOUT = 45000;

  hash_map<unsigned short, vector<LS_Entry*>*> table;
  vector<LS_Entry*> linkst;
  hash_map<unsigned short, unsigned int> ls_sequence_num;
  unsigned int sequence_num;
  unsigned short router_id;

};

#endif
