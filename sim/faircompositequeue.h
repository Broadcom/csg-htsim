// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-        
#ifndef FAIR_COMPOSITE_QUEUE_H
#define FAIR_COMPOSITE_QUEUE_H

/*
 * A fair queue between a composite NDP queue that transforms packets into headers when there is no space and services headers with priority, and a standard DROP-TAIL queue for TCP traffic. 
 */

#define QUEUE_INVALID -1
#define QUEUE_LOW 0
#define QUEUE_HIGH 1
#define QUEUE_DROPTAIL 2

#include <list>
#include "queue.h"
#include <string>
#include "config.h"
#include "eventlist.h"
#include "network.h"
#include "loggertypes.h"

class FairCompositeQueue : public Queue {
 public:
    FairCompositeQueue(linkspeed_bps bitrate, mem_b maxsize, mem_b maxsize2,
                   EventList &eventlist, QueueLogger* logger);

    virtual void receivePacket(Packet& pkt);
    virtual void doNextEvent();
    // should really be private, but loggers want to see
    mem_b _queuesize_low,_queuesize_high, _queuesize_dt;
    int num_headers() const { return _num_headers;}
    int num_packets() const { return _num_packets;}
    int num_stripped() const { return _num_stripped;}
    int num_bounced() const { return _num_bounced;}
    int num_acks() const { return _num_acks;}
    int num_nacks() const { return _num_nacks;}
    int num_pulls() const { return _num_pulls;}

    virtual mem_b queuesize();
    virtual void setName(const string& name) {
        Logged::setName(name); 
        _nodename += name;
    }
    virtual const string& nodename() { return _nodename; }

    int _num_packets;
    int _num_headers; // only includes data packets stripped to headers, not acks or nacks
    int _num_acks;
    int _num_nacks;
    int _num_pulls;
    int _num_stripped; // count of packets we stripped
    int _num_bounced;  // count of packets we bounced

 protected:
    // Mechanism
    void beginService(); // start serving the item at the head of the queue
    void completeService(); // wrap up serving the item at the head of the queue

    int _serv;
    int _ratio_high, _ratio_low, _crt;

    uint64_t _virtime[3];
    
    list<tuple<Packet*,uint64_t>> _enqueued_low;
    list<tuple<Packet*,uint64_t>> _enqueued_high;
    list<tuple<Packet*,uint64_t>> _enqueued_dt;    
};

#endif
