# Intra-Domain Routing Protocols for Bisco GSR9999

##Group Member
Peng Chu S01216798
Li Shen S01216847
Jie Zhu S01216956

##Introduction

In this project, a variant of the distance vector routing protocol (DV) and a variant of the link-state routing protocol (LS) are implemented. A GSR9999 simulator written in C++ is used to test the two routing protocols.

##Implementation
In our project, in order to reduce the complexity of class RoutingProtocolImpl, we abstract LS and DV protocol to class LSTable and class DVTable. In each class, it maintains their own data structure, which implemented by hash_map, to store all necessary information such as all other nodes’ link states or routing information. The final routing table is placed in class RoutingProtocolImpl which can be updated by either class LSTable or class DVTable.

##Test
For more details about test, please reference to the file test.txt
