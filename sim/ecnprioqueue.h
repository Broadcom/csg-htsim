// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-        
#ifndef ECN_PRIO_QUEUE_H
#define ECN_PRIO_QUEUE_H

/*
 * A two-level priority queue supporting ECN
 */

#include <list>
#include "queue.h"
#include "config.h"
#include "eventlist.h"
#include "network.h"
#include "loggertypes.h"

class ECNPrioQueue : public Queue {
public:
    typedef enum {Q_LO=0, Q_HI=1, Q_NONE=2} queue_priority_t;
    ECNPrioQueue(linkspeed_bps bitrate,
                 mem_b maxsize_hi, mem_b maxsize_low,
                 mem_b ecn_thresh_hi, mem_b ecn_thresh_lo,  
                 EventList &eventlist, QueueLogger* logger);
    virtual void receivePacket(Packet& pkt);
    virtual void doNextEvent();
    // should really be private, but loggers want to see
    int num_packets() const { return _num_packets;}
    virtual mem_b queuesize() const;
    mem_b lo_queuesize() const;
    mem_b hi_queuesize() const;
    virtual void setName(const string& name) {
        Logged::setName(name); 
        _nodename += name;
    }
    virtual const string& nodename() { return _nodename; }

protected:
    // Mechanism
    void beginService(); // start serving the item at the head of the queue
    void completeService(); // wrap up serving the item at the head of the queue
    queue_priority_t getPriority(Packet& pkt);

    mem_b _queuesize[Q_NONE];
    mem_b _maxsize[Q_NONE];
    mem_b _ecn_thresh[Q_NONE];
    int _num_packets;
    int _serv;
    bool _ecn; // set ECN_CE when service is complete

    CircularBuffer<Packet*> _enqueued[Q_NONE];
};

#endif
