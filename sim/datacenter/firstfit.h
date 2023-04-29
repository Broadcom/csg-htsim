// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#ifndef firstfit
#define firstfit

#include "main.h"
#include "tcp.h"
#include "randomqueue.h"
#include "eventlist.h"
#include <list>
#include <map>

struct flow_entry {
    int byte_counter;
    int allocated;
    int src;
    int dest;
    const Route* route;

    flow_entry(int bc, int a,int s, int d, const Route* r){
        byte_counter = bc;
        allocated = a;
        src = s;
        dest = d;
        route = r;
    }
};

class FirstFit: public EventSource{
public:
    FirstFit(simtime_picosec scanPeriod, EventList& eventlist,vector<const Route*>*** np = NULL);
    void doNextEvent();

    void run();
    void add_flow(int src,int dest,TcpSrc* flow);
    void add_queue(BaseQueue* queue);
    vector<const Route*>*** net_paths;

private:
    map<TcpSrc*,flow_entry*> flow_counters;
    //TcpSrc* connections[N][N];
    map<BaseQueue*,int> path_allocations;
    simtime_picosec _scanPeriod;  
    //int _init;
    int threshold;
};

#endif
