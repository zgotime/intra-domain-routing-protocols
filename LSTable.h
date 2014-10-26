#ifndef LSTABLE_H
#define LSTABLE_H

#include "global.h"

struct LS_Entry {
  unsigned int time_to_expire;
  unsigned short neighbor_id;
  unsigned short cost;
};

struct LS_Info{
  unsigned short cost;
  unsigned short next_hop;
};


class LSTable{

public:
  LSTable() {}
  ~LSTable();
  void init(unsigned short router_id);
  void delete_ls(vector<unsigned short>& deleted_dst_ids);
  bool check_ls_state(unsigned int current_time);
  bool check_lsp_sequence_num(char* packet);
  void update_by_ls(char* packet, unsigned int current_time, unsigned short size);
  void dijkstra(hash_map<unsigned short, unsigned short>& routing_table);
  void set_ls_packet(char* packet, unsigned short packet_size);
  bool update_by_pong(unsigned short src_id, unsigned short cost, unsigned short current_time);
  void increase_seq() { ++sequence_num; }
  void delete_neighbor(unsigned short neighbor_id);

private:

  static const unsigned int LS_TIMEOUT = 45000;

  hash_map<unsigned short, vector<LS_Entry*>*> table;
  hash_map<unsigned short, unsigned int> ls_sequence_num;

  vector<LS_Entry*> linkst;
  unsigned int sequence_num;
  unsigned short router_id;

  LS_Entry* check_linkst_constains(unsigned short src_id);
};

#endif
