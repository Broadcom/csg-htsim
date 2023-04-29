// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-        
#ifndef ROUTETABLE_H
#define ROUTETABLE_H

/*
 * A Route Table resolves switch IDs to output ports. 
 */

#include "queue.h"

#include <list>
#include <vector>
#include <unordered_map>

class FibEntry{
public:
    FibEntry(Route* outport, uint32_t cost, packet_direction direction){ _out = outport; _cost = cost;_direction = direction;}

    Route* getEgressPort(){return _out;}
    uint32_t getCost(){return _cost;}
    packet_direction getDirection(){return _direction;}
    
protected:
    Route* _out;
    uint32_t _cost;
    packet_direction _direction;
};

class HostFibEntry{
public:
    HostFibEntry(Route* outport, int flowid){ _flowid = flowid; _out = outport;}

    Route* getEgressPort(){return _out;}
    int getFlowID(){return _flowid;}

protected:
    Route* _out;
    uint32_t _flowid;

};

class RouteTable {
public:
    RouteTable() {};
    void addRoute(int destination, Route* port, int cost, packet_direction direction);  
    void addHostRoute(int destination, Route* port, int flowid);  
    void setRoutes(int destination, vector<FibEntry*>* routes);  
    vector <FibEntry*>* getRoutes(int destination);
    HostFibEntry* getHostRoute(int destination, int flowid);
    
private:
    unordered_map<int,vector<FibEntry*>* > _fib;
    unordered_map<int,unordered_map<int,HostFibEntry*>*> _hostfib;
};

#endif
