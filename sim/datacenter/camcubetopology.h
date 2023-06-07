// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#ifndef CAMCUBE
#define CAMCUBE
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
#include <ostream>

//size of camcube (K^3)
#define K 5
//total number of servers
#define NUM_SRV 125

#ifndef QT
#define QT
typedef enum {RANDOM, COMPOSITE, COMPOSITE_PRIO} queue_type;
#endif

class CamCubeTopology: public Topology{
public:
    Pipe * pipes[NUM_SRV][6];
    Queue * queues[NUM_SRV][6];
    Queue * prio_queues[NUM_SRV][6];
    unsigned int addresses[NUM_SRV][3];

    Logfile* logfile;
    EventList* eventlist;
  
    CamCubeTopology(Logfile* log,EventList* ev,queue_type q,simtime_picosec rtt);

    void init_network();
    virtual vector<const Route*>* get_paths(int src, int dest);
    virtual vector<Route*>* get_paths_camcube(int src, int dest, int first=1);
 
    void count_queue(Queue*);
    void print_path(std::ofstream& paths,int src,const Route* route);
    void print_paths(std::ofstream& p, int src, vector<const Route*>* paths);
    int get_distance(int src,int dest,int dimension,int* iface);

    vector<int>* get_neighbours(int src) { return NULL;};    

private:
    queue_type qt;
    simtime_picosec _rtt;

    int srv_from_address(unsigned int* address);
    void address_from_srv(int srv, unsigned int* address);

    Queue* alloc_src_queue(QueueLogger* queueLogger);
    Queue* alloc_queue(QueueLogger* queueLogger);
    Queue* alloc_queue(QueueLogger* queueLogger, uint64_t speed);
};

#endif
