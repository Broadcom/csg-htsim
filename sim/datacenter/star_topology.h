// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#ifndef STAR
#define STAR
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
#include <ostream>

#define NSRV 576

class StarTopology: public Topology{
public:
    Pipe * pipe_out_ns[NSRV];
    RandomQueue * queue_out_ns[NSRV];

    Pipe * pipe_in_ns[NSRV];
    RandomQueue * queue_in_ns[NSRV];

    FirstFit * ff;
    Logfile* logfile;
    EventList* eventlist;
  
    StarTopology(Logfile* log,EventList* ev,FirstFit* f,simtime_picosec rtt);

    void init_network();
    virtual vector<const Route*>* get_paths(uint32_t src, uint32_t dest);

    void count_queue(RandomQueue*);
    void print_path(std::ofstream& paths, uint32_t src, const Route* route);
    vector<uint32_t>* get_neighbours(uint32_t src);
    uint32_t no_of_nodes() const {return _no_of_nodes;}
private:
    simtime_picosec _rtt;
    map<RandomQueue*,int> _link_usage;
    uint32_t _no_of_nodes;
};

#endif
