#include "LSTable.h"
#include <arpa/inet.h>
#include <algorithm>

LSTable::~LSTable(){
  /* release memory for linkst */
  vector<LS_Entry*>::iterator ls_iter = linkst.begin();

  while (ls_iter != linkst.end()) {
    LS_Entry* entry = *ls_iter;
    ls_iter = linkst.erase(ls_iter);
    free(entry);
  }

  /* release memory for table */
  hash_map<unsigned short, vector<LS_Entry*>*>::iterator table_iter = table.begin();

  while (table_iter != table.end()) {
    vector<LS_Entry*>* vec = table_iter->second;
    table.erase(table_iter++);

    vector<LS_Entry*>::iterator vec_iter = vec->begin();
    while (vec_iter != vec->end()) {
      LS_Entry* entry = *vec_iter;
      vec_iter = vec->erase(vec_iter);
      free(entry);
    }

    delete vec;
  }
}

void LSTable::init(unsigned short router_id){
  sequence_num = 0;
  this->router_id = router_id;
}

void LSTable::delete_ls(vector<unsigned short>& deleted_dst_ids) {
  for (vector<unsigned short>::iterator iter = deleted_dst_ids.begin(); iter != deleted_dst_ids.end(); ++iter) {
    vector<LS_Entry*>::iterator ls_iter = linkst.begin();

    while (ls_iter != linkst.end()) {
      LS_Entry* entry = *ls_iter;

      if (entry->neighbor_id == *iter) {
        delete_neighbor(entry->neighbor_id);
        ls_iter = linkst.erase(ls_iter);
        free(entry);

        break;
      } else {
        ++ls_iter;
      }
    }
  }
}

bool LSTable::check_ls_state(unsigned int current_time){
  bool update = false;
  vector<LS_Entry*>::iterator iter = linkst.begin();

  while (iter != linkst.end()) {
    if ((*iter)->time_to_expire < current_time) {
      update = true;
      LS_Entry* timeout_entry = *iter;
      delete_neighbor(timeout_entry->neighbor_id);
      iter = linkst.erase(iter);
      free(timeout_entry);
    } else {
      ++iter;
    }
  }

  return update;
}

void LSTable::update_by_ls(char* packet, unsigned int current_time, unsigned short size){
  unsigned short source_id = (unsigned short)ntohs(*(unsigned short*)(packet + 4));
  unsigned int count = (size - 12) >> 2;
  vector<LS_Entry*>* ls_vec = new vector<LS_Entry*>;

  for (unsigned int i = 0; i < count; ++i) {
    unsigned int offset = 12 + (i << 2);
    unsigned short neighbor_id = (unsigned short)ntohs(*(unsigned short*)(packet + offset));
    unsigned short cost = (unsigned short)ntohs(*(unsigned short*)(packet + offset + 2));

    if (neighbor_id == router_id) {
      continue;
    }

    LS_Entry* entry=(LS_Entry*)malloc(sizeof(LS_Entry));
    entry->neighbor_id = neighbor_id;
    entry->cost = cost;

    ls_vec->push_back(entry);
  }

  cout << "*****************************" << endl;
  cout << "Router ID: " << router_id << "received link state from Router " << source_id << ": "<< endl;
  cout << "*****************************" << endl;
  cout << "Destination ID\tnext hop" << endl;
  cout << "*****************************" << endl;
  cout << "ls_vec empty: " << ls_vec->empty() << endl;
  cout << "ls vec size: " << ls_vec->size() << endl;
  for (vector<LS_Entry*>::iterator iter = ls_vec->begin(); iter != ls_vec->end(); ++iter) {
    cout << (*iter)->neighbor_id << "\t" << (*iter)->cost << endl;
  }

  cout << "*****************************" << endl;

  /* update time_to_expire */
  LS_Entry* neighbor_entry = check_linkst_contains(source_id);

  if (neighbor_entry != NULL) {
    neighbor_entry->time_to_expire = current_time + LS_TIMEOUT;
  }

  hash_map<unsigned short, vector<LS_Entry*>*>::iterator it = table.find(source_id);

  /* replace link state */
  if (it != table.end()) {
    //free mem;
    vector<LS_Entry*>* vec = it->second;
    vector<LS_Entry*>::iterator vec_iter = vec->begin();

    while (vec_iter != vec->end()) {
      LS_Entry* entry = *vec_iter;
      vec_iter = vec->erase(vec_iter);
      free(entry);
    }

    delete vec;
  }

  table[source_id]=ls_vec;
}

bool LSTable::check_lsp_sequence_num(char* packet) {
  unsigned short source_id = (unsigned short)ntohs(*(unsigned short*)(packet + 4));
  unsigned int sequence_num = (unsigned int)ntohl(*(unsigned int*)(packet + 8));

  if (source_id == router_id) {
    return false;
  }

  if(ls_sequence_num.find(source_id) == ls_sequence_num.end() || ls_sequence_num[source_id] < sequence_num){
    ls_sequence_num[source_id]=sequence_num;
    return true;
  }

  return false;
}

void LSTable::dijkstra(hash_map<unsigned short, unsigned short>& routing_table){
  //destination id and LS_info
  hash_map<unsigned short, LS_Info*> tentative;
  routing_table.clear();

  for (vector<LS_Entry*>::iterator it = linkst.begin(); it != linkst.end(); it++) {
    LS_Info *lin = (LS_Info*)malloc(sizeof(LS_Info));
    lin->cost = (*it)->cost;
    lin->next_hop = (*it)->neighbor_id;
    tentative[(*it)->neighbor_id] = lin;
  }

  while (!tentative.empty()) {
    // get the min cost link and confirm it into routing table
    LS_Info* min;
    unsigned short dest;
    unsigned short min_cost = INFINITY_COST;
    for (hash_map<unsigned short, LS_Info*>::iterator it = tentative.begin(); it != tentative.end(); it++) {
      if (it->second->cost < min_cost) {
        min_cost = it->second->cost;
        dest = it->first;
        min = it->second;
      }
    }

    unsigned short cost = min->cost;
    unsigned short next = min->next_hop;

    // erase it form tentative
    tentative.erase(dest);
    free(min);

    // put in routing table
    routing_table[dest] = next;

    // update tentative table;
    hash_map<unsigned short, vector<LS_Entry*>*>::iterator iter = table.find(dest);

    if (iter != table.end()) {
      vector<LS_Entry*>* vec = iter->second;

      for (vector<LS_Entry*>::iterator it = vec->begin(); it != vec->end(); ++it){
        LS_Entry* entry = *it;
        unsigned short cost_thru_next = cost + entry->cost;
        hash_map<unsigned short, LS_Info*>::iterator tentative_iter = tentative.find(entry->neighbor_id);

        if (tentative_iter != tentative.end()) {
          LS_Info* ls_info = tentative_iter->second;

          if (cost_thru_next < ls_info->cost) {
            ls_info->cost = cost_thru_next;
            ls_info->next_hop = dest;
          }
        } else {
          hash_map<unsigned short, unsigned short>::iterator route_iter = routing_table.find(entry->neighbor_id);

          if (route_iter == routing_table.end() && router_id != entry->neighbor_id) {
            LS_Info* temp = (LS_Info*)malloc(sizeof(LS_Info));
            temp->cost = cost_thru_next;
            temp->next_hop = dest;
            tentative[entry->neighbor_id] = temp;
          }
        }
      }
    }
  }

  print_routing_table(routing_table);
}

void LSTable::set_ls_packet(char* packet, unsigned short packet_size){
  *(char*)packet = LS;
  *(unsigned short*)(packet + 2) = (unsigned short)htons(packet_size);
  *(unsigned short*)(packet + 4) = (unsigned short)htons(router_id);
  *(unsigned int*)(packet + 8) = (unsigned int)htonl(sequence_num);

  /* get neighbor ID and cost from linkst */
  int count = 0;
  for(vector<LS_Entry *>::iterator it = linkst.begin(); it != linkst.end(); it++){
    int offset = 12 + (count << 2);
    *(unsigned short*)((char*)packet + offset) = (unsigned short)htons((*it)->neighbor_id);
    *(unsigned short*)((char*)packet + offset + 2) = (unsigned short)htons((*it)->cost);

    ++count;
  }
}

bool LSTable::update_by_pong(unsigned short src_id, unsigned short cost, unsigned int current_time){
  bool update = false;
  LS_Entry* entry = check_linkst_contains(src_id);

  if (entry != NULL) {
    entry->time_to_expire = current_time + LS_TIMEOUT;

    if (cost != entry->cost) {
      update = true;
      entry->cost = cost;
    }
  } else {
    update = true;
    entry = (LS_Entry*)malloc(sizeof(LS_Entry));
    entry->neighbor_id = src_id;
    entry->cost = cost;
    entry->time_to_expire = current_time + LS_TIMEOUT;
    linkst.push_back(entry);
  }

  return update;
}

LS_Entry* LSTable::check_linkst_contains(unsigned short src_id) {
  for (vector<LS_Entry*>::iterator iter = linkst.begin(); iter != linkst.end(); ++iter) {
    LS_Entry* entry = *iter;

    if (entry->neighbor_id == src_id) {
      return entry;
    }
  }

  return NULL;
}

void LSTable::delete_neighbor(unsigned short neighbor_id) {
  hash_map<unsigned short, vector<LS_Entry*>*>::iterator it = table.find(neighbor_id);

  if (it != table.end()) {
    //free mem;
    vector<LS_Entry*>* vec = it->second;
    vector<LS_Entry *>::iterator vec_iter = vec->begin();

    while (vec_iter != vec->end()) {
      LS_Entry* entry = *vec_iter;
      vec_iter = vec->erase(vec_iter);
      free(entry);
    }

    table.erase(it);
    delete vec;
  }
}

void LSTable::print_routing_table(hash_map<unsigned short, unsigned short>& routing_table) {
  cout << "*****************************" << endl;
  cout << "Router ID: " << router_id << endl;
  cout << "*****************************" << endl;
  cout << "Destination ID\tnext hop" << endl;
  cout << "*****************************" << endl;

  for (hash_map<unsigned short, unsigned short>::iterator iter = routing_table.begin(); iter != routing_table.end(); ++iter) {
    cout << iter->first << "\t" << iter->second << endl;
  }

  cout << "*****************************" << endl;
}
