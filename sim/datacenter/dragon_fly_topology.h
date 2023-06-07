// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#ifndef DRAGONFLY
#define DRAGONFLY
#include "main.h"
#include "randomqueue.h"
#include "pipe.h"
#include "config.h"
#include "loggers.h"
#include "network.h"
#include "topology.h"
#include "logfile.h"
#include "eventlist.h"
#include "switch.h"
#include <ostream>

//Dragon Fly parameters
// p = number of hosts per router.
// a = number of routers per group.
// h = number of links used to connect to other groups.
// k = router radix.
// g = number of groups.
// 
// The dragonfly parameters a, p, and h can have any values.
// However to balance channel load on load-balanced traffic, the
// network should have a = 2p = 2h; p = h;
// relations between parameters. [Kim et al, ISCA 2008, https://static.googleusercontent.com/media/research.google.com/en//pubs/archive/34926.pdf]
// 
// k = p + h + a - 1
// k = 4h - 1.
// N = ap (ah + 1) = 2h * h (2h*h +1) = 4h^4 + 2h^2 = 4 * (k/4)^4 + 2 * (k/4)^2.
// g <= ah + 1 = 2h^2 + 1. 

#define HOST_TOR(src) (src/_p)
//#define HOST_GROUP_ID(src) src%NSRV
#define HOST_GROUP(src) (src/(_a*_p))

#ifndef QT
#define QT
typedef enum {RANDOM, ECN, COMPOSITE, CTRL_PRIO, LOSSLESS, LOSSLESS_INPUT, LOSSLESS_INPUT_ECN, COMPOSITE_ECN} queue_type;
#endif

class DragonFlyTopology: public Topology{
public:
    vector <Switch*> switches;

    vector< vector<Pipe*> > pipes_host_switch;
    vector< vector<Pipe*> > pipes_switch_switch;
    vector< vector<Queue*> > queues_host_switch;
    vector< vector<Queue*> > queues_switch_switch;
    vector< vector<Pipe*> > pipes_switch_host;
    vector< vector<Queue*> > queues_switch_host;
  
    Logfile* logfile;
    EventList* _eventlist;
    uint32_t failed_links;
    queue_type qt;

    DragonFlyTopology(uint32_t p, uint32_t h, uint32_t a, mem_b queuesize, Logfile* log,EventList* ev,queue_type q,simtime_picosec rtt);
    DragonFlyTopology(uint32_t no_of_nodes, mem_b queuesize, Logfile* log,EventList* ev,queue_type q, simtime_picosec rtt);

    void init_network();
    virtual vector<const Route*>* get_bidir_paths(uint32_t src, uint32_t dest, bool reverse);

    Queue* alloc_src_queue(QueueLogger* q);
    Queue* alloc_queue(QueueLogger* q, mem_b queuesize, bool tor);
    Queue* alloc_queue(QueueLogger* q, uint64_t speed, mem_b queuesize, bool tor);

    void count_queue(Queue*);
    void print_path(std::ofstream& paths, uint32_t src, const Route* route);
    vector<uint32_t>* get_neighbours(uint32_t src) { return NULL;};
    uint32_t no_of_nodes() const {return _no_of_nodes;}
private:
    int64_t find_switch(Queue* queue);
    int64_t find_destination(Queue* queue);

    void set_params(uint32_t no_of_nodes);
    void set_params();

    uint32_t _p, _a, _h;
    uint32_t _no_of_nodes;
    uint32_t _no_of_groups,_no_of_switches;
    simtime_picosec _rtt;
    mem_b _queuesize;
};

#endif
