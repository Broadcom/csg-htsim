// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#ifndef MH_FAT_TREE
#define MH_FAT_TREE
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

#define K 12
#define NK (K*K/2)
#define NLP (K*K)
#define NC (K*K/4)
#define NSRV (K*K*K/3)

#define HOST_POD_SWITCH1(src) (2*(3*src/K/2))
#define HOST_POD_SWITCH2(src) (2*(3*src/K/2)+1)
#define HOST_POD(src) (3*src/(K*K))

#define MIN_POD_ID(pod_id) (pod_id*K/2)
#define MAX_POD_ID(pod_id) ((pod_id+1)*K/2-1)

class MultihomedFatTreeTopology: public Topology{
public:
    Pipe * pipes_nc_nup[NC][NK];
    Pipe * pipes_nup_nlp[NK][NLP];
    Pipe * pipes_nlp_ns[NLP][NSRV];
    RandomQueue * queues_nc_nup[NC][NK];
    RandomQueue * queues_nup_nlp[NK][NLP];
    RandomQueue * queues_nlp_ns[NLP][NSRV];

    Pipe * pipes_nup_nc[NK][NC];
    Pipe * pipes_nlp_nup[NLP][NK];
    Pipe * pipes_ns_nlp[NSRV][NLP];
    RandomQueue * queues_nup_nc[NK][NC];
    RandomQueue * queues_nlp_nup[NLP][NK];
    RandomQueue * queues_ns_nlp[NSRV][NLP];

    FirstFit * ff;
    Logfile* logfile;
    EventList* eventlist;
  
    MultihomedFatTreeTopology(Logfile* log,EventList* ev,FirstFit* f,simtime_picosec rtt);

    void init_network();
    virtual vector<const Route*>* get_paths(uint32_t src, uint32_t dest);

    void count_queue(RandomQueue*);
    void print_path(std::ofstream& paths,uint32_t src,const Route* route);
    vector<uint32_t>* get_neighbours(uint32_t src);
    uint32_t no_of_nodes() const {return _no_of_nodes;}
private:
    simtime_picosec _rtt;
    map<RandomQueue*,uint32_t> _link_usage;
    int64_t find_lp_switch(RandomQueue* queue);
    int64_t find_up_switch(RandomQueue* queue);
    int64_t find_core_switch(RandomQueue* queue);
    int64_t find_destination(RandomQueue* queue);
    uint32_t _no_of_nodes;
};

#endif
