// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#ifndef VL2
#define VL2
#include "main.h"
#include "randomqueue.h"
#include "pipe.h"
#include "config.h"
#include "loggers.h"
#include "network.h"
#include "firstfit.h"
#include "topology.h"
#include "logfile.h"
#include "eventlist.h"

#define NT2A 2      //Number of connections from a ToR to aggregation switches

#define TOR_ID(id) N+id
#define AGG_ID(id) N+NT+id
#define INT_ID(id) N+NT+NA+id
#define HOST_ID(hid,tid) tid*NS+hid

#define HOST_TOR(host) host/NS
#define HOST_TOR_ID(host) host%NS
#define TOR_AGG1(tor) tor%NA

class VL2Topology: public Topology{
public:
    Pipe * pipes_ni_na[NI][NA];
    Pipe * pipes_na_nt[NA][NT];
    Pipe * pipes_nt_ns[NT][NS];
    RandomQueue * queues_ni_na[NI][NA];
    RandomQueue * queues_na_nt[NA][NT];
    RandomQueue * queues_nt_ns[NT][NS];
    Pipe * pipes_na_ni[NA][NI];
    Pipe * pipes_nt_na[NT][NA];
    Pipe * pipes_ns_nt[NS][NT];
    RandomQueue * queues_na_ni[NA][NI];
    RandomQueue * queues_nt_na[NT][NA];
    RandomQueue * queues_ns_nt[NS][NT];

    FirstFit * ff;
    Logfile* logfile;
    EventList* eventlist;

    VL2Topology(Logfile* log,EventList* ev,FirstFit* f,simtime_picosec rtt);

    void init_network();
    virtual vector<const Route*>* get_paths(uint32_t src, uint32_t dest);
    vector<uint32_t>* get_neighbours(uint32_t src) {return NULL;};
    uint32_t no_of_nodes() const {return _no_of_nodes;}
private:
    uint32_t _no_of_nodes;
    simtime_picosec _rtt;
};

#endif
