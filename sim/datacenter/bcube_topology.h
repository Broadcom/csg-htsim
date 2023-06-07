// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#ifndef BCUBE
#define BCUBE
#include "main.h"
#include "queue.h"
#include "pipe.h"
#include "config.h"
#include "loggers.h"
#include "network.h"
#include "firstfit.h"
#include "topology.h"
#include "logfile.h"
#include "eventlist.h"
#include "matrix.h"
#include <ostream>

//number of levels
//#define K 1
//number of ports per SWITCH
//#define NUM_PORTS 4
//total number of servers
//#define NUM_SRV 16
//switches per level
//=(K+1)*NUM_SRV/NUM_PORTS
//#define NUM_SW 8

#ifndef QT
#define QT
typedef enum {RANDOM, COMPOSITE, COMPOSITE_PRIO} queue_type;
#endif

class BCubeTopology: public Topology{
public:
    Matrix3d<Pipe*> pipes_srv_switch;
    Matrix3d<Pipe*> pipes_switch_srv;
    Matrix3d<Queue*> queues_srv_switch;
    Matrix3d<Queue*> queues_switch_srv;
    Matrix2d<Queue*> prio_queues_srv;
    Matrix2d<uint32_t> addresses;

    FirstFit * ff;
    Logfile* logfile;
    EventList* eventlist;
  
    BCubeTopology(uint32_t no_of_nodes, uint32_t ports_per_switch, uint32_t no_of_levels, 
                  Logfile* log,EventList* ev,FirstFit* f, queue_type q,simtime_picosec rtt);

    void init_network();
    virtual vector<const Route*>* get_paths(uint32_t src, uint32_t dest);
 

    void count_queue(Queue*);
    void print_path(std::ofstream& paths,uint32_t src,const Route* route);
    void print_paths(std::ofstream& p, uint32_t src, vector<const Route*>* paths);
    vector<uint32_t>* get_neighbours(uint32_t src);
    uint32_t no_of_nodes() const {
        cout << "NoN: " << _NUM_SRV << "\n";
        return _NUM_SRV;
    }
private:
    queue_type qt;
    map<Queue*,int> _link_usage;
    uint32_t _K, _NUM_PORTS, _NUM_SRV, _NUM_SW;
    simtime_picosec _rtt;
    void set_params(uint32_t no_of_nodes, uint32_t ports_per_switch, uint32_t no_of_levels);

    uint32_t srv_from_address(unsigned int* address);
    void address_from_srv(uint32_t srv);
    uint32_t get_neighbour(uint32_t src, uint32_t level);
    uint32_t switch_from_srv(uint32_t srv, uint32_t level);

    Route* bcube_routing(uint32_t src,uint32_t dest, uint32_t* permutation, uint32_t* nic);
    Route* dc_routing(uint32_t src,uint32_t dest, uint32_t i);  
    Route* alt_dc_routing(uint32_t src,uint32_t dest, uint32_t i,uint32_t c);  

    Queue* alloc_src_queue(QueueLogger* queueLogger);
    Queue* alloc_queue(QueueLogger* queueLogger);
    Queue* alloc_queue(QueueLogger* queueLogger, uint64_t speed);
};

#endif
