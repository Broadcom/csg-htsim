// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-        
#ifndef OV_FAT_TREE
#define OV_FAT_TREE
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

#define K 8
#define NK (K*K/2)
#define NC (K*K/4)
#define NSRV (K*K*K)

#define HOST_POD_SWITCH(src) (src/2/K)
#define HOST_POD(src) (src/K/K)

#define MIN_POD_ID(pod_id) (pod_id*K/2)
#define MAX_POD_ID(pod_id) ((pod_id+1)*K/2-1)

#ifndef QT
#define QT
typedef enum {RANDOM, ECN, COMPOSITE, CTRL_PRIO, LOSSLESS, LOSSLESS_INPUT, LOSSLESS_INPUT_ECN} queue_type;
#endif

class OversubscribedFatTreeTopology: public Topology{
public:
    Pipe * pipes_nc_nup[NC][NK];
    Pipe * pipes_nup_nlp[NK][NK];
    Pipe * pipes_nlp_ns[NK][NSRV];
    Queue * queues_nc_nup[NC][NK];
    Queue * queues_nup_nlp[NK][NK];
    Queue * queues_nlp_ns[NK][NSRV];

    Pipe * pipes_nup_nc[NK][NC];
    Pipe * pipes_nlp_nup[NK][NK];
    Pipe * pipes_ns_nlp[NSRV][NK];
    Queue * queues_nup_nc[NK][NC];
    Queue * queues_nlp_nup[NK][NK];
    Queue * queues_ns_nlp[NSRV][NK];

    FirstFit * ff;
    Logfile* logfile;
    EventList* eventlist;

    queue_type qt;
  
    OversubscribedFatTreeTopology(mem_b queuesize, Logfile* log,EventList* ev,FirstFit* f,queue_type q,simtime_picosec rtt, int n);

    void init_network();
    virtual vector<const Route*>* get_bidir_paths(uint32_t src, uint32_t dest, bool reverse);

    Queue* alloc_src_queue(QueueLogger* q);
    Queue* alloc_queue(QueueLogger* q);
    Queue* alloc_queue(QueueLogger* q,uint64_t speed);

    void count_queue(Queue*);
    void print_path(std::ofstream& paths,uint32_t src,const Route* route);
    vector<uint32_t>* get_neighbours(uint32_t src);
    uint32_t no_of_nodes() const {return _no_of_nodes;}
private:
    map<Queue*,int> _link_usage;
    int64_t find_lp_switch(Queue* queue);
    int64_t find_up_switch(Queue* queue);
    int64_t find_core_switch(Queue* queue);
    int64_t find_destination(Queue* queue);
    uint32_t _no_of_nodes;
    simtime_picosec _rtt;
    mem_b _queuesize;
    int _N;
};

#endif
